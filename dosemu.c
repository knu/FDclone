/*
 *	dosemu.c
 *
 *	UNIX function emulations for DOS
 */

#include <fcntl.h>
#include "fd.h"
#include "termio.h"
#include "func.h"

#if	!defined (_NODOSDRIVE) && defined (CODEEUC)
extern int noconv;
#endif

#ifdef	_NODOSDRIVE
# if	defined (DNAMESIZE) && DNAMESIZE < (MAXNAMLEN + 1)
typedef struct _st_dirent {
	char buf[(int)sizeof(struct dirent) - DNAMESIZE + MAXNAMLEN + 1];
} st_dirent;
# else
typedef struct dirent	st_dirent;
# endif
#endif	/* _NODOSDRIVE */

#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
typedef struct _opendirpath_t {
	DIR *dirp;
	char *path;
} opendirpath_t;
#endif

#ifdef	CYGWIN
#include <sys/cygwin.h>
static struct dirent *NEAR pseudoreaddir __P_((DIR *));
#else
#define	pseudoreaddir	readdir
#endif

#ifndef	_NODOSDRIVE
int lastdrv = -1;
int dosdrive = 0;
#endif

static char cachecwd[MAXPATHLEN] = "";
#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
static opendirpath_t *dirpathlist = NULL;
static int maxdirpath = 0;
#endif


#ifndef	_NODOSDRIVE
int _dospath(path)
char *path;
{
	if (!dosdrive) return(0);

	return((isalpha2(*path) && path[1] == ':') ? *path : 0);
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

DIR *Xopendir(path)
char *path;
{
#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE) || defined (CYGWIN)
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
	if (!dirp) return(NULL);
#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE) || defined (CYGWIN)
	realpath2(path, buf, 1);
#endif
#if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
	dirpathlist = (opendirpath_t *)realloc2(dirpathlist,
		++maxdirpath * sizeof(opendirpath_t));
	cp = convput(conv, buf, 0, 1, NULL, NULL);
	dirpathlist[maxdirpath - 1].dirp = dirp;
	dirpathlist[maxdirpath - 1].path = strdup2(cp);
#endif
#ifdef	CYGWIN
#define	opendir_saw_u_cygdrive	(1 << (8 * sizeof(dirp -> __flags) - 2))
#define	opendir_saw_s_cygdrive	(1 << (8 * sizeof(dirp -> __flags) - 3))
	if (buf[0] != _SC_ || buf[1])
		dirp -> __flags |=
			opendir_saw_u_cygdrive | opendir_saw_s_cygdrive;
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
				(maxdirpath-- - i) * sizeof(opendirpath_t));
			if (maxdirpath <= 0) {
				free(dirpathlist);
				dirpathlist = NULL;
			}
		}
	}
#endif	/* !_NOKANJICONV || !_NOROCKRIDGE */
#ifndef	_NODOSDRIVE
	if (((dosDIR *)dirp) -> dd_id == DID_IFDOSDRIVE)
		return(dosclosedir(dirp));
#endif
	closedir(dirp);

	return(0);
}

