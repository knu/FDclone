/*
 *	FD (File & Directory maintenance tool)
 *
 *	by T.Shirai <shirai@nintendo.co.jp>
 */

#include <ctype.h>
#include <signal.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"
#include "version.h"

#ifndef	_NOORIGSHELL
#include "system.h"
#endif

#if	MSDOS
#include <process.h>
#include <sys/timeb.h>
# ifdef	DJGPP
extern char *adjustfname __P_((char *));
# else
# include <dos.h>
#  ifdef	__TURBOC__
extern unsigned _stklen = 0x6000;
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

#define	sigcst_t	sigarg_t (*)__P_((sigfnc_t))

#ifdef	_NOORIGSHELL
extern char **environ;
#endif
extern bindtable bindlist[];
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
extern int win_x;
extern int win_y;
extern char *deftmpdir;
#ifndef	_NODOSDRIVE
extern char *unitblpath;
#endif

#define	CLOCKUPDATE	10	/* sec */

#ifndef	SIG_DFL
#define	SIG_DFL		((sigcst_t)0)
#endif
#ifndef	SIG_IGN
#define	SIG_IGN		((sigcst_t)1)
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

#if	MSDOS && !defined (PROTECTED_MODE)
static harderr_t far criticalerror __P_((u_short, u_short, u_short far *));
#endif
static VOID NEAR signalexit __P_((int));
#ifdef	SIGALRM
static int ignore_alrm __P_((VOID_A));
#endif
#ifdef	SIGWINCH
static int ignore_winch __P_((VOID_A));
#endif
#ifdef	SIGINT
static int ignore_int __P_((VOID_A));
#endif
#ifdef	SIGQUIT
static int ignore_quit __P_((VOID_A));
#endif
#ifdef	SIGHUP
static int hangup __P_((VOID_A));
#endif
#ifdef	SIGILL
static int illerror __P_((VOID_A));
#endif
#ifdef	SIGTRAP
static int traperror __P_((VOID_A));
#endif
#ifdef	SIGIOT
static int ioerror __P_((VOID_A));
#endif
#ifdef	SIGEMT
static int emuerror __P_((VOID_A));
#endif
#ifdef	SIGFPE
static int floaterror __P_((VOID_A));
#endif
#ifdef	SIGBUS
static int buserror __P_((VOID_A));
#endif
#ifdef	SIGSEGV
static int segerror __P_((VOID_A));
#endif
#ifdef	SIGSYS
static int syserror __P_((VOID_A));
#endif
#ifdef	SIGPIPE
static int pipeerror __P_((VOID_A));
#endif
#ifdef	SIGTERM
static int terminate __P_((VOID_A));
#endif
#ifdef	SIGXCPU
static int xcpuerror __P_((VOID_A));
#endif
#ifdef	SIGXFSZ
static int xsizerror __P_((VOID_A));
#endif
#ifdef	SIGWINCH
static int wintr __P_((VOID_A));
#endif
#ifdef	SIGALRM
static int printtime __P_((VOID_A));
#endif
#ifdef	_NOORIGSHELL
static int NEAR doexec __P_((char *, char *, int, char *));
#endif
static int NEAR initoption __P_((int, char *[], char *[]));
static int NEAR evaloption __P_((char *[]));
static VOID NEAR setexecname __P_((char *));
static VOID NEAR setexecpath __P_((char *, char *[]));
static VOID NEAR prepareexitfd __P_((VOID_A));

char *origpath = NULL;
char *progpath = NULL;
char *progname = NULL;
char *tmpfilename = NULL;
#if	!MSDOS
int adjtty = 0;
#endif
int showsecond = 0;
int hideclock = 0;
u_short today[3] = {0, 0, 0};
int fd_restricted = 0;
#ifndef	_NOCUSTOMIZE
char **orighelpindex = NULL;
bindtable *origbindlist = NULL;
# if	!MSDOS && !defined (_NOKEYMAP)
keymaptable *origkeymaplist = NULL;
# endif
#endif
int inruncom = 0;
int fdmode = 0;

static int timersec = 0;


#if	MSDOS && !defined (PROTECTED_MODE)
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
	int duperrno;

	duperrno = errno;
	sigvecreset();
	forcecleandir(deftmpdir, tmpfilename);
#ifndef	_NODOSDRIVE
	dosallclose();
