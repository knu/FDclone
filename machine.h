/*
 *	machine.h
 *
 *	Definition of Machine Depended Identifier
 */

#ifndef	__MACHINE_H_
#define	__MACHINE_H_

#ifdef	MSDOS
#undef	MSDOS
#endif
#define	MSDOS		(DOSV || PC98 || J3100)

#if	MSDOS
#define	NOTZFILEH
#define	USETIMEH
#define	USEUTIME
#define	USEMKTIME
#define	SENSEPERSEC	20
# ifdef	__GNUC__
# define	SIGFNCINT
#  ifndef	DJGPP
#  define	DJGPP	1
#  else
#  define	USEREGCOMP
#  endif
# else	/* !__GNUC__ */
# define	NOUID_T
# define	NOFILEMODE
# define	NOUNISTDH
# endif	/* !__GNUC__ */
# if	!defined (__GNUC__) || (DJGPP >= 2)
typedef unsigned int	u_int;
typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned long	u_long;
# endif
# ifdef	__TURBOC__
# define	FORCEDSTDC
# define	SIGFNCINT
typedef long	off_t;
# endif
#define	_SC_	'\\'
#define	_SS_	"\\"
#else	/* !MSDOS */
#define	_SC_	'/'
#define	_SS_	"/"
#endif	/* !MSDOS */

#if	defined (sun)
#define	CODEEUC
#define	CPP7BIT
#define	USELEAPCNT
#define	USERESOURCEH
#include <sys/param.h>
# if	!defined (MAXHOSTNAMELEN)
# define	SVR4
# define	OSTYPE		"SOLARIS"
# include <stdio.h>
#  if	defined (__SUNPRO_C) && defined (BSD)
#  define	USEDIRECT
#  define	USERE_COMP
#  endif
# define	USEMANLANG
# define	REGEXPLIB	"-lgen"
# define	NODNAMLEN
# define	NOTMGMTOFF
# define	USESTATVFSH
# define	USEMNTTABH
# define	USEMKTIME
# define	SIGFNCINT
# else
# define	BSD43
# define	OSTYPE		"SUN_OS"
# define	BSDINSTALL
# define	USERE_COMP
# define	USETIMELOCAL
# endif
#endif

#if	defined (sony)
#define	USELEAPCNT
#define	USEMANLANG
# if	defined (USGr4) || defined (SYSTYPE_SYSV)
# define	SVR4
# define	OSTYPE		"NEWS_OS6"
# define	REGEXPLIB	"-lgen"
# define	NODNAMLEN
# define	NOTMGMTOFF
# define	USESTATVFSH
# define	USEMNTTABH
# define	USEMKTIME
# define	USEUTIME
# define	USERESOURCEH
# define	SIGFNCINT
# define	GETTODARGS	1
# else
# define	BSD43
# define	BSDINSTALL
# define	USERE_COMP
# define	SIGARGINT
#  if	defined (__sony) || !defined (bsd43)
#  define	OSTYPE		"NEWS_OS4"
#  define	USESETENV
#  define	USERESOURCEH
#  else
#  define	OSTYPE		"NEWS_OS3"
#  define	NOERRNO
#  define	NOFILEMODE
#  define	NOUNISTDH
#  define	NOSTDLIBH
#  define	USEDIRECT
#  define	NOTMGMTOFF
#  define	NOVSPRINTF
#  define	USEGETWD
#  define	USETIMESH
#  endif
# endif
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
#define	STRICTSTDC
#define	NODNAMLEN
#define	NOTMGMTOFF
#define	USESTATFSH
#define	STATFSARGS	4
#define	USERE_COMP
#define	USEMKTIME
#define	USERESOURCEH
#endif

