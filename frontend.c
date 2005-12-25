/*
 *	frontend.c
 *
 *	frontend of terminal emulation
 */

#include "fd.h"

#ifndef	_NOPTY

#include "func.h"
#include "kanji.h"
#include "termemu.h"

#ifdef	_NOORIGSHELL
#include "termio.h"
#include "wait.h"
#else
#include "system.h"
# ifndef	NOALIAS
extern int addalias __P_((char *, char *));
extern int deletealias __P_((char *));
# endif
#endif

#ifdef	SIGALRM
#define	sigalrm()	((!noalrm) ? SIGALRM : 0)
#else
#define	sigalrm()	0
#endif

extern int internal_status;
#ifdef	SIGALRM
extern int noalrm;
#endif
extern int fdmode;
extern int fdflags;
extern int wheader;
extern int ptymode;
extern int ptymenukey;
extern p_id_t emupid;
extern int emufd;
extern int parentfd;
extern char *ptytmpfile;

static int (*lastfunc)__P_((VOID_A)) = NULL;
static u_long lockflags = 0;

static int waitpty __P_((VOID_A));
static int NEAR ptygetkey __P_((VOID_A));
static int NEAR recvvar __P_((int, char ***));
#ifndef	_NOORIGSHELL
static int NEAR recvheredoc __P_((int, heredoc_t **));
static int NEAR recvrlist __P_((int, redirectlist **));
static int NEAR recvcommand __P_((int, command_t **, syntaxtree *));
static int NEAR recvstree __P_((int, syntaxtree **, syntaxtree *));
#endif
static VOID NEAR recvchild __P_((int));


int waitstatus(pid, options, statusp)
p_id_t pid;
int options, *statusp;
{
	wait_pid_t w;
	p_id_t tmp;

	tmp = Xwait4(pid, &w, options, NULL);
	if (tmp < (p_id_t)0 || tmp != pid) return(-1);
	if (statusp) *statusp = (WIFSIGNALED(w))
		? WTERMSIG(w) + 128 : (WEXITSTATUS(w) & 0377);

	return(0);
}

static int waitpty(VOID_A)
{
	char result[MAXWINDOWS];
	int i, n, fds[MAXWINDOWS];

	if (lastfunc && (*lastfunc)() < 0) return(-1);

	for (i = 0; i < MAXWINDOWS; i++) fds[i] = ptylist[i].pipe;
	if (selectpty(-1, fds, result, 0) > 0) {
		for (i = 0; i < MAXWINDOWS; i++)
			if (result[i] && ptylist[i].pid) recvchild(i);
	}

	if (!(ptylist[win].pid)) return(0);
	if (ptylist[win].status >= 0) return(-1);
	if (waitstatus(ptylist[win].pid, WNOHANG, &n) < 0) return(0);
	ptylist[win].pid = (p_id_t)0;
	ptylist[win].status = n;

	return(-1);
}

VOID Xttyiomode(isnl)
int isnl;
{
	int dupdumbterm;

	if (!emupid) {
		syncptyout(-1, -1);
		VOID_C ttyiomode(isnl);
	}
	else {
		dupdumbterm = dumbterm;
		dumbterm = 1;
		VOID_C ttyiomode(isnl);
		dumbterm = dupdumbterm;
		if (!dumbterm) Xputterm(T_KEYPAD);
	}
}

VOID Xstdiomode(VOID_A)
{
	int dupdumbterm;

	if (!emupid) {
		syncptyout(-1, -1);
		VOID_C stdiomode();
	}
	else {
		dupdumbterm = dumbterm;
		dumbterm = 1;
		VOID_C stdiomode();
		dumbterm = dupdumbterm;
		if (!dumbterm) Xputterm(T_NOKEYPAD);
	}
}

int Xtermmode(init)
int init;
{
	int mode, dupdumbterm;

	dupdumbterm = dumbterm;
#ifndef	_NOORIGSHELL
	if (isshptymode()) dumbterm = 1;
#endif
	if (isptymode()) dumbterm = 1;
	mode = termmode(init);
	dumbterm = dupdumbterm;

	return(mode);
}


VOID Xputch2(c)
int c;
{
	if (!emupid) VOID_C putch2(c);
	else {
		sendword(emufd, TE_PUTCH2);
		sendword(emufd, c);
	}
}

