/*
 *	doscom.c
 *
 *	builtin commands for DOS
 */

#ifdef	FD
#include "fd.h"
#include "term.h"
#include "types.h"
#else
#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#endif

#include "dirent.h"
#include "sysemu.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#if	defined (FD) || defined (WITHNETWORK)
#include "realpath.h"
#endif
#ifdef	DEP_URLPATH
#include "urldisk.h"
#endif
#if	MSDOS && !defined (FD)
#define	gettext			dummy_gettext	/* fake for DJGPP gcc-3.3 */
#include <conio.h>
#endif

#if	MSDOS && !defined (FD)
#define	VOL_FAT32		"FAT32"
# ifdef	DOUBLESLASH
# define	MNTDIRSIZ	MAXPATHLEN
# else
# define	MNTDIRSIZ	(3 + 1)
# endif
#endif	/* MSDOS && !FD */

#if	MSDOS
#define	DOSCOMOPT		'/'
#define	C_EOF			K_CTRL('Z')
#else
#define	DOSCOMOPT		'-'
#define	C_EOF			K_CTRL('D')
#endif

#define	COPYRETRY		10
#define	DIRSORTFLAG		"NSEDGA"
#define	DIRATTRFLAG		"DRHSA"
#define	DEFDIRATTR		"-H-S"
#define	dir_isdir(sp)		(((((sp) -> mod) & S_IFMT) == S_IFDIR) ? 1 : 0)

#if	!defined (FD) && !defined (WITHNETWORK)
# if	MSDOS
# define	Xrealpath(p, r, f) \
				unixrealpath(p, r)
# else
# define	Xrealpath(p, r, f) \
				realpath(p, r)
# endif
#endif	/* !FD && !WITHNETWORK */

struct filestat_t {
	char *nam;
#ifdef	DEP_DOSLFN
	char *d_alias;
#else
#define	d_alias			nam
#endif
#ifndef	NOSYMLINK
	char *lnam;
#endif
	u_short mod;
	off_t siz;
	time_t mtim;
	time_t atim;
	u_char flags;
};

#define	FS_NOTMATCH		0001
#define	FS_BADATTR		0002

#if	defined (DOSCOMMAND) && defined (DEP_ORIGSHELL)

#ifdef	FD
#define	c_left			termstr[C_LEFT]
#else	/* !FD */
# if	MSDOS
# define	cc_intr		K_CTRL('C')
# define	K_CR		'\r'
# else	/* !MSDOS */
extern int ttyio;
static int cc_intr = -1;
# define	K_CR		'\n'
# endif	/* !MSDOS */
#define	K_CTRL(c)		((c) & 037)
#define	K_BS			010
#define	n_line			24
#define	t_clear			"\033[;H\033[2J"
#define	c_left			"\010"
#endif	/* !FD */

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
#  define	statfs2(p, b)	(statfs(p, b) - 1)
#  define	blocksize(fs)	1024
#  define	f_blocks	fd_req.btot
#  define	f_bavail	fd_req.bfreen
#  endif	/* USEFSDATA */
#  ifdef	USEFFSIZE
#  define	f_bsize		f_fsize
#  endif
#  if	defined (USESTATFSH) || defined (USEVFSH) || defined (USEMOUNTH)
#   if	(STATFSARGS >= 4)
#   define	statfs2(p, b)	statfs(p, b, sizeof(statfs_t), 0)
#   else
#    if	(STATFSARGS == 3)
#    define	statfs2(p, b)	statfs(p, b, sizeof(statfs_t))
#    else
#    define	statfs2		statfs
#    endif
#   endif
#  endif	/* USESTATFSH || USEVFSH || USEMOUNTH */
# endif	/* !MSDOS */
#endif	/* !FD */

#ifdef	DEBUG
extern char *_mtrace_file;
#endif

#ifdef	FD
extern int getinfofs __P_((CONST char *, off_t *, off_t *, off_t *));
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
# define	ttyiomode(n)
# define	stdiomode()
# define	getkey3(n, c, t) \
				getch();
# else	/* !MSDOS */
static VOID NEAR ttymode __P_((int));
# define	ttyiomode(n)	(ttymode(1))
# define	stdiomode()	(ttymode(0))
static int NEAR getkey3 __P_((int, int, int));
# endif	/* !MSDOS */
static int NEAR touchfile __P_((CONST char *, struct stat *));
# ifndef	NODIRLOOP
static int NEAR issamebody __P_((CONST char *, CONST char *));
# endif
# ifndef	NOSYMLINK
static int NEAR cpsymlink __P_((CONST char *, CONST char *));
# endif
#endif	/* !FD */

static VOID NEAR doserror __P_((CONST char *, int));
static VOID NEAR dosperror __P_((CONST char *));
static VOID NEAR fputsize __P_((off_t *, off_t *));
static int NEAR inputkey __P_((VOID_A));
static int cmpdirent __P_((CONST VOID_P, CONST VOID_P));
static int NEAR filterattr __P_((CONST struct filestat_t *));
static VOID NEAR evalenvopt __P_((CONST char *, CONST char *,
		int (NEAR *)__P_((int, char *CONST []))));
static int NEAR adddirflag __P_((char *, CONST char *, CONST char *));
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
static int NEAR textread __P_((int, char *, int, int, int, off_t *, off_t));
static VOID NEAR textclose __P_((int, int));
static int NEAR doscopy __P_((CONST char *, CONST char *,
		struct stat *, int, int, int));
int doscomcopy __P_((int, char *CONST []));
int doscomcls __P_((int, char *CONST []));
int doscomtype __P_((int, char *CONST []));

