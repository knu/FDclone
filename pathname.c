/*
 *	pathname.c
 *
 *	Path Name Management Module
 */

#ifdef	FD
#include "fd.h"
#else	/* !FD */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "machine.h"

# ifndef	NOUNISTDH
# include <unistd.h>
# endif

# ifndef	NOSTDLIBH
# include <stdlib.h>
# endif
#endif	/* !FD */
#include <ctype.h>

#ifdef	LSI_C
#include <jctype.h>
#define	issjis1(c)	iskanji(c)
#define	issjis2(c)	iskanji2(c)
#endif

#if	MSDOS
#include <process.h>
#include "unixemu.h"
#ifndef	FD
#include <dos.h>
#include <io.h>
#define	DS_IRDONLY	001
#define	DS_IHIDDEN	002
#define	DS_IFSYSTEM	004
#define	DS_IFLABEL	010
#define	DS_IFDIR	020
#define	DS_IARCHIVE	040
#define	SEARCHATTRS	(DS_IRDONLY | DS_IHIDDEN | DS_IFSYSTEM \
			| DS_IFDIR | DS_IARCHIVE)
#endif
#define	EXTWIDTH	4
#define	strpathcmp	stricmp
#define	strnpathcmp	strnicmp
# if defined (DJGPP) && DJGPP < 2
# include <dir.h>
# define	find_t	ffblk
# define	_dos_findfirst(p, a, f)	findfirst(p, f, a)
# define	_dos_findnext		findnext
# define	_ENOENT_		ENMFILE
# else
# define	ff_name			name
# define	_ENOENT_		ENOENT
# endif
#else	/* !MSDOS */
#include <pwd.h>
#include <sys/file.h>
#include <sys/param.h>
#define	EXTWIDTH	0
#define	strpathcmp	strcmp
#define	strnpathcmp	strncmp
# ifdef	USEDIRECT
# include <sys/dir.h>
# define	dirent	direct
# else
# include <dirent.h>
# endif
#endif	/* !MSDOS */

#ifdef	NOVOID
#define	VOID
#define	VOID_P	char *
#else
#define	VOID	void
#define	VOID_P	void *
#endif

#include "pathname.h"

#ifdef	NOERRNO
extern int errno;
#endif
#ifdef	DEBUG
extern char *_mtrace_file;
#endif

#ifdef	FD
extern char *getenv2 __P_((char *));
extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strdup2 __P_((char *));
extern char *strcpy2 __P_((char *, char *));
extern char *strncpy2 __P_((char *, char *, int));
extern int isdotdir __P_((char *));
extern char *catargs __P_((char *[], int));
# ifdef	_NOROCKRIDGE
extern DIR *_Xopendir __P_((char *));
# define	Xopendir	_Xopendir
# else
extern DIR *Xopendir __P_((char *));
# endif
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern int Xstat __P_((char *, struct stat *));
extern int Xaccess __P_((char *, int));
# if	MSDOS || !defined (_NODOSDRIVE)
extern int _dospath __P_((char *));
#endif
# if	MSDOS && !defined (_NOUSELFN)
extern char *shortname __P_((char *, char *));
# endif
extern char *progpath;
#else	/* !FD */
#define	getenv2		(char *)getenv
static VOID allocerror __P_((VOID_A));
char *malloc2 __P_((ALLOC_T));
char *realloc2 __P_((VOID_P, ALLOC_T));
char *strdup2 __P_((char *));
char *strcpy2 __P_((char *, char *));
char *strncpy2 __P_((char *, char *, int));
int isdotdir __P_((char *));
static char *catargs __P_((char *[], int));
# if	MSDOS
DIR *Xopendir __P_((char *));
int Xclosedir __P_((DIR *));
struct dirent *Xreaddir __P_((DIR *));
# else
# define	Xopendir	opendir
# define	Xclosedir	closedir
# define	Xreaddir	readdir
# endif
#define	Xstat(f, s)	(stat(f, s) ? -1 : 0)
#define	Xaccess		access
#define	_dospath(s)	(isalpha(*(s)) && (s)[1] == ':')
#endif	/* !FD */

static char *getvar __P_((char *, int));
static int setvar __P_((char *, char *, int));
#ifdef	_NOORIGGLOB
static char *cnvregexp __P_((char *, int));
#else
static int _regexp_exec __P_((char **, char *));
#endif
static char *catpath __P_((char *, char *, int *, int, int));
#if	MSDOS
static int _evalwild __P_((int, char ***, char *, char *, int));
#else
static int _evalwild __P_((int, char ***, char *, char *, int,
		int, devino_t *));
#endif
#ifndef	_NOUSEHASH
static int calchash __P_((char *));
static VOID inithash __P_((VOID_A));
static hashlist *newhash __P_((char *, char *, int, hashlist *));
static hashlist *findhash __P_((char *, int));
static VOID rmhash __P_((char *, int));
#endif
#if	MSDOS
static int extaccess __P_((char *, char *, int));
#endif
#if	!defined (FDSH) && !defined (_NOCOMPLETE)
# if	!MSDOS
static int completeuser __P_((char *, int, char ***));
# endif
static int completefile __P_((char *, char *, int, int, int, char ***));
static int completeexe __P_((char *, int, char ***));
#endif
static char *evalshellparam __P_((int, int));
static int replacevar __P_((char *, char **, int, int, int, int, int));
static char *insertarg __P_((char *, int, int, int));
static int evalvar __P_((char **, int, int));
static int evalhome __P_((char **, int));

#ifndef	issjis1
#define	issjis1(c)	(((u_char)(c) >= 0x81 && (u_char)(c) <= 0x9f) \
			|| ((u_char)(c) >= 0xe0 && (u_char)(c) <= 0xfc))
#endif
#ifndef	issjis2
#define	issjis2(c)	((u_char)(c) >= 0x40 && (u_char)(c) <= 0x7c \
			&& (u_char)(c) != 0x7f)
#endif

#ifndef	iseuc
#define	iseuc(c)	((u_char)(c) >= 0xa1 && (u_char)(c) <= 0xfe)
#endif

#define	iskna(c)	((u_char)(c) >= 0xa1 && (u_char)(c) <= 0xdf)
#define	isekana(s, i)	((u_char)((s)[i]) == 0x8e && iskna((s)[(i) + 1]))

#ifdef	CODEEUC
#define	iskanji1(s, i)	(iseuc((s)[i]) && iseuc((s)[(i) + 1]))
#else
#define	iskanji1(s, i)	(issjis1((s)[i]) && issjis2((s)[(i) + 1]))
#endif

#define	IFS_SET		" \t\n"
#define	MAXLONGWIDTH	10
#define	BUFUNIT		32
#define	n_size(n)	((((n) / BUFUNIT) + 1) * BUFUNIT)
#define	c_malloc(size)	(malloc2((size) = BUFUNIT))
#define	c_realloc(ptr, n, size) \
			(((n) + 1 < (size)) \
			? (ptr) : realloc2(ptr, (size) *= 2))

static int skipdotfile = 0;
static char *wildsymbol1 = "?";
static char *wildsymbol2 = "*";
#ifndef	_NOUSEHASH
hashlist **hashtable = NULL;
#endif
char *(*getvarfunc)__P_((char *, int)) = NULL;
int (*putvarfunc)__P_((char *, int)) = NULL;
int (*getretvalfunc)__P_((VOID_A)) = NULL;
long (*getlastpidfunc)__P_((VOID_A)) = NULL;
char **(*getarglistfunc)__P_((VOID_A)) = NULL;
char *(*getflagfunc)__P_((VOID_A)) = NULL;
int (*checkundeffunc)__P_((char *, char *, int)) = NULL;
VOID (*exitfunc)__P_((VOID_A)) = NULL;


