/*
 *	unixemu.c
 *
 *	UNIX Function Emulation on DOS
 */

#include <io.h>
#include "fd.h"
#include "func.h"
#include "unixdisk.h"
#include "dosdisk.h"

extern char *deftmpdir;
extern char *tmpfilename;
#ifdef	LSI_C
extern u_char _openfile[];
#endif

#ifndef	_NODOSDRIVE
static int NEAR checkpath __P_((char *, char *));
int lastdrv = -1;
#endif

int _dospath(path)
char *path;
{
	return((isalpha(*path) && path[1] == ':') ? *path : 0);
}

int dospath(path, buf)
char *path, *buf;
{
	char tmp[MAXPATHLEN];
#ifndef	_NOUSELFN
	char *cp;
#endif
	int drv;

	if (path == buf) {
		strcpy(tmp, path);
		path = tmp;
	}
#ifdef	_NOUSELFN
	if (buf) strcpy(buf, path);
#else
	if (buf && (cp = shortname(path, buf))) path = cp;
#endif
	drv = _dospath(path);
	return((drv) ? drv : getcurdrv());
}

#ifndef	_NODOSDRIVE
int dospath2(path)
char *path;
{
	int drv, drive;

	if (!(drive = _dospath(path))) drive = getcurdrv();
	drv = toupper2(drive) - 'A';
	if (drv < 0 || drv > 'Z' - 'A' || checkdrive(drv) <= 0) return(0);
	return(drive);
}

int dospath3(path)
char *path;
{
	int drive;

	if ((drive = supportLFN(path)) >= 0 || drive <= -3) return(0);
	return((drive = _dospath(path)) ? drive : getcurdrv());
}

static int NEAR checkpath(path, buf)
char *path, *buf;
{
	char *cp, tmp[MAXPATHLEN];
	int i, drive;

	if ((drive = _dospath(path))) cp = path + 2;
	else {
		cp = path;
		drive = getcurdrv();
	}
	i = toupper2(drive) - 'A';
	if (i < 0 || i > 'Z' - 'A' || checkdrive(i) <= 0) return(0);
	if (!buf) return(drive);

	if (path == buf) {
		strcpy(tmp, path);
		path = tmp;
	}
	if (*cp == _SC_) {
		*(buf++) = drive;
		*(buf++) = ':';
	}
	else {
		if (!dosgetcwd(buf, MAXPATHLEN)) return(0);
		buf = strcatdelim(buf);
	}
	strcpy(buf, cp);
	return(drive);
}
#endif	/* !_NODOSDRIVE */

DIR *_Xopendir(path)
char *path;
{
	return(unixopendir(path));
}

#ifndef	_NOROCKRIDGE
DIR *Xopendir(path)
char *path;
{
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 1) == buf) path = buf;
	return(unixopendir(path));
}
#endif

int Xclosedir(dirp)
DIR *dirp;
{
	return(unixclosedir(dirp));
}

struct dirent *Xreaddir(dirp)
DIR *dirp;
{
#ifdef	_NOROCKRIDGE
	return(unixreaddir(dirp));
#else
	static struct dirent buf;
	struct dirent *dp;
	char tmp[MAXNAMLEN + 1];

	if (!(dp = unixreaddir(dirp))) return(NULL);
	memcpy(&buf, dp, sizeof(struct dirent));
	if (transfile(buf.d_name, tmp) == tmp) strcpy(buf.d_name, tmp);
	return(&buf);
#endif
}

VOID Xrewinddir(dirp)
DIR *dirp;
{
	unixrewinddir(dirp);
}

int rawchdir(path)
char *path;
{
	if (setcurdrv(dospath(path, NULL), 1) < 0 || unixchdir(path) < 0)
		return(-1);
	return(0);
}

