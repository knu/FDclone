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

#ifndef	_NOARCHIVE

#include <signal.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef	USETIMEH
#include <time.h>
#endif

#if	MSDOS
extern FILE *Xpopen();
extern int Xpclose();
#include "unixemu.h"
#else
#define	Xpopen		popen
#define	Xpclose		pclose
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/param.h>
#endif

#if	!MSDOS && !defined(_NODOSDRIVE)
extern int shutdrv();
#endif

extern bindtable bindlist[];
extern functable funclist[];
extern int filepos;
extern int mark;
extern int fnameofs;
extern int sorton;
extern char fullpath[];
extern char typesymlist[];
extern u_short typelist[];
extern char *findpattern;
extern char *deftmpdir;
extern char *tmpfilename;
extern namelist *filelist;
extern int maxfile;
extern int maxent;
extern char *curfilename;
extern int sizeinfo;

#if	MSDOS
#define	PM_LHA	5, 2,\
		{-1, -1, -1, 1, 4, 4, 4, 5, 0},\
		{0, 0, 0, 0, 0, '-', 128 + 6, 0, 0},\
		{0, 0, 0, 0, '-', '-', 0, 0, 0},\
		{-1, -1, -1}, 2
#define	PM_TAR	0, 0,\
		{0, 1, 1, 2, 5, 3, 4, 6, 7},\
		{0, 0, '/', 0, 0, 0, 0, 0, 0},\
		{0, '/', 0, 0, 0, 0, 0, 0, 0},\
		{-1, -1, -1}, 1
#else	/* !MSDOS */
#define	PM_LHA	2, 2,\
		{0, 1, 1, 2, 6, 4, 5, 6, 7},\
		{0, 0, '/', 0, 0, 0, 0, 0, 0},\
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0},\
		{9, -1, -1}, 1
#ifdef	TARUSESPACE
#define	PM_TAR	0, 0,\
		{0, 1, 1, 2, 6, 3, 4, 5, 7},\
		{0, 0, '/', 0, 0, 0, 0, 0, 0},\
		{0, '/', 0, 0, 0, 0, 0, 0, 0},\
		{-1, -1, -1}, 1
#else
#define	PM_TAR	0, 0,\
		{0, 1, 1, 2, 6, 3, 4, 5, 7},\
		{0, 0, '/', 0, 0, 0, 0, 0, 0},\
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0},\
		{9, -1, -1}, 1
#endif
#endif	/* !MSDOS */

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
#if	MSDOS
	{" ^.*\\.lzh$",		"lha v",		PM_LHA},
#else
	{" ^.*\\.lzh$",		"lha l",		PM_LHA},
#endif
	{" ^.*\\.tar$",		"tar tvf", 		PM_TAR},
	{" ^.*\\.tar\\.Z$",	"zcat %C | tar tvf -",	PM_TAR},
	{" ^.*\\.tar\\.gz$",	"gzip -cd %C | tar tvf -", PM_TAR},
	{NULL,			NULL,		-1, 0, "", "", "", "", 1}
};
archivetable archivelist[MAXARCHIVETABLE] = {
#if	MSDOS
	{" ^.*\\.lzh$",		"lha a %C %TA",		"lha x %C %TA"},
#else
	{" ^.*\\.lzh$",		"lha aq %C %TA",	"lha xq %C %TA"},
#endif
	{" ^.*\\.tar$",		"tar cf %C %T",		"tar xf %C %TA"},
	{" ^.*\\.tar\\.Z$",	"tar cf %X %T; compress %X",
						"zcat %C | tar xf - %TA"},
	{" ^.*\\.tar\\.gz$",	"tar cf %X %T; gzip %X",
						"gzip -cd %C | tar xf - %TA"},
	{NULL,			NULL,			NULL}
};

static launchtable *launchp;
static namelist *arcflist;
static int maxarcf;
static int launchlevel = 0;


