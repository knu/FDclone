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
#include "machine.h"
#include "printf.h"
#include "kctype.h"
#include "pathname.h"

# ifndef	NOUNISTDH
# include <unistd.h>
# endif

# ifndef	NOSTDLIBH
# include <stdlib.h>
# endif
#endif	/* !FD */

#if	MSDOS
#include <process.h>
#include "unixemu.h"
# ifndef	FD
# include <fcntl.h>
# include <dos.h>
# include <io.h>
# define	DS_IRDONLY	001
# define	DS_IHIDDEN	002
# define	DS_IFSYSTEM	004
# define	DS_IFLABEL	010
# define	DS_IFDIR	020
# define	DS_IARCHIVE	040
# define	SEARCHATTRS	(DS_IRDONLY | DS_IHIDDEN | DS_IFSYSTEM \
			| DS_IFDIR | DS_IARCHIVE)
# endif	/* !FD */
#define	EXTWIDTH	4
# if	defined (__TURBOC__) || (defined (DJGPP) && DJGPP < 2)
# include <dir.h>
# endif
# if	defined (DJGPP) && DJGPP < 2
# define	find_t	ffblk
# define	_dos_findfirst(p, a, f)	findfirst(p, f, a)
# define	_dos_findnext		findnext
# define	_ENOENT_		ENMFILE
# else
# define	ff_attrib		attrib
# define	ff_ftime		wr_time
# define	ff_fdate		wr_date
# define	ff_fsize		size
# define	ff_name			name
# define	_ENOENT_		ENOENT
# endif
#else	/* !MSDOS */
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/param.h>
#define	EXTWIDTH	0
# ifdef	USEDIRECT
# include <sys/dir.h>
# define	dirent	direct
# else
# include <dirent.h>
# endif
#endif	/* !MSDOS */

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
extern char *c_realloc __P_((char *, ALLOC_T, ALLOC_T *));
extern char *strdup2 __P_((char *));
extern char *strndup2 __P_((char *, int));
extern char *strchr2 __P_((char *, int));
extern char *strcpy2 __P_((char *, char *));
extern char *strncpy2 __P_((char *, char *, int));
# ifdef	_USEDOSPATH
extern int _dospath __P_((char *));
# endif
extern DIR *Xopendir __P_((char *));
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern char *Xgetwd __P_((char *));
extern int Xstat __P_((char *, struct stat *));
extern int Xaccess __P_((char *, int));
# if	MSDOS && !defined (_NOUSELFN)
extern char *shortname __P_((char *, char *));
# endif
extern VOID demacroarg __P_((char **));
extern char *progpath;
#else	/* !FD */
#define	getenv2		(char *)getenv
VOID error __P_((char *));
char *malloc2 __P_((ALLOC_T));
char *realloc2 __P_((VOID_P, ALLOC_T));
char *c_realloc __P_((char *, ALLOC_T, ALLOC_T *));
char *strdup2 __P_((char *));
char *strndup2 __P_((char *, int));
char *strchr2 __P_((char *, int));
char *strcpy2 __P_((char *, char *));
char *strncpy2 __P_((char *, char *, int));
# if	MSDOS
int _dospath __P_((char *));
DIR *Xopendir __P_((char *));
int Xclosedir __P_((DIR *));
struct dirent *Xreaddir __P_((DIR *));
static u_int NEAR getdosmode __P_((u_int));
static time_t NEAR getdostime __P_((u_int, u_int));
int Xstat __P_((char *, struct stat *));
# else	/* !MSDOS */
# define	Xopendir	opendir
# define	Xclosedir	closedir
# define	Xreaddir	readdir
# define	Xstat		stat
# endif	/* !MSDOS */
# ifdef	DJGPP
char *Xgetwd __P_((char *));
# else	/* !DJGPP */
#  ifdef	USEGETWD
#  define	Xgetwd		(char *)getwd
#  else
#  define	Xgetwd(p)	(char *)getcwd(p, MAXPATHLEN)
#  endif
# endif	/* !DJGPP */
#define	Xaccess		access
#endif	/* !FD */

static char *NEAR getenvvar __P_((char *, int));
static int NEAR setvar __P_((char *, char *, int));
static int NEAR ismeta __P_((char *s, int, int, int));
#ifdef	_NOORIGGLOB
static char *NEAR cnvregexp __P_((char *, int));
#else
static int NEAR _regexp_exec __P_((char **, char *));
#endif
static void NEAR addstrbuf __P_((strbuf_t *, char *, int));
static void NEAR duplwild(wild_t *, wild_t *);
static void NEAR freewild(wild_t *);
static int NEAR _evalwild __P_((int, char ***, wild_t *));
#ifndef	_NOUSEHASH
static int NEAR calchash __P_((char *));
static VOID NEAR inithash __P_((VOID_A));
static hashlist *NEAR newhash __P_((char *, char *, int, hashlist *));
static VOID NEAR freehash __P_((hashlist **));
static hashlist *NEAR findhash __P_((char *, int));
static VOID NEAR rmhash __P_((char *, int));
#endif
static int NEAR isexecute __P_((char *, int, int));
#if	MSDOS
static int NEAR extaccess __P_((char *, char *, int, int));
#endif
#if	!defined (FDSH) && !defined (_NOCOMPLETE)
# ifndef	NOUID
static int NEAR completeuser __P_((char *, int, int, char ***));
# endif
static int NEAR completefile __P_((char *, int, int, char ***,
	char *, int, int));
static int NEAR completeexe __P_((char *, int, int, char ***));
#endif	/* !FDSH && !_NOCOMPLETE */
static int NEAR addmeta __P_((char *, char *, int, int));
static int NEAR skipvar __P_((char **, int *, int *, int));
#ifdef	MINIMUMSHELL
static int NEAR skipvarvalue __P_((char *, int *, char *, int, int));
static char *NEAR evalshellparam __P_((int, int));
#else	/* !MINIMUMSHELL */
static int NEAR skipvarvalue __P_((char *, int *, char *, int, int, int));
static char *NEAR removeword __P_((char *, char *, int, int));
static char **NEAR removevar __P_((char **, char *, int, int));
static char *NEAR evalshellparam __P_((int, int, char *, int, int *));
#endif	/* !MINIMUMSHELL */
static int NEAR replacevar __P_((char *, char **, int, int, int, int, int));
static char *NEAR insertarg __P_((char *, int, char *, int, int));
static int NEAR evalvar __P_((char **, int, char **, int));
static char *NEAR replacebackquote __P_((char *, int *, char *, int));

#define	BUFUNIT		32
#define	b_size(n, type)	((((n) / BUFUNIT) + 1) * BUFUNIT * sizeof(type))
#define	b_realloc(ptr, n, type) \
			(((n) % BUFUNIT) ? ((type *)(ptr)) \
			: (type *)realloc2(ptr, b_size(n, type)))
#define	getconstvar(s)	(getenvvar(s, sizeof(s) - 1))

char **argvar = NULL;
#ifndef	_NOUSEHASH
hashlist **hashtable = NULL;
#endif
char *(*getvarfunc)__P_((char *, int)) = NULL;
int (*putvarfunc)__P_((char *, int)) = NULL;
int (*getretvalfunc)__P_((VOID_A)) = NULL;
p_id_t (*getpidfunc)__P_((VOID_A)) = NULL;
p_id_t (*getlastpidfunc)__P_((VOID_A)) = NULL;
char *(*getflagfunc)__P_((VOID_A)) = NULL;
int (*checkundeffunc)__P_((char *, char *, int)) = NULL;
VOID (*exitfunc)__P_((VOID_A)) = NULL;
char *(*backquotefunc)__P_((char *)) = NULL;
#ifndef	MINIMUMSHELL
char *(*posixsubstfunc)__P_((char *, int *)) = NULL;
#endif
#ifndef	PATHNOCASE
int pathignorecase = 0;
#endif

static int skipdotfile = 0;
#if	!defined (USEREGCMP) && !defined (USEREGCOMP) && !defined (USERE_COMP)
static char *wildsymbol1 = "?";
static char *wildsymbol2 = "*";
#endif
#if	defined (FD) && !defined (NOUID)
static uidtable *uidlist = NULL;
static int maxuid = 0;
static gidtable *gidlist = NULL;
static int maxgid = 0;
#endif	/* FD && !NOUID */


#ifndef	FD
VOID error(s)
char *s;
{
	fputs(s, stderr);
	fputs(": memory allocation error", stderr);
	fputnl(stderr);
	exit(2);
}

char *malloc2(size)
ALLOC_T size;
{
	char *tmp;

	if (!size || !(tmp = (char *)malloc(size))) {
		error("malloc()");
#ifdef	FAKEUNINIT
		tmp = NULL;	/* fake for -Wuninitialized */
#endif
	}
	return(tmp);
}

