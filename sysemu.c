/*
 *	sysemu.c
 *
 *	system call emulations
 */

#ifdef	FD
#include "fd.h"
#include "term.h"
#include "dosdisk.h"
#include "kconv.h"
#else
#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#endif

#include "sysemu.h"
#include "log.h"
#include "pathname.h"
#include "termio.h"
#include "realpath.h"

#if	MSDOS && defined (FD)
#include "unixdisk.h"
#endif
#ifdef	DEP_SOCKET
#include "socket.h"
#endif
#ifdef	DEP_URLPATH
#include "urldisk.h"
#endif
#ifdef	CYGWIN
#include <sys/cygwin.h>
#endif

#if	MSDOS && !defined (FD)
#define	DS_IRDONLY		001
#define	DS_IHIDDEN		002
#define	DS_IFSYSTEM		004
#define	DS_IFLABEL		010
#define	DS_IFDIR		020
#define	DS_IARCHIVE		040
#define	SEARCHATTRS		(DS_IRDONLY | DS_IHIDDEN | DS_IFSYSTEM \
				| DS_IFDIR | DS_IARCHIVE)
# if	defined (DJGPP) && DJGPP < 2
# define	find_t		ffblk
# define	_dos_findfirst(p, a, f)	\
				findfirst(p, f, a)
# define	_dos_findnext	findnext
# define	_ENOENT_	ENMFILE
# else	/* !DJGPP || DJGPP >= 2 */
# define	ff_attrib	attrib
# define	ff_ftime	wr_time
# define	ff_fdate	wr_date
# define	ff_fsize	size
# define	ff_name		name
# define	_ENOENT_	ENOENT
# endif	/* !DJGPP || DJGPP >= 2 */
#endif	/* MSDOS && !FD */

#if	!MSDOS
#define	unixopendir		opendir
#define	unixclosedir(d)		(closedir(d), 0)
#define	unixreaddir		readdir
#define	unixrewinddir		rewinddir
# ifdef	USEGETWD
# define	unixgetcwd(p,s)	(char *)getwd(p)
# else
# define	unixgetcwd	(char *)getcwd
# endif
#define	unixstat2(p, s)		((stat(p, s)) ? -1 : 0)
#define	unixlstat2(p, s)	((lstat(p, s)) ? -1 : 0)
#define	unixchmod(p, m)		((chmod(p, m)) ? -1 : 0)
#define	unixutime(p, t)		((utime(p, t)) ? -1 : 0)
#define	unixutimes(p, t)	((utimes(p, t)) ? -1 : 0)
#define	unixunlink(p)		((unlink(p)) ? -1 : 0)
#define	unixrename(f, t)	((rename(f,t)) ? -1 : 0)
#define	unixrmdir(p)		((rmdir(p)) ? -1 : 0)
#endif	/* !MSDOS */
#if	MSDOS && !defined (DJGPP) && !defined (FD)
#define	unixmkdir(p, m)		((mkdir(p)) ? -1 : 0)
#endif
#if	!MSDOS || (defined (DJGPP) && !defined (FD))
#define	unixmkdir(p, m)		((mkdir(p,m)) ? -1 : 0)
#endif

#ifndef	FD
#define	convput(b,p,n,r,rp,cp)	(p)
#define	convget(b,p,d)		(p)
#endif
#ifdef	CYGWIN
#define	opendir_saw_u_cygdrive	(1 << (8 * sizeof(dirp -> __flags) - 2))
#define	opendir_saw_s_cygdrive	(1 << (8 * sizeof(dirp -> __flags) - 3))
#endif

typedef struct _openstat_t {
	u_char type;
	u_char dev;
#ifdef	FORCEDSTDC
	union {
		DIR *dirp;
		int fd;
	} body;
#else
	DIR *dirp;
	int fd;
#endif
#if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE)
	char *path;
#endif
} openstat_t;

#define	OP_DIRP			1
#define	OP_FD			2
#ifdef	FORCEDSTDC
#define	body_dirp(n)		(openlist[n].body.dirp)
#define	body_fd(n)		(openlist[n].body.fd)
#else
#define	body_dirp(n)		(openlist[n].dirp)
#define	body_fd(n)		(openlist[n].fd)
#endif

#ifdef	DEP_DOSDRIVE
#define	DEVMAX_DOS		DOSNOFILE
#else
#define	DEVMAX_DOS		0
#endif
#ifdef	DEP_URLPATH
#define	DEVMAX_URL		URLNOFILE
#else
#define	DEVMAX_URL		0
#endif
#define	DEVOFS_NORMAL		0
#define	DEVOFS_DOS		(DEVOFS_NORMAL + 1)
#define	DEVOFS_URL		(DEVOFS_DOS + DEVMAX_DOS)

#ifdef	DEP_ROCKRIDGE
extern int rrlstat __P_((CONST char *, struct stat *));
extern int rrreadlink __P_((CONST char *, char *, int));
#endif

#ifdef	FD
extern int win_x;
extern int win_y;
extern char fullpath[];
#endif

#if	MSDOS && defined (DEP_DOSDRIVE)
static int NEAR checkpath __P_((CONST char *, char *));
#endif
#ifdef	CYGWIN
static struct dirent *NEAR pseudoreaddir __P_((DIR *));
#else
#define	pseudoreaddir		unixreaddir
#endif
#if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) \
||	defined (DEP_PSEUDOPATH)
static int NEAR getopenlist __P_((int, VOID_P));
static VOID NEAR putopenlist __P_((int, int, VOID_P, CONST char *));
static int NEAR chkopenlist __P_((int, VOID_P));
static int NEAR delopenlist __P_((int, VOID_P));
#endif
#if	MSDOS && !defined (FD)
static DIR *NEAR unixopendir __P_((CONST char *));
static int NEAR unixclosedir __P_((DIR *));
static struct dirent *NEAR unixreaddir __P_((DIR *));
static int NEAR unixrewinddir __P_((DIR *));
static int NEAR unixchdir __P_((CONST char *));
static char *NEAR unixgetcwd __P_((char *, ALLOC_T));
static u_int NEAR getdosmode __P_((u_int));
static time_t NEAR getdostime __P_((u_int, u_int));
static int NEAR unixstat __P_((CONST char *, struct stat *));
static VOID NEAR putdostime __P_((u_short *, u_short *, time_t));
# ifdef	USEUTIME
static int NEAR unixutime __P_((CONST char *, CONST struct utimbuf *));
# else
static int NEAR unixutimes __P_((CONST char *, CONST struct timeval *));
# endif
#endif	/* MSDOS && !FD */
#ifdef	DEP_DOSDRIVE
static int NEAR checkchdir __P_((int, CONST char *));
#endif
#if	MSDOS
static int NEAR unixstat2 __P_((CONST char *, struct stat *));
#define	unixlstat2		unixstat2
#endif

#ifdef	DEP_PSEUDOPATH
int lastdrv = -1;
#endif
#ifdef	DEP_DOSDRIVE
int dosdrive = 0;
#endif
#ifdef	DEP_URLPATH
# ifdef	WITHNETWORK
int urldrive = 1;
# else
int urldrive = 0;
# endif
int urlkcode = NOCNV;
#endif	/* DEP_URLPATH */
#ifndef	NOSELECT
int (*readintrfunc)__P_((VOID_A)) = NULL;
#endif

