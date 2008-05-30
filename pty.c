/*
 *	pty.c
 *
 *	pseudo terminal access
 */

#include "fd.h"
#include "sysemu.h"
#include "pathname.h"
#include "termio.h"

#if	!MSDOS
#include <grp.h>
#endif
#ifdef	SYSV
#include <sys/stropts.h>
#endif

#ifdef	GETPGRPVOID
#define	getpgroup		getpgrp
#else
#define	getpgroup()		getpgrp(0)
#endif
#ifdef	USESETPGID
#define	setpgroup		setpgid
#else
#define	setpgroup		setpgrp
#endif

#define	TTY_GROUP		"tty"
#ifndef	_PATH_DEVNULL
#define	_PATH_DEVNULL		"/dev/null"
#endif
#ifndef	_PATH_PTY
#define	_PATH_PTY		"/dev/pty"
#endif
#ifndef	_PATH_DEVPTMX
#define	_PATH_DEVPTMX		"/dev/ptmx"
#endif
#ifndef	_PATH_DEVPTS
#define	_PATH_DEVPTS		"/dev/pts"
#endif

#ifdef	DEP_PTY

static p_id_t NEAR Xsetsid __P_((VOID_A));
static VOID NEAR Xgrantpt __P_((int, CONST char *));
static VOID NEAR Xunlockpt __P_((int, CONST char *));
static int NEAR Xptsname __P_((int, CONST char *, char *, ALLOC_T));

#ifndef	USEDEVPTMX
static CONST char pty_char1[] = "pqrstuvwxyzPQRST";
static CONST char pty_char2[] = "0123456789abcdefghijklmnopqrstuv";
#endif


static p_id_t NEAR Xsetsid(VOID_A)
{
#ifdef	USESETSID
	return(setsid());
#else	/* USESETSID */
# ifdef	TIOCNOTTY
	int fd;
# endif
	p_id_t pid;

	pid = getpid();
	if (pid == getpgroup()) {
		errno = EPERM;
		return((p_id_t)-1);
	}
	if (setpgroup(0, pid) < 0) return((p_id_t)-1);

# ifdef	TIOCNOTTY
	if ((fd = Xopen(_PATH_TTY, O_RDWR, 0)) >= 0) {
		VOID_C Xioctl(fd, TIOCNOTTY, NULL);
		safeclose(fd);
	}
# else
	if ((fd = Xopen(_PATH_DEVNULL, O_RDWR, 0)) >= 0) {
		VOID_C Xdup2(fd, STDIN_FILENO);
		VOID_C Xdup2(fd, STDOUT_FILENO);
		VOID_C Xdup2(fd, STDERR_FILENO);
		safeclose(fd);
	}
# endif

	return(pid);
#endif	/* USESETSID */
}

/*ARGSUSED*/
static VOID NEAR Xgrantpt(fd, path)
int fd;
CONST char *path;
{
#ifdef	USEDEVPTMX
	extern int grantpt __P_((int));		/* for Linux */

	VOID_C grantpt(fd);
#else	/* !USEDEVPTMX */
	struct group *grp;
	gid_t gid;

	gid = ((grp = getgrnam(TTY_GROUP))) ? grp -> gr_gid : (gid_t)-1;

	VOID_C chown(path, getuid(), gid);
	VOID_C chmod(path, 0620);
#endif	/* !USEDEVPTMX */
}

/*ARGSUSED*/
static VOID NEAR Xunlockpt(fd, path)
int fd;
CONST char *path;
{
#if	defined (USEDEVPTMX) && defined (TIOCSPTLCK)
	int n;
#endif

#ifdef	USEDEVPTMX
# ifdef	TIOCSPTLCK
	n = 0;
	VOID_C Xioctl(fd, TIOCSPTLCK, &n);
# else
	VOID_C unlockpt(fd);
# endif
#else	/* !USEDEVPTMX */
# ifdef	BSD44
	VOID_C revoke(path);
# endif
#endif	/* !USEDEVPTMX */
}

/*ARGSUSED*/
static int NEAR Xptsname(fd, path, spath, size)
int fd;
CONST char *path;
char *spath;
ALLOC_T size;
{
#if	!defined (USEDEVPTMX) || !defined (TIOCGPTN)
	char *cp;
#endif
#if	defined (USEDEVPTMX) && defined (TIOCGPTN)
	int n;
#endif

#ifdef	USEDEVPTMX
# ifdef	TIOCGPTN
	if (Xioctl(fd, TIOCGPTN, &n) < 0) return(-1);
	snprintf2(spath, size, "%s/%d", _PATH_DEVPTS, n);
# else
	if (!(cp = ptsname(fd))) return(-1);
	snprintf2(spath, size, "%s", cp);
# endif
#else	/* !USEDEVPTMX */
	snprintf2(spath, size, "%s", path);
	if ((cp = strrchr2(spath, '/'))) *(++cp) = 't';
#endif	/* !USEDEVPTMX */

	return(0);
}

