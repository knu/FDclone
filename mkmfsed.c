/*
 *	mkmfsed.c
 *
 *	sed script maker for Makefile
 */

#include "fd.h"

#ifdef	NOVOID
#define	VOID
#else
#define	VOID	void
#endif


/*ARGSUSED*/
int main(argc, argv)
int argc;
char *argv[];
{
#ifdef	USEMANLANG
	char *cp;
#endif

	printf("s:__VERSION__:%d:\n", FD);
/*NOTDEFINED*/
#if	FD >= 2
	printf("s:__RCVERSION__:%d:\n", FD);
#else
	printf("s:__RCVERSION__::\n");
#endif
/*NOTDEFINED*/
	printf("s:__PREFIX__:%s:\n", PREFIX);
	printf("s:__EXE__::g\n");
	printf("s:__OBJ__:.o:g\n");
	printf("s:__OBJS__:dosemu.o:\n");
	printf("s:__OBJLIST__:$(OBJ1) $(OBJ2) $(OBJ3):\n");
	printf("s:__DEFRC__:'\"'$(DEFRC)'\"':\n");

#ifdef	USEDATADIR
	printf("s:__UNITBLPATH__:-DDATADIR='\"'$(DATADIR)'\"':\n");
	printf("s:__DATADIR__:$(DATADIR):g\n");
#else
	printf("s:__UNITBLPATH__::\n");
	printf("s:__DATADIR__:$(BINDIR):g\n");
#endif

	printf("s:	__RENAME__:#	mv:\n");
	printf("s:	__AOUT2EXE__:#	aout2exe:\n");
	printf("s:	__REMOVE__:#	rm -f:\n");
	printf("s:__COPY__:cp:\n");
	printf("s:__RM__:rm -f:\n");

	printf("s:__OSTYPE__:%s:\n", OSTYPE);

#ifdef	USEMANLANG
	if ((cp = (char *)getenv("LANG")) && *cp)
		printf("s:__LANGDIR__:/%s:\n", cp);
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
	printf("s:__LN__:ln:\n");

	printf("s:__CC__:%s:\n", CCCOMMAND);
	printf("s:__CCOPTIONS__:%s:\n", EXTENDCCOPT);
#ifdef	HOSTCCCOMMAND
	if (strcmp(CCCOMMAND, HOSTCCCOMMAND)) {
		printf("s:__HOSTCC__:%s:\n", HOSTCCCOMMAND);
		printf("s:__HOSTCCOPTIONS__:-O:\n");
	}
	else
#endif
	{
		printf("s:__HOSTCC__:$(CC):\n");
		printf("s:__HOSTCCOPTIONS__:%s:\n", EXTENDCCOPT);
	}
	printf("s:__MEM__::\n");
	printf("s:__SHMEM__::\n");
	printf("s:__BSHMEM__::\n");
#ifdef	CCOUTOPT
	printf("s:__OUT__:%s:\n", CCOUTOPT);
#else
	printf("s:__OUT__:-o $@:\n");
#endif
#ifdef	CCLNKOPT
	printf("s:__LNK__:%s:\n", CCLNKOPT);
#else
	printf("s:__LNK__:-o $@:\n");
#endif

	printf("s:__TERMLIBS__:%s:\n", TERMCAPLIB);
#ifdef	_NOORIGGLOB
	printf("s:__REGLIBS__:%s:\n", REGEXPLIB);
#else
	printf("s:__REGLIBS__::\n");
#endif
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

#ifdef	_NODOSDRIVE
	printf("s:__UNITBL__::\n");
#else
	printf("s:__UNITBL__:$(UNITBL):\n");
#endif

#ifdef	SUPPORTSJIS
	printf("s:__PREFIXOPTION__::\n");
#else
	printf("s:__PREFIXOPTION__:-c:\n");
#endif

	return(0);
}