#if	defined (DEP_DOSEMU) || defined (DEP_URLPATH)
static char cachecwd[MAXPATHLEN] = "";
#endif
#if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) \
||	defined (DEP_PSEUDOPATH)
static openstat_t *openlist = NULL;
static int maxopenlist = 0;
#endif


int seterrno(n)
int n;
{
	errno = n;

	return(-1);
}

#if	MSDOS && defined (DJGPP)
int dos_putpath(path, offset)
CONST char *path;
int offset;
{
	int i;

	i = strlen(path) + 1;
	dosmemput(path, i, __tb + offset);

	return(i);
}
#endif	/* MSDOS && DJGPP */

#if	MSDOS && !defined (FD)
int getcurdrv(VOID)
{
	return((bdos(0x19, 0, 0) & 0xff) + 'a');
}

/*ARGSUSED*/
int setcurdrv(drive, nodir)
int drive, nodir;
{
	int drv, olddrv;

	drv = Xtoupper(drive) - 'A';
	olddrv = (bdos(0x19, 0, 0) & 0xff);
	if ((bdos(0x0e, drv, 0) & 0xff) < drv
	&& (bdos(0x19, 0, 0) & 0xff) != drv) {
		bdos(0x0e, olddrv, 0);
		return(seterrno(EINVAL));
	}

	return(0);
}

char *unixrealpath(path, resolved)
CONST char *path;
char *resolved;
{
	struct SREGS sreg;
	__dpmi_regs reg;
# ifdef	DJGPP
	int i;
# endif

	reg.x.ax = 0x6000;
	reg.x.cx = 0;
# ifdef	DJGPP
	i = dos_putpath(path, 0);
# endif
	sreg.ds = PTR_SEG(path);
	reg.x.si = PTR_OFF(path, 0);
	sreg.es = PTR_SEG(resolved);
	reg.x.di = PTR_OFF(resolved, i);
	if (intcall(0x21, &reg, &sreg) < 0) return(NULL);
# ifdef	DJGPP
	dosmemget(__tb + i, MAXPATHLEN, resolved);
# endif

	return(resolved);
}
#endif	/* !MSDOS && !FD */

#ifdef	DEP_DOSPATH
int _dospath(path)
CONST char *path;
{
# if	!MSDOS
	if (!dosdrive) return(0);
# endif

	return((Xisalpha(*path) && path[1] == ':') ? *path : 0);
}

int dospath(path, buf)
CONST char *path;
char *buf;
{
# if	!MSDOS
	char *s;
	int len;
# endif
	CONST char *cp;
	char tmp[MAXPATHLEN];
	int drive;

	cp = path;
	if ((drive = _dospath(path))) /*EMPTY*/;
# if	MSDOS
#  ifdef	DOUBLESLASH
	else if (isdslash(path)) drive = '_';
#  endif
	else drive = getcurdrv();
# else
	else if (*path != _SC_ && (drive = _dospath(cachecwd))) cp = cachecwd;
	else return(0);
# endif

	if (!buf) return(drive);
	if (cp == buf) {
		VOID_C Xsnprintf(tmp, sizeof(tmp), cp);
		cp = tmp;
	}

# if	MSDOS
#  ifdef	DEP_DOSLFN
	if (shortname(path, buf) == buf) /*EMPTY*/;
	else
#  endif
	unixrealpath(path, buf);
# else	/* !MSDOS */
# ifdef	CODEEUC
	if (!noconv || cp != path)
		buf[ujis2sjis(buf, (u_char *)cp, sizeof(tmp) - 1)] = '\0';
	else
# endif
	VOID_C Xsnprintf(buf, MAXPATHLEN, cp);
	if (cp == cachecwd && *path) {
		s = strcatdelim(buf);
		len = MAXPATHLEN - (s - buf);
# ifdef	CODEEUC
		if (!noconv) s[ujis2sjis(s, (u_char *)path, len - 1)] = '\0';
		else
# endif
		VOID_C Xsnprintf(s, MAXPATHLEN - (s - buf), path);
	}
# endif	/* !MSDOS */

	return(drive);
}
#endif	/* DEP_DOSPATH */

#if	MSDOS && defined (DEP_DOSDRIVE)
int dospath2(path)
CONST char *path;
{
	int drv, drive;

	if (!(drive = _dospath(path))) drive = getcurdrv();
	drv = Xtoupper(drive) - 'A';
	if (drv < 0 || drv > 'Z' - 'A' || checkdrive(drv) <= 0) return(0);

	return(drive);
}

int dospath3(path)
CONST char *path;
{
	int i, drive;

	if ((i = supportLFN(path)) >= 0 || i <= -3) return(0);

	return((drive = _dospath(path)) ? drive : getcurdrv());
}

static int NEAR checkpath(path, buf)
CONST char *path;
char *buf;
{
	CONST char *cp;
	char tmp[MAXPATHLEN];
	int i, drive;

# ifdef	DOUBLESLASH
	if (isdslash(path)) return(0);
# endif
	if ((drive = _dospath(path))) cp = path + 2;
	else {
		cp = path;
		drive = getcurdrv();
	}
	i = Xtoupper(drive) - 'A';
	if (i < 0 || i > 'Z' - 'A' || checkdrive(i) <= 0) return(0);
	if (!buf) return(drive);

	if (path == buf) {
		Xstrcpy(tmp, cp);
		cp = tmp;
	}
	if (*cp == _SC_) buf = gendospath(buf, drive, '\0');
	else {
		if (!dosgetcwd(buf, MAXPATHLEN)) return(0);
		buf = strcatdelim(buf);
	}
	Xstrcpy(buf, cp);

	return(drive);
}
#endif	/* MSDOS && DEP_DOSDRIVE */

#ifdef	DEP_URLPATH
int _urlpath(path, hostp, typep)
CONST char *path;
char **hostp;
int *typep;
{
	if (!urldrive) return(0);

	return(urlparse(path, NULL, hostp, typep));
}

int urlpath(path, hostp, buf, typep)
CONST char *path;
char **hostp, *buf;
int *typep;
{
	CONST char *cp;
	char *s, tmp[MAXPATHLEN];
	int n;

	if ((n = _urlpath(path, hostp, typep))) cp = path;
	else if (*path != _SC_ && (n = _urlpath(cachecwd, hostp, typep)))
		cp = cachecwd;
	else return(0);

	if (!buf) return(n);
	if (cp == buf) {
		VOID_C Xsnprintf(tmp, sizeof(tmp), cp);
		cp = tmp;
	}
	if (cp[n]) VOID_C Xsnprintf(buf, MAXPATHLEN, &(cp[n]));
	else copyrootpath(buf);

	if (cp == cachecwd && *path) {
		s = strcatdelim(buf);
		VOID_C Xsnprintf(s, MAXPATHLEN - (s - buf), path);
	}
	Xrealpath(buf, tmp, RLP_PSEUDOPATH);
	VOID_C Xsnprintf(buf, MAXPATHLEN, "%.*s%s", n, cp, tmp);

	return(n);
}
#endif	/* DEP_URLPATH */