#if	defined (hpux) || defined (__hpux) \
|| defined (__H3050) || defined (__H3050R) || defined (__H3050RX)
#define	SVR4
#define	OSTYPE			"HPUX"
#define	EXTENDCCOPT		""
#define	TERMCAPLIB		"-lcurses"
#define	STRICTSTDC
#define	NOTZFILEH
#define	NOTMGMTOFF
#define	USEREGCOMP
#define	USEUTIME
#define	USEMKTIME
#define	USERESOURCEH
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
# define	NODNAMLEN
# define	NOTMGMTOFF
# define	USESTATVFSH
# define	USEMNTTABH
# define	USEUTIME
# define	SIGFNCINT
# define	USEMKTIME
# else
# define	SYSV
# endif
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
#define	WAITKEYPAD		720
#define	WAITMETA		720
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
#define	SIGARGINT
#endif

#if	(defined (__alpha) || defined (alpha)) \
&& !defined (linux) && !defined (__FreeBSD__) && !defined (__NetBSD__)
#define	CODEEUC
#define	TARUSESPACE
#define	EXTENDLIB	"-lc_r"
#define	USEMNTINFOR
# if	defined (SYSTYPE_BSD)
# define	BSD43
# define	OSTYPE		"DECOSF1V2"
# define	BSDINSTALL
# define	BSDINSTCMD	"installbsd"
# define	USEMOUNTH
# define	STATFSARGS	3
# define	USERE_COMP
# else
# define	SVR4
# define	OSTYPE		"DECOSF1V3"
# define	NODNAMLEN
# define	USESTATVFSH
# define	USEREGCOMP
# endif
#endif

#if	defined (_IBMR2)
#define	SVR4
#define	OSTYPE			"AIX"
#define	TERMCAPLIB		"-lcurses"
#define	NOTZFILEH
#define	USESELECTH
#define	USESYSDIRH
#define	USETIMEH
#define	USETERMIO
#define	NOTMGMTOFF
#define	USESTATFSH
#define	STATFSARGS	4
#define	USEMNTCTL
#define	USERE_COMP
# if	defined (_AIX41)
# define	USEMKTIME
# define	SIGFNCINT
# else
# define	SIGARGINT
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
#define	STATFSARGS	4
#define	USESYSDIRH
#define	NODNAMLEN
#define	USERE_COMP
#endif

#if	defined (__uxpm__)
#define	SVR4
#define	OSTYPE			"UXPM"
#define	CODEEUC
#define	CCOUTOPT		"-o $*"
#define	REGEXPLIB		"-lgen"
#define	NODNAMLEN
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
#define	NODNAMLEN
#define	NOTMGMTOFF
#define	USESTATVFSH
#define	USEMNTTABH
#define	USEUTIME
#define	USEMKTIME
#endif

#if	defined (mips) && !defined (OSTYPE)
#define	BSD43
#define	OSTYPE			"MIPS"
#define	CODEEUC
# if	defined (SYSTYPE_SYSV)
# undef	CCCOMMAND
# define	CCCOMMAND	"/bsd43/bin/cc"
# endif
#define	TERMCAPLIB	"-lcurses -ltermcap"
#define	NOERRNO
#define	NOFILEMODE
#define	NOUNISTDH
#define	NOSTDLIBH
#define	USEDIRECT
#define	USERE_COMP
#define	USESETENV
#define	USEGETWD
#define	SIGARGINT
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
#endif

#if	defined (linux)
#define	OSTYPE			"LINUX"
#define	CODEEUC
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
# define	TERMCAPLIB		"-ltermcap"
# else
# define	TERMCAPLIB		"-lncurses"
# endif
#define	DECLSIGLIST
#define	DECLERRLIST
#define	NOTZFILEH
#define	USETERMIOS
#define	SYSVDIRENT
#define	HAVETIMEZONE
#define	NOTMGMTOFF
#define	USEREGCOMP
#define	USESETENV
#define	USEMKTIME
#define	USEWAITPID
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#define	SIGFNCINT
#endif

#if	defined (__FreeBSD__) && defined (__powerpc__)
#define	BSD43
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
#define	USEWAITPID
#define	USERESOURCEH
#define	SIGFNCINT
#endif

