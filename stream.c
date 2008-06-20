/*
 *	stream.c
 *
 *	stream I/O functions
 */

#include "headers.h"
#include "depend.h"
#include "typesize.h"
#include "malloc.h"
#include "sysemu.h"
#include "termio.h"
#include "stream.h"

#ifdef	DEP_SOCKET
#include "socket.h"
#endif
#if	defined (FD) && !defined (DEP_ORIGSHELL)
#include "string.h"
#include "pathname.h"
#include "wait.h"
#include "dosdisk.h"
#include "unixdisk.h"
#endif

#define	PIPEDIR			"PIPE_DIR"
#define	PIPEFILE		"FAKEPIPE"

#if	defined (FD) && !defined (DEP_ORIGSHELL)
# ifdef	USESIGACTION
extern sigcst_t signal2 __P_((int, sigcst_t));
# else
#define	signal2			signal
# endif
# ifdef	DEP_DOSLFN
extern char *preparefile __P_((CONST char *, char *));
# endif
#endif	/* FD && !DEP_ORIGSHELL */

#ifdef	DEP_DOSDRIVE
extern int dosflushbuf __P_((int));
#endif
#if	defined (FD) && !defined (DEP_ORIGSHELL)
extern int mktmpdir __P_((char *));
extern int rmtmpdir __P_((CONST char *));
#endif

#if	defined (FD) && !defined (DEP_ORIGSHELL)
extern char *deftmpdir;
extern char *tmpfilename;
#endif

#ifdef	DEP_ORIGSTREAM
static int NEAR mode2flags __P_((CONST char *));
static XFILE *NEAR fmalloc __P_((int, int));
static int NEAR checkfp __P_((XFILE *, int));
static int NEAR fillbuf __P_((XFILE *));
# ifdef	DEP_SOCKET
static int NEAR replytelnet __P_((XFILE *));
# endif
#endif	/* DEP_ORIGSTREAM */
#if	!defined (FD) && !defined (DEP_ORIGSTREAM)
static char *NEAR fgets2 __P_((int));
#endif

#ifdef	DEP_ORIGSTREAM
static XFILE stdiobuf[] = {
	{STDIN_FILENO, "", (ALLOC_T)0, (ALLOC_T)0,
		XF_RDONLY},
	{STDOUT_FILENO, "", (ALLOC_T)0, (ALLOC_T)0,
		XF_WRONLY | XF_LINEBUF},
	{STDERR_FILENO, "", (ALLOC_T)0, (ALLOC_T)0,
		XF_WRONLY | XF_NOBUF},
};
#define	STDIOBUFSIZ		arraysize(stdiobuf)
#endif	/* DEP_ORIGSTREAM */
#if	defined (FD) && !defined (DEP_ORIGSHELL)
# if	!MSDOS
static p_id_t popenpid = (p_id_t)-1;
# endif
static int popenstat = 0;
#endif	/* FD && !DEP_ORIGSHELL */

#ifdef	DEP_ORIGSTREAM
XFILE *Xstdin = &(stdiobuf[0]);
XFILE *Xstdout = &(stdiobuf[1]);
XFILE *Xstderr = &(stdiobuf[2]);
#endif


#ifdef	DEP_ORIGSTREAM
static int NEAR mode2flags(mode)
CONST char *mode;
{
	int flags;

	if (!mode) return(0);
	flags = 0;

	switch (*(mode++)) {
		case 'r':
			flags |= O_RDONLY;
			break;
		case 'w':
			flags |= (O_WRONLY | O_CREAT | O_TRUNC);
			break;
		case 'a':
			flags |= (O_WRONLY | O_APPEND | O_CREAT);
			break;
		default:
			return(0);
/*NOTREACHED*/
			break;
	}
	if (*mode == '+') {
		flags &= ~O_ACCMODE;
		flags |= O_RDWR;
		mode++;
	}
	flags |= (*mode == 'b') ? O_BINARY : O_TEXT;

	return(flags);
}

static XFILE *NEAR fmalloc(fd, flags)
int fd, flags;
{
	XFILE *fp;

	fp = (XFILE *)malloc2(sizeof(XFILE));

	fp -> fd = fd;
#if	defined (DEP_URLPATH) && !defined (NOSELECT)
	fp -> timeout = -1;
#endif
	fp -> ptr = fp -> count = (ALLOC_T)0;
	fp -> flags = 0;

	switch (flags & O_ACCMODE) {
		case O_RDONLY:
			fp -> flags |= XF_RDONLY;
			break;
		case O_WRONLY:
			fp -> flags |= XF_WRONLY;
			break;
		default:
			break;
	}

	return(fp);
}

