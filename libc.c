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
extern int setcurdrv __P_((int));
extern char *unixrealpath __P_((char *, char *));
#else
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/param.h>
#endif

#ifdef	PWNEEDERROR
char Error[1024];
#endif

extern char **environ;
extern char fullpath[];
extern char *origpath;
extern char *findpattern;

#ifndef	CHAR_BIT
# ifdef	NBBY
# define	CHAR_BIT	NBBY
# else
# define	CHAR_BIT	0x8
# endif
#endif
#define	char2long(cp)	(  ((u_char *)cp)[3] \
			| (((u_char *)cp)[2] << (CHAR_BIT * 1)) \
			| (((u_char *)cp)[1] << (CHAR_BIT * 2)) \
			| (((u_char *)cp)[0] << (CHAR_BIT * 3)) )

static char *_realpath2 __P_((char *, char *));
static int _getenv2 __P_((char *, int, char **));
static char **_putenv2 __P_((char *, char **));
#if	!MSDOS && !defined (NOTZFILEH)\
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static int tmcmp __P_((struct tm *, struct tm *));
#endif
#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long gettimezone __P_((struct tm *, time_t));
#endif

static char **environ2 = NULL;
static char *lastpath = NULL;
#ifndef	_NODOSDRIVE
static char *unixpath = NULL;
#endif


int rename2(from, to)
char *from, *to;
{
	if (!strpathcmp(from, to)) return(0);
	return(Xrename(from, to));
}

int stat2(path, buf)
char *path;
struct stat *buf;
{
#if	MSDOS
	return(Xstat(path, buf));
#else	/* !MSDOS */
	int tmperr;

	if (Xstat(path, buf) < 0) {
#ifndef	_NODOSDRIVE
		if (dospath2(path)) return(-1);
#endif
		tmperr = errno;
		if (lstat(path, buf) < 0
		|| (buf -> st_mode & S_IFMT) != S_IFLNK) {
			errno = tmperr;
			return(-1);
		}
		buf -> st_mode &= ~S_IFMT;
	}
	return(0);
#endif	/* !MSDOS */
}

static char *_realpath2(path, resolved)
char *path, *resolved;
{
	char *cp;

	if (!*path || !strcmp(path, ".")) return(resolved);
	else if ((cp = strdelim(path, 0))) {
		*cp = '\0';
		_realpath2(path, resolved);
		*(cp++) = _SC_;
		_realpath2(cp, resolved);
		return(resolved);
	}

	if (!strcmp(path, "..")) {
		cp = strrdelim(resolved, 0);
#if	MSDOS
		if (cp && cp != &resolved[2]) *cp = '\0';
		else resolved[3] = '\0';
#else	/* !MSDOS */
#ifndef	_NODOSDRIVE
		if (_dospath(resolved)) {
			if (cp && cp != &resolved[2]) *cp = '\0';
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
		strcpy(cp, path);
#if	!MSDOS
		if (!_dospath(resolved)) {
			struct stat status;
			char buf[MAXPATHLEN + 1];
			int i;

			if (lstat(resolved, &status) >= 0
			&& (status.st_mode & S_IFMT) == S_IFLNK
			&& (i = readlink(resolved, buf, MAXPATHLEN)) >= 0) {
				buf[i] = '\0';
				if (*buf == _SC_) strcpy(resolved, buf);
				else {
					*cp = '\0';
					_realpath2(buf, resolved);
				}
			}
		}
#endif
	}
	return(resolved);
}

char *realpath2(path, resolved)
char *path, *resolved;
{
	char tmp[MAXPATHLEN + 1];
	int drv;

	if (path == resolved) {
		strcpy(tmp, path);
		path = tmp;
	}

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
		if (setcurdrv(resolved[0]) < 0) {
			resolved[1] = ':';
			resolved[2] = _SC_;
			resolved[3] = '\0';
		}
		else if (!Xgetcwd(resolved, MAXPATHLEN) || setcurdrv(drv) < 0)
			error(NULL);
	}
#else	/* !MSDOS */
	if (*path == _SC_) strcpy(resolved, _SS_);
# ifndef	_NODOSDRIVE
	else if ((drv = _dospath(path))) {
		path += 2;
		resolved[0] = drv;
		resolved[drv = 1] = ':';
		if (*path == _SC_) resolved[++drv] = _SC_;
		resolved[++drv] = '\0';
	}
# endif
#endif	/* !MSDOS */
	else if (resolved != fullpath) strcpy(resolved, fullpath);
	return(_realpath2(path, resolved));
}

