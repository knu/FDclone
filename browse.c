/*
 *	browse.c
 *
 *	Directory Browsing Module
 */

#include "fd.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#if	MSDOS
extern int getcurdrv __P_((VOID_A));
extern int setcurdrv __P_((int, int));
#endif

#ifdef	USEMKDEVH
#include <sys/mkdev.h>
#else
# ifdef	USEMKNODH
# include <sys/mknod.h>
# else
#  ifdef	SVR4
#  include <sys/sysmacros.h>
#   ifndef	major
#   define	major(n)	((((unsigned long)(n)) >> 18) & 0x3fff)
#   endif
#   ifndef	minor
#   define	minor(n)	(((unsigned long)(n)) & 0x3ffff)
#   endif
#  else
#   ifndef	major
#   define	major(n)	((((unsigned)(n)) >> 8) & 0xff)
#   endif
#   ifndef	minor
#   define	minor(n)	(((unsigned)(n)) & 0xff)
#   endif
#  endif
# endif
#endif

#define	CL_NORM		0
#define	CL_BACK		1
#define	CL_DIR		2
#define	CL_RONLY	3
#define	CL_HIDDEN	4
#define	CL_LINK		5
#define	CL_SOCK		6
#define	CL_FIFO		7
#define	CL_BLOCK	8
#define	CL_CHAR		9
#define	ANSI_FG		8
#define	ANSI_BG		9

extern bindtable bindlist[];
extern functable funclist[];
#ifndef	_NOWRITEFS
extern int writefs;
#endif
extern char *curfilename;
extern char *origpath;
#ifndef	_NOARCHIVE
extern char archivedir[];
#endif
extern int win_x;
extern int win_y;
#ifndef	_NOCUSTOMIZE
extern int custno;
#endif
extern int internal_status;
extern int hideclock;
extern int fd_restricted;
extern int physical_path;

#ifndef	_NOCOLOR
static int NEAR getcolorid __P_((namelist *));
static int NEAR getcolor __P_((int));
#endif
static VOID NEAR pathbar __P_((VOID_A));
static VOID NEAR statusbar __P_((VOID_A));
static VOID NEAR stackbar __P_((VOID_A));
static VOID NEAR cputbytes __P_((off_t, off_t, int));
static VOID NEAR sizebar __P_((VOID_A));
#ifndef	NOUID
static int NEAR putowner __P_((char *, uid_t));
static int NEAR putgroup __P_((char *, gid_t));
#endif
static int NEAR putsize __P_((char *, off_t, int, int));
static int NEAR putsize2 __P_((char *, namelist *, int));
static int NEAR putfilename __P_((char *, namelist *, int));
static VOID NEAR infobar __P_((VOID_A));
static int NEAR calclocate __P_((int));
#ifndef	_NOSPLITWIN
static int NEAR listupwin __P_((char *));
#endif
static int NEAR searchmove __P_((int, char *));
#ifndef	_NOPRECEDE
static VOID readstatus __P_((VOID_A));
#endif
static int NEAR readfilelist __P_((reg_t *, char *));
static int NEAR browsedir __P_((char *, char *));
static char *NEAR initcwd __P_((char *, char *));

int curcolumns = 0;
int defcolumns = 0;
int minfilename = 0;
int mark = 0;
int fnameofs = 0;
int displaymode = 0;
int sorttype = 0;
int chgorder = 0;
int stackdepth = 0;
int tradlayout = 0;
int sizeinfo = 0;
off_t marksize = (off_t)0;
off_t totalsize = (off_t)0;
off_t blocksize = (off_t)0;
namelist filestack[MAXSTACK];
char fullpath[MAXPATHLEN] = "";
char *macrolist[MAXMACROTABLE];
int maxmacro = 0;
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
#ifdef	HAVEFLAGS
char fflagsymlist[] = "ANacuacu";
u_long fflaglist[] = {
	SF_ARCHIVED, UF_NODUMP,
	SF_APPEND, SF_IMMUTABLE, SF_NOUNLINK,
	UF_APPEND, UF_IMMUTABLE, UF_NOUNLINK
};
#endif
winvartable winvar[MAXWINDOWS];
#ifndef	_NOCOLOR
int ansicolor = 0;
char *ansipalette = NULL;
#endif
#ifndef	_NOPRECEDE
char *precedepath = NULL;
#endif
#ifndef	_NOSPLITWIN
int windows = WINDOWS;
int win = 0;
#endif
int calc_x = -1;
int calc_y = -1;

static CONST u_short modelist[] = {
	S_IFDIR, S_IFLNK, S_IFSOCK, S_IFIFO, S_IFBLK, S_IFCHR
};
#define	MAXMODELIST	(sizeof(modelist) / sizeof(u_short))
static CONST char suffixlist[] = {
	'/', '@', '=', '|'
};
#define	MAXSUFFIXLIST	(sizeof(suffixlist) / sizeof(char))
#ifndef	_NOCOLOR
static CONST u_char colorlist[] = {
	CL_DIR, CL_LINK, CL_SOCK, CL_FIFO, CL_BLOCK, CL_CHAR
};
static CONST char defpalette[] = {
	ANSI_FG,	/* CL_NORM */
	ANSI_BG,	/* CL_BACK */
	ANSI_CYAN,	/* CL_DIR */
	ANSI_GREEN,	/* CL_RONLY */
	ANSI_BLUE,	/* CL_HIDDEN */
	ANSI_YELLOW,	/* CL_LINK */
	ANSI_MAGENTA,	/* CL_SOCK */
	ANSI_RED,	/* CL_FIFO */
	ANSI_FG,	/* CL_BLOCK */
	ANSI_FG,	/* CL_CHAR */
};
#endif
#ifndef	_NOPRECEDE
static int maxstat = 0;
static int haste = 0;
#endif
static int search_x = -1;
static int search_y = -1;


#ifndef	_NOCOLOR
static int NEAR getcolorid(namep)
namelist *namep;
{
	int i;

	if (!isread(namep)) return(CL_HIDDEN);
	if (!iswrite(namep)) return(CL_RONLY);
	for (i = 0; i < MAXMODELIST; i++)
		if ((namep -> st_mode & S_IFMT) == modelist[i])
			return(colorlist[i]);
	return(CL_NORM);
}

static int NEAR getcolor(id)
int id;
{
	int color;

	if (!ansipalette || id >= strlen(ansipalette)) color = defpalette[id];
	else {
		color = ansipalette[id];
		if (isdigit2(color)) color -= '0';
		else color = defpalette[id];
	}

	if (color == ANSI_FG)
		color = (ansicolor == 3) ? ANSI_BLACK : ANSI_WHITE;
	else if (color == ANSI_BG) color = (ansicolor == 2) ? ANSI_BLACK : -1;

	return(color);
}
#endif	/* !_NOCOLOR */

