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
#include "pathname.h"
#include "termio.h"
#include "stream.h"

#ifdef	DEP_STREAMSOCKET
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
#ifdef	USECRNL
#define	ISCRNL(fp)		(!((fp) -> status & XS_BINARY) \
				|| ((fp) -> flags & XF_CRNL))
#else
#define	ISCRNL(fp)		((fp) -> flags & XF_CRNL)
#endif

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
static int NEAR mode2flags __P_((CONST char *, int *));
# ifdef	DEP_STREAMLOG
static XFILE *NEAR fmalloc __P_((int, int, int, int, CONST char *));
# else
static XFILE *NEAR fmalloc __P_((int, int, int));
# endif
static int NEAR checkfp __P_((XFILE *, int));
# ifdef	DEP_STREAMLOG
static VOID NEAR dumplog __P_((CONST char *, ALLOC_T, XFILE *));
# endif
static int NEAR fillbuf __P_((XFILE *));
static int NEAR flushbuf __P_((XFILE *));
# ifdef	DEP_STREAMSOCKET
static int NEAR replytelnet __P_((XFILE *));
# endif
#endif	/* DEP_ORIGSTREAM */
#if	!defined (FD) && !defined (DEP_ORIGSTREAM)
static char *NEAR fgets2 __P_((int));
#endif

#ifdef	DEP_ORIGSTREAM
static XFILE stdiobuf[] = {
	{STDIN_FILENO, XS_RDONLY, 0},
	{STDOUT_FILENO, XS_WRONLY, XF_LINEBUF},
	{STDERR_FILENO, XS_WRONLY, XF_NOBUF},
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
int (*stream_isnfsfunc)__P_((CONST char *)) = NULL;
XFILE *Xstdin = &(stdiobuf[0]);
XFILE *Xstdout = &(stdiobuf[1]);
XFILE *Xstderr = &(stdiobuf[2]);
#endif


#ifdef	DEP_ORIGSTREAM
static int NEAR mode2flags(mode, flagsp)
CONST char *mode;
int *flagsp;
{
	int flags;

	if (!mode) {
		errno = EINVAL;
		return(-1);
	}
	flags = 0;
	if (flagsp) flags |= (*flagsp & ~O_ACCMODE);

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
			errno = EINVAL;
			return(-1);
/*NOTREACHED*/
			break;
	}
	if (*mode == '+') {
		flags &= ~O_ACCMODE;
		flags |= O_RDWR;
		mode++;
	}
	flags |= O_BINARY;
	if (flagsp) *flagsp = flags;

	return((*mode == 'b') ? 1 : 0);
}

# ifdef	DEP_STREAMLOG
static XFILE *NEAR fmalloc(fd, flags, isbin, timeout, path)
int fd, flags, isbin, timeout;
CONST char *path;
# else
static XFILE *NEAR fmalloc(fd, flags, isbin)
int fd, flags, isbin;
# endif
{
	XFILE *fp;
	ALLOC_T len;

	len = sizeof(XFILE);
# ifdef	DEP_STREAMLOG
	if (!path) path = nullstr;
	len += strlen(path);
# endif
	fp = (XFILE *)Xmalloc(len);

	fp -> fd = fd;
# ifdef	DEP_STREAMTIMEOUT
#  ifdef	DEP_STREAMLOG
	fp -> timeout = timeout;
#  else
	fp -> timeout = -1;
#  endif
# endif	/* DEP_STREAMTIMEOUT */
	fp -> ptr = fp -> count = (ALLOC_T)0;
	fp -> status = fp -> flags = 0;
# ifdef	DEP_STREAMLOG
	fp -> dumpfunc = NULL;
	fp -> debuglvl = 0;
	fp -> debugmes = NULL;
	Xstrcpy(fp -> path, path);
# endif

	switch (flags & O_ACCMODE) {
		case O_RDONLY:
			fp -> status |= XS_RDONLY;
			break;
		case O_WRONLY:
			fp -> status |= XS_WRONLY;
			break;
		default:
			break;
	}
	if (isbin) fp -> status |= XS_BINARY;

	return(fp);
}

