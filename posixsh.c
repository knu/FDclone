/*
 *	posixsh.c
 *
 *	XSH command for POSIX
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include "machine.h"
#include "kctype.h"

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#if	MSDOS
#include <io.h>
#include "unixemu.h"
#define	Xexit2		exit2
#define	DEFPATH		":"
#else	/* !MSDOS */
#include <sys/param.h>
#include <sys/ioctl.h>
#define	Xexit2(n)	((ttypgrp >= 0 && ttypgrp == getpid()) \
			? (VOID)exit2(n) : (VOID)exit(n))
#define	DEFPATH		":/bin:/usr/bin"
# ifdef	USETERMIOS
# include <termios.h>
typedef struct termios	termioctl_t;
# define	tioctl(d, r, a)	((r) \
				? tcsetattr(d, (r) - 1, a) : tcgetattr(d, a))
# define	REQGETP		0
# define	REQSETP		(TCSAFLUSH + 1)
# else	/* !USETERMIOS */
# define	tioctl		ioctl
#  ifdef	USETERMIO
#  include <termio.h>
typedef struct termio	termioctl_t;
#  define	REQGETP		TCGETA
#  define	REQSETP		TCSETAF
#  else	/* !USETERMIO */
#  include <sgtty.h>
typedef struct sgttyb	termioctl_t;
#  define	REQGETP		TIOCGETP
#  define	REQSETP		TIOCSETP
#  endif	/* !USETERMIO */
# endif	/* !USETERMIOS */
#endif	/* !MSDOS */

#ifdef	FD
#include "term.h"
extern int Xaccess __P_((char *, int));
extern int kanjifputs __P_((char *, FILE *));
#else	/* !FD */
extern int ttyio;
#define	Xaccess(p, m)	(access(p, m) ? -1 : 0)
#define	kanjifputs	fputs
# ifdef	DEBUG
# define	exit2(n)	(muntrace(), exit(n))
# else
# define	exit2		exit
# endif
#endif	/* !FD */

#include "pathname.h"
#include "system.h"

#if	!defined (_NOORIGSHELL) && !defined (MINIMUMSHELL) \
&& (!defined (FD) || (FD >= 2))

#ifndef	EPERM
#define	EPERM		EACCES
#endif

extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strdup2 __P_((char *));
extern char *strncpy2 __P_((char *, char *, int));
extern char *ascnumeric __P_((char *, long, int, int));
extern int setenv2 __P_((char *, char *));

#if	!MSDOS && !defined (NOJOB)
static char *NEAR headstree __P_((syntaxtree *));
#endif
#ifndef	NOALIAS
static aliastable *NEAR duplalias __P_((aliastable *));
static int cmpalias __P_((CONST VOID_P, CONST VOID_P));
#endif
static int NEAR _evalexpression __P_((char *, int, long *));
static int NEAR evalexpression __P_((char *, int, long *, int));
#if	!MSDOS
static VOID NEAR addmailpath __P_((char *, char *, time_t));
#endif

#define	ALIASDELIMIT	"();&|<>"
#define	BUFUNIT		32
#define	getconstvar(s)	(getshellvar(s, sizeof(s) - 1))
#define	ER_COMNOFOUND	1
#define	ER_NOTFOUND	2
#define	ER_BADOPTIONS	13
#define	ER_MISSARG	15
#define	ER_NOTALIAS	19
#define	ER_NOSUCHJOB	20
#define	ER_NUMOUTRANGE	21
#define	ER_UNKNOWNSIG	22

#if	!MSDOS
typedef struct _mailpath_t {
	char *path;
	char *msg;
	time_t mtime;
} mailpath_t;
#endif

#if	!MSDOS && !defined (NOJOB)
int gettermio __P_((long));
VOID dispjob __P_((int, FILE *));
int searchjob __P_((long, int *));
int getjob __P_((char *));
int stackjob __P_((long, int, syntaxtree *));
int stoppedjob __P_((long));
VOID killjob __P_((VOID_A));
VOID checkjob __P_((int));
#endif	/* !MSDOS && !NOJOB */
char *evalposixsubst __P_((char *, int *));
#if	!MSDOS
VOID replacemailpath __P_((char *, int));
VOID checkmail __P_((int));
#endif
#ifndef	NOALIAS
VOID freealias __P_((aliastable *));
int checkalias __P_((syntaxtree *, char *, int, int));
#endif
#if	!MSDOS && !defined (NOJOB)
int posixjobs __P_((syntaxtree *));
int posixfg __P_((syntaxtree *));
int posixbg __P_((syntaxtree *));
#endif
#ifndef	NOALIAS
int posixalias __P_((syntaxtree *));
int posixunalias __P_((syntaxtree *));
#endif
int posixkill __P_((syntaxtree *));
#ifndef	NOPOSIXUTIL
int posixcommand __P_((syntaxtree *));
int posixgetopts __P_((syntaxtree *));
#endif

#if	!MSDOS && !defined (NOJOB)
jobtable *joblist = NULL;
int maxjobs = 0;
#endif
#if	!MSDOS
int mailcheck = 0;
#endif
#ifndef	NOALIAS
aliastable *shellalias = NULL;
#endif
#ifndef	NOPOSIXUTIL
int posixoptind = 0;
#endif