#if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) \
||	defined (DEP_PSEUDOPATH)
static int NEAR getopenlist(type, bodyp)
int type;
VOID_P bodyp;
{
	int n;

	for (n = maxopenlist - 1; n >= 0; n--) {
		if (type != openlist[n].type) continue;
		switch (type) {
			case OP_DIRP:
				if (body_dirp(n) == (DIR *)bodyp) return(n);
				break;
			case OP_FD:
				if (body_fd(n) == *((int *)bodyp)) return(n);
				break;
			default:
				break;
		}
	}

	return(-1);
}

/*ARGSUSED*/
static VOID NEAR putopenlist(type, dev, bodyp, path)
int type, dev;
VOID_P bodyp;
CONST char *path;
{
	int n;

	if (!bodyp) return;
	if (type == OP_FD && *((int *)bodyp) < 0) return;

	if ((n = getopenlist(type, bodyp)) < 0) {
		n = maxopenlist++;
		openlist = (openstat_t *)Xrealloc(openlist,
			maxopenlist * sizeof(openstat_t));
	}
# if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE)
	else Xfree(openlist[n].path);
# endif

	openlist[n].type = type;
	openlist[n].dev = dev;
# if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE)
	openlist[n].path = Xstrdup(path);
# endif

	switch (type) {
		case OP_DIRP:
			body_dirp(n) = (DIR *)bodyp;
			break;
		case OP_FD:
			body_fd(n) = *((int *)bodyp);
			break;
		default:
			break;
	}
}

static int NEAR chkopenlist(type, bodyp)
int type;
VOID_P bodyp;
{
	int n;

	if ((n = getopenlist(type, bodyp)) < 0) return(DEV_NORMAL);

	return(openlist[n].dev);
}

static int NEAR delopenlist(type, bodyp)
int type;
VOID_P bodyp;
{
	int n, dev;

	if ((n = getopenlist(type, bodyp)) < 0) return(-1);
	dev = openlist[n].dev;
# if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE)
	Xfree(openlist[n].path);
# endif
	memmove((char *)(&(openlist[n])), (char *)(&(openlist[n + 1])),
		(--maxopenlist - n) * sizeof(openstat_t));
	if (maxopenlist <= 0) {
		maxopenlist = 0;
		Xfree(openlist);
		openlist = NULL;
	}

	return(dev);
}

# ifdef	DEBUG
VOID freeopenlist(VOID_A)
{
#  if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE)
	while (maxopenlist > 0) Xfree(openlist[--maxopenlist].path);
#  endif
	maxopenlist = 0;
	Xfree(openlist);
	openlist = NULL;
}
# endif	/* DEBUG */
#endif	/* DEP_KANJIPATH || DEP_ROCKRIDGE || DEP_PSEUDOPATH */

#ifdef	DEP_PSEUDOPATH
int checkdrv(drv, drvp)
int drv, *drvp;
{
	int dev;

	if (drv < 0) dev = -1;
	else if (drv < DEVOFS_NORMAL + 1) {
		dev = DEV_NORMAL;
		drv -= DEVOFS_NORMAL;
	}
# ifdef	DEP_DOSDRIVE
	else if (drv >= DEVOFS_DOS && drv < DEVOFS_DOS + DEVMAX_DOS) {
		dev = DEV_DOS;
		drv -= DEVOFS_DOS;
	}
# endif
# ifdef	DEP_URLPATH
	else if (drv >= DEVOFS_URL && drv < DEVOFS_URL + DEVMAX_URL) {
		dev = DEV_URL;
		drv -= DEVOFS_URL;
	}
# endif
	else dev = drv = -1;

	if (drvp) *drvp = drv;

	return(dev);
}

/*ARGSUSED*/
int preparedrv(path, drivep, buf)
CONST char *path;
int *drivep;
char *buf;
{
# ifdef	DEP_URLPATH
	char *host;
	int type;
# endif
	int n, dev, drv;

	dev = DEV_NORMAL;
# ifdef	DEP_DOSDRIVE
	if ((n = dospath3(path))) {
		dev = DEV_DOS;
		drv = dosopendev(n);
		if (drv < 0) return(-1);
		if (drivep) *drivep = n;
		drv += DEVOFS_DOS;
	}
	else
# endif
# ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, buf, &type))) {
		dev = DEV_URL;
		drv = urlopendev(host, type);
		Xfree(host);
		if (drv < 0) return(-1);
		if (drivep) *drivep = n;
		drv += DEVOFS_URL;
	}
	else
# endif
	drv = DEVOFS_NORMAL;

	return(drv);
}

VOID shutdrv(drv)
int drv;
{
	int duperrno;

	duperrno = errno;
	switch (checkdrv(drv, &drv)) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			dosclosedev(drv);
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
			urlclosedev(drv);
			break;
# endif
		default:
			break;
	}
	errno = duperrno;
}
#endif	/* DEP_PSEUDOPATH */

#if	MSDOS && !defined (FD)
static DIR *NEAR unixopendir(path)
CONST char *path;
{
	DIR *dirp;
	char *cp;
	int n;

	dirp = (DIR *)Xmalloc(sizeof(DIR));
	dirp -> dd_off = 0;
	dirp -> dd_buf = Xmalloc(sizeof(struct find_t));
	dirp -> dd_path = Xmalloc(strlen(path) + 1 + 3 + 1);
	cp = strcatdelim2(dirp -> dd_path, path, NULL);

	dirp -> dd_id = DID_IFNORMAL;
	Xstrcpy(cp, "*.*");
	if (&(cp[-1]) > &(path[3])) n = -1;
	else n = _dos_findfirst(dirp -> dd_path, DS_IFLABEL,
		(struct find_t *)(dirp -> dd_buf));
	if (n == 0) dirp -> dd_id = DID_IFLABEL;
	else n = _dos_findfirst(dirp -> dd_path, (SEARCHATTRS | DS_IFLABEL),
		(struct find_t *)(dirp -> dd_buf));

	if (n < 0) {
		if (!errno || errno == _ENOENT_) dirp -> dd_off = -1;
		else {
			VOID_C unixclosedir(dirp);
			return(NULL);
		}
	}

	return(dirp);
}

static int NEAR unixclosedir(dirp)
DIR *dirp;
{
	Xfree(dirp -> dd_buf);
	Xfree(dirp -> dd_path);
	Xfree(dirp);

	return(0);
}

static struct dirent *NEAR unixreaddir(dirp)
DIR *dirp;
{
	static struct dirent d;
	struct find_t *findp;
	int n;

	if (dirp -> dd_off < 0) return(NULL);
	d.d_off = dirp -> dd_off;

	findp = (struct find_t *)(dirp -> dd_buf);
	Xstrcpy(d.d_name, findp -> ff_name);

	if (!(dirp -> dd_id & DID_IFLABEL)) n = _dos_findnext(findp);
	else n = _dos_findfirst(dirp -> dd_path, SEARCHATTRS, findp);
	if (n == 0) dirp -> dd_off++;
	else dirp -> dd_off = -1;
	dirp -> dd_id &= ~DID_IFLABEL;

	return(&d);
}

static int NEAR unixrewinddir(dirp)
DIR *dirp;
{
	return(0);
}

static int NEAR unixchdir(path)
CONST char *path;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x3b00;
# ifdef	DJGPP
	dos_putpath(path, 0);
# endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);

	return(intcall(0x21, &reg, &sreg));
}

