/*
 *	archive.c
 *
 *	Archive File Access Module
 */

#include "fd.h"
#include "func.h"
#include "kanji.h"

#ifndef	_NOARCHIVE

#ifndef	_NOORIGSHELL
#include "system.h"
#endif

#if	MSDOS
extern char *unixgetcurdir __P_((char *, int));
#endif

extern int mark;
extern int stackdepth;
#ifndef	_NOTRADLAYOUT
extern int tradlayout;
#endif
extern int sizeinfo;
extern off_t marksize;
extern namelist filestack[];
extern char fullpath[];
extern char typesymlist[];
extern u_short typelist[];
#ifndef	_NOCOLOR
extern int ansicolor;
#endif
extern int win_x;
extern int win_y;
extern char *deftmpdir;
extern int hideclock;
extern u_short today[];
extern int n_args;

#if	FD >= 2
static char *form_lha[] = {
#if	MSDOS
	"%*f\\n%s %x %x %y-%m-%d %t %a",	/* MS-DOS (-v) */
	"%1x %12f %s %x %x %y-%m-%d %t %a",	/* MS-DOS (-l) */
#else
	"%a %u/%g %s %x %m %d %{yt} %*f",	/* >=1.14 */
	"%9a %u/%g %s %x %m %d %{yt} %*f",	/* traditional */
#endif
	NULL
};
static char *ign_lha[] = {
#if	MSDOS
	"Listing of archive : *",
	"  Name          Original *",
	"--------------*",
# if	defined (FAKEMETA) || !defined (BSPATHDELIM)
	"* files * ???.?% ?\077-?\077-?? ??:??:??",	/* avoid trigraph */
# else
	"* files * ???.?%% ?\077-?\077-?? ??:??:??",	/* avoid trigraph */
# endif
	"",
#else	/* !MSDOS */
	" PERMSSN * UID*GID *",
	"----------*",
	" Total * file* ???.*%*",
#endif	/* !MSDOS */
	NULL
};
static char *form_tar[] = {
#if	MSDOS
	"%a %u/%g %s %m %d %t %y %*f",		/* traditional */
	"%a %u/%g %s %y-%m-%d %t %*f",		/* GNU >=1.12 */
	"%a %u/%g %s %m %d %y %t %*f",		/* kmtar */
#else	/* !MSDOS */
	"%a %u/%g %s %m %d %t %y %*f",		/* SVR4 */
	"%a %u/%g %s %y-%m-%d %t %*f",		/* GNU >=1.12 */
	"%a %l %u %g %s %m %d %{yt} %*f",	/* pax */
	"%10a %u/%g %s %m %d %t %y %*f",	/* tar (UXP/DS) */
	"%9a %u/%g %s %m %d %t %y %*f",		/* traditional */
	"%a %u/%g %m %d %t %y %*f",		/* IRIX */
#endif	/* !MSDOS */
	NULL
};
#define	PM_LHA	form_lha, ign_lha, 0, 0
#define	PM_TAR	form_tar, NULL, 0, 0
#define	PM_NULL	NULL, NULL, 0, 0
#define	LINESEP	'\n'
#define	MAXSCORE	256
#define	isarchbr(l)	((l) -> format)
#else	/* FD < 2 */
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
# ifdef	UXPDS
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 10, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{10, 255, 255}, 1
# else	/* !UXPDS */
#  ifdef	TARFROMPAX
#define	PM_TAR	0, 0, \
		{0, 2, 3, 4, 7, 5, 6, 7, 8}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#  else	/* !TARFROMPAX */
