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

#if	!MSDOS
# ifdef	_NODOSDRIVE
# include <sys/param.h>
# else
# include "dosdisk.h"
# endif
#endif

#ifndef	_NOARCHIVE
extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
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
#ifndef	DECLERRLIST
extern char *sys_errlist[];
#endif
extern int inruncom;

static VOID NEAR builtinerror __P_((char *[], char *, int));
#ifdef	_NOORIGSHELL
static VOID NEAR hitkey __P_((int));
#else
#define	hitkey(n)
#endif
#ifndef	_NOARCHIVE
static int NEAR setlaunch __P_((int, char *[]));
static int NEAR setarch __P_((int, char *[]));
static int NEAR printlaunch __P_((int, char *[]));
static int NEAR printarch __P_((int, char *[]));
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

#define	BINDCOMMENT	';'
#define	DRIVESEP	','
#define	ALIASSEP	'\t'
#define	FUNCNAME	1

#define	ER_FEWMANYARG	1
#define	ER_OUTOFLIMIT	2
#define	ER_NOENTRY	3
#define	ER_SYNTAXERR	4
#define	ER_EXIST	5
#define	ER_INVALDEV	6
static char *builtinerrstr[] = {
	"",
	"Too few or many arguments",
	"Out of limits",
	"No such entry",
	"Syntax error",
	"Entry already exists",
	"Invalid device",
};
#define	BUILTINERRSIZ	((int)(sizeof(builtinerrstr) / sizeof(char *)))

static builtintable builtinlist[] = {
#ifndef	_NOARCHIVE
	{setlaunch,	BL_LAUNCH},
	{setarch,	BL_ARCH},
	{printlaunch,	BL_PLAUNCH},
	{printarch,	BL_PARCH},
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
#ifdef	_NOORIGSHELL
	{printenv,	BL_PENV},
	{setalias,	BL_ALIAS},
	{unalias,	BL_UALIAS},
	{setuserfunc,	BL_FUNCTION},
	{exportenv,	BL_EXPORT},
	{dochdir,	BL_CHDIR},
	{dochdir,	BL_CD},
	{loadsource,	BL_SOURCE},
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
	if (n >= 0) fputs(builtinerrstr[n], stderr);
#if	MSDOS
	else fputs(strerror(duperrno), stderr);
#else
	else fputs((char *)sys_errlist[duperrno], stderr);
#endif
	fputc('\n', stderr);
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
		ttyiomode();
		win_x = 0;
		win_y = n_line - 1;
		hideclock = 1;
		warning(0, HITKY_K);
		stdiomode();
	}
}
#endif	/* _NOORIGSHELL */