XFILE *Xfopen(path, mode)
CONST char *path, *mode;
{
	XFILE *fp;
	int fd, flags;

	flags = mode2flags(mode);
	if ((fd = Xopen(path, flags, 0666)) < 0) return(NULL);
	if (!(fp = fmalloc(fd, flags))) {
		VOID_C Xclose(fd);
		return(NULL);
	}

	return(fp);
}

XFILE *Xfdopen(fd, mode)
int fd;
CONST char *mode;
{
#if	!MSDOS
	int n;
#endif
	int flags;

	flags = mode2flags(mode);
#if	!MSDOS
	n = fcntl(fd, F_GETFL);
	if (n < 0) /*EMPYU*/;
	else if ((flags & O_APPEND) != (n & O_APPEND)) n = seterrno(EINVAL);
	else switch (n & O_ACCMODE) {
		case O_RDONLY:
			if ((flags & O_ACCMODE) != O_RDONLY)
				n = seterrno(EINVAL);
			break;
		case O_WRONLY:
			if ((flags & O_ACCMODE) == O_RDONLY)
				n = seterrno(EINVAL);
			break;
		default:
			break;
	}
	if (n < 0) return(NULL);
#endif

	return(fmalloc(fd, flags));
}

int Xfclose(fp)
XFILE *fp;
{
	int i, n, duperrno;

	if (!fp) {
		errno = EINVAL;
		return(EOF);
	}
	if (fp -> flags & XF_CLOSED) return(0);

	duperrno = errno;
	if ((n = Xfflush(fp)) == EOF) duperrno = errno;
	if (Xclose(fp -> fd) < 0) {
		n = EOF;
		duperrno = errno;
	}
	for (i = 0; i < STDIOBUFSIZ; i++) if (fp == &(stdiobuf[i])) break;
	if (i >= STDIOBUFSIZ) free2(fp);
	else {
		fp -> flags |= XF_CLOSED;
		fp -> flags &= ~(XF_EOF | XF_ERROR | XF_READ | XF_WRITTEN);
		fp -> ptr = fp -> count = (ALLOC_T)0;
	}
	errno = duperrno;

	return(n);
}

VOID Xclearerr(fp)
XFILE *fp;
{
	if (fp) fp -> flags &= ~(XF_EOF | XF_ERROR);
}

int Xfeof(fp)
XFILE *fp;
{
	return((fp) ? fp -> flags & (XF_EOF | XF_CLOSED) : 0);
}

int Xferror(fp)
XFILE *fp;
{
	return((fp) ? fp -> flags & XF_ERROR : 0);
}

int Xfileno(fp)
XFILE *fp;
{
	return((fp) ? fp -> fd : seterrno(EBADF));
}

VOID Xsetflags(fp, flags)
XFILE *fp;
int flags;
{
	if (fp) fp -> flags |= flags;
}

#if	defined (DEP_URLPATH) && !defined (NOSELECT)
VOID Xsettimeout(fp, timeout)
XFILE *fp;
int timeout;
{
	if (fp) fp -> timeout = timeout;
}
#endif

static int NEAR checkfp(fp, flags)
XFILE *fp;
int flags;
{
	if (!fp) return(seterrno(EINVAL));
	if (((flags & XF_RDONLY) && (fp -> flags & XF_WRONLY))
	|| ((flags & XF_WRONLY) && (fp -> flags & XF_RDONLY))) {
		fp -> flags |= XF_ERROR;
		return(seterrno(EBADF));
	}

	return(0);
}

static int NEAR fillbuf(fp)
XFILE *fp;
{
	ALLOC_T len;
	int n;

	if (checkfp(fp, XF_RDONLY) < 0) return(-1);
	if ((fp -> flags & XF_WRITTEN) && Xfflush(fp) == EOF) return(-1);
	if (fp == Xstdin) {
		if (Xstdout -> flags & XF_LINEBUF) VOID_C Xfflush(Xstdout);
		if (Xstderr -> flags & XF_LINEBUF) VOID_C Xfflush(Xstderr);
	}

	fp -> ptr = (ALLOC_T)0;
	len = (fp -> flags & XF_NOBUF) ? 1 : XF_BUFSIZ;
	n = Xread(fp -> fd, fp -> buf, len);
	if (n <= 0) {
		if (n < 0) fp -> flags |= XF_ERROR;
		else {
			fp -> flags |= XF_EOF;
			fp -> flags &= ~XF_READ;
		}
		fp -> count = (ALLOC_T)0;
		return(-1);
	}

	fp -> count = (ALLOC_T)n;
	fp -> flags |= XF_READ;

	return(0);
}

