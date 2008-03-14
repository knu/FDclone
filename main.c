/*
 *	FD (File & Directory maintenance tool)
 *
 *	by T.Shirai <shirai@unixusers.net>
 */

#include "fd.h"
#include "func.h"
#include "kanji.h"
#include "version.h"

#if	MSDOS
#include <process.h>
# ifndef	DJGPP
# include <dos.h>
# endif
#endif	/* !MSDOS */

#ifdef	_NOORIGSHELL
#include "termio.h"
#include "wait.h"
#define	isorgpid()	(1)
#else
#include "system.h"
#define	isorgpid()	(mypid == orgpid)
#endif

#if	!defined (_NOIME) && defined (DEBUG)
#include "roman.h"
#endif

#ifdef	__TURBOC__
extern u_int _stklen = 0x5800;
#define	harderr_t	void
#else
#define	harderr_t	int
#endif

#ifndef	BINDIR
#define	BINDIR		"/usr/local/bin"
#endif
#ifndef	DATADIR
#define	DATADIR		progpath
#endif
#ifndef	DEFPATH
#define	DEFPATH		":/bin:/usr/bin"
#endif

#if	MSDOS
# ifndef	BSPATHDELIM
extern char *adjustpname __P_((char *));
# endif
# if	defined (DJGPP) || !defined (BSPATHDELIM)
extern char *adjustfname __P_((char *));
# endif
#endif	/* MSDOS */

#if	!defined (_NOORIGSHELL) && !defined (NOJOB)
extern VOID killjob __P_((VOID_A));
#endif

#ifdef	_NOORIGSHELL
extern char **environ;
extern char **environ2;
#endif
extern bindtable bindlist[];
#ifndef	_NOARCHIVE
extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
#endif
extern int wheader;
extern char fullpath[];
extern short savehist[];
extern char *helpindex[];
extern int subwindow;
extern int win_x;
extern int win_y;
#ifndef	_NOCUSTOMIZE
extern int custno;
#endif
extern char *deftmpdir;
#ifdef	_USEUNICODE
extern char *unitblpath;
#endif
#ifndef	_NOIME
extern char *dicttblpath;
#endif
#ifndef	_NOPTY
extern int ptymode;
extern int ptyinternal;
extern int parentfd;
#endif

#define	CLOCKUPDATE	10	/* sec */

#if	MSDOS && !defined (PROTECTED_MODE)
static harderr_t far criticalerror __P_((u_int, u_int, u_short far *));
#endif
#ifdef	_NOORIGSHELL
static VOID NEAR Xexit2 __P_((int));
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
#ifdef	SIGCONT
static int ignore_cont __P_((VOID_A));
#endif
#ifdef	SIGHUP
static int hanguperror __P_((VOID_A));
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
static int NEAR minrow __P_((int));
#ifndef	_NOEXTRAWIN
static int NEAR biaswsize __P_((int, int));
#endif
#ifdef	SIGWINCH
static int wintr __P_((VOID_A));
#endif
static VOID NEAR printtime __P_((int));
#ifdef	SIGALRM
static int trapalrm __P_((VOID_A));
#endif
#ifndef	_NODOSDRIVE
static int wrap_intrkey __P_((VOID_A));
#endif
static char *NEAR getversion __P_((int *));
#ifndef	_NOLOGGING
static VOID NEAR startlog __P_((char *CONST *));
static VOID NEAR endlog __P_((int));
#endif
#ifdef	_NOORIGSHELL
static int NEAR execruncomline __P_((char *, CONST char *, int, CONST char *));
#endif
static int NEAR initoption __P_((int, char *CONST []));
static int NEAR evaloption __P_((char *CONST []));
static char *NEAR searchenv __P_((CONST char *, char *CONST []));
#if	!MSDOS
static char *NEAR searchexecname __P_((CONST char *, char *CONST []));
#endif
static VOID NEAR setexecname __P_((CONST char *));
static VOID NEAR setexecpath __P_((CONST char *, char *CONST []));
int main __P_((int, char *CONST [], char *CONST []));

