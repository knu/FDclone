/*
 *	unixemu.c
 *
 *	UNIX function emulation on DOS
 */

#include <fcntl.h>
#include "fd.h"
#include "termio.h"
#include "func.h"
#include "unixdisk.h"

#ifndef	O_ACCMODE
#define	O_ACCMODE	(O_RDONLY | O_WRONLY | O_RDWR)
#endif

#ifndef	_NOROCKRIDGE
typedef struct _opendirpath_t {
	DIR *dirp;
	char *path;
} opendirpath_t;
#endif

#ifndef	_NODOSDRIVE
static int NEAR checkpath __P_((char *, char *));
#endif
static int NEAR statcommon __P_((char *, struct stat *));

#ifndef	_NODOSDRIVE
int lastdrv = -1;
#endif

#ifndef	_NOROCKRIDGE
static opendirpath_t *dirpathlist = NULL;
static int maxdirpath = 0;
#endif


int _dospath(path)
char *path;
{
	return((isalpha2(*path) && path[1] == ':') ? *path : 0);
}

int dospath(path, buf)
char *path, *buf;
{
	char tmp[MAXPATHLEN];
	int drv;

	if (path == buf) {
		strcpy(tmp, path);
		path = tmp;
	}
	if (buf) {
#ifndef	_NOUSELFN
		if (shortname(path, buf) == buf);
		else
#endif
		strcpy(buf, path);
	}
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
		strcpy(tmp, cp);
		cp = tmp;
	}
	if (*cp == _SC_) buf = gendospath(buf, drive, '\0');
	else {
		if (!dosgetcwd(buf, MAXPATHLEN)) return(0);
		buf = strcatdelim(buf);
	}
	strcpy(buf, cp);

	return(drive);
}
#endif	/* !_NODOSDRIVE */

DIR *Xopendir(path)
char *path;
{
#ifndef	_NOROCKRIDGE
	char buf[MAXPATHLEN];
#endif
	DIR *dirp;
	char *cp, conv[MAXPATHLEN];

	cp = convput(conv, path, 1, 1, NULL, NULL);
	dirp = unixopendir(cp);
#ifndef	_NOROCKRIDGE
	if (dirp) {
		dirpathlist = (opendirpath_t *)realloc2(dirpathlist,
			++maxdirpath * sizeof(opendirpath_t));
		realpath2(path, buf, 1);
		cp = convput(conv, buf, 0, 1, NULL, NULL);
		dirpathlist[maxdirpath - 1].dirp = dirp;
		dirpathlist[maxdirpath - 1].path = strdup2(cp);
	}
#endif	/* !_NOROCKRIDGE */

	return(dirp);
}

int Xclosedir(dirp)
DIR *dirp;
{
#ifndef	_NOROCKRIDGE
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
#endif	/* !_NOROCKRIDGE */

	return(unixclosedir(dirp));
}

struct dirent *Xreaddir(dirp)
DIR *dirp;
{
#ifdef	_NOROCKRIDGE
	return(unixreaddir(dirp));
#else	/* !_NOROCKRIDGE */
	static struct dirent buf;
	struct dirent *dp;
	char *src, *dest, path[MAXPATHLEN], conv[MAXPATHLEN];
	int i;

	if (!(dp = unixreaddir(dirp))) return(dp);
	src = dp -> d_name;
	dest = buf.d_name;
	memcpy((char *)&buf, (char *)dp, dest - (char *)&buf);
	memcpy(&(buf.d_alias), dp -> d_alias, sizeof(dp -> d_alias));
	if (isdotdir(src)) {
		strcpy(dest, src);
		return(&buf);
	}
	for (i = maxdirpath - 1; i >= 0; i--)
		if (dirp == dirpathlist[i].dirp) break;
	if (i >= 0) {
		strcatdelim2(path, dirpathlist[i].path, src);
		if (convget(conv, path, 0) == conv) {
			if ((src = strrdelim(conv, 0))) src++;
			else src = conv;
		}
	}
	strcpy(dest, src);

	return(&buf);
#endif	/* !_NOROCKRIDGE */
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

int Xchdir(path)
char *path;
{
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN];
	int drive, dd;
#endif
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	_NODOSDRIVE
	n = rawchdir(path);
#else	/* !_NODOSDRIVE */
	if (!(drive = dospath3(path))) {
		if ((n = rawchdir(path)) >= 0) {
			if (lastdrv >= 0) shutdrv(lastdrv);
			lastdrv = -1;
		}
	}
	else if ((dd = preparedrv(drive)) < 0) n = -1;
	else if (setcurdrv(drive, 1) < 0
	|| (checkpath(path, buf) ? doschdir(buf) : unixchdir(path)) < 0) {
		shutdrv(dd);
		n = -1;
	}
	else {
		if (lastdrv >= 0) {
			if ((lastdrv % DOSNOFILE) != (dd % DOSNOFILE))
				shutdrv(lastdrv);
			else dd = lastdrv;
		}
		lastdrv = dd;
		n = 0;
	}
#endif	/* !_NODOSDRIVE */
	LOG1(_LOG_INFO_, n, "chdir(\"%k\");", path);

	return(n);
}

