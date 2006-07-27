/*
 *	builtin.c
 *
 *	builtin commands
 */

#include "fd.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#ifdef	_NOORIGSHELL
#define	RET_SUCCESS	0
#define	RET_NOTICE	255
#else
#include "system.h"
#endif

#ifndef	_NOPTY
#include "termemu.h"
#endif

#define	MD5_BUFSIZ	(128 / 32)
#define	MD5_BLOCKS	16

#ifndef	_NOARCHIVE
extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
# ifndef	_NOBROWSE
extern char **browsevar;
# endif
#endif
extern char *macrolist[];
extern int maxmacro;
extern bindtable bindlist[];
extern strtable keyidentlist[];
extern functable funclist[];
extern char **history[];
extern short histsize[];
extern short histno[];
extern char *helpindex[];
#ifndef	_NOCUSTOMIZE
extern char **orighelpindex;
#endif
extern int fd_restricted;
#if	FD >= 2
extern char *progpath;
extern char *progname;
extern char *promptstr;
extern int lcmdline;
#endif
extern int internal_status;
#ifdef	_NOORIGSHELL
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
#ifdef	_USEUNICODE
extern int unicodebuffer;
#endif
#ifndef	_NOPTY
extern int parentfd;
#endif

static VOID NEAR builtinerror __P_((char *[], char *, int));
#ifdef	_NOORIGSHELL
static VOID NEAR hitkey __P_((int));
#else
#define	hitkey(n)
#endif
static VOID NEAR fputsmeta __P_((char *, FILE *));
#if	!defined (_NOARCHIVE) && !defined(_NOBROWSE)
static char **NEAR file2argv __P_((FILE *, char *, int));
#endif
#ifndef	_NOARCHIVE
# if	FD >= 2
static int NEAR getlaunchopt __P_((int, char *[], char *, launchtable *));
# endif
static int NEAR setlaunch __P_((int, char *[]));
static int NEAR setarch __P_((int, char *[]));
static int NEAR printlaunch __P_((int, char *[]));
static int NEAR printarch __P_((int, char *[]));
# ifndef	_NOBROWSE
static char **NEAR readargv __P_((char **, char **));
static int NEAR custbrowse __P_((int, char *[]));
# endif
#endif
static int NEAR getmacro __P_((char *));
static int NEAR setkeybind __P_((int, char *[]));
static int NEAR printbind __P_((int, char *[]));
#ifdef	_USEDOSEMU
static int NEAR _setdrive __P_((int, char *[], int));
static int NEAR setdrive __P_((int, char *[]));
static int NEAR unsetdrive __P_((int, char *[]));
static int NEAR printdrive __P_((int, char *[]));
#endif
#ifndef	_NOKEYMAP
static int NEAR setkeymap __P_((int, char *[]));
static int NEAR keytest __P_((int, char *[]));
#endif
static int NEAR printhist __P_((int, char *[]));
#if	FD >= 2
# ifndef	NOPOSIXUTIL
static int NEAR fixcommand __P_((int, char *[]));
# endif
static VOID NEAR voidmd5 __P_((u_long, u_long, u_long, u_long));
static VOID NEAR calcmd5 __P_((u_long [MD5_BUFSIZ], u_long [MD5_BLOCKS]));
static int NEAR printmd5 __P_((char *, FILE *));
static int NEAR md5sum __P_((int, char *[]));
static int NEAR evalmacro __P_((int, char *[]));
# ifndef	_NOKANJICONV
static int NEAR kconv __P_((int, char *[]));
# endif
static int NEAR getinputstr __P_((int, char *[]));
static int NEAR getyesno __P_((int, char *[]));
#endif	/* FD >= 2 */
#if	!MSDOS && (FD >= 2)
static int NEAR savetty __P_((int, char *[]));
#endif
#ifdef	_NOORIGSHELL
static int NEAR printenv __P_((int, char *[]));
static int NEAR setalias __P_((int, char *[]));
static int NEAR unalias __P_((int, char *[]));
static int NEAR checkuserfunc __P_((int, char *[]));
static int NEAR setuserfunc __P_((int, char *[]));
static int NEAR exportenv __P_((int, char *[]));
static int NEAR dochdir __P_((int, char *[]));
static int NEAR loadsource __P_((int, char *[]));
#endif	/* _NOORIGSHELL */

#if	FD >= 2
#define	BINDCOMMENT	':'
#define	DRIVESEP	' '
#define	ALIASSEP	'='
#define	FUNCNAME	0
#else
#define	BINDCOMMENT	';'
#define	DRIVESEP	','
#define	ALIASSEP	'\t'
#define	FUNCNAME	1
#endif

static CONST char *builtinerrstr[] = {
	NULL,
#define	ER_FEWMANYARG	1
	"Too few or many arguments",
#define	ER_OUTOFLIMIT	2
	"Out of limits",
#define	ER_NOENTRY	3
	"No such entry",
#define	ER_SYNTAXERR	4
	"Syntax error",
#define	ER_EXIST	5
	"Entry already exists",
#define	ER_INVALDEV	6
	"Invalid device",
#define	ER_EVENTNOFOUND	7
	"Event not found",
#define	ER_UNKNOWNOPT	8
	"Unknown option",
#define	ER_NOARGSPEC	9
	"No argument is specified",
#define	ER_NOTINSHELL	10
	"Cannot execute in shell mode",
#define	ER_NOTINRUNCOM	11
	"Cannot execute in startup file",
#define	ER_NOTRECURSE	12
	"Cannot execute recursively",
#define	ER_NOTDUMBTERM	13
	"Cannot execute on dumb term",
};
#define	BUILTINERRSIZ	arraysize(builtinerrstr)

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
#ifdef	_USEDOSEMU
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
# ifndef	_NOKANJICONV
	{kconv,		BL_KCONV},
# endif
	{getinputstr,	BL_READLINE},
	{getyesno,	BL_YESNO},
#endif	/* FD >= 2 */
#if	!MSDOS && (FD >= 2)
	{savetty,	BL_SAVETTY},
#endif
#ifdef	_NOORIGSHELL
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
#endif	/* _NOORIGSHELL */
};
#define	BUILTINSIZ	arraysize(builtinlist)


static VOID NEAR builtinerror(argv, s, n)
char *argv[], *s;
int n;
{
	int duperrno;

	duperrno = errno;
	if (n >= BUILTINERRSIZ || (n < 0 && !errno)) return;
	if (argv && argv[0]) fprintf2(stderr, "%k: ", argv[0]);
	if (s) fprintf2(stderr, "%k: ", s);
	fprintf2(stderr, "%s.",
		(n >= 0 && builtinerrstr[n])
			? builtinerrstr[n] : strerror2(duperrno));
	fputnl(stderr);
}

#ifdef	_NOORIGSHELL
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
		fflush(stdout);
		ttyiomode(1);
		win_x = 0;
		win_y = n_line - 1;
		hideclock = 1;
		warning(0, HITKY_K);
		stdiomode();
	}
}
#endif	/* _NOORIGSHELL */

static VOID NEAR fputsmeta(arg, fp)
char *arg;
FILE *fp;
{
	char *cp;

	if (!*arg) fputs("\"\"", fp);
	else {
		cp = killmeta(arg);
		kanjifputs(cp, fp);
		free(cp);
	}
}

