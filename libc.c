/*
 *	libc.c
 *
 *	Arrangememt of Library Function
 */

#include <fcntl.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"

#ifndef	NOTZFILEH
#include <tzfile.h>
#endif

#if	MSDOS
#include <sys/timeb.h>
extern int setcurdrv __P_((int, int));
extern char *unixrealpath __P_((char *, char *));
#endif

#ifndef	_NODOSDRIVE
extern int flushdrv __P_((int, VOID_T (*)__P_((VOID_A))));
extern char *dosgetcwd __P_((char *, int));
#endif

#ifdef	_NOORIGSHELL
#include <signal.h>
#define	dosystem(s)		system(s)
#else
#include "system.h"
#endif

#ifdef	CYGWIN
#include <sys/cygwin.h>
#endif

#ifdef	PWNEEDERROR
char Error[1024];
#endif

#ifdef	_NOORIGSHELL
extern char **environ;
#endif
extern char fullpath[];
extern char *origpath;
extern int hideclock;
#ifndef	_NODOSDRIVE
extern int lastdrv;
#endif

#ifndef	SIG_ERR
#define	SIG_ERR		((sigcst_t)-1)
#endif
#ifndef	CHAR_BIT
# ifdef	NBBY
# define	CHAR_BIT	NBBY
# else
# define	CHAR_BIT	0x8
# endif
#endif
#define	char2long(cp)	(  ((u_char *)(cp))[3] \
			| (((u_char *)(cp))[2] << (CHAR_BIT * 1)) \
			| (((u_char *)(cp))[1] << (CHAR_BIT * 2)) \
			| (((u_char *)(cp))[0] << (CHAR_BIT * 3)) )

#ifndef	NOSYMLINK
static int NEAR evallink __P_((char *, char *));
#endif
static char *NEAR _realpath2 __P_((char *, char *, int));
#ifdef	_NOORIGSHELL
static int NEAR _getenv2 __P_((char *, int, char **));
static char **NEAR _putenv2 __P_((char *, char **));
#endif
#if	!MSDOS && !defined (NOTZFILEH) \
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static int NEAR tmcmp __P_((struct tm *, struct tm *));
#endif
#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long NEAR gettimezone __P_((struct tm *, time_t));
#endif

#ifdef	_NOORIGSHELL
char **environ2 = NULL;
#endif
int physical_path = 0;
int norealpath = 0;

static char *lastpath = NULL;
#ifndef	_NODOSDRIVE
static char *unixpath = NULL;
#endif
static int wasttyflags = 0;
#define	TF_TTYIOMODE	001
#define	TF_TTYNL	002


int stat2(path, stp)
char *path;
struct stat *stp;
{
#ifdef	NOSYMLINK
	return(Xstat(path, stp));
#else	/* !NOSYMLINK */
	int duperrno;

	if (Xstat(path, stp) < 0) {
		duperrno = errno;
# ifndef	_NODOSDRIVE
		if (dospath2(path)) {
			errno = duperrno;
			return(-1);
		}
# endif
		if (nodoslstat(path, stp) < 0
		|| (stp -> st_mode & S_IFMT) != S_IFLNK) {
			errno = duperrno;
			return(-1);
		}
		stp -> st_mode &= ~S_IFMT;
	}
	return(0);
#endif	/* !NOSYMLINK */
}

#ifndef	NOSYMLINK
static int NEAR evallink(path, delim)
char *path, *delim;
{
	struct stat st;
	char buf[MAXPATHLEN];
	int i, duperrno;

	duperrno = errno;
	if (nodoslstat(path, &st) < 0) {
		errno = duperrno;
		return(-1);
	}
	if ((st.st_mode & S_IFMT) != S_IFLNK) {
		errno = duperrno;
		return(0);
	}
	if ((i = Xreadlink(path, buf, MAXPATHLEN - 1)) < 0) {
		errno = duperrno;
		return(-1);
	}

	if (*buf == _SC_ || !delim || delim == path) {
		*path = _SC_;
		delim = path + 1;
	}
	buf[i] = *delim = '\0';
	_realpath2(buf, path, 1);
	errno = duperrno;
	return(1);
}
#endif	/* !NOSYMLINK */