#if	defined (__FreeBSD__) && !defined (OSTYPE) && !defined (__BOW__)
#define	BSD43
#define	OSTYPE			"FREEBSD"
#define	CODEEUC
#define	USEMANLANG
#define	BSDINSTALL
#define	TARUSESPACE
#define	REGEXPLIB		"-lcompat"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	NOTZFILEH
#define	USETERMIOS
#define	USEMOUNTH
#define	USEMNTINFO
# if	__FreeBSD__ < 3
#define	USEVFCNAME
# else
#define	USEFFSTYPE
# endif
#define	USERE_COMP
#define	USESETENV
#define	USEMKTIME
#define	USEWAITPID
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#define	SIGFNCINT
#endif

#if	defined (__NetBSD__)
#define	BSD43
#define	OSTYPE			"NETBSD"
#define	CODEEUC
#define	BSDINSTALL
#define	TARUSESPACE
#define	REGEXPLIB		"-lcompat"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USELEAPCNT
#define	USETERMIOS
#define	USEMOUNTH
#define	USEMNTINFO
#define	USEFFSTYPE
#define	USERE_COMP
#define	USESETENV
#define	USEMKTIME
#define	USEWAITPID
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#define	SIGFNCINT
#include <sys/param.h>
# if	defined (NetBSD1_0) && (NetBSD1_0 < 1)
# define	USEFFSIZE
# endif
#endif

#if	defined (__bsdi__)
#define	BSD43
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
#define	USEWAITPID
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#include <sys/param.h>
# if	!defined (BSD) || (BSD < 199306)
# define	USEFFSIZE
# endif
#endif

#if	defined (__BOW__) \
|| (defined (__386BSD__) && defined (__BSD_NET2) && !defined (OSTYPE))
#define	BSD43
#define	OSTYPE			"BOW"
#define	TARUSESPACE
#define	TERMCAPLIB		"-ltermcap"
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USEMOUNTH
#define	USEMNTINFO
#define	USEVFCNAME
#define	USEREGCOMP
#define	USESETENV
#define	USEMKTIME
#define	USEWAITPID
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#define	SIGFNCINT
#endif

#if	defined (__386BSD__) && !defined (OSTYPE)
#define	BSD43
#define	OSTYPE			"ORG_386BSD"
#define	TARUSESPACE
#define	DECLSIGLIST
#define	DECLERRLIST
#define	USELEAPCNT
#define	USEMOUNTH
#define	USEMNTINFO
#define	USESETENV
#define	USEMKTIME
#define	USEWAITPID
#define	USERESOURCEH
#define	GETPGRPVOID
#define	USESETPGID
#endif

/****************************************************************
 *	If your machine is not described above,	you should	*
 *	comment out below manually to apply your environment.	*
 ****************************************************************/

/* #define SYSV		;OS type is System V older Rel.3 */
/* #define SVR4		;OS type is System V Rel.4 */
/* #define BSD4		;OS type is older BSD 4.2 */
/* #define BSD43	;OS type is newer BSD 4.3 */

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
/*	386BSD_ORG	;386BSD */

/* #define CODEEUC	;kanji code type is EUC */
/* #define USEMANLANG	;man(1) directory includes LANG environment value */
/* #define BSDINSTALL	;install(1) with option -c is valid like BSD */
/* #define BSDINSTCMD	;command name except "install" to install like BSD */
/* #define TARUSESPACE	;tar(1) uses space to devide file mode from UID */
/* #define CPP7BIT	;cpp(1) cannot through 8bit */
/* #define CCCOMMAND	;fullpath of suitable cc(1) */
/* #define EXTENDCCOPT	;additional option on cc(1) */
/* #define CCOUTOPT	;option for output file name on cc(1) with -c */
/* #define CCLNKOPT	;option for output file name on cc(1) without -c */
/* #define TERMCAPLIB	;library needed for termcap */
/* #define REGEXPLIB	;library needed for regular expression */
/* #define EXTENDLIB	;library needed for the other extended */

