/*
 *	archive.c
 *
 *	archive file accessing module
 */

#include "fd.h"
#include "realpath.h"
#include "kconv.h"
#include "func.h"
#include "kanji.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	DEP_PTY
#include "termemu.h"
#endif

#ifndef	_NOARCHIVE

#if	MSDOS
extern char *unixgetcurdir __P_((char *, int));
#endif

extern int mark;
#if	FD >= 2
extern int sorttype;
#endif
extern int stackdepth;
#ifndef	_NOTRADLAYOUT
extern int tradlayout;
#endif
extern int sizeinfo;
extern off_t marksize;
extern namelist filestack[];
extern char fullpath[];
#ifndef	_NOCOLOR
extern int ansicolor;
#endif
extern int win_x;
extern int win_y;
extern char *deftmpdir;
extern int hideclock;
extern int n_args;

#ifdef	OLDPARSE
# if	MSDOS
#define	PM_LHA	5, 2, \
		{FLD_NONE, FLD_NONE, FLD_NONE, 1, 4, 4, 4, 5, 0}, \
		{0, 0, 0, 0, 0, '-', 128 + 6, 0, 0}, \
		{0, 0, 0, 0, '-', '-', 0, 0, 0}, \
		{SEP_NONE, SEP_NONE, SEP_NONE}, 2
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 5, 3, 4, 6, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{0, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{SEP_NONE, SEP_NONE, SEP_NONE}, 1
# else	/* !MSDOS */
#define	PM_LHA	2, 2, \
		{0, 1, 1, 2, 6, 4, 5, 6, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{9, SEP_NONE, SEP_NONE}, 1
#  ifdef	UXPDS
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 10, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{10, SEP_NONE, SEP_NONE}, 1
#  else	/* !UXPDS */
#   ifdef	TARFROMPAX
#define	PM_TAR	0, 0, \
		{0, 2, 3, 4, 7, 5, 6, 7, 8}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{SEP_NONE, SEP_NONE, SEP_NONE}, 1
#   else	/* !TARFROMPAX */
#    ifdef	TARUSESPACE
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{0, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{SEP_NONE, SEP_NONE, SEP_NONE}, 1
#    else	/* !TARUSESPACE */
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{9, SEP_NONE, SEP_NONE}, 1
#    endif	/* !TARUSESPACE */
#   endif	/* !TARFROMPAX */
#  endif	/* !UXPDS */
# endif	/* !MSDOS */
#define	PM_NULL	SKP_NONE, 0, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{SEP_NONE, SEP_NONE, SEP_NONE}, 1
#define	isarchbr(l)		(((l) -> topskip) != SKP_NONE)
#else	/* !OLDPARSE */
#define	PM_LHA			form_lha, ign_lha, NULL, 0, 0
#define	PM_TAR			form_tar, NULL, NULL, 0, 0
#define	PM_NULL			NULL, NULL, NULL, 0, 0
#define	isarchbr(l)		((l) -> format)
#endif	/* !OLDPARSE */

#ifndef	_NOBROWSE
static VOID NEAR copyargvar __P_((int, char *CONST *));
static VOID NEAR pushbrowsevar __P_((CONST char *));
static VOID NEAR popbrowsevar __P_((VOID_A));
#endif
static VOID NEAR pusharchdupl __P_((VOID_A));
static VOID NEAR poparchdupl __P_((VOID_A));
static VOID NEAR Xwaitmes __P_((VOID_A));
static VOID NEAR unpackerror __P_((VOID_A));
static char *archfgets __P_((VOID_P));
static int NEAR readarchive __P_((CONST char *, lsparse_t *, int));
static CONST char *NEAR skiparchdir __P_((CONST char *));
static int NEAR getarchdirlen __P_((CONST char *));
static char *NEAR searcharcdir __P_((CONST char *, int));
static char *NEAR archoutdir __P_((VOID_A));
static int NEAR undertmp __P_((CONST char *));
#ifdef	DEP_PSEUDOPATH
static char *NEAR genfullpath __P_((char *,
		CONST char *, CONST char *, CONST char *));
static int NEAR archdostmpdir __P_((char *, char **, CONST char *));
#else
static char *NEAR genfullpath __P_((char *, CONST char *, CONST char *));
#endif
#ifndef	NOSYMLINK
static int NEAR archrealpath __P_((char *, char *));
static int NEAR unpacklink __P_((namelist *, CONST char *));
#endif

#ifndef	OLDPARSE
static char *form_lha[] = {
# if	MSDOS
	"%*f\\n%s %x %x %y-%m-%d %t %a",	/* MS-DOS (-v) */
	"%1x %12f %s %x %x %y-%m-%d %t %a",	/* MS-DOS (-l) */
# else
	"%a %u/%g %s %x %m %d %{yt} %*f",	/* >=1.14 */
	"%9a %u/%g %s %x %m %d %{yt} %*f",	/* traditional */
# endif
	NULL
};
static char *ign_lha[] = {
# if	MSDOS
	"Listing of archive : *",
	"  Name          Original *",
	"--------------*",
#  if	defined (FAKEESCAPE) || !defined (BSPATHDELIM)
	"* files * ???.?% ?\077-?\077-?? ??:??:??",	/* avoid trigraph */
#  else
	"* files * ???.?%% ?\077-?\077-?? ??:??:??",	/* avoid trigraph */
#  endif
	"",
# else	/* !MSDOS */
	" PERMSSN * UID*GID *",
	"----------*",
	" Total * file* ???.*%*",
# endif	/* !MSDOS */
	NULL
};
static char *form_tar[] = {
# if	MSDOS
	"%a %u/%g %s %m %d %t %y %*f",		/* traditional */
	"%a %u/%g %s %y-%m-%d %t %*f",		/* GNU >=1.12 */
	"%a %u/%g %s %m %d %y %t %*f",		/* kmtar */
# else	/* !MSDOS */
	"%a %u/%g %s %m %d %t %y %*f",		/* SVR4 */
	"%a %u/%g %s %y-%m-%d %t %*f",		/* GNU >=1.12 */
	"%a %l %u %g %s %m %d %{yt} %*f",	/* pax */
	"%10a %u/%g %s %m %d %t %y %*f",	/* tar (UXP/DS) */
	"%9a %u/%g %s %m %d %t %y %*f",		/* traditional */
	"%a %u %g %s %m %d %t %y %*f",		/* AIX */
	"%a %u/%g %m %d %t %y %*f",		/* IRIX */
# endif	/* !MSDOS */
	NULL
};
#endif	/* !OLDPARSE */

int maxlaunch = 0;
int maxarchive = 0;
#if	defined (DEP_DYNAMICLIST) || !defined (_NOCUSTOMIZE)
int origmaxlaunch = 0;
int origmaxarchive = 0;
#endif
#ifdef	DEP_DYNAMICLIST
launchlist_t launchlist = NULL;
origlaunchlist_t origlaunchlist
#else	/* !DEP_DYNAMICLIST */
# ifndef	_NOCUSTOMIZE
origlaunchlist_t origlaunchlist = NULL;
# endif
launchlist_t launchlist
#endif	/* !DEP_DYNAMICLIST */
= {
#if	MSDOS
	{"*.lzh",	"lha v %S",		PM_LHA, 0},
	{"*.tar",	"tar tvf %S",		PM_TAR, 0},
	{"*.tar.Z",	"gzip -cd %S|tar tvf -",	PM_TAR, 0},
	{"*.tar.gz",	"gzip -cd %S|tar tvf -",	PM_TAR, 0},
	{"*.tar.bz2",	"bzip2 -cd %S|tar tvf -",	PM_TAR, 0},
	{"*.taz",	"gzip -cd %S|tar tvf -",	PM_TAR, 0},
	{"*.tgz",	"gzip -cd %S|tar tvf -",	PM_TAR, 0},
#else	/* !MSDOS */
	{"*.lzh",	"lha l",		PM_LHA, 0},
	{"*.tar",	"tar tvf",		PM_TAR, 0},
	{"*.tar.Z",	"zcat %C|tar tvf -",	PM_TAR, 0},
	{"*.tar.gz",	"gzip -cd %C|tar tvf -",	PM_TAR, 0},
	{"*.tar.bz2",	"bzip2 -cd %C|tar tvf -",	PM_TAR, 0},
# ifndef	CYGWIN
	{"*.taZ",	"zcat %C|tar tvf -",	PM_TAR, 0},
# endif
	{"*.taz",	"gzip -cd %C|tar tvf -",	PM_TAR, 0},
	{"*.tgz",	"gzip -cd %C|tar tvf -",	PM_TAR, 0},
#endif	/* !MSDOS */
	{NULL,		NULL,			PM_NULL, 0}
};
#ifdef	DEP_DYNAMICLIST
archivelist_t archivelist = NULL;
origarchivelist_t origarchivelist
#else	/* !DEP_DYNAMICLIST */
# ifndef	_NOCUSTOMIZE
origarchivelist_t origarchivelist = NULL;
# endif
archivelist_t archivelist
#endif	/* !DEP_DYNAMICLIST */
= {
#if	MSDOS
	{"*.lzh",	"lha a %S %TA",		"lha x %S %TA", 0},
	{"*.tar",	"tar cf %C %T",		"tar xf %C %TA", 0},
	{"*.tar.Z",	"tar cf - %T|compress -c > %C",
					"gzip -cd %S|tar xf - %TA", 0},
	{"*.tar.gz",	"tar cf - %T|gzip -c > %C",
					"gzip -cd %S|tar xf - %TA", 0},
	{"*.tar.bz2",	"tar cf - %T|bzip2 -c > %C",
					"bzip2 -cd %S|tar xf - %TA", 0},
	{"*.taz",	"tar cf - %T|compress -c > %C",
					"gzip -cd %S|tar xf - %TA", 0},
	{"*.tgz",	"tar cf - %T|gzip -c > %C",
					"gzip -cd %S|tar xf - %TA", 0},
#else	/* !MSDOS */
	{"*.lzh",	"lha aq %C %TA",	"lha xq %C %TA", 0},
	{"*.tar",	"tar cf %C %T",		"tar xf %C %TA", 0},
	{"*.tar.Z",	"tar cf - %T|compress -c > %C",
					"zcat %C|tar xf - %TA", 0},
	{"*.tar.gz",	"tar cf - %T|gzip -c > %C",
					"gzip -cd %C|tar xf - %TA", 0},
	{"*.tar.bz2",	"tar cf - %T|bzip2 -c > %C",
					"bzip2 -cd %C|tar xf - %TA", 0},
# ifndef	CYGWIN
	{"*.taZ",	"tar cf - %T|compress -c > %C",
					"zcat %C|tar xf - %TA", 0},
# endif
	{"*.taz",	"tar cf - %T|gzip -c > %C",
					"gzip -cd %C|tar xf - %TA", 0},
	{"*.tgz",	"tar cf - %T|gzip -c > %C",
					"gzip -cd %C|tar xf - %TA", 0},
#endif	/* !MSDOS */
	{NULL,		NULL,			NULL, 0}
};
char archivedir[MAXPATHLEN];
#ifndef	_NOBROWSE
char **browsevar = NULL;
#endif


#ifndef	_NOBROWSE
static VOID NEAR copyargvar(argc, argv)
int argc;
char *CONST *argv;
{
	int i;

	if (argvar && argvar[0]) for (i = 1; argvar[i]; i++) Xfree(argvar[i]);
	argvar = (char **)Xrealloc(argvar, (argc + 2) * sizeof(char *));
	for (i = 0; i < argc; i++) argvar[i + 1] = Xstrdup(argv[i]);
	argvar[i + 1] = NULL;
}

static VOID NEAR pushbrowsevar(s)
CONST char *s;
{
	int i;

	i = countvar(browsevar);
	browsevar = (char **)Xrealloc(browsevar, (i + 2) * sizeof(char *));
	if (!i) browsevar[1] = NULL;
	else memmove((char *)&(browsevar[1]), (char *)&(browsevar[0]),
		(i + 1) * sizeof(char *));
	browsevar[0] = Xstrdup(s);
	copyargvar(i + 1, browsevar);
}

static VOID NEAR popbrowsevar(VOID_A)
{
	int i;

	i = countvar(browsevar);
	if (i <= 0) return;
	Xfree(browsevar[0]);
	memmove((char *)&(browsevar[0]), (char *)&(browsevar[1]),
		i * sizeof(char *));
	copyargvar(i - 1, browsevar);
}
#endif	/* !_NOBROWSE */

static VOID NEAR pusharchdupl(VOID_A)
{
	winvartable *new;

	new = (winvartable *)Xmalloc(sizeof(winvartable));

	new -> v_archduplp = archduplp;
	new -> v_archivedir = NULL;
	new -> v_archivefile = archivefile;
	new -> v_archtmpdir = archtmpdir;
	new -> v_launchp = launchp;
	new -> v_arcflist = arcflist;
	new -> v_maxarcf = maxarcf;
#ifdef	DEP_PSEUDOPATH
	new -> v_archdrive = archdrive;
#endif
#ifndef	_NOBROWSE
	new -> v_browselist = browselist;
	new -> v_browselevel = browselevel;
#endif
#ifndef	_NOTREE
	new -> v_treepath = treepath;
#endif
	new -> v_fullpath =
	new -> v_lastfile = NULL;
	new -> v_findpattern = findpattern;
	new -> v_filepos = filepos;
	new -> v_sorton = sorton;
	archduplp = new;
}

VOID escapearch(VOID_A)
{
#ifndef	_NOBROWSE
	if (!archduplp) return;
	if (browselist) popbrowsevar();
#endif
	poparchdupl();
}

static VOID NEAR poparchdupl(VOID_A)
{
#ifdef	DEP_PSEUDOPATH
	int drive;
#endif
	winvartable *old;
	char *cp, *dir;

	if (!(old = archduplp)) return;

	cp = archivefile;
	dir = archtmpdir;
#ifdef	DEP_PSEUDOPATH
	drive = archdrive;
#endif
	freelist(arcflist, maxarcf);

	archduplp = old -> v_archduplp;
	if (!(old -> v_archivedir)) *archivedir = '\0';
	else {
		Xstrcpy(archivedir, old -> v_archivedir);
		Xfree(old -> v_archivedir);
	}
	archivefile = old -> v_archivefile;
	archtmpdir = old -> v_archtmpdir;
	launchp = old -> v_launchp;
	arcflist = old -> v_arcflist;
	maxarcf = old -> v_maxarcf;
#ifdef	DEP_PSEUDOPATH
	archdrive = old -> v_archdrive;
#endif
#ifndef	_NOBROWSE
	browselevel = old -> v_browselevel;
#endif
#ifndef	_NOTREE
	treepath = old -> v_treepath;
#endif
	Xfree(findpattern);
	findpattern = old -> v_findpattern;
	filepos = old -> v_filepos;
	sorton = old -> v_sorton;
	maxfile = maxent;
	while (maxfile > 0) filelist[--maxfile].name = NULL;

#ifndef	_NOBROWSE
	if (browselist) {
		if (!(old -> v_browselist)) {
			freebrowse(browselist);
			browselist = NULL;
		}
	}
	else
#endif
#ifdef	DEP_PSEUDOPATH
	if (drive > 0) {
		Xstrcpy(fullpath, old -> v_fullpath);
		Xfree(old -> v_fullpath);
		removetmp(dir, cp);
	}
	else
#endif
	if (archivefile) {
		Xstrcpy(fullpath, old -> v_fullpath);
		Xfree(old -> v_fullpath);
		removetmp(dir, NULL);
	}

	Xfree(cp);
	Xfree(old);
}

VOID archbar(file, dir)
CONST char *file, *dir;
{
#ifndef	_NOBROWSE
	int i;
#endif
	char *arch;
	int len;

#ifndef	_NOBROWSE
	if (browselist) {
		if (!browsevar) arch = Xstrdup(nullstr);
		else {
			len = 0;
			for (i = 0; browsevar[i]; i++)
				len += strlen(browsevar[i]) + 1;
			arch = Xmalloc(len + 1);
			len = 0;
			for (i--; i >= 0; i--) {
				Xstrcpy(&(arch[len]), browsevar[i]);
				len += strlen(browsevar[i]);
				if (i > 0) arch[len++] = '>';
			}
			arch[len] = '\0';
		}
	}
	else
#endif	/* !_NOBROWSE */
	{
		arch = Xmalloc(strlen(fullpath)
			+ strlen(file) + strlen(dir) + 3);
		Xstrcpy(Xstrcpy(strcatdelim2(arch, fullpath, file), ":"), dir);
	}

#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		Xlocate(0, TL_PATH);
		Xputterm(L_CLEAR);

		Xlocate(TC_PATH, TL_PATH);
		Xputterm(T_STANDOUT);
# ifndef	_NOBROWSE
		if (browselist) {
			XXcputs(TS_BROWSE);
			len = TD_BROWSE;
		}
		else
# endif
		{
			XXcputs(TS_ARCH);
			len = TD_ARCH;
		}
		Xputterm(END_STANDOUT);
		cputstr(len, arch);
		Xfree(arch);

		Xlocate(TC_MARK, TL_PATH);
		VOID_C XXcprintf("%<*d", TD_MARK, mark);
		Xputterm(T_STANDOUT);
		XXcputs(TS_MARK);
		Xputterm(END_STANDOUT);
		VOID_C XXcprintf("%<'*qd", TD_SIZE, marksize);

		Xtflush();
		return;
	}
#endif	/* !_NOTRADLAYOUT */

	Xlocate(0, L_PATH);
	Xputterm(L_CLEAR);

	Xlocate(C_PATH, L_PATH);
	Xputterm(T_STANDOUT);
#ifndef	_NOBROWSE
	if (browselist) {
		XXcputs(S_BROWSE);
		len = D_BROWSE;
	}
	else
#endif
	{
		XXcputs(S_ARCH);
		len = D_ARCH;
	}
	Xputterm(END_STANDOUT);
	cputstr(len, arch);
	Xfree(arch);

	Xtflush();
}

static VOID NEAR Xwaitmes(VOID_A)
{
	int x, y;

	x = win_x;
	y = win_y;
	win_x = win_y = -1;
	waitmes();
	win_x = x;
	win_y = y;
}

static VOID NEAR unpackerror(VOID_A)
{
	if (!isttyiomode) {
		kanjifputs(UNPNG_K, Xstderr);
		VOID_C fputnl(Xstderr);
	}
	else {
		Xlocate(0, n_line - 1);
		Xcputnl();
		hideclock = 1;
		warning(0, UNPNG_K);
	}
}

static char *archfgets(vp)
VOID_P vp;
{
	return(Xfgets((XFILE *)vp));
}

static int NEAR readarchive(file, list, flags)
CONST char *file;
lsparse_t *list;
int flags;
{
	XFILE *fp;
	int n, wastty;

	wastty = isttyiomode;
	if (!(fp = popenmacro(list -> comm, file, flags))) return(-1);
	if (wastty) Xwaitmes();
	maxarcf = lsparse(fp, list, &arcflist, archfgets);
	n = pclose2(fp);

	if (maxarcf < 0 || n) {
		if (maxarcf == -3 || n > 0) unpackerror();
		freelist(arcflist, maxarcf);
		maxarcf = 0;
		return(-1);
	}
	if (maxarcf <= 1) {
		maxarcf = 0;
		Xfree(arcflist[0].name);
		arcflist[0].name = (char *)NOFIL_K;
#ifndef	NOSYMLINK
		Xfree(arcflist[0].linkname);
		arcflist[0].linkname = NULL;
#endif
		arcflist[0].st_nlink = -1;
		arcflist[0].flags = 0;
		arcflist[0].tmpflags = F_STAT;
	}

	return(0);
}

static CONST char *NEAR skiparchdir(file)
CONST char *file;
{
	int len;

	if (!*archivedir) len = 0;
	else if ((len = dirmatchlen(archivedir, file))) file += len;
	else return(NULL);

	if (*file && len > 0 && (len > 1 || *archivedir != _SC_)) file++;

	return(file);
}

static int NEAR getarchdirlen(file)
CONST char *file;
{
	CONST char *tmp;
	int len;

	if (!(tmp = strdelim(file, 0))) len = strlen(file);
	else {
		len = (tmp == file) ? 1 : tmp - file;
		while (*(++tmp) == _SC_) /*EMPTY*/;
	}

	if (len == 2 && file[0] == '.' && file[1] == '.') len = 0;
	if (tmp && *tmp) return(-1 - len);

	return(len);
}

VOID copyarcf(re, arcre)
CONST reg_t *re;
CONST char *arcre;
{
	CONST char *cp;
	int i, j, n, len, parent;

	parent = (*archivedir) ? 0 : -1;
	/* omit arcflist[0] as pseudo ".." */
	for (i = 1; i < maxarcf; i++) {
		if (!(cp = skiparchdir(arcflist[i].name))) continue;
		if (!*cp) {
			parent = i;
			continue;
		}

		len = getarchdirlen(cp);
		if (len >= -1 && len <= 0) {
			if (parent <= 0) parent = i;
		}
		if (len < 0) continue;
		if (re && !regexp_exec(re, cp, 1)) continue;

		if (arcre) {
			if (!(n = searcharc(arcre, arcflist, maxarcf, i)))
				continue;
			if (n < 0) break;
		}

		for (j = 0; j < stackdepth; j++)
			if (!strcmp(cp, filestack[j].name)) break;
		if (j < stackdepth) continue;

		filelist = addlist(filelist, maxfile, &maxent);
		memcpy((char *)&(filelist[maxfile]),
			(char *)&(arcflist[i]), sizeof(namelist));
		filelist[maxfile].name = (char *)cp;
#ifndef	NOSYMLINK
		filelist[maxfile].linkname = arcflist[i].linkname;
#endif
		maxfile++;
	}

	if (parent < 0) return;
	for (i = 0; i < maxfile; i++)
		if (isdotdir(filelist[i].name) == 1) return;
	filelist = addlist(filelist, maxfile, &maxent);
	memmove((char *)&(filelist[1]), (char *)&(filelist[0]),
		maxfile++ * sizeof(namelist));
	memcpy((char *)&(filelist[0]),
		(char *)&(arcflist[parent]), sizeof(namelist));
	filelist[0].name = arcflist[0].name;
#ifndef	NOSYMLINK
	filelist[0].linkname = NULL;
#endif
}

static char *NEAR searcharcdir(file, flen)
CONST char *file;
int flen;
{
	CONST char *cp;
	int i, len;

	errno = ENOENT;
	/* omit arcflist[0] as pseudo ".." */
	for (i = 1; i < maxarcf; i++) {
		if (!(cp = skiparchdir(arcflist[i].name))) continue;
		if (!file) return(arcflist[(*cp) ? 0 : i].name);
		if (!*cp) continue;

		len = getarchdirlen(cp);
		if (len >= -1 && len <= 0) {
			if (*file) continue;
			return(arcflist[0].name);
		}
		if (len < 0) continue;
		if (len != flen || strnpathcmp(file, cp, flen)) continue;

		if (!isdir(&(arcflist[i]))) {
			errno = ENOTDIR;
			continue;
		}

		return(arcflist[i].name);
	}

	return(NULL);
}

static char *NEAR archoutdir(VOID_A)
{
	char *cp, *file;

	if (!*archivedir) return((char *)-1);
	if (!(file = searcharcdir(NULL, 0))) return(NULL);

	cp = &(file[(int)strlen(file) - 1]);
	while (cp > file && *cp == _SC_) cp--;
#ifdef	BSPATHDELIM
	if (onkanji1(file, cp - file)) cp++;
#endif
	cp = strrdelim2(file, cp);
	if (!cp) *archivedir = '\0';
	else {
		if (cp == file) copyrootpath(archivedir);
		else Xstrncpy(archivedir, file, cp - file);
		file = ++cp;
	}

	return(file);
}

CONST char *archchdir(path)
CONST char *path;
{
#ifndef	_NOBROWSE
	int i, n, flags, dupfilepos;
#endif
	CONST char *cp, *file;
	char *tmp, duparcdir[MAXPATHLEN];
	int len;

	Xfree(findpattern);
	findpattern = NULL;
#ifndef	_NOBROWSE
	if (browselist) {
		if (!path || !*path) path = parentpath;
		if ((cp = strdelim(path, 0))) {
			for (i = 1; cp[i]; i++) if (cp[i] != _SC_) break;
			if (cp[i]) {
				errno = ENOENT;
				return(NULL);
			}
			Xstrncpy(duparcdir, path, (cp - path));
			path = duparcdir;
		}

		n = browselevel;
		cp = browselist[n].ext;
		flags = (F_NOCONFIRM | F_ARGSET | F_NOADDOPT | F_IGNORELIST);
		dupfilepos = filepos;

		if (path == filelist[filepos].name) i = filepos;
		else for (i = 0; i < maxfile; i++)
			if (!strcmp(path, filelist[i].name)) break;
		if (i < maxfile && isdir(&(filelist[i]))) filepos = i;
		else if (!isdotdir(path)) {
			errno = ENOENT;
			return(NULL);
		}

		pushbrowsevar(path);
		if (cp && !(browselist[n].flags & LF_DIRNOPREP))
			execusercomm(cp, path, flags);
		if (isdotdir(path)) {
			filepos = dupfilepos;
			popbrowsevar();
			return((path[1]) ? (char *)-1 : path);
		}

		if (browselist[n + 1].comm
		&& !(browselist[n].flags & LF_DIRLOOP))
			n++;
		if (dolaunch(&(browselist[n]), flags) < 0
		|| !isarchbr(&(browselist[n]))) {
			popbrowsevar();
			return(path);
		}
		browselevel = n;
		return(parentpath);
	}
#endif	/* !_NOBROWSE */

	if (!path) return(archoutdir());
	Xstrcpy(duparcdir, archivedir);
	file = nullstr;
	do {
		if (*path == _SC_) len = 1;
		else if ((cp = strdelim(path, 0))) len = cp - path;
		else len = strlen(path);

		cp = path;
		if (*cp != '.') /*EMPTY*/;
		else if (len == 1) cp = NULL;
		else if (len == 2 && cp[1] == '.') cp = nullstr;

		if (!cp) /*EMPTY*/;
		else if (searcharcdir(cp, len)) {
			if (*(tmp = archivedir)) tmp = strcatdelim(archivedir);
			Xstrncpy(tmp, path, len);
			file = parentpath;
		}
		else if (*cp || !(file = archoutdir())) {
			Xstrcpy(archivedir, duparcdir);
			errno = ENOENT;
			return(NULL);
		}
		else if (file == (char *)-1) break;

		path += len;
		while (*path == _SC_) path++;
	} while (*path);

	return(file);
}

#ifndef	_NOCOMPLETE
int completearch(path, flen, argc, argvp)
CONST char *path;
int flen, argc;
char ***argvp;
{
	CONST char *cp, *file;
	char *new, dir[MAXPATHLEN], duparcdir[MAXPATHLEN];
	int i, len, parent;

# ifdef	DEP_DOSPATH
	if (_dospath(path)) return(argc);
# endif

	Xstrcpy(duparcdir, archivedir);
	if (!(file = strrdelim(path, 0))) file = path;
# ifndef	_NOBROWSE
	else if (browselist) return(argc);
# endif
	else {
		Xstrncpy(dir, path, (file == path) ? 1 : file - path);
		if (!(cp = archchdir(dir)) || cp == (char *)-1) {
			Xstrcpy(archivedir, duparcdir);
			return(argc);
		}
		flen -= ++file - path;
	}

	/* omit arcflist[0] as pseudo ".." */
	for (i = 1; i < maxarcf; i++) {
		if (!(cp = skiparchdir(arcflist[i].name))) continue;
		if (!*cp) continue;

		parent = 0;
		len = getarchdirlen(cp);
		if (len >= -1 && len <= 0) {
			len = 2;
			parent++;
		}
		if (len < 0) continue;
		if (len != flen || strnpathcmp(file, cp, flen)) continue;

		new = Xmalloc(len + 1 + 1);
		Xstrncpy(new, cp, len);
		if (parent || isdir(&(arcflist[i]))) new[len++] = _SC_;
		new[len] = '\0';
		argc = addcompletion(NULL, new, argc, argvp);
	}
	Xstrcpy(archivedir, duparcdir);

	return(argc);
}
#endif	/* !_NOCOMPLETE */

int dolaunch(list, flags)
lsparse_t *list;
int flags;
{
#ifdef	DEP_PSEUDOPATH
	int drive;
#endif
	CONST char *cp;
	char *tmpdir;

	tmpdir = NULL;
#ifdef	DEP_PSEUDOPATH
	drive = 0;
#endif

	if (flags & F_ARGSET) /*EMPTY*/;
	else if (archivefile) {
		if (!(tmpdir = tmpunpack(1))) {
			Xputterm(T_BELL);
			return(-1);
		}
	}
#ifdef	DEP_PSEUDOPATH
	else if ((drive = tmpdosdupl(nullstr, &tmpdir, 1)) < 0) {
		Xputterm(T_BELL);
		return(-1);
	}
#endif

	if (!isarchbr(list)) {
		ptyusercomm(list -> comm, filelist[filepos].name, flags);
		if (flags & F_ARGSET) /*EMPTY*/;
#ifdef	DEP_PSEUDOPATH
		else if (drive) removetmp(tmpdir, filelist[filepos].name);
#endif
		else if (archivefile) removetmp(tmpdir, NULL);
		return(1);
	}

	pusharchdupl();
	if (flags & F_ARGSET) /*EMPTY*/;
#ifdef	DEP_PSEUDOPATH
	else if (drive) {
		archduplp -> v_fullpath = Xstrdup(fullpath);
		Xstrcpy(fullpath, tmpdir);
	}
#endif
	else if (archivefile) {
		archduplp -> v_fullpath = Xstrdup(fullpath);
		Xstrcpy(fullpath, tmpdir);
		if (*archivedir) {
			Xstrcpy(strcatdelim(fullpath), archivedir);
			archduplp -> v_archivedir = Xstrdup(archivedir);
		}
	}
	cp = (filelist && filepos < maxfile) ? filelist[filepos].name : NULL;
	if (!cp) cp = nullstr;
	if (archivefile) maxfile = 0;
	*archivedir = '\0';
	archivefile = Xstrdup(cp);
	archtmpdir = tmpdir;
	launchp = list;
	arcflist = NULL;
	maxarcf = 0;
#ifdef	DEP_PSEUDOPATH
	archdrive = drive;
#endif
#ifndef	_NOTREE
	treepath = NULL;
#endif
	findpattern = NULL;
	while (maxfile > 0) Xfree(filelist[--maxfile].name);
#if	FD >= 2
	if (sorttype < 200) sorton = 0;
#else
	sorton = 0;
#endif

	if (readarchive(archivefile, launchp, flags) < 0) {
		arcflist = NULL;
		poparchdupl();
		return(-1);
	}
	maxfile = 0;
	filepos = -1;

	return(1);
}

int launcher(VOID_A)
{
#ifndef	_NOBROWSE
	CONST char *cp;
	int flags;
#endif
	reg_t *re;
	int i, n;

#ifndef	_NOBROWSE
	if (browselist) {
		n = browselevel;
		cp = browselist[n].ext;
		flags = (F_NOCONFIRM | F_ARGSET | F_NOADDOPT | F_IGNORELIST);

		pushbrowsevar(filelist[filepos].name);
		if (cp && !(browselist[n].flags & LF_FILENOPREP))
			execusercomm(cp, filelist[filepos].name, flags);

		if (browselist[n + 1].comm
		&& !(browselist[n].flags & LF_FILELOOP))
			n++;
		if (dolaunch(&(browselist[n]), flags) < 0
		|| !isarchbr(&(browselist[n]))) {
			popbrowsevar();
			return(-1);
		}
		browselevel = n;
		return(1);
	}
#endif	/* !_NOBROWSE */

	for (i = 0; i < maxlaunch; i++) {
#ifndef	PATHNOCASE
		if (launchlist[i].flags & LF_IGNORECASE) pathignorecase++;
#endif
		re = regexp_init(launchlist[i].ext, -1);
		n = regexp_exec(re, filelist[filepos].name, 0);
		regexp_free(re);
#ifndef	PATHNOCASE
		if (launchlist[i].flags & LF_IGNORECASE) pathignorecase--;
#endif
		if (n) break;
	}
	if (i >= maxlaunch) return(0);

	return(dolaunch(&(launchlist[i]), F_NOCONFIRM | F_IGNORELIST));
}

static int NEAR undertmp(path)
CONST char *path;
{
	winvartable *wvp;
	char *full, rpath[MAXPATHLEN], dir[MAXPATHLEN];

	full = fullpath;
	Xrealpath(path, rpath, RLP_READLINK);
	for (wvp = archduplp; wvp; wvp = wvp -> v_archduplp) {
		if (!(wvp -> v_fullpath)) continue;
		Xrealpath(full, dir, RLP_READLINK);
		if (underpath(rpath, dir, -1)) return(1);
		full = wvp -> v_fullpath;
	}

	return(0);
}

#ifdef	DEP_PSEUDOPATH
static char *NEAR genfullpath(path, file, full, tmpdir)
char *path;
CONST char *file, *full, *tmpdir;
#else
static char *NEAR genfullpath(path, file, full)
char *path;
CONST char *file, *full;
#endif
{
#if	MSDOS
	char *tmp;
	int drive;
#endif
	CONST char *cp;

	cp = file;
#if	MSDOS
	if ((drive = _dospath(cp))) cp += 2;
#endif
#ifdef	DEP_DOSEMU
	if (_dospath(cp)) cp += 2;
#endif

#ifdef	DEP_PSEUDOPATH
	if (tmpdir) Xstrcpy(path, tmpdir);
	else
#endif
	if (*cp == _SC_) {
		Xstrcpy(path, file);
		return(path);
	}
	else if (!full || !*full) {
		errno = ENOENT;
		return(NULL);
	}
#if	MSDOS
	else if (drive) {
		tmp = gendospath(path, drive, _SC_);
		drive = Xtoupper(drive);
		if (drive == Xtoupper(*full)) Xstrcpy(tmp, &(full[3]));
		else if (!unixgetcurdir(tmp, drive - 'A' + 1)) return(NULL);
	}
#endif
	else Xstrcpy(path, full);
	Xstrcpy(strcatdelim(path), cp);

	return(path);
}

#ifdef	DEP_PSEUDOPATH
static int NEAR archdostmpdir(path, dirp, full)
char *path, **dirp;
CONST char *full;
{
	char *cp, dupfullpath[MAXPATHLEN];
	int drive;

	Xstrcpy(dupfullpath, fullpath);
	if (full != fullpath) {
		Xstrcpy(fullpath, full);
		if (_chdir2(fullpath) < 0) full = NULL;
	}
# ifdef	DEP_DOSDRIVE
	if ((drive = dospath2(path))) /*EMPTY*/;
	else
# endif
# ifdef	DEP_URLPATH
	if (urlpath(path, NULL, NULL, &drive)) drive += '0';
	else
# endif
	drive = 0;
	Xstrcpy(fullpath, dupfullpath);
	if (full != fullpath && _chdir2(fullpath) < 0) {
		lostcwd(fullpath);
		return(-1);
	}

	if (!drive) return(0);
	cp = path;
# ifdef	DEP_DOSPATH
	if (_dospath(cp)) cp += 2;
# endif
	if (full) Xstrcpy(fullpath, full);
	else if (*cp != _SC_) return(-1);
	Xrealpath(path, path, RLP_READLINK);
	Xstrcpy(fullpath, dupfullpath);

	if (!(cp = dostmpdir(drive))) return(-1);
	*dirp = cp;

	return(1);
}
#endif	/* DEP_PSEUDOPATH */

int pack(arc)
CONST char *arc;
{
#ifdef	DEP_PSEUDOPATH
	char *dest, *tmpdest;
	int drive;
#endif
	winvartable *wvp;
	reg_t *re;
	char *tmpdir, *full, *duparchivefile, path[MAXPATHLEN];
	int i, n, ret;

	for (i = 0; i < maxarchive; i++) {
		if (!archivelist[i].p_comm) continue;
#ifndef	PATHNOCASE
		if (archivelist[i].flags & AF_IGNORECASE) pathignorecase++;
#endif
		re = regexp_init(archivelist[i].ext, -1);
		n = regexp_exec(re, arc, 0);
		regexp_free(re);
#ifndef	PATHNOCASE
		if (archivelist[i].flags & AF_IGNORECASE) pathignorecase--;
#endif
		if (n) break;
	}
	if (i >= maxarchive) return(-1);

	full = fullpath;
	for (wvp = archduplp; wvp; wvp = wvp -> v_archduplp)
		if (wvp -> v_fullpath) full = wvp -> v_fullpath;

	ret = 1;
	tmpdir = NULL;
#ifdef	DEP_PSEUDOPATH
	tmpdest = NULL;
	drive = 0;

	Xstrcpy(path, arc);
	if ((dest = strrdelim(path, 1))) *(++dest) = '\0';
	else copycurpath(path);
	if ((n = archdostmpdir(path, &tmpdest, full)) < 0) {
		warning(ENOENT, arc);
		return(0);
	}
	if (n) dest = Xstrdup(path);
#endif	/* DEP_PSEUDOPATH */

	if (archivefile) {
		if (!(tmpdir = tmpunpack(0))) {
#ifdef	DEP_PSEUDOPATH
			Xfree(dest);
#endif
			return(0);
		}
	}
#ifdef	DEP_PSEUDOPATH
	else if ((drive = tmpdosdupl(nullstr, &tmpdir, 0)) < 0) {
		Xfree(dest);
		return(0);
	}
#endif

#ifdef	DEP_PSEUDOPATH
	if (!genfullpath(path, arc, full, tmpdest)) warning(-1, arc);
#else
	if (!genfullpath(path, arc, full)) warning(-1, arc);
#endif
	else {
		duparchivefile = archivefile;
		archivefile = NULL;
		Xwaitmes();
		ret = execusercomm(archivelist[i].p_comm, path,
			F_ARGSET | F_ISARCH | F_NOADDOPT);
		if (ret > 0) {
			if (ret < 127) {
				hideclock = 1;
				warning(0, HITKY_K);
			}
			rewritefile(1);
		}
		archivefile = duparchivefile;
	}
#ifdef	DEP_PSEUDOPATH
	if (tmpdest) {
		if (!ret) {
			if (_chdir2(tmpdest) < 0) ret = 1;
			else VOID_C forcemovefile(dest);
		}
		Xfree(dest);
		removetmp(tmpdest, NULL);
	}
	if (drive) removetmp(tmpdir, NULL);
	else
#endif	/* DEP_PSEUDOPATH */
	if (archivefile) removetmp(tmpdir, NULL);

	return((ret) ? 0 : 1);
}

/*ARGSUSED*/
int unpack(arc, dir, arg, tr, flags)
CONST char *arc, *dir, *arg;
int tr, flags;
{
#ifdef	DEP_PSEUDOPATH
	winvartable *wvp;
	namelist alist[1], *dupfilelist;
	char *full, *dest, *tmpdest;
	int drive, dupmaxfile, dupfilepos;
#endif
	reg_t *re;
	char *cp, *new, *tmpdir, path[MAXPATHLEN];
	int i, n, ret;

#ifndef	_NOBROWSE
	if (browselist) {
		warning(0, UNPNG_K);
		return(0);
	}
#endif
	for (i = 0; i < maxarchive; i++) {
		if (!archivelist[i].u_comm) continue;
#ifndef	PATHNOCASE
		if (archivelist[i].flags & AF_IGNORECASE) pathignorecase++;
#endif
		re = regexp_init(archivelist[i].ext, -1);
		n = regexp_exec(re, arc, 0);
		regexp_free(re);
#ifndef	PATHNOCASE
		if (archivelist[i].flags & AF_IGNORECASE) pathignorecase--;
#endif
		if (n) break;
	}
	if (i >= maxarchive) return(-1);

	if (dir) Xstrcpy(path, dir);
	else for (;;) {
#ifndef	_NOTREE
		if (tr) dir = new = tree(0, NULL);
		else
#endif	/* !_NOTREE */
		{
			if (arg && *arg) new = Xstrdup(arg);
			else new = inputstr(UNPAC_K, 1, -1, NULL, HST_PATH);
			dir = new = evalpath(new, 0);
		}
		if (!dir) return(0);
		if (!*dir) copycurpath(path);
		else {
			cp = Xstrcpy(path, dir);
#ifdef	DEP_DOSPATH
			if (_dospath(dir) && !dir[2]) copycurpath(cp);
#endif
		}
		Xfree(new);
		dir = NULL;

		if (!undertmp(path)) break;
		Xlocate(0, L_CMDLINE);
		Xputterm(L_CLEAR);
		VOID_C XXcprintf("[%.*s]", n_column - 2, path);
		if (yesno(UNPTM_K)) break;
	}

	ret = 1;
	cp = path;
	tmpdir = NULL;
#ifdef	DEP_PSEUDOPATH
	tmpdest = NULL;

	full = fullpath;
	for (wvp = archduplp; wvp; wvp = wvp -> v_archduplp)
		if (wvp -> v_fullpath) full = wvp -> v_fullpath;

	if ((n = archdostmpdir(path, &tmpdest, full)) < 0) {
		warning(ENOENT, path);
		return(0);
	}
	if (n) {
		dest = Xstrdup(path);
		cp = tmpdest;
	}
# ifdef	FAKEUNINIT
	else dest = NULL;	/* fake for -Wuninitialized */
# endif

	dupfilelist = filelist;
	dupmaxfile = maxfile;
	dupfilepos = filepos;
	alist[0].name = (char *)arc;
# ifndef	NOSYMLINK
	alist[0].linkname = NULL;
# endif
	alist[0].st_mode = 0700;
	filelist = alist;
	maxfile = 1;
	filepos = 0;
	drive = tmpdosdupl(arc, &tmpdir, 0);
	filelist = dupfilelist;
	maxfile = dupmaxfile;
	filepos = dupfilepos;

	if (drive < 0) warning(-1, arc);
	else
#endif	/* DEP_PSEUDOPATH */
	if ((!dir && preparedir(path) < 0) || _chdir2(cp) < 0)
		warning(-1, path);
#ifdef	DEP_PSEUDOPATH
	else if (!genfullpath(path, arc, fullpath, tmpdir)) warning(-1, arc);
#else
	else if (!genfullpath(path, arc, fullpath)) warning(-1, arc);
#endif
	else {
		Xwaitmes();
		ret = execusercomm(archivelist[i].u_comm, path, flags);
		if (ret > 0) {
			if (ret < 127) {
				hideclock = 1;
				warning(0, HITKY_K);
			}
			rewritefile(1);
		}
	}

#ifdef	DEP_PSEUDOPATH
	if (tmpdest) {
		if (!ret) VOID_C forcemovefile(dest);
		Xfree(dest);
		removetmp(tmpdest, NULL);
	}
	if (tmpdir) removetmp(tmpdir, arc);
	else
#endif	/* DEP_PSEUDOPATH */
	if (_chdir2(fullpath) < 0) lostcwd(fullpath);

	return((ret) ? 0 : 1);
}

#ifndef	NOSYMLINK
static int NEAR archrealpath(path, resolved)
char *path, *resolved;
{
	char *cp, tmp[MAXPATHLEN];
	int n;

	if (!*path || (n = isdotdir(path)) == 2) /*EMPTY*/;
	else if (n == 1) {
		cp = strrdelim(resolved, 0);
		if (!cp) return(-1);
		*cp = '\0';
	}
	else if ((cp = strdelim(path, 0))) {
		Xstrncpy(tmp, path, cp - path);
		if (archrealpath(tmp, resolved) < 0) return(-1);
		if (archrealpath(++cp, resolved) < 0) return(-1);
	}
	else {
		cp = strcatdelim(resolved);
		Xstrncpy(cp, path, MAXPATHLEN - 1 - (cp - resolved));
	}

	return(0);
}

static int NEAR unpacklink(list, dir)
namelist *list;
CONST char *dir;
{
	namelist duplist;
	char *cp, path[MAXPATHLEN], duparcdir[MAXPATHLEN];
	int i, ret;

	if (!(list -> linkname)) return(0);

	if (!islink(list)) cp = list -> linkname;
	else {
		strcatdelim2(duparcdir, archivedir, list -> linkname);
		cp = duparcdir;
	}
	*path = '\0';
	if (archrealpath(cp, path) < 0) return(-1);

	for (i = 0; i < maxarcf; i++) {
		*duparcdir = '\0';
		if (archrealpath(arcflist[i].name, duparcdir) < 0) continue;
		if (!strcmp(path, duparcdir)) break;
	}
	if (i >= maxarcf) return(-1);

	if (unpacklink(&(arcflist[i]), dir) < 0) return(-1);

	Xstrcpy(duparcdir, archivedir);
	memcpy(&duplist, &(filelist[filepos]), sizeof(namelist));
	memcpy(&(filelist[filepos]), &(arcflist[i]), sizeof(namelist));
	if (!(cp = strrdelim(filelist[filepos].name, 0))) *archivedir = '\0';
	else {
		Xstrncpy(archivedir,
			filelist[filepos].name, cp - filelist[filepos].name);
		filelist[filepos].name = ++cp;
	}
	ret = unpack(archivefile, dir, NULL,
		0, F_ARGSET | F_ISARCH | F_NOADDOPT);
	memcpy(&(filelist[filepos]), &duplist, sizeof(namelist));
	Xstrcpy(archivedir, duparcdir);

	return(ret);
}
#endif	/* !NOSYMLINK */

char *tmpunpack(single)
int single;
{
#ifdef	DEP_DOSEMU
	char tmp[MAXPATHLEN];
#endif
	winvartable *wvp;
	char *subdir, *tmpdir, path[MAXPATHLEN];
	int i, ret, dupmark;

	for (i = 0, wvp = archduplp; wvp; i++, wvp = wvp -> v_archduplp)
		/*EMPTY*/;
	Xstrcpy(path, ARCHTMPPREFIX);
	if (mktmpdir(path) < 0) {
		warning(-1, path);
		return(NULL);
	}
	tmpdir = Xstrdup(path);

	for (i = 0; i < maxfile; i++)
		if (ismark(&(filelist[i]))) filelist[i].tmpflags |= F_WSMRK;
	dupmark = mark;
	if (single) {
		mark = 0;
#ifndef	NOSYMLINK
		VOID_C unpacklink(&(filelist[filepos]), path);
#endif
	}
	ret = unpack(archivefile, path, NULL,
		0, F_ARGSET | F_ISARCH | F_NOADDOPT);
	mark = dupmark;
	for (i = 0; i < maxfile; i++) {
		if (wasmark(&(filelist[i]))) filelist[i].tmpflags |= F_ISMRK;
		filelist[i].tmpflags &= ~F_WSMRK;
	}

	subdir = archivedir;
	while (*subdir == _SC_) subdir++;

	if (ret < 0) Xputterm(T_BELL);
	else if (!ret) /*EMPTY*/;
	else if (_chdir2(path) < 0) {
		hideclock = 1;
		warning(-1, path);
	}
	else if (*subdir && _chdir2(subdir) < 0) {
		hideclock = 1;
		warning(-1, subdir);
	}
	else if (single || mark <= 0) {
		if (Xaccess(fnodospath(tmp, filepos), F_OK) >= 0)
			return(tmpdir);
		hideclock = 1;
		warning(-1, filelist[filepos].name);
	}
	else {
		for (i = 0; i < maxfile; i++) {
			if (!ismark(&(filelist[i]))) continue;
			if (Xaccess(fnodospath(tmp, i), F_OK) < 0) {
				hideclock = 1;
				warning(-1, filelist[i].name);
				break;
			}
		}
		if (i >= maxfile) return(tmpdir);
	}
	removetmp(tmpdir, NULL);

	return(NULL);
}

int backup(dev)
CONST char *dev;
{
	macrostat st;
	char *tmp;
	int i;

	waitmes();
	if (!filelist) n_args = 0;
	else {
		for (i = 0; i < maxfile; i++) {
			if (ismark(&(filelist[i])))
				filelist[i].tmpflags |= F_ISARG;
			else filelist[i].tmpflags &= ~F_ISARG;
		}
		n_args = mark;
	}

	st.flags = (F_NOCONFIRM | F_ISARCH);
	if (!(tmp = evalcommand("tar cf %C %TA", dev, &st))) return(0);

#ifdef	_NOEXTRAMACRO
	if (filelist) for (;;) {
		system2(tmp, st.flags);
		Xfree(tmp);

		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand("tar rf %C %TA", dev, NULL)))
			break;
	}
	else
#endif
	{
		system2(tmp, st.flags);
		Xfree(tmp);
	}

	if (filelist) {
		for (i = 0; i < maxfile; i++)
			filelist[i].tmpflags &= ~(F_ISARG | F_ISMRK);
		mark = 0;
		marksize = (off_t)0;
	}

	return(1);
}

