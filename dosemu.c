/*
 *	dosemu.c
 *
 *	UNIX Function Emulation for DOS
 */

#include "fd.h"
#include "func.h"
#include "kctype.h"

#ifdef	_NOKANJIFCONV
#define	convget(buf, s)	(s)
#define	convput(buf, s)	(s)
#else
#define	convget(buf, s)	((nokanjifget) \
			? s : kanjiconv2(buf, s, MAXPATHLEN - 1, \
			getkcode(s), DEFCODE))
#define	convput(buf, s)	kanjiconv2(buf, s, MAXPATHLEN - 1, \
			DEFCODE, getkcode(s))
typedef struct _opendirpath_t {
	DIR *dirp;
	char *path;
} opendirpath_t;

extern char fullpath[];

int nokanjifget = 0;
char *sjispath = NULL;
char *eucpath = NULL;
char *jis7path = NULL;
char *jis8path = NULL;
char *junetpath = NULL;
char *ojis7path = NULL;
char *ojis8path = NULL;
char *ojunetpath = NULL;
char *hexpath = NULL;
char *cappath = NULL;
char *utf8path = NULL;
char *noconvpath = NULL;

static opendirpath_t *dirpathlist = NULL;
static int maxdirpath = 0;
#endif

#ifndef	_NODOSDRIVE
int lastdrv = -1;
int dosdrive = 0;

static char pseudocwd[MAXPATHLEN] = "";


int _dospath(path)
char *path;
{
	if (!dosdrive) return(0);
	return((isalpha(*path) && path[1] == ':') ? *path : 0);
}

int dospath(path, buf)
char *path, *buf;
{
	char *cp, tmp[MAXPATHLEN];
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

# ifdef	CODEEUC
	buf[ujis2sjis(buf, (u_char *)cp, MAXPATHLEN - 1)] = '\0';
# else
	strcpy(buf, cp);
# endif
	if (cp != path && *path) {
		buf = strcatdelim(buf);
# ifdef	CODEEUC
		buf[ujis2sjis(buf, (u_char *)path, MAXPATHLEN - 1)] = '\0';
# else
		strcpy(buf, path);
# endif
	}
	return(drive);
}
#endif	/* !_NODOSDRIVE */

#ifndef	_NOKANJIFCONV
int getkcode(path)
char *path;
{
	static int recurse = 0;
	int c;

	if (recurse) return(NOCNV);
	recurse = 1;
# ifndef	_NODOSDRIVE
	if (_dospath(path)) c = SJIS;
	else
# endif
	if (includepath(NULL, path, &sjispath)) c = SJIS;
	else if (includepath(NULL, path, &eucpath)) c = EUC;
	else if (includepath(NULL, path, &jis7path)) c = JIS7;
	else if (includepath(NULL, path, &jis8path)) c = JIS8;
	else if (includepath(NULL, path, &junetpath)) c = JUNET;
	else if (includepath(NULL, path, &ojis7path)) c = O_JIS7;
	else if (includepath(NULL, path, &ojis8path)) c = O_JIS8;
	else if (includepath(NULL, path, &ojunetpath)) c = O_JUNET;
	else if (includepath(NULL, path, &hexpath)) c = HEX;
	else if (includepath(NULL, path, &cappath)) c = CAP;
	else if (includepath(NULL, path, &utf8path)) c = UTF8;
	else if (includepath(NULL, path, &noconvpath)) c = NOCNV;
	else c = fnamekcode;
	recurse = 0;
	return(c);
}
#endif

DIR *_Xopendir(path, raw)
char *path;
int raw;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif
	DIR *dirp;
	char *cp;

#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = (raw) ? buf : detransfile(buf, rbuf, 1);
		return(dosopendir(path));
	}
#endif
	cp = path;
	if (!raw) {
		cp = convput(buf, cp);
		cp = detransfile(cp, rbuf, 1);
	}
	dirp = opendir(cp);