#endif
	if (!s) s = progname;
	endterm();
	inittty(1);
	fputc('\007', stderr);
	errno = duperrno;
	perror(s);
#ifndef	_NOORIGSHELL
# if	!MSDOS && !defined (NOJOB)
	killjob();
# endif
	prepareexit();
#endif
	exit(2);
}

static VOID NEAR signalexit(sig)
int sig;
{
	signal(sig, SIG_IGN);
	forcecleandir(deftmpdir, tmpfilename);
#ifndef	_NODOSDRIVE
	dosallclose();
#endif
	endterm();
	inittty(1);
	signal(sig, SIG_DFL);
	kill(getpid(), sig);
}

#ifdef	SIGALRM
static int ignore_alrm(VOID_A)
{
	signal(SIGALRM, (sigcst_t)ignore_alrm);
	return(0);
}
#endif

#ifdef	SIGWINCH
static int ignore_winch(VOID_A)
{
	signal(SIGWINCH, (sigcst_t)ignore_winch);
	return(0);
}
#endif

#ifdef	SIGINT
static int ignore_int(VOID_A)
{
	signal(SIGINT, (sigcst_t)ignore_int);
	return(0);
}
#endif

#ifdef	SIGQUIT
static int ignore_quit(VOID_A)
{
	signal(SIGQUIT, (sigcst_t)ignore_quit);
	return(0);
}
#endif

#ifdef	SIGHUP
static int hangup(VOID_A)
{
	signalexit(SIGHUP);
	return(0);
}
#endif

#ifdef	SIGILL
static int illerror(VOID_A)
{
	signalexit(SIGILL);
	return(0);
}
#endif

#ifdef	SIGTRAP
static int traperror(VOID_A)
{
	signalexit(SIGTRAP);
	return(0);
}
#endif

#ifdef	SIGIOT
static int ioerror(VOID_A)
{
	signalexit(SIGIOT);
	return(0);
}
#endif

#ifdef	SIGEMT
static int emuerror(VOID_A)
{
	signalexit(SIGEMT);
	return(0);
}
#endif

#ifdef	SIGFPE
static int floaterror(VOID_A)
{
	signalexit(SIGFPE);
	return(0);
}
#endif

#ifdef	SIGBUS
static int buserror(VOID_A)
{
	signalexit(SIGBUS);
	return(0);
}
#endif

#ifdef	SIGSEGV
static int segerror(VOID_A)
{
	signalexit(SIGSEGV);
	return(0);
}
#endif

#ifdef	SIGSYS
static int syserror(VOID_A)
{
	signalexit(SIGSYS);
	return(0);
}
#endif

#ifdef	SIGPIPE
static int pipeerror(VOID_A)
{
	signalexit(SIGPIPE);
	return(0);
}
#endif

#ifdef	SIGTERM
static int terminate(VOID_A)
{
	signalexit(SIGTERM);
	return(0);
}
#endif

#ifdef	SIGXCPU
static int xcpuerror(VOID_A)
{
	signalexit(SIGXCPU);
	return(0);
}
#endif

#ifdef	SIGXFSZ
static int xsizerror(VOID_A)
{
	signalexit(SIGXFSZ);
	return(0);
}
#endif

#ifdef	SIGWINCH
static int wintr(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal(SIGWINCH, SIG_IGN);
	getwsize(80, WHEADERMAX + WFOOTER + WFILEMIN);
	rewritefile(1);
	if (subwindow) ungetch2(CTRL('L'));
	signal(SIGWINCH, (sigcst_t)wintr);
	errno = duperrno;
	return(0);
}
#endif

#ifdef	SIGALRM
static int printtime(VOID_A)
{
	static time_t now;
	struct tm *tm;
#if	MSDOS
	struct timeb buffer;
#else
	struct timeval t_val;
	struct timezone tz;
#endif
	int duperrno;

	duperrno = errno;
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
		_mtrace_file = "localtime(start)";
		tm = localtime(&now);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "localtime(end)";
			malloc(0);	/* dummy malloc */
		}
#else
		tm = localtime(&now);