VOID Xcputs2(s)
char *s;
{
	int len;

	if (!emupid) VOID_C cputs2(s);
	else if (!s) return;
	else {
		sendword(emufd, TE_CPUTS2);
		len = strlen(s);
		sendword(emufd, len);
		sendbuf(emufd, s, len);
	}
}

VOID Xputterm(n)
int n;
{
	if (!emupid) VOID_C putterm(n);
	else {
		sendword(emufd, TE_PUTTERM);
		sendword(emufd, n);
	}
}

VOID Xputterms(n)
int n;
{
	if (!emupid) VOID_C putterms(n);
	else {
		sendword(emufd, TE_PUTTERMS);
		sendword(emufd, n);
	}
}

int Xsetscroll(min, max)
int min, max;
{
	if (!emupid) return(setscroll(min, max));
	else {
		sendword(emufd, TE_SETSCROLL);
		sendword(emufd, min);
		sendword(emufd, max);

		return(0);
	}
}

VOID Xlocate(x, y)
int x, y;
{
	if (!emupid) VOID_C locate(x, y);
	else {
		sendword(emufd, TE_LOCATE);
		sendword(emufd, x);
		sendword(emufd, y);
	}
}

VOID Xtflush(VOID_A)
{
	if (!emupid) VOID_C tflush();
}

#ifdef	USESTDARGH
/*VARARGS1*/
int Xcprintf2(CONST char *fmt, ...)
#else
/*VARARGS1*/
int Xcprintf2(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *buf;
	int n;

	VA_START(args, fmt);
	n = vasprintf2(&buf, fmt, args);
	va_end(args);
	if (n < 0) error("malloc()");

	if (!emupid) VOID_C cputs2(buf);
	else {
		sendword(emufd, TE_CPUTS2);
		sendword(emufd, n);
		sendbuf(emufd, buf, n);
	}
	free(buf);

	return(n);
}

VOID Xcputnl(VOID_A)
{
	if (!emupid) VOID_C cputnl();
	else {
		sendword(emufd, TE_CPUTNL);
		sendword(emufd, 0);
	}
}

int Xkanjiputs(s)
char *s;
{
	return(Xcprintf2("%k", s));
}

VOID Xchgcolor(color, reverse)
int color, reverse;
{
	if (!emupid) VOID_C chgcolor(color, reverse);
	else {
		sendword(emufd, TE_CHGCOLOR);
		sendword(emufd, color);
		sendword(emufd, reverse);
	}
}

VOID Xmovecursor(n1, n2, c)
int n1, n2, c;
{
	if (!emupid) VOID_C movecursor(n1, n2, c);
	else {
		sendword(emufd, TE_MOVECURSOR);
		sendword(emufd, n1);
		sendword(emufd, n2);
		sendword(emufd, c);
	}
}

VOID changewin(n, pid)
int n;
p_id_t pid;
{
	int i;

	if (!emupid) return;

	sendword(emufd, TE_CHANGEWIN);
	sendword(emufd, n);
	sendbuf(emufd, &pid, sizeof(pid));
	if (pid) return;

	for (i = 0; i < MAXWINDOWS; i++) if (ptylist[i].pid) return;
	VOID_C waitstatus(emupid, 0, NULL);
	emupid = (p_id_t)0;
	killallpty();
}

VOID changewsize(h, n)
int h, n;
{
	int i;

	if (!emupid) return;

	sendword(emufd, TE_CHANGEWSIZE);
	sendword(emufd, h);
	sendword(emufd, n);
	for (i = 0; i < n; i++) sendword(emufd, winvar[i].v_fileperrow);
}

VOID insertwin(n, max)
int n, max;
{
	ptyinfo_t tmp;

	memcpy((char *)&tmp, (char *)&(ptylist[max - 1]), sizeof(ptyinfo_t));
	memmove((char *)&(ptylist[n + 1]), (char *)&(ptylist[n]),
		(max - 1 - n) * sizeof(ptyinfo_t));
	memcpy((char *)&(ptylist[n]), (char *)&tmp, sizeof(ptyinfo_t));

	if (!emupid) return;

	sendword(emufd, TE_INSERTWIN);
	sendword(emufd, n);
	sendword(emufd, max);
}

