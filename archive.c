/*
 *	archive.c
 *
 *	Archive File Access Module
 */

#include "fd.h"
#include "term.h"
#include "funcno.h"
#include "kanji.h"

#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>

#ifdef	USEDIRECT
#include <sys/dir.h>
#else
#include <dirent.h>
#endif

extern bindtable bindlist[];
extern functable funclist[];
extern int filepos;
extern int mark;
extern int fnameofs;
extern int sorton;
extern char fullpath[];
extern char *findpattern;
extern char **path_history;

#define	PM_LHA	2, 2,\
		{0, 1, 1, 2, -1, 4, 5, 6, 7},\
		{0, 0, '/', 0, 0, 0, 0, 0, 0},\
		{0, '/', 0, 0, 0, 0, 0, 0, 0},\
		{-1, -1, -1}
#ifdef	TARUSESPACE
#define	PM_TAR	0, 0,\
		{0, 1, 1, 2, 6, 3, 4, 5, 7},\
		{0, 0, '/', 0, 0, 0, 0, 0, 0},\
		{0, '/', 0, 0, 0, 0, 0, 0, 0},\
		{-1, -1, -1}
#else
#define	PM_TAR	0, 0,\
		{0, 1, 1, 2, 6, 3, 4, 5, 7},\
		{0, 0, '/', 0, 0, 0, 0, 0, 0},\
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0},\
		{9, -1, -1}
#endif

static int countfield();
static char *getfield();
static namelist *readfileent();
static VOID archbar();
static char *pullfilename();
static int archbrowse();

char *archivefile = NULL;
char *archivedir;
int maxlaunch;
int maxarchive;
launchtable launchlist[MAXLAUNCHTABLE] = {
	{"^.*\\.lzh$",		"lha l",		PM_LHA},
	{"^.*\\.tar$",		"tar tvf", 		PM_TAR},
	{"^.*\\.tar\\.Z$",	"zcat %C | tar tvf -",	PM_TAR},
	{"^.*\\.tar\\.gz$",	"gunzip -c %C | tar tvf -", PM_TAR},
	{"^.*\\.Z$",		"zcat %C | $PAGER",	-1, 0, "", "", "", ""},
	{"^.*\\.gz$",		"gunzip -c %C | $PAGER",-1, 0, "", "", "", ""},
	{NULL,			NULL,			-1, 0, "", "", "", ""}
};
archivetable archivelist[MAXARCHIVETABLE] = {
	{"^.*\\.lzh$",		"lha aq %C %TA",	"lha xq %C %TA"},
	{"^.*\\.tar$",		"tar cf %C %T",		"tar xf %C %TA"},
	{"^.*\\.tar\\.Z$",	"tar cf %X %T; compress %X",
						"zcat %C | tar xf - %TA"},
	{"^.*\\.tar\\.gz$",	"tar cf %X %T; gzip %X",
						"gunzip -c %C | tar xf - %TA"},
	{NULL,			NULL,			NULL}
};

static namelist *arcflist;
static int maxarcf;

static int countfield(line, sep, field)
char *line, *sep;
int field;
{
	int i, j, f, s;

	j = 0;
	f = -1;
	s = 1;
	for (i = 0; line[i]; i++) {
		if (line[i] == ' ' || line[i] == '\t') s = 1;
		else if (s) {
			f++;
			s = 0;
		}
		if (sep[j] >= 0 && i >= sep[j]) {
			j++;
			for (; sep[j] < 0 || i < sep[j]; i++)
				if (line[i] != ' ' && line[i] != '\t') break;
			s = 0;
			if (f < 0) f = 1;
			else f++;
		}
		if (field >= 0) {
			if (f == field) return(i);
			else if (f > field) return(-1);
		}
	}
	return(f);
}

static char *getfield(buf, line, field, delim, width, sep)
char *buf, *line;
int field, delim;
u_char width;
char *sep;
{
	char *cp, *end;
	int i;

	*buf = '\0';

	if (field < 0 || (i = countfield(line, sep, field)) < 0) return(buf);
	cp = &line[i];

	if (delim) {
		if (cp = strchr(cp, delim)) cp++;
		else return(buf);
	}

	if (width >= 128) i = width - 128;
	else {
		end = NULL;
		if (width) end = strchr(cp, width);
		if (!end) end = strpbrk(cp, " \t");
		if (end) i = end - cp;
		else i = strlen(cp);
	}

	strncpy(buf, cp, i);
	buf[i] = '\0';

	return(buf);
}