char *realloc2(ptr, size)
VOID_P ptr;
ALLOC_T size;
{
	char *tmp;

	if (!size
	|| !(tmp = (ptr) ? (char *)realloc(ptr, size) : (char *)malloc(size)))
	{
		error("realloc()");
#ifdef	FAKEUNINIT
		tmp = NULL;	/* fake for -Wuninitialized */
#endif
	}
	return(tmp);
}

char *c_realloc(ptr, n, sizep)
char *ptr;
ALLOC_T n, *sizep;
{
	if (!ptr) {
		*sizep = BUFUNIT;
		return(malloc2(*sizep));
	}
	while (n + 1 >= *sizep) *sizep *= 2;
	return(realloc2(ptr, *sizep));
}

char *strdup2(s)
char *s;
{
	char *tmp;
	int n;

	if (!s) return(NULL);
	n = strlen(s);
	if (!(tmp = (char *)malloc((ALLOC_T)n + 1))) error("malloc()");
	memcpy(tmp, s, n + 1);
	return(tmp);
}

char *strndup2(s, n)
char *s;
int n;
{
	char *tmp;
	int i;

	if (!s) return(NULL);
	for (i = 0; i < n; i++) if (!s[i]) break;
	if (!(tmp = (char *)malloc((ALLOC_T)i + 1))) error("malloc()");
	memcpy(tmp, s, i);
	tmp[i] = '\0';
	return(tmp);
}

char *strchr2(s, c)
char *s;
int c;
{
	int i;

	for (i = 0; s[i]; i++) {
		if (s[i] == c) return(&(s[i]));
		if (iskanji1(s, i)) i++;
#ifdef	CODEEUC
		else if (isekana(s, i)) i++;
#endif
	}
	return(NULL);
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
	return(&(s1[i]));
}

# if	MSDOS
int _dospath(path)
char *path;
{
	return((isalpha2(*path) && path[1] == ':') ? *path : 0);
}

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

static u_int NEAR getdosmode(attr)
u_int attr;
{
	u_int mode;

	mode = 0;
	if (attr & DS_IARCHIVE) mode |= S_ISVTX;
	if (!(attr & DS_IHIDDEN)) mode |= S_IREAD;
	if (!(attr & DS_IRDONLY)) mode |= S_IWRITE;
	if (attr & DS_IFDIR) mode |= (S_IFDIR | S_IEXEC);
	else if (attr & DS_IFLABEL) mode |= S_IFIFO;
	else if (attr & DS_IFSYSTEM) mode |= S_IFSOCK;
	else mode |= S_IFREG;

	return(mode);
}

static time_t NEAR getdostime(d, t)
u_int d, t;
{
	struct tm tm;

	tm.tm_year = 1980 + ((d >> 9) & 0x7f);
	tm.tm_year -= 1900;
	tm.tm_mon = ((d >> 5) & 0x0f) - 1;
	tm.tm_mday = (d & 0x1f);
	tm.tm_hour = ((t >> 11) & 0x1f);
	tm.tm_min = ((t >> 5) & 0x3f);
	tm.tm_sec = ((t << 1) & 0x3e);
	tm.tm_isdst = -1;

	return(mktime(&tm));
}

int Xstat(path, stp)
char *path;
struct stat *stp;
{
	struct find_t find;

	if (_dos_findfirst(path, SEARCHATTRS, &find) != 0) return(-1);
	stp -> st_mode = getdosmode(find.ff_attrib);
	stp -> st_mtime = getdostime(find.ff_fdate, find.ff_ftime);
	stp -> st_size = find.ff_fsize;
	stp -> st_ctime = stp -> st_atime = stp -> st_mtime;
	stp -> st_dev = stp -> st_ino = 0;
	stp -> st_nlink = 1;
	stp -> st_uid = stp -> st_gid = -1;
	return(0);
}
# endif	/* MSDOS */

# ifdef	DJGPP
char *Xgetwd(path)
char *path;
{
	char *cp;
	int i;

	if (!(cp = (char *)getcwd(path, MAXPATHLEN))) return(NULL);
	for (i = 0; cp[i]; i++) if (cp[i] == '/') cp[i] = _SC_;
	return(cp);
}
# endif	/* !DJGPP */
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
# ifdef	BSPATHDELIM
		if (iskanji1(s, i)) i++;
# endif
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
# ifdef	BSPATHDELIM
		if (iskanji1(s, i)) i++;
# endif
	}
	return(cp);
}
#endif	/* MSDOS || (FD && !_NODOSDRIVE) */

char *strrdelim2(s, eol)
char *s, *eol;
{
#ifdef	BSPATHDELIM
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
#ifdef	BSPATHDELIM
	int i;
#endif

	if (ptr < 0 || s[ptr] != _SC_) return(0);
#ifdef	BSPATHDELIM
	if (--ptr < 0) return(1);
	if (!ptr) return(!iskanji1(s, 0));

	for (i = 0; s[i] && i < ptr; i++) if (iskanji1(s, i)) i++;
	if (!s[i] || i > ptr) return(1);
	return(!iskanji1(s, i));
#else
	return(1);
#endif
}

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
	if (!s[i]) return(&(s[i]));
	if (s[i] == _SC_ && !s[i + 1]) return(&(s[i + 1]));

	cp = NULL;
	for (; s[i]; i++) {
		if (s[i] == _SC_) {
			if (!cp) cp = &(s[i]);
			continue;
		}
		cp = NULL;
#ifdef	BSPATHDELIM
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
	if (!s1[i]) cp = &(buf[i]);
	else if (s1[i] == _SC_ && !s1[i + 1]) {
		cp = &(buf[i]);
		*(cp++) = _SC_;
	}
	else {
		cp = NULL;
		for (; s1[i]; i++) {
			buf[i] = s1[i];
			if (s1[i] == _SC_) {
				if (!cp) cp = &(buf[i]) + 1;
				continue;
			}
			cp = NULL;
#ifdef	BSPATHDELIM
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
			*(cp++) = _SC_;
		}
	}
	if (s2) {
		len = MAXPATHLEN - 1 - (cp - buf);
		for (i = 0; s2[i] && i < len; i++) *(cp++) = s2[i];
	}
	*cp = '\0';
	return(cp);
}

int strcasecmp2(s1, s2)
char *s1, *s2;
{
	for (;;) {
		if (toupper2(*s1) != toupper2(*s2)) return(*s1 - *s2);
#ifndef	CODEEUC
		if (issjis1(*s1)) {
			s1++;
			s2++;
			if (*s1 != *s2) return(*s1 - *s2);
		}
#endif
		if (!*s1) break;
		s1++;
		s2++;
	}
	return(0);
}

int strncasecmp2(s1, s2, n)
char *s1, *s2;
int n;
{
	while (n-- > 0) {
		if (toupper2(*s1) != toupper2(*s2)) return(*s1 - *s2);
#ifndef	CODEEUC
		if (issjis1(*s1)) {
			if (n-- <= 0) break;
			s1++;
			s2++;
			if (*s1 != *s2) return(*s1 - *s2);
		}
#endif
		if (!*s1) break;
		s1++;
		s2++;
	}
	return(0);
}

#ifndef	PATHNOCASE
int strpathcmp2(s1, s2)
char *s1, *s2;
{
	if (pathignorecase) return(strcasecmp2(s1, s2));
	else return(strcmp(s1, s2));
}

int strnpathcmp2(s1, s2, n)
char *s1, *s2;
int n;
{
	if (pathignorecase) return(strncasecmp2(s1, s2, n));
	else return(strncmp(s1, s2, n));
}
#endif	/* !PATHNOCASE */

char *underpath(path, dir, len)
char *path, *dir;
int len;
{
	char *cp;

	if (len < 0) len = strlen(dir);
	if ((cp = strrdelim2(dir, &(dir[len]))) && !*(cp + 1)) len = cp - dir;
	if (len <= 0 || strnpathcmp(path, dir, len)) return(NULL);
	if (path[len] && path[len] != _SC_) return(NULL);

	return(&(path[len]));
}

static char *NEAR getenvvar(ident, len)
char *ident;
int len;
{
#if	(!defined (FD) && !defined (FDSH)) || defined (_NOORIGSHELL)
	char *cp, *env;
#endif

	if (getvarfunc) return((*getvarfunc)(ident, len));
#if	(defined (FD) || defined (FDSH)) && !defined (_NOORIGSHELL)
	return(NULL);
#else
	if (len < 0) len = strlen(ident);
	cp = strndup2(ident, len);
	env = getenv2(cp);
	free(cp);
	return(env);
#endif
}

static int NEAR setvar(ident, value, len)
char *ident, *value;
int len;
{
	char *cp;
	int ret, vlen;

	vlen = strlen(value);
	cp = malloc2(len + 1 + vlen + 1);
	strncpy(cp, ident, len);
	cp[len] = '=';
	strncpy2(&(cp[len + 1]), value, vlen);

#if	defined (FD) || defined (FDSH)
	ret = (putvarfunc) ? (*putvarfunc)(cp, len) : -1;
#else
	ret = (putvarfunc) ? (*putvarfunc)(cp, len) : putenv(cp);
#endif
	if (ret < 0) free(cp);
	return(ret);
}

int isidentchar(c)
int c;
{
	return(c == '_' || isalpha2(c));
}

int isidentchar2(c)
int c;
{
	return(c == '_' || isalnum2(c));
}

int isdotdir(s)
char *s;
{
	if (s[0] == '.' && (!s[1] || (s[1] == '.' && !s[2]))) return(1);
	return(0);
}

char *isrootdir(s)
char *s;
{
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
	if (_dospath(s)) s += 2;
#endif
	if (*s != _SC_) return(NULL);
	return(s);
}

char *getbasename(s)
char *s;
{
	char *cp;

	if ((cp = strrdelim(s, 1))) return(cp + 1);
	return(s);
}

char *getshellname(s, loginp, restrictedp)
char *s;
int *loginp, *restrictedp;
{
	if (loginp) *loginp = 0;
	if (restrictedp) *restrictedp = 0;
	if (*s == '-') {
		s++;
		if (loginp) *loginp = 1;
	}
#ifdef	PATHNOCASE
	if (tolower2(*s) == 'r')
#else
	if (*s == 'r')
#endif
	{
		s++;
		if (restrictedp) *restrictedp = 1;
	}

	return(s);
}

static int NEAR ismeta(s, ptr, quote, len)
char *s;
int ptr, quote, len;
{
#ifdef	FAKEMETA
	return(0);
#else	/* !FAKEMETA */
	if (s[ptr] != PMETA || quote == '\'') return(0);

	if (len >= 0) {
		if (ptr + 1 >= len) return(0);
# ifndef	BASHSTYLE
	/* bash does not treat "\" as \ */
		if (quote == '"' && s[ptr + 1] == quote && ptr + 2 >= len)
			return(0);
# endif
	}
	else {
		if (!s[ptr + 1]) return(0);
# ifndef	BASHSTYLE
	/* bash does not treat "\" as \ */
		if (quote == '"' && s[ptr + 1] == quote && !s[ptr + 2])
			return(0);
# endif
	}

# ifdef	BSPATHDELIM
	if (quote == '"') {
		if (!strchr(DQ_METACHAR, s[ptr + 1])) return(0);
	}
	else {	/* if (!quote || quote == '`') */
		if (!strchr(METACHAR, s[ptr + 1])) return(0);
	}
# endif

	return(1);
#endif	/* !FAKEMETA */
}

