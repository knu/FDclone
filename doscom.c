/*
 *	doscom.c
 *
 *	Builtin Command for DOS
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "machine.h"
#include "kctype.h"

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#if	MSDOS
#include <io.h>
#include "unixemu.h"
#define	DOSCOMOPT	'/'
#define	C_EOF		('Z' & 037)
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
#define	C_EOF		('D' & 037)
#endif	/* !MSDOS */

#ifdef	NOVOID
#define	VOID
#define	VOID_T	int
#define	VOID_P	char *
#else
#define	VOID	void
#define	VOID_T	void
#define	VOID_P	void *
#endif

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

#ifndef	L_SET
# ifdef	SEEK_SET
# define	L_SET	SEEK_SET
# else
# define	L_SET	0
# endif
#endif
#ifndef	L_INCR
# ifdef	SEEK_CUR
# define	L_INCR	SEEK_CUR
# else
# define	L_INCR	1
# endif
#endif

#include "term.h"
#include "pathname.h"
#include "system.h"

#if	!defined (_NOORIGSHELL) && defined (DOSCOMMAND) \
&& (!defined (FD) || (FD >= 2))

#ifdef	NOERRNO
extern int errno;
#endif
#ifdef	DEBUG
extern char *_mtrace_file;
#endif
#ifdef	LSI_C
extern u_char _openfile[];
#endif
#ifndef	DECLERRLIST
extern char *sys_errlist[];
#endif

#ifdef	FD
extern char *Xgetwd __P_((char *));
extern int Xstat __P_((char *, struct stat *));
extern int Xlstat __P_((char *, struct stat *));
extern int Xaccess __P_((char *, int));
extern int Xsymlink __P_((char *, char *));
extern int Xreadlink __P_((char *, char *, int));
extern int Xchmod __P_((char *, int));
# ifdef	USEUTIME
extern int Xutime __P_((char *, struct utimbuf *));
# else
extern int Xutimes __P_((char *, struct timeval []));
# endif
extern int Xunlink __P_((char *));
extern int Xrename __P_((char *, char *));
# if	MSDOS && defined (_NOROCKRIDGE) && defined (_NOUSELFN)
#define	Xopen		open
# else
extern int Xopen __P_((char *, int, int));
# endif
# ifdef	_NODOSDRIVE
#define	Xclose		close
#define	Xread		read
#define	Xwrite		write
# else	/* !_NODOSDRIVE */
extern int Xclose __P_((int));
extern int Xread __P_((int, char *, int));
extern int Xwrite __P_((int, char *, int));
# endif	/* !_NODOSDRIVE */
# if	MSDOS && defined (_NOUSELFN)
#  ifdef	DJGPP
#  define	_Xmkdir(p, m)	(mkdir(p, m) ? -1 : 0)
#  else
extern int _Xmkdir __P_((char *, int));
#  endif
# define	_Xrmdir(p)	(rmdir(p) ? -1 : 0)
# else	/* !MSDOS || !_NOUSELFN */
#  if	!MSDOS && defined (_NODOSDRIVE) && defined (_NOKANJICONV)
#  define	_Xmkdir		mkdir
#  define	_Xrmdir		rmdir
#  else
extern int _Xmkdir __P_((char *, int));
extern int _Xrmdir __P_((char *));
#  endif
# endif	/* !MSDOS || !_NOUSELFN */
extern int chdir3 __P_((char *));
extern char *realpath2 __P_((char *, char *, int));
extern long getblocksize __P_((char *));
# ifdef	_NOROCKRIDGE
extern DIR *_Xopendir __P_((char *));
# define	Xopendir	_Xopendir
# else
extern DIR *Xopendir __P_((char *));
# endif
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern char *ascnumeric __P_((char *, long, int, int));
extern int touchfile __P_((char *, time_t, time_t));
extern int safewrite __P_((int, char *, int));
extern VOID getinfofs __P_((char *, long *, long *));
#else	/* !FD */
# ifdef DJGPP
extern char *Xgetwd __P_((char *));
# else
#  ifdef	USEGETWD
#  define	Xgetwd		(char *)getwd
#  else
#  define	Xgetwd(p)	(char *)getcwd(p, MAXPATHLEN)
#  endif
# endif
#define	Xaccess(p, m)	(access(p, m) ? -1 : 0)
#define	Xsymlink(o, n)	(symlink(o, n) ? -1 : 0)
#define	Xreadlink	readlink
#define	Xchmod		chmod
#define	Xunlink		unlink
#define	Xutime(f, t)	(utime(f, t) ? -1 : 0)
#define	Xutimes(f, t)	(utimes(f, t) ? -1 : 0)
#define	Xrename(o, n)	(rename(o, n) ? -1 : 0)
#define	Xopen		open
#define	Xclose		close
#define	Xread		read
#define	Xwrite		write
#define	_Xrmdir(p)	(rmdir(p) ? -1 : 0)
# if	MSDOS
#  ifdef	DJGPP
#  define	_Xmkdir(p, m)	(mkdir(p, m) ? -1 : 0)
#  else
extern int _Xmkdir __P_((char *, int));
#  endif
# define	Xstat(f, s)	(stat(f, s) ? -1 : 0)
# define	Xlstat(f, s)	(stat(f, s) ? -1 : 0)
extern VOID getinfofs __P_((char *, long *, long *));
extern int chdir3 __P_((char *));
extern char *realpath2 __P_((char *, char *, int));
extern long getblocksize __P_((char *));
extern DIR *Xopendir __P_((char *));
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
# else	/* !MSDOS */
# define	_Xmkdir		mkdir
# define	Xstat		stat
# define	Xlstat		lstat
# define	getinfofs(p, t, f) \
				(*(t) = *(f) = -1L)
