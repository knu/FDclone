/*
 *	browse.c
 *
 *	Directory Browsing Module
 */

#include <signal.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#if	MSDOS
extern int getcurdrv __P_((VOID_A));
extern int setcurdrv __P_((int));
#else
#include <sys/file.h>
#include <sys/param.h>
#endif

extern bindtable bindlist[];
extern functable funclist[];
extern char *findpattern;
#ifndef	_NOWRITEFS
extern int writefs;
#endif
extern char *curfilename;
extern char *origpath;

static VOID pathbar __P_((VOID_A));
static VOID stackbar __P_((VOID_A));
#if	!MSDOS
static char *putowner __P_((char *, uid_t));
static char *putgroup __P_((char *, gid_t));
#endif
static VOID calclocate __P_((int));
static int browsedir __P_((char *, char *));

int columns = 0;
int minfilename = 0;
int filepos = 0;
int mark = 0;
int fnameofs = 0;
int dispmode = 0;
int sorton = 0;
int sorttype = 0;
int chgorder = 0;
int stackdepth = 0;
int sizeinfo = 0;
long marksize = 0;
long totalsize = 0;
long blocksize = 0;
namelist filestack[MAXSTACK];
char fullpath[MAXPATHLEN + 1] = "";
char *macrolist[MAXMACROTABLE];
int maxmacro = 0;
namelist *filelist = NULL;
int maxfile = 0;
int maxent = 0;
int isearch = 0;
char *helpindex[10] = {
#ifdef	_NOTREE
	"help", "eXec", "Copy", "Delete", "Rename",
	"Sort", "Find", "Logdir", "Editor", "Unpack"
#else
	"Logdir", "eXec", "Copy", "Delete", "Rename",
	"Sort", "Find", "Tree", "Editor", "Unpack"
#endif
};
char typesymlist[] = "dbclsp";
u_short typelist[] = {
	S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFSOCK, S_IFIFO
};

static u_short modelist[] = {
	S_IFDIR, S_IFLNK, S_IFSOCK, S_IFIFO
};
static char suffixlist[] = "/@=|";
#ifndef	_NOCOLOR
int ansicolor = 0;
static u_char colorlist[] = {
	ANSI_CYAN, ANSI_YELLOW, ANSI_MAGENTA, ANSI_RED
};
#endif


static VOID pathbar(VOID_A)
{
	char *path;

	path = strdup2(fullpath);

	locate(0, LPATH);
	putterm(l_clear);
	putch2(' ');
	putterm(t_standout);
	cputs2("Path:");
	putterm(end_standout);
	kanjiputs2(path, n_column - 6, 0);
	free(path);

	tflush();
}

VOID helpbar(VOID_A)
{
	int i, j, width, len, ofs;

	width = (n_column - 10) / 10 - 1;
	ofs = (n_column - (width + 1) * 10 - 2) / 2;

	locate(0, LHELP);
	putterm(l_clear);
	putch2(isdisplnk(dispmode) ? 'S' : ' ');
	putch2(isdisptyp(dispmode) ? 'T' : ' ');
	putch2(ishidedot(dispmode) ? 'H' : ' ');

	for (i = 0; i < 10; i++) {
		locate(ofs + (width + 1) * i + (i / 5) * 3, LHELP);
		len = (width - strlen(helpindex[i])) / 2;
		putterm(t_standout);
		for (j = 0; j < len; j++) putch2(' ');
		kanjiputs2(helpindex[i], width - len, 0);
		putterm(end_standout);
	}

	tflush();
}

