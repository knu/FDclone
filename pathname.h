/*
 *	pathname.h
 *
 *	Function Prototype Declaration for "pathname.c"
 */

#ifndef	__PATHNAME_H_
#define	__PATHNAME_H_

#define	IFS_SET		" \t\n"
#define	META		'\\'
#if	!MSDOS
#define	PATHDELIM	':'
#define	PMETA		META
#define	METACHAR	"\t\n !\"#$&'()*;<=>?[\\]`|"
#define	DQ_METACHAR	"\"$\\`"
#else	/* MSDOS */
# ifdef	_NOORIGSHELL
# define	FAKEMETA
# define	PMETA		'$'
# define	METACHAR	"\t\n !\"$'*<>?|"
# define	DQ_METACHAR	"\"$"
# define	PATHDELIM	';'
# else
# define	PMETA		'%'
# define	METACHAR	"\t\n !\"#$%&'()*;<=>?[]`|"
# define	DQ_METACHAR	"\"$%`"
# define	PATHDELIM	':'
# endif
#endif	/* MSDOS */

#define	PC_NORMAL	0
#define	PC_OPQUOTE	1
#define	PC_CLQUOTE	2
#define	PC_SQUOTE	3
#define	PC_DQUOTE	4
#define	PC_BQUOTE	5
#define	PC_WORD		6
#define	PC_META		7

#define	MAXLONGWIDTH	20		/* log10(2^64) = 19.266 */
#ifdef	LSI_C
#define	toupper2	toupper
#define	tolower2	tolower
#else
extern u_char uppercase[256];
extern u_char lowercase[256];
#define	toupper2(c)	(uppercase[(u_char)(c)])
#define	tolower2(c)	(lowercase[(u_char)(c)])
#endif

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
# ifdef	USEREGCMP
# undef	USEREGCMP
# endif
# ifdef	USEREGCOMP
# undef	USEREGCOMP
# endif
# ifdef	USERE_COMP
# undef	USERE_COMP
# endif
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

#if	!MSDOS
typedef struct _devino_t {
	dev_t dev;
	ino_t ino;
} devino_t;
#endif

#ifdef	NOUID_T
typedef u_short	uid_t;
typedef u_short	gid_t;
#endif

#ifdef	USEPID_T
typedef	pid_t		p_id_t;
# else
typedef	long		p_id_t;
#endif

#ifndef	VOID_P
#ifdef	NOVOID
#define	VOID
#define	VOID_T	int
#define	VOID_P	char *
#else
#define	VOID	void
#define	VOID_T	void
#define	VOID_P	void *
#endif
#endif

#if	defined (SIGARGINT) || defined (NOVOID)
#define	sigarg_t	int
#else
#define	sigarg_t	void
#endif

#ifdef	SIGFNCINT
#define	sigfnc_t	int
#else
# ifdef	NOVOID
# define	sigfnc_t
# else
# define	sigfnc_t	void
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
} gidtable;

#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
extern char *strdelim __P_((char *, int));
extern char *strrdelim __P_((char *, int));
#else
#define	strdelim(s, d)	strchr(s, _SC_)
#define	strrdelim(s, d)	strrchr(s, _SC_)
#endif
extern char *strrdelim2 __P_((char *, char *));
extern int isdelim __P_((char *, int));
extern char *strcatdelim __P_((char *));
extern char *strcatdelim2 __P_((char *, char *, char *));
#if	MSDOS
#define	strnpathcmp	strnpathcmp2
#define	strpathcmp	strpathcmp2
#else
#define	strnpathcmp	strncmp
#define	strpathcmp	strcmp
#endif
extern int strcasecmp2 __P_((char *, char *));
extern int strpathcmp2 __P_((char *, char *));
extern int strnpathcmp2 __P_((char *, char *, int));
extern char *strdupcpy __P_((char *, int));
extern int isidentchar __P_((int));
extern int isdotdir __P_((char *));
extern char *isrootdir __P_((char *));
extern reg_t *regexp_init __P_((char *, int));
extern int regexp_exec __P_((reg_t *, char *, int));
extern VOID regexp_free __P_((reg_t *));
extern int cmppath __P_((CONST VOID_P, CONST VOID_P));
extern char **evalwild __P_((char *));
#ifndef	_NOUSEHASH
hashlist **duplhash __P_((hashlist **));
#endif
extern int searchhash __P_((hashlist **, char *, char *));
extern char *searchpath __P_((char *, char *));
#if	!defined (FDSH) && !defined (_NOCOMPLETE)
extern char *finddupl __P_((char *, int, char **));
extern int completepath __P_((char *, int, int, char ***, int));
extern char *findcommon __P_((int, char **));
#endif
extern char *catvar __P_((char *[], int));
extern int countvar __P_((char **));
extern VOID freevar __P_((char **));
extern int parsechar __P_((char *, int, int, int, int *, int *));
#if	!MSDOS
extern uidtable *finduid __P_((uid_t, char *));
extern gidtable *findgid __P_((gid_t, char *));
# ifdef	DEBUG
extern VOID freeidlist __P_((VOID_A));
# endif
#endif
extern char *gethomedir __P_((VOID_A));
extern int evalhome __P_((char **, int, char **));
extern char *evalarg __P_((char *, int, int, int));
extern int evalifs __P_((int, char ***, char *));
extern int evalglob __P_((int, char ***, int));
extern int stripquote __P_((char *, int));
extern char *_evalpath __P_((char *, char *, int, int));
extern char *evalpath __P_((char *, int));

extern char **argvar;
#ifndef	_NOUSEHASH
extern hashlist **hashtable;
#endif
extern char *(*getvarfunc)__P_((char *, int));
extern int (*putvarfunc)__P_((char *, int));
extern int (*getretvalfunc)__P_((VOID_A));
extern p_id_t (*getpidfunc)__P_((VOID_A));
extern p_id_t (*getlastpidfunc)__P_((VOID_A));
extern char *(*getflagfunc)__P_((VOID_A));
extern int (*checkundeffunc)__P_((char *, char *, int));
extern VOID (*exitfunc)__P_((VOID_A));
extern char *(*backquotefunc)__P_((char *));
#ifndef	MINIMUMSHELL
extern char *(*posixsubstfunc)__P_((char *, int *));
#endif
#if	!MSDOS
extern int pathignorecase;
#endif

#endif	/* __PATHNAME_H_ */
