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

extern char *_evalpath();
extern char *evalpath();
extern char *cnvregexp();
extern reg_t *regexp_init();
extern int regexp_exec();
extern int regexp_free();
#if	!MSDOS || !defined (_NOCOMPLETE)
extern char *lastpointer();
extern char *finddupl();
extern int completepath();
#endif
#ifndef	_NOCOMPLETE
extern char *findcommon();
#endif