static namelist *readfileent(line, list, max)
char *line;
launchtable *list;
int max;
{
	namelist *tmp;
	struct tm tm;
	int i, ofs, skip;
	char *cp, buf[MAXLINESTR + 1];

	cp = line;
	i = countfield(line, list -> sep, -1);
	skip = (max > i) ? max - i : 0;

	getfield(buf, line, list -> field[F_NAME] - skip,
		list -> delim[F_NAME], list -> width[F_NAME], list -> sep);
	if (!*buf) return(NULL);
	tmp = (namelist *)malloc2(sizeof(namelist));
	tmp -> name = strdup2(buf);

	getfield(buf, line, list -> field[F_MODE] - skip,
		list -> delim[F_MODE], list -> width[F_MODE], list -> sep);
	if ((ofs = strlen(buf) - 9) < 0) ofs = 0;
	tmp -> st_mode = 0;
	for (i = 0; i < 9; i++) {
		tmp -> st_mode *= 2;
		if (buf[ofs + i] != '-') tmp -> st_mode += 1;
	}
	if (buf[ofs + 2] == 's') tmp -> st_mode |= S_ISUID;
	if (buf[ofs + 5] == 's') tmp -> st_mode |= S_ISGID;
	if (buf[ofs + 8] == 't') tmp -> st_mode |= S_ISVTX;

	tmp -> st_nlink = 1;
	tmp -> st_uid = atoi2(getfield(buf, line, list -> field[F_UID] - skip,
		list -> delim[F_UID], list -> width[F_UID], list -> sep));
	tmp -> st_gid = atoi2(getfield(buf, line, list -> field[F_GID] - skip,
		list -> delim[F_GID], list -> width[F_GID], list -> sep));
	tmp -> st_size = atol(getfield(buf, line, list -> field[F_SIZE] - skip,
		list -> delim[F_SIZE], list -> width[F_SIZE], list -> sep));
	if (tmp -> st_mode <= 0 && tmp -> st_uid <= 0 && !tmp -> st_gid) {
		free(tmp -> name);
		free(tmp);
		return(NULL);
	}

	getfield(buf, line, list -> field[F_YEAR] - skip,
		list -> delim[F_YEAR], list -> width[F_YEAR], list -> sep);
	tm.tm_year = (*buf) ? atoi(buf) : 1970;
	if (tm.tm_year < 100 && (tm.tm_year += 1900) < 1970) tm.tm_year += 100;
	getfield(buf, line, list -> field[F_MON] - skip,
		list -> delim[F_MON], list -> width[F_MON], list -> sep);
	if (!strncmp(buf, "Jan", 3)) tm.tm_mon = 0;
	else if (!strncmp(buf, "Feb", 3)) tm.tm_mon = 1;
	else if (!strncmp(buf, "Mar", 3)) tm.tm_mon = 2;
	else if (!strncmp(buf, "Apr", 3)) tm.tm_mon = 3;
	else if (!strncmp(buf, "May", 3)) tm.tm_mon = 4;
	else if (!strncmp(buf, "Jun", 3)) tm.tm_mon = 5;
	else if (!strncmp(buf, "Jul", 3)) tm.tm_mon = 6;
	else if (!strncmp(buf, "Aug", 3)) tm.tm_mon = 7;
	else if (!strncmp(buf, "Sep", 3)) tm.tm_mon = 8;
	else if (!strncmp(buf, "Oct", 3)) tm.tm_mon = 9;
	else if (!strncmp(buf, "Nov", 3)) tm.tm_mon = 10;
	else if (!strncmp(buf, "Dec", 3)) tm.tm_mon = 11;
	else if ((tm.tm_mon = atoi(buf)) > 0) tm.tm_mon--;
	tm.tm_mday = atoi(getfield(buf, line, list -> field[F_DAY] - skip,
		list -> delim[F_DAY], list -> width[F_DAY], list -> sep));

	getfield(buf, line, list -> field[F_TIME] - skip,
		 list -> delim[F_TIME], list -> width[F_TIME], list -> sep);
	tm.tm_hour = atoi(buf);
	cp = strchr(buf, ':');
	tm.tm_min = (cp) ? atoi(++cp) : 0;
	tm.tm_sec = (cp && (cp = strchr(cp, ':'))) ? atoi(++cp) : 0;

	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags = 0;

	return(tmp);
}