#ifdef	_NOORIGGLOB
static char *NEAR cnvregexp(s, len)
char *s;
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
		else if (ismeta(s, i, '\0', len)) {
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

reg_t *regexp_init(s, len)
char *s;
int len;
{
# ifdef	PATHNOCASE
	re_ignore_case = 1;
# endif
	skipdotfile = (*s == '*' || *s == '?' || *s == '[');
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

	skipdotfile = (*s == '*' || *s == '?' || *s == '[');
	s = cnvregexp(s, len);
	re = (reg_t *)malloc2(sizeof(reg_t));
# ifdef	PATHNOCASE
	if (regcomp(re, s, REG_EXTENDED | REG_ICASE)) {
# else
	if (regcomp(re, s, REG_EXTENDED | (pathignorecase ? REG_ICASE : 0))) {
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
	if (re) {
		regfree(re);
		free(re);
	}
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

	skipdotfile = (*s == '*' || *s == '?' || *s == '[');
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
	ALLOC_T size;
	int i, j, n, pc, plen, metachar, quote;

	skipdotfile = 0;
	if (len < 0) len = strlen(s);
	re = NULL;
	paren = NULL;
	n = plen = size = 0;
	quote = '\0';
	re = b_realloc(re, n, reg_t);
	re[n] = NULL;
	for (i = 0; i < len; i++) {
		cp = NULL;
		metachar = 0;
		pc = parsechar(&(s[i]), len - i, '\0', 0, &quote, NULL);
		if (pc == PC_OPQUOTE || pc == PC_CLQUOTE) continue;
		else if (pc == PC_META) {
			metachar = 1;
			i++;
		}

		if (paren) {
			paren = c_realloc(paren, plen + 1, &size);

			if (quote || metachar) {
#ifdef	BASHSTYLE
	/* bash treats a character quoted by \ in "[]" as a character itself */
				paren[plen++] = PMETA;
# ifndef	PATHNOCASE
				if (!pathignorecase) paren[plen++] = s[i];
				else
# endif
				paren[plen++] = toupper2(s[i]);
#endif
			}
			else if (s[i] == ']') {
				if (!plen) {
					free(paren);
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
				cp = realloc2(paren, plen + 1);
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
				else j = toupper2(s[i + 1]);

#ifdef	BASHSTYLE
	/* bash does not allow "[b-a]" */
				if (plen && j && j != ']'
				&& j < paren[plen - 1]) {
					free(paren);
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
					free(paren);
					regexp_free(re);
					return(NULL);
				}
#endif
				else paren[plen++] = s[i];
			}
			else if (iskanji1(s, i)) {
				paren[plen++] = s[i++];
				paren[plen++] = s[i];
			}
#ifdef	CODEEUC
			else if (isekana(s, i)) {
				paren[plen++] = s[i++];
				paren[plen++] = s[i];
			}
#endif
#ifndef	PATHNOCASE
			else if (!pathignorecase) paren[plen++] = s[i];
#endif
			else paren[plen++] = toupper2(s[i]);
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
			cp = malloc2(2 + 1);
			j = 0;
			if (iskanji1(s, i)) {
				cp[j++] = s[i++];
				cp[j++] = s[i];
			}
#ifdef	CODEEUC
			else if (isekana(s, i)) {
				cp[j++] = s[i++];
				cp[j++] = s[i];
			}
#endif
#ifndef	PATHNOCASE
			else if (!pathignorecase) cp[j++] = s[i];
#endif
			else cp[j++] = toupper2(s[i]);
			cp[j] = '\0';
		}
		re[n++] = cp;
		re = b_realloc(re, n, reg_t);
		re[n] = NULL;
	}
	if (paren) {
		free(paren);
		if (plen) {
			regexp_free(re);
			return(NULL);
		}
		cp = strdup2("[");
		re[n++] = cp;
		re = b_realloc(re, n, reg_t);
		re[n] = NULL;
	}
	return((reg_t *)realloc2(re, (n + 1) * sizeof(reg_t)));
}

static int NEAR _regexp_exec(re, s)
reg_t *re;
char *s;
{
	int i, n1, n2, c1, c2, beg, rev;

	for (n1 = n2 = 0; re[n1] && s[n2]; n1++, n2++) {
		c1 = (u_char)(s[i = n2]);
		if (iskanji1(s, n2)) c1 = (c1 << 8) + (u_char)(s[++n2]);
#ifdef	CODEEUC
		else if (isekana(s, n2)) c1 = (c1 << 8) + (u_char)(s[++n2]);
#endif
#ifndef	PATHNOCASE
		else if (!pathignorecase) /*EMPTY*/;
#endif
		else c1 = toupper2(c1);

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
			if (re[n1][i] == PMETA && re[n1][i + 1]) i++;
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
			if (iskanji1(re[n1], i))
				c2 = (c2 << 8) + (u_char)(re[n1][++i]);
#ifdef	CODEEUC
			else if (isekana(re[n1], i))
				c2 = (c2 << 8) + (u_char)(re[n1][++i]);
#endif
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

static void NEAR addstrbuf(sp, s, len)
strbuf_t *sp;
char *s;
int len;
{
	sp -> s = c_realloc(sp -> s, sp -> len + len, &(sp -> size));
	memcpy(&(sp -> s[sp -> len]), s, len);
	sp -> len += len;
	sp -> s[sp -> len] = '\0';
}

static void NEAR duplwild(dst, src)
wild_t *dst, *src;
{
	memcpy(dst, src, sizeof(wild_t));
	dst -> fixed.s = (char *)malloc2(src -> fixed.size);
	memcpy(dst -> fixed.s, src -> fixed.s, src -> fixed.len + 1);
	dst -> path.s = (char *)malloc2(src -> path.size);
	memcpy(dst -> path.s, src -> path.s, src -> path.len + 1);

#ifndef	NODIRLOOP
	if (src -> ino) {
		dst -> ino = (devino_t *)malloc2(src -> nino
			* sizeof(devino_t));
		memcpy(dst -> ino, src -> ino, src -> nino * sizeof(devino_t));
	}
#endif	/* !NODIRLOOP */
}

static void NEAR freewild(wp)
wild_t *wp;
{
	if (wp -> fixed.s) free(wp -> fixed.s);
	if (wp -> path.s) free(wp -> path.s);
#ifndef	NODIRLOOP
	if (wp -> ino) free(wp -> ino);
#endif
}

static int NEAR _evalwild(argc, argvp, wp)
int argc;
char ***argvp;
wild_t *wp;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	reg_t *re;
	wild_t dupl;
	ALLOC_T flen, plen;
	char *cp;
	int i, n, w, pc, quote, isdir;

	if (!*(wp -> s)) {
		if (!(wp -> fixed.len)) return(argc);
		*argvp = (char **)realloc2(*argvp,
			(argc + 2) * sizeof(char *));
		(*argvp)[argc++] = wp -> fixed.s;
		wp -> fixed.s = NULL;
		return(argc);
	}

	flen = wp -> fixed.len;
	plen = wp -> path.len;
	quote = wp -> quote;

	if (wp -> fixed.len) addstrbuf(&(wp -> path), _SS_, 1);

	for (i = w = 0; wp -> s[i] && wp -> s[i] != _SC_; i++) {
		pc = parsechar(&(wp -> s[i]), -1,
			'\0', 0, &(wp -> quote), NULL);
		if (pc == PC_OPQUOTE || pc == PC_CLQUOTE) {
			if (!(wp -> flags & EA_STRIPQ))
				addstrbuf(&(wp -> fixed), &(wp -> s[i]), 1);
			continue;
		}
		else if (pc == PC_WORD) {
			addstrbuf(&(wp -> fixed), &(wp -> s[i]), 1);
			addstrbuf(&(wp -> path), &(wp -> s[i]), 1);
			i++;
		}
		else if (pc == PC_META) {
			if (wp -> flags & EA_KEEPMETA)
				addstrbuf(&(wp -> fixed), &(wp -> s[i]), 1);
			if (wp -> s[i + 1] == _SC_) continue;

			if (wp -> quote == '\''
			|| (wp -> quote == '"'
			&& !strchr(DQ_METACHAR, wp -> s[i + 1]))) {
				if (!(wp -> flags & EA_KEEPMETA))
					addstrbuf(&(wp -> fixed),
						&(wp -> s[i]), 1);
				addstrbuf(&(wp -> path), &(wp -> s[i]), 1);
			}
			i++;
		}
		else if (pc == PC_NORMAL && strchr("?*[", wp -> s[i])) {
			if (w >= 0 && wp -> s[i] == '*') w++;
			else w = -1;
		}

		addstrbuf(&(wp -> fixed), &(wp -> s[i]), 1);
		addstrbuf(&(wp -> path), &(wp -> s[i]), 1);
	}

	if (!(wp -> s[i])) isdir = 0;
	else {
		isdir = 1;
		addstrbuf(&(wp -> fixed), _SS_, 1);
	}

	if (!w) {
		if (wp -> path.len <= plen) w++;
		else if (Xstat(wp -> path.s, &st) < 0) return(argc);

		wp -> s += i;
		if (isdir) {
			if (!w && (st.st_mode & S_IFMT) != S_IFDIR)
				return(argc);
			(wp -> s)++;
		}

#ifndef	NODIRLOOP
		if (!w) {
			wp -> ino = (devino_t *)realloc2(wp -> ino,
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
		cp = malloc2(i + 2);
		n = 0;
		if (quote) cp[n++] = quote;
		memcpy(&(cp[n]), wp -> s, i);
		n += i;
		if (wp -> quote) cp[n++] = wp -> quote;
		re = regexp_init(cp, n);
		free(cp);
		if (!re) return(argc);
		wp -> s += i + 1;
	}

	if (wp -> path.len) cp = wp -> path.s;
	else if (wp -> fixed.len) cp = _SS_;
	else cp = ".";

	if (!(dirp = Xopendir(cp))) {
		regexp_free(re);
		return(argc);
	}
	if (wp -> path.len || wp -> fixed.len)
		addstrbuf(&(wp -> path), _SS_, 1);

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

			dupl.ino = (devino_t *)realloc2(dupl.ino,
				(dupl.nino + 1) * sizeof(devino_t));
			dupl.ino[dupl.nino].dev = st.st_dev;
			dupl.ino[(dupl.nino)++].ino = st.st_ino;
#endif

			addstrbuf(&(dupl.fixed), _SS_, 1);
			argc = _evalwild(argc, argvp, &dupl);
		}
		else if (regexp_exec(re, dp -> d_name, 1)) {
			*argvp = (char **)realloc2(*argvp,
				(argc + 2) * sizeof(char *));
			(*argvp)[argc++] = dupl.fixed.s;
			dupl.fixed.s = NULL;
		}

		freewild(&dupl);
	}
	Xclosedir(dirp);
	regexp_free(re);

	return(argc);
}

int cmppath(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	return(strpathcmp2(*((char **)vp1), *((char **)vp2)));
}

char **evalwild(s, flags)
char *s;
int flags;
{
	wild_t w;
	char **argv;
	int argc;

	argv = (char **)malloc2(1 * sizeof(char *));
	w.s = s;
	w.fixed.s = c_realloc(NULL, 0, &(w.fixed.size));
	w.path.s = c_realloc(NULL, 0, &(w.path.size));
	w.fixed.len = w.path.len = (ALLOC_T)0;
	w.quote = '\0';
#ifndef	NODIRLOOP
	w.nino = 0;
	w.ino = NULL;
#endif
	w.flags = flags;

	argc = _evalwild(0, &argv, &w);
	freewild(&w);

	if (!argc) {
		free(argv);
		return(NULL);
	}

	argv[argc] = NULL;
	if (argc > 1) qsort(argv, argc, sizeof(char *), cmppath);
	return(argv);
}

#ifndef	_NOUSEHASH
static int NEAR calchash(s)
char *s;
{
	u_int n;
	int i;

	for (i = n = 0; s[i]; i++) n += (u_char)(s[i]);
	return(n % MAXHASH);
}

static VOID NEAR inithash(VOID_A)
{
	int i;

	if (hashtable) return;
	hashtable = (hashlist **)malloc2(MAXHASH * sizeof(hashlist *));
	for (i = 0; i < MAXHASH; i++) hashtable[i] = NULL;
}

static hashlist *NEAR newhash(com, path, cost, next)
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
			(*nextp) -> type = hp -> type;
			(*nextp) -> hits = hp -> hits;
			nextp = &((*nextp) -> next);
		}
	}
	return(new);
}

static hashlist *NEAR findhash(com, n)
char *com;
int n;
{
	hashlist *hp;

	for (hp = hashtable[n]; hp; hp = hp -> next)
		if (!strpathcmp(com, hp -> command)) return(hp);
	return(NULL);
}

static VOID NEAR rmhash(com, n)
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

static int NEAR isexecute(path, dirok, exe)
char *path;
int dirok, exe;
{
	struct stat st;
	int d;

	if (Xstat(path, &st) < 0) return(-1);
	d = ((st.st_mode & S_IFMT) == S_IFDIR);
	if (!exe) return(d);
	if (d) return(dirok ? d : -1);
	return(Xaccess(path, X_OK));
}

#if	MSDOS
static int NEAR extaccess(path, ext, len, exe)
char *path, *ext;
int len, exe;
{
	if (ext) {
		if (isexecute(path, 0, exe) >= 0) {
			if (!(strpathcmp(ext, "COM"))) return(0);
			else if (!(strpathcmp(ext, "EXE"))) return(CM_EXE);
			else if (!(strpathcmp(ext, "BAT"))) return(CM_BATCH);
		}
	}
	else {
		path[len++] = '.';
		strcpy(&(path[len]), "COM");
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT);
		strcpy(&(path[len]), "EXE");
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT | CM_EXE);
		strcpy(&(path[len]), "BAT");
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT | CM_BATCH);
	}
	return(-1);
}
#endif	/* MSDOS */

