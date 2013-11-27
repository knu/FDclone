/*
 *	libc.c
 *
 *	arrangememt of library functions
 */

#include "fd.h"
#include "log.h"
#include "realpath.h"
#include "parse.h"
#include "kconv.h"
#include "func.h"
#include "kanji.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	DEP_PTY
#include "termemu.h"
#endif
#ifdef	DEP_URLPATH
#include "url.h"
#endif
#ifdef	CYGWIN
#include <sys/cygwin.h>
#endif

#if	MSDOS
extern char *unixrealpath __P_((CONST char *, char *));
#endif

#ifndef	DEP_ORIGSHELL
extern char **environ;
#endif
extern char fullpath[];
extern char *origpath;
extern int hideclock;

#ifndef	DEP_ORIGSHELL
static int NEAR _getenv2 __P_((CONST char *, int, char **));
static char **NEAR _putenv2 __P_((char *, char **));
#endif

#ifdef	PWNEEDERROR
char Error[1024];
#endif
#ifndef	DEP_ORIGSHELL
char **environ2 = NULL;
#endif
int physical_path = 0;
#ifdef	DEP_PSEUDOPATH
char *unixpath = NULL;
#endif
int lostcount = 0;

static char *lastpath = NULL;
static int wasttyflags = 0;


int stat2(path, stp)
CONST char *path;
struct stat *stp;
{
#ifdef	NOSYMLINK
	return(Xstat(path, stp));
#else	/* !NOSYMLINK */
	int duperrno;

	if (Xstat(path, stp) < 0) {
		duperrno = errno;
# ifdef	DEP_DOSDRIVE
		if (dospath2(path)) {
			errno = duperrno;
			return(-1);
		}
# endif
# ifdef	DEP_URLPATH
		if (urlpath(path, NULL, NULL, NULL)) {
			errno = duperrno;
			return(-1);
		}
# endif
		if (reallstat(path, stp) < 0 || !s_islnk(stp)) {
			errno = duperrno;
			return(-1);
		}
		stp -> st_mode &= ~S_IFMT;
	}

	return(0);
#endif	/* !NOSYMLINK */
}

int _chdir2(path)
CONST char *path;
{
#if	!MSDOS
	int fd, duperrno;
#endif
#ifdef	CYGWIN
	char *cygdrive, tmp[MAXPATHLEN];
	int len;
#endif
	char cwd[MAXPATHLEN];

	if (!Xgetwd(cwd)) copyrootpath(cwd);
	if (Xchdir(path) < 0) return(-1);
#if	!MSDOS
# ifdef	DEP_DOSDRIVE
	if (dospath2(nullstr)) /*EMPTY*/;
	else
# endif
# ifdef	DEP_URLPATH
	if (urlpath(nullstr, NULL, NULL, NULL)) /*EMPTY*/;
	else
# endif
	if ((fd = open(curpath, O_RDONLY, 0666)) >= 0) VOID_C close(fd);
	else {
# ifdef	CYGWIN
		if (Xgetwd(tmp)) {
			len = strlen(tmp);
			cygdrive = getcygdrive_user();
			if ((*cygdrive && !strnpathcmp(tmp, cygdrive, len)
			&& (!cygdrive[len] || cygdrive[len] == _SC_)))
				return(0);
			cygdrive = getcygdrive_system();
			if ((*cygdrive && !strnpathcmp(tmp, cygdrive, len)
			&& (!cygdrive[len] || cygdrive[len] == _SC_)))
				return(0);
		}
# endif	/* CYGWIN */
		duperrno = errno;
		if (Xchdir(cwd) < 0) {
			lostcwd(cwd);
			lostcount++;
		}
		errno = duperrno;
		return(-1);
	}
#endif	/* !MSDOS */

	return(0);
}