char *origpath = NULL;
char *progpath = NULL;
char *progname = NULL;
char *tmpfilename = NULL;
#if	!MSDOS
int adjtty = 0;
#endif
int showsecond = 0;
#if	FD >= 2
int thruargs = 0;
#endif
int hideclock = 0;
#ifdef	SIGALRM
int noalrm = 1;
#endif
int fd_restricted = 0;
#ifndef	_NOCUSTOMIZE
char **orighelpindex = NULL;
bindtable *origbindlist = NULL;
# ifndef	_NOKEYMAP
keyseq_t *origkeymaplist = NULL;
# endif
# ifndef	_NOARCHIVE
launchtable *origlaunchlist = NULL;
int origmaxlaunch = 0;
archivetable *origarchivelist = NULL;
int origmaxarchive = 0;
# endif
# ifdef	_USEDOSEMU
devinfo *origfdtype = NULL;
# endif
#endif	/* !_NOCUSTOMIZE */
int inruncom = 0;
#ifndef	_NOORIGSHELL
int fdmode = 0;
#endif

static int timersec = 0;
#ifdef	SIGWINCH
static int nowinch = 0;
#endif


#if	MSDOS && !defined (PROTECTED_MODE)
/*ARGSUSED*/
static harderr_t far criticalerror(deverr, errval, devhdr)
u_int deverr, errval;
u_short far *devhdr;
{
	if ((deverr & 0x8800) == 0x0800) _hardresume(_HARDERR_FAIL);
	_hardresume(_HARDERR_ABORT);
}
#endif

VOID error(s)
CONST char *s;
{
	static int doing = 0;
	int duperrno;

	if (doing == 1) exit2(2);
	if (doing == 2) exit(2);
	duperrno = errno;
	doing = 1;

	sigvecset(0);
	if (isorgpid()) {
		forcecleandir(deftmpdir, tmpfilename);
#ifndef	_NODOSDRIVE
		dosallclose();
#endif
		Xstdiomode();
		endterm();
	}

	if (!s) s = progname;
	if (dumbterm <= 2) fputc('\007', stderr);
	errno = duperrno;
	if (errno) perror2(s);
	else {
		fputs(s, stderr);
		fputnl(stderr);
	}
	fclose(stderr);
	doing = 2;

	if (isorgpid()) {
		inittty(1);
		keyflush();
		prepareexitfd(2);
#ifndef	_NOORIGSHELL
# ifndef	NOJOB
		killjob();
# endif
		prepareexit(0);
#endif	/* !_NOORIGSHELL */
#ifndef	_NOPTY
		killallpty();
#endif
#ifdef	DEBUG
# if	!MSDOS
		freeterment();
# endif
		closetty(&ttyio, &ttyout);
		muntrace();
#endif	/* DEBUG */
	}

	exit(2);
}

#ifdef	_NOORIGSHELL
static VOID NEAR Xexit2(n)
int n;
{
	prepareexitfd(n);
	exit2(n);
}
#endif	/* _NOORIGSHELL */

static VOID NEAR signalexit(sig)
int sig;
{
	signal2(sig, SIG_IGN);

	if (isorgpid()) {
		forcecleandir(deftmpdir, tmpfilename);
#ifndef	_NODOSDRIVE
		dosallclose();
#endif
#ifndef	_NOLOGGING
		endlog(sig + 128);
#endif
		endterm();
		inittty(1);
		keyflush();
#ifndef	_NOORIGSHELL
# if	defined (SIGHUP) && !defined (NOJOB)
		if (sig == SIGHUP) killjob();
# endif
		prepareexit(0);
#endif	/* !_NOORIGSHELL */
#ifndef	_NOPTY
		killallpty();
#endif
#ifdef	DEBUG
# if	!MSDOS
		freeterment();
# endif
		closetty(&ttyio, &ttyout);
		muntrace();
#endif	/* DEBUG */
	}

	signal2(sig, SIG_DFL);
	VOID_C kill(getpid(), sig);
}

#ifdef	SIGALRM
static int ignore_alrm(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal2(SIGALRM, (sigcst_t)ignore_alrm);
	errno = duperrno;

	return(0);
}
#endif	/* SIGALRM */

#ifdef	SIGWINCH
static int ignore_winch(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal2(SIGWINCH, (sigcst_t)ignore_winch);
	errno = duperrno;

	return(0);
}
#endif	/* SIGWINCH */

#ifdef	SIGINT
static int ignore_int(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal2(SIGINT, (sigcst_t)ignore_int);
	errno = duperrno;

	return(0);
}
#endif	/* SIGINT */

#ifdef	SIGQUIT
static int ignore_quit(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal2(SIGQUIT, (sigcst_t)ignore_quit);
	errno = duperrno;

	return(0);
}
#endif	/* SIGQUIT */

#ifdef	SIGCONT
static int ignore_cont(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal2(SIGCONT, (sigcst_t)ignore_cont);
# if	!MSDOS
	suspended = 1;
#  ifndef	_NOORIGSHELL
	sigconted = 1;
#  endif
# endif	/* !MSDOS */
	errno = duperrno;

	return(0);
}
#endif	/* SIGCONT */

