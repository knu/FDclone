/*
 *	file.c
 *
 *	file accessing module
 */

#include <fcntl.h>
#include "fd.h"
#include "termio.h"
#include "func.h"
#include "kanji.h"

#ifndef	ENOSPC
#define	ENOSPC	EACCES
#endif

#if	MSDOS
#include <process.h>
extern int getcurdrv __P_((VOID_A));
# ifndef	_NOUSELFN
extern char *preparefile __P_((char *, char *));
# endif
#else
#include <pwd.h>
#include <grp.h>
#endif

#ifndef	_NODOSDRIVE
extern int lastdrv;
# if	MSDOS && !defined (_NOUSELFN)
extern int checkdrive __P_((int));
# endif
#endif

extern int mark;
extern char fullpath[];
extern char *tmpfilename;
extern int physical_path;
extern int noconv;
#ifndef	_NODOSDRIVE
extern int dosdrive;
#endif

#define	MAXTMPNAMLEN	8
#ifndef	O_BINARY
#define	O_BINARY	0
#endif
#ifdef	_NODOSDRIVE
#define	DOSDIRENT	32
#define	LFNENTSIZ	13
#endif
#define	LFNONLY		" +,;=[]"
#define	DOSBODYLEN	8
#define	DOSEXTLEN	3

#ifndef	L_SET
# ifdef	SEEK_SET
# define	L_SET	SEEK_SET
# else
# define	L_SET	0
# endif
#endif	/* !L_SET */

#ifdef	_NODOSDRIVE
#define	nodoschdir	Xchdir
#define	nodosgetwd	Xgetwd
#define	nodosmkdir	Xmkdir
#define	nodosrmdir	Xrmdir
#else
static int NEAR nodoschdir __P_((char *));
static char *NEAR nodosgetwd __P_((char *));
static int NEAR nodosmkdir __P_((char *, int));
static int NEAR nodosrmdir __P_((char *));
#endif
static int NEAR cpfile __P_((char *, char *, struct stat *, struct stat *));
static VOID changemes __P_((VOID_A));
static int NEAR genrand __P_((int));
static int dounlink __P_((char *));
static int dormdir __P_((char *));
#ifndef	_NOWRITEFS
static int NEAR isexist __P_((char *));
static int NEAR realdirsiz __P_((char *, int, int, int, int));
static int NEAR getnamlen __P_((int, int, int, int, int));
static int NEAR saferename __P_((char *, char *));
static char *NEAR maketmpfile __P_((int, int, char *, char *));
#if	!MSDOS
static off_t NEAR getdirblocksize __P_((char *));
static u_char *NEAR getentnum __P_((char *, off_t));
#endif
static VOID NEAR restorefile __P_((char *, char *, int));
#endif	/* !_NOWRITEFS */

char *deftmpdir = NULL;
#if	!defined (_USEDOSCOPY) && !defined (_NOEXTRACOPY)
int inheritcopy = 0;
#endif
int tmpumask = TMPUMASK;

#ifndef	_NODOSDRIVE
static int dosdrv = -1;
#endif


#ifdef	_USEDOSEMU
char *nodospath(path, file)
char *path, *file;
{
	if (!_dospath(file)) return(file);
	path[0] = '.';
	path[1] = _SC_;
	strcpy(&(path[2]), file);

	return(path);
}
#endif	/* _USEDOSEMU */

#ifdef	NOUID
int logical_access(mode)
u_int mode;
#else
int logical_access(mode, uid, gid)
u_int mode;
uid_t uid;
gid_t gid;
#endif
{
	int dir;

	dir = ((mode & S_IFMT) == S_IFDIR);
#ifdef	NOUID
	mode >>= 6;
#else	/* !NOUID */
	if (uid == geteuid()) mode >>= 6;
	else if (gid == getegid()) mode >>= 3;
	else if (isgroupmember(gid)) mode >>= 3;
#endif	/* !NOUID */
	if (dir && !(mode & F_ISEXE)) mode &= ~(F_ISRED | F_ISWRI);

	return(mode & 007);
}

