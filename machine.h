/*
 *	machine.h
 *
 *	definitions of machine depended identifier
 */

#ifndef	__MACHINE_H_
#define	__MACHINE_H_

#undef	MSDOS
#define	MSDOS			(DOSV || PC98 || J3100)

#if	MSDOS
#define	NOMULTIKANJI
#define	PATHNOCASE
#define	COMMNOCASE
#define	ENVNOCASE
#define	NOUID
#define	NODIRLOOP
#define	NOSYMLINK
#define	BSPATHDELIM
#define	USECRNL
#define	CWDINPATH
#define	DOUBLESLASH
#define	NOTZFILEH
#define	USETIMEH
#define	USEUTIME
#define	NOFLOCK
#define	NOSYSLOG
#define	USEMKTIME
#define	USESTRERROR
#define	SENSEPERSEC		20
# if	defined (__TURBOC__) && defined (__WIN32__)
# define	BCC32
# endif
# ifdef	__GNUC__
# define	USERESOURCEH
# define	SIGFNCINT
#  ifndef	DJGPP
#  define	DJGPP		1
#  else
#  define	USEREGCOMP
#  endif
# else	/* !__GNUC__ */
#  ifndef	BCC32
#  define	NOUID_T
#  endif
# define	NOFILEMODE
# define	NOUNISTDH
# endif	/* !__GNUC__ */
# if	!defined (__GNUC__) || (DJGPP >= 2)
typedef unsigned int		u_int;
typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned long		u_long;
# endif
# ifdef	__TURBOC__
#  ifndef	BCC32
#  define	NEAR		near
#  define	DOS16
#  endif
# define	FORCEDSTDC
# define	SIGFNCINT
typedef long	off_t;
# endif
# ifdef	LSI_C
# define	NEAR		near
# define	DOS16
# define	FORCEDSTDC
# endif
# if	defined (DJGPP) || defined (BCC32)
# define	PROTECTED_MODE
# endif
# if	!defined (DJGPP) || (DJGPP < 2)
# define	NOSELECT
# endif
#endif	/* !MSDOS */

#ifndef	NEAR
#define	NEAR
#endif

#if	defined (sun)
#define	CODEEUC
#define	CPP7BIT
#define	USELEAPCNT
#define	SOCKETLIB		"-lsocket -lnsl"
#include <sys/param.h>
# if	!defined (MAXHOSTNAMELEN) \
|| defined (USGr4) || defined (__svr4__) || defined (__SVR4)
# define	SVR4
# define	OSTYPE		"SOLARIS"
# define	EXTENDCCOPT	"-O -D_FILE_OFFSET_BITS=64"
# include <stdio.h>
#  if	defined (__SUNPRO_C) && defined (BSD)
#  define	USEDIRECT
#  define	USERE_COMP
#  endif
# define	USEMANLANG
# define	REGEXPLIB	"-lgen"
# define	USEMKDEVH
# define	NODNAMLEN
# define	DNAMESIZE	1
# define	NOTMGMTOFF
# define	USEINSYSTMH
# define	USESTATVFSH
# define	USESTATVFS_T
# define	USEMNTTABH
# define	USEMKTIME
# define	SIGFNCINT
# define	USEINETPTON
# else	/* MAXHOSTNAMELEN && !USGr4 && !__svr4__ && !__SVR4 */
# define	BSD43
# define	OSTYPE		"SUN_OS"
# define	BSDINSTALL
# define	USERE_COMP
# define	USETIMELOCAL
# define	USESYSCONF
# define	USEWAITPID
# define	USERESOURCEH
# define	USESETVBUF
# endif	/* MAXHOSTNAMELEN && !USGr4 && !__svr4__ && !__SVR4 */
#endif

#if	defined (sony)
#define	USELEAPCNT
#define	USEMANLANG
#define	SUPPORTSJIS
# if	defined (USGr4) || defined (SYSTYPE_SYSV)
# define	SVR4
# define	OSTYPE		"NEWS_OS6"
# define	REGEXPLIB	"-lgen"
# define	USEMKDEVH
# define	NODNAMLEN
# define	DNAMESIZE	1
# define	NOTMGMTOFF
# define	USESTATVFSH
# define	USEMNTTABH
# define	USEMKTIME
# define	USEUTIME
# define	USETIMES
# define	SIGFNCINT
# define	GETTODARGS	1
# else	/* !USGr4 && !SYSTYPE_SYSV */
# define	BSD43
# define	BSDINSTALL
# define	USERE_COMP
# define	SIGARGINT
#  if	defined (__sony) || !defined (bsd43)
#  define	OSTYPE		"NEWS_OS4"
#  define	USESETENV
#  define	USERESOURCEH
#  else	/* !__sony && bsd43 */
#  define	OSTYPE		"NEWS_OS3"
#  define	NOERRNO
#  define	NOFILEMODE
#  define	NOUNISTDH
#  define	NOSTDLIBH
#  define	USEDIRECT
#  define	NOTMGMTOFF
#  define	NOVSPRINTF
#  define	USEGETWD
#  define	USETIMES
#  endif	/* !__sony && bsd43 */
# endif	/* !USGr4 && !SYSTYPE_SYSV */
#endif

