/*
 *	builtin.c
 *
 *	builtin commands
 */

#include "fd.h"
#include "encode.h"
#include "parse.h"
#include "kconv.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	DEP_PTY
#include "termemu.h"
#endif
#ifdef	DEP_IME
#include "roman.h"
#endif

#define	STRHDD			"HDD"
#define	STRHDD98		"98"
#ifndef	DEP_ORIGSHELL
#define	RET_SUCCESS		0
#define	RET_NOTICE		255
#endif
#if	FD >= 2
#define	BINDCOMMENT		':'
#define	DRIVESEP		' '
#define	ALIASSEP		'='
#define	FUNCNAME		0
#else
#define	BINDCOMMENT		';'
#define	DRIVESEP		','
#define	ALIASSEP		'\t'
#define	FUNCNAME		1
#endif

extern macrolist_t macrolist;
extern int maxmacro;
extern bindlist_t bindlist;
extern int maxbind;
#ifndef	_NOARCHIVE
extern launchlist_t launchlist;
extern int maxlaunch;
extern archivelist_t archivelist;
extern int maxarchive;
# ifndef	_NOBROWSE
extern char **browsevar;
# endif
#endif	/* !_NOARCHIVE */
extern CONST strtable keyidentlist[];
extern CONST functable funclist[];
extern char **history[];
extern short histsize[];
extern short histno[];
extern helpindex_t helpindex;
#ifndef	_NOCUSTOMIZE
extern orighelpindex_t orighelpindex;
#endif
extern int fd_restricted;
#if	FD >= 2
extern char *progpath;
extern char *progname;
extern CONST char *promptstr;
extern int lcmdline;
#endif
extern int internal_status;
#ifndef	DEP_ORIGSHELL
extern char **environ2;
extern aliastable aliaslist[];
extern int maxalias;
extern userfunctable userfunclist[];
extern int maxuserfunc;
extern int win_x;
extern int win_y;
extern int hideclock;
#endif
extern int inruncom;

static VOID NEAR builtinerror __P_((char *CONST [], CONST char *, int));
#ifdef	DEP_ORIGSHELL
#define	hitkey(n)
#else
static VOID NEAR hitkey __P_((int));
#endif
static VOID NEAR fputsmeta __P_((CONST char *, XFILE *));
#if	(!defined (_NOARCHIVE) && !defined (_NOBROWSE)) || defined (DEP_IME)
static char **NEAR file2argv __P_((XFILE *, CONST char *, int));
#endif
#ifndef	_NOARCHIVE
# ifndef	OLDPARSE
static int NEAR getlaunchopt __P_((int, char *CONST [],
		CONST char *, lsparse_t *));
# endif
static int NEAR setlaunch __P_((int, char *CONST []));
static int NEAR setarch __P_((int, char *CONST []));
static int NEAR printlaunch __P_((int, char *CONST []));
static int NEAR printarch __P_((int, char *CONST []));
# ifndef	_NOBROWSE
static char **NEAR readargv __P_((char *CONST *, char **));
static int NEAR custbrowse __P_((int, char *CONST []));
# endif
#endif
static int NEAR setmacro __P_((char *));
static int NEAR setkeybind __P_((int, char *CONST []));
static int NEAR printbind __P_((int, char *CONST []));
#ifdef	DEP_DOSEMU
static int NEAR _setdrive __P_((int, char *CONST [], int));
static int NEAR setdrive __P_((int, char *CONST []));
static int NEAR unsetdrive __P_((int, char *CONST []));
static int NEAR printdrive __P_((int, char *CONST []));
#endif
#ifndef	_NOKEYMAP
static int NEAR setkeymap __P_((int, char *CONST []));
static int NEAR keytest __P_((int, char *CONST []));
#endif
static int NEAR printhist __P_((int, char *CONST []));
#if	FD >= 2
# ifndef	NOPOSIXUTIL
static int NEAR fixcommand __P_((int, char *CONST []));
# endif
static int NEAR printmd5 __P_((CONST char *, XFILE *));
static int NEAR md5sum __P_((int, char *CONST []));
static int NEAR evalmacro __P_((int, char *CONST []));
# ifdef	DEP_KCONV
static int NEAR kconv __P_((int, char *CONST []));
# endif
static int NEAR getinputstr __P_((int, char *CONST []));
static int NEAR getyesno __P_((int, char *CONST []));
#endif	/* FD >= 2 */
#if	!MSDOS && (FD >= 2)
static int NEAR savetty __P_((int, char *CONST []));
#endif
#ifdef	DEP_IME
static int NEAR setroman __P_((int, char *CONST []));
static VOID NEAR disproman __P_((char *, int, XFILE *));
static int NEAR printroman __P_((int, char *CONST []));
#endif
#ifndef	DEP_ORIGSHELL
static int NEAR printenv __P_((int, char *CONST []));
static int NEAR setalias __P_((int, char *CONST []));
static int NEAR unalias __P_((int, char *CONST []));
static int NEAR checkuserfunc __P_((int, char *CONST []));
static int NEAR setuserfunc __P_((int, char *CONST []));
static int NEAR exportenv __P_((int, char *CONST []));
static int NEAR dochdir __P_((int, char *CONST []));
static int NEAR loadsource __P_((int, char *CONST []));
#endif	/* !DEP_ORIGSHELL */

static CONST char *CONST builtinerrstr[] = {
	NULL,
#define	ER_FEWMANYARG		1
	"Too few or many arguments",
#define	ER_OUTOFLIMIT		2
	"Out of limits",
#define	ER_NOENTRY		3
	"No such entry",
#define	ER_SYNTAXERR		4
	"Syntax error",
#define	ER_EXIST		5
	"Entry already exists",
#define	ER_INVALDEV		6
	"Invalid device",
#define	ER_EVENTNOFOUND		7
	"Event not found",
#define	ER_UNKNOWNOPT		8
	"Unknown option",
#define	ER_NOARGSPEC		9
	"No argument is specified",
#define	ER_NOTINSHELL		10
	"Cannot execute in shell mode",
#define	ER_NOTINRUNCOM		11
	"Cannot execute in startup file",
#define	ER_NOTRECURSE		12
	"Cannot execute recursively",
#define	ER_NOTDUMBTERM		13
	"Cannot execute on dumb term",
};
#define	BUILTINERRSIZ		arraysize(builtinerrstr)
static CONST builtintable builtinlist[] = {
#ifndef	_NOARCHIVE
	{setlaunch,	BL_LAUNCH},
	{setarch,	BL_ARCH},
	{printlaunch,	BL_PLAUNCH},
	{printarch,	BL_PARCH},
# ifndef	_NOBROWSE
	{custbrowse,	BL_BROWSE},
# endif
#endif
	{setkeybind,	BL_BIND},
	{printbind,	BL_PBIND},
#ifdef	DEP_DOSEMU
	{setdrive,	BL_SDRIVE},
	{unsetdrive,	BL_UDRIVE},
	{printdrive,	BL_PDRIVE},
#endif
#ifndef	_NOKEYMAP
	{setkeymap,	BL_KEYMAP},
	{keytest,	BL_GETKEY},
#endif
	{printhist,	BL_HISTORY},
#if	FD >= 2
# ifndef	NOPOSIXUTIL
	{fixcommand,	BL_FC},
# endif
	{md5sum,	BL_CHECKID},
	{evalmacro,	BL_EVALMACRO},
# ifdef	DEP_KCONV
	{kconv,		BL_KCONV},
# endif
	{getinputstr,	BL_READLINE},
	{getyesno,	BL_YESNO},
#endif	/* FD >= 2 */
#if	!MSDOS && (FD >= 2)
	{savetty,	BL_SAVETTY},
#endif
#ifdef	DEP_IME
	{setroman,	BL_SETROMAN},
	{printroman,	BL_PRINTROMAN},
#endif
#ifndef	DEP_ORIGSHELL
# if	FD >= 2
	{printenv,	BL_SET},
# else
	{printenv,	BL_PENV},
# endif
	{setalias,	BL_ALIAS},
	{unalias,	BL_UALIAS},
# if	FD < 2
	{setuserfunc,	BL_FUNCTION},
# endif
	{exportenv,	BL_EXPORT},
# if	MSDOS || (FD < 2)
	{dochdir,	BL_CHDIR},
# endif
	{dochdir,	BL_CD},
	{loadsource,	BL_SOURCE},
# if	FD >= 2
	{loadsource,	BL_DOT},
# endif
#endif	/* !DEP_ORIGSHELL */
};
#define	BUILTINSIZ		arraysize(builtinlist)


static VOID NEAR builtinerror(argv, s, n)
char *CONST argv[];
CONST char *s;
int n;
{
	int duperrno;

	duperrno = errno;
	if (n >= BUILTINERRSIZ || (n < 0 && !errno)) return;
	if (argv && argv[0]) VOID_C Xfprintf(Xstderr, "%k: ", argv[0]);
	if (s) VOID_C Xfprintf(Xstderr, "%k: ", s);
	VOID_C Xfprintf(Xstderr,
		"%s.\n",
		(n >= 0 && builtinerrstr[n])
			? builtinerrstr[n] : Xstrerror(duperrno));
}

#ifndef	DEP_ORIGSHELL
static VOID NEAR hitkey(init)
int init;
{
	static int n = 0;

	if (init) {
		if (init < 0) n = init;
		else if (init == 1) n = 0;
		else if (init == 2) {
			if (n >= 0) n = 1;
		}
		else if (n > 0 && n < n_line) {
			hideclock = 1;
			warning(0, HITKY_K);
		}
		return;
	}

	if (n <= 0) return;

	if (n >= n_line) n = 1;
	if (++n >= n_line) {
		Xfflush(Xstdout);
		ttyiomode(1);
		win_x = 0;
		win_y = n_line - 1;
		hideclock = 1;
		warning(0, HITKY_K);
		stdiomode();
	}
}
#endif	/* !DEP_ORIGSHELL */

static VOID NEAR fputsmeta(arg, fp)
CONST char *arg;
XFILE *fp;
{
	char *cp;

	if (!*arg) Xfputs("\"\"", fp);
	else {
		cp = killmeta(arg);
		kanjifputs(cp, fp);
		Xfree(cp);
	}
}

#if	(!defined (_NOARCHIVE) && !defined (_NOBROWSE)) || defined (DEP_IME)
static char **NEAR file2argv(fp, s, whole)
XFILE *fp;
CONST char *s;
int whole;
{
	char *cp, *line, **argv;
	ALLOC_T size;
	int i, j, pc, argc, escape, quote, pqoute, quoted;

	argc = 1;
	argv = (char **)Xmalloc(2 * sizeof(char *));
	argv[0] = Xstrdup(s);
	argv[1] = NULL;
	j = escape = 0;
	quote = pqoute = quoted = '\0';
	cp = c_realloc(NULL, 0, &size);
	while ((line = Xfgets(fp))) {
		if (!escape && !quote && *line == '#') {
			Xfree(line);
			continue;
		}
		escape = 0;
		for (i = 0; line[i]; i++) {
			cp = c_realloc(cp, j + 2, &size);
			pc = parsechar(&(line[i]), -1,
				'\0', EA_EOLESCAPE, &quote, &pqoute);
			if (pc == PC_CLQUOTE) quoted = line[i];
			else if (pc == PC_WCHAR) {
				cp[j++] = line[i++];
				cp[j++] = line[i];
			}
			else if (pc == PC_SQUOTE || pc == PC_DQUOTE)
				cp[j++] = line[i];
			else if (pc == PC_ESCAPE) {
				if (!line[++i]) {
					escape = 1;
					break;
				}
				cp[j++] = line[i];
			}
			else if (pc == PC_OPQUOTE) quoted = '\0';
			else if (pc != PC_NORMAL) /*EMPTY*/;
			else if (!Xstrchr(IFS_SET, line[i])) cp[j++] = line[i];
			else if (j || quoted) {
				quoted = cp[j] = '\0';
				argv = (char **)Xrealloc(argv,
					(argc + 2) * sizeof(*argv));
				argv[argc++] = Xstrdup(cp);
				argv[argc] = NULL;
				j = 0;
			}
		}

		if (escape);
		else if (quote) cp[j++] = '\n';
		else if (j || quoted) {
			quoted = cp[j] = '\0';
			argv = (char **)Xrealloc(argv,
				(argc + 2) * sizeof(*argv));
			argv[argc++] = Xstrdup(cp);
			argv[argc] = NULL;
			j = 0;
		}
		Xfree(line);

		if (!whole && !escape && !quote) break;
	}

	if (j) {
		cp[j] = '\0';
		argv = (char **)Xrealloc(argv, (argc + 2) * sizeof(*argv));
		argv[argc++] = Xstrdup(cp);
		argv[argc] = NULL;
	}
	Xfree(cp);

	return(argv);
}
#endif	/* (!_NOARCHIVE && !_NOBROWSE) || DEP_IME */

