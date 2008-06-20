/*
 *	mkmfsed.c
 *
 *	sed script maker for Makefile
 */

#define	__FD_PRIMAL__
#include "fd.h"
#include "version.h"

#ifdef	USESELECTH
#include <sys/select.h>
#endif

#if	defined (USESYSCONF) && defined (_SC_OPEN_MAX)
#define	MAXOPENFILE		sysconf(_SC_OPEN_MAX)
#else	/* !USESYSCONF || !_SC_OPEN_MAX */
# ifdef	NOFILE
# define	MAXOPENFILE	NOFILE
# else	/* !NOFILE */
#  ifdef	OPEN_MAX
#  define	MAXOPENFILE	OPEN_MAX
#  else
#  define	MAXOPENFILE	64
#  endif
# endif	/* !NOFILE */
#endif	/* !USESYSCONF || !_SC_OPEN_MAX */

#ifndef	PREFIX
#define	PREFIX			"/usr/local"
#endif
#ifndef	CONFDIR
#define	CONFDIR			"/etc"
#endif
#ifndef	CCCOMMAND
#define	CCCOMMAND		"cc"
#endif
#ifndef	DICTSRC
#define	DICTSRC			""
#endif

int main __P_((int, char *CONST []));


/*ARGSUSED*/
int main(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp;
	int n;

	printf("s:__VERMAJ__:%d:\n", FD);
#if	FD >= 2
	printf("s:__RCVERSION__:%d:\n", 2);
#else
	printf("s:__RCVERSION__::\n");
#endif
	if (!(cp = strchr(version, ' '))) cp = version;
	else while (*cp == ' ') cp++;
	for (n = 0; cp[n]; n++) if (cp[n] == ' ') break;
	printf("s:__VERSION__:%-.*s:\n", n, cp);
	if (!distributor || !*distributor) distributor = "none";
	printf("s:__DIST__:%s:\n", distributor);
	printf("s:__PREFIX__:%s:\n", PREFIX);
	printf("s:__CONFDIR__:%s:\n", CONFDIR);
#ifdef	__CYGWIN__
	printf("s:__EXE__:.exe:g\n");
#else
	printf("s:__EXE__::g\n");
#endif
	printf("s:__OBJ__:.o:g\n");
	printf("s:__DOSOBJS__::\n");
#ifdef	DEP_IME
	printf("s:__IMEOBJS__:$(IMEOBJS):\n");
	printf("s:__DICTTBL__:$(DICTTBL):\n");
	if (DICTSRC[0]) {
		printf("s:__DICTSRC__:%s:\n", DICTSRC);
		printf("s:__MKDICTOPTION__:-h -v:\n");
	}
	else {
		printf("s:__DICTSRC__:$(DICTTXT):\n");
		printf("s:__MKDICTOPTION__::\n");
	}
#else	/* !DEP_IME */
	printf("s:__IMEOBJS__::\n");
	printf("s:__DICTTBL__::\n");
	printf("s:__DICTSRC__::\n");
	printf("s:__MKDICTOPTION__::\n");
#endif	/* !DEP_IME */
#ifdef	_NOCATALOG
	printf("s:__CATTBL__::\n");
#else
	printf("s:__CATTBL__:$(CATTBL) $(ECATTBL):\n");
#endif
#ifdef	_NOSOCKET
	printf("s:__SOCKETOBJS__::\n");
	printf("s:__SOCKETLIBS__::\n");
#else
	printf("s:__SOCKETOBJS__:$(SCKOBJS):\n");
	printf("s:__SOCKETLIBS__:%s:\n", SOCKETLIB);
#endif
	printf("s:__OBJLIST__:$(OBJ1) $(OBJ2) $(OBJ3) $(OBJ4) $(OBJ5) $(OBJ6):\n");
	printf("s:__SOBJLIST__:$(SOBJ1) $(SOBJ2):\n");
	printf("s:__NOBJLIST__:$(NOBJ1) $(NOBJ2) $(NOBJ3):\n");
	printf("s:__DEFRC__:'\"'$(DEFRC)'\"':\n");

#ifdef	USEDATADIR
	printf("s:__TBLPATH__:-DBINDIR='\"'$(BINDIR)'\"' -DDATADIR='\"'$(DATADIR)'\"':\n");
	printf("s:__DATADIR__:$(DATADIR):g\n");
	printf("s:__DATADIR2__:$(DATADIR)/$(VERSION):g\n");
#else
	printf("s:__TBLPATH__:-DBINDIR='\"'$(BINDIR)'\"':\n");
	printf("s:__DATADIR__:$(BINDIR):g\n");
	printf("s:__DATADIR2__:$(BINDIR):g\n");
#endif

#ifdef	BUGGYMAKE
	printf("s:__SLEEP__::\n");
#else
	printf("s:__SLEEP__:#:\n");
#endif
	printf("s:__DJGPP1__:#:\n");
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
#if	!defined (__GNUC__) || !defined (__GNUC_MINOR__) \
|| __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 1)
	printf("s:__CCOPTIONS__:%s:\n", EXTENDCCOPT);
#else
	printf("s:__CCOPTIONS__:%s -Wno-attributes:\n", EXTENDCCOPT);
#endif
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
	printf("s:__NSHMEM__::\n");
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
	printf("s:[\t ]*$::\n");
	printf("/^[\t ][\t ]*-*\\$(RM) *$/d\n");

	return(0);
}