#ifdef	SIGHUP
static int hanguperror(VOID_A)
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

VOID setlinecol(VOID_A)
{
	char buf[MAXLONGWIDTH + 1];

	if (getconstvar(ENVLINES)) {
		snprintf2(buf, sizeof(buf), "%d", n_line);
		setenv2(ENVLINES, buf, 1);
	}
	if (getconstvar(ENVCOLUMNS)) {
		snprintf2(buf, sizeof(buf), "%d", n_column);
		setenv2(ENVCOLUMNS, buf, 1);
	}
}

static int NEAR minrow(n)
int n;
{
	if (n == win) {
#ifndef	_NOTREE
		if (treepath) return(WFILEMINTREE);
#endif
#ifndef	_NOCUSTOMIZE
		if (custno >= 0) return(WFILEMINCUSTOM);
#endif
	}

	return(WFILEMIN);
}

#ifndef	_NOEXTRAWIN
static int NEAR biaswsize(n, max)
int n, max;
{
	int i, row, min;

	for (i = row = 0; i < n; i++) row += winvar[i].v_fileperrow + 1;
	min = minrow(n);
	if (max - row < min) {
		winvar[n].v_fileperrow = min;
		return(max - min);
	}
	winvar[n].v_fileperrow = max - row;

	return(0);
}
#endif	/* !_NOEXTRAWIN */

VOID checkscreen(xmax, ymax)
int xmax, ymax;
{
	CONST char *cp;
	char *tty;
	int i, row, wastty, dupn_line, dupdumbterm;

	if (ttyio < 0) return;
#ifdef	SIGWINCH
	nowinch++;
#endif
	dupn_line = -1;
#ifndef	_NOPTY
	if (parentfd >= 0) row = WFILEMIN;
	else
#endif
	{
		row = wheader + WFOOTER;
#ifdef	_NOEXTRAWIN
		row += (minrow(win) + 1) * windows;
#else
		for (i = 0; i < windows; i++) row += minrow(i) + 1;
#endif
		row--;
	}

	savetermio(ttyio, &tty, NULL);
	wastty = isttyiomode;
	Xttyiomode(1);
	dupdumbterm = dumbterm;
	if (xmax < 0 || ymax < 0) {
		dupn_line = n_line;
		xmax = WCOLUMNMIN;
		ymax = 0;
		dumbterm = 1;
	}
	for (i = 0;; i++) {
		if (!(cp = getwsize(xmax, ymax))) {
			if (n_line >= row) break;
			cp = NOROW_K;
		}
		if (!i) {
			Xputterms(T_CLEAR);
			Xlocate(0, 0);
			keyflush();
		}

		if (i & 1) Xcputs2(SCRSZ_K);
		else {
			Xputterm(T_STANDOUT);
			Xcputs2(cp);
			Xputterm(END_STANDOUT);
		}
		Xputterm(T_BELL);
		Xcputnl();
		Xtflush();
		if (kbhit2(1000000L) && getkey3(0, inputkcode) == K_ESC) {
			errno = 0;
			error(INTR_K);
		}
	}
	dumbterm = dupdumbterm;
	loadtermio(ttyio, tty, NULL);
	isttyiomode = wastty;
	if (tty) free(tty);

	setlinecol();
	if (n_line != dupn_line) {
#ifdef	_NOEXTRAWIN
		calcwin();
#else
		row = fileperrow(1);
		for (i = windows - 1; i >= 0; i--) {
			if (!(row = biaswsize(i, row))) break;
			row--;
		}
#endif
	}
#ifdef	SIGWINCH
	nowinch--;
#endif
}

#ifdef	SIGWINCH
VOID pollscreen(forced)
int forced;
{
	static int winched = 0;
	int x, y;

	if (forced < 0) nowinch = winched = 0;
	else if (nowinch) {
		if (forced) winched++;
	}
	else if (winched || forced) {
		winched = 0;
		x = n_column;
		y = n_line;
		checkscreen(-1, -1);
		if (isorgpid()) {
			if (x != n_column || y != n_line) rewritefile(1);
			if (subwindow) ungetkey2(K_CTRL('L'));
		}
	}
}

static int wintr(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal2(SIGWINCH, SIG_IGN);
	pollscreen(1);
	signal2(SIGWINCH, (sigcst_t)wintr);
	errno = duperrno;

	return(0);
}
#endif	/* SIGWINCH */