#ifndef	_NOARCHIVE
# ifndef	OLDPARSE
static int NEAR getlaunchopt(n, argv, opts, lp)
int n;
char *CONST argv[];
CONST char *opts;
lsparse_t *lp;
{
	CONST char *cp;
	int i, c;

	if (argv[n][0] != '-') return(0);
	c = argv[n][1];
	if (!c || c == '-') return(++n);
	else if (!Xstrchr(opts, c)) {
		lp -> topskip = ER_UNKNOWNOPT;
		lp -> bottomskip = n;
		return(-1);
	}

	if (argv[n][2]) cp = &(argv[n][2]);
	else if (argv[n + 1]) cp = argv[++n];
	else {
		lp -> topskip = ER_NOARGSPEC;
		lp -> bottomskip = n;
		return(-1);
	}

	if (c == 'f') {
		i = countvar(lp -> format);
		lp -> format = (char **)Xrealloc(lp -> format,
			(i + 2) * sizeof(*(lp -> format)));
		lp -> format[i++] = Xstrdup(cp);
		lp -> format[i] = NULL;
	}
	else if (c == 'i') {
		i = countvar(lp -> lignore);
		lp -> lignore = (char **)Xrealloc(lp -> lignore,
			(i + 2) * sizeof(*(lp -> lignore)));
		lp -> lignore[i++] = Xstrdup(cp);
		lp -> lignore[i] = NULL;
	}
	else if (c == 'e') {
		i = countvar(lp -> lerror);
		lp -> lerror = (char **)Xrealloc(lp -> lerror,
			(i + 2) * sizeof(*(lp -> lerror)));
		lp -> lerror[i++] = Xstrdup(cp);
		lp -> lerror[i] = NULL;
	}
	else if (c == 't' || c == 'b') {
		if ((i = Xatoi(cp)) < 0) {
			lp -> topskip = ER_SYNTAXERR;
			lp -> bottomskip = n;
			return(-1);
		}
		if (c == 't') lp -> topskip = i;
		else lp -> bottomskip = i;
	}
	else if (c == 'p') {
		Xfree(lp -> ext);
		lp -> ext = Xstrdup(cp);
	}
	else {
		if (!strcmp(cp, "loop"))
			i = (c == 'd') ? LF_DIRLOOP : LF_FILELOOP;
		else if (!strcmp(cp, "noprep"))
			i = (c == 'd') ? LF_DIRNOPREP : LF_FILENOPREP;
		else {
			lp -> topskip = ER_SYNTAXERR;
			lp -> bottomskip = n;
			return(-1);
		}
		lp -> flags |= i;
	}
	n++;

	return(n);
}
# endif	/* FD >= 2 */

VOID freelaunch(lp)
lsparse_t *lp;
{
	Xfree(lp -> ext);
	Xfree(lp -> comm);
# ifndef	OLDPARSE
	freevar(lp -> format);
	freevar(lp -> lignore);
	freevar(lp -> lerror);
# endif
}

int searchlaunch(lp, list, max)
CONST lsparse_t *lp, *list;
int max;
{
	int i, n;

	for (i = 0; i < max; i++) {
		n = extcmp(lp -> ext, lp -> flags,
			list[i].ext, list[i].flags, 1);
		if (!n) break;
	}

	return(i);
}

int parselaunch(argc, argv, lp)
int argc;
char *CONST argv[];
lsparse_t *lp;
{
# ifdef	OLDPARSE
	char *cp, *tmp;
	u_char c;
	int ch;
# else
	int j, opt, err;
# endif
	int i, n;

	lp -> comm = NULL;
# ifndef	OLDPARSE
	lp -> format = lp -> lignore = lp -> lerror = NULL;
# endif

	if (argc <= 1) {
		lp -> topskip = ER_FEWMANYARG;
		lp -> bottomskip = 0;
		return(-1);
	}

	lp -> ext = getext(argv[1], &(lp -> flags));

	n = searchlaunch(lp, launchlist, maxlaunch);
# ifndef	DEP_DYNAMICLIST
	if (n >= MAXLAUNCHTABLE) {
		Xfree(lp -> ext);
		lp -> topskip = ER_OUTOFLIMIT;
		lp -> bottomskip = 0;
		return(-1);
	}
# endif

	if (argc < 3) return(n);

# ifdef	OLDPARSE
	lp -> comm = Xstrdup(argv[2]);
	if (argc <= 3) lp -> topskip = lp -> bottomskip = SKP_NONE;
	else {
		cp = tmp = catvar(&(argv[3]), '\0');
		if (!(cp = Xsscanf(cp, "%Cu,", &c))) {
			lp -> topskip = ER_SYNTAXERR;
			lp -> bottomskip = SKP_NONE;
			Xfree(lp -> ext);
			Xfree(lp -> comm);
			lp -> ext = tmp;
			return(-1);
		}
		lp -> topskip = c;
		if (!(cp = Xsscanf(cp, "%Cu", &c))) {
			lp -> topskip = ER_SYNTAXERR;
			lp -> bottomskip = SKP_NONE;
			Xfree(lp -> ext);
			Xfree(lp -> comm);
			lp -> ext = tmp;
			return(-1);
		}
		lp -> bottomskip = c;

		ch = ':';
		for (i = 0; i < MAXLSPARSEFIELD; i++) {
			cp = getrange(cp, ch, &(lp -> field[i]),
				&(lp -> delim[i]), &(lp -> width[i]));
			if (!cp) {
				lp -> topskip = ER_SYNTAXERR;
				lp -> bottomskip = SKP_NONE;
				Xfree(lp -> ext);
				Xfree(lp -> comm);
				lp -> ext = tmp;
				return(-1);
			}
			ch = ',';
		}

		ch = ':';
		for (i = 0; i < MAXLSPARSESEP; i++) {
			if (*cp != ch) break;
			if (!(cp = Xsscanf(++cp, "%Cu", &c))) {
				lp -> topskip = ER_SYNTAXERR;
				lp -> bottomskip = SKP_NONE;
				Xfree(lp -> ext);
				Xfree(lp -> comm);
				lp -> ext = tmp;
				return(-1);
			}
			lp -> sep[i] = (c) ? c - 1 : SEP_NONE;
			ch = ',';
		}
		for (; i < MAXLSPARSESEP; i++) lp -> sep[i] = SEP_NONE;

		if (!*cp) lp -> lines = 1;
		else {
			if (!Xsscanf(cp, ":%Cu%$", &c)) {
				lp -> topskip = ER_SYNTAXERR;
				lp -> bottomskip = SKP_NONE;
				Xfree(lp -> ext);
				Xfree(lp -> comm);
				lp -> ext = tmp;
				return(-1);
			}
			lp -> lines = (c) ? c : 1;
		}

		Xfree(tmp);
	}
# else	/* !OLDPARSE */
	lp -> topskip = lp -> bottomskip = 0;
	opt = err = 0;
	for (i = 2; i < argc; i++) {
		j = getlaunchopt(i, argv, "fietb", lp);
		if (!j) {
			if (lp -> comm) break;
			lp -> comm = Xstrdup(argv[i]);
		}
		else if (j > 0) {
			opt++;
			i = j - 1;
		}
		else {
			err++;
			break;
		}
	}

	if (err);
	else if (!(lp -> comm)) {
		lp -> topskip = ER_FEWMANYARG;
		lp -> bottomskip = 0;
		err++;
	}
	else if (i + 3 < argc) {
		lp -> topskip = ER_FEWMANYARG;
		lp -> bottomskip = i + 3;
		err++;
	}
	else if (i < argc && opt) {
		lp -> topskip = ER_FEWMANYARG;
		lp -> bottomskip = i;
		err++;
	}
	if (err) {
		freelaunch(lp);
		return(-1);
	}

	if (i < argc) {
		lp -> format = (char **)Xmalloc(2 * sizeof(*(lp -> format)));
		lp -> format[0] = Xstrdup(argv[i]);
		lp -> format[1] = NULL;
	}

	if (i + 1 >= argc) /*EMPTY*/;
	else if ((j = Xatoi(argv[i + 1])) >= 0) lp -> topskip = j;
	else {
		lp -> topskip = ER_SYNTAXERR;
		lp -> bottomskip = i + 1;
		freelaunch(lp);
		return(-1);
	}

	if (i + 2 >= argc) /*EMPTY*/;
	else if ((j = Xatoi(argv[i + 2])) >= 0) lp -> bottomskip = j;
	else {
		lp -> topskip = ER_SYNTAXERR;
		lp -> bottomskip = i + 2;
		freelaunch(lp);
		return(-1);
	}
# endif	/* !OLDPARSE */

	return(n);
}

VOID addlaunch(n, lp)
int n;
lsparse_t *lp;
{
	if (n < maxlaunch) freelaunch(&(launchlist[n]));
	else {
		maxlaunch = n + 1;
# ifdef	DEP_DYNAMICLIST
		launchlist = (launchlist_t)Xrealloc(launchlist,
			maxlaunch * sizeof(*launchlist));
# endif
	}
	memcpy((char *)&(launchlist[n]), (char *)lp, sizeof(*launchlist));
}

VOID deletelaunch(n)
int n;
{
	if (n >= maxlaunch) return;
	freelaunch(&(launchlist[n]));
	memmove((char *)&(launchlist[n]), (char *)&(launchlist[n + 1]),
		(--maxlaunch - n) * sizeof(*launchlist));
}

static int NEAR setlaunch(argc, argv)
int argc;
char *CONST argv[];
{
	lsparse_t launch;
	int n;

	if ((n = parselaunch(argc, argv, &launch)) < 0) {
# ifdef	OLDPARSE
		if (launch.bottomskip == SKP_NONE) {
			builtinerror(argv, launch.ext, launch.topskip);
			Xfree(launch.ext);
		}
		else
# endif
		builtinerror(argv,
			(launch.bottomskip) ? argv[launch.bottomskip] : NULL,
			launch.topskip);
		return(-1);
	}

	if (!(launch.comm)) {
		Xfree(launch.ext);
		if (n >= maxlaunch) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		deletelaunch(n);
# ifdef	DEP_PTY
		sendparent(TE_DELETELAUNCH, n);
# endif

		return(0);
	}

	addlaunch(n, &launch);
# ifdef	DEP_PTY
	sendparent(TE_ADDLAUNCH, n, &launch);
# endif

	return(0);
}

VOID freearch(ap)
archive_t *ap;
{
	Xfree(ap -> ext);
	Xfree(ap -> p_comm);
	Xfree(ap -> u_comm);
}

int searcharch(ap, list, max)
CONST archive_t *ap, *list;
int max;
{
	int i, n;

	for (i = 0; i < max; i++) {
		n = extcmp(ap -> ext, ap -> flags,
			list[i].ext, list[i].flags, 1);
		if (!n) break;
	}

	return(i);
}

int parsearch(argc, argv, ap)
int argc;
char *CONST argv[];
archive_t *ap;
{
	int n;

	ap -> p_comm = ap -> u_comm = NULL;

	if (argc <= 1 || argc >= 5) {
		ap -> flags = ER_FEWMANYARG;
		ap -> ext = NULL;
		return(-1);
	}

	ap -> ext = getext(argv[1], &(ap -> flags));

	n = searcharch(ap, archivelist, maxarchive);
# ifndef	DEP_DYNAMICLIST
	if (n >= MAXARCHIVETABLE) {
		Xfree(ap -> ext);
		ap -> flags = ER_OUTOFLIMIT;
		ap -> ext = NULL;
		return(-1);
	}
# endif

	if (argc < 3) return(n);

	if (argv[2][0]) ap -> p_comm = Xstrdup(argv[2]);
	if (argc > 3 && argv[3][0]) ap -> u_comm = Xstrdup(argv[3]);
	if (!(ap -> p_comm) && !(ap -> u_comm)) {
		Xfree(ap -> ext);
		ap -> flags = ER_FEWMANYARG;
		ap -> ext = NULL;
		return(-1);
	}

	return(n);
}

