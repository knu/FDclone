/*
 *	pathname.c
 *
 *	Path Name Management Module
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include "machine.h"

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#if	MSDOS
#include "unixemu.h"
#define	strpathcmp	stricmp
#define	strnpathcmp	strnicmp
#else
#include <pwd.h>
#include <sys/file.h>
#include <sys/param.h>
#define	strpathcmp	strcmp
#define	strnpathcmp	strncmp
# ifdef	USEDIRECT
# include <sys/dir.h>
# define	dirent	direct
# else
# include <dirent.h>
# endif
#endif

#include "pathname.h"

#ifdef	NOERRNO
extern int errno;
#endif

#ifdef	FD
extern char *getenv2 __P_((char *));
extern DIR *Xopendir __P_((char *));
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern int Xstat __P_((char *, struct stat *));
extern int Xaccess __P_((char *, int));
# ifdef	NOVOID
extern error __P_((char *));
# else
extern void error __P_((char *));
# endif
extern char *progpath;
#else
#define	getenv2		(char *)getenv
#define	Xopendir	opendir
#define	Xclosedir	closedir
#define	Xreaddir	readdir
#define	Xstat		stat
#define	Xaccess		access
#define	error		return
#endif

#if	!MSDOS
static int completeuser __P_((char *, int, char **));
#endif
#if	!MSDOS || !defined (_NOCOMPLETE)
static DIR *opennextpath __P_((char **, char *, char**));
static struct dirent *readnextpath __P_((DIR **, char **, char *, char**));
#endif

static int skipdotfile;


char *_evalpath(path, eol, keepdelim)
char *path, *eol;
int keepdelim;
{
#if	!MSDOS
	struct passwd *pwd;
#endif
	char *cp, *tmp, *next, buf[MAXPATHLEN + 1];

	if (*path == '~') {
		if (!(cp = strchr(path + 1, _SC_)) || cp > eol) cp = eol;
		if (cp > path + 1) {
			strncpy(buf, path + 1, cp - path - 1);
			buf[cp - path - 1] = '\0';
#ifdef	FD
			if (!strpathcmp(buf, "FD")) tmp = progpath;
			else
#endif
#if	!MSDOS
			if (pwd = getpwnam(buf)) tmp = pwd -> pw_dir;
			else
#endif
			tmp = NULL;
		}
#if	MSDOS
		else tmp = (char *)getenv("HOME");
#else
		else if (!(tmp = (char *)getenv("HOME"))
		&& (pwd = getpwuid(getuid())))
			tmp = pwd -> pw_dir;
#endif

		if (tmp) strcpy(buf, tmp);
		else {
			strncpy(buf, path, cp - path);
			buf[cp - path] = '\0';
		}
		path = cp;
		if (path < eol) {
			strcat(buf, _SS_);
			path++;
		}
	}
	else if (*path == _SC_) {
		strcpy(buf, _SS_);
		path++;
	}
#if	MSDOS
	else if (isalpha(*path) && path[1] == ':') {
		strncpy(buf, path, 2);
		buf[2] = '\0';
		path += 2;
		if (*path == _SC_) {
			strcat(buf, _SS_);
			path++;
		}
	}
#endif
	else *buf = '\0';

	while (path < eol) {
		if (*path == _SC_) {
			if (keepdelim) strcat(buf, _SS_);
			path++;
			continue;
		}
		if (!(next = strchr(path, _SC_)) || next > eol) next = eol;
		while ((cp = strchr(path, '$')) && cp < next) {
			strncat(buf, path, cp - path);
#if	MSDOS
			if (*(cp + 1) == '$') {
				path = cp + 2;
#else
			if (cp > path && *(cp - 1) == '\\') {
				path = ++cp;
#endif
				strcat(buf, "$");
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
			strcat(buf, _SS_);
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
	cp = _evalpath(cp, eol, 0);
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
#if	defined (USERE_COMP) || defined (USEREGCOMP) || defined (USEREGCMP)
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
#if	defined (USERE_COMP) || defined (USEREGCOMP) || defined (USEREGCMP)
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
extern char *re_comp __P_((char *));
extern int re_exec __P_((char *));

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

#else	/* !USERE_COMP */
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

# else	/* !USEREGCOMP */

int regexp_free(re)
reg_t *re;
{
	if (re) free(re);
}

#  ifdef	USEREGCMP
extern char *regcmp(char *, int);
extern char *regex(char *, char *);

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

#  else	/* !USEREGCMP */

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
#if	MSDOS
				else if (toupper(*(cp1++)) == toupper(*cp2))
					cp2++;
#else
				else if (*(cp1++) == *cp2) cp2++;
#endif
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

#  endif	/* !USEREGCMP */
# endif		/* !USEREGCOMP */
#endif		/* !USERE_COMP */