int Xopenpty(amaster, spath, size)
int *amaster;
char *spath;
ALLOC_T size;
{
	char path[MAXPATHLEN];
	int master, slave;

#ifdef	USEDEVPTMX
	snprintf2(path, sizeof(path), "%s", _PATH_DEVPTMX);
	if ((master = Xopen(path, O_RDWR, 0)) < 0) return(-1);

	Xgrantpt(master, path);
	Xunlockpt(master, path);
	if (Xptsname(master, path, spath, size) < 0
	|| (slave = Xopen(spath, O_RDWR | O_NOCTTY, 0)) < 0) {
		safeclose(master);
		return(-1);
	}
#else	/* !USEDEVPTMX */
	CONST char *cp1, *cp2;
	int n;

	n = snprintf2(path, sizeof(path), "%sXX", _PATH_PTY);
	n -= 2;
	master = slave = -1;
	for (cp1 = pty_char1; *cp1; cp1++) {
		path[n] = *cp1;
		for (cp2 = pty_char2; *cp2; cp2++) {
			path[n + 1] = *cp2;
			master = Xopen(path, O_RDWR, 0);
			if (master < 0) {
				if (errno == ENOENT) break;
				continue;
			}

			VOID_C Xptsname(master, path, spath, size);
			Xgrantpt(master, spath);
			Xunlockpt(master, spath);
			slave = Xopen(spath, O_RDWR, 0);
			if (slave >= 0) break;

			safeclose(master);
		}

		if (master >= 0 && slave >= 0) break;
	}

	if (!*cp1) return(seterrno(ENOENT));
#endif	/* !USEDEVPTMX */

	*amaster = master;
	safeclose(slave);

	return(0);
}

#if	defined (IRIX) || defined (DECOSF1V2) || defined (DECOSF1V3)
#undef	I_PUSH
#endif

int Xlogin_tty(path, tty, ws)
CONST char *path, *tty, *ws;
{
	int fd;

	VOID_C Xsetsid();

	Xclose(STDIN_FILENO);
	Xclose(STDOUT_FILENO);
	Xclose(STDERR_FILENO);
	if ((fd = Xopen(path, O_RDWR, 0)) < 0) return(-1);

#ifdef	I_PUSH
	if (Xioctl(fd, I_PUSH, "ptem") < 0
	|| Xioctl(fd, I_PUSH, "ldterm") < 0) {
		Xclose(fd);
		return(-1);
	}
# if	defined (SOLARIS) || defined (NEWS_OS6)
	VOID_C Xioctl(fd, I_PUSH, "ttcompat");
# endif
#endif	/* I_PUSH */
#ifdef	TIOCSCTTY
	if (Xioctl(fd, TIOCSCTTY, NULL) < 0) {
		Xclose(fd);
		return(-1);
	}
#endif

	VOID_C Xdup2(fd, STDIN_FILENO);
	VOID_C Xdup2(fd, STDOUT_FILENO);
	VOID_C Xdup2(fd, STDERR_FILENO);
	loadtermio(fd, tty, ws);
	safeclose(fd);

	return(0);
}

p_id_t Xforkpty(fdp, tty, ws)
int *fdp;
CONST char *tty, *ws;
{
	char path[MAXPATHLEN];
	p_id_t pid;
	u_char uc;
	int n, fds[2];

	if (pipe(fds) < 0) return((p_id_t)-1);

	if (Xopenpty(fdp, path, sizeof(path)) < 0) {
		pid = (p_id_t)-1;
	}
	else if ((pid = Xfork()) < (p_id_t)0) safeclose(*fdp);
	else if (pid) VOID_C sureread(fds[0], &uc, sizeof(uc));
	else {
		safeclose(*fdp);
		n = Xlogin_tty(path, tty, ws);
		uc = '\n';
		VOID_C surewrite(fds[1], &uc, sizeof(uc));
		if (n < 0) _exit(1);
	}

	safeclose(fds[0]);
	safeclose(fds[1]);

	return(pid);
}
#endif	/* DEP_PTY */