int getstatus(namep)
namelist *namep;
{
#ifdef	_USEDOSEMU
	char path[MAXPATHLEN];
#endif
	struct stat st, lst;
	char *cp;

	cp = nodospath(path, namep -> name);
	if (Xlstat(cp, &lst) < 0 || stat2(cp, &st) < 0) return(-1);
	namep -> flags = 0;
	if (s_isdir(&st)) namep -> flags |= F_ISDIR;
	if (s_islnk(&lst)) namep -> flags |= F_ISLNK;

	if (isdisplnk(dispmode))
		memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
#if	!MSDOS
	if ((lst.st_mode & S_IFMT) == S_IFCHR
	|| (lst.st_mode & S_IFMT) == S_IFBLK)
		namep -> flags |= F_ISDEV;
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
	namep -> flags |=
#ifdef	NOUID
		logical_access((u_int)(st.st_mode));
#else
		logical_access((u_int)(st.st_mode), st.st_uid, st.st_gid);
#endif
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
	char *cp1, *cp2;
	int tmp;

	if (!(((treelist *)vp1) -> name)) return(1);
	if (!(((treelist *)vp2) -> name)) return(-1);
	switch (sorton & 7) {
		case 1:
			tmp = strpathcmp2(((treelist *)vp1) -> name,
				((treelist *)vp2) -> name);
			break;
		case 2:
			if ((cp1 = strrchr(((treelist *)vp1) -> name, '.')))
				cp1++;
			else cp1 = nullstr;
			if ((cp2 = strrchr(((treelist *)vp2) -> name, '.')))
				cp2++;
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
#endif

/*ARGSUSED*/
struct dirent *searchdir(dirp, regexp, arcregstr)
DIR *dirp;
reg_t *regexp;
char *arcregstr;
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
	char *cp, cwd[MAXPATHLEN];
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
#ifndef	_NODOSDRIVE
		if (dospath2(cp)) return(-1);
#endif
		if (_chdir2(cp) < 0) return(-1);
		homedir = getwd2();
		if (_chdir2(cwd) < 0) lostcwd(cwd);
	}

	if (buf && !physical_path) strcpy(cwd, fullpath);
	len = strlen(homedir);
#if	MSDOS
	if (len <= 3) cp = NULL;
#else
	if (len <= 1) cp = NULL;
#endif
	else cp = underpath(cwd, homedir, len);

	if (buf) strcpy(buf, (cp) ? cp : nullstr);
#ifdef	DEBUG
	free(homedir);
	homedir = NULL;
#endif

	return((cp) ? 1 : 0);
}

int preparedir(dir)
char *dir;
{
	struct stat st;
	char tmp[MAXPATHLEN];
	char *cp;

	cp = dir;
#ifdef	_USEDOSPATH
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
		if (!s_isdir(&st)) {
			errno = ENOTDIR;
			return(-1);
		}
	}
	entryhist(1, realpath2(dir, tmp, 0), 1);

	return(0);
}

#ifndef	NOFLOCK
int lockfile(fd, mode)
int fd, mode;
{
	static int lockmode[] = {
# ifdef	USEFCNTLOCK
		F_RDLCK, F_WRLCK, F_UNLCK,
# else	/* !USEFCNTLOCK */
#  ifdef	USELOCKF
		F_LOCK, F_LOCK, F_ULOCK,
#  else
		LOCK_SH, LOCK_EX, LOCK_UN,
#  endif
# endif	/* !USEFCNTLOCK */
	};
# ifdef	USEFCNTLOCK
	struct flock lock;
# endif

# ifndef	_NODOSDRIVE
	if (fd >= DOSFDOFFSET) return(0);
# endif

# ifdef	USEFCNTLOCK
	lock.l_type = lockmode[mode];
	lock.l_start = lock.l_len = (off_t)0;
	lock.l_whence = SEEK_SET;
	return(fcntl(fd, F_SETLKW, &lock));
# else	/* !USEFCNTLOCK */
#  ifdef	USELOCKF
	return(lockf(fd, lockmode[mode], (off_t)0));
#  else
	return(flock(fd, lockmode[mode]));
#  endif
# endif	/* !USEFCNTLOCK */
}
#endif	/* !NOFLOCK */

int touchfile(path, stp)
char *path;
struct stat *stp;
{
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
#ifdef	FAKEUNINIT
	duperrno = errno;	/* fake for -Wuninitialized */
#endif
#if	MSDOS
	if (!(st.st_mode & S_IWRITE)) Xchmod(path, (st.st_mode | S_IWRITE));
#endif