VOID addarch(n, ap)
int n;
archive_t *ap;
{
	if (n < maxarchive) freearch(&(archivelist[n]));
	else {
		maxarchive = n + 1;
# ifdef	DEP_DYNAMICLIST
		archivelist = (archivelist_t)Xrealloc(archivelist,
			maxarchive * sizeof(*archivelist));
# endif
	}
	memcpy((char *)&(archivelist[n]), (char *)ap, sizeof(*archivelist));
}

VOID deletearch(n)
int n;
{
	if (n >= maxarchive) return;
	freearch(&(archivelist[n]));
	memmove((char *)&(archivelist[n]), (char *)&(archivelist[n + 1]),
		(--maxarchive - n) * sizeof(*archivelist));
}

static int NEAR setarch(argc, argv)
int argc;
char *CONST argv[];
{
	archive_t arch;
	int n;

	if ((n = parsearch(argc, argv, &arch)) < 0) {
		builtinerror(argv, arch.ext, arch.flags);
		return(-1);
	}

	if (!(arch.p_comm) && !(arch.u_comm)) {
		Xfree(arch.ext);
		if (n >= maxarchive) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		deletearch(n);
# ifdef	DEP_PTY
		sendparent(TE_DELETEARCH, n);
# endif

		return(0);
	}

	addarch(n, &arch);
# ifdef	DEP_PTY
	sendparent(TE_ADDARCH, n, &arch);
# endif

	return(0);
}

/*ARGSUSED*/
VOID printlaunchcomm(list, n, isset, omit, fp)
CONST lsparse_t *list;
int n, isset, omit;
XFILE *fp;
{
	int i, ch;

	VOID_C Xfprintf(fp, "%s ", BL_LAUNCH);
# ifndef	OLDPARSE
	if (list[n].flags & LF_IGNORECASE) Xfputc('/', fp);
# endif
	fputsmeta(&(list[n].ext[1]), fp);
	if (isset) for (;;) {
		Xfputc('\t', fp);

		fputsmeta(list[n].comm, fp);
# ifdef	OLDPARSE
		if (list[n].topskip == SKP_NONE) break;
		if (omit) {
			Xfputs("\t(Arch)", fp);
			break;
		}
		VOID_C Xfprintf(fp,
			"\t%d,%d",
			(int)(list[n].topskip), (int)(list[n].bottomskip));
		ch = ':';
		for (i = 0; i < MAXLSPARSEFIELD; i++) {
			Xfputc(ch, fp);
			if (list[n].field[i] == FLD_NONE) Xfputc('0', fp);
			else VOID_C Xfprintf(fp,
				"%d", (int)(list[n].field[i]) + 1);
			if (list[n].delim[i] >= 128)
				VOID_C Xfprintf(fp,
					"[%d]",
					(int)(list[n].delim[i]) - 128 + 1);
			else if (list[n].delim[i])
				VOID_C Xfprintf(fp,
					"'%c'", (int)(list[n].delim[i]));
			if (!(list[n].width[i])) /*EMPTY*/;
			else if (list[n].width[i] >= 128)
				VOID_C Xfprintf(fp,
					"-%d", (int)(list[n].width[i]) - 128);
			else VOID_C Xfprintf(fp,
				"-'%c'", (int)(list[n].width[i]));
			ch = ',';
		}
		ch = ':';
		for (i = 0; i < MAXLSPARSESEP; i++) {
			if (list[n].sep[i] == SEP_NONE) break;
			VOID_C Xfprintf(fp,
				"%c%d", ch, (int)(list[n].sep[i]) + 1);
			ch = ',';
		}
		if (list[n].lines > 1) {
			if (!i) Xfputc(':', fp);
			VOID_C Xfprintf(fp, "%d", (int)(list[n].lines));
		}
# else	/* !OLDPARSE */
		if (!list[n].format) break;

		ch = 0;
		for (i = 0; list[n].format[i]; i++) {
			if (i) {
				Xfputs(" \\\n", fp);
				ch++;
			}
			Xfputs("\t-f ", fp);
			fputsmeta(list[n].format[i], fp);
		}
		if (list[n].lignore) for (i = 0; list[n].lignore[i]; i++) {
			Xfputs(" \\\n\t-i ", fp);
			ch++;
			fputsmeta(list[n].lignore[i], fp);
		}
		if (list[n].lerror) for (i = 0; list[n].lerror[i]; i++) {
			Xfputs(" \\\n\t-e ", fp);
			ch++;
			fputsmeta(list[n].lerror[i], fp);
		}
		if (!list[n].topskip && !list[n].bottomskip) break;

		if (list[n].topskip) {
			if (ch) Xfputs(" \\\n", fp);
			VOID_C Xfprintf(fp, "\t-t %d", (int)(list[n].topskip));
		}
		if (list[n].bottomskip) {
			if (list[n].topskip) Xfputc(' ', fp);
			else if (ch) Xfputs(" \\\n\t", fp);
			else Xfputc('\t', fp);
			VOID_C Xfprintf(fp,
				"-b %d", (int)(list[n].bottomskip));
		}
# endif	/* !OLDPARSE */
		break;
/*NOTREACHED*/
	}
	VOID_C fputnl(fp);
}

static int NEAR printlaunch(argc, argv)
int argc;
char *CONST argv[];
{
	char *ext;
	int i, n;
	u_char flags;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	hitkey(2);
	if (argc <= 1) for (i = 0; i < maxlaunch; i++) {
		printlaunchcomm(launchlist, i, 1, 1, Xstdout);
		hitkey(0);
	}
	else {
		ext = getext(argv[1], &flags);
		for (i = 0; i < maxlaunch; i++) {
			n = extcmp(ext, flags,
				launchlist[i].ext, launchlist[i].flags, 0);
			if (!n) break;
		}
		Xfree(ext);
		if (i >= maxlaunch) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		printlaunchcomm(launchlist, i, 1, 0, Xstdout);
	}

	return(0);
}

VOID printarchcomm(list, n, isset, fp)
CONST archive_t *list;
int n, isset;
XFILE *fp;
{
	VOID_C Xfprintf(fp, "%s ", BL_ARCH);
# ifndef	OLDPARSE
	if (list[n].flags & LF_IGNORECASE) Xfputc('/', fp);
# endif
	fputsmeta(&(list[n].ext[1]), fp);
	if (isset) {
		Xfputc('\t', fp);

		if (list[n].p_comm) fputsmeta(list[n].p_comm, fp);
		if (!list[n].u_comm) return;
		Xfputc('\t', fp);
		fputsmeta(list[n].u_comm, fp);
	}
	VOID_C fputnl(fp);
}

static int NEAR printarch(argc, argv)
int argc;
char *CONST argv[];
{
	char *ext;
	int i, n;
	u_char flags;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	hitkey(2);
	if (argc <= 1) for (i = 0; i < maxarchive; i++) {
		printarchcomm(archivelist, i, 1, Xstdout);
		hitkey(0);
	}
	else {
		ext = getext(argv[1], &flags);
		for (i = 0; i < maxarchive; i++) {
			n = extcmp(ext, flags,
				archivelist[i].ext, archivelist[i].flags, 0);
			if (!n) break;
		}
		Xfree(ext);
		if (i >= maxarchive) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		printarchcomm(archivelist, i, 1, Xstdout);
	}

	return(0);
}

# ifndef	_NOBROWSE
static char **NEAR readargv(sargv, dargv)
char *CONST *sargv, **dargv;
{
	XFILE *fp;
	CONST char *cp;
	char **argv;
	int n, dargc;

	dargc = countvar(dargv);
	for (n = 1; sargv[n]; n++) {
		if (sargv[n][0] != '-' || sargv[n][1] != '@') {
			dargv = (char **)Xrealloc(dargv,
				(dargc + 2) * sizeof(*dargv));
			dargv[dargc++] = Xstrdup(sargv[n]);
			dargv[dargc] = NULL;
			continue;
		}
		if (sargv[n][2]) cp = &(sargv[n][2]);
		else if (sargv[n + 1]) cp = sargv[++n];
		else {
			builtinerror(sargv, sargv[n], ER_NOARGSPEC);
			freevar(dargv);
			return(NULL);
		}
		if (cp[0] == '-' && !cp[1]) fp = Xstdin;
		else if (!(fp = Xfopen(cp, "r"))) {
			builtinerror(sargv, cp, -1);
			freevar(dargv);
			return(NULL);
		}

		argv = file2argv(fp, sargv[0], 1);
		if (fp != Xstdin) VOID_C Xfclose(fp);
		else Xclearerr(fp);

		dargv = readargv(argv, dargv);
		freevar(argv);
		if (!dargv) return(NULL);
		dargc = countvar(dargv);
	}

	return(dargv);
}

VOID freebrowse(list)
lsparse_t *list;
{
	int i;

	browselevel = 0;
	freevar(browsevar);
	browsevar = NULL;
	if (argvar) {
		for (i = 1; argvar[i]; i++) {
			Xfree(argvar[i]);
			argvar[i] = NULL;
		}
	}
	if (!list) return;
	for (i = 0; list[i].comm; i++) freelaunch(&(list[i]));
	Xfree(list);
}

static int NEAR custbrowse(argc, argv)
int argc;
char *CONST argv[];
{
	lsparse_t *list;
	char **argv2;
	int i, n, lvl;

#  ifdef	DEP_ORIGSHELL
	if (shellmode) {
		builtinerror(argv, NULL, ER_NOTINSHELL);
		return(-1);
	}
#  endif
	if (inruncom) {
		builtinerror(argv, NULL, ER_NOTINRUNCOM);
		return(-1);
	}
	if (browselist) {
		builtinerror(argv, NULL, ER_NOTRECURSE);
		return(-1);
	}
	argv2 = (char **)Xmalloc(2 * sizeof(char *));
	argv2[0] = Xstrdup(argv[0]);
	argv2[1] = NULL;
	if (!(argv2 = readargv(argv, argv2))) return(-1);

	list = NULL;
	lvl = 0;
	n = 1;
	while (argv2[n]) {
		list = (lsparse_t *)Xrealloc(list, (lvl + 2) * sizeof(*list));
		list[lvl].topskip = list[lvl].bottomskip = 0;
		list[lvl].comm = Xstrdup(argv2[n++]);
		list[lvl].flags = 0;
		list[lvl].ext = list[lvl + 1].comm = NULL;
		list[lvl].format = list[lvl].lignore = list[lvl].lerror = NULL;
		while (argv2[n]) {
			i = getlaunchopt(n, argv2, "fietbpdn", &(list[lvl]));
			if (!i) break;
			else if (i > 0) n = i;
			else {
				builtinerror(argv,
					(list[lvl].bottomskip)
					? argv[list[lvl].bottomskip] : NULL,
					list[lvl].topskip);
				freevar(argv2);
				freebrowse(list);
				return(-1);
/*NOTREACHED*/
				break;
			}
		}
		lvl++;
	}
	for (i = 0; i < lvl; i++)
		if ((!i || i < lvl - 1) && !list[i].format) break;
	if (!lvl || i < lvl) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		freevar(argv2);
		freebrowse(list);
		return(-1);
	}
	freevar(argv2);
	if (list) {
		freebrowse(browselist);
		n = dolaunch(&(list[0]),
			F_NOCONFIRM | F_ARGSET | F_NOADDOPT | F_IGNORELIST);
		if (n < 0) {
			freebrowse(list);
			return(-1);
		}
		browselist = list;
	}

	return(0);
}
# endif	/* !_NOBROWSE */
#endif	/* !_NOARCHIVE */

int ismacro(n)
int n;
{
	if (n < FUNCLISTSIZ || n >= FUNCLISTSIZ + maxmacro) return(0);

	return(1);
}

CONST char *getmacro(n)
int n;
{
	if (ismacro(n)) return(macrolist[n - FUNCLISTSIZ]);

	return(nullstr);
}

