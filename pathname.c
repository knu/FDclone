/*
 *	pathname.c
 *
 *	path name management module
 */

#ifdef	FD
#include "fd.h"
#else
#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#endif

#include "dirent.h"
#include "sysemu.h"
#include "pathname.h"

#ifdef	DEP_URLPATH
#include "url.h"
#endif

#if	MSDOS
#include <process.h>
# ifndef	FD
# include <dos.h>
# include <io.h>
# endif
#endif	/* !MSDOS */
#ifndef	NOUID
#include <pwd.h>
#include <grp.h>
#endif

#ifdef	FD
#define	FDSTR			"FD"
#endif
#if	MSDOS
#define	EXTWIDTH		4
#else
#define	EXTWIDTH		0
#endif
#define	getconstvar(s)		(getenvvar(s, sizeof(s) - 1))

#ifdef	FD
extern int stat2 __P_((CONST char *, struct stat *));
extern int _chdir2 __P_((CONST char *));
extern char *getenv2 __P_((CONST char *));
# ifdef	DEP_DOSLFN
extern char *shortname __P_((CONST char *, char *));
# endif
extern VOID demacroarg __P_((char **));
extern VOID lostcwd __P_((char *));
#else	/* !FD */
#define	_chdir2			Xchdir
# ifdef	NOSYMLINK
# define	stat2		Xstat
# else
static int NEAR stat2 __P_((CONST char *, struct stat *));
# endif
#define	getenv2			(char *)getenv
#endif	/* !FD */

#ifdef	DEBUG
extern char *_mtrace_file;
#endif
#ifdef	FD
extern char *progpath;
#endif

#if	defined (DEP_PATHTOP) || defined (BSPATHDELIM) || defined (FD)
static CONST char *NEAR topdelim __P_((CONST char *, int, int *, int *));
#endif
static CONST char *NEAR topdelim2 __P_((CONST char *, int *));
static char *NEAR getenvvar __P_((CONST char *, int));
static int NEAR setvar __P_((CONST char *, CONST char *, int));
static int NEAR isescape __P_((CONST char *, int, int, int, int));
#ifdef	_NOORIGGLOB
static char *NEAR cnvregexp __P_((CONST char *, int));
#else
static int NEAR _regexp_exec __P_((CONST reg_ex_t *, CONST char *));
#endif
static VOID NEAR addstrbuf __P_((strbuf_t *, CONST char *, int));
static VOID NEAR duplwild __P_((wild_t *, CONST wild_t *));
static VOID NEAR freewild __P_((wild_t *));
static int NEAR _evalwild __P_((int, char ***, wild_t *));
#ifndef	_NOUSEHASH
static int NEAR calchash __P_((CONST char *));
static VOID NEAR inithash __P_((VOID_A));
static hashlist *NEAR newhash __P_((CONST char *, char *, int, hashlist *));
static VOID NEAR freehash __P_((hashlist **));
static hashlist *NEAR findhash __P_((CONST char *, int));
static VOID NEAR rmhash __P_((CONST char *, int));
#endif
static int NEAR isexecute __P_((CONST char *, int, int));
#if	MSDOS
static int NEAR extaccess __P_((char *, CONST char *, int, int));
#endif
#if	!defined (FDSH) && !defined (_NOCOMPLETE)
static int NEAR completefile __P_((CONST char *, int, int, char ***,
		CONST char *, int, int));
static int NEAR completeexe __P_((CONST char *, int, int, char ***));
#endif	/* !FDSH && !_NOCOMPLETE */
static int NEAR addmeta __P_((char *, CONST char *, int));
static int NEAR skipvar __P_((CONST char **, int *, int *, int));
#ifdef	MINIMUMSHELL
static int NEAR skipvarvalue __P_((CONST char *, int *,
		CONST char *, int, int));
static char *NEAR evalshellparam __P_((int, int));
#else	/* !MINIMUMSHELL */
static int NEAR skipvarvalue __P_((CONST char *, int *,
		CONST char *, int, int, int));
static char *NEAR removeword __P_((CONST char *, CONST char *, int, int));
static char **NEAR removevar __P_((char **, CONST char *, int, int));
static char *NEAR evalshellparam __P_((int, int, CONST char *, int, int *));
#endif	/* !MINIMUMSHELL */
static int NEAR replacevar __P_((CONST char *, char **, int, int, int, int));
static char *NEAR insertarg __P_((char *, int, CONST char *, int, int));
static int NEAR evalvar __P_((char **, int, CONST char **, int));
static char *NEAR replacebackquote __P_((char *, int *, char *, int, int));
#ifdef	BASHSTYLE
static VOID replaceifs __P_((char *, int));
#endif
#ifndef	MINIMUMSHELL
static int NEAR evalhome __P_((char **, int, CONST char **));
#endif

CONST char nullstr[] = "";
CONST char rootpath[] = _SS_;
CONST char curpath[] = ".";
CONST char parentpath[] = "..";
char **argvar = NULL;
#ifndef	_NOUSEHASH
hashlist **hashtable = NULL;
#endif
char *(*getvarfunc)__P_((CONST char *, int)) = NULL;
int (*putvarfunc)__P_((char *, int)) = NULL;
int (*getretvalfunc)__P_((VOID_A)) = NULL;
p_id_t (*getpidfunc)__P_((VOID_A)) = NULL;
p_id_t (*getlastpidfunc)__P_((VOID_A)) = NULL;
char *(*getflagfunc)__P_((VOID_A)) = NULL;
int (*checkundeffunc)__P_((CONST char *, CONST char *, int)) = NULL;
VOID_T (*exitfunc)__P_((VOID_A)) = NULL;
char *(*backquotefunc)__P_((CONST char *)) = NULL;
#ifndef	MINIMUMSHELL
char *(*posixsubstfunc)__P_((CONST char *, int *)) = NULL;
#endif
#ifndef	PATHNOCASE
int pathignorecase = 0;
#endif
#ifndef	_NOVERSCMP
int versioncomp = 0;
#endif

static int skipdotfile = 0;
#if	!defined (USEREGCMP) && !defined (USEREGCOMP) && !defined (USERE_COMP)
static char *wildsymbol1 = "?";
static char *wildsymbol2 = "*";
#endif
#ifndef	NOUID
static uidtable *uidlist = NULL;
static int maxuid = 0;
static gidtable *gidlist = NULL;
static int maxgid = 0;
#endif	/* !NOUID */


#ifndef	FD
# ifndef	NOSYMLINK
static int NEAR stat2(path, stp)
CONST char *path;
struct stat *stp;
{
	int duperrno;

	if (Xstat(path, stp) < 0) {
		duperrno = errno;
		if (Xlstat(path, stp) < 0
		|| (stp -> st_mode & S_IFMT) != S_IFLNK) {
			errno = duperrno;
			return(-1);
		}
		stp -> st_mode &= ~S_IFMT;
	}

	return(0);
}
# endif	/* !NOSYMLINK */
#endif	/* !FD */

int getpathtop(s, drvp, typep)
CONST char *s;
int *drvp, *typep;
{
	int n, drv, type;

	n = drv = 0;
	type = PT_NONE;

	if (!s) /*EMPTY*/;
#ifdef	DEP_DOSPATH
	else if ((drv = _dospath(s))) {
		n = 2;
		type = PT_DOS;
	}
#endif
#ifdef	DOUBLESLASH
	else if ((n = isdslash(s))) type = PT_DSLASH;
#endif
#ifdef	DEP_URLPATH
	else if ((n = _urlpath(s, NULL, NULL))) type = PT_URL;
#endif

	if (drvp) *drvp = drv;
	if (typep) *typep = type;

	return(n);
}

#ifdef	DEP_DOSPATH
char *gendospath(path, drive, c)
char *path;
int drive, c;
{
	*(path++) = drive;
	*(path++) = ':';
	if (c) *(path++) = c;
	*path = '\0';

	return(path);
}
#endif	/* DEP_DOSPATH */

#if	defined (DEP_PATHTOP) || defined (BSPATHDELIM) || defined (FD)
static CONST char *NEAR topdelim(s, d, np, typep)
CONST char *s;
int d, *np, *typep;
{
	if (d) {
		*np = getpathtop(s, NULL, typep);
# ifdef	DEP_DOSPATH
		if (*typep == PT_DOS) return(++s);
# endif
	}
	else {
		*np = 0;
		*typep = PT_NONE;
	}

	return(NULL);
}
#endif	/* DEP_PATHTOP || BSPATHDELIM || defined FD */

static CONST char *NEAR topdelim2(s, np)
CONST char *s;
int *np;
{
	int type;

	*np = getpathtop(s, NULL, &type);
	s += *np;

	switch (type) {
		case PT_NONE:
		case PT_DOS:
			if (!*s) return(s);
			break;
		default:
			break;
	}
	if (*s == _SC_ && !s[1]) return(&(s[1]));

	return(NULL);
}

#if	defined (DEP_PATHTOP) || defined (BSPATHDELIM)
char *strdelim(s, d)
CONST char *s;
int d;
{
	CONST char *cp;
	int n, type;

	if ((cp = topdelim(s, d, &n, &type))) return((char *)cp);

	for (s += n; *s; s++) {
		if (*s == _SC_) return((char *)s);
# ifdef	BSPATHDELIM
		if (iskanji1(s, 0)) s++;
# endif
	}

	return(NULL);
}

char *strrdelim(s, d)
CONST char *s;
int d;
{
	CONST char *cp;
	int n, type;

	cp = topdelim(s, d, &n, &type);

	for (s += n; *s; s++) {
		if (*s == _SC_) cp = s;
# ifdef	BSPATHDELIM
		if (iskanji1(s, 0)) s++;
# endif
	}

	return((char *)cp);
}
#endif	/* DEP_PATHTOP || BSPATHDELIM */

#ifdef	FD
char *strrdelim2(s, d, eol)
CONST char *s;
int d;
CONST char *eol;
{
	CONST char *cp;
	int n, type;

	cp = topdelim(s, d, &n, &type);

# ifdef	BSPATHDELIM
	for (s += n; *s && s < eol; s++) {
		if (*s == _SC_) cp = s;
		if (iskanji1(s, 0)) s++;
	}
# else
	s += n;
	for (eol--; eol >= s; eol--) if (*eol == _SC_) return((char *)eol);
# endif

	return((char *)cp);
}
#endif	/* FD */

#ifndef	MINIMUMSHELL
int isdelim(s, ptr)
CONST char *s;
int ptr;
{
# ifdef	BSPATHDELIM
	int i;
# endif

	if (ptr < 0 || s[ptr] != _SC_) return(0);
# ifdef	BSPATHDELIM
	if (--ptr < 0) return(1);
	if (!ptr) return(!iskanji1(s, 0));

	for (i = 0; s[i] && i < ptr; i++) if (iskanji1(s, i)) i++;
	if (!s[i] || i > ptr) return(1);

	return(!iskanji1(s, i));
# else
	return(1);
# endif
}
#endif	/* !MINIMUMSHELL */

char *strcatdelim(s)
char *s;
{
	char *cp, *eol;
	int n;

	eol = &(s[MAXPATHLEN - 1]);
	if ((cp = (char *)topdelim2(s, &n))) return(cp);

	for (s += n; *s; s++) {
		if (*s == _SC_) {
			if (!cp) cp = s;
			continue;
		}
		cp = NULL;
#ifdef	BSPATHDELIM
		if (iskanji1(s, 0)) s++;
#endif
	}

	if (!cp) {
		cp = s;
		if (cp >= eol) return(cp);
		*cp = _SC_;
	}
	*(++cp) = '\0';

	return(cp);
}

char *strcatdelim2(buf, s1, s2)
char *buf;
CONST char *s1, *s2;
{
	CONST char *s;
	char *cp;
	int n, len;

	s = topdelim2(s1, &n);
	if (n > MAXPATHLEN - 1) {
		n = MAXPATHLEN - 1;
		Xstrncpy(buf, s1, n);
		return(&(buf[n]));
	}

	memcpy(buf, s1, n);
	if (s) {
		cp = &(buf[s - s1]);
		memcpy(&(buf[n]), &(s1[n]), cp - &(buf[n]));
	}
	else {
		cp = NULL;
		for (; s1[n]; n++) {
			if (n >= MAXPATHLEN - 1) {
				buf[n] = '\0';
				return(&(buf[n]));
			}
			buf[n] = s1[n];
			if (s1[n] == _SC_) {
				if (!cp) cp = &(buf[n]) + 1;
				continue;
			}
			cp = NULL;
#ifdef	BSPATHDELIM
			if (iskanji1(s1, n)) {
				if (!s1[++n]) break;
				buf[n] = s1[n];
			}
#endif
		}
		if (!cp) {
			cp = &(buf[n]);
			if (n >= MAXPATHLEN - 1) {
				*cp = '\0';
				return(cp);
			}
			*(cp++) = _SC_;
		}
	}
	if (s2) {
		len = MAXPATHLEN - 1 - (cp - buf);
		for (n = 0; s2[n] && n < len; n++) *(cp++) = s2[n];
	}
	*cp = '\0';

	return(cp);
}

