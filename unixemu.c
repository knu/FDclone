/*
 *	unixemu.c
 *
 *	UNIX Function Emulation on DOS
 */

#include <io.h>
#include "fd.h"
#include "func.h"
#include "unixdisk.h"

#ifdef	LSI_C
extern u_char _openfile[];
#endif

extern int norealpath;
#ifndef	_NOROCKRIDGE
extern int norockridge;
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

int noconv = 0;
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
	return((isalpha(*path) && path[1] == ':') ? *path : 0);
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

/*ARGSUSED*/
char *convget(buf, path, dos)
char *buf, *path;
int dos;
{
#ifndef	_NOROCKRIDGE
	if (noconv) return(path);

	if (!norockridge) return(transpath(path, buf));
#endif
	return(path);
}

/*ARGSUSED*/
char *convput(buf, path, needfile, rdlink, rrreal, codep)
char *buf, *path;
int needfile, rdlink;
char *rrreal;
int *codep;
{
#ifdef	_NOROCKRIDGE
	return(path);
#else	/* !_NOROCKRIDGE */
	char *cp, *file, rbuf[MAXPATHLEN], rpath[MAXPATHLEN];
	int n;

	if (rrreal) *rrreal = '\0';

	if (noconv || isdotdir(path)) return(path);

	if (needfile && strdelim(path, 0)) needfile = 0;
	if (norealpath) cp = path;
	else {
		if ((file = strrdelim(path, 0))) {
			n = file - path;
			if (file++ == isrootdir(path)) n++;
			strncpy2(rpath, path, n);
		}
		else {
			file = path;
			strcpy(rpath, ".");
		}
		realpath2(rpath, rpath, 1);
		cp = strcatdelim(rpath);
		strncpy2(cp, file, MAXPATHLEN - 1 - (cp - rpath));
		cp = rpath;
	}
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
	if (cp == path) return(path);
	if (needfile && (file = strrdelim(cp, 0))) file++;
	else file = cp;
	strcpy(buf, file);
	return(buf);
#endif	/* !_NOROCKRIDGE */
}

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
				(maxdirpath - i) * sizeof(opendirpath_t));
			if (--maxdirpath <= 0) {
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
#else
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

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	_NODOSDRIVE
	return(rawchdir(path));
#else
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
#endif	/* !_NODOSDRIVE */
}

char *Xgetwd(path)
char *path;
{
	char *cp, conv[MAXPATHLEN];

	if (!(cp = unixgetcwd(path, MAXPATHLEN, 0))) return(NULL);
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
		|| !stricmp(cp, "EXE")) mode |= S_IEXEC;
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

	path = convput(conv, path, 1, 1, NULL, NULL);
	return(statcommon(path, stp));
}

int Xlstat(path, stp)
char *path;
struct stat *stp;
{
#ifndef	_NOROCKRIDGE
	char rpath[MAXPATHLEN];
#endif
	char conv[MAXPATHLEN];

#ifdef	_NOROCKRIDGE
	path = convput(conv, path, 1, 0, NULL, NULL);
#else
	path = convput(conv, path, 1, 0, rpath, NULL);
#endif
	if (statcommon(path, stp) < 0) return(-1);
#ifndef	_NOROCKRIDGE
	if (*rpath) rrlstat(rpath, stp);
#endif
	return(0);
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

	cp = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	if (checkpath(cp, buf)) return(dosaccess(buf, mode));
# endif
	if (!(cp = preparefile(cp, buf))) return(-1);
#endif
	if (access(cp, mode) != 0) return(-1);

	if (!(mode & X_OK)) return(0);
	if (Xstat(path, &st) < 0 || !(st.st_mode & S_IEXEC)) {
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
	char conv[MAXPATHLEN], lbuf[MAXPATHLEN];
	int len;

	path = convput(conv, path, 1, 0, lbuf, NULL);
	if (*lbuf && (len = rrreadlink(lbuf, buf, bufsiz)) >= 0)
		return(len);
#endif
	errno = EINVAL;
	return(-1);
}

int Xchmod(path, mode)
char *path;
int mode;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
	return(unixchmod(path, mode));
}

#ifdef	USEUTIME
int Xutime(path, times)
char *path;
struct utimbuf *times;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
	return(unixutime(path, times));
}
#else	/* !USEUTIME */
int Xutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
	return(unixutimes(path, tvp));
}
#endif	/* !USEUTIME */

int Xunlink(path)
char *path;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
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
	char conv1[MAXPATHLEN], conv2[MAXPATHLEN];

	from = convput(conv1, from, 1, 0, NULL, NULL);
	to = convput(conv2, to, 1, 0, NULL, NULL);
	if (dospath(from, NULL) != dospath(to, NULL)) {
		errno = EXDEV;
		return(-1);
	}
	return(unixrename(from, to) ? -1 : 0);
}

int Xopen(path, flags, mode)
char *path;
int flags, mode;
{
#ifndef	_NOUSELFN
	char buf[MAXPATHLEN];
#endif
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	if (checkpath(path, buf)) return(dosopen(buf, flags, mode));
# endif
	if (flags & O_CREAT) return(unixopen(path, flags, mode));
	else if (!(path = preparefile(path, buf))) return(-1);
#endif
	return(open(path, flags, mode));
}

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

int Xmkdir(path, mode)
char *path;
int mode;
{
#if	defined (_NOUSELFN) && !defined (DJGPP)
	struct stat st;
#endif
	char conv[MAXPATHLEN];

#if	defined (_NOUSELFN) && !defined (DJGPP)
	if (Xstat(path, &st) >= 0) {
		errno = EEXIST;
		return(-1);
	}
#endif
	path = convput(conv, path, 1, 1, NULL, NULL);
	return(unixmkdir(path, mode) ? -1 : 0);
}

int Xrmdir(path)
char *path;
{
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
	return(unixrmdir(path) ? -1 : 0);
}

FILE *Xfopen(path, type)
char *path, *type;
{
#ifndef	_NOUSELFN
	char buf[MAXPATHLEN];
#endif
	char conv[MAXPATHLEN];

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	if (checkpath(path, buf)) return(dosfopen(buf, type));
# endif
	if (strchr(type, 'w')) return(unixfopen(path, type));
	else if (!(path = preparefile(path, buf))) return(NULL);
#endif
	return(fopen(path, type));
}

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
	return((errno = no) ? -1 : popenstat);
}
#endif	/* _NOORIGSHELL */