VOID deletewin(n, max)
int n, max;
{
	ptyinfo_t tmp;

	memcpy((char *)&tmp, (char *)&(ptylist[n]), sizeof(ptyinfo_t));
	memmove((char *)&(ptylist[n]), (char *)&(ptylist[n + 1]),
		(max - n) * sizeof(ptyinfo_t));
	memcpy((char *)&(ptylist[max]), (char *)&tmp, sizeof(ptyinfo_t));
	killpty(max, NULL);

	if (!emupid) return;

	sendword(emufd, TE_DELETEWIN);
	sendword(emufd, n);
	sendword(emufd, max);
}

#ifndef	_NOKANJICONV
VOID changekcode(VOID_A)
{
	if (!emupid) return;

	sendword(emufd, TE_CHANGEKCODE);
	sendword(emufd, inputkcode);
	sendword(emufd, outputkcode);
}

VOID changeinkcode(VOID_A)
{
	ptylist[win].incode = ptyinkcode;
	if (!emupid) return;

	sendword(emufd, TE_CHANGEINKCODE);
	sendword(emufd, win);
	sendword(emufd, ptyinkcode);
}

VOID changeoutkcode(VOID_A)
{
	ptylist[win].outcode = ptyoutkcode;
	if (!emupid) return;

	sendword(emufd, TE_CHANGEOUTKCODE);
	sendword(emufd, win);
	sendword(emufd, ptyoutkcode);
}
#endif	/* !_NOKANJICONV */

static int NEAR ptygetkey(VOID_A)
{
	char *cp, *str[4];
	int n, c, ch, val[4];

	for (;;) {
		n = -1;
		c = getkey2(sigalrm());
		while (lockflags & (1 << win)) {
			kbhit2(1000000L / SENSEPERSEC);
			waitpty();
		}
#ifndef	_NOORIGSHELL
		if (isshptymode()) break;
#endif
		if (c < 0 || ptymenukey < 0 || c != ptymenukey) break;

		str[0] = asprintf3(PTYAI_K, getkeysym(ptymenukey, 0));
		str[1] = PTYIC_K;
		str[2] = PTYBR_K;
		str[3] = PTYNW_K;
		val[0] = 0;
		val[1] = 1;
		val[2] = 2;
		val[3] = 3;

		n = 0;
		changewin(MAXWINDOWS, (p_id_t)-1);
		ch = selectstr(&n, (windows > 1) ? 4 : 3, 0, str, val);
		changewin(win, (p_id_t)-1);
		free(str[0]);
		if (ch != K_CR) /*EMPTY*/;
		else if (!n) break;
		else if (n == 2) {
			killpty(win, NULL);
			c = -1;
			break;
		}
		else if (n == 3) {
			VOID_C nextwin();
			c = -2;
			break;
		}
		else {
			changewin(MAXWINDOWS, (p_id_t)-1);
			cp = inputstr(PTYKC_K, 1, -1, NULL, -1);
			changewin(win, (p_id_t)-1);
			if (cp) {
				c = getkeycode(cp, 0);
				free(cp);
				if (c >= 0) break;
				warning(0, VALNG_K);
			}
		}

		rewritefile(1);
	}

	if (n >= 0) rewritefile(1);

	return(c);
}

static int NEAR recvvar(fd, varp)
int fd;
char ***varp;
{
	char *cp, **var;
	int i, c;

	if (recvbuf(fd, &var, sizeof(var)) < 0) return(-1);
	if (var) {
		if (recvbuf(fd, &c, sizeof(c)) < 0) return(-1);
		var = (char **)malloc2((c + 1) * sizeof(char *));
		for (i = 0; i < c; i++) {
			if (recvstring(fd, &cp) < 0) {
				var[i] = NULL;
				freevar(var);
				return(-1);
			}
			var[i] = cp;
		}
		var[i] = NULL;
	}
	*varp = var;

	return(0);
}

#ifndef	_NOORIGSHELL
static int NEAR recvheredoc(fd, hdpp)
int fd;
heredoc_t **hdpp;
{
	heredoc_t *hdp;

	if (recvbuf(fd, &hdp, sizeof(hdp)) < 0) return(-1);

	if (hdp) {
		hdp = (heredoc_t *)malloc2(sizeof(heredoc_t));
		if (recvbuf(fd, hdp, sizeof(*hdp)) < 0) {
			free(hdp);
			return(-1);
		}
		hdp -> eof = hdp -> filename = hdp -> buf = NULL;
		if (recvstring(fd, &(hdp -> eof)) < 0
		|| recvstring(fd, &(hdp -> filename)) < 0
		|| recvstring(fd, &(hdp -> buf)) < 0) {
			freeheredoc(hdp, 0);
			return(-1);
		}
	}
	*hdpp = hdp;

	return(0);
}