static VOID NEAR pathbar(VOID_A)
{
#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		locate(0, TL_PATH);
		putterm(l_clear);

		locate(TC_PATH, TL_PATH);
		putterm(t_standout);
		cputs2(TS_PATH);
		putterm(end_standout);
		cprintf2("%-*.*k", TD_PATH, TD_PATH, fullpath);

		locate(TC_MARK, TL_PATH);
		cprintf2("%<*d", TD_MARK, mark);
		putterm(t_standout);
		cputs2(TS_MARK);
		putterm(end_standout);
		cprintf2("%<'*qd", TD_SIZE, marksize);

		tflush();
		return;
	}
#endif	/* !_NOTRADLAYOUT */

	locate(0, L_PATH);
	putterm(l_clear);

	locate(C_PATH, L_PATH);
	putterm(t_standout);
	cputs2(S_PATH);
	putterm(end_standout);
	cprintf2("%-*.*k", D_PATH, D_PATH, fullpath);

	tflush();
}

VOID helpbar(VOID_A)
{
	int i, j, col, gap, width, len, ofs;

	if (ishardomit()) {
		col = n_column + 1;
		gap = 0;
	}
	else if (iswellomit()) {
		col = n_column + 1;
		gap = 1;
	}
	else if (isrightomit()) {
		col = n_column + 1;
		gap = 2;
	}
	else if (isleftshift()) {
		col = n_column + 1;
		gap = 3;
	}
	else {
		col = n_column;
		gap = 3;
	}
	width = (col - 4 - gap * 2) / 10 - 1;
	ofs = (col - (width + 1) * 10 - 2) / 2;
	if (ofs < 4) ofs = 4;

	locate(0, L_HELP);
	putterm(l_clear);
	putch2(isdisplnk(dispmode) ? 'S' : ' ');
	putch2(isdisptyp(dispmode) ? 'T' : ' ');
	putch2(ishidedot(dispmode) ? 'H' : ' ');
#ifdef	HAVEFLAGS
	putch2(isfileflg(dispmode) ? 'F' : ' ');
#endif

	for (i = 0; i < 10; i++) {
		locate(ofs + (width + 1) * i + (i / 5) * gap, L_HELP);
		len = (width - strlen2(helpindex[i])) / 2;
		if (len < 0) len = 0;
		putterm(t_standout);
		for (j = 0; j < len; j++) putch2(' ');
		cprintf2("%-*.*k", width - len, width - len, helpindex[i]);
		putterm(end_standout);
	}

	tflush();
}

static VOID NEAR statusbar(VOID_A)
{
	char *str[6];

#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {

		locate(0, TL_STATUS);
		putterm(l_clear);

		locate(TC_INFO, TL_STATUS);
		putterm(t_standout);
		cputs2(TS_INFO);
		putterm(end_standout);

		locate(TC_SIZE, TL_STATUS);
		cprintf2("%<*d", TD_MARK, maxfile);
		putterm(t_standout);
		cputs2(TS_SIZE);
		putterm(end_standout);
		cprintf2("%<'*qd", TD_SIZE, totalsize);

		return;
	}
#endif	/* !_NOTRADLAYOUT */

	locate(0, L_STATUS);
	putterm(l_clear);

	locate(C_PAGE, L_STATUS);
	putterm(t_standout);
	cputs2(S_PAGE);
	putterm(end_standout);
	cprintf2("%<*d/%<*d", D_PAGE, filepos / FILEPERPAGE + 1,
		D_PAGE, (maxfile - 1) / FILEPERPAGE + 1);

	locate(C_MARK, L_STATUS);
	putterm(t_standout);
	cputs2(S_MARK);
	putterm(end_standout);
	cprintf2("%<*d/%<*d", D_MARK, mark, D_MARK, maxfile);

	if (!ishardomit()) {
		locate(C_SORT, L_STATUS);
		putterm(t_standout);
		cputs2(S_SORT);
		putterm(end_standout);

#ifndef	_NOPRECEDE
		if (haste) kanjiputs(OMIT_K);
		else
#endif
		if (!(sorton & 7)) kanjiputs(ORAW_K + 3);
		else {
			str[0] = ONAME_K;
			str[1] = OEXT_K;
			str[2] = OSIZE_K;
			str[3] = ODATE_K;
			str[4] = OLEN_K;
			kanjiputs(&(str[(sorton & 7) - 1][3]));

			str[0] = OINC_K;
			str[1] = ODEC_K;
			cprintf2("(%k)", &(str[sorton / 8][3]));
		}
	}

	locate(C_FIND, L_STATUS);
	putterm(t_standout);
	cputs2(S_FIND);
	putterm(end_standout);
	if (findpattern) cprintf2("%-*.*k", D_FIND, D_FIND, findpattern);

	tflush();
}

static VOID NEAR stackbar(VOID_A)
{
#ifndef	_NOCOLOR
	int x, color, bgcolor;
#endif
	int i, width;

	width = n_column / MAXSTACK;

	locate(0, L_STACK);
#ifndef	_NOCOLOR
	if ((bgcolor = getcolor(CL_BACK)) >= 0) chgcolor(bgcolor, 1);
	x = 0;
#endif
	putterm(l_clear);

	for (i = 0; i < stackdepth; i++) {
#ifndef	_NOCOLOR
		if (ansicolor == 2) for (; x < width * i + 1; x++) putch2(' ');
		else
#endif
		locate(width * i + 1, L_STACK);
#ifndef	_NOCOLOR
		color = getcolor(getcolorid(&(filestack[i])));
		if (ansicolor && color >= 0) chgcolor(color, 1);
		else
#endif
		putterm(t_standout);
		cprintf2("%-*.*k", width - 2, width - 2, filestack[i].name);
#ifndef	_NOCOLOR
		x += width - 2;
		if (bgcolor >= 0) chgcolor(bgcolor, 1);
		else if (ansicolor) putterms(t_normal);
		else
#endif
		putterm(end_standout);
	}
#ifndef	_NOCOLOR
	if (bgcolor >= 0) {
		for (; x < n_column; x++) putch2(' ');
		putterm(t_normal);
	}
#endif

	tflush();
}

static VOID NEAR cputbytes(size, bsize, width)
off_t size, bsize;
int width;
{
	off_t kb;

	if (size < (off_t)0) cprintf2("%*s", width, "?");
	else if (size < (off_t)10000000 / bsize)
		cprintf2("%<'*qd%s", width - W_BYTES, size * bsize, S_BYTES);
	else if ((kb = calcKB(size, bsize)) < (off_t)1000000000)
		cprintf2("%<'*qd%s", width - W_KBYTES, kb, S_KBYTES);
	else cprintf2("%<'*qd%s",
		width - W_MBYTES, kb / (off_t)1024, S_MBYTES);
}