#ifndef	_NOARCHIVE
static int NEAR setlaunch(argc, argv)
int argc;
char *argv[];
{
	char *ext;
	long n;
	int i, flags;
	char *cp, *tmp;
	int j, ch;

	if (argc <= 1) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}

	flags = 0;
	ext = getext(argv[1], &flags);

	for (i = 0; i < maxlaunch; i++)
		if (!extcmp(ext, flags,
		launchlist[i].ext, launchlist[i].flags, 1)) break;

	if (i >= MAXLAUNCHTABLE) {
		builtinerror(argv, NULL, ER_OUTOFLIMIT);
		free(ext);
		return(-1);
	}

	if (argc < 3) {
		free(ext);
		if (i >= maxlaunch) {
			builtinerror(argv, argv[1], ER_NOENTRY);
			return(-1);
		}

		free(launchlist[i].ext);
		free(launchlist[i].comm);
		maxlaunch--;
		for (; i < maxlaunch; i++)
			memcpy((char *)&(launchlist[i]),
				(char *)&(launchlist[i + 1]),
				sizeof(launchtable));
		return(0);
	}

	if (argc <= 3) launchlist[i].topskip = 255;
	else {
		cp = tmp = catvar(&(argv[3]), '\0');
		if (!(cp = evalnumeric(cp, &n, 0)) || *cp != ',') {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(ext);
			free(tmp);
			return(-1);
		}
		launchlist[i].topskip = n;
		if (!(cp = evalnumeric(++cp, &n, 0))) {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(ext);
			free(tmp);
			return(-1);
		}
		launchlist[i].bottomskip = n;

		ch = ':';
		for (j = 0; j < MAXLAUNCHFIELD; j++) {
			if (!cp || *cp != ch) {
				builtinerror(argv, tmp, ER_SYNTAXERR);
				free(ext);
				free(tmp);
				return(-1);
			}
			cp = getrange(++cp,
				&(launchlist[i].field[j]),
				&(launchlist[i].delim[j]),
				&(launchlist[i].width[j]));
			ch = ',';
		}

		ch = ':';
		for (j = 0; j < MAXLAUNCHSEP; j++) {
			if (*cp != ch) break;
			if (!(cp = evalnumeric(++cp, &n, 0))) {
				builtinerror(argv, tmp, ER_SYNTAXERR);
				free(ext);
				free(tmp);
				return(-1);
			}
			launchlist[i].sep[j] = (n > 0) ? n - 1 : 255;
			ch = ',';
		}
		for (; j < MAXLAUNCHSEP; j++) launchlist[i].sep[j] = -1;

		if (!cp || *cp != ':') launchlist[i].lines = 1;
		else {
			if (!(cp = evalnumeric(++cp, &n, 0))) {
				builtinerror(argv, tmp, ER_SYNTAXERR);
				free(ext);
				free(tmp);
				return(-1);
			}
			launchlist[i].lines = (n > 0) ? n : 1;
		}

		if (*cp) {
			builtinerror(argv, tmp, ER_SYNTAXERR);
			free(ext);
			free(tmp);
			return(-1);
		}
		free(tmp);
	}

	if (i >= maxlaunch) maxlaunch++;
	else {
		free(launchlist[i].ext);
		free(launchlist[i].comm);
	}
	launchlist[i].ext = ext;
	launchlist[i].comm = strdup2(argv[2]);
	flags = 0;
	launchlist[i].flags = flags;
	return(0);
}

static int NEAR setarch(argc, argv)
int argc;
char *argv[];
{
	char *ext;
	int i, flags;

	if (argc <= 1 || argc >= 5) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}

	flags = 0;
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
	flags = 0;
	archivelist[i].flags = flags;
	return(0);
}

/*ARGSUSED*/
VOID printlaunchcomm(n, omit, fp)
int n, omit;
FILE *fp;
{
	char buf[MAXNUMCOLS + 1];
	int i, ch;

	fputs(BL_LAUNCH, fp);
	fputc(' ', fp);
	if (launchlist[n].flags & LF_IGNORECASE) fputc('/', fp);
	fputs(launchlist[n].ext + 1, fp);
	fputc('\t', fp);

	fputc('"', fp);
	kanjifputs(launchlist[n].comm, fp);
	fputc('"', fp);
	if (launchlist[n].topskip >= 255) return;
	if (omit) {
		fputs("\t(Arch)", fp);
		return;
	}
	fputc('\t', fp);
	ascnumeric(buf, launchlist[n].topskip, 0, MAXNUMCOLS);
	fputs(buf, fp);
	fputc(',', fp);
	ascnumeric(buf, launchlist[n].bottomskip, 0, MAXNUMCOLS);
	fputs(buf, fp);
	ch = ':';
	for (i = 0; i < MAXLAUNCHFIELD; i++) {
		fputc(ch, fp);
		if (launchlist[n].field[i] >= 255) fputc('0', fp);
		else {
			ascnumeric(buf, launchlist[n].field[i] + 1,
				0, MAXNUMCOLS);
			fputs(buf, fp);
		}
		if (launchlist[n].delim[i] >= 128) {
			fputc('[', fp);
			ascnumeric(buf, launchlist[n].delim[i] - 128,
				0, MAXNUMCOLS);
			fputs(buf, fp);
			fputc(']', fp);
		}
		else if (launchlist[n].delim[i]) {
			fputc('\'', fp);
			fputc(launchlist[n].delim[i], fp);
			fputc('\'', fp);
		}
		if (launchlist[n].width[i]) {
			fputc('-', fp);
			if (launchlist[n].width[i] >= 128) {
				ascnumeric(buf, launchlist[n].width[i] - 128,
					0, MAXNUMCOLS);
				fputs(buf, fp);
			}
			else {
				fputc('\'', fp);
				fputc(launchlist[n].width[i], fp);
				fputc('\'', fp);
			}
		}
		ch = ',';
	}
	ch = ':';
	for (i = 0; i < MAXLAUNCHSEP; i++) {
		if (launchlist[n].sep[i] >= 255) break;
		fputc(ch, fp);
		ascnumeric(buf, launchlist[n].sep[i] + 1,
			0, MAXNUMCOLS);
		fputs(buf, fp);
		ch = ',';
	}
	if (launchlist[n].lines > 1) {
		if (!i) fputc(':', fp);
		ascnumeric(buf, launchlist[n].lines, 0, MAXNUMCOLS);
		fputs(buf, fp);
	}
}

