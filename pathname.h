/*
 *	pathname.h
 *
 *	Function Prototype Declaration for "pathname.c"
 */

#ifndef	__PATHNAME_H_
#define	__PATHNAME_H_

#if	MSDOS
#define	PATHDELIM	';'
#else
#define	PATHDELIM	':'
#endif

#if	MSDOS && !defined (_NOORIGGLOB)
#define	META		'^'
#else
#define	META		'\\'
#endif

#define	ismeta(s, i, q)	((s)[i] == META && (s)[(i) + 1] \
			&& (!(q) || (s)[(i) + 1] != (q) || (s)[(i) + 2]))
#define	isnmeta(s, i, q, n) \
			((s)[i] == META && (i) + 1 < n \
			&& (!(q) || (s)[(i) + 1] != (q) || (i) + 2 < n))

#if	defined (_NOORIGGLOB) \
&& !defined (USEREGCMP) && !defined (USEREGCOMP) && !defined (USERE_COMP)
#undef	_NOORIGGLOB
#endif

#ifdef	_NOORIGGLOB
# ifdef	USEREGCOMP
#include <regex.h>
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

#if	!MSDOS
typedef struct _devino_t {
	dev_t	dev;
	ino_t	ino;
} devino_t;
#endif

extern int isdelim __P_((char *, int));
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
extern char *strdelim __P_((char *, int));
extern char *strrdelim __P_((char *, int));
#else
#define	strdelim(s, d)	strchr(s, _SC_)
#define	strrdelim(s, d)	strrchr(s, _SC_)
#endif
extern char *strrdelim2 __P_((char *, char *));
extern char *strcatdelim __P_((char *));
extern char *strcatdelim2 __P_((char *, char *, char *));
#ifdef	FD
extern char *_evalpath __P_((char *, char *, int, int));
extern char *evalpath __P_((char *, int));
#endif
#ifdef	_NOORIGGLOB
extern char *cnvregexp __P_((char *, int));
#endif
extern reg_t *regexp_init __P_((char *, int));
extern int regexp_exec __P_((reg_t *, char *, int));
extern VOID regexp_free __P_((reg_t *));
extern int cmppath __P_((CONST VOID_P, CONST VOID_P));
extern char **evalwild __P_((char *));
#ifndef	_NOUSEHASH
VOID freehash __P_((hashlist **));
hashlist **duplhash __P_((hashlist **));
#endif
extern int searchhash __P_((hashlist **, char *));
extern char *searchpath __P_((char *));
#ifndef	_NOCOMPLETE
extern char *finddupl __P_((char *, int, char **));
extern int completepath __P_((char *, int, int, char ***));
extern char *findcommon __P_((int, char **));
#endif
extern int addmeta __P_((char *, char *, int, int));
extern char *evalarg __P_((char *, int));
extern int evalifs __P_((int, char ***));
extern int evalglob __P_((int, char ***, int));
extern int stripquote __P_((char *));

extern int noglob;
extern hashlist **hashtable;
extern char *(*getvarfunc)__P_((char *, int));
extern int (*putvarfunc)__P_((char *, int));
extern int (*getretvalfunc)__P_((VOID_A));
extern long (*getlastpidfunc)__P_((VOID_A));
extern char **(*getarglistfunc)__P_((VOID_A));
extern char *(*getflagfunc)__P_((VOID_A));
extern int (*checkundeffunc)__P_((char *, char *, int));
extern VOID (*exitfunc)__P_((VOID_A));

#endif	/* __PATHNAME_H_ */
