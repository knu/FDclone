/*
 *	unixdisk.c
 *
 *	UNIXlike Disk Access Module
 */

#include "machine.h"
#include "unixdisk.h"

#ifndef	NOLFNEMU
static int seterrno();
static char *shortname();
static int dos_findfirst();
static int dos_findnext();
static u_short lfn_findfirst();
static int lfn_findnext();
static int lfn_findclose();
#endif
static u_short getdosmode();
static u_char putdosmode();
static time_t getdostime();

static u_short doserrlist[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 65, 80
};
static int unixerrlist[] = {
	0, EINVAL, ENOENT, ENOENT, EMFILE, EACCES,
	EBADF, ENOMEM, ENOMEM, ENOMEM, EACCES, EEXIST
};
static int dos7access = 1;

#if	defined (DJGPP) && (DJGPP >= 2)
#include <crt0.h>
int _crt0_startup_flags = _CRT0_FLAG_PRESERVE_FILENAME_CASE;
#endif


#ifndef	NOLFNEMU
static int seterrno(doserr)
u_short doserr;
{
	int i;

	for (i = 0; i < sizeof(doserrlist) / sizeof(u_short); i++)
		if (doserr == doserrlist[i]) return(errno = unixerrlist[i]);
	return(errno = EINVAL);
}
#endif	/* !NOLFNEMU */

int getcurdrv()
{
	return(bdos(0x19, 0, 0) + ((dos7access) ? 'a' : 'A'));
}

int setcurdrv(drive)
int drive;
{
	union REGS regs;

	errno = EINVAL;
	if (drive >= 'a' && drive <= 'z') {
		drive -= 'a';
		dos7access = 1;
	}
	else {
		drive -= 'A';
		dos7access = 0;
	}
	if (drive < 0) return(-1);

	regs.x.ax = 0x0e00;
	regs.h.dl = drive;
	intdos(&regs, &regs);
	if (regs.h.al < drive) return(-1);

	return(0);
}

int supportLFN(path)
char *path;
{
#ifdef	NOLFNEMU
	return(0);
#else	/* !NOLFNEMU */
	union REGS regs;
	struct SREGS segs;
	char buf[128], drv[4];

	if (bdos(0x30, 0, 0) < 7) return(0);

	if (path && path[0] && path[1] == ':') drv[0] = path[0];
	else drv[0] = getcurdrv();
	if (drv[0] >= 'A' && drv[0] <= 'Z') return(0);
	strcpy(&drv[1], ":\\");

	regs.x.ax = 0x71a0;
	regs.x.cx = sizeof(buf);
	segs.ds = FP_SEG(drv);
	regs.x.dx = FP_OFF(drv);
	segs.es = FP_SEG(buf);
	regs.x.di = FP_OFF(buf);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) return(0);

	return(1);
#endif	/* !NOLFNEMU */
}

#ifndef	NOLFNEMU
static char *shortname(path, alias)
char *path, *alias;
{
	union REGS regs;
	struct SREGS segs;
	int drv;

	regs.x.ax = 0x7160;
	regs.x.cx = 1;
	segs.ds = FP_SEG(path);
	regs.x.si = FP_OFF(path);
	segs.es = FP_SEG(alias);
	regs.x.di = FP_OFF(alias);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(regs.x.ax);
		return(NULL);
	}
	if (path && path[0] && path[1] == ':') drv = path[0];
	else drv = getcurdrv();
	if (drv >= 'a' && drv <= 'z') *alias += 'a' - 'A';

	return(alias);
}
#endif	/* !NOLFNEMU */

char *unixrealpath(path, resolved)
char *path, *resolved;
{
#ifdef	NOLFNEMU
	int i;

	_fixpath(path, resolved);
	for (i = 0; resolved[i]; i++) {
		if (resolved[i] == '/') resolved[i] = _SC_;
#if	defined (DJGPP) && (DJGPP >= 2)
		else if (_USE_LFN);
#endif
		else if (resolved[i] >= 'a' && resolved[i] <= 'z')
			resolved[i] -= 'a' - 'A';
	}
#else	/* !NOLFNEMU */
	union REGS regs;
	struct SREGS segs;

	if (supportLFN(path)) return(shortname(path, resolved));
	regs.x.ax = 0x6000;
	regs.x.cx = 0;
	segs.ds = FP_SEG(path);
	regs.x.si = FP_OFF(path);
	segs.es = FP_SEG(resolved);
	regs.x.di = FP_OFF(resolved);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(regs.x.ax);
		return(NULL);
	}

#endif	/* !NOLFNEMU */
	return(resolved);
}

char *preparefile(path, alias, iscreat)
char *path, *alias;
int iscreat;
{
#ifdef	NOLFNEMU
	return(path);
#else	/* !NOLFNEMU */
	union REGS regs;
	struct SREGS segs;
	char *cp;

	if (!supportLFN(path)) return(path);
	if (cp = shortname(path, alias)) return(cp);
	if (!iscreat || errno != ENOENT) return(NULL);

	regs.x.ax = 0x716c;
	regs.x.bx = 0x0111;	/* O_WRONLY | SH_DENYRW | NO_BUFFER */
	regs.x.cx = DS_IARCHIVE;
	regs.x.dx = 0x0012;	/* O_CREAT | O_TRUNC */
	segs.ds = FP_SEG(path);
	regs.x.si = FP_OFF(path);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(regs.x.ax);
		return(NULL);
	}
	regs.x.bx = (u_short)(regs.x.ax);
	regs.x.ax = 0x3e00;
	intdosx(&regs, &regs, &segs);

	return(shortname(path, alias));
#endif	/* !NOLFNEMU */
}

#ifdef	__GNUC__
char *adjustfname(path)
char *path;
{
	int i;
#if	defined (DJGPP) && (DJGPP >= 2)
	struct ffblk dbuf;
	char *cp, tmp[MAXPATHLEN + 1];
	int j;

	if (_USE_LFN) {
		cp = NULL;
		strcpy(tmp, path);
		for (i = j = 0; tmp[i]; i++) {
			if (tmp[i] != '/' && tmp[i] != _SC_) {
				path[j++] = tmp[i];
				continue;
			}
			path[j] = '\0';
			if (cp && !findfirst(path, &dbuf, SEARCHATTRS)) {
				strcpy(cp, dbuf.ff_name);
				j = strlen(path);
			}
			path[j++] = _SC_;
			cp = &path[j];
		}
		path[j] = '\0';
		if (!cp) cp = path;
		if ((cp[0] != '.' || (cp[1] && (cp[1] != '.' || cp[2])))
		&& !findfirst(path, &dbuf, SEARCHATTRS))
			strcpy(cp, dbuf.ff_name);
	}
	else
#endif	/* DJGPP >= 2 */
	for (i = 0; path[i]; i++) {
		if (path[i] == '/') path[i] = _SC_;
		else if (path[i] >= 'a' && path[i] <= 'z')
			path[i] -= 'a' - 'A';
	}
	return(path);
}
#endif	/* __GNUC__ */

#ifndef	NOLFNEMU
static int dos_findfirst(path, result)
char *path;
struct dosfind_t *result;
{
	union REGS regs;
	struct SREGS segs;

	regs.x.ax = 0x1a00;
	segs.ds = FP_SEG(result);
	regs.x.dx = FP_OFF(result);
	intdosx(&regs, &regs, &segs);

	regs.x.ax = 0x4e00;
	regs.x.cx = SEARCHATTRS;
	segs.ds = FP_SEG(path);
	regs.x.dx = FP_OFF(path);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(((u_short)(regs.x.ax) != 0x12) ? regs.x.ax : 0);
		return(-1);
	}

	return(0);
}

static int dos_findnext(result)
struct dosfind_t *result;
{
	union REGS regs;
	struct SREGS segs;

