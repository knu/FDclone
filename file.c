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
#if	MSDOS && !defined (_NODOSDRIVE)
extern int checkdrive __P_((int));
#endif

extern int filepos;
extern int mark;
extern int dispmode;
extern int sorton;
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

static int safewrite __P_((int, char *, int));
#ifndef	_NOWRITEFS
static int k_strlen __P_((char *));
static int nofile __P_((char *));
static char *maketmpfile __P_((int, int, char *, char *));
#if	!MSDOS
static char *getentnum __P_((char *, int, int));
#endif
static VOID restorefile __P_((char *, char *, int));
#endif	/* !_NOWRITEFS */

char *deftmpdir = NULL;


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
	uid_t euid;
	char *name;
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
	else if ((name = getpwuid2(euid))) {
		setgrent();
		while ((gp = getgrent())) {
			if (gid != gp -> gr_gid) continue;
			for (i = 0; gp -> gr_mem[i]; i++) {
				if (!strcmp(name, gp -> gr_mem[i])) {
					mode >>= 3;
					break;
				}
			}
			break;
		}
		endgrent();
	}
#endif
	if (dir && !(mode & F_ISEXE)) mode &= ~F_ISRED;
	return(mode & 007);
}

int getstatus(list)
namelist *list;
{
	struct stat st, lst;

#if	MSDOS
	if (stat2(list -> name, &st) < 0) return(-1);
	memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
#else
# ifndef	_NODOSDRIVE
	if (dospath(list -> name, NULL)) {
		if (stat2(list -> name, &st) < 0) return(-1);
		memcpy((char *)&lst, (char *)&st, sizeof(struct stat));
	}
	else
# endif
	if (Xlstat(list -> name, &lst) < 0
	|| stat2(list -> name, &st) < 0) return(-1);
#endif
	list -> st_mode = lst.st_mode;
	list -> flags = 0;
	if ((st.st_mode & S_IFMT) == S_IFDIR) list -> flags |= F_ISDIR;
	if ((lst.st_mode & S_IFMT) == S_IFLNK) list -> flags |= F_ISLNK;
#if	!MSDOS
	if ((lst.st_mode & S_IFMT) == S_IFCHR
	|| (lst.st_mode & S_IFMT) == S_IFBLK) list -> flags |= F_ISDEV;
#endif

	if (isdisplnk(dispmode))
		memcpy((char *)&lst, (char *)&st, sizeof(struct stat));

	list -> st_nlink = lst.st_nlink;
#if	!MSDOS
	list -> st_uid = lst.st_uid;
	list -> st_gid = lst.st_gid;
#endif
#ifdef	HAVEFLAGS
	list -> st_flags = lst.st_flags;
#endif
	list -> st_size = isdev(list) ? lst.st_rdev : lst.st_size;
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
			tmp = strlen(list1 -> name) - strlen(list2 -> name);
			if (tmp != 0) break;
		case 1:
			tmp = strpathcmp(list1 -> name, list2 -> name);
			break;
		case 2:
			if (isdir(list1)) {
				tmp = strpathcmp(list1 -> name, list2 -> name);
				break;
			}
			if ((cp1 = strrchr(list1 -> name, '.'))) cp1++;
			else cp1 = "";
			if ((cp2 = strrchr(list2 -> name, '.'))) cp2++;
			else cp2 = "";
			tmp = strpathcmp(cp1, cp2);
			break;
		case 3:
			if (isdir(list1))
				tmp = strpathcmp(list1 -> name, list2 -> name);
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
			tmp = strpathcmp(((treelist *)vp1) -> name,
				((treelist *)vp2) -> name);
			break;
		case 2:
			if ((cp1 = strrchr(((treelist *)vp1) -> name, '.')))
				cp1++;
			else cp1 = "";
			if ((cp2 = strrchr(((treelist *)vp2) -> name, '.')))
				cp2++;
			else cp2 = "";
			tmp = strpathcmp(cp1, cp2);
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
	char *cp, *cwd;
	int len;
#if	!MSDOS
	struct passwd *pwd;
#endif

#if	MSDOS
	if (!buf) return(1);
#endif
	cwd = getwd2();
	if (!homedir) {
		if (!(homedir = (char *)getenv("HOME"))) {
#if	MSDOS
			homedir = "";
#else
			if (!(pwd = getpwuid(getuid()))) {
				free(cwd);
				return(-1);
			}
			homedir = pwd -> pw_dir;
#endif
		}
		if (
#ifndef	_NODOSDRIVE
		dospath2(homedir) ||
#endif
		_chdir2(homedir) < 0) {
			homedir = NULL;
			free(cwd);
			return(-1);
		}
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
	free(cwd);
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

int touchfile(path, atime, mtime)
char *path;
time_t atime, mtime;
{
#ifdef	USEUTIME
	struct utimbuf times;

	times.actime = atime;
	times.modtime = mtime;
	return(Xutime(path, &times));
#else
	struct timeval tvp[2];

	tvp[0].tv_sec = atime;
	tvp[0].tv_usec = 0;
	tvp[1].tv_sec = mtime;
	tvp[1].tv_usec = 0;
	return(Xutimes(path, tvp));
#endif
}

static int safewrite(fd, buf, size)
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

int cpfile(src, dest, stp)
char *src, *dest;
struct stat *stp;
{
	char buf[BUFSIZ];
	int i, n, fd1, fd2, tmperrno;

	if ((stp -> st_mode & S_IFMT) == S_IFLNK) {
		if ((i = Xreadlink(src, buf, BUFSIZ)) < 0) return(-1);
		buf[i] = '\0';
		return(Xsymlink(buf, dest));
	}
	if ((fd1 = Xopen(src, O_RDONLY | O_BINARY, stp -> st_mode)) < 0)
		return(-1);
	if ((fd2 = Xopen(dest, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
#if	MSDOS
	stp -> st_mode | S_IWRITE)) < 0) {
#else
	stp -> st_mode)) < 0) {
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
	if (touchfile(dest, stp -> st_atime, stp -> st_mtime) < 0)
		return(-1);
	if (!(stp -> st_mode & S_IWRITE)) Xchmod(dest, stp -> st_mode);
#endif
	return(0);
}

int mvfile(src, dest, stp)
char *src, *dest;
struct stat *stp;
{
#if	MSDOS
	u_short mode;
#endif

	if (Xrename(src, dest) >= 0) return(0);
	if (errno != EXDEV || (stp -> st_mode & S_IFMT) == S_IFDIR)
		return(-1);
#if	MSDOS
	mode = stp -> st_mode;
	if (!(stp -> st_mode & S_IWRITE)) {
		stp -> st_mode |= S_IWRITE;
		Xchmod(src, stp -> st_mode);
	}
#endif
	if (cpfile(src, dest, stp) < 0 || Xunlink(src) < 0
	|| ((stp -> st_mode & S_IFMT) != S_IFLNK
	&& (touchfile(dest, stp -> st_atime, stp -> st_mtime) < 0
#ifdef	HAVEFLAGS
	|| chflags(dest, stp -> st_flags) < 0
#endif
	))) return(-1);
#if	MSDOS
	if (!(mode & S_IWRITE)) Xchmod(dest, mode);
#endif
	return(0);
}

int mktmpdir(dir)
char *dir;
{
	char *cp, path[MAXPATHLEN];
	int no;

	if (!deftmpdir || !*deftmpdir || !dir || !*dir) {
		errno = ENOENT;
		return(-1);
	}
	realpath2(deftmpdir, path, 1);
	free(deftmpdir);
#if	MSDOS && !defined (_NODOSDRIVE)
# ifdef	USEGETWD
	if (checkdrive(toupper2(path[0]) - 'A')) getwd(path);
# else
	if (checkdrive(toupper2(path[0]) - 'A')) getcwd(path, MAXPATHLEN);
# endif
#endif
	deftmpdir = strdup2(path);
#if	MSDOS
	no = getcurdrv();
	if (no >= 'a' && no <= 'z' && *path >= 'A' && *path <= 'Z')
		*path += 'a' - 'A';
	if (no >= 'A' && no <= 'Z' && *path >= 'a' && *path <= 'z')
		*path += 'A' - 'a';
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
		else if (applydir(".", Xunlink, NULL, 0, NULL) < 0) {
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
	if (_chdir2(fullpath) < 0) error(fullpath);
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

	if (chdir(buf) != 0) return(0);
	chdir(_SS_);
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
int tmpdosdupl(dir, dirp, list, max, single)
char *dir, **dirp;
namelist *list;
int max, single;
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
		strcpy(cp, list[filepos].name);
		st.st_mode = list[filepos].st_mode;
		st.st_atime =
		st.st_mtime = list[filepos].st_mtim;
		if (cpfile(list[filepos].name, path, &st) < 0) {
			removetmp(path, NULL, NULL);
			return(-1);
		}
	}
	else for (i = 0; i < max; i++) {
		if (!ismark(&(list[i]))) continue;
		strcpy(cp, list[i].name);
		st.st_mode = list[i].st_mode;
		st.st_atime = st.st_mtime = list[i].st_mtim;
		if (cpfile(list[i].name, path, &st) < 0) {
			removetmp(path, NULL, "");
			return(-1);
		}
	}

	*(--cp) = '\0';
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
	if (Xlstat(file, &st) < 0 || cpfile(file, path, &st) < 0)
		return(-1);

	return(0);
}
#endif	/* !_NODOSDRIVE */

#ifndef	_NOWRITEFS
static int k_strlen(s)
char *s;
{
	int i;

	for (i = 0; s[i]; i++) if (iskanji1(s, i)) i++;
	return(i);
}

static int nofile(file)
char *file;
{
#if	MSDOS && !defined (_NOUSELFN)
	char *cp, buf[MAXPATHLEN];

	if (!(cp = preparefile(file, buf, 0)) && errno == ENOENT) return(1);
	if (cp != file) return(0);
#endif
	if (Xaccess(file, F_OK) < 0 && errno == ENOENT) return(1);
	return(0);
}

static char *maketmpfile(len, dos, tmpdir, old)
int len, dos;
char *tmpdir, *old;
{
	char *fname, path[MAXPATHLEN];
	int i, l, fd;

	if (len < 0) return(NULL);
	fname = malloc2(len + 1);

	for (i = 0; i < len; i++) fname[i] = '_';
	fname[i] = '\0';

	if (tmpdir) l = strcatdelim2(path, tmpdir, NULL) - path;

	for (;;) {
		if (tmpdir) {
			strcpy(path + l, fname);
			if (nofile(path)) {
				if (old) {
#if	!MSDOS
					if (!nofile(fname));
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
static char *getentnum(dir, bsiz, fs)
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

static VOID restorefile(dir, path, fnamp)
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

VOID arrangedir(list, max, fs)
namelist *list;
int max, fs;
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

	for (i = 0; i < max; i++) if (!isdotdir(list[i].name)) break;
	if (i >= max) return;
	top = i;
	if (dos == 1) len = 8;
	else len = getnamlen(realdirsiz(list[top].name));
	if (!(tmpdir = maketmpfile(len, dos, NULL, NULL))) {
		warning(0, NOWRT_K);
		return;
	}
#if	!MSDOS
	persec = getblocksize(tmpdir);
#endif
	fnamp = strcatdelim2(path, tmpdir, NULL) - path;
	waitmes();

	if (!(dirp = _Xopendir("."))) error(".");
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

	for (i = 0; i < max; i++) {
		if (isdotdir(list[i].name) || i == top) continue;
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
				ent = realdirsiz(list[i].name);
				break;
			default:
				break;
		}
		if (ent < realdirsiz(list[i].name)) {
			tmpfile = b_realloc(tmpfile, tmpno, char *);
			tmpfile[tmpno] =
				maketmpfile(getnamlen(ent), dos, tmpdir, NULL);
			tmpno++;
			ptr = 0;
			totalent = headbyte;
			if (entnum) totalptr = entnum[++block];
		}
#endif
		strcpy(path + fnamp, list[i].name);
		if (rename2(path, list[i].name) < 0) error(path);
#if	!MSDOS
		totalent += realdirsiz(list[i].name);
		ptr++;
#endif
	}
#if	!MSDOS
	if (entnum) free(entnum);
#endif

	if (!(cp = maketmpfile(len, dos, tmpdir, tmpdir))) error(tmpdir);
	free(tmpdir);
	tmpdir = cp;
	strcatdelim2(path, tmpdir, list[top].name);
	if (rename2(path, list[top].name) < 0) error(path);
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