#if	!MSDOS
static mailpath_t *mailpathlist = NULL;
static int maxmailpath = 0;
#endif


#if	!MSDOS && !defined (NOJOB)
int gettermio(pgrp)
long pgrp;
{
	int ret;
	sigmask_t mask, omask;

	if (!jobok || pgrp < 0 || pgrp == ttypgrp) return(0);
	Xsigemptyset(mask);
#ifdef	SIGTSTP
	Xsigaddset(mask, SIGTSTP);
#endif
#ifdef	SIGCHLD
	Xsigaddset(mask, SIGCHLD);
#endif
#ifdef	SIGTTIN
	Xsigaddset(mask, SIGTTIN);
#endif
#ifdef	SIGTTOU
	Xsigaddset(mask, SIGTTOU);
#endif
	Xsigblock(omask, mask);

#ifdef	JOBVERBOSE
	fputs("gettermio: ", ttyout);
	fputlong(mypid, ttyout);
	fputs(": ", ttyout);
	fputlong(ttypgrp, ttyout);
	fputs(" -> ", ttyout);
	fputlong(pgrp, ttyout);
	fputc('\n', ttyout);
#endif	/* JOBVERBOSE */
	if ((ret = settcpgrp(ttyio, pgrp)) >= 0) ttypgrp = pgrp;
	Xsigsetmask(omask);
	return(ret);
}

static char *NEAR headstree(trp)
syntaxtree *trp;
{
	int id;

	if (!trp) return(NULL);
	if (trp -> flags & ST_NODE) return("{");
	if (!(trp -> comm)) return(NULL);
	if (!isstatement(trp -> comm)) return((trp -> comm) -> argv[0]);

	id = (trp -> comm) -> id;
	if (id != SM_STATEMENT) return(NULL);
	trp = statementbody(trp);
	if (!trp || !(trp -> comm)) return(NULL);
	id = (trp -> comm) -> id;
	if (id == SM_CHILD || id == SM_STATEMENT) return(NULL);
	return(statementlist[id - 1].ident);
}

VOID dispjob(n, fp)
int n;
FILE *fp;
{
	int i, sig;

	if (n < 0 || n >= maxjobs || !(joblist[n].pids)) return;
	fputc('[', fp);
	fputlong(n + 1, fp);
	fputc(']', fp);
	if (n == lastjob) fputc('+', fp);
	else if (n == prevjob) fputc('-', fp);
	else fputc(' ', fp);
	fputc(' ', fp);
	i = joblist[n].npipe;
	fputlong(joblist[n].pids[i], fp);
	fputc(' ', fp);
	sig = joblist[n].stats[i];

	if (!sig) fputstr("Running", 28, fp);
	else if (sig < 0) fputstr("Done", 28, fp);
	else {
		if (sig >= 128) sig -= 128;
		dispsignal(sig, 28, fp);
	}
	if (joblist[n].trp) {
		if (!isopbg(joblist[n].trp)) joblist[n].trp -> type = OP_NONE;
		printstree(joblist[n].trp, -1, fp);
	}
	fputc('\n', fp);
	fflush(fp);
}

int searchjob(pid, np)
long pid;
int *np;
{
	int i, j;

	for (i = 0; i < maxjobs; i++) {
		if (!(joblist[i].pids)) continue;
		for (j = 0; j <= joblist[i].npipe; j++) {
			if (joblist[i].pids[j] != pid) continue;
			if (np) *np = j;
			return(i);
		}
	}
	return(-1);
}

int getjob(s)
char *s;
{
	char *cp;
	int i, j;

	if (!jobok) return(-1);
	if (!s) i = lastjob;
	else {
		if (s[0] != '%') return(-1);
		if (!s[1] || ((s[1] == '%' || s[1] == '+') && !s[2]))
			i = lastjob;
		else if (s[1] == '-' && !s[2]) i = prevjob;
		else if ((i = isnumeric(&(s[1]))) >= 0) i--;
		else {
			j = strlen(&(s[1]));
			for (i = 0; i < maxjobs; i++) {
				if (!(joblist[i].pids) || !(joblist[i].trp)
				|| !(cp = headstree(joblist[i].trp)))
					continue;
				if (!strnpathcmp(&(s[1]), cp, j)) break;
			}
		}
	}
	if (i < 0 || i >= maxjobs || !joblist || !(joblist[i].pids))
		return(-1);
	j = joblist[i].npipe;
	if (joblist[i].stats[j] < 0 || joblist[i].stats[j] >= 128) return(-1);

	return(i);
}

