/*
 *	pathname.h
 *
 *	definitions & function prototype declarations for "pathname.c"
 */

#ifndef	__PATHNAME_H_
#define	__PATHNAME_H_

#ifndef	__SYS_TYPES_STAT_H_
#define	__SYS_TYPES_STAT_H_
#include <sys/types.h>
#include <sys/stat.h>
#endif

/* #define BASHSTYLE		; rather near to bash style */
/* #define MINIMUMSHELL		; omit verbose extension from Bourne shell */
/* #define NESTINGQUOTE		; allow `...` included in "..." */

#ifndef	MINIMUMSHELL
#define	NESTINGQUOTE
#endif

#if	defined (MINIMUMSHELL) && defined (DOUBLESLASH)
#undef	DOUBLESLASH
#endif

#define	IFS_SET		" \t\n"
#define	META		'\\'
#if	MSDOS && defined (_NOORIGSHELL)
#define	PATHDELIM	';'
#else
#define	PATHDELIM	':'
#endif

#define	ENVPATH		"PATH"
#define	ENVHOME		"HOME"
#define	ENVPWD		"PWD"
#define	ENVIFS		"IFS"

#define	EXTCOM		"COM"
#define	EXTEXE		"EXE"
#define	EXTBAT		"BAT"

#ifndef	BSPATHDELIM
#define	PMETA		META
#define	RMSUFFIX	'%'
# ifdef	BASHSTYLE
	/* bash treats '\r' as just a character */
# define	METACHAR	"\t\n !\"#$&'()*;<=>?[\\]`|"
# else
# define	METACHAR	"\t\r\n !\"#$&'()*;<=>?[\\]`|"
# endif
#define	DQ_METACHAR	"\"$\\`"
#else	/* BSPATHDELIM */
# ifdef	_NOORIGSHELL
# define	FAKEMETA
# define	PMETA		'$'
# define	RMSUFFIX	'%'
# define	METACHAR	"\t\r\n !\"$'*<>?|"
# define	DQ_METACHAR	"\"$"
# else
# define	PMETA		'%'
# define	RMSUFFIX	'\\'
# define	METACHAR	"\t\r\n !\"#$%&'()*;<=>?[]`|"
# define	DQ_METACHAR	"\"$%`"
# endif
#endif	/* BSPATHDELIM */

#define	PC_NORMAL	0
#define	PC_OPQUOTE	1
#define	PC_CLQUOTE	2
#define	PC_SQUOTE	3
#define	PC_DQUOTE	4
#define	PC_BQUOTE	5
#define	PC_WORD		6
#define	PC_ESCAPE	7
#define	PC_META		8
#define	PC_EXMETA	9
#define	PC_DELIM	10

#if	defined (_NOORIGGLOB) \
&& !defined (USEREGCMP) && !defined (USEREGCOMP) && !defined (USERE_COMP)
#undef	_NOORIGGLOB
#endif

#ifdef	_NOORIGGLOB
# ifdef	USEREGCOMP
# include <regex.h>
typedef regex_t	reg_t;
# else
typedef char	reg_t;
# endif
#else	/* !_NOORIGGLOB */
typedef char *	reg_t;
# undef	USEREGCMP
# undef	USEREGCOMP
# undef	USERE_COMP
#endif	/* !_NOORIGGLOB */

#ifdef	_NOUSEHASH
typedef char	hashlist;
#else
typedef struct _hashlist {
	char *command;
	char *path;
	int hits;
	int cost;
	struct _hashlist *next;
	u_char type;
} hashlist;
#define	MAXHASH		64
#endif

#define	CM_NOTFOUND	0001
#define	CM_FULLPATH	0002
#define	CM_HASH		0004
#define	CM_BATCH	0010
#define	CM_EXE		0020
#define	CM_ADDEXT	0040
#define	CM_RECALC	0100
#define	CM_REHASH	0200

typedef struct _strbuf_t {
	char *s;
	ALLOC_T size;
	ALLOC_T len;
} strbuf_t;

#ifndef	NODIRLOOP
typedef struct _devino_t {
	dev_t dev;
	ino_t ino;
} devino_t;
#endif

typedef struct _wild_t {
	CONST char *s;
	strbuf_t fixed;
	strbuf_t path;
	int quote;
#ifndef	NODIRLOOP
	int nino;
	devino_t *ino;
#endif
	u_char flags;
} wild_t;

#define	EA_STRIPQ	0001
#define	EA_BACKQ	0002
#define	EA_KEEPMETA	0004
#define	EA_NOEVALQ	0010
#define	EA_STRIPQLATER	0020
#define	EA_NOUNIQDELIM	0040
#define	EA_EOLMETA	0100
#define	EA_FINDMETA	0200
#define	EA_FINDDELIM	0400

#ifdef	NOUID_T
typedef u_short		uid_t;
typedef u_short		gid_t;
#endif

#ifdef	USEPID_T
typedef pid_t		p_id_t;
#else
typedef long		p_id_t;
#endif

#ifdef	SIGARGINT
typedef int		sigarg_t;
#else
# ifdef	NOVOID
# define		sigarg_t
# else
typedef void		sigarg_t;
# endif
#endif

#ifdef	SIGFNCINT
typedef int		sigfnc_t;
#else
# ifdef	NOVOID
# define		sigfnc_t
# else
typedef void		sigfnc_t;
# endif
#endif

typedef sigarg_t (*sigcst_t)__P_((sigfnc_t));

typedef struct _uidtable {
	uid_t uid;
	char *name;
	char *home;
} uidtable;

typedef struct _gidtable {
	gid_t gid;
	char *name;
#ifndef	USEGETGROUPS
	char **gr_mem;
#endif
	char ismem;
} gidtable;