XFILE *Xfopen(path, mode)
CONST char *path, *mode;
{
# ifdef	DEP_STREAMLOCK
#  ifndef	NOFTRUNCATE
	int trunc;
#  endif
	int isnfs, operation;
# endif	/* DEP_STREAMLOCK */
	XFILE *fp;
	int fd, flags, isbin;

	flags = 0;
	if ((isbin = mode2flags(mode, &flags)) < 0) return(NULL);
# ifdef	DEP_STREAMLOCK
	isnfs = (stream_isnfsfunc) ? (*stream_isnfsfunc)(path) : 1;
#  ifndef	NOFTRUNCATE
	if (isnfs) trunc = 0;
	else {
		trunc = (flags & O_TRUNC);
		flags &= ~O_TRUNC;
	}
#  endif
# endif	/* DEP_STREAMLOCK */
	fd = Xopen(path, flags, 0666);
	if (fd < 0) return(NULL);

# ifdef	DEP_STREAMLOG
	fp = fmalloc(fd, flags, isbin, -1, path);
# else
	fp = fmalloc(fd, flags, isbin);
# endif
	if (!fp) {
		VOID_C Xclose(fd);
		return(NULL);
	}

# ifdef	DEP_STREAMLOCK
	if (!isnfs) {
		operation = ((flags & O_ACCMODE) == O_RDONLY)
			? LOCK_SH : LOCK_EX;
		if (Xflock(fd, operation | LOCK_NB) < 0) {
			VOID_C Xfclose(fp);
			return(NULL);
		}
		fp -> status |= XS_LOCKED;
#  ifndef	NOFTRUNCATE
		if (trunc && Xftruncate(fd, (off_t)0) < 0) {
			VOID_C Xfclose(fp);
			return(NULL);
		}
#  endif
	}
# endif	/* DEP_STREAMLOCK */

	return(fp);
}

XFILE *Xfdopen(fd, mode)
int fd;
CONST char *mode;
{
# if	!MSDOS
	int n;
# endif
	XFILE *fp;
	int flags, isbin;

	flags = 0;
	if ((isbin = mode2flags(mode, &flags)) < 0) return(NULL);
# if	!MSDOS
	n = fcntl(fd, F_GETFL);
	if (n < 0) return(NULL);
	if ((flags & O_APPEND) != (n & O_APPEND)) {
		errno = EINVAL;
		return(NULL);
	}
	switch (n & O_ACCMODE) {
		case O_RDONLY:
			if ((flags & O_ACCMODE) != O_RDONLY) {
				errno = EINVAL;
				return(NULL);
			}
			break;
		case O_WRONLY:
			if ((flags & O_ACCMODE) != O_WRONLY) {
				errno = EINVAL;
				return(NULL);
			}
			break;
		default:
			break;
	}
# endif	/* !MSDOS */

# ifdef	DEP_STREAMLOG
	fp = fmalloc(fd, flags, isbin, -1, NULL);
# else
	fp = fmalloc(fd, flags, isbin);
# endif
	if (!fp) return(NULL);

	return(fp);
}

int Xfclose(fp)
XFILE *fp;
{
	int i, duperrno;

	if (!fp) {
		errno = EINVAL;
		return(EOF);
	}
	if (fp -> status & XS_CLOSED) return(0);

	duperrno = errno;
	if (flushbuf(fp) < 0) return(EOF);

# ifdef	DEP_STREAMLOCK
	if (fp -> status & XS_LOCKED) {
		VOID_C Xlseek(fp -> fd, (off_t)0, L_XTND);
		VOID_C Xflock(fp -> fd, LOCK_UN);
	}
# endif	/* DEP_STREAMLOCK */
	if (!(fp -> flags & XF_NOCLOSE)) {
# ifdef	DEP_STREAMSOCKET
		if (fp -> flags & XF_CONNECTED) shutdown(fp -> fd, SHUT_RDWR);
# endif
		if (Xclose(fp -> fd) < 0) return(EOF);
	}
	for (i = 0; i < STDIOBUFSIZ; i++) if (fp == &(stdiobuf[i])) break;
	if (i >= STDIOBUFSIZ) Xfree(fp);
	else {
		fp -> status |= XS_CLOSED;
		fp -> status &= ~(XS_EOF | XS_ERROR | XS_READ | XS_WRITTEN);
		fp -> ptr = fp -> count = (ALLOC_T)0;
	}
	errno = duperrno;

	return(0);
}

