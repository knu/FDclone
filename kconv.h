/*
 *	kconv.h
 *
 *	definitions & function prototype declarations for "kconv.c"
 */

#include "depend.h"

extern int onkanji1 __P_((CONST char *, int));
#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
extern int getlang __P_((CONST char *, int));
#endif
#if	!defined (_NOKANJICONV) || (defined (DEP_DOSEMU) && defined (CODEEUC))
extern int sjis2ujis __P_((char *, CONST u_char *, int));
extern int ujis2sjis __P_((char *, CONST u_char *, int));
#endif
#ifdef	DEP_UNICODE
extern VOID readunitable __P_((int));
extern VOID discardunitable __P_((VOID_A));
extern u_int unifysjis __P_((u_int, int));
extern u_int cnvunicode __P_((u_int, int));
#endif	/* DEP_UNICODE */
#ifndef	_NOKANJICONV
# ifdef	DEP_UNICODE
extern VOID ucs2normalization __P_((u_short *, int *, int, u_int, int));
extern u_int ucs2denormalization __P_((CONST u_short *, int *, int));
extern int ucs2toutf8 __P_((char *, int, u_int));
extern u_int ucs2fromutf8 __P_((CONST u_char *, int *));
# endif
extern int kanjiconv __P_((char *, CONST char *, int, int, int, int));
extern CONST char *kanjiconv2 __P_((char *, CONST char *, int, int, int, int));
extern char *newkanjiconv __P_((CONST char *, int, int, int));
extern VOID renewkanjiconv __P_((char **, int, int, int));
#endif	/* !_NOKANJICONV */
#if	defined (FD) && !defined (_NOKANJIFCONV)
extern int getkcode __P_((CONST char *));
#endif
#ifndef	_NOKANJICONV
extern int getoutputkcode __P_((VOID_A));
#endif
#ifdef	FD
extern char *convget __P_((char *, char *, int));
extern char *convput __P_((char *, CONST char *, int, int, char *, int *));
#endif

#ifdef	FD
extern int noconv;
#endif
#ifndef	_NOKANJIFCONV
extern int nokanjifconv;
extern char *sjispath;
extern char *eucpath;
extern char *jis7path;
extern char *jis8path;
extern char *junetpath;
extern char *ojis7path;
extern char *ojis8path;
extern char *ojunetpath;
extern char *hexpath;
extern char *cappath;
extern char *utf8path;
extern char *utf8macpath;
extern char *utf8iconvpath;
extern char *noconvpath;
#endif	/* !_NOKANJIFCONV */
#ifdef	DEP_UNICODE
extern char *unitblpath;
extern int unicodebuffer;
#endif
