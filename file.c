/*
 *	file.c
 *
 *	file accessing module
 */

#include "fd.h"
#include "time.h"
#include "termio.h"
#include "realpath.h"
#include "kconv.h"
#include "func.h"
#include "kanji.h"

#ifdef	DEP_URLPATH
#include "urldisk.h"
#endif
#if	MSDOS
#include <process.h>
#endif

#define	MAXTMPNAMLEN		8
#define	LOCKEXT			"LCK"
#ifndef	DEP_DOSDRIVE
#define	DOSDIRENT		32
#define	LFNENTSIZ		13
#endif
#define	LFNONLY			" +,;=[]"
#define	DOSBODYLEN		8
#define	DOSEXTLEN		3
#define	FAT_NONE		0
#define	FAT_PRIMAL		1
#define	FAT_LFN			2
#define	FAT_DOSDRIVE		3

#if	MSDOS
extern int getcurdrv __P_((VOID_A));
#endif
#if	defined (DEP_DOSDRIVE) && defined (DEP_DOSLFN)
extern int checkdrive __P_((int));
#endif

extern int mark;
extern char fullpath[];
extern char *tmpfilename;
extern int physical_path;
#ifdef	DEP_PSEUDOPATH
extern char *unixpath;
#endif

#ifndef	NOFLOCK
static int NEAR fcntllock __P_((int, int));
#endif
static char *NEAR excllock __P_((CONST char *, int));
#ifdef	DEP_PSEUDOPATH
static int NEAR pushswap __P_((VOID_A));
static VOID NEAR popswap __P_((int));
static int NEAR realchdir __P_((CONST char *));
static char *NEAR realgetwd __P_((char *));
static int NEAR realmkdir __P_((CONST char *, int));
static int NEAR realrmdir __P_((CONST char *));
#else
#define	realchdir		Xchdir
#define	realgetwd		Xgetwd
#define	realmkdir		Xmkdir
#define	realrmdir		Xrmdir
#endif
static int NEAR cpfile __P_((CONST char *, CONST char *,
		struct stat *, struct stat *));
static VOID changemes __P_((VOID_A));
static int NEAR genrand __P_((int));
static int dounlink __P_((CONST char *));
static int dormdir __P_((CONST char *));
#ifndef	_NOWRITEFS
static int NEAR isexist __P_((CONST char *));
static int NEAR realdirsiz __P_((CONST char *, int, int, int, int));
static int NEAR getnamlen __P_((int, int, int, int, int));
static int NEAR saferename __P_((CONST char *, CONST char *));
static char *NEAR maketmpfile __P_((int, int, CONST char *, CONST char *));
#if	!MSDOS
static off_t NEAR getdirblocksize __P_((CONST char *));
static u_char *NEAR getentnum __P_((CONST char *, off_t));
#endif
static VOID NEAR restorefile __P_((char *, char *, int));
#endif	/* !_NOWRITEFS */

char *deftmpdir = NULL;
#if	!defined (_USEDOSCOPY) && !defined (_NOEXTRACOPY)
int inheritcopy = 0;
#endif
int tmpumask = TMPUMASK;

#ifdef	DEP_PSEUDOPATH
static int pseudodrv = -1;
static int *swaplist[] = {
# ifdef	DEP_DOSDRIVE
	&dosdrive,
# endif
# ifdef	DEP_URLPATH
	&urldrive,
# endif
};
#define	SWAPLISTSIZ		arraysize(swaplist)
#endif	/* DEP_PSEUDOPATH */


#ifdef	DEP_DOSEMU
CONST char *nodospath(path, file)
char *path;
CONST char *file;
{
	if (!_dospath(file)) return(file);
	path[0] = '.';
	path[1] = _SC_;
	Xstrcpy(&(path[2]), file);

	return(path);
}
#endif	/* DEP_DOSEMU */

int getstatus(namep)
namelist *namep;
{
#ifdef	DEP_DOSEMU
	char path[MAXPATHLEN];
#endif
	struct stat st, lst;
	CONST char *cp;

	cp = nodospath(path, namep -> name);
	if (Xlstat(cp, &lst) < 0 || stat2(cp, &st) < 0) return(-1);
	namep -> flags = 0;
	if (s_isdir(&st)) namep -> flags |= F_ISDIR;
	if (s_islnk(&lst)) namep -> flags |= F_ISLNK;

	if (isdisplnk(dispmode))
		memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
#if	!MSDOS
	if (s_ischr(&lst) || s_isblk(&lst)) namep -> flags |= F_ISDEV;
#endif

	namep -> st_mode = lst.st_mode;
	namep -> st_nlink = lst.st_nlink;
#ifndef	NOUID
	namep -> st_uid = lst.st_uid;
	namep -> st_gid = lst.st_gid;
#endif
#if	MSDOS
	namep -> st_size = lst.st_size;
#else
	namep -> st_size = isdev(namep) ? (off_t)(lst.st_rdev) : lst.st_size;
#endif
#ifdef	HAVEFLAGS
	namep -> st_flags = lst.st_flags;
#endif
	namep -> st_mtim = lst.st_mtime;
	namep -> flags |= logical_access2(&st);
	namep -> tmpflags |= F_STAT;

	return(0);
}

int cmplist(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	namelist *namep1, *namep2;
	char *cp1, *cp2;
	int tmp;

	namep1 = (namelist *)vp1;
	namep2 = (namelist *)vp2;
	if (!sorton) return(namep1 -> ent - namep2 -> ent);

#ifndef	_NOPRECEDE
	if (!havestat(namep1) && getstatus(namep1) < 0) return(1);
	if (!havestat(namep2) && getstatus(namep2) < 0) return(-1);
#endif

	if (!isdir(namep1) && isdir(namep2)) return(1);
	if (isdir(namep1) && !isdir(namep2)) return(-1);
	if ((tmp = isdotdir(namep2 -> name) - isdotdir(namep1 -> name)))
		return(tmp);

	switch (sorton & 7) {
		case 5:
			tmp = (int)strlen(namep1 -> name)
				- (int)strlen(namep2 -> name);
			if (tmp) break;
/*FALLTHRU*/
		case 1:
			tmp = strpathcmp2(namep1 -> name, namep2 -> name);
			break;
		case 2:
			if (isdir(namep1)) {
				tmp = strpathcmp2(namep1 -> name,
					namep2 -> name);
				break;
			}
			cp1 = namep1 -> name + strlen(namep1 -> name);
			cp2 = namep2 -> name + strlen(namep2 -> name);
			for (;;) {
				while (cp1 > namep1 -> name)
					if (*(--cp1) == '.') break;
				while (cp2 > namep2 -> name)
					if (*(--cp2) == '.') break;
				if (*cp2 != '.') {
					tmp = (*cp1 == '.');
					break;
				}
				else if (*cp1 != '.') {
					tmp = -1;
					break;
				}
				if ((tmp = strpathcmp2(cp1 + 1, cp2 + 1)))
					break;
			}
			break;
		case 3:
			if (isdir(namep1))
				tmp = strpathcmp2(namep1 -> name,
					namep2 -> name);
			else if (namep1 -> st_size == namep2 -> st_size)
				tmp = 0;
			else tmp = (namep1 -> st_size > namep2 -> st_size)
				? 1 : -1;
			break;
		case 4:
			if (namep1 -> st_mtim == namep2 -> st_mtim)
				tmp = 0;
			else tmp = (namep1 -> st_mtim > namep2 -> st_mtim)
				? 1 : -1;
			break;
		default:
			tmp = 0;
			break;
	}

	if (sorton > 7) tmp = -tmp;
	if (!tmp) tmp = namep1 -> ent - namep2 -> ent;

	return(tmp);
}