static int NEAR checkfp(fp, status)
XFILE *fp;
int status;
{
	if (!fp) {
		errno = EINVAL;
		return(-1);
	}
	if ((status & XS_ERROR) && (fp -> status & XS_ERROR)) return(-1);
	if ((status & XS_RDONLY) && (fp -> status & XS_WRONLY)) {
		errno = EBADF;
		fp -> status |= XS_ERROR;
		return(-1);
	}
	if ((status & XS_WRONLY) && (fp -> status & XS_RDONLY)) {
		errno = EBADF;
		fp -> status |= XS_ERROR;
		return(-1);
	}

	return(0);
}

VOID Xclearerr(fp)
XFILE *fp;
{
	if (checkfp(fp, 0) < 0) return;
	fp -> status &= ~(XS_EOF | XS_ERROR);
}

int Xfeof(fp)
XFILE *fp;
{
	if (checkfp(fp, 0) < 0) return(-1);

	return(fp -> status & (XS_EOF | XS_CLOSED));
}

int Xferror(fp)
XFILE *fp;
{
	if (checkfp(fp, 0) < 0) return(-1);

	return(fp -> status & XS_ERROR);
}

int Xfileno(fp)
XFILE *fp;
{
	if (checkfp(fp, 0) < 0) return(-1);

	return(fp -> fd);
}

VOID Xsetflags(fp, flags)
XFILE *fp;
int flags;
{
	if (checkfp(fp, 0) < 0) return;
	fp -> flags |= flags;
}

# ifdef	DEP_STREAMTIMEOUT
VOID Xsettimeout(fp, timeout)
XFILE *fp;
int timeout;
{
	if (checkfp(fp, 0) < 0) return;
	fp -> timeout = timeout;
}
# endif	/* DEP_STREAMTIMEOUT */

# ifdef	DEP_STREAMLOG
static VOID NEAR dumplog(buf, size, fp)
CONST char *buf;
ALLOC_T size;
XFILE *fp;
{
	if (!(fp -> dumpfunc) || !(fp -> debuglvl)) return;
	if (debuglevel < fp -> debuglvl - 1) return;

	(*(fp -> dumpfunc))((CONST u_char *)buf, size, fp -> debugmes);
}
# endif	/* DEP_STREAMLOG */

