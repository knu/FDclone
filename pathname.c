/*
 *	pathname.c
 *
 *	Path Name Management Module
 */

#include "machine.h"
#include "pathname.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/stat.h>

#ifndef	NOSTDLIB
#include <stdlib.h>
#endif

#ifdef	FD
extern char *getenv2();
# ifdef	NOVOID
extern error();
# else
extern void error();
# endif
#else
#define	getenv2 getenv
#define	error return
#endif

static char *completeuser();


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
		else if (!(tmp = getenv("HOME")) && (pwd = getpwuid(getuid())))
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
			if (*(++cp) == '{') cp++;
			path = cp;
			if (*cp == '_' || isalpha(*cp)) {
				for (cp++; *cp == '_' || isalpha(*cp)
					|| isdigit(*cp); cp++);
			}
			if (cp > path) {
				tmp = buf + strlen(buf);
				strncpy(tmp, path, cp - path);
				tmp[cp - path] = '\0';
				if (path = getenv2(tmp)) strcpy(tmp, path);
				else *tmp = '\0';
			}
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

	if (!path) return(NULL);
	for (cp = path; *cp == ' ' || *cp == '\t'; cp++);
	eol = cp + strlen(cp);
	cp = _evalpath(cp, eol);
	free(path);
	return(cp);
}

char *cnvregexp(str)
char *str;
{
	char *cp, *pattern;
	int i;

	if (!*str) str = "*";
	if (!(pattern = (char *)malloc(strlen(str) * 2 + 3))) error(NULL);
	i = 0;
	pattern[i++] = '^';
	for (cp = str; *cp; cp++) {
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
	char *cp;

	re_comp(s);
	return((reg_t *)1);
}

int regexp_exec(re, s)
reg_t *re;
char *s;
{
	return(re_exec(s) > 0);
}

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

	if (!(re = (reg_t *)malloc(sizeof(reg_t)))) error(NULL);
	if (regcomp(re, s, REG_EXTENDED)) {
		free(re);
		return(NULL);
	}
	return(re);
}

int regexp_exec(re, s)
reg_t *re;
char *s;
{
	if (!re) return(0);
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
	return(regcmp(s, 0));
}

int regexp_exec(re, s)
reg_t *re;
char *s;
{
	if (!re) return(0);
	return(regex(re, s) ? 1 : 0);
}

#  else		/* USEREGCMP */

reg_t *regexp_init(s)
char *s;
{
	reg_t *re;

	if (!(re = malloc(strlen(s) + 1))) error(NULL);
	strcpy(re, s);
	return(re);
}

int regexp_exec(re, s)
char *re, *s;
{
	char *cp1, *cp2, *bank1, *bank2;

	cp1 = re;
	cp2 = s;
	bank1 = NULL;

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

static char *completeuser(name)
char *name;
{
	struct passwd *pwd;
	char *cp, match[MAXNAMLEN + 1];
	int i, len, matchno;

	matchno = 0;
	*match = '~';
	len = strlen(name);
	setpwent();
	while (pwd =getpwent()) {
		if (strncmp(name, pwd -> pw_name, len)) continue;
		if (!matchno++) strcpy(match + 1, pwd -> pw_name);
		else {
			for (i = 0; match[i]; i++)
				if (match[i + 1] != pwd -> pw_name[i]) break;
			match[++i] = '\0';
		}
	}
	endpwent();
	if (!matchno) return(NULL);
	if (matchno == 1) strcat(match, "/");

	if (!(cp = (char *)malloc(strlen(match) + 1))) error(NULL);
	strcpy(cp, match);
	return(cp);
}

char *completepath(path)
char *path;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char *cp, dir[MAXPATHLEN + 1], match[MAXNAMLEN + 1];
	int i, len, matchno;

	if (cp = strrchr(path, '/')) {
		if (cp == path) strcpy(dir, "/");
		else {
			strncpy(dir, path, cp - path);
			dir[cp - path] = '\0';
		}
		cp++;
	}
	else if (*path == '~') return(completeuser(path + 1));
	else {
		strcpy(dir, ".");
		cp = path;
	}
	len = strlen(cp);

	matchno = 0;
	if (!(dirp = opendir(dir))) return(NULL);
	while (dp = readdir(dirp)) {
		if (!len) {
			if (!strcmp(dp -> d_name, ".")
			|| !strcmp(dp -> d_name, "..")) continue;
		}
		else if (strncmp(cp, dp -> d_name, len)) continue;
		if (!matchno++) strcpy(match, dp -> d_name);
		else {
			for (i = 0; match[i]; i++)
				if (match[i] != dp -> d_name[i]) break;
			match[i] = '\0';
		}
	}
	closedir(dirp);
	if (!matchno) return(NULL);

	len = cp - path;
	strncpy(dir, path, len);
	strcpy(dir + len, match);
	if (matchno == 1) {
		if (stat(dir, &status) == 0
		&& (status.st_mode & S_IFMT) == S_IFDIR) strcat(dir, "/");
		else strcat(dir, " ");
	}

	if (!(cp = (char *)malloc(strlen(dir) + 1))) error(NULL);
	strcpy(cp, dir);
	return(cp);
}
