/*
 *	file.c
 *
 *	File Access Module
 */

#include <fcntl.h>
#include "fd.h"
#include "func.h"
#include "kctype.h"
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
extern int flushdrv __P_((int, VOID_T (*)__P_((VOID_A))));
extern int shutdrv __P_((int));
extern int lastdrv;
# if	MSDOS
extern int checkdrive __P_((int));
# endif
#endif

extern int mark;
extern off_t marksize;
extern off_t totalsize;
extern off_t blocksize;
extern char fullpath[];
extern char *tmpfilename;
extern int physical_path;

#define	MAXTMPNAMLEN		8
#ifndef	O_BINARY
#define	O_BINARY		0
#endif

#define	realdirsiz(name)	((dos) ? ((k_strlen(name) / LFNENTSIZ + 1) \
					* DOSDIRENT + DOSDIRENT) \
				:	(((strlen(name) + boundary) \
					& ~(boundary - 1)) + dirsize))
#define	getnamlen(ent)		((dos) ? ((((ent) - DOSDIRENT) \
					/ DOSDIRENT) * LFNENTSIZ - 1) \
				: 	((((ent) - dirsize) \
					& ~(boundary - 1)) - 1))

static int NEAR cpfile __P_((char *, char *, struct stat *, struct stat *));
static VOID changemes __P_((VOID_A));
static int NEAR genrand __P_((int));
static int dounlink __P_((char *));
#ifndef	_NOWRITEFS
static int NEAR isexist __P_((char *));
static int NEAR k_strlen __P_((char *));
static int NEAR saferename __P_((char *, char *));
static char *NEAR maketmpfile __P_((int, char *, char *));
#if	!MSDOS
static char *NEAR getentnum __P_((char *, long, int));
#endif
static VOID NEAR restorefile __P_((char *, char *, int));
#endif	/* !_NOWRITEFS */

char *deftmpdir = NULL;
#if	!MSDOS && !defined (_NOEXTRACOPY)
int inheritcopy = 0;
#endif

#ifndef	_NODOSDRIVE
static int dosdrv = -1;
#endif


#if	!MSDOS && !defined (_NODOSDRIVE)
char *nodospath(path, file)
char *path, *file;
{
	if (!_dospath(file)) return(file);
	path[0] = '.';
	path[1] = _SC_;
	strcpy(&(path[2]), file);
	return(path);
}
#endif

#if	MSDOS
int logical_access(mode)
u_short mode;
#else
int logical_access(mode, uid, gid)
u_short mode;
uid_t uid;
gid_t gid;
#endif
{
#if	!MSDOS
	struct group *gp;
	uidtable *up;
	uid_t euid;
	int i;
#endif
	int dir;

	dir = ((mode & S_IFMT) == S_IFDIR);
#if	MSDOS
	mode >>= 6;
#else
	euid = geteuid();
	if (uid == euid) mode >>= 6;
	else if (gid == getegid()) mode >>= 3;
	else if ((up = finduid(euid, NULL))) {
# ifdef	DEBUG
		_mtrace_file = "setgrent(start)";
		setgrent();
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "setgrent(end)";
			malloc(0);	/* dummy alloc */
		}
		for (;;) {
			_mtrace_file = "getgrent(start)";
			gp = getgrent();
			if (_mtrace_file) _mtrace_file = NULL;
			else {
				_mtrace_file = "getgrent(end)";
				malloc(0);	/* dummy alloc */
			}
			if (!gp) break;
			if (gid != gp -> gr_gid) continue;
			for (i = 0; gp -> gr_mem[i]; i++) {
				if (!strcmp(up -> name, gp -> gr_mem[i])) {
					mode >>= 3;
					break;
				}
			}
			break;
		}
		_mtrace_file = "endgrent(start)";
		endgrent();
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "endgrent(end)";
			malloc(0);	/* dummy alloc */
		}
# else	/* !DEBUG */
		setgrent();
		while ((gp = getgrent())) {
			if (gid != gp -> gr_gid) continue;
			for (i = 0; gp -> gr_mem[i]; i++) {
				if (!strcmp(up -> name, gp -> gr_mem[i])) {
					mode >>= 3;
					break;
				}
			}
			break;
		}
		endgrent();