char *Xgetwd(path)
char *path;
{
	char *cp, conv[MAXPATHLEN];

	if (!(cp = unixgetcwd(path, MAXPATHLEN))) return(NULL);
	cp = convget(conv, cp, 0);
	if (cp == path) return(cp);
	strcpy(path, cp);

	return(path);
}

static int NEAR statcommon(path, stp)
char *path;
struct stat *stp;
{
	char *cp;
	u_short mode;

	if (unixstat(path, stp) < 0) return(-1);

	mode = (u_short)(stp -> st_mode);
	if ((mode & S_IFMT) != S_IFDIR
	&& (cp = strrchr(path, '.')) && strlen(++cp) == 3) {
		if (!stricmp(cp, "BAT")
		|| !stricmp(cp, "COM")
		|| !stricmp(cp, "EXE"))
			mode |= S_IEXEC;
	}
	mode &= (S_IREAD | S_IWRITE | S_IEXEC);
	mode |= (mode >> 3) | (mode >> 6);
	stp -> st_mode |= mode;

	return(0);
}

int Xstat(path, stp)
char *path;
struct stat *stp;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
	n = statcommon(path, stp);

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

#ifdef	_NOROCKRIDGE
	path = convput(conv, path, 1, 0, NULL, NULL);
#else
	path = convput(conv, path, 1, 0, rpath, NULL);
#endif
	n = statcommon(path, stp);
#ifndef	_NOROCKRIDGE
	if (n >= 0 && *rpath) rrlstat(rpath, stp);
#endif

	return(n);
}

int Xaccess(path, mode)
char *path;
int mode;
{
#ifndef	_NOUSELFN
	char buf[MAXPATHLEN];
#endif
	char *cp, conv[MAXPATHLEN];
	struct stat st;
	int n;

	cp = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	if (checkpath(cp, buf)) n = dosaccess(buf, mode);
	else
# endif
	if (!(cp = preparefile(cp, buf))) n = -1;
	else
#endif
	if ((n = (access(cp, mode)) ? -1 : 0) < 0) /*EMPTY*/;
	else if (!(mode & X_OK)) /*EMPTY*/;
	else if (Xstat(path, &st) < 0 || !(st.st_mode & S_IEXEC)) {
		errno = EACCES;
		n = -1;
	}

	return(n);
}

/*ARGSUSED*/
int Xsymlink(name1, name2)
char *name1, *name2;
{
	errno = EINVAL;
	LOG2(_LOG_WARNING_, -1, "symlink(\"%k\", \"%k\");", name1, name2);

	return(-1);
}

/*ARGSUSED*/
int Xreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
#ifndef	_NOROCKRIDGE
	char conv[MAXPATHLEN], lbuf[MAXPATHLEN];
#endif
	int n;

	n = -1;
#ifndef	_NOROCKRIDGE
	path = convput(conv, path, 1, 0, lbuf, NULL);
	if (*lbuf && (n = rrreadlink(lbuf, buf, bufsiz)) >= 0) /*EMPTY*/;
	else
#endif
	errno = EINVAL;

	return(n);
}

int Xchmod(path, mode)
char *path;
int mode;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
	n = unixchmod(path, mode);
	LOG2(_LOG_NOTICE_, n, "chmod(\"%k\", %d);", path, mode);

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
	n = unixutime(path, times);
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
	n = unixutimes(path, tvp);
	LOG1(_LOG_NOTICE_, n, "utimes(\"%k\");", path);

	return(n);
}
#endif	/* !USEUTIME */

