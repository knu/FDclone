/*
 *	Definition of Machine Depended Identifier
 */

#if defined (sun)
#define	CODEEUC
#define	CPP7BIT
# if defined (USGr4) || defined (__svr4__) || defined (__SVR4)
# define	SVR4
# define	SOLARIS
# define	REGEXPLIB	"-lgen"
# define	USESTATVFSH
# define	USEMNTTABH
# define	MOUNTED		MNTTAB
# else
# define	BSD43
# define	SUN_OS
# define	USERE_COMP
# endif
#endif

#if defined (sony)
#define	USELEAPCNT
# if defined (USGr4) || defined (SYSTYPE_SYSV)
# define	SVR4
# define	NEWS_OS6
# define	REGEXPLIB	"-lgen"
# define	USESTATVFSH
# define	USEMNTTABH
# define	MOUNTED		MNTTAB
# define	USEUTIME
# else
# define	BSD43
#  if defined (__sony)
#  define	NEWS_OS
#  define	USESETENV
#  else
#  define	NEWS_OS3
#  define	NOVOID
#  define	NOERRNO
#  define	NOFILEMODE
#  define	NOUNISTDH
#  define	NOSTDLIBH
#  define	USEDIRECT
#  define	HAVETIMEZONE
#  define	NOPUTENV
#  define	NOSTRSTR
#  define	USEGETWD
#  endif
# define	USESYSDIRH
# define	USERE_COMP
# endif
#endif

#if defined (sgi)
#define	SYSV
#define	IRIX
#define	CODEEUC
#define	EXTENDCCOPT		"-O -signed"
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
#define	HPUX
#define	EXTENDCCOPT
#define	TERMCAPLIB		"-lcurses"
#define	USEREGCOMP
#define	USEUTIME
#endif

#if defined (nec_ews) || defined (_nec_ews)
#define	CODEEUC
# if defined (nec_ews_svr4) || defined (_nec_ews_svr4)
# define	SVR4
# define	EWSUXV
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
#define	UNIOS
#define	CODEEUC
#define	EXTENDCCOPT		"-O -Zu"
#define	TERMCAPLIB		"-lcurses"
#define	UNKNOWNFS
#define	NOVOID
#define	NOUID_T
#define	NOFILEMODE
#define	NOSTDLIBH
#define	USETERMIO
#define	HAVETIMEZONE
#define	USETIMEH
#define	NOSTRSTR
#endif

#if defined (luna88k)
#define	BSD43
#define	LUNA88K
#define	CODEEUC
#define	NOERRNO
#define	NOFILEMODE
#define	USEDIRECT
#define	USESETENV
#define	NOSTRSTR
#define	USEGETWD
#endif

#if defined (__alpha) || defined (alpha)
#define	CODEEUC
# if defined (SYSTYPE_BSD)
# define	BSD43
# define	DECOSF1
# define	EXTENDLIB	"-lsys5"
# define	USEMOUNTH
# define	STATFSARGS	3
# define	USEFSTABH
# else
# define	SVR4
# define	DECOSF3
# define	EXTENDLIB	"-lc_r"
# define	USEMNTINFO
# define	USESTATVFSH
# define	USEREGCOMP
# endif
#endif

#if defined (_IBMR2)
#define	SVR4
#define	AIX
#define	EXTENDCCOPT		"-O -qchars=signed"
#define	TERMCAPLIB		"-ltermcap"
#define	USESTATFSH
#define	STATFSARGS	4
#define	USEMNTCTL
#define	USESELECTH
#define	USESYSDIRH
#define	USETIMEH
#endif

#if defined (ultrix)
#define	BSD43
#define	ULTRIX
#define	CODEEUC
#define	TARUSESPACE
#define	CPP7BIT
#define	USEMOUNTH
#define	USEFSDATA
#define	USEGETMNT
#define	USESYSDIRH
#define	USERE_COMP
#endif

#if defined (_AUX_SOURCE)
#define	SYSV
#define	AUX
#define	CPP7BIT
#define	TERMCAPLIB		"-ltermcap"
#define	PWNEEDERROR
#define	USETIMEH
#endif

#if defined (linux)
#define	SVR4
#define	LINUX
#define	CODEEUC
#define	TERMCAPLIB		"-lcurses"
#define	USERE_COMP
#endif

#if defined (__FreeBSD__)
#define	BSD43
#define	FREEBSD
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
#define	NETBSD
#define	DECLERRLIST
#define	USEMOUNTH
#define	USEFFSIZE
#define	USEFSTABH
#define	USERE_COMP
#endif

/****************************************************************
 *	If your machine is not described above,	you should	*
 *	comment out below manually to apply your environment.	*
 ****************************************************************/