#ifndef	_NOKANJIFCONV
	if (dirp) {
		dirpathlist = (opendirpath_t *)realloc2(dirpathlist,
			(++maxdirpath + 1) * sizeof(opendirpath_t *));
		realpath2(path, buf, 1);
		dirpathlist[maxdirpath - 1].dirp = dirp;
		dirpathlist[maxdirpath - 1].path = strdup2(buf);
	}
#endif
	return(dirp);
}

int Xclosedir(dirp)
DIR *dirp;
{
#ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) return(dosclosedir(dirp));
#endif
#ifndef	_NOKANJIFCONV
	if (dirp) {
		int i;

		for (i = maxdirpath - 1; i >= 0; i--)
			if (dirp == dirpathlist[i].dirp) break;
		if (i >= 0) {
			free(dirpathlist[i++].path);
			memmove((char *)(&(dirpathlist[i - 1])),
				(char *)(&(dirpathlist[i])),
				(maxdirpath - i) * sizeof(opendirpath_t));
			if (--maxdirpath <= 0) {
				free(dirpathlist);
				dirpathlist = NULL;
			}
		}
	}
#endif	/* !_NOKANJICONV */
	closedir(dirp);
	return(0);
}

/*ARGSUSED*/
struct dirent *_Xreaddir(dirp, raw)
DIR *dirp;
int raw;
{
#if	defined (_NODOSDRIVE) && defined (_NOKANJIFCONV) \
&& defined (_NOROCKRIDGE)
	return(readdir(dirp));
#else	/* !_NODOSDRIVE || !_NOKANJIFCONV || !_NOROCKRIDGE */
	static st_dirent buf;
	struct dirent *dp;
	char *src, *dest;
# ifndef	_NOROCKRIDGE
	char rbuf[MAXNAMLEN + 1];
# endif
# ifndef	_NOKANJIFCONV
	char kbuf[MAXPATHLEN], duplpath[MAXPATHLEN];
# endif

# ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) dp = dosreaddir(dirp);
	else
# endif
	dp = readdir(dirp);
	if (!dp) return(NULL);
	src = dp -> d_name;
	dest = ((struct dirent *)&buf) -> d_name;
	memcpy(&buf, dp, dest - (char *)&buf);
	if (!raw) src = transfile(src, rbuf);

	if (*((int *)dirp) < 0)
# ifdef	CODEEUC
		dest[sjis2ujis(dest, (u_char *)src, MAXNAMLEN)] = '\0';
# else
		strcpy(dest, src);
# endif
	else {
# ifndef	_NOKANJIFCONV
		int i;

		for (i = maxdirpath - 1; i >= 0; i--)
			if (dirp == dirpathlist[i].dirp) break;
		if (i >= 0) {
			strcpy(duplpath, fullpath);
			strcpy(fullpath, dirpathlist[i].path);
		}
		if (!raw) src = convget(kbuf, src);
		if (i >= 0) strcpy(fullpath, duplpath);
# endif
		strcpy(dest, src);
	}

	return((struct dirent *)&buf);
#endif	/* !_NODOSDRIVE || !_NOKANJICONV || !_NOROCKRIDGE */
}

VOID Xrewinddir(dirp)
DIR *dirp;
{
#ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) dosrewinddir(dirp);
#endif
	rewinddir(dirp);
}

int _Xchdir(path, notrans)
char *path;
int notrans;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifdef	_NODOSDRIVE
	path = convput(buf, path);
	if (!notrans) path = detransfile(path, rbuf, 1);
	return(chdir(path));
#else	/* !_NODOSDRIVE */
	int dd, drive;

	if ((drive = dospath(path, buf))) {
		path = (notrans) ? buf : detransfile(buf, rbuf, 1);
		if ((dd = preparedrv(drive)) < 0) return(-1);
		if (doschdir(path) < 0) {
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
		path = convput(buf, path);
		if (!notrans) path = detransfile(path, rbuf, 1);
		if (chdir(path) < 0) return(-1);
		if (lastdrv >= 0) shutdrv(lastdrv);
		lastdrv = -1;
	}
	if (!Xgetwd(pseudocwd)) {
		*pseudocwd = '\0';
		return(-1);
	}
	return(0);
#endif	/* !_NODOSDRIVE */
}