int strcatpath(path, cp, name)
char *path, *cp;
CONST char *name;
{
	int len;

	len = strlen(name);
	if (len + (cp - path) >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return(-1);
	}
	Xstrncpy(cp, name, len);

	return(len);
}

#ifndef	PATHNOCASE
int strpathcmp2(s1, s2)
CONST char *s1, *s2;
{
	if (pathignorecase) return(Xstrcasecmp(s1, s2));

	return(strcmp(s1, s2));
}

int strnpathcmp2(s1, s2, n)
CONST char *s1, *s2;
int n;
{
	if (pathignorecase) return(Xstrncasecmp(s1, s2, n));

	return(strncmp(s1, s2, n));
}
#endif	/* !PATHNOCASE */

#ifndef	_NOVERSCMP
int strverscmp2(s1, s2)
CONST char *s1, *s2;
{
	if (versioncomp) return(Xstrverscmp(s1, s2, pathignorecase));

	return(strpathcmp2(s1, s2));
}
#endif	/* !_NOVERSCMP */

#ifdef	FD
char *underpath(path, dir, len)
CONST char *path, *dir;
int len;
{
	char *cp;

	if (len < 0) len = strlen(dir);
	if ((cp = strrdelim2(dir, 1, &(dir[len]))) && !cp[1]) len = cp - dir;
	if (len > 0 && strnpathcmp(path, dir, len)) return(NULL);
	if (path[len] && path[len] != _SC_) return(NULL);

	return((char *)&(path[len]));
}
#endif	/* FD */

static char *NEAR getenvvar(ident, len)
CONST char *ident;
int len;
{
#if	(!defined (FD) && !defined (FDSH)) || !defined (DEP_ORIGSHELL)
	char *cp, *env;
#endif

	if (getvarfunc) return((*getvarfunc)(ident, len));
#if	(defined (FD) || defined (FDSH)) && defined (DEP_ORIGSHELL)
	return(NULL);
#else
	if (len < 0) len = strlen(ident);
	cp = Xstrndup(ident, len);
	env = getenv2(cp);
	Xfree(cp);

	return(env);
#endif
}

static int NEAR setvar(ident, value, len)
CONST char *ident, *value;
int len;
{
	char *cp;
	int ret, vlen;

	vlen = strlen(value);
	cp = Xmalloc(len + 1 + vlen + 1);
	Xstrncpy(cp, ident, len);
	cp[len] = '=';
	Xstrncpy(&(cp[len + 1]), value, vlen);

#if	defined (FD) || defined (FDSH)
	ret = (putvarfunc) ? (*putvarfunc)(cp, len) : -1;
#else
	ret = (putvarfunc) ? (*putvarfunc)(cp, len) : putenv(cp);
#endif
	if (ret < 0) Xfree(cp);

	return(ret);
}

int isidentchar(c)
int c;
{
	return(c == '_' || Xisalpha(c));
}

int isidentchar2(c)
int c;
{
	return(c == '_' || Xisalnum(c));
}

int isdotdir(s)
CONST char *s;
{
	if (s[0] != '.') /*EMPTY*/;
	else if (!s[1]) return(2);
	else if (s[1] != '.') /*EMPTY*/;
	else if (!s[2]) return(1);

	return(0);
}

char *isrootdir(s)
CONST char *s;
{
#ifdef	DEP_DOSPATH
	if (_dospath(s)) s += 2;
#endif
	if (*s != _SC_) return(NULL);

	return((char *)s);
}

#ifndef	MINIMUMSHELL
int isrootpath(s)
CONST char *s;
{
	return((s[0] == _SC_ && !s[1]) ? 1 : 0);
}

VOID copyrootpath(s)
char *s;
{
	*(s++) = _SC_;
	*s = '\0';
}
#endif	/* !MINIMUMSHELL */

VOID copycurpath(s)
char *s;
{
	*(s++) = '.';
	*s = '\0';
}

#ifdef	DOUBLESLASH
int isdslash(s)
CONST char *s;
{
# if	MSDOS || defined (CYGWIN)
	char *cp;

	if (s[0] != _SC_ || s[1] != _SC_ || !s[2] || s[2] == _SC_) return(0);
	if ((cp = strdelim(&(s[2]), 0))) return(cp - s);

	return(strlen(s));
# else
	return((s[0] == _SC_ && s[1] == _SC_) ? 2 : 0);
# endif
}
#endif	/* DOUBLESLASH */

char *getbasename(s)
CONST char *s;
{
	char *cp;

	if ((cp = strrdelim(s, 1))) return(&(cp[1]));

	return((char *)s);
}

char *getshellname(s, loginp, restrictedp)
CONST char *s;
int *loginp, *restrictedp;
{
	if (loginp) *loginp = 0;
	if (restrictedp) *restrictedp = 0;
	if (*s == '-') {
		s++;
		if (loginp) *loginp = 1;
	}
#ifdef	PATHNOCASE
	if (Xtolower(*s) == 'r')
#else
	if (*s == 'r')
#endif
	{
		s++;
		if (restrictedp) *restrictedp = 1;
	}

	return((char *)s);
}

static int NEAR isescape(s, ptr, quote, len, flags)
CONST char *s;
int ptr, quote, len, flags;
{
#ifdef	FAKEESCAPE
	return(0);
#else	/* !FAKEESCAPE */
	if (s[ptr] != PESCAPE || quote == '\'') return(0);

	if (flags & EA_EOLESCAPE) /*EMPTY*/;
	else if (len >= 0) {
		if (ptr + 1 >= len) return(0);
	}
	else {
		if (!s[ptr + 1]) return(0);
	}

# ifdef	BSPATHDELIM
	if (quote == '"') {
		if (!Xstrchr(DQ_METACHAR, s[ptr + 1])) return(0);
	}
	else {	/* if (!quote || quote == '`') */
		if (!Xstrchr(METACHAR, s[ptr + 1])) return(0);
	}
# endif

	return(1);
#endif	/* !FAKEESCAPE */
}

#ifdef	_NOORIGGLOB
static char *NEAR cnvregexp(s, len)
CONST char *s;
int len;
{
	char *re;
	ALLOC_T size;
	int i, j, paren;

	if (!*s) {
		s = "*";
		len = 1;
	}
	if (len < 0) len = strlen(s);
	re = c_realloc(NULL, 0, &size);
	paren = j = 0;
	re[j++] = '^';
	for (i = 0; i < len; i++) {
		re = c_realloc(re, j + 2, &size);

		if (paren) {
			if (s[i] == ']') paren = 0;
			re[j++] = s[i];
			continue;
		}
		else if (isescape(s, i, '\0', len, 0)) {
			re[j++] = s[i++];
			re[j++] = s[i];
		}
		else switch (s[i]) {
			case '?':
				re[j++] = '.';
				break;
			case '*':
				re[j++] = '.';
				re[j++] = '*';
				break;
			case '[':
				paren = 1;
				re[j++] = s[i];
				break;
			case '^':
			case '$':
			case '.':
				re[j++] = '\\';
				re[j++] = s[i];
				break;
			default:
				re[j++] = s[i];
				break;
		}
	}
	re[j++] = '$';
	re[j++] = '\0';

	return(re);
}
#endif	/* _NOORIGGLOB */

#ifdef	USERE_COMP
extern char *re_comp __P_((CONST char *));
extern int re_exec __P_((CONST char *));
# ifdef	PATHNOCASE
extern int re_ignore_case;
# endif

reg_ex_t *regexp_init(s, len)
CONST char *s;
int len;
{
	char *new;

# ifdef	PATHNOCASE
	re_ignore_case = 1;
# endif
	skipdotfile = (*s == '*' || *s == '?' || *s == '[');
	new = cnvregexp(s, len);
# ifdef	DEBUG
	_mtrace_file = "re_comp(start)";
	re_comp(new);
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "re_comp(end)";
		malloc(0);	/* dummy alloc */
	}
# else
	re_comp(new);
# endif
	Xfree(new);

	return((reg_ex_t *)1);
}

/*ARGSUSED*/
int regexp_exec(re, s, fname)
CONST reg_ex_t *re;
CONST char *s;
int fname;
{
	if (fname && skipdotfile && *s == '.') return(0);

	return(re_exec(s) > 0);
}

/*ARGSUSED*/
VOID regexp_free(re)
reg_ex_t *re;
{
	return;
}

#else	/* !USERE_COMP */
# ifdef	USEREGCOMP

reg_ex_t *regexp_init(s, len)
CONST char *s;
int len;
{
	reg_ex_t *re;
	char *new;
	int n;

	skipdotfile = (*s == '*' || *s == '?' || *s == '[');
	s = new = cnvregexp(s, len);
	re = (reg_ex_t *)Xmalloc(sizeof(reg_ex_t));
# ifdef	PATHNOCASE
	n = regcomp(re, s, REG_EXTENDED | REG_ICASE);
# else
	n = regcomp(re, s, REG_EXTENDED | (pathignorecase) ? REG_ICASE : 0);
# endif
	if (n) {
		Xfree(re);
		re = NULL;
	}
	Xfree(new);

	return(re);
}

int regexp_exec(re, s, fname)
CONST reg_ex_t *re;
CONST char *s;
int fname;
{
	if (!re || (fname && skipdotfile && *s == '.')) return(0);

	return(!regexec(re, s, 0, NULL, 0));
}

VOID regexp_free(re)
reg_ex_t *re;
{
	if (re) {
		regfree(re);
		Xfree(re);
	}
}

# else	/* !USEREGCOMP */
#  ifdef	USEREGCMP
extern char *regcmp __P_((CONST char *, int));
extern char *regex __P_((CONST char *, CONST char *, ...));

reg_ex_t *regexp_init(s, len)
CONST char *s;
int len;
{
	reg_ex_t *re;
	char *new;

	skipdotfile = (*s == '*' || *s == '?' || *s == '[');
	new = cnvregexp(s, len);
	re = regcmp(new, 0);
	Xfree(new);

	return(re);
}

int regexp_exec(re, s, fname)
CONST reg_ex_t *re;
CONST char *s;
int fname;
{
	if (!re || (fname && skipdotfile && *s == '.')) return(0);

	return(regex(re, s) ? 1 : 0);
}

VOID regexp_free(re)
reg_ex_t *re;
{
	Xfree(re);
}
#  else	/* !USEREGCMP */

