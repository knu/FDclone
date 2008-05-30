/*
 *	headers.h
 *
 *	include headers
 */

#include "machine.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#ifdef	USESTDARGH
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#if	!MSDOS
#include <sys/time.h>
#include <sys/param.h>
#include <sys/file.h>
# ifdef	USEUTIME
# include <utime.h>
# endif
#endif	/* !MSDOS */

#ifndef	O_BINARY
#define	O_BINARY		0
#endif
#ifndef	O_TEXT
#define	O_TEXT			0
#endif
#ifndef	O_NOCTTY
#define	O_NOCTTY		0
#endif
#ifndef	O_ACCMODE
#define	O_ACCMODE		(O_RDONLY | O_WRONLY | O_RDWR)
#endif

#ifndef	L_SET
# ifdef	SEEK_SET
# define	L_SET		SEEK_SET
# else
# define	L_SET		0
# endif
#endif	/* !L_SET */
#ifndef	L_INCR
# ifdef	SEEK_CUR
# define	L_INCR		SEEK_CUR
# else
# define	L_INCR		1
# endif
#endif	/* !L_INCR */
#ifndef	L_XTND
# ifdef	SEEK_END
# define	L_XTND		SEEK_END
# else
# define	L_XTND		2
# endif
#endif	/* !L_XTND */

#ifdef	__TURBOC__
#undef	EFAULT
#undef	ENFILE
#undef	ENOSPC
#undef	EROFS
#undef	EPERM
#undef	EINTR
#undef	EIO
#undef	ENXIO
#undef	ENOTDIR
#undef	EISDIR
#endif

#ifndef	ENOSPC
#define	ENOSPC			EACCES
#endif
#ifndef	ENODEV
#define	ENODEV			EACCES
#endif
#ifndef	EIO
#define	EIO			ENODEV
#endif
#ifndef	EISDIR
#define	EISDIR			EACCES
#endif
#ifndef	ENOTEMPTY
# ifdef	ENFSNOTEMPTY
# define	ENOTEMPTY	ENFSNOTEMPTY
# else
# define	ENOTEMPTY	EACCES
# endif
#endif	/* !ENOTEMPTY */
#ifndef	EPERM
#define	EPERM			EACCES
#endif
#ifndef	EFAULT
#define	EFAULT			ENODEV
#endif
#ifndef	EROFS
#define	EROFS			EACCES
#endif
#ifndef	ENAMETOOLONG
#define	ENAMETOOLONG		ERANGE
#endif
#ifndef	ENOTDIR
#define	ENOTDIR			ENOENT
#endif
#ifndef	EINTR
#define	EINTR			0
#endif
#ifndef	ETIMEDOUT
#define	ETIMEDOUT		EIO
#endif

#ifdef	NOERRNO
extern int errno;
#endif
#ifdef	USESTRERROR
#define	strerror2		strerror
#else	/* !USESTRERROR */
# ifndef	DECLERRLIST
extern CONST char *CONST sys_errlist[];
# endif
#define	strerror2(n)		(char *)sys_errlist[n]
#endif	/* !USESTRERROR */

#ifdef	USESTDARGH
#define	VA_START(a, f)		va_start(a, f)
#else
#define	VA_START(a, f)		va_start(a)
#endif

#if	!MSDOS && defined (UF_SETTABLE) && defined (SF_SETTABLE)
#define	HAVEFLAGS
#endif

#undef	S_IRUSR
#undef	S_IWUSR
#undef	S_IXUSR
#undef	S_IRGRP
#undef	S_IWGRP
#undef	S_IXGRP
#undef	S_IROTH
#undef	S_IWOTH
#undef	S_IXOTH
#define	S_IRUSR			00400
#define	S_IWUSR			00200
#define	S_IXUSR			00100
#define	S_IRGRP			00040
#define	S_IWGRP			00020
#define	S_IXGRP			00010
#define	S_IROTH			00004
#define	S_IWOTH			00002
#define	S_IXOTH			00001
#define	S_IREAD_ALL		(S_IRUSR | S_IRGRP | S_IROTH)
#define	S_IWRITE_ALL		(S_IWUSR | S_IWGRP | S_IWOTH)
#define	S_IEXEC_ALL		(S_IXUSR | S_IXGRP | S_IXOTH)

#ifdef	HAVEFLAGS
# ifndef	UF_NODUMP
# define	UF_NODUMP	0x00000001
# endif
# ifndef	UF_IMMUTABLE
# define	UF_IMMUTABLE	0x00000002
# endif
# ifndef	UF_APPEND
# define	UF_APPEND	0x00000004
# endif
# ifndef	UF_NOUNLINK
# define	UF_NOUNLINK	0x00000010
# endif
# ifndef	SF_ARCHIVED
# define	SF_ARCHIVED	0x00010000
# endif
# ifndef	SF_IMMUTABLE
# define	SF_IMMUTABLE	0x00020000
# endif
# ifndef	SF_APPEND
# define	SF_APPEND	0x00040000
# endif
# ifndef	SF_NOUNLINK
# define	SF_NOUNLINK	0x00080000
# endif
#endif	/* HAVEFLAGS */
