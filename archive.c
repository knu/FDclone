/*
 *	archive.c
 *
 *	Archive File Access Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>

#ifdef	USETIMEH
#include <time.h>
#endif

extern bindtable bindlist[];
extern functable funclist[];
extern int filepos;
extern int mark;
extern int fnameofs;
extern int sorton;
extern char fullpath[];
extern char *findpattern;
extern char *deftmpdir;

#define	PM_LHA	2, 2,\
		{0, 1, 1, 2, 6, 4, 5, 6, 7},\
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
static char *pulldirname();
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
static int launchlevel = 0;


static int countfield(line, sep, field)
char *line;
u_char *sep;
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
		if (sep[j] < 255 && i >= (int)(sep[j])) {
			j++;
			for (; sep[j] == 255 || i < (int)(sep[j]); i++)
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

static char *getfield(buf, line, field, skip, delim, width, sep)
char *buf, *line;
u_char field;
int skip, delim;
u_char width;
u_char *sep;
{
	char *cp, *end;
	int i;

	*buf = '\0';
	skip = (field < 255) ? (int)field - skip : -1;

	if (skip < 0 || (i = countfield(line, sep, skip)) < 0) return(buf);
	cp = &line[i];

	if (delim) {
		if (cp = strchr(cp, delim)) cp++;
		else return(buf);
	}

	if (width >= 128) i = width - 128;
	else {
		end = NULL;
		if (width) end = strchr2(cp, width);
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
	struct passwd *pw;
	struct group *gr;
	int i, ofs, skip;
	char *cp, buf[MAXLINESTR + 1];

	cp = line;
	i = countfield(line, list -> sep, -1);
	skip = (max > i) ? max - i : 0;

	getfield(buf, line, list -> field[F_NAME], skip,
		list -> delim[F_NAME], list -> width[F_NAME], list -> sep);
	if (!*buf) return(NULL);
	tmp = (namelist *)malloc2(sizeof(namelist));
	tmp -> name = strdup2(buf);

	getfield(buf, line, list -> field[F_MODE], skip,
		list -> delim[F_MODE], list -> width[F_MODE], list -> sep);
	for (i = ofs = strlen(buf); i < 9; i++) buf[i] = '\0';
	if ((ofs -= 9) < 0) ofs = 0;
	tmp -> st_mode = 0;
	for (i = 0; i < 9; i++) {
		tmp -> st_mode *= 2;
		if (buf[ofs + i] != '-') tmp -> st_mode += 1;
	}
	if (buf[ofs + 2] == 's') tmp -> st_mode |= S_ISUID;
	if (buf[ofs + 5] == 's') tmp -> st_mode |= S_ISGID;
	if (buf[ofs + 8] == 't') tmp -> st_mode |= S_ISVTX;
	tmp -> flags = 0;
	if (ofs > 0) {
		switch (buf[ofs - 1]) {
			case 'd':
				tmp -> st_mode |= S_IFDIR;
				tmp -> flags |= F_ISDIR;
				break;
			case 'b':
				tmp -> st_mode |= S_IFBLK;
				break;
			case 'c':
				tmp -> st_mode |= S_IFCHR;
				break;
			case 'l':
				tmp -> st_mode |= S_IFLNK;
				tmp -> flags |= F_ISLNK;
				break;
			case 's':
				tmp -> st_mode |= S_IFSOCK;
				break;
			case 'p':
				tmp -> st_mode |= S_IFIFO;
				break;
			default:
				break;
		}
	}

	tmp -> st_nlink = 1;
	getfield(buf, line, list -> field[F_UID], skip,
		list -> delim[F_UID], list -> width[F_UID], list -> sep);
	if (isdigit(*buf)) tmp -> st_uid = atoi2(buf);
	else tmp -> st_uid = (pw = getpwnam(buf)) ? pw -> pw_uid : -1;
	getfield(buf, line, list -> field[F_GID], skip,
		list -> delim[F_GID], list -> width[F_GID], list -> sep);
	if (isdigit(*buf)) tmp -> st_gid = atoi2(buf);
	else tmp -> st_gid = (gr = getgrnam(buf)) ? gr -> gr_gid : -1;
	tmp -> st_size = atol(getfield(buf, line, list -> field[F_SIZE], skip,
		list -> delim[F_SIZE], list -> width[F_SIZE], list -> sep));
	if (tmp -> st_mode <= 0 && tmp -> st_uid <= 0 && !tmp -> st_gid) {
		free(tmp -> name);
		free(tmp);
		return(NULL);
	}

	getfield(buf, line, list -> field[F_YEAR], skip,
		list -> delim[F_YEAR], list -> width[F_YEAR], list -> sep);
	tm.tm_year = (*buf) ? atoi(buf) : 1970;
	if (tm.tm_year < 100 && (tm.tm_year += 1900) < 1970) tm.tm_year += 100;
	getfield(buf, line, list -> field[F_MON], skip,
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
	tm.tm_mday = atoi(getfield(buf, line, list -> field[F_DAY], skip,
		list -> delim[F_DAY], list -> width[F_DAY], list -> sep));

	getfield(buf, line, list -> field[F_TIME], skip,
		list -> delim[F_TIME], list -> width[F_TIME], list -> sep);
	tm.tm_hour = atoi(buf);
	if (tm.tm_hour < 0 || tm.tm_hour > 23) tm.tm_hour = 0;
	cp = strchr(buf, ':');
	tm.tm_min = (cp) ? atoi(++cp) : 0;
	tm.tm_sec = (cp && (cp = strchr(cp, ':'))) ? atoi(++cp) : 0;

	tmp -> st_mtim = timelocal2(&tm);

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
	kanjiputs2(arch, n_column - 6, 0);
	free(arch);

	tflush();
}

static char *pulldirname(dir, list, dlist, max)
char *dir;
namelist *list;
char **dlist;
int max;
{
	char  *cp, *tmp;
	int i, len;

	cp = list -> name;
	if ((len = strlen(dir)) > 0) {
		if (strncmp(cp, dir, len) || (cp[len] && cp[len] != '/'))
			return(NULL);
		cp += (cp[len]) ? len + 1 : len;
	}
	if (!*cp || (!(tmp = strchr(cp, '/')) && !isdir(list))) {
		if (!*dir) return(NULL);
		for (i = 0; i < max; i++) if (!strcmp(dlist[i], ".."))
			return(NULL);
		return(strdup2(".."));
	}
	len = (tmp) ? tmp - cp : strlen(cp);
	for (i = 0; i < max; i++) if (!strncmp(dlist[i], cp, len)) return(NULL);
	tmp = (char *)malloc2(len + 1);
	strncpy2(tmp, cp, len);
	return(tmp);
}

static char *pullfilename(dir, list)
char *dir;
namelist *list;
{
	char *cp;
	int len;

	cp = list -> name;
	if ((len = strlen(dir)) > 0) {
		if (strncmp(cp, dir, len) || cp[len] != '/') return(NULL);
		cp += len + 1;
	}
	if (!*cp || isdir(list) || strchr(cp, '/')) return(NULL);
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
	char **dirlist;
	char *cp, line[MAXLINESTR + 1];
	int ch, i, no, old, max, dmax, dmaxent;

	if (!(cp = evalcommand(list -> comm, archivefile, NULL, 0, NULL))) {
		warning(E2BIG, list -> comm);
		return(-1);
	}
	fp = popen(cp, "r");
	free(cp);
	waitmes();

	max = 0;
	for (i = 0; i < MAXLAUNCHFIELD; i++)
		if (list -> field[i] < 255 && (int)(list -> field[i]) > max)
			max = (int)(list -> field[i]);

	if (!findpattern) re = NULL;
	else {
		cp = cnvregexp(findpattern, 1);
		re = regexp_init(cp);
		free(cp);
	}
	for (i = 0; i < (int)(list -> topskip); i++)
		if (!fgets(line, MAXLINESTR, fp)) break;

	maxarcf = no = dmax = dmaxent = 0;
	dirlist = NULL;
	while (fgets(line, MAXLINESTR, fp)) {
		no++;
		if (cp = strchr(line, '\n')) *cp = '\0';
		if (!(tmp = readfileent(line, list, max))) continue;

		if (cp = pulldirname(archivedir, tmp, dirlist, dmax)) {
			dirlist = (char **)addlist(dirlist, dmax,
				&dmaxent, sizeof(char *));
			dirlist[dmax] = strdup2(cp);
			dmax++;
			arcflist = (namelist *)addlist(arcflist, maxarcf,
				maxarcentp, sizeof(namelist));
			memcpy(&arcflist[maxarcf], tmp, sizeof(namelist));
			arcflist[maxarcf].st_mode |= S_IFDIR;
			arcflist[maxarcf].flags |= F_ISDIR;
			arcflist[maxarcf].name = cp;
			arcflist[maxarcf].ent = no;
			maxarcf++;
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
			arcflist[maxarcf].ent = no;
			maxarcf++;
		}
		free(tmp);
	}
	if (re) regexp_free(re);
	for (i = 0; i < (int)(list -> bottomskip); i++) {
		if (maxarcf < 1
		|| arcflist[maxarcf - 1].ent <= no - (int)(list -> bottomskip))
			break;
		free(arcflist[--maxarcf].name);
	}
	for (i = 0; i < maxarcf; i++) arcflist[i].ent = i;
	if (maxarcf <= 0) {
		maxarcf = 0;
		arcflist = (namelist *)addlist(arcflist, 0,
			maxarcentp, sizeof(namelist));
		arcflist[0].name = NOFIL_K;
		arcflist[0].st_nlink = -1;
	}
	for (i = 0; i < dmax; i++) free(dirlist[i]);
	if (dirlist) free(dirlist);

	if (pclose(fp)) {
		warning(0, HITKY_K);
		for (i = 0; i < maxarcf; i++) free(arcflist[i].name);
		return(-1);
	}

	if (stable_standout) putterms(t_clear);
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
			if (ch == (int)(bindlist[i].key)) break;
		no = (bindlist[i].d_func < 255 && isdir(&arcflist[filepos])) ?
			(int)(bindlist[i].d_func) : (int)(bindlist[i].f_func);
		if (no > NO_OPERATION) continue;
		fstat = funclist[no].stat;
		if (!(fstat & ARCH) || (maxarcf <= 0 && !(fstat & NO_FILE)))
			no = 0;
		else no = (*funclist[no].func)(arcflist, &maxarcf);

		if (no < 0) break;
		if (no == 1 || no == 3) helpbar();
		if (no < 2) {
			fstat = 0;
			continue;
		}
		else if (no >= 4) break;

		if (!(fstat & REWRITE)) fnameofs = 0;
		if ((fstat & RELIST) == RELIST) {
			title();
			archbar(archivefile, archivedir);
			statusbar(maxarcf);
			locate(0, LSTACK);
			putterm(l_clear);
			helpbar();
		}
	}

	if (no <= 4) strcpy(file, arcflist[filepos].name);
	else if (strcmp(arcflist[filepos].name, "..")) {
		if (*archivedir) strcat(archivedir, "/");
		strcat(archivedir, arcflist[filepos].name);
		*file = '\0';
	}
	else if (!*archivedir) no = -1;
	else {
		if (cp = strrchr(archivedir, '/')) strcpy(file, cp + 1);
		else {
			strcpy(file, archivedir);
			cp = archivedir;
		}
		*cp = '\0';
	}
	for (i = 0; i < maxarcf; i++) free(arcflist[i].name);
	return(no);
}

int launcher(list, max)
namelist *list;
int max;
{
	reg_t *re;
	namelist *duparcflist;
	char *dupfullpath, *duparchivefile, *duparchivedir;
	char *dir, *dupfindpat, path[MAXPATHLEN + 1], file[MAXNAMLEN + 1];
	int i, dupmaxarcf, dupfilepos, dupsorton, maxarcent;

	for (i = 0; i < maxlaunch; i++) {
		re = regexp_init(launchlist[i].ext);
		if (regexp_exec(re, list[filepos].name)) break;
		regexp_free(re);
	}
	if (i >= maxlaunch) return(-1);
	regexp_free(re);

	if (archivefile && !(dir = tmpunpack(list, max))) {
		putterm(t_bell);
		return(1);
	}
	if (launchlist[i].topskip == 255) {
		execmacro(launchlist[i].comm, list[filepos].name,
			NULL, 0, 1, 0);
		if (archivefile) removetmp(dir, list[filepos].name);
		return(0);
	}

	if (archivefile) {
		dupfullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
		if (*archivedir) {
			strcat(fullpath, "/");
			strcat(fullpath, archivedir);
		}
		if (chdir(fullpath) < 0) error(fullpath);
	}
	duparchivefile = archivefile;
	duparchivedir = archivedir;
	duparcflist = arcflist;
	dupmaxarcf = maxarcf;
	dupfilepos = filepos;
	dupfindpat = findpattern;
	dupsorton = sorton;

	archivefile = list[filepos].name;
	archivedir = path;
	arcflist = NULL;
	maxarcent = 0;
	findpattern = NULL;
	sorton = 0;

	*path = *file = '\0';
	launchlevel++;
	while (archbrowse(file, &launchlist[i], &maxarcent) >= 0);
	launchlevel--;
	free(arcflist);

	archivefile = duparchivefile;
	archivedir = duparchivedir;
	arcflist = duparcflist;
	maxarcf = dupmaxarcf;
	filepos = dupfilepos;
	findpattern = dupfindpat;
	sorton = dupsorton;

	if (archivefile) {
		strcpy(fullpath, dupfullpath);
		free(dupfullpath);
		removetmp(dir, list[filepos].name);
	}

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
		if (regexp_exec(re, arc) && archivelist[i].p_comm) break;
		regexp_free(re);
	}
	if (i >= maxarchive) return(-1);
	regexp_free(re);

	waitmes();
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
		if (regexp_exec(re, arc) && archivelist[i].u_comm) break;
		regexp_free(re);
	}
	if (i >= maxarchive) return(-1);
	regexp_free(re);

	if (dir) strcpy(path, dir);
	else {
		if (tr) dir = tree(0);
		else {
			dir = inputstr2(UNPAC_K, -1, NULL, NULL);
			dir = evalpath(dir);
		}
		if (!dir) return(0);
		strcpy(path, (*dir) ? dir : ".");
		free(dir);
	}

	if (chdir(path) < 0) {
		warning(-1, path);
		return(0);
	}

	strcpy(path, fullpath);
	strcat(path, "/");
	strcat(path, arc);
	waitmes();
	if (execmacro(archivelist[i].u_comm, path, list, max, -1, 1) < 0) {
		warning(E2BIG, archivelist[i].u_comm);
		if (chdir(fullpath) < 0) error(fullpath);
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

	cp = evalpath(strdup2(deftmpdir));
	sprintf(path, "%s/fd%d.%d", cp, getpid(), launchlevel);
	free(cp);
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
	if (i < 0) putterm(t_bell);
	else if (i > 0) {
		if (*archivedir && chdir(archivedir) < 0)
			warning(-1, archivedir);
		else if (access(list[filepos].name, F_OK) < 0)
			warning(-1, list[filepos].name);
		else return(cp);
	}
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
	macrostat st;
	char *tmp;

	waitmes();
	st.flags = 0;
	if (!(tmp = evalcommand("tar cf %C %TA", dev, list, max, &st))) {
		warning(E2BIG, dev);
		return(0);
	}
	for (;;) {
		system3(tmp, -1);
		free(tmp);
		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand("tar rf %C %TA", dev, list, max, 0)))
			return(1);
	}
}
