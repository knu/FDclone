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

#ifdef	_NOORIGSHELL
#define	dosystem(s)		system(s)
#else
#include "system.h"
#endif

#ifndef	NOTZFILEH
#include <tzfile.h>
#endif

#if	MSDOS
#include <sys/timeb.h>
extern int setcurdrv __P_((int, int));
extern char *unixrealpath __P_((char *, char *));
#else
#include <sys/file.h>
#include <sys/param.h>
#endif
#ifndef	_NODOSDRIVE
extern int flushdrv __P_((int, VOID_T (*)__P_((VOID_A))));
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
static char *lastpath = NULL;
#ifndef	_NODOSDRIVE
static char *unixpath = NULL;
#endif


int stat2(path, stp)
char *path;
struct stat *stp;
{
#if	MSDOS
	return(Xstat(path, stp));
#else	/* !MSDOS */
	int tmperr;

	if (Xstat(path, stp) < 0) {
#ifndef	_NODOSDRIVE
		if (dospath2(path)) return(-1);
#endif
		tmperr = errno;
		if (_Xlstat(path, stp) < 0
		|| (stp -> st_mode & S_IFMT) != S_IFLNK) {
			errno = tmperr;
			return(-1);
		}
		stp -> st_mode &= ~S_IFMT;
	}
	return(0);
#endif	/* !MSDOS */
}

/*ARGSUSED*/
static char *NEAR _realpath2(path, resolved, rdlink)
char *path, *resolved;
int rdlink;
{
	char *cp;

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
#if	MSDOS
		if (cp && cp != &(resolved[2])) *cp = '\0';
		else resolved[3] = '\0';
#else	/* !MSDOS */
#ifndef	_NODOSDRIVE
		if (_dospath(resolved)) {
			if (cp && cp != &(resolved[2])) *cp = '\0';
			else resolved[3] = '\0';
		}
		else
#endif
		{
			if (cp && cp != resolved) *cp = '\0';
			else resolved[1] = '\0';
		}
#endif	/* !MSDOS */
	}
	else {
		cp = strcatdelim(resolved);
		strncpy2(cp, path, MAXPATHLEN - 1 - (cp - resolved));
#if	!MSDOS
		if (!rdlink);
# ifndef	_NODOSDRIVE
		else if (_dospath(resolved));
# endif
		else {
			struct stat st;
			char buf[MAXPATHLEN];
			int i;

			if (_Xlstat(resolved, &st) >= 0
			&& (st.st_mode & S_IFMT) == S_IFLNK
			&& (i = readlink(resolved, buf, MAXPATHLEN - 1)) >= 0)
			{
				buf[i] = '\0';
				if (*buf == _SC_) strcpy(resolved, buf);
				else {
					if (cp - 1 > resolved) cp--;
					*cp = '\0';
					_realpath2(buf, resolved, rdlink);
				}
			}
		}
#endif
	}
	return(resolved);
}

char *realpath2(path, resolved, rdlink)
char *path, *resolved;
int rdlink;
{
	char tmp[MAXPATHLEN];
	int drv;

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
		else if (!Xgetwd(resolved) || setcurdrv(drv, 0) < 0)
			error(NULL);
	}
#else	/* !MSDOS */
	if (*path == _SC_) strcpy(resolved, _SS_);
# ifndef	_NODOSDRIVE
	else if ((drv = _dospath(path))) {
		char cwd[MAXPATHLEN];

		path += 2;
		resolved[0] = drv;
		resolved[1] = ':';
		resolved[2] = '\0';
		if (*path == _SC_ || !Xgetwd(cwd)
		|| Xchdir(resolved) < 0 || !Xgetwd(resolved)) {
			resolved[2] = _SC_;
			resolved[3] = '\0';
		}
		else if (Xchdir(cwd) < 0) error(cwd);
	}
# endif
#endif	/* !MSDOS */
	else if (!*fullpath) {
		if (!Xgetwd(resolved)) strcpy(resolved, _SS_);
	}
	else if (resolved != fullpath) strcpy(resolved, fullpath);
	return(_realpath2(path, resolved, rdlink));
}

int _chdir2(path)
char *path;
{
	char cwd[MAXPATHLEN];

	if (!Xgetwd(cwd)) strcpy(cwd, _SS_);
	if (Xchdir(path) < 0) return(-1);
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (!dospath2("")) {
		int fd;

		if ((fd = open(".", O_RDONLY, 0600)) < 0) {
			if (Xchdir(cwd) < 0) lostcwd(cwd);
			return(-1);
		}
		close(fd);
	}
#endif
	return(0);
}