int stackjob(pid, sig, trp)
long pid;
int sig;
syntaxtree *trp;
{
	int i, j, n;

	if (!joblist) {
		joblist = (jobtable *)malloc2(BUFUNIT * sizeof(jobtable));
		maxjobs = BUFUNIT;
		i = 0;
		for (n = 0; n < BUFUNIT; n++) joblist[n].pids = NULL;
	}
	else {
		n = -1;
		for (i = 0; i < maxjobs; i++) {
			if (!(joblist[i].pids)) {
				if (n < 0) n = i;
				continue;
			}
			for (j = 0; j <= joblist[i].npipe; j++)
				if (joblist[i].pids[j] == pid) break;
			if (j <= joblist[i].npipe) break;
			else if (joblist[i].pids[0] == childpgrp) {
				j = joblist[i].npipe + 1;
				break;
			}
		}
		if (i < maxjobs);
		else if (n >= 0) i = n;
		else {
			joblist = (jobtable *)realloc2(joblist,
				(maxjobs + BUFUNIT) * sizeof(jobtable));
			maxjobs += BUFUNIT;
			for (n = 0; n < BUFUNIT; n++)
				joblist[i + n].pids = NULL;
		}
	}

	if (!(joblist[i].pids)) {
		joblist[i].pids = (long *)malloc2(BUFUNIT * sizeof(long));
		joblist[i].stats = (int *)malloc2(BUFUNIT * sizeof(int));
		joblist[i].npipe = 0;
		joblist[i].trp = NULL;
		j = 0;
	}
	else if (j > joblist[i].npipe) {
		if (!(j % BUFUNIT)) {
			joblist[i].pids = (long *)realloc2(joblist[i].pids,
				(j + BUFUNIT) * sizeof(long));
			joblist[i].stats = (int *)realloc2(joblist[i].stats,
				(j + BUFUNIT) * sizeof(int));
		}
		joblist[i].npipe = j;
	}

	joblist[i].pids[j] = pid;
	joblist[i].stats[j] = sig;
	if (!j && !(joblist[i].trp) && trp) {
		joblist[i].trp = duplstree(trp, NULL);
		if (joblist[i].trp -> next) {
			freestree(joblist[i].trp -> next);
			joblist[i].trp -> next = NULL;
		}
	}

#ifdef	JOBVERBOSE
	fputs("stackjob: ", ttyout);
	fputlong(mypid, ttyout);
	fputs(": ", ttyout);
	fputlong(pid, ttyout);
	fputs(", ", ttyout);
	fputlong(i, ttyout);
	fputc(':', ttyout);
	for (j = 0; j <= joblist[i].npipe; j++) {
		fputlong(joblist[i].pids[j], ttyout);
		fputc(' ', ttyout);
	}
	fputc('\n', ttyout);
	fflush(ttyout);
#endif	/* JOBVERBOSE */
	return(i);
}

int stoppedjob(pid)
long pid;
{
	int i, j, sig;

	if (stopped) return(1);
	checkjob(0);
	if ((i = searchjob(pid, &j)) >= 0) {
		for (; j <= joblist[i].npipe; j++) {
			sig = joblist[i].stats[j];
			if (sig > 0 && sig < 128) return(1);
			else if (!sig) return(0);
		}
	}
	return(-1);
}

VOID checkjob(verbose)
int verbose;
{
	int i, j;

	while (waitjob(-1, NULL, WNOHANG | WUNTRACED) > 0);
	if (verbose) for (i = 0; i < maxjobs; i++) {
		if (!(joblist[i].pids)) continue;
		j = joblist[i].npipe;
		if (joblist[i].stats[j] >= 0 && joblist[i].stats[j] < 128)
			continue;

		if (joblist[i].trp && isopbg(joblist[i].trp))
			joblist[i].trp -> type = OP_NONE;
		if (jobok && interactive && !nottyout) dispjob(i, stderr);
		free(joblist[i].pids);
		free(joblist[i].stats);
		if (joblist[i].trp) {
			freestree(joblist[i].trp);
			free(joblist[i].trp);
		}
		joblist[i].pids = NULL;
	}
}

VOID killjob(VOID_A)
{
	int i, j;

	for (i = 0; i < maxjobs; i++) {
		if (!(joblist[i].pids)) continue;
		j = joblist[i].stats[joblist[i].npipe];
		if (j < 0 || j >= 128) continue;
		killpg(joblist[i].pids[0], SIGHUP);
		if (j > 0) killpg(joblist[i].pids[0], SIGCONT);
	}
}
#endif	/* !MSDOS && !NOJOB */

#ifndef	NOALIAS
static aliastable *NEAR duplalias(alias)
aliastable *alias;
{
	aliastable *dupl;
	int i, n;

	if (!alias) n = 0;
	else for (n = 0; alias[n].ident; n++);
	dupl = (aliastable *)malloc2((n + 1) * sizeof(aliastable));
	for (i = 0; i < n; i++) {
		dupl[i].ident = strdup2(alias[i].ident);
		dupl[i].comm = strdup2(alias[i].comm);
	}
	dupl[i].ident = NULL;
	return(dupl);
}

VOID freealias(alias)
aliastable *alias;
{
	int i;

	if (alias) {
		for (i = 0; alias[i].ident; i++) {
			free(alias[i].ident);
			free(alias[i].comm);
		}
		free(alias);
	}
}

static int cmpalias(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	aliastable *ap1, *ap2;

	ap1 = (aliastable *)vp1;
	ap2 = (aliastable *)vp2;
	return(strpathcmp2(ap1 -> ident, ap2 -> ident));
}