reg_ex_t *regexp_init(s, len)
CONST char *s;
int len;
{
	reg_ex_t *re;
	char *cp, *paren;
	ALLOC_T size;
	int i, j, n, pc, plen, metachar, quote;

	skipdotfile = 0;
	if (len < 0) len = strlen(s);
	re = NULL;
	paren = NULL;
	n = plen = size = 0;
	quote = '\0';
	re = b_realloc(re, n, reg_ex_t);
	re[n] = NULL;
	for (i = 0; i < len; i++) {
		cp = NULL;
		metachar = 0;
		pc = parsechar(&(s[i]), len - i, '\0', 0, &quote, NULL);
		if (pc == PC_OPQUOTE || pc == PC_CLQUOTE) continue;
		else if (pc == PC_ESCAPE) {
			metachar = 1;
			i++;
		}

		if (paren) {
			paren = c_realloc(paren, plen + 1, &size);

			if (quote || metachar) {
#ifdef	BASHSTYLE
	/* bash treats a character quoted by \ in "[]" as a character itself */
				paren[plen++] = PESCAPE;
				if (iswchar(s, i)) {
					paren[plen++] = s[i++];
					paren[plen++] = s[i];
				}
# ifndef	PATHNOCASE
				else if (!pathignorecase) paren[plen++] = s[i];
# endif
				paren[plen++] = Xtoupper(s[i]);
#endif	/* BASHSTYLE */
			}
			else if (s[i] == ']') {
				if (!plen) {
					Xfree(paren);
					regexp_free(re);
					return(NULL);
				}
#ifndef	BASHSTYLE
	/* bash treats "[a-]]" as "[a-]" + "]" */
				if (paren[plen - 1] == '-') {
					paren[plen++] = s[i];
					continue;
				}
#endif
				paren[plen] = '\0';
				cp = Xrealloc(paren, plen + 1);
				paren = NULL;
			}
#ifdef	BASHSTYLE
	/* bash treats "[^abc]" as "[!abc]" */
			else if (!plen && (s[i] == '!' || s[i] == '^'))
				paren[plen++] = '\0';
#else
			else if (!plen && s[i] == '!') paren[plen++] = '\0';
#endif
			else if (s[i] == '-') {
				if (i + 1 >= len) j = '\0';
#ifndef	PATHNOCASE
				else if (!pathignorecase) j = s[i + 1];
#endif
				else j = Xtoupper(s[i + 1]);

#ifdef	BASHSTYLE
	/* bash does not allow "[b-a]" */
				if (plen && j && j != ']'
				&& j < paren[plen - 1]) {
					Xfree(paren);
					regexp_free(re);
					return(NULL);
				}
#else
	/* Bourne shell ignores "[b-a]" */
				if (plen && j && j < paren[plen - 1]) {
					do {
						if (i + 1 >= len) break;
						i++;
					} while (s[i + 1] != ']');
				}
	/* Bourne shell does not allow "[-a]" */
				else if ((!plen && j != '-')) {
					Xfree(paren);
					regexp_free(re);
					return(NULL);
				}
#endif
				else paren[plen++] = s[i];
			}
			else if (iswchar(s, i)) {
				paren[plen++] = s[i++];
				paren[plen++] = s[i];
			}
#ifndef	PATHNOCASE
			else if (!pathignorecase) paren[plen++] = s[i];
#endif
			else paren[plen++] = Xtoupper(s[i]);
		}
		else if (!quote && !metachar) switch (s[i]) {
			case '?':
				cp = wildsymbol1;
				if (!n) skipdotfile++;
				break;
			case '*':
				cp = wildsymbol2;
				if (!n) skipdotfile++;
				break;
			case '[':
				paren = c_realloc(NULL, 0, &size);
				plen = 0;
				if (!n) skipdotfile++;
				break;
			default:
				break;
		}

		if (paren) continue;
		if (!cp) {
			cp = Xmalloc(2 + 1);
			j = 0;
			if (iswchar(s, i)) {
				cp[j++] = s[i++];
				cp[j++] = s[i];
			}
#ifndef	PATHNOCASE
			else if (!pathignorecase) cp[j++] = s[i];
#endif
			else cp[j++] = Xtoupper(s[i]);
			cp[j] = '\0';
		}
		re[n++] = cp;
		re = b_realloc(re, n, reg_ex_t);
		re[n] = NULL;
	}
	if (paren) {
		Xfree(paren);
		if (plen) {
			regexp_free(re);
			return(NULL);
		}
		cp = Xstrdup("[");
		re[n++] = cp;
		re = b_realloc(re, n, reg_ex_t);
		re[n] = NULL;
	}

	return((reg_ex_t *)Xrealloc(re, (n + 1) * sizeof(reg_ex_t)));
}

static int NEAR _regexp_exec(re, s)
CONST reg_ex_t *re;
CONST char *s;
{
	int i, n1, n2, c1, c2, beg, rev;

	for (n1 = n2 = 0; re[n1] && s[n2]; n1++, n2++) {
		c1 = (u_char)(s[i = n2]);
		if (iswchar(s, n2)) c1 = (c1 << 8) + (u_char)(s[++n2]);
#ifndef	PATHNOCASE
		else if (!pathignorecase) /*EMPTY*/;
#endif
		else c1 = Xtoupper(c1);

		if (re[n1] == wildsymbol1) continue;
		if (re[n1] == wildsymbol2) {
			if (_regexp_exec(&(re[n1 + 1]), &(s[i]))) return(1);
			n1--;
			continue;
		}

		i = rev = 0;
		if (!re[n1][i]) {
			rev = 1;
			i++;
		}
		beg = -1;
		c2 = '\0';
		for (; re[n1][i]; i++) {
#ifdef	BASHSTYLE
	/* bash treats a character quoted by \ in "[]" as a character itself */
			if (re[n1][i] == PESCAPE && re[n1][i + 1]) i++;
	/* bash treats "[a-]]" as "[a-]" + "]" */
			else if (re[n1][i] == '-' && re[n1][i + 1] && c2)
#else
			if (re[n1][i] == '-' && c2)
#endif
			{
				beg = c2;
				i++;
			}
			c2 = (u_char)(re[n1][i]);
			if (iswchar(re[n1], i))
				c2 = (c2 << 8) + (u_char)(re[n1][++i]);
			if (beg >= 0) {
				if (beg && c1 >= beg && c1 <= c2) break;
				beg = -1;
			}
			else if (c1 == c2) break;
		}
		if (rev) {
			if (re[n1][i]) return(0);
		}
		else {
			if (!re[n1][i]) return(0);
		}
	}
	while (re[n1] == wildsymbol2) n1++;
	if (re[n1] || s[n2]) return(0);

	return(1);
}

int regexp_exec(re, s, fname)
CONST reg_ex_t *re;
CONST char *s;
int fname;
{
	if (!re || (fname && skipdotfile && *s == '.')) return(0);

	return(_regexp_exec(re, s));
}

VOID regexp_free(re)
reg_ex_t *re;
{
	int i;

	if (re) {
		for (i = 0; re[i]; i++)
			if (re[i] != wildsymbol1 && re[i] != wildsymbol2)
				Xfree(re[i]);
		Xfree(re);
	}
}
#  endif	/* !USEREGCMP */
# endif		/* !USEREGCOMP */
#endif		/* !USERE_COMP */

static VOID NEAR addstrbuf(sp, s, len)
strbuf_t *sp;
CONST char *s;
int len;
{
	sp -> s = c_realloc(sp -> s, sp -> len + len, &(sp -> size));
	memcpy(&(sp -> s[sp -> len]), s, len);
	sp -> len += len;
	sp -> s[sp -> len] = '\0';
}

static VOID NEAR duplwild(dst, src)
wild_t *dst;
CONST wild_t *src;
{
	memcpy((char *)dst, (char *)src, sizeof(wild_t));
	dst -> fixed.s = Xmalloc(src -> fixed.size);
	memcpy(dst -> fixed.s, src -> fixed.s, src -> fixed.len + 1);
	dst -> path.s = Xmalloc(src -> path.size);
	memcpy(dst -> path.s, src -> path.s, src -> path.len + 1);

#ifndef	NODIRLOOP
	if (src -> ino) {
		dst -> ino = (devino_t *)Xmalloc(src -> nino
			* sizeof(devino_t));
		memcpy((char *)(dst -> ino), (char *)(src -> ino),
			src -> nino * sizeof(devino_t));
	}
#endif	/* !NODIRLOOP */
}

static VOID NEAR freewild(wp)
wild_t *wp;
{
	Xfree(wp -> fixed.s);
	Xfree(wp -> path.s);
#ifndef	NODIRLOOP
	Xfree(wp -> ino);
#endif
}

static int NEAR _evalwild(argc, argvp, wp)
int argc;
char ***argvp;
wild_t *wp;
{
#if	defined (DOUBLESLASH) || defined (DEP_URLPATH)
	int prefix;
#endif
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	reg_ex_t *re;
	wild_t dupl;
	ALLOC_T flen, plen;
	CONST char *cp;
	char *new;
	int i, n, w, pc, quote, isdir;

	if (!*(wp -> s)) {
		if (!(wp -> fixed.len)) return(argc);
		*argvp = (char **)Xrealloc(*argvp,
			(argc + 2) * sizeof(char *));
		(*argvp)[argc++] = wp -> fixed.s;
		wp -> fixed.s = NULL;
		return(argc);
	}

	flen = wp -> fixed.len;
	plen = wp -> path.len;
	quote = wp -> quote;

	if (wp -> fixed.len) addstrbuf(&(wp -> path), rootpath, 1);

#if	defined (DOUBLESLASH) || defined (DEP_URLPATH)
	if (wp -> path.len) prefix = 0;
# ifdef	DOUBLESLASH
	else if ((prefix = isdslash(wp -> s))) {
		addstrbuf(&(wp -> fixed), wp -> s, prefix);
		addstrbuf(&(wp -> path), wp -> s, prefix);
		wp -> s += prefix;
		wp -> type = WT_DOUBLESLASH;
	}
# endif
# ifdef	DEP_URLPATH
	else if ((prefix = isurl(wp -> s, 0))) {
		addstrbuf(&(wp -> fixed), wp -> s, prefix);
		addstrbuf(&(wp -> path), wp -> s, prefix);
		wp -> s += prefix;
		wp -> type = WT_URLPATH;
	}
# endif
#endif	/* DOUBLESLASH || DEP_URLPATH */

	for (i = w = 0; wp -> s[i] && wp -> s[i] != _SC_; i++) {
		pc = parsechar(&(wp -> s[i]), -1,
			'\0', 0, &(wp -> quote), NULL);
		if (pc == PC_OPQUOTE || pc == PC_CLQUOTE) {
			if (!(wp -> flags & EA_STRIPQ))
				addstrbuf(&(wp -> fixed), &(wp -> s[i]), 1);
			continue;
		}
		else if (pc == PC_WCHAR) {
			addstrbuf(&(wp -> fixed), &(wp -> s[i]), 1);
			addstrbuf(&(wp -> path), &(wp -> s[i]), 1);
			i++;
		}
		else if (pc == PC_ESCAPE) {
			if (wp -> flags & EA_KEEPESCAPE)
				addstrbuf(&(wp -> fixed), &(wp -> s[i]), 1);
			if (wp -> s[i + 1] == _SC_) continue;

			if (wp -> quote == '\''
			|| (wp -> quote == '"'
			&& !Xstrchr(DQ_METACHAR, wp -> s[i + 1]))) {
				if (!(wp -> flags & EA_KEEPESCAPE))
					addstrbuf(&(wp -> fixed),
						&(wp -> s[i]), 1);
				addstrbuf(&(wp -> path), &(wp -> s[i]), 1);
			}
			i++;
		}
		else if (pc == PC_NORMAL && Xstrchr("?*[", wp -> s[i])) {
			if (w >= 0 && wp -> s[i] == '*') w++;
			else w = -1;
		}

		addstrbuf(&(wp -> fixed), &(wp -> s[i]), 1);
		addstrbuf(&(wp -> path), &(wp -> s[i]), 1);
	}

	if (!(wp -> s[i])) isdir = 0;
	else {
		isdir = 1;
		addstrbuf(&(wp -> fixed), rootpath, 1);
	}

	if (!w) {
		if (wp -> path.len <= plen) w++;
#if	defined (DOUBLESLASH) || defined (DEP_URLPATH)
		else if (prefix || wp -> type == WT_URLPATH) w++;
#endif
		else if (stat2(wp -> path.s, &st) < 0) return(argc);

		wp -> s += i;
		if (isdir) {
			if (!w && (st.st_mode & S_IFMT) != S_IFDIR)
				return(argc);
			(wp -> s)++;
		}

#ifndef	NODIRLOOP
		if (!w) {
			wp -> ino = (devino_t *)Xrealloc(wp -> ino,
				(wp -> nino + 1) * sizeof(devino_t));
			wp -> ino[wp -> nino].dev = st.st_dev;
			wp -> ino[(wp -> nino)++].ino = st.st_ino;
		}
#endif

		return(_evalwild(argc, argvp, wp));
	}

	if (w != 2 || !isdir || strcmp(&(wp -> path.s[plen]), "**")) w = -1;
	wp -> fixed.len = flen;
	wp -> path.len = plen;
	wp -> fixed.s[flen] = wp -> path.s[plen] = '\0';

	if (w > 0) {
		duplwild(&dupl, wp);
		dupl.s += i + 1;
		argc = _evalwild(argc, argvp, &dupl);
		freewild(&dupl);
		re = NULL;
	}
	else {
		new = Xmalloc(i + 2);
		n = 0;
		if (quote) new[n++] = quote;
		memcpy(&(new[n]), wp -> s, i);
		n += i;
		if (wp -> quote) new[n++] = wp -> quote;
		re = regexp_init(new, n);
		Xfree(new);
		if (!re) return(argc);
		wp -> s += i + 1;
	}

	if (wp -> path.len) cp = wp -> path.s;
	else if (wp -> fixed.len) cp = rootpath;
	else cp = curpath;

	if (!(dirp = Xopendir(cp))) {
		regexp_free(re);
		return(argc);
	}
	if (wp -> path.len || wp -> fixed.len)
		addstrbuf(&(wp -> path), rootpath, 1);

	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;

		duplwild(&dupl, wp);
		n = strlen(dp -> d_name);
		addstrbuf(&(dupl.fixed), dp -> d_name, n);
		addstrbuf(&(dupl.path), dp -> d_name, n);

		if (isdir) {
			if (re) n = regexp_exec(re, dp -> d_name, 1);
			else n = (*(dp -> d_name) == '.') ? 0 : 1;

			if (!n || Xstat(dupl.path.s, &st) < 0
			|| (st.st_mode & S_IFMT) != S_IFDIR) {
				freewild(&dupl);
				continue;
			}

#ifndef	NODIRLOOP
			if (!re) {
				for (n = 0; n < dupl.nino; n++)
					if (dupl.ino[n].dev == st.st_dev
					&& dupl.ino[n].ino == st.st_ino)
						break;
				if (n < dupl.nino) {
					freewild(&dupl);
					continue;
				}
			}

			dupl.ino = (devino_t *)Xrealloc(dupl.ino,
				(dupl.nino + 1) * sizeof(devino_t));
			dupl.ino[dupl.nino].dev = st.st_dev;
			dupl.ino[(dupl.nino)++].ino = st.st_ino;
#endif

			addstrbuf(&(dupl.fixed), rootpath, 1);
			argc = _evalwild(argc, argvp, &dupl);
		}
		else if (regexp_exec(re, dp -> d_name, 1)) {
			*argvp = (char **)Xrealloc(*argvp,
				(argc + 2) * sizeof(char *));
			(*argvp)[argc++] = dupl.fixed.s;
			dupl.fixed.s = NULL;
		}

		freewild(&dupl);
	}
	VOID_C Xclosedir(dirp);
	regexp_free(re);

	return(argc);
}