VOID statusbar(max)
int max;
{
	char *str[6];
	int width;

	locate(0, LSTATUS);
	putterm(l_clear);

	locate(CPAGE, LSTATUS);
	putterm(t_standout);
	cputs2("Page:");
	putterm(end_standout);
	cprintf2("%2d/%2d ",
		filepos / FILEPERPAGE + 1, (max - 1) / FILEPERPAGE + 1);

	locate(CMARK, LSTATUS);
	putterm(t_standout);
	cputs2("Mark:");
	putterm(end_standout);
	cprintf2("%4d/%4d ", mark, max);

	locate(CSORT, LSTATUS);
	putterm(t_standout);
	cputs2("Sort:");
	putterm(end_standout);

	if (sorton & 7) {
		str[0] = ONAME_K;
		str[1] = OEXT_K;
		str[2] = OSIZE_K;
		str[3] = ODATE_K;
		str[4] = OLEN_K;
		kanjiputs(str[(sorton & 7) - 1] + 3);

		str[0] = OINC_K;
		str[1] = ODEC_K;
		putch2('(');
		kanjiputs(str[sorton / 8] + 3);
		putch2(')');
	}
	else kanjiputs(ORAW_K + 3);

	locate(CFIND, LSTATUS);
	putterm(t_standout);
	cputs2("Find:");
	putterm(end_standout);
	if (findpattern) {
		width = n_column - (CFIND + 5);
		kanjiputs2(findpattern, width, 0);
	}

	tflush();
}

static VOID stackbar(VOID_A)
{
#ifndef	_NOCOLOR
	int j, x, color;
#endif
	int i, width;

	width = n_column / MAXSTACK;

	locate(0, LSTACK);
#ifndef	_NOCOLOR
	if (ansicolor == 2) chgcolor(ANSI_BLACK, 1);
	x = 0;
#endif
	putterm(l_clear);

	for (i = 0; i < stackdepth; i++) {
#ifndef	_NOCOLOR
		if (ansicolor == 2) for (; x < width * i + 1; x++) putch2(' ');
		else
#endif
		locate(width * i + 1, LSTACK);
#ifndef	_NOCOLOR
		if (!isread(&filestack[i])) color = ANSI_BLUE;
		else if (!iswrite(&filestack[i])) color = ANSI_GREEN;
		else {
			for (j = 0; j < sizeof(modelist) / sizeof(u_short); j++)
				if ((filestack[i].st_mode & S_IFMT)
				== modelist[j]) break;
			if (j < sizeof(modelist) / sizeof(u_short))
				color = colorlist[j];
			else if (ansicolor == 3) color = ANSI_BLACK;
			else color = ANSI_WHITE;
		}
		if (ansicolor) chgcolor(color, 1);
		else
#endif
		putterm(t_standout);
		kanjiputs2(filestack[i].name, width - 2, 0);
#ifndef	_NOCOLOR
		x += width - 2;
		if (ansicolor == 2) chgcolor(ANSI_BLACK, 1);
		else if (ansicolor) putterms(t_normal);
		else
#endif
		putterm(end_standout);
	}
#ifndef	_NOCOLOR
	if (ansicolor == 2) {
		for (; x < n_column; x++) putch2(' ');
		putterm(t_normal);
	}
#endif

	tflush();
}

VOID sizebar(VOID_A)
{
	char buf[16];
	long total, fre;

	if (!sizeinfo) return;

	locate(0, LSIZE);
	putterm(l_clear);

	locate(CSIZE, LSIZE);
	putterm(t_standout);
	cputs2("Size:");
	putterm(end_standout);
	cprintf2("%14.14s/", inscomma(buf, marksize, 3));
	cprintf2("%14.14s ", inscomma(buf, totalsize, 3));

	getinfofs(".", &total, &fre);

	locate(CTOTAL, LSIZE);
	putterm(t_standout);
	cputs2("Total:");
	putterm(end_standout);
	if (total < 0) cprintf2("%15.15s ", "?");
	else cprintf2("%12.12s KB ", inscomma(buf, total, 3));

	locate(CFREE, LSIZE);
	putterm(t_standout);
	cputs2("Free:");
	putterm(end_standout);
	if (fre < 0) cprintf2("%15.15s ", "?");
	else cprintf2("%12.12s KB ", inscomma(buf, fre, 3));

	tflush();
}