int checkalias(trp, ident, len, delim)
syntaxtree *trp;
char *ident;
int len, delim;
{
	int i;

	if (!trp || (trp -> flags & ST_NODE) || hascomm(trp)
	|| !interactive
	|| ((i = getstatid(trp = parentshell(trp))) >= 0
	&& !(statementlist[i].type & STT_NEEDLIST))) return(-1);

	if ((!strchr(IFS_SET, delim) && !strchr(ALIASDELIMIT, delim)))
		return(-1);
	for (i = 0; shellalias[i].ident; i++)
		if (!(shellalias[i].flags & AL_USED)
		&& !strnpathcmp(ident, shellalias[i].ident, len)
		&& !(shellalias[i].ident[len])) break;
	if (!(shellalias[i].ident)) return(-1);

	*ident = '\0';
	shellalias[i].flags |= AL_USED;
	return(i);
}
#endif	/* NOALIAS */

static int NEAR _evalexpression(s, ptr, resultp)
char *s;
int ptr;
long *resultp;
{
	long n;

	while (s[ptr] && strchr(IFS_SET, s[ptr])) ptr++;
	if (!s[ptr]) return(-1);

	if (s[ptr] == '+') ptr = _evalexpression(s, ptr + 1, resultp);
	else if (s[ptr] == '-') {
		ptr = _evalexpression(s, ptr + 1, resultp);
		*resultp = -(*resultp);
	}
	else if (s[ptr] == '~') {
		ptr = _evalexpression(s, ptr + 1, resultp);
		*resultp = ~(*resultp);
	}
	else if (s[ptr] == '!' && s[ptr + 1] != '=') {
		ptr = _evalexpression(s, ptr + 1, resultp);
		*resultp = (*resultp) ? 0 : 1;
	}
	else if (s[ptr] == '(') {
		ptr = evalexpression(s, ptr + 1, resultp, 9);
		if (ptr >= 0) {
			while (strchr(IFS_SET, s[ptr])) ptr++;
			if (s[ptr] != ')') ptr = -1;
			else ptr++;
		}
	}
	else if (isdigit(s[ptr])) {
		n = s[ptr++] - '0';
		while (isdigit(s[ptr])) {
			n = n * 10 + (s[ptr++] - '0');
			if (n < 0) return(-1);
		}
		*resultp = n;
	}

	while (s[ptr] && strchr(IFS_SET, s[ptr])) ptr++;
	return(ptr);
}

static int NEAR evalexpression(s, ptr, resultp, lvl)
char *s;
int ptr;
long *resultp;
int lvl;
{
	long n;

	if (lvl < 1) {
		if ((ptr = _evalexpression(s, ptr, resultp)) < 0) return(-1);
	}
	else {
		if ((ptr = evalexpression(s, ptr, resultp, lvl - 1)) < 0)
			return(-1);
	}

	while (s[ptr]) {
		if (lvl < 1) {
			if (s[ptr] == '*') {
				ptr = _evalexpression(s, ptr + 1, &n);
				if (ptr < 0) return(-1);
				*resultp *= n;
			}
			else if (s[ptr] == '/') {
				ptr = _evalexpression(s, ptr + 1, &n);
				if (ptr < 0) return(-1);
				*resultp /= n;
			}
			else if (s[ptr] == '%') {
				ptr = _evalexpression(s, ptr + 1, &n);
				if (ptr < 0) return(-1);
				*resultp %= n;
			}
			else break;
		}
		else if (lvl < 2) {
			if (s[ptr] == '+') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp += n;
			}
			else if (s[ptr] == '-') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp -= n;
			}
			else break;
		}
		else if (lvl < 3) {
			if (s[ptr] == '<' && s[ptr + 1] == '<') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp <<= n;
			}
			else if (s[ptr] == '>' && s[ptr + 1] == '>') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp >>= n;
			}
			else break;
		}
		else if (lvl < 4) {
			if ((s[ptr] == '<' && s[ptr + 1] == '=')
			|| (s[ptr] == '=' && s[ptr + 1] == '<')) {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp <= n);
			}
			else if ((s[ptr] == '>' && s[ptr + 1] == '=')
			|| (s[ptr] == '=' && s[ptr + 1] == '>')) {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp >= n);
			}
			else if (s[ptr] == '>') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp > n);
			}
			else if (s[ptr] == '<') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp < n);
			}
			else break;
		}
		else if (lvl < 5) {
			if (s[ptr] == '=' && s[ptr + 1] == '=') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp == n);
			}
			else if (s[ptr] == '!' && s[ptr + 1] == '=') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp != n);
			}
			else break;
		}
		else if (lvl < 6) {
			if (s[ptr] == '&' && s[ptr + 1] != '&') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp & n);
			}
			else break;
		}
		else if (lvl < 7) {
			if (s[ptr] == '^') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp ^ n);
			}
			else break;
		}
		else if (lvl < 8) {
			if (s[ptr] == '|' && s[ptr + 1] != '|') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp ^ n);
			}
			else break;
		}
		else if (lvl < 9) {
			if (s[ptr] == '&' && s[ptr + 1] == '&') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp && n);
			}
			else break;
		}
		else {
			if (s[ptr] == '|' && s[ptr + 1] == '|') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (*resultp || n);
			}
			else break;
		}
	}

	return(ptr);
}

