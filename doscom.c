/*
 *	doscom.c
 *
 *	builtin commands for DOS
 */

#include <fcntl.h>
#ifdef	FD
#include "fd.h"
#else	/* !FD */
#include "machine.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include "printf.h"
#include "kctype.h"
#include "pathname.h"
#endif	/* !FD */

#ifdef	USETIMEH
#include <time.h>
#endif

#if	MSDOS && !defined (FD)
#define	VOL_FAT32	"FAT32"
# ifdef	DOUBLESLASH
# define	MNTDIRSIZ	MAXPATHLEN
# else
# define	MNTDIRSIZ	(3 + 1)
# endif
#endif	/* MSDOS && !FD */

#if	MSDOS
# ifdef	__TURBOC__
# include <dir.h>
# endif
#include "unixemu.h"
#define	DOSCOMOPT	'/'
#define	C_EOF		K_CTRL('Z')
#else	/* !MSDOS */
#include <sys/time.h>
#include <sys/file.h>
#include <sys/param.h>
# ifdef	USEDIRECT
# include <sys/dir.h>
# define	dirent	direct
# else
# include <dirent.h>
# endif
# ifdef	USEUTIME
# include <utime.h>
# endif
#define	DOSCOMOPT	'-'
#define	C_EOF		K_CTRL('D')
#endif	/* !MSDOS */

#include "system.h"

#ifndef	FD
# if	MSDOS
typedef struct _statfs_t {
	u_short f_type __attribute__ ((packed));
	u_short f_version __attribute__ ((packed));
	u_long f_clustsize __attribute__ ((packed));
	u_long f_sectsize __attribute__ ((packed));
	u_long f_bavail __attribute__ ((packed));
	u_long f_blocks __attribute__ ((packed));
	u_long f_real_bavail_sect __attribute__ ((packed));
	u_long f_real_blocks_sect __attribute__ ((packed));
	u_long f_real_bavail __attribute__ ((packed));
	u_long f_real_blocks __attribute__ ((packed));
	u_char reserved[8] __attribute__ ((packed));
} statfs_t;
# else	/* !MSDOS */
#  ifdef	USESTATVFSH
#include <sys/statvfs.h>
#   ifdef	USESTATVFS_T
typedef statvfs_t		statfs_t;
#   else
typedef struct statvfs		statfs_t;
#   endif
#  define	statfs2		statvfs
#  define	blocksize(fs)	((fs).f_frsize ? (fs).f_frsize : (fs).f_bsize)
#  endif	/* USESTATVFSH */
#  ifdef	USESTATFSH
#include <sys/statfs.h>
typedef struct statfs		statfs_t;
#  define	f_bavail	f_bfree
#  define	blocksize(fs)	(fs).f_bsize
#  endif	/* USESTATFSH */
#  ifdef	USEVFSH
#include <sys/vfs.h>
typedef struct statfs		statfs_t;
#  define	blocksize(fs)	(fs).f_bsize
#  endif	/* USEVFSH */
#  ifdef	USEMOUNTH
#include <sys/mount.h>
typedef struct statfs		statfs_t;
#  define	blocksize(fs)	(fs).f_bsize
#  endif	/* USEMOUNTH */
#  ifdef	USEFSDATA
#include <sys/mount.h>
typedef struct fs_data		statfs_t;
#  define	statfs2(path, buf)	(statfs(path, buf) - 1)
#  define	blocksize(fs)	1024
#  define	f_blocks	fd_req.btot
#  define	f_bavail	fd_req.bfreen
#  endif	/* USEFSDATA */
#  ifdef	USEFFSIZE
#  define	f_bsize		f_fsize
#  endif
#  if	defined (USESTATFSH) || defined (USEVFSH) || defined (USEMOUNTH)
#   if	(STATFSARGS >= 4)
#   define	statfs2(path, buf)	statfs(path, buf, sizeof(statfs_t), 0)
#   else
#    if	(STATFSARGS == 3)
#    define	statfs2(path, buf)	statfs(path, buf, sizeof(statfs_t))
#    else
#    define	statfs2			statfs
#    endif
#   endif
#  endif	/* USESTATFSH || USEVFSH || USEMOUNTH */
# endif	/* !MSDOS */
#endif	/* !FD */

#ifdef	NOFILEMODE
# ifdef	S_IRUSR
# undef	S_IRUSR
# endif
# ifdef	S_IWUSR
# undef	S_IWUSR
# endif
#define	S_IRUSR	00400
#define	S_IWUSR	00200
#endif

#ifdef	NOERRNO
extern int errno;
#endif
#ifdef	DEBUG
extern char *_mtrace_file;
#endif

#if	defined (DOSCOMMAND) \
&& (!defined (FD) || (FD >= 2 && !defined (_NOORIGSHELL)))

#ifdef	FD
#include "term.h"
#define	c_left		termstr[C_LEFT]
#else	/* !FD */
# if	MSDOS
# define	gettext		dummy_gettext	/* fake for DJGPP gcc-3.3 */
# include <conio.h>
# define	cc_intr		K_CTRL('C')
# define	K_CR		'\r'
# else	/* !MSDOS */
extern int ttyio;
static int cc_intr = -1;
# define	K_CR		'\n'
# endif	/* !MSDOS */
#define	K_CTRL(c)	((c) & 037)
#define	K_BS		010
#define	n_line		24
#define	t_clear		"\033[;H\033[2J"
#define	c_left		"\010"
#endif	/* !FD */

#ifdef	FD
extern DIR *Xopendir __P_((CONST char *));
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern char *Xgetwd __P_((char *));
extern int Xstat __P_((CONST char *, struct stat *));
extern int Xlstat __P_((CONST char *, struct stat *));
extern int Xaccess __P_((CONST char *, int));
extern int Xchmod __P_((CONST char *, int));
extern int Xunlink __P_((CONST char *));
extern int Xrename __P_((CONST char *, CONST char *));
extern int Xopen __P_((CONST char *, int, int));
# ifdef	_NODOSDRIVE
# define	Xclose(f)	((close(f)) ? -1 : 0)
# define	Xlseek		lseek
# else	/* !_NODOSDRIVE */
extern int Xclose __P_((int));
extern off_t Xlseek __P_((int, off_t, int));
# endif	/* !_NODOSDRIVE */
extern int Xmkdir __P_((CONST char *, int));
extern int Xrmdir __P_((CONST char *));
extern int stat2 __P_((CONST char *, struct stat *));
extern int chdir2 __P_((CONST char *));
#else	/* !FD */
# if	MSDOS
extern DIR *Xopendir __P_((CONST char *));
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
# else
# define	Xopendir	opendir
# define	Xclosedir	closedir
# define	Xreaddir	readdir
# endif
# ifdef	DJGPP
extern int dos_putpath __P_((CONST char *, int));
extern char *Xgetwd __P_((char *));
# else	/* !DJGPP */
#  ifdef	USEGETWD
#  define	Xgetwd		(char *)getwd
#  else
#  define	Xgetwd(p)	(char *)getcwd(p, MAXPATHLEN)
#  endif
# endif	/* !DJGPP */
# if	MSDOS
extern int Xstat __P_((CONST char *, struct stat *));
# define	Xlstat		Xstat
# else
# define	Xstat(p, s)	((stat(p, s)) ? -1 : 0)
# define	Xlstat(p, s)	((lstat(p, s)) ? -1 : 0)
# endif
#define	Xaccess(p, m)	((access(p, m)) ? -1 : 0)
# ifndef	NOSYMLINK
# define	Xsymlink(o, n)	((symlink(o, n)) ? -1 : 0)
# define	Xreadlink	readlink
# endif
#define	Xchmod(p, m)	((chmod(p, m)) ? -1 : 0)
#define	Xunlink(p)	((unlink(p)) ? -1 : 0)
#define	Xrename(f, t)	((rename(f, t)) ? -1 : 0)
#define	Xopen		open
#define	Xclose(f)	((close(f)) ? -1 : 0)
#define	Xlseek		lseek
# if	MSDOS
#  ifdef	DJGPP
#  define	Xmkdir(p, m)	((mkdir(p, m)) ? -1 : 0)
#  else
extern int Xmkdir __P_((CONST char *, int));
#  endif
# else
# define	Xmkdir(p, m)	((mkdir(p, m)) ? -1 : 0)
# endif
#define	Xrmdir(p)	((rmdir(p)) ? -1 : 0)
# if	MSDOS
extern int intcall __P_((int, __dpmi_regs *, struct SREGS *));
extern int chdir2 __P_((CONST char *));
# else
# define	chdir2(p)	((chdir(p)) ? -1 : 0)
# endif
# ifdef	NOSYMLINK
# define	stat2		Xstat
# else
extern int stat2 __P_((CONST char *, struct stat *));
# endif
#endif	/* !FD */

struct filestat_t {
	char *nam;
#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
	char *d_alias;
#else
#define	d_alias		nam
#endif
	u_short mod;
	off_t siz;
	time_t mtim;
	time_t atim;
	u_char flags;
};

#define	FS_NOTMATCH	0001