#ifndef	_NODOSDRIVE
int _Xchdir(path)
char *path;
{
	char buf[MAXPATHLEN];
	int drive, dd;

	if (!(drive = dospath3(path))) {
		if (rawchdir(path) < 0) return(-1);
		if (lastdrv >= 0) shutdrv(lastdrv);
		lastdrv = -1;
		return(0);
	}

	if ((dd = preparedrv(drive)) < 0) return(-1);
	if (setcurdrv(drive, 1) < 0
	|| (checkpath(path, buf) ? doschdir(buf) : unixchdir(path)) < 0) {
		shutdrv(dd);
		return(-1);
	}
	if (lastdrv >= 0) {
		if ((lastdrv % DOSNOFILE) != (dd % DOSNOFILE))
			shutdrv(lastdrv);
		else dd = lastdrv;
	}
	lastdrv = dd;
	return(0);
}
#endif	/* !_NODOSDRIVE */

#ifndef	_NOROCKRIDGE
int Xchdir(path)
char *path;
{
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 1) == buf) path = buf;
	return(_Xchdir(path));
}
#endif

char *_Xgetwd(path)
char *path;
{
	return(unixgetcwd(path, MAXPATHLEN, 0));
}

char *Xgetwd(path)
char *path;
{
	return(unixgetcwd(path, MAXPATHLEN, 1));
}

int Xstat(path, stp)
char *path;
struct stat *stp;
{
	char *cp;
	u_short mode;
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN];

	if (strcmp(path, "..") && detransfile(path, buf, 1) == buf) path = buf;
#endif
	if (unixstat(path, stp) < 0) return(-1);

	mode = (u_short)(stp -> st_mode);
	if ((mode & S_IFMT) != S_IFDIR
	&& (cp = strrchr(path, '.')) && strlen(++cp) == 3) {
		if (!stricmp(cp, "BAT")
		|| !stricmp(cp, "COM")
		|| !stricmp(cp, "EXE")) mode |= S_IEXEC;
	}
	mode &= (S_IREAD | S_IWRITE | S_IEXEC);
	mode |= (mode >> 3) | (mode >> 6);
	stp -> st_mode |= mode;

	return(0);
}

int Xlstat(path, stp)
char *path;
struct stat *stp;
{
	return(Xstat(path, stp));
}

int Xaccess(path, mode)
char *path;
int mode;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NOUSELFN)
	char buf[MAXPATHLEN];
#endif
	struct stat st;

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
	else
#endif
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	if (checkpath(path, buf)) return(dosaccess(buf, mode));
	else
# endif
	if (!(path = preparefile(path, buf))) return(-1);
#else
	;
#endif
	if (access(path, mode) != 0) return(-1);
	if (!(mode & X_OK)) return(0);
	if (Xstat(path, &st) < 0
	|| !(st.st_mode & S_IEXEC)) {
		errno = EACCES;
		return(-1);
	}
	return(0);
}

/*ARGSUSED*/
int Xsymlink(name1, name2)
char *name1, *name2;
{
	errno = EINVAL;
	return(-1);
}

/*ARGSUSED*/
int Xreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
#ifndef	_NOROCKRIDGE
	char tmp[MAXPATHLEN];

	if (detransfile(path, tmp, 0) == buf) {
		detransfile(path, buf, 1);
		if (!strcmp(tmp, buf)) return(0);
	}
#endif
	errno = EINVAL;
	return(-1);
}

int Xchmod(path, mode)
char *path;
int mode;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
	return(unixchmod(path, mode));
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
# ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 0) == buf) path = buf;
# endif
	return(unixutime(path, times));
#else	/* !USEUTIME */
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
# ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 0) == buf) path = buf;
# endif
	return(unixutimes(path, tvp));
#endif	/* !USEUTIME */
}

int Xunlink(path)
char *path;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NOUSELFN)
	char buf[MAXPATHLEN];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
	else
#endif
	if (unixunlink(path) != 0) {
		if (errno != EACCES
		|| unixchmod(path, (S_IREAD | S_IWRITE | S_ISVTX)) < 0
		|| unixunlink(path) != 0) return(-1);
	}
	return(0);
}

