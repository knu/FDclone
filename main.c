/*
 *	FD (File & Directory maintenance tool)
 *
 *	by T.Shirai <shirai@unixusers.net>
 */

#include "fd.h"
#include "time.h"
#include "log.h"
#include "termio.h"
#include "realpath.h"
#include "parse.h"
#include "kconv.h"
#include "func.h"
#include "kanji.h"
#include "version.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	DEP_PTY
#include "termemu.h"
#endif
#ifdef	DEP_IME
#include "roman.h"
#endif
#ifdef	DEP_URLPATH
#include "urldisk.h"
#endif

#if	MSDOS
#include <process.h>
# ifndef	DJGPP
# include <dos.h>
# endif
#endif	/* !MSDOS */

#define	CLOCKUPDATE		10	/* sec */
#define	SWITCHUSER		"su"
#ifndef	BINDIR
#define	BINDIR			"/usr/local/bin"
#endif
#ifndef	DATADIR
#define	DATADIR			progpath
#endif
#ifndef	DEFPATH
#define	DEFPATH			":/bin:/usr/bin"
#endif

#ifdef	DEP_ORIGSHELL
#define	isorgpid()		(mypid == orgpid)
#else
#define	isorgpid()		(1)
#endif

#ifdef	__TURBOC__
extern u_int _stklen = 0x5800;
#define	harderr_t		void
#else
#define	harderr_t		int
#endif

#if	MSDOS
# ifndef	BSPATHDELIM
extern char *adjustpname __P_((char *));
# endif
# if	defined (DJGPP) || !defined (BSPATHDELIM)
extern char *adjustfname __P_((char *));
# endif
#endif	/* MSDOS */

#if	defined (DEP_ORIGSHELL) && !defined (NOJOB)
extern VOID killjob __P_((VOID_A));
#endif

#ifndef	DEP_ORIGSHELL
extern char **environ;
extern char **environ2;
#endif
extern bindlist_t bindlist;
extern int maxbind;
#if	defined (DEP_DYNAMICLIST) || !defined (_NOCUSTOMIZE)
extern origbindlist_t origbindlist;
extern int origmaxbind;
#endif
#ifndef	_NOARCHIVE
extern launchlist_t launchlist;
extern int maxlaunch;
extern archivelist_t archivelist;
extern int maxarchive;
# if	defined (DEP_DYNAMICLIST) || !defined (_NOCUSTOMIZE)
extern origlaunchlist_t origlaunchlist;
extern int origmaxlaunch;
extern origarchivelist_t origarchivelist;
extern int origmaxarchive;
# endif
#endif	/* !_NOARCHIVE */
extern helpindex_t helpindex;
#if	defined (DEP_DYNAMICLIST) || !defined (_NOCUSTOMIZE)
extern orighelpindex_t orighelpindex;
#endif
extern int wheader;
extern char fullpath[];
extern short savehist[];
extern int subwindow;
extern int win_x;
extern int win_y;
#ifndef	_NOCUSTOMIZE
extern int custno;
#endif
extern char *deftmpdir;
#ifdef	DEP_IME
extern char *dicttblpath;
#endif

#if	MSDOS && !defined (PROTECTED_MODE)
static harderr_t far criticalerror __P_((u_int, u_int, u_short far *));
#endif
#ifndef	DEP_ORIGSHELL
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
#ifdef	DEP_DOSDRIVE
static int dos_intrkey __P_((VOID_A));
#endif
static int read_intrkey __P_((VOID_A));
#ifdef	DEP_LOGGING
static VOID NEAR startlog __P_((char *CONST *));
static VOID NEAR endlog __P_((int));
#endif
#ifndef	DEP_ORIGSHELL
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
int hideclock = 0;
#ifdef	SIGALRM
int noalrm = 1;
#endif
int fd_restricted = 0;
#if	!defined (_NOCUSTOMIZE) && !defined (_NOKEYMAP)
keyseq_t *origkeymaplist = NULL;
#endif
int inruncom = 0;
#ifdef	DEP_ORIGSHELL
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
#ifdef	DEP_DOSDRIVE
		VOID_C dosallclose();
#endif
#ifdef	DEP_URLPATH
		VOID_C urlallclose();