char *evalposixsubst(s, ptrp)
char *s;
int *ptrp;
{
	char *cp, buf[MAXLONGWIDTH + 1];
	long n;
	int i, len;

	i = 1;
	if (s[i] == '(') i++;

	s += i;
	len = *ptrp - i * 2;
	s = strdupcpy(s, len);
	if (i <= 1) {
#ifndef	BASHBUG
	/* bash cannot include 'case' statement within $() */
		syntaxtree *trp, *stree;
		int type, num;

		i = len = type = num = 0;
		stree = newstree(NULL);
		trp = startvar(stree, s, &i, &type, &num, s, &len, 0);
		trp -> cont = CN_COMM;
		trp = analyze(s, trp, '`', -1);
		if (trp && (trp -> cont & CN_SBST) != CN_CASE) trp = NULL;
		freestree(stree);
		free(stree);
		if (trp) cp = NULL;
		else
#endif	/* !BASHBUG */
		if (!(cp = evalbackquote(s))) *ptrp = -1;
	}
	else {
		n = 0L;
		if (!(cp = evalvararg(s, 0, 1, '\'', 0, 0))) *ptrp = -1;
		else {
			for (i = 0; cp[i] && strchr(IFS_SET, cp[i]); i++);
			if (cp[i]) {
				i = evalexpression(cp, i, &n, 9);
				if (i < 0 || cp[i]) *ptrp = -1;
			}
			free(cp);
		}

		if (*ptrp < 0) cp = NULL;
		else {
			ascnumeric(buf, n, 0, sizeof(buf) - 1);
			cp = strdup2(buf);
		}
	}
	free(s);

	return(cp);
}

#if	!MSDOS
static VOID NEAR addmailpath(path, msg, mtime)
char *path, *msg;
time_t mtime;
{
	int i;

	for (i = 0; i < maxmailpath; i++)
		if (!strpathcmp(path, mailpathlist[i].path)) break;
	if (i < maxmailpath) {
		if (mailpathlist[i].msg) free(mailpathlist[i].msg);
	}
	else {
		mailpathlist = (mailpath_t *)realloc2(mailpathlist,
			(i + 1) * sizeof(mailpath_t));
		maxmailpath = i + 1;
		mailpathlist[i].path = strdup2(path);
	}
	mailpathlist[i].msg = msg;
	mailpathlist[i].mtime = mtime;
}

VOID replacemailpath(s, multi)
char *s;
int multi;
{
	mailpath_t *mailpath;
	time_t mtime;
	char *cp, *msg, path[MAXPATHLEN];
	int i, max, len;

	mailpath = mailpathlist;
	max = maxmailpath;
	mailpathlist = NULL;
	maxmailpath = 0;

	if (!multi) {
		for (i = 0; i < max; i++)
			if (!strpathcmp(s, mailpath[i].path)) break;
		mtime = (i < max) ? mailpath[i].mtime : 0;
		addmailpath(s, NULL, mtime);
	}
	else while (*s) {
		if ((cp = strchr(s, ':'))) len = (cp++) - s;
		else {
			len = strlen(s);
			cp = s + len;
		}
		if ((msg = strchr(s, '%'))) {
			if (msg > s + len) msg = NULL;
			else {
				i = s + len - (msg + 1);
				len = msg - s;
				msg = strdupcpy(&(s[len + 1]), i);
			}
		}
		strncpy2(path, s, len);
		s = cp;
		for (i = 0; i < max; i++)
			if (!strpathcmp(path, mailpath[i].path)) break;
		mtime = (i < max) ? mailpath[i].mtime : 0;
		addmailpath(path, msg, mtime);
	}

	for (i = 0; i < max; i++) {
		free(mailpath[i].path);
		if (mailpath[i].msg) free(mailpath[i].msg);
	}
	if (mailpath) free(mailpath);
}

VOID checkmail(reset)
int reset;
{
	static time_t lastchecked = 0;
	time_t now;
	int i;

	if (reset) {
		for (i = 0; i < maxmailpath; i++) {
			free(mailpathlist[i].path);
			if (mailpathlist[i].msg) free(mailpathlist[i].msg);
		}
		free(mailpathlist);
		mailpathlist = NULL;
		maxmailpath = 0;
		return;
	}

	if (mailcheck > 0) {
		now = time2();
		if (now < lastchecked + mailcheck) return;
		lastchecked = now;
	}

	for (i = 0; i < maxmailpath; i++)
		cmpmail(mailpathlist[i].path,
			mailpathlist[i].msg, &(mailpathlist[i].mtime));
}
#endif	/* !MSDOS */

