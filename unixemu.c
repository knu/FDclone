/*
 *	unixemu.c
 *
 *	UNIX Function Emulation on DOS
 */

#include "fd.h"
#include "func.h"

#include <fcntl.h>

#include "unixdisk.h"

extern char *deftmpdir;
extern char *tmpfilename;


int _dospath(path)
char *path;
{
	return((isalpha(*path) && path[1] == ':') ? *path : 0);
}

int dospath(path, buf)
char *path, *buf;
{
	char tmp[MAXPATHLEN + 1];
	int drv;

	if (path == buf) {
		strcpy(tmp, path);
		path = tmp;
	}
	if (buf && unixrealpath(path, buf)) return(*buf);
	drv = _dospath(path);
	return((drv) ? drv : getcurdrv());
}

DIR *_Xopendir(path)
char *path;
{
	return(unixopendir(path));
}

DIR *Xopendir(path)
char *path;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
	return(unixopendir(path));
}

int Xclosedir(dirp)
DIR *dirp;
{
	return(unixclosedir(dirp));
}

struct dirent *Xreaddir(dirp)
DIR *dirp;
{
#ifdef	NOLFNEMU
	struct dirent *dp;

	if (!(dp = unixreaddir(dirp))) return(NULL);
	adjustfname(dp -> d_name);
	return(dp);
#else
	return(unixreaddir(dirp));
#endif
}

VOID Xrewinddir(dirp)
DIR *dirp;
{
	unixrewinddir(dirp);
}

int _Xchdir(path)
char *path;
{
	char buf[MAXPATHLEN + 1];
	int drive;

	drive = dospath(path, buf);
	if (setcurdrv(drive) < 0
	|| !(path = preparefile(path, buf, 0))) return(-1);
	return((chdir(path) != 0) ? -1 : 0);
}

int Xchdir(path)
char *path;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
	return(_Xchdir(path));
}

char *Xgetcwd(path, size)
char *path;
int size;
{
#ifdef	NOLFNEMU
	if (!unixgetcwd(path, size)) return(NULL);
	adjustfname(path + 2);
	return(path);
#else
	return(unixgetcwd(path, size));
#endif
}

int Xstat(path, statp)
char *path;
struct stat *statp;
{
	char *cp;
	u_short mode;
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (strcmp(path, "..") && detransfile(path, buf, 1) == buf) path = buf;
#endif
	if (unixstat(path, statp) < 0) return(-1);

	mode = statp -> st_mode;
	if ((mode & S_IFMT) != S_IFDIR
	&& (cp = strrchr(path, '.')) && strlen(++cp) == 3) {
		if (!strcasecmp2(cp, "BAT")
		|| !strcasecmp2(cp, "COM")
		|| !strcasecmp2(cp, "EXE")) mode |= S_IEXEC;
	}
	mode &= (S_IREAD | S_IWRITE | S_IEXEC);
	mode |= (mode >> 3) | (mode >> 6);
	statp -> st_mode |= mode;

	return(0);
}

int Xlstat(path, statp)
char *path;
struct stat *statp;
{
	return(Xstat(path, statp));
}

int Xaccess(path, mode)
char *path;
int mode;
{
	char buf[MAXPATHLEN + 1];
	struct stat status;

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
	else
#endif
	if (!(path = preparefile(path, buf, 0))) return(-1);
	if (access(path, mode) != 0) return(-1);
	if (!(mode & X_OK)) return(0);
	if (stat(path, &status) != 0
	|| !(status.st_mode & S_IEXEC)) {
		errno = EACCES;
		return(-1);
	}
	return(0);
}

int Xsymlink(name1, name2)
char *name1, *name2;
{
	errno = EINVAL;
	return(-1);
}

int Xreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
#ifndef	_NOROCKRIDGE
	char tmp[MAXPATHLEN + 1];

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
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 0) == buf) path = buf;
	else
#endif
	return(unixchmod(path, mode));
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
	char buf[MAXPATHLEN + 1];

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
	else
#endif
	if (!(path = preparefile(path, buf, 0))) return(-1);
	return((utime(path, times) != 0) ? -1 : 0);
#else
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
	char buf[MAXPATHLEN + 1];

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
	else