#ifndef	_NOTREE
int cmptree(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	treelist *tp1, *tp2;
	CONST char *cp1, *cp2;
	int tmp;

	tp1 = (treelist *)vp1;
	tp2 = (treelist *)vp2;
	if (!(tp1 -> name)) return(1);
	if (!(tp2 -> name)) return(-1);
	switch (sorton & 7) {
		case 1:
			tmp = strpathcmp2(tp1 -> name, tp2 -> name);
			break;
		case 2:
			if ((cp1 = Xstrrchr(tp1 -> name, '.'))) cp1++;
			else cp1 = nullstr;
			if ((cp2 = Xstrrchr(tp2 -> name, '.'))) cp2++;
			else cp2 = nullstr;
			tmp = strpathcmp2(cp1, cp2);
			break;
		case 3:
		case 4:
		default:
			tmp = 0;
			break;
	}
	if (sorton > 7) tmp = -tmp;

	return(tmp);
}
#endif	/* !_NOTREE */

/*ARGSUSED*/
struct dirent *searchdir(dirp, regexp, arcregstr)
DIR *dirp;
CONST reg_t *regexp;
CONST char *arcregstr;
{
	struct dirent *dp;
#ifndef	_NOARCHIVE
	struct stat st;
	namelist tmp;
	int i;
#endif

	if (regexp) while ((dp = Xreaddir(dirp))) {
		if (regexp_exec(regexp, dp -> d_name, 1)) break;
	}
#ifndef	_NOARCHIVE
	else if (arcregstr) while ((dp = Xreaddir(dirp))) {
		if (stat2(dp -> d_name, &st) < 0 || s_isdir(&st)) continue;

		tmp.name = dp -> d_name;
		tmp.flags = 0;
		tmp.tmpflags = F_STAT;
		i = searcharc(arcregstr, &tmp, 1, -1);
		if (i < 0) return(NULL);
		if (i) break;
	}
#endif
	else dp = Xreaddir(dirp);

	return(dp);
}

int underhome(buf)
char *buf;
{
	static char *homedir = NULL;
	CONST char *cp;
	char tmp[MAXPATHLEN], cwd[MAXPATHLEN];
	int len;

#if	MSDOS
	if (!buf) return(1);
#endif
	if (!Xgetwd(cwd)) {
		lostcwd(cwd);
		return(-1);
	}
	if (!homedir) {
		if (!(cp = gethomedir())) return(-1);
#ifdef	DEP_DOSDRIVE
		if (dospath2(cp)) return(-1);
#endif
#ifdef	DEP_URLPATH
		if (urlpath(cp, NULL, NULL, NULL)) return(-1);
#endif
		if (getrealpath(cp, tmp, cwd) != tmp) return(-1);
		homedir = Xstrdup(tmp);
	}

	if (buf && !physical_path) Xstrcpy(cwd, fullpath);
	len = strlen(homedir);
#if	MSDOS
	if (len <= 3) cp = NULL;
#else
	if (len <= 1) cp = NULL;
#endif
	else cp = underpath(cwd, homedir, len);

	if (buf) Xstrcpy(buf, (cp) ? cp : nullstr);
#ifdef	DEBUG
	Xfree(homedir);
	homedir = NULL;
#endif

	return((cp) ? 1 : 0);
}

int preparedir(dir)
CONST char *dir;
{
	struct stat st;
	char tmp[MAXPATHLEN];
	CONST char *cp;

	cp = dir;
#ifdef	DEP_DOSPATH
	if (_dospath(dir)) cp += 2;
#endif
	if (!isdotdir(cp)) {
		if (stat2(dir, &st) < 0) {
			if (errno != ENOENT) return(-1);
			if (Xmkdir(dir, 0777) < 0) {
#if	MSDOS
				if (errno != EEXIST) return(-1);
				if (cp[0] != _SC_ || cp[1]) {
					cp = getbasename(dir);
					if (cp[0] != '.' || cp[1]) return(-1);
				}
				st.st_mode = S_IFDIR;
#else
				return(-1);
#endif
			}
			else if (stat2(dir, &st) < 0) return(-1);
		}
		if (!s_isdir(&st)) return(seterrno(ENOTDIR));
	}
	entryhist(Xrealpath(dir, tmp, 0), HST_PATH | HST_UNIQ);

	return(0);
}

#ifndef	NOFLOCK
static int NEAR fcntllock(fd, operation)
int fd, operation;
{
	int i, n;

# ifdef	DEP_PSEUDOPATH
	if (chkopenfd(fd) != DEV_NORMAL) return(1);
# endif

	n = -1;
	errno = 0;
	for (i = 0; i < LCK_MAXRETRY; i++) {
		n = Xflock(fd, operation);
		if (n >= 0) {
			n = 1;
			break;
		}
# ifdef	EACCES
		else if (errno == EACCES) /*EMPTY*/;
# endif
# ifdef	EAGAIN
		else if (errno == EAGAIN) /*EMPTY*/;
# endif
# ifdef	EWOULDBLOCK
		else if (errno == EWOULDBLOCK) /*EMPTY*/;
# endif
		else break;

		if (intrkey(-1)) {
			errno = EINTR;
			break;
		}
# if	!MSDOS || defined (DJGPP)
		usleep(100000L);
# endif
	}
	if (i >= LCK_MAXRETRY) return(0);

	return(n);
}
#endif	/* !NOFLOCK */

static char *NEAR excllock(file, operation)
CONST char *file;
int operation;
{
#if	MSDOS
	char *ext;
#endif
	static char **locklist = NULL;
	struct stat st;
	char *cp, path[MAXPATHLEN];
	int i, fd;

	if (!file) {
		if (locklist) {
			for (i = 0; locklist[i]; i++) {
				VOID_C unlink(locklist[i]);
				Xfree(locklist[i]);
			}
			Xfree(locklist);
		}
		return(NULL);
	}

	if (operation & LOCK_UN) {
		VOID_C Xunlink(file);
		if (locklist) {
			for (i = 0; locklist[i]; i++)
				if (file == locklist[i]) break;
			if (locklist[i]) {
				for (; locklist[i + 1]; i++)
					locklist[i] = locklist[i + 1];
				locklist[i] = NULL;
			}
			if (!i) {
				Xfree(locklist);
				locklist = NULL;
			}
		}
		return(NULL);
	}

#if	MSDOS
	cp = getbasename(file);
	for (i = 0; cp[i]; i++) if (cp[i] == '.') break;
	ext = &(cp[i]);
	if (i > DOSBODYLEN - strsize(LOCKEXT))
		i = DOSBODYLEN - strsize(LOCKEXT);
	i += cp - file;
	VOID_C Xsnprintf(path, sizeof(path),
		"%-.*s%s%s", i, file, LOCKEXT, ext);
#else
	VOID_C Xsnprintf(path, sizeof(path), "%s.%s", file, LOCKEXT);
#endif

	fd = -1;
	for (i = 0; i < LCK_MAXRETRY; i++) {
		if (!(operation & LOCK_SH))
			fd = Xopen(path,
				O_BINARY | O_WRONLY | O_CREAT | O_EXCL,
				0666 & ~tmpumask);
		else if (Xlstat(path, &st) >= 0) fd = seterrno(EEXIST);
		else fd = (errno == ENOENT) ? 0 : -1;
		if (fd >= 0 || errno != EEXIST) break;

		if (intrkey(-1)) {
			errno = EINTR;
			break;
		}
#if	!MSDOS || defined (DJGPP)
		usleep(100000L);
#endif
	}
	if (i >= LCK_MAXRETRY) return(vnullstr);
	if (fd < 0) return(NULL);
	if (operation & LOCK_SH) return((char *)file);

	VOID_C Xclose(fd);
	cp = Xstrdup(path);
	i = countvar(locklist);
	locklist = (char **)Xrealloc(locklist, (i + 2) * sizeof(char *));
	locklist[i] = cp;
	locklist[++i] = NULL;

	return(cp);
}

