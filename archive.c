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

extern int shutdrv();

extern bindtable bindlist[];
extern functable funclist[];
extern int filepos;
extern int mark;
extern int fnameofs;
extern int sorton;
extern char fullpath[];
extern char *findpattern;
extern char *deftmpdir;
extern char *tmpfilename;
extern namelist *filelist;
extern int maxfile;
extern int maxent;
extern char *curfilename;
extern int sizeinfo;

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
static int readfileent();
static VOID archbar();
static char *pseudodir();
static int readarchive();
static int archbrowse();

char *archivefile = NULL;
char *archivedir;
int maxlaunch;
int maxarchive;
launchtable launchlist[MAXLAUNCHTABLE] = {
	{" ^.*\\.lzh$",		"lha l",		PM_LHA},
	{" ^.*\\.tar$",		"tar tvf", 		PM_TAR},
	{" ^.*\\.tar\\.Z$",	"zcat %C | tar tvf -",	PM_TAR},
	{" ^.*\\.tar\\.gz$",	"gunzip -c %C | tar tvf -", PM_TAR},
	{NULL,			NULL,			-1, 0, "", "", "", ""}
};
archivetable archivelist[MAXARCHIVETABLE] = {
	{" ^.*\\.lzh$",		"lha aq %C %TA",	"lha xq %C %TA"},
	{" ^.*\\.tar$",		"tar cf %C %T",		"tar xf %C %TA"},
	{" ^.*\\.tar\\.Z$",	"tar cf %X %T; compress %X",
						"zcat %C | tar xf - %TA"},
	{" ^.*\\.tar\\.gz$",	"tar cf %X %T; gzip %X",
						"gunzip -c %C | tar xf - %TA"},
	{NULL,			NULL,			NULL}
};

static launchtable *launchp;
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

static int readfileent(tmp, line, list, max)
namelist *tmp;
char *line;
launchtable *list;
int max;
{
	struct tm tm;
	struct passwd *pw;
	struct group *gr;
	int i, ofs, skip;
	char *cp, buf[MAXLINESTR + 1];

	cp = line;
	i = countfield(line, list -> sep, -1);
	skip = (max > i) ? max - i : 0;

	tmp -> flags = 0;
	tmp -> st_mode = 0;
	getfield(buf, line, list -> field[F_NAME], skip,
		list -> delim[F_NAME], list -> width[F_NAME], list -> sep);
	if (!*buf) return(-1);
	if (buf[(i = (int)strlen(buf) - 1)] == '/') {
		buf[i] = '\0';
		tmp -> st_mode |= S_IFDIR;
		tmp -> flags |= F_ISDIR;
	}
	tmp -> name = strdup2(buf);

	getfield(buf, line, list -> field[F_MODE], skip,
		list -> delim[F_MODE], list -> width[F_MODE], list -> sep);
	for (i = ofs = strlen(buf); i < 9; i++) buf[i] = '\0';
	if ((ofs -= 9) < 0) ofs = 0;
	for (i = 0; i < 9; i++) {
		tmp -> st_mode *= 2;
		if (buf[ofs + i] != '-') tmp -> st_mode += 1;
	}
	if (buf[ofs + 2] == 's') tmp -> st_mode |= S_ISUID;
	if (buf[ofs + 5] == 's') tmp -> st_mode |= S_ISGID;
	if (buf[ofs + 8] == 't') tmp -> st_mode |= S_ISVTX;
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
		return(-1);
	}

	getfield(buf, line, list -> field[F_YEAR], skip,
		list -> delim[F_YEAR], list -> width[F_YEAR], list -> sep);
	tm.tm_year = (*buf) ? atoi(buf) : 1970;
	if (tm.tm_year < 100 && (tm.tm_year += 1900) < 1970) tm.tm_year += 100;
	tm.tm_year -= 1900;
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

	return(0);
}