int cmppath(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	return(strverscmp2(*((char **)vp1), *((char **)vp2)));
}

char **evalwild(s, flags)
CONST char *s;
int flags;
{
	wild_t w;
	char **argv;
	int argc;

	argv = (char **)Xmalloc(1 * sizeof(char *));
	w.s = s;
	w.fixed.s = c_realloc(NULL, 0, &(w.fixed.size));
	w.path.s = c_realloc(NULL, 0, &(w.path.size));
	w.fixed.len = w.path.len = (ALLOC_T)0;
	w.quote = '\0';
#ifndef	NODIRLOOP
	w.nino = 0;
	w.ino = NULL;
#endif
#if	defined (DOUBLESLASH) || defined (DEP_URLPATH)
	w.type = WT_NORMAL;
#endif
	w.flags = flags;

	argc = _evalwild(0, &argv, &w);
	freewild(&w);

	if (!argc) {
		Xfree(argv);
		return(NULL);
	}

	argv[argc] = NULL;
	if (argc > 1) qsort(argv, argc, sizeof(char *), cmppath);

	return(argv);
}

#ifndef	_NOUSEHASH
static int NEAR calchash(s)
CONST char *s;
{
	u_int n;
	int i;

	for (i = n = 0; s[i]; i++) n = n * 12345 + (u_char)(s[i]);

	return(n % MAXHASH);
}

static VOID NEAR inithash(VOID_A)
{
	int i;

	if (hashtable) return;
	hashtable = (hashlist **)Xmalloc(MAXHASH * sizeof(hashlist *));
	for (i = 0; i < MAXHASH; i++) hashtable[i] = NULL;
}

static hashlist *NEAR newhash(com, path, cost, next)
CONST char *com;
char *path;
int cost;
hashlist *next;
{
	hashlist *new;

	new = (hashlist *)Xmalloc(sizeof(hashlist));
	new -> command = Xstrdup(com);
	new -> path = path;
	new -> hits = 0;
	new -> cost = cost;
	new -> next = next;

	return(new);
}

static VOID NEAR freehash(htable)
hashlist **htable;
{
	hashlist *hp, *next;
	int i;

	if (!htable) {
		if (!hashtable) return;
		htable = hashtable;
	}
	for (i = 0; i < MAXHASH; i++) {
		next = NULL;
		for (hp = htable[i]; hp; hp = next) {
			next = hp -> next;
			Xfree(hp -> command);
			Xfree(hp -> path);
			Xfree(hp);
		}
		htable[i] = NULL;
	}
	Xfree(htable);
	if (htable == hashtable) hashtable = NULL;
}

hashlist **duplhash(htable)
hashlist **htable;
{
	CONST hashlist *hp;
	hashlist **nextp, **new;
	int i;

	if (!htable) return(NULL);
	new = (hashlist **)Xmalloc(MAXHASH * sizeof(hashlist *));

	for (i = 0; i < MAXHASH; i++) {
		*(nextp = &(new[i])) = NULL;
		for (hp = htable[i]; hp; hp = hp -> next) {
			*nextp = newhash(hp -> command,
				Xstrdup(hp -> path), hp -> cost, NULL);
			(*nextp) -> type = hp -> type;
			(*nextp) -> hits = hp -> hits;
			nextp = &((*nextp) -> next);
		}
	}

	return(new);
}

static hashlist *NEAR findhash(com, n)
CONST char *com;
int n;
{
	hashlist *hp;

	for (hp = hashtable[n]; hp; hp = hp -> next)
		if (!strpathcmp(com, hp -> command)) return(hp);

	return(NULL);
}

static VOID NEAR rmhash(com, n)
CONST char *com;
int n;
{
	hashlist *hp, *prev;

	prev = NULL;
	for (hp = hashtable[n]; hp; hp = hp -> next) {
		if (!strpathcmp(com, hp -> command)) {
			if (!prev) hashtable[n] = hp -> next;
			else prev -> next = hp -> next;
			Xfree(hp -> command);
			Xfree(hp -> path);
			Xfree(hp);
			return;
		}
		prev = hp;
	}
}
#endif	/* !_NOUSEHASH */

static int NEAR isexecute(path, dirok, exe)
CONST char *path;
int dirok, exe;
{
	struct stat st;
	int d;

	if (stat2(path, &st) < 0) return(-1);
	d = ((st.st_mode & S_IFMT) == S_IFDIR);
	if (!exe) return(d);
	if (d) return(dirok ? d : -1);

	return(Xaccess(path, X_OK));
}

#if	MSDOS
static int NEAR extaccess(path, ext, len, exe)
char *path;
CONST char *ext;
int len, exe;
{
	if (ext) {
		if (isexecute(path, 0, exe) >= 0) {
			if (!(strpathcmp(ext, EXTCOM))) return(0);
			else if (!(strpathcmp(ext, EXTEXE))) return(CM_EXE);
			else if (!(strpathcmp(ext, EXTBAT))) return(CM_BATCH);
		}
	}
	else {
		path[len++] = '.';
		Xstrcpy(&(path[len]), EXTCOM);
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT);
		Xstrcpy(&(path[len]), EXTEXE);
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT | CM_EXE);
		Xstrcpy(&(path[len]), EXTBAT);
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT | CM_BATCH);
	}

	return(-1);
}
#endif	/* MSDOS */

int searchhash(hpp, com, search)
hashlist **hpp;
CONST char *com, *search;
{
#ifndef	_NOUSEHASH
	hashlist *hp;
	int n, recalc, duperrno;
#endif
#if	MSDOS
	char *ext;
#endif
	CONST char *cp, *next;
	char *tmp, *path;
	int len, dlen, cost, size, ret;

#ifndef	_NOUSEHASH
	if (!hpp || (!com && !search)) {
		duperrno = errno;
		if (!com && !search) freehash(hpp);
		if (!hpp && hashtable) for (n = 0; n < MAXHASH; n++) {
			for (hp = hashtable[n]; hp; hp = hp -> next)
				if (hp -> type & CM_RECALC)
					hp -> type |= CM_REHASH;
		}
		errno = duperrno;
		return(CM_NOTFOUND);
	}
#endif	/* !_NOUSEHASH */

#if	MSDOS
	if ((ext = Xstrrchr(com, '.')) && strdelim(++ext, 0)) ext = NULL;
#endif
	if (strdelim(com, 1)) {
#if	MSDOS
		len = strlen(com);
		path = Xmalloc(len + EXTWIDTH + 1);
		Xstrcpy(path, com);
		ret = extaccess(path, ext, len, 0);
		Xfree(path);
		if (ret >= 0) return(ret | CM_FULLPATH);
#else
		if (isexecute(com, 0, 0) >= 0) return(CM_FULLPATH);
#endif
		return(CM_NOTFOUND | CM_FULLPATH);
	}

#ifndef	_NOUSEHASH
	if (!hashtable) inithash();
	n = calchash(com);
	if ((*hpp = findhash(com, n))) {
		if (!((*hpp) -> type & CM_REHASH)
		&& isexecute((*hpp) -> path, 0, 1) >= 0)
			return((*hpp) -> type);
		rmhash(com, n);
	}
#endif

#ifdef	CWDINPATH
	len = strlen(com);
	path = Xmalloc(len + 2 + EXTWIDTH + 1);
	path[0] = '.';
	path[1] = _SC_;
	Xstrcpy(&(path[2]), com);
# if	MSDOS
	if ((ret = extaccess(path, ext, len + 2, 1)) < 0) Xfree(path);
# else
	if ((ret = isexecute(path, 0, 0)) < 0) Xfree(path);
# endif
	else {
# ifdef	_NOUSEHASH
		*hpp = (hashlist *)path;
# else
		*hpp = newhash(com, path, 0, hashtable[n]);
		hashtable[n] = *hpp;
		(*hpp) -> type = (ret |= (CM_HASH | CM_RECALC));
# endif
		return(ret);
	}
#endif	/* CWDINPATH */

#ifndef	_NOUSEHASH
	recalc = 0;
#endif
	if ((next = (search) ? search : getconstvar(ENVPATH))) {
		len = strlen(com);
		size = ret = 0;
		path = NULL;
		cost = 1;
		for (cp = next; cp; cp = next) {
			next = cp;
#ifdef	DEP_DOSPATH
			if (_dospath(cp)) next += 2;
#endif
#ifndef	_NOUSEHASH
			if (*next != _SC_) recalc = CM_RECALC;
#endif
			next = Xstrchr(next, PATHDELIM);
			dlen = (next) ? (next++) - cp : strlen(cp);
			if (!dlen) tmp = NULL;
			else {
				tmp = _evalpath(cp, &(cp[dlen]), 0);
				dlen = strlen(tmp);
			}
			if (dlen + len + 1 + EXTWIDTH + 1 > size) {
				size = dlen + len + 1 + EXTWIDTH + 1;
				path = Xrealloc(path, size);
			}
			if (tmp) {
				Xstrncpy(path, tmp, dlen);
				Xfree(tmp);
				dlen = strcatdelim(path) - path;
			}
			Xstrncpy(&(path[dlen]), com, len);
			dlen += len;
#if	MSDOS
			if ((ret = extaccess(path, ext, dlen, 1)) >= 0) break;
#else
			if (isexecute(path, 0, 1) >= 0) break;
#endif
			cost++;
		}
		if (cp) {
#ifdef	_NOUSEHASH
			*hpp = (hashlist *)path;
#else
			*hpp = newhash(com, path, cost, hashtable[n]);
			hashtable[n] = *hpp;
			(*hpp) -> type = (ret |= (CM_HASH | recalc));
#endif
			return(ret);
		}
		Xfree(path);
	}

	return(CM_NOTFOUND);
}

#ifdef	FD
char *searchexecpath(path, search)
CONST char *path, *search;
{
	hashlist *hp;
	int type;

	if ((type = searchhash(&hp, path, search)) & CM_NOTFOUND) return(NULL);
	if (type & CM_FULLPATH) return(Xstrdup(path));
# ifdef	_NOUSEHASH
	return((char *)hp);
# else
	return(Xstrdup(hp -> path));
# endif
}
#endif	/* FD */

#if	!defined (FDSH) && !defined (_NOCOMPLETE)
int addcompletion(s, cp, argc, argvp)
CONST char *s;
char *cp;
int argc;
char ***argvp;
{
	int n;

	if (!s) {
		if (!cp) return(argc);
		s = cp;
	}

	for (n = 0; n < argc; n++)
		if (!strpathcmp(s, (*argvp)[n])) break;

	if (n < argc) Xfree(cp);
	else {
		*argvp = (char **)Xrealloc(*argvp, ++argc * sizeof(char *));
		(*argvp)[n] = (cp) ? cp : Xstrdup(s);
	}

	return(argc);
}

# ifndef	NOUID
#  ifndef	NOGETPWENT
int completeuser(name, len, argc, argvp, home)
CONST char *name;
int len, argc;
char ***argvp;
int home;
{
	struct passwd *pwd;
	char *new;
	ALLOC_T size;

	len = strlen(name);
#   ifdef	DEBUG
	_mtrace_file = "setpwent(start)";
	setpwent();
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "setpwent(end)";
		malloc(0);	/* dummy alloc */
	}
	for (;;) {
		_mtrace_file = "getpwent(start)";
		pwd = getpwent();
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getpwent(end)";
			malloc(0);	/* dummy alloc */
		}
		if (!pwd) break;
		if (strnpathcmp(name, pwd -> pw_name, len)) continue;
		if (!home) new = Xstrdup(pwd -> pw_name);
		else {
			size = strlen(pwd -> pw_name);
			new = Xmalloc(size + 2 + 1);
			new[0] = '~';
			VOID_C strcatdelim2(&(new[1]), pwd -> pw_name, NULL);
		}
		argc = addcompletion(NULL, new, argc, argvp);
	}
	_mtrace_file = "endpwent(start)";
	endpwent();
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "endpwent(end)";
		malloc(0);	/* dummy alloc */
	}