static int NEAR recvrlist(fd, rpp)
int fd;
redirectlist **rpp;
{
	redirectlist *rp;
	int n;

	if (recvbuf(fd, &rp, sizeof(rp)) < 0) return(-1);

	if (rp) {
		rp = (redirectlist *)malloc2(sizeof(redirectlist));
		if (recvbuf(fd, rp, sizeof(*rp)) < 0) {
			free(rp);
			return(-1);
		}
		rp -> filename = NULL;
		rp -> next = NULL;
		if (rp -> type & MD_HEREDOC)
			n = recvheredoc(fd, (heredoc_t **)&(rp -> filename));
		else n = recvstring(fd, &(rp -> filename));
		if (n < 0 || recvrlist(fd, &(rp -> next)) < 0) {
			freerlist(rp, 0);
			return(-1);
		}
	}
	*rpp = rp;

	return(0);
}

static int NEAR recvcommand(fd, commp, trp)
int fd;
command_t **commp;
syntaxtree *trp;
{
	command_t *comm;
	char **argv;
	int n;

	if (recvbuf(fd, &comm, sizeof(comm)) < 0) return(-1);

	if (comm) {
		if (trp -> flags & ST_NODE)
			return(recvstree(fd, (syntaxtree **)commp, trp));

		comm = (command_t *)malloc2(sizeof(command_t));
		if (recvbuf(fd, comm, sizeof(*comm)) < 0) {
			free(comm);
			return(-1);
		}
		argv = comm -> argv;
		comm -> argv = NULL;
		comm -> redp = NULL;
		if (!argv) n = 0;
		else if (!isstatement(comm)) n = recvvar(fd, &(comm -> argv));
		else n = recvstree(fd, (syntaxtree **)&(comm -> argv), trp);
		if (n < 0 || recvrlist(fd, &(comm -> redp)) < 0) {
			freecomm(comm, 0);
			return(-1);
		}
	}
	*commp = comm;

	return(0);
}

static int NEAR recvstree(fd, trpp, parent)
int fd;
syntaxtree **trpp, *parent;
{
	syntaxtree *trp;

	if (recvbuf(fd, &trp, sizeof(trp)) < 0) return(-1);

	if (trp) {
		trp = newstree(parent);
		if (recvbuf(fd, trp, sizeof(*trp)) < 0) {
			free(trp);
			return(-1);
		}
		trp -> comm = NULL;
		trp -> next = NULL;
		if (recvcommand(fd, &(trp -> comm), trp) < 0
		|| recvstree(fd, &(trp -> next), trp) < 0) {
			freestree(trp);
			return(-1);
		}
	}
	*trpp = trp;

	return(0);
}
#endif	/* !_NOORIGSHELL */