#if	!defined (_NOARCHIVE) && !defined(_NOBROWSE)
static char **NEAR file2argv(fp, s, whole)
FILE *fp;
char *s;
int whole;
{
	char *cp, *line, **argv;
	ALLOC_T size;
	int i, j, pc, argc, escape, quote, pqoute, quoted;

	argc = 1;
	argv = (char **)malloc2(2 * sizeof(char *));
	argv[0] = strdup2(s);
	argv[1] = NULL;
	j = escape = 0;
	quote = pqoute = quoted = '\0';
	cp = c_realloc(NULL, 0, &size);
	while ((line = fgets2(fp, 0))) {
		if (!escape && !quote && *line == '#') {
			free(line);
			continue;
		}
		escape = 0;
		for (i = 0; line[i]; i++) {
			cp = c_realloc(cp, j + 2, &size);
			pc = parsechar(&(line[i]), -1,
				'\0', EA_EOLMETA, &quote, &pqoute);
			if (pc == PC_CLQUOTE) quoted = line[i];
			else if (pc == PC_WORD) {
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
			else if (!strchr(IFS_SET, line[i])) cp[j++] = line[i];
			else if (j || quoted) {
				quoted = cp[j] = '\0';
				argv = (char **)realloc2(argv,
					(argc + 2) * sizeof(char *));
				argv[argc++] = strdup2(cp);
				argv[argc] = NULL;
				j = 0;
			}
		}

		if (escape);
		else if (quote) cp[j++] = '\n';
		else if (j || quoted) {
			quoted = cp[j] = '\0';
			argv = (char **)realloc2(argv,
				(argc + 2) * sizeof(char *));
			argv[argc++] = strdup2(cp);
			argv[argc] = NULL;
			j = 0;
		}
		free(line);

		if (!whole && !escape && !quote) break;
	}

	if (j) {
		cp[j] = '\0';
		argv = (char **)realloc2(argv, (argc + 2) * sizeof(char *));
		argv[argc++] = strdup2(cp);
		argv[argc] = NULL;
	}
	free(cp);

	return(argv);
}
#endif	/* !_NOARCHIVE && !_NOBROWSE */

#ifndef	_NOARCHIVE
# if	FD >= 2
static int NEAR getlaunchopt(n, argv, opts, lp)
int n;
char *argv[], *opts;
launchtable *lp;
{
	char *cp;
	int i, c;

	if (argv[n][0] != '-') return(0);
	c = argv[n][1];
	if (!c || c == '-') return(++n);
	else if (!strchr2(opts, c)) {
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
		lp -> format = (char **)realloc2(lp -> format,
			(i + 2) * sizeof(char *));
		lp -> format[i++] = strdup2(cp);
		lp -> format[i] = NULL;
	}
	else if (c == 'i') {
		i = countvar(lp -> lignore);
		lp -> lignore = (char **)realloc2(lp -> lignore,
			(i + 2) * sizeof(char *));
		lp -> lignore[i++] = strdup2(cp);
		lp -> lignore[i] = NULL;
	}
	else if (c == 'e') {
		i = countvar(lp -> lerror);
		lp -> lerror = (char **)realloc2(lp -> lerror,
			(i + 2) * sizeof(char *));
		lp -> lerror[i++] = strdup2(cp);
		lp -> lerror[i] = NULL;
	}
	else if (c == 't' || c == 'b') {
		if ((i = atoi2(cp)) < 0) {
			lp -> topskip = ER_SYNTAXERR;
			lp -> bottomskip = n;
			return(-1);
		}
		if (c == 't') lp -> topskip = i;
		else lp -> bottomskip = i;
	}
	else if (c == 'p') {
		if (lp -> ext) free (lp -> ext);
		lp -> ext = strdup2(cp);
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
launchtable *lp;
{
	if (lp -> ext) free(lp -> ext);
	if (lp -> comm) free(lp -> comm);
# if	FD >= 2
	freevar(lp -> format);
	freevar(lp -> lignore);
	freevar(lp -> lerror);
# endif
}

int searchlaunch(list, max, lp)
launchtable *list;
int max;
launchtable *lp;
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
char *argv[];
launchtable *lp;
{
# if	FD >= 2
	int j, opt, err;
# else
	char *cp, *tmp;
	u_char c;
	int ch;
# endif
	int i, n;

	lp -> comm = NULL;
# if	FD >= 2
	lp -> format = lp -> lignore = lp -> lerror = NULL;
# endif

	if (argc <= 1) {
		lp -> topskip = ER_FEWMANYARG;
		lp -> bottomskip = 0;
		return(-1);
	}

	lp -> ext = getext(argv[1], &(lp -> flags));

	n = searchlaunch(launchlist, maxlaunch, lp);
	if (n >= MAXLAUNCHTABLE) {
		free(lp -> ext);
		lp -> topskip = ER_OUTOFLIMIT;
		lp -> bottomskip = 0;
		return(-1);
	}

	if (argc < 3) return(n);

# if	FD >= 2
	lp -> topskip = lp -> bottomskip = 0;
	opt = err = 0;
	for (i = 2; i < argc; i++) {
		j = getlaunchopt(i, argv, "fietb", lp);
		if (!j) {
			if (lp -> comm) break;
			lp -> comm = strdup2(argv[i]);
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
		lp -> format = (char **)malloc2(2 * sizeof(char *));
		lp -> format[0] = strdup2(argv[i]);
		lp -> format[1] = NULL;
	}

	if (i + 1 >= argc) /*EMPTY*/;
	else if ((j = atoi2(argv[i + 1])) >= 0) lp -> topskip = j;
	else {
		lp -> topskip = ER_SYNTAXERR;
		lp -> bottomskip = i + 1;
		freelaunch(lp);
		return(-1);
	}

	if (i + 2 >= argc) /*EMPTY*/;
	else if ((j = atoi2(argv[i + 2])) >= 0) lp -> bottomskip = j;
	else {
		lp -> topskip = ER_SYNTAXERR;
		lp -> bottomskip = i + 2;
		freelaunch(lp);
		return(-1);
	}

# else	/* FD < 2 */
	lp -> comm = strdup2(argv[2]);
	if (argc <= 3) lp -> topskip = lp -> bottomskip = 255;
	else {
		cp = tmp = catvar(&(argv[3]), '\0');
		if (!(cp = sscanf2(cp, "%Cu,", &c))) {
			lp -> topskip = ER_SYNTAXERR;
			lp -> bottomskip = (u_char)-1;
			free(lp -> ext);
			free(lp -> comm);
			lp -> ext = tmp;
			return(-1);
		}
		lp -> topskip = c;
		if (!(cp = sscanf2(cp, "%Cu", &c))) {
			lp -> topskip = ER_SYNTAXERR;
			lp -> bottomskip = (u_char)-1;
			free(lp -> ext);
			free(lp -> comm);
			lp -> ext = tmp;
			return(-1);
		}
		lp -> bottomskip = c;

		ch = ':';
		for (i = 0; i < MAXLAUNCHFIELD; i++) {
			cp = getrange(cp, ch, &(lp -> field[i]),
				&(lp -> delim[i]), &(lp -> width[i]));
			if (!cp) {
				lp -> topskip = ER_SYNTAXERR;
				lp -> bottomskip = (u_char)-1;
				free(lp -> ext);
				free(lp -> comm);
				lp -> ext = tmp;
				return(-1);
			}
			ch = ',';
		}

		ch = ':';
		for (i = 0; i < MAXLAUNCHSEP; i++) {
			if (*cp != ch) break;
			if (!(cp = sscanf2(++cp, "%Cu", &c))) {
				lp -> topskip = ER_SYNTAXERR;
				lp -> bottomskip = (u_char)-1;
				free(lp -> ext);
				free(lp -> comm);
				lp -> ext = tmp;
				return(-1);
			}
			lp -> sep[i] = (c) ? c - 1 : 255;
			ch = ',';
		}
		for (; i < MAXLAUNCHSEP; i++) lp -> sep[i] = 255;

		if (!*cp) lp -> lines = 1;
		else {
			if (!sscanf2(cp, ":%Cu%$", &c)) {
				lp -> topskip = ER_SYNTAXERR;
				lp -> bottomskip = (u_char)-1;
				free(lp -> ext);
				free(lp -> comm);
				lp -> ext = tmp;
				return(-1);
			}
			lp -> lines = (c) ? c : 1;
		}

		free(tmp);
	}
# endif	/* FD < 2 */

	return(n);
}

VOID addlaunch(n, lp)
int n;
launchtable *lp;
{
	if (n >= maxlaunch) maxlaunch++;
	else freelaunch(&(launchlist[n]));
	memcpy((char *)&(launchlist[n]), (char *)lp, sizeof(*lp));
}

VOID deletelaunch(n)
int n;
{
	freelaunch(&(launchlist[n]));
	memmove((char *)&(launchlist[n]), (char *)&(launchlist[n + 1]),
		(--maxlaunch - n) * sizeof(launchtable));
}

static int NEAR setlaunch(argc, argv)
int argc;
char *argv[];
{
	launchtable launch;
	int n;

	if ((n = parselaunch(argc, argv, &launch)) < 0) {
# if	FD < 2
		if (launch.bottomskip == (u_char)-1) {
			builtinerror(argv, launch.ext, launch.topskip);
			free(launch.ext);
		}
		else
# endif
		builtinerror(argv,
			(launch.bottomskip) ? argv[launch.bottomskip] : NULL,
			launch.topskip);
		return(-1);
	}

	if (!(launch.comm)) {
		free(launch.ext);
		if (n >= maxlaunch) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		deletelaunch(n);
# ifndef	_NOPTY
		sendparent(TE_DELETELAUNCH, n);
# endif

		return(0);
	}

	addlaunch(n, &launch);
# ifndef	_NOPTY
	sendparent(TE_ADDLAUNCH, n, &launch);
# endif

	return(0);
}

VOID freearch(ap)
archivetable *ap;
{
	if (ap -> ext) free(ap -> ext);
	if (ap -> p_comm) free(ap -> p_comm);
	if (ap -> u_comm) free(ap -> u_comm);
}

int searcharch(list, max, ap)
archivetable *list;
int max;
archivetable *ap;
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
char *argv[];
archivetable *ap;
{
	int n;

	ap -> p_comm = ap -> u_comm = NULL;

	if (argc <= 1 || argc >= 5) {
		ap -> flags = ER_FEWMANYARG;
		ap -> ext = NULL;
		return(-1);
	}

	ap -> ext = getext(argv[1], &(ap -> flags));

	n = searcharch(archivelist, maxarchive, ap);
	if (n >= MAXARCHIVETABLE) {
		free(ap -> ext);
		ap -> flags = ER_OUTOFLIMIT;
		ap -> ext = NULL;
		return(-1);
	}

	if (argc < 3) return(n);

	if (argv[2][0]) ap -> p_comm = strdup2(argv[2]);
	if (argc > 3 && argv[3][0]) ap -> u_comm = strdup2(argv[3]);
	if (!(ap -> p_comm) && !(ap -> u_comm)) {
		free(ap -> ext);
		ap -> flags = ER_FEWMANYARG;
		ap -> ext = NULL;
		return(-1);
	}

	return(n);
}

VOID addarch(n, ap)
int n;
archivetable *ap;
{
	if (n >= maxarchive) maxarchive++;
	else freearch(&(archivelist[n]));
	memcpy((char *)&(archivelist[n]), (char *)ap, sizeof(*ap));
}

VOID deletearch(n)
int n;
{
	freearch(&(archivelist[n]));
	memmove((char *)&(archivelist[n]), (char *)&(archivelist[n + 1]),
		(--maxarchive - n) * sizeof(archivetable));
}

static int NEAR setarch(argc, argv)
int argc;
char *argv[];
{
	archivetable arch;
	int n;

	if ((n = parsearch(argc, argv, &arch)) < 0) {
		builtinerror(argv, arch.ext, arch.flags);
		return(-1);
	}

	if (!(arch.p_comm) && !(arch.u_comm)) {
		free(arch.ext);
		if (n >= maxarchive) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		deletearch(n);
# ifndef	_NOPTY
		sendparent(TE_DELETEARCH, n);
# endif

		return(0);
	}

	addarch(n, &arch);
# ifndef	_NOPTY
	sendparent(TE_ADDARCH, n, &arch);
# endif

	return(0);
}

/*ARGSUSED*/
VOID printlaunchcomm(list, n, isset, omit, fp)
launchtable *list;
int n, isset, omit;
FILE *fp;
{
	int i, ch;

	fprintf2(fp, "%s ", BL_LAUNCH);
# if	FD >= 2
	if (list[n].flags & LF_IGNORECASE) fputc('/', fp);
# endif
	fputsmeta(&(list[n].ext[1]), fp);
	if (isset) for (;;) {
		fputc('\t', fp);

		fputsmeta(list[n].comm, fp);
# if	FD >= 2
		if (!list[n].format) break;

		ch = 0;
		for (i = 0; list[n].format[i]; i++) {
			if (i) {
				fputs(" \\\n", fp);
				ch++;
			}
			fputs("\t-f ", fp);
			fputsmeta(list[n].format[i], fp);
		}
		if (list[n].lignore) for (i = 0; list[n].lignore[i]; i++) {
			fputs(" \\\n\t-i ", fp);
			ch++;
			fputsmeta(list[n].lignore[i], fp);
		}
		if (list[n].lerror) for (i = 0; list[n].lerror[i]; i++) {
			fputs(" \\\n\t-e ", fp);
			ch++;
			fputsmeta(list[n].lerror[i], fp);
		}
		if (!list[n].topskip && !list[n].bottomskip) break;

		if (list[n].topskip) {
			if (ch) fputs(" \\\n", fp);
			fprintf2(fp, "\t-t %d", (int)(list[n].topskip));
		}
		if (list[n].bottomskip) {
			if (list[n].topskip) fputc(' ', fp);
			else if (ch) fputs(" \\\n\t", fp);
			else fputc('\t', fp);
			fprintf2(fp, "-b %d", (int)(list[n].bottomskip));
		}
# else	/* FD < 2 */
		if (list[n].topskip >= 255) break;
		if (omit) {
			fputs("\t(Arch)", fp);
			break;
		}
		fprintf2(fp, "\t%d,%d",
			(int)(list[n].topskip), (int)(list[n].bottomskip));
		ch = ':';
		for (i = 0; i < MAXLAUNCHFIELD; i++) {
			fputc(ch, fp);
			if (list[n].field[i] >= 255) fputc('0', fp);
			else fprintf2(fp, "%d", (int)(list[n].field[i]) + 1);
			if (list[n].delim[i] >= 128)
				fprintf2(fp, "[%d]",
					(int)(list[n].delim[i]) - 128 + 1);
			else if (list[n].delim[i])
				fprintf2(fp, "'%c'", (int)(list[n].delim[i]));
			if (!(list[n].width[i])) /*EMPTY*/;
			else if (list[n].width[i] >= 128)
				fprintf2(fp, "-%d",
					(int)(list[n].width[i]) - 128);
			else fprintf2(fp, "-'%c'", (int)(list[n].width[i]));
			ch = ',';
		}
		ch = ':';
		for (i = 0; i < MAXLAUNCHSEP; i++) {
			if (list[n].sep[i] >= 255) break;
			fprintf2(fp, "%c%d", ch, (int)(list[n].sep[i]) + 1);
			ch = ',';
		}
		if (list[n].lines > 1) {
			if (!i) fputc(':', fp);
			fprintf2(fp, "%d", (int)(list[n].lines));
		}
# endif	/* FD < 2 */
		break;
/*NOTREACHED*/
	}

	fputnl(fp);
}

static int NEAR printlaunch(argc, argv)
int argc;
char *argv[];
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
		printlaunchcomm(launchlist, i, 1, 1, stdout);
		hitkey(0);
	}
	else {
		ext = getext(argv[1], &flags);
		for (i = 0; i < maxlaunch; i++) {
			n = extcmp(ext, flags,
				launchlist[i].ext, launchlist[i].flags, 0);
			if (!n) break;
		}
		free(ext);
		if (i >= maxlaunch) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		printlaunchcomm(launchlist, i, 1, 0, stdout);
	}

	return(0);
}

VOID printarchcomm(list, n, isset, fp)
archivetable *list;
int n, isset;
FILE *fp;
{
	fprintf2(fp, "%s ", BL_ARCH);
# if	FD >= 2
	if (list[n].flags & LF_IGNORECASE) fputc('/', fp);
# endif
	fputsmeta(&(list[n].ext[1]), fp);
	if (isset) {
		fputc('\t', fp);

		if (list[n].p_comm) fputsmeta(list[n].p_comm, fp);
		if (!list[n].u_comm) return;
		fputc('\t', fp);
		fputsmeta(list[n].u_comm, fp);
	}
	fputnl(fp);
}

static int NEAR printarch(argc, argv)
int argc;
char *argv[];
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
		printarchcomm(archivelist, i, 1, stdout);
		hitkey(0);
	}
	else {
		ext = getext(argv[1], &flags);
		for (i = 0; i < maxarchive; i++) {
			n = extcmp(ext, flags,
				archivelist[i].ext, archivelist[i].flags, 0);
			if (!n) break;
		}
		free(ext);
		if (i >= maxarchive) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		printarchcomm(archivelist, i, 1, stdout);
	}

	return(0);
}

