/*
 *	pathname.h
 *
 *	Function Prototype Declaration for "pathname.c"
 */

#ifndef	__PATHNAME_H_
#define	__PATHNAME_H_

#ifdef	USEREGCOMP
#include <regex.h>
typedef regex_t reg_t;
#else
typedef char reg_t;
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

#endif	/* __PATHNAME_H_ */