static VOID archbar(file, dir)
char *file, *dir;
{
	char *arch;

	arch = (char *)malloc2(strlen(fullpath)
		+ strlen(file) + strlen(dir) + 3);
	strcpy(arch, fullpath);
	strcat(arch, "/");
	strcat(arch, file);
	strcat(arch, ":");
	strcat(arch, dir);

	locate(0, LPATH);
	putterm(l_clear);
	putch(' ');
	putterm(t_standout);
	cputs("Arch:");
	putterm(end_standout);
	cputs2(arch, n_column - 6, 0);
	free(arch);

	tflush();
}

static char *pullfilename(dir, list)
char *dir;
namelist *list;
{
	char *cp, *tmp;
	int len;

	cp = list -> name;
	if ((len = strlen(dir)) > 0) {
		if (strncmp(cp, dir, len) || cp[len] != '/') return(NULL);
		cp += len + 1;
	}
	if (!*cp) {
		list -> st_mode |= S_IFDIR;
		list -> flags |= F_ISDIR;
		return(strdup2(".."));
	}
	if (tmp = strchr(cp, '/')) {
		if (*(tmp + 1)) return(NULL);
		*tmp = '\0';
		list -> st_mode |= S_IFDIR;
		list -> flags |= F_ISDIR;
	}
	return(strdup2(cp));
}

VOID rewritearc()
{
	archbar(archivefile, archivedir);
	helpbar();
	statusbar(maxarcf);
	locate(0, LSTACK);
	putterm(l_clear);
	movepos(arcflist, maxarcf, filepos, LISTUP);
	locate(0, 0);
	tflush();
}