#ifdef	CYGWIN
static struct dirent *NEAR pseudoreaddir(dirp)
DIR *dirp;
{
	static char *upath = NULL;
	static char *spath = NULL;
	struct dirent *dp;

	if (!upath) {
		char ubuf[MAXPATHLEN], sbuf[MAXPATHLEN];

		cygwin_internal(CW_GET_CYGDRIVE_PREFIXES, ubuf, sbuf);
		for (upath = ubuf; *upath == _SC_; upath++);
# ifdef	DEBUG
		_mtrace_file = "pseudoreaddir(upath)";
# endif
		upath = strdup2(upath);
		for (spath = sbuf; *spath == _SC_; spath++);
		if (*upath && !strpathcmp(spath, upath)) *spath = '\0';
# ifdef	DEBUG
		_mtrace_file = "pseudoreaddir(spath)";
# endif
		spath = strdup2(spath);
	}

	dp = readdir(dirp);
	if (dirp -> __d_cookie != __DIRENT_COOKIE) return(dp);

	if (!(*upath)) dirp -> __flags |= opendir_saw_u_cygdrive;
	if (!(*spath)) dirp -> __flags |= opendir_saw_s_cygdrive;

	if (dp) {
		if (*upath && !(dirp -> __flags & opendir_saw_u_cygdrive)
		&& !strpathcmp(dp -> d_name, upath))
			dirp -> __flags |= opendir_saw_u_cygdrive;
		else if (*spath && !(dirp -> __flags & opendir_saw_s_cygdrive)
		&& !strpathcmp(dp -> d_name, spath))
			dirp -> __flags |= opendir_saw_s_cygdrive;
	}
	else {
		if (!(dirp -> __flags & opendir_saw_u_cygdrive)) {
			dp = dirp -> __d_dirent;
			strcpy(dp -> d_name, upath);
			dirp -> __flags |= opendir_saw_u_cygdrive;
			dirp -> __d_position++;
		}
		else if (!(dirp -> __flags & opendir_saw_s_cygdrive)) {
			dp = dirp -> __d_dirent;
			strcpy(dp -> d_name, spath);
			dirp -> __flags |= opendir_saw_s_cygdrive;
			dirp -> __d_position++;
		}
	}

	return(dp);
}
#endif	/* CYGWIN */

struct dirent *Xreaddir(dirp)
DIR *dirp;
{
#if	defined (_NODOSDRIVE) && defined (_NOKANJIFCONV) \
&& defined (_NOROCKRIDGE)
	return(pseudoreaddir(dirp));
#else	/* !_NODOSDRIVE || !_NOKANJIFCONV || !_NOROCKRIDGE */
# if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
	char path[MAXPATHLEN], conv[MAXPATHLEN];
	int i;
# endif
	static st_dirent buf;
	struct dirent *dp;
	char *src, *dest;
	int dos;

# ifndef	_NODOSDRIVE
	if (((dosDIR *)dirp) -> dd_id == DID_IFDOSDRIVE) {
		dp = (struct dirent *)dosreaddir(dirp);
		dos = 1;
	}
	else
# endif
	{
		dp = pseudoreaddir(dirp);
		dos = 0;
	}
	if (!dp) return(NULL);

	dest = ((struct dirent *)&buf) -> d_name;
#ifdef	CYGWIN
	/* Some versions of Cygwin have neither d_fileno nor d_ino */
	if (dos) {
		src = ((struct dosdirent *)dp) -> d_name;
		buf.d_reclen = ((struct dosdirent *)dp) -> d_reclen;
	}
	else
#endif
	{
		src = dp -> d_name;
		memcpy((char *)(&buf), (char *)dp, dest - (char *)&buf);
	}

	if (isdotdir(src)) {
		strcpy(dest, src);
		return((struct dirent *)&buf);
	}
# if	!defined (_NOKANJIFCONV) || !defined (_NOROCKRIDGE)
	for (i = maxdirpath - 1; i >= 0; i--)
		if (dirp == dirpathlist[i].dirp) break;
	if (i >= 0) {
		strcatdelim2(path, dirpathlist[i].path, src);
		if (convget(conv, path, dos) == conv) {
			if ((src = strrdelim(conv, 0))) src++;
			else src = conv;
		}
		strcpy(dest, src);
	}
	else
# endif	/* !_NOKANJIFCONV || !_NOROCKRIDGE */
# if	!defined (_NODOSDRIVE) && defined (CODEEUC)
	if (dos && !noconv)
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
	if (((dosDIR *)dirp) -> dd_id == DID_IFDOSDRIVE) dosrewinddir(dirp);
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
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	_NODOSDRIVE
	n = (chdir(path)) ? -1 : 0;
#else	/* !_NODOSDRIVE */
	if ((drive = _dospath(path))) {
		if ((dd = preparedrv(drive)) < 0) n = -1;
		else if ((n = doschdir(path)) < 0) shutdrv(dd);
		else if ((n = (chdir(rootpath)) ? -1 : 0) >= 0) {
			if (lastdrv >= 0) {
				if ((lastdrv % DOSNOFILE) != (dd % DOSNOFILE))
					shutdrv(lastdrv);
				else dd = lastdrv;
			}
			lastdrv = dd;
		}
	}
	else {
		if ((n = (chdir(path)) ? -1 : 0) >= 0) {
			if (lastdrv >= 0) shutdrv(lastdrv);
			lastdrv = -1;
		}
	}
#endif	/* !_NODOSDRIVE */
	LOG1(_LOG_INFO_, n, "chdir(\"%k\");", path);
	if (n >= 0 && !Xgetwd(cachecwd)) {
		*cachecwd = '\0';
		n = -1;
	}

	return(n);
}

