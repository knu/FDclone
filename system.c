/*
 *	system.c
 *
 *	Command Line Analysis
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "machine.h"
#include "kctype.h"

#if	!defined (_NOORIGSHELL) && (!defined (FD) || (FD >= 2))

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#if	MSDOS
#include <dos.h>
#define	FR_CARRY	00001
#define	VOL_FAT32	"FAT32"
# ifdef	DJGPP
# include <dpmi.h>
# include <go32.h>
# include <sys/farptr.h>
#  if	(DJGPP >= 2)
#  include <libc/dosio.h>
#  else
#  define	__dpmi_regs	_go32_dpmi_registers
#  define	__dpmi_int(v,r)	((r) -> x.ss = (r) -> x.sp = 0, \
				_go32_dpmi_simulate_int(v, r))
#  define	__tb	_go32_info_block.linear_address_of_transfer_buffer
#  define	__tb_offset	(__tb & 15)
#  define	__tb_segment	(__tb / 16)
#  endif
# define	PTR_FAR(ptr)		((u_long)(__tb))
# define	PTR_SEG(ptr)		(__tb_segment)
# define	PTR_OFF(ptr, ofs)	(__tb_offset + (ofs))
# else	/* !DJGPP */
#  ifdef	__TURBOC__	/* Oops!! Borland C++ has not x.bp !! */
typedef union DPMI_REGS {
	struct XREGS {
		u_short ax, bx, cx, dx, si, di, bp, flags;
	} x;
	struct HREGS {
		u_char al, ah, bl, bh, cl, ch, dl, dh;
	} h;
} __dpmi_regs;
#  else
typedef union REGS	__dpmi_regs;
#  endif
# define	__attribute__(x)
# define	PTR_FAR(ptr)		(((u_long)FP_SEG(ptr) << 4) + \
					FP_OFF(ptr))
# define	PTR_SEG(ptr)		FP_SEG(ptr)
# define	PTR_OFF(ptr, ofs)	FP_OFF(ptr)
# endif	/* !DJGPP */
#endif

#ifdef	DEBUG
#undef	JOBVERBOSE
#define	PSIGNALSTYLE
#define	SHOWSTREE
#endif

/*
 *	Notes: extension from original Bourne shell
 *
 *	1. pathname globing: wildcard `**' matches any directories recursively.
 *	2. `export' & `readonly' allows arguments with `=value'.
 *	3. redirector `<>' and `><' mean redirection for read and write.
 *	4. command hash is always valid. Instead,
 *	   -h option means to check command-path ahead for pipe line,
 *	   and to check command-path when a function is defined (not executed).
 *	5. job control: `jobs', `fg', `bg', ^Z/^Y(suspend) are valid.
 *	   -m option means to validate the job control.
 *	   this option is valid by default for the interactive shell.
 *	6. builtin command: `echo', `test', `kill' is builtin.
 *	7. job name: `%' introduces job name in `fg', `bg', `wait', `kill'.
 *	   `%%', `%', '%+' mean the current job.
 *	   `%-' means the previous job.
 *	   `%n' means the job which has job number n.
 *	   `%string' means the job whose command begins with `string'.
 *	8. builtin commands of COMMAND.COM for MS-DOS:
 *	   `rem', `dir', `mkdir(md)', `rmdir(rd)', `erase(del)',
 *	   `rename(ren)', `copy' is builtin on MS-DOS.
 *	9. aliases: `alias', `unalias' is builtin.
 *	10. optional POSIX utilities are added.
 */

/* #define BASHSTYLE		; rather near to bash style */
/* #define PSIGNALSTYLE		; based upon psignal(3) messages */
/* #define NOJOB		; not use job control */
/* #define CHILDSTATEMENT	; make any statement child for suspend */
/* #define NOALIAS		; not use alias */
/* #define DOSCOMMAND		; emulate builtin commands of COMMAND.COM */
/* #define USEFAKEPIPE		; use DOS-like pipe instead of pipe(2) */
/* #define SHOWSTREE		; show syntax tree with -n option */
/* #define NOPOSIXUTIL		; not use POSIX utilities */
/* #define STRICTPOSIX		; keep POSIX strictly */

#if	MSDOS
#include <process.h>
#include <io.h>
#include "unixemu.h"
#define	Xexit		exit
#define	Xexit2		exit2
#define	getdtablesize()	20
#define	DEFPATH		":"
#else	/* !MSDOS */
#include <pwd.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/time.h>
# ifdef	USERESOURCEH
# include <sys/resource.h>
# endif
# ifdef	USETIMESH
# include <sys/times.h>
# endif
# ifdef	USEULIMITH
# include <ulimit.h>
# endif
#define	Xexit		_exit
#define	Xexit2(n)	((ttypgrp >= 0 && ttypgrp == getpid()) \
			? (VOID)exit2(n) : (VOID)exit(n))
#define	DEFPATH		":/bin:/usr/bin"
#define	NULLDEVICE	"/dev/null"
# ifdef	USETERMIOS
# include <termios.h>
typedef struct termios	termioctl_t;
typedef struct termios	ldiscioctl_t;
# define	tioctl(d, r, a)	((r) \
				? tcsetattr(d, (r) - 1, a) : tcgetattr(d, a))
# define	ldisc(a)	((a).c_line)
# define	REQGETP		0
# define	REQSETP		(TCSAFLUSH + 1)
# define	REQGETD		0
# define	REQSETD		(TCSADRAIN + 1)
# else	/* !USETERMIOS */
# define	tioctl		ioctl
#  ifdef	USETERMIO
#  include <termio.h>
typedef struct termio	termioctl_t;
typedef struct termio	ldiscioctl_t;
#  define	ldisc(a)	((a).c_line)
#  define	REQGETP		TCGETA
#  define	REQSETP		TCSETAF
#  define	REQGETD		TCGETA
#  define	REQSETD		TCSETAW
#  else	/* !USETERMIO */
#  include <sgtty.h>
typedef struct sgttyb	termioctl_t;
typedef int 		ldiscioctl_t;
#  define	ldisc(a)	(a)
#  define	REQGETP		TIOCGETP
#  define	REQSETP		TIOCSETP
#  define	REQGETD		TIOCGETD
#  define	REQSETD		TIOCSETD
#  endif	/* !USETERMIO */
# endif	/* !USETERMIOS */
#endif	/* !MSDOS */

#ifdef	NOVOID
#define	VOID
#define	VOID_T	int
#define	VOID_P	char *
#else
#define	VOID	void
#define	VOID_T	void
#define	VOID_P	void *
#endif

#include "term.h"

#ifdef	USESIGPMASK
typedef	sigset_t	sigmask_t;
#define	Xsigemptyset(m)	sigemptyset(&m);
#define	Xsigaddset(m,s)	sigaddset(&m, s);
#define	Xsigdelset(m,s)	sigdelset(&m, s);
#define	Xsigsetmask(m)	sigprocmask(SIG_SETMASK, &m, NULL)
#define	Xsigblock(o,m)	sigprocmask(SIG_BLOCK, &m, &o)
#else	/* !USESIGPMASK */
typedef int		sigmask_t;
#define	Xsigemptyset(m)	((m) = 0);
#define	Xsigaddset(m,s)	((m) |= sigmask(s));
#define	Xsigdelset(m,s)	((m) &= ~sigmask(s));
#define	Xsigsetmask(m)	sigsetmask(m)
#define	Xsigblock(o,m)	((o) = sigblock(m))
#endif	/* !USESIGPMASK */

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

#ifdef	GETPGRPVOID
#define	getpgroup()	getpgrp()
#else
#define	getpgroup()	getpgrp(0)
#endif
#ifdef	USESETPGID
#define	setpgroup(p, g)	setpgid(p, g)
#else
#define	setpgroup(p, g)	setpgrp(p, g)
#endif

#ifdef	TIOCGPGRP
#define	gettcpgrp(f, g)	((ioctl(f, TIOCGPGRP, &g) < 0) ? (g = -1) : g)
#else
#define	gettcpgrp(f, g)	(g = tcgetpgrp(f))
#endif
#ifdef	TIOCSPGRP
#define	settcpgrp(f, g)	ioctl(f, TIOCSPGRP, &(g))
#else
#define	settcpgrp(f, g)	tcsetpgrp(f, g)
#endif

#ifdef	USESETVBUF
#define	setnbuf(f)	setvbuf(f, NULL, _IONBF, 0)
#define	setlbuf(f)	setvbuf(f, NULL, _IOLBF, 0)
#else
#define	setnbuf(f)	setbuf(f, NULL)
#define	setlbuf(f)	setlinebuf(f)
#endif

#if	!MSDOS
# ifdef	USEWAITPID
# define	Xwait3(wp, opts, ru)	waitpid(-1, wp, opts)
# define	Xwait4(p, wp, opts, ru)	waitpid(p, wp, opts)
typedef int		wait_t;
#  ifndef	WIFSTOPPED
#  define	WIFSTOPPED(x)	(((x) & 0177) == WSTOPPED)
#  endif
#  ifndef	WIFSIGNALED
#  define	WIFSIGNALED(x)	(((x) & 0177) != WSTOPPED \
				&& ((x) & 0177) != 0)
#  endif
#  ifndef	WIFEXITED
#  define	WIFEXITED(x)	(((x) & 0177) == 0)
#  endif
#  ifndef	WCOREDUMP
#  define	WCOREDUMP(x)	((x) & 0200))
#  endif
#  ifndef	WSTOPSIG
#  define	WSTOPSIG(x)	(((x) >> 8) & 0177)
#  endif
#  ifndef	WTERMSIG
#  define	WTERMSIG(x)	((x) & 0177)
#  endif
#  ifndef	WEXITSTATUS
#  define	WEXITSTATUS(x)	(((x) >> 8) & 0377)
#  endif
# else	/* !USEWAITPID */
# define	Xwait3			wait3
# define	Xwait4			wait4
typedef union wait	wait_t;
#  ifndef	WIFSTOPPED
#  define	WIFSTOPPED(x)	((x).w_stopval == WSTOPPED)
#  endif
#  ifndef	WIFSIGNALED
#  define	WIFSIGNALED(x)	((x).w_stopval != WSTOPPED \
				&& (x).w_termsig != 0)
#  endif
#  ifndef	WIFEXITED
#  define	WIFEXITED(x)	((x).w_stopval != WSTOPPED \
				&& (x).w_termsig == 0)
#  endif
#  ifndef	WCOREDUMP
#  define	WCOREDUMP(x)	((x).w_coredump)
#  endif
#  ifndef	WSTOPSIG
#  define	WSTOPSIG(x)	((x).w_stopsig)
#  endif
#  ifndef	WTERMSIG
#  define	WTERMSIG(x)	((x).w_termsig)
#  endif
#  ifndef	WEXITSTATUS
#  define	WEXITSTATUS(x)	((x).w_retcode)
#  endif
# endif	/* !USEWAITPID */
#endif	/* !MSDOS */

#include "pathname.h"
#include "system.h"

#ifdef	NOERRNO
extern int errno;
#endif
#ifdef	LSI_C
extern u_char _openfile[];
#endif
#ifndef	DECLERRLIST
extern char *sys_errlist[];
#endif

#ifdef	FD
extern VOID main_fd __P_((char *));
extern int sigvecset __P_((int));
extern char *Xgetwd __P_((char *));
extern int Xstat __P_((char *, struct stat *));
extern int Xlstat __P_((char *, struct stat *));
# if	MSDOS || !defined (_NODOSDRIVE)
extern int _dospath __P_((char *));
# endif
extern int Xaccess __P_((char *, int));
extern int Xunlink __P_((char *));
# if	MSDOS && defined (_NOROCKRIDGE) && defined (_NOUSELFN)
#define	Xopen		open
# else
extern int Xopen __P_((char *, int, int));
# endif
# ifdef	_NODOSDRIVE
#define	Xclose		close
#define	Xread		read
#define	Xwrite		write
#define	Xfdopen		fdopen
#define	Xfclose		fclose
# else	/* !_NODOSDRIVE */
extern int Xclose __P_((int));
extern int Xread __P_((int, char *, int));
extern int Xwrite __P_((int, char *, int));
extern FILE *Xfdopen __P_((int, char*));
extern int Xfclose __P_((FILE *));
# define	DOSFDOFFSET	(1 << (8 * sizeof(int) - 2))
# endif	/* !_NODOSDRIVE */
# if	MSDOS && defined (_NOUSELFN)
#  ifdef	DJGPP
#  define	_Xmkdir(p, m)	(mkdir(p, m) ? -1 : 0)
#  else
extern int _Xmkdir __P_((char *, int));
#  endif
# define	_Xrmdir(p)	(rmdir(p) ? -1 : 0)
# else	/* !MSDOS || !_NOUSELFN */
#  if	!MSDOS && defined (_NODOSDRIVE) && defined (_NOKANJICONV)
#  define	_Xmkdir		mkdir
#  define	_Xrmdir		rmdir
#  else
extern int _Xmkdir __P_((char *, int));
extern int _Xrmdir __P_((char *));
#  endif
# endif	/* !MSDOS || !_NOUSELFN */
extern int checkbuiltin __P_((char *));
extern int checkinternal __P_((char *));
extern int execbuiltin __P_((int, int, char *[]));
extern int execinternal __P_((int, int, char *[]));
# ifndef	_NOCOMPLETE
extern int completebuiltin __P_((char *, int, int, char ***));
extern int completeinternal __P_((char *, int, int, char ***));
# endif
extern VOID evalenv __P_((VOID_A));
extern char *inputshellstr __P_((char *, int, int, char *));
extern int entryhist __P_((int, char *, int));
extern int loadhistory __P_((int, char *));
extern int savehistory __P_((int, char *));
extern int savehist;
extern int internal_status;
extern char *origpath;
extern int inruncom;
extern int fd_restricted;
#else	/* !FD */
# ifdef	__TURBOC__
extern unsigned _stklen = 0x6000;
# endif
# ifdef	DJGPP
char *Xgetwd __P_((char *));
# else
#  ifdef	USEGETWD
#  define	Xgetwd		(char *)getwd
#  else
#  define	Xgetwd(p)	(char *)getcwd(p, MAXPATHLEN)
#  endif
# endif
#define	_dospath(s)	(isalpha(*(s)) && (s)[1] == ':')
#define	Xaccess(p, m)	(access(p, m) ? -1 : 0)
#define	Xunlink		unlink
#define	Xopen		open
#define	Xclose		close
#define	Xread		read
#define	Xwrite		write
#define	Xfdopen		fdopen
#define	Xfclose		fclose
#define	_Xrmdir(p)	(rmdir(p) ? -1 : 0)
# if	MSDOS
#  ifdef	DJGPP
#  define	_Xmkdir(p, m)	(mkdir(p, m) ? -1 : 0)
#  else
int _Xmkdir __P_((char *, int));
#  endif
# define	Xstat(f, s)	(stat(f, s) ? -1 : 0)
# define	Xlstat(f, s)	(stat(f, s) ? -1 : 0)
static char *deftmpdir = "\\";
# else	/* !MSDOS */
# define	_Xmkdir		mkdir
# define	Xstat		stat
# define	Xlstat		lstat
static char *deftmpdir = "/tmp";
# endif	/* !MSDOS */
static char *tmpfilename = NULL;
#endif	/* !FD */

#ifndef	O_BINARY
#define	O_BINARY	0
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
#ifndef	DEV_BSIZE
#define	DEV_BSIZE	512
#endif
#ifndef	RLIM_INFINITY
#define	RLIM_INFINITY	0x7fffffff
#endif
#ifndef	RLIMIT_FSIZE
#define	RLIMIT_FSIZE	255
#endif
#ifndef	UL_GETFSIZE
#define	UL_GETFSIZE	1
#endif
#ifndef	UL_SETFSIZE
#define	UL_SETFSIZE	2
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
#ifndef	FD_CLOEXEC
#define	FD_CLOEXEC	1
#endif
#ifndef	EPERM
#define	EPERM		EACCES
#endif
#ifndef	EISDIR
#define	EISDIR		EACCES
#endif

#if	!defined (SIGCHLD) && defined (SIGCLD)
#define	SIGCHLD		SIGCLD
#endif
#if	!defined (ENOTEMPTY) && defined (ENFSNOTEMPTY)
#define	ENOTEMPTY	ENFSNOTEMPTY
#endif
#if	!defined (RLIMIT_NOFILE) && defined (RLIMIT_OFILE)
#define	RLIMIT_NOFILE	RLIMIT_OFILE
#endif
#if	!defined (RLIMIT_VMEM) && defined (RLIMIT_AS)
#define	RLIMIT_VMEM	RLIMIT_AS
#endif

#ifndef	NOFILE
# ifdef	OPEN_MAX
# define	NOFILE	OPEN_MAX
# else
#  if	MSDOS
#  define	NOFILE	20
#  else
#  define	NOFILE	64
#  endif
# endif
#endif

#ifndef	NSIG
# ifdef	_NSIG
# define	NSIG	_NSIG
# else
# define	NSIG	64
# endif
#endif

#ifndef	CLK_TCK
# ifdef	HZ
# define	CLK_TCK	HZ
# else
# define	CLK_TCK	60
# endif
#endif

extern int kanjifputs __P_((char *, FILE *));

#ifdef	DOSCOMMAND
extern int doscomdir __P_((int, char *[]));
extern int doscommkdir __P_((int, char *[]));
extern int doscomrmdir __P_((int, char *[]));
extern int doscomerase __P_((int, char *[]));
extern int doscomrename __P_((int, char *[]));
extern int doscomcopy __P_((int, char *[]));
extern int doscomcls __P_((int, char *[]));
#endif	/* DOSCOMMAND */

extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strdup2 __P_((char *));
extern char *strncpy2 __P_((char *, char *, int));
#ifdef	FD
# if	!defined (LSI_C) && defined (_NODOSDRIVE)
# define	Xdup		dup
# define	Xdup2		dup2
# else
extern int Xdup __P_((int));
extern int Xdup2 __P_((int, int));
# endif
extern VOID freevar __P_((char **));
extern int mktmpdir __P_((char *));
extern int rmtmpdir __P_((char *));
extern int mktmpfile __P_((char *, char *));
extern int rmtmpfile __P_((char *));
# if	MSDOS
extern int setcurdrv __P_((int, int));
# endif
extern int chdir3 __P_((char *));
extern char *ascnumeric __P_((char *, long, int, int));
extern int setenv2 __P_((char *, char *));
#else	/* !FD */
# ifndef	LSI_C
# define	Xdup		dup
# define	Xdup2		dup2
# else
static int NEAR Xdup __P_((int));
static int NEAR Xdup2 __P_((int, int));
# endif
int strncpy3 __P_((char *, char *, int *, int));	/* for kanji.c */
char *strstr2 __P_((char *, char *));			/* for kanji.c */
VOID freevar __P_((char **));
static int NEAR mktmpdir __P_((char *));
static int NEAR rmtmpdir __P_((char *));
static int NEAR mktmpfile __P_((char *, char *));
static int NEAR rmtmpfile __P_((char *));
# if	MSDOS
#  ifdef	DJGPP
static int NEAR dos_putpath __P_((char *, int));
#  endif
static int NEAR int21call __P_((__dpmi_regs *, struct SREGS *));
static int NEAR setcurdrv __P_((int, int));
int chdir3 __P_((char *));
VOID getinfofs __P_((char *, long *, long *));
char *realpath2 __P_((char *, char *, int));
long getblocksize __P_((char *));
# else	/* !MSDOS */
#define	chdir3(p)	(chdir(p) ? -1 : 0)
# endif	/* !MSDOS */
static char *NEAR ascnumeric __P_((char *, long, int, int));
static int NEAR setenv2 __P_((char *, char *));
#endif	/* !FD */
static VOID NEAR setsignal __P_((VOID_A));
static VOID NEAR resetsignal __P_((int));
static VOID NEAR exectrapcomm __P_((VOID_A));
static int NEAR trap_common __P_((int));
#ifdef	SIGHUP
static int trap_hup __P_((VOID_A));
#endif
#ifdef	SIGINT
static int trap_int __P_((VOID_A));
#endif
#ifdef	SIGQUIT
static int trap_quit __P_((VOID_A));
#endif
#ifdef	SIGILL
static int trap_ill __P_((VOID_A));
#endif
#ifdef	SIGTRAP
static int trap_trap __P_((VOID_A));
#endif
#ifdef	SIGIOT
static int trap_iot __P_((VOID_A));
#else
# ifdef	SIGABRT
static int trap_abrt __P_((VOID_A));
# endif
#endif
#ifdef	SIGEMT
static int trap_emt __P_((VOID_A));
#endif
#ifdef	SIGFPE
static int trap_fpe __P_((VOID_A));
#endif
#ifdef	SIGBUS
static int trap_bus __P_((VOID_A));
#endif
#ifdef	SIGSEGV
static int trap_segv __P_((VOID_A));
#endif
#ifdef	SIGSYS
static int trap_sys __P_((VOID_A));
#endif
#ifdef	SIGPIPE
static int trap_pipe __P_((VOID_A));
#endif
#ifdef	SIGALRM
static int trap_alrm __P_((VOID_A));
#endif
#ifdef	SIGTERM
static int trap_term __P_((VOID_A));
#endif
#ifdef	SIGSTKFLT
static int trap_stkflt __P_((VOID_A));
#endif
#ifdef	SIGURG
static int trap_urg __P_((VOID_A));
#endif
#ifdef	SIGSTOP
static int trap_stop __P_((VOID_A));
#endif
#ifdef	SIGTSTP
static int trap_tstp __P_((VOID_A));
#endif
#ifdef	SIGCONT
static int trap_cont __P_((VOID_A));
#endif
#ifdef	SIGCHLD
static int trap_chld __P_((VOID_A));
#endif
#ifdef	SIGTTIN
static int trap_ttin __P_((VOID_A));
#endif
#ifdef	SIGTTOU
static int trap_ttou __P_((VOID_A));
#endif
#ifdef	SIGIO
static int trap_io __P_((VOID_A));
#else
# ifdef	SIGPOLL
static int trap_poll __P_((VOID_A));
# endif
#endif
#ifdef	SIGXCPU
static int trap_xcpu __P_((VOID_A));
#endif
#ifdef	SIGXFSZ
static int trap_xfsz __P_((VOID_A));
#endif
#ifdef	SIGVTALRM
static int trap_vtalrm __P_((VOID_A));
#endif
#ifdef	SIGPROF
static int trap_prof __P_((VOID_A));
#endif
#ifdef	SIGWINCH
static int trap_winch __P_((VOID_A));
#endif
#ifdef	SIGLOST
static int trap_lost __P_((VOID_A));
#endif
#ifdef	SIGINFO
static int trap_info __P_((VOID_A));
#endif
#ifdef	SIGPWR
static int trap_pwr __P_((VOID_A));
#endif
#ifdef	SIGUSR1
static int trap_usr1 __P_((VOID_A));
#endif
#ifdef	SIGUSR2
static int trap_usr2 __P_((VOID_A));
#endif
static VOID NEAR syntaxerror __P_((char *, int));
static VOID NEAR execerror __P_((char *[], char *, int));
static VOID NEAR doperror __P_((char *[], char *));
static long NEAR isnumeric __P_((char *));
static VOID NEAR fputlong __P_((long, FILE *));
#if	!MSDOS
static int NEAR closeonexec __P_((int));
static VOID NEAR dispsignal __P_((int, int, FILE *));
#ifndef	NOJOB
static int NEAR gettermio __P_((long));
static VOID NEAR dispjob __P_((int, FILE *));
static int NEAR searchjob __P_((long, int *));
static int NEAR getjob __P_((char *));
static int NEAR stackjob __P_((long, int, syntaxtree *));
static int NEAR stoppedjob __P_((long));
static VOID NEAR checkjob __P_((int));
#endif	/* !NOJOB */
static int NEAR waitjob __P_((long, wait_t *, int));
static VOID NEAR setstopsig __P_((int));
static long NEAR makechild __P_((int, long));
static int NEAR waitchild __P_((long, syntaxtree *));
#endif
static VOID NEAR safeclose __P_((int));
static VOID NEAR safefclose __P_((FILE *));
static int NEAR getoption __P_((int, char *[], char *[]));
static int NEAR readchar __P_((int));
static redirectlist *NEAR newrlist __P_((int, char *, int, redirectlist *));
static VOID NEAR freerlist __P_((redirectlist *));
static command_t *NEAR newcomm __P_((VOID_A));
static VOID NEAR freecomm __P_((command_t *));
static syntaxtree *NEAR parentstree __P_((syntaxtree *));
static syntaxtree *NEAR parentshell __P_((syntaxtree *));
static syntaxtree *NEAR childstree __P_((syntaxtree *, int));
static syntaxtree *NEAR skipfuncbody __P_((syntaxtree *));
static syntaxtree *NEAR linkstree __P_((syntaxtree *, int));
static int NEAR evalfiledesc __P_((char *));
static int NEAR newdup __P_((int));
static int NEAR redmode __P_((int));
static VOID NEAR closeredirect __P_((redirectlist *));
static int NEAR openheredoc __P_((char *, int, int));
#if	defined (FD) && !defined (_NODOSDRIVE)
static int NEAR fdcopy __P_((int, int));
static int NEAR openpseudofd __P_((redirectlist *));
static int NEAR closepseudofd __P_((redirectlist *));
#endif
static redirectlist *NEAR doredirect __P_((redirectlist *));
static int NEAR redirect __P_((syntaxtree *, int, char *, int));
static int NEAR identcheck __P_((char *, int));
static char *NEAR getvar __P_((char **, char *, int));
static char **NEAR putvar __P_((char **, char *, int));
static int NEAR checkprimal __P_((char *, int));
static int NEAR checkronly __P_((char *, int));
static int NEAR _putshellvar __P_((char *, int));
static int NEAR unset __P_((char *, int));
static char **NEAR duplvar __P_((char **, int));
static redirectlist *NEAR duplredirect __P_((redirectlist *));
static syntaxtree *NEAR duplstree __P_((syntaxtree *, syntaxtree *));
static shfunctable *NEAR duplfunc __P_((shfunctable *));
static VOID NEAR freefunc __P_((shfunctable *));
static int cmpfunc __P_((CONST VOID_P, CONST VOID_P));
#ifndef	NOALIAS
static aliastable *NEAR duplalias __P_((aliastable *));
static VOID NEAR freealias __P_((aliastable *));
static int cmpalias __P_((CONST VOID_P, CONST VOID_P));
static int NEAR checkalias __P_((syntaxtree *, char *, int, int));
#endif
static char *NEAR getifs __P_((VOID_A));
static int getretval __P_((VOID_A));
static long getorgpid __P_((VOID_A));
static long getlastpid __P_((VOID_A));
static char **getarglist __P_((VOID_A));
static char *getflagstr __P_((VOID_A));
static int checkundefvar __P_((char *, char *, int));
static VOID safeexit __P_((VOID_A));
static int NEAR getstatid __P_((syntaxtree *trp));
static int NEAR parsestatement __P_((syntaxtree **, int, int, int));
static syntaxtree *NEAR _addarg __P_((syntaxtree *, char *));
static int NEAR addarg __P_((syntaxtree **, char *, int *, int *, int, int));
static int NEAR evalredprefix __P_((syntaxtree **, char *, int *, int *, int));
static syntaxtree *NEAR rparen __P_((syntaxtree *));
static syntaxtree *NEAR semicolon __P_((syntaxtree *, char *,
		int *, int *, int));
static syntaxtree *NEAR ampersand __P_((syntaxtree *, char *,
		int *, int *, int *));
static syntaxtree *NEAR vertline __P_((syntaxtree *, char *, int *));
static syntaxtree *NEAR lessthan __P_((syntaxtree *, char *,
		int *, int *, int));
static syntaxtree *NEAR morethan __P_((syntaxtree *, char *,
		int *, int *, int));
static syntaxtree *NEAR normaltoken __P_((syntaxtree *, char *,
		int *, int *, int *, char *, int *, int));
static syntaxtree *NEAR casetoken __P_((syntaxtree *, char *,
		int *, int *, int *, char *, int *));
static syntaxtree *NEAR analyzeloop __P_((syntaxtree *, char *,
		int *, int *, char **, int *, int));
static syntaxtree *NEAR statementcheck __P_((syntaxtree *, int));
static int NEAR check_statement __P_((syntaxtree *));
static int NEAR check_command __P_((syntaxtree *));
static int NEAR check_stree __P_((syntaxtree *));
static syntaxtree *NEAR analyzeline __P_((char *));
static VOID NEAR Xexecve __P_((char *, char *[], char *[], int));
#if	MSDOS
static char *NEAR addext __P_((char *, int));
static char **NEAR replacebat __P_((char **, char **));
#endif
static int NEAR openpipe __P_((long *, int, int, int, int));
static pipelist **NEAR searchpipe __P_((int));
static int NEAR reopenpipe __P_((int, int));
static FILE *NEAR fdopenpipe __P_((int));
static int NEAR closepipe __P_((int));
#ifndef	_NOUSEHASH
static VOID NEAR disphash __P_((VOID_A));
#endif
static char *evalbackquote __P_((char *));
static int NEAR checktype __P_((char *, int *, int, int));
static char *NEAR evalvararg __P_((char *, int, int));
static int NEAR substvar __P_((char **));
static int NEAR evalargv __P_((command_t *, int *, int));
static char *NEAR evalexternal __P_((command_t *));
static VOID NEAR printindent __P_((int, FILE *));
static VOID NEAR printnewline __P_((int, FILE *));
static VOID NEAR printredirect __P_((redirectlist *, FILE *));
static VOID NEAR printstree __P_((syntaxtree *, int, FILE *));
static VOID NEAR printfunction __P_((shfunctable *, FILE *));
static char *NEAR headstree __P_((syntaxtree *));
static int NEAR dochild __P_((syntaxtree *));
static int NEAR doif __P_((syntaxtree *));
static int NEAR dowhile __P_((syntaxtree *));
static int NEAR dountil __P_((syntaxtree *));
static int NEAR dofor __P_((syntaxtree *));
static int NEAR docase __P_((syntaxtree *));
static int NEAR dolist __P_((syntaxtree *));
static int NEAR donull __P_((syntaxtree *));
static int NEAR dobreak __P_((syntaxtree *));
static int NEAR docontinue __P_((syntaxtree *));
static int NEAR doreturn __P_((syntaxtree *));
static int NEAR execpath __P_((command_t *, int));
static int NEAR doexec __P_((syntaxtree *));
static int NEAR dologin __P_((syntaxtree *));
static int NEAR dologout __P_((syntaxtree *));
static int NEAR doeval __P_((syntaxtree *));
static int NEAR doexit __P_((syntaxtree *));
static int NEAR doread __P_((syntaxtree *));
static int NEAR doshift __P_((syntaxtree *));
static int NEAR doset __P_((syntaxtree *));
static int NEAR dounset __P_((syntaxtree *));
#ifndef	_NOUSEHASH
static int NEAR dohash __P_((syntaxtree *));
#endif
static int NEAR dochdir __P_((syntaxtree *));
static int NEAR dopwd __P_((syntaxtree *));
static int NEAR dosource __P_((syntaxtree *));
static int NEAR doexport __P_((syntaxtree *));
static int NEAR doreadonly __P_((syntaxtree *));
static int NEAR dotimes __P_((syntaxtree *));
static int NEAR dowait __P_((syntaxtree *));
static int NEAR doumask __P_((syntaxtree *));
static int NEAR doulimit __P_((syntaxtree *));
static int NEAR dotrap __P_((syntaxtree *));
static int NEAR dojobs __P_((syntaxtree *));
static int NEAR dofg __P_((syntaxtree *));
static int NEAR dobg __P_((syntaxtree *));
static int NEAR dodisown __P_((syntaxtree *));
static int NEAR typeone __P_((char *, FILE *));
static int NEAR dotype __P_((syntaxtree *));
#ifdef	DOSCOMMAND
static int NEAR dodir __P_((syntaxtree *));
static int NEAR domkdir __P_((syntaxtree *));
static int NEAR dormdir __P_((syntaxtree *));
static int NEAR doerase __P_((syntaxtree *));
static int NEAR dorename __P_((syntaxtree *));
static int NEAR doscopy __P_((char *, char *, struct stat *, int, int, int));
static int NEAR docopy __P_((syntaxtree *));
static int NEAR docls __P_((syntaxtree *));
#endif
#ifndef	NOALIAS
static int NEAR doalias __P_((syntaxtree *));
static int NEAR dounalias __P_((syntaxtree *));
#endif
static int NEAR doecho __P_((syntaxtree *));
static int NEAR dokill __P_((syntaxtree *));
static int NEAR dotestsub1 __P_((int, char *, int *));
static int NEAR dotestsub2 __P_((int, char *[], int *));
static int NEAR dotestsub3 __P_((int, char *[], int *, int));
static int NEAR dotest __P_((syntaxtree *));
#ifndef	NOPOSIXUTIL
static int NEAR dofalse __P_((syntaxtree *));
static int NEAR docommand __P_((syntaxtree *));
static int NEAR dogetopts __P_((syntaxtree *));
static int NEAR donewgrp __P_((syntaxtree *));
# if	0
/* exists in FD original builtin */
static int NEAR dofc __P_((syntaxtree *));
# endif
#endif
#ifdef	FD
static int NEAR dofd __P_((syntaxtree *));
#endif
static int NEAR dofunction __P_((syntaxtree *, int));
#ifdef	SHOWSTREE
static VOID NEAR show_stree __P_((syntaxtree *, int));
#endif
static int NEAR setfunc __P_((char *, syntaxtree *));
static int NEAR unsetfunc __P_((char *, int));
static int NEAR exec_statement __P_((syntaxtree *));
static int NEAR exec_simplecom __P_((syntaxtree *, int, int, int));
static int NEAR exec_command __P_((syntaxtree *, int));
static int NEAR exec_process __P_((syntaxtree *, int));
static int NEAR exec_stree __P_((syntaxtree *, int));
static syntaxtree *NEAR execline __P_((char *, syntaxtree *, syntaxtree *));
static int NEAR _dosystem __P_((char *));
static FILE *NEAR _dopopen __P_((char *));
static int NEAR sourcefile __P_((int, char *, int));

#if	MSDOS
#define	RUNCOMFILE	"~\\fdsh.rc"
#define	FD_RUNCOMFILE	"~\\fd2.rc"
#define	FD_HISTORYFILE	"~\\fd.hst"
#else
#define	RUNCOMFILE	"~/.fdshrc"
#define	FD_RUNCOMFILE	"~/.fd2rc"
#define	FD_HISTORYFILE	"~/.fd_history"
#endif

#define	PS1STR		"$ "
#define	PS1ROOT		"# "
#define	PS2STR		"> "
#define	RSHELL		"rfdsh"
#define	RFD		"rfd"
#define	UNLIMITED	"unlimited"
#define	ALIASDELIMIT	"();&|<>"
#define	READ_EOF	0x100
#ifndef	PIPEDIR
#define	PIPEDIR		"PIPE"
#endif
#ifndef	REDIRECTDIR
#define	REDIRECTDIR	"REDIRECT"
#endif
#define	MAXTMPNAMLEN	8
#define	BUFUNIT		32
#define	c_malloc(size)	(malloc2((size) = BUFUNIT))
#define	c_realloc(ptr, n, size) \
			(((n) + 1 < (size)) \
			? (ptr) : realloc2(ptr, (size) *= 2))