/*ARGSUSED*/
static char *NEAR _realpath2(path, resolved, rdlink)
char *path, *resolved;
int rdlink;
{
	char *cp, *top;

	if (!*path || !strcmp(path, ".")) return(resolved);
	else if ((cp = strdelim(path, 0))) {
		*cp = '\0';
		_realpath2(path, resolved, rdlink);
		*(cp++) = _SC_;
		_realpath2(cp, resolved, rdlink);
		return(resolved);
	}

	if (!strcmp(path, "..")) {
		cp = strrdelim(resolved, 0);
		top = resolved;
#if	MSDOS
		top += 2;
#else	/* !MSDOS */
# ifndef	_NODOSDRIVE
		if (_dospath(resolved)) top += 2;
		else
# endif
		if (rdlink && evallink(resolved, cp) > 0)
			return(_realpath2(path, resolved, rdlink));
#endif	/* !MSDOS */
		if (!cp || cp == top) {
			*top = _SC_;
			cp = top + 1;
		}
		*cp = '\0';
	}
	else {
		cp = strcatdelim(resolved);
		strncpy2(cp, path, MAXPATHLEN - 1 - (cp - resolved));
#ifndef	NOSYMLINK
		if (!rdlink) /*EMPTY*/;
# ifndef	_NODOSDRIVE
		else if (_dospath(resolved)) /*EMPTY*/;
# endif
		else evallink(resolved, cp - 1);
#endif	/* !NOSYMLINK */
	}
	return(resolved);
}

char *realpath2(path, resolved, rdlink)
char *path, *resolved;
int rdlink;
{
#if	!MSDOS && !defined (_NODOSDRIVE)
	char *cp;
	int duplastdrive;
#endif
#if	MSDOS || !defined (_NODOSDRIVE)
	int drv;
#endif
	char tmp[MAXPATHLEN];

	strcpy(tmp, path);
	path = tmp;

#if	MSDOS
	drv = dospath("", NULL);
	if ((resolved[0] = _dospath(path))) path += 2;
	if (*path == _SC_) {
		if (!resolved[0]) resolved[0] = drv;
		resolved[1] = ':';
		resolved[2] = _SC_;
		resolved[3] = '\0';
	}
	else if (resolved[0] && resolved[0] != drv) {
		if (setcurdrv(resolved[0], 0) < 0) {
			resolved[1] = ':';
			resolved[2] = _SC_;
			resolved[3] = '\0';
		}
		else {
			if (!Xgetwd(resolved)) lostcwd(resolved);
			if (setcurdrv(drv, 0) < 0) error("setcurdrv()");
		}
	}
#else	/* !MSDOS */
	if (*path == _SC_) strcpy(resolved, _SS_);
# ifndef	_NODOSDRIVE
	else if ((drv = _dospath(path))) {
		path += 2;
		if (*path == _SC_) cp = NULL;
		else {
			duplastdrive = lastdrive;
			lastdrive = drv;
			cp = dosgetcwd(resolved, MAXPATHLEN - 1);
			lastdrive = duplastdrive;
		}
		if (!cp) {
			resolved[0] = drv;
			resolved[1] = ':';
			resolved[2] = _SC_;
			resolved[3] = '\0';
		}
	}
# endif
#endif	/* !MSDOS */
	else if (!rdlink && resolved != fullpath && *fullpath)
		strcpy(resolved, fullpath);
	else if (!Xgetwd(resolved)) strcpy(resolved, _SS_);
	norealpath++;
	_realpath2(path, resolved, rdlink);
	norealpath--;
	return(resolved);
}

int _chdir2(path)
char *path;
{
	char cwd[MAXPATHLEN];

	if (!Xgetwd(cwd)) strcpy(cwd, _SS_);
	if (Xchdir(path) < 0) return(-1);
#if	!MSDOS
# ifndef	_NODOSDRIVE
	if (dospath2("")) /*EMPTY*/;
	else
# endif
	{
		int fd, duperrno;

		if ((fd = open(".", O_RDONLY, 0600)) < 0) {
# ifdef	CYGWIN
			char upath[MAXPATHLEN], spath[MAXPATHLEN];
			char tmp[MAXPATHLEN];
			int len;

			if (Xgetwd(tmp)) {
				len = strlen(tmp);
				cygwin_internal(CW_GET_CYGDRIVE_PREFIXES,
					upath, spath);
				if ((*upath && !strnpathcmp(tmp, upath, len)
				&& (!upath[len] || upath[len] == _SC_))
				|| (*spath && !strnpathcmp(tmp, spath, len)
				&& (!spath[len] || spath[len] == _SC_)))
					return(0);
			}
# endif	/* CYGWIN */
			duperrno = errno;
			if (Xchdir(cwd) < 0) lostcwd(cwd);
			errno = duperrno;
			return(-1);
		}
		close(fd);
	}
#endif	/* !MSDOS */
	return(0);
}