static VOID NEAR printtime(hide)
int hide;
{
	static time_t now;
	struct tm *tm;
	int x;

	if (timersec) now++;
	else {
		now = time2();
		timersec = CLOCKUPDATE;
	}
	if (timersec-- < CLOCKUPDATE && !showsecond) return;
#ifndef	_NOPTY
	if (checkallpty() < 0) rewritefile(0);
	if (isptymode() && !ptyinternal && hide <= 1) hide = 0;
#endif
	if (hide) return;

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
	x = n_column - 15 - ((showsecond) ? 3 : 0);
	if (!isleftshift()) x--;
	Xlocate(x, L_TITLE);
	Xputterm(T_STANDOUT);
	Xcprintf2("%02d-%02d-%02d %02d:%02d",
		tm -> tm_year % 100,
		tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min);
	if (showsecond) Xcprintf2(":%02d", tm -> tm_sec);
	Xputterm(END_STANDOUT);
	Xlocate(win_x, win_y);
	Xtflush();
}

#ifdef	SIGALRM
static int trapalrm(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal2(SIGALRM, SIG_IGN);
# if	!defined (_NOORIGSHELL) && !defined (_NOPTY)
	if (isshptymode()) {
		if (mypid == shellpid) printtime(hideclock);
	}
	else
# endif
	if (isorgpid()) printtime(hideclock);
# ifdef	SIGWINCH
	pollscreen(0);
# endif
	signal2(SIGALRM, (sigcst_t)trapalrm);
	errno = duperrno;

	return(0);
}
#endif	/* SIGALRM */

#ifndef	_NODOSDRIVE
static int wrap_intrkey(VOID_A)
{
	return(intrkey(-1));
}
#endif	/* !_NODOSDRIVE */

int sigvecset(set)
int set;
{
	static int status = -1;
	int old;

	old = status;
	if (set == old) /*EMPTY*/;
	else if (set) {
#ifdef	SIGALRM
		signal2(SIGALRM, (sigcst_t)trapalrm);
		noalrm = 0;
#endif
#ifdef	SIGTSTP
		signal2(SIGTSTP, SIG_IGN);
#endif
#ifdef	SIGWINCH
		signal2(SIGWINCH, (sigcst_t)wintr);
#endif
#ifndef	_NODOSDRIVE
		doswaitfunc = waitmes;
		dosintrfunc = wrap_intrkey;
#endif
		status = 1;
	}
	else {
#ifdef	SIGALRM
		signal2(SIGALRM, (sigcst_t)ignore_alrm);
		noalrm = 1;
#endif
#ifdef	SIGTSTP
		signal2(SIGTSTP, SIG_DFL);
#endif
#ifdef	SIGWINCH
		signal2(SIGWINCH, (sigcst_t)ignore_winch);
#endif
#ifndef	_NODOSDRIVE
		doswaitfunc = NULL;
		dosintrfunc = NULL;
#endif
		status = 0;
	}

	return(old);
}

static char *NEAR getversion(lenp)
int *lenp;
{
	char *cp, *eol;

	cp = strchr(version, ' ');
	while (*(++cp) == ' ');
	if (lenp) {
		if (!(eol = strchr(cp, ' '))) eol = cp + strlen(cp);
		*lenp = eol - cp;
	}

	return(cp);
}

VOID title(VOID_A)
{
	CONST char *cp;
	int n, len;

	Xlocate(0, L_TITLE);
	Xputterm(T_STANDOUT);
	len = 0;
	if (!isleftshift()) {
		Xputch2(' ');
		len++;
	}
	Xcputs2(" FD");
	len += 3;
	if (!ishardomit()) {
		Xcputs2("(File & Directory tool)");
		len += 23;
	}
	Xcputs2(" Ver.");
	len += 5;
	cp = getversion(&n);
	cputstr(n, cp);
	if (distributor) {
		Xputch2('#');
		n++;
	}
	cp = (iswellomit()) ? nullstr : " (c)1995-2008 T.Shirai  ";
	Xcputs2(cp);
	n = n_column - len - strlen2(cp) - n;
	while (n-- > 0) Xputch2(' ');
	Xputterm(END_STANDOUT);
	timersec = 0;
	printtime(0);
}

#ifndef	_NOCUSTOMIZE
VOID saveorigenviron(VOID_A)
{
	orighelpindex = copystrarray(NULL, helpindex, NULL, MAXHELPINDEX);
	origbindlist = copybind(NULL, bindlist);
# ifndef	_NOKEYMAP
	origkeymaplist = copykeyseq(NULL);
# endif
# ifndef	_NOARCHIVE
	origlaunchlist = copylaunch(NULL, launchlist,
		&origmaxlaunch, maxlaunch);
	origarchivelist = copyarch(NULL, archivelist,
		&origmaxarchive, maxarchive);
# endif
# ifdef	_USEDOSEMU
	origfdtype = copydosdrive(NULL, fdtype);
# endif
}
#endif	/* !_NOCUSTOMIZE */