static int archbrowse(file, list, maxarcentp)
char *file;
launchtable *list;
int *maxarcentp;
{
	namelist *tmp;
	reg_t *re;
	FILE *fp;
	u_char fstat;
	char *cp, line[MAXLINESTR + 1];
	int ch, i, no, old, curdir, max;

	if (!(cp = evalcommand(list -> comm, archivefile, NULL, 0, 0))) {
		warning(E2BIG, list -> comm);
		return(-1);
	}
	fp = popen(cp + 2, "r");
	free(cp);
	locate(0, LMESLINE);
	putterm(l_clear);
	cputs(WAIT_K);
	tflush();

	max = 0;
	for (i = 0; i < MAXLAUNCHFIELD; i++)
		if (list -> field[i] >= 0 && list -> field[i] > max)
			max = list -> field[i];

	if (!findpattern) re = NULL;
	else {
		cp = cnvregexp(findpattern);
		re = regexp_init(cp);
		free(cp);
	}
	for (i = 0; i < list -> topskip; i++)
		if (!fgets(line, MAXLINESTR, fp)) break;

	maxarcf = curdir = 0;
	while (fgets(line, MAXLINESTR, fp)) {
		if (cp = strchr(line, '\n')) *cp = '\0';
		if (!(tmp = readfileent(line, list, max))) break;

		if (!curdir
		&& (!*archivedir || !strcmp(archivedir, "."))
		&& !strncmp(tmp -> name, "./", 2)) {
			arcflist = (namelist *)addlist(arcflist, maxarcf,
				maxarcentp, sizeof(namelist));
			memcpy(&arcflist[maxarcf], tmp, sizeof(namelist));
			arcflist[maxarcf].st_mode |= S_IFDIR;
			arcflist[maxarcf].flags |= F_ISDIR;
			arcflist[maxarcf].name =
				strdup2(*archivedir ? ".." : ".");
			arcflist[maxarcf].ent = maxarcf;
			maxarcf++;
			curdir++;
		}
		cp = pullfilename(archivedir, tmp);
		free(tmp -> name);
		if (cp) tmp -> name = cp;
		else {
			free(tmp);
			continue;
		}

		if (!re || regexp_exec(re, cp)) {
			arcflist = (namelist *)addlist(arcflist, maxarcf,
				maxarcentp, sizeof(namelist));
			memcpy(&arcflist[maxarcf], tmp, sizeof(namelist));
			arcflist[maxarcf].ent = maxarcf;
			maxarcf++;
		}
		free(tmp);
	}
	pclose(fp);
	if (re) regexp_free(re);
	for (i = 0; i < list -> bottomskip; i++) {
		if (--maxarcf < 0) break;
		free(arcflist[maxarcf].name);
	}
	if (maxarcf <= 0) return(-1);

	title();
	archbar(archivefile, archivedir);
	statusbar(maxarcf);
	locate(0, LSTACK);
	putterm(l_clear);
	helpbar();

	old = filepos = listupfile(arcflist, maxarcf, file);
	fstat = 0;
	no = 1;

	keyflush();
	for (;;) {
		if (no) movepos(arcflist, maxarcf, old, fstat);
		locate(0, 0);
		tflush();
		ch = getkey(SIGALRM);

		old = filepos;
		for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			if (ch == (int)bindlist[i].key) break;
		no = (bindlist[i].d_func > 0 && isdir(&arcflist[filepos])) ?
			bindlist[i].d_func : bindlist[i].f_func;
		if (no > NO_OPERATION) continue;
		fstat = funclist[no].stat;
		if (!(fstat & ARCH)) no = 0;
		else no = (*funclist[no].func)(arcflist, &maxarcf);

		if (no < 0) break;
		if (no == 1 || no == 3) helpbar();
		if (no < 2) {
			fstat = 0;
			continue;
		}
		else if (no >= 4) break;

		if (!(fstat & REWRITE)) fnameofs = 0;
		if ((fstat & (REWRITE | LISTUP)) == (REWRITE | LISTUP)) {
			title();
			archbar(archivefile, archivedir);
			statusbar(maxarcf);
			locate(0, LSTACK);
			putterm(l_clear);
			helpbar();
		}
	}

	if (no > 4) {
		if (strcmp(arcflist[filepos].name, "..")) {
			if (*archivedir) strcat(archivedir, "/");
			strcat(archivedir, arcflist[filepos].name);
			*file = '\0';
		}
		else if (!*archivedir) no = -1;
		else {
			if (!(cp = strrchr(archivedir, '/'))) cp = archivedir;
			strcpy(file, cp + 1);
			*cp = '\0';
		}
	}
	else strcpy(file, arcflist[filepos].name);
	for (i = 0; i < maxarcf; i++) free(arcflist[i].name);
	return(no);
}

int launcher(list, max)
namelist *list;
int max;
{
	reg_t *re;
	char *dir, *dupfindpat, path[MAXPATHLEN + 1], file[MAXNAMLEN + 1];
	int i, dupfilepos, dupsorton, maxarcent;

	for (i = 0; i < maxlaunch; i++) {
		re = regexp_init(launchlist[i].ext);
		if (regexp_exec(re, list[filepos].name)) break;
		regexp_free(re);
	}
	if (i >= maxlaunch) return(-1);
	regexp_free(re);

	if (archivefile) {
		if (launchlist[i].topskip >= 0 ||
		!(dir = tmpunpack(list, max))) {
			putterm(t_bell);
			return(1);
		}
		execmacro(launchlist[i].comm, list[filepos].name,
			NULL, 0, -1, 0);
		removetmp(dir, list[filepos].name);
		return(0);
	}
	if (launchlist[i].topskip < 0) {
		execmacro(launchlist[i].comm, list[filepos].name,
			NULL, 0, -1, 0);
		return(0);
	}

	archivefile = list[filepos].name;
	archivedir = path;
	arcflist = NULL;
	maxarcent = 0;
	dupfilepos = filepos;
	dupfindpat = findpattern;
	dupsorton = sorton;
	findpattern = NULL;
	sorton = 0;

	*path = *file = '\0';
	while (archbrowse(file, &launchlist[i], &maxarcent) >= 0);

	archivefile = NULL;
	free(arcflist);
	filepos = dupfilepos;
	findpattern = dupfindpat;
	sorton = dupsorton;

	return(0);
}