int searchhash(hpp, com, search)
hashlist **hpp;
char *com, *search;
{
#if	MSDOS
	char *ext;
#endif
	char *cp, *tmp, *next, *path;
	int len, dlen, cost, size, ret, recalc;
#ifndef	_NOUSEHASH
	hashlist *hp;
	int n, duperrno;

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
#endif

#if	MSDOS
	if ((ext = strrchr(com, '.')) && strdelim(++ext, 0)) ext = NULL;
#endif
	if (strdelim(com, 1)) {
#if	MSDOS
		len = strlen(com);
		path = malloc2(len + EXTWIDTH + 1);
		strcpy(path, com);
		ret = extaccess(path, ext, len, 0);
		free(path);
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
	path = malloc2(len + 2 + EXTWIDTH + 1);
	path[0] = '.';
	path[1] = _SC_;
	strcpy(&(path[2]), com);
# if	MSDOS
	if ((ret = extaccess(path, ext, len + 2, 1)) < 0) free(path);
# else
	if ((ret = isexecute(path, 0, 0)) < 0) free(path);
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

	recalc = 0;
	if ((next = (search) ? search : getconstvar("PATH"))) {
		len = strlen(com);
		size = ret = 0;
		path = NULL;
		cost = 1;
		for (cp = next; cp; cp = next) {
			next = cp;
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
			if (_dospath(cp)) next += 2;
#endif
			if (*next != _SC_) recalc = CM_RECALC;
			next = strchr(next, PATHDELIM);
			dlen = (next) ? (next++) - cp : strlen(cp);
			if (!dlen) tmp = NULL;
			else {
				tmp = _evalpath(cp, cp + dlen, 0);
				dlen = strlen(tmp);
			}
			if (dlen + len + 1 + EXTWIDTH + 1 > size) {
				size = dlen + len + 1 + EXTWIDTH + 1;
				path = realloc2(path, size);
			}
			if (tmp) {
				strncpy2(path, tmp, dlen);
				free(tmp);
				dlen = strcatdelim(path) - path;
			}
			strncpy2(&(path[dlen]), com, len);
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
		if (path) free(path);
	}

	return(CM_NOTFOUND);
}

char *searchexecpath(path, search)
char *path, *search;
{
	hashlist *hp;
	int type;

	if ((type = searchhash(&hp, path, search)) & CM_NOTFOUND) return(NULL);
	if (type & CM_FULLPATH) return(strdup2(path));
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

# ifndef	NOUID
static int NEAR completeuser(name, len, argc, argvp)
char *name;
int len, argc;
char ***argvp;
{
	struct passwd *pwd;
	char *new;
	int size;

	len = strlen(name);
#  ifdef	DEBUG
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
		size = strlen(pwd -> pw_name);
		new = malloc2(size + 2 + 1);
		new[0] = '~';
		strcatdelim2(&(new[1]), pwd -> pw_name, NULL);
		if (finddupl(new, argc, *argvp)) {
			free(new);
			continue;
		}

		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = new;
	}
	_mtrace_file = "endpwent(start)";
	endpwent();
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "endpwent(end)";
		malloc(0);	/* dummy alloc */
	}
#  else	/* !DEBUG */
	setpwent();
	while ((pwd = getpwent())) {
		if (strnpathcmp(name, pwd -> pw_name, len)) continue;
		size = strlen(pwd -> pw_name);
		new = malloc2(size + 2 + 1);
		new[0] = '~';
		strcatdelim2(&(new[1]), pwd -> pw_name, NULL);
		if (finddupl(new, argc, *argvp)) {
			free(new);
			continue;
		}

		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = new;
	}
	endpwent();
#  endif	/* !DEBUG */
	return(argc);
}
# endif	/* !NOUID */

static int NEAR completefile(file, len, argc, argvp, dir, dlen, exe)
char *file;
int len, argc;
char ***argvp, *dir;
int dlen, exe;
{
	DIR *dirp;
	struct dirent *dp;
	char *cp, *new, path[MAXPATHLEN];
	int d, size;

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

		if ((d = isexecute(path, 1, exe)) < 0) continue;

		new = malloc2(size + 1 + 1);
		strncpy(new, dp -> d_name, size);
		if (d) new[size++] = _SC_;
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

static int NEAR completeexe(file, len, argc, argvp)
char *file;
int len, argc;
char ***argvp;
{
	char *cp, *tmp, *next;
	int dlen;

# ifdef	CWDINPATH
	argc = completefile(file, len, argc, argvp, ".", 1, 1);
# endif
	if (!(next = getconstvar("PATH"))) return(argc);
	for (cp = next; cp; cp = next) {
# if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
		if (_dospath(cp)) next = strchr(&(cp[2]), PATHDELIM);
		else
# endif
		next = strchr(cp, PATHDELIM);
		dlen = (next) ? (next++) - cp : strlen(cp);
		tmp = _evalpath(cp, cp + dlen, 0);
		dlen = strlen(tmp);
		argc = completefile(file, len, argc, argvp, tmp, dlen, 1);
		free(tmp);
	}
	return(argc);
}

int completepath(path, len, argc, argvp, exe)
char *path;
int len, argc;
char ***argvp;
int exe;
{
	char *file, *dir;
	int dlen;

	dir = path;
	dlen = 0;
# if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
	if (_dospath(path)) {
		dir += 2;
		dlen = 2;
	}
# endif

	if ((file = strrdelim(dir, 0))) {
		dlen += (file == dir) ? 1 : file - dir;
		return(completefile(file + 1, strlen(file + 1), argc, argvp,
			path, dlen, exe));
	}
# ifndef	NOUID
	else if (*path == '~')
		return(completeuser(&(path[1]), len - 1, argc, argvp));
# endif
	else if (exe && dir == path)
		return(completeexe(path, len, argc, argvp));
	return(completefile(path, len, argc, argvp, ".", 1, exe));
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

static int NEAR addmeta(s1, s2, quoted, flags)
char *s1, *s2;
int quoted, flags;
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
			if (!(flags & EA_KEEPMETA) && s2[i + 1] == PMETA) i++;
		}
		else if (!quoted && s2[i] == '\'') {
			if (s1) s1[j] = PMETA;
			j++;
		}
		else if (iskanji1(s2, i)) {
			if (s1) s1[j] = s2[i];
			j++;
			i++;
		}
#ifdef	CODEEUC
		else if (isekana(s2, i)) {
			if (s1) s1[j] = s2[i];
			j++;
			i++;
		}
#endif
		if (s1) s1[j] = s2[i];
	}
	return(j);
}

char *catvar(argv, delim)
char *argv[];
int delim;
{
	char *cp;
	int i, len;

	if (!argv) return(NULL);
	for (i = len = 0; argv[i]; i++) len += strlen(argv[i]);
	if (i < 1) return(strdup2(""));
	len += (delim) ? i - 1 : 0;
	cp = malloc2(len + 1);
	len = strcpy2(cp, argv[0]) - cp;
	for (i = 1; argv[i]; i++) {
		if (delim) cp[len++] = delim;
		len = strcpy2(&(cp[len]), argv[i]) - cp;
	}
	return(cp);
}

int countvar(var)
char **var;
{
	int i;

	if (!var) return(0);
	for (i = 0; var[i]; i++);
	return(i);
}

VOID freevar(var)
char **var;
{
	int i, duperrno;

	duperrno = errno;
	if (var) {
		for (i = 0; var[i]; i++) free(var[i]);
		free(var);
	}
	errno = duperrno;
}

char **duplvar(var, margin)
char **var;
int margin;
{
	char **dupl;
	int i, n;

	if (margin < 0) {
		if (!var) return(NULL);
		margin = 0;
	}
	n = countvar(var);
	dupl = (char **)malloc2((n + margin + 1) * sizeof(char *));
	for (i = 0; i < n; i++) dupl[i] = strdup2(var[i]);
	dupl[i] = NULL;
	return(dupl);
}

int parsechar(s, len, spc, flags, qp, pqp)
char *s;
int len, spc, flags, *qp, *pqp;
{
	if (*s == *qp) {
		if (!pqp) *qp = '\0';
		else {
			*qp = *pqp;
			*pqp = '\0';
		}
		return(PC_CLQUOTE);
	}
	else if (iskanji1(s, 0)) return(PC_WORD);
#ifdef	CODEEUC
	else if (isekana(s, 0)) return(PC_WORD);
#endif
#ifdef	BASHSTYLE
	else if (*qp == '`') return(PC_BQUOTE);
#endif
	else if (*qp == '\'') return(PC_SQUOTE);
	else if (spc && *s == spc) return(*s);
	else if (ismeta(s, 0, *qp, len)) return(PC_META);
#ifdef	BASHSTYLE
	/* bash can include `...` in "..." */
	else if ((flags & EA_BACKQ) && *s == '`') {
		if (pqp && *qp) *pqp = *qp;
		*qp = *s;
		return(PC_OPQUOTE);
	}
#else
	else if (*qp == '`') return(PC_BQUOTE);
#endif
	else if (*qp) return(PC_DQUOTE);
	else if (!(flags & EA_NOEVALQ) && *s == '\'') {
		*qp = *s;
		return(PC_OPQUOTE);
	}
	else if (*s == '"') {
		*qp = *s;
		return(PC_OPQUOTE);
	}
#ifndef	BASHSTYLE
	/* bash can include `...` in "..." */
	else if ((flags & EA_BACKQ) && *s == '`') {
		if (pqp && *qp) *pqp = *qp;
		*qp = *s;
		return(PC_OPQUOTE);
	}
#endif

	return(PC_NORMAL);
}

