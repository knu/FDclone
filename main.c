/*
 *	FD (File & Directory maintenance tool)
 *
 *	by T.Shirai <shirai@red.nintendo.co.jp>
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"
#include "funcno.h"
#include "version.h"

#include <signal.h>
#include <sys/time.h>

#ifdef	USETIMEH
#include <time.h>
#endif

extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
extern char *macrolist[];
extern int maxmacro;
extern bindtable bindlist[];
extern functable funclist[];
extern char *archivefile;
extern int subwindow;

#define	CLOCKUPDATE	10	/* sec */

#ifndef	SIGWINCH
# if defined (SIGWINDOW)
# define	SIGWINCH	SIGWINDOW
# else
#  if defined (NSIG)
#  define	SIGWINCH	NSIG
#  else
#  define	SIGWINCH	28
#  endif
# endif
#endif

static VOID wintr();
static VOID printtime();
static char *skipspace();
static char *geteostr();
static char *getrange();
static char *getenvval();
static VOID getlaunch();
static int getcommand();
static VOID getkeybind();
static VOID loadruncom();
static VOID printext();
static int getoption();

static char *progname;
static int timersec = 0;


VOID error(str)
char *str;
{
	if (!str) str = progname;
	endterm();
	inittty(1);
	perror(str);
	exit2(1);
}

VOID usage(no)
int no;
{
	exit2(no);
}

static VOID wintr()
{
	signal(SIGWINCH, SIG_IGN);
	getwsize(80, WHEADER + WFOOTER + 2);
	title();
	if (archivefile) rewritearc();
	else rewritefile();
	if (subwindow) ungetch2(CTRL_L);
	signal(SIGWINCH, wintr);
}

static VOID printtime()
{
	struct timeval t;
	struct timezone tz;
	struct tm *tm;

	signal(SIGALRM, SIG_IGN);
	if (!timersec) {
		gettimeofday(&t, &tz);
		tzset();
		tm = localtime(&(t.tv_sec));

		locate(n_column - 16, LTITLE);
		putterm(t_standout);
		cprintf("%02d-%02d-%02d %02d:%02d",
			tm -> tm_year, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min);
		putterm(end_standout);
		locate(0, 0);
		tflush();
		timersec = CLOCKUPDATE;
	}
	timersec--;
	signal(SIGALRM, printtime);
}

VOID sigvecset()
{
	getwsize(80, WHEADER + WFOOTER + 2);
	signal(SIGWINCH, wintr);
	signal(SIGALRM, printtime);
}