static char *NEAR unixgetcwd(path, size)
char *path;
ALLOC_T size;
{
# ifdef	DJGPP
	int i;
# endif
	char *cp;

# ifdef	USEGETWD
	cp = (char *)getwd(path);
# else
	cp = (char *)getcwd(path, size);
# endif
	if (!cp) return(NULL);
# ifdef	DJGPP
	for (i = 0; cp[i]; i++) if (cp[i] == '/') cp[i] = _SC_;
# endif

	return(cp);
}

static u_int NEAR getdosmode(attr)
u_int attr;
{
	u_int mode;

	mode = 0;
	if (attr & DS_IARCHIVE) mode |= S_ISVTX;
	if (!(attr & DS_IHIDDEN)) mode |= S_IRUSR;
	if (!(attr & DS_IRDONLY)) mode |= S_IWUSR;
	if (attr & DS_IFDIR) mode |= (S_IFDIR | S_IXUSR);
	else if (attr & DS_IFLABEL) mode |= S_IFIFO;
	else if (attr & DS_IFSYSTEM) mode |= S_IFSOCK;
	else mode |= S_IFREG;

	return(mode);
}

static time_t NEAR getdostime(d, t)
u_int d, t;
{
	struct tm tm;

	tm.tm_year = 1980 + ((d >> 9) & 0x7f);
	tm.tm_year -= 1900;
	tm.tm_mon = ((d >> 5) & 0x0f) - 1;
	tm.tm_mday = (d & 0x1f);
	tm.tm_hour = ((t >> 11) & 0x1f);
	tm.tm_min = ((t >> 5) & 0x3f);
	tm.tm_sec = ((t << 1) & 0x3e);
	tm.tm_isdst = -1;

	return(mktime(&tm));
}

static int NEAR unixstat(path, stp)
CONST char *path;
struct stat *stp;
{
	struct find_t find;

	if (_dos_findfirst(path, SEARCHATTRS, &find) != 0) return(-1);
	stp -> st_mode = getdosmode(find.ff_attrib);
	stp -> st_mtime = getdostime(find.ff_fdate, find.ff_ftime);
	stp -> st_size = find.ff_fsize;
	stp -> st_ctime = stp -> st_atime = stp -> st_mtime;
	stp -> st_dev = stp -> st_ino = 0;
	stp -> st_nlink = 1;
	stp -> st_uid = stp -> st_gid = -1;

	return(0);
}

static VOID NEAR putdostime(dp, tp, t)
u_short *dp, *tp;
time_t t;
{
	struct tm *tm;

	tm = localtime(&t);
	*dp = (((tm -> tm_year - 80) & 0x7f) << 9)
		+ (((tm -> tm_mon + 1) & 0x0f) << 5)
		+ (tm -> tm_mday & 0x1f);
	*tp = ((tm -> tm_hour & 0x1f) << 11)
		+ ((tm -> tm_min & 0x3f) << 5)
		+ ((tm -> tm_sec & 0x3e) >> 1);
}

# ifdef	USEUTIME
static int NEAR unixutime(path, times)
CONST char *path;
CONST struct utimbuf *times;
{
	time_t t;
	__dpmi_regs reg;
	struct SREGS sreg;
	int n, fd;

	t = times -> modtime;
	if ((fd = open(path, O_RDONLY, 0666)) < 0) return(-1);
	reg.x.ax = 0x5701;
	reg.x.bx = fd;
	putdostime(&(reg.x.dx), &(reg.x.cx), t);
	n = intcall(0x21, &reg, &sreg);
	VOID_C close(fd);

	return(n);
}
# else
static int NEAR unixutimes(path, tvp)
CONST char *path;
CONST struct timeval *tvp;
{
	time_t t;
	__dpmi_regs reg;
	struct SREGS sreg;
	int n, fd;

	t = tvp[1].tv_sec;
	if ((fd = open(path, O_RDONLY, 0666)) < 0) return(-1);
	reg.x.ax = 0x5701;
	reg.x.bx = fd;
	putdostime(&(reg.x.dx), &(reg.x.cx), t);
	n = intcall(0x21, &reg, &sreg);
	VOID_C close(fd);

	return(n);
}
# endif
#endif	/* MSDOS && !FD */

#ifdef	DEP_DIRENT
DIR *Xopendir(path)
CONST char *path;
{
#if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) \
|| defined (DEP_URLPATH) || defined (CYGWIN)
	char tmp[MAXPATHLEN];
#endif
#ifdef	DEP_URLPATH
	char *host;
	int n, type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	DIR *dirp;
	CONST char *cp;
	int dev;

	dev = DEV_NORMAL;
	cp = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(cp)) {
		dev = DEV_DOS;
		dirp = dosopendir(cp);
	}
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(cp, &host, tmp, &type))) {
		dev = DEV_URL;
		dirp = urlopendir(host, type, &(tmp[n]));
		Xfree(host);
	}
	else
#endif
	dirp = unixopendir(cp);
	if (!dirp) return(NULL);

#if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) || defined (CYGWIN)
	cp = Xrealpath(path, tmp, RLP_READLINK);
#endif
#ifdef	CYGWIN
	if (tmp[0] != _SC_ || tmp[1])
		dirp -> __flags |=
			opendir_saw_u_cygdrive | opendir_saw_s_cygdrive;
#endif
#if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) \
||	defined (DEP_DOSEMU) || defined (DEP_URLPATH)
	putopenlist(OP_DIRP, dev, dirp, convput(conv, cp, 0, 1, NULL, NULL));
#endif

	return(dirp);
}

int Xclosedir(dirp)
DIR *dirp;
{
#if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) \
||	defined (DEP_DOSEMU) || defined (DEP_URLPATH)
	switch (delopenlist(OP_DIRP, dirp)) {
# ifdef	DEP_DOSEMU
		case DEV_DOS:
			return(dosclosedir(dirp));
/*NOTREACHED*/
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
			return(urlclosedir(dirp));
/*NOTREACHED*/
			break;
# endif
		default:
			break;
	}
#endif	/* DEP_KANJIPATH || DEP_ROCKRIDGE || DEP_DOSEMU || DEP_URLPATH */

	return(unixclosedir(dirp));
}