#ifndef	O_BINARY
#define	O_BINARY	0
#endif
#ifndef	O_TEXT
#define	O_TEXT		0
#endif
#ifndef	ENOSPC
#define	ENOSPC		EACCES
#endif
#ifndef	ENODEV
#define	ENODEV		EACCES
#endif
#ifndef	EIO
#define	EIO		ENODEV
#endif
#ifndef	DEV_BSIZE
#define	DEV_BSIZE	512
#endif

#ifndef	L_SET
# ifdef	SEEK_SET
# define	L_SET	SEEK_SET
# else
# define	L_SET	0
# endif
#endif	/* !L_SET */
#ifndef	L_INCR
# ifdef	SEEK_CUR
# define	L_INCR	SEEK_CUR
# else
# define	L_INCR	1
# endif
#endif	/* !L_INCR */

extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *c_realloc __P_((char *, ALLOC_T, ALLOC_T *));
extern char *strdup2 __P_((CONST char *));
extern char *strncpy2 __P_((char *, CONST char *, int));

#ifdef	FD
extern int getinfofs __P_((CONST char *, off_t *, off_t *, off_t *));
extern char *realpath2 __P_((CONST char *, char *, int));
extern int touchfile __P_((CONST char *, struct stat *));
#ifndef	NODIRLOOP
extern int issamebody __P_((CONST char *, CONST char *));
#endif
#ifndef	NOSYMLINK
extern int cpsymlink __P_((CONST char *, CONST char *));
#endif
extern char *inputstr __P_((CONST char *, int, int, CONST char *, int));
#else	/* !FD */
static int NEAR getinfofs __P_((CONST char *, off_t *, off_t *, off_t *));
# if	MSDOS
static char *NEAR realpath2 __P_((CONST char *, char *, int));
static int NEAR putdostime __P_((u_short *, u_short *, time_t));
#  ifdef	USEUTIME
static int NEAR Xutime __P_((CONST char *, CONST struct utimbuf *));
#  else
static int NEAR Xutimes __P_((CONST char *, CONST struct timeval *));
#  endif
# define	ttyiomode(n)
# define	stdiomode()
# define	getkey3(n, c)	getch();
# else	/* !MSDOS */
# define	realpath2(p, r, f) \
				realpath(p, r)
# define	Xutime(f, t)	(utime(f, t) ? -1 : 0)
# define	Xutimes(f, t)	(utimes(f, t) ? -1 : 0)
static VOID NEAR ttymode __P_((int));
# define	ttyiomode(n)	(ttymode(1))
# define	stdiomode()	(ttymode(0))
static int NEAR getkey3 __P_((int, int));
# endif	/* !MSDOS */
static int NEAR touchfile __P_((CONST char *, struct stat *));
#ifndef	NODIRLOOP
static int NEAR issamebody __P_((CONST char *, CONST char *));
#endif
#ifndef	NOSYMLINK
static int NEAR cpsymlink __P_((CONST char *, CONST char *));
#endif
static char *NEAR inputstr __P_((CONST char *, int, int, CONST char *, int));
#endif	/* !FD */

static VOID NEAR doserror __P_((CONST char *, int));
static VOID NEAR dosperror __P_((CONST char *));
static VOID NEAR fputsize __P_((off_t *, off_t *));
static int NEAR inputkey __P_((VOID_A));
static int cmpdirent __P_((CONST VOID_P, CONST VOID_P));
static VOID NEAR evalenvopt __P_((CONST char *, CONST char *,
		int (NEAR *)__P_((int, char *CONST []))));
static int NEAR getdiropt __P_((int, char *CONST []));
static int NEAR showstr __P_((char *, int, int));
#ifdef	MINIMUMSHELL
static VOID NEAR showfname __P_((struct filestat_t *, off_t *));
#else
static VOID NEAR showfname __P_((struct filestat_t *, off_t *, int));
#endif
static VOID NEAR showfnamew __P_((struct filestat_t *));
static VOID NEAR showfnameb __P_((struct filestat_t *));
#ifdef	MINIMUMSHELL
static int NEAR _checkline __P_((VOID_A));
#define	checkline(n)		_checkline()
#else
static int NEAR _checkline __P_((int));
#define	checkline		_checkline
#endif
static VOID NEAR dosdirheader __P_((VOID_A));
#ifdef	MINIMUMSHELL
static VOID NEAR dosdirfooter __P_((off_t *, int, int, off_t *, off_t *));
static int NEAR dosdir __P_((reg_t *, off_t *, int *, int *, off_t *));
#else
static VOID NEAR dosdirfooter __P_((off_t *, int, int,
		off_t *, off_t *, off_t *, off_t *));
static int NEAR dosdir __P_((reg_t *, off_t *, int *, int *,
		off_t *, off_t *));
#endif
int doscomdir __P_((int, char *CONST []));
static int NEAR checkarg __P_((int, char *CONST []));
int doscommkdir __P_((int, char *CONST []));
int doscomrmdir __P_((int, char *CONST []));
int doscomerase __P_((int, char *CONST []));
static char *NEAR convwild __P_((char *, CONST char *,
		CONST char *, CONST char *));
int doscomrename __P_((int, char *CONST []));
static int NEAR getcopyopt __P_((int, char *CONST []));
static int NEAR getbinmode __P_((CONST char *, int));
static int NEAR writeopen __P_((CONST char *, CONST char *));
static int NEAR textread __P_((int, u_char *, int, int));
static int NEAR textclose __P_((int, int));
static int NEAR doscopy __P_((CONST char *, CONST char *,
		struct stat *, int, int, int));
int doscomcopy __P_((int, char *CONST []));
int doscomcls __P_((int, char *CONST []));
int doscomtype __P_((int, char *CONST []));

#define	BUFUNIT		32
#define	DIRSORTFLAG	"NSEDGA"
#define	DIRATTRFLAG	"DRHSA"
#define	b_size(n, type)	((((n) / BUFUNIT) + 1) * BUFUNIT * sizeof(type))
#define	b_realloc(ptr, n, type) \
			(((n) % BUFUNIT) ? ((type *)(ptr)) \
			: (type *)realloc2(ptr, b_size(n, type)))
#define	dir_isdir(sp)	(((((sp) -> mod) & S_IFMT) == S_IFDIR) ? 1 : 0)
#ifdef	USESTRERROR
#define	strerror2		strerror
#else
# ifndef	DECLERRLIST
extern CONST char *CONST sys_errlist[];
# endif
#define	strerror2(n)		(char *)sys_errlist[n]
#endif

static CONST char *doserrstr[] = {
	NULL,
#define	ER_REQPARAM	1
	"Required parameter missing",
#define	ER_TOOMANYPARAM	2
	"Too many parameters",
#define	ER_FILENOTFOUND	3
	"File not found",
#define	ER_INVALIDPARAM	4
	"Invalid parameter",
#define	ER_DUPLFILENAME	5
	"Duplicate file name or file in use",
#define	ER_COPIEDITSELF	6
	"File cannot be copied onto itself",
#define	ER_INVALIDSW	7
	"Invalid switch",
};
#define	DOSERRSIZ	arraysize(doserrstr)

static char dirsort[strsize(DIRSORTFLAG) * 2 + 1] = "";
static char dirattr[strsize(DIRATTRFLAG) * 2 + 1] = "";
static int dirline = 0;
static char *dirwd = NULL;
static int dirtype = '\0';
static int dirflag = 0;
#define	DF_LOWER	001
#define	DF_PAUSE	002
#define	DF_SUBDIR	004
#define	DF_CANCEL	010
static int copyflag = 0;
#define	CF_BINARY	001
#define	CF_TEXT		002
#define	CF_VERIFY	004
#define	CF_NOCONFIRM	010
#define	CF_VERBOSE	020
#define	CF_CANCEL	040


#ifndef	FD
static int NEAR getinfofs(path, totalp, freep, bsizep)
CONST char *path;
off_t *totalp, *freep, *bsizep;
{
# if	MSDOS
#  ifdef	DOUBLESLASH
	char *cp;
	int len;
#  endif
	struct SREGS sreg;
	__dpmi_regs reg;
	char drv[MNTDIRSIZ], buf[128];
# endif	/* !MSDOS */
	statfs_t fsbuf;

# if	MSDOS
#  ifdef	DOUBLESLASH
	if ((len = isdslash(path))) {
		if ((cp = strdelim(&(path[len]), 0))) {
			len = cp - path;
			if (!(cp = strdelim(&(path[len + 1]), 0)))
				cp += strlen(cp);
			len = cp - path;
		}
		strncpy2(drv, path, len);
	}
	else
#  endif
	VOID_C gendospath(drv, toupper2(path[0]), _SC_);

	reg.x.ax = 0x71a0;
	reg.x.bx = 0;
	reg.x.cx = sizeof(buf);
#  ifdef	DJGPP
	dos_putpath(drv, sizeof(buf));
#  endif
	sreg.ds = PTR_SEG(drv);
	reg.x.dx = PTR_OFF(drv, sizeof(buf));
	sreg.es = PTR_SEG(buf);
	reg.x.di = PTR_OFF(buf, 0);
	if (intcall(0x21, &reg, &sreg) >= 0 && (reg.x.bx & 0x4000)
#  ifdef	DJGPP
	&& (dosmemget(__tb, sizeof(buf), buf), 1)
#  endif
	&& !strcmp(buf, VOL_FAT32)) {
		reg.x.ax = 0x7303;
		reg.x.cx = sizeof(fsbuf);
#  ifdef	DJGPP
		dos_putpath(path, sizeof(fsbuf));
#  endif
		sreg.es = PTR_SEG(&fsbuf);
		reg.x.di = PTR_OFF(&fsbuf, 0);
		sreg.ds = PTR_SEG(path);
		reg.x.dx = PTR_OFF(path, sizeof(fsbuf));
		if (intcall(0x21, &reg, &sreg) < 0) return(-1);

#  ifdef	DJGPP
		dosmemget(__tb, sizeof(fsbuf), &fsbuf);
#  endif
		*totalp = (off_t)(fsbuf.f_blocks);
		*freep = (off_t)(fsbuf.f_bavail);
		*bsizep = (off_t)(fsbuf.f_clustsize)
			* (off_t)(fsbuf.f_sectsize);
	}
	else {
		reg.x.ax = 0x3600;
		reg.h.dl = drv[0] - 'A' + 1;
		intcall(0x21, &reg, &sreg);
		if (reg.x.ax == 0xffff) {
			errno = ENOENT;
			return(-1);
		}

		*totalp = (off_t)(reg.x.dx);
		*freep = (off_t)(reg.x.bx);
		*bsizep = (off_t)(reg.x.ax) * (off_t)(reg.x.cx);
	}
	if (!*totalp || !*bsizep || *totalp < *freep) {
		errno = EIO;
		return(-1);
	}
# else	/* !MSDOS */
	if (statfs2(path, &fsbuf) < 0) return(-1);
	*totalp = fsbuf.f_blocks;
	*freep = fsbuf.f_bavail;
	*bsizep = blocksize(fsbuf);
# endif	/* !MSDOS */

	return(0);
}

