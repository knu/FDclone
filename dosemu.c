/*
 *	dosemu.c
 *
 *	UNIX Function Emulation for DOS
 */

#include "fd.h"
#include "func.h"
#include "kctype.h"

extern int norealpath;
#ifndef	_NOROCKRIDGE
extern int norockridge;
#endif

#ifdef	_NODOSDRIVE
# if	defined (DNAMESIZE) && DNAMESIZE < (MAXNAMLEN + 1)
typedef struct _st_dirent {
	char buf[sizeof(struct dirent) - DNAMESIZE + MAXNAMLEN + 1];
} st_dirent;
# else
typedef struct dirent	st_dirent;
# endif
#endif

#ifndef	_NOKANJIFCONV
typedef struct _kpathtable {
	char **path;
	u_short code;
} kpathtable;
#endif

#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
typedef struct _opendirpath_t {
	DIR *dirp;
	char *path;
} opendirpath_t;
#endif

int noconv = 0;
#ifndef	_NODOSDRIVE
int lastdrv = -1;
int dosdrive = 0;
#endif
#ifndef	_NOKANJIFCONV
int nokanjifconv = 0;
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
#endif	/* !_NOKANJIFCONV */

static char cachecwd[MAXPATHLEN] = "";
#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
static opendirpath_t *dirpathlist = NULL;
static int maxdirpath = 0;
#endif
#ifndef	_NOKANJIFCONV
static kpathtable kpathlist[] = {
	{ &sjispath, SJIS },
	{ &eucpath, EUC },
	{ &jis7path, JIS7 },
	{ &jis8path, JIS8 },
	{ &junetpath, JUNET },
	{ &ojis7path, O_JIS7 },
	{ &ojis8path, O_JIS8 },
	{ &ojunetpath, O_JUNET },
	{ &hexpath, HEX },
	{ &cappath, CAP },
	{ &utf8path, UTF8 },
	{ &noconvpath, NOCNV },
};
#define	MAXKPATHLIST	(sizeof(kpathlist) / sizeof(kpathtable))
#endif


#ifndef	_NODOSDRIVE
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
	int drive, len;

	if (!dosdrive) return(0);
	if (_dospath(path)) cp = path;
	else if (*path == _SC_ || !_dospath(cachecwd)) return(0);
	else cp = cachecwd;

	drive = *cp;
	if (!buf) return(drive);
	len = MAXPATHLEN - 1;
	if (cp == buf) {
		strncpy2(tmp, cp, len);
		cp = tmp;
	}

# ifdef	CODEEUC
	if (!noconv || cp != path)
		buf[ujis2sjis(buf, (u_char *)cp, len)] = '\0';
	else
# endif
	strncpy2(buf, cp, len);
	if (cp != path && *path) {
		cp = strcatdelim(buf);
		len -= cp - buf;
# ifdef	CODEEUC
		if (!noconv) cp[ujis2sjis(cp, (u_char *)path, len)] = '\0';
		else
# endif
		strncpy2(cp, path, len);
	}

	return(drive);
}
#endif	/* !_NODOSDRIVE */

#ifndef	_NOKANJIFCONV
int getkcode(path)
char *path;
{
	int i;

# ifndef	_NODOSDRIVE
	if (_dospath(path)) return(SJIS);
# endif
	for (i = 0; i < MAXKPATHLIST; i++) {
		if (includepath(path, *(kpathlist[i].path)))
			return(kpathlist[i].code);
	}
	return(fnamekcode);
}
#endif	/* !_NOKANJIFCONV */

