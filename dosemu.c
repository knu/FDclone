/*
 *	dosemu.c
 *
 *	UNIX Function Emulation for DOS
 */

#include "fd.h"
#include "func.h"
#include "kctype.h"
#include "dosdisk.h"

#ifndef	_NODOSDRIVE
static char pseudocwd[MAXPATHLEN + 1] = "";
int lastdrv = -1;
int dosdrive = 0;


int _dospath(path)
char *path;
{
	if (!dosdrive) return(0);
	return((isalpha(*path) && path[1] == ':') ? *path : 0);
}

int dospath(path, buf)
char *path, *buf;
{
	char *cp, tmp[MAXPATHLEN + 1];
	int drive;

	if (!dosdrive) return(0);
	if (_dospath(path)) cp = path;
	else if (*path == '/' || !_dospath(pseudocwd)) return(0);
	else cp = pseudocwd;

	if (cp == buf) {
		strcpy(tmp, cp);
		cp = tmp;
	}
	drive = *cp;
	if (!buf) return(drive);

#ifdef	CODEEUC
	buf[ujis2sjis(buf, cp)] = '\0';
#else
	strcpy(buf, cp);
#endif
	if (cp != path && *path) {
		if (*buf && buf[strlen(buf) - 1] != '/') strcat(buf, "/");
#ifdef	CODEEUC
		buf += strlen(buf);
		buf[ujis2sjis(buf, path)] = '\0';
#else
		strcat(buf, path);
#endif
	}
	return(drive);
}
#endif	/* !_NODOSDRIVE */

DIR *_Xopendir(path)
char *path;
{
#ifdef	_NODOSDRIVE
	return(opendir(path));
#else
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosopendir(buf) : opendir(path));
#endif
}

DIR *Xopendir(path)
char *path;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
	return(_Xopendir(path));
}

int Xclosedir(dirp)
DIR *dirp;
{
#ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) return(dosclosedir(dirp));
#endif
	closedir(dirp);
	return(0);
}

struct dirent *Xreaddir(dirp)
DIR *dirp;
{
#if	defined (_NOROCKRIDGE) && defined (_NODOSDRIVE)
	return(readdir(dirp));
#else	/* !_NOROCKRIDGE || !_NODOSDRIVE */
	static st_dirent buf;
	struct dirent *dp;
#ifndef	_NOROCKRIDGE
	char tmp[MAXNAMLEN + 1];
#endif
#if	defined (CODEEUC) && !defined (_NODOSDRIVE)
	int i;
#endif

#ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) dp = dosreaddir(dirp);
	else
#endif
	dp = readdir(dirp);
	if (!dp) return(NULL);
	memcpy(&buf, dp, sizeof(buf));
#if	defined (CODEEUC) && !defined (_NODOSDRIVE)
	if (*((int *)dirp) < 0) {
		i = sjis2ujis(((struct dirent *)&buf) -> d_name, dp -> d_name);
		((struct dirent *)&buf) -> d_name[i] = '\0';
	}
#endif
#ifndef	_NOROCKRIDGE
	if (transfile(((struct dirent *)&buf) -> d_name, tmp) == tmp)
		strcpy(((struct dirent *)&buf) -> d_name, tmp);
#endif
	return((struct dirent *)(&buf));
#endif	/* !_NOROCKRIDGE || !_NODOSDRIVE */
}

VOID Xrewinddir(dirp)
DIR *dirp;
{
#ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) dosrewinddir(dirp);
	else
#endif
	rewinddir(dirp);
}

