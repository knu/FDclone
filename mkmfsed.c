/*
 *	mkmfsed.c
 *
 *	sed script maker for Makefile
 */

#include <stdio.h>
#include "machine.h"

#ifdef	NOVOID
#define	VOID
#else
#define	VOID	void
#endif

#ifdef	USEMANLANG
extern char *getenv __P_((char *));
#endif
extern VOID exit __P_((int));

int main (argc, argv)
int argc;
char *argv[];
{
#ifdef	USEMANLANG
	char *cp;
#endif

	printf("s:__EXE__::g\n");
	printf("s:__OBJ__:.o:g\n");
	printf("s:__OBJS__:dosemu.o dosdisk.o:\n");
	printf("s:__OBJLIST__:$(OBJ1) $(OBJ2) $(OBJ3):\n");
	printf("s:__DEFRC__:'\"'$(DEFRC)'\"':\n");

	printf("s:	__RENAME__:#	mv:\n");
	printf("s:	__AOUT2EXE__:#	aout2exe:\n");
	printf("s:	__REMOVE__:#	rm -f:\n");
	printf("s:__COPY__:cp:\n");

	printf("s:__OSTYPE__:%s:\n", OSTYPE);

#ifdef	USEMANLANG
	if ((cp = getenv("LANG")) && *cp) printf("s:__LANGDIR__:/%s:\n", cp);
	else
#endif
	printf("s:__LANGDIR__::\n");

#ifdef	BSDINSTALL
# ifdef	BSDINSTCMD
	printf("s:__INSTALL__:%s -c:\n", BSDINSTCMD);
# else
	printf("s:__INSTALL__:install -c:\n");
# endif
	printf("s:__INSTSTRIP__:-s:\n");
#else
	printf("s:__INSTALL__:cp -p:\n");
	printf("s:__INSTSTRIP__::\n");
#endif

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