static int NEAR skipvar(bufp, eolp, ptrp, qed)
char **bufp;
int *eolp, *ptrp, qed;
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
			if (skipvarvalue(*bufp, ptrp, ")", qed, 0, 1) < 0)
				return(-1);
		}
		else {
			(*ptrp)++;
			if (skipvarvalue(*bufp, ptrp, "))", qed, 0, 1) < 0)
				return(-1);
		}
		if (eolp) *eolp = *ptrp;
		return('(');
	}
#endif

	if ((*bufp)[*ptrp] != '{') {
		if ((*bufp)[*ptrp] != qed) (*ptrp)++;
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
		if (!strchr("-=?+", mode)) return(-1);
		mode |= 0x80;
	}
	else if (strchr("-=?+", i)) mode = i;
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
	if (skipvarvalue(*bufp, ptrp, "}", qed, 0) < 0) return(-1);
# else
	if (skipvarvalue(*bufp, ptrp, "}", qed, 0, 0) < 0) return(-1);
# endif
#else	/* BASHSTYLE */
# ifdef	MINIMUMSHELL
	if (skipvarvalue(*bufp, ptrp, "}", qed, 1) < 0) return(-1);
# else
	if (skipvarvalue(*bufp, ptrp, "}", qed, 1, 0) < 0) return(-1);
# endif
#endif	/* BASHSTYLE */
	return(mode);
}

#ifdef	MINIMUMSHELL
static int NEAR skipvarvalue(s, ptrp, next, qed, nonl)
char *s;
int *ptrp;
char *next;
int qed, nonl;
#else
static int NEAR skipvarvalue(s, ptrp, next, qed, nonl, nest)
char *s;
int *ptrp;
char *next;
int qed, nonl, nest;
#endif
{
#ifdef	BASHSTYLE
	int pq;
#endif
	char *cp;
	int pc, q, len;

#ifdef	BASHSTYLE
	pq = '\0';
#endif
	q = '\0';
	len = strlen(next);
	while (s[*ptrp]) {
#ifdef	BASHSTYLE
	/* bash can include `...` in "..." */
		pc = parsechar(&(s[*ptrp]), -1, '$', EA_BACKQ, &q, &pq);
#else
		pc = parsechar(&(s[*ptrp]), -1, '$', EA_BACKQ, &q, NULL);
#endif
		if (pc == PC_WORD || pc == PC_META) (*ptrp)++;
		else if (pc == '$') {
			if (!s[++(*ptrp)]) return(0);
			cp = s;
			if (skipvar(&cp, NULL, ptrp, (q) ? q : qed) < 0)
				return(-1);
			*ptrp += (cp - s);
			continue;
		}
		else if (pc != PC_NORMAL) /*EMPTY*/;
		else if (nonl && s[*ptrp] == '\n') return(-1);
#ifndef	MINIMUMSHELL
		else if (nest && s[*ptrp] == '(') {
			int tmpq;

			tmpq = (q) ? q : qed;
			(*ptrp)++;
			if (skipvarvalue(s, ptrp, ")", tmpq, nonl, nest) < 0)
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
char *s, *pattern;
int plen, mode;
{
	reg_t *re;
	char *cp, *ret, *tmp;
	int c, len;

	if (!s || !*s) return(NULL);
	tmp = strndup2(pattern, plen);
	pattern = evalarg(tmp, '\0', EA_BACKQ);
	free(tmp);
	re = regexp_init(pattern, -1);
	free(pattern);
	if (!re) return(NULL);
	ret = NULL;
	len = strlen(s);
	if ((mode & ~0x80) != '#') {
		if (mode & 0x80) for (cp = s; cp < s + len; cp++) {
			if (regexp_exec(re, cp, 0)) {
				ret = cp;
				break;
			}
		}
		else for (cp = s + len - 1; cp >= s; cp--) {
			if (regexp_exec(re, cp, 0)) {
				ret = cp;
				break;
			}
		}
		if (ret) ret = strndup2(s, ret - s);
	}
	else {
		tmp = strdup2(s);
		if (mode & 0x80)
		for (cp = tmp + len; cp >= tmp; cp--) {
			*cp = '\0';
			if (regexp_exec(re, tmp, 0)) {
				ret = cp;
				break;
			}
		}
		else for (cp = tmp + 1; cp <= tmp + len; cp++) {
			c = *cp;
			*cp = '\0';
			if (regexp_exec(re, tmp, 0)) {
				ret = cp;
				break;
			}
			*cp = c;
		}
		if (ret) ret = strdup2(&(s[ret - tmp]));
		free(tmp);
	}
	regexp_free(re);
	return(ret);
}

static char **NEAR removevar(var, pattern, plen, mode)
char **var, *pattern;
int plen, mode;
{
	char *cp, **new;
	int i, n;

	n = countvar(var);
	new = (char **)malloc2((n + 1) * sizeof(char *));
	for (i = 0; i < n; i++) {
		if ((cp = removeword(var[i], pattern, plen, mode)))
			new[i] = cp;
		else new[i] = strdup2(var[i]);
	}
	new[i] = NULL;
	return(new);
}
#endif	/* !MINIMUMSHELL */

#ifdef	MINIMUMSHELL
static char *NEAR evalshellparam(c, quoted)
int c, quoted;
#else
static char *NEAR evalshellparam(c, quoted, pattern, plen, modep)
int c, quoted;
char *pattern;
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
			if (!quoted) {
				cp = catvar(&(arglist[1]), sp);
				break;
			}
#ifdef	BASHSTYLE
	/* bash uses IFS instead of a space as a separator */
			if ((cp = getconstvar("IFS")) && *cp)
				sp = *cp;
#endif

			for (i = j = 0; arglist[i + 1]; i++)
				j += addmeta(NULL,
					arglist[i + 1], quoted, EA_KEEPMETA);
			if (i <= 0) cp = strdup2("");
			else {
				j += (i - 1) * 3;
				cp = malloc2(j + 1);
				j = addmeta(cp, arglist[1],
					quoted, EA_KEEPMETA);
				for (i = 2; arglist[i]; i++) {
					cp[j++] = quoted;
					cp[j++] = sp;
					cp[j++] = quoted;
					j += addmeta(&(cp[j]), arglist[i],
						quoted, EA_KEEPMETA);
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
			if (quoted && (cp = getconstvar("IFS")))
				sp = (*cp) ? *cp : '\0';
#endif
			cp = catvar(&(arglist[1]), sp);
			break;
		case '#':
			if (!argvar) break;
			snprintf2(tmp, sizeof(tmp), "%d",
				countvar(argvar + 1));
			cp = tmp;
			break;
		case '?':
			snprintf2(tmp, sizeof(tmp), "%d",
				(getretvalfunc) ? (*getretvalfunc)() : 0);
			cp = tmp;
			break;
		case '$':
			if (!getpidfunc) pid = getpid();
			else pid = (*getpidfunc)();
			snprintf2(tmp, sizeof(tmp), "%id", pid);
			cp = tmp;
			break;
		case '!':
			if (getlastpidfunc
			&& (pid = (*getlastpidfunc)()) >= 0) {
				snprintf2(tmp, sizeof(tmp), "%id", pid);
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
#ifndef	MINIMUMSHELL
	freevar(new);
#endif
	return(cp);
}

static int NEAR replacevar(arg, cpp, s, len, vlen, mode, quoted)
char *arg, **cpp;
int s, len, vlen, mode, quoted;
{
	char *val;
	int i;

	if (!mode) return(0);
	if (mode == '+') {
		if (!*cpp) return(0);
	}
	else if (*cpp) return(0);
	else if (mode == '=' && !isidentchar(*arg)) return(-1);

	val = strndup2(&(arg[s]), vlen);
	*cpp = evalarg(val, quoted,
		(mode == '=' || mode == '?')
		? EA_STRIPQ | EA_BACKQ : EA_BACKQ);
	free(val);
	if (!*cpp) return(-1);

	if (mode == '=') {
#ifdef	FD
		demacroarg(cpp);
#endif
		if (setvar(arg, *cpp, len) < 0) {
			free(*cpp);
			*cpp = NULL;
			return(-1);
		}
#ifdef	BASHSTYLE
	/* bash does not evaluates a quoted string in substitution itself */
		free(*cpp);
		val = strndup2(&(arg[s]), vlen);
		*cpp = evalarg(val, quoted, EA_BACKQ);
		free(val);
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
		for (i = 0; i < len; i++) fputc(arg[i], stderr);
		fprintf2(stderr, ": %k",
			(vlen > 0) ? *cpp : "parameter null or not set");
		fputnl(stderr);
		free(*cpp);
		*cpp = NULL;
		if (exitfunc) (*exitfunc)();
		return(-2);
	}
	return(mode);
}

static char *NEAR insertarg(buf, ptr, arg, olen, nlen)
char *buf;
int ptr;
char *arg;
int olen, nlen;
{
	if (nlen <= olen) return(buf);
	buf = realloc2(buf, ptr + nlen + (int)strlen(arg) - olen + 1);
	return(buf);
}

static int NEAR evalvar(bufp, ptr, argp, quoted)
char **bufp;
int ptr;
char **argp;
int quoted;
{
#ifndef	MINIMUMSHELL
	char tmp[MAXLONGWIDTH + 1];
#endif
	char *cp, *new, *arg, *top;
	int i, c, n, len, vlen, s, nul, mode;

	new = NULL;
	arg = *argp;
	top = *argp + 1;
	n = 0;
	if ((mode = skipvar(&top, &s, &n, quoted)) < 0) return(-1);

	*argp = top + n - 1;
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
	if (strchr("-=?+", mode & ~0x80)) mode &= ~0x80;
	else if (mode == '(') {
		if (!posixsubstfunc) return(-1);
		for (;;) {
			if ((new = (*posixsubstfunc)(top, &n))) break;
			if (n < 0 || !top[n]) return(-1);
			if (skipvarvalue(top, &n, ")", quoted, 0, 1) < 0)
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
	else if (isdigit2(*top)) {
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
		cp = evalshellparam(c, quoted);
#else
		cp = evalshellparam(c, quoted,
			top + s, vlen, (nul < 0) ? &mode : NULL);
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
		snprintf2(tmp, sizeof(tmp), "%d",
			(cp) ? strlen(cp) : 0);
		cp = tmp;
	}
	else if (nul == -1) {
		new = removeword(cp, top + s, vlen, mode);
		if (new) cp = new;
	}
	else
#endif	/* !MINIMUMSHELL */
	if ((mode = replacevar(top, &cp, s, len, vlen, mode, quoted)) < 0) {
		if (new) free(new);
		return(mode);
	}
	else if (mode) {
		if (new) free(new);
		new = cp;
	}

	if (!mode && (c != '@' || !quoted)) {
		vlen = addmeta(NULL, cp, quoted, EA_KEEPMETA);
		*bufp = insertarg(*bufp, ptr, arg, *argp - arg + 1, vlen);
		addmeta(&((*bufp)[ptr]), cp, quoted, EA_KEEPMETA);
	}
	else if (!cp) vlen = 0;
	else if (c == '@' && !*cp && quoted
	&& ptr > 0 && (*bufp)[ptr - 1] == quoted && *(*argp + 1) == quoted) {
		vlen = 0;
		ptr--;
		(*argp)++;
	}
	else {
		vlen = strlen(cp);
		*bufp = insertarg(*bufp, ptr, arg, *argp - arg + 1, vlen);
		strncpy(&((*bufp)[ptr]), cp, vlen);
	}

	ptr += vlen;
	if (new) free(new);
	return(ptr);
}

#if	defined (FD) && !defined (NOUID)
uidtable *finduid(uid, name)
uid_t uid;
char *name;
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
		pwd = getpwuid(uid);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getpwuid(end)";
			malloc(0);	/* dummy alloc */
		}
# else
		pwd = getpwuid(uid);
# endif
	}

	if (!pwd) return(NULL);
	uidlist = b_realloc(uidlist, maxuid, uidtable);
	uidlist[maxuid].uid = pwd -> pw_uid;
	uidlist[maxuid].name = strdup2(pwd -> pw_name);
	uidlist[maxuid].home = strdup2(pwd -> pw_dir);
	return(&(uidlist[maxuid++]));
}

gidtable *findgid(gid, name)
gid_t gid;
char *name;
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
		grp = getgrgid(gid);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getgrgid(end)";
			malloc(0);	/* dummy alloc */
		}
# else
		grp = getgrgid(gid);
# endif
	}

	if (!grp) return(NULL);
	gidlist = b_realloc(gidlist, maxgid, gidtable);
	gidlist[maxgid].gid = grp -> gr_gid;
	gidlist[maxgid].name = strdup2(grp -> gr_name);
	gidlist[maxgid].gr_mem = duplvar(grp -> gr_mem, -1);
	gidlist[maxgid].ismem = 0;
	return(&(gidlist[maxgid++]));
}

int isgroupmember(gid)
gid_t gid;
{
	uidtable *up;
	gidtable *gp;
	int i;

	if (!(gp = findgid(gid, NULL))) return(0);
	if (!(gp -> ismem)) {
		gp -> ismem++;
		if (gp -> gr_mem && (up = finduid(geteuid(), NULL)))
		for (i = 0; gp -> gr_mem[i]; i++) {
			if (!strpathcmp(up -> name, gp -> gr_mem[i])) {
				gp -> ismem++;
				break;
			}
		}
		freevar(gp -> gr_mem);
		gp -> gr_mem = NULL;
	}

	return(gp -> ismem - 1);
}

# ifdef	DEBUG
VOID freeidlist(VOID_A)
{
	int i;

	if (uidlist) {
		for (i = 0; i < maxuid; i++) {
			free(uidlist[i].name);
			free(uidlist[i].home);
		}
		free(uidlist);
	}
	if (gidlist) {
		for (i = 0; i < maxgid; i++) {
			free(gidlist[i].name);
			freevar(gidlist[i].gr_mem);
		}
		free(gidlist);
	}
	uidlist = NULL;
	gidlist = NULL;
	maxuid = maxgid = 0;
}
# endif
#endif	/* FD && !NOUID */

static char *NEAR replacebackquote(buf, ptrp, bbuf, rest)
char *buf;
int *ptrp;
char *bbuf;
int rest;
{
	char *tmp;
	int len, size;

	stripquote(bbuf, 0);
	if (!(tmp = (*backquotefunc)(bbuf))) return(buf);
	len = addmeta(NULL, tmp, '\0', 0);
	size = *ptrp + len + rest + 1;
	buf = realloc2(buf, size);
	addmeta(&(buf[*ptrp]), tmp, '\0', 0);
	*ptrp += len;
	free(tmp);

	return(buf);
}

char *gethomedir(VOID_A)
{
#ifndef	NOUID
# ifdef	FD
	uidtable *up;
# else
	struct passwd *pwd;
# endif
#endif	/* !NOUID */
	char *cp;

	if ((cp = getconstvar("HOME"))) return(cp);
#ifndef	NOUID
# ifdef	FD
	if ((up = finduid(getuid(), NULL))) return(up -> home);
# else	/* !FD */
#  ifdef	DEBUG
	_mtrace_file = "getpwuid(start)";
	pwd = getpwuid(getuid());
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "getpwuid(end)";
		malloc(0);	/* dummy alloc */
	}
#  else
	pwd = getpwuid(getuid());
#  endif
	if (pwd) return(pwd -> pw_dir);
# endif	/* !FD */
#endif	/* !NOUID */
	return(NULL);
}

