/*
 *	file.c
 *
 *	File Access Module
 */

#include "fd.h"
#include "term.h"
#include "kanji.h"

#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>

extern int filepos;
extern int mark;
extern int lnstat;
extern int sorton;
extern char *destpath;
extern int copypolicy;
extern char fullpath[];

#ifdef	IRIXFS
#define	realdirsiz(name)		(((strlen(name) + 2) & ~1)\
					+ sizeof(u_long))
#define	getptrsiz(total, ptr)		(((total) > (ptr)) ? (total) : (ptr))
#define	getnamlen(ent)			((((ent) - sizeof(u_long)) & ~1) - 1)
#define	HEADBYTE			4
#else
#define	realdirsiz(name)		(((strlen(name) + 4) & ~3)\
					+ sizeof(u_long)\
					+ sizeof(u_short) + sizeof(u_short))
#define	getptrsiz(total, ptr)		0
#define	getnamlen(ent)			((((ent) - sizeof(u_long)\
					- sizeof(u_short) - sizeof(u_short))\
					& ~3) - 1)
#define	HEADBYTE			0
#endif

static char *getdistdir();
static VOID touch();
static char *maketmpfile();
static char *getentnum();
static VOID restorefile();


VOID getstatus(list, i, file)
namelist *list;
int i;
char *file;
{
	struct stat status, lstatus;

	if (lstat(file, &lstatus) < 0) error(file);
	if (stat2(file, &status) < 0) error(file);
	list[i].st_mode = lstatus.st_mode;
	list[i].flags = 0;
	if ((status.st_mode & S_IFMT) == S_IFDIR) list[i].flags |= F_ISDIR;
	if ((lstatus.st_mode & S_IFMT) == S_IFLNK) list[i].flags |= F_ISLNK;

	if (lnstat) memcpy(&lstatus, &status, sizeof(struct stat));

	list[i].st_nlink = lstatus.st_nlink;
	list[i].st_uid = lstatus.st_uid;
	list[i].st_gid = lstatus.st_gid;
	list[i].st_size = lstatus.st_size;
	list[i].st_mtim = lstatus.st_mtime;
}