#   ifdef	TARUSESPACE
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{0, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#   else	/* !TARUSESPACE */
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{9, 255, 255}, 1
#   endif	/* !TARUSESPACE */
#  endif	/* !TARFROMPAX */
# endif	/* !UXPDS */
#endif	/* !MSDOS */
#define	PM_NULL	255, 0, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#define	LINESEP	'\t'
#define	isarchbr(l)	(((l) -> topskip) < (u_char)255)
#endif	/* FD < 2 */

#define	iswhitespace(c)	((c) == ' ' || (c) == '\t')
#define	S_IREAD_ALL	(S_IRUSR | S_IRGRP | S_IROTH)
#define	S_IWRITE_ALL	(S_IWUSR | S_IWGRP | S_IWOTH)
#define	S_IEXEC_ALL	(S_IXUSR | S_IXGRP | S_IXOTH)

#ifndef	_NOBROWSE
static VOID NEAR copyargvar __P_((int, char **));
static VOID NEAR pushbrowsevar __P_((char *));
static VOID NEAR popbrowsevar __P_((VOID_A));
#endif
static VOID NEAR pusharchdupl __P_((VOID_A));
#ifdef	_NOBROWSE
#define	poparchdupl	escapearch
#else
static VOID NEAR poparchdupl __P_((VOID_A));
#endif
static int NEAR readattr __P_((namelist *, char *));
#if	FD >= 2
static char *NEAR checkspace __P_((char *, int *));
# ifdef	_NOKANJIFCONV
#define	readfname	strndup2
# else
static char *NEAR readfname __P_((char *, int));
# endif
static int NEAR readfileent __P_((namelist *, char *, char *, int));
#else	/* FD < 2 */
static int NEAR countfield __P_((char *, u_char [], int, int *));
static char *NEAR getfield __P_((char *, char *, int, launchtable *, int));
static int NEAR readfileent __P_((namelist *, char *, launchtable *, int));
#endif	/* FD < 2 */
static int NEAR dircmp __P_((char *, char *));
static int NEAR dirmatchlen __P_((char *, char *));
static char *NEAR pseudodir __P_((namelist *));
static VOID NEAR Xwaitmes __P_((VOID_A));
#if	FD >= 2
static char **NEAR decodevar __P_((char **));
static int NEAR matchlist __P_((char *, char **));
#endif
static int NEAR parsearchive __P_((FILE *, launchtable *, namelist *, int *));
static VOID NEAR unpackerror __P_((VOID_A));
static int NEAR readarchive __P_((char *, launchtable *, int));
static char *NEAR searcharcdir __P_((char *, int));
static char *NEAR archoutdir __P_((VOID_A));
static int NEAR undertmp __P_((char *));
#ifdef	_NODOSDRIVE
static char *NEAR genfullpath __P_((char *, char *, char *));
#else
static char *NEAR genfullpath __P_((char *, char *, char *, char *));
static int NEAR archdostmpdir __P_((char *, char **, char *));
#endif

int maxlaunch = 0;
int maxarchive = 0;
launchtable launchlist[MAXLAUNCHTABLE] = {
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
	{"*.taZ",	"zcat %C|tar tvf -",	PM_TAR, 0},
	{"*.taz",	"gzip -cd %C|tar tvf -",	PM_TAR, 0},
	{"*.tgz",	"gzip -cd %C|tar tvf -",	PM_TAR, 0},
#endif	/* !MSDOS */
	{NULL,		NULL,			PM_NULL, 0}
};
archivetable archivelist[MAXARCHIVETABLE] = {
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
	{"*.taZ",	"tar cf - %T|compress -c > %C",
					"zcat %C|tar xf - %TA", 0},
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

#if	FD >= 2
static char *autoformat[] = {
# if	MSDOS
	"%*f\n%s %x %x %y-%m-%d %t %a",		/* LHa (-v) */
	"%a %u/%g %s %m %d %t %y %*f",		/* tar (traditional) */
	"%a %u/%g %s %y-%m-%d %t %*f",		/* tar (GNU >=1.12) */
	"%a %l %u %g %s %m %d %{yt} %*f",	/* ls or pax */
	"%a %u/%g %s %m %d %y %t %*f",		/* tar (kmtar) */
	" %s %y-%m-%d %t %*f",			/* zip */
	" %s %x %x %x %y-%m-%d %t %*f",		/* pkunzip */
	" %s %x %x %d %m %y %t %*f",		/* zoo */
	"%1x %12f %s %x %x %y-%m-%d %t %a",	/* LHa (-l) */
# else	/* !MSDOS */
	"%a %u/%g %s %m %d %t %y %*f",		/* tar (SVR4) */
	"%a %u/%g %s %y-%m-%d %t %*f",		/* tar (GNU >=1.12) */
	"%a %l %u %g %s %m %d %{yt} %*f",	/* ls or pax */
	"%a %u/%g %s %x %m %d %{yt} %*f",	/* LHa (1.14<=) */
	" %s %m-%d-%y %t %*f",			/* zip */
	" %s %x %x %d %m %y %t %*f",		/* zoo */
	"%10a %u/%g %s %m %d %t %y %*f",	/* tar (UXP/DS) */
	"%9a %u/%g %s %m %d %t %y %*f",		/* tar (traditional) */
	"%9a %u/%g %s %x %m %d %{yt} %*f",	/* LHa */
	"%a %u/%g %m %d %t %y %*f",		/* tar (IRIX) */
# endif	/* !MSDOS */
};
#define	AUTOFORMATSIZ	((int)(sizeof(autoformat) / sizeof(char *)))
#endif	/* FD >= 2 */


#ifndef	_NOBROWSE
static VOID NEAR copyargvar(argc, argv)
int argc;
char **argv;
{
	int i;

	if (argvar && argvar[0]) for (i = 1; argvar[i]; i++) free(argvar[i]);
	argvar = (char **)realloc2(argvar, (argc + 2) * sizeof(char *));
	for (i = 0; i < argc; i++) argvar[i + 1] = strdup2(argv[i]);
	argvar[i + 1] = NULL;
}

static VOID NEAR pushbrowsevar(s)
char *s;
{
	int i;

	i = countvar(browsevar);
	browsevar = (char **)realloc2(browsevar, (i + 2) * sizeof(char *));
	if (!i) browsevar[1] = NULL;
	else memmove((char *)&(browsevar[1]), (char *)&(browsevar[0]),
		(i + 1) * sizeof(char *));
	browsevar[0] = strdup2(s);
	copyargvar(i + 1, browsevar);
}

static VOID NEAR popbrowsevar(VOID_A)
{
	int i;

	i = countvar(browsevar);
	if (i <= 0) return;
	free(browsevar[0]);
	memmove((char *)&(browsevar[0]), (char *)&(browsevar[1]),
		i * sizeof(char *));
	copyargvar(i - 1, browsevar);
}
#endif	/* !_NOBROWSE */

static VOID NEAR pusharchdupl(VOID_A)
{
	winvartable *new;

	new = (winvartable *)malloc2(sizeof(winvartable));

	new -> v_archduplp = archduplp;
	new -> v_archivedir = NULL;
	new -> v_archivefile = archivefile;
	new -> v_archtmpdir = archtmpdir;
	new -> v_launchp = launchp;
	new -> v_arcflist = arcflist;
	new -> v_maxarcf = maxarcf;
#ifndef	_NODOSDRIVE
	new -> v_archdrive = archdrive;
#endif
#ifndef	_NOBROWSE
	new -> v_browselist = browselist;
	new -> v_browselevel = browselevel;
#endif
#ifndef	_NOTREE
	new -> v_treepath = treepath;
#endif
	new -> v_fullpath = NULL;
	new -> v_findpattern = findpattern;
	new -> v_filelist = filelist;
	new -> v_maxfile = maxfile;
	new -> v_maxent = maxent;
	new -> v_filepos = filepos;
	new -> v_sorton = sorton;
	archduplp = new;
}

VOID escapearch(VOID_A)
#ifndef	_NOBROWSE
{
	if (!archduplp) return;
	if (browselist) popbrowsevar();
	poparchdupl();
}

static VOID NEAR poparchdupl(VOID_A)
#endif	/* !_NOBROWSE */
{
#ifndef	_NODOSDRIVE
	int drive;
#endif
	winvartable *old;
	char *cp, *dir;
	int i;

	if (!(old = archduplp)) return;

	cp = archivefile;
	dir = archtmpdir;
#ifndef	_NODOSDRIVE
	drive = archdrive;
#endif
	if (arcflist) {
		for (i = 0; i < maxarcf; i++) {
			free(arcflist[i].name);
#ifndef	NOSYMLINK
			if (arcflist[i].linkname) free(arcflist[i].linkname);
#endif
		}
		free(arcflist);
	}

	archduplp = old -> v_archduplp;
	if (!(old -> v_archivedir)) *archivedir = '\0';
	else {
		strcpy(archivedir, old -> v_archivedir);
		free(old -> v_archivedir);
	}
	archivefile = old -> v_archivefile;
	archtmpdir = old -> v_archtmpdir;
	launchp = old -> v_launchp;
	arcflist = old -> v_arcflist;
	maxarcf = old -> v_maxarcf;
#ifndef	_NODOSDRIVE
	archdrive = old -> v_archdrive;
#endif
#ifndef	_NOBROWSE
	browselevel = old -> v_browselevel;
#endif
#ifndef	_NOTREE
	treepath = old -> v_treepath;
#endif
	if (findpattern) free(findpattern);
	findpattern = old -> v_findpattern;
	filepos = old -> v_filepos;
	sorton = old -> v_sorton;

	free(filelist);
	filelist = old -> v_filelist;
	maxfile = old -> v_maxfile;
	maxent = old -> v_maxent;

#ifndef	_NOBROWSE
	if (browselist) {
		if (!(old -> v_browselist)) {
			freebrowse(browselist);
			browselist = NULL;
		}
	}
	else
#endif
#ifndef	_NODOSDRIVE
	if (drive > 0) {
		strcpy(fullpath, old -> v_fullpath);
		free(old -> v_fullpath);
		removetmp(dir, cp);
	}
	else
#endif
	if (archivefile) {
		strcpy(fullpath, old -> v_fullpath);
		free(old -> v_fullpath);
		removetmp(dir, NULL);
	}

	free(cp);
	free(old);
}

static int NEAR readattr(tmp, buf)
namelist *tmp;
char *buf;
{
	int i, c, len;
	u_int mode;

	len = strlen(buf);
	if (len < 9) {
		mode = (tmp -> st_mode & S_IFMT) | S_IREAD_ALL | S_IWRITE_ALL;
		while (--len >= 0) {
			c = tolower2(buf[len]);
			for (i = 0; typesymlist[i]; i++)
				if (c == typesymlist[i]) break;
			if (typesymlist[i]) {
				mode &= ~S_IFMT;
				mode |= typelist[i];
			}
			else switch (c) {
				case 'a':
					mode |= S_ISVTX;
					break;
				case 'h':
					mode &= ~S_IREAD_ALL;
					break;
				case 'r':
				case 'o':
					mode &= ~S_IWRITE_ALL;
					break;
				case 'w':
				case '-':
				case ' ':
					break;
				case 'v':
					mode &= ~S_IFMT;
					mode |= S_IFIFO;
					break;
				default:
					return(0);
/*NOTREACHED*/
					break;
			}
		}
		if ((mode & S_IFMT) == S_IFDIR) mode |= S_IEXEC_ALL;
	}
	else {
		mode = tmp -> st_mode & S_IFMT;

		i = len - 9;
		if (buf[i] == 'r' || buf[i] == 'R') mode |= S_IRUSR;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'w' || buf[i] == 'W') mode |= S_IWUSR;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'x' || buf[i] == 'X') mode |= S_IXUSR;
		else if (buf[i] == 's') mode |= (S_IXUSR | S_ISUID);
		else if (buf[i] == 'S') mode |= S_ISUID;
		else if (buf[i] != '-') return(0);

		i++;
		if (buf[i] == 'r' || buf[i] == 'R') mode |= S_IRGRP;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'w' || buf[i] == 'W') mode |= S_IWGRP;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'x' || buf[i] == 'X') mode |= S_IXGRP;
		else if (buf[i] == 's') mode |= (S_IXGRP | S_ISGID);
		else if (buf[i] == 'S') mode |= S_ISGID;
		else if (buf[i] != '-') return(0);

		i++;
		if (buf[i] == 'r' || buf[i] == 'R') mode |= S_IROTH;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'w' || buf[i] == 'W') mode |= S_IWOTH;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'x' || buf[i] == 'X') mode |= S_IXOTH;
		else if (buf[i] == 't') mode |= (S_IXOTH | S_ISVTX);
		else if (buf[i] == 'T') mode |= S_ISVTX;
		else if (buf[i] != '-') return(0);

		if (len >= 10 && buf[len - 10] != '-') {
			c = tolower2(buf[len - 10]);
			for (i = 0; typesymlist[i]; i++)
				if (c == typesymlist[i]) break;
			if (!typesymlist[i]) return(0);
			mode &= ~S_IFMT;
			mode |= typelist[i];
		}
	}

	tmp -> st_mode = mode;
	return(len);
}

#if	FD >= 2
static char *NEAR checkspace(s, scorep)
char *s;
int *scorep;
{
	int i, len;

	if (iswhitespace(*s)) {
		(*scorep)++;
		for (s++; iswhitespace(*s); s++);
	}
	for (i = 0; s[len = i]; i++) {
		if (iswhitespace(s[i])) {
			(*scorep)++;
			for (i++; iswhitespace(s[i]); i++);
			if (!s[i]) break;
			*scorep += 4;
		}
	}
	return(strndup2(s, len));
}