# ifndef	_NOBROWSE
static char **NEAR readargv(sargv, dargv)
char **sargv, **dargv;
{
	FILE *fp;
	char *cp, **argv;
	int n, dargc;

	dargc = countvar(dargv);
	for (n = 1; sargv[n]; n++) {
		if (sargv[n][0] != '-' || sargv[n][1] != '@') {
			dargv = (char **)realloc2(dargv,
				(dargc + 2) * sizeof(char *));
			dargv[dargc++] = strdup2(sargv[n]);
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
		if (cp[0] == '-' && !cp[1]) fp = stdin;
		else if (!(fp = Xfopen(cp, "r"))) {
			builtinerror(sargv, cp, -1);
			freevar(dargv);
			return(NULL);
		}

		argv = file2argv(fp, sargv[0], 1);
		if (fp != stdin) Xfclose(fp);
		else clearerr(fp);

		dargv = readargv(argv, dargv);
		freevar(argv);
		if (!dargv) return(NULL);
		dargc = countvar(dargv);
	}

	return(dargv);
}

VOID freebrowse(list)
launchtable *list;
{
	int i;

	browselevel = 0;
	freevar(browsevar);
	browsevar = NULL;
	if (argvar) {
		for (i = 1; argvar[i]; i++) {
			free(argvar[i]);
			argvar[i] = NULL;
		}
	}
	if (!list) return;
	for (i = 0; list[i].comm; i++) freelaunch(&(list[i]));
	free(list);
}

static int NEAR custbrowse(argc, argv)
int argc;
char *argv[];
{
	launchtable *list;
	char **argv2;
	int i, n, lvl;

#  ifndef	_NOORIGSHELL
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
	argv2 = (char **)malloc2(2 * sizeof(char *));
	argv2[0] = strdup2(argv[0]);
	argv2[1] = NULL;
	if (!(argv2 = readargv(argv, argv2))) return(-1);

	list = NULL;
	lvl = 0;
	n = 1;
	while (argv2[n]) {
		list = (launchtable *)realloc2(list,
			(lvl + 2) * sizeof(launchtable));
		list[lvl].topskip = list[lvl].bottomskip = 0;
		list[lvl].comm = strdup2(argv2[n++]);
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

static int NEAR getmacro(cp)
char *cp;
{
	if (maxmacro >= MAXMACROTABLE) return(0);
	else {
		macrolist[maxmacro] = cp;
		return(FUNCLISTSIZ + maxmacro++);
	}
}

int ismacro(n)
int n;
{
	if (n < FUNCLISTSIZ || n >= FUNCLISTSIZ + MAXMACROTABLE) return(0);

	return(1);
}

int freemacro(n)
int n;
{
	int i;

	if (!ismacro(n)) return(-1);

	free(macrolist[n - FUNCLISTSIZ]);
	memmove((char *)&(macrolist[n - FUNCLISTSIZ]),
		(char *)&(macrolist[n - FUNCLISTSIZ + 1]),
		(--maxmacro - (n - FUNCLISTSIZ)) * sizeof(char *));

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (ismacro(bindlist[i].f_func) && bindlist[i].f_func > n)
			bindlist[i].f_func--;
		if (ismacro(bindlist[i].d_func) && bindlist[i].d_func > n)
			bindlist[i].d_func--;
	}

	return(0);
}

int searchkeybind(list, bindp)
bindtable *list, *bindp;
{
	int i;

	for (i = 0; i < MAXBINDTABLE && list[i].key >= 0; i++)
		if (list[i].key == bindp -> key) break;

	return(i);
}

int parsekeybind(argc, argv, bindp)
int argc;
char *argv[];
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

	n = searchkeybind(bindlist, bindp);
	if (n >= MAXBINDTABLE - 1) {
		bindp -> key = ER_OUTOFLIMIT;
		bindp -> f_func = 0;
		return(-1);
	}

	if (argc == 2) {
		bindp -> f_func = bindp -> d_func = 255;
		return(n);
	}

	for (i = 0; i < FUNCLISTSIZ; i++)
		if (!strcommcmp(argv[2], funclist[i].ident)) break;
	bindp -> f_func = (i < FUNCLISTSIZ) ? i : 254;

	if (argc <= 3 || argv[3][0] == BINDCOMMENT) {
		bindp -> d_func = 255;
		i = 3;
	}
	else {
		for (i = 0; i < FUNCLISTSIZ; i++)
			if (!strcommcmp(argv[3], funclist[i].ident)) break;
		bindp -> d_func = (i < FUNCLISTSIZ) ? i : 254;
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
			bindp -> f_func = i;
			return(-1);
		}
	}

	return(n);
}