# if	MSDOS
/*ARGSUSED*/
static char *NEAR realpath2(path, resolved, rdlink)
CONST char *path;
char *resolved;
int rdlink;
{
	struct SREGS sreg;
	__dpmi_regs reg;
#   ifdef	DJGPP
	int i;
#   endif

	reg.x.ax = 0x6000;
	reg.x.cx = 0;
#   ifdef	DJGPP
	i = dos_putpath(path, 0);
#   endif
	sreg.ds = PTR_SEG(path);
	reg.x.si = PTR_OFF(path, 0);
	sreg.es = PTR_SEG(resolved);
	reg.x.di = PTR_OFF(resolved, i);
	if (intcall(0x21, &reg, &sreg) < 0) return(NULL);
#   ifdef	DJGPP
	dosmemget(__tb + i, MAXPATHLEN, resolved);
#   endif

	return(resolved);
}

static int NEAR putdostime(dp, tp, tim)
u_short *dp, *tp;
time_t tim;
{
	struct tm *tm;

	tm = localtime(&tim);
	*dp = (((tm -> tm_year - 80) & 0x7f) << 9)
		+ (((tm -> tm_mon + 1) & 0x0f) << 5)
		+ (tm -> tm_mday & 0x1f);
	*tp = ((tm -> tm_hour & 0x1f) << 11)
		+ ((tm -> tm_min & 0x3f) << 5)
		+ ((tm -> tm_sec & 0x3e) >> 1);

	return(*tp);
}