int Xfflush(fp)
XFILE *fp;
{
	ALLOC_T len;

	fp -> flags &= ~XF_WRITTEN;
	if (fp -> flags & XF_RDONLY) return(0);

	len = fp -> ptr;
	fp -> ptr = (ALLOC_T)0;
	fp -> count = (fp -> flags & XF_NOBUF) ? 1 : XF_BUFSIZ;
	if (!len || (fp -> flags & XF_CLOSED)) return(0);

	if (Xwrite(fp -> fd, fp -> buf, len) != len) {
		fp -> flags |= XF_ERROR;
		return(EOF);
	}
#ifdef	DEP_DOSDRIVE
	if (chkopenfd(fp -> fd) == DEV_DOS && dosflushbuf(fp -> fd) < 0) {
		fp -> flags |= XF_ERROR;
		return(EOF);
	}
#endif

	return(0);
}

int Xfread(buf, size, fp)
char *buf;
ALLOC_T size;
XFILE *fp;
{
	ALLOC_T len;
	int total;

	if (checkfp(fp, XF_RDONLY) < 0) return(-1);
	total = 0;

	while (size > (ALLOC_T)0) {
		if (fp -> flags & (XF_EOF | XF_CLOSED)) break;
		if (!(fp -> count) && fillbuf(fp) < 0) break;
		len = fp -> count;
		if (len > size) len = size;
		memcpy(buf, &(fp -> buf[fp -> ptr]), len);
		fp -> ptr += len;
		buf += len;
		size -= len;
		fp -> count -= len;
		total += len;
	}

	return((fp -> flags & XF_ERROR) ? -1 : total);
}

int Xfwrite(buf, size, fp)
CONST char *buf;
ALLOC_T size;
XFILE *fp;
{
	ALLOC_T len;

	if (checkfp(fp, XF_WRONLY) < 0) return(-1);
	if (fp -> flags & XF_READ) fp -> flags &= ~(XF_EOF | XF_READ);
	if (fp -> flags & XF_CLOSED) return(0);

	while (size > (ALLOC_T)0) {
		len = fp -> count;
		if (len > size) len = size;
		memcpy(&(fp -> buf[fp -> ptr]), buf, len);
		fp -> ptr += len;
		buf += len;
		size -= len;
		fp -> count -= len;
		fp -> flags |= XF_WRITTEN;
		if (fp -> count <= (ALLOC_T)0 && Xfflush(fp) == EOF) break;
	}

	if (fp -> flags & XF_ERROR) return(-1);
	if (fp -> flags & XF_LINEBUF) {
		for (len = fp -> ptr; len > (ALLOC_T)0; len--)
			if (fp -> buf[len - 1] == '\n') break;
		if (len <= (ALLOC_T)0) /*EMPTY*/;
		else if (Xwrite(fp -> fd, fp -> buf, len) < 0) {
			fp -> flags |= XF_ERROR;
			return(-1);
		}
		else {
			fp -> ptr -= len;
			memmove(fp -> buf, &(fp -> buf[len]), fp -> ptr);
		}
	}

	return(0);
}

int Xfgetc(fp)
XFILE *fp;
{
# if	defined (DEP_URLPATH) && !defined (NOSELECT)
	int n;
# endif
	u_char uc;

# if	defined (DEP_URLPATH) && !defined (NOSELECT)
	if (fp -> flags & XF_NONBLOCK) {
		if (checkfp(fp, XF_RDONLY) < 0) return(EOF);
		if (fp -> flags & (XF_EOF | XF_CLOSED)) return(EOF);
		n = checkread(fp -> fd, &uc, sizeof(uc), fp -> timeout);
		if (n <= 0) {
			if (n < 0) fp -> flags |= XF_ERROR;
			else {
				fp -> flags |= XF_EOF;
				fp -> flags &= ~XF_READ;
			}
			fp -> ptr = fp -> count = (ALLOC_T)0;
			return(EOF);
		}
	}
	else
# endif	/* DEP_URLPATH && !NOSELECT */
	if (Xfread((char *)&uc, sizeof(uc), fp) <= 0) return(EOF);

	return((int)uc);
}