# define	chdir3(p)	(chdir(p) ? -1 : 0)
# define	realpath2(p, r, f) \
				realpath(p, r)
# define	getblocksize(s)	DEV_BSIZE
# define	Xopendir	opendir
# define	Xclosedir	closedir
# define	Xreaddir	readdir
# endif	/* !MSDOS */
static char *NEAR ascnumeric __P_((char *, long, int, int));
static int NEAR touchfile __P_((char *, time_t, time_t));
static int NEAR safewrite __P_((int, char *, int));
#endif	/* !FD */

struct filestat_t {
	char *nam;
	u_short mod;
	off_t siz;
	time_t mtim;
	time_t atim;
};
static char dirsort[sizeof("NSEDGA")] = "";
static char dirattr[sizeof("DRHSA")] = "";
static int dirtype = '\0';
static int dirflag = 0;
#define	DF_LOWER	001
#define	DF_PAUSE	002
#define	DF_SUBDIR	004
static int copyflag = 0;
#define	CF_BINARY	001
#define	CF_TEXT		002
#define	CF_VERIFY	004
#define	CF_NOCONFIRM	010
#define	CF_VERBOSE	020

#ifndef	O_BINARY
#define	O_BINARY	0
#endif
#ifndef	ENOSPC
#define	ENOSPC		EACCES
#endif

extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strdup2 __P_((char *));
extern int isdotdir __P_((char *));
extern int kanjifputs __P_((char *, FILE *));
extern VOID freevar __P_((char **));

static VOID NEAR doserror __P_((char *, int));
static VOID NEAR dosperror __P_((char *));
static int NEAR inputkey __P_((VOID_A));
static int cmpdirent __P_((CONST VOID_P, CONST VOID_P));
static VOID NEAR evalenvopt __P_((char *, char *,
		int (NEAR *)__P_((int, char *[]))));
static int NEAR getdiropt __P_((int, char *[]));
static VOID NEAR showfname __P_((struct filestat_t *, long, int));
static VOID NEAR showfnamew __P_((struct filestat_t *));
static VOID NEAR showfnameb __P_((struct filestat_t *));
static VOID NEAR checkline __P_((int *, char *));
static char *NEAR convwild __P_((char *, char *, char *, char *));
static int NEAR getcopyopt __P_((int, char *[]));
static int NEAR getbinmode __P_((char *, char *[], int));
static int NEAR writeopen __P_((char *, char *));
static int NEAR textread __P_((int, u_char *, int, int));
static int NEAR textclose __P_((int, int));
static int NEAR doscopy __P_((char *, char *, struct stat *, int, int, int));

#define	ER_REQPARAM	1
#define	ER_TOOMANYPARAM	2
#define	ER_FILENOTFOUND	3
#define	ER_INVALIDPARAM	4
#define	ER_DUPLFILENAME	5
#define	ER_COPIEDITSELF	6
static char *doserrstr[] = {
	"",
	"Required parameter missing",
	"Too many parameters",
	"File not found",
	"Invalid parameter",
	"Duplicate file name or file in use",
	"File cannot be copied onto itself",
};
#define	DOSERRSIZ	((int)(sizeof(doserrstr) / sizeof(char *)))


#ifndef	FD
static char *NEAR ascnumeric(buf, n, digit, max)
char *buf;
long n;
int digit, max;
{
	char tmp[20 * 2 + 1];
	int i, j, d;

	i = j = 0;
	d = digit;
	if (digit < 0) digit = -digit;
	if (n < 0) tmp[i++] = '?';
	else if (!n) tmp[i++] = '0';
	else {
		for (;;) {
			tmp[i++] = '0' + n % 10;
			if (!(n /= 10) || i >= max) break;
			if (digit > 1 && ++j >= digit) {
				if (i >= max - 1) break;
				tmp[i++] = ',';
				j = 0;
			}
		}
		if (n) for (j = 0; j < i; j++) if (tmp[j] != ',') tmp[j] = '9';
	}

	if (d <= 0) j = 0;
	else if (d > max) for (j = 0; j < max - i; j++) buf[j] = '0';
	else for (j = 0; j < max - i; j++) buf[j] = ' ';
	while (i--) buf[j++] = tmp[i];
	if (d < 0) for (; j < max; j++) buf[j] = ' ';
	buf[j] = '\0';

	return(buf);
}

