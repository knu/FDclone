/*
 *	unixemu.h
 *
 *	UNIX Emulate Functions
 */

#ifndef	_UNIXEMU_H
#define	_UNIXEMU_H

#ifdef	NOLFNEMU
#include <dirent.h>
#endif

#ifdef	MAXPATHLEN
#undef	MAXPATHLEN
#endif
#define	MAXPATHLEN	260

#ifdef	MAXNAMLEN
#undef	MAXNAMLEN
#endif
#define	MAXNAMLEN	255

#define	PIPEDIR		"FD-PIPE"
#define	PIPEFILE	"FAKEPIPE"

#define	srandom(seed)		srand(seed)
#define	lstat			stat

#define	R_OK		4
#define	W_OK		2
#define	X_OK		1
#define	F_OK		0

#define	S_IFLNK		0120000
#define	S_IFSOCK	0140000
#define	S_ISVTX		0001000

#if	!defined (DJGPP) || (DJGPP < 2)
#define	ENOTEMPTY	EACCES
#endif

#ifdef	__GNUC__
# if	defined (DJGPP) && (DJGPP < 2)
# define	S_ISUID		0004000
# define	S_ISGID		0002000
# define	S_ISVTX		0001000
# endif
#define	FP_SEG(p)	0
#define	FP_OFF(p)	(unsigned)(p)
#else	/* !__GNUC__ */
#define	random()		rand()
#define	kill(pid, sig)		raise(sig)

#define	S_IFIFO		0010000
#define	SIGALRM		11	/* SIGSEGV */
#undef	SIGSEGV
#undef	SIGTERM
#undef	SIGILL
#define	EINTR		0
#define	ENOTDIR		ENOENT
#endif	/* !__GNUC__ */

#ifndef	NOLFNEMU
typedef struct _dirdesc {
	u_short	dd_fd;
/*
	long	dd_loc;
	long	dd_size;
	long	dd_bsize;
*/
	long	dd_off;
	char	*dd_buf;

	char	*dd_path;
} DIR;

struct	dirent {
	long	d_off;
/*
	u_long	d_fileno;
	u_short	d_reclen;
*/
	char	d_name[MAXNAMLEN + 1];

	char	d_alias[14];
};
#endif	/* !NOLFNEMU */

struct utimbuf {
	time_t	actime;
	time_t	modtime;
};

extern	int utime __P_((CONST char *, CONST struct utimbuf *));

#endif	/* !_UNIXEMU_H */
