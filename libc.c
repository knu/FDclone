/*
 *	libc.c
 *
 *	Arrangememt of Library Function
 */

#include "fd.h"
#include "term.h"
#include "kctype.h"
#include "kanji.h"

#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>

#ifdef	USELEAPCNT
#include <tzfile.h>
#endif

extern char fullpath[];

#define	BUFUNIT		32

static assoclist *_getenv2();
static long gettimezone();

static assoclist *environ2 = NULL;


int access2(path, mode)
char *path;
int mode;
{
	char *name, dir[MAXPATHLEN + 1];
	struct stat status;

	if (lstat(path, &status) < 0) error(path);
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

	if (lstat(dir, &status) < 0) error(dir);
	if (access(path, mode) < 0) {
		if (errno == ENOENT) return(0);
		if (errno != EACCES) error(path);
		if (status.st_uid == getuid() && !yesno(DELPM_K, path))
			return(-1);
	}
	return(0);
}

int unlink2(path)
char *path;
{
	if (access2(path, W_OK) < 0) return(1);
	return(unlink(path));
}

int rmdir2(path)
char *path;
{
	if (access2(path, R_OK | W_OK | X_OK) < 0) return(1);
	return(rmdir(path));
}

int rename2(from, to)
char *from, *to;
{
	if (!strcmp(from, to)) return(0);
	return(rename(from, to));
}

int stat2(path, buf)
char *path;
struct stat *buf;
{
	int tmperr;