	if (stp -> st_nlink & (TCH_ATIME | TCH_MTIME)) {
#ifdef	USEUTIME
		struct utimbuf times;
#else
		struct timeval tvp[2];
#endif

		if (!(stp -> st_nlink & TCH_ATIME))
			stp -> st_atime = st.st_atime;
		if (!(stp -> st_nlink & TCH_MTIME))
			stp -> st_mtime = st.st_mtime;
#ifdef	USEUTIME
		times.actime = stp -> st_atime;
		times.modtime = stp -> st_mtime;
		if (Xutime(path, &times) < 0) {
			duperrno = errno;
			ret = -1;
		}
#else
		tvp[0].tv_sec = stp -> st_atime;
		tvp[0].tv_usec = 0;
		tvp[1].tv_sec = stp -> st_mtime;
		tvp[1].tv_usec = 0;
		if (Xutimes(path, tvp) < 0) {
			duperrno = errno;
			ret = -1;
		}
#endif
	}

#ifdef	HAVEFLAGS
	if (stp -> st_nlink & TCH_FLAGS) {
		if (Xchflags(path, stp -> st_flags) < 0) {
			duperrno = errno;
			ret = -1;
		}
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
		if (Xchmod(path, mode) < 0) {
			duperrno = errno;
			ret = -1;
		}
	}
#if	MSDOS
	else if (!(st.st_mode & S_IWRITE)) Xchmod(path, st.st_mode);
#endif

#ifndef	NOUID
	if (stp -> st_nlink & (TCH_UID | TCH_GID)) {
		if (!(stp -> st_nlink & TCH_UID)) stp -> st_uid = (uid_t)-1;
		if (!(stp -> st_nlink & TCH_GID)) stp -> st_gid = (gid_t)-1;
		if (Xchown(path, stp -> st_uid, stp -> st_gid) >= 0) /*EMPTY*/;
		else if (!(stp -> st_nlink & TCH_CHANGE))
			Xchown(path, (uid_t)-1, stp -> st_gid);
		else {
			duperrno = errno;
			ret = -1;
		}
	}
#endif	/* !NOUID */
	if (ret < 0) errno = duperrno;

	return(ret);
}

#ifndef	_NODOSDRIVE
static int NEAR nodoschdir(path)
char *path;
{
	int n, dupdosdrive;

	dupdosdrive = dosdrive;
	dosdrive = 0;
	n = Xchdir(path);
	dosdrive = dupdosdrive;

	return(n);
}

static char *NEAR nodosgetwd(path)
char *path;
{
	char *cp;
	int dupdosdrive;

	dupdosdrive = dosdrive;
	dosdrive = 0;
	cp = Xgetwd(path);
	dosdrive = dupdosdrive;

	return(cp);
}

int nodoslstat(path, stp)
char *path;
struct stat *stp;
{
	int n, dupdosdrive;

	dupdosdrive = dosdrive;
	dosdrive = 0;
	n = Xlstat(path, stp);
	dosdrive = dupdosdrive;

	return(n);
}

static int NEAR nodosmkdir(path, mode)
char *path;
int mode;
{
	int n, dupdosdrive;

	dupdosdrive = dosdrive;
	dosdrive = 0;
	n = Xmkdir(path, mode);
	dosdrive = dupdosdrive;

	return(n);
}

static int NEAR nodosrmdir(path)
char *path;
{
	int n, dupdosdrive;

	dupdosdrive = dosdrive;
	dosdrive = 0;
	n = Xrmdir(path);
	dosdrive = dupdosdrive;

	return(n);
}
#endif	/* !_NODOSDRIVE */

VOID lostcwd(path)
char *path;
{
	char *cp, buf[MAXPATHLEN];
	int duperrno;

	duperrno = errno;
	if (!path) path = buf;

	if (path != fullpath && !nodoschdir(fullpath) && nodosgetwd(path))
		cp = NOCWD_K;
	else if ((cp = gethomedir()) && !nodoschdir(cp) && nodosgetwd(path))
		cp = GOHOM_K;
	else if (!nodoschdir(rootpath) && nodosgetwd(path)) cp = GOROT_K;
	else error(rootpath);

	if (path != fullpath) strncpy2(fullpath, path, MAXPATHLEN - 1);
	warning(0, cp);
#ifndef	_NOUSEHASH
	searchhash(NULL, nullstr, nullstr);
#endif
	errno = duperrno;
}