int chdir2(path)
CONST char *path;
{
	char cwd[MAXPATHLEN], tmp[MAXPATHLEN];
	int duperrno;

#ifdef	DEBUG
	if (!path) {
		Xfree(lastpath);
		lastpath = NULL;
# ifdef	DEP_PSEUDOPATH
		Xfree(unixpath);
		unixpath = NULL;
# endif
		return(0);
	}
#endif	/* DEBUG */

	Xrealpath(path, tmp, (physical_path) ? RLP_READLINK : 0);
	Xstrcpy(cwd, fullpath);
	Xstrcpy(fullpath, tmp);

	if (_chdir2(fullpath) < 0) {
		duperrno = errno;
		if (_chdir2(cwd) < 0) {
			lostcwd(fullpath);
			lostcount++;
		}
		else Xstrcpy(fullpath, cwd);
		errno = duperrno;
		return(-1);
	}
	Xfree(lastpath);
	lastpath = Xstrdup(cwd);
#ifdef	DEP_PSEUDOPATH
	switch (checkdrv(lastdrv, NULL)) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			if (!unixpath) unixpath = Xstrdup(cwd);
			if (Xgetwd(cwd)) Xstrcpy(fullpath, cwd);
			Xrealpath(fullpath, fullpath, 0);
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
			if (!unixpath) unixpath = Xstrdup(cwd);
			break;
# endif
		default:
			Xfree(unixpath);
			unixpath = NULL;
			break;
	}
#endif	/* DEP_PSEUDOPATH */
	if (getconstvar(ENVPWD)) setenv2(ENVPWD, fullpath, 1);
#if	MSDOS
	if (unixrealpath(fullpath, tmp)) Xstrcpy(fullpath, tmp);
#endif
	VOID_C entryhist(fullpath, HST_PATH | HST_UNIQ);
#ifdef	DEP_PTY
	sendparent(TE_CHDIR, fullpath);
#endif

	return(0);
}

int chdir3(path, raw)
CONST char *path;
int raw;
{
#ifdef	DEP_DOSDRIVE
	int drive;
#endif
	CONST char *cwd;

	cwd = path;
	if (!raw && path[0] && !path[1]) switch (path[0]) {
		case '.':
			cwd = NULL;
			break;
		case '?':
			path = origpath;
			break;
		case '-':
			if (!lastpath) return(seterrno(ENOENT));
			path = lastpath;
			break;
#ifdef	DEP_PSEUDOPATH
		case '@':
			if (!unixpath) return(seterrno(ENOENT));
			path = unixpath;
			break;
#endif
		default:
			break;
	}
#ifdef	DEP_DOSDRIVE
	if ((drive = dospath3(fullpath))) flushdrv(drive, NULL);
#endif
	if (chdir2(path) < 0) return(-1);
	if (!cwd && !Xgetwd(fullpath)) lostcwd(fullpath);
#ifndef	_NOUSEHASH
	else VOID_C searchhash(NULL, nullstr, nullstr);
#endif

	return(0);
}

int chdir4(path, raw, arcf)
CONST char *path;
int raw;
CONST char *arcf;
{
	char *eol, dupfullpath[MAXPATHLEN];
	CONST char *cp;

#ifndef	_NOARCHIVE
	if (!arcf || getpathtop(path, NULL, NULL)) /*EMPTY*/;
	else if (!path || *path != _SC_) {
		if (!(cp = archchdir(path))) return(-1);

		if (cp != (char *)-1) {
			if (*cp) setlastfile(cp);
		}
		else {
			setlastfile(arcf);
			escapearch(0);
		}

		return(0);
	}
#endif	/* !_NOARCHIVE */

	if (!path) return(0);
	Xstrcpy(dupfullpath, fullpath);
	if (chdir3(path, raw) < 0) return(-1);

	if (!filelist) return(0);
	else if (!(cp = underpath(dupfullpath, fullpath, -1)))
		setlastfile(parentpath);
	else if (!*(cp++)) return(0);
	else {
		if ((eol = strdelim(cp, 0))) *eol = '\0';
		setlastfile(cp);
	}

	Xfree(findpattern);
	findpattern = NULL;
#ifndef	_NOARCHIVE
	if (archivefile) {
		Xstrcpy(dupfullpath, fullpath);
		while (archivefile) escapearch(1);
		Xstrcpy(fullpath, dupfullpath);
	}
#endif	/* !_NOARCHIVE */

	return(0);
}