#define	statementbody(trp) \
			((syntaxtree *)(((trp) -> comm) -> argv))
#define	hascomm(trp)	((trp) -> comm && ((trp) -> comm) -> argc >= 0)

VOID prepareexit __P_((int));
char *readline __P_((int));
syntaxtree *newstree __P_((syntaxtree *));
VOID freestree __P_((syntaxtree *));
char *getshellvar __P_((char *, int));
int putexportvar __P_((char *, int));
int putshellvar __P_((char *, int));
syntaxtree *analyze __P_((char *, syntaxtree *, int));
#if	!defined (FDSH) && !defined (_NOCOMPLETE)
int completeshellcomm __P_((char *, int, int, char ***));
#endif
int getsubst __P_((int, char **, char ***, int **));
char **getsimpleargv __P_((syntaxtree *));
int exec_line __P_((char *));
int dosystem __P_((char *));
FILE *dopopen __P_((char *));
int dopclose __P_((FILE *));
int execruncom __P_((char *, int));
int initshell __P_((int, char *[], char *[]));
int shell_loop __P_((int));
int main_shell __P_((int, char *[], char *[]));
#ifdef	FDSH
int main __P_((int, char *[], char *[]));
#endif

char **envvar = NULL;
char **shellvar = NULL;
char **exportvar = NULL;
long exportsize = 0L;
char **exportlist = NULL;
char **ronlylist = NULL;
char **argvar = NULL;
int shellmode = 0;

static char *shellname = NULL;
static FILE* dupstdin = NULL;
static int definput = -1;
static long mypid = -1;
static int ret_status = RET_SUCCESS;
static int exit_status = RET_SUCCESS;
static sigmask_t oldsigmask;
static long orgpid = -1;
static long lastpid = -1;
static int setsigflag = 0;
static int trapok = 0;
static int readtrap = 0;
static int loginshell = 0;
static int autoexport = 0;
static int errorexit = 0;
static int tmperrorexit = 0;
#if	MSDOS
static int noglob = 1;
#else
static int noglob = 0;
#endif
static int freeenviron = 0;
static int hashahead = 0;
static int notexec = 0;
static int terminated = 0;
static int undeferror = 0;
static int verboseinput = 0;
static int verboseexec = 0;
static int interactive = 0;
static int interactive_io = 0;
static int forcedstdin = 0;
static int restricted = 0;
static int bgnotify = 0;
static int noclobber = 0;
static int ignoreeof = 0;
static shfunctable *shellfunc = NULL;
#ifndef	NOALIAS
static aliastable *shellalias = NULL;
#endif
static pipelist *pipetop = NULL;
#if	!MSDOS && !defined (NOJOB)
static int jobok = -1;
static jobtable *joblist = NULL;
static int maxjobs = 0;
static int lastjob = -1;
static int prevjob = -1;
static int stopped = 0;
static long orgpgrp = -1;
static long childpgrp = -1;
#endif
static long ttypgrp = -1;
static int childdepth = 0;
static int loopdepth = 0;
static int breaklevel = 0;
static int continuelevel = 0;
static int functionlevel = 0;
static int returnlevel = 0;
static int interrupted = 0;
static int nottyout = 0;
#ifndef	NOPOSIXUTIL
static int optind = 0;
#endif
static int syntaxerrno = 0;
static int execerrno = 0;
#define	ER_UNEXPTOK	1
#define	ER_UNEXPNL	2
#define	ER_UNEXPEOF	3
static char *syntaxerrstr[] = {
	"",
	"unexpected token",
	"unexpected newline or `;'",
	"unexpected end of file",
};
#define	SYNTAXERRSIZ	((int)(sizeof(syntaxerrstr) / sizeof(char *)))
#define	ER_COMNOFOUND	1
#define	ER_NOTFOUND	2
#define	ER_CANNOTEXE	3
#define	ER_NOTIDENT	4
#define	ER_BADSUBST	5
#define	ER_BADNUMBER	6
#define	ER_BADDIR	7
#define	ER_CANNOTRET	8
#define	ER_CANNOTSTAT	9
#define	ER_CANNOTUNSET	10
#define	ER_ISREADONLY	11
#define	ER_CANNOTSHIFT	12
#define	ER_BADOPTIONS	13
#define	ER_PARAMNOTSET	14
#define	ER_MISSARG	15
#define	ER_RESTRICTED	16
#define	ER_BADULIMIT	17
#define	ER_BADTRAP	18
#define	ER_NOTALIAS	19
#define	ER_NOSUCHJOB	20
#define	ER_NUMOUTRANGE	21
#define	ER_UNKNOWNSIG	22
#define	ER_NOHOMEDIR	23
#define	ER_INVALDRIVE	24
#define	ER_RECURSIVEFD	25
#define	ER_INCORRECT	26
#define	ER_NOTLOGINSH	27
static char *execerrstr[] = {
	"",
	"command not found",
	"not found",
	"cannot execute",
	"is not an identifier",
	"bad substitution",
	"bad number",
	"bad directory",
	"cannot return when not in function",
	"cannot stat .",
	"cannot unset",
	"is read only",
	"cannot shift",
	"bad option(s)",
	"parameter not set",
	"Missing argument",
	"restricted",
	"Bad ulimit",
	"bad trap",
	"is not an alias",
	"No such job",
	"number out of range",
	"unknown signal; kill -l lists signals",
	"no home directory",
	"Invalid drive specification",
	"recursive call for FDclone",
	"incorrect",
	"not login shell",
};
#define	EXECERRSIZ	((int)(sizeof(execerrstr) / sizeof(char *)))
static opetable opelist[] = {
	{OP_FG, 4, ";"},
	{OP_BG, 4, "&"},
	{OP_NOWN, 4, "&|"},
	{OP_AND, 3, "&&"},
	{OP_OR, 3, "||"},
	{OP_PIPE, 2, "|"},
	{OP_NOT, 2, "!"},
};
#define	OPELISTSIZ	((int)(sizeof(opelist) / sizeof(opetable)))
static shbuiltintable shbuiltinlist[] = {
	{donull, ":", BT_POSIXSPECIAL},
	{dobreak, "break", BT_POSIXSPECIAL},
	{docontinue, "continue", BT_POSIXSPECIAL},
	{doreturn, "return", BT_POSIXSPECIAL},
	{doexec, "exec", BT_POSIXSPECIAL},
	{dologin, "login", 0},
	{dologout, "logout", 0},
	{doeval, "eval", BT_POSIXSPECIAL},
	{doexit, "exit", BT_POSIXSPECIAL},
	{doread, "read", 0},
	{doshift, "shift", BT_POSIXSPECIAL},
	{doset, "set", BT_POSIXSPECIAL},
	{dounset, "unset", BT_POSIXSPECIAL},
#ifndef	_NOUSEHASH
	{dohash, "hash", 0},
#endif
	{dochdir, "cd", BT_RESTRICT},
	{dopwd, "pwd", 0},
	{dosource, ".", BT_POSIXSPECIAL},
	{dosource, "source", 0},
	{doexport, "export", BT_POSIXSPECIAL},
	{doreadonly, "readonly", BT_POSIXSPECIAL},
	{dotimes, "times", BT_POSIXSPECIAL},
	{dowait, "wait", 0},
	{doumask, "umask", 0},
	{doulimit, "ulimit", 0},
	{dotrap, "trap", BT_POSIXSPECIAL},
	{dojobs, "jobs", 0},
	{dofg, "fg", 0},
	{dobg, "bg", 0},
	{dodisown, "disown", 0},
	{dotype, "type", 0},
#ifdef	DOSCOMMAND
	{donull, "rem", 0},
	{dodir, "dir", BT_NOGLOB},
	{dochdir, "chdir", BT_RESTRICT},
	{domkdir, "mkdir", BT_RESTRICT},
	{domkdir, "md", BT_RESTRICT},
	{dormdir, "rmdir", BT_RESTRICT},
	{dormdir, "rd", BT_RESTRICT},
	{doerase, "erase", BT_NOGLOB | BT_RESTRICT},
	{doerase, "del", BT_NOGLOB | BT_RESTRICT},
	{dorename, "rename", BT_NOGLOB | BT_RESTRICT},
	{dorename, "ren", BT_NOGLOB | BT_RESTRICT},
	{docopy, "copy", BT_NOGLOB | BT_RESTRICT},
	{docls, "cls", 0},
#endif
#ifndef	NOALIAS
	{doalias, "alias", 0},
	{dounalias, "unalias", BT_NOGLOB},
#endif
	{doecho, "echo", 0},
	{dokill, "kill", 0},
	{dotest, "test", 0},
	{dotest, "[", 0},
#ifndef	NOPOSIXUTIL
	{donull, "true", 0},
	{dofalse, "false", 0},
	{docommand, "command", 0},
	{dogetopts, "getopts", 0},
	{donewgrp, "newgrp", BT_RESTRICT},
# if	0
/* exists in FD original builtin */
	{dofc, "fc", 0},
# endif
#endif
#ifdef	FD
	{dofd, "fd", 0},
#endif
};
#define	SHBUILTINSIZ	((int)(sizeof(shbuiltinlist) / sizeof(shbuiltintable)))
static statementtable statementlist[] = {
	{doif, "if", STT_NEEDLIST, {0, 0, 0, 0}},
	{NULL, "then", STT_NEEDLIST, {SM_IF, SM_ELIF, 0, 0}},
	{NULL, "elif", STT_NEEDLIST, {SM_THEN, 0, 0, 0}},
	{NULL, "else", STT_NEEDLIST, {SM_THEN, 0, 0, 0}},
	{NULL, "fi", STT_NEEDNONE, {SM_THEN, SM_ELSE, 0, 0}},
	{dowhile, "while", STT_NEEDLIST, {0, 0, 0, 0}},
	{dountil, "until", STT_NEEDLIST, {0, 0, 0, 0}},
	{NULL, "do", STT_NEEDLIST, {SM_WHILE, SM_UNTIL, SM_FOR, SM_IN}},
	{NULL, "done", STT_NEEDNONE, {SM_DO, 0, 0, 0}},
	{dofor, "for", STT_NEEDIDENT | STT_FOR, {0, 0, 0, 0}},
	{NULL, "in", STT_NEEDIDENT | STT_IN, {SM_FOR, SM_ANOTHER, 0, 0}},
	{docase, "case", STT_NEEDIDENT | STT_CASE, {0, 0, 0, 0}},
	{NULL, "in", STT_NEEDIDENT | STT_INCASE, {SM_CASE, 0, 0, 0}},
	{NULL, ")", STT_NEEDLIST, {SM_INCASE, SM_CASEEND, SM_ANOTHER, 0}},
	{NULL, ";;", STT_CASEEND, {SM_RPAREN, 0, 0, 0}},
	{NULL, "esac", STT_NEEDNONE, {SM_CASEEND, SM_RPAREN, 0, 0}},
	{NULL, "(", STT_LPAREN, {0, 0, 0, 0}},
	{NULL, ")", STT_FUNC, {SM_LPAREN, 0, 0, 0}},
	{dolist, "{", STT_LIST | STT_NEEDLIST, {0, 0, 0, 0}},
	{NULL, "}", STT_NEEDNONE, {SM_LIST, 0, 0, 0}},
};
#define	STATEMENTSIZ	((int)(sizeof(statementlist) / sizeof(statementtable)))
static char *primalvar[] = {
	"PATH", "PS1", "PS2", "IFS"
};
#define	PRIMALVARSIZ	((int)(sizeof(primalvar) / sizeof(char *)))
static char getflags[] = "xnvtsierkuhfabCm";
static char setflags[] = "xnvt\0\0e\0kuhfabCm";
static char *optionflags[] = {
	"xtrace",
	"noexec",
	"verbose",
	"onecmd",
	NULL,
	NULL,
	"errexit",
	NULL,
	"keyword",
	"nounset",
	"hashahead",
	"noglob",
	"allexport",
	"notify",
	"noclobber",
#if	!MSDOS && !defined (NOJOB)
	"monitor",
#endif
}; 
static int *setvals[] = {
	&verboseexec,
	&notexec,
	&verboseinput,
	&terminated,
	&forcedstdin,
	&interactive_io,
	&tmperrorexit,
	&restricted,
	&freeenviron,
	&undeferror,
	&hashahead,
	&noglob,
	&autoexport,
	&bgnotify,
	&noclobber,
#if	!MSDOS && !defined (NOJOB)
	&jobok,
#endif
};
#define	FLAGSSIZ	((int)(sizeof(setvals) / sizeof(int)))
static ulimittable ulimitlist[] = {
#ifdef	RLIMIT_CPU
	{'t', RLIMIT_CPU, 1, "time(seconds)"},
#endif
#ifdef	RLIMIT_FSIZE
	{'f', RLIMIT_FSIZE, DEV_BSIZE, "file(blocks)"},
#endif
#ifdef	RLIMIT_DATA
	{'d', RLIMIT_DATA, 1024, "data(kbytes)"},
#endif
#ifdef	RLIMIT_STACK
	{'s', RLIMIT_STACK, 1024, "stack(kbytes)"},
#endif
#ifdef	RLIMIT_CORE
	{'c', RLIMIT_CORE, DEV_BSIZE, "coredump(blocks)"},
#endif
#ifdef	RLIMIT_RSS
	{'m', RLIMIT_RSS, 1024, "memory(kbytes)"},
#endif
#ifdef	RLIMIT_MEMLOCK
	{'l', RLIMIT_MEMLOCK, 1024, "locked memory(kbytes)"},
#endif
#ifdef	RLIMIT_NPROC
	{'u', RLIMIT_NPROC, 1, "processes(units)"},
#endif
#ifdef	RLIMIT_NOFILE
	{'n', RLIMIT_NOFILE, 1, "nofiles(descriptors)"},
#endif
#ifdef	RLIMIT_VMEM
	{'v', RLIMIT_VMEM, 1024, "virtual memory(kbytes)"},
#endif
};
#define	ULIMITSIZ	((int)(sizeof(ulimitlist) / sizeof(ulimittable)))
#ifdef	PSIGNALSTYLE
#define	MESHUP		"Hangup"
#define	MESINT		"Interrupt"
#define	MESQUIT		"Quit"
#define	MESILL		"Illegal instruction"
#define	MESTRAP		"Trace/BPT trap"
#define	MESIOT		"IOT trap"
#define	MESABRT		"Aborted"
#define	MESEMT		"EMT trap"
#define	MESFPE		"Floating point exception"
#define	MESKILL		"Killed"
#define	MESBUS		"Bus error"
#define	MESSEGV		"Segmentation fault"
#define	MESSYS		"Bad system call"
#define	MESPIPE		"Broken pipe"
#define	MESALRM		"Alarm clock"
#define	MESTERM		"Terminated"
#define	MESSTKFLT	"Stack fault"
#define	MESURG		"Urgent I/O condition"
#define	MESSTOP		"Stopped (signal)"
#define	MESTSTP		"Stopped"
#define	MESCONT		"Continued"
#define	MESCHLD		"Child exited"
#define	MESTTIN		"Stopped (tty input)"
#define	MESTTOU		"Stopped (tty output)"
#define	MESIO		"I/O possible"
#define	MESPOLL		"Profiling alarm clock"
#define	MESXCPU		"Cputime limit exceeded"
#define	MESXFSZ		"Filesize limit exceeded"
#define	MESVTALRM	"Virtual timer expired"
#define	MESPROF		"Profiling timer expired"
#define	MESWINCH	"Window changed"
#define	MESLOST		"Resource lost"
#define	MESINFO		"Information request"
#define	MESPWR		"Power failure"
#define	MESUSR1		"User defined signal 1"
#define	MESUSR2		"User defined signal 2"
#else
#define	MESHUP		"Hangup"
#define	MESINT		""
#define	MESQUIT		"Quit"
#define	MESILL		"Illegal instruction"
#define	MESTRAP		"Trace/BPT trap"
#define	MESIOT		"IOT trap"
#define	MESABRT		"Aborted"
#define	MESEMT		"EMT trap"
#define	MESFPE		"Floating exception"
#define	MESKILL		"Killed"
#define	MESBUS		"Bus error"
#define	MESSEGV		"Memory fault"
#define	MESSYS		"Bad system call"
#define	MESPIPE		""
#define	MESALRM		"Alarm call"
#define	MESTERM		"Terminated"
#define	MESSTKFLT	"Stack fault"
#define	MESURG		"Urgent condition"
#define	MESSTOP		"Stopped"
#define	MESTSTP		"Stopped from terminal"
#define	MESCONT		"Continued"
#define	MESCHLD		"Child terminated"
#define	MESTTIN		"Stopped on terminal input"
#define	MESTTOU		"Stopped on terminal output"
#define	MESIO		"Asynchronous I/O"
#define	MESPOLL		"Profiling alarm clock"
#define	MESXCPU		"Exceeded cpu time limit"
#define	MESXFSZ		"Exceeded file size limit"
#define	MESVTALRM	"Virtual time alarm"
#define	MESPROF		"Profiling time alarm"
#define	MESWINCH	"Window changed"
#define	MESLOST		"Resource lost"
#define	MESINFO		"Information request"
#define	MESPWR		"Power failure"
#define	MESUSR1		"User defined signal 1"
#define	MESUSR2		"User defined signal 2"
#endif
static signaltable signallist[] = {
#ifdef	SIGHUP
	{SIGHUP, trap_hup, "HUP", MESHUP, TR_TERM},
#endif
#ifdef	SIGINT
	{SIGINT, trap_int, "INT", MESINT, TR_TERM | TR_BLOCK},
#endif
#ifdef	SIGQUIT
	{SIGQUIT, trap_quit, "QUIT", MESQUIT, TR_TERM | TR_BLOCK},
#endif
#ifdef	SIGILL
	{SIGILL, trap_ill, "ILL", MESILL, TR_TERM},
#endif
#ifdef	SIGTRAP
	{SIGTRAP, trap_trap, "TRAP", MESTRAP, TR_TERM},
#endif
#ifdef	SIGIOT
	{SIGIOT, trap_iot, "IOT", MESIOT, TR_TERM},
#else
# ifdef	SIGABRT
	{SIGABRT, trap_abrt, "ABRT", MESABRT, TR_TERM},
# endif
#endif
#ifdef	SIGEMT
	{SIGEMT, trap_emt, "EMT", MESEMT, TR_TERM},
#endif
#ifdef	SIGFPE
	{SIGFPE, trap_fpe, "FPE", MESFPE, TR_TERM},
#endif
#ifdef	SIGKILL
	{SIGKILL, NULL, "KILL", MESKILL, 0},
#endif
#ifdef	SIGBUS
	{SIGBUS, trap_bus, "BUS", MESBUS, TR_TERM},
#endif
#ifdef	SIGSEGV
# ifdef	BASHSTYLE
	{SIGSEGV, trap_segv, "SEGV", MESSEGV, TR_TERM},
# else
	{SIGSEGV, trap_segv, "SEGV", MESSEGV, TR_TERM | TR_BLOCK | TR_NOTRAP},
# endif
#endif
#ifdef	SIGSYS
	{SIGSYS, trap_sys, "SYS", MESSYS, TR_TERM},
#endif
#ifdef	SIGPIPE
	{SIGPIPE, trap_pipe, "PIPE", MESPIPE, TR_TERM},
#endif
#ifdef	SIGALRM
	{SIGALRM, trap_alrm, "ALRM", MESALRM, TR_TERM},
#endif
#ifdef	SIGTERM
	{SIGTERM, trap_term, "TERM", MESTERM, TR_TERM | TR_BLOCK},
#endif
#ifdef	SIGSTKFLT
	{SIGSTKFLT, trap_stkflt, "STKFLT", MESSTKFLT, TR_TERM},
#endif
#ifdef	SIGURG
	{SIGURG, trap_urg, "URG", MESURG, TR_IGN},
#endif
#ifdef	SIGSTOP
	{SIGSTOP, trap_stop, "STOP", MESSTOP, TR_STOP},
#endif
#ifdef	SIGTSTP
	{SIGTSTP, trap_tstp, "TSTP", MESTSTP, TR_STOP | TR_BLOCK},
#endif
#ifdef	SIGCONT
	{SIGCONT, trap_cont, "CONT", MESCONT, TR_IGN},
#endif
#ifdef	SIGCHLD
# ifdef	NOJOB
	{SIGCHLD, trap_chld, "CHLD", MESCHLD, TR_IGN},
# else
	{SIGCHLD, trap_chld, "CHLD", MESCHLD, TR_IGN | TR_BLOCK},
# endif
#endif
#ifdef	SIGTTIN
	{SIGTTIN, trap_ttin, "TTIN", MESTTIN, TR_STOP | TR_BLOCK},
#endif
#ifdef	SIGTTOU
	{SIGTTOU, trap_ttou, "TTOU", MESTTOU, TR_STOP | TR_BLOCK},
#endif
#ifdef	SIGIO
	{SIGIO, trap_io, "IO", MESIO, TR_IGN},
#else
# ifdef	SIGPOLL
	{SIGPOLL, trap_poll, "POLL", MESPOLL, TR_TERM},
# endif
#endif
#ifdef	SIGXCPU
	{SIGXCPU, trap_xcpu, "XCPU", MESXCPU, TR_TERM},
#endif
#ifdef	SIGXFSZ
	{SIGXFSZ, trap_xfsz, "XFSZ", MESXFSZ, TR_TERM},
#endif
#ifdef	SIGVTALRM
	{SIGVTALRM, trap_vtalrm, "VTALRM", MESVTALRM, TR_TERM},
#endif
#ifdef	SIGPROF
	{SIGPROF, trap_prof, "PROF", MESPROF, TR_TERM},
#endif
#ifdef	SIGWINCH
	{SIGWINCH, trap_winch, "WINCH", MESWINCH, TR_IGN},
#endif
#ifdef	SIGLOST
	{SIGLOST, trap_lost, "LOST", MESLOST, TR_TERM},
#endif
#ifdef	SIGINFO
	{SIGINFO, trap_info, "INFO", MESINFO, TR_IGN},
#endif
#ifdef	SIGPWR
	{SIGPWR, trap_pwr, "PWR", MESPWR, TR_TERM},
#endif
#ifdef	SIGUSR1
# ifdef	BASHSTYLE
	{SIGUSR1, trap_usr1, "USR1", MESUSR1, TR_TERM},
# else
	{SIGUSR1, trap_usr1, "USR1", MESUSR1, TR_TERM | TR_BLOCK},
# endif
#endif
#ifdef	SIGUSR2
# ifdef	BASHSTYLE
	{SIGUSR2, trap_usr2, "USR2", MESUSR2, TR_TERM},
# else
	{SIGUSR2, trap_usr2, "USR2", MESUSR2, TR_TERM | TR_BLOCK},
# endif
#endif
};
#define	SIGNALSIZ	((int)(sizeof(signallist) / sizeof(signaltable)))
static int trapmode[NSIG];
static char *trapcomm[NSIG];
static sigarg_t (*oldsigfunc[NSIG])__P_((sigfnc_t));


#ifndef	FD
#ifdef	LSI_C
static int NEAR Xdup(oldd)
int oldd;
{
	int fd;

	if ((fd = dup(oldd)) < 0) return(-1);
	if (fd < SYS_OPEN && oldd >= 0 && oldd < SYS_OPEN)
		_openfile[fd] = _openfile[oldd];
	return(fd);
}

static int NEAR Xdup2(oldd, newd)
int oldd, newd;
{
	int fd;

	if ((fd = dup2(oldd, newd)) < 0) return(-1);
	if (newd >= 0 && newd < SYS_OPEN && oldd >= 0 && oldd < SYS_OPEN)
		_openfile[newd] = _openfile[oldd];
	return(fd);
}
#endif	/* LSI_C */

int strncpy3(s1, s2, lenp, ptr)
char *s1, *s2;
int *lenp, ptr;
{
	int i, j, l;

	for (i = j = 0; i < ptr; i++, j++) {
#ifdef	CODEEUC
		if (isekana(s2, j)) j++;
		else
#endif
		if (iskanji1(s2, j)) {
			i++;
			j++;
		}
	}
	if (!i || i <= ptr) i = 0;
	else {
		s1[0] = ' ';
		i = 1;
	}

	while ((*lenp < 0 || i < *lenp) && s2[j]) {
#ifdef	CODEEUC
		if (isekana(s2, j)) {
			if (*lenp >= 0) (*lenp)++;
			s1[i++] = s2[j++];
		}
		else
#endif
		if (iskanji1(s2, j)) {
			if (*lenp >= 0 && i >= *lenp - 1) {
				s1[i++] = ' ';
				break;
			}
			s1[i++] = s2[j++];
		}
		else if (isctl(s2[j])) {
			s1[i++] = '^';
			if (*lenp >= 0 && i >= *lenp) break;
			s1[i++] = ((s2[j++] + '@') & 0x7f);
			continue;
		}
		s1[i++] = s2[j++];
	}

	l = i;
	if (*lenp >= 0 && ptr >= 0) while (i < *lenp) s1[i++] = ' ';
	s1[i] = '\0';
	return(l);
}

char *strstr2(s1, s2)
char *s1, *s2;
{
	char *cp;
	int len;

	len = strlen(s2);
	for (cp = s1; (cp = strchr(cp, *s2)); cp++)
		if (!strncmp(cp, s2, len)) return(cp);
	return(NULL);
}

VOID freevar(var)
char **var;
{
	int i;

	if (var) {
		for (i = 0; var[i]; i++) free(var[i]);
		free(var);
	}
}

static int NEAR mktmpdir(dir)
char *dir;
{
	char *cp, path[MAXPATHLEN];
	int no;

	if (!tmpfilename) {
		sprintf(path, "TM%ld", mypid);
		tmpfilename = strdup2(path);
	}
	if (!deftmpdir || !*deftmpdir || !dir || !*dir) {
		errno = ENOENT;
		return(-1);
	}
	strcpy(path, deftmpdir);
	strcpy(strcatdelim(path), tmpfilename);
	if (_Xmkdir(path, 0755) < 0 && errno != EEXIST) return(-1);
	strcpy((cp = strcatdelim(path)), dir);
	if (_Xmkdir(path, 0755) < 0 && errno != EEXIST) {
		*(--cp) = '\0';
		no = errno;
		if (_Xrmdir(path) < 0
		&& errno != ENOTEMPTY && errno != EEXIST && errno != EACCES) {
			fputs("fatal error: ", stderr);
			kanjifputs(path, stderr);
			fputs(": cannot remove temporary directory\n", stderr);
			fflush(stderr);
			prepareexit(-1);
			Xexit2(RET_FATALERR);
		}
		errno = no;
		return(-1);
	}
	strcpy(dir, path);
	return(0);
}

static int NEAR rmtmpdir(dir)
char *dir;
{
	char path[MAXPATHLEN];

	if (dir && *dir && _Xrmdir(dir) < 0) return(-1);
	strcatdelim2(path, deftmpdir, tmpfilename);
	if (_Xrmdir(path) < 0
	&& errno != ENOTEMPTY && errno != EEXIST && errno != EACCES)
		return(-1);
	return(0);
}

static int NEAR mktmpfile(buf, dir)
char *buf, *dir;
{
	char *cp, path[MAXPATHLEN];
	int i, fd;

	strcpy(path, dir);
	if (mktmpdir(path) < 0) return(-1);
	cp = strcatdelim(path);
	memset(cp, '\0', sizeof(path) - (cp - path));
	cp[0] = '_';

	for (;;) {
		fd = Xopen(path, O_BINARY | O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd >= 0) {
			strcpy(buf, path);
			return(fd);
		}
		if (errno != EEXIST) return(-1);

		for (i = 0; i < MAXTMPNAMLEN; i++) {
			if (!cp[i]) cp[i] = '_';
			else if (cp[i] == '9') {
				cp[i] = '_';
				continue;
			}
			else if (cp[i] == '_') cp[i] = 'a';
#if	MSDOS
			else if (cp[i] == 'z') cp[i] = '0';
#else
			else if (cp[i] == 'z') cp[i] = 'A';
			else if (cp[i] == 'Z') cp[i] = '0';
#endif
			else cp[i]++;
			break;
		}
		if (i >= MAXTMPNAMLEN) break;
	}
	if (cp > path) *(--cp) = '\0';
	rmtmpdir(path);
	return(-1);
}

static int NEAR rmtmpfile(path)
char *path;
{
	char *cp;
	int ret;

	ret = 0;
	if (Xunlink(path) != 0 && errno != ENOENT) ret = -1;
	else if (!(cp = strrdelim(path, 0)));
#if	MSDOS
	else if (cp == path + 2 && isalpha(path[0]) && path[1] == ':');
#endif
	else if (cp != path) {
		*cp = '\0';
		if (rmtmpdir(path) < 0
		&& errno != ENOTEMPTY && errno != EEXIST && errno != EACCES)
			ret = -1;
	}
	free(path);
	return(ret);
}

# if	MSDOS
#  ifdef	DJGPP
static int NEAR dos_putpath(path, offset)
char *path;
int offset;
{
	int i;

	i = strlen(path) + 1;
	dosmemput(path, i, __tb + offset);
	return(i);
}

char *Xgetwd(path)
char *path;
{
	char *cp;
	int i;

	if (!(cp = (char *)getcwd(path, MAXPATHLEN))) return(NULL);
	for (i = 0; cp[i]; i++) if (cp[i] == '/') cp[i] = _SC_;
	return(cp);
}
#  else	/* !DJGPP */
/*ARGSUSED*/
int _Xmkdir(path, mode)
char * path;
int mode;
{
	struct stat st;

	if (Xstat(path, &st) >= 0) {
		errno = EEXIST;
		return(-1);
	}
	return(mkdir(path) ? -1 : 0);
}
#  endif	/* !DJGPP */

static int NEAR int21call(regp, sregp)
__dpmi_regs *regp;
struct SREGS *sregp;
{
#  ifdef	__TURBOC__
	struct REGPACK preg;
#  endif

	(*regp).x.flags |= FR_CARRY;
#  ifdef	__TURBOC__
	preg.r_ax = (*regp).x.ax;
	preg.r_bx = (*regp).x.bx;
	preg.r_cx = (*regp).x.cx;
	preg.r_dx = (*regp).x.dx;
	preg.r_bp = (*regp).x.bp;
	preg.r_si = (*regp).x.si;
	preg.r_di = (*regp).x.di;
	preg.r_ds = (*sregp).ds;
	preg.r_es = (*sregp).es;
	preg.r_flags = (*regp).x.flags;
	intr(0x21, &preg);
	(*regp).x.ax = preg.r_ax;
	(*regp).x.bx = preg.r_bx;
	(*regp).x.cx = preg.r_cx;
	(*regp).x.dx = preg.r_dx;
	(*regp).x.bp = preg.r_bp;
	(*regp).x.si = preg.r_si;
	(*regp).x.di = preg.r_di;
	(*sregp).ds = preg.r_ds;
	(*sregp).es = preg.r_es;
	(*regp).x.flags = preg.r_flags;
#  else	/* !__TURBOC__ */
#   ifdef	DJGPP
	(*regp).x.ds = (*sregp).ds;
	(*regp).x.es = (*sregp).es;
	__dpmi_int(0x21, regp);
	(*sregp).ds = (*regp).x.ds;
	(*sregp).es = (*regp).x.es;
#   else
	int86x(0x21, regp, regp, sregp);
#   endif
#  endif	/* !__TURBOC__ */
	if (((*regp).x.flags & FR_CARRY)) return(-1);
	return(0);
}

/*ARGSUSED*/
static int NEAR setcurdrv(drive, nodir)
int drive, nodir;
{
	int drv, olddrv;

	drv = toupper2(drive) - 'A';
	olddrv = (bdos(0x19, 0, 0) & 0xff);
	if ((bdos(0x0e, drv, 0) & 0xff) < drv
	&& (bdos(0x19, 0, 0) & 0xff) != drv) {
		bdos(0x0e, olddrv, 0);
		errno = EINVAL;
		return(-1);
	}
	return(0);
}

int chdir3(dir)
char *dir;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int drv, olddrv;

	if (_dospath(dir)) {
		drv = toupper2(*dir) - 'A';
		olddrv = (bdos(0x19, 0, 0) & 0xff);
		if ((bdos(0x0e, drv, 0) & 0xff) < drv
		|| (bdos(0x19, 0, 0) & 0xff) != drv) {
			bdos(0x0e, olddrv, 0);
			errno = EINVAL;
			return(-1);
		}
	}

	reg.x.ax = 0x3b00;
#  ifdef	DJGPP
	dos_putpath(dir, 0);
#  endif
	sreg.ds = PTR_SEG(dir);
	reg.x.dx = PTR_OFF(dir, 0);
	if (int21call(&reg, &sreg) < 0) {
		drv = errno;
		bdos(0x0e, olddrv, 0);
		errno = drv;
		return(-1);
	}
	return(0);
}

#  ifdef	DOSCOMMAND
struct fat32statfs_t {
	u_short f_type __attribute__ ((packed));
	u_short f_version __attribute__ ((packed));
	u_long f_clustsize __attribute__ ((packed));
	u_long f_sectsize __attribute__ ((packed));
	u_long f_bavail __attribute__ ((packed));
	u_long f_blocks __attribute__ ((packed));
	u_long f_real_bavail_sect __attribute__ ((packed));
	u_long f_real_blocks_sect __attribute__ ((packed));
	u_long f_real_bavail __attribute__ ((packed));
	u_long f_real_blocks __attribute__ ((packed));
	u_char reserved[8] __attribute__ ((packed));
};

VOID getinfofs(path, totalp, freep)
char *path;
long *totalp, *freep;
{
	struct fat32statfs_t fsbuf;
	struct SREGS sreg;
	__dpmi_regs reg;
	long size;
	char *cp, buf[128], dupl[MAXPATHLEN];

	*totalp = *freep = -1L;

	strcpy(dupl, path);
	if (!(cp = strdelim(dupl, 0))) *(cp = dupl + strlen(dupl)) = _SC_;
	*(++cp) = '\0';
	path = dupl;

	reg.x.ax = 0x71a0;
	reg.x.bx = 0;
	reg.x.cx = sizeof(buf);
#   ifdef	DJGPP
	dos_putpath(path, sizeof(buf));
#   endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, sizeof(buf));
	sreg.es = PTR_SEG(buf);
	reg.x.di = PTR_OFF(buf, 0);
	if (int21call(&reg, &sreg) >= 0 && (reg.x.bx & 0x4000)
#   ifdef	DJGPP
	&& (dosmemget(__tb, sizeof(buf), buf), 1)
#   endif
	&& !strcmp(buf, VOL_FAT32)) {
		reg.x.ax = 0x7303;
		reg.x.cx = sizeof(fsbuf);
#   ifdef	DJGPP
		dos_putpath(path, sizeof(fsbuf));
#   endif
		sreg.es = PTR_SEG(&fsbuf);
		reg.x.di = PTR_OFF(&fsbuf, 0);
		sreg.ds = PTR_SEG(path);
		reg.x.dx = PTR_OFF(path, sizeof(fsbuf));
		if (int21call(&reg, &sreg) < 0) return;
#   ifdef	DJGPP
		dosmemget(__tb, sizeof(fsbuf), &fsbuf);
#   endif
		*totalp = (long)(fsbuf.f_blocks);
		*freep = (long)(fsbuf.f_bavail);
		size = (long)(fsbuf.f_clustsize) * (long)(fsbuf.f_sectsize);
	}
	else {
		reg.x.ax = 0x3600;
		reg.h.dl = toupper2(*path) - 'A' + 1;
		int21call(&reg, &sreg);
		if (reg.x.ax == 0xffff || !reg.x.ax || !reg.x.cx || !reg.x.dx
		&& reg.x.dx < reg.x.bx) return;

		*totalp = (long)(reg.x.dx);
		*freep = (long)(reg.x.bx);
		size = (long)(reg.x.ax) * (long)(reg.x.cx);
	}

	if (size >= 1024) {
		size = (size + 512) / 1024;
		*freep *= size;
		*totalp *= size;
	}
	else {
		size = (1024 + (size / 2)) / size;
		*freep /= size;
		*totalp /= size;
	}
}