static VOID NEAR sizebar(VOID_A)
{
	off_t total, fre, bsize;

	if (!hassizeinfo() || !*fullpath) return;
	if (getinfofs(".", &total, &fre, &bsize) < 0) total = fre = (off_t)-1;

#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		locate(0, TL_SIZE);
		putterm(l_clear);

		locate(TC_PAGE, TL_SIZE);
		putterm(t_standout);
		cputs2(TS_PAGE);
		putterm(end_standout);
		cprintf2("%<*d/%<*d", TD_PAGE, filepos / FILEPERPAGE + 1,
			TD_PAGE, (maxfile - 1) / FILEPERPAGE + 1);

		locate(TC_TOTAL, TL_SIZE);
		putterm(t_standout);
		cputs2(TS_TOTAL);
		putterm(end_standout);
		cputbytes(total, bsize, TD_TOTAL);

		locate(TC_USED, TL_SIZE);
		putterm(t_standout);
		cputs2(TS_USED);
		putterm(end_standout);
		cputbytes(total - fre, bsize, TD_USED);

		locate(TC_FREE, TL_SIZE);
		putterm(t_standout);
		cputs2(TS_FREE);
		putterm(end_standout);
		cputbytes(fre, bsize, TD_FREE);

		return;
	}
#endif	/* !_NOTRADLAYOUT */

	locate(0, L_SIZE);
	putterm(l_clear);

	locate(C_SIZE, L_SIZE);
	putterm(t_standout);
	cputs2(S_SIZE);
	putterm(end_standout);
	cprintf2("%<'*qd/%<'*qd", D_SIZE, marksize, D_SIZE, totalsize);

	if (!ishardomit()) {
		locate(C_TOTAL, L_SIZE);
		putterm(t_standout);
		cputs2(S_TOTAL);
		putterm(end_standout);
		cputbytes(total, bsize, D_TOTAL);
	}

	if (!iswellomit()) {
		locate(C_FREE, L_SIZE);
		putterm(t_standout);
		cputs2(S_FREE);
		putterm(end_standout);
		cputbytes(fre, bsize, D_FREE);
	}

	tflush();
}

int putmode(buf, mode, notype)
char *buf;
u_int mode;
int notype;
{
	int i, j;

	i = 0;
	if (!notype) {
		for (j = 0; j < sizeof(typelist) / sizeof(u_short); j++)
			if ((mode & S_IFMT) == typelist[j]) break;
		buf[i++] = (j < sizeof(typelist) / sizeof(u_short))
			? typesymlist[j] : '-';
	}

	buf[i++] = (mode & S_IRUSR) ? 'r' : '-';
	buf[i++] = (mode & S_IWUSR) ? 'w' : '-';
#if	MSDOS
	buf[i++] = (mode & S_IXUSR) ? 'x' : '-';
	buf[i++] = (mode & S_ISVTX) ? 'a' : '-';
#else
	buf[i++] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S')
		: ((mode & S_IXUSR) ? 'x' : '-');
	buf[i++] = (mode & S_IRGRP) ? 'r' : '-';
	buf[i++] = (mode & S_IWGRP) ? 'w' : '-';
	buf[i++] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S')
		: ((mode & S_IXGRP) ? 'x' : '-');
	buf[i++] = (mode & S_IROTH) ? 'r' : '-';
	buf[i++] = (mode & S_IWOTH) ? 'w' : '-';
	buf[i++] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T')
		: ((mode & S_IXOTH) ? 'x' : '-');
#endif
	buf[i] = '\0';

	return(i);
}

#ifdef	HAVEFLAGS
int putflags(buf, flags)
char *buf;
u_long flags;
{
	int i;

	for (i = 0; i < sizeof(fflaglist) / sizeof(u_long); i++)
		buf[i] = (flags & fflaglist[i]) ? fflagsymlist[i] : '-';
	buf[i] = '\0';

	return(i);
}
#endif

#ifndef	NOUID
static int NEAR putowner(buf, uid)
char *buf;
uid_t uid;
{
	uidtable *up;
	int i, len;

	i = len = (iswellomit()) ? WOWNERMIN : WOWNER;
	if (uid == (uid_t)-1) while (--i >= 0) buf[i] = '?';
	else if ((up = finduid(uid, NULL))) strncpy3(buf, up -> name, &len, 0);
	else snprintf2(buf, len + 1, "%-*d", len, (int)uid);

	return(len);
}

static int NEAR putgroup(buf, gid)
char *buf;
gid_t gid;
{
	gidtable *gp;
	int i, len;

	i = len = (iswellomit()) ? WGROUPMIN : WGROUP;
	if (gid == (gid_t)-1) while (--i >= 0) buf[i] = '?';
	else if ((gp = findgid(gid, NULL))) strncpy3(buf, gp -> name, &len, 0);
	else snprintf2(buf, len + 1, "%-*d", len, (int)gid);

	return(len);
}
#endif	/* !NOUID */

static int NEAR putsize(buf, n, width, max)
char *buf;
off_t n;
int width, max;
{
	char tmp[MAXLONGWIDTH + 1];
	int i, len;

	if (max > MAXLONGWIDTH) max = MAXLONGWIDTH;
	snprintf2(tmp, sizeof(tmp), "%<*qd", max, n);
	for (i = max - width; i > 0; i--) if (tmp[i - 1] == ' ') break;
	len = max - i;
	strncpy(buf, &(tmp[i]), len);
	return(len);
}

static int NEAR putsize2(buf, namep, width)
char *buf;
namelist *namep;
int width;
{
	if (isdir(namep))
		snprintf2(buf, width + 1, "%*.*s", width, width, "<DIR>");
#if	MSDOS
	else if ((namep -> st_mode & S_IFMT) == S_IFIFO)
		snprintf2(buf, width + 1, "%*.*s", width, width, "<VOL>");
#else	/* !MSDOS */
# ifndef	_NODOSDRIVE
	else if (dospath2("")
	&& (namep -> st_mode & S_IFMT) == S_IFIFO)
		snprintf2(buf, width + 1, "%*.*s", width, width, "<VOL>");
# endif
	else if (isdev(namep))
		snprintf2(buf, width + 1, "%<*lu,%<*lu",
			width / 2,
			(u_long)major((u_long)(namep -> st_size)),
			width - (width / 2) - 1,
			(u_long)minor((u_long)(namep -> st_size)));
#endif	/* !MSDOS */
	else snprintf2(buf, width + 1, "%<*qd", width, namep -> st_size);

	return(width);
}

static int NEAR putfilename(buf, namep, width)
char *buf;
namelist *namep;
int width;
{
#ifdef	NOSYMLINK
	strncpy3(buf, namep -> name, &width, fnameofs);
#else	/* !NOSYMLINK */
# ifndef	_NODOSDRIVE
	char path[MAXPATHLEN];
# endif
	char *tmp;
	int i, w, len;

	i = strncpy3(buf, namep -> name, &width, fnameofs);
	if (!islink(namep)) return(width);

	len = fnameofs - strlen3(namep -> name);
	if (--len < 0) i++;
	if (--len < 0 && i < width) buf[i++] = '-';
	if (--len < 0 && i < width) buf[i++] = '>';
	if (--len < 0) {
		i++;
		len = 0;
	}
	buf += i;

	if ((w = width - i) <= 0) /*EMPTY*/;
# ifndef	_NOARCHIVE
	else if (archivefile) {
		if (namep -> linkname)
			strncpy3(buf, &(namep -> linkname[len]), &w, 0);
	}
# endif
	else {
		tmp = malloc2(width * 2 + len + 1);
		i = Xreadlink(nodospath(path, namep -> name),
			tmp, width * 2 + len);
		if (i >= 0) {
			tmp[i] = '\0';
			strncpy3(buf, &(tmp[len]), &w, 0);
		}
		free(tmp);
	}
#endif	/* !NOSYMLINK */
	return(width);
}

