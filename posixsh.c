/*
 *	posixsh.c
 *
 *	XSH command for POSIX
 */

#include <fcntl.h>
#ifdef	FD
#include "fd.h"
#else	/* !FD */
#include "machine.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
# ifndef	NOUNISTDH
# include <unistd.h>
# endif
# ifndef	NOSTDLIBH
# include <stdlib.h>
# endif
#include "printf.h"
#include "kctype.h"
#endif	/* !FD */

#if	MSDOS
#include "unixemu.h"
#else
#include <sys/param.h>
#endif

#include "system.h"

#if	!defined (MINIMUMSHELL) \
&& (!defined (FD) || (FD >= 2 && !defined (_NOORIGSHELL)))

#ifdef	FD
#include "term.h"
extern int Xstat __P_((CONST char *, struct stat *));
extern int Xlstat __P_((CONST char *, struct stat *));
extern int Xaccess __P_((CONST char *, int));
# ifndef	_NOPTY
#include "termemu.h"
extern VOID sendparent __P_((int, ...));
# endif
#else	/* !FD */
extern int ttyio;
extern FILE *ttyout;
# if	MSDOS
extern int Xstat __P_((CONST char *, struct stat *));
# define	Xlstat		Xstat
# else
# define	Xstat(p, s)	((stat(p, s)) ? -1 : 0)
# define	Xlstat(p, s)	((lstat(p, s)) ? -1 : 0)
# endif
#define	Xaccess(p, m)		((access(p, m)) ? -1 : 0)
#endif	/* !FD */

#define	ALIASDELIMIT		"();&|<>"
#define	BUFUNIT			32
#define	getconstvar(s)		(getshellvar(s, strsize(s)))
#define	ER_COMNOFOUND		1
#define	ER_NOTFOUND		2
#define	ER_NOTIDENT		4
#define	ER_BADOPTIONS		13
#define	ER_NUMOUTRANGE		18
#define	ER_NOTALIAS		20
#define	ER_MISSARG		21
#define	ER_NOSUCHJOB		22
#define	ER_TERMINATED		23
#define	ER_UNKNOWNSIG		26
#ifndef	EPERM
#define	EPERM			EACCES
#endif

#if	!MSDOS
typedef struct _mailpath_t {
	char *path;
	char *msg;
	time_t mtime;
} mailpath_t;
#endif

extern VOID error __P_((CONST char *));
extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strdup2 __P_((CONST char *));
extern char *strndup2 __P_((CONST char *, int));
extern char *strchr2 __P_((CONST char *, int));
extern char *strncpy2 __P_((char *, CONST char *, int));
extern int setenv2 __P_((CONST char *, CONST char *, int));
extern time_t time2 __P_((VOID_A));

#ifndef	NOJOB
static CONST char *NEAR headstree __P_((syntaxtree *));
#endif
#ifndef	NOALIAS
static int NEAR searchalias __P_((CONST char *, int));
static shaliastable *NEAR duplalias __P_((shaliastable *));
static int cmpalias __P_((CONST VOID_P, CONST VOID_P));
#endif
static int NEAR _evalexpression __P_((CONST char *, int, long *));
static int NEAR evalexpression __P_((CONST char *, int, long *, int));
#if	!MSDOS
static VOID NEAR addmailpath __P_((CONST char *, char *, time_t));
#endif
static int NEAR testsub1 __P_((int, CONST char *, int *));
static int NEAR testsub2 __P_((int, char *CONST *, int *));
static int NEAR testsub3 __P_((int, char *CONST *, int *, int));

#ifndef	NOJOB
int gettermio __P_((p_id_t, int));
VOID dispjob __P_((int, FILE *));
int searchjob __P_((p_id_t, int *));
int getjob __P_((CONST char *));
int stackjob __P_((p_id_t, int, syntaxtree *));
int stoppedjob __P_((p_id_t));
VOID killjob __P_((VOID_A));
VOID checkjob __P_((FILE *));
#endif	/* !NOJOB */
char *evalposixsubst __P_((CONST char *, int *));
#if	!MSDOS
VOID replacemailpath __P_((CONST char *, int));
VOID checkmail __P_((int));
#endif
#ifndef	NOALIAS
int addalias __P_((char *, char *));
int deletealias __P_((CONST char *));
VOID freealias __P_((shaliastable *));
int checkalias __P_((syntaxtree *, char *, int, int));
#endif
#ifndef	NOJOB
int posixjobs __P_((syntaxtree *));
int posixfg __P_((syntaxtree *));
int posixbg __P_((syntaxtree *));
#endif
#ifndef	NOALIAS
int posixalias __P_((syntaxtree *));
int posixunalias __P_((syntaxtree *));
#endif
int posixkill __P_((syntaxtree *));
int posixtest __P_((syntaxtree *));
#ifndef	NOPOSIXUTIL
int posixcommand __P_((syntaxtree *));
int posixgetopts __P_((syntaxtree *));
#endif