int mkdir2(path, mode)
char *path;
int mode;
{
	char *cp1, *cp2, *eol;

	eol = &(path[(int)strlen(path) - 1]);
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

/*
 *	strncpy2(buf, s, &(x), n): like as sprintf(buf + n, "%-*.*s", x, x, s);
 *	strncpy2(buf, s, &(-x), n): like as sprintf(buf + n, "%s", s);
 */
int strncpy2(s1, s2, lenp, ptr)
char *s1;
CONST char *s2;
int *lenp, ptr;
{
	int i, j, r, v;

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

	v = Xsnprintf(&(s1[i]), *lenp * KANAWID + 1 - i,
		"%^.*s", *lenp - i, &(s2[j]));
#ifdef	CODEEUC
	r = strlen(&(s1[i]));
#else
	r = v;
#endif
	v += i;
	r += i;

	if (ptr >= 0) {
		*lenp += r - v;
		for (i = r; i < *lenp; i++) s1[i] = ' ';
		s1[i] = '\0';
	}

	return(r);
}

VOID perror2(s)
CONST char *s;
{
	int duperrno;

	duperrno = errno;
	if (s) VOID_C Xfprintf(Xstderr, "%k: ", s);
	Xfputs(Xstrerror(duperrno), Xstderr);
	if (isttyiomode) Xfputc('\r', Xstderr);
	VOID_C fputnl(Xstderr);
}

#ifndef	DEP_ORIGSHELL
static int NEAR _getenv2(name, len, envp)
CONST char *name;
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

	if ((cp = Xstrchr(s, '='))) len = (int)(cp - s);
	else len = strlen(s);

	if ((n = _getenv2(s, len, envp)) < 0) n = 0;
	else if (envp[n]) {
		Xfree(envp[n]);
		if (cp) envp[n] = s;
		else for (i = n; envp[i]; i++) envp[i] = envp[i + 1];
		return(envp);
	}
	if (!cp) return(envp);

	new = (char **)Xrealloc(envp, (n + 2) * sizeof(char *));
	new[n] = s;
	new[n + 1] = (char *)NULL;

	return(new);
}
#endif	/* !DEP_ORIGSHELL */

char *getenv2(name)
CONST char *name;
{
#ifdef	DEP_ORIGSHELL
	char *cp;
#else
	char **envpp[2];
	int i, n, len;
#endif

#ifdef	DEP_ORIGSHELL
	if ((cp = getshellvar(name, -1))) return(cp);
	if (!strnenvcmp(name, FDENV, FDESIZ)
	&& (cp = getshellvar(&(name[FDESIZ]), -1)))
		return(cp);
#else	/* !DEP_ORIGSHELL */
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
#endif	/* !DEP_ORIGSHELL */

	return(NULL);
}

int setenv2(name, value, export)
CONST char *name, *value;
int export;
{
	char *cp;
	int len;

	len = strlen(name);
	if (!value) {
#ifdef	DEP_ORIGSHELL
		return(unset(name, len));
#else
		cp = (char *)name;
#endif
	}
	else {
		cp = Xmalloc(len + strlen(value) + 2);
		memcpy(cp, name, len);
#if	defined (ENVNOCASE) && !defined (DEP_ORIGSHELL)
		Xstrntoupper(cp, len);
#endif
		cp[len] = '=';
		Xstrcpy(&(cp[len + 1]), value);
	}
#ifdef	DEP_ORIGSHELL
	if (((export) ? putexportvar(cp, len) : putshellvar(cp, len)) < 0) {
		Xfree(cp);
		return(-1);
	}
#else	/* !DEP_ORIGSHELL */
	if (export) environ = _putenv2(cp, environ);
	else environ2 = _putenv2(cp, environ2);
	evalenv(name, len);
# ifdef	DEP_PTY
	sendparent(TE_PUTSHELLVAR, name, value, export);
# endif
#endif	/* !DEP_ORIGSHELL */

	return(0);
}