#endif
		today[0] = tm -> tm_year;
		today[1] = tm -> tm_mon;
		today[2] = tm -> tm_mday;
		if (!hideclock) {
			locate(n_column - 16 - ((showsecond) ? 3 : 0), LTITLE);
			putterm(t_standout);
			cprintf2("%02d-%02d-%02d %02d:%02d",
				tm -> tm_year % 100,
				tm -> tm_mon + 1, tm -> tm_mday,
				tm -> tm_hour, tm -> tm_min);
			if (showsecond) cprintf2(":%02d", tm -> tm_sec);
			putterm(end_standout);
			locate(win_x, win_y);
			tflush();
		}
	}
	timersec--;
	signal(SIGALRM, (sigcst_t)printtime);
	errno = duperrno;
	return(0);
}
#endif

VOID sigvecset(VOID_A)
{
#ifdef	SIGALRM
	signal(SIGALRM, (sigcst_t)printtime);
#endif
#ifdef	SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
#ifdef	SIGWINCH
	signal(SIGWINCH, (sigcst_t)wintr);
#endif
#ifndef	_NODOSDRIVE
	doswaitfunc = waitmes;
	dosintrfunc = intrkey;
#endif
}

VOID sigvecreset(VOID_A)
{
#ifdef	SIGALRM
	signal(SIGALRM, (sigcst_t)ignore_alrm);
#endif
#ifdef	SIGTSTP
	signal(SIGTSTP, SIG_DFL);
#endif
#ifdef	SIGWINCH
	signal(SIGWINCH, (sigcst_t)ignore_winch);
#endif
#ifndef	_NODOSDRIVE
	doswaitfunc = NULL;
	dosintrfunc = NULL;
#endif
}

VOID title(VOID_A)
{
	char *cp, *eol;
	int i;

	locate(0, LTITLE);
	putterm(t_standout);
	cputs2("  FD(File & Directory tool) Ver.");
	cp = strchr(version, ' ');
	while (*(++cp) == ' ');
	if (!(eol = strchr(cp, ' '))) eol = cp + strlen(cp);
	i = eol - cp;
	cprintf2("%-*.*s", i, i, cp);
	if (distributor) {
		putch2('#');
		i++;
	}
	cp = " (c)1995-2002 T.Shirai  ";
	cputs2(cp);
	i = n_column - 32 - strlen(cp) - i;
	while (i-- > 0) putch2(' ');
	putterm(end_standout);
	timersec = 0;
#ifdef	SIGALRM
	printtime();
#endif
}

#ifdef	_NOORIGSHELL
static int NEAR doexec(command, file, n, line)
char *command, *file;
int n;
char *line;
{
	char *cp;
	int i;

	if (!(*(cp = skipspace(command)))) i = 0;
	else i = execpseudoshell(cp, 0, 1);
	if (i) {
		putterm(l_clear);
		cprintf2("%s, line %d: %s\r\n", file, n, ILFNC_K);
		putterm(l_clear);
		cprintf2("\t%s\r\n", line);
		tflush();
	}
	free(command);
	return((i) ? -1 : 0);
}
#endif	/* _NOORIGSHELL */

/*ARGSUSED*/
int loadruncom(file, exist)
char *file;
int exist;
{
#ifdef	_NOORIGSHELL
	FILE *fp;
	char *cp, *tmp, *fold, *line;
	int n, cont;
#endif
	int er;

#ifdef	_NOORIGSHELL
# if	!MSDOS
	tmp = NULL;
	if (!exist && (tmp = (char *)getenv("TERM"))) {
		struct stat st;

		cp = malloc2(strlen(file) + strlen(tmp) + 1 + 1);
		strcpy(strcpy2(strcpy2(cp, file), "."), tmp);
		tmp = evalpath(cp, 1);
		if (stat2(tmp, &st) < 0 || (st.st_mode & S_IFMT) != S_IFREG) {
			free(tmp);
			tmp = NULL;
		}
	}
	if (!tmp)
# endif	/* !MSDOS */
	tmp = evalpath(strdup2(file), 1);
	fp = fopen(tmp, "r");
	if (!fp) {
		if (!exist) {
			free(tmp);
			return(0);
		}
		cprintf2("%s: Not found\r\n", tmp);
		free(tmp);
		return(-1);
	}

	fold = NULL;
	n = er = 0;
	while ((line = fgets2(fp, 0))) {
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

		if (doexec(fold, tmp, n, line) < 0) er++;
		if (fold != line) free(line);
		fold = NULL;
	}

	if (fold && doexec(fold, tmp, n, line) < 0) er++;
	fclose(fp);
	free(tmp);
#else	/* !_NOORIGSHELL */
	file = evalpath(strdup2(file), 1);
	stdiomode();
	er = execruncom(file, 1);
	ttyiomode();
	free(file);
#endif	/* !_NOORIGSHELL */

	return(er ? -1 : 0);
}