static int countfield(line, sep, field, eolp)
char *line;
u_char sep[];
int field, *eolp;
{
	int i, j, f, s, sp;

	f = -1;
	s = 1;
	sp = (int)sep[0];

	for (i = j = 0; line[i]; i++) {
		if (sp < 255 && i >= sp) {
			j++;
			sp = (j < MAXLAUNCHSEP) ? (int)sep[j] : 255;
			for (; sp == 255 || i < sp; i++)
				if (!strchr(" \t", line[i])) break;
			if (f < 0) f = 1;
			else f++;
			s = 0;
		}
		else if (strchr(" \t", line[i])) s = 1;
		else if (s) {
			f++;
			s = 0;
		}

		if (field < 0) continue;
		else if (f > field) return(-1);
		else if (f == field) {
			if (eolp) {
				for (j = i; line[j]; j++) {
					if ((sp < 255 && j >= sp)
					|| strchr(" \t", line[j])) break;
				}
				*eolp = j;
			}
			return(i);
		}
	}
	return((field < 0) ? f : -1);
}

static char *getfield(buf, line, skip, list, no)
char *buf, *line;
int skip;
launchtable *list;
int no;
{
	char *cp, *tmp;
	int i, f, eol;

	*buf = '\0';
	f = (list -> field[no] < 255) ? (int)(list -> field[no]) - skip : -1;

	if (f < 0 || (i = countfield(line, list -> sep, f, &eol)) < 0)
		return(buf);
	cp = &line[i];

	i = (int)(list -> delim[no]);
	if (i >= 128) {
		i -= 128;
		if (&cp[i] < &line[eol]) cp += i;
		else return(buf);
	}
	else if (i && (!(cp = strchr2(cp, i)) || ++cp >= &line[eol]))
		return(buf);

	i = (int)(list -> width[no]);
	if (i >= 128) i -= 128;
	else if (i) {
		if (tmp = strchr2(cp, i)) i = tmp - cp;
		else i = &line[eol] - cp;
	}
	if (!i || &cp[i] > &line[eol]) i = &line[eol] - cp;

	return(strncpy2(buf, cp, i));
}

static int readfileent(tmp, line, list, max)
namelist *tmp;
char *line;
launchtable *list;
int max;
{
	struct tm tm;
#if	!MSDOS
	struct passwd *pw;
	struct group *gr;
#endif
	u_short mode;
	int i, ofs, skip;
	char *cp, buf[MAXLINESTR + 1];

	i = countfield(line, list -> sep, -1, NULL);
	skip = (max > i) ? max - i : 0;

	cp = line;
	tmp -> flags = 0;
	tmp -> st_mode = 0;
	getfield(buf, line, skip, list, F_NAME);
	if (!*buf) return(-1);
	if (buf[(i = (int)strlen(buf) - 1)] == '/') {
		buf[i] = '\0';
		tmp -> st_mode |= S_IFDIR;
	}
	tmp -> name = strdup2(buf);

	getfield(buf, line, skip, list, F_MODE);
	for (i = ofs = strlen(buf); i < 9; i++) buf[i] = '\0';
	if ((ofs -= 9) < 0) ofs = 0;
	if (!*buf) mode = 0644;
	else for (i = 0, mode = 0; i < 9; i++) {
		mode *= 2;
		if (buf[ofs + i] && buf[ofs + i] != '-') mode |= 1;
	}
	tmp -> st_mode |= mode;
	if (buf[ofs + 2] == 's') tmp -> st_mode |= S_ISUID;
	if (buf[ofs + 5] == 's') tmp -> st_mode |= S_ISGID;
	if (buf[ofs + 8] == 't') tmp -> st_mode |= S_ISVTX;

	if (ofs <= 0) i = -1;
	else {
		for (i = 0; typesymlist[i]; i++)
			if (buf[ofs - 1] == typesymlist[i]) break;
	}
	if (i >= 0 && typesymlist[i]) {
		tmp -> st_mode &= ~S_IFMT;
		tmp -> st_mode |= typelist[i];
	}
	else if (!(tmp -> st_mode & S_IFMT)) tmp -> st_mode |= S_IFREG;
	if ((tmp -> st_mode & S_IFMT) == S_IFDIR) tmp -> flags |= F_ISDIR;
	else if ((tmp -> st_mode & S_IFMT) == S_IFLNK) tmp -> flags |= F_ISLNK;

	tmp -> st_nlink = 1;
#if	!MSDOS
	getfield(buf, line, skip, list, F_UID);
	if (isdigit(*buf)) tmp -> st_uid = atoi2(buf);
	else tmp -> st_uid = (pw = getpwnam(buf)) ? pw -> pw_uid : -1;
	getfield(buf, line, skip, list, F_GID);
	if (isdigit(*buf)) tmp -> st_gid = atoi2(buf);
	else tmp -> st_gid = (gr = getgrnam(buf)) ? gr -> gr_gid : -1;
	if (tmp -> st_uid < 0 && tmp -> st_gid < 0) {
		free(tmp -> name);
		return(-1);
	}
#endif
	tmp -> st_size = atol(getfield(buf, line, skip, list, F_SIZE));

	getfield(buf, line, skip, list, F_YEAR);
	tm.tm_year = (*buf) ? atoi(buf) : 1970;
	if (tm.tm_year < 100 && (tm.tm_year += 1900) < 1970) tm.tm_year += 100;
	tm.tm_year -= 1900;
	getfield(buf, line, skip, list, F_MON);
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
	tm.tm_mday = atoi(getfield(buf, line, skip, list, F_DAY));

	getfield(buf, line, skip, list, F_TIME);
	tm.tm_hour = atoi(buf);
	if (tm.tm_hour < 0 || tm.tm_hour > 23) tm.tm_hour = 0;
	cp = strchr(buf, ':');
	tm.tm_min = (cp) ? atoi(++cp) : 0;
	tm.tm_sec = (cp && (cp = strchr(cp, ':'))) ? atoi(++cp) : 0;

	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags |=
#if	MSDOS
		logical_access(tmp -> st_mode);
#else
		logical_access(tmp -> st_mode, tmp -> st_uid, tmp -> st_gid);
#endif

	return(0);
}