#   else	/* !DEBUG */
	setpwent();
	while ((pwd = getpwent())) {
		if (strnpathcmp(name, pwd -> pw_name, len)) continue;
		if (!home) new = Xstrdup(pwd -> pw_name);
		else {
			size = strlen(pwd -> pw_name);
			new = Xmalloc(size + 2 + 1);
			new[0] = '~';
			VOID_C strcatdelim2(&(new[1]), pwd -> pw_name, NULL);
		}
		argc = addcompletion(NULL, new, argc, argvp);
	}
	endpwent();
#   endif	/* !DEBUG */

	return(argc);
}
#  endif	/* !NOGETPWENT */

#  ifndef	NOGETGRENT
int completegroup(name, len, argc, argvp)
CONST char *name;
int len, argc;
char ***argvp;
{
	struct group *grp;

	len = strlen(name);
#   ifdef	DEBUG
	_mtrace_file = "setgrent(start)";
	setgrent();
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "setgrent(end)";
		malloc(0);	/* dummy alloc */
	}
	for (;;) {
		_mtrace_file = "getgrent(start)";
		grp = getgrent();
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getgrent(end)";
			malloc(0);	/* dummy alloc */
		}
		if (!grp) break;
		if (strnpathcmp(name, grp -> gr_name, len)) continue;
		argc = addcompletion(grp -> gr_name, NULL, argc, argvp);
	}
	_mtrace_file = "endgrent(start)";
	endgrent();
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "endgrent(end)";
		malloc(0);	/* dummy alloc */
	}
#   else	/* !DEBUG */
	setgrent();
	while ((grp = getgrent())) {
		if (strnpathcmp(name, grp -> gr_name, len)) continue;
		argc = addcompletion(grp -> gr_name, NULL, argc, argvp);
	}
	endgrent();
#   endif	/* !DEBUG */

	return(argc);
}
#  endif	/* !NOGETGRENT */
# endif	/* !NOUID */

static int NEAR completefile(file, len, argc, argvp, dir, dlen, exe)
CONST char *file;
int len, argc;
char ***argvp;
CONST char *dir;
int dlen, exe;
{
	DIR *dirp;
	struct dirent *dp;
	char *cp, *new, path[MAXPATHLEN];
	int d, size, dirok;

	if (dlen >= MAXPATHLEN - 2) return(argc);
	Xstrncpy(path, dir, dlen);
	if (!(dirp = Xopendir(path))) return(argc);
	cp = strcatdelim(path);

	dirok = (exe <= 1) ? 1 : 0;
	while ((dp = Xreaddir(dirp))) {
		if ((!len && isdotdir(dp -> d_name))
		|| strnpathcmp(file, dp -> d_name, len))
			continue;
		size = strcatpath(path, cp, dp -> d_name);
		if (size < 0) continue;

		if (isdotdir(dp -> d_name)) d = 1;
		else if ((d = isexecute(path, dirok, exe)) < 0) continue;

		new = Xmalloc(size + 1 + 1);
		Xstrncpy(new, dp -> d_name, size);
		if (d) new[size++] = _SC_;
		new[size] = '\0';
		argc = addcompletion(NULL, new, argc, argvp);
	}
	VOID_C Xclosedir(dirp);

	return(argc);
}

static int NEAR completeexe(file, len, argc, argvp)
CONST char *file;
int len, argc;
char ***argvp;
{
	char *cp, *tmp, *next;
	int dlen;

# ifdef	CWDINPATH
	argc = completefile(file, len, argc, argvp, curpath, 1, 2);
# endif
	if (!(next = getconstvar(ENVPATH))) return(argc);
	for (cp = next; cp; cp = next) {
# ifdef	DEP_DOSPATH
		if (_dospath(cp)) next = Xstrchr(&(cp[2]), PATHDELIM);
		else
# endif
		next = Xstrchr(cp, PATHDELIM);
		dlen = (next) ? (next++) - cp : strlen(cp);
		tmp = _evalpath(cp, &(cp[dlen]), 0);
		dlen = strlen(tmp);
		argc = completefile(file, len, argc, argvp, tmp, dlen, 2);
		Xfree(tmp);
	}

	return(argc);
}

int completepath(path, len, argc, argvp, exe)
CONST char *path;
int len, argc;
char ***argvp;
int exe;
{
# ifdef	DEP_DOSPATH
	char cwd[4];
# endif
# if	defined (DOUBLESLASH) || defined (DEP_URLPATH)
	int n;
# endif
	CONST char *dir, *file;
	int dlen;

	dlen = 0;
# ifdef	DEP_DOSPATH
	if (_dospath(path)) dlen = 2;
# endif
	dir = &(path[dlen]);

	if ((file = strrdelim(dir, 0))) {
		if (file == dir) {
			dlen++;
			file++;
		}
# ifdef	DOUBLESLASH
		else if ((n = isdslash(path)) && file < &(path[n])) {
			if (path[n - 1] != _SC_) return(argc);
			dlen = n;
			file = &(path[n]);
			if (*file == _SC_) file++;
		}
# endif
# ifdef	DEP_URLPATH
		else if ((n = _urlpath(path, NULL, NULL)) && file < &(path[n]))
			return(argc);
# endif
		else dlen += file++ - dir;
		return(completefile(file, strlen(file), argc, argvp,
			path, dlen, exe));
	}
# if	!defined (NOUID) && !defined (NOGETPWENT)
	else if (*path == '~')
		return(completeuser(&(path[1]), len - 1, argc, argvp, 1));
# endif
# ifdef	DEP_DOSPATH
	else if (dir != path) {
		len -= dlen;
		dlen = gendospath(cwd, *path, '.') - cwd;
		return(completefile(dir, len, argc, argvp, cwd, dlen, exe));
	}
# endif
	else if (exe) return(completeexe(path, len, argc, argvp));

	return(completefile(path, len, argc, argvp, curpath, 1, exe));
}

char *findcommon(argc, argv)
int argc;
char *CONST *argv;
{
	char *common;
	int i, n;

	if (!argv || !argv[0]) return(NULL);
	common = Xstrdup(argv[0]);

	for (n = 1; n < argc; n++) {
		for (i = 0; common[i]; i++) if (common[i] != argv[n][i]) break;
		common[i] = '\0';
	}

	if (!common[0]) {
		Xfree(common);
		return(NULL);
	}

	return(common);
}
#endif	/* !FDSH && !_NOCOMPLETE */

static int NEAR addmeta(s1, s2, flags)
char *s1;
CONST char *s2;
int flags;
{
	int i, j, c;

	if (!s2) return(0);
	if (flags & EA_INQUOTE) flags |= EA_NOEVALQ;
	for (i = j = 0; s2[i]; i++, j++) {
		c = '\0';
		if (s2[i] == '\'' && !(flags & EA_NOEVALQ)) c = PESCAPE;
		else if (s2[i] == '"' && !(flags & EA_NOEVALDQ)) c = PESCAPE;
#ifndef	FAKEESCAPE
		else if (s2[i] == PESCAPE && !(flags & EA_STRIPESCAPE))
			c = PESCAPE;
#endif
		else if (iswchar(s2, i)) c = s2[i++];

		if (c) {
			if (s1) s1[j] = c;
			j++;
		}
		if (s1) s1[j] = s2[i];
	}

	return(j);
}

char *catvar(argv, delim)
char *CONST *argv;
int delim;
{
	char *cp;
	int i, len;

	if (!argv) return(NULL);
	for (i = len = 0; argv[i]; i++) len += strlen(argv[i]);
	if (i < 1) return(Xstrdup(nullstr));
	len += (delim) ? i - 1 : 0;
	cp = Xmalloc(len + 1);
	len = Xstrcpy(cp, argv[0]) - cp;
	for (i = 1; argv[i]; i++) {
		if (delim) cp[len++] = delim;
		len = Xstrcpy(&(cp[len]), argv[i]) - cp;
	}

	return(cp);
}

int countvar(var)
char *CONST *var;
{
	int i;

	if (!var) return(0);
	for (i = 0; var[i]; i++) /*EMPTY*/;

	return(i);
}

VOID freevar(var)
char **var;
{
	int i, duperrno;

	duperrno = errno;
	if (var) {
		for (i = 0; var[i]; i++) Xfree(var[i]);
		Xfree(var);
	}
	errno = duperrno;
}

char **duplvar(var, margin)
char *CONST *var;
int margin;
{
	char **dupl;
	int i, n;

	if (margin < 0) {
		if (!var) return(NULL);
		margin = 0;
	}
	n = countvar(var);
	dupl = (char **)Xmalloc((n + margin + 1) * sizeof(char *));
	for (i = 0; i < n; i++) dupl[i] = Xstrdup(var[i]);
	dupl[i] = NULL;

	return(dupl);
}

int parsechar(s, len, spc, flags, qp, pqp)
CONST char *s;
int len, spc, flags, *qp, *pqp;
{
	if (*s == *qp) {
#ifdef	FD
		if (flags & EA_FINDMETA)
			return((*qp == '\'') ? PC_EXMETA : PC_META);
#endif
		if (!pqp) *qp = '\0';
		else {
			*qp = *pqp;
			*pqp = '\0';
		}
		return(PC_CLQUOTE);
	}
	else if (iswchar(s, 0)) return(PC_WCHAR);
	else if (*qp == '\'') return(PC_SQUOTE);
#ifdef	FD
	else if ((flags & EA_FINDMETA) && Xstrchr(DQ_METACHAR, *s))
		return(PC_META);
	else if ((flags & EA_FINDDELIM) && Xstrchr(CMDLINE_DELIM, *s))
		return(PC_DELIM);
#endif
	else if (isescape(s, 0, *qp, len, flags)) return(PC_ESCAPE);
	else if (*qp == '`') return(PC_BQUOTE);
#ifdef	NESTINGQUOTE
	else if ((flags & EA_BACKQ) && *s == '`') {
		if (pqp && *qp) *pqp = *qp;
		*qp = *s;
		return(PC_OPQUOTE);
	}
#endif
	else if (spc && *s == spc) return(*s);
	else if (*qp) return(PC_DQUOTE);
	else if (!(flags & EA_NOEVALQ) && *s == '\'') {
#ifdef	FD
		if (flags & EA_FINDMETA) return(PC_META);
#endif
		*qp = *s;
		return(PC_OPQUOTE);
	}
	else if (!(flags & EA_NOEVALDQ) && *s == '"') {
		*qp = *s;
		return(PC_OPQUOTE);
	}
#ifndef	NESTINGQUOTE
	else if ((flags & EA_BACKQ) && *s == '`') {
		if (pqp && *qp) *pqp = *qp;
		*qp = *s;
		return(PC_OPQUOTE);
	}
#endif
#ifdef	FD
	else if ((flags & EA_FINDMETA) && Xstrchr(METACHAR, *s))
		return(PC_META);
#endif

	return(PC_NORMAL);
}