static int NEAR fillbuf(fp)
XFILE *fp;
{
	ALLOC_T len;
	int n;

	if (checkfp(fp, XS_ERROR | XS_RDONLY) < 0) return(-1);
	if (fp == Xstdin) {
		if (Xstdout -> flags & XF_LINEBUF) VOID_C flushbuf(Xstdout);
		if (Xstderr -> flags & XF_LINEBUF) VOID_C flushbuf(Xstderr);
	}

# ifdef	DEP_STREAMTIMEOUT
	if (fp -> status & XS_NOAHEAD) {
#  ifdef	DEP_STREAMLOG
		if (fp -> status & XS_CLEARBUF) {
			fp -> status &= ~XS_CLEARBUF;
			fp -> ptr = (ALLOC_T)0;
		}
		else
#  endif
		if (fp -> ptr >= XF_BUFSIZ) {
#  ifdef	DEP_STREAMLOG
			dumplog(fp -> buf, XF_BUFSIZ, fp);
#  endif
			fp -> ptr = (ALLOC_T)0;
		}
		n = checkread(fp -> fd, &(fp -> buf[fp -> ptr]),
			(ALLOC_T)1, fp -> timeout);
	}
	else
# endif	/* DEP_STREAMTIMEOUT */
	{
		fp -> ptr = (ALLOC_T)0;
		len = (fp -> flags & XF_NOBUF) ? (ALLOC_T)1 : XF_BUFSIZ;
		n = Xread(fp -> fd, fp -> buf, len);
	}
	if (n <= 0) {
		if (n < 0) fp -> status |= XS_ERROR;
		else {
			fp -> status |= XS_EOF;
			fp -> status &= ~XS_READ;
		}
		fp -> count = (ALLOC_T)0;
		return(-1);
	}

	fp -> count = (ALLOC_T)n;
	fp -> status |= XS_READ;
# ifdef	DEP_STREAMLOG
#  ifdef	DEP_STREAMTIMEOUT
	if (fp -> status & XS_NOAHEAD) {
		if (fp -> buf[fp -> ptr] == '\n') {
			fp -> status |= XS_CLEARBUF;
			dumplog(fp -> buf, fp -> ptr + 1, fp);
		}
	}
	else
#  endif
	dumplog(fp -> buf, (ALLOC_T)n, fp);
# endif	/* DEP_STREAMLOG */

	return(0);
}

static int NEAR flushbuf(fp)
XFILE *fp;
{
	ALLOC_T len;
	int n;

	if (checkfp(fp, XS_ERROR) < 0) return(-1);
	fp -> status &= ~XS_WRITTEN;
	if (fp -> status & XS_RDONLY) return(0);

	len = fp -> ptr;
	fp -> ptr = fp -> count = (ALLOC_T)0;
	if (!len || (fp -> status & XS_CLOSED)) return(0);

	n = Xwrite(fp -> fd, fp -> buf, len);
	if (n < (int)len) {
		fp -> status |= XS_ERROR;
		return(-1);
	}
# ifdef	DEP_DOSDRIVE
	if (chkopenfd(fp -> fd) == DEV_DOS && dosflushbuf(fp -> fd) < 0) {
		fp -> status |= XS_ERROR;
		return(-1);
	}
# endif
# ifdef	DEP_STREAMLOG
	dumplog(fp -> buf, (ALLOC_T)n, fp);
# endif

	return(n);
}

int Xfflush(fp)
XFILE *fp;
{
	return((flushbuf(fp) < 0) ? EOF : 0);
}

int Xfpurge(fp)
XFILE *fp;
{
	if (checkfp(fp, XS_RDONLY) < 0) return(EOF);

	fp -> status &= ~(XS_EOF | XS_READ);
	fp -> ptr = fp -> count = (ALLOC_T)0;

	return(0);
}

int Xfread(buf, size, fp)
char *buf;
ALLOC_T size;
XFILE *fp;
{
	ALLOC_T ptr, len;
	int nl, total;

	if (checkfp(fp, XS_ERROR | XS_RDONLY) < 0) return(-1);
	if ((fp -> status & XS_WRITTEN) && flushbuf(fp) < 0) return(-1);
	if (!(fp -> status & XS_READ)) fp -> ptr = fp -> count = (ALLOC_T)0;
	nl = total = 0;

	while (size > (ALLOC_T)0) {
		if (fp -> status & (XS_EOF | XS_CLOSED)) break;
		if (!(fp -> count) && fillbuf(fp) < 0) break;
		if (nl && fp -> buf[fp -> ptr] != '\n') {
			*(buf++) = '\r';
			size--;
			total++;
		}
		len = fp -> count;
		if (len > size) len = size;

		nl = 0;
		if (ISCRNL(fp)) for (ptr = (ALLOC_T)0; ptr < len; ptr++) {
# ifdef	USECRNL
			if (fp -> buf[fp -> ptr + ptr] == CH_EOF) {
				fp -> status |= XS_EOF;
				break;
			}
# endif
			if (fp -> buf[fp -> ptr + ptr] == '\r') {
				nl++;
				len = ptr;
				break;
			}
		}

		memcpy(buf, &(fp -> buf[fp -> ptr]), len);
		fp -> ptr += len;
		buf += len;
		size -= len;
		fp -> count -= len;
		total += len;

# ifdef	USECRNL
		if (fp -> status & XS_EOF)
			fp -> ptr = fp -> count = (ALLOC_T)0;
		else
# endif
		if (nl) {
			(fp -> ptr)++;
			(fp -> count)--;
		}
	}

	return((fp -> status & XS_ERROR) ? -1 : total);
}

