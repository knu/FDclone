/*
 *	pathname.c
 *
 *	Path Name Management Module
 */

#include "machine.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USEDIRECT
#include <sys/dir.h>
#define	dirent	direct
#else
#include <dirent.h>
#endif

#include "pathname.h"

#ifdef	NOERRNO
extern int errno;
#endif

#ifdef	FD
extern char *getenv2();
extern DIR *Xopendir();
extern int Xclosedir();
extern struct dirent *Xreaddir();
extern int Xstat();
extern int Xaccess();
# ifdef	NOVOID
extern error();
# else
extern void error();
# endif
#else
#define	getenv2		(char *)getenv
#define	Xopendir	opendir
#define	Xclosedir	closedir
#define	Xreaddir	readdir
#define	Xstat		stat
#define	Xaccess		access
#define	error		return
#endif

static int completeuser();

static int skipdotfile;


char *_evalpath(path, eol)
char *path, *eol;
{
	struct passwd *pwd;
	char *cp, *tmp, *next, buf[MAXPATHLEN + 1];

	if (*path == '~') {
		if (!(cp = strchr(path + 1, '/')) || cp > eol) cp = eol;
		if (cp > path + 1) {
			strncpy(buf, path + 1, cp - path - 1);
			buf[cp - path - 1] = '\0';
			if (pwd = getpwnam(buf)) tmp = pwd -> pw_dir;
			else tmp = NULL;
		}
		else if (!(tmp = (char *)getenv("HOME"))
		&& (pwd = getpwuid(getuid())))
			tmp = pwd -> pw_dir;
		if (tmp) strcpy(buf, tmp);
		else {
			strncpy(buf, path, cp - path);
			buf[cp - path] = '\0';
		}
		path = cp;
		if (*path) {
			strcat(buf, "/");
			path++;
		}
	}
	else if (*path == '/') {
		path++;
		strcpy(buf, "/");
	}
	else *buf = '\0';

	while (path < eol) {
		if (!(next = strchr(path, '/')) || next > eol) next = eol;
		while ((cp = strchr(path, '$')) && cp < next) {
			strncat(buf, path, cp - path);
			if (cp > path && *(cp - 1) == '\\') {
				strcat(buf, "$");
				path = ++cp;
				continue;
			}
			if (*(++cp) == '{') cp++;
			path = cp;
			if (*cp == '_' || isalpha(*cp))
				for (cp++; *cp == '_' || isalnum(*cp); cp++);
			if (cp > path) {
				tmp = buf + strlen(buf);
				strncpy(tmp, path, cp - path);
				tmp[cp - path] = '\0';
				if (path = getenv2(tmp)) strcpy(tmp, path);
				else *tmp = '\0';
			}
			else strcat(buf, "$");
			if (*cp == '}') cp++;
			path = cp;
		}
		strncat(buf, path, next - path);
		path = next;
		if (path < eol) {
			strcat(buf, "/");
			path++;
		}
	}

	if (!(tmp = (char *)malloc(strlen(buf) + 1))) error(NULL);
	strcpy(tmp, buf);
	return(tmp);
}

char *evalpath(path)
char *path;
{
	char *cp, *eol;

	if (!path || !(*path)) return(path);
	for (cp = path; *cp == ' ' || *cp == '\t'; cp++);
	eol = cp + strlen(cp);
	cp = _evalpath(cp, eol);
	free(path);
	return(cp);
}

char *cnvregexp(str, exceptdot)
char *str;
int exceptdot;
{
	char *cp, *pattern;
	int i, flag;

	if (!*str) str = "*";
	if (!(pattern = (char *)malloc(1 + strlen(str) * 2 + 3))) error(NULL);
	pattern[0] = (exceptdot && (*str == '*' || *str == '?')) ? '.' : ' ';
	i = 1;
	pattern[i++] = '^';
	flag = 0;
	for (cp = str; *cp; cp++) {
#if defined (USERE_COMP) || defined (USEREGCOMP) || defined (USEREGCMP)
		if (flag) {
			if (*cp == ']') flag = 0;
			pattern[i++] = *cp;
			continue;
		}
#endif
		switch (*cp) {
			case '\\':
				if (!*(cp + 1)) break;
				pattern[i++] = *(cp++);
				pattern[i++] = *cp;
				break;
			case '?':
				pattern[i++] = '.';
				break;
			case '*':
				pattern[i++] = '.';
				pattern[i++] = '*';
				break;
#if defined (USERE_COMP) || defined (USEREGCOMP) || defined (USEREGCMP)
			case '[':
				flag = 1;
				pattern[i++] = *cp;
				break;
#endif
			case '^':
			case '$':
			case '.':
				pattern[i++] = '\\';
			default:
				pattern[i++] = *cp;
				break;
		}
	}
	pattern[i++] = '$';
	pattern[i++] = '\0';

	return(pattern);
}