#ifndef	NOJOB
jobtable *joblist = NULL;
int maxjobs = 0;
#endif
#if	!MSDOS
int mailcheck = 0;
#endif
#ifndef	NOALIAS
shaliastable *shellalias = NULL;
#endif
#ifndef	NOPOSIXUTIL
int posixoptind = 0;
#endif

#if	!MSDOS
static mailpath_t *mailpathlist = NULL;
static int maxmailpath = 0;
#endif


#ifndef	NOJOB
int gettermio(pgrp, job)
p_id_t pgrp;
int job;
{
	int ret;
	sigmask_t mask, omask;

	if (ttypgrp < (p_id_t)0 || pgrp < (p_id_t)0 || pgrp == ttypgrp)
		return(0);
	else if (!job) {
		ttypgrp = pgrp;
		return(0);
	}

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
	fprintf2(ttyout, "gettermio: %id: %id -> %id", mypid, ttypgrp, pgrp);
	fputnl(ttyout);
#endif	/* JOBVERBOSE */
	if ((ret = settcpgrp(ttyio, pgrp)) >= 0) ttypgrp = pgrp;
	Xsigsetmask(omask);

	return(ret);
}

static CONST char *NEAR headstree(trp)
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
	i = joblist[n].npipe;
	fprintf2(fp, "[%d]%c %id ",
		n + 1, (n == lastjob) ? '+' : ((n == prevjob) ? '-' : ' '),
		joblist[n].pids[i]);
	sig = joblist[n].stats[i];

	if (sig <= 0)
		fprintf2(fp, "%-28.28s", (sig) ? "Done" : "Running");
	else {
		if (sig >= 128) sig -= 128;
		dispsignal(sig, 28, fp);
	}
	if (joblist[n].trp) {
		if (!isopbg(joblist[n].trp) && !isopnown(joblist[n].trp)
		&& !isoppipe(joblist[n].trp))
			joblist[n].trp -> type = OP_NONE;
		printstree(joblist[n].trp, -1, fp);
	}
	fputnl(fp);
}

int searchjob(pid, np)
p_id_t pid;
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
CONST char *s;
{
	CONST char *cp;
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
	if (joblist[i].stats[j] < 0 || joblist[i].stats[j] >= 128) return(-2);

	return(i);
}

int stackjob(pid, sig, trp)
p_id_t pid;
int sig;
syntaxtree *trp;
{
	int i, j, n;

#ifdef	FAKEUNINIT
	j = 0;	/* fake for -Wuninitialized */
#endif
	if (!joblist) {
		joblist = (jobtable *)malloc2(BUFUNIT * sizeof(jobtable));
		maxjobs = BUFUNIT;
		i = 0;
		for (n = 0; n < maxjobs; n++) joblist[n].pids = NULL;
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
		if (i < maxjobs) /*EMPTY*/;
		else if (n >= 0) i = n;
		else {
			joblist = (jobtable *)realloc2(joblist,
				(maxjobs + BUFUNIT) * sizeof(jobtable));
			maxjobs += BUFUNIT;
			for (n = i; n < maxjobs; n++) joblist[n].pids = NULL;
		}
	}

	if (!(joblist[i].pids)) {
		joblist[i].pids = (p_id_t *)malloc2(BUFUNIT * sizeof(p_id_t));
		joblist[i].stats = (int *)malloc2(BUFUNIT * sizeof(int));
		joblist[i].npipe = 0;
		joblist[i].trp = NULL;
		joblist[i].tty = NULL;
# ifdef	USESGTTY
		joblist[i].ttyflag = NULL;
# endif
		j = 0;
	}
	else if (j > joblist[i].npipe) {
		if (!(j % BUFUNIT)) {
			joblist[i].pids = (p_id_t *)realloc2(joblist[i].pids,
				(j + BUFUNIT) * sizeof(p_id_t));
			joblist[i].stats = (int *)realloc2(joblist[i].stats,
				(j + BUFUNIT) * sizeof(int));
		}
		joblist[i].npipe = j;
	}

	joblist[i].pids[j] = pid;
	joblist[i].stats[j] = sig;
	if (!j && !(joblist[i].trp) && trp) {
		joblist[i].trp = duplstree(trp, NULL, 0L);
		if (!isoppipe(joblist[i].trp) && joblist[i].trp -> next) {
			freestree(joblist[i].trp -> next);
			joblist[i].trp -> next = NULL;
		}
	}

#ifdef	JOBVERBOSE
	fprintf2(ttyout, "stackjob: %id: %id, %d:", mypid, pid, i);
	for (j = 0; j <= joblist[i].npipe; j++)
		fprintf2(ttyout, "%id ", joblist[i].pids[j]);
	fputnl(ttyout);
#endif	/* JOBVERBOSE */

	return(i);
}