VOID sigvecreset()
{
	signal(SIGWINCH, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
}

VOID title()
{
	char *cp, *eol;

	locate(0, LTITLE);
	putterm(t_standout);
	cputs("  FD (File & Directory tool) Ver. ");
	cp = strchr(version, ' ');
	while (*(++cp) == ' ');
	if (!(eol = strchr(cp, ' '))) eol = cp + strlen(cp);
	cprintf("%-*.*s", eol - cp, eol - cp, cp);
	cprintf("%-*.*s", n_column - 34 - (eol - cp),
		n_column - 34 - (eol - cp), "  (c)1995 T.Shirai  ");
	putterm(end_standout);
	timersec = 0;
	printtime();
}

static char *skipspace(cp)
char *cp;
{
	while (*cp && (*cp == ' ' || *cp == '\t')) cp++;
	return(cp);
}

static char *geteostr(cp)
char **cp;
{
	char *tmp;

	if (**cp == '"') {
		for (tmp = ++(*cp); tmp = strchr(tmp, '"'); tmp++) {
			if (*(tmp - 1) != '\\') return(tmp);
		}
	}
	return(*cp + strlen(*cp));
}

static char *getrange(cp, fp, dp, wp)
char *cp, *fp, *dp;
u_char *wp;
{
	*fp = *wp = 0;
	*dp = '\0';

	if (*cp >= '0' && *cp <= '9') *fp = atoi(cp) - 1;
	while (*cp >= '0' && *cp <= '9') cp++;

	if (*cp == '\'') {
		*dp = *(++cp);
		if (*cp && *(++cp) == '\'') cp++;
	}

	if (*cp == '-') {
		cp++;
		if (*cp >= '0' && *cp <= '9') *wp = atoi(cp) + 128;
		else if (*cp == '\'') *wp = *(++cp);
	}
	return(cp);
}

static char *getenvval(str)
char *str;
{
	char *cp;
	int ch;

	if (cp = strpbrk(str, "= \t")) {
		ch = *cp;
		*cp = '\0';
		if (ch == '=' || (cp = strchr(++cp, '='))) cp++;
	}
	str = cp;
	if (str) {
		str = skipspace(str);
		cp = geteostr(&str);
		if (cp > str) *cp = '\0';
		else str = NULL;
	}
	if (str && *str == '$') str = getenv2(++str);
	return(str);
}

static VOID getlaunch(line)
char *line;
{
	char *cp, *tmp, *ext, *eol;
	int i, ch, n;

	cp = line;
	eol = line + strlen(line);

	tmp = geteostr(&cp);
	*tmp = '\0';
	if (tmp >= eol) return;
	if (*cp == '*') ext = cnvregexp(cp);
	else {
		cp = cnvregexp(cp);
		ext = (char *)malloc2(strlen(cp) + 3);
		strcpy(ext + 2, cp);
		memcpy(ext, "^.*", 3);
		free(cp);
	}

	tmp = skipspace(++tmp);
	if (*tmp == 'A') {
		for (n = 0; n < maxarchive; n++)
			if (!strcmp(archivelist[n].ext, ext)) break;
		if (n == maxarchive) {
			if (maxarchive >= MAXARCHIVETABLE) return;
			else maxarchive++;
		}

		archivelist[n].ext = ext;
		cp = skipspace(++tmp);
		tmp = geteostr(&cp);
		*tmp = '\0';
		archivelist[n].p_comm = strdup2(cp);

		if (tmp >= eol || (cp = skipspace(++tmp)) >= eol)
			archivelist[n].p_comm = NULL;
		else {
			tmp = geteostr(&cp);
			*tmp = '\0';
			archivelist[n].u_comm = strdup2(cp);
		}
	}
	else {
		for (n = 0; n < maxlaunch; n++)
			if (!strcmp(launchlist[n].ext, ext)) break;
		if (n == maxlaunch) {
			if (maxlaunch >= MAXLAUNCHTABLE) return;
			else maxlaunch++;
		}

		launchlist[n].ext = ext;
		cp = tmp;
		tmp = geteostr(&cp);
		*tmp = '\0';
		launchlist[n].comm = strdup2(cp);
		cp = skipspace(++tmp);
		if (!*cp) launchlist[n].topskip = -1;
		else {
			launchlist[n].topskip = atoi(cp);
			if (!(tmp = strchr(cp, ','))) {
				launchlist[n].topskip = -1;
				return;
			}
			cp = skipspace(++tmp);
			launchlist[n].bottomskip = atoi(cp);
			ch = ':';
			for (i = 0; i < MAXLAUNCHFIELD; i++) {
				if (!(tmp = strchr(cp, ch))) {
					launchlist[n].topskip = -1;
					return;
				}
				cp = skipspace(++tmp);
				cp = getrange(cp,
					&(launchlist[n].field[i]),
					&(launchlist[n].delim[i]),
					&(launchlist[n].width[i]));
				ch = ',';
			}
			ch = ':';
			for (i = 0; i < MAXLAUNCHSEP; i++) {
				if (!(tmp = strchr(cp, ch))) break;
				cp = skipspace(++tmp);
				launchlist[n].sep[i] = atoi(cp) - 1;
				ch = ',';
			}
			for (; i < MAXLAUNCHSEP; i++) launchlist[n].sep[i] = -1;
		}
	}
}

static int getcommand(cp)
char **cp;
{
	char *tmp;
	int n;

	if (**cp == '"') {
		tmp = geteostr(cp);
		*tmp = '\0';
		if (maxmacro >= MAXMACROTABLE) n = -1;
		else {
			macrolist[maxmacro] = strdup2(*cp);
			n = NO_OPERATION + ++maxmacro;
		}
	}
	else {
		if (!(tmp = strpbrk(*cp, " \t"))) tmp = *cp + strlen(*cp);
		*tmp = '\0';
		for (n = 0; n < NO_OPERATION; n++)
			if (!strcmp(*cp, funclist[n].ident)) break;
		if (n >= NO_OPERATION) n = -1;
	}
	*cp = tmp;
	return(n);
}

static VOID getkeybind(line)
char *line;
{
	char *cp, *eol;
	int i, ch, n1, n2;

	cp = line;
	eol = line + strlen(line);

	ch = *(cp++);
	if (ch == '^') ch = toupper2(*(cp++)) - '@';
	if (*cp == '\'') cp++;

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
		if (ch == bindlist[i].key) break;
	if (i >= MAXBINDTABLE - 1) return;
	if (bindlist[i].key < 0)
		memcpy(&bindlist[i + 1], &bindlist[i], sizeof(bindtable));

	cp = skipspace(cp);
	if ((n1 = getcommand(&cp)) < 0) return;
	if (cp < eol) {
		cp = skipspace(cp + 1);
		n2 = getcommand(&cp);
	}
	else n2 = -1;

	bindlist[i].key = ch;
	bindlist[i].f_func = n1;
	bindlist[i].d_func = n2;
}

int evalconfigline(line)
char *line;
{
	char *cp, *tmp;
	int len;

	if (!strncmp(line, "export", 6)) {
		tmp = skipspace(line + 6);
#ifdef	USESETENV
		if (!(cp = getenvval(tmp))) unsetenv(tmp);
		else if (setenv(tmp, cp, 1) < 0) error(cp);
#else
		cp = getenvval(tmp);
		len = strlen(tmp);
		tmp[len++] = '=';
		if (cp) strcpy(tmp + len, cp);
		cp = strdup2(tmp);
		if (putenv2(cp)) error(cp);
#endif
	}
	else if (isalpha(*line) || *line == '_') {
		if (setenv2(line, getenvval(line), 1) < 0) error(line);
	}
	else if (*line == '"') getlaunch(line);
	else if (*line == '\'') getkeybind(line + 1);
	else return(-1);
	return(0);
}

static VOID loadruncom(file)
char *file;
{
	FILE *fp;
	char *cp, line[MAXLINESTR + 1];

	for (maxlaunch = 0; maxlaunch < MAXLAUNCHTABLE; maxlaunch++)
		if (!launchlist[maxlaunch].ext) break;
	for (maxarchive = 0; maxarchive < MAXARCHIVETABLE; maxarchive++)
		if (!archivelist[maxarchive].ext) break;

	cp = evalpath(strdup2(file));
	fp = fopen(cp, "r");
	free(cp);
	if (!fp) return;
	while (fgets(line, MAXLINESTR, fp)) {
		if (cp = strchr(line, '\n')) *cp = '\0';
		if (*line && *line != ';' && *line != '#') evalconfigline(line);
	}
	fclose(fp);
}

int printmacro()
{
	int i, n;

	n = 0;
	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (bindlist[i].f_func <= NO_OPERATION
		&& bindlist[i].d_func <= NO_OPERATION) continue;
		if (bindlist[i].key < ' ')
			cprintf("'^%c'\t", bindlist[i].key + '@');
		else cprintf("'%c'\t", bindlist[i].key);
		if (bindlist[i].f_func <= NO_OPERATION)
			cputs(funclist[bindlist[i].f_func].ident);
		else kanjiprintf("\"%s\"",
			macrolist[bindlist[i].f_func - NO_OPERATION - 1]);
		if (bindlist[i].d_func < 0);
		else if (bindlist[i].d_func <= NO_OPERATION)
			cprintf("\t%s", funclist[bindlist[i].d_func].ident);
		else kanjiprintf("\t\"%s\"",
			macrolist[bindlist[i].d_func - NO_OPERATION - 1]);
		cputs("\r\n");
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

static VOID printext(ext)
char *ext;
{
	char *cp;

	for (cp = ext; *cp; cp++) {
		if (*cp == '.' && *(cp + 1) != '*') putch('?');
		else {
			if (strchr(".\\^$", *cp) && !*(++cp)) break;
			putch(*cp);
		}
	}
}

int printlaunch()
{
	char *cp;
	int i, n;

	n = 0;
	for (i = 0; i < maxlaunch; i++) {
		putch('"');
		printext(launchlist[i].ext);
		kanjiprintf("\"\t\"%s\"", launchlist[i].comm);
		if (launchlist[i].topskip >= 0) cputs(" (Arch)");
		cputs("\r\n");
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

int printarch()
{
	char *cp;
	int i, n;

	n = 0;
	for (i = 0; i < maxarchive; i++) {
		putch('"');
		printext(archivelist[i].ext);
		kanjiprintf("\"\tA \"%s\"", archivelist[i].p_comm);
		kanjiprintf("\t\"%s\"\r\n", archivelist[i].u_comm);
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

static int getoption(argc, argv)
int argc;
char *argv[];
{
	char *cp, *env;
	int i, ch;

	for (i = 1; i < argc; i++) {
		env = argv[i] + 1;
		if (*argv[i] != '-' || !isupper(*env)) return(i);

		if (cp = strpbrk(env, "= \t")) {
			ch = *cp;
			*cp = '\0';
			if (ch == '=' || (cp = strchr(++cp, '='))
			|| (++i < argc && (cp = strchr(argv[i], '=')))) cp++;
			else i--;
		}

		if (cp) {
			while (*cp && (*cp == ' ' || *cp == '\t')) cp++;
			if (setenv2(env, cp, 1) < 0) error(env);
		} else setenv2(env, NULL, 1);
	}
	return(i);
}

int main(argc, argv)
int argc;
char *argv[];
{
	int i;

	if (progname = strrchr(argv[0], '/')) progname++;
	else progname = argv[0];

	inittty(0);
	raw2();
	noecho2();
	nonl2();
	tabs();
	getterment();
	initterm();
	sigvecset();

	loadruncom(RUNCOMFILE);
	i = getoption(argc, argv);
#if (ADJTTY == 0)
	if (atoi2(getenv2("FD_ADJTTY")) > 0) {
#else
	{
#endif
		cooked2();
		echo2();
		nl2();
		inittty(0);
		raw2();
		noecho2();
		nonl2();
	}

	putterms(t_clear);
	title();

	main_fd(argv[i]);

	exit2(0);
}