static VOID NEAR infobar(VOID_A)
{
	char *buf;
	struct tm *tm;
	int len;

	if (!filelist || maxfile < 0) return;

#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		locate(TC_INFO + TW_INFO, TL_STATUS);
		cprintf2("%*s", TD_INFO, "");

		locate(TC_INFO + TW_INFO, TL_STATUS);
		if (filepos >= maxfile) {
			if (filelist[0].st_nlink < 0 && filelist[0].name)
				kanjiputs(filelist[0].name);
			tflush();
			return;
		}
		len = TD_INFO
			- (1 + TWSIZE2 + 1 + WDATE + 1 + WTIME + 1 + TWMODE);
# ifndef	_NOPRECEDE
		if (!havestat(&(filelist[filepos]))) {
			kanjiputs2(filelist[filepos].name, len, fnameofs);
			tflush();
			return;
		}
# endif

		buf = malloc2(TD_INFO * KANAWID + 1);
		tm = localtime(&(filelist[filepos].st_mtim));

		len = putfilename(buf, &(filelist[filepos]), len);
		buf[len++] = ' ';

		len += putsize2(&(buf[len]), &(filelist[filepos]), TWSIZE2);
		buf[len++] = ' ';

		snprintf2(&(buf[len]), WDATE + 1 + WTIME + 1 + 1,
			"%02d-%02d-%02d %2d:%02d ",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min);
		len += WDATE + 1 + WTIME + 1;

# ifdef	HAVEFLAGS
		if (isfileflg(dispmode)) {
			len += putflags(&(buf[len]),
				filelist[filepos].st_flags);
			while (len < TWMODE) buf[len++] = ' ';
		}
		else
# endif
		len += putmode(&(buf[len]),
			(!isdisplnk(dispmode) && islink(&(filelist[filepos])))
			? (S_IFLNK | 0777) : filelist[filepos].st_mode, 1);

		kanjiputs(buf);
		free(buf);
		tflush();
		return;
	}
#endif	/* !_NOTRADLAYOUT */

	locate(0, L_INFO);

	if (filepos >= maxfile) {
		putterm(l_clear);
		if (filelist[0].st_nlink < 0 && filelist[0].name)
			kanjiputs(filelist[0].name);
		tflush();
		return;
	}
#ifndef	_NOPRECEDE
	if (!havestat(&(filelist[filepos]))) {
		putterm(l_clear);
		len = WMODE + WSIZE2 + 1 + WDATE + 1 + WTIME + 1;
		if (!ishardomit()) {
			len += 1 + WNLINK + 1;
# ifndef	NOUID
			len += (iswellomit())
				? WOWNERMIN + 1 + WGROUPMIN + 1
				: WOWNER + 1 + WGROUP + 1;
# endif
		}
		locate(len, L_INFO);
		kanjiputs2(filelist[filepos].name,
			n_lastcolumn - len, fnameofs);
		tflush();
		return;
	}
#endif

	buf = malloc2(n_lastcolumn * KANAWID + 1);
	tm = localtime(&(filelist[filepos].st_mtim));

	if (isbestomit()) len = 0;
#ifdef	HAVEFLAGS
	else if (isfileflg(dispmode)) {
		len = putflags(buf, filelist[filepos].st_flags);
		while (len < WMODE) buf[len++] = ' ';
	}
#endif
	else len = putmode(buf,
		(!isdisplnk(dispmode) && islink(&(filelist[filepos])))
		? (S_IFLNK | 0777) : filelist[filepos].st_mode, 0);

	if (!ishardomit()) {
		snprintf2(&(buf[len]), 1 + WNLINK + 1 + 1, " %<*d ",
			WNLINK, (int)(filelist[filepos].st_nlink));
		len += 1 + WNLINK + 1;

#ifndef	NOUID
		len += putowner(&(buf[len]), filelist[filepos].st_uid);
		buf[len++] = ' ';
		len += putgroup(&(buf[len]), filelist[filepos].st_gid);
		buf[len++] = ' ';
#endif
	}

#if	!MSDOS
	if (isdev(&(filelist[filepos]))) {
		len += putsize(&(buf[len]),
			(off_t)major((u_long)(filelist[filepos].st_size)),
			3, n_lastcolumn - len);
		buf[len++] = ',';
		buf[len++] = ' ';
		len += putsize(&(buf[len]),
			(off_t)minor((u_long)(filelist[filepos].st_size)),
			3, n_lastcolumn - len);
	}
	else
#endif
	len += putsize(&(buf[len]),
		filelist[filepos].st_size, WSIZE2, n_lastcolumn - len);
	buf[len++] = ' ';

	snprintf2(&(buf[len]), WDATE + 1 + WTIME + 1 + 1,
		"%02d-%02d-%02d %02d:%02d ",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min);
	len += WDATE + 1 + WTIME + 1;

	putfilename(&(buf[len]), &(filelist[filepos]), n_lastcolumn - len);
	kanjiputs(buf);
	free(buf);
	tflush();
}

VOID waitmes(VOID_A)
{
	helpbar();
	locate(0, L_MESLINE);
	putterm(l_clear);
	kanjiputs(WAIT_K);
	if (win_x >= 0 && win_y >= 0) locate(win_x, win_y);
	tflush();
}

static int NEAR calclocate(i)
int i;
{
#ifndef	_NOCOLOR
	int bgcolor;
#endif
	int col, width;

	col = n_column;
	if (ispureshift()) col++;
	width = col / FILEPERLINE;
	i %= FILEPERPAGE;
	calc_x = (i / FILEPERROW) * width;
	calc_y = i % FILEPERROW + LFILETOP;
#ifndef	_NOCOLOR
	if ((bgcolor = getcolor(CL_BACK)) >= 0) {
		chgcolor(bgcolor, 1);
		locate(calc_x, calc_y);
		if (!isleftshift()) {
			putch2(' ');
			calc_x++;
		}
	}
	else
#endif
	{
		if (!isleftshift()) calc_x++;
		locate(calc_x, calc_y);
	}
	return(width);
}

#ifdef	NOUID
#define	WIDTH1	(WMODE + 1 + WSECOND + 1)
#else
#define	WIDTH1	(iswellomit() \
		? (WOWNERMIN + 1 + WGROUPMIN + 1 + WMODE + 1 + WSECOND + 1) \
		: (WOWNER + 1 + WGROUP + 1 + WMODE + 1 + WSECOND + 1))