int stoppedjob(pid)
p_id_t pid;
{
	int i, j, sig;

	if (stopped) return(1);
	if ((i = searchjob(pid, &j)) >= 0) {
		for (; j <= joblist[i].npipe; j++) {
			sig = joblist[i].stats[j];
			if (sig > 0 && sig < 128) return(1);
			else if (!sig) return(0);
		}
	}

	return(-1);
}

VOID checkjob(fp)
FILE *fp;
{
	int i, j;

	while (waitjob(-1, NULL, WNOHANG | WUNTRACED) > 0);
	if (fp) for (i = 0; i < maxjobs; i++) {
		if (!(joblist[i].pids)) continue;
		j = joblist[i].npipe;
		if (joblist[i].stats[j] >= 0 && joblist[i].stats[j] < 128)
			continue;

		if (joblist[i].trp
		&& (isopbg(joblist[i].trp) || isopnown(joblist[i].trp)))
			joblist[i].trp -> type = OP_NONE;
		if (jobok && interactive && !nottyout) dispjob(i, fp);
		free(joblist[i].pids);
		free(joblist[i].stats);
		if (joblist[i].trp) {
			freestree(joblist[i].trp);
			free(joblist[i].trp);
		}
		if (joblist[i].tty) free(joblist[i].tty);
# ifdef	USESGTTY
		if (joblist[i].ttyflag) free(joblist[i].ttyflag);
# endif
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
		Xkillpg(joblist[i].pids[0], SIGHUP);
		if (j > 0) Xkillpg(joblist[i].pids[0], SIGCONT);
	}
}
#endif	/* !NOJOB */

#ifndef	NOALIAS
static int NEAR searchalias(ident, len)
CONST char *ident;
int len;
{
	int i;

	if (len < 0) len = strlen(ident);
	for (i = 0; shellalias[i].ident; i++)
		if (!strncommcmp(ident, shellalias[i].ident, len)
		&& !(shellalias[i].ident[len]))
			break;

	return(i);
}

int addalias(ident, comm)
char *ident, *comm;
{
	int i;

	i = searchalias(ident, -1);
	if (shellalias[i].ident) {
		free(shellalias[i].ident);
		free(shellalias[i].comm);
	}
	else {
		shellalias = (shaliastable *)realloc2(shellalias,
			(i + 2) * sizeof(shaliastable));
		shellalias[i + 1].ident = NULL;
	}

	shellalias[i].ident = ident;
	shellalias[i].comm = comm;
# if	defined (FD) && !defined (_NOPTY)
	sendparent(TE_ADDALIAS, ident, comm);
# endif

	return(0);
}

int deletealias(ident)
CONST char *ident;
{
	reg_t *re;
	int i, n, max;

	n = 0;
	re = regexp_init(ident, -1);
	for (max = 0; shellalias[max].ident; max++) /*EMPTY*/;
	for (i = 0; i < max; i++) {
		if (re) {
			if (!regexp_exec(re, shellalias[i].ident, 0)) continue;
		}
		else if (strcommcmp(ident, shellalias[i].ident)) continue;
		free(shellalias[i].ident);
		free(shellalias[i].comm);
		memmove((char *)&(shellalias[i]), (char *)&(shellalias[i + 1]),
			(max-- - i) * sizeof(shaliastable));
		i--;
		n++;
	}
	regexp_free(re);
	if (!n) return(-1);
# if	defined (FD) && !defined (_NOPTY)
	sendparent(TE_DELETEALIAS, ident);
# endif

	return(0);
}

static shaliastable *NEAR duplalias(alias)
shaliastable *alias;
{
	shaliastable *dupl;
	int i, n;

	if (!alias) n = 0;
	else for (n = 0; alias[n].ident; n++) /*EMPTY*/;
	dupl = (shaliastable *)malloc2((n + 1) * sizeof(shaliastable));
	for (i = 0; i < n; i++) {
		dupl[i].ident = strdup2(alias[i].ident);
		dupl[i].comm = strdup2(alias[i].comm);
	}
	dupl[i].ident = NULL;

	return(dupl);
}