#ifndef	FD
static VOID allocerror(VOID_A)
{
	fputs("fatal error: memory allocation error\n", stderr);
	fflush(stderr);
	exit(2);
}

char *malloc2(size)
ALLOC_T size;
{
	char *tmp;

	if (!size || !(tmp = (char *)malloc(size))) allocerror();
	return(tmp);
}

char *realloc2(ptr, size)
VOID_P ptr;
ALLOC_T size;
{
	char *tmp;

	if (!size
	|| !(tmp = (ptr) ? (char *)realloc(ptr, size) : (char *)malloc(size)))
		allocerror();
	return(tmp);
}

char *strdup2(s)
char *s;
{
	char *tmp;

	if (!s) return(NULL);
	if (!(tmp = (char *)malloc((ALLOC_T)strlen(s) + 1))) allocerror();
	strcpy(tmp, s);
	return(tmp);
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

int isdotdir(name)
char *name;
{
	if (name[0] == '.'
	&& (!name[1] || (name[1] == '.' && !name[2]))) return(1);
	return(0);
}

static char *catargs(argv, delim)
char *argv[];
int delim;
{
	char *cp;
	int i, len;

	if (!argv) return(NULL);
	for (i = len = 0; argv[i]; i++) len += strlen(argv[i]);
	if (i < 1) return(NULL);
	len += (delim) ? i - 1 : 0;
	cp = malloc2(len + 1);
	len = strcpy2(cp, argv[0]) - cp;
	for (i = 1; argv[i]; i++) {
		if (delim) cp[len++] = delim;
		len = strcpy2(cp + len, argv[i]) - cp;
	}
	return(cp);
}

# if	MSDOS
DIR *Xopendir(dir)
char *dir;
{
	DIR *dirp;
	char *cp;
	int i;

	dirp = (DIR *)malloc2(sizeof(DIR));
	dirp -> dd_off = 0;
	dirp -> dd_buf = malloc2(sizeof(struct find_t));
	dirp -> dd_path = malloc2(strlen(dir) + 1 + 3 + 1);
	cp = strcatdelim2(dirp -> dd_path, dir, NULL);

	dirp -> dd_id = DID_IFNORMAL;
	strcpy(cp, "*.*");
	if (cp - 1 > dir + 3) i = -1;
	else i = _dos_findfirst(dirp -> dd_path, DS_IFLABEL,
		(struct find_t *)(dirp -> dd_buf));
	if (i == 0) dirp -> dd_id = DID_IFLABEL;
	else i = _dos_findfirst(dirp -> dd_path, (SEARCHATTRS | DS_IFLABEL),
		(struct find_t *)(dirp -> dd_buf));

	if (i < 0) {
		if (!errno || errno == _ENOENT_) dirp -> dd_off = -1;
		else {
			free(dirp -> dd_path);
			free(dirp -> dd_buf);
			free(dirp);
			return(NULL);
		}
	}
	return(dirp);
}

int Xclosedir(dirp)
DIR *dirp;
{
	free(dirp -> dd_buf);
	free(dirp -> dd_path);
	free(dirp);
	return(0);
}

struct dirent *Xreaddir(dirp)
DIR *dirp;
{
	static struct dirent d;
	struct find_t *findp;
	int n;

	if (dirp -> dd_off < 0) return(NULL);
	d.d_off = dirp -> dd_off;

	findp = (struct find_t *)(dirp -> dd_buf);
	strcpy(d.d_name, findp -> ff_name);

	if (!(dirp -> dd_id & DID_IFLABEL)) n = _dos_findnext(findp);
	else n = _dos_findfirst(dirp -> dd_path, SEARCHATTRS, findp);
	if (n == 0) dirp -> dd_off++;
	else dirp -> dd_off = -1;
	dirp -> dd_id &= ~DID_IFLABEL;

	return(&d);
}
# endif	/* MSDOS */
#endif	/* !FD */

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
		if (iskanji1(s, i)) i++;
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
		if (iskanji1(s, i)) i++;
#endif
	}
	return(cp);
}
#endif	/* MSDOS || (FD && !_NODOSDRIVE) */

#ifndef	FDSH
char *strrdelim2(s, eol)
char *s, *eol;
{
#if	MSDOS
	char *cp;
	int i;

	cp = NULL;
	for (i = 0; s[i] && &(s[i]) < eol; i++) {
		if (s[i] == _SC_) cp = &(s[i]);
		if (iskanji1(s, i)) i++;
	}
	return(cp);
#else
	for (eol--; eol >= s; eol--) if (*eol == _SC_) return(eol);
	return(NULL);
#endif
}

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
	if (!ptr) return(!iskanji1(s, 0));

	for (i = 0; s[i] && i < ptr; i++) if (iskanji1(s, i)) i++;
	if (!s[i] || i > ptr) return(1);
	return(!iskanji1(s, i));
#else
	return(1);
#endif
}
#endif	/* !FDSH */

char *strcatdelim(s)
char *s;
{
	char *cp;
	int i;

#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
	if (_dospath(s)) i = 2;
	else
#endif
	i = 0;
	if (s[i] == _SC_ && !s[i + 1]) return(&(s[i + 1]));

	cp = NULL;
	for (; s[i]; i++) {
		if (s[i] == _SC_) {
			if (!cp) cp = &(s[i]);
			continue;
		}
		cp = NULL;
#if	MSDOS
		if (iskanji1(s, i)) i++;
#endif
	}
	if (!cp) {
		cp = &(s[i]);
		if (i >= MAXPATHLEN - 1) return(cp);
		*cp = _SC_;
	}
	*(++cp) = '\0';
	return(cp);
}

char *strcatdelim2(buf, s1, s2)
char *buf, *s1, *s2;
{
	char *cp;
	int i, len;

#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
	if (_dospath(s1)) {
		buf[0] = s1[0];
		buf[1] = s1[1];
		i = 2;
	}
	else
#endif
	i = 0;
	if (s1[i] == _SC_ && !s1[i + 1]) *(cp = &(buf[i])) = _SC_;
	else {
		cp = NULL;
		for (; s1[i]; i++) {
			buf[i] = s1[i];
			if (s1[i] == _SC_) {
				if (!cp) cp = &(buf[i]);
				continue;
			}
			cp = NULL;
#if	MSDOS
			if (iskanji1(s1, i)) {
				if (!s1[++i]) break;
				buf[i] = s1[i];
			}
#endif
		}
		if (!cp) {
			cp = &(buf[i]);
			if (i >= MAXPATHLEN - 1) {
				*cp = '\0';
				return(cp);
			}
			*cp = _SC_;
		}
	}
	if (s2) {
		len = MAXPATHLEN - 1 - (cp - buf);
		for (i = 0; s2[i] && i < len; i++) *(++cp) = s2[i];
	}
	*(++cp) = '\0';
	return(cp);
}

static char *getvar(ident, len)
char *ident;
int len;
{
	char *cp, *env;

	if (len < 0) len = strlen(ident);
	if (getvarfunc) return((*getvarfunc)(ident, len));
	cp = malloc2(len + 1);
	strncpy2(cp, ident, len);
	env = getenv2(cp);
	free(cp);
	return(env);
}

static int setvar(ident, value, len)
char *ident, *value;
int len;
{
	char *cp;
	int ret, vlen;

	vlen = strlen(value);
	cp = malloc2(len + 1 + vlen + 1);
	strncpy(cp, ident, len);
	cp[len] = '=';
	strncpy2(cp + len + 1, value, vlen);

	ret = (putvarfunc) ? (*putvarfunc)(cp, len) : putenv(cp);
	if (ret < 0) free(cp);
	return(ret);
}

