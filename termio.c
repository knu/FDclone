/*
 *	termio.c
 *
 *	terminal I/O
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "machine.h"
#include "termio.h"

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include <errno.h>
#ifdef	NOERRNO
extern int errno;
#endif

#ifdef	USERESOURCEH
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef	USEULIMITH
#include <ulimit.h>
#endif

#if	defined (USESYSCONF) && defined (_SC_OPEN_MAX)
#define	MAXOPENFILE	sysconf(_SC_OPEN_MAX)
#else
# ifdef	NOFILE
# define	MAXOPENFILE	NOFILE
# else
#  ifdef	OPEN_MAX
#  define	MAXOPENFILE	OPEN_MAX
#  else
#   if	MSDOS
#   define	MAXOPENFILE	20
#   else
#   define	MAXOPENFILE	64
#   endif
#  endif
# endif
#endif

#ifndef	_PATH_TTY
# if	MSDOS
# define	_PATH_TTY	"CON"
# else
# define	_PATH_TTY	"/dev/tty"
# endif
#endif	/* !_PATH_TTY */

#ifndef	UL_GDESLIM
#define	UL_GDESLIM	4
#endif
#ifndef	FD_CLOEXEC
#define	FD_CLOEXEC	1
#endif
#if	!defined (RLIMIT_NOFILE) && defined (RLIMIT_OFILE)
#define	RLIMIT_NOFILE	RLIMIT_OFILE
#endif

#ifdef	USETERMIO
#undef	VSTART
#undef	VSTOP
#endif

#define	K_CTRL(c)	((c) & 037)

#if	!defined (FD) || defined (_NODOSDRIVE)
#define	Xread		read
#define	Xwrite		write
#else
extern int Xread __P_((int, char *, int));
extern int Xwrite __P_((int, char *, int));
#endif

#ifdef	LSI_C
extern u_char _openfile[];
#endif

#if	defined (FD) && !defined (_NODOSDRIVE)
#define	DOSFDOFFSET	(1 << (8 * sizeof(int) - 2))
#endif

#if	MSDOS
#include "unixemu.h"
static CONST u_short doserrlist[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 17, 18, 65, 80
};
#define	DOSERRLISTSIZ	((int)(sizeof(doserrlist) / sizeof(u_short)))
static CONST int unixerrlist[] = {
	0, EINVAL, ENOENT, ENOENT, EMFILE, EACCES,
	EBADF, ENOMEM, ENOMEM, ENOMEM, ENODEV, EXDEV, 0, EACCES, EEXIST
};
#endif	/* MSDOS */

#ifdef	CYGWIN
static int save_ttyio = -1;
#endif


#ifdef	LSI_C
int safe_dup(oldd)
int oldd;
{
	int fd;

	if ((fd = dup(oldd)) < 0) return(-1);
	if (fd < SYS_OPEN && oldd >= 0 && oldd < SYS_OPEN)
		_openfile[fd] = _openfile[oldd];

	return(fd);
}

int safe_dup2(oldd, newd)
int oldd, newd;
{
	int fd;

	if (oldd == newd) return(newd);
	if ((fd = dup2(oldd, newd)) < 0) return(-1);
	if (newd >= 0 && newd < SYS_OPEN && oldd >= 0 && oldd < SYS_OPEN)
		_openfile[newd] = _openfile[oldd];

	return(fd);
}
#endif	/* LSI_C */

#if	MSDOS
int seterrno(doserr)
u_int doserr;
{
	int i;

	for (i = 0; i < DOSERRLISTSIZ; i++)
		if (doserr == doserrlist[i]) return(errno = unixerrlist[i]);

	return(errno = EINVAL);
}

int intcall(vect, regp, sregp)
int vect;
__dpmi_regs *regp;
struct SREGS *sregp;
{
# ifdef	__TURBOC__
	struct REGPACK preg;
# endif

