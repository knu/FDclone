/*
 *	libc.c
 *
 *	Arrangememt of Library Function
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"

#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>

#ifdef	USETIMEH
#include <time.h>
#endif

#ifndef	NOTZFILEH
#include <tzfile.h>
#endif

#ifdef	USEGETWD
#define	Xgetcwd(buf, size)	Xgetwd(buf)
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

static char *_realpath2();
static assoclist *_getenv2();
static int tmcmp();
static long gettimezone();

static assoclist *environ2 = NULL;
static char *lastpath = NULL;
static char *unixpath = NULL;


int access2(path, mode)
char *path;
int mode;
{
	struct stat status;
	char *cp, *name, dir[MAXPATHLEN + 1], *str[4];
	int val[4];

	if (dospath(path, NULL)) {
		if (Xaccess(path, mode) < 0) {
			warning(-1, path);
			return(-1);
		}
		return(0);
	}
	if (lstat(path, &status) < 0) {
		warning(-1, path);
		return(-1);
	}
	if ((status.st_mode & S_IFMT) == S_IFLNK) return(0);

	if ((name = strrchr(path, '/'))) {
		if (name == path) strcpy(dir, "/");
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
	if (access(path, mode) < 0) {
		if (errno == ENOENT) return(0);
		if (errno != EACCES) {
			warning(-1, path);
			return(-1);
		}
		if (status.st_uid == getuid()) {
			if (copypolicy > 0) return(copypolicy - 2);
			locate(0, LCMDLINE);
			putterm(l_clear);
			putch('[');
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
			if (selectstr(&copypolicy, 4, 0, str, val) == ESC)
				copypolicy = 2;
			return((copypolicy > 0) ? copypolicy - 2 : copypolicy);
		}
	}
	return(0);
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
	if (!strcmp(from, to)) return(0);
	return(Xrename(from, to));
}

int stat2(path, buf)
char *path;
struct stat *buf;
{
	int tmperr;

	if (Xstat(path, buf) < 0) {
		if (dospath(path, NULL)) return(-1);
		tmperr = errno;
		if (lstat(path, buf) < 0
		|| (buf -> st_mode & S_IFMT) != S_IFLNK) {
			errno = tmperr;
			return(-1);
		}
		buf -> st_mode &= ~S_IFMT;
	}
	return(0);
}

static char *_realpath2(path, resolved)
char *path, *resolved;
{
	char *cp;
	int drive;

	if (!*path || !strcmp(path, ".")) return(resolved);
	else if (cp = strchr(path, '/')) {
		*cp = '\0';
		_realpath2(path, resolved);
		*(cp++) = '/';
		_realpath2(cp, resolved);
		return(resolved);
	}

	if (!strcmp(path, "..")) {
		cp = strrchr(resolved, '/');
		if (drive = _dospath(resolved)) {
			if (cp && cp != &resolved[2]) *cp = '\0';
			else sprintf(resolved, "%c:/", drive);
		}
		else {
			if (cp && cp != resolved) *cp = '\0';
			else strcpy(resolved, "/");
		}
	}
	else {
		if (*resolved && resolved[(int)strlen(resolved) - 1] != '/')
			strcat(resolved, "/");
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

	if (*path == '/') strcpy(resolved, "/");
	else if (drv = _dospath(path)) {
		sprintf(resolved, "%c:/", drv);
		path += 2;
	}
	else if (resolved != fullpath) strcpy(resolved, fullpath);
	return(_realpath2(path, resolved));
}

int _chdir2(path)
char *path;
{
	char cwd[MAXPATHLEN + 1];
	int fd;

	if (!Xgetcwd(cwd, MAXPATHLEN)) strcpy(cwd, "/");
	if (Xchdir(path) < 0) return(-1);
	if (!dospath("", NULL)) {
		if ((fd = open(".", O_RDONLY, 0600)) < 0) {
			if (Xchdir(cwd) < 0) error(cwd);
			return(-1);
		}
		close(fd);
	}
	return(0);
}

int chdir2(path)
char *path;
{
#ifndef	USESETENV
	static char pwd[4 + MAXPATHLEN + 1];
#endif
	char cwd[MAXPATHLEN + 1];

	if (_chdir2(path) < 0) return(-1);

	strcpy(cwd, fullpath);
	realpath2(path, fullpath);

	if (_chdir2(fullpath) < 0) {
		if (_chdir2(cwd) < 0) error(cwd);
		strcpy(fullpath, cwd);
		return(-1);
	}
	if (lastpath) free(lastpath);
	lastpath = strdup2(cwd);
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

	if (getenv("PWD")) {
#ifdef	USESETENV
		setenv("PWD", fullpath, 1);
#else
		strcpy(pwd, "PWD=");
		strcpy(pwd + 4, fullpath);
		putenv2(pwd);
#endif
	}
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
	else if (!strcmp(path, "@")) {
		if (!unixpath) return(".");
		path = unixpath;
	}
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
		if (*eol != '/') break;
		*eol = '\0';
	}

	cp1 = ++eol;
	cp2 = strrchr(path, '/');
	for (;;) {
		if (Xmkdir(path, mode) >= 0) break;
		if (errno != ENOENT || !cp2 || cp2 <= path) return(-1);
		*cp2 = '\0';
		if (cp1 < eol) *cp1 = '/';
		cp1 = cp2;
		cp2 = strrchr(path, '/');
	}

	while (cp1 && cp1 < eol) {
		cp2 = strchr(cp1 + 1, '/');
		*cp1 = '/';
		if (cp2) *cp2 = '\0';
		if (Xmkdir(path, mode) < 0 && errno != EEXIST) return(-1);
		cp1 = cp2;
	}
	return(0);
}

VOID_P malloc2(size)
unsigned size;
{
	VOID_P tmp;

	if (!(tmp = (VOID_P)malloc(size))) error(NULL);
	return(tmp);
}

VOID_P realloc2(ptr, size)
VOID_P ptr;
unsigned size;
{
	VOID_P tmp;

	if (!ptr) return(malloc2(size));
	if (!(tmp = (VOID_P)realloc(ptr, size))) error(NULL);
	return(tmp);
}

char *strdup2(str)
char *str;
{
	char *tmp;

	if (!str) return(NULL);
	if (!(tmp = (char *)malloc(strlen(str) + 1))) error(NULL);
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
		tmp = realloc2(array, *max * size);
	}
	else tmp = array;
	return(tmp);
}

int toupper2(c)
int c;
{
	return(islower(c) ? c - 'a' + 'A' : c);
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

int strncpy3(s1, s2, len, ptr)
char *s1, *s2;
int len, ptr;
{
	char *cp;
	int i, l;

	cp = s1;
	l = len;
	if (ptr && onkanji1(s2, ptr - 1)) {
		*(cp++) = ' ';
		ptr++;
		l--;
	}
	if (l < strlen(s2 + ptr) && onkanji1(s2, ptr + l - 1)) {
		strncpy2(cp, s2 + ptr, --l);
		strcat(cp, " ");
		len--;
	}
	else {
		for (i = 0; i < l && s2[ptr + i]; i++) *(cp++) = s2[ptr + i];
		len = i;
		for (; i < l; i++) *(cp++) = ' ';
		*cp = '\0';
	}
	return(len);
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
	for (cp = s1; cp = strchr(cp, *s2); cp++)
		if (!strncmp(cp, s2, len)) return(cp);
	return(NULL);
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
	extern char **environ;
	static char **newenvp = NULL;
	char *cp, **envp;
	int i;

	if (cp = strchr(str, '=')) cp++;
	else return(0);
	for (envp = environ, i = 1; *envp; envp++, i++) {
		if (!strncmp(*envp, str, cp - str)) {
			if (*cp) *envp = str;
			else do {
				*envp = *(envp + 1);
			} while (*(++envp));
			return(0);
		}
	}
	envp = environ;
	if (!(environ = (char **)malloc((i + 1) * sizeof(char *)))) return(-1);
	*environ = str;
	memcpy(environ + 1, envp, i * sizeof(char *));
	if (newenvp) free(newenvp);
	newenvp = environ;
	return(0);
}
#endif

static assoclist *_getenv2(name)
char *name;
{
	assoclist *ap;

	for (ap = environ2; ap; ap = ap -> next)
		if (!strcmp(name, ap -> org)) return(ap);
	if (!strncmp(name, "FD_", 3)) for (ap = environ2; ap; ap = ap -> next)
		if (!strcmp(name + 3, ap -> org)) return(ap);
	return(NULL);
}

int setenv2(name, value, overwrite)
char *name, *value;
int overwrite;
{
	assoclist *ap, *tmp;

	if (ap = _getenv2(name)) {
		if (!overwrite) return(0);
		free(ap -> assoc);
	}
	else {
		if (!value) return(0);
		ap = (assoclist *)malloc2(sizeof(assoclist));
		ap -> org = strdup2(name);
		ap -> next = environ2;
		environ2 = ap;
	}

	if (value) ap -> assoc = (*value) ? strdup2(value) : NULL;
	else {
		free(ap -> org);
		for (tmp = environ2; tmp; tmp = tmp -> next)
			if (tmp -> next == ap) break;
		if (tmp) {
			tmp -> next = ap -> next;
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

	if (ap = _getenv2(name)) return(ap -> assoc);
	else if (cp = (char *)getenv(name)) return(cp);
	return(strncmp(name, "FD_", 3) ? NULL : (char *)getenv(name + 3));
}

int printenv()
{
	assoclist *ap;
	int n;

	n = 0;
	for (ap = environ2; ap; ap = ap -> next) {
		cprintf("%s=%s\r\n", ap -> org,
			(ap -> assoc) ? ap -> assoc : "");
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

int system2(command)
char *command;
{
	char *cp, *alias;
	int status;

	for (cp = command; *cp == ' ' || *cp == '\t'; cp++);
	if (alias = evalalias(cp)) command = alias;
	if (*cp == '!') status = execinternal(cp + 1);
	else status = system(command);
	if (alias) free(alias);
	return(status);
}

int system3(command, noconf)
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
	sigvecreset();
	cooked2();
	echo2();
	nl2();
	tabs();
	status = system2(command);
	raw2();
	noecho2();
	nonl2();
	notabs();
	if (status > 127 || !noconf) warning(0, HITKY_K);
	if (noconf >= 0) {
		if (noconf) putterms(t_init);
		putterms(t_keypad);
	}
	sigvecset();
	return(status);
}

char *getwd2()
{
	char cwd[MAXPATHLEN + 1];

	if (!Xgetcwd(cwd, MAXPATHLEN)) error(NULL);
	return(strdup2(cwd));
}

char *getpwuid2(uid)
int uid;
{
	static strtable *uidlist = NULL;
	static int maxuidbuf = 0;
	static int maxuid = 0;
	struct passwd *pwd;
	int i;

	for (i = 0; i < maxuid; i++)
		if (uid == uidlist[i].no) return(uidlist[i].str);

	if (pwd = getpwuid(uid)) {
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
int gid;
{
	static strtable *gidlist = NULL;
	static int maxgidbuf = 0;
	static int maxgid = 0;
	struct group *grp;
	int i;

	for (i = 0; i < maxgid; i++)
		if (gid == gidlist[i].no) return(gidlist[i].str);

	if (grp = getgrgid(gid)) {
		gidlist = (strtable *)addlist(gidlist, i,
			&maxgidbuf, sizeof(strtable));
		gidlist[i].no = grp -> gr_gid;
		gidlist[i].str = strdup2(grp -> gr_name);
		maxgid++;
		return(gidlist[i].str);
	}

	return(NULL);
}

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

static long gettimezone(tm, time)
struct tm *tm;
int time;
{
#ifdef	NOTMGMTOFF
	struct timeval t_val;
	struct timezone t_zone;
#endif
#ifndef	NOTZFILEH
	struct tzhead head;
#endif
	struct tm tmbuf;
	FILE *fp;
	long tz, tmp, leap, nleap, ntime, ntype, nchar;
	char *cp, buf[MAXPATHLEN + 1];
	u_char c;
	int i;

	memcpy(&tmbuf, tm, sizeof(struct tm));

#ifdef	NOTMGMTOFF
	gettimeofday(&t_val, &t_zone);
	tz = t_zone.tz_minuteswest * 60L;
#else
	tz = -(localtime(&time) -> tm_gmtoff);
#endif

#ifndef	NOTZFILEH
	cp = (char *)getenv("TZ");
	if (!cp || !*cp) cp = TZDEFAULT;
	if (cp[0] == '/') strcpy(buf, cp);
	else {
		strcpy(buf, TZDIR);
		strcat(buf, "/");
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
		if (fread(buf, sizeof(char), 4, fp) != 1) {
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
	|| fread(buf, sizeof(char), 4, fp) != 1) {
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
		if (fread(buf, sizeof(char), 4, fp) != 1) {
			fclose(fp);
			return(tz);
		}
		tmp = char2long(buf);
		if (tmcmp(&tmbuf, localtime(&tmp)) <= 0) break;
		if (fread(buf, sizeof(char), 4, fp) != 1) {
			fclose(fp);
			return(tz);
		}
		leap = char2long(buf);
	}

	tz += leap;
	fclose(fp);
#endif	/* NOTZFILEH */

	return(tz);
}

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
	time_t date, time;
	int i, year;

	year = (tm -> tm_year < 1900) ? tm -> tm_year + 1900 : tm -> tm_year;

	date = (year - 1970) * 365;
	date += ((year - 1 - 1968) / 4)
		- ((year - 1 - 1900) / 100)
		+ ((year - 1 - 1600) / 400);
	for (i = 1; i < tm -> tm_mon + 1; i++) {
		switch (i) {
			case 2:
				if (!(year % 4)
				&& ((year % 100)
				|| !(year % 400))) date++;
				date += 28;
				break;
			case 4:
			case 6:
			case 9:
			case 11:
				date += 30;
				break;
			default:
				date += 31;
				break;
		}
	}
	date += tm -> tm_mday - 1;
	time = (tm -> tm_hour * 60 + tm -> tm_min) * 60 + tm -> tm_sec;
	time += date * 60 * 60 * 24;
	time += gettimezone(tm, time);

	return(time);
# endif
#endif
}