int chdir2(path)
char *path;
{
	char *pwd, cwd[MAXPATHLEN], tmp[MAXPATHLEN];

	realpath2(path, tmp, 0);
	if (_chdir2(path) < 0) return(-1);

	strcpy(cwd, fullpath);
	strcpy(fullpath, tmp);

	if (_chdir2(fullpath) < 0) {
		if (_chdir2(cwd) < 0) lostcwd(fullpath);
		else strcpy(fullpath, cwd);
		return(-1);
	}
	if (lastpath) free(lastpath);
#ifdef	DEBUG
	_mtrace_file = "chdir2";
#endif
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
	if (getenv("PWD")) {
#ifdef	DEBUG
		_mtrace_file = "chdir2(PWD)";
#endif
		pwd = malloc2(strlen(fullpath) + 4 + 1);
		strcpy(strcpy2(pwd, "PWD="), fullpath);
		if (putenv2(pwd) < 0) error("PWD");
	}
#if	MSDOS
	unixrealpath(fullpath, tmp);
	strcpy(fullpath, tmp);
#endif
	entryhist(1, fullpath, 1);
	return(0);
}

int chdir3(path)
char *path;
{
	char *cwd;
#ifndef	_NODOSDRIVE
	int drive;
#endif

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
#if	MSDOS
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

	if (!size || !(tmp = (char *)malloc(size))) error(NULL);
	return(tmp);
}

char *realloc2(ptr, size)
VOID_P ptr;
ALLOC_T size;
{
	char *tmp;

	if (!size
	|| !(tmp = (ptr) ? (char *)realloc(ptr, size) : (char *)malloc(size)))
		error(NULL);
	return(tmp);
}

char *strdup2(s)
char *s;
{
	char *tmp;

	if (!s) return(NULL);
	if (!(tmp = (char *)malloc((ALLOC_T)strlen(s) + 1))) error(NULL);
	strcpy(tmp, s);
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
	return(s1);
}