static int NEAR touchfile(path, atime, mtime)
char *path;
time_t atime, mtime;
{
# ifdef	USEUTIME
	struct utimbuf times;

	times.actime = atime;
	times.modtime = mtime;
	return(Xutime(path, &times));
# else
	struct timeval tvp[2];

	tvp[0].tv_sec = atime;
	tvp[0].tv_usec = 0;
	tvp[1].tv_sec = mtime;
	tvp[1].tv_usec = 0;
	return(Xutimes(path, tvp));
# endif
}

static int NEAR safewrite(fd, buf, size)
int fd;
char *buf;
int size;
{
	int n;

	n = Xwrite(fd, buf, size);
# if	MSDOS
	if (n < size) n = 0;
# else
	if (n < 0) {
		if (errno == EINTR) n = size;
		else if (errno == ENOSPC) n = 0;
	}
# endif
	return(n);
}
#endif	/* !FD */

static VOID NEAR doserror(s, n)
char *s;
int n;
{
	if (!n || n >= DOSERRSIZ) return;
	if (s) {
		kanjifputs(s, stderr);
		fputc('\n', stderr);
	}
	fputs(doserrstr[n], stderr);
	fputc('\n', stderr);
	fflush(stderr);
}

static VOID NEAR dosperror(s)
char *s;
{
	int duperrno;

	duperrno = errno;
	if (errno < 0) return;
	if (s) {
		kanjifputs(s, stderr);
		fputc('\n', stderr);
	}
#if	MSDOS
	fputs(strerror(duperrno), stderr);
#else
	fputs((char *)sys_errlist[duperrno], stderr);
#endif
	fputc('\n', stderr);
	fflush(stderr);
	errno = 0;
}

static int NEAR inputkey(VOID_A)
{
	int c;

	ttyiomode();
	c = getkey2(0);
	stdiomode();
	return(c);
}

#define	dir_isdir(sp)	(((((sp) -> mod) & S_IFMT) == S_IFDIR) ? 1 : 0)

static int cmpdirent(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	struct filestat_t *sp1, *sp2;
	char *cp1, *cp2;
	int i, ret;

	sp1 = (struct filestat_t *)vp1;
	sp2 = (struct filestat_t *)vp2;
	if (!strpbrk(dirsort, "Ee")) cp1 = cp2 = NULL;
	else {
		if ((cp1 = strrchr(sp1 -> nam, '.'))) *cp1 = '\0';
		if ((cp2 = strrchr(sp2 -> nam, '.'))) *cp2 = '\0';
	}

	ret = 0;
	for (i = 0; dirsort[i]; i++) {
		switch (dirsort[i]) {
			case 'N':
				ret = strpathcmp2(sp1 -> nam, sp2 -> nam);
				break;
			case 'n':
				ret = strpathcmp2(sp2 -> nam, sp1 -> nam);
				break;
			case 'S':
				ret = sp1 -> siz - sp2 -> siz;
				break;
			case 's':
				ret = sp2 -> siz - sp1 -> siz;
				break;
			case 'E':
				if (cp1) ret = (cp2) ?
					strpathcmp2(cp1 + 1, cp2 + 1) : 1;
				else ret = (cp2) ? -1 : 0;
				break;
			case 'e':
				if (cp2) ret = (cp1) ?
					strpathcmp2(cp2 + 1, cp1 + 1) : 1;
				else ret = (cp1) ? -1 : 0;
				break;
			case 'D':
				ret = sp1 -> mtim - sp2 -> mtim;
				break;
			case 'd':
				ret = sp2 -> mtim - sp1 -> mtim;
				break;
			case 'G':
				ret = dir_isdir(sp2) - dir_isdir(sp1);
				break;
			case 'g':
				ret = dir_isdir(sp1) - dir_isdir(sp2);
				break;
			case 'A':
				ret = sp1 -> atim - sp2 -> atim;
				break;
			case 'a':
				ret = sp2 -> atim - sp1 -> atim;
				break;
			default:
				break;
		}
		if (ret) break;
	}
	if (cp1) *cp1 = '.';
	if (cp2) *cp2 = '.';
	return(ret);
}

static VOID NEAR evalenvopt(cmd, env, getoptcmd)
char *cmd, *env;
int (NEAR *getoptcmd)__P_((int, char *[]));
{
	char *cp, **argv;
	int n, er, argc;

	if (!(cp = getshellvar(env, -1)) || !*cp) return;
	argv = (char **)malloc2(3 * sizeof(char *));
	argc = 2;
	argv[0] = strdup2(cmd);
	argv[1] = strdup2(cp);
	argv[2] = NULL;
	argc = evalifs(argc, &argv, IFS_SET, 1);

	er = 0;
	if ((n = (*getoptcmd)(argc, argv)) < 0) er++;
	else if (argc > n) {
		er++;
		doserror(argv[n], ER_TOOMANYPARAM);
	}
	if (er) fputs("(Error occurred in environment variable)\n\n", stdout);
	for (n = 0; n < argc; n++) free(argv[n]);
	free(argv);
}

