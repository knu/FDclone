/*
 *	Definition of Machine Depended Identifier
 */

#if defined (__GNUC__)
#define	EXTENDCCOPT		"-O -fsigned-char"
#endif

#if defined (sun)
#define	CODEEUC
#define	CPP7BIT
# if defined (USGr4) || defined (__svr4__) || defined (__SVR4)
# define	SVR4
# define	OSTYPE		"SOLARIS"
#  if defined (__SUNPRO_C)
#  undef	CCCOMMAND
#  define	CCCOMMAND	"acc"
# endif
# define	REGEXPLIB	"-lgen"
# define	USESTATVFSH
# define	USEMNTTABH
# define	MOUNTED		MNTTAB
# else
# define	BSD43
# define	OSTYPE		"SUN_OS"
# define	USERE_COMP
# endif
#endif

#if defined (sony)
#define	USELEAPCNT
# if defined (USGr4) || defined (SYSTYPE_SYSV)
# define	SVR4
# define	OSTYPE		"NEWS_OS6"
# define	REGEXPLIB	"-lgen"
# define	USESTATVFSH
# define	USEMNTTABH
# define	MOUNTED		MNTTAB
# define	USEUTIME
# else
# define	BSD43
# define	USESYSDIRH
# define	USERE_COMP
# define	SIGARGINT
#  if defined (__sony)
#  define	OSTYPE		"NEWS_OS4"
#  define	USESETENV
#  else
#   define	NOSTRSTR
#   if !defined (bsd43)
#   define	OSTYPE		"NEWS_OS41"
#   define	USESETENV
#   else
#   define	OSTYPE		"NEWS_OS3"
#   define	NOVOID
#   define	NOERRNO
#   define	NOFILEMODE
#   define	NOUNISTDH
#   define	NOSTDLIBH
#   define	USEDIRECT
#   define	USEGETWD
#   endif
#  endif
# endif
#endif

#if defined (sgi)
#define	SYSV
#define	OSTYPE			"IRIX"
#define	CODEEUC
# if defined (_COMPILER_VERSION) && (_COMPILER_VERSION >= 600)
# define	EXTENDCCOPT	"-32 -O -signed"
# else
# define	EXTENDCCOPT	"-O -signed"
# endif
# if !defined (SYSTYPE_SVR4)
# define	TERMCAPLIB	"-lcurses"
# define	EXTENDLIB	"-lsun"
# endif
#define	IRIXFS
#define	USESTATFSH
#define	STATFSARGS	4
#define	USERE_COMP
#endif

#if defined (hpux) || defined (__H3050) || defined (__H3050R) || defined (__H3050RX)
#define	SVR4
#define	OSTYPE			"HPUX"
#define	EXTENDCCOPT
#define	TERMCAPLIB		"-lcurses"
#define	USEREGCOMP
#define	USEUTIME
#endif

#if defined (nec_ews) || defined (_nec_ews)
#define	CODEEUC
# if defined (nec_ews_svr4) || defined (_nec_ews_svr4)
# define	SVR4
# define	OSTYPE		"EWSUXV"
# undef	CCCOMMAND
#  if defined (nec_ews)
#  define	CCCOMMAND	"/usr/necccs/bin/cc"
#  else
#  define	CCCOMMAND	"/usr/abiccs/bin/cc"
#  endif
# define	TERMCAPLIB	"-lcurses"
# define	REGEXPLIB	"-lgen"
# define	USESTATVFSH
# define	USEMNTTABH
# define	USEUTIME
# define	SIGARGINT
# else
# define	SYSV
# endif
#endif

#if defined (uniosu)
#define	BSD43
#define	OSTYPE			"UNIOS"
#define	CODEEUC
#define	EXTENDCCOPT		"-O -Zs"
#define	TERMCAPLIB		"-lcurses"
#define	UNKNOWNFS
#define	NOVOID
#define	NOUID_T
#define	NOFILEMODE
#define	NOSTDLIBH
#define	USETIMEH
#define	USETERMIO
#define	HAVETIMEZONE
#define	NOSELECT
#define	NOSTRSTR
#endif

#if defined (luna88k)
#define	BSD43
#define	OSTYPE			"LUNA88K"
#define	CODEEUC
#define	NOERRNO
#define	NOFILEMODE
#define	USEDIRECT
#define	USERE_COMP
#define	USESETENV
#define	NOSTRSTR
#define	USEGETWD
#endif