lockbuf_t *lockopen(path, flags, mode)
CONST char *path;
int flags, mode;
{
#ifndef	NOFLOCK
	int n;
#endif
	char *lckname;
	lockbuf_t *lck;
	int fd, err, lckflags, operation, duperrno;

#ifdef	FAKEUNINIT
	fd = -1;	/* fake for -Wuninitialized */
#endif
	lckname = NULL;
	lckflags = 0;
	operation = ((flags & O_ACCMODE) == O_RDONLY) ? LOCK_SH : LOCK_EX;
	operation |= LOCK_NB;
	err = 0;

#ifndef	NOFLOCK
	if (isnfs(path) <= 0) {
		lckflags |= LCK_FLOCK;
		fd = newdup(Xopen(path, flags & ~O_TRUNC, mode));
		if (fd < 0) {
			if ((flags & O_ACCMODE) == O_WRONLY || errno != ENOENT)
				return(NULL);
		}
		else if ((n = fcntllock(fd, operation)) <= 0) {
			VOID_C Xclose(fd);
			if (n < 0) return(NULL);
			err++;
			lckflags &= ~LCK_FLOCK;
		}
		else if ((flags & O_TRUNC) && Xftruncate(fd, (off_t)0) < 0) {
			duperrno = errno;
			VOID_C fcntllock(fd, LOCK_UN);
			VOID_C Xclose(fd);
			errno = duperrno;
			return(NULL);
		}
	}
#endif	/* !NOFLOCK */

	if (!(lckflags & LCK_FLOCK)) {
		if (!(lckname = excllock(path, operation))) return(NULL);
		else if (operation & LOCK_SH) {
			if (lckname == vnullstr) err++;
			lckname = NULL;
		}
		else if (lckname == vnullstr) return(NULL);

		if ((fd = newdup(Xopen(path, flags, mode))) >= 0) /*EMPTY*/;
		else if ((flags & O_ACCMODE) == O_WRONLY || errno != ENOENT) {
			duperrno = errno;
			if (lckname) {
				VOID_C excllock(lckname, LOCK_UN);
				Xfree(lckname);
			}
			errno = duperrno;
			return(NULL);
		}
	}

	if (fd < 0) lckflags |= LCK_INVALID;
	else if (err) {
		VOID_C Xfprintf(Xstderr, "%k: %k\r\n", path, NOLCK_K);
		if (isttyiomode) warning(0, HITKY_K);
	}

	lck = (lockbuf_t *)Xmalloc(sizeof(lockbuf_t));
	lck -> fd = fd;
	lck -> fp = NULL;
	lck -> name = lckname;
	lck -> flags = lckflags;

	return(lck);
}

lockbuf_t *lockfopen(path, type, flags)
CONST char *path, *type;
int flags;
{
	lockbuf_t *lck;
	XFILE *fp;

	if (!(lck = lockopen(path, flags, 0666))) return(NULL);

	if (!(lck -> flags & LCK_INVALID)) {
		if (!(fp = Xfdopen(lck -> fd, type))) {
			lockclose(lck);
			return(NULL);
		}
		lck -> fp = fp;
	}
	lck -> flags |= LCK_STREAM;

	return(lck);
}

VOID lockclose(lck)
lockbuf_t *lck;
{
	if (lck) {
		if (lck -> name) {
			VOID_C excllock(lck -> name, LOCK_UN);
			Xfree(lck -> name);
		}
		if (!(lck -> flags & LCK_INVALID)) {
#ifndef	NOFLOCK
			if (lck -> flags & LCK_FLOCK)
				VOID_C fcntllock(lck -> fd, LOCK_UN);
#endif
			if (lck -> flags & LCK_STREAM)
				VOID_C Xfclose(lck -> fp);
			else VOID_C Xclose(lck -> fd);
		}

		Xfree(lck);
	}
}

int touchfile(path, stp)
CONST char *path;
struct stat *stp;
{
#ifdef	USEUTIME
	struct utimbuf times;
#else
	struct timeval tvp[2];
#endif
#ifndef	_NOEXTRAATTR
	int i;
#endif
	struct stat st;
	u_int mode;
	int ret, duperrno;

	if (!(stp -> st_nlink)) return(0);
	if (Xlstat(path, &st) < 0) return(-1);
	if (s_islnk(&st)) return(1);

	ret = 0;
#if	MSDOS
	if (!(st.st_mode & S_IWUSR)) Xchmod(path, (st.st_mode | S_IWUSR));
#endif

	if (stp -> st_nlink & (TCH_ATIME | TCH_MTIME)) {
		if (!(stp -> st_nlink & TCH_ATIME))
			stp -> st_atime = st.st_atime;
		if (!(stp -> st_nlink & TCH_MTIME))
			stp -> st_mtime = st.st_mtime;
#ifdef	USEUTIME
		times.actime = stp -> st_atime;
		times.modtime = stp -> st_mtime;
		duperrno = errno;
		if (Xutime(path, &times) >= 0) errno = duperrno;
		else ret = -1;
#else
		tvp[0].tv_sec = stp -> st_atime;
		tvp[0].tv_usec = 0;
		tvp[1].tv_sec = stp -> st_mtime;
		tvp[1].tv_usec = 0;
		duperrno = errno;
		if (Xutimes(path, tvp) >= 0) errno = duperrno;
		else ret = -1;
#endif
	}

#ifdef	HAVEFLAGS
	if (stp -> st_nlink & TCH_FLAGS) {
		duperrno = errno;
		if (Xchflags(path, stp -> st_flags) >= 0) errno = duperrno;
		else ret = -1;
	}
#endif