char *Xgetwd(path)
char *path;
{
	char *cp, conv[MAXPATHLEN];

	if (path == cachecwd || !*cachecwd) /*EMPTY*/;
#ifndef	_NODOSDRIVE
	else if (dosdrive && _dospath(cachecwd)) /*EMPTY*/;
#endif
	else {
		strcpy(path, cachecwd);
		return(path);
	}

#ifndef	_NODOSDRIVE
	if (dosdrive && lastdrv >= 0) {
		if (!(cp = dosgetcwd(path, MAXPATHLEN))) return(NULL);
		if (isupper2(cp[0])) {
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
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) n = dosstat(path, stp);
	else
#endif
	n = (stat(path, stp)) ? -1 : 0;

	return(n);
}

int Xlstat(path, stp)
char *path;
struct stat *stp;
{
#ifndef	_NOROCKRIDGE
	char rpath[MAXPATHLEN];
#endif
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 0, rpath, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) n = doslstat(path, stp);
	else
#endif
	n = (lstat(path, stp)) ? -1 : 0;
#ifndef	_NOROCKRIDGE
	if (n >= 0 && *rpath) rrlstat(rpath, stp);
#endif

	return(n);
}

int Xaccess(path, mode)
char *path;
int mode;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) n = dosaccess(path, mode);
	else
#endif
	n = (access(path, mode)) ? -1 : 0;

	return(n);
}

int Xsymlink(name1, name2)
char *name1, *name2;
{
	char conv1[MAXPATHLEN], conv2[MAXPATHLEN];
	int n;

	name1 = convput(conv1, name1, 1, 0, NULL, NULL);
	name2 = convput(conv2, name2, 1, 0, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(name1) || _dospath(name2)) n = dossymlink(name1, name2);
	else
#endif
	n = (symlink(name1, name2)) ? -1 : 0;
	LOG2(_LOG_WARNING_, n, "symlink(\"%k\", \"%k\");", name1, name2);

	return(n);
}

int Xreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
	char conv[MAXPATHLEN], lbuf[MAXPATHLEN + 1];
	int n, c;

	path = convput(conv, path, 1, 0, lbuf, &c);
#ifndef	_NOROCKRIDGE
	if (*lbuf && (n = rrreadlink(lbuf, lbuf, MAXPATHLEN)) >= 0) /*EMPTY*/;
	else
#endif
#ifndef	_NODOSDRIVE
	if (_dospath(path)) n = dosreadlink(path, buf, bufsiz);
	else
#endif
	n = readlink(path, lbuf, MAXPATHLEN);
	if (n >= 0) {
		lbuf[n] = '\0';
#ifndef	_NOKANJIFCONV
		path = kanjiconv2(conv, lbuf,
			MAXPATHLEN - 1, c, DEFCODE, L_FNAME);
#else
		path = lbuf;
#endif
		for (n = 0; n < bufsiz && path[n]; n++) buf[n] = path[n];
	}

	return(n);
}

int Xchmod(path, mode)
char *path;
int mode;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) n = doschmod(path, mode);
	else
#endif
	n = (chmod(path, mode)) ? -1 : 0;
	LOG2(_LOG_NOTICE_, n, "chmod(\"%k\", %05o);", path, mode);

	return(n);
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifndef	_NODOSDRIVE
	if (_dospath(path)) n = dosutime(path, times);
	else