#   ifdef	USEUTIME
static int NEAR Xutime(path, times)
CONST char *path;
CONST struct utimbuf *times;
{
	time_t t;
	__dpmi_regs reg;
	struct SREGS sreg;
	int i, fd;

	t = times -> modtime;
#   else	/* !USEUTIME */
static int NEAR Xutimes(path, tvp)
CONST char *path;
CONST struct timeval *tvp;
{
	time_t t;
	__dpmi_regs reg;
	struct SREGS sreg;
	int i, fd;

	t = tvp[1].tv_sec;
#   endif	/* !USEUTIME */
	if ((fd = open(path, O_RDONLY, 0666)) < 0) return(-1);
	reg.x.ax = 0x5701;
	reg.x.bx = fd;
	putdostime(&(reg.x.dx), &(reg.x.cx), t);
	i = intcall(0x21, &reg, &sreg);
	close(fd);

 	return(i);
}
# else	/* !MSDOS */
static VOID NEAR ttymode(on)
int on;
{
	termioctl_t tty;

	if (tioctl(ttyio, REQGETP, &tty) < 0) return;
#  if	defined (USETERMIOS) || defined (USETERMIO)
	if (cc_intr < 0) cc_intr = tty.c_cc[VINTR];
	if (on) tty.c_lflag &= ~(ECHO | ICANON | ISIG);
	else tty.c_lflag |= (ECHO | ICANON | ISIG);
#  else	/* !USETERMIO && !USETERMIO */
	if (cc_intr < 0) {
		struct tchars cc;

		if (tioctl(ttyio, TIOCGETC, &cc) < 0) cc_intr = K_CTRL('C');
		else cc_intr = cc.t_intrc;
	}
	if (on) {
		tty.sg_flags |= RAW;
		tty.sg_flags &= ~ECHO;
	}
	else {
		tty.sg_flags &= ~RAW;
		tty.sg_flags |= ECHO;
	}
#  endif	/* !USETERMIOS && !USETERMIO */
	tioctl(ttyio, REQSETP, &tty);
}

/*ARGSUSED*/
static int NEAR getkey3(sig, code)
int sig, code;
{
	u_char ch;
	int i;

	while ((i = read(ttyio, &ch, sizeof(u_char))) < 0 && errno == EINTR);
	if (i < (int)sizeof(u_char)) return(EOF);

	return((int)ch);
}
# endif	/* !MSDOS */

static int NEAR touchfile(path, stp)
CONST char *path;
struct stat *stp;
{
# ifdef	USEUTIME
	struct utimbuf times;

	times.actime = stp -> st_atime;
	times.modtime = stp -> st_mtime;

	return(Xutime(path, &times));
# else
	struct timeval tvp[2];

	tvp[0].tv_sec = stp -> st_atime;
	tvp[0].tv_usec = 0;
	tvp[1].tv_sec = stp -> st_mtime;
	tvp[1].tv_usec = 0;

	return(Xutimes(path, tvp));
# endif
}

# ifndef	NODIRLOOP
static int NEAR issamebody(src, dest)
CONST char *src, *dest;
{
	struct stat st1, st2;

	if (Xstat(src, &st1) < 0 || Xstat(dest, &st2) < 0) return(0);

	return(st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino);
}
# endif	/* !NODIRLOOP */

# ifndef	NOSYMLINK
static int NEAR cpsymlink(src, dest)
CONST char *src, *dest;
{
	struct stat st;
	char path[MAXPATHLEN];
	int len;

	if ((len = Xreadlink(src, path, strsize(path))) < 0) return(-1);
	if (Xlstat(dest, &st) >= 0) {
#  ifndef	NODIRLOOP
		if (issamebody(src, dest)) return(0);
#  endif
		if (Xunlink(dest) < 0) return(-1);
	}
	path[len] = '\0';

	return(Xsymlink(path, dest));
}
# endif	/* !NOSYMLINK */

/*ARGSUSED*/
static char *NEAR inputstr(prompt, delsp, ptr, def, h)
CONST char *prompt;
int delsp, ptr;
CONST char *def;
int h;
{
	char *cp;
	ALLOC_T size;
	int i, c;

	fputs(prompt, stdout);
	fflush(stdout);

	cp = c_realloc(NULL, 0, &size);
	for (i = 0; (c = inputkey()) != '\n'; i++) {
		if (c < 0) {
			free(cp);
			return(NULL);
		}
		if (c > (int)MAXUTYPE(u_char)) i--;
		else if (c != K_BS) {
			cp = c_realloc(cp, i, &size);
			cp[i] = c;
			if (!iscntrl2(c)) fputc(c, stdout);
			else fprintf2(stdout, "^%c", (c + '@') & 0x7f);
			fflush(stdout);
		}
		else if (i <= 0) i--;
		else {
			i -= 2;
			fputs("\b \b", stdout);
			if (iscntrl2(cp[i + 1])) fputs("\b \b", stdout);
			fflush(stdout);
		}
	}
	cp[i++] = '\0';
	fputnl(stdout);

	return(realloc2(cp, i));
}
#endif	/* !FD */

static VOID NEAR doserror(s, n)
CONST char *s;
int n;
{
	if (!n || n >= DOSERRSIZ) return;
	if (s) {
		kanjifputs(s, stderr);
		fputnl(stderr);
	}
	if (doserrstr[n]) fputs(doserrstr[n], stderr);
	fputnl(stderr);
}

static VOID NEAR dosperror(s)
CONST char *s;
{
	int duperrno;

	duperrno = errno;
	if (errno < 0) return;
	if (s) {
		kanjifputs(s, stderr);
		fputnl(stderr);
	}
	fputs(strerror2(duperrno), stderr);
	fputnl(stderr);
	errno = 0;
}

static VOID NEAR fputsize(np, bsizep)
off_t *np, *bsizep;
{
	off_t s, mb;

	if (*np < (off_t)0 || !*bsizep) {
		fprintf2(stdout, "%15.15s bytes", "?");
		return;
	}

	s = ((off_t)1024 * (off_t)1024 + (*bsizep / 2)) / *bsizep;
	mb = *np / s;
	if (mb < (off_t)1024) fprintf2(stdout, "%'15qd bytes", *np * *bsizep);
	else fprintf2(stdout, "%'12qd.%02qd MB",
		mb, ((*np - mb * s) * (off_t)100) / s);
}

static int NEAR inputkey(VOID_A)
{
	int c;

	ttyiomode(1);
	c = getkey3(0, inputkcode);
	stdiomode();
	if (c == EOF) c = -1;
	else if (c == cc_intr) {
		fputs("^C", stdout);
		fputnl(stdout);
		c = -1;
	}

	return(c);
}

static int cmpdirent(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	struct filestat_t *sp1, *sp2;
	char *cp1, *cp2;
	int i, r, ret;

	sp1 = (struct filestat_t *)vp1;
	sp2 = (struct filestat_t *)vp2;
	if (!strchr(dirsort, 'E')) cp1 = cp2 = NULL;
	else {
		if ((cp1 = strrchr(sp1 -> nam, '.'))) *cp1 = '\0';
		if ((cp2 = strrchr(sp2 -> nam, '.'))) *cp2 = '\0';
	}

	ret = 0;
	for (i = 0; dirsort[i]; i++) {
		r = 0;
		if (dirsort[i] == '-') {
			i++;
			r = 1;
		}
		switch (dirsort[i]) {
			case 'N':
				ret = strpathcmp2(sp1 -> nam, sp2 -> nam);
				break;
			case 'S':
				if (sp1 -> siz == sp2 -> siz) ret = 0;
				else ret = (sp1 -> siz > sp2 -> siz) ? 1 : -1;
				break;
			case 'E':
				if (cp1) ret = (cp2) ?
					strpathcmp2(cp1 + 1, cp2 + 1) : 1;
				else ret = (cp2) ? -1 : 0;
				break;
			case 'D':
				if (sp1 -> mtim == sp2 -> mtim) ret = 0;
				else ret = (sp1 -> mtim > sp2 -> mtim)
					? 1 : -1;
				break;
			case 'G':
				ret = dir_isdir(sp2) - dir_isdir(sp1);
				break;
			case 'A':
				if (sp1 -> atim == sp2 -> atim) ret = 0;
				else ret = (sp1 -> atim > sp2 -> atim)
					? 1 : -1;
				break;
			default:
				break;
		}
		if (r) ret = -ret;
		if (ret) break;
	}
	if (cp1) *cp1 = '.';
	if (cp2) *cp2 = '.';

	return(ret);
}

static VOID NEAR evalenvopt(cmd, env, getoptcmd)
CONST char *cmd, *env;
int (NEAR *getoptcmd)__P_((int, char *CONST []));
{
	char *cp, **argv;
	int n, er, argc;

	if (!(cp = getshellvar(env, -1)) || !*cp) return;
	argv = (char **)malloc2(3 * sizeof(char *));
	argc = 2;
	argv[0] = strdup2(cmd);
	argv[1] = strdup2(cp);
	argv[2] = NULL;
	argc = evalifs(argc, &argv, IFS_SET);

	er = 0;
	if ((n = (*getoptcmd)(argc, argv)) < 0) er++;
	else if (argc > n) {
		er++;
		doserror(argv[n], ER_TOOMANYPARAM);
	}
	if (er) fputs("(Error occurred in environment variable)\n\n", stdout);
	freevar(argv);
}

static int NEAR getdiropt(argc, argv)
int argc;
char *CONST argv[];
{
	char *arg;
	int i, j, n, r, rr;

	for (i = 1; i < argc; i++) {
		arg = argv[i];
		if (arg[0] != DOSCOMOPT) break;
		for (j = 1; arg[j]; j++) arg[j] = toupper2(arg[j]);
		rr = (arg[1] == '-') ? 1 : 0;
		switch (arg[1 + rr]) {
			case 'P':
				if (rr) dirflag &= ~DF_PAUSE;
				else dirflag |= DF_PAUSE;
				break;
			case 'W':
				if (rr) {
					if (dirtype == 'W') dirtype = '\0';
					break;
				}
				if (dirtype != 'B') dirtype = 'W';
				break;
			case 'A':
				if (rr) {
					strcpy(dirattr, "hs");
					break;
				}
				n = r = 0;
				arg += 2;
				for (j = 0; arg[j]; j++) {
					if (strchr(DIRATTRFLAG, arg[j])) {
						dirattr[n] = '\0';
						if (!strchr(dirattr, arg[j]))
							dirattr[n++] = arg[j];
						r = 0;
					}
					else if (arg[j] == '-' && !r) {
						dirattr[n++] = '-';
						r = 1;
					}
					else {
						doserror(arg, ER_INVALIDSW);
						return(-1);
					}
				}
				if (n) dirattr[n] = '\0';
				else strcpy(dirattr, DIRATTRFLAG);
				if (r) {
					doserror(arg, ER_INVALIDSW);
					return(-1);
				}
				arg += j - 2;
				break;
			case 'O':
				if (rr) {
					dirsort[0] = '\0';
					break;
				}
				n = r = 0;
				arg += 2;
				for (j = 0; arg[j]; j++) {
					if (strchr(DIRSORTFLAG, arg[j])) {
						dirsort[n] = '\0';
						if (!strchr(dirsort, arg[j]))
							dirsort[n++] = arg[j];
						r = 0;
					}
					else if (arg[j] == '-' && !r) {
						dirsort[n++] = '-';
						r = 1;
					}
					else {
						doserror(arg, ER_INVALIDSW);
						return(-1);
					}
				}
				if (!n) dirsort[n++] = 'N';
				dirsort[n] = '\0';
				if (r) {
					doserror(arg, ER_INVALIDSW);
					return(-1);
				}
				arg += j - 2;
				break;
			case 'S':
				if (rr) dirflag &= ~DF_SUBDIR;
				else dirflag |= DF_SUBDIR;
				break;
			case 'B':
				if (rr) {
					if (dirtype == 'B') dirtype = '\0';
					break;
				}
				dirtype = 'B';
				break;
			case 'L':
				if (rr) dirflag &= ~DF_LOWER;
				else dirflag |= DF_LOWER;
				break;
#ifndef	MINIMUMSHELL
			case 'V':
				if (rr) {
					if (dirtype == 'V') dirtype = '\0';
					break;
				}
				if (!dirtype) dirtype = 'V';
				break;
			case '4':
				if (rr) {
					if (dirtype == '4') dirtype = '\0';
					break;
				}
				if (!dirtype) dirtype = '4';
				break;
#endif	/* !MINIMUMSHELL */
			default:
				doserror(argv[i], ER_INVALIDSW);
				return(-1);
/*NOTREACHED*/
				break;
		}
		if (*(arg += 2 + rr)) {
			doserror(arg, ER_INVALIDSW);
			return(-1);
		}
	}

	return(i);
}

static int NEAR showstr(s, len, lower)
char *s;
int len, lower;
{
	int i, olen;

	olen = len;
	if (len < 0) len = -len;

	for (i = 0; s[i] && len > 0; i++, len--) {
		if (iskanji1(s, i)) {
			if (len <= 1) break;
			i++;
			len--;
		}
#ifdef	CODEEUC
		else if (isekana(s, i)) i++;
#endif
	}
	if (olen >= 0) while (len-- > 0) s[i++] = ' ';
	else olen = -olen - len;
	s[i] = '\0';
	if (lower) for (i = 0; s[i]; i++) s[i] = tolower2(s[i]);
	else for (i = 0; s[i]; i++) s[i] = toupper2(s[i]);
	kanjifputs(s, stdout);

	return(olen);
}

#ifdef	MINIMUMSHELL
static VOID NEAR showfname(dirp, bsizep)
struct filestat_t *dirp;
off_t *bsizep;
#else
static VOID NEAR showfname(dirp, bsizep, verbose)
struct filestat_t *dirp;
off_t *bsizep;
int verbose;
#endif
{
	struct tm *tm;
	char *ext, buf[MAXNAMLEN + 1];
	int i;

	strcpy(buf, dirp -> d_alias);
	if (isdotdir(buf)) ext = NULL;
	else if ((ext = strrchr(buf, '.'))) {
		if (ext == buf) {
			ext = NULL;
			strcpy(buf, &(dirp -> d_alias[1]));
		}
		else {
			*(ext++) = '\0';
			i = ext - buf;
			ext = &(dirp -> d_alias[i]);
		}
	}
	showstr(buf, 8, (dirflag & DF_LOWER));
	fputc(' ', stdout);

	if (!ext) fputs("   ", stdout);
	else {
		strcpy(buf, ext);
		showstr(buf, 3, (dirflag & DF_LOWER));
	}
	fputc(' ', stdout);

	if (dir_isdir(dirp)) fputs("  <DIR>      ", stdout);
	else if ((dirp -> mod & S_IFMT) == S_IFREG)
		fprintf2(stdout, "%'13qd", dirp -> siz);
	else fputs("             ", stdout);

#ifndef	MINIMUMSHELL
	if (verbose > 0) {
		if ((dirp -> mod & S_IFMT) != S_IFREG)
			fputs("              ", stdout);
		else {
			dirp -> siz = ((dirp -> siz + *bsizep - 1) / *bsizep)
				* *bsizep;
			fprintf2(stdout, "%'14qd", dirp -> siz);
		}
	}
#endif	/* !MINIMUMSHELL */

#ifdef	DEBUG
	_mtrace_file = "localtime(start)";
	tm = localtime(&(dirp -> mtim));
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "localtime(end)";
		malloc(0);	/* dummy malloc */
	}
#else
	tm = localtime(&(dirp -> mtim));
#endif
	fputs("  ", stdout);
#ifndef	MINIMUMSHELL
	if (verbose < 0) fprintf2(stdout, "%04d-%02d-%02d  %2d:%02d ",
		tm -> tm_year + 1900, tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min);
	else
#endif
	fprintf2(stdout, "%02d-%02d-%02d  %2d:%02d ",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min);

#ifndef	MINIMUMSHELL
	if (verbose > 0) {
# ifdef	DEBUG
		_mtrace_file = "localtime(start)";
		tm = localtime(&(dirp -> atim));
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "localtime(end)";
			malloc(0);	/* dummy malloc */
		}
# else
		tm = localtime(&(dirp -> atim));
# endif
		fprintf2(stdout, " %02d-%02d-%02d  ",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
		strcpy(buf, "           ");
		if (!(dirp -> mod & S_IWUSR)) buf[0] = 'R';
		if (!(dirp -> mod & S_IRUSR)) buf[1] = 'H';
		if ((dirp -> mod & S_IFMT) == S_IFSOCK) buf[2] = 'S';
		if ((dirp -> mod & S_IFMT) == S_IFIFO) buf[3] = 'L';
		if (dir_isdir(dirp)) buf[4] = 'D';
		if (dirp -> mod & S_ISVTX) buf[5] = 'A';
		fputs(buf, stdout);
	}
#endif	/* !MINIMUMSHELL */

	kanjifputs(dirp -> nam, stdout);
	fputnl(stdout);
}