int searcharc(regstr, flist, maxf, n)
CONST char *regstr;
namelist *flist;
int maxf, n;
{
	namelist tmp;
	lsparse_t *list;
	namelist *dupfilelist;
	reg_t *re;
	XFILE *fp;
	char *cp, *next, *tmpdir, *file;
	int i, c, no, match, dupmaxfile, dupfilepos;

	if (n < 0) {
		file = flist -> name;
		if (isdir(flist)) return(0);
	}
	else {
		file = flist[n].name;
		if (isdir(&(flist[n]))) return(0);
	}

	for (i = 0; i < maxlaunch; i++) {
		if (!isarchbr(&(launchlist[i]))) continue;
#ifndef	PATHNOCASE
		if (launchlist[i].flags & LF_IGNORECASE) pathignorecase++;
#endif
		re = regexp_init(launchlist[i].ext, -1);
		c = regexp_exec(re, file, 0);
		regexp_free(re);
#ifndef	PATHNOCASE
		if (launchlist[i].flags & LF_IGNORECASE) pathignorecase--;
#endif
		if (c) break;
	}
	if (i >= maxlaunch) return(0);

	list = &(launchlist[i]);
	if (n >= 0) {
		dupfilelist = filelist;
		dupmaxfile = maxfile;
		dupfilepos = filepos;
		filelist = flist;
		maxfile = maxf;
		filepos = n;
		tmpdir = tmpunpack(1);
		filelist = dupfilelist;
		maxfile = dupmaxfile;
		filepos = dupfilepos;
		if (!tmpdir) return(0);
	}
#ifdef	FAKEUNINIT
	else tmpdir = NULL;	/* fake for -Wuninitialized */
#endif

	Xlocate(0, n_line - 1);
	Xtflush();
	if (!(fp = popenmacro(list -> comm, file, F_IGNORELIST))) {
		if (n >= 0) removetmp(tmpdir, file);
		return(0);
	}

	Xlocate(0, L_MESLINE);
	Xputterm(L_CLEAR);
	Xputterm(T_STANDOUT);
	VOID_C Xkanjiputs(SEACH_K);
	Xputterm(END_STANDOUT);
	VOID_C Xkanjiputs(file);
	Xtflush();

	for (i = 0; i < (int)(list -> topskip); i++) {
		if (!(cp = Xfgets(fp))) break;
		Xfree(cp);
	}

	no = match = 0;
	re = regexp_init(regstr, -1);
	parsefilelist(NULL, list, NULL, NULL, NULL);
	for (;;) {
		if (match && match <= no - (int)(list -> bottomskip)) break;
		i = parsefilelist(fp, list, &tmp, &no, archfgets);
		if (i < 0) {
			if (i == -1) break;
			VOID_C pclose2(fp);
			parsefilelist(NULL, NULL, NULL, NULL, NULL);
			regexp_free(re);
			if (n >= 0) removetmp(tmpdir, file);
			return(-1);
		}
		if (!i) continue;

		cp = tmp.name;
		while (cp) {
			if ((next = strdelim(cp, 0))) *(next++) = '\0';
			if (regexp_exec(re, cp, 1)) break;
			cp = next;
		}
		if (cp) match = no;
		Xfree(tmp.name);
#ifndef	NOSYMLINK
		Xfree(tmp.linkname);
#endif
	}
	parsefilelist(NULL, NULL, NULL, NULL, NULL);
	regexp_free(re);
	if (n >= 0) removetmp(tmpdir, file);

	if ((i = pclose2(fp))) return((i > 0) ? -1 : 0);
	if (match > no - (int)(list -> bottomskip)) match = 0;

	return(match);
}
#endif	/* !_NOARCHIVE */
