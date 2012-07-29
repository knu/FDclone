/*
 *	termio.c
 *
 *	terminal I/O
 */

#include "headers.h"
#include "depend.h"
#include "typesize.h"
#include "sysemu.h"
#include "termio.h"
#include "unixemu.h"

#ifdef	USESELECTH
#include <sys/select.h>
#endif

#if	MSDOS
# ifdef	USERESOURCEH
# include <sys/time.h>
# include <sys/resource.h>
# endif
#else	/* !MSDOS */
# ifdef	USERESOURCEH
# include <sys/resource.h>
# endif
# ifdef	USEULIMITH
# include <ulimit.h>
# endif
#endif	/* !MSDOS */

#if	defined (USESYSCONF) && defined (_SC_OPEN_MAX)
#define	MAXOPENFILE		sysconf(_SC_OPEN_MAX)
#else	/* !USESYSCONF || !_SC_OPEN_MAX */
# ifdef	NOFILE
# define	MAXOPENFILE	NOFILE
# else	/* !NOFILE */
#  ifdef	OPEN_MAX
#  define	MAXOPENFILE	OPEN_MAX
#  else		/* !OPEN_MAX */
#   if	MSDOS
#   define	MAXOPENFILE	20
#   else
#   define	MAXOPENFILE	64
#   endif
#  endif	/* !OPEN_MAX */
# endif	/* !NOFILE */
#endif	/* !USESYSCONF || !_SC_OPEN_MAX */

#ifndef	UL_GDESLIM
#define	UL_GDESLIM		4
#endif
#ifndef	FD_CLOEXEC
#define	FD_CLOEXEC		1
#endif
#if	!defined (RLIMIT_NOFILE) && defined (RLIMIT_OFILE)
#define	RLIMIT_NOFILE		RLIMIT_OFILE
#endif

#ifdef	USETERMIO
#undef	VSTART
#undef	VSTOP
#endif

#define	K_CTRL(c)		((c) & 037)
#define	MAXFDSET		256
#define	MAXSELECT		16

#ifndef	FD_SET
typedef struct fd_set {
	u_int fds_bits[1];
} fd_set;
# define	FD_SET(n, p)	(((p) -> fds_bits[0]) |= ((u_int)1 << (n)))
# define	FD_ISSET(n, p)	(((p) -> fds_bits[0]) & ((u_int)1 << (n)))
# define	FD_ZERO(p)	(((p) -> fds_bits[0]) = 0)
# undef		MAXFDSET
# define	MAXFDSET	(BITSPERBYTE * sizeof(u_int))
#endif	/* !FD_SET */

#if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
extern int catchsignal __P_((int));
#endif
#if	!MSDOS && defined (FD) && !defined (NOSELECT)
extern VOID checksuspend __P_((VOID));
#endif

#ifdef	DEP_ORIGSHELL
extern int interrupted;
#endif
#ifdef	LSI_C
extern u_char _openfile[];
#endif

#if	MSDOS
static CONST u_short doserrlist[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 17, 18, 65, 80
};
#define	DOSERRLISTSIZ		arraysize(doserrlist)
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
VOID dosseterrno(doserr)
u_int doserr;
{
	int i;

	for (i = 0; i < DOSERRLISTSIZ; i++) if (doserr == doserrlist[i]) {
		errno = unixerrlist[i];
		return;
	}

	errno = EINVAL;
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
	dosseterrno((*regp).x.ax);

	return(-1);
}
#endif	/* MSDOS */

int Xgetdtablesize(VOID_A)
{
#if	!MSDOS
# ifndef	SYSV
	int n;
# else	/* SYSV */
#  ifdef	USERESOURCEH
	struct rlimit lim;
#  else	/* !USERESOURCEH */
#   ifdef	USEULIMITH
	long n;
#   endif
#  endif	/* !USERESOURCEH */
# endif	/* SYSV */
#endif	/* !MSDOS */

#if	!MSDOS && !defined (MINIX3)
# ifndef	SYSV
	if ((n = getdtablesize()) >= 0) return(n);
# else	/* SYSV */
#  ifdef	USERESOURCEH
	if (getrlimit(RLIMIT_NOFILE, &lim) >= 0) return(lim.rlim_cur);
#  else	/* !USERESOURCEH */
#   ifdef	USEULIMITH
	if ((n = ulimit(UL_GDESLIM, 0L)) >= 0L) return((int)n);
#   endif
#  endif	/* !USERESOURCEH */
# endif	/* SYSV */
#endif	/* !MSDOS && !MINIX3 */

	return(MAXOPENFILE);
}

int isvalidfd(fd)
int fd;
{
#if	MSDOS
	struct SREGS sreg;
	__dpmi_regs reg;
#endif

#if	MSDOS
	reg.x.ax = 0x4400;
	reg.x.bx = fd;

	return(intcall(0x21, &reg, &sreg));
#else
	return(fcntl(fd, F_GETFD, NULL));
#endif
}