static VOID NEAR showfnamew(dirp)
struct filestat_t *dirp;
{
	char *ext, buf[MAXNAMLEN + 1];
	int i, len;

	if (dir_isdir(dirp)) fputc('[', stdout);
	strcpy(buf, dirp -> d_alias);
	if (isdotdir(buf)) ext = NULL;
	else if ((ext = strrchr(buf, '.'))) {
		if (ext == buf) {
			ext = NULL;
			strcpy(buf, &(dirp -> d_alias[1]));
		}
		else {
			*(ext++) = '\0';
			i = ext - buf;
			ext = &(dirp -> d_alias[i]);
		}
	}
	len = showstr(buf, -8, (dirflag & DF_LOWER));

	if (ext) {
		fputc('.', stdout);
		len++;
		strcpy(buf, ext);
		len += showstr(buf, -3, (dirflag & DF_LOWER));
	}

	if (dir_isdir(dirp)) fputc(']', stdout);
	for (; len < 8 + 1 + 3; len++) fputc(' ', stdout);
}

static VOID NEAR showfnameb(dirp)
struct filestat_t *dirp;
{
	char *cp, buf[MAXPATHLEN];
	int i;

	if (dirflag & DF_SUBDIR) {
		cp = dirwd;
		if (dirflag & DF_LOWER) {
			for (i = 0; dirwd[i]; i++) buf[i] = tolower2(dirwd[i]);
			buf[i] = '\0';
			cp = buf;
		}
		fprintf2(stdout, "%k%c", cp, _SC_);
	}

	if (dirflag & DF_LOWER)
		for (i = 0; dirp -> nam[i]; i++)
			dirp -> nam[i] = tolower2(dirp -> nam[i]);
	kanjifputs(dirp -> nam, stdout);
	fputnl(stdout);
}

#ifdef	MINIMUMSHELL
#define	n_incline	1
static int NEAR _checkline(VOID_A)
#else
static int NEAR _checkline(n_incline)
int n_incline;
#endif
{
	if (!(dirflag & DF_PAUSE)) return(0);
	if (dirflag & DF_CANCEL) return(-1);
	if ((dirline += n_incline) >= n_line - 2) {
		fputs("Press any key to continue . . .", stdout);
		fputnl(stdout);
		if (inputkey() < 0) {
			dirflag |= DF_CANCEL;
			return(-1);
		}
		fprintf2(stdout, "\n(continuing %k)\n", dirwd);
		dirline = n_incline;
	}

	return(0);
}

static VOID NEAR dosdirheader(VOID_A)
{
	if (dirtype != 'B') {
		if (checkline(1) < 0) return;
		if (dirflag & DF_SUBDIR) {
			fputnl(stdout);
			if (checkline(1) < 0) return;
		}
		else fputc(' ', stdout);
		fprintf2(stdout, "Directory of %k\n", dirwd);
	}
#ifndef	MINIMUMSHELL
	if (dirtype == 'V') {
		if (checkline(1) < 0) return;
		fputs("File Name         Size        Allocated      ", stdout);
		fputs("Modified      Accessed  Attrib\n", stdout);
		if (checkline(1) < 0) return;
		fputnl(stdout);
		if (checkline(1) < 0) return;
		fputnl(stdout);
	}
#endif	/* !MINIMUMSHELL */
	if (dirtype != 'B') {
		if (checkline(1) < 0) return;
		fputnl(stdout);
	}
}

#ifdef	MINIMUMSHELL
static VOID NEAR dosdirfooter(bsizep, nf, nd, sump, fp)
off_t *bsizep;
int nf, nd;
off_t *sump, *fp;
#else
static VOID NEAR dosdirfooter(bsizep, nf, nd, sump, bsump, fp, tp)
off_t *bsizep;
int nf, nd;
off_t *sump, *bsump, *fp, *tp;
#endif
{
	if (dirtype == 'B') return;
	if (!nf && !nd) {
		if (!fp) return;
		if (checkline(1) < 0) return;
		fputs("File not found\n", stdout);
		nd = -1;
	}
	else {
		if (fp && (dirflag & DF_SUBDIR)) {
			if (checkline(1) < 0) return;
			fputnl(stdout);
			if (checkline(1) < 0) return;
			fputs("Total files listed:\n", stdout);
		}
		if (checkline(1) < 0) return;
		fprintf2(stdout, "%10d file(s)%'15qd bytes\n", nf, *sump);
#ifndef	MINIMUMSHELL
		if (dirtype == 'V') {
			if (checkline(1) < 0) return;
			fprintf2(stdout, "%10d dir(s) ", nd);
			fputsize(bsump, bsizep);
			fputs(" allocated\n", stdout);
			nd = -1;
		}
#endif	/* !MINIMUMSHELL */
	}
	if (!fp) return;

	if (checkline(1) < 0) return;
	if (nd < 0) fputs("                  ", stdout);
	else fprintf2(stdout, "%10d dir(s) ", nd);

	fputsize(fp, bsizep);
	fputs(" free\n", stdout);

#ifndef	MINIMUMSHELL
	if (tp && dirtype == 'V') {
		if (checkline(1) < 0) return;
		fputs("                  ", stdout);
		fputsize(tp, bsizep);
		fprintf2(stdout, " total disk space, %3d%% in use\n",
			(int)(((*tp - *fp) * (off_t)100) / *tp));
	}
#endif	/* !MINIMUMSHELL */
}

