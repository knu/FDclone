/*
 *	pathname.h
 *
 *	Function Prototype Declaration for "pathname.c"
 */

#ifndef	__PATHNAME_H_
#define	__PATHNAME_H_

#ifndef	__SYS_TYPES_STAT_H_
#define	__SYS_TYPES_STAT_H_
#include <sys/types.h>
#include <sys/stat.h>
#endif

#define	IFS_SET		" \t\n"
#define	META		'\\'
#if	MSDOS && defined (_NOORIGSHELL)
#define	PATHDELIM	';'
#else
#define	PATHDELIM	':'
#endif

#ifndef	BSPATHDELIM
#define	PMETA		META
#define	RMSUFFIX	'%'
#define	METACHAR	"\t\n !\"#$&'()*;<=>?[\\]`|"
#define	DQ_METACHAR	"\"$\\`"
#else	/* BSPATHDELIM */
# ifdef	_NOORIGSHELL
# define	FAKEMETA
# define	PMETA		'$'
# define	RMSUFFIX	'%'
# define	METACHAR	"\t\n !\"$'*<>?|"
# define	DQ_METACHAR	"\"$"
# else
# define	PMETA		'%'
# define	RMSUFFIX	'\\'
# define	METACHAR	"\t\n !\"#$%&'()*;<=>?[]`|"
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
#define	PC_META		7

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
	char *s;
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

#ifdef	NOUID_T
typedef u_short	uid_t;
typedef u_short	gid_t;
#endif

#ifdef	USEPID_T
typedef pid_t		p_id_t;
# else
typedef long		p_id_t;
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
	char **gr_mem;
	char ismem;
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
extern int strcasecmp2 __P_((char *, char *));
extern int strncasecmp2 __P_((char *, char *, int));
#ifdef	PATHNOCASE
#define	strpathcmp	strcasecmp2
#define	strnpathcmp	strncasecmp2
#define	strpathcmp2	strcasecmp2
#define	strnpathcmp2	strncasecmp2
#else
#define	strpathcmp	strcmp
#define	strnpathcmp	strncmp
extern int strpathcmp2 __P_((char *, char *));
extern int strnpathcmp2 __P_((char *, char *, int));
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
extern char *underpath(char *, char *, int);
extern int isidentchar __P_((int));
extern int isidentchar2 __P_((int));
extern int isdotdir __P_((char *));
extern char *isrootdir __P_((char *));
extern char *getbasename __P_((char *));
extern char *getshellname __P_((char *, int *, int *));
extern reg_t *regexp_init __P_((char *, int));
extern int regexp_exec __P_((reg_t *, char *, int));
extern VOID regexp_free __P_((reg_t *));
extern int cmppath __P_((CONST VOID_P, CONST VOID_P));
extern char **evalwild __P_((char *, int));
#ifndef	_NOUSEHASH
hashlist **duplhash __P_((hashlist **));
#endif
extern int searchhash __P_((hashlist **, char *, char *));
extern char *searchexecpath __P_((char *, char *));
#if	!defined (FDSH) && !defined (_NOCOMPLETE)
extern char *finddupl __P_((char *, int, char **));
extern int completepath __P_((char *, int, int, char ***, int));
extern char *findcommon __P_((int, char **));
#endif
extern char *catvar __P_((char *[], int));
extern int countvar __P_((char **));
extern VOID freevar __P_((char **));
extern char **duplvar __P_((char **, int));
extern int parsechar __P_((char *, int, int, int, int *, int *));
#if	defined (FD) && !defined (NOUID)
extern uidtable *finduid __P_((uid_t, char *));
extern gidtable *findgid __P_((gid_t, char *));
extern int isgroupmember __P_((gid_t));
# ifdef	DEBUG
extern VOID freeidlist __P_((VOID_A));
# endif
#endif	/* FD && !NOUID */
extern char *gethomedir __P_((VOID_A));
#ifndef	MINIMUMSHELL
extern int evalhome __P_((char **, int, char **));
#endif
extern char *evalarg __P_((char *, int, int));
extern int evalifs __P_((int, char ***, char *));
extern int evalglob __P_((int, char ***, int));
extern int stripquote __P_((char *, int));
extern char *_evalpath __P_((char *, char *, int));
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
#ifndef	PATHNOCASE
extern int pathignorecase;
#endif

#endif	/* __PATHNAME_H_ */