#if defined (__alpha) || defined (alpha)
#define	CODEEUC
#define	TARUSESPACE
# if defined (SYSTYPE_BSD)
# define	BSD43
# define	OSTYPE		"DECOSF1V2"
# define	EXTENDLIB	"-lsys5"
# define	USEMOUNTH
# define	USEFFSIZE
# define	STATFSARGS	3
# define	USEFSTABH
# define	USERE_COMP
# else
# define	SVR4
# define	OSTYPE		"DECOSF1V3"
# define	EXTENDLIB	"-lc_r"
# define	USESTATVFSH
# define	USEMNTINFOR
# define	USEREGCOMP
# endif
#endif

#if defined (_IBMR2)
#define	SVR4
#define	OSTYPE			"AIX"
#define	EXTENDCCOPT		"-O -qchars=signed"
#define	TERMCAPLIB		"-ltermcap"
#define	USESELECTH
#define	USESYSDIRH
#define	USETIMEH
#define	USESTATFSH
#define	STATFSARGS	4
#define	USEMNTCTL
#endif

#if defined (ultrix)
#define	BSD43
#define	OSTYPE			"ULTRIX"
#define	CODEEUC
#define	TARUSESPACE
#define	CPP7BIT
#define	USESYSDIRH
#define	USEMOUNTH
#define	USEFSDATA
#define	USEGETMNT
#define	USERE_COMP
#endif

#if defined (_AUX_SOURCE)
#define	SYSV
#define	OSTYPE			"AUX"
#define	CPP7BIT
#define	TERMCAPLIB		"-ltermcap"
#define	UNKNOWNFS		/* Because of the buggy (?) 'rename(2)' */
#define	PWNEEDERROR
#define	USETIMEH
#endif

#if defined (DGUX) || defined (__DGUX__)
#define	SYSV
#define	OSTYPE			"DGUX"
#define	CODEEUC
#define	TERMCAPLIB		"-ltermcap"
#define	USESTATFSH
#define	STATFSARGS	4
#define	SVR3FS
#endif

#if defined (__uxpm__)
#define	SVR4
#define	OSTYPE			"UXPM"
#define	CODEEUC
#define	REGEXPLIB		"-lgen"
#define	USESTATVFSH
#define	USEMNTTABH
#define	USEUTIME
#define	SIGARGINT
#endif

#if defined (mips) && !defined (OSTYPE)
#define	BSD43
#define	OSTYPE			"MIPS"
#define	CODEEUC
# if defined (SYSTYPE_SYSV)
# undef	CCCOMMAND
# define	CCCOMMAND	"/bsd43/bin/cc"
# endif
#define	EXTENDCCOPT	"-O -signed"
#define	TERMCAPLIB	"-lcurses -ltermcap"
#define	NOERRNO
#define	NOFILEMODE
#define	NOUNISTDH
#define	NOSTDLIBH
#define	USEDIRECT
#define	USESETENV
#define	NOSTRSTR
#define	USEGETWD
#endif

#if defined (linux)
#define	SVR4
#define	OSTYPE			"LINUX"
#define	CODEEUC
#define	TERMCAPLIB		"-lncurses"
#define	USERE_COMP
#endif

#if defined (__FreeBSD__)
#define	BSD43
#define	OSTYPE			"FREEBSD"
#define	CODEEUC
#define	TARUSESPACE
#define	REGEXPLIB		"-lcompat"
#define	DECLERRLIST
#define	USEMOUNTH
#define	USEFSTABH
#define	USERE_COMP
#endif

#if defined (__NetBSD__)
#define	BSD43
#define	OSTYPE			"NETBSD"
#define	DECLERRLIST
#define	USEMOUNTH
#define	USEFFSIZE
#define	USEFSTABH
#define	USERE_COMP
#endif

#if defined (__bsdi__)
#define	BSD43
#define	OSTYPE			"BSDOS"
#define	CODEEUC
#define	TARUSESPACE
#define	REGEXPLIB		"-lcompat"
#define	DECLERRLIST
#define	USEMOUNTH
#define	USEFSTABH
#define	USERE_COMP
#include <sys/param.h>
# if (BSD4_4 != 1)
# define	USEFFSIZE
# endif
#endif

/****************************************************************
 *	If your machine is not described above,	you should	*
 *	comment out below manually to apply your environment.	*
 ****************************************************************/

/* #define SYSV		/* OS type is System V older Rel.3 */
/* #define SVR4		/* OS type is System V Rel.4 */
/* #define BSD4		/* OS type is older BSD 4.2 */
/* #define BSD43	/* OS type is newer BSD 4.3 */