#if	defined (sgi)
#define	SYSV
#define	OSTYPE			"IRIX"
#define	CODEEUC
# if	defined (_COMPILER_VERSION) && (_COMPILER_VERSION >= 600)
# define	EXTENDCCOPT	"-32 -O"
# endif
# if	!defined (SYSTYPE_SVR4) && !defined (_SYSTYPE_SVR4)
# define	TERMCAPLIB	"-lcurses"
# define	EXTENDLIB	"-lsun"
# endif
#define	USEPID_T
#define	DECLERRLIST
#define	USEMKDEVH
#define	NODNAMLEN
#define	DNAMESIZE		1
#define	NOTMGMTOFF
#define	USESTATFSH
#define	STATFSARGS		4
#define	USERE_COMP
#define	USEMKTIME
#define	USESYSCONF
#define	USEWAITPID
#define	USESIGPMASK
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#define	USESETVBUF
#define	USESTRERROR
#endif

#if	defined (hpux) || defined (__hpux) \
|| defined (__H3050) || defined (__H3050R) || defined (__H3050RX)
#define	SYSV
#define	OSTYPE			"HPUX"
# ifdef	__CLASSIC_C__
# define	NOSTDC
# endif
# ifdef	__STDC_EXT__
# define	EXTENDCCOPT	"-D_FILE_OFFSET_BITS=64"
# endif
/*
 *	This is a fake '#if' for some buggy HP-UX terminfo library.
 *	Will you please define 'BUGGY_HPUX' manually on such environment.
 *
 *	Some HP-UX has buggy libcurses/terminfo, to use libtermcap/termcap.
 *	Another HP-UX has buggy libtermlib/termcap, to use libcurses/terminfo.
 *	Or another HP-UX does not support termcap at all.
 *	If you use HP-UX and the terminal trouble occured,
 *	you should try to define 'BUGGY_HPUX' here and to re-compile.
 *
#define	BUGGY_HPUX
 *
 */
# ifndef	__HP_cc
# define	BUGGY_HPUX	/* Maybe HP-UX 10.20 */
# endif
# if	!defined (BUGGY_HPUX)
# define	USETERMINFO
# define	TERMCAPLIB	"-lcurses"
# endif
#define	BUGGYMAKE
#define	USEPID_T
#define	NOSIGLIST
#define	NOTZFILEH
#define	USETIMEH
#define	NOTMGMTOFF
#define	USEINSYSTMH
#define	USEREGCOMP
#define	NOTERMVAR
#define	USEUTIME
#define	USEMKTIME
#define	USESYSCONF
#define	USEWAITPID
#define	USESIGPMASK
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#define	USESETVBUF
#define	SIGFNCINT
#define	USESTRERROR
#define	USESETSID
#define	USESETRESUID
#define	USESETRESGID
#endif

#if	defined (nec_ews) || defined (_nec_ews)
#define	CODEEUC
# if	defined (nec_ews_svr4) || defined (_nec_ews_svr4)
# define	SVR4
# define	OSTYPE		"EWSUXV"
# undef	CCCOMMAND
#  if	defined (nec_ews)
#  define	CCCOMMAND	"/usr/necccs/bin/cc"
#  else
#  define	CCCOMMAND	"/usr/abiccs/bin/cc"
#  define	STRICTSTDC
#  endif
# define	TERMCAPLIB	"-lcurses"
# define	REGEXPLIB	"-lgen"
# define	USEMKDEVH
# define	NODNAMLEN
# define	DNAMESIZE	1
# define	NOTMGMTOFF
# define	USESTATVFSH
# define	USEMNTTABH
# define	USEUTIME
# define	USEMKTIME
# define	SIGFNCINT
# else	/* !nec_ews_svr4 && !_nec_ews_svr4 */
# define	SYSV
# endif	/* !nec_ews_svr4 && !_nec_ews_svr4 */
#endif

#if	defined (uniosu)
#define	BSD43
#define	OSTYPE			"UNIOSU"
#define	CODEEUC
#define	EXTENDCCOPT		"-O -Zs"
#define	TERMCAPLIB		"-lcurses"
#define	REGEXPLIB		"-lc -lPW"
#define	UNKNOWNFS
#define	NOVOID
#define	NOUID_T
#define	NOFILEMODE
#define	NOSTDLIBH
#define	USETIMEH
#define	USETERMIO
#define	SYSVDIRENT
#define	HAVETIMEZONE
#define	USEREGCMP
#define	USERESOURCEH
#define	WAITKEYPAD		720
#define	WAITKANJI		720
#endif

#if	defined (uniosb)
#define	BSD43
#define	OSTYPE			"UNIOSB"
#define	NOERRNO
#define	NOFILEMODE
#define	NOUNISTDH
#define	NOSTDLIBH
#define	USEDIRECT
#define	USERE_COMP
#define	USESETENV
#define	NOVSPRINTF
#define	USEGETWD
#define	USERESOURCEH
#define	SIGARGINT
#endif

#if	defined (luna88k)
#define	BSD43
#define	OSTYPE			"LUNA88K"
#define	CODEEUC
#define	NOERRNO
#define	NOFILEMODE
#define	USEDIRECT
#define	USERE_COMP
#define	USESETENV
#define	USEGETWD
#define	USERESOURCEH
#define	SIGARGINT
#endif