#ifdef	CYGWIN
static struct dirent *NEAR pseudoreaddir(dirp)
DIR *dirp;
{
	static char *upath = NULL;
	static char *spath = NULL;
	struct dirent *dp;
	char ubuf[MAXPATHLEN], sbuf[MAXPATHLEN];

	if (!upath) {
		cygwin_internal(CW_GET_CYGDRIVE_PREFIXES, ubuf, sbuf);
		for (upath = ubuf; *upath == _SC_; upath++);
# ifdef	DEBUG
		_mtrace_file = "pseudoreaddir(upath)";
# endif
		upath = Xstrdup(upath);
		for (spath = sbuf; *spath == _SC_; spath++);
		if (*upath && !strpathcmp(spath, upath)) *spath = '\0';
# ifdef	DEBUG
		_mtrace_file = "pseudoreaddir(spath)";
# endif
		spath = Xstrdup(spath);
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
			Xstrcpy(dp -> d_name, upath);
			dirp -> __flags |= opendir_saw_u_cygdrive;
			dirp -> __d_position++;
		}
		else if (!(dirp -> __flags & opendir_saw_s_cygdrive)) {
			dp = dirp -> __d_dirent;
			Xstrcpy(dp -> d_name, spath);
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
#if	!defined (DEP_DOSEMU) && !defined (DEP_URLPATH) \
&& !defined (DEP_KANJIPATH) && !defined (DEP_ROCKRIDGE)
	return(pseudoreaddir(dirp));
#else	/* DEP_DOSEMU || DEP_URLPATH || DEP_KANJIPATH || DEP_ROCKRIDGE */
# if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE)
	char path[MAXPATHLEN], conv[MAXPATHLEN];
# endif
	static st_dirent buf;
	struct dirent *dp;
	char *src, *dest;
	int n, dev;

	n = getopenlist(OP_DIRP, dirp);
	dev = (n >= 0) ? openlist[n].dev : DEV_NORMAL;
	switch (dev) {
# ifdef	DEP_DOSEMU
		case DEV_DOS:
			dp = (struct dirent *)dosreaddir(dirp);
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
			dp = urlreaddir(dirp);
			break;
# endif
		default:
			dp = pseudoreaddir(dirp);
			break;
	}
	if (!dp) return(NULL);

	dest = ((struct dirent *)&buf) -> d_name;
# if	defined (CYGWIN) && defined (DEP_DOSEMU)
	/* Some versions of Cygwin have neither d_fileno nor d_ino */
	if (dev == DEV_DOS) {
		src = ((struct dosdirent *)dp) -> d_name;
		wrap_reclen(&buf) = ((struct dosdirent *)dp) -> d_reclen;
	}
	else
# endif
	{
		src = dp -> d_name;
		memcpy((char *)(&buf), (char *)dp, dest - (char *)&buf);
	}
# if	MSDOS && defined (FD)
	memcpy(&(buf.d_alias), dp -> d_alias, sizeof(dp -> d_alias));
# endif

	if (isdotdir(src)) {
		Xstrcpy(dest, src);
		return((struct dirent *)&buf);
	}
# if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE)
	if (n >= 0) {
		strcatdelim2(path, openlist[n].path, src);
		if (convget(conv, path, dev) == conv) {
			if ((src = strrdelim(conv, 0))) src++;
			else src = conv;
		}
		Xstrcpy(dest, src);
	}
	else
# endif	/* DEP_KANJIPATH || DEP_ROCKRIDGE */
# if	defined (DEP_DOSEMU) && defined (CODEEUC)
	if (dev == DEV_DOS && !noconv)
		dest[sjis2ujis(dest, (u_char *)src, MAXNAMLEN)] = '\0';
	else
# endif
	Xstrcpy(dest, src);

	return((struct dirent *)&buf);
#endif	/* DEP_DOSEMU || DEP_URLPATH || DEP_KANJIPATH || DEP_ROCKRIDGE */
}

VOID Xrewinddir(dirp)
DIR *dirp;
{
#if	defined (DEP_DOSEMU) || defined (DEP_URLPATH)
	switch (chkopenlist(OP_DIRP, dirp)) {
# ifdef	DEP_DOSEMU
		case DEV_DOS:
			dosrewinddir(dirp);
			return;
/*NOTREACHED*/
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
			urlrewinddir(dirp);
			return;
/*NOTREACHED*/
			break;
# endif
	}
#endif	/* DEP_DOSEMU || DEP_URLPATH */

	unixrewinddir(dirp);
}
#endif	/* DEP_DIRENT */

#if	MSDOS
int rawchdir(path)
CONST char *path;
{
	if (setcurdrv(dospath(path, NULL), 1) < 0 || unixchdir(path) < 0)
		return(-1);

	return(0);
}
#endif	/* MSDOS */

#ifdef	DEP_DOSDRIVE
/*ARGSUSED*/
static int NEAR checkchdir(drive, path)
int drive;
CONST char *path;
{
# if	MSDOS
	char tmp[MAXPATHLEN];
# endif

# if	MSDOS
	if (setcurdrv(drive, 1) < 0) return(-1);
	if (checkpath(path, tmp)) path = tmp;
	else return(unixchdir(path));
# endif
	return(doschdir(path));
}
#endif	/* DEP_DOSDRIVE */

int Xchdir(path)
CONST char *path;
{
#ifdef	DEP_PSEUDOPATH
	char tmp[MAXPATHLEN];
	int dd, drv, drive, dev, lastdev;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifndef	DEP_PSEUDOPATH
	n = rawchdir(path);
#else	/* DEP_PSEUDOPATH */
	lastdev = checkdrv(lastdrv, NULL);
	if ((drv = preparedrv(path, &drive, tmp)) < 0) n = dev = -1;
	else switch ((dev = checkdrv(drv, &dd))) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			n = checkchdir(drive, path);
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
			n = urlchdir(dd, &(tmp[drive]));
			break;
# endif
		case DEV_NORMAL:
			n = rawchdir(path);
			break;
		default:
			n = seterrno(ENOENT);
			break;
	}

	if (n < 0) shutdrv(drv);
	else {
		switch (lastdev) {
			case DEV_DOS:
			case DEV_URL:
				shutdrv(lastdrv);
				break;
			default:
# if	!MSDOS
				switch (dev) {
					case DEV_DOS:
					case DEV_URL:
						VOID_C rawchdir(rootpath);
						break;
					default:
						break;
				}
# endif
				break;
		}
		lastdrv = drv;
	}
#endif	/* DEP_PSEUDOPATH */

	LOG1(_LOG_INFO_, n, "chdir(\"%k\");", path);
#if	defined (DEP_DOSEMU) || defined (DEP_URLPATH)
	if (n < 0) return(-1);
	else if (n) /*EMPTY*/;
	else if (!Xgetwd(cachecwd)) {
		*cachecwd = '\0';
		n = -1;
	}
# if	defined (FD) && defined (DEP_URLPATH)
	else if (dev == DEV_URL) Xstrcpy(fullpath, cachecwd);
# endif
#endif	/* DEP_DOSEMU || DEP_URLPATH */

	return(n);
}

char *Xgetwd(path)
char *path;
{
#ifdef	DEP_DOSEMU
	int i;
#endif
#ifdef	DEP_URLPATH
	int drv;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	char *cp;

#if	defined (DEP_DOSEMU) || defined (DEP_URLPATH)
	if (path == cachecwd || !*cachecwd) /*EMPTY*/;
# ifdef	DEP_DOSEMU
	else if (_dospath(cachecwd)) /*EMPTY*/;
# endif
	else {
		Xstrcpy(path, cachecwd);
		return(path);
	}
#endif	/* DEP_DOSEMU || DEP_URLPATH */

#ifdef	DEP_DOSEMU
	if (dosdrive && checkdrv(lastdrv, NULL) == DEV_DOS) {
		if (!(cp = dosgetcwd(path, MAXPATHLEN))) return(NULL);
		if (Xisupper(cp[0])) {
			for (i = 2; cp[i]; i++) {
				if (issjis1((u_char)(cp[i]))) i++;
				else cp[i] = Xtolower(cp[i]);
			}
		}
		cp = convget(conv, cp, DEV_DOS);
	}
	else
#endif
#ifdef	DEP_URLPATH
	if (urldrive && checkdrv(lastdrv, &drv) == DEV_URL) {
		if (!(cp = urlgetcwd(drv, path, MAXPATHLEN))) return(NULL);
		else cp = convget(conv, cp, DEV_URL);
	}
	else
#endif
	if (!(cp = unixgetcwd(path, MAXPATHLEN))) return(NULL);
	else cp = convget(conv, cp, DEV_NORMAL);

	if (cp == path) return(cp);
	Xstrcpy(path, cp);

	return(path);
}

