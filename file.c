/*
 *	file.c
 *
 *	File Access Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"

#include <ctype.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>

extern int preparedrv();
extern int shutdrv();

extern int filepos;
extern int mark;
extern int dispmode;
extern int sorton;
extern char *destpath;
extern int copypolicy;
extern char fullpath[];
extern char *tmpfilename;

#define	realdirsiz(name)		(((strlen(name) + boundary)\
					& ~(boundary - 1)) + dirsize)
#define	getnamlen(ent)			((((ent) - dirsize)\
					& ~(boundary - 1)) - 1)

static int iscurdir();
static char *getdistdir();
static VOID touch();
static char *maketmpfile();
static char *getentnum();
static VOID restorefile();

char *deftmpdir;

static int distdrive = -1;


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
	list[i].st_uid = lstatus.st_uid;
	list[i].st_gid = lstatus.st_gid;
	list[i].st_size = isdev(&list[i]) ? lstatus.st_rdev : lstatus.st_size;
	list[i].st_mtim = lstatus.st_mtime;
	return(0);
}

int cmplist(listp1, listp2)
namelist *listp1, *listp2;
{
	char *cp1, *cp2;
	int tmp;

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
			tmp = strcmp(listp1 -> name, listp2 -> name);
			break;
		case 2:
			if (cp1 = strrchr(listp1 -> name, '.')) cp1++;
			else cp1 = "";
			if (cp2 = strrchr(listp2 -> name, '.')) cp2++;
			else cp2 = "";
			tmp = strcmp(cp1, cp2);
			break;
		case 3:
			tmp = (int)(listp1 -> st_size)
				- (int)(listp2 -> st_size);
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
	return(tmp);
}

int cmptree(listp1, listp2)
treelist *listp1, *listp2;
{
	char *cp1, *cp2;
	int tmp;

	if (!strcmp(listp1 -> name, "...")) return(1);
	if (!strcmp(listp2 -> name, "...")) return(-1);
	switch (sorton & 7) {
		case 1:
			tmp = strcmp(listp1 -> name, listp2 -> name);
			break;
		case 2:
			if (cp1 = strrchr(listp1 -> name, '.')) cp1++;
			else cp1 = "";
			if (cp2 = strrchr(listp2 -> name, '.')) cp2++;
			else cp2 = "";
			tmp = strcmp(cp1, cp2);
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
	if (dospath(path, NULL) != dospath(cwd, NULL)) return(0);
	if (_chdir2(path) < 0) path = NULL;
	else {
		path = getwd2();
		if (_chdir2(cwd) < 0) error(cwd);
	}
	i = (path) ? !strcmp(cwd, path) : 0;
	free(cwd);
	return(i);
}

int underhome()
{
	static char *homedir = NULL;
	struct passwd *pwd;
	char *cp;
	int i;

	if (!homedir) {
		if (!(homedir = (char *)getenv("HOME"))) {
			if (pwd = getpwuid(getuid())) homedir = pwd -> pw_dir;
			else return(-1);
		}
		if (dospath(homedir, NULL) || _chdir2(homedir) < 0) {
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
}

static char *getdistdir(mes)
char *mes;
{
	struct stat status;
	char *dir;
	int drive;

	if (!(dir = inputstr(mes, 1, -1, NULL, NULL))) return(NULL);
	if (!*(dir = evalpath(dir))) {
		free(dir);
		dir = strdup2(".");
	}

	distdrive = -1;
	if ((drive = dospath(dir, NULL))
	&& (distdrive = preparedrv(drive, waitmes)) < 0) {
		warning(-1, dir);
		free(dir);
		return(NULL);
	}
	if (stat2(dir, &status) < 0) {
		if (errno != ENOENT || Xmkdir(dir, 0777) < 0
		|| stat2(dir, &status) < 0) {
			warning(-1, dir);
			free(dir);
			if (distdrive >= 0) shutdrv(distdrive);
			return(NULL);
		}
	}
	if ((status.st_mode & S_IFMT) != S_IFDIR) {
		warning(ENOTDIR, dir);
		free(dir);
		if (distdrive >= 0) shutdrv(distdrive);
		return(NULL);
	}
	return(dir);
}

int copyfile(list, max, tr)
namelist *list;
int max, tr;
{
	if (!mark && (!strcmp(list[filepos].name, ".")
	|| !strcmp(list[filepos].name, ".."))) {
		putterm(t_bell);
		return(0);
	}
	destpath = (tr) ? tree(1, &distdrive) : getdistdir(COPYD_K);
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
	if (distdrive >= 0) shutdrv(distdrive);
	return(4);
}

int movefile(list, max, tr)
namelist *list;
int max, tr;
{
	if (!mark && (!strcmp(list[filepos].name, ".")
	|| !strcmp(list[filepos].name, ".."))) {
		putterm(t_bell);
		return(0);
	}
	destpath = (tr) ? tree(1, &distdrive) : getdistdir(MOVED_K);
	if (!destpath || iscurdir(destpath)) return((tr) ? 2 : 1);
	copypolicy = 0;
	if (mark > 0) filepos = applyfile(list, max, mvfile, ENDMV_K);
	else if (mvfile(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	else filepos++;
	if (filepos >= max) filepos = max - 1;
	free(destpath);
	if (distdrive >= 0) shutdrv(distdrive);
	return(4);
}

int mktmpdir(dir)
char *dir;
{
	char *cp, path[MAXPATHLEN + 1];
	int no;

	strcpy(path, deftmpdir);
	strcat(path, "/");
	strcat(path, tmpfilename);
	if (mkdir(path, 0777) < 0 && errno != EEXIST) return(-1);
	*(cp = path + strlen(path)) = '/';
	strcpy(cp + 1, dir);
	if (mkdir(path, 0777) < 0) {
		*cp = '\0';
		no = errno;
		if (rmdir(path) < 0 && errno != ENOTEMPTY && errno != EEXIST)
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

	if (dir && *dir && rmdir(dir) < 0) return(-1);
	strcpy(path, deftmpdir);
	strcat(path, "/");
	strcat(path, tmpfilename);
	if (rmdir(path) < 0 && errno != ENOTEMPTY && errno != EEXIST)
		return(-1);
	return(0);
}

int forcecleandir(dir, file)
char *dir, *file;
{
	extern char **environ;
	char buf[MAXPATHLEN + 1];
	int i, len, pid;

	for (i = 0; dir[i]; i++) buf[i] = dir[i];
	len = i;
	buf[len++] = '/';
	for (i = 0; file[i]; i++) buf[len + i] = file[i];
	len += i;
	buf[len] = '\0';

	chdir("/");
	if ((pid = fork()) < 0) return(-1);
	else if (!pid) {
		execle("/bin/rm", "rm", "-rf", buf, NULL, environ);
		_exit(127);
	}
	return(0);
}

static VOID touch(file)
char *file;
{
	FILE *fp;

	if (!(fp = fopen(file, "w"))) error(file);
	fclose(fp);
}

static char *maketmpfile(len)
int len;
{
	struct stat buf;
	char *str;
	int i;

	if (len < 0) return(NULL);
	str = (char *)malloc2(len + 1);

	for (i = 0; i < len; i++) str[i] = '0';
	str[i] = '\0';

	for (;;) {
		if (lstat(str, &buf) < 0 && errno == ENOENT) return(str);

		for (i = len - 1; i >= 0; i--) {
			if (str[i] == '9') str[i] = 'A';
			else if (str[i] == 'Z') str[i] = 'a';
			else if (str[i] == 'z') str[i] = '0';
			else str[i]++;
			if (str[i] != '0') break;
		}
		if (i < 0) {
			free(str);
			return(NULL);
		}
	}
}

static char *getentnum(dir, bsiz, fs)
char *dir;
int bsiz, fs;
{
	FILE *fp;
	struct stat status;
	char *tmp;
	int i, n;

	if (lstat(dir, &status) < 0) error(dir);
	n = (int)(status.st_size) / bsiz;
	tmp = (char *)malloc2(n + 1);

	switch (fs) {
		case 2:	/* IRIX File System */
			fp = fopen(dir, "r");
			for (i = 0; i < n; i++) {
				fseek(fp, i * bsiz + 3, 0);
				tmp[i] = fgetc(fp) + 1;
			}
			tmp[i] = 0;
			fclose(fp);
			break;
		default:
			for (i = 0; i <= n; i++) tmp[i] = 0;
			break;
	}

	return(tmp);
}

