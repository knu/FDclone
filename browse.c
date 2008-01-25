/*
 *	browse.c
 *
 *	directory browsing module
 */

#include "fd.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#ifndef	_NOPTY
#include "termemu.h"
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
#   define	major(n)	((((u_long)(n)) >> 18) & 0x3fff)
#   endif
#   ifndef	minor
#   define	minor(n)	(((u_long)(n)) & 0x3ffff)
#   endif
#  else
#   ifndef	major
#   define	major(n)	((((u_int)(n)) >> 8) & 0xff)
#   endif
#   ifndef	minor
#   define	minor(n)	(((u_int)(n)) & 0xff)
#   endif
#  endif
# endif
#endif

#define	CL_REG		0
#define	CL_BACK		1
#define	CL_DIR		2
#define	CL_RONLY	3
#define	CL_HIDDEN	4
#define	CL_LINK		5
#define	CL_SOCK		6
#define	CL_FIFO		7
#define	CL_BLOCK	8
#define	CL_CHAR		9
#define	CL_EXE		10
#define	ANSI_FG		8
#define	ANSI_BG		9

#if	MSDOS
extern int setcurdrv __P_((int, int));
#endif

extern bindtable bindlist[];
extern CONST functable funclist[];
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
#ifndef	_NOPTY
extern int ptymode;
extern int parentfd;
#endif

#ifndef	_NOCOLOR
static int NEAR getcolorid __P_((namelist *));
static int NEAR biascolor __P_((int));
static int NEAR getcolor __P_((int));
#endif
static VOID NEAR pathbar __P_((VOID_A));
static VOID NEAR statusbar __P_((VOID_A));
static VOID NEAR stackbar __P_((VOID_A));
static VOID NEAR cputbytes __P_((off_t, off_t, int));
static VOID NEAR sizebar __P_((VOID_A));
static int NEAR putsize __P_((char *, off_t, int, int));
static int NEAR putsize2 __P_((char *, namelist *, int));
static int NEAR putfilename __P_((char *, namelist *, int));
static VOID NEAR infobar __P_((VOID_A));
static int NEAR calclocate __P_((int));
static int NEAR calcfilepos __P_((namelist *, int, CONST char *));
static int NEAR listupmyself __P_((CONST char *));
#ifndef	_NOSPLITWIN
static int NEAR listupwin __P_((CONST char *));
#endif
static int NEAR searchmove __P_((int, char *));
#ifndef	_NOPRECEDE
static int readstatus __P_((VOID_A));
#endif
static int NEAR readfilelist __P_((CONST reg_t *, CONST char *));
static VOID NEAR getfilelist __P_((VOID_A));
static int NEAR browsedir __P_((char *, CONST char *));
static char *NEAR initcwd __P_((CONST char *, char *, int));

int curcolumns = 0;
int defcolumns = 0;
int minfilename = 0;
int mark = 0;
int fnameofs = 0;
int displaymode = 0;
int sorttype = 0;
int chgorder = 0;
int stackdepth = 0;
#ifndef	_NOTRADLAYOUT
int tradlayout = 0;
#endif
int sizeinfo = 0;
int wheader = WHEADERMIN;
off_t marksize = (off_t)0;
off_t totalsize = (off_t)0;
off_t blocksize = (off_t)0;
namelist filestack[MAXSTACK];
char fullpath[MAXPATHLEN] = "";
char *macrolist[MAXMACROTABLE];
int maxmacro = 0;
int isearch = 0;
#if	FD >= 2
int helplayout = 0;
#endif
char *helpindex[MAXHELPINDEX] = {
#ifdef	_NOTREE
	"help",
#else
	"Logdir",
#endif
	"eXec", "Copy", "Delete", "Rename", "Sort", "Find",
#ifdef	_NOTREE
	"Logdir",
#else
	"Tree",
#endif
	"Editor",
#ifdef	_NOARCHIVE
	"",
#else
	"Unpack",
#endif
#if	FD >= 2
	"Attr", "Info", "Move", "rmDir", "mKdir",
	"sHell",
# ifdef	_NOWRITEFS
	"",
# else
	"Write",
# endif
# ifdef	_NOARCHIVE
	"",
# else
	"Backup",
# endif
	"View",
# ifdef	_NOARCHIVE
	"",
# else
	"Pack",
# endif
#endif	/* FD >= 2 */
};
#ifdef	HAVEFLAGS
CONST char fflagsymlist[] = "ANacuacu";
CONST u_long fflaglist[] = {
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
int windows = 1;
#endif
#if	!defined (_NOSPLITWIN) || !defined (_NOPTY)
int win = 0;
#endif
int calc_x = -1;
int calc_y = -1;

static CONST char typesymlist[] = "dbclsp";
static CONST u_short typelist[] = {
	S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFSOCK, S_IFIFO
};
static CONST u_short modelist[] = {
	S_IFDIR, S_IFLNK, S_IFSOCK, S_IFIFO, S_IFBLK, S_IFCHR
};
#define	MAXMODELIST	arraysize(modelist)
static CONST char suffixlist[] = {
	'/', '@', '=', '|'
};
#define	MAXSUFFIXLIST	arraysize(suffixlist)
#ifndef	_NOCOLOR
static CONST u_char colorlist[] = {
	CL_DIR, CL_LINK, CL_SOCK, CL_FIFO, CL_BLOCK, CL_CHAR
};
static CONST char defpalette[] = {
	ANSI_FG,	/* CL_REG */
	ANSI_BG,	/* CL_BACK */
	ANSI_CYAN,	/* CL_DIR */
	ANSI_GREEN,	/* CL_RONLY */
	ANSI_BLUE,	/* CL_HIDDEN */
	ANSI_YELLOW,	/* CL_LINK */
	ANSI_MAGENTA,	/* CL_SOCK */
	ANSI_RED,	/* CL_FIFO */
	ANSI_FG,	/* CL_BLOCK */
	ANSI_FG,	/* CL_CHAR */
	ANSI_FG,	/* CL_EXE */
};
#endif	/* !_NOCOLOR */
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
	if (isexec(namep)) return(CL_EXE);

	return(CL_REG);
}