#ifndef	_NOLOGGING
static VOID NEAR startlog(argv)
char *CONST *argv;
{
# ifndef	NOUID
	uid_t uid;
	int lvl;
# endif
	char *cp, *tmp, buf[MAXLOGLEN + 1];
	int i, len;

	cp = buf;
	for (i = 1; argv && argv[i]; i++) {
		if (cp > buf) *(cp++) = ' ';
		tmp = killmeta(argv[i]);
		len = snprintf2(cp, (int)sizeof(buf) - (cp - buf), "%s", tmp);
		free(tmp);
		if (len < 0) break;
		cp += len;
	}
	*cp = '\0';

	cp = getversion(&len);
# ifdef	NOUID
	logmessage(_LOG_DEBUG_, "%s (%-*.*s) starts; PWD=%k; ARGS=%k",
		progname, len, len, cp, origpath, buf);
# else
	lvl = ((uid = getuid())) ? _LOG_DEBUG_ : _LOG_WARNING_;
	logmessage(lvl, "%s (%-*.*s) starts; UID=%d; PWD=%k; ARGS=%k",
		progname, len, len, cp, uid, origpath, buf);
# endif
}

static VOID NEAR endlog(status)
int status;
{
# ifndef	NOUID
	uid_t uid;
	int lvl;
# endif
	char cwd[MAXPATHLEN];

	if (!Xgetwd(cwd)) strcpy(cwd, "?");
# ifdef	NOUID
	logmessage(_LOG_DEBUG_, "%s ends; PWD=%k; STATUS=%d",
		progname, cwd, status);
# else
	lvl = ((uid = getuid())) ? _LOG_DEBUG_ : _LOG_WARNING_;
	logmessage(lvl, "%s ends; UID=%d; PWD=%k; STATUS=%d",
		progname, uid, cwd, status);
# endif
	logclose();
}
#endif	/* !_NOLOGGING */

#ifdef	_NOORIGSHELL
static int NEAR execruncomline(command, file, n, line)
char *command;
CONST char *file;
int n;
CONST char *line;
{
	char *cp;
	int i;

	if (!*(cp = skipspace(command))) i = 0;
	else i = execpseudoshell(cp, F_IGNORELIST | F_NOCOMLINE);
	if (i) {
		Xputterm(L_CLEAR);
		Xcprintf2("%s, line %d: %s", file, n, ILFNC_K);
		Xcputnl();
		Xputterm(L_CLEAR);
		Xcprintf2("\t%s", line);
		Xcputnl();
	}
	free(command);

	return((i) ? -1 : 0);
}
#endif	/* _NOORIGSHELL */

/*ARGSUSED*/
int loadruncom(file, exist)
CONST char *file;
int exist;
{
#ifdef	_NOORIGSHELL
# if	!MSDOS
	struct stat st;
# endif
	FILE *fp;
	char *cp, *tmp, *fold, *line;
	int cont;
#endif	/* _NOORIGSHELL */
	int n, er;

#ifdef	_NOORIGSHELL
# if	!MSDOS
	tmp = NULL;
	if (!exist && (tmp = getconstvar(ENVTERM))) {
		cp = malloc2(strlen(file) + strlen(tmp) + 1 + 1);
		strcpy(strcpy2(strcpy2(cp, file), "."), tmp);
		tmp = evalpath(cp, 0);
		if (stat2(tmp, &st) < 0 || !s_isreg(&st)) {
			free(tmp);
			tmp = NULL;
		}
	}
	if (!tmp)
# endif	/* !MSDOS */
	tmp = evalpath(strdup2(file), 0);
	fp = Xfopen(tmp, "r");
	if (!fp) {
		if (!exist) {
			free(tmp);
			return(0);
		}
		Xcprintf2("%s: Not found", tmp);
		Xcputnl();
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
		for (cp--; cp >= line; cp--) if (!isblank2(*cp)) break;
		cp[1] = '\0';

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

		if (execruncomline(fold, tmp, n, line) < 0) er++;
		if (fold != line) free(line);
		fold = NULL;
	}

	if (fold && execruncomline(fold, tmp, n, line) < 0) er++;
	fclose(fp);
	free(tmp);
#else	/* !_NOORIGSHELL */
	if ((n = isttyiomode)) Xstdiomode();
	er = execruncom(file, 1);
	if (n) Xttyiomode(n - 1);
#endif	/* !_NOORIGSHELL */

	return(er ? -1 : 0);
}