#ifdef	USESIGACTION
sigcst_t signal2(sig, func)
int sig;
sigcst_t func;
{
	struct sigaction act, oact;

	act.sa_handler = func;
# ifdef	SA_INTERRUPT
	act.sa_flags = SA_INTERRUPT;
# else
	act.sa_flags = 0;
# endif
	sigemptyset(&(act.sa_mask));
	sigemptyset(&(oact.sa_mask));
	if (sigaction(sig, &act, &oact) < 0) return(SIG_ERR);

	return(oact.sa_handler);
}
#endif	/* USESIGACTION */

int system2(command, flags)
CONST char *command;
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

#ifdef	DEP_FILECONV
	if (!(flags & F_NOKANJICONV)) printf_defkanji++;
#endif
	ret = dosystem(command);
#ifdef	DEP_FILECONV
	if (!(flags & F_NOKANJICONV)) printf_defkanji--;
#endif

	LOG1(_LOG_NOTICE_, ret, "system(\"%s\");", command);
#ifndef	DEP_ORIGSHELL
	checkscreen(-1, -1);
#endif
	sigvecset(n);
	if (ret >= 127 && (flags & F_NOCONFIRM)) {
		if (dumbterm <= 2) Xfputc('\007', Xstderr);
		VOID_C Xfprintf(Xstderr, "\n%k", HITKY_K);
		Xfflush(Xstderr);
		Xttyiomode(1);
		keyflush();
		getkey3(0, inputkcode, 0);
		Xstdiomode();
		VOID_C fputnl(Xstderr);
	}

	if (wastty) {
		Xttyiomode(wastty - 1);
		if (n && (flags & F_NOCONFIRM)) VOID_C Xtermmode(mode);
		if (!(flags & (F_NOCONFIRM | F_ISARCH))
		|| ((flags & F_ISARCH) && ret >= 127)) {
			hideclock = 1;
			warning(0, HITKY_K);
		}
	}

	return(ret);
}

XFILE *popen2(command, flags)
CONST char *command;
int flags;
{
	XFILE *fp;
	int n;

	if (!command || !*command) return(NULL);
	n = sigvecset(0);
	wasttyflags = 0;
	if (isttyiomode) {
		wasttyflags |= F_TTYIOMODE;
		if (isttyiomode > 1) wasttyflags |= F_TTYNL;
		Xstdiomode();
	}

#ifdef	DEP_FILECONV
	if (!(flags & F_NOKANJICONV)) printf_defkanji++;
#endif
	fp = dopopen(command);
#ifdef	DEP_FILECONV
	if (!(flags & F_NOKANJICONV)) printf_defkanji--;
#endif

	sigvecset(n);
	if (fp) {
		if (wasttyflags & F_TTYIOMODE) {
			Xputterm(T_KEYPAD);
			Xtflush();
		}
	}
	else {
		if (dumbterm <= 2) Xfputc('\007', Xstderr);
		VOID_C fputnl(Xstderr);
		perror2(command);
		Xfflush(Xstderr);
		Xttyiomode(1);
		keyflush();
		getkey3(0, inputkcode, 0);
		Xstdiomode();
		VOID_C fputnl(Xstderr);
		if (wasttyflags & F_TTYIOMODE)
			Xttyiomode((wasttyflags & F_TTYNL) ? 1 : 0);
	}

	return(fp);
}

int pclose2(fp)
XFILE *fp;
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

	return(Xstrdup(cwd));
}
