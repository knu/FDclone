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