int chdir2(path)
char *path;
{
	char cwd[MAXPATHLEN], tmp[MAXPATHLEN];
	int duperrno;

#ifdef	DEBUG
	if (!path) {
		if (lastpath) free(lastpath);
		lastpath = NULL;
# ifndef	_NODOSDRIVE
		if (unixpath) free(unixpath);
		unixpath = NULL;
# endif
		return(0);
	}
#endif

	realpath2(path, tmp, physical_path);
	strcpy(cwd, fullpath);
	strcpy(fullpath, tmp);

	if (_chdir2(fullpath) < 0) {
		duperrno = errno;
		if (_chdir2(cwd) < 0) lostcwd(fullpath);
		else strcpy(fullpath, cwd);
		errno = duperrno;
		return(-1);
	}
	if (lastpath) free(lastpath);
	lastpath = strdup2(cwd);
#ifndef	_NODOSDRIVE
	if (dospath2(fullpath)) {
		if (!dospath2(cwd)) {
			if (unixpath) free(unixpath);
			unixpath = strdup2(cwd);
		}
		if (Xgetwd(cwd)) strcpy(fullpath, cwd);
		realpath2(fullpath, fullpath, 0);
	}
	else {
		if (unixpath) free(unixpath);
		unixpath = NULL;
	}
#endif
	if (getconstvar("PWD")) setenv2("PWD", fullpath, 1);
#if	MSDOS
	if (unixrealpath(fullpath, tmp)) strcpy(fullpath, tmp);
#endif
	entryhist(1, fullpath, 1);
	return(0);
}

int chdir3(path)
char *path;
{
#ifndef	_NODOSDRIVE
	int drive;
#endif
	char *cwd;

	cwd = path;
	if (!strcmp(path, ".")) cwd = NULL;
	else if (!strcmp(path, "?")) path = origpath;
	else if (!strcmp(path, "-")) {
		if (!lastpath) {
			errno = ENOENT;
			return(-1);
		}
		path = lastpath;
	}
#ifndef	_NODOSDRIVE
	else if (!strcmp(path, "@")) {
		if (!unixpath) {
			errno = ENOENT;
			return(-1);
		}
		path = unixpath;
	}
	if ((drive = dospath3(fullpath))) flushdrv(drive, NULL);
#endif
	if (chdir2(path) < 0) return(-1);
	if (!cwd) {
		if (!Xgetwd(fullpath)) lostcwd(fullpath);
	}
	else {
		if (findpattern) free(findpattern);
		findpattern = NULL;
	}
	return(0);
}

int mkdir2(path, mode)
char *path;
int mode;
{
	char *cp1, *cp2, *eol;

	eol = path + (int)strlen(path) - 1;
	while (eol > path && *eol == _SC_) eol--;
#ifdef	BSPATHDELIM
	if (onkanji1(path, eol - path)) eol++;
#endif
	*(++eol) = '\0';

	cp1 = eol;
	cp2 = strrdelim(path, 0);
	for (;;) {
		if (Xmkdir(path, mode) >= 0) break;
		if (errno != ENOENT || !cp2 || cp2 <= path) return(-1);
		*cp2 = '\0';
		if (cp1 < eol) *cp1 = _SC_;
		cp1 = cp2;
		cp2 = strrdelim(path, 0);
	}

	while (cp1 && cp1 < eol) {
		cp2 = strdelim(cp1 + 1, 0);
		*cp1 = _SC_;
		if (cp2) *cp2 = '\0';
		if (Xmkdir(path, mode) < 0 && errno != EEXIST) return(-1);
		cp1 = cp2;
	}
	return(0);
}

char *malloc2(size)
ALLOC_T size;
{
	char *tmp;

	if (!size || !(tmp = (char *)malloc(size))) {
		error("malloc()");
#ifdef	FAKEUNINIT
		tmp = NULL;	/* fake for -Wuninitialized */
#endif
	}
	return(tmp);
}

char *realloc2(ptr, size)
VOID_P ptr;
ALLOC_T size;
{
	char *tmp;

	if (!size
	|| !(tmp = (ptr) ? (char *)realloc(ptr, size) : (char *)malloc(size)))
	{
		error("realloc()");
#ifdef	FAKEUNINIT
		tmp = NULL;	/* fake for -Wuninitialized */
#endif
	}
	return(tmp);
}

char *c_realloc(ptr, n, sizep)
char *ptr;
ALLOC_T n, *sizep;
{
	if (!ptr) {
		*sizep = BUFUNIT;
		return(malloc2(*sizep));
	}
	while (n + 1 >= *sizep) *sizep *= 2;
	return(realloc2(ptr, *sizep));
}

char *strdup2(s)
char *s;
{
	char *tmp;
	int n;

	if (!s) return(NULL);
	n = strlen(s);
	if (!(tmp = (char *)malloc((ALLOC_T)n + 1))) error("malloc()");
	memcpy(tmp, s, n + 1);
	return(tmp);
}

char *strndup2(s, n)
char *s;
int n;
{
	char *tmp;
	int i;

	if (!s) return(NULL);
	for (i = 0; i < n; i++) if (!s[i]) break;
	if (!(tmp = (char *)malloc((ALLOC_T)i + 1))) error("malloc()");
	memcpy(tmp, s, i);
	tmp[i] = '\0';
	return(tmp);
}

char *strchr2(s, c)
char *s;
int c;
{
	int i;

	for (i = 0; s[i]; i++) {
		if (s[i] == c) return(&(s[i]));
		if (iskanji1(s, i)) i++;
	}
	return(NULL);
}

char *strrchr2(s, c)
char *s;
int c;
{
	int i;
	char *cp;

	cp = NULL;
	for (i = 0; s[i]; i++) {
		if (s[i] == c) cp = &(s[i]);
		if (iskanji1(s, i)) i++;
	}
	return(cp);
}

char *strcpy2(s1, s2)
char *s1, *s2;
{
	int i;

	for (i = 0; s2[i]; i++) s1[i] = s2[i];
	s1[i] = '\0';
	return(&(s1[i]));
}

char *strncpy2(s1, s2, n)
char *s1, *s2;
int n;
{
	int i;

	for (i = 0; i < n && s2[i]; i++) s1[i] = s2[i];
	s1[i] = '\0';
	return(&(s1[i]));
}

/*
 *	strncpy3(buf, s, &(x), 0): same as sprintf(buf, "%-*.*s", x, x, s);
 *	strncpy3(buf, s, &(-x), 0): same as sprintf(buf, "%s", s);
 */
int strncpy3(s1, s2, lenp, ptr)
char *s1, *s2;
int *lenp, ptr;
{
	int i, j, l;

	for (i = j = 0; i < ptr && s2[j]; i++, j++) {
#ifdef	CODEEUC
		if (isekana(s2, j)) j++;
		else
#endif
		if (iskanji1(s2, j)) {
			i++;
			j++;
		}
	}
	if (!i || i <= ptr) i = 0;
	else {
		s1[0] = ' ';
		i = 1;
	}

	while ((*lenp < 0 || i < *lenp) && s2[j]) {
#ifdef	CODEEUC
		if (isekana(s2, j)) {
			if (*lenp >= 0) (*lenp)++;
			s1[i++] = s2[j++];
		}
#else
		if (iskna(s2[j])) /*EMPTY*/;
#endif
		else if (iskanji1(s2, j)) {
			if (*lenp >= 0 && i >= *lenp - 1) {
				if (ptr >= 0) s1[i++] = ' ';
				break;
			}
			s1[i++] = s2[j++];
		}
		else if (isctl(s2[j])) {
			s1[i++] = '^';
			if (*lenp >= 0 && i >= *lenp) break;
			s1[i++] = ((s2[j++] + '@') & 0x7f);
			continue;
		}
		else if (ismsb(s2[j])) {
			int c;

			c = s2[j++] & 0xff;
			s1[i++] = '\\';
			if (*lenp >= 0 && i >= *lenp) break;
			s1[i++] = (c / (8 * 8)) + '0';
			if (*lenp >= 0 && i >= *lenp) break;
			s1[i++] = ((c % (8 * 8)) / 8) + '0';
			if (*lenp >= 0 && i >= *lenp) break;
			s1[i++] = (c % 8) + '0';
			continue;
		}
		s1[i++] = s2[j++];
	}

	l = i;
	if (*lenp >= 0 && ptr >= 0) while (i < *lenp) s1[i++] = ' ';
	s1[i] = '\0';
	return(l);
}