static VOID restorefile(dir, path, fnamp)
char *dir, *path;
int fnamp;
{
	DIR *dirp;
	struct dirent *dp;

	if (!(dirp = opendir(dir))) error(dir);
	while (dp = readdir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;
		else {
			strcpy(path + fnamp, dp -> d_name);
			if (rename2(path, dp -> d_name) < 0) error(path);
		}
	}
	closedir(dirp);
	if (rmdir(dir) < 0) error(dir);
	free(dir);
}

VOID arrangedir(list, max, fs)
namelist *list;
int max, fs;
{
	DIR *dirp;
	struct dirent *dp;
	char **tmpfile;
	int maxtmp, persec;
	int headbyte, boundary, dirsize;
	int i, top, len, fnamp, tmpno, block, ent, totalent, ptr, totalptr;
	char *cp, *tmpdir, *entnum, path[MAXPATHLEN + 1];

	switch (fs) {
		case 2:	/* IRIX File System */
			headbyte = 4;
			boundary = 2;
			dirsize = sizeof(u_long);
			break;
		case 3:	/* SystemV R3 File System */
			headbyte = 0;
			boundary = 8;
			dirsize = sizeof(u_short);
			break;
		default:
			headbyte = 0;
			boundary = 4;
			dirsize = sizeof(u_long) + sizeof(u_short) * 2;
			break;
	}

	for (i = 0; i < max; i++) {
		if (strcmp(list[i].name, ".")
		&& strcmp(list[i].name, "..")) break;
	}
	top = i;
	len = getnamlen(realdirsiz(list[top].name));
	tmpdir = maketmpfile(len);
	if (mkdir(tmpdir, 0777) < 0) {
		warning(0, NOWRT_K);
		free(tmpdir);
		return;
	}
	persec = getblocksize(tmpdir);
	strcpy(path, tmpdir);
	strcat(path, "/");
	fnamp = strlen(path);
	waitmes();

	if (!(dirp = opendir("."))) error(".");
	i = ent = 0;
	while (dp = readdir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;
		else if (!strcmp(dp -> d_name, tmpdir)) ent = i;
		else {
			strcpy(path + fnamp, dp -> d_name);
			if (rename2(dp -> d_name, path) < 0) {
				closedir(dirp);
				warning(-1, dp -> d_name);
				restorefile(tmpdir, path, fnamp);
				return;
			}
		}
		i++;
	}
	closedir(dirp);

	if (ent > 0) {
		cp = maketmpfile(len);
		if (rename2(tmpdir, cp) < 0) error(tmpdir);
		free(tmpdir);
		tmpdir = cp;
		strcpy(path, tmpdir);
		strcat(path, "/");
	}

	entnum = getentnum(".", persec, fs);
	totalent = headbyte + realdirsiz(".") + realdirsiz("..")
		+ realdirsiz(tmpdir);
	block = tmpno = 0;
	ptr = 3;
	totalptr = entnum[block];
	tmpfile = NULL;
	maxtmp = 0;

	for (i = 0; i < max; i++) {
		if (!strcmp(list[i].name, ".")
		|| !strcmp(list[i].name, "..")
		|| i == top) continue;

		ent = persec - totalent;
		switch (fs) {
			case 2:	/* IRIX File System */
 				if (totalptr > ptr + 1) ent -= totalptr;
 				else ent -= ptr + 1;
				break;
			case 3:	/* SystemV R3 File System */
				ent = realdirsiz(list[i].name);
				break;
			default:
				break;
		}
		if (ent < realdirsiz(list[i].name)) {
			tmpfile = (char **)addlist(tmpfile, tmpno,
				&maxtmp, sizeof(char *));
			tmpfile[tmpno] = maketmpfile(getnamlen(ent));
			if (tmpfile[tmpno]) touch(tmpfile[tmpno]);
			tmpno++;
			ptr = 0;
			totalent = headbyte;
			totalptr = entnum[++block];
		}
		strcpy(path + fnamp, list[i].name);
		if (rename2(path, list[i].name) < 0) error(path);
		totalent += realdirsiz(list[i].name);
		ptr++;
	}
	free(entnum);

	cp = maketmpfile(len);
	if (rename2(tmpdir, cp) < 0) error(tmpdir);
	free(tmpdir);
	tmpdir = cp;
	strcpy(path, tmpdir);
	strcat(path, "/");
	strcat(path, list[top].name);
	if (rename2(path, list[top].name) < 0) error(path);
	restorefile(tmpdir, path, fnamp);

	for (i = 0; i < tmpno; i++) if (tmpfile[i]) {
		if (unlink(tmpfile[i]) < 0) error(tmpfile[i]);
		free(tmpfile[i]);
	}
	free(tmpfile);
}