#ifdef	USERE_COMP
extern char *re_comp();
extern int re_exec();

reg_t *regexp_init(s)
char *s;
{
	skipdotfile = (*s == '.');
	re_comp(s + 1);
	return((reg_t *)1);
}

/*ARGSUSED*/
int regexp_exec(re, s)
reg_t *re;
char *s;
{
	if (skipdotfile && *s == '.') return(0);
	return(re_exec(s) > 0);
}

/*ARGSUSED*/
int regexp_free(re)
reg_t *re;
{
	return(0);
}

#else		/* USERE_COMP */
# ifdef	USEREGCOMP

reg_t *regexp_init(s)
char *s;
{
	reg_t *re;

	skipdotfile = (*s == '.');
	if (!(re = (reg_t *)malloc(sizeof(reg_t)))) error(NULL);
	if (regcomp(re, s + 1, REG_EXTENDED)) {
		free(re);
		return(NULL);
	}
	return(re);
}

int regexp_exec(re, s)
reg_t *re;
char *s;
{
	if (!re || (skipdotfile && *s == '.')) return(0);
	return(!regexec(re, s, 0, NULL, 0));
}

int regexp_free(re)
reg_t *re;
{
	if (re) regfree(re);
}

# else		/* USEREGCOMP */

int regexp_free(re)
reg_t *re;
{
	if (re) free(re);
}

#  ifdef	USEREGCMP
extern char *regcmp();
extern char *regex();

reg_t *regexp_init(s)
char *s;
{
	skipdotfile = (*s == '.');
	return(regcmp(s + 1, 0));
}

int regexp_exec(re, s)
reg_t *re;
char *s;
{
	if (!re || (skipdotfile && *s == '.')) return(0);
	return(regex(re, s) ? 1 : 0);
}

#  else		/* USEREGCMP */

reg_t *regexp_init(s)
char *s;
{
	reg_t *re;

	skipdotfile = (*s == '.');
	if (!(re = malloc(strlen(s + 1) + 1))) error(NULL);
	strcpy(re, s + 1);
	return(re);
}

int regexp_exec(re, s)
char *re, *s;
{
	char *cp1, *cp2, *bank1, *bank2;

	cp1 = re;
	cp2 = s;
	bank1 = NULL;

	if (skipdotfile && *s == '.') return(0);
	while (cp1 && *cp1) {
		switch (*cp1) {
			case '^':
				if (cp2 == s) cp1++;
				else cp1 = NULL;
				break;
			case '$':
				if (!*cp2) cp1++;
				else cp1 = NULL;
				break;
			case '.':
				cp1++;
				if (*cp2) cp2++;
				else if (*(cp1++) != '*') cp1 = NULL;
				break;
			case '*':
				if (cp1 > re && *cp2) {
					bank1 = cp1 - 1;
					bank2 = cp2;
				}
				cp1++;
				break;
			case '\\':
				if (!*(++cp1)) break;
			default:
				if (!*cp2) cp1 = bank1 = NULL;
				else if (*(cp1++) == *cp2) cp2++;
				else cp1 = NULL;
				break;
		}
		if (!cp1 && bank1) {
			cp1 = bank1;
			cp2 = bank2;
			bank1 = NULL;
		}
	}
	return(cp1 && !*cp2);
}

#  endif	/* USEREGCMP */
# endif		/* USEREGCOMP */
#endif		/* USERE_COMP */

char *lastpointer(buf, n)
char *buf;
int n;
{
	while (n--) buf += strlen(buf) + 1;
	return(buf);
}

char *finddupl(buf, n, target)
char *buf;
int n;
char *target;
{
	while (n--) {
		if (!strcmp(buf, target)) return(buf);
		buf += strlen(buf) + 1;
	}
	return(NULL);
}

