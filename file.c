/*
 *	file.c
 *
 *	File Access Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"

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
extern int preparedrv __P_((int));
extern int shutdrv __P_((int));
#endif

extern int filepos;
extern int mark;
extern int dispmode;
extern int sorton;
extern char *destpath;
extern int copypolicy;
extern char fullpath[];
extern char *tmpfilename;

#define	DOSDIRENT		32
#define	LFNENTSIZ		13

#define	realdirsiz(name)	((dos) ? ((k_strlen(name) / LFNENTSIZ + 1)\
					* DOSDIRENT + DOSDIRENT)\
				:	(((strlen(name) + boundary)\
					& ~(boundary - 1)) + dirsize))
#define	getnamlen(ent)		((dos) ? ((((ent) - DOSDIRENT)\
					/ DOSDIRENT) * LFNENTSIZ - 1)\
				: 	((((ent) - dirsize)\
					& ~(boundary - 1)) - 1))

static int iscurdir __P_((char *));
static int islowerdir __P_((char *, char *));
static char *getdestdir __P_((char *, char *));
#ifndef	_NOWRITEFS
static int k_strlen __P_((char *));
#if	!MSDOS
static VOID touch __P_((char *));
#endif
static int nofile __P_((char *));
static char *maketmpfile __P_((int, int, char *));
#if	!MSDOS
static char *getentnum __P_((char *, int, int));
#endif
static VOID restorefile __P_((char *, char *, int));
#endif	/* !_NOWRITEFS */

char *deftmpdir = NULL;

#ifndef	_NODOSDRIVE
static int destdrive = -1;
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

int getstatus(list, i, file)
namelist *list;
int i;
char *file;
{
	struct stat status, lstatus;

#if	MSDOS
	if (stat2(file, &status) < 0) return(-1);
	memcpy(&lstatus, &status, sizeof(struct stat));
#else
# ifndef	_NODOSDRIVE
	if (_dospath(file)) {
		if (stat2(file, &status) < 0) return(-1);
		memcpy(&lstatus, &status, sizeof(struct stat));
	}
	else
# endif
	if (Xlstat(file, &lstatus) < 0 || stat2(file, &status) < 0) return(-1);
#endif
	list[i].st_mode = lstatus.st_mode;
	list[i].flags = 0;
	if ((status.st_mode & S_IFMT) == S_IFDIR) list[i].flags |= F_ISDIR;
	if ((lstatus.st_mode & S_IFMT) == S_IFLNK) list[i].flags |= F_ISLNK;
#if	!MSDOS
	if ((lstatus.st_mode & S_IFMT) == S_IFCHR
	|| (lstatus.st_mode & S_IFMT) == S_IFBLK) list[i].flags |= F_ISDEV;
#endif

	if (isdisplnk(dispmode)) memcpy(&lstatus, &status, sizeof(struct stat));

	list[i].st_nlink = lstatus.st_nlink;
#if	!MSDOS
	list[i].st_uid = lstatus.st_uid;
	list[i].st_gid = lstatus.st_gid;
#endif
	list[i].st_size = isdev(&list[i]) ? lstatus.st_rdev : lstatus.st_size;
	list[i].st_mtim = lstatus.st_mtime;
	list[i].flags |=
#if	MSDOS
		logical_access((u_short)(status.st_mode));
#else
		logical_access((u_short)(status.st_mode),
			status.st_uid, status.st_gid);
#endif

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
	char *cp1, *cp2;
	long tmp;

	if (!sorton)
		return(((namelist *)vp1) -> ent - ((namelist *)vp2) -> ent);
	if (!isdir((namelist *)vp1) && isdir((namelist *)vp2)) return(1);
	if (isdir((namelist *)vp1) && !isdir((namelist *)vp2)) return(-1);

	if (((namelist *)vp1) -> name[0] == '.'
	&& ((namelist *)vp1) -> name[1] == '\0') return(-1);
	if (((namelist *)vp2) -> name[0] == '.'
	&& ((namelist *)vp2) -> name[1] == '\0') return(1);
	if (((namelist *)vp1) -> name[0] == '.'
	&& ((namelist *)vp1) -> name[1] == '.'
	&& ((namelist *)vp1) -> name[2] == '\0') return(-1);
	if (((namelist *)vp2) -> name[0] == '.'
	&& ((namelist *)vp2) -> name[1] == '.'
	&& ((namelist *)vp2) -> name[2] == '\0') return(1);

	switch (sorton & 7) {
		case 5:
			tmp = strlen(((namelist *)vp1) -> name)
				- strlen(((namelist *)vp2) -> name);
			if (tmp != 0) break;
		case 1:
			tmp = strpathcmp(((namelist *)vp1) -> name,
				((namelist *)vp2) -> name);
			break;
		case 2:
			if ((cp1 = strrchr(((namelist *)vp1) -> name, '.')))
				cp1++;
			else cp1 = "";
			if ((cp2 = strrchr(((namelist *)vp2) -> name, '.')))
				cp2++;
			else cp2 = "";
			tmp = strpathcmp(cp1, cp2);
			break;
		case 3:
			tmp = (long)(((namelist *)vp1) -> st_size)
				- (long)(((namelist *)vp2) -> st_size);
			break;
		case 4:
			tmp = ((namelist *)vp1) -> st_mtim
				- ((namelist *)vp2) -> st_mtim;
			break;
		default:
			tmp = 0;
			break;
	}

	if (sorton > 7) tmp = -tmp;
	if (!tmp) tmp = ((namelist *)vp1) -> ent - ((namelist *)vp2) -> ent;

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
	namelist tmp;
	int i;
#endif

	if (regexp) while ((dp = Xreaddir(dirp))) {
		if (regexp_exec(regexp, dp -> d_name)) break;
	}
#ifndef	_NOARCHIVE
	else if (arcregstr) while ((dp = Xreaddir(dirp))) {
		tmp.name = dp -> d_name;
		i = searcharc(arcregstr, &tmp, 1, -1);
		if (i < 0) return(NULL);
		if (i) break;
	}
#endif
	else dp = Xreaddir(dirp);
	return(dp);
}