char *putmode(buf, mode)
char *buf;
u_short mode;
{
	int i;

	for (i = 0; i < sizeof(typelist) / sizeof(u_short); i++)
		if ((mode & S_IFMT) == typelist[i]) break;
	buf[0] = (i < sizeof(typelist) / sizeof(u_short))
		? typesymlist[i] : '-';
	buf[1] = (mode & S_IRUSR) ? 'r' : '-';
	buf[2] = (mode & S_IWUSR) ? 'w' : '-';
#if	MSDOS
	buf[3] = (mode & S_IXUSR) ? 'x' : '-';
	buf[4] = (mode & S_ISVTX) ? 'a' : '-';
	buf[5] = '\0';
#else
	buf[3] = (mode & S_ISUID) ? 's' : (mode & S_IXUSR) ? 'x' : '-';
	buf[4] = (mode & S_IRGRP) ? 'r' : '-';
	buf[5] = (mode & S_IWGRP) ? 'w' : '-';
	buf[6] = (mode & S_ISGID) ? 's' : (mode & S_IXGRP) ? 'x' : '-';
	buf[7] = (mode & S_IROTH) ? 'r' : '-';
	buf[8] = (mode & S_IWOTH) ? 'w' : '-';
	buf[9] = (mode & S_ISVTX) ? 't' : (mode & S_IXOTH) ? 'x' : '-';
	buf[10] = '\0';
#endif

	return(buf);
}

#if	!MSDOS
static char *putowner(buf, uid)
char *buf;
uid_t uid;
{
	char *str;
	int i;

	i = WOWNER;
	if ((str = getpwuid2(uid))) strncpy3(buf, str, &i, 0);
	else sprintf(buf, "%-*u", WOWNER, uid);

	return(buf);
}

static char *putgroup(buf, gid)
char *buf;
gid_t gid;
{
	char *str;
	int i;

	i = WGROUP;
	if ((str = getgrgid2(gid))) strncpy3(buf, str, &i, 0);
	else sprintf(buf, "%-*u", WGROUP, gid);

	return(buf);
}
#endif	/* !MSDOS */

VOID infobar(list, no)
namelist *list;
int no;
{
	char buf[MAXLINESTR + 1];
	struct tm *tm;
	int len, width;

	locate(0, LINFO);

	if (list[no].st_nlink < 0) {
		putterm(l_clear);
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

#if	!MSDOS
	putowner(buf + len, list[no].st_uid);
	len += WOWNER;
	strcpy(buf + (len++), " ");
	putgroup(buf + len, list[no].st_gid);
	len += WGROUP;
	strcpy(buf + (len++), " ");
#endif

	if (isdev(&list[no])) sprintf(buf + len, "%3u, %3u ",
		((unsigned)(list[no].st_size) >> 8) & 0xff,
		(unsigned)(list[no].st_size) & 0xff);
	else sprintf(buf + len, "%8ld ", (long)(list[no].st_size));
	len = strlen(buf);

	sprintf(buf + len, "%02d-%02d-%02d %02d:%02d ",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min);
	len += WDATE + 1 + WTIME + 1;
	width = n_lastcolumn - len;
	strncpy3(buf + len, list[no].name, &width, fnameofs);

#if	!MSDOS
	if (islink(&list[no])) {
		len = strlen(buf);
		while (buf[--len] == ' ');
		len += 2;
		if (strlen3(list[no].name) > fnameofs) {
			if (len < n_lastcolumn) buf[len++] = '-';
			if (len < n_lastcolumn) buf[len++] = '>';
			len++;
		}
		if (len < n_lastcolumn) {
			width = n_lastcolumn - len;
			Xreadlink(list[no].name, buf + len, width);
		}
	}
#endif

	kanjiputs(buf);
	tflush();
}

VOID waitmes(VOID_A)
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
#ifndef	_NOCOLOR
	if (ansicolor == 2) {
		chgcolor(ANSI_BLACK, 1);
		locate(x, y + WHEADER);
		putch2(' ');
	}
	else
#endif
	locate(x + 1, y + WHEADER);
}

#if	MSDOS
#define	WIDTH1	(WMODE + 1 + WSECOND + 1)
#else
#define	WIDTH1	(WOWNER + 1 + WGROUP + 1 + WMODE + 1 + WSECOND + 1)
#endif
#define	WIDTH2	(WTIME + 1 + WDATE + 1)
#define	WIDTH3	(WSIZE + 1)

int calcwidth(VOID_A)
{
	int width;

	width = (n_column / columns) - 2 - 1;
	if (columns < 2 && width - WIDTH1 >= minfilename) width -= WIDTH1;
	if (columns < 3 && width - WIDTH2 >= minfilename) width -= WIDTH2;
	if (columns < 4 && width - WIDTH3 >= minfilename) width -= WIDTH3;
	return(width);
}