#ifndef	MINIMUMSHELL
int evalhome(bufp, ptr, argp)
char **bufp;
int ptr;
char **argp;
{
	char *cp, *top;
	int len, vlen;

	top = *argp + 1;

	len = ((cp = strdelim(top, 0))) ? (cp - top) : strlen(top);
	if (!len) cp = gethomedir();
# ifdef	FD
	else if (len == sizeof("FD") - 1 && !strnpathcmp(top, "FD", len))
		cp = progpath;
# endif
	else {
# ifdef	NOUID
		cp = NULL;
# else	/* !NOUID */
#  ifdef	FD
		uidtable *up;

		cp = strndup2(top, len);
		up = finduid(0, cp);
		free(cp);
		cp = (up) ? up -> home : NULL;
#  else	/* !FD */
		struct passwd *pwd;

		cp = strndup2(top, len);
#   ifdef	DEBUG
		_mtrace_file = "getpwnam(start)";
		pwd = getpwnam(cp);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "getpwnam(end)";
			malloc(0);	/* dummy alloc */
		}
#   else
		pwd = getpwnam(cp);
#   endif
		free(cp);
		cp = (pwd) ? pwd -> pw_dir : NULL;
#  endif	/* !FD */
# endif	/* !NOUID */
	}

	if (!cp) {
		vlen = len;
		(*bufp)[ptr++] = '~';
		cp = top;
	}
	else {
		vlen = strlen(cp);
		*bufp = insertarg(*bufp, ptr, *argp, len + 1, vlen);
	}
	strncpy(&((*bufp)[ptr]), cp, vlen);
	ptr += vlen;
	*argp += len;
	return(ptr);
}
#endif	/* !MINIMUMSHELL */