static VOID archbar(file, dir)
char *file, *dir;
{
	char *arch;

	arch = (char *)malloc2(strlen(fullpath)
		+ strlen(file) + strlen(dir) + 3);
	strcpy(arch, fullpath);
	if (!*fullpath || fullpath[(int)strlen(fullpath) - 1] != '/')
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

static char *pseudodir(namep, list, max)
namelist *namep, *list;
int max;
{
	char *cp, *tmp;
	int i, len;

	cp = namep -> name;
	while (tmp = strchr(cp, '/')) {
		len = tmp - (namep -> name);
		for (i = 0; i < max; i++) {
			if (isdir(&list[i]) && len == strlen(list[i].name)
			&& !strncmp(list[i].name, namep -> name, len)) break;
		}
		if (i >= max) {
			tmp = (char *)malloc2(len + 1);
			strncpy2(tmp, namep -> name, len);
			return(tmp);
		}
		cp = tmp + 1;
	}
	if (isdir(namep)) for (i = 0; i < max; i++) {
		if (isdir(&list[i]) && !strcmp(list[i].name, namep -> name)) {
			memcpy(&(list[i].st_mode), &(namep -> st_mode),
				sizeof(namelist)
				- sizeof(char *) - sizeof(u_short));
			return(NULL);
		}
	}
	return(namep -> name);
}

VOID rewritearc(all)
int all;
{
	archbar(archivefile, archivedir);
	if (all) {
		helpbar();
		infobar(arcflist, filepos);
	}
	statusbar(maxarcf);
	locate(0, LSTACK);
	putterm(l_clear);
	listupfile(arcflist, maxarcf, arcflist[filepos].name);
	locate(0, 0);
	tflush();
}

static int readarchive(file, list)
char *file;
launchtable *list;
{
	namelist tmp;
	FILE *fp;
	char *cp, line[MAXLINESTR + 1];
	int i, no, max;

	if (!(cp = evalcommand(list -> comm, file, NULL, 0, NULL))) {
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

	for (i = 0; i < (int)(list -> topskip); i++)
		if (!fgets(line, MAXLINESTR, fp)) break;

	filelist = (namelist *)addlist(filelist, 0, &maxent, sizeof(namelist));
	filelist[0].name = strdup2("..");
	filelist[0].flags = F_ISDIR;
	maxfile = 1;
	no = 0;

	while (fgets(line, MAXLINESTR, fp)) {
		no++;
		if (cp = strchr(line, '\n')) *cp = '\0';
		if (readfileent(&tmp, line, list, max) < 0) continue;
		while ((cp = pseudodir(&tmp, filelist, maxfile))
		&& cp != tmp.name) {
			filelist = (namelist *)addlist(filelist, maxfile,
				&maxent, sizeof(namelist));
			memcpy(&filelist[maxfile], &tmp, sizeof(namelist));
			filelist[maxfile].st_mode |= S_IFDIR;
			filelist[maxfile].flags |= F_ISDIR;
			filelist[maxfile].name = cp;
			filelist[maxfile].ent = no;
			maxfile++;
		}
		if (!cp) {
			free(tmp.name);
			continue;
		}

		filelist = (namelist *)addlist(filelist, maxfile,
			&maxent, sizeof(namelist));
		memcpy(&filelist[maxfile], &tmp, sizeof(namelist));
		filelist[maxfile].ent = no;
		maxfile++;
	}
	for (i = 0; i < (int)(list -> bottomskip); i++) {
		if (maxfile < 2
		|| filelist[maxfile - 1].ent <= no - (int)(list -> bottomskip))
			break;
		free(filelist[--maxfile].name);
	}
	for (i = 0; i < maxfile; i++) filelist[i].ent = i;
	if (maxfile <= 1) {
		maxfile = 0;
		free(filelist[0].name);
		filelist[0].name = NOFIL_K;
		filelist[0].st_nlink = -1;
	}

	if (pclose(fp)) {
		warning(0, HITKY_K);
		for (i = 0; i < maxfile; i++) free(filelist[i].name);
		return(-1);
	}
	return(maxfile);
}

static int archbrowse(file, max)
char *file;
int max;
{
	reg_t *re;
	u_char fstat;
	char *cp;
	int ch, i, no, len, old;

	if (!findpattern) re = NULL;
	else {
		cp = cnvregexp(findpattern, 1);
		re = regexp_init(cp);
		free(cp);
	}

	maxarcf = (len = strlen(archivedir)) ? 1 : 0;

	for (i = 1; i < max; i++) {
		if (strncmp(archivedir, filelist[i].name, len)) continue;

		cp = filelist[i].name + len;
		if (len > 0) cp++;
		if (len == strlen(filelist[i].name)) {
			memcpy(&arcflist[0], &filelist[i], sizeof(namelist));
			arcflist[0].name = filelist[0].name;
		}
		else if (!strchr(cp, '/') && (!re || regexp_exec(re, cp))) {
			memcpy(&arcflist[maxarcf], &filelist[i],
				sizeof(namelist));
			arcflist[maxarcf].name = cp;
			maxarcf++;
		}
	}
	if (re) regexp_free(re);
	if (!maxarcf) {
		arcflist[0].name = NOFIL_K;
		arcflist[0].st_nlink = -1;
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
		getkey2(-1);
		ch = getkey2(SIGALRM);
		getkey2(-1);

		old = filepos;
		for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			if (ch == (int)(bindlist[i].key)) break;
		no = (bindlist[i].d_func < 255 && isdir(&arcflist[filepos])) ?
			(int)(bindlist[i].d_func) : (int)(bindlist[i].f_func);
		if (no > NO_OPERATION) continue;
		fstat = funclist[no].stat;
		curfilename = arcflist[filepos].name;
		if (!(fstat & ARCH) || (maxarcf <= 0 && !(fstat & NO_FILE)))
			no = 0;
		else no = (*funclist[no].func)(arcflist, &maxarcf, NULL);

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
	else if (filepos >= 0 && strcmp(arcflist[filepos].name, "..")) {
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
	return(no);
}

int launcher(list, max)
namelist *list;
int max;
{
	reg_t *re;
	namelist *duparcflist;
	launchtable *duplaunchp;
	char *dupfullpath, *duparchivefile, *duparchivedir, *dupfindpat;
	char *cp, *dir, path[MAXPATHLEN + 1], file[MAXNAMLEN + 1];
	int i, drive, dupmaxarcf, dupfilepos, dupsorton;

	for (i = 0; i < maxlaunch; i++) {
		re = regexp_init(launchlist[i].ext);
		if (regexp_exec(re, list[filepos].name)) break;
		regexp_free(re);
	}
	if (i >= maxlaunch) return(-1);
	regexp_free(re);

	drive = 0;
	if ((archivefile && !(dir = tmpunpack(list, max)))
	|| ((drive = dospath("", NULL)) && !(dir = tmpdosdupl(drive,
	list[filepos].name, list[filepos].st_mode)))) {
		putterm(t_bell);
		return(1);
	}
	if (launchlist[i].topskip == 255) {
		execusercomm(launchlist[i].comm, list[filepos].name,
			NULL, NULL, 1, 0);
		if (drive) removetmp(dir, NULL, list[filepos].name);
		else if (archivefile)
			removetmp(dir, archivedir, list[filepos].name);
		return(0);
	}

	if (drive) {
		dupfullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
	}
	else if (archivefile) {
		dupfullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
		if (*archivedir) {
			strcat(fullpath, "/");
			strcat(fullpath, archivedir);
		}
	}
	duparchivefile = archivefile;
	duparchivedir = archivedir;
	duparcflist = arcflist;
	dupmaxarcf = maxarcf;
	dupfilepos = filepos;
	dupfindpat = findpattern;
	dupsorton = sorton;
	duplaunchp = launchp;

	archivefile = cp = strdup2(list[filepos].name);
	archivedir = path;
	findpattern = NULL;
	sorton = 0;
	launchp = &launchlist[i];
	for (i = 0; i < maxfile; i++) free(filelist[i].name);

	*path = *file = '\0';
	launchlevel++;
	if (readarchive(archivefile, launchp) >= 0) {
		arcflist = (namelist *)malloc2(maxfile * sizeof(namelist));
		while (archbrowse(file, maxfile) >= 0);
		free(arcflist);
		for (i = 0; i < maxfile; i++) free(filelist[i].name);
	}
	launchlevel--;

	archivefile = duparchivefile;
	archivedir = duparchivedir;
	arcflist = duparcflist;
	maxarcf = dupmaxarcf;
	filepos = dupfilepos;
	findpattern = dupfindpat;
	sorton = dupsorton;
	launchp = duplaunchp;

	if (drive) {
		strcpy(fullpath, dupfullpath);
		free(dupfullpath);
		removetmp(dir, NULL, cp);
		filelist[0].name = cp;
		filepos = 0;
		maxfile = 1;
	}
	else if (archivefile) {
		strcpy(fullpath, dupfullpath);
		free(dupfullpath);
		removetmp(dir, archivedir, cp);
		free(cp);
		if (readarchive(archivefile, launchp) < 0) {
			filelist[0].name = NOFIL_K;
			*archivedir = '\0';
			filepos = 0;
			maxfile = 0;
		}
		else {
			i = arcflist[filepos].ent;
			if (cp = strrchr(filelist[i].name, '/')) cp++;
			else cp = filelist[i].name;
			arcflist[filepos].name = cp;
		}
	}
	else {
		filelist[0].name = cp;
		filepos = 0;
		maxfile = 1;
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
	execusercomm(archivelist[i].p_comm, arc, list, &max, -1, 1);
	return(1);
}

int unpack(arc, dir, list, max, arg, tr)
char *arc, *dir;
namelist *list;
int max;
char *arg;
int tr;
{
	reg_t *re;
	char *tmpdir, path[MAXPATHLEN + 1];
	int i, dd, drive;

	for (i = 0; i < maxarchive; i++) {
		re = regexp_init(archivelist[i].ext);
		if (regexp_exec(re, arc) && archivelist[i].u_comm) break;
		regexp_free(re);
	}
	if (i >= maxarchive) return(-1);
	regexp_free(re);

	tmpdir = NULL;
	drive = dospath(arc, NULL);
	if (dir) strcpy(path, dir);
	else {
		if (tr) {
			dir = tree(0, &dd);
			if (dd >= 0) shutdrv(dd);
		}
		else {
			if (arg && *arg) dir = strdup2(arg);
			else dir = inputstr(UNPAC_K, 0, -1, NULL, NULL);
			dir = evalpath(dir);
		}
		if (!dir) return(0);
		strcpy(path, (*dir) ? dir : ".");
		free(dir);
	}
	if (dospath(path, NULL)) {
		warning(ENOENT, path);
		return(0);
	}

	if (drive && !(tmpdir = tmpdosdupl(drive, arc, 0700))) {
		warning(-1, arc);
		return(0);
	}
	if (_chdir2(path) < 0) {
		warning(-1, path);
		return(0);
	}

	if (!(drive = _dospath(fullpath))) strcpy(path, fullpath);
	else sprintf(path, "%s/%s/%c:", deftmpdir, tmpfilename, drive);
	strcat(path, "/");
	strcat(path, arc);
	waitmes();
	if (execusercomm(archivelist[i].u_comm, path, list, &max, -1, 1) < 0) {
		warning(E2BIG, archivelist[i].u_comm);
		if (_chdir2(fullpath) < 0) error(fullpath);
		return(0);
	}
	if (tmpdir) removetmp(tmpdir, NULL, arc);
	if (_chdir2(fullpath) < 0) error(fullpath);
	return(1);
}

char *tmpunpack(list, max)
namelist *list;
int max;
{
	char path[MAXPATHLEN + 1];
	int i, dupmark;

	sprintf(path, "L%d", launchlevel);
	if (mktmpdir(path) < 0) {
		warning(-1, path);
		return(NULL);
	}

	dupmark = mark;
	mark = 0;
	i = unpack(archivefile, path, list, max, NULL, 0);
	mark = dupmark;

	if (_chdir2(path) < 0) error(path);
	if (i < 0) putterm(t_bell);
	else if (i > 0) {
		if (*archivedir && _chdir2(archivedir) < 0)
			warning(-1, archivedir);
		else if (access(list[filepos].name, F_OK) < 0)
			warning(-1, list[filepos].name);
		else return(strdup2(path));
	}
	removetmp(path, archivedir, NULL);
	return(NULL);
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
			tmp = strrchr(dupdir, '/');
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

int backup(dev, list, max)
char *dev;
namelist *list;
int max;
{
	macrostat st;
	char *tmp;
	int i;

	waitmes();
	for (i = 0; i < max; i++)
		if (ismark(&list[i])) list[i].flags |= F_ISARG;
	st.flags = 0;
	if (!(tmp = evalcommand("tar cf %C %TA", dev, list, max, &st))) {
		warning(E2BIG, dev);
		return(0);
	}
	for (;;) {
		system2(tmp, -1);
		free(tmp);
		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand("tar rf %C %TA", dev, list, max, NULL)))
			break;
	}
	for (i = 0; i < max; i++) list[i].flags &= ~(F_ISARG | F_ISMRK);
	return(1);
}