static int NEAR setmacro(cp)
char *cp;
{
#ifdef	DEP_DYNAMICLIST
	macrolist = (macrolist_t)Xrealloc(macrolist,
		(maxmacro + 1) * sizeof(*macrolist));
#else
	if (maxmacro >= MAXMACROTABLE) return(-1);
#endif
	macrolist[maxmacro] = cp;

	return(FUNCLISTSIZ + maxmacro++);
}

int freemacro(n)
int n;
{
	int i;

	if (!ismacro(n)) return(-1);

	Xfree(macrolist[n - FUNCLISTSIZ]);
	memmove((char *)&(macrolist[n - FUNCLISTSIZ]),
		(char *)&(macrolist[n - FUNCLISTSIZ + 1]),
		(--maxmacro - (n - FUNCLISTSIZ)) * sizeof(*macrolist));

	for (i = 0; i < maxbind; i++) {
		if (ismacro(ffunc(i)) && ffunc(i) > n) ffunc(i)--;
		if (ismacro(dfunc(i)) && dfunc(i) > n) dfunc(i)--;
	}

	return(0);
}

int searchkeybind(bindp, list, max)
CONST bindtable *bindp, *list;
int max;
{
	int i;

	for (i = 0; i < max; i++) if (list[i].key == bindp -> key) break;

	return(i);
}

int parsekeybind(argc, argv, bindp)
int argc;
char *CONST argv[];
bindtable *bindp;
{
	int i, n;

	if (argc <= 1 || argc >= 6) {
		bindp -> key = ER_FEWMANYARG;
		bindp -> f_func = 0;
		return(-1);
	}
	if ((i = getkeycode(argv[1], 0)) < 0 || i == '\033') {
		bindp -> key = ER_SYNTAXERR;
		bindp -> f_func = 1;
		return(-1);
	}
	bindp -> key = i;

	n = searchkeybind(bindp, bindlist, maxbind);
#ifndef	DEP_DYNAMICLIST
	if (n >= MAXBINDTABLE) {
		bindp -> key = ER_OUTOFLIMIT;
		bindp -> f_func = 0;
		return(-1);
	}
#endif

	if (argc == 2) {
		bindp -> f_func = bindp -> d_func = FNO_NONE;
		return(n);
	}

	for (i = 0; i < FUNCLISTSIZ; i++)
		if (!strcommcmp(argv[2], funclist[i].ident)) break;
	bindp -> f_func = (i < FUNCLISTSIZ) ? (funcno_t)i : FNO_SETMACRO;

	if (argc <= 3 || argv[3][0] == BINDCOMMENT) {
		bindp -> d_func = FNO_NONE;
		i = 3;
	}
	else {
		for (i = 0; i < FUNCLISTSIZ; i++)
			if (!strcommcmp(argv[3], funclist[i].ident)) break;
		bindp -> d_func =
			(i < FUNCLISTSIZ) ? (funcno_t)i : FNO_SETMACRO;
		i = 4;
	}

	if (argc > i + 1) {
		bindp -> key = ER_FEWMANYARG;
		bindp -> f_func = 0;
		return(-1);
	}
	else if (argc > i) {
		if (argv[i][0] != BINDCOMMENT
		|| bindp -> key < K_F(1) || bindp -> key > K_F(MAXHELPINDEX)) {
			bindp -> key = ER_SYNTAXERR;
			bindp -> f_func = (funcno_t)i;
			return(-1);
		}
	}

	return(n);
}

int addkeybind(n, bindp, func1, func2, cp)
int n;
CONST bindtable *bindp;
char *func1, *func2, *cp;
{
	bindtable bind;
	int n1, n2;

	memcpy((char *)&bind, (char *)bindp, sizeof(bind));
	if (bind.f_func != FNO_SETMACRO) Xfree(func1);
	else {
		n1 = setmacro(func1);
#ifndef	DEP_DYNAMICLIST
		if (n1 < 0) {
			Xfree(func1);
			Xfree(func2);
			Xfree(cp);
			return(-1);
		}
#endif
		bind.f_func = (funcno_t)n1;
	}

	if (bind.d_func != FNO_SETMACRO) Xfree(func2);
	else {
		n2 = setmacro(func2);
#ifndef	DEP_DYNAMICLIST
		if (n2 < 0) {
			freemacro(bind.f_func);
			Xfree(func2);
			Xfree(cp);
			return(-2);
		}
#endif
		bind.d_func = (funcno_t)n2;
	}

	if (bind.key < K_F(1) || bind.key > K_F(MAXHELPINDEX)) Xfree(cp);
	else if (cp) {
		Xfree(helpindex[bind.key - K_F(1)]);
		helpindex[bind.key - K_F(1)] = cp;
	}

	if (n < maxbind) {
		n1 = ffunc(n);
		n2 = dfunc(n);
	}
	else {
		maxbind = n + 1;
#ifdef	DEP_DYNAMICLIST
		bindlist = (bindlist_t)Xrealloc(bindlist,
			maxbind * sizeof(*bindlist));
#endif
		n1 = n2 = -1;
	}

	memcpy((char *)&(bindlist[n]), (char *)&bind, sizeof(*bindlist));
	freemacro(n1);
	freemacro(n2);

	return(0);
}

VOID deletekeybind(n)
int n;
{
	if (n >= maxbind) return;
	freemacro(ffunc(n));
	freemacro(dfunc(n));
	if (bindlist[n].key >= K_F(1)
	&& bindlist[n].key <= K_F(MAXHELPINDEX)) {
		Xfree(helpindex[bindlist[n].key - K_F(1)]);
		helpindex[bindlist[n].key - K_F(1)] = Xstrdup(nullstr);
	}
	memmove((char *)&(bindlist[n]), (char *)&(bindlist[n + 1]),
		(--maxbind - n) * sizeof(*bindlist));
}

static int NEAR setkeybind(argc, argv)
int argc;
char *CONST argv[];
{
	bindtable bind;
	char *cp, *func1, *func2;
	int n, no;

	if ((no = parsekeybind(argc, argv, &bind)) < 0) {
		builtinerror(argv,
			(bind.f_func) ? argv[bind.f_func] : NULL, bind.key);
		return(-1);
	}

	if (bind.f_func == FNO_NONE) {
		if (no >= maxbind) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		deletekeybind(no);
#ifdef	DEP_PTY
		sendparent(TE_DELETEKEYBIND, no);
#endif

		return(0);
	}

	func1 = (bind.f_func == FNO_SETMACRO) ? Xstrdup(argv[2]) : NULL;
	func2 = (bind.d_func == FNO_SETMACRO) ? Xstrdup(argv[3]) : NULL;
	n = (bind.d_func == FNO_NONE) ? 3 : 4;
	cp = (argc > n) ? Xstrdup(&(argv[n][1])) : NULL;

	if ((n = addkeybind(no, &bind, func1, func2, cp)) < 0) {
		builtinerror(argv, argv[1 - n], ER_OUTOFLIMIT);
		return(-1);
	}
#ifdef	DEP_PTY
	sendparent(TE_ADDKEYBIND, no, &bind, func1, func2, cp);
#endif

	return(0);
}

char *gethelp(bindp)
CONST bindtable *bindp;
{
	int n;

	if (bindp -> key < K_F(1) || bindp -> key > K_F(MAXHELPINDEX))
		return(NULL);
	n = bindp -> key - K_F(1);
#ifndef	_NOCUSTOMIZE
	if (!strcmp(helpindex[n], orighelpindex[n])) return(NULL);
#endif

	return(helpindex[n]);
}

VOID printmacro(list, n, isset, fp)
CONST bindtable *list;
int n, isset;
XFILE *fp;
{
	char *cp;

	VOID_C Xfprintf(fp, "%s ", BL_BIND);
	fputsmeta(getkeysym(list[n].key, 0), fp);
	if (isset) {
		Xfputc('\t', fp);

		if (list[n].f_func < FUNCLISTSIZ)
			Xfputs(funclist[list[n].f_func].ident, fp);
		else if (ismacro(list[n].f_func))
			fputsmeta(getmacro(list[n].f_func), fp);
		else Xfputs("\"\"", fp);
		if (list[n].d_func < FUNCLISTSIZ)
			VOID_C Xfprintf(fp,
				"\t%s", funclist[list[n].d_func].ident);
		else if (ismacro(list[n].d_func)) {
			Xfputc('\t', fp);
			fputsmeta(getmacro(list[n].d_func), fp);
		}
		if ((cp = gethelp(&(list[n]))))
			VOID_C Xfprintf(fp, "\t%c%k", BINDCOMMENT, cp);
	}
	VOID_C fputnl(fp);
}

static int NEAR printbind(argc, argv)
int argc;
char *CONST argv[];
{
	int i, c;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	hitkey(2);
	if (argc <= 1)
	for (i = 0; i < maxbind; i++) {
		if (!ismacro(ffunc(i)) && !ismacro(dfunc(i))) continue;
		printmacro(bindlist, i, 1, Xstdout);
		hitkey(0);
	}
	else if ((c = getkeycode(argv[1], 0)) < 0) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	else {
		for (i = 0; i < maxbind; i++) if (c == bindlist[i].key) break;
		if (i >= maxbind) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		printmacro(bindlist, i, 1, Xstdout);
	}

	return(0);
}

#ifdef	DEP_DOSEMU
/*ARGSUSED*/
int searchdrv(devp, list, max, isset)
CONST devinfo *devp, *list;
int max, isset;
{
	int i;

	for (i = 0; i < max; i++) {
		if (devp -> drive != list[i].drive) continue;
# ifdef	HDDMOUNT
		if (!list[i].cyl && isset) break;
# endif
		if (devp -> head == list[i].head
		&& devp -> sect == list[i].sect
		&& devp -> cyl == list[i].cyl
		&& !strpathcmp(devp -> name, list[i].name))
			break;
	}

	return(i);
}

int deletedrv(no)
int no;
{
# ifdef	HDDMOUNT
	CONST char *dev;
	int s;
# endif
	int i, n;

	if (no >= maxfdtype) return(no);
	n = 1;
# ifdef	HDDMOUNT
	if (!fdtype[no].cyl) {
		dev = fdtype[no].name;
		for (s = 0; s < no; s++) {
			if (fdtype[no - s - 1].cyl
			|| fdtype[no - s - 1].drive + 1 != fdtype[no - s].drive
			|| strpathcmp(dev, fdtype[no - s - 1].name))
				break;
		}
		for (n = 1; fdtype[no + n].name; n++) {
			if (fdtype[no + n].cyl
			|| fdtype[no + n].drive != fdtype[no + n - 1].drive + 1
			|| strpathcmp(dev, fdtype[no + n].name))
				break;
		}
		no -= s;
		n += s;
	}
# endif	/* HDDMOUNT */
	for (i = 0; i < n; i++) Xfree(fdtype[no + i].name);
	memmove((char *)&(fdtype[no]), (char *)&(fdtype[no + n]),
		(--maxfdtype - no - n) * sizeof(*fdtype));

	return(no);
}