#if	(defined (alpha) || defined (__alpha) || defined (__alpha__)) \
&& !defined (linux) \
&& !defined (__FreeBSD__) && !defined (__NetBSD__) && !defined (__OpenBSD__)
#define	CODEEUC
#define	TARUSESPACE
#define	NOFUNCMACRO
#define	EXTENDLIB		"-lc_r"
#define	USEMNTINFOR
# if	defined (SYSTYPE_BSD)
# define	BSD43
# define	OSTYPE		"DECOSF1V2"
# define	BSDINSTALL
# define	BSDINSTCMD	"installbsd"
# define	USEMOUNTH
# define	STATFSARGS	3
# define	USERE_COMP
# define	USERESOURCEH
# else	/* !SYSTYPE_BSD */
# define	SVR4
# define	OSTYPE		"DECOSF1V3"
# define	DECLERRLIST
# define	USELEAPCNT
# define	USETERMIO
# define	USESTATVFSH
# define	USEREGCOMP
# define	USEMKTIME
# define	SIGFNCINT
# endif	/* !SYSTYPE_BSD */
#endif

#if	defined (_IBMR2)
#define	SVR4
#define	OSTYPE			"AIX"
#define	EXTENDCCOPT		"-O -D_LARGE_FILES -U_LARGE_FILE_API"
#define	TERMCAPLIB		"-lcurses"
#define	NOTZFILEH
#define	USESELECTH
#define	USESYSDIRH
#define	USETIMEH
#define	USETERMIO
#define	NOTMGMTOFF
#define	USEMNTCTL
#define	USERE_COMP
# if	defined (_AIX41)
# define	HAVELONGLONG
# define	USESTATVFSH
# define	USEMKTIME
# define	SIGFNCINT
# else
# define	USESTATFSH
# define	STATFSARGS	4
# define	SIGARGINT
# endif
# if	defined (_AIX43)
# define	NOTERMVAR
# endif
#endif

#if	defined (ultrix)
#define	BSD43
#define	OSTYPE			"ULTRIX"
#define	CODEEUC
#define	BSDINSTALL
#define	TARUSESPACE
#define	CPP7BIT
#define	USEFSDATA
#define	USEGETMNT
#define	USERE_COMP
#define	USESETENV
#define	USEMKTIME
#define	USESYSCONF
#define	USESIGPMASK
#define	USERESOURCEH
#endif

#if	defined (_AUX_SOURCE)
#define	SYSV
#define	OSTYPE			"AUX"
#define	CPP7BIT
#define	TERMCAPLIB		"-ltermcap"
#define	UNKNOWNFS		/* Because of the buggy (?) 'rename(2)' */
#define	PWNEEDERROR
#define	USELEAPCNT
#define	USETIMEH
#endif

#if	defined (DGUX) || defined (__DGUX__)
#define	SYSV
#define	OSTYPE			"DGUX"
#define	CODEEUC
#define	TERMCAPLIB		"-ltermcap"
#define	USESTATFSH
#define	STATFSARGS		4
#define	USESYSDIRH
#define	NODNAMLEN
#define	DNAMESIZE		1
#define	USERE_COMP
#endif

#if	defined (__uxpm__)
#define	SVR4
#define	OSTYPE			"UXPM"
#define	CODEEUC
#define	CCOUTOPT		"-o $*"
#define	REGEXPLIB		"-lgen"
#define	USEMKDEVH
#define	NODNAMLEN
#define	DNAMESIZE		1
#define	NOTMGMTOFF
#define	USESTATVFSH
#define	USEMNTTABH
#define	USEUTIME
#define	USEMKTIME
#endif

#if	defined (__uxps__)
#define	SVR4
#define	OSTYPE			"UXPDS"
#define	CODEEUC
#define	REGEXPLIB		"-lgen"
#define	USEMKDEVH
#define	NODNAMLEN
#define	DNAMESIZE		1
#define	NOTMGMTOFF
#define	USESTATVFSH
#define	USEMNTTABH
#define	USEUTIME
#define	USEMKTIME
#endif

#if	defined (NeXT)
#define	BSD43
#define	OSTYPE			"NEXTSTEP"
#define	CODEEUC
#define	NOFILEMODE
#define	NOUNISTDH
#define	USEDIRECT
#define	USERE_COMP
#define	USEGETWD
#define	USERESOURCEH
#endif

#if	defined (__CYGWIN__)
#define	POSIX
#define	OSTYPE			"CYGWIN"
#define	EXTENDCCOPT		"-O -D_FILE_OFFSET_BITS=64"
#define	PATHNOCASE
#define	COMMNOCASE
#define	USECRNL
#define	DOUBLESLASH
#define	USEMANLANG
#define	BSDINSTALL
#define	TARUSESPACE
#define	TERMCAPLIB		"-ltermcap"
#define	NOSIGLIST
#define	DECLERRLIST
#define	HAVECLINE
#define	SYSVDIRENT
#define	NODNAMLEN
#define	NODRECLEN
#define	HAVETIMEZONE
#define	NOTMGMTOFF
#define	USEREGCOMP
#define	USESETENV
#define	USEMKTIME
#define	DEFFDSETSIZE
#define	SIGFNCINT
#define	USESOCKLEN
#define	USEINETATON
#define	WAITKEYPAD		36
#endif

#if	defined (linux)
#define	POSIX
#define	OSTYPE			"LINUX"
#define	CODEEUC
# if	defined (alpha) || defined (__alpha) || defined (__alpha__)
/* Linux/Alpha need not support LFS and cannot use statfs(2) with LFS */
# define	EXTENDCCOPT	"-O"
# else
# define	EXTENDCCOPT	"-O -D_FILE_OFFSET_BITS=64"
# endif
#define	USEMANLANG
#define	BSDINSTALL
#define	TARUSESPACE
/*
 *	This is a fake '#if' for some buggy Slackware distribution.
 *	Will you please define 'Slackware' manually on Slackware.
 *
 *	Some old SlackWare has buggy libncurses, to use libtermcap.
 *	Another distribution has no libtermcap, to use libncurses.
 *	If you use older Slackware and the terminal trouble occured,
 *	you should try to define 'Slackware' here and to re-compile.
 *
#define	Slackware
 *
 */