int addkeybind(n, bindp, func1, func2, cp)
int n;
bindtable *bindp;
char *func1, *func2, *cp;
{
	bindtable bind;
	int n1, n2;

	memcpy((char *)&bind, (char *)bindp, sizeof(bind));
	if (bind.f_func != 254) {
		if (func1) free(func1);
	}
	else if (!(bind.f_func = getmacro(func1))) {
		if (func1) free(func1);
		if (func2) free(func2);
		if (cp) free(cp);
		return(-1);
	}

	if (bind.d_func != 254) {
		if (func2) free(func2);
	}
	else if (!(bind.d_func = getmacro(func2))) {
		freemacro(bind.f_func);
		if (func2) free(func2);
		if (cp) free(cp);
		return(-2);
	}

	if (bind.key < K_F(1) || bind.key > K_F(MAXHELPINDEX)) {
		if (cp) free(cp);
	}
	else if (cp) {
		free(helpindex[bind.key - K_F(1)]);
		helpindex[bind.key - K_F(1)] = cp;
	}

	if (bindlist[n].key < 0) {
		n1 = n2 = -1;
		memcpy((char *)&(bindlist[n + 1]), (char *)&(bindlist[n]),
			sizeof(bindtable));
	}
	else {
		n1 = bindlist[n].f_func;
		n2 = bindlist[n].d_func;
	}

	memcpy((char *)&(bindlist[n]), (char *)&bind, sizeof(bind));
	freemacro(n1);
	freemacro(n2);

	return(0);
}

VOID deletekeybind(n)
int n;
{
	freemacro(bindlist[n].f_func);
	freemacro(bindlist[n].d_func);
	if (bindlist[n].key >= K_F(1)
	&& bindlist[n].key <= K_F(MAXHELPINDEX)) {
		free(helpindex[bindlist[n].key - K_F(1)]);
		helpindex[bindlist[n].key - K_F(1)] = strdup2(nullstr);
	}
	memmove((char *)&(bindlist[n]), (char *)&(bindlist[n + 1]),
		(MAXBINDTABLE - n) * sizeof(bindtable));
}

static int NEAR setkeybind(argc, argv)
int argc;
char *argv[];
{
	bindtable bind;
	char *cp, *func1, *func2;
	int n, no;

	if ((no = parsekeybind(argc, argv, &bind)) < 0) {
		builtinerror(argv,
			(bind.f_func) ? argv[bind.f_func] : NULL, bind.key);
		return(-1);
	}

	if (bind.f_func == 255) {
		if (bindlist[no].key < 0) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		deletekeybind(no);
#ifndef	_NOPTY
		sendparent(TE_DELETEKEYBIND, no);
#endif

		return(0);
	}

	func1 = (bind.f_func == 254) ? strdup2(argv[2]) : NULL;
	func2 = (bind.d_func == 254) ? strdup2(argv[3]) : NULL;
	n = (bind.d_func == 255) ? 3 : 4;
	cp = (argc > n) ? strdup2(&(argv[n][1])) : NULL;

	if ((n = addkeybind(no, &bind, func1, func2, cp)) < 0) {
		builtinerror(argv, argv[1 - n], ER_OUTOFLIMIT);
		return(-1);
	}
#ifndef	_NOPTY
	sendparent(TE_ADDKEYBIND, no, &bind, func1, func2, cp);
#endif

	return(0);
}

char *gethelp(bindp)
bindtable *bindp;
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
bindtable *list;
int n, isset;
FILE *fp;
{
	char *cp;

	fprintf2(fp, "%s ", BL_BIND);
	fputsmeta(getkeysym(list[n].key, 0), fp);
	if (isset) {
		fputc('\t', fp);

		if (list[n].f_func < FUNCLISTSIZ)
			fputs(funclist[list[n].f_func].ident, fp);
		else if (ismacro(list[n].f_func))
			fputsmeta(macrolist[list[n].f_func - FUNCLISTSIZ], fp);
		else fputs("\"\"", fp);
		if (list[n].d_func < FUNCLISTSIZ)
			fprintf2(fp, "\t%s", funclist[list[n].d_func].ident);
		else if (ismacro(list[n].d_func)) {
			fputc('\t', fp);
			fputsmeta(macrolist[list[n].d_func - FUNCLISTSIZ], fp);
		}
		if ((cp = gethelp(&(list[n]))))
			fprintf2(fp, "\t%c%k", BINDCOMMENT, cp);
	}
	fputnl(fp);
}

static int NEAR printbind(argc, argv)
int argc;
char *argv[];
{
	int i, c;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	hitkey(2);
	if (argc <= 1)
	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (!ismacro(bindlist[i].f_func)
		&& !ismacro(bindlist[i].d_func))
			continue;
		printmacro(bindlist, i, 1, stdout);
		hitkey(0);
	}
	else if ((c = getkeycode(argv[1], 0)) < 0) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	else {
		for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			if (c == bindlist[i].key) break;
		if (i >= MAXBINDTABLE || bindlist[i].key < 0) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		printmacro(bindlist, i, 1, stdout);
	}

	return(0);
}