static int NEAR getdiropt(argc, argv)
int argc;
char *argv[];
{
	char *cp;
	int i, j, k, c1, c2, n, r, rr;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != DOSCOMOPT) break;
		for (cp = argv[i]; *cp; cp++) *cp = toupper2(*cp);
		cp = argv[i];
		rr = (cp[1] == '-') ? 1 : 0;
		switch (cp[1 + rr]) {
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
				cp += 2;
				for (j = 0; cp[j]; j++) switch (cp[j]) {
					case 'D':
					case 'R':
					case 'H':
					case 'S':
					case 'A':
						c1 = toupper2(cp[j]);
						c2 = tolower2(cp[j]);
						for (k = 0; k < n; k++)
							if (dirattr[k] == c1
							|| dirattr[k] == c2)
								break;
						if (k < n) break;
						dirattr[n++] = (r) ? c2 : c1;
						r = 0;
						break;
					case '-':
						if (!r) {
							r = 1;
							break;
						}
					default:
						doserror(cp, ER_REQPARAM);
						return(-1);
/*NOTREACHED*/
						break;
				}
				if (n) dirattr[n] = '\0';
				else strcpy(dirattr, "DRHSA");
				if (r) {
					doserror(cp, ER_REQPARAM);
					return(-1);
				}
				cp += j - 2;
				break;
			case 'O':
				if (rr) {
					dirsort[0] = '\0';
					break;
				}
				n = r = 0;
				cp += 2;
				for (j = 0; cp[j]; j++) switch (cp[j]) {
					case 'N':
					case 'S':
					case 'E':
					case 'D':
					case 'G':
					case 'A':
						c1 = toupper2(cp[j]);
						c2 = tolower2(cp[j]);
						for (k = 0; k < n; k++)
							if (dirsort[k] == c1
							|| dirsort[k] == c2)
								break;
						if (k < n) break;
						dirsort[n++] = (r) ? c2 : c1;
						r = 0;
						break;
					case '-':
						if (!r) {
							r = 1;
							break;
						}
					default:
						doserror(cp, ER_REQPARAM);
						return(-1);
/*NOTREACHED*/
						break;
				}
				if (!n) dirsort[n++] = 'N';
				dirsort[n] = '\0';
				if (r) {
					doserror(cp, ER_REQPARAM);
					return(-1);
				}
				cp += j - 2;
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
			case 'V':
				if (rr) {
					if (dirtype == 'V') dirtype = '\0';
					break;
				}
				if (!dirtype) dirtype = 'V';
				break;
			default:
				doserror(argv[i], ER_REQPARAM);
				return(-1);
/*NOTREACHED*/
				break;
		}
		if (*(cp += 2 + rr)) {
			doserror(cp, ER_REQPARAM);
			return(-1);
		}
	}
	return(i);
}