/*ARGSUSED*/
static int NEAR initoption(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
	char *cp, **optv;
	int i, j, optc;

	optc = 1;
	optv = (char **)malloc2((argc + 1) * sizeof(char *));
	optv[0] = argv[0];
	optv[1] = NULL;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '+') cp = argv[i];
		else if (argv[i][0] != '-' || !argv[i][1]
		|| (argv[i][1] == '-' && !argv[i][2])) break;
		else for (cp = &(argv[i][1]); *cp; cp++) {
			if (*cp != '_' && !isalpha(*cp)
			&& (cp == &(argv[i][1]) || *cp < '0' || *cp > '9'))
				break;
		}
		if (cp <= &(argv[i][1]) || *cp != '=') {
			optv[optc++] = argv[i];
			optv[optc] = NULL;
			for (j = i; j < argc; j++) argv[j] = argv[j + 1];
			argc--;
			i--;
		}
	}

#ifdef	_NOORIGSHELL
	inittty(0);
	getterment();
#else
	if (initshell(optc, optv, envp) < 0) exit2(RET_FAIL);
#endif	/* !_NOORIGSHELL */
	free(optv);
	return(argc);
}

static int NEAR evaloption(argv)
char *argv[];
{
	char *cp, *tmp;
	int i;

	for (i = 1; argv[i]; i++) {
		if (argv[i][0] != '-') return(i);
		if (!argv[i][1] || (argv[i][1] == '-' && !argv[i][2]))
			return(i + 1);
		tmp = strdup2(&(argv[i][1]));
		if ((cp = strchr(tmp, '='))) *(cp++) = '\0';
		setenv2(tmp, cp);
		free(tmp);
	}
	return(i);
}

static VOID NEAR setexecname(argv)
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