int cmplist(listp1, listp2)
namelist *listp1, *listp2;
{
	char *cpi, *cpj;
	int tmp;

	if (!sorton) return(listp1 -> ent - listp2 -> ent);
	if (!isdir(listp1) && isdir(listp2)) return(1);
	if (isdir(listp1) && !isdir(listp2)) return(-1);

	if (listp1 -> name[0] == '.'
		&& (listp1 -> name[1] == '\0'
		|| (listp1 -> name[1] == '.' && listp1 -> name[2] == '\0')))
			return(-1);
	if (listp2 -> name[0] == '.'
		&& (listp2 -> name[1] == '\0'
		|| (listp2 -> name[1] == '.' && listp2 -> name[2] == '\0')))
			return(1);

	switch (sorton & 7) {
		case 1:
			tmp = strcmp(listp1 -> name, listp2 -> name);
			break;
		case 2:
			if (cpi = strrchr(listp1 -> name, '.')) cpi++;
			else cpi = "";
			if (cpj = strrchr(listp2 -> name, '.')) cpj++;
			else cpj = "";
			tmp = strcmp(cpi, cpj);
			break;
		case 3:
			tmp = listp1 -> st_size - listp2 -> st_size;
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

struct dirent *searchdir(dirp, regexp)
DIR *dirp;
reg_t *regexp;
{
	struct dirent *dp;

	if (!regexp) dp = readdir(dirp);
	else while (dp = readdir(dirp)) {
		if (regexp_exec(regexp, dp -> d_name)) break;
	}
	return(dp);
}

int underhome()
{
	static char *homedir = NULL;
	struct passwd *pwd;
	char *cp;

	if (!homedir) {
		if (!(homedir = getenv("HOME"))) {
			if (pwd = getpwuid(getuid())) homedir = pwd -> pw_dir;
			else return(-1);
		}
		if (chdir(homedir) < 0) return(-1);
		homedir = getwd2();
		if (chdir(fullpath) < 0) error(fullpath);
	}
	cp = getwd2();
	if (strncmp(cp, homedir, strlen(homedir))) return(0);
	else return(1);
}

static char *getdistdir(mes)
char *mes;
{
	struct stat status;
	char *dir;

	if (!(dir = evalpath(inputstr2(mes, -1, NULL, NULL)))) return(NULL);
	if (!*dir) {
		free(dir);
		dir = strdup2(".");
	}

	if (stat2(dir, &status) < 0) {
		if (errno != ENOENT || mkdir(dir, 0777) < 0) {
			warning(-1, dir);
			free(dir);
			return(NULL);
		}
		if (stat2(dir, &status) < 0) error(dir);
	}
	if ((status.st_mode & S_IFMT) != S_IFDIR) {
		warning(ENOTDIR, dir);
		free(dir);
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
	destpath = (tr) ? tree() : getdistdir(COPYD_K);
	if (!destpath) return((tr) ? 3 : 1);
	copypolicy = 0;
	if (mark > 0) applyfile(list, max, cpfile, ENDCP_K);
	else if (isdir(&list[filepos]) && !islink(&list[filepos]))
		applydir(list[filepos].name, cpfile, cpdir, NULL, ENDCP_K);
	else if (cpfile(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	free(destpath);
	return(3);
}

int movefile(list, max, tr)
namelist *list;
int max, tr;
{
	destpath = (tr) ? tree() : getdistdir(MOVED_K);
	if (!destpath) return((tr) ? 3 : 1);
	copypolicy = 0;
	if (mark > 0) filepos = applyfile(list, max, mvfile, ENDMV_K);
	else if (mvfile(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	else filepos++;
	if (filepos >= max) filepos = max - 1;
	free(destpath);
	return(4);
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

static char *getentnum(dir, bsiz)
char *dir;
int bsiz;
{
	FILE *fp;
	struct stat status;
	char *tmp;
	int i, n;

	if (lstat(dir, &status) < 0) error(dir);
	n = status.st_size / bsiz;
	tmp = (char *)malloc2(n + 1);

#ifdef	IRIXFS
	fp = fopen(dir, "r");
	for (i = 0; i < n; i++) {
		fseek(fp, i * bsiz + 3, 0);
		tmp[i] = fgetc(fp) + 1;
	}
	tmp[i] = 0;
	fclose(fp);
#else
	for (i = 0; i <= n; i++) tmp[i] = 0;
#endif

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

VOID arrangedir(list, max)
namelist *list;
int max;
{
	DIR *dirp;
	struct dirent *dp;
	char **tmpfile;
	int maxtmp;
	int persec;
	int i, top, len, fnamp, tmpno, block, ent, totalent, ptr, totalptr;
	char *cp, *tmpdir, *entnum, path[MAXPATHLEN + 1];

	persec = getblocksize();
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
	strcpy(path, tmpdir);
	strcat(path, "/");
	fnamp = strlen(path);
	locate(0, LMESLINE);
	putterm(l_clear);
	cputs(WAIT_K);
	tflush();

	if (!(dirp = opendir("."))) error(".");
	i = 0;
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

	entnum = getentnum(".", persec);
	totalent = HEADBYTE + realdirsiz(".") + realdirsiz("..")
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

		ent = persec - totalent - getptrsiz(totalptr, ptr + 1);
		if (ent < realdirsiz(list[i].name)) {
			tmpfile = (char **)addlist(tmpfile, tmpno,
				&maxtmp, sizeof(char *));
			tmpfile[tmpno] = maketmpfile(getnamlen(ent));
			if (tmpfile[tmpno]) touch(tmpfile[tmpno]);
			tmpno++;
			ptr = 0;
			totalent = HEADBYTE;
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
	restorefile(tmpdir, path, fnamp, tmpfile, tmpno);

	for (i = 0; i < tmpno; i++) if (tmpfile[i]) {
		if (unlink(tmpfile[i]) < 0) error(tmpfile[i]);
		free(tmpfile[i]);
	}
	free(tmpfile);
}