# ifndef	_NOKANJIFCONV
static char *NEAR readfname(s, len)
char *s;
int len;
{
	char *cp, *tmp;

	cp = strndup2(s, len);
	tmp = newkanjiconv(cp, fnamekcode, DEFCODE, L_FNAME);
	if (tmp != cp) free(cp);
	return(tmp);
}
# endif	/* !_NOKANJIFCONV */

static int NEAR readfileent(tmp, line, form, skip)
namelist *tmp;
char *line, *form;
int skip;
{
# ifndef	NOUID
	uidtable *up;
	gidtable *gp;
	uid_t uid;
	gid_t gid;
# endif
	struct tm tm;
	off_t n;
	int i, ch, l, len, hit, err, err2, score;
	char *cp, *s, *buf, *rawbuf;

	if (skip && skip > strlen(form)) return(-1);

	tmp -> name = NULL;
	tmp -> st_mode = 0644;
	tmp -> st_nlink = 1;
# ifndef	NOUID
	tmp -> st_uid = (uid_t)-1;
	tmp -> st_gid = (gid_t)-1;
	tmp -> linkname = NULL;
# endif
	tmp -> st_size = (off_t)0;
	tmp -> flags = 0;
	tmp -> tmpflags = F_STAT;
	tm.tm_year = tm.tm_mon = tm.tm_mday = -1;
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	score = 0;

	while (*form) if (iswhitespace(*form)) {
		while (iswhitespace(*line)) line++;
		form++;
	}
	else if (*form == '\n') {
		if (*line != *(form++)) score += 20;
		else line++;
		while (iswhitespace(*line)) line++;
	}
	else if (*form != '%' || *(++form) == '%') {
		ch = *(form++);
		if (skip) {
			skip--;
			continue;
		}
		if (*line != ch) score += 10;
		else line++;
	}
	else {
		ch = '\0';
		if (*form == '*') {
			form++;
			for (i = 0; line[i]; i++) if (line[i] == '\n') break;
			len = i;
		}
		else if (!(cp = sscanf2(form, "%+d", &len))) len = -1;
		else {
			form = cp;
			for (i = 0; i < len; i++) if (!line[i]) break;
			len = i;
			ch = -1;
		}

		if (*form != '{') {
			l = 1;
			s = form++;
		}
		else {
			s = ++form;
			form = strchr2(s, '}');
			if (!form || form <= s) {
				if (tmp -> name) free(tmp -> name);
# ifndef	NOSYMLINK
				if (tmp -> linkname) free(tmp -> linkname);
# endif
				return(-1);
			}
			l = form++ - s;
		}

		if (skip) {
			skip--;
			continue;
		}

		if (len < 0) {
			if (!iswhitespace(*form)
			&& (*form != '%' || *(form + 1) == '%')) ch = *form;
			for (len = 0; line[len]; len++) {
				if (ch) {
					if (ch == line[len]) break;
				}
				else if (iswhitespace(line[len])) break;
			}
		}

		rawbuf = strndup2(line, len);
		hit = err = err2 = 0;
		buf = checkspace(rawbuf, &err);
		while (l--) switch (s[l]) {
			case 'a':
				if (readattr(tmp, buf)) hit++;
				break;
			case 'l':
				if ((i = atoi2(buf)) < 0) break;
				tmp -> st_nlink = i;
				hit++;
				break;
			case 'u':
# ifndef	NOUID
				cp = sscanf2(buf, "%-*d%$",
					sizeof(uid_t), &uid);
				if (cp) tmp -> st_uid = uid;
				else if ((up = finduid(0, buf)))
					tmp -> st_uid = up -> uid;
# endif
				hit++;
				break;
			case 'g':
# ifndef	NOUID
				cp = sscanf2(buf, "%-*d%$",
					sizeof(gid_t), &gid);
				if (cp) tmp -> st_gid = gid;
				else if ((gp = findgid(0, buf)))
					tmp -> st_gid = gp -> gid;
# endif
				hit++;
				break;
			case 's':
				if (!(cp = sscanf2(buf, "%qd", &n))) break;
				if (*cp) err2++;
				tmp -> st_size = n;
				hit++;
				break;
			case 'y':
				if ((i = atoi2(buf)) < 0) break;
				tm.tm_year = i;
				hit++;
				break;
			case 'm':
				if (!strncmp(buf, "Jan", 3)) tm.tm_mon = 0;
				else if (!strncmp(buf, "Feb", 3))
					tm.tm_mon = 1;
				else if (!strncmp(buf, "Mar", 3))
					tm.tm_mon = 2;
				else if (!strncmp(buf, "Apr", 3))
					tm.tm_mon = 3;
				else if (!strncmp(buf, "May", 3))
					tm.tm_mon = 4;
				else if (!strncmp(buf, "Jun", 3))
					tm.tm_mon = 5;
				else if (!strncmp(buf, "Jul", 3))
					tm.tm_mon = 6;
				else if (!strncmp(buf, "Aug", 3))
					tm.tm_mon = 7;
				else if (!strncmp(buf, "Sep", 3))
					tm.tm_mon = 8;
				else if (!strncmp(buf, "Oct", 3))
					tm.tm_mon = 9;
				else if (!strncmp(buf, "Nov", 3))
					tm.tm_mon = 10;
				else if (!strncmp(buf, "Dec", 3))
					tm.tm_mon = 11;
				else if ((i = atoi2(buf)) >= 1 && i <= 12)
					tm.tm_mon = i - 1;
				else break;
				hit++;
				break;
			case 'd':
				if ((i = atoi2(buf)) < 1 || i > 31) break;
				tm.tm_mday = i;
				hit++;
				break;
			case 't':
				if (!(cp = sscanf2(buf, "%d", &i)) || i > 23)
					break;
				tm.tm_hour = i;
				hit++;
				if (!(cp = sscanf2(cp, ":%d", &i)) || i > 59) {
					tm.tm_min = tm.tm_sec = 0;
					err2++;
					break;
				}
				tm.tm_min = i;
				if (!sscanf2(cp, ":%d%$", &i) || i > 59) {
					tm.tm_sec = 0;
					break;
				}
				tm.tm_sec = i;
				break;
			case 'f':
				for (i = 0; i < len; i++) {
# if	MSDOS
#  ifdef	BSPATHDELIM
					if (rawbuf[i] == '/') rawbuf[i] = _SC_;
#  else
					if (rawbuf[i] == '\\')
						rawbuf[i] = _SC_;
					if (iskanji1(rawbuf, i)) {
						i++;
						continue;
					}
#  endif
# endif	/* MSDOS */
					if (!iswhitespace(rawbuf[i])) continue;
					cp = &(rawbuf[i]);
					for (cp++; iswhitespace(*cp); cp++);
					if (cp >= &(rawbuf[len])) {
						if (!ch) i = len;
						break;
					}
# ifndef	NOSYMLINK
					if (cp[0] == '-' && cp[1] == '>')
						break;
# endif
				}
				cp = &(line[i]);
				if (isdelim(rawbuf, i - 1)) {
					tmp -> st_mode &= ~S_IFMT;
					tmp -> st_mode |= S_IFDIR;
					if (i > 1) i--;
				}
				if (tmp -> name) free(tmp -> name);
				tmp -> name = readfname(rawbuf, i);
				hit++;
				err = 0;
# ifndef	NOSYMLINK
				while (iswhitespace(*cp)) cp++;
				if (ch && cp >= &(line[len])) break;
				if (cp[0] != '-' || cp[1] != '>') break;
				for (cp += 2; iswhitespace(*cp); cp++);
				for (i = 0; cp[i]; i++)
					if (iswhitespace(cp[i])) break;
				if (i) {
					if (tmp -> linkname)
						free(tmp -> linkname);
					tmp -> linkname = readfname(cp, i);
					i = &(cp[i]) - line;
					if (i > len) len = i;
				}
# endif
				break;
			case 'x':
				hit++;
				err = 0;
				break;
			default:
				hit = -1;
				break;
		}

		free(buf);
		free(rawbuf);

		if (hit < 0) {
			if (tmp -> name) free(tmp -> name);
# ifndef	NOSYMLINK
			if (tmp -> linkname) free(tmp -> linkname);
# endif
			return(hit);
		}

		if (!hit) score += 5;
		else if (hit <= err2) score += err2;
		score += err;
		line += len;
		if (!ch) while (iswhitespace(*line)) line++;
	}

	if (score >= MAXSCORE || !(tmp -> name) || !*(tmp -> name)) {
		if (tmp -> name) free(tmp -> name);
# ifndef	NOSYMLINK
		if (tmp -> linkname) free(tmp -> linkname);
# endif
		return(MAXSCORE);
	}

	if (!(tmp -> st_mode & S_IFMT)) tmp -> st_mode |= S_IFREG;
	if (s_isdir(tmp)) tmp -> flags |= F_ISDIR;
	else if (s_islnk(tmp)) tmp -> flags |= F_ISLNK;
	if (tm.tm_year < 0) {
		tm.tm_year = today[0];
		if (tm.tm_mon < 0 || tm.tm_mday < 0) tm.tm_year = 1970 - 1900;
		else if (tm.tm_mon > today[1]
		|| (tm.tm_mon == today[1] && tm.tm_mday > today[2]))
			tm.tm_year--;
	}
	else {
		if (tm.tm_year < 1900 && (tm.tm_year += 1900) < 1970)
			tm.tm_year += 100;
		tm.tm_year -= 1900;
	}
	if (tm.tm_mon < 0) tm.tm_mon = 0;
	if (tm.tm_mday < 0) tm.tm_mday = 1;

	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags |=
# ifdef	NOUID
		logical_access(tmp -> st_mode);
# else
		logical_access(tmp -> st_mode, tmp -> st_uid, tmp -> st_gid);
# endif

	return(score);
}