#ifdef	HAVEFLAGS
/*ARGSUSED*/
int Xchflags(path, flags)
char *path;
u_long flags;
{
	errno = EACCESS;
	LOG2(_LOG_WARNING_, -1, "chflags(\"%k\", %05o);", path, flags);

	return(-1);
}
#endif	/* !HAVEFLAGS */

#ifndef	NOUID
/*ARGSUSED*/
int Xchown(path, uid, gid)
char *path;
uid_t uid;
gid_t gid;
{
	errno = EACCESS;
	LOG3(_LOG_WARNING_, -1, "chown(\"%k\", %d, %d);", path, uid, gid);

	return(-1);
}
#endif	/* !NOUID */

int Xunlink(path)
char *path;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
	if ((n = unixunlink(path)) < 0) {
		if (errno == EACCES
		&& unixchmod(path, (S_IREAD | S_IWRITE | S_ISVTX)) >= 0)
			n = unixunlink(path);
	}
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
	if (dospath(from, NULL) != dospath(to, NULL)) {
		errno = EXDEV;
		n = -1;
	}
	else n = unixrename(from, to);
	LOG2(_LOG_WARNING_, n, "rename(\"%k\", \"%k\");", from, to);

	return(n);
}

int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
#ifndef	_NOUSELFN
	char buf[MAXPATHLEN];
#endif
	char conv[MAXPATHLEN];
	int fd;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	if (checkpath(path, buf)) fd = dosopen(buf, flags, mode);
	else
# endif
	if (flags & O_CREAT) fd = unixopen(path, flags, mode);
	else if (!(path = preparefile(path, buf))) fd = -1;
	else
#endif	/* !_NOUSELFN */
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

int Xftruncate(fd, len)
int fd;
off_t len;
{
	int n;

	if (fd >= DOSFDOFFSET) n = dosftruncate(fd, len);
	else n = ftruncate(fd, len);

	return(n);
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
#if	defined (_NOUSELFN) && !defined (DJGPP)
	struct stat st;
#endif
	char conv[MAXPATHLEN];
	int n;

#if	defined (_NOUSELFN) && !defined (DJGPP)
	if (Xstat(path, &st) >= 0) {
		errno = EEXIST;
		n = -1;
	}
	else
#endif
	{
		path = convput(conv, path, 1, 1, NULL, NULL);
		n = unixmkdir(path, mode);
	}
	LOG2(_LOG_WARNING_, n, "mkdir(\"%k\", %05o);", path, mode);

	return(n);
}

int Xrmdir(path)
char *path;
{
	char conv[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
	n = unixrmdir(path);
	LOG1(_LOG_WARNING_, n, "rmdir(\"%k\");", path);

	return(n);
}

FILE *Xfopen(path, type)
char *path, *type;
{
#ifndef	_NOUSELFN
	char buf[MAXPATHLEN];
#endif
	FILE *fp;
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	if (checkpath(path, buf)) fp = dosfopen(buf, type);
	else
# endif
	if (*type != 'r' || *(type + 1) == '+') fp = unixfopen(path, type);
	else if (!(path = preparefile(path, buf))) fp = NULL;
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

#ifdef	_NOORIGSHELL
extern char *deftmpdir;
extern char *tmpfilename;
static int popenstat = 0;
#define	PIPEDIR		"PIPE_DIR"
#define	PIPEFILE	"FAKEPIPE"

FILE *Xpopen(command, type)
char *command, *type;
{
#ifndef	_NOUSELFN
	char *cp, buf[MAXPATHLEN];
#endif
	char *cmdline, path[MAXPATHLEN];
	int l1, l2;

	strcpy(path, PIPEDIR);
	if (mktmpdir(path) < 0) return(NULL);
#ifndef	_NOUSELFN
	if (!(cp = preparefile(path, buf))) return(NULL);
	else if (cp == buf) strcpy(path, buf);
#endif
	strcpy(strcatdelim(path), PIPEFILE);

	l1 = strlen(command);
	l2 = strlen(path);
	cmdline = malloc2(l1 + l2 + 3 + 1);
	strncpy(cmdline, command, l1);
	strcpy(&(cmdline[l1]), " > ");
	l1 += 3;
	strcpy(&(cmdline[l1]), path);
	popenstat = system(cmdline);

	return(fopen(path, type));
}

int Xpclose(fp)
FILE *fp;
{
	char *cp, path[MAXPATHLEN];
	int no;

	no = 0;
	if (fclose(fp)) no = errno;
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

	return((errno = no) ? -1 : popenstat);
}
#endif	/* _NOORIGSHELL */