#ifdef	_USEDOSEMU
/*ARGSUSED*/
int searchdrv(list, devp, isset)
devinfo *list, *devp;
int isset;
{
	int i;

	for (i = 0; list[i].name; i++) if (devp -> drive == list[i].drive) {
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
	int i, n;

	n = 1;
# ifdef	HDDMOUNT
	if (!fdtype[no].cyl) {
		char *dev;
		int s;

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
	for (i = 0; i < n; i++) free(fdtype[no + i].name);
	memmove((char *)&(fdtype[no]), (char *)&(fdtype[no + n]),
		(MAXDRIVEENTRY - no - n) * sizeof(devinfo));

	return(no);
}

int insertdrv(no, devp)
int no;
devinfo *devp;
{
# ifdef	HDDMOUNT
	l_off_t *sp;
	char *drvlist;
# endif
	int i, n, min, max;

	min = -1;
# ifdef	FAKEUNINIT
	max = -1;	/* fake for -Wuninitialized */
# endif
	for (i = 0; fdtype[i].name; i++) {
		if (strpathcmp(devp -> name, fdtype[i].name)) continue;
		if (min < 0) min = i;
		max = i;
	}
	if (min < 0) {
		if (no > 0 && no < i
		&& !strpathcmp(fdtype[no - 1].name, fdtype[no].name))
			no = i;
	}
	else if (no < min) no = min;
	else if (no > max + 1) no = max + 1;

	max = i;
	n = 1;
# ifdef	HDDMOUNT
	sp = NULL;
	if (!(devp -> cyl)) {
		int j, drive;

		if (!(sp = readpt(devp -> name, devp -> sect))) return(-1);
		for (n = 0; sp[n + 1]; n++);
		if (!n) {
			free(sp);
			return(-1);
		}
		else if (n >= MAXDRIVEENTRY - no) {
			free(sp);
			return(-2);
		}
		devp -> sect = sp[0];

		drvlist = malloc2(n);
		drvlist[0] = devp -> drive;
		for (i = 0; i < n - 1; i++) {
			for (drive = drvlist[i] + 1; drive <= 'Z'; drive++) {
				for (j = 0; fdtype[j].name; j++)
					if (drive == fdtype[j].drive) break;
				if (!fdtype[j].name) break;
			}
			if (drive > 'Z') break;
			drvlist[i + 1] = drive;
		}
		if (i < n - 1) {
			free(sp);
			free(drvlist);
			return(-2);
		}
	}
#  ifdef	FAKEUNINIT
	else drvlist = NULL;		/* fake for -Wuninitialized */
#  endif
# endif	/* HDDMOUNT */

	fdtype[max + n].name = NULL;
	memmove((char *)&(fdtype[max + n - 1]), (char *)&(fdtype[max - 1]),
		(max - no) * sizeof(devinfo));

# ifdef	HDDMOUNT
	if (!(devp -> cyl)) {
		for (i = 0; i < n; i++) {
			memcpy((char *)&(fdtype[no + i]), (char *)devp,
				sizeof(devinfo));
			fdtype[no + i].drive = drvlist[i];
			fdtype[no + i].name = strdup2(devp -> name);
			fdtype[no + i].offset = sp[i + 1];
		}
		free(sp);
		free(drvlist);
		free(devp -> name);
		return(no);
	}
	fdtype[no].offset = 0;
# endif	/* HDDMOUNT */
	memcpy((char *)&(fdtype[no]), (char *)devp, sizeof(devinfo));
	fdtype[no].name = devp -> name;

	return(no);
}

int parsesetdrv(argc, argv, devp)
int argc;
char *argv[];
devinfo *devp;
{
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

	devp -> drive = toupper2(argv[1][0]);
	devp -> name = argv[2];
	if (!isalpha2(devp -> drive)) {
		devp -> drive = ER_SYNTAXERR;
		devp -> head = 1;
		return(-1);
	}

# ifdef	HDDMOUNT
	if (!strncmp(argv[3], "HDD", strsize("HDD"))) {
		if (argc > 5) {
			devp -> drive = ER_FEWMANYARG;
			devp -> head = 0;
			return(-1);
		}
		cyl = 0;
		if (!argv[3][3]) sect = 0;
		else if (!strcmp(&(argv[3][3]), "98")) sect = 98;
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
		if (sect) head = toupper2(head);
	}
	else
# endif	/* HDDMOUNT */
# if	FD >= 2
	if (argc != 6) {
		devp -> drive = ER_FEWMANYARG;
		devp -> head = 0;
		return(-1);
	}
	else if ((head = atoi2(argv[3])) <= 0) {
		devp -> drive = ER_SYNTAXERR;
		devp -> head = 3;
		return(-1);
	}
	else if (head > MAXUTYPE(u_char)) {
		devp -> drive = ER_OUTOFLIMIT;
		devp -> head = 3;
		return(-1);
	}
	else if ((sect = atoi2(argv[4])) <= 0) {
		devp -> drive = ER_SYNTAXERR;
		devp -> head = 4;
		return(-1);
	}
	else if (sect > MAXUTYPE(u_short)) {
		devp -> drive = ER_OUTOFLIMIT;
		devp -> head = 4;
		return(-1);
	}
	else if ((cyl = atoi2(argv[5])) <= 0) {
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
		char *cp, *tmp;
		u_char c;

		cp = tmp = catvar(&(argv[3]), '\0');

		if (!(cp = sscanf2(cp, "%+Cu%c", &c, DRIVESEP))) {
			devp -> drive = ER_SYNTAXERR;
			devp -> head = (u_char)-1;
			devp -> name = tmp;
			return(-1);
		}
		head = c;
		if (!(cp = sscanf2(cp, "%+Cu%c", &c, DRIVESEP))) {
			devp -> drive = ER_SYNTAXERR;
			devp -> head = (u_char)-1;
			devp -> name = tmp;
			return(-1);
		}
		sect = c;
		if (!sscanf2(cp, "%+Cu%$", &c)) {
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
		free(tmp);
	}
# endif	/* FD < 2 */
	devp -> head = head;
	devp -> sect = sect;
	devp -> cyl = cyl;

	return(0);
}

static int NEAR _setdrive(argc, argv, isset)
int argc;
char *argv[];
int isset;
{
	devinfo dev;
	int i, n;

	if (parsesetdrv(argc, argv, &dev) < 0) {
# if	FD < 2
		if (dev.head == (u_char)-1) {
			builtinerror(argv, dev.name, dev.drive);
			free(dev.name);
		}
		else
# endif
		builtinerror(argv,
			(dev.head) ? argv[dev.head] : NULL, dev.drive);
		return(-1);
	}

# ifdef	HDDMOUNT
	if (!(dev.cyl)) for (i = 0; fdtype[i].name; i++) {
		if (dev.drive == fdtype[i].drive) break;
	}
	else
# endif
	i = searchdrv(fdtype, &dev, isset);

	if (!isset) {
		if (!fdtype[i].name) {
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
		deletedrv(i);
# ifndef	_NOPTY
		sendparent(TE_DELETEDRV, i);
# endif
	}
	else {
		if (fdtype[i].name) {
			builtinerror(argv, argv[1], ER_EXIST);
			return(-1);
		}
		if (i >= MAXDRIVEENTRY - 1) {
			builtinerror(argv, NULL, ER_OUTOFLIMIT);
			return(-1);
		}

		dev.name = strdup2(dev.name);
		n = insertdrv(i, &dev);
		if (n < -1) {
			builtinerror(argv, NULL, ER_OUTOFLIMIT);
			free(dev.name);
			return(-1);
		}
		if (!inruncom && n < 0) {
			builtinerror(argv, argv[2], ER_INVALDEV);
			free(dev.name);
			return(-1);
		}
# ifndef	_NOPTY
		sendparent(TE_INSERTDRV, i, &dev);
# endif
	}

	return(0);
}

static int NEAR setdrive(argc, argv)
int argc;
char *argv[];
{
	return(_setdrive(argc, argv, 1));
}

static int NEAR unsetdrive(argc, argv)
int argc;
char *argv[];
{
	return(_setdrive(argc, argv, 0));
}

/*ARGSUSED*/
VOID printsetdrv(fdlist, n, isset, verbose, fp)
devinfo fdlist[];
int n, isset, verbose;
FILE *fp;
{
	fprintf2(fp, "%s %c\t",
		(isset) ? BL_SDRIVE : BL_UDRIVE, fdlist[n].drive);

	fputsmeta(fdlist[n].name, fp);
	fputc('\t', fp);
# ifdef	HDDMOUNT
	if (!fdlist[n].cyl) {
		fputs("HDD", fp);
		if (isupper2(fdlist[n].head)) fputs("98", fp);
		if (verbose) fprintf2(fp, " #offset=%'Ld",
			fdlist[n].offset / fdlist[n].sect);
	}
	else
# endif	/* HDDMOUNT */
	fprintf2(fp, "%d%c%d%c%d", (int)(fdlist[n].head), DRIVESEP,
		(int)(fdlist[n].sect), DRIVESEP, (int)(fdlist[n].cyl));
	fputnl(fp);
}

static int NEAR printdrive(argc, argv)
int argc;
char *argv[];
{
	int i, n;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	hitkey(2);
	if (argc <= 1) for (i = 0; fdtype[i].name; i++) {
		printsetdrv(fdtype, i, 1, 1, stdout);
		hitkey(0);
	}
	else {
		for (i = n = 0; fdtype[i].name; i++)
		if (toupper2(argv[1][0]) == fdtype[i].drive) {
			n++;
			printsetdrv(fdtype, i, 1, 1, stdout);
			hitkey(0);
		}
		if (!n) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
	}

	return(0);
}
#endif	/* _USEDOSEMU */

#ifndef	_NOKEYMAP
VOID printkeymap(kp, isset, fp)
keyseq_t *kp;
int isset;
FILE *fp;
{
	char *cp;

	fprintf2(fp, "%s ", BL_KEYMAP);
	fputsmeta(getkeysym(kp -> code, 1), fp);
	if (isset) {
		fputc('\t', fp);

		cp = encodestr(kp -> str, kp -> len);
		fputsmeta(cp, fp);
		free(cp);
	}
	fputnl(fp);
}

int parsekeymap(argc, argv, kp)
int argc;
char *argv[];
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
		free(kp -> str);
		kp -> code = ER_SYNTAXERR;
		kp -> len = (u_char)2;
		return(-1);
	}

	return(0);
}

static int NEAR setkeymap(argc, argv)
int argc;
char *argv[];
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
			printkeymap(&key, 1, stdout);
			hitkey(0);
		}
		for (i = 0; (key.code = keyidentlist[i].no) > 0; i++) {
			if (getkeyseq(&key) < 0 || !(key.len)) continue;
			printkeymap(&key, 1, stdout);
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
		printkeymap(&key, 1, stdout);
		return(0);
	}
	if (!(key.str)) {
		setkeyseq(key.code, NULL, 0);
# ifndef	_NOPTY
		key.len = (u_char)0;
		sendparent(TE_SETKEYSEQ, &key);
# endif
		return(0);
	}

	setkeyseq(key.code, key.str, key.len);
# ifndef	_NOPTY
	sendparent(TE_SETKEYSEQ, &key);
# endif

	return(0);
}

static int NEAR keytest(argc, argv)
int argc;
char *argv[];
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

	n = (argc >= 2) ? atoi2(argv[1]) : 1;
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
		cputs2("\r\"");
		putterm(L_CLEAR);
		buf[1] = '\0';
		do {
			if ((ch = getch2()) == EOF) break;
			next = kbhit2(WAITKEYPAD * 1000L);
			buf[0] = ch;
			cp = encodestr(buf, 1);
			cputs2(cp);
			free(cp);
		} while (next);
		putch2('"');
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
char *argv[];
{
	int i, n, max, size;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	size = (int)histsize[0];
	if (argc < 2) n = size;
	else if ((n = atoi2(argv[1])) < 1) {
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
		fprintf2(stdout, "%5d  %k", n + 1, history[0][i]);
		fputnl(stdout);
		if (n++ >= (int)MAXHISTNO) n = 0;
		hitkey(0);
	}

	return(0);
}