/*ARGSUSED*/
char *convget(buf, path, dos)
char *buf, *path;
int dos;
{
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	int fgetok;
#endif
	char *cp;

	if (noconv) return(path);
#ifndef	_NOKANJIFCONV
	fgetok = (nokanjifconv) ? 0 : 1;
#endif

	cp = path;
#ifndef	_NOROCKRIDGE
	if (!norockridge) cp = transpath(cp, rbuf);
#endif
#ifndef	_NODOSDRIVE
	if (dos) {
# ifdef	CODEEUC
		buf[sjis2ujis(buf, (u_char *)cp, MAXPATHLEN - 1)] = '\0';
		cp = buf;
# endif
# ifndef	_NOKANJIFCONV
		fgetok = 0;
# endif
	}
#endif
#ifndef	_NOKANJIFCONV
	if (fgetok) cp = kanjiconv2(buf, cp,
		MAXPATHLEN - 1, getkcode(cp), DEFCODE);
#endif
	if (cp == path) return(cp);
	strcpy(buf, cp);
	return(buf);
}

/*ARGSUSED*/
char *convput(buf, path, needfile, rdlink, rrreal, codep)
char *buf, *path;
int needfile, rdlink;
char *rrreal;
int *codep;
{
#if	!defined (_NODOSDRIVE) || !defined (_NOKANJIFCONV)
	char kbuf[MAXPATHLEN];
#endif
#ifndef	_NOROCKRIDGE
	char rbuf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	int fputok;
#endif
	char *cp, *file, rpath[MAXPATHLEN];
	int n;

	if (rrreal) *rrreal = '\0';
	if (codep) *codep = NOCNV;

	if (noconv || isdotdir(path)) {
#ifndef	_NODOSDRIVE
		if (dospath(path, buf)) return(buf);
#endif
		return(path);
	}
#ifndef	_NOKANJIFCONV
	fputok = (nokanjifconv) ? 0 : 1;
#endif

	if (needfile && strdelim(path, 0)) needfile = 0;
	if (norealpath) cp = path;
	else {
		if ((file = strrdelim(path, 0))) {
			n = file - path;
			if (file++ == isrootdir(path)) n++;
			strncpy2(rpath, path, n);
		}
#ifndef	_NODOSDRIVE
		else if ((n = _dospath(path))) {
			file = path + 2;
			rpath[0] = n;
			rpath[1] = ':';
			rpath[2] = '.';
			rpath[3] = '\0';
		}
#endif
		else {
			file = path;
			strcpy(rpath, ".");
		}
		realpath2(rpath, rpath, 1);
		cp = strcatdelim(rpath);
		strncpy2(cp, file, MAXPATHLEN - 1 - (cp - rpath));
		cp = rpath;
	}
#ifndef	_NODOSDRIVE
	if ((n = dospath(cp, kbuf))) {
		cp = kbuf;
		if (codep) *codep = SJIS;
# ifndef	_NOKANJIFCONV
		fputok = 0;
# endif
	}
#endif
#ifndef	_NOKANJIFCONV
	if (fputok) {
		int c;

		c = getkcode(cp);
		if (codep) *codep = c;
		cp = kanjiconv2(kbuf, cp, MAXPATHLEN - 1, DEFCODE, c);
	}
#endif
#ifndef	_NOROCKRIDGE
	if (!norockridge && (cp = detranspath(cp, rbuf)) == rbuf) {
		if (rrreal) strcpy(rrreal, rbuf);
		if (rdlink && rrreadlink(cp, buf, MAXPATHLEN - 1) >= 0) {
			if (needfile && strdelim(buf, 0)) needfile = 0;
			if (*buf == _SC_ || !(cp = strrdelim(rbuf, 0)))
				cp = buf;
			else {
				cp++;
				strncpy2(cp, buf, MAXPATHLEN - (cp - rbuf));
				cp = rbuf;
			}
			realpath2(cp, rpath, 1);
			cp = detranspath(rpath, rbuf);
		}
	}
#endif
	if (cp == path) return(path);
	if (needfile && (file = strrdelim(cp, 0))) file++;
	else file = cp;
#ifndef	_NODOSDRIVE
	if (n && !_dospath(file)) {
		buf[0] = n;
		buf[1] = ':';
		strcpy(&(buf[2]), file);
	}
	else
#endif
	strcpy(buf, file);
	return(buf);
}