/* #define UNKNOWNFS	;use unsupported type FileSystem */

/* #define FORCEDSTDC	;not defined __STDC__, but expect standard C */
/* #define STRICTSTDC	;cannot allow K&R type function declaration */
/* #define NOVOID	;cannot use type 'void' */
/* #define NOUID_T	;uid_t, gid_t is not defined in <sys/types.h> */
/* #define DECLSIGLIST	;'sys_siglist[]' declared in <signal.h> */
/* #define NOSIGLIST	;have not 'sys_siglist[]' in library */
/* #define DECLERRLIST	;'sys_errlist[]' declared in <stdio.h> or <errno.h> */
/* #define PWNEEDERROR	;/lib/libPW.a needs the extern variable 'Error[]' */
/* #define NOERRNO	;'errno' not declared in <errno.h> */
/* #define NOFILEMODE	;'S_I?{USR|GRP|OTH}' not defined in <sys/stat.h> */
/* #define NOUNISTDH	;have not <unistd.h> */
/* #define NOSTDLIBH	;have not <stdlib.h> */
/* #define NOTZFILEH	;have not <tzfile.h> */
/* #define USELEAPCNT	;struct tzhead have tzh_leapcnt as leap second */
/* #define USESELECTH	;use <sys/select.h> for select() */
/* #define USESYSDIRH	;use <sys/dir.h> for DEV_BSIZE */
/* #define USETIMEH	;use <time.h> for 'struct tm' */
/* #define USETERMIO	;use termio interface */
/* #define USETERMIOS	;use termios interface */
/* #define USEDIRECT	;use 'struct direct' instead of dirent */
/* #define SYSVDIRENT	;dirent interface behaves as System V */
/* #define NODNAMLEN	;struct dirent haven't d_namlen */
/* #define HAVETIMEZONE	;have extern valiable 'timezone' */
/* #define NOTMGMTOFF	;struct tm haven't tm_gmtoff */

/* following 5 items are exclusive */
/* #define USESTATVFSH	;use <sys/statvfs.h> as header of the FS status */
/* #define USESTATFSH	;use <sys/statfs.h> as header of the FS status */
/* #define USEVFSH	;use <sys/vfs.h> as header of the FS status */
/* #define USEMOUNTH	;use <sys/mount.h> as header of the FS status */
/* #define USEFSDATA	;use 'struct fs_data' as structure of hte FS status */

/* #define USEFFSIZE	;'struct statfs' has 'f_fsize' as block size */
/* #define STATFSARGS	;the number of arguments in statfs() */

/* following 8 items are exclusive */
/* #define USEMNTENTH	;use <mntent.h> as header of the mount entry */
/* #define USEMNTTABH	;use <sys/mnttab.h> as header of the mount entry */
/* #define USEGETFSSTAT	;use getfsstat() to get the mount entry */
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
/* #define NOVSPRINTF	;have not vsprintf() */
/* #define USEUTIME	;use utime() instead of utimes() */
/* #define USEGETWD	;use getwd() instead of getcwd() */
/* #define USETIMELOCAL	;have timelocal() as inverse of localtime() */
/* #define USEMKTIME	;use mktime() instead of timelocal() */
/* #define USEUNAME	;use uname() instead of gethostname() */
/* #define USEWAITPID	;use waitpid() instead of wait3() */
/* #define USERESOURCEH	;use <sys/resource.h> for resource info. */
/* #define USEULIMITH	;use <ulimit.h> for resource info. */
/* #define USETIMESH	;use <sys/times.h> for resource info. */
/* #define GETPGRPVOID	;getpgrp() needs void argument */
/* #define USESETPGID	;use setpgid() instead of setpgrp() */
/* #define SIGARGINT	;the 2nd argument function of signal() returns int */
/* #define SIGFNCINT	;the 2nd argument function of signal() needs int */
/* #define GETTODARGS	;the number of arguments in gettimeofday() */