int Xfputc(c, fp)
int c;
XFILE *fp;
{
	u_char uc;

	uc = (u_char)c;
	if (Xfwrite((char *)&uc, sizeof(uc), fp) < 0) return(EOF);

	return(c);
}

#ifdef	DEP_SOCKET
static int NEAR replytelnet(fp)
XFILE *fp;
{
	u_char buf[3];
	int c, s;

	s = Xfileno(fp);
	if (issocket(s) < 0) return(0);

	c = Xfgetc(fp);
	switch (c) {
		case TELNET_WILL:
		case TELNET_WONT:
			c = Xfgetc(fp);
			buf[0] = TELNET_IAC;
			buf[1] = TELNET_DONT;
			buf[2] = c;
			VOID_C Xwrite(s, (char *)buf, sizeof(buf));
			break;
		case TELNET_DO:
		case TELNET_DONT:
			c = Xfgetc(fp);
			buf[0] = TELNET_IAC;
			buf[1] = TELNET_WONT;
			buf[2] = c;
			VOID_C Xwrite(s, (char *)buf, sizeof(buf));
			break;
		case TELNET_IAC:
			return(c);
/*NOTREACHED*/
			break;
		default:
			break;
	}

	return(0);
}
#endif	/* DEP_SOCKET */

char *Xfgets(fp)
XFILE *fp;
{
	char *cp;
	ALLOC_T i, size;
	int c;

	cp = c_realloc(NULL, 0, &size);
	for (i = 0;; i++) {
		c = Xfgetc(fp);
		if (c == EOF) {
			if (!i || (fp -> flags & XF_ERROR)) {
				free2(cp);
				return(NULL);
			}
			break;
		}
		if (c == '\n') break;
#ifdef	DEP_SOCKET
		if ((fp -> flags & XF_TELNET) && c == TELNET_IAC) {
			c = replytelnet(fp);
			if (!c) continue;
		}
#endif
		cp = c_realloc(cp, i, &size);
		if ((fp -> flags & XF_NULLCONV) && !c) c = '\n';
		cp[i] = c;
	}
#ifndef	USECRNL
	if (!(fp -> flags & XF_CRNL)) /*EMPTY*/;
	else
#endif
	if (i > 0 && cp[i - 1] == '\r') i--;
	cp[i++] = '\0';

	return(realloc2(cp, i));
}

int Xfputs(s, fp)
CONST char *s;
XFILE *fp;
{
	if (Xfwrite(s, strlen(s), fp) < 0) return(EOF);

	return(0);
}

VOID Xsetbuf(fp)
XFILE *fp;
{
	if (fp) {
		fp -> flags |= XF_NOBUF;
		fp -> flags &= ~XF_LINEBUF;
		fp -> ptr = fp -> count = (ALLOC_T)0;
	}
}

VOID Xsetlinebuf(fp)
XFILE *fp;
{
	if (fp && (fp == Xstdout || fp == Xstderr)) {
		VOID_C Xfflush(fp);
		fp -> flags |= XF_LINEBUF;
		fp -> flags &= ~XF_NOBUF;
	}
}
#endif	/* DEP_ORIGSTREAM */

#if	defined (FD) && !defined (DEP_ORIGSHELL)
XFILE *Xpopen(command, mode)
CONST char *command, *mode;
{
# if	!MSDOS
	extern char **environ;
	int fds[2];
# endif
# ifdef	DEP_DOSLFN
	char *cp, tmp[MAXPATHLEN];
# endif
	char *cmdline, path[MAXPATHLEN];
	int l1, l2;

# if	!MSDOS
	if (pipe(fds) < 0) popenpid = (p_id_t)-1;
	else if ((popenpid = Xfork()) < (p_id_t)0) {
		safeclose(fds[0]);
		safeclose(fds[1]);
	}
	else if (popenpid) {
		safeclose(fds[1]);
		return(Xfdopen(fds[0], mode));
	}
	else {
		Xdup2(fds[1], STDOUT_FILENO);
		safeclose(fds[0]);
		safeclose(fds[1]);
		execle("/bin/sh", "sh", "-c", command, NULL, environ);
		_exit(1);
	}
# endif	/* !MSDOS */

	strcpy2(path, PIPEDIR);
	if (mktmpdir(path) < 0) return(NULL);
# ifdef	DEP_DOSLFN
	if (!(cp = preparefile(path, tmp))) return(NULL);
	else if (cp == tmp) strcpy2(path, tmp);
# endif
	strcpy2(strcatdelim(path), PIPEFILE);

	l1 = strlen(command);
	l2 = strlen(path);
	cmdline = malloc2(l1 + l2 + 3 + 1);
	strncpy2(cmdline, command, l1);
	strcpy2(&(cmdline[l1]), " > ");
	l1 += 3;
	strcpy2(&(cmdline[l1]), path);
	popenstat = system(cmdline);

	return(Xfopen(path, mode));
}