static VOID NEAR showfname(dirp, bsize, verbose)
struct filestat_t *dirp;
long bsize;
int verbose;
{
	struct tm *tm;
	char *ext, buf[MAXNAMLEN + 1];
	int i;

	strcpy(buf, dirp -> nam);
	if (isdotdir(buf)) ext = NULL;
	else if ((ext = strrchr(buf, '.'))) {
		*(ext++) = '\0';
		i = ext - buf;
		ext = &(dirp -> nam[i]);
	}
	for (i = 0; i < 8; i++) {
		if (!buf[i]) break;
		if (iskanji1(buf, i)) i++;
	}
	if (i > 8) i -= 2;
	for (; i < 9; i++) buf[i] = ' ';
	buf[i] = '\0';
	if (dirflag & DF_LOWER)
		for (i = 0; i < 9; i++) buf[i] = tolower2(buf[i]);
	kanjifputs(buf, stdout);

	if (!ext) i = 0;
	else {
		strcpy(buf, ext);
		for (i = 0; i < 4; i++) {
			if (!buf[i]) break;
			if (iskanji1(buf, i)) i++;
		}
		if (i > 4) i -= 2;
	}
	for (; i < 5; i++) buf[i] = ' ';
	buf[i] = '\0';
	if (dirflag & DF_LOWER)
		for (i = 0; i < 5; i++) buf[i] = tolower2(buf[i]);
	kanjifputs(buf, stdout);

	if (dir_isdir(dirp)) fputs(" <DIR>      ", stdout);
	else if ((dirp -> mod & S_IFMT) == S_IFREG)
		fputs(ascnumeric(buf, dirp -> siz, 3, 12), stdout);
	else fputs("            ", stdout);

	if (verbose) {
		if ((dirp -> mod & S_IFMT) != S_IFREG)
			fputs("              ", stdout);
		else {
			dirp -> siz = ((dirp -> siz + bsize - 1) / bsize)
				* bsize;
			fputs(ascnumeric(buf, dirp -> siz, 3, 12), stdout);
		}
	}

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
	fprintf(stdout, "  %02d-%02d-%02d  %2d:%02d ",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec);

	if (verbose) {
#ifdef	DEBUG
		_mtrace_file = "localtime(start)";
		tm = localtime(&(dirp -> atim));
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "localtime(end)";
			malloc(0);	/* dummy malloc */
		}
#else
		tm = localtime(&(dirp -> atim));
#endif
		fprintf(stdout, " %02d-%02d-%02d  ",
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

	kanjifputs(dirp -> nam, stdout);
	fputc('\n', stdout);
}

static VOID NEAR showfnamew(dirp)
struct filestat_t *dirp;
{
	char *ext, buf[MAXNAMLEN + 1];
	int i, len;

	strcpy(buf, dirp -> nam);
	if (isdotdir(buf)) ext = NULL;
	else if ((ext = strrchr(buf, '.'))) {
		*(ext++) = '\0';
		i = ext - buf;
		ext = &(dirp -> nam[i]);
	}
	for (i = 0; i < 8; i++) {
		if (!buf[i]) break;
		if (iskanji1(buf, i)) i++;
	}
	if (i > 8) i -= 2;
	buf[i] = '\0';
	len = i;

	if (!ext) {
		buf[len] = '\0';
		i = 0;
	}
	else {
		buf[len++] = '.';
		strcpy(&(buf[len]), ext);
		for (i = 0; i < 4; i++) {
			if (!buf[len + i]) break;
			if (iskanji1(buf, len + i)) i++;
		}
		if (i > 4) i -= 2;
	}
	buf[len + i] = '\0';
	len += i;
	if (dirflag & DF_LOWER)
		for (i = 0; buf[i]; i++) buf[i] = tolower2(buf[i]);

	if (dir_isdir(dirp)) {
		fputc('[', stdout);
		kanjifputs(buf, stdout);
		fputc(']', stdout);
	}
	else if ((dirp -> mod & S_IFMT) == S_IFREG)
		kanjifputs(buf, stdout);
	else len = 0;
	for (; len < 8 + 1 + 3; len++) fputc(' ', stdout);
}

static VOID NEAR showfnameb(dirp)
struct filestat_t *dirp;
{
	int i;

	if (isdotdir(dirp -> nam)) return;
	if (dirflag & DF_LOWER)
		for (i = 0; dirp -> nam[i]; i++)
			dirp -> nam[i] = tolower2(dirp -> nam[i]);
	kanjifputs(dirp -> nam, stdout);
	fputc('\n', stdout);
}

static VOID NEAR checkline(linep, dir)
int *linep;
char *dir;
{
	if (!(dirflag & DF_PAUSE)) return;
	if (++(*linep) >= n_line) {
		fputs("Press any key to continue . . .\n", stdout);
		fflush(stdout);
		inputkey();
		fputs("\n(continuing ", stdout);
		fputs(dir, stdout);
		fputs(")\n", stdout);
		*linep = 3;
	}
}

int doscomdir(argc, argv)
int argc;
char *argv[];
{
	struct filestat_t *dirlist;
	reg_t *re;
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	char *dir, *file;
	char wd[MAXPATHLEN], cwd[MAXPATHLEN], buf[MAXPATHLEN];
	long size, bsize, fre;
	int i, n, nf, nd, max, line;

	strcpy(dirattr, "hs");
	dirsort[0] = dirtype = '\0';
	dirflag = 0;
	evalenvopt(argv[0], "DIRCMD", getdiropt);
	if ((n = getdiropt(argc, argv)) < 0)
		return(RET_FAIL);
	if (argc <= n) {
		dir = ".";
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
			dir = ".";
			file = argv[n];
		}
		else {
			i = file - buf;
			*(++file) = '\0';
			dir = buf;
			file = &(argv[n][++i]);
			if (!(*file)) file = "*";
		}
	}

	if (Xgetwd(cwd) < 0) {
		dosperror(NULL);
		return(RET_FAIL);
	}
	if (dir != buf) strcpy(wd, cwd);
	else {
		if (chdir3(buf) < 0) {
			dosperror(buf);
			return(RET_FAIL);
		}
		if (Xgetwd(wd) < 0) {
			dosperror(NULL);
			return(RET_FAIL);
		}
		if (chdir3(cwd) < 0) {
			dosperror(cwd);
			return(RET_FAIL);
		}
	}
	getinfofs(wd, &size, &fre);

	if (!(dirp = Xopendir(dir))) {
		dosperror(dir);
		return(RET_FAIL);
	}
	fputc('\n', stdout);
	fputs(" Directory of ", stdout);
	kanjifputs(wd, stdout);
	fputc('\n', stdout);
	if (dirtype == 'V') {
		fputs("File Name         ", stdout);
		fputs("Size        ", stdout);
		fputs("Allocated      ", stdout);
		fputs("Modified      ", stdout);
		fputs("Accessed  ", stdout);
		fputs("Attrib\n\n", stdout);
	}
	fputc('\n', stdout);

	n = nf = nd = 0;
	size = 0L;
	bsize = getblocksize(dir);
	line = 4;
	i = strlen(wd);
	dir = strcatdelim(wd);
	dirlist = NULL;
	if ((re = regexp_init(file, -1))) while ((dp = Xreaddir(dirp))) {
		strcpy(dir, dp -> d_name);
		if (Xstat(wd, &st) < 0) continue;
		if (!regexp_exec(re, dp -> d_name, 0)) continue;

		dirlist = (struct filestat_t *)realloc2(dirlist,
			sizeof(struct filestat_t) * (n + 1));
		dirlist[n].nam = strdup2(dp -> d_name);
		dirlist[n].mod = st.st_mode;
		dirlist[n].siz = st.st_size;
		dirlist[n].mtim = st.st_mtime;
		dirlist[n].atim = st.st_atime;
		if ((st.st_mode & S_IFMT) == S_IFDIR) nd++;
		else if ((st.st_mode & S_IFMT) == S_IFREG) {
			nf++;
			size += dirlist[n].siz;
		}
		n++;
	}
	Xclosedir(dirp);
	wd[i] = '\0';
	if (re) regexp_free(re);
	max = n;
	if (*dirsort)
		qsort(dirlist, max, sizeof(struct filestat_t), cmpdirent);

	for (n = 0; n < max; n++) {
		for (i = 0; dirattr[i]; i++) {
			int f;

			f = 0;
			switch (dirattr[i]) {
				case 'D':
					if (!dir_isdir(&(dirlist[n]))) f = 1;
					break;
				case 'd':
					if (dir_isdir(&(dirlist[n]))) f = 1;
					break;
				case 'R':
					if (dirlist[n].mod & S_IWUSR) f = 1;
					break;
				case 'r':
					if (!(dirlist[n].mod & S_IWUSR)) f = 1;
					break;
				case 'H':
					if (dirlist[n].mod & S_IRUSR) f = 1;
					break;
				case 'h':
					if (!(dirlist[n].mod & S_IRUSR)) f = 1;
					break;
				case 'S':
					if ((dirlist[n].mod & S_IFMT)
					!= S_IFSOCK) f = 1;
					break;
				case 's':
					if ((dirlist[n].mod & S_IFMT)
					== S_IFSOCK) f = 1;
					break;
				case 'A':
					if (!(dirlist[n].mod & S_ISVTX)) f = 1;
					break;
				case 'a':
					if (dirlist[n].mod & S_ISVTX) f = 1;
					break;
				default:
					break;
			}
			if (!f) break;
		}
		if (!dirattr[i]) continue;

		switch (dirtype) {
			case 'W':
				showfnamew(&(dirlist[n]));
				if (n != max - 1 && (n % 5) != 5 - 1)
					fputc('\t', stdout);
				else {
					fputc('\n', stdout);
					checkline(&line, wd);
				}
				break;
			case 'B':
				showfnameb(&(dirlist[n]));
				checkline(&line, wd);
				break;
			default:
				showfname(&(dirlist[n]), bsize,
					dirtype == 'V');
				checkline(&line, wd);
				break;
		}
		free(dirlist[n].nam);
	}
	if (dirlist) free(dirlist);

	if (!nf && !nd) {
		fputs("File not found\n", stdout);
		fputs("                  ", stdout);
	}
	else {
		fputs(ascnumeric(buf, nf, 10, 10), stdout);
		fputs(" file(s)", stdout);
		fputs(ascnumeric(buf, size, 3, 15), stdout);
		fputs(" bytes\n", stdout);
		fputs(ascnumeric(buf, nd, 10, 10), stdout);
		fputs(" dir(s) ", stdout);
	}

	if (fre < 1024L * 1024L) {
		fputs(ascnumeric(buf, fre * 1024L, 3, 15), stdout);
		fputs(" bytes", stdout);
	}
	else {
		fputs(ascnumeric(buf, fre / 1024L, 3, 12), stdout);
		fre = ((fre * 100L) / 1024L) % 100;
		fputc('.', stdout);
		fputs(ascnumeric(buf, fre, 3, 2), stdout);
		fputs(" MB", stdout);
	}
	fputs(" free\n\n", stdout);
	fflush(stdout);
	return(RET_SUCCESS);
}