# endif
	n = (utime(path, times)) ? -1 : 0;
	LOG1(_LOG_NOTICE_, n, "utime(\"%k\");", path);

	return(n);
}
#else	/* !USEUTIME */
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifndef	_NODOSDRIVE
	if (_dospath(path)) n = dosutimes(path, tvp);
	else
# endif
	n = (utimes(path, tvp)) ? -1 : 0;
	LOG1(_LOG_NOTICE_, n, "utimes(\"%k\");", path);

	return(n);
}
#endif	/* !USEUTIME */

#ifdef	HAVEFLAGS
int Xchflags(path, flags)
char *path;
u_long flags;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifndef	_NODOSDRIVE
	if (_dospath(path)) {
		errno = EACCES;
		n = -1;
	}
	else
# endif
	n = (chflags(path, flags)) ? -1 : 0;
	LOG2(_LOG_WARNING_, n, "chflags(\"%k\", %05o);", path, flags);

	return(n);
}
#endif	/* !HAVEFLAGS */

#ifndef	NOUID
int Xchown(path, uid, gid)
char *path;
uid_t uid;
gid_t gid;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifndef	_NODOSDRIVE
	if (_dospath(path)) {
		errno = EACCES;
		n = -1;
	}
	else
# endif
	n = (chown(path, uid, gid)) ? -1 : 0;
	LOG3(_LOG_WARNING_, n, "chown(\"%k\", %d, %d);", path, uid, gid);

	return(n);
}
#endif	/* !NOUID */

int Xunlink(path)
char *path;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) n = dosunlink(path);
	else
#endif
	n = (unlink(path)) ? -1 : 0;
	LOG1(_LOG_WARNING_, n, "unlink(\"%k\");", path);

	return(n);
}

int Xrename(from, to)
char *from, *to;
{
	char conv1[MAXPATHLEN], conv2[MAXPATHLEN];
	int n;

	from = convput(conv1, from, 1, 0, NULL, NULL);
	to = convput(conv2, to, 1, 0, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(from)) {
		if (_dospath(to)) n = dosrename(from, to);
		else {
			errno = EXDEV;
			n = -1;
		}
	}
	else if (_dospath(to)) {
		errno = EXDEV;
		n = -1;
	}
	else
#endif
	n = (rename(from, to)) ? -1 : 0;
	LOG2(_LOG_WARNING_, n, "rename(\"%k\", \"%k\");", from, to);

	return(n);
}

int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
	char conv[MAXPATHLEN];
	int fd;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) fd = dosopen(path, flags, mode);
	else
#endif
	fd = open(path, flags, mode);
#ifndef	_NOLOGGING
	switch (flags & O_ACCMODE) {
		case O_WRONLY:
			LOG2(_LOG_WARNING_, fd,
				"open(\"%k\", O_WRONLY, %05o);", path, mode);
			break;
		case O_RDWR:
			LOG2(_LOG_WARNING_, fd,
				"open(\"%k\", O_RDWR, %05o);", path, mode);
			break;
		default:
			LOG2(_LOG_INFO_, fd,
				"open(\"%k\", O_RDONLY, %05o);", path, mode);
			break;
	}
#endif	/* !_NOLOGGING */

	return(fd);
}

#ifndef	_NODOSDRIVE
int Xclose(fd)
int fd;
{
	int n;

	if (fd >= DOSFDOFFSET) n = dosclose(fd);
	else n = (close(fd)) ? -1 : 0;

	return(n);
}

int Xread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	int n;

	if (fd >= DOSFDOFFSET) n = dosread(fd, buf, nbytes);
	else n = read(fd, buf, nbytes);

	return(n);
}

int Xwrite(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	int n;

	if (fd >= DOSFDOFFSET) n = doswrite(fd, buf, nbytes);
	else n = write(fd, buf, nbytes);

	return(n);
}

off_t Xlseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
	off_t ofs;

	if (fd >= DOSFDOFFSET) ofs = doslseek(fd, offset, whence);
	else ofs = lseek(fd, offset, whence);

	return(ofs);
}

