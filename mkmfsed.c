/*
 *	mkmfsed.c
 *
 *	sed script maker for Makefile
 */

#include "machine.h"
#include <stdio.h>

int main (argc, argv)
int argc;
char *argv[];
{
	printf("s:__EXE__::g\n");
	printf("s:__OBJ__:.o:g\n");
	printf("s:__OBJS__:dosemu.o dosdisk.o:\n");
	printf("s:__OBJLIST__:$(OBJ1) $(OBJ2):\n");
	printf("s:__DEFRC__:'\"'$(DEFRC)'\"':\n");

	printf("s:	__RENAME__:#	mv:\n");
	printf("s:	__AOUT2EXE__:#	aout2exe:\n");
	printf("s:	__REMOVE__:#	rm -f:\n");
	printf("s:__COPY__:cp:\n");

	printf("s:__OSTYPE__:%s:\n", OSTYPE);

	printf("s:__CC__:%s:\n", CCCOMMAND);
	printf("s:__CPP__:$(CC) -E:\n");
	printf("s:__CCOPTIONS__:%s:\n", EXTENDCCOPT);

	printf("s:__TERMLIBS__:%s:\n", TERMCAPLIB);
	printf("s:__REGLIBS__:%s:\n", REGEXPLIB);
	printf("s:__OTHERLIBS__:%s:\n", EXTENDLIB);

#ifdef	CODEEUC
	printf("s:__KCODEOPTION__:-e:\n");
#else
	printf("s:__KCODEOPTION__:-s:\n");
#endif

#ifdef	CPP7BIT
	printf("s:__MSBOPTION__:-7:\n");
#else
	printf("s:__MSBOPTION__::\n");
#endif

	printf("s:__PREFIXOPTION__:-c:\n");

	exit(0);
}