static int NEAR skipvar(bufp, eolp, ptrp, flags)
CONST char **bufp;
int *eolp, *ptrp, flags;
{
	int i, mode;

	mode = '\0';
	*bufp += *ptrp;
	*ptrp = 0;

	if (isidentchar((*bufp)[*ptrp])) {
		while ((*bufp)[++(*ptrp)])
			if (!isidentchar2((*bufp)[*ptrp])) break;
		if (eolp) *eolp = *ptrp;
		return(mode);
	}

#ifndef	MINIMUMSHELL
	if ((*bufp)[*ptrp] == '(') {
		if ((*bufp)[++(*ptrp)] != '(') {
			if (skipvarvalue(*bufp, ptrp, ")", flags, 0, 1) < 0)
				return(-1);
		}
		else {
			(*ptrp)++;
			if (skipvarvalue(*bufp, ptrp, "))", flags, 0, 1) < 0)
				return(-1);
		}
		if (eolp) *eolp = *ptrp;
		return('(');
	}
#endif

	if ((*bufp)[*ptrp] != '{') {
		if (!(flags & EA_INQUOTE) || (*bufp)[*ptrp] != '"') (*ptrp)++;
		if (eolp) *eolp = *ptrp;
		return(mode);
	}

	(*bufp)++;
#ifndef	MINIMUMSHELL
	if ((*bufp)[*ptrp] == '#' && (*bufp)[*ptrp + 1] != '}') {
		(*bufp)++;
		mode = 'n';
	}
#endif
	if (isidentchar((*bufp)[*ptrp])) {
		while ((*bufp)[++(*ptrp)])
			if (!isidentchar2((*bufp)[*ptrp])) break;
	}
	else (*ptrp)++;

	if (eolp) *eolp = *ptrp;
	if ((i = (*bufp)[(*ptrp)++]) == '}') return(mode);
#ifndef	MINIMUMSHELL
	if (mode) return(-1);
#endif

	if (i == ':') {
		mode = (*bufp)[(*ptrp)++];
		if (!Xstrchr("-=?+", mode)) return(-1);
		mode |= 0x80;
	}
	else if (Xstrchr("-=?+", i)) mode = i;
#ifndef	MINIMUMSHELL
	else if (i != RMSUFFIX && i != '#') return(-1);
	else if ((*bufp)[*ptrp] != i) mode = i;
	else {
		(*ptrp)++;
		mode = i | 0x80;
	}
#endif	/* !MINIMUMSHELL */

#ifdef	BASHSTYLE
	/* bash treats any meta character in ${} as just a character */
# ifdef	MINIMUMSHELL
	if (skipvarvalue(*bufp, ptrp, "}", flags, 0) < 0) return(-1);
# else
	if (skipvarvalue(*bufp, ptrp, "}", flags, 0, 0) < 0) return(-1);
# endif
#else	/* !BASHSTYLE */
# ifdef	MINIMUMSHELL
	if (skipvarvalue(*bufp, ptrp, "}", flags, 1) < 0) return(-1);
# else
	i = (flags & EA_INQUOTE) ? 0 : 1;
	if (skipvarvalue(*bufp, ptrp, "}", flags, i, 0) < 0) return(-1);
# endif
#endif	/* !BASHSTYLE */

	return(mode);
}

#ifdef	MINIMUMSHELL
static int NEAR skipvarvalue(s, ptrp, next, flags, nonl)
CONST char *s;
int *ptrp;
CONST char *next;
int flags, nonl;
#else
static int NEAR skipvarvalue(s, ptrp, next, flags, nonl, nest)
CONST char *s;
int *ptrp;
CONST char *next;
int flags, nonl, nest;
#endif
{
#ifdef	NESTINGQUOTE
	int pq;
#endif
	CONST char *cp;
	int pc, q, f, len;

#ifdef	NESTINGQUOTE
	pq = '\0';
#endif
	q = '\0';
	len = strlen(next);
	while (s[*ptrp]) {
		f = flags;
		if (q == '"') f |= EA_INQUOTE;
#ifdef	NESTINGQUOTE
		pc = parsechar(&(s[*ptrp]), -1, '$', EA_BACKQ, &q, &pq);
#else
		pc = parsechar(&(s[*ptrp]), -1, '$', EA_BACKQ, &q, NULL);
#endif
		if (pc == PC_WCHAR || pc == PC_ESCAPE) (*ptrp)++;
		else if (pc == '$') {
			if (!s[++(*ptrp)]) return(0);
			cp = s;
			if (skipvar(&cp, NULL, ptrp, f) < 0) return(-1);
			*ptrp += (cp - s);
			continue;
		}
		else if (pc != PC_NORMAL) /*EMPTY*/;
		else if (nonl && s[*ptrp] == '\n') return(-1);
#ifndef	MINIMUMSHELL
		else if (nest && s[*ptrp] == '(') {
			(*ptrp)++;
			if (skipvarvalue(s, ptrp, ")", f, nonl, nest) < 0)
				return(-1);
			continue;
		}
#endif
		else if (!strncmp(&(s[*ptrp]), next, len)) {
			*ptrp += len;
			return(0);
		}

		(*ptrp)++;
	}

	return(-1);
}

#ifndef	MINIMUMSHELL
static char *NEAR removeword(s, pattern, plen, mode)
CONST char *s, *pattern;
int plen, mode;
{
	reg_ex_t *re;
	CONST char *cp;
	char *ret, *tmp, *new;
	int c, n, len;

	if (!s || !*s) return(NULL);
	tmp = Xstrndup(pattern, plen);
	new = evalarg(tmp, EA_BACKQ);
	Xfree(tmp);
	re = regexp_init(new, -1);
	Xfree(new);
	if (!re) return(NULL);
	ret = NULL;
	len = strlen(s);
	n = -1;
	if ((mode & ~0x80) != '#') {
		if (mode & 0x80) for (cp = s; cp < &(s[len]); cp++) {
			if (regexp_exec(re, cp, 0)) {
				n = cp - s;
				break;
			}
		}
		else for (cp = &(s[len - 1]); cp >= s; cp--) {
			if (regexp_exec(re, cp, 0)) {
				n = cp - s;
				break;
			}
		}
		if (n >= 0) ret = Xstrndup(s, n);
	}
	else {
		new = Xstrdup(s);
		if (mode & 0x80)
		for (tmp = &(new[len]); tmp >= new; tmp--) {
			*tmp = '\0';
			if (regexp_exec(re, new, 0)) {
				n = tmp - new;
				break;
			}
		}
		else for (tmp = &(new[1]); tmp <= &(new[len]); tmp++) {
			c = *tmp;
			*tmp = '\0';
			if (regexp_exec(re, new, 0)) {
				n = tmp - new;
				break;
			}
			*tmp = c;
		}
		if (n >= 0) ret = Xstrdup(&(s[n]));
		Xfree(new);
	}
	regexp_free(re);

	return(ret);
}

static char **NEAR removevar(var, pattern, plen, mode)
char **var;
CONST char *pattern;
int plen, mode;
{
	char *cp, **new;
	int i, n;

	n = countvar(var);
	new = (char **)Xmalloc((n + 1) * sizeof(char *));
	for (i = 0; i < n; i++) {
		if ((cp = removeword(var[i], pattern, plen, mode)))
			new[i] = cp;
		else new[i] = Xstrdup(var[i]);
	}
	new[i] = NULL;

	return(new);
}
#endif	/* !MINIMUMSHELL */

#ifdef	MINIMUMSHELL
static char *NEAR evalshellparam(c, flags)
int c, flags;
#else
static char *NEAR evalshellparam(c, flags, pattern, plen, modep)
int c, flags;
CONST char *pattern;
int plen, *modep;
#endif
{
#ifndef	MINIMUMSHELL
	char **new;
#endif
	char *cp, **arglist, tmp[MAXLONGWIDTH + 1];
	p_id_t pid;
	int i, j, sp;

	cp = NULL;
#ifndef	MINIMUMSHELL
	new = NULL;
#endif
	switch (c) {
		case '@':
			if (!(arglist = argvar)) break;
#ifndef	MINIMUMSHELL
			if (modep) {
				new = removevar(arglist,
					pattern, plen, *modep);
				arglist = new;
				*modep = '\0';
			}
#endif
			sp = ' ';
			if (!(flags & EA_INQUOTE)) {
				cp = catvar(&(arglist[1]), sp);
				break;
			}
#ifdef	BASHSTYLE
	/* bash uses IFS instead of a space as a separator */
			if ((cp = getconstvar(ENVIFS)) && *cp)
				sp = *cp;
#endif

			for (i = j = 0; arglist[i + 1]; i++)
				j += addmeta(NULL, arglist[i + 1], flags);
			if (i <= 0) cp = Xstrdup(nullstr);
			else {
				j += (i - 1) * 3;
				cp = Xmalloc(j + 1);
				j = addmeta(cp, arglist[1], flags);
				for (i = 2; arglist[i]; i++) {
					cp[j++] = '"';
					cp[j++] = sp;
					cp[j++] = '"';
					j += addmeta(&(cp[j]), arglist[i],
						flags);
				}
				cp[j] = '\0';
			}
			break;
		case '*':
			if (!(arglist = argvar)) break;
#ifndef	MINIMUMSHELL
			if (modep) {
				new = removevar(arglist,
					pattern, plen, *modep);
				arglist = new;
				*modep = '\0';
			}
#endif
			sp = ' ';
#ifdef	BASHSTYLE
	/* bash uses IFS instead of a space as a separator */
			if ((flags & EA_INQUOTE) && (cp = getconstvar(ENVIFS)))
				sp = (*cp) ? *cp : '\0';
#endif
			cp = catvar(&(arglist[1]), sp);
			break;
		case '#':
			if (!argvar) break;
			VOID_C Xsnprintf(tmp, sizeof(tmp),
				"%d", countvar(argvar + 1));
			cp = tmp;
			break;
		case '?':
			VOID_C Xsnprintf(tmp, sizeof(tmp),
				"%d",
				(getretvalfunc) ? (*getretvalfunc)() : 0);
			cp = tmp;
			break;
		case '$':
			if (!getpidfunc) pid = getpid();
			else pid = (*getpidfunc)();
			VOID_C Xsnprintf(tmp, sizeof(tmp), "%id", pid);
			cp = tmp;
			break;
		case '!':
			if (getlastpidfunc
			&& (pid = (*getlastpidfunc)()) >= 0) {
				VOID_C Xsnprintf(tmp, sizeof(tmp), "%id", pid);
				cp = tmp;
			}
			break;
		case '-':
			if (getflagfunc) cp = (*getflagfunc)();
			break;
		default:
			break;
	}
	if (cp == tmp) cp = Xstrdup(tmp);
#ifndef	MINIMUMSHELL
	freevar(new);
#endif

	return(cp);
}

static int NEAR replacevar(arg, cpp, s, len, vlen, mode)
CONST char *arg;
char **cpp;
int s, len, vlen, mode;
{
	char *val;
	int i;

	if (!mode) return(0);
	if (mode == '+') {
		if (!*cpp) return(0);
	}
	else if (*cpp) return(0);
	else if (mode == '=' && !isidentchar(*arg)) return(-1);

	val = Xstrndup(&(arg[s]), vlen);
	*cpp = evalarg(val,
		(mode == '=' || mode == '?')
		? EA_STRIPQ | EA_BACKQ : EA_BACKQ);
	Xfree(val);
	if (!*cpp) return(-1);

	if (mode == '=') {
#ifdef	FD
		demacroarg(cpp);
#endif
		if (setvar(arg, *cpp, len) < 0) {
			Xfree(*cpp);
			*cpp = NULL;
			return(-1);
		}
#ifdef	BASHSTYLE
	/* bash does not evaluates a quoted string in substitution itself */
		Xfree(*cpp);
		val = Xstrndup(&(arg[s]), vlen);
		*cpp = evalarg(val, EA_BACKQ);
		Xfree(val);
		if (!*cpp) return(-1);
# ifdef	FD
		demacroarg(cpp);
# endif
#endif
	}
	else if (mode == '?') {
#ifdef	FD
		demacroarg(cpp);
#endif
		for (i = 0; i < len; i++) Xfputc(arg[i], Xstderr);
		VOID_C Xfprintf(Xstderr,
			": %k",
			(vlen > 0) ? *cpp : "parameter null or not set");
		VOID_C fputnl(Xstderr);
		Xfree(*cpp);
		*cpp = NULL;
		if (exitfunc) (*exitfunc)();
		return(-2);
	}

	return(mode);
}

static char *NEAR insertarg(buf, ptr, arg, olen, nlen)
char *buf;
int ptr;
CONST char *arg;
int olen, nlen;
{
	if (nlen <= olen) return(buf);
	buf = Xrealloc(buf, ptr + nlen + (int)strlen(arg) - olen + 1);

	return(buf);
}