int insertdrv(no, devp)
int no;
devinfo *devp;
{
# ifdef	HDDMOUNT
	l_off_t *sp;
	char *drvlist;
	int j, drive;
# endif
	int i, n, min, max;

	min = -1;
# ifdef	FAKEUNINIT
	max = -1;	/* fake for -Wuninitialized */
# endif
	for (i = 0; i < maxfdtype; i++) {
		if (strpathcmp(devp -> name, fdtype[i].name)) continue;
		if (min < 0) min = i;
		max = i;
	}
	if (min < 0) {
		if (no > 0 && no < maxfdtype
		&& !strpathcmp(fdtype[no - 1].name, fdtype[no].name))
			no = maxfdtype;
	}
	else if (no < min) no = min;
	else if (no > max + 1) no = max + 1;

	n = 1;
# ifdef	HDDMOUNT
	sp = NULL;
	if (!(devp -> cyl)) {
		if (!(sp = readpt(devp -> name, devp -> sect))) return(-1);
		for (n = 0; sp[n + 1]; n++) /*EMPTY*/;
		if (!n) {
			Xfree(sp);
			return(-1);
		}
#  ifndef	DEP_DYNAMICLIST
		else if (n >= MAXDRIVEENTRY - no) {
			Xfree(sp);
			return(-2);
		}
#  endif
		devp -> sect = sp[0];

		drvlist = Xmalloc(n);
		drvlist[0] = devp -> drive;
		for (i = 0; i < n - 1; i++) {
			for (drive = drvlist[i] + 1; drive <= 'Z'; drive++) {
				for (j = 0; j < maxfdtype; j++)
					if (drive == fdtype[j].drive) break;
				if (j >= maxfdtype) break;
			}
			if (drive > 'Z') break;
			drvlist[i + 1] = drive;
		}
		if (i < n - 1) {
			Xfree(sp);
			Xfree(drvlist);
			return(-2);
		}
	}
#  ifdef	FAKEUNINIT
	else drvlist = NULL;		/* fake for -Wuninitialized */
#  endif
# endif	/* HDDMOUNT */

	maxfdtype += n;
# ifdef	DEP_DYNAMICLIST
	fdtype = (fdtype_t)Xrealloc(fdtype, maxfdtype * sizeof(*fdtype));
# endif
	memmove((char *)&(fdtype[maxfdtype - 1]),
		(char *)&(fdtype[maxfdtype - n - 1]),
		(maxfdtype - n - no) * sizeof(*fdtype));

# ifdef	HDDMOUNT
	if (!(devp -> cyl)) {
		for (i = 0; i < n; i++) {
			memcpy((char *)&(fdtype[no + i]), (char *)devp,
				sizeof(*fdtype));
			fdtype[no + i].drive = drvlist[i];
			fdtype[no + i].name = Xstrdup(devp -> name);
			fdtype[no + i].offset = sp[i + 1];
		}
		Xfree(sp);
		Xfree(drvlist);
		Xfree(devp -> name);
		return(no);
	}
	fdtype[no].offset = 0;
# endif	/* HDDMOUNT */
	memcpy((char *)&(fdtype[no]), (char *)devp, sizeof(*fdtype));
	fdtype[no].name = devp -> name;

	return(no);
}

int parsesetdrv(argc, argv, devp)
int argc;
char *CONST argv[];
devinfo *devp;
{
# if	FD < 2
	char *cp, *tmp;
	u_char c;
# endif
	int head, sect, cyl;

	if (argc <= 3) {
		devp -> drive = ER_FEWMANYARG;
		devp -> head = 0;
		return(-1);
	}
# if	FD >= 2
	if (argc >= 7) {
		devp -> drive = ER_FEWMANYARG;
		devp -> head = 0;
		return(-1);
	}
# endif

	devp -> drive = Xtoupper(argv[1][0]);
	devp -> name = argv[2];
	if (!Xisalpha(devp -> drive)) {
		devp -> drive = ER_SYNTAXERR;
		devp -> head = 1;
		return(-1);
	}

# ifdef	HDDMOUNT
	if (!strncmp(argv[3], STRHDD, strsize(STRHDD))) {
		if (argc > 5) {
			devp -> drive = ER_FEWMANYARG;
			devp -> head = 0;
			return(-1);
		}
		cyl = 0;
		if (!argv[3][3]) sect = 0;
		else if (!strcmp(&(argv[3][3]), STRHDD98)) sect = 98;
		else {
			devp -> drive = ER_SYNTAXERR;
			devp -> head = 3;
			return(-1);
		}
		if (argc < 5) head = 'n';
		else if (!strcmp(argv[4], "ro")) head = 'r';
		else if (!strcmp(argv[4], "rw")) head = 'w';
		else {
			devp -> drive = ER_SYNTAXERR;
			devp -> head = 4;
			return(-1);
		}
		if (sect) head = Xtoupper(head);
	}
	else
# endif	/* HDDMOUNT */
# if	FD >= 2
	if (argc != 6) {
		devp -> drive = ER_FEWMANYARG;
		devp -> head = 0;
		return(-1);
	}
	else if ((head = Xatoi(argv[3])) <= 0) {
		devp -> drive = ER_SYNTAXERR;
		devp -> head = 3;
		return(-1);
	}
	else if (head > MAXUTYPE(u_char)) {
		devp -> drive = ER_OUTOFLIMIT;
		devp -> head = 3;
		return(-1);
	}
	else if ((sect = Xatoi(argv[4])) <= 0) {
		devp -> drive = ER_SYNTAXERR;
		devp -> head = 4;
		return(-1);
	}
	else if (sect > MAXUTYPE(u_short)) {
		devp -> drive = ER_OUTOFLIMIT;
		devp -> head = 4;
		return(-1);
	}
	else if ((cyl = Xatoi(argv[5])) <= 0) {
		devp -> drive = ER_SYNTAXERR;
		devp -> head = 5;
		return(-1);
	}
	else if (cyl > MAXUTYPE(u_short)) {
		devp -> drive = ER_OUTOFLIMIT;
		devp -> head = 5;
		return(-1);
	}
# else	/* FD < 2 */
	{
		cp = tmp = catvar(&(argv[3]), '\0');

		if (!(cp = Xsscanf(cp, "%+Cu%c", &c, DRIVESEP))) {
			devp -> drive = ER_SYNTAXERR;
			devp -> head = (u_char)-1;
			devp -> name = tmp;
			return(-1);
		}
		head = c;
		if (!(cp = Xsscanf(cp, "%+Cu%c", &c, DRIVESEP))) {
			devp -> drive = ER_SYNTAXERR;
			devp -> head = (u_char)-1;
			devp -> name = tmp;
			return(-1);
		}
		sect = c;
		if (!Xsscanf(cp, "%+Cu%$", &c)) {
			devp -> drive = ER_SYNTAXERR;
			devp -> head = (u_char)-1;
			devp -> name = tmp;
			return(-1);
		}
		cyl = c;
		if (head > MAXUTYPE(u_char)
		|| sect > MAXUTYPE(u_short)
		|| cyl > MAXUTYPE(u_short)) {
			devp -> drive = ER_OUTOFLIMIT;
			devp -> head = (u_char)-1;
			devp -> name = tmp;
			return(-1);
		}
		Xfree(tmp);
	}
# endif	/* FD < 2 */
	devp -> head = head;
	devp -> sect = sect;
	devp -> cyl = cyl;

	return(0);
}

static int NEAR _setdrive(argc, argv, isset)
int argc;
char *CONST argv[];
int isset;
{
	devinfo dev;
	int i, n;

	if (parsesetdrv(argc, argv, &dev) < 0) {
# if	FD < 2
		if (dev.head == (u_char)-1) {
			builtinerror(argv, dev.name, dev.drive);
			Xfree(dev.name);
		}
		else
# endif
		builtinerror(argv,
			(dev.head) ? argv[dev.head] : NULL, dev.drive);
		return(-1);
	}

# ifdef	HDDMOUNT
	if (!(dev.cyl)) {
		for (i = 0; i < maxfdtype; i++)
			if (dev.drive == fdtype[i].drive) break;
	}
	else
# endif
	i = searchdrv(&dev, fdtype, maxfdtype, isset);

	if (!isset) {
		if (i >= maxfdtype) {
			builtinerror(argv, argv[2], ER_NOENTRY);
			return(-1);
		}
# ifdef	HDDMOUNT
		if (!(dev.cyl)
		&& (fdtype[i].cyl || strpathcmp(argv[2], fdtype[i].name))) {
			builtinerror(argv, argv[2], ER_NOENTRY);
			return(-1);
		}
# endif
		VOID_C deletedrv(i);
# ifdef	DEP_PTY
		sendparent(TE_DELETEDRV, i);
# endif
	}
	else {
		if (i < maxfdtype) {
			builtinerror(argv, argv[1], ER_EXIST);
			return(-1);
		}
# ifndef	DEP_DYNAMICLIST
		if (i >= MAXDRIVEENTRY - 1) {
			builtinerror(argv, NULL, ER_OUTOFLIMIT);
			return(-1);
		}
# endif

		dev.name = Xstrdup(dev.name);
		n = insertdrv(i, &dev);
		if (n < -1) {
			builtinerror(argv, NULL, ER_OUTOFLIMIT);
			Xfree(dev.name);
			return(-1);
		}
		if (!inruncom && n < 0) {
			builtinerror(argv, argv[2], ER_INVALDEV);
			Xfree(dev.name);
			return(-1);
		}
# ifdef	DEP_PTY
		sendparent(TE_INSERTDRV, i, &dev);
# endif
	}

	return(0);
}

static int NEAR setdrive(argc, argv)
int argc;
char *CONST argv[];
{
	return(_setdrive(argc, argv, 1));
}

static int NEAR unsetdrive(argc, argv)
int argc;
char *CONST argv[];
{
	return(_setdrive(argc, argv, 0));
}

/*ARGSUSED*/
VOID printsetdrv(fdlist, n, isset, verbose, fp)
CONST devinfo *fdlist;
int n, isset, verbose;
XFILE *fp;
{
	VOID_C Xfprintf(fp,
		"%s %c\t", (isset) ? BL_SDRIVE : BL_UDRIVE, fdlist[n].drive);

	fputsmeta(fdlist[n].name, fp);
	Xfputc('\t', fp);
# ifdef	HDDMOUNT
	if (!fdlist[n].cyl) {
		Xfputs(STRHDD, fp);
		if (Xisupper(fdlist[n].head)) Xfputs(STRHDD98, fp);
		if (verbose) VOID_C Xfprintf(fp,
			" #offset=%'Ld", fdlist[n].offset / fdlist[n].sect);
	}
	else
# endif	/* HDDMOUNT */
	VOID_C Xfprintf(fp,
		"%d%c%d%c%d\n",
		(int)(fdlist[n].head), DRIVESEP,
		(int)(fdlist[n].sect), DRIVESEP, (int)(fdlist[n].cyl));
}

static int NEAR printdrive(argc, argv)
int argc;
char *CONST argv[];
{
	int i, n;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	hitkey(2);
	if (argc <= 1) for (i = 0; i < maxfdtype; i++) {
		printsetdrv(fdtype, i, 1, 1, Xstdout);
		hitkey(0);
	}
	else {
		for (i = n = 0; i < maxfdtype; i++)
		if (Xtoupper(argv[1][0]) == fdtype[i].drive) {
			n++;
			printsetdrv(fdtype, i, 1, 1, Xstdout);
			hitkey(0);
		}
		if (!n) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
	}

	return(0);
}
#endif	/* DEP_DOSEMU */

#ifndef	_NOKEYMAP
VOID printkeymap(kp, isset, fp)
keyseq_t *kp;
int isset;
XFILE *fp;
{
	char *cp;

	VOID_C Xfprintf(fp, "%s ", BL_KEYMAP);
	fputsmeta(getkeysym(kp -> code, 1), fp);
	if (isset) {
		Xfputc('\t', fp);

		cp = encodestr(kp -> str, kp -> len);
		fputsmeta(cp, fp);
		Xfree(cp);
	}
	VOID_C fputnl(fp);
}

int parsekeymap(argc, argv, kp)
int argc;
char *CONST argv[];
keyseq_t *kp;
{
	kp -> code = (short)-1;
	kp -> len = (u_char)0;
	kp -> str = NULL;

	if (argc >= 4) {
		kp -> code = ER_FEWMANYARG;
		kp -> len = (u_char)0;
		return(-1);
	}

	if (argc <= 1) return(0);
	if ((kp -> code = getkeycode(argv[1], 1)) < 0) {
		kp -> code = ER_SYNTAXERR;
		kp -> len = (u_char)1;
		return(-1);
	}
	if (argc == 2) return(0);

	if (!(argv[2][0])) {
		kp -> len = (u_char)-1;
		return(0);
	}

	kp -> str = decodestr(argv[2], &(kp -> len), 1);
	if (!(kp -> len)) {
		Xfree(kp -> str);
		kp -> code = ER_SYNTAXERR;
		kp -> len = (u_char)2;
		return(-1);
	}

	return(0);
}

static int NEAR setkeymap(argc, argv)
int argc;
char *CONST argv[];
{
	keyseq_t key;
	int i;

	if (parsekeymap(argc, argv, &key) < 0) {
		builtinerror(argv, (key.len) ? argv[key.len] : NULL, key.code);
		return(-1);
	}

	if (key.code < 0) {
		hitkey(2);
		for (i = 1; i <= 20; i++) {
			key.code = K_F(i);
			if (getkeyseq(&key) < 0 || !(key.len)) continue;
			printkeymap(&key, 1, Xstdout);
			hitkey(0);
		}
		for (i = 0; (key.code = keyidentlist[i].no) > 0; i++) {
			if (getkeyseq(&key) < 0 || !(key.len)) continue;
			printkeymap(&key, 1, Xstdout);
			hitkey(0);
		}
		return(0);
	}

	if (!(key.len)) {
		if (getkeyseq(&key) < 0 || !(key.len)) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		hitkey(2);
		printkeymap(&key, 1, Xstdout);
		return(0);
	}
	if (!(key.str)) {
		setkeyseq(key.code, NULL, 0);
# ifdef	DEP_PTY
		key.len = (u_char)0;
		sendparent(TE_SETKEYSEQ, &key);
# endif
		return(0);
	}

	setkeyseq(key.code, key.str, key.len);
# ifdef	DEP_PTY
	sendparent(TE_SETKEYSEQ, &key);
# endif

	return(0);
}