#endif
#define	WIDTH2	(WTIME + 1 + WDATE + 1)
#define	WIDTH3	(WSIZE + 1)

int calcwidth(VOID_A)
{
	int col, w1, width;

	col = n_column;
	if (ispureshift()) col++;
	width = (col / FILEPERLINE) - 2 - 1 + ((isshortwid()) ? 1 : 0);
	w1 = WIDTH1;
	if (curcolumns < 2 && width - (w1 + WIDTH2 + WIDTH3) >= minfilename)
		width -= w1 + WIDTH2 + WIDTH3;
	else if (curcolumns < 3 && width - (WIDTH2 + WIDTH3) >= minfilename)
		width -= WIDTH2 + WIDTH3;
	else if (curcolumns < 4 && width - WIDTH3 >= minfilename)
		width -= WIDTH3;
	return(width);
}

int putname(list, no, isstandout)
namelist *list;
int no, isstandout;
{
	char *buf;
	struct tm *tm;
	int i, j, col, len, wid, width;
#ifndef	_NOCOLOR
	int color, bgcolor;
#endif

	col = calclocate(no) - 2 - 1 + ((isshortwid()) ? 1 : 0);
	width = calcwidth();
	putch2(ismark(&(list[no])) ? '*' : ' ');

	if (list != filelist) {
		len = strlen3(list[no].name);
		if (col > len) col = len;
		if (width > len) width = len;
	}

	if (isstandout < 0 && stable_standout) {
		putterm(end_standout);
		calclocate(no);
#ifndef	_NOPRECEDE
		if (!havestat(&(list[no]))) return(width);
#endif
		return(col);
	}

	buf = malloc2(col * KANAWID + 1);
	i = (isstandout && fnameofs > 0) ? fnameofs : 0;
	wid = width;
	i = strncpy3(buf, list[no].name, &width, i);

#ifndef	_NOPRECEDE
	if (!havestat(&(list[no]))) {
		if (isstandout > 0) putterm(t_standout);
		kanjiputs(buf);
		free(buf);
		if (isstandout > 0) putterm(end_standout);
		return(wid);
	}
#endif

	if (isdisptyp(dispmode) && i < width) {
		for (j = 0; j < MAXMODELIST; j++)
			if ((list[no].st_mode & S_IFMT) == modelist[j]) break;
		if (j < MAXSUFFIXLIST) buf[i] = suffixlist[j];
		else if ((list[no].st_mode & S_IFMT) == S_IFREG
		&& (list[no].st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
			buf[i] = '*';
	}
#ifndef	_NOCOLOR
	color = getcolor(getcolorid(&(list[no])));
#endif
	len = width;
	width += col - wid;

	tm = NULL;
	if (curcolumns < 5 && len + WIDTH3 <= width) {
		buf[len++] = ' ';
		len += putsize2(&(buf[len]), &(list[no]), WSIZE);
	}
	if (curcolumns < 3 && len + WIDTH2 <= width) {
		tm = localtime(&(list[no].st_mtim));
		snprintf2(&(buf[len]), WIDTH2 + 1, " %02d-%02d-%02d %2d:%02d",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min);
		len += WIDTH2;
	}
	if (curcolumns < 2 && len + WIDTH1 <= width) {
		if (!tm) tm = localtime(&(list[no].st_mtim));
		snprintf2(&(buf[len]), 1 + WSECOND + 1, ":%02d", tm -> tm_sec);
		len += 1 + WSECOND;
		buf[len++] = ' ';
#ifndef	NOUID
		len += putowner(&(buf[len]), list[no].st_uid);
		buf[len++] = ' ';
		len += putgroup(&(buf[len]), list[no].st_gid);
		buf[len++] = ' ';
#endif
#ifdef	HAVEFLAGS
		if (isfileflg(dispmode)) {
			i = putflags(&(buf[len]), list[no].st_flags);
			while (i < WMODE) buf[len + i++] = ' ';
			len += i;
		}
		else
#endif
		len += putmode(&(buf[len]),
			(!isdisplnk(dispmode) && islink(&(list[no])))
			? (S_IFLNK | 0777) : list[no].st_mode, 0);
	}
	while (len < width) buf[len++] = ' ';
	buf[len] = '\0';

#ifndef	_NOCOLOR
	if (ansicolor && color >= 0) chgcolor(color, isstandout > 0);
	else
#endif
	if (isstandout > 0) putterm(t_standout);
	kanjiputs(buf);
	free(buf);
#ifndef	_NOCOLOR
	if ((bgcolor = getcolor(CL_BACK)) >= 0) {
		chgcolor(bgcolor, 1);
		putch2(' ');
	}
	if (ansicolor) putterms(t_normal);
	else
#endif
	if (isstandout > 0) putterm(end_standout);
	return(col);
}

int listupfile(list, max, def)
namelist *list;
int max;
char *def;
{
	char *cp;
	int i, count, start, ret;

	for (i = 0; i < FILEPERROW; i++) {
		locate(0, i + LFILETOP);
		putterm(l_clear);
	}

	if (max <= 0) {
		i = (n_column / FILEPERLINE) - 2 - 1;
		locate(1, LFILETOP);
		putch2(' ');
		if (def) putterm(t_standout);
		cp = NOFIL_K;
		if (i <= strlen2(cp)) cp = "NoFiles";
		cprintf2("%-*.*k", i, i, cp);
		if (def) putterm(end_standout);
		win_x = i + 2;
		win_y = LFILETOP;
		return(0);
	}

	ret = -1;
	if (def && !(*def)) def = "..";

	start = 0;
	for (i = count = 0; i < max; i++, count++) {
		if (count >= FILEPERPAGE) {
			count = 0;
			start = i;
		}
		if (def && !strpathcmp(def, list[i].name)) {
			ret = i;
			break;
		}
	}
	if (ret < 0) {
		start = 0;
		ret = (max <= 1 || strcmp(list[1].name, "..")) ? 0 : 1;
	}

	if (list == filelist) {
#ifndef	_NOTRADLAYOUT
		if (istradlayout()) {
			locate(TC_PAGE + TW_PAGE, TL_SIZE);
			cprintf2("%<*d", TD_PAGE, start / FILEPERPAGE + 1);
		}
		else
#endif
		{
			locate(C_PAGE + W_PAGE, L_STATUS);
			cprintf2("%<*d", D_PAGE, start / FILEPERPAGE + 1);
		}
	}

	for (i = start, count = 0; i < max; i++, count++) {
		if (count >= FILEPERPAGE) break;
		if (i != ret) putname(list, i, 0);
		else {
			win_x = putname(list, i, 1) + 1;
			win_x += calc_x;
			win_y = calc_y;
		}
	}

	return(ret);
}

#ifndef	_NOSPLITWIN
static int NEAR listupwin(def)
char *def;
{
	int i, x, y, n, duplwin;

	duplwin = win;
	for (n = 1; n < windows; n++) {
		locate(0, WHEADER - 1 + (n * (FILEPERROW + 1)));
		putterm(l_clear);
		putch2(' ');
		for (i = 2; i < n_column; i++) putch2('-');
	}
	for (n = WHEADER - 1 + (n * (FILEPERROW + 1)); n < L_STACK; n++) {
		locate(0, n);
		putterm(l_clear);
	}
#ifdef	FAKEUNINIT
	x = y = -1;	/* fake for -Wuninitialized */
#endif
	n = -1;
	for (win = 0; win < windows; win++) {
		if (!filelist) continue;
		if (win == duplwin) {
			n = listupfile(filelist, maxfile, def);
			x = win_x;
			y = win_y;
		}
		else if (maxfile <= 0) listupfile(filelist, maxfile, NULL);
		else {
			listupfile(filelist, maxfile, filelist[filepos].name);
			putname(filelist, filepos, -1);
		}
	}
	win = duplwin;

	win_x = x;
	win_y = y;
	return(n);
}

int shutwin(n)
int n;
{
	int i, duplwin;

	duplwin = win;
	win = n;
#ifndef	_NOARCHIVE
	while (archivefile) {
# ifdef	_NOBROWSE
		escapearch();
# else
		do {
			escapearch();
		} while (browselist);
# endif
	}
	if (winvar[win].v_archivedir) {
		free(winvar[win].v_archivedir);
		winvar[win].v_archivedir = NULL;
	}
#endif
	if (winvar[win].v_fullpath) {
		free(winvar[win].v_fullpath);
		winvar[win].v_fullpath = NULL;
	}
	if (filelist) {
		for (i = 0; i < maxfile; i++)
			if (filelist[i].name) free(filelist[i].name);
		free(filelist);
		filelist = NULL;
	}
	maxfile = maxent = filepos = sorton = dispmode = 0;
	if (findpattern) {
		free(findpattern);
		findpattern = NULL;
	}
	win = duplwin;
	return(n);
}
#endif	/* !_NOSPLITWIN */

VOID movepos(old, funcstat)
int old, funcstat;
{
#ifndef	_NOSPLITWIN
	if ((funcstat & REWRITEMODE) >= REWIN) {
		filepos = listupwin(filelist[filepos].name);
		keyflush();
	}
	else
#endif
	if (((funcstat & REWRITEMODE) >= RELIST)
	|| old / FILEPERPAGE != filepos / FILEPERPAGE) {
		filepos =
			listupfile(filelist, maxfile, filelist[filepos].name);
		keyflush();
	}
	else if (((funcstat & REWRITEMODE) >= REWRITE) || old != filepos) {
		if (old != filepos) putname(filelist, old, -1);
		win_x = putname(filelist, filepos, 1) + 1;
		win_x += calc_x;
		win_y = calc_y;
	}
	infobar();
}

VOID rewritefile(all)
int all;
{
	if (!filelist || maxfile < 0) return;
	if (all > 0) {
		title();
		helpbar();
		infobar();
	}
	sizebar();
	statusbar();
	stackbar();
#ifndef	_NOARCHIVE
	if (archivefile) archbar(archivefile, archivedir);
	else
#endif
	pathbar();
	if (all >= 0) {
#ifndef	_NOTREE
		if (treepath) rewritetree();
		else
#endif
#ifndef	_NOCUSTOMIZE
		if (custno >= 0) rewritecust(all);
		else
#endif
		if (filelist && filepos < maxfile) {
#ifdef	_NOSPLITWIN
			listupfile(filelist, maxfile, filelist[filepos].name);
#else
			listupwin(filelist[filepos].name);
#endif
		}
	}
	locate(win_x, win_y);
	tflush();
}

static int NEAR searchmove(ch, buf)
int ch;
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
		if (!len) isearch = 0;
		else buf[--len] = '\0';
	}
	else if (len && bindlist[i].f_func == SEARCH_FORW) {
		if (n > 2) pos++;
		else if (n > 1) pos = 0;
		s = 1;
	}
	else if (len && bindlist[i].f_func == SEARCH_BACK) {
		if (n > 2) pos--;
		else if (n > 1) pos = maxfile - 1;
		s = -1;
	}
	else {
		if (n == 1) buf[len = 0] = '\0';
		if (iscntrl2(ch) || ch >= K_MIN) isearch = 0;
		else if (len < MAXNAMLEN - 1) {
			buf[len++] = ch;
			buf[len] = '\0';
		}
	}

	if (!isearch) {
		helpbar();
		search_x = search_y = -1;
		win_x = calc_x;
		win_y = calc_y;
		if (ch == K_CR) return(-1);
		else if (ch != K_ESC) putterm(t_bell);
		return(0);
	}

	locate(0, L_HELP);
	str[0] = SEAF_K;
	str[1] = SEAFF_K;
	str[2] = SEAB_K;
	str[3] = SEABF_K;

	i = 0;
	if (pos >= 0 && pos < maxfile) for (;;) {
		if (!strnpathcmp2(filelist[pos].name, buf, len)) {
			i = 1;
			break;
		}
		pos += s;
		if (pos < 0 || pos >= maxfile) break;
	}

	putterm(t_standout);
	search_x = len;
	len = kanjiputs(str[2 - s - i]);
	putterm(end_standout);
	cprintf2("%-*.*k", n_column - len - 1, n_column - len - 1, buf);
	if ((search_x += len) >= n_column) search_x = n_column - 1;
	search_y = L_HELP;
	if (i) filepos = pos;
	else if (n != 2 && ch != K_BS) putterm(t_bell);
	isearch = s * (i + 2);
	return(i);
}

