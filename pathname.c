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

#if	MSDOS
#include <process.h>
#include "unixemu.h"
# ifndef	FD
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
# ifdef	_NOROCKRIDGE
extern DIR *_Xopendir __P_((char *));
# define	Xopendir	_Xopendir
# else
extern DIR *Xopendir __P_((char *));
# endif
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern char *Xgetwd __P_((char *));
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
static VOID NEAR allocerror __P_((VOID_A));
char *malloc2 __P_((ALLOC_T));
char *realloc2 __P_((VOID_P, ALLOC_T));
char *strdup2 __P_((char *));
char *strcpy2 __P_((char *, char *));
char *strncpy2 __P_((char *, char *, int));
int isdotdir __P_((char *));
# if	MSDOS
DIR *Xopendir __P_((char *));
int Xclosedir __P_((DIR *));
struct dirent *Xreaddir __P_((DIR *));
# else
# define	Xopendir	opendir
# define	Xclosedir	closedir
# define	Xreaddir	readdir
# endif
# ifdef DJGPP
extern char *Xgetwd __P_((char *));
# else
#  ifdef	USEGETWD
#  define	Xgetwd		(char *)getwd
#  else
#  define	Xgetwd(p)	(char *)getcwd(p, MAXPATHLEN)
#  endif
# endif
#define	Xstat(f, s)	(stat(f, s) ? -1 : 0)
#define	Xaccess		access
#define	_dospath(s)	(isalpha(*(s)) && (s)[1] == ':')
#endif	/* !FD */

static char *NEAR getvar __P_((char *, int));
static int NEAR setvar __P_((char *, char *, int));
#ifdef	_NOORIGGLOB
static char *NEAR cnvregexp __P_((char *, int));
#else
static int NEAR _regexp_exec __P_((char **, char *));
#endif
static char *NEAR catpath __P_((char *, char *, int *, int, int));
#if	MSDOS
static int NEAR _evalwild __P_((int, char ***, char *, char *, int));
#else
static int NEAR _evalwild __P_((int, char ***, char *, char *, int,
		int, devino_t *));
#endif
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
#ifndef	_NOCOMPLETE
# if	!MSDOS
static int NEAR completeuser __P_((char *, int, int, char ***));
# endif
static int NEAR completefile __P_((char *, int, int, char ***,
	char *, int, int));
static int NEAR completeexe __P_((char *, int, int, char ***));
#endif
static int NEAR addmeta __P_((char *, char *, int, int));
static char *NEAR evalshellparam __P_((int, int));
static int NEAR replacevar __P_((char *, char **, int, int, int, int));
static char *NEAR insertarg __P_((char *, int, char *, int, int));
static int NEAR evalvar __P_((char **, int, char **, int));
static int NEAR evalhome __P_((char **, int, char **));

#ifdef	LSI_C
#include <jctype.h>
#define	iskna		iskana
#define	issjis1		iskanji
#define	issjis2		iskanji2
#else	/* !LSI_C */
#define	KC_SJIS1	0001
#define	KC_SJIS2	0002
#define	KC_KANA		0004
#define	KC_EUCJP	0010
#define	iskna(c)	(kctypetable[(u_char)(c)] & KC_KANA)

# ifndef	issjis1
# define	issjis1(c)	(kctypetable[(u_char)(c)] & KC_SJIS1)
# endif
# ifndef	issjis2
# define	issjis2(c)	(kctypetable[(u_char)(c)] & KC_SJIS2)
# endif

# ifndef	iseuc
# define	iseuc(c)	(kctypetable[(u_char)(c)] & KC_EUCJP)
# endif
#endif	/* !LSI_C */

#define	isekana(s, i)	((u_char)((s)[i]) == 0x8e && iskna((s)[(i) + 1]))

#ifdef	CODEEUC
#define	iskanji1(s, i)	(iseuc((s)[i]) && iseuc((s)[(i) + 1]))
#else
#define	iskanji1(s, i)	(issjis1((s)[i]) && issjis2((s)[(i) + 1]))
#endif

#define	MAXLONGWIDTH	10
#define	BUFUNIT		32
#define	n_size(n)	((((n) / BUFUNIT) + 1) * BUFUNIT)
#define	b_size(n, type) ((((n) / BUFUNIT) + 1) * BUFUNIT * sizeof(type))
#define	b_realloc(ptr, n, type) \
			(((n) % BUFUNIT) ? ((type *)(ptr)) \
			: (type *)realloc2(ptr, b_size(n, type)))