DIR *Xopendir(path)
char *path;
{
#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
	char buf[MAXPATHLEN];
#endif
	DIR *dirp;
	char *cp, conv[MAXPATHLEN];

	cp = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(cp)) dirp = dosopendir(cp);
	else
#endif
	dirp = opendir(cp);
#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
	if (dirp) {
		dirpathlist = (opendirpath_t *)realloc2(dirpathlist,
			++maxdirpath * sizeof(opendirpath_t));
		realpath2(path, buf, 1);
		cp = convput(conv, buf, 0, 1, NULL, NULL);
		dirpathlist[maxdirpath - 1].dirp = dirp;
		dirpathlist[maxdirpath - 1].path = strdup2(cp);
	}
#endif
	return(dirp);
}

int Xclosedir(dirp)
DIR *dirp;
{
#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
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
#endif	/* !_NOKANJICONV || !_NOROCKRIDGE */
#ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) return(dosclosedir(dirp));
#endif
	closedir(dirp);
	return(0);
}

struct dirent *Xreaddir(dirp)
DIR *dirp;
{
#if	defined (_NODOSDRIVE) && defined (_NOKANJIFCONV) \
&& defined (_NOROCKRIDGE)
	return(readdir(dirp));
#else	/* !_NODOSDRIVE || !_NOKANJIFCONV || !_NOROCKRIDGE */
# if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
	char path[MAXPATHLEN], conv[MAXPATHLEN];
	int i;
# endif
	static st_dirent buf;
	struct dirent *dp;
	char *src, *dest;

# ifndef	_NODOSDRIVE
	if (*((int *)dirp) < 0) dp = dosreaddir(dirp);
	else
# endif
	dp = readdir(dirp);
	if (!dp) return(NULL);
	src = dp -> d_name;
	dest = ((struct dirent *)&buf) -> d_name;
	memcpy((char *)(&buf), (char *)dp, dest - (char *)&buf);
	if (isdotdir(src)) {
		strcpy(dest, src);
		return((struct dirent *)&buf);
	}
# if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
	for (i = maxdirpath - 1; i >= 0; i--)
		if (dirp == dirpathlist[i].dirp) break;
	if (i >= 0) {
		strcatdelim2(path, dirpathlist[i].path, src);
		if (convget(conv, path, *((int *)dirp) < 0) == conv) {
			if ((src = strrdelim(conv, 0))) src++;
			else src = conv;
		}
		strcpy(dest, src);
	}
	else
# endif	/* !_NOKANJIFCONV || !_NOROCKRIDGE */
# if	!defined (_NODOSDRIVE) && defined (CODEEUC)
	if (*((int *)dirp) < 0 && !noconv)
		dest[sjis2ujis(dest, (u_char *)src, MAXNAMLEN)] = '\0';
	else
# endif
	strcpy(dest, src);

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

int Xchdir(path)
char *path;
{
#ifndef	_NODOSDRIVE
	int dd, drive;
#endif
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	_NODOSDRIVE
	if (chdir(path) < 0) return(-1);
#else	/* !_NODOSDRIVE */
	if ((drive = _dospath(path))) {
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
		if (chdir(path) < 0) return(-1);
		if (lastdrv >= 0) shutdrv(lastdrv);
		lastdrv = -1;
	}
#endif	/* !_NODOSDRIVE */
	if (!Xgetwd(cachecwd)) {
		*cachecwd = '\0';
		return(-1);
	}
	return(0);
}

char *Xgetwd(path)
char *path;
{
	char *cp, conv[MAXPATHLEN];

	if (path == cachecwd || !*cachecwd);
#ifndef	_NODOSDRIVE
	else if (dosdrive && _dospath(cachecwd));
#endif
	else {
		strcpy(path, cachecwd);
		return(path);
	}

#ifndef	_NODOSDRIVE
	if (dosdrive && lastdrv >= 0) {
		if (!(cp = dosgetcwd(path, MAXPATHLEN))) return(NULL);
		if (cp[0] >= 'A' && cp[0] <= 'Z') {
			int i;

			for (i = 2; cp[i]; i++) {
				if (issjis1((u_char)(cp[i]))) i++;
				else cp[i] = tolower2(cp[i]);
			}
		}
		cp = convget(conv, cp, 1);
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
		cp = convget(conv, cp, 0);
	}
	if (cp == path) return(cp);
	strcpy(path, cp);
	return(path);
}

int Xstat(path, stp)
char *path;
struct stat *stp;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosstat(path, stp));
#endif
	return(stat(path, stp) ? -1 : 0);
}

