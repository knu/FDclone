/*
 *	sed script for Makefile
 */

#include "machine.h"

#ifdef	TERMCAPLIB
s/__TERMLIBS__/TERMCAPLIB/
#else
s/__TERMLIBS__//
#endif

#ifdef	REGEXPLIB
s/__REGLIBS__/REGEXPLIB/
#else
s/__REGLIBS__//
#endif

#ifdef	EXTENDLIB
s/__OTHERLIBS__/EXTENDLIB/
#else
s/__OTHERLIBS__//
#endif

#ifdef	NOOPTIMIZE
# ifdef	UNSIGNEDCHAR
s/__CCOPTIONS__/-signed/
# else
s/__CCOPTIONS__//
# endif
#else	/* NOOPTIMIZE */
# ifdef	UNSIGNEDCHAR
s/__CCOPTIONS__/-O -signed/
# else
s/__CCOPTIONS__/-O/
# endif
#endif	/* NOOPTIMIZE */