int newdup(fd)
int fd;
{
	int n;

	if (fd < 0
	|| fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
		return(fd);
#ifdef	DEP_DOSDRIVE
	if (chkopenfd(fd) == DEV_DOS) return(fd);
#endif
	n = Xgetdtablesize();

	for (n--; n > fd; n--) if (isvalidfd(n) < 0) break;
	if (n <= fd || Xdup2(fd, n) < 0) return(fd);
	VOID_C Xclose(fd);

	return(n);
}

int sureread(fd, buf, nbytes)
int fd;
VOID_P buf;
int nbytes;
{
	int n;

#if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
	VOID_C catchsignal(-1);
#endif
	for (;;) {
#ifdef	DEP_ORIGSHELL
		if (interrupted) {
			n = seterrno(EINTR);
			break;
		}
#endif
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
#if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
		else if (catchsignal(1) < 0) break;
#endif
	}

	return(-1);
}

int surewrite(fd, buf, nbytes)
int fd;
CONST VOID_P buf;
int nbytes;
{
	char *cp;
	int n;

	cp = (char *)buf;
#if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
	VOID_C catchsignal(-1);
#endif
	for (;;) {
#ifdef	DEP_ORIGSHELL
		if (interrupted) {
			n = seterrno(EINTR);
			break;
		}
#endif
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
#if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
		else if (catchsignal(1) < 0) break;
#endif
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
		VOID_C Xclose(fd);
	errno = duperrno;
}

VOID safefclose(fp)
XFILE *fp;
{
	int fd, duperrno;

	if (!fp) return;
	duperrno = errno;
	fd = Xfileno(fp);
	if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
		VOID_C Xfclose(fp);
	errno = duperrno;
}

int opentty(fdp, fpp)
int *fdp;
XFILE **fpp;
{
	XFILE *fp;
	int fd, flags;

#ifdef	SELECTRWONLY
	flags = O_RDONLY;
#else
	flags = O_RDWR;
#endif

	if (*fdp >= 0) fd = *fdp;
#ifdef	MINIX
	else if (!isatty(STDIN_FILENO)) return(seterrno(ENOTTY));
	else if ((fd = newdup(Xdup(STDIN_FILENO))) < 0) return(-1);
#else
	else if ((fd = newdup(Xopen(_PATH_TTY, flags, 0666))) < 0) return(-1);
#endif

	if (*fpp) fp = *fpp;
#if	!defined (MINIX3) && !defined (SELECTRWONLY)
	else if ((fp = Xfdopen(fd, "w+b"))) /*EMPTY*/;
#endif
	else if (!(fp = Xfopen(_PATH_TTY, "w+b"))) {
		VOID_C Xclose(fd);
		return(-1);
	}

	*fdp = fd;
	*fpp = fp;
#ifdef	CYGWIN
	save_ttyio = fd;
#endif

	return(0);
}

VOID closetty(fdp, fpp)
int *fdp;
XFILE **fpp;
{
	if (*fpp) {
		if (Xfileno(*fpp) == *fdp) *fdp = -1;
		VOID_C Xfclose(*fpp);
	}
	if (*fdp >= 0) VOID_C Xclose(*fdp);

	*fdp = -1;
	*fpp = NULL;
}

#if	!MSDOS
VOID closeonexec(fd)
int fd;
{
	int n;

	if (fd >= 0 && (n = fcntl(fd, F_GETFD, NULL)) >= 0)
		VOID_C fcntl(fd, F_SETFD, n | FD_CLOEXEC);
}

int Xioctl(fd, request, argp)
int fd;
ioctlreq_t request;
VOID_P argp;
{
# if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
	VOID_C catchsignal(-1);
# endif
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
# if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
		else if (catchsignal(1) < 0) break;
# endif
	}

	return(-1);
}

# ifdef	USETERMIOS
int Xtcgetattr(fd, t)
int fd;
termioctl_t *t;
{
#  if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
	VOID_C catchsignal(-1);
#  endif
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
#  if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
		else if (catchsignal(1) < 0) break;
#  endif
	}

	return(-1);
}

int Xtcsetattr(fd, action, t)
int fd, action;
CONST termioctl_t *t;
{
#  if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
	VOID_C catchsignal(-1);
#  endif
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
#  if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
		else if (catchsignal(1) < 0) break;
#  endif
	}

	return(-1);
}

int Xtcflush(fd, selector)
int fd, selector;
{
#  if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
	VOID_C catchsignal(-1);
#  endif
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
#  if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
		else if (catchsignal(1) < 0) break;
#  endif
	}

	return(-1);
}
# endif	/* USETERMIOS */
#endif	/* !MSDOS */