#else	/* FD < 2 */

static int NEAR countfield(line, sep, field, eolp)
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
				if (!iswhitespace(line[i])) break;
			if (f < 0) f = 1;
			else f++;
			s = 0;
		}
		else if (iswhitespace(line[i])) s = 1;
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
					|| iswhitespace(line[j])) break;
				}
				*eolp = j;
			}
			return(i);
		}
	}
	return((field < 0) ? f : -1);
}

static char *NEAR getfield(buf, line, skip, list, no)
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

	strncpy2(buf, cp, i);
	return(buf);
}

static int NEAR readfileent(tmp, line, list, max)
namelist *tmp;
char *line;
launchtable *list;
int max;
{
# ifndef	NOUID
	uidtable *up;
	gidtable *gp;
	uid_t uid;
	gid_t gid;
# endif
	struct tm tm;
	off_t n;
	int i, skip;
	char *cp, *buf;

	i = countfield(line, list -> sep, -1, NULL);
	skip = (max > i) ? max - i : 0;
	buf = malloc2(strlen(line) + 1);

	tmp -> flags = 0;
	tmp -> tmpflags = F_STAT;
	tmp -> st_mode = 0;
	tmp -> st_nlink = 1;
# ifndef	NOSYMLINK
	tmp -> linkname = NULL;
# endif
	getfield(buf, line, skip, list, F_NAME);
	if (!*buf) {
		free(buf);
		return(-1);
	}
# if	MSDOS
#  ifdef	BSPATHDELIM
	for (i = 0; buf[i]; i++) if (buf[i] == '/') buf[i] = _SC_;
#  else
	for (i = 0; buf[i]; i++) {
		if (buf[i] == '\\') buf[i] = _SC_;
		if (iskanji1(buf, i)) i++;
	}
#  endif
# endif	/* MSDOS */
	if (isdelim(buf, i = (int)strlen(buf) - 1)) {
		if (i > 0) buf[i] = '\0';
		tmp -> st_mode |= S_IFDIR;
	}
	tmp -> name = strdup2(buf);

	getfield(buf, line, skip, list, F_MODE);
	readattr(tmp, buf);
	if (!(tmp -> st_mode & S_IFMT)) tmp -> st_mode |= S_IFREG;
	if (s_isdir(tmp)) tmp -> flags |= F_ISDIR;
	else if (s_islnk(tmp)) tmp -> flags |= F_ISLNK;

# ifndef	NOUID
	getfield(buf, line, skip, list, F_UID);
	if (sscanf2(buf, "%-*d%$", sizeof(uid_t), &uid)) tmp -> st_uid = uid;
	else tmp -> st_uid = ((up = finduid(0, buf))) ? up -> uid : (uid_t)-1;
	getfield(buf, line, skip, list, F_GID);
	if (sscanf2(buf, "%-*d%$", sizeof(gid_t), &gid)) tmp -> st_gid = gid;
	else tmp -> st_gid = ((gp = findgid(0, buf))) ? gp -> gid : (gid_t)-1;
# endif
	getfield(buf, line, skip, list, F_SIZE);
	tmp -> st_size = (sscanf2(buf, "%qd%$", &n)) ? n : (off_t)0;

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
	else if ((i = atoi2(buf)) >= 1 && i < 12) tm.tm_mon = i - 1;
	else tm.tm_mon = 0;
	getfield(buf, line, skip, list, F_DAY);
	tm.tm_mday = ((i = atoi2(buf)) >= 1 && i < 31) ? i : 1;
	getfield(buf, line, skip, list, F_YEAR);
	if (!*buf) tm.tm_year = 1970;
	else if ((i = atoi2(buf)) >= 0) tm.tm_year = i;
	else if (list -> field[F_YEAR] == list -> field[F_TIME]
	&& strchr(buf, ':')) {
		tm.tm_year = today[0];
		if (tm.tm_mon > today[1]
		|| (tm.tm_mon == today[1] && tm.tm_mday > today[2]))
			tm.tm_year--;
	}
	else tm.tm_year = 1970;
	if (tm.tm_year < 1900 && (tm.tm_year += 1900) < 1970)
		tm.tm_year += 100;
	tm.tm_year -= 1900;

	getfield(buf, line, skip, list, F_TIME);
	if (!(cp = sscanf2(buf, "%d", &i)) || i > 23)
		tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	else {
		tm.tm_hour = i;
		if (!(cp = sscanf2(cp, ":%d", &i)) || i > 59)
			tm.tm_min = tm.tm_sec = 0;
		else {
			tm.tm_min = i;
			if (!sscanf2(cp, ":%d%$", &i) || i > 59) tm.tm_sec = 0;
			else tm.tm_sec = i;
		}
	}

	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags |=
# ifdef	NOUID
		logical_access(tmp -> st_mode);
# else
		logical_access(tmp -> st_mode, tmp -> st_uid, tmp -> st_gid);
# endif

	free(buf);
	return(0);
}
#endif	/* FD < 2 */

VOID archbar(file, dir)
char *file, *dir;
{
	char *arch;
	int len;

#ifndef	_NOBROWSE
	if (browselist) {
		int i;

		if (!browsevar) arch = strdup2("");
		else {
			len = 0;
			for (i = 0; browsevar[i]; i++)
				len += strlen(browsevar[i]) + 1;
			arch = malloc2(len + 1);
			len = 0;
			for (i--; i >= 0; i--) {
				strcpy(&(arch[len]), browsevar[i]);
				len += strlen(browsevar[i]);
				if (i > 0) arch[len++] = '>';
			}
			arch[len] = '\0';
		}
	}
	else
#endif	/* !_NOBROWSE */
	{
		arch = malloc2(strlen(fullpath)
			+ strlen(file) + strlen(dir) + 3);
		strcpy(strcpy2(strcatdelim2(arch, fullpath, file), ":"), dir);
	}

#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		locate(0, TL_PATH);
		putterm(l_clear);

		locate(TC_PATH, TL_PATH);
		putterm(t_standout);
# ifndef	_NOBROWSE
		if (browselist) {
			cputs2(TS_BROWSE);
			len = TD_BROWSE;
		}
		else
# endif
		{
			cputs2(TS_ARCH);
			len = TD_ARCH;
		}
		putterm(end_standout);
		cprintf2("%-*.*k", len, len, arch);
		free(arch);

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
#ifndef	_NOBROWSE
	if (browselist) {
		cputs2(S_BROWSE);
		len = D_BROWSE;
	}
	else
#endif
	{
		cputs2(S_ARCH);
		len = D_ARCH;
	}
	putterm(end_standout);
	cprintf2("%-*.*k", len, len, arch);
	free(arch);

	tflush();
}