static VOID archbar(file, dir)
char *file, *dir;
{
	char *arch;

	arch = (char *)malloc2(strlen(fullpath)
		+ strlen(file) + strlen(dir) + 3);
	strcpy(arch, fullpath);
	if (!*fullpath || fullpath[(int)strlen(fullpath) - 1] != _SC_)
		strcat(arch, _SS_);
	strcat(arch, file);
	strcat(arch, ":");
	strcat(arch, dir);

	locate(0, LPATH);
	putterm(l_clear);
	putch2(' ');
	putterm(t_standout);
	cputs2("Arch:");
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
	char *cp, *buf, line[MAXLINESTR + 1];
	int i, no, max;

	if (!(cp = evalcommand(list -> comm, file, NULL, 0, NULL))) {
		warning(E2BIG, list -> comm);
		return(-1);
	}
	fp = Xpopen(cp, "r");
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
	i = no = 0;
	if (list -> lines == 1) buf = line;
	else buf = (char *)malloc2(list -> lines * sizeof(line));

	while (fgets(line, MAXLINESTR, fp)) {
		no++;
		if (cp = strchr(line, '\n')) *cp = '\0';
		if (list -> lines > 1) {
			if (!(i++)) {
				strcpy(buf, line);
				continue;
			}
			else {
				strcat(buf, "\t");
				strcat(buf, line);
				if (i < list -> lines) continue;
			}
			i = 0;
		}
		if (readfileent(&tmp, buf, list, max) < 0) continue;
		while ((cp = pseudodir(&tmp, filelist, maxfile))
		&& cp != tmp.name) {
			filelist = (namelist *)addlist(filelist, maxfile,
				&maxent, sizeof(namelist));
			memcpy(&filelist[maxfile], &tmp, sizeof(namelist));
			filelist[maxfile].st_mode &= ~S_IFMT;
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
	if (buf != line) free(buf);

	no -= (int)(list -> bottomskip);
	for (i = maxfile - 1; i > 0; i--) {
		if (filelist[i].ent <= no) break;
		free(filelist[i].name);
	}
	maxfile = i + 1;
	for (i = 0; i < maxfile; i++) filelist[i].ent = i;
	if (maxfile <= 1) {
		maxfile = 0;
		free(filelist[0].name);
		filelist[0].name = NOFIL_K;
		filelist[0].st_nlink = -1;
	}

	if (Xpclose(fp) < 0) {
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
	sizebar();
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
#ifdef	_NOEDITMODE
		ch = getkey2(SIGALRM);
#else
		getkey2(-1);
		ch = getkey2(SIGALRM);
		getkey2(-1);
#endif

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
			sizebar();
			statusbar(maxarcf);
			locate(0, LSTACK);
			putterm(l_clear);
			helpbar();
		}
	}

	if (no <= 4) strcpy(file, arcflist[filepos].name);
	else if (filepos >= 0 && strcmp(arcflist[filepos].name, "..")) {
		if (*archivedir) strcat(archivedir, _SS_);
		strcat(archivedir, arcflist[filepos].name);
		*file = '\0';
	}
	else if (!*archivedir) no = -1;
	else {
		if (cp = strrchr(archivedir, _SC_)) strcpy(file, cp + 1);
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
	int i, dupmaxarcf, dupfilepos, dupsorton;
#if	!MSDOS
	int drive = 0;
#endif

	for (i = 0; i < maxlaunch; i++) {
		re = regexp_init(launchlist[i].ext);
		if (regexp_exec(re, list[filepos].name)) break;
		regexp_free(re);
	}
	if (i >= maxlaunch) return(-1);
	regexp_free(re);

	if ((archivefile && !(dir = tmpunpack(list, max)))
#if	!MSDOS && !defined (_NODOSDRIVE)
	|| ((drive = dospath("", NULL)) && !(dir = tmpdosdupl(drive,
	list[filepos].name, list[filepos].st_mode)))
#endif
	) {
		putterm(t_bell);
		return(1);
	}
	if (launchlist[i].topskip == 255) {
		execusercomm(launchlist[i].comm, list[filepos].name,
			NULL, NULL, 1, 0);
#if	!MSDOS
		if (drive) removetmp(dir, NULL, list[filepos].name);
		else
#endif
		if (archivefile)
			removetmp(dir, archivedir, list[filepos].name);
		return(0);
	}

#if	!MSDOS
	if (drive) {
		dupfullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
	}
	else
#endif
	if (archivefile) {
		dupfullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
		if (*archivedir) {
			strcat(fullpath, _SS_);
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
		arcflist = (namelist *)malloc2(((maxfile) ? maxfile : 1)
			* sizeof(namelist));
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

#if	!MSDOS
	if (drive) {
		strcpy(fullpath, dupfullpath);
		free(dupfullpath);
		removetmp(dir, NULL, cp);
		filelist[0].name = cp;
		filepos = 0;
		maxfile = 1;
	}
	else
#endif
	if (archivefile) {
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

/* ARGSUSED */
int unpack(arc, dir, list, max, arg, tr)
char *arc, *dir;
namelist *list;
int max;
char *arg;
int tr;
{
	reg_t *re;
	char *tmpdir, path[MAXPATHLEN + 1];
	int i;
#if	!MSDOS
	int dd, drive;
#endif

	for (i = 0; i < maxarchive; i++) {
		re = regexp_init(archivelist[i].ext);
		if (regexp_exec(re, arc) && archivelist[i].u_comm) break;
		regexp_free(re);
	}
	if (i >= maxarchive) return(-1);
	regexp_free(re);

	tmpdir = NULL;
	if (dir) strcpy(path, dir);
	else {
#ifndef	_NOTREE
		if (tr) {
#if	MSDOS || defined(_NODOSDRIVE)
			dir = tree(0, NULL);
#else
			dir = tree(0, &dd);
			if (dd >= 0) shutdrv(dd);
#endif
		}
		else
#endif	/* !_NOTREE */
		{
			if (arg && *arg) dir = strdup2(arg);
			else dir = inputstr(UNPAC_K, 0, -1, NULL, NULL);
			dir = evalpath(dir);
		}
		if (!dir) return(0);
		strcpy(path, (*dir) ? dir : ".");
		free(dir);
	}
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (dospath(path, NULL)) {
		warning(ENOENT, path);
		return(0);
	}

	drive = dospath(arc, NULL);
	if (drive && !(tmpdir = tmpdosdupl(drive, arc, 0700))) {
		warning(-1, arc);
		return(0);
	}
#endif
	if (_chdir2(path) < 0) {
		warning(-1, path);
		return(0);
	}

#if	MSDOS || defined (_NODOSDRIVE)
	strcpy(path, fullpath);
#else
	if (!(drive = _dospath(fullpath))) strcpy(path, fullpath);
	else {
		strcpy(path, deftmpdir);
		strcat(path, _SS_);
		strcat(path, tmpfilename);
		strcat(path, _SS_);
		sprintf(path + strlen(path), "%c", drive);
		strcat(path, ":");
	}
#endif
	strcat(path, _SS_);
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
		else if (Xaccess(list[filepos].name, F_OK) < 0)
			warning(-1, list[filepos].name);
		else return(strdup2(path));
	}
	removetmp(path, archivedir, (i > 0) ? list[filepos].name : NULL);
	return(NULL);
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
#endif	/* !_NOARCHIVE */