VOID putname(list, no, standout)
namelist *list;
int no, standout;
{
	char buf[MAXLINESTR + 1];
	struct tm *tm;
	int i, j, len, width;
#ifdef	CODEEUC
	int wid;
#endif
#ifndef	_NOCOLOR
	int color;
#endif

	calclocate(no);
	putch2(ismark(&list[no]) ? '*' : ' ');

	if (standout < 0 && stable_standout) {
		putterm(end_standout);
		calclocate(no);
		return;
	}

	width = calcwidth();
	i = (standout && fnameofs > 0) ? fnameofs : 0;
#ifdef	CODEEUC
	wid = width;
#endif
	i = strncpy3(buf, list[no].name, &width, i);

#ifdef	_NOCOLOR
	if (isdisptyp(dispmode) && i < width)
#endif
	{
		for (j = 0; j < sizeof(modelist) / sizeof(u_short); j++)
			if ((list[no].st_mode & S_IFMT) == modelist[j]) break;
#ifndef	_NOCOLOR
	}
	if (isdisptyp(dispmode) && i < width) {
#endif
		if (j < sizeof(modelist) / sizeof(u_short))
			buf[i] = suffixlist[j];
		else if ((list[no].st_mode & S_IFMT) == S_IFREG
		&& (list[no].st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
			buf[i] = '*';
	}
#ifndef	_NOCOLOR
	if (!isread(&list[no])) color = ANSI_BLUE;
	else if (!iswrite(&list[no])) color = ANSI_GREEN;
	else if (j < sizeof(modelist) / sizeof(u_short)) color = colorlist[j];
	else if (ansicolor == 3) color = ANSI_BLACK;
	else color = ANSI_WHITE;
#endif
	len = width;
#ifdef	CODEEUC
	width += (n_column / columns) - 2 - 1 - wid;
#else
	width = (n_column / columns) - 2 - 1;
#endif

	tm = NULL;
	if (columns < 5 && len + WIDTH3 <= width) {
		if (isdir(&list[no]))
			sprintf(buf + len, " %*.*s", WSIZE, WSIZE, "<DIR>");
		else if (
#if	!MSDOS && !defined (_NODOSDRIVE)
		dospath("", NULL) &&
#endif
		(list[no].st_mode & S_IFMT) == S_IFIFO)
			sprintf(buf + len, " %*.*s", WSIZE, WSIZE, "<VOL>");
		else if (isdev(&list[no])) sprintf(buf + len, " %*u,%*u",
			WSIZE / 2, ((unsigned)(list[no].st_size) >> 8) & 0xff,
			WSIZE - (WSIZE / 2) - 1,
			(unsigned)(list[no].st_size) & 0xff);
		else sprintf(buf + len, " %*ld",
			WSIZE, (long)(list[no].st_size));
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
		if (!tm) tm = localtime(&list[no].st_mtim);
		sprintf(buf + len, ":%02d", tm -> tm_sec);
		len += 1 + WSECOND;
		buf[len++] = ' ';
#if	!MSDOS
		putowner(buf + len, list[no].st_uid);
		len += WOWNER;
		buf[len++] = ' ';
		putgroup(buf + len, list[no].st_gid);
		len += WGROUP;
		buf[len++] = ' ';
#endif
		putmode(buf + len, (!isdisplnk(dispmode) && islink(&list[no])) ?
			(S_IFLNK | 0777) : list[no].st_mode);
	}

#ifndef	_NOCOLOR
	if (ansicolor) chgcolor(color, standout > 0);
	else
#endif
	if (standout > 0) putterm(t_standout);
	kanjiputs(buf);
#ifndef	_NOCOLOR
	if (ansicolor == 2) {
		chgcolor(ANSI_BLACK, 1);
		putch2(' ');
	}
	if (ansicolor) putterms(t_normal);
	else
#endif
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
		else cprintf2("%-*.*s", i, i, "No Files");
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
		if (!strpathcmp(def, list[i].name)) {
			ret = i;
			break;
		}
	}
	if (ret < 0) {
		start = 0;
		ret = (max <= 1 || strcmp(list[1].name, "..")) ? 0 : 1;
	}

	locate(6, LSTATUS);
	cprintf2("%2d", start / FILEPERPAGE + 1);

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

VOID rewritefile(all)
int all;
{
	pathbar();
	sizebar();
	if (all) {
		helpbar();
		infobar(filelist, filepos);
	}
	statusbar(maxfile);
	stackbar();
	listupfile(filelist, maxfile, filelist[filepos].name);
	locate(0, 0);
	tflush();
}

int searchmove(list, max, ch, buf)
namelist *list;
int max, ch;
char *buf;
{
	char *str[4];
	int i, n, s, pos, len;

	if (isearch > 0) {
		n = isearch;
		s = 1;
	}
	else {
		n = -isearch;
		s = -1;
	}
	len = strlen(buf);
	pos = filepos;

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
		if (ch == (int)(bindlist[i].key)) break;
	if (ch == K_BS) {
		if (!len) pos = -1;
		else buf[--len] = '\0';
	}
	else if (len && bindlist[i].f_func == SEARCH_FORW) {
		if (n > 2) pos++;
		else if (n > 1) pos = 0;
		s = 1;
	}
	else if (len && bindlist[i].f_func == SEARCH_BACK) {
		if (n > 2) pos--;
		else if (n > 1) pos = max - 1;
		s = -1;
	}
	else {
		if (n == 1) buf[len = 0] = '\0';
		if (ch < ' ' || ch == C_DEL || ch >= K_MIN) pos = -1;
		else if (len < MAXNAMLEN - 1) {
			buf[len++] = ch;
			buf[len] = '\0';
		}
	}

	if (pos < 0 || pos >= max) {
		isearch = 0;
		helpbar();
		if (ch == CR) return(-1);
		else if (ch != ESC) putterm(t_bell);
		return(0);
	}

	locate(0, LHELP);
	str[0] = SEAF_K;
	str[1] = SEAFF_K;
	str[2] = SEAB_K;
	str[3] = SEABF_K;

	i = 0;
	for (;;) {
		if (!strnpathcmp(list[pos].name, buf, len)) {
			i = 1;
			break;
		}
		pos += s;
		if (pos < 0 || pos >= max) break;
	}

	putterm(t_standout);
	len = kanjiputs(str[2 - s - i]);
	putterm(end_standout);
	kanjiputs2(buf, n_column - len - 1, 0);
	if (i) filepos = pos;
	else if (n != 2 && ch != K_BS) putterm(t_bell);
	isearch = s * (i + 2);
	return(i);
}

static int browsedir(file, def)
char *file, *def;
{
	DIR *dirp;
	struct dirent *dp;
	reg_t *re;
	u_char fstat;
	char *cp, buf[MAXNAMLEN + 1];
	int ch, i, no, old;

#ifndef	_NOCOLOR
	if (ansicolor) putterms(t_normal);
#endif

	maxfile = mark = 0;
	totalsize = marksize = 0;
	buf[0] = '\0';
	blocksize = getblocksize(".");
	chgorder = 0;
	if (sorttype < 100) sorton = sorttype;

	if (!(dirp = Xopendir("."))) error(".");
	fnameofs = 0;
	waitmes();

	if (!findpattern) re = NULL;
	else {
		cp = cnvregexp(findpattern, 1);
		re = regexp_init(cp);
		free(cp);
	}
	while ((dp = searchdir(dirp, re))) {
		if (ishidedot(dispmode) && *(dp -> d_name) == '.'
		&& !isdotdir(dp -> d_name)) continue;
		strcpy(file, dp -> d_name);
		for (i = 0; i < stackdepth; i++)
			if (!strcmp(file, filestack[i].name)) break;
		if (i < stackdepth) continue;

		filelist = (namelist *)addlist(filelist, maxfile,
			&maxent, sizeof(namelist));
		if (getstatus(filelist, maxfile, file) < 0) continue;
		filelist[maxfile].name = strdup2(file);
		filelist[maxfile].ent = maxfile;
		if (!isdir(&filelist[maxfile]))
			totalsize += 
				((filelist[maxfile].st_size + blocksize - 1)
				/ blocksize) * blocksize;
		maxfile++;
	}
	Xclosedir(dirp);
	if (re) regexp_free(re);

	if (!maxfile) {
		filelist = (namelist *)addlist(filelist, 0,
			&maxent, sizeof(namelist));
		filelist[0].name = NOFIL_K;
		filelist[0].st_nlink = -1;
	}

	if (sorton) qsort(filelist, maxfile, sizeof(namelist), cmplist);

	if (stable_standout) {
		putterms(t_clear);
		helpbar();
	}
	title();
	pathbar();
	sizebar();
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
#ifdef	_NOEDITMODE
		ch = Xgetkey(SIGALRM);
#else
		Xgetkey(-1);
		ch = Xgetkey(SIGALRM);
		Xgetkey(-1);
#endif

		old = filepos;
		if (isearch) {
			no = searchmove(filelist, maxfile, ch, buf);
			if (no >= 0) {
				fnameofs = 0;
				continue;
			}
		}
		for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			if (ch == (int)(bindlist[i].key)) break;
		no = (bindlist[i].d_func < 255 && isdir(&filelist[filepos])) ?
			(int)(bindlist[i].d_func) : (int)(bindlist[i].f_func);
		fstat = (no <= NO_OPERATION) ? funclist[no].stat
			: (KILLSTK | RELIST);

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
#ifndef	_NOWRITEFS
			else if (chgorder && writefs < 1 && no != WRITE_DIR
			&& (i = writablefs(".")) > 0 && underhome(NULL) > 0) {
				chgorder = 0;
				if (yesno(WRTOK_K))
					arrangedir(filelist, maxfile, i);
			}
#endif
		}

		curfilename = filelist[filepos].name;
		if (maxfile <= 0 && !(fstat & NO_FILE)) no = 0;
		else if (no <= NO_OPERATION)
			no = (*funclist[no].func)(filelist, &maxfile, NULL);
		else {
			no = execusercomm(macrolist[no - NO_OPERATION - 1],
				filelist[filepos].name,
				filelist, &maxfile, 0, 0);
			if (no < 0) no = 0;
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
			sizebar();
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
	int i, ischgdir;

	findpattern = def = NULL;
	if (cur) {
		cp = evalpath(strdup2(cur), 1);
#if	MSDOS
		if (_dospath(cp)) {
			if (setcurdrv(*cp) < 0) fullpath[0] = getcurdrv();
			else {
				def = getwd2();
				strcpy(fullpath, def);
				def = NULL;
			}
			for (i = 2; cp[i]; i++) cp[i - 2] = cp[i];
			cp [i - 2] = '\0';
		}
		if (*cp == _SC_) fullpath[2] = '\0';
		strcat(fullpath, _SS_);
#else	/* !MSDOS */
		if (
#ifndef	_NODOSDRIVE
		_dospath(cp) ||
#endif
		*cp == _SC_) *fullpath = '\0';
		else strcat(fullpath, _SS_);
#endif	/* !MSDOS */
		if (_chdir2(cp) >= 0) {
			strcat(fullpath, cp);
			free(cp);
			cp = fullpath + (int)strlen(fullpath) - 1;
		}
		else {
			def = strrdelim(cp);
#if	!MSDOS && !defined (_NODOSDRIVE)
			if (!def && _dospath(cp)) def = &cp[2];
#endif
			if (def) {
				i = *def;
				*def = '\0';
				if (def == cp) {
					if (_chdir2(_SS_) < 0) error(_SS_);
#if	MSDOS
					strcpy(fullpath + 2, _SS_);
#else
					strcpy(fullpath, _SS_);
#endif
				}
				else if (_chdir2(cp) >= 0) strcat(fullpath, cp);
				else {
					warning(-1, cp);
					def = NULL;
					strcpy(fullpath, origpath);
				}
				if (def && (*def = i) == _SC_) def++;
			}
			else def = cp;

			if (def) {
				strcpy(prev, def);
				def = prev;
			}
			free(cp);
		}
		realpath2(fullpath, fullpath);
		entryhist(1, fullpath, 1);
	}

	filelist = NULL;
	maxent = 0;
	strcpy(file, ".");
	sorton = sorttype % 100;

	for (;;) {
		if (!def && !strcmp(file, "..")) {
			if ((cp = strrdelim(fullpath))) cp++;
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

		ischgdir = browsedir(file, def);
		if (ischgdir < 0) break;
		if (ischgdir) def = NULL;
		else {
			strcpy(prev, file);
			strcpy(file, ".");
			def = prev;
		}
	}

	if (filelist) free(filelist);
}