# if	defined (Slackware)
# define	TERMCAPLIB	"-ltermcap"
# else
# define	TERMCAPLIB	"-lncurses"
# endif
#define	DECLSIGLIST
#define	DECLERRLIST
#define	NOTZFILEH
#define	USETIMEH
#define	HAVECLINE
#define	SYSVDIRENT
#define	NODNAMLEN
#define	HAVETIMEZONE
#define	NOTMGMTOFF
#define	USEREGCOMP
#define	USESETENV
#define	USEMKTIME
# if	!defined (alpha) && !defined (__alpha) && !defined (__alpha__) \
&& !defined (ia64) && !defined (__ia64) && !defined (__ia64__) \
&& !defined (x86_64) && !defined (__x86_64) && !defined (__x86_64__) \
&& !defined (s390x) && !defined (__s390x) && !defined (__s390x__) \
&& !defined (CONFIG_ARCH_S390X) \
&& (!defined (PPC) || !defined (__GNUC__) || __GNUC__ >= 3)	/* for bug */
# define	USELLSEEK
# endif
#define	SIGFNCINT
#define	USESOCKLEN
#define	USEINETATON
#endif

#if	defined (__FreeBSD__) && defined (__powerpc__)
#define	BSD44
#define	OSTYPE			"JCCBSD"
#define	CODEEUC
#define	TARUSESPACE
#define	REGEXPLIB		"-lcompat"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USELEAPCNT
#define	USEMOUNTH
#define	USEMNTINFO
#define	USERE_COMP
#define	USESETENV
#define	USEMKTIME
#define	SIGFNCINT
#define	USEINETATON
#endif

#if	defined (__FreeBSD__) && !defined (OSTYPE) && !defined (__BOW__)
#define	BSD44
#define	OSTYPE			"FREEBSD"
#define	CODEEUC
#define	USEMANLANG
#define	BSDINSTALL
#define	TARUSESPACE
#define	REGEXPLIB		"-lcompat"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	NOTZFILEH
#define	USEMOUNTH
#define	USEMNTINFO
# if	__FreeBSD__ < 3
# define	USEVFCNAME
# else
# define	USEFFSTYPE
# endif
#define	USERE_COMP
#define	USESETENV
#define	USEMKTIME
#define	SIGFNCINT
#define	USEINETATON
#endif

#if	defined (__NetBSD__)
#define	BSD44
#define	OSTYPE			"NETBSD"
#define	CODEEUC
#define	BSDINSTALL
#define	TARUSESPACE
#define	REGEXPLIB		"-lcompat"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USELEAPCNT
#define	USEFFSTYPE
#define	USERE_COMP
#define	USESETENV
#define	USEMKTIME
#define	SIGFNCINT
#define	USEINETATON
#include <sys/param.h>
# if	defined (NetBSD1_0) && (NetBSD1_0 < 1)
# define	USEFFSIZE
# endif
# if	defined (__NetBSD_Version__) && (__NetBSD_Version__ >= 300000000)
# define	USESTATVFSH
# define	USEGETVFSTAT
# else
# define	USEMOUNTH
# define	USEMNTINFO
# endif
#endif

#if	defined (__bsdi__)
#define	BSD44
#define	OSTYPE			"BSDOS"
#define	CODEEUC
#define	TARUSESPACE
#define	REGEXPLIB		"-lcompat"
#define	STRICTSTDC
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USEMOUNTH
#define	USEMNTINFO
#define	USERE_COMP
#define	USESETENV
#define	USEMKTIME
#define	USEINETATON
#include <sys/param.h>
# if	!defined (BSD) || (BSD < 199306)
# define	USEFFSIZE
# endif
#endif

#if	defined (__BOW__) \
|| (defined (__386BSD__) && defined (__BSD_NET2) && !defined (OSTYPE))
#define	BSD44
#define	OSTYPE			"BOW"
#define	TARUSESPACE
#define	TERMCAPLIB		"-ltermcap"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USEMOUNTH
#define	USEMNTINFO
# ifdef	__FreeBSD__
#  if	__FreeBSD__ < 3
#  define	USEVFCNAME
#  else
#  define	USEFFSTYPE
#  endif
# endif	/* __FreeBSD__ */
#define	USEREGCOMP
#define	USESETENV
#define	USEMKTIME
#define	SIGFNCINT
#define	USEINETATON
#endif

#if	defined (__OpenBSD__)
#define	BSD44
#define	OSTYPE			"OPENBSD"
#define	CODEEUC
#define	BSDINSTALL
#define	TARFROMPAX
#define	REGEXPLIB		"-lcompat"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USELEAPCNT
#define	USEMOUNTH
#define	USEMNTINFO
#define	USEFFSTYPE
#define	USERE_COMP
#define	USESETENV
#define	USEMKTIME
#define	SIGFNCINT
#define	USEINETATON
#endif

#if	defined (__APPLE__) && defined (__MACH__) && !defined (OSTYPE)
#define	BSD44
#define	OSTYPE			"DARWIN"	/* aka Mac OS X */
#define	USEMANLANG
#define	BSDINSTALL
#define	TARFROMPAX
#define	TERMCAPLIB		"-lcurses"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USELEAPCNT
#define	USEMOUNTH
#define	USEMNTINFO
#define	USEFFSTYPE
#define	USEREGCOMP
#define	USESETENV
#define	SELECTRWONLY
#define	USEMKTIME
#define	SIGFNCINT
#define	USESOCKLEN
#define	USEINETATON
#endif