int strlen2(s)
char *s;
{
	int i, len;

	for (i = len = 0; s[i]; i++, len++) if (isctl(s[i])) len++;
	return(len);
}

int strlen3(s)
char *s;
{
	int i, len;

	for (i = len = 0; s[i]; i++, len++) {
#ifdef	CODEEUC
		if (isekana(s, i)) i++;
		else
#endif
		if (isctl(s[i])) len++;
	}
	return(len);
}

int atoi2(s)
char *s;
{
	long n;

	if (!s || !(s = evalnumeric(s, &n, 0)) || *s) return(-1);
#if	MSDOS
	if (n > MAXTYPE(int)) return(-1);
#endif
	return((int)n);
}

#ifdef	USESTDARGH
/*VARARGS2*/
int fprintf2(FILE *fp, CONST char *fmt, ...)
#else
/*VARARGS2*/
int fprintf2(fp, fmt, va_alist)
FILE *fp;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *buf;
	int n;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	n = vasprintf2(&buf, fmt, args);
	va_end(args);
	if (n < 0) error("malloc()");
	fputs(buf, fp);
	free(buf);
	return(n);
}

#ifdef	USESTDARGH
/*VARARGS3*/
int snprintf2(char *s, int size, CONST char *fmt, ...)
#else
/*VARARGS3*/
int snprintf2(s, size, fmt, va_alist)
char *s;
int size;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *buf;
	int n;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	n = vasprintf2(&buf, fmt, args);
	va_end(args);
	if (n < 0) error("malloc()");
	strncpy2(s, buf, size - 1);
	free(buf);
	return(n);
}

VOID perror2(s)
char *s;
{
	int duperrno;

	duperrno = errno;
	if (s) {
		kanjifputs(s, stderr);
		fputs(": ", stderr);
	}
	fputs(strerror2(duperrno), stderr);
	if (isttyiomode) fputc('\r', stderr);
	fputc('\n', stderr);
	fflush(stderr);
}

#ifdef	_NOORIGSHELL
static int NEAR _getenv2(name, len, envp)
char *name;
int len;
char **envp;
{
	int i;

	if (!envp) return(-1);

	for (i = 0; envp[i]; i++)
		if (!strnenvcmp(envp[i], name, len) && envp[i][len] == '=')
			break;
	return(i);
}

static char **NEAR _putenv2(s, envp)
char *s, **envp;
{
	char *cp, **new;
	int i, n, len;

	if ((cp = strchr(s, '='))) len = (int)(cp - s);
	else len = strlen(s);

	if ((n = _getenv2(s, len, envp)) < 0) n = 0;
	else if (envp[n]) {
		free(envp[n]);
		if (cp) envp[n] = s;
		else for (i = n; envp[i]; i++) envp[i] = envp[i + 1];
		return(envp);
	}
	if (!cp) return(envp);

	new = (char **)realloc2(envp, (n + 2) * sizeof(char *));
	new[n] = s;
	new[n + 1] = (char *)NULL;
	return(new);
}
#endif	/* _NOORIGSHELL */

