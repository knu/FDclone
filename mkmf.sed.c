/*
 *	sed script for Makefile
 */

#include "machine.h"

s:__OSTYPE__:OSTYPE:

s:__CC__:CCCOMMAND:
s:__CCOPTIONS__:EXTENDCCOPT:

s:__TERMLIBS__:TERMCAPLIB:
s:__REGLIBS__:REGEXPLIB:
s:__OTHERLIBS__:EXTENDLIB:

#ifdef	CODEEUC
s:__KCODEOPTION__:-e:
#else
s:__KCODEOPTION__:-s:
#endif

#ifdef	CPP7BIT
s:__MSBOPTION__:-7:
#else
s:__MSBOPTION__::
#endif