#ifndef	NODIRLOOP
int issamebody(src, dest)
char *src, *dest;
{
	struct stat st1, st2;

	if (Xstat(src, &st1) < 0 || Xstat(dest, &st2) < 0) return(0);

	return(st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino);
}
#endif	/* !NODIRLOOP */

#ifndef	NOSYMLINK
int cpsymlink(src, dest)
char *src, *dest;
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

	return(Xsymlink(path, dest));
}
#endif	/* !NOSYMLINK */

/*ARGSUSED*/
static int NEAR cpfile(src, dest, stp1, stp2)
char *src, *dest;
struct stat *stp1, *stp2;
{
#if	MSDOS
	struct stat st;
#endif
	char buf[BUFSIZ];
	int i, fd1, fd2, flags, mode, duperrno;

#if	MSDOS
	if (!stp2 && Xlstat(dest, &st) >= 0) stp2 = &st;
	if (stp2 && !(stp2 -> st_mode & S_IWRITE))
		Xchmod(src, stp2 -> st_mode | S_IWRITE);
#endif
#ifndef	NOSYMLINK
	if (s_islnk(stp1)) return(cpsymlink(src, dest));
#endif

	flags = (O_BINARY | O_RDONLY);
	mode = stp1 -> st_mode;
	if ((fd1 = Xopen(src, flags, mode)) < 0) return(-1);

	flags = (O_BINARY | O_WRONLY | O_CREAT | O_TRUNC);
#if	MSDOS
	mode |= S_IWRITE;
#endif
#ifndef	NODIRLOOP
	if (issamebody(src, dest)) {
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

#ifdef	FAKEUNINIT
	duperrno = errno;	/* fake for -Wuninitialized */
#endif
	for (;;) {
		if ((i = sureread(fd1, buf, BUFSIZ)) <= 0) {
			duperrno = errno;
			break;
		}
		if (surewrite(fd2, buf, i) < 0) {
			i = -1;
			duperrno = errno;
			break;
		}
		if (i < BUFSIZ) break;
	}

	VOID_C Xclose(fd2);
	VOID_C Xclose(fd1);

	if (i < 0) {
		Xunlink(dest);
		errno = duperrno;
		return(-1);
	}
	stp1 -> st_nlink = (TCH_ATIME | TCH_MTIME);
#ifdef	_USEDOSCOPY
	if (touchfile(dest, stp1) < 0) return(-1);
#else
# ifndef	_NOEXTRACOPY
	if (inheritcopy && touchfile(dest, stp1) < 0) return(-1);
# endif
#endif

	return(0);
}

static VOID changemes(VOID_A)
{
	warning(0, CHGFD_K);
}

int safecpfile(src, dest, stp1, stp2)
char *src, *dest;
struct stat *stp1, *stp2;
{
#ifndef	_NODOSDRIVE
	int drive;
#endif