char *getenv2(name)
char *name;
{
#ifdef	_NOORIGSHELL
	char **envpp[2];
	int i, n, len;

	len = strlen(name);
	envpp[0] = environ2;
	envpp[1] = environ;

	for (i = 0; i < 2; i++) {
		n = _getenv2(name, len, envpp[i]);
		if (n >= 0 && envpp[i][n]) return(&(envpp[i][n][len + 1]));
		if (strnenvcmp(name, FDENV, FDESIZ)) continue;
		n = _getenv2(&(name[FDESIZ]), len - FDESIZ, envpp[i]);
		if (n >= 0 && envpp[i][n])
			return(&(envpp[i][n][len - FDESIZ + 1]));
	}
#else	/* !_NOORIGSHELL */
	char *cp;

	if ((cp = getshellvar(name, -1))) return(cp);
	if (!strnenvcmp(name, FDENV, FDESIZ)
	&& (cp = getshellvar(&(name[FDESIZ]), -1)))
		return(cp);
#endif	/* !_NOORIGSHELL */
	return(NULL);
}

int setenv2(name, value, export)
char *name, *value;
int export;
{
	char *cp;
	int len;
#if	defined (ENVNOCASE) && defined (_NOORIGSHELL)
	int i;
#endif

	len = strlen(name);
	if (!value) {
#ifdef	_NOORIGSHELL
		cp = name;
#else
		return(unset(name, len));
#endif
	}
	else {
		cp = malloc2(len + strlen(value) + 2);
		memcpy(cp, name, len);
#if	defined (ENVNOCASE) && defined (_NOORIGSHELL)
		for (i = 0; i < len ; i++) cp[i] = toupper2(cp[i]);
#endif
		cp[len] = '=';
		strcpy(&(cp[len + 1]), value);
	}
#ifdef	_NOORIGSHELL
	if (export) environ = _putenv2(cp, environ);
	else environ2 = _putenv2(cp, environ2);
#else	/* !_NOORIGSHELL */
	if (((export) ? putexportvar(cp, len) : putshellvar(cp, len)) < 0) {
		free(cp);
		return(-1);
	}
#endif	/* !_NOORIGSHELL */
	return(0);
}

#ifdef	USESIGACTION
sigcst_t signal2(sig, func)
int sig;
sigcst_t func;
{
	struct sigaction act, oact;

	act.sa_handler = func;
	act.sa_flags = 0;
	sigemptyset(&(act.sa_mask));
	sigemptyset(&(oact.sa_mask));
	if (sigaction(sig, &act, &oact) < 0) return(SIG_ERR);
	return(oact.sa_handler);
}
#endif	/* USESIGACTION */

int system2(command, noconf)
char *command;
int noconf;
{
	int n, wastty, mode, nl, ret;

	if (!command || !*command) return(0);
#ifdef	FAKEUNINIT
	mode = nl = 0;		/* fake for -Wuninitialized */
#endif
	n = sigvecset(0);
	if ((wastty = isttyiomode)) {
		if (noconf >= 0) {
			locate(0, n_line - 1);
			putterm(l_clear);
		}
		if (n && noconf > 0) mode = termmode(0);
		nl = stdiomode();
	}

	ret = dosystem(command);
	sigvecset(n);
	if (ret >= 127 && noconf > 0) {
		if (dumbterm <= 2) fputc('\007', stderr);
		fputc('\n', stderr);
		kanjifputs(HITKY_K, stderr);
		fflush(stderr);
		ttyiomode(1);
		keyflush();
		getkey2(0);
		stdiomode();
		fputc('\n', stderr);
	}

	if (wastty) {
		ttyiomode(nl);
		if (n && noconf > 0) termmode(mode);
		if (!noconf || (noconf < 0 && ret >= 127)) {
			hideclock = 1;
			warning(0, HITKY_K);
		}
	}
	return(ret);
}

/*ARGSUSED*/
FILE *popen2(command, type)
char *command, *type;
{
	FILE *fp;
	int n;

	if (!command || !*command) return(NULL);
	n = sigvecset(0);
	wasttyflags = 0;
	if (isttyiomode) {
		wasttyflags |= TF_TTYIOMODE;
		if (stdiomode()) wasttyflags |= TF_TTYNL;
	}

#ifdef	_NOORIGSHELL
# ifdef	DEBUG
	_mtrace_file = "popen(start)";
	fp = Xpopen(command, type);
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "popen(end)";
		malloc(0);	/* dummy malloc */
	}
# else
	fp = Xpopen(command, type);
# endif
#else	/* !_NOORIGSHELL */
	fp = dopopen(command);