int _Xchdir(path)
char *path;
{
#ifdef	_NODOSDRIVE
	return(chdir(path));
#else	/* !_NODOSDRIVE */
	char buf[MAXPATHLEN + 1];
	int dd, drive;

	if (drive = dospath(path, buf)) {
		if ((dd = preparedrv(drive, waitmes)) < 0) return(-1);
		if (doschdir(buf) < 0) {
			shutdrv(dd);
			return(-1);
		}
		if (chdir("/") < 0) return(-1);
		if (lastdrv >= 0) {
			if ((lastdrv % DOSNOFILE) != (dd % DOSNOFILE))
				shutdrv(lastdrv);
			else dd = lastdrv;
		}
		lastdrv = dd;
	}
	else {
		if (chdir(path) < 0) return(-1);
		if (lastdrv >= 0) shutdrv(lastdrv);
		lastdrv = -1;
	}
	Xgetcwd(pseudocwd, MAXPATHLEN);
	return(0);
#endif	/* !_NODOSDRIVE */
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

/* ARGSUSED */
char *Xgetcwd(path, size)
char *path;
int size;
{
#ifdef	_NODOSDRIVE
#ifdef	USEGETWD
	return((char *)getwd(path));
#else
	return((char *)getcwd(path, size));
#endif
#else	/* !_NODOSDRIVE */
#ifdef	CODEEUC
	char tmpbuf[MAXPATHLEN + 1];
#endif
	int i;

#ifdef	USEGETWD
	if (lastdrv < 0) return((char *)getwd(path));
#else
	if (lastdrv < 0) return((char *)getcwd(path, size));
#endif
	else if (!dosgetcwd(path, size)) return(NULL);

	if (path[0] >= 'A' && path[0] <= 'Z') for (i = 2; path[i]; i++) {
		if (issjis1((u_char)(path[i]))) i++;
		else if (path[i] >= 'A' && path[i] <= 'Z')
			path[i] += 'a' - 'A';
	}
#ifdef	CODEEUC
	tmpbuf[sjis2ujis(tmpbuf, path)] = '\0';
	strcpy(path, tmpbuf);
#endif
	return(path);
#endif	/* !_NODOSDRIVE */
}

int Xstat(path, statp)
char *path;
struct stat *statp;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
#ifdef	_NODOSDRIVE
	return(stat(path, statp));
#else
	return(dospath(path, buf) ? dosstat(buf, statp) : stat(path, statp));
#endif
}

int Xlstat(path, statp)
char *path;
struct stat *statp;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
#ifdef	_NODOSDRIVE
	return(lstat(path, statp));
#else
	return(dospath(path, buf) ? doslstat(buf, statp) : lstat(path, statp));
#endif
}

int Xaccess(path, mode)
char *path;
int mode;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
#ifdef	_NODOSDRIVE
	return(access(path, mode));
#else
	return(dospath(path, buf) ? dosaccess(buf, mode) : access(path, mode));
#endif
}

int Xsymlink(name1, name2)
char *name1, *name2;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(name1, buf, 0) == buf) name1 = buf;
#endif
#ifdef	_NODOSDRIVE
	return(symlink(name1, name2));
#else
	return((dospath(name1, NULL) || dospath(name2, NULL)) ?
		dossymlink(name1, name2) : symlink(name1, name2));
#endif
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
		errno = EINVAL;
		return(-1);
	}
#endif
#ifdef	_NODOSDRIVE
	return(readlink(path, buf, bufsiz));
#else
	return(dospath(path, NULL) ?
		dosreadlink(path, buf, bufsiz) : readlink(path, buf, bufsiz));
#endif
}

int Xchmod(path, mode)
char *path;
int mode;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
#ifdef	_NODOSDRIVE
	return(chmod(path, mode));
#else
	return(dospath(path, buf) ? doschmod(buf, mode) : chmod(path, mode));
#endif
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
#ifdef	_NODOSDRIVE
	return(utime(path, times));
#else
	return(dospath(path, buf) ? dosutime(buf, times) : utime(path, times));
#endif
#else	/* !USEUTIME */
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
#ifdef	_NODOSDRIVE
	return(utimes(path, tvp));
#else
	return(dospath(path, buf) ? dosutimes(buf, tvp) : utimes(path, tvp));
#endif
#endif	/* !USEUTIME */
}

int Xunlink(path)
char *path;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
#ifdef	_NODOSDRIVE
	return(unlink(path));
#else
	return(dospath(path, buf) ? dosunlink(buf) : unlink(path));
#endif
}

int Xrename(from, to)
char *from, *to;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf1[MAXPATHLEN + 1], buf2[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(from, buf1, 0) == buf1) from = buf1;
	if (detransfile(to, buf2, 0) == buf2) to = buf2;
#endif
#ifdef	_NODOSDRIVE
	return(rename(from, to));
#else
	errno = EXDEV;
	if (dospath(from, buf1))
		return(dospath(to, buf2) ? dosrename(buf1, buf2) : -1);
	else return(dospath(to, NULL) ? -1 : rename(from, to));
#endif
}

int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN + 1];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
#ifdef	_NODOSDRIVE
	return(open(path, flags, mode));
#else
	return(dospath(path, buf) ?
		dosopen(buf, flags, mode) : open(path, flags, mode));
#endif
}

int Xclose(fd)
int fd;
{
#ifdef	_NODOSDRIVE
	return(close(fd));
#else
	return((fd >= DOSFDOFFSET) ? dosclose(fd) : close(fd));
#endif
}

int Xread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
#ifdef	_NODOSDRIVE
	return(read(fd, buf, nbytes));
#else
	return((fd >= DOSFDOFFSET) ?
		dosread(fd, buf, nbytes) : read(fd, buf, nbytes));