char *evalarg(arg, qed, flags)
char *arg;
int qed, flags;
{
#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
	char path[MAXPATHLEN], alias[MAXPATHLEN];
#endif
#ifdef	BASHSTYLE
	int pq;
#endif
	char *cp, *buf, *bbuf;
	int i, j, pc, prev, q;

#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
	if (*arg == '"' && (i = strlen(arg)) > 2 && arg[i - 1] == '"') {
		strncpy2(path, &(arg[1]), i - 2);
		if (shortname(path, alias) == alias) {
			if (flags & (EA_STRIPQ | EA_STRIPQLATER))
				return(strdup2(alias));
			i = strlen(alias);
			buf = malloc2(i + 2 + 1);
			buf[0] = '"';
			memcpy(&(buf[1]), alias, i);
			buf[++i] = '"';
			buf[++i] = '\0';
			return(buf);
		}
	}
#endif

	i = strlen(arg) + 1;
	buf = malloc2(i);
	if (!backquotefunc) flags &= ~EA_BACKQ;
	bbuf = (flags & EA_BACKQ) ? malloc2(i) : NULL;
#ifdef	BASHSTYLE
	pq = '\0';
#endif
	prev = q = '\0';
	i = j = 0;

	for (cp = arg; *cp; prev = *(cp++)) {
#ifdef	BASHSTYLE
	/* bash can include `...` in "..." */
		pc = parsechar(cp, -1, '$', flags, &q, &pq);
#else
		pc = parsechar(cp, -1, '$', flags, &q, NULL);
#endif
		if (pc == PC_CLQUOTE) {
			if (*cp == '`') {
				bbuf[j] = '\0';
				buf = replacebackquote(buf, &i,
					bbuf, strlen(cp + 1));
				j = 0;
			}
			else if (!(flags & EA_STRIPQ)) buf[i++] = *cp;
		}
		else if (pc == PC_WORD) {
			if (q == '`') {
				bbuf[j++] = *cp++;
				bbuf[j++] = *cp;
			}
			else {
				buf[i++] = *cp++;
				buf[i++] = *cp;
			}
		}
		else if (pc == PC_BQUOTE) bbuf[j++] = *cp;
		else if (pc == PC_SQUOTE || pc == PC_DQUOTE) buf[i++] = *cp;
		else if (pc == '$') {
			int tmpq;

			tmpq = (q) ? q : qed;
			if (!cp[1]) buf[i++] = *cp;
#ifdef	FAKEMETA
# ifdef	MINIMUMSHELL
			else if (cp[1] == '$') cp++;
# else
			else if (cp[1] == '$' || cp[1] == '~') cp++;
# endif
#endif
			else if ((i = evalvar(&buf, i, &cp, tmpq)) < 0) {
				if (bbuf) free(bbuf);
				free(buf);
				if (i < -1) *arg = '\0';
				return(NULL);
			}
		}
		else if (pc == PC_META) {
			cp++;
			if (flags & EA_KEEPMETA) buf[i++] = PMETA;
			else if (*cp == '$') /*EMPTY*/;
			else if ((flags & EA_BACKQ) && *cp == '`') /*EMPTY*/;
			else if ((flags & EA_STRIPQ)
			&& (*cp == '\'' || *cp == '"'))
				/*EMPTY*/;
			else buf[i++] = PMETA;
			buf[i++] = *cp;
		}
		else if (pc == PC_OPQUOTE) {
			if (*cp == '`') j = 0;
			else if (!(flags & EA_STRIPQ)) buf[i++] = *cp;
		}
		else if (pc != PC_NORMAL) /*EMPTY*/;
#ifndef	MINIMUMSHELL
		else if (*cp == '~' && (!prev || prev == ':' || prev == '='))
			i = evalhome(&buf, i, &cp);
#endif
		else buf[i++] = *cp;
	}
#ifndef	BASHSTYLE
	/* bash does not allow unclosed quote */
	if ((flags & EA_BACKQ) && q == '`') {
		bbuf[j] = '\0';
		buf = replacebackquote(buf, &i, bbuf, 0);
	}
#endif
	if (bbuf) free(bbuf);
	buf[i] = '\0';

	if (flags & EA_STRIPQLATER) stripquote(buf, EA_STRIPQ);

	return(buf);
}

int evalifs(argc, argvp, ifs)
int argc;
char ***argvp, *ifs;
{
	char *cp;
	int i, j, n, pc, quote;

	for (n = 0; n < argc; n++) {
		for (i = 0, quote = '\0'; (*argvp)[n][i]; i++) {
			pc = parsechar(&((*argvp)[n][i]), -1,
				'\0', 0, &quote, NULL);
			if (pc == PC_WORD || pc == PC_META) i++;
			else if (pc != PC_NORMAL) /*EMPTY*/;
			else if (strchr2(ifs, (*argvp)[n][i])) {
				for (j = i + 1; (*argvp)[n][j]; j++)
					if (!strchr2(ifs, (*argvp)[n][j]))
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
				cp = strdup2(&((*argvp)[n][j]));
				*argvp = (char **)realloc2(*argvp,
					(argc + 2) * sizeof(char *));
				memmove((char *)(&((*argvp)[n + 2])),
					(char *)(&((*argvp)[n + 1])),
					(argc++ - n) * sizeof(char *));
				(*argvp)[n + 1] = cp;
				break;
			}
		}
		if (!i) {
			free((*argvp)[n--]);
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
			*argvp = (char **)realloc2(*argvp,
				(argc + i) * sizeof(char *));
			memmove((char *)(&((*argvp)[n + i])),
				(char *)(&((*argvp)[n + 1])),
				(argc - n) * sizeof(char *));
			argc += i - 1;
		}
		free((*argvp)[n]);
		memmove((char *)(&((*argvp)[n])),
			(char *)wild, i * sizeof(char *));
		free(wild);
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
			stripped = 1;
			if (flags & EA_STRIPQ) continue;
		}
		else if (pc == PC_WORD) arg[j++] = arg[i++];
		else if (pc == PC_META) {
			stripped = 1;
			if ((flags & EA_KEEPMETA)
			|| quote == '\''
			|| (quote == '"' && !strchr(DQ_METACHAR, arg[i + 1])))
				arg[j++] = arg[i];
			i++;
		}

		arg[j++] = arg[i];
	}
	arg[j] = '\0';
	return(stripped);
}

char *_evalpath(path, eol, flags)
char *path, *eol;
int flags;
{
#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
	char alias[MAXPATHLEN];
	int top = -1;
#endif
	char *cp, *tmp;
	int i, j, c, pc, size, quote;

	if (eol) i = eol - path;
	else i = strlen(path);
	cp = strndup2(path, i);

	if (!(tmp = evalarg(cp, '\'', EA_KEEPMETA))) {
		*cp = '\0';
		return(cp);
	}
	free(cp);
	cp = tmp;

	size = strlen(cp) + 1;
	tmp = malloc2(size);
	quote = '\0';
	for (i = j = c = 0; cp[i]; c = cp[i++]) {
		pc = parsechar(&(cp[i]), -1, '\0', 0, &quote, NULL);
		if (pc == PC_CLQUOTE) {
#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
			if ((flags & EA_NOEVALQ) && top >= 0 && ++top < j) {
				tmp[j] = '\0';
				if (shortname(&(tmp[top]), alias) == alias) {
					int len;

					len = strlen(alias);
					size += top + len - j;
					j = top + len;
					tmp = realloc2(tmp, size);
					strncpy(&(tmp[top]), alias, len);
				}
			}
			top = -1;
#endif
			if (!(flags & EA_NOEVALQ)) continue;
		}
		else if (pc == PC_WORD) tmp[j++] = cp[i++];
		else if (pc == PC_META) {
			i++;
			if ((flags & EA_KEEPMETA)
			|| quote == '\''
			|| (quote == '"' && !strchr(DQ_METACHAR, cp[i])))
				tmp[j++] = PMETA;
		}
		else if (pc == PC_OPQUOTE) {
#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
			if (cp[i] == '"') top = j;
#endif
			if (!(flags & EA_NOEVALQ)) continue;
		}
		else if (pc != PC_NORMAL) /*EMPTY*/;
		else if (!(flags & EA_NOUNIQDELIM)
		&& cp[i] == _SC_ && c == _SC_)
			continue;
		tmp[j++] = cp[i];
	}
	tmp[j] = '\0';
	free(cp);
	return(tmp);
}

char *evalpath(path, flags)
char *path;
int flags;
{
	char *cp;

	if (!path || !*path) return(path);
	for (cp = path; *cp == ' ' || *cp == '\t'; cp++);
	cp = _evalpath(cp, NULL, flags);
	free(path);
	return(cp);
}