#endif
		Xstdiomode();
		endterm();
	}

	if (!s) s = progname;
	if (dumbterm <= 2) Xfputc('\007', Xstderr);
	errno = duperrno;
	if (errno) perror2(s);
	else {
		Xfputs(s, Xstderr);
		VOID_C fputnl(Xstderr);
	}
	VOID_C Xfclose(Xstderr);
	doing = 2;

	if (isorgpid()) {
		inittty(1);
		keyflush();
		prepareexitfd(2);
#ifdef	DEP_ORIGSHELL
# ifndef	NOJOB
		killjob();
# endif
		prepareexit(0);
#endif	/* DEP_ORIGSHELL */
#ifdef	DEP_PTY
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

#ifndef	DEP_ORIGSHELL
static VOID NEAR Xexit2(n)
int n;
{
	prepareexitfd(n);
	exit2(n);
}
#endif	/* !DEP_ORIGSHELL */

static VOID NEAR signalexit(sig)
int sig;
{
	signal2(sig, SIG_IGN);

	if (isorgpid()) {
		printf_urgent++;
		forcecleandir(deftmpdir, tmpfilename);
#ifdef	DEP_DOSDRIVE
		VOID_C dosallclose();
#endif
#ifdef	DEP_URLPATH
		VOID_C urlallclose();
#endif
#ifdef	DEP_LOGGING
		endlog(sig + 128);
#endif
		endterm();
		inittty(1);
		keyflush();
#ifdef	DEP_ORIGSHELL
# if	defined (SIGHUP) && !defined (NOJOB)
		if (sig == SIGHUP) killjob();
# endif
		prepareexit(0);
#endif	/* DEP_ORIGSHELL */
#ifdef	DEP_PTY
		killallpty();
#endif
#ifdef	DEBUG
# if	!MSDOS
		freeterment();
# endif
		closetty(&ttyio, &ttyout);
		muntrace();
#endif	/* DEBUG */
		printf_urgent--;
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
#  ifdef	DEP_ORIGSHELL
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
		VOID_C Xsnprintf(buf, sizeof(buf), "%d", n_line);
		setenv2(ENVLINES, buf, 1);
	}
	if (getconstvar(ENVCOLUMNS)) {
		VOID_C Xsnprintf(buf, sizeof(buf), "%d", n_column);
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
#ifdef	DEP_PTY
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

		if (i & 1) XXcputs(SCRSZ_K);
		else {
			Xputterm(T_STANDOUT);
			XXcputs(cp);
			Xputterm(END_STANDOUT);
		}
		Xputterm(T_BELL);
		Xcputnl();
		Xtflush();
		if (kbhit2(1000000L) && getkey3(0, inputkcode, 0) == K_ESC) {
			errno = 0;
			error(INTR_K);
		}
	}
	dumbterm = dupdumbterm;
	loadtermio(ttyio, tty, NULL);
	isttyiomode = wastty;
	Xfree(tty);

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
			if (subwindow) VOID_C ungetkey2(K_CTRL('L'), 1);
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
		now = Xtime(NULL);
		timersec = CLOCKUPDATE;
	}
	if (timersec-- < CLOCKUPDATE && !showsecond) return;
#ifdef	DEP_PTY
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
	VOID_C XXcprintf("%02d-%02d-%02d %02d:%02d",
		tm -> tm_year % 100,
		tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min);
	if (showsecond) VOID_C XXcprintf(":%02d", tm -> tm_sec);
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
# if	defined (DEP_ORIGSHELL) && defined (DEP_PTY)
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

#ifdef	DEP_DOSDRIVE
static int dos_intrkey(VOID_A)
{
	return(intrkey(-1));
}
#endif

static int read_intrkey(VOID_A)
{
	if (isttyiomode) return(intrkey(K_ESC));

	return(0);
}

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
#ifdef	DEP_DOSDRIVE
		doswaitfunc = waitmes;
		dosintrfunc = dos_intrkey;
#endif
#ifdef	DEP_LSPARSE
		lsintrfunc = read_intrkey;
#endif
#ifndef	NOSELECT
		readintrfunc = read_intrkey;
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
#ifdef	DEP_DOSDRIVE
		doswaitfunc = NULL;
		dosintrfunc = NULL;
#endif
		status = 0;
	}

	return(old);
}

