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

extern int copypolicy;
extern char fullpath[];
extern char *origpath;
extern char *findpattern;

#define	BUFUNIT		32
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
static assoclist *_getenv2 __P_((char *));
#if	!MSDOS && !defined (NOTZFILEH)\
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static int tmcmp __P_((struct tm *, struct tm *));
#endif
#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long gettimezone __P_((struct tm *, time_t));
#endif

static assoclist *environ2 = NULL;
static char *lastpath = NULL;
#if	!MSDOS && !defined (_NODOSDRIVE)
static char *unixpath = NULL;
#endif


int access2(path, mode)
char *path;
int mode;
{
	char *cp, *str[4];
	int val[4];
#if	MSDOS

	if (Xaccess(path, mode) >= 0) return(0);
#else	/* !MSDOS */
	struct stat *statp, status;

# ifndef	_NODOSDRIVE
	if (dospath(path, NULL)) {
		if (Xaccess(path, mode) >= 0) return(0);
		statp = NULL;
	}
	else
# endif
	{
		char *name, dir[MAXPATHLEN + 1];

		if (lstat(path, &status) < 0) {
			warning(-1, path);
			return(-1);
		}
		if ((status.st_mode & S_IFMT) == S_IFLNK) return(0);

		if ((name = strrchr(path, _SC_))) {
			if (name == path) strcpy(dir, _SS_);
			else strncpy2(dir, path, name - path);
			name++;
		}
		else {
			strcpy(dir, ".");
			name = path;
		}

		if (lstat(dir, &status) < 0) {
			warning(-1, dir);
			return(-1);
		}
		statp = &status;
		if (access(path, mode) >= 0) return(0);
	}
#endif	/* !MSDOS */
	if (errno == ENOENT) return(0);
	if (errno != EACCES) {
		warning(-1, path);
		return(-1);
	}
#if	!MSDOS
	if (statp && statp -> st_uid != geteuid()) return(-1);
#endif
	if (copypolicy > 0) return(copypolicy - 2);
	locate(0, LCMDLINE);
	putterm(l_clear);
	putch2('[');
	cp = DELPM_K;
	kanjiputs2(path, n_column - (int)strlen(cp) - 1, -1);
	kanjiputs(cp);
	str[0] = ANYES_K;
	str[1] = ANNO_K;
	str[2] = ANALL_K;
	str[3] = ANKEP_K;
	val[0] = 0;
	val[1] = -1;
	val[2] = 2;
	val[3] = 1;
	if (selectstr(&copypolicy, 4, 0, str, val) == ESC) copypolicy = 2;
	return((copypolicy > 0) ? copypolicy - 2 : copypolicy);
}

int unlink2(path)
char *path;
{
	if (access2(path, W_OK) < 0) return(1);
	return(Xunlink(path));
}

int rmdir2(path)
char *path;
{
	if (access2(path, R_OK | W_OK | X_OK) < 0) return(1);
	return(Xrmdir(path));
}

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
		if (dospath(path, NULL)) return(-1);
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
	else if ((cp = strchr(path, _SC_))) {
		*cp = '\0';
		_realpath2(path, resolved);
		*(cp++) = _SC_;
		_realpath2(cp, resolved);
		return(resolved);
	}

	if (!strcmp(path, "..")) {
		cp = strrchr(resolved, _SC_);
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
		if (*resolved && resolved[(int)strlen(resolved) - 1] != _SC_)
			strcat(resolved, _SS_);
		strcat(resolved, path);
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
		setcurdrv(resolved[0]);
		if (!Xgetcwd(resolved, MAXPATHLEN)) error(NULL);
		setcurdrv(drv);
	}
#else	/* !MSDOS */
	if (*path == _SC_) strcpy(resolved, _SS_);