static VOID NEAR recvchild(w)
int w;
{
#ifndef	_NOORIGSHELL
	syntaxtree *trp;
#endif
#ifndef	_NOARCHIVE
	launchtable launch;
	archivetable arch;
#endif
#ifdef	_USEDOSEMU
	devinfo dev;
#endif
	bindtable bind;
	keyseq_t key;
	char *cp, *func1, *func2, ***varp, **var;
	u_char uc;
	int n, fd, val;

	fd = ptylist[w].pipe;
	if (recvbuf(fd, &uc, sizeof(uc)) < 0) return;

	switch(uc) {
		case TE_SETVAR:
			if (recvbuf(fd, &varp, sizeof(varp)) < 0
			|| recvvar(fd, &var) < 0)
				break;
			freevar(*varp);
			*varp = var;
			break;
		case TE_PUSHVAR:
			if (recvbuf(fd, &varp, sizeof(varp)) < 0
			|| recvstring(fd, &cp) < 0)
				break;
			n = countvar(*varp);
			(*varp) = (char **)realloc2(*varp,
				(n + 1 + 1) * sizeof(char *));
			memmove((char *)&((*varp)[1]), (char *)&((*varp)[0]),
				n * sizeof(char *));
			(*varp)[0] = cp;
			(*varp)[n + 1] = NULL;
			break;
		case TE_POPVAR:
			if (recvbuf(fd, &varp, sizeof(varp)) < 0
			|| (n = countvar(*varp)) <= 0)
				break;
			free((*varp)[0]);
			memmove((char *)&((*varp)[0]), (char *)&((*varp)[1]),
				n * sizeof(char *));
			break;
		case TE_CHDIR:
			if (recvstring(fd, &cp) < 0 || !cp) break;
			VOID_C chdir2(cp);
			free(cp);
			break;
#ifdef	_NOORIGSHELL
		case TE_PUTSHELLVAR:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvstring(fd, &func1) < 0)
				break;
			if (recvstring(fd, &cp) < 0 || !cp) {
				if (func1) free(func1);
				break;
			}
			setenv2(cp, func1, n);
			free(cp);
			if (func1) free(func1);
			evalenv();
			break;
		case TE_ADDFUNCTION:
			if (recvvar(fd, &var) < 0) break;
			if (recvstring(fd, &cp) < 0 || !cp) {
				freevar(var);
				break;
			}
			VOID_C addfunction(cp, var);
			break;
		case TE_DELETEFUNCTION:
			if (recvstring(fd, &cp) < 0 || !cp) break;
			VOID_C deletefunction(cp);
			free(cp);
			break;
#else	/* !_NOORIGSHELL */
		case TE_PUTEXPORTVAR:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvstring(fd, &cp) < 0 || !cp)
				break;
			if (putexportvar(cp, n) < 0) free(cp);
			evalenv();
			break;
		case TE_PUTSHELLVAR:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvstring(fd, &cp) < 0 || !cp)
				break;
			if (putshellvar(cp, n) < 0) free(cp);
			evalenv();
			break;
		case TE_UNSET:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvstring(fd, &cp) < 0 || !cp)
				break;
			VOID_C unset(cp, n);
			evalenv();
			free(cp);
			break;
		case TE_SETEXPORT:
			if (recvstring(fd, &cp) < 0 || !cp) break;
			VOID_C setexport(cp);
			free(cp);
			break;
		case TE_SETRONLY:
			if (recvstring(fd, &cp) < 0 || !cp) break;
			VOID_C setronly(cp);
			free(cp);
			break;
		case TE_SETSHFLAG:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvbuf(fd, &val, sizeof(val)) < 0)
				break;
			setshflag(n, val);
			errorexit = tmperrorexit;
			break;
		case TE_ADDFUNCTION:
			if (recvstring(fd, &cp) < 0) break;
			if (recvstree(fd, &trp, NULL) < 0 || !trp) {
				if (cp) free(cp);
				break;
			}
			if (cp) setshfunc(cp, trp);
			break;
		case TE_DELETEFUNCTION:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvstring(fd, &cp) < 0 || !cp)
				break;
			VOID_C unsetshfunc(cp, n);
			free(cp);
			break;
#endif	/* !_NOORIGSHELL */
#if	defined (_NOORIGSHELL) || !defined (NOALIAS)
		case TE_ADDALIAS:
			if (recvstring(fd, &func1) < 0) break;
			if (recvstring(fd, &cp) < 0 || !cp) {
				if (func1) free(func1);
				break;
			}
			VOID_C addalias(cp, func1);
			break;
		case TE_DELETEALIAS:
			if (recvstring(fd, &cp) < 0 || !cp) break;
			VOID_C deletealias(cp);
			free(cp);
			break;
#endif	/* _NOORIGSHELL || !NOALIAS */
		case TE_SETHISTORY:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvbuf(fd, &val, sizeof(val)) < 0
			|| recvstring(fd, &cp) < 0 || !cp)
				break;
			entryhist(n, cp, val);
			free(cp);
			break;
		case TE_ADDKEYBIND:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvbuf(fd, &bind, sizeof(bind)) < 0
			|| recvstring(fd, &func1) < 0)
				break;
			if (recvstring(fd, &func2) < 0) {
				if (func1) free(func1);
				break;
			}
			if (recvstring(fd, &cp) < 0) {
				if (func1) free(func1);
				if (func2) free(func2);
				break;
			}
			VOID_C addkeybind(n, &bind, func1, func2, cp);
			break;
		case TE_DELETEKEYBIND:
			if (recvbuf(fd, &n, sizeof(n)) < 0) break;
			deletekeybind(n);
			break;
		case TE_SETKEYSEQ:
			if (recvbuf(fd, &(key.code), sizeof(key.code)) < 0
			|| recvbuf(fd, &(key.len), sizeof(key.len)) < 0
			|| recvstring(fd, &(key.str)) < 0)
				break;
			setkeyseq(key.code, key.str, key.len);
			break;