#ifdef	MINIMUMSHELL
static int NEAR dosdir(re, bsizep, nfp, ndp, sump)
reg_t *re;
off_t *bsizep;
int *nfp, *ndp;
off_t *sump;
#else
static int NEAR dosdir(re, bsizep, nfp, ndp, sump, bsump)
reg_t *re;
off_t *bsizep;
int *nfp, *ndp;
off_t *sump, *bsump;
#endif
{
#ifndef	MINIMUMSHELL
	off_t bsum;
#endif
	struct filestat_t *dirlist;
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	char *file;
	off_t sum;
	int i, c, n, nf, nd, f, len, max;

	if (!(dirp = Xopendir(dirwd))) return(-1);

	n = nf = nd = 0;
#ifndef	MINIMUMSHELL
	bsum =
#endif
	sum = (off_t)0;
	len = strlen(dirwd);
	file = strcatdelim(dirwd);
	dirlist = NULL;
	while ((dp = Xreaddir(dirp))) {
		strcpy(file, dp -> d_name);
		if (stat2(dirwd, &st) < 0) continue;

		dirlist = b_realloc(dirlist, n, struct filestat_t);
		dirlist[n].nam = strdup2(dp -> d_name);
#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
		dirlist[n].d_alias = (dp -> d_alias[0])
			? strdup2(dp -> d_alias) : dirlist[n].nam;
#endif
		dirlist[n].mod = st.st_mode;
		dirlist[n].siz = st.st_size;
		dirlist[n].mtim = st.st_mtime;
		dirlist[n].atim = st.st_atime;
		dirlist[n].flags = 0;
		if (re && !regexp_exec(re, dirlist[n].nam, 0))
			dirlist[n].flags |= FS_NOTMATCH;
		else if ((st.st_mode & S_IFMT) == S_IFDIR) nd++;
		else {
			nf++;
			if ((st.st_mode & S_IFMT) == S_IFREG) {
				sum += dirlist[n].siz;
#ifndef	MINIMUMSHELL
				bsum += (dirlist[n].siz + *bsizep - 1)
					/ *bsizep;
#endif
			}
		}
		n++;
	}
	Xclosedir(dirp);
	dirwd[len] = '\0';
	max = n;
	if (*dirsort)
		qsort(dirlist, max, sizeof(struct filestat_t), cmpdirent);

	if (nf || nd) {
		*nfp += nf;
		*ndp += nd;
		*sump += sum;
#ifndef	MINIMUMSHELL
		*bsump += bsum;
#endif
		dosdirheader();
	}

	for (n = c = 0; n < max; n++) {
		if (dirflag & DF_CANCEL) break;
		if (dirlist[n].flags & FS_NOTMATCH) continue;
		for (i = 0; dirattr[i]; i++) {
			f = 0;
			if (dirattr[i] == '-') {
				i++;
				f = 1;
			}
			switch (dirattr[i]) {
				case 'D':
					if (!dir_isdir(&(dirlist[n]))) f ^= 1;
					break;
				case 'R':
					if (dirlist[n].mod & S_IWUSR) f ^= 1;
					break;
				case 'H':
					if (dirlist[n].mod & S_IRUSR) f ^= 1;
					break;
				case 'S':
					if ((dirlist[n].mod & S_IFMT)
					!= S_IFSOCK)
						f ^= 1;
					break;
				case 'A':
					if (!(dirlist[n].mod & S_ISVTX))
						f ^= 1;
					break;
				default:
					break;
			}
			if (!f) break;
		}
		if (!dirattr[i]) continue;

		switch (dirtype) {
			case 'W':
				if (!(c % 5) && checkline(1) < 0) break;
				showfnamew(&(dirlist[n]));
				if (c != (nf + nd - 1) && (c % 5) != 5 - 1)
					fputc('\t', stdout);
				else fputnl(stdout);
				break;
			case 'B':
				if (isdotdir(dirlist[n].nam)) break;
				if (checkline(1) >= 0)
					showfnameb(&(dirlist[n]));
				break;
#ifndef	MINIMUMSHELL
			case 'V':
				if (checkline(2) >= 0)
					showfname(&(dirlist[n]), bsizep, 1);
				break;
#endif
			default:
				if (checkline(1) >= 0)
#ifdef	MINIMUMSHELL
					showfname(&(dirlist[n]), bsizep);
#else
					showfname(&(dirlist[n]), bsizep,
						(dirtype == '4') ? -1 : 0);
#endif
				break;
		}
		c++;
	}

	if (dirflag & DF_SUBDIR) for (;;) {
#ifdef	MINIMUMSHELL
		dosdirfooter(bsizep, nf, nd, &sum, NULL);
#else
		dosdirfooter(bsizep, nf, nd, &sum, &bsum, NULL, NULL);
#endif
		file = strcatdelim(dirwd);
		for (n = 0; n < max; n++) {
			if (dirflag & DF_CANCEL) break;
			if (!dir_isdir(&(dirlist[n]))
			|| isdotdir(dirlist[n].nam))
				continue;
			strcpy(file, dirlist[n].nam);
#ifdef	MINIMUMSHELL
			c = dosdir(re, bsizep, nfp, ndp, sump);
#else
			c = dosdir(re, bsizep, nfp, ndp, sump, bsump);
#endif
			if (c < 0) {
				dosperror(dirwd);
				break;
			}
		}
		break;
/*NOTREACHED*/
	}
	dirwd[len] = '\0';

	for (n = 0; n < max; n++) {
		free(dirlist[n].nam);
#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
		if (dirlist[n].d_alias != dirlist[n].nam)
			free(dirlist[n].d_alias);
#endif
	}
	if (dirlist) free(dirlist);

	return(max);
}

int doscomdir(argc, argv)
int argc;
char *CONST argv[];
{
#ifndef	MINIMUMSHELL
	off_t bsum;
#endif
	reg_t *re;
	struct stat st;
	CONST char *dir;
	char *file, wd[MAXPATHLEN], cwd[MAXPATHLEN], buf[MAXPATHLEN];
	off_t sum, total, fre, bsize;
	int i, n, nf, nd;

	strcpy(dirattr, "hs");
	dirsort[0] = dirtype = '\0';
	dirflag = 0;
	evalenvopt(argv[0], "DIRCMD", getdiropt);
	if ((n = getdiropt(argc, argv)) < 0)
		return(RET_FAIL);
	if (argc <= n) {
		dir = curpath;
		file = "*";
	}
	else if (argc > n + 1) {
		doserror(argv[n + 1], ER_TOOMANYPARAM);
		return(RET_FAIL);
	}
	else if (Xstat(argv[n], &st) >= 0
	&& (st.st_mode & S_IFMT) == S_IFDIR) {
		strcpy(buf, argv[n]);
		dir = buf;
		file = "*";
	}
	else {
		strcpy(buf, argv[n]);
		if (!(file = strrdelim(buf, 1))) {
			dir = curpath;
			file = argv[n];
		}
		else {
			i = file - buf;
			if (isrootpath(buf) || *file != _SC_) file++;
			*file = '\0';
			dir = buf;
			file = &(argv[n][++i]);
			if (!*file) file = "*";
		}
	}

	if (!Xgetwd(cwd)) {
		dosperror(NULL);
		return(RET_FAIL);
	}
	if (dir != buf) strcpy(wd, cwd);
	else if (chdir2(buf) >= 0) {
		if (!Xgetwd(wd)) {
			dosperror(NULL);
			return(RET_FAIL);
		}
		if (chdir2(cwd) < 0) {
			dosperror(cwd);
			return(RET_FAIL);
		}
	}
#ifdef	DOUBLESLASH
	else if (isdslash(buf)) realpath2(buf, wd, 1);
#endif
	else {
		dosperror(buf);
		return(RET_FAIL);
	}
	if (getinfofs(wd, &total, &fre, &bsize) < 0) total = fre = (off_t)-1;

	if (dirtype != 'B') fputnl(stdout);

	dirline = nf = nd = 0;
#ifndef	MINIMUMSHELL
	bsum =
#endif
	sum = (off_t)0;
	dirwd = wd;
	re = regexp_init(file, -1);
#ifdef	MINIMUMSHELL
	n = dosdir(re, &bsize, &nf, &nd, &sum);
#else
	n = dosdir(re, &bsize, &nf, &nd, &sum, &bsum);
#endif
	if (re) regexp_free(re);
	if (n < 0) {
		dosperror(dir);
		return(RET_FAIL);
	}

#ifdef	MINIMUMSHELL
	dosdirfooter(&bsize, nf, nd, &sum, &fre);
#else
	dosdirfooter(&bsize, nf, nd, &sum, &bsum, &fre, &total);
#endif

	fflush(stdout);

	return(RET_SUCCESS);
}

static int NEAR checkarg(argc, argv)
int argc;
char *CONST argv[];
{
	if (argc <= 1) {
		doserror(NULL, ER_REQPARAM);
		return(-1);
	}
	else if (argc > 2) {
		doserror(argv[2], ER_TOOMANYPARAM);
		return(-1);
	}

	return(0);
}

int doscommkdir(argc, argv)
int argc;
char *CONST argv[];
{
	if (checkarg(argc, argv) < 0) return(RET_FAIL);

	if (Xmkdir(argv[1], 0777) < 0) {
		dosperror(argv[1]);
		return(RET_FAIL);
	}

	return(RET_SUCCESS);
}

int doscomrmdir(argc, argv)
int argc;
char *CONST argv[];
{
	if (checkarg(argc, argv) < 0) return(RET_FAIL);

	if (Xrmdir(argv[1]) < 0) {
		dosperror(argv[1]);
		return(RET_FAIL);
	}

	return(RET_SUCCESS);
}

int doscomerase(argc, argv)
int argc;
char *CONST argv[];
{
	char *buf, **wild;
	int i, n, c, flag, ret;

	flag = 0;
	for (n = 1; n < argc; n++) {
		if (argv[n][0] != DOSCOMOPT) break;
		if (n > 1) {
			doserror(argv[n], ER_TOOMANYPARAM);
			return(RET_FAIL);
		}
		if ((argv[n][1] == 'P' || argv[n][1] == 'p') && !argv[n][2])
			flag = 1;
		else {
			doserror(argv[n], ER_INVALIDSW);
			return(RET_FAIL);
		}
	}
	if (argc <= n) {
		doserror(argv[1], ER_REQPARAM);
		return(RET_FAIL);
	}
	else if (argc > n + 1) {
		doserror(argv[n + 1], ER_TOOMANYPARAM);
		return(RET_FAIL);
	}

	buf = strrdelim(argv[n], 1);
	if (buf) buf++;
	else buf = argv[n];
	if (!flag && (!strcmp(buf, "*") || !strcmp(buf, "*.*"))) {
		do {
			fputs("All files in directory will be deleted!",
				stdout);
			fputnl(stdout);
			ttyiomode(1);
			buf = inputstr("Are you sure (Y/N)?", 0, 0, NULL, -1);
			stdiomode();
			if (!buf) return(RET_SUCCESS);
			if (!isatty(STDIN_FILENO)) fputnl(stdout);
			c = *buf;
			free(buf);
		} while (!strchr("ynYN", c));
		if (c == 'n' || c == 'N') return(RET_SUCCESS);
	}
	if (!(wild = evalwild(argv[n], 0))) {
		doserror(argv[n], ER_FILENOTFOUND);
		return(RET_FAIL);
	}
	ret = RET_SUCCESS;
	for (i = 0; wild[i]; i++) {
		if (flag) {
			do {
				fprintf2(stdout,
					"%k,    Delete (Y/N)?", wild[i]);
				fflush(stdout);
				if ((c = inputkey()) < 0) {
					freevar(wild);
					return(ret);
				}
				if (c <= (int)MAXUTYPE(u_char) && isprint2(c))
					fputc(c, stdout);
				fputnl(stdout);
			} while (!strchr("ynYN", c));
			if (c == 'n' || c == 'N') continue;
		}
		if (Xunlink(wild[i]) < 0) {
			dosperror(wild[i]);
			ret = RET_FAIL;
			ERRBREAK;
		}
	}
	freevar(wild);

	return(ret);
}