#if	MSDOS
static int NEAR unixstat2(path, stp)
CONST char *path;
struct stat *stp;
{
	char *cp;
	u_short mode;

	if (unixstat(path, stp) < 0) return(-1);

	mode = (u_short)(stp -> st_mode);
	if ((mode & S_IFMT) != S_IFDIR
	&& (cp = Xstrrchr(path, '.')) && strlen(++cp) == 3) {
		if (!Xstrcasecmp(cp, EXTCOM)
		|| !Xstrcasecmp(cp, EXTEXE)
		|| !Xstrcasecmp(cp, EXTBAT))
			mode |= S_IXUSR;
	}
	mode &= (S_IRUSR | S_IWUSR | S_IXUSR);
	mode |= (mode >> 3) | (mode >> 6);
	stp -> st_mode |= mode;

	return(0);
}
#endif	/* MSDOS */

#ifdef	DEP_DIRENT
int Xstat(path, stp)
CONST char *path;
struct stat *stp;
{
#ifdef	DEP_URLPATH
	char *host, tmp[MAXPATHLEN];
	int type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(path)) n = dosstat(path, stp);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, tmp, &type))) {
		n = urlstat(host, type, &(tmp[n]), stp);
		Xfree(host);
	}
	else
#endif
	n = unixstat2(path, stp);

	return(n);
}

int Xlstat(path, stp)
CONST char *path;
struct stat *stp;
{
#ifdef	DEP_ROCKRIDGE
	char rpath[MAXPATHLEN];
#endif
#ifdef	DEP_URLPATH
	char *host, tmp[MAXPATHLEN];
	int type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	int n;

#ifdef	DEP_ROCKRIDGE
	path = convput(conv, path, 1, 0, rpath, NULL);
#else
	path = convput(conv, path, 1, 0, NULL, NULL);
#endif
#ifdef	DEP_DOSEMU
	if (_dospath(path)) n = doslstat(path, stp);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, tmp, &type))) {
		n = urllstat(host, type, &(tmp[n]), stp);
		Xfree(host);
	}
	else
#endif
	n = unixlstat2(path, stp);
#ifdef	DEP_ROCKRIDGE
	if (n >= 0 && *rpath) rrlstat(rpath, stp);
#endif

	return(n);
}
#endif	/* DEP_DIRENT */

#ifdef	DEP_BIASPATH
int Xaccess(path, mode)
CONST char *path;
int mode;
{
#if	defined (DEP_DOSLFN) || defined (DEP_URLPATH)
	char tmp[MAXPATHLEN];
#endif
#ifdef	DEP_URLPATH
	char *host;
	int type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
#if	MSDOS
	struct stat st;
#endif
	CONST char *cp;
	int n;

	cp = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(cp)) n = dosaccess(cp, mode);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(cp, &host, tmp, &type))) {
		n = urlaccess(host, type, &(tmp[n]), mode);
		Xfree(host);
	}
	else
#endif
#ifdef	DEP_DOSLFN
# ifdef	DEP_DOSDRIVE
	if (checkpath(cp, tmp)) n = dosaccess(tmp, mode);
	else
# endif
	if (!(cp = preparefile(cp, tmp))) n = -1;
	else
#endif	/* DEP_DOSLFN */
	if ((n = (access(cp, mode)) ? -1 : 0) < 0) /*EMPTY*/;
#if	MSDOS
	else if (!(mode & X_OK)) /*EMPTY*/;
	else if (stat(path, &st) < 0 || !(st.st_mode & S_IXUSR))
		n = seterrno(EACCES);
#endif

	return(n);
}

int Xsymlink(name1, name2)
CONST char *name1, *name2;
{
#ifdef	FD
	char conv1[MAXPATHLEN], conv2[MAXPATHLEN];
#endif
	int n;

	name1 = convput(conv1, name1, 1, 0, NULL, NULL);
	name2 = convput(conv2, name2, 1, 0, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(name2)) n = dossymlink(name1, name2);
	else
#endif
#ifdef	DEP_URLPATH
	if (urlpath(name2, NULL, NULL, NULL)) n = seterrno(EACCES);
	else
#endif
#if	MSDOS
	n = seterrno(EACCES);
#else
	n = (symlink(name1, name2)) ? -1 : 0;
#endif
	LOG2(_LOG_WARNING_, n, "symlink(\"%k\", \"%k\");", name1, name2);

	return(n);
}

int Xreadlink(path, buf, bufsiz)
CONST char *path;
char *buf;
int bufsiz;
{
#ifdef	DEP_URLPATH
	char *host, tmp[MAXPATHLEN];
	int type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
	int c;
#endif
	char lbuf[MAXPATHLEN];
	int n;

	path = convput(conv, path, 1, 0, lbuf, &c);
#ifdef	DEP_ROCKRIDGE
	if (*lbuf && (n = rrreadlink(lbuf, lbuf, sizeof(lbuf) - 1)) >= 0)
		/*EMPTY*/;
	else
#endif
#ifdef	DEP_DOSEMU
	if (_dospath(path)) n = dosreadlink(path, lbuf, sizeof(lbuf) - 1);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, tmp, &type))) {
		n = urlreadlink(host, type, &(tmp[n]), lbuf, sizeof(lbuf) - 1);
		Xfree(host);
	}
	else
#endif
#if	MSDOS
	n = seterrno(EINVAL);
#else
	n = readlink(path, lbuf, sizeof(lbuf) - 1);
#endif

	if (n >= 0) {
		lbuf[n] = '\0';
#ifdef	DEP_KANJIPATH
		path = kanjiconv2(conv, lbuf,
			strsize(conv), c, DEFCODE, L_FNAME);
#else
		path = lbuf;
#endif
		for (n = 0; n < bufsiz && path[n]; n++) buf[n] = path[n];
	}

	return(n);
}

int Xchmod(path, mode)
CONST char *path;
int mode;
{
#ifdef	DEP_URLPATH
	char *host, tmp[MAXPATHLEN];
	int type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(path)) n = doschmod(path, mode);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, tmp, &type))) {
		n = urlchmod(host, type, &(tmp[n]), mode);
		Xfree(host);
	}
	else
#endif
	n = unixchmod(path, mode);
	LOG2(_LOG_NOTICE_, n, "chmod(\"%k\", %05o);", path, mode);

	return(n);
}
#endif	/* DEP_BIASPATH */