int Xlstat(path, stp)
char *path;
struct stat *stp;
{
#ifndef	_NOROCKRIDGE
	char rpath[MAXPATHLEN];
#endif
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 0, rpath, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) {
		if (doslstat(path, stp) < 0) return(-1);
	}
	else
#endif
	if (lstat(path, stp)) return(-1);
#ifndef	_NOROCKRIDGE
	if (*rpath) rrlstat(rpath, stp);
#endif
	return(0);
}

int Xaccess(path, mode)
char *path;
int mode;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosaccess(path, mode));
#endif
	return(access(path, mode) ? -1 : 0);
}

int Xsymlink(name1, name2)
char *name1, *name2;
{
	char conv1[MAXPATHLEN], conv2[MAXPATHLEN];

	name1 = convput(conv1, name1, 1, 0, NULL, NULL);
	name2 = convput(conv2, name2, 1, 0, NULL, NULL);
#ifndef	_NODOSDRIVE
	if ((_dospath(name1) || _dospath(name2)))
		return(dossymlink(name1, name2));
#endif
	return(symlink(name1, name2) ? -1 : 0);
}

int Xreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
	char conv[MAXPATHLEN], lbuf[MAXPATHLEN + 1];
	int c, len;

	path = convput(conv, path, 1, 0, lbuf, &c);
#ifndef	_NOROCKRIDGE
	if (*lbuf && (len = rrreadlink(lbuf, lbuf, MAXPATHLEN)) >= 0);
	else
#endif
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosreadlink(path, buf, bufsiz));
	else
#endif
	if ((len = readlink(path, lbuf, MAXPATHLEN)) < 0) return(-1);
	lbuf[len] = '\0';
#ifndef	_NOKANJIFCONV
	path = kanjiconv2(conv, lbuf, MAXPATHLEN - 1, c, DEFCODE);
#else
	path = lbuf;
#endif
	for (len = 0; len < bufsiz && path[len]; len++) buf[len] = path[len];
	return(len);
}

int Xchmod(path, mode)
char *path;
int mode;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(doschmod(path, mode));
#endif
	return(chmod(path, mode) ? -1 : 0);
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosutime(path, times));
# endif
	return(utime(path, times) ? -1 : 0);
}
#else	/* !USEUTIME */
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosutimes(path, tvp));
# endif
	return(utimes(path, tvp) ? -1 : 0);
}
#endif	/* !USEUTIME */

int Xunlink(path)
char *path;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosunlink(path));
#endif
	return(unlink(path) ? -1 : 0);
}

int Xrename(from, to)
char *from, *to;
{
	char conv1[MAXPATHLEN], conv2[MAXPATHLEN];

	from = convput(conv1, from, 1, 0, NULL, NULL);
	to = convput(conv2, to, 1, 0, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(from)) {
		if (!_dospath(to)) {
			errno = EXDEV;
			return(-1);
		}
		return(dosrename(from, to));
	}
	else if (_dospath(to)) {
		errno = EXDEV;
		return(-1);
	}
#endif
	return(rename(from, to) ? -1 : 0);
}

int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosopen(path, flags, mode));
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

int Xmkdir(path, mode)
char *path;
int mode;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosmkdir(path, mode));
#endif
	return(mkdir(path, mode) ? -1 : 0);
}

int Xrmdir(path)
char *path;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosrmdir(path));
#endif
	return(rmdir(path) ? -1 : 0);
}

FILE *Xfopen(path, type)
char *path, *type;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) return(dosfopen(path, type));
#endif
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
