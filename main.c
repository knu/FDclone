/*
 *	FD (File & Directory maintenance tool)
 *
 *	by T.Shirai <shirai@red.nintendo.co.jp>
 */

#include <signal.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "funcno.h"
#include "kanji.h"
#include "version.h"

#if	MSDOS
#include <process.h>
#include <sys/timeb.h>
# ifdef	__GNUC__
extern char *adjustfname __P_((char *));
# endif
# ifdef	__TURBOC__
extern unsigned _stklen = 0x6000;
# endif
#else
# ifdef	_NODOSDRIVE
# include <sys/param.h>
# else
# include "dosdisk.h"
# endif
#endif	/* !MSDOS */

#if	defined (SIGARGINT) || defined (NOVOID)
#define	sigarg_t	int
#else
#define	sigarg_t	void
#endif

#ifdef	SIGFNCINT
#define	sigfnc_t	int
#else
# ifdef	NOVOID
# define	sigfnc_t
# else
# define	sigfnc_t	void
# endif
#endif

#ifndef	_NOARCHIVE
extern char *archivefile;
extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
#endif
#ifndef	_NODOSDRIVE
#if	!MSDOS
extern devinfo fdtype[];
#endif
extern char *unitblpath;
#endif
extern char fullpath[];
extern char **history[];
extern char *helpindex[];
extern int subwindow;
extern int columns;
extern int dispmode;
extern char *deftmpdir;

#define	CLOCKUPDATE	10	/* sec */

#ifndef	SIG_DFL
#define	SIG_DFL		(sigarg_t (*)__P_((sigfnc_t)))0
#endif
#ifndef	SIG_IGN
#define	SIG_IGN		(sigarg_t (*)__P_((sigfnc_t)))1
#endif

#if	!MSDOS && !defined (SIGWINCH)
# if	defined (SIGWINDOW)
# define	SIGWINCH	SIGWINDOW
# else
#  if	defined (NSIG)
#  define	SIGWINCH	NSIG
#  else
#  define	SIGWINCH	28
#  endif
# endif
#endif	/* !MSDOS && !SIGWINCH */

#if	!defined (SIGIOT) && defined (SIGABRT)
#define	SIGIOT	SIGABRT
#endif

static VOID signalexit __P_((int));
#ifdef	SIGALRM
static sigarg_t ignore_alrm __P_((VOID_A));
#endif
#ifdef	SIGWINCH
static sigarg_t ignore_winch __P_((VOID_A));
#endif
#ifdef	SIGINT
static sigarg_t ignore_int __P_((VOID_A));
#endif
#ifdef	SIGQUIT
static sigarg_t ignore_quit __P_((VOID_A));
#endif
#ifdef	SIGHUP
static sigarg_t hangup __P_((VOID_A));
#endif
#ifdef	SIGILL
static sigarg_t illerror __P_((VOID_A));
#endif
#ifdef	SIGTRAP
static sigarg_t traperror __P_((VOID_A));
#endif
#ifdef	SIGIOT
static sigarg_t ioerror __P_((VOID_A));
#endif
#ifdef	SIGEMT
static sigarg_t emuerror __P_((VOID_A));
#endif
#ifdef	SIGFPE
static sigarg_t floaterror __P_((VOID_A));
#endif
#ifdef	SIGBUS
static sigarg_t buserror __P_((VOID_A));
#endif
#ifdef	SIGSEGV
static sigarg_t segerror __P_((VOID_A));
#endif
#ifdef	SIGSYS
static sigarg_t syserror __P_((VOID_A));
#endif
#ifdef	SIGPIPE
static sigarg_t pipeerror __P_((VOID_A));
#endif
#ifdef	SIGTERM
static sigarg_t terminate __P_((VOID_A));
#endif
#ifdef	SIGXCPU
static sigarg_t xcpuerror __P_((VOID_A));
#endif
#ifdef	SIGXFSZ
static sigarg_t xsizerror __P_((VOID_A));
#endif
#ifdef	SIGWINCH
static sigarg_t wintr __P_((VOID_A));
#endif
#ifdef	SIGALRM
static sigarg_t printtime __P_((VOID_A));
#endif
static int getoption __P_((int, char *[]));
static VOID setexecname __P_((char *));
static VOID setexecpath __P_((char *));

