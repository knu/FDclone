/*
 *	mkmfsed.c
 *
 *	sed script for Makefile
 */

#include "machine.h"

s:__EXE__::g
s:__OBJ__:.o:g
s:__OBJS__:dosemu.o dosdisk.o:
s:__OBJLIST__:$(OBJ1) $(OBJ2):
s:__DEFRC__:'"'$(DEFRC)'"':

s:__RENAME__:#:
s:__AOUT2EXE__:#:
s:__REMOVE__:#:

s:__OSTYPE__:OSTYPE:

s:__CC__:CCCOMMAND:
s:__CPP__:$(CC) -E:
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

s:__PREFIXOPTION__:-c:
