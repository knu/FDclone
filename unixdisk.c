/*
 *	unixdisk.c
 *
 *	UNIXlike Disk Access Module
 */

#include "machine.h"
#include "unixdisk.h"

static int seterrno();
#ifdef	DJGPP
static int dos_putpath();
#endif
#ifndef	NOLFNEMU
static long int21call();
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
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 18, 65, 80
};
static int unixerrlist[] = {
	0, EINVAL, ENOENT, ENOENT, EMFILE, EACCES,
	EBADF, ENOMEM, ENOMEM, ENOMEM, ENODEV, 0, EACCES, EEXIST
};
static int dos7access = 1;


static int seterrno(doserr)
u_short doserr;
{
	int i;

	for (i = 0; i < sizeof(doserrlist) / sizeof(u_short); i++)
		if (doserr == doserrlist[i]) return(errno = unixerrlist[i]);
	return(errno = EINVAL);
}

#ifdef	DJGPP
static int dos_putpath(path, offset)
char *path;
int offset;
{
	dosmemput(path, strlen(path) + 1, __tb + offset);
	return(offset);
}
#endif

#ifndef	NOLFNEMU
/*ARGSUSED*/
static long int21call(regsp, segsp)
__dpmi_regs *regsp;
struct SREGS *segsp;
{
	int n;

# ifdef	DJGPP
	__dpmi_int(0x21, regsp);
	n = ((*regsp).x.flags & 1);
# else
	intdosx(regsp, regsp, segsp);
	n = (*regsp).x.cflag;
# endif
	if (!n) return((*regsp).x.ax);
	seterrno((*regsp).x.ax);
	return(-1);
}
#endif	/* !NOLFNEMU */

int getcurdrv()
{
	return((u_char)bdos(0x19, 0, 0) + ((dos7access) ? 'a' : 'A'));
}

int setcurdrv(drive)
int drive;
{
	union REGS regs;
	int i;

	errno = EINVAL;
	if (drive >= 'a' && drive <= 'z') {
		drive -= 'a';
		i = 1;
	}
	else {
		drive -= 'A';
		i = 0;
	}
	if (drive < 0) return(-1);

	dos7access = i;
	regs.x.ax = 0x0e00;
	regs.h.dl = drive;
	intdos(&regs, &regs);
	if (regs.h.al < drive) {
		seterrno(0x0f);		/* Invarid drive */
		return(-1);
	}

	return(0);
}

int supportLFN(path)
char *path;
{
#ifdef	NOLFNEMU
	return(0);
#else	/* !NOLFNEMU */
	struct SREGS segs;
	__dpmi_regs regs;
	char drv[4], buf[128];

	if ((u_char)bdos(0x30, 0, 0) < 7) return(-1);

	if (!path || !isalpha(drv[0] = path[0]) || path[1] != ':')
		drv[0] = getcurdrv();
	if (drv[0] >= 'A' && drv[0] <= 'Z') return(0);
	drv[1] = ':';
	drv[2] = '\\';
	drv[3] = '\0';

	regs.x.ax = 0x71a0;
	regs.x.cx = sizeof(buf);
# ifdef	DJGPP
	dos_putpath(drv, 0);
	regs.x.ds = __tb_segment;
	regs.x.dx = __tb_offset;
	regs.x.es = regs.x.ds;
	regs.x.di = __tb_offset + 4;
# else
	segs.ds = FP_SEG(drv);
	regs.x.dx = FP_OFF(drv);
	segs.es = FP_SEG(buf);
	regs.x.di = FP_OFF(buf);
# endif
	if (int21call(&regs, &segs) < 0) return(-1);
	return((regs.x.bx & 0x4000) ? 1 : -1);
#endif	/* !NOLFNEMU */
}

char *shortname(path, alias)
char *path, *alias;
{
#ifdef	NOLFNEMU
	return(path);
#else
	struct SREGS segs;
	__dpmi_regs regs;
	int i;

	regs.x.ax = 0x7160;
	regs.x.cx = 0x8001;
# ifdef	DJGPP
	i = strlen(path) + 1;
	dos_putpath(path, 0);
	regs.x.ds = __tb_segment;
	regs.x.si = __tb_offset;
	regs.x.es = regs.x.ds;
	regs.x.di = __tb_offset + i;
	if (int21call(&regs, &segs) < 0) return(NULL);
	dosmemget(__tb + i, MAXPATHLEN, alias);
# else
	segs.ds = FP_SEG(path);
	regs.x.si = FP_OFF(path);
	segs.es = FP_SEG(alias);
	regs.x.di = FP_OFF(alias);
	if (int21call(&regs, &segs) < 0) return(NULL);
# endif
	if (!path || !isalpha(i = path[0]) || path[1] != ':') i = getcurdrv();
	if (i >= 'a' && i <= 'z') *alias += 'a' - 'A';

	return(alias);
#endif	/* !NOLFNEMU */
}

char *unixrealpath(path, resolved)
char *path, *resolved;
{
	int i;

#ifdef	NOLFNEMU
	_fixpath(path, resolved);
	for (i = 2; resolved[i]; i++) {
		if (resolved[i] == '/') resolved[i] = _SC_;
		else if (resolved[i] >= 'a' && resolved[i] <= 'z')
			resolved[i] -= 'a' - 'A';
	}
#else	/* !NOLFNEMU */
	struct SREGS segs;
	__dpmi_regs regs;

	switch (supportLFN(path)) {
		case 1:
			regs.x.ax = 0x7160;
			regs.x.cx = 0x8002;
			break;
		case 0:
			regs.x.ax = 0x7160;
			regs.x.cx = 0x8001;
			break;
		default:
			regs.x.ax = 0x6000;
			regs.x.cx = 0;
	}
# ifdef	DJGPP
	i = strlen(path) + 1;
	dos_putpath(path, 0);
	regs.x.ds = __tb_segment;
	regs.x.si = __tb_offset;
	regs.x.es = regs.x.ds;
	regs.x.di = __tb_offset + i;
	if (int21call(&regs, &segs) < 0) return(NULL);
	dosmemget(__tb + i, MAXPATHLEN, resolved);
# else
	segs.ds = FP_SEG(path);
	regs.x.si = FP_OFF(path);
	segs.es = FP_SEG(resolved);
	regs.x.di = FP_OFF(resolved);
	if (int21call(&regs, &segs) < 0) return(NULL);
# endif
#endif	/* !NOLFNEMU */
	if (!path || !isalpha(i = path[0]) || path[1] != ':') i = getcurdrv();
	if (i >= 'a' && i <= 'z' && *resolved >= 'A' && *resolved <= 'Z')
		*resolved += 'a' - 'A';
	if (i >= 'A' && i <= 'Z' && *resolved >= 'a' && *resolved <= 'z')
		*resolved += 'A' - 'a';

	return(resolved);
}

char *preparefile(path, alias, iscreat)
char *path, *alias;
int iscreat;
{
#ifdef	NOLFNEMU
	return(path);
#else	/* !NOLFNEMU */
	struct SREGS segs;
	__dpmi_regs regs;
	char *cp;

	if (supportLFN(path) < 0) return(path);
	if (cp = shortname(path, alias)) return(cp);
	if (!iscreat || errno != ENOENT) return(NULL);

	regs.x.ax = 0x716c;
	regs.x.bx = 0x0111;	/* O_WRONLY | SH_DENYRW | NO_BUFFER */
	regs.x.cx = DS_IARCHIVE;
	regs.x.dx = 0x0012;	/* O_CREAT | O_TRUNC */
# ifdef	DJGPP
	dos_putpath(path, 0);
	regs.x.ds = __tb_segment;
	regs.x.si = __tb_offset;
# else
	segs.ds = FP_SEG(path);
	regs.x.si = FP_OFF(path);
# endif
	if (int21call(&regs, &segs) < 0) return(NULL);

	regs.x.bx = regs.x.ax;
	regs.x.ax = 0x3e00;
	int21call(&regs, &segs);

	return(shortname(path, alias));
#endif	/* !NOLFNEMU */
}