char *getversion(lenp)
int *lenp;
{
	char *cp, *eol;

	cp = Xstrchr(version, ' ');
	while (*(++cp) == ' ');
	if (lenp) {
		if (!(eol = Xstrchr(cp, ' '))) eol = cp + strlen(cp);
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
		VOID_C XXputch(' ');
		len++;
	}
	XXcputs(" FD");
	len += 3;
	if (!ishardomit()) {
		XXcputs("(File & Directory tool)");
		len += 23;
	}
	XXcputs(" Ver.");
	len += 5;
	cp = getversion(&n);
	cputstr(n, cp);
	if (distributor) {
		VOID_C XXputch('#');
		n++;
	}
	cp = (iswellomit()) ? nullstr : " (c)1995-2010 T.Shirai  ";
	XXcputs(cp);
	n = n_column - len - strlen2(cp) - n;
	while (n-- > 0) VOID_C XXputch(' ');
	Xputterm(END_STANDOUT);
	timersec = 0;
	printtime(0);
}

#ifndef	_NOCUSTOMIZE
VOID saveorigenviron(VOID_A)
{
# if	FD >= 3
	char *cp;
	int n;
# endif

# ifndef	_NOKEYMAP
	origkeymaplist = copykeyseq(NULL);
# endif
# ifndef	DEP_DYNAMICLIST
	orighelpindex = copystrarray(NULL, helpindex, NULL, MAXHELPINDEX);
	origbindlist = copybind(NULL, bindlist, &origmaxbind, maxbind);
#  ifndef	_NOARCHIVE
	origlaunchlist = copylaunch(NULL, launchlist,
		&origmaxlaunch, maxlaunch);
	origarchivelist = copyarch(NULL, archivelist,
		&origmaxarchive, maxarchive);
#  endif
#  ifdef	DEP_DOSEMU
	origfdtype = copydosdrive(NULL, fdtype, &origmaxfdtype, maxfdtype);
#  endif
# endif	/* !DEP_DYNAMICLIST */

# if	FD >= 3
	cp = getversion(&n);
	cp = Xstrndup(cp, n);
	setenv2(FDVERSION, cp, 0);
	Xfree(cp);
# endif
}
#endif	/* !_NOCUSTOMIZE */

#ifdef	DEP_LOGGING
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
		len = Xsnprintf(cp, (int)sizeof(buf) - (cp - buf), "%s", tmp);
		Xfree(tmp);
		if (len < 0) break;
#ifdef	CODEEUC
		len = strlen(cp);
#endif
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
		progname, len, len, cp, (int)uid, origpath, buf);
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

	if (!Xgetwd(cwd)) Xstrcpy(cwd, "?");
# ifdef	NOUID
	logmessage(_LOG_DEBUG_, "%s ends; PWD=%k; STATUS=%d",
		progname, cwd, status);
# else
	lvl = ((uid = getuid())) ? _LOG_DEBUG_ : _LOG_WARNING_;
	logmessage(lvl, "%s ends; UID=%d; PWD=%k; STATUS=%d",
		progname, (int)uid, cwd, status);
# endif
	logclose();
}
#endif	/* DEP_LOGGING */

#ifndef	DEP_ORIGSHELL
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
		VOID_C XXcprintf("%s, line %d: %s", file, n, ILFNC_K);
		Xcputnl();
		Xputterm(L_CLEAR);
		VOID_C XXcprintf("\t%s", line);
		Xcputnl();
	}
	Xfree(command);

	return((i) ? -1 : 0);
}
#endif	/* !DEP_ORIGSHELL */