int pack(arc, list, max)
char *arc;
namelist *list;
int max;
{
	reg_t *re;
	int i;

	for (i = 0; i < maxarchive; i++) {
		re = regexp_init(archivelist[i].ext);
		if (regexp_exec(re, arc)) break;
		regexp_free(re);
	}
	if (i >= maxarchive) return(-1);
	regexp_free(re);

	execmacro(archivelist[i].p_comm, arc, list, max, -1, 1);
	return(1);
}

int unpack(arc, dir, list, max, tr)
char *arc, *dir;
namelist *list;
int max, tr;
{
	reg_t *re;
	char path[MAXPATHLEN + 1];
	int i;

	for (i = 0; i < maxarchive; i++) {
		re = regexp_init(archivelist[i].ext);
		if (regexp_exec(re, arc)) break;
		regexp_free(re);
	}
	if (i >= maxarchive) return(-1);
	regexp_free(re);

	if (dir) strcpy(path, dir);
	else {
		if (tr) dir = tree();
		else {
			dir = inputstr2(UNPAC_K, -1, NULL, path_history);
			path_history = entryhist(path_history, path);
			dir = evalpath(dir);
		}
		if (!dir) return(0);
		strcpy(path, dir);
		free(dir);
	}

	if (chdir(path) < 0) {
		warning(-1, path);
		return(0);
	}

	strcpy(path, fullpath);
	strcat(path, "/");
	strcat(path, arc);
	if (execmacro(archivelist[i].u_comm, path, list, max, -1, 1) == 127) {
		warning(E2BIG, archivelist[i].u_comm);
		return(0);
	}
	if (chdir(fullpath) < 0) error(fullpath);
	return(1);
}

char *tmpunpack(list, max)
namelist *list;
int max;
{
	char *cp, path[MAXPATHLEN + 1];
	int i, dupmark;

	if (!(cp = getenv2("FD_TMPDIR"))) cp = TMPDIR;
	sprintf(path, "%s/fd.%d", cp, getpid());
	if (mkdir(path, 0777) < 0) {
		warning(-1, path);
		return(NULL);
	}

	dupmark = mark;
	mark = 0;
	i = unpack(archivefile, path, list, max, 0);
	mark = dupmark;

	if (chdir(path) < 0) error(path);
	cp = strdup2(path);
	if (i > 0) {
		if (!*archivedir || chdir(archivedir) >= 0) return(cp);
		warning(-1, archivedir);
	}
	else if (i < 0) putterm(t_bell);
	removetmp(cp, NULL);
	return(NULL);
}

VOID removetmp(dir, file)
char *dir, *file;
{
	char *cp, *tmp, *dupdir;

	if (file && unlink(file) < 0) error(file);
	if (*archivedir) {
		if (chdir(dir) < 0) error(dir);
		dupdir = strdup2(archivedir);
		cp = dupdir + strlen(dupdir);
		for (;;) {
			*cp = '\0';
			tmp = strrchr(dupdir, '/');
			if (tmp) tmp++;
			else tmp = dupdir;
			if (strcmp(tmp, ".") && rmdir(dupdir) < 0)
				error(dupdir);
			if (tmp == dupdir) break;
			cp = tmp - 1;
		}
		free(dupdir);
	}
	if (chdir(fullpath) < 0) error(fullpath);
	if (rmdir(dir) < 0) error(dir);
	free(dir);
}

int backup(dev, list, max)
char *dev;
namelist *list;
int max;
{
	char flag, *tmp;

	if (!(tmp = evalcommand("tar cf %C %TA", dev, list, max, 0))) {
		warning(E2BIG, dev);
		return(0);
	}
	flag = *tmp;
	for (;;) {
		system2(tmp + 2, -1);
		free(tmp);
		if (!(flag & F_REMAIN)
		|| !(tmp = evalcommand("tar rf %C %TA", dev, list, max, 0)))
			return(1);
		flag = *tmp;
	}
}