static int iscurdir(path)
char *path;
{
	char *cwd;
	int i;

	cwd = getwd2();
#if	MSDOS || !defined (_NODOSDRIVE)
	if (dospath(path, NULL) != dospath(cwd, NULL)) {
		free(cwd);
		return(0);
	}
#endif
	if (_chdir2(path) < 0) path = NULL;
	else {
		path = getwd2();
		if (_chdir2(cwd) < 0) error(cwd);
	}
	i = (path) ? !strpathcmp(cwd, path) : 0;
	free(cwd);
	if (path) free(path);
	return(i);
}

static int islowerdir(path, org)
char *path, *org;
{
	char *cp, *top, *cwd, tmp[MAXPATHLEN + 1];
	int i;

	cwd = getwd2();
	strcpy(tmp, path);
	top = tmp;
#if	MSDOS || !defined (_NODOSDRIVE)
	if (dospath(path, NULL) != dospath(org, NULL)) {
		free(cwd);
		return(0);
	}
	if (_dospath(tmp)) top += 2;
#endif
	while (_chdir2(tmp) < 0) {
		if ((cp = strrdelim(top, 0)) && cp > top) *cp = '\0';
		else {
			path = NULL;
			break;
		}
	}
	i = 0;
	if (path) {
		path = getwd2();
		if (_chdir2(cwd) < 0) error(cwd);
		if (_chdir2(org) >= 0) {
			org = getwd2();
			i = strlen(org);
			if (strnpathcmp(org, path, i) || path[i] != _SC_)
				i = 0;
			free(org);
		}
		if (_chdir2(cwd) < 0) error(cwd);
		free(path);
	}
	free(cwd);
	return(i);
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
	if (!len || strnpathcmp(cwd, homedir, len)) {
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
	struct stat status;
	char tmp[MAXPATHLEN + 1];
#if	MSDOS
	char *cp;
#endif

	if (stat2(dir, &status) < 0) {
		if (errno != ENOENT) return(-1);
		if (Xmkdir(dir, 0777) < 0) {
#if	MSDOS
			if (errno != EEXIST) return(-1);
			if ((cp = strrdelim(dir, 1))) cp++;
			else cp = dir;
			if (cp[0] == '.' && !cp[1]) status.st_mode = S_IFDIR;
			else
#endif
			return(-1);
		}
		else if (stat2(dir, &status) < 0) return(-1);
	}
	if ((status.st_mode & S_IFMT) != S_IFDIR) {
		errno = ENOTDIR;
		return(-1);
	}
	entryhist(1, realpath2(dir, tmp), 1);
	return(0);
}

static char *getdestdir(mes, arg)
char *mes, *arg;
{
	char *dir;
#ifndef	_NODOSDRIVE
	int drive;
#endif

	if (arg && *arg) dir = strdup2(arg);
	else if (!(dir = inputstr(mes, 1, -1, NULL, 1))) return(NULL);
	else if (!*(dir = evalpath(dir, 1))) {
		dir = (char *)realloc2(dir, 2);
		dir[0] = '.';
		dir[1] = '\0';
	}
#if	MSDOS || !defined (_NODOSDRIVE)
	else if (_dospath(dir) && !dir[2]) {
		dir = (char *)realloc2(dir, 4);
		dir[2] = '.';
		dir[3] = '\0';
	}
#endif

#ifndef	_NODOSDRIVE
	destdrive = -1;
	if ((drive = dospath2(dir))
	&& (destdrive = preparedrv(drive)) < 0) {
		warning(-1, dir);
		free(dir);
		return(NULL);
	}
#endif
	if (preparedir(dir) < 0) {
		warning(-1, dir);
		free(dir);
#ifndef	_NODOSDRIVE
		if (destdrive >= 0) shutdrv(destdrive);
#endif
		return(NULL);
	}
	return(dir);
}

/*ARGSUSED*/
int copyfile(list, max, arg, tr)
namelist *list;
int max;
char *arg;
int tr;
{
	if (!mark && isdotdir(list[filepos].name)) {
		putterm(t_bell);
		return(0);
	}
	destpath =
#ifndef	_NOTREE
# ifdef	_NODOSDRIVE
	(tr) ? tree(1, (int *)1) :
# else
	(tr) ? tree(1, &destdrive) :
# endif
#endif	/* !_NOTREE */
	getdestdir(COPYD_K, arg);

	if (!destpath) return((tr) ? 2 : 1);
	copypolicy = (iscurdir(destpath)) ? 2 : 0;
	if (mark > 0) applyfile(list, max, cpfile, ENDCP_K);
	else if (isdir(&list[filepos]) && !islink(&list[filepos])) {
		if (copypolicy) {
			warning(EEXIST, list[filepos].name);
			return((tr) ? 2 : 1);
		}
		applydir(list[filepos].name, cpfile, cpdir,
			(islowerdir(destpath, list[filepos].name)) ? 2 : 1,
			ENDCP_K);
	}
	else if (cpfile(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	free(destpath);
#ifndef	_NODOSDRIVE
	if (destdrive >= 0) shutdrv(destdrive);
#endif
	return(4);
}

/*ARGSUSED*/
int movefile(list, max, arg, tr)
namelist *list;
int max;
char *arg;
int tr;
{
	if (!mark && isdotdir(list[filepos].name)) {
		putterm(t_bell);
		return(0);
	}
	destpath =
#ifndef	_NOTREE
# ifdef	_NODOSDRIVE
	(tr) ? tree(1, (int *)1) :
# else
	(tr) ? tree(1, &destdrive) :
# endif
#endif	/* !_NOTREE */
	getdestdir(MOVED_K, arg);

	if (!destpath || iscurdir(destpath)) return((tr) ? 2 : 1);
	copypolicy = 0;
	if (mark > 0) filepos = applyfile(list, max, mvfile, ENDMV_K);
	else if (islowerdir(destpath, list[filepos].name))
		warning(EINVAL, list[filepos].name);
	else if (mvfile(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	else filepos++;
	if (filepos >= max) filepos = max - 1;
	free(destpath);
#ifndef	_NODOSDRIVE
	if (destdrive >= 0) shutdrv(destdrive);
#endif
	return(4);
}

int mktmpdir(dir)
char *dir;
{
	char *cp, path[MAXPATHLEN + 1];
	int no;

	if (!deftmpdir || !*deftmpdir || !dir || !*dir) {
		errno = ENOENT;
		return(-1);
	}
	realpath2(deftmpdir, path);
	free(deftmpdir);
	deftmpdir = strdup2(path);
#if	MSDOS
	no = getcurdrv();
	if (no >= 'a' && no <= 'z' && *path >= 'A' && *path <= 'Z')
		*path += 'a' - 'A';
	if (no >= 'A' && no <= 'Z' && *path >= 'a' && *path <= 'z')
		*path += 'A' - 'a';
#endif
	strcat(path, _SS_);
	strcat(path, tmpfilename);
	if (_Xmkdir(path, 0777) < 0 && errno != EEXIST) return(-1);
	*(cp = path + strlen(path)) = _SC_;
	strcpy(cp + 1, dir);
	if (_Xmkdir(path, 0777) < 0 && errno != EEXIST) {
		*cp = '\0';
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
	char path[MAXPATHLEN + 1];

	if (dir && *dir && _Xrmdir(dir) < 0) return(-1);
	strcpy(path, deftmpdir);
	strcat(path, _SS_);
	strcat(path, tmpfilename);
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
	if (subdir && *subdir == _SC_) subdir++;
	if (subdir && *subdir && _chdir2(subdir) < 0) {
		warning(-1, subdir);
		subdir = file = NULL;
	}
	if (file && Xunlink(file) < 0) {
		warning(-1, file);
		subdir = NULL;
	}
	if (subdir && *subdir) {
		if (_chdir2(dir) < 0) error(dir);
		dupdir = strdup2(subdir);
		cp = dupdir + strlen(dupdir);
		for (;;) {
			*cp = '\0';
			if ((tmp = strrdelim(dupdir, 0))) tmp++;
			else tmp = dupdir;
			if ((*tmp && strcmp(tmp, ".") && _Xrmdir(dupdir) < 0)
			|| tmp == dupdir) break;
			cp = tmp - 1;
		}
		free(dupdir);
	}
	if (_chdir2(fullpath) < 0) error(fullpath);
	if (rmtmpdir(dir) < 0) warning(-1, dir);
	if (dir) free(dir);
}

int forcecleandir(dir, file)
char *dir, *file;
{
#if	!MSDOS
	long pid;
#endif
	extern char **environ;
	char buf[MAXPATHLEN + 1];
	int i, len;

	if (!dir || !*dir || !file || !*file) return(0);
	for (i = 0; dir[i]; i++) buf[i] = dir[i];
	len = i;
	buf[len++] = _SC_;
	for (i = 0; file[i]; i++) buf[len + i] = file[i];
	len += i;
	buf[len] = '\0';

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
int tmpdosdupl(dir, dirp, file, mode)
char *dir, **dirp, *file;
int mode;
{
	char *cp, path[MAXPATHLEN + 1];
	int drive;

	if (!(drive = dospath2(dir))) return(0);
	sprintf(path, "D%c", drive);
	if (mktmpdir(path) < 0) {
		warning(-1, path);
		return(-1);
	}

	cp = path + strlen(path);
	*cp = _SC_;
	strcpy(cp + 1, file);
	waitmes();
	if (_cpfile(file, path, mode) < 0) {
		removetmp(path, NULL, NULL);
		return(-1);
	}

	*cp = '\0';
	if (_chdir2(path) < 0) error(path);
	*dirp = strdup2(path);
	return(drive);
}

int tmpdosrestore(drive, file, mode)
int drive;
char *file;
int mode;
{
	char path[MAXPATHLEN + 1];

	sprintf(path, "%c:%s", drive, file);
	waitmes();
	if (_cpfile(file, path, mode) < 0) return(-1);

	return(0);
}
#endif	/* !_NODOSDRIVE */

#ifndef	_NOWRITEFS
static int k_strlen(s)
char *s;
{
	int i;

	for (i = 0; s[i]; i++)
		if (issjis1((u_char)(s[i])) && s[i + 1]) i++;
	return(i);
}

#if	!MSDOS
static VOID touch(file)
char *file;
{
	FILE *fp;

	if (!(fp = fopen(file, "w"))) error(file);
	fclose(fp);
}
#endif

static int nofile(file)
char *file;
{
	struct stat status;
#if	MSDOS && !defined (NOUSELFN)
	char *cp, buf[MAXPATHLEN + 1];

	cp = preparefile(file, buf, 0);
	if ((!cp || (cp == file && Xstat(file, &status) < 0))
#else
	if (Xlstat(file, &status) < 0
#endif
	&& errno == ENOENT) return(1);
	return(0);
}

static char *maketmpfile(len, dos, tmpdir)
int len, dos;
char *tmpdir;
{
	char *str, tmp[MAXPATHLEN + 1];
	int i, l;

	if (len < 0) return(NULL);
	str = (char *)malloc2(len + 1);

	for (i = 0; i < len; i++) str[i] = 'a';
	str[i] = '\0';

	l = 0;
	if (tmpdir) {
		strcpy(tmp, tmpdir);
		l = strlen(tmp);
		tmp[l++] = _SC_;
	}

	for (;;) {
		if (tmpdir) strcpy(&tmp[l], str);
		if (nofile(str) && (!tmpdir || nofile(tmp))) return(str);

		for (i = len - 1; i >= 0; i--) {
			if (dos) {
				if (str[i] == 'z') str[i] = 'a';
				else str[i]++;
			}
			else {
				if (str[i] == 'z') str[i] = 'A';
				else if (str[i] == 'Z') str[i] = '0';
				else if (str[i] == '9') str[i] = 'a';
				else str[i]++;
			}
			if (str[i] != 'a') break;
		}
		if (i < 0) {
			free(str);
			return(NULL);
		}
	}
}

#if	!MSDOS
static char *getentnum(dir, bsiz, fs)
char *dir;
int bsiz, fs;
{
	FILE *fp;
	struct stat status;
	char *tmp;
	int i, n;

	if (fs != 2) return(NULL);	/* without IRIX File System */

	if (lstat(dir, &status) < 0) error(dir);
	n = (long)(status.st_size) / bsiz;
	tmp = (char *)malloc2(n + 1);

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
	char *cp, *tmpdir, path[MAXPATHLEN + 1];
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
	top = i;
	if (dos == 1) len = 8;
	else len = getnamlen(realdirsiz(list[top].name));
	tmpdir = maketmpfile(len, dos, NULL);
	if (_Xmkdir(tmpdir, 0777) < 0) {
		warning(0, NOWRT_K);
		free(tmpdir);
		return;
	}
#if	!MSDOS
	persec = getblocksize(tmpdir);
#endif
	strcpy(path, tmpdir);
	strcat(path, _SS_);
	fnamp = strlen(path);
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
		cp = maketmpfile(len, dos, tmpdir);
		if (rename2(tmpdir, cp) < 0) error(tmpdir);
		free(tmpdir);
		tmpdir = cp;
		strcpy(path, tmpdir);
		strcat(path, _SS_);
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
			tmpfile = (char **)b_realloc(tmpfile, tmpno, char *);
			tmpfile[tmpno] =
				maketmpfile(getnamlen(ent), dos, tmpdir);
			if (tmpfile[tmpno]) touch(tmpfile[tmpno]);
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

	cp = maketmpfile(len, dos, tmpdir);
	if (rename2(tmpdir, cp) < 0) error(tmpdir);
	free(tmpdir);
	tmpdir = cp;
	strcpy(path, tmpdir);
	strcat(path, _SS_);
	strcat(path, list[top].name);
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