/*ARGSUSED*/
int loadruncom(file, exist)
CONST char *file;
int exist;
{
#ifndef	DEP_ORIGSHELL
# if	!MSDOS
	struct stat st;
# endif
	XFILE *fp;
	char *cp, *tmp, *fold, *line;
	int cont;
#endif	/* !DEP_ORIGSHELL */
	int n, er;

#ifdef	DEP_ORIGSHELL
	if ((n = isttyiomode)) Xstdiomode();
	er = execruncom(file, 1);
	if (n) Xttyiomode(n - 1);
#else	/* !DEP_ORIGSHELL */
# if	!MSDOS
	tmp = NULL;
	if (!exist && (tmp = getconstvar(ENVTERM))) {
		cp = Xmalloc(strlen(file) + strlen(tmp) + 1 + 1);
		Xstrcpy(Xstrcpy(Xstrcpy(cp, file), "."), tmp);
		tmp = evalpath(cp, 0);
		if (stat2(tmp, &st) < 0 || !s_isreg(&st)) {
			Xfree(tmp);
			tmp = NULL;
		}
	}
	if (!tmp)
# endif	/* !MSDOS */
	tmp = evalpath(Xstrdup(file), 0);
	fp = Xfopen(tmp, "r");
	if (!fp) {
		if (!exist) {
			Xfree(tmp);
			return(0);
		}
		VOID_C XXcprintf("%s: Not found", tmp);
		Xcputnl();
		Xfree(tmp);
		return(-1);
	}

	fold = NULL;
	n = er = 0;
	while ((line = Xfgets(fp))) {
		n++;
		if (*line == ';' || *line == '#') {
			Xfree(line);
			continue;
		}

		cp = line + strlen(line);
		for (cp--; cp >= line; cp--) if (!Xisblank(*cp)) break;
		cp[1] = '\0';

		cont = 0;
		if (cp >= line && *cp == ESCAPE
		&& (cp - 1 < line || *(cp - 1) != ESCAPE)
		&& !onkanji1(line, cp - line - 1)) {
			*cp = '\0';
			cont = 1;
		}

		if (!fold) fold = line;
		else if (*line) {
			fold = Xrealloc(fold, strlen(fold) + strlen(line) + 1);
			strcat(fold, line);
		}

		if (cont) {
			if (fold != line) Xfree(line);
			continue;
		}

		if (execruncomline(fold, tmp, n, line) < 0) er++;
		if (fold != line) Xfree(line);
		fold = NULL;
	}

	if (fold && execruncomline(fold, tmp, n, line) < 0) er++;
	VOID_C Xfclose(fp);
	Xfree(tmp);
#endif	/* !DEP_ORIGSHELL */

	return(er ? -1 : 0);
}

static int NEAR initoption(argc, argv)
int argc;
char *CONST argv[];
{
	char *cp, **optv;
	int i, optc;

	optc = 1;
	optv = (char **)Xmalloc((argc + 1) * sizeof(char *));
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

#ifdef	DEP_ORIGSHELL
	if (initshell(optc, optv) < 0) Xexit2(RET_FAIL);
#endif
	Xfree(optv);

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
		tmp = Xstrdup(&(argv[i][1]));
		if ((cp = Xstrchr(tmp, '='))) *(cp++) = '\0';
		setenv2(tmp, cp, 0);
		Xfree(tmp);
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
	CONST char *s, *env;
	char *cp;
	int n;

	n = 0;
	if (!(env = searchenv(ENVPATH, envp))) env = DEFPATH;
	s = progname;
	if (*s == '-') s++;
	if (strpathcmp(s, SWITCHUSER)) {
		if ((cp = searchexecpath(argv, env))) return(cp);
		if (*argv == '-') {
			if ((cp = searchexecpath(++argv, env))) return(cp);
			n++;
		}
		if (*argv == 'r' && (cp = searchexecpath(++argv, env)))
			return(cp);
		if ((cp = searchexecpath(FDSHELL, env))) return(cp);
		if ((cp = searchexecpath(FDPROG, env))) return(cp);

		if (!n) return(NULL);
	}

	getlogininfo(NULL, &s);
	return(Xstrdup(s));
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
	if ((cp = Xstrchr(progname, '.')) && cp > progname)
		progname = Xstrndup(progname, cp - progname);
	else
#endif
	progname = Xstrdup(progname);
}