#ifdef	DEP_DIRENT
#ifdef	USEUTIME
int Xutime(path, times)
CONST char *path;
CONST struct utimbuf *times;
{
# ifdef	FD
	char conv[MAXPATHLEN];
# endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifdef	DEP_DOSEMU
	if (_dospath(path)) n = dosutime(path, times);
	else
# endif
# ifdef	DEP_URLPATH
	if (urlpath(path, NULL, NULL, NULL)) n = seterrno(EACCES);
	else
# endif
	n = unixutime(path, times);
	LOG1(_LOG_NOTICE_, n, "utime(\"%k\");", path);

	return(n);
}
#else	/* !USEUTIME */
int Xutimes(path, tvp)
CONST char *path;
CONST struct timeval *tvp;
{
# ifdef	FD
	char conv[MAXPATHLEN];
# endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifdef	DEP_DOSEMU
	if (_dospath(path)) n = dosutimes(path, tvp);
	else
# endif
# ifdef	DEP_URLPATH
	if (urlpath(path, NULL, NULL, NULL)) n = seterrno(EACCES);
	else
# endif
	n = unixutimes(path, tvp);
	LOG1(_LOG_NOTICE_, n, "utimes(\"%k\");", path);

	return(n);
}
#endif	/* !USEUTIME */
#endif	/* DEP_DIRENT */

#ifdef	DEP_BIASPATH
#ifdef	HAVEFLAGS
int Xchflags(path, flags)
CONST char *path;
u_long flags;
{
# ifdef	FD
	char conv[MAXPATHLEN];
# endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifdef	DEP_DOSEMU
	if (_dospath(path)) n = seterrno(EACCES);
	else
# endif
# ifdef	DEP_URLPATH
	if (urlpath(path, NULL, NULL, NULL)) n = seterrno(EACCES);
	else
# endif
# if	MSDOS
	n = seterrno(EACCES);
# else
	n = (chflags(path, flags)) ? -1 : 0;
# endif
	LOG2(_LOG_WARNING_, n, "chflags(\"%k\", %05o);", path, flags);

	return(n);
}
#endif	/* !HAVEFLAGS */

#ifndef	NOUID
int Xchown(path, uid, gid)
CONST char *path;
uid_t uid;
gid_t gid;
{
# ifdef	FD
	char conv[MAXPATHLEN];
# endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
# ifdef	DEP_DOSEMU
	if (_dospath(path)) n = seterrno(EACCES);
	else
# endif
# ifdef	DEP_URLPATH
	if (urlpath(path, NULL, NULL, NULL)) n = seterrno(EACCES);
	else
# endif
# if	MSDOS
	n = seterrno(EACCES);
# else
	n = (chown(path, uid, gid)) ? -1 : 0;
# endif
	LOG3(_LOG_WARNING_, n, "chown(\"%k\", %d, %d);", path, uid, gid);

	return(n);
}
#endif	/* !NOUID */

int Xunlink(path)
CONST char *path;
{
#ifdef	DEP_URLPATH
	char *host, tmp[MAXPATHLEN];
	int type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(path)) n = dosunlink(path);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, tmp, &type))) {
		n = urlunlink(host, type, &(tmp[n]));
		Xfree(host);
	}
	else
#endif
	{
		n = unixunlink(path);
#if	MSDOS
		if (n >= 0 || errno != EACCES) /*EMPTY*/;
		else if (unixchmod(path, (S_IRUSR | S_IWUSR | S_ISVTX)) >= 0)
			n = unixunlink(path);
#endif
	}
	LOG1(_LOG_WARNING_, n, "unlink(\"%k\");", path);

	return(n);
}

int Xrename(from, to)
CONST char *from, *to;
{
#ifdef	DEP_URLPATH
	char *host1, *host2, tmp1[MAXPATHLEN], tmp2[MAXPATHLEN];
	int n2, type1, type2;
#endif
#ifdef	FD
	char conv1[MAXPATHLEN], conv2[MAXPATHLEN];
#endif
	int n;

	from = convput(conv1, from, 1, 0, NULL, NULL);
	to = convput(conv2, to, 1, 0, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(from)) {
		if (_dospath(to)) n = dosrename(from, to);
		else n = seterrno(EXDEV);
	}
	else if (_dospath(to)) n = seterrno(EXDEV);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(from, &host1, tmp1, &type1))) {
		if ((n2 = urlpath(to, &host2, tmp2, &type2))) {
			if (type1 != type2 || cmpsockaddr(host1, host2))
				n = seterrno(EXDEV);
			else n = urlrename(host1,
				type1, &(tmp1[n]), &(tmp2[n2]));
			Xfree(host2);
		}
		else n = seterrno(EXDEV);
		Xfree(host1);
	}
	else if (urlpath(to, NULL, NULL, NULL)) n = seterrno(EXDEV);
	else
#endif
#if	MSDOS
	if (dospath(from, NULL) != dospath(to, NULL)) n = seterrno(EXDEV);
	else
#endif
	n = unixrename(from, to);
	LOG2(_LOG_WARNING_, n, "rename(\"%k\", \"%k\");", from, to);

	return(n);
}

int Xopen(path, flags, mode)
CONST char *path;
int flags, mode;
{
#if	defined (DEP_URLPATH) || defined (DEP_DOSLFN)
	char tmp[MAXPATHLEN];
#endif
#ifdef	DEP_URLPATH
	char *host;
	int n, type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
#ifdef	DEP_PSEUDOPATH
	int dev;
#endif
	int fd;

#ifdef	DEP_PSEUDOPATH
	dev = DEV_NORMAL;
#endif
	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(path)) {
		dev = DEV_DOS;
		fd = dosopen(path, flags, mode);
	}
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, tmp, &type))) {
		dev = DEV_URL;
		fd = urlopen(host, type, &(tmp[n]), flags);
		Xfree(host);
	}
	else
#endif
#ifdef	DEP_DOSLFN
# ifdef	DEP_DOSDRIVE
	if (checkpath(path, tmp)) {
		dev = DEV_DOS;
		fd = dosopen(tmp, flags, mode);
	}
	else
# endif
	if (flags & O_CREAT) fd = unixopen(path, flags, mode);
	else if (!(path = preparefile(path, tmp))) fd = -1;
	else
#endif	/* DEP_DOSLFN */
	fd = open(path, flags, mode);

#ifdef	DEP_PSEUDOPATH
	putopenfd(dev, fd);
#endif
#ifdef	DEP_LOGGING
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
#endif	/* DEP_LOGGING */

	return(fd);
}
#endif	/* DEP_BIASPATH */

#ifdef	DEP_PSEUDOPATH
VOID putopenfd(dev, fd)
int dev, fd;
{
	putopenlist(OP_FD, dev, &fd, NULL);
}

int chkopenfd(fd)
int fd;
{
	return(chkopenlist(OP_FD, &fd));
}

int delopenfd(fd)
int fd;
{
	return(delopenlist(OP_FD, &fd));
}

int Xclose(fd)
int fd;
{
	int n;

	switch (delopenfd(fd)) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			n = dosclose(fd);
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
		case DEV_HTTP:
			n = urlclose(fd);
			break;
# endif
		default:
			n = (close(fd)) ? -1 : 0;
			break;
	}

	return(n);
}

int Xread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	int n;

	switch (chkopenfd(fd)) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			n = dosread(fd, buf, nbytes);
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
		case DEV_HTTP:
			n = urlread(fd, buf, nbytes);
			break;
# endif
		default:
			n = read(fd, buf, nbytes);
			break;
	}

	return(n);
}

int Xwrite(fd, buf, nbytes)
int fd;
CONST char *buf;
int nbytes;
{
	int n;

	switch (chkopenfd(fd)) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			n = doswrite(fd, buf, nbytes);
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
		case DEV_HTTP:
			n = urlwrite(fd, buf, nbytes);
			break;
# endif
		default:
			n = write(fd, buf, nbytes);
			break;
	}

	return(n);
}