static int NEAR dircmp(s1, s2)
char *s1, *s2;
{
	int i, j;

	for (i = j = 0; s2[j]; i++, j++) {
		if (s1[i] == s2[j]) {
#ifdef	BSPATHDELIM
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

static int NEAR dirmatchlen(s1, s2)
char *s1, *s2;
{
	int i, j;

	for (i = j = 0; s1[i]; i++, j++) {
		if (s1[i] == s2[j]) {
#ifdef	BSPATHDELIM
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

static char *NEAR pseudodir(namep)
namelist *namep;
{
	char *cp, *next;
	int i, len;
	u_short ent;

	for (cp = namep -> name; (next = strdelim(cp, 0)); cp = ++next) {
		while (*(next + 1) == _SC_) next++;
		if (!*(next + 1)) break;
		len = next - namep -> name;
		if (!len) len++;
		for (i = 0; i < maxfile; i++) {
			if (isdir(&(filelist[i]))
			&& len == dirmatchlen(filelist[i].name, namep -> name))
				break;
		}
		if (i >= maxfile) return(strndup2(namep -> name, len));
		if (strncmp(filelist[i].name, namep -> name, len)) {
			filelist[i].name = realloc2(filelist[i].name, len + 1);
			strncpy2(filelist[i].name, namep -> name, len);
		}
	}

	if (isdir(namep) && !isdotdir(namep -> name))
	for (i = 0; i < maxfile; i++) {
		if (isdir(&(filelist[i]))
		&& !dircmp(filelist[i].name, namep -> name)) {
			cp = filelist[i].name;
			ent = filelist[i].ent;
			memcpy((char *)&(filelist[i]), (char *)namep,
				sizeof(namelist));
			filelist[i].name = cp;
			filelist[i].ent = ent;
			return(NULL);
		}
	}

	return(namep -> name);
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

#if	FD >= 2
static char **NEAR decodevar(argv)
char **argv;
{
	char **new;
	int n, max;

	max = countvar(argv);
	new = (char **)malloc2((max + 1) * sizeof(char *));
	for (n = 0; n < max; n++) new[n] = decodestr(argv[n], NULL, 0);
	new[n] = NULL;

	return(new);
}

static int NEAR matchlist(s, argv)
char *s, **argv;
{
# ifndef	PATHNOCASE
	int duppathignorecase;
# endif
	reg_t *re;
	char *s1, *s2;
	int i, len, ret;

	if (!argv) return(0);
# ifndef	PATHNOCASE
	duppathignorecase = pathignorecase;
	pathignorecase = 0;
# endif

	ret = 0;
	for (i = 0; !ret && argv[i]; i++) {
		s1 = argv[i];
		s2 = s;
		if (!iswhitespace(s1[0])) while (iswhitespace(*s2)) s2++;
		len = strlen(s1);
		if (!len || iswhitespace(s1[len - 1])) s2 = strdup2(s2);
		else {
			len = strlen(s2);
			for (len--; len >= 0; len--)
				if (!iswhitespace(s2[len])) break;
			s2 = strndup2(s2, ++len);
		}
		re = regexp_init(s1, -1);
		ret = regexp_exec(re, s2, 0);
		free(s2);
		regexp_free(re);
	}
# ifndef	PATHNOCASE
	pathignorecase = duppathignorecase;
# endif
	return(ret);
}
#endif	/* FD >= 2 */

static int NEAR parsearchive(fp, list, namep, linenop)
FILE *fp;
launchtable *list;
namelist *namep;
int *linenop;
{
#if	FD >= 2
	static char **formlist = NULL;
	static char **lign = NULL;
	static char **lerr = NULL;
	namelist tmp;
	char *form, *form0;
	short *scorelist;
	int nf, na, ret, skip;
#else
	int max;
#endif
	static char **lvar = NULL;
	char *cp;
	int i, score, len, nline, needline;

	if (!fp) {
		freevar(lvar);
		lvar = NULL;
#if	FD >= 2
		freevar(formlist);
		freevar(lign);
		freevar(lerr);
		if (!list || !(list -> format)) formlist = lign = lerr = NULL;
		else {
			formlist = decodevar(list -> format);
			lign = decodevar(list -> lignore);
			lerr = decodevar(list -> lerror);
		}
#endif
		return(0);
	}

#if	FD >= 2
	if (!formlist || !*formlist) return(-3);
#endif
	if (!(nline = countvar(lvar))) {
		lvar = (char **)realloc2(lvar, (nline + 2) * sizeof(char *));
		if (!(lvar[nline] = fgets2(fp, 0))) return(-1);
		lvar[++nline] = NULL;
	}

	if (isttyiomode && intrkey()) return(-2);
#if	FD >= 2
	if (matchlist(lvar[0], lign)) {
		free(lvar[0]);
		memmove((char *)(&(lvar[0])), (char *)(&(lvar[1])),
			nline * sizeof(char *));
		(*linenop)++;
		return(0);
	}
	if (matchlist(lvar[0], lerr)) return(-3);
#endif	/* FD >= 2 */

#if	FD >= 2
	nf = countvar(formlist);
	scorelist = (short *)malloc2(nf * sizeof(short));
	for (i = 0; i < nf; i++) scorelist[i] = MAXSCORE;
	nf = na = skip = 0;
	form0 = NULL;
	ret = 1;
#else	/* FD < 2 */
	max = 0;
	for (i = 0; i < MAXLAUNCHFIELD; i++)
		if (list -> field[i] < 255 && (int)(list -> field[i]) > max)
			max = (int)(list -> field[i]);
#endif	/* FD < 2 */
	namep -> name = NULL;
#ifndef	NOSYMLINK
	namep -> linkname = NULL;
#endif

	needline = 0;
	for (;;) {
#if	FD >= 2
		if (formlist[nf]) form = formlist[nf];
		else if (scorelist[0] < MAXSCORE) {
			ret = scorelist[0] + 1;
			score = 0;
			break;
		}
		else if (na < AUTOFORMATSIZ) form = autoformat[na++];
		else {
			if (!form0)
				form0 = decodestr(list -> format[0], NULL, 0);
			form = form0;
			skip++;
		}

		needline = 1;
		for (i = 0; form[i]; i++) if (form[i] == '\n') needline++;
#else
		needline = list -> lines;
#endif

		if (nline < needline) {
			lvar = (char **)realloc2(lvar,
				(needline + 1) * sizeof(char *));
			for (; nline < needline; nline++)
				if (!(lvar[nline] = fgets2(fp, 0))) break;
			lvar[nline] = NULL;
			if (nline < needline) {
#if	FD >= 2
				if (needline > 1 && na < AUTOFORMATSIZ) {
					if (!formlist[nf]) continue;
					free(formlist[nf]);
					for (i = nf; formlist[i]; i++)
						formlist[i] = formlist[i + 1];
					continue;
				}
				if (namep -> name) free(namep -> name);
				namep -> name = NULL;
# ifndef	NOSYMLINK
				if (namep -> linkname) free(namep -> linkname);
				namep -> linkname = NULL;
# endif
				if (form0) free(form0);
				free(scorelist);
#endif
				*linenop += nline;
				return(-1);
			}
		}

		for (i = len = 0; i < needline; i++)
			len += strlen(lvar[i]) + 1;
		cp = malloc2(len + 1);
		for (i = len = 0; i < needline; i++) {
			strcpy(&(cp[len]), lvar[i]);
			len += strlen(lvar[i]);
			cp[len++] = LINESEP;
		}
		if (len > 0) len--;
		cp[len] = '\0';

#if	FD >= 2
		score = readfileent(&tmp, cp, form, skip);
		free(cp);

		if (score < 0) {
			if (formlist[nf]) {
				free(formlist[nf]);
				for (i = nf; formlist[i]; i++)
					formlist[i] = formlist[i + 1];
			}
			else if (na >= AUTOFORMATSIZ) break;
			continue;
		}

		if (!formlist[nf]) {
			if (score < scorelist[0]) i = 0;
		}
		else {
			for (i = 0; i < nf; i++) if (score < scorelist[i]) {
				memmove((char *)(&(formlist[i + 1])),
					(char *)(&(formlist[i])),
					(nf - i) * sizeof(char *));
				memmove((char *)(&(scorelist[i + 1])),
					(char *)(&(scorelist[i])),
					(nf - i) * sizeof(short));
				break;
			}
			scorelist[i] = score;
			formlist[i] = form;
			nf++;
		}

		if (score >= MAXSCORE);
		else if (!i) {
			if (namep -> name) free(namep -> name);
# ifndef	NOSYMLINK
			if (namep -> linkname) free(namep -> linkname);
# endif
			memcpy((char *)namep, (char *)&tmp, sizeof(tmp));
		}
		else {
			if (tmp.name) free(tmp.name);
# ifndef	NOSYMLINK
			if (tmp.linkname) free(tmp.linkname);
# endif
		}

		if (!score) break;
#else	/* FD < 2 */
		score = readfileent(namep, cp, list, max);
		free(cp);
		break;
/*NOTREACHED*/
#endif	/* FD < 2 */
	}

#if	FD >= 2
	if (score < 0) {
		if (namep -> name) free(namep -> name);
		namep -> name = NULL;
# ifndef	NOSYMLINK
		if (namep -> linkname) free(namep -> linkname);
		namep -> linkname = NULL;
# endif
	}
#endif	/* FD >= 2 */
	if (lvar) {
		for (i = 0; i < needline; i++) free(lvar[i]);
		if (nline + 1 > needline)
			memmove((char *)(&(lvar[0])),
				(char *)(&(lvar[needline])),
				(nline - needline + 1) * sizeof(char *));
	}
	*linenop += needline;
#if	FD >= 2
	if (form0) free(form0);
	free(scorelist);
	return((score) ? 0 : ret);
#else
	return((score) ? 0 : 1);
#endif
}

static VOID NEAR unpackerror(VOID_A)
{
	if (!isttyiomode) {
		kanjifputs(UNPNG_K, stderr);
		fputnl(stderr);
	}
	else {
		locate(0, n_line - 1);
		cputnl();
		hideclock = 1;
		warning(0, UNPNG_K);
	}
}

static int NEAR readarchive(file, list, argset)
char *file;
launchtable *list;
int argset;
{
	namelist tmp;
	FILE *fp;
	char *cp, *dir;
	int i, no, max, wastty;

	wastty = isttyiomode;
	if (!(fp = popenmacro(list -> comm, file, argset))) return(-1);
	if (wastty) Xwaitmes();

	for (i = 0; i < (int)(list -> topskip); i++) {
		if (!(cp = fgets2(fp, 0))) break;
		free(cp);
	}

	maxfile = 0;
	addlist();
	filelist[0].name = strdup2("..");
#ifndef	NOSYMLINK
	filelist[0].linkname = NULL;
#endif
	filelist[0].flags = (F_ISEXE | F_ISRED | F_ISWRI | F_ISDIR);
	filelist[0].tmpflags = F_STAT;
	maxfile++;
#ifdef	HAVEFLAGS
	tmp.st_flags = (u_long)0;
#endif

	no = 0;
	max = -1;
	parsearchive(NULL, list, NULL, NULL);
	for (;;) {
		i = parsearchive(fp, list, &tmp, &no);
		if (i < 0) {
			if (i == -1) break;
			pclose2(fp);
			if (i == -3) unpackerror();
			parsearchive(NULL, NULL, NULL, NULL);
			for (i = 0; i < maxfile; i++) {
				free(filelist[i].name);
#ifndef	NOSYMLINK
				if (filelist[i].linkname)
					free(filelist[i].linkname);
#endif
			}
			return(-1);
		}
		if (!i) continue;
		else if (max < 0) max = i;
		else if (i > max) continue;

		while ((dir = pseudodir(&tmp)) && dir != tmp.name) {
			addlist();
			memcpy((char *)&(filelist[maxfile]), (char *)&tmp,
				sizeof(namelist));
			filelist[maxfile].st_mode &= ~S_IFMT;
			filelist[maxfile].st_mode |= S_IFDIR;
			filelist[maxfile].st_size = 0;
			filelist[maxfile].flags |= F_ISDIR;
			filelist[maxfile].name = dir;
#ifndef	NOSYMLINK
			filelist[maxfile].linkname = NULL;
#endif
			filelist[maxfile].ent = no;
			maxfile++;
		}
		if (!dir) {
			free(tmp.name);
#ifndef	NOSYMLINK
			if (tmp.linkname) free(tmp.linkname);
#endif
			continue;
		}

		addlist();
		memcpy((char *)&(filelist[maxfile]), (char *)&tmp,
			sizeof(namelist));
		filelist[maxfile].ent = no;
		maxfile++;
	}
	parsearchive(NULL, NULL, NULL, NULL);

	no -= (int)(list -> bottomskip);
	for (i = maxfile - 1; i > 0; i--) {
		if (filelist[i].ent <= no) break;
		free(filelist[i].name);
#ifndef	NOSYMLINK
		if (filelist[i].linkname) free(filelist[i].linkname);
#endif
	}
	maxfile = i + 1;
	for (i = 0; i < maxfile; i++) filelist[i].ent = i;
	if (maxfile <= 1) {
		maxfile = 0;
		free(filelist[0].name);
		filelist[0].name = NOFIL_K;
#ifndef	NOSYMLINK
		if (filelist[0].linkname) {
			free(filelist[0].linkname);
			filelist[0].linkname = NULL;
		}
#endif
		filelist[0].st_nlink = -1;
		filelist[0].flags = 0;
		filelist[0].tmpflags = F_STAT;
	}

	if ((i = pclose2(fp))) {
		if (i > 0) unpackerror();
		for (i = 0; i < maxfile; i++) {
			free(filelist[i].name);
#ifndef	NOSYMLINK
			if (filelist[i].linkname) free(filelist[i].linkname);
#endif
		}
		return(-1);
	}
	return(maxfile);
}

VOID copyarcf(re, arcre)
reg_t *re;
char *arcre;
{
	char *cp, *tmp;
	int i, j, n, len, parent;

	parent = (*archivedir) ? 0 : -1;
	/* omit filelist[0] as pseudo ".." */
	for (i = 1; i < maxarcf; i++) {
		if (!*archivedir) len = 0;
		else if (!(len = dirmatchlen(archivedir, arcflist[i].name)))
			continue;

		cp = &(arcflist[i].name[len]);
		if (*cp && len > 0 && (len > 1 || *archivedir != _SC_)) cp++;
		if (!*cp) {
			parent = i;
			continue;
		}

		if (!(tmp = strdelim(cp, 0))) len = strlen(cp);
		else {
			len = (tmp == cp) ? 1 : tmp - cp;
			while (*(++tmp) == _SC_);
		}

		if (parent <= 0 && len == 2 && cp[0] == '.' && cp[1] == '.')
			parent = i;
		if ((tmp && *tmp) || (re && !regexp_exec(re, cp, 1))) continue;
		if (arcre) {
			if (!(n = searcharc(arcre, arcflist, maxarcf, i)))
				continue;
			if (n < 0) break;
		}

		for (j = 0; j < stackdepth; j++)
			if (!strcmp(cp, filestack[j].name)) break;
		if (j < stackdepth) continue;

		memcpy((char *)&(filelist[maxfile]),
			(char *)&(arcflist[i]), sizeof(namelist));
		filelist[maxfile].name = cp;
#ifndef	NOSYMLINK
		filelist[maxfile].linkname = arcflist[i].linkname;
#endif
		maxfile++;
	}

	if (parent < 0) return;
	for (i = 0; i < maxfile; i++)
		if (!strcmp(filelist[i].name, "..")) return;
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
char *file;
int flen;
{
	char *cp, *tmp;
	int i, len;

	errno = ENOENT;
	for (i = 0; i < maxarcf; i++) {
		if (!*archivedir) len = 0;
		else if (!(len = dirmatchlen(archivedir, arcflist[i].name)))
			continue;

		cp = &(arcflist[i].name[len]);
		if (*cp && len > 0 && (len > 1 || *archivedir != _SC_)) cp++;
		if (!*cp) {
			if (file) continue;
		}
		else {
			if (!file) continue;
			if (!(tmp = strdelim(cp, 0))) len = strlen(cp);
			else {
				len = (tmp == cp) ? 1 : tmp - cp;
				while (*(++tmp) == _SC_);
			}
			if (!*file && len == 2 && cp[0] == '.' && cp[1] == '.')
				return(arcflist[0].name);
			if ((tmp && *tmp) || len != flen
			|| strnpathcmp(file, cp, flen))
				continue;
			if (!isdir(&(arcflist[i]))) {
				errno = ENOTDIR;
				continue;
			}
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
		if (cp == file) strcpy(archivedir, _SS_);
		else strncpy2(archivedir, file, cp - file);
		file = ++cp;
	}

	return(file);
}

char *archchdir(path)
char *path;
{
	char *cp, *file, duparcdir[MAXPATHLEN];
	int len;

	if (findpattern) free(findpattern);
	findpattern = NULL;
#ifndef	_NOBROWSE
	if (browselist) {
		int i, n, dupfilepos;

		if (!path || !*path) path = "..";
		if ((cp = strdelim(path, 0))) {
			for (i = 1; cp[i]; i++) if (cp[i] != _SC_) break;
			if (cp[i]) {
				errno = ENOENT;
				return(NULL);
			}
			strncpy2(duparcdir, path, (cp - path));
			path = duparcdir;
		}

		n = browselevel;
		cp = browselist[n].ext;
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
			execusercomm(cp, path, 1, 1, 1);
		if (isdotdir(path)) {
			filepos = dupfilepos;
			popbrowsevar();
			return((path[1]) ? (char *)-1 : path);
		}

		if (browselist[n + 1].comm
		&& !(browselist[n].flags & LF_DIRLOOP)) n++;
		if (dolaunch(&(browselist[n]), 1) < 0
		|| !isarchbr(&(browselist[n]))) {
			popbrowsevar();
			return(path);
		}
		browselevel = n;
		return("..");
	}
#endif	/* !_NOBROWSE */

	if (!path) return(archoutdir());
	strcpy(duparcdir, archivedir);
	do {
		if (*path == _SC_) len = 1;
		else if ((cp = strdelim(path, 0))) len = cp - path;
		else len = strlen(path);

		cp = path;
		if (len == 2 && path[0] == '.' && path[1] == '.') cp = "";
		if (searcharcdir(cp, len)) {
			if (*(cp = archivedir)) cp = strcatdelim(archivedir);
			strncpy2(cp, path, len);
			file = "..";
		}
		else if (*cp || !(file = archoutdir())) {
			strcpy(archivedir, duparcdir);
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
char *path;
int flen, argc;
char ***argvp;
{
	char *cp, *tmp, *new, *file, dir[MAXPATHLEN], duparcdir[MAXPATHLEN];
	int i, len, parent;

# ifdef	_USEDOSPATH
	if (_dospath(path)) return(argc);
# endif

	strcpy(duparcdir, archivedir);
	if (!(file = strrdelim(path, 0))) file = path;
# ifndef	_NOBROWSE
	else if (browselist) return(argc);
# endif
	else {
		strncpy2(dir, path, (file == path) ? 1 : file - path);
		if (!(cp = archchdir(dir)) || cp == (char *)-1) {
			strcpy(archivedir, duparcdir);
			return(argc);
		}
		flen -= ++file - path;
	}

	/* omit filelist[0] as pseudo ".." */
	for (i = 1; i < maxarcf; i++) {
		if (!*archivedir) len = 0;
		else if (!(len = dirmatchlen(archivedir, arcflist[i].name)))
			continue;

		cp = &(arcflist[i].name[len]);
		if (*cp && len > 0 && (len > 1 || *archivedir != _SC_)) cp++;
		if (!*cp) continue;

		if (!(tmp = strdelim(cp, 0))) len = strlen(cp);
		else {
			len = (tmp == cp) ? 1 : tmp - cp;
			while (*(++tmp) == _SC_);
		}

		parent = 0;
		if (len == 2 && cp[0] == '.' && cp[1] == '.') parent++;
		else if (tmp && *tmp) continue;
		if (strnpathcmp(file, cp, flen)) continue;

		new = malloc2(len + 1 + 1);
		strncpy(new, cp, len);
		if (parent || isdir(&(arcflist[i]))) new[len++] = _SC_;
		new[len] = '\0';
		if (finddupl(new, argc, *argvp)) {
			free(new);
			continue;
		}

		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = new;
	}
	strcpy(archivedir, duparcdir);
	return(argc);
}
#endif	/* !_NOCOMPLETE */

int dolaunch(list, argset)
launchtable *list;
int argset;
{
#ifndef	_NODOSDRIVE
	int drive;
#endif
	char *tmpdir;
	int i;

	tmpdir = NULL;
#ifndef	_NODOSDRIVE
	drive = 0;
#endif

	if (argset) /*EMPTY*/;
	else if (archivefile) {
		if (!(tmpdir = tmpunpack(1))) {
			putterm(t_bell);
			return(-1);
		}
	}
#ifndef	_NODOSDRIVE
	else if ((drive = tmpdosdupl("", &tmpdir, 1)) < 0) {
		putterm(t_bell);
		return(-1);
	}
#endif

	if (!isarchbr(list)) {
		execusercomm(list -> comm, filelist[filepos].name,
			1, argset, 1);
		if (argset) /*EMPTY*/;
#ifndef	_NODOSDRIVE
		else if (drive) removetmp(tmpdir, filelist[filepos].name);
#endif
		else if (archivefile) removetmp(tmpdir, NULL);
		return(1);
	}

	pusharchdupl();
	if (argset) /*EMPTY*/;
#ifndef	_NODOSDRIVE
	else if (drive) {
		archduplp -> v_fullpath = strdup2(fullpath);
		strcpy(fullpath, tmpdir);
	}
#endif
	else if (archivefile) {
		archduplp -> v_fullpath = strdup2(fullpath);
		strcpy(fullpath, tmpdir);
		if (*archivedir) {
			strcpy(strcatdelim(fullpath), archivedir);
			archduplp -> v_archivedir = strdup2(archivedir);
		}
	}
	archivefile = (filelist && filepos < maxfile)
		? filelist[filepos].name : NULL;
	if (!archivefile) archivefile = "";
	archivefile = strdup2(archivefile);
	*archivedir = '\0';
	archtmpdir = tmpdir;
#ifndef	_NODOSDRIVE
	archdrive = drive;
#endif
#ifndef	_NOTREE
	treepath = NULL;
#endif
	findpattern = NULL;
	launchp = list;
	filelist = NULL;
	maxent = 0;
	sorton = 0;

	if (readarchive(archivefile, launchp, argset) < 0) {
		arcflist = NULL;
		poparchdupl();
		return(-1);
	}

	arcflist = (namelist *)malloc2(((maxfile) ? maxfile : 1)
		* sizeof(namelist));
	for (i = 0; i < maxfile; i++)
		memcpy((char *)&(arcflist[i]), (char *)&(filelist[i]),
			sizeof(namelist));
	maxarcf = maxfile;
	maxfile = 0;
	filepos = -1;

	return(1);
}

int launcher(VOID_A)
{
	reg_t *re;
	int i, n;

#ifndef	_NOBROWSE
	if (browselist) {
		char *cp;

		n = browselevel;
		cp = browselist[n].ext;

		pushbrowsevar(filelist[filepos].name);
		if (cp && !(browselist[n].flags & LF_FILENOPREP))
			execusercomm(cp, filelist[filepos].name, 1, 1, 1);

		if (browselist[n + 1].comm
		&& !(browselist[n].flags & LF_FILELOOP)) n++;
		if (dolaunch(&(browselist[n]), 1) < 0
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

	return(dolaunch(&(launchlist[i]), 0));
}

static int NEAR undertmp(path)
char *path;
{
	winvartable *wvp;
	char *full, rpath[MAXPATHLEN], dir[MAXPATHLEN];

	full = fullpath;
	realpath2(path, rpath, 1);
	for (wvp = archduplp; wvp; wvp = wvp -> v_archduplp) {
		if (!(wvp -> v_fullpath)) continue;
		realpath2(full, dir, 1);
		if (underpath(rpath, dir, -1)) return(1);
		full = wvp -> v_fullpath;
	}

	return(0);
}

#ifdef	_NODOSDRIVE
static char *NEAR genfullpath(path, file, full)
char *path, *file, *full;
#else
static char *NEAR genfullpath(path, file, full, tmpdir)
char *path, *file, *full, *tmpdir;
#endif
{
	char *cp;

	cp = file;
#ifdef	_USEDOSPATH
	if (_dospath(cp)) cp += 2;
#endif

#ifndef	_NODOSDRIVE
	if (tmpdir) strcpy(path, tmpdir);
	else
#endif
	if (*cp == _SC_) {
		strcpy(path, file);
		return(path);
	}
	else if (!full || !*full) {
		errno = ENOENT;
		return(NULL);
	}
#if	MSDOS
	else if (cp > file) {
		int drive;

		path[0] = *file;
		path[1] = ':';
		path[2] = _SC_;
		if ((drive = toupper2(*file)) == toupper2(*full))
			strcpy(&(path[3]), &(full[3]));
		else if (!unixgetcurdir(&(path[3]), drive - 'A' + 1))
			return(NULL);
	}
#endif
	else strcpy(path, full);

	strcpy(strcatdelim(path), cp);
	return(path);
}

#ifndef	_NODOSDRIVE
static int NEAR archdostmpdir(path, dirp, full)
char *path, **dirp, *full;
{
	char *cp, dupfullpath[MAXPATHLEN];
	int drive;

	strcpy(dupfullpath, fullpath);
	if (full != fullpath) {
		strcpy(fullpath, full);
		if (_chdir2(fullpath) < 0) full = NULL;
	}
	drive = dospath2(path);
	strcpy(fullpath, dupfullpath);
	if (full != fullpath && _chdir2(fullpath) < 0) {
		lostcwd(fullpath);
		return(-1);
	}

	if (!drive) return(0);
	cp = path;
	if (_dospath(cp)) cp += 2;
	if (full) strcpy(fullpath, full);
	else if (*cp != _SC_) return(-1);
	realpath2(path, path, 1);
	strcpy(fullpath, dupfullpath);

	if (!(cp = dostmpdir(drive))) return(-1);
	*dirp = cp;
	return(1);
}
#endif	/* !_NODOSDRIVE */

int pack(arc)
char *arc;
{
#ifndef	_NODOSDRIVE
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
#ifndef	_NODOSDRIVE
	tmpdest = NULL;
	drive = 0;

	strcpy(path, arc);
	if ((dest = strrdelim(path, 1))) *(++dest) = '\0';
	else strcpy(path, ".");
	if ((n = archdostmpdir(path, &tmpdest, full)) < 0) {
		warning(ENOENT, arc);
		return(0);
	}
	if (n) dest = strdup2(path);
#endif	/* !_NODOSDRIVE */

	if (archivefile) {
		if (!(tmpdir = tmpunpack(0))) {
#ifndef	_NODOSDRIVE
			free(dest);
#endif
			return(0);
		}
	}
#ifndef	_NODOSDRIVE
	else if ((drive = tmpdosdupl("", &tmpdir, 0)) < 0) {
		free(dest);
		return(0);
	}
#endif

#ifdef	_NODOSDRIVE
	if (!genfullpath(path, arc, full)) warning(-1, arc);
#else
	if (!genfullpath(path, arc, full, tmpdest)) warning(-1, arc);
#endif
	else {
		duparchivefile = archivefile;
		archivefile = NULL;
		Xwaitmes();
		ret = execusercomm(archivelist[i].p_comm, path, -1, 1, 0);
		if (ret > 0) {
			if (ret < 127) {
				hideclock = 1;
				warning(0, HITKY_K);
			}
			rewritefile(1);
		}
		archivefile = duparchivefile;
	}
#ifndef	_NODOSDRIVE
	if (tmpdest) {
		if (!ret) {
			if (_chdir2(tmpdest) < 0) ret = 1;
			else forcemovefile(dest);
		}
		free(dest);
		removetmp(tmpdest, NULL);
	}
	if (drive) removetmp(tmpdir, NULL);
	else
#endif	/* !_NODOSDRIVE */
	if (archivefile) removetmp(tmpdir, NULL);

	return((ret) ? 0 : 1);
}

/*ARGSUSED*/
int unpack(arc, dir, arg, tr, ignorelist)
char *arc, *dir;
char *arg;
int tr, ignorelist;
{
#ifndef	_NODOSDRIVE
	winvartable *wvp;
	namelist alist[1], *dupfilelist;
	char *full, *dest, *tmpdest;
	int dd, drive, dupmaxfile, dupfilepos;
#endif
	reg_t *re;
	char *cp, *tmpdir, path[MAXPATHLEN];
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

	if (dir) strcpy(path, dir);
	else for (;;) {
#ifndef	_NOTREE
		if (tr) {
# ifdef	_NODOSDRIVE
			dir = tree(0, (int *)1);
# else
			dir = tree(0, &dd);
			if (dd >= 0) shutdrv(dd);
# endif
		}
		else
#endif	/* !_NOTREE */
		{
			if (arg && *arg) dir = strdup2(arg);
			else dir = inputstr(UNPAC_K, 1, -1, NULL, 1);
			dir = evalpath(dir, 0);
		}
		if (!dir) return(0);
		if (!*dir) strcpy(path, ".");
		else {
			cp = strcpy2(path, dir);
#ifdef	_USEDOSPATH
			if (_dospath(dir) && !dir[2]) strcpy(cp, ".");
#endif
		}
		free(dir);
		dir = NULL;

		if (!undertmp(path)) break;
		locate(0, L_CMDLINE);
		putterm(l_clear);
		cprintf2("[%.*s]", n_column - 2, path);
		if (yesno(UNPTM_K)) break;
	}

	ret = 1;
	cp = path;
	tmpdir = NULL;
#ifndef	_NODOSDRIVE
	tmpdest = NULL;

	full = fullpath;
	for (wvp = archduplp; wvp; wvp = wvp -> v_archduplp)
		if (wvp -> v_fullpath) full = wvp -> v_fullpath;

	if ((n = archdostmpdir(path, &tmpdest, full)) < 0) {
		warning(ENOENT, path);
		return(0);
	}
	if (n) {
		dest = strdup2(path);
		cp = tmpdest;
	}
# ifdef	FAKEUNINIT
	else dest = NULL;	/* fake for -Wuninitialized */
# endif

	dupfilelist = filelist;
	dupmaxfile = maxfile;
	dupfilepos = filepos;
	alist[0].name = arc;
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
#endif	/* !_NODOSDRIVE */
	if ((!dir && preparedir(path) < 0) || _chdir2(cp) < 0)
		warning(-1, path);
#ifdef	_NODOSDRIVE
	else if (!genfullpath(path, arc, fullpath)) warning(-1, arc);
#else
	else if (!genfullpath(path, arc, fullpath, tmpdir)) warning(-1, arc);
#endif
	else {
		Xwaitmes();
		ret = execusercomm(archivelist[i].u_comm, path,
			-1, 1, ignorelist);
		if (ret > 0) {
			if (ret < 127) {
				hideclock = 1;
				warning(0, HITKY_K);
			}
			rewritefile(1);
		}
	}

#ifndef	_NODOSDRIVE
	if (tmpdest) {
		if (!ret) forcemovefile(dest);
		free(dest);
		removetmp(tmpdest, NULL);
	}
	if (tmpdir) removetmp(tmpdir, arc);
	else
#endif	/* !_NODOSDRIVE */
	if (_chdir2(fullpath) < 0) lostcwd(fullpath);
	return((ret) ? 0 : 1);
}

char *tmpunpack(single)
int single;
{
#ifdef	_USEDOSEMU
	char tmp[MAXPATHLEN];
#endif
	winvartable *wvp;
	char *subdir, *tmpdir, path[MAXPATHLEN];
	int i, ret, dupmark;

	for (i = 0, wvp = archduplp; wvp; i++, wvp = wvp -> v_archduplp);
	strcpy(path, ARCHTMPPREFIX);
	if (mktmpdir(path) < 0) {
		warning(-1, path);
		return(NULL);
	}
	tmpdir = strdup2(path);

	for (i = 0; i < maxfile; i++)
		if (ismark(&(filelist[i]))) filelist[i].tmpflags |= F_WSMRK;
	dupmark = mark;
	if (single) mark = 0;
	ret = unpack(archivefile, path, NULL, 0, 0);
	mark = dupmark;
	for (i = 0; i < maxfile; i++) {
		if (wasmark(&(filelist[i]))) filelist[i].tmpflags |= F_ISMRK;
		filelist[i].tmpflags &= ~F_WSMRK;
	}

	subdir = archivedir;
	while (*subdir == _SC_) subdir++;

	if (ret < 0) putterm(t_bell);
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
char *dev;
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

	st.flags = 0;
	if (!(tmp = evalcommand("tar cf %C %TA", dev, &st, 0))) return(0);

#ifdef	_NOEXTRAMACRO
	if (filelist) for (;;) {
		system2(tmp, -1);
		free(tmp);

		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand("tar rf %C %TA", dev, NULL, 0)))
			break;
	}
	else
#endif
	{
		system2(tmp, -1);
		free(tmp);
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
char *regstr;
namelist *flist;
int maxf, n;
{
	namelist tmp;
	launchtable *list;
	namelist *dupfilelist;
	reg_t *re;
	FILE *fp;
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

	locate(0, n_line - 1);
	tflush();
	if (!(fp = popenmacro(list -> comm, file, 0))) {
		if (n >= 0) removetmp(tmpdir, file);
		return(0);
	}

	locate(0, L_MESLINE);
	putterm(l_clear);
	putterm(t_standout);
	kanjiputs(SEACH_K);
	putterm(end_standout);
	kanjiputs(file);
	tflush();

	for (i = 0; i < (int)(list -> topskip); i++) {
		if (!(cp = fgets2(fp, 0))) break;
		free(cp);
	}

	no = match = 0;
	re = regexp_init(regstr, -1);
	parsearchive(NULL, list, NULL, NULL);
	for (;;) {
		if (match && match <= no - (int)(list -> bottomskip)) break;
		i = parsearchive(fp, list, &tmp, &no);
		if (i < 0) {
			if (i == -1) break;
			pclose2(fp);
			parsearchive(NULL, NULL, NULL, NULL);
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
		free(tmp.name);
#ifndef	NOSYMLINK
		if (tmp.linkname) free(tmp.linkname);
#endif
	}
	parsearchive(NULL, NULL, NULL, NULL);
	regexp_free(re);
	if (n >= 0) removetmp(tmpdir, file);

	if ((i = pclose2(fp))) return((i > 0) ? -1 : 0);
	if (match > no - (int)(list -> bottomskip)) match = 0;

	return(match);
}
#endif	/* !_NOARCHIVE */
