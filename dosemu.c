/*
 *	dosemu.c
 *
 *	UNIX Function Emulation for DOS
 */

#include "fd.h"
#include "func.h"
#include "kctype.h"
#include "dosdisk.h"

#ifdef	_NOKANJIFCONV
#define	convget(buf, s)	(s)
#define	convput(buf, s)	(s)
#else
#define	convget(buf, s)	kanjiconv2(buf, s, MAXPATHLEN - 1, \
			getkcode(s), DEFCODE)
#define	convput(buf, s)	kanjiconv2(buf, s, MAXPATHLEN - 1, \
			DEFCODE, getkcode(s))

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
#endif

#ifndef	_NODOSDRIVE
static char pseudocwd[MAXPATHLEN] = "";
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

DIR *_Xopendir(path)
char *path;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif

#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosopendir(buf));
#endif
	return(opendir(convput(buf, path)));
}

#ifndef	_NOROCKRIDGE
DIR *Xopendir(path)
char *path;
{
	char buf[MAXPATHLEN];

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
#if	defined (_NOROCKRIDGE) && defined (_NODOSDRIVE) \
&& defined (_NOKANJIFCONV)
	return(readdir(dirp));
#else	/* !_NOROCKRIDGE || !_NODOSDRIVE || !_NOKANJIFCONV */
	static st_dirent buf;
	struct dirent *dp;
	char *cp;
# ifndef	_NOROCKRIDGE
	char tmp[MAXNAMLEN + 1];
# endif

# ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) dp = dosreaddir(dirp);
	else
# endif
	dp = readdir(dirp);
	if (!dp) return(NULL);
	cp = ((struct dirent *)&buf) -> d_name;
	memcpy(&buf, dp, cp - (char *)&buf);
	if (*((int *)dirp) < 0)
# ifdef	CODEEUC
		cp[sjis2ujis(cp, (u_char *)(dp -> d_name), MAXNAMLEN)] = '\0';
# else
		strcpy(cp, dp -> d_name);
# endif
	else
# ifndef	_NOKANJIFCONV
	if (convget(cp, dp -> d_name) != cp)
# endif
		strcpy(cp, dp -> d_name);
# ifndef	_NOROCKRIDGE
	if (transfile(cp, tmp) == tmp) strcpy(cp, tmp);
# endif
	return((struct dirent *)&buf);
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
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char buf[MAXPATHLEN];
#endif

#ifdef	_NODOSDRIVE
	return(chdir(convput(buf, path)));
#else	/* !_NODOSDRIVE */
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
		if (chdir(convput(buf, path)) < 0) return(-1);
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
#ifdef	USEGETWD
	return((char *)getwd(path));
#else
	return((char *)getcwd(path, MAXPATHLEN));
#endif
}

char *Xgetwd(path)
char *path;
{
#if	!defined (_NOKANJIFCONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC))
	char buf[MAXPATHLEN];
#endif
	char *cp;

#ifdef	_NODOSDRIVE
	if (!(cp = _Xgetwd(path))) return(NULL);
# ifndef	_NOKANJIFCONV
	if (convget(buf, cp) == buf) strcpy(cp, buf);
# endif
	return(cp);
#else	/* !_NODOSDRIVE */
	if (lastdrv < 0) {
		if (!(cp = _Xgetwd(path))) return(NULL);
# ifndef	_NOKANJIFCONV
		if (convget(buf, cp) == buf) strcpy(cp, buf);
# endif
		return(cp);
	}
	else if (!(cp = dosgetcwd(path, MAXPATHLEN))) return(NULL);

	if (cp[0] >= 'A' && cp[0] <= 'Z') {
		int i;

		for (i = 2; cp[i]; i++) {
			if (issjis1((u_char)(cp[i]))) i++;
			else cp[i] = tolower2(cp[i]);
		}
	}
# ifdef	CODEEUC
	buf[sjis2ujis(buf, (u_char *)cp, MAXPATHLEN - 1)] = '\0';
	strcpy(cp, buf);
# endif
	return(cp);
#endif	/* !_NODOSDRIVE */
}

int Xstat(path, stp)
char *path;
struct stat *stp;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	char buf2[MAXPATHLEN];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosstat(buf, stp));
#endif
	return(stat(convput(buf2, path), stp) ? -1 : 0);
}

#ifndef	_NOKANJIFCONV
int _Xlstat(path, stp)
char *path;
struct stat *stp;
{
	char buf[MAXPATHLEN];

	return(lstat(convput(buf, path), stp) ? -1 : 0);
}
#endif