	if (stp -> st_nlink & TCH_MODE) {
		mode = stp -> st_mode;
		mode &= ~S_IFMT;
		mode |= (st.st_mode & S_IFMT);
#ifndef	_NOEXTRAATTR
		if ((stp -> st_nlink & TCH_MODEEXE)
		&& !s_isdir(&st) && !(st.st_mode & S_IEXEC_ALL)) {
			for (i = 0; i < 3; i++) {
				if (!(stp -> st_mode & (1 << (i + 12))))
					continue;
				mode &= ~(1 << (i * 3));
				mode |= (st.st_mode & (1 << (i * 3)));
			}
		}
		if (stp -> st_nlink & TCH_MASK) {
			mode &= ~(stp -> st_size);
			mode |= (st.st_mode & (stp -> st_size));
		}
#endif	/* !_NOEXTRAATTR */
		duperrno = errno;
		if (Xchmod(path, mode) >= 0) errno = duperrno;
		else ret = -1;
	}
#if	MSDOS
	else if (!(st.st_mode & S_IWUSR)) {
		duperrno = errno;
		Xchmod(path, st.st_mode);
		errno = duperrno;
	}
#endif

#ifndef	NOUID
	if (stp -> st_nlink & (TCH_UID | TCH_GID)) {
		if (!(stp -> st_nlink & TCH_UID)) stp -> st_uid = (uid_t)-1;
		if (!(stp -> st_nlink & TCH_GID)) stp -> st_gid = (gid_t)-1;
		duperrno = errno;
		if (Xchown(path, stp -> st_uid, stp -> st_gid) >= 0)
			errno = duperrno;
		else if (!(stp -> st_nlink & TCH_CHANGE)) {
			Xchown(path, (uid_t)-1, stp -> st_gid);
			errno = duperrno;
		}
		else ret = -1;
	}
#endif	/* !NOUID */
	if (stp -> st_nlink & TCH_IGNOREERR) ret = 0;

	return(ret);
}

#ifdef	DEP_PSEUDOPATH
static int NEAR pushswap(VOID_A)
{
	int i, flags;

	flags = 0;
	for (i = 0; i < SWAPLISTSIZ; i++) {
		flags <<= 2;
		flags |= (*(swaplist[i]) & 3);
		*(swaplist[i]) = 0;
	}

	return(flags);
}

static VOID NEAR popswap(flags)
int flags;
{
	int i;

	for (i = SWAPLISTSIZ - 1; i >= 0; i--) {
		*(swaplist[i]) = (flags & 3);
		flags >>= 2;
	}
}

static int NEAR realchdir(path)
CONST char *path;
{
	int n, flags;

	flags = pushswap();
	n = Xchdir(path);
	popswap(flags);

	return(n);
}

static char *NEAR realgetwd(path)
char *path;
{
	char *cp;
	int flags;

	flags = pushswap();
	cp = Xgetwd(path);
	popswap(flags);

	return(cp);
}

int reallstat(path, stp)
CONST char *path;
struct stat *stp;
{
	int n, flags;

	flags = pushswap();
	n = Xlstat(path, stp);
	popswap(flags);

	return(n);
}

static int NEAR realmkdir(path, mode)
CONST char *path;
int mode;
{
	int n, flags;

	flags = pushswap();
	n = Xmkdir(path, mode);
	popswap(flags);

	return(n);
}

static int NEAR realrmdir(path)
CONST char *path;
{
	int n, flags;

	flags = pushswap();
	n = Xrmdir(path);
	popswap(flags);

	return(n);
}
#endif	/* DEP_PSEUDOPATH */

VOID lostcwd(path)
char *path;
{
	CONST char *cp;
	char buf[MAXPATHLEN];
	int duperrno;

	duperrno = errno;
	if (!path) path = buf;

	if (path != fullpath && realchdir(fullpath) >= 0 && realgetwd(path))
		cp = NOCWD_K;
#ifdef	DEP_PSEUDOPATH
	else if (unixpath && realchdir(unixpath) >= 0 && realgetwd(path))
		cp = NOPSU_K;
#endif
	else if ((cp = gethomedir()) && realchdir(cp) >= 0 && realgetwd(path))
		cp = GOHOM_K;
	else if (realchdir(rootpath) >= 0 && realgetwd(path)) cp = GOROT_K;
	else error(rootpath);

	if (path != fullpath) Xstrncpy(fullpath, path, MAXPATHLEN - 1);
	warning(0, cp);
#ifndef	_NOUSEHASH
	searchhash(NULL, nullstr, nullstr);
#endif
	errno = duperrno;
}

#ifndef	NODIRLOOP
int issamebody(src, dest)
CONST char *src, *dest;
{
	struct stat st1, st2;

	if (Xstat(src, &st1) < 0 || Xstat(dest, &st2) < 0) return(0);
	if (st1.st_dev != st2.st_dev || st1.st_ino != st2.st_ino) return(0);
# ifdef	DEP_URLPATH
	if (st1.st_dev == (dev_t)-1 || st1.st_ino == (ino_t)-1) return(0);
# endif

	return(1);
}
#endif	/* !NODIRLOOP */

#ifndef	NOSYMLINK
int cpsymlink(src, dest)
CONST char *src, *dest;
{
	struct stat st;
	char path[MAXPATHLEN];
	int len;

	if ((len = Xreadlink(src, path, strsize(path))) < 0) return(-1);
	if (Xlstat(dest, &st) >= 0) {
# ifndef	NODIRLOOP
		if (issamebody(src, dest)) return(0);
# endif
		if (Xunlink(dest) < 0) return(-1);
	}
	path[len] = '\0';
	if (Xsymlink(path, dest) < 0) return(-1);

	return(1);
}
#endif	/* !NOSYMLINK */

/*ARGSUSED*/
static int NEAR cpfile(src, dest, stp1, stp2)
CONST char *src, *dest;
struct stat *stp1, *stp2;
{
#if	MSDOS
	struct stat st;
#endif
#ifndef	NODIRLOOP
	int issame;
#endif
	char buf[BUFSIZ];
	off_t total;
	int n, fd1, fd2, tty, flags, mode, timeout, duperrno;

#if	MSDOS
	if (!stp2 && Xlstat(dest, &st) >= 0) stp2 = &st;
	if (stp2 && !(stp2 -> st_mode & S_IWUSR))
		Xchmod(src, stp2 -> st_mode | S_IWUSR);
#endif
#ifndef	NOSYMLINK
	if (s_islnk(stp1)) {
# ifdef	DEP_URLPATH
		fd1 = urlpath(src, NULL, NULL, NULL);
		fd2 = urlpath(dest, NULL, NULL, NULL);
		if (fd1 != fd2) {
			if (Xstat(src, stp1) < 0) return(-1);
		}
		else
# endif
		if (cpsymlink(src, dest) < 0) return(-1);
		else return(0);
	}
#endif	/* !NOSYMLINK */

	flags = (O_BINARY | O_RDONLY);
	mode = stp1 -> st_mode;
#ifndef	NODIRLOOP
	issame = issamebody(src, dest);
#endif
	if ((fd1 = Xopen(src, flags, mode)) < 0) return(-1);

	flags = (O_BINARY | O_WRONLY | O_CREAT | O_TRUNC);
#if	MSDOS
	mode |= S_IWUSR;
#endif
#ifndef	NODIRLOOP
	if (issame) {
		flags |= O_EXCL;
		if (Xunlink(dest) < 0) {
			VOID_C Xclose(fd1);
			return(-1);
		}
	}
#endif
	if ((fd2 = Xopen(dest, flags, mode)) < 0) {
		VOID_C Xclose(fd1);
		return(-1);
	}

