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

#ifdef	USETIMEH
#include <time.h>
#endif

#if	MSDOS
#include "unixemu.h"
# ifdef	__GNUC__
extern char *adjustfname();
# endif
#else	/* !MSDOS */
#include <sys/time.h>
# ifdef	_NODOSDRIVE
# include <sys/param.h>
# else
# include "dosdisk.h"
# endif
#endif	/* !MSDOS */

#if defined (SIGARGINT) || defined (NOVOID)
#define	sigarg_t	int
#else
#define	sigarg_t	void
#endif

#ifndef	_NOARCHIVE
extern char *archivefile;
extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
#endif
#if	!MSDOS && !defined (_NODOSDRIVE)
extern devinfo fdtype[];
#endif
extern char fullpath[];
extern char **sh_history;
extern char *helpindex[];
extern int subwindow;
extern int columns;
extern int dispmode;
extern char *deftmpdir;

#define	CLOCKUPDATE	10	/* sec */

#if !MSDOS && !defined (SIGWINCH)
# if defined (SIGWINDOW)
# define	SIGWINCH	SIGWINDOW
# else
#  if defined (NSIG)
#  define	SIGWINCH	NSIG
#  else
#  define	SIGWINCH	28
#  endif
# endif
#endif	/* !MSDOS && !SIGWINCH */

#if !defined (SIGIOT) && defined (SIGABRT)
#define	SIGIOT	SIGABRT
#endif

static VOID signalexit();
#ifdef	SIGALRM
static sigarg_t ignore_alrm();
#endif
#ifdef	SIGWINCH
static sigarg_t ignore_winch();
#endif
#ifdef	SIGINT
static sigarg_t ignore_int();
#endif
#ifdef	SIGQUIT
static sigarg_t ignore_quit();
#endif
#ifdef	SIGHUP
static sigarg_t hangup();
#endif
#ifdef	SIGILL
static sigarg_t illerror();
#endif
#ifdef	SIGTRAP
static sigarg_t traperror();
#endif
#ifdef	SIGIOT
static sigarg_t ioerror();
#endif
#ifdef	SIGEMT
static sigarg_t emuerror();
#endif
#ifdef	SIGFPE
static sigarg_t floaterror();
#endif
#ifdef	SIGBUS
static sigarg_t buserror();
#endif
#ifdef	SIGSEGV
static sigarg_t segerror();
#endif
#ifdef	SIGSYS
static sigarg_t syserror();
#endif
#ifdef	SIGPIPE
static sigarg_t pipeerror();
#endif
#ifdef	SIGTERM
static sigarg_t terminate();
#endif
#ifdef	SIGXCPU
static sigarg_t xcpuerror();
#endif
#ifdef	SIGXFSZ
static sigarg_t xsizerror();
#endif
#ifdef	SIGWINCH
static sigarg_t wintr();
#endif
#ifdef	SIGALRM
static sigarg_t printtime();
#endif
static int getoption();
static VOID setexecname();
static VOID setexecpath();

char *origpath;
char *progpath = NULL;
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
#if	!MSDOS && !defined (_NODOSDRIVE)
	dosallclose();
#endif
	exit(127);
}