	(*regp).x.flags |= FR_CARRY;
# ifdef	__TURBOC__
	preg.r_ax = (*regp).x.ax;
	preg.r_bx = (*regp).x.bx;
	preg.r_cx = (*regp).x.cx;
	preg.r_dx = (*regp).x.dx;
	preg.r_bp = (*regp).x.bp;
	preg.r_si = (*regp).x.si;
	preg.r_di = (*regp).x.di;
	preg.r_ds = (*sregp).ds;
	preg.r_es = (*sregp).es;
	preg.r_flags = (*regp).x.flags;
	intr(vect, &preg);
	(*regp).x.ax = preg.r_ax;
	(*regp).x.bx = preg.r_bx;
	(*regp).x.cx = preg.r_cx;
	(*regp).x.dx = preg.r_dx;
	(*regp).x.bp = preg.r_bp;
	(*regp).x.si = preg.r_si;
	(*regp).x.di = preg.r_di;
	(*sregp).ds = preg.r_ds;
	(*sregp).es = preg.r_es;
	(*regp).x.flags = preg.r_flags;
# else	/* !__TURBOC__ */
#  ifdef	DJGPP
	(*regp).x.ds = (*sregp).ds;
	(*regp).x.es = (*sregp).es;
	__dpmi_int(vect, regp);
	(*sregp).ds = (*regp).x.ds;
	(*sregp).es = (*regp).x.es;
#  else
	int86x(vect, regp, regp, sregp);
#  endif
# endif	/* !__TURBOC__ */
	if (!((*regp).x.flags & FR_CARRY)) return(0);
	seterrno((*regp).x.ax);

	return(-1);
}
#endif	/* MSDOS */

int Xgetdtablesize(VOID_A)
{
#if	!MSDOS
# ifndef	SYSV
	int n;

	if ((n = getdtablesize()) >= 0) return(n);
# else	/* SYSV */
#  ifdef	USERESOURCEH
	struct rlimit lim;

	if (getrlimit(RLIMIT_NOFILE, &lim) >= 0) return(lim.rlim_cur);
#  else	/* !USERESOURCEH */
#   ifdef	USEULIMITH
	long n;

	if ((n = ulimit(UL_GDESLIM, 0L)) >= 0L) return(n);
#   endif
#  endif	/* !USERESOURCEH */
# endif	/* SYSV */
#endif	/* !MSDOS */

	return(MAXOPENFILE);
}

int isvalidfd(fd)
int fd;
{
#if	MSDOS
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x4400;
	reg.x.bx = fd;

	return(intcall(0x21, &reg, &sreg));
#else	/* !MSDOS */
	return(fcntl(fd, F_GETFD, NULL));
#endif	/* !MSDOS */
}

