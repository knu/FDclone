/*
 *	unixemu.h
 *
 *	UNIX function emulation
 */

#if	MSDOS && !defined (__UNIXEMU_H_)
#define	__UNIXEMU_H_

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

#endif	/* MSDOS && !__UNIXEMU_H_ */