/* #define SENSEPERSEC	;ratio of key sense per 1 second */
/* #define WAITKEYPAD	;interval to wait after getting input of ESC [ms] */
/* #define WAITMETA	;interval to wait after getting input of META [ms] */

#include "config.h"


/*                             */
/* DO NOT DELETE or EDIT BELOW */
/*                             */

#if	defined (SVR4) || defined (SYSV)
#define	TARUSESPACE
# if	!defined (USETERMIOS) && !defined (USETERMIO)
#  ifdef	SVR4
#  define	USETERMIOS
#  else
#  define	USETERMIO
#  endif
# endif
#define	SYSVDIRENT
#define	HAVETIMEZONE
#define	USERAND48
#define	USEUNAME
# if	!defined (USERE_COMP) && !defined (USEREGCOMP)
# define	USEREGCMP
# endif
#endif

#ifdef	SVR4
#define	GETPGRPVOID
#define	USESETPGID
#endif

#if	(defined (__STDC__) || defined (FORCEDSTDC)) \
&& !defined (__STRICT_ANSI__) && !defined (STRICTSTDC)
#define	__P_(args)	args
#define	CONST		const
#define	ALLOC_T		size_t
#define	VOID_A		void
#else
#define	__P_(args)	()
#define	CONST
#define	ALLOC_T		unsigned
#define	VOID_A
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
#define	TERMCAPLIB		"-ltermlib"
#endif
#ifndef	REGEXPLIB
#define	REGEXPLIB		""
#endif
#ifndef	EXTENDLIB
#define	EXTENDLIB		""
#endif

#if	defined (USESTATVFSH)
# ifdef	USESTATFSH
# undef	USESTATFSH
# endif
#endif

#if	defined (USESTATVFSH) || defined (USESTATFSH)
# ifdef	USEMOUNTH
# undef	USEMOUNTH
# endif
#endif

#if	defined (USESTATVFSH) || defined (USESTATFSH) || defined (USEMOUNTH)
# ifdef	USEFSDATA
# undef	USEFSDATA
# endif
#endif

#if	defined (USESTATVFSH) || defined (USESTATFSH) || defined (USEMOUNTH) \
|| defined (USEFSDATA)
# ifdef	USEVFSH
# undef	USEVFSH
# endif
#else
#define	USEVFSH
#endif

#if	defined (USESTATFSH) || defined (USEVFSH) || defined (USEMOUNTH)
# ifndef	STATFSARGS
# define	STATFSARGS	2
# endif
#endif


#if	defined (USEMNTTABH)
# ifdef	USEGETFSSTAT
# undef	USEGETFSSTAT
# endif
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT)
# ifdef	USEMNTCTL
# undef	USEMNTCTL
# endif
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) || defined (USEMNTCTL)
# ifdef	USEMNTINFOR
# undef	USEMNTINFOR
# endif
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) || defined (USEMNTCTL) \
|| defined (USEMNTINFOR)
# ifdef	USEMNTINFO
# undef	USEMNTINFO
# endif
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) || defined (USEMNTCTL) \
|| defined (USEMNTINFOR) || defined (USEMNTINFO)
# ifdef	USEGETMNT
# undef	USEGETMNT
# endif
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) || defined (USEMNTCTL) \
|| defined (USEMNTINFOR) || defined (USEMNTINFO) || defined (USEGETMNT)
# ifdef	USEGETFSENT
# undef	USEGETFSENT
# endif
#endif

#if	defined (USEMNTTABH) || defined (USEGETFSSTAT) || defined (USEMNTCTL) \
|| defined (USEMNTINFOR) || defined (USEMNTINFO) || defined (USEGETMNT) \
|| defined (USEGETFSENT)
# ifdef	USEMNTENTH
# undef	USEMNTENTH
# endif
#else
#define	USEMNTENTH
#endif

#ifndef	GETTODARGS
#define	GETTODARGS	2
#endif

#endif	/* __MACHINE_H_ */