static int NEAR keytest(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp, buf[2];
	int i, n, ch, next;

	if (dumbterm > 1) {
		builtinerror(argv, NULL, ER_NOTDUMBTERM);
		return(-1);
	}
	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}

	n = (argc >= 2) ? Xatoi(argv[1]) : 1;
	if (n < 0) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}

	hitkey(2);
	ttyiomode(1);
	i = 0;
	for (;;) {
		kanjiputs(GETKY_K);
		if (n != 1) kanjiputs(SPCED_K);
		tflush();

		next = 0;
		while (!kbhit2(1000000L / SENSEPERSEC));
		Xcputs("\r\"");
		putterm(L_CLEAR);
		buf[1] = '\0';
		do {
			if ((ch = Xgetch()) == EOF) break;
			next = kbhit2(WAITKEYPAD * 1000L);
			buf[0] = ch;
			cp = encodestr(buf, 1);
			Xcputs(cp);
			Xfree(cp);
		} while (next);
		VOID_C Xputch('"');
		cputnl();
		if (n) {
			if (++i >= n) break;
		}
		if (n != 1 && ch == ' ') break;
	}
	stdiomode();

	return(0);
}
#endif	/* !_NOKEYMAP */

static int NEAR printhist(argc, argv)
int argc;
char *CONST argv[];
{
	int i, n, max, size;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	size = (int)histsize[0];
	if (argc < 2) n = size;
	else if ((n = Xatoi(argv[1])) < 1) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	else if (n > size) n = size;

	if (!history[0]) return(0);
	if (--n > (int)MAXHISTNO) n = (int)MAXHISTNO;
	for (max = n; max >= 0; max--) if (history[0][max]) break;
	if (max < (int)histno[0]) n = (int)histno[0] - max - 1;
	else n = (int)MAXHISTNO - (max - (int)histno[0]);

	hitkey(2);
	for (i = max; i >= 0; i--) {
		VOID_C Xprintf("%5d  %k\n", n + 1, history[0][i]);
		if (n++ >= (int)MAXHISTNO) n = 0;
		hitkey(0);
	}

	return(0);
}

#if	FD >= 2
# ifndef	NOPOSIXUTIL
static int NEAR fixcommand(argc, argv)
int argc;
char *CONST argv[];
{
	XFILE *fp;
	CONST char *r, *editor;
	char *cp, *s, *tmp, path[MAXPATHLEN];
	int i, j, n, f, fd, l, l1, l2, len, max, skip, list, nonum, rev, exe;

	editor = NULL;
	skip = list = nonum = rev = exe = 0;
	for (n = 1; n < argc && argv[n][0] == '-'; n++) {
		skip = 0;
		for (i = 1; argv[n][i]; i++) {
			skip = 0;
			switch (argv[n][i]) {
				case 'e':
					if (argv[n][i + 1]) {
						editor = &(argv[n][i + 1]);
						skip = 1;
					}
					else if (n + 1 < argc) {
						editor = argv[++n];
						skip = 1;
					}
					else skip = -1;
					break;
				case 'l':
					list = 1;
					break;
				case 'n':
					nonum = 1;
					break;
				case 'r':
					rev = 1;
					break;
				case 's':
					exe = 1;
					break;
				default:
					skip = -1;
					break;
			}
			if (skip) {
				skip--;
				break;
			}
		}
		if (skip) {
			skip--;
			break;
		}
	}
	if (skip) {
		VOID_C Xfprintf(Xstderr,
	"%k: usage: %k [-ls] [-nr] [-e editor] [old=new] [first] [last]\n",
			argv[0], argv[0]);
		return(-1);
	}

	if (!history[0]) return(0);
	tmp = removehist(0);
	if (exe) {
		i = n;
		for (; n < argc; n++) if (!Xstrchr(argv[n], '=')) break;
		f = parsehist((n < argc) ? argv[n] : "!", NULL, '\0');
		if (f < 0) {
			builtinerror(argv, argv[n], ER_EVENTNOFOUND);
			entryhist(tmp, HST_COMM);
			Xfree(tmp);
			return(-1);
		}
		Xfree(tmp);
		s = Xstrdup(history[0][f]);
		max = len = strlen(s);
		for (; i < n; i++) {
			if (!(r = Xstrchr(argv[i], '='))) continue;
			l1 = r - argv[i];
			for (cp = s; (cp = Xstrchr(cp, argv[i][0])); cp++)
				if (!strncmp(cp, argv[i], l1)) break;
			if (!cp) continue;
			r++;
			l2 = strlen(r);
			len += l2 - l1;
			if (len > max) {
				max = len;
				j = cp - s;
				s = Xrealloc(s, max + 1);
				cp = s + j;
			}
			memmove(&(cp[l2]), &(cp[l1]),
				&(s[len]) - &(cp[l2]) + 1);
			memcpy(cp, r, l2);
		}
		kanjifputs(s, Xstdout);
		VOID_C fputnl(Xstdout);
		entryhist(s, HST_COMM);
		n = execmacro(s, NULL,
			F_NOCONFIRM | F_ARGSET | F_IGNORELIST);
		if (n < 0) n = 0;
		Xfree(s);
		return(n);
	}

	if (!editor) editor = getenv2("FD_FCEDIT");
	if (!editor) editor = getenv2("FD_EDITOR");
	if (!editor) editor = EDITOR;

	if (list) {
		f = parsehist((n < argc) ? argv[n] : "-16", NULL, '\0');
		l = parsehist((n + 1 < argc) ? argv[n + 1] : "!", NULL, '\0');
	}
	else {
		f = parsehist((n < argc) ? argv[n] : "!", NULL, '\0');
		l = (n + 1 < argc) ? parsehist(argv[n + 1], NULL, '\0') : f;
	}
	if (f < 0 || l < 0) {
		if (f < 0) builtinerror(argv, argv[n], ER_EVENTNOFOUND);
		else builtinerror(argv,
			(n + 1 < argc) ? argv[n + 1] : NULL, ER_EVENTNOFOUND);
		entryhist(tmp, HST_COMM);
		Xfree(tmp);
		return(-1);
	}

	if (rev) {
		n = f;
		f = l;
		l = n;
	}

	if (f < (int)histno[0]) n = (int)histno[0] - f - 1;
	else n = (int)MAXHISTNO - (f - (int)histno[0]);

	if (list) {
		fp = Xstdout;
		hitkey(2);
	}
	else {
		if ((fd = mktmpfile(path)) < 0) {
			builtinerror(argv, argv[0], -1);
			Xfree(tmp);
			return(-1);
		}
		if (!(fp = Xfdopen(fd, "w"))) {
			builtinerror(argv, path, -1);
			VOID_C Xclose(fd);
			rmtmpfile(path);
			Xfree(tmp);
			return(-1);
		}
		nonum = 1;
	}

	if (f >= l) for (i = f; i >= l; i--) {
		if (history[0][i]) {
			if (!nonum) {
				VOID_C Xfprintf(fp, "%5d  ", n + 1);
				if (n++ >= (int)MAXHISTNO) n = 0;
			}
			kanjifputs(history[0][i], fp);
			VOID_C fputnl(fp);
#  ifndef	DEP_ORIGSHELL
			if (list) hitkey(0);
#  endif
		}
	}
	else for (i = f; i <= l; i++) {
		if (history[0][i]) {
			if (!nonum) {
				VOID_C Xfprintf(fp, "%5d  ", n + 1);
				if (--n < 0) n = (int)MAXHISTNO;
			}
			kanjifputs(history[0][i], fp);
			VOID_C fputnl(fp);
#  ifndef	DEP_ORIGSHELL
			if (list) hitkey(0);
#  endif
		}
	}

	if (list) {
		Xfflush(Xstdout);
		entryhist(tmp, HST_COMM);
		Xfree(tmp);
		return(0);
	}

	VOID_C Xfclose(fp);
	n = execmacro(editor, path, F_NOCONFIRM | F_IGNORELIST);
	if (n < 0) n = 0;
	if (n) {
		builtinerror(argv, editor, -1);
		rmtmpfile(path);
		Xfree(tmp);
		return(-1);
	}

	if (!(fp = Xfopen(path, "r"))) {
		builtinerror(argv, path, -1);
		rmtmpfile(path);
		Xfree(tmp);
		return(-1);
	}
	Xfree(tmp);

	for (; (cp = Xfgets(fp)); Xfree(cp)) {
		if (!*cp) continue;
		kanjifputs(cp, Xstdout);
		VOID_C fputnl(Xstdout);
		entryhist(cp, HST_COMM);
		n = execmacro(cp, NULL,
			F_NOCONFIRM | F_ARGSET | F_IGNORELIST);
	}
	VOID_C Xfclose(fp);
	rmtmpfile(path);

	return(n);
}
# endif	/* !NOPOSIXUTIL */

static int NEAR printmd5(path, fp)
CONST char *path;
XFILE *fp;
{
	XFILE *fpin;
	ALLOC_T size;
	u_char buf[MD5_BUFSIZ * 4];
	int i, n;

	if (!(fpin = Xfopen(path, "rb"))) return(-1);
	size = sizeof(buf);
	n = md5fencode(buf, &size, fpin);
	VOID_C Xfclose(fpin);
	if (n < 0) return(-1);

	VOID_C Xfprintf(fp, "MD5 (%k) = ", path);
	for (i = 0; i < size; i++) VOID_C Xfprintf(fp, "%02x", buf[i]);

	return(fputnl(fp));
}

static int NEAR md5sum(argc, argv)
int argc;
char *CONST argv[];
{
	char path[MAXPATHLEN];
	int i, n;

	hitkey(2);
	if (argc < 2) {
# if	MSDOS || defined (CYGWIN)
		Xstrcpy(strcatdelim2(path, progpath, progname), ".EXE");
# else
		strcatdelim2(path, progpath, progname);
# endif
		if (printmd5(path, Xstdout) < 0) {
			builtinerror(argv, path, -1);
			return(-1);
		}
		return(0);
	}

	n = 0;
	for (i = 1; i < argc; i++) {
		if (printmd5(argv[i], Xstdout) < 0) {
			builtinerror(argv, argv[i], -1);
			n = -1;
			continue;
		}
		hitkey(0);
	}

	return(n);
}

static int NEAR evalmacro(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp;
	int n;

	if (argc <= 1 || !(cp = catvar(&(argv[1]), ' '))) return(0);
	n = execmacro(cp, NULL, F_NOCONFIRM | F_ARGSET | F_NOKANJICONV);
	if (n < 0) n = 0;
	Xfree(cp);

	return(n);
}

# ifdef	DEP_KCONV
static int NEAR kconv(argc, argv)
int argc;
char *CONST argv[];
{
	XFILE *fpin, *fpout;
	char *cp;
	int i, in, out, err;

	err = 0;
	in = out = DEFCODE;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') break;
		if (argv[i][1] == '-' && !argv[i][2]) {
			i++;
			break;
		}
		switch (argv[i][1]) {
			case 'i':
				if (argv[i][2]) cp = &(argv[i][2]);
				else if (!(cp = argv[++i]) || !*cp) {
					err = 1;
					break;
				}
				if ((in = getlang(cp, L_FNAME)) == NOCNV)
					in = DEFCODE;
				break;
			case 'o':
				if (argv[i][2]) cp = &(argv[i][2]);
				else if (!(cp = argv[++i]) || !*cp) {
					err = 1;
					break;
				}
				if ((out = getlang(cp, L_FNAME)) == NOCNV)
					out = DEFCODE;
				break;
			default:
				err = 1;
				break;
		}
		if (err) {
			VOID_C Xfprintf(Xstderr,
		"Usage: %s [-i inputcode] [-o outputcode] [filename]\n",
				argv[0]);
			return(-1);
		}
	}

	if (i >= argc || !*(argv[i])) {
		fpin = Xstdin;
		fpout = Xstdout;
	}
	else if (!(fpin = Xfopen(argv[i], "r"))) {
		builtinerror(argv, argv[i], -1);
		return(-1);
	}
	else if (i + 1 >= argc || !*(argv[i + 1])) fpout = Xstdout;
	else if (!(fpout = Xfopen(argv[i + 1], "w"))) {
		builtinerror(argv, argv[i + 1], -1);
		VOID_C Xfclose(fpin);
		return(-1);
	}