#endif
}

int Xwrite(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
#ifdef	_NODOSDRIVE
	return(write(fd, buf, nbytes));
#else
	return((fd >= DOSFDOFFSET) ?
		doswrite(fd, buf, nbytes) : write(fd, buf, nbytes));
#endif
}

off_t Xlseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
#ifdef	_NODOSDRIVE
	return(lseek(fd, offset, whence));
#else
	return((fd >= DOSFDOFFSET) ?
		doslseek(fd, offset, whence) : lseek(fd, offset, whence));
#endif
}

int _Xmkdir(path, mode)
char *path;
int mode;
{
#ifdef	_NODOSDRIVE
	return(mkdir(path, mode));
#else
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosmkdir(buf, mode) : mkdir(path, mode));
#endif
}

int Xmkdir(path, mode)
char *path;
int mode;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
	return(_Xmkdir(path, mode));
}

int _Xrmdir(path)
char *path;
{
#ifdef	_NODOSDRIVE
	return(rmdir(path));
#else
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosrmdir(buf) : rmdir(path));
#endif
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
#ifdef	_NODOSDRIVE
	return(fopen(path, type));
#else
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosfopen(buf, type) : fopen(path, type));
#endif
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
#ifdef	_NODOSDRIVE
	return(fclose(stream));
#else
	return(stream2fd(stream) >= 0 ? dosfclose(stream) : fclose(stream));
#endif
}

int Xfeof(stream)
FILE *stream;
{
#ifdef	_NODOSDRIVE
	return(feof(stream));
#else
	return(stream2fd(stream) >= 0 ? dosfeof(stream) : feof(stream));
#endif
}

int Xfread(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
#ifdef	_NODOSDRIVE
	return(fread(buf, size, nitems, stream));
#else
	return(stream2fd(stream) >= 0 ?
		dosfread(buf, size, nitems, stream) :
		fread(buf, size, nitems, stream));
#endif
}

int Xfwrite(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
#ifdef	_NODOSDRIVE
	return(fwrite(buf, size, nitems, stream));
#else
	return(stream2fd(stream) >= 0 ?
		dosfwrite(buf, size, nitems, stream) :
		fwrite(buf, size, nitems, stream));
#endif
}

int Xfflush(stream)
FILE *stream;
{
#ifdef	_NODOSDRIVE
	return(fflush(stream));
#else
	return(stream2fd(stream) >= 0 ? dosfflush(stream) : fflush(stream));
#endif
}

int Xfgetc(stream)
FILE *stream;
{
#ifdef	_NODOSDRIVE
	return(fgetc(stream));
#else
	return(stream2fd(stream) >= 0 ? dosfgetc(stream) : fgetc(stream));
#endif
}

int Xfputc(c, stream)
int c;
FILE *stream;
{
#ifdef	_NODOSDRIVE
	return(fputc(c, stream));
#else
	return(stream2fd(stream) >= 0 ? dosfputc(c, stream) : fputc(c, stream));
#endif
}

char *Xfgets(s, n, stream)
char *s;
int n;
FILE *stream;
{
#ifdef	_NODOSDRIVE
	return(fgets(s, n, stream));
#else
	return(stream2fd(stream) >= 0 ?
		dosfgets(s, n, stream) : fgets(s, n, stream));
#endif
}

int Xfputs(s, stream)
char *s;
FILE *stream;
{
#ifdef	_NODOSDRIVE
	return(fputs(s, stream));
#else
	return(stream2fd(stream) >= 0 ?
		dosfputs(s, stream) : fputs(s, stream));
#endif
}

#ifndef	_NODOSDRIVE
char *tmpdosdupl(drive, file, mode)
int drive;
char *file;
int mode;
{
	char *cp, path[MAXPATHLEN + 1];

	sprintf(path, "%c:", drive);
	if (mktmpdir(path) < 0) {
		warning(-1, path);
		return(NULL);
	}

	cp = path + strlen(path);
	*cp = '/';
	strcpy(cp + 1, file);
	waitmes();
	if (_cpfile(file, path, mode) < 0) {
		removetmp(path, NULL, NULL);
		return(NULL);
	}

	*cp = '\0';
	if (_chdir2(path) < 0) error(path);
	return(strdup2(path));
}

int tmpdosrestore(drive, file, mode)
int drive;
char *file;
int mode;
{
	char path[MAXPATHLEN + 1];

	sprintf(path, "%c:%s", drive, file);
	waitmes();
	if (_cpfile(file, path, mode) < 0) return(-1);

	return(0);
}
#endif	/* !_NODOSDRIVE */