/*ARGSUSED*/
char *_Xgetwd(path, nodos)
char *path;
int nodos;
{
#if	(!defined (_NODOSDRIVE) && defined (CODEEUC)) \
|| !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif
	char *cp;

#ifndef	_NODOSDRIVE
	if (!nodos && lastdrv >= 0) {
		if (!(cp = dosgetcwd(path, MAXPATHLEN))) return(NULL);
		if (cp[0] >= 'A' && cp[0] <= 'Z') {
			int i;

			for (i = 2; cp[i]; i++) {
				if (issjis1((u_char)(cp[i]))) i++;
				else cp[i] = tolower2(cp[i]);
			}
		}
		cp = transfile(cp, rbuf);
# ifdef	CODEEUC
		buf[sjis2ujis(buf, (u_char *)cp, MAXPATHLEN - 1)] = '\0';
		cp = buf;
# endif
	}
	else
#endif	/* !_NODOSDRIVE */
	{
#ifdef	USEGETWD
		cp = (char *)getwd(path);
#else
		cp = (char *)getcwd(path, MAXPATHLEN);
#endif
		if (!cp) return(NULL);
		cp = transfile(cp, rbuf);
		cp = convget(buf, cp);
	}
	if (cp == path) return(cp);
	strcpy(path, cp);
	return(path);
}

int Xstat(path, stp)
char *path;
struct stat *stp;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = detransfile(buf, rbuf, 1);
		return(dosstat(path, stp));
	}
#endif
	path = convput(buf, path);
	path = detransfile(path, rbuf, 1);
	return(stat(path, stp) ? -1 : 0);
}

/*ARGSUSED*/
int _Xlstat(path, stp, raw, nodos)
char *path;
struct stat *stp;
int raw, nodos;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (!nodos && dospath(path, buf)) {
		path = (raw) ? buf : detransfile(buf, rbuf, 1);
		return(doslstat(path, stp));
	}
#endif
	if (!raw) {
		path = convput(buf, path);
		path = detransfile(path, rbuf, 1);
	}
	return(lstat(path, stp) ? -1 : 0);
}

int Xaccess(path, mode)
char *path;
int mode;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = detransfile(buf, rbuf, 1);
		return(dosaccess(path, mode));
	}
#endif
	path = convput(buf, path);
	path = detransfile(path, rbuf, 1);
	return(access(path, mode));
}

int Xsymlink(name1, name2)
char *name1, *name2;
{
#ifndef	_NOKANJIFCONV
	char buf1[MAXPATHLEN], buf2[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf1[MAXPATHLEN], rbuf2[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if ((dospath(name1, NULL) || dospath(name2, NULL)))
		return(dossymlink(name1, name2));
#endif
	name1 = convput(buf1, name1);
	name1 = detransfile(name1, rbuf1, 0);
	name2 = convput(buf2, name2);
	name2 = detransfile(name2, rbuf2, 0);
	return(symlink(name1, name2));
}

int Xreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
#ifndef	_NOKANJIFCONV
	char kbuf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif
	char lbuf[MAXPATHLEN + 1];
	int len;

#ifndef	_NODOSDRIVE
	if (dospath(path, NULL)) return(dosreadlink(path, buf, bufsiz));
#endif
	path = convput(kbuf, path);
	path = detransfile(path, rbuf, 0);
#ifndef	_NOROCKRIDGE
	if (path == rbuf) {
		detransfile(path, lbuf, 1);
		if (strcmp(path, lbuf)) {
			errno = EINVAL;
			return(-1);
		}
	}
	else
#endif
	{
		if ((len = readlink(path, lbuf, MAXPATHLEN)) < 0) return(-1);
		lbuf[len] = '\0';
	}
	path = convget(kbuf, lbuf);
	for (len = 0; len < bufsiz && path[len]; len++) buf[len] = path[len];
	return(len);
}

int Xchmod(path, mode)
char *path;
int mode;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = detransfile(buf, rbuf, 1);
		return(doschmod(path, mode));
	}
#endif
	path = convput(buf, path);
	path = detransfile(path, rbuf, 1);
	return(chmod(path, mode));
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
# if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
# endif
# ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
# endif

# ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = detransfile(buf, rbuf, 1);
		return(dosutime(path, times));
	}
# endif
	path = convput(buf, path);
	path = detransfile(path, rbuf, 1);
	return(utime(path, times));
#else	/* !USEUTIME */
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
# if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
# endif
# ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
# endif

# ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = detransfile(buf, rbuf, 1);
		return(dosutimes(path, tvp));
	}
# endif
	path = convput(buf, path);
	path = detransfile(path, rbuf, 1);
	return(utimes(path, tvp));
#endif	/* !USEUTIME */
}

int Xunlink(path)
char *path;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = detransfile(buf, rbuf, 1);
		return(dosunlink(path));
	}
#endif
	path = convput(buf, path);
	path = detransfile(path, rbuf, 1);
	return(unlink(path));
}

int _Xrename(from, to, raw)
char *from, *to;
int raw;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf1[MAXPATHLEN], buf2[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf1[MAXPATHLEN], rbuf2[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (dospath(from, buf1)) {
		if (!dospath(to, buf2)) {
			errno = EXDEV;
			return(-1);
		}
		if (raw) {
			from = buf1;
			to = buf2;
		}
		else {
			from = detransfile(buf1, rbuf1, 0);
			to = detransfile(buf2, rbuf2, 0);
		}
		return(dosrename(from, to));
	}
	else if (dospath(to, NULL)) {
		errno = EXDEV;
		return(-1);
	}
#endif
	if (!raw) {
		from = convput(buf1, from);
		from = detransfile(from, rbuf1, 0);
		to = convput(buf2, to);
		to = detransfile(to, rbuf2, 0);
	}
	return(rename(from, to));
}

int _Xopen(path, flags, mode, raw)
char *path;
int flags, mode, raw;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = (raw) ? buf : detransfile(buf, rbuf, 1);
		return(dosopen(path, flags, mode));
	}
#endif
	if (!raw) {
		path = convput(buf, path);
		path = detransfile(path, rbuf, 1);
	}
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

int Xdup(oldd)
int oldd;
{
	if (oldd >= DOSFDOFFSET) {
		errno = EBADF;
		return(-1);
	}
	return(dup(oldd));
}

int Xdup2(oldd, newd)
int oldd, newd;
{
	if (oldd >= DOSFDOFFSET || newd >= DOSFDOFFSET) {
		errno = EBADF;
		return(-1);
	}
	return(dup2(oldd, newd));
}
#endif	/* !_NODOSDRIVE */

/*ARGSUSED*/
int _Xmkdir(path, mode, raw, nodos)
char *path;
int mode, raw, nodos;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (!nodos && dospath(path, buf)) {
		path = (raw) ? buf : detransfile(buf, rbuf, 1);
		return(dosmkdir(path, mode));
	}
#endif
	if (!raw) {
		path = convput(buf, path);
		path = detransfile(path, rbuf, 1);
	}
	return(mkdir(path, mode));
}

/*ARGSUSED*/
int _Xrmdir(path, raw, nodos)
char *path;
int raw, nodos;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (!nodos && dospath(path, buf)) {
		path = (raw) ? buf : detransfile(buf, rbuf, 1);
		return(dosrmdir(path));
	}
#endif
	if (!raw) {
		path = convput(buf, path);
		path = detransfile(path, rbuf, 1);
	}
	return(rmdir(path));
}

FILE *_Xfopen(path, type, notrans)
char *path, *type;
int notrans;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif

# ifndef	_NODOSDRIVE
	if (dospath(path, buf)) {
		path = (notrans) ? buf : detransfile(buf, rbuf, 1);
		return(dosfopen(path, type));
	}
# endif
	path = convput(buf, path);
	if (!notrans) path = detransfile(path, rbuf, 1);
	return(fopen(path, type));
}

#ifndef	_NODOSDRIVE
FILE *Xfdopen(fd, type)
int fd;
char *type;
{
	if (fd >= DOSFDOFFSET) return(dosfdopen(fd, type));
	return(fdopen(fd, type));
}

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
