/*
 *	builtin.c
 *
 *	Builtin Command
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "funcno.h"
#include "kanji.h"

#ifdef	_NOORIGSHELL
#define	RET_SUCCESS	0
#define	RET_NOTICE	255
#else
#include "system.h"
#endif

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
#if	!MSDOS && !defined (_NODOSDRIVE)
extern devinfo fdtype[];
#endif
extern char **history[];
extern short histsize[];
extern short histno[];
extern char *helpindex[];
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

static VOID NEAR builtinerror __P_((char *[], char *, int));
#ifdef	_NOORIGSHELL
static VOID NEAR hitkey __P_((int));
#else
#define	hitkey(n)
#endif
static VOID NEAR fputsmeta __P_((char *, FILE *));
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
static int NEAR getcommand __P_((char *));
static int NEAR setkeybind __P_((int, char *[]));
static int NEAR printbind __P_((int, char *[]));
#if	!MSDOS && !defined (_NODOSDRIVE)
static int NEAR _setdrive __P_((int, char *[], int));
static int NEAR setdrive __P_((int, char *[]));
static int NEAR unsetdrive __P_((int, char *[]));
static int NEAR printdrive __P_((int, char *[]));
#endif
#if	!MSDOS && !defined (_NOKEYMAP)
static int NEAR setkeymap __P_((int, char *[]));
static int NEAR keytest __P_((int, char *[]));
#endif
static int NEAR printhist __P_((int, char *[]));
#ifndef	NOPOSIXUTIL
static int NEAR fixcommand __P_((int, char *[]));
#endif
#if	FD >= 2
static VOID NEAR voidmd5 __P_((u_long, u_long, u_long, u_long));
static VOID NEAR calcmd5 __P_((u_long [], u_long []));
static int NEAR printmd5 __P_((char *, FILE *));
static int NEAR md5sum __P_((int, char *[]));
static int NEAR evalmacro __P_((int, char *[]));
# if	!MSDOS && !defined (_NOKANJICONV)
static int NEAR kconv __P_((int, char *[]));
# endif
static int NEAR getinputstr __P_((int, char *[]));
static int NEAR getyesno __P_((int, char *[]));
#endif	/* FD >= 2 */
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

static char *builtinerrstr[] = {
	"",
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
#define	BUILTINERRSIZ	((int)(sizeof(builtinerrstr) / sizeof(char *)))

static builtintable builtinlist[] = {
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
#if	!MSDOS && !defined (_NODOSDRIVE)
	{setdrive,	BL_SDRIVE},
	{unsetdrive,	BL_UDRIVE},
	{printdrive,	BL_PDRIVE},
#endif
#if	!MSDOS && !defined (_NOKEYMAP)
	{setkeymap,	BL_KEYMAP},
	{keytest,	BL_GETKEY},
#endif
	{printhist,	BL_HISTORY},
#ifndef	NOPOSIXUTIL
	{fixcommand,	BL_FC},
#endif
#if	FD >= 2
	{md5sum,	BL_CHECKID},
	{evalmacro,	BL_EVALMACRO},
# if	!MSDOS && !defined (_NOKANJICONV)
	{kconv,		BL_KCONV},
# endif
	{getinputstr,	BL_READLINE},
	{getyesno,	BL_YESNO},
#endif	/* FD >= 2 */
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
#define	BUILTINSIZ	((int)(sizeof(builtinlist) / sizeof(builtintable)))


static VOID NEAR builtinerror(argv, s, n)
char *argv[], *s;
int n;
{
	int duperrno;

	duperrno = errno;
	if (n >= BUILTINERRSIZ || (n < 0 && !errno)) return;
	if (argv && argv[0]) {
		kanjifputs(argv[0], stderr);
		fputs(": ", stderr);
	}
	if (s) {
		kanjifputs(s, stderr);
		fputs(": ", stderr);
	}
	fprintf2(stderr, "%s.\n",
		(n >= 0) ? builtinerrstr[n] : strerror2(duperrno));
	fflush(stderr);
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

#ifndef	_NOARCHIVE
# if	FD >= 2
static int NEAR getlaunchopt(n, argv, opts, list)
int n;
char *argv[], *opts;
launchtable *list;
{
	char *cp;
	int i, c;

	if (argv[n][0] != '-') return(0);
	c = argv[n][1];
	if (!c || c == '-') return(++n);
	else if (!strchr(opts, c)) {
		builtinerror(argv, argv[n], ER_UNKNOWNOPT);
		return(-1);
	}

	if (argv[n][2]) cp = &(argv[n][2]);
	else if (argv[n + 1]) cp = argv[++n];
	else {
		builtinerror(argv, argv[n], ER_NOARGSPEC);
		return(-1);
	}

	if (c == 'f') {
		i = countvar(list -> format);
		list -> format = (char **)realloc2(list -> format,
			sizeof(char *) * (i + 2));
		list -> format[i++] = strdup2(cp);
		list -> format[i] = NULL;
	}
	else if (c == 'i') {
		i = countvar(list -> lignore);
		list -> lignore = (char **)realloc2(list -> lignore,
			sizeof(char *) * (i + 2));
		list -> lignore[i++] = strdup2(cp);
		list -> lignore[i] = NULL;
	}
	else if (c == 'e') {
		i = countvar(list -> lerror);
		list -> lerror = (char **)realloc2(list -> lerror,
			sizeof(char *) * (i + 2));
		list -> lerror[i++] = strdup2(cp);
		list -> lerror[i] = NULL;
	}
	else if (c == 't' || c == 'b') {
		if ((i = atoi2(cp)) < 0) {
			builtinerror(argv, cp, ER_SYNTAXERR);
			return(-1);
		}
		if (c == 't') list -> topskip = i;
		else list -> bottomskip = i;
	}
	else if (c == 'p') {
		if (list -> ext) free (list -> ext);
		list -> ext = strdup2(cp);
	}
	else {
		if (!strcmp(cp, "loop"))
			i = (c == 'd') ? LF_DIRLOOP : LF_FILELOOP;
		else if (!strcmp(cp, "noprep"))
			i = (c == 'd') ? LF_DIRNOPREP : LF_FILENOPREP;
		else {
			builtinerror(argv, cp, ER_SYNTAXERR);
			return(-1);
		}
		list -> flags |= i;
	}
	n++;

	return(n);
}
# endif

static int NEAR setlaunch(argc, argv)
int argc;
char *argv[];
{
# if	FD >= 2
	int j, opt, err;
# else
	char *cp, *tmp;
	long l;
	int ch;
# endif
	launchtable list;
	int i, n;

	if (argc <= 1) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}

	list.flags = 0;
	list.ext = getext(argv[1], &(list.flags));

	for (n = 0; n < maxlaunch; n++)
		if (!extcmp(list.ext, list.flags,
		launchlist[n].ext, launchlist[n].flags, 1)) break;

	if (n >= MAXLAUNCHTABLE) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		free(list.ext);
		return(-1);
	}

	if (argc < 3) {
		free(list.ext);
		if (n >= maxlaunch) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}

		free(launchlist[n].ext);
		free(launchlist[n].comm);
# if	FD >= 2
		freevar(launchlist[n].format);
		freevar(launchlist[n].lignore);
		freevar(launchlist[n].lerror);
# endif
		maxlaunch--;
		for (; n < maxlaunch; n++)
			memcpy((char *)&(launchlist[n]),
				(char *)&(launchlist[n + 1]),
				sizeof(launchtable));
		return(0);
	}

# if	FD >= 2
	list.format = list.lignore = list.lerror = NULL;
	list.comm = NULL;
	list.topskip = list.bottomskip = 0;
	opt = err = 0;
	for (i = 2; i < argc; i++) {
		j = getlaunchopt(i, argv, "fietb", &list);
		if (!j) {
			if (list.comm) break;
			list.comm = strdup2(argv[i]);
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
	else if (!(list.comm)) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		err++;
	}
	else if (i + 3 < argc) {
		builtinerror(argv, argv[i + 3], ER_FEWMANYARG);
		err++;
	}
	else if (i < argc && opt) {
		builtinerror(argv, argv[i], ER_FEWMANYARG);
		err++;
	}
	if (err) {
		free(list.ext);
		free(list.comm);
		freevar(list.format);
		freevar(list.lignore);
		freevar(list.lerror);
		return(-1);
	}

	if (i < argc) {
		list.format = (char **)malloc2(sizeof(char *) * 2);
		list.format[0] = strdup2(argv[i]);
		list.format[1] = NULL;
	}

	if (i + 1 >= argc);
	else if ((j = atoi2(argv[i + 1])) >= 0) list.topskip = j;
	else {
		builtinerror(argv, argv[i + 1], ER_SYNTAXERR);
		free(list.ext);
		free(list.comm);
		freevar(list.format);
		freevar(list.lignore);
		freevar(list.lerror);
		return(-1);
	}

	if (i + 2 >= argc);
	else if ((j = atoi2(argv[i + 2])) >= 0) list.bottomskip = j;
	else {
		builtinerror(argv, argv[i + 2], ER_SYNTAXERR);
		free(list.ext);
		free(list.comm);
		freevar(list.format);
		freevar(list.lignore);
		freevar(list.lerror);
		return(-1);
	}

# else	/* FD < 2 */
	list.comm = strdup2(argv[2]);
	if (argc <= 3) list.topskip = list.bottomskip = 255;
	else {
		cp = tmp = catvar(&(argv[3]), '\0');
		if (!(cp = evalnumeric(cp, &l, 0)) || *cp != ',') {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(list.ext);
			free(list.comm);
			free(tmp);
			return(-1);
		}
		list.topskip = l;
		if (!(cp = evalnumeric(++cp, &l, 0))) {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(list.ext);
			free(list.comm);
			free(tmp);
			return(-1);
		}
		list.bottomskip = l;

		ch = ':';
		for (i = 0; i < MAXLAUNCHFIELD; i++) {
			if (!cp || *cp != ch) {
				builtinerror(argv, tmp, ER_SYNTAXERR);
				free(list.ext);
				free(list.comm);
				free(tmp);
				return(-1);
			}
			cp = getrange(++cp, &(list.field[i]),
				&(list.delim[i]), &(list.width[i]));
			ch = ',';
		}

		ch = ':';
		for (i = 0; i < MAXLAUNCHSEP; i++) {
			if (*cp != ch) break;
			if (!(cp = evalnumeric(++cp, &l, 0))) {
				builtinerror(argv, tmp, ER_SYNTAXERR);
				free(list.ext);
				free(list.comm);
				free(tmp);
				return(-1);
			}
			list.sep[i] = (l > 0) ? l - 1 : 255;
			ch = ',';
		}
		for (; i < MAXLAUNCHSEP; i++) list.sep[i] = 255;

		if (!cp || *cp != ':') list.lines = 1;
		else {
			if (!(cp = evalnumeric(++cp, &l, 0))) {
				builtinerror(argv, tmp, ER_SYNTAXERR);
				free(list.ext);
				free(list.comm);
				free(tmp);
				return(-1);
			}
			list.lines = (l > 0) ? l : 1;
		}

		if (*cp) {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(list.ext);
			free(list.comm);
			free(tmp);
			return(-1);
		}
		free(tmp);
	}
# endif	/* FD < 2 */

	if (n >= maxlaunch) maxlaunch++;
	else {
		free(launchlist[n].comm);
		free(launchlist[n].ext);
# if	FD >= 2
		freevar(launchlist[n].format);
		freevar(launchlist[n].lignore);
		freevar(launchlist[n].lerror);
# endif
	}
	memcpy((char *)&(launchlist[n]), (char *)&list, sizeof(list));
	return(0);
}

static int NEAR setarch(argc, argv)
int argc;
char *argv[];
{
	char *ext;
	int i;
	u_char flags;

	if (argc <= 1 || argc >= 5) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}

	ext = getext(argv[1], &flags);

	for (i = 0; i < maxarchive; i++)
		if (!extcmp(ext, flags,
		archivelist[i].ext, archivelist[i].flags, 1)) break;

	if (i >= MAXARCHIVETABLE) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		free(ext);
		return(-1);
	}

	if (argc < 3) {
		free(ext);
		if (i >= maxarchive) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}

		free(archivelist[i].ext);
		if (archivelist[i].p_comm) free(archivelist[i].p_comm);
		if (archivelist[i].u_comm) free(archivelist[i].u_comm);
		maxarchive--;
		for (; i < maxarchive; i++)
			memcpy((char *)&(archivelist[i]),
				(char *)&(archivelist[i + 1]),
				sizeof(archivetable));
		return(0);
	}

	if (i >= maxarchive) maxarchive++;
	else {
		free(archivelist[i].ext);
		if (archivelist[i].p_comm) free(archivelist[i].p_comm);
		if (archivelist[i].u_comm) free(archivelist[i].u_comm);
	}
	archivelist[i].ext = ext;
	archivelist[i].p_comm = (argv[2][0]) ? strdup2(argv[2]) : NULL;
	archivelist[i].u_comm =
		(argc > 3 && argv[3][0]) ? strdup2(argv[3]) : NULL;
# if	FD < 2
	flags = 0;
# endif
	archivelist[i].flags = flags;
	return(0);
}

/*ARGSUSED*/
VOID printlaunchcomm(n, omit, fp)
int n, omit;
FILE *fp;
{
	int i, ch;

	fputs(BL_LAUNCH, fp);
	fputc(' ', fp);
	if (launchlist[n].flags & LF_IGNORECASE) fputc('/', fp);
	fputsmeta(&(launchlist[n].ext[1]), fp);
	fputc('\t', fp);

	fputsmeta(launchlist[n].comm, fp);
# if	FD >= 2
	if (!launchlist[n].format) return;

	ch = 0;
	for (i = 0; launchlist[n].format[i]; i++) {
		if (i) {
			fputs(" \\\n", fp);
			ch++;
		}
		fputs("\t-f ", fp);
		fputsmeta(launchlist[n].format[i], fp);
	}
	if (launchlist[n].lignore) for (i = 0; launchlist[n].lignore[i]; i++) {
		fputs(" \\\n\t-i ", fp);
		ch++;
		fputsmeta(launchlist[n].lignore[i], fp);
	}
	if (launchlist[n].lerror) for (i = 0; launchlist[n].lerror[i]; i++) {
		fputs(" \\\n\t-e ", fp);
		ch++;
		fputsmeta(launchlist[n].lerror[i], fp);
	}
	if (!launchlist[n].topskip && !launchlist[n].bottomskip) return;

	if (launchlist[n].topskip) {
		if (ch) fputs(" \\\n", fp);
		fprintf2(fp, "\t-t %d", launchlist[n].topskip);
	}
	if (launchlist[n].bottomskip) {
		if (launchlist[n].topskip) fputc(' ', fp);
		else if (ch) fputs(" \\\n\t", fp);
		else fputc('\t', fp);
		fprintf2(fp, "-b %d", launchlist[n].bottomskip);
	}
# else	/* FD < 2 */
	if (launchlist[n].topskip >= 255) return;
	if (omit) {
		fputs("\t(Arch)", fp);
		return;
	}
	fprintf2(fp, "\t%d,%d",
		launchlist[n].topskip, launchlist[n].bottomskip);
	ch = ':';
	for (i = 0; i < MAXLAUNCHFIELD; i++) {
		fputc(ch, fp);
		if (launchlist[n].field[i] >= 255) fputc('0', fp);
		else fprintf2(fp, "%d", launchlist[n].field[i] + 1);
		if (launchlist[n].delim[i] >= 128)
			fprintf2(fp, "[%d]", launchlist[n].delim[i] - 128);
		else if (launchlist[n].delim[i])
			fprintf2(fp, "'%c'", launchlist[n].delim[i]);
		if (!(launchlist[n].width[i])) /*EMPTY*/;
		else if (launchlist[n].width[i] >= 128)
			fprintf2(fp, "-%d", launchlist[n].width[i] - 128);
		else fprintf2(fp, "-'%c'", launchlist[n].width[i]);
		ch = ',';
	}
	ch = ':';
	for (i = 0; i < MAXLAUNCHSEP; i++) {
		if (launchlist[n].sep[i] >= 255) break;
		fprintf2(fp, "%c%d", ch, launchlist[n].sep[i] + 1);
		ch = ',';
	}
	if (launchlist[n].lines > 1) {
		if (!i) fputc(':', fp);
		fprintf2(fp, "%d", launchlist[n].lines);
	}
# endif	/* FD < 2 */
}

static int NEAR printlaunch(argc, argv)
int argc;
char *argv[];
{
	char *ext;
	int i;
	u_char flags;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	hitkey(2);
	if (argc <= 1) for (i = 0; i < maxlaunch; i++) {
		printlaunchcomm(i, 1, stdout);
		fputc('\n', stdout);
		hitkey(0);
	}
	else {
		ext = getext(argv[1], &flags);
		for (i = 0; i < maxlaunch; i++)
			if (!extcmp(ext, flags,
			launchlist[i].ext, launchlist[i].flags, 0)) break;
		free(ext);
		if (i >= maxlaunch) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		printlaunchcomm(i, 0, stdout);
		fputc('\n', stdout);
	}
	fflush(stdout);
	return(0);
}

VOID printarchcomm(n, fp)
int n;
FILE *fp;
{
	fputs(BL_ARCH, fp);
	fputc(' ', fp);
	if (archivelist[n].flags & LF_IGNORECASE) fputc('/', fp);
	fputsmeta(&(archivelist[n].ext[1]), fp);
	fputc('\t', fp);

	if (archivelist[n].p_comm) fputsmeta(archivelist[n].p_comm, fp);
	if (!archivelist[n].u_comm) return;
	fputc('\t', fp);
	fputsmeta(archivelist[n].u_comm, fp);
}

static int NEAR printarch(argc, argv)
int argc;
char *argv[];
{
	char *ext;
	int i;
	u_char flags;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	hitkey(2);
	if (argc <= 1) for (i = 0; i < maxarchive; i++) {
		printarchcomm(i, stdout);
		fputc('\n', stdout);
		hitkey(0);
	}
	else {
		ext = getext(argv[1], &flags);
		for (i = 0; i < maxarchive; i++)
			if (!extcmp(ext, flags,
			archivelist[i].ext, archivelist[i].flags, 0)) break;
		free(ext);
		if (i >= maxarchive) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		printarchcomm(i, stdout);
		fputc('\n', stdout);
	}
	fflush(stdout);
	return(0);
}

# ifndef	_NOBROWSE
static char **NEAR readargv(sargv, dargv)
char **sargv, **dargv;
{
	FILE *fp;
	char *cp, *line, **argv;
	ALLOC_T size;
	int i, j, n, argc, dargc, meta, quote, quoted;

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

		argc = 1;
		argv = (char **)malloc2(2 * sizeof(char *));
		argv[0] = strdup2(sargv[0]);
		argv[1] = NULL;
		j = meta = 0;
		quote = quoted = '\0';
		cp = c_realloc(NULL, 0, &size);
		while ((line = fgets2(fp, 0))) {
			if (!meta && !quote && *line == '#') {
				free(line);
				continue;
			}
			meta = 0;
			for (i = 0; line[i]; i++) {
				cp = c_realloc(cp, j + 2, &size);
				if (line[i] == quote) {
					quoted = quote;
					quote = '\0';
				}
#  ifdef	CODEEUC
				else if (isekana(line, i)) {
					cp[j++] = line[i++];
					cp[j++] = line[i];
				}
#  endif
				else if (iskanji1(line, i)) {
					cp[j++] = line[i++];
					cp[j++] = line[i];
				}
				else if (quote) cp[j++] = line[i];
				else if (line[i] == PMETA) {
					if (!line[++i]) {
						meta = 1;
						break;
					}
					cp[j++] = line[i];
				}
				else if (line[i] == '\'' || line[i] == '"') {
					quoted = '\0';
					quote = line[i];
				}
				else if (!strchr(IFS_SET, line[i]))
					cp[j++] = line[i];
				else if (j || quoted) {
					quoted = cp[j] = '\0';
					argv = (char **)realloc2(argv,
						(argc + 2) * sizeof(char *));
					argv[argc++] = strdup2(cp);
					argv[argc] = NULL;
					j = 0;
				}
			}
			if (meta);
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
		}
		if (fp != stdin) Xfclose(fp);
		else clearerr(fp);
		if (j) {
			cp[j] = '\0';
			argv = (char **)realloc2(argv,
				(argc + 2) * sizeof(char *));
			argv[argc++] = strdup2(cp);
			argv[argc] = NULL;
		}
		free(cp);
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
	for (i = 0; list[i].comm; i++) {
		if (list[i].ext) free(list[i].ext);
		free(list[i].comm);
		freevar(list[i].format);
		freevar(list[i].lignore);
		freevar(list[i].lerror);
	}
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
				freevar(argv2);
				freebrowse(list);
				return(-1);
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
		if (dolaunch(&(list[0]), 1) < 0) {
			freebrowse(list);
			return(-1);
		}
		browselist = list;
	}
	return(0);
}
# endif	/* !_NOBROWSE */
#endif	/* !_NOARCHIVE */

static int NEAR getcommand(cp)
char *cp;
{
	int n;

	for (n = 0; n < FUNCLISTSIZ; n++)
		if (!strpathcmp(cp, funclist[n].ident)) break;
	if (n < FUNCLISTSIZ) /*EMPTY*/;
	else if (maxmacro >= MAXMACROTABLE) n = -1;
	else {
		macrolist[maxmacro] = strdup2(cp);
		return(FUNCLISTSIZ + maxmacro++);
	}
	return(n);
}

int freemacro(n)
int n;
{
	int i;

	if (n < FUNCLISTSIZ || n >= 255) return(-1);

	free(macrolist[n - FUNCLISTSIZ]);
	maxmacro--;
	for (i = n - FUNCLISTSIZ; i < maxmacro; i++)
		macrolist[i] = macrolist[i + 1];

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (bindlist[i].f_func > n) bindlist[i].f_func--;
		if (bindlist[i].d_func > n && bindlist[i].d_func < 255)
			bindlist[i].d_func--;
	}

	return(0);
}

static int NEAR setkeybind(argc, argv)
int argc;
char *argv[];
{
	int i, j, ch, n1, n2;

	if (argc <= 1 || argc >= 6) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	if ((ch = getkeycode(argv[1], 0)) < 0 || ch == '\033') {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
		if (ch == (int)(bindlist[i].key)) break;
	if (i >= MAXBINDTABLE - 1) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		return(-1);
	}

	if (argc == 2) {
		if (bindlist[i].key < 0) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}

		freemacro(bindlist[i].f_func);
		freemacro(bindlist[i].d_func);
		if (ch >= K_F(1) && ch <= K_F(10)) {
			free(helpindex[ch - K_F(1)]);
			helpindex[ch - K_F(1)] = strdup2("");
		}
		for (; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			memcpy((char *)&(bindlist[i]),
				(char *)&(bindlist[i + 1]),
				sizeof(bindtable));
		return(0);
	}

	if ((n1 = getcommand(argv[2])) < 0) {
		builtinerror(argv, argv[2], ER_OUTOFLIMIT);
		return(-1);
	}

	n2 = 255;
	j = 4;
	if (argc > 3) {
		if (argv[3][0] == BINDCOMMENT) j = 3;
		else if ((n2 = getcommand(argv[3])) < 0) {
			builtinerror(argv, argv[3], ER_OUTOFLIMIT);
			freemacro(n1);
			return(-1);
		}
	}
	if (argc > j) {
		if (argv[j][0] != BINDCOMMENT || ch < K_F(1) || ch > K_F(10)) {
			builtinerror(argv, NULL, ER_FEWMANYARG);
			freemacro(n1);
			freemacro(n2);
			return(-1);
		}
		free(helpindex[ch - K_F(1)]);
		helpindex[ch - K_F(1)] = strdup2(&(argv[j][1]));
	}

	if (bindlist[i].key < 0) {
		memcpy((char *)&(bindlist[i + 1]), (char *)&(bindlist[i]),
			sizeof(bindtable));
		bindlist[i].f_func = (u_char)n1;
		bindlist[i].d_func = (u_char)n2;
	}
	else {
		j = bindlist[i].f_func;
		bindlist[i].f_func = (u_char)n1;
		freemacro(j);
		j = bindlist[i].d_func;
		bindlist[i].d_func = (u_char)n2;
		freemacro(j);
	}
	bindlist[i].key = (short)ch;
	return(0);
}

VOID printmacro(n, fp)
int n;
FILE *fp;
{
	fputs(BL_BIND, fp);
	fputc(' ', fp);
	fputsmeta(getkeysym(bindlist[n].key, 0), fp);
	fputc('\t', fp);

	if (bindlist[n].f_func < FUNCLISTSIZ)
		fputs(funclist[bindlist[n].f_func].ident, fp);
	else fputsmeta(macrolist[bindlist[n].f_func - FUNCLISTSIZ], fp);
	if (bindlist[n].d_func < FUNCLISTSIZ)
		fprintf2(fp, "\t%s", funclist[bindlist[n].d_func].ident);
	else if (bindlist[n].d_func < 255) {
		fputc('\t', fp);
		fputsmeta(macrolist[bindlist[n].d_func - FUNCLISTSIZ], fp);
	}
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
		if (bindlist[i].f_func < FUNCLISTSIZ
		&& (bindlist[i].d_func < FUNCLISTSIZ
		|| bindlist[i].d_func == 255)) continue;
		printmacro(i, stdout);
		fputc('\n', stdout);
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
		printmacro(i, stdout);
		fputc('\n', stdout);
	}
	fflush(stdout);
	return(0);
}

#if	!MSDOS && !defined (_NODOSDRIVE)
/*ARGSUSED*/
int searchdrv(list, drive, name, head, sect, cyl, set)
devinfo *list;
int drive;
char *name;
int head, sect, cyl, set;
{
	int i;

	for (i = 0; list[i].name; i++) if (drive == list[i].drive) {
# ifdef	HDDMOUNT
		if (!list[i].cyl && set) break;
# endif
		if (head == list[i].head
		&& sect == list[i].sect
		&& cyl == list[i].cyl
		&& !strpathcmp(name, list[i].name)) break;
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
			|| strpathcmp(dev, fdtype[no - s - 1].name)) break;
		}
		for (n = 1; fdtype[no + n].name; n++) {
			if (fdtype[no + n].cyl
			|| strpathcmp(dev, fdtype[no + n].name)) break;
		}
		no -= s;
		n += s;
	}
# endif	/* HDDMOUNT */
	for (i = 0; i < n; i++) free(fdtype[no + i].name);
	for (i = no; fdtype[i + n].name; i++)
		memcpy((char *)&(fdtype[i]),
			(char *)&(fdtype[i + n]), sizeof(devinfo));
	fdtype[i].name = NULL;
	return(no);
}

int insertdrv(no, drive, dev, head, sect, cyl)
int no, drive;
char *dev;
int head, sect, cyl;
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
	for (i = 0; fdtype[i].name; i++)
	if (!strpathcmp(dev, fdtype[i].name)) {
		if (min < 0) min = i;
		max = i;
	}
	if (min < 0) {
		if (no > 0 && no < i
		&& !strpathcmp(fdtype[no - 1].name, fdtype[no].name)) no = i;
	}
	else if (no < min) no = min;
	else if (no > max + 1) no = max + 1;

	max = i;
	n = 1;
# ifdef	HDDMOUNT
	sp = NULL;
	if (!cyl) {
		int j;

		if (!(sp = readpt(dev, sect))) return(-1);
		for (n = 0; sp[n + 1]; n++);
		if (!n) {
			free(sp);
			return(-1);
		}
		else if (n >= MAXDRIVEENTRY - no) {
			free(sp);
			return(-2);
		}
		sect = sp[0];

		drvlist = malloc2(n);
		drvlist[0] = drive;
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
	for (i = max; i > no; i--) {
		memcpy((char *)&(fdtype[i + n - 1]),
			(char *)&(fdtype[i - 1]), sizeof(devinfo));
	}

# ifdef	HDDMOUNT
	if (!cyl) {
		for (i = 0; i < n; i++) {
			fdtype[no + i].drive = drvlist[i];
			fdtype[no + i].name = strdup2(dev);
			fdtype[no + i].head = head;
			fdtype[no + i].sect = sect;
			fdtype[no + i].cyl = cyl;
			fdtype[no + i].offset = sp[i + 1];
		}
		free(sp);
		free(drvlist);
		return(no);
	}
	fdtype[no].offset = 0;
# endif	/* HDDMOUNT */
	fdtype[no].drive = drive;
	fdtype[no].name = strdup2(dev);
	fdtype[no].head = head;
	fdtype[no].sect = sect;
	fdtype[no].cyl = cyl;
	return(no);
}

static int NEAR _setdrive(argc, argv, set)
int argc;
char *argv[];
int set;
{
	int i, n, drive, head, sect, cyl;

	if (argc <= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
# if	FD >= 2
	if (argc >= 7) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
# endif
	if (!isalpha(drive = toupper2(argv[1][0]))) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}

# ifdef	HDDMOUNT
	if (!strncmp(argv[3], "HDD", 3)) {
		if (argc > 5) {
			builtinerror(argv, NULL, ER_FEWMANYARG);
			return(-1);
		}
		cyl = 0;
		if (!argv[3][3]) sect = 0;
		else if (!strcmp(&(argv[3][3]), "98")) sect = 98;
		else {
			builtinerror(argv, argv[3], ER_SYNTAXERR);
			return(-1);
		}
		if (argc < 5) head = 'n';
		else if (!strcmp(argv[4], "ro")) head = 'r';
		else if (!strcmp(argv[4], "rw")) head = 'w';
		else {
			builtinerror(argv, argv[4], ER_SYNTAXERR);
			return(-1);
		}
		if (sect) head = toupper2(head);
	}
	else
# endif	/* HDDMOUNT */
# if	FD >= 2
	if (argc != 6) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	else if ((head = atoi2(argv[3])) <= 0) {
		builtinerror(argv, argv[3], ER_SYNTAXERR);
		return(-1);
	}
	else if ((sect = atoi2(argv[4])) <= 0) {
		builtinerror(argv, argv[4], ER_SYNTAXERR);
		return(-1);
	}
	else if ((cyl = atoi2(argv[5])) <= 0) {
		builtinerror(argv, argv[5], ER_SYNTAXERR);
		return(-1);
	}
# else	/* FD < 2 */
	{
		char *cp, *tmp;
		long l;

		cp = tmp = catvar(&(argv[3]), '\0');

		if (!(cp = evalnumeric(cp, &l, 1)) || *cp != DRIVESEP) {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(tmp);
			return(-1);
		}
		head = l;
		if (!(cp = evalnumeric(++cp, &l, 1)) || *cp != DRIVESEP) {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(tmp);
			return(-1);
		}
		sect = l;
		if (!(cp = evalnumeric(++cp, &l, 1)) || *cp) {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(tmp);
			return(-1);
		}
		cyl = l;
		free(tmp);
	}
# endif	/* FD < 2 */

# ifdef	HDDMOUNT
	if (!cyl) for (i = 0; fdtype[i].name; i++) {
		if (drive == fdtype[i].drive) break;
	}
	else
# endif
	i = searchdrv(fdtype, drive, argv[2], head, sect, cyl, set);

	if (!set) {
		if (!fdtype[i].name) {
			builtinerror(argv, argv[2], ER_NOENTRY);
			return(-1);
		}
# ifdef	HDDMOUNT
		if (!cyl
		&& (fdtype[i].cyl || strpathcmp(argv[2], fdtype[i].name))) {
			builtinerror(argv, argv[2], ER_NOENTRY);
			return(-1);
		}
# endif
		deletedrv(i);
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

		n = insertdrv(i, drive, argv[2], head, sect, cyl);
		if (n < -1) {
			builtinerror(argv, NULL, ER_OUTOFLIMIT);
			return(-1);
		}
		if (!inruncom && n < 0) {
			builtinerror(argv, argv[2], ER_INVALDEV);
			return(-1);
		}
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
VOID printsetdrv(n, verbose, fp)
int n, verbose;
FILE *fp;
{
	fputs(BL_SDRIVE, fp);
	fputc(' ', fp);
	fputc(fdtype[n].drive, fp);
	fputc('\t', fp);

	fputsmeta(fdtype[n].name, fp);
	fputc('\t', fp);
# ifdef	HDDMOUNT
	if (!fdtype[n].cyl) {
		char buf[MAXCOLSCOMMA(3) + 1];

		fputs("HDD", fp);
		if (fdtype[n].head >= 'A' && fdtype[n].head <= 'Z')
			fputs("98", fp);
		if (!verbose) return;
		ascnumeric(buf, fdtype[n].offset / fdtype[n].sect,
			-3, MAXCOLSCOMMA(3));
		fputs(" #offset=", fp);
		fputs(buf, fp);
		return;
	}
# endif	/* HDDMOUNT */
	fprintf2(fp, "%d%c%d%c%d", fdtype[n].head, DRIVESEP,
		fdtype[n].sect, DRIVESEP, fdtype[n].cyl);
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
		printsetdrv(i, 1, stdout);
		fputc('\n', stdout);
		hitkey(0);
	}
	else {
		for (i = n = 0; fdtype[i].name; i++)
		if (toupper2(argv[1][0]) == fdtype[i].drive) {
			n++;
			printsetdrv(i, 1, stdout);
			fputc('\n', stdout);
			hitkey(0);
		}
		if (!n) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
	}
	fflush(stdout);
	return(0);
}
#endif	/* !MSDOS && !_NODOSDRIVE */

#if	!MSDOS && !defined (_NOKEYMAP)
VOID printkeymap(key, s, len, fp)
int key;
char *s;
int len;
FILE *fp;
{
	char *cp;

	fputs(BL_KEYMAP, fp);
	fputc(' ', fp);
	fputsmeta(getkeysym(key, 1), fp);
	fputc('\t', fp);

	cp = encodestr(s, len);
	fputsmeta(cp, fp);
	free(cp);
}

static int NEAR setkeymap(argc, argv)
int argc;
char *argv[];
{
	char *cp;
	int i, ch, l;

	if (argc >= 4) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	if (argc <= 1) {
		hitkey(2);
		for (i = 1; i <= 20; i++) {
			ch = K_F(i);
			if (!(cp = getkeyseq(ch, &l)) || !l) continue;
			printkeymap(ch, cp, l, stdout);
			fputc('\n', stdout);
			hitkey(0);
		}
		for (i = 0; (ch = keyidentlist[i].no) > 0; i++) {
			if (!(cp = getkeyseq(ch, &l)) || !l) continue;
			printkeymap(ch, cp, l, stdout);
			fputc('\n', stdout);
			hitkey(0);
		}
		fflush(stdout);
		return(0);
	}

	if ((ch = getkeycode(argv[1], 1)) < 0) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	if (argc == 2) {
		if (!(cp = getkeyseq(ch, &l)) || !l) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		hitkey(2);
		printkeymap(ch, cp, l, stdout);
		fputc('\n', stdout);
		fflush(stdout);
		return(0);
	}
	if (!argv[2][0]) {
		setkeyseq(ch, NULL, 0);
		return(0);
	}

	cp = decodestr(argv[2], &i, 1);
	if (!i) {
		builtinerror(argv, argv[2], ER_SYNTAXERR);
		free(cp);
		return(-1);
	}
	setkeyseq(ch, cp, i);

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
		putterm(l_clear);
		buf[1] = '\0';
		do {
			ch = getch2();
			next = kbhit2(WAITKEYPAD * 1000L);
			buf[0] = ch;
			cp = encodestr(buf, 1);
			cputs2(cp);
			free(cp);
		} while (next);
		cputs2("\"\r\n");
		if (n) {
			if (++i >= n) break;
		}
		if (n != 1 && ch == ' ') break;
	}
	stdiomode();
	return(0);
}
#endif	/* !MSDOS && !_NOKEYMAP */

static int NEAR printhist(argc, argv)
int argc;
char *argv[];
{
	char *cp;
	long no;
	int i, n, max, size;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	size = histsize[0];
	if (argc < 2) no = size;
	else if (!(cp = evalnumeric(argv[1], &no, 1)) || *cp) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	else if (no > size) no = size;

	if (!history[0]) return(0);
	if (--no > (long)MAXHISTNO) no = MAXHISTNO;
	for (max = no; max >= 0; max--) if (history[0][max]) break;
	if (max < histno[0]) n = histno[0] - max - 1;
	else n = MAXHISTNO - (max - histno[0]);

	hitkey(2);
	for (i = max; i >= 0; i--) {
		fprintf2(stdout, "%5d  ", n + 1);
		if (n++ >= MAXHISTNO) n = 0;
		kanjifputs(history[0][i], stdout);
		fputc('\n', stdout);
		hitkey(0);
	}
	fflush(stdout);
	return(0);
}

#ifndef	NOPOSIXUTIL
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
			switch(argv[n][i]) {
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
		kanjifputs(argv[0], stderr);
		fputs(": usage: ", stderr);
		kanjifputs(argv[0], stderr);
		fputs(" [-ls] [-nr] [-e editor] [old=new] [first] [last]\n",
			stderr);
		fflush(stderr);
		return(-1);
	}

	if (!history[0]) return(0);
	tmp = removehist(0);
	if (exe) {
		char *s, *r;
		int j, l1, l2, len, max;

		i = n;
		for (; n < argc; n++) if (!strchr(argv[n], '=')) break;
		if ((f = parsehist((n < argc) ? argv[n] : "!", NULL)) < 0) {
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
			for (cp = s; (cp = strchr(cp, argv[i][0])); cp++)
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
		fputc('\n', stdout);
		fflush(stdout);
		entryhist(0, s, 0);
		if ((ret = execmacro(s, NULL, 1, -1, 1)) < 0) ret = 0;
		free(s);
		return(ret);
	}

	if (!editor) editor = getenv2("FD_FCEDIT");
	if (!editor) editor = getenv2("FD_EDITOR");
	if (!editor) editor = EDITOR;

	if (list) {
		f = parsehist((n < argc) ? argv[n] : "-16", NULL);
		l = parsehist((n + 1 < argc) ? argv[n + 1] : "!", NULL);
	}
	else {
		f = parsehist((n < argc) ? argv[n] : "!", NULL);
		l = (n + 1 < argc) ? parsehist(argv[n + 1], NULL) : f;
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

	if (f < histno[0]) n = histno[0] - f - 1;
	else n = MAXHISTNO - (f - histno[0]);

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
			Xclose(fd);
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
				if (n++ >= MAXHISTNO) n = 0;
			}
			kanjifputs(history[0][i], fp);
			fputc('\n', fp);
			if (list) hitkey(0);
		}
	}
	else for (i = f; i <= l; i++) {
		if (history[0][i]) {
			if (!nonum) {
				fprintf2(fp, "%5d  ", n + 1);
				if (--n < 0) n = MAXHISTNO;
			}
			kanjifputs(history[0][i], fp);
			fputc('\n', fp);
			if (list) hitkey(0);
		}
	}

	if (list) {
		fflush(stdout);
		entryhist(0, tmp, 0);
		free(tmp);
		return(0);
	}

	Xfclose(fp);
	ret = execmacro(editor, path, 1, 0, 1);
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
		fputc('\n', stdout);
		fflush(stdout);
		entryhist(0, cp, 0);
		ret = execmacro(cp, NULL, 1, -1, 1);
		free(cp);
	}
	Xfclose(fp);
	rmtmpfile(path);
	return(ret);
}
#endif	/* !NOPOSIXUTIL */

#if	FD >= 2
/*ARGSUSED*/
static VOID NEAR voidmd5(a, b, c, d)
u_long a, b, c, d;
{
	/* For bugs on NEWS-OS optimizer */
}