int Xpclose(fp)
XFILE *fp;
{
# if	!MSDOS
	sigcst_t ivec, qvec, hvec;
	wait_pid_t w;
	p_id_t pid;
# endif
	char *cp, path[MAXPATHLEN];
	int duperrno;

	duperrno = 0;
	if (Xfclose(fp)) duperrno = errno;

# if	!MSDOS
	if (popenpid >= (p_id_t)0) {
		if (!popenpid) popenstat = 0;
		else {
			ivec = signal2(SIGINT, SIG_IGN);
			qvec = signal2(SIGQUIT, SIG_IGN);
			hvec = signal2(SIGHUP, SIG_IGN);
			VOID_C kill(popenpid, SIGPIPE);
			for (;;) {
				pid = Xwait4(popenpid, &w, WUNTRACED, NULL);
				if (pid == popenpid) break;
				else if (pid < (p_id_t)0) {
					duperrno = errno;
					break;
				}
			}
			VOID_C signal2(SIGINT, ivec);
			VOID_C signal2(SIGQUIT, qvec);
			VOID_C signal2(SIGHUP, hvec);

			if (duperrno) popenstat = seterrno(duperrno);
			else if (WIFSTOPPED(w)) popenstat = 128 + WSTOPSIG(w);
			else if (WIFSIGNALED(w)) popenstat = 128 + WTERMSIG(w);
			else popenstat = WEXITSTATUS(w);
		}
		popenpid = (p_id_t)-1;
		return(popenstat);
	}
# endif	/* !MSDOS */

	if (!deftmpdir || !*deftmpdir || !tmpfilename || !*tmpfilename)
		return(seterrno(ENOENT));
	strcatdelim2(path, deftmpdir, tmpfilename);
	strcpy2(strcatdelim(path), PIPEDIR);
	strcpy2((cp = strcatdelim(path)), PIPEFILE);

	if (Xunlink(path) < 0) duperrno = errno;
	*(--cp) = '\0';
	if (rmtmpdir(path) < 0) duperrno = errno;
	if (duperrno) return(seterrno(duperrno));

	return(popenstat);
}
#endif	/* FD && !DEP_ORIGSHELL */

#ifndef	FD
# ifndef	DEP_ORIGSTREAM
static char *NEAR fgets2(fd)
int fd;
{
	char *cp;
	ALLOC_T i, size;
	u_char uc;
	int n;

	cp = c_realloc(NULL, 0, &size);
	for (i = 0;; i++) {
		n = read(fd, &uc, sizeof(uc));
		if (n <= 0) {
			if (!i || n < 0) {
				free2(cp);
				return(NULL);
			}
			break;
		}
		if (uc == '\n') break;
		cp = c_realloc(cp, i, &size);
		cp[i] = uc;
	}
	cp[i++] = '\0';

	return(realloc2(cp, i));
}
# endif	/* !DEP_ORIGSTREAM */

char *gets2(prompt)
CONST char *prompt;
{
# ifdef	DEP_ORIGSTREAM
#  ifdef	DEP_ORIGSHELL
	extern int ttyio;
	int fd;
#  endif
	XFILE *fp;
# endif	/* DEP_ORIGSTREAM */
	char *cp;

	Xfputs(prompt, Xstderr);
	VOID_C Xfflush(Xstderr);

# ifdef	DEP_ORIGSTREAM
#  ifdef	DEP_ORIGSHELL
	fd = safe_dup(ttyio);
	fp = (fd >= 0) ? Xfdopen(fd, "rb") : NULL;
	if (fp) /*EMPTY*/;
	else
#  endif
	fp = Xstdin;
	cp = Xfgets(fp);
	safefclose(fp);
# else	/* !DEP_ORIGSTREAM */
	cp = fgets2(STDIN_FILENO);
# endif	/* !DEP_ORIGSTREAM */

	return(cp);
}
#endif	/* !FD */
