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
#ifndef	issjis1
#define	issjis1(c)	((0x81 <= (c) && (c) <= 0x9f) \
			|| (0xe0 <= (c) && (c) <= 0xfc))
#endif
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
extern int _dospath __P_((char *));
# ifdef	NOVOID
extern error __P_((char *));
# else
extern void error __P_((char *));
# endif
# if	MSDOS && !defined (_NOUSELFN)
extern char *shortname __P_((char *, char *));
# endif
extern char *progpath;
#else
#define	getenv2		(char *)getenv
#define	Xopendir	opendir
#define	Xclosedir	closedir
#define	Xreaddir	readdir
#define	Xstat		stat
#define	Xaccess		access
#define	_dospath(s)	(isalpha(*s) && s[1] == ':')
#define	error		return
#endif

static int evalenv __P_((char *, int, int));
#if	!MSDOS
static int completeuser __P_((char *, int, char **));
#endif
#if	!MSDOS || !defined (_NOCOMPLETE)
static DIR *opennextpath __P_((char **, char *, char**));
static struct dirent *readnextpath __P_((DIR **, char **, char *, char**));
#endif

static int skipdotfile = 0;


int isdelim(s, ptr)
char *s;
int ptr;
{
#if	MSDOS
	int i;
#endif

	if (ptr < 0 || s[ptr] != _SC_) return(0);
#if	MSDOS
	if (--ptr < 0) return(1);
	if (!ptr) return(!issjis1((u_char)(s[0])));

	for (i = 0; s[i] && i < ptr; i++) if (issjis1((u_char)(s[i]))) i++;
	if (!s[i] || i > ptr) return(1);
	return(!issjis1((u_char)(s[i])));
#else
	return(1);
#endif
}

#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
char *strdelim(s, d)
char *s;
int d;
{
	int i;

	if (d && _dospath(s)) return(s + 1);
	for (i = 0; s[i]; i++) {
		if (s[i] == _SC_) return(&(s[i]));
#if	MSDOS
		if (issjis1((u_char)(s[i])) && !s[++i]) break;
#endif
	}
	return(NULL);
}

char *strrdelim(s, d)
char *s;
int d;
{
	char *cp;
	int i;

	if (d && _dospath(s)) cp = s + 1;
	else cp = NULL;
	for (i = 0; s[i]; i++) {
		if (s[i] == _SC_) cp = &(s[i]);
#if	MSDOS
		if (issjis1((u_char)(s[i])) && !s[++i]) break;
#endif
	}
	return(cp);
}
#endif	/* MSDOS || (FD && !_NODOSDRIVE) */

char *strrdelim2(s, eol)
char *s, *eol;
{
#if	MSDOS
	char *cp;
	int i;

	cp = NULL;
	for (i = 0; s[i] && &(s[i]) < eol; i++) {
		if (s[i] == _SC_) cp = &(s[i]);
		if (issjis1((u_char)(s[i])) && !s[++i]) break;
	}
	return(cp);
#else
	for (eol--; eol >= s; eol--) if (*eol == _SC_) return(eol);
	return(NULL);
#endif
}

char *strcatdelim(s)
char *s;
{
	char *cp;
	int i;

	cp = NULL;
	for (i = 0; s[i]; i++) {
		if (s[i] == _SC_) {
			if (!cp) cp = &(s[i]);
			continue;
		}
		cp = NULL;
#if	MSDOS
		if (issjis1((u_char)(s[i])) && !s[++i]) break;
#endif
	}
	if (!cp) *(cp = &(s[i])) = _SC_;
	*(++cp) = '\0';
	return(cp);
}

static int evalenv(s, off, len)
char *s;
int off, len;
{
	char *env;

	if (off < 0) return(len);
	if (off == len) {
		s[off] = '$';
		return(off + 1);
	}
	s[len] = '\0';
	if (!(env = getenv2(s + off))) return(len);
	len = strlen(env);
	if (off + len >= MAXPATHLEN) len = MAXPATHLEN - 1 - off;
	strncpy(s + off, env, len);
	len += off;
	s[len] = '\0';
	return(len);
}

char *_evalpath(path, eol, keepdelim, evalq)
char *path, *eol;
int keepdelim, evalq;
{
#if	!MSDOS
	struct passwd *pwd;
#else
# if	defined (FD) && MSDOS && !defined (_NOUSELFN)
	char alias[MAXPATHLEN + 1];
	int top = -1;
# endif
#endif
	char *cp, *tmp, buf[MAXPATHLEN + 1], qstack[MAXPATHLEN + 1];
	int i, j, env, quote, paren;

	if (!eol) eol = path + strlen(path);
	i = j = 0;

	quote = -1;
	if ((path[i] == '"' || path[i] == '\'' || path[i] == '`')) {
		qstack[++quote] = path[i++];
		if (!evalq) {
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
			top = j;
#endif
			buf[j++] = qstack[quote];
		}
	}

	if ((quote < 0 || qstack[quote] != '\'') && path[i] == '~') {
		if (!(cp = strdelim(path + i + 1, 0)) || cp > eol) cp = eol;
		if (cp > path + i + 1) {
			strncpy(buf + j, path + i + 1, cp - (path + i + 1));
			buf[j + cp - (path + i + 1)] = '\0';
#ifdef	FD
			if (!strpathcmp(buf + j, "FD")) tmp = progpath;
			else
#endif
#if	!MSDOS
			if ((pwd = getpwnam(buf + j))) tmp = pwd -> pw_dir;
			else
#endif
			tmp = NULL;
		}
		else {
			tmp = (char *)getenv("HOME");
#if	!MSDOS
			if (!tmp && (pwd = getpwuid(getuid())))
				tmp = pwd -> pw_dir;
#endif
		}

		if (tmp) {
			strcpy(buf + j, tmp);
			j = strlen(buf);
		}
		else {
			strncpy(buf + j, path + i, cp - (path + i));
			j += cp - (path + i);
		}
		i = cp - path;
		if (path + i < eol) {
			j = strcatdelim(buf) - buf;
			i++;
		}
	}
	else if (path[i] == _SC_) {
		buf[j++] = _SC_;
		i++;
	}
#if	MSDOS
	else if (isalpha(path[i]) && path[i + 1] == ':') {
		strncpy(buf + j, path + i, 2);
		j += 2;
		i += 2;
		if (path[i] == _SC_) {
			buf[j++] = _SC_;
			i++;
		}
	}
#endif
	else buf[j] = '\0';

	paren = '\0';
	env = -1;
	for (; i < eol - path && path[i] && j < MAXPATHLEN; i++) {
#if	!MSDOS
		if (path[i] == '\\' && (quote < 0 || qstack[quote] != '\'')) {
			if (env >= 0) {
				j = evalenv(buf, env, j);
				env = -1;
			}
			buf[j++] = path[i++];
			buf[j++] = path[i];
			continue;
		}
#endif
		if (quote >= 0) {
			if (path[i] == qstack[quote]) {
				if (env >= 0) {
					j = evalenv(buf, env, j);
					env = -1;
				}
				if (!evalq) {
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
					cp = NULL;
					if (qstack[quote] == '"' &&
					top >= 0 && top + 1 < j) {
						buf[j] = '\0';
						cp = shortname(buf + top + 1,
							alias);
					}
					if (!cp) buf[j++] = qstack[quote];
					else {
						j = top + strlen(alias);
						if (j > MAXPATHLEN)
							j = MAXPATHLEN;
						strncpy(buf + top, alias,
							j - top);
					}
					top = -1;
#else
					buf[j++] = qstack[quote];
#endif
				}
				quote--;
				continue;
			}
			if (qstack[quote] == '\'') {
				buf[j++] = path[i];
				continue;
			}
			if (path[i] == '"'
			|| path[i] == '\'' || path[i] == '`') {
				qstack[++quote] = path[i];
				if (!evalq) {
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
					top = j;
#endif
					buf[j++] = qstack[quote];
				}
				continue;
			}
		}
		if (env >= 0) {
			if (paren) {
				if (path[i] != '}') buf[j++] = path[i];
				else {
					j = evalenv(buf, env, j);
					env = -1;
					paren = 0;
				}
				continue;
			}
			if (path[i] == '{' && env == j) {
				paren = 1;
				continue;
			}
			if (path[i] != '_' && !isalpha(path[i])
			&& (path[i] < '0' || path[i] > '9' || env == j)) {
#if	MSDOS
				if (env == j
				&& (path[i] < '0' || path[i] > '9')) {
					buf[j++] = path[i];
#else
				if (env == j && path[i] == '$') {
					env = getpid();
					sprintf(buf + j, "%d", env);
					while (buf[j]) j++;
#endif
					env = -1;
					continue;
				}
				j = evalenv(buf, env, j);
				env = -1;
			}
		}

		if (quote < 0
		&& (path[i] == '"' || path[i] == '\'' || path[i] == '`')) {
			qstack[++quote] = path[i];
			if (!evalq) {
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
				top = j;
#endif
				buf[j++] = qstack[quote];
			}
		}
		else if (path[i] == _SC_
		&& isdelim(path, i - 1) && !keepdelim);
		else if (path[i] == '$') env = j;
		else buf[j++] = path[i];
	}
	buf[j = evalenv(buf, env, j)] = '\0';

	if (!(tmp = (char *)malloc(j + 1))) error(NULL);
	strcpy(tmp, buf);
	return(tmp);
}

char *evalpath(path, evalq)
char *path;
int evalq;
{
	char *cp;

	if (!path || !(*path)) return(path);
	for (cp = path; *cp == ' ' || *cp == '\t'; cp++);
	cp = _evalpath(cp, NULL, 0, evalq);
	free(path);
	return(cp);
}

char *cnvregexp(str, exceptdot)
char *str;
int exceptdot;
{
	char *cp, *pattern;
	int i;
#if	defined (USERE_COMP) || defined (USEREGCOMP) || defined (USEREGCMP)
	int flag = 0;
#endif

	if (!*str) str = "*";
	if (!(pattern = (char *)malloc(1 + strlen(str) * 2 + 3))) error(NULL);
	pattern[0] = (exceptdot && (*str == '*' || *str == '?')) ? '.' : ' ';
	i = 1;
	pattern[i++] = '^';
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
	if (!(pattern = (char *)realloc(pattern, i))) error(NULL);

	return(pattern);
}

#ifdef	USERE_COMP
extern char *re_comp __P_((CONST char *));
extern int re_exec __P_((CONST char *));
# if	MSDOS
extern int re_ignore_case;
# endif

reg_t *regexp_init(s)
char *s;
{
# if	MSDOS
	re_ignore_case = 1;
# endif
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
# if	MSDOS
	if (regcomp(re, s + 1, REG_EXTENDED | REG_ICASE)) {
# else
	if (regcomp(re, s + 1, REG_EXTENDED)) {
# endif
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
	return(0);
}

# else	/* !USEREGCOMP */

int regexp_free(re)
reg_t *re;
{
	if (re) free(re);
	return(0);
}

#  ifdef	USEREGCMP
extern char *regcmp __P_((char *, int));
extern char *regex __P_((char *, char *));

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
	bank1 = bank2 = NULL;

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
	while ((pwd =getpwent())) {
		if (strncmp(name, pwd -> pw_name, len)) continue;
		ptr = size;
		size += 1 + strlen(pwd -> pw_name) + 1 + 1;
		*matchp = (*matchp)
			? (char *)realloc(*matchp, size)
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
	*eolp = strcatdelim(dir);
	return(dirp);
}

static struct dirent *readnextpath(dirpp, pathp, dir, eolp)
DIR **dirpp;
char **pathp, *dir, **eolp;
{
	struct dirent *dp;

	if ((dp = Xreaddir(*dirpp))) return(dp);
	while (*pathp) {
		Xclosedir(*dirpp);
		if (!(*dirpp = opennextpath(pathp, dir, eolp))) return(NULL);
		if ((dp = Xreaddir(*dirpp))) return(dp);
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
	len = 0;
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
	if (_dospath(path)) {
		dir[0] = path[0];
		dir[1] = path[1];
		dir[2] = '\0';
		path += 2;
		len = 2;
	}
#endif
	if ((file = strrdelim(path, 0))) {
		if (file == path) strcpy(dir + len, _SS_);
		else {
			strncpy(dir + len, path, file - path);
			dir[len + file - path] = '\0';
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
		strcpy(dir + len, ".");
		next = dir;
		file = path;
	}
	len = strlen(file);

	if (!(dirp = opennextpath(&next, dir, &cp))) return(matchno);
	while ((dp = readnextpath(&dirp, &next, dir, &cp))) {
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
		*matchp = (*matchp)
			? (char *)realloc(*matchp, size)
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

	if (!*common) return(NULL);
	if (!(cp = (char *)malloc(strlen(common) + 1))) error(NULL);
	strcpy(cp, common);
	return(cp);
}
#endif	/* _NOCOMPLETE */