	tty = isatty(fd1);
	timeout = 0;
#ifdef	DEP_URLPATH
	switch (chkopenfd(fd1)) {
		case DEV_URL:
		case DEV_HTTP:
			timeout = urltimeout;
			VOID_C urlfstat(fd1, stp1);
			break;
		default:
			break;
	}
#endif

	total = (off_t)0;
	if (tty) stp1 -> st_size = (off_t)-1;
	for (;;) {
		if (stp1 -> st_size >= (off_t)0 && total >= stp1 -> st_size)
			n = 0;
		else n = checkread(fd1, buf, sizeof(buf), timeout);
		if (n < 0) break;
		total += (off_t)n;
		if (!n) {
			if (total < stp1 -> st_size) n = seterrno(ETIMEDOUT);
			break;
		}
#ifndef	_NOEXTRACOPY
		showprogress(n);
#endif
		if ((n = surewrite(fd2, buf, n)) < 0) break;
	}

	safeclose(fd2);
	safeclose(fd1);
#ifndef	_NOEXTRACOPY
	fshowprogress(dest);
#endif

	if (n < 0) {
		duperrno = errno;
		VOID_C Xunlink(dest);
		errno = duperrno;
		return(-1);
	}
	stp1 -> st_nlink = (TCH_ATIME | TCH_MTIME | TCH_IGNOREERR);
#if	defined (_USEDOSCOPY) || !defined (_NOEXTRACOPY)
# ifndef	_USEDOSCOPY
	if (!inheritcopy) /*EMPTY*/;
	else
# endif
	if (touchfile(dest, stp1) < 0) return(-1);
#endif	/* _USEDOSCOPY || !_NOEXTRACOPY */

	return(0);
}

static VOID changemes(VOID_A)
{
	warning(0, CHGFD_K);
}

int safecpfile(src, dest, stp1, stp2)
CONST char *src, *dest;
struct stat *stp1, *stp2;
{
#ifdef	DEP_DOSDRIVE
	int drive;
#endif

	for (;;) {
		if (cpfile(src, dest, stp1, stp2) >= 0) break;
		if (errno && errno != ENOSPC) return(-1);
		for (;;) {
			if (!yesno(NOSPC_K)) return(seterrno(ENOSPC));
#ifdef	DEP_DOSDRIVE
			if ((drive = dospath3(dest))) {
				if (flushdrv(drive, changemes) < 0) continue;
			}
			else
#endif
			changemes();
			break;
		}
	}

	return(0);
}

int safemvfile(src, dest, stp1, stp2)
CONST char *src, *dest;
struct stat *stp1, *stp2;
{
	if (Xrename(src, dest) >= 0) {
#ifndef	_NOEXTRACOPY
		fshowprogress(dest);
#endif
		return(0);
	}
	if (errno != EXDEV || s_isdir(stp1)) return(-1);
	if (safecpfile(src, dest, stp1, stp2) < 0 || Xunlink(src) < 0)
		return(-1);

	stp1 -> st_nlink = (TCH_MODE | TCH_UID | TCH_GID
		| TCH_ATIME | TCH_MTIME | TCH_IGNOREERR);
	return (touchfile(dest, stp1));
}

static int NEAR genrand(max)
int max;
{
	static long last = -1L;
	time_t now;

	if (last < 0L) {
		now = Xtime(NULL);
		last = ((now & 0xff) << 16) + (now & ~0xff) + getpid();
	}

	do {
		last = last * (u_long)1103515245 + 12345;
	} while (last < 0L);

	return((last / 65537L) % max);
}

char *genrandname(buf, len)
char *buf;
int len;
{
	static char seq[] = {
#ifdef	PATHNOCASE
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
		'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
		'U', 'V', 'W', 'X', 'Y', 'Z', '_'
#else
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
		'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z', '_'
#endif
	};
	int i, j, c;

	if (!buf) {
		for (i = 0; i < arraysize(seq); i++) {
			j = genrand(arraysize(seq));
			c = seq[i];
			seq[i] = seq[j];
			seq[j] = c;
		}
	}
	else {
		for (i = 0; i < len; i++) {
			j = genrand(arraysize(seq));
			buf[i] = seq[j];
		}
		buf[i] = '\0';
	}

	return(buf);
}

int mktmpdir(dir)
char *dir;
{
	char *cp, path[MAXPATHLEN];
	int n, len, mask;

	if (!deftmpdir || !*deftmpdir || !dir) return(seterrno(ENOENT));
	Xrealpath(deftmpdir, path, RLP_READLINK);
	Xfree(deftmpdir);
#if	defined (DEP_DOSDRIVE) && defined (DEP_DOSLFN)
	if (checkdrive(Xtoupper(path[0]) - 'A') && !realgetwd(path)) {
		lostcwd(path);
		deftmpdir = NULL;
		return(-1);
	}
#endif
	deftmpdir = Xstrdup(path);
#if	MSDOS
	*path = (Xisupper(getcurdrv())) ? Xtoupper(*path) : Xtolower(*path);
#endif

	mask = 0777 & ~tmpumask;
	cp = strcatdelim(path);
	if (tmpfilename) {
		Xstrcpy(cp, tmpfilename);
		if (Xaccess(path, R_OK | W_OK | X_OK) < 0) {
			Xfree(tmpfilename);
			tmpfilename = NULL;
		}
	}
	if (!tmpfilename) {
		n = strsize(TMPPREFIX);
		Xstrncpy(cp, TMPPREFIX, n);
		len = strsize(path) - (cp - path);
		if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
		len -= n;
		genrandname(NULL, 0);

		for (;;) {
			genrandname(&(cp[n]), len);
			if (realmkdir(path, mask) >= 0) break;
			if (errno != EEXIST) return(-1);
		}
		tmpfilename = Xstrdup(cp);
	}

	if (!(n = strlen(dir))) {
		Xstrcpy(dir, path);
		return(0);
	}

	Xstrncpy((cp = strcatdelim(path)), dir, n);
	len = strsize(path) - (cp - path);
	if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
	len -= n;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(&(cp[n]), len);
		if (realmkdir(path, mask) >= 0) {
			Xstrcpy(dir, path);
			return(0);
		}
		if (errno != EEXIST) break;
	}

	if (cp > path) {
		*(--cp) = '\0';
		n = errno;
		realrmdir(path);
		errno = n;
	}

	return(-1);
}

int rmtmpdir(dir)
CONST char *dir;
{
	char path[MAXPATHLEN];

	if (dir && *dir && realrmdir(dir) < 0) return(-1);
	if (!deftmpdir || !*deftmpdir || !tmpfilename || !*tmpfilename)
		return(seterrno(ENOENT));
	strcatdelim2(path, deftmpdir, tmpfilename);
	if (realrmdir(path) >= 0) {
		Xfree(tmpfilename);
		tmpfilename = NULL;
	}
	else if (errno != ENOTEMPTY && errno != EEXIST && errno != EACCES)
		return(-1);

	return(0);
}

