/*
 *	mkmfsed.c
 *
 *	sed script maker for Makefile
 */

#define	__FD_PRIMAL__
#define	__HOST_CC__
#include "fd.h"
#include "version.h"

#ifdef	USESELECTH
#include <sys/select.h>
#endif
#ifdef	__CYGWIN__
#include <cygwin/version.h>
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

#define	MAXLINEBUF		255
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

static VOID NEAR strappend __P_((char *, CONST char *));
int main __P_((int, char *CONST []));


static VOID NEAR strappend(buf, s)
char *buf;
CONST char *s;
{
	int len, len2;

	if (!buf || !s) return;
	len = strlen(buf);
	len2 = strlen(s);
	if (len >= MAXLINEBUF) return;
	if (len + 1 + len2 >= MAXLINEBUF) len2 = MAXLINEBUF - (len + 1);
	if (len2 <= 0) return;
	if (len) buf[len++] = ' ';
	memcpy(&(buf[len]), s, len2);
	buf[len + len2] = '\0';
}

/*ARGSUSED*/
int main(argc, argv)
int argc;
char *CONST argv[];
{
	char buf1[MAXLINEBUF + 1], buf2[MAXLINEBUF + 1], buf3[MAXLINEBUF + 1];
	CONST char *cp;
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
	printf("s:__DICTSRC__::\n");
	printf("s:__MKDICTOPTION__::\n");
#endif	/* !DEP_IME */
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

#if	!defined (__CYGWIN__) || !defined (CYGWIN_VERSION_DLL_MAJOR)
	n = 1;
#else	/* __CYGWIN__ && CYGWIN_VERSION_DLL_MAJOR */
	n = (CYGWIN_VERSION_DLL_MAJOR) * 1000;
# ifdef	CYGWIN_VERSION_DLL_MINOR
	n += CYGWIN_VERSION_DLL_MINOR;
# endif
#endif	/* __CYGWIN__ && CYGWIN_VERSION_DLL_MAJOR */
#ifdef	OSTYPE2
	printf("s:__OSTYPE__:%s=%d -D%s=1:\n", OSTYPE, n, OSTYPE2);
#else
	printf("s:__OSTYPE__:%s=%d:\n", OSTYPE, n);
#endif

#ifdef	USEMANLANG
	if ((cp = (char *)getenv("LANG")) && *cp) {
		n = strlen(cp);
# ifdef	LANGWIDTH
		if (n > LANGWIDTH) n = LANGWIDTH;
# endif
		printf("s:__LANGDIR__:/%-.*s:\n", n, cp);
	}
	else
#endif	/* USEMANLANG */
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

	*buf1 = *buf2 = *buf3 = '\0';
	printf("s:__CC__:%s:\n", CCCOMMAND);
	printf("s:__CFLAGS__:%s:\n", CFLAGS);
	printf("s:__CPPFLAGS__:%s:\n", CPPFLAGS);
	printf("s:__LDFLAGS__:%s:\n", LDFLAGS);
	strappend(buf1, "$(OSOPTS)");
	strappend(buf1, EXTENDCCOPT);
	strappend(buf1, "$(DEBUG)");
#if	defined (__GNUC__) && defined (__GNUC_MINOR__)
# if	__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
	strappend(buf3, "-Wno-attributes");
# endif
# if	__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
	strappend(buf3, "-Wno-empty-body");
# endif
#endif	/* __GNUC__ && __GNUC_MINOR__ */
#ifdef	HOSTCCCOMMAND
	if (strcmp(CCCOMMAND, HOSTCCCOMMAND)) {
		printf("s:__HOSTCC__:%s:\n", HOSTCCCOMMAND);
		printf("s:__HOSTCFLAGS__:%s:\n", HOSTCFLAGS);
		printf("s:__HOSTCPPFLAGS__:%s:\n", HOSTCPPFLAGS);
		printf("s:__HOSTLDFLAGS__:%s:\n", HOSTLDFLAGS);
# if	defined (H___GNUC__) && defined (H___GNUC_MINOR__)
#  if	H___GNUC__ > 4 || (H___GNUC__ == 4 && H___GNUC_MINOR__ >= 1)
		strappend(buf1, "-Wno-attributes");
#  endif
#  if	H___GNUC__ > 4 || (H___GNUC__ == 4 && H___GNUC_MINOR__ >= 3)
		strappend(buf1, "-Wno-empty-body");
#  endif
# endif	/* H___GNUC__ && H___GNUC_MINOR__ */
		strappend(buf1, "$(CFLAGS)");
		strappend(buf2, "$(OSOPTS)");
		strappend(buf2, "-D__HOST_CC__");
		strappend(buf2, buf3);
		strappend(buf2, "$(HOSTCFLAGS)");
	}
	else
#endif	/* HOSTCCCOMMAND */
	{
		printf("s:__HOSTCC__:$(CC):\n");
		printf("s:__HOSTCFLAGS__:$(CFLAGS):\n");
		printf("s:__HOSTCPPFLAGS__:$(CPPFLAGS):\n");
		printf("s:__HOSTLDFLAGS__:$(LDFLAGS):\n");
		strappend(buf1, buf3);
		strappend(buf1, "$(CFLAGS)");
		strappend(buf2, "$(COPTS)");
	}

	printf("s:__COPTS__:%s:\n", buf1);
	printf("s:__HOSTCOPTS__:%s:\n", buf2);
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

	*buf1 = *buf2 = *buf3 = '\0';
	strappend(buf1, TERMCAPLIB);
#ifdef	_NOORIGGLOB
	strappend(buf1, REGEXPLIB);
	strappend(buf2, REGEXPLIB);
	strappend(buf3, REGEXPLIB);
#endif
#ifndef	_NOSOCKET
	strappend(buf1, SOCKETLIB);
	strappend(buf3, SOCKETLIB);
#endif
	strappend(buf1, EXTENDLIB);
	strappend(buf2, EXTENDLIB);
	strappend(buf3, EXTENDLIB);
	cp = "$(ALLOC)";
	strappend(buf1, cp);
	strappend(buf2, cp);
	strappend(buf3, cp);
	cp = "$(LDFLAGS)";
	strappend(buf1, cp);
	strappend(buf2, cp);
	strappend(buf3, cp);
	printf("s:__FLDFLAGS__:%s:\n", buf1);
	printf("s:__SLDFLAGS__:%s:\n", buf2);
	printf("s:__NLDFLAGS__:%s:\n", buf3);

#ifdef	CODEEUC
	printf("s:__KCODEOPTION__:-e:\n");
#else
	printf("s:__KCODEOPTION__:-s:\n");
#endif
#ifdef	UTF8DOC
	printf("s:__KDOCOPTION__:-u:\n");
#else	/* !UTF8DOC */
# ifdef	CODEEUC
	printf("s:__KDOCOPTION__:-e:\n");
# else
	printf("s:__KDOCOPTION__:-s:\n");
# endif
#endif	/* !UTF8DOC */

#ifdef	CPP7BIT
	printf("s:__MSBOPTION__:-7:\n");
#else
	printf("s:__MSBOPTION__::\n");
#endif

	*buf1 = '\0';
#if	defined (DEP_UNICODE) && !defined (DEP_EMBEDUNITBL)
	strappend(buf1, "$(UNITBL)");
#endif
#if	defined (DEP_IME) && !defined (DEP_EMBEDDICTTBL)
	strappend(buf1, "$(DICTTBL)");
#endif
#ifndef	_NOCATALOG
	strappend(buf1, "$(CATTBL)");
	strappend(buf1, "$(ECATTBL)");
#endif
	printf("s:__TABLES__:%s:\n", buf1);

#if	defined (DEP_UNICODE) && defined (DEP_EMBEDUNITBL)
	printf("s:__UNITBLOBJ__:unitbl$(OBJ):\n");
#else
	printf("s:__UNITBLOBJ__::\n");
#endif
#ifdef	UTF8DOC
	printf("s:__TUNITBLOBJ__:tunitbl$(OBJ):\n");
#else
	printf("s:__TUNITBLOBJ__::\n");
#endif
#if	defined (DEP_IME) && defined (DEP_EMBEDDICTTBL)
	printf("s:__DICTTBLOBJ__:dicttbl$(OBJ):\n");
#else
	printf("s:__DICTTBLOBJ__::\n");
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