#ifndef	_NOARCHIVE
		case TE_ADDLAUNCH:
			launch.ext = launch.comm = NULL;
			launch.format = launch.lignore = launch.lerror = NULL;
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvbuf(fd, &launch, sizeof(launch)) < 0
			|| recvstring(fd, &(launch.ext)) < 0
			|| recvstring(fd, &(launch.comm)) < 0
			|| recvvar(fd, &(launch.format)) < 0
			|| recvvar(fd, &(launch.lignore)) < 0
			|| recvvar(fd, &(launch.lerror)) < 0) {
				freelaunch(&launch);
				break;
			}
			addlaunch(n, &launch);
			break;
		case TE_DELETELAUNCH:
			if (recvbuf(fd, &n, sizeof(n)) < 0) break;
			deletelaunch(n);
			break;
		case TE_ADDARCH:
			arch.ext = arch.p_comm = arch.u_comm = NULL;
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvbuf(fd, &(arch.flags), sizeof(arch.flags)) < 0
			|| recvstring(fd, &(arch.ext)) < 0
			|| recvstring(fd, &(arch.p_comm)) < 0
			|| recvstring(fd, &(arch.u_comm)) < 0) {
				freearch(&arch);
				break;
			}
			addarch(n, &arch);
			break;
		case TE_DELETEARCH:
			if (recvbuf(fd, &n, sizeof(n)) < 0) break;
			deletearch(n);
			break;
#endif	/* !_NOARCHIVE */
#ifdef	_USEDOSEMU
		case TE_INSERTDRV:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvbuf(fd, &dev, sizeof(dev)) < 0
			|| recvstring(fd, &(dev.name)) < 0)
				break;
			insertdrv(n, &dev);
			break;
		case TE_DELETEDRV:
			if (recvbuf(fd, &n, sizeof(n)) < 0) break;
			deletedrv(n);
			break;
#endif	/* _USEDOSEMU */
		case TE_LOCKFRONT:
			lockflags |= (1 << w);
			if (!emupid) break;
			sendword(emufd, TE_LOCKBACK);
			sendword(emufd, w);
			break;
		case TE_UNLOCKFRONT:
			lockflags &= ~(1 << w);
			if (!emupid) break;
			sendword(emufd, TE_UNLOCKBACK);
			sendword(emufd, w);
			break;
		case TE_SAVETTYIO:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvbuf(fd, &val, sizeof(val)) < 0) break;
			if (!val) cp = NULL;
			else {
				cp = malloc2(val);
				if (recvbuf(fd, cp, val) < 0) {
					free(cp);
					break;
				}
			}
			if (n >= 0) {
				if (duptty[n]) free(duptty[n]);
				duptty[n] = cp;
			}
			else if (cp) free(cp);
			break;
		case TE_CHANGESTATUS:
			if (recvbuf(fd, &n, sizeof(n)) < 0) break;
			ptylist[w].status = n;
			break;
		default:
			break;
	}
}

int frontend(VOID_A)
{
	int n, ch, wastty, status;

	ptylist[win].status = -1;
	n = sigvecset(1);
	lastfunc = keywaitfunc;
	keywaitfunc = waitpty;
	changewsize(wheader, windows);
	changewin(win, ptylist[win].pid);

	if (!(wastty = isttyiomode)) Xttyiomode(1);
	while ((ch = ptygetkey()) >= 0) sendword(emufd, ch);
	if (!wastty) Xstdiomode();

	if (ch < -1) {
		status = 0;
		internal_status = FNC_CANCEL;
	}
	else {
		status = ptylist[win].status;
		if (!(ptylist[win].pid)) {
			ptylist[win].status = -1;
			recvchild(win);
			killpty(win, &status);
		}
		internal_status = (status < 128) ? status : FNC_FAIL;
	}

	sigvecset(n);
	keywaitfunc = lastfunc;
	if (!(ptylist[win].pid)) {
		changewin(win, (p_id_t)0);
		if (!emupid && ptytmpfile) {
			rmtmpfile(ptytmpfile);
			free(ptytmpfile);
			ptytmpfile = NULL;
		}
	}
	changewin(MAXWINDOWS, (p_id_t)-1);
	setlinecol();

	return(status);
}
#endif	/* !_NOPTY */