	regs.x.ax = 0x1a00;
	segs.ds = FP_SEG(result);
	regs.x.dx = FP_OFF(result);
	intdosx(&regs, &regs, &segs);

	regs.x.ax = 0x4f00;
	intdos(&regs, &regs);
	if (regs.x.cflag) {
		seterrno(((u_short)(regs.x.ax) != 0x12) ? regs.x.ax : 0);
		return(-1);
	}

	return(0);
}

static u_short lfn_findfirst(path, result)
char *path;
struct lfnfind_t *result;
{
	union REGS regs;
	struct SREGS segs;

	regs.x.ax = 0x714e;
	regs.x.cx = SEARCHATTRS;
	segs.ds = FP_SEG(path);
	regs.x.dx = FP_OFF(path);
	segs.es = FP_SEG(result);
	regs.x.di = FP_OFF(result);
	regs.x.si = DATETIMEFORMAT;
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(((u_short)(regs.x.ax) != 0x12) ? regs.x.ax : 0);
		return((u_short)-1);
	}

	return((u_short)(regs.x.ax));
}

static int lfn_findnext(fd, result)
u_short fd;
struct lfnfind_t *result;
{
	union REGS regs;
	struct SREGS segs;

	regs.x.ax = 0x714f;
	regs.x.bx = fd;
	regs.x.cx = SEARCHATTRS;
	segs.es = FP_SEG(result);
	regs.x.di = FP_OFF(result);
	regs.x.si = DATETIMEFORMAT;
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(((u_short)(regs.x.ax) != 0x12) ? regs.x.ax : 0);
		return(-1);
	}

	return(0);
}

static int lfn_findclose(fd)
u_short fd;
{
	union REGS regs;

	regs.x.ax = 0x71a1;
	regs.x.bx = fd;
	intdos(&regs, &regs);
	if (regs.x.cflag) {
		seterrno(regs.x.ax);
		return(-1);
	}

	return(0);
}

DIR *unixopendir(dir)
char *dir;
{
	DIR *dirp;
	char path[MAXPATHLEN + 1];
	int i;

	i = strlen(dir);
	strcpy(path, dir);
	if (i && path[i - 1] != _SC_) strcat(path, _SS_);
	if (!(dirp = (DIR *)malloc(sizeof(DIR)))) return(NULL);
	dirp -> dd_off = 0;

	if (!supportLFN(path)) {
		strcat(path, "*.*");
		dirp -> dd_buf = (char *)malloc(sizeof(struct dosfind_t));
		if (!dirp -> dd_buf) {
			free(dirp);
			return(NULL);
		}
		dirp -> dd_fd = (u_short)-1;
		i = dos_findfirst(path, (struct dosfind_t *)dirp -> dd_buf);
	}
	else {
		strcat(path, "*");
		dirp -> dd_buf = (char *)malloc(sizeof(struct lfnfind_t));
		if (!dirp -> dd_buf) {
			free(dirp);
			return(NULL);
		}
		dirp -> dd_fd =
			lfn_findfirst(path, (struct lfnfind_t *)dirp -> dd_buf);
		i = (dirp -> dd_fd == (u_short)-1) ? -1 : 0;
	}

	if (i < 0
	|| !(dirp -> dd_path = (char *)malloc(strlen(dir) + 1))) {
		if (!errno) dirp -> dd_off = -1;
		else {
			free(dirp -> dd_buf);
			free(dirp);
			return(NULL);
		}
	}
	strcpy(dirp -> dd_path, dir);
	return(dirp);
}

int unixclosedir(dirp)
DIR *dirp;
{
	if (dirp -> dd_fd != (u_short)-1) lfn_findclose(dirp -> dd_fd);
	free(dirp -> dd_buf);
	free(dirp -> dd_path);
	free(dirp);
	return(0);
}