#ifdef	__GNUC__
char *adjustfname(path)
char *path;
{
	int i;

#if	defined (DJGPP) && (DJGPP >= 2)
	if (supportLFN(path) > 0) {
		char tmp[MAXPATHLEN + 1];

		unixrealpath(path, tmp);
		strcpy(path, tmp);
	}
	else
#endif
	for (i = (isalpha(path[0]) && path[1] == ':') ? 2 : 0; path[i]; i++) {
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
	struct SREGS segs;
	__dpmi_regs regs;
# ifdef	DJGPP
	int i;

	i = strlen(path) + 1;
	regs.x.ds = __tb_segment;
	regs.x.dx = __tb_offset + i;
# else
	segs.ds = FP_SEG(result);
	regs.x.dx = FP_OFF(result);
# endif
	regs.x.ax = 0x1a00;
	int21call(&regs, &segs);

	regs.x.ax = 0x4e00;
	regs.x.cx = SEARCHATTRS;
# ifdef	DJGPP
	dos_putpath(path, 0);
	regs.x.ds = __tb_segment;
	regs.x.dx = __tb_offset;
	if (int21call(&regs, &segs) < 0) return(-1);
	dosmemget(__tb + i, sizeof(struct dosfind_t), result);
# else
	segs.ds = FP_SEG(path);
	regs.x.dx = FP_OFF(path);
	if (int21call(&regs, &segs) < 0) return(-1);
# endif
	return(0);
}

static int dos_findnext(result)
struct dosfind_t *result;
{
	struct SREGS segs;
	__dpmi_regs regs;

	regs.x.ax = 0x1a00;
# ifdef	DJGPP
	regs.x.ds = __tb_segment;
	regs.x.dx = __tb_offset;
	int21call(&regs, &segs);
	dosmemput(result, sizeof(struct dosfind_t), __tb);
# else
	segs.ds = FP_SEG(result);
	regs.x.dx = FP_OFF(result);
	int21call(&regs, &segs);
# endif

	regs.x.ax = 0x4f00;
	if (int21call(&regs, &segs) < 0) return(-1);
# ifdef	DJGPP
	dosmemget(__tb, sizeof(struct dosfind_t), result);
# endif
	return(0);
}

static u_short lfn_findfirst(path, result)
char *path;
struct lfnfind_t *result;
{
# ifdef	DJGPP
	int i;
# endif
	struct SREGS segs;
	__dpmi_regs regs;
	long fd;

	regs.x.ax = 0x714e;
	regs.x.cx = SEARCHATTRS;
	regs.x.si = DATETIMEFORMAT;
# ifdef	DJGPP
	i = strlen(path) + 1;
	dos_putpath(path, 0);
	regs.x.ds = __tb_segment;
	regs.x.dx = __tb_offset;
	regs.x.es = regs.x.ds;
	regs.x.di = __tb_offset + i;
	if ((fd = int21call(&regs, &segs)) < 0) return((u_short)-1);
	dosmemget(__tb + i, sizeof(struct lfnfind_t), result);
# else
	segs.ds = FP_SEG(path);
	regs.x.dx = FP_OFF(path);
	segs.es = FP_SEG(result);
	regs.x.di = FP_OFF(result);
	if ((fd = int21call(&regs, &segs)) < 0) return((u_short)-1);
# endif
	return((u_short)fd);
}

static int lfn_findnext(fd, result)
u_short fd;
struct lfnfind_t *result;
{
	struct SREGS segs;
	__dpmi_regs regs;

	regs.x.ax = 0x714f;
	regs.x.bx = fd;
	regs.x.cx = SEARCHATTRS;
	regs.x.si = DATETIMEFORMAT;
# ifdef	DJGPP
	regs.x.es = __tb_segment;
	regs.x.di = __tb_offset;
	if (int21call(&regs, &segs) < 0) return(-1);
	dosmemget(__tb, sizeof(struct lfnfind_t), result);
# else
	segs.es = FP_SEG(result);
	regs.x.di = FP_OFF(result);
	if (int21call(&regs, &segs) < 0) return(-1);
# endif
	return(0);
}

static int lfn_findclose(fd)
u_short fd;
{
	struct SREGS segs;
	__dpmi_regs regs;

	regs.x.ax = 0x71a1;
	regs.x.bx = fd;
	return((int21call(&regs, &segs) < 0) ? -1 : 0);
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

	if (supportLFN(path) <= 0) {
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

int unixseekdir(dirp, loc)
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

int unixrename(from, to)
char *from, *to;
{
	struct SREGS segs;
	__dpmi_regs regs;
# ifdef	DJGPP
	int i;
#endif

	regs.x.ax =
		(supportLFN(from) > 0 || supportLFN(to) > 0) ? 0x7156 : 0x5600;
# ifdef	DJGPP
	dos_putpath(from, 0);
	i = strlen(from) + 1;
	dos_putpath(to, i);
	regs.x.ds = __tb_segment;
	regs.x.dx = __tb_offset;
	regs.x.es = regs.x.ds;
	regs.x.di = __tb_offset + i;
# else
	segs.ds = FP_SEG(from);
	regs.x.dx = FP_OFF(from);
	segs.es = FP_SEG(to);
	regs.x.di = FP_OFF(to);
# endif
	return((int21call(&regs, &segs) < 0) ? -1 : 0);
}

/*ARGSUSED*/
int unixmkdir(path, mode)
char *path;
int mode;
{
	struct SREGS segs;
	__dpmi_regs regs;

	regs.x.ax = (supportLFN(path) > 0) ? 0x7139 : 0x3900;
# ifdef	DJGPP
	dos_putpath(path, 0);
	regs.x.ds = __tb_segment;
	regs.x.dx = __tb_offset;
# else
	segs.ds = FP_SEG(path);
	regs.x.dx = FP_OFF(path);
# endif
	return((int21call(&regs, &segs) < 0) ? -1 : 0);
}
#endif	/* !NOLFNEMU */

/*ARGSUSED*/
char *unixgetcwd(pathname, size)
char *pathname;
int size;
{
#ifdef	NOLFNEMU
	if (getcwd(pathname, size)) adjustfname(pathname + 2);
	else pathname = NULL;
#else	/* !NOLFNEMU */
# ifdef	DJGPP
	char tmp[MAXPATHLEN + 1];
# endif
	struct SREGS segs;
	__dpmi_regs regs;

	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);

	pathname[0] = getcurdrv();
	pathname[1] = ':';
	pathname[2] = '\\';

	regs.x.ax = (supportLFN(NULL) > 0) ? 0x7147 : 0x4700;
	regs.h.dl = 0;			/* Current Drive */
# ifdef	DJGPP
	regs.x.ds = __tb_segment;
	regs.x.si = __tb_offset;
	if (int21call(&regs, &segs) < 0) return(NULL);
	dosmemget(__tb, MAXPATHLEN, pathname + 3);
# else
	segs.ds = FP_SEG(pathname);
	regs.x.si = FP_OFF(pathname + 3);
	if (int21call(&regs, &segs) < 0) return(NULL);
# endif
#endif	/* !NOLFNEMU */
	if (dos7access && *pathname >= 'A' && *pathname <= 'Z')
		*pathname += 'a' - 'A';
	if (!dos7access && *pathname >= 'a' && *pathname <= 'z')
		*pathname += 'A' - 'a';
#if	!defined (NOLFNEMU) && defined (DJGPP)
	strcpy(tmp, pathname);
	unixrealpath(tmp, pathname);
#endif
	return(pathname);
}

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
	&& (errno != ENMFILE || strcmp(path, "..")
	|| findfirst(".", &dbuf, SEARCHATTRS) != 0)) {
		if (!errno) errno = ENOENT;
		return(-1);
	}
	status -> st_mode = getdosmode(dbuf.ff_attrib);
	status -> st_mtime = getdostime(dbuf.ff_fdate, dbuf.ff_ftime);
	status -> st_size = dbuf.ff_fsize;
#else	/* !NOLFNEMU */
	if (supportLFN(path) < 0) {
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

		if ((fd = lfn_findfirst(path, &lbuf)) == (u_short)-1
		&& (errno != ENOENT || strcmp(path, "..")
		|| (fd = lfn_findfirst(".", &lbuf)) == (u_short)-1)) {
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
#ifndef	NOLFNEMU
	struct SREGS segs;
#endif
	__dpmi_regs regs;

#ifdef	NOLFNEMU
	regs.x.ax = 0x4301;
#else
	if (supportLFN(path) < 0) regs.x.ax = 0x4301;
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
		seterrno((u_short)(regs.x.ax));
		return(-1);
	}
	return(0);
#else
# ifdef	DJGPP
	dos_putpath(path, 0);
	regs.x.ds = __tb_segment;
	regs.x.dx = __tb_offset;
# else
	segs.ds = FP_SEG(path);
	regs.x.dx = FP_OFF(path);
# endif
	return((int21call(&regs, &segs) < 0) ? -1 : 0);
#endif
}