#if	defined (__386BSD__) && !defined (OSTYPE)
#define	BSD43
#define	OSTYPE			"ORG_386BSD"
#define	TARUSESPACE
#define	USEPID_T
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USELEAPCNT
#define	USEMOUNTH
#define	USEMNTINFO
#define	USESETENV
#define	USEMKTIME
#define	USEWAITPID
#define	USESIGPMASK
#define	USERESOURCEH
#endif

#if	defined (mips) && !defined (OSTYPE)
#define	BSD43
#define	OSTYPE			"MIPS"
#define	CODEEUC
# if	defined (SYSTYPE_SYSV)
# undef	CCCOMMAND
# define	CCCOMMAND	"/bsd43/bin/cc"
# endif
#define	TERMCAPLIB		"-lcurses -ltermcap"
#define	NOERRNO
#define	NOFILEMODE
#define	NOUNISTDH
#define	NOSTDLIBH
#define	USEDIRECT
#define	USERE_COMP
#define	USESETENV
#define	USEGETWD
#define	USERESOURCEH
#define	SIGARGINT
#endif

/****************************************************************
 *	If your machine is not described above,	you should	*
 *	comment out below manually to apply your environment.	*
 ****************************************************************/

/* #define SYSV		;OS type is System V Rel.3 */
/* #define SVR4		;OS type is System V Rel.4 */
/* #define BSD4		;OS type is BSD 4.2 */
/* #define BSD43	;OS type is BSD 4.3 */
/* #define BSD44	;OS type is BSD 4.4 */
/* #define POSIX	;OS type is based on POSIX */

/* #define OSTYPE	;OS type is one of the followings */
/*	SOLARIS		;newer Solalis 2.0 (Sun) */
/*	SUN_OS		;older SunOS 4.1 (Sun) */
/*	NEWS_OS6	;newer NEWS-OS R6.0 (SONY) */
/*	NEWS_OS4	;newer NEWS-OS R4.2 (SONY) */
/*	NEWS_OS3	;older NEWS-OS R3.3 (SONY) */
/*	IRIX		;IRIX (SGI) */
/*	HPUX		;HP-UX (HP) */
/*	EWSUXV		;EWS-UX/V (NEC) */
/*	UNIOSU		;UniOS-U (OMRON) */
/*	UNIOSB		;UniOS-B (OMRON) */
/*	LUNA88K		;Luna/Mach (OMRON) */
/*	DECOSF1V2	;older OSF/1 V2 (DEC) */
/*	DECOSF1V3	;newer OSF/1 V3 (DEC) */
/*	AIX		;AIX (IBM) */
/*	ULTRIX		;Ultrix (DEC) */
/*	AUX		;A/UX (Apple) */
/*	DGUX		;DG/UX AViiON (DG) */
/*	UXPM		;UXP/M (Fujitsu) */
/*	MIPS		;RISC/os (MIPS) */
/*	NEXTSTEP	;NEXTSTEP (NeXT) */
/*	LINUX		;Linux */
/*	JCCBSD		;4.4BSD-Lite (JCC) */
/*	FREEBSD		;FreeBSD */
/*	NETBSD		;NetBSD */
/*	BSDOS		;BSD/OS (BSDI) */
/*	BOW		;BSD on Windows (ASCII) */
/*	OPENBSD		;OpenBSD */
/*	DARWIN		;Darwin (Apple) */
/*	ORG_386BSD	;386BSD */

/* #define CODEEUC	;kanji code type is EUC */
/* #define NOMULTIKANJI	;no need to support multiple kanji codes */
/* #define PATHNOCASE	;pathname is case insensitive */
/* #define COMMNOCASE	;shell command name is case insensitive */
/* #define ENVNOCASE	;environment variable is case insensitive */
/* #define NOUID	;have not uid, gid */
/* #define NODIRLOOP	;any directory structure cannot loop */
/* #define NOSYMLINK	;have not symbolic link */
/* #define BSPATHDELIM	;path delimtor is backspace */
/* #define USECRNL	;use CR-NL as end of line */
/* #define CWDINPATH	;CWD is implicitly included in command path */
/* #define DOUBLESLASH	;pathname starting with // has some special mean */
/* #define USEMANLANG	;man(1) directory includes LANG environment value */
/* #define SUPPORTSJIS	;cc(1) or man(1) supports Shift JIS perfectly */
/* #define BSDINSTALL	;install(1) with option -c is valid like BSD */
/* #define BSDINSTCMD	;command name except "install" to install like BSD */
/* #define TARUSESPACE	;tar(1) uses space to devide file mode from UID */
/* #define TARFROMPAX	;tar(1) comes from pax(1) */
/* #define BUGGYMAKE	;make(1) assumes the same timestamp as earlier */
/* #define CPP7BIT	;cpp(1) cannot through 8bit */
/* #define CCCOMMAND	;fullpath of suitable cc(1) */
/* #define EXTENDCCOPT	;additional option on cc(1) */
/* #define CCOUTOPT	;option for output file name on cc(1) with -c */
/* #define CCLNKOPT	;option for output file name on cc(1) without -c */
/* #define USETERMINFO	;use terminfo library instead of termcap */
/* #define TERMCAPLIB	;library needed for termcap */
/* #define REGEXPLIB	;library needed for regular expression */
/* #define SOCKETLIB	;library needed for socket */
/* #define EXTENDLIB	;library needed for the other extended */