static VOID NEAR calcmd5(sum, x)
u_long sum[4], x[16];
{
	u_long a, b, c, d, tmp, t[16];
	int i, s[4];

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
	for (i = 0; i < 16; i++) {
		a += ((b & c) | (~b & d)) + x[i] + t[i];
		tmp = b + ((a << s[i % 4]) | a >> (32 - s[i % 4]));
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
	for (i = 0; i < 16; i++) {
		a += ((b & d) | (c & ~d)) + x[(i * 5 + 1) % 16] + t[i];
		tmp = b + ((a << s[i % 4]) | a >> (32 - s[i % 4]));
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
	for (i = 0; i < 16; i++) {
		a += (b ^ c ^ d) + x[(i * 3 + 5) % 16] + t[i];
		tmp = b + ((a << s[i % 4]) | a >> (32 - s[i % 4]));
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
	for (i = 0; i < 16; i++) {
		a += (c ^ (b | ~d)) + x[(i * 7) % 16] + t[i];
		tmp = b + ((a << s[i % 4]) | a >> (32 - s[i % 4]));
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
	u_long cl, ch, sum[4], x[16];
	int i, c, b, n;

	if (!(fpin = Xfopen(path, "rb"))) return(-1);

	cl = ch = (u_long)0;
	sum[0] = (u_long)0x67452301;
	sum[1] = (u_long)0xefcdab89;
	sum[2] = (u_long)0x98badcfe;
	sum[3] = (u_long)0x10325476;

	n = b = 0;
	for (i = 0; i < 16; i++) x[i] = (u_long)0;
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
			if (++n >= 16) {
				calcmd5(sum, x);
				n = 0;
				for (i = 0; i < 16; i++) x[i] = (u_long)0;
			}
		}
	}
	Xfclose(fpin);

	x[n] |= 1 << (b + BITSPERBYTE - 1);
	if (n >= 14) {
		calcmd5(sum, x);
		for (i = 0; i < 16; i++) x[i] = (u_long)0;
	}
	x[14] = cl;
	x[15] = ch;
	calcmd5(sum, x);

	fputs("MD5 (", fp);
	fputs(path, fp);
	fputs(") = ", fp);
	for (i = 0; i < 4; i++) {
		for (n = 0; n < 4; n++) {
			c = (sum[i] & 0xf0) >> 4;
			fputc((c >= 10) ? c + 'a' - 10 : c + '0', fp);
			c = (sum[i] & 0xf);
			fputc((c >= 10) ? c + 'a' - 10 : c + '0', fp);
			sum[i] >>= 8;
		}
	}
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
# if	MSDOS
		strcpy(strcatdelim2(path, progpath, progname), ".EXE");
# else
		strcatdelim2(path, progpath, progname);
# endif
		if (printmd5(path, stdout) < 0) {
			builtinerror(argv, path, -1);
			return(-1);
		}
		fputc('\n', stdout);
		fflush(stdout);
		return(0);
	}

	ret = 0;
	for (i = 1; i < argc; i++) {
		if (printmd5(argv[i], stdout) < 0) {
			builtinerror(argv, argv[i], -1);
			ret = -1;
			continue;
		}
		fputc('\n', stdout);
		hitkey(0);
	}
	fflush(stdout);
	return(ret);
}

static int NEAR evalmacro(argc, argv)
int argc;
char *argv[];
{
	char *cp;
	int ret;

	if (argc <= 1 || !(cp = catvar(&(argv[1]), ' '))) return(0);
	if ((ret = execmacro(cp, NULL, 1, -1, 0)) < 0) ret = 0;
	free(cp);
	return(ret);
}

# if	!MSDOS && !defined (_NOKANJICONV)
static int NEAR kconv(argc, argv)
int argc;
char *argv[];
{
	FILE *fpin, *fpout;
	char *cp, *src, *dest;
	int i, in, out, len, max, err;

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
			fputs("Usage: ", stderr);
			fputs(argv[0], stderr);
			fputs("[-i inputcode] [-o outputcode] [filename]\n",
				stderr);
			fflush(stderr);
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

	if (in == UTF8 || out == UTF8) readunitable();
	while ((src = fgets2(fpin, 0))) {
		if (in != DEFCODE) {
			len = strlen(src);
			max = len * 3 + 3;
			cp = malloc2(max + 1);
			len = kanjiconv(cp, src, max, in, DEFCODE, L_OUTPUT);
			free(src);
			cp[len] = '\0';
			src = cp;
		}
		len = strlen(src);
		max = len * 3 + 3;
		dest = malloc2(max + 1);
		len = kanjiconv(dest, src, max, DEFCODE, out, L_OUTPUT);
		free(src);
		dest[len] = '\0';
		fputs(dest, fpout);
		fputc('\n', fpout);
		free(dest);
	}
	fflush(fpout);

	discardunitable();
	if (fpin != stdin) Xfclose(fpin);
	else clearerr(fpin);
	if (fpout != stdout) Xfclose(fpout);
	return(0);
}
# endif	/* !MSDOS && !_NOKANJICONV */

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
	promptstr = (argc >= 2) ? argv[1] : "";
	s = inputstr(NULL, 0, -1, NULL, -1);
	promptstr = duppromptstr;
	if (!wastty) stdiomode();
	if (!s) return(-1);

	kanjifputs(s, stdout);
	fputc('\n', stdout);
	fflush(stdout);
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
	if (argc < 2) s = "";
	else s = argv[1];

	if (!(wastty = isttyiomode)) ttyiomode(1);
	lcmdline = -1;
	ret = yesno(s);
	if (!wastty) stdiomode();
	return((ret) ? 0 : -1);
}
#endif	/* FD >= 2 */

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
			if (!strnpathcmp(environ2[i], argv[1], len)
			&& environ2[i][len] == '=') break;
		if (!environ2[i]) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		kanjifputs(environ2[i], stdout);
		fputc('\n', stdout);
		fflush(stdout);
		return(0);
	}
# endif

	hitkey(2);
	if (environ2) {
		for (i = 0; environ2[i]; i++) {
			kanjifputs(environ2[i], stdout);
			fputc('\n', stdout);
			hitkey(0);
		}
	}
# if	FD >= 2
	for (i = 0; i < maxuserfunc; i++) {
		printfunction(i, 0, stdout);
		fputc('\n', stdout);
		hitkey(0);
	}
# endif
	fflush(stdout);
	return(0);
}

VOID printalias(n, fp)
int n;
FILE *fp;
{
	fputs(BL_ALIAS, fp);
	fputc(' ', fp);
	kanjifputs(aliaslist[n].alias, fp);
	fputc(ALIASSEP, fp);
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
			fputc('\n', stdout);
			hitkey(0);
		}
		fflush(stdout);
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

	for (i = 0; i < maxalias; i++)
		if (!strnpathcmp(argv[1], aliaslist[i].alias, len)) break;

# if	FD >= 2
	if (*cp) *cp = '\0';
# else
	if (argc > 2) /*EMPTY*/;
# endif
	else {
		if (i >= maxalias) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}
		hitkey(2);
		printalias(i, stdout);
		fputc('\n', stdout);
		fflush(stdout);
		return(0);
	}

	if (i >= MAXALIASTABLE) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		return(-1);
	}
	if (i >= maxalias) maxalias++;
	else {
		free(aliaslist[i].comm);
		free(aliaslist[i].alias);
	}
	aliaslist[i].alias = strdup2(argv[1]);
	aliaslist[i].comm = strdup2(argv[2]);
# if	MSDOS
	for (cp = aliaslist[i].alias; cp && *cp; cp++)
		if (*cp >= 'A' && *cp <= 'Z') *cp += 'a' - 'A';
# endif
	return(0);
}

/*ARGSUSED*/
static int NEAR unalias(argc, argv)
int argc;
char *argv[];
{
	reg_t *re;
	int i, j, n;

	if (argc != 2) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	n = 0;
	re = regexp_init(argv[1], -1);
	for (i = 0; i < maxalias; i++)
		if (regexp_exec(re, aliaslist[i].alias, 0)) {
			free(aliaslist[i].alias);
			free(aliaslist[i].comm);
			maxalias--;
			for (j = i; j < maxalias; j++)
				memcpy((char *)&(aliaslist[j]),
					(char *)&(aliaslist[j + 1]),
					sizeof(aliastable));
			i--;
			n++;
		}
	regexp_free(re);
	if (!n) {
		builtinerror(argv, argv[1], ER_NOENTRY);
		return(-1);
	}
	return(0);
}

VOID printfunction(no, verbose, fp)
int no, verbose;
FILE *fp;
{
	int i;

# if	FD < 2
	fputs(BL_FUNCTION, fp);
	fputc(' ', fp);
# endif
	kanjifputs(userfunclist[no].func, fp);
	fputs("() {", fp);
	if (verbose) fputc('\n', fp);
	for (i = 0; userfunclist[no].comm[i]; i++) {
		fputc((verbose) ? '\t' : ' ', fp);
		kanjifputs(userfunclist[no].comm[i], fp);
		fputc(';', fp);
		if (verbose) fputc('\n', fp);
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
		for (i = 1; i < n; i++) free(argv[i]);
		for (i = 1; n + i < argc; i++) argv[i] = argv[n + i];
		argc = i;
		argv[i] = NULL;
	}

	return(argc);
}