static int NEAR biascolor(color)
int color;
{
	if (color == ANSI_FG)
		color = (ansicolor == 3) ? ANSI_BLACK : ANSI_WHITE;
	else if (color == ANSI_BG) color = (ansicolor == 2) ? ANSI_BLACK : -1;

	return(color);
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

	return(biascolor(color));
}
#endif	/* !_NOCOLOR */

static VOID NEAR pathbar(VOID_A)
{
#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		Xlocate(0, TL_PATH);
		Xputterm(L_CLEAR);

		Xlocate(TC_PATH, TL_PATH);
		Xputterm(T_STANDOUT);
		Xcputs2(TS_PATH);
		Xputterm(END_STANDOUT);
		cputstr(TD_PATH, fullpath);

		Xlocate(TC_MARK, TL_PATH);
		Xcprintf2("%<*d", TD_MARK, mark);
		Xputterm(T_STANDOUT);
		Xcputs2(TS_MARK);
		Xputterm(END_STANDOUT);
		Xcprintf2("%<'*qd", TD_SIZE, marksize);

		Xtflush();
		return;
	}
#endif	/* !_NOTRADLAYOUT */

	Xlocate(0, L_PATH);
	Xputterm(L_CLEAR);

	Xlocate(C_PATH, L_PATH);
	Xputterm(T_STANDOUT);
	Xcputs2(S_PATH);
	Xputterm(END_STANDOUT);
	cputstr(D_PATH, fullpath);

	Xtflush();
}

VOID helpbar(VOID_A)
{
	int i, j, col, gap, width, len, ofs, max, blk, rest;

#if	FD >= 2
	if (helplayout) {
		max = helplayout / 100;
		blk = helplayout % 100;
		if (max < 0) max = FUNCLAYOUT / 100;
		else if (max > MAXHELPINDEX) max = MAXHELPINDEX;
		if (blk <= 0 || blk > max) blk = max;
	}
	else
#endif
	{
		max = FUNCLAYOUT / 100;
		blk = FUNCLAYOUT % 100;
	}

	if (ishardomit()) {
		col = n_column;
		gap = 0;
	}
	else if (iswellomit()) {
		col = n_column;
		gap = 1;
	}
	else if (isrightomit()) {
		col = n_column;
		gap = 2;
	}
	else if (isleftshift()) {
		col = n_column;
		gap = 3;
	}
	else {
		col = n_column - 1;
		gap = 3;
	}
	if (max > FUNCLAYOUT / 100) gap = 1;

	rest = max - 1 + gap * ((max - 1) / blk);
	width = (col - 4 - rest) / max;
	ofs = (n_column - width * max - rest) / 2;
	if (ofs < 4) ofs = 4;

	Xlocate(0, L_HELP);
	Xputterm(L_CLEAR);
	Xputch2(isdisplnk(dispmode) ? 'S' : ' ');
	Xputch2(isdisptyp(dispmode) ? 'T' : ' ');
	Xputch2(ishidedot(dispmode) ? 'H' : ' ');
#ifdef	HAVEFLAGS
	Xputch2(isfileflg(dispmode) ? 'F' : ' ');
#endif

	for (i = 0; i < max; i++) {
		Xlocate(ofs + (width + 1) * i + (i / blk) * gap, L_HELP);
		len = (width - strlen2(helpindex[i]) + 1) / 2;
		if (len < 0) len = 0;
		Xputterm(T_STANDOUT);
		for (j = 0; j < len; j++) Xputch2(' ');
		cputstr(width - len, helpindex[i]);
		Xputterm(END_STANDOUT);
	}

	Xtflush();
}

static VOID NEAR statusbar(VOID_A)
{
	CONST char *str[6];

#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {

		Xlocate(0, TL_STATUS);
		Xputterm(L_CLEAR);

		Xlocate(TC_INFO, TL_STATUS);
		Xputterm(T_STANDOUT);
		Xcputs2(TS_INFO);
		Xputterm(END_STANDOUT);

		Xlocate(TC_SIZE, TL_STATUS);
		Xcprintf2("%<*d", TD_MARK, maxfile);
		Xputterm(T_STANDOUT);
		Xcputs2(TS_SIZE);
		Xputterm(END_STANDOUT);
		Xcprintf2("%<'*qd", TD_SIZE, totalsize);

		return;
	}