#if	FD >= 2
# ifndef	NOPOSIXUTIL
static int NEAR fixcommand(argc, argv)
int argc;
char *argv[];
{
	FILE *fp;
	char *cp, *tmp, *editor, path[MAXPATHLEN];
	int i, n, f, l, skip, list, nonum, rev, exe, ret;

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
		fprintf2(stderr,
	"%k: usage: %k [-ls] [-nr] [-e editor] [old=new] [first] [last]",
			argv[0], argv[0]);
		fputnl(stderr);
		return(-1);
	}

	if (!history[0]) return(0);
	tmp = removehist(0);
	if (exe) {
		char *s, *r;
		int j, l1, l2, len, max;

		i = n;
		for (; n < argc; n++) if (!strchr(argv[n], '=')) break;
		f = parsehist((n < argc) ? argv[n] : "!", NULL, '\0');
		if (f < 0) {
			builtinerror(argv, argv[n], ER_EVENTNOFOUND);
			entryhist(0, tmp, 0);
			free(tmp);
			return(-1);
		}
		free(tmp);
		s = strdup2(history[0][f]);
		max = len = strlen(s);
		for (; i < n; i++) {
			if (!(r = strchr(argv[i], '='))) continue;
			l1 = r - argv[i];
			for (cp = s; (cp = strchr2(cp, argv[i][0])); cp++)
				if (!strncmp(cp, argv[i], l1)) break;
			if (!cp) continue;
			r++;
			l2 = strlen(r);
			len += l2 - l1;
			if (len > max) {
				max = len;
				j = cp - s;
				s = realloc2(s, max + 1);
				cp = s + j;
			}
			memmove(&(cp[l2]), &(cp[l1]),
				&(s[len]) - &(cp[l2]) + 1);
			strncpy(cp, r, l2);
		}
		kanjifputs(s, stdout);
		fputnl(stdout);
		entryhist(0, s, 0);
		ret = execmacro(s, NULL,
			F_NOCONFIRM | F_ARGSET | F_IGNORELIST);
		if (ret < 0) ret = 0;
		free(s);
		return(ret);
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
		entryhist(0, tmp, 0);
		free(tmp);
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
		fp = stdout;
		hitkey(2);
	}
	else {
		int fd;

		if ((fd = mktmpfile(path)) < 0) {
			builtinerror(argv, argv[0], -1);
			free(tmp);
			return(-1);
		}
		if (!(fp = Xfdopen(fd, "w"))) {
			builtinerror(argv, path, -1);
			VOID_C Xclose(fd);
			rmtmpfile(path);
			free(tmp);
			return(-1);
		}
		nonum = 1;
	}

	if (f >= l) for (i = f; i >= l; i--) {
		if (history[0][i]) {
			if (!nonum) {
				fprintf2(fp, "%5d  ", n + 1);
				if (n++ >= (int)MAXHISTNO) n = 0;
			}
			kanjifputs(history[0][i], fp);
			fputnl(fp);
#  ifdef	_NOORIGSHELL
			if (list) hitkey(0);
#  endif
		}
	}
	else for (i = f; i <= l; i++) {
		if (history[0][i]) {
			if (!nonum) {
				fprintf2(fp, "%5d  ", n + 1);
				if (--n < 0) n = (int)MAXHISTNO;
			}
			kanjifputs(history[0][i], fp);
			fputnl(fp);
#  ifdef	_NOORIGSHELL
			if (list) hitkey(0);
#  endif
		}
	}

	if (list) {
		fflush(stdout);
		entryhist(0, tmp, 0);
		free(tmp);
		return(0);
	}

	Xfclose(fp);
	ret = execmacro(editor, path, F_NOCONFIRM | F_IGNORELIST);
	if (ret < 0) ret = 0;
	if (ret) {
		builtinerror(argv, editor, -1);
		rmtmpfile(path);
		free(tmp);
		return(-1);
	}

	if (!(fp = Xfopen(path, "r"))) {
		builtinerror(argv, path, -1);
		rmtmpfile(path);
		free(tmp);
		return(-1);
	}
	free(tmp);

	for (n = 0; (cp = fgets2(fp, 0)); n++) {
		if (!*cp) {
			free(cp);
			continue;
		}
		kanjifputs(cp, stdout);
		fputnl(stdout);
		entryhist(0, cp, 0);
		ret = execmacro(cp, NULL,
			F_NOCONFIRM | F_ARGSET | F_IGNORELIST);
		free(cp);
	}
	Xfclose(fp);
	rmtmpfile(path);

	return(ret);
}
# endif	/* !NOPOSIXUTIL */

/*ARGSUSED*/
static VOID NEAR voidmd5(a, b, c, d)
u_long a, b, c, d;
{
	/* For bugs on NEWS-OS optimizer */
}

static VOID NEAR calcmd5(sum, x)
u_long sum[MD5_BUFSIZ], x[MD5_BLOCKS];
{
	u_long a, b, c, d, tmp, t[MD5_BLOCKS];
	int i, s[MD5_BUFSIZ];

	a = sum[0];
	b = sum[1];
	c = sum[2];
	d = sum[3];

	s[0] = 7;
	s[1] = 12;
	s[2] = 17;
	s[3] = 22;
	t[0] = 0xd76aa478;	/* floor(4294967296.0 * fabs(sin(1.0))) */
	t[1] = 0xe8c7b756;
	t[2] = 0x242070db;
	t[3] = 0xc1bdceee;
	t[4] = 0xf57c0faf;
	t[5] = 0x4787c62a;
	t[6] = 0xa8304613;
	t[7] = 0xfd469501;
	t[8] = 0x698098d8;
	t[9] = 0x8b44f7af;
	t[10] = 0xffff5bb1;
	t[11] = 0x895cd7be;
	t[12] = 0x6b901122;
	t[13] = 0xfd987193;
	t[14] = 0xa679438e;
	t[15] = 0x49b40821;
	for (i = 0; i < MD5_BLOCKS; i++) {
		a += ((b & c) | (~b & d)) + x[i] + t[i];
		tmp = b + ((a << s[i % MD5_BUFSIZ])
			| a >> (32 - s[i % MD5_BUFSIZ]));
		tmp &= (u_long)0xffffffff;
		a = d;
		d = c;
		c = b;
		b = tmp;
		voidmd5(a, b, c, d);
	}

	s[0] = 5;
	s[1] = 9;
	s[2] = 14;
	s[3] = 20;
	t[0] = 0xf61e2562;	/* floor(4294967296.0 * fabs(sin(16.0))) */
	t[1] = 0xc040b340;
	t[2] = 0x265e5a51;
	t[3] = 0xe9b6c7aa;
	t[4] = 0xd62f105d;
	t[5] = 0x02441453;
	t[6] = 0xd8a1e681;
	t[7] = 0xe7d3fbc8;
	t[8] = 0x21e1cde6;
	t[9] = 0xc33707d6;
	t[10] = 0xf4d50d87;
	t[11] = 0x455a14ed;
	t[12] = 0xa9e3e905;
	t[13] = 0xfcefa3f8;
	t[14] = 0x676f02d9;
	t[15] = 0x8d2a4c8a;
	for (i = 0; i < MD5_BLOCKS; i++) {
		a += ((b & d) | (c & ~d)) + x[(i * 5 + 1) % MD5_BLOCKS] + t[i];
		tmp = b + ((a << s[i % MD5_BUFSIZ])
			| a >> (32 - s[i % MD5_BUFSIZ]));
		tmp &= (u_long)0xffffffff;
		a = d;
		d = c;
		c = b;
		b = tmp;
		voidmd5(a, b, c, d);
	}

	s[0] = 4;
	s[1] = 11;
	s[2] = 16;
	s[3] = 23;
	t[0] = 0xfffa3942;	/* floor(4294967296.0 * fabs(sin(32.0))) */
	t[1] = 0x8771f681;
	t[2] = 0x6d9d6122;
	t[3] = 0xfde5380c;
	t[4] = 0xa4beea44;
	t[5] = 0x4bdecfa9;
	t[6] = 0xf6bb4b60;
	t[7] = 0xbebfbc70;
	t[8] = 0x289b7ec6;
	t[9] = 0xeaa127fa;
	t[10] = 0xd4ef3085;
	t[11] = 0x04881d05;
	t[12] = 0xd9d4d039;
	t[13] = 0xe6db99e5;
	t[14] = 0x1fa27cf8;
	t[15] = 0xc4ac5665;
	for (i = 0; i < MD5_BLOCKS; i++) {
		a += (b ^ c ^ d) + x[(i * 3 + 5) % MD5_BLOCKS] + t[i];
		tmp = b + ((a << s[i % MD5_BUFSIZ])
			| a >> (32 - s[i % MD5_BUFSIZ]));
		tmp &= (u_long)0xffffffff;
		a = d;
		d = c;
		c = b;
		b = tmp;
		voidmd5(a, b, c, d);
	}

	s[0] = 6;
	s[1] = 10;
	s[2] = 15;
	s[3] = 21;
	t[0] = 0xf4292244;	/* floor(4294967296.0 * fabs(sin(48.0))) */
	t[1] = 0x432aff97;
	t[2] = 0xab9423a7;
	t[3] = 0xfc93a039;
	t[4] = 0x655b59c3;
	t[5] = 0x8f0ccc92;
	t[6] = 0xffeff47d;
	t[7] = 0x85845dd1;
	t[8] = 0x6fa87e4f;
	t[9] = 0xfe2ce6e0;
	t[10] = 0xa3014314;
	t[11] = 0x4e0811a1;
	t[12] = 0xf7537e82;
	t[13] = 0xbd3af235;
	t[14] = 0x2ad7d2bb;
	t[15] = 0xeb86d391;
	for (i = 0; i < MD5_BLOCKS; i++) {
		a += (c ^ (b | ~d)) + x[(i * 7) % MD5_BLOCKS] + t[i];
		tmp = b + ((a << s[i % MD5_BUFSIZ])
			| a >> (32 - s[i % MD5_BUFSIZ]));
		tmp &= (u_long)0xffffffff;
		a = d;
		d = c;
		c = b;
		b = tmp;
		voidmd5(a, b, c, d);
	}

	sum[0] = (sum[0] + a) & (u_long)0xffffffff;
	sum[1] = (sum[1] + b) & (u_long)0xffffffff;
	sum[2] = (sum[2] + c) & (u_long)0xffffffff;
	sum[3] = (sum[3] + d) & (u_long)0xffffffff;
}