static int NEAR setuserfunc(argc, argv)
int argc;
char *argv[];
{
	char *cp, *tmp, *line, *list[MAXFUNCLINES + 1];
	int i, j, n;

	if (argc <= 1) {
# if	FD >= 2
		builtinerror(argv, NULL, ER_SYNTAXERR);
		return(-1);
# else	/* FD < 2 */
		hitkey(2);
		for (i = 0; i < maxuserfunc; i++) {
			printfunction(i, 0, stdout);
			fputc('\n', stdout);
			hitkey(0);
		}
		fflush(stdout);
		return(0);
# endif	/* FD < 2 */
	}

# if	FD < 2
	if ((n = checkuserfunc(argc - 1, &(argv[FUNCNAME]))) < 0) {
		builtinerror(argv, argv[FUNCNAME], ER_SYNTAXERR);
		return(-1);
	}
# endif

	for (i = 0; i < maxuserfunc; i++)
		if (!strpathcmp(argv[FUNCNAME], userfunclist[i].func)) break;

# if	FD < 2
	if (!n) {
		if (i >= maxuserfunc) {
			builtinerror(argv, argv[FUNCNAME], ER_NOENTRY);
			return(-1);
		}
		hitkey(2);
		printfunction(i, 1, stdout);
		fputc('\n', stdout);
		fflush(stdout);
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
	}
	else if (!n) {
		free(line);
		if (i >= maxuserfunc) {
			builtinerror(argv, argv[FUNCNAME], ER_NOENTRY);
			return(-1);
		}

		free(userfunclist[i].func);
		for (j = 0; userfunclist[i].comm[j]; j++)
			free(userfunclist[i].comm[j]);
		free(userfunclist[i].comm);
		maxuserfunc--;
		for (; i < maxuserfunc; i++)
			memcpy((char *)&(userfunclist[i]),
				(char *)&(userfunclist[i + 1]),
				sizeof(userfunctable));
		return(0);
	}
	free(line);

	if (i >= MAXFUNCTABLE) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		for (j = 0; j < n; j++) free(list[j]);
		return(-1);
	}

	if (i >= maxuserfunc) {
		userfunclist[i].func = strdup2(argv[FUNCNAME]);
# if	MSDOS
		for (cp = userfunclist[i].func; *cp; cp++)
			if (*cp >= 'A' && *cp <= 'Z') *cp += 'a' - 'A';
# endif
		maxuserfunc++;
	}
	else {
		for (j = 0; userfunclist[i].comm[j]; j++)
			free(userfunclist[i].comm[j]);
		free(userfunclist[i].comm);
	}
	userfunclist[i].comm = (char **)malloc2(sizeof(char *) * (n + 1));
	for (j = 0; j < n; j++) userfunclist[i].comm[j] = list[j];
	userfunclist[i].comm[j] = NULL;
	return(0);
}

static int NEAR exportenv(argc, argv)
int argc;
char *argv[];
{
	char *cp, *env;
	int i, len;

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
	if (!cp) env = argv[1];
	else {
		len = strlen(argv[1]);
		env = malloc2(len + strlen(cp) + 2);
		memcpy(env, argv[1], len);
		env[len++] = '=';
		strcpy(&(env[len]), cp);
		free(cp);
	}
	if (putenv2(env) < 0) error(argv[1]);
# if	!MSDOS
	adjustpath();
# endif
	evalenv();
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
	if (chdir3(argv[1]) < 0) {
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
		if (!strpathcmp(comname, builtinlist[i].ident)) return(i);
	return(-1);
}

int checkinternal(comname)
char *comname;
{
	int i;

	for (i = 0; i < FUNCLISTSIZ; i++)
		if (!strpathcmp(comname, funclist[i].ident)) return(i);
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
	if (fd_restricted && (funclist[n].status & RESTRICT)) {
		fputs(argv[0], stderr);
		fputs(": ", stderr);
		kanjifputs(RESTR_K, stderr);
		fputc('\n', stderr);
		fflush(stderr);
		return(RET_NOTICE);
	}
	if (argc > 2 || !filelist || maxfile <= 0) {
		fputs(argv[0], stderr);
		fputs(": ", stderr);
		kanjifputs(ILFNC_K, stderr);
		fputc('\n', stderr);
		fflush(stderr);
		return(RET_NOTICE);
	}
	ttyiomode(0);
	internal_status = (*funclist[n].func)(argv[1]);
	locate(0, n_line - 1);
	stdiomode();
	return(RET_SUCCESS);
}

#ifdef	_NOORIGSHELL
int execpseudoshell(command, comline, ignorelist)
char *command;
int comline, ignorelist;
{
	char *cp, **argv;
	int i, n, nl, argc;

	n = -1;
	command = skipspace(command);
	internal_status = -2;
	if (!(argc = getargs(command, &argv))) {
		freevar(argv);
		return(-1);
	}
	if ((i = checkbuiltin(argv[0])) >= 0) {
		if (comline) {
			locate(0, n_line - 1);
			tflush();
		}
		hitkey((comline) ? 1 : -1);
		nl = stdiomode();
		n = execbuiltin(i, argc, argv);
		ttyiomode(nl);
		if (n == RET_SUCCESS) hitkey(3);
		else if (comline) {
			hideclock = 1;
			warning(0, ILFNC_K);
		}
	}
	else if (!ignorelist && (i = checkinternal(argv[0])) >= 0) {
		if (comline) {
			locate(0, n_line - 1);
			tflush();
		}
		nl = stdiomode();
		n = execinternal(i, argc, argv);
		ttyiomode(nl);
		if (n == RET_SUCCESS) /*EMPTY*/;
		else if (comline) {
			hideclock = 1;
			warning(0, ILFNC_K);
		}
	}
# if	FD >= 2
	else if ((i = checkuserfunc(argc, argv)) != -1) {
		if (comline) {
			locate(0, n_line - 1);
			tflush();
		}
		nl = stdiomode();
		n = setuserfunc(i, argv);
		n = (n < 0) ? RET_NOTICE : RET_SUCCESS;
		ttyiomode(nl);
		if (n == RET_SUCCESS);
		else if (comline) {
			hideclock = 1;
			warning(0, ILFNC_K);
		}
	}
# endif
	else if (isidentchar(*command)) {
		i = argc;
		if ((cp = getenvval(&i, argv)) != (char *)-1 && i == argc) {
			if (setenv2(argv[0], cp) < 0) error(argv[0]);
			evalenv();
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
		if (strnpathcmp(com, builtinlist[i].ident, len)
		|| finddupl(builtinlist[i].ident, argc, *argvp)) continue;
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
		if (strnpathcmp(com, funclist[i].ident, len)
		|| finddupl(funclist[i].ident, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(funclist[i].ident);
	}
	return(argc);
}
#endif

#ifdef	DEBUG
VOID freedefine(VOID_A)
{
	int i;

# ifndef	_NOARCHIVE
	for (i = 0; i < maxlaunch; i++) {
		free(launchlist[i].ext);
		free(launchlist[i].comm);
# if	FD >= 2
		freevar(launchlist[i].format);
		freevar(launchlist[i].lignore);
		freevar(launchlist[i].lerror);
# endif
	}
	for (i = 0; i < maxarchive; i++) {
		free(archivelist[i].ext);
		if (archivelist[i].p_comm) free(archivelist[i].p_comm);
		if (archivelist[i].u_comm) free(archivelist[i].u_comm);
	}
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
# if	!MSDOS && !defined (_NODOSDRIVE)
	for (i = 0; fdtype[i].name; i++) free(fdtype[i].name);
# endif
	for (i = 0; i < 10; i++) free(helpindex[i]);
}
#endif
