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
#include "dosdisk.h"

#include <signal.h>
#include <sys/time.h>

#ifdef	USETIMEH
#include <time.h>
#endif

#if defined (SIGARGINT) || defined (NOVOID)
#define	sigarg_t	int
#else
#define	sigarg_t	void
#endif

extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
extern devinfo fdtype[];
extern char *archivefile;
extern char **sh_history;
extern char *helpindex[];
extern int subwindow;
extern int columns;
extern int dispmode;
extern char *deftmpdir;

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

#if !defined(SIGIOT) && defined(SIGABRT)
#define	SIGIOT	SIGABRT
#endif

static VOID signalexit();
static sigarg_t ignore_alrm();
static sigarg_t ignore_winch();
static sigarg_t ignore_int();
static sigarg_t ignore_quit();
static sigarg_t hungup();
static sigarg_t illerror();
static sigarg_t traperror();
#ifdef	SIGIOT
static sigarg_t ioerror();
#endif
#ifdef	SIGEMT
static sigarg_t emuerror();
#endif
static sigarg_t floaterror();
static sigarg_t buserror();
static sigarg_t segerror();
#ifdef	SIGSYS
static sigarg_t syserror();
#endif
static sigarg_t pipeerror();
static sigarg_t terminate();
#ifdef	SIGXCPU
static sigarg_t xcpuerror();
#endif
#ifdef	SIGXFSZ
static sigarg_t xsizerror();
#endif
static sigarg_t wintr();
static sigarg_t printtime();
static int getoption();

char *tmpfilename;
int showsecond;

static char *progname;
static int timersec = 0;


VOID error(str)
char *str;
{
	if (!str) str = progname;
	endterm();
	inittty(1);
	fputc('\007', stderr);
	perror(str);
	forcecleandir(deftmpdir, tmpfilename);
	dosallclose();
	exit(127);
}

static VOID signalexit(sig)
int sig;
{
	signal(sig, SIG_IGN);
	inittty(1);
	forcecleandir(deftmpdir, tmpfilename);
	dosallclose();
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

static sigarg_t ignore_alrm()
{
	signal(SIGALRM, (sigarg_t (*)())ignore_alrm);
}

static sigarg_t ignore_winch()
{
	signal(SIGWINCH, (sigarg_t (*)())ignore_winch);
}

static sigarg_t ignore_int()
{
	signal(SIGINT, (sigarg_t (*)())ignore_int);
}

static sigarg_t ignore_quit()
{
	signal(SIGQUIT, (sigarg_t (*)())ignore_quit);
}

static sigarg_t hangup()
{
	signalexit(SIGHUP);
}

static sigarg_t illerror()
{
	signalexit(SIGILL);
}

static sigarg_t traperror()
{
	signalexit(SIGTRAP);
}

#ifdef	SIGIOT
static sigarg_t ioerror()
{
	signalexit(SIGIOT);
}
#endif

#ifdef	SIGEMT
static sigarg_t emuerror()
{
	signalexit(SIGEMT);
}
#endif

static sigarg_t floaterror()
{
	signalexit(SIGFPE);
}

static sigarg_t buserror()
{
	signalexit(SIGBUS);
}

static sigarg_t segerror()
{
	signalexit(SIGSEGV);
}

#ifdef	SIGSYS
static sigarg_t syserror()
{
	signalexit(SIGSYS);
}
#endif

static sigarg_t pipeerror()
{
	signalexit(SIGPIPE);
}

static sigarg_t terminate()
{
	signalexit(SIGTERM);
}

#ifdef	SIGXCPU
static sigarg_t xcpuerror()
{
	signalexit(SIGXCPU);
}
#endif

#ifdef	SIGXFSZ
static sigarg_t xsizerror()
{
	signalexit(SIGXFSZ);
}
#endif

static sigarg_t wintr()
{
	signal(SIGWINCH, SIG_IGN);
	getwsize(80, WHEADERMAX + WFOOTER + 2);
	title();
	if (archivefile) rewritearc(1);
	else rewritefile(1);
	if (subwindow) ungetch2(CTRL('L'));
	signal(SIGWINCH, (sigarg_t (*)())wintr);
}

static sigarg_t printtime()
{
	static time_t now;
	struct timeval t;
	struct timezone tz;
	struct tm *tm;

	signal(SIGALRM, SIG_IGN);
	if (timersec) now++;
	else {
		gettimeofday(&t, &tz);
		now = t.tv_sec;
		timersec = CLOCKUPDATE;
	}
	if (showsecond || timersec == CLOCKUPDATE) {
		tm = localtime(&now);
		locate(n_column - 16 - ((showsecond) ? 3 : 0), LTITLE);
		putterm(t_standout);
		if (showsecond) cprintf("%02d-%02d-%02d %02d:%02d:%02d",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min, tm -> tm_sec);
		else cprintf("%02d-%02d-%02d %02d:%02d",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min);
		putterm(end_standout);
		locate(0, 0);
		tflush();
	}
	timersec--;
	signal(SIGALRM, (sigarg_t (*)())printtime);
}

VOID sigvecset()
{
	getwsize(80, WHEADERMAX + WFOOTER + 2);
	signal(SIGALRM, (sigarg_t (*)())printtime);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGWINCH, (sigarg_t (*)())wintr);
}

VOID sigvecreset()
{
	signal(SIGALRM, (sigarg_t (*)())ignore_alrm);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGWINCH, (sigarg_t (*)())ignore_winch);
}