/* #define UNKNOWNFS	;use unsupported type FileSystem */

/* #define NOFILEMACRO	;cc(1) cannot evaluate __FILE__ macro */
/* #define NOFUNCMACRO	;cc(1) cannot evaluate __FUNCTION__ macro */
/* #define NOLINEMACRO	;cc(1) cannot evaluate __LINE__ macro */
/* #define FORCEDSTDC	;not defined __STDC__, but expect standard C */
/* #define STRICTSTDC	;cannot allow K&R type function declaration */
/* #define NOSTDC	;defined __STDC__, but expect traditional C */
/* #define NOCONST	;defined __STDC__, but cannot use 'const' qualifier */
/* #define NOVOID	;cannot use type 'void' */
/* #define NOLONGLONG	;cannot use type 'long long' */
/* #define HAVELONGLONG	;have type 'long long' */
/* #define NOUID_T	;uid_t, gid_t is not defined in <sys/types.h> */
/* #define USEPID_T	;pid_t is defined in <sys/types.h> as process ID */
/* #define DECLSIGLIST	;'sys_siglist[]' declared in <signal.h> */
/* #define NOSIGLIST	;have not 'sys_siglist[]' in library */
/* #define DECLERRLIST	;'sys_errlist[]' declared in <stdio.h> or <errno.h> */
/* #define PWNEEDERROR	;/lib/libPW.a needs the extern variable 'Error[]' */
/* #define NOERRNO	;'errno' not declared in <errno.h> */
/* #define NOFILEMODE	;'S_I?{USR|GRP|OTH}' not defined in <sys/stat.h> */
/* #define NOUNISTDH	;have not <unistd.h> */
/* #define NOSTDLIBH	;have not <stdlib.h> */
/* #define NOTZFILEH	;have not <tzfile.h> */
/* #define USELEAPCNT	;'struct tzhead' has 'tzh_leapcnt' as leap second */
/* #define USESELECTH	;use <sys/select.h> for select() */
/* #define USESYSDIRH	;use <sys/dir.h> for DEV_BSIZE */
/* #define USETIMEH	;use <time.h> for 'struct tm' */
/* #define USESTDARGH	;use <stdarg.h> for va_list */
/* #define USEMKDEVH	;use <sys/mkdev.h> for major()/minor() */
/* #define USEMKNODH	;use <sys/mknod.h> for major()/minor() */
/* #define USELOCKFH	;use <sys/lockf.h> for lockf() */
/* #define USESGTTY	;use sgtty interface */
/* #define USETERMIO	;use termio interface */
/* #define USETERMIOS	;use termios interface */
/* #define HAVECLINE	;'struct termios' has 'c_line' */
/* #define USEDEVPTMX	;use /dev/ptmx and /dev/pts/? for pseudo terminal */
/* #define USEDIRECT	;use 'struct direct' instead of dirent */
/* #define SYSVDIRENT	;dirent interface behaves as System V */
/* #define NODNAMLEN	;'struct dirent' hasn't 'd_namlen' */
/* #define NODRECLEN	;'struct dirent' hasn't 'd_reclen' but 'd_fd' */
/* #define DNAMESIZE	;size of 'd_name' in 'struct dirent' */
/* #define HAVETIMEZONE	;have extern variable 'timezone' */
/* #define NOTMGMTOFF	;'struct tm' hasn't 'tm_gmtoff' */
/* #define USEINSYSTMH	;use <netinet/in_systm.h> for <netinet/ip.h> */
/* #define NOHADDRLIST	;'struct hostent' hasn't 'h_addr_list' */

/* following 5 items are exclusive */
/* #define USESTATVFSH	;use <sys/statvfs.h> as header of the FS status */
/* #define USESTATFSH	;use <sys/statfs.h> as header of the FS status */
/* #define USEVFSH	;use <sys/vfs.h> as header of the FS status */
/* #define USEMOUNTH	;use <sys/mount.h> as header of the FS status */
/* #define USEFSDATA	;use 'struct fs_data' as structure of hte FS status */

/* #define USESTATVFS_T	;use 'statvfs_t' instead of 'struct statvfs' */
/* #define USEFFSIZE	;'struct statfs' has 'f_fsize' as block size */
/* #define STATFSARGS	;the number of arguments in statfs() */

/* following 9 items are exclusive */
/* #define USEMNTENTH	;use <mntent.h> as header of the mount entry */
/* #define USEMNTTABH	;use <sys/mnttab.h> as header of the mount entry */
/* #define USEGETFSSTAT	;use getfsstat() to get the mount entry */
/* #define USEGETVFSTAT	;use getvfsstat() to get the mount entry */
/* #define USEMNTCTL	;use mntctl() to get the mount entry */
/* #define USEMNTINFOR	;use getmntinfo_r() to get the mount entry */
/* #define USEMNTINFO	;use getmntinfo() to get the mount entry */
/* #define USEGETMNT	;use getmnt() to get the mount entry */
/* #define USEGETFSENT	;use getfsent() to get the mount entry */

/* following 2 items are exclusive */
/* #define USEVFCNAME	;'struct vfsconf' has 'vfc_name' as the mount type */
/* #define USEFFSTYPE	;'struct statfs' has 'f_fstypename' as mount type */

/* following 3 items are exclusive */
/* #define USERE_COMP	;use re_comp() family as search */
/* #define USEREGCOMP	;use regcomp() family as search */
/* #define USEREGCMP	;use regcmp() family as search */