int opentmpfile(path, mode)
CONST char *path;
int mode;
{
	char *cp;
	int fd, len;

	cp = getbasename(path);
	len = MAXPATHLEN - (cp - path) - 1;
	if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(cp, len);
		fd = Xopen(path, O_BINARY | O_WRONLY | O_CREAT | O_EXCL, mode);
		if (fd >= 0) break;
		if (errno != EEXIST) return(-1);
	}

	return(fd);
}

int mktmpfile(file)
char *file;
{
	char path[MAXPATHLEN];
	int fd, duperrno;

	path[0] = '\0';
	if (mktmpdir(path) < 0) return(-1);
	VOID_C strcatdelim(path);
	if ((fd = opentmpfile(path, 0666 & ~tmpumask)) >= 0) {
		Xstrcpy(file, path);
		return(fd);
	}
	duperrno = errno;
	rmtmpdir(NULL);
	errno = duperrno;

	return(-1);
}

int rmtmpfile(file)
CONST char *file;
{
	if ((Xunlink(file) < 0 && errno != ENOENT) || rmtmpdir(NULL) < 0)
		return(-1);

	return(0);
}

static int dounlink(path)
CONST char *path;
{
	if (Xunlink(path) < 0) return(APL_ERROR);

	return(APL_OK);
}

static int dormdir(path)
CONST char *path;
{
	if (isdotdir(path)) return(APL_OK);
	if (Xrmdir(path) < 0) return(APL_ERROR);

	return(APL_OK);
}

VOID removetmp(dir, file)
char *dir;
CONST char *file;
{
#ifdef	DEP_DOSEMU
	char path[MAXPATHLEN];
#endif

	if (!dir || !*dir) /*EMPTY*/;
	else if (_chdir2(dir) < 0) {
		warning(-1, dir);
		*dir = '\0';
	}
	else if (!file) VOID_C applydir(NULL, dounlink,
		NULL, dormdir, ORD_NOPREDIR, NULL);
	else if (Xunlink(nodospath(path, file)) < 0) warning(-1, file);

	if (_chdir2(fullpath) < 0) lostcwd(fullpath);
#ifdef	DEP_PSEUDOPATH
	shutdrv(pseudodrv);
	pseudodrv = -1;
#endif
	if (dir) {
		if (*dir && rmtmpdir(dir) < 0) warning(-1, dir);
		Xfree(dir);
	}
}

int forcecleandir(dir, file)
CONST char *dir, *file;
{
#if	!MSDOS
	p_id_t pid;
#endif
	extern char **environ;
	char *argv[4], buf[MAXPATHLEN];

	VOID_C excllock(NULL, LOCK_UN);

	if (!dir || !*dir || !file || !*file) return(0);
	strcatdelim2(buf, dir, file);

	if (rawchdir(buf) < 0) return(0);
	VOID_C rawchdir(rootpath);

#if	MSDOS
	argv[0] = "DELTREE";
	argv[1] = "/Y";
#else
	argv[0] = "rm";
	argv[1] = "-rf";
#endif
	argv[2] = buf;
	argv[3] = NULL;

#if	MSDOS
	spawnve(P_WAIT, "DELTREE.EXE", argv, environ);
#else
	if ((pid = fork()) < (p_id_t)0) return(-1);
	else if (!pid) {
		execve("/bin/rm", argv, environ);
		_exit(1);
	}
#endif

	return(0);
}

#ifdef	DEP_PSEUDOPATH
char *dostmpdir(drive)
int drive;
{
	char path[MAXPATHLEN];

	path[0] = DOSTMPPREFIX;
	path[1] = drive;
	path[2] = '\0';
	if (mktmpdir(path) < 0) return(NULL);

	return(Xstrdup(path));
}

int tmpdosdupl(dir, dirp, single)
CONST char *dir;
char **dirp;
int single;
{
# ifdef	DEP_DOSEMU
	char tmp[MAXPATHLEN];
# endif
	struct stat st;
	char *cp, *tmpdir, path[MAXPATHLEN];
	int i, drive;

# ifdef	DEP_DOSDRIVE
	if ((drive = dospath2(dir))) /*EMPTY*/;
	else
# endif
# ifdef	DEP_URLPATH
	if (urlpath(dir, NULL, NULL, &drive)) drive += '0';
	else
# endif
	return(0);
	if (!(tmpdir = dostmpdir(drive))) {
		warning(-1, dir);
		return(-1);
	}

	Xstrcpy(path, tmpdir);
	cp = strcatdelim(path);
	waitmes();

	if (single || mark <= 0) {
		Xstrcpy(cp, filelist[filepos].name);
		st.st_mode = filelist[filepos].st_mode;
		st.st_atime = st.st_mtime = filelist[filepos].st_mtim;
		st.st_size = filelist[filepos].st_size;
		if (cpfile(fnodospath(tmp, filepos), path, &st, NULL) < 0) {
			warning(-1, path);
			removetmp(tmpdir, NULL);
			return(-1);
		}
	}
	else for (i = 0; i < maxfile; i++) {
		if (!ismark(&(filelist[i]))) continue;
		Xstrcpy(cp, filelist[i].name);
		st.st_mode = filelist[i].st_mode;
		st.st_atime = st.st_mtime = filelist[i].st_mtim;
		st.st_size = filelist[i].st_size;
		if (cpfile(fnodospath(tmp, i), path, &st, NULL) < 0) {
			warning(-1, path);
			removetmp(tmpdir, NULL);
			return(-1);
		}
	}

	if ((pseudodrv = preparedrv(fullpath, NULL, NULL)) < 0) {
		warning(-1, fullpath);
		removetmp(tmpdir, NULL);
		return(-1);
	}
	if (_chdir2(tmpdir) < 0) {
		warning(-1, tmpdir);
		removetmp(tmpdir, NULL);
		return(-1);
	}
	*dirp = tmpdir;

	return(drive);
}

int tmpdosrestore(drive, file)
int drive;
CONST char *file;
{
	struct stat st;
	char path[MAXPATHLEN];
# ifdef	DEP_DOSEMU
	char tmp[MAXPATHLEN];
# endif

# ifdef	DEP_DOSDRIVE
	if (isalpha(drive)) Xstrcpy(gendospath(path, drive, '\0'), file);
	else
# endif
	strcatdelim2(path, fullpath, file);
# ifdef	DEP_DOSEMU
	file = nodospath(tmp, file);
# endif
	waitmes();
	if (Xlstat(file, &st) < 0 || cpfile(file, path, &st, NULL) < 0)
		return(-1);

	return(0);
}
#endif	/* DEP_PSEUDOPATH */

#ifndef	_NOWRITEFS
static int NEAR isexist(file)
CONST char *file;
{
	struct stat st;

	if (Xlstat(file, &st) < 0 && errno == ENOENT) return(0);

	return(1);
}