int doscommkdir(argc, argv)
int argc;
char *argv[];
{
	if (argc <= 1) {
		doserror(NULL, ER_REQPARAM);
		return(RET_FAIL);
	}
	else if (argc > 2) {
		doserror(argv[2], ER_TOOMANYPARAM);
		return(RET_FAIL);
	}

	if (_Xmkdir(argv[1], 0777) < 0) {
		dosperror(argv[1]);
		return(RET_FAIL);
	}
	return(RET_SUCCESS);
}

int doscomrmdir(argc, argv)
int argc;
char *argv[];
{
	if (argc <= 1) {
		doserror(NULL, ER_REQPARAM);
		return(RET_FAIL);
	}
	else if (argc > 2) {
		doserror(argv[2], ER_TOOMANYPARAM);
		return(RET_FAIL);
	}

	if (_Xrmdir(argv[1]) < 0) {
		dosperror(argv[1]);
		return(RET_FAIL);
	}
	return(RET_SUCCESS);
}

int doscomerase(argc, argv)
int argc;
char *argv[];
{
	char *buf, **wild;
	int i, n, key, flag, ret;

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
			doserror(argv[n], ER_REQPARAM);
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

	if (!strcmp(argv[n], "*") || !strcmp(argv[n], "*.*")) {
		do {
			fputs("All files in directory will be deleted!\n",
				stdout);
			fputs("Are you sure (Y/N)?", stdout);
			fflush(stdout);
			buf = readline(STDIN_FILENO);
			if (!isatty(STDIN_FILENO)) {
				fputc('\n', stdout);
				fflush(stdout);
			}
			key = *buf;
			free(buf);
		} while (!strchr("ynYN", key));
		if (key == 'n' || key == 'N') return(RET_FAIL);
	}
	if (!(wild = evalwild(argv[n]))) {
		doserror(argv[n], ER_FILENOTFOUND);
		return(RET_FAIL);
	}
	ret = RET_SUCCESS;
	for (i = 0; wild[i]; i++) {
		if (flag) {
			do {
				kanjifputs(wild[i], stdout);
				fputs(",    Delete (Y/N)?", stdout);
				fflush(stdout);
				key = inputkey();
				fputc(key, stdout);
				fputc('\n', stdout);
				fflush(stdout);
			} while (!strchr("ynYN", key));
			if (key == 'n' || key == 'N') continue;
		}
		if (Xunlink(wild[i]) != 0) {
			dosperror(wild[i]);
			ret = RET_FAIL;
			ERRBREAK;
		}
	}
	freevar(wild);
	return(ret);
}