/* #define USERAND48	;use rand48() family instead of random() */
/* #define USESETENV	;use setenv() instead of putenv() */
/* #define NOSELECT	;have not select() */
/* #define DEFFDSETSIZE	;pre-define FD_SETSIZE as OPEN_MAX for select() */
/* #define SELECTRWONLY	;select() cannot respond with the file opened as r/w */
/* #define NOVSPRINTF	;have not vsprintf() */
/* #define NOTERMVAR	;have not termcap variables such as PC, ospeed, etc. */
/* #define USEUTIME	;use utime() instead of utimes() */
/* #define USEGETWD	;use getwd() instead of getcwd() */
/* #define USEFCNTLOCK	;use fcntl() lock instead of flock() */
/* #define USELOCKF	;use lockf() instead of flock() */
/* #define NOFLOCK	;have neither flock() nor lockf() */
/* #define NOSYSLOG	;have not syslog() */
/* #define USETIMELOCAL	;have timelocal() as inverse of localtime() */
/* #define USEMKTIME	;use mktime() instead of timelocal() */
/* #define USESYSCONF	;use sysconf() for getting system configuration */
/* #define USELLSEEK	;use _llseek() for 64bits width instead of lseek() */
/* #define USEUNAME	;use uname() instead of gethostname() */
/* #define USEWAITPID	;use waitpid() instead of wait3() */
/* #define USESIGACTION	;use sigaction() instead of signal() */
/* #define USESIGPMASK	;use sigprocmask() instead of sigsetmask() */
/* #define USERESOURCEH	;use <sys/resource.h> for resource info. */
/* #define USEULIMITH	;use <ulimit.h> for resource info. */
/* #define USEGETRUSAGE	;use getrusage() for getting process time */
/* #define USETIMES	;use times() for getting process time */
/* #define GETPGRPVOID	;getpgrp() needs void argument */
/* #define USESETPGID	;use setpgid() instead of setpgrp() */
/* #define USESETVBUF	;use setvbuf() instead of setbuf() or setlinebuf() */
/* #define SIGARGINT	;the 2nd argument function of signal() returns int */
/* #define SIGFNCINT	;the 2nd argument function of signal() needs int */
/* #define USESTRERROR	;use strerror() instead of sys_errlist[] */
/* #define GETTODARGS	;the number of arguments in gettimeofday() */
/* #define USESETSID	;use setsid() to set session ID */
/* #define USEMMAP	;use mmap() to map files into memory */
/* #define USESOCKLEN	;use socklen_t for bind()/connect()/accept() */
/* #define USEINETATON	;use inet_aton() instead of inet_addr() */
/* #define USEINETPTON	;use inet_pton() instead of inet_addr() */
/* #define USESETREUID	;use setreuid() to set effective user ID */
/* #define USESETRESUID	;use setresuid() to set effective user ID */
/* #define USESETREGID	;use setregid() to set effective group ID */
/* #define USESETRESGID	;use setresgid() to set effective group ID */
/* #define USEGETGROUPS	;use getgroups() to get group access list */

/* #define SENSEPERSEC	;ratio of key sense per 1 second */
/* #define WAITKEYPAD	;interval to wait after getting input of ESC [ms] */
/* #define WAITKANJI	;interval to wait after getting input of Kanji [ms] */


/*                             */
/* DO NOT DELETE or EDIT BELOW */
/*                             */

#ifdef	BSPATHDELIM
#define	_SC_			'\\'
#define	_SS_			"\\"
#else
#define	_SC_			'/'
#define	_SS_			"/"
#endif

#if	defined (BSD43) || defined (BSD44)
#define	BSD4
#endif

#if	defined (BSD44) || defined (SVR4)
#define	POSIX
#define	DEFFDSETSIZE
#endif

#ifdef	POSIX
# if	!defined (USESGTTY) && !defined (USETERMIO) && !defined (USETERMIOS)
# define	USETERMIOS
# endif
#define	USEPID_T
#define	USESYSCONF
#define	USEWAITPID
#define	USESIGACTION
#define	USESIGPMASK
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#define	USESETVBUF
#define	USESTRERROR
#define	USESETSID
#define	USEMMAP
#endif

#ifdef	SVR4
#define	SYSV
#define	USESOCKLEN
#endif

#ifdef	SYSV
#define	TARUSESPACE
# if	!defined (USESGTTY) && !defined (USETERMIO) && !defined (USETERMIOS)
# define	USETERMIO
# endif
#define	SYSVDIRENT
#define	HAVETIMEZONE
#define	USERAND48
#define	USEUNAME
# if	!defined (USERE_COMP) && !defined (USEREGCOMP)
# define	USEREGCMP
# endif
#endif

#ifndef	BSD4
#define	USEDEVPTMX
#endif

#if	defined (POSIX) || defined (SYSV)
#define	USEFCNTLOCK
#endif

#if	defined (BSD4)
#define	USEINSYSTMH
#endif

#if	defined (BSD4) && !defined (BSD43) && !defined (BSD44)
#define	NOHADDRLIST
#endif

#if	defined (POSIX) || defined (BSD4)
#define	USEGETGROUPS
#endif

#if	!defined (__GNUC__) && !defined (__attribute__)
#define	__attribute__(x)
#endif

/*                                 */
/* Eval configurations by Configur */
/*                                 */

#include "config.h"