int Xlstat(path, stp)
char *path;
struct stat *stp;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	char buf2[MAXPATHLEN];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(doslstat(buf, stp));
#endif
	return(lstat(convput(buf2, path), stp) ? -1 : 0);
}

int Xaccess(path, mode)
char *path;
int mode;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	char buf2[MAXPATHLEN];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosaccess(buf, mode));
#endif
	return(access(convput(buf2, path), mode));
}

int Xsymlink(name1, name2)
char *name1, *name2;
{
#ifndef	_NOKANJIFCONV
	char buf1[MAXPATHLEN], buf2[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN];

	if (detransfile(name1, buf, 0) == buf) name1 = buf;
#endif
#ifndef	_NODOSDRIVE
	if ((dospath(name1, NULL) || dospath(name2, NULL)))
		return(dossymlink(name1, name2));
#endif
	return(symlink(convput(buf1, name1), convput(buf2, name2)));
}

int Xreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NOKANJIFCONV)
	char tmp[MAXPATHLEN];
#endif
	char buf2[MAXPATHLEN + 1];
	int len;

#ifndef	_NOROCKRIDGE
	if (detransfile(path, tmp, 0) == tmp) {
		detransfile(path, buf2, 1);
		if (strcmp(tmp, buf2)) {
			errno = EINVAL;
			return(-1);
		}
	}
	else
#endif
	{
#ifndef	_NODOSDRIVE
		if (dospath(path, NULL))
			return(dosreadlink(path, buf, bufsiz));
#endif
		if ((len = readlink(convput(tmp, path), buf2, MAXPATHLEN)) < 0)
			return(-1);
		buf2[len] = '\0';
	}
#ifndef	_NOKANJIFCONV
	if (convget(tmp, buf2) != buf2) strcpy(buf2, tmp);
#endif
	for (len = 0; len < bufsiz && buf2[len]; len++) buf[len] = buf2[len];
	return(len);
}

int Xchmod(path, mode)
char *path;
int mode;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	char buf2[MAXPATHLEN];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(doschmod(buf, mode));
#endif
	return(chmod(convput(buf2, path), mode));
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
# if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN];
# endif
# ifndef	_NOKANJIFCONV
	char buf2[MAXPATHLEN];
# endif

# ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
# endif
# ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosutime(buf, times));
# endif
	return(utime(convput(buf2, path), times));
#else	/* !USEUTIME */
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
# if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN];
# endif
# ifndef	_NOKANJIFCONV
	char buf2[MAXPATHLEN];
# endif

# ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
# endif
# ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosutimes(buf, tvp));
# endif
	return(utimes(convput(buf2, path), tvp));
#endif	/* !USEUTIME */
}

int Xunlink(path)
char *path;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	char buf2[MAXPATHLEN];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 0) == buf) path = buf;
#endif
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosunlink(buf));
#endif
	return(unlink(convput(buf2, path)));
}

int Xrename(from, to)
char *from, *to;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE) \
|| !defined (_NOKANJIFCONV)
	char buf1[MAXPATHLEN], buf2[MAXPATHLEN];
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
	return(rename(convput(buf1, from), convput(buf2, to)));
}

int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
#if	!defined (_NOROCKRIDGE) || !defined (_NODOSDRIVE)
	char buf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	char buf2[MAXPATHLEN];
#endif

#ifndef	_NOROCKRIDGE
	if (detransfile(path, buf, 1) == buf) path = buf;
#endif
#ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosopen(buf, flags, mode));
#endif
	return(open(convput(buf2, path), flags, mode));
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

#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
int _Xmkdir(path, mode)
char *path;
int mode;
{
	char buf[MAXPATHLEN];

# ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosmkdir(buf, mode));
# endif
	return(mkdir(convput(buf, path), mode));
}

int _Xrmdir(path)
char *path;
{
	char buf[MAXPATHLEN];

# ifndef	_NODOSDRIVE
	if (dospath(path, buf)) return(dosrmdir(buf));
# endif
	return(rmdir(convput(buf, path)));
}
#endif

#ifndef	_NOROCKRIDGE
int Xmkdir(path, mode)
char *path;
int mode;
{
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 0) == buf) path = buf;
	return(_Xmkdir(path, mode));
}

int Xrmdir(path)
char *path;
{
	char buf[MAXPATHLEN];

	if (detransfile(path, buf, 0) == buf) path = buf;
	return(_Xrmdir(path));
}
#endif

#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
FILE *_Xfopen(path, type)
char *path, *type;
{
	char buf[MAXPATHLEN];

# ifndef	_NODOSDRIVE
	if (dospath(path, buf))  return(dosfopen(buf, type));
# endif
	return(fopen(convput(buf, path), type));
}
#endif

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