static int NEAR printmd5(path, fp)
char *path;
FILE *fp;
{
	FILE *fpin;
	u_long cl, ch, sum[MD5_BUFSIZ], x[MD5_BLOCKS];
	int i, c, b, n;

	if (!(fpin = Xfopen(path, "rb"))) return(-1);

	cl = ch = (u_long)0;
	sum[0] = (u_long)0x67452301;
	sum[1] = (u_long)0xefcdab89;
	sum[2] = (u_long)0x98badcfe;
	sum[3] = (u_long)0x10325476;

	n = b = 0;
	memset((char *)x, 0, sizeof(x));
	while ((c = Xfgetc(fpin)) != EOF) {
		if (cl <= (u_long)0xffffffff - (u_long)BITSPERBYTE)
			cl += (u_long)BITSPERBYTE;
		else {
			cl -= (u_long)0xffffffff - (u_long)BITSPERBYTE + 1;
			ch++;
			ch &= (u_long)0xffffffff;
		}

		x[n] |= c << b;
		if ((b += BITSPERBYTE) >= 32) {
			b = 0;
			if (++n >= MD5_BLOCKS) {
				n = 0;
				calcmd5(sum, x);
				memset((char *)x, 0, sizeof(x));
			}
		}
	}
	Xfclose(fpin);

	x[n] |= 1 << (b + BITSPERBYTE - 1);
	if (n >= 14) {
		calcmd5(sum, x);
		memset((char *)x, 0, sizeof(x));
	}
	x[14] = cl;
	x[15] = ch;
	calcmd5(sum, x);

	fprintf2(fp, "MD5 (%k) = ", path);
	for (i = 0; i < MD5_BUFSIZ; i++) for (n = 0; n < 4; n++) {
		fprintf2(fp, "%02x", (int)(sum[i] & 0xff));
		sum[i] >>= BITSPERBYTE;
	}
	fputnl(fp);

	return(0);
}

static int NEAR md5sum(argc, argv)
int argc;
char *argv[];
{
	char path[MAXPATHLEN];
	int i, ret;

	hitkey(2);
	if (argc < 2) {
# if	MSDOS || defined (CYGWIN)
		strcpy(strcatdelim2(path, progpath, progname), ".EXE");
# else
		strcatdelim2(path, progpath, progname);
# endif
		if (printmd5(path, stdout) < 0) {
			builtinerror(argv, path, -1);
			return(-1);
		}
		return(0);
	}

	ret = 0;
	for (i = 1; i < argc; i++) {
		if (printmd5(argv[i], stdout) < 0) {
			builtinerror(argv, argv[i], -1);
			ret = -1;
			continue;
		}
		hitkey(0);
	}

	return(ret);
}

static int NEAR evalmacro(argc, argv)
int argc;
char *argv[];
{
	char *cp;
	int ret;

	if (argc <= 1 || !(cp = catvar(&(argv[1]), ' '))) return(0);
	ret = execmacro(cp, NULL, F_NOCONFIRM | F_ARGSET | F_NOKANJICONV);
	if (ret < 0) ret = 0;
	free(cp);

	return(ret);
}

# ifndef	_NOKANJICONV
static int NEAR kconv(argc, argv)
int argc;
char *argv[];
{
	FILE *fpin, *fpout;
	char *cp, *tmp;
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
			fprintf2(stderr,
		"Usage: %s [-i inputcode] [-o outputcode] [filename]",
				argv[0]);
			fputnl(stderr);
			return(-1);
		}
	}

	if (i >= argc || !*(argv[i])) {
		fpin = stdin;
		fpout = stdout;
	}
	else if (!(fpin = Xfopen(argv[i], "r"))) {
		builtinerror(argv, argv[i], -1);
		return(-1);
	}
	else if (i + 1 >= argc || !*(argv[i + 1])) fpout = stdout;
	else if (!(fpout = Xfopen(argv[i + 1], "w"))) {
		builtinerror(argv, argv[i + 1], -1);
		Xfclose(fpin);
		return(-1);
	}

#  ifdef	_USEUNICODE
	if ((i = (in > out) ? in : out) >= UTF8) readunitable(i - UTF8);
#  endif
	while ((cp = fgets2(fpin, 0))) {
		if (in != DEFCODE) {
			tmp = newkanjiconv(cp, in, DEFCODE, L_OUTPUT);
			if (cp != tmp) free(cp);
			cp = tmp;
		}
		tmp = newkanjiconv(cp, DEFCODE, out, L_OUTPUT);
		if (cp != tmp) free(cp);
		fputs(tmp, fpout);
		fputnl(fpout);
		free(tmp);
	}

#  ifdef	_USEUNICODE
	if (!unicodebuffer) discardunitable();
#  endif
	if (fpin != stdin) Xfclose(fpin);
	else clearerr(fpin);
	if (fpout != stdout) Xfclose(fpout);

	return(0);
}
# endif	/* !_NOKANJICONV */

static int NEAR getinputstr(argc, argv)
int argc;
char *argv[];
{
	char *s, *duppromptstr;
	int wastty;

	if (dumbterm > 1) {
		builtinerror(argv, NULL, ER_NOTDUMBTERM);
		return(-1);
	}
	if (!(wastty = isttyiomode)) ttyiomode(1);
	duppromptstr = promptstr;
	promptstr = (argc >= 2) ? argv[1] : nullstr;
	s = inputstr(NULL, 0, -1, NULL, -1);
	promptstr = duppromptstr;
	if (!wastty) stdiomode();
	if (!s) return(-1);

	kanjifputs(s, stdout);
	fputnl(stdout);
	free(s);

	return(0);
}

static int NEAR getyesno(argc, argv)
int argc;
char *argv[];
{
	char *s;
	int ret, wastty;

	if (dumbterm > 1) {
		builtinerror(argv, NULL, ER_NOTDUMBTERM);
		return(-1);
	}
	if (argc < 2) s = nullstr;
	else s = argv[1];

	if (!(wastty = isttyiomode)) ttyiomode(1);
	lcmdline = -1;
	ret = yesno(s);
	if (!wastty) stdiomode();

	return((ret) ? 0 : -1);
}
#endif	/* FD >= 2 */

#if	!MSDOS
int savestdio(reset)
int reset;
{
	int n;

	if (isttyiomode) return(0);

	n = savettyio(reset);
# ifndef	_NOPTY
	sendparent(TE_SAVETTYIO, reset);
# endif
	return(n);
}

# if	FD >= 2
static int NEAR savetty(argc, argv)
int argc;
char *argv[];
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

#ifdef	_NOORIGSHELL
static int NEAR printenv(argc, argv)
int argc;
char *argv[];
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
		kanjifputs(environ2[i], stdout);
		fputnl(stdout);
		return(0);
	}
# endif

	hitkey(2);
	if (environ2) for (i = 0; environ2[i]; i++) {
		kanjifputs(environ2[i], stdout);
		fputnl(stdout);
		hitkey(0);
	}
# if	FD >= 2
	for (i = 0; i < maxuserfunc; i++) {
		printfunction(i, 0, stdout);
		fputnl(stdout);
		hitkey(0);
	}
# endif

	return(0);
}

int searchalias(ident, len)
char *ident;
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
		free(ident);
		free(comm);
		return(-1);
	}
	else if (i < maxalias) {
		free(aliaslist[i].alias);
		free(aliaslist[i].comm);
	}
	else maxalias++;

	aliaslist[i].alias = ident;
	aliaslist[i].comm = comm;
# ifdef	COMMNOCASE
	if (ident) for (i = 0; ident[i]; i++) ident[i] = tolower2(ident[i]);
# endif
# ifndef	 _NOPTY
	sendparent(TE_ADDALIAS, ident, comm);
# endif

	return(0);
}