int Xfwrite(buf, size, fp)
CONST char *buf;
ALLOC_T size;
XFILE *fp;
{
	ALLOC_T ptr, len, max;
	int n, nl, prev, total;

	if (checkfp(fp, XS_ERROR | XS_WRONLY) < 0) return(-1);
	if (fp -> status & XS_READ) {
		fp -> status &= ~(XS_EOF | XS_READ);
		fp -> ptr = fp -> count = (ALLOC_T)0;
	}
	if (fp -> status & XS_CLOSED) return(0);
	max = (fp -> flags & XF_NOBUF) ? (ALLOC_T)1 : XF_BUFSIZ;
	total = 0;
	prev = '\0';

	while (size > (ALLOC_T)0) {
		len = max - fp -> count;
		if (len > size) len = size;

		nl = 0;
		if (ISCRNL(fp)) for (ptr = (ALLOC_T)0; ptr < len; ptr++) {
			if (buf[ptr] == '\n' && prev != '\r') {
				nl++;
				len = ptr;
				break;
			}
			prev = buf[ptr];
		}

		memcpy(&(fp -> buf[fp -> ptr]), buf, len);
		if (nl) fp -> buf[fp -> ptr + len++] = '\r';
		total += len;
		fp -> ptr += len;
		buf += len;
		size -= len;
		fp -> count += len;
		fp -> status |= XS_WRITTEN;
		if (fp -> count >= max && flushbuf(fp) < 0) break;

		if (nl) {
			fp -> buf[(fp -> ptr)++] = '\n';
			(fp -> count)++;
			fp -> status |= XS_WRITTEN;
			if (fp -> count >= max && flushbuf(fp) < 0) break;
		}
	}

	if (fp -> status & XS_ERROR) return(total);
	if (!(fp -> flags & XF_LINEBUF)) return(total);

	for (len = fp -> ptr; len > (ALLOC_T)0; len--)
		if (fp -> buf[len - 1] == '\n') break;
	if (len <= (ALLOC_T)0) return(total);
	n = Xwrite(fp -> fd, fp -> buf, len);
	if (n < (int)len) {
		fp -> status |= XS_ERROR;
		total -= (int)len - n;
		return(total);
	}
	fp -> ptr -= len;
	memmove(fp -> buf, &(fp -> buf[len]), fp -> ptr);

	return(total);
}

int Xfgetc(fp)
XFILE *fp;
{
	u_char uc;
	int n;

# ifdef	DEP_STREAMTIMEOUT
	if (fp -> flags & XF_NONBLOCK) fp -> status |= XS_NOAHEAD;
# endif
	n = Xfread((char *)&uc, sizeof(uc), fp);
# ifdef	DEP_STREAMTIMEOUT
	fp -> status &= ~XS_NOAHEAD;
# endif

	return((n > 0) ? (int)uc : EOF);
}

int Xfputc(c, fp)
int c;
XFILE *fp;
{
	u_char uc;

	uc = (u_char)c;
	if (Xfwrite((char *)&uc, sizeof(uc), fp) < (int)sizeof(uc))
		return(EOF);

	return(c);
}

# ifdef	DEP_STREAMSOCKET
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
# endif	/* DEP_STREAMSOCKET */