static int NEAR printlaunch(argc, argv)
int argc;
char *argv[];
{
	char *ext;
	int i, flags;

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
		flags = 0;
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
	fputs(archivelist[n].ext + 1, fp);
	fputc('\t', fp);

	fputc('"', fp);
	if (archivelist[n].p_comm) kanjifputs(archivelist[n].p_comm, fp);
	fputc('"', fp);
	if (!archivelist[n].u_comm) return;
	fputs("\t\"", fp);
	kanjifputs(archivelist[n].u_comm, fp);
	fputc('"', fp);
}

static int NEAR printarch(argc, argv)
int argc;
char *argv[];
{
	char *ext;
	int i, flags;

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
		flags = 0;
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
#endif	/* !_NOARCHIVE */

static int NEAR getcommand(cp)
char *cp;
{
	int n;

	for (n = 0; n < FUNCLISTSIZ; n++)
		if (!strpathcmp(cp, funclist[n].ident)) break;
	if (n < FUNCLISTSIZ);
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
	fputs(getkeysym(bindlist[n].key, 0), fp);
	fputc('\t', fp);

	if (bindlist[n].f_func < FUNCLISTSIZ)
		fputs(funclist[bindlist[n].f_func].ident, fp);
	else {
		fputc('"', fp);
		kanjifputs(macrolist[bindlist[n].f_func - FUNCLISTSIZ], fp);
		fputc('"', fp);
	}
	if (bindlist[n].d_func < FUNCLISTSIZ) {
		fputc('\t', fp);
		fputs(funclist[bindlist[n].d_func].ident, fp);
	}
	else if (bindlist[n].d_func < 255) {
		fputs("\t\"", fp);
		kanjifputs(macrolist[bindlist[n].d_func - FUNCLISTSIZ], fp);
		fputc('"', fp);
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
	off64_t *sp;
	char *drvlist;
# endif
	int i, n, min, max;

	min = -1;
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
		if (j < n - 1) {
			free(sp);
			free(drvlist);
			return(-2);
		}
	}
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

# ifdef	HDDMOUNT
	if (!cyl) for (i = 0; fdtype[i].name; i++) {
		if (drive == fdtype[i].drive) break;
	}
	else
# endif
	for (i = 0; fdtype[i].name; i++) if (drive == fdtype[i].drive) {
# ifdef	HDDMOUNT
		if (!fdtype[i].cyl && set) break;
# endif
		if (head == fdtype[i].head
		&& sect == fdtype[i].sect
		&& cyl == fdtype[i].cyl
		&& !strpathcmp(argv[2], fdtype[i].name)) break;
	}

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
	char buf[MAXCOLSCOMMA(3) + 1];

	fputs(BL_SDRIVE, fp);
	fputc(' ', fp);
	fputc(fdtype[n].drive, fp);
	fputc('\t', fp);

	fputc('"', fp);
	kanjifputs(fdtype[n].name, fp);
	fputs("\"\t", fp);
# ifdef	HDDMOUNT
	if (!fdtype[n].cyl) {
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
	ascnumeric(buf, fdtype[n].head, 0, MAXCOLSCOMMA(3));
	fputs(buf, fp);
	fputc(DRIVESEP, fp);
	ascnumeric(buf, fdtype[n].sect, 0, MAXCOLSCOMMA(3));
	fputs(buf, fp);
	fputc(DRIVESEP, fp);
	ascnumeric(buf, fdtype[n].cyl, 0, MAXCOLSCOMMA(3));
	fputs(buf, fp);
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
	fputs(getkeysym(key, 1), fp);
	fputc('\t', fp);

	fputc('"', fp);
	cp = encodestr(s, len);
	fputs(cp, fp);
	free(cp);
	fputc('"', fp);
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
	ttyiomode();
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
	char buf[5 + 1];
	int i, no, max, size;

	if (argc >= 3) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}
	size = histsize[0];
	no = histno[0];
	if (argc < 2 || (max = atoi2(argv[1])) > size) max = size;
	if (max <= 0) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}

	hitkey(2);
	for (i = 1; i <= max; i++) {
		if (!history[0][max - i]) continue;
		ascnumeric(buf, no + i - max, 5, 5);
		fputs(buf, stdout);
		fputs("  ", stdout);
		kanjifputs(history[0][max - i], stdout);
		fputc('\n', stdout);
		hitkey(0);
	}
	fflush(stdout);
	return(0);
}