static int completeuser(name, matchno, matchp)
char *name;
int matchno;
char **matchp;
{
	struct passwd *pwd;
	int len, ptr, size;

	size = lastpointer(*matchp, matchno) - *matchp;
	len = strlen(name);
	setpwent();
	while (pwd =getpwent()) {
		if (strncmp(name, pwd -> pw_name, len)) continue;
		ptr = size;
		size += 1 + strlen(pwd -> pw_name) + 1 + 1;
		*matchp = (*matchp) ? (char *)realloc(*matchp, size)
			: (char *)malloc(size);
		if (!*matchp) error(NULL);

		*(*matchp + (ptr++)) = '~';
		strcpy(*matchp + ptr, pwd -> pw_name);
		strcat(*matchp + ptr, "/");
		matchno++;
	}
	endpwent();
	return(matchno);
}

int completepath(path, exe, matchno, matchp)
char *path;
int exe, matchno;
char **matchp;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char *cp, *file, *next, dir[MAXPATHLEN + 1];
	int len, ptr, size, dirflag;

	size = lastpointer(*matchp, matchno) - *matchp;
	next = NULL;
	if (file = strrchr(path, '/')) {
		if (file == path) strcpy(dir, "/");
		else {
			strncpy(dir, path, file - path);
			dir[file - path] = '\0';
		}
		file++;
	}
	else if (*path == '~') return(completeuser(path + 1, matchno, matchp));
	else if (exe) {
		cp = (char *)getenv("PATH");
		if (!(next = strchr(cp, ':'))) strcpy(dir, cp);
		else {
			strncpy(dir, cp, next - cp);
			dir[(next++) - cp] = '\0';
		}
		file = path;
	}
	else {
		strcpy(dir, ".");
		file = path;
	}
	len = strlen(file);

	if (!(dirp = Xopendir(dir))) return(matchno);
	cp = dir + strlen(dir);
	if (strcmp(dir, "/")) strcpy(cp++, "/");
	while ((dp = Xreaddir(dirp)) || next) {
		while (!dp && next) {
			do {
				if (dirp) Xclosedir(dirp);
				cp = next;
				if (!(next = strchr(cp, ':'))) strcpy(dir, cp);
				else {
					strncpy(dir, cp, next - cp);
					dir[(next++) - cp] = '\0';
				}
			} while (!(dirp = Xopendir(dir)) && next);
			cp = dir + strlen(dir);
			if (strcmp(dir, "/")) strcpy(cp++, "/");
			dp = Xreaddir(dirp);
		}
		if (!dp) break;

		if (!len) {
			if (!strcmp(dp -> d_name, ".")
			|| !strcmp(dp -> d_name, "..")) continue;
		}
		if (strncmp(file, dp -> d_name, len)
		|| finddupl(*matchp, matchno, dp -> d_name)) continue;

		strcpy(cp, dp -> d_name);
		dirflag = (Xstat(dir, &status) >= 0
		&& (status.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
		if (exe && (dirflag || Xaccess(dir, X_OK) < 0)) continue;
		ptr = size;
		size += strlen(dp -> d_name) + dirflag + 1;
		*matchp = (*matchp) ? (char *)realloc(*matchp, size)
			: (char *)malloc(size);
		if (!*matchp) error(NULL);

		strcpy(*matchp + ptr, dp -> d_name);
		if (dirflag) strcat(*matchp + ptr, "/");
		matchno++;
	}
	if (dirp) Xclosedir(dirp);

	return(matchno);
}

char *findcommon(strs, max)
char *strs;
int max;
{
	char *cp, common[MAXNAMLEN + 1];
	int i, j;

	cp = strs;
	strcpy(common, cp);

	for (i = 1; i < max; i++) {
		cp += strlen(cp) + 1;
		for (j = 0; common[j]; j++) if (common[j] != cp[j]) break;
		common[j] = '\0';
	}
	if (max == 1
	&& (cp = common + (int)strlen(common) - 1) >= common && *cp != '/') {
		*(++cp) = ' ';
		*(++cp) = '\0';
	}

	if (!*common) return(NULL);
	if (!(cp = (char *)malloc(strlen(common) + 1))) error(NULL);
	strcpy(cp, common);
	return(cp);
}