# endif	/* !DEBUG */
	}
#endif
	if (dir && !(mode & F_ISEXE)) mode &= ~(F_ISRED | F_ISWRI);
	return(mode & 007);
}

int getstatus(list)
namelist *list;
{
#if	!MSDOS
# ifndef	_NODOSDRIVE
	char path[MAXPATHLEN];
# endif
	char *cp;
#endif
	struct stat st, lst;

#if	MSDOS
	if (stat2(list -> name, &st) < 0) return(-1);
	memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
#else	/* !MSDOS */
	cp = nodospath(path, list -> name);
# ifndef	_NODOSDRIVE
	if (dospath(cp, NULL)) {
		if (stat2(cp, &st) < 0) return(-1);
		memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
	}
	else
# endif
	if (Xlstat(cp, &lst) < 0 || stat2(cp, &st) < 0) return(-1);
#endif	/* !MSDOS */
	list -> flags = 0;
	if ((st.st_mode & S_IFMT) == S_IFDIR) list -> flags |= F_ISDIR;
	if ((lst.st_mode & S_IFMT) == S_IFLNK) list -> flags |= F_ISLNK;

	if (isdisplnk(dispmode))
		memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
#if	!MSDOS
	if ((lst.st_mode & S_IFMT) == S_IFCHR
	|| (lst.st_mode & S_IFMT) == S_IFBLK) list -> flags |= F_ISDEV;
#endif

	list -> st_mode = lst.st_mode;
	list -> st_nlink = lst.st_nlink;
#if	MSDOS
	list -> st_size = lst.st_size;
#else
	list -> st_uid = lst.st_uid;
	list -> st_gid = lst.st_gid;
	list -> st_size = isdev(list) ? lst.st_rdev : lst.st_size;
#endif
#ifdef	HAVEFLAGS
	list -> st_flags = lst.st_flags;
#endif
	list -> st_mtim = lst.st_mtime;
	list -> flags |=
#if	MSDOS
		logical_access((u_short)(st.st_mode));
#else
		logical_access((u_short)(st.st_mode), st.st_uid, st.st_gid);
#endif

	if (isfile(list) && list -> st_size) {
		totalsize += getblock(list -> st_size);
		if (ismark(list)) marksize += getblock(list -> st_size);
	}
	list -> tmpflags |= F_STAT;
	return(0);
}