static int NEAR initoption(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp, **optv;
	int i, optc;

	optc = 1;
	optv = (char **)malloc2((argc + 1) * sizeof(char *));
	optv[0] = argv[0];

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '+') /*EMPTY*/;
		else if (argv[i][0] != '-' || !argv[i][1]
		|| (argv[i][1] == '-' && !argv[i][2]))
			break;
		else if (!isidentchar(argv[i][1])) /*EMPTY*/;
		else {
			for (cp = &(argv[i][2]); *cp; cp++)
				if (!isidentchar2(*cp)) break;
			if (*cp == '=') continue;
		}

		optv[optc++] = argv[i];
		memmove((char *)&(argv[i]), (char *)&(argv[i + 1]),
			(argc-- - i) * sizeof(char *));
		i--;
	}
	optv[optc] = NULL;

#ifndef	_NOORIGSHELL
	if (initshell(optc, optv) < 0) Xexit2(RET_FAIL);
#endif
	free(optv);

	return(argc);
}

static int NEAR evaloption(argv)
char *CONST argv[];
{
	char *cp, *tmp;
	int i;

	for (i = 1; argv[i]; i++) {
		if (argv[i][0] != '-') break;
		if (!argv[i][1] || (argv[i][1] == '-' && !argv[i][2])) {
			i++;
			break;
		}
		tmp = strdup2(&(argv[i][1]));
		if ((cp = strchr(tmp, '='))) *(cp++) = '\0';
		setenv2(tmp, cp, 0);
		free(tmp);
	}

	return(i);
}

static char *NEAR searchenv(s, envp)
CONST char *s;
char *CONST envp[];
{
	int i, len;

	len = strlen(s);
	for (i = 0; envp[i]; i++)
		if (!strnenvcmp(envp[i], s, len) && envp[i][len] == '=')
			return(&(envp[i][++len]));

	return(NULL);
}

#if	!MSDOS
static char *NEAR searchexecname(argv, envp)
CONST char *argv;
char *CONST envp[];
{
	CONST char *env;
	char *cp;
	int n;

	n = 0;
	if (!(env = searchenv(ENVPATH, envp))) env = DEFPATH;
	if ((cp = searchexecpath(argv, env))) return(cp);
	if (*argv == '-') {
		if ((cp = searchexecpath(++argv, env))) return(cp);
		n++;
	}
	if (*argv == 'r' && (cp = searchexecpath(++argv, env))) return(cp);
	if ((cp = searchexecpath(FDSHELL, env))) return(cp);
	if ((cp = searchexecpath(FDPROG, env))) return(cp);
	if (n) {
		getlogininfo(NULL, &env);
		cp = strdup2(env);
	}

	return(cp);
}
#endif	/* !MSDOS */

static VOID NEAR setexecname(argv)
CONST char *argv;
{
#if	MSDOS || defined (CYGWIN)
	char *cp;
#endif

	progname = getbasename(argv);
#if	MSDOS || defined (CYGWIN)
	if ((cp = strchr(progname, '.')) && cp > progname)
		progname = strndup2(progname, cp - progname);
	else
#endif
	progname = strdup2(progname);
}

static VOID NEAR setexecpath(argv, envp)
CONST char *argv;
char *CONST envp[];
{
	CONST char *cp;
	char *tmp, buf[MAXPATHLEN];

	if (!Xgetwd(buf)) error(NOCWD_K);
	origpath = strdup2(buf);
	if ((cp = searchenv(ENVPWD, envp))) {
		*fullpath = '\0';
		realpath2(cp, fullpath, 0);
		realpath2(fullpath, buf, 1);
		if (!strpathcmp(origpath, buf)) {
			free(origpath);
			origpath = strdup2(fullpath);
		}
	}
	strcpy(fullpath, origpath);

	tmp = NULL;
#if	MSDOS
	cp = argv;
#else
	if (strdelim(argv, 0)) cp = argv;
	else cp = tmp = searchexecname(argv, envp);
	if (!cp) progpath = strdup2(BINDIR);
	else
#endif
	{
		realpath2(cp, buf, 1);
		if (tmp) free(tmp);
		if ((tmp = strrdelim(buf, 0))) *tmp = '\0';
		progpath = strdup2(buf);
	}
}

/*ARGSUSED*/
VOID initfd(argv)
char *CONST *argv;
{
	int i;

#ifndef	_NOORIGSHELL
	if (!interactive) /*EMPTY*/;
	else
#endif
	{
#if	!MSDOS
		if (adjtty) {
			Xstdiomode();
			inittty(0);
			Xttyiomode(0);
		}
#endif	/* !MSDOS */
		for (i = 0; i < 2; i++) loadhistory(i);
		entryhist(1, origpath, 1);
	}
#ifndef	_NOLOGGING
	startlog(argv);
#endif
}

/*ARGSUSED*/
VOID prepareexitfd(status)
int status;
{
	char cwd[MAXPATHLEN];
	int i;

#ifndef	_NOLOGGING
	endlog(status);
#endif
#ifndef	_NOORIGSHELL
	if (!interactive) /*EMPTY*/;
	else
#endif
	for (i = 0; i < 2; i++) savehistory(i);

	cwd[0] = '\0';
	if (origpath && _chdir2(origpath) < 0) {
		perror2(origpath);
		if (!Xgetwd(cwd)) *cwd = '\0';
		rawchdir(rootpath);
	}
	free(origpath);
	free(progname);
#ifdef	_USEUNICODE
	free(unitblpath);
#endif
#ifndef	_NOIME
	free(dicttblpath);
#endif
#ifndef	_NODOSDRIVE
	dosallclose();
#endif
	free(progpath);

#ifdef	DEBUG
	if (tmpfilename) free(tmpfilename);
# ifdef	_NOORIGSHELL
	freevar(environ);
	freevar(environ2);
# endif
# ifndef	_NOCUSTOMIZE
	freestrarray(orighelpindex, MAXHELPINDEX);
	if (orighelpindex) free(orighelpindex);
	free(origbindlist);
#  ifndef	_NOKEYMAP
	freekeyseq(origkeymaplist);
#  endif
#  ifndef	_NOARCHIVE
	freelaunchlist(origlaunchlist, origmaxlaunch);
	if (origlaunchlist) free(origlaunchlist);
	freearchlist(origarchivelist, origmaxarchive);
	if (origarchivelist) free(origarchivelist);
#  endif
#  ifdef	_USEDOSEMU
	if (origfdtype) {
		freedosdrive(origfdtype);
		free(origfdtype);
	}
#  endif
# endif	/* !_NOCUSTOMIZE */
	freeenvpath();
	freehistory(0);
	freehistory(1);
	freedefine();
# ifndef	NOUID
	freeidlist();
# endif
	chdir2(NULL);
# if	!defined (_NOUSEHASH) && defined (_NOORIGSHELL)
	searchhash(NULL, NULL, NULL);
# endif
# ifndef	_NOROCKRIDGE
	detranspath(NULL, NULL);
# endif
# ifdef	_USEUNICODE
	discardunitable();
# endif
# ifndef	_NOIME
	ime_freebuf();
	freeroman(0);
	discarddicttable();
# endif
#endif	/* DEBUG */
	if (*cwd) rawchdir(cwd);
}