#endif	/* !_NOORIGSHELL */
	sigvecset(n);
	if (fp) {
		if (wasttyflags & TF_TTYIOMODE) {
			putterms(t_keypad);
			tflush();
		}
	}
	else {
		if (dumbterm <= 2) fputc('\007', stderr);
		fputc('\n', stderr);
		perror2(command);
		fflush(stderr);
		ttyiomode(1);
		keyflush();
		getkey2(0);
		stdiomode();
		fputc('\n', stderr);
		if (wasttyflags & TF_TTYIOMODE)
			ttyiomode((wasttyflags & TF_TTYNL) ? 1 : 0);
	}
	return(fp);
}

int pclose2(fp)
FILE *fp;
{
	int ret;

#ifdef	_NOORIGSHELL
	ret = Xpclose(fp);
#else
	ret = dopclose(fp);
#endif
	if (wasttyflags & TF_TTYIOMODE)
		ttyiomode((wasttyflags & TF_TTYNL) ? 1 : 0);
	return(ret);
}

char *getwd2(VOID_A)
{
	char cwd[MAXPATHLEN];

	if (!Xgetwd(cwd)) lostcwd(cwd);
	return(strdup2(cwd));
}

#if	!MSDOS && !defined (NOTZFILEH) \
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static int NEAR tmcmp(tm1, tm2)
struct tm *tm1, *tm2;
{
	if (tm1 -> tm_year != tm2 -> tm_year)
		return (tm1 -> tm_year - tm2 -> tm_year);
	if (tm1 -> tm_mon != tm2 -> tm_mon)
		return (tm1 -> tm_mon - tm2 -> tm_mon);
	if (tm1 -> tm_mday != tm2 -> tm_mday)
		return (tm1 -> tm_mday - tm2 -> tm_mday);
	if (tm1 -> tm_hour != tm2 -> tm_hour)
		return (tm1 -> tm_hour - tm2 -> tm_hour);
	if (tm1 -> tm_min != tm2 -> tm_min)
		return (tm1 -> tm_min - tm2 -> tm_min);
	return (tm1 -> tm_sec - tm2 -> tm_sec);
}
#endif	/* !MSDOS && !NOTZFILEH && !USEMKTIME && !USETIMELOCAL */

#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long NEAR gettimezone(tm, t)
struct tm *tm;
time_t t;
{
# if	MSDOS
	struct timeb buffer;

	ftime(&buffer);
	return((long)(buffer.timezone) * 60L);
# else	/* !MSDOS */
#  ifdef	NOTMGMTOFF
	struct timeval t_val;
	struct timezone t_zone;
#  endif
#  ifndef	NOTZFILEH
	struct tzhead head;
#  endif
	struct tm tmbuf;
	FILE *fp;
	time_t tmp;
	long i, tz, leap, nleap, ntime, ntype, nchar;
	char *cp, buf[MAXPATHLEN];
	u_char c;

	memcpy((char *)&tmbuf, (char *)tm, sizeof(struct tm));

#  ifdef	NOTMGMTOFF
	gettimeofday2(&t_val, &t_zone);
	tz = t_zone.tz_minuteswest * 60L;
#  else
	tz = -(localtime(&t) -> tm_gmtoff);
#  endif

#  ifndef	NOTZFILEH
	cp = getconstvar("TZ");
	if (!cp || !*cp) cp = TZDEFAULT;
	if (cp[0] == _SC_) strcpy(buf, cp);
	else strcatdelim2(buf, TZDIR, cp);
	if (!(fp = fopen(buf, "r"))) return(tz);
	if (fread(&head, sizeof(struct tzhead), 1, fp) != 1) {
		fclose(fp);
		return(tz);
	}
#   ifdef	USELEAPCNT
	nleap = char2long(head.tzh_leapcnt);
#   else
	nleap = char2long(head.tzh_timecnt - 4);
#   endif
	ntime = char2long(head.tzh_timecnt);
	ntype = char2long(head.tzh_typecnt);
	nchar = char2long(head.tzh_charcnt);

	for (i = 0; i < ntime; i++) {
		if (fread(buf, sizeof(char), 4, fp) != 4) {
			fclose(fp);
			return(tz);
		}
		tmp = char2long(buf);
		if (tmcmp(&tmbuf, localtime(&tmp)) < 0) break;
	}
	if (i > 0) {
		i--;
		i *= sizeof(char);
		i += sizeof(struct tzhead) + ntime * sizeof(char) * 4;
		if (fseek(fp, i, 0) < 0
		|| fread(&c, sizeof(char), 1, fp) != 1) {
			fclose(fp);
			return(tz);
		}
		i = c;
	}
	i *= sizeof(char) * (4 + 1 + 1);
	i += sizeof(struct tzhead) + ntime * sizeof(char) * (4 + 1);
	if (fseek(fp, i, 0) < 0
	|| fread(buf, sizeof(char), 4, fp) != 4) {
		fclose(fp);
		return(tz);
	}
	tmp = char2long(buf);
	tz = -tmp;

	i = sizeof(struct tzhead) + ntime * sizeof(char) * (4 + 1)
		+ ntype * sizeof(char) * (4 + 1 + 1)
		+ nchar * sizeof(char);
	if (fseek(fp, i, 0) < 0) {
		fclose(fp);
		return(tz);
	}
	leap = 0;
	for (i = 0; i < nleap; i++) {
		if (fread(buf, sizeof(char), 4, fp) != 4) {
			fclose(fp);
			return(tz);
		}
		tmp = char2long(buf);
		if (tmcmp(&tmbuf, localtime(&tmp)) <= 0) break;
		if (fread(buf, sizeof(char), 4, fp) != 4) {
			fclose(fp);
			return(tz);
		}
		leap = char2long(buf);
	}

	tz += leap;
	fclose(fp);
#  endif	/* !NOTZFILEH */

	return(tz);
# endif	/* !MSDOS */
}
#endif	/* !USEMKTIME && !USETIMELOCAL */