int Xrename(from, to)
char *from, *to;
{
#ifndef	_NOROCKRIDGE
	char buf1[MAXPATHLEN], buf2[MAXPATHLEN];

	if (detransfile(from, buf1, 0) == buf1) from = buf1;
	if (detransfile(to, buf2, 0) == buf2) to = buf2;
#endif
	if (_dospath(from) != _dospath(to)) {
		errno = EXDEV;
		return(-1);
	}
	return((unixrename(from, to) != 0) ? -1 : 0);
}

#if	!defined (_NOROCKRIDGE) || !defined (_NOUSELFN)
int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
	char buf[MAXPATHLEN];

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
	else
#endif
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	if (checkpath(path, buf)) return(dosopen(buf, flags, mode));
	else
# endif
	if (flags & O_CREAT) return(unixopen(path, flags, mode));
	else if (!(path = preparefile(path, buf))) return(-1);
#else
	;
#endif
	return(open(path, flags, mode));
}
#endif	/* !_NOROCKRIDGE || !_NOUSELFN */

#ifndef	_NODOSDRIVE
int Xclose(fd)
int fd;
{
	if ((fd >= DOSFDOFFSET)) return(dosclose(fd));
	return((close(fd) != 0) ? -1 : 0);
}

int Xread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	if ((fd >= DOSFDOFFSET)) return(dosread(fd, buf, nbytes));
	return(read(fd, buf, nbytes));
}

int Xwrite(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	if ((fd >= DOSFDOFFSET)) return(doswrite(fd, buf, nbytes));
	return(write(fd, buf, nbytes));
}

off_t Xlseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
	if ((fd >= DOSFDOFFSET)) return(doslseek(fd, offset, whence));
	return(lseek(fd, offset, whence));
}
#endif	/* !_NODOSDRIVE */

#if	defined (LSI_C) || !defined (_NODOSDRIVE)
int Xdup(oldd)
int oldd;
{
	int fd;

#ifndef	_NODOSDRIVE
	if ((oldd >= DOSFDOFFSET)) {
		errno = EBADF;
		return(-1);
	}
#endif
	if ((fd = dup(oldd)) < 0) return(-1);
#ifdef	LSI_C
	if (fd < SYS_OPEN && oldd >= 0 && oldd < SYS_OPEN)
		_openfile[fd] = _openfile[oldd];
#endif
	return(fd);
}

int Xdup2(oldd, newd)
int oldd, newd;
{
	int fd;

#ifndef	_NODOSDRIVE
	if ((oldd >= DOSFDOFFSET || newd >= DOSFDOFFSET)) {
		errno = EBADF;
		return(-1);
	}
#endif
	if ((fd = dup2(oldd, newd)) < 0) return(-1);
#ifdef	LSI_C
	if (newd >= 0 && newd < SYS_OPEN && oldd >= 0 && oldd < SYS_OPEN)
		_openfile[newd] = _openfile[oldd];
#endif
	return(fd);
}
#endif	/* LSI_C || !_NODOSDRIVE */

#ifdef	_NOUSELFN
# ifndef	DJGPP
int _Xmkdir(path, mode)
char *path;
int mode;
{
	struct stat st;

	if (Xstat(path, &st) >= 0) {
		errno = EEXIST;
		return(-1);
	}
	return(mkdir(path) ? -1 : 0);
}
# endif	/* !DJGPP */
#else	/* !_NOUSELFN */
int _Xmkdir(path, mode)
char *path;
int mode;
{
	return(unixmkdir(path, mode));
}

int _Xrmdir(path)
char *path;
{
	return(unixrmdir(path));
}
#endif	/* !_NOUSELFN */

#ifndef	_NOROCKRIDGE
int Xmkdir(path, mode)
char *path;
int mode;
{
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 0) == buf) path = buf;
	return((unixmkdir(path, mode) != 0) ? -1 : 0);
}

int Xrmdir(path)
char *path;
{
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 0) == buf) path = buf;
	return((unixrmdir(path) != 0) ? -1 : 0);
}
#endif