static CONST char *doserrstr[] = {
	NULL,
#define	ER_REQPARAM		1
	"Required parameter missing",
#define	ER_TOOMANYPARAM		2
	"Too many parameters",
#define	ER_FILENOTFOUND		3
	"File not found",
#define	ER_INVALIDPARAM		4
	"Invalid parameter",
#define	ER_DUPLFILENAME		5
	"Duplicate file name or file in use",
#define	ER_COPIEDITSELF		6
	"File cannot be copied onto itself",
#define	ER_INVALIDSW		7
	"Invalid switch",
};
#define	DOSERRSIZ		arraysize(doserrstr)
static char dirsort[strsize(DIRSORTFLAG) * 2 + 1] = "";
static char dirattr[strsize(DIRATTRFLAG) * 2 + 1] = "";
static int dirline = 0;
static char *dirwd = NULL;
static int dirtype = '\0';
static int dirflag = 0;
#define	DF_LOWER		001
#define	DF_PAUSE		002
#define	DF_SUBDIR		004
#define	DF_CANCEL		010
static int copyflag = 0;
#define	CF_BINARY		001
#define	CF_TEXT			002
#define	CF_VERIFY		004
#define	CF_NOCONFIRM		010
#define	CF_VERBOSE		020
#define	CF_CANCEL		040


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
# else	/* !MSDOS */
	struct stat st;
# endif	/* !MSDOS */
	statfs_t fsbuf;
	int n;

	n = 0;
	*totalp = *freep = *bsizep = (off_t)0;
# if	MSDOS
#  ifdef	DOUBLESLASH
	if ((len = isdslash(path))) {
		if ((cp = strdelim(&(path[len]), 0))) {
			len = cp - path;
			if (!(cp = strdelim(&(path[len + 1]), 0)))
				cp += strlen(cp);
			len = cp - path;
		}
		Xstrncpy(drv, path, len);
	}
	else
#  endif
	VOID_C gendospath(drv, Xtoupper(path[0]), _SC_);

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
		if (intcall(0x21, &reg, &sreg) < 0) n = -1;
		else {
#  ifdef	DJGPP
			dosmemget(__tb, sizeof(fsbuf), &fsbuf);
#  endif
			*totalp = (off_t)(fsbuf.f_blocks);
			*freep = (off_t)(fsbuf.f_bavail);
			*bsizep = (off_t)(fsbuf.f_clustsize)
				* (off_t)(fsbuf.f_sectsize);
		}
	}
	else {
		reg.x.ax = 0x3600;
		reg.h.dl = drv[0] - 'A' + 1;
		intcall(0x21, &reg, &sreg);
		if (reg.x.ax == 0xffff) n = seterrno(ENOENT);
		else {
			*totalp = (off_t)(reg.x.dx);
			*freep = (off_t)(reg.x.bx);
			*bsizep = (off_t)(reg.x.ax) * (off_t)(reg.x.cx);
		}
	}
	if (!*totalp || !*bsizep || *totalp < *freep) n = seterrno(EIO);
# else	/* !MSDOS */
	if (statfs2(path, &fsbuf) < 0) n = -1;
	else {
		*totalp = fsbuf.f_blocks;
		*freep = fsbuf.f_bavail;
		*bsizep = blocksize(fsbuf);
	}
# endif	/* !MSDOS */

	if (n < 0) {
# if	MSDOS
		*bsizep = (off_t)1024;
# else
		if (Xstat(path, &st) < 0) *bsizep = (off_t)DEV_BSIZE;
		else *bsizep = (off_t)(st.st_blksize);
# endif
	}

	return(n);
}