#if	defined (FD) || defined (CYGWIN) || !defined (NOJOB)
/*ARGSUSED*/
VOID loadtermio(fd, tty, ws)
int fd;
CONST char *tty, *ws;
{
# if	MSDOS
#  ifndef	DJGPP
	union REGS reg;
#  endif
# else	/* !MSDOS */
	ALLOC_T size;
# endif	/* !MSDOS */

	if (fd < 0) return;
# if	MSDOS
#  ifndef	DJGPP
	if (tty) {
		reg.x.ax = 0x3301;
		memcpy((char *)&(reg.h.dl), (char *)tty, sizeof(reg.h.dl));
		int86(0x21, &reg, &reg);
	}
#  endif
# else	/* !MSDOS */
	if (tty) {
		size = (ALLOC_T)0;
		VOID_C tioctl(fd, REQSETN, (termioctl_t *)&(tty[size]));
#  ifdef	USESGTTY
		size += sizeof(termioctl_t);
		VOID_C Xioctl(fd, TIOCLSET, (int *)&(tty[size]));
		size += sizeof(int);
		VOID_C Xioctl(fd, TIOCSETC, (struct tchars *)&(tty[size]));
#  endif
	}

#  ifndef	NOTERMWSIZE
	if (ws) VOID_C Xioctl(fd, REQSETWS, (termwsize_t *)ws);
#  endif
# endif	/* !MSDOS */
}

VOID savetermio(fd, ttyp, wsp)
int fd;
char **ttyp, **wsp;
{
# if	MSDOS
#  ifndef	DJGPP
	union REGS reg;
	char *tty;
#  endif
# else	/* !MSDOS */
#  ifndef	NOTERMWSIZE
	char *ws;
#  endif
	ALLOC_T size;
	char *tty;
# endif	/* !MSDOS */

	if (fd < 0) return;
# if	MSDOS
	if (ttyp) do {
		*ttyp = NULL;
#  ifndef	DJGPP
		if (!(tty = (char *)malloc(TIO_BUFSIZ))) break;

		reg.x.ax = 0x3300;
		int86(0x21, &reg, &reg);
		memcpy((char *)tty, (char *)&(reg.h.dl), TIO_BUFSIZ);
		*ttyp = tty;
#  endif	/* !DJGPP */
	} while (0);

	if (wsp) *wsp = NULL;
# else	/* !MSDOS */
	if (ttyp) do {
		*ttyp = NULL;
		if (!(tty = (char *)malloc(TIO_BUFSIZ))) break;

		size = (ALLOC_T)0;
		if (tioctl(fd, REQGETP, (termioctl_t *)&(tty[size])) < 0) {
			free(tty);
			break;
		}
#  ifdef	USESGTTY
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
#  endif	/* USESGTTY */
		*ttyp = tty;
	} while (0);

	if (wsp) do {
		*wsp = NULL;
#  ifndef	NOTERMWSIZE
		if (!(ws = (char *)malloc(TIO_WINSIZ))) break;

		if (Xioctl(fd, REQGETWS, (termwsize_t *)ws) < 0) {
			free(ws);
			break;
		}
		*wsp = ws;
#  endif	/* !NOTERMWSIZE */
	} while (0);
# endif	/* !MSDOS */
}
#endif	/* FD || CYGWIN || !NOJOB */

#ifdef	CYGWIN
p_id_t Xfork(VOID_A)
{
	p_id_t pid;
	char *buf;

	/* Cygwin's fork() breaks ISIG */
	savetermio(save_ttyio, &buf, NULL);
	pid = fork();
	loadtermio(save_ttyio, buf, NULL);
	if (buf) free(buf);

	return(pid);
}
#endif	/* CYGWIN */

#ifndef	NOSELECT
int sureselect(nfd, fds, result, vp, flags)
int nfd, fds[];
char result[];
VOID_P vp;
int flags;
{
	fd_set *readfds, *writefds, set;
	int i, n, max, dupfds[MAXSELECT];

	if (flags & SEL_WRITE) {
		readfds = NULL;
		writefds = &set;
	}
	else {
		readfds = &set;
		writefds = NULL;
	}
	FD_ZERO(&set);
	max = -1;
	if (nfd > MAXSELECT) nfd = MAXSELECT;

	for (i = 0; i < nfd; i++) {
		dupfds[i] = fds[i];
		if (result) result[i] = 0;
		if (fds[i] < 0) continue;
		if (fds[i] >= MAXFDSET && (n = safe_dup(fds[i])) >= 0)
			dupfds[i] = n;
		if (dupfds[i] > max) max = dupfds[i];
		FD_SET(dupfds[i], &set);
	}
	if (max++ < 0) return(0);

# if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
	VOID_C catchsignal(-1);
# endif
	for (;;) {
# ifdef	DEP_ORIGSHELL
		if (interrupted) {
			n = seterrno(EINTR);
			break;
		}
# endif
# if	!MSDOS && defined (FD)
		if (flags & SEL_TTYIO) checksuspend();
# endif
		n = select(max, readfds, writefds, NULL, (struct timeval *)vp);
		if (n >= 0 || errno != EINTR) break;
# if	defined (DEP_ORIGSHELL) && !defined(MINIMUMSHELL)
		else if (catchsignal(1) < 0) break;
# endif
		if (flags & SEL_NOINTR) break;
	}
	for (i = 0; i < nfd; i++)
		if (dupfds[i] != fds[i]) safeclose(dupfds[i]);
	if (n <= 0) return(n);

	if (result) for (i = 0; i < nfd; i++) {
		if (dupfds[i] < 0) continue;
		if (FD_ISSET(dupfds[i], &set)) result[i] = 1;
	}

	return(n);
}
#endif	/* !NOSELECT */