#endif	/* !_NOTRADLAYOUT */

	Xlocate(0, L_STATUS);
	Xputterm(L_CLEAR);

	Xlocate(C_PAGE, L_STATUS);
	Xputterm(T_STANDOUT);
	Xcputs2(S_PAGE);
	Xputterm(END_STANDOUT);
	Xcprintf2("%<*d/%<*d", D_PAGE, filepos / FILEPERPAGE + 1,
		D_PAGE, (maxfile - 1) / FILEPERPAGE + 1);

	Xlocate(C_MARK, L_STATUS);
	Xputterm(T_STANDOUT);
	Xcputs2(S_MARK);
	Xputterm(END_STANDOUT);
	Xcprintf2("%<*d/%<*d", D_MARK, mark, D_MARK, maxfile);

	if (!ishardomit()) {
		Xlocate(C_SORT, L_STATUS);
		Xputterm(T_STANDOUT);
		Xcputs2(S_SORT);
		Xputterm(END_STANDOUT);

#ifndef	_NOPRECEDE
		if (haste) Xkanjiputs(OMIT_K);
		else
#endif
		if (!(sorton & 7)) Xkanjiputs(ORAW_K + 3);
		else {
			str[0] = ONAME_K;
			str[1] = OEXT_K;
			str[2] = OSIZE_K;
			str[3] = ODATE_K;
			str[4] = OLEN_K;
			Xkanjiputs(&(str[(sorton & 7) - 1][3]));

			str[0] = OINC_K;
			str[1] = ODEC_K;
			Xcprintf2("(%k)", &(str[sorton / 8][3]));
		}
	}

	Xlocate(C_FIND, L_STATUS);
	Xputterm(T_STANDOUT);
	Xcputs2(S_FIND);
	Xputterm(END_STANDOUT);
	if (findpattern) cputstr(D_FIND, findpattern);

	Xtflush();
}

static VOID NEAR stackbar(VOID_A)
{
#ifndef	_NOCOLOR
	int x, color, bgcolor;
#endif
	int i, width;

	width = n_column / MAXSTACK;

	Xlocate(0, L_STACK);
#ifndef	_NOCOLOR
	if ((bgcolor = getcolor(CL_BACK)) >= 0) Xchgcolor(bgcolor, 1);
	x = 0;
#endif
	Xputterm(L_CLEAR);

	for (i = 0; i < stackdepth; i++) {
#ifndef	_NOCOLOR
		if (ansicolor == 2)
			for (; x < width * i + 1; x++) Xputch2(' ');
		else
#endif
		Xlocate(width * i + 1, L_STACK);
#ifndef	_NOCOLOR
		color = getcolor(getcolorid(&(filestack[i])));
		if (ansicolor && color >= 0) Xchgcolor(color, 1);
		else
#endif
		Xputterm(T_STANDOUT);
		cputstr(width - 2, filestack[i].name);
#ifndef	_NOCOLOR
		x += width - 2;
		if (bgcolor >= 0) Xchgcolor(bgcolor, 1);
		else if (ansicolor) Xputterm(T_NORMAL);
		else
#endif
		Xputterm(END_STANDOUT);
	}
#ifndef	_NOCOLOR
	if (bgcolor >= 0) {
		for (; x < n_column; x++) Xputch2(' ');
		Xputterm(T_NORMAL);
	}
#endif

	Xtflush();
}

static VOID NEAR cputbytes(size, bsize, width)
off_t size, bsize;
int width;
{
	off_t kb;

	if (size < (off_t)0) Xcprintf2("%*s", width, "?");
	else if (size < (off_t)10000000 / bsize)
		Xcprintf2("%<'*qd%s", width - W_BYTES, size * bsize, S_BYTES);
	else if ((kb = calcKB(size, bsize)) < (off_t)1000000000)
		Xcprintf2("%<'*qd%s", width - W_KBYTES, kb, S_KBYTES);
	else Xcprintf2("%<'*qd%s",
		width - W_MBYTES, kb / (off_t)1024, S_MBYTES);
}

static VOID NEAR sizebar(VOID_A)
{
	off_t total, fre, bsize;

	if (!hassizeinfo() || !*fullpath) return;
	if (getinfofs(curpath, &total, &fre, &bsize) < 0)
		total = fre = (off_t)-1;

#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		Xlocate(0, TL_SIZE);
		Xputterm(L_CLEAR);

		Xlocate(TC_PAGE, TL_SIZE);
		Xputterm(T_STANDOUT);
		Xcputs2(TS_PAGE);
		Xputterm(END_STANDOUT);
		Xcprintf2("%<*d/%<*d", TD_PAGE, filepos / FILEPERPAGE + 1,
			TD_PAGE, (maxfile - 1) / FILEPERPAGE + 1);

		Xlocate(TC_TOTAL, TL_SIZE);
		Xputterm(T_STANDOUT);
		Xcputs2(TS_TOTAL);
		Xputterm(END_STANDOUT);
		cputbytes(total, bsize, TD_TOTAL);

		Xlocate(TC_USED, TL_SIZE);
		Xputterm(T_STANDOUT);
		Xcputs2(TS_USED);
		Xputterm(END_STANDOUT);
		cputbytes(total - fre, bsize, TD_USED);

		Xlocate(TC_FREE, TL_SIZE);
		Xputterm(T_STANDOUT);
		Xcputs2(TS_FREE);
		Xputterm(END_STANDOUT);
		cputbytes(fre, bsize, TD_FREE);

		return;
	}
