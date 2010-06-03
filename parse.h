/*
 *	parse.h
 *
 *	definitions & function prototype declarations for "parse.c"
 */

#include "depend.h"

#if	defined (FD) && (FD < 2) && !defined (OLDPARSE)
#define	OLDPARSE
#endif

extern char *skipspace __P_((CONST char *));
extern char *Xsscanf __P_((CONST char *, CONST char *, ...));
extern int Xatoi __P_((CONST char *));
#if	defined (FD) && !defined (DEP_ORIGSHELL)
extern char *strtkchr __P_((CONST char *, int, int));
extern int getargs __P_((CONST char *, char ***));
extern char *gettoken __P_((CONST char *));
extern char *getenvval __P_((int *, char *CONST []));
extern char *evalcomstr __P_((CONST char *, CONST char *));
#endif
extern char *evalpaths __P_((CONST char *, int));
#if	MSDOS && defined (FD) && !defined (DEP_ORIGSHELL)
#define	killmeta(s)		Xstrdup(s)
#else
extern char *killmeta __P_((CONST char *));
#endif
#if	defined (FD) && !defined (DEP_ORIGSHELL)
extern VOID adjustpath __P_((VOID_A));
#endif
#ifdef	FD
extern char *includepath __P_((CONST char *, CONST char *));
#endif
#if	defined (OLDPARSE) && !defined (_NOARCHIVE)
extern char *getrange __P_((CONST char *, int, u_char *, u_char *, u_char *));
#endif
extern int getprintable __P_((char *, ALLOC_T, CONST char *, ALLOC_T, int *));
extern int evalprompt __P_((char **, CONST char *));
#if	defined (FD) && !defined (_NOARCHIVE)
extern char *getext __P_((CONST char *, u_char *));
extern int extcmp __P_((CONST char *, int, CONST char *, int, int));
#endif
#ifdef	FD
extern int getkeycode __P_((CONST char *, int));
#endif
extern CONST char *getkeysym __P_((int, int));
extern char *decodestr __P_((CONST char *, u_char *, int));
#if	defined (FD) && !defined (_NOKEYMAP)
extern char *encodestr __P_((CONST char *, int));
#endif