#if	!MSDOS
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
		strcat(*matchp + ptr, _SS_);
		matchno++;
	}
	endpwent();
	return(matchno);
}
#endif

#if	!MSDOS || !defined (_NOCOMPLETE)
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
		if (!strpathcmp(buf, target)) return(buf);
		buf += strlen(buf) + 1;
	}
	return(NULL);
}

static DIR *opennextpath(pathp, dir, eolp)
char **pathp, *dir, **eolp;
{
	DIR *dirp;
	char *cp;

	if (dir == *pathp) {
		dirp = Xopendir(dir);
		*pathp = NULL;
	}
	else do {
		cp = *pathp;
#if	MSDOS
		if (!(*pathp = strchr(cp, ';'))) strcpy(dir, cp);
#else
		if (!(*pathp = strchr(cp, ':'))) strcpy(dir, cp);
#endif
		else {
			strncpy(dir, cp, *pathp - cp);
			dir[((*pathp)++) - cp] = '\0';
		}
	} while (!(dirp = Xopendir(dir)) && *pathp);

	if (!dirp) return(NULL);
	cp = dir + strlen(dir);
	if (cp <= dir || *(cp - 1) != _SC_) strcpy(cp++, _SS_);
	*eolp = cp;
	return(dirp);
}

static struct dirent *readnextpath(dirpp, pathp, dir, eolp)
DIR **dirpp;
char **pathp, *dir, **eolp;
{
	struct dirent *dp;

	if (dp = Xreaddir(*dirpp)) return(dp);
	while (*pathp) {
		Xclosedir(*dirpp);
		if (!(*dirpp = opennextpath(pathp, dir, eolp))) return(NULL);
		if (dp = Xreaddir(*dirpp)) return(dp);
	}
	return(NULL);
}

int completepath(path, matchno, matchp, exe, full)
char *path;
int matchno;
char **matchp;
int exe, full;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char *cp, *name, *file, *next, dir[MAXPATHLEN + 1];
	int len, ptr, size, dirflag;

	size = lastpointer(*matchp, matchno) - *matchp;
	next = NULL;
	if (file = strrchr(path, _SC_)) {
		if (file == path) strcpy(dir, _SS_);
#if	MSDOS
		else if (isalpha(*path) && path[1] == ':' && file == &path[2]) {
			strncpy(dir, path, 3);
			dir[3] = '\0';
		}
#endif
		else {
			strncpy(dir, path, file - path);
			dir[file - path] = '\0';
		}
		next = dir;
		file++;
	}
#if	!MSDOS
	else if (*path == '~') return(completeuser(path + 1, matchno, matchp));
#endif
	else if (exe) {
		next = (char *)getenv("PATH");
		file = path;
	}
	else {
		strcpy(dir, ".");
		next = dir;
		file = path;
	}
	len = strlen(file);

	if (!(dirp = opennextpath(&next, dir, &cp))) return(matchno);
	while (dp = readnextpath(&dirp, &next, dir, &cp)) {
		if (!len) {
			if (!strcmp(dp -> d_name, ".")
			|| !strcmp(dp -> d_name, "..")) continue;
		}
		strcpy(cp, dp -> d_name);
		name = (full) ? dir : dp -> d_name;
		if (exe > 1) {
			if (strpathcmp(file, dp -> d_name)) continue;
		}
		else if (strnpathcmp(file, dp -> d_name, len)) continue;
		if (finddupl(*matchp, matchno, name)) continue;

		dirflag = (Xstat(dir, &status) >= 0
		&& (status.st_mode & S_IFMT) == S_IFDIR) ? 1 : 0;
		if ((exe > 1 && dirflag) || (exe && Xaccess(dir, X_OK) < 0))
			continue;
		ptr = size;
		size += strlen(name) + dirflag + 1;
		*matchp = (*matchp) ? (char *)realloc(*matchp, size)
			: (char *)malloc(size);
		if (!*matchp) error(NULL);

		strcpy(*matchp + ptr, name);
		if (dirflag) strcat(*matchp + ptr, _SS_);
		matchno++;
		if (exe > 1) break;
	}
	if (dirp) Xclosedir(dirp);

	return(matchno);
}
#endif	/* !MSDOS || !_NOCOMPLETE */

#ifndef	_NOCOMPLETE
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
	&& (cp = common + (int)strlen(common) - 1) >= common && *cp != _SC_) {
		*(++cp) = ' ';
		*(++cp) = '\0';
	}

	if (!*common) return(NULL);
	if (!(cp = (char *)malloc(strlen(common) + 1))) error(NULL);
	strcpy(cp, common);
	return(cp);
}
#endif	/* _NOCOMPLETE */
