/*
 *	wait.h
 *
 *	definitions for waiting child
 */

#include <signal.h>
#if	!MSDOS
#include <sys/wait.h>
#endif

#ifdef	USESIGPMASK
#define	sigmask_t	sigset_t
#define	Xsigemptyset(m)	sigemptyset(&m)
#define	Xsigfillset(m)	sigfillset(&m)
#define	Xsigaddset(m,s)	sigaddset(&m, s)
#define	Xsigdelset(m,s)	sigdelset(&m, s)
#define	Xsigsetmask(m)	sigprocmask(SIG_SETMASK, &m, NULL)
#define	Xsigblock(o,m)	sigprocmask(SIG_BLOCK, &m, &o)
#else	/* !USESIGPMASK */
typedef int		sigmask_t;
#define	Xsigemptyset(m)	((m) = 0)
#define	Xsigfillset(m)	((m) = ~0)
#define	Xsigaddset(m,s)	((m) |= sigmask(s))
#define	Xsigdelset(m,s)	((m) &= ~sigmask(s))
#define	Xsigsetmask(m)	sigsetmask(m)
#define	Xsigblock(o,m)	((o) = sigblock(m))
#endif	/* !USESIGPMASK */

#ifdef	SYSV
#define	Xkillpg(p, s)	kill(-(p), s)
#else
#define	Xkillpg(p, s)	killpg(p, s)
#endif

#ifdef	USEWAITPID
typedef int		wait_pid_t;
#else
#define	wait_pid_t	union wait
#endif

#ifndef	WSTOPPED
#define	WSTOPPED	0177
#endif
#ifndef	WNOHANG
#define	WNOHANG		1
#endif
#ifndef	WUNTRACED
#define	WUNTRACED	2
#endif

#ifdef	USEWAITPID
#define	Xwait3(wp, opts, ru)	waitpid(-1, wp, opts)
#define	Xwait4(p, wp, opts, ru)	waitpid(p, wp, opts)
# ifndef	WIFSTOPPED
# define	WIFSTOPPED(x)	(((x) & 0177) == WSTOPPED)
# endif
# ifndef	WIFSIGNALED
# define	WIFSIGNALED(x)	(((x) & 0177) != WSTOPPED \
				&& ((x) & 0177) != 0)
# endif
# ifndef	WIFEXITED
# define	WIFEXITED(x)	(((x) & 0177) == 0)
# endif
# ifndef	WCOREDUMP
# define	WCOREDUMP(x)	((x) & 0200)
# endif
# ifndef	WSTOPSIG
# define	WSTOPSIG(x)	(((x) >> 8) & 0177)
# endif
# ifndef	WTERMSIG
# define	WTERMSIG(x)	((x) & 0177)
# endif
# ifndef	WEXITSTATUS
# define	WEXITSTATUS(x)	(((x) >> 8) & 0377)
# endif
#else	/* !USEWAITPID */
#define	Xwait3			wait3
#define	Xwait4			wait4
# ifndef	WIFSTOPPED
# define	WIFSTOPPED(x)	((x).w_stopval == WSTOPPED)
# endif
# ifndef	WIFSIGNALED
# define	WIFSIGNALED(x)	((x).w_stopval != WSTOPPED \
				&& (x).w_termsig != 0)
# endif
# ifndef	WIFEXITED
# define	WIFEXITED(x)	((x).w_stopval != WSTOPPED \
				&& (x).w_termsig == 0)
# endif
# ifndef	WCOREDUMP
# define	WCOREDUMP(x)	((x).w_coredump)
# endif
# ifndef	WSTOPSIG
# define	WSTOPSIG(x)	((x).w_stopsig)
# endif
# ifndef	WTERMSIG
# define	WTERMSIG(x)	((x).w_termsig)
# endif
# ifndef	WEXITSTATUS
# define	WEXITSTATUS(x)	((x).w_retcode)
# endif
#endif	/* !USEWAITPID */

#ifndef	NSIG
# ifdef	_NSIG
# define	NSIG	_NSIG
# else
#  ifdef	DJGPP
#  define	NSIG	301
#  else
#  define	NSIG	64
#  endif
# endif
#endif

#ifndef	SIG_ERR
#define	SIG_ERR		((sigcst_t)-1)
#endif
#ifndef	SIG_DFL
#define	SIG_DFL		((sigcst_t)0)
#endif
#ifndef	SIG_IGN
#define	SIG_IGN		((sigcst_t)1)
#endif
#if	!defined (SIGIOT) && defined (SIGABRT)
#define	SIGIOT		SIGABRT
#endif
#if	!defined (SIGCHLD) && defined (SIGCLD)
#define	SIGCHLD		SIGCLD
#endif
#if	!defined (SIGWINCH) && defined (SIGWINDOW)
#define	SIGWINCH	SIGWINDOW
#endif

#ifdef	FD
# ifdef	SIGALRM
extern int noalrm;
# define	sigalrm(sig)	((!noalrm && (sig)) ? SIGALRM : 0)
# else
# define	sigalrm(sig)	0
# endif
#endif	/* FD */