static VOID NEAR setexecpath(argv, envp)
CONST char *argv;
char *CONST envp[];
{
	CONST char *cp;
	char *tmp, buf[MAXPATHLEN];

	if (!Xgetwd(buf)) {
		errno = 0;
		error("No current working directory.");
	}
	origpath = Xstrdup(buf);
	if ((cp = searchenv(ENVPWD, envp))) {
		*fullpath = '\0';
		Xrealpath(cp, fullpath, 0);
		Xrealpath(fullpath, buf, RLP_READLINK);
		if (!strpathcmp(origpath, buf)) {
			Xfree(origpath);
			origpath = Xstrdup(fullpath);
		}
	}
	Xstrcpy(fullpath, origpath);

	tmp = NULL;
#if	MSDOS
	cp = argv;
#else
	if (strdelim(argv, 0)) cp = argv;
	else cp = tmp = searchexecname(argv, envp);
	if (!cp) progpath = Xstrdup(BINDIR);
	else
#endif
	{
		Xrealpath(cp, buf, RLP_READLINK);
		Xfree(tmp);
		if ((tmp = strrdelim(buf, 0))) *tmp = '\0';
		progpath = Xstrdup(buf);
	}
}

/*ARGSUSED*/
VOID initfd(argv)
char *CONST *argv;
{
	int i;

#ifdef	DEP_ORIGSHELL
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
		entryhist(origpath, HST_PATH | HST_UNIQ);
	}
#ifdef	DEP_LOGGING
	startlog(argv);
#endif
}

/*ARGSUSED*/
VOID prepareexitfd(status)
int status;
{
	char cwd[MAXPATHLEN];
	int i;

#ifdef	DEP_LOGGING
	endlog(status);
#endif
#ifdef	DEP_ORIGSHELL
	if (!interactive) /*EMPTY*/;
	else
#endif
	for (i = 0; i < 2; i++) savehistory(i);

	cwd[0] = '\0';
	if (origpath && _chdir2(origpath) < 0) {
		perror2(origpath);
		if (!Xgetwd(cwd)) *cwd = '\0';
		VOID_C rawchdir(rootpath);
	}
	Xfree(origpath);
	Xfree(progname);
#ifdef	DEP_UNICODE
	Xfree(unitblpath);
#endif
#ifdef	DEP_IME
	Xfree(dicttblpath);
#endif
#ifndef	_NOCATALOG
	Xfree(cattblpath);
#endif
#ifdef	DEP_DOSDRIVE
	VOID_C dosallclose();
#endif
#ifdef	DEP_URLPATH
	VOID_C urlallclose();
#endif
	Xfree(progpath);

#ifdef	DEBUG
	Xfree(tmpfilename);
	tmpfilename = NULL;
# ifndef	DEP_ORIGSHELL
	freevar(environ);
	freevar(environ2);
# endif
# ifndef	_NOCUSTOMIZE
#  ifndef	_NOKEYMAP
	freekeyseq(origkeymaplist);
#  endif
#  ifndef	DEP_DYNAMICLIST
	freestrarray(orighelpindex, MAXHELPINDEX);
	Xfree(orighelpindex);
	Xfree(origbindlist);
#   ifndef	_NOARCHIVE
	freelaunchlist(origlaunchlist, origmaxlaunch);
	Xfree(origlaunchlist);
	freearchlist(origarchivelist, origmaxarchive);
	Xfree(origarchivelist);
#   endif
#   ifdef	DEP_DOSEMU
	freedosdrive(origfdtype, origmaxfdtype);
	Xfree(origfdtype);
#   endif
#  endif	/* !DEP_DYNAMICLIST */
# endif	/* !_NOCUSTOMIZE */
	freeenvpath();
	freehistory(0);
	freehistory(1);
	freedefine();
# ifndef	NOUID
	freeidlist();
# endif
	chdir2(NULL);
# if	!defined (_NOUSEHASH) && !defined (DEP_ORIGSHELL)
	searchhash(NULL, NULL, NULL);
# endif
# ifndef	_NOROCKRIDGE
	detranspath(NULL, NULL);
# endif
# ifdef	DEP_UNICODE
	discardunitable();
# endif
# ifdef	DEP_IME
	ime_freebuf();
	freeroman(0);
	discarddicttable();
# endif
#endif	/* DEBUG */
#ifndef	_NOCATALOG
	freecatalog();
#endif
	if (*cwd) VOID_C rawchdir(cwd);
}