#ifdef	_NOORIGGLOB
static char *cnvregexp(s, len)
char *s;
int len;
{
	char *re;
	int i, j, paren, size;

	if (!*s) {
		s = "*";
		len = 1;
	}
	if (len < 0) len = strlen(s);
	re = c_malloc(size);
	paren = j = 0;
	re[j++] = '^';
	for (i = 0; i < len; i++) {
		re = c_realloc(re, j + 2, size);

		if (paren) {
			if (s[i] == ']') paren = 0;
			re[j++] = s[i];
			continue;
		}
		else if (isnmeta(s, i, '\0', len)) {
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
# if	MSDOS
extern int re_ignore_case;
# endif

reg_t *regexp_init(s, len)
char *s;
int len;
{
# if	MSDOS
	re_ignore_case = 1;
# endif
	skipdotfile = (*s == '*' || *s == '?');
	s = cnvregexp(s, len);
	re_comp(s);
	free(s);
	return((reg_t *)1);
}

/*ARGSUSED*/
int regexp_exec(re, s, fname)
reg_t *re;
char *s;
int fname;
{
	if (fname && skipdotfile && *s == '.') return(0);
	return(re_exec(s) > 0);
}

/*ARGSUSED*/
VOID regexp_free(re)
reg_t *re;
{
	return;
}

#else	/* !USERE_COMP */
# ifdef	USEREGCOMP

reg_t *regexp_init(s, len)
char *s;
int len;
{
	reg_t *re;

	skipdotfile = (*s == '*' || *s == '?');
	s = cnvregexp(s, len);
	re = (reg_t *)malloc2(sizeof(reg_t));
# if	MSDOS
	if (regcomp(re, s, REG_EXTENDED | REG_ICASE)) {
# else
	if (regcomp(re, s, REG_EXTENDED)) {
# endif
		free(re);
		re = NULL;
	}
	free(s);
	return(re);
}

int regexp_exec(re, s, fname)
reg_t *re;
char *s;
int fname;
{
	if (!re || (fname && skipdotfile && *s == '.')) return(0);
	return(!regexec(re, s, 0, NULL, 0));
}

VOID regexp_free(re)
reg_t *re;
{
	if (re) regfree(re);
}

# else	/* !USEREGCOMP */
#  ifdef	USEREGCMP
extern char *regcmp __P_((char *, int));
extern char *regex __P_((char *, char *));

reg_t *regexp_init(s, len)
char *s;
int len;
{
	reg_t *re;

	skipdotfile = (*s == '*' || *s == '?');
	s = cnvregexp(s, len);
	re = regcmp(s, 0);
	free(s);
	return(re);
}

int regexp_exec(re, s, fname)
reg_t *re;
char *s;
int fname;
{
	if (!re || (fname && skipdotfile && *s == '.')) return(0);
	return(regex(re, s) ? 1 : 0);
}

VOID regexp_free(re)
reg_t *re;
{
	if (re) free(re);
}
#  else	/* !USEREGCMP */

reg_t *regexp_init(s, len)
char *s;
int len;
{
	reg_t *re;
	char *cp, *paren;
	int i, j, n, plen, size, meta, quote;

	skipdotfile = (*s == '*' || *s == '?');
	if (len < 0) len = strlen(s);
	paren = NULL;
	n = plen = size = 0;
	re = (reg_t *)malloc2(1 * sizeof(reg_t));
	re[0] = NULL;
	for (i = 0, quote = '\0'; i < len; i++) {
		cp = NULL;
		meta = 0;
		if (s[i] == quote) {
			quote = '\0';
			continue;
		}
		else if (quote == '\'');
		else if (isnmeta(s, i, quote, len)) {
			meta = 1;
			i++;
		}
		else if (quote);
		else if (s[i] == '\'' || s[i] == '"') {
			quote = s[i];
			continue;
		}

		if (paren) {
			paren = c_realloc(paren, plen + 1, size);

			if (quote || meta) {
#ifdef	BASHSTYLE
				paren[plen++] = PMETA;
				paren[plen++] = s[i];
#endif
			}
			else if (s[i] == ']') {
				if (!plen) {
					free(paren);
					regexp_free(re);
					return(NULL);
				}
				paren[plen] = '\0';
				cp = paren;
				paren = NULL;
			}
			else if (!plen && s[i] == '!') paren[plen++] = '\0';
#ifndef	BASHSTYLE
			else if (s[i] == '-') {
				if (!plen
				|| !s[i + 1] || s[i + 1] == ']') {
					free(paren);
					regexp_free(re);
					return(NULL);
				}
				paren[plen++] = s[i];
			}
#endif
			else {
#ifdef	CODEEUC
				if (isekana(s, i))
					paren[plen++] = s[i++];
				else
#endif
				if (iskanji1(s, i))
					paren[plen++] = s[i++];
				paren[plen++] = s[i];
			}
		}
		else if (!quote) switch(s[i]) {
			case '?':
				cp = wildsymbol1;
				break;
			case '*':
				cp = wildsymbol2;
				break;
			case '[':
				paren = c_malloc(size);
				plen = 0;
				break;
		}

		if (paren) continue;
		if (!cp) {
			cp = malloc2(2 + 1);
			j = 0;
#ifdef	CODEEUC
			if (isekana(s, i)) cp[j++] = s[i++];
			else
#endif
			if (iskanji1(s, i)) cp[j++] = s[i++];
			cp[j++] = s[i];
			cp[j] = '\0';
		}
		re = (reg_t *)realloc2(re, (n + 2) * sizeof(reg_t));
		re[n++] = cp;
		re[n] = NULL;
	}
	if (paren) {
		free(paren);
		regexp_free(re);
		return(NULL);
	}
	return(re);
}

static int _regexp_exec(re, s)
reg_t *re;
char *s;
{
	int i, n1, n2, c1, c2, beg, rev;

	for (n1 = n2 = 0; re[n1] && s[n2]; n1++, n2++) {
		c1 = (u_char)(s[i = n2]);
#if	MSDOS
		if (c1 >= 'a' && c1 <= 'z') c1 -= 'a' - 'A';
#endif
#ifdef	CODEEUC
		if (isekana(s, n2)) c1 = (c1 << 8) + (u_char)(s[++n2]);
		else
#endif
		if (iskanji1(s, n2)) c1 = (c1 << 8) + (u_char)(s[++n2]);
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
			if (re[n1][i] == PMETA && re[n1][i + 1]) i++;
			if (re[n1][i] == '-' && re[n1][i + 1] && c2)
#else
			if (re[n1][i] == '-')
#endif
			{
				beg = c2;
				i++;
			}
			c2 = (u_char)(re[n1][i]);
#if	MSDOS
			if (c2 >= 'a' && c2 <= 'z') c2 -= 'a' - 'A';
#endif
#ifdef	CODEEUC
			if (isekana(re[n1], i))
				c1 = (c1 << 8) + (u_char)(re[n1][++i]);
			else
#endif
			if (iskanji1(re[n1], i))
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
reg_t *re;
char *s;
int fname;
{
	if (!re || (fname && skipdotfile && *s == '.')) return(0);
	return(_regexp_exec(re, s));
}

VOID regexp_free(re)
reg_t *re;
{
	int i;

	if (re) {
		for (i = 0; re[i]; i++)
			if (re[i] != wildsymbol1 && re[i] != wildsymbol2)
				free(re[i]);
		free(re);
	}
}
#  endif	/* !USEREGCMP */
# endif		/* !USEREGCOMP */
#endif		/* !USERE_COMP */

static char *catpath(path, file, plenp, flen, overwrite)
char *path, *file;
int *plenp, flen, overwrite;
{
	char *new;
	int i;

	if (!*plenp) {
		new = malloc2(flen + 1);
		strncpy2(new, file, flen);
	}
	else {
#if	MSDOS
		if (_dospath(path)) i = 2;
		else
#endif
		i = 0;

		if (path[i] == _SC_) {
			for (i++; path[i] == _SC_; i++);
			if (!path[i]) *plenp = i - 1;
		}
		if (overwrite) new = realloc2(path, *plenp + 1 + flen + 1);
		else {
			new = malloc2(*plenp + 1 + flen + 1);
			strncpy(new, path, *plenp);
		}
		new[(*plenp)++] = _SC_;
		strncpy2(new + *plenp, file, flen);
	}
	*plenp += flen;
	return(new);
}

#if	MSDOS
static int _evalwild(argc, argvp, s, fixed, len)
int argc;
char ***argvp, *s, *fixed;
int len;
#else
static int _evalwild(argc, argvp, s, fixed, len, nino, ino)
int argc;
char ***argvp, *s, *fixed;
int len, nino;
devino_t *ino;
#endif
{
#if	!MSDOS
	devino_t *dupino;
#endif
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	reg_t *re;
	char *cp;
	int i, n, l;

	if (!*s) {
		if (len) {
			fixed = realloc2(fixed, len + 1 + 1);
			fixed[len++] = _SC_;
			fixed[len] = '\0';
			*argvp = (char **)realloc2(*argvp,
				(argc + 2) * sizeof(char *));
			(*argvp)[argc++] = fixed;
		}
#if	!MSDOS
		if (ino) free(ino);
#endif
		return(argc);
	}

#if	MSDOS || (defined(FD) && !defined(_NODOSDRIVE))
	if (!len && _dospath(s)) {
		fixed = malloc2(2 + 1);
		fixed[0] = *s;
		fixed[1] = ':';
		fixed[2] = '\0';
# if	MSDOS
		return(_evalwild(argc, argvp, s + 2, fixed, 2));
# else
		return(_evalwild(argc, argvp, s + 2, fixed, 2, nino, ino));
# endif
	}
#endif

	n = 0;
	for (i = 0; s[i]; i++) {
		if (s[i] == _SC_) break;
		if (s[i] == '*' || s[i] == '?'
		|| s[i] == '[' || s[i] == ']') n = 1;
#if	MSDOS
		if (iskanji1(s, i)) i++;
#endif
	}

	if (!i) {
		fixed = realloc2(fixed, len + 1 + 1);
		fixed[len++] = _SC_;
		fixed[len] = '\0';
#if	MSDOS
		return(_evalwild(argc, argvp, s + 1, fixed, len));
#else
		return(_evalwild(argc, argvp, s + 1, fixed, len, nino, ino));
#endif
	}

	if (!n) {
		fixed = catpath(fixed, s, &len, i, 1);
		if (Xstat(fixed, &st) < 0) free(fixed);
		else if (s[i]) {
			if ((st.st_mode & S_IFMT) != S_IFDIR) free(fixed);
			else {
#if	MSDOS
				return(_evalwild(argc, argvp,
					s + i + 1, fixed, len));
#else
				ino = (devino_t *)realloc2(ino,
					(nino + 1) * sizeof(devino_t));
				ino[nino].dev = st.st_dev;
				ino[nino++].ino = st.st_ino;
				return(_evalwild(argc, argvp,
					s + i + 1, fixed, len, nino, ino));
#endif
			}
		}
		else {
			*argvp = (char **)realloc2(*argvp,
				(argc + 2) * sizeof(char *));
			(*argvp)[argc++] = fixed;
		}
#if	!MSDOS
		if (ino) free(ino);
#endif
		return(argc);
	}

	if (i == 2 && s[i] && s[0] == '*' && s[1] == '*') {
#if	MSDOS
		argc = _evalwild(argc, argvp, s + 3, strdup2(fixed), len);
#else
		if (!ino) dupino = NULL;
		else {
			dupino = (devino_t *)malloc2(nino * sizeof(devino_t));
			for (n = 0; n < nino; n++) {
				dupino[n].dev = ino[n].dev;
				dupino[n].ino = ino[n].ino;
			}
		}
		argc = _evalwild(argc, argvp,
			s + 3, strdup2(fixed), len, nino, dupino);
#endif
		re = NULL;
	}
	else if (!(re = regexp_init(s, i))) {
		if (fixed) free(fixed);
#if	!MSDOS
		if (ino) free(ino);
#endif
		return(argc);
	}
	if (!(dirp = Xopendir((len) ? fixed : "."))) {
		regexp_free(re);
		if (fixed) free(fixed);
#if	!MSDOS
		if (ino) free(ino);
#endif
		return(argc);
	}

	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;

		l = len;
		cp = catpath(fixed, dp -> d_name, &l, strlen(dp -> d_name), 0);
		if (s[i]) {
			if (Xstat(cp, &st) < 0
			|| (st.st_mode & S_IFMT) != S_IFDIR) {
				free(cp);
				continue;
			}

#if	!MSDOS
			dupino = (devino_t *)malloc2((nino + 1)
				* sizeof(devino_t));
			for (n = 0; n < nino; n++) {
				dupino[n].dev = ino[n].dev;
				dupino[n].ino = ino[n].ino;
			}
			dupino[n].dev = st.st_dev;
			dupino[n].ino = st.st_ino;
#endif
			if (!re) {
#if	MSDOS
				argc = _evalwild(argc, argvp, s, cp, l);
#else
				for (n = 0; n < nino; n++)
					if (ino[n].dev == st.st_dev
					&& ino[n].ino == st.st_ino) break;
				if (n < nino) {
					free(cp);
					free(dupino);
				}
				else argc = _evalwild(argc, argvp,
						s, cp, l, nino + 1, dupino);
#endif
			}
			else if (!regexp_exec(re, dp -> d_name, 1)) {
				free(cp);
#if	!MSDOS
				free(dupino);
#endif
			}
#if	MSDOS
			else argc = _evalwild(argc, argvp, s + i + 1, cp, l);
#else
			else argc = _evalwild(argc, argvp,
					s + i + 1, cp, l, nino + 1, dupino);
#endif
		}
		else if (!re || !regexp_exec(re, dp -> d_name, 1)) free(cp);
		else {
			*argvp = (char **)realloc2(*argvp,
				(argc + 2) * sizeof(char *));
			(*argvp)[argc++] = cp;
		}
	}
	Xclosedir(dirp);
	regexp_free(re);
	if (fixed) free(fixed);
#if	!MSDOS
	if (ino) free(ino);
#endif
	return(argc);
}

int cmppath(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	return(strpathcmp(*((char **)vp1), *((char **)vp2)));
}

char **evalwild(s)
char *s;
{
	char **argv;
	int argc;

	argv = (char **)malloc2(1 * sizeof(char *));
#if	MSDOS
	argc = _evalwild(0, &argv, s, NULL, 0);
#else
	argc = _evalwild(0, &argv, s, NULL, 0, 0, NULL);
#endif
	if (!argc) {
		free(argv);
		return(NULL);
	}
	argv[argc] = NULL;
	if (argc > 1) qsort(argv, argc, sizeof(char *), cmppath);
	return(argv);
}

#ifndef	_NOUSEHASH
static int calchash(s)
char *s;
{
	u_int n;
	int i;

	for (i = n = 0; s[i]; i++) n += (u_char)(s[i]);
	return(n % MAXHASH);
}

static VOID inithash(VOID_A)
{
	int i;

	if (hashtable) return;
	hashtable = (hashlist **)malloc2(MAXHASH * sizeof(hashlist *));
	for (i = 0; i < MAXHASH; i++) hashtable[i] = NULL;
}

static hashlist *newhash(com, path, cost, next)
char *com, *path;
int cost;
hashlist *next;
{
	hashlist *new;

	new = (hashlist *)malloc2(sizeof(hashlist));
	new -> command = strdup2(com);
	new -> path = path;
	new -> hits = 0;
	new -> cost = cost;
	new -> next = next;
	return(new);
}

VOID freehash(hashlist **htable)
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
			free(hp -> command);
			free(hp -> path);
			free(hp);
		}
		htable[i] = NULL;
	}
	free(htable);
	if (htable == hashtable) hashtable = NULL;
}

hashlist **duplhash(htable)
hashlist **htable;
{
	hashlist *hp, **nextp, **new;
	int i;

	if (!htable) return(NULL);
	new = (hashlist **)malloc2(MAXHASH * sizeof(hashlist *));

	for (i = 0; i < MAXHASH; i++) {
		*(nextp = &(new[i])) = NULL;
		for (hp = htable[i]; hp; hp = hp -> next) {
			*nextp = newhash(hp -> command,
				strdup2(hp -> path), hp -> cost, NULL);
			(*nextp) -> hits = hp -> hits;
			nextp = &((*nextp) -> next);
		}
	}
	return(new);
}

static hashlist *findhash(com, n)
char *com;
int n;
{
	hashlist *hp;

	for (hp = hashtable[n]; hp; hp = hp -> next)
		if (!strpathcmp(com, hp -> command)) return(hp);
	return(NULL);
}

static VOID rmhash(com, n)
char *com;
int n;
{
	hashlist *hp, *prev;

	prev = NULL;
	for (hp = hashtable[n]; hp; hp = hp -> next) {
		if (!strpathcmp(com, hp -> command)) {
			if (!prev) hashtable[n] = hp -> next;
			else prev -> next = hp -> next;
			free(hp -> command);
			free(hp -> path);
			free(hp);
			return;
		}
		prev = hp;
	}
}
#endif	/* !_NOUSEHASH */

#if	MSDOS
static int extaccess(path, ext, len)
char *path, *ext;
int len;
{
	if (ext) {
		if (Xaccess(path, F_OK) >= 0) {
			if (!(strpathcmp(ext, "COM"))
			|| !(strpathcmp(ext, "EXE"))) return(0);
			else if (!(strpathcmp(ext, "BAT"))) return(CM_BATCH);
		}
	}
	else {
		path[len++] = '.';
		strcpy(path + len, "COM");
		if (Xaccess(path, F_OK) >= 0) return(0);
		strcpy(path + len, "EXE");
		if (Xaccess(path, F_OK) >= 0) return(0);
		strcpy(path + len, "BAT");
		if (Xaccess(path, F_OK) >= 0) return(CM_BATCH);
	}
	return(-1);
}
#endif

int searchhash(hpp, com)
hashlist **hpp;
char *com;
{
	char *cp, *next, *path;
	int l, len, cost, size, ret;
#if	MSDOS
	char *ext;
#endif
#ifndef	_NOUSEHASH
	int n;
#endif

#if	MSDOS
	if ((ext = strrchr(com, '.')) && strdelim(++ext, 0)) ext = NULL;
#endif
	if (strdelim(com, 1)) {
#if	MSDOS
		len = strlen(com);
		path = malloc2(len + EXTWIDTH + 1);
		strcpy(path, com);
		ret = extaccess(path, ext, len);
		free(path);
		if (ret >= 0) return(ret | CM_FULLPATH);
#else
		if (Xaccess(com, F_OK) >= 0) return(CM_FULLPATH);
#endif
		return(CM_NOTFOUND | CM_FULLPATH);
	}

#ifndef	_NOUSEHASH
	if (!hashtable) inithash();
	n = calchash(com);
	if ((*hpp = findhash(com, n))) {
		path = (*hpp) -> path;
		if (Xaccess(path, F_OK) >= 0) return((*hpp) -> type);
		rmhash(com, n);
	}
#endif

#if	MSDOS && !defined (DISMISS_CURPATH)
	len = strlen(com);
	path = malloc2(len + 2 + EXTWIDTH + 1);
	path[0] = '.';
	path[1] = _SC_;
	strcpy(path + 2, com);
	if ((ret = extaccess(path, ext, len)) < 0) free(path);
	else {
# ifdef	_NOUSEHASH
		*hpp = (hashlist *)path;
# else
		*hpp = newhash(com, path, 0, hashtable[n]);
		hashtable[n] = *hpp;
		(*hpp) -> type = (ret |= CM_HASH);
# endif
		return(ret);
	}
#endif
	if ((next = getvar("PATH", -1))) {
		len = strlen(com);
		size = ret = 0;
		path = NULL;
		cost = 1;
		for (cp = next; cp; cp = next) {
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
			if (_dospath(cp)) next = strchr(cp + 2, PATHDELIM);
			else
#endif
			next = strchr(cp, PATHDELIM);
			l = (next) ? (next++) - cp : strlen(cp);
			if (l + 1 + len + EXTWIDTH + 1 > size) {
				size = l + len + 1 + EXTWIDTH + 1;
				path = realloc2(path, size);
			}
			if (l) {
				strncpy2(path, cp, l);
				l = strcatdelim(path) - path;
			}
			l = strcpy2(path + l, com) - path;
#if	MSDOS
			if ((ret = extaccess(path, ext, l)) >= 0) break;
#else
			if (Xaccess(path, F_OK) >= 0) break;
#endif
			cost++;
		}
		if (cp) {
#ifdef	_NOUSEHASH
			*hpp = (hashlist *)path;
#else
			*hpp = newhash(com, path, cost, hashtable[n]);
			hashtable[n] = 	*hpp;
			(*hpp) -> type = (ret |= CM_HASH);
#endif
			return(ret);
		}
	}
	free(path);
	return(CM_NOTFOUND);
}

char *searchpath(path)
char *path;
{
	hashlist *hp;

	if (searchhash(&hp, path) & CM_NOTFOUND) return(NULL);
#ifdef	_NOUSEHASH
	return((char *)hp);
#else
	return(strdup2(hp -> path));
#endif
}

#if	!defined (FDSH) && !defined (_NOCOMPLETE)
char *finddupl(target, argc, argv)
char *target;
int argc;
char **argv;
{
	int i;

	for (i = 0; i < argc; i++)
		if (!strpathcmp(argv[i], target)) return(argv[i]);
	return(NULL);
}

#if	!MSDOS
static int completeuser(name, argc, argvp)
char *name;
int argc;
char ***argvp;
{
	struct passwd *pwd;
	char *new;
	int len, size;

	len = strlen(name);
	setpwent();
	while ((pwd =getpwent())) {
		if (strnpathcmp(name, pwd -> pw_name, len)) continue;
		size = strlen(pwd -> pw_name);
		new = malloc2(size + 2 + 1);
		new[0] = '~';
		strcatdelim2(new + 1, pwd -> pw_name, NULL);
		if (finddupl(new, argc, *argvp)) {
			free(new);
			continue;
		}

		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = new;
	}
	endpwent();
	return(argc);
}
#endif	/* !MSDOS */

static int completefile(file, dir, dlen, exe, argc, argvp)
char *file, *dir;
int dlen, exe, argc;
char ***argvp;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	char *cp, *new, path[MAXPATHLEN];
	int len, size;

	len = strlen(file);
	if (dlen >= MAXPATHLEN - 2) return(argc);
	strncpy2(path, dir, dlen);
	if (!(dirp = Xopendir(path))) return(argc);
	cp = strcatdelim(path);

	while ((dp = Xreaddir(dirp))) {
		if ((!len && isdotdir(dp -> d_name))
		|| strnpathcmp(file, dp -> d_name, len)) continue;
		size = strlen(dp -> d_name);
		if (size + (cp - path) >= MAXPATHLEN) continue;
		strncpy2(cp, dp -> d_name, size);

		if (Xstat(path, &st) < 0
		|| (exe && (st.st_mode & S_IFMT) != S_IFDIR
		&& Xaccess(path, X_OK) < 0)) continue;

		new = malloc2(size + 1 + 1);
		strncpy(new, dp -> d_name, size);
		if ((st.st_mode & S_IFMT) == S_IFDIR) new[size++] = _SC_;
		new[size] = '\0';
		if (finddupl(new, argc, *argvp)) {
			free(new);
			continue;
		}

		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = new;
	}
	Xclosedir(dirp);
	return(argc);
}

static int completeexe(file, argc, argvp)
char *file;
int argc;
char ***argvp;
{
	char *cp, *path;
	int len;

#if	MSDOS && !defined (DISMISS_CURPATH)
	argc = completefile(file, ".", 1, 1, argc, argvp);
#endif
	if (!(path = getvar("PATH", -1))) return(argc);
	do {
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
		if (_dospath(path)) cp = strchr(path + 2, PATHDELIM);
		else
#endif
		cp = strchr(path, PATHDELIM);
		len = (cp) ? cp++ - path : strlen(path);
		argc = completefile(file, path, len, 1, argc, argvp);
		path = cp;
	} while(path);
	return(argc);
}

int completepath(path, exe, argc, argvp)
char *path;
int exe, argc;
char ***argvp;
{
	char *file, *dir;
	int len;

	dir = path;
	len = 0;
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
	if (_dospath(path)) {
		dir += 2;
		len = 2;
	}
#endif

	if ((file = strrdelim(dir, 0))) {
		len += (file == dir) ? 1 : file - dir;
		return(completefile(file + 1, path, len, exe, argc, argvp));
	}
#if	!MSDOS
	else if (*path == '~') return(completeuser(path + 1, argc, argvp));
#endif
	else if (exe && dir == path) return(completeexe(path, argc, argvp));
	return(completefile(path, ".", 1, exe, argc, argvp));
}

char *findcommon(argc, argv)
int argc;
char **argv;
{
	char *common;
	int i, n;

	if (!argv || !argv[0]) return(NULL);
	common = strdup2(argv[0]);

	for (n = 1; n < argc; n++) {
		for (i = 0; common[i]; i++) if (common[i] != argv[n][i]) break;
		common[i] = '\0';
	}

	if (!common[0]) {
		free(common);
		return(NULL);
	}
	return(common);
}
#endif	/* !FDSH && !_NOCOMPLETE */

int addmeta(s1, s2, stripm, quoted)
char *s1, *s2, stripm, quoted;
{
	int i, j;

	if (!s2) return(0);
	for (i = j = 0; s2[i]; i++, j++) {
		if (s2[i] == '"') {
			if (s1) s1[j] = PMETA;
			j++;
		}
		else if (s2[i] == PMETA) {
			if (s1) s1[j] = PMETA;
			j++;
			if (stripm && s2[i + 1] == PMETA) i++;
		}
		else if (!quoted && s2[i] == '\'') {
			if (s1) s1[j] = PMETA;
			j++;
		}
#ifdef	CODEEUC
		else if (isekana(s2, i)) {
			if (s1) s1[j] = s2[i];
			j++;
			i++;
		}
#endif
		else if (iskanji1(s2, i)) {
			if (s1) s1[j] = s2[i];
			j++;
			i++;
		}
		if (s1) s1[j] = s2[i];
	}
	return(j);
}

static char *evalshellparam(c, quoted)
int c, quoted;
{
	char *cp, **arglist, tmp[MAXLONGWIDTH + 1];
	long pid;
	int i, j;

	cp = NULL;
	switch (c) {
		case '@':
			if (!getarglistfunc
			|| !(arglist = (*getarglistfunc)())) break;
			if (!quoted) {
				cp = catargs(&(arglist[1]), ' ');
				break;
			}
			for (i = j = 0; arglist[i + 1]; i++)
				j += addmeta(NULL,
					arglist[i + 1], 0, quoted);
			if (i > 0) {
				j += (i - 1) * 3;
				cp = malloc2(j + 1);
				j = addmeta(cp, arglist[1], 0, quoted);
				for (i = 2; arglist[i]; i++) {
					cp[j++] = quoted;
					cp[j++] = ' ';
					cp[j++] = quoted;
					j += addmeta(cp + j,
						arglist[i], 0, quoted);
				}
				cp[j] = '\0';
			}
			break;
		case '*':
			if (getarglistfunc && (arglist = (*getarglistfunc)()))
				cp = catargs(&(arglist[1]), ' ');
			break;
		case '#':
			if (getarglistfunc
			&& (arglist = (*getarglistfunc)())) {
				for (i = 0; arglist[i + 1]; i++);
				sprintf(tmp, "%d", i);
				cp = tmp;
			}
			break;
		case '?':
			i = (getretvalfunc) ? (*getretvalfunc)() : 0;
			sprintf(tmp, "%d", i);
			cp = tmp;
			break;
		case '$':
			sprintf(tmp, "%ld", (long)getpid());
			cp = tmp;
			break;
		case '!':
			if (getlastpidfunc
			&& (pid = (*getlastpidfunc)()) >= 0) {
				sprintf(tmp, "%ld", pid);
				cp = tmp;
			}
			break;
		case '-':
			if (getflagfunc) cp = (*getflagfunc)();
			break;
		default:
			break;
	}
	if (cp == tmp) cp = strdup2(tmp);
	return(cp);
}

static int replacevar(arg, cpp, top, s, len, vlen, mode)
char *arg, **cpp;
int top, s, len, vlen, mode;
{
	char *val;
	int i;

	if (!mode) return(0);
	if (mode == '+') {
		if (!*cpp) return(0);
	}
	else if (*cpp) return(0);
	else if (mode == '=' && arg[top] != '_' && !isalpha(arg[top]))
		return(-1);

	val = malloc2(vlen + 1);
	strncpy2(val, arg + s, vlen);
	if (!(*cpp = evalarg(val, (mode == '=' || mode == '?')))) {
		free(val);
		return(-1);
	}
	if (mode == '=') {
		if (setvar(arg + top, *cpp, len) < 0) {
			free(*cpp);
			return(-1);
		}
#ifdef	BASHSTYLE
		free(*cpp);
		val = malloc2(vlen + 1);
		strncpy2(val, arg + s, vlen);
		if (!(*cpp = evalarg(val, 0))) {
			free(val);
			return(-1);
		}
#endif
	}
	else if (mode == '?') {
		for (i = 0; i < len; i++) fputc(arg[top + i], stderr);
		fputs(": ", stderr);
		if (vlen <= 0)
			fputs("parameter null or not set", stderr);
		else for (i = 0; (*cpp)[i]; i++) fputc((*cpp)[i], stderr);
		free(*cpp);
		*cpp = NULL;
		fputc('\n', stderr);
		fflush(stderr);
		if (exitfunc) (*exitfunc)();
		return(-1);
	}
	return(mode);
}

static char *insertarg(arg, ptr, next, nlen)
char *arg;
int ptr, next, nlen;
{
	int i, olen, len;

	olen = next - ptr;
	if (nlen < olen) {
		for (i = 0; arg[next + i]; i++)
			arg[ptr + nlen + i] = arg[next + i];
		arg[ptr + nlen + i] = '\0';
	}
	else if (nlen > olen) {
		len = strlen(arg + next);
		arg = realloc2(arg, ptr + nlen + len + 1);
		for (i = len; i >= 0; i--) arg[ptr + nlen + i] = arg[next + i];
	}
	return(arg);
}

static int evalvar(argp, ptr, quoted)
char **argp;
int ptr, quoted;
{
	char *cp, *new, **arglist;
	int i, c, n, top, len, vlen, s, e, next, nul, mode, nest, quote;

	nul = 0;
	mode = '\0';
	new = NULL;
	top = ptr + 1;

	if ((*argp)[top] == '_' || isalpha((*argp)[top])) {
		for (i = top + 1; (*argp)[i]; i++)
			if ((*argp)[i] != '_' && !isalnum((*argp)[i])) break;
		len = i - top;
		s = e = next = i;
	}
	else if ((*argp)[top] != '{') {
		if ((*argp)[top] == quoted) return(top);
		len = 1;
		s = e = next = top + 1;
	}
	else {
		nest = 1;
		len = s = e = -1;
		for (i = ++top, quote = quoted; nest > 0; i++) {
			if ((*argp)[i] == quote) quote = '\0';
#ifdef	CODEEUC
			else if (isekana(*argp, i)) {
				if ((*argp)[i + 1]) i++;
			}
#endif
			else if (iskanji1(*argp, i)) {
				if ((*argp)[i + 1]) i++;
			}
			else if (quote == '\'');
			else if (ismeta(*argp, i, quote)) {
				if (len < 0) return(-1);
#ifndef	BASHSTYLE
				if (!quote) e = i;
#endif
			}
			else if (quote);
			else if ((*argp)[i] == '\'' || (*argp)[i] == '"')
				quote = (*argp)[i];
			else switch ((*argp)[i]) {
				case '\0':
				case '\n':
					return(-1);
/*NOTREACHED*/
					break;
				case '{':
					if (len < 0) return(-1);
					nest++;
					i++;
					break;
				case '}':
					nest--;
					break;
				case ':':
					if (len >= 0) break;
					len = i - top;
					s = i + 1;
					nul = 1;
					break;
				case '-':
				case '=':
				case '?':
				case '+':
					if (len < 0) len = i - top;
					else if (!nul || i - top > len + 1)
						break;
					s = i + 1;
					mode = (*argp)[i];
					break;
				default:
					if (len >= 0) {
						if (strchr(IFS_SET,
						(*argp)[i]))
							return(-1);
					}
					else if (i > top && (*argp)[i] != '_'
					&& !isalnum((*argp)[i]))
						return(-1);
					break;
			}
		}
		if (e < 0) e = i - 1;
		if (len < 0) len = e - top;
		if (s < 0) s = e;
		next = i;
	}

	vlen = e - s;
	if (len <= 0 || (!mode && vlen > 0)) return(-1);

	cp = NULL;
	c = '\0';
	if ((*argp)[top] == '_' || isalpha((*argp)[top]))
		cp = getvar(*argp + top, len);
	else if (isdigit((*argp)[top])) {
		if (len > 1) return(-1);
		if (getarglistfunc && (arglist = (*getarglistfunc)())) {
			i = (*argp)[top] - '0';
			for (n = 0; arglist[n]; n++);
			if (i < n) cp = arglist[i];
		}
	}
	else if (len == 1) {
		if ((cp = evalshellparam(c = (*argp)[top], quoted))) new = cp;
		else return(next);
	}
	else return(-1);

	if (!mode && checkundeffunc
	&& (*checkundeffunc)(cp, *argp + top, len) < 0)
		return(-1);

	if (cp && nul && !*cp) cp = NULL;

	if ((mode = replacevar(*argp, &cp, top, s, len, vlen, mode)) < 0) {
		if (new) free(new);
		return(-1);
	}

	if (!mode && (c != '@' || !quoted)) {
		vlen = addmeta(NULL, cp, 0, quoted);
		*argp = insertarg(*argp, ptr, next, vlen);
		addmeta(*argp + ptr, cp, 0, quoted);
	}
	else if (!cp) *argp = insertarg(*argp, ptr, next, 0);
	else {
		new = cp;
		vlen = strlen(cp);
		*argp = insertarg(*argp, ptr, next, vlen);
		strncpy(*argp + ptr, cp, vlen);
	}

	ptr += vlen;
	if (new) free(new);
	return(ptr);
}

static int evalhome(argp, ptr)
char **argp;
int ptr;
{
#if	!MSDOS
	struct passwd *pwd;
#endif
	char *cp;
	int len, top, next;

	top = ptr + 1;

	if (ptr && (*argp)[ptr - 1] != ':' && (*argp)[ptr - 1] != '=')
		return(ptr);
	len = ((cp = strdelim(*argp + top, 0)))
		? (cp - *argp) - top : strlen(*argp + top);
	next = top + len;
	if (!len) {
		cp = getvar("HOME", -1);
#if	!MSDOS
		if (!cp && (pwd = getpwuid(getuid()))) cp = pwd -> pw_dir;
#endif
	}
#ifdef	FD
	else if (!strpathcmp(*argp + top, "FD")) cp = progpath;
#endif
	else {
#if	!MSDOS
		cp = malloc2(len + 1);
		strncpy2(cp, *argp + top, len);
		pwd = getpwnam(cp);
		free(cp);
		if (pwd) cp = pwd -> pw_dir;
		else
#endif
		cp = NULL;
	}
	if (!cp) return(ptr);
	len = strlen(cp);
	*argp = insertarg(*argp, ptr, next, len);
	strncpy(*argp + ptr, cp, len);
	return(next - 1);
}

char *evalarg(arg, stripq)
char *arg;
int stripq;
{
	int i, j, n, quote;

	for (i = j = 0, quote = '\0'; arg[i]; i++) {
		if (arg[i] == quote) {
			quote = '\0';
			continue;
		}
#ifdef	CODEEUC
		else if (isekana(arg, i)) {
			if (stripq) arg[j++] = arg[i++];
		}
#endif
		else if (iskanji1(arg, i)) {
			if (stripq) arg[j++] = arg[i++];
		}
		else if (quote == '\'');
		else if (arg[i] == '$' && arg[i + 1]) {
#ifdef	FAKEMETA
			if (arg[i + 1] == '$' || arg[i + 1] == '~') {
				arg = insertarg(arg, j++, ++i, 0);
				continue;
			}
#endif
			if (stripq && i > j) {
				arg = insertarg(arg, j, i, 0);
				i = j;
			}
			if ((n = evalvar(&arg, i, quote)) < 0) return(NULL);
			i = j = n - 1;
		}
		else if (ismeta(arg, i, quote)) {
			i++;
			if (stripq && quote
			&& arg[i] != quote && arg[i] != PMETA)
				arg[j++] = PMETA;
		}
		else if (quote);
		else if (arg[i] == '\'' || arg[i] == '"') {
			quote = arg[i];
			continue;
		}
		else if (arg[i] == '~') {
			if (stripq && i > j) {
				arg = insertarg(arg, j, i, 0);
				i = j;
			}
			i = j = evalhome(&arg, i);
		}

		if (stripq) arg[j++] = arg[i];
	}
	if (stripq) arg[j] = '\0';
	return(arg);
}

int evalifs(argc, argvp)
int argc;
char ***argvp;
{
	char *cp, *ifs;
	int i, j, n, quote;

	if (!(ifs = getvar("IFS", -1))) ifs = IFS_SET;
	for (n = 0; n < argc; n++) {
		for (i = 0, quote = '\0'; (*argvp)[n][i]; i++) {
			if ((*argvp)[n][i] == quote) quote = '\0';
#ifdef	CODEEUC
			else if (isekana((*argvp)[n], i)) i++;
#endif
			else if (iskanji1((*argvp)[n], i)) i++;
			else if (quote == '\'');
			else if (ismeta((*argvp)[n], i, quote)) i++;
			else if (quote);
			else if ((*argvp)[n][i] == '\''
			|| (*argvp)[n][i] == '"')
				quote = (*argvp)[n][i];
			else if (strchr(ifs, (*argvp)[n][i])) {
				for (j = i + 1; (*argvp)[n][j]; j++)
					if (!strchr(ifs, (*argvp)[n][j]))							break;
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
				cp = strdup2(&((*argvp)[n][j]));
				*argvp = (char **)realloc2(*argvp,
					(argc + 2) * sizeof(char *));
				for (j = argc; j > n; j--)
					(*argvp)[j + 1] = (*argvp)[j];
				(*argvp)[n + 1] = cp;
				argc++;
				break;
			}
		}
		if (!i) {
			free((*argvp)[n]);
			for (i = n; i < argc; i++)
				(*argvp)[i] = (*argvp)[i + 1];
			argc--;
			n--;
		}
	}
	return(argc);
}

int evalglob(argc, argvp, iscom)
int argc;
char ***argvp;
int iscom;
{
	char *cp, **wild;
	int i, j, n, w, size, quote;

	for (n = 0; n < argc; n++) {
		cp = c_malloc(size);
		for (i = j = w = 0, quote = '\0'; (*argvp)[n][i]; i++) {
			cp = c_realloc(cp, j + 1, size);
			if ((*argvp)[n][i] == quote) {
				quote = '\0';
				continue;
			}
#ifdef	CODEEUC
			else if (isekana((*argvp)[n], i)) {
				cp[j++] = (*argvp)[n][i];
				i++;
			}
#endif
			else if (iskanji1((*argvp)[n], i)) {
				cp[j++] = (*argvp)[n][i];
				i++;
			}
			else if (quote == '\'');
			else if (ismeta((*argvp)[n], i, quote)) {
				i++;
				if (quote && (*argvp)[n][i] != quote
				&& (*argvp)[n][i] != PMETA)
					cp[j++] = PMETA;
			}
			else if (quote);
			else if ((*argvp)[n][i] == '\''
			|| (*argvp)[n][i] == '"') {
				quote = (*argvp)[n][i];
				continue;
			}
			else if (iscom && !n);
			else if (!strchr("?*[", (*argvp)[n][i]));
			else if (wild = evalwild((*argvp)[n])) {
				for (w = 0; wild[w]; w++);
				if (w > 1) {
					*argvp = (char **)realloc2(*argvp,
						(argc + w) * sizeof(char *));
					if (!*argvp) {
						free(cp);
						for (w = 0; wild[w]; w++)
							free(wild[w]);
						free(wild);
						return(argc);
					}
					for (i = argc; i > n; i--)
						(*argvp)[i + w - 1] =
							(*argvp)[i];
					argc += w - 1;
				}
				free((*argvp)[n]);
				free(cp);
				for (w = 0; wild[w]; w++)
					(*argvp)[n + w] = wild[w];
				free(wild);
				n += w;
				break;
			}

			cp[j++] = (*argvp)[n][i];
		}

		if (!w) {
			cp[j] = '\0';
			free((*argvp)[n]);
			(*argvp)[n] = cp;
		}
	}
	return(argc);
}

int stripquote(arg)
char *arg;
{
	int i, j, quote;

	for (i = j = 0, quote = '\0'; arg[i]; i++) {
		if (arg[i] == quote) {
			quote = '\0';
			continue;
		}
#ifdef	CODEEUC
		else if (isekana(arg, i)) arg[j++] = arg[i++];
#endif
		else if (iskanji1(arg, i)) arg[j++] = arg[i++];
		else if (quote == '\'');
		else if (ismeta(arg, i, quote)) {
			i++;
			if (quote && arg[i] != quote && arg[i] != PMETA)
				arg[j++] = PMETA;
		}
		else if (quote);
		else if (arg[i] == '\'' || arg[i] == '"') {
			quote = arg[i];
			continue;
		}

		arg[j++] = arg[i];
	}
	arg[j] = '\0';
	return(j);
}

#ifndef	FDSH
char *_evalpath(path, eol, uniqdelim, evalq)
char *path, *eol;
int uniqdelim, evalq;
{
	char *cp, *tmp, **argv;
	int i, j, c, size, quote;

	if (eol) i = eol - path;
	else i = strlen(path);
	cp = malloc2(i + 1);
	strncpy2(cp, path, i);

	if (!(tmp = evalarg(cp, 0))) {
		*cp = '\0';
		return(cp);
	}
	cp = tmp;

#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
	if (uniqdelim || !evalq) {
		char alias[MAXPATHLEN];
		int top;

		top = -1;
#else
	if (uniqdelim) {
#endif
		tmp = malloc2((size = strlen(cp) + 1));
		for (i = j = c = 0, quote = '\0'; cp[i]; i++) {
			if (cp[i] == quote) {
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
				if (!evalq && top >= 0 && ++top < j) {
					tmp[j] = '\0';
					if (shortname(&(tmp[top]), alias)
					&& strpathcmp(alias, &(tmp[top]))) {
						int len;

						len = strlen(alias);
						size += top + len - j;
						j = top + len;
						tmp = realloc2(tmp, size + 1);
						strncpy(tmp + top, alias, len);
					}
				}
				top = -1;
#endif
				quote = '\0';
			}
#ifdef	CODEEUC
			else if (isekana(cp, i)) tmp[j++] = cp[i++];
#endif
			else if (iskanji1(cp, i)) tmp[j++] = cp[i++];
			else if (quote == '\'');
			else if (ismeta(cp, i, quote)) tmp[j++] = cp[i++];
			else if (quote);
			else if (cp[i] == '\'') quote = cp[i];
			else if (cp[i] == '"') {
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
				top = j;
#endif
				quote = cp[i];
			}
			else {
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
				if (uniqdelim && cp[i] == _SC_ && c == _SC_)
					continue;
#else
				if (cp[i] == _SC_ && c == _SC_) continue;
#endif
			}
			c = tmp[j++] = cp[i];
		}
		tmp[j] = '\0';
		free(cp);
		cp = tmp;
		if (!j) return(cp);
	}

	if (evalq) {
		argv = (char **)malloc2(sizeof(char *) * 2);
		argv[0] = cp;
		argv[1] = NULL;
		i = evalifs(1, &argv);
		if (!i) {
			free(argv);
			return(strdup2(""));
		}
		for (j = 1; j < i; j++) free(argv[j]);
	
		argv[1] = NULL;
		i = evalglob(1, &argv, 0);
		for (j = 1; j < i; j++) free(argv[j]);
		cp = argv[0];
		free(argv);
	}

	return(cp);
}

char *evalpath(path, uniqdelim)
char *path;
int uniqdelim;
{
	char *cp;

	if (!path || !(*path)) return(path);
	for (cp = path; *cp == ' ' || *cp == '\t'; cp++);
	cp = _evalpath(cp, NULL, uniqdelim, 1);
	free(path);
	return(cp);
}
#endif	/* !FDSH */