# if	!MSDOS
static VOID NEAR ttymode(on)
int on;
{
#  if	!defined (USETERMIOS) && !defined (USETERMIO)
	struct tchars cc;
#  endif
	termioctl_t tty;

	if (ttyio < 0 || tioctl(ttyio, REQGETP, &tty) < 0) return;
#  if	defined (USETERMIOS) || defined (USETERMIO)
	if (cc_intr < 0) cc_intr = tty.c_cc[VINTR];
	if (on) tty.c_lflag &= ~(ECHO | ICANON | ISIG);
	else tty.c_lflag |= (ECHO | ICANON | ISIG);
#  else	/* !USETERMIO && !USETERMIO */
	if (cc_intr < 0) {
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
static int NEAR getkey3(sig, code, timeout)
int sig, code, timeout;
{
	u_char uc;
	int i;

	if (ttyio < 0) return(EOF);
	while ((i = read(ttyio, &uc, sizeof(uc))) < 0 && errno == EINTR);
	if (i < (int)sizeof(uc)) return(EOF);

	return((int)uc);
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
	if (st1.st_dev != st2.st_dev || st1.st_ino != st2.st_ino) return(0);
#  ifdef	DEP_URLPATH
	if (st1.st_dev == (dev_t)-1 || st1.st_ino == (ino_t)-1) return(0);
#  endif

	return(1);
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
	if (Xsymlink(path, dest) < 0) return(-1);

	return(1);
}
# endif	/* !NOSYMLINK */
#endif	/* !FD */

static VOID NEAR doserror(s, n)
CONST char *s;
int n;
{
	if (!n || n >= DOSERRSIZ) return;
	if (s) {
		kanjifputs(s, Xstderr);
		VOID_C fputnl(Xstderr);
	}
	if (doserrstr[n]) Xfputs(doserrstr[n], Xstderr);
	VOID_C fputnl(Xstderr);
}

static VOID NEAR dosperror(s)
CONST char *s;
{
	int duperrno;

	duperrno = errno;
	if (errno < 0) return;
	if (s) {
		kanjifputs(s, Xstderr);
		VOID_C fputnl(Xstderr);
	}
	Xfputs(Xstrerror(duperrno), Xstderr);
	VOID_C fputnl(Xstderr);
	errno = 0;
}

static VOID NEAR fputsize(np, bsizep)
off_t *np, *bsizep;
{
	off_t s, mb;

	if (*np < (off_t)0 || !*bsizep) {
		VOID_C Xprintf("%15.15s bytes", "?");
		return;
	}

	s = ((off_t)1024 * (off_t)1024 + (*bsizep / 2)) / *bsizep;
	mb = *np / s;
	if (mb < (off_t)1024) VOID_C Xprintf("%'15qd bytes", *np * *bsizep);
	else VOID_C Xprintf("%'12qd.%02qd MB",
		mb, ((*np - mb * s) * (off_t)100) / s);
}

static int NEAR inputkey(VOID_A)
{
	int c;

	ttyiomode(1);
	c = getkey3(0, inputkcode, 0);
	stdiomode();
	if (c == EOF) c = -1;
	else if (c == cc_intr) {
		Xfputs("^C", Xstdout);
		VOID_C fputnl(Xstdout);
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
	if (!Xstrchr(dirsort, 'E')) cp1 = cp2 = NULL;
	else {
		if ((cp1 = Xstrrchr(sp1 -> nam, '.'))) *cp1 = '\0';
		if ((cp2 = Xstrrchr(sp2 -> nam, '.'))) *cp2 = '\0';
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

static int NEAR filterattr(dp)
CONST struct filestat_t *dp;
{
	int i, f;

	for (i = 0; dirattr[i]; i++) {
		f = 0;
		if (dirattr[i] == '-') {
			i++;
			f++;
		}

		switch (dirattr[i]) {
			case 'D':
				if (!(dir_isdir(dp))) f ^= 1;
				break;
			case 'R':
				if (dp -> mod & S_IWUSR) f ^= 1;
				break;
			case 'H':
				if (dp -> mod & S_IRUSR) f ^= 1;
				break;
			case 'S':
				if ((dp -> mod & S_IFMT) != S_IFSOCK) f ^= 1;
				break;
			case 'A':
				if (!(dp -> mod & S_ISVTX)) f ^= 1;
				break;
			default:
				break;
		}

		if (f) return(-1);
	}

	return(0);
}

static VOID NEAR evalenvopt(cmd, env, getoptcmd)
CONST char *cmd, *env;
int (NEAR *getoptcmd)__P_((int, char *CONST []));
{
	char *cp, **argv;
	int n, er, argc;

	if (!(cp = getshellvar(env, -1)) || !*cp) return;
	argv = (char **)Xmalloc(3 * sizeof(char *));
	argc = 2;
	argv[0] = Xstrdup(cmd);
	argv[1] = Xstrdup(cp);
	argv[2] = NULL;
	argc = evalifs(argc, &argv, IFS_SET);

	er = 0;
	if ((n = (*getoptcmd)(argc, argv)) < 0) er++;
	else if (argc > n) {
		er++;
		doserror(argv[n], ER_TOOMANYPARAM);
	}
	if (er) Xfputs("(Error occurred in environment variable)\n\n",
		Xstdout);
	freevar(argv);
}

static int NEAR adddirflag(buf, arg, flags)
char *buf;
const char *arg, *flags;
{
	int i, n, r;

	n = r = 0;
	arg += 2;
	for (i = 0; arg[i]; i++) {
		if (Xstrchr(flags, arg[i])) {
			if (!Xmemchr(buf, arg[i], n))
			buf[n++] = arg[i];
			r = 0;
		}
		else if (arg[i] == '-' && !r) {
			buf[n++] = '-';
			r = 1;
		}
		else {
			doserror(arg, ER_INVALIDSW);
			return(-1);
		}
	}

	if (r) {
		doserror(arg, ER_INVALIDSW);
		return(-1);
	}

	return(n);
}

static int NEAR getdiropt(argc, argv)
int argc;
char *CONST argv[];
{
	char *arg;
	int i, j, n, r;

	for (i = 1; i < argc; i++) {
		arg = argv[i];
		if (arg[0] != DOSCOMOPT) break;
		for (j = 1; arg[j]; j++) arg[j] = Xtoupper(arg[j]);
		r = (arg[1] == '-') ? 1 : 0;
		switch (arg[1 + r]) {
			case 'P':
				if (r) dirflag &= ~DF_PAUSE;
				else dirflag |= DF_PAUSE;
				break;
			case 'W':
				if (r) {
					if (dirtype == 'W') dirtype = '\0';
					break;
				}
				if (dirtype != 'B') dirtype = 'W';
				break;
			case 'A':
				if (r) {
					Xstrcpy(dirattr, DEFDIRATTR);
					break;
				}
				n = adddirflag(dirattr, arg, DIRATTRFLAG);
				if (n < 0) return(-1);
				dirattr[n] = '\0';
				continue;
/*NOTREACHED*/
				break;
			case 'O':
				if (r) {
					dirsort[0] = '\0';
					break;
				}
				n = adddirflag(dirsort, arg, DIRSORTFLAG);
				if (n < 0) return(-1);
				if (!n) dirsort[n++] = 'N';
				dirsort[n] = '\0';
				continue;
/*NOTREACHED*/
				break;
			case 'S':
				if (r) dirflag &= ~DF_SUBDIR;
				else dirflag |= DF_SUBDIR;
				break;
			case 'B':
				if (r) {
					if (dirtype == 'B') dirtype = '\0';
					break;
				}
				dirtype = 'B';
				break;
			case 'L':
				if (r) dirflag &= ~DF_LOWER;
				else dirflag |= DF_LOWER;
				break;
#ifndef	MINIMUMSHELL
			case 'V':
				if (r) {
					if (dirtype == 'V') dirtype = '\0';
					break;
				}
				if (!dirtype) dirtype = 'V';
				break;
			case '4':
				if (r) {
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
		if (*(arg += 2 + r)) {
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
	if (lower) for (i = 0; s[i]; i++) s[i] = Xtolower(s[i]);
	else for (i = 0; s[i]; i++) s[i] = Xtoupper(s[i]);
	kanjifputs(s, Xstdout);

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

	Xstrcpy(buf, dirp -> d_alias);
	if (isdotdir(buf)) ext = NULL;
	else if ((ext = Xstrrchr(buf, '.'))) {
		if (ext == buf) {
			ext = NULL;
			Xstrcpy(buf, &(dirp -> d_alias[1]));
		}
		else {
			*(ext++) = '\0';
			i = ext - buf;
			ext = &(dirp -> d_alias[i]);
		}
	}
	showstr(buf, 8, (dirflag & DF_LOWER));
	Xfputc(' ', Xstdout);

	if (!ext) Xfputs("   ", Xstdout);
	else {
		Xstrcpy(buf, ext);
		showstr(buf, 3, (dirflag & DF_LOWER));
	}
	Xfputc(' ', Xstdout);

	switch (dirp -> mod & S_IFMT) {
		case S_IFREG:
#ifndef	NOSYMLINK
		case S_IFLNK:
#endif
			VOID_C Xprintf("%'13qd", dirp -> siz);
			break;
		case S_IFDIR:
			Xfputs("  <DIR>      ", Xstdout);
			break;
		default:
			Xfputs("             ", Xstdout);
			break;
	}

#ifndef	MINIMUMSHELL
	if (verbose > 0) {
		if ((dirp -> mod & S_IFMT) != S_IFREG)
			Xfputs("              ", Xstdout);
		else {
			dirp -> siz = ((dirp -> siz + *bsizep - 1) / *bsizep)
				* *bsizep;
			VOID_C Xprintf("%'14qd", dirp -> siz);
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
	Xfputs("  ", Xstdout);
#ifndef	MINIMUMSHELL
	if (verbose < 0) VOID_C Xprintf("%04d-%02d-%02d  %2d:%02d ",
		tm -> tm_year + 1900, tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min);
	else
#endif
	VOID_C Xprintf("%02d-%02d-%02d  %2d:%02d ",
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
		VOID_C Xprintf(" %02d-%02d-%02d  ",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
		Xstrcpy(buf, "           ");
		if (!(dirp -> mod & S_IWUSR)) buf[0] = 'R';
		if (!(dirp -> mod & S_IRUSR)) buf[1] = 'H';
		if ((dirp -> mod & S_IFMT) == S_IFSOCK) buf[2] = 'S';
		if ((dirp -> mod & S_IFMT) == S_IFIFO) buf[3] = 'L';
		if (dir_isdir(dirp)) buf[4] = 'D';
		if (dirp -> mod & S_ISVTX) buf[5] = 'A';
		Xfputs(buf, Xstdout);
	}
#endif	/* !MINIMUMSHELL */

	kanjifputs(dirp -> nam, Xstdout);
#ifndef	NOSYMLINK
	if (dirp -> lnam) VOID_C Xprintf(" -> %s", dirp -> lnam);
#endif
	VOID_C fputnl(Xstdout);
}

static VOID NEAR showfnamew(dirp)
struct filestat_t *dirp;
{
	char *ext, buf[MAXNAMLEN + 1];
	int i, len;

	if (dir_isdir(dirp)) Xfputc('[', Xstdout);
	Xstrcpy(buf, dirp -> d_alias);
	if (isdotdir(buf)) ext = NULL;
	else if ((ext = Xstrrchr(buf, '.'))) {
		if (ext == buf) {
			ext = NULL;
			Xstrcpy(buf, &(dirp -> d_alias[1]));
		}
		else {
			*(ext++) = '\0';
			i = ext - buf;
			ext = &(dirp -> d_alias[i]);
		}
	}
	len = showstr(buf, -8, (dirflag & DF_LOWER));

	if (ext) {
		Xfputc('.', Xstdout);
		len++;
		Xstrcpy(buf, ext);
		len += showstr(buf, -3, (dirflag & DF_LOWER));
	}

	if (dir_isdir(dirp)) Xfputc(']', Xstdout);
	for (; len < 8 + 1 + 3; len++) Xfputc(' ', Xstdout);
}

static VOID NEAR showfnameb(dirp)
struct filestat_t *dirp;
{
	char *cp, buf[MAXPATHLEN];
	int i;

	if (dirflag & DF_SUBDIR) {
		cp = dirwd;
		if (dirflag & DF_LOWER) {
			for (i = 0; dirwd[i]; i++) buf[i] = Xtolower(dirwd[i]);
			buf[i] = '\0';
			cp = buf;
		}
		VOID_C Xprintf("%k%c", cp, _SC_);
	}

	if (dirflag & DF_LOWER)
		for (i = 0; dirp -> nam[i]; i++)
			dirp -> nam[i] = Xtolower(dirp -> nam[i]);
	kanjifputs(dirp -> nam, Xstdout);
	VOID_C fputnl(Xstdout);
}

#ifdef	MINIMUMSHELL
#define	n_incline		1
static int NEAR _checkline(VOID_A)
#else
static int NEAR _checkline(n_incline)
int n_incline;
#endif
{
	if (!(dirflag & DF_PAUSE)) return(0);
	if (dirflag & DF_CANCEL) return(-1);
	if ((dirline += n_incline) >= n_line - 2) {
		Xfputs("Press any key to continue . . .", Xstdout);
		VOID_C fputnl(Xstdout);
		if (inputkey() < 0) {
			dirflag |= DF_CANCEL;
			return(-1);
		}
		VOID_C Xprintf("\n(continuing %k)\n", dirwd);
		dirline = n_incline;
	}

	return(0);
}

static VOID NEAR dosdirheader(VOID_A)
{
	if (dirtype != 'B') {
		if (checkline(1) < 0) return;
		if (dirflag & DF_SUBDIR) {
			VOID_C fputnl(Xstdout);
			if (checkline(1) < 0) return;
		}
		else Xfputc(' ', Xstdout);
		VOID_C Xprintf("Directory of %k\n", dirwd);
	}
#ifndef	MINIMUMSHELL
	if (dirtype == 'V') {
		if (checkline(1) < 0) return;
		Xfputs("File Name         Size        Allocated      ",
			Xstdout);
		Xfputs("Modified      Accessed  Attrib\n", Xstdout);
		if (checkline(1) < 0) return;
		VOID_C fputnl(Xstdout);
		if (checkline(1) < 0) return;
		VOID_C fputnl(Xstdout);
	}
#endif	/* !MINIMUMSHELL */
	if (dirtype != 'B') {
		if (checkline(1) < 0) return;
		VOID_C fputnl(Xstdout);
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
		Xfputs("File not found\n", Xstdout);
		nd = -1;
	}
	else {
		if (fp && (dirflag & DF_SUBDIR)) {
			if (checkline(1) < 0) return;
			VOID_C fputnl(Xstdout);
			if (checkline(1) < 0) return;
			Xfputs("Total files listed:\n", Xstdout);
		}
		if (checkline(1) < 0) return;
		VOID_C Xprintf("%10d file(s)%'15qd bytes\n", nf, *sump);
#ifndef	MINIMUMSHELL
		if (dirtype == 'V') {
			if (checkline(1) < 0) return;
			VOID_C Xprintf("%10d dir(s) ", nd);
			fputsize(bsump, bsizep);
			Xfputs(" allocated\n", Xstdout);
			nd = -1;
		}
#endif	/* !MINIMUMSHELL */
	}
	if (!fp) return;

	if (checkline(1) < 0) return;
	if (nd < 0) Xfputs("                  ", Xstdout);
	else VOID_C Xprintf("%10d dir(s) ", nd);

	fputsize(fp, bsizep);
	Xfputs(" free\n", Xstdout);

#ifndef	MINIMUMSHELL
	if (tp && dirtype == 'V') {
		if (checkline(1) < 0) return;
		Xfputs("                  ", Xstdout);
		fputsize(tp, bsizep);
		VOID_C Xprintf(" total disk space, %3d%% in use\n",
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
#ifndef	NOSYMLINK
	char lnam[MAXPATHLEN];
	int llen;
#endif
	struct filestat_t *dirlist;
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	char *file;
	off_t sum;
	int c, n, nf, nd, len, max;

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
		Xstrcpy(file, dp -> d_name);
#ifndef	NOSYMLINK
		if (Xlstat(dirwd, &st) < 0 || (st.st_mode & S_IFMT) != S_IFLNK)
			llen = -1;
		else llen = Xreadlink(dirwd, lnam, sizeof(lnam) - 1);
#endif
		if (Xstat(dirwd, &st) < 0) continue;

		dirlist = b_realloc(dirlist, n, struct filestat_t);
		dirlist[n].nam = Xstrdup(dp -> d_name);
#ifdef	DEP_DOSLFN
		dirlist[n].d_alias = (dp -> d_alias[0])
			? Xstrdup(dp -> d_alias) : dirlist[n].nam;
#endif
#ifndef	NOSYMLINK
		if (llen < 0) dirlist[n].lnam = NULL;
		else {
			lnam[llen] = '\0';
			dirlist[n].lnam = Xstrdup(lnam);
		}
#endif
		dirlist[n].mod = st.st_mode;
		dirlist[n].siz = st.st_size;
		dirlist[n].mtim = st.st_mtime;
		dirlist[n].atim = st.st_atime;
		dirlist[n].flags = 0;
		if (filterattr(&(dirlist[n])) < 0)
			dirlist[n].flags |= FS_BADATTR;
		else if (re && !regexp_exec(re, dirlist[n].nam, 0))
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
	VOID_C Xclosedir(dirp);
	dirwd[len] = '\0';
	max = n;
	if (*dirsort) qsort(dirlist, max, sizeof(*dirlist), cmpdirent);

	*nfp += nf;
	*ndp += nd;
	*sump += sum;
#ifndef	MINIMUMSHELL
	*bsump += bsum;
#endif
	dosdirheader();

	for (n = c = 0; n < max; n++) {
		if (dirflag & DF_CANCEL) break;
		if (dirlist[n].flags & (FS_NOTMATCH | FS_BADATTR)) continue;

		switch (dirtype) {
			case 'W':
				if (!(c % 5) && checkline(1) < 0) break;
				showfnamew(&(dirlist[n]));
				if (c != (nf + nd - 1) && (c % 5) != 5 - 1)
					Xfputc('\t', Xstdout);
				else VOID_C fputnl(Xstdout);
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
			Xstrcpy(file, dirlist[n].nam);
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
		Xfree(dirlist[n].nam);
#ifdef	DEP_DOSLFN
		if (dirlist[n].d_alias != dirlist[n].nam)
			Xfree(dirlist[n].d_alias);
#endif
#ifndef	NOSYMLINK
		Xfree(dirlist[n].lnam);
#endif
	}
	Xfree(dirlist);

	return(max);
}

int doscomdir(argc, argv)
int argc;
char *CONST argv[];
{
#ifndef	MINIMUMSHELL
	off_t bsum;
#endif
#ifdef	DEP_PSEUDOPATH
	int drv;
#endif
	reg_t *re;
	struct stat st;
	CONST char *cp, *dir, *file;
	char *tmp, wd[MAXPATHLEN], cwd[MAXPATHLEN], buf[MAXPATHLEN];
	off_t sum, total, fre, bsize;
	int i, n, nf, nd;

	Xstrcpy(dirattr, DEFDIRATTR);
	dirsort[0] = dirtype = '\0';
	dirflag = 0;
	evalenvopt(argv[0], "DIRCMD", getdiropt);
	if ((n = getdiropt(argc, argv)) < 0) return(RET_FAIL);

#ifdef	DEP_PSEUDOPATH
	drv = -1;
#endif

	dir = buf;
	file = "*";
	if (argc <= n) dir = curpath;
	else if (argc > n + 1) {
		doserror(argv[n + 1], ER_TOOMANYPARAM);
		return(RET_FAIL);
	}
	else {
		Xstrcpy(buf, argv[n]);
#ifdef	DEP_PSEUDOPATH
		if ((drv = preparedrv(buf, NULL, NULL)) < 0) {
			dosperror(buf);
			return(RET_FAIL);
		}
#endif
		if (Xstat(buf, &st) >= 0 && (st.st_mode & S_IFMT) == S_IFDIR)
			/*EMPTY*/;
		else if ((tmp = strrdelim(buf, 1))) {
			i = tmp - buf;
			if (isrootdir(buf) || *tmp != _SC_) tmp++;
			*tmp = '\0';
			if (argv[n][++i]) file = &(argv[n][i]);
		}
#ifdef	DEP_URLPATH
		else if (checkdrv(drv, NULL) == DEV_URL)
			Xrealpath(buf, buf, 0);
#endif
		else {
			dir = curpath;
			file = argv[n];
		}
	}

	if (!Xgetwd(cwd)) {
#ifdef	DEP_PSEUDOPATH
		shutdrv(drv);
#endif
		dosperror(NULL);
		return(RET_FAIL);
	}

	if (dir != buf) Xstrcpy(wd, cwd);
	else if ((cp = getrealpath(buf, wd, cwd)) == wd) /*EMPTY*/;
#ifdef	DOUBLESLASH
	else if (cp == buf && isdslash(buf)) Xrealpath(buf, wd, RLP_READLINK);
#endif
	else {
#ifdef	DEP_PSEUDOPATH
		shutdrv(drv);
#endif
		dosperror(cp);
		return(RET_FAIL);
	}
	if (getinfofs(wd, &total, &fre, &bsize) < 0) total = fre = (off_t)-1;

	if (dirtype != 'B') VOID_C fputnl(Xstdout);

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
#ifdef	DEP_PSEUDOPATH
	shutdrv(drv);
#endif
	regexp_free(re);
	if (n < 0) {
		dosperror(dir);
		return(RET_FAIL);
	}

#ifdef	MINIMUMSHELL
	dosdirfooter(&bsize, nf, nd, &sum, &fre);
#else
	dosdirfooter(&bsize, nf, nd, &sum, &bsum, &fre, &total);
#endif

	Xfflush(Xstdout);

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
			Xfputs("All files in directory will be deleted!",
				Xstdout);
			VOID_C fputnl(Xstdout);
#ifdef	FD
			ttyiomode(1);
			buf = inputstr("Are you sure (Y/N)?", 0, 0, NULL, -1);
			stdiomode();
#else
			buf = gets2("Are you sure (Y/N)?");
#endif
			if (!buf) return(RET_SUCCESS);
			if (!isatty(STDIN_FILENO)) VOID_C fputnl(Xstdout);
			c = *buf;
			Xfree(buf);
		} while (!Xstrchr("ynYN", c));
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
				VOID_C Xprintf("%k,    Delete (Y/N)?",
					wild[i]);
				Xfflush(Xstdout);
				if ((c = inputkey()) < 0) {
					freevar(wild);
					return(ret);
				}
				if (c <= (int)MAXUTYPE(u_char) && Xisprint(c))
					Xfputc(c, Xstdout);
				VOID_C fputnl(Xstdout);
			} while (!Xstrchr("ynYN", c));
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
			for (rest = j; src[rest]; rest++) /*EMPTY*/;
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
		Xstrcpy(new, wild[i]);
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
		for (j = 1; arg[j]; j++) arg[j] = Xtoupper(arg[j]);
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
	if (!(cp = Xstrchr(name, DOSCOMOPT)) || !cp[1] || cp[2]) /*EMPTY*/;
	else if (Xtoupper(*cp) == 'B') bin = CF_BINARY;
	else if (Xtoupper(*cp) == 'A') bin = CF_TEXT;

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
		if (src
		&& Xrealpath(src, buf, RLP_READLINK)
		&& Xrealpath(file, buf2, RLP_READLINK)
		&& !strpathcmp(buf, buf2))
			return(seterrno(0));
		if ((fd = Xopen(file, O_RDONLY, 0666)) < 0) return(fd);
		c = (isatty(fd)) ? 0 : 1;
		VOID_C Xclose(fd);
	}

	if (c) {
		VOID_C Xfprintf(Xstderr, "Overwrite %k (Yes/No/All)?", file);
		Xfflush(Xstderr);
		key = -1;
		for (;;) {
			if ((c = inputkey()) < 0) {
				copyflag |= CF_CANCEL;
				return(0);
			}
			if (c == K_CR && key >= 0) break;
			if (!Xstrchr("ynaYNA", c)) continue;
			key = Xtoupper(c);
			VOID_C Xfprintf(Xstderr, "%c%s", c, c_left);
			Xfflush(Xstderr);
		}
		VOID_C fputnl(Xstderr);
		if (key == 'N') return(0);
		else if (key == 'A') copyflag |= CF_NOCONFIRM;
	}
	else if (src && (copyflag & CF_VERBOSE)) {
		kanjifputs(src, Xstdout);
		VOID_C fputnl(Xstdout);
	}

	flags = (copyflag & CF_VERIFY) ? O_RDWR : O_WRONLY;
	flags |= (O_BINARY | O_CREAT | O_TRUNC);

#ifndef	NODIRLOOP
	if (src && issamebody(src, file)) {
		flags |= O_EXCL;
		if (Xunlink(file) < 0) return(-1);
	}
#endif

	return(Xopen(file, flags, 0666));
}

static int NEAR textread(fd, buf, size, bin, timeout, totalp, max)
int fd;
char *buf;
int size, bin, timeout;
off_t *totalp, max;
{
	off_t total;
	u_char uc;
	int i, n;

	if (!(bin & (CF_BINARY | CF_TEXT))) bin = copyflag;
	if (!totalp) total = (off_t)0;
	else {
		total = *totalp;
		if (total == max || (max >= (off_t)0 && total > max))
			return(0);
	}

	if (!(bin & CF_TEXT)) {
		i = checkread(fd, buf, size, timeout);
		if (i < 0) return(-1);
		total += (off_t)i;
		n = i;
	}
	else for (n = i = 0; n < size; n++) {
		i = checkread(fd, &uc, sizeof(uc), timeout);
		if (i < 0) return(-1);
		total += (off_t)i;
#if	MSDOS
		if (!i) /*EMPTY*/;
#else
		if (!i) break;
#endif
		else if (uc != C_EOF) {
			buf[n] = uc;
			continue;
		}

		for (;;) {
			i = checkread(fd, &uc, sizeof(uc), timeout);
			if (i < 0) return(-1);
			total = max;
			if (!i || uc == '\n') break;
		}
		break;
	}

	if (totalp) {
		if (i) /*EMPTY*/;
		else if (total < max) return(seterrno(ETIMEDOUT));
		else total = max;
		*totalp = total;
	}

	return(n);
}

static VOID NEAR textclose(fd, bin)
int fd, bin;
{
	u_char uc;
	int duperrno;

	duperrno = errno;
	if (!(bin & (CF_BINARY | CF_TEXT))) bin = copyflag;
	if (bin & CF_TEXT) {
		uc = C_EOF;
		VOID_C surewrite(fd, &uc, sizeof(uc));
	}
	safeclose(fd);
	errno = duperrno;
}

static int NEAR doscopy(src, dest, stp, sbin, dbin, dfd)
CONST char *src, *dest;
struct stat *stp;
int sbin, dbin, dfd;
{
	char *cp, buf[BUFSIZ], buf2[BUFSIZ];
	off_t ofs, total;
	int n, nread, retry, size, duperrno;
	int fd1, fd2, tty1, tty2, timeout1, timeout2;

#ifndef	NOSYMLINK
	if ((stp -> st_mode & S_IFMT) == S_IFLNK) {
		if (dfd < 0) {
# ifdef	DEP_URLPATH
			fd1 = urlpath(src, NULL, NULL, NULL);
			fd2 = urlpath(dest, NULL, NULL, NULL);
			if (fd1 != fd2) /*EMPTY*/;
			else
# endif
			return(cpsymlink(src, dest));
		}
		if (Xstat(src, stp) < 0) return(-1);
	}
#endif	/* !NOSYMLINK */

	if ((fd1 = Xopen(src, O_TEXT | O_RDONLY, stp -> st_mode)) < 0)
		return(-1);
	if (dfd >= 0) fd2 = dfd;
	else if ((fd2 = writeopen(dest, src)) <= 0) {
		VOID_C Xclose(fd1);
		return(fd2);
	}

	tty1 = isatty(fd1);
	tty2 = isatty(fd2);
#if	MSDOS && !defined (LSI_C)
# ifdef	DJGPP
	if (tty1) setmode(fd2, O_TEXT);
	else
# endif
	{
		setmode(fd1, O_BINARY);
		setmode(fd2, O_BINARY);
	}
#endif	/* MSDOS && !LSI_C */
	if (tty1 || tty2) sbin = CF_TEXT;

	timeout1 = timeout2 = 0;
#ifdef	DEP_URLPATH
	switch (chkopenfd(fd1)) {
		case DEV_URL:
		case DEV_HTTP:
			timeout1 = urltimeout;
			VOID_C urlfstat(fd1, stp);
			break;
		default:
			break;
	}
	switch (chkopenfd(fd2)) {
		case DEV_URL:
		case DEV_HTTP:
			timeout2 = urltimeout;
		default:
			break;
	}
#endif

	total = (off_t)0;
	if (tty1) stp -> st_size = (off_t)-1;
	for (;;) {
		for (retry = n = 0; retry < COPYRETRY; retry++) {
			ofs = Xlseek(fd1, (off_t)0, L_INCR);
			n = textread(fd1, buf, sizeof(buf),
				sbin, timeout1, &total, stp -> st_size);
			if (n >= 0) break;
			duperrno = errno;
			if (Xlseek(fd1, ofs, L_SET) < (off_t)0) {
				errno = duperrno;
				break;
			}
		}
		if (n <= 0) break;
		nread = n;

		for (retry = n = 0; retry < COPYRETRY; retry++) {
			ofs = Xlseek(fd2, (off_t)0, L_INCR);
			n = surewrite(fd2, buf, nread);
			if (n < 0) /*EMPTY*/;
			else if (tty2 || !(copyflag & CF_VERIFY)) break;
			else if (Xlseek(fd2, ofs, L_SET) < (off_t)0) break;
			else {
				cp = buf2;
				size = nread;
				while (size > 0) {
					n = checkread(fd2, cp, size, timeout2);
					if (n <= 0) break;
					cp += n;
					size -= n;
				}
				if (size || memcmp(buf, buf2, nread))
					n = seterrno(EINVAL);
				else {
					n = 0;
					break;
				}
			}

			duperrno = errno;
			if (Xlseek(fd2, ofs, L_SET) < (off_t)0) {
				errno = duperrno;
				break;
			}
		}
		if (n < 0) break;
	}

	if (dfd < 0) textclose(fd2, dbin);
	safeclose(fd1);

	if (n < 0) {
		duperrno = errno;
		VOID_C Xunlink(dest);
		errno = duperrno;
		return(dfd < 0 ? -1 : -2);
	}

#ifdef	FD
	stp -> st_nlink = (TCH_ATIME | TCH_MTIME | TCH_IGNOREERR);
	if (!tty2 && touchfile(dest, stp) < 0) return(-1);
#else
	if (!tty2) VOID_C touchfile(dest, stp);
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
	src = Xmalloc(size);
	Xstrcpy(src, argv[n]);
	for (n++; n < argc; n++) {
		if ((cp = Xstrchr(&(argv[n - 1][1]), '+')) && !*(++cp))
			/*EMPTY*/;
		else if (argv[n][0] != '+' && argv[n][0] != DOSCOMOPT) break;
		j = strlen(argv[n]);
		src = Xrealloc(src, size + j);
		Xstrcpy(&(src[size - 1]), argv[n]);
		size += j;
	}

	for (cp = src, nf = 0; cp; nf++) {
		if ((cp = Xstrchr(cp, '+'))) {
			*(cp++) = '\0';
			if (!*cp) {
				if (nf++) break;
				doserror(src, ER_COPIEDITSELF);
				Xfree(src);
				return(RET_FAIL);
			}
		}
	}
	arg = (char **)Xmalloc(nf * sizeof(char *));
	sbin = (int *)Xmalloc(nf * sizeof(int));

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
		Xfree(arg[0]);
		Xfree(arg);
		Xfree(sbin);
		return(RET_FAIL);
	}
#endif
	else {
		Xstrcpy(dest, argv[n]);
		dbin = getbinmode(dest, sbin[i - 1]);
	}

	if (dbin < 0) {
		Xfree(arg[0]);
		Xfree(arg);
		Xfree(sbin);
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
			arg[0] = Xrealloc(arg[0], size + 2);
			Xstrcpy(strcatdelim(arg[0]), "*");
		}
		else if (!*dest) {
			doserror(arg[0], ER_COPIEDITSELF);
			Xfree(arg[0]);
			Xfree(arg);
			Xfree(sbin);
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
			else if (!n && file) Xstrcpy(file, src);
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
	Xfree(arg[0]);
	Xfree(arg);
	Xfree(sbin);
	if (fd >= 0) {
		if (fd > 0) textclose(fd, dbin);
		if (nc > 0) nc = 1;
	}
	if (!(copyflag & CF_CANCEL))
		VOID_C Xprintf("%9d file(s) copied\n", nc);

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
	Xfputs(t_clear, Xstdout);
	Xfflush(Xstdout);
#endif
	VOID_C fputnl(Xstderr);

	return(RET_SUCCESS);
}

int doscomtype(argc, argv)
int argc;
char *CONST argv[];
{
#if	MSDOS && !defined (LSI_C)
	int omode;
#endif
	struct stat st;
	char *cp;
	u_char uc;
	off_t total;
	ALLOC_T size;
	int i, n, fd, tty, timeout;

	if (checkarg(argc, argv) < 0) return(RET_FAIL);

	if (Xlstat(argv[1], &st) < 0
	|| (fd = Xopen(argv[1], O_TEXT | O_RDONLY, 0666)) < 0) {
		dosperror(argv[1]);
		return(RET_FAIL);
	}

	tty = isatty(fd);
#if	MSDOS && !defined (LSI_C)
# ifdef	DJGPP
	if (tty) omode = setmode(STDOUT_FILENO, O_TEXT);
	else
# endif
	{
		setmode(fd, O_BINARY);
		omode = setmode(STDOUT_FILENO, O_BINARY);
	}
#endif	/* MSDOS && !LSI_C */

	timeout = 0;
#ifdef	DEP_URLPATH
	if (chkopenfd(fd) == DEV_URL) {
		timeout = urltimeout;
		VOID_C urlfstat(fd, &st);
	}
#endif

	cp = c_realloc(NULL, 0, &size);
	i = 0;
	total = (off_t)0;
	if (tty) st.st_size = (off_t)-1;
	for (;;) {
		n = textread(fd, (char *)&uc, sizeof(uc),
			CF_TEXT, timeout, &total, st.st_size);
		if (n < 0) break;
		if (!n) {
			n = surewrite(STDOUT_FILENO, cp, i);
			break;
		}
		else if (uc == '\n') {
			cp[i++] = '\n';
			n = surewrite(STDOUT_FILENO, cp, i);
			if (n < 0) break;
			i = 0;
		}
		else {
			cp = c_realloc(cp, i, &size);
			cp[i++] = uc;
		}
	}
	Xfree(cp);
	safeclose(fd);
#if	MSDOS && !defined (LSI_C)
	if (omode >= 0) setmode(STDOUT_FILENO, omode);
#endif

	if (n < 0) {
		dosperror(argv[1]);
		return(RET_FAIL);
	}

	return(RET_SUCCESS);
}
#endif	/* DOSCOMMAND && DEP_ORIGSHELL */