VOID freealias(alias)
shaliastable *alias;
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
	shaliastable *ap1, *ap2;

	ap1 = (shaliastable *)vp1;
	ap2 = (shaliastable *)vp2;

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
	|| ((i = getstatid(parentstree(trp))) >= 0
	&& !(statementlist[i].type & STT_NEEDLIST)))
		return(-1);

	if ((!strchr(IFS_SET, delim) && !strchr(ALIASDELIMIT, delim)))
		return(-1);
	i = searchalias(ident, len);
	if (!(shellalias[i].ident) || (shellalias[i].flags & AL_USED))
		return(-1);

	*ident = '\0';
	shellalias[i].flags |= AL_USED;

	return(i);
}
#endif	/* NOALIAS */

static int NEAR _evalexpression(s, ptr, resultp)
CONST char *s;
int ptr;
long *resultp;
{
	long n;
	int i, c, base;

	while (s[ptr] && strchr(IFS_SET, s[ptr])) ptr++;
	if (!s[ptr]) return(-1);

	if (s[ptr] == '+') {
		if ((ptr = _evalexpression(s, ptr + 1, resultp)) < 0)
			return(-1);
	}
	else if (s[ptr] == '-') {
		if ((ptr = _evalexpression(s, ptr + 1, resultp)) < 0)
			return(-1);
		*resultp = -(*resultp);
	}
	else if (s[ptr] == '~') {
		if ((ptr = _evalexpression(s, ptr + 1, resultp)) < 0)
			return(-1);
		*resultp = ~(*resultp);
	}
	else if (s[ptr] == '!' && s[ptr + 1] != '=') {
		if ((ptr = _evalexpression(s, ptr + 1, resultp)) < 0)
			return(-1);
		*resultp = (*resultp) ? 0L : 1L;
	}
	else if (s[ptr] == '(') {
		if ((ptr = evalexpression(s, ptr + 1, resultp, 9)) < 0)
			return(-1);
		while (strchr(IFS_SET, s[ptr])) ptr++;
		if (s[ptr++] != ')') return(-1);
	}
	else if (isdigit2(s[ptr])) {
		if (s[ptr] != '0') base = 10;
		else if (toupper2(s[++ptr]) != 'X') base = 8;
		else {
			base = 16;
			ptr++;
		}

		n = 0;
		for (i = ptr; s[i]; i++) {
			c = s[i];
			if (isdigit2(c)) c -= '0';
			else if (islower2(c)) c -= 'a' - 10;
			else if (isupper2(c)) c -= 'A' - 10;
			else c = -1;
			if (c < 0 || c >= base) break;

			if (n > MAXTYPE(long) / base
			|| (n == MAXTYPE(long) / base
			&& c > MAXTYPE(long) % base))
				return(-1);
			n = n * base + c;
		}
		if (i <= ptr) return(-1);
		ptr = i;
		*resultp = n;
	}
	else return(-1);

	while (s[ptr] && strchr(IFS_SET, s[ptr])) ptr++;

	return(ptr);
}

static int NEAR evalexpression(s, ptr, resultp, lvl)
CONST char *s;
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
				if (ptr < 0 || !n) return(-1);
				*resultp /= n;
			}
			else if (s[ptr] == '%') {
				ptr = _evalexpression(s, ptr + 1, &n);
				if (ptr < 0 || !n) return(-1);
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
				*resultp = (long)(*resultp <= n);
			}
			else if ((s[ptr] == '>' && s[ptr + 1] == '=')
			|| (s[ptr] == '=' && s[ptr + 1] == '>')) {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (long)(*resultp >= n);
			}
			else if (s[ptr] == '>') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (long)(*resultp > n);
			}
			else if (s[ptr] == '<') {
				ptr = evalexpression(s, ptr + 1, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (long)(*resultp < n);
			}
			else break;
		}
		else if (lvl < 5) {
			if (s[ptr] == '=' && s[ptr + 1] == '=') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (long)(*resultp == n);
			}
			else if (s[ptr] == '!' && s[ptr + 1] == '=') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (long)(*resultp != n);
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
				*resultp = (*resultp | n);
			}
			else break;
		}
		else if (lvl < 9) {
			if (s[ptr] == '&' && s[ptr + 1] == '&') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (long)(*resultp && n);
			}
			else break;
		}
		else {
			if (s[ptr] == '|' && s[ptr + 1] == '|') {
				ptr = evalexpression(s, ptr + 2, &n, lvl - 1);
				if (ptr < 0) return(-1);
				*resultp = (long)(*resultp || n);
			}
			else break;
		}
	}

	return(ptr);
}