# ifndef	_NODOSDRIVE
	else if ((drv = _dospath(path))) {
		path += 2;
		resolved[0] = drv;
		resolved[1] = ':';
		resolved[2] = '\0';
		if (*path == _SC_) strcat(resolved, _SS_);
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
#if	!MSDOS
	int fd;
#endif

	if (!Xgetcwd(cwd, MAXPATHLEN)) strcpy(cwd, _SS_);
	if (Xchdir(path) < 0) return(-1);
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (!dospath("", NULL)) {
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
#ifndef	USESETENV
	char *pwd;
#endif
	char cwd[MAXPATHLEN + 1], tmp[MAXPATHLEN + 1];

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
	lastpath = strdup2(cwd);
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (_dospath(fullpath)) {
		if (!_dospath(cwd)) {
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
#ifdef	USESETENV
		if (setenv("PWD", fullpath, 1) < 0) error("PWD");
#else
		pwd = (char *)malloc2(strlen(fullpath) + 4 + 1);
		strcpy(pwd, "PWD=");
		strcpy(pwd + 4, fullpath);
		if (putenv2(pwd) < 0) error("PWD");
#endif
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
#if	!MSDOS && !defined (_NODOSDRIVE)
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

	for (eol = path + (int)strlen(path) - 1; eol > path; eol--) {
		if (*eol != _SC_) break;
		*eol = '\0';
	}

	cp1 = ++eol;
	cp2 = strrchr(path, _SC_);
	for (;;) {
		if (Xmkdir(path, mode) >= 0) break;
		if (errno != ENOENT || !cp2 || cp2 <= path) return(-1);
		*cp2 = '\0';
		if (cp1 < eol) *cp1 = _SC_;
		cp1 = cp2;
		cp2 = strrchr(path, _SC_);
	}

	while (cp1 && cp1 < eol) {
		cp2 = strchr(cp1 + 1, _SC_);
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

VOID_P addlist(array, i, max, size)
VOID_P array;
int i, *max, size;
{
	VOID_P tmp;

	if (i >= *max) {
		*max = ((i + 1) / BUFUNIT + 1) * BUFUNIT;
		tmp = realloc2(array, (long)size * *max);
	}
	else tmp = array;
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
	int i, len;

	len = strlen(s);
	for (i = 0; i < len; i++) {
		if (s[i] == c) return(&s[i]);
		if (iskanji1(s[i])) i++;
	}
	return(NULL);
}

char *strrchr2(s, c)
char *s;
int c;
{
	int i, len;
	char *cp;

	cp = NULL;
	len = strlen(s);
	for (i = 0; i < len; i++) {
		if (s[i] == c) cp = &s[i];
		if (iskanji1(s[i])) i++;
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
			(*lenp)++;
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

#ifndef	USESETENV
int putenv2(str)
char *str;
{
#if	MSDOS
	char *cp;

	if (!(cp = strchr(str, '='))) return(0);
	while (--cp >= str) *cp = toupper2(*cp);
	return(putenv(str));
#else	/* !MSDOS */
	extern char **environ;
	static char **newenvp = NULL;
	static int envsize = 0;
	char *cp, *tmp;
	int i, j, e, n, size;

	if ((cp = strchr(str, '='))) i = (int)(cp - str);
	else i = strlen(str);
	e = -1;
	for (n = 0; environ[n]; n++)
		if (e < 0 && !strnpathcmp(environ[n], str, i)
		&& environ[n][i] == '=') e = n;
	if (e < 0) e = n++;

	if (e < envsize) {
		tmp = environ[e];
		if (cp) environ[e] = str;
		else for (i = e; environ[i]; i++) environ[i] = environ[i + 1];
		if (tmp) free(tmp);
		return(0);
	}

	size = ((n + 1) / BUFUNIT + 1) * BUFUNIT;
	if (!newenvp) {
		if (!(newenvp = (char **)malloc(size * sizeof(char *)))) {
			free(str);
			return(-1);
		}
		for (i = j = 0; i < n; i++) {
			if (i != e) newenvp[j++] = strdup2(environ[i]);
			else if (cp) newenvp[j++] = str;
		}
	}
	else {
		if (!(newenvp = (char **)realloc(newenvp,
		size * sizeof(char *)))) {
			newenvp = environ;
			free(str);
			return(-1);
		}
		if (cp) newenvp[e] = str;
		j = ++e;
	}
	while (j < size) newenvp[j++] = (char *)NULL;
	environ = newenvp;
	envsize = size;
	return(0);
#endif	/* !MSDOS */
}
#endif	/* !USESETENV */

static assoclist *_getenv2(name)
char *name;
{
	assoclist *ap;

	for (ap = environ2; ap; ap = ap -> next)
		if (!strpathcmp(name, ap -> org)) return(ap);
	if (!strnpathcmp(name, "FD_", 3))
	for (ap = environ2; ap; ap = ap -> next)
		if (!strpathcmp(name + 3, ap -> org)) return(ap);
	return(NULL);
}

int setenv2(name, value, overwrite)
char *name, *value;
int overwrite;
{
#if	MSDOS
	char *cp;
#endif
	assoclist *ap, **tmp;

	if ((ap = _getenv2(name))) {
		if (!overwrite) return(0);
		free(ap -> assoc);
	}
	else {
		if (!value) return(0);
		ap = (assoclist *)malloc2(sizeof(assoclist));
		ap -> org = strdup2(name);
#if	MSDOS
		for (cp = ap -> org; cp && *cp; cp++) *cp = toupper2(*cp);
#endif
		ap -> next = environ2;
		environ2 = ap;
	}

	if (value) ap -> assoc = (*value) ? strdup2(value) : NULL;
	else {
		free(ap -> org);
		for (tmp = &environ2; *tmp; tmp = &((*tmp) -> next))
			if (*tmp == ap) break;
		if (*tmp) {
			*tmp = ap -> next;
			free(ap);
		}
	}
	return(0);
}

char *getenv2(name)
char *name;
{
	assoclist *ap;
	char *cp;

	if ((ap = _getenv2(name))) return(ap -> assoc);
	else if ((cp = (char *)getenv(name))) return(cp);
	return(strnpathcmp(name, "FD_", 3) ? NULL : (char *)getenv(name + 3));
}

/*ARGSUSED*/
int printenv(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	assoclist *ap;
	int n;

	if (argc >= 3) return(-1);
	if (!comline) return(0);
	n = 1;
	if (argc <= 1) for (ap = environ2, n = 0; ap; ap = ap -> next) {
		cprintf2("%s=%s\r\n", ap -> org,
			(ap -> assoc) ? ap -> assoc : "");
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	else for (ap = environ2; ap; ap = ap -> next)
	if (!strpathcmp(argv[1], ap -> org)) {
		cprintf2("%s=%s\r\n", ap -> org,
			(ap -> assoc) ? ap -> assoc : "");
		break;
	}
	return(ap != environ2 ? n : 1);
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
	static int maxuidbuf = 0;
	static int maxuid = 0;
	struct passwd *pwd;
	int i;

	for (i = 0; i < maxuid; i++)
		if (uid == uidlist[i].no) return(uidlist[i].str);

	if ((pwd = getpwuid(uid))) {
		uidlist = (strtable *)addlist(uidlist, i,
			&maxuidbuf, sizeof(strtable));
		uidlist[i].no = pwd -> pw_uid;
		uidlist[i].str = strdup2(pwd -> pw_name);
		maxuid++;
		return(uidlist[i].str);
	}
	return(NULL);
}

char *getgrgid2(gid)
gid_t gid;
{
	static strtable *gidlist = NULL;
	static int maxgidbuf = 0;
	static int maxgid = 0;
	struct group *grp;
	int i;

	for (i = 0; i < maxgid; i++)
		if (gid == gidlist[i].no) return(gidlist[i].str);

	if ((grp = getgrgid(gid))) {
		gidlist = (strtable *)addlist(gidlist, i,
			&maxgidbuf, sizeof(strtable));
		gidlist[i].no = grp -> gr_gid;
		gidlist[i].str = strdup2(grp -> gr_name);
		maxgid++;
		return(gidlist[i].str);
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
	return(buffer.timezone);
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

	memcpy(&tmbuf, tm, sizeof(struct tm));

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
		strcat(buf, _SS_);
		strcat(buf, cp);
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

	d = (y - 1970) * 365;
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
	t = (tm -> tm_hour * 60 + tm -> tm_min) * 60 + tm -> tm_sec;
	t += d * 60 * 60 * 24;
	t += gettimezone(tm, t);

	return(t);
# endif
#endif
}