#ifdef	USETERMIOS
#undef	USETERMIO
#endif
#if	defined (USETERMIOS) || defined (USETERMIO)
#undef	USESGTTY
#endif
#if	!defined (USETERMIOS) && !defined (USETERMIO) && !defined (USESGTTY)
#define	USESGTTY
#endif

#if	defined (__STDC__) && !defined (NOSTDC) && !defined (FORCEDSTDC)
#define	FORCEDSTDC
#endif

#if	defined (__STRICT_ANSI__) && !defined (STRICTSTDC)
#define	STRICTSTDC
#endif

#if	defined (FORCEDSTDC) && !defined (USESTDARGH)
#define	USESTDARGH
#endif

#if	defined (__STDC__) && !defined (NOSTDC) \
&& !defined (NOLONGLONG) && !defined (HAVELONGLONG)
#define	HAVELONGLONG
#endif

#if	defined (FORCEDSTDC) && !defined (STRICTSTDC)
#define	__P_(args)		args
#else
#define	__P_(args)		()
#endif

#if	defined (FORCEDSTDC) && !defined (NOCONST)
#define	CONST			const
#else
#define	CONST
#endif

#ifdef	FORCEDSTDC
#define	ALLOC_T			size_t
#define	VOID_A			void
#else
#define	ALLOC_T			u_int
#define	VOID_A
#endif

#ifdef	NOVOID
#define	VOID
#define	VOID_T			int
#define	VOID_P			char *
#define	VOID_C
#else
#define	VOID			void
#define	VOID_T			void
#define	VOID_P			void *
#define	VOID_C			(void)
#endif

#if	defined (USEREGCMP) && !defined (REGEXPLIB)
#define	REGEXPLIB		"-lPW"
#endif

#if	defined (BSDINSTALL) && !defined (BSDINSTCMD)
#define	BSDINSTCMD		"install"
#endif

#ifndef	OSTYPE
#define	OSTYPE			"UNKNOWN"
#endif
#ifndef	CCCOMMAND
#define	CCCOMMAND		"cc"
#endif
#ifndef	EXTENDCCOPT
#define	EXTENDCCOPT		"-O"
#endif
#ifndef	TERMCAPLIB
# ifdef	USETERMINFO
# define	TERMCAPLIB	"-lcurses"
# else
# define	TERMCAPLIB	"-ltermlib"
# endif
#endif
#ifndef	REGEXPLIB
#define	REGEXPLIB		""
#endif
#ifndef	SOCKETLIB
#define	SOCKETLIB		""
#endif
#ifndef	EXTENDLIB
#define	EXTENDLIB		""
#endif
#ifndef	DNAMESIZE
#define	DNAMESIZE		(MAXNAMLEN + 1)
#endif

#if	defined (USESTATVFSH)
#undef	USESTATFSH
#endif

#if	defined (USESTATVFSH) || defined (USESTATFSH)
#undef	USEMOUNTH
#endif

#if	defined (USESTATVFSH) || defined (USESTATFSH) || defined (USEMOUNTH)
#undef	USEFSDATA
#endif

#if	defined (USESTATVFSH) || defined (USESTATFSH) \
|| defined (USEMOUNTH) || defined (USEFSDATA)
#undef	USEVFSH
#endif

#if	!defined (USESTATVFSH) && !defined (USESTATFSH) \
&& !defined (USEMOUNTH) && !defined (USEFSDATA) && !defined (USEVFSH)
#define	USEVFSH
#endif

#if	defined (USESTATFSH) || defined (USEVFSH) || defined (USEMOUNTH)
# ifndef	STATFSARGS
# define	STATFSARGS	2
# endif
#endif


#if	defined (USEMNTTABH)
#undef	USEGETFSSTAT
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT)
#undef	USEGETVFSTAT
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) \
|| defined (USEGETVFSTAT)
#undef	USEMNTCTL
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) \
|| defined (USEGETVFSTAT) || defined (USEMNTCTL)
#undef	USEMNTINFOR
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) \
|| defined (USEGETVFSTAT) || defined (USEMNTCTL) || defined (USEMNTINFOR)
#undef	USEMNTINFO
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) \
|| defined (USEGETVFSTAT) || defined (USEMNTCTL) || defined (USEMNTINFOR) \
|| defined (USEMNTINFO)
#undef	USEGETMNT
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) \
|| defined (USEGETVFSTAT) || defined (USEMNTCTL) || defined (USEMNTINFOR) \
|| defined (USEMNTINFO) || defined (USEGETMNT)
#undef	USEGETFSENT
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) \
|| defined (USEGETVFSTAT) || defined (USEMNTCTL) || defined (USEMNTINFOR) \
|| defined (USEMNTINFO) || defined (USEGETMNT) || defined (USEGETFSENT)
#undef	USEMNTENTH
#endif

#if	!defined (USEMNTTABH) && !defined (USEGETFSSTAT) \
&& !defined (USEGETVFSTAT) && !defined (USEMNTCTL) && !defined (USEMNTINFOR) \
&& !defined (USEMNTINFO) && !defined (USEGETMNT) && !defined (USEGETFSENT) \
&& !defined (USEMNTENTH)
#define	USEMNTENTH
#endif

#if	MSDOS
#undef	USEVFSH
#undef	USEMNTENTH
#endif


#if	!defined (USEGETRUSAGE) && !defined (USETIMES) \
&& defined (USERESOURCEH)
#define	USEGETRUSAGE
#endif

#ifndef	GETTODARGS
#define	GETTODARGS		2
#endif

#endif	/* !__MACHINE_H_ */