char *origpath = NULL;
char *progpath = NULL;
char *tmpfilename = NULL;
int showsecond = 0;
u_short today[3] = {0, 0, 0};

static char *progname = NULL;
static int timersec = 0;


VOID error(str)
char *str;
{
	forcecleandir(deftmpdir, tmpfilename);
#if	!MSDOS && !defined (_NODOSDRIVE)
	dosallclose();
#endif
	if (!str) str = progname;
	endterm();
	inittty(1);
	fputc('\007', stderr);
	perror(str);
	exit(1);
}

static VOID signalexit(sig)
int sig;
{
	signal(sig, SIG_IGN);
	forcecleandir(deftmpdir, tmpfilename);
#if	!MSDOS && !defined (_NODOSDRIVE)
	dosallclose();
#endif
	inittty(1);
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

#ifdef	SIGALRM
static sigarg_t ignore_alrm(VOID_A)
{
	signal(SIGALRM, (sigarg_t (*)__P_((sigfnc_t)))ignore_alrm);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGWINCH
static sigarg_t ignore_winch(VOID_A)
{
	signal(SIGWINCH, (sigarg_t (*)__P_((sigfnc_t)))ignore_winch);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGINT
static sigarg_t ignore_int(VOID_A)
{
	signal(SIGINT, (sigarg_t (*)__P_((sigfnc_t)))ignore_int);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGQUIT
static sigarg_t ignore_quit(VOID_A)
{
	signal(SIGQUIT, (sigarg_t (*)__P_((sigfnc_t)))ignore_quit);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGHUP
static sigarg_t hangup(VOID_A)
{
	signalexit(SIGHUP);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGILL
static sigarg_t illerror(VOID_A)
{
	signalexit(SIGILL);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGTRAP
static sigarg_t traperror(VOID_A)
{
	signalexit(SIGTRAP);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGIOT
static sigarg_t ioerror(VOID_A)
{
	signalexit(SIGIOT);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGEMT
static sigarg_t emuerror(VOID_A)
{
	signalexit(SIGEMT);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGFPE
static sigarg_t floaterror(VOID_A)
{
	signalexit(SIGFPE);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGBUS
static sigarg_t buserror(VOID_A)
{
	signalexit(SIGBUS);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGSEGV
static sigarg_t segerror(VOID_A)
{
	signalexit(SIGSEGV);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGSYS
static sigarg_t syserror(VOID_A)
{
	signalexit(SIGSYS);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGPIPE
static sigarg_t pipeerror(VOID_A)
{
	signalexit(SIGPIPE);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGTERM
static sigarg_t terminate(VOID_A)
{
	signalexit(SIGTERM);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGXCPU
static sigarg_t xcpuerror(VOID_A)
{
	signalexit(SIGXCPU);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGXFSZ
static sigarg_t xsizerror(VOID_A)
{
	signalexit(SIGXFSZ);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGWINCH
static sigarg_t wintr(VOID_A)
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
	signal(SIGWINCH, (sigarg_t (*)__P_((sigfnc_t)))wintr);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

#ifdef	SIGALRM
static sigarg_t printtime(VOID_A)
{
	static time_t now;
	struct tm *tm;
#if	MSDOS
	struct timeb buffer;
#else
	struct timeval t_val;
	struct timezone tz;
#endif

	signal(SIGALRM, SIG_IGN);
	if (timersec) now++;
	else {
#if	MSDOS
		ftime(&buffer);
		now = (time_t)(buffer.time);
#else
		gettimeofday2(&t_val, &tz);
		now = (time_t)(t_val.tv_sec);
#endif
		timersec = CLOCKUPDATE;
	}
	if (showsecond || timersec == CLOCKUPDATE) {
		tm = localtime(&now);
		today[0] = tm -> tm_year;
		today[1] = tm -> tm_mon;
		today[2] = tm -> tm_mday;
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
	signal(SIGALRM, (sigarg_t (*)__P_((sigfnc_t)))printtime);
#if	defined (SIGARGINT) || defined (NOVOID)
	return(0);
#endif
}
#endif

VOID sigvecset(VOID_A)
{
	getwsize(80, WHEADERMAX + WFOOTER + 2);
#ifdef	SIGALRM
	signal(SIGALRM, (sigarg_t (*)__P_((sigfnc_t)))printtime);
#endif
#ifdef	SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
#ifdef	SIGWINCH
	signal(SIGWINCH, (sigarg_t (*)__P_((sigfnc_t)))wintr);
#endif
}

VOID sigvecreset(VOID_A)
{
#ifdef	SIGALRM
	signal(SIGALRM, (sigarg_t (*)__P_((sigfnc_t)))ignore_alrm);
#endif
#ifdef	SIGTSTP
	signal(SIGTSTP, SIG_DFL);
#endif
#ifdef	SIGWINCH
	signal(SIGWINCH, (sigarg_t (*)__P_((sigfnc_t)))ignore_winch);
#endif
}

VOID title(VOID_A)
{
	char *cp, *eol;

	locate(0, LTITLE);
	putterm(t_standout);
	cputs2("  FD(File & Directory tool) Ver.");
	cp = strchr(version, ' ');
	while (*(++cp) == ' ');
	if (!(eol = strchr(cp, ' '))) eol = cp + strlen(cp);
	cprintf2("%-*.*s", (int)(eol - cp), (int)(eol - cp), cp);
	if (distributor) {
		putch2('#');
		eol++;
	}
	cprintf2("%-*.*s", n_column - 32 - (int)(eol - cp),
		n_column - 32 - (int)(eol - cp), " (c)1995-98 T.Shirai  ");
	putterm(end_standout);
	timersec = 0;
#ifdef	SIGALRM
	printtime();
#endif
}

int loadruncom(file, exist)
char *file;
int exist;
{
#if	!MSDOS
	struct stat status;
	char *tmp;
#endif
	FILE *fp;
	char *cp, *fold, line[MAXLINESTR + 1];
	int i, n, er, cont;

#if	!MSDOS
	tmp = NULL;
	if ((cp = (char *)getenv("TERM"))) {
		tmp = (char *)malloc2(strlen(file) + strlen(cp) + 1 + 1);
		strcpy(tmp, file);
		strcat(tmp, ".");
		strcat(tmp, cp);
		cp = evalpath(strdup2(tmp), 1);
		if (stat2(cp, &status) >= 0
		&& (status.st_mode & S_IFMT) == S_IFREG) file = tmp;
		else {
			free(cp);
			free(tmp);
			tmp = NULL;
		}
	}
	if (!tmp)
#endif
	cp = evalpath(strdup2(file), 1);
	fp = fopen(cp, "r");
	free(cp);
	if (!fp) {
#if	!MSDOS
		if (tmp) free(tmp);
#endif
		if (!exist) return(0);
		cprintf2("%s: Not found\r\n", file);
		return(-1);
	}

	fold = NULL;
	n = er = 0;
	while (fgets(line, MAXLINESTR, fp)) {
		n++;
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
			if (fold) {
				fold = (char *)realloc2(fold,
					strlen(fold) + strlen(line) + 1);
				strcat(fold, line);
			}
			else if (cont) fold = strdup2(skipspace(line));
		}

		if (cont) continue;

		if (fold) {
			i = execbuiltin(fold, NULL, NULL, 0);
			free(fold);
			fold = NULL;
		}
		else if (*(cp = skipspace(line)))
			i = execbuiltin(cp, NULL, NULL, 0);
		else i = 4;
		if (i < 4) {
			er++;
			cprintf2("%s, line %d: %s\r\n", file, n, ILFNC_K);
			cprintf2("\t%s\r\n", line);
			tflush();
		}
	}

	if (fold) {
		i = execbuiltin(fold, NULL, NULL, 0);
		free(fold);
		if (i < 4) {
			er++;
			cprintf2("%s, line %d: %s\r\n", file, n, ILFNC_K);
			cprintf2("\t%s\r\n", line);
			tflush();
		}
	}

	fclose(fp);
#if	!MSDOS
	if (tmp) free(tmp);
#endif
	return(er ? -1 : 0);
}

static int getoption(argc, argv)
int argc;
char *argv[];
{
	char *cp, *env, *tmp;
	int i, n;

	for (i = 1; i < argc; i++) {
		env = &(argv[i][1]);
		if (*argv[i] != '-') return(i);
		if (*env == '-' && !*(env + 1)) return(i + 1);

		tmp = argv[i];
		argv[i] = strdup2(env);
		n = argc - i;
		cp = getenvval(&n, &(argv[i]));
		env = argv[i];
		argv[i] = tmp;
		if (cp == (char *)-1) {
			errno = EINVAL;
			error(argv[i]);
		}
		i += n - 1;

		if (setenv2(env, cp, 1) < 0) error(env);
		if (cp) free(cp);
		free(env);
	}
	return(i);
}

static VOID setexecname(argv)
char *argv;
{
	char buf[MAXNAMLEN + 1];

	if ((progname = strrdelim(argv))) progname++;
	else progname = argv;
#if	MSDOS
	{
		char *cp;

		if ((cp = strchr(argv, '.'))) *cp = '\0';
	}
#endif
	sprintf(buf, "%s%ld", progname, (long)getpid());
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
	if (strdelim(argv)) cp = argv;
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
		if ((cp = strrdelim(buf))) *cp = '\0';
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
	getwsize(80, WHEADERMAX + WFOOTER + 2);
	sigvecreset();

#ifdef	SIGHUP
	signal(SIGHUP, (sigarg_t (*)__P_((sigfnc_t)))hangup);
#endif
#ifdef	SIGINT
	signal(SIGINT, (sigarg_t (*)__P_((sigfnc_t)))ignore_int);
#endif
#ifdef	SIGQUIT
	signal(SIGQUIT, (sigarg_t (*)__P_((sigfnc_t)))ignore_quit);
#endif
#ifdef	SIGILL
	signal(SIGILL, (sigarg_t (*)__P_((sigfnc_t)))illerror);
#endif
#ifdef	SIGTRAP
	signal(SIGTRAP, (sigarg_t (*)__P_((sigfnc_t)))traperror);
#endif
#ifdef	SIGIOT
	signal(SIGIOT, (sigarg_t (*)__P_((sigfnc_t)))ioerror);
#endif
#ifdef	SIGEMT
	signal(SIGEMT, (sigarg_t (*)__P_((sigfnc_t)))emuerror);
#endif
#ifdef	SIGFPE
	signal(SIGFPE, (sigarg_t (*)__P_((sigfnc_t)))floaterror);
#endif
#ifdef	SIGBUS
	signal(SIGBUS, (sigarg_t (*)__P_((sigfnc_t)))buserror);
#endif
#ifdef	SIGSEGV
	signal(SIGSEGV, (sigarg_t (*)__P_((sigfnc_t)))segerror);
#endif
#ifdef	SIGSYS
	signal(SIGSYS, (sigarg_t (*)__P_((sigfnc_t)))syserror);
#endif
#ifdef	SIGPIPE
	signal(SIGPIPE, (sigarg_t (*)__P_((sigfnc_t)))pipeerror);
#endif
#ifdef	SIGTERM
	signal(SIGTERM, (sigarg_t (*)__P_((sigfnc_t)))terminate);
#endif
#ifdef	SIGXCPU
	signal(SIGXCPU, (sigarg_t (*)__P_((sigfnc_t)))xcpuerror);
#endif
#ifdef	SIGXFSZ
	signal(SIGXFSZ, (sigarg_t (*)__P_((sigfnc_t)))xsizerror);
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
#ifndef	_NODOSDRIVE
	unitblpath = progpath;
#endif
	i = loadruncom(DEFRUNCOM, 0);
	i += loadruncom(RUNCOMFILE, 0);
	if (i < 0) warning(0, HITKY_K);
	i = getoption(argc, argv);
#if	!MSDOS
	adjustpath();
#endif
	evalenv();
	if ((dispmode = atoi2(getenv2("FD_DISPLAYMODE"))) < 0)
#if	(DISPLAYMODE < 0) || (DISPLAYMODE > 7)
		dispmode = 0;
#else
		dispmode = DISPLAYMODE;
#endif
	columns = atoi2(getenv2("FD_COLUMNS"));
	if (columns < 1 || columns == 4 || columns > 5)
#if	(COLUMNS < 1) || (COLUMNS == 4) || (COLUMNS > 5)
		columns = 2;
#else
		columns = COLUMNS;
#endif
#if	(ADJTTY == 0)
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

	loadhistory(0, HISTORYFILE);
	entryhist(1, origpath, 1);
	putterms(t_clear);
	title();
	sigvecset();

	main_fd(argv[i]);
	if (_chdir2(origpath) < 0) error(origpath);
	free(origpath);
#if	!MSDOS && !defined (_NODOSDRIVE)
	dosallclose();
#endif

	exit2(0);
	return(0);
}