time_t time2(VOID_A)
{
#if	MSDOS
	struct timeb buffer;

	ftime(&buffer);
	return((time_t)(buffer.time));
#else
	struct timeval t_val;
	struct timezone tz;

	gettimeofday2(&t_val, &tz);
	return((time_t)(t_val.tv_sec));
#endif
}

time_t timelocal2(tm)
struct tm *tm;
{
#ifdef	USEMKTIME
	tm -> tm_isdst = -1;
	return(mktime(tm));
#else	/* !USEMKTIME */
# ifdef	USETIMELOCAL
	return(timelocal(tm));
# else	/* !USETIMELOCAL */
	time_t d, t;
	int i, y;

	y = (tm -> tm_year < 1900) ? tm -> tm_year + 1900 : tm -> tm_year;

	d = ((long)y - 1970) * 365;
	d += ((y - 1 - 1968) / 4)
		- ((y - 1 - 1900) / 100)
		+ ((y - 1 - 1600) / 400);
	for (i = 1; i < tm -> tm_mon + 1; i++) {
		switch (i) {
			case 2:
				if (!(y % 4)
				&& ((y % 100)
				|| !(y % 400))) d++;
				d += 28;
				break;
			case 4:
			case 6:
			case 9:
			case 11:
				d += 30;
				break;
			default:
				d += 31;
				break;
		}
	}
	d += tm -> tm_mday - 1;
	t = ((long)(tm -> tm_hour) * 60L + tm -> tm_min) * 60L + tm -> tm_sec;
	t += d * 60L * 60L * 24L;
	t += gettimezone(tm, t);

	return(t);
# endif	/* !USETIMELOCAL */
#endif	/* !USEMKTIME */
}

char *fgets2(fp, nulcnv)
FILE *fp;
int nulcnv;
{
	char *cp;
	ALLOC_T i, size;
	int c;

	cp = c_realloc(NULL, 0, &size);
	for (i = 0; (c = Xfgetc(fp)) != '\n'; i++) {
		if (c == EOF) {
			if (!i || ferror(fp)) {
				free(cp);
				return(NULL);
			}
			break;
		}
		cp = c_realloc(cp, i, &size);
		if (nulcnv && !c) c = '\n';
		cp[i] = c;
	}
#ifdef	USECRNL
	if (i > 0 && cp[i - 1] == '\r') i--;
#endif
	cp[i++] = '\0';
	return(realloc2(cp, i));
}