static int NEAR realdirsiz(s, fat, boundary, dirsize, ofs)
CONST char *s;
int fat, boundary, dirsize, ofs;
{
	int i, len, lfn, dot;

	if (fat == FAT_NONE) {
		len = (strlen(s) + ofs + boundary) & ~(boundary - 1);
		return(len + dirsize);
	}

	if (fat > FAT_PRIMAL && !isdotdir(s)) {

		lfn = dot = 0;
		for (i = len = 0; s[i]; i++, len++) {
			if (s[i] == '.') {
				if (dot || !i || i > DOSBODYLEN) lfn = 1;
				dot = i + 1;
			}
			else if (issjis1(s[i]) && issjis2(s[i + 1])) {
				i++;
				lfn = 1;
			}
			else if (lfn) /*EMPTY*/;
			else if (Xislower(s[i])) lfn = 1;
			else if (Xstrchr(LFNONLY, s[i])) lfn = 1;
		}
		if (lfn) /*EMPTY*/;
		else if (dot) {
			if (i - dot > DOSEXTLEN) lfn = 1;
		}
		else if (i > DOSBODYLEN) lfn = 1;

		len += ofs;
		if (lfn) return((len / LFNENTSIZ + 1) * DOSDIRENT + DOSDIRENT);
	}

	return(DOSDIRENT);
}

static int NEAR getnamlen(size, fat, boundary, dirsize, ofs)
int size, fat, boundary, dirsize, ofs;
{
	if (fat == FAT_NONE) {
		size -= dirsize;
		size = (size & ~(boundary - 1)) - 1 - ofs;
		if (size > MAXNAMLEN) size = MAXNAMLEN;
	}
	else if (size <= DOSDIRENT) size = DOSBODYLEN;
	else {
		size -= DOSDIRENT;
		size = (size / DOSDIRENT) * LFNENTSIZ - 1;
		if (size > DOSMAXNAMLEN) size = DOSMAXNAMLEN;
	}

	return(size);
}

static int NEAR saferename(from, to)
CONST char *from, *to;
{
#ifdef	DEP_DOSEMU
	char fpath[MAXPATHLEN], tpath[MAXPATHLEN];
#endif

	if (!strpathcmp2(from, to)) return(0);
#ifdef	DEP_DOSEMU
	from = nodospath(fpath, from);
	to = nodospath(tpath, to);
#endif

	return(Xrename(from, to));
}

/*ARGSUSED*/
static char *NEAR maketmpfile(len, fat, tmpdir, old)
int len, fat;
CONST char *tmpdir, *old;
{
#ifndef	PATHNOCASE
	int i;
#endif
	char *fname, path[MAXPATHLEN];
	int l, fd;

	if (len < 0) return(NULL);
	fname = Xmalloc(len + 1);
	genrandname(NULL, 0);

	if (tmpdir) l = strcatdelim2(path, tmpdir, NULL) - path;
#ifdef	FAKEUNINIT
	else l = 0;	/* fake for -Wuninitialized */
#endif

	for (;;) {
		genrandname(fname, len);
#ifndef	PATHNOCASE
		if (fat != FAT_NONE) for (i = 0; fname[i]; i++)
			fname[i] = Xtoupper(fname[i]);
#endif
		if (tmpdir) {
			Xstrcpy(&(path[l]), fname);
			if (!isexist(path)) {
				if (old) {
#if	!MSDOS
					if (isexist(fname)) /*EMPTY*/;
					else
#endif
					if (saferename(old, fname) >= 0)
						return(fname);
#if	MSDOS
					else if (errno == EACCES) /*EMPTY*/;
#endif
					else break;
				}
				else {
					fd = Xopen(fname,
						O_WRONLY | O_CREAT | O_EXCL,
						0600);
					if (fd >= 0) {
						VOID_C Xclose(fd);
						return(fname);
					}
					if (errno != EEXIST) break;
				}
			}
		}
		else {
			if (Xmkdir(fname, 0777) >= 0) return(fname);
			if (errno != EEXIST) break;
		}
	}
	Xfree(fname);

	return(NULL);
}

#if	!MSDOS
static off_t NEAR getdirblocksize(dir)
CONST char *dir;
{
	struct stat st;

	if (Xlstat(dir, &st) < 0) return(getblocksize(dir));

	return((off_t)st.st_size);
}

static u_char *NEAR getentnum(dir, bsiz)
CONST char *dir;
off_t bsiz;
{
	struct stat st;
	u_char ch, *tmp;
	int i, n, fd;

	if (Xlstat(dir, &st) < 0
	|| (fd = Xopen(dir, O_BINARY | O_RDONLY, 0666)) < 0)
		return(NULL);
	n = (off_t)(st.st_size) / bsiz;
	tmp = (u_char *)Xmalloc(n + 1);

	for (i = 0; i < n; i++) {
		if (Xlseek(fd, (off_t)(i * bsiz + 3), L_SET) < 0) {
			Xfree(tmp);
			return(NULL);
		}
		if (sureread(fd, &ch, sizeof(ch)) < 0) {
			Xfree(tmp);
			return(NULL);
		}
		tmp[i] = ch + 1;
	}
	tmp[i] = 0;
	VOID_C Xclose(fd);

	return(tmp);
}
#endif	/* !MSDOS */

static VOID NEAR restorefile(dir, path, fnamp)
char *dir, *path;
int fnamp;
{
	DIR *dirp;
	struct dirent *dp;

	if (!(dirp = Xopendir(dir))) warning(-1, dir);
	else {
		while ((dp = Xreaddir(dirp))) {
			if (isdotdir(dp -> d_name)) continue;
			else {
				Xstrcpy(&(path[fnamp]), dp -> d_name);
				if (saferename(path, dp -> d_name) < 0)
					warning(-1, path);
			}
		}
		VOID_C Xclosedir(dirp);
	}
	if (Xrmdir(dir) < 0) warning(-1, dir);
	Xfree(dir);
}

