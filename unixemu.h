/*
 *	unixemu.h
 *
 *	UNIX Emulate Functions
 */

#ifndef	_UNIXEMU_H
#define	_UNIXEMU_H

#ifndef	__SYS_TYPES_STAT_H_
#define	__SYS_TYPES_STAT_H_
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#ifdef	MAXPATHLEN
#undef	MAXPATHLEN
#endif
#define	MAXPATHLEN	260

#ifdef	MAXNAMLEN
#undef	MAXNAMLEN
#endif
#define	MAXNAMLEN	255

#ifndef	R_OK
#define	R_OK		4
#endif
#ifndef	W_OK
#define	W_OK		2
#endif
#ifndef	X_OK
#define	X_OK		1
#endif
#ifndef	F_OK
#define	F_OK		0
#endif

#ifndef	S_IFLNK
#define	S_IFLNK		0120000
#endif
#ifndef	S_IFSOCK
#define	S_IFSOCK	0140000
#endif
#ifndef	S_IFIFO
#define	S_IFIFO		0010000
#endif
#ifndef	S_ISUID
#define	S_ISUID		0004000
#endif
#ifndef	S_ISGID
#define	S_ISGID		0002000
#endif
#ifndef	S_ISVTX
#define	S_ISVTX		0001000
#endif

#if	!defined (DJGPP) || (DJGPP < 2)
#define	ENOTEMPTY	EACCES
#endif

#ifndef	DJGPP
# ifndef	random
# define	random()		rand()
# endif
#define	kill(pid, sig)		((raise(sig)) ? -1 : 0)
#define	SIGALRM		11	/* SIGSEGV */
#undef	SIGSEGV
#undef	SIGTERM
#undef	SIGILL
# ifdef	__TURBOC__
# undef	EFAULT
# undef	ENFILE
# undef	ENOSPC
# undef	EROFS
# undef	EPERM
# undef	EINTR
# undef	EIO
# undef	ENXIO
# undef	ENOTDIR
# undef	EISDIR
# endif
# ifndef	EINTR
# define	EINTR		0
# endif
# ifndef	ENOTDIR
# define	ENOTDIR		ENOENT
# endif
#endif	/* !DJGPP */

typedef struct _dirdesc {
	int dd_id;
	u_short dd_fd;
#if	0
	long dd_loc;
	long dd_size;
	long dd_bsize;
#endif
	long dd_off;
	char *dd_buf;

	char *dd_path;
} DIR;

#define	DID_IFNORMAL	000
#define	DID_IFLABEL	001
#define	DID_IFLFN	002

struct dirent {
	long d_off;
#ifndef	_NODOSDRIVE
	u_long d_fileno;
	u_short d_reclen;
#endif
	char d_name[MAXNAMLEN + 1];

	char d_alias[14];
};

struct utimbuf {
	time_t actime;
	time_t modtime;
};

#ifdef	LSI_C
extern int utime __P_((CONST char *, CONST time_t[]));
#else
extern int utime __P_((CONST char *, CONST struct utimbuf *));
#endif

#endif	/* !_UNIXEMU_H */