char *evalposixsubst(s, ptrp)
CONST char *s;
int *ptrp;
{
#ifndef	BASHBUG
	syntaxtree *trp, *stree;
	redirectlist red;
#endif
	char *cp, *new;
	long n;
	int i, len;

	i = 1;
	if (s[i] == '(') i++;

	s += i;
	len = *ptrp - i * 2;
	s = new = strndup2(s, len);
	if (i <= 1) {
#ifndef	BASHBUG
	/* bash cannot include 'case' statement within $() */
		red.fd = red.type = red.new = 0;
		red.filename = vnullstr;
		i = len = 0;
		stree = newstree(NULL);
		trp = startvar(stree, &red, nullstr, &i, &len, 0);
		trp -> cont = CN_COMM;
		trp = analyze(s, trp, -1);
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
		if (!(cp = evalvararg(new, '\'', EA_BACKQ, 0))) *ptrp = -1;
		else {
			for (i = 0; cp[i]; i++)
				if (!strchr(IFS_SET, cp[i])) break;
			if (cp[i]) {
				i = evalexpression(cp, i, &n, 9);
				if (i < 0 || cp[i]) *ptrp = -1;
			}
			free(cp);
		}

		if (*ptrp < 0) cp = NULL;
		else if (asprintf2(&cp, "%ld", n) < 0) error("malloc()");
	}
	free(new);

	return(cp);
}

