/*
 *	string.h
 *
 *	function prototype declarations for "string.c"
 */

extern char *Xstrchr __P_((CONST char *, int));
extern char *Xstrrchr __P_((CONST char *, int));
extern char *Xmemchr __P_((CONST char *, int, int));
extern char *Xstrcpy __P_((char *, CONST char *));
extern char *Xstrncpy __P_((char *, CONST char *, int));
extern int Xstrcasecmp __P_((CONST char *, CONST char *));
extern int Xstrncasecmp __P_((CONST char *, CONST char *, int));
#ifndef	_NOVERSCMP
extern int Xstrverscmp __P_((CONST char *, CONST char *, int));
#endif
#ifdef	CODEEUC
extern int strlen2 __P_((CONST char *));
#else
#define	strlen2			strlen
#endif
extern VOID Xstrtolower __P_((char *));
extern VOID Xstrtoupper __P_((char *));
extern VOID Xstrntolower __P_((char *, int));
extern VOID Xstrntoupper __P_((char *, int));