/*ARGSUSED*/
char *realpath2(path, resolved, rdlink)
char *path, *resolved;
int rdlink;
{
	struct SREGS sreg;
	__dpmi_regs reg;
#   ifdef	DJGPP
	int i;
#   endif

	reg.x.ax = 0x6000;
	reg.x.cx = 0;
#   ifdef	DJGPP
	i = dos_putpath(path, 0);
#   endif
	sreg.ds = PTR_SEG(path);
	reg.x.si = PTR_OFF(path, 0);
	sreg.es = PTR_SEG(resolved);
	reg.x.di = PTR_OFF(resolved, i);
	if (int21call(&reg, &sreg) < 0) return(NULL);
#   ifdef	DJGPP
	dosmemget(__tb + i, MAXPATHLEN, resolved);
#   endif
	return(resolved);
}

long getblocksize(path)
char *path;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x3600;
	reg.h.dl = (_dospath(path)) ? toupper2(*path) - 'A' + 1 : 0;
	int21call(&reg, &sreg);
	if (reg.x.ax == 0xffff) return(BUFSIZ);
	return((long)(reg.x.ax) * (long)(reg.x.cx));
}
#  endif	/* DOSCOMMAND */
# endif	/* MSDOS */

/*
 *	ascnumeric(buf, n, 0, max): same as sprintf(buf, "%d", n)
 *	ascnumeric(buf, n, max, max): same as sprintf(buf, "%*d", max, n)
 *	ascnumeric(buf, n, -1, max): same as sprintf(buf, "%-*d", max, n)
 *	ascnumeric(buf, n, x, max): like as sprintf(buf, "%*d", max, n)
 *	ascnumeric(buf, n, -x, max): like as sprintf(buf, "%-*d", max, n)
 */
static char *NEAR ascnumeric(buf, n, digit, max)
char *buf;
long n;
int digit, max;
{
	char tmp[20 * 2 + 1];
	int i, j, d;

	i = j = 0;
	d = digit;
	if (digit < 0) digit = -digit;
	if (n < 0) tmp[i++] = '?';
	else if (!n) tmp[i++] = '0';
	else {
		for (;;) {
			tmp[i++] = '0' + n % 10;
			if (!(n /= 10) || i >= max) break;
			if (digit > 1 && ++j >= digit) {
				if (i >= max - 1) break;
				tmp[i++] = ',';
				j = 0;
			}
		}
		if (n) for (j = 0; j < i; j++) if (tmp[j] != ',') tmp[j] = '9';
	}

	if (d <= 0) j = 0;
	else if (d > max) for (j = 0; j < max - i; j++) buf[j] = '0';
	else for (j = 0; j < max - i; j++) buf[j] = ' ';
	while (i--) buf[j++] = tmp[i];
	if (d < 0) for (; j < max; j++) buf[j] = ' ';
	buf[j] = '\0';

	return(buf);
}

static int NEAR setenv2(name, value)
char *name, *value;
{
	char *cp;
	int len;

	len = strlen(name);
	if (!value) cp = name;
	else {
		cp = malloc2(len + strlen(value) + 2);
		memcpy(cp, name, len);
		cp[len] = '=';
		strcpy(&(cp[len + 1]), value);
	}
	if (putshellvar(cp, len) < 0) {
		free(cp);
		return(-1);
	}
	return(0);
}
#endif	/* !FD */

static VOID NEAR setsignal(VOID_A)
{
	int i, sig;

	if (setsigflag++) return;
	for (i = 0; i < SIGNALSIZ; i++) {
		sig = signallist[i].sig;
		if (signallist[i].flags & TR_BLOCK);
		else if ((trapmode[sig] & TR_STAT) != TR_TRAP) continue;
		else if ((signallist[i].flags & TR_STAT) == TR_STOP) continue;

		signal(sig, (sigcst_t)(signallist[i].func));
	}
}

static VOID NEAR resetsignal(forced)
int forced;
{
	int i, duperrno;
#if	!MSDOS
# ifdef	NOJOB
	long tmp;

	do {
		tmp = Xwait3(NULL, WNOHANG | WUNTRACED, NULL);
	} while (tmp > 0 || (tmp < 0 && errno == EINTR));
# else
	checkjob(1);
	stopped = 0;
# endif
	if (ttypgrp < 0 || ttypgrp != getpid());
	else
#endif
	if (interrupted && interactive && !nottyout) {
		fflush(stdout);
		fputc('\n', stderr);
		fflush(stderr);
	}
	interrupted = 0;
	if (!setsigflag) return;
	if (forced) setsigflag = 0;
	else if (--setsigflag) return;
	duperrno = errno;
	for (i = 0; i < NSIG; i++)
		if (oldsigfunc[i] != SIG_ERR) signal(i, oldsigfunc[i]);
#if	!MSDOS
	Xsigsetmask(oldsigmask);
#endif
	errno = duperrno;
}

static VOID NEAR exectrapcomm(VOID_A)
{
#if	MSDOS
	sigarg_t (*ofunc)__P_((sigfnc_t));
#else
	sigmask_t mask, omask;
#endif
	int i;

#if	!MSDOS
	if (ttypgrp < 0 || ttypgrp != mypid) return;
#endif
	for (i = 0; i < NSIG; i++) {
		if (!(trapmode[i] & TR_CATCH)) continue;
		trapmode[i] &= ~TR_CATCH;
		if (!*(trapcomm[i])) continue;
#if	MSDOS
		ofunc = signal(i, SIG_IGN);
#else
		Xsigemptyset(mask);
		Xsigaddset(mask, i);
		Xsigblock(omask, mask);
#endif
		_dosystem(trapcomm[i]);
#if	MSDOS
		signal(i, ofunc);
#else
		Xsigsetmask(omask);
#endif
	}
}

static int NEAR trap_common(sig)
int sig;
{
	int i, trapped, duperrno;

	for (i = 0; i < SIGNALSIZ; i++) if (sig == signallist[i].sig) break;
	if (i >= SIGNALSIZ) {
		signal(sig, SIG_DFL);
		return(0);
	}

	duperrno = errno;
	trapped = 0;
	if (mypid != orgpid) {
		if ((signallist[i].flags & TR_BLOCK)
		|| (signallist[i].flags & TR_STAT) == TR_TERM)
			trapped = -1;
	}
	else if ((trapmode[sig] & TR_STAT) == TR_TRAP) trapped = 1;
	else if (signallist[i].flags & TR_BLOCK) {
		if (readtrap) {
#if	!MSDOS && defined (TIOCSTI)
			u_char c;

			c = '\n';
			ioctl(STDIN_FILENO, TIOCSTI, &c);
#endif
			readtrap = 0;
		}
	}
	else if	((signallist[i].flags & TR_STAT) == TR_TERM) trapped = -1;

#if	!MSDOS
	if (ttypgrp < 0 || ttypgrp != mypid);
	else
#endif
	if (trapped > 0) {
		trapmode[sig] |= TR_CATCH;
		if (trapok) exectrapcomm();
	}
	else if (trapped < 0 && *(signallist[i].mes)) {
		fputs(signallist[i].mes, stderr);
		fputc('\n', stderr);
		fflush(stderr);
	}

	if (trapped < 0) {
		prepareexit(0);
		Xexit2(sig + 128);
	}

	if (signallist[i].func) signal(sig, (sigcst_t)(signallist[i].func));
	errno = duperrno;
	return(trapped);
}

#ifdef	SIGHUP
static int trap_hup(VOID_A)
{
	if (!trap_common(SIGHUP)) {
#if	!MSDOS && !defined (NOJOB)
		if (ttypgrp >= 0 && ttypgrp == getpid()) killjob();
#endif
		if (oldsigfunc[SIGHUP] && oldsigfunc[SIGHUP] != SIG_ERR
		&& oldsigfunc[SIGHUP] != SIG_DFL
		&& oldsigfunc[SIGHUP] != SIG_IGN) {
#ifdef	SIGFNCINT
			(*oldsigfunc[SIGHUP])(SIGHUP);
#else
			(*oldsigfunc[SIGHUP])();
#endif
		}
		prepareexit(0);
		Xexit2(RET_FAIL);
	}
	return(0);
}
#endif

#ifdef	SIGINT
static int trap_int(VOID_A)
{
	trap_common(SIGINT);
	if (trapok) interrupted = 1;
	return(0);
}
#endif

#ifdef	SIGQUIT
static int trap_quit(VOID_A)
{
	return(trap_common(SIGQUIT));
}
#endif

#ifdef	SIGILL
static int trap_ill(VOID_A)
{
	return(trap_common(SIGILL));
}
#endif

#ifdef	SIGTRAP
static int trap_trap(VOID_A)
{
	return(trap_common(SIGTRAP));
}
#endif

#ifdef	SIGIOT
static int trap_iot(VOID_A)
{
	return(trap_common(SIGIOT));
}
#else
# ifdef	SIGABRT
static int trap_abrt(VOID_A)
{
	return(trap_common(SIGABRT));
}
# endif
#endif

#ifdef	SIGEMT
static int trap_emt(VOID_A)
{
	return(trap_common(SIGEMT));
}
#endif

#ifdef	SIGFPE
static int trap_fpe(VOID_A)
{
	return(trap_common(SIGFPE));
}
#endif

#ifdef	SIGBUS
static int trap_bus(VOID_A)
{
	return(trap_common(SIGBUS));
}
#endif

#ifdef	SIGSEGV
static int trap_segv(VOID_A)
{
	return(trap_common(SIGSEGV));
}
#endif

#ifdef	SIGSYS
static int trap_sys(VOID_A)
{
	return(trap_common(SIGSYS));
}
#endif

#ifdef	SIGPIPE
static int trap_pipe(VOID_A)
{
	return(trap_common(SIGPIPE));
}
#endif

#ifdef	SIGALRM
static int trap_alrm(VOID_A)
{
	return(trap_common(SIGALRM));
}
#endif

#ifdef	SIGTERM
static int trap_term(VOID_A)
{
	return(trap_common(SIGTERM));
}
#endif

#ifdef	SIGSTKFLT
static int trap_stkflt(VOID_A)
{
	return(trap_common(SIGSTKFLT));
}
#endif

#ifdef	SIGURG
static int trap_urg(VOID_A)
{
	return(trap_common(SIGURG));
}
#endif

#ifdef	SIGSTOP
static int trap_stop(VOID_A)
{
	return(trap_common(SIGSTOP));
}
#endif

#ifdef	SIGTSTP
static int trap_tstp(VOID_A)
{
	return(trap_common(SIGTSTP));
}
#endif

#ifdef	SIGCONT
static int trap_cont(VOID_A)
{
	return(trap_common(SIGCONT));
}
#endif

#ifdef	SIGCHLD
static int trap_chld(VOID_A)
{
# ifndef	NOJOB
	if (bgnotify) checkjob(1);
# endif
	return(trap_common(SIGCHLD));
}
#endif

#ifdef	SIGTTIN
static int trap_ttin(VOID_A)
{
	return(trap_common(SIGTTIN));
}
#endif

#ifdef	SIGTTOU
static int trap_ttou(VOID_A)
{
	return(trap_common(SIGTTOU));
}
#endif

#ifdef	SIGIO
static int trap_io(VOID_A)
{
	return(trap_common(SIGIO));
}
#else
# ifdef	SIGPOLL
static int trap_poll(VOID_A)
{
	return(trap_common(SIGPOLL));
}
# endif
#endif

#ifdef	SIGXCPU
static int trap_xcpu(VOID_A)
{
	return(trap_common(SIGXCPU));
}
#endif

#ifdef	SIGXFSZ
static int trap_xfsz(VOID_A)
{
	return(trap_common(SIGXFSZ));
}
#endif

#ifdef	SIGVTALRM
static int trap_vtalrm(VOID_A)
{
	return(trap_common(SIGVTALRM));
}
#endif

#ifdef	SIGPROF
static int trap_prof(VOID_A)
{
	return(trap_common(SIGPROF));
}
#endif

#ifdef	SIGWINCH
static int trap_winch(VOID_A)
{
	return(trap_common(SIGWINCH));
}
#endif

#ifdef	SIGLOST
static int trap_lost(VOID_A)
{
	return(trap_common(SIGLOST));
}
#endif

#ifdef	SIGINFO
static int trap_info(VOID_A)
{
	return(trap_common(SIGINFO));
}
#endif

#ifdef	SIGPWR
static int trap_pwr(VOID_A)
{
	return(trap_common(SIGPWR));
}
#endif

#ifdef	SIGUSR1
static int trap_usr1(VOID_A)
{
	return(trap_common(SIGUSR1));
}
#endif

#ifdef	SIGUSR2
static int trap_usr2(VOID_A)
{
	return(trap_common(SIGUSR2));
}
#endif

VOID prepareexit(noexit)
int noexit;
{
#ifdef	DEBUG
	int i;
#endif
	int duperrno;

	duperrno = errno;
	if (ttypgrp >= 0 && ttypgrp == getpid()) {
		exectrapcomm();
		if (!noexit && (trapmode[0] & TR_STAT) == TR_TRAP) {
			trapmode[0] = 0;
			_dosystem(trapcomm[0]);
		}
	}
	resetsignal(1);
#ifdef	DEBUG
	if (definput >= 0) {
		if (definput != ttyio) safeclose(definput);
		definput = -1;
	}
	if (dupstdin) {
		safefclose(dupstdin);
		dupstdin = NULL;
	}
	doexec(NULL);
	freevar(shellvar);
	shellvar = NULL;
	freevar(exportvar);
	exportvar = NULL;
	exportsize = 0L;
	freevar(exportlist);
	exportlist = NULL;
	freevar(ronlylist);
	ronlylist = NULL;
	freevar(argvar);
	argvar = NULL;
	freefunc(shellfunc);
	shellfunc = NULL;
# ifndef	NOALIAS
	freealias(shellalias);
	shellalias = NULL;
# endif
# ifndef	_NOUSEHASH
	searchhash(NULL, NULL, NULL);
# endif
# ifndef	FD
	if (tmpfilename) {
		free(tmpfilename);
		tmpfilename = NULL;
	}
# endif
	for (i = 0; i < NSIG; i++) {
		signal(i, SIG_IGN);
		if (trapcomm[i]) {
			free(trapcomm[i]);
			trapcomm[i] = NULL;
		}
		signal(i, oldsigfunc[i]);
	}
# if	!MSDOS && !defined (NOJOB)
	if (joblist) {
		for (i = 0; i < maxjobs; i++) {
			if (!(joblist[i].pids)) continue;
			free(joblist[i].pids);
			free(joblist[i].stats);
			if (joblist[i].trp) {
				freestree(joblist[i].trp);
				free(joblist[i].trp);
			}
			joblist[i].pids = NULL;
		}
		free(joblist);
		joblist = NULL;
	}
# endif	/* !MSDOS && !NOJOB */
#endif	/* DEBUG */
	errno = duperrno;
}

static VOID NEAR syntaxerror(s, n)
char *s;
int n;
{
	if (n <= 0 || n >= SYNTAXERRSIZ) return;
	if (!interactive && argvar && argvar[0]) {
		kanjifputs(argvar[0], stderr);
		fputs(": ", stderr);
	}
	if (s) {
		if (!*s || n == ER_UNEXPNL) fputs("syntax error", stderr);
		else kanjifputs(s, stderr);
		fputs(": ", stderr);
	}
	fputs(syntaxerrstr[n], stderr);
	fputc('\n', stderr);
	fflush(stderr);
	ret_status = RET_SYNTAXERR;
	if (errorexit) {
#if	!MSDOS && !defined (NOJOB)
		if (loginshell && interactive_io) killjob();
#endif
		prepareexit(0);
		Xexit2(RET_SYNTAXERR);
	}
	safeexit();
}

static VOID NEAR execerror(argv, s, n)
char *argv[], *s;
int n;
{
	if (n == ER_BADSUBST
	&& (execerrno == ER_ISREADONLY || execerrno == ER_PARAMNOTSET)) return;

	if (!n || n >= EXECERRSIZ) return;
#ifndef	BASHSTYLE
	/* bash shows its name, even in interective shell */
	if (interactive);
	else
#endif
	if (argv && argv[0]) {
		kanjifputs(argv[0], stderr);
		fputs(": ", stderr);
	}
	else if (argvar && argvar[0]) {
		kanjifputs(argvar[0], stderr);
		fputs(": ", stderr);
	}
	if (s) {
		kanjifputs(s, stderr);
		fputs(": ", stderr);
	}
	fputs(execerrstr[n], stderr);
	fputc('\n', stderr);
	fflush(stderr);
	execerrno = n;
#ifndef	BASHSTYLE
	/* bash does not exit on error, in non interactive shell */
	breaklevel = loopdepth;
	if (argv != argvar) safeexit();
#endif
}

static VOID NEAR doperror(argv, s)
char *argv[], *s;
{
	int duperrno;

	duperrno = errno;
	if (errno < 0) return;
	if (argv && argv[0]) {
		kanjifputs(argv[0], stderr);
		fputs(": ", stderr);
	}
	if (s) {
		kanjifputs(s, stderr);
		fputs(": ", stderr);
	}
#if	MSDOS
	fputs(strerror(duperrno), stderr);
#else
	fputs((char *)sys_errlist[duperrno], stderr);
#endif
	fputc('\n', stderr);
	fflush(stderr);
	errno = 0;
}

static long NEAR isnumeric(s)
char *s;
{
	long n;
	int i;

	if (!*s) return(-1L);
	for (i = n = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9') {
#ifndef	BASHSTYLE
	/* bash always treats non numeric character as error */
			if (i) break;
#endif
			return(-1L);
		}
		n = n * 10 + (s[i] - '0');
	}
	return(n);
}

static VOID NEAR fputlong(n, fp)
long n;
FILE *fp;
{
	char buf[20 + 1];

	ascnumeric(buf, n, 0, 20);
	fputs(buf, fp);
}

#if	!MSDOS
static int NEAR closeonexec(fd)
int fd;
{
	int n;

	if ((n = fcntl(fd, F_GETFD, NULL)) < 0) return(-1);
	n |= FD_CLOEXEC;
	return(fcntl(fd, F_SETFD, n));
}

static VOID NEAR dispsignal(sig, width, fp)
int sig, width;
FILE *fp;
{
	int i;

	for (i = 0; i < SIGNALSIZ; i++) if (sig == signallist[i].sig) break;
	if (i >= SIGNALSIZ) {
		fputs("Signal ", fp);
		if (width < 7) fputlong(sig, fp);
		else fprintf(fp, "Signal %-*.d", width - 7, sig);
	}
	else if (!width) fputs(signallist[i].mes, fp);
	else fprintf(fp, "%-*.*s", width, width, signallist[i].mes);
}

# ifndef	NOJOB
static int NEAR gettermio(pgrp)
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
#endif
	if ((ret = settcpgrp(ttyio, pgrp)) >= 0) ttypgrp = pgrp;
	Xsigsetmask(omask);
	return(ret);
}

static VOID NEAR dispjob(n, fp)
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

	if (!sig) fprintf(fp, "%-*.*s", 28, 28, "Running");
	else if (sig < 0) fprintf(fp, "%-*.*s", 28, 28, "Done");
	else {
		if (sig >= 128) sig -= 128;
		dispsignal(sig, 28, fp);
	}
	if (joblist[n].trp) {
		if ((joblist[n].trp -> flags & ST_TYPE) != OP_BG)
			joblist[n].trp -> flags &= ~ST_TYPE;
		printstree(joblist[n].trp, -1, fp);
	}
	fputc('\n', fp);
	fflush(fp);
}

static int NEAR searchjob(pid, np)
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

static int NEAR getjob(s)
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

static int NEAR stackjob(pid, sig, trp)
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
#endif
	return(i);
}

static int NEAR stoppedjob(pid)
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