#if	!MSDOS
static VOID NEAR addmailpath(path, msg, mtime)
CONST char *path;
char *msg;
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
CONST char *s;
int multi;
{
	mailpath_t *mailpath;
	time_t mtime;
	CONST char *cp;
	char *msg, path[MAXPATHLEN];
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
			cp = &(s[len]);
		}
		if ((msg = strchr(s, '%'))) {
			if (msg > s + len) msg = NULL;
			else {
				i = s + len - (msg + 1);
				len = msg - s;
				msg = strndup2(&(s[len + 1]), i);
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
		if (mailpathlist) free(mailpathlist);
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

#ifndef	NOJOB
/*ARGSUSED*/
int posixjobs(trp)
syntaxtree *trp;
{
	int i, j;

	if (mypid != orgpgrp) return(RET_SUCCESS);
	checkjob(NULL);
	for (i = 0; i < maxjobs; i++) {
		if (!(joblist[i].pids)) continue;
		j = joblist[i].npipe;

		if (joblist[i].stats[j] >= 0 && joblist[i].stats[j] < 128)
			dispjob(i, stdout);
		else {
			if (isopbg(joblist[i].trp) || isopnown(joblist[i].trp))
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
	checkjob(NULL);
	if ((i = getjob(s)) < 0) {
		execerror((trp -> comm) -> argv[1],
			(i < -1) ? ER_TERMINATED : ER_NOSUCHJOB, 2);
		return(RET_FAIL);
	}
	n = joblist[i].npipe;
	if (tioctl(ttyio, REQGETP, &tty) < 0) {
		doperror((trp -> comm) -> argv[0], "fatal error");
		prepareexit(-1);
		Xexit(RET_FATALERR);
	}
	fprintf2(stderr, "[%d] %id", i + 1, joblist[i].pids[n]);
	fputnl(stderr);
	if (joblist[i].tty) tioctl(ttyio, REQSETP, joblist[i].tty);
# ifdef	USESGTTY
	if (joblist[i].ttyflag) ioctl(ttyio, TIOCLSET, joblist[i].ttyflag);
# endif
	gettermio(joblist[i].pids[0], jobok);
	if ((j = joblist[i].stats[n]) > 0 && j < 128) {
		Xkillpg(joblist[i].pids[0], SIGCONT);
		for (j = 0; j <= n; j++) joblist[i].stats[j] = 0;
	}
	if (isopbg(joblist[i].trp) || isopnown(joblist[i].trp))
		joblist[i].trp -> type = OP_NONE;
	ret = waitchild(joblist[i].pids[n], NULL);
	gettermio(orgpgrp, jobok);
	if (tioctl(ttyio, REQSETP, &tty) < 0) {
		doperror((trp -> comm) -> argv[0], "fatal error");
		prepareexit(-1);
		Xexit(RET_FATALERR);
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
	checkjob(NULL);
	if ((i = getjob(s)) < 0) {
		execerror((trp -> comm) -> argv[1],
			(i < -1) ? ER_TERMINATED : ER_NOSUCHJOB, 2);
		return(RET_FAIL);
	}
	n = joblist[i].npipe;
	if ((j = joblist[i].stats[n]) > 0 && j < 128) {
		Xkillpg(joblist[i].pids[0], SIGCONT);
		for (j = 0; j <= n; j++) joblist[i].stats[j] = 0;
	}
	if (!isopbg(joblist[i].trp) || !isopnown(joblist[i].trp))
		joblist[i].trp -> type = OP_BG;
	if (interactive && !nottyout) dispjob(i, stderr);
	prevjob = lastjob;
	lastjob = i;

	return(RET_SUCCESS);
}
#endif	/* !NOJOB */

#ifndef	NOALIAS
int posixalias(trp)
syntaxtree *trp;
{
	shaliastable *alias;
	char **argv;
	int i, n, len, set, ret;

	argv = (trp -> comm) -> argv;
	if ((trp -> comm) -> argc <= 1) {
		alias = duplalias(shellalias);
		for (i = 0; alias[i].ident; i++) /*EMPTY*/;
		if (i > 1) qsort(alias, i, sizeof(shaliastable), cmpalias);
		for (i = 0; alias[i].ident; i++) {
			fprintf2(stdout, "alias %k='%k'",
				alias[i].ident, alias[i].comm);
			fputnl(stdout);
		}
		freealias(alias);
		return(RET_SUCCESS);
	}

	ret = RET_SUCCESS;
	for (n = 1; n < (trp -> comm) -> argc; n++) {
		set = 0;
		for (len = 0; argv[n][len]; len++) if (argv[n][len] == '=') {
			set = 1;
			break;
		}

		if (set) VOID_C addalias(strndup2(argv[n], len),
			strdup2(&(argv[n][len + 1])));
		else {
			i = searchalias(argv[n], len);
			if (!(shellalias[i].ident)) {
				if (interactive) {
					fputs("alias: ", stderr);
					execerror(argv[n], ER_NOTFOUND, 2);
				}
				ret = RET_FAIL;
				ERRBREAK;
			}
			fprintf2(stdout, "alias %k='%k'",
				shellalias[i].ident, shellalias[i].comm);
			fputnl(stdout);
		}
	}

	return(ret);
}

int posixunalias(trp)
syntaxtree *trp;
{
	char **argv;
	int n, ret;

	argv = (trp -> comm) -> argv;
	ret = RET_SUCCESS;
	for (n = 1; n < (trp -> comm) -> argc; n++) {
		if (deletealias(argv[n]) < 0) {
			if (interactive) {
				fputs("alias: ", stderr);
				execerror(argv[n], ER_NOTALIAS, 2);
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
	p_id_t pid;
	int i, n, sig;

	if ((trp -> comm) -> argc <= 1) {
		fputs("usage: kill [ -sig ] pid ...\n", stderr);
		fputs("for a list of signals: kill -l", stderr);
		fputnl(stderr);
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
				fprintf2(stdout, "%s%c",
					signallist[i].ident,
					(++n % 16) ? ' ' : '\n');
			}
			if (n % 16) fputnl(stdout);
			fflush(stdout);
			return(RET_SUCCESS);
		}
		if ((sig = isnumeric(&(s[1]))) < 0) {
			for (i = 0; signallist[i].sig >= 0; i++)
				if (!strcmp(&(s[1]), signallist[i].ident))
					break;
			if (signallist[i].sig < 0) {
				execerror(&(s[1]), ER_UNKNOWNSIG, 1);
				return(RET_FAIL);
			}
			sig = signallist[i].sig;
		}
		if (sig < 0 || sig >= NSIG) {
			execerror(s, ER_NUMOUTRANGE, 1);
			return(RET_FAIL);
		}
		i = 2;
	}

#ifndef	NOJOB
	checkjob(NULL);
#endif
	for (; i < (trp -> comm) -> argc; i++) {
		s = (trp -> comm) -> argv[i];
#ifndef	NOJOB
		if (*s == '%') {
			if ((n = getjob(s)) < 0) {
				execerror(s, (n < -1)
					? ER_TERMINATED : ER_NOSUCHJOB, 2);
				return(RET_FAIL);
			}
			n = Xkillpg(joblist[n].pids[0], sig);
		}
		else
#endif	/* !NOJOB */
		if ((pid = isnumeric(s)) < (p_id_t)0) {
			fputs("usage: kill [ -sig ] pid ...\n", stderr);
			fputs("for a list of signals: kill -l", stderr);
			fputnl(stderr);
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

static int NEAR testsub1(c, s, ptrp)
int c;
CONST char *s;
int *ptrp;
{
	struct stat st;
	int ret;

	ret = -1;
	switch (c) {
		case 'r':
			if (s) ret = (Xaccess(s, R_OK) >= 0)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'w':
			if (s) ret = (Xaccess(s, W_OK) >= 0)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'x':
			if (s) ret = (Xaccess(s, X_OK) >= 0)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'f':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& (st.st_mode & S_IFMT) != S_IFDIR)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'd':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& (st.st_mode & S_IFMT) == S_IFDIR)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'c':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& (st.st_mode & S_IFMT) == S_IFCHR)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'b':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& (st.st_mode & S_IFMT) == S_IFBLK)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'p':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& (st.st_mode & S_IFMT) == S_IFIFO)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'u':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& st.st_mode & S_ISUID)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'g':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& st.st_mode & S_ISGID)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'k':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& st.st_mode & S_ISVTX)
				? RET_SUCCESS : RET_FAIL;
			break;
#if	defined (BASHSTYLE) || defined (STRICTPOSIX)
	/* "test" on bash & POSIX has -e & -L option */
		case 'e':
			if (s) ret = (*s && Xlstat(s, &st) >= 0)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 'L':
#endif
		case 'h':
			if (s) ret = (*s && Xlstat(s, &st) >= 0
			&& (st.st_mode & S_IFMT) == S_IFLNK)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 's':
			if (s) ret = (*s && Xstat(s, &st) >= 0
			&& st.st_size > 0)
				? RET_SUCCESS : RET_FAIL;
			break;
		case 't':
			if (s) {
#ifdef	BASHBUG
	/* Maybe bash's bug. */
				if ((ret = isnumeric(s)) < 0) ret = 1;
#else
				if ((ret = isnumeric(s)) < 0) ret = 0;
#endif
				ret = (isatty(ret)) ? RET_SUCCESS : RET_FAIL;
			}
			else {
				(*ptrp)++;
#ifdef	BASHBUG
	/* Maybe bash's bug. */
				ret = 0;
#else
				ret = 1;
#endif
				return((isatty(ret)) ? RET_SUCCESS : RET_FAIL);
			}
			break;
		case 'z':
			if (s) ret = (!*s) ? RET_SUCCESS : RET_FAIL;
			break;
		case 'n':
			if (s) ret = (*s) ? RET_SUCCESS : RET_FAIL;
			break;
		default:
			ret = -2;
			break;
	}
	if (ret >= 0) *ptrp += 2;

	return(ret);
}

static int NEAR testsub2(argc, argv, ptrp)
int argc;
char *CONST *argv;
int *ptrp;
{
	CONST char *s, *a1, *a2;
	int ret, v1, v2;

	if (*ptrp >= argc) return(-1);
	if (argv[*ptrp][0] == '(' && !argv[*ptrp][1]) {
		(*ptrp)++;
		if ((ret = testsub3(argc, argv, ptrp, 0)) < 0)
			return(ret);
		if (*ptrp >= argc
		|| argv[*ptrp][0] != ')' || argv[*ptrp][1])
			return(-1);
		(*ptrp)++;
		return(ret);
	}
	if (*ptrp + 2 < argc) {
		s = argv[*ptrp + 1];
		a1 = argv[*ptrp];
		a2 = argv[*ptrp + 2];
		ret = -1;

		if (s[0] == '!' && s[1] == '=' && !s[2])
			ret = (strcmp(a1, a2)) ? RET_SUCCESS : RET_FAIL;
		else if (s[0] == '=' && !s[1])
			ret = (!strcmp(a1, a2)) ? RET_SUCCESS : RET_FAIL;
		else if (s[0] == '-') {
			if ((v1 = isnumeric(a1)) < 0) {
#ifdef	BASHSTYLE
	/* bash's "test" does not allow arithmetic comparison with strings */
				return(-3);
#else
				v1 = 0;
#endif
			}
			if ((v2 = isnumeric(a2)) < 0) {
#ifdef	BASHSTYLE
	/* bash's "test" does not allow arithmetic comparison with strings */
				*ptrp += 2;
				return(-3);
#else
				v2 = 0;
#endif
			}
			if (s[1] == 'e' && s[2] == 'q' && !s[3])
				ret = (v1 == v2) ? RET_SUCCESS : RET_FAIL;
			else if (s[1] == 'n' && s[2] == 'e' && !s[3])
				ret = (v1 != v2) ? RET_SUCCESS : RET_FAIL;
			else if (s[1] == 'g' && s[2] == 't' && !s[3])
				ret = (v1 > v2) ? RET_SUCCESS : RET_FAIL;
			else if (s[1] == 'g' && s[2] == 'e' && !s[3])
				ret = (v1 >= v2) ? RET_SUCCESS : RET_FAIL;
			else if (s[1] == 'l' && s[2] == 't' && !s[3])
				ret = (v1 < v2) ? RET_SUCCESS : RET_FAIL;
			else if (s[1] == 'l' && s[2] == 'e' && !s[3])
				ret = (v1 <= v2) ? RET_SUCCESS : RET_FAIL;
			else {
				(*ptrp)++;
				return(-2);
			}
		}
		if (ret >= 0) {
			*ptrp += 3;
			return(ret);
		}
	}
	if (argv[*ptrp][0] == '-' && !argv[*ptrp][2]) {
		a1 = (*ptrp + 1 < argc) ? argv[*ptrp + 1] : NULL;
		ret = testsub1(argv[*ptrp][1], a1, ptrp);
		if (ret >= -1) return(ret);
	}
	ret = (argv[*ptrp][0]) ? RET_SUCCESS : RET_FAIL;
	(*ptrp)++;

	return(ret);
}

static int NEAR testsub3(argc, argv, ptrp, lvl)
int argc;
char *CONST *argv;
int *ptrp, lvl;
{
	int ret1, ret2;

	if (lvl > 2) {
		if (*ptrp >= argc) return(-1);
		if (argv[*ptrp][0] == '!' && !argv[*ptrp][1]) {
			(*ptrp)++;
			if ((ret1 = testsub2(argc, argv, ptrp)) < 0)
				return(ret1);
			return(1 - ret1);
		}
		return(testsub2(argc, argv, ptrp));
	}
	if ((ret1 = testsub3(argc, argv, ptrp, lvl + 1)) < 0) return(ret1);
	if (*ptrp >= argc || argv[*ptrp][0] != '-'
	|| argv[*ptrp][1] != ((lvl) ? 'a' : 'o') || argv[*ptrp][2])
		return(ret1);

	(*ptrp)++;
	if ((ret2 = testsub3(argc, argv, ptrp, lvl)) < 0) return(ret2);
	ret1 = ((lvl)
		? (ret1 != RET_FAIL && ret2 != RET_FAIL)
		: (ret1 != RET_FAIL || ret2 != RET_FAIL))
			? RET_SUCCESS : RET_FAIL;

	return(ret1);
}

int posixtest(trp)
syntaxtree *trp;
{
	char *CONST *argv;
	int ret, ptr, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;

	if (argc <= 0) return(RET_FAIL);
	if (argv[0][0] == '[' && !argv[0][1]
	&& (--argc <= 0 || argv[argc][0] != ']' || argv[argc][1])) {
		fputs("] missing", stderr);
		fputnl(stderr);
		return(RET_NOTICE);
	}
	if (argc <= 1) return(RET_FAIL);
	ptr = 1;
	ret = testsub3(argc, argv, &ptr, 0);
	if (!ret && ptr < argc) ret = -1;
	if (ret < 0) {
		switch (ret) {
			case -1:
				fputs("argument expected", stderr);
				break;
			case -2:
				fprintf2(stderr, "unknown operator %k",
					argv[ptr]);
				break;
			default:
				fputs("syntax error", stderr);
				break;
		}
		fputnl(stderr);
		return(RET_NOTICE);
	}

	return(ret);
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
					execerror(cp, ER_COMNOFOUND, 1);
					cp = NULL;
				}
			}

			if (cp) {
				kanjifputs(cp, stdout);
				fputnl(stdout);
# ifdef	BASHSTYLE
				ret = RET_SUCCESS;
# endif
			}
		}
		return(ret);
	}