int main(argc, argv, envp)
int argc;
char *CONST argv[], *CONST envp[];
{
	char *cp;
	int i;

#ifndef	NOFLOCK
	stream_isnfsfunc = isnfs;
#endif
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

#ifdef	DEP_DYNAMICLIST
	helpindex = copystrarray(NULL, orighelpindex, NULL, MAXHELPINDEX);
	for (i = 0; origbindlist[i].key >= 0; i++) /*EMPTY*/;
	origmaxbind = i;
	bindlist = copybind(NULL, origbindlist, &maxbind, origmaxbind);
# ifndef	_NOARCHIVE
	for (i = 0; origlaunchlist[i].ext; i++) /*EMPTY*/;
	origmaxlaunch = i;
	launchlist = copylaunch(NULL, origlaunchlist,
		&maxlaunch, origmaxlaunch);
	for (i = 0; origarchivelist[i].ext; i++) /*EMPTY*/;
	origmaxarchive = i;
	archivelist = copyarch(NULL, origarchivelist,
		&maxarchive, origmaxarchive);
# endif
# ifdef	DEP_DOSEMU
	for (i = 0; origfdtype[i].name; i++) /*EMPTY*/;
	origmaxfdtype = i;
	fdtype = copydosdrive(NULL, origfdtype, &maxfdtype, origmaxfdtype);
# endif
#else	/* !DEP_DYNAMICLIST */
	copystrarray(helpindex, helpindex, NULL, MAXHELPINDEX);
	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) /*EMPTY*/;
	maxbind = i;
# ifndef	_NOARCHIVE
	for (i = 0; i < MAXLAUNCHTABLE && launchlist[i].ext; i++) /*EMPTY*/;
	maxlaunch = i;
	copylaunch(launchlist, launchlist, &maxlaunch, maxlaunch);
	for (i = 0; i < MAXARCHIVETABLE && archivelist[i].ext; i++) /*EMPTY*/;
	maxarchive = i;
	copyarch(archivelist, archivelist, &maxarchive, maxarchive);
# endif
# ifdef	DEP_DOSEMU
	for (i = 0; i < MAXDRIVEENTRY && fdtype[i].name; i++) /*EMPTY*/;
	maxfdtype = i;
	copydosdrive(fdtype, fdtype, &maxfdtype, maxfdtype);
# endif
#endif	/* !DEP_DYNAMICLIST */

#ifndef	DEP_ORIGSHELL
	i = countvar(envp);
	environ = (char **)Xmalloc((i + 1) * sizeof(char *));
	for (i = 0; envp[i]; i++) environ[i] = Xstrdup(envp[i]);
	environ[i] = NULL;
#endif

	setexecpath(argv[0], envp);
#ifdef	DEP_UNICODE
	unitblpath = Xstrdup(DATADIR);
#endif
#ifdef	DEP_IME
	dicttblpath = Xstrdup(DATADIR);
#endif
#ifndef	_NOCATALOG
# ifdef	USEDATADIR
	cp = getversion(&i);
	cattblpath = Xmalloc(strsize(DATADIR) + 1 + i + 1);
	VOID_C Xsnprintf(cattblpath, strsize(DATADIR) + 1 + i + 1,
		"%s%c%-.*s", DATADIR, _SC_, i, cp);
# else
	cattblpath = Xstrdup(DATADIR);
# endif
#endif	/* !_NOCATALOG */

#ifdef	DEP_ORIGSHELL
	cp = getshellname(progname, NULL, NULL);
	if (!strpathcmp(cp, FDSHELL) || !strpathcmp(cp, SWITCHUSER)) {
		i = main_shell(argc, argv, envp);
# ifdef	DEP_PTY
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
#else	/* !DEP_ORIGSHELL */
	inittty(0);
	getterment(NULL);
	argc = initoption(argc, argv);
	adjustpath();
#endif	/* !DEP_ORIGSHELL */
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
	main_fd(&(argv[i]), 0);
	sigvecset(0);

	Xstdiomode();
#if	defined (DEP_ORIGSHELL) && !defined (NOJOB)
	killjob();
#endif
#ifdef	DEP_PTY
	killallpty();
#endif
	Xexit2(0);

	return(0);
}
