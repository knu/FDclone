/*
 *	pathname.h
 *
 *	Function Prototype Declaration for "pathname.c"
 */

#ifdef	USEREGCOMP
#include <regex.h>
typedef regex_t reg_t;
#else
typedef char reg_t;
#endif

extern int isdelim __P_((char *, int));
#if	MSDOS
extern char *strdelim __P_((char *));
extern char *strrdelim __P_((char *));
#else
#define	strdelim(s)	strchr(s, _SC_)
#define	strrdelim(s)	strrchr(s, _SC_)
#endif
extern char *strrdelim2 __P_((char *, char *));
extern char *_evalpath __P_((char *, char *, int, int));
extern char *evalpath __P_((char *, int));
extern char *cnvregexp __P_((char *, int));
extern reg_t *regexp_init __P_((char *));
extern int regexp_exec __P_((reg_t *, char *));
extern int regexp_free __P_((reg_t *));
#if	!MSDOS || !defined (_NOCOMPLETE)
extern char *lastpointer __P_((char *, int));
extern char *finddupl __P_((char *, int, char *));
extern int completepath __P_((char *, int, char **, int, int));
#endif
#ifndef	_NOCOMPLETE
extern char *findcommon __P_((char *, int));
#endif