static char *NEAR convwild(dest, src, wild, swild)
char *dest;
CONST char *src, *wild, *swild;
{
	int i, j, n, rest, w;

	for (i = j = n = 0; wild[i]; i++) {
		if (wild[i] == '?') {
			if (src[j]) dest[n++] = src[j];
		}
		else if (wild[i] != '*') dest[n++] = wild[i];
		else if (!src[j]) continue;
		else {
			for (rest = j; src[rest]; rest++);
			for (w = strlen(swild); w > 0 && rest > 0; w--, rest--)
				if (src[rest - 1] != swild[w - 1]) break;
			for (; j < rest; j++) dest[n++] = src[j];
			if (wild[i + 1] != '.') break;
		}

		if (src[j]) j++;
	}
	dest[n] = '\0';

	return(dest);
}

int doscomrename(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp, **wild, new[MAXPATHLEN];
	int i, j, ret;

	if (argc <= 2) {
		doserror(NULL, ER_REQPARAM);
		return(RET_FAIL);
	}
#if	0
	else if (argc > 3) {
		doserror(argv[3], ER_TOOMANYPARAM);
		return(RET_FAIL);
	}
#endif

	if (strdelim(argv[2], 1)) {
		doserror(argv[2], ER_INVALIDPARAM);
		return(RET_FAIL);
	}

	if (!(wild = evalwild(argv[1], 0))) {
		doserror(argv[1], ER_FILENOTFOUND);
		return(RET_FAIL);
	}
	ret = RET_SUCCESS;
	for (i = 0; wild[i]; i++) {
		strcpy(new, wild[i]);
		cp = getbasename(new);
		j = cp - new;
		convwild(cp, &(wild[i][j]), argv[2], argv[1]);
		if (Xaccess(new, F_OK) >= 0) {
			doserror(new, ER_DUPLFILENAME);
			ret = RET_FAIL;
			ERRBREAK;
		}
		if (errno != ENOENT || Xrename(wild[i], new) < 0) {
			dosperror(wild[i]);
			ret = RET_FAIL;
			ERRBREAK;
		}
	}
	freevar(wild);

	return(ret);
}

static int NEAR getcopyopt(argc, argv)
int argc;
char *CONST argv[];
{
	char *arg;
	int i, j;

	for (i = 1; i < argc; i++) {
		arg = argv[i];
		if (arg[0] != DOSCOMOPT) break;
		for (j = 1; arg[j]; j++) arg[j] = toupper2(arg[j]);
		if (!arg[1] || (arg[2] && arg[3])) {
			doserror(arg, ER_INVALIDSW);
			return(-1);
		}
		if (arg[1] == '-') switch (arg[2]) {
			case 'Y':
				copyflag &= ~CF_NOCONFIRM;
				break;
			default:
				doserror(arg, ER_INVALIDSW);
				return(-1);
/*NOTREACHED*/
				break;
		}
		else switch (arg[1]) {
			case 'A':
				copyflag &= ~CF_BINARY;
				copyflag |= CF_TEXT;
				break;
			case 'B':
				copyflag |= CF_BINARY;
				copyflag &= ~CF_TEXT;
				break;
			case 'V':
				copyflag |= CF_VERIFY;
				break;
			case 'Y':
				copyflag |= CF_NOCONFIRM;
				break;
			default:
				break;
		}
	}

	return(i);
}

static int NEAR getbinmode(name, bin)
CONST char *name;
int bin;
{
	char *cp;

	if (bin < 0) return(-1);
	if (!(cp = strchr(name, DOSCOMOPT)) || !cp[1] || cp[2]) /*EMPTY*/;
	else if (toupper2(*cp) == 'B') bin = CF_BINARY;
	else if (toupper2(*cp) == 'A') bin = CF_TEXT;

	return(bin);
}

static int NEAR writeopen(file, src)
CONST char *file, *src;
{
	struct stat st;
	char buf[MAXPATHLEN], buf2[MAXPATHLEN];
	int c, fd, key, flags;

	if ((copyflag & CF_NOCONFIRM) || Xstat(file, &st) < 0) c = 0;
	else {
		if (src && realpath2(src, buf, 1) && realpath2(file, buf2, 1)
		&& !strpathcmp(buf, buf2)) {
			errno = 0;
			return(-1);
		}
		if ((fd = Xopen(file, O_RDONLY, 0666)) < 0) return(fd);
		c = (isatty(fd)) ? 0 : 1;
		close(fd);
	}

	if (c) {
		fprintf2(stderr, "Overwrite %k (Yes/No/All)?", file);
		fflush(stderr);
		key = -1;
		for (;;) {
			if ((c = inputkey()) < 0) {
				copyflag |= CF_CANCEL;
				return(0);
			}
			if (c == K_CR && key >= 0) break;
			if (!strchr("ynaYNA", c)) continue;
			key = toupper2(c);
			fprintf2(stderr, "%c%s", c, c_left);
			fflush(stderr);
		}
		fputnl(stderr);
		if (key == 'N') return(0);
		else if (key == 'A') copyflag |= CF_NOCONFIRM;
	}
	else if (copyflag & CF_VERBOSE) {
		kanjifputs(src, stdout);
		fputnl(stdout);
	}

	flags = (copyflag & CF_VERIFY) ? O_RDWR : O_WRONLY;
	flags |= (O_BINARY | O_CREAT | O_TRUNC);

#ifndef	NODIRLOOP
	if (issamebody(src, file)) {
		flags |= O_EXCL;
		if (Xunlink(file) < 0) return(-1);
	}
#endif

	return(Xopen(file, flags, 0666));
}

static int NEAR textread(fd, buf, size, bin)
int fd;
u_char *buf;
int size, bin;
{
	u_char ch;
	int i, n;

	if (!(bin & (CF_BINARY | CF_TEXT))) bin = copyflag;
	if (!(bin & CF_TEXT)) n = sureread(fd, buf, size);
	else for (n = 0; n < size; n++) {
		i = sureread(fd, &ch, sizeof(u_char));
		if (i < 0) return(-1);
#if	MSDOS
		if (i >= (int)sizeof(u_char) && ch != C_EOF) buf[n] = ch;
#else
		if (i < (int)sizeof(u_char)) break;
		else if (ch != C_EOF) buf[n] = ch;
#endif
		else for (;;) {
			i = sureread(fd, &ch, sizeof(u_char));
			if (i < 0) return(-1);
			if (i < (int)sizeof(u_char) || ch == '\n') return(n);
		}
	}

	return(n);
}

static int NEAR textclose(fd, bin)
int fd, bin;
{
	u_char ch;
	int ret;

	ret = 0;
	if (!(bin & (CF_BINARY | CF_TEXT))) bin = copyflag;
	if (bin & CF_TEXT) {
		ch = C_EOF;
		if (surewrite(fd, &ch, sizeof(u_char)) < 0) ret = -1;
	}
	VOID_C Xclose(fd);

	return(ret);
}