struct dirent *unixreaddir(dirp)
DIR *dirp;
{
	static struct dirent d;
	int i;

	if (dirp -> dd_off < 0) return(NULL);
	d.d_off = dirp -> dd_off;

	if (dirp -> dd_fd == (u_short)-1) {
		struct dosfind_t *bufp;

		bufp = (struct dosfind_t *)(dirp -> dd_buf);
		strcpy(d.d_name, bufp -> name);
		d.d_alias[0] = '\0';
		i = dos_findnext(bufp);
	}
	else {
		struct lfnfind_t *bufp;

		bufp = (struct lfnfind_t *)(dirp -> dd_buf);
		strcpy(d.d_name, bufp -> name);
		strcpy(d.d_alias, bufp -> alias);
		i = lfn_findnext(dirp -> dd_fd, bufp);
	}
	if (i < 0) dirp -> dd_off = -1;
	else dirp -> dd_off++;

	return(&d);
}

int unixrewinddir(dirp, loc)
DIR *dirp;
long loc;
{
	DIR *dupdirp;
	char *cp;
	long i;

	if (dirp -> dd_fd != (u_short)-1) lfn_findclose(dirp -> dd_fd);
	if (!(dupdirp = unixopendir(dirp -> dd_path))) return(-1);

	free(dirp -> dd_buf);
	free(dupdirp -> dd_path);
	cp = dirp -> dd_path;
	memcpy(dirp, dupdirp, sizeof(DIR));
	dirp -> dd_path = cp;
	free(dupdirp);

	for (i = 0; i < loc; i++) if (!unixreaddir(dirp)) break;
	return(0);
}

/*ARGSUSED*/
char *unixgetcwd(pathname, size)
char *pathname;
int size;
{
	union REGS regs;
	struct SREGS segs;

	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);

	pathname[0] = getcurdrv();
	strcpy(pathname + 1, ":\\");
	pathname += 3;
	regs.x.ax = (supportLFN(NULL)) ? 0x7147 : 0x4700;
	regs.h.dl = 0;			/* Current Drive */
	segs.ds = FP_SEG(pathname);
	regs.x.si = FP_OFF(pathname);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(regs.x.ax);
		return(NULL);
	}
	return(pathname);
}

int unixrename(from, to)
char *from, *to;
{
	union REGS regs;
	struct SREGS segs;

	regs.x.ax = (supportLFN(from) || supportLFN(to)) ? 0x7156 : 0x5600;
	segs.ds = FP_SEG(from);
	regs.x.dx = FP_OFF(from);
	segs.es = FP_SEG(to);
	regs.x.di = FP_OFF(to);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(regs.x.ax);
		return(-1);
	}
	return(0);
}

/*ARGSUSED*/
int unixmkdir(path, mode)
char *path;
int mode;
{
	union REGS regs;
	struct SREGS segs;

	regs.x.ax = (supportLFN(path)) ? 0x7139 : 0x3900;
	segs.ds = FP_SEG(path);
	regs.x.dx = FP_OFF(path);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(regs.x.ax);
		return(-1);
	}
	return(0);
}
#endif	/* !NOLFNEMU */

static u_short getdosmode(attr)
u_char attr;
{
	u_short mode;

	mode = 0;
	if (attr & DS_IARCHIVE) mode |= S_ISVTX;
	if (!(attr & DS_IHIDDEN)) mode |= S_IREAD;
	if (!(attr & DS_IRDONLY)) mode |= S_IWRITE;
	if (attr & DS_IFDIR) mode |= (S_IFDIR | S_IEXEC);
	else if (attr & DS_IFLABEL) mode |= S_IFIFO;
	else if (attr & DS_IFSYSTEM) mode |= S_IFSOCK;
	else mode |= S_IFREG;

	return(mode);
}

static u_char putdosmode(mode)
u_short mode;
{
	u_char attr;

	attr = 0;
	if (mode & S_ISVTX) attr |= DS_IARCHIVE;
	if (!(mode & S_IREAD)) attr |= DS_IHIDDEN;
	if (!(mode & S_IWRITE)) attr |= DS_IRDONLY;
	if ((mode & S_IFMT) == S_IFDIR) attr |= DS_IFDIR;
	else if ((mode & S_IFMT) == S_IFIFO) attr |= DS_IFLABEL;
	else if ((mode & S_IFMT) == S_IFSOCK) attr |= DS_IFSYSTEM;

	return(attr);
}