/* #define SYSV		/* OS type is System V older Rel.3 */
/* #define SVR4		/* OS type is System V Rel.4 */
/* #define BSD4		/* OS type is older BSD 4.2 */
/* #define BSD43	/* OS type is newer BSD 4.3 */
/* #define SOLARIS	/* OS type is newer Solalis 2.0 (Sun) */
/* #define SUN_OS	/* OS type is older SunOS 4.1 (Sun) */
/* #define NEWS_OS6	/* OS type is newer NEWS-OS R6.0 (SONY) */
/* #define NEWS_OS	/* OS type is older NEWS-OS R4.2 (SONY) */
/* #define NEWS_OS3	/* OS type is older NEWS-OS R3.3 (SONY) */
/* #define IRIX		/* OS type is IRIX (SGI) */
/* #define HPUX		/* OS type is HP-UX (HP) */
/* #define EWSUXV	/* OS type is EWS-UX/V (NEC) */
/* #define UNIOS	/* OS type is UniOS-U (OMRON) */
/* #define LUNA88K	/* OS type is Luna/Mach (OMRON) */
/* #define DECOSF	/* OS type is OSF/1 (DEC) */
/* #define AIX		/* OS type is AIX (IBM) */
/* #define ULTRIX	/* OS type is ULTRIX (DEC) */
/* #define AUX		/* OS type is A/UX */
/* #define LINUX	/* OS type is Linux */
/* #define FREEBSD	/* OS type is FreeBSD */
/* #define NETBSD	/* OS type is NetBSD */

/* #define CODEEUC	/* kanji code type is EUC */
/* #define TARUSESPACE	/* tar(1) uses space to devide file mode from UID */
/* #define CPP7BIT	/* /bin/cpp cannot through 8bit */
/* #define EXTENDCCOPT	/* additional option on /bin/cc */
/* #define TERMCAPLIB	/* library needed for termcap */
/* #define REGEXPLIB	/* library needed for regular expression */
/* #define EXTENDLIB	/* library needed for the other extended */

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
/* #define USETERMIO	/* use termio interface */
/* #define SYSVDIRENT	/* dirent interface behaves as System V */
/* #define USEDIRECT	/* use 'struct direct' instead of dirent */
/* #define HAVETIMEZONE	/* have extern valiable 'timezone' */

/* #define USESTATVFSH	/* use <sys/statvfs.h> as header of the FS status */
/* #define USESTATFSH	/* use <sys/statfs.h> as header of the FS status */
/* #define USEMOUNTH	/* use <mount.h> as header of the FS status */
/* #define USEVFSH	/* use <sys/vfs.h> as header of the FS status */

/* #define USESTATVFS	/* use 'struct statvfs' as structure of hte FS status */
/* #define USEFSDATA	/* use 'struct fs_data' as structure of hte FS status */
/* #define USESTATFS	/* use 'struct statfs' as structure of hte FS status */
/* #define USEFFSIZE	/* 'struct statfs' fas 'f_fsize' instead of 'f_bsize' */
/* #define STATFSARGS	/* the number of arguments in statfs() */

/* #define USEMNTTABH	/* use <sys/mnttab.h> as header of the mount entry */
/* #define USEFSTABH	/* use <fstab.h> as header of the mount entry */
/* #define USEMNTCTL	/* use mntctl() to get the mount entry */
/* #define USEMNTINFO	/* use getmntinfo_r() to get the mount entry */
/* #define USEGETMNT	/* use getmnt() to get the mount entry */
/* #define USEMNTENTH	/* use <mntent.h> as header of the mount entry */
/* #define MOUNTED	/* means '/etc/mtab' defined in <mntent.h> */

/* #define USESELECTH	/* use <sys/select.h> for select() */
/* #define USESYSDIRH	/* use <sys/dir.h> for DEV_BSIZE */
/* #define USETIMEH	/* use <time.h> for 'struct tm' */
/* #define USERAND48	/* use rand48() family instead of random() */
/* #define USERE_COMP	/* use re_comp() family as search */
/* #define USEREGCOMP	/* use regcomp() family as search */
/* #define USEREGCMP	/* use regcmp() family as search */
/* #define USESETENV	/* use setenv() instead of putenv() */
/* #define NOPUTENV	/* have neither putenv() nor setenv() */
/* #define NOSTRSTR	/* have not strstr() */
/* #define USEUTIME	/* use utime() instead of utimes() */
/* #define USEGETWD	/* use getwd() instead of getcwd() */
/* #define SIGARGINT	/* signal() needs the 2nd argument as int */


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
#define	USESTATVFS
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
# ifdef	USEVFSH
# undef	USEVFSH
# endif
#else
#define	USEVFSH
#endif


#if defined (USESTATVFS)
# ifdef	USEFSDATA
# undef	USEFSDATA
# endif
#endif

#if defined (USESTATVFS) || defined (USEFSDATA)
# ifdef	USESTATFS
# undef	USESTATFS
# endif
#else
# define	USESTATFS
# ifndef	STATFSARGS
# define	STATFSARGS	2
# endif
#endif


#if defined (USEMNTTABH)
# ifdef	USEFSTABH
# undef	USEFSTABH
# endif
#endif

#if defined (USEMNTTABH) || defined (USEFSTABH)
# ifdef	USEMNTCTL
# undef	USEMNTCTL
# endif
#endif

#if defined (USEMNTTABH) || defined (USEFSTABH) || defined (USEMNTCTL)
# ifdef	USEMNTINFO
# undef	USEMNTINFO
# endif
#endif

#if defined (USEMNTTABH) || defined (USEFSTABH) || defined (USEMNTCTL)\
 || defined (USEMNTINFO)
# ifdef	USEGETMNT
# undef	USEGETMNT
# endif
#endif

#if defined (USEMNTTABH) || defined (USEFSTABH) || defined (USEMNTCTL)\
 || defined (USEMNTINFO) || defined (USEGETMNT)
# ifdef	USEMNTENTH
# undef	USEMNTENTH
# endif
#else
#define	USEMNTENTH
#endif
