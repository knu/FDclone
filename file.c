/*
 *	file.c
 *
 *	File Access Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"
#include "kctype.h"

#include <sys/stat.h>

#if	MSDOS
#include "unixemu.h"
extern char *preparefile();
#else
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/param.h>
extern int preparedrv();
extern int shutdrv();
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

static int iscurdir();
static char *getdistdir();
#ifndef	_NOWRITEFS
static int k_strlen();
static VOID touch();
static char *maketmpfile();
#if	!MSDOS
static char *getentnum();
#endif
static VOID restorefile();
#endif	/* !_NOWRITEFS */

char *deftmpdir;

#if	!MSDOS && !defined (_NODOSDRIVE)
static int distdrive = -1;
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
	else if (name = getpwuid2(euid)) {
		setgrent();
		while (gp = getgrent()) {
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

	if (Xlstat(file, &lstatus) < 0 || stat2(file, &status) < 0) return(-1);
	list[i].st_mode = lstatus.st_mode;
	list[i].flags = 0;
	if ((status.st_mode & S_IFMT) == S_IFDIR) list[i].flags |= F_ISDIR;
	if ((lstatus.st_mode & S_IFMT) == S_IFLNK) list[i].flags |= F_ISLNK;
	if ((lstatus.st_mode & S_IFMT) == S_IFCHR
	|| (lstatus.st_mode & S_IFMT) == S_IFBLK) list[i].flags |= F_ISDEV;

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
		logical_access(status.st_mode);
#else
		logical_access(status.st_mode, status.st_uid, status.st_gid);
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

int cmplist(listp1, listp2)
namelist *listp1, *listp2;
{
	char *cp1, *cp2;
	long tmp;

	if (!sorton) return(listp1 -> ent - listp2 -> ent);
	if (!isdir(listp1) && isdir(listp2)) return(1);
	if (isdir(listp1) && !isdir(listp2)) return(-1);

	if (listp1 -> name[0] == '.' && listp1 -> name[1] == '\0') return(-1);
	if (listp2 -> name[0] == '.' && listp2 -> name[1] == '\0') return(1);
	if (listp1 -> name[0] == '.' && listp1 -> name[1] == '.'
	&& listp1 -> name[2] == '\0') return(-1);
	if (listp2 -> name[0] == '.' && listp2 -> name[1] == '.'
	&& listp2 -> name[2] == '\0') return(1);

	switch (sorton & 7) {
		case 1:
			tmp = strpathcmp(listp1 -> name, listp2 -> name);
			break;
		case 2:
			if (cp1 = strrchr(listp1 -> name, '.')) cp1++;
			else cp1 = "";
			if (cp2 = strrchr(listp2 -> name, '.')) cp2++;
			else cp2 = "";
			tmp = strpathcmp(cp1, cp2);
			break;
		case 3:
			tmp = (long)(listp1 -> st_size)
				- (long)(listp2 -> st_size);
			break;
		case 4:
			tmp = listp1 -> st_mtim - listp2 -> st_mtim;
			break;
		default:
			tmp = 0;
			break;
	}

	if (sorton > 7) tmp = -tmp;
	if (!tmp) tmp = listp1 -> ent - listp2 -> ent;

	if (tmp > 0) tmp = 1;
	else if (tmp < 0) tmp = -1;
	return((int)tmp);
}

#ifndef	_NOTREE
int cmptree(listp1, listp2)
treelist *listp1, *listp2;
{
	char *cp1, *cp2;
	int tmp;

	if (!strcmp(listp1 -> name, "...")) return(1);
	if (!strcmp(listp2 -> name, "...")) return(-1);
	switch (sorton & 7) {
		case 1:
			tmp = strpathcmp(listp1 -> name, listp2 -> name);
			break;
		case 2:
			if (cp1 = strrchr(listp1 -> name, '.')) cp1++;
			else cp1 = "";
			if (cp2 = strrchr(listp2 -> name, '.')) cp2++;
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

struct dirent *searchdir(dirp, regexp)
DIR *dirp;
reg_t *regexp;
{
	struct dirent *dp;

	if (!regexp) dp = Xreaddir(dirp);
	else while (dp = Xreaddir(dirp)) {
		if (regexp_exec(regexp, dp -> d_name)) break;
	}
	return(dp);
}

static int iscurdir(path)
char *path;
{
	char *cwd;
	int i;

	cwd = getwd2();
#ifndef	_NODOSDRIVE
	if (dospath(path, NULL) != dospath(cwd, NULL)) return(0);
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

int underhome()
{
#if	MSDOS
	return(1);
#else	/* !MSDOS */
	static char *homedir = NULL;
	struct passwd *pwd;
	char *cp;
	int i;

	if (!homedir) {
		if (!(homedir = (char *)getenv("HOME"))) {
			if (pwd = getpwuid(getuid())) homedir = pwd -> pw_dir;
			else return(-1);
		}
		if (
#ifndef	_NODOSDRIVE
		dospath(homedir, NULL) ||
#endif
		_chdir2(homedir) < 0) {
			homedir = NULL;
			return(-1);
		}
		homedir = getwd2();
		if (_chdir2(fullpath) < 0) error(fullpath);
	}
	cp = getwd2();
	i = strncmp(cp, homedir, strlen(homedir));
	free(cp);
	return(i ? 0 : 1);
#endif	/* !MSDOS */
}

static char *getdistdir(mes, arg)
char *mes, *arg;
{
	struct stat status;
	char *dir;
#if	!MSDOS
	int drive;
#endif

	if (arg && *arg) dir = evalpath(strdup2(arg));
	else if (!(dir = inputstr(mes, 1, -1, NULL, NULL))) return(NULL);
	if (!*(dir = evalpath(dir))) {
		free(dir);
		dir = strdup2(".");
	}

#if	!MSDOS && !defined (_NODOSDRIVE)
	distdrive = -1;
	if ((drive = dospath(dir, NULL))
	&& (distdrive = preparedrv(drive, waitmes)) < 0) {
		warning(-1, dir);
		free(dir);
		return(NULL);
	}
#endif
	if (stat2(dir, &status) < 0) {
		if (errno != ENOENT || Xmkdir(dir, 0777) < 0
		|| stat2(dir, &status) < 0) {
			warning(-1, dir);
			free(dir);
#if	!MSDOS && !defined (_NODOSDRIVE)
			if (distdrive >= 0) shutdrv(distdrive);
#endif
			return(NULL);
		}
	}
	if ((status.st_mode & S_IFMT) != S_IFDIR) {
		warning(ENOTDIR, dir);
		free(dir);
#if	!MSDOS && !defined (_NODOSDRIVE)
		if (distdrive >= 0) shutdrv(distdrive);
#endif
		return(NULL);
	}
	return(dir);
}

/* ARGSUSED */
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
# if	MSDOS || defined (_NODOSDRIVE)
	(tr) ? tree(1, NULL) :
# else
	(tr) ? tree(1, &distdrive) :
# endif
#endif	/* !_NOTREE */
	getdistdir(COPYD_K, arg);

	if (!destpath) return((tr) ? 2 : 1);
	copypolicy = (iscurdir(destpath)) ? 2 : 0;
	if (mark > 0) applyfile(list, max, cpfile, ENDCP_K);
	else if (isdir(&list[filepos]) && !islink(&list[filepos])) {
		if (copypolicy) {
			warning(EEXIST, list[filepos].name);
			return((tr) ? 2 : 1);
		}
		applydir(list[filepos].name, cpfile, cpdir, NULL, ENDCP_K);
	}
	else if (cpfile(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	free(destpath);
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (distdrive >= 0) shutdrv(distdrive);
#endif
	return(4);
}

/* ARGSUSED */
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
# if	MSDOS || defined (_NODOSDRIVE)
	(tr) ? tree(1, NULL) :
# else
	(tr) ? tree(1, &distdrive) :
# endif
#endif	/* !_NOTREE */
	getdistdir(MOVED_K, arg);

	if (!destpath || iscurdir(destpath)) return((tr) ? 2 : 1);
	copypolicy = 0;
	if (mark > 0) filepos = applyfile(list, max, mvfile, ENDMV_K);
	else if (mvfile(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	else filepos++;
	if (filepos >= max) filepos = max - 1;
	free(destpath);
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (distdrive >= 0) shutdrv(distdrive);
#endif
	return(4);
}

int mktmpdir(dir)
char *dir;
{
	char *cp, path[MAXPATHLEN + 1];
	int no;

	strcpy(path, deftmpdir);
	strcat(path, _SS_);
	strcat(path, tmpfilename);
	if (_Xmkdir(path, 0777) < 0 && errno != EEXIST) return(-1);
	*(cp = path + strlen(path)) = _SC_;
	strcpy(cp + 1, dir);
	if (_Xmkdir(path, 0777) < 0) {
		*cp = '\0';
		no = errno;
		if (_Xrmdir(path) < 0 && errno != ENOTEMPTY && errno != EEXIST)
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
	if (_Xrmdir(path) < 0 && errno != ENOTEMPTY && errno != EEXIST)
		return(-1);
	return(0);
}

VOID removetmp(dir, subdir, file)
char *dir, *subdir, *file;
{
	char *cp, *tmp, *dupdir;

	if (_chdir2(dir) < 0) {
		warning(-1, dir);
		*dir = '\0';
		subdir = file = NULL;
	}
	if (subdir && *subdir && _chdir2(subdir) < 0) {
		warning(-1, subdir);
		subdir = file = NULL;
	}
	if (file && unlink(file) < 0) {
		warning(-1, file);
		subdir = NULL;
	}
	if (subdir && *subdir) {
		if (_chdir2(dir) < 0) error(dir);
		dupdir = strdup2(subdir);
		cp = dupdir + strlen(dupdir);
		for (;;) {
			*cp = '\0';
			tmp = strrchr(dupdir, _SC_);
			if (tmp) tmp++;
			else tmp = dupdir;
			if ((strcmp(tmp, ".") && rmdir(dupdir) < 0)
			|| tmp == dupdir) break;
			cp = tmp - 1;
		}
		free(dupdir);
	}
	if (_chdir2(fullpath) < 0) error(fullpath);
	if (rmtmpdir(dir) < 0) warning(-1, dir);
	free(dir);
}

int forcecleandir(dir, file)
char *dir, *file;
{
#if	!MSDOS
	extern char **environ;
	char buf[MAXPATHLEN + 1];
	long pid;
	int i, len;

	for (i = 0; dir[i]; i++) buf[i] = dir[i];
	len = i;
	buf[len++] = _SC_;
	for (i = 0; file[i]; i++) buf[len + i] = file[i];
	len += i;
	buf[len] = '\0';

	chdir(_SS_);
	if ((pid = fork()) < 0) return(-1);
	else if (!pid) {
		execle("/bin/rm", "rm", "-rf", buf, NULL, environ);
		_exit(127);
	}
#endif
	return(0);
}

#ifndef	_NOWRITEFS
static int k_strlen(s)
char *s;
{
	int i;

	for (i = 0; *s; i++) if (issjis1((u_char)(*(s++)))) s++;
	return(i);
}

static VOID touch(file)
char *file;
{
	FILE *fp;
#if	MSDOS
	char *cp, buf[MAXPATHLEN + 1];

	if (!(cp = preparefile(file, buf, 1))) error(file);
	if (cp != file) return;
#endif

	if (!(fp = fopen(file, "w"))) error(file);
	fclose(fp);
}

static char *maketmpfile(len, dos)
int len, dos;
{
#if	MSDOS
	char *cp, buf[MAXPATHLEN + 1];
#endif
	struct stat status;
	char *str;
	int i;

	if (len < 0) return(NULL);
	str = (char *)malloc2(len + 1);

	for (i = 0; i < len; i++) str[i] = 'a';
	str[i] = '\0';

	for (;;) {
#if	MSDOS
		cp = preparefile(str, buf, 0);
		if ((!cp || (cp == str && Xstat(str, &status) < 0))
		&& errno == ENOENT) return(str);
#else
		if (Xlstat(str, &status) < 0 && errno == ENOENT) return(str);
#endif

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
		fseek(fp, i * bsiz + 3, 0);
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
	while (dp = Xreaddir(dirp)) {
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
	int tmpno, maxtmp, block, totalent, ptr, totalptr;
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
	tmpdir = maketmpfile(len, dos);
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
	while (dp = Xreaddir(dirp)) {
		if (isdotdir(dp -> d_name)) continue;
		else if (!strpathcmp(dp -> d_name, tmpdir)) {
#if	MSDOS
#ifndef	__GNUC__
			if (!(dp -> d_alias[0])) len = 8;
#endif
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
		cp = maketmpfile(len, dos);
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
	if (entnum) totalptr = entnum[block];
	tmpfile = NULL;
	maxtmp = 0;
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
			tmpfile = (char **)addlist(tmpfile, tmpno,
				&maxtmp, sizeof(char *));
			tmpfile[tmpno] = maketmpfile(getnamlen(ent), dos);
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

	cp = maketmpfile(len, dos);
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