#ifdef	_NOORIGSHELL
static int NEAR printenv(argc, argv)
int argc;
char *argv[];
{
	int i, len;

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

	hitkey(2);
	if (environ2) {
		for (i = 0; environ2[i]; i++) {
			kanjifputs(environ2[i], stdout);
			fputc('\n', stdout);
			hitkey(0);
		}
	}
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
	fputc('"', fp);
	kanjifputs(aliaslist[n].comm, fp);
	fputc('"', fp);
}

static int NEAR setalias(argc, argv)
int argc;
char *argv[];
{
	char *cp;
	int i, len;

	if (argc >= 4) {
		builtinerror(argv, NULL, ER_FEWMANYARG);
		return(-1);
	}

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

	if (!(cp = gettoken(argv[1])) || *cp) {
		builtinerror(argv, argv[1], ER_SYNTAXERR);
		return(-1);
	}
	len = strlen(argv[1]);

	for (i = 0; i < maxalias; i++)
		if (!strnpathcmp(argv[1], aliaslist[i].alias, len)) break;

	if (argc > 2);
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

	fputs(BL_FUNCTION, fp);
	fputc(' ', fp);
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
		hitkey(2);
		for (i = 0; i < maxuserfunc; i++) {
			printfunction(i, 0, stdout);
			fputc('\n', stdout);
			hitkey(0);
		}
		fflush(stdout);
		return(0);
	}

	if ((n = checkuserfunc(argc - 1, &(argv[FUNCNAME]))) < 0) {
		builtinerror(argv, argv[FUNCNAME], ER_SYNTAXERR);
		return(-1);
	}

	for (i = 0; i < maxuserfunc; i++)
		if (!strpathcmp(argv[FUNCNAME], userfunclist[i].func)) break;

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
	ttyiomode();
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
	if (fd_restricted && (funclist[n].stat & RESTRICT)) {
		fputs(argv[0], stderr);
		fputs(": ", stderr);
		kanjifputs(RESTR_K, stderr);
		fputc('\n', stderr);
		return(RET_NOTICE);
	}
	if (argc > 2 || !filelist || maxfile <= 0) {
		fputs(argv[0], stderr);
		fputs(": ", stderr);
		kanjifputs(ILFNC_K, stderr);
		fputc('\n', stderr);
		return(RET_NOTICE);
	}
	ttyiomode();
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
	int i, n, argc;

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
		stdiomode();
		n = execbuiltin(i, argc, argv);
		ttyiomode();
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
		stdiomode();
		n = execinternal(i, argc, argv);
		ttyiomode();
		if (n == RET_SUCCESS);
		else if (comline) {
			hideclock = 1;
			warning(0, ILFNC_K);
		}
	}
	else if (isalpha(*command) || *command == '_') {
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