static VOID NEAR checkjob(verbose)
int verbose;
{
	int i, j;

	while (waitjob(-1, NULL, WNOHANG | WUNTRACED) > 0);
	if (verbose) for (i = 0; i < maxjobs; i++) {
		if (!(joblist[i].pids)) continue;
		j = joblist[i].npipe;
		if (joblist[i].stats[j] >= 0 && joblist[i].stats[j] < 128)
			continue;

		if (joblist[i].trp
		&& (joblist[i].trp -> flags & ST_TYPE) == OP_BG)
			joblist[i].trp -> flags &= ~ST_TYPE;
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
# endif	/* !NOJOB */

static int NEAR waitjob(pid, wp, opt)
long pid;
wait_t *wp;
int opt;
{
# ifndef	NOJOB
	int i, j, sig;
# endif
	wait_t w;
	long tmp;
	int ret;

	do {
		tmp = (pid < 0)
			? Xwait3(&w, opt, NULL) : Xwait4(pid, &w, opt, NULL);
	} while (tmp < 0 && errno == EINTR);

	if (!tmp) return(0);
	else if (tmp < 0) {
		if (pid < 0 || errno != ECHILD) return(-1);
		ret = -1;
# ifndef	NOJOB
		sig = -1;
# endif
	}
	else {
		ret = (pid < 0 || tmp == pid) ? 1 : 0;
# ifndef	NOJOB
		if (WIFSTOPPED(w)) sig = WSTOPSIG(w);
		else if (WIFSIGNALED(w)) sig = 128 + WTERMSIG(w);
		else sig = -1;
# endif
	}

# ifndef	NOJOB
	if (ret >= 0 && (i = searchjob(tmp, &j)) >= 0)
		joblist[i].stats[j] = sig;
		if (interactive && !nottyout && pid != tmp && WIFSTOPPED(w))
			dispjob(i, stderr);
# endif
	if (wp) *wp = w;
	return(ret);
}

static VOID NEAR setstopsig(valid)
int valid;
{
	int i;

	for (i = 0; i < SIGNALSIZ; i++) {
		if (signallist[i].flags & TR_BLOCK);
		else if ((signallist[i].flags & TR_STAT) != TR_STOP) continue;

		if (valid) signal(signallist[i].sig, SIG_DFL);
		else signal(signallist[i].sig, (sigcst_t)(signallist[i].func));
	}
}

/*ARGSUSED*/
static long NEAR makechild(tty, parent)
int tty;
long parent;
{
	long pid;
	int stop;

	if ((pid = fork()) < 0) return(-1L);
#ifdef	DEBUG
	if (!pid) {
		extern VOID (*__free_hook) __P_((VOID_P));
		extern VOID_P (*__malloc_hook) __P_((ALLOC_T));
		extern VOID_P (*__realloc_hook) __P_((VOID_P, ALLOC_T));

		__free_hook = NULL;
		__malloc_hook = NULL;
		__realloc_hook = NULL;
	}
#endif
	if (!pid) {
# ifdef	SIGCHLD
		sigmask_t mask;

		memcpy(&mask, &oldsigmask, sizeof(sigmask_t));
		Xsigdelset(mask, SIGCHLD);
		Xsigsetmask(mask);
# else
		Xsigsetmask(oldsigmask);
# endif
		mypid = getpid();
		stop = 1;
# ifdef	NOJOB
		ttypgrp = -1;
		if (loginshell) stop = 0;
# else
		if (jobok && childpgrp == orgpgrp) stop = 0;
# endif
		setstopsig(stop);
	}

# ifndef	NOJOB
	if (!jobok) {
		if (childpgrp < 0) childpgrp = orgpgrp;
	}
	else if (pid) {
		if (childpgrp < 0) childpgrp = (parent >= 0) ? parent : pid;
		setpgroup(pid, childpgrp);
		if (tty && ttypgrp >= 0) ttypgrp = childpgrp;
	}
	else {
		if (childpgrp < 0) childpgrp = (parent >= 0) ? parent : mypid;
		if (setpgroup(mypid, childpgrp) < 0) {
			doperror(NULL, "fatal error");
			prepareexit(-1);
			Xexit(RET_FATALERR);
		}
		if (tty && ttypgrp >= 0) gettermio(childpgrp);
	}
# endif	/* !NOJOB */

	return(pid);
}

static int NEAR waitchild(pid, trp)
long pid;
syntaxtree *trp;
{
# ifndef	NOJOB
	int i, j;
# endif
	wait_t w;
	int ret;

	trapok = 1;
	for (;;) {
		if (interrupted) return(RET_INTR);
		if ((ret = waitjob(pid, &w, WUNTRACED)) < 0) break;
		if (!ret) continue;
		if (!WIFSTOPPED(w)) break;
# ifdef	NOJOB
		if (loginshell) kill(pid, SIGCONT);
# else
		if (!jobok || mypid != orgpgrp) continue;

		trapok = 0;
		ret = WSTOPSIG(w);
		gettermio(orgpgrp);
		prevjob = lastjob;
		lastjob = stackjob(pid, ret, trp);
		if (interactive && !nottyout) dispjob(lastjob, stderr);
		breaklevel = loopdepth;
		stopped = 1;
		return(RET_SUCCESS);
# endif
	}
	trapok = 0;

# ifndef	NOJOB
	if (jobok && mypid == orgpgrp) gettermio(orgpgrp);
# endif
	if (ret < 0) {
		if (!trp && errno == ECHILD) ret = errno = 0;
	}
	else if (WIFSIGNALED(w)) {
		ret = WTERMSIG(w);

# ifndef	NOJOB
		if ((i = searchjob(pid, NULL)) >= 0)
			killpg(joblist[i].pids[0], ret);
# endif
# ifdef	SIGINT
		if (ret == SIGINT) interrupted = 1;
		else
# endif
# if	defined (SIGPIPE) && !defined (PSIGNALSTYLE)
		if (ret == SIGPIPE);
		else
# endif
		if (!nottyout) {
			if (!interactive && argvar && argvar[0]) {
				kanjifputs(argvar[0], stderr);
				fputs(": ", stderr);
				fputlong(pid, stderr);
				fputc(' ', stderr);
			}
			dispsignal((int)ret, 0, stderr);
			fputc('\n', stderr);
			fflush(stderr);
		}
		ret += 128;
	}
	else ret = (WEXITSTATUS(w) & 0377);

	if (ret < 0) return(-1);
# ifndef	NOJOB
	if ((i = searchjob(pid, &j)) >= 0 && j == joblist[i].npipe) {
		free(joblist[i].pids);
		free(joblist[i].stats);
		if (joblist[i].trp) {
			freestree(joblist[i].trp);
			free(joblist[i].trp);
		}
		joblist[i].pids = NULL;
	}
# endif
	return((int)ret);
}
#endif	/* !MSDOS */

static VOID NEAR safeclose(fd)
int fd;
{
	if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
		Xclose(fd);
}

static VOID NEAR safefclose(fp)
FILE *fp;
{
	int fd;

	fd = fileno(fp);
	if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
		Xfclose(fp);
}

static int NEAR getoption(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
	u_long flags;
	char *cp;
	int i, j, com;

	if (argc <= 1 || (argv[1][0] != '-' && argv[1][0] != '+')) return(1);
	else if (argv[1][1] == '-') return(2);

	cp = (envp) ? getflags : setflags;
	com = 0;
	flags = 0;
	for (i = 1; argv[1][i]; i++) {
		if (envp) {
			if (argv[1][i] == 'c' && !com && argc > 2) {
				com = 1;
				continue;
			}
		}
		else {
			if (argv[1][i] == 'o' && !com) {
				com = (argv[1][0] == '-') ? 1 : 2;
				continue;
			}
		}
		for (j = 0; j < FLAGSSIZ; j++) if (argv[1][i] == cp[j]) break;
		if (j < FLAGSSIZ) flags |= (1 << j);
		else if (argv[1][0] == '-') {
			execerror(NULL, argv[1], ER_BADOPTIONS);
			return(-1);
		}
	}
	for (j = 0; j < FLAGSSIZ; j++) {
		if (flags & 1) *(setvals[j]) = (argv[1][0] == '-') ? 1 : 0;
		flags >>= 1;
	}

	return(com + 2);
}

static int NEAR readchar(fd)
int fd;
{
	static int savech = -1;
	static int prevfd = -1;
	u_char ch;
	int n;

	n = savech;
	savech = -1;
	if (prevfd == fd && n >= 0) return(n);
	prevfd = fd;

	while ((n = Xread(fd, &ch, sizeof(ch))) < 0 && errno == EINTR);
	if (n < 0) return(-1);
	if (!n) return(READ_EOF);
	if (ch == '\r') {
		while ((n = Xread(fd, &ch, sizeof(ch))) < 0 && errno == EINTR);
		if (n < 0) return(-1);
		if (!n) {
			savech = READ_EOF;
			return('\r');
		}
		if (ch == '\n') return(ch);
		savech = ch;
		return('\r');
	}
	return((int)ch);
}

char *readline(fd)
int fd;
{
	char *cp;
	ALLOC_T i, size;
	int c;

	cp = c_malloc(size);
	for (i = 0; (c = readchar(fd)) != '\n'; i++) {
		if (c < 0) {
			free(cp);
			return(NULL);
		}
		if (c == READ_EOF) {
			if (!i) {
				free(cp);
				errno = 0;
				return(NULL);
			}
			break;
		}
		cp = c_realloc(cp, i, size);
		cp[i] = c;
	}
#if	MSDOS
	if (i > 0 && cp[i - 1] == '\r') i--;
#endif
	cp[i++] = '\0';
	return(realloc2(cp, i));
}

static redirectlist *NEAR newrlist(fd, filename, type, next)
int fd;
char *filename;
int type;
redirectlist *next;
{
	redirectlist *new;

	if ((type & MD_FILEDESC) && filename[0] == '-' && !filename[1]) {
		filename = NULL;
		type &= ~MD_FILEDESC;
	}
	new = (redirectlist *)malloc2(sizeof(redirectlist));
	new -> fd = fd;
	new -> filename = strdup2(filename);
	new -> type = (u_char)type;
	new -> new = new -> old = -1;
#if	defined (FD) && !defined (_NODOSDRIVE)
	new -> fakepipe = NULL;
	new -> dosfd = -1;
#endif
	new -> next = next;
	return(new);
}

static VOID NEAR freerlist(redp)
redirectlist *redp;
{
	if (!redp) return;
	if (redp -> next) freerlist(redp -> next);
	if (redp -> filename) free(redp -> filename);
	if (redp -> new >= 0 && !(redp -> type & MD_FILEDESC)) {
		if (redp -> type & MD_HEREDOC) closepipe(redp -> new);
		else safeclose(redp -> new);
	}
	if (redp -> old >= 0 && redp -> old != redp -> fd) {
		safeclose(redp -> fd);
		Xdup2(redp -> old, redp -> fd);
		safeclose(redp -> old);
	}
#if	defined (FD) && !defined (_NODOSDRIVE)
	closepseudofd(redp);
#endif
	free(redp);
}

static command_t *NEAR newcomm()
{
	command_t *new;

	new = (command_t *)malloc2(sizeof(command_t));
	new -> hash = NULL;
	new -> argc = -1;
	new -> argv = NULL;
	new -> redp = NULL;
	new -> type =
	new -> id = 0;
	return(new);
}

static VOID NEAR freecomm(comm)
command_t *comm;
{
	int i;

	if (!comm) return;
	if (comm -> argv) {
		if (isstatement(comm))
			freestree((syntaxtree *)(comm -> argv));
		else for (i = 0; i <= comm -> argc; i++)
			if (comm -> argv[i]) free(comm -> argv[i]);
		free(comm -> argv);
	}
	if (comm -> redp) freerlist(comm -> redp);
	free(comm);
}

syntaxtree *newstree(parent)
syntaxtree *parent;
{
	syntaxtree *new;

	new = (syntaxtree *)malloc2(sizeof(syntaxtree));
	new -> comm = NULL;
	new -> parent = parent;
	new -> next = NULL;
	new -> flags = 0;
	return(new);
}

VOID freestree(trp)
syntaxtree *trp;
{
	if (!trp) return;
	if (trp -> comm) {
		if (!(trp -> flags & ST_NODE)) freecomm(trp -> comm);
		else {
			freestree((syntaxtree *)(trp -> comm));
			free(trp -> comm);
		}
		trp -> comm = NULL;
	}

	if (trp -> next) {
		if (!(trp -> flags & (ST_QUOT | ST_META)))
			freestree(trp -> next);
		free(trp -> next);
		trp -> next = NULL;
	}
	trp -> flags = 0;
}

static syntaxtree *NEAR parentstree(trp)
syntaxtree *trp;
{
	while (trp) {
		if (!(trp -> flags & ST_NEXT)) return(trp -> parent);
		trp = trp -> parent;
	}
	return(NULL);
}

static syntaxtree *NEAR parentshell(trp)
syntaxtree *trp;
{
	while ((trp = parentstree(trp)))
		if (!(trp -> flags & ST_NODE)) return(trp);
	return(NULL);
}

static syntaxtree *NEAR childstree(trp, no)
syntaxtree *trp;
int no;
{
	syntaxtree *new;

	new = newstree(trp);
	if (!(trp -> comm)) trp -> comm = newcomm();
	(trp -> comm) -> argc = 0;
	(trp -> comm) -> argv = (char **)new;
	(trp -> comm) -> type = CT_STATEMENT;
	(trp -> comm) -> id = no;
	trp -> flags &= ~(ST_TYPE | ST_NODE);
	return(new);
}

static syntaxtree *NEAR skipfuncbody(trp)
syntaxtree *trp;
{
	syntaxtree *tmptr;

	tmptr = statementcheck(trp -> next, SM_STATEMENT);
	if (getstatid(tmptr) == SM_LPAREN - 1) trp = trp -> next;
	return(trp);
}

static syntaxtree *NEAR linkstree(trp, type)
syntaxtree *trp;
int type;
{
	syntaxtree *new, *tmptr;
	int i, l1, l2;

	type &= ST_TYPE;
	if (trp && isstatement(trp -> comm)
	&& (trp -> comm) -> id == SM_STATEMENT
	&& (tmptr = statementbody(trp))
	&& getstatid(tmptr) == SM_LPAREN - 1)
		trp = trp -> parent;
	tmptr = (trp -> flags & ST_NEXT) ? trp -> parent : NULL;

	for (i = 0; i < OPELISTSIZ; i++) if (type == opelist[i].op) break;
	l1 = (i < OPELISTSIZ) ? opelist[i].level : 0;
	if (!tmptr) l2 = 0;
	else {
		for (i = 0; i < OPELISTSIZ; i++)
			if ((tmptr -> flags & ST_TYPE) == opelist[i].op) break;
		l2 = (i < OPELISTSIZ) ? opelist[i].level : 0;
	}

	if (!l1);
#ifndef	BASHSTYLE
	/* bash does not allow the format like as "foo | ; bar" */
	else if (tmptr && (tmptr -> flags & ST_TYPE) == OP_PIPE && l1 > l2);
#endif
	else if (type == OP_NOT && (!l2 || l1 < l2));
	else if (!hascomm(trp)) {
		if (type != OP_FG) {
			syntaxerrno = ER_UNEXPTOK;
			return(trp);
		}
#ifdef	BASHSTYLE
	/* bash does not allow a null before ";" */
		else {
			syntaxerrno = ER_UNEXPNL;
			return(trp);
		}
#endif
	}

	tmptr = trp;
	if (!l1 || !l2);
	else if (l1 < l2) {
		new = newstree(trp);
		new -> comm = trp -> comm;
		trp -> comm = (command_t *)new;
		trp -> flags &= ~ST_TYPE;
		trp -> flags |= ST_NODE;
		trp = new;
	}
	else if (l1 > l2) {
		if (!(trp = parentstree(trp))) {
			for (trp = tmptr; trp -> parent; trp = trp -> parent);
			new = newstree(trp);
			new -> comm = trp -> comm;
			new -> next = trp -> next;
			new -> flags = trp -> flags;
			if (new -> next) (new -> next) -> parent = new;
			trp -> comm = (command_t *)new;
			trp -> flags = (type | ST_NODE);
		}
		else if (!(trp -> flags & ST_NODE)) {
			new = newstree(trp);
			new -> comm = (command_t *)((trp -> comm) -> argv);
			(trp -> comm) -> argv = (char **)new;
			trp = new;
			trp -> flags = (type | ST_NODE);
		}
	}

	trp = skipfuncbody(trp);
	new = trp -> next = newstree(trp);
	new -> flags = ST_NEXT;
	trp -> flags &= ~ST_TYPE;
	trp -> flags |= type;
	return(new);
}

static int NEAR evalfiledesc(tok)
char *tok;
{
	int i, n, max;

	n = 0;
	if ((max = getdtablesize()) <= 0) max = NOFILE;
	for (i = 0; tok[i]; i++) {
		if (tok[i] < '0' || tok[i] > '9') return(-1);
		n = n * 10 + tok[i] - '0';
		if (n > max) n = max;
	}
	return((i) ? n : -1);
}

static int NEAR newdup(fd)
int fd;
{
#if	MSDOS
	struct stat st;
#endif
	int n;

	if (fd < 0
	|| fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
		return(fd);
#if	defined (FD) && !defined (_NODOSDRIVE)
	if (fd >= DOSFDOFFSET) return(fd);
#endif
	if ((n = getdtablesize()) <= 0) n = NOFILE;

	for (n--; n > fd; n--)
#if	MSDOS
		if (fstat(n, &st) != 0) break;
#else
		if (fcntl(n, F_GETFD, NULL) < 0) break;
#endif
	if (n <= fd || Xdup2(fd, n) < 0) return(fd);
	Xclose(fd);
	return(n);
}

static int NEAR redmode(type)
int type;
{
	int mode;

	mode = O_BINARY;
	if (type & MD_READ) {
		if (type & MD_WRITE) {
			mode |= (O_RDWR | O_CREAT);
			if (noclobber && !(type & MD_FORCED)) mode |= O_EXCL;
		}
		else mode |= O_RDONLY;
		if (type & MD_APPEND) mode |= O_APPEND;
	}
	else {
		mode |= O_WRONLY | O_CREAT;
		if (type & MD_APPEND) mode |= O_APPEND;
		else {
			mode |= O_TRUNC;
			if (noclobber && !(type & MD_FORCED)) mode |= O_EXCL;
		}
	}
	return(mode);
}

static VOID NEAR closeredirect(redp)
redirectlist *redp;
{
	if (!redp) return;
	if (redp -> next) closeredirect(redp -> next);

	if (redp -> type & MD_WITHERR) {
		if (redp -> fd != STDERR_FILENO) {
			Xclose(STDERR_FILENO);
			Xdup2(redp -> fd, STDERR_FILENO);
			safeclose(redp -> fd);
		}
		redp -> fd = STDOUT_FILENO;
	}

	if (redp -> new >= 0 && !(redp -> type & MD_FILEDESC)) {
		if (redp -> type & MD_HEREDOC) closepipe(redp -> new);
		else {
			safeclose(redp -> new);
#if	defined (FD) && !defined (_NODOSDRIVE)
			closepseudofd(redp);
#endif
		}
	}
	if (redp -> old >= 0 && redp -> old != redp -> fd) {
		safeclose(redp -> fd);
		Xdup2(redp -> old, redp -> fd);
		safeclose(redp -> old);
	}
	redp -> old = redp -> new = -1;
}

static int NEAR openheredoc(eof, old, ignoretab)
char *eof;
int old, ignoretab;
{
	char *cp, *ps;
	long pipein;
	int i, j, c, fd, ret, size, quoted, duperrno;

	if ((fd = openpipe(&pipein, old, 1, interactive_io, mypid)) < 0)
		return(-1);
	if (pipein > 0) {
#if	!MSDOS && !defined (USEFAKEPIPE)
		if (waitchild(pipein, NULL) != RET_SUCCESS) {
			closepipe(fd);
			errno = -1;
			return(-1);
		}
#endif
		return(fd);
	}
	quoted = stripquote(eof = strdup2(eof), 1);

	ps = (interactive_io) ? getshellvar("PS2", -1) : NULL;
	cp = c_malloc(size);
	ret = RET_SUCCESS;

	for (i = j = 0;;) {
		if (!j++ && ps) {
			kanjifputs(ps, stderr);
			fflush(stderr);
		}
		if (!dupstdin || (c = readchar(fileno(dupstdin))) < 0) {
			duperrno = errno;
			free(cp);
			free(eof);
			closepipe(fd);
			errno = duperrno;
			return(-1);
		}
		if (c == READ_EOF || c == '\n') {
			if (i) {
				char *tmp;

				cp[i] = '\0';
				i = j = 0;
				if (!strcmp(eof, cp)) break;

				if (ret != RET_SUCCESS);
				else if (quoted) fputs(cp, stdout);
				else if ((tmp = evalarg(cp, 0, 1))) {
					fputs(tmp, stdout);
					free(tmp);
				}
				else {
					if (*cp) execerror(argvar, cp,
						ER_BADSUBST);
					ret = RET_FAIL;
				}
			}
			if (ret == RET_SUCCESS) fputc('\n', stdout);
			if (c == READ_EOF) break;
			continue;
		}
		if (ignoretab && c == '\t' && !i) continue;
		cp = c_realloc(cp, i, size);
		cp[i++] = c;
	}
	fflush(stdout);
	free(cp);
	free(eof);
	if ((fd = reopenpipe(fd, ret)) < 0) return(-1);
	if (ret != RET_SUCCESS) {
		closepipe(fd);
		errno = -1;
		return(-1);
	}
	return(fd);
}

#if	defined (FD) && !defined (_NODOSDRIVE)
static int NEAR fdcopy(fdin, fdout)
int fdin, fdout;
{
	u_char ch;
	int n;

	for (;;) {
		while ((n = Xread(fdin, &ch, sizeof(ch))) < 0
		&& errno == EINTR);
		if (n <= 0) break;
		if (Xwrite(fdout, &ch, sizeof(ch)) < 0) return(-1);
	}
	return(n);
}

static int NEAR openpseudofd(redp)
redirectlist *redp;
{
# if	!MSDOS && !defined (USEFAKEPIPE)
	long pid;
	int fildes[2];
# endif
	char pfile[MAXPATHLEN];
	int fd;

	if (!(redp -> type & MD_RDWR) || (redp -> type & MD_RDWR) == MD_RDWR)
		return(-1);

# if	!MSDOS && !defined (USEFAKEPIPE)
	if (pipe(fildes) < 0);
	else if ((pid = makechild(0, mypid)) < 0) {
		safeclose(fildes[0]);
		safeclose(fildes[1]);
		return(-1);
	}
	else if (pid) {
		safeclose(redp -> new);
		if (redp -> type & MD_READ) {
			safeclose(fildes[1]);
			redp -> new = newdup(fildes[0]);
		}
		else {
			safeclose(fildes[0]);
			redp -> new = newdup(fildes[1]);
		}
		return(0);
	}
	else {
		if (redp -> type & MD_READ) {
			safeclose(fildes[0]);
			fildes[1] = newdup(fildes[1]);
			fdcopy(redp -> new, fildes[1]);
			safeclose(redp -> new);
			safeclose(fildes[1]);
		}
		else {
			safeclose(fildes[1]);
			fildes[0] = newdup(fildes[0]);
			fdcopy(fildes[0], redp -> new);
			safeclose(redp -> new);
			safeclose(fildes[0]);
		}
		prepareexit(1);
		Xexit2(RET_SUCCESS);
	}
# endif	/* !MSDOS && !USEFAKEPIPE */

	if ((fd = newdup(mktmpfile(pfile, PIPEDIR))) < 0) return(-1);
	if (redp -> type & MD_WRITE) redp -> dosfd = redp -> new;
	else {
		fdcopy(redp -> new, fd);
		safeclose(redp -> new);
		safeclose(fd);
		fd = newdup(Xopen(pfile, O_BINARY | O_RDONLY, 0666));
		if (fd < 0) {
			rmtmpfile(strdup2(pfile));
			redp -> fakepipe = NULL;
			return(-1);
		}
	}
	redp -> new = fd;
	redp -> fakepipe = strdup2(pfile);
	return(0);
}

static int NEAR closepseudofd(redp)
redirectlist *redp;
{
	int fd;

	if (!(redp -> fakepipe)) return(0);
	if (redp -> dosfd >= 0 && redp -> type & MD_WRITE) {
		fd = Xopen(redp -> fakepipe, O_BINARY | O_RDONLY, 0666);
		if (fd >= 0) {
			fdcopy(fd, redp -> dosfd);
			safeclose(fd);
		}
		safeclose(redp -> dosfd);
	}
	rmtmpfile(redp -> fakepipe);
	redp -> fakepipe = NULL;
	redp -> dosfd = -1;
	return(0);
}
#endif	/* FD && !_NODOSDRIVE */

static redirectlist *NEAR doredirect(redp)
redirectlist *redp;
{
#if	MSDOS
	struct stat st;
#else
	int oldexec, newexec = -1;
#endif
	redirectlist *errp;
	int duperrno;

	if (redp -> next && (errp = doredirect(redp -> next))) return(errp);

	if (!(redp -> filename));
	else if (redp -> type & MD_FILEDESC) {
		if ((redp -> new = evalfiledesc(redp -> filename)) < 0) {
			redp -> new = -1;
			errno = EBADF;
			return(redp);
		}
#if	MSDOS
		if (fstat(redp -> new, &st) != 0) {
#else
		if ((newexec = fcntl(redp -> new, F_GETFD, NULL)) < 0) {
#endif
			redp -> new = -1;
#ifdef	BASHSTYLE
	/* bash treats ineffective descriptor as error */
			errno = EBADF;
			return(redp);
#else
			redp -> old = -1;
			redp -> type = 0;
			return(NULL);
#endif
		}
	}
	else if (redp -> type & MD_HEREDOC) {
		redp -> new = openheredoc(redp -> filename, redp -> fd,
			redp -> type & MD_APPEND);
		if (redp -> new < 0) return(redp);
	}
	else if (restricted && (redp -> type & MD_WRITE)) {
		errno = 0;
		return(redp);
	}
	else {
		char *tmp;

		tmp = evalvararg(redp -> filename, 0, 1);
		if (!tmp) {
			errno = -1;
			return(redp);
		}
		redp -> new = newdup(Xopen(tmp, redmode(redp -> type), 0666));
		free(tmp);
		if (redp -> new < 0) return(redp);
	}

#if	MSDOS
	redp -> old = newdup(Xdup(redp -> fd));
#else
	if ((oldexec = fcntl(redp -> fd, F_GETFD, NULL)) < 0)
		redp -> old = -1;
	else if ((redp -> old = newdup(Xdup(redp -> fd))) < 0) {
		if (redp -> new >= 0 && !(redp -> type & MD_FILEDESC))
			safeclose(redp -> new);
		redp -> new = -1;
		return(redp);
	}
	else if (oldexec > 0 || redp -> fd == STDIN_FILENO
	|| redp -> fd == STDOUT_FILENO || redp -> fd == STDERR_FILENO)
		closeonexec(redp -> old);
#endif

	if (redp -> new != redp -> fd) {
		safeclose(redp -> fd);
		if (redp -> new >= 0) {
#if	defined (FD) && !defined (_NODOSDRIVE)
			if (redp -> new >= DOSFDOFFSET) openpseudofd(redp);
#endif
			if (Xdup2(redp -> new, redp -> fd) < 0) return(redp);
		}
	}

	if ((redp -> type & MD_WITHERR) && redp -> new != STDERR_FILENO
	&& redp -> fd == STDOUT_FILENO) {
		if ((redp -> fd = newdup(Xdup(STDERR_FILENO))) < 0)
			return(redp);
		if (Xdup2(redp -> new, STDERR_FILENO) < 0) {
			duperrno = errno;
			safeclose(redp -> fd);
			safeclose(redp -> new);
			redp -> fd = redp -> new = -1;
			errno = duperrno;
			return(redp);
		}
	}
#if	!MSDOS
	if (newexec > 0
	&& redp -> fd != STDIN_FILENO
	&& redp -> fd != STDOUT_FILENO
	&& redp -> fd != STDERR_FILENO)
		closeonexec(redp -> fd);
#endif

	return(NULL);
}

static int NEAR redirect(trp, from, to, type)
syntaxtree *trp;
int from;
char *to;
int type;
{
	redirectlist *rp;

	if (to && !*to) {
		syntaxerrno = ER_UNEXPTOK;
		return(-1);
	}

	if (from < 0) from = (type & MD_READ) ? STDIN_FILENO : STDOUT_FILENO;

	if (!(trp -> comm)) trp -> comm = newcomm();
	rp = newrlist(from, to, type, (trp -> comm) -> redp);
	(trp -> comm) -> redp = rp;
	return(0);
}

static int NEAR identcheck(ident, delim)
char *ident;
int delim;
{
	int i;

	if (!ident || !*ident || (*ident != '_' && !isalpha(*ident)))
		return(0);
	for (i = 1; ident[i]; i++)
		if (ident[i] != '_' && !isalnum(ident[i])) break;
	return((ident[i] == delim) ? i : ((ident[i]) ? 0 : -i));
}

static char *NEAR getvar(var, ident, len)
char **var, *ident;
int len;
{
	int i;

	if (!var) return(NULL);
	if (len < 0) len = strlen(ident);
	for (i = 0; var[i]; i++)
		if (!strnpathcmp(ident, var[i], len) && var[i][len] == '=')
			return(&(var[i][len + 1]));
	return(NULL);
}

char *getshellvar(ident, len)
char *ident;
int len;
{
	return(getvar(shellvar, ident, len));
}

static char **NEAR putvar(var, s, len)
char **var, *s;
int len;
{
	int i, export;

	export = (var == exportvar);
	for (i = 0; var[i]; i++)
	if (!strnpathcmp(s, var[i], len) && var[i][len] == '=') break;

	if (var[i]) {
		if (export) exportsize -= (int)strlen(var[i]) + 1;
		free(var[i]);
		if (!s[len]) {
			for (; var[i + 1]; i++) var[i] = var[i + 1];
			var[i] = NULL;
			return(var);
		}
	}
	else if (!s[len]) return(var);
	else {
		var = (char **)realloc2(var, (i + 2) * sizeof(char *));
		var[i + 1] = NULL;
	}

	var[i] = s;
#if	MSDOS
	for (i = 0; i < len; i++) s[i] = toupper2(s[i]);
#endif
	if (export) {
		exportsize += strlen(s) + 1;
#if	MSDOS
		if (len == sizeof("PATH") - 1
		&& !strnpathcmp(s, "PATH", len)) {
			char *cp, *next;

			next = &(s[len + 1]);
			for (cp = next; cp; cp = next) {
				if (_dospath(cp))
					next = strchr(cp + 2, PATHDELIM);
				else next = strchr(cp, PATHDELIM);
				if (next) *(next++) = ';';
			}
		}
#endif
	}
	return(var);
}

static int NEAR checkprimal(s, len)
char *s;
int len;
{
	char *cp;
	int i;

	for (i = 0; i < PRIMALVARSIZ; i++)
	if (!strnpathcmp(s, primalvar[i], len) && !primalvar[i][len]) {
		cp = malloc2(len + 1);
		strncpy2(cp, s, len);
		execerror(NULL, cp, ER_CANNOTUNSET);
		free(cp);
		return(-1);
	}
	return(0);
}

static int NEAR checkronly(s, len)
char *s;
int len;
{
	char *cp;
	int i;

	for (i = 0; ronlylist[i]; i++)
	if (!strnpathcmp(s, ronlylist[i], len) && !ronlylist[i][len]) {
		cp = malloc2(len + 1);
		strncpy2(cp, s, len);
		execerror(NULL, cp, ER_ISREADONLY);
		free(cp);
		return(-1);
	}
	return(0);
}

static int NEAR _putshellvar(s, len)
char *s;
int len;
{
#ifdef	BASHSTYLE
	/* bash distinguishes the same named function and variable */
	if (checkronly(s, len) < 0) return(-1);
#else
	if (unsetfunc(s, len) < 0) return(-1);
#endif

	if (len == sizeof("PATH") - 1 && !strnpathcmp(s, "PATH", len)) {
		if (restricted) {
			execerror(NULL, "PATH", ER_RESTRICTED);
			return(-1);
		}
#ifndef	_NOUSEHASH
		searchhash(NULL, NULL, NULL);
#endif
	}
	if (restricted
	&& len == sizeof("SHELL") - 1 && !strnpathcmp(s, "SHELL", len)) {
		execerror(NULL, "SHELL", ER_RESTRICTED);
		return(-1);
	}
#ifndef	NOPOSIXUTIL
	if (len == sizeof("OPTIND") - 1 && !strnpathcmp(s, "OPTIND", len)) {
		optind = isnumeric(&(s[len + 1]));
		if (optind <= 1) optind = 0;
	}
#endif

	shellvar = putvar(shellvar, s, len);
	return(0);
}

int putexportvar(s, len)
char *s;
int len;
{
	char *cp;
	int i;

	if (len < 0) {
		if (!(cp = strchr(s, '='))) return(0);
		len = cp - s;
	}

	if (_putshellvar(s, len) < 0) return(-1);
	for (i = 0; exportlist[i]; i++)
		if (!strnpathcmp(s, exportlist[i], len) && !exportlist[i][len])
			break;
	if (!exportlist[i]) {
		exportlist = (char **)realloc2(exportlist,
			(i + 2) * sizeof(char *));
		exportlist[i] = malloc2(len + 1);
		strncpy2(exportlist[i], s, len);
		exportlist[++i] = NULL;
	}
	exportvar = putvar(exportvar, strdup2(s), len);

	return(0);
}

int putshellvar(s, len)
char *s;
int len;
{
	char *cp;
	int i;

	if (autoexport) return(putexportvar(s, len));
	if (len < 0) {
		if (!(cp = strchr(s, '='))) return(0);
		len = cp - s;
	}

	if (_putshellvar(s, len) < 0) return(-1);
	for (i = 0; exportlist[i]; i++)
		if (!strnpathcmp(s, exportlist[i], len) && !exportlist[i][len])
			break;
	if (exportlist[i]) exportvar = putvar(exportvar, strdup2(s), len);

	return(0);
}

static int NEAR unset(ident, len)
char *ident;
int len;
{
	int i;

	if (checkprimal(ident, len) < 0 || checkronly(ident, len) < 0)
		return(-1);
	shellvar = putvar(shellvar, ident, len);
	exportvar = putvar(exportvar, ident, len);
	for (i = 0; exportlist[i]; i++)
	if (!strnpathcmp(ident, exportlist[i], len) && !exportlist[i][len]) {
		free(exportlist[i]);
		for (; exportlist[i + 1]; i++)
			exportlist[i] = exportlist[i + 1];
		exportlist[i] = NULL;
		break;
	}
	return(0);
}

static char **NEAR duplvar(var, margin)
char **var;
int margin;
{
	char **dupl;
	int i, n;

	if (!var) n = 0;
	else for (n = 0; var[n]; n++);
	dupl = (char **)malloc2((n + margin + 1) * sizeof(char *));
	for (i = 0; i < n; i++) dupl[i] = strdup2(var[i]);
	dupl[i] = NULL;
	return(dupl);
}

static redirectlist *NEAR duplredirect(redp)
redirectlist *redp;
{
	redirectlist *new, *next;

	if (!redp) return(NULL);
	next = duplredirect(redp -> next);
	new = newrlist(redp -> fd, redp -> filename, redp -> type, next);
	new -> new = redp -> new;
	new -> old = redp -> old;
	return(new);
}

static syntaxtree *NEAR duplstree(trp, parent)
syntaxtree *trp, *parent;
{
	syntaxtree *new;
	command_t *comm;

	new = newstree(parent);
	new -> flags = trp -> flags;
	if ((comm = trp -> comm)) {
		if (trp -> flags & ST_NODE)
			new -> comm = (command_t *)duplstree(
				(syntaxtree *)comm, new);
		else {
			new -> comm = newcomm();
			(new -> comm) -> hash = comm -> hash;
			(new -> comm) -> argc = comm -> argc;
			(new -> comm) -> type = comm -> type;
			(new -> comm) -> id = comm -> id;
			if ((trp -> comm) -> argv) {
				if (!isstatement(new -> comm))
					(new -> comm) -> argv =
						duplvar(comm -> argv, 0);
				else (new -> comm) -> argv =
					(char **)duplstree((syntaxtree *)
						(comm -> argv), new);
			}
			(new -> comm) -> redp = duplredirect(comm -> redp);
		}
	}
	if (trp -> next) {
		if (!(trp -> flags & (ST_QUOT | ST_META)))
			new -> next = duplstree(trp -> next, new);
		else new -> next =
			(syntaxtree *)strdup2((char *)(trp -> next));
	}
	return(new);
}

static shfunctable *NEAR duplfunc(func)
shfunctable *func;
{
	shfunctable *dupl;
	int i, n;

	if (!func) n = 0;
	else for (n = 0; func[n].ident; n++);
	dupl = (shfunctable *)malloc2((n + 1) * sizeof(shfunctable));
	for (i = 0; i < n; i++) {
		dupl[i].ident = strdup2(func[i].ident);
		dupl[i].func = duplstree(func[i].func, NULL);
	}
	dupl[i].ident = NULL;
	return(dupl);
}

static VOID NEAR freefunc(func)
shfunctable *func;
{
	int i;

	if (func) {
		for (i = 0; func[i].ident; i++) {
			free(func[i].ident);
			freestree(func[i].func);
			free(func[i].func);
		}
		free(func);
	}
}

static int cmpfunc(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	shfunctable *fp1, *fp2;

	fp1 = (shfunctable *)vp1;
	fp2 = (shfunctable *)vp2;
	return(strpathcmp2(fp1 -> ident, fp2 -> ident));
}

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

static VOID NEAR freealias(alias)
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

static int NEAR checkalias(trp, ident, len, delim)
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
		&& !(shellalias[i].ident[len])) return(i);
	return(-1);
}
#endif	/* NOALIAS */

static char *NEAR getifs(VOID_A)
{
	char *ifs;

	return((ifs = getshellvar("IFS", -1)) ? ifs : IFS_SET);
}

static int getretval(VOID_A)
{
	return(ret_status);
}

static long getorgpid(VOID_A)
{
	return(orgpid);
}

static long getlastpid(VOID_A)
{
	return(lastpid);
}

static char **getarglist(VOID_A)
{
	return(argvar);
}

static char *getflagstr(VOID_A)
{
	char *cp;
	int i, j;

	cp = malloc2(FLAGSSIZ + 1);
	for (i = j = 0; i < FLAGSSIZ; i++)
		if (*(setvals[i])) cp[j++] = getflags[i];
	cp[j] = '\0';
	return(cp);
}

static int checkundefvar(cp, arg, len)
char *cp, *arg;
int len;
{
	if (cp || !undeferror) return(0);
	cp = malloc2(len + 1);
	strncpy2(cp, arg, len);
	execerror(NULL, cp, ER_PARAMNOTSET);
	free(cp);
	return(-1);
}

static VOID safeexit(VOID_A)
{
	if (interactive_io) return;
	prepareexit(0);
	Xexit2(RET_FAIL);
}

static int NEAR getstatid(trp)
syntaxtree *trp;
{
	int id;

	if (!trp || !isstatement(trp -> comm)
	|| (id = (trp -> comm) -> id) <= 0 || id > STATEMENTSIZ) return(-1);
	return(id - 1);
}

static int NEAR parsestatement(trpp, no, prev, type)
syntaxtree **trpp;
int no, prev, type;
{
	syntaxtree *tmptr;
	int i;

	if (!(statementlist[no].prev[0])) {
		if ((*trpp) -> comm) return(-1);
		if (prev > 0) {
			if ((statementlist[prev - 1].type & STT_FUNC)) {
#ifdef	BASHSTYLE
	/* bash does not allow the function definition without "{ }" */
				if (no != SM_LIST - 1) return(-1);
#endif
			}
			else if (!(statementlist[prev - 1].type
			& STT_NEEDLIST))
				return(-1);
		}
		*trpp = childstree(*trpp, SM_STATEMENT);
	}
	else {
		if (prev <= 0 || !(tmptr = parentshell(*trpp))) return(-1);

		for (i = 0; i < SMPREV; i++) {
			if (!(statementlist[no].prev[i])) continue;
			if (statementlist[no].prev[i] == SM_ANOTHER) return(0);
			if (prev == statementlist[no].prev[i]) break;
		}
		if (i >= SMPREV) return(-1);

		if ((type & STT_NEEDLIST)
		&& (statementbody(tmptr) -> flags & ST_TYPE) != OP_NOT
		&& !(statementbody(tmptr) -> comm))
			return(-1);

		*trpp = tmptr;
	}

	if (statementlist[no].type & STT_NEEDNONE) {
		if (!(tmptr = parentshell(*trpp))) return(-1);
		*trpp = tmptr;
		if (getstatid(tmptr = (tmptr -> parent)) == SM_FUNC - 1) {
			if (!(tmptr = parentshell(tmptr))) return(-1);
			*trpp = tmptr;
		}
	}
	else {
		if (statementlist[no].prev[0]) {
			tmptr = (*trpp) -> next = newstree(*trpp);
			tmptr -> flags = ST_NEXT;
			*trpp = tmptr;
		}
		*trpp = childstree(*trpp, no + 1);
	}
	return(1);
}

static syntaxtree *NEAR _addarg(trp, arg)
syntaxtree *trp;
char *arg;
{
	syntaxtree *tmptr;
	command_t *comm;
	int id;

	if (trp -> flags & ST_NODE) return(trp);

	comm = trp -> comm;
	if (ischild(comm));
	else if (comm) {
		if (isstatement(comm)) return(trp);
		comm -> argc++;
		comm -> argv = (char **)realloc2(comm -> argv,
				(comm -> argc + 1) * sizeof(char *));
		comm -> argv[comm -> argc] = strdup2(arg);
	}
	else if (arg) {
		if ((trp -> flags & ST_NEXT)
		&& (id = getstatid(parentshell(trp))) >= 0
		&& !(statementlist[id].type & STT_NEEDLIST)) {
			syntaxerrno = ER_UNEXPTOK;
			return(trp);
		}

		comm = trp -> comm = newcomm();
		comm -> argc = 0;
		comm -> argv = (char **)malloc2(1 * sizeof(char *));
		comm -> argv[0] = strdup2(arg);
	}

	if ((id = getstatid(trp -> parent)) >= 0)
	switch (statementlist[id].type & STT_TYPE) {
		case STT_FOR:
		case STT_CASE:
			if (!comm || comm -> argc) {
				syntaxerrno = ER_UNEXPNL;
				return(trp);
			}
			comm -> argv = (char **)realloc2(comm -> argv,
					2 * sizeof(char *));
			comm -> argv[1] = NULL;
			comm -> argc = 1;
			trp -> next = newstree(trp);
			(trp -> next) -> flags = ST_NEXT;
			trp = trp -> next;
			break;
		case STT_IN:
			if (!comm) {
				syntaxerrno = ER_UNEXPNL;
				return(trp);
			}
			break;
		case STT_FUNC:
			if (!arg && comm) {
				if (!(tmptr = parentshell(trp))
				|| !(tmptr = parentshell(tmptr))) {
					syntaxerrno = ER_UNEXPNL;
					return(trp);
				}
				trp = tmptr;
			}
			break;
		default:
			break;
	}
	return(trp);
}

static int NEAR addarg(trpp, tok, lenp, typep, from, notok)
syntaxtree **trpp;
char *tok;
int *lenp, *typep, from, notok;
{
	syntaxtree *tmptr;
	int i, n, id, type;

	if ((*trpp) -> flags & ST_NODE) {
		syntaxerrno = ER_UNEXPTOK;
		return(-1);
	}

	if (!*lenp) {
		if (*typep && notok) {
			syntaxerrno = ER_UNEXPTOK;
			return(-1);
		}
		return(0);
	}
	tok[*lenp] = '\0';
	*lenp = 0;

	if (!*typep) {
		id = getstatid(tmptr = parentshell(*trpp));
		type = (id >= 0) ? statementlist[id].type : 0;

		if ((type & STT_NEEDIDENT) && tmptr == (*trpp) -> parent);
		else if (!((*trpp) -> comm) || ischild((*trpp) -> comm)) {
			for (i = 0; i < STATEMENTSIZ; i++)
			if (!strpathcmp(tok, statementlist[i].ident)) {
				n = parsestatement(trpp, i, id + 1, type);
				if (!n) continue;
				else if (n > 0) return(0);

				syntaxerrno = ER_UNEXPTOK;
				return(-1);
			}
		}
		if (isstatement((*trpp) -> comm)) {
			syntaxerrno = ER_UNEXPTOK;
			return(-1);
		}

		*trpp = _addarg(*trpp, tok);
	}
	else if (from >= 0 || (*typep & MD_RDWR) != MD_RDWR) {
		if (redirect(*trpp, from, tok, *typep) < 0) return(-1);
	}
	else {
		if (redirect(*trpp, STDIN_FILENO, tok, *typep) < 0
		|| redirect(*trpp, STDOUT_FILENO, tok, *typep) < 0) return(-1);
	}
	*typep = 0;
	return(0);
}

static int NEAR evalredprefix(trpp, tok, lenp, typep, from)
syntaxtree **trpp;
char *tok;
int *lenp, *typep, from;
{
	int n;

	tok[*lenp] = '\0';
	if ((n = evalfiledesc(tok)) < 0)
		addarg(trpp, tok, lenp, typep, from, 1);
	else if (*typep) {
		syntaxerrno = ER_UNEXPTOK;
		return(-2);
	}
	*lenp = 0;
	return(n);
}

static syntaxtree *NEAR rparen(trp)
syntaxtree *trp;
{
	syntaxtree *tmptr;

	if ((!(trp -> comm) && !(trp -> flags & ST_NEXT))
	|| !(tmptr = parentshell(trp)) || !ischild(tmptr -> comm))
		syntaxerrno = ER_UNEXPTOK;
	else {
		_addarg(trp, NULL);
		trp = _addarg(tmptr, NULL);
	}
	return(trp);
}

static syntaxtree *NEAR semicolon(trp, s, ptrp, typep, num)
syntaxtree *trp;
char *s;
int *ptrp, *typep, num;
{
	char tmptok[3];
	int i, id;

	id = getstatid(parentshell(trp));
	if (!(trp -> comm) && !(trp -> flags & ST_NEXT) && id >= 0) {
		syntaxerrno = ER_UNEXPNL;
		return(trp);
	}

	if (s[*ptrp + 1] != ';') {
		trp = _addarg(trp, NULL);
		trp = linkstree(trp, OP_FG);
	}
	else {
		if (id != SM_RPAREN - 1) {
			syntaxerrno = ER_UNEXPTOK;
			return(trp);
		}
		if (trp -> comm) {
			trp = _addarg(trp, NULL);
			trp = linkstree(trp, OP_FG);
		}
		(*ptrp)++;
		i = 0;
		tmptok[i++] = ';';
		tmptok[i++] = ';';
		addarg(&trp, tmptok, &i, typep, num, 0);
	}
	return(trp);
}

static syntaxtree *NEAR ampersand(trp, s, ptrp, typep, nump)
syntaxtree *trp;
char *s;
int *ptrp, *typep, *nump;
{
	switch (s[*ptrp + 1]) {
		case '&':
			trp = _addarg(trp, NULL);
			(*ptrp)++;
			trp = linkstree(trp, OP_AND);
			break;
		case '>':
			(*ptrp)++;
			*nump = STDOUT_FILENO;
			*typep = (MD_WRITE | MD_WITHERR);
			if (s[*ptrp + 1] == '>') {
				(*ptrp)++;
				*typep |= MD_APPEND;
			}
			else if (s[*ptrp + 1] == '|') {
				(*ptrp)++;
				*typep |= MD_FORCED;
			}
			break;
		case '|':
			trp = _addarg(trp, NULL);
			(*ptrp)++;
			trp = linkstree(trp, OP_NOWN);
			break;
		default:
			trp = _addarg(trp, NULL);
			trp = linkstree(trp, OP_BG);
			break;
	}
	return(trp);
}

static syntaxtree *NEAR vertline(trp, s, ptrp)
syntaxtree *trp;
char *s;
int *ptrp;
{
	trp = _addarg(trp, NULL);
	switch (s[*ptrp + 1]) {
		case '|':
			(*ptrp)++;
			trp = linkstree(trp, OP_OR);
			break;
		case '&':
			(*ptrp)++;
			if (redirect(trp, STDERR_FILENO, "1",
			MD_WRITE | MD_FILEDESC) >= 0)
				trp = linkstree(trp, OP_PIPE);
			break;
		default:
			trp = linkstree(trp, OP_PIPE);
			break;
	}
	return(trp);
}

static syntaxtree *NEAR lessthan(trp, s, ptrp, typep, num)
syntaxtree *trp;
char *s;
int *ptrp, *typep, num;
{
	*typep = MD_READ;
	switch (s[*ptrp + 1]) {
		case '<':
			(*ptrp)++;
			*typep |= MD_HEREDOC;
			if (s[*ptrp + 1] == '-') {
				(*ptrp)++;
				*typep |= MD_APPEND;
			}
			break;
		case '>':
			(*ptrp)++;
			*typep |= MD_WRITE;
			break;
		case '&':
			(*ptrp)++;
			if (s[*ptrp + 1] != '-') *typep |= MD_FILEDESC;
			else {
				(*ptrp)++;
				if (redirect(trp, num, NULL, *typep) >= 0)
					*typep = 0;
			}
			break;
		case '-':
			(*ptrp)++;
			if (redirect(trp, num, NULL, *typep) >= 0) *typep = 0;
			break;
		default:
			break;
	}
	return(trp);
}

static syntaxtree *NEAR morethan(trp, s, ptrp, typep, num)
syntaxtree *trp;
char *s;
int *ptrp, *typep, num;
{
	*typep = MD_WRITE;
	switch (s[*ptrp + 1]) {
		case '<':
			(*ptrp)++;
			*typep |= MD_READ;
			break;
		case '>':
			(*ptrp)++;
			*typep |= MD_APPEND;
			break;
		case '&':
			(*ptrp)++;
			if (s[*ptrp + 1] != '-') *typep |= MD_FILEDESC;
			else {
				(*ptrp)++;
				if (redirect(trp, num, NULL, *typep) >= 0)
					*typep = 0;
			}
			break;
		case '-':
			(*ptrp)++;
			if (redirect(trp, num, NULL, *typep) >= 0) *typep = 0;
			break;
		case '|':
			(*ptrp)++;
			*typep |= MD_FORCED;
			break;
		default:
			break;
	}
	return(trp);
}

static syntaxtree *NEAR normaltoken(trp, s, ptrp, typep, nump, tok, tptrp, st)
syntaxtree *trp;
char *s;
int *ptrp, *typep, *nump;
char *tok;
int *tptrp, st;
{
	char tmptok[2];
	int i;

	switch (s[*ptrp]) {
		case '{':
			if (!strchr(IFS_SET, s[*ptrp + 1])) {
				tok[(*tptrp)++] = s[*ptrp];
				break;
			}
			if (addarg(&trp, tok, tptrp, typep, *nump, 0) < 0)
				break;
			i = 0;
			tmptok[i++] = s[*ptrp];
			addarg(&trp, tmptok, &i, typep, *nump, 0);
			break;
		case '}':
			tok[(*tptrp)++] = s[*ptrp];
			if ((*tptrp) > 1 || st != STT_LIST) break;
			if (trp -> comm) {
#ifdef	BASHSTYLE
	/* bash allows the list which does not end with ";" */
				trp = _addarg(trp, NULL);
				trp = linkstree(trp, OP_NONE);
#else
				break;
#endif
			}
			addarg(&trp, tok, tptrp, typep, *nump, 0);
			break;
		case '(':
			if (hascomm(trp)) {
				if (*typep || *tptrp || (trp -> comm) -> argc)
					syntaxerrno = ER_UNEXPTOK;
				else {
					trp = _addarg(trp, NULL);
					trp = linkstree(trp, OP_NONE);
					i = 0;
					tmptok[i++] = s[*ptrp];
					addarg(&trp, tmptok, &i,
						typep, *nump, 0);
				}
			}
			else if (*tptrp) {
				if (!*tptrp
				|| addarg(&trp, tok, tptrp,
					typep, *nump, 0) < 0)
					syntaxerrno = ER_UNEXPTOK;
				else {
					trp = _addarg(trp, NULL);
					trp = linkstree(trp, OP_NONE);
					i = 0;
					tmptok[i++] = s[*ptrp];
					addarg(&trp, tmptok, &i,
						typep, *nump, 0);
				}
			}
			else if (*typep) syntaxerrno = ER_UNEXPTOK;
#ifdef	BASHSTYLE
	/* bash does not allow the function definition without "{ }" */
			else if (getstatid(parentshell(trp)) == SM_FUNC - 1)
				syntaxerrno = ER_UNEXPTOK;
#endif
			else trp = childstree(trp, SM_CHILD);
			break;
		case ')':
			if (st == STT_LPAREN) {
				if (*tptrp || trp -> comm)
					syntaxerrno = ER_UNEXPTOK;
				else {
					i = 0;
					tmptok[i++] = s[*ptrp];
					addarg(&trp, tmptok, &i,
						typep, *nump, 0);
				}
				break;
			}
			if (addarg(&trp, tok, tptrp, typep, *nump, 0) >= 0)
				trp = rparen(trp);
			break;
		case ';':
			if (addarg(&trp, tok, tptrp, typep, *nump, 1) >= 0)
				trp = semicolon(trp, s, ptrp, typep, *nump);
			break;
		case '&':
			if (addarg(&trp, tok, tptrp, typep, *nump, 1) >= 0)
				trp = ampersand(trp, s, ptrp, typep, nump);
			break;
		case '|':
			if (addarg(&trp, tok, tptrp, typep, *nump, 1) >= 0)
				trp = vertline(trp, s, ptrp);
			break;
		case '!':
			if (*tptrp || trp -> comm) tok[(*tptrp)++] = s[*ptrp];
			else if (*typep || (trp -> flags & ST_NODE)) {
				syntaxerrno = ER_UNEXPTOK;
				return(trp);
			}
			else trp = linkstree(trp, OP_NOT);
			break;
		case '<':
			if ((*nump = evalredprefix(&trp, tok, tptrp,
			typep, *nump)) >= -1)
				trp = lessthan(trp, s, ptrp, typep, *nump);
			break;
		case '>':
			if ((*nump = evalredprefix(&trp, tok, tptrp,
			typep, *nump)) >= -1)
				trp = morethan(trp, s, ptrp, typep, *nump);
			break;
		case '\r':
#ifdef	BASHSTYLE
	/* bash treats '\r' as just a character */
			if (*typep) syntaxerrno = ER_UNEXPNL;
			else tok[(*tptrp)++] = s[*ptrp];
			break;
#endif
		case '\n':
			if (addarg(&trp, tok, tptrp, typep, *nump, 1) < 0)
				break;
			i = getstatid(parentshell(trp)) + 1;
			if (trp -> comm) {
				trp = _addarg(trp, NULL);
				trp = linkstree(trp, OP_FG);
			}
			else if (!(trp -> flags & ST_NEXT) && i > 0) {
				if (i == SM_FOR || i == SM_CASE || i == SM_IN
				|| *tptrp)
					syntaxerrno = ER_UNEXPNL;
			}
			break;
		default:
			if (!(*tptrp) && s[*ptrp] == '#')
				while (s[*ptrp + 1] && s[*ptrp + 1] != '\r'
				&& s[*ptrp + 1] != '\n') (*ptrp)++;
			else if (!strchr(IFS_SET, s[*ptrp]))
				tok[(*tptrp)++] = s[*ptrp];
			else addarg(&trp, tok, tptrp, typep, *nump, 0);
			break;
	}
	return(trp);
}

static syntaxtree *NEAR casetoken(trp, s, ptrp, typep, nump, tok, tptrp)
syntaxtree *trp;
char *s;
int *ptrp, *typep, *nump;
char *tok;
int *tptrp;
{
	char tmptok[2];
	int i;

	switch (s[*ptrp]) {
		case ')':
			if (*tptrp < 0) *tptrp = 0;
			else if (addarg(&trp, tok, tptrp, typep, *nump, 0) < 0)
				break;
			trp = _addarg(trp, NULL);
			trp = linkstree(trp, OP_NONE);
			i = 0;
			tmptok[i++] = s[*ptrp];
			addarg(&trp, tmptok, &i, typep, *nump, 0);
			break;
		case '|':
			if (*tptrp < 0) *tptrp = 0;
			else if (!*tptrp) syntaxerrno = ER_UNEXPTOK;
			else addarg(&trp, tok, tptrp, typep, *nump, 0);
			break;
		case ';':
			if (*tptrp
			&& addarg(&trp, tok, tptrp, typep, *nump, 1) >= 0) {
				int id, stype;

				id = getstatid(parentshell(trp));
				stype = (id >= 0)
					? (statementlist[id].type & STT_TYPE)
					: 0;
				/* for "esac;" */
				if (stype != STT_INCASE
				&& stype != STT_CASEEND) {
					trp = semicolon(trp, s, ptrp,
						typep, *nump);
					break;
				}
			}
/*FALLTHRU*/
		case '{':
		case '}':
		case '&':
		case '<':
		case '>':
			syntaxerrno = ER_UNEXPTOK;
			break;
		case '(':
#ifdef	SIRICTPOSIX
			if (!*tptrp && !(trp -> comm)) break;
#endif
			syntaxerrno = ER_UNEXPTOK;
			break;
		case '\r':
#ifdef	BASHSTYLE
	/* bash treats '\r' as just a character */
			syntaxerrno = ER_UNEXPNL;
			break;
#endif
		case '\n':
			if (addarg(&trp, tok, tptrp, typep, *nump, 1) < 0)
				break;
			i = getstatid(parentshell(trp)) + 1;
			if (trp -> comm) {
				trp = _addarg(trp, NULL);
				trp = linkstree(trp, OP_FG);
			}
			else if (!(trp -> flags & ST_NEXT) && i > 0) {
				if (*tptrp) syntaxerrno = ER_UNEXPNL;
			}
			break;
		default:
			if (!*tptrp && s[*ptrp] == '#')
				while (s[*ptrp + 1] && s[*ptrp + 1] != '\r'
				&& s[*ptrp + 1] != '\n') (*ptrp)++;
			else if (!strchr(IFS_SET, s[*ptrp])) {
				if (*tptrp < 0) syntaxerrno = ER_UNEXPTOK;
				tok[(*tptrp)++] = s[*ptrp];
			}
			else if (*tptrp > 0) {
				addarg(&trp, tok, tptrp, typep, *nump, 0);
				*tptrp = -1;
			}
			break;
	}
	return(trp);
}

/*ARGSUSED*/
static syntaxtree *NEAR analyzeloop(trp, s, typep, nump, tokp, qp, quiet)
syntaxtree *trp;
char *s;
int *typep, *nump;
char **tokp;
int *qp, quiet;
{
	char *cp;
	int i, j, id, size, stype;
#ifndef	NOALIAS
	int n;
#endif

	if (!(*tokp)) {
		j = 0;
		*tokp = c_malloc(size);
	}
	else {
		j = strlen(*tokp);
		for (size = BUFUNIT; size < j + 1; size *= 2);
	}

	for (i = 0; s && s[i]; i++) {
		syntaxerrno = 0;
		id = getstatid(parentshell(trp));
		stype = (id >= 0) ? (statementlist[id].type & STT_TYPE) : 0;
		if (stype != STT_INCASE && stype != STT_CASEEND && j < 0)
			j = 0;
		*tokp = c_realloc(*tokp, j + 2, size);

		if (s[i] == *qp) {
			(*tokp)[j++] = s[i];
			*qp = '\0';
		}
#ifdef	CODEEUC
		else if (isekana(s, i)) {
			if (j < 0) syntaxerrno = ER_UNEXPTOK;
			else {
				(*tokp)[j++] = s[i++];
				(*tokp)[j++] = s[i];
			}
		}
#endif
		else if (iskanji1(s, i)) {
			if (j < 0) syntaxerrno = ER_UNEXPTOK;
			else {
				(*tokp)[j++] = s[i++];
				(*tokp)[j++] = s[i];
			}
		}
		else if (*qp == '\'') (*tokp)[j++] = s[i];
		else if (s[i] == PMETA) {
			if (j < 0) syntaxerrno = ER_UNEXPTOK;
			else {
				(*tokp)[j++] = s[i++];
				if (s[i]) (*tokp)[j++] = s[i];
				else {
					trp -> flags |= ST_META;
					break;
				}
			}
		}
		else if (*qp) (*tokp)[j++] = s[i];
		else if (s[i] == '\'' || s[i] == '"' || s[i] == '`') {
			if (j < 0) syntaxerrno = ER_UNEXPTOK;
			else *qp = (*tokp)[j++] = s[i];
		}
		else if (stype == STT_INCASE || stype == STT_CASEEND)
			trp = casetoken(trp, s, &i, typep, nump, *tokp, &j);
		else {
#ifndef	NOALIAS
			if ((n = checkalias(trp, *tokp, j, s[i])) >= 0) {
				**tokp = '\0';
				shellalias[n].flags |= AL_USED;
				trp = analyzeloop(trp, shellalias[n].comm,
					typep, nump, tokp, qp, quiet);
				shellalias[n].flags &= ~AL_USED;
				if (!trp) return(NULL);
				j = strlen(*tokp);
				i--;
			}
			else
#endif
			trp = normaltoken(trp, s, &i, typep, nump,
				*tokp, &j, stype);
		}

		if (syntaxerrno) {
			if (strchr(IFS_SET, s[i])) {
				if (j > 0) (*tokp)[j] = '\0';
				if (!quiet) syntaxerror(*tokp, syntaxerrno);
				return(NULL);
			}
			for (j = i + 1; s[j]; j++) {
				if (strchr(IFS_SET, s[j])) break;
#ifdef	CODEEUC
				else if (isekana(s, j)) j++;
#endif
				else if (iskanji1(s, j)) j++;
			}
			if (!quiet) {
				cp = malloc2(j - i + 1);
				strncpy2(cp, &(s[i]), j - i);
				syntaxerror(cp, syntaxerrno);
				free(cp);
			}
			return(NULL);
		}
	}
	(*tokp)[j] = '\0';
#ifndef	NOALIAS
	if ((n = checkalias(trp, *tokp, j, '\0')) >= 0) {
		**tokp = '\0';
		shellalias[n].flags |= AL_USED;
		trp = analyzeloop(trp, shellalias[n].comm,
			typep, nump, tokp, qp, quiet);
		shellalias[n].flags &= ~AL_USED;
	}
#endif
	return(trp);
}

syntaxtree *analyze(s, stree, quiet)
char *s;
syntaxtree *stree;
int quiet;
{
	syntaxtree *trp;
	char *tok;
	int i, id, type, stype, num, quote;

	trp = stree;
	type = MD_NORMAL;
	num = -1;
	syntaxerrno = 0;
	quote = '\0';

	if (trp -> flags & (ST_QUOT | ST_META)) {
		tok = (char *)(trp -> next);
		i = strlen(tok);
		if (i > 0) {
			if (trp -> flags & ST_QUOT) quote = tok[--i];
			if ((trp -> flags & ST_META) && quote != '\'') i--;
			else if (s) tok[i++] = '\n';
		}
#ifndef	BASHSTYLE
	/* bash does not allow unclosed quote */
		if (!s || s == (char *)-1) {
			if (trp -> flags & ST_QUOT) tok[i++] = quote;
			s = NULL;
			quote = '\0';
		}
#endif
		tok[i] = '\0';
	}
	else if (s) tok = NULL;
	else {
		freestree(stree);
		return(NULL);
	}

	trp -> next = NULL;
	trp -> flags &= ~ST_CONT;

	exectrapcomm();
	if (s) {
		for (i = 0; shellalias[i].ident; i++)
			shellalias[i].flags &= ~AL_USED;
		trp = analyzeloop(trp, s, &type, &num, &tok, &quote, quiet);
		if (!trp) {
			if (tok) free(tok);
			freestree(stree);
			syntaxerrno = 0;
			return(NULL);
		}
	}

	id = getstatid(parentshell(trp));
	stype = (id >= 0) ? (statementlist[id].type & STT_TYPE) : 0;

	i = (tok) ? strlen(tok) : 0;
	if (quote) {
		tok[i++] = quote;
		tok[i] = '\0';
		trp -> next = (syntaxtree *)tok;
		tok = NULL;
		trp -> flags |= ST_QUOT;
	}
	else if (trp -> flags & ST_META) {
		trp -> next = (syntaxtree *)tok;
		tok = NULL;
	}
	else if (stype == STT_INCASE) syntaxerrno = ER_UNEXPNL;
	else if (addarg(&trp, tok, &i, &type, num, 1) < 0);
	else if (!(trp = _addarg(trp, NULL)) || syntaxerrno);
	else if (trp -> comm) trp = linkstree(trp, OP_FG);
	else if (trp -> parent) {
		if (((trp -> parent) -> flags & ST_TYPE) == OP_NOT)
			syntaxerrno = ER_UNEXPNL;
		if (((trp -> parent) -> flags & ST_TYPE) != OP_FG
		&& ((trp -> parent) -> flags & ST_TYPE) != OP_BG
		&& ((trp -> parent) -> flags & ST_TYPE) != OP_NOWN)
			trp -> flags |= ST_STAT;
	}
	if (syntaxerrno) {
		if (!quiet) syntaxerror(tok, syntaxerrno);
		if (tok) free(tok);
		freestree(stree);
		return(NULL);
	}
	if (tok) free(tok);
#ifndef	BASHSTYLE
	/* bash does not allow unclosed quote */
	if (s);
	else if (!(trp -> flags & ST_STAT) && !parentshell(trp)) return(trp);
	else {
		freestree(stree);
		return(NULL);
	}
#endif
	if (parentshell(trp)) trp -> flags |= ST_STAT;

	return(trp);
}

static syntaxtree *NEAR statementcheck(trp, id)
syntaxtree *trp;
int id;
{
	if (!trp || !isstatement(trp -> comm)
	|| (id > 0 && (trp -> comm) -> id != id)
	|| !(trp = statementbody(trp))) {
		errno = EINVAL;
		return(NULL);
	}
	return(trp);
}

static int NEAR check_statement(trp)
syntaxtree *trp;
{
	syntaxtree *body;
	int i, id, prev;

	if ((trp -> comm) -> id == SM_CHILD)
		return(check_stree(statementbody(trp)));
	if ((id = getstatid(trp = statementcheck(trp, SM_STATEMENT))) < 0
	|| !statementlist[id].func || statementlist[id].prev[0]) {
		errno = EINVAL;
		doperror(NULL, shellname);
		return(-1);
	}

	for (;;) {
		if (statementlist[id].type & STT_NEEDLIST) {
			if (!(body = statementcheck(trp, 0))) {
				doperror(NULL, statementlist[id].ident);
				return(-1);
			}
			if (check_stree(body)) return(-1);
		}
		if (!(trp = trp -> next) || !(trp -> comm)) break;
		prev = id + 1;
		if ((id = getstatid(trp)) < 0) i = SMPREV;
		else for (i = 0; i < SMPREV; i++) {
			if (!(statementlist[id].prev[i])) continue;
			if (prev == statementlist[id].prev[i]) break;
		}
		if (i >= SMPREV) {
			errno = EINVAL;
			doperror(NULL, statementlist[id].ident);
			return(-1);
		}
	}
	return(0);
}

static int NEAR check_command(trp)
syntaxtree *trp;
{
	command_t *comm;
	char **argv, **subst;
	int id, type, argc, *len;

	comm = trp -> comm;
	argc = comm -> argc;
	argv = comm -> argv;
	if (argc <= 0) return(0);

	comm -> argv = duplvar(comm -> argv, 0);
	comm -> argc = getsubst(comm -> argc, comm -> argv, &subst, &len);
	if ((id = evalargv(comm, &type, 0)) >= 0 && type == CT_COMMAND)
		id = searchhash(&(comm -> hash), comm -> argv[0], NULL);
	freevar(subst);
	free(len);
	comm -> argc = argc;
	freevar(comm -> argv);
	comm -> argv = argv;
	return((id >= 0) ? 0 : -1);
}

static int NEAR check_stree(trp)
syntaxtree *trp;
{
	syntaxtree *tmptr;

	if (!trp) {
		errno = EINVAL;
		doperror(NULL, shellname);
		return(-1);
	}
	if (!(trp -> comm)) return(0);
	if (trp -> flags & ST_NODE) {
		if (check_stree((syntaxtree *)(trp -> comm)) < 0) return(-1);
	}
	else if (isstatement(trp -> comm)) {
		if (check_statement(trp) < 0) return(-1);
	}
	else if (getstatid(tmptr = statementcheck(trp -> next, SM_STATEMENT))
	== SM_LPAREN - 1) {
		if (identcheck((trp -> comm) -> argv[0], '\0') <= 0) {
			execerror(NULL, (trp -> comm) -> argv[0], ER_NOTIDENT);
			return(-1);
		}
		if (!statementcheck(tmptr -> next, 0)) {
			errno = EINVAL;
			doperror(NULL, shellname);
			return(-1);
		}
		trp = trp -> next;
	}
	else if (check_command(trp) < 0) return(-1);

	return((trp -> next) ? check_stree(trp -> next) : 0);
}

static syntaxtree *NEAR analyzeline(command)
char *command;
{
	syntaxtree *trp, *stree;

	stree = newstree(NULL);
	trp = analyze(command, stree, 0);
	if (trp && (trp -> flags & ST_CONT)) {
#ifndef	BASHSTYLE
	/* bash does not allow unclosed quote */
		if (!(trp -> flags & ST_STAT)
		&& (trp = analyze(NULL, trp, 0)));
	/* bash does not allow the format like as "foo |" */
		else if ((trp -> flags & ST_NEXT) && trp -> parent
		&& ((trp -> parent) -> flags & ST_TYPE) == OP_PIPE);
		else
#endif
		{
			syntaxerror("", ER_UNEXPEOF);
			freestree(stree);
			free(stree);
			return(NULL);
		}
	}
	if (!trp) {
		freestree(stree);
		free(stree);
		return(NULL);
	}
#ifndef	_NOUSEHASH
	if (hashahead && check_stree(stree) < 0) {
		freestree(stree);
		free(stree);
		return(NULL);
	}
#endif
	return(stree);
}

/*ARGSUSED*/
static VOID NEAR Xexecve(path, argv, envp, bg)
char *path, *argv[], *envp[];
int bg;
{
	int fd, ret;

	execve(path, argv, envp);
	if (errno != ENOEXEC) {
		if (errno == EACCES) {
			execerror(NULL, argv[0], ER_CANNOTEXE);
			ret = RET_NOTEXEC;
		}
		else {
			doperror(NULL, argv[0]);
			ret = RET_FAIL;
		}
	}
	else if ((fd = newdup(Xopen(path, O_BINARY | O_RDONLY, 0666))) < 0) {
		doperror(NULL, argv[0]);
		ret = RET_NOTEXEC;
	}
	else {
		sourcefile(fd, argv[0], 0);
		ret = ret_status;
	}
	prepareexit(1);
#ifdef	DEBUG
	if (!bg) Xexit2(ret);
#endif
	Xexit(ret);
}

#if	MSDOS
static char *NEAR addext(path, ext)
char *path;
int ext;
{
	int len;

	len = strlen(path);
	path = realloc2(path, len + 1 + 3 + 1);
	path[len++] = '.';
	if (ext & CM_BATCH) strcpy(path + len, "BAT");
	else if (ext & CM_EXE) strcpy(path + len, "EXE");
	else strcpy(path + len, "COM");

	return(path);
}

static char **NEAR replacebat(pathp, argv)
char **pathp, **argv;
{
	char *com;
	int i;

	if (!(com = getshellvar("COMSPEC", -1))
	&& !(com = getshellvar("SHELL", -1))) com = "\\COMMAND.COM";

	for (i = 0; argv[i]; i++);
	for (i += 2; i > 2; i--) argv[i] = argv[i - 2];
	free(argv[2]);
	argv[2] = strdup2(*pathp);
	argv[1] = strdup2("/C");
	argv[0] = *pathp = strdup2(com);
	return(argv);
}
#endif

/*ARGSUSED*/
static int NEAR openpipe(pidp, fdin, new, tty, parent)
long *pidp;
int fdin, new, tty, parent;
{
#if	!MSDOS && !defined (USEFAKEPIPE)
	long pid;
	int fildes[2];
#endif
	pipelist *pl;
	char pfile[MAXPATHLEN];
	int fd, dupl;

	pl = (pipelist *)malloc2(sizeof(pipelist));
	pl -> file = NULL;
	pl -> fp = NULL;
	pl -> fd = fdin;
	pl -> old = -1;
#if	!MSDOS && !defined (USEFAKEPIPE)
	if (pipe(fildes) < 0) pid = -1;
	else if ((pid = makechild(tty, parent)) < 0) {
		safeclose(fildes[0]);
		safeclose(fildes[1]);
	}
	else if (!pid) {
		if ((fd = newdup(Xdup(STDOUT_FILENO))) < 0) {
			prepareexit(1);
			Xexit(RET_NOTEXEC);
		}
		if (fildes[1] != STDOUT_FILENO)
			Xdup2(fildes[1], STDOUT_FILENO);
		safeclose(fildes[0]);
		safeclose(fildes[1]);
		pl -> old = fd;
	}
	else {
		if (new) fd = newdup(fildes[0]);
		else {
			if ((fd = newdup(Xdup(fdin))) < 0) {
# ifndef	NOJOB
				if (stoppedjob(pid));
				else
# endif
				{
					kill(pid, SIGPIPE);
					while (!waitjob(pid, NULL, WUNTRACED))
						if (interrupted) break;
				}
				free(pl);
				safeclose(fildes[0]);
				safeclose(fildes[1]);
				return(-1);
			}
# ifndef	NOJOB
			stackjob(pid, 0, NULL);
# endif
			if (fildes[0] != fdin) Xdup2(fildes[0], fdin);
			safeclose(fildes[0]);
			pl -> old = fd;
		}
		safeclose(fildes[1]);
	}
	if (pid >= 0) {
		pl -> new = fd;
		pl -> pid = *pidp = pid;
		pl -> next = pipetop;
		pipetop = pl;
		return(fd);
	}
#endif	/* !MSDOS && !USEFAKEPIPE */
	if ((dupl = newdup(Xdup(STDOUT_FILENO))) < 0) {
		free(pl);
		return(-1);
	}
	if ((fd = newdup(mktmpfile(pfile, PIPEDIR))) < 0) {
		free(pl);
		safeclose(dupl);
		return(-1);
	}
	if (fd != STDOUT_FILENO) {
		Xclose(STDOUT_FILENO);
		Xdup2(fd, STDOUT_FILENO);
	}
	safeclose(fd);

	if (new) pl -> fd = -1;
	pl -> file = strdup2(pfile);
	pl -> new = pl -> old = dupl;
	pl -> pid = *pidp = 0;
	pl -> next = pipetop;
	pipetop = pl;
	return(dupl);
}

static pipelist **NEAR searchpipe(fd)
int fd;
{
	pipelist **prevp;

	prevp = &pipetop;
	while (*prevp) {
		if (fd == (*prevp) -> new) break;
		prevp = &((*prevp) -> next);
	}
	if (!(*prevp)) return(NULL);
	return(prevp);
}

static int NEAR reopenpipe(fd, ret)
int fd, ret;
{
	pipelist *pl, **prevp;
	int dupl;

	if (!(prevp = searchpipe(fd))) return(-1);
	pl = *prevp;

	if (pl -> pid > 0) return(fd);
	if (pl -> old != STDOUT_FILENO) {
		Xclose(STDOUT_FILENO);
		Xdup2(pl -> old, STDOUT_FILENO);
	}
	safeclose(pl -> old);
	pl -> old = -1;
	if (!(pl -> file)) {
		prepareexit(1);
		Xexit(ret);
	}

	pl -> ret = ret;
	if ((fd = newdup(Xopen(pl -> file, O_BINARY | O_RDONLY, 0666))) < 0) {
		rmtmpfile(pl -> file);
		*prevp = pl -> next;
		free(pl);
		return(-1);
	}
	if (pl -> fd >= 0) {
		if ((dupl = newdup(Xdup(pl -> fd))) < 0) {
			safeclose(fd);
			rmtmpfile(pl -> file);
			*prevp = pl -> next;
			free(pl);
			return(-1);
		}
		if (fd != pl -> fd) Xdup2(fd, pl -> fd);
		safeclose(fd);
		fd = pl -> old = dupl;
	}

	pl -> new = fd;
	return(fd);
}

static FILE *NEAR fdopenpipe(fd)
int fd;
{
	pipelist *pl, **prevp;
	FILE *fp;

	if (!(prevp = searchpipe(fd))) return(NULL);
	pl = *prevp;
	if (!(fp = Xfdopen(fd, "r"))) {
		safeclose(fd);
		rmtmpfile(pl -> file);
		*prevp = pl -> next;
		free(pl);
		return(NULL);
	}
	pl -> fp = fp;
	return(fp);
}

static int NEAR closepipe(fd)
int fd;
{
	pipelist *pl, **prevp;
	int ret;

	if (!(prevp = searchpipe(fd))) return(-1);
	pl = *prevp;
#if	!MSDOS && !defined (NOJOB)
	if (pl -> pid > 0 && stoppedjob(pl -> pid) > 0) return(RET_SUCCESS);
#endif

	if (pl -> old >= 0 && pl -> old != pl -> fd) {
		safeclose(pl -> fd);
		Xdup2(pl-> old, pl -> fd);
	}
	if (fd != STDIN_FILENO &&
	fd != STDOUT_FILENO && fd != STDERR_FILENO) {
		if (pl -> fp) safefclose(pl -> fp);
		else safeclose(pl -> old);
	}

	if (pl -> file) {
		if (rmtmpfile(pl -> file) < 0) doperror(NULL, pl -> file);
		ret = pl -> ret;
	}
#if	!MSDOS
	else if (!(pl -> pid)) {
		prepareexit(1);
		Xexit(RET_SUCCESS);
	}
	else if (pl -> pid > 0) {
# ifdef	NOJOB
		wait_t w;

		kill(pl -> pid, SIGPIPE);
		while (!(ret = waitjob(pl -> pid, &w, WUNTRACED)))
			if (interrupted) break;
		if (ret < 0) {
			if (errno == ECHILD) ret = errno = 0;
		}
		else if (WIFSTOPPED(w)) ret = -1;
		else if (WIFSIGNALED(w) && WTERMSIG(w) != SIGPIPE)
			ret = -1;
# else	/* !NOJOB */
		ret = waitchild(pl -> pid, NULL);
# endif	/* !NOJOB */
	}
#endif	/* !MSDOS */
	else ret = RET_SUCCESS;

	*prevp = pl -> next;
	free(pl);

	return(ret);
}

#ifndef	_NOUSEHASH
static VOID NEAR disphash(VOID_A)
{
	hashlist *hp;
	char buf[7 + 1];
	int i, j;

	fputs("hits    cost    command\n", stdout);
	if (hashtable) for (i = 0; i < MAXHASH; i++)
		for (hp = hashtable[i]; hp; hp = hp -> next) {
			ascnumeric(buf, hp -> hits, 0, 6);
			for (j = 0; buf[j]; j++);
			buf[j++] = (hp -> type & CM_RECALC) ? '*' : ' ';
			while (j < 7) buf[j++] = ' ';
			buf[j] = '\0';
			fputs(buf, stdout);
			fputc(' ', stdout);
			ascnumeric(buf, hp -> cost, -1, 7);
			fputs(buf, stdout);
			fputc(' ', stdout);
			kanjifputs(hp -> path, stdout);
			fputc('\n', stdout);
		}
	fflush(stdout);
}
#endif	/* !_NOUSEHASH */

static char *evalbackquote(arg)
char *arg;
{
	FILE *fp;
	char *buf;
	int i, c, size, duptrapok;

	duptrapok = trapok;
	trapok = 0;
	fp = _dopopen(arg);
	trapok = duptrapok;
	if (!fp) {
		if (errno) doperror(NULL, shellname);
		buf = NULL;
		ret_status = RET_NOTEXEC;
	}
	else {
		buf = c_malloc(size);
		for (i = 0; (c = fgetc(fp)) != EOF; i++) {
			buf = c_realloc(buf, i, size);
			buf[i] = c;
		}
#ifdef	BASHSTYLE
	/* bash ignores any following newlines */
		while (i > 0 && buf[i - 1] == '\n') i--;
#else
		if (i > 0 && buf[i - 1] == '\n') i--;
#endif
		buf[i] = '\0';
		ret_status = dopclose(fp);
	}
	return(buf);
}

/*ARGSUSED*/
static int NEAR checktype(s, idp, alias, func)
char *s;
int *idp, alias, func;
{
	int i;

	if (!s) {
		if (idp) *idp = 0;
		return(CT_NONE);
	}
	else if (!strdelim(s, 1)) {
#ifndef	NOALIAS
		if (alias) {
			for (i = 0; shellalias[i].ident; i++)
			if (!strpathcmp(s, shellalias[i].ident)) {
				if (idp) *idp = i;
				return(CT_ALIAS);
			}
		}
#endif
#ifdef	STRICTPOSIX
		for (i = 0; i < SHBUILTINSIZ; i++) {
			if (!(shbuiltinlist[i].flags & BT_POSIXSPECIAL))
				continue;
			if (!strpathcmp(s, shbuiltinlist[i].ident)) {
				if (idp) *idp = i;
				return(CT_BUILTIN);
			}
		}
#endif
		if (func) {
			for (i = 0; shellfunc[i].ident; i++)
			if (!strpathcmp(s, shellfunc[i].ident)) {
				if (idp) *idp = i;
				return(CT_FUNCTION);
			}
		}
#if	MSDOS
		if (_dospath(s) && (!s[2] || (s[2] == _SC_ && !s[3]))) {
			if (idp) *idp = *s;
			return(CT_LOGDRIVE);
		}
#endif
		for (i = 0; i < SHBUILTINSIZ; i++) {
#ifdef	STRICTPOSIX
			if (shbuiltinlist[i].flags & BT_POSIXSPECIAL)
				continue;
#endif
			if (!strpathcmp(s, shbuiltinlist[i].ident)) {
				if (idp) *idp = i;
				return(CT_BUILTIN);
			}
		}
#ifdef	FD
		if ((i = checkbuiltin(s)) >= 0) {
			if (idp) *idp = i;
			return(CT_FDORIGINAL);
		}
		if (!shellmode && (i = checkinternal(s)) >= 0) {
			if (idp) *idp = i;
			return(CT_FDINTERNAL);
		}
#endif
	}
	if (idp) *idp = 0;
	return(CT_COMMAND);
}

#if	!defined (FDSH) && !defined (_NOCOMPLETE)
int completeshellcomm(s, len, argc, argvp)
char *s;
int len, argc;
char ***argvp;
{
	int i;

# ifndef	NOALIAS
	for (i = 0; shellalias[i].ident; i++) {
		if (strnpathcmp(s, shellalias[i].ident, len)
		|| finddupl(shellalias[i].ident, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(shellalias[i].ident);
	}
# endif	/* !NOALIAS */
	for (i = 0; i < STATEMENTSIZ; i++) {
		if (strnpathcmp(s, statementlist[i].ident, len)
		|| finddupl(statementlist[i].ident, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(statementlist[i].ident);
	}
	for (i = 0; i < SHBUILTINSIZ; i++) {
		if (strnpathcmp(s, shbuiltinlist[i].ident, len)
		|| finddupl(shbuiltinlist[i].ident, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(shbuiltinlist[i].ident);
	}
# ifdef	FD
	argc = completebuiltin(s, len, argc, argvp);
	if (!shellmode) argc = completeinternal(s, len, argc, argvp);
# endif
	for (i = 0; shellfunc[i].ident; i++) {
		if (strnpathcmp(s, shellfunc[i].ident, len)
		|| finddupl(shellfunc[i].ident, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(shellfunc[i].ident);
	}
	return(argc);
}
#endif	/* !FDSH && !_NOCOMPLETE */

int getsubst(argc, argv, substp, lenp)
int argc;
char **argv, ***substp;
int **lenp;
{
	int i, j, n, len;

	*substp = (char **)malloc2((argc + 1) * sizeof(char *));
	*lenp = (int *)malloc2(argc * sizeof(int));

	for (i = n = 0; i < argc; i++) {
		len = (freeenviron || n >= i) ? identcheck(argv[i], '=') : -1;
		if (len > 0) {
			(*substp)[n] = argv[i];
			(*lenp)[n] = len;
			for (j = i; j < argc; j++) argv[j] = argv[j + 1];
			n++;
			argc--;
			i--;
		}
	}
	(*substp)[n] = NULL;
	return(argc);
}

static char *NEAR evalvararg(arg, stripq, backq)
char *arg;
int stripq, backq;
{
	char *tmp;

	if ((tmp = evalarg(arg, stripq, backq))) return(tmp);
	if (*arg) execerror(NULL, arg, ER_BADSUBST);
	return(NULL);
}

static int NEAR substvar(argv)
char **argv;
{
	char *tmp;
	int i;

	for (i = 0; argv[i]; i++) {
		trapok = 1;
		tmp = evalvararg(argv[i], 0, 1);
		trapok = 0;
		if (!tmp) return(-1);
		free(argv[i]);
		argv[i] = tmp;
	}
	return(i);
}

static int NEAR evalargv(comm, typep, valid)
command_t *comm;
int *typep, valid;
{
	char *tmp;
	int i, id;

	if (!comm) return(-1);
	else if (comm -> type) {
		*typep = comm -> type;
		return(comm -> id);
	}

	if (valid && substvar(comm -> argv) < 0) return(-1);

	trapok = 1;
#ifdef	BASHSTYLE
	/* bash does not use IFS as a command separator */
	comm -> argc = evalifs(comm -> argc, &(comm -> argv), " \t");
#else
	comm -> argc = evalifs(comm -> argc, &(comm -> argv), getifs());
#endif
	if (!(comm -> argc)) {
		*typep = CT_NONE;
		id = 0;
	}
	else {
		stripquote(tmp = strdup2(comm -> argv[0]), 1);
		*typep = checktype(tmp, &id, 0, 1);
		free(tmp);
	}

#if	MSDOS
	if (*typep == CT_COMMAND) {
		if (!noglob) comm -> argc = evalglob(comm -> argc,
				&(comm -> argv), 0);
		else {
			stripquote(comm -> argv[0], 1);
			for (i = 1; i < comm -> argc; i++)
				stripquote(comm -> argv[i], 0);
		}
	}
	else
#endif	/* MSDOS */
	if (!noglob
#ifdef	FD
	&& *typep != CT_FDINTERNAL
#endif
	&& (*typep != CT_BUILTIN || !(shbuiltinlist[id].flags & BT_NOGLOB)))
		comm -> argc = evalglob(comm -> argc, &(comm -> argv), 1);
	else for (i = 0; i < comm -> argc; i++) stripquote(comm -> argv[i], 1);
	trapok = 0;

	return(id);
}

static char *NEAR evalexternal(comm)
command_t *comm;
{
	char *path;
	int type;

	type = searchhash(&(comm -> hash), comm -> argv[0], NULL);
	if (type & CM_NOTFOUND) {
		execerror(NULL, comm -> argv[0], ER_COMNOFOUND);
		ret_status = RET_NOTFOUND;
		return(NULL);
	}
	if (restricted && (type & CM_FULLPATH)) {
		execerror(NULL, comm -> argv[0], ER_RESTRICTED);
		ret_status = RET_FAIL;
		return(NULL);
	}
	if (type & CM_FULLPATH) {
#if	MSDOS
		if (type & CM_ADDEXT)
			path = comm -> argv[0] = addext(comm -> argv[0], type);
		else
#endif
		path = comm -> argv[0];
	}
	else {
#ifdef	_NOUSEHASH
		path = (char *)(comm -> hash);
#else
		((comm -> hash) -> hits)++;
		path = (comm -> hash) -> path;
#endif
	}
#if	MSDOS
	if (type & CM_BATCH)
		comm -> argv = replacebat(&path, comm -> argv);
#endif
	return(path);
}

static VOID NEAR printindent(n, fp)
int n;
FILE *fp;
{
	while (n-- > 0) fputs("  ", fp);
}

static VOID NEAR printnewline(n, fp)
int n;
FILE *fp;
{
	if (n < 0) fputc(' ', fp);
	else {
		fputc('\n', fp);
		printindent(n, fp);
	}
}

static VOID NEAR printredirect(rp, fp)
redirectlist *rp;
FILE *fp;
{
	if (!rp) return;
	printredirect(rp -> next, fp);

	fputc(' ', fp);
	switch (rp -> type & MD_RDWR) {
		case MD_READ:
			if (rp -> fd != STDIN_FILENO)
				fputlong(rp -> fd, fp);
			fputc('<', fp);
			if (rp -> type & MD_HEREDOC) {
				fputc('<', fp);
				if (rp -> type & MD_APPEND) fputc('-', fp);
			}
			break;
		case MD_WRITE:
			if (rp -> fd != STDOUT_FILENO)
				fputlong(rp -> fd, fp);
			if (rp -> type & MD_WITHERR) fputc('&', fp);
			fputc('>', fp);
			if (rp -> type & MD_APPEND) fputc('>', fp);
			else if (rp -> type & MD_FORCED) fputc('|', fp);
			break;
		case MD_RDWR:
			if (rp -> fd != STDOUT_FILENO)
				fputlong(rp -> fd, fp);
			fputs("<>", fp);
			break;
		default:
			break;
	}
	if (!(rp -> filename)) fputc('-', fp);
	else {
		if (rp -> type & MD_FILEDESC) fputc('&', fp);
		else fputc(' ', fp);
		kanjifputs(rp -> filename, fp);
	}
}

static VOID NEAR printstree(trp, indent, fp)
syntaxtree *trp;
int indent;
FILE *fp;
{
	syntaxtree *tmptr;
	int i, j, id, prev, ind2;

	if (!trp) return;
	prev = getstatid(tmptr = parentshell(trp)) + 1;

	if ((trp -> flags & ST_TYPE) == OP_NOT) {
		fputc('!', fp);
		printstree(trp -> next, indent, fp);
		return;
	}

	if (trp -> flags & ST_NODE)
		printstree((syntaxtree *)(trp -> comm), indent, fp);
	else if (!(trp -> comm)) {
		if (tmptr && !(tmptr -> next) && prev > 0) {
			for (i = 0; i < STATEMENTSIZ; i++) {
				if (!(statementlist[i].type & STT_NEEDNONE))
					continue;
				for (j = 0; j < SMPREV; j++) {
					if (!(statementlist[i].prev[j]))
						continue;
					if (prev == statementlist[i].prev[j])
						break;
				}
				if (j < SMPREV) break;
			}
			if (i < STATEMENTSIZ) {
				printnewline(indent - 1, fp);
				fputs(statementlist[i].ident, fp);
			}
		}
	}
	else {
		if (isstatement(trp -> comm)) {
			id = (trp -> comm) -> id;
			i = id - 1;
			if (id == SM_CHILD) {
				fputs("( ", fp);
				printstree(statementbody(trp), indent, fp);
				fputs(" )", fp);
			}
			else if (id == SM_STATEMENT)
				printstree(statementbody(trp), indent, fp);
			else {
				fputs(statementlist[i].ident, fp);
				switch (id) {
					case SM_THEN:
					case SM_ELSE:
					case SM_DO:
					case SM_RPAREN:
					case SM_LIST:
						ind2 = 1;
						break;
					case SM_INCASE:
						if (indent >= 0) indent++;
						ind2 = 0;
						break;
					case SM_CASEEND:
						if (trp -> next) ind2 = 0;
						else ind2 = -1;
						break;
					case SM_FUNC:
#ifdef	BASHSTYLE
	/* bash type pretty print */
						ind2 = 0;
						break;
#endif
					case SM_LPAREN:
						ind2 = -1;
						break;
					default:
						ind2 = -1;
						fputc(' ', fp);
						break;
				}

				if (ind2 < 0) ind2 = indent;
				else {
					if (indent < 0) ind2 = indent;
					else ind2 += indent;
					printnewline(ind2, fp);
				}
				printstree(statementbody(trp), ind2, fp);

				switch (id) {
					case SM_IF:
					case SM_ELIF:
					case SM_WHILE:
					case SM_UNTIL:
					case SM_IN:
#ifdef	BASHSTYLE
	/* bash type pretty print */
						fputc(' ', fp);
						break;
#endif
					case SM_THEN:
					case SM_RPAREN:
						printnewline(indent, fp);
						break;
					case SM_FOR:
					case SM_CASE:
						fputc(' ', fp);
						break;
					default:
						break;
				}
			}
		}
		else if ((trp -> comm) -> argc > 0) {
			kanjifputs((trp -> comm) -> argv[0], fp);
			for (i = 1; i < (trp -> comm) -> argc; i++) {
				fputc(' ', fp);
				if (prev == SM_INCASE || prev == SM_CASEEND)
					fputs("| ", fp);
				kanjifputs((trp -> comm) -> argv[i], fp);
			}
		}

		printredirect((trp -> comm) -> redp, fp);
	}

	if ((trp -> flags & ST_TYPE) == OP_FG) {
		if (trp -> next && (trp -> next) -> comm) {
#ifdef	BASHSTYLE
	/* bash type pretty print */
			fputc(';', fp);
			printnewline(indent, fp);
#else
			if (indent < 0) fputs("; ", fp);
			else {
				fputc('\n', fp);
				printindent(indent, fp);
			}
#endif
		}
#ifndef	BASHSTYLE
	/* bash type pretty print */
		else if (indent >= 0);
#endif
		else fputc(';', fp);
	}
	else if (trp -> flags & ST_TYPE) {
		for (i = 0; i < OPELISTSIZ; i++)
			if ((trp -> flags & ST_TYPE) == opelist[i].op) break;
		if (i < OPELISTSIZ) {
			fputc(' ', fp);
			fputs(opelist[i].symbol, fp);
			if (trp -> next && (trp -> next) -> comm) {
#ifndef	BASHSTYLE
	/* bash type pretty print */
				if (opelist[i].level >= 4)
					printnewline(indent, fp);
				else
#endif
				fputc(' ', fp);
			}
		}
	}
	if (trp -> next) printstree(trp -> next, indent, fp);
	fflush(fp);
}

static VOID NEAR printfunction(f, fp)
shfunctable *f;
FILE *fp;
{
	kanjifputs(f -> ident, fp);
	fputs("()", fp);
#ifdef	BASHSTYLE
	/* bash type pretty print */
	fputc('\n', fp);
#endif
	if (getstatid(statementcheck(f -> func, SM_STATEMENT)) == SM_LIST - 1)
		printstree(f -> func, 0, fp);
	else {
		fputs("{\n", fp);
		if (f -> func) {
			printindent(1, fp);
			printstree(f -> func, 1, fp);
		}
		fputs("\n}", fp);
	}
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

static int NEAR dochild(trp)
syntaxtree *trp;
{
#if	MSDOS
	char **svar, **evar, **elist, **rlist, cwd[MAXPATHLEN];
	long esize;
	int ret, blevel, clevel;

	if (!Xgetwd(cwd)) return(RET_FAIL);
	svar = shellvar;
	evar = exportvar;
	esize = exportsize;
	elist = exportlist;
	rlist = ronlylist;
	blevel = breaklevel;
	clevel = continuelevel;
	shellvar = duplvar(envvar, 0);
	exportvar = duplvar(exportvar, 0);
	exportlist = duplvar(exportlist, 0);
	ronlylist = duplvar(ronlylist, 0);
	breaklevel = continuelevel = 0;
	childdepth++;
	ret = exec_stree(statementbody(trp), 0);
	childdepth--;
	doexec(NULL);
	freevar(shellvar);
	freevar(exportvar);
	freevar(exportlist);
	freevar(ronlylist);
	shellvar = svar;
	exportvar = evar;
	exportsize = esize;
	exportlist = elist;
	ronlylist = rlist;
	breaklevel = blevel;
	continuelevel = clevel;
	if (chdir3(cwd) < 0) return(RET_FAIL);
	return(ret);
#else
	syntaxtree *body;
	int ret, pid;

	body = statementbody(trp);
	if ((pid = makechild(1, -1)) < 0) return(-1);
	else if (!pid) {
		childdepth++;
# ifndef	NOJOB
		stackjob(mypid, 0, body);
# endif
		ret = exec_stree(body, 0);
		prepareexit(1);
		Xexit((ret >= 0) ? ret : RET_NOTEXEC);
	}
	else {
# ifndef	NOJOB
		stackjob(pid, 0, body);
# endif
		if ((trp -> flags & ST_TYPE) == OP_BG
		|| (trp -> flags & ST_TYPE) == OP_NOWN) return(ret);
		ret = waitchild(pid, trp);
		if (ret == RET_NOTEXEC) ret = -1;
		if (ret < 0) breaklevel = loopdepth;
	}
	return(ret);
#endif
}

static int NEAR doif(trp)
syntaxtree *trp;
{
	syntaxtree *cond, *body;
	int ret;

	cond = statementbody(trp);
	if (!(body = statementcheck(trp -> next, SM_THEN))) return(-1);

	if ((ret = exec_stree(cond, 1)) < 0 || returnlevel) return(ret);
	if (interrupted) return(RET_INTR);
	if (ret == RET_SUCCESS) return(exec_stree(body, 0));
	if (!(trp = (trp -> next) -> next)) return(RET_SUCCESS);

	body = statementbody(trp);
	if (interrupted) return(RET_INTR);
	if ((trp -> comm) -> id == SM_ELSE) return(exec_stree(body, 0));
	if ((trp -> comm) -> id == SM_ELIF) return(doif(trp));

	errno = EINVAL;
	return(-1);
}

static int NEAR dowhile(trp)
syntaxtree *trp;
{
	syntaxtree *cond, *body;
	int ret, tmp;

	cond = statementbody(trp);
	if (!(body = statementcheck(trp -> next, SM_DO))) return(-1);

	ret = RET_SUCCESS;
	loopdepth++;
	for (;;) {
		if ((tmp = exec_stree(cond, 1)) < 0 || returnlevel) {
			ret = tmp;
			break;
		}
		if (tmp != RET_SUCCESS) break;
		if (interrupted) {
			ret = RET_INTR;
			break;
		}
		if ((ret = exec_stree(body, 0)) < 0 || returnlevel) break;
		if (interrupted) {
			ret = RET_INTR;
			break;
		}
		if (breaklevel) {
			breaklevel--;
			break;
		}
		if (continuelevel && --continuelevel) break;
	}
	loopdepth--;
	return(ret);
}

static int NEAR dountil(trp)
syntaxtree *trp;
{
	syntaxtree *cond, *body;
	int ret, tmp;

	cond = statementbody(trp);
	if (!(body = statementcheck(trp -> next, SM_DO))) return(-1);

	ret = RET_SUCCESS;
	loopdepth++;
	for (;;) {
		if ((tmp = exec_stree(cond, 1)) < 0 || returnlevel) {
			ret = tmp;
			break;
		}
#ifndef	BASHSTYLE
	/* bash ignores "break" in the "until" condition */
		if (breaklevel) {
			breaklevel--;
			break;
		}
#endif
		if (tmp == RET_SUCCESS) break;
		if (interrupted) {
			ret = RET_INTR;
			break;
		}
		if ((ret = exec_stree(body, 0)) < 0 || returnlevel) break;
		if (interrupted) {
			ret = RET_INTR;
			break;
		}
#ifdef	BASHSTYLE
	/* bash ignores "break" in the "until" condition */
		if (breaklevel) {
			breaklevel--;
			break;
		}
#endif
		if (continuelevel && --continuelevel) break;
	}
	loopdepth--;
	return(ret);
}

static int NEAR dofor(trp)
syntaxtree *trp;
{
	syntaxtree *var, *body;
	command_t *comm;
	char *tmp, *ident, **argv, **tmpargv;
	int i, argc, ret;

	var = statementbody(trp);
	body = statementbody(trp -> next);
	comm = var -> comm;

	ident = comm -> argv[0];
	if (identcheck(ident, '\0') <= 0) {
		execerror(NULL, ident, ER_NOTIDENT);
		return(RET_FAIL);
	}
	trp = trp -> next;
	if ((trp -> comm) -> id != SM_IN) {
		tmpargv = NULL;
		argv = &(argvar[1]);
	}
	else {
		if (!(comm = body -> comm)) {
			errno = EINVAL;
			return(-1);
		}
		trp = trp -> next;

		tmpargv = (char **)malloc2((comm -> argc + 1)
			* sizeof(char *));
		for (i = 0; i < comm -> argc; i++) {
			if (!(tmp = evalvararg(comm -> argv[i], 0, 1))) {
				while (--i >= 0) free(tmpargv[i]);
				free(tmpargv);
				return(RET_FAIL);
			}
			tmpargv[i] = tmp;
		}
		tmpargv[i] = NULL;
		argc = evalifs(comm -> argc, &tmpargv, getifs());
		if (!noglob) argc = evalglob(argc, &tmpargv, 1);
		else for (i = 0; i < argc; i++) stripquote(tmpargv[i], 1);
		argv = tmpargv;
	}

	if (!(body = statementcheck(trp, SM_DO))) {
		freevar(tmpargv);
		return(-1);
	}

	ret = RET_SUCCESS;
	loopdepth++;
	for (i = 0; argv[i]; i++) {
		if (interrupted) {
			ret = RET_INTR;
			break;
		}
		if (setenv2(ident, argv[i]) < 0) break;
		if ((ret = exec_stree(body, 0)) < 0 || returnlevel) break;
		if (interrupted) {
			ret = RET_INTR;
			break;
		}
		if (breaklevel) {
			breaklevel--;
			break;
		}
		if (continuelevel && --continuelevel) break;
	}
	loopdepth--;
	freevar(tmpargv);
	return(ret);
}

static int NEAR docase(trp)
syntaxtree *trp;
{
	syntaxtree *var, *body;
	command_t *comm;
	reg_t *re;
	char *tmp, *key;
	int i, n, ret;

	var = statementbody(trp);
	comm = var -> comm;
	if (!(key = evalvararg(comm -> argv[0], 1, 1))) return(RET_FAIL);

	ret = RET_SUCCESS;
	for (trp = trp -> next; trp; trp = (trp -> next) -> next) {
		if (!(trp -> next)) break;
		if (interrupted) {
			ret = RET_INTR;
			break;
		}
		var = statementbody(trp);
		if (!(body = statementcheck(trp -> next, SM_RPAREN))) {
			ret = -1;
			break;
		}
		if (!(comm = var -> comm)) break;
		ret = -1;
		for (i = 0; i < comm -> argc; i++) {
			if (!(tmp = evalvararg(comm -> argv[i], 0, 1))) {
				ret = RET_FAIL;
				break;
			}
			re = regexp_init(tmp, -1);
			free(tmp);
			if (!re) continue;
			n = regexp_exec(re, key, 0);
			regexp_free(re);
			if (n) {
				ret = exec_stree(body, 0);
				break;
			}
			if (interrupted) {
				ret = RET_INTR;
				break;
			}
		}
		if (ret >= 0) break;
		ret = RET_SUCCESS;
	}
	free(key);
	return(ret);
}

static int NEAR dolist(trp)
syntaxtree *trp;
{
	return(exec_stree(statementbody(trp), 0));
}

/*ARGSUSED*/
static int NEAR donull(trp)
syntaxtree *trp;
{
	return(RET_SUCCESS);
}

static int NEAR dobreak(trp)
syntaxtree *trp;
{
	int n;

	if (!loopdepth) return(RET_FAIL);
	else if ((trp -> comm) -> argc <= 1) breaklevel = 1;
	else if ((n = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror(NULL, (trp -> comm) -> argv[1], ER_BADNUMBER);
		return(RET_FAIL);
	}
#ifndef	BASHSTYLE
	/* bash ignores "break 0" */
	else if (!n) breaklevel = -1;
#endif
	else if (n > loopdepth) breaklevel = loopdepth;
	else breaklevel = n;
	return(RET_SUCCESS);
}

static int NEAR docontinue(trp)
syntaxtree *trp;
{
	int n;

	if (!loopdepth) return(RET_FAIL);
	else if ((trp -> comm) -> argc <= 1) continuelevel = 1;
	else if ((n = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror(NULL, (trp -> comm) -> argv[1], ER_BADNUMBER);
		return(RET_FAIL);
	}
#ifndef	BASHSTYLE
	/* bash ignores "break 0" */
	else if (!n) continuelevel = -1;
#endif
	else if (n > loopdepth) continuelevel = loopdepth;
	else continuelevel = n;
	return(RET_SUCCESS);
}

static int NEAR doreturn(trp)
syntaxtree *trp;
{
	int ret;

	if (!functionlevel) {
		execerror(NULL, NULL, ER_CANNOTRET);
		return(RET_FAIL);
	}
	if ((trp -> comm) -> argc <= 1) ret = ret_status;
	else if ((ret = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror(NULL, (trp -> comm) -> argv[1], ER_BADNUMBER);
		ret = RET_FAIL;
#ifdef	BASHSTYLE
	/* bash ignores "return -1" */
		return(ret);
#endif
	}
	returnlevel = functionlevel;
	return(ret);
}

static int NEAR execpath(comm, errexit)
command_t *comm;
int errexit;
{
	char *path, **evar;

	path = evalexternal(comm);
	if (!errexit && !path) return(RET_NOTFOUND);
	path = strdup2(path);
	evar = exportvar;
	exportvar = NULL;
#ifdef	FD
	if (savehist > 0) savehistory(0, FD_HISTORYFILE);
#endif
#if	!MSDOS && !defined (NOJOB)
	if (loginshell && interactive_io) killjob();
#endif

	prepareexit(1);
	if (errexit && !path) {
# ifdef	DEBUG
		freevar(evar);
# endif
		Xexit2(RET_FAIL);
	}

	Xexecve(path, comm -> argv, evar, 0);
	return(RET_NOTEXEC);
}

static int NEAR doexec(trp)
syntaxtree *trp;
{
	static redirectlist *redp = NULL;
	command_t *comm;
	int i;

	if (!trp) {
		if (redp) {
			closeredirect(redp);
			freerlist(redp);
			redp = NULL;
		}
		return(RET_SUCCESS);
	}

	comm = trp -> comm;
	if (comm -> argc >= 2) {
		if (restricted) {
			execerror(NULL, comm -> argv[1], ER_RESTRICTED);
			return(RET_FAIL);
		}
		(comm -> argc)--;
		free(comm -> argv[0]);
		for (i = 0; i <= comm -> argc; i++)
			comm -> argv[i] = comm -> argv[i + 1];
#ifdef	BASHSTYLE
	/* bash ignores the unexecutable external command */
		return(execpath(comm, 0));
#else
		return(execpath(comm, 1));
#endif
	}

	if ((trp -> flags & ST_TYPE) != OP_BG
	&& (trp -> flags & ST_TYPE) != OP_NOWN && comm -> redp) {
		if (redp) {
			closeredirect(redp);
			freerlist(redp);
		}
		if (definput == ttyio && !isatty(STDIN_FILENO))
			definput = STDIN_FILENO;
		else if (definput == STDIN_FILENO && isatty(STDIN_FILENO))
			definput = ttyio;
		redp = comm -> redp;
		comm -> redp = NULL;
	}
	return(RET_SUCCESS);
}

static int NEAR dologin(trp)
syntaxtree *trp;
{
#if	MSDOS
	return(RET_SUCCESS);
#else	/* !MSDOS */
	return(execpath(trp -> comm, 0));
#endif	/* !MSDOS */
}

static int NEAR dologout(trp)
syntaxtree *trp;
{
	if ((!loginshell && interactive_io) || exit_status < 0) {
		execerror(argvar, (trp -> comm) -> argv[0], ER_NOTLOGINSH);
		return(RET_FAIL);
	}
	return(doexit(trp));
}

static int NEAR doeval(trp)
syntaxtree *trp;
{
	syntaxtree *stree;
	char *cp;
	int ret;

	if ((trp -> comm) -> argc <= 1) return(RET_SUCCESS);
	cp = catvar(&((trp -> comm) -> argv[1]), ' ');

#ifdef	BASHSTYLE
	/* bash displays arguments of "eval", in -v mode */
	if (verboseinput) {
		kanjifputs(cp, stderr);
		fputc('\n', stderr);
		fflush(stderr);
	}
#endif
	stree = newstree(NULL);

#if	!MSDOS && !defined (NOJOB)
	childpgrp = -1;
#endif
	if (!execline(cp, stree, NULL)) ret = ret_status;
	else {
		syntaxerror("", ER_UNEXPEOF);
		ret = RET_SYNTAXERR;
	}
	freestree(stree);
	free(stree);
	free(cp);
	return(ret);
}

static int NEAR doexit(trp)
syntaxtree *trp;
{
	int ret;

	if ((trp -> comm) -> argc <= 1) ret = ret_status;
	else if ((ret = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror(NULL, (trp -> comm) -> argv[1], ER_BADNUMBER);
		ret = RET_FAIL;
#ifdef	BASHSTYLE
	/* bash ignores "exit -1" */
		return(ret);
#endif
	}
	if (exit_status < 0) exit_status = ret;
	else {
#ifdef	FD
		if (savehist > 0) savehistory(0, FD_HISTORYFILE);
#endif
#if	!MSDOS && !defined (NOJOB)
		if (loginshell && interactive_io) killjob();
#endif
		prepareexit(0);
		Xexit2(ret);
	}
	return(RET_SUCCESS);
}

static int NEAR doread(trp)
syntaxtree *trp;
{
	char *cp, *next, *buf, *ifs;
	int i, c, size, duperrno;

	if ((trp -> comm) -> argc <= 1) {
		execerror(NULL, NULL, ER_MISSARG);
		return(RET_FAIL);
	}
	for (i = 1; i < (trp -> comm) -> argc; i++) {
		if (identcheck((trp -> comm) -> argv[i], '\0') <= 0) {
			execerror(NULL, (trp -> comm) -> argv[i], ER_NOTIDENT);
			return(RET_FAIL);
		}
	}

	buf = c_malloc(size);
	trapok = 1;
	readtrap = 1;
	for (i = 0;;) {
		if ((c = readchar(STDIN_FILENO)) < 0) {
			duperrno = errno;
			free(buf);
			errno = duperrno;
			return(-1);
		}
		if (c == READ_EOF || c == '\n') break;
		buf = c_realloc(buf, i, size);
		buf[i++] = c;
	}
	trapok = 0;
	if (!readtrap) {
		free(buf);
		return(RET_FAIL);
	}
	readtrap = 0;
	buf[i] = '\0';
	c = (c != READ_EOF) ? RET_SUCCESS : RET_FAIL;

	ifs = getifs();
	if ((trp -> comm) -> argc > 1) {
		cp = buf;
		for (i = 1; i < (trp -> comm) -> argc - 1; i++) {
			if (!*cp) next = cp;
			else if (!(next = strpbrk(cp, ifs)))
				next = cp + strlen(cp);
			else do {
				*(next++) = '\0';
			} while (strchr(ifs, *next));
			if (setenv2((trp -> comm) -> argv[i], cp) < 0) {
				c = RET_FAIL;
				cp = NULL;
				break;
			}
			cp = next;
		}
		if (cp && setenv2((trp -> comm) -> argv[i], cp) < 0)
			c = RET_FAIL;
	}
	free(buf);

	return(c);
}

static int NEAR doshift(trp)
syntaxtree *trp;
{
	int i, n;

	if ((trp -> comm) -> argc <= 1) n = 1;
	else if ((n = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror(NULL, (trp -> comm) -> argv[1], ER_BADNUMBER);
		return(RET_FAIL);
	}
	else if (!n) return(RET_SUCCESS);

	for (i = 0; argvar[i + 1]; i++);
	if (i < n) {
		execerror(NULL, NULL, ER_CANNOTSHIFT);
		return(RET_FAIL);
	}
	for (i = 0; i < n; i++) free(argvar[i + 1]);
	for (i = 0; argvar[i + n + 1]; i++) argvar[i + 1] = argvar[i + n + 1];
	argvar[i + 1] = NULL;
	return(RET_SUCCESS);
}

static int NEAR doset(trp)
syntaxtree *trp;
{
	shfunctable *func;
	char **var, **argv;
	int i, n, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;
	if (argc <= 1) {
		var = duplvar(shellvar, 0);
		for (i = 0; var[i]; i++);
		if (i > 1) qsort(var, i, sizeof(char *), cmppath);
		for (i = 0; var[i]; i++) {
			kanjifputs(var[i], stdout);
			fputc('\n', stdout);
		}
		freevar(var);

		func = duplfunc(shellfunc);
		for (i = 0; func[i].ident; i++);
		if (i > 1) qsort(func, i, sizeof(shfunctable), cmpfunc);
		for (i = 0; func[i].ident; i++) {
			printfunction(&(func[i]), stdout);
			fputc('\n', stdout);
		}
		freefunc(func);
		fflush(stdout);
		return(RET_SUCCESS);
	}

	if ((n = getoption(argc, argv, NULL)) < 0) return(RET_FAIL);
#ifdef	BASHSTYLE
	/* bash makes -e option effective immediately */
	errorexit = tmperrorexit;
#endif
	if (n > 2) {
		if (argc <= 2) {
			for (i = 0; i < FLAGSSIZ; i++) {
				if (!optionflags[i]) continue;
				fprintf(stdout, "%-16.16s", optionflags[i]);
				fputs((*(setvals[i])) ? "on" : "off", stdout);
				fputc('\n', stdout);
			}
		}
		else if (!strpathcmp(argv[2], "ignoreeof"))
			ignoreeof = (n <= 3) ? 1 : 0;
#if	defined (FD) && !defined (_NOEDITMODE)
		else if (!strpathcmp(argv[2], "vi")
		|| !strpathcmp(argv[2], "emacs")) {
			extern char *editmode;

			setenv2("FD_EDITMODE", argv[2]);
			editmode = getshellvar("FD_EDITMODE", -1);
		}
#endif
		else {
			for (i = 0; i < FLAGSSIZ; i++) {
				if (!optionflags[i]) continue;
				if (!strpathcmp(argv[2], optionflags[i]))
					break;
			}
			if (i >= FLAGSSIZ) {
				execerror(NULL, argv[2], ER_BADOPTIONS);
				return(RET_FAIL);
			}
			*(setvals[i]) = (n <= 3) ? 1 : 0;
		}
		n = 3;
	}
	if (n >= argc) return(RET_SUCCESS);

	for (i = 1; argvar[i]; i++) free(argvar[i]);
	argvar = (char **)realloc2(argvar, (argc - n + 2) * sizeof(char *));
	for (i = 1; n < argc; i++, n++) argvar[i] = strdup2(argv[n]);
	argvar[i] = NULL;

	return(RET_SUCCESS);
}

static int NEAR dounset(trp)
syntaxtree *trp;
{
	char **argv;
	int i, n, len, ret;

	argv = (trp -> comm) -> argv;
	ret = RET_SUCCESS;
	for (i = 1; i < (trp -> comm) -> argc; i++) {
		len = strlen(argv[i]);
		if (getshellvar(argv[i], len)) n = unset(argv[i], len);
		else n = unsetfunc(argv[i], len);
		if (n < 0) {
			ret = RET_FAIL;
			ERRBREAK;
		}
	}
	return(ret);
}

#ifndef	_NOUSEHASH
static int NEAR dohash(trp)
syntaxtree *trp;
{
	hashlist *hp;
	int i, n, ret;

	if ((trp -> comm) -> argc <= 1) {
		disphash();
		return(RET_SUCCESS);
	}

	if (!strcmp((trp -> comm) -> argv[1], "-r")) {
		searchhash(NULL, NULL, NULL);
		return(RET_SUCCESS);
	}

	ret = RET_SUCCESS;
	for (i = 1; i < (trp -> comm) -> argc; i++) {
		n = searchhash(&hp, (trp -> comm) -> argv[i], NULL);
		if (n & CM_NOTFOUND) {
			execerror(NULL, (trp -> comm) -> argv[i], ER_NOTFOUND);
			ret = RET_FAIL;
			ERRBREAK;
		}
		if (!(n & CM_HASH)) continue;
		hp -> hits = 0;
	}
	return(ret);
}
#endif	/* !_NOUSEHASH */

static int NEAR dochdir(trp)
syntaxtree *trp;
{
	char *cp, *tmp, *dir, *path, *next;
	int dlen, len, size;

	if ((trp -> comm) -> argc > 1) dir = (trp -> comm) -> argv[1];
	else if (!(dir = getshellvar("HOME", -1))) {
		execerror(NULL, (trp -> comm) -> argv[0], ER_NOHOMEDIR);
		return(RET_FAIL);
	}
	else if (!*dir) return(RET_SUCCESS);

	if (!(next = getshellvar("CDPATH", -1))) {
		if (chdir3(dir) >= 0) return(RET_SUCCESS);
	}
	else {
		len = strlen(dir);
		size = 0;
		path = NULL;
		for (cp = next; cp; cp = next) {
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
			if (_dospath(cp)) next = strchr(cp + 2, PATHDELIM);
			else
#endif
			next = strchr(cp, PATHDELIM);
			dlen = (next) ? (next++) - cp : strlen(cp);
			if (!dlen) tmp = NULL;
			else {
				tmp = _evalpath(cp, cp + dlen, 1, 1);
				dlen = strlen(tmp);
			}
			if (dlen + len + 1 + 1 > size) {
				size = dlen + len + 1 + 1;
				path = realloc2(path, size);
			}
			if (tmp) {
				strncpy2(path, tmp, dlen);
				free(tmp);
				dlen = strcatdelim(path) - path;
			}
			strncpy2(path + dlen, dir, len);
			if (chdir3(path) >= 0) {
				kanjifputs(path, stdout);
				fputc('\n', stdout);
				fflush(stdout);
				free(path);
				return(RET_SUCCESS);
			}
		}
		if (path) free(path);
	}
	execerror(NULL, dir, ER_BADDIR);
	return(RET_FAIL);
}

/*ARGSUSED*/
static int NEAR dopwd(trp)
syntaxtree *trp;
{
	char buf[MAXPATHLEN];

	if (!Xgetwd(buf)) {
		execerror(NULL, (trp -> comm) -> argv[0], ER_CANNOTSTAT);
		return(RET_FAIL);
	}
	kanjifputs(buf, stdout);
	fputc('\n', stdout);
	fflush(stdout);
	return(RET_SUCCESS);
}

/*ARGSUSED*/
static int NEAR dosource(trp)
syntaxtree *trp;
{
	hashlist *hp;
	char *fname;
	int n, fd;

	if ((trp -> comm) -> argc <= 1) return(RET_SUCCESS);
	fname = (trp -> comm) -> argv[1];
	n = searchhash(&hp, fname, NULL);
	if (restricted && (n & CM_FULLPATH)) {
		execerror(NULL, fname, ER_RESTRICTED);
		return(RET_FAIL);
	}
	if (n & CM_NOTFOUND) {
		execerror(NULL, fname, ER_NOTFOUND);
		return(RET_FAIL);
	}
	if ((fd = newdup(Xopen(fname, O_BINARY | O_RDONLY, 0666))) < 0) {
		doperror((trp -> comm) -> argv, fname);
		return(RET_FAIL);
	}
	sourcefile(fd, fname, 0);
	return(ret_status);
}

static int NEAR doexport(trp)
syntaxtree *trp;
{
	char *tmp, **argv;
	int i, n, len, ret;

	argv = (trp -> comm) -> argv;
	if ((trp -> comm) -> argc <= 1) {
		for (i = 0; exportlist[i]; i++) {
			fputs("export ", stdout);
			kanjifputs(exportlist[i], stdout);
			fputc('\n', stdout);
		}
		fflush(stdout);
		return(RET_SUCCESS);
	}

	ret = RET_SUCCESS;
	for (n = 1; n < (trp -> comm) -> argc; n++) {
		if (!(len = identcheck(argv[n], '='))) {
			execerror(NULL, argv[n], ER_NOTIDENT);
			ret = RET_FAIL;
			ERRBREAK;
		}
		else if (len < 0) len = -len;
		else if (_putshellvar(tmp = strdup2(argv[n]), len) < 0) {
			free(tmp);
			ret = RET_FAIL;
			ERRBREAK;
		}

		for (i = 0; exportlist[i]; i++)
			if (!strnpathcmp(argv[n], exportlist[i], len)
			&& !exportlist[i][len]) break;
		if (!exportlist[i]) {
			exportlist = (char **)realloc2(exportlist,
				(i + 2) * sizeof(char *));
			exportlist[i] = malloc2(len + 1);
			strncpy2(exportlist[i], argv[n], len);
			exportlist[++i] = NULL;
		}

		for (i = 0; shellvar[i]; i++)
			if (!strnpathcmp(argv[n], shellvar[i], len)
			&& shellvar[i][len] == '=') break;
		if (!shellvar[i]) continue;
		exportvar = putvar(exportvar, strdup2(shellvar[i]), len);
	}
	return(ret);
}

static int NEAR doreadonly(trp)
syntaxtree *trp;
{
	char *tmp, **argv;
	int i, n, len, ret;

	argv = (trp -> comm) -> argv;
	if ((trp -> comm) -> argc <= 1) {
		for (i = 0; ronlylist[i]; i++) {
			fputs("readonly ", stdout);
			kanjifputs(ronlylist[i], stdout);
			fputc('\n', stdout);
		}
		fflush(stdout);
		return(RET_SUCCESS);
	}

	ret = RET_SUCCESS;
	for (n = 1; n < (trp -> comm) -> argc; n++) {
		if (!(len = identcheck(argv[n], '='))) {
			execerror(NULL, argv[n], ER_NOTIDENT);
			ret = RET_FAIL;
			ERRBREAK;
		}
		else if (len < 0) len = -len;
		else if (putshellvar(tmp = strdup2(argv[n]), len) < 0) {
			free(tmp);
			ret = RET_FAIL;
			ERRBREAK;
		}

		for (i = 0; ronlylist[i]; i++)
			if (!strnpathcmp(argv[n], ronlylist[i], len)
			&& !ronlylist[i][len]) break;
		if (!ronlylist[i]) {
			ronlylist = (char **)realloc2(ronlylist,
				(i + 2) * sizeof(char *));
			ronlylist[i] = malloc2(len + 1);
			strncpy2(ronlylist[i], argv[n], len);
			ronlylist[++i] = NULL;
		}
	}
	return(ret);
}

/*ARGSUSED*/
static int NEAR dotimes(trp)
syntaxtree *trp;
{
	int utime, stime;
#ifdef	USERESOURCEH
	struct rusage ru;

	getrusage(RUSAGE_CHILDREN, &ru);
	utime = ru.ru_utime.tv_sec;
	stime = ru.ru_stime.tv_sec;
#else	/* !USERESOURCEH */
# ifdef	USETIMESH
	struct tms buf;

	times(&buf);
	utime = buf.tms_cutime / CLK_TCK;
	if (buf.tms_cutime % CLK_TCK > CLK_TCK / 2) utime++;
	stime = buf.tms_cstime / CLK_TCK;
	if (buf.tms_cstime % CLK_TCK > CLK_TCK / 2) stime++;
# else
	utime = stime = 0;
# endif
#endif	/* !USERESOURCEH */
	fputlong(utime / 60, stdout);
	fputc('m', stdout);
	fputlong(utime % 60, stdout);
	fputs("s ", stdout);
	fputlong(stime / 60, stdout);
	fputc('m', stdout);
	fputlong(stime % 60, stdout);
	fputs("s\n", stdout);
	fflush(stdout);
	return(RET_SUCCESS);
}

/*ARGSUSED*/
static int NEAR dowait(trp)
syntaxtree *trp;
{
#if	MSDOS
	return(RET_SUCCESS);
#else	/* !MSDOS */
	char *s;
	long pid;
# ifndef	NOJOB
	int i, j, sig;
# endif

	if ((trp -> comm) -> argc <= 1) {
# ifdef	NOJOB
		pid = lastpid;
# else
		for (;;) {
			for (i = 0; i < maxjobs; i++) {
				if (!(joblist[i].pids)) continue;
				j = joblist[i].npipe;
				sig = joblist[i].stats[j];
				if (!sig) break;
			}
			if (i >= maxjobs) return(RET_SUCCESS);
			waitchild(joblist[i].pids[j], NULL);
		}
# endif
	}
	else {
		s = (trp -> comm) -> argv[1];
# ifndef	NOJOB
		if (*s == '%') {
			checkjob(0);
			if ((i = getjob(s)) < 0) pid = -1;
			else {
				j = joblist[i].npipe;
				pid = joblist[i].pids[j];
			}
		}
		else
# endif	/* !NOJOB */
		if ((pid = isnumeric(s)) < 0) {
			execerror(NULL, s, ER_BADNUMBER);
			return(RET_FAIL);
		}
# ifndef	NOJOB
		else {
			checkjob(0);
			if (!joblist || (i = searchjob(pid, &j)) < 0) pid = -1;
		}

		if (pid < 0) {
			execerror(NULL, s, ER_NOSUCHJOB);
			return(RET_FAIL);
		}
		if (joblist[i].stats[j]) return(RET_SUCCESS);
# endif	/* !NOJOB */
	}

	return(waitchild(pid, NULL));
#endif	/* !MSDOS */
}

static int NEAR doumask(trp)
syntaxtree *trp;
{
	char *s;
	int i, n;

	if ((trp -> comm) -> argc <= 1) {
		n = umask(022);
		umask(n);
		n &= 0777;
		fprintf(stdout, "%04o\n", n);
		fflush(stdout);
	}
	else {
		s = (trp -> comm) -> argv[1];
		n = 0;
		for (i = 0; s[i] >= '0' && s[i] <= '7'; i++)
			n = (n << 3) + s[i] - '0';
		n &= 0777;
		umask(n);
	}

	return(RET_SUCCESS);
}

/*ARGSUSED*/
static int NEAR doulimit(trp)
syntaxtree *trp;
{
#if	!defined (USERESOURCEH) && !defined (USEULIMITH)
	execerror(NULL, NULL, ER_BADULIMIT);
	return(RET_FAIL);
#else	/* USERESOURCEH || USEULIMITH */
# ifdef	USERESOURCEH
	struct rlimit lim;
	u_long flags;
	int j, n, err, hs, res, inf;
# endif
	long val;
	char **argv;
	int i, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;
# ifdef	USERESOURCEH
	flags = 0;
	err = hs = 0;
	n = 1;
	if (argc > 1 && argv[1][0] == '-') {
		for (i = 1; !err && argv[1][i]; i++) switch (argv[1][i]) {
			case 'a':
				flags = (1 << ULIMITSIZ) - 1;
				break;
			case 'H':
				hs |= 1;
				break;
			case 'S':
				hs |= 2;
				break;
			default:
				for (j = 0; j < ULIMITSIZ; j++)
					if (argv[1][i] == ulimitlist[j].opt)
						break;
				if (j >= ULIMITSIZ) err = 1;
				else flags |= (1 << j);
				break;
		}
		n++;
	}
	res = -1;
	if (!flags) for (i = 0, flags = 1; i < ULIMITSIZ; i++, flags <<= 1) {
		if (ulimitlist[i].res == RLIMIT_FSIZE) {
			res = i;
			break;
		}
	}
	else for (i = 0; i < ULIMITSIZ; i++) {
		if (flags & (1 << i)) {
			if (res < 0) res = i;
			else {
				res = -1;
				break;
			}
		}
	}

	if (n >= argc) {
		if (!hs) hs = 2;
	}
	else {
		inf = 0;
		if (!hs) hs = (1 | 2);
		if (res < 0) err = 1;
		else if (!strcmp(argv[n], UNLIMITED)) inf = 1;
		else {
			if ((val = isnumeric(argv[n])) < 0) {
				execerror(NULL, argv[n], ER_BADULIMIT);
				return(RET_FAIL);
			}
			val *= ulimitlist[res].unit;
		}
	}
	if (err) {
		fputs("usage: ", stdout);
		kanjifputs(argv[0], stdout);
		fputs(" [ -HSa", stdout);
		for (j = 0; j < ULIMITSIZ; j++)
			fputc(ulimitlist[j].opt, stdout);
		fputs(" ] [ limit ]\n", stdout);
		return(RET_SUCCESS);
	}

	if (n >= argc) {
		for (i = 0; i < ULIMITSIZ; i++) if (flags & (1 << i)) {
			if (res < 0) {
				fputs(ulimitlist[i].mes, stdout);
				fputc(' ', stdout);
			}
			if (getrlimit(ulimitlist[i].res, &lim) < 0) {
				execerror(NULL, NULL, ER_BADULIMIT);
				return(RET_FAIL);
			}
			if (hs & 2) {
				if (lim.rlim_cur == RLIM_INFINITY)
					fputs(UNLIMITED, stdout);
				else if (lim.rlim_cur)
					fputlong(lim.rlim_cur
					/ ulimitlist[i].unit, stdout);
			}
			if (hs & 1) {
				if (hs & 2) fputc(':', stdout);
				if (lim.rlim_max == RLIM_INFINITY)
					fputs(UNLIMITED, stdout);
				else if (lim.rlim_max)
					fputlong(lim.rlim_max
					/ ulimitlist[i].unit, stdout);
			}
			fputc('\n', stdout);
		}
	}
	else {
		if (getrlimit(ulimitlist[res].res, &lim) < 0) {
			execerror(NULL, NULL, ER_BADULIMIT);
			return(RET_FAIL);
		}
		if (hs & 1) lim.rlim_max = (inf) ? RLIM_INFINITY : val;
		if (hs & 2) lim.rlim_cur = (inf) ? RLIM_INFINITY : val;
		if (setrlimit(ulimitlist[res].res, &lim) < 0) {
			execerror(NULL, NULL, ER_BADULIMIT);
			return(RET_FAIL);
		}
	}
# else	/* !USERESOURCEH */
	if (argc <= 1) {
		if ((val = ulimit(UL_GETFSIZE, 0L)) < 0) {
			execerror(NULL, NULL, ER_BADULIMIT);
			return(RET_FAIL);
		}
		if (val == RLIM_INFINITY) fputs(UNLIMITED, stdout);
		else fputlong(val * 512L, stdout);
		fputc('\n', stdout);
	}
	else {
		if (!strcmp(argv[1], UNLIMITED)) val = RLIM_INFINITY;
		else {
			if ((val = isnumeric(argv[1])) < 0) {
				execerror(NULL, argv[1], ER_BADULIMIT);
				return(RET_FAIL);
			}
			val /= 512L;
		}
		if ((ulimit(UL_SETFSIZE, val)) < 0) {
			execerror(NULL, NULL, ER_BADULIMIT);
			return(RET_FAIL);
		}
	}
# endif	/* !USERESOURCEH */
	return(RET_SUCCESS);
#endif	/* USERESOURCEH || USEULIMITH */
}

static int NEAR dotrap(trp)
syntaxtree *trp;
{
	char *comm, **argv;
	int i, n, sig;

	argv = (trp -> comm) -> argv;
	if ((trp -> comm) -> argc <= 1) {
		for (i = 0; i < NSIG; i++) {
			if ((trapmode[i] & TR_STAT) != TR_TRAP) continue;
			fputlong(i, stdout);
			fputs(": ", stdout);
			kanjifputs(trapcomm[i], stdout);
			fputc('\n', stdout);
		}
		fflush(stdout);
		return(RET_SUCCESS);
	}

	n = 1;
	comm = (isnumeric(argv[n]) < 0) ? argv[n++] : NULL;

	for (; n < (trp -> comm) -> argc; n++) {
		if ((sig = isnumeric(argv[n])) < 0) {
			execerror(NULL, argv[n], ER_BADNUMBER);
			return(RET_FAIL);
		}
		if (sig >= NSIG) {
			execerror(NULL, argv[n], ER_BADTRAP);
			return(RET_FAIL);
		}

		if (!sig) {
			trapmode[0] = (comm) ? TR_TRAP : 0;
			if (trapcomm[0]) free(trapcomm[0]);
			trapcomm[0] = strdup2(comm);
			continue;
		}

		for (i = 0; i < SIGNALSIZ; i++)
			if (sig == signallist[i].sig) break;
		if (i >= SIGNALSIZ || (signallist[i].flags & TR_NOTRAP)) {
			execerror(NULL, argv[n], ER_BADTRAP);
			return(RET_FAIL);
		}

		signal(sig, SIG_IGN);
		if (trapcomm[sig]) free(trapcomm[sig]);
		if (!comm) {
			trapmode[sig] = signallist[i].flags;
			if (trapmode[sig] & TR_BLOCK)
				signal(sig, (sigcst_t)(signallist[i].func));
			else signal(sig, oldsigfunc[sig]);
			trapcomm[sig] = NULL;
		}
		else {
			trapmode[sig] = TR_TRAP;
			trapcomm[sig] = strdup2(comm);
			if ((signallist[i].flags & TR_STAT) == TR_STOP)
				signal(sig, SIG_DFL);
			else signal(sig, (sigcst_t)(signallist[i].func));
		}
	}
	return(RET_SUCCESS);
}

/*ARGSUSED*/
static int NEAR dojobs(trp)
syntaxtree *trp;
{
#if	!MSDOS && !defined (NOJOB)
	int i, j;

	if (mypid != orgpgrp) return(RET_SUCCESS);
	checkjob(0);
	for (i = 0; i < maxjobs; i++) {
		if (!(joblist[i].pids)) continue;
		j = joblist[i].npipe;

		if (joblist[i].stats[j] >= 0 && joblist[i].stats[j] < 128)
			dispjob(i, stdout);
		else {
			if ((joblist[i].trp -> flags & ST_TYPE) == OP_BG)
				joblist[i].trp -> flags &= ~ST_TYPE;
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
#endif	/* !MSDOS && !NOJOB */
	return(RET_SUCCESS);
}

/*ARGSUSED*/
static int NEAR dofg(trp)
syntaxtree *trp;
{
#if	MSDOS || defined (NOJOB)
	return(RET_FAIL);
#else	/* !MSDOS && !NOJOB */
	termioctl_t tty;
	char *s;
	int i, j, n, ret;

	s = ((trp -> comm) -> argc > 1) ? (trp -> comm) -> argv[1] : NULL;
	checkjob(0);
	if ((i = getjob(s)) < 0) {
		execerror(argvar, (trp -> comm) -> argv[1], ER_NOSUCHJOB);
		return(RET_FAIL);
	}
	n = joblist[i].npipe;
	if (tioctl(ttyio, REQGETP, &tty) < 0) {
		doperror((trp -> comm) -> argv, "fatal error");
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
	if ((joblist[i].trp -> flags & ST_TYPE) == OP_BG)
		joblist[i].trp -> flags &= ~ST_TYPE;
	ret = waitchild(joblist[i].pids[n], NULL);
	gettermio(orgpgrp);
	if (tioctl(ttyio, REQSETP, &tty) < 0) {
		doperror((trp -> comm) -> argv, "fatal error");
		prepareexit(-1);
		Xexit2(RET_FATALERR);
	}
	return(ret);
#endif	/* !MSDOS && !NOJOB */
}

/*ARGSUSED*/
static int NEAR dobg(trp)
syntaxtree *trp;
{
#if	MSDOS || defined (NOJOB)
	return(RET_FAIL);
#else	/* !MSDOS && !NOJOB */
	char *s;
	int i, j, n;

	s = ((trp -> comm) -> argc > 1) ? (trp -> comm) -> argv[1] : NULL;
	checkjob(0);
	if ((i = getjob(s)) < 0) {
		execerror(argvar, (trp -> comm) -> argv[1], ER_NOSUCHJOB);
		return(RET_FAIL);
	}
	n = joblist[i].npipe;
	if ((j = joblist[i].stats[n]) > 0 && j < 128) {
		killpg(joblist[i].pids[0], SIGCONT);
		for (j = 0; j <= n; j++) joblist[i].stats[j] = 0;
	}
	if ((joblist[i].trp -> flags & ST_TYPE) != OP_BG) {
		joblist[i].trp -> flags &= ~ST_TYPE;
		joblist[i].trp -> flags |= OP_BG;
	}
	if (interactive && !nottyout) dispjob(i, stderr);
	prevjob = lastjob;
	lastjob = i;
	return(RET_SUCCESS);
#endif	/* !MSDOS && !NOJOB */
}

/*ARGSUSED*/
static int NEAR dodisown(trp)
syntaxtree *trp;
{
#if	!MSDOS && !defined (NOJOB)
	char *s;
	int i;

	s = ((trp -> comm) -> argc > 1) ? (trp -> comm) -> argv[1] : NULL;
	checkjob(0);
	if ((i = getjob(s)) < 0) {
		execerror(argvar, (trp -> comm) -> argv[1], ER_NOSUCHJOB);
		return(RET_FAIL);
	}
	free(joblist[i].pids);
	free(joblist[i].stats);
	if (joblist[i].trp) {
		freestree(joblist[i].trp);
		free(joblist[i].trp);
	}
	joblist[i].pids = NULL;
#endif	/* !MSDOS && !NOJOB */
	return(RET_SUCCESS);
}

static int NEAR typeone(s, fp)
char *s;
FILE *fp;
{
	hashlist *hp;
	int type, id;

	kanjifputs(s, fp);
	type = checktype(s, &id, 1, 1);

	if (type == CT_BUILTIN) fputs(" is a shell builtin", fp);
#ifdef	FD
	else if (type == CT_FDORIGINAL) fputs(" is a FDclone builtin", fp);
	else if (type == CT_FDINTERNAL)
		fputs(" is a FDclone internal", fp);
#endif
#if	MSDOS
	else if (type == CT_LOGDRIVE)
		fputs(" is a change drive command", fp);
#endif
	else if (type == CT_FUNCTION) {
		fputs(" is a function\n", fp);
		printfunction(&(shellfunc[id]), fp);
	}
#ifndef	NOALIAS
	else if (type == CT_ALIAS) {
		fputs(" is a aliased to `", fp);
		kanjifputs(shellalias[id].comm, fp);
		fputc('\'', fp);
	}
#endif
	else {
		type = searchhash(&hp, s, NULL);
#ifdef	_NOUSEHASH
		if (!(type & CM_FULLPATH)) s = (char *)hp;
#else
		if (type & CM_HASH) s = hp -> path;
#endif
		if (restricted && (type & CM_FULLPATH))
			fputs(": restricted", fp);
		else if ((type & CM_NOTFOUND) || Xaccess(s, X_OK) < 0) {
			fputs(" not found\n", fp);
			return(-1);
		}
		else {
			fputs(" is ", fp);
			kanjifputs(s, fp);
		}
	}
	fputc('\n', fp);
	return(0);
}

static int NEAR dotype(trp)
syntaxtree *trp;
{
	int i, ret;

	if ((trp -> comm) -> argc <= 1) return(RET_SUCCESS);

#ifdef	BASHSTYLE
	ret = RET_FAIL;
#else
	ret = RET_SUCCESS;
#endif
	for (i = 1; i < (trp -> comm) -> argc; i++) {
		if (typeone((trp -> comm) -> argv[i], stdout) >= 0) {
#ifdef	BASHSTYLE
			ret = RET_SUCCESS;
#endif
		}
	}
	fflush(stdout);
	return(ret);
}

#ifdef	DOSCOMMAND
static int NEAR dodir(trp)
syntaxtree *trp;
{
	return(doscomdir((trp -> comm) -> argc, (trp -> comm) -> argv));
}

static int NEAR domkdir(trp)
syntaxtree *trp;
{
	return(doscommkdir((trp -> comm) -> argc, (trp -> comm) -> argv));
}

static int NEAR dormdir(trp)
syntaxtree *trp;
{
	return(doscomrmdir((trp -> comm) -> argc, (trp -> comm) -> argv));
}

static int NEAR doerase(trp)
syntaxtree *trp;
{
	return(doscomerase((trp -> comm) -> argc, (trp -> comm) -> argv));
}

static int NEAR dorename(trp)
syntaxtree *trp;
{
	return(doscomrename((trp -> comm) -> argc, (trp -> comm) -> argv));
}

static int NEAR docopy(trp)
syntaxtree *trp;
{
	return(doscomcopy((trp -> comm) -> argc, (trp -> comm) -> argv));
}

static int NEAR docls(trp)
syntaxtree *trp;
{
	return(doscomcls((trp -> comm) -> argc, (trp -> comm) -> argv));
}
#endif	/* DOSCOMMAND */

#ifndef	NOALIAS
static int NEAR doalias(trp)
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
					execerror(NULL, argv[n], ER_NOTFOUND);
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
			shellalias[i].ident = malloc2(len + 1);
			strncpy2(shellalias[i].ident, argv[n], len);
			shellalias[i].comm = strdup2(&(argv[n][++len]));
			shellalias[++i].ident = NULL;
		}
	}
	return(ret);
}

static int NEAR dounalias(trp)
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
				execerror(NULL, argv[n], ER_NOTALIAS);
			}
			ret = RET_FAIL;
			ERRBREAK;
		}
	}
	return(ret);
}
#endif	/* NOALIAS */

static int NEAR doecho(trp)
syntaxtree *trp;
{
	char **argv;
	int i, n;

	argv = (trp -> comm) -> argv;
	n = 1;
	if ((trp -> comm) -> argc > 1 && argv[n][0] == '-'
	&& argv[n][1] == 'n' && argv[n][2] == '\0') n++;

	for (i = n; i < (trp -> comm) -> argc; i++) {
		if (i > n) fputc(' ', stdout);
		if (argv[i]) kanjifputs(argv[i], stdout);
	}
	if (n == 1) fputc('\n', stdout);
	fflush(stdout);
	return(RET_SUCCESS);
}

static int NEAR dokill(trp)
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
				for (i = 0; i < SIGNALSIZ; i++)
					if (sig == signallist[i].sig) break;
				if (i >= SIGNALSIZ) continue;
				fputs(signallist[i].ident, stdout);
				fputc((++n % 16) ? ' ' : '\n', stdout);
			}
			if (n % 16) fputc('\n', stdout);
			fflush(stdout);
			return(RET_SUCCESS);
		}
		if ((sig = isnumeric(&(s[1]))) < 0) {
			for (i = 0; i < SIGNALSIZ; i++)
				if (!strcmp(&(s[1]), signallist[i].ident))
					break;
			if (i >= SIGNALSIZ) {
				execerror((trp -> comm) -> argv, &(s[1]),
					ER_UNKNOWNSIG);
				return(RET_FAIL);
			}
			sig = signallist[i].sig;
		}
		if (sig < 0 || sig >= NSIG) {
			execerror((trp -> comm) -> argv, s, ER_NUMOUTRANGE);
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
				execerror((trp -> comm) -> argv, s,
					ER_NOSUCHJOB);
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
			doperror((trp -> comm) -> argv, s);
			return(RET_FAIL);
		}
	}
	return(RET_SUCCESS);
}

static int NEAR dotestsub1(c, s, ptrp)
int c;
char *s;
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
#ifdef	BASHSTYLE
	/* Maybe bash's bug. */
				if ((ret = isnumeric(s)) < 0) ret = 1;
#else
				if ((ret = isnumeric(s)) < 0) ret = 0;
#endif
				ret = (isatty(ret)) ? RET_SUCCESS : RET_FAIL;
			}
			else {
				(*ptrp)++;
#ifdef	BASHSTYLE
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
#ifdef	BASHSTYLE
	/* bash's "test" has -e option */
		case 'e':
			if (s) ret = (*s && Xstat(s, &st) >= 0)
				? RET_SUCCESS : RET_FAIL;
			break;
#endif
		default:
			ret = -2;
			break;
	}
	if (ret >= 0) *ptrp += 2;
	return(ret);
}

static int NEAR dotestsub2(argc, argv, ptrp)
int argc;
char *argv[];
int *ptrp;
{
	char *s, *a1, *a2;
	int ret, v1, v2;

	if (*ptrp >= argc) return(-1);
	if (argv[*ptrp][0] == '(' && !argv[*ptrp][1]) {
		(*ptrp)++;
		if ((ret = dotestsub3(argc, argv, ptrp, 0)) < 0)
			return(ret);
		if (*ptrp >= argc
		|| argv[*ptrp][0] != ')' || argv[*ptrp][1]) return(-1);
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
				*ptrp = argc;
				return(-3);
#else
				v1 = 0;
#endif
			}
			if ((v2 = isnumeric(a2)) < 0) {
#ifdef	BASHSTYLE
	/* bash's "test" does not allow arithmetic comparison with strings */
				*ptrp = argc;
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
				*ptrp = argc;
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
		ret = dotestsub1(argv[*ptrp][1], a1, ptrp);
		if (ret >= -1) return(ret);
	}
	ret = (argv[*ptrp][0]) ? RET_SUCCESS : RET_FAIL;
	(*ptrp)++;
	return(ret);
}

static int NEAR dotestsub3(argc, argv, ptrp, lvl)
int argc;
char *argv[];
int *ptrp, lvl;
{
	int ret1, ret2;

	if (lvl > 2) {
		if (*ptrp >= argc) return(-1);
		if (argv[*ptrp][0] == '!' && !argv[*ptrp][1]) {
			(*ptrp)++;
			if ((ret1 = dotestsub2(argc, argv, ptrp)) < 0)
				return(ret1);
			return(1 - ret1);
		}
		return(dotestsub2(argc, argv, ptrp));
	}
	if ((ret1 = dotestsub3(argc, argv, ptrp, lvl + 1)) < 0) return(ret1);
	if (*ptrp >= argc || argv[*ptrp][0] != '-'
	|| argv[*ptrp][1] != ((lvl) ? 'a' : 'o') || argv[*ptrp][2])
		return(ret1);

	(*ptrp)++;
	if ((ret2 = dotestsub3(argc, argv, ptrp, lvl)) < 0) return(ret2);
	ret1 = ((lvl)
		? (ret1 != RET_FAIL && ret2 != RET_FAIL)
		: (ret1 != RET_FAIL || ret2 != RET_FAIL))
			? RET_SUCCESS : RET_FAIL;
	return(ret1);
}

static int NEAR dotest(trp)
syntaxtree *trp;
{
	char **argv;
	int ret, ptr, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;

	if (argc <= 0) return(RET_FAIL);
	if (argv[0][0] == '[' && !argv[0][1]
	&& (--argc <= 0 || argv[argc][0] != ']' || argv[argc][1])) {
		fputs("] missing\n", stderr);
		fflush(stderr);
		return(RET_NOTICE);
	}
	if (argc <= 1) return(RET_FAIL);
	ptr = 1;
	ret = dotestsub3(argc, argv, &ptr, 0);
	if (ptr < argc) ret = -1;
	if (ret < 0) {
		switch (ret) {
			case -1:
				fputs("argument expected\n", stderr);
				break;
			case -2:
				fputs("unknown operator ", stderr);
				fputs(argv[ptr], stderr);
				fputc('\n', stderr);
				break;
			default:
				fputs("syntax error\n", stderr);
				break;
		}
		fflush(stderr);
		return(RET_NOTICE);
	}
	return(ret);
}

#ifndef	NOPOSIXUTIL
/*ARGSUSED*/
static int NEAR dofalse(trp)
syntaxtree *trp;
{
	return(RET_FAIL);
}

static int NEAR docommand(trp)
syntaxtree *trp;
{
	hashlist *hp;
	char *cp, **argv, *path;
	int i, n, flag, ret, id, type, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;

	n = 1;
	flag = '\0';
	if (argc > 1 && argv[n][0] == '-') {
		switch (argv[n][1]) {
			case 'p':
			case 'v':
			case 'V':
				flag = argv[n][1];
				break;
			default:
				break;
		}
		if (flag && !argv[n][2]) n++;
		else flag = '\0';
	}

	if (interrupted) return(RET_INTR);
	if (argc <= n) return(ret_status);

#ifdef	BASHSTYLE
	ret = RET_FAIL;
#else
	ret = RET_SUCCESS;
#endif
	if (flag == 'V') {
		for (i = n; i < argc; i++) {
			if (typeone(argv[i], stdout) >= 0) {
#ifdef	BASHSTYLE
				ret = RET_SUCCESS;
#endif
			}
		}
		fflush(stdout);
		return(ret);
	}
	else if (flag == 'v') {
		for (i = n; i < argc; i++) {
			type = checktype(argv[i], &id, 0, 0);

			cp = argv[i];
			if (type == CT_COMMAND) {
				type = searchhash(&hp, cp, NULL);
#ifdef	_NOUSEHASH
				if (!(type & CM_FULLPATH)) cp = (char *)hp;
#else
				if (type & CM_HASH) cp = hp -> path;
#endif
				if ((type & CM_NOTFOUND)
				|| Xaccess(cp, X_OK) < 0) {
					execerror(argvar, cp, ER_COMNOFOUND);
					cp = NULL;
				}
			}

			if (cp) {
				fputs(cp, stdout);
				fputc('\n', stdout);
#ifdef	BASHSTYLE
				ret = RET_SUCCESS;
#endif
			}
		}
		fflush(stdout);
		return(ret);
	}

	type = checktype(argv[n], &id, 0, 0);
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
	if (flag != 'p') ret = exec_simplecom(trp, type, id, 0);
	else {
		path = strdup2(getshellvar("PATH", -1));
		setenv2("PATH", DEFPATH);
		ret = exec_simplecom(trp, type, id, 0);
		setenv2("PATH", path);
		free(path);
	}
	(trp -> comm) -> argc = argc;
	(trp -> comm) -> argv = argv;
	return(ret);
}

static int NEAR dogetopts(trp)
syntaxtree *trp;
{
	static int ptr = 0;
	char *cp, *optstr, *name, **argv, buf[20 + 1];
	int n, ret, quiet, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;

	if (argc <= 2) {
		execerror(argvar, NULL, ER_MISSARG);
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
		for (argc = 1; argv[argc]; argc++);
	}

	if (optind <= 0) {
		ptr = 0;
		optind = 1;
	}

	ret = RET_SUCCESS;
	buf[0] = buf[1] = '\0';
	if (optind >= argc || !argv[optind]) ret = RET_FAIL;
	else if (ptr >= strlen(argv[optind])) {
		if (++optind < argc && argv[optind]) ptr = 0;
		else ret = RET_FAIL;
	}

	if (ret == RET_SUCCESS && !ptr) {
		if (argv[optind][0] != '-') ret = RET_FAIL;
		else if (argv[optind][1] == '-' && !(argv[optind][2])) {
			optind++;
			ret = RET_FAIL;
		}
		else if (!(argv[optind][ptr + 1])) ret = RET_FAIL;
		else ptr++;
	}

	if (ret == RET_SUCCESS) {
		n = argv[optind][ptr++];
		if (!islower(n & 0xff) || !(cp = strchr(optstr, n))) {
			buf[0] = n;
			n = '?';
			if (!quiet) {
				execerror(argvar, buf, ER_BADOPTIONS);
				buf[0] = '\0';
			}
		}
		else if (*(cp + 1) == ':') {
			if (argv[optind][ptr])
				strcpy(buf, &(argv[optind++][ptr]));
			else if (++optind < argc && argv[optind])
				strcpy(buf, argv[optind++]);
			else {
				buf[0] = n;
				if (quiet) n = ':';
				else {
					n = '?';
					execerror(argvar, buf, ER_MISSARG);
					buf[0] = '\0';
				}
			}
			ptr = 0;
		}
	}

	if (buf[0]) setenv2("OPTARG", buf);
	else if (ret == RET_SUCCESS) unset("OPTARG", sizeof("OPTARG") - 1);

	if (ret != RET_SUCCESS) n = '?';
	buf[0] = n;
	buf[1] = '\0';
	setenv2(name, buf);

	if (setenv2("OPTIND", ascnumeric(buf, n = optind, 0, 20)));
	optind = n;

	return(ret);
}

static int NEAR donewgrp(trp)
syntaxtree *trp;
{
#if	MSDOS
	return(RET_SUCCESS);
#else	/* !MSDOS */
	return(execpath(trp -> comm, 0));
#endif	/* !MSDOS */
}

# if	0
/* exists in FD original builtin */
static int NEAR dofc(trp)
syntaxtree *trp;
{
	return(RET_SUCCESS);
}
# endif
#endif	/* NOPOSIXUTIL */

#ifdef	FD
static int NEAR dofd(trp)
syntaxtree *trp;
{
	int n;

	if (!shellmode || exit_status < 0) {
		execerror((trp -> comm) -> argv, NULL, ER_RECURSIVEFD);
		return(RET_FAIL);
	}
	else {
		n = sigvecset(1);
		if (!n) putterms(t_init);
		ttyiomode();
		shellmode = 0;
		main_fd((trp -> comm) -> argv[1]);
		shellmode = 1;
		if (!n) putterms(t_end);
		stdiomode();
		sigvecset(n);
	}
	return(RET_SUCCESS);
}
#endif

static int NEAR dofunction(trp, no)
syntaxtree *trp;
int no;
{
	char **avar;
	int ret;

	avar = argvar;
	argvar = duplvar((trp -> comm) -> argv, 0);
	functionlevel++;
	ret = exec_stree(shellfunc[no].func, 0);
	if (returnlevel >= functionlevel) returnlevel = 0;
	functionlevel--;
	freevar(argvar);
	argvar = avar;
	return(ret);
}

char **getsimpleargv(trp)
syntaxtree *trp;
{
	char **argv;

	if ((trp -> flags & ST_NODE) || (trp -> flags & ST_CONT)) return(NULL);
	if (!(trp -> comm) || isstatement(trp -> comm)) return(NULL);
	argv = (trp -> comm) -> argv;
	while (trp -> next) {
		trp = trp -> next;
		if ((trp -> flags & ST_NODE) || trp -> comm) return(NULL);
	}
	return(argv);
}

#ifdef	SHOWSTREE
static VOID NEAR show_stree(trp, lvl)
syntaxtree *trp;
int lvl;
{
	redirectlist *rp;
	int i, id;

	if (trp -> flags & ST_NODE) {
		printindent(lvl, stdout);
		fputs("node:\n", stdout);
		show_stree((syntaxtree *)(trp -> comm), lvl + 1);
	}
	else if (!(trp -> comm)) {
		printindent(lvl, stdout);
		fputs("body: NULL\n", stdout);
	}
	else {
		if (isstatement(trp -> comm)) {
			id = (trp -> comm) -> id;
			i = id - 1;
			printindent(lvl, stdout);
			if (id == SM_CHILD) fputs("sub shell", stdout);
			else if ((id == SM_STATEMENT))
				fputs("statement", stdout);
			else fputs(statementlist[i].ident, stdout);
			fputs(":\n", stdout);
			show_stree(statementbody(trp), lvl + 1);
		}
		else for (i = 0; i <= (trp -> comm) -> argc; i++) {
			printindent(lvl, stdout);
			fputs("argv[", stdout);
			fputlong(i, stdout);
			fputs("]: ", stdout);
			if (!((trp -> comm) -> argv[i])) fputs("NULL", stdout);
			else if (!i && isbuiltin(trp -> comm))
				kanjifputs((trp -> comm) -> argv[i], stdout);
			else {
				fputc('"', stdout);
				kanjifputs((trp -> comm) -> argv[i], stdout);
				fputc('"', stdout);
			}
			fputc('\n', stdout);
		}
		for (rp = (trp -> comm) -> redp; rp; rp = rp -> next) {
			printindent(lvl, stdout);
			fputs("redirect ", stdout);
			fputlong(rp -> fd, stdout);
			if (!(rp -> filename)) fputs(">-: ", stdout);
			else {
				fputs("> \"", stdout);
				kanjifputs(rp -> filename, stdout);
				fputs("\": ", stdout);
			}
			fprintf(stdout, "%06o", rp -> type);
			fputc('\n', stdout);
		}
	}
	if (trp -> flags & ST_TYPE) {
		for (i = 0; i < OPELISTSIZ; i++)
			if ((trp -> flags & ST_TYPE) == opelist[i].op) break;
		if (i < OPELISTSIZ) {
			printindent(lvl, stdout);
			fputs(opelist[i].symbol, stdout);
			fputc('\n', stdout);
		}
	}
	if (trp -> next) show_stree(trp -> next, lvl);
	fflush(stdout);
}
#endif	/* SHOWSTREE */

static int NEAR setfunc(ident, func)
char *ident;
syntaxtree *func;
{
	syntaxtree *tmptr;
	int i, len;

	len = strlen(ident);
#ifndef	BASHSTYLE
	/* bash distinguishes the same named function and variable */
	if (unset(ident, len) < 0) return(RET_FAIL);
#endif

	func = func -> next;
	if (!(tmptr = statementbody(func))) return(RET_FAIL);
	tmptr = duplstree(tmptr, NULL);

	for (i = 0; shellfunc[i].ident; i++)
		if (!strpathcmp(ident, shellfunc[i].ident)) break;
	if (shellfunc[i].ident) {
		freestree(shellfunc[i].func);
		free(shellfunc[i].func);
	}
	else {
		shellfunc = (shfunctable *)realloc2(shellfunc,
			(i + 2) * sizeof(shfunctable));
		shellfunc[i].ident = strdup2(ident);
		shellfunc[i + 1].ident = NULL;
	}
	shellfunc[i].func = tmptr;
#ifndef	_NOUSEHASH
	if (hashahead) check_stree(tmptr);
#endif
	return(RET_SUCCESS);
}

static int NEAR unsetfunc(ident, len)
char *ident;
int len;
{
	int i;

	if (checkronly(ident, len) < 0) return(-1);
	for (i = 0; shellfunc[i].ident; i++)
		if (!strnpathcmp(ident, shellfunc[i].ident, len)
		&& !shellfunc[i].ident[len]) break;
	if (!shellfunc[i].ident) return(0);
	free(shellfunc[i].ident);
	freestree(shellfunc[i].func);
	free(shellfunc[i].func);
	for (; shellfunc[i + 1].ident; i++) {
		shellfunc[i].ident = shellfunc[i + 1].ident;
		shellfunc[i].func = shellfunc[i + 1].func;
	}
	shellfunc[i].ident = NULL;
	return(0);
}

static int NEAR exec_statement(trp)
syntaxtree *trp;
{
	int id;

	if ((id = getstatid(trp = statementbody(trp))) < 0) {
		errno = EINVAL;
		return(-1);
	}
	return((*statementlist[id].func)(trp));
}

/*ARGSUSED*/
static int NEAR exec_simplecom(trp, type, id, bg)
syntaxtree *trp;
int type, id, bg;
{
#if	MSDOS
	int ret;
#else
	long pid;
#endif
	command_t *comm;
	char *path;

	comm = trp -> comm;

	if (type == CT_NONE) return(ret_status);
	if (type == CT_BUILTIN) {
		if (!shbuiltinlist[id].func) return(RET_FAIL);
		if (!restricted || !(shbuiltinlist[id].flags & BT_RESTRICT))
			return((*shbuiltinlist[id].func)(trp));
		execerror(NULL, comm -> argv[0], ER_RESTRICTED);
		return(RET_FAIL);
	}
#ifdef	FD
	if (type == CT_FDORIGINAL)
		return(execbuiltin(id, comm -> argc, comm -> argv));
	if (type == CT_FDINTERNAL) {
		if (shellmode) return(RET_FAIL);
		return(execinternal(id, comm -> argc, comm -> argv));
	}
#endif
#if	MSDOS
	if (type == CT_LOGDRIVE) {
		if (setcurdrv(id, 1) >= 0) return(RET_SUCCESS);
		execerror(NULL, comm -> argv[0], ER_INVALDRIVE);
		return(RET_FAIL);
	}
#endif
	if (type == CT_FUNCTION) {
		if (!shellfunc[id].func) return(RET_FAIL);
		return(dofunction(trp, id));
	}

	if (!(path = evalexternal(trp -> comm))) return(ret_status);
#if	MSDOS
	if ((ret = spawnve(P_WAIT, path, comm -> argv, exportvar)) < 0
	&& errno == ENOENT) {
		execerror(NULL, comm -> argv[0], ER_COMNOFOUND);
		return(RET_NOTFOUND);
	}
	return(ret);
#else
# ifndef	BASHSTYLE
	/* bash does fork() all processes even in a sub shell */
	if (childdepth && !(trp -> next)) bg = 1;
# endif
	if (bg || (trp -> flags & ST_TYPE) == OP_BG
	|| (trp -> flags & ST_TYPE) == OP_NOWN)
		Xexecve(path, comm -> argv, exportvar, 1);
	if ((pid = makechild(1, -1)) < 0) return(-1);
	if (!pid) Xexecve(path, comm -> argv, exportvar, 1);
	return(waitchild(pid, trp));
#endif
}

/*ARGSUSED*/
static int NEAR exec_command(trp, bg)
syntaxtree *trp;
int bg;
{
#ifndef	_NOUSEHASH
	hashlist **htable;
	char *pathvar;
#endif
	command_t *comm;
	char **argv, **subst, **svar, **evar;
	long esize;
	int i, ret, id, type, argc, nsubst, keepvar, *len;

	execerrno = 0;
	comm = trp -> comm;
	argc = comm -> argc;
	argv = comm -> argv;
	if (interrupted) return(RET_INTR);
	if (argc <= 0) return(ret_status);

	type = 0;
	comm -> argv = duplvar(comm -> argv, 2);
	comm -> argc = getsubst(comm -> argc, comm -> argv, &subst, &len);

	if ((id = evalargv(comm, &type, 1)) < 0
	|| (nsubst = substvar(subst)) < 0) {
		freevar(subst);
		free(len);
		comm -> argc = argc;
		freevar(comm -> argv);
		comm -> argv = argv;
		return(RET_FAIL);
	}

#ifndef	BASHSTYLE
	/* bash treats the substitution before a function as effective */
	if (type == CT_FUNCTION) {
		for (i = 0; subst[i]; i++) free(subst[i]);
		subst[0] = NULL;
		nsubst = 0;
	}
#endif

	if (verboseexec && comm -> argc) {
		fputs("+ ", stderr);
		kanjifputs(comm -> argv[0], stderr);
		for (i = 1; i < comm -> argc; i++) {
			fputc(' ', stderr);
			kanjifputs(comm -> argv[i], stderr);
		}
		fputc('\n', stderr);
		fflush(stderr);
	}

#ifdef	BASHSTYLE
	/* bash treats the substitution before builtin as ineffective */
	keepvar = (type != CT_NONE);
#else
	keepvar = (type == CT_COMMAND);
#endif
#ifdef	STRICTPOSIX
	if (type == CT_BUILTIN)
		keepvar = (!(shbuiltinlist[id].flags & BT_POSIXSPECIAL));
#endif
	if (keepvar) {
		shellvar = duplvar(svar = shellvar, 0);
		exportvar = duplvar(evar = exportvar, 0);
		esize = exportsize;
#ifndef	_NOUSEHASH
		htable = duplhash(hashtable);
		pathvar = getshellvar("PATH", -1);
#endif
	}

	while (nsubst--) {
		stripquote(subst[nsubst], 1);
		if (putshellvar(subst[nsubst], len[nsubst]) < 0) {
			while (nsubst >= 0) free(subst[nsubst--]);
			free(subst);
			free(len);
			comm -> argc = argc;
			freevar(comm -> argv);
			comm -> argv = argv;
			return(RET_FAIL);
		}
		if (type == CT_COMMAND)
			exportvar = putvar(exportvar,
				strdup2(subst[nsubst]), len[nsubst]);
		if (verboseexec) {
			kanjifputs(subst[nsubst], stderr);
			if (nsubst) fputc(' ', stderr);
			else {
				fputc('\n', stderr);
				fflush(stderr);
			}
		}
	}
	free(subst);
	free(len);

	ret = exec_simplecom(trp, type, id, bg);

	if (keepvar) {
#ifndef	_NOUSEHASH
		if (pathvar != getshellvar("PATH", -1)) {
			searchhash(NULL, NULL, NULL);
			hashtable = htable;
		}
		else if (htable) searchhash(htable, NULL, NULL);
#endif
		freevar(shellvar);
		freevar(exportvar);
		shellvar = svar;
		exportvar = evar;
		exportsize = esize;
	}
#ifdef	FD
	if (type != CT_NONE && type != CT_FDINTERNAL && type != CT_FUNCTION)
		internal_status = -2;
#endif

	comm -> argc = argc;
	freevar(comm -> argv);
	comm -> argv = argv;
#ifdef	FD
	evalenv();
#endif
	return(ret);
}

/*ARGSUSED*/
static int NEAR exec_process(trp, pipein)
syntaxtree *trp;
int pipein;
{
#if	!MSDOS
	syntaxtree *tmptr;
	long pid;
	int sub, pipeend;
#endif
	syntaxtree *next;
	int ret;

	next = statementcheck(trp -> next, SM_STATEMENT);
	if (getstatid(next) == SM_LPAREN - 1) {
#if	!MSDOS
		tmptr = trp -> next;
#endif
	}
	else {
#if	!MSDOS
		tmptr = trp;
#endif
		next = NULL;
	}
#if	!MSDOS
	pipeend = (!(tmptr -> next) && (tmptr -> flags & ST_NEXT)
		&& (tmptr = trp -> parent)
		&& (tmptr -> flags & ST_TYPE) == OP_PIPE) ? 1 : 0;
	sub = 0;
# if	!defined (BASHSTYLE) && !defined (USEFAKEPIPE)
	/* bash does not treat the end of pipeline as sub shell */
	if (pipeend) sub = 1;
# endif

	if (isstatement(trp -> comm)) {
		if ((trp -> comm) -> id == SM_CHILD) sub = 0;
# if	!defined (NOJOB) && defined (CHILDSTATEMENT)
		else if (jobok && mypid == orgpgrp) sub = 1;
# endif
# ifndef	BASHSTYLE
	/* bash does not treat redirected statements as sub shell */
		else if ((trp -> comm) -> redp) sub = 1;
# endif
	}

	if (sub) {
		if ((pid = makechild(1, -1)) < 0) return(-1);
		else if (!pid) {
# ifndef	NOJOB
			stackjob(mypid, 0, trp);
# endif
		}
		else {
# ifndef	NOJOB
			stackjob(pid, 0, trp);
# endif
			return(waitchild(pid, trp));
		}
	}
#endif	/* !MSDOS */

	if (isstatement(trp -> comm)) {
		if ((trp -> comm) -> id == SM_CHILD) ret = dochild(trp);
		else ret = exec_statement(trp);
	}
	else if (next) ret = setfunc((trp -> comm) -> argv[0], next);
#if	!MSDOS
	else if (sub) ret = exec_command(trp, 1);
# if	defined (BASHSTYLE) && !defined (USEFAKEPIPE)
	/* bash invokes all external commands in pipeline from current shell */
	else if (!pipein) ret = exec_command(trp, 1);
# endif
# ifndef	NOJOB
	else if (ttypgrp < 0 && !(trp -> next))
		ret = exec_command(trp, 1);
# endif
#endif	/* !MSDOS */
	else ret = exec_command(trp, 0);

#if	!MSDOS
	if (sub) {
		prepareexit(1);
		Xexit((ret >= 0) ? ret : RET_NOTEXEC);
	}
#endif
	return(ret);
}

static int NEAR exec_stree(trp, cond)
syntaxtree *trp;
int cond;
{
#if	!MSDOS
	long pid;
#endif
	syntaxtree *tmptr;
	redirectlist *errp;
	long pipein;
	int ret, fd;

	exectrapcomm();
	if (breaklevel || continuelevel) return(RET_SUCCESS);

	if ((trp -> flags & ST_TYPE) == OP_NOT) {
		trp = skipfuncbody(trp);
		if (!(trp -> next)) return(RET_FAIL);
		if ((ret = exec_stree(trp -> next, cond)) < 0) return(-1);
		return((ret != RET_SUCCESS) ? RET_SUCCESS : RET_FAIL);
	}

#if	!MSDOS
	if ((trp -> flags & ST_TYPE) != OP_BG
	&& (trp -> flags & ST_TYPE) != OP_NOWN) pid = -1;
	else if ((pid = makechild(0, -1)) < 0) return(-1);
	else if (!pid) {
# ifndef	NOJOB
		ttypgrp = -1;
		if ((trp -> flags & ST_TYPE) != OP_NOWN)
			stackjob(mypid, 0, trp);
		if (jobok);
		else
# endif
		if ((fd = Xopen(NULLDEVICE, O_BINARY | O_RDONLY, 0666)) >= 0) {
			Xdup2(fd, STDIN_FILENO);
			safeclose(fd);
		}
	}
	else {
# ifndef	NOJOB
		childpgrp = -1;
		if ((trp -> flags & ST_TYPE) != OP_NOWN) {
			prevjob = lastjob;
			lastjob = stackjob(pid, 0, trp);
		}
# endif
		if (interactive && !nottyout) {
# ifndef	NOJOB
			if (jobok && (trp -> flags & ST_TYPE) != OP_NOWN) {
				if (mypid == orgpgrp) {
					fputc('[', stderr);
					fputlong(lastjob + 1, stderr);
					fputs("] ", stderr);
					fputlong(pid, stderr);
					fputc('\n', stderr);
					fflush(stderr);
				}
			}
			else
# endif
			{
				fputlong(pid, stderr);
				fputc('\n', stderr);
				fflush(stderr);
			}
		}
		lastpid = pid;
		trp = skipfuncbody(trp);
		if (!(trp -> next)) return(RET_SUCCESS);
		return(exec_stree(trp -> next, cond));
	}
#endif

	tmptr = skipfuncbody(trp);
	if ((tmptr -> flags & ST_TYPE) != OP_PIPE) pipein = -1;
	else if ((fd = openpipe(&pipein, STDIN_FILENO, 0, 1, -1)) < 0)
		return(-1);

	if (!(trp -> comm) || pipein > 0) {
#if	!MSDOS && !defined (NOJOB)
		if (pipein > 0) stackjob(pipein, 0, trp);
#endif
		ret = ret_status;
	}
	else if (trp -> flags & ST_NODE) {
		ret = exec_stree((syntaxtree *)(trp -> comm), cond);
		if (ret < 0) return(-1);
	}
	else if (!((trp -> comm) -> redp)) ret = exec_process(trp, pipein);
	else {
		if (!(errp = doredirect((trp -> comm) -> redp)))
			ret = exec_process(trp, pipein);
		else {
			if (!errno) execerror(NULL,
				errp -> filename, ER_RESTRICTED);
			else if (errno < 0);
			else if (errp -> filename)
				doperror(NULL, errp -> filename);
			else doperror(NULL, "-");
			ret = RET_FAIL;
		}
		closeredirect((trp -> comm) -> redp);
	}
	trp = tmptr;

	ret_status = (ret >= 0) ? ret : RET_FAIL;
	if (returnlevel) {
		if (pipein >= 0) closepipe(fd);
		return(ret);
	}
	if (ret < 0) {
		if (pipein >= 0) closepipe(fd);
		return(-1);
	}
	if (pipein >= 0 && (fd = reopenpipe(fd, RET_SUCCESS)) < 0) return(-1);
#if	!MSDOS
	if (!pid) {
		prepareexit(1);
		Xexit(RET_SUCCESS);
	}
# ifndef	NOJOB
#  ifndef	USEFAKEPIPE
	if ((trp -> flags & ST_TYPE) == OP_PIPE);
	else
#  endif
	if (jobok && mypid == orgpgrp) childpgrp = -1;
# endif
#endif
	if (interrupted) return(RET_INTR);

	if (trp -> next) {
		if (errorexit && !cond
		&& (trp -> flags & ST_TYPE) == OP_FG && ret)
			breaklevel = loopdepth;
		else if (((trp -> flags & ST_TYPE) != OP_AND || !ret)
		&& ((trp -> flags & ST_TYPE) != OP_OR || ret))
			ret = exec_stree(trp -> next, cond);
	}
	if (pipein >= 0) closepipe(fd);
	exectrapcomm();
	return(ret);
}

static syntaxtree *NEAR execline(command, stree, trp)
char *command;
syntaxtree *stree, *trp;
{
	int ret;

	if (!trp) {
		stree -> comm = NULL;
		stree -> parent = stree -> next = NULL;
		stree -> flags = 0;
		trp = stree;
	}

	if (!(trp = analyze(command, trp, 0))) {
		freestree(stree);
		if (errorexit) {
			free(stree);
#if	!MSDOS && !defined (NOJOB)
			if (loginshell && interactive_io) killjob();
#endif
			prepareexit(0);
			Xexit2(RET_SYNTAXERR);
		}
		if (syntaxerrno) execerrno = syntaxerrno;
		return(NULL);
	}

	if (trp -> flags & ST_CONT) return(trp);

	if (notexec) {
		ret = RET_SUCCESS;
#ifdef	SHOWSTREE
		show_stree(stree, 0);
#endif
	}
#ifndef	_NOUSEHASH
	else if (hashahead && check_stree(stree) < 0)
		ret = ret_status = RET_FAIL;
#endif
	else ret = exec_stree(stree, 0);
	errorexit = tmperrorexit;
	freestree(stree);

	if (ret < 0 && errno) doperror(NULL, shellname);
	if ((errorexit && ret) || terminated) {
		free(stree);
#if	!MSDOS && !defined (NOJOB)
		if (loginshell && interactive_io) killjob();
#endif
		prepareexit(0);
		Xexit2(ret);
	}
	return(NULL);
}

int exec_line(command)
char *command;
{
	static syntaxtree *stree = NULL;
	static syntaxtree *trp;
	int ret;

	if (!command || command == (char *)-1) {
		if (stree) {
			if (command && trp && !(trp -> flags & ST_STAT))
				execline(command, stree, trp);
			freestree(stree);
			free(stree);
			stree = trp = NULL;
		}
		return(0);
	}

	setsignal();
	if (verboseinput) {
		kanjifputs(command, stderr);
		fputc('\n', stderr);
		fflush(stderr);
	}
	if (!stree) {
		stree = newstree(NULL);
		trp = NULL;
	}

#if	!MSDOS && !defined (NOJOB)
	childpgrp = -1;
#endif
	if ((trp = execline(command, stree, trp))) ret = trp -> flags;
	else {
		free(stree);
		stree = trp = NULL;
		ret = 0;
	}
	resetsignal(0);
	return(ret);
}

static int NEAR _dosystem(command)
char *command;
{
	syntaxtree *trp;
	int ret;

	if (!(trp = analyzeline(command))) ret = RET_SYNTAXERR;
	else {
		if (notexec) {
			ret = RET_SUCCESS;
#ifdef	SHOWSTREE
			show_stree(trp, 0);
#endif
		}
#ifndef	_NOUSEHASH
		else if (hashahead && check_stree(trp) < 0)
			ret = RET_FAIL;
#endif
		else ret = exec_stree(trp, 0);
		freestree(trp);
		free(trp);
	}
	errorexit = tmperrorexit;
	return((ret >= 0) ? ret : RET_FAIL);
}

int dosystem(command)
char *command;
{
	int ret;

	setsignal();
#if	!MSDOS && !defined (NOJOB)
	childpgrp = -1;
#endif
	ret = _dosystem(command);
	resetsignal(0);

	return(ret);
}

static FILE *NEAR _dopopen(command)
char *command;
{
	syntaxtree *trp;
	long pipein;
	int fd;

	if (!(trp = analyzeline(command))) return(NULL);

	if ((fd = openpipe(&pipein, STDIN_FILENO, 1, 1, mypid)) < 0);
	else if (trp -> comm && pipein <= 0) {
		if (notexec) {
#ifdef	SHOWSTREE
			show_stree(trp, 0);
#endif
			closepipe(fd);
			fd = -1;
		}
#ifndef	_NOUSEHASH
		else if (hashahead && check_stree(trp) < 0) {
			closepipe(fd);
			fd = -1;
		}
#endif
		else if (exec_stree(trp, 0) < 0) {
			closepipe(fd);
			fd = -1;
		}
		else fd = reopenpipe(fd, ret_status);
	}
	freestree(trp);
	free(trp);
	errorexit = tmperrorexit;
	return((fd >= 0) ? fdopenpipe(fd) : NULL);
}

FILE *dopopen(command)
char *command;
{
	FILE *fp;

	nottyout = 1;
	setsignal();
#if	!MSDOS && !defined (NOJOB)
	childpgrp = -1;
#endif
	fp = _dopopen(command);
	resetsignal(0);
	nottyout = 0;
	return(fp);
}

int dopclose(fp)
FILE *fp;
{
	return(closepipe(fileno(fp)));
}

static int NEAR sourcefile(fd, fname, verbose)
int fd;
char *fname;
int verbose;
{
	syntaxtree *stree, *trp;
	char *buf, **dupargvar;
	int n, ret, dupinteractive;

	dupargvar = argvar;
	argvar = (char **)malloc2(2 * sizeof(char *));
	argvar[0] = strdup2(fname);
	argvar[1] = NULL;
	dupinteractive = interactive;
	interactive = 0;
	stree = newstree(NULL);
	trp = NULL;
	ret= RET_SUCCESS;
	n = errno = 0;
	while ((buf = readline(fd))) {
		trp = execline(buf, stree, trp);
		n++;
		if (execerrno) {
			if (verbose) {
				fputs(fname, stderr);
				fputc(':', stderr);
				fputlong(n, stderr);
				fputs(": ", stderr);
				fputs(buf, stderr);
				fputc('\n', stderr);
			}
			free(buf);
			ret_status = RET_FAIL;
			ERRBREAK;
		}
		if (ret < ret_status) ret = ret_status;
		free(buf);
	}
	safeclose(fd);

	if (ret == RET_SUCCESS && errno) {
		doperror(NULL, fname);
		ret = ret_status = RET_FAIL;
	}
	if (trp) {
		if (!(trp -> flags & ST_STAT)) execline(NULL, stree, trp);
		else {
			syntaxerror("", ER_UNEXPEOF);
			ret = ret_status = RET_SYNTAXERR;
		}
	}
	freestree(stree);
	free(stree);
	freevar(argvar);
	argvar = dupargvar;
	interactive = dupinteractive;
	return(ret);
}

int execruncom(fname, verbose)
char *fname;
int verbose;
{
	int fd, ret, duprestricted;

#ifdef	FD
	inruncom = 1;
#endif
	setsignal();
	fname = evalpath(strdup2(fname), 1);
	if ((fd = newdup(Xopen(fname, O_BINARY | O_RDONLY, 0666))) < 0)
		ret = RET_SUCCESS;
	else {
		duprestricted = restricted;
		restricted = 0;
		ret = sourcefile(fd, fname, verbose);
		restricted = duprestricted;
	}
	resetsignal(0);
	free(fname);
#ifdef	FD
	inruncom = 0;
#endif
	return(ret);
}

int initshell(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
#if	!MSDOS
# if	!defined (NOJOB) && defined (NTTYDISC)
	ldiscioctl_t tty;
# endif
	struct passwd *pwd;
	sigmask_t mask;
#endif
	char *cp, *name, *home;
	int i, n, len, fd;

	shellname = argv[0];
	if ((name = strrdelim(shellname, 1))) name++;
	else name = shellname;
	if (*name == '-') {
		loginshell = 1;
		name++;
	}

	opentty();
	ttyio = newdup(ttyio);
	inittty(0);
	getterment();

	definput = STDIN_FILENO;
	interactive =
		(isatty(STDIN_FILENO) && isatty(STDERR_FILENO)) ? 1 : 0;
	ttypgrp = orgpid = mypid = getpid();
#if	!MSDOS && !defined (NOJOB)
	if ((orgpgrp = getpgroup()) < 0) {
		doperror(NULL, shellname);
		return(-1);
	}
#endif

	if ((n = getoption(argc, argv, envp)) < 0) return(-1);
#ifdef	BASHSTYLE
	noclobber = 0;
#endif
	errorexit = tmperrorexit;
	if (n > 2) interactive = interactive_io;
	else if (n >= argc || forcedstdin) forcedstdin = 1;
	else {
		definput = newdup(Xopen(argv[n], O_BINARY | O_RDONLY, 0666));
		if (definput < 0) {
			kanjifputs(argv[n], stderr);
			fputs(": cannot open\n", stderr);
			fflush(stderr);
			return(-1);
		}
		if (!interactive_io || !isatty(definput)) {
			interactive = 0;
#if	!MSDOS
			closeonexec(definput);
#endif
		}
		else {
			Xclose(STDIN_FILENO);
			if (Xdup2(definput, STDIN_FILENO) < 0) {
				kanjifputs(argv[n], stderr);
				fputs(": cannot open\n", stderr);
				fflush(stderr);
				return(-1);
			}
			safeclose(definput);
			definput = STDIN_FILENO;
		}
	}
	interactive_io = interactive;

	setnbuf(stdin);
#if	!MSDOS
	setlbuf(stderr);
	setlbuf(stdout);
	Xsigemptyset(mask);
	Xsigblock(oldsigmask, mask);
#endif

	if (n > 2) dupstdin = NULL;
	else if ((fd = newdup(Xdup(definput))) < 0
	|| !(dupstdin = Xfdopen(fd, "r"))) {
		doperror(NULL, shellname);
		safeclose(ttyio);
		if (fd >= 0) safeclose(fd);
		return(-1);
	}
	if (definput == STDIN_FILENO && isatty(STDIN_FILENO)) definput = ttyio;

	getvarfunc = getshellvar;
	putvarfunc = putshellvar;
	getretvalfunc = getretval;
	getpidfunc = getorgpid;
	getlastpidfunc = getlastpid;
	getarglistfunc = getarglist;
	getflagfunc = getflagstr;
	checkundeffunc = checkundefvar;
	exitfunc = safeexit;
	backquotefunc = evalbackquote;
	envvar = envp;
	shellvar = duplvar(envp, 0);
	exportvar = duplvar(envp, 0);
	exportsize = 0L;
	for (i = 0; exportvar[i]; i++) exportsize += strlen(exportvar[i]) + 1;
	exportlist = (char **)malloc2((i + 1) * sizeof(char *));
	for (i = 0; exportvar[i]; i++) {
		len = ((cp = strchr(exportvar[i], '=')))
			? cp - exportvar[i] : strlen(exportvar[i]);
		exportlist[i] = malloc2(len + 1);
		strncpy2(exportlist[i], exportvar[i], len);
	}
	exportlist[i] = NULL;
	ronlylist = duplvar(NULL, 0);
	shellfunc = (shfunctable *)malloc2(1 * sizeof(shfunctable));
	shellfunc[0].ident = NULL;
#ifndef	NOALIAS
	shellalias = (aliastable *)malloc2(1 * sizeof(aliastable));
	shellalias[0].ident = NULL;
#endif
#ifndef	_NOUSEHASH
	searchhash(NULL, NULL, NULL);
#endif
	if ((!interactive || forcedstdin) && n < argc) {
		argvar = (char **)malloc2((argc - n + 1 + forcedstdin)
			* sizeof(char *));
		for (i = 0; i < argc - n + 1; i++)
			argvar[i + forcedstdin] = strdup2(argv[i + n]);
		if (forcedstdin) argvar[0] = strdup2(argv[0]);
	}
	else {
		argvar = (char **)malloc2(2 * sizeof(char *));
		argvar[0] = strdup2(argv[0]);
		argvar[1] = NULL;
	}

	if (!(cp = getshellvar("PATH", -1))) setenv2("PATH", DEFPATH);
#if	MSDOS
	else for (i = 0; cp[i]; i++) if (cp[i] == ';') cp[i] = PATHDELIM;
#endif

	if (interactive) {
#if	!MSDOS
		if (!getuid()) setenv2("PS1", PS1ROOT);
		else
#endif
		setenv2("PS1", PS1STR);
		setenv2("PS2", PS2STR);
	}
	else {
		shellvar = putvar(shellvar, "PS1", sizeof("PS1") - 1);
		shellvar = putvar(shellvar, "PS2", sizeof("PS2") - 1);
	}
	setenv2("IFS", IFS_SET);
	if (loginshell && !(home = getshellvar("HOME", -1))) {
#if	!MSDOS
		if ((pwd = getpwuid(getuid())) && pwd -> pw_dir)
			home = pwd -> pw_dir;
		else
#endif
		home = _SS_;
		setenv2("HOME", home);
	}
#ifndef	NOPOSIXUTIL
	setenv2("OPTIND", "1");
#endif

	for (i = 0; i < NSIG; i++) {
		trapmode[i] = 0;
		trapcomm[i] = NULL;
		if ((oldsigfunc[i] = (sigcst_t)signal(i, SIG_DFL)) == SIG_ERR)
			oldsigfunc[i] = SIG_DFL;
		else signal(i, oldsigfunc[i]);
	}
	for (i = 0; i < SIGNALSIZ; i++)
		trapmode[signallist[i].sig] = signallist[i].flags;
#if	!MSDOS && !defined (NOJOB)
	if (!interactive) {
		jobok = 0;
		orgpgrp = -1;
	}
	else {
		if (jobok < 0) jobok = 1;
		if (!orgpgrp && (setpgroup(0, orgpgrp = mypid) < 0
		|| settcpgrp(ttyio, orgpgrp) < 0)) {
			doperror(NULL, shellname);
			prepareexit(0);
			return(-1);
		}
		gettcpgrp(ttyio, ttypgrp);
# ifdef	SIGTTIN
		while (ttypgrp >= 0 && ttypgrp != orgpgrp) {
			signal(SIGTTIN, SIG_DFL);
			kill(0, SIGTTIN);
			signal(SIGTTIN, oldsigfunc[SIGTTIN]);
			gettcpgrp(ttyio, ttypgrp);
		}
# endif
# ifdef	NTTYDISC
		if (tioctl(ttyio, REQGETD, &tty) < 0
		|| (ldisc(tty) != NTTYDISC && (ldisc(tty) = NTTYDISC) > 0
		&& tioctl(ttyio, REQSETD, &tty) < 0)) {
			doperror(NULL, shellname);
			prepareexit(0);
			return(-1);
		}
# endif
		if ((orgpgrp != mypid && setpgroup(0, orgpgrp = mypid) < 0)
		|| (mypid != ttypgrp && gettermio(orgpgrp) < 0)) {
			doperror(NULL, shellname);
			prepareexit(0);
			return(-1);
		}
		closeonexec(ttyio);
	}
#endif

	if (n > 2) {
		setsignal();
		if (verboseinput) {
			kanjifputs(argv[2], stderr);
			fflush(stderr);
		}
		n = _dosystem(argv[2]);
		resetsignal(0);
		prepareexit(0);
		Xexit2(n);
	}

	if (loginshell && chdir3(home) < 0) {
		fputs("No directory\n", stderr);
		prepareexit(0);
		Xexit2(RET_FAIL);
	}
	if (!(cp = getshellvar("SHELL", -1))) cp = name;
	if (!strpathcmp(cp, RSHELL) || !strpathcmp(cp, RFD)) restricted = 1;
#ifdef	FD
	fd_restricted = restricted;
#endif

	return(0);
}

int shell_loop(pseudoexit)
int pseudoexit;
{
	char *ps, *buf;
	int flags;

	shellmode = 1;
	setsignal();
	flags = 0;
	if (pseudoexit) exit_status = -1;
	for (;;) {
		trapok = 1;
		if (!interactive) buf = readline(definput);
		else {
			ps = getshellvar((flags) ? "PS2" : "PS1", -1);
#ifdef	FD
			ttyiomode();
			buf = inputshellstr(ps, -1, 1, NULL);
			stdiomode();
			if (!buf) continue;
			if (buf == (char *)-1) {
				buf = NULL;
				errno = 0;
			}
#else
			kanjifputs(ps, stderr);
			fflush(stderr);
			buf = readline(definput);
#endif
		}
		trapok = 0;
		if (!buf) {
			if (errno) {
				doperror(NULL, shellname);
				exec_line(NULL);
				break;
			}
			if (interactive && ignoreeof) {
				if (flags) {
					exec_line(NULL);
					syntaxerror("", ER_UNEXPEOF);
				}
				else {
					fputs("Use \"", stderr);
					fputs((loginshell) ? "logout" : "exit",
						stderr);
					fputs("\" to leave to the shell.\n",
						stderr);
				}
				flags = 0;
				continue;
			}
			if (!flags) break;
#ifndef	BASHSTYLE
	/* bash ignores the last quoted argument before EOF */
			if (flags & (ST_META | ST_QUOT)) {
				exec_line((char *)-1);
				break;
			}
#endif
			exec_line(NULL);
			syntaxerror("", ER_UNEXPEOF);
			flags = 0;
			ERRBREAK;
		}
		flags = exec_line(buf);
		free(buf);
		if (pseudoexit && exit_status >= 0) break;
	}
	resetsignal(0);
	if (exit_status >= 0) ret_status = exit_status;
	shellmode = 0;
	return(ret_status);
}

int main_shell(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
	if (initshell(argc, argv, envp) < 0) return(RET_FAIL);
	getwsize(0, 0);
	ret_status = RET_SUCCESS;
	if (loginshell) {
		ret_status = execruncom(DEFRUNCOM, 0);
		ret_status = execruncom(RUNCOMFILE, 0);
	}
#ifdef	FD
	else {
		ret_status = execruncom(DEFRUNCOM, 1);
		ret_status = execruncom(FD_RUNCOMFILE, 1);
	}
	evalenv();

	loadhistory(0, FD_HISTORYFILE);
	entryhist(1, origpath, 1);
#endif
	shell_loop(0);
#ifdef	FD
	if (savehist > 0) savehistory(0, FD_HISTORYFILE);
#endif
#if	!MSDOS && !defined (NOJOB)
	if (loginshell && interactive) killjob();
#endif
	prepareexit(0);
	return(ret_status);
}

#ifdef	FDSH
int main(argc, argv, envp)
int argc;
char *argv[], *envp[];
{
	int ret;

# ifdef	DEBUG
	mtrace();
# endif
	ret = main_shell(argc, argv, envp);
	Xexit2(ret);
	return(0);
}
#endif	/* FDSH */
#endif	/* !_NOORIGSHELL && (!FD || (FD >= 2)) */