VOID addlist(VOID_A)
{
	if (maxfile < maxent) return;
	maxent = ((maxfile + 1) / BUFUNIT + 1) * BUFUNIT;
	filelist = (namelist *)realloc2(filelist, maxent * sizeof(namelist));
}

#ifndef	_NOPRECEDE
static VOID readstatus(VOID_A)
{
	int i;

	for (i = maxstat; i < maxfile; i++)
		if (!havestat(&(filelist[i]))) break;
	if (i >= maxfile) return;
	maxstat = i + 1;
	if (getstatus(&(filelist[i])) < 0) return;
	if (keywaitfunc != readstatus) return;
	if (isfile(&(filelist[i])) && filelist[i].st_size) sizebar();
	if (i == filepos) {
		win_x = putname(filelist, i, 1) + 1;
		win_x += calc_x;
		win_y = calc_y;
		infobar();
	}
	else if (i / FILEPERPAGE == filepos / FILEPERPAGE)
		putname(filelist, i, 0);
	locate(win_x, win_y);
	tflush();
}
#endif

static int NEAR readfilelist(re, arcre)
reg_t *re;
char *arcre;
{
#ifndef	_NOPRECEDE
	char cwd[MAXPATHLEN];
#endif
	DIR *dirp;
	struct dirent *dp;
	char buf[MAXNAMLEN + 1];
	int i, n;

	n = 0;
#ifndef	_NOPRECEDE
	if (Xgetwd(cwd) && includepath(cwd, precedepath)) {
		n = 1;
		sorton = 0;
	}
#endif

	if (!(dirp = Xopendir("."))) {
		lostcwd(NULL);
		if (!(dirp = Xopendir("."))) error(".");
	}

	while ((dp = searchdir(dirp, re, arcre))) {
		if (ishidedot(dispmode) && *(dp -> d_name) == '.'
		&& !isdotdir(dp -> d_name)) continue;
		strcpy(buf, dp -> d_name);
		for (i = 0; i < stackdepth; i++)
			if (!strcmp(buf, filestack[i].name)) break;
		if (i < stackdepth) continue;

		addlist();
		filelist[maxfile].name = strdup2(buf);
		filelist[maxfile].ent = maxfile;
		filelist[maxfile].tmpflags = 0;
#ifndef	_NOPRECEDE
		if (n) {
			if (isdotdir(dp -> d_name)) {
				filelist[maxfile].flags = F_ISDIR;
				filelist[maxfile].st_mode = S_IFDIR;
			}
		}
		else
#endif
		if (getstatus(&(filelist[maxfile])) < 0) {
			free(filelist[maxfile].name);
			continue;
		}

		maxfile++;
	}

	Xclosedir(dirp);
	return(n);
}

