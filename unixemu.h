/*
 *	unixemu.h
 *
 *	UNIX function emulation
 */

#if	MSDOS && !defined (__UNIXEMU_H_)
#define	__UNIXEMU_H_

#undef	MAXPATHLEN
#define	MAXPATHLEN		260

#undef	MAXNAMLEN
#define	MAXNAMLEN		255

#ifndef	R_OK
#define	R_OK			4
#endif
#ifndef	W_OK
#define	W_OK			2
#endif
#ifndef	X_OK
#define	X_OK			1
#endif
#ifndef	F_OK
#define	F_OK			0
#endif

#ifndef	S_IFLNK
#define	S_IFLNK			0120000
#endif
#ifndef	S_IFSOCK
#define	S_IFSOCK		0140000
#endif
#ifndef	S_IFIFO
#define	S_IFIFO			0010000
#endif
#ifndef	S_ISUID
#define	S_ISUID			0004000
#endif
#ifndef	S_ISGID
#define	S_ISGID			0002000
#endif
#ifndef	S_ISVTX
#define	S_ISVTX			0001000
#endif

#ifndef	DJGPP
# ifndef	random
# define	random()	rand()
# endif
#define	kill(pid, sig)		((raise(sig)) ? -1 : 0)
#define	SIGALRM			11	/* SIGSEGV */
#undef	SIGSEGV
#undef	SIGTERM
#undef	SIGILL
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

#define	DID_IFNORMAL		000
#define	DID_IFLABEL		001
#define	DID_IFLFN		002

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

#endif	/* MSDOS && !__UNIXEMU_H_ */
