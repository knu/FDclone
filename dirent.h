/*
 *	dirent.h
 *
 *	definitions for directory entry
 */

#ifndef	__DIRENT_H_
#define	__DIRENT_H_

#include "unixemu.h"

#if	defined (USESYSDIRH) || defined (USEDIRECT)
#include <sys/dir.h>
#endif
#if	MSDOS
# if	defined (__TURBOC__) || (defined (DJGPP) && DJGPP < 2)
# include <dir.h>
# endif
#else	/* !MSDOS */
# ifndef	USEDIRECT
# include <dirent.h>
# endif
#endif	/* !MSDOS */

#ifdef	USEDIRECT
#define	dirent			direct
#undef	DIRSIZ
#endif

#ifndef	DEV_BSIZE
#define	DEV_BSIZE		512
#endif
#if	!defined (MAXNAMLEN) && defined (FILENAME_MAX)
#define	MAXNAMLEN		FILENAME_MAX
#endif

#if	defined (DNAMESIZE) && DNAMESIZE < (MAXNAMLEN + 1)
typedef struct _st_dirent {
	char buf[(int)sizeof(struct dirent) - DNAMESIZE + MAXNAMLEN + 1];
} st_dirent;
#else
typedef struct dirent		st_dirent;
#endif

#endif	/* !__DIRENT_H_ */