#ifndef	_NOUSELFN
FILE *_Xfopen(path, type)
char *path, *type;
{
	char buf[MAXPATHLEN];

# ifndef	_NODOSDRIVE
	if (checkpath(path, buf)) return(dosfopen(buf, type));
# endif
	if (strchr(type, 'w')) return(unixfopen(buf, type));
	else if (!(path = preparefile(path, buf))) return(NULL);
	return(fopen(path, type));
}
#endif	/* !_NOUSELFN */

#ifndef	_NOROCKRIDGE
FILE *Xfopen(path, type)
char *path, *type;
{
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 1) == buf) path = buf;
	return(_Xfopen(path, type));
}
#endif

#ifndef	_NODOSDRIVE
FILE *Xfdopen(fd, type)
int fd;
char *type;
{
	if ((fd >= DOSFDOFFSET)) return(dosfdopen(fd, type));
	return(fdopen(fd, type));
}

int Xfclose(stream)
FILE *stream;
{
	if (dosfileno(stream) > 0) return(dosfclose(stream));
	return(fclose(stream));
}

int Xfeof(stream)
FILE *stream;
{
	if (dosfileno(stream) > 0) return(dosfeof(stream));
	return(feof(stream));
}

int Xfread(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	if (dosfileno(stream) > 0)
		return(dosfread(buf, size, nitems, stream));
	return(fread(buf, size, nitems, stream));
}

int Xfwrite(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	if (dosfileno(stream) > 0)
		return(dosfwrite(buf, size, nitems, stream));
	return(fwrite(buf, size, nitems, stream));
}

int Xfflush(stream)
FILE *stream;
{
	if (dosfileno(stream) > 0) return(dosfflush(stream));
	return(fflush(stream));
}

int Xfgetc(stream)
FILE *stream;
{
	if (dosfileno(stream) > 0) return(dosfgetc(stream));
	return(fgetc(stream));
}

int Xfputc(c, stream)
int c;
FILE *stream;
{
	if (dosfileno(stream) > 0) return(dosfputc(c, stream));
	return(fputc(c, stream));
}

char *Xfgets(s, n, stream)
char *s;
int n;
FILE *stream;
{
	if (dosfileno(stream) > 0) return(dosfgets(s, n, stream));
	return(fgets(s, n, stream));
}

int Xfputs(s, stream)
char *s;
FILE *stream;
{
	if (dosfileno(stream) > 0) return(dosfputs(s, stream));
	return(fputs(s, stream));
}
#endif	/* !_NODOSDRIVE */

FILE *Xpopen(command, type)
char *command, *type;
{
#ifndef	_NOUSELFN
	char *tmp, buf[MAXPATHLEN];
#endif
	char cmdline[128], path[MAXPATHLEN];

	strcpy(path, PIPEDIR);
	if (mktmpdir(path) < 0) return(NULL);
#ifndef	_NOUSELFN
	if (!(tmp = preparefile(path, buf))) return(NULL);
	else if (tmp != path) strcpy(path, tmp);
#endif
	strcpy(strcatdelim(path), PIPEFILE);

	sprintf(cmdline, "%s > %s", command, path);
	system(cmdline);
	return(fopen(path, type));
}

int Xpclose(fp)
FILE *fp;
{
	char *cp, path[MAXPATHLEN];
	int no;

	no = 0;
	if (fclose(fp) != 0) no = errno;
	if (!deftmpdir || !*deftmpdir || !tmpfilename || !*tmpfilename) {
		errno = ENOENT;
		return(-1);
	}
	strcatdelim2(path, deftmpdir, tmpfilename);
	strcpy(strcatdelim(path), PIPEDIR);
	strcpy((cp = strcatdelim(path)), PIPEFILE);

	if (unixunlink(path) != 0) no = errno;
	*(--cp) = '\0';
	if (rmtmpdir(path) < 0) no = errno;
	return((errno = no) ? -1 : 0);
}