#endif
	if (!(path = preparefile(path, buf, 0))) return(-1);
	return((utimes(path, tvp) != 0) ? -1 : 0);
#endif
}

int Xunlink(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
	else
#endif
	if (!(path = preparefile(path, buf, 0))) return(-1);
	return((unlink(path) != 0) ? -1 : 0);
}

int Xrename(from, to)
char *from, *to;
{
#ifndef	_NOROCKRIDGE
	char buf1[MAXPATHLEN + 1], buf2[MAXPATHLEN + 1];

	if (detransfile(from, buf1, 0) == buf1) from = buf1;
	if (detransfile(to, buf2, 0) == buf2) to = buf2;
#endif
	if (_dospath(from) != _dospath(to)) {
		errno = EXDEV;
		return(-1);
	}
	return(unixrename(from, to));
}

int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
	char buf[MAXPATHLEN + 1];
	int iscreat;

	iscreat = (flags & O_CREAT) ? 1 : 0;
#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
	else
#endif
	if (!(path = preparefile(path, buf, iscreat))) return(-1);
	return(open(path, flags, mode));
}

int Xclose(fd)
int fd;
{
	return((close(fd) != 0) ? -1 : 0);
}

int Xread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	return(read(fd, buf, nbytes));
}

int Xwrite(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	return(write(fd, buf, nbytes));
}

off_t Xlseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
	return(lseek(fd, offset, whence));
}

int _Xmkdir(path, mode)
char *path;
int mode;
{
	return(unixmkdir(path, mode));
}

int Xmkdir(path, mode)
char *path;
int mode;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
	return(unixmkdir(path, mode));
}

int _Xrmdir(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

	if (!(path = preparefile(path, buf, 0))) return(-1);
	return((rmdir(path) != 0) ? -1 : 0);
}

int Xrmdir(path)
char *path;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
	return(_Xrmdir(path));
}

FILE *_Xfopen(path, type)
char *path, *type;
{
	char buf[MAXPATHLEN + 1];
	int iscreat;

	iscreat = (strchr(type, 'w')) ? 1 : 0;
	if (!(path = preparefile(path, buf, iscreat))) return(NULL);
	return(fopen(path, type));
}

FILE *Xfopen(path, type)
char *path, *type;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
	return(_Xfopen(path, type));
}

int Xfclose(stream)
FILE *stream;
{
	return(fclose(stream));
}

int Xfeof(stream)
FILE *stream;
{
	return(feof(stream));
}

int Xfread(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	return(fread(buf, size, nitems, stream));
}

int Xfwrite(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	return(fwrite(buf, size, nitems, stream));
}

int Xfflush(stream)
FILE *stream;
{
	return(fflush(stream));
}

int Xfgetc(stream)
FILE *stream;
{
	return(fgetc(stream));
}

int Xfputc(c, stream)
int c;
FILE *stream;
{
	return(fputc(c, stream));
}

char *Xfgets(s, n, stream)
char *s;
int n;
FILE *stream;
{
	return(fgets(s, n, stream));
}

int Xfputs(s, stream)
char *s;
FILE *stream;
{
	return(fputs(s, stream));
}

FILE *Xpopen(command, type)
char *command, *type;
{
	char cmdline[128], path[MAXPATHLEN + 1];

	strcpy(path, PIPEDIR);
	if (mktmpdir(path) < 0) return(NULL);
	strcat(path, _SS_);
	strcat(path, PIPEFILE);
	sprintf(cmdline, "%s > %s", command, path);
	system(cmdline);
	return(fopen(path, type));
}

int Xpclose(fp)
FILE *fp;
{
	char *cp, path[MAXPATHLEN + 1];
	int no;

	no = 0;
	if (fclose(fp) != 0) no = errno;
	strcpy(path, deftmpdir);
	strcat(path, _SS_);
	strcat(path, tmpfilename);
	strcat(path, _SS_);
	strcat(path, PIPEDIR);
	cp = path + strlen(path);
	strcat(path, _SS_);
	strcat(path, PIPEFILE);

	if (unlink(path) != 0) no = errno;
	*cp = '\0';
	if (rmtmpdir(path) < 0) no = errno;
	return((errno = no) ? -1 : 0);
}