	for (;;) {
		if (cpfile(src, dest, stp1, stp2) >= 0) break;
		if (errno && errno != ENOSPC) return(-1);
		for (;;) {
			if (!yesno(NOSPC_K)) {
				errno = ENOSPC;
				return(-1);
			}
#ifndef	_NODOSDRIVE
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
char *src, *dest;
struct stat *stp1, *stp2;
{
	if (Xrename(src, dest) >= 0) return(0);
	if (errno != EXDEV || s_isdir(stp1)) return(-1);
	if (safecpfile(src, dest, stp1, stp2) < 0 || Xunlink(src) < 0)
		return(-1);

	stp1 -> st_nlink = (TCH_MODE | TCH_UID | TCH_GID
		| TCH_ATIME | TCH_MTIME);
	return (touchfile(dest, stp1));
}

static int NEAR genrand(max)
int max;
{
	static long last = -1L;
	time_t now;

	if (last < 0L) {
		now = time2();
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

	if (!deftmpdir || !*deftmpdir || !dir) {
		errno = ENOENT;
		return(-1);
	}
	realpath2(deftmpdir, path, 1);
	free(deftmpdir);
#if	MSDOS && !defined (_NOUSELFN) && !defined (_NODOSDRIVE)
	if (checkdrive(toupper2(path[0]) - 'A') && !nodosgetwd(path)) {
		lostcwd(path);
		deftmpdir = NULL;
		return(-1);
	}
#endif
	deftmpdir = strdup2(path);
#if	MSDOS
	n = getcurdrv();
	*path = (isupper2(n)) ? toupper2(*path) : tolower2(*path);
#endif

	mask = 0777 & ~tmpumask;
	cp = strcatdelim(path);
	if (tmpfilename) {
		strcpy(cp, tmpfilename);
		if (Xaccess(path, R_OK | W_OK | X_OK) < 0) {
			free(tmpfilename);
			tmpfilename = NULL;
		}
	}
	if (!tmpfilename) {
		n = strsize(TMPPREFIX);
		strncpy(cp, TMPPREFIX, n);
		len = strsize(path) - (cp - path);
		if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
		len -= n;
		genrandname(NULL, 0);

		for (;;) {
			genrandname(cp + n, len);
			if (nodosmkdir(path, mask) >= 0) break;
			if (errno != EEXIST) return(-1);
		}
		tmpfilename = strdup2(cp);
	}

	if (!(n = strlen(dir))) {
		strcpy(dir, path);
		return(0);
	}

	strncpy((cp = strcatdelim(path)), dir, n);
	len = strsize(path) - (cp - path);
	if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
	len -= n;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(cp + n, len);
		if (nodosmkdir(path, mask) >= 0) {
			strcpy(dir, path);
			return(0);
		}
		if (errno != EEXIST) break;
	}

	if (cp > path) {
		*(--cp) = '\0';
		n = errno;
		nodosrmdir(path);
		errno = n;
	}

	return(-1);
}

int rmtmpdir(dir)
char *dir;
{
	char path[MAXPATHLEN];

	if (dir && *dir && nodosrmdir(dir) < 0) return(-1);
	if (!deftmpdir || !*deftmpdir || !tmpfilename || !*tmpfilename) {
		errno = ENOENT;
		return(-1);
	}
	strcatdelim2(path, deftmpdir, tmpfilename);
	if (nodosrmdir(path) >= 0) {
		free(tmpfilename);
		tmpfilename = NULL;
	}
	else if (errno != ENOTEMPTY && errno != EEXIST && errno != EACCES)
		return(-1);

	return(0);
}

int mktmpfile(file)
char *file;
{
	char *cp, path[MAXPATHLEN];
	int fd, len, duperrno;

	path[0] = '\0';
	if (mktmpdir(path) < 0) return(-1);
	cp = strcatdelim(path);
	len = strsize(path) - (cp - path);
	if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(cp, len);
		fd = Xopen(path, O_BINARY | O_WRONLY | O_CREAT | O_EXCL,
			0666 & ~tmpumask);
		if (fd >= 0) {
			strcpy(file, path);
			return(fd);
		}
		if (errno != EEXIST) break;
	}
	duperrno = errno;
	rmtmpdir(NULL);
	errno = duperrno;

	return(-1);
}

int rmtmpfile(file)
char *file;
{
	if ((Xunlink(file) < 0 && errno != ENOENT) || rmtmpdir(NULL) < 0)
		return(-1);

	return(0);
}

static int dounlink(path)
char *path;
{
	if (Xunlink(path) < 0) return(APL_ERROR);

	return(APL_OK);
}

static int dormdir(path)
char *path;
{
	if (isdotdir(path)) return(APL_OK);
	if (Xrmdir(path) < 0) return(APL_ERROR);

	return(APL_OK);
}

VOID removetmp(dir, file)
char *dir, *file;
{
#ifdef	_USEDOSEMU
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

#ifndef	_NODOSDRIVE
	if (dosdrv >= 0) {
		if (lastdrv >= 0) {
# if	MSDOS
			if (_chdir2(deftmpdir) < 0);
			else
# endif
			shutdrv(lastdrv);
		}
		lastdrv = dosdrv;
		dosdrv = -1;
	}
#endif	/* !_NODOSDRIVE */

	if (_chdir2(fullpath) < 0) lostcwd(fullpath);
	if (dir) {
		if (*dir && rmtmpdir(dir) < 0) warning(-1, dir);
		free(dir);
	}
}

int forcecleandir(dir, file)
char *dir, *file;
{
#if	!MSDOS
	p_id_t pid;
#endif
	extern char **environ;
	char buf[MAXPATHLEN];

	if (!dir || !*dir || !file || !*file) return(0);
	strcatdelim2(buf, dir, file);

	if (rawchdir(buf) != 0) return(0);
	rawchdir(rootpath);
#if	MSDOS
	spawnlpe(P_WAIT, "DELTREE.EXE", "DELTREE", "/Y", buf, NULL, environ);
#else
	if ((pid = fork()) < (p_id_t)0) return(-1);
	else if (!pid) {
		execle("/bin/rm", "rm", "-rf", buf, NULL, environ);
		_exit(1);
	}
#endif

	return(0);
}

#ifndef	_NODOSDRIVE
char *dostmpdir(drive)
int drive;
{
	char path[MAXPATHLEN];

	path[0] = DOSTMPPREFIX;
	path[1] = drive;
	path[2] = '\0';
	if (mktmpdir(path) < 0) return(NULL);

	return(strdup2(path));
}

int tmpdosdupl(dir, dirp, single)
char *dir, **dirp;
int single;
{
	struct stat st;
	char *cp, *tmpdir, path[MAXPATHLEN];
# if	!MSDOS
	char tmp[MAXPATHLEN];
# endif
	int i, drive;

	if (!(drive = dospath2(dir))) return(0);
	if (!(tmpdir = dostmpdir(drive))) {
		warning(-1, dir);
		return(-1);
	}

	strcpy(path, tmpdir);
	cp = strcatdelim(path);
	waitmes();

	if (single || mark <= 0) {
		strcpy(cp, filelist[filepos].name);
		st.st_mode = filelist[filepos].st_mode;
		st.st_atime = st.st_mtime = filelist[filepos].st_mtim;
		if (cpfile(fnodospath(tmp, filepos), path, &st, NULL) < 0) {
			warning(-1, path);
			removetmp(tmpdir, NULL);
			return(-1);
		}
	}
	else for (i = 0; i < maxfile; i++) {
		if (!ismark(&(filelist[i]))) continue;
		strcpy(cp, filelist[i].name);
		st.st_mode = filelist[i].st_mode;
		st.st_atime = st.st_mtime = filelist[i].st_mtim;
		if (cpfile(fnodospath(tmp, i), path, &st, NULL) < 0) {
			warning(-1, path);
			removetmp(tmpdir, NULL);
			return(-1);
		}
	}

	dosdrv = lastdrv;
	lastdrv = -1;
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
char *file;
{
	struct stat st;
	char path[MAXPATHLEN];
# if	!MSDOS
	char tmp[MAXPATHLEN];
# endif

	strcpy(gendospath(path, drive, '\0'), file);
# if	!MSDOS
	file = nodospath(tmp, file);
# endif
	waitmes();
	if (Xlstat(file, &st) < 0 || cpfile(file, path, &st, NULL) < 0)
		return(-1);

	return(0);
}
#endif	/* !_NODOSDRIVE */

#ifndef	_NOWRITEFS
static int NEAR isexist(file)
char *file;
{
	struct stat st;

	if (Xlstat(file, &st) < 0 && errno == ENOENT) return(0);

	return(1);
}

static int NEAR realdirsiz(s, dos, boundary, dirsize, ofs)
char *s;
int dos, boundary, dirsize, ofs;
{
	int i, len, lfn, dot;

	if (!dos) {
		len = (strlen(s) + ofs + boundary) & ~(boundary - 1);
		return(len + dirsize);
	}

	if (dos > 1 && !isdotdir(s)) {

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
			else if (!lfn && strchr(LFNONLY, s[i])) lfn = 1;
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

static int NEAR getnamlen(size, dos, boundary, dirsize, ofs)
int size, dos, boundary, dirsize, ofs;
{
	if (!dos) {
		size -= dirsize;
		return((size & ~(boundary - 1)) - 1 - ofs);
	}

	if (size > DOSDIRENT) {
		size -= DOSDIRENT;
		return((size / DOSDIRENT) * LFNENTSIZ - 1);
	}

	return(DOSBODYLEN);
}

static int NEAR saferename(from, to)
char *from, *to;
{
#ifdef	_USEDOSEMU
	char fpath[MAXPATHLEN], tpath[MAXPATHLEN];
#endif

	if (!strpathcmp(from, to)) return(0);
#ifdef	_USEDOSEMU
	from = nodospath(fpath, from);
	to = nodospath(tpath, to);
#endif

	return(Xrename(from, to));
}

/*ARGSUSED*/
static char *NEAR maketmpfile(len, dos, tmpdir, old)
int len, dos;
char *tmpdir, *old;
{
	char *fname, path[MAXPATHLEN];
	int l, fd;

	if (len < 0) return(NULL);
	fname = malloc2(len + 1);
	genrandname(NULL, 0);

	if (tmpdir) l = strcatdelim2(path, tmpdir, NULL) - path;
#ifdef	FAKEUNINIT
	else l = 0;	/* fake for -Wuninitialized */
#endif

	for (;;) {
		genrandname(fname, len);
#ifndef	PATHNOCASE
		if (dos) {
			int i;

			for (i = 0; fname[i]; i++)
				fname[i] = toupper2(fname[i]);
		}
#endif
		if (tmpdir) {
			strcpy(&(path[l]), fname);
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
						O_CREAT | O_EXCL, 0600);
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
	free(fname);

	return(NULL);
}

#if	!MSDOS
static off_t NEAR getdirblocksize(dir)
char *dir;
{
	struct stat st;

	if (Xlstat(dir, &st) < 0) return(getblocksize(dir));

	return((off_t)st.st_size);
}

static u_char *NEAR getentnum(dir, bsiz)
char *dir;
off_t bsiz;
{
	struct stat st;
	u_char ch, *tmp;
	int i, n, fd;

	if (Xlstat(dir, &st) < 0
	|| (fd = Xopen(dir, O_BINARY | O_RDONLY, 0666)) < 0)
		return(NULL);
	n = (off_t)(st.st_size) / bsiz;
	tmp = (u_char *)malloc2(n + 1);

	for (i = 0; i < n; i++) {
		if (Xlseek(fd, (off_t)(i * bsiz + 3), L_SET) < 0) {
			free(tmp);
			return(NULL);
		}
		if (sureread(fd, &ch, sizeof(ch)) < 0) {
			free(tmp);
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
				strcpy(&(path[fnamp]), dp -> d_name);
				if (saferename(path, dp -> d_name) < 0)
					warning(-1, path);
			}
		}
		Xclosedir(dirp);
	}
	if (Xrmdir(dir) < 0) warning(-1, dir);
	free(dir);
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
	DIR *dirp;
	struct dirent *dp;
	int dos, boundary, dirsize, namofs;
	int i, top, size, len, fnamp, ent;
	char *cp, *tmpdir, **fnamelist, path[MAXPATHLEN];

	switch (fs) {
#if	!MSDOS
		case FSID_EFS:		/* IRIX File System */
			dos = 0;
			headbyte = 4;
			boundary = 2;
			dirsize = sizeof(u_long);
			namofs = 0;
			break;
		case FSID_SYSV:		/* SystemV R3 File System */
			dos = 0;
			headbyte = 0;
			boundary = 8;
			dirsize = sizeof(u_short);
			namofs = 0;
			break;
		case FSID_LINUX:	/* Linux File System */
			dos = 0;
			headbyte = 0;
			boundary = 4;
			dirsize = 4;	/* short + short */
			namofs = 3;
			break;
# ifndef	_NODOSDRIVE
		case FSID_DOSDRIVE:	/* Windows95 File System on DOSDRIVE */
			dos = 3;
			headbyte = -1;
			boundary = LFNENTSIZ;
			dirsize = DOSDIRENT;
			namofs = 0;
			break;
# endif
#endif	/* !MSDOS */
		case FSID_FAT:		/* MS-DOS File System */
			dos = 1;
#if	!MSDOS
			headbyte = -1;
#endif
			boundary = 1;
			dirsize = DOSDIRENT;
			namofs = 0;
			break;
		case FSID_LFN:		/* Windows95 File System */
			dos = 2;
#if	!MSDOS
			headbyte = -1;
#endif
			boundary = LFNENTSIZ;
			dirsize = DOSDIRENT;
			namofs = 0;
			break;
		default:
			dos = 0;
#if	!MSDOS
			headbyte = 0;
#endif
			boundary = 4;
			dirsize = 8;	/* long + short + short */
			namofs = 0;
			break;
	}

	top = -1;
	fnamelist = (char **)malloc2((maxfile + 1) * sizeof(char *));
	for (i = 0; i < maxfile; i++) {
		if (isdotdir(filelist[i].name)) cp = filelist[i].name;
		else {
			if (top < 0) top = i;
			cp = convput(path, filelist[i].name, 1, 0, NULL, NULL);
		}
#ifdef	_USEDOSEMU
		if (_dospath(cp)) cp += 2;
#endif
		fnamelist[i] = strdup2(cp);
	}
	fnamelist[i] = NULL;
	if (top < 0) {
		freevar(fnamelist);
		return;
	}

	noconv++;
	size = realdirsiz(fnamelist[top], dos, boundary, dirsize, namofs);
	len = getnamlen(size, dos, boundary, dirsize, namofs);
	if (!(tmpdir = maketmpfile(len, dos, NULL, NULL))) {
		warning(0, NOWRT_K);
		freevar(fnamelist);
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
		noconv--;
		lostcwd(path);
		return;
	}
	i = ent = 0;
	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;
		else if (!strpathcmp(dp -> d_name, tmpdir)) {
#if	MSDOS
			if (!(dp -> d_alias[0])) len = DOSBODYLEN;
#else
# ifndef	_NODOSDRIVE
			if (dos == 3 && dp -> d_reclen == DOSDIRENT)
				len = DOSBODYLEN;
# endif
#endif
			ent = i;
		}
		else {
			strcpy(&(path[fnamp]), dp -> d_name);
			if (saferename(dp -> d_name, path) < 0) {
				Xclosedir(dirp);
				warning(-1, dp -> d_name);
				restorefile(tmpdir, path, fnamp);
				freevar(fnamelist);
				noconv--;
				return;
			}
		}
		i++;
	}
	Xclosedir(dirp);

	if (ent > 0) {
		if (!(cp = maketmpfile(len, dos, tmpdir, tmpdir))) {
			warning(-1, tmpdir);
			restorefile(tmpdir, path, fnamp);
			freevar(fnamelist);
			noconv--;
			return;
		}
		free(tmpdir);
		tmpdir = cp;
		fnamp = strcatdelim2(path, tmpdir, NULL) - path;
	}

#if	!MSDOS
	if (fs != FSID_EFS) entnum = NULL;	/* except IRIX File System */
	else if (!(entnum = getentnum(curpath, persec))) {
		warning(-1, curpath);
		restorefile(tmpdir, path, fnamp);
		freevar(fnamelist);
		noconv--;
		return;
	}
	totalent = headbyte
		+ realdirsiz(tmpdir, dos, boundary, dirsize, namofs)
		+ realdirsiz(curpath, dos, boundary, dirsize, namofs)
		+ realdirsiz(parentpath, dos, boundary, dirsize, namofs);
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
			dos, boundary, dirsize, namofs);
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
			n = getnamlen(ent, dos, boundary, dirsize, namofs);
			if (n > 0) {
				tmpfiles = b_realloc(tmpfiles, tmpno, char *);
				tmpfiles[tmpno++] =
					maketmpfile(n, dos, tmpdir, NULL);
			}
			ptr = 0;
			totalent = headbyte;
			if (entnum) totalptr = entnum[++block];
		}
#endif	/* !MSDOS */
		strcpy(&(path[fnamp]), fnamelist[i]);
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
	if (entnum) free(entnum);
	if (tmpfiles) {
		for (i = 0; i < tmpno; i++) if (tmpfiles[i]) {
			if (Xunlink(tmpfiles[i]) < 0) warning(-1, tmpfiles[i]);
			free(tmpfiles[i]);
		}
		free(tmpfiles);
	}
#endif

	if (!(cp = maketmpfile(len, dos, tmpdir, tmpdir))) warning(-1, tmpdir);
	else {
		free(tmpdir);
		tmpdir = cp;
	}
	fnamp = strcatdelim2(path, tmpdir, NULL) - path;
	snprintf2(&(path[fnamp]), sizeof(path) - fnamp, fnamelist[top]);
	if (saferename(path, fnamelist[top]) < 0) warning(-1, path);
	restorefile(tmpdir, path, fnamp);

	freevar(fnamelist);
	noconv--;
}
#endif	/* !_NOWRITEFS */