static int NEAR browsedir(file, def)
char *file, *def;
{
#ifndef	_NOPRECEDE
	int dupsorton;
#endif
	reg_t *re;
	char *cp, *arcre, buf[MAXNAMLEN + 1];
	int ch, i, no, old, funcstat;

#ifndef	_NOPRECEDE
	haste = 0;
	dupsorton = sorton;
#endif
#ifndef	_NOCOLOR
	if (ansicolor) putterms(t_normal);
#endif

	mark = 0;
	totalsize = marksize = (off_t)0;
	chgorder = 0;

	fnameofs = 0;
	win_x = win_y = 0;
	waitmes();

	re = NULL;
	arcre = NULL;
	if (findpattern) {
#ifndef	_NOARCHIVE
		if (*findpattern == '/') arcre = findpattern + 1;
		else
#endif
		re = regexp_init(findpattern, -1);
	}

#ifndef	_NOARCHIVE
	if (archivefile) {
		maxfile = (*archivedir) ? 1 : 0;
		blocksize = (off_t)1;
		if (sorttype < 100) sorton = 0;
		copyarcf(re, arcre);
		def = (*file) ? file : "";
	}
	else
#endif
	{
		maxfile = 0;
		blocksize = getblocksize(".");
		if (sorttype < 100) sorton = sorttype;
		if (readfilelist(re, arcre)) {
#ifndef	_NOPRECEDE
			haste = 1;
			sorton = 0;
#endif
		}
	}
	regexp_free(re);

	if (!maxfile) {
		addlist();
		filelist[0].name = NOFIL_K;
		filelist[0].st_nlink = -1;
	}

#ifndef	_NOPRECEDE
	maxstat = (haste) ? 0 : maxfile;
#endif
	if (sorton) qsort(filelist, maxfile, sizeof(namelist), cmplist);

	for (i = 0; i < maxfile; i++) {
		if (!isfile(&(filelist[i])) || !filelist[i].st_size) continue;
		totalsize += getblock(filelist[i].st_size);
		if (ismark(&(filelist[i])))
			marksize += getblock(filelist[i].st_size);
	}

	putterms(t_clear);
	title();
	helpbar();
	rewritefile(-1);
#ifdef	_NOSPLITWIN
	old = filepos = listupfile(filelist, maxfile, def);
#else
	old = filepos = listupwin(def);
#endif
	funcstat = 0;
	no = 1;

	keyflush();
	buf[0] = '\0';
	for (;;) {
		if (no) movepos(old, funcstat);
		if (search_x >= 0 && search_y >= 0) {
			win_x = search_x;
			win_y = search_y;
		}
		locate(win_x, win_y);
		tflush();
#ifndef	_NOPRECEDE
		if (haste) keywaitfunc = readstatus;
#endif
		Xgetkey(-1, 0);
		ch = Xgetkey(1, 0);
		Xgetkey(-1, 0);
#ifndef	_NOPRECEDE
		keywaitfunc = NULL;
#endif

		old = filepos;
		if (isearch) {
			no = searchmove(ch, buf);
			if (no >= 0) {
				fnameofs = 0;
				continue;
			}
		}
		for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			if (ch == (int)(bindlist[i].key)) break;
#ifndef	_NOPRECEDE
		if (haste && !havestat(&(filelist[filepos]))
		&& (bindlist[i].d_func < 255
		|| (funclist[bindlist[i].f_func].status & NEEDSTAT))
		&& getstatus(&(filelist[filepos])) < 0)
			no = WARNING_BELL;
		else
#endif
		no = (bindlist[i].d_func < 255 && isdir(&(filelist[filepos])))
			? (int)(bindlist[i].d_func)
			: (int)(bindlist[i].f_func);
		if (no < FUNCLISTSIZ) funcstat = funclist[no].status;
#ifndef	_NOARCHIVE
		else if (archivefile) continue;
#endif
		else funcstat = (KILLSTK | RESCRN | REWIN);

		if (funcstat & KILLSTK) {
			if (stackdepth > 0) {
				chgorder = 0;
				if (!yesno(KILOK_K)) continue;
				for (i = maxfile - 1; i > filepos; i--)
					memcpy((char *)&(filelist[i
						+ stackdepth]),
						(char *)&(filelist[i]),
						sizeof(namelist));
				for (i = 0; i < stackdepth; i++)
					memcpy((char *)&(filelist[i
						+ filepos + 1]),
						(char *)&(filestack[i]),
						sizeof(namelist));
				maxfile += stackdepth;
				stackdepth = 0;
				filepos = listupfile(filelist, maxfile,
					filelist[filepos].name);
				stackbar();
			}
#ifndef	_NOARCHIVE
			else if (archivefile) /*EMPTY*/;
#endif
#ifndef	_NOWRITEFS
			else if (chgorder && writefs < 1 && no != WRITE_DIR
			&& !fd_restricted
			&& (i = writablefs(".")) > 0 && underhome(NULL) > 0) {
				chgorder = 0;
				if (yesno(WRTOK_K)) arrangedir(i);
			}
#endif
		}

		curfilename = filelist[filepos].name;
		if ((maxfile <= 0 && !(funcstat & NO_FILE))
#ifndef	_NOARCHIVE
		|| (archivefile && !(funcstat & ARCH))
#endif
		) no = 0;
		else if (no < FUNCLISTSIZ) {
			if (!fd_restricted || !(funcstat & RESTRICT))
				no = (*funclist[no].func)(NULL);
			else {
				warning(0, RESTR_K);
				no = 0;
			}
		}
		else {
			no = execusercomm(macrolist[no - FUNCLISTSIZ],
				filelist[filepos].name, 0, 0, 0);
			no = (no < 0) ? 1 :
				((internal_status >= -1) ? internal_status: 4);
		}
#ifndef	_NOPRECEDE
		if (sorton) haste = 0;
#endif

		if (no < 0 || no >= 4) break;
		if (no == 1 || no == 3) helpbar();
		if (no < 2) {
			funcstat = 0;
			continue;
		}

		if ((funcstat & REWRITEMODE) != REWRITE || old != filepos)
			fnameofs = 0;
		if (funcstat & RESCRN) {
			title();
			helpbar();
			rewritefile(-1);
		}
	}

#ifndef	_NOARCHIVE
	if (archivefile) {
		if (no < 0) {
# ifdef	_NOBROWSE
			escapearch();
# else
			do {
				escapearch();
			} while (browselist);
# endif
			strcpy(file, filelist[filepos].name);
		}
		else if (no > 4) {
			char *tmp;

			tmp = (filepos < 0) ? ".." : filelist[filepos].name;
			if (!(cp = archchdir(tmp))) {
				warning(-1, tmp);
				strcpy(file, tmp);
			}
			else if (cp != (char *)-1) strcpy(file, cp);
			else {
				escapearch();
				strcpy(file, filelist[filepos].name);
			}
		}
		else if (no == 4) {
			if (filepos < 0) *file = '\0';
			else strcpy(file, filelist[filepos].name);
		}
		no = 0;
	}
	else
#endif	/* !_NOARCHIVE */
	if (no >= 4) {
		no -= 4;
		strcpy(file, (maxfile) ? filelist[filepos].name : ".");
	}
#ifndef	_NOARCHIVE
	if (!archivefile)
#endif
	for (i = 0; i < maxfile; i++) {
		free(filelist[i].name);
		filelist[i].name = NULL;
	}
	maxfile = filepos = 0;

#ifndef	_NOPRECEDE
	if (haste && !sorton) sorton = dupsorton;
#endif
	return(no);
}

