/*
 *	libc.c
 *
 *	arrangememt of library functions
 */

#include <fcntl.h>
#include "fd.h"
#include "func.h"
#include "kanji.h"

#ifndef	NOTZFILEH
#include <tzfile.h>
#endif

#if	MSDOS
#include <sys/timeb.h>
extern int setcurdrv __P_((int, int));
extern char *unixrealpath __P_((char *, char *));
#endif

#ifdef	_NOORIGSHELL
#include "wait.h"
#else
#include "system.h"
#endif

#ifndef	_NOPTY
#include "termemu.h"
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
static long NEAR char2long __P_((u_char *));
static int NEAR tmcmp __P_((struct tm *, struct tm *));
#endif
#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static time_t NEAR timegm2 __P_((struct tm *));
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
		if (nodoslstat(path, stp) < 0 || !s_islnk(stp)) {
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
	if (!s_islnk(&st)) {
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
	int n;

	if (!*path || (n = isdotdir(path)) == 2) return(resolved);
	else if ((cp = strdelim(path, 0))) {
		*cp = '\0';
		_realpath2(path, resolved, rdlink);
		*(cp++) = _SC_;
		_realpath2(cp, resolved, rdlink);
		return(resolved);
	}

	if (n == 1) {
		cp = strrdelim(resolved, 0);
		top = resolved;
#ifdef	_USEDOSEMU
		if (_dospath(resolved)) top += 2;
		else
#endif
#ifdef	DOUBLESLASH
		if ((n = isdslash(resolved)) && cp < &(resolved[n])) {
			top = &(resolved[n - 1]);
			if (*top != _SC_) return(resolved);
		}
		else
#endif
#if	MSDOS
		top += 2;
#else
		if (rdlink && evallink(resolved, cp) > 0)
			return(_realpath2(path, resolved, rdlink));
#endif
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
#ifdef	_USEDOSEMU
	char *cp;
	int duplastdrive;
#endif
#ifdef	_USEDOSPATH
	int drv;
#endif
#ifdef	DOUBLESLASH
	int n;
#endif
#if	MSDOS
	int drive;
#endif
	char tmp[MAXPATHLEN];

	strcpy(tmp, path);
	path = tmp;

#ifdef	_USEDOSEMU
	if ((drv = _dospath(path))) {
		path += 2;
		if (*path == _SC_) cp = NULL;
		else {
			duplastdrive = lastdrive;
			lastdrive = drv;
			cp = dosgetcwd(resolved, MAXPATHLEN - 1);
			lastdrive = duplastdrive;
		}
		if (!cp) VOID_C gendospath(resolved, drv, _SC_);
	}
	else
#endif
#ifdef	DOUBLESLASH
	if ((n = isdslash(path))) {
		strncpy2(resolved, path, n);
		path += n;
		if (*path == _SC_) path++;
	}
	else
#endif
	{
#if	MSDOS
		drv = dospath(nullstr, NULL);
		if ((drive = _dospath(path))) path += 2;
		if (*path == _SC_) {
			if (!drive) drive = drv;
			VOID_C gendospath(resolved, drive, _SC_);
		}
		else if (drive && drive != drv) {
			if (setcurdrv(drive, 0) < 0)
				VOID_C gendospath(resolved, drive, _SC_);
			else {
				if (!Xgetwd(resolved)) lostcwd(resolved);
				if (setcurdrv(drv, 0) < 0) error("setcurdrv()");
			}
		}
#else	/* !MSDOS */
		if (*path == _SC_) copyrootpath(resolved);
#endif	/* !MSDOS */
		else if (!rdlink && resolved != fullpath && *fullpath)
			strcpy(resolved, fullpath);
		else if (!Xgetwd(resolved)) copyrootpath(resolved);
	}

	norealpath++;
	_realpath2(path, resolved, rdlink);
	norealpath--;

	return(resolved);
}

int _chdir2(path)
char *path;
{
	char cwd[MAXPATHLEN];

	if (!Xgetwd(cwd)) copyrootpath(cwd);
	if (Xchdir(path) < 0) return(-1);
#if	!MSDOS
# ifndef	_NODOSDRIVE
	if (dospath2(nullstr)) /*EMPTY*/;
	else
# endif
	{
		int fd, duperrno;

		if ((fd = open(curpath, O_RDONLY, 0666)) < 0) {
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
#endif	/* DEBUG */

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
	if (getconstvar(ENVPWD)) setenv2(ENVPWD, fullpath, 1);
#if	MSDOS
	if (unixrealpath(fullpath, tmp)) strcpy(fullpath, tmp);
#endif
	entryhist(1, fullpath, 1);
#ifndef	_NOPTY
	sendparent(TE_CHDIR, fullpath);
#endif

	return(0);
}

int chdir3(path, raw)
char *path;
int raw;
{
#ifndef	_NODOSDRIVE
	int drive;
#endif
	char *cwd;

	cwd = path;
	if (!raw && path[0] && !path[1]) switch (path[0]) {
		case '.':
			cwd = NULL;
			break;
		case '?':
			path = origpath;
			break;
		case '-':
			if (!lastpath) {
				errno = ENOENT;
				return(-1);
			}
			path = lastpath;
			break;
#ifndef	_NODOSDRIVE
		case '@':
			if (!unixpath) {
				errno = ENOENT;
				return(-1);
			}
			path = unixpath;
			break;
#endif
		default:
			break;
	}
#ifndef	_NODOSDRIVE
	if ((drive = dospath3(fullpath))) flushdrv(drive, NULL);
#endif
	if (chdir2(path) < 0) return(-1);
	if (!cwd) {
		if (!Xgetwd(fullpath)) lostcwd(fullpath);
	}
	else {
		if (findpattern) free(findpattern);
		findpattern = NULL;
#ifndef	_NOUSEHASH
		searchhash(NULL, nullstr, nullstr);
#endif
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
#ifdef	CODEEUC
		else if (isekana(s, i)) i++;
#endif
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
#ifdef	CODEEUC
		else if (isekana(s, i)) i++;
#endif
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
	int i, j, len;

	for (i = j = 0; i < ptr && s2[j]; i++, j++) {
		if (iskanji1(s2, j)) {
			i++;
			j++;
		}
#ifdef	CODEEUC
		else if (isekana(s2, j)) j++;
#endif
	}
	if (!i || i <= ptr) i = 0;
	else {
		s1[0] = ' ';
		i = 1;
	}

	while (i < *lenp && s2[j]) {
		if (iskanji1(s2, j)) {
			i += snprintf2(&(s1[i]), *lenp - i + 1,
				"%.2s", &(s2[j]));
			j += 2;
			continue;
		}
#ifdef	CODEEUC
		else if (isekana(s2, j)) {
			(*lenp)++;
			s1[i++] = s2[j++];
		}
#else
		else if (isskana(s2, j)) /*EMPTY*/;
#endif
		else if (iscntrl2(s2[j])) {
			i += snprintf2(&(s1[i]), *lenp - i + 1,
				"^%c", (s2[j++] + '@') & 0x7f);
			continue;
		}
		else if (ismsb(s2[j])) {
			i += snprintf2(&(s1[i]), *lenp - i + 1,
				"\\%03o", s2[j++] & 0xff);
			continue;
		}
		s1[i++] = s2[j++];
	}

	len = i;
	if (ptr >= 0) while (i < *lenp) s1[i++] = ' ';
	s1[i] = '\0';

	return(len);
}

#ifdef	CODEEUC
int strlen2(s)
char *s;
{
	int i, len;

	for (i = len = 0; s[i]; i++, len++) if (isekana(s, i)) i++;

	return(len);
}
#endif	/* CODEEUC */

int strlen3(s)
char *s;
{
	int i, len;

	for (i = len = 0; s[i]; i++, len++) {
		if (iskanji1(s, i)) {
			i++;
			len++;
		}
#ifdef	CODEEUC
		else if (isekana(s, i)) i++;
#else
		else if (isskana(s, i)) /*EMPTY*/;
#endif
		else if (iscntrl2(s[i])) len++;
		else if (ismsb(s[i])) len += 3;
	}

	return(len);
}

int atoi2(s)
char *s;
{
	int n;

	if (!sscanf2(s, "%<d%$", &n)) return(-1);

	return(n);
}

#ifdef	USESTDARGH
/*VARARGS1*/
char *asprintf3(CONST char *fmt, ...)
#else
/*VARARGS1*/
char *asprintf3(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *cp;
	int n;

	VA_START(args, fmt);
	n = vasprintf2(&cp, fmt, args);
	va_end(args);
	if (n < 0) error("malloc()");

	return(cp);
}

VOID perror2(s)
char *s;
{
	int duperrno;

	duperrno = errno;
	if (s) fprintf2(stderr, "%k: ", s);
	fputs(strerror2(duperrno), stderr);
	if (isttyiomode) fputc('\r', stderr);
	fputnl(stderr);
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
	evalenv(name, len);
# ifndef	_NOPTY
	sendparent(TE_PUTSHELLVAR, name, value, export);
# endif
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

int system2(command, flags)
char *command;
int flags;
{
	int n, wastty, mode, ret;

	if (!command || !*command) return(0);
#ifdef	FAKEUNINIT
	mode = 0;		/* fake for -Wuninitialized */
#endif
	n = sigvecset(0);
	if ((wastty = isttyiomode)) {
		if (!(flags & F_ISARCH)) {
			Xlocate(0, n_line - 1);
			Xputterm(L_CLEAR);
		}
		if (n && (flags & F_NOCONFIRM)) mode = Xtermmode(0);
		Xstdiomode();
	}

	ret = dosystem(command);
	LOG1(_LOG_NOTICE_, ret, "system(\"%s\");", command);
#ifdef	_NOORIGSHELL
	checkscreen(-1, -1);
#endif
	sigvecset(n);
	if (ret >= 127 && (flags & F_NOCONFIRM)) {
		if (dumbterm <= 2) fputc('\007', stderr);
		fprintf2(stderr, "\n%k", HITKY_K);
		fflush(stderr);
		Xttyiomode(1);
		keyflush();
		getkey3(0, inputkcode);
		Xstdiomode();
		fputnl(stderr);
	}

	if (wastty) {
		Xttyiomode(wastty - 1);
		if (n && (flags & F_NOCONFIRM)) Xtermmode(mode);
		if (!(flags & (F_NOCONFIRM | F_ISARCH))
		|| ((flags & F_ISARCH) && ret >= 127)) {
			hideclock = 1;
			warning(0, HITKY_K);
		}
	}

	return(ret);
}

FILE *popen2(command)
char *command;
{
	FILE *fp;
	int n;

	if (!command || !*command) return(NULL);
	n = sigvecset(0);
	wasttyflags = 0;
	if (isttyiomode) {
		wasttyflags |= F_TTYIOMODE;
		if (isttyiomode > 1) wasttyflags |= F_TTYNL;
		Xstdiomode();
	}

	fp = dopopen(command);
	sigvecset(n);
	if (fp) {
		if (wasttyflags & F_TTYIOMODE) {
			Xputterm(T_KEYPAD);
			Xtflush();
		}
	}
	else {
		if (dumbterm <= 2) fputc('\007', stderr);
		fputnl(stderr);
		perror2(command);
		fflush(stderr);
		Xttyiomode(1);
		keyflush();
		getkey3(0, inputkcode);
		Xstdiomode();
		fputnl(stderr);
		if (wasttyflags & F_TTYIOMODE)
			Xttyiomode((wasttyflags & F_TTYNL) ? 1 : 0);
	}

	return(fp);
}

int pclose2(fp)
FILE *fp;
{
	int ret;

	ret = dopclose(fp);
	if (wasttyflags & F_TTYIOMODE)
		Xttyiomode((wasttyflags & F_TTYNL) ? 1 : 0);

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
static long NEAR char2long(s)
u_char *s;
{
	return((long)((u_long)(s[3])
		| ((u_long)(s[2]) << (BITSPERBYTE * 1))
		| ((u_long)(s[1]) << (BITSPERBYTE * 2))
		| ((u_long)(s[0]) << (BITSPERBYTE * 3))));
}

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
static time_t NEAR timegm2(tm)
struct tm *tm;
{
	time_t t;
	int i, y;

	y = (tm -> tm_year < 1900) ? tm -> tm_year + 1900 : tm -> tm_year;

	t = ((long)y - 1970) * 365;
	t += ((y - 1 - 1968) / 4)
		- ((y - 1 - 1900) / 100)
		+ ((y - 1 - 1600) / 400);
	for (i = 1; i < tm -> tm_mon + 1; i++) switch (i) {
		case 2:
			if (!(y % 4) && ((y % 100) || !(y % 400))) t++;
			t += 28;
			break;
		case 4:
		case 6:
		case 9:
		case 11:
			t += 30;
			break;
		default:
			t += 31;
			break;
	}
	t += tm -> tm_mday - 1;
	t *= 60L * 60L * 24L;
	t += ((long)(tm -> tm_hour) * 60L + tm -> tm_min) * 60L + tm -> tm_sec;

	return(t);
}

static long NEAR gettimezone(tm, t)
struct tm *tm;
time_t t;
{
# if	MSDOS
	struct timeb buffer;

	ftime(&buffer);
	return((long)(buffer.timezone) * 60L);
# else	/* !MSDOS */
#  ifndef	NOTZFILEH
	struct tzhead head;
	FILE *fp;
	time_t tmp;
	long i, leap, nleap, ntime, ntype, nchar;
	char *cp, buf[MAXPATHLEN];
	u_char c;
#  endif
	struct tm tmbuf;
	long tz;

	memcpy((char *)&tmbuf, (char *)tm, sizeof(struct tm));

#  ifdef	NOTMGMTOFF
	tz = (long)t - (long)timegm2(localtime(&t));
#  else
	tz = -(localtime(&t) -> tm_gmtoff);
#  endif

#  ifndef	NOTZFILEH
	cp = getconstvar("TZ");
	if (!cp || !*cp) cp = TZDEFAULT;
	if (cp[0] == _SC_) strcpy(buf, cp);
	else strcatdelim2(buf, TZDIR, cp);
	if (!(fp = fopen(buf, "rb"))) return(tz);
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
		i *= (int)sizeof(char);
		i += (int)sizeof(struct tzhead) + ntime * 4 * sizeof(char);
		if (fseek(fp, i, 0) < 0
		|| fread(&c, sizeof(char), 1, fp) != 1) {
			fclose(fp);
			return(tz);
		}
		i = c;
	}
	i *= (4 + 1 + 1) * sizeof(char);
	i += (int)sizeof(struct tzhead) + ntime * (4 + 1) * sizeof(char);
	if (fseek(fp, i, 0) < 0
	|| fread(buf, sizeof(char), 4, fp) != 4) {
		fclose(fp);
		return(tz);
	}
	tmp = char2long(buf);
	tz = -tmp;

	i = (int)sizeof(struct tzhead) + ntime * (4 + 1) * sizeof(char)
		+ ntype * (4 + 1 + 1) * sizeof(char)
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
	time_t t;

	t = timegm2(tm);
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
