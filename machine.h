/*
 *	Definition of Machine Depended Identifier
 */

#if defined (sun)
#define	CODEEUC
#define	CPP7BIT
# if defined (USGr4) || defined (__svr4__) || defined (__SVR4)
# define	SVR4
# define	SOLARIS
# define	REGEXPLIB	-lgen
# define	USESTATVFS
# define	USEMNTTAB
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
# define	REGEXPLIB	-lgen
# define	USESTATVFS
# define	USEMNTTAB
# define	USEUTIME
# else
# define	BSD43
# define	NEWS_OS
# define	USERE_COMP
# define	USESETENV
# endif
#endif

#if defined (sgi)
#define	SYSV
#define	IRIX
#define	CODEEUC
#define	UNSIGNEDCHAR
# if !defined (SYSTYPE_SVR4)
# define	TERMCAPLIB	-lcurses
# define	EXTENDLIB	-lsun
# endif
#define	IRIXFS
#define	USESTATFS
#define	USEMATHRAND
#define	USERE_COMP
#endif

#if defined (hpux) || defined (__H3050) || defined (__H3050R) || defined (__H3050RX)
#define	SVR4
#define	HPUX
#define	NOOPTIMIZE
#define	TERMCAPLIB	-lcurses
#define	USEREGCOMP
#define	USEUTIME
#endif

#if defined (nec_ews) || defined (_nec_ews)
#define	CODEEUC
# if defined (USGr4) || defined (SYSTYPE_SYSV)
# define	SVR4
# define	EWSUXV
# define	USESTATVFS
# define	USEMNTTAB
# define	USEREGCOMP
# define	SIGARGINT
# endif
#endif

#if defined (uniosu)
#define	BSD43
#define	UNIOS
#define	CODEEUC
# ifdef	__GNUC__
# define	TERMCAPLIB	-lcurses
# define	NOVOID
# define	NOUID_T
# define	NOSTDLIB
# define	USETERMIO
# define	HAVETIMEZONE
# endif
#endif

#if defined (__alpha) || defined (alpha)
#define	CODEEUC
#define	USEREGCOMP
# if defined (SYSTYPE_BSD)
# define	SVR4
# define	DECOSF1
# define	EXTENDLIB	-lsys5
# define	USEMOUNTH
# define	USEFSTABH
# else
# define	BSD43
# define	DECOSF3
# define	TARUSESPACE
# define	EXTENDLIB	-lc_r
# define	USESTATVFS
# endif
#endif

#if defined (ultrix)
#define	BSD43
#define	ULTRIX
#define	CODEEUC
#define	CPP7BIT
#define	USERE_COMP
#endif

#if defined (linux)
#define	SVR4
#define	LINUX
#define	USERE_COMP
#endif

#if defined (__FREEBSD__)
#define	BSD43
#define	FREEBSD
#define	REGEXPLIB	-lcomat
#define	DECLERRLIST
#define	USEMOUNTH
#define	USEFSTABH
#define	USERE_COMP
#endif

/****************************************************************
 *	If your machine is not described above,	you should	*
 *	comment out below manually to apply your environment.	*
 ****************************************************************/

/* #define SYSV		/* OS type is System V older Rel.3 */
/* #define SVR4		/* OS type is System V Rel.4 */
/* #define BSD43	/* OS type is BSD 4.3 */
/* #define SOLARIS	/* OS type is newer Solalis 2.0 (Sun) */
/* #define SUN_OS	/* OS type is older SunOS 4.1 (Sun) */
/* #define NEWS_OS6	/* OS type is newer NEWS-OS R6.0 (SONY) */
/* #define NEWS_OS	/* OS type is older NEWS-OS R4.2 (SONY) */
/* #define IRIX		/* OS type is IRIX (SGI) */
/* #define HPUX		/* OS type is HP-UX (HP) */
/* #define EWSUXV	/* OS type is EWS-UX/V (NEC) */
/* #define UNIOS	/* OS type is UniOS-U (OMRON) */
/* #define DECOSF	/* OS type is OSF/1 (DEC) */
/* #define ULTRIX	/* OS type is ULTRIX (DEC) */
/* #define LINUX	/* OS type is Linux */
/* #define FREEBSD	/* OS type is FreeBSD */

/* #define CODEEUC	/* kanji code type is EUC */
/* #define TARUSESPACE	/* tar(1) uses space to devide file mode from UID */
/* #define CPP7BIT	/* /bin/cpp cannot through 8bit */
/* #define NOOPTIMIZE	/* -O option not exists on /bin/cc */
/* #define UNSIGNEDCHAR	/* signed char needs -signed option on /bin/cc */
/* #define TERMCAPLIB	/* library needed for termcap */
/* #define REGEXPLIB	/* library needed for regular expression */
/* #define EXTENDLIB	/* library needed for the other extended */
/* #define IRIXFS	/* use IRIX type FileSystem */
/* #define USELEAPCNT	/* TZFILE includes tzh_leapcnt as leap second */
/* #define NOVOID	/* cannot use type 'void' */
/* #define NOUID_T	/* uid_t, gid_t is not defined in <sys/types.h> */
/* #define DECLERRLIST	/* 'sys_errlist' already declared in <stdio.h> */
/* #define NOSTDLIB	/* have not <stdlib.h> */
/* #define USETERMIO	/* use termio interface */
/* #define SYSVDIRENT	/* dirent interface behaves as System V */
/* #define HAVETIMEZONE	/* have extern valiable 'timezone' */
/* #define USESTATVFS	/* use <sys/statvfs.h> instead of <sys/vfs.h> */
/* #define USESTATFS	/* use <sys/statfs.h> instead of <sys/vfs.h> */
/* #define USEMOUNTH	/* use <mount.h> instead of <sys/vfs.h> */
/* #define USEMNTTAB	/* use <sys/mnttab.h> instead of <mntent.h> */
/* #define USEFSTABH	/* use <fstab.h> instead of <mntent.h> */
/* #define USERAND48	/* use rand48() family instead of random() */
/* #define USEMATHRAND	/* use random() family with <math.h> */
/* #define USERE_COMP	/* use re_comp() family as search */
/* #define USEREGCOMP	/* use regcomp() family as search */
/* #define USEREGCMP	/* use regcmp() family as search */
/* #define USESETENV	/* use setenv() instead of putenv() */
/* #define USEUTIME	/* use utime() instead of utimes() */
/* #define SIGARGINT	/* signal() needs the 2nd argument as int */


#if defined (SVR4) || defined (SYSV)
#define	TARUSESPACE
#define	USETERMIO
#define	SYSVDIRENT
#define	HAVETIMEZONE
# ifndef	USEMATHRAND
# define	USERAND48
# endif
# if !defined (USERE_COMP) && !defined (USEREGCOMP)
# define	USEREGCMP
# endif
#endif

#if defined (USEREGCMP) && !defined (REGEXPLIB)
#define	REGEXPLIB	-lPW
#endif

#ifndef TERMCAPLIB
#define	TERMCAPLIB	-ltermlib
#endif
