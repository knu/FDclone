/*
 *	mkmfsed.c
 *
 *	sed script maker for Makefile
 */

#define	__FD_PRIMAL__
#include "fd.h"
#include <sys/param.h>

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USESELECTH
#include <sys/select.h>
#endif

#if	defined (USESYSCONF) && defined (_SC_OPEN_MAX)
#define	MAXOPENFILE	sysconf(_SC_OPEN_MAX)
#else
# ifdef	NOFILE
# define	MAXOPENFILE	NOFILE
# else
#  ifdef	OPEN_MAX
#  define	MAXOPENFILE	OPEN_MAX
#  else
#  define	MAXOPENFILE	64
#  endif
# endif
#endif

#ifndef	VER
#define	VER		"0.00"
#endif
#ifndef	DIST
#define	DIST		""
#endif
#ifndef	PREFIX
#define	PREFIX		"/usr/local"
#endif
#ifndef	CONFDIR
#define	CONFDIR		"/etc"
#endif
#ifndef	CCCOMMAND
#define	CCCOMMAND	"cc"
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
#if	FD >= 2
	printf("s:__RCVERSION__:%d:\n", FD);
#else
	printf("s:__RCVERSION__::\n");
#endif
	printf("s:__VER__:%s:\n", VER);
	if ((int)sizeof(DIST) > 1) printf("s:__DIST__:%s:\n", DIST);
	else printf("s:__DIST__:none:\n");
	printf("s:__PREFIX__:%s:\n", PREFIX);
	printf("s:__CONFDIR__:%s:\n", CONFDIR);
#ifdef	__CYGWIN__
	printf("s:__EXE__:.exe:g\n");
#else
	printf("s:__EXE__::g\n");
#endif
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
	printf("s:__ECHO__:echo:\n");
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
#if	defined (DEFFDSETSIZE) && defined (FD_SETSIZE)
	if (FD_SETSIZE < MAXOPENFILE)
		printf("s:__FDSETSIZE__:-DFD_SETSIZE=%d:\n", MAXOPENFILE);
	else
#endif
	printf("s:__FDSETSIZE__::\n");
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