int cmplist(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	namelist *list1, *list2;
	char *cp1, *cp2;
	long tmp;

	list1 = (namelist *)vp1;
	list2 = (namelist *)vp2;
	if (!sorton) return(list1 -> ent - list2 -> ent);

#ifndef	_NOPRECEDE
	if (!havestat(list1) && getstatus(list1) < 0) return(1);
	if (!havestat(list2) && getstatus(list2) < 0) return(-1);
#endif

	if (!isdir(list1) && isdir(list2)) return(1);
	if (isdir(list1) && !isdir(list2)) return(-1);

	if (list1 -> name[0] == '.' && list1 -> name[1] == '\0') return(-1);
	if (list2 -> name[0] == '.' && list2 -> name[1] == '\0') return(1);
	if (list1 -> name[0] == '.' && list1 -> name[1] == '.'
	&& list1 -> name[2] == '\0') return(-1);
	if (list2 -> name[0] == '.' && list2 -> name[1] == '.'
	&& list2 -> name[2] == '\0') return(1);

	switch (sorton & 7) {
		case 5:
			tmp = (int)strlen(list1 -> name)
				- (int)strlen(list2 -> name);
			if (tmp != 0) break;
/*FALLTHRU*/
		case 1:
			tmp = strpathcmp2(list1 -> name, list2 -> name);
			break;
		case 2:
			if (isdir(list1)) {
				tmp = strpathcmp2(list1 -> name,
					list2 -> name);
				break;
			}
			cp1 = list1 -> name + strlen(list1 -> name);
			cp2 = list2 -> name + strlen(list2 -> name);
			for (;;) {
				while (cp1 > list1 -> name)
					if (*(--cp1) == '.') break;
				while (cp2 > list2 -> name)
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
			if (isdir(list1))
				tmp = strpathcmp2(list1 -> name,
					list2 -> name);
			else tmp = (long)(list1 -> st_size)
				- (long)(list2 -> st_size);
			break;
		case 4:
			tmp = list1 -> st_mtim - list2 -> st_mtim;
			break;
		default:
			tmp = 0;
			break;
	}

	if (sorton > 7) tmp = -tmp;
	if (!tmp) tmp = list1 -> ent - list2 -> ent;

	if (tmp > 0) return(1);
	if (tmp < 0) return(-1);
	return(0);
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
			else cp1 = "";
			if ((cp2 = strrchr(((treelist *)vp2) -> name, '.')))
				cp2++;
			else cp2 = "";
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
		if (stat2(dp -> d_name, &st) < 0
		|| (st.st_mode & S_IFMT) == S_IFDIR) continue;

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
		if (
#ifndef	_NODOSDRIVE
		dospath2(cp) ||
#endif
		_chdir2(cp) < 0) return(-1);
		homedir = getwd2();
		if (_chdir2(cwd) < 0) error(cwd);
	}

	if (buf && !physical_path) strcpy(cwd, fullpath);
	len = strlen(homedir);
#if	MSDOS
	if (len <= 3 || strnpathcmp(cwd, homedir, len)) {
#else
	if (len <= 1 || strnpathcmp(cwd, homedir, len)) {
#endif
		cp = NULL;
		if (buf) *buf = '\0';
	}
	else {
		cp = cwd + len;
		if (buf) strcpy(buf, cp);
	}
#ifdef	DEBUG
	free(homedir);
	homedir = NULL;
#endif
	return(cp ? 1 : 0);
}

int preparedir(dir)
char *dir;
{
	struct stat st;
	char tmp[MAXPATHLEN];
	char *cp;

	cp = dir;
#if	MSDOS || !defined (_NODOSDRIVE)
	if (_dospath(dir)) cp += 2;
#endif
	if (!isdotdir(cp)) {
		if (stat2(dir, &st) < 0) {
			if (errno != ENOENT) return(-1);
			if (Xmkdir(dir, 0777) < 0) {
#if	MSDOS
				if (errno != EEXIST) return(-1);
				if (cp[0] != _SC_ || cp[1]) {
					if ((cp = strrdelim(dir, 1))) cp++;
					else cp = dir;
					if (cp[0] != '.' || cp[1]) return(-1);
				}
				st.st_mode = S_IFDIR;
#else
				return(-1);
#endif
			}
			else if (stat2(dir, &st) < 0) return(-1);
		}
		if ((st.st_mode & S_IFMT) != S_IFDIR) {
			errno = ENOTDIR;
			return(-1);
		}
	}

	entryhist(1, realpath2(dir, tmp, 0), 1);
	return(0);
}

int touchfile(path, stp)
char *path;
struct stat *stp;
{
	struct stat st, dummy;
	int ret, duperrno;

	if (Xlstat(path, &st) < 0) return(-1);
	if ((st.st_mode & S_IFMT) == S_IFLNK) return(1);

	ret = 0;
#if	MSDOS
	if (!(st.st_mode & S_IWRITE)) Xchmod(path, (st.st_mode | S_IWRITE));
#endif
	if (stp -> st_atime != (time_t)-1 && stp -> st_mtime != (time_t)-1) {
#ifdef	USEUTIME
		struct utimbuf times;

		times.actime = stp -> st_atime;
		times.modtime = stp -> st_mtime;
		if (Xutime(path, &times) < 0) {
			duperrno = errno;
			ret = -1;
		}
#else
		struct timeval tvp[2];

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
	if (stp -> st_flags != 0xffffffff) {
# ifndef	_NODOSDRIVE
		if (dospath(path, NULL)) {
			duperrno = EACCES;
			ret = -1;
		}
		else
# endif
		if (chflags(path, stp -> st_flags) < 0) {
			duperrno = errno;
			ret = -1;
		}
	}
#endif
	dummy.st_mode = 0xffff;
	if (stp -> st_mode != dummy.st_mode) {
		stp -> st_mode &= ~S_IFMT;
		stp -> st_mode |= (st.st_mode & S_IFMT);
		if (Xchmod(path, stp -> st_mode) < 0) {
			duperrno = errno;
			ret = -1;
		}
	}
#if	MSDOS
	else if (!(st.st_mode & S_IWRITE)) Xchmod(path, st.st_mode);
#else
	if (stp -> st_gid != (gid_t)-1) {
# ifndef	_NODOSDRIVE
		if (dospath(path, NULL));
		else
# endif
		if (chown(path, stp -> st_uid, stp -> st_gid) < 0)
			chown(path, -1, stp -> st_gid);
	}
#endif
	if (ret < 0) errno = duperrno;
	return(ret);
}

VOID lostcwd(path)
char *path;
{
	char *cp, buf[MAXPATHLEN];
	int duperrno;

	duperrno = errno;
	if (!path) path = buf;

	if (path != fullpath && !rawchdir(fullpath) && _Xgetwd(path, 1))
		cp = NOCWD_K;
	else if ((cp = gethomedir()) && !rawchdir(cp) && _Xgetwd(path, 1))
		cp = GOHOM_K;
	else if (!rawchdir(_SS_) && _Xgetwd(path, 1)) cp = GOROT_K;
	else error(_SS_);

	if (path != fullpath) strncpy2(fullpath, path, MAXPATHLEN - 1);
	warning(0, cp);
	errno = duperrno;
}

int safewrite(fd, buf, size)
int fd;
char *buf;
int size;
{
	int n;

	n = Xwrite(fd, buf, size);
#if	MSDOS
	if (n >= 0 && n < size) {
		n = -1;
		errno = ENOSPC;
	}
#else
	if (n < 0 && errno == EINTR) n = size;
#endif
	return(n);
}

/*ARGSUSED*/
static int NEAR cpfile(src, dest, stp1, stp2)
char *src, *dest;
struct stat *stp1, *stp2;
{
#if	MSDOS
	struct stat st;
#endif
	char buf[BUFSIZ];
	int i, fd1, fd2, tmperrno;

#if	MSDOS
	if (!stp2 && Xlstat(dest, &st) >= 0) stp2 = &st;
	if (stp2 && !(stp2 -> st_mode & S_IWRITE))
		Xchmod(src, stp2 -> st_mode | S_IWRITE);
#endif
	if ((stp1 -> st_mode & S_IFMT) == S_IFLNK) {
		if ((i = Xreadlink(src, buf, BUFSIZ - 1)) < 0) return(-1);
		buf[i] = '\0';
		return(Xsymlink(buf, dest));
	}
	if ((fd1 = Xopen(src, O_BINARY | O_RDONLY, stp1 -> st_mode)) < 0)
		return(-1);
#if	MSDOS
	fd2 = Xopen(dest,
		O_BINARY | O_WRONLY | O_CREAT | O_TRUNC,
		stp1 -> st_mode | S_IWRITE);
#else
	fd2 = Xopen(dest,
		O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, stp1 -> st_mode);
#endif
	if (fd2 < 0) {
		Xclose(fd1);
		return(-1);
	}

	for (;;) {
		while ((i = Xread(fd1, buf, BUFSIZ)) < 0 && errno == EINTR);
		tmperrno = errno;
		if (i < BUFSIZ) break;
		if ((i = safewrite(fd2, buf, BUFSIZ)) < 0) {
			tmperrno = errno;
			break;
		}
	}
	if (i > 0 && (i = safewrite(fd2, buf, i)) < 0) tmperrno = errno;

	Xclose(fd2);
	Xclose(fd1);

	if (i < 0) {
		Xunlink(dest);
		errno = tmperrno;
		return(-1);
	}
#ifdef	HAVEFLAGS
	stp1 -> st_flags = 0xffffffff;
#endif
#if	MSDOS
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
		if (cpfile(src, dest, stp1, stp2) >= 0) return(0);
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
}

int safemvfile(src, dest, stp1, stp2)
char *src, *dest;
struct stat *stp1, *stp2;
{
	if (Xrename(src, dest) >= 0) return(0);
	if (errno != EXDEV || (stp1 -> st_mode & S_IFMT) == S_IFDIR)
		return(-1);
	if (safecpfile(src, dest, stp1, stp2) < 0 || Xunlink(src) < 0)
		return(-1);
#ifdef	HAVEFLAGS
	stp1 -> st_flags = 0xffffffff;
#endif
	return (touchfile(dest, stp1));
}

static int NEAR genrand(max)
int max;
{
	static long last = -1;
	time_t now;

	if (last < 0) {
		now = time2();
		last = ((now & 0xff) << 16) + (now & ~0xff) + getpid();
	}

	do {
		last = last * (u_long)1103515245 + 12345;
	} while (last < 0);

	return((last / 65537) % max);
}

char *genrandname(buf, len)
char *buf;
int len;
{
	static char seq[] = {
#if	MSDOS
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
		for (i = 0; i < sizeof(seq) / sizeof(char); i++) {
			j = genrand(sizeof(seq) / sizeof(char));
			c = seq[i];
			seq[i] = seq[j];
			seq[j] = c;
		}
	}
	else {
		for (i = 0; i < len; i++) {
			j = genrand(sizeof(seq) / sizeof(char));
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
	int n, len;

	if (!deftmpdir || !*deftmpdir || !dir) {
		errno = ENOENT;
		return(-1);
	}
	realpath2(deftmpdir, path, 1);
	free(deftmpdir);
#if	MSDOS && !defined (_NODOSDRIVE)
	if (checkdrive(toupper2(path[0]) - 'A'))
		if (!_Xgetwd(path, 1)) {
			lostcwd(path);
			deftmpdir = NULL;
			return(-1);
		}
#endif
	deftmpdir = strdup2(path);
#if	MSDOS
	n = getcurdrv();
	*path = (n >= 'A' && n <= 'Z') ? toupper2(*path) : tolower2(*path);
#endif

	cp = strcatdelim(path);
	if (tmpfilename) strcpy(cp, tmpfilename);
	else {
		n = sizeof(TMPPREFIX) - 1;
		strncpy(cp, TMPPREFIX, n);
		len = sizeof(path) - 1 - (cp - path);
		if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
		len -= n;
		genrandname(NULL, 0);

		for (;;) {
			genrandname(cp + n, len);
			if (_Xmkdir(path, 0755, 0, 1) >= 0) break;
			if (errno != EEXIST) return(-1);
		}
		tmpfilename = strdup2(cp);
	}

	if (!(n = strlen(dir))) {
		strcpy(dir, path);
		return(0);
	}

	strncpy((cp = strcatdelim(path)), dir, n);
	len = sizeof(path) - 1 - (cp - path);
	if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
	len -= n;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(cp + n, len);
		if (_Xmkdir(path, 0755, 0, 1) >= 0) {
			strcpy(dir, path);
			return(0);
		}
		if (errno != EEXIST) break;
	}

	if (cp > path) {
		*(--cp) = '\0';
		n = errno;
		if (Xrmdir(path) < 0
		&& errno != ENOTEMPTY && errno != EEXIST && errno != EACCES)
			error(path);
		errno = n;
	}
	return(-1);
}

int rmtmpdir(dir)
char *dir;
{
	char path[MAXPATHLEN];

	if (dir && *dir && _Xrmdir(dir, 0, 1) < 0) return(-1);
	if (!deftmpdir || !*deftmpdir || !tmpfilename || !*tmpfilename) {
		errno = ENOENT;
		return(-1);
	}
	strcatdelim2(path, deftmpdir, tmpfilename);
	if (_Xrmdir(path, 0, 1) >= 0) {
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
	int fd, len;

	*path = '\0';
	if (mktmpdir(path) < 0) return(-1);
	cp = strcatdelim(path);
	len = sizeof(path) - 1 - (cp - path);
	if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(cp, len);
		fd = Xopen(path, O_BINARY | O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd >= 0) {
			strcpy(file, path);
			return(fd);
		}
		if (errno != EEXIST) break;
	}
	rmtmpdir(NULL);
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
	return(Xunlink(path));
}

static int dormdir(path)
char *path;
{
	if (isdotdir(path)) return(0);
	return(Xrmdir(path));
}

VOID removetmp(dir, subdir, file)
char *dir, *subdir, *file;
{
#if	!MSDOS && !defined (_NODOSDRIVE)
	char path[MAXPATHLEN];
#endif
	char *cp, *tmp, *dupdir;

	if (!dir || !*dir) subdir = file = NULL;
	else if (_chdir2(dir) < 0) {
		warning(-1, dir);
		*dir = '\0';
		subdir = file = NULL;
	}
	if (subdir) {
		while (*subdir == _SC_) subdir++;
		if (*subdir && _chdir2(subdir) < 0) {
			warning(-1, subdir);
			subdir = file = NULL;
		}
	}
	if (file) {
		if (*file) {
			if (Xunlink(nodospath(path, file)) < 0) {
				warning(-1, file);
				subdir = NULL;
			}
		}
		else if (applydir(NULL, dounlink, NULL, dormdir, 3, NULL) < 0)
			subdir = NULL;
	}
	if (subdir && *subdir) {
		if (_chdir2(dir) < 0) error(dir);
		dupdir = strdup2(subdir);
		cp = dupdir + strlen(dupdir);
		for (;;) {
			*cp = '\0';
			if ((tmp = strrdelim(dupdir, 1))) tmp++;
			else tmp = dupdir;
			if ((*tmp && strcmp(tmp, ".")
			&& _Xrmdir(dupdir, 0, 1) < 0)
			|| tmp == dupdir) break;
			cp = tmp - 1;
		}
		free(dupdir);
	}
#if	MSDOS && !defined (_NODOSDRIVE)
	if (_chdir2(deftmpdir) < 0) error(deftmpdir);
#endif
#ifndef	_NODOSDRIVE
	if (dosdrv >= 0) {
		if (lastdrv >= 0) shutdrv(lastdrv);
		lastdrv = dosdrv;
		dosdrv = -1;
	}
#endif
	if (_chdir2(fullpath) < 0) lostcwd(fullpath);
	if (dir) {
		if (rmtmpdir(dir) < 0) warning(-1, dir);
		free(dir);
	}
}

int forcecleandir(dir, file)
char *dir, *file;
{
#if	!MSDOS
	long pid;
#endif
	extern char **environ;
	char buf[MAXPATHLEN];

	if (!dir || !*dir || !file || !*file) return(0);
	strcatdelim2(buf, dir, file);

	if (rawchdir(buf) != 0) return(0);
	rawchdir(_SS_);
#if	MSDOS
	spawnlpe(P_WAIT, "DELTREE.EXE", "DELTREE", "/Y", buf, NULL, environ);
#else
	if ((pid = fork()) < 0) return(-1);
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
			removetmp(tmpdir, NULL, NULL);
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
			removetmp(tmpdir, NULL, "");
			return(-1);
		}
	}

	dosdrv = lastdrv;
	lastdrv = -1;
	if (_chdir2(tmpdir) < 0) error(tmpdir);
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

	path[0] = drive;
	path[1] = ':';
	strcpy(&(path[2]), file);
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

	if (_Xlstat(file, &st, 1, 0) < 0 && errno == ENOENT) return(0);
	return(1);
}

static int NEAR k_strlen(s)
char *s;
{
	int i;

	for (i = 0; s[i]; i++) if (iskanji1(s, i)) i++;
	return(i);
}

static int NEAR saferename(from, to)
char *from, *to;
{
#if	!MSDOS && !defined (_NODOSDRIVE)
	char fpath[MAXPATHLEN], tpath[MAXPATHLEN];
#endif

	if (!strpathcmp(from, to)) return(0);
#if	!MSDOS && !defined (_NODOSDRIVE)
	from = nodospath(fpath, from);
	to = nodospath(tpath, to);
#endif
	return(_Xrename(from, to, 1));
}

static char *NEAR maketmpfile(len, tmpdir, old)
int len;
char *tmpdir, *old;
{
	char *fname, path[MAXPATHLEN];
	int l, fd;

	if (len < 0) return(NULL);
	fname = malloc2(len + 1);
	genrandname(NULL, 0);

	if (tmpdir) l = strcatdelim2(path, tmpdir, NULL) - path;

	for (;;) {
		genrandname(fname, len);
		if (tmpdir) {
			strcpy(&(path[l]), fname);
			if (!isexist(path)) {
				if (old) {
#if	!MSDOS
					if (isexist(fname));
					else
#endif
					if (saferename(old, fname) >= 0)
						return(fname);
#if	MSDOS
					else if (errno == EACCES);
#endif
					else break;
				}
				else {
					if ((fd = _Xopen(fname,
					O_CREAT | O_EXCL, 0600, 1))) {
						Xclose(fd);
						return(fname);
					}
					if (errno != EEXIST) break;
				}
			}
		}
		else {
			if (_Xmkdir(fname, 0777, 1, 0) >= 0) return(fname);
			if (errno != EEXIST) break;
		}
	}
	free(fname);
	return(NULL);
}

#if	!MSDOS
static char *NEAR getentnum(dir, bsiz, fs)
char *dir;
long bsiz;
int fs;
{
	FILE *fp;
	struct stat st;
	char *tmp;
	int i, n;

	if (fs != 2) return(NULL);	/* without IRIX File System */

	if (_Xlstat(dir, &st, 1, 0) < 0) error(dir);
	n = (long)(st.st_size) / bsiz;
	tmp = malloc2(n + 1);

	fp = fopen(dir, "r");
	for (i = 0; i < n; i++) {
		if (fseek(fp, i * bsiz + 3, 0) < 0) error(dir);
		tmp[i] = fgetc(fp) + 1;
	}
	tmp[i] = 0;
	fclose(fp);

	return(tmp);
}
#endif

static VOID NEAR restorefile(dir, path, fnamp)
char *dir, *path;
int fnamp;
{
	DIR *dirp;
	struct dirent *dp;

	if (!(dirp = _Xopendir(dir, 1))) error(dir);
	while ((dp = _Xreaddir(dirp, 1))) {
		if (isdotdir(dp -> d_name)) continue;
		else {
			strcpy(&(path[fnamp]), dp -> d_name);
			if (saferename(path, dp -> d_name) < 0) error(path);
		}
	}
	Xclosedir(dirp);
	if (_Xrmdir(dir, 1, 0) < 0) error(dir);
	free(dir);
}

VOID arrangedir(fs)
int fs;
{
	DIR *dirp;
	struct dirent *dp;
	int dos, headbyte, boundary, dirsize;
	int i, top, len, fnamp, ent;
	char *cp, *tmpdir, path[MAXPATHLEN];
#if	!MSDOS
# ifndef	_NOKANJIFCONV
	char conv[MAXNAMLEN + 1];
# endif
	long persec;
	char *entnum, **tmpfile;
	int tmpno, block, totalent, ptr, totalptr;
#endif

	switch (fs) {
#if	!MSDOS
		case 2:	/* IRIX File System */
			dos = 0;
			headbyte = 4;
			boundary = 2;
			dirsize = sizeof(u_long);
			break;
		case 3:	/* SystemV R3 File System */
			dos = 0;
			headbyte = 0;
			boundary = 8;
			dirsize = sizeof(u_short);
			break;
#endif
		case 4:	/* MS-DOS File System */
			dos = 1;
			headbyte = -1;
			boundary = 1;
			dirsize = DOSDIRENT;
			break;
		case 5:	/* Windows95 File System */
			dos = 2;
			headbyte = -1;
			boundary = LFNENTSIZ;
			dirsize = DOSDIRENT;
			break;
		default:
			dos = 0;
			headbyte = 0;
			boundary = 4;
			dirsize = sizeof(u_long) + sizeof(u_short) * 2;
			break;
	}

	for (i = 0; i < maxfile; i++) if (!isdotdir(filelist[i].name)) break;
	if (i >= maxfile) return;
	top = i;
	if (dos == 1) len = 8;
	else {
		cp = filelist[top].name;
#if	!MSDOS && !defined (_NOKANJIFCONV)
		cp = kanjiconv2(conv, cp, MAXNAMLEN, DEFCODE, getkcode(cp));
#endif
		len = getnamlen(realdirsiz(cp));
	}
	if (!(tmpdir = maketmpfile(len, NULL, NULL))) {
		warning(0, NOWRT_K);
		return;
	}
#if	!MSDOS
	persec = getblocksize(tmpdir);
#endif
	fnamp = strcatdelim2(path, tmpdir, NULL) - path;
	waitmes();

	if (!(dirp = _Xopendir(".", 1))) {
		lostcwd(path);
		return;
	}
	i = ent = 0;
	while ((dp = _Xreaddir(dirp, 1))) {
		if (isdotdir(dp -> d_name)) continue;
		else if (!strpathcmp(dp -> d_name, tmpdir)) {
#if	MSDOS
			if (!(dp -> d_alias[0])) len = 8;
#else
			if (dos == 2 && dp -> d_reclen == DOSDIRENT) len = 8;
#endif
			ent = i;
		}
		else {
			strcpy(&(path[fnamp]), dp -> d_name);
			if (saferename(dp -> d_name, path) < 0) {
				Xclosedir(dirp);
				warning(-1, dp -> d_name);
				restorefile(tmpdir, path, fnamp);
				return;
			}
		}
		i++;
	}
	Xclosedir(dirp);

	if (ent > 0) {
		if (!(cp = maketmpfile(len, tmpdir, tmpdir))) error(tmpdir);
		free(tmpdir);
		tmpdir = cp;
		strcatdelim2(path, tmpdir, NULL);
	}

#if	!MSDOS
	entnum = getentnum(".", persec, fs);
	totalent = headbyte + realdirsiz(".") + realdirsiz("..")
		+ realdirsiz(tmpdir);
	block = tmpno = 0;
	ptr = 3;
	totalptr = 0;
	if (entnum) totalptr = entnum[block];
	tmpfile = NULL;
#endif

	for (i = 0; i < maxfile; i++) {
		if (isdotdir(filelist[i].name) || i == top) continue;
		cp = filelist[i].name;
#if	!MSDOS
		ent = persec - totalent;
# ifndef	_NOKANJIFCONV
		cp = kanjiconv2(conv, cp, MAXNAMLEN, DEFCODE, getkcode(cp));
# endif
		switch (fs) {
			case 2:	/* IRIX File System */
 				if (totalptr > ptr + 1) ent -= totalptr;
 				else ent -= ptr + 1;
				break;
			case 3:	/* SystemV R3 File System */
			case 4:	/* MS-DOS File System */
			case 5:	/* Windows95 File System */
				ent = realdirsiz(cp);
				break;
			default:
				break;
		}
		if (ent < realdirsiz(cp)) {
			tmpfile = b_realloc(tmpfile, tmpno, char *);
			tmpfile[tmpno] =
				maketmpfile(getnamlen(ent), tmpdir, NULL);
			tmpno++;
			ptr = 0;
			totalent = headbyte;
			if (entnum) totalptr = entnum[++block];
		}
#endif	/* !MSDOS */
		strcpy(&(path[fnamp]), cp);
		if (saferename(path, cp) < 0) error(path);
#if	!MSDOS
		totalent += realdirsiz(cp);
		ptr++;
#endif
	}
#if	!MSDOS
	if (entnum) free(entnum);
#endif

	if (!(cp = maketmpfile(len, tmpdir, tmpdir))) error(tmpdir);
	free(tmpdir);
	tmpdir = cp;
	cp = filelist[top].name;
#if	!MSDOS && !defined (_NOKANJIFCONV)
	cp = kanjiconv2(conv, cp, MAXNAMLEN, DEFCODE, getkcode(cp));
#endif
	strcatdelim2(path, tmpdir, cp);
	if (saferename(path, cp) < 0) error(path);
	restorefile(tmpdir, path, fnamp);

#if	!MSDOS
	for (i = 0; i < tmpno; i++) if (tmpfile[i]) {
		if (_Xunlink(tmpfile[i], 1) < 0) error(tmpfile[i]);
		free(tmpfile[i]);
	}
	free(tmpfile);
#endif
}
#endif	/* !_NOWRITEFS */