#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
extern char *gendospath __P_((char *, int, int));
extern char *strdelim __P_((CONST char *, int));
extern char *strrdelim __P_((CONST char *, int));
#else
#define	strdelim(s, d)	strchr(s, _SC_)
#define	strrdelim(s, d)	strrchr(s, _SC_)
#endif
#ifdef	FD
extern char *strrdelim2 __P_((CONST char *, CONST char *));
#endif
extern int isdelim __P_((CONST char *, int));
extern char *strcatdelim __P_((char *));
extern char *strcatdelim2 __P_((char *, CONST char *, CONST char *));
extern int strcasecmp2 __P_((CONST char *, CONST char *));
extern int strncasecmp2 __P_((CONST char *, CONST char *, int));
#ifdef	PATHNOCASE
#define	strpathcmp	strcasecmp2
#define	strnpathcmp	strncasecmp2
#define	strpathcmp2	strcasecmp2
#define	strnpathcmp2	strncasecmp2
#else
#define	strpathcmp	strcmp
#define	strnpathcmp	strncmp
extern int strpathcmp2 __P_((CONST char *, CONST char *));
extern int strnpathcmp2 __P_((CONST char *, CONST char *, int));
#endif
#ifdef	COMMNOCASE
#define	strcommcmp	strcasecmp2
#define	strncommcmp	strncasecmp2
#else
#define	strcommcmp	strcmp
#define	strncommcmp	strncmp
#endif
#ifdef	ENVNOCASE
#define	strenvcmp	strcasecmp2
#define	strnenvcmp	strncasecmp2
#else
#define	strenvcmp	strcmp
#define	strnenvcmp	strncmp
#endif
#ifdef	FD
extern char *underpath __P_((CONST char *, CONST char *, int));
#endif
extern int isidentchar __P_((int));
extern int isidentchar2 __P_((int));
extern int isdotdir __P_((CONST char *));
extern char *isrootdir __P_((CONST char *));
extern int isrootpath __P_((CONST char *));
extern VOID copyrootpath __P_((char *));
extern VOID copycurpath __P_((char *));
#ifdef	DOUBLESLASH
extern int isdslash __P_((CONST char *));
#endif
extern char *getbasename __P_((CONST char *));
extern char *getshellname __P_((CONST char *, int *, int *));
extern reg_t *regexp_init __P_((CONST char *, int));
extern int regexp_exec __P_((CONST reg_t *, CONST char *, int));
extern VOID regexp_free __P_((reg_t *));
extern int cmppath __P_((CONST VOID_P, CONST VOID_P));
extern char **evalwild __P_((CONST char *, int));
#ifndef	_NOUSEHASH
extern hashlist **duplhash __P_((hashlist **));
#endif
extern int searchhash __P_((hashlist **, CONST char *, CONST char *));
#ifdef	FD
extern char *searchexecpath __P_((CONST char *, CONST char *));
#endif
#if	!defined (FDSH) && !defined (_NOCOMPLETE)
extern char *finddupl __P_((CONST char *, int, char *CONST *));
# ifndef	NOUID
extern int completeuser __P_((CONST char *, int, int, char ***, int));
extern int completegroup __P_((CONST char *, int, int, char ***));
# endif
extern int completepath __P_((CONST char *, int, int, char ***, int));
extern char *findcommon __P_((int, char *CONST *));
#endif	/* !FDSH && !_NOCOMPLETE */
extern char *catvar __P_((char *CONST *, int));
extern int countvar __P_((char *CONST *));
extern VOID freevar __P_((char **));
extern char **duplvar __P_((char *CONST *, int));
extern int parsechar __P_((CONST char *, int, int, int, int *, int *));
#if	defined (FD) && !defined (NOUID)
extern uidtable *finduid __P_((uid_t, CONST char *));
extern gidtable *findgid __P_((gid_t, CONST char *));
extern int isgroupmember __P_((gid_t));
# ifdef	DEBUG
extern VOID freeidlist __P_((VOID_A));
# endif
#endif	/* FD && !NOUID */
extern char *gethomedir __P_((VOID_A));
extern CONST char *getrealpath __P_((CONST char *, char *, char *));
#ifndef	MINIMUMSHELL
extern int evalhome __P_((char **, int, CONST char **));
#endif
extern char *evalarg __P_((char *, int, int));
extern int evalifs __P_((int, char ***, CONST char *));
extern int evalglob __P_((int, char ***, int));
extern int stripquote __P_((char *, int));
extern char *_evalpath __P_((CONST char *, CONST char *, int));
extern char *evalpath __P_((char *, int));

extern CONST char nullstr[];
extern CONST char rootpath[];
extern CONST char curpath[];
extern CONST char parentpath[];
extern char **argvar;
#ifndef	_NOUSEHASH
extern hashlist **hashtable;
#endif
extern char *(*getvarfunc)__P_((CONST char *, int));
extern int (*putvarfunc)__P_((char *, int));
extern int (*getretvalfunc)__P_((VOID_A));
extern p_id_t (*getpidfunc)__P_((VOID_A));
extern p_id_t (*getlastpidfunc)__P_((VOID_A));
extern char *(*getflagfunc)__P_((VOID_A));
extern int (*checkundeffunc)__P_((CONST char *, CONST char *, int));
extern VOID (*exitfunc)__P_((VOID_A));
extern char *(*backquotefunc)__P_((CONST char *));
#ifndef	MINIMUMSHELL
extern char *(*posixsubstfunc)__P_((CONST char *, int *));
#endif
#ifndef	PATHNOCASE
extern int pathignorecase;
#endif

#endif	/* !__PATHNAME_H_ */