off_t Xlseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
	off_t ofs;

	switch (chkopenfd(fd)) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			ofs = doslseek(fd, offset, whence);
			break;
# endif
		default:
			ofs = lseek(fd, offset, whence);
			break;
	}

	return(ofs);
}

int Xftruncate(fd, len)
int fd;
off_t len;
{
#ifdef	__TURBOC__
	char buf[BUFSIZ];
	off_t ofs, eofs;
	int size, duperrno;
#endif
	int n;

	switch (chkopenfd(fd)) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			n = dosftruncate(fd, len);
			break;
# endif
		default:
#ifndef	__TURBOC__
			n = ftruncate(fd, len);
#else	/* __TURBOC__ */
			if ((ofs = lseek(fd, 0L, L_INCR)) < (off_t)0) {
				n = -1;
				break;
			}
			memset(buf, 0, sizeof(buf));
			if ((eofs = lseek(fd, 0L, L_XTND)) < (off_t)0) n = -1;
			else if (eofs < len) {
				while (eofs < len) {
					size = sizeof(buf);
					if (eofs + size > len)
						size = len - eofs;
					if (write(fd, buf, size) < 0) break;
				}
				n = (eofs < len) ? -1 : 0;
			}
			else if (eofs > len) {
				eofs = lseek(fd, len, L_SET);
				if (eofs == (off_t)-1) n = -1;
				else if (eofs != len) n = seterrno(EIO);
				else n = write(fd, buf, 0);
			}
			duperrno = errno;
			VOID_C lseek(fd, ofs, L_SET);
			errno = duperrno;
#endif	/* __TURBOC__ */
			break;
	}

	return(n);
}

int Xdup(old)
int old;
{
	int fd, dev;

	dev = chkopenfd(old);
	switch (dev) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			fd = seterrno(EBADF);
			break;
# endif
		default:
			fd = safe_dup(old);
			break;
	}

	if (fd >= 0) {
# ifdef	DEP_URLPATH
		if (dev == DEV_URL) VOID_C urldup2(old, fd);
# endif
		putopenfd(dev, fd);
	}

	return(fd);
}

int Xdup2(old, new)
int old, new;
{
	int fd, odev, ndev;

	if (old == new) return(new);

	odev = chkopenfd(old);
	ndev = chkopenfd(old);
# ifdef	DEP_DOSDRIVE
	if (odev == DEV_DOS || ndev == DEV_DOS) fd = seterrno(EBADF);
	else
# endif
	fd = safe_dup2(old, new);

	if (fd >= 0) {
# ifdef	DEP_URLPATH
		if (odev == DEV_URL || ndev == DEV_URL
		|| odev == DEV_HTTP || ndev == DEV_HTTP)
			VOID_C urldup2(old, fd);
# endif
		putopenfd(odev, fd);
	}

	return(fd);
}
#endif	/* DEP_PSEUDOPATH */

int Xmkdir(path, mode)
CONST char *path;
int mode;
{
#ifdef	DEP_URLPATH
	char *host, tmp[MAXPATHLEN];
	int type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(path)) n = dosmkdir(path, mode);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, tmp, &type))) {
		n = urlmkdir(host, type, &(tmp[n]));
		Xfree(host);
	}
	else
#endif
#if	MSDOS && !defined (DJGPP)
	if (Xaccess(path, F_OK) >= 0) n = seterrno(EEXIST);
	else
#endif
	n = unixmkdir(path, mode);
	LOG2(_LOG_WARNING_, n, "mkdir(\"%k\", %05o);", path, mode);

	return(n);
}

#ifdef	DEP_BIASPATH
int Xrmdir(path)
CONST char *path;
{
#ifdef	DEP_URLPATH
	char *host, tmp[MAXPATHLEN];
	int type;
#endif
#ifdef	FD
	char conv[MAXPATHLEN];
#endif
	int n;

	path = convput(conv, path, 1, 1, NULL, NULL);
#ifdef	DEP_DOSEMU
	if (_dospath(path)) n = dosrmdir(path);
	else
#endif
#ifdef	DEP_URLPATH
	if ((n = urlpath(path, &host, tmp, &type))) {
		n = urlrmdir(host, type, &(tmp[n]));
		Xfree(host);
	}
	else
#endif
	n = unixrmdir(path);
	LOG1(_LOG_WARNING_, n, "rmdir(\"%k\");", path);

	return(n);
}
#endif	/* DEP_BIASPATH */

#ifndef	NOFLOCK
int Xflock(fd, operation)
int fd, operation;
{
# ifdef	USEFCNTLOCK
	struct flock lock;
	int cmd;
# endif
	int n;

# ifdef	USEFCNTLOCK
	switch (operation & (LOCK_SH | LOCK_EX | LOCK_UN)) {
		case LOCK_SH:
			lock.l_type = F_RDLCK;
			break;
		case LOCK_EX:
			lock.l_type = F_WRLCK;
			break;
		default:
			lock.l_type = F_UNLCK;
			break;
	}
	cmd = (operation & LOCK_NB) ? F_SETLK : F_SETLKW;
	lock.l_start = lock.l_len = (off_t)0;
	lock.l_whence = SEEK_SET;

	n = fcntl(fd, cmd, &lock);
# else	/* !USEFCNTLOCK */
#  ifdef	USELOCKF
	switch (operation & (LOCK_SH | LOCK_EX | LOCK_UN)) {
		case LOCK_SH:
			operation = F_TEST;
			break;
		case LOCK_EX:
			operation = (operation & LOCK_NB) ? F_TLOCK : F_LOCK;
			break;
		default:
			operation = F_ULOCK;
			break;
	}
	n = lockf(fd, operation, (off_t)0);
#  else	/* !USELOCKF */
	n = flock(fd, operation);
#  endif	/* !USELOCKF */
# endif	/* !USEFCNTLOCK */

	if (n >= 0) {
		errno = 0;
		return(0);
	}

	return(-1);
}
#endif	/* !NOFLOCK */

#ifndef	NOSELECT
int checkread(fd, buf, nbytes, timeout)
int fd;
VOID_P buf;
int nbytes, timeout;
{
	struct timeval tv;
	int i, n;

	n = 1;
#ifdef	DEP_PSEUDOPATH
	switch (chkopenfd(fd)) {
# ifdef	DEP_DOSDRIVE
		case DEV_DOS:
			n = 0;
			break;
# endif
# ifdef	DEP_URLPATH
		case DEV_URL:
		case DEV_HTTP:
			if ((n = urlselect(fd)) <= 0) return(n);
			break;
# endif
		default:
			break;
	}
#endif	/* DEP_PSEUDOPATH */

	if (!n) i = timeout = 0;
	else for (i = 0; timeout <= 0 || i < timeout * 10; i++) {
		if (readintrfunc && (*readintrfunc)()) return(seterrno(EINTR));
		tv.tv_sec = 0L;
		tv.tv_usec = 1000000L / 10;
		if ((n = readselect(1, &fd, NULL, &tv)) < 0) return(-1);
		else if (n) break;
	}
	if (timeout > 0 && i >= timeout * 10) return(0);

	return(sureread(fd, buf, nbytes));
}
#endif	/* !NOSELECT */