static int NEAR evalvar(bufp, ptr, argp, flags)
char **bufp;
int ptr;
CONST char **argp;
int flags;
{
#ifndef	MINIMUMSHELL
	char tmp[MAXLONGWIDTH + 1];
#endif
	CONST char *arg, *top;
	char *cp, *new;
	int i, c, n, len, vlen, s, nul, mode;

	new = NULL;
	arg = *argp;
	top = &(arg[1]);
	n = 0;
	if ((mode = skipvar(&top, &s, &n, flags)) < 0) return(-1);

	*argp = &(top[n - 1]);
	if (!(len = s)) {
		(*bufp)[ptr++] = '$';
		return(ptr);
	}

	nul = (mode & 0x80);
	if (n > 0 && top[n - 1] == '}') n--;
	if (!mode) vlen = 0;
	else {
		s++;
		if (nul) s++;
		vlen = n - s;
	}

#ifdef	MINIMUMSHELL
	mode &= ~0x80;
#else
	if (Xstrchr("-=?+", mode & ~0x80)) mode &= ~0x80;
	else if (mode == '(') {
		if (!posixsubstfunc) return(-1);
		for (;;) {
			if ((new = (*posixsubstfunc)(top, &n))) break;
			if (n < 0 || !top[n]) return(-1);
			if (skipvarvalue(top, &n, ")", flags, 0, 1) < 0)
				return(-1);
			*argp = top + n - 1;
		}
		mode = '\0';
	}
	else if (mode == 'n') {
		if (len == 1 && (*top == '*' || *top == '@')) {
			top--;
			mode = '\0';
		}
	}
	else nul = -1;
#endif	/* !MINIMUMSHELL */

	c = '\0';
	if ((cp = new)) /*EMPTY*/;
	else if (isidentchar(*top)) cp = getenvvar(top, len);
	else if (Xisdigit(*top)) {
		if (len > 1) return(-1);
		if (!argvar) {
			(*bufp)[ptr++] = '$';
			(*bufp)[ptr++] = *top;
			return(ptr);
		}
		i = *top - '0';
		if (i < countvar(argvar)) cp = argvar[i];
	}
	else if (len == 1) {
		c = *top;
#ifdef	MINIMUMSHELL
		cp = evalshellparam(c, flags);
#else
		cp = evalshellparam(c, flags,
			&(top[s]), vlen, (nul < 0) ? &mode : NULL);
#endif
		if (cp) new = cp;
		else {
			(*bufp)[ptr++] = '$';
			(*bufp)[ptr++] = *top;
			return(ptr);
		}
	}
	else return(-1);

	if (!mode && checkundeffunc && (*checkundeffunc)(cp, top, len) < 0)
		return(-1);
	if (cp && (nul == 0x80) && !*cp) cp = NULL;

#ifndef	MINIMUMSHELL
	if (mode == 'n') {
		VOID_C Xsnprintf(tmp, sizeof(tmp),
			"%d", (cp) ? strlen(cp) : 0);
		cp = tmp;
	}
	else if (nul == -1) {
		new = removeword(cp, top + s, vlen, mode);
		if (new) cp = new;
	}
	else
#endif	/* !MINIMUMSHELL */
	if ((mode = replacevar(top, &cp, s, len, vlen, mode)) < 0) {
		Xfree(new);
		return(mode);
	}
	else if (mode) {
		Xfree(new);
		new = cp;
	}

	if (!mode && (c != '@' || !(flags & EA_INQUOTE))) {
		vlen = addmeta(NULL, cp, flags);
		*bufp = insertarg(*bufp, ptr, arg, *argp - arg + 1, vlen);
		addmeta(&((*bufp)[ptr]), cp, flags);
	}
	else if (!cp) vlen = 0;
	else if (c == '@' && !*cp && (flags & EA_INQUOTE)
	&& ptr > 0 && (*bufp)[ptr - 1] == '"' && *(*argp + 1) == '"') {
		vlen = 0;
		ptr--;
		(*argp)++;
	}
	else {
		vlen = strlen(cp);
		*bufp = insertarg(*bufp, ptr, arg, *argp - arg + 1, vlen);
		Xstrncpy(&((*bufp)[ptr]), cp, vlen);
	}

	ptr += vlen;
	Xfree(new);

	return(ptr);
}

#ifndef	NOUID
VOID getlogininfo(homep, shellp)
CONST char **homep, **shellp;
{
	struct passwd *pwd;

	if (homep) *homep = NULL;
	if (shellp) *shellp = NULL;
# ifdef	DEBUG
	_mtrace_file = "getpwuid(start)";
	pwd = getpwuid(getuid());
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "getpwuid(end)";
		malloc(0);	/* dummy alloc */
	}
# else
	pwd = getpwuid(getuid());
# endif
	if (!pwd) return;
	if (homep && pwd -> pw_dir && *(pwd -> pw_dir)) *homep = pwd -> pw_dir;
	if (shellp && pwd -> pw_shell && *(pwd -> pw_shell))
		*shellp = pwd -> pw_shell;
}

uidtable *finduid(uid, name)
u_id_t uid;
CONST char *name;
{
	struct passwd *pwd;
	int i;

	if (name) {
		for (i = 0; i < maxuid; i++)
			if (!strpathcmp(name, uidlist[i].name))
				return(&(uidlist[i]));
# ifdef	DEBUG
		_mtrace_file = "getpwnam(start)";
		pwd = getpwnam(name);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getpwnam(end)";
			malloc(0);	/* dummy alloc */
		}
# else
		pwd = getpwnam(name);
# endif
	}
	else {
		for (i = 0; i < maxuid; i++)
			if (uid == uidlist[i].uid) return(&(uidlist[i]));
# ifdef	DEBUG
		_mtrace_file = "getpwuid(start)";
		pwd = getpwuid((uid_t)uid);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getpwuid(end)";
			malloc(0);	/* dummy alloc */
		}
# else
		pwd = getpwuid((uid_t)uid);
# endif
	}

	if (!pwd) return(NULL);
	uidlist = b_realloc(uidlist, maxuid, uidtable);
	uidlist[maxuid].uid = convuid(pwd -> pw_uid);
	uidlist[maxuid].name = Xstrdup(pwd -> pw_name);
	uidlist[maxuid].home = Xstrdup(pwd -> pw_dir);

	return(&(uidlist[maxuid++]));
}

gidtable *findgid(gid, name)
g_id_t gid;
CONST char *name;
{
	struct group *grp;
	int i;

	if (name) {
		for (i = 0; i < maxgid; i++)
			if (!strpathcmp(name, gidlist[i].name))
				return(&(gidlist[i]));
# ifdef	DEBUG
		_mtrace_file = "getgrnam(start)";
		grp = getgrnam(name);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getgrnam(end)";
			malloc(0);	/* dummy alloc */
		}
# else
		grp = getgrnam(name);
# endif
	}
	else {
		for (i = 0; i < maxgid; i++)
			if (gid == gidlist[i].gid) return(&(gidlist[i]));
# ifdef	DEBUG
		_mtrace_file = "getgrgid(start)";
		grp = getgrgid((gid_t)gid);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getgrgid(end)";
			malloc(0);	/* dummy alloc */
		}
# else
		grp = getgrgid((gid_t)gid);
# endif
	}

	if (!grp) return(NULL);
	gidlist = b_realloc(gidlist, maxgid, gidtable);
	gidlist[maxgid].gid = convgid(grp -> gr_gid);
	gidlist[maxgid].name = Xstrdup(grp -> gr_name);
# ifndef	USEGETGROUPS
	gidlist[maxgid].gr_mem = duplvar(grp -> gr_mem, -1);
# endif
	gidlist[maxgid].ismem = 0;

	return(&(gidlist[maxgid++]));
}

int isgroupmember(gid)
g_id_t gid;
{
# ifdef	USEGETGROUPS
	gid_t *gidset;
	int n;
# else
	uidtable *up;
# endif
	gidtable *gp;
	int i;

	if (!(gp = findgid(gid, NULL))) return(0);
	if (!(gp -> ismem)) {
		gp -> ismem++;
# ifdef	USEGETGROUPS
		if ((n = getgroups(0, NULL)) > 0) {
			gidset = (gid_t *)Xmalloc(n * sizeof(*gidset));
			n = getgroups(n, gidset);
			for (i = 0; i < n; i++) {
				if (gidset[i] != (gid_t)(gp -> gid)) continue;
				gp -> ismem++;
				break;
			}
			Xfree(gidset);
		}
# else	/* !USEGETGROUPS */
		if (gp -> gr_mem && (up = finduid(geteuid(), NULL)))
		for (i = 0; gp -> gr_mem[i]; i++) {
			if (!strpathcmp(up -> name, gp -> gr_mem[i])) {
				gp -> ismem++;
				break;
			}
		}
		freevar(gp -> gr_mem);
		gp -> gr_mem = NULL;
# endif	/* !USEGETGROUPS */
	}

	return(gp -> ismem - 1);
}

# ifdef	DEBUG
VOID freeidlist(VOID_A)
{
	int i;

	if (uidlist) {
		for (i = 0; i < maxuid; i++) {
			Xfree(uidlist[i].name);
			Xfree(uidlist[i].home);
		}
		Xfree(uidlist);
	}
	if (gidlist) {
		for (i = 0; i < maxgid; i++) {
			Xfree(gidlist[i].name);
#  ifndef	USEGETGROUPS
			freevar(gidlist[i].gr_mem);
#  endif
		}
		Xfree(gidlist);
	}
	uidlist = NULL;
	gidlist = NULL;
	maxuid = maxgid = 0;
}
# endif
#endif	/* !NOUID */

static char *NEAR replacebackquote(buf, ptrp, bbuf, rest, flags)
char *buf;
int *ptrp;
char *bbuf;
int rest, flags;
{
	char *tmp;
	int len, size;

	stripquote(bbuf, EA_BACKQ);
	if (!(tmp = (*backquotefunc)(bbuf))) return(buf);
	len = addmeta(NULL, tmp, flags);
	size = *ptrp + len + rest + 1;
	buf = Xrealloc(buf, size);
	addmeta(&(buf[*ptrp]), tmp, flags);
	*ptrp += len;
	Xfree(tmp);

	return(buf);
}

#ifndef	MINIMUMSHELL
CONST char *gethomedir(VOID_A)
{
# ifndef	NOUID
#  ifdef	FD
	uidtable *up;
#  endif
# endif	/* !NOUID */
	CONST char *cp;

	if (!(cp = getconstvar(ENVHOME))) {
# ifndef	NOUID
#  ifdef	FD
		if ((up = finduid(getuid(), NULL))) cp = up -> home;
#  else
		getlogininfo(&cp, NULL);
#  endif
# endif	/* !NOUID */
	}

	return(cp);
}
#endif	/* !MINIMUMSHELL */

CONST char *getrealpath(path, resolved, cwd)
CONST char *path;
char *resolved, *cwd;
{
#ifdef	DEP_PSEUDOPATH
	int drv;
#endif
	CONST char *err;
	char buf[MAXPATHLEN];

	if (cwd) /*EMPTY*/;
	else if (!Xgetwd(buf)) return(NULL);
	else cwd = buf;

#ifdef	DEP_PSEUDOPATH
	if ((drv = preparedrv(cwd, NULL, NULL)) < 0) return(cwd);
#endif
	if (_chdir2(path) < 0) err = path;
	else if (!Xgetwd(resolved)) err = NULL;
	else if (_chdir2(cwd) < 0) {
#ifdef	FD
		lostcwd(NULL);
		err = resolved;
#else
		err = cwd;
#endif
	}
	else err = resolved;
#ifdef	DEP_PSEUDOPATH
	shutdrv(drv);
#endif

	return(err);
}

#ifdef	BASHSTYLE
static VOID replaceifs(s, len)
char *s;
int len;
{
	CONST char *ifs;
	int i;

	if (!(ifs = getconstvar(ENVIFS))) return;
	for (i = 0; i < len; i++) {
		if (Xstrchr(ifs, s[i]) && !Xstrchr(IFS_SET, s[i])) s[i] = ' ';
		else if (iswchar(s, i)) i++;
	}
}
#endif	/* BASHSTYLE */

#ifndef	MINIMUMSHELL
static int NEAR evalhome(bufp, ptr, argp)
char **bufp;
int ptr;
CONST char **argp;
{
# ifndef	NOUID
#  ifdef	FD
	uidtable *up;
#  else
	struct passwd *pwd;
#  endif
	char *tmp;
# endif	/* !NOUID */
	CONST char *cp, *top;
	int len, vlen;

	top = &((*argp)[1]);

	len = ((cp = strdelim(top, 0))) ? (cp - top) : strlen(top);
	if (!len) cp = gethomedir();
# ifdef	FD
	else if (len == strsize(FDSTR) && !strnpathcmp(top, FDSTR, len))
		cp = progpath;
# endif
	else {
# ifdef	NOUID
		cp = NULL;
# else	/* !NOUID */
		tmp = Xstrndup(top, len);
#  ifdef	FD
		up = finduid(0, tmp);
		cp = (up) ? up -> home : NULL;
#  else	/* !FD */
#   ifdef	DEBUG
		_mtrace_file = "getpwnam(start)";
		pwd = getpwnam(tmp);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getpwnam(end)";
			malloc(0);	/* dummy alloc */
		}
#   else
		pwd = getpwnam(tmp);
#   endif
		cp = (pwd) ? pwd -> pw_dir : NULL;
#  endif	/* !FD */
		Xfree(tmp);
# endif	/* !NOUID */
	}

	if (!cp) {
		vlen = len = 0;
		(*bufp)[ptr++] = '~';
	}
	else {
		top = cp;
		vlen = strlen(top);
		*bufp = insertarg(*bufp, ptr, *argp, len + 1, vlen);
	}
	Xstrncpy(&((*bufp)[ptr]), top, vlen);
	ptr += vlen;
	*argp += len;

	return(ptr);
}
#endif	/* !MINIMUMSHELL */