static VOID NEAR setexecpath(argv, envp)
char *argv, *envp[];
{
	char *cp, buf[MAXPATHLEN];

	origpath = getwd2();
	strcpy(fullpath, origpath);

#if	MSDOS
	cp = argv;
#else
	if (strdelim(argv, 0)) cp = argv;
	else {
		int i;

		for (i = 0; envp[i]; i++)
			if (!strncmp(envp[i], "PATH=", sizeof("PATH=") - 1))
				break;
		if (!envp[i]) cp = NULL;
		else cp = searchpath(argv, envp[i] + sizeof("PATH=") - 1);
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

static VOID NEAR prepareexitfd(VOID_A)
{
	if (_chdir2(origpath) < 0) error(origpath);
	free(origpath);
#ifndef	_NODOSDRIVE
	dosallclose();
#endif
	free(progpath);

#ifdef	DEBUG
	free(tmpfilename);
# ifdef	_NOORIGSHELL
	freeenv();
# endif
# ifndef	_NOCUSTOMIZE
	freevar(orighelpindex);
	free(origbindlist);
#  if	!MSDOS && !defined (_NOKEYMAP)
	freekeymap(origkeymaplist);
#  endif
# endif
	freeenvpath();
	freehistory(0);
	freehistory(1);
	freedefine();
# if	!MSDOS
	freeidlist();
# endif
# if	!defined (_NOUSEHASH) && defined (_NOORIGSHELL)
	searchhash(NULL, NULL, NULL);
# endif
#endif	/* DEBUG */
}

int main(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
#ifndef	_NOORIGSHELL
	char *cp;
#endif
	int i;

#ifdef	DEBUG
	mtrace();
#endif

#if	MSDOS && defined (DJGPP)
	adjustfname(argv[0]);
#endif
	setexecname(argv[0]);

#if	MSDOS && !defined (PROTECTED_MODE)
	_harderr(criticalerror);
#endif

#ifdef	SIGHUP
	signal(SIGHUP, (sigcst_t)hangup);
#endif
#ifdef	SIGINT
	signal(SIGINT, (sigcst_t)ignore_int);
#endif
#ifdef	SIGQUIT
	signal(SIGQUIT, (sigcst_t)ignore_quit);
#endif
#ifdef	SIGILL
	signal(SIGILL, (sigcst_t)illerror);
#endif
#ifdef	SIGTRAP
	signal(SIGTRAP, (sigcst_t)traperror);
#endif
#ifdef	SIGIOT
	signal(SIGIOT, (sigcst_t)ioerror);
#endif
#ifdef	SIGEMT
	signal(SIGEMT, (sigcst_t)emuerror);
#endif
#ifdef	SIGFPE
	signal(SIGFPE, (sigcst_t)floaterror);
#endif
#ifdef	SIGBUS
	signal(SIGBUS, (sigcst_t)buserror);
#endif
#ifdef	SIGSEGV
	signal(SIGSEGV, (sigcst_t)segerror);
#endif
#ifdef	SIGSYS
	signal(SIGSYS, (sigcst_t)syserror);
#endif
#ifdef	SIGPIPE
	signal(SIGPIPE, (sigcst_t)pipeerror);
#endif
#ifdef	SIGTERM
	signal(SIGTERM, (sigcst_t)terminate);
#endif
#ifdef	SIGXCPU
	signal(SIGXCPU, (sigcst_t)xcpuerror);
#endif
#ifdef	SIGXFSZ
	signal(SIGXFSZ, (sigcst_t)xsizerror);
#endif
	sigvecreset();

#ifndef	_NOARCHIVE
	for (maxlaunch = 0; maxlaunch < MAXLAUNCHTABLE; maxlaunch++) {
		if (!launchlist[maxlaunch].ext) break;
		launchlist[maxlaunch].ext =
			strdup2(launchlist[maxlaunch].ext);
		launchlist[maxlaunch].comm =
			strdup2(launchlist[maxlaunch].comm);
# if	FD >= 2
		launchlist[maxlaunch].format =
			strdup2(launchlist[maxlaunch].format);
# endif
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

#ifdef	_NOORIGSHELL
	for (i = 0; envp[i]; i++);
	environ = (char **)malloc2((i + 1) * sizeof(char *));
	for (i = 0; envp[i]; i++) environ[i] = strdup2(envp[i]);
	environ[i] = NULL;
#endif

	setexecpath(argv[0], envp);
#ifndef	_NODOSDRIVE
	unitblpath = progpath;
#endif

#ifndef	_NOCUSTOMIZE
	orighelpindex = copystrarray(NULL, helpindex, NULL, 10);
	origbindlist = copybind(NULL, bindlist);
# if	!MSDOS && !defined (_NOKEYMAP)
	origkeymaplist = copykeymap(NULL);
# endif
#endif	/* !_NOCUSTOMIZE */

#ifndef	_NOORIGSHELL
	cp = progname;
	if (*cp == '-') cp++;
# if	MSDOS
	if (*cp == 'r' || *cp == 'R') cp++;
# else
	if (*cp == 'r') cp++;
# endif
	if (!strpathcmp(cp, FDSHELL)) {
		if (!Xgetwd(fullpath)) exit2(1);
		i = main_shell(argc, argv, envp);
		prepareexitfd();
		exit2(i);
	}
#endif

	fdmode = 1;
	argc = initoption(argc, argv, envp);
	ttyiomode();
	initterm();
	getwsize(80, WHEADERMAX + WFOOTER + WFILEMIN);

	locate(0, n_line - 1);
	inruncom = 1;
	i = loadruncom(DEFRUNCOM, 0);
	i += loadruncom(RUNCOMFILE, 0);
	inruncom = 0;
	sigvecset();
	if (i < 0) {
		hideclock = 1;
		warning(0, HITKY_K);
	}
	i = evaloption(argv);
#if	!MSDOS && defined (_NOORIGSHELL)
	adjustpath();
#endif
	evalenv();
#if	!MSDOS
	if (adjtty) {
		stdiomode();
		inittty(0);
		ttyiomode();
	}
#endif	/* !MSDOS */

	loadhistory(0, HISTORYFILE);
	entryhist(1, origpath, 1);
	putterms(t_clear);

	main_fd(argv[i]);
	sigvecreset();
	prepareexitfd();

	stdiomode();
#ifndef	_NOORIGSHELL
# if	!MSDOS && !defined (NOJOB)
	killjob();
# endif
	prepareexit();
#endif
	exit2(0);
	return(0);
}
