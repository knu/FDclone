/*
 *	FD (File & Directory maintenance tool)
 *
 *	by T.Shirai <shirai@red.nintendo.co.jp>
 */

#include <signal.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"
#include "version.h"

#if	MSDOS
#include <process.h>
#include <sys/timeb.h>
# ifdef	DJGPP
extern char *adjustfname __P_((char *));
# else
# include <dos.h>
#  ifdef	__TURBOC__
extern unsigned _stklen = 8000;
#  define	harderr_t	void
#  else
#  define	harderr_t	int
#  endif
# endif
#else
# ifdef	_NODOSDRIVE
# include <sys/param.h>
# endif
#endif	/* !MSDOS */

#ifndef	_NODOSDRIVE
#include "dosdisk.h"
#endif

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

extern char **environ;
#ifndef	_NOARCHIVE
extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
#endif
extern char fullpath[];
extern char **history[];
extern char *helpindex[];
extern int subwindow;
extern char *deftmpdir;
#if	DEBUG
extern char *rockridgepath;
#endif

#define	CLOCKUPDATE	10	/* sec */

#ifndef	SIG_DFL
#define	SIG_DFL		((sigarg_t (*)__P_((sigfnc_t)))0)
#endif
#ifndef	SIG_IGN
#define	SIG_IGN		((sigarg_t (*)__P_((sigfnc_t)))1)
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

#if	MSDOS && !defined (DJGPP)
static harderr_t far criticalerror __P_((u_short, u_short, u_short far *));
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
static int doexec __P_((char *, char *, int, char *));
static int getoption __P_((int, char *[]));
static VOID setexecname __P_((char *));
static VOID setexecpath __P_((char *));

char *origpath = NULL;
char *progpath = NULL;
char *tmpfilename = NULL;
#if	!MSDOS
int adjtty = 0;
#endif
int showsecond = 0;
u_short today[3] = {0, 0, 0};

static char *progname = NULL;
static int timersec = 0;


#if	MSDOS && !defined (DJGPP)
/*ARGSUSED*/
static harderr_t far criticalerror(deverr, errval, devhdr)
u_short deverr, errval;
u_short far *devhdr;
{
	if ((deverr & 0x8800) == 0x0800) _hardresume(_HARDERR_FAIL);
	_hardresume(_HARDERR_ABORT);
}
#endif

VOID error(s)
char *s;
{
	forcecleandir(deftmpdir, tmpfilename);
#ifndef	_NODOSDRIVE
	dosallclose();
#endif
	if (!s) s = progname;
	endterm();
	inittty(1);
	fputc('\007', stderr);
	perror(s);
	exit(2);
}

static VOID signalexit(sig)
int sig;
{
	signal(sig, SIG_IGN);
	forcecleandir(deftmpdir, tmpfilename);
#ifndef	_NODOSDRIVE
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
	getwsize(80, WHEADERMAX + WFOOTER + WFILEMIN);
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
#ifdef	DEBUG
		_mtrace_file = "localtime(x4)";
#endif
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
	getwsize(80, WHEADERMAX + WFOOTER + WFILEMIN);
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
		n_column - 32 - (int)(eol - cp), " (c)1995-2000 T.Shirai  ");
	putterm(end_standout);
	timersec = 0;
#ifdef	SIGALRM
	printtime();
#endif
}

static int doexec(command, file, n, line)
char *command, *file;
int n;
char *line;
{
	char *cp;
	int i;

	if (!(*(cp = skipspace(command)))) i = 4;
	else i = execbuiltin(cp, NULL, NULL, 0);
	if (i < 4) {
		putterm(l_clear);
		cprintf2("%s, line %d: %s\r\n", file, n, ILFNC_K);
		putterm(l_clear);
		cprintf2("\t%s\r\n", line);
		tflush();
	}
	free(command);
	return(i < 4 ? -1 : 0);
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
	char *cp, *fold, *line;
	int n, er, cont;

#if	!MSDOS
	tmp = NULL;
	if (!exist && (cp = (char *)getenv("TERM"))) {
		tmp = malloc2(strlen(file) + strlen(cp) + 1 + 1);
		strcpy(strcpy2(strcpy2(tmp, file), "."), cp);
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
	while ((line = fgets2(fp))) {
		n++;
		if (*line == ';' || *line == '#') {
			free(line);
			continue;
		}

		cp = line + strlen(line);
		for (cp--; cp >= line && (*cp == ' ' || *cp == '\t'); cp--);
		*(cp + 1) = '\0';

		cont = 0;
		if (cp >= line && *cp == META
		&& (cp - 1 < line || *(cp - 1) != META)
		&& !onkanji1(line, cp - line - 1)) {
			*cp = '\0';
			cont = 1;
		}

		if (!fold) fold = line;
		else if (*line) {
			fold = realloc2(fold, strlen(fold) + strlen(line) + 1);
			strcat(fold, line);
		}

		if (cont) {
			if (fold != line) free(line);
			continue;
		}

		if (doexec(fold, file, n, line) < 0) er++;
		if (fold != line) free(line);
		fold = NULL;
	}

	if (fold && doexec(fold, file, n, line) < 0) er++;
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

		if (setenv2(env, cp) < 0) error(env);
		if (cp) free(cp);
		free(env);
	}
	return(i);
}