VOID title()
{
	char *cp, *eol;

	locate(0, LTITLE);
	putterm(t_standout);
	cputs("  FD(File & Directory tool) Ver.");
	cp = strchr(version, ' ');
	while (*(++cp) == ' ');
	if (!(eol = strchr(cp, ' '))) eol = cp + strlen(cp);
	cprintf("%-*.*s", eol - cp, eol - cp, cp);
	cprintf("%-*.*s", n_column - 32 - (eol - cp),
		n_column - 32 - (eol - cp), " (c)1995,96 T.Shirai  ");
	putterm(end_standout);
	timersec = 0;
	printtime();
}

int loadruncom(file)
char *file;
{
	FILE *fp;
	char *cp, *pack, line[MAXLINESTR + 1];
	int cont;

	cp = evalpath(strdup2(file));
	fp = fopen(cp, "r");
	free(cp);
	if (!fp) return(-1);

	pack = NULL;
	while (fgets(line, MAXLINESTR, fp)) {
		if (*line == ';' || *line == '#') continue;
		if (!(cp = strchr(line, '\n'))) cp = line + strlen(line);
		for (cp--; cp >= line && (*cp == ' ' || *cp == '\t'); cp--);
		*(cp + 1) = '\0';

		cont = 0;
		if (cp >= line && *cp == '\\'
		&& (cp - 1 < line || *(cp - 1) != '\\')) {
			*cp = '\0';
			cont = 1;
		}

		if (*line) {
			if (pack) {
				pack = (char *)realloc2(pack,
					strlen(pack) + strlen(line) + 1);
				strcat(pack, line);
			}
			else if (cont) pack = strdup2(skipspace(line));
		}

		if (cont) continue;

		if (pack) {
			evalconfigline(pack);
			free(pack);
			pack = NULL;
		}
		else if (*(cp = skipspace(line))) evalconfigline(cp);
	}

	if (pack) {
		evalconfigline(pack);
		free(pack);
	}

	fclose(fp);
	return(0);
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
	char buf[MAXNAMLEN + 1];
	int i;

	if (progname = strrchr(argv[0], '/')) progname++;
	else progname = argv[0];
	sprintf(buf, "%s%d", progname, getpid());
	tmpfilename = strdup2(buf);

	inittty(0);
	raw2();
	noecho2();
	nonl2();
	notabs();
	getterment();
	initterm();
	sigvecset();

	signal(SIGHUP, (sigarg_t (*)())hangup);
	signal(SIGINT, (sigarg_t (*)())ignore_int);
	signal(SIGQUIT, (sigarg_t (*)())ignore_quit);
	signal(SIGILL, (sigarg_t (*)())illerror);
	signal(SIGTRAP, (sigarg_t (*)())traperror);
#ifdef	SIGIOT
	signal(SIGIOT, (sigarg_t (*)())ioerror);
#endif
#ifdef	SIGEMT
	signal(SIGEMT, (sigarg_t (*)())emuerror);
#endif
	signal(SIGFPE, (sigarg_t (*)())floaterror);
	signal(SIGBUS, (sigarg_t (*)())buserror);
	signal(SIGSEGV, (sigarg_t (*)())segerror);
#ifdef	SIGSYS
	signal(SIGSYS, (sigarg_t (*)())syserror);
#endif
	signal(SIGPIPE, (sigarg_t (*)())pipeerror);
	signal(SIGTERM, (sigarg_t (*)())terminate);
#ifdef	SIGXCPU
	signal(SIGXCPU, (sigarg_t (*)())xcpuerror);
#endif
#ifdef	SIGXFSZ
	signal(SIGXFSZ, (sigarg_t (*)())xsizerror);
#endif

	for (maxlaunch = 0; maxlaunch < MAXLAUNCHTABLE; maxlaunch++)
		if (!launchlist[maxlaunch].ext) break;
		else {
			launchlist[maxlaunch].ext =
				strdup2(launchlist[maxlaunch].ext);
			launchlist[maxlaunch].comm =
				strdup2(launchlist[maxlaunch].comm);
		}
	for (maxarchive = 0; maxarchive < MAXARCHIVETABLE; maxarchive++)
		if (!archivelist[maxarchive].ext) break;
		else {
			archivelist[maxarchive].ext =
				strdup2(archivelist[maxarchive].ext);
			archivelist[maxarchive].p_comm =
				strdup2(archivelist[maxarchive].p_comm);
			archivelist[maxarchive].u_comm =
				strdup2(archivelist[maxarchive].u_comm);
		}
	for (i = 0; i < 10; i++) helpindex[i] = strdup2(helpindex[i]);
	for (i = 0; fdtype[i].name; i++)
		fdtype[i].name = strdup2(fdtype[i].name);

	loadruncom(DEFRUNCOM);
	loadruncom(RUNCOMFILE);
	i = getoption(argc, argv);
	adjustpath();
	evalenv();
	if ((dispmode = atoi2(getenv2("FD_DISPLAYMODE"))) < 0)
#if (DISPLAYMODE < 0) || (DISPLAYMODE > 7)
		dispmode = 0;
#else
		dispmode = DISPLAYMODE;
#endif
	columns = atoi2(getenv2("FD_COLUMNS"));
	if (columns < 1 || columns == 4 || columns > 5)
#if (COLUMNS < 1) || (COLUMNS == 4) || (COLUMNS > 5)
		columns = 2;
#else
		columns = COLUMNS;
#endif
#if (ADJTTY == 0)
	if (evalbool(getenv2("FD_ADJTTY")) > 0) {
#else
	{
#endif
		cooked2();
		echo2();
		nl2();
		tabs();
		inittty(0);
		raw2();
		noecho2();
		nonl2();
		notabs();
	}

	sh_history = loadhistory(HISTORYFILE);
	putterms(t_clear);
	title();

	main_fd(argv[i]);
	dosallclose();

	exit2(0);
}