static time_t getdostime(date, time)
u_short date, time;
{
	struct tm tm;

	tm.tm_year = 1980 + ((date >> 9) & 0x7f);
	tm.tm_year -= 1900;
	tm.tm_mon = ((date >> 5) & 0x0f) - 1;
	tm.tm_mday = (date & 0x1f);
	tm.tm_hour = ((time >> 11) & 0x1f);
	tm.tm_min = ((time >> 5) & 0x3f);
	tm.tm_sec = ((time << 1) & 0x3e);
	tm.tm_isdst = -1;

	return(mktime(&tm));
}

int unixstat(path, status)
char *path;
struct stat *status;
{
#ifdef	NOLFNEMU
	struct ffblk dbuf;

	if (findfirst(path, &dbuf, SEARCHATTRS) != 0
#if	defined (DJGPP) && (DJGPP >= 2)
	&& (errno != ENOENT || strcmp(path, "..")
#else
	&& (errno != ENMFILE || strcmp(path, "..")
#endif
	|| findfirst(".", &dbuf, SEARCHATTRS) != 0)) {
		if (!errno) errno = ENOENT;
		return(-1);
	}
	status -> st_mode = getdosmode(dbuf.ff_attrib);
	status -> st_mtime = getdostime(dbuf.ff_fdate, dbuf.ff_ftime);
	status -> st_size = dbuf.ff_fsize;
#else	/* !NOLFNEMU */
	if (!supportLFN(path)) {
		struct dosfind_t dbuf;

		if (dos_findfirst(path, &dbuf) < 0
		&& (errno || strcmp(path, "..")
		|| dos_findfirst(".", &dbuf) < 0)) {
			if (!errno) errno = ENOENT;
			return(-1);
		}
		status -> st_mode = getdosmode(dbuf.attr);
		status -> st_mtime = getdostime(dbuf.wrdate, dbuf.wrtime);
		status -> st_size = dbuf.size_l;
	}
	else {
		struct lfnfind_t lbuf;
		int fd;

		if ((fd = lfn_findfirst(path, &lbuf)) < 0
		&& (errno != ENOENT || strcmp(path, "..")
		|| (fd = lfn_findfirst(".", &lbuf)) < 0)) {
			if (!errno) errno = ENOENT;
			return(-1);
		}
		status -> st_mode = getdosmode(lbuf.attr);
		status -> st_mtime = getdostime(lbuf.wrdate, lbuf.wrtime);
		status -> st_size = lbuf.size_l;

		lfn_findclose(fd);
	}
#endif	/* !NOLFNEMU */
	status -> st_ctime = status -> st_atime = status -> st_mtime;
	status -> st_dev =
	status -> st_ino = 0;
	status -> st_nlink = 1;
	status -> st_uid =
	status -> st_gid = -1;
	return(0);
}

int unixchmod(path, mode)
char *path;
int mode;
{
	union REGS regs;
#ifndef	NOLFNEMU
	struct SREGS segs;
#endif

#ifdef	NOLFNEMU
	regs.x.ax = 0x4301;
#else
	if (!supportLFN(path)) regs.x.ax = 0x4301;
	else {
		regs.x.ax = 0x7143;
		regs.h.bl = 0x01;
	}
#endif
	regs.x.cx = putdosmode(mode);
#ifdef	NOLFNEMU
	regs.x.dx = (unsigned)path;
	intdos(&regs, &regs);
	if (regs.x.cflag) {
		errno = (u_short)(regs.x.ax);
		return(-1);
	}
#else
	segs.ds = FP_SEG(path);
	regs.x.dx = FP_OFF(path);
	intdosx(&regs, &regs, &segs);
	if (regs.x.cflag) {
		seterrno(regs.x.ax);
		return(-1);
	}
#endif
	return(0);
}