#endif	/* !_NOTRADLAYOUT */

	Xlocate(0, L_SIZE);
	Xputterm(L_CLEAR);

	Xlocate(C_SIZE, L_SIZE);
	Xputterm(T_STANDOUT);
	Xcputs2(S_SIZE);
	Xputterm(END_STANDOUT);
	Xcprintf2("%<'*qd/%<'*qd", D_SIZE, marksize, D_SIZE, totalsize);

	if (!ishardomit()) {
		Xlocate(C_TOTAL, L_SIZE);
		Xputterm(T_STANDOUT);
		Xcputs2(S_TOTAL);
		Xputterm(END_STANDOUT);
		cputbytes(total, bsize, D_TOTAL);
	}

	if (!iswellomit()) {
		Xlocate(C_FREE, L_SIZE);
		Xputterm(T_STANDOUT);
		Xcputs2(S_FREE);
		Xputterm(END_STANDOUT);
		cputbytes(fre, bsize, D_FREE);
	}

	Xtflush();
}

u_int getfmode(c)
int c;
{
	int i;

	c = tolower2(c);
	for (i = 0; typesymlist[i]; i++)
		if (c == typesymlist[i]) return(typelist[i]);

	return((u_int)-1);
}

int putmode(buf, mode, notype)
char *buf;
u_int mode;
int notype;
{
	int i, j;

	i = 0;
	if (!notype) {
		for (j = 0; j < arraysize(typelist); j++)
			if ((mode & S_IFMT) == typelist[j]) break;
		buf[i++] = (j < arraysize(typelist)) ? typesymlist[j] : '-';
	}

	buf[i++] = (mode & S_IRUSR) ? 'r' : '-';
	buf[i++] = (mode & S_IWUSR) ? 'w' : '-';
#if	MSDOS
	buf[i++] = (mode & S_IXUSR) ? 'x' : '-';
	buf[i++] = (mode & S_ISVTX) ? 'a' : '-';
#else	/* !MSDOS */
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
#endif	/* !MSDOS */
	buf[i] = '\0';

	return(i);
}

#ifdef	HAVEFLAGS
int putflags(buf, flags)
char *buf;
u_long flags;
{
	int i;

	for (i = 0; i < arraysize(fflaglist); i++)
		buf[i] = (flags & fflaglist[i]) ? fflagsymlist[i] : '-';
	buf[i] = '\0';

	return(i);
}
#endif

#ifndef	NOUID
int putowner(buf, uid)
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

int putgroup(buf, gid)
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
	else if (s_isfifo(namep))
		snprintf2(buf, width + 1, "%*.*s", width, width, "<VOL>");
#else	/* !MSDOS */
# ifndef	_NODOSDRIVE
	else if (dospath2(nullstr) && s_isfifo(namep))
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
			strncpy3(buf, namep -> linkname, &w, len);
	}