static VOID signalexit(sig)
int sig;
{
	signal(sig, SIG_IGN);
	inittty(1);
	forcecleandir(deftmpdir, tmpfilename);
#if	!MSDOS && !defined (_NODOSDRIVE)
	dosallclose();
#endif
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

#ifdef	SIGALRM
static sigarg_t ignore_alrm()
{
	signal(SIGALRM, (sigarg_t (*)())ignore_alrm);
}
#endif

#ifdef	SIGWINCH
static sigarg_t ignore_winch()
{
	signal(SIGWINCH, (sigarg_t (*)())ignore_winch);
}
#endif

#ifdef	SIGINT
static sigarg_t ignore_int()
{
	signal(SIGINT, (sigarg_t (*)())ignore_int);
}
#endif

#ifdef	SIGQUIT
static sigarg_t ignore_quit()
{
	signal(SIGQUIT, (sigarg_t (*)())ignore_quit);
}
#endif

#ifdef	SIGHUP
static sigarg_t hangup()
{
	signalexit(SIGHUP);
}
#endif

#ifdef	SIGILL
static sigarg_t illerror()
{
	signalexit(SIGILL);
}
#endif

#ifdef	SIGTRAP
static sigarg_t traperror()
{
	signalexit(SIGTRAP);
}
#endif

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

#ifdef	SIGFPE
static sigarg_t floaterror()
{
	signalexit(SIGFPE);
}
#endif

#ifdef	SIGBUS
static sigarg_t buserror()
{
	signalexit(SIGBUS);
}
#endif

#ifdef	SIGSEGV
static sigarg_t segerror()
{
	signalexit(SIGSEGV);
}
#endif

#ifdef	SIGSYS
static sigarg_t syserror()
{
	signalexit(SIGSYS);
}
#endif

#ifdef	SIGPIPE
static sigarg_t pipeerror()
{
	signalexit(SIGPIPE);
}
#endif

#ifdef	SIGTERM
static sigarg_t terminate()
{
	signalexit(SIGTERM);
}
#endif

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

#ifdef	SIGWINCH
static sigarg_t wintr()
{
	signal(SIGWINCH, SIG_IGN);
	getwsize(80, WHEADERMAX + WFOOTER + 2);
	title();
#ifndef	_NOARCHIVE
	if (archivefile) rewritearc(1);
	else
#endif
	rewritefile(1);
	if (subwindow) ungetch2(CTRL('L'));
	signal(SIGWINCH, (sigarg_t (*)())wintr);
}
#endif

#ifdef	SIGALRM
static sigarg_t printtime()
{
	static time_t now;
	struct tm *tm;
#if	!MSDOS
	struct timeval t;
	struct timezone tz;
#endif

	signal(SIGALRM, SIG_IGN);
	if (timersec) now++;
	else {
#if	MSDOS
		now = time(NULL);
#else
		gettimeofday(&t, &tz);
		now = t.tv_sec;
#endif
		timersec = CLOCKUPDATE;
	}
	if (showsecond || timersec == CLOCKUPDATE) {
		tm = localtime(&now);
		locate(n_column - 16 - ((showsecond) ? 3 : 0), LTITLE);
		putterm(t_standout);
		cprintf2("%02d-%02d-%02d %02d:%02d",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min);
		if (showsecond) cprintf2(":%02d", tm -> tm_sec);
		putterm(end_standout);
		locate(0, 0);
		tflush();
	}
	timersec--;
	signal(SIGALRM, (sigarg_t (*)())printtime);
}
#endif

VOID sigvecset()
{
	getwsize(80, WHEADERMAX + WFOOTER + 2);
#ifdef	SIGALRM
	signal(SIGALRM, (sigarg_t (*)())printtime);
#endif
#ifdef	SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
#ifdef	SIGWINCH
	signal(SIGWINCH, (sigarg_t (*)())wintr);
#endif
}

VOID sigvecreset()
{
#ifdef	SIGALRM
	signal(SIGALRM, (sigarg_t (*)())ignore_alrm);
#endif
#ifdef	SIGTSTP
	signal(SIGTSTP, SIG_DFL);
#endif
#ifdef	SIGWINCH
	signal(SIGWINCH, (sigarg_t (*)())ignore_winch);
#endif
}

VOID title()
{
	char *cp, *eol;

	locate(0, LTITLE);
	putterm(t_standout);
	cputs2("  FD(File & Directory tool) Ver.");
	cp = strchr(version, ' ');
	while (*(++cp) == ' ');
	if (!(eol = strchr(cp, ' '))) eol = cp + strlen(cp);
	cprintf2("%-*.*s", eol - cp, eol - cp, cp);
	cprintf2("%-*.*s", n_column - 32 - (eol - cp),
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
			execbuiltin(pack, NULL, NULL, 0);
			free(pack);
			pack = NULL;
		}
		else if (*(cp = skipspace(line)))
			execbuiltin(cp, NULL, NULL, 0);
	}

	if (pack) {
		execbuiltin(pack, NULL, NULL, 0);
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

static VOID setexecname(argv)
char *argv;
{
	char buf[MAXNAMLEN + 1];

	if (progname = strrchr(argv, _SC_)) progname++;
	else progname = argv;
#if	MSDOS
	{
		char *cp;

		if (cp = strchr(argv, '.')) *cp = '\0';
	}
#endif
	sprintf(buf, "%s%d", progname, getpid());
	tmpfilename = strdup2(buf);
}

static VOID setexecpath(argv)
char *argv;
{
	char *cp, buf[MAXPATHLEN];

	origpath = getwd2();
	strcpy(fullpath, origpath);

#if	MSDOS
	cp = argv;
#else
	if (strchr(argv, _SC_)) cp = argv;
	else {
		adjustpath();
		cp = NULL;
		completepath(argv, 0, &cp, 2, 1);
	}
	if (!cp) progpath = origpath;
	else
#endif
	{
		realpath2(cp, buf);
		if (cp != argv) free(cp);
		if (cp = strrchr(buf, _SC_)) *cp = '\0';
		progpath = strdup2(buf);
	}
}

int main(argc, argv)
int argc;
char *argv[];
{
	int i;

#if	MSDOS && defined (__GNUC__)
	adjustfname(argv[0]);
#endif
	setexecname(argv[0]);

	inittty(0);
	raw2();
	noecho2();
	nonl2();
	notabs();
	getterment();
	initterm();
	sigvecset();

#ifdef	SIGHUP
	signal(SIGHUP, (sigarg_t (*)())hangup);
#endif
#ifdef	SIGINT
	signal(SIGINT, (sigarg_t (*)())ignore_int);
#endif
#ifdef	SIGQUIT
	signal(SIGQUIT, (sigarg_t (*)())ignore_quit);
#endif
#ifdef	SIGILL
	signal(SIGILL, (sigarg_t (*)())illerror);
#endif
#ifdef	SIGTRAP
	signal(SIGTRAP, (sigarg_t (*)())traperror);
#endif
#ifdef	SIGIOT
	signal(SIGIOT, (sigarg_t (*)())ioerror);
#endif
#ifdef	SIGEMT
	signal(SIGEMT, (sigarg_t (*)())emuerror);
#endif
#ifdef	SIGFPE
	signal(SIGFPE, (sigarg_t (*)())floaterror);
#endif
#ifdef	SIGBUS
	signal(SIGBUS, (sigarg_t (*)())buserror);
#endif
#ifdef	SIGSEGV
	signal(SIGSEGV, (sigarg_t (*)())segerror);
#endif
#ifdef	SIGSYS
	signal(SIGSYS, (sigarg_t (*)())syserror);
#endif
#ifdef	SIGPIPE
	signal(SIGPIPE, (sigarg_t (*)())pipeerror);
#endif
#ifdef	SIGTERM
	signal(SIGTERM, (sigarg_t (*)())terminate);
#endif
#ifdef	SIGXCPU
	signal(SIGXCPU, (sigarg_t (*)())xcpuerror);
#endif
#ifdef	SIGXFSZ
	signal(SIGXFSZ, (sigarg_t (*)())xsizerror);
#endif

#ifndef	_NOARCHIVE
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
#endif	/* !_NOARCHIVE */

	for (i = 0; i < 10; i++) helpindex[i] = strdup2(helpindex[i]);
#if	!MSDOS && !defined (_NODOSDRIVE)
	for (i = 0; fdtype[i].name; i++)
		fdtype[i].name = strdup2(fdtype[i].name);
#endif

	setexecpath(argv[0]);
	loadruncom(DEFRUNCOM);
	loadruncom(RUNCOMFILE);
	i = getoption(argc, argv);
#if	!MSDOS
	adjustpath();
#endif
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
	if (_chdir2(origpath) < 0) error(origpath);
	free(origpath);
#if	!MSDOS && !defined (_NODOSDRIVE)
	dosallclose();
#endif

	exit2(0);
}