static char *NEAR convwild(dest, src, wild, swild)
char *dest, *src, *wild, *swild;
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
char *argv[];
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

	if (!(wild = evalwild(argv[1]))) {
		doserror(argv[1], ER_FILENOTFOUND);
		return(RET_FAIL);
	}
	ret = RET_SUCCESS;
	for (i = 0; wild[i]; i++) {
		strcpy(new, wild[i]);
		if ((cp = strrdelim(new, 1))) cp++;
		else cp = new;
		j = cp - new;
		convwild(cp, &(wild[i][j]), argv[2], argv[1]);
		if (Xaccess(new, F_OK) >= 0 || errno != ENOENT) {
			doserror(new, ER_DUPLFILENAME);
			ret = RET_FAIL;
			ERRBREAK;
		}
		if (Xrename(wild[i], new) < 0) {
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
char *argv[];
{
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != DOSCOMOPT) break;
		if (!argv[i][1] || (argv[i][2] && argv[i][3])) {
			doserror(argv[i], ER_REQPARAM);
			return(-1);
		}
		if (argv[i][1] == '-') switch (argv[i][2]) {
			case 'Y':
			case 'y':
				copyflag &= ~CF_NOCONFIRM;
				break;
			default:
				doserror(argv[i], ER_REQPARAM);
				return(-1);
/*NOTREACHED*/
				break;
		}
		else switch (argv[i][1]) {
			case 'A':
			case 'a':
				copyflag &= ~CF_BINARY;
				copyflag |= CF_TEXT;
				break;
			case 'B':
			case 'b':
				copyflag |= CF_BINARY;
				copyflag &= ~CF_TEXT;
				break;
			case 'V':
			case 'v':
				copyflag |= CF_VERIFY;
				break;
			case 'Y':
			case 'y':
				copyflag |= CF_NOCONFIRM;
				break;
			default:
				break;
		}
	}
	return(i);
}

static int NEAR getbinmode(name, argv, bin)
char *name, *argv[];
int bin;
{
	char *cp;

	if (bin < 0) return(-1);
	if ((cp = strchr(name, DOSCOMOPT))) {
		if (!cp[1] || (cp[2] && cp[3])) {
			doserror(cp, ER_REQPARAM);
			return(-1);
		}
		*(cp++) = '\0';
		if (*cp == 'B' || *cp == 'b') bin = CF_BINARY;
		else if (*cp == 'A' || *cp == 'a') bin = CF_TEXT;
	}
	return(bin);
}

static int NEAR writeopen(file, src)
char *file, *src;
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
		if ((fd = Xopen(file, O_RDONLY, 0644)) < 0) return(fd);
		c = (isatty(fd)) ? 0 : 1;
		close(fd);
	}

	if (c) {
		fputs("Overwrite ", stderr);
		kanjifputs(file, stderr);
		fputs(" (Yes/No/All)?", stderr);
		fflush(stderr);
		key = -1;
		for (;;) {
			c = inputkey();
			if (c == K_CR && key >= 0) break;
			if (!strchr("ynaYNA", c)) continue;
			key = toupper2(c);
			fputc(c, stderr);
			fputs(c_left, stderr);
			fflush(stderr);
		}
		fputc('\n', stderr);
		fflush(stderr);
		if (key == 'N') return(0);
		else if (key == 'A') copyflag |= CF_NOCONFIRM;
	}
	else if (copyflag & CF_VERBOSE) {
		kanjifputs(src, stdout);
		fputc('\n', stdout);
		fflush(stdout);
	}

	flags = (copyflag & CF_VERIFY) ? O_RDWR : O_WRONLY;
	flags |= (O_BINARY | O_CREAT | O_TRUNC);
	return(Xopen(file, flags, 0644));
}

static int NEAR textread(fd, buf, size, bin)
int fd;
u_char *buf;
int size, bin;
{
	u_char ch;
	int i, n;

	if (!(bin & (CF_BINARY | CF_TEXT))) bin = copyflag;
	if (!(bin & CF_TEXT)) return(Xread(fd, buf, size));
	for (i = 0; i < size; i++) {
		while ((n = Xread(fd, &ch, sizeof(u_char))) < 0
		&& errno == EINTR);
		if (n < 0) return(n);
		else if (n < sizeof(u_char) || ch == C_EOF) return(i);
		buf[i] = ch;
	}
	return(i);
}