#if	!MSDOS && !defined (NOJOB)
/*ARGSUSED*/
int posixjobs(trp)
syntaxtree *trp;
{
	int i, j;

	if (mypid != orgpgrp) return(RET_SUCCESS);
	checkjob(0);
	for (i = 0; i < maxjobs; i++) {
		if (!(joblist[i].pids)) continue;
		j = joblist[i].npipe;

		if (joblist[i].stats[j] >= 0 && joblist[i].stats[j] < 128)
			dispjob(i, stdout);
		else {
			if (isopbg(joblist[i].trp))
				joblist[i].trp -> type = OP_NONE;
			dispjob(i, stdout);
			free(joblist[i].pids);
			free(joblist[i].stats);
			if (joblist[i].trp) {
				freestree(joblist[i].trp);
				free(joblist[i].trp);
			}
			joblist[i].pids = NULL;
		}
	}
	return(RET_SUCCESS);
}

/*ARGSUSED*/
int posixfg(trp)
syntaxtree *trp;
{
	termioctl_t tty;
	char *s;
	int i, j, n, ret;

	s = ((trp -> comm) -> argc > 1) ? (trp -> comm) -> argv[1] : NULL;
	checkjob(0);
	if ((i = getjob(s)) < 0) {
		execerror((trp -> comm) -> argv[1], ER_NOSUCHJOB, 0);
		return(RET_FAIL);
	}
	n = joblist[i].npipe;
	if (tioctl(ttyio, REQGETP, &tty) < 0) {
		doperror((trp -> comm) -> argv[0], "fatal error");
		prepareexit(-1);
		Xexit2(RET_FATALERR);
	}
	fputc('[', stderr);
	fputlong(i + 1, stderr);
	fputs("] ", stderr);
	fputlong(joblist[i].pids[n], stderr);
	fputc('\n', stderr);
	fflush(stderr);
	gettermio(joblist[i].pids[0]);
	if ((j = joblist[i].stats[n]) > 0 && j < 128) {
		killpg(joblist[i].pids[0], SIGCONT);
		for (j = 0; j <= n; j++) joblist[i].stats[j] = 0;
	}
	if (isopbg(joblist[i].trp)) joblist[i].trp -> type = OP_NONE;
	ret = waitchild(joblist[i].pids[n], NULL);
	gettermio(orgpgrp);
	if (tioctl(ttyio, REQSETP, &tty) < 0) {
		doperror((trp -> comm) -> argv[0], "fatal error");
		prepareexit(-1);
		Xexit2(RET_FATALERR);
	}
	return(ret);
}

/*ARGSUSED*/
int posixbg(trp)
syntaxtree *trp;
{
	char *s;
	int i, j, n;

	s = ((trp -> comm) -> argc > 1) ? (trp -> comm) -> argv[1] : NULL;
	checkjob(0);
	if ((i = getjob(s)) < 0) {
		execerror((trp -> comm) -> argv[1], ER_NOSUCHJOB, 0);
		return(RET_FAIL);
	}
	n = joblist[i].npipe;
	if ((j = joblist[i].stats[n]) > 0 && j < 128) {
		killpg(joblist[i].pids[0], SIGCONT);
		for (j = 0; j <= n; j++) joblist[i].stats[j] = 0;
	}
	if (!isopbg(joblist[i].trp))
		joblist[i].trp -> type = OP_BG;
	if (interactive && !nottyout) dispjob(i, stderr);
	prevjob = lastjob;
	lastjob = i;
	return(RET_SUCCESS);
}
#endif	/* !MSDOS && !NOJOB */

#ifndef	NOALIAS
int posixalias(trp)
syntaxtree *trp;
{
	aliastable *alias;
	char **argv;
	int i, n, len, set, ret;

	argv = (trp -> comm) -> argv;
	if ((trp -> comm) -> argc <= 1) {
		alias = duplalias(shellalias);
		for (i = 0; alias[i].ident; i++);
		if (i > 1) qsort(alias, i, sizeof(aliastable), cmpalias);
		for (i = 0; alias[i].ident; i++) {
			fputs("alias ", stdout);
			kanjifputs(alias[i].ident, stdout);
			fputs("='", stdout);
			kanjifputs(alias[i].comm, stdout);
			fputs("'\n", stdout);
		}
		freealias(alias);
		fflush(stdout);
		return(RET_SUCCESS);
	}

	ret = RET_SUCCESS;
	for (n = 1; n < (trp -> comm) -> argc; n++) {
		set = 0;
		for (len = 0; argv[n][len]; len++) if (argv[n][len] == '=') {
			set = 1;
			break;
		}
		for (i = 0; shellalias[i].ident; i++)
			if (!strnpathcmp(shellalias[i].ident, argv[n], len)
			&& !(shellalias[i].ident[len]))
				break;

		if (!set) {
			if (!(shellalias[i].ident)) {
				if (interactive) {
					fputs("alias: ", stderr);
					execerror(argv[n], ER_NOTFOUND, 0);
				}
				ret = RET_FAIL;
				ERRBREAK;
			}
			fputs("alias ", stdout);
			kanjifputs(shellalias[i].ident, stdout);
			fputs("='", stdout);
			kanjifputs(shellalias[i].comm, stdout);
			fputs("'\n", stdout);
			fflush(stdout);
		}
		else if (shellalias[i].ident) {
			free(shellalias[i].comm);
			shellalias[i].comm = strdup2(&(argv[n][++len]));
		}
		else {
			shellalias = (aliastable *)realloc2(shellalias,
				(i + 2) * sizeof(aliastable));
			shellalias[i].ident = strdupcpy(argv[n], len);
			shellalias[i].comm = strdup2(&(argv[n][++len]));
			shellalias[++i].ident = NULL;
		}
	}
	return(ret);
}

int posixunalias(trp)
syntaxtree *trp;
{
	reg_t *re;
	char **argv;
	int i, n, c, ret;

	argv = (trp -> comm) -> argv;
	ret = RET_SUCCESS;
	for (n = 1; n < (trp -> comm) -> argc; n++) {
		c = 0;
		re = regexp_init(argv[n], -1);
		for (i = 0; shellalias[i].ident; i++) {
			if (re) {
				if (!regexp_exec(re, shellalias[i].ident, 0))
					continue;
			}
			else if (strpathcmp(shellalias[i].ident, argv[n]))
				continue;
			c++;
			free(shellalias[i].ident);
			free(shellalias[i].comm);
			for (; shellalias[i + 1].ident; i++) {
				shellalias[i].ident = shellalias[i + 1].ident;
				shellalias[i].comm = shellalias[i + 1].comm;
			}
			shellalias[i].ident = NULL;
		}
		if (re) regexp_free(re);
		if (!c) {
			if (interactive) {
				fputs("alias: ", stderr);
				execerror(argv[n], ER_NOTALIAS, 0);
			}
			ret = RET_FAIL;
			ERRBREAK;
		}
	}
	return(ret);
}
#endif	/* NOALIAS */

int posixkill(trp)
syntaxtree *trp;
{
	char *s;
	long pid;
	int i, n, sig;

	if ((trp -> comm) -> argc <= 1) {
		fputs("usage: kill [ -sig ] pid ...\n", stderr);
		fputs("for a list of signals: kill -l\n", stderr);
		fflush(stderr);
		return(RET_SYNTAXERR);
	}
#ifdef	SIGTERM
	sig = SIGTERM;
#else
	sig = 0;
#endif
	s = (trp -> comm) -> argv[1];
	if (s[0] != '-') i = 1;
	else {
		if (s[1] == 'l') {
			for (sig = n = 0; sig < NSIG; sig++) {
				for (i = 0; signallist[i].sig >= 0; i++)
					if (sig == signallist[i].sig) break;
				if (signallist[i].sig < 0) continue;
				fputs(signallist[i].ident, stdout);
				fputc((++n % 16) ? ' ' : '\n', stdout);
			}
			if (n % 16) fputc('\n', stdout);
			fflush(stdout);
			return(RET_SUCCESS);
		}
		if ((sig = isnumeric(&(s[1]))) < 0) {
			for (i = 0; signallist[i].sig >= 0; i++)
				if (!strcmp(&(s[1]), signallist[i].ident))
					break;
			if (signallist[i].sig < 0) {
				execerror(&(s[1]), ER_UNKNOWNSIG, 0);
				return(RET_FAIL);
			}
			sig = signallist[i].sig;
		}
		if (sig < 0 || sig >= NSIG) {
			execerror(s, ER_NUMOUTRANGE, 0);
			return(RET_FAIL);
		}
		i = 2;
	}

#if	!MSDOS && !defined (NOJOB)
	checkjob(0);
#endif
	for (; i < (trp -> comm) -> argc; i++) {
		s = (trp -> comm) -> argv[i];
#if	!MSDOS && !defined (NOJOB)
		if (*s == '%') {
			if ((n = getjob(s)) < 0) {
				execerror(s, ER_NOSUCHJOB, 0);
				return(RET_FAIL);
			}
			n = killpg(joblist[n].pids[0], sig);
		}
		else
#endif	/* !MSDOS && !NOJOB */
		if ((pid = isnumeric(s)) < 0) {
			fputs("usage: kill [ -sig ] pid ...\n", stderr);
			fputs("for a list of signals: kill -l\n", stderr);
			fflush(stderr);
			return(RET_SYNTAXERR);
		}
#if	MSDOS
		else if (pid && pid != mypid) {
			errno = EPERM;
			n = -1;
		}
#endif
		else n = kill(pid, sig);

		if (n < 0) {
			doperror((trp -> comm) -> argv[0], s);
			return(RET_FAIL);
		}
	}
	return(RET_SUCCESS);
}