#  ifdef	DEP_UNICODE
	if ((i = (in > out) ? in : out) >= UTF8) readunitable(i - UTF8);
#  endif
	while ((cp = Xfgets(fpin))) {
		if (in != DEFCODE) renewkanjiconv(&cp, in, DEFCODE, L_OUTPUT);
		renewkanjiconv(&cp, DEFCODE, out, L_OUTPUT);
		Xfputs(cp, fpout);
		VOID_C fputnl(fpout);
		Xfree(cp);
	}

#  ifdef	DEP_UNICODE
	if (!unicodebuffer) discardunitable();
#  endif
	if (fpin != Xstdin) VOID_C Xfclose(fpin);
	else Xclearerr(fpin);
	if (fpout != Xstdout) VOID_C Xfclose(fpout);

	return(0);
}
# endif	/* DEP_KCONV */

static int NEAR getinputstr(argc, argv)
int argc;
char *CONST argv[];
{
	CONST char *duppromptstr;
	char *s;
	int wastty;

	if (dumbterm > 1) {
		builtinerror(argv, NULL, ER_NOTDUMBTERM);
		return(-1);
	}
	if (!(wastty = isttyiomode)) ttyiomode(1);
	duppromptstr = promptstr;
	if (argc >= 2) promptstr = argv[1];
	else promptstr = nullstr;
	s = inputstr(NULL, 0, -1, NULL, -1);
	promptstr = duppromptstr;
	if (!wastty) stdiomode();
	if (!s) return(-1);

	kanjifputs(s, Xstdout);
	VOID_C fputnl(Xstdout);
	Xfree(s);

	return(0);
}

static int NEAR getyesno(argc, argv)
int argc;
char *CONST argv[];
{
	CONST char *s;
	int n, wastty;

	if (dumbterm > 1) {
		builtinerror(argv, NULL, ER_NOTDUMBTERM);
		return(-1);
	}
	if (argc < 2) s = nullstr;
	else s = argv[1];

	if (!(wastty = isttyiomode)) ttyiomode(1);
	lcmdline = -1;
	n = yesno(s);
	if (!wastty) stdiomode();

	return((n) ? 0 : -1);
}
#endif	/* FD >= 2 */

#if	!MSDOS
int savestdio(reset)
int reset;
{
	int n;

	if (isttyiomode) return(0);

	n = savettyio(reset);
# ifdef	DEP_PTY
	sendparent(TE_SAVETTYIO, reset);
# endif
	return(n);
}

# if	FD >= 2
static int NEAR savetty(argc, argv)
int argc;
char *CONST argv[];
{
	int n;

	n = 0;
	if (argc > 1 && argv[1][0] == '-'
	&& argv[1][1] == 'n' && !(argv[1][2]))
		n++;

	return(savestdio(n));
}
# endif	/* FD >= 2 */
#endif	/* !MSDOS */

#ifdef	DEP_IME
static int NEAR setroman(argc, argv)
int argc;
char *CONST argv[];
{
	XFILE *fp;
	char *file, **args;
	int i, n, skip, clean;

	file = NULL;
	skip = clean = 0;
	for (n = 1; n < argc && argv[n][0] == '-'; n++) {
		skip = 0;
		for (i = 1; argv[n][i]; i++) {
			skip = 0;
			switch (argv[n][i]) {
				case 'c':
					clean = 2;
					break;
				case 'r':
					clean = 1;
					break;
				case 'f':
					if (argv[n][i + 1]) {
						file = &(argv[n][i + 1]);
						skip = 1;
					}
					else if (n + 1 < argc) {
						file = argv[++n];
						skip = 1;
					}
					else skip = -1;
					break;
				default:
					skip = -1;
					break;
			}
			if (skip) {
				skip--;
				break;
			}
		}
		if (skip) {
			skip--;
			break;
		}
	}
	if (skip || (!file && !clean && n >= argc)) {
		VOID_C Xfprintf(Xstderr,
			"%k: usage: %k [-c] [-r] [-f file] [roman [kanji]]]\n",
			argv[0], argv[0]);
		return(-1);
	}

	if (clean) {
		freeroman(clean - 1);
# ifdef	DEP_PTY
		sendparent(TE_FREEROMAN, clean - 1);
# endif
	}

	if (!file) fp = NULL;
	else if (!(fp = Xfopen(file, "r"))) {
		builtinerror(argv, file, -1);
		return(-1);
	}
	else for (;;) {
		args = file2argv(fp, argv[0], 0);
		i = 1;
		if (args[i] && !strcommcmp(args[i], BL_SETROMAN)) i++;

		if (!args[i]) /*EMPTY*/;
		else if (addroman(args[i], args[i + 1]) < 0)
			builtinerror(argv, args[i], ER_SYNTAXERR);
# ifdef	DEP_PTY
		else sendparent(TE_ADDROMAN, args[i], args[i + 1]);
# endif
		freevar(args);

		if (Xfeof(fp)) break;
	}
	if (fp) VOID_C Xfclose(fp);

	if (n >= argc) /*EMPTY*/;
	else if (addroman(argv[n], argv[n + 1]) < 0) {
		builtinerror(argv, argv[n], ER_SYNTAXERR);
		return(-1);
	}
# ifdef	DEP_PTY
	else sendparent(TE_ADDROMAN, argv[n], argv[n + 1]);
# endif

	return(0);
}

static VOID NEAR disproman(s, n, fp)
char *s;
int n;
XFILE *fp;
{
	char buf[2 + 1];
	int i;

	if (s && *s) VOID_C Xfprintf(fp, "%s ", s);
	VOID_C Xfprintf(fp, "%s \"", romanlist[n].str);
	for (i = 0; i < R_MAXKANA; i++) {
		if (!romanlist[n].code[i]) break;
		VOID_C jis2str(buf, romanlist[n].code[i]);
		VOID_C Xfprintf(fp, "%K", buf);
	}
	Xfputc('"', fp);
	VOID_C fputnl(fp);
}

static int NEAR printroman(argc, argv)
int argc;
char *CONST argv[];
{
	int i, n, ret;

	initroman();
	ret = -1;

	if (argc < 2) {
		for (i = 0; i < maxromanlist; i++)
			disproman(BL_SETROMAN, i, Xstdout);
		ret = 0;
	}
	else for (n = 1; n < argc; n++) {
		if (!*(argv[n])
		|| (i = searchroman(argv[n], strlen(argv[n]))) < 0) {
			builtinerror(argv, argv[n], ER_NOENTRY);
			continue;
		}
		disproman(BL_PRINTROMAN, i, Xstdout);
		ret = 0;
	}

	return(ret);
}
#endif	/* DEP_IME */

#ifndef	DEP_ORIGSHELL
static int NEAR printenv(argc, argv)
int argc;
char *CONST argv[];
{
# if	FD < 2
	int len;
# endif
	int i;

# if	FD >= 2
	if (argc >= 2) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
# else
	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}

	if (argc > 1) {
		hitkey(2);
		len = strlen(argv[1]);
		for (i = 0; environ2[i]; i++)
			if (!strnenvcmp(environ2[i], argv[1], len)
			&& environ2[i][len] == '=')
				break;
		if (!environ2[i]) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		kanjifputs(environ2[i], Xstdout);
		VOID_C fputnl(Xstdout);
		return(0);
	}
# endif

	hitkey(2);
	if (environ2) for (i = 0; environ2[i]; i++) {
		kanjifputs(environ2[i], Xstdout);
		VOID_C fputnl(Xstdout);
		hitkey(0);
	}
# if	FD >= 2
	for (i = 0; i < maxuserfunc; i++) {
		printfunction(i, 0, Xstdout);
		VOID_C fputnl(Xstdout);
		hitkey(0);
	}
# endif

	return(0);
}

int searchalias(ident, len)
CONST char *ident;
int len;
{
	int i;

	if (len < 0) len = strlen(ident);
	for (i = 0; i < maxalias; i++)
		if (!strncommcmp(ident, aliaslist[i].alias, len)
		&& !(aliaslist[i].alias[len]))
			break;

	return(i);
}

int addalias(ident, comm)
char *ident, *comm;
{
	int i;

	if ((i = searchalias(ident, -1)) >= MAXALIASTABLE) {
		Xfree(ident);
		Xfree(comm);
		return(-1);
	}
	else if (i < maxalias) {
		Xfree(aliaslist[i].alias);
		Xfree(aliaslist[i].comm);
	}
	else maxalias++;

	aliaslist[i].alias = ident;
	aliaslist[i].comm = comm;
# ifdef	COMMNOCASE
	Xstrtolower(ident);
# endif
# ifdef	DEP_PTY
	sendparent(TE_ADDALIAS, ident, comm);
# endif

	return(0);
}

int deletealias(ident)
CONST char *ident;
{
	reg_t *re;
	int i, n;

	n = 0;
	re = regexp_init(ident, -1);
	for (i = 0; i < maxalias; i++) {
		if (re) {
			if (!regexp_exec(re, aliaslist[i].alias, 0)) continue;
		}
		else if (strcommcmp(ident, aliaslist[i].alias)) continue;
		Xfree(aliaslist[i].alias);
		Xfree(aliaslist[i].comm);
		memmove((char *)&(aliaslist[i]), (char *)&(aliaslist[i + 1]),
			(--maxalias - i) * sizeof(*aliaslist));
		i--;
		n++;
	}
	regexp_free(re);
	if (!n) return(-1);
# ifdef	DEP_PTY
	sendparent(TE_DELETEALIAS, ident);
# endif

	return(0);
}

VOID printalias(n, fp)
int n;
XFILE *fp;
{
	VOID_C Xfprintf(fp, "%s %k%c", BL_ALIAS, aliaslist[n].alias, ALIASSEP);
	fputsmeta(aliaslist[n].comm, fp);
}

static int NEAR setalias(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp;
	int i, len;

# if	FD >= 2
	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
# else
	if (argc >= 4) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
# endif

	if (argc <= 1) {
		hitkey(2);
		for (i = 0; i < maxalias; i++) {
			printalias(i, Xstdout);
			VOID_C fputnl(Xstdout);
			hitkey(0);
		}
		return(0);
	}

# if	FD >= 2
	if (!(cp = gettoken(argv[1])) || (*cp && *cp != ALIASSEP)) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	len = cp - argv[1];
# else
	if (!(cp = gettoken(argv[1])) || *cp) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	len = strlen(argv[1]);
# endif

# if	FD >= 2
	if (*cp) cp++;
# else
	if (argc > 2) cp = argv[2];
# endif
	else {
		if ((i = searchalias(argv[1], len)) >= maxalias) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		hitkey(2);
		printalias(i, Xstdout);
		VOID_C fputnl(Xstdout);
		return(0);
	}

	if (addalias(Xstrndup(argv[1], len), Xstrdup(cp)) < 0) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		return(-1);
	}

	return(0);
}

/*ARGSUSED*/
static int NEAR unalias(argc, argv)
int argc;
char *CONST argv[];
{
	if (argc != 2) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	if (deletealias(argv[1]) < 0) {
		builtinerror(argv, argv[1], ER_NOENTRY);
		return(-1);
	}

	return(0);
}

int searchfunction(ident)
CONST char *ident;
{
	int i;

	for (i = 0; i < maxuserfunc; i++)
		if (!strcommcmp(ident, userfunclist[i].func)) break;

	return(i);
}

