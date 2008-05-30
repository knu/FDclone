/*
 *	catalog.h
 *
 *	function prototype declarations for "catalog.c"
 */

#include "depend.h"

#ifdef	_NOCATALOG
# ifdef	_NOENGMES
# define	mesconv2(n, jpn, eng)	(jpn)
# define	meslist(n, jpn, eng)	jpn
# else	/* !_NOENGMES */
#  ifdef	_NOJPNMES
#  define	mesconv2(n, jpn, eng)	(eng)
#  define	meslist(n, jpn, eng)	eng
#  else
#  define	mesconv2(n, jpn, eng)	mesconv(jpn, eng)
#  define	meslist(n, jpn, eng)	jpn, eng
#  endif
# endif	/* !_NOENGMES */
#else	/* !_NOCATALOG */
#define	mesconv2(n, jpn, eng)	mesconv(n)
#define	meslist(n, jpn, eng)	(n)
#endif	/* !_NOCATALOG */

#ifdef	_NOCATALOG
# if	!defined (_NOENGMES) && !defined (_NOJPNMES)
extern CONST char *mesconv __P_((CONST char *, CONST char *));
# endif
#else	/* !_NOCATALOG */
extern VOID freecatalog __P_((VOID_A));
extern char **listcatalog __P_((VOID_A));
extern int chkcatalog __P_((VOID_A));
extern CONST char *mesconv __P_((int));

extern char *cattblpath;
extern CONST char *catname;
#endif	/* !_NOCATALOG */
