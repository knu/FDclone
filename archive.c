/*
 *	archive.c
 *
 *	Archive File Access Module
 */

#include <signal.h>
#include <ctype.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "funcno.h"
#include "kctype.h"
#include "kanji.h"

#ifndef	_NOARCHIVE

#if	!MSDOS
#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/param.h>
#endif

#ifndef	_NODOSDRIVE
extern int shutdrv __P_((int));
# if	MSDOS
extern int checkdrive __P_((int));
# endif
#endif

extern bindtable bindlist[];
extern functable funclist[];
extern int filepos;
extern int mark;
extern int isearch;
extern int fnameofs;
extern int dispmode;
extern int sorton;
extern int sorttype;
extern off_t marksize;
extern off_t totalsize;
extern off_t blocksize;
extern char fullpath[];
extern char typesymlist[];
extern u_short typelist[];
extern char *findpattern;
extern char *deftmpdir;
extern char *tmpfilename;
extern u_short today[];
extern namelist *filelist;
extern int maxfile;
extern int maxent;
extern char *curfilename;
extern int sizeinfo;
#ifndef	_NOCOLOR
extern int ansicolor;
#endif
extern int n_args;

#if	MSDOS
#define	PM_LHA	5, 2, \
		{255, 255, 255, 1, 4, 4, 4, 5, 0}, \
		{0, 0, 0, 0, 0, '-', 128 + 6, 0, 0}, \
		{0, 0, 0, 0, '-', '-', 0, 0, 0}, \
		{255, 255, 255}, 2
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 5, 3, 4, 6, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{0, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#else	/* !MSDOS */
#define	PM_LHA	2, 2, \
		{0, 1, 1, 2, 6, 4, 5, 6, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{9, 255, 255}, 1
#ifdef	UXPDS
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 10, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{10, 255, 255}, 1
#else	/* !UXPDS */
#ifdef	TARUSESPACE
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{0, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#else
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{9, 255, 255}, 1
#endif
#endif	/* !UXPDS */
#endif	/* !MSDOS */

static int countfield __P_((char *, u_char [], int, int *));
static char *getfield __P_((char *, char *, int, launchtable *, int));
static int readfileent __P_((namelist *, char *, launchtable *, int));
static VOID archbar __P_((char *, char *));
static int dircmp __P_((char *, char *));
static int dirmatchlen __P_((char *, char *));
static char *pseudodir __P_((namelist *, namelist *, int));
static int readarchive __P_((char *, launchtable *));
static int archbrowse __P_((char *, int));

char *archivefile = NULL;
char *archivedir = NULL;
int maxlaunch = 0;
int maxarchive = 0;
launchtable launchlist[MAXLAUNCHTABLE] = {
#if	MSDOS
	{"*.lzh",	"lha v %S",		PM_LHA},
	{"*.tar",	"tar tvf %S", 		PM_TAR},
	{"*.tar.Z",	"gzip -cd %S | tar tvf -",	PM_TAR},
	{"*.tar.gz",	"gzip -cd %S | tar tvf -",	PM_TAR},
#else
	{"*.lzh",	"lha l",		PM_LHA},
	{"*.tar",	"tar tvf", 		PM_TAR},
	{"*.tar.Z",	"zcat %C | tar tvf -",	PM_TAR},
	{"*.tar.gz",	"gzip -cd %C | tar tvf -",	PM_TAR},
#endif
	{NULL,		NULL,			255, 0, "", "", "", "", 1}
};
archivetable archivelist[MAXARCHIVETABLE] = {
#if	MSDOS
	{"*.lzh",	"lha a %S %TA",		"lha x %S %TA"},
	{"*.tar",	"tar cf %C %T",		"tar xf %C %TA"},
	{"*.tar.Z",	"tar cfZ %C %TA",	"gzip -cd %S | tar xf - %TA"},
	{"*.tar.gz",	"tar cfz %C %TA",	"gzip -cd %S | tar xf - %TA"},
#else
	{"*.lzh",	"lha aq %C %TA",	"lha xq %C %TA"},
	{"*.tar",	"tar cf %C %T",		"tar xf %C %TA"},
	{"*.tar.Z",	"tar cf %X %T; compress %X",
						"zcat %C | tar xf - %TA"},
	{"*.tar.gz",	"tar cf %X %T; gzip %X",
						"gzip -cd %C | tar xf - %TA"},
#endif
	{NULL,		NULL,			NULL}
};

static launchtable *launchp = NULL;
static namelist *arcflist = NULL;
static int maxarcf = 0;
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
	cp = &(line[i]);

	i = (int)(list -> delim[no]);
	if (i >= 128) {
		i -= 128;
		if (&(cp[i]) < &(line[eol])) cp += i;
		else return(buf);
	}
	else if (i && (!(cp = strchr2(cp, i)) || ++cp >= &(line[eol])))
		return(buf);

	i = (int)(list -> width[no]);
	if (i >= 128) i -= 128;
	else if (i) {
		if ((tmp = strchr2(cp, i))) i = tmp - cp;
		else i = &(line[eol]) - cp;
	}
	if (!i || &(cp[i]) > &(line[eol])) i = &(line[eol]) - cp;

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
	char *cp, *buf;

	i = countfield(line, list -> sep, -1, NULL);
	skip = (max > i) ? max - i : 0;
	buf = malloc2(strlen(line) + 1);

	cp = line;
	tmp -> flags = 0;
	tmp -> tmpflags = F_STAT;
	tmp -> st_mode = 0;
	getfield(buf, line, skip, list, F_NAME);
	if (!*buf) {
		free(buf);
		return(-1);
	}
#if	MSDOS
	for (i = 0; buf[i]; i++) if (buf[i] == '/') buf[i] = _SC_;
#endif
	if (isdelim(buf, i = (int)strlen(buf) - 1)) {
		if (i > 0) buf[i] = '\0';
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
	else tmp -> st_uid = (pw = getpwnam(buf)) ? pw -> pw_uid : (uid_t)-1;
	getfield(buf, line, skip, list, F_GID);
	if (isdigit(*buf)) tmp -> st_gid = atoi2(buf);
	else tmp -> st_gid = (gr = getgrnam(buf)) ? gr -> gr_gid : (gid_t)-1;
#endif
	tmp -> st_size = atol(getfield(buf, line, skip, list, F_SIZE));

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
	getfield(buf, line, skip, list, F_YEAR);
	if (!*buf) tm.tm_year = 1970;
	else if (list -> field[F_YEAR] != list -> field[F_TIME]
	|| !strchr(buf, ':')) tm.tm_year = atoi(buf);
	else {
		tm.tm_year = today[0];
		if (tm.tm_mon > today[1]
		|| (tm.tm_mon == today[1] && tm.tm_mday > today[2]))
			tm.tm_year--;
	}
	if (tm.tm_year < 1900 && (tm.tm_year += 1900) < 1970)
		tm.tm_year += 100;
	tm.tm_year -= 1900;

	getfield(buf, line, skip, list, F_TIME);
	tm.tm_hour = atoi(buf);
	tm.tm_min = (cp = strchr(buf, ':')) ? atoi(++cp) : 0;
	tm.tm_sec = (cp && (cp = strchr(cp, ':'))) ? atoi(++cp) : 0;
	if (tm.tm_hour < 0 || tm.tm_hour > 23)
		tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	else if (tm.tm_min < 0 || tm.tm_min > 59) tm.tm_min = tm.tm_sec = 0;
	else if (tm.tm_sec < 0 || tm.tm_sec > 59) tm.tm_sec = 0;

	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags |=
#if	MSDOS
		logical_access(tmp -> st_mode);
#else
		logical_access(tmp -> st_mode, tmp -> st_uid, tmp -> st_gid);
#endif

	free(buf);
	return(0);
}

static VOID archbar(file, dir)
char *file, *dir;
{
	char *arch;

	arch = malloc2(strlen(fullpath) + strlen(file) + strlen(dir) + 3);
	strcpy(strcpy2(strcatdelim2(arch, fullpath, file), ":"), dir);

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

static int dircmp(s1, s2)
char *s1, *s2;
{
	int i, j;

	for (i = j = 0; s2[j]; i++, j++) {
		if (s1[i] == s2[j]) {
#if	MSDOS
			if (iskanji1(s1, i) && (s1[++i] != s2[++j] || !s2[j]))
				return(s1[i] - s2[j]);
#endif
			continue;
		}
		if (s1[i] != _SC_ || s2[j] != _SC_) return(s1[i] - s2[j]);
		while (s1[i + 1] == _SC_) i++;
		while (s2[j + 1] == _SC_) j++;
	}
	return(s1[i]);
}

static int dirmatchlen(s1, s2)
char *s1, *s2;
{
	int i, j;

	for (i = j = 0; s1[i]; i++, j++) {
		if (s1[i] == s2[j]) {
#if	MSDOS
			if (iskanji1(s1, i)) {
				if (s1[++i] != s2[++j]) return(0);
				if (s1[i]) continue;
				if (s2[j]) j++;
				break;
			}
#endif
			continue;
		}
		if (s1[i] != _SC_ || s2[j] != _SC_) return(0);
		while (s1[i + 1] == _SC_) i++;
		while (s2[j + 1] == _SC_) j++;
	}
	if (s2[j] && s2[j] != _SC_) {
		for (j = 0; s1[j] == _SC_; j++);
		return((i == j) ? 1 : 0);
	}
	if (i && s2[j]) while (s2[j + 1] == _SC_) j++;
	return(j);
}

static char *pseudodir(namep, list, max)
namelist *namep, *list;
int max;
{
	char *cp, *tmp;
	int i, len;
	u_short ent;

	cp = namep -> name;
	while ((tmp = strdelim(cp, 0))) {
		while (*(tmp + 1) == _SC_) tmp++;
		if (!*(tmp + 1)) break;
		len = tmp - (namep -> name);
		if (!len) len++;
		for (i = 0; i < max; i++) {
			if (isdir(&(list[i]))
			&& len == dirmatchlen(list[i].name, namep -> name))
				break;
		}
		if (i >= max) {
			tmp = malloc2(len + 1);
			strncpy2(tmp, namep -> name, len);
			return(tmp);
		}
		if (strncmp(list[i].name, namep -> name, len)) {
			list[i].name = realloc2(list[i].name, len + 1);
			strncpy2(list[i].name, namep -> name, len);
		}
		cp = tmp + 1;
	}
	if (isdir(namep)) for (i = 0; i < max; i++) {
		if (isdir(&(list[i]))
		&& !dircmp(list[i].name, namep -> name)) {
			tmp = list[i].name;
			ent = list[i].ent;
			memcpy((char *)&(list[i]), (char *)namep,
				sizeof(namelist));
			list[i].name = tmp;
			list[i].ent = ent;
			return(NULL);
		}
	}
	return(namep -> name);
}

VOID rewritearc(all)
int all;
{
	if (all > 0) {
		title();
		helpbar();
		infobar(arcflist, filepos);
	}
	sizebar();
	statusbar(maxarcf);
	locate(0, LSTACK);
	putterm(l_clear);
#ifndef	_NOCOLOR
	if (ansicolor == 2) {
		int i;

		chgcolor(ANSI_BLACK, 1);
		for (i = 0; i < n_column; i++) putch2(' ');
		putterm(t_normal);
	}
#endif
	archbar(archivefile, archivedir);
	if (all >= 0) {
		listupfile(arcflist, maxarcf, arcflist[filepos].name);
		locate(0, 0);
	}
}

static int readarchive(file, list)
char *file;
launchtable *list;
{
	namelist tmp;
	FILE *fp;
	char *cp, *buf, *line;
	int i, c, no, len, max;

	if (!(cp = evalcommand(list -> comm, file, NULL, 0, NULL))) {
		warning(E2BIG, list -> comm);
		return(-1);
	}
#ifdef	DEBUG
	_mtrace_file = "popen";
#endif
	fp = Xpopen(cp, "r");
	free(cp);
	if (!fp) {
		warning(-1, list -> comm);
		return(-1);
	}
	waitmes();

	max = 0;
	for (i = 0; i < MAXLAUNCHFIELD; i++)
		if (list -> field[i] < 255 && (int)(list -> field[i]) > max)
			max = (int)(list -> field[i]);

	for (i = 0; i < (int)(list -> topskip); i++) {
		if (!(line = fgets2(fp))) break;
		free(line);
	}

	maxfile = 0;
	addlist();
	filelist[0].name = strdup2("..");
	filelist[0].flags = F_ISDIR;
	filelist[0].tmpflags = F_STAT;
	maxfile++;

	i = no = len = 0;
	buf = NULL;
	while ((line = fgets2(fp))) {
		if (kbhit2(0) && ((c = getkey2(0)) == cc_intr || c == ESC)) {
			Xpclose(fp);
			warning(0, INTR_K);
			free(line);
			if (buf) free(buf);
			for (i = 0; i < maxfile; i++) free(filelist[i].name);
			return(-1);
		}

		no++;
		if (list -> lines <= 1) buf = line;
		else {
			c = strlen(line);
			if (!(i++)) {
				buf = line;
				len = c;
				continue;
			}
			buf = realloc2(buf, len + c + 2);
			buf[len++] = '\t';
			memcpy(&(buf[len]), line, c);
			free(line);
			len += c;
			if (i < list -> lines) continue;

			buf[len] = '\0';
			i = len = 0;
		}

		c = readfileent(&tmp, buf, list, max);
		free(buf);
		buf = NULL;
		if (c < 0) continue;
		while ((cp = pseudodir(&tmp, filelist, maxfile))
		&& cp != tmp.name) {
			addlist();
			memcpy((char *)&(filelist[maxfile]), (char *)&tmp,
				sizeof(namelist));
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

		addlist();
		memcpy((char *)&(filelist[maxfile]), (char *)&tmp,
			sizeof(namelist));
		filelist[maxfile].ent = no;
		maxfile++;
	}

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
		filelist[0].flags = 0;
		filelist[0].tmpflags = F_STAT;
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
	char *cp, *tmp, *arcre, buf[MAXNAMLEN + 1];
	int ch, i, no, len, old;

	maxarcf = (*archivedir) ? 1 : 0;
	mark = 0;
	totalsize = marksize = 0;
	buf[0] = '\0';
	blocksize = 1;
	if (sorttype < 100) sorton = 0;

	re = NULL;
	arcre = NULL;
	if (findpattern) {
		if (*findpattern == '/') arcre = findpattern + 1;
		else re = regexp_init(findpattern, -1);
	}

	for (i = 1; i < max; i++) {
		if (!*archivedir) len = 0;
		else if (!(len = dirmatchlen(archivedir, filelist[i].name)))
			continue;

		cp = filelist[i].name + len;
		if (len > 0 && (len > 1 || *archivedir != _SC_)) cp++;
		if (!filelist[i].name[len]) {
			memcpy((char *)&(arcflist[0]),
				(char *)&(filelist[i]), sizeof(namelist));
			arcflist[0].name = filelist[0].name;
			continue;
		}
		if ((tmp = strdelim(cp, 0))) while (*(++tmp) == _SC_);
		if ((!tmp || !*tmp)
		&& (!re || regexp_exec(re, cp, 1))
		&& (!arcre || searcharc(arcre, filelist, max, i))) {
			memcpy((char *)&(arcflist[maxarcf]),
				(char *)&(filelist[i]), sizeof(namelist));
			arcflist[maxarcf].name = cp;
			if (isfile(&(arcflist[maxarcf])))
				totalsize +=
					getblock(arcflist[maxarcf].st_size);
			maxarcf++;
		}
	}
	if (re) regexp_free(re);
	if (!maxarcf) {
		arcflist[0].name = NOFIL_K;
		arcflist[0].st_nlink = -1;
	}

	if (sorton) qsort(arcflist, maxarcf, sizeof(namelist), cmplist);

	if (stable_standout) {
		putterms(t_clear);
		title();
		helpbar();
	}
	rewritearc(-1);

	old = filepos = listupfile(arcflist, maxarcf, file);
	fstat = 0;
	no = 1;

	keyflush();
	for (;;) {
		if (no) movepos(arcflist, maxarcf, old, fstat);
		locate(0, 0);
		tflush();
#ifdef	_NOEDITMODE
		ch = Xgetkey(SIGALRM);
#else
		Xgetkey(-1);
		ch = Xgetkey(SIGALRM);
		Xgetkey(-1);
#endif

		old = filepos;
		if (isearch) {
			no = searchmove(arcflist, maxarcf, ch, buf);
			if (no >= 0) {
				fnameofs = 0;
				continue;
			}
		}
		for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			if (ch == (int)(bindlist[i].key)) break;
		no = (bindlist[i].d_func < 255
			&& isdir(&(arcflist[filepos]))) ?
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
			helpbar();
			rewritearc(-1);
		}
	}

	if (no <= 4) {
		strcpy(file, arcflist[filepos].name);
		return(no);
	}

	if (findpattern) free(findpattern);
	findpattern = NULL;
	if (filepos >= 0 && strcmp(arcflist[filepos].name, "..")) {
		if (*(cp = archivedir) && (*cp != _SC_ || *(cp + 1)))
			cp = strcatdelim(archivedir);
		strcpy(cp, arcflist[filepos].name);
		*file = '\0';
	}
	else if (!*archivedir) no = -1;
	else {
		cp = archivedir + (int)strlen(archivedir) - 1;
		while (cp > archivedir && *cp == _SC_) cp--;
#if	MSDOS
		if (onkanji1(archivedir, cp - archivedir)) cp++;
#endif
		cp = strrdelim2(archivedir, cp);
		if (cp) strcpy(file, cp + 1);
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
	char *cp, *dir, path[MAXPATHLEN], file[MAXNAMLEN + 1];
	int i, dupmaxarcf, dupfilepos, dupdispmode, dupsorton;
#ifndef	_NODOSDRIVE
	int drive = 0;
#endif

	for (i = 0; i < maxlaunch; i++) {
		re = regexp_init(launchlist[i].ext, -1);
		if (regexp_exec(re, list[filepos].name, 0)) break;
		regexp_free(re);
	}
	if (i >= maxlaunch) return(-1);
	regexp_free(re);

	dir = NULL;
	if ((archivefile && !(dir = tmpunpack(list, max, 1)))
#ifndef	_NODOSDRIVE
	|| (drive = tmpdosdupl("", &dir, list, max, 1)) < 0
#endif
	) {
		putterm(t_bell);
		return(1);
	}
	if (launchlist[i].topskip == 255) {
		execusercomm(launchlist[i].comm, list[filepos].name,
			NULL, NULL, 1, 0);
#ifndef	_NODOSDRIVE
		if (drive) removetmp(dir, NULL, list[filepos].name);
		else
#endif
		if (archivefile)
			removetmp(dir, archivedir, list[filepos].name);
		return(0);
	}

	dupfullpath = NULL;
#ifndef	_NODOSDRIVE
	if (drive) {
		dupfullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
	}
	else
#endif
	if (archivefile) {
		dupfullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
		if (*archivedir) strcpy(strcatdelim(fullpath), archivedir);
	}
	duparchivefile = archivefile;
	duparchivedir = archivedir;
	duparcflist = arcflist;
	dupmaxarcf = maxarcf;
	dupfilepos = filepos;
	dupfindpat = findpattern;
	dupdispmode = dispmode;
	dupsorton = sorton;
	duplaunchp = launchp;

	archivefile = cp = strdup2(list[filepos].name);
	archivedir = path;
	findpattern = NULL;
	dispmode &= ~(F_SYMLINK | F_FILEFLAG);
	sorton = 0;
	launchp = &(launchlist[i]);
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
	if (findpattern) free(findpattern);
	findpattern = dupfindpat;
	dispmode = dupdispmode;
	sorton = dupsorton;
	launchp = duplaunchp;

#ifndef	_NODOSDRIVE
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
			if ((cp = strrdelim(filelist[i].name, 1))) cp++;
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
	char *duparchivefile;
	int i;

	for (i = 0; i < maxarchive; i++) {
		re = regexp_init(archivelist[i].ext, -1);
		if (archivelist[i].p_comm && regexp_exec(re, arc, 0)) break;
		regexp_free(re);
	}
	if (i >= maxarchive) return(-1);
	regexp_free(re);

	waitmes();
	duparchivefile = archivefile;
	archivefile = NULL;
	execusercomm(archivelist[i].p_comm, arc, list, &max, -1, 1);
	archivefile = duparchivefile;
	return(1);
}

/*ARGSUSED*/
int unpack(arc, dir, list, max, arg, tr)
char *arc, *dir;
namelist *list;
int max;
char *arg;
int tr;
{
	reg_t *re;
	char *tmp, *tmpdir, path[MAXPATHLEN];
	int i;
#ifndef	_NODOSDRIVE
	namelist alist[1];
	int dd, drive, dupfilepos;
#endif

	for (i = 0; i < maxarchive; i++) {
		re = regexp_init(archivelist[i].ext, -1);
		if (archivelist[i].u_comm && regexp_exec(re, arc, 0)) break;
		regexp_free(re);
	}
	if (i >= maxarchive) return(-1);
	regexp_free(re);

	tmpdir = NULL;
	if (dir) strcpy(path, dir);
	else {
#ifndef	_NOTREE
		if (tr) {
#ifdef	_NODOSDRIVE
			dir = tree(0, (int *)1);
#else
			dir = tree(0, &dd);
			if (dd >= 0) shutdrv(dd);
#endif
		}
		else
#endif	/* !_NOTREE */
		{
			if (arg && *arg) dir = strdup2(arg);
			else dir = inputstr(UNPAC_K, 1, -1, NULL, 1);
			dir = evalpath(dir, 1);
		}
		if (!dir) return(0);
		if (!*dir) strcpy(path, ".");
		else {
			tmp = strcpy2(path, dir);
#if	MSDOS || !defined (_NODOSDRIVE)
			if (_dospath(dir) && !dir[2]) strcpy(tmp, ".");
#endif
		}
		free(dir);
		dir = NULL;
	}
#ifndef	_NODOSDRIVE
	if (dospath2(path)) {
		warning(ENOENT, path);
		return(0);
	}

	dupfilepos = filepos;
	alist[0].name = arc;
	alist[0].st_mode = 0700;
	filepos = 0;
	drive = tmpdosdupl(arc, &tmpdir, alist, 1, 0);
	filepos = dupfilepos;
	if (drive < 0) {
		warning(-1, arc);
		return(0);
	}
#endif
	if ((!dir && preparedir(path) < 0) || _chdir2(path) < 0) {
		warning(-1, path);
		return(0);
	}

#ifdef	_NODOSDRIVE
	strcpy(path, fullpath);
#else
	if (!(drive = dospath2(fullpath))) strcpy(path, fullpath);
	else {
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
		strcpy(strcatdelim(path), tmpfilename);
		tmp = strcatdelim(path);
		*(tmp++) = 'D';
		*(tmp++) = drive;
		*tmp = '\0';
	}
#endif
	strcpy(strcatdelim(path), arc);
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

char *tmpunpack(list, max, single)
namelist *list;
int max, single;
{
	char *subdir, path[MAXPATHLEN];
	int i, ret, dupmark;

	sprintf(path, "L%d", launchlevel);
	if (mktmpdir(path) < 0) {
		warning(-1, path);
		return(NULL);
	}

	for (i = 0; i < max; i++)
		if (ismark(&(list[i]))) list[i].tmpflags |= F_WSMRK;
	dupmark = mark;
	if (single) mark = 0;
	ret = unpack(archivefile, path, list, max, NULL, 0);
	mark = dupmark;
	for (i = 0; i < max; i++) {
		if (wasmark(&(list[i]))) list[i].tmpflags |= F_ISMRK;
		list[i].tmpflags &= ~F_WSMRK;
	}

	if (_chdir2(path) < 0) error(path);
	subdir = archivedir;
	while (*subdir == _SC_) subdir++;
	if (ret < 0) putterm(t_bell);
	else if (ret > 0) {
		if (*subdir && _chdir2(subdir) < 0) {
			warning(-1, subdir);
			ret = 0;
		}
		else if (single || mark <= 0) {
			if (Xaccess(list[filepos].name, F_OK) < 0)
				warning(-1, list[filepos].name);
			else return(strdup2(path));
		}
		else {
			for (i = 0; i < max; i++)
				if (ismark(&(list[i]))
				&& Xaccess(list[i].name, F_OK) < 0) {
					warning(-1, list[i].name);
					break;
				}
			if (i >= max) return(strdup2(path));
		}
	}
	if (ret <= 0) removetmp(strdup2(path), NULL, NULL);
	else if (single || mark <= 0)
		removetmp(strdup2(path), subdir, list[filepos].name);
	else removetmp(strdup2(path), subdir, "");
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
		if (ismark(&(list[i]))) list[i].tmpflags |= F_ISARG;
	n_args = mark;

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
	for (i = 0; i < max; i++) list[i].tmpflags &= ~(F_ISARG | F_ISMRK);
	return(1);
}

int searcharc(regstr, flist, maxf, n)
char *regstr;
namelist *flist;
int maxf, n;
{
	namelist tmp;
	launchtable *list;
	reg_t *re;
	FILE *fp;
	char *cp, *buf, *dir, *file, *line;
	int i, c, no, len, max, match, dupfilepos;

	if (n < 0) {
		file = flist -> name;
		if (isdir(flist)) return(0);
	}
	else {
		file = flist[n].name;
		if (isdir(&(flist[n]))) return(0);
	}

	for (i = 0; i < maxlaunch; i++) {
		re = regexp_init(launchlist[i].ext, -1);
		if (launchlist[i].topskip < 255 && regexp_exec(re, file, 0))
			break;
		regexp_free(re);
	}
	if (i >= maxlaunch) return(0);
	regexp_free(re);
	list = &(launchlist[i]);

	if (n >= 0) {
		dupfilepos = filepos;
		filepos = n;
		dir = tmpunpack(flist, maxf, 1);
		filepos = dupfilepos;
		if (!dir) return(0);
	}

	if (!(cp = evalcommand(list -> comm, file, NULL, 0, NULL))) {
		if (n >= 0) removetmp(dir, NULL, file);
		return(0);
	}
#ifdef	DEBUG
	_mtrace_file = "popen";
#endif
	fp = Xpopen(cp, "r");
	free(cp);
	if (!fp) {
		warning(-1, list -> comm);
		return(-1);
	}

	locate(0, LMESLINE);
	putterm(l_clear);
	putterm(t_standout);
	kanjiputs(SEACH_K);
	putterm(end_standout);
	kanjiputs(file);
	tflush();

	max = 0;
	for (i = 0; i < MAXLAUNCHFIELD; i++)
		if (list -> field[i] < 255 && (int)(list -> field[i]) > max)
			max = (int)(list -> field[i]);

	for (i = 0; i < (int)(list -> topskip); i++) {
		if (!(line = fgets2(fp))) break;
		free(line);
	}

	i = no = len = match = 0;
	buf = NULL;
	re = regexp_init(regstr, -1);
	while ((line = fgets2(fp))) {
		if (match && match <= no - (int)(list -> bottomskip)) {
			free(line);
			if (buf) free(buf);
			break;
		}
		if (kbhit2(0) && ((c = getkey2(0)) == cc_intr || c == ESC)) {
			Xpclose(fp);
			warning(0, INTR_K);
			free(line);
			if (buf) free(buf);
			regexp_free(re);
			if (n >= 0) removetmp(dir, NULL, file);
			return(-1);
		}

		no++;
		if (list -> lines <= 1) buf = line;
		else {
			c = strlen(line);
			if (!(i++)) {
				buf = line;
				len = c;
				continue;
			}
			buf = realloc2(buf, len + c + 2);
			buf[len++] = '\t';
			memcpy(&(buf[len]), line, c);
			free(line);
			len += c;
			if (i < list -> lines) continue;

			buf[len] = '\0';
			i = len = 0;
		}

		c = readfileent(&tmp, buf, list, max);
		free(buf);
		buf = NULL;
		if (c < 0) continue;
		if (regexp_exec(re, tmp.name, 1)) match = no;
		free(tmp.name);
	}
	regexp_free(re);
	if (n >= 0) removetmp(dir, NULL, file);

	if (Xpclose(fp) < 0) return(0);
	if (match > no - (int)(list -> bottomskip)) match = 0;

	return(match);
}
#endif	/* !_NOARCHIVE */