#ifdef	NOALIAS
	type = checktype(argv[n], &id, 0);
#else
	type = checktype(argv[n], &id, 0, 0);
#endif
	if (verboseexec) {
		fprintf2(stderr, "+ %k", argv[n]);
		for (i = n + 1; i < argc; i++)
			fprintf2(stderr, " %k", argv[i]);
		fputnl(stderr);
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
		path = strdup2(getconstvar(ENVPATH));
		setenv2(ENVPATH, DEFPATH, 1);
#if	MSDOS
		ret = exec_simplecom(trp, type, id);
#else
		ret = exec_simplecom(trp, type, id, 0);
#endif
		setenv2(ENVPATH, path, 1);
		if (path) free(path);
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
		execerror(NULL, ER_MISSARG, 1);
		return(RET_FAIL);
	}
	if (identcheck(argv[2], '\0') <= 0) {
		execerror(argv[2], ER_NOTIDENT, 0);
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

	if (ret != RET_SUCCESS) n = '?';
	else {
		n = argv[posixoptind][ptr++];
		if (n == ':' || ismsb(n) || !(cp = strchr2(optstr, n))) {
			buf[0] = n;
			buf[1] = '\0';
			n = '?';
			if (quiet) posixoptarg = buf;
			else execerror(buf, ER_BADOPTIONS, 2);
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
					execerror(buf, ER_MISSARG, 2);
				}
			}
			ptr = 0;
		}
	}

	if (posixoptarg) setenv2(ENVOPTARG, posixoptarg, 0);
	else if (ret == RET_SUCCESS) unset(ENVOPTARG, strsize(ENVOPTARG));

	buf[0] = n;
	buf[1] = '\0';
	setenv2(name, buf, 0);

	n = posixoptind;
	snprintf2(buf, sizeof(buf), "%d", n);
	setenv2(ENVOPTIND, buf, 0);
	posixoptind = n;

	return(ret);
}
#endif	/* NOPOSIXUTIL */
#endif	/* !MINIMUMSHELL && (!FD || (FD >= 2 && !_NOORIGSHELL)) */