VOID arrangedir(fs)
int fs;
{
#if	!MSDOS
	off_t persec, totalent, dirblocksize;
	u_char *entnum;
	char **tmpfiles;
	int n, tmpno, block, ptr, totalptr, headbyte;
#endif
#ifndef	PATHNOCASE
	int duppathignorecase;
#endif
	DIR *dirp;
	struct dirent *dp;
	int fat, boundary, dirsize, namofs;
	int i, top, size, len, fnamp, ent;
	CONST char *cp;
	char *tmp, *tmpdir, **fnamelist, path[MAXPATHLEN];

	switch (fs) {
#if	!MSDOS
		case FSID_EFS:		/* IRIX File System */
			fat = FAT_NONE;
			headbyte = 4;
			boundary = 2;
			dirsize = sizeof(u_long);
			namofs = 0;
			break;
		case FSID_SYSV:		/* SystemV R3 File System */
			fat = FAT_NONE;
			headbyte = 0;
			boundary = 8;
			dirsize = sizeof(u_short);
			namofs = 0;
			break;
		case FSID_LINUX:	/* Linux File System */
			fat = FAT_NONE;
			headbyte = 0;
			boundary = 4;
			dirsize = 4;	/* short + short */
			namofs = 3;
			break;
# ifdef	DEP_DOSDRIVE
		case FSID_DOSDRIVE:	/* Windows95 File System on DOSDRIVE */
			fat = FAT_DOSDRIVE;
			headbyte = -1;
			boundary = LFNENTSIZ;
			dirsize = DOSDIRENT;
			namofs = 0;
			break;
# endif
#endif	/* !MSDOS */
		case FSID_FAT:		/* MS-DOS File System */
			fat = FAT_PRIMAL;
#if	!MSDOS
			headbyte = -1;
#endif
			boundary = 1;
			dirsize = DOSDIRENT;
			namofs = 0;
			break;
		case FSID_LFN:		/* Windows95 File System */
			fat = FAT_LFN;
#if	!MSDOS
			headbyte = -1;
#endif
			boundary = LFNENTSIZ;
			dirsize = DOSDIRENT;
			namofs = 0;
			break;
		default:
			fat = FAT_NONE;
#if	!MSDOS
			headbyte = 0;
#endif
			boundary = 4;
			dirsize = 8;	/* long + short + short */
			namofs = 0;
			break;
	}

	top = -1;
	fnamelist = (char **)Xmalloc((maxfile + 1) * sizeof(char *));
	for (i = 0; i < maxfile; i++) {
		if (isdotdir(filelist[i].name)) cp = filelist[i].name;
		else {
			if (top < 0) top = i;
			cp = convput(path, filelist[i].name, 1, 0, NULL, NULL);
		}
#ifdef	DEP_DOSEMU
		if (_dospath(cp)) cp += 2;
#endif
		fnamelist[i] = Xstrdup(cp);
	}
	fnamelist[i] = NULL;
	if (top < 0) {
		freevar(fnamelist);
		return;
	}

#ifndef	PATHNOCASE
	duppathignorecase = pathignorecase;
	pathignorecase = (fat != FAT_NONE) ? 1 : 0;
#endif
	noconv++;
	size = realdirsiz(fnamelist[top], fat, boundary, dirsize, namofs);
	len = getnamlen(size, fat, boundary, dirsize, namofs);
	if (!(tmpdir = maketmpfile(len, fat, NULL, NULL))) {
		warning(0, NOWRT_K);
		freevar(fnamelist);
#ifndef	PATHNOCASE
		pathignorecase = duppathignorecase;
#endif
		noconv--;
		return;
	}
#if	!MSDOS
	persec = getblocksize(tmpdir);
	dirblocksize = getdirblocksize(tmpdir);
#endif
	fnamp = strcatdelim2(path, tmpdir, NULL) - path;
	waitmes();

	if (!(dirp = Xopendir(curpath))) {
		freevar(fnamelist);
#ifndef	PATHNOCASE
		pathignorecase = duppathignorecase;
#endif
		noconv--;
		lostcwd(path);
		return;
	}
	i = ent = 0;
	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;
		else if (!strpathcmp2(dp -> d_name, tmpdir)) {
#if	MSDOS
			if (!(dp -> d_alias[0])) len = DOSBODYLEN;
#else	/* !MSDOS */
# ifdef	DEP_DOSDRIVE
			if (fat == FAT_DOSDRIVE
			&& wrap_reclen(dp) == DOSDIRENT)
				len = DOSBODYLEN;
# endif
#endif	/* !MSDOS */
			ent = i;
		}
		else {
			Xstrcpy(&(path[fnamp]), dp -> d_name);
			if (saferename(dp -> d_name, path) < 0) {
				VOID_C Xclosedir(dirp);
				warning(-1, dp -> d_name);
				restorefile(tmpdir, path, fnamp);
				freevar(fnamelist);
#ifndef	PATHNOCASE
				pathignorecase = duppathignorecase;
#endif
				noconv--;
				return;
			}
		}
		i++;
	}
	VOID_C Xclosedir(dirp);

	if (ent > 0) {
		if (!(tmp = maketmpfile(len, fat, tmpdir, tmpdir))) {
			warning(-1, tmpdir);
			restorefile(tmpdir, path, fnamp);
			freevar(fnamelist);
#ifndef	PATHNOCASE
			pathignorecase = duppathignorecase;
#endif
			noconv--;
			return;
		}
		Xfree(tmpdir);
		tmpdir = tmp;
		fnamp = strcatdelim2(path, tmpdir, NULL) - path;
	}

#if	!MSDOS
	if (fs != FSID_EFS) entnum = NULL;	/* except IRIX File System */
	else if (!(entnum = getentnum(curpath, persec))) {
		warning(-1, curpath);
		restorefile(tmpdir, path, fnamp);
		freevar(fnamelist);
#ifndef	PATHNOCASE
		pathignorecase = duppathignorecase;
#endif
		noconv--;
		return;
	}
	totalent = headbyte
		+ realdirsiz(tmpdir, fat, boundary, dirsize, namofs)
		+ realdirsiz(curpath, fat, boundary, dirsize, namofs)
		+ realdirsiz(parentpath, fat, boundary, dirsize, namofs);
	block = tmpno = 0;
	ptr = 3;
	totalptr = 0;
	if (entnum) totalptr = entnum[block];
	tmpfiles = NULL;
#endif	/* !MSDOS */

	for (i = 0; i < maxfile; i++) {
		if (isdotdir(fnamelist[i]) || i == top) continue;
#if	!MSDOS
		ent = dirblocksize - totalent;
		size = realdirsiz(fnamelist[i],
			fat, boundary, dirsize, namofs);
		switch (fs) {
			case FSID_EFS:	/* IRIX File System */
				if (totalptr > ptr + 1) ent -= totalptr;
				else ent -= ptr + 1;
				break;
			case FSID_SYSV:	/* SystemV R3 File System */
			case FSID_FAT:	/* MS-DOS File System */
			case FSID_LFN:	/* Windows95 File System */
				ent = size;
				break;
			default:
				break;
		}
		if (ent < size) {
			n = getnamlen(ent, fat, boundary, dirsize, namofs);
			if (n > 0) {
				tmpfiles = b_realloc(tmpfiles, tmpno, char *);
				tmpfiles[tmpno++] =
					maketmpfile(n, fat, tmpdir, NULL);
			}
			ptr = 0;
			totalent = headbyte;
			if (entnum) totalptr = entnum[++block];
		}
#endif	/* !MSDOS */
		Xstrcpy(&(path[fnamp]), fnamelist[i]);
		if (saferename(path, fnamelist[i]) < 0) {
			warning(-1, path);
			break;
		}
#if	!MSDOS
		totalent += size;
		ptr++;
#endif
	}
#if	!MSDOS
	Xfree(entnum);
	if (tmpfiles) {
		for (i = 0; i < tmpno; i++) if (tmpfiles[i]) {
			if (Xunlink(tmpfiles[i]) < 0) warning(-1, tmpfiles[i]);
			Xfree(tmpfiles[i]);
		}
		Xfree(tmpfiles);
	}
#endif

	if (!(tmp = maketmpfile(len, fat, tmpdir, tmpdir)))
		warning(-1, tmpdir);
	else {
		Xfree(tmpdir);
		tmpdir = tmp;
	}
	fnamp = strcatdelim2(path, tmpdir, NULL) - path;
	VOID_C Xsnprintf(&(path[fnamp]), sizeof(path) - fnamp, fnamelist[top]);
	if (saferename(path, fnamelist[top]) < 0) warning(-1, path);
	restorefile(tmpdir, path, fnamp);

	freevar(fnamelist);
#ifndef	PATHNOCASE
	pathignorecase = duppathignorecase;
#endif
	noconv--;
}
#endif	/* !_NOWRITEFS */