/* #define OSTYPE	/* OS type is one of the followings */
/*	SOLARIS		/* newer Solalis 2.0 (Sun) */
/*	SUN_OS		/* older SunOS 4.1 (Sun) */
/*	NEWS_OS6	/* newer NEWS-OS R6.0 (SONY) */
/*	NEWS_OS4	/* newer NEWS-OS R4.2 (SONY) */
/*	NEWS_OS41	/* NEWS-OS R4.1 (SONY) */
/*	NEWS_OS3	/* older NEWS-OS R3.3 (SONY) */
/*	IRIX		/* IRIX (SGI) */
/*	HPUX		/* HP-UX (HP) */
/*	EWSUXV		/* EWS-UX/V (NEC) */
/*	UNIOS		/* UniOS-U (OMRON) */
/*	LUNA88K		/* Luna/Mach (OMRON) */
/*	DECOSF1V2	/* older OSF/1 V2 (DEC) */
/*	DECOSF1V3	/* newer OSF/1 V3 (DEC) */
/*	AIX		/* AIX (IBM) */
/*	ULTRIX		/* Ultrix (DEC) */
/*	AUX		/* A/UX (Apple) */
/*	DGUX		/* DG/UX AViiON (DG) */
/*	UXPM		/* UXP/M (Fujitsu) */
/*	MIPS		/* RISC/os (MIPS) */
/*	LINUX		/* Linux */
/*	FREEBSD		/* FreeBSD */
/*	NETBSD		/* NetBSD */
/*	BSDOS		/* BSD/OS (BSDI) */

/* #define CODEEUC	/* kanji code type is EUC */
/* #define TARUSESPACE	/* tar(1) uses space to devide file mode from UID */
/* #define CPP7BIT	/* cpp(1) cannot through 8bit */
/* #define CCCOMMAND	/* fullpath of suitable cc(1) */
/* #define EXTENDCCOPT	/* additional option on cc(1) */
/* #define TERMCAPLIB	/* library needed for termcap */
/* #define REGEXPLIB	/* library needed for regular expression */
/* #define EXTENDLIB	/* library needed for the other extended */

/* following 3 items are exclusive
/* #define SVR3FS	/* use SVR3 type FileSystem */
/* #define IRIXFS	/* use IRIX type FileSystem */
/* #define UNKNOWNFS	/* use unsupported type FileSystem */

/* #define USELEAPCNT	/* TZFILE includes tzh_leapcnt as leap second */
/* #define NOVOID	/* cannot use type 'void' */
/* #define NOUID_T	/* uid_t, gid_t is not defined in <sys/types.h> */
/* #define DECLERRLIST	/* 'sys_errlist' already declared in <stdio.h> */
/* #define PWNEEDERROR	/* /lib/libPW.a needs the extern variable 'Error[]' */
/* #define NOERRNO	/* 'errno' not declared in <errno.h> */
/* #define NOFILEMODE	/* 'S_I?{USR|GRP|OTH}' not defined in <sys/stat.h> */
/* #define NOUNISTDH	/* have not <unistd.h> */
/* #define NOSTDLIBH	/* have not <stdlib.h> */
/* #define USESELECTH	/* use <sys/select.h> for select() */
/* #define USESYSDIRH	/* use <sys/dir.h> for DEV_BSIZE */
/* #define USETIMEH	/* use <time.h> for 'struct tm' */
/* #define USETERMIO	/* use termio interface */
/* #define USEDIRECT	/* use 'struct direct' instead of dirent */
/* #define SYSVDIRENT	/* dirent interface behaves as System V */
/* #define HAVETIMEZONE	/* have extern valiable 'timezone' */

/* following 5 items are exclusive
/* #define USESTATVFSH	/* use <sys/statvfs.h> as header of the FS status */
/* #define USESTATFSH	/* use <sys/statfs.h> as header of the FS status */
/* #define USEVFSH	/* use <sys/vfs.h> as header of the FS status */
/* #define USEMOUNTH	/* use <sys/mount.h> as header of the FS status */
/* #define USEFSDATA	/* use 'struct fs_data' as structure of hte FS status */

/* #define USEFFSIZE	/* 'struct statfs' has 'f_fsize' as block size */
/* #define STATFSARGS	/* the number of arguments in statfs() */

