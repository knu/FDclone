/*
 *	dosemu.c
 *
 *	UNIX function emulation for DOS
 */

#include "fd.h"
#include "func.h"
#include "dosdisk.h"

#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef	USEUTIME
#include <utime.h>
#endif

static char pseudocwd[MAXPATHLEN + 1] = "";
int lastdrv = -1;
int dosdrive = 0;


int _dospath(path)
char *path;
{
	if (!dosdrive) return(0);
	return((isalpha(*path) && *(path + 1) == ':') ? *path : 0);
}

int dospath(path, buf)
char *path, *buf;
{
	char *cp;
	int drive;

	if (!dosdrive) return(0);
	if (_dospath(path)) cp = path;
	else if (*path == '/' || !_dospath(pseudocwd)) return(0);
	else cp = pseudocwd;

	drive = *cp;
	if (islower(drive)) drive += 'A' - 'a';
	if (!buf) return(drive);

	strcpy(buf, cp);
	if (cp != path && *path) {
		if (*buf && buf[strlen(buf) - 1] != '/') strcat(buf, "/");
		strcat(buf, path);
	}
	return(drive);
}

DIR *Xopendir(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosopendir(buf) : opendir(path));
}

int Xclosedir(dirp)
DIR *dirp;
{
	if (*((int *)dirp) < 0) return(dosclosedir(dirp));
	closedir(dirp);
	return(0);
}

struct dirent *Xreaddir(dirp)
DIR *dirp;
{
#ifdef	NODNAMLEN
	static char buf[sizeof(struct dirent) + MAXNAMLEN];
#else
	static struct dirent buf;
#endif
	struct dirent *dp;
	int i;

	if (*((int *)dirp) >= 0) dp = readdir(dirp);
	else {
		dp = dosreaddir(dirp);
		if (dp) for (i = 0; dp -> d_name[i]; i++)
			if (isupper(dp -> d_name[i]))
				dp -> d_name[i] += 'a' - 'A';
			else if (dp -> d_name[i] == '`')
				dp -> d_name[i] = '+';
			else if (dp -> d_name[i] == '\'')
				dp -> d_name[i] = ',';
			else if (dp -> d_name[i] == '&')
				dp -> d_name[i] = '[';
			else if (dp -> d_name[i] == '$')
				dp -> d_name[i] = '.';
	}
	if (!dp) return(NULL);
	memcpy(&buf, dp, sizeof(buf));
	return((struct dirent *)(&buf));
}

int Xchdir(path)
char *path;
{
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
#ifdef	USEGETWD
		Xgetwd(pseudocwd);
#else
		Xgetcwd(pseudocwd, MAXPATHLEN);
#endif
	return(0);
}

#ifdef	USEGETWD
char *Xgetwd(path)
char *path;
{
	int i;

	if (lastdrv < 0) return(getwd(path));
	else if (!dosgetwd(path)) return(NULL);
#else
char *Xgetcwd(path, size)
char *path;
int size;
{
	int i;

	if (lastdrv < 0) return(getcwd(path, size));
	else if (!dosgetcwd(path, size)) return(NULL);
#endif
	for (i = 0; path[i]; i++)
		if (isupper(path[i])) path[i] += 'a' - 'A';
	return(path);
}

int Xstat(path, statp)
char *path;
struct stat *statp;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosstat(buf, statp) : stat(path, statp));
}

int Xlstat(path, statp)
char *path;
struct stat *statp;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? doslstat(buf, statp) : lstat(path, statp));
}

int Xaccess(path, mode)
char *path;
int mode;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosaccess(buf, mode) : access(path, mode));
}

int Xsymlink(name1, name2)
char *name1, *name2;
{
	return((dospath(name1, NULL) || dospath(name1, NULL)) ?
		dossymlink(name1, name2) : symlink(name1, name2));
}

int Xreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
	return(dospath(path, NULL) ?
		dosreadlink(path, buf, bufsiz) : readlink(path, buf, bufsiz));
}

int Xchmod(path, mode)
char *path;
int mode;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? doschmod(buf, mode) : chmod(path, mode));
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosutime(buf, times) : utime(path, times));
#else
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosutimes(buf, tvp) : utimes(path, tvp));
#endif
}

int Xunlink(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosunlink(buf) : unlink(path));
}

int Xrename(from, to)
char *from, *to;
{
	char buf1[MAXPATHLEN + 1], buf2[MAXPATHLEN + 1];

	errno = EXDEV;
	if (dospath(from, buf1))
		return(dospath(to, buf2) ? dosrename(buf1, buf2) : -1);
	else return(dospath(to, NULL) ? -1 : rename(from, to));
}

int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ?
		dosopen(buf, flags, mode) : open(path, flags, mode));
}

int Xclose(fd)
int fd;
{
	return((fd >= DOSFDOFFSET) ? dosclose(fd) : close(fd));
}

int Xread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	return((fd >= DOSFDOFFSET) ?
		dosread(fd, buf, nbytes) : read(fd, buf, nbytes));
}

int Xwrite(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	return((fd >= DOSFDOFFSET) ?
		doswrite(fd, buf, nbytes) : write(fd, buf, nbytes));
}

int Xlseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
	return((fd >= DOSFDOFFSET) ?
		doslseek(fd, offset, whence) : lseek(fd, offset, whence));
}

int Xmkdir(path, mode)
char *path;
int mode;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosmkdir(buf, mode) : mkdir(path, mode));
}

int Xrmdir(path)
char *path;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosrmdir(buf) : rmdir(path));
}

FILE *Xfopen(path, type)
char *path, *type;
{
	char buf[MAXPATHLEN + 1];

	return(dospath(path, buf) ? dosfopen(buf, type) : fopen(path, type));
}

int Xfclose(stream)
FILE *stream;
{
	return(stream2fd(stream) < 0 ? dosfclose(stream) : fclose(stream));
}

int Xfread(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	return(stream2fd(stream) < 0 ?
		dosfread(buf, size, nitems, stream) :
		fread(buf, size, nitems, stream));
}

int Xfwrite(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	return(stream2fd(stream) < 0 ?
		dosfwrite(buf, size, nitems, stream) :
		fwrite(buf, size, nitems, stream));
}

int Xfflush(stream)
FILE *stream;
{
	return(stream2fd(stream) < 0 ? dosfflush(stream) : fflush(stream));
}

int Xfgetc(stream)
FILE *stream;
{
	return(stream2fd(stream) < 0 ? dosfgetc(stream) : fgetc(stream));
}

int Xfputc(c, stream)
int c;
FILE *stream;
{
	return(stream2fd(stream) < 0 ? dosfputc(c, stream) : fputc(c, stream));
}

char *Xfgets(s, n, stream)
char *s;
int n;
FILE *stream;
{
	return(stream2fd(stream) < 0 ?
		dosfgets(s, n, stream) : fgets(s, n, stream));
}

int Xfputs(s, stream)
char *s;
FILE *stream;
{
	return(stream2fd(stream) < 0 ?
		dosfputs(s, stream) : fputs(s, stream));
}

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