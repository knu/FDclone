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
	else if (*path == _SC_ || !_dospath(pseudocwd)) return(0);
	else cp = pseudocwd;

	drive = *cp;
	if (!buf) return(drive);
	if (cp == buf) {
		strcpy(tmp, cp);
		cp = tmp;
	}

#ifdef	CODEEUC
	buf[ujis2sjis(buf, (u_char *)cp)] = '\0';
#else
	strcpy(buf, cp);
#endif
	if (cp != path && *path) {
		if (!isdelim(buf, (int)strlen(buf) - 1)) strcat(buf, _SS_);
#ifdef	CODEEUC
		buf += strlen(buf);
		buf[ujis2sjis(buf, (u_char *)path)] = '\0';
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
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN + 1];

	if (dospath(path, buf)) return(dosopendir(buf));
#endif
	return(opendir(path));
}

#ifndef	_NOROCKRIDGE
DIR *Xopendir(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 1) == buf) path = buf;
	return(_Xopendir(path));
}
#endif

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
	memcpy(&buf, dp,
		(char *)(((struct dirent *)&buf) -> d_name) - (char *)&buf);
	strcpy(((struct dirent *)&buf) -> d_name, dp -> d_name);
#if	defined (CODEEUC) && !defined (_NODOSDRIVE)
	if (*((int *)dirp) < 0) {
		i = sjis2ujis(((struct dirent *)&buf) -> d_name,
			(u_char *)(dp -> d_name));
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

	if ((drive = dospath(path, buf))) {
		if ((dd = preparedrv(drive)) < 0) return(-1);
		if (doschdir(buf) < 0) {
			shutdrv(dd);
			return(-1);
		}
		if (chdir(_SS_) < 0) return(-1);
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

#ifndef	_NOROCKRIDGE
int Xchdir(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 1) == buf) path = buf;
	return(_Xchdir(path));
}
#endif

/*ARGSUSED*/
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
	tmpbuf[sjis2ujis(tmpbuf, (u_char *)path)] = '\0';
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
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosstat(buf, statp));
#endif
	return(stat(path, statp));
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
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(doslstat(buf, statp));
#endif
	return(lstat(path, statp));
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
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosaccess(buf, mode));
#endif
	return(access(path, mode));
}

int Xsymlink(name1, name2)
char *name1, *name2;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN + 1];

	if (detransfile(name1, buf, 0) == buf) name1 = buf;
#endif
#ifndef	_NODOSDRIVE
	if ((dospath(name1, NULL) || dospath(name2, NULL)))
		return(dossymlink(name1, name2));
#endif
	return(symlink(name1, name2));
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
#ifndef	_NODOSDRIVE
	if (dospath(path, NULL)) return(dosreadlink(path, buf, bufsiz));
#endif
	return(readlink(path, buf, bufsiz));
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
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(doschmod(buf, mode));
#endif
	return(chmod(path, mode));
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
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosutime(buf, times));
#endif
	return(utime(path, times));
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
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosutimes(buf, tvp));
#endif
	return(utimes(path, tvp));
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
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosunlink(buf));
#endif
	return(unlink(path));
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
#ifndef	_NODOSDRIVE
	errno = EXDEV;
	if (dospath(from, buf1))
		return(dospath(to, buf2) ? dosrename(buf1, buf2) : -1);
	else if (dospath(to, NULL)) return(-1);
#endif
	return(rename(from, to));
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
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosopen(buf, flags, mode));
#endif
	return(open(path, flags, mode));
}

#ifndef	_NODOSDRIVE
int Xclose(fd)
int fd;
{
	if (fd >= DOSFDOFFSET) return(dosclose(fd));
	return(close(fd));
}

int Xread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	if (fd >= DOSFDOFFSET) return(dosread(fd, buf, nbytes));
	return(read(fd, buf, nbytes));
}

int Xwrite(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	if (fd >= DOSFDOFFSET) return(doswrite(fd, buf, nbytes));
	return(write(fd, buf, nbytes));
}

off_t Xlseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
	if (fd >= DOSFDOFFSET) return(doslseek(fd, offset, whence));
	return(lseek(fd, offset, whence));
}
#endif	/* !_NODOSDRIVE */

int _Xmkdir(path, mode)
char *path;
int mode;
{
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN + 1];

	if (dospath(path, buf)) return(dosmkdir(buf, mode));
#endif
	return(mkdir(path, mode));
}

#ifndef	_NOROCKRIDGE
int Xmkdir(path, mode)
char *path;
int mode;
{
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 0) == buf) path = buf;
	return(_Xmkdir(path, mode));
}
#endif

#ifndef	_NODOSDRIVE
int _Xrmdir(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

	if (dospath(path, buf)) return(dosrmdir(buf));
	return(rmdir(path));
}
#endif

#ifndef	_NOROCKRIDGE
int Xrmdir(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 0) == buf) path = buf;
	return(_Xrmdir(path));
}
#endif

#ifndef	_NODOSDRIVE
FILE *_Xfopen(path, type)
char *path, *type;
{
	char buf[MAXPATHLEN + 1];

	if (dospath(path, buf))  return(dosfopen(buf, type));
	return(fopen(path, type));
}
#endif

#ifndef	_NOROCKRIDGE
FILE *Xfopen(path, type)
char *path, *type;
{
	char buf[MAXPATHLEN + 1];

	if (detransfile(path, buf, 1) == buf) path = buf;
	return(_Xfopen(path, type));
}
#endif

#ifndef	_NODOSDRIVE
int Xfclose(stream)
FILE *stream;
{
	if (dosfileno(stream) >= 0) return(dosfclose(stream));
	return(fclose(stream));
}

int Xfeof(stream)
FILE *stream;
{
	if (dosfileno(stream) >= 0) return(dosfeof(stream));
	return(feof(stream));
}

int Xfread(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	if (dosfileno(stream) >= 0)
		return(dosfread(buf, size, nitems, stream));
	return(fread(buf, size, nitems, stream));
}

int Xfwrite(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	if (dosfileno(stream) >= 0)
		return(dosfwrite(buf, size, nitems, stream));
	return(fwrite(buf, size, nitems, stream));
}

int Xfflush(stream)
FILE *stream;
{
	if (dosfileno(stream) >= 0) return(dosfflush(stream));
	return(fflush(stream));
}

int Xfgetc(stream)
FILE *stream;
{
	if (dosfileno(stream) >= 0) return(dosfgetc(stream));
	return(fgetc(stream));
}

int Xfputc(c, stream)
int c;
FILE *stream;
{
	if (dosfileno(stream) >= 0) return(dosfputc(c, stream));
	return(fputc(c, stream));
}

char *Xfgets(s, n, stream)
char *s;
int n;
FILE *stream;
{
	if (dosfileno(stream) >= 0) return(dosfgets(s, n, stream));
	return(fgets(s, n, stream));
}

int Xfputs(s, stream)
char *s;
FILE *stream;
{
	if (dosfileno(stream) >= 0) return(dosfputs(s, stream));
	return(fputs(s, stream));
}
#endif	/* !_NODOSDRIVE */