char *evalarg(arg, flags)
char *arg;
int flags;
{
#ifdef	DEP_DOSLFN
	char path[MAXPATHLEN], alias[MAXPATHLEN];
#endif
#ifndef	MINIMUMSHELL
	int q2, prev;
#endif
#ifdef	NESTINGQUOTE
	int pq;
#endif
#ifdef	BASHSTYLE
	int top;
#endif
	CONST char *cp;
	char *buf, *bbuf;
	int i, j, pc, q, f;

#ifdef	DEP_DOSLFN
	if (*arg == '"' && (i = strlen(arg)) > 2 && arg[i - 1] == '"') {
		Xstrncpy(path, &(arg[1]), i - 2);
		if (shortname(path, alias) == alias) {
			if (flags & (EA_STRIPQ | EA_STRIPQLATER))
				return(Xstrdup(alias));
			i = strlen(alias);
			buf = Xmalloc(i + 2 + 1);
			buf[0] = '"';
			memcpy(&(buf[1]), alias, i);
			buf[++i] = '"';
			buf[++i] = '\0';
			return(buf);
		}
	}
#endif

	i = strlen(arg) + 1;
	buf = Xmalloc(i);
	if (!backquotefunc) flags &= ~EA_BACKQ;
	bbuf = (flags & EA_BACKQ) ? Xmalloc(i) : NULL;
#ifndef	MINIMUMSHELL
	q2 = prev =
#endif
#ifdef	NESTINGQUOTE
	pq =
#endif
	q = '\0';
	i = j = 0;
	cp = arg;

	while (*cp) {
#ifdef	BASHSTYLE
		top = i;
#endif
#ifdef	NESTINGQUOTE
		pc = parsechar(cp, -1, '$', flags, &q, &pq);
#else
		pc = parsechar(cp, -1, '$', flags, &q, NULL);
#endif
		if (pc == PC_CLQUOTE) {
			if (*cp == '`') {
				bbuf[j] = '\0';
				buf = replacebackquote(buf, &i,
					bbuf, strlen(&(cp[1])), flags);
#ifdef	BASHSTYLE
	/* bash replaces the IFS character to a space */
				if (flags & EA_EVALIFS)
					replaceifs(&(buf[top]), i - top);
#endif
				j = 0;
			}
			else if (!(flags & EA_STRIPQ)) buf[i++] = *cp;
		}
		else if (pc == PC_WCHAR) {
			if (q == '`') {
				bbuf[j++] = *cp++;
				bbuf[j++] = *cp;
			}
			else {
				buf[i++] = *cp++;
				buf[i++] = *cp;
			}
		}
		else if (pc == PC_BQUOTE) {
			bbuf[j++] = *cp;
#ifndef	MINIMUMSHELL
			parsechar(cp, -1, '\0', flags, &q2, NULL);
#endif
		}
		else if (pc == PC_SQUOTE || pc == PC_DQUOTE) buf[i++] = *cp;
		else if (pc == '$') {
			f = flags;
			if (q == '"') f |= EA_INQUOTE;
			if (!cp[1]) buf[i++] = *cp;
#ifdef	FAKEESCAPE
# ifdef	MINIMUMSHELL
			else if (cp[1] == '$') cp++;
# else
			else if (cp[1] == '$' || cp[1] == '~') cp++;
# endif
#endif	/* FAKEESCAPE */
			else if ((i = evalvar(&buf, i, &cp, f)) < 0) {
				Xfree(bbuf);
				Xfree(buf);
				if (i < -1) *arg = '\0';
				return(NULL);
			}
#ifdef	BASHSTYLE
	/* bash replaces the IFS character to a space */
			else if (flags & EA_EVALIFS)
				replaceifs(&(buf[top]), i - top);
#endif
		}
		else if (pc == PC_ESCAPE) {
			cp++;
			if (flags & EA_KEEPESCAPE) pc = PC_NORMAL;
			else if (*cp == '$') /*EMPTY*/;
			else if ((flags & EA_BACKQ) && *cp == '`') /*EMPTY*/;
			else if ((flags & EA_STRIPQ)
			&& (*cp == '\'' || *cp == '"'))
				/*EMPTY*/;
			else if ((flags & EA_STRIPESCAPE) && *cp == PESCAPE)
				/*EMPTY*/;
			else pc = PC_NORMAL;

			if (q == '`') {
				if (*cp == '$') /*EMPTY*/;
#ifndef	MINIMUMSHELL
				else if (q2 == '\'' && *cp == PESCAPE)
					/*EMPTY*/;
#endif
				else bbuf[j++] = PESCAPE;
				bbuf[j++] = *cp;
			}
			else {
				if (pc != PC_ESCAPE) buf[i++] = PESCAPE;
				buf[i++] = *cp;
			}
		}
		else if (pc == PC_OPQUOTE) {
			if (*cp == '`') {
				j = 0;
#ifndef	MINIMUMSHELL
				q2 = '\0';
#endif
			}
			else if (!(flags & EA_STRIPQ)) buf[i++] = *cp;
		}
		else if (pc != PC_NORMAL) /*EMPTY*/;
#ifndef	MINIMUMSHELL
		else if (*cp == '~' && (!prev || prev == ':' || prev == '='))
			i = evalhome(&buf, i, &cp);
#endif
		else buf[i++] = *cp;

#ifdef	MINIMUMSHELL
		cp++;
#else
		prev = *(cp++);
#endif
	}
#ifndef	BASHSTYLE
	/* bash does not allow unclosed quote */
	if ((flags & EA_BACKQ) && q == '`') {
		bbuf[j] = '\0';
		buf = replacebackquote(buf, &i, bbuf, 0, flags);
	}
#endif
	Xfree(bbuf);
	buf[i] = '\0';

	if (flags & EA_STRIPQLATER) stripquote(buf, EA_STRIPQ);

	return(buf);
}

int evalifs(argc, argvp, ifs)
int argc;
char ***argvp;
CONST char *ifs;
{
	char *cp;
	int i, j, n, pc, quote;

	for (n = 0; n < argc; n++) {
		for (i = 0, quote = '\0'; (*argvp)[n][i]; i++) {
			pc = parsechar(&((*argvp)[n][i]), -1,
				'\0', 0, &quote, NULL);
			if (pc == PC_WCHAR || pc == PC_ESCAPE) i++;
			else if (pc != PC_NORMAL) /*EMPTY*/;
			else if (Xstrchr(ifs, (*argvp)[n][i])) {
				for (j = i + 1; (*argvp)[n][j]; j++)
					if (!Xstrchr(ifs, (*argvp)[n][j]))
						break;
				if (!i) {
					for (i = 0; (*argvp)[n][i + j]; i++)
						(*argvp)[n][i] =
							(*argvp)[n][i + j];
					(*argvp)[n][i] = '\0';
					i = -1;
					continue;
				}
				(*argvp)[n][i] = '\0';
				if (!(*argvp)[n][j]) break;
				cp = Xstrdup(&((*argvp)[n][j]));
				*argvp = (char **)Xrealloc(*argvp,
					(argc + 2) * sizeof(char *));
				memmove((char *)(&((*argvp)[n + 2])),
					(char *)(&((*argvp)[n + 1])),
					(argc++ - n) * sizeof(char *));
				(*argvp)[n + 1] = cp;
				break;
			}
		}
		if (!i) {
			Xfree((*argvp)[n--]);
			memmove((char *)(&((*argvp)[n + 1])),
				(char *)(&((*argvp)[n + 2])),
				(argc-- - n) * sizeof(char *));
		}
	}

	return(argc);
}

int evalglob(argc, argvp, flags)
int argc;
char ***argvp;
int flags;
{
	char **wild;
	int i, n;

	for (n = 0; n < argc; n++) {
		if (!(wild = evalwild((*argvp)[n], flags))) {
			stripquote((*argvp)[n], flags);
			continue;
		}

		i = countvar(wild);
		if (i > 1) {
			*argvp = (char **)Xrealloc(*argvp,
				(argc + i) * sizeof(char *));
			memmove((char *)(&((*argvp)[n + i])),
				(char *)(&((*argvp)[n + 1])),
				(argc - n) * sizeof(char *));
			argc += i - 1;
		}
		Xfree((*argvp)[n]);
		memmove((char *)(&((*argvp)[n])),
			(char *)wild, i * sizeof(char *));
		Xfree(wild);
		n += i - 1;
	}

	return(argc);
}

int stripquote(arg, flags)
char *arg;
int flags;
{
	int i, j, pc, quote, stripped;

	stripped = 0;
	if (!arg) return(stripped);
	for (i = j = 0, quote = '\0'; arg[i]; i++) {
		pc = parsechar(&(arg[i]), -1, '\0', 0, &quote, NULL);
		if (pc == PC_OPQUOTE || pc == PC_CLQUOTE) {
			stripped++;
			if (flags & EA_STRIPQ) continue;
		}
		else if (pc == PC_WCHAR) arg[j++] = arg[i++];
		else if (pc == PC_ESCAPE) {
			i++;
			if (flags & EA_KEEPESCAPE) pc = PC_NORMAL;
			else if (!quote && !(flags & EA_BACKQ)) /*EMPTY*/;
			else if (!Xstrchr(DQ_METACHAR, arg[i])) pc = PC_NORMAL;

			if (pc != PC_ESCAPE) arg[j++] = PESCAPE;
			else stripped++;
		}

		arg[j++] = arg[i];
	}
	arg[j] = '\0';

	return(stripped);
}

char *_evalpath(path, eol, flags)
CONST char *path, *eol;
int flags;
{
#ifdef	DEP_DOSLFN
	char alias[MAXPATHLEN];
	int top, len;
#endif
#ifdef	DOUBLESLASH
	int ds;
#endif
#ifdef	DEP_URLPATH
	int url;
#endif
	char *cp, *tmp;
	int i, j, c, pc, size, quote;

	if (eol) i = eol - path;
	else i = strlen(path);
	cp = Xstrndup(path, i);

	tmp = evalarg(cp, EA_NOEVALQ | EA_NOEVALDQ | EA_KEEPESCAPE);
	if (!tmp) {
		*cp = '\0';
		return(cp);
	}
	Xfree(cp);
	cp = tmp;

#ifdef	DEP_DOSLFN
	top = -1;
#endif
#ifdef	DOUBLESLASH
	ds = isdslash(cp);
#endif
#ifdef	DEP_URLPATH
	url = isurl(cp, 0);
#endif
	size = strlen(cp) + 1;
	tmp = Xmalloc(size);
	quote = '\0';

	for (i = j = c = 0; cp[i]; c = cp[i++]) {
		pc = parsechar(&(cp[i]), -1, '\0', 0, &quote, NULL);
		if (pc == PC_CLQUOTE) {
#ifdef	DEP_DOSLFN
			if ((flags & EA_NOEVALQ) && top >= 0 && ++top < j) {
				tmp[j] = '\0';
				if (shortname(&(tmp[top]), alias) == alias) {
					len = strlen(alias);
					size += top + len - j;
					j = top + len;
					tmp = Xrealloc(tmp, size);
					Xstrncpy(&(tmp[top]), alias, len);
				}
			}
			top = -1;
#endif
			if (!(flags & EA_NOEVALQ)) continue;
		}
		else if (pc == PC_WCHAR) tmp[j++] = cp[i++];
		else if (pc == PC_ESCAPE) {
			i++;
			if ((flags & EA_KEEPESCAPE)
			|| (quote && !Xstrchr(DQ_METACHAR, cp[i])))
				tmp[j++] = PESCAPE;
		}
		else if (pc == PC_OPQUOTE) {
#ifdef	DEP_DOSLFN
			if (cp[i] == '"') top = j;
#endif
			if (!(flags & EA_NOEVALQ)) continue;
		}
		else if (pc != PC_NORMAL) /*EMPTY*/;
		else if (flags & EA_NOUNIQDELIM) /*EMPTY*/;
#ifdef	DOUBLESLASH
		else if (ds && i == 1) /*EMPTY*/;
#endif
#ifdef	DEP_URLPATH
		else if (url && i < url) /*EMPTY*/;
#endif
		else if (cp[i] == _SC_ && c == _SC_) continue;

		tmp[j++] = cp[i];
	}
	tmp[j] = '\0';
	Xfree(cp);

	return(tmp);
}

char *evalpath(path, flags)
char *path;
int flags;
{
	char *cp;

	if (!path || !*path) return(path);
	for (cp = path; Xisblank(*cp); cp++) /*EMPTY*/;
	cp = _evalpath(cp, NULL, flags);
	Xfree(path);

	return(cp);
}