int strncpy3(s1, s2, lenp, ptr)
char *s1, *s2;
int *lenp, ptr;
{
	int i, j, l;

	for (i = j = 0; i < ptr; i++, j++) {
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
		if (iskna(s2[j]));
#endif
		else if (iskanji1(s2, j)) {
			if (*lenp >= 0 && i >= *lenp - 1) {
				s1[i++] = ' ';
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

char *strstr2(s1, s2)
char *s1, *s2;
{
	char *cp;
	int len;

	len = strlen(s2);
	for (cp = s1; (cp = strchr(cp, *s2)); cp++)
		if (!strncmp(cp, s2, len)) return(cp);
	return(NULL);
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

#ifdef	_NOORIGSHELL
static int NEAR _getenv2(name, len, envp)
char *name;
int len;
char **envp;
{
	int i;

	if (!envp) return(-1);

	for (i = 0; envp[i]; i++)
		if (!strnpathcmp(envp[i], name, len) && envp[i][len] == '=')
			break;
	return(i);
}

static char **NEAR _putenv2(s, envp)
char *s, **envp;
{
	char *cp, *tmp, **new;
	int i, n, len;

	if ((cp = strchr(s, '='))) len = (int)(cp - s);
	else len = strlen(s);

	if ((n = _getenv2(s, len, envp)) < 0) n = 0;
	else if (envp[n]) {
		tmp = envp[n];
		if (cp) envp[n] = s;
		else for (i = n; envp[i]; i++) envp[i] = envp[i + 1];
		free(tmp);
		return(envp);
	}
	if (!cp) return(envp);

# ifdef	DEBUG
	_mtrace_file = "_putenv2";
# endif
	if (!envp) new = (char **)malloc((n + 2) * sizeof(char *));
	else new = (char **)realloc(envp, (n + 2) * sizeof(char *));
	if (!new) {
		free(s);
		return(NULL);
	}
	new[n] = s;
	new[n + 1] = (char *)NULL;
	return(new);
}
#endif	/* _NOORIGSHELL */

int putenv2(s)
char *s;
{
#ifdef	_NOORIGSHELL
	char **new;
# if	MSDOS
	char *cp;

	for (cp = s; *cp && *cp != '='; cp++) *cp = toupper2(*cp);
# endif
	if (!(new = _putenv2(s, environ))) return(-1);
	environ = new;
	return(0);
#else	/* !_NOORIGSHELL */
	return(putexportvar(s, -1));
#endif	/* !_NOORIGSHELL */
}

char *getenv2(name)
char *name;
{
#ifdef	_NOORIGSHELL
	char **envpp[2];
	int i, c, n, len;

	len = strlen(name);
	c = strnpathcmp(name, "FD_", 3);
	envpp[0] = environ2;
	envpp[1] = environ;

	for (i = 0; i < 2; i++) {
		n = _getenv2(name, len, envpp[i]);
		if (n >= 0 && envpp[i][n]) return(&(envpp[i][n][len + 1]));
		if (c) continue;
		n = _getenv2(name + 3, len - 3, envpp[i]);
		if (n >= 0 && envpp[i][n]) return(&(envpp[i][n][len - 3 + 1]));
	}
#else	/* !_NOORIGSHELL */
	char *cp;

	if ((cp = getshellvar(name, -1))) return(cp);
	if (!strnpathcmp(name, "FD_", 3) && (cp = getshellvar(name + 3, -1)))
		return(cp);
#endif	/* !_NOORIGSHELL */
	return(NULL);
}

int setenv2(name, value)
char *name, *value;
{
	char *cp;
	int len;
#ifdef	_NOORIGSHELL
	char **new;
# if	MSDOS
	int i;
# endif
#endif

	len = strlen(name);
	if (!value) cp = name;
	else {
		cp = malloc2(len + strlen(value) + 2);
		memcpy(cp, name, len);
#if	MSDOS && defined (_NOORIGSHELL)
		for (i = 0; i < len ; i++) cp[i] = toupper2(cp[i]);
#endif
		cp[len] = '=';
		strcpy(&(cp[len + 1]), value);
	}
#ifdef	_NOORIGSHELL
	if (!(new = _putenv2(cp, environ2))) return(-1);
	environ2 = new;
#else	/* !_NOORIGSHELL */
	if (putshellvar(cp, len) < 0) {
		free(cp);
		return(-1);
	}
#endif	/* !_NOORIGSHELL */
	return(0);
}

#if	defined (DEBUG) && defined (_NOORIGSHELL)
VOID freeenv(VOID_A)
{
	int i;

	if (environ) {
		for (i = 0; environ[i]; i++) free(environ[i]);
		free(environ);
	}
	if (environ2) {
		for (i = 0; environ2[i]; i++) free(environ2[i]);
		free(environ2);
	}
}
#endif	/* DEBUG && _NOORIGSHELL */

int system2(command, noconf)
char *command;
int noconf;
{
	int n, ret;

	if (!command || !*command) return(0);
	n = sigvecset(0);
	if (noconf >= 0) {
		locate(0, n_line - 1);
		putterm(l_clear);
		if (n && noconf) putterms(t_end);
	}
	stdiomode();
	ret = dosystem(command);
	if (noconf > 0) {
		if (ret >= 127) {
			fputs("\007\n", stderr);
			kanjifputs(HITKY_K, stderr);
			fflush(stderr);
			ttyiomode();
			keyflush();
			getkey2(0);
			stdiomode();
			fputc('\n', stderr);
		}
		if (n) putterms(t_init);
	}
	ttyiomode();
	sigvecset(n);
	if (!noconf || (noconf < 0 && ret >= 127)) {
		hideclock = 1;
		warning(0, HITKY_K);
	}
	return(ret);
}

char *getwd2(VOID_A)
{
	char cwd[MAXPATHLEN];

	if (!Xgetwd(cwd)) error(NULL);
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
#if	MSDOS
	struct timeb buffer;

	ftime(&buffer);
	return((long)(buffer.timezone) * 60L);
#else	/* !MSDOS */
#ifdef	NOTMGMTOFF
	struct timeval t_val;
	struct timezone t_zone;
#endif
#ifndef	NOTZFILEH
	struct tzhead head;
#endif
	struct tm tmbuf;
	FILE *fp;
	time_t tmp;
	long i, tz, leap, nleap, ntime, ntype, nchar;
	char *cp, buf[MAXPATHLEN];
	u_char c;

	memcpy((char *)&tmbuf, (char *)tm, sizeof(struct tm));

#ifdef	NOTMGMTOFF
	gettimeofday2(&t_val, &t_zone);
	tz = t_zone.tz_minuteswest * 60L;
#else
	tz = -(localtime(&t) -> tm_gmtoff);
#endif

#ifndef	NOTZFILEH
	cp = (char *)getenv("TZ");
	if (!cp || !*cp) cp = TZDEFAULT;
	if (cp[0] == _SC_) strcpy(buf, cp);
	else strcatdelim2(buf, TZDIR, cp);
	if (!(fp = fopen(buf, "r"))) return(tz);
	if (fread(&head, sizeof(struct tzhead), 1, fp) != 1) {
		fclose(fp);
		return(tz);
	}
#ifdef	USELEAPCNT
	nleap = char2long(head.tzh_leapcnt);
#else
	nleap = char2long(head.tzh_timecnt - 4);
#endif
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
#endif	/* !NOTZFILEH */

	return(tz);
#endif	/* !MSDOS */
}
#endif	/* !USEMKTIME && !USETIMELOCAL */

time_t timelocal2(tm)
struct tm *tm;
{
#ifdef	USEMKTIME
	tm -> tm_isdst = -1;
	return(mktime(tm));
#else
# ifdef	USETIMELOCAL
	return(timelocal(tm));
# else
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
# endif
#endif
}

char *fgets2(fp, nulcnv)
FILE *fp;
int nulcnv;
{
	char *cp;
	long i, size;
	int c;

	cp = c_malloc(size);
	for (i = 0; (c = Xfgetc(fp)) != '\n'; i++) {
		if (c == EOF) {
			if (!i || ferror(fp)) {
				free(cp);
				return(NULL);
			}
			break;
		}
		cp = c_realloc(cp, i, size);
		if (nulcnv && !c) c = '\n';
		cp[i] = c;
	}
#if	MSDOS
	if (i > 0 && cp[i - 1] == '\r') i--;
#endif
	cp[i++] = '\0';
	return(realloc2(cp, i));
}
