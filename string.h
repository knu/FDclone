/*
 *	string.h
 *
 *	function prototype declarations for "string.c"
 */

extern char *Xstrchr __P_((CONST char *, int));
extern char *Xstrrchr __P_((CONST char *, int));
#ifndef	MINIMUMSHELL
extern char *Xmemchr __P_((CONST char *, int, int));
#endif
extern char *Xstrcpy __P_((char *, CONST char *));
extern char *Xstrncpy __P_((char *, CONST char *, int));
extern int Xstrcasecmp __P_((CONST char *, CONST char *));
extern int Xstrncasecmp __P_((CONST char *, CONST char *, int));
#ifdef	CODEEUC
extern int strlen2 __P_((CONST char *));
#else
#define	strlen2			strlen
#endif