static VOID setexecname(argv)
char *argv;
{
	char buf[MAXNAMLEN + 1];

	if ((progname = strrdelim(argv, 1))) progname++;
	else progname = argv;
#if	MSDOS
	{
		char *cp;

		if ((cp = strchr(progname, '.'))) *cp = '\0';
	}
	sprintf(buf, "FD%ld", (long)getpid());
#else
	sprintf(buf, "%s%ld", progname, (long)getpid());
#endif
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
	if (strdelim(argv, 0)) cp = argv;
	else {
		adjustpath();
		cp = searchpath(argv);
	}
	if (!cp) progpath = strdup2(origpath);
	else
#endif
	{
		realpath2(cp, buf, 1);
		if (cp != argv) free(cp);
		if ((cp = strrdelim(buf, 0))) *cp = '\0';
		progpath = strdup2(buf);
	}
}

int main(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
	char **ep;
	int i;

#ifdef	DEBUG
	mtrace();
#endif

#if	MSDOS && defined (DJGPP)
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
	getwsize(80, WHEADERMAX + WFOOTER + WFILEMIN);
	sigvecreset();

#if	MSDOS && !defined (DJGPP)
	_harderr(criticalerror);
#endif

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
	for (maxlaunch = 0; maxlaunch < MAXLAUNCHTABLE; maxlaunch++) {
		if (!launchlist[maxlaunch].ext) break;
		launchlist[maxlaunch].ext =
			strdup2(launchlist[maxlaunch].ext);
		launchlist[maxlaunch].comm =
			strdup2(launchlist[maxlaunch].comm);
	}
	for (maxarchive = 0; maxarchive < MAXARCHIVETABLE; maxarchive++) {
		if (!archivelist[maxarchive].ext) break;
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
	for (i = 0; fdtype[i].name; i++) {
		fdtype[i].name = strdup2(fdtype[i].name);
# ifdef	HDDMOUNT
		fdtype[i].offset = 0;
# endif
	}
#endif

	for (i = 0; envp[i]; i++);
	ep = (char **)malloc2((i + 1) * sizeof(char *));
	for (i = 0; envp[i]; i++) ep[i] = strdup2(envp[i]);
	ep[i] = NULL;
	environ = ep;

	setexecpath(argv[0]);
#ifndef	_NODOSDRIVE
	unitblpath = progpath;
	doswaitfunc = waitmes;
	dosintrfunc = intrkey;
#endif
	i = loadruncom(DEFRUNCOM, 0);
	i += loadruncom(RUNCOMFILE, 0);
	if (i < 0) warning(0, HITKY_K);
	i = getoption(argc, argv);
#if	!MSDOS
	adjustpath();
#endif
	evalenv();
#if	!MSDOS
	if (adjtty) {
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
#endif	/* !MSDOS */

	loadhistory(0, HISTORYFILE);
	entryhist(1, origpath, 1);
	putterms(t_clear);
	sigvecset();

	main_fd(argv[i]);
	if (_chdir2(origpath) < 0) error(origpath);
	free(origpath);
#ifndef	_NODOSDRIVE
	dosallclose();
#endif
	free(progpath);

#ifdef	DEBUG
	free(tmpfilename);
	freeenv();
	freehistory(0);
	freehistory(1);
	if (deftmpdir) free(deftmpdir);
	if (rockridgepath) free(rockridgepath);
	freedefine();
#ifndef	_NOUSEHASH
	freehash(NULL);
#endif
#endif
	exit2(0);
	return(0);
}