static int NEAR doscopy(src, dest, stp, sbin, dbin, dfd)
CONST char *src, *dest;
struct stat *stp;
int sbin, dbin, dfd;
{
	off_t ofs;
	u_char buf[BUFSIZ], buf2[BUFSIZ];
	int i, n, fd1, fd2, tty, retry, duperrno;

#ifndef	NOSYMLINK
	if (dfd < 0 && (stp -> st_mode & S_IFMT) == S_IFLNK)
		return(cpsymlink(src, dest));
#endif

	if ((fd1 = Xopen(src, O_TEXT | O_RDONLY, stp -> st_mode)) < 0)
		return(-1);
	if (dfd >= 0) {
		fd2 = dfd;
		ofs = Xlseek(fd2, (off_t)0, L_INCR);
	}
	else if ((fd2 = writeopen(dest, src)) <= 0) {
		VOID_C Xclose(fd1);
		return(fd2);
	}
	else ofs = (off_t)0;

#if	MSDOS && !defined (LSI_C)
# ifdef	DJGPP
	if (isatty(fd1)) setmode(fd2, O_TEXT);
	else
# endif
	{
		setmode(fd1, O_BINARY);
		setmode(fd2, O_BINARY);
	}
#endif
	if (isatty(fd1) || isatty(fd2)) sbin = CF_TEXT;
	tty = isatty(fd2);

#ifdef	FAKEUNINIT
	duperrno = errno;	/* fake for -Wuninitialized */
#endif
	for (retry = 0; retry < 10; retry++) {
		for (;;) {
			if ((i = textread(fd1, buf, BUFSIZ, sbin)) <= 0) {
				duperrno = errno;
				break;
			}
			if (surewrite(fd2, buf, i) < 0) {
				i = -1;
				duperrno = errno;
				break;
			}
			if (i < BUFSIZ) break;
		}
		if (i >= 0 && !(copyflag & CF_VERIFY)) break;
		if (Xlseek(fd1, (off_t)0, L_SET) < (off_t)0
		|| Xlseek(fd2, ofs, L_SET) < (off_t)0)
			break;
		if (i < 0) continue;

		for (;;) {
			if ((i = textread(fd1, buf, BUFSIZ, sbin)) <= 0) {
				duperrno = errno;
				break;
			}
			if ((n = textread(fd2, buf2, BUFSIZ, CF_BINARY)) < 0) {
				duperrno = errno;
				i = -1;
				break;
			}
			if (n != i || memcmp((char *)buf, (char *)buf2, i)) {
				duperrno = EINVAL;
				i = -1;
				break;
			}
			if (i < BUFSIZ) break;
		}
		if (i >= 0) break;
		if (Xlseek(fd1, (off_t)0, L_SET) < (off_t)0
		|| Xlseek(fd2, ofs, L_SET) < (off_t)0)
			break;
	}

	if (dfd < 0) textclose(fd2, dbin);
	VOID_C Xclose(fd1);

	if (i < 0) {
		VOID_C Xunlink(dest);
		errno = duperrno;
		return(dfd < 0 ? -1 : -2);
	}

#ifdef	FD
	stp -> st_nlink = (TCH_ATIME | TCH_MTIME | TCH_IGNOREERR);
	if (!tty && touchfile(dest, stp) < 0) return(-1);
#else
	if (!tty) VOID_C touchfile(dest, stp);
#endif

	return(1);
}

int doscomcopy(argc, argv)
int argc;
char *CONST argv[];
{
	struct stat sst, dst;
	char *cp, *file, *form, **arg, *src, **wild;
	char dest[MAXPATHLEN];
	int i, j, n, nf, nc, *sbin, dbin, size, fd, ret;

	copyflag = 0;
	evalenvopt(argv[0], "COPYCMD", getcopyopt);
	if ((n = getcopyopt(argc, argv)) < 0)
		return(RET_FAIL);
	if (argc <= n) {
		doserror(NULL, ER_REQPARAM);
		return(RET_FAIL);
	}

	size = strlen(argv[n]) + 1;
	src = malloc2(size);
	strcpy(src, argv[n]);
	for (n++; n < argc; n++) {
		if ((cp = strchr(&(argv[n - 1][1]), '+')) && !*(++cp))
			/*EMPTY*/;
		else if (argv[n][0] != '+' && argv[n][0] != DOSCOMOPT) break;
		j = strlen(argv[n]);
		src = realloc2(src, size + j);
		strcpy(&(src[size - 1]), argv[n]);
		size += j;
	}

	for (cp = src, nf = 0; cp; nf++) {
		if ((cp = strchr(cp, '+'))) {
			*(cp++) = '\0';
			if (!*cp) {
				if (nf++) break;
				doserror(src, ER_COPIEDITSELF);
				free(src);
				return(RET_FAIL);
			}
		}
	}
	arg = (char **)malloc2(nf * sizeof(char *));
	sbin = (int *)malloc2(nf * sizeof(int));

	arg[0] = src;
	sbin[0] = copyflag;
	for (i = 1; i < nf; i++) {
		arg[i] = arg[i - 1] + strlen(arg[i - 1]) + 1;
		sbin[i - 1] = getbinmode(arg[i - 1], sbin[i - 1]);
		sbin[i] = sbin[i - 1];
	}
	sbin[i - 1] = getbinmode(arg[i - 1], sbin[i - 1]);

	if (n >= argc) {
		dest[0] = '\0';
		dbin = sbin[i - 1];
	}
#if	0
	else if (n + 1 < argc) {
		doserror(argv[n + 1], ER_TOOMANYPARAM);
		free(arg[0]);
		free(arg);
		free(sbin);
		return(RET_FAIL);
	}
#endif
	else {
		strcpy(dest, argv[n]);
		dbin = getbinmode(dest, sbin[i - 1]);
	}

	if (dbin < 0) {
		free(arg[0]);
		free(arg);
		free(sbin);
		return(RET_FAIL);
	}

	file = form = NULL;
	if (!*dest) file = dest;
	else if (Xstat(dest, &dst) >= 0) {
		if ((dst.st_mode & S_IFMT) == S_IFDIR)
			file = strcatdelim(dest);
	}
	else {
		file = getbasename(dest);
		if (strpbrk(file, "*?")) form = &(argv[n][file - dest]);
		else if (*file) file = NULL;
	}

	if (nf <= 1) {
		if (Xlstat(arg[0], &sst) < 0) /*EMPTY*/;
		else if ((sst.st_mode & S_IFMT) == S_IFDIR) {
			arg[0] = realloc2(arg[0], size + 2);
			strcpy(strcatdelim(arg[0]), "*");
		}
		else if (!*dest) {
			doserror(arg[0], ER_COPIEDITSELF);
			free(arg[0]);
			free(arg);
			free(sbin);
			return(RET_FAIL);
		}
	}

	ret = RET_SUCCESS;
	nc = 0;
	fd = -1;
	if (nf > 1 || strpbrk(arg[0], "*?")) {
		if (nf > 1 || !file) {
			if (!(copyflag & (CF_BINARY | CF_TEXT)))
				copyflag |= CF_TEXT;
			if ((fd = writeopen(dest, NULL)) <= 0) {
				if (fd < 0) ret = RET_FAIL;
				nf = 0;
			}
		}
		copyflag |= CF_VERBOSE;
	}
	for (n = 0; n < nf; n++) {
		if (!(wild = evalwild(arg[n], 0))) {
			if (nf > 1) continue;
			doserror(arg[n], ER_FILENOTFOUND);
			ret = RET_FAIL;
			ERRBREAK;
		}
		for (i = 0; wild[i]; i++) {
			if (Xlstat(wild[i], &sst) < 0) {
				dosperror(wild[i]);
				ret = RET_FAIL;
				ERRBREAK;
			}
			if ((sst.st_mode & S_IFMT) == S_IFDIR) continue;

			src = getbasename(wild[i]);
			if (form) convwild(file, src, form, arg[n]);
			else if (!n && file) strcpy(file, src);
			j = doscopy(wild[i], dest, &sst, sbin[n], dbin, fd);
			if (j < 0) {
				if (errno) dosperror(wild[i]);
				else doserror(wild[i], ER_COPIEDITSELF);
				ret = RET_FAIL;
				ERRBREAK;
			}
			if (j > 0) nc++;
			if (copyflag & CF_CANCEL) break;
		}
		freevar(wild);
		if (ret == RET_FAIL) ERRBREAK;
	}
	free(arg[0]);
	free(arg);
	free(sbin);
	if (fd >= 0) {
		if (fd > 0) textclose(fd, dbin);
		if (nc > 0) nc = 1;
	}
	if (!(copyflag & CF_CANCEL)) {
		fprintf2(stdout, "%9d file(s) copied", nc);
		fputnl(stdout);
	}

	return(ret);
}

/*ARGSUSED*/
int doscomcls(argc, argv)
int argc;
char *CONST argv[];
{
#ifdef	FD
	putterms(T_CLEAR);
	tflush();
#else
	fputs(t_clear, stdout);
	fflush(stdout);
#endif
	fputnl(stderr);

	return(RET_SUCCESS);
}

int doscomtype(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp;
	u_char ch;
	ALLOC_T size;
	int i, n, fd;
#if	MSDOS && !defined (LSI_C)
	int omode;
#endif

	if (checkarg(argc, argv) < 0) return(RET_FAIL);

	if ((fd = Xopen(argv[1], O_TEXT | O_RDONLY, 0666)) < 0) {
		dosperror(argv[1]);
		return(RET_FAIL);
	}
#if	MSDOS && !defined (LSI_C)
# ifdef	DJGPP
	if (isatty(fd)) omode = setmode(STDOUT_FILENO, O_TEXT);
	else
# endif
	{
		setmode(fd, O_BINARY);
		omode = setmode(STDOUT_FILENO, O_BINARY);
	}
#endif	/* MSDOS && !LSI_C */

	cp = c_realloc(NULL, 0, &size);
	i = 0;
	for (;;) {
		if ((n = textread(fd, &ch, sizeof(ch), CF_TEXT)) < 0) break;
		if (n < (int)sizeof(ch)) {
			n = surewrite(STDOUT_FILENO, cp, i);
			break;
		}
		else if (ch == '\n') {
			cp[i++] = '\n';
			if ((n = surewrite(STDOUT_FILENO, cp, i)) < 0) break;
			i = 0;
		}
		else {
			cp = c_realloc(cp, i, &size);
			cp[i++] = ch;
		}
	}
	free(cp);
	VOID_C Xclose(fd);
#if	MSDOS && !defined (LSI_C)
	if (omode >= 0) setmode(STDOUT_FILENO, omode);
#endif

	if (n < 0) {
		dosperror(argv[1]);
		return(RET_FAIL);
	}

	return(RET_SUCCESS);
}
#endif	/* DOSCOMMAND && (!FD || (FD >= 2 && !_NOORIGSHELL)) */