int addfunction(ident, comm)
char *ident, **comm;
{
	int i;

	if ((i = searchfunction(ident)) >= MAXFUNCTABLE) {
		Xfree(ident);
		freevar(comm);
		return(-1);
	}
	else if (i < maxuserfunc) {
		Xfree(userfunclist[i].func);
		freevar(userfunclist[i].comm);
	}
	else maxuserfunc++;

	userfunclist[i].func = ident;
	userfunclist[i].comm = comm;
# ifdef	COMMNOCASE
	Xstrtolower(ident);
# endif
# ifdef	DEP_PTY
	sendparent(TE_ADDFUNCTION, ident, comm);
# endif

	return(0);
}

int deletefunction(ident)
CONST char *ident;
{
	int i;

	if ((i = searchfunction(ident)) >= maxuserfunc) return(-1);

	Xfree(userfunclist[i].func);
	freevar(userfunclist[i].comm);
	memmove((char *)&(userfunclist[i]),
		(char *)&(userfunclist[i + 1]),
		(--maxuserfunc - i) * sizeof(*userfunclist));
# ifdef	DEP_PTY
	sendparent(TE_DELETEFUNCTION, ident);
# endif

	return(0);
}

VOID printfunction(no, verbose, fp)
int no, verbose;
XFILE *fp;
{
	int i;

# if	FD < 2
	VOID_C Xfprintf(fp, "%s ", BL_FUNCTION);
# endif
	VOID_C Xfprintf(fp, "%k() {", userfunclist[no].func);
	if (verbose) VOID_C fputnl(fp);
	for (i = 0; userfunclist[no].comm[i]; i++) {
		if (verbose) VOID_C Xfprintf(fp,
			"\t%k;\n", userfunclist[no].comm[i]);
		else VOID_C Xfprintf(fp, " %k;", userfunclist[no].comm[i]);
	}
	Xfputs(" }", fp);
}

static int NEAR checkuserfunc(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp;
	int i, n;

	n = 0;
	if (n >= argc) return(-1);
	if (!(cp = gettoken(argv[n]))) return(-1);
	if (!*cp) cp = argv[++n];

	if (!cp || *cp != '(') return(-1);
	*(cp++) = '\0';
	if (!*cp) cp = argv[++n];

	if (!cp || *(cp++) != ')') return(-1);
	if (!*cp) cp = argv[++n];

	if (!cp) return(0);
	if (*(cp++) != '{' || *cp) return(-2);

	if (n > 0) {
		for (i = 1; i <= n; i++) Xfree(argv[i]);
		memmove((char *)&(argv[1]), (char *)&(argv[n + 1]),
			(argc -= n) * sizeof(*argv));
	}

	return(argc);
}

static int NEAR setuserfunc(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp, *tmp, *line, **var, *list[MAXFUNCLINES + 1];
	int i, j, n;

	if (argc <= 1) {
# if	FD >= 2
		builtinerror(argv, NULL, ER_SYNTAXERR);
		return(-1);
# else	/* FD < 2 */
		hitkey(2);
		for (i = 0; i < maxuserfunc; i++) {
			printfunction(i, 0, Xstdout);
			VOID_C fputnl(Xstdout);
			hitkey(0);
		}
		return(0);
# endif	/* FD < 2 */
	}

# if	FD < 2
	if ((n = checkuserfunc(argc - 1, &(argv[FUNCNAME]))) < 0) {
		builtinerror(argv, argv[FUNCNAME], ER_SYNTAXERR);
		return(-1);
	}
# endif

# if	FD < 2
	if (!n) {
		if ((i = searchfunction(argv[FUNCNAME])) >= maxuserfunc) {
			builtinerror(argv, argv[FUNCNAME], ER_NOENTRY);
			return(-1);
		}
		hitkey(2);
		printfunction(i, 1, Xstdout);
		VOID_C fputnl(Xstdout);
		return(0);
	}
# endif	/* FD < 2 */

	cp = line = catvar(&(argv[FUNCNAME + 1]), ' ');
	for (n = 0; n < MAXFUNCLINES && *cp; n++) {
		if (!(tmp = strtkchr(cp, ';', 0))) break;
		*(tmp++) = '\0';
		list[n] = Xstrdup(cp);
		cp = skipspace(tmp);
	}
	if (!(tmp = strtkchr(cp, '}', 0)) || *(tmp + 1)) {
		builtinerror(argv, line, ER_SYNTAXERR);
		for (j = 0; j < n; j++) Xfree(list[j]);
		Xfree(line);
		return(-1);
	}

	if (tmp > cp) {
		*tmp = '\0';
		list[n++] = Xstrdup(cp);
	}
	else if (!n) {
		Xfree(line);
		if (deletefunction(argv[FUNCNAME]) < 0) {
			builtinerror(argv, argv[FUNCNAME], ER_NOENTRY);
			return(-1);
		}

		return(0);
	}
	Xfree(line);

	var = (char **)Xmalloc((n + 1) * sizeof(*var));
	for (i = 0; i < n; i++) var[i] = list[i];
	var[i] = NULL;
	if (addfunction(Xstrdup(argv[FUNCNAME]), var) < 0) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		return(-1);
	}

	return(0);
}

static int NEAR exportenv(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp;
	int i;

	if (argc <= 1) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	i = argc - 1;
	if ((cp = getenvval(&i, &(argv[1]))) == vnullstr) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	if (i + 1 < argc) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	setenv2(argv[1], cp, 1);
	Xfree(cp);
	adjustpath();

	return(0);
}

static int NEAR dochdir(argc, argv)
int argc;
char *CONST argv[];
{
	if (argc != 2) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	if (chdir3(argv[1], 0) < 0) {
		builtinerror(argv, argv[1], -1);
		return(-1);
	}

	return(0);
}

static int NEAR loadsource(argc, argv)
int argc;
char *CONST argv[];
{
	int n;

	if (argc != 2) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	ttyiomode(1);
	n = loadruncom(argv[1], 1);
	stdiomode();
	if (n < 0) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}

	return(0);
}
#endif	/* !DEP_ORIGSHELL */

int checkbuiltin(comname)
CONST char *comname;
{
	int i;

	for (i = 0; i < BUILTINSIZ; i++)
		if (!strcommcmp(comname, builtinlist[i].ident)) return(i);

	return(-1);
}

int checkinternal(comname)
CONST char *comname;
{
	int i;

	for (i = 0; i < FUNCLISTSIZ; i++)
		if (!strcommcmp(comname, funclist[i].ident)) return(i);

	return(-1);
}

int execbuiltin(n, argc, argv)
int n, argc;
char *CONST argv[];
{
	return(((*builtinlist[n].func)(argc, argv) < 0)
		? RET_NOTICE : RET_SUCCESS);
}

int execinternal(n, argc, argv)
int n, argc;
char *CONST argv[];
{
	if (dumbterm > 1) {
		builtinerror(argv, NULL, ER_NOTDUMBTERM);
		return(-1);
	}

	if (argc > 2 || !filelist || maxfile <= 0) {
		VOID_C Xfprintf(Xstderr, "%s: %K\n", argv[0], ILFNC_K);
		return(RET_NOTICE);
	}
#ifdef	DEP_PTY
	if (parentfd >= 0) {
		sendparent(TE_INTERNAL, n, argv[1]);
		return(RET_SUCCESS);
	}
#endif
	ttyiomode(0);
	n = dointernal(n, argv[1], ICM_CMDLINE, NULL);
	locate(0, n_line - 1);
	stdiomode();
	if (n == FNC_FAIL) return(RET_FAIL);

	return(RET_SUCCESS);
}

#ifndef	DEP_ORIGSHELL
int execpseudoshell(command, flags)
CONST char *command;
int flags;
{
	char *cp, **argv;
	int i, n, argc, wastty;

	n = -1;
	command = skipspace(command);
	internal_status = FNC_FAIL;
	if (!(argc = getargs(command, &argv))) {
		freevar(argv);
		return(-1);
	}

# ifdef	DEP_FILECONV
	if (!(flags & F_NOKANJICONV)) printf_defkanji++;
# endif

	if ((i = checkbuiltin(argv[0])) >= 0) {
		if (!(flags & F_NOCOMLINE)) {
			locate(0, n_line - 1);
			tflush();
		}
		hitkey((flags & F_NOCOMLINE) ? -1 : 1);
		if ((wastty = isttyiomode)) stdiomode();
		n = execbuiltin(i, argc, argv);
		if (wastty) ttyiomode(wastty - 1);
		if (n == RET_SUCCESS) hitkey(3);
		else if (!(flags & F_NOCOMLINE)) {
			hideclock = 1;
			warning(0, ILFNC_K);
		}
	}
	else if (!(flags & F_IGNORELIST)
	&& (i = checkinternal(argv[0])) >= 0) {
		if (!(flags & F_NOCOMLINE)) {
			locate(0, n_line - 1);
			tflush();
		}
		if ((wastty = isttyiomode)) stdiomode();
		n = execinternal(i, argc, argv);
		if (wastty) ttyiomode(wastty - 1);
		if (n == RET_SUCCESS) /*EMPTY*/;
		else if (!(flags & F_NOCOMLINE)) {
			hideclock = 1;
			warning(0, ILFNC_K);
		}
	}
# if	FD >= 2
	else if ((i = checkuserfunc(argc, argv)) != -1) {
		if (!(flags & F_NOCOMLINE)) {
			locate(0, n_line - 1);
			tflush();
		}
		if ((wastty = isttyiomode)) stdiomode();
		n = setuserfunc(i, argv);
		n = (n < 0) ? RET_NOTICE : RET_SUCCESS;
		if (wastty) ttyiomode(wastty - 1);
		if (n == RET_SUCCESS);
		else if (!(flags & F_NOCOMLINE)) {
			hideclock = 1;
			warning(0, ILFNC_K);
		}
	}
# endif
	else if (isidentchar(*command)) {
		i = argc;
		if ((cp = getenvval(&i, argv)) != vnullstr && i == argc) {
			if (setenv2(argv[0], cp, 0) < 0) error(argv[0]);
			Xfree(cp);
			n = RET_SUCCESS;
		}
	}

# ifdef	DEP_FILECONV
	if (!(flags & F_NOKANJICONV)) printf_defkanji--;
# endif
	freevar(argv);

	return(n);
}
#endif	/* !DEP_ORIGSHELL */

#ifndef	_NOCOMPLETE
int completebuiltin(com, len, argc, argvp)
CONST char *com;
int len, argc;
char ***argvp;
{
	int i;

	for (i = 0; i < BUILTINSIZ; i++) {
		if (strncommcmp(com, builtinlist[i].ident, len)) continue;
		argc = addcompletion(builtinlist[i].ident, NULL, argc, argvp);
	}

	return(argc);
}

int completeinternal(com, len, argc, argvp)
CONST char *com;
int len, argc;
char ***argvp;
{
	int i;

	for (i = 0; i < FUNCLISTSIZ; i++) {
		if (strncommcmp(com, funclist[i].ident, len)) continue;
		argc = addcompletion(funclist[i].ident, NULL, argc, argvp);
	}

	return(argc);
}
#endif	/* !_NOCOMPLETE */

#ifdef	DEBUG
VOID freedefine(VOID_A)
{
# ifndef	DEP_ORIGSHELL
	int i, j;
# endif

	freestrarray(macrolist, maxmacro);
	freestrarray(helpindex, MAXHELPINDEX);
# ifdef	DEP_DYNAMICLIST
	Xfree(macrolist);
	Xfree(helpindex);
	Xfree(bindlist);
# endif
# ifndef	_NOARCHIVE
	freelaunchlist(launchlist, maxlaunch);
	freearchlist(archivelist, maxarchive);
#  ifdef	DEP_DYNAMICLIST
	Xfree(launchlist);
	Xfree(archivelist);
#  endif
# endif	/* !_NOARCHIVE */
# ifdef	DEP_DOSEMU
	freedosdrive(fdtype, maxfdtype);
#  ifdef	DEP_DYNAMICLIST
	Xfree(fdtype);
#  endif
# endif	/* DEP_DOSEMU */
# ifndef	DEP_ORIGSHELL
	for (i = 0; i < maxalias; i++) {
		Xfree(aliaslist[i].alias);
		Xfree(aliaslist[i].comm);
	}
	for (i = 0; i < maxuserfunc; i++) {
		Xfree(userfunclist[i].func);
		for (j = 0; userfunclist[i].comm[j]; j++)
			Xfree(userfunclist[i].comm[j]);
		Xfree(userfunclist[i].comm);
	}
# endif	/* !DEP_ORIGSHELL */
}
#endif	/* DEBUG */