int Xdup(oldd)
int oldd;
{
	int fd;

	if (oldd >= DOSFDOFFSET) {
		errno = EBADF;
		fd = -1;
	}
	else fd = safe_dup(oldd);

	return(fd);
}

int Xdup2(oldd, newd)
int oldd, newd;
{
	int fd;

	if (oldd == newd) fd = newd;
	else if (oldd >= DOSFDOFFSET || newd >= DOSFDOFFSET) {
		errno = EBADF;
		fd = -1;
	}
	else fd = safe_dup2(oldd, newd);

	return(fd);
}
#endif	/* !_NODOSDRIVE */

int Xmkdir(path, mode)
char *path;
int mode;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) n = dosmkdir(path, mode);
	else
#endif
	n = (mkdir(path, mode)) ? -1 : 0;
	LOG2(_LOG_WARNING_, n, "mkdir(\"%k\", %05o);", path, mode);

	return(n);
}

int Xrmdir(path)
char *path;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) n = dosrmdir(path);
	else
#endif
	n = (rmdir(path)) ? -1 : 0;
	LOG1(_LOG_WARNING_, n, "rmdir(\"%k\");", path);

	return(n);
}

FILE *Xfopen(path, type)
char *path, *type;
{
	FILE *fp;
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NODOSDRIVE
	if (_dospath(path)) fp = dosfopen(path, type);
	else
#endif
	fp = fopen(path, type);
	LOG2((*type == 'r') ? _LOG_INFO_ : _LOG_WARNING_, (fp) ? 0 : -1,
		"fopen(\"%k\", \"%s\");", path, type);

	return(fp);
}

#ifndef	_NODOSDRIVE
FILE *Xfdopen(fd, type)
int fd;
char *type;
{
	FILE *fp;

	if (fd >= DOSFDOFFSET) fp = dosfdopen(fd, type);
	else fp = fdopen(fd, type);

	return(fp);
}

int Xfclose(stream)
FILE *stream;
{
	int n;

	if (dosfileno(stream) >= 0) n = dosfclose(stream);
	else n = fclose(stream);

	return(n);
}

int Xfileno(stream)
FILE *stream;
{
	int fd;

	if ((fd = dosfileno(stream)) >= 0) fd += DOSFDOFFSET;
	else fd = fileno(stream);

	return(fd);
}

int Xfeof(stream)
FILE *stream;
{
	int n;

	if (dosfileno(stream) >= 0) n = dosfeof(stream);
	else n = feof(stream);

	return(n);
}

int Xfread(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	int n;

	if (dosfileno(stream) >= 0) n = dosfread(buf, size, nitems, stream);
	else n = fread(buf, size, nitems, stream);

	return(n);
}

int Xfwrite(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	int n;

	if (dosfileno(stream) >= 0) n = dosfwrite(buf, size, nitems, stream);
	else n = fwrite(buf, size, nitems, stream);

	return(n);
}

int Xfflush(stream)
FILE *stream;
{
	int n;

	if (dosfileno(stream) >= 0) n = dosfflush(stream);
	else n = fflush(stream);

	return(n);
}

int Xfgetc(stream)
FILE *stream;
{
	int c;

	if (dosfileno(stream) >= 0) c = dosfgetc(stream);
	else c = fgetc(stream);

	return(c);
}

int Xfputc(c, stream)
int c;
FILE *stream;
{
	int n;

	if (dosfileno(stream) >= 0) n = dosfputc(c, stream);
	else n = fputc(c, stream);

	return(n);
}

char *Xfgets(s, n, stream)
char *s;
int n;
FILE *stream;
{
	char *cp;

	if (dosfileno(stream) >= 0) cp = dosfgets(s, n, stream);
	else cp = fgets(s, n, stream);

	return(cp);
}

int Xfputs(s, stream)
char *s;
FILE *stream;
{
	int n;

	if (dosfileno(stream) >= 0) n = dosfputs(s, stream);
	else n = fputs(s, stream);

	return(n);
}
#endif	/* !_NODOSDRIVE */