	if (stat(path, buf) < 0) {
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

static VOID _chdir2(path)
char *path;
{
	char *cp;

	if (!*path || !strcmp(path, ".")) return;
	else if (cp = strchr(path, '/')) {
		*cp = 0;
		_chdir2(path);
		*(cp++) = '/';
		_chdir2(cp);
		return;
	}

	if (!strcmp(path, "..")) {
		cp = strrchr(fullpath, '/');
		if (cp && cp != fullpath) *cp = '\0';
		else strcpy(fullpath, "/");
	}
	else {
		if (strcmp(fullpath, "/")) strcat(fullpath, "/");
		strcat(fullpath, path);
	}
}

int chdir2(path)
char *path;
{
	if (access(path, R_OK | X_OK) < 0 || chdir(path) < 0) return(-1);

	if (*path == '/') strcpy(fullpath, "/");
	_chdir2(path);

	if (chdir(fullpath) < 0) return(-1);

	return(0);
}

int mkdir2(path, mode)
char *path;
int mode;
{
	char *cp1, *cp2, *eol;

	for (eol = path + strlen(path) - 1; eol > path; eol--) {
		if (*eol != '/') break;
		*eol = '\0';
	}

	cp1 = ++eol;
	cp2 = strrchr(path, '/');
	for (;;) {
		if (mkdir(path, mode) >= 0) break;
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
		if (mkdir(path, mode) < 0 && errno != EEXIST) return(-1);
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

int onkanji1(s, ptr)
u_char *s;
int ptr;
{
	int i;

	if (ptr < 0) return(0);
	if (!ptr) return(iskanji1((int)s[0]));

	for (i = 0; i < ptr; i++) if (iskanji1((int)s[i])) i++;
	if (i > ptr) return(0);
	return(iskanji1((int)s[ptr]));
}

u_char *strchr2(s, c)
u_char *s;
u_char c;
{
	int i, len;

	len = strlen((char *)s);
	for (i = 0; i < len; i++) {
		if (s[i] == c) return(&s[i]);
		if (iskanji1(s[i])) i++;
	}
	return(NULL);
}

u_char *strrchr2(s, c)
u_char *s;
u_char c;
{
	int i, len;
	u_char *cp;

	cp = NULL;
	len = strlen((char *)s);
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

char *strncpy3(s1, s2, len, ptr)
char *s1, *s2;
int len, ptr;
{
	char *cp;

	cp = s1;
	if (ptr && onkanji1(s2, ptr - 1)) {
		*(cp++) = ' ';
		ptr++;
		len--;
	}
	if (len < strlen(s2 + ptr) && onkanji1(s2, ptr + len - 1)) {
		strncpy2(cp, s2 + ptr, --len);
		strcat(cp, " ");
	}
	else sprintf(cp, "%-*.*s", len, len, s2 + ptr);
	return(s1);
}

VOID cputs2(s, len, ptr)
char *s;
int len, ptr;
{
	if (ptr > 0 && onkanji1(s, ptr - 1)) {
		putch(' ');
		ptr++;
		len--;
	}
	if (len <= 0) return;
	else if (len >= strlen(s + ptr)) {
		if (ptr < 0) cputs(s);
		else cprintf("%-*.*s", len, len, s + ptr);
	}
	else if (onkanji1(s, ptr + len - 1)) {
		len--;
		cprintf("%-*.*s", len, len, s + ptr);
		putch(' ');
	}
	else cprintf("%-*.*s", len, len, s + ptr);
}

int atoi2(str)
char *str;
{
	return((str && *str >= '0' && *str <= '9') ? atoi(str) : -1);
}

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
	assoclist *ap;

	if (ap = _getenv2(name)) {
		if (!overwrite) return(0);
		free(ap -> assoc);
	}
	else {
		ap = (assoclist *)malloc2(sizeof(assoclist));
		ap -> org = strdup2(name);
		ap -> next = environ2;
		environ2 = ap;
	}
	ap -> assoc = (value) ? strdup2(value) : NULL;
	return(0);
}

char *getenv2(name)
char *name;
{
	assoclist *ap;
	char *cp;

	if (ap = _getenv2(name)) return(ap -> assoc);
	else if (cp = getenv(name)) return(cp);
	return(strncmp(name, "FD_", 3) ? NULL : getenv(name + 3));
}

int printenv()
{
	assoclist *ap;
	int n;

	n = 0;
	for (ap = environ2; ap; ap = ap -> next) {
		cprintf("%s=%s\r\n", ap -> org, ap -> assoc);
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

int system2(command, noconf)
char *command;
int noconf;
{
	int status;

	locate(0, n_line - 1);
	putterm(l_clear);
	tflush();
	if (noconf == 1) putterms(t_end);
	putterms(t_nokeypad);
	tflush();
	cooked2();
	echo2();
	nl2();
	status = system(command);
	raw2();
	noecho2();
	nonl2();
	if (status > 127 || !noconf) warning(0, HITKY_K);
	if (noconf == 1) putterms(t_init);
	putterms(t_keypad);
	return(status);
}

char *getwd2()
{
	char cwd[MAXPATHLEN + 1];

	if (!getcwd(cwd, MAXPATHLEN)) error(NULL);
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

static long gettimezone()
{
#ifdef	USELEAPCNT
	struct tzhead buf;
	FILE *fp;
	char path[MAXPATHLEN + 1];
#endif
	long tz;
#ifdef	HAVETIMEZONE
	extern long timezone;

	tz = timezone;
#else
	struct tm *tm;

	tz = 0;
	tm = localtime(&tz);
	tz = -(tm -> tm_gmtoff);
#endif
#ifdef	USELEAPCNT
	strcpy(path, TZDIR);
	strcat(path, "/");
	strcat(path, TZDEFAULT);
	if (fp = fopen(path, "r")) {
		if (fread(&buf, sizeof(struct tzhead), 1, fp) == 1)
			tz += *((long *)buf.tzh_leapcnt);
		fclose(fp);
	}
#endif
	return(tz);
}

time_t timelocal2(tm)
struct tm *tm;
{
	int i, date, time;

	if (tm -> tm_year < 1900) tm -> tm_year += 1900;

	date = (tm -> tm_year - 1970) * 365;
	date += ((tm -> tm_year - 1 - 1968) / 4)
		- ((tm -> tm_year - 1 - 1900) / 100)
		+ ((tm -> tm_year - 1 - 1600) / 400);
	for (i = 1; i < tm -> tm_mon + 1; i++) {
		switch (i) {
			case 2:
				if (!(tm -> tm_year % 4)
				&& ((tm -> tm_year % 100)
				|| !(tm -> tm_year % 400))) date++;
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

	return(((time_t)date * 60 * 60 * 24) + (time_t)time + gettimezone());
}