/* following 7 items are exclusive
/* #define USEMNTTABH	/* use <sys/mnttab.h> as header of the mount entry */
/* #define USEMNTCTL	/* use mntctl() to get the mount entry */
/* #define USEMNTENTH	/* use <mntent.h> as header of the mount entry */
/* #define USEMNTINFOR	/* use getmntinfo_r() to get the mount entry */
/* #define USEMNTINFO	/* use getmntinfo() to get the mount entry */
/* #define USEGETMNT	/* use getmnt() to get the mount entry */
/* #define USEFSTABH	/* use <fstab.h> as header of the mount entry */

/* #define MOUNTED	/* means '/etc/mtab' defined in <mntent.h> */

/* following 3 items are exclusive
/* #define USERE_COMP	/* use re_comp() family as search */
/* #define USEREGCOMP	/* use regcomp() family as search */
/* #define USEREGCMP	/* use regcmp() family as search */

/* #define USERAND48	/* use rand48() family instead of random() */
/* #define USESETENV	/* use setenv() instead of putenv() */
/* #define NOSELECT	/* have not select() */
/* #define NOSTRSTR	/* have not strstr() */
/* #define USEUTIME	/* use utime() instead of utimes() */
/* #define USEGETWD	/* use getwd() instead of getcwd() */
/* #define SIGARGINT	/* signal() needs the 2nd argument as int */

#include "config.h"


/*                             */
/* DO NOT DELETE or EDIT BELOW */
/*                             */

#if defined (SVR4) || defined (SYSV)
#define	TARUSESPACE
#define	USETERMIO
#define	SYSVDIRENT
#define	HAVETIMEZONE
#define	USERAND48
# if !defined (USERE_COMP) && !defined (USEREGCOMP)
# define	USEREGCMP
# endif
#endif

#if defined (USEREGCMP) && !defined (REGEXPLIB)
#define	REGEXPLIB		"-lPW"
#endif

#ifndef CCCOMMAND
#define	CCCOMMAND		"cc"
#endif
#ifndef EXTENDCCOPT
#define	EXTENDCCOPT		"-O"
#endif
#ifndef TERMCAPLIB
#define	TERMCAPLIB		"-ltermlib"
#endif
#ifndef REGEXPLIB
#define	REGEXPLIB
#endif
#ifndef EXTENDLIB
#define	EXTENDLIB
#endif

#if defined (USESTATVFSH)
# ifdef	USESTATFSH
# undef	USESTATFSH
# endif
#endif

#if defined (USESTATVFSH) || defined (USESTATFSH)
# ifdef	USEMOUNTH
# undef	USEMOUNTH
# endif
#endif

#if defined (USESTATVFSH) || defined (USESTATFSH) || defined (USEMOUNTH)
# ifdef	USEFSDATA
# undef	USEFSDATA
# endif
#endif

#if defined (USESTATVFSH) || defined (USESTATFSH) || defined (USEMOUNTH)\
|| defined (USEFSDATA)
# ifdef	USEVFSH
# undef	USEVFSH
# endif
#else
#define	USEVFSH
#endif

#ifdef	USESTATFSH
# ifndef	STATFSARGS
# define	STATFSARGS	2
# endif
#endif


#if defined (USEMNTTABH)
# ifdef	USEFSTABH
# undef	USEFSTABH
# endif
#endif

#if defined (USEMNTTABH) || defined (USEMNTCTL)
# ifdef	USEMNTINFOR
# undef	USEMNTINFOR
# endif
#endif

#if defined (USEMNTTABH) || defined (USEMNTCTL) || defined (USEMNTINFOR)
# ifdef	USEMNTINFO
# undef	USEMNTINFO
# endif
#endif

#if defined (USEMNTTABH) || defined (USEMNTCTL) || defined (USEMNTINFOR)\
 || defined (USEMNTINFO)
# ifdef	USEGETMNT
# undef	USEGETMNT
# endif
#endif

#if defined (USEMNTTABH) || defined (USEMNTCTL) || defined (USEMNTINFOR)\
 || defined (USEMNTINFO) || defined (USEGETMNT)
# ifdef	USEFSTABH
# undef	USEFSTABH
# endif
#endif

#if defined (USEMNTTABH) || defined (USEMNTCTL) || defined (USEMNTINFOR)\
 || defined (USEMNTINFO) || defined (USEGETMNT) || defined (USEFSTABH)
# ifdef	USEMNTENTH
# undef	USEMNTENTH
# endif
#else
#define	USEMNTENTH
#endif