int main(argc, argv, envp)
int argc;
char *CONST argv[], *CONST envp[];
{
	char *cp;
	int i;

#ifdef	DEBUG
	mtrace();
#endif

#if	MSDOS && (defined (DJGPP) || !defined (BSPATHDELIM))
	adjustfname(argv[0]);
#endif
	setexecname(argv[0]);

#if	MSDOS && !defined (PROTECTED_MODE)
	_harderr(criticalerror);
#endif

#ifdef	SIGHUP
	signal2(SIGHUP, (sigcst_t)hanguperror);
#endif
#ifdef	SIGINT
	signal2(SIGINT, (sigcst_t)ignore_int);
#endif
#ifdef	SIGQUIT
	signal2(SIGQUIT, (sigcst_t)ignore_quit);
#endif
#ifdef	SIGCONT
	signal2(SIGCONT, (sigcst_t)ignore_cont);
#endif
#ifdef	SIGILL
	signal2(SIGILL, (sigcst_t)illerror);
#endif
#ifdef	SIGTRAP
	signal2(SIGTRAP, (sigcst_t)traperror);
#endif
#ifdef	SIGIOT
	signal2(SIGIOT, (sigcst_t)ioerror);
#endif
#ifdef	SIGEMT
	signal2(SIGEMT, (sigcst_t)emuerror);
#endif
#ifdef	SIGFPE
	signal2(SIGFPE, (sigcst_t)floaterror);
#endif
#ifdef	SIGBUS
	signal2(SIGBUS, (sigcst_t)buserror);
#endif
#ifdef	SIGSEGV
	signal2(SIGSEGV, (sigcst_t)segerror);
#endif
#ifdef	SIGSYS
	signal2(SIGSYS, (sigcst_t)syserror);
#endif
#ifdef	SIGPIPE
	signal2(SIGPIPE, (sigcst_t)pipeerror);
#endif
#ifdef	SIGTERM
	signal2(SIGTERM, (sigcst_t)terminate);
#endif
#ifdef	SIGXCPU
	signal2(SIGXCPU, (sigcst_t)xcpuerror);
#endif
#ifdef	SIGXFSZ
	signal2(SIGXFSZ, (sigcst_t)xsizerror);
#endif
	sigvecset(0);

#ifndef	_NOARCHIVE
	for (maxlaunch = 0; maxlaunch < MAXLAUNCHTABLE; maxlaunch++) {
		if (!launchlist[maxlaunch].ext) break;
		launchlist[maxlaunch].ext =
			strdup2(launchlist[maxlaunch].ext);
		launchlist[maxlaunch].comm =
			strdup2(launchlist[maxlaunch].comm);
# if	FD >= 2
		launchlist[maxlaunch].format =
			duplvar(launchlist[maxlaunch].format, -1);
		launchlist[maxlaunch].lignore =
			duplvar(launchlist[maxlaunch].lignore, -1);
		launchlist[maxlaunch].lerror =
			duplvar(launchlist[maxlaunch].lerror, -1);
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

	for (i = 0; i < MAXHELPINDEX; i++)
		helpindex[i] = strdup2(helpindex[i]);
#ifdef	_USEDOSEMU
	for (i = 0; fdtype[i].name; i++) {
		fdtype[i].name = strdup2(fdtype[i].name);
# ifdef	HDDMOUNT
		fdtype[i].offset = 0;
# endif
	}
#endif	/* _USEDOSEMU */

#ifdef	_NOORIGSHELL
	for (i = 0; envp[i]; i++);
	environ = (char **)malloc2((i + 1) * sizeof(char *));
	for (i = 0; envp[i]; i++) environ[i] = strdup2(envp[i]);
	environ[i] = NULL;
#endif

	setexecpath(argv[0], envp);
#ifdef	_USEUNICODE
	unitblpath = strdup2(DATADIR);
#endif
#ifndef	_NOIME
	dicttblpath = strdup2(DATADIR);
#endif

#ifdef	_NOORIGSHELL
	inittty(0);
	getterment(NULL);
	argc = initoption(argc, argv);
	adjustpath();
#else	/* !_NOORIGSHELL */
	cp = getshellname(progname, NULL, NULL);
	if (!strpathcmp(cp, FDSHELL) || !strpathcmp(cp, "su")) {
		i = main_shell(argc, argv, envp);
# ifndef	_NOPTY
		killallpty();
# endif
		Xexit2(i);
	}
	interactive = fdmode = 1;
	setshellvar(envp);
	prepareterm();
	argc = initoption(argc, argv);
	if (dumbterm > 1) {
		errno = 0;
		error(NTERM_K);
	}
#endif	/* !_NOORIGSHELL */
	initenv();

	Xttyiomode(0);
	initterm();
	if ((cp = getwsize(WCOLUMNMIN, WHEADERMAX + WFOOTER + WFILEMIN))) {
		errno = 0;
		error(cp);
	}
	setlinecol();
#ifndef	_NOCUSTOMIZE
	saveorigenviron();
#endif

	Xlocate(0, n_line - 1);
	inruncom = 1;
	i = loadruncom(DEFRC, 0);
	i += loadruncom(FD_RCFILE, 0);
	inruncom = 0;
#ifdef	SIGWINCH
	nowinch = 1;
#endif
	sigvecset(1);
	if (i < 0) {
		hideclock = 2;
		warning(0, HITKY_K);
	}

	i = evaloption(argv);
	checkscreen(WCOLUMNMIN, WHEADERMAX + WFOOTER + WFILEMIN);
	initfd(argv);
	Xputterms(T_CLEAR);

#ifdef	SIGWINCH
	nowinch = 0;
#endif
#if	FD >= 2
	main_fd(&(argv[i]), (thruargs) ? 1 : 0);
#else
	main_fd(&(argv[i]), 0);
#endif
	sigvecset(0);

	Xstdiomode();
#if	!defined (_NOORIGSHELL) && !defined (NOJOB)
	killjob();
#endif
#ifndef	_NOPTY
	killallpty();
#endif
	Xexit2(0);

	return(0);
}