static int NEAR textclose(fd, bin)
int fd, bin;
{
	u_char ch;
	int n, ret;

	ret = 0;
	if (!(bin & (CF_BINARY | CF_TEXT))) bin = copyflag;
	if (bin & CF_TEXT) {
		ch = C_EOF;
		if ((n = safewrite(fd, &ch, sizeof(u_char))) <= 0) {
			if (!n) errno = ENOSPC;
			ret = -1;
		}
	}
	Xclose(fd);
	return(ret);
}

static int NEAR doscopy(src, dest, stp, sbin, dbin, dfd)
char *src, *dest;
struct stat *stp;
int sbin, dbin, dfd;
{
	off_t ofs;
	char buf[BUFSIZ], buf2[BUFSIZ];
	int i, n, fd1, fd2, tty, retry, tmperrno;

#if	!MSDOS
	if (dfd < 0 && (stp -> st_mode & S_IFMT) == S_IFLNK) {
		if ((i = Xreadlink(src, buf, BUFSIZ - 1)) < 0) return(-1);
		buf[i] = '\0';
		return(Xsymlink(buf, dest) >= 0);
	}
#endif

	if ((fd1 = Xopen(src, O_RDONLY | O_BINARY, stp -> st_mode)) < 0)
		return(-1);
	if (dfd >= 0) {
		fd2 = dfd;
		ofs = lseek(fd2, 0L, L_INCR);
	}
	else if ((fd2 = writeopen(dest, src)) <= 0) {
		Xclose(fd1);
		return(fd2);
	}
	else ofs = 0L;

	if (isatty(fd1)) sbin = CF_TEXT;
	tty = isatty(fd2);

	for (retry = 0; retry < 10; retry++) {
		for (;;) {
			while ((i = textread(fd1, buf, BUFSIZ, sbin)) < 0
			&& errno == EINTR);
			if (i <= 0) {
				tmperrno = errno;
				break;
			}
			if ((n = safewrite(fd2, buf, i)) <= 0) {
				tmperrno = (n < 0) ? errno : ENOSPC;
				i = -1;
				break;
			}
			if (i < BUFSIZ) break;
		}

		if (i < 0 || !(copyflag & CF_VERIFY)
		|| lseek(fd1, 0L, L_SET) < 0
		|| lseek(fd2, ofs, L_SET) < 0) break;
		for (;;) {
			while ((i = textread(fd1, buf, BUFSIZ, sbin)) < 0
			&& errno == EINTR);
			if (i <= 0) {
				tmperrno = errno;
				break;
			}
			while ((n = textread(fd2, buf2, BUFSIZ, CF_BINARY)) < 0
			&& errno == EINTR);
			if (n < 0) {
				tmperrno = errno;
				i = -1;
				break;
			}
			if (n != i || memcmp(buf, buf2, i)) {
				tmperrno = EINVAL;
				n = -1;
				break;
			}
			if (i < BUFSIZ) break;
		}
		if (i < 0 || n >= 0) break;
	}

	if (dfd < 0) textclose(fd2, dbin);
	Xclose(fd1);

	if (i < 0 || retry >= 10) {
		Xunlink(dest);
		errno = tmperrno;
		return(dfd < 0 ? -1 : -2);
	}

	if (!tty && touchfile(dest, stp -> st_atime, stp -> st_mtime) < 0)
		return(-1);
	return(1);
}

int doscomcopy(argc, argv)
int argc;
char *argv[];
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
	src = (char *)malloc2(size);
	strcpy(src, argv[n]);
	for (n++; n < argc; n++) {
		if ((cp = strchr(&(argv[n - 1][1]), '+')) && !*(++cp));
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
		sbin[i - 1] = getbinmode(arg[i - 1], argv, sbin[i - 1]);
		sbin[i] = sbin[i - 1];
	}
	sbin[i - 1] = getbinmode(arg[i - 1], argv, sbin[i - 1]);

	if (n >= argc) {
		*dest = '\0';
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
		dbin = getbinmode(dest, argv, sbin[i - 1]);
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
		if ((file = strrdelim(dest, 1))) file++;
		else file = dest;
		if (strpbrk(file, "*?")) form = &(argv[n][file - dest]);
		else if (*file) file = NULL;
	}

	if (nf <= 1) {
		if (Xlstat(arg[0], &sst) < 0);
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
		if (!(wild = evalwild(arg[n]))) {
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

			if ((src = strrdelim(wild[i], 1))) src++;
			else src = wild[i];
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
	fprintf(stdout, "%9d file(s) copied\n\n", nc);
	fflush(stdout);
	return(ret);
}

/*ARGSUSED*/
int doscomcls(argc, argv)
int argc;
char *argv[];
{
	fputs(t_clear, stdout);
	fflush(stdout);
	fputc('\n', stderr);
	fflush(stderr);
	return(RET_SUCCESS);
}
#endif	/* !_NOORIGSHELL && DOSCOMMAND && (!FD || (FD >= 2)) */
