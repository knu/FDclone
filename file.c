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
extern char *preparefile __P_((char *, char *, int));
# endif
#else
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/param.h>
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

#define	DOSDIRENT		32
#define	LFNENTSIZ		13
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
#ifndef	_NOWRITEFS
static int NEAR k_strlen __P_((char *));
static char *NEAR maketmpfile __P_((int, int, char *, char *));
#if	!MSDOS
static char *NEAR getentnum __P_((char *, int, int));
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
	struct stat st, lst;

#if	MSDOS
	if (stat2(list -> name, &st) < 0) return(-1);
	memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
#else	/* !MSDOS */
# ifndef	_NODOSDRIVE
	if (dospath(list -> name, NULL)) {
		if (stat2(list -> name, &st) < 0) return(-1);
		memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
	}
	else
# endif
	if (Xlstat(list -> name, &lst) < 0
	|| stat2(list -> name, &st) < 0) return(-1);
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

int isdotdir(name)
char *name;
{
	if (name[0] == '.'
	&& (!name[1] || (name[1] == '.' && !name[2]))) return(1);
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
#if	MSDOS
	char *cp;
#endif

	if (stat2(dir, &st) < 0) {
		if (errno != ENOENT) return(-1);
		if (Xmkdir(dir, 0777) < 0) {
#if	MSDOS
			if (errno != EEXIST) return(-1);
			cp = (_dospath(dir)) ? dir + 2 : dir;
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
#ifdef HAVEFLAGS
	if (stp -> st_flags != 0xffffffff) {
# ifndef	_NODOSDRIVE
		if (dospath(path, NULL)) {
			errno = EACCES;
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

	if (path != fullpath && !rawchdir(fullpath) && _Xgetwd(path))
		cp = NOCWD_K;
	else {
		if ((cp = gethomedir()) && !rawchdir(cp) && _Xgetwd(path))
			cp = GOHOM_K;
		else if (!rawchdir(_SS_) && _Xgetwd(path)) cp = GOROT_K;
		else error(_SS_);
		strncpy2(fullpath, path, MAXPATHLEN - 1);
	}

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
	if (n < size) n = 0;
#else
	if (n < 0) {
		if (errno == EINTR) n = size;
		else if (errno == ENOSPC) n = 0;
	}
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
	int i, n, fd1, fd2, tmperrno;

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
	if ((fd1 = Xopen(src, O_RDONLY | O_BINARY, stp1 -> st_mode)) < 0)
		return(-1);
	if ((fd2 = Xopen(dest, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
#if	MSDOS
	stp1 -> st_mode | S_IWRITE)) < 0) {
#else
	stp1 -> st_mode)) < 0) {
#endif
		Xclose(fd1);
		return(-1);
	}

	for (;;) {
		while ((i = Xread(fd1, buf, BUFSIZ)) < 0 && errno == EINTR);
		tmperrno = errno;
		if (i < BUFSIZ) break;
		if ((n = safewrite(fd2, buf, BUFSIZ)) <= 0) {
			tmperrno = (n < 0) ? errno : ENOSPC;
			i = -1;
			break;
		}
	}
	if (i > 0 && (n = safewrite(fd2, buf, i)) <= 0) {
		tmperrno = (n < 0) ? errno : ENOSPC;
		i = -1;
	}

	Xclose(fd2);
	Xclose(fd1);

	if (i < 0) {
		Xunlink(dest);
		errno = tmperrno;
		return(-1);
	}
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
	if (safecpfile(src, dest, stp1, stp2) < 0 || Xunlink(src) < 0
	|| touchfile(dest, stp1) < 0) return(-1);
	return(0);
}

int mktmpdir(dir)
char *dir;
{
	char *cp, path[MAXPATHLEN];
	int no;

	if (!deftmpdir || !*deftmpdir || !tmpfilename || !*tmpfilename
	|| !dir || !*dir) {
		errno = ENOENT;
		return(-1);
	}
	realpath2(deftmpdir, path, 1);
	free(deftmpdir);
#if	MSDOS && !defined (_NODOSDRIVE)
	if (checkdrive(toupper2(path[0]) - 'A'))
		if (!_Xgetwd(path)) {
			lostcwd(path);
			deftmpdir = NULL;
			return(-1);
		}
#endif
	deftmpdir = strdup2(path);
#if	MSDOS
	no = getcurdrv();
	*path = (no >= 'A' && no <= 'Z') ? toupper2(*path) : tolower2(*path);
#endif
	strcpy(strcatdelim(path), tmpfilename);
	if (_Xmkdir(path, 0777) < 0 && errno != EEXIST) return(-1);
	strcpy((cp = strcatdelim(path)), dir);
	if (_Xmkdir(path, 0777) < 0 && errno != EEXIST) {
		*(--cp) = '\0';
		no = errno;
		if (_Xrmdir(path) < 0
		&& errno != ENOTEMPTY && errno != EEXIST && errno != EACCES)
			error(path);
		errno = no;
		return(-1);
	}
	strcpy(dir, path);
	return(0);
}

int rmtmpdir(dir)
char *dir;
{
	char path[MAXPATHLEN];

	if (dir && *dir && _Xrmdir(dir) < 0) return(-1);
	if (!deftmpdir || !*deftmpdir || !tmpfilename || !*tmpfilename) {
		errno = ENOENT;
		return(-1);
	}
	strcatdelim2(path, deftmpdir, tmpfilename);
	if (_Xrmdir(path) < 0
	&& errno != ENOTEMPTY && errno != EEXIST && errno != EACCES)
		return(-1);
	return(0);
}

VOID removetmp(dir, subdir, file)
char *dir, *subdir, *file;
{
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
			if (Xunlink(file) < 0) {
				warning(-1, file);
				subdir = NULL;
			}
		}
		else if (applydir(".", Xunlink, NULL, NULL, 0, NULL) < 0) {
			warning(-1, dir);
			subdir = NULL;
		}
	}
	if (subdir && *subdir) {
		if (_chdir2(dir) < 0) error(dir);
		dupdir = strdup2(subdir);
		cp = dupdir + strlen(dupdir);
		for (;;) {
			*cp = '\0';
			if ((tmp = strrdelim(dupdir, 1))) tmp++;
			else tmp = dupdir;
			if ((*tmp && strcmp(tmp, ".") && _Xrmdir(dupdir) < 0)
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
int tmpdosdupl(dir, dirp, single)
char *dir, **dirp;
int single;
{
	struct stat st;
	char *cp, path[MAXPATHLEN];
	int i, drive;

	if (!(drive = dospath2(dir))) return(0);
	cp = path;
	*(cp++) = 'D';
	*(cp++) = drive;
	*cp = '\0';
	if (mktmpdir(path) < 0) {
		warning(-1, path);
		return(-1);
	}

	cp = strcatdelim(path);
	waitmes();

	if (single || mark <= 0) {
		strcpy(cp, filelist[filepos].name);
		st.st_mode = filelist[filepos].st_mode;
		st.st_atime = st.st_mtime = filelist[filepos].st_mtim;
		if (cpfile(filelist[filepos].name, path, &st, NULL) < 0) {
			*(--cp) = '\0';
			removetmp(path, NULL, NULL);
			return(-1);
		}
	}
	else for (i = 0; i < maxfile; i++) {
		if (!ismark(&(filelist[i]))) continue;
		strcpy(cp, filelist[i].name);
		st.st_mode = filelist[i].st_mode;
		st.st_atime = st.st_mtime = filelist[i].st_mtim;
		if (cpfile(filelist[i].name, path, &st, NULL) < 0) {
			*(--cp) = '\0';
			removetmp(path, NULL, "");
			return(-1);
		}
	}

	*(--cp) = '\0';
	dosdrv = lastdrv;
	lastdrv = -1;
	if (_chdir2(path) < 0) error(path);
	*dirp = strdup2(path);
	return(drive);
}

int tmpdosrestore(drive, file)
int drive;
char *file;
{
	struct stat st;
	char path[MAXPATHLEN];

	path[0] = drive;
	path[1] = ':';
	strcpy(path + 2, file);
	waitmes();
	if (Xlstat(file, &st) < 0 || cpfile(file, path, &st, NULL) < 0)
		return(-1);

	return(0);
}
#endif	/* !_NODOSDRIVE */

int isexist(file)
char *file;
{
#if	MSDOS && !defined (_NOUSELFN)
	char *cp, buf[MAXPATHLEN];

	if (!(cp = preparefile(file, buf, 0)) && errno == ENOENT) return(0);
	if (cp != file) return(1);
#endif
	if (Xaccess(file, F_OK) < 0 && errno == ENOENT) return(0);
	return(1);
}

#ifndef	_NOWRITEFS
static int NEAR k_strlen(s)
char *s;
{
	int i;

	for (i = 0; s[i]; i++) if (iskanji1(s, i)) i++;
	return(i);
}

static char *NEAR maketmpfile(len, dos, tmpdir, old)
int len, dos;
char *tmpdir, *old;
{
	char *fname, path[MAXPATHLEN];
	int i, l, fd;

	if (len < 0) return(NULL);
	fname = malloc2(len + 1);

	memset(fname, '_', len);
	fname[len] = '\0';

	if (tmpdir) l = strcatdelim2(path, tmpdir, NULL) - path;

	for (;;) {
		if (tmpdir) {
			strcpy(path + l, fname);
			if (!isexist(path)) {
				if (old) {
#if	!MSDOS
					if (isexist(fname));
					else
#endif
					{
						if (rename2(old, fname) >= 0)
							return(fname);
#if	MSDOS
						if (errno != EACCES)
#endif
						break;
					}
				}
				else {
					if ((fd = Xopen(fname,
					O_CREAT | O_EXCL, 0600))) {
						Xclose(fd);
						return(fname);
					}
					if (errno != EEXIST) break;
				}
			}
		}
		else {
			if (_Xmkdir(fname, 0777) >= 0) return(fname);
			if (errno != EEXIST) break;
		}

		for (i = len - 1; i >= 0; i--) {
			if (fname[i] == '9') {
				fname[i] = '_';
				continue;
			}
			else if (fname[i] == '_') fname[i] = 'a';
			else if (dos) {
				if (fname[i] == 'z') fname[i] = '0';
				else fname[i]++;
			}
			else {
				if (fname[i] == 'z') fname[i] = 'A';
				else if (fname[i] == 'Z') fname[i] = '0';
				else fname[i]++;
			}
			break;
		}
		if (i < 0) break;
	}
	free(fname);
	return(NULL);
}

#if	!MSDOS
static char *NEAR getentnum(dir, bsiz, fs)
char *dir;
int bsiz, fs;
{
	FILE *fp;
	struct stat st;
	char *tmp;
	int i, n;

	if (fs != 2) return(NULL);	/* without IRIX File System */

	if (_Xlstat(dir, &st) < 0) error(dir);
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

	if (!(dirp = _Xopendir(dir))) error(dir);
	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;
		else {
			strcpy(path + fnamp, dp -> d_name);
			if (rename2(path, dp -> d_name) < 0) error(path);
		}
	}
	Xclosedir(dirp);
	if (_Xrmdir(dir) < 0) error(dir);
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
	else len = getnamlen(realdirsiz(filelist[top].name));
	if (!(tmpdir = maketmpfile(len, dos, NULL, NULL))) {
		warning(0, NOWRT_K);
		return;
	}
#if	!MSDOS
	persec = getblocksize(tmpdir);
#endif
	fnamp = strcatdelim2(path, tmpdir, NULL) - path;
	waitmes();

	if (!(dirp = _Xopendir("."))) {
		lostcwd(path);
		return;
	}
	i = ent = 0;
	while ((dp = Xreaddir(dirp))) {
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
			strcpy(path + fnamp, dp -> d_name);
			if (rename2(dp -> d_name, path) < 0) {
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
		if (!(cp = maketmpfile(len, dos, tmpdir, tmpdir)))
			error(tmpdir);
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
#if	!MSDOS
		ent = persec - totalent;
		switch (fs) {
			case 2:	/* IRIX File System */
 				if (totalptr > ptr + 1) ent -= totalptr;
 				else ent -= ptr + 1;
				break;
			case 3:	/* SystemV R3 File System */
			case 4:	/* MS-DOS File System */
			case 5:	/* Windows95 File System */
				ent = realdirsiz(filelist[i].name);
				break;
			default:
				break;
		}
		if (ent < realdirsiz(filelist[i].name)) {
			tmpfile = b_realloc(tmpfile, tmpno, char *);
			tmpfile[tmpno] =
				maketmpfile(getnamlen(ent), dos, tmpdir, NULL);
			tmpno++;
			ptr = 0;
			totalent = headbyte;
			if (entnum) totalptr = entnum[++block];
		}
#endif	/* !MSDOS */
		strcpy(path + fnamp, filelist[i].name);
		if (rename2(path, filelist[i].name) < 0) error(path);
#if	!MSDOS
		totalent += realdirsiz(filelist[i].name);
		ptr++;
#endif
	}
#if	!MSDOS
	if (entnum) free(entnum);
#endif

	if (!(cp = maketmpfile(len, dos, tmpdir, tmpdir))) error(tmpdir);
	free(tmpdir);
	tmpdir = cp;
	strcatdelim2(path, tmpdir, filelist[top].name);
	if (rename2(path, filelist[top].name) < 0) error(path);
	restorefile(tmpdir, path, fnamp);

#if	!MSDOS
	for (i = 0; i < tmpno; i++) if (tmpfile[i]) {
		if (unlink(tmpfile[i]) < 0) error(tmpfile[i]);
		free(tmpfile[i]);
	}
	free(tmpfile);
#endif
}
#endif	/* !_NOWRITEFS */