# endif
	else {
		tmp = malloc2(width * 2 + len + 1);
		i = Xreadlink(nodospath(path, namep -> name),
			tmp, width * 2 + len);
		if (i >= 0) {
			tmp[i] = '\0';
			strncpy3(buf, tmp, &w, len);
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

	if (!filelist || filepos < 0 || maxfile < 0) return;
#ifndef	_NOPTY
	if (parentfd >= 0) return;
#endif

#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		Xlocate(TC_INFO + TW_INFO, TL_STATUS);
		cputspace(TD_INFO);

		Xlocate(TC_INFO + TW_INFO, TL_STATUS);
		if (filepos >= maxfile) {
			if (filelist[0].st_nlink < 0 && filelist[0].name)
				Xkanjiputs(filelist[0].name);
			Xtflush();
			return;
		}
		len = TD_INFO
			- (1 + TWSIZE2 + 1 + WDATE + 1 + WTIME + 1 + TWMODE);
# ifndef	_NOPRECEDE
		if (!havestat(&(filelist[filepos]))) {
			kanjiputs2(filelist[filepos].name, len, fnameofs);
			Xtflush();
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

		Xkanjiputs(buf);
		free(buf);
		Xtflush();
		return;
	}
#endif	/* !_NOTRADLAYOUT */

	Xlocate(0, L_INFO);

	if (filepos >= maxfile) {
		Xputterm(L_CLEAR);
		if (filelist[0].st_nlink < 0 && filelist[0].name)
			Xkanjiputs(filelist[0].name);
		Xtflush();
		return;
	}
#ifndef	_NOPRECEDE
	if (!havestat(&(filelist[filepos]))) {
		Xputterm(L_CLEAR);
		len = WMODE + WSIZE2 + 1 + WDATE + 1 + WTIME + 1;
		if (!ishardomit()) {
			len += 1 + WNLINK + 1;
# ifndef	NOUID
			len += (iswellomit())
				? WOWNERMIN + 1 + WGROUPMIN + 1
				: WOWNER + 1 + WGROUP + 1;
# endif
		}
		Xlocate(len, L_INFO);
		kanjiputs2(filelist[filepos].name,
			n_lastcolumn - len, fnameofs);
		Xtflush();
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
	Xkanjiputs(buf);
	free(buf);
	Xtflush();
}

VOID waitmes(VOID_A)
{
	helpbar();
	Xlocate(0, L_MESLINE);
	Xputterm(L_CLEAR);
	Xkanjiputs(WAIT_K);
	if (win_x >= 0 && win_y >= 0) Xlocate(win_x, win_y);
	Xtflush();
}

int filetop(w)
int w;
{
	int n;

	n = wheader;
	while (w) n += winvar[--w].v_fileperrow + 1;

	return(n);
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
	calc_y = i % FILEPERROW + filetop(win);
#ifndef	_NOCOLOR
	if ((bgcolor = getcolor(CL_BACK)) >= 0) {
		Xchgcolor(bgcolor, 1);
		Xlocate(calc_x, calc_y);
		if (!isleftshift()) {
			Xputch2(' ');
			calc_x++;
		}
	}
	else
#endif
	{
		if (!isleftshift()) calc_x++;
		Xlocate(calc_x, calc_y);
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
	Xputch2(ismark(&(list[no])) ? '*' : ' ');

	if (list != filelist) {
		len = strlen3(list[no].name);
		if (isdisptyp(dispmode) && s_isdir(&(list[no]))) len++;
		if (col > len) col = len;
		if (width > len) width = len;
	}

	if (isstandout < 0 && stable_standout) {
		Xputterm(END_STANDOUT);
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
		if (isstandout > 0) Xputterm(T_STANDOUT);
		Xkanjiputs(buf);
		free(buf);
		if (isstandout > 0) Xputterm(END_STANDOUT);
		return(wid);
	}
#endif

	if (isdisptyp(dispmode) && i < width) {
		for (j = 0; j < MAXMODELIST; j++)
			if ((list[no].st_mode & S_IFMT) == modelist[j]) break;
		if (j < MAXSUFFIXLIST) buf[i] = suffixlist[j];
		else if (s_isreg(&(list[no]))
		&& (list[no].st_mode & S_IEXEC_ALL))
			buf[i] = '*';
	}
#ifndef	_NOCOLOR
	if (list == filelist) color = getcolor(getcolorid(&(list[no])));
	else color = biascolor(ANSI_FG);
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
	if (ansicolor && color >= 0) Xchgcolor(color, isstandout > 0);
	else
#endif
	if (isstandout > 0) Xputterm(T_STANDOUT);
	Xkanjiputs(buf);
	free(buf);
#ifndef	_NOCOLOR
	if ((bgcolor = getcolor(CL_BACK)) >= 0) {
		Xchgcolor(bgcolor, 1);
		Xputch2(' ');
	}
	if (ansicolor) Xputterm(T_NORMAL);
	else
#endif
	if (isstandout > 0) Xputterm(END_STANDOUT);

	return(col);
}

static int NEAR calcfilepos(list, max, def)
namelist *list;
int max;
CONST char *def;
{
	int i;

	if (def) for (i = 0; i < max; i++)
		if (!strpathcmp(def, list[i].name)) return(i);

	if (list == filelist && max > 1 && isdotdir(list[1].name) == 1)
		return(1);

	return(0);
}

int listupfile(list, max, def, isstandout)
namelist *list;
int max;
CONST char *def;
int isstandout;
{
	CONST char *cp;
	int i, n, pos;

	for (i = 0; i < FILEPERROW; i++) {
		Xlocate(0, i + filetop(win));
		Xputterm(L_CLEAR);
	}

	if (!list || max <= 0) {
		i = (n_column / FILEPERLINE) - 2 - 1;
		Xlocate(1, filetop(win));
		Xputch2(' ');
		if (isstandout) Xputterm(T_STANDOUT);
		cp = NOFIL_K;
		if (i <= strlen2(cp)) cp = "NoFiles";
		cputstr(i, cp);
		if (isstandout) Xputterm(END_STANDOUT);
		win_x = calc_x = i + 2;
		win_y = calc_y = filetop(win);
		return(0);
	}

	pos = calcfilepos(list, max, def);

	if (list == filelist) {
#ifndef	_NOTRADLAYOUT
		if (istradlayout()) {
			Xlocate(TC_PAGE + TW_PAGE, TL_SIZE);
			Xcprintf2("%<*d", TD_PAGE, pos / FILEPERPAGE + 1);
		}
		else
#endif
		{
			Xlocate(C_PAGE + W_PAGE, L_STATUS);
			Xcprintf2("%<*d", D_PAGE, pos / FILEPERPAGE + 1);
		}
	}

	n = (pos / FILEPERPAGE) * FILEPERPAGE;
	for (i = 0; i < FILEPERPAGE; i++, n++) {
		if (n >= max) break;
		if (!isstandout || n != pos) putname(list, n, 0);
		else {
			calc_x += putname(list, n, 1) + 1;
			win_x = calc_x;
			win_y = calc_y;
		}
	}

	calc_x = win_x;
	calc_y = win_y;

	return(pos);
}

static int NEAR listupmyself(def)
CONST char *def;
{
#ifndef	_NOTREE
	if (treepath) rewritetree();
	else
#endif
#ifndef	_NOCUSTOMIZE
	if (custno >= 0) rewritecust();
	else
#endif
#ifndef	_NOPTY
	if (ptylist[win].pid && ptylist[win].status < 0) /*EMPTY*/;
	else
#endif
	if (filelist && (filepos < maxfile || (!filepos && !maxfile)))
		return(listupfile(filelist, maxfile, def, 1));

	return(-1);
}

#ifndef	_NOSPLITWIN
static int NEAR listupwin(def)
CONST char *def;
{
	int i, x, y, n, dupwin;

	dupwin = win;
	y = wheader + winvar[0].v_fileperrow;
	for (n = 1; n < windows; n++) {
		Xlocate(0, y);
		Xputterm(L_CLEAR);
		Xputch2(' ');
		for (i = 2; i < n_column; i++) Xputch2('-');
		y += winvar[n].v_fileperrow + 1;
	}
	for (; y < L_STACK; y++) {
		Xlocate(0, y);
		Xputterm(L_CLEAR);
	}

# ifdef	FAKEUNINIT
	x = y = -1;	/* fake for -Wuninitialized */
# endif
	n = -1;
	for (win = 0; win < windows; win++) {
		if (win == dupwin) {
			n = listupmyself(def);
			x = win_x;
			y = win_y;
		}
# ifndef	_NOPTY
		else if (ptylist[win].pid && ptylist[win].status < 0) continue;
# endif
		else if (filelist
		&& (filepos < maxfile || (!filepos && !maxfile)))
			listupfile(filelist, maxfile,
				filelist[filepos].name, 0);
	}
	win = dupwin;

	win_x = x;
	win_y = y;

	return(n);
}

int shutwin(n)
int n;
{
	int i, dupwin;

	dupwin = win;
	win = n;
# ifndef	_NOARCHIVE
	while (archivefile) {
#  ifdef	_NOBROWSE
		escapearch();
#  else
		do {
			escapearch();
		} while (browselist);
#  endif
	}
	if (winvar[win].v_archivedir) free(winvar[win].v_archivedir);
	winvar[win].v_archivedir = NULL;
# endif	/* !_NOARCHIVE */
	if (winvar[win].v_fullpath) free(winvar[win].v_fullpath);
	winvar[win].v_fullpath = NULL;
	if (filelist) {
		for (i = 0; i < maxfile; i++)
			if (filelist[i].name) free(filelist[i].name);
		free(filelist);
		filelist = NULL;
	}
	maxfile = maxent = filepos = sorton = dispmode = 0;
	if (findpattern) free(findpattern);
	findpattern = NULL;
# ifndef	_NOPTY
	killpty(win, NULL);
# endif
	win = dupwin;

	return(n);
}
#endif	/* !_NOSPLITWIN */

VOID calcwin(VOID_A)
{
	int i, row;

	row = fileperrow(windows);
	winvar[windows - 1].v_fileperrow = fileperrow(1);
	for (i = 0; i < windows - 1; i++) {
		winvar[i].v_fileperrow = row;
		winvar[windows - 1].v_fileperrow -= row + 1;
	}
	for (i = windows; i < MAXWINDOWS; i++) winvar[i].v_fileperrow = 0;
}

VOID movepos(old, funcstat)
int old, funcstat;
{
#ifndef	_NOSPLITWIN
	if (rewritemode(funcstat) >= FN_REWIN) {
		filepos = listupwin(filelist[filepos].name);
		keyflush();
	}
	else
#endif
	if (rewritemode(funcstat) >= FN_RELIST
	|| old / FILEPERPAGE != filepos / FILEPERPAGE) {
		filepos = listupfile(filelist, maxfile,
			filelist[filepos].name, 1);
		keyflush();
	}
	else if (rewritemode(funcstat) >= FN_REWRITE || old != filepos) {
		if (old != filepos) putname(filelist, old, -1);
		calc_x += putname(filelist, filepos, 1) + 1;
		win_x = calc_x;
		win_y = calc_y;
	}
	infobar();
}

VOID rewritefile(all)
int all;
{
	int x, y;

	if (!filelist || filepos < 0 || maxfile < 0) return;
#ifndef	_NOPTY
	if (parentfd >= 0) return;
#endif

	x = win_x;
	y = win_y;
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
#ifdef	_NOSPLITWIN
		listupmyself(filelist[filepos].name);
#else
		listupwin(filelist[filepos].name);
#endif
	}

	if (!all) {
		win_x = x;
		win_y = y;
	}
	Xlocate(win_x, win_y);
	Xtflush();
}

static int NEAR searchmove(ch, buf)
int ch;
char *buf;
{
	CONST char *str[4];
	int i, n, s, pos, len, func;

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
	func = (i < MAXBINDTABLE && bindlist[i].key >= 0) ? ffunc(i) : -1;
	if (ch == K_BS) {
		if (!len) isearch = 0;
		else buf[--len] = '\0';
	}
	else if (len && func == SEARCH_FORW) {
		if (n > 2) pos++;
		else if (n > 1) pos = 0;
		s = 1;
	}
	else if (len && func == SEARCH_BACK) {
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
		else if (ch != K_ESC) Xputterm(T_BELL);
		return(0);
	}

	Xlocate(0, L_HELP);
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

	Xputterm(T_STANDOUT);
	search_x = len;
	len = Xkanjiputs(str[2 - s - i]);
	Xputterm(END_STANDOUT);
	cputstr(n_column - len - 1, buf);
	if ((search_x += len) >= n_column) search_x = n_column - 1;
	search_y = L_HELP;
	if (i) filepos = pos;
	else if (n != 2 && ch != K_BS) Xputterm(T_BELL);
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
static int readstatus(VOID_A)
{
	int i;

	for (i = maxstat; i < maxfile; i++)
		if (!havestat(&(filelist[i]))) break;
	if (i >= maxfile) return(0);
	maxstat = i + 1;
	if (getstatus(&(filelist[i])) < 0) return(0);
	if (keywaitfunc != readstatus) return(0);
	if (isfile(&(filelist[i])) && filelist[i].st_size) sizebar();
	if (i == filepos) {
		calc_x += putname(filelist, i, 1) + 1;
		win_x = calc_x;
		win_y = calc_y;
		infobar();
	}
	else if (i / FILEPERPAGE == filepos / FILEPERPAGE)
		putname(filelist, i, 0);
	Xlocate(win_x, win_y);
	Xtflush();

	return(0);
}
#endif	/* !_NOPRECEDE */

static int NEAR readfilelist(re, arcre)
CONST reg_t *re;
CONST char *arcre;
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

	if (!(dirp = Xopendir(curpath))) {
		lostcwd(NULL);
		if (!(dirp = Xopendir(curpath))) error(curpath);
	}

	while ((dp = searchdir(dirp, re, arcre))) {
		if (ishidedot(dispmode) && *(dp -> d_name) == '.'
		&& !isdotdir(dp -> d_name))
			continue;
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

static VOID NEAR getfilelist(VOID_A)
{
	reg_t *re;
	char *arcre;

	re = NULL;
	arcre = NULL;
	if (findpattern) {
#ifndef	_NOARCHIVE
		if (*findpattern == '/') arcre = &(findpattern[1]);
		else
#endif
		re = regexp_init(findpattern, -1);
	}

	maxfile = 0;
#ifndef	_NOARCHIVE
	if (archivefile) {
		blocksize = (off_t)1;
		if (sorttype < 100) sorton = 0;
		copyarcf(re, arcre);
	}
	else
#endif
	{
		blocksize = getblocksize(curpath);
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
		filelist[0].name = (char *)NOFIL_K;
		filelist[0].tmpflags = 0;
		filelist[0].st_nlink = -1;
	}

#ifndef	_NOPRECEDE
	maxstat = (haste) ? 0 : maxfile;
#endif
	if (sorton) qsort(filelist, maxfile, sizeof(namelist), cmplist);
}

static int NEAR browsedir(file, def)
char *file;
CONST char *def;
{
#ifndef	_NOARCHIVE
	CONST char *tmp;
#endif
#ifndef	_NOPRECEDE
	int dupsorton;
#endif
	CONST char *cp;
	char buf[MAXNAMLEN + 1];
	int ch, i, no, old, funcstat;

#ifndef	_NOPRECEDE
	haste = 0;
	dupsorton = sorton;
#endif
#ifndef	_NOCOLOR
	if (ansicolor) Xputterm(T_NORMAL);
#endif

	mark = 0;
	totalsize = marksize = (off_t)0;
	chgorder = 0;

	fnameofs = 0;
	win_x = win_y = 0;
	waitmes();

#ifndef	_NOARCHIVE
	if (!archivefile) /*EMPTY*/;
	else if (*file) def = file;
	else def = nullstr;
#endif
	getfilelist();

	for (i = 0; i < maxfile; i++) {
		if (!isfile(&(filelist[i])) || !filelist[i].st_size) continue;
		totalsize += getblock(filelist[i].st_size);
		if (ismark(&(filelist[i])))
			marksize += getblock(filelist[i].st_size);
	}

	title();
	helpbar();
	rewritefile(-1);
#ifdef	_NOSPLITWIN
	old = filepos = listupfile(filelist, maxfile, def, 1);
#else
	old = filepos = listupwin(def);
#endif
	funcstat = 0;
	no = FNC_CANCEL;

	keyflush();
	buf[0] = '\0';
	for (;;) {
		if (no) movepos(old, funcstat);
		if (search_x >= 0 && search_y >= 0) {
			win_x = search_x;
			win_y = search_y;
		}
		Xlocate(win_x, win_y);
		Xtflush();
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
		if (i >= MAXBINDTABLE || bindlist[i].key < 0)
			no = NO_OPERATION;
#ifndef	_NOPRECEDE
		else if (haste && !havestat(&(filelist[filepos]))
		&& (hasdfunc(i) || (funclist[ffunc(i)].status & FN_NEEDSTATUS))
		&& getstatus(&(filelist[filepos])) < 0)
			no = WARNING_BELL;
#endif
		else no = (hasdfunc(i) && isdir(&(filelist[filepos])))
			? dfunc(i) : ffunc(i);
		if (no < FUNCLISTSIZ) funcstat = funclist[no].status;
#ifndef	_NOARCHIVE
		else if (archivefile) continue;
#endif
		else funcstat = (FN_KILLSTACK | FN_RESCREEN | FN_REWIN);

		if (funcstat & FN_KILLSTACK) {
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
					filelist[filepos].name, 1);
				stackbar();
			}
#ifndef	_NOWRITEFS
# ifndef	_NOARCHIVE
			else if (archivefile) /*EMPTY*/;
# endif
			else if (chgorder && writefs < 1 && no != WRITE_DIR
			&& !fd_restricted
			&& (i = writablefs(curpath)) > 0
			&& underhome(NULL) > 0) {
				chgorder = 0;
				if (yesno(WRTOK_K)) arrangedir(i);
			}
#endif	/* !_NOWRITEFS */
		}

		curfilename = filelist[filepos].name;
		if (maxfile <= 0 && !(funcstat & FN_NOFILE)) no = FNC_NONE;
#ifndef	_NOARCHIVE
		else if (archivefile && !(funcstat & FN_ARCHIVE))
			no = FNC_NONE;
#endif
		else if (no < FUNCLISTSIZ) {
			if (!fd_restricted || !(funcstat & FN_RESTRICT))
				no = (*funclist[no].func)(NULL);
			else {
				warning(0, RESTR_K);
				no = FNC_NONE;
			}
		}
		else {
			no = ptyusercomm(getmacro(no),
				filelist[filepos].name, 0);
			no = evalstatus(no);
#ifndef	_NOPTY
			if (ptymode && isearch) no = FNC_NONE;
#endif
		}

#ifndef	_NOPTY
		while (ptylist[win].pid && ptylist[win].status < 0) {
			VOID_C frontend();
			no = FNC_UPDATE;
			funcstat &= ~FN_REWRITEMODE;
			funcstat |= FN_REWIN;
		}
#endif	/* !_NOPTY */

#ifndef	_NOPRECEDE
		if (sorton) haste = 0;
#endif
		if (no < FNC_NONE || no >= FNC_EFFECT) break;
		if (no == FNC_CANCEL || no == FNC_HELPSPOT) helpbar();
		if (no < FNC_UPDATE) {
			funcstat = 0;
			continue;
		}

		if (rewritemode(funcstat) != FN_REWRITE || old != filepos)
			fnameofs = 0;
		if (funcstat & FN_RESCREEN) {
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
		else if (no == FNC_CHDIR) {
			tmp = (filepos >= 0) ? filelist[filepos].name : NULL;
			if (!(cp = archchdir(tmp))) {
				if (!tmp) tmp = parentpath;
				warning(-1, tmp);
				strcpy(file, tmp);
			}
			else if (cp != (char *)-1) strcpy(file, cp);
			else {
				escapearch();
				strcpy(file, filelist[filepos].name);
			}
		}
		else if (no == FNC_EFFECT) {
			if (filepos < 0) *file = '\0';
			else strcpy(file, filelist[filepos].name);
		}
		no = FNC_NONE;
	}
	else
#endif	/* !_NOARCHIVE */
	if (no >= FNC_EFFECT) {
		no -= FNC_EFFECT;
		if (!maxfile && !ischgdir(&(filelist[filepos]))) cp = curpath;
		else cp = filelist[filepos].name;
		strcpy(file, cp);
	}

#ifndef	_NOARCHIVE
	if (archivefile) i = 0;
	else
#endif
	i = (maxfile || !ischgdir(&(filelist[0]))) ? maxfile : 1;
	while (i-- > 0) {
		free(filelist[i].name);
		filelist[i].name = NULL;
	}
	maxfile = filepos = 0;
#ifndef	_NOPRECEDE
	if (haste && !sorton) sorton = dupsorton;
#endif

	return(no);
}

static char *NEAR initcwd(path, buf, evaled)
CONST char *path;
char *buf;
int evaled;
{
	char *cp, *file;
	int i;

	if (!path) return(NULL);

	cp = strdup2(path);
	if (!evaled) cp = evalpath(cp, 0);
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
				if (chdir2(rootpath) < 0) error(rootpath);
			}
			else if (chdir2(cp) < 0) {
				hideclock = 2;
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

VOID main_fd(pathlist, evaled)
char *CONST *pathlist;
int evaled;
{
#ifdef	_USEDOSEMU
	char buf[MAXPATHLEN];
#endif
	char *def, *cwd, file[MAXNAMLEN + 1], prev[MAXNAMLEN + 1];
	int i, argc, ischgdir;

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
#ifndef	_NOPTY
		ptylist[i].pid = (p_id_t)0;
		ptylist[i].path = NULL;
		ptylist[i].fd = ptylist[i].pipe = -1;
		ptylist[i].status = 0;
#endif

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
		FILEPERROW = 0;

		if (i >= argc) continue;

		chdir2(cwd);
		def = initcwd(pathlist[i], prev, evaled);

#ifndef	_NOSPLITWIN
		winvar[i].v_fullpath = strdup2(fullpath);
		if (i) {
			getfilelist();
			sorton = sorttype % 100;
			filepos = calcfilepos(filelist, maxfile, def);
		}
#endif
	}

#ifndef	_NOSPLITWIN
	windows = 1;
	win = 0;
	while (windows < argc) {
		if (fileperrow(windows + 1) < WFILEMIN) {
			hideclock = 2;
			warning(0, NOROW_K);
			for (i = windows; i < argc; i++) shutwin(i);
			break;
		}
		windows++;
	}
#endif	/* !_NOSPLITWIN */
	calcwin();

	copycurpath(file);
	_chdir2(fullpath);

	for (;;) {
		if (!def && isdotdir(file) == 1) {
			strcpy(prev, getbasename(fullpath));
			if (*prev) def = prev;
			else copycurpath(file);
		}

		if (isdotdir(file) != 2
		&& chdir3(nodospath(buf, file), 1) < 0) {
			hideclock = 2;
			warning(-1, file);
			strcpy(prev, file);
			def = prev;
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
			copycurpath(file);
			def = prev;
		}
	}

	free(cwd);
#ifdef	_NOSPLITWIN
	if (filelist) free(filelist);
	if (findpattern) free(findpattern);
#else
	for (i = 0; i < MAXWINDOWS; i++) shutwin(i);
	windows = 1;
	win = 0;
	calcwin();
#endif
#ifndef	_NOPTY
	killallpty();
#endif
	Xlocate(0, n_line - 1);
	Xtflush();
}