int deletealias(ident)
char *ident;
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
		free(aliaslist[i].alias);
		free(aliaslist[i].comm);
		memmove((char *)&(aliaslist[i]), (char *)&(aliaslist[i + 1]),
			(--maxalias - i) * sizeof(aliastable));
		i--;
		n++;
	}
	if (re) regexp_free(re);
	if (!n) return(-1);
# ifndef	 _NOPTY
	sendparent(TE_DELETEALIAS, ident);
# endif

	return(0);
}

VOID printalias(n, fp)
int n;
FILE *fp;
{
	fprintf2(fp, "%s %k%c", BL_ALIAS, aliaslist[n].alias, ALIASSEP);
	fputsmeta(aliaslist[n].comm, fp);
}

static int NEAR setalias(argc, argv)
int argc;
char *argv[];
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
			printalias(i, stdout);
			fputnl(stdout);
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
		printalias(i, stdout);
		fputnl(stdout);

		return(0);
	}

	if (addalias(strndup2(argv[1], len), strdup2(cp)) < 0) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		return(-1);
	}

	return(0);
}

/*ARGSUSED*/
static int NEAR unalias(argc, argv)
int argc;
char *argv[];
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
char *ident;
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
		free(ident);
		freevar(comm);
		return(-1);
	}
	else if (i < maxuserfunc) {
		free(userfunclist[i].func);
		freevar(userfunclist[i].comm);
	}
	else maxuserfunc++;

	userfunclist[i].func = ident;
	userfunclist[i].comm = comm;
# ifdef	COMMNOCASE
	if (ident) for (i = 0; ident[i]; i++) ident[i] = tolower2(ident[i]);
# endif
# ifndef	 _NOPTY
	sendparent(TE_ADDFUNCTION, ident, comm);
# endif

	return(0);
}

int deletefunction(ident)
char *ident;
{
	int i;

	if ((i = searchfunction(ident)) >= maxuserfunc) return(-1);

	free(userfunclist[i].func);
	freevar(userfunclist[i].comm);
	memmove((char *)&(userfunclist[i]),
		(char *)&(userfunclist[i + 1]),
		(--maxuserfunc - i) * sizeof(userfunctable));
# ifndef	 _NOPTY
	sendparent(TE_DELETEFUNCTION, ident);
# endif

	return(0);
}

VOID printfunction(no, verbose, fp)
int no, verbose;
FILE *fp;
{
	int i;

# if	FD < 2
	fprintf2(fp, "%s ", BL_FUNCTION);
# endif
	fprintf2(fp, "%k() {", userfunclist[no].func);
	if (verbose) fputnl(fp);
	for (i = 0; userfunclist[no].comm[i]; i++) {
		if (verbose) fprintf2(fp, "\t%k;\n", userfunclist[no].comm[i]);
		else fprintf2(fp, " %k;", userfunclist[no].comm[i]);
	}
	fputs(" }", fp);
}

static int NEAR checkuserfunc(argc, argv)
int argc;
char *argv[];
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
		for (i = 1; i <= n; i++) free(argv[i]);
		memmove((char *)&(argv[1]), (char *)&(argv[n + 1]),
			(argc -= n) * sizeof(char *));
	}

	return(argc);
}

static int NEAR setuserfunc(argc, argv)
int argc;
char *argv[];
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
			printfunction(i, 0, stdout);
			fputnl(stdout);
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
		printfunction(i, 1, stdout);
		fputnl(stdout);
		return(0);
	}
# endif	/* FD < 2 */

	cp = line = catvar(&(argv[FUNCNAME + 1]), ' ');
	for (n = 0; n < MAXFUNCLINES && *cp; n++) {
		if (!(tmp = strtkchr(cp, ';', 0))) break;
		*(tmp++) = '\0';
		list[n] = strdup2(cp);
		cp = skipspace(tmp);
	}
	if (!(tmp = strtkchr(cp, '}', 0)) || *(tmp + 1)) {
		builtinerror(argv, line, ER_SYNTAXERR);
		for (j = 0; j < n; j++) free(list[j]);
		free(line);
		return(-1);
	}

	if (tmp > cp) {
		*tmp = '\0';
		list[n++] = strdup2(cp);
		free(line);
	}
	else if (!n) {
		free(line);
		if (deletefunction(argv[FUNCNAME]) < 0) {
			builtinerror(argv, argv[FUNCNAME], ER_NOENTRY);
			return(-1);
		}

		return(0);
	}

	var = (char **)malloc2((n + 1) * sizeof(char *));
	for (i = 0; i < n; i++) var[i] = list[i];
	var[i] = NULL;
	if (addfunction(strdup2(argv[FUNCNAME]), var) < 0) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		return(-1);
	}

	return(0);
}

static int NEAR exportenv(argc, argv)
int argc;
char *argv[];
{
	char *cp;
	int i;

	if (argc <= 1) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	i = argc - 1;
	if ((cp = getenvval(&i, &(argv[1]))) == (char *)-1) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	if (i + 1 < argc) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	setenv2(argv[1], cp, 1);
	if (cp) free(cp);
	adjustpath();

	return(0);
}

static int NEAR dochdir(argc, argv)
int argc;
char *argv[];
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
char *argv[];
{
	int ret;

	if (argc != 2) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	ttyiomode(1);
	ret = loadruncom(argv[1], 1);
	stdiomode();
	if (ret < 0) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}

	return(0);
}
#endif	/* _NOORIGSHELL */

int checkbuiltin(comname)
char *comname;
{
	int i;

	for (i = 0; i < BUILTINSIZ; i++)
		if (!strcommcmp(comname, builtinlist[i].ident)) return(i);

	return(-1);
}

int checkinternal(comname)
char *comname;
{
	int i;

	for (i = 0; i < FUNCLISTSIZ; i++)
		if (!strcommcmp(comname, funclist[i].ident)) return(i);

	return(-1);
}

int execbuiltin(n, argc, argv)
int n, argc;
char *argv[];
{
	return(((*builtinlist[n].func)(argc, argv) < 0)
		? RET_NOTICE : RET_SUCCESS);
}

int execinternal(n, argc, argv)
int n, argc;
char *argv[];
{
	if (dumbterm > 1) {
		builtinerror(argv, NULL, ER_NOTDUMBTERM);
		return(-1);
	}
	if (fd_restricted && (funclist[n].status & FN_RESTRICT)) {
		fprintf2(stderr, "%s: %k", argv[0], RESTR_K);
		fputnl(stderr);
		return(RET_NOTICE);
	}
	if (argc > 2 || !filelist || maxfile <= 0) {
		fprintf2(stderr, "%s: %k", argv[0], ILFNC_K);
		fputnl(stderr);
		return(RET_NOTICE);
	}
#ifndef	_NOPTY
	if (parentfd >= 0) {
		sendparent(TE_INTERNAL, n, argv[1]);
		return(RET_SUCCESS);
	}
#endif
	ttyiomode(0);
	internal_status = (*funclist[n].func)(argv[1]);
	locate(0, n_line - 1);
	stdiomode();

	return(RET_SUCCESS);
}

#ifdef	_NOORIGSHELL
int execpseudoshell(command, flags)
char *command;
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
		if ((cp = getenvval(&i, argv)) != (char *)-1 && i == argc) {
			if (setenv2(argv[0], cp, 0) < 0) error(argv[0]);
			if (cp) free(cp);
			n = RET_SUCCESS;
		}
	}

	freevar(argv);

	return(n);
}
#endif	/* _NOORIGSHELL */

#ifndef	_NOCOMPLETE
int completebuiltin(com, len, argc, argvp)
char *com;
int len, argc;
char ***argvp;
{
	int i;

	for (i = 0; i < BUILTINSIZ; i++) {
		if (strncommcmp(com, builtinlist[i].ident, len)
		|| finddupl(builtinlist[i].ident, argc, *argvp))
			continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(builtinlist[i].ident);
	}

	return(argc);
}

int completeinternal(com, len, argc, argvp)
char *com;
int len, argc;
char ***argvp;
{
	int i;

	for (i = 0; i < FUNCLISTSIZ; i++) {
		if (strncommcmp(com, funclist[i].ident, len)
		|| finddupl(funclist[i].ident, argc, *argvp))
			continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(funclist[i].ident);
	}

	return(argc);
}
#endif	/* !_NOCOMPLETE */

#ifdef	DEBUG
VOID freedefine(VOID_A)
{
	int i;

# ifndef	_NOARCHIVE
	for (i = 0; i < maxlaunch; i++) freelaunch(&(launchlist[i]));
	for (i = 0; i < maxarchive; i++) freearch(&(archivelist[i]));
# endif	/* !_NOARCHIVE */
	for (i = 0; i < maxmacro; i++) free(macrolist[i]);
# ifdef	_NOORIGSHELL
	for (i = 0; i < maxalias; i++) {
		free(aliaslist[i].alias);
		free(aliaslist[i].comm);
	}
	for (i = 0; i < maxuserfunc; i++) {
		int j;

		free(userfunclist[i].func);
		for (j = 0; userfunclist[i].comm[j]; j++)
			free(userfunclist[i].comm[j]);
		free(userfunclist[i].comm);
	}
# endif	/* _NOORIGSHELL */
# ifdef	_USEDOSEMU
	for (i = 0; fdtype[i].name; i++) free(fdtype[i].name);
# endif
	for (i = 0; i < MAXHELPINDEX; i++) free(helpindex[i]);
}
#endif