int newdup(fd)
int fd;
{
	int n;

	if (fd < 0
	|| fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
		return(fd);
#if	defined (FD) && !defined (_NODOSDRIVE)
	if (fd >= DOSFDOFFSET) return(fd);
#endif
	n = Xgetdtablesize();

	for (n--; n > fd; n--) if (isvalidfd(n) < 0) break;
	if (n <= fd || safe_dup2(fd, n) < 0) return(fd);
	close(fd);

	return(n);
}

int sureread(fd, buf, nbytes)
int fd;
VOID_P buf;
int nbytes;
{
	int n;

	for (;;) {
		if ((n = Xread(fd, buf, nbytes)) >= 0) {
			errno = 0;
			return(n);
		}
#ifdef	EAGAIN
		else if (errno == EAGAIN) continue;
#endif
#ifdef	EWOULDBLOCK
		else if (errno == EWOULDBLOCK) continue;
#endif
		else if (errno != EINTR) break;
	}

	return(-1);
}

int surewrite(fd, buf, nbytes)
int fd;
VOID_P buf;
int nbytes;
{
	char *cp;
	int n;

	cp = (char *)buf;
	for (;;) {
		if ((n = Xwrite(fd, cp, nbytes)) >= 0) {
			if (n >= nbytes) {
				errno = 0;
				return(0);
			}
#if	MSDOS
			if (!n) {
				errno = ENOSPC;
				break;
			}
#endif
			cp += n;
			nbytes -= n;
		}
#ifdef	EAGAIN
		else if (errno == EAGAIN) continue;
#endif
#ifdef	EWOULDBLOCK
		else if (errno == EWOULDBLOCK) continue;
#endif
		else if (errno != EINTR) break;
	}

	return(-1);
}

VOID safeclose(fd)
int fd;
{
	int duperrno;

	if (fd < 0) return;
	duperrno = errno;
	if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
		close(fd);
	errno = duperrno;
}

int opentty(fdp, fpp)
int *fdp;
FILE **fpp;
{
	FILE *fp;
	int fd, flags;

#ifdef	SELECTRWONLY
	flags = O_RDONLY;
#else
	flags = O_RDWR;
#endif

	if (*fdp >= 0) fd = *fdp;
	else if ((fd = newdup(open(_PATH_TTY, flags, 0600))) < 0) return(-1);
	if (*fpp) fp = *fpp;
	else if (!(fp = fdopen(fd, "w+b")) && !(fp = fopen(_PATH_TTY, "w+b")))
		return(-1);

	*fdp = fd;
	*fpp = fp;
#ifdef	CYGWIN
	save_ttyio = fd;
#endif

	return(0);
}

VOID closetty(fdp, fpp)
int *fdp;
FILE **fpp;
{
	if (*fpp) {
		if (fileno(*fpp) == *fdp) *fdp = -1;
		fclose(*fpp);
	}
	if (*fdp >= 0) close(*fdp);

	*fdp = -1;
	*fpp = NULL;
}

#if	MSDOS
/*ARGSUSED*/
VOID loadtermio(fd, tty, ws)
int fd;
char *tty, *ws;
{
# ifndef	DJGPP
	union REGS reg;

	if (tty) {
		reg.x.ax = 0x3301;
		memcpy((char *)&(reg.h.dl), (char *)tty, sizeof(reg.h.dl));
		int86(0x21, &reg, &reg);
	}
# endif	/* !DJGPP */
}

VOID savetermio(fd, ttyp, wsp)
int fd;
char **ttyp, **wsp;
{
# ifndef	DJGPP
	union REGS reg;
	char *tty;
# endif

	if (ttyp) do {
		*ttyp = NULL;
# ifndef	DJGPP
		if (!(tty = (char *)malloc(TIO_BUFSIZ))) break;

		reg.x.ax = 0x3300;
		int86(0x21, &reg, &reg);
		memcpy((char *)tty, (char *)&(reg.h.dl), TIO_BUFSIZ);
		*ttyp = tty;
# endif	/* !DJGPP */
	} while (0);

	if (wsp) *wsp = NULL;
}
#else	/* !MSDOS */
VOID closeonexec(fd)
int fd;
{
	int n;

	if (fd >= 0 && (n = fcntl(fd, F_GETFD, NULL)) >= 0)
		VOID_C fcntl(fd, F_SETFD, n | FD_CLOEXEC);
}

int Xioctl(fd, request, argp)
int fd, request;
VOID_P argp;
{
	for (;;) {
		if (ioctl(fd, request, argp) >= 0) {
			errno = 0;
			return(0);
		}
# ifdef	EAGAIN
		else if (errno == EAGAIN) continue;
# endif
# ifdef	EWOULDBLOCK
		else if (errno == EWOULDBLOCK) continue;
# endif
		else if (errno != EINTR) break;
	}

	return(-1);
}

# ifdef	USETERMIOS
int Xtcgetattr(fd, t)
int fd;
termioctl_t *t;
{
	for (;;) {
		if (tcgetattr(fd, t) >= 0) {
			errno = 0;
			return(0);
		}
#  ifdef	EAGAIN
		else if (errno == EAGAIN) continue;
#  endif
#  ifdef	EWOULDBLOCK
		else if (errno == EWOULDBLOCK) continue;
#  endif
		else if (errno != EINTR) break;
	}

	return(-1);
}

int Xtcsetattr(fd, action, t)
int fd, action;
termioctl_t *t;
{
	for (;;) {
		if (tcsetattr(fd, action, t) >= 0) {
			errno = 0;
			return(0);
		}
#  ifdef	EAGAIN
		else if (errno == EAGAIN) continue;
#  endif
#  ifdef	EWOULDBLOCK
		else if (errno == EWOULDBLOCK) continue;
#  endif
		else if (errno != EINTR) break;
	}

	return(-1);
}

int Xtcflush(fd, selector)
int fd, selector;
{
	for (;;) {
		if (tcflush(fd, selector) >= 0) {
			errno = 0;
			return(0);
		}
#  ifdef	EAGAIN
		else if (errno == EAGAIN) continue;
#  endif
#  ifdef	EWOULDBLOCK
		else if (errno == EWOULDBLOCK) continue;
#  endif
		else if (errno != EINTR) break;
	}

	return(-1);
}
# endif	/* USETERMIOS */

/*ARGSUSED*/
VOID loadtermio(fd, tty, ws)
int fd;
char *tty, *ws;
{
	ALLOC_T size;

	if (tty) {
		size = (ALLOC_T)0;
		VOID_C tioctl(fd, REQSETN, (termioctl_t *)&(tty[size]));
# ifdef	USESGTTY
		size += sizeof(termioctl_t);
		VOID_C Xioctl(fd, TIOCLSET, (int *)&(tty[size]));
		size += sizeof(int);
		VOID_C Xioctl(fd, TIOCSETC, (struct tchars *)&(tty[size]));
# endif
	}

# ifndef	NOTERMWSIZE
	if (ws) VOID_C Xioctl(fd, REQSETWS, (termwsize_t *)ws);
# endif
}

VOID savetermio(fd, ttyp, wsp)
int fd;
char **ttyp, **wsp;
{
# ifndef	NOTERMWSIZE
	char *ws;
# endif
	ALLOC_T size;
	char *tty;

	if (ttyp) do {
		*ttyp = NULL;
		if (!(tty = (char *)malloc(TIO_BUFSIZ))) break;

		size = (ALLOC_T)0;
		if (tioctl(fd, REQGETP, (termioctl_t *)&(tty[size])) < 0) {
			free(tty);
			break;
		}
# ifdef	USESGTTY
		size += sizeof(termioctl_t);
		if (Xioctl(fd, TIOCLGET, (int *)&(tty[size])) < 0) {
			free(tty);
			break;
		}
		size += sizeof(int);
		if (Xioctl(fd, TIOCGETC, (struct tchars *)&(tty[size])) < 0) {
			free(tty);
			break;
		}
# endif	/* USESGTTY */
		*ttyp = tty;
	} while (0);

	if (wsp) do {
		*wsp = NULL;
# ifndef	NOTERMWSIZE
		if (!(ws = (char *)malloc(TIO_WINSIZ))) break;

		if (Xioctl(fd, REQGETWS, (termwsize_t *)ws) < 0) {
			free(ws);
			break;
		}
		*wsp = ws;
# endif	/* !NOTERMWSIZE */
	} while (0);
}
#endif	/* !MSDOS */

#ifdef	CYGWIN
p_id_t Xfork(VOID_A)
{
	p_id_t pid;
	char *buf;

	/* Cygwin's fork() breaks ISIG */
	if (save_ttyio >= 0) savetermio(save_ttyio, &buf, NULL);
	pid = fork();
	if (save_ttyio >= 0) {
		loadtermio(save_ttyio, buf, NULL);
		if (buf) free(buf);
	}

	return(pid);
}
#endif	/* CYGWIN */