char *Xfgets(fp)
XFILE *fp;
{
# ifdef	DEP_STREAMLOG
	int dupdebuglvl;
# endif
	char *buf;
	ALLOC_T len, size;
	int n, c;

	if (checkfp(fp, XS_ERROR | XS_RDONLY) < 0) return(NULL);
	buf = c_realloc(NULL, 0, &size);

	len = (ALLOC_T)0;
	for (;;) {
# ifdef	DEP_STREAMLOG
		dupdebuglvl = fp -> debuglvl;
		if (fp -> flags & XF_NONBLOCK) fp -> debuglvl = 0;
# endif
		n = c = Xfgetc(fp);
# ifdef	DEP_STREAMLOG
		fp -> debuglvl = dupdebuglvl;
# endif
		if (c == EOF) {
			if (!len || (fp -> status & XS_ERROR)) /*EMPTY*/;
			else break;
			Xfree(buf);
			return(NULL);
		}
# ifdef	DEP_STREAMSOCKET
		if ((fp -> flags & XF_TELNET) && c == TELNET_IAC) {
			c = replytelnet(fp);
			if (!c) continue;
		}
# endif
		buf = c_realloc(buf, len, &size);
		if ((fp -> flags & XF_NULLCONV) && !c) c = '\n';

		buf[len++] = c;
		if (n == '\n') break;
	}

	if (len > 0 && buf[len - 1] == '\n') len--;
	buf[len++] = '\0';
	buf = Xrealloc(buf, len);

	return(buf);
}

int Xfputs(s, fp)
CONST char *s;
XFILE *fp;
{
	ALLOC_T len;

	len = strlen(s);
	return((Xfwrite(s, len, fp) < (int)len) ? EOF : 0);
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
		VOID_C flushbuf(fp);
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

	Xstrcpy(path, PIPEDIR);
	if (mktmpdir(path) < 0) return(NULL);
# ifdef	DEP_DOSLFN
	if (!(cp = preparefile(path, tmp))) return(NULL);
	else if (cp == tmp) Xstrcpy(path, tmp);
# endif
	Xstrcpy(strcatdelim(path), PIPEFILE);

	l1 = strlen(command);
	l2 = strlen(path);
	cmdline = Xmalloc(l1 + l2 + 3 + 1);
	Xstrncpy(cmdline, command, l1);
	Xstrcpy(&(cmdline[l1]), " > ");
	l1 += 3;
	Xstrcpy(&(cmdline[l1]), path);
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
	Xstrcpy(strcatdelim(path), PIPEDIR);
	Xstrcpy((cp = strcatdelim(path)), PIPEFILE);

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
	char *buf;
	ALLOC_T len, size;
	u_char uc;
	int n;

	buf = c_realloc(NULL, 0, &size);

	len = (ALLOC_T)0;
	for (;;) {
		n = read(fd, &uc, sizeof(uc));
		if (n <= 0) {
			if (!len || n < 0) {
				Xfree(buf);
				return(NULL);
			}
			break;
		}
		if (uc == '\n') break;
		buf = c_realloc(buf, len, &size);
		buf[len++] = uc;
	}
	buf[len++] = '\0';

	return(Xrealloc(buf, len));
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
	char *buf;

	Xfputs(prompt, Xstderr);
	VOID_C Xfflush(Xstderr);

# ifdef	DEP_ORIGSTREAM
#  ifdef	DEP_ORIGSHELL
	fd = safe_dup(ttyio);
	fp = (fd >= 0) ? Xfdopen(fd, "r") : NULL;
	if (fp) /*EMPTY*/;
	else
#  endif
	fp = Xstdin;
	buf = Xfgets(fp);
	safefclose(fp);
# else	/* !DEP_ORIGSTREAM */
	buf = fgets2(STDIN_FILENO);
# endif	/* !DEP_ORIGSTREAM */

	return(buf);
}
#endif	/* !FD */
