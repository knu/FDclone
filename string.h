/*
 *	string.h
 *
 *	function prototype declarations for "string.c"
 */

extern char *strchr2 __P_((CONST char *, int));
extern char *strrchr2 __P_((CONST char *, int));
#ifndef	MINIMUMSHELL
extern char *memchr2 __P_((CONST char *, int, int));
#endif
extern char *strcpy2 __P_((char *, CONST char *));
extern char *strncpy2 __P_((char *, CONST char *, int));
extern int strcasecmp2 __P_((CONST char *, CONST char *));
extern int strncasecmp2 __P_((CONST char *, CONST char *, int));
#ifdef	CODEEUC
extern int strlen2 __P_((CONST char *));
#else
#define	strlen2			strlen
#endif