int _chdir2(path)
char *path;
{
	char cwd[MAXPATHLEN + 1];

	if (!Xgetcwd(cwd, MAXPATHLEN)) strcpy(cwd, _SS_);
	if (Xchdir(path) < 0) return(-1);
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (!dospath2("")) {
		int fd;

		if ((fd = open(".", O_RDONLY, 0600)) < 0) {
			if (Xchdir(cwd) < 0) error(cwd);
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
	char *pwd, cwd[MAXPATHLEN + 1], tmp[MAXPATHLEN + 1];

	realpath2(path, tmp);
	if (_chdir2(path) < 0) return(-1);

	strcpy(cwd, fullpath);
	strcpy(fullpath, tmp);

	if (_chdir2(fullpath) < 0) {
		if (_chdir2(cwd) < 0) error(cwd);
		strcpy(fullpath, cwd);
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
		if (Xgetcwd(cwd, MAXPATHLEN)) strcpy(fullpath, cwd);
		realpath2(fullpath, fullpath);
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
		pwd = (char *)malloc2(strlen(fullpath) + 4 + 1);
		strcpy(pwd, "PWD=");
		strcpy(pwd + 4, fullpath);
		if (putenv2(pwd) < 0) error("PWD");
	}
#if	MSDOS
	unixrealpath(fullpath, tmp);
	strcpy(fullpath, tmp);
#endif
	entryhist(1, fullpath, 1);
	return(0);
}

char *chdir3(path)
char *path;
{
	char *cwd;

	cwd = path;
	if (!strcmp(path, ".")) cwd = NULL;
	else if (!strcmp(path, "?")) path = origpath;
	else if (!strcmp(path, "-")) {
		if (!lastpath) return(".");
		path = lastpath;
	}
#ifndef	_NODOSDRIVE
	else if (!strcmp(path, "@")) {
		if (!unixpath) return(".");
		path = unixpath;
	}
#endif
	if (chdir2(path) < 0) return(NULL);
	if (!cwd) {
		cwd = getwd2();
		strcpy(fullpath, cwd);
		free(cwd);
	}
	else {
		if (findpattern) free(findpattern);
		findpattern = NULL;
	}
	return(path);
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

VOID_P malloc2(size)
ALLOC_T size;
{
	VOID_P tmp;

	if (!size) error(NULL);
	if (!(tmp = (VOID_P)malloc(size))) error(NULL);
	return(tmp);
}

VOID_P realloc2(ptr, size)
VOID_P ptr;
ALLOC_T size;
{
	VOID_P tmp;

	if (!size) error(NULL);
	if (!ptr) return(malloc2(size));
	if (!(tmp = (VOID_P)realloc(ptr, size))) error(NULL);
	return(tmp);
}

char *strdup2(str)
char *str;
{
	char *tmp;

	if (!str) return(NULL);
	if (!(tmp = (char *)malloc((ALLOC_T)strlen(str) + 1))) error(NULL);
	strcpy(tmp, str);
	return(tmp);
}

int toupper2(c)
int c;
{
	return((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
}

char *strchr2(s, c)
char *s;
int c;
{
	int i;

	for (i = 0; s[i]; i++) {
		if (s[i] == c) return(&s[i]);
		if (iskanji1(s[i]) && !s[++i]) break;
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
		if (s[i] == c) cp = &s[i];
		if (iskanji1(s[i]) && !s[++i]) break;
	}
	return(cp);
}

char *strncpy2(s1, s2, n)
char *s1;
char *s2;
int n;
{
	strncpy(s1, s2, n);
	s1[n] = '\0';
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
		if (iskanji1(s2[j])) {
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
		else
#endif
		if (iskanji1(s2[j])) {
			if (*lenp >= 0 && i >= *lenp - 1) {
				s1[i++] = ' ';
				break;
			}
			s1[i++] = s2[j++];
		}
		else if ((u_char)(s2[j]) < ' ' || s2[j] == C_DEL) {
			s1[i++] = '^';
			if (*lenp >= 0 && i >= *lenp) break;
			s1[i++] = ((s2[j++] + '@') & 0x7f);
			continue;
		}
		s1[i++] = s2[j++];
	}
	l = i;
	if (*lenp >= 0 && ptr >= 0) while (i < *lenp) s1[i++] = ' ';
	s1[i] = '\0';
	return(l);
}

int strcasecmp2(s1, s2)
char *s1, *s2;
{
	int c1, c2;

	while ((c1 = toupper2(*s1)) == (c2 = toupper2(*(s2++))))
		if (!*(s1++)) return(0);
	return(c1 - c2);
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

int strlen2(str)
char *str;
{
	int i, len;

	for (i = len = 0; str[i]; i++, len++)
		if ((u_char)(str[i]) < ' ' || str[i] == C_DEL) len++;
	return(len);
}

int strlen3(str)
char *str;
{
	int i, len;

	for (i = len = 0; str[i]; i++, len++) {
#ifdef	CODEEUC
		if (isekana(str, i)) i++;
		else
#endif
		if ((u_char)(str[i]) < ' ' || str[i] == C_DEL) len++;
	}
	return(len);
}

int atoi2(str)
char *str;
{
	return((str && *str >= '0' && *str <= '9') ? atoi(str) : -1);
}

static int _getenv2(name, len, envp)
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

static char **_putenv2(str, envp)
char *str, **envp;
{
	char *cp, *tmp, **new;
	int i, n, len;

	if ((cp = strchr(str, '='))) len = (int)(cp - str);
	else len = strlen(str);

	if ((n = _getenv2(str, len, envp)) < 0) n = 0;
	else if (envp[n]) {
		tmp = envp[n];
		if (cp) envp[n] = str;
		else for (i = n; envp[i]; i++) envp[i] = envp[i + 1];
		free(tmp);
		return(envp);
	}
	if (!cp) return(envp);

#ifdef	DEBUG
	_mtrace_file = "putenv";
#endif
	if (!envp) new = (char **)malloc((n + 2) * sizeof(char *));
	else new = (char **)realloc(envp, (n + 2) * sizeof(char *));
	if (!new) {
		free(str);
		return(NULL);
	}
	new[n] = str;
	new[n + 1] = (char *)NULL;
	return(new);
}

int putenv2(str)
char *str;
{
	char **new;
#if	MSDOS
	char *cp;

	for (cp = str; *cp && *cp != '='; cp++) *cp = toupper2(*cp);
#endif	/* !MSDOS */
	if (!(new = _putenv2(str, environ))) return(-1);
	environ = new;
	return(0);
}

char *getenv2(name)
char *name;
{
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
	return(NULL);
}

int setenv2(name, value)
char *name, *value;
{
	char *cp, **new;
	int len;
#if	MSDOS
	int i;
#endif

	if (!value) cp = name;
	else {
		len = strlen(name);
		cp = (char *)malloc2(len + strlen(value) + 2);
		memcpy(cp, name, len);
#if	MSDOS
		for (i = 0; i < len ; i++) cp[i] = toupper2(cp[i]);
#endif
		cp[len++] = '=';
		strcpy(&(cp[len]), value);
	}
	if (!(new = _putenv2(cp, environ2))) return(-1);
	environ2 = new;
	return(0);
}

#ifdef	DEBUG
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
#endif

/*ARGSUSED*/
int printenv(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, n;

	if (argc >= 3) return(-1);
	if (!comline) return(0);
	if (argc == 2) {
		n = _getenv2(argv[1], strlen(argv[1]), environ2);
		if (n >= 0 && environ2[n]) cprintf2("%s\r\n", environ2[n]);
		return(1);
	}
	if (!environ2) return(0);
	for (i = n = 0; environ2[i]; i++) {
		cprintf2("%s\r\n", environ2[i]);
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	return(n);
}

int system2(command, noconf)
char *command;
int noconf;
{
	int status;

	if (noconf >= 0) {
		locate(0, n_line - 1);
		putterm(l_clear);
		tflush();
		if (noconf) putterms(t_end);
		putterms(t_nokeypad);
		tflush();
	}
	cooked2();
	echo2();
	nl2();
	tabs();
	status = system(command);
	raw2();
	noecho2();
	nonl2();
	notabs();
	if (status > 127 || !noconf) warning(0, HITKY_K);
	if (noconf >= 0) {
		if (noconf) putterms(t_init);
		putterms(t_keypad);
	}
	return(status);
}

char *getwd2(VOID_A)
{
	char cwd[MAXPATHLEN + 1];

	if (!Xgetcwd(cwd, MAXPATHLEN)) error(NULL);
	return(strdup2(cwd));
}

#if	!MSDOS
char *getpwuid2(uid)
uid_t uid;
{
	static strtable *uidlist = NULL;
	static int maxuid = 0;
	struct passwd *pwd;
	int i;

	for (i = 0; i < maxuid; i++)
		if (uid == uidlist[i].no) return(uidlist[i].str);

#ifdef	DEBUG
	_mtrace_file = "getpwuid";
#endif
	if ((pwd = getpwuid(uid))) {
#ifdef	DEBUG
		_mtrace_file = "getpwuid2(1)";
#endif
		uidlist = b_realloc(uidlist, maxuid, strtable);
		uidlist[maxuid].no = pwd -> pw_uid;
#ifdef	DEBUG
		_mtrace_file = "getpwuid2(2)";
#endif
		uidlist[maxuid].str = strdup2(pwd -> pw_name);
		return(uidlist[maxuid++].str);
	}
	return(NULL);
}

char *getgrgid2(gid)
gid_t gid;
{
	static strtable *gidlist = NULL;
	static int maxgid = 0;
	struct group *grp;
	int i;

	for (i = 0; i < maxgid; i++)
		if (gid == gidlist[i].no) return(gidlist[i].str);

#ifdef	DEBUG
	_mtrace_file = "getgrgid";
#endif
	if ((grp = getgrgid(gid))) {
#ifdef	DEBUG
		_mtrace_file = "getgrgid2(1)";
#endif
		gidlist = b_realloc(gidlist, maxgid, strtable);
		gidlist[maxgid].no = grp -> gr_gid;
#ifdef	DEBUG
		_mtrace_file = "getgrgid2(2)";
#endif
		gidlist[maxgid].str = strdup2(grp -> gr_name);
		return(gidlist[maxgid++].str);
	}
	return(NULL);
}
#endif

#if	!MSDOS && !defined (NOTZFILEH)\
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static int tmcmp(tm1, tm2)
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
static long gettimezone(tm, t)
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
	char *cp, buf[MAXPATHLEN + 1];
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
	else {
		strcpy(buf, TZDIR);
		strcpy(strcatdelim(buf), cp);
	}
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

char *fgets2(fp)
FILE *fp;
{
	char *cp;
	long i, size;
	int c;

	cp = (char *)malloc2((size = BUFUNIT));

	for (i = 0; (c = Xfgetc(fp)) != '\n'; i++) {
		if (c == EOF) {
			if (!i || ferror(fp)) {
				free(cp);
				return(NULL);
			}
			break;
		}
		if (i + 1 >= size) {
			if (size < (1L << (BITSPERBYTE * sizeof(long) - 2)))
				cp = (char *)realloc2(cp, (size *= 2));
			else {
				free(cp);
				errno = ENOMEM;
				error(NULL);
			}
		}
		cp[i] = (c) ? c : '\n';
	}
	cp[i++] = '\0';
	return(realloc2(cp, i));
}