static char *NEAR initcwd(path, buf)
char *path, *buf;
{
	char *cp, *file;
	int i;

	if (!path) return(NULL);

	cp = evalpath(strdup2(path), 0);
#if	MSDOS
	if (_dospath(cp)) {
		if (setcurdrv(*cp, 0) >= 0 && !Xgetwd(fullpath))
			error("getwd()");
		for (i = 0; cp[i + 2]; i++) cp[i] = cp[i + 2];
		cp[i] = '\0';
	}
#endif	/* !MSDOS */

	if (chdir2(cp) >= 0) file = NULL;
	else {
		file = strrdelim(cp, 0);
#ifdef	_USEDOSEMU
		if (!file && dospath2(cp)) file = &(cp[2]);
#endif

		if (!file) file = cp;
		else {
			i = *file;
			*file = '\0';
			if (file == cp) {
				if (chdir2(_SS_) < 0) error(_SS_);
			}
			else if (chdir2(cp) < 0) {
				hideclock = 1;
				warning(-1, cp);
#if	MSDOS
				strcpy(fullpath, origpath);
#endif
				free(cp);
				return(NULL);
			}
			if (i == _SC_) file++;
			else *file = i;
		}
		strcpy(buf, file);
		file = buf;
	}

	free(cp);
	return(file);
}

VOID main_fd(pathlist)
char **pathlist;
{
	char file[MAXNAMLEN + 1], prev[MAXNAMLEN + 1];
	char *def, *cwd;
	int i, n, argc, ischgdir;

	if (!pathlist) argc = 0;
	else for (argc = 0; argc < MAXINVOKEARGS; argc++)
		if (!pathlist[argc]) break;

	cwd = getwd2();
	def = NULL;
	for (i = MAXWINDOWS - 1; i >= 0; i--) {
#ifndef	_NOSPLITWIN
		win = i;
		winvar[i].v_fullpath = NULL;
# ifndef	_NOARCHIVE
		winvar[i].v_archivedir = NULL;
# endif
#endif	/* !_NOSPLITWIN */

#ifndef	_NOARCHIVE
		archduplp = NULL;
		archivefile = NULL;
		archtmpdir = NULL;
		launchp = NULL;
		arcflist = NULL;
		maxarcf = 0;
# ifndef	_NODOSDRIVE
		archdrive = 0;
# endif
# ifndef	_NOBROWSE
		browselist = NULL;
		browselevel = 0;
# endif
#endif	/* !_NOARCHIVE */

#ifndef	_NOTREE
		treepath = NULL;
#endif
		findpattern = NULL;
		filelist = NULL;
		maxfile = maxent = filepos = 0;
		sorton = sorttype % 100;
		dispmode = displaymode;
		curcolumns = defcolumns;

		if (i >= argc) continue;

		chdir2(cwd);
		def = initcwd(pathlist[i], prev);

#ifndef	_NOSPLITWIN
		winvar[i].v_fullpath = strdup2(fullpath);
		if (!i) continue;

		readfilelist(NULL, NULL);
		if (sorton)
			qsort(filelist, maxfile, sizeof(namelist), cmplist);
		sorton = sorttype % 100;

		if (def) for (n = 0; n < maxfile; n++) {
			if (!strpathcmp(def, filelist[n].name)) {
				filepos = n;
				break;
			}
		}
#endif
	}

#ifndef	_NOSPLITWIN
	win = 0;
	while (windows < argc) {
		windows++;
		if (FILEPERROW < WFILEMIN) {
			windows--;
			hideclock = 1;
			warning(0, NOROW_K);
			for (i = windows; i < argc; i++) shutwin(i);
			break;
		}
	}
#endif

	strcpy(file, ".");
	_chdir2(fullpath);

	for (;;) {
		if (def) /*EMPTY*/;
		else if (strcmp(file, "..")) def = "";
		else {
			def = prev;
			strcpy(prev, getbasename(fullpath));
			if (!*prev) strcpy(file, ".");
		}

		if (strcmp(file, ".")) {
#ifdef	_USEDOSEMU
			char buf[MAXPATHLEN];
#endif

			if (chdir3(nodospath(buf, file), 1) < 0) {
				hideclock = 1;
				warning(-1, file);
				strcpy(prev, file);
				def = prev;
			}
		}

#ifdef	_NOARCHIVE
		ischgdir = browsedir(file, def);
#else
		do {
			ischgdir = browsedir(file, def);
		} while (archivefile);
#endif
		if (ischgdir < 0) {
			if (ischgdir > -2) chdir2(cwd);
			break;
		}
		if (ischgdir) def = NULL;
		else {
			strcpy(prev, file);
			strcpy(file, ".");
			def = prev;
		}
	}

	free(cwd);
#ifdef	_NOSPLITWIN
	if (filelist) free(filelist);
	if (findpattern) free(findpattern);
#else
	for (i = 0; i < MAXWINDOWS; i++) shutwin(i);
#endif
	locate(0, n_line - 1);
	tflush();
}