#ifndef	NOPOSIXUTIL
int posixcommand(trp)
syntaxtree *trp;
{
	hashlist *hp;
	char *cp, **argv, *path;
	int i, n, flag, ret, id, type, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;

	if ((flag = tinygetopt(trp, "pvV", &n)) < 0) return(RET_FAIL);

	if (interrupted) return(RET_INTR);
	if (argc <= n) return(ret_status);

# ifdef	BASHSTYLE
	ret = RET_FAIL;
# else
	ret = RET_SUCCESS;
# endif
	if (flag == 'V') {
		for (i = n; i < argc; i++) {
			if (typeone(argv[i], stdout) >= 0) {
# ifdef	BASHSTYLE
				ret = RET_SUCCESS;
# endif
			}
		}
		fflush(stdout);
		return(ret);
	}
	else if (flag == 'v') {
		for (i = n; i < argc; i++) {
#ifdef	NOALIAS
			type = checktype(argv[i], &id, 0);
#else
			type = checktype(argv[i], &id, 0, 0);
#endif

			cp = argv[i];
			if (type == CT_COMMAND) {
				type = searchhash(&hp, cp, NULL);
# ifdef	_NOUSEHASH
				if (!(type & CM_FULLPATH)) cp = (char *)hp;
# else
				if (type & CM_HASH) cp = hp -> path;
# endif
				if ((type & CM_NOTFOUND)
				|| Xaccess(cp, X_OK) < 0) {
					execerror(cp, ER_COMNOFOUND, 0);
					cp = NULL;
				}
			}

			if (cp) {
				fputs(cp, stdout);
				fputc('\n', stdout);
# ifdef	BASHSTYLE
				ret = RET_SUCCESS;
# endif
			}
		}
		fflush(stdout);
		return(ret);
	}

#ifdef	NOALIAS
	type = checktype(argv[n], &id, 0);
#else
	type = checktype(argv[n], &id, 0, 0);
#endif
	if (verboseexec) {
		fputs("+ ", stderr);
		kanjifputs(argv[n], stderr);
		for (i = n + 1; i < argc; i++) {
			fputc(' ', stderr);
			kanjifputs(argv[i], stderr);
		}
		fputc('\n', stderr);
		fflush(stderr);
	}

	(trp -> comm) -> argc -= n;
	(trp -> comm) -> argv += n;
	if (flag != 'p')
#if	MSDOS
		ret = exec_simplecom(trp, type, id);
#else
		ret = exec_simplecom(trp, type, id, 0);
#endif
	else {
		path = strdup2(getconstvar("PATH"));
		setenv2("PATH", DEFPATH);
#if	MSDOS
		ret = exec_simplecom(trp, type, id);
#else
		ret = exec_simplecom(trp, type, id, 0);
#endif
		setenv2("PATH", path);
		free(path);
	}
	(trp -> comm) -> argc = argc;
	(trp -> comm) -> argv = argv;
	return(ret);
}

int posixgetopts(trp)
syntaxtree *trp;
{
	static int ptr = 0;
	char *cp, *optstr, *posixoptarg, *name, **argv, buf[MAXLONGWIDTH + 1];
	int n, ret, quiet, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;

	if (argc <= 2) {
		execerror(NULL, ER_MISSARG, 0);
		return(RET_FAIL);
	}

	optstr = argv[1];
	name = argv[2];
	quiet = 0;
	if (*optstr == ':') {
		quiet = 1;
		optstr++;
	}

	if (argc > 3) {
		argv = &(argv[2]);
		argc -= 2;
	}
	else {
		argv = argvar;
		argc = countvar(argv);
	}

	if (posixoptind <= 0) {
		ptr = 0;
		posixoptind = 1;
	}

	ret = RET_SUCCESS;
	posixoptarg = NULL;
	if (posixoptind >= argc || !argv[posixoptind]) ret = RET_FAIL;
	else if (ptr >= strlen(argv[posixoptind])) {
		if (++posixoptind < argc && argv[posixoptind]) ptr = 0;
		else ret = RET_FAIL;
	}

	if (ret == RET_SUCCESS && !ptr) {
		if (argv[posixoptind][0] != '-') ret = RET_FAIL;
		else if (argv[posixoptind][1] == '-'
		&& !(argv[posixoptind][2])) {
			posixoptind++;
			ret = RET_FAIL;
		}
		else if (!(argv[posixoptind][ptr + 1])) ret = RET_FAIL;
		else ptr++;
	}

	if (ret == RET_SUCCESS) {
		n = argv[posixoptind][ptr++];
		if (!islower(n & 0xff) || !(cp = strchr(optstr, n))) {
			buf[0] = n;
			buf[1] = '\0';
			n = '?';
			if (quiet) posixoptarg = buf;
			else execerror(buf, ER_BADOPTIONS, 0);
		}
		else if (*(cp + 1) == ':') {
			if (argv[posixoptind][ptr])
				posixoptarg = &(argv[posixoptind++][ptr]);
			else if (++posixoptind < argc && argv[posixoptind])
				posixoptarg = argv[posixoptind++];
			else {
				buf[0] = n;
				buf[1] = '\0';
				if (quiet) {
					n = ':';
					posixoptarg = buf;
				}
				else {
					n = '?';
					execerror(buf, ER_MISSARG, 0);
				}
			}
			ptr = 0;
		}
	}

	if (posixoptarg) setenv2("OPTARG", posixoptarg);
	else if (ret == RET_SUCCESS) unset("OPTARG", sizeof("OPTARG") - 1);

	if (ret != RET_SUCCESS) n = '?';
	buf[0] = n;
	buf[1] = '\0';
	setenv2(name, buf);

	n = posixoptind;
	setenv2("OPTIND", ascnumeric(buf, n, 0, sizeof(buf) - 1));
	posixoptind = n;

	return(ret);
}
#endif	/* NOPOSIXUTIL */
#endif	/* !_NOORIGSHELL && !MINIMUMSHELL && (!FD || (FD >= 2)) */
