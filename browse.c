/*
 *	browse.c
 *
 *	Directory Browsing Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#include <time.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>

extern bindtable bindlist[];
extern functable funclist[];
extern char *findpattern;
extern int sorttype;
extern int writefs;
extern int minfilename;

static VOID pathbar();
static VOID stackbar();
static char *putowner();
static char *putgroup();
static VOID infobar();
static VOID calclocate();
static VOID putname();
static int browsedir();

int columns;
int filepos;
int mark;
int fnameofs;
int dispmode;
int sorton;
int chgorder;
int stackdepth = 0;
namelist filestack[MAXSTACK];
char fullpath[MAXPATHLEN + 1];
char *lastpath = NULL;
char *origpath;
char *macrolist[MAXMACROTABLE];
int maxmacro = 0;
char *helpindex[10] = {
	"Logdir", "eXec", "Copy", "Delete", "Rename",
	"Sort", "Find", "Tree", "Editor", "Unpack"
};

static namelist *filelist;
static int maxfile;


static VOID pathbar()
{
	char *path;

	path = strdup2(fullpath);

	locate(0, LPATH);
	putterm(l_clear);
	putch(' ');
	putterm(t_standout);
	cputs("Path:");
	putterm(end_standout);
	kanjiputs2(path, n_column - 6, 0);
	free(path);

	tflush();
}

VOID helpbar()
{
	char *buf;
	int i, j, width, len, ofs;

	width = (n_column - 10) / 10 - 1;
	ofs = (n_column - (width + 1) * 10 - 2) / 2;
	buf = (char *)malloc2(width + 1);

	locate(0, LHELP);
	putterm(l_clear);
	putch(isdisplnk(dispmode) ? 'S' : ' ');
	putch(isdisptyp(dispmode) ? 'T' : ' ');
	putch(ishidedot(dispmode) ? 'H' : ' ');

	for (i = 0; i < 10; i++) {
		locate(ofs + (width + 1) * i + (i / 5) * 3, LHELP);
		len = (width - strlen(helpindex[i])) / 2;
		for (j = 0; j < len; j++) buf[j] = ' ';
		strncpy2(buf + len, helpindex[i], width - len);
		len = strlen(buf);
		for (j = len; j < width; j++) buf[j] = ' ';
		putterm(t_standout);
		cputs(buf);
		putterm(end_standout);
	}
	free(buf);

	tflush();
}

VOID statusbar(max)
int max;
{
	char *str[5];
	int width;

	locate(0, LSTATUS);
	putterm(l_clear);
	putch(' ');

	putterm(t_standout);
	cputs("Page:");
	putterm(end_standout);
	cprintf("%2d/%2d ",
		filepos / FILEPERPAGE + 1, (max - 1) / FILEPERPAGE + 1);

	putterm(t_standout);
	cputs("Mark:");
	putterm(end_standout);
	cprintf("%4d/%4d ", mark, max);

	putterm(t_standout);
	cputs("Sort:");
	putterm(end_standout);

	if (sorton & 7) {
		str[0] = ONAME_K;
		str[1] = OEXT_K;
		str[2] = OSIZE_K;
		str[3] = ODATE_K;
		kanjiputs(str[(sorton & 7) - 1] + 3);

		str[0] = OINC_K;
		str[1] = ODEC_K;
		putch('(');
		kanjiputs(str[sorton / 8] + 3);
		putch(')');
	}
	else kanjiputs(ORAW_K + 3);

	locate(CFIND, LSTATUS);
	putterm(t_standout);
	cputs("Find:");
	putterm(end_standout);
	if (findpattern) {
		width = n_column - (CFIND + 5);
		kanjiputs2(findpattern, width, 0);
	}

	tflush();
}

static VOID stackbar()
{
	int i, width;

	width = n_column / MAXSTACK;

	locate(0, LSTACK);
	putterm(l_clear);

	for (i = 0; i < stackdepth; i++) {
		locate(width * i + 1, LSTACK);
		putterm(t_standout);
		kanjiputs2(filestack[i].name, width - 2, 0);
		putterm(end_standout);
	}

	tflush();
}

char *putmode(buf, mode)
char *buf;
u_short mode;
{
	switch(mode & S_IFMT) {
		case S_IFDIR:
			buf[0] = 'd';
			break;
		case S_IFBLK:
			buf[0] = 'b';
			break;
		case S_IFCHR:
			buf[0] = 'c';
			break;
		case S_IFLNK:
			buf[0] = 'l';
			break;
		case S_IFSOCK:
			buf[0] = 's';
			break;
		case S_IFIFO:
			buf[0] = 'p';
			break;
		default:
			buf[0] = '-';
			break;
	}

	buf[1] = (mode & S_IRUSR) ? 'r' : '-';
	buf[2] = (mode & S_IWUSR) ? 'w' : '-';
	buf[3] = (mode & S_ISUID) ? 's' : (mode & S_IXUSR) ? 'x' : '-';
	buf[4] = (mode & S_IRGRP) ? 'r' : '-';
	buf[5] = (mode & S_IWGRP) ? 'w' : '-';
	buf[6] = (mode & S_ISGID) ? 's' : (mode & S_IXGRP) ? 'x' : '-';
	buf[7] = (mode & S_IROTH) ? 'r' : '-';
	buf[8] = (mode & S_IWOTH) ? 'w' : '-';
	buf[9] = (mode & S_ISVTX) ? 't' : (mode & S_IXOTH) ? 'x' : '-';
	buf[10] = '\0';

	return(buf);
}

static char *putowner(buf, uid)
char *buf;
uid_t uid;
{
	char *str;

	if (str = getpwuid2(uid)) strncpy3(buf, str, WOWNER, 0);
	else sprintf(buf, "%-*d", WOWNER, uid);

	return(buf);
}

static char *putgroup(buf, gid)
char *buf;
gid_t gid;
{
	char *str;

	if (str = getgrgid2(gid)) strncpy3(buf, str, WGROUP, 0);
	else sprintf(buf, "%-*d", WGROUP, gid);

	return(buf);
}

static VOID infobar(list, no)
namelist *list;
int no;
{
	char buf[MAXLINESTR + 1];
	struct tm *tm;
	int len, width;

	locate(0, LINFO);

	if (list[no].st_nlink < 0) {
		kanjiputs(list[no].name);
		tflush();
		return;
	}

	tm = localtime(&list[no].st_mtim);

	putmode(buf, (!isdisplnk(dispmode) && islink(&list[no])) ?
		(S_IFLNK | 0777) : list[no].st_mode);
	len = WMODE;

	sprintf(buf + len, " %2d ", list[no].st_nlink);
	len += 4;

	putowner(buf + len, list[no].st_uid);
	len += WOWNER;
	strcpy(buf + (len++), " ");
	putgroup(buf + len, list[no].st_gid);
	len += WGROUP;
	strcpy(buf + (len++), " ");

	if (isdev(&list[no])) sprintf(buf + len, "%3u, %3u ",
		((unsigned)(list[no].st_size) >> 8) & 0xff,
		(unsigned)(list[no].st_size) & 0xff);
	else sprintf(buf + len, "%8d ", (unsigned)(list[no].st_size));
	len = strlen(buf);

	sprintf(buf + len, "%02d-%02d-%02d %02d:%02d ",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min);
	len += WDATE + 1 + WTIME + 1;
	width = n_lastcolumn - len;
	strncpy3(buf + len, list[no].name, width, fnameofs);

	if (islink(&list[no])) {
		len = strlen(buf);
		while (buf[--len] == ' ');
		len += 2;
		if (list[no].name[fnameofs]) {
			if (len < n_lastcolumn) buf[len++] = '-';
			if (len < n_lastcolumn) buf[len++] = '>';
			len++;
		}
		if (len < n_lastcolumn) {
			width = n_lastcolumn - len;
			readlink(list[no].name, buf + len, width);
		}
	}

	kanjiputs(buf);
	tflush();
}

VOID waitmes()
{
	helpbar();
	locate(0, LMESLINE);
	putterm(l_clear);
	kanjiputs(WAIT_K);
	tflush();
}

static VOID calclocate(i)
int i;
{
	int x, y;

	i %= FILEPERPAGE;
	x = (i / FILEPERLOW) * (n_column / FILEPERLINE);
	y = i % FILEPERLOW;
	locate(x + 1, y + WHEADER);
}

#define	WIDTH1	(WOWNER + 1 + WGROUP + 1 + WMODE + 1 + WSECOND + 1)
#define	WIDTH2	(WTIME + 1 + WDATE + 1)
#define	WIDTH3	(WSIZE + 1)

int calcwidth()
{
	int width;

	width = (n_column / columns) - 2 - 1;
	if (columns < 2 && width - WIDTH1 >= minfilename) width -= WIDTH1;
	if (columns < 3 && width - WIDTH2 >= minfilename) width -= WIDTH2;
	if (columns < 4 && width - WIDTH3 >= minfilename) width -= WIDTH3;
	return(width);
}

static VOID putname(list, no, standout)
namelist *list;
int no;
int standout;
{
	char buf[MAXLINESTR + 1];
	struct tm *tm;
	int i, len, width;

	calclocate(no);
	putch(ismark(&list[no]) ? '*' : ' ');

	if (standout < 0 && stable_standout) {
		putterm(end_standout);
		calclocate(no);
		return;
	}

	width = calcwidth();
	i = (standout && fnameofs > 0) ? fnameofs : 0;
	strncpy3(buf, list[no].name, width, i);
	if (isdisptyp(dispmode)) {
		for (i = 0; i < width; i++) if (buf[i] == ' ') {
			switch (list[no].st_mode & S_IFMT) {
				case S_IFDIR:
					buf[i] = '/';
					break;
				case S_IFLNK:
					buf[i] = '@';
					break;
				case S_IFSOCK:
					buf[i] = '=';
					break;
				case S_IFIFO:
					buf[i] = '|';
					break;
				case S_IFREG:
					if (list[no].st_mode &
					(S_IXUSR | S_IXGRP | S_IXOTH))
						buf[i] = '*';
					break;
				default:
					break;
			}
			break;
		}
	}
	len = width;
	width = (n_column / columns) - 2 - 1;

	if (columns < 5 && len + WIDTH3 <= width) {
		if (isdir(&list[no]))
			sprintf(buf + len, " %*.*s", WSIZE, WSIZE, "<DIR>");
		else if (isdev(&list[no])) sprintf(buf + len, " %*u,%*u",
			WSIZE / 2, ((unsigned)(list[no].st_size) >> 8) & 0xff,
			WSIZE - (WSIZE / 2) - 1,
			(unsigned)(list[no].st_size) & 0xff);
		else sprintf(buf + len, " %*d",
			WSIZE, (unsigned)(list[no].st_size));
		if (strlen(buf + len) > WSIZE + 1)
			sprintf(buf + len, " %*.*s", WSIZE, WSIZE, "OVERFLOW");
		len += WIDTH3;
	}
	if (columns < 3 && len + WIDTH2 <= width) {
		tm = localtime(&list[no].st_mtim);
		sprintf(buf + len, " %02d-%02d-%02d %2d:%02d",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min);
		len += WIDTH2;
	}
	if (columns < 2 && len + WIDTH1 <= width) {
		sprintf(buf + len, ":%02d", tm -> tm_sec);
		len += 1 + WSECOND;
		buf[len++] = ' ';
		putowner(buf + len, list[no].st_uid);
		len += WOWNER;
		buf[len++] = ' ';
		putgroup(buf + len, list[no].st_gid);
		len += WGROUP;
		buf[len++] = ' ';
		putmode(buf + len, (!isdisplnk(dispmode) && islink(&list[no])) ?
			(S_IFLNK | 0777) : list[no].st_mode);
	}

	if (standout > 0) putterm(t_standout);
	kanjiputs(buf);
	if (standout > 0) putterm(end_standout);
	tflush();
}

int listupfile(list, max, def)
namelist *list;
int max;
char *def;
{
	char *cp;
	int i, count, start, ret;

	for (i = 0; i < FILEPERLOW; i++) {
		locate(0, i + WHEADER);
		putterm(l_clear);
	}

	if (max <= 0) {
		i = (n_column / columns) - 2 - 1;
		locate(1, WHEADER);
		putterm(t_standout);
		cp = NOFIL_K;
		if (i > strlen(cp)) kanjiputs2(cp, i, 0);
		else cprintf("%-*.*s", i, i, "No Files");
		putterm(end_standout);
		tflush();
		return(0);
	}

	ret = -1;
	if (!def) def = "..";

	start = 0;
	for (i = count = 0; i < max; i++, count++) {
		if (count >= FILEPERPAGE) {
			count = 0;
			start = i;
		}
		if (!strcmp(def, list[i].name)) {
			ret = i;
			break;
		}
	}
	if (ret < 0) {
		start = 0;
		ret = (max <= 1 || strcmp(list[1].name, "..")) ? 0 : 1;
	}

	locate(6, LSTATUS);
	cprintf("%2d", start / FILEPERPAGE + 1);

	for (i = start, count = 0; i < max; i++, count++) {
		if (count >= FILEPERPAGE) break;
		putname(list, i, (i == ret) ? 1 : 0);
	}

	tflush();
	return(ret);
}

VOID movepos(list, max, old, fstat)
namelist *list;
int max, old;
u_char fstat;
{
	if ((fstat & LISTUP) || old / FILEPERPAGE != filepos / FILEPERPAGE) {
		filepos = listupfile(list, max, list[filepos].name);
		keyflush();
	}
	else if ((fstat & REWRITE) || old != filepos) {
		if (old != filepos) putname(list, old, -1);
		putname(list, filepos, 1);
	}
	infobar(list, filepos);
}

VOID rewritefile()
{
	pathbar();
	helpbar();
	statusbar(maxfile);
	stackbar();
	movepos(filelist, maxfile, filepos, LISTUP);
	locate(0, 0);
	tflush();
}

static int browsedir(file, def, maxentp)
char *file, *def;
int *maxentp;
{
	DIR *dirp;
	struct dirent *dp;
	reg_t *re;
	u_char fstat;
	char *cp;
	int ch, i, no, old;

	maxfile = mark = 0;
	chgorder = 0;
	if (sorttype < 100) sorton = sorttype;

	if (!(dirp = opendir("."))) error(".");
	fnameofs = 0;
	waitmes();

	if (!findpattern) re = NULL;
	else {
		cp = cnvregexp(findpattern, 1);
		re = regexp_init(cp);
		free(cp);
	}
	while (dp = searchdir(dirp, re)) {
		if (ishidedot(dispmode) && *(dp -> d_name) == '.'
		&& strcmp(dp -> d_name, ".")
		&& strcmp(dp -> d_name, "..")) continue;
		strcpy(file, dp -> d_name);
		for (i = 0; i < stackdepth; i++)
			if (!strcmp(file, filestack[i].name)) break;
		if (i < stackdepth) continue;

		filelist = (namelist *)addlist(filelist, maxfile,
			maxentp, sizeof(namelist));
		if (getstatus(filelist, maxfile, file) < 0) continue;
		filelist[maxfile].name = strdup2(file);
		filelist[maxfile].ent = maxfile;
		maxfile++;
	}
	closedir(dirp);
	if (re) regexp_free(re);

	if (!maxfile) {
		filelist = (namelist *)addlist(filelist, 0,
			maxentp, sizeof(namelist));
		filelist[0].name = NOFIL_K;
		filelist[0].st_nlink = -1;
	}

	if (sorton) qsort(filelist, maxfile, sizeof(namelist), cmplist);

	if (stable_standout) putterms(t_clear);
	title();
	pathbar();
	statusbar(maxfile);
	stackbar();

	old = filepos = listupfile(filelist, maxfile, def);
	fstat = 0;
	no = 1;

	keyflush();
	for (;;) {
		if (no) movepos(filelist, maxfile, old, fstat);
		locate(0, 0);
		tflush();
		ch = getkey(SIGALRM);

		old = filepos;
		for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			if (ch == (int)(bindlist[i].key)) break;
		no = (bindlist[i].d_func < 255 && isdir(&filelist[filepos])) ?
			(int)(bindlist[i].d_func) : (int)(bindlist[i].f_func);
		fstat = (no <= NO_OPERATION) ? funclist[no].stat
			: (KILLSTK | REWRITE);

		if (fstat & KILLSTK) {
			if (stackdepth > 0) {
				chgorder = 0;
				if (!yesno(KILOK_K)) continue;
				for (i = maxfile - 1; i > filepos; i--)
					memcpy(&filelist[i + stackdepth],
						&filelist[i], sizeof(namelist));
				for (i = 0; i < stackdepth; i++)
					memcpy(&filelist[i + filepos + 1],
						&filestack[i],
						sizeof(namelist));
				maxfile += stackdepth;
				stackdepth = 0;
				filepos = listupfile(filelist, maxfile,
					filelist[filepos].name);
				stackbar();
			}
#if (WRITEFS < 1)
			else if (chgorder && writefs < 1 && no != WRITE_DIR
			&& (i = writablefs(".")) > 0 && underhome() > 0) {
				chgorder = 0;
				if (yesno(WRTOK_K))
					arrangedir(filelist, maxfile, i);
			}
#endif
		}

		if (maxfile <= 0 && !(fstat & NO_FILE)) no = 0;
		else if (no <= NO_OPERATION)
			no = (*funclist[no].func)(filelist, &maxfile);
		else {
			execmacro(macrolist[no - NO_OPERATION - 1],
				filelist[filepos].name,
				filelist, maxfile, 0, 0);
			no = 4;
		}

		if (no < 0) break;
		if (no == 1 || no == 3) helpbar();
		if (no < 2) {
			fstat = 0;
			continue;
		}
		else if (no >= 4) {
			no -= 4;
			strcpy(file, filelist[filepos].name);
			break;
		}

		if (!(fstat & REWRITE)) fnameofs = 0;
		if ((fstat & RELIST) == RELIST) {
			title();
			pathbar();
			statusbar(maxfile);
			stackbar();
			helpbar();
		}
	}

	for (i = 0; i < maxfile; i++) free(filelist[i].name);
	return(no);
}

VOID main_fd(cur)
char *cur;
{
	char file[MAXNAMLEN + 1], prev[MAXNAMLEN + 1];
	char *cp, *def;
	int ischgdir, maxent;

	origpath = getwd2();
	strcpy(fullpath, origpath);

	findpattern = def = NULL;
	if (cur) {
		cp = evalpath(strdup2(cur));
		if (*cp == '/') *fullpath = '\0';
		else strcat(fullpath, "/");
		if (chdir(cp) >= 0) {
			strcat(fullpath, cp);
			free(cp);
			cp = fullpath + strlen(fullpath) - 1;
			if (cp > fullpath && *cp == '/') *cp = '\0';
		}
		else {
			def = strrchr(cp, '/');
			if (def) {
				*def = '\0';
				if (def++ == cp) {
					if (chdir("/") < 0) error("/");
					else strcpy(fullpath, "/");
				}
				else if (chdir(cp) >= 0) strcat(fullpath, cp);
				else {
					warning(-1, cp);
					def = NULL;
					strcpy(fullpath, origpath);
				}
			}
			else def = cp;

			if (def) {
				strcpy(prev, def);
				def = prev;
			}
			free(cp);
		}
	}

	filelist = NULL;
	maxent = 0;
	strcpy(file, ".");
	sorton = sorttype % 100;

	for (;;) {
		if (!def && !strcmp(file, "..")) {
			if (cp = strrchr(fullpath, '/')) cp++;
			else cp = fullpath;
			strcpy(prev, cp);
			if (!*prev) {
				strcpy(file, ".");
				def = NULL;
			}
			else def = prev;
		}

		if (strcmp(file, ".")) {
			if (findpattern) free(findpattern);
			findpattern = NULL;
			if (chdir2(file) < 0) {
				warning(-1, file);
				strcpy(prev, file);
				def = prev;
			}
		}

		ischgdir = browsedir(file, def, &maxent);
		if (ischgdir < 0) break;
		if (ischgdir) def = NULL;
		else {
			strcpy(prev, file);
			strcpy(file, ".");
			def = prev;
		}
	}

	free(filelist);
	if (chdir(origpath) < 0) error(origpath);
	free(origpath);
}