#define	c_malloc(size)	(malloc2((size) = BUFUNIT))
#define	c_realloc(ptr, n, size) \
			(((n) + 1 < (size)) \
			? (ptr) : realloc2(ptr, (size) *= 2))

#ifndef	_NOUSEHASH
hashlist **hashtable = NULL;
#endif
char *(*getvarfunc)__P_((char *, int)) = NULL;
int (*putvarfunc)__P_((char *, int)) = NULL;
int (*getretvalfunc)__P_((VOID_A)) = NULL;
long (*getpidfunc)__P_((VOID_A)) = NULL;
long (*getlastpidfunc)__P_((VOID_A)) = NULL;
char **(*getarglistfunc)__P_((VOID_A)) = NULL;
char *(*getflagfunc)__P_((VOID_A)) = NULL;
int (*checkundeffunc)__P_((char *, char *, int)) = NULL;
VOID (*exitfunc)__P_((VOID_A)) = NULL;
char *(*backquotefunc)__P_((char *)) = NULL;
#if	!MSDOS
int pathignorecase = 0;
#endif	/* !MSDOS */
#ifndef	LSI_C
u_char uppercase[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* 0x00 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,	/* 0x10 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x40 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x50 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x60 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x70 */
	0x58, 0x59, 0x5a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,	/* 0x80 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,	/* 0x90 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,	/* 0xa0 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,	/* 0xb0 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,	/* 0xc0 */
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,	/* 0xd0 */
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,	/* 0xe0 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,	/* 0xf0 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
u_char lowercase[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* 0x00 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,	/* 0x10 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x40 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x50 */
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x60 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x70 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,	/* 0x80 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,	/* 0x90 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,	/* 0xa0 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,	/* 0xb0 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,	/* 0xc0 */
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,	/* 0xd0 */
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,	/* 0xe0 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,	/* 0xf0 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
# if	defined (FD) || defined (FDSH)
extern u_char kctypetable[256];
# else	/* FD || FDSH */
u_char kctypetable[256] = {
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,	/* 0x00 */
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,	/* 0x10 */
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
	   0, 0060, 0060, 0060, 0060, 0060, 0060, 0060,	/* 0x20 */
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,	/* 0x30 */
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,	/* 0x40 */
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,	/* 0x50 */
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,	/* 0x60 */
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,	/* 0x70 */
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0200,
	0002, 0003, 0003, 0003, 0003, 0003, 0003, 0003,	/* 0x80 */
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,	/* 0x90 */
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,
	0002, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xa0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xb0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xc0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xd0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,	/* 0xe0 */
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,	/* 0xf0 */
	0013, 0013, 0013, 0013, 0013, 0010, 0010,    0
};
# endif	/* FD || FDSH */
#endif	/* !LSI_C */

static int skipdotfile = 0;
static char *wildsymbol1 = "?";
static char *wildsymbol2 = "*";
#if	!MSDOS
static uidtable *uidlist = NULL;
static int maxuid = 0;
static gidtable *gidlist = NULL;
static int maxgid = 0;
#endif	/* !MSDOS */


#ifndef	FD
static VOID NEAR allocerror(VOID_A)
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

#if	!MSDOS
int strcasecmp2(s1, s2)
char *s1, *s2;
{
	int c1, c2;

	for (;;) {
		c1 = toupper2(*s1);
		c2 = toupper2(*s2);
		if (c1 != c2) return(c1 - c2);
# ifndef	CODEEUC
		if (issjis1(c1)) {
			s1++;
			s2++;
			if (*s1 != *s2) return(*s1 - *s2);
		}
# endif
		if (!*s1) break;
		s1++;
		s2++;
	}
	return(0);
}
#endif	/* !MSDOS */

int strpathcmp2(s1, s2)
char *s1, *s2;
{
	int c1, c2;

#if	!MSDOS
	if (!pathignorecase) for (;;) {
		if (*s1 != *s2) return(*s1 - *s2);
		if (!*s1) break;
		s1++;
		s2++;
	}
	else
#endif
	for (;;) {
		c1 = toupper2(*s1);
		c2 = toupper2(*s2);
		if (c1 != c2) return(c1 - c2);
#ifndef	CODEEUC
		if (issjis1(c1)) {
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

int strnpathcmp2(s1, s2, n)
char *s1, *s2;
int n;
{
	int c1, c2;

#if	!MSDOS
	if (!pathignorecase) while (n-- > 0) {
		if (*s1 != *s2) return(*s1 - *s2);
		if (!*s1) break;
		s1++;
		s2++;
	}
	else
#endif
	while (n-- > 0) {
		c1 = toupper2(*s1);
		c2 = toupper2(*s2);
		if (c1 != c2) return(c1 - c2);
#ifndef	CODEEUC
		if (issjis1(c1)) {
			if (--n <= 0) break;
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

static char *NEAR getvar(ident, len)
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
	strncpy2(cp + len + 1, value, vlen);

	ret = (putvarfunc) ? (*putvarfunc)(cp, len) : putenv(cp);
	if (ret < 0) free(cp);
	return(ret);
}

#ifdef	_NOORIGGLOB
static char *NEAR cnvregexp(s, len)
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
# if	MSDOS
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
	int i, j, n, plen, size, metachar, quote;

	skipdotfile = (*s == '*' || *s == '?' || *s == '[');
	if (len < 0) len = strlen(s);
	paren = NULL;
	n = plen = size = 0;
	re = (reg_t *)malloc2(1 * sizeof(reg_t));
	re[0] = NULL;
	for (i = 0, quote = '\0'; i < len; i++) {
		cp = NULL;
		metachar = 0;
		if (s[i] == quote) {
			quote = '\0';
			continue;
		}
		else if (quote == '\'');
		else if (isnmeta(s, i, quote, len)) {
			metachar = 1;
			i++;
		}
		else if (quote);
		else if (s[i] == '\'' || s[i] == '"') {
			quote = s[i];
			continue;
		}

		if (paren) {
			paren = c_realloc(paren, plen + 1, size);

			if (quote || metachar) {
#ifdef	BASHSTYLE
	/* bash treats a character quoted by \ in "[]" as a character itself */
				paren[plen++] = PMETA;
# if	!MSDOS
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
				cp = paren;
				paren = NULL;
			}
			else if (!plen && s[i] == '!') paren[plen++] = '\0';
			else if (s[i] == '-') {
				if (i + 1 >= len) j = '\0';
#if	!MSDOS
				else if (!pathignorecase) j = s[i + 1];
#endif
				else j = toupper2(s[i + 1]);

#ifdef	BASHSTYLE
	/* bash does not allows "[b-a]" */
				if (plen && j && j != ']'
				&& j < paren[plen - 1]) {
					free(paren);
					regexp_free(re);
					return(NULL);
				}
#else
				if (plen && j && j < paren[plen - 1]) {
					do {
						if (i + 1 >= len) break;
						i++;
					} while (s[i + 1] != ']');
				}
	/* bash allows "[-a]" */
				else if ((!plen && j != '-')) {
					free(paren);
					regexp_free(re);
					return(NULL);
				}
#endif
				else paren[plen++] = s[i];
			}
			else {
#ifdef	CODEEUC
				if (isekana(s, i)) {
					paren[plen++] = s[i++];
					paren[plen++] = s[i];
				}
				else
#endif
				if (iskanji1(s, i)) {
					paren[plen++] = s[i++];
					paren[plen++] = s[i];
				}
#if	!MSDOS
				else if (!pathignorecase) paren[plen++] = s[i];
#endif
				else paren[plen++] = toupper2(s[i]);
			}
		}
		else if (!quote) switch (s[i]) {
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
			default:
				break;
		}

		if (paren) continue;
		if (!cp) {
			cp = malloc2(2 + 1);
			j = 0;
#ifdef	CODEEUC
			if (isekana(s, i)) {
				cp[j++] = s[i++];
				cp[j++] = s[i];
			}
			else
#endif
			if (iskanji1(s, i)) {
				cp[j++] = s[i++];
				cp[j++] = s[i];
			}
#if	!MSDOS
			else if (!pathignorecase) cp[j++] = s[i];
#endif
			else cp[j++] = toupper2(s[i]);
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

static int NEAR _regexp_exec(re, s)
reg_t *re;
char *s;
{
	int i, n1, n2, c1, c2, beg, rev;

	for (n1 = n2 = 0; re[n1] && s[n2]; n1++, n2++) {
		c1 = (u_char)(s[i = n2]);
#if	!MSDOS
		if (pathignorecase)
#endif
		c1 = toupper2(c1);
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
	/* bash treats a character quoted by \ in "[]" as a character itself */
			if (re[n1][i] == PMETA && re[n1][i + 1]) i++;
			else if (re[n1][i] == '-' && re[n1][i + 1] && c2)
#else
			if (re[n1][i] == '-' && c2)
#endif
			{
				beg = c2;
				i++;
			}
			c2 = (u_char)(re[n1][i]);
#ifdef	CODEEUC
			if (isekana(re[n1], i))
				c2 = (c2 << 8) + (u_char)(re[n1][++i]);
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

static char *NEAR catpath(path, file, plenp, flen, isoverwrite)
char *path, *file;
int *plenp, flen, isoverwrite;
{
	char *new;
	int i, sc;

#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
	if (*plenp >= 2 && _dospath(path)) i = 2;
	else
#endif
	i = 0;

	if (*plenp <= i) sc = 0;
	else {
		sc = 1;
		if (path[i] == _SC_) {
			for (i++; i < *plenp; i++) if (path[i] != _SC_) break;
			if (i >= *plenp) sc = 0;
		}
	}

	if (isoverwrite) new = realloc2(path, *plenp + sc + flen + 1);
	else {
		new = malloc2(*plenp + sc + flen + 1);
		strncpy(new, path, *plenp);
	}
	if (sc) new[(*plenp)++] = _SC_;
	strncpy2(new + *plenp, file, flen);
	*plenp += flen;
	return(new);
}

#if	MSDOS
static int NEAR _evalwild(argc, argvp, s, fixed, len)
int argc;
char ***argvp, *s, *fixed;
int len;
#else
static int NEAR _evalwild(argc, argvp, s, fixed, len, nino, ino)
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

#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
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
				if (*(dp -> d_name) == '.') {
					free(cp);
#if	!MSDOS
					free(dupino);
#endif
					continue;
				}
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
	return(strpathcmp2(*((char **)vp1), *((char **)vp2)));
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
		strcpy(path + len, "COM");
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT);
		strcpy(path + len, "EXE");
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT | CM_EXE);
		strcpy(path + len, "BAT");
		if (isexecute(path, 0, exe) >= 0) return(CM_ADDEXT | CM_BATCH);
	}
	return(-1);
}
#endif

int searchhash(hpp, com, search)
hashlist **hpp;
char *com, *search;
{
	char *cp, *tmp, *next, *path;
	int len, dlen, cost, size, ret, recalc;
#if	MSDOS
	char *ext;
#endif
#ifndef	_NOUSEHASH
	static char *searchcwd = NULL;
	char buf[MAXPATHLEN];
	int n;

	if (!com && !search) {
		freehash(hpp);
		if (searchcwd) free(searchcwd);
		searchcwd = NULL;
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
		path = (*hpp) -> path;
		if (((*hpp) -> type & CM_RECALC)
		&& (!searchcwd || strcmp(searchcwd, Xgetwd(buf))));
		else if (isexecute(path, 0, 1) >= 0) return((*hpp) -> type);
		rmhash(com, n);
	}
#endif

#if	MSDOS && !defined (DISMISS_CURPATH)
	len = strlen(com);
	path = malloc2(len + 2 + EXTWIDTH + 1);
	path[0] = '.';
	path[1] = _SC_;
	strcpy(path + 2, com);
	if ((ret = extaccess(path, ext, len + 2, 1)) < 0) free(path);
	else {
# ifdef	_NOUSEHASH
		*hpp = (hashlist *)path;
# else
		*hpp = newhash(com, path, 0, hashtable[n]);
		hashtable[n] = *hpp;
		(*hpp) -> type = (ret |= (CM_HASH | CM_RECALC));
		if (searchcwd) free(searchcwd);
		searchcwd = strdup2(Xgetwd(buf));
# endif
		return(ret);
	}
#endif	/* MSDOS && !DISMISS_CURPATH */

	recalc = 0;
	if ((next = (search) ? search : getvar("PATH", -1))) {
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
				tmp = _evalpath(cp, cp + dlen, 1, 1);
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
			strncpy2(path + dlen, com, len);
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
			if (searchcwd) free(searchcwd);
			searchcwd = strdup2(Xgetwd(buf));
#endif
			return(ret);
		}
		if (path) free(path);
	}

	return(CM_NOTFOUND);
}

char *searchpath(path, search)
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

#ifndef	_NOCOMPLETE
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
static int NEAR completeuser(name, len, argc, argvp)
char *name;
int len, argc;
char ***argvp;
{
	struct passwd *pwd;
	char *new;
	int size;

	len = strlen(name);
# ifdef	DEBUG
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
		strcatdelim2(new + 1, pwd -> pw_name, NULL);
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
# else	/* !DEBUG */
	setpwent();
	while ((pwd = getpwent())) {
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
# endif	/* !DEBUG */
	return(argc);
}
#endif	/* !MSDOS */

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

#if	MSDOS && !defined (DISMISS_CURPATH)
	argc = completefile(file, len, argc, argvp, ".", 1, 1);
#endif
	if (!(next = getvar("PATH", -1))) return(argc);
	for (cp = next; cp; cp = next) {
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
		if (_dospath(cp)) next = strchr(cp + 2, PATHDELIM);
		else
#endif
		next = strchr(cp, PATHDELIM);
		dlen = (next) ? (next++) - cp : strlen(cp);
		tmp = _evalpath(cp, cp + dlen, 1, 1);
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
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
	if (_dospath(path)) {
		dir += 2;
		dlen = 2;
	}
#endif

	if ((file = strrdelim(dir, 0))) {
		dlen += (file == dir) ? 1 : file - dir;
		return(completefile(file + 1, strlen(file + 1), argc, argvp,
			path, dlen, exe));
	}
#if	!MSDOS
	else if (*path == '~')
		return(completeuser(path + 1, len - 1, argc, argvp));
#endif
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
#endif	/* !_NOCOMPLETE */

static int NEAR addmeta(s1, s2, stripm, quoted)
char *s1, *s2;
int stripm, quoted;
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
		len = strcpy2(cp + len, argv[i]) - cp;
	}
	return(cp);
}

static char *NEAR evalshellparam(c, quoted)
int c, quoted;
{
	char *cp, **arglist, tmp[MAXLONGWIDTH + 1];
	long pid;
	int i, j, sp;

	cp = NULL;
	switch (c) {
		case '@':
			if (!getarglistfunc
			|| !(arglist = (*getarglistfunc)())) break;
			sp = ' ';
			if (!quoted) {
				cp = catvar(&(arglist[1]), sp);
				break;
			}
#ifdef	BASHSTYLE
	/* bash uses IFS instead of a space as a separator */
			if ((cp = getvar("IFS", -1)) && *cp) sp = *cp;
#endif

			for (i = j = 0; arglist[i + 1]; i++)
				j += addmeta(NULL,
					arglist[i + 1], 0, quoted);
			if (i <= 0) cp = strdup2("");
			else {
				j += (i - 1) * 3;
				cp = malloc2(j + 1);
				j = addmeta(cp, arglist[1], 0, quoted);
				for (i = 2; arglist[i]; i++) {
					cp[j++] = quoted;
					cp[j++] = sp;
					cp[j++] = quoted;
					j += addmeta(cp + j,
						arglist[i], 0, quoted);
				}
				cp[j] = '\0';
			}
			break;
		case '*':
			if (!getarglistfunc
			|| !(arglist = (*getarglistfunc)())) break;
			sp = ' ';
#ifdef	BASHSTYLE
	/* bash uses IFS instead of a space as a separator */
			if (quoted && (cp = getvar("IFS", -1)))
				sp = (*cp) ? *cp : '\0';
#endif
			cp = catvar(&(arglist[1]), sp);
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
			if (!getpidfunc) pid = getpid();
			else pid = (*getpidfunc)();
			sprintf(tmp, "%ld", pid);
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

static int NEAR replacevar(arg, cpp, s, len, vlen, mode)
char *arg, **cpp;
int s, len, vlen, mode;
{
	char *val;
	int i;

	if (!mode) return(0);
	if (mode == '+') {
		if (!*cpp) return(0);
	}
	else if (*cpp) return(0);
	else if (mode == '=' && *arg != '_' && !isalpha(*arg))
		return(-1);

	val = malloc2(vlen + 1);
	strncpy2(val, arg + s, vlen);
	*cpp = evalarg(val, (mode == '=' || mode == '?'), 0);
	free(val);
	if (!(*cpp)) return(-1);

	if (mode == '=') {
		if (setvar(arg, *cpp, len) < 0) {
			free(*cpp);
			*cpp = NULL;
			return(-1);
		}
#ifdef	BASHSTYLE
	/* bash does not evaluates a quoted string in substitution itself */
		free(*cpp);
		val = malloc2(vlen + 1);
		strncpy2(val, arg + s, vlen);
		*cpp = evalarg(val, 0, 0);
		free(val);
		if (!(*cpp)) return(-1);
#endif
	}
	else if (mode == '?') {
		for (i = 0; i < len; i++) fputc(arg[i], stderr);
		fputs(": ", stderr);
		if (vlen <= 0)
			fputs("parameter null or not set", stderr);
		else for (i = 0; (*cpp)[i]; i++) fputc((*cpp)[i], stderr);
		free(*cpp);
		*cpp = NULL;
		fputc('\n', stderr);
		fflush(stderr);
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
	char *cp, *new, *arg, *top, **arglist;
	int i, c, n, len, vlen, s, e, nul, mode, nest, quote;

	nul = 0;
	mode = '\0';
	new = NULL;
	arg = *argp;
	top = *argp + 1;

	if (*top == '_' || isalpha(*top)) {
		for (i = 1; top[i]; i++)
			if (top[i] != '_' && !isalnum(top[i])) break;
		len = s = e = i;
		*argp += i;
	}
	else if (*top != '{') {
		if (*top == quoted) {
			(*bufp)[ptr++] = '$';
			return(ptr);
		}
		len = s = e = 1;
		(*argp)++;
	}
	else {
		nest = 1;
		len = s = -1;
		top++;
		quote = '\0';
		for (i = 0; nest > 0; i++) {
			if (!top[i]) return(-1);
			else if (top[i] == quote) quote = '\0';
#ifdef	CODEEUC
			else if (isekana(top, i)) {
				if (top[i + 1]) i++;
			}
#endif
			else if (iskanji1(top, i)) {
				if (top[i + 1]) i++;
			}
			else if (quote == '\'');
			else if (ismeta(top, i, quote)) {
				if (len < 0) return(-1);
				i++;
			}
			else if (quote);
			else if (top[i] == '\'' || top[i] == '"') {
				if (len < 0) return(-1);
				quote = top[i];
			}
			else switch (top[i]) {
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
					len = i;
					s = i + 1;
					nul = 1;
					break;
				case '-':
				case '=':
				case '?':
				case '+':
					if (len < 0) len = i;
					else if (!nul || i > len + 1) break;
					s = i + 1;
					mode = top[i];
					break;
				default:
					if (len >= 0);
					else if (i > 0 && top[i] != '_'
					&& !isalnum(top[i]))
						return(-1);
					break;
			}
		}
		e = i - 1;
		if (len < 0) len = e;
		if (s < 0) s = e;
		*argp += i + 1;
	}

	vlen = e - s;
	if (len <= 0 || (!mode && vlen > 0)) return(-1);

	cp = NULL;
	c = '\0';
	if (*top == '_' || isalpha(*top)) cp = getvar(top, len);
	else if (isdigit(*top)) {
		if (len > 1) return(-1);
		if (!getarglistfunc || !(arglist = (*getarglistfunc)())) {
			(*bufp)[ptr++] = '$';
			(*bufp)[ptr++] = *top;
			return(ptr);
		}
		else {
			i = *top - '0';
			for (n = 0; arglist[n]; n++);
			if (i < n) cp = arglist[i];
		}
	}
	else if (len == 1) {
		if ((cp = evalshellparam(c = *top, quoted))) new = cp;
		else {
			(*bufp)[ptr++] = '$';
			(*bufp)[ptr++] = *top;
			return(ptr);
		}
	}
	else return(-1);

	if (!mode && checkundeffunc && (*checkundeffunc)(cp, top, len) < 0)
		return(-1);

	if (cp && nul && !*cp) cp = NULL;

	if ((mode = replacevar(top, &cp, s, len, vlen, mode)) < 0) {
		if (new) free(new);
		return(mode);
	}
	else if (mode) {
		if (new) free(new);
		new = cp;
	}

	if (!mode && (c != '@' || !quoted)) {
		vlen = addmeta(NULL, cp, 0, quoted);
		*bufp = insertarg(*bufp, ptr, arg, *argp - arg + 1, vlen);
		addmeta(*bufp + ptr, cp, 0, quoted);
	}
	else if (cp) {
		vlen = strlen(cp);
		*bufp = insertarg(*bufp, ptr, arg, *argp - arg + 1, vlen);
		strncpy(*bufp + ptr, cp, vlen);
	}

	ptr += vlen;
	if (new) free(new);
	return(ptr);
}

#if	!MSDOS
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
	return(&(gidlist[maxgid++]));
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
		for (i = 0; i < maxgid; i++) free(gidlist[i].name);
		free(gidlist);
	}
	uidlist = NULL;
	gidlist = NULL;
	maxuid = maxgid = 0;
}
# endif
#endif	/* !MSDOS */

char *gethomedir(VOID_A)
{
#if	!MSDOS
	uidtable *up;
#endif
	char *cp;

	if ((cp = getvar("HOME", -1))) return(cp);
#if	!MSDOS
	if ((up = finduid(getuid(), NULL))) return(up -> home);
#endif
	return(NULL);
}

static int NEAR evalhome(bufp, ptr, argp)
char **bufp;
int ptr;
char **argp;
{
	char *cp, *top;
	int len, vlen;

	top = *argp + 1;

	len = ((cp = strdelim(top, 0))) ? (cp - top) : strlen(top);
	if (!len) cp = gethomedir();
#ifdef	FD
	else if (len == sizeof("FD") - 1 && !strnpathcmp(top, "FD", len))
		cp = progpath;
#endif
	else {
#if	!MSDOS
		uidtable *up;

		cp = malloc2(len + 1);
		strncpy2(cp, top, len);
		up = finduid(0, cp);
		free(cp);
		if (up) cp = up -> home;
		else
#endif
		cp = NULL;
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
	strncpy(*bufp + ptr, cp, vlen);
	ptr += vlen;
	*argp += len;
	return(ptr);
}

char *evalarg(arg, stripq, backq)
char *arg;
int stripq, backq;
{
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
	char path[MAXPATHLEN], alias[MAXPATHLEN];
#endif
	char *cp, *buf, *bbuf;
	int i, j, prev, quote;

#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
	if (*arg == '"' && (cp = strchr(arg + 1, '"')) && !*(cp + 1)) {
		strncpy2(path, arg + 1, cp - arg - 1);
		if (shortname(path, alias) && strpathcmp(path, alias)) {
			if (stripq) return(strdup2(alias));
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
	bbuf = (backq) ? malloc2(i) : NULL;
	prev = quote = '\0';
	i = 0;
	j = -1;
	for (cp = arg; *cp; prev = *(cp++)) {
		if (*cp == quote) {
			quote = '\0';
			if (j >= 0) bbuf[j++] = *cp;
			else if (!stripq) buf[i++] = *cp;
			continue;
		}
#ifdef	CODEEUC
		else if (isekana(cp, 0)) {
			if (j >= 0) bbuf[j++] = *cp;
			else buf[i++] = *cp;
			cp++;
		}
#endif
		else if (iskanji1(cp, 0)) {
			if (j >= 0) bbuf[j++] = *cp;
			buf[i++] = *cp;
			cp++;
		}
		else if (quote == '\'');
		else if (*cp == '$' && *(cp + 1) && j < 0) {
#ifdef	FAKEMETA
			if (*(cp + 1) == '$' || *(cp + 1) == '~') cp++;
			else
#endif
			if ((i = evalvar(&buf, i, &cp, quote)) >= 0) continue;
			else {
				if (bbuf) free(bbuf);
				free(buf);
				if (i < -1) *arg = '\0';
				return(NULL);
			}
		}
		else if (ismeta(cp, 0, quote)) {
			cp++;
			if (*cp == '$');
			else if (backq && *cp == '`');
			else if (stripq && (*cp == '\'' || *cp == '"'));
			else if (j >= 0) bbuf[j++] = PMETA;
			else buf[i++] = PMETA;
		}
		else if (backq && backquotefunc && *cp == '`') {
			if (j < 0) j = 0;
			else if (j) {
				char *tmp;
				int size;

				bbuf[j] = '\0';
				tmp = (*backquotefunc)(bbuf);
				if (tmp) {
					j = addmeta(NULL, tmp, 1, 0);
					size = i + j + strlen(cp + 1) + 1;
					buf = realloc2(buf, size);
					addmeta(&(buf[i]), tmp, 1, 0);
					i += j;
					free(tmp);
				}
				j = -1;
			}
			continue;
		}
		else if (quote);
		else if (*cp == '\'' || *cp == '"') {
			quote = *cp;
			if (j >= 0) bbuf[j++] = *cp;
			else if (!stripq) buf[i++] = *cp;
			continue;
		}
		else if (*cp == '~' && j < 0
		&& (!prev || prev == ':' || prev == '=')) {
			i = evalhome(&buf, i, &cp);
			continue;
		}

		if (j >= 0) bbuf[j++] = *cp;
		else buf[i++] = *cp;
	}
	if (bbuf) free(bbuf);
	buf[i] = '\0';
	return(buf);
}

int evalifs(argc, argvp, ifs)
int argc;
char ***argvp, *ifs;
{
	char *cp;
	int i, j, n, quote;

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
					if (!strchr(ifs, (*argvp)[n][j]))
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

int evalglob(argc, argvp, stripq)
int argc;
char ***argvp;
int stripq;
{
	char *cp, **wild;
	int i, j, n, w, size, quote;

	for (n = 0; n < argc; n++) {
		cp = c_malloc(size);
		for (i = j = w = 0, quote = '\0'; (*argvp)[n][i]; i++) {
			cp = c_realloc(cp, j + 1, size);
			if ((*argvp)[n][i] == quote) {
				quote = '\0';
				if (stripq) continue;
			}
#ifdef	CODEEUC
			else if (isekana((*argvp)[n], i))
				cp[j++] = (*argvp)[n][i++];
#endif
			else if (iskanji1((*argvp)[n], i))
				cp[j++] = (*argvp)[n][i++];
			else if (quote == '\'');
			else if (ismeta((*argvp)[n], i, quote)) {
				i++;
				if (quote
				&& !strchr(DQ_METACHAR, (*argvp)[n][i]))
					cp[j++] = PMETA;
			}
			else if (quote);
			else if ((*argvp)[n][i] == '\''
			|| (*argvp)[n][i] == '"') {
				quote = (*argvp)[n][i];
				if (stripq) continue;
			}
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
				n += w - 1;
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

int stripquote(arg, stripq)
char *arg;
int stripq;
{
	int i, j, quote, stripped;

	stripped = 0;
	for (i = j = 0, quote = '\0'; arg[i]; i++) {
		if (arg[i] == quote) {
			quote = '\0';
			stripped = 1;
			if (stripq) continue;
		}
#ifdef	CODEEUC
		else if (isekana(arg, i)) arg[j++] = arg[i++];
#endif
		else if (iskanji1(arg, i)) arg[j++] = arg[i++];
		else if (quote == '\'');
		else if (ismeta(arg, i, quote)) {
			i++;
			stripped = 1;
			if (quote && arg[i] != quote && arg[i] != PMETA)
				arg[j++] = PMETA;
		}
		else if (quote);
		else if (arg[i] == '\'' || arg[i] == '"') {
			quote = arg[i];
			stripped = 1;
			if (stripq) continue;
		}

		arg[j++] = arg[i];
	}
	arg[j] = '\0';
	return(stripped);
}

char *_evalpath(path, eol, uniqdelim, evalq)
char *path, *eol;
int uniqdelim, evalq;
{
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
	char alias[MAXPATHLEN];
	int top = -1;
#endif
	char *cp, *tmp;
	int i, j, c, size, quote;

	if (eol) i = eol - path;
	else i = strlen(path);
	cp = malloc2(i + 1);
	strncpy2(cp, path, i);

	if (!(tmp = evalarg(cp, 0, 0))) {
		*cp = '\0';
		return(cp);
	}
	free(cp);
	cp = tmp;

	size = strlen(cp) + 1;
	tmp = malloc2(size);
	quote = '\0';
	for (i = j = c = 0; cp[i]; c = cp[i++]) {
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
					tmp = realloc2(tmp, size);
					strncpy(tmp + top, alias, len);
				}
			}
			top = -1;
#endif
			quote = '\0';
			if (evalq) continue;
		}
#ifdef	CODEEUC
		else if (isekana(cp, i)) tmp[j++] = cp[i++];
#endif
		else if (iskanji1(cp, i)) tmp[j++] = cp[i++];
		else if (quote == '\'');
		else if (ismeta(cp, i, quote)) {
			i++;
			if (!evalq
			|| (quote && !strchr(DQ_METACHAR, cp[i])))
				tmp[j++] = PMETA;
		}
		else if (quote);
		else if (cp[i] == '\'' || cp[i] == '"') {
#if	defined (FD) && MSDOS && !defined (_NOUSELFN)
			if (cp[i] == '"') top = j;
#endif
			quote = cp[i];
			if (evalq) continue;
		}
		else if (uniqdelim && cp[i] == _SC_ && c == _SC_) continue;
		tmp[j++] = cp[i];
	}
	tmp[j] = '\0';
	free(cp);
	return(tmp);
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
