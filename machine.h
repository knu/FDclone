/*
 *	Definition of Machine Depended Identifier
 */

#if defined (sun)
#define	CODEEUC
# if defined (USGr4) || defined (__svr4__) || defined (__SVR4)
# define	SVR4
# define	SOLARIS
# define	USEREGCMP
# else
# define	BSD43
# define	SUN_OS
# define	USERE_COMP
# endif
#endif

#if defined (sony)
# if defined (USGr4) || defined (SYSTYPE_SYSV)
# define	SVR4
# define	NEWS_OS6
# define	USEREGCMP
# else
# define	BSD43
# define	NEWS_OS
# define	USERE_COMP
# define	USESETENV
# endif
#define	USELEAPCNT
#endif

#if defined (sgi)
#define	SVR4
#define	IRIX
#define	UNSIGNEDCHAR
#define	IRIXFS
#define	USESTATVFS
#define	USERE_COMP
#define	CODEEUC
#endif

#if defined (hpux)
#define	SVR4
#define	HPUX
#define	NOOPTIMIZE
#define	USECURSES
#define	USEREGCOMP
#define	USEUTIME
#endif

#if defined (ultrix)
#define	BSD43
#define	ULTRIX
#define	USERE_COMP
#define	CPP7BIT
#define	CODEEUC
#endif


/****************************************************************
 *	If your machine is not described above,	you should	*
 *	comment out below manually to apply your environment.	*
 ****************************************************************/

/* #define SVR4		/* OS type is System V Rel.4 */
/* #define BSD43	/* OS type is BSD 4.3 */
/* #define SOLARIS	/* OS type is newer Solalis 2.0 (Sun) */
/* #define SUN_OS	/* OS type is older SunOS 4.1 (Sun) */
/* #define NEWS_OS6	/* OS type is newer NEWS-OS R6.0 (SONY) */
/* #define NEWS_OS	/* OS type is older NEWS-OS R4.2 (SONY) */
/* #define IRIX		/* OS type is IRIX (SGI) */
/* #define HPUX		/* OS type is HP-UX (HP) */
/* #define ULTRIX	/* OS type is ULTRIX (DEC) */

/* #define NOOPTIMIZE	/* -O option not exists on /bin/cc */
/* #define UNSIGNEDCHAR	/* signed char needs -signed option on /bin/cc */
/* #define USECURSES	/* use curses library as termcap library */
/* #define USETERMIO	/* use termio interface */
/* #define SVR4DIRENT	/* dirent interface behaves as System V Rel.4 */
/* #define IRIXFS	/* use IRIX type FileSystem */
/* #define USESTATVFS	/* use <sys/statvfs.h> instead of <sys/vfs.h> */
/* #define USERAND48	/* use rand48() family instead of random() */
/* #define USERE_COMP	/* use re_comp() family as search */
/* #define USEREGCOMP	/* use regcomp() family as search */
/* #define USEREGCMP	/* use regcmp() family as search */
/* #define HAVETIMEZONE	/* have extern valiable 'timezone' */
/* #define USESETENV	/* use setenv() instead of putenv() */
/* #define USELEAPCNT	/* TZFILE includes tzh_leapcnt as leap second */
/* #define USEUTIME	/* use utime() instead of utimes() */
/* #define CPP7BIT	/* /bin/cpp cannot through 8bit */
/* #define TARUSESPACE	/* tar(1) uses space to devide file mode from UID */
/* #define CODEEUC	/* kanji code type is EUC */


#ifdef	SVR4
#define	USETERMIO
#define	SVR4DIRENT
#define	USERAND48
# if !defined(USERE_COMP) && !defined(USEREGCOMP)
# define	USEREGCMP
# endif
#define	HAVETIMEZONE
#define	TARUSESPACE
#endif
