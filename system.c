/*
 *	system.c
 *
 *	Command Line Analysis
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "machine.h"
#include "kctype.h"

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#if	MSDOS && defined (_NOUSELFN) && !defined (_NODOSDRIVE)
#define	_NODOSDRIVE
#endif

#if	defined (_NOKANJICONV) && !defined (_NOKANJIFCONV)
#define	_NOKANJIFCONV
#endif

#if	MSDOS
#include <dos.h>
#define	FR_CARRY	00001
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
# define	PTR_SEG(ptr)		FP_SEG(ptr)
# define	PTR_OFF(ptr, ofs)	FP_OFF(ptr)
# endif	/* !DJGPP */
#endif	/* MSDOS */

#ifdef	DEBUG
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
/* #define BASHBUG		; keep bash's bug strictly */
/* #define PSIGNALSTYLE		; based upon psignal(3) messages */
/* #define NOJOB		; not use job control */
/* #define CHILDSTATEMENT	; make any statement child for suspend */
/* #define NOALIAS		; not use alias */
/* #define DOSCOMMAND		; emulate builtin commands of COMMAND.COM */
/* #define USEFAKEPIPE		; use DOS-like pipe instead of pipe(2) */
/* #define SHOWSTREE		; show syntax tree with -n option */
/* #define NOPOSIXUTIL		; not use POSIX utilities */
/* #define STRICTPOSIX		; keep POSIX strictly */
/* #define MINIMUMSHELL		; omit verbose extension from Bourne shell */

#if	MSDOS
#include <process.h>
#include <io.h>
#include <sys/timeb.h>
#include "unixemu.h"
#define	Xexit		exit
#define	getdtablesize()	20
#define	DEFPATH		":"
#else	/* !MSDOS */
#include <pwd.h>
#include <sys/file.h>
#include <sys/param.h>
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
#define	DEFPATH		":/bin:/usr/bin"
#define	NULLDEVICE	"/dev/null"
#endif	/* !MSDOS */

#include "pathname.h"
#include "system.h"

#if	!defined (FD) || (FD >= 2 && !defined (_NOORIGSHELL))

#ifdef	FD
#include "term.h"
extern VOID main_fd __P_((char *));
extern int sigvecset __P_((int));
#ifndef	_NOCUSTOMIZE
VOID saveorigenviron __P_((VOID_A));
#endif
extern int checkbuiltin __P_((char *));
extern int checkinternal __P_((char *));
extern int execbuiltin __P_((int, int, char *[]));
extern int execinternal __P_((int, int, char *[]));
# ifndef	_NOCOMPLETE
extern int completebuiltin __P_((char *, int, int, char ***));
extern int completeinternal __P_((char *, int, int, char ***));
# endif
extern VOID evalenv __P_((VOID_A));
extern int underhome __P_((char *));
extern int replaceargs __P_((int *, char ***, char **, int));
extern char *inputshellstr __P_((char *, int, char *));
extern int entryhist __P_((int, char *, int));
extern int loadhistory __P_((int, char *));
extern int savehistory __P_((int, char *));
extern int savehist;
extern int internal_status;
extern char fullpath[];
extern char *origpath;
extern int inruncom;
# ifdef	SIGALRM
extern int noalrm;
# endif
extern int fd_restricted;
extern int physical_path;
# if	!MSDOS && !defined (_NOKANJIFCONV)
extern int nokanjifconv;
# endif
# ifndef	_NODOSDRIVE
# define	DOSFDOFFSET	(1 << (8 * sizeof(int) - 2))
# endif
extern char *deftmpdir;
#else	/* !FD */
# ifdef	__TURBOC__
extern unsigned _stklen = 0x8000;
# endif
# if	MSDOS
# define	deftmpdir	"\\"
# define	TTYNAME		"CON"
# else
# define	deftmpdir	"/tmp"
# define	TTYNAME		"/dev/tty"
# endif
int ttyio = -1;
FILE *ttyout = NULL;
# ifdef	DEBUG
# define	exit2(n)	(muntrace(), exit(n))
# else
# define	exit2		exit
# endif
#endif	/* !FD */

#if	(GETTODARGS == 1)
#define	gettimeofday2(tv, tz)	gettimeofday(tv)
#else
#define	gettimeofday2(tv, tz)	gettimeofday(tv, tz)
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

#ifdef	USESETVBUF
#define	setnbuf(f)	setvbuf(f, NULL, _IONBF, 0)
#define	setlbuf(f)	setvbuf(f, NULL, _IOLBF, 0)
#else
#define	setnbuf(f)	setbuf(f, NULL)
#define	setlbuf(f)	setlinebuf(f)
#endif

#if	MSDOS
#define	havetty()	(1)
#else
# ifdef	NOJOB
# define	havetty()	(mypid == orgpid)
# else
# define	havetty()	(ttypgrp >= 0 && mypid == ttypgrp)
# endif
#endif

#if	!MSDOS
# ifdef	USEWAITPID
# define	Xwait3(wp, opts, ru)	waitpid(-1, wp, opts)
# define	Xwait4(p, wp, opts, ru)	waitpid(p, wp, opts)
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
# define	getwaitsig(x)	((WIFSTOPPED(x)) ? WSTOPSIG(x) : \
				((WIFSIGNALED(x)) ? (128 + WTERMSIG(x)) : -1))
#endif	/* !MSDOS */

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
# if	MSDOS || !defined (_NODOSDRIVE)
extern int _dospath __P_((char *));
# endif
extern char *Xgetwd __P_((char *));
extern int Xstat __P_((char *, struct stat *));
extern int Xlstat __P_((char *, struct stat *));
extern int Xaccess __P_((char *, int));
extern int Xopen __P_((char *, int, int));
# ifdef	_NODOSDRIVE
# define	Xclose		close
# define	Xread		read
# define	Xwrite		write
# define	Xfdopen		fdopen
# define	Xfclose		fclose
# else	/* !_NODOSDRIVE */
extern int Xclose __P_((int));
extern int Xread __P_((int, char *, int));
extern int Xwrite __P_((int, char *, int));
extern FILE *Xfdopen __P_((int, char*));
extern int Xfclose __P_((FILE *));
# endif	/* !_NODOSDRIVE */
# if	!defined (LSI_C) && defined (_NODOSDRIVE)
# define	Xdup		dup
# define	Xdup2		dup2
# else
extern int Xdup __P_((int));
extern int Xdup2 __P_((int, int));
# endif
extern int kanjifputs __P_((char *, FILE *));
#else	/* !FD */
#define	_dospath(s)	(isalpha(*(s)) && (s)[1] == ':')
# ifdef	DJGPP
extern char *Xgetwd __P_((char *));
# else	/* !DJGPP */
#  ifdef	USEGETWD
#  define	Xgetwd		(char *)getwd
#  else
#  define	Xgetwd(p)	(char *)getcwd(p, MAXPATHLEN)
#  endif
# endif	/* !DJGPP */
#define	Xstat(f, s)	(stat(f, s) ? -1 : 0)
# if	MSDOS
# define	Xlstat(f, s)	(stat(f, s) ? -1 : 0)
# else
# define	Xlstat		lstat
# endif
#define	Xaccess(p, m)	(access(p, m) ? -1 : 0)
#define	Xunlink(p)	(unlink(p) ? -1 : 0)
#define	Xopen		open
#define	Xclose		close
#define	Xread		read
#define	Xwrite		write
#define	Xfdopen		fdopen
#define	Xfclose		fclose
# ifndef	LSI_C
# define	Xdup		dup
# define	Xdup2		dup2
# else
static int NEAR Xdup __P_((int));
static int NEAR Xdup2 __P_((int, int));
# endif
# if	MSDOS
#  ifdef	DJGPP
#  define	Xmkdir(p, m)	(mkdir(p, m) ? -1 : 0)
#  else
int Xmkdir __P_((char *, int));
#  endif
# else
# define	Xmkdir		mkdir
# endif
#define	Xrmdir(p)	(rmdir(p) ? -1 : 0)
#define	kanjifputs	fputs
#endif	/* !FD */

#ifndef	O_BINARY
#define	O_BINARY	0
#endif
#ifndef	O_TEXT
#define	O_TEXT		0
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
#ifndef	PENDIN
#define	PENDIN		0
#endif
#ifndef	IEXTEN
#define	IEXTEN		0
#endif
#ifndef	ECHOCTL
#define	ECHOCTL		0
#endif
#ifndef	ECHOKE
#define	ECHOKE		0
#endif
#ifndef	OCRNL
#define	OCRNL		0
#endif
#ifndef	ONOCR
#define	ONOCR		0
#endif
#ifndef	ONLRET
#define	ONLRET		0
#endif
#ifndef	TAB3
#define	TAB3		OXTABS
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

#ifndef	CLK_TCK
# ifdef	HZ
# define	CLK_TCK	HZ
# else
# define	CLK_TCK	60
# endif
#endif

#ifdef	DOSCOMMAND
extern int doscomdir __P_((int, char *[]));
extern int doscommkdir __P_((int, char *[]));
extern int doscomrmdir __P_((int, char *[]));
extern int doscomerase __P_((int, char *[]));
extern int doscomrename __P_((int, char *[]));
extern int doscomcopy __P_((int, char *[]));
extern int doscomcls __P_((int, char *[]));
extern int doscomtype __P_((int, char *[]));
#endif	/* DOSCOMMAND */

#ifndef	MINIMUMSHELL
# if	!MSDOS && !defined (NOJOB)
extern int gettermio __P_((p_id_t));
extern VOID dispjob __P_((int, FILE *));
extern int searchjob __P_((p_id_t, int *));
extern int getjob __P_((char *));
extern int stackjob __P_((p_id_t, int, syntaxtree *));
extern int stoppedjob __P_((p_id_t));
extern VOID killjob __P_((VOID_A));
extern VOID checkjob __P_((int));
# endif	/* !MSDOS && !NOJOB */
extern char *evalposixsubst __P_((char *, int *));
# if	!MSDOS
extern VOID replacemailpath __P_((char *, int));
extern VOID checkmail __P_((int));
# endif
# ifndef	NOALIAS
extern VOID NEAR freealias __P_((aliastable *));
extern int checkalias __P_((syntaxtree *, char *, int, int));
# endif
# if	!MSDOS && !defined (NOJOB)
extern int posixjobs __P_((syntaxtree *));
extern int posixfg __P_((syntaxtree *));
extern int posixbg __P_((syntaxtree *));
extern int posixdisown __P_((syntaxtree *));
# endif
# ifndef	NOALIAS
extern int posixalias __P_((syntaxtree *));
extern int posixunalias __P_((syntaxtree *));
# endif
extern int posixkill __P_((syntaxtree *));
extern int posixtest __P_((syntaxtree *));
# ifndef	NOPOSIXUTIL
extern int posixcommand __P_((syntaxtree *));
extern int posixgetopts __P_((syntaxtree *));
# endif
#endif	/* !MINIMUMSHELL */

extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *c_realloc __P_((char *, ALLOC_T, ALLOC_T *));
extern char *strdup2 __P_((char *));
extern char *strncpy2 __P_((char *, char *, int));
extern char *ascnumeric __P_((char *, long, int, int));

#ifdef	DEBUG
extern VOID mtrace __P_ ((VOID));
extern VOID muntrace __P_ ((VOID));
extern char *_mtrace_file;
#endif

#ifndef	MINIMUMSHELL
# if	!MSDOS && !defined (NOJOB)
extern jobtable *joblist;
extern int maxjobs;
# endif
# if	!MSDOS
extern int mailcheck;
# endif
# ifndef	NOALIAS
extern aliastable *shellalias;
# endif
# ifndef	NOPOSIXUTIL
extern int posixoptind;
# endif
#endif	/* !MINIMUMSHELL */

#ifdef	FD
extern int mktmpfile __P_((char *));
extern int rmtmpfile __P_((char *));
extern char **duplvar __P_((char **, int));
# if	MSDOS
extern int setcurdrv __P_((int, int));
# endif
extern int chdir3 __P_((char *));
extern int setenv2 __P_((char *, char *));
#else	/* !FD */
# if	!MSDOS || !defined (MINIMUMSHELL)
time_t time2 __P_((VOID_A));
# endif
static int NEAR genrand __P_((int));
static char *NEAR genrandname __P_((char *, int));
static int NEAR mktmpfile __P_((char *));
static int NEAR rmtmpfile __P_((char *));
static char **NEAR duplvar __P_((char **, int));
# ifdef	DJGPP
int dos_putpath __P_((char *, int));
# endif
# if	MSDOS
int int21call __P_((__dpmi_regs *, struct SREGS *));
static int NEAR setcurdrv __P_((int, int));
int chdir3 __P_((char *));
# else	/* !MSDOS */
# define	chdir3(p)	(chdir(p) ? -1 : 0)
# endif	/* !MSDOS */
int setenv2 __P_((char *, char *));
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
static VOID NEAR syntaxerror __P_((char *));
static VOID NEAR fputoctal __P_((int, int, FILE *));
#if	!MSDOS
static int NEAR closeonexec __P_((int));
static VOID NEAR setstopsig __P_((int));
static p_id_t NEAR makechild __P_((int, p_id_t));
#endif	/* !MSDOS */
static VOID NEAR safeclose __P_((int));
static VOID NEAR safefclose __P_((FILE *));
static VOID NEAR safermtmpfile __P_((char *));
static int NEAR getoption __P_((int, char *[], char *[]));
static ALLOC_T NEAR c_allocsize __P_((int));
static int NEAR readchar __P_((int));
static char *NEAR readline __P_((int));
static char *NEAR readfile __P_((int, ALLOC_T *));
static heredoc_t *NEAR newheredoc __P_((char *, int));
static VOID NEAR freeheredoc __P_((heredoc_t *, int, int));
static redirectlist *NEAR newrlist __P_((int, char *, int, redirectlist *));
static int NEAR freerlist __P_((redirectlist *, int, int));
static command_t *NEAR newcomm __P_((VOID_A));
static VOID NEAR freecomm __P_((command_t *, int));
static syntaxtree *NEAR eldeststree __P_((syntaxtree *));
static syntaxtree *NEAR childstree __P_((syntaxtree *, int));
static syntaxtree *NEAR skipfuncbody __P_((syntaxtree *));
static syntaxtree *NEAR insertstree __P_((syntaxtree *, syntaxtree *, int));
static syntaxtree *NEAR linkstree __P_((syntaxtree *, int));
static VOID NEAR nownstree __P_((syntaxtree *));
static int NEAR evalfiledesc __P_((char *));
static int NEAR isvalidfd __P_((int));
static int NEAR newdup __P_((int));
static int NEAR redmode __P_((int));
static VOID NEAR closeredirect __P_((redirectlist *));
static heredoc_t *NEAR searchheredoc __P_((syntaxtree *, int));
static int NEAR saveheredoc __P_((char *, syntaxtree *));
static int NEAR openheredoc __P_((heredoc_t *, int));
#if	defined (FD) && !defined (_NODOSDRIVE)
static int NEAR fdcopy __P_((int, int));
static int NEAR openpseudofd __P_((redirectlist *));
static int NEAR closepseudofd __P_((redirectlist *));
#endif
static redirectlist *NEAR doredirect __P_((redirectlist *));
static int NEAR redirect __P_((syntaxtree *, int, char *, int));
#if	!MSDOS && defined (MINIMUMSHELL)
static VOID NEAR checkmail __P_((int));
#endif
static char *NEAR getvar __P_((char **, char *, int));
static char **NEAR putvar __P_((char **, char *, int));
static int NEAR checkprimal __P_((char *, int));
static int NEAR checkronly __P_((char *, int));
static int NEAR _putshellvar __P_((char *, int));
static heredoc_t *NEAR duplheredoc __P_((heredoc_t *));
static redirectlist *NEAR duplrlist __P_((redirectlist *));
static shfunctable *NEAR duplfunc __P_((shfunctable *));
static VOID NEAR freefunc __P_((shfunctable *));
static int cmpfunc __P_((CONST VOID_P, CONST VOID_P));
static char *NEAR getifs __P_((VOID_A));
static int getretval __P_((VOID_A));
static p_id_t getorgpid __P_((VOID_A));
static p_id_t getlastpid __P_((VOID_A));
static char *getflagstr __P_((VOID_A));
static int checkundefvar __P_((char *, char *, int));
static VOID safeexit __P_((VOID_A));
static int NEAR getparenttype __P_((syntaxtree *));
static int NEAR parsestatement __P_((syntaxtree **, int, int, int));
static syntaxtree *NEAR _addarg __P_((syntaxtree *, char *));
static int NEAR addarg __P_((syntaxtree **, redirectlist *,
		char *, int *, int));
static int NEAR evalredprefix __P_((syntaxtree **, redirectlist *, int *));
static syntaxtree *NEAR rparen __P_((syntaxtree *));
static syntaxtree *NEAR semicolon __P_((syntaxtree *, redirectlist *,
		char *, int *));
static syntaxtree *NEAR ampersand __P_((syntaxtree *, redirectlist *,
		char *, int *));
static syntaxtree *NEAR vertline __P_((syntaxtree *, char *, int *));
static syntaxtree *NEAR lessthan __P_((syntaxtree *, redirectlist *,
		char *, int *));
static syntaxtree *NEAR morethan __P_((syntaxtree *, redirectlist *,
		char *, int *));
#if	defined (BASHSTYLE) || !defined (MINIMUMSHELL)
static syntaxtree *NEAR endvar __P_((syntaxtree *, redirectlist *,
		char *, int *, int *, ALLOC_T *, int));
static syntaxtree *NEAR addvar __P_((syntaxtree *,
		char *, int *, char *, int *, int));
#endif
static syntaxtree *NEAR normaltoken __P_((syntaxtree *, redirectlist *,
		char *, int *, int *, ALLOC_T *));
static syntaxtree *NEAR casetoken __P_((syntaxtree *, redirectlist *,
		char *, int *, int *));
#if	!defined (BASHBUG) && !defined (MINIMUMSHELL)
static int NEAR cmpstatement __P_((char *, int));
static syntaxtree *NEAR comsubtoken __P_((syntaxtree *, redirectlist *,
		char *, int *, int *, ALLOC_T *));
#endif
static syntaxtree *NEAR analyzeloop __P_((syntaxtree *, redirectlist *,
		char *, int));
static syntaxtree *NEAR analyzeeof __P_((syntaxtree *));
static syntaxtree *NEAR statementcheck __P_((syntaxtree *, int));
static int NEAR check_statement __P_((syntaxtree *));
static int NEAR check_command __P_((syntaxtree *));
static int NEAR check_stree __P_((syntaxtree *));
static syntaxtree *NEAR analyzeline __P_((char *));
#ifdef	DEBUG
static VOID NEAR Xexecve __P_((char *, char *[], char *[], int));
#else
static VOID NEAR Xexecve __P_((char *, char *[], char *[]));
#endif
#if	MSDOS
static char *NEAR addext __P_((char *, int));
static char **NEAR replacebat __P_((char **, char **));
#endif
#if	MSDOS || defined (USEFAKEPIPE)
static int NEAR openpipe __P_((p_id_t *, int, int));
#else
static int NEAR openpipe __P_((p_id_t *, int, int, int, p_id_t));
#endif
static pipelist **NEAR searchpipe __P_((int));
static int NEAR reopenpipe __P_((int, int));
static FILE *NEAR fdopenpipe __P_((int));
static int NEAR closepipe __P_((int));
#ifndef	_NOUSEHASH
static VOID NEAR disphash __P_((VOID_A));
#endif
static int NEAR substvar __P_((char **));
static int NEAR evalargv __P_((command_t *, int *, int));
static char *NEAR evalexternal __P_((command_t *));
static VOID NEAR printindent __P_((int, FILE *));
static VOID NEAR printnewline __P_((int, FILE *));
static int NEAR printredirect __P_((redirectlist *, FILE *));
#ifndef	MINIMUMSHELL
static VOID NEAR printheredoc __P_((redirectlist *, FILE *));
static redirectlist **NEAR _printstree __P_((syntaxtree *,
		redirectlist **, int, FILE *));
#endif
static VOID NEAR printfunction __P_((shfunctable *, FILE *));
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
#ifndef	MINIMUMSHELL
static int NEAR dologin __P_((syntaxtree *));
static int NEAR dologout __P_((syntaxtree *));
#endif
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
#if	!MSDOS && !defined (NOJOB)
static int NEAR dojobs __P_((syntaxtree *));
static int NEAR dofg __P_((syntaxtree *));
static int NEAR dobg __P_((syntaxtree *));
static int NEAR dodisown __P_((syntaxtree *));
#endif
static int NEAR dotype __P_((syntaxtree *));
#ifdef	DOSCOMMAND
static int NEAR dodir __P_((syntaxtree *));
static int NEAR domkdir __P_((syntaxtree *));
static int NEAR dormdir __P_((syntaxtree *));
static int NEAR doerase __P_((syntaxtree *));
static int NEAR dorename __P_((syntaxtree *));
static int NEAR docopy __P_((syntaxtree *));
static int NEAR docls __P_((syntaxtree *));
static int NEAR dodtype __P_((syntaxtree *));
#endif
#ifndef	NOALIAS
static int NEAR doalias __P_((syntaxtree *));
static int NEAR dounalias __P_((syntaxtree *));
#endif
static int NEAR doecho __P_((syntaxtree *));
#ifndef	MINIMUMSHELL
static int NEAR dokill __P_((syntaxtree *));
static int NEAR dotestsub1 __P_((int, char *, int *));
static int NEAR dotestsub2 __P_((int, char *[], int *));
static int NEAR dotestsub3 __P_((int, char *[], int *, int));
static int NEAR dotest __P_((syntaxtree *));
#endif
#ifndef	NOPOSIXUTIL
static int NEAR dofalse __P_((syntaxtree *));
static int NEAR docommand __P_((syntaxtree *));
static int NEAR dogetopts __P_((syntaxtree *));
static int NEAR donewgrp __P_((syntaxtree *));
# if	0
/* exists in FD original builtin */
static int NEAR dofc __P_((syntaxtree *));
# endif
#endif	/* !NOPOSIXUTIL */
#ifndef	MINIMUMSHELL
static int NEAR getworkdir __P_((char *));
static int NEAR dopushd __P_((syntaxtree *));
static int NEAR dopopd __P_((syntaxtree *));
static int NEAR dodirs __P_((syntaxtree *));
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
static char **NEAR checkshellbuiltinargv __P_((int, char **));
static int NEAR checkshellbuiltin __P_((syntaxtree *));
#if	MSDOS
static int NEAR exec_command __P_((syntaxtree *));
#else
static int NEAR exec_command __P_((syntaxtree *, int));
#endif
#if	MSDOS || defined (USEFAKEPIPE)
static int NEAR exec_process __P_((syntaxtree *));
#else
static int NEAR exec_process __P_((syntaxtree *, p_id_t));
#endif
static int NEAR exec_stree __P_((syntaxtree *, int));
static syntaxtree *NEAR execline __P_((char *,
	syntaxtree *, syntaxtree *, int));
static int NEAR exec_line __P_((char *));
static int NEAR _dosystem __P_((char *));
static FILE *NEAR _dopopen __P_((char *));
static int NEAR sourcefile __P_((int, char *, int));

#ifdef	FDSH
# if	MSDOS
# define	DEFRUNCOM	"\\etc\\profile"
# define	RUNCOMFILE	"~\\profile.rc"
# else
# define	DEFRUNCOM	"/etc/profile"
# define	RUNCOMFILE	"~/.profile"
# endif
#else	/* !FDSH */
# if	MSDOS
# define	RUNCOMFILE	"~\\fdsh.rc"
# define	FD_RUNCOMFILE	"~\\fd2.rc"
# define	FD_HISTORYFILE	"~\\fd.hst"
# else
# define	RUNCOMFILE	"~/.fdshrc"
# define	FD_RUNCOMFILE	"~/.fd2rc"
# define	FD_HISTORYFILE	"~/.fd_history"
# endif
#endif	/* !FDSH */

#define	PS1STR		"$ "
#define	PS1ROOT		"# "
#define	PS2STR		"> "
#define	RSHELL		"rfdsh"
#define	RFD		"rfd"
#define	UNLIMITED	"unlimited"
#define	MAXTMPNAMLEN	8
#if	MSDOS
#define	TMPPREFIX	"TM"
#else
#define	TMPPREFIX	"tm"
#endif
#define	BUFUNIT		32
#define	getconstvar(s)	(getshellvar(s, sizeof(s) - 1))

#ifdef	FDSH
int main __P_((int, char *[], char *[]));
#endif

int shellmode = 0;
p_id_t mypid = -1;
int ret_status = RET_SUCCESS;
int verboseexec = 0;
int notexec = 0;
int verboseinput = 0;
int terminated = 0;
int forcedstdin = 0;
int interactive_io = 0;
int tmperrorexit = 0;
int restricted = 0;
int freeenviron = 0;
int undeferror = 0;
int hashahead = 0;
#if	MSDOS
int noglob = 1;
#else
int noglob = 0;
#endif
int autoexport = 0;
#ifndef	MINIMUMSHELL
int noclobber = 0;
# if	!MSDOS && !defined (NOJOB)
int bgnotify = 0;
int jobok = -1;
# endif
int ignoreeof = 0;
#endif
int interactive = 0;
#if	!MSDOS && !defined (NOJOB)
int lastjob = -1;
int prevjob = -1;
int stopped = 0;
p_id_t orgpgrp = -1;
p_id_t childpgrp = -1;
p_id_t ttypgrp = -1;
#endif
int interrupted = 0;
int nottyout = 0;
int syntaxerrno = 0;

static char **shellvar = NULL;
static char **exportvar = NULL;
static long exportsize = 0L;
static char **exportlist = NULL;
static char **ronlylist = NULL;
static char *shellname = NULL;
static int definput = -1;
static int exit_status = RET_SUCCESS;
static sigmask_t oldsigmask;
static p_id_t orgpid = -1;
static p_id_t lastpid = -1;
#if	!MSDOS && !defined (NOJOB)
static p_id_t oldttypgrp = -1;
#endif
static int setsigflag = 0;
static int trapok = 0;
static int readtrap = 0;
static int loginshell = 0;
static int errorexit = 0;
static shfunctable *shellfunc = NULL;
static pipelist *pipetop = NULL;
static int childdepth = 0;
static int loopdepth = 0;
static int breaklevel = 0;
static int continuelevel = 0;
static int functionlevel = 0;
static int returnlevel = 0;
#ifndef	MINIMUMSHELL
static char **dirstack = NULL;
#endif
static int isshellbuiltin = 0;
static int execerrno = 0;
static char *syntaxerrstr[] = {
	"",
#define	ER_UNEXPTOK	1
	"unexpected token",
#define	ER_UNEXPNL	2
	"unexpected newline or `;'",
#define	ER_UNEXPEOF	3
	"unexpected end of file",
};
#define	SYNTAXERRSIZ	((int)(sizeof(syntaxerrstr) / sizeof(char *)))
static char *execerrstr[] = {
	"",
#define	ER_COMNOFOUND	1
	"command not found",
#define	ER_NOTFOUND	2
	"not found",
#define	ER_CANNOTEXE	3
	"cannot execute",
#define	ER_NOTIDENT	4
	"is not an identifier",
#define	ER_BADSUBST	5
	"bad substitution",
#define	ER_BADNUMBER	6
	"bad number",
#define	ER_BADDIR	7
	"bad directory",
#define	ER_CANNOTRET	8
	"cannot return when not in function",
#define	ER_CANNOTSTAT	9
	"cannot stat .",
#define	ER_CANNOTUNSET	10
	"cannot unset",
#define	ER_ISREADONLY	11
	"is read only",
#define	ER_CANNOTSHIFT	12
	"cannot shift",
#define	ER_BADOPTIONS	13
	"bad option(s)",
#define	ER_PARAMNOTSET	14
	"parameter not set",
#define	ER_MISSARG	15
	"Missing argument",
#define	ER_RESTRICTED	16
	"restricted",
#define	ER_BADULIMIT	17
	"Bad ulimit",
#define	ER_BADTRAP	18
	"bad trap",
#define	ER_NOTALIAS	19
	"is not an alias",
#define	ER_NOSUCHJOB	20
	"No such job",
#define	ER_NUMOUTRANGE	21
	"number out of range",
#define	ER_UNKNOWNSIG	22
	"unknown signal; kill -l lists signals",
#define	ER_NOHOMEDIR	23
	"no home directory",
#define	ER_INVALDRIVE	24
	"Invalid drive specification",
#define	ER_RECURSIVEFD	25
	"recursive call for FDclone",
#define	ER_INCORRECT	26
	"incorrect",
#define	ER_NOTLOGINSH	27
	"not login shell",
#define	ER_DIREMPTY	28
	"directory stack empty",
};
#define	EXECERRSIZ	((int)(sizeof(execerrstr) / sizeof(char *)))
static opetable opelist[] = {
	{OP_FG, 4, ";"},
	{OP_BG, 4, "&"},
#ifndef	MINIMUMSHELL
	{OP_NOWN, 4, "&|"},
#endif
	{OP_AND, 3, "&&"},
	{OP_OR, 3, "||"},
	{OP_PIPE, 2, "|"},
#ifndef	MINIMUMSHELL
	{OP_NOT, 2, "!"},
#endif
};
#define	OPELISTSIZ	((int)(sizeof(opelist) / sizeof(opetable)))
#if	!defined (BASHBUG) && !defined (MINIMUMSHELL)
static opetable delimlist[] = {
	{OP_NONE, 1, "{"},
	{OP_NONE, 1, ";;"},
	{OP_FG, 1, ";"},
	{OP_NONE, 0, "&>>"},
	{OP_NONE, 0, "&>|"},
	{OP_NONE, 0, "&>"},
	{OP_AND, 1, "&&"},
	{OP_NOWN, 1, "&|"},
	{OP_BG, 1, "&"},
	{OP_OR, 1, "||"},
	{OP_PIPE, 1, "|&"},
	{OP_PIPE, 1, "|"},
	{OP_NOT, 1, "!"},
	{OP_NONE, 0, "<<-"},
	{OP_NONE, 0, "<<"},
	{OP_NONE, 0, "<>"},
	{OP_NONE, 0, "<&-"},
	{OP_NONE, 0, "<&"},
	{OP_NONE, 0, "<-"},
	{OP_NONE, 0, "<"},
	{OP_NONE, 0, "><"},
	{OP_NONE, 0, ">>"},
	{OP_NONE, 0, ">&-"},
	{OP_NONE, 0, ">&"},
	{OP_NONE, 0, ">-"},
	{OP_NONE, 0, ">|"},
	{OP_NONE, 0, ">"},
};
#define	DELIMLISTSIZ	((int)(sizeof(delimlist) / sizeof(opetable)))
#endif	/* !BASHBUG && !MINIMUMSHELL */
static shbuiltintable shbuiltinlist[] = {
	{donull, ":", BT_POSIXSPECIAL},
	{dobreak, "break", BT_POSIXSPECIAL},
	{docontinue, "continue", BT_POSIXSPECIAL},
	{doreturn, "return", BT_POSIXSPECIAL},
	{doexec, "exec", BT_POSIXSPECIAL | BT_NOKANJIFGET},
#ifndef	MINIMUMSHELL
	{dologin, "login", 0},
	{dologout, "logout", 0},
#endif
	{doeval, "eval", BT_POSIXSPECIAL | BT_NOKANJIFGET},
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
#ifndef	MINIMUMSHELL
	{dosource, "source", 0},
#endif
	{doexport, "export", BT_POSIXSPECIAL},
	{doreadonly, "readonly", BT_POSIXSPECIAL},
	{dotimes, "times", BT_POSIXSPECIAL},
	{dowait, "wait", 0},
	{doumask, "umask", 0},
	{doulimit, "ulimit", 0},
	{dotrap, "trap", BT_POSIXSPECIAL},
#if	!MSDOS && !defined (NOJOB)
	{dojobs, "jobs", 0},
	{dofg, "fg", 0},
	{dobg, "bg", 0},
	{dodisown, "disown", 0},
#endif
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
	{dodtype, "dtype", 0},
#endif
#ifndef	NOALIAS
	{doalias, "alias", 0},
	{dounalias, "unalias", BT_NOGLOB},
#endif
	{doecho, "echo", 0},
#ifndef	MINIMUMSHELL
	{dokill, "kill", 0},
	{dotest, "test", 0},
	{dotest, "[", 0},
#endif
#ifndef	NOPOSIXUTIL
	{donull, "true", 0},
	{dofalse, "false", 0},
	{docommand, "command", BT_NOKANJIFGET},
	{dogetopts, "getopts", 0},
	{donewgrp, "newgrp", BT_RESTRICT},
# if	0
/* exists in FD original builtin */
	{dofc, "fc", 0},
# endif
#endif
#ifndef	MINIMUMSHELL
	{dopushd, "pushd", BT_RESTRICT},
	{dopopd, "popd", 0},
	{dodirs, "dirs", 0},
#endif
#ifdef	FD
	{dofd, "fd", 0},
#endif
};
#define	SHBUILTINSIZ	((int)(sizeof(shbuiltinlist) / sizeof(shbuiltintable)))
statementtable statementlist[] = {
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
	"PATH", "PS1", "PS2", "IFS",
#if	!MSDOS && !defined (MINIMUMSHELL)
	"MAILCHECK",
#endif
};
#define	PRIMALVARSIZ	((int)(sizeof(primalvar) / sizeof(char *)))
static char getflags[] = "xnvtsierkuhfaCbm";
static char setflags[] = "xnvt\0\0e\0kuhfaCbm";
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
#ifndef	MINIMUMSHELL
	"noclobber",
#endif
#if	!MSDOS && !defined (NOJOB)
	"notify",
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
#ifndef	MINIMUMSHELL
	&noclobber,
#endif
#if	!MSDOS && !defined (NOJOB)
	&bgnotify,
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
signaltable signallist[] = {
#ifdef	SIGHUP
	{SIGHUP, trap_hup, "HUP", MESHUP, TR_TERM},
#endif
#ifdef	SIGINT
	{SIGINT, trap_int, "INT", MESINT, TR_TERM | TR_BLOCK | TR_READBL},
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
	{SIGALRM, trap_alrm, "ALRM", MESALRM, TR_TERM | TR_READBL},
#endif
#ifdef	SIGTERM
	{SIGTERM, trap_term, "TERM", MESTERM, TR_TERM | TR_BLOCK | TR_READBL},
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
	{-1, NULL, NULL, NULL, 0}
};
static int trapmode[NSIG];
static char *trapcomm[NSIG];
static sigarg_t (*oldsigfunc[NSIG])__P_((sigfnc_t));


#ifndef	FD
# if	!MSDOS || !defined (MINIMUMSHELL)
time_t time2(VOID_A)
{
#  if	MSDOS
	struct timeb buffer;

	ftime(&buffer);
	return((time_t)(buffer.time));
#  else
	struct timeval t_val;
	struct timezone tz;

	gettimeofday2(&t_val, &tz);
	return((time_t)(t_val.tv_sec));
#  endif
}
# endif	/* !MSDOS || !MINIMUMSHELL */

static int NEAR genrand(max)
int max;
{
# if	MSDOS && defined (MINIMUMSHELL)
	struct SREGS sreg;
	__dpmi_regs reg;
# endif
	static long last = -1L;
	time_t now;

	if (last < 0L) {
# if	!MSDOS || !defined (MINIMUMSHELL)
		now = time2();
# else
		reg.x.ax = 0x2a00;
		now = -1L;
		if (int21call(&reg, &sreg) >= 0)
			now ^= ((long)reg.x.cx << 16) | reg.x.dx;
		reg.x.ax = 0x2c00;
		if (int21call(&reg, &sreg) >= 0)
			now ^= ((long)reg.x.cx << 16) | reg.x.dx;
# endif
		last = ((now & 0xff) << 16) + (now & ~0xff) + getpid();
	}

	do {
		last = last * (u_long)1103515245 + 12345;
	} while (last < 0L);

	return((last / 65537L) % max);
}

static char *NEAR genrandname(buf, len)
char *buf;
int len;
{
	static char seq[] = {
# if	MSDOS
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
		'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
		'U', 'V', 'W', 'X', 'Y', 'Z', '_'
# else
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
		'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
		'u', 'v', 'w', 'x', 'y', 'z', '_'
# endif
	};
	int i, j, c;

	if (!buf) {
		for (i = 0; i < sizeof(seq) / sizeof(char); i++) {
			j = genrand(sizeof(seq) / sizeof(char));
			c = seq[i];
			seq[i] = seq[j];
			seq[j] = c;
		}
	}
	else {
		for (i = 0; i < len; i++) {
			j = genrand(sizeof(seq) / sizeof(char));
			buf[i] = seq[j];
		}
		buf[i] = '\0';
	}
	return(buf);
}

static int NEAR mktmpfile(file)
char *file;
{
	char *cp, path[MAXPATHLEN];
	int fd, n, len;

	strcpy(path, deftmpdir);
	cp = strcatdelim(path);
	n = sizeof(TMPPREFIX) - 1;
	strncpy(cp, TMPPREFIX, n);
	len = sizeof(path) - 1 - (cp - path);
	if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
	len -= n;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(cp + n, len);
		fd = Xopen(path, O_BINARY | O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd >= 0) {
			strcpy(file, path);
			return(fd);
		}
		if (errno != EEXIST) break;
	}
	return(-1);
}

static int NEAR rmtmpfile(file)
char *file;
{
	if (Xunlink(file) < 0 && errno != ENOENT) return(-1);
	return(0);
}

static char **NEAR duplvar(var, margin)
char **var;
int margin;
{
	char **dupl;
	int i, n;

	if (margin < 0) {
		if (!var) return(NULL);
		margin = 0;
	}
	n = countvar(var);
	dupl = (char **)malloc2((n + margin + 1) * sizeof(char *));
	for (i = 0; i < n; i++) dupl[i] = strdup2(var[i]);
	dupl[i] = NULL;
	return(dupl);
}

# ifdef	DJGPP
int dos_putpath(path, offset)
char *path;
int offset;
{
	int i;

	i = strlen(path) + 1;
	dosmemput(path, i, __tb + offset);
	return(i);
}
# endif	/* !DJGPP */

# ifdef	LSI_C
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
# endif	/* LSI_C */

# if	MSDOS
#  ifndef	DJGPP
/*ARGSUSED*/
int Xmkdir(path, mode)
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

int int21call(regp, sregp)
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
# endif	/* MSDOS */

int setenv2(name, value)
char *name, *value;
{
	char *cp;
	int len;

	len = strlen(name);
	if (!value) return(unset(name, len));
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
#if	defined (FD) && defined (SIGALRM)
	noalrm++;
#endif
	for (i = 0; signallist[i].sig >= 0; i++) {
		sig = signallist[i].sig;
		if (signallist[i].flags & TR_BLOCK);
		else if (signallist[i].flags & TR_READBL);
		else if ((trapmode[sig] & TR_STAT) != TR_TRAP) continue;
		else if ((signallist[i].flags & TR_STAT) == TR_STOP) continue;

		signal(sig, (sigcst_t)(signallist[i].func));
	}
}

static VOID NEAR resetsignal(forced)
int forced;
{
#if	!MSDOS && defined (NOJOB)
	p_id_t tmp;
#endif
	int i, duperrno;

	duperrno = errno;
#if	!MSDOS
# ifdef	NOJOB
	do {
		tmp = Xwait3(NULL, WNOHANG | WUNTRACED, NULL);
	} while (tmp > 0L || (tmp < 0L && errno == EINTR));
# else
	checkjob(1);
	stopped = 0;
# endif
#endif	/* !MSDOS */
	if (havetty() && interrupted && interactive && !nottyout) {
		fflush(stdout);
		fputc('\n', stderr);
		fflush(stderr);
	}
	interrupted = 0;
	if (setsigflag) {
		if (forced) setsigflag = 1;
		if (!(--setsigflag)) {
#if	defined (FD) && defined (SIGALRM)
			noalrm--;
#endif
			for (i = 0; i < NSIG; i++)
				if (oldsigfunc[i] != SIG_ERR)
					signal(i, oldsigfunc[i]);
#if	!MSDOS
			Xsigsetmask(oldsigmask);
#endif
		}
	}
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

	if (!havetty()) return;
	for (i = 0; i < NSIG; i++) {
		if (!(trapmode[i] & TR_CATCH)) continue;
		trapmode[i] &= ~TR_CATCH;
		if (!trapcomm[i] || !*(trapcomm[i])) continue;
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
	int i, trapped, flags, duperrno;

	for (i = 0; signallist[i].sig >= 0; i++)
		if (sig == signallist[i].sig) break;
	if (signallist[i].sig < 0) {
		signal(sig, SIG_DFL);
		return(0);
	}

	duperrno = errno;
	trapped = 0;
	flags = signallist[i].flags;
	if (readtrap && !interactive && (flags & TR_READBL)) flags ^= TR_BLOCK;

	if (mypid != orgpid) {
		if ((flags & TR_BLOCK) && (flags & TR_STAT) == TR_TERM)
			trapped = -1;
	}
	else if ((trapmode[sig] & TR_STAT) == TR_TRAP) trapped = 1;
	else if (flags & TR_BLOCK) {
#ifdef	SIGINT
		if (readtrap && sig == SIGINT) {
# if	!MSDOS && defined (TIOCSTI)
			u_char c;

			c = '\n';
			ioctl(STDIN_FILENO, TIOCSTI, &c);
# endif
			readtrap = 0;
		}
#endif	/* SIGINT */
	}
	else if	((flags & TR_STAT) == TR_TERM) trapped = -1;

	if (havetty() && trapped > 0) {
		trapmode[sig] |= TR_CATCH;
		if (trapok) exectrapcomm();
	}
	else if (trapped < 0 && *(signallist[i].mes)) {
		fputs(signallist[i].mes, stderr);
		fputc('\n', stderr);
		fflush(stderr);
	}

	if (trapped < 0) Xexit2(sig + 128);

	if (signallist[i].func) signal(sig, (sigcst_t)(signallist[i].func));
	errno = duperrno;
	return(trapped);
}

#ifdef	SIGHUP
static int trap_hup(VOID_A)
{
	if (!trap_common(SIGHUP)) {
# if	!MSDOS && !defined (NOJOB)
		if (havetty()) killjob();
# endif
		if (oldsigfunc[SIGHUP] && oldsigfunc[SIGHUP] != SIG_ERR
		&& oldsigfunc[SIGHUP] != SIG_DFL
		&& oldsigfunc[SIGHUP] != SIG_IGN) {
# ifdef	SIGFNCINT
			(*oldsigfunc[SIGHUP])(SIGHUP);
# else
			(*oldsigfunc[SIGHUP])();
# endif
		}
		Xexit2(RET_FAIL);
	}
	return(0);
}
#endif	/* SIGHUP */

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
# if	!MSDOS && defined (FD)
	suspended = 1;
# endif
	return(trap_common(SIGCONT));
}
#endif

#ifdef	SIGCHLD
static int trap_chld(VOID_A)
{
# if	!MSDOS && !defined (NOJOB)
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

	if (noexit > 0) {
		resetsignal(1);
		return;
	}
	duperrno = errno;
	if (havetty()) {
		exectrapcomm();
		if (!noexit && (trapmode[0] & TR_STAT) == TR_TRAP) {
			trapmode[0] = 0;
			if (trapcomm[0] && *(trapcomm[0]))
				_dosystem(trapcomm[0]);
		}
#if	!MSDOS && !defined (NOJOB)
		if (oldttypgrp >= 0L) settcpgrp(ttyio, oldttypgrp);
#endif
	}
	resetsignal(1);
	freefunc(shellfunc);
	shellfunc = NULL;
#ifdef	DEBUG
	if (definput >= 0) {
		if (definput != ttyio) safeclose(definput);
		definput = -1;
	}
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
# ifndef	MINIMUMSHELL
	freevar(dirstack);
	dirstack = NULL;
# endif	/* !MINIMUMSHELL */
# ifndef	NOALIAS
	freealias(shellalias);
	shellalias = NULL;
# endif
# ifndef	_NOUSEHASH
	searchhash(NULL, NULL, NULL);
# endif
# if	!MSDOS && !defined (MINIMUMSHELL)
	checkmail(1);
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

VOID Xexit2(n)
int n;
{
	if (havetty()) {
#ifdef	FD
		putterm(t_normal);
		endterm();
		inittty(1);
		keyflush();
#endif	/* !FD */
		prepareexit(0);

#ifdef	DEBUG
		if (mypid == orgpid) {
# if	!MSDOS && defined (FD)
			freeterment();
# endif
			if (ttyout && ttyout != stderr) fclose(ttyout);
			else if (ttyio >= 0) close(ttyio);
			muntrace();
		}
#endif
	}

	exit(n);
}

static VOID NEAR syntaxerror(s)
char *s;
{
	if (syntaxerrno <= 0 || syntaxerrno >= SYNTAXERRSIZ) return;
#ifndef	BASHSTYLE
	/* bash shows its name, even in interective shell */
	if (interactive);
	else
#endif
	if (argvar && argvar[0]) {
		kanjifputs(argvar[0], stderr);
		fputs(": ", stderr);
	}
	if (s) {
		if (!*s || syntaxerrno == ER_UNEXPNL)
			fputs("syntax error", stderr);
		else kanjifputs(s, stderr);
		fputs(": ", stderr);
	}
	fputs(syntaxerrstr[syntaxerrno], stderr);
	fputc('\n', stderr);
	fflush(stderr);
	ret_status = RET_SYNTAXERR;
	if (errorexit) {
#if	!MSDOS && !defined (NOJOB)
		if (loginshell && interactive_io) killjob();
#endif
		Xexit2(RET_SYNTAXERR);
	}
	safeexit();
}

/*ARGSUSED*/
VOID execerror(s, n, noexit)
char *s;
int n, noexit;
{
	if (n == ER_BADSUBST
	&& (execerrno == ER_ISREADONLY || execerrno == ER_PARAMNOTSET)) return;

	if (!n || n >= EXECERRSIZ) return;
#ifndef	BASHSTYLE
	/* bash shows its name, even in interective shell */
	if (interactive);
	else
#endif
	if (argvar && argvar[0]) {
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
	/* bash does not break any statement on error */
	if (noexit < 2) breaklevel = loopdepth;
#endif
#if	MSDOS
	/* child process fake */
	if (childdepth) return;
#endif

	if (noexit < 0) safeexit();
	else if (noexit);
#if	!defined (BASHSTYLE) || defined (STRICTPOSIX)
	/* bash does not exit on error, in non interactive shell */
	else if (isshellbuiltin) safeexit();
#endif
}

VOID doperror(command, s)
char *command, *s;
{
	int duperrno;

	duperrno = errno;
	if (errno < 0) return;
	if (!command) command = (argvar && argvar[0]) ? argvar[0] : shellname;
	kanjifputs(command, stderr);
	fputs(": ", stderr);
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

#ifndef	BASHSTYLE
	/* bash does not break any statement on error */
	breaklevel = loopdepth;
#endif
#if	MSDOS
	/* child process fake */
	if (childdepth) return;
#endif

#if	!defined (BASHSTYLE) || defined (STRICTPOSIX)
	/* bash does not exit on error, in non interactive shell */
	if (isshellbuiltin) safeexit();
#endif
}

int isnumeric(s)
char *s;
{
	int i, n;

	if (!*s) return(-1);
	for (i = n = 0; s[i]; i++) {
		if (!isdigit(s[i])) {
#ifndef	BASHSTYLE
	/* bash always treats non numeric character as error */
			if (i) break;
#endif
			return(-1);
		}
#ifdef	DOS16
		if (n > 3276 || (n == 3276 && s[i] > '7')) return(-1);
#endif
		n = n * 10 + (s[i] - '0');
	}
	return(n);
}

VOID fputlong(n, fp)
long n;
FILE *fp;
{
	char buf[MAXLONGWIDTH + 1];

	ascnumeric(buf, n, 0, sizeof(buf) - 1);
	fputs(buf, fp);
}

VOID fputstr(s, max, fp)
char *s;
int max;
FILE *fp;
{
	int i;

	for (i = 0; s[i] && i < max; i++) fputc(s[i], fp);
	while (i++ < max) fputc(' ', fp);
}

static VOID NEAR fputoctal(n, max, fp)
int n, max;
FILE *fp;
{
	char buf[(64 / 3) + 1 + 1];
	int i;

	if (max > sizeof(buf) - 1) max = sizeof(buf) - 1;
	for (i = 0; i < max; i++) {
		buf[i] = '0' + (n & 7);
		n >>= 3;
	}
	while (--i >= 0) fputc(buf[i], fp);
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

VOID dispsignal(sig, width, fp)
int sig, width;
FILE *fp;
{
	char buf[80 + 1];
	int i;

	for (i = 0; signallist[i].sig >= 0; i++)
		if (sig == signallist[i].sig) break;
	if (signallist[i].sig < 0) {
		fputs("Signal ", fp);
		if ((width -= 7) > 0) i = -width;
		else {
			i = 0;
			width = sizeof(buf) - 1;
		}
		ascnumeric(buf, sig, i, width);
		fputs(buf, fp);
	}
	else if (!width) fputs(signallist[i].mes, fp);
	else fputstr(signallist[i].mes, width, fp);
}

int waitjob(pid, wp, opt)
p_id_t pid;
wait_t *wp;
int opt;
{
# ifndef	NOJOB
	int i, j, sig;
# endif
	wait_t w;
	p_id_t tmp;
	int ret;

	for (;;) {
# ifndef	JOBTEST
		tmp = (pid < 0L)
			? Xwait3(&w, opt, NULL) : Xwait4(pid, &w, opt, NULL);
# else	/* JOBTEST */
		tmp = Xwait3(&w, opt, NULL);
		if (pid >= 0L && tmp >= 0L && pid != tmp) {
			if ((i = searchjob(tmp, &j)) < 0) continue;
			sig = joblist[i].stats[j];
			joblist[i].stats[j] = getwaitsig(w);
#  if	defined (SIGCONT) && (defined (SIGTTIN) || defined (SIGTTOU))
			if (sig == joblist[i].stats[j]) sig = 0;
#   ifdef	SIGTTIN
			else if (joblist[i].stats[j] == SIGTTIN);
#   endif
#   ifdef	SIGTTOU
			else if (joblist[i].stats[j] == SIGTTOU);
#   endif
			else sig = 0;

			if (sig) {
				usleep(100000L);
				kill(tmp, SIGCONT);
			}
#  endif	/* SIGCONT  && (SIGTTIN || SIGTTOU) */
			continue;
		}
# endif	/* JOBTEST */
		if (tmp >= 0L || errno != EINTR) break;
	}

	if (!tmp) return(0);
	else if (tmp < 0L) {
		if (pid < 0L || errno != ECHILD) return(-1);
		ret = -1;
# ifndef	NOJOB
		sig = -1;
# endif
	}
	else {
		ret = (pid < 0L || tmp == pid) ? 1 : 0;
# ifndef	NOJOB
		sig = getwaitsig(w);
# endif
	}

# ifndef	NOJOB
	if (ret >= 0 && (i = searchjob(tmp, &j)) >= 0
	&& sig != joblist[i].stats[j]) {
		joblist[i].stats[j] = sig;
		if (jobok && interactive && !nottyout
		&& pid != tmp && WIFSTOPPED(w))
			dispjob(i, stderr);
	}
# endif
	if (wp) *wp = w;
	return(ret);
}

static VOID NEAR setstopsig(valid)
int valid;
{
	int i;

	for (i = 0; signallist[i].sig >= 0; i++) {
		if (signallist[i].flags & TR_BLOCK);
		else if ((signallist[i].flags & TR_STAT) != TR_STOP) continue;

		if (valid) signal(signallist[i].sig, SIG_DFL);
		else signal(signallist[i].sig, (sigcst_t)(signallist[i].func));
	}
}

/*ARGSUSED*/
static p_id_t NEAR makechild(tty, ppid)
int tty;
p_id_t ppid;
{
	p_id_t pid;
	int stop;

	if ((pid = fork()) < 0L) return(-1L);
# ifdef	DEBUG
	if (!pid) {
		extern VOID (*__free_hook) __P_((VOID_P));
		extern VOID_P (*__malloc_hook) __P_((ALLOC_T));
		extern VOID_P (*__realloc_hook) __P_((VOID_P, ALLOC_T));

		__free_hook = NULL;
		__malloc_hook = NULL;
		__realloc_hook = NULL;
	}
# endif	/* DEBUG */
	if (!pid) {
# ifdef	SIGCHLD
		sigmask_t mask;

		memcpy((char *)&mask, (char *)&oldsigmask, sizeof(sigmask_t));
		Xsigdelset(mask, SIGCHLD);
		Xsigsetmask(mask);
# else
		Xsigsetmask(oldsigmask);
# endif	/* SIGCHLD */
		mypid = getpid();
		stop = 1;
# ifdef	NOJOB
		if (loginshell) stop = 0;
# else
		if (jobok && (!tty || childpgrp == orgpgrp)) stop = 0;
# endif
		setstopsig(stop);
	}

# ifndef	NOJOB
	if (childpgrp < 0L)
		childpgrp = (ppid >= 0L) ? ppid : (pid) ? pid : mypid;
	if (pid) {
		if (jobok) setpgroup(pid, childpgrp);
		if (tty && ttypgrp >= 0L) ttypgrp = childpgrp;
	}
	else {
		if (jobok && setpgroup(mypid, childpgrp) < 0) {
			doperror(NULL, "fatal error");
			prepareexit(-1);
			Xexit(RET_FATALERR);
		}
		if (tty) gettermio(childpgrp);
	}
# endif	/* !NOJOB */

	return(pid);
}

int waitchild(pid, trp)
p_id_t pid;
syntaxtree *trp;
{
# ifndef	NOJOB
#  ifdef	USESGTTY
	int lflag;
#  endif
	termioctl_t tty;
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
#  ifdef	SIGCONT
		if (loginshell) kill(pid, SIGCONT);
#  endif
# else	/* !NOJOB */
#  ifdef	BASHBUG
	/* bash cannot be suspended with +m mode */
		if (!jobok) continue;
#  endif
		if (mypid != orgpgrp) continue;

		trapok = 0;
		ret = WSTOPSIG(w);
		gettermio(orgpgrp);
		prevjob = lastjob;
		lastjob = stackjob(pid, ret, trp);
		breaklevel = loopdepth;
		stopped = 1;
		if (!jobok) return(RET_SUCCESS);
		if (tioctl(ttyio, REQGETP, &tty) >= 0) {
			if (joblist[lastjob].tty) free(joblist[lastjob].tty);
			joblist[lastjob].tty =
				(termioctl_t *)malloc2(sizeof(tty));
			memcpy((char *)(joblist[lastjob].tty), (char *)&tty,
				sizeof(tty));
#  ifndef	FD
#   ifdef	USESGTTY
			tty.sg_flags |= ECHO | CRMOD;
			tty.sg_flags &= ~(RAW | CBREAK | XTABS);
#   else
			tty.c_lflag |= ISIG | ICANON | IEXTEN
				| ECHO | ECHOE | ECHOCTL | ECHOKE;
			tty.c_lflag &= ~(PENDIN | ECHONL);
			tty.c_iflag |= BRKINT | IXON | ICRNL;
			tty.c_iflag &= ~(IGNBRK | ISTRIP);
			tty.c_oflag |= OPOST | ONLCR;
			tty.c_oflag &= ~(OCRNL | ONOCR | ONLRET | TAB3);
#   endif
			tioctl(ttyio, REQSETP, &tty);
#  endif	/* !FD */
		}
#  ifdef	USESGTTY
		if (ioctl(ttyio, TIOCLGET, &lflag) >= 0) {
			if (joblist[lastjob].ttyflag)
				free(joblist[lastjob].ttyflag);
			joblist[lastjob].ttyflag =
				(int *)malloc2(sizeof(lflag));
			memcpy((char *)(joblist[lastjob].ttyflag),
				(char *)&lflag, sizeof(lflag));
#   ifndef	!FD
			lflag |= LPASS8 | LCRTBS | LCRTERA | LCRTKIL | LCTLECH;
			lflag &= ~(LLITOUT | LPENDIN);
			ioctl(ttyio, TIOCLSET, &lflag);
#   endif
		}
#  endif	/* USESGTTY */
#  ifdef	FD
		stdiomode();
#  endif	/* FD */
		if (interactive && !nottyout) dispjob(lastjob, stderr);
		return(RET_SUCCESS);
# endif	/* !NOJOB */
	}
	trapok = 0;

# ifndef	NOJOB
	if (mypid == orgpgrp) {
		gettermio(orgpgrp);
		childpgrp = -1L;
	}
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
# endif	/* !NOJOB */
	return((int)ret);
}
#endif	/* !MSDOS */

static VOID NEAR safeclose(fd)
int fd;
{
	int duperrno;

	if (fd < 0) return;
	duperrno = errno;
	if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
		Xclose(fd);
	errno = duperrno;
}

static VOID NEAR safefclose(fp)
FILE *fp;
{
	int fd, duperrno;

	if (!fp) return;
	duperrno = errno;
	fd = fileno(fp);
	if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
		Xfclose(fp);
	errno = duperrno;
}

static VOID NEAR safermtmpfile(file)
char *file;
{
	int duperrno;

	duperrno = errno;
	rmtmpfile(file);
	errno = duperrno;
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
	flags = (u_long)0;
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
		if (j < FLAGSSIZ) flags |= ((u_long)1 << j);
		else if (argv[1][0] == '-') {
			execerror(argv[1], ER_BADOPTIONS, 0);
			return(-1);
		}
	}
	for (j = 0; j < FLAGSSIZ; j++) {
		if (flags & (u_long)1)
			*(setvals[j]) = (argv[1][0] == '-') ? 1 : 0;
		flags >>= 1;
	}

	return(com + 2);
}

static ALLOC_T NEAR c_allocsize(n)
int n;
{
	ALLOC_T size;

	n++;
	for (size = BUFUNIT; size < n; size *= 2);
	return(size);
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

static char *NEAR readline(fd)
int fd;
{
	char *cp;
	ALLOC_T i, size;
	int c;

	cp = c_realloc(NULL, 0, &size);
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
		cp = c_realloc(cp, i, &size);
		cp[i] = c;
	}
#if	MSDOS
	if (i > 0 && cp[i - 1] == '\r') i--;
#endif
	cp[i++] = '\0';
	return(realloc2(cp, i));
}

static char *NEAR readfile(fd, lenp)
int fd;
ALLOC_T *lenp;
{
	char *cp;
	ALLOC_T i, size;
	int c;

	cp = c_realloc(NULL, 0, &size);
	for (i = 0; (c = readchar(fd)) != READ_EOF; i++) {
		if (c < 0) {
			free(cp);
			return(NULL);
		}
		cp = c_realloc(cp, i, &size);
		cp[i] = c;
	}
	cp[i] = '\0';
	if (lenp) *lenp = i;
	return(realloc2(cp, i + 1));
}

char *evalvararg(arg, stripq, backq, quoted, stripqlater, noexit)
char *arg;
int stripq, backq, quoted, stripqlater, noexit;
{
	char *tmp;

	if ((tmp = evalarg(arg, stripq, backq, quoted))) {
		if (stripqlater) stripquote(tmp, 1);
		return(tmp);
	}
#if	defined (BASHSTYLE) && defined (STRICTPOSIX)
	if (!noexit) noexit = -1;
#endif
	if (*arg) execerror(arg, ER_BADSUBST, noexit);
	return(NULL);
}

static heredoc_t *NEAR newheredoc(eof, ignoretab)
char *eof;
int ignoretab;
{
	heredoc_t *new;
	char *cp, path[MAXPATHLEN];
	int fd, flags;

	flags = (ignoretab) ? HD_IGNORETAB : 0;

	cp = strdup2(eof);
	if (stripquote(cp, 1)) flags |= HD_QUOTED;
#ifndef	BASHSTYLE
	/* bash allows no variables as the EOF identifier */
	free(cp);
	if (!(cp = evalvararg(eof, 0, 0, '\0', 1, 0))) {
		errno = -1;
		return(NULL);
	}
#endif

	if ((fd = newdup(mktmpfile(path))) < 0) return(NULL);
	new = (heredoc_t *)malloc2(sizeof(heredoc_t));
	new -> eof = cp;
	new -> filename = strdup2(path);
	new -> buf = NULL;
	new -> fd = fd;
	new -> flags = flags;

	return(new);
}

static VOID NEAR freeheredoc(hdp, undo, nown)
heredoc_t *hdp;
int undo, nown;
{
	if (!hdp) return;
	if (hdp -> eof) free(hdp -> eof);
	if (hdp -> filename) {
		if (undo && !nown) safermtmpfile(hdp -> filename);
		free(hdp -> filename);
	}
	if (hdp -> buf) free(hdp -> buf);
	if (undo) safeclose(hdp -> fd);
	free(hdp);
}

static redirectlist *NEAR newrlist(fd, filename, type, next)
int fd;
char *filename;
int type;
redirectlist *next;
{
	redirectlist *new;

	new = (redirectlist *)malloc2(sizeof(redirectlist));
	new -> fd = fd;
	new -> filename = filename;
	new -> type = (u_char)type;
	new -> new = new -> old = -1;
#if	defined (FD) && !defined (_NODOSDRIVE)
	new -> fakepipe = NULL;
	new -> dosfd = -1;
#endif
	new -> next = next;
	return(new);
}

static int NEAR freerlist(rp, undo, nown)
redirectlist *rp;
int undo, nown;
{
	int ret;

	ret = 0;
	if (!rp) return(0);
#if	defined (FD) && !defined (_NODOSDRIVE)
	if (!undo && rp -> fakepipe) {
		errno = EBADF;
		doperror(NULL, rp -> filename);
		ret = -1;
		undo = 1;
	}
#endif
	if (rp -> next) ret = freerlist(rp -> next, undo, nown);
	if (ret < 0) undo = 1;

	if (undo && rp -> old >= 0 && rp -> old != rp -> fd) {
		safeclose(rp -> fd);
		Xdup2(rp -> old, rp -> fd);
		safeclose(rp -> old);
	}

	if (rp -> type & MD_HEREDOC) {
		if (undo) closepipe(rp -> new);
		freeheredoc((heredoc_t *)(rp -> filename), undo, nown);
	}
	else {
		if (undo && rp -> new >= 0 && !(rp -> type & MD_FILEDESC))
			safeclose(rp -> new);
		if (rp -> filename) free(rp -> filename);
	}
#if	defined (FD) && !defined (_NODOSDRIVE)
	closepseudofd(rp);
#endif
	free(rp);
	return(ret);
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

static VOID NEAR freecomm(comm, nown)
command_t *comm;
int nown;
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
	if (comm -> redp) freerlist(comm -> redp, 1, nown);
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
	new -> type = OP_NONE;
	if (parent) {
		new -> cont = (parent -> cont & CN_INHR);
		new -> flags = 0;
	}
	else {
		new -> cont = 0;
		new -> flags = ST_TOP;
	}
	return(new);
}

VOID freestree(trp)
syntaxtree *trp;
{
	int duperrno;

	if (!trp) return;

	duperrno = errno;
	if (trp -> comm) {
		if (!(trp -> flags & ST_NODE))
			freecomm(trp -> comm, trp -> flags & ST_NOWN);
		else {
			freestree((syntaxtree *)(trp -> comm));
			free(trp -> comm);
		}
		trp -> comm = NULL;
	}

	if (trp -> next) {
#ifndef	MINIMUMSHELL
		if (trp -> flags & ST_BUSY) {
			redirectlist *rp;

			rp = (redirectlist *)(trp -> next);
			free(rp -> filename);
			freestree((syntaxtree *)(rp -> next));
			free(rp -> next);
		}
		else
#endif
		if (!(trp -> cont & (CN_QUOT | CN_META)))
			freestree(trp -> next);
		else if (((redirectlist *)(trp -> next)) -> filename)
			free(((redirectlist *)(trp -> next)) -> filename);
		free(trp -> next);
		trp -> next = NULL;
	}
	trp -> type = OP_NONE;
	trp -> cont = trp -> flags = 0;
	errno = duperrno;
}

static syntaxtree *NEAR eldeststree(trp)
syntaxtree *trp;
{
	syntaxtree *tmptr;

	while (trp) {
		tmptr = getparent(trp);
		if (!(trp -> flags & ST_NEXT)) return(tmptr);
		trp = tmptr;
	}
	return(NULL);
}

syntaxtree *parentstree(trp)
syntaxtree *trp;
{
	while ((trp = eldeststree(trp)))
		if (!(trp -> flags & ST_NODE) && isstatement(trp -> comm))
			return(trp);
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
	trp -> type = OP_NONE;
	trp -> flags &= ~ST_NODE;
	return(new);
}

static syntaxtree *NEAR skipfuncbody(trp)
syntaxtree *trp;
{
	syntaxtree *tmptr;

	if (trp -> flags & ST_BUSY) return(trp);
	tmptr = statementcheck(trp -> next, SM_STATEMENT);
	if (getstatid(tmptr) == SM_LPAREN - 1) trp = trp -> next;
	return(trp);
}

static syntaxtree *NEAR insertstree(trp, parent, type)
syntaxtree *trp, *parent;
int type;
{
	syntaxtree *new, *tmptr;
	int i, l1, l2;

	for (i = 0; i < OPELISTSIZ; i++) if (type == opelist[i].op) break;
	l1 = (i < OPELISTSIZ) ? opelist[i].level : 0;
	if (!parent) l2 = 0;
	else {
		for (i = 0; i < OPELISTSIZ; i++)
			if (parent -> type == opelist[i].op) break;
		l2 = (i < OPELISTSIZ) ? opelist[i].level : 0;
	}

	if (!l1);
#ifndef	BASHSTYLE
	/* bash does not allow the format like as "foo | ; bar" */
	else if (parent && isoppipe(parent) && l1 > l2);
#endif
#ifndef	MINIMUMSHELL
	else if (type == OP_NOT && (!l2 || l1 < l2));
#endif
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

	/* && or || */
	if (l1 == 3 && l2 == 3) l2--;

	if (!l1 || !l2);
	else if (l1 < l2) {
		new = newstree(trp);
		new -> comm = trp -> comm;
		new -> next = trp -> next;
		new -> type = trp -> type;
		new -> cont = trp -> cont;
		if (trp -> flags & ST_NODE) {
			new -> flags = trp -> flags;
			trp -> flags = 0;
		}
		trp -> comm = (command_t *)new;
		trp -> type = OP_NONE;
		trp -> flags |= ST_NODE;
		trp = new;
	}
	else if (l1 > l2) {
		if (!(tmptr = eldeststree(parent))) {
			while (hasparent(trp)) trp = trp -> parent;
			new = newstree(trp);
			new -> comm = trp -> comm;
			new -> next = trp -> next;
			new -> type = trp -> type;
			new -> cont = trp -> cont;
			new -> flags = trp -> flags;
			if (trp -> next) (trp -> next) -> parent = new;
			trp -> comm = (command_t *)new;
			trp -> type = type;
			trp -> cont = 0;
			trp -> flags = ST_NODE;
		}
		else if (tmptr -> flags & ST_NODE)
			trp = insertstree(tmptr, eldeststree(tmptr), type);
		else if (!isstatement(tmptr -> comm))
			trp = insertstree(trp, tmptr, type);
		else {
			new = newstree(tmptr);
			new -> comm = (command_t *)((tmptr -> comm) -> argv);
			new -> next = trp -> next;
			new -> type = trp -> type;
			new -> cont = trp -> cont;
			new -> flags = trp -> flags;
			(tmptr -> comm) -> argv = (char **)new;
			trp = new;
			trp -> type = type;
			trp -> cont = 0;
			trp -> flags = ST_NODE;
		}
	}

	return(trp);
}

static syntaxtree *NEAR linkstree(trp, type)
syntaxtree *trp;
int type;
{
	syntaxtree *new, *tmptr;
	int cont;

	cont = ((trp -> cont) & CN_INHR);
	if (trp && isstatement(trp -> comm)
	&& (trp -> comm) -> id == SM_STATEMENT
	&& (tmptr = statementbody(trp))
	&& getstatid(tmptr) == SM_LPAREN - 1)
		trp = getparent(trp);
	tmptr = (trp -> flags & ST_NEXT) ? getparent(trp) : NULL;
	trp = insertstree(trp, tmptr, type);

	trp = skipfuncbody(trp);
	new = trp -> next = newstree(trp);
	new -> cont = cont;
	new -> flags = ST_NEXT;
	trp -> type = type;
	return(new);
}

static VOID NEAR nownstree(trp)
syntaxtree *trp;
{
	if (!trp) return;

	if (!(trp -> comm));
	else if (trp -> flags & ST_NODE)
		nownstree((syntaxtree *)(trp -> comm));
	else if (isstatement(trp -> comm)) nownstree(statementbody(trp));

	trp -> flags |= ST_NOWN;
	nownstree(trp -> next);
}

static int NEAR evalfiledesc(tok)
char *tok;
{
	int i, n, max;

	n = 0;
	if ((max = getdtablesize()) <= 0) max = NOFILE;
	for (i = 0; tok[i]; i++) {
		if (!isdigit(tok[i])) return(-1);
		n = n * 10 + tok[i] - '0';
		if (n > max) n = max;
	}
	return((i) ? n : -1);
}

static int NEAR isvalidfd(fd)
int fd;
{
#if	MSDOS
# ifdef	FD
	struct stat st;

	return((fstat(fd, &st)) ? -1 : 0);
# else
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x4400;
	reg.x.bx = fd;
	return(int21call(&reg, &sreg));
# endif
#else
	return(fcntl(fd, F_GETFD, NULL));
#endif
}

static int NEAR newdup(fd)
int fd;
{
	int n;

	if (fd < 0
	|| fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
		return(fd);
#if	defined (FD) && !defined (_NODOSDRIVE)
	if (fd >= DOSFDOFFSET) return(fd);
#endif
	if ((n = getdtablesize()) <= 0) n = NOFILE;

	for (n--; n > fd; n--) if (isvalidfd(n) < 0) break;
	if (n <= fd || Xdup2(fd, n) < 0) return(fd);
	Xclose(fd);
	return(n);
}

static int NEAR redmode(type)
int type;
{
	int mode;

	mode = O_TEXT;
	if (type & MD_READ) {
		if (type & MD_WRITE) {
			mode |= (O_RDWR | O_CREAT);
#ifndef	MINIMUMSHELL
			if (noclobber && !(type & MD_FORCED)) mode |= O_EXCL;
#endif
		}
		else mode |= O_RDONLY;
		if (type & MD_APPEND) mode |= O_APPEND;
	}
	else {
		mode |= O_WRONLY | O_CREAT;
		if (type & MD_APPEND) mode |= O_APPEND;
		else {
			mode |= O_TRUNC;
#ifndef	MINIMUMSHELL
			if (noclobber && !(type & MD_FORCED)) mode |= O_EXCL;
#endif
		}
	}
	return(mode);
}

static VOID NEAR closeredirect(rp)
redirectlist *rp;
{
	if (!rp) return;
	if (rp -> next) closeredirect(rp -> next);

	if (rp -> type & MD_WITHERR) {
		if (rp -> fd != STDERR_FILENO) {
			Xclose(STDERR_FILENO);
			Xdup2(rp -> fd, STDERR_FILENO);
			safeclose(rp -> fd);
		}
		rp -> fd = STDOUT_FILENO;
	}

	if (rp -> old >= 0 && rp -> old != rp -> fd) {
		safeclose(rp -> fd);
		Xdup2(rp -> old, rp -> fd);
		safeclose(rp -> old);
	}
	if (rp -> type & MD_HEREDOC) closepipe(rp -> new);
	else if (rp -> new >= 0 && !(rp -> type & MD_FILEDESC))
		safeclose(rp -> new);
#if	defined (FD) && !defined (_NODOSDRIVE)
	closepseudofd(rp);
#endif
	rp -> old = rp -> new = -1;
}

static heredoc_t *NEAR searchheredoc(trp, rm)
syntaxtree *trp;
int rm;
{
	redirectlist *rp;
	heredoc_t *hdp, *hit;

	if (!trp) return(NULL);
	if (rm && (trp -> flags & ST_NOWN)) return(NULL);

	if (!(trp -> comm));
	else if (trp -> flags & ST_NODE) {
		if ((hdp = searchheredoc((syntaxtree *)(trp -> comm), rm)))
			return(hdp);
	}
	else {
		if (isstatement(trp -> comm)) {
			if ((hdp = searchheredoc(statementbody(trp), rm)))
				return(hdp);
		}

		hit = NULL;
		if (!rm && !(trp -> cont & CN_HDOC));
		else for (rp = (trp -> comm) -> redp; rp; rp = rp -> next) {
			if (!(rp -> type & MD_HEREDOC)) continue;
			if (!(hdp = (heredoc_t *)(rp -> filename))) continue;

			if (!rm) {
				if (hdp -> fd >= 0) hit = hdp;
			}
			else if (hdp -> filename) {
				safermtmpfile(hdp -> filename);
				free(hdp -> filename);
				hdp -> filename = NULL;
			}
		}
		if (hit) return(hit);
	}

	if ((hdp = searchheredoc(trp -> next, rm))) return(hdp);
	if (!rm) trp -> cont &= ~CN_HDOC;
	return(NULL);
}

static int NEAR saveheredoc(s, trp)
char *s;
syntaxtree *trp;
{
	heredoc_t *hdp;
	char *cp;
	int i, len, ret;

	while (hasparent(trp)) trp = trp -> parent;
	if (!(hdp = searchheredoc(trp, 0))) return(0);

	cp = hdp -> buf;
	hdp -> buf = NULL;
	if (!cp) cp = strdup2(s);
	else {
		len = strlen(cp);
		if (s) {
			cp = realloc2(cp, len + strlen(s) + 1);
			strcpy(&(cp[len]), s);
		}
	}

	if (cp) {
		if (!(hdp -> flags & HD_QUOTED)) {
			for (i = 0; cp[i]; i++) {
				if (cp[i] == PMETA && !cp[i + 1]) break;
#ifdef	CODEEUC
				else if (isekana(cp, i)) i++;
#endif
				else if (iskanji1(cp, i)) i++;
			}
			if (cp[i]) {
				cp[i] = '\0';
				if (s) {
					hdp -> buf = cp;
					return(1);
				}
			}
		}
		i = 0;
		if (hdp -> flags & HD_IGNORETAB) while (cp[i] == '\t') i++;
		if (!strcmp(&(cp[i]), hdp -> eof)) {
			free(cp);
			cp = s = NULL;
		}
	}
#ifdef	FAKEUNINIT
	else i = 0;	/* fake for -Wuninitialized */
#endif

	ret = 1;
	if (cp) {
		len = strlen(cp);
		cp[len++] = '\n';
		len -= i;
		if (Xwrite(hdp -> fd, &(cp[i]), len) < len) ret = -1;
		free(cp);
	}
	if (!s) {
		safeclose(hdp -> fd);
		hdp -> fd = -1;
		searchheredoc(trp, 0);
	}

	return(ret);
}

static int NEAR openheredoc(hdp, old)
heredoc_t *hdp;
int old;
{
	char *cp;
	p_id_t pipein;
	ALLOC_T i, size;
	int c, fd, fdin, ret;

	fdin = newdup(Xopen(hdp -> filename, O_BINARY | O_RDONLY, 0666));
	if (fdin < 0) return(-1);
#if	MSDOS || defined (USEFAKEPIPE)
	if ((fd = openpipe(&pipein, old, 1)) < 0)
#else
	if ((fd = openpipe(&pipein, old, 1, interactive_io, mypid)) < 0)
#endif
	{
		safeclose(fdin);
		return(-1);
	}
#ifdef	DJGPP
	setmode(STDOUT_FILENO, O_TEXT);
#endif

	if (pipein > 0L) {
#if	!MSDOS && !defined (USEFAKEPIPE)
		if (waitchild(pipein, NULL) != RET_SUCCESS) {
			closepipe(fd);
			errno = -1;
			return(-1);
		}
#endif
		safeclose(fdin);
		return(fd);
	}

	cp = c_realloc(NULL, 0, &size);
	ret = RET_SUCCESS;

	i = 0;
	for (;;) {
		if ((c = readchar(fdin)) < 0) {
			free(cp);
			closepipe(fd);
			safeclose(fdin);
			return(-1);
		}
		if (c == READ_EOF || c == '\n') {
			if (i) {
				char *tmp;

				cp[i] = '\0';
				i = 0;

				if (ret != RET_SUCCESS);
				else if (hdp -> flags & HD_QUOTED)
					fputs(cp, stdout);
				else if (!(tmp = evalvararg(cp,
				0, 1, '\'', 0, 1)))
					ret = RET_FAIL;
				else {
					fputs(tmp, stdout);
					free(tmp);
				}
			}
			if (c == READ_EOF) break;
			if (ret == RET_SUCCESS) fputc('\n', stdout);
			continue;
		}
		cp = c_realloc(cp, i, &size);
		cp[i++] = c;
	}
	safeclose(fdin);
	fflush(stdout);
	free(cp);
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

static int NEAR openpseudofd(rp)
redirectlist *rp;
{
# if	!MSDOS && !defined (USEFAKEPIPE)
	p_id_t pid;
	int fildes[2];
# endif
	char pfile[MAXPATHLEN];
	int fd;

	if (!(rp -> type & MD_RDWR) || (rp -> type & MD_RDWR) == MD_RDWR)
		return(-1);

# if	!MSDOS && !defined (USEFAKEPIPE)
	if (pipe(fildes) < 0);
	else if ((pid = makechild(0, mypid)) < 0L) {
		safeclose(fildes[0]);
		safeclose(fildes[1]);
		return(-1);
	}
	else if (pid) {
		safeclose(rp -> new);
		if (rp -> type & MD_READ) {
			safeclose(fildes[1]);
			rp -> new = newdup(fildes[0]);
		}
		else {
			safeclose(fildes[0]);
			rp -> new = newdup(fildes[1]);
		}
		return(0);
	}
	else {
		if (rp -> type & MD_READ) {
			safeclose(fildes[0]);
			fildes[1] = newdup(fildes[1]);
			fdcopy(rp -> new, fildes[1]);
			safeclose(rp -> new);
			safeclose(fildes[1]);
		}
		else {
			safeclose(fildes[1]);
			fildes[0] = newdup(fildes[0]);
			fdcopy(fildes[0], rp -> new);
			safeclose(rp -> new);
			safeclose(fildes[0]);
		}
		prepareexit(1);
		Xexit(RET_SUCCESS);
	}
# endif	/* !MSDOS && !USEFAKEPIPE */

	if ((fd = newdup(mktmpfile(pfile))) < 0) return(-1);
	if (rp -> type & MD_WRITE) rp -> dosfd = rp -> new;
	else {
		fdcopy(rp -> new, fd);
		safeclose(rp -> new);
		safeclose(fd);
		fd = newdup(Xopen(pfile, O_BINARY | O_RDONLY, 0666));
		if (fd < 0) {
			safermtmpfile(pfile);
			rp -> fakepipe = NULL;
			return(-1);
		}
	}
	rp -> new = fd;
	rp -> fakepipe = strdup2(pfile);
	return(0);
}

static int NEAR closepseudofd(rp)
redirectlist *rp;
{
	int fd;

	if (!(rp -> fakepipe)) return(0);
	if (rp -> dosfd >= 0 && rp -> type & MD_WRITE) {
		fd = Xopen(rp -> fakepipe, O_BINARY | O_RDONLY, 0666);
		if (fd >= 0) {
			fdcopy(fd, rp -> dosfd);
			safeclose(fd);
		}
		safeclose(rp -> dosfd);
	}
	safermtmpfile(rp -> fakepipe);
	free(rp -> fakepipe);
	rp -> fakepipe = NULL;
	rp -> dosfd = -1;
	return(0);
}
#endif	/* FD && !_NODOSDRIVE */

static redirectlist *NEAR doredirect(rp)
redirectlist *rp;
{
#if	!MSDOS
	int oldexec, newexec = -1;
#endif
	redirectlist *errp;
	char *tmp;
	int type;

	if (rp -> next && (errp = doredirect(rp -> next))) return(errp);

	type = rp -> type;
	if (!(rp -> filename)) tmp = NULL;
	else if (type & MD_HEREDOC) tmp = rp -> filename;
	else if (!(tmp = evalvararg(rp -> filename, 0, 1, '\0', 1, 0))) {
		errno = -1;
		return(rp);
	}
	else if ((type & MD_FILEDESC) && tmp[0] == '-' && !tmp[1]) {
		free(tmp);
		tmp = NULL;
		type &= ~MD_FILEDESC;
	}

	if (!tmp);
	else if (type & MD_HEREDOC) {
		rp -> new = openheredoc((heredoc_t *)tmp, rp -> fd);
		if (rp -> new < 0) return(rp);
	}
	else if (type & MD_FILEDESC) {
		rp -> new = evalfiledesc(tmp);
		free(tmp);
		if (rp -> new < 0) {
			errno = EBADF;
			return(rp);
		}
#if	MSDOS
		if (isvalidfd(rp -> new) < 0) {
#else
		if ((newexec = isvalidfd(rp -> new)) < 0) {
#endif
			rp -> new = -1;
#ifdef	BASHSTYLE
	/* bash treats ineffective descriptor as error */
			errno = EBADF;
			return(rp);
#else
			rp -> old = -1;
			rp -> type = 0;
			return(NULL);
#endif
		}
	}
	else if (restricted && (type & MD_WRITE)) {
		free(tmp);
		errno = 0;
		return(rp);
	}
	else {
		rp -> new = newdup(Xopen(tmp, redmode(type), 0666));
		free(tmp);
		if (rp -> new < 0) return(rp);
#if	MSDOS && !defined (LSI_C)
# ifdef	DJGPP
		if (isatty(rp -> new));
		else
# endif
		setmode(rp -> new, O_BINARY);
#endif
	}

#if	MSDOS
	rp -> old = newdup(Xdup(rp -> fd));
#else
	if ((oldexec = fcntl(rp -> fd, F_GETFD, NULL)) < 0) rp -> old = -1;
	else if ((rp -> old = newdup(Xdup(rp -> fd))) < 0) {
		if (rp -> new >= 0 && !(type & MD_FILEDESC))
			safeclose(rp -> new);
		rp -> new = -1;
		return(rp);
	}
	else if (oldexec > 0 || rp -> fd == STDIN_FILENO
	|| rp -> fd == STDOUT_FILENO || rp -> fd == STDERR_FILENO)
		closeonexec(rp -> old);
#endif

	if (rp -> new != rp -> fd) {
		safeclose(rp -> fd);
		if (rp -> new >= 0) {
#if	defined (FD) && !defined (_NODOSDRIVE)
			if (rp -> new >= DOSFDOFFSET) openpseudofd(rp);
#endif
			if (Xdup2(rp -> new, rp -> fd) < 0) return(rp);
		}
	}

	if ((type & MD_WITHERR) && rp -> new != STDERR_FILENO
	&& rp -> fd == STDOUT_FILENO) {
		if ((rp -> fd = newdup(Xdup(STDERR_FILENO))) < 0)
			return(rp);
		if (Xdup2(rp -> new, STDERR_FILENO) < 0) {
			safeclose(rp -> fd);
			safeclose(rp -> new);
			rp -> fd = rp -> new = -1;
			return(rp);
		}
	}
#if	!MSDOS
	if (newexec > 0
	&& rp -> fd != STDIN_FILENO
	&& rp -> fd != STDOUT_FILENO
	&& rp -> fd != STDERR_FILENO)
		closeonexec(rp -> fd);
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

	if (!(type & MD_HEREDOC)) to = strdup2(to);
	else {
		if (!(to = (char *)newheredoc(to, type & MD_APPEND))) {
			doperror(NULL, deftmpdir);
			return(-1);
		}
		trp -> cont |= CN_HDOC;
	}
	if (from < 0) from = (type & MD_READ) ? STDIN_FILENO : STDOUT_FILENO;

	if (!(trp -> comm)) trp -> comm = newcomm();
	rp = newrlist(from, to, type, (trp -> comm) -> redp);
	(trp -> comm) -> redp = rp;
	return(0);
}

#if	!MSDOS
/*ARGSUSED*/
VOID cmpmail(path, msg, mtimep)
char *path, *msg;
time_t *mtimep;
{
	struct stat st;

	if (!path || stat(path, &st) < 0) return;
	if (st.st_size > 0 && *mtimep && st.st_mtime > *mtimep) {
# ifndef	MINIMUMSHELL
		if (msg) kanjifputs(msg, stderr);
		else
# endif
		fputs("you have mail", stderr);
		fputc('\n', stderr);
		fflush(stderr);
	}
	*mtimep = st.st_mtime;
}

# ifdef	MINIMUMSHELL
static VOID NEAR checkmail(reset)
int reset;
{
	static time_t mtime = 0;

	if (reset) mtime = 0;
	else cmpmail(getconstvar("MAIL"), NULL, &mtime);
}
# endif	/* !MINIMUMSHELL */
#endif	/* !MSDOS */

int identcheck(ident, delim)
char *ident;
int delim;
{
	int i;

	if (!ident || !*ident || !isidentchar(*ident)) return(0);
	for (i = 1; ident[i]; i++)
		if (!isidentchar(ident[i]) && !isdigit(ident[i])) break;
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
		if (export) exportsize -= (long)strlen(var[i]) + 1;
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
		exportsize += (long)strlen(s) + 1;
#if	MSDOS
		if (len == sizeof("PATH") - 1
		&& !strnpathcmp(s, "PATH", len)) {
			char *cp, *next;

			next = &(s[len + 1]);
			for (cp = next; cp; cp = next) {
				if (_dospath(cp))
					next = strchr(&(cp[2]), PATHDELIM);
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
		cp = strdupcpy(s, len);
		execerror(cp, ER_CANNOTUNSET, 0);
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
		cp = strdupcpy(s, len);
		execerror(cp, ER_ISREADONLY, 0);
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
			execerror("PATH", ER_RESTRICTED, 0);
			return(-1);
		}
#ifndef	_NOUSEHASH
		searchhash(NULL, NULL, NULL);
#endif
	}
	if (restricted
	&& len == sizeof("SHELL") - 1 && !strnpathcmp(s, "SHELL", len)) {
		execerror("SHELL", ER_RESTRICTED, 0);
		return(-1);
	}
#if	!MSDOS
# ifdef	MINIMUMSHELL
	if (len == sizeof("MAIL") - 1 && !strnpathcmp(s, "MAIL", len))
		checkmail(1);
# else	/* !MINIMUMSHELL */
	if (!strnpathcmp(s, "MAIL", sizeof("MAIL") - 1)) {
		if (len == sizeof("MAIL") - 1) {
			if (!getconstvar("MAILPATH"))
				replacemailpath(&(s[len + 1]), 0);
		}
		else if (len == sizeof("MAILPATH") - 1
		&& !strnpathcmp(s + sizeof("MAIL") - 1, "PATH", len))
			replacemailpath(&(s[len + 1]), 1);
		else if (len == sizeof("MAILCHECK") - 1
		&& !strnpathcmp(s + sizeof("MAIL") - 1, "CHECK", len)) {
			if ((mailcheck = isnumeric(&(s[len + 1]))) < 0)
				mailcheck = 0;
		}
	}
# endif	/* !MINIMUMSHELL */
#endif	/* !MSDOS */
#ifndef	NOPOSIXUTIL
	if (len == sizeof("OPTIND") - 1 && !strnpathcmp(s, "OPTIND", len))
		if ((posixoptind = isnumeric(&(s[len + 1]))) <= 1)
			posixoptind = 0;
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
		exportlist[i] = strdupcpy(s, len);
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

int unset(ident, len)
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

static heredoc_t *NEAR duplheredoc(hdp)
heredoc_t *hdp;
{
	heredoc_t *new;

	if (!hdp) return(NULL);

	new = (heredoc_t *)malloc2(sizeof(heredoc_t));
	new -> eof = strdup2(hdp -> eof);
	new -> filename = strdup2(hdp -> filename);
	new -> buf = strdup2(hdp -> buf);
	new -> fd = hdp -> fd;
	new -> flags = hdp -> flags;
	return(new);
}

static redirectlist *NEAR duplrlist(rp)
redirectlist *rp;
{
	redirectlist *new, *next;
	char *filename;

	if (!rp) return(NULL);
	next = duplrlist(rp -> next);
	if (rp -> type & MD_HEREDOC)
		filename = (char *)duplheredoc((heredoc_t *)(rp -> filename));
	else filename = strdup2(rp -> filename);
	new = newrlist(rp -> fd, filename, rp -> type, next);
	new -> new = rp -> new;
	new -> old = rp -> old;
#if	defined (FD) && !defined (_NODOSDRIVE)
	new -> fakepipe = rp -> fakepipe;
	new -> dosfd = rp -> dosfd;
#endif
	return(new);
}

syntaxtree *duplstree(trp, parent)
syntaxtree *trp, *parent;
{
	syntaxtree *new;
	redirectlist *rp;
	command_t *comm;

	new = newstree(parent);
	new -> type = trp -> type;
	new -> cont = trp -> cont;
	new -> flags = (trp -> flags | ST_NOWN);
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
			(new -> comm) -> redp = duplrlist(comm -> redp);
		}
	}
	if (trp -> next) {
		if (!(trp -> cont & (CN_QUOT | CN_META)))
			new -> next = duplstree(trp -> next, new);
		else {
			rp = (redirectlist *)malloc2(sizeof(redirectlist));
			memcpy((char *)rp, (char *)(trp -> next),
				sizeof(redirectlist));
			rp -> filename = strdup2(rp -> filename);
			new -> next = (syntaxtree *)rp;
		}
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

static char *NEAR getifs(VOID_A)
{
	char *ifs;

	return((ifs = getconstvar("IFS")) ? ifs : IFS_SET);
}

static int getretval(VOID_A)
{
	return(ret_status);
}

static p_id_t getorgpid(VOID_A)
{
	return(orgpid);
}

static p_id_t getlastpid(VOID_A)
{
	return(lastpid);
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
	cp = strdupcpy(arg, len);
	execerror(cp, ER_PARAMNOTSET, 0);
	free(cp);
	return(-1);
}

static VOID safeexit(VOID_A)
{
	if (interactive_io) return;
	Xexit2(RET_FAIL);
}

int getstatid(trp)
syntaxtree *trp;
{
	int id;

	if (!trp || !isstatement(trp -> comm)
	|| (id = (trp -> comm) -> id) <= 0 || id > STATEMENTSIZ) return(-1);
	return(id - 1);
}

static int NEAR getparenttype(trp)
syntaxtree *trp;
{
	int id;

	if ((id = getstatid(parentstree(trp))) < 0) return(0);
	return(statementlist[id].type & STT_TYPE);
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
		if (prev <= 0 || !(tmptr = parentstree(*trpp))) return(-1);

		for (i = 0; i < SMPREV; i++) {
			if (!(statementlist[no].prev[i])) continue;
			if (statementlist[no].prev[i] == SM_ANOTHER) return(0);
			if (prev == statementlist[no].prev[i]) break;
		}
		if (i >= SMPREV) return(-1);

		if ((type & STT_NEEDLIST) && !isopnot(statementbody(tmptr))
		&& !(statementbody(tmptr) -> comm))
			return(-1);

		*trpp = tmptr;
	}

	if (statementlist[no].type & STT_NEEDNONE) {
		if (!(tmptr = parentstree(*trpp))) return(-1);
		*trpp = tmptr;
		if (getstatid(tmptr = getparent(tmptr)) == SM_FUNC - 1) {
			if (!(tmptr = parentstree(tmptr))) return(-1);
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
		if (arg) {
			comm -> argv = (char **)realloc2(comm -> argv,
					(comm -> argc + 2) * sizeof(char *));
			comm -> argv[comm -> argc] = strdup2(arg);
			comm -> argv[comm -> argc + 1] = NULL;
		}
	}
	else if (arg) {
		if ((trp -> flags & ST_NEXT)
		&& (id = getstatid(parentstree(trp))) >= 0
		&& !(statementlist[id].type & STT_NEEDLIST)) {
			syntaxerrno = ER_UNEXPTOK;
			return(trp);
		}

		comm = trp -> comm = newcomm();
		comm -> argc = 0;
		comm -> argv = (char **)malloc2(2 * sizeof(char *));
		comm -> argv[0] = strdup2(arg);
		comm -> argv[1] = NULL;
	}

	if ((id = getstatid(getparent(trp))) >= 0)
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
				if (!(tmptr = parentstree(trp))
				|| !(tmptr = parentstree(tmptr))) {
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

static int NEAR addarg(trpp, rp, tok, lenp, notok)
syntaxtree **trpp;
redirectlist *rp;
char *tok;
int *lenp, notok;
{
	syntaxtree *tmptr;
	int i, n, id, type;

	type = rp -> type;
	rp -> type = 0;
	if (!tok) tok = rp -> filename;

	if ((*trpp) -> flags & ST_NODE) {
		syntaxerrno = ER_UNEXPTOK;
		return(-1);
	}

	if (!*lenp) {
		if (type && notok) {
			syntaxerrno = ER_UNEXPTOK;
			return(-1);
		}
		rp -> type = type;
		return(0);
	}
	tok[*lenp] = '\0';
	*lenp = 0;

	if (!type) {
		id = getstatid(tmptr = parentstree(*trpp));
		type = (id >= 0) ? statementlist[id].type : 0;

		if ((type & STT_NEEDIDENT) && tmptr == getparent(*trpp));
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
	else if (rp -> fd >= 0 || (type & MD_RDWR) != MD_RDWR) {
		if (redirect(*trpp, rp -> fd, tok, type) < 0) return(-1);
	}
	else {
		if (redirect(*trpp, STDIN_FILENO, tok, type) < 0
		|| redirect(*trpp, STDOUT_FILENO, tok, type) < 0) return(-1);
	}
	return(0);
}

static int NEAR evalredprefix(trpp, rp, lenp)
syntaxtree **trpp;
redirectlist *rp;
int *lenp;
{
	int n;

	rp -> filename[*lenp] = '\0';
	if ((n = evalfiledesc(rp -> filename)) < 0)
		addarg(trpp, rp, NULL, lenp, 1);
	else if (rp -> type) {
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
	|| !(tmptr = parentstree(trp)) || !ischild(tmptr -> comm))
		syntaxerrno = ER_UNEXPTOK;
	else {
		_addarg(trp, NULL);
		tmptr = _addarg(tmptr, NULL);
		tmptr -> cont |= (trp -> cont & CN_INHR);
		trp = tmptr;
	}
	return(trp);
}

static syntaxtree *NEAR semicolon(trp, rp, s, ptrp)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp;
{
	char tmptok[3];
	int i, id;

	id = getstatid(parentstree(trp));
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
		addarg(&trp, rp, tmptok, &i, 0);
	}
	return(trp);
}

static syntaxtree *NEAR ampersand(trp, rp, s, ptrp)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp;
{
	switch (s[*ptrp + 1]) {
		case '&':
			trp = _addarg(trp, NULL);
			(*ptrp)++;
			trp = linkstree(trp, OP_AND);
			break;
#ifndef	MINIMUMSHELL
		case '>':
			(*ptrp)++;
			rp -> fd = STDOUT_FILENO;
			rp -> type = (MD_WRITE | MD_WITHERR);
			if (s[*ptrp + 1] == '>') {
				(*ptrp)++;
				rp -> type |= MD_APPEND;
			}
			else if (s[*ptrp + 1] == '|') {
				(*ptrp)++;
				rp -> type |= MD_FORCED;
			}
			break;
		case '|':
			trp = _addarg(trp, NULL);
			(*ptrp)++;
			trp = linkstree(trp, OP_NOWN);
			break;
#endif	/* !MINIMUMSHELL */
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
#ifndef	MINIMUMSHELL
		case '&':
			(*ptrp)++;
			if (redirect(trp,
			STDERR_FILENO, "1", MD_WRITE | MD_FILEDESC) >= 0)
				trp = linkstree(trp, OP_PIPE);
			break;
#endif
		default:
			trp = linkstree(trp, OP_PIPE);
			break;
	}
	return(trp);
}

static syntaxtree *NEAR lessthan(trp, rp, s, ptrp)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp;
{
	rp -> type = MD_READ;
	switch (s[*ptrp + 1]) {
		case '<':
			(*ptrp)++;
			rp -> type |= MD_HEREDOC;
			if (s[*ptrp + 1] == '-') {
				(*ptrp)++;
				rp -> type |= MD_APPEND;
			}
			break;
#ifndef	MINIMUMSHELL
		case '>':
			(*ptrp)++;
			rp -> type |= MD_WRITE;
			break;
#endif
		case '&':
			(*ptrp)++;
			if (s[*ptrp + 1] != '-') rp -> type |= MD_FILEDESC;
			else {
				(*ptrp)++;
				if (redirect(trp,
				rp -> fd, NULL, rp -> type) >= 0)
					rp -> type = 0;
			}
			break;
		case '-':
			(*ptrp)++;
			if (redirect(trp, rp -> fd, NULL, rp -> type) >= 0)
				rp -> type = 0;
			break;
		default:
			break;
	}
	return(trp);
}

static syntaxtree *NEAR morethan(trp, rp, s, ptrp)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp;
{
	rp -> type = MD_WRITE;
	switch (s[*ptrp + 1]) {
		case '>':
			(*ptrp)++;
			rp -> type |= MD_APPEND;
			break;
#ifndef	MINIMUMSHELL
		case '<':
			(*ptrp)++;
			rp -> type |= MD_READ;
			break;
#endif
		case '&':
			(*ptrp)++;
			if (s[*ptrp + 1] != '-') rp -> type |= MD_FILEDESC;
			else {
				(*ptrp)++;
				if (redirect(trp,
				rp -> fd, NULL, rp -> type) >= 0)
					rp -> type = 0;
			}
			break;
		case '-':
			(*ptrp)++;
			if (redirect(trp, rp -> fd, NULL, rp -> type) >= 0)
				rp -> type = 0;
			break;
#ifndef	MINIMUMSHELL
		case '|':
			(*ptrp)++;
			rp -> type |= MD_FORCED;
			break;
#endif
		default:
			break;
	}
	return(trp);
}

#if	defined (BASHSTYLE) || !defined (MINIMUMSHELL)
syntaxtree *startvar(trp, rp, s, ptrp, tptrp, n)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp, *tptrp, n;
{
	syntaxtree *new;
	redirectlist *newrp;
	char *cp;

	cp = malloc2(*tptrp + n + 1);
	strncpy(cp, rp -> filename, *tptrp);
	strncpy2(&(cp[*tptrp]), &(s[*ptrp]), n);
	*tptrp = 0;
	*ptrp += n - 1;

	new = newstree(NULL);
	new -> parent = trp;
	newrp = (redirectlist *)malloc2(sizeof(redirectlist));
	memcpy((char *)newrp, (char *)rp, sizeof(redirectlist));
	newrp -> filename = cp;
	newrp -> next = (redirectlist *)new;
	trp -> next = (syntaxtree *)newrp;
	trp -> flags |= ST_BUSY;
	rp -> type = MD_NORMAL;
	rp -> fd = -1;
	rp -> new = rp -> old = '\0';

	return(new);
}

static syntaxtree *NEAR endvar(trp, rp, s, ptrp, tptrp, sp, n)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp, *tptrp;
ALLOC_T *sp;
int n;
{
	syntaxtree *tmptr;
	char *cp;
	ALLOC_T size;
	int len;

	if (!(tmptr = trp -> parent) || !(tmptr -> flags & ST_BUSY)
	|| !(tmptr -> next)) {
		syntaxerrno = ER_UNEXPTOK;
		return(trp);
	}
	trp = tmptr;
	tmptr = trp -> next;
	trp -> next = NULL;
	trp -> flags &= ~ST_BUSY;

	cp = rp -> filename;
	memcpy((char *)rp, (char *)tmptr, sizeof(redirectlist));
	len = strlen(rp -> filename);
	size = c_allocsize(len + *tptrp + n + 2);
	if (size > *sp) cp = realloc2(cp, *sp = size);
	memmove(&(cp[len]), cp, *tptrp);
	strncpy(cp, rp -> filename, len);
	*tptrp += len;
	strncpy(&(cp[*tptrp]), &(s[*ptrp]), n);
	*tptrp += n;
	*ptrp += n - 1;
	free(rp -> filename);
	rp -> filename = cp;

	free(rp -> next);
	rp -> next = NULL;
	free(tmptr);

	return(trp);
}

static syntaxtree *NEAR addvar(trp, s, ptrp, tok, tptrp, n)
syntaxtree *trp;
char *s;
int *ptrp;
char *tok;
int *tptrp, n;
{
	syntaxtree *tmptr;
	redirectlist *rp;
	int len;

	if (!(tmptr = trp -> parent) || !(tmptr -> flags & ST_BUSY)
	|| !(rp = (redirectlist *)(tmptr -> next))) {
		syntaxerrno = ER_UNEXPTOK;
		return(trp);
	}

	len = strlen(rp -> filename);
	rp -> filename = realloc2(rp -> filename, len + *tptrp + n + 1);
	strncpy(&(rp -> filename[len]), tok, *tptrp);
	len += *tptrp;
	*tptrp = 0;
	if (n > 0) {
		strncpy(&(rp -> filename[len]), &(s[*ptrp]), n);
		len += n;
		*ptrp += n - 1;
	}
	rp -> filename[len] = '\0';

	return(trp);
}
#endif	/* BASHSTYLE || !MINIMUMSHELL */

static syntaxtree *NEAR normaltoken(trp, rp, s, ptrp, tptrp, sp)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp, *tptrp;
ALLOC_T *sp;
{
	char tmptok[2];
	ALLOC_T size;
	int i;

	switch (s[*ptrp]) {
		case '{':
			if (!strchr(IFS_SET, s[*ptrp + 1])) {
				rp -> filename[(*tptrp)++] = s[*ptrp];
				break;
			}
			if (addarg(&trp, rp, NULL, tptrp, 0) < 0) break;
			i = 0;
			tmptok[i++] = s[*ptrp];
			addarg(&trp, rp, tmptok, &i, 0);
			break;
		case '}':
			rp -> filename[(*tptrp)++] = s[*ptrp];
			if ((*tptrp) > 1 || getparenttype(trp) != STT_LIST)
				break;
			if (trp -> comm) {
#ifdef	BASHSTYLE
	/* bash allows the list which does not end with ";" */
				trp = _addarg(trp, NULL);
				trp = linkstree(trp, OP_NONE);
#else
				break;
#endif
			}
			addarg(&trp, rp, NULL, tptrp, 0);
			break;
		case '(':
			if (hascomm(trp)) {
				if (rp -> type
				|| *tptrp || (trp -> comm) -> argc)
					syntaxerrno = ER_UNEXPTOK;
				else {
					trp = _addarg(trp, NULL);
					trp = linkstree(trp, OP_NONE);
					i = 0;
					tmptok[i++] = s[*ptrp];
					addarg(&trp, rp, tmptok, &i, 0);
				}
			}
			else if (*tptrp) {
				if (!*tptrp
				|| addarg(&trp, rp, NULL, tptrp, 0) < 0)
					syntaxerrno = ER_UNEXPTOK;
				else {
					trp = _addarg(trp, NULL);
					trp = linkstree(trp, OP_NONE);
					i = 0;
					tmptok[i++] = s[*ptrp];
					addarg(&trp, rp, tmptok, &i, 0);
				}
			}
			else if (rp -> type) syntaxerrno = ER_UNEXPTOK;
#ifdef	BASHSTYLE
	/* bash does not allow the function definition without "{ }" */
			else if (getstatid(parentstree(trp)) == SM_FUNC - 1)
				syntaxerrno = ER_UNEXPTOK;
#endif
			else trp = childstree(trp, SM_CHILD);
			break;
		case ')':
			if (getparenttype(trp) == STT_LPAREN) {
				if (*tptrp || trp -> comm)
					syntaxerrno = ER_UNEXPTOK;
				else {
					i = 0;
					tmptok[i++] = s[*ptrp];
					addarg(&trp, rp, tmptok, &i, 0);
				}
				break;
			}
			if (addarg(&trp, rp, NULL, tptrp, 0) < 0) break;
			trp = rparen(trp);
			break;
		case ';':
			if (addarg(&trp, rp, NULL, tptrp, 1) < 0) break;
			trp = semicolon(trp, rp, s, ptrp);
			break;
		case '&':
			if (addarg(&trp, rp, NULL, tptrp, 1) < 0) break;
			trp = ampersand(trp, rp, s, ptrp);
			break;
		case '|':
			if (addarg(&trp, rp, NULL, tptrp, 1) < 0) break;
			trp = vertline(trp, s, ptrp);
			break;
#ifndef	MINIMUMSHELL
		case '!':
			if (*tptrp || trp -> comm)
				rp -> filename[(*tptrp)++] = s[*ptrp];
			else if (rp -> type || (trp -> flags & ST_NODE)) {
				syntaxerrno = ER_UNEXPTOK;
				return(trp);
			}
			else trp = linkstree(trp, OP_NOT);
			break;
#endif
		case '<':
			if ((rp -> fd = evalredprefix(&trp, rp, tptrp)) >= -1)
				trp = lessthan(trp, rp, s, ptrp);
			break;
		case '>':
			if ((rp -> fd = evalredprefix(&trp, rp, tptrp)) >= -1)
				trp = morethan(trp, rp, s, ptrp);
			break;
		case '\r':
#ifdef	BASHSTYLE
	/* bash treats '\r' as just a character */
			if (rp -> type) syntaxerrno = ER_UNEXPNL;
			else rp -> filename[(*tptrp)++] = s[*ptrp];
			break;
#else
/*FALLTHRU*/
#endif
		case '\n':
			if (addarg(&trp, rp, NULL, tptrp, 1) < 0) break;
			if (trp -> comm) {
				trp = _addarg(trp, NULL);
				trp = linkstree(trp, OP_FG);
			}
			else if (!(trp -> flags & ST_NEXT)
			&& (i = getstatid(parentstree(trp)) + 1) > 0) {
				if (i == SM_FOR || i == SM_CASE || i == SM_IN
				|| *tptrp)
					syntaxerrno = ER_UNEXPNL;
			}

			if (trp -> cont & CN_HDOC) {
				i = *ptrp + 1;
#ifdef	BASHSTYLE
	/* bash treats '\r' as just a character */
				while (s[*ptrp + 1] && s[*ptrp + 1] != '\n')
					(*ptrp)++;
#else
				while (s[*ptrp + 1] && s[*ptrp + 1] != '\r'
				&& s[*ptrp + 1] != '\n') (*ptrp)++;
#endif
				*tptrp = *ptrp - i + 1;
				size = c_allocsize(*tptrp + 2);
				if (size > *sp)
					rp -> filename =
						realloc2(rp -> filename,
							*sp = size);
				strncpy(rp -> filename, &(s[i]), *tptrp);
				trp -> flags |= ST_HDOC;
			}
			break;
		case '#':
			if (*tptrp) rp -> filename[(*tptrp)++] = s[*ptrp];
			else {
#ifdef	BASHSTYLE
	/* bash treats '\r' as just a character */
				while (s[*ptrp + 1] && s[*ptrp + 1] != '\n')
					(*ptrp)++;
#else
				while (s[*ptrp + 1] && s[*ptrp + 1] != '\r'
				&& s[*ptrp + 1] != '\n') (*ptrp)++;
#endif
			}
			break;
		default:
			if (!strchr(IFS_SET, s[*ptrp]))
				rp -> filename[(*tptrp)++] = s[*ptrp];
			else addarg(&trp, rp, NULL, tptrp, 0);
			break;
	}
	return(trp);
}

static syntaxtree *NEAR casetoken(trp, rp, s, ptrp, tptrp)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp, *tptrp;
{
	char tmptok[2];
	int i, stype;

	switch (s[*ptrp]) {
		case ')':
			if (!*tptrp);
			else if (addarg(&trp, rp, NULL, tptrp, 0) < 0) break;
			trp = _addarg(trp, NULL);
			trp = linkstree(trp, OP_NONE);
			i = 0;
			tmptok[i++] = s[*ptrp];
			addarg(&trp, rp, tmptok, &i, 0);
			break;
		case '|':
			if (!*tptrp);
			else addarg(&trp, rp, NULL, tptrp, 0);
			break;
		case ';':
			if (addarg(&trp, rp, NULL, tptrp, 1) < 0) break;
			/* for "esac;" */
			if ((stype = getparenttype(trp)) != STT_INCASE
			&& stype != STT_CASEEND)
				trp = semicolon(trp, rp, s, ptrp);
			else syntaxerrno = ER_UNEXPTOK;
			break;
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
			if (getparenttype(trp) == STT_INCASE && *tptrp) {
				syntaxerrno = ER_UNEXPNL;
				break;
			}
			if (addarg(&trp, rp, NULL, tptrp, 1) < 0) break;
			if (trp -> comm) {
				trp = _addarg(trp, NULL);
				trp = linkstree(trp, OP_FG);
			}
			else if (!(trp -> flags & ST_NEXT)
			&& (i = getstatid(parentstree(trp)) + 1) > 0) {
				if (*tptrp) syntaxerrno = ER_UNEXPNL;
			}
			break;
		case '#':
			if (*tptrp) rp -> filename[(*tptrp)++] = s[*ptrp];
			else {
#ifdef	BASHSTYLE
	/* bash treats '\r' as just a character */
				while (s[*ptrp + 1] && s[*ptrp + 1] != '\n')
					(*ptrp)++;
#else
				while (s[*ptrp + 1] && s[*ptrp + 1] != '\r'
				&& s[*ptrp + 1] != '\n') (*ptrp)++;
#endif
			}
			break;
		default:
			if (!strchr(IFS_SET, s[*ptrp]))
				rp -> filename[(*tptrp)++] = s[*ptrp];
			else if (*tptrp > 0) {
				addarg(&trp, rp, NULL, tptrp, 0);
				do {
					(*ptrp)++;
				} while (s[*ptrp] == ' ' || s[*ptrp] == '\t');

				/* for "esac " */
				if ((stype = getparenttype(trp)) != STT_INCASE
				&& stype != STT_CASEEND) (*ptrp)--;
				else if (!(s[*ptrp]) || s[*ptrp] == '\n')
					syntaxerrno = ER_UNEXPNL;
				else if (s[*ptrp] == ')' || s[*ptrp] == '|')
					(*ptrp)--;
				else syntaxerrno = ER_UNEXPTOK;
			}
			break;
	}
	return(trp);
}

#if	!defined (BASHBUG) && !defined (MINIMUMSHELL)
static int NEAR cmpstatement(s, id)
char *s;
int id;
{
	int len;

	if (--id < 0) return(-1);
	len = strlen(statementlist[id].ident);
	if (strnpathcmp(s, statementlist[id].ident, len)) return(-1);
	return(len);
}

static syntaxtree *NEAR comsubtoken(trp, rp, s, ptrp, tptrp, sp)
syntaxtree *trp;
redirectlist *rp;
char *s;
int *ptrp, *tptrp;
ALLOC_T *sp;
{
	int i, len;

	if (!*tptrp) {
		if ((len = cmpstatement(&(s[*ptrp]), SM_CASE)) >= 0
		&& (!s[*ptrp + len] || strchr(IFS_SET, s[*ptrp + len]))) {
			trp = startvar(trp, rp, s, ptrp, tptrp, len);
			trp -> cont = CN_CASE;
			return(trp);
		}
		if ((trp -> cont & CN_SBST) == CN_CASE
		&& (len = cmpstatement(&(s[*ptrp]), SM_ESAC)) >= 0
		&& (!s[*ptrp + len] || strchr(IFS_SET, s[*ptrp + len])
		|| s[*ptrp + len] == ';' || s[*ptrp + len] == ')')) {
			trp = endvar(trp, rp, s, ptrp, tptrp, sp, len);
			return(trp);
		}
		if (strchr(IFS_SET, s[*ptrp])) {
			for (len = 1; s[*ptrp + len]; len++)
				if (!strchr(IFS_SET, s[*ptrp + len])) break;
			trp = addvar(trp, s, ptrp, rp -> filename, tptrp, len);
			return(trp);
		}
	}

	if (s[*ptrp] == '(') {
		trp = startvar(trp, rp, s, ptrp, tptrp, 1);
		trp -> cont = CN_COMM;
	}
	else if (s[*ptrp] == ')') {
		if ((trp -> cont & CN_SBST) == CN_CASE)
			rp -> filename[(*tptrp)++] = s[*ptrp];
		else trp = endvar(trp, rp, s, ptrp, tptrp, sp, 1);
	}
# ifdef	BASHSTYLE
	/* bash treats '\r' as just a character */
	else if (s[*ptrp] == '\n')
# else
	else if (s[*ptrp] == '\n' || s[*ptrp] == '\r')
# endif
		trp = addvar(trp, s, ptrp, rp -> filename, tptrp, 1);
	else {
		for (i = 0; i < DELIMLISTSIZ; i++) {
			len = strlen(delimlist[i].symbol);
			if (!strncmp(&(s[*ptrp]), delimlist[i].symbol, len))
				break;
		}
		if (i >= DELIMLISTSIZ) rp -> filename[(*tptrp)++] = s[*ptrp];
		else if (delimlist[i].level)
			trp = addvar(trp, s, ptrp, rp -> filename, tptrp, len);
		else {
			strncpy(&(rp -> filename[*tptrp]), &(s[*ptrp]), len);
			*tptrp += len;
			*ptrp += len - 1;
		}
	}

	return(trp);
}
#endif	/* !BASHBUG && !MINIMUMSHELL */

static syntaxtree *NEAR analyzeloop(trp, rp, s, quiet)
syntaxtree *trp;
redirectlist *rp;
char *s;
int quiet;
{
	char *cp;
	ALLOC_T size;
	int i, j, n, stype, hdoc;

	if (!rp -> filename) {
		j = 0;
		rp -> filename = c_realloc(NULL, 0, &size);
	}
	else {
		j = strlen(rp -> filename);
		size = c_allocsize(j);
	}

	hdoc = 0;
	for (i = 0; s && s[i]; i++) {
		syntaxerrno = 0;
		rp -> filename = c_realloc(rp -> filename, j + 2, &size);

		if ((trp -> cont & CN_HDOC) && (trp -> flags & ST_HDOC)) {
			if (s[i] != '\n') {
				if (hdoc) {
					rp -> filename[j++] = s[i];
					continue;
				}
			}
			else {
				rp -> filename[j] = '\0';
				n = saveheredoc(rp -> filename, trp);
				if (n < 0) {
					if (!quiet) doperror(NULL, NULL);
					return(NULL);
				}
				hdoc = (trp -> cont & CN_HDOC);
				if (n > 0) {
					j = 0;
					continue;
				}
				hdoc = 0;
				trp -> cont &= ~CN_HDOC;
			}
		}

		if (s[i] == rp -> new) {
			rp -> filename[j++] = s[i];
#ifdef	BASHSTYLE
			rp -> new = rp -> old;
			rp -> old = '\0';
#else
			rp -> new = '\0';
#endif
		}
#ifdef	BASHSTYLE
		else if (rp -> new == '`') rp -> filename[j++] = s[i];
#endif
#ifdef	CODEEUC
		else if (isekana(s, i)) {
			rp -> filename[j++] = s[i++];
			rp -> filename[j++] = s[i];
		}
#endif
		else if (iskanji1(s, i)) {
			rp -> filename[j++] = s[i++];
			rp -> filename[j++] = s[i];
		}
		else if (rp -> new == '\'') rp -> filename[j++] = s[i];
		else if (s[i] == PMETA) {
			if (s[++i] != '\n' || rp -> new) {
				rp -> filename[j++] = PMETA;
				if (s[i]) rp -> filename[j++] = s[i];
				else {
					trp -> cont |= CN_META;
					break;
				}
			}
		}
#ifdef	BASHSTYLE
	/* bash can include `...` in "..." */
		else if (s[i] == '`') {
			rp -> filename[j++] = s[i];
			if (rp -> new) rp -> old = rp -> new;
			rp -> new = s[i];
		}
#endif
#ifndef	MINIMUMSHELL
		else if (s[i] == '$' && s[i + 1] == '(') {
			if (s[i + 2] != '(') {
				trp = startvar(trp, rp, s, &i, &j, 2);
				trp -> cont = CN_COMM;
			}
			else {
				trp = startvar(trp, rp, s, &i, &j, 3);
				trp -> cont = CN_EXPR;
			}
		}
#endif	/* !MINIMUMSHELL */
#ifdef	BASHSTYLE
	/* bash treats any meta character in ${} as just a character */
		else if (s[i] == '$' && s[i + 1] == '{') {
			trp = startvar(trp, rp, s, &i, &j, 2);
			trp -> cont = CN_VAR;
		}
		else if ((trp -> cont & CN_SBST) == CN_VAR) {
			if (s[i] != '}') rp -> filename[j++] = s[i];
			else trp = endvar(trp, rp, s, &i, &j, &size, 1);
		}
#else	/* !BASHSTYLE */
		else if (s[i] == '$' && s[i + 1] == '{') {
			rp -> filename[j++] = s[i++];
			rp -> filename[j++] = s[i];
			trp -> cont = CN_VAR;
		}
		else if (s[i] == '}' && (trp -> cont & CN_SBST) == CN_VAR) {
			rp -> filename[j++] = s[i];
			trp -> cont &= ~CN_VAR;
		}
#endif	/* !BASHSTYLE */
		else if (s[i] == '$') rp -> filename[j++] = s[i];
		else if (rp -> new) rp -> filename[j++] = s[i];
#ifdef	BASHSTYLE
		else if (s[i] == '\'' || s[i] == '"')
#else
		else if (s[i] == '\'' || s[i] == '"' || s[i] == '`')
#endif
		{
			rp -> filename[j++] = s[i];
			rp -> new = s[i];
		}
#ifndef	MINIMUMSHELL
# ifdef	BASHBUG
	/* bash cannot include 'case' statement within $() */
		else if ((trp -> cont & CN_SBST) == CN_COMM) {
			if (s[i] == '(') {
				trp = startvar(trp, rp, s, &i, &j, 1);
				trp -> cont = CN_COMM;
			}
			else if (s[i] != ')') rp -> filename[j++] = s[i];
			else trp = endvar(trp, rp, s, &i, &j, &size, 1);
		}
# else
		else if ((trp -> cont & CN_SBST) == CN_COMM
		|| (trp -> cont & CN_SBST) == CN_CASE)
			trp = comsubtoken(trp, rp, s, &i, &j, &size);
# endif
		else if ((trp -> cont & CN_SBST) == CN_EXPR) {
			if (s[i] == '(') {
				trp = startvar(trp, rp, s, &i, &j, 2);
				trp -> cont = CN_COMM;
			}
			else if (s[i] != ')' || s[i + 1] != ')')
				rp -> filename[j++] = s[i];
			else trp = endvar(trp, rp, s, &i, &j, &size, 2);
		}
#endif	/* !MINIMUMSHELL */
		else if ((stype = getparenttype(trp)) == STT_INCASE
		|| stype == STT_CASEEND)
			trp = casetoken(trp, rp, s, &i, &j);
		else {
#ifndef	NOALIAS
			n = checkalias(trp, rp -> filename, j, s[i]);
			if (n >= 0) {
				trp = analyzeloop(trp, rp,
					shellalias[n].comm, quiet);
				shellalias[n].flags &= ~AL_USED;
				if (!trp) return(NULL);
				j = strlen(rp -> filename);
				i--;
			}
			else
#endif	/* !NOALIAS */
			trp = normaltoken(trp, rp, s, &i, &j, &size);
		}

		if (syntaxerrno) {
			if (quiet) return(NULL);
			if (strchr(IFS_SET, s[i])) {
				if (j < 0) j = 0;
				rp -> filename[j] = '\0';
				syntaxerror(rp -> filename);
				return(NULL);
			}
			for (j = i + 1; s[j]; j++) {
				if (strchr(IFS_SET, s[j])) break;
#ifdef	CODEEUC
				else if (isekana(s, j)) j++;
#endif
				else if (iskanji1(s, j)) j++;
			}
			cp = strdupcpy(&(s[i]), j - i);
			syntaxerror(cp);
			free(cp);
			return(NULL);
		}
	}

	if (j && (trp -> cont & CN_HDOC) && (trp -> flags & ST_HDOC)) {
		rp -> filename[j] = '\0';
		if ((n = saveheredoc(rp -> filename, trp)) < 0) {
			if (!quiet) doperror(NULL, NULL);
			return(NULL);
		}
		if (n > 0) j = 0;
		else trp -> cont &= ~CN_HDOC;
	}
	rp -> filename[j] = '\0';

#ifndef	NOALIAS
	if ((n = checkalias(trp, rp -> filename, j, '\0')) >= 0) {
		trp = analyzeloop(trp, rp, shellalias[n].comm, quiet);
		shellalias[n].flags &= ~AL_USED;
	}
#endif	/* !NOALIAS */
	return(trp);
}

syntaxtree *analyze(s, trp, quiet)
char *s;
syntaxtree *trp;
int quiet;
{
	syntaxtree *parent;
	redirectlist *rp, red;
	int i;

	syntaxerrno = 0;
	red.fd = -1;
	red.filename = NULL;
	red.type = MD_NORMAL;
	red.new = red.old = 0;
#if	defined (FD) && !defined (_NODOSDRIVE)
	red.fakepipe = NULL;
	red.dosfd = -1;
#endif
	red.next = NULL;

	if (trp -> cont & (CN_QUOT | CN_META)) {
		memcpy((char *)&red, (char *)(trp -> next), sizeof(red));
		free(trp -> next);

		i = strlen(red.filename);
		if (i > 0) {
			if ((trp -> cont & CN_META) && red.new != '\'') i--;
			else if (s) red.filename[i++] = '\n';
		}
#ifndef	BASHSTYLE
	/* bash does not allow unclosed quote */
		if (!s) {
			if (trp -> cont & CN_QUOT) red.filename[i++] = red.new;
			red.new = '\0';
		}
#endif
		red.filename[i] = '\0';
	}
	else if ((trp -> cont & CN_HDOC)) {
		if ((i = saveheredoc(s, trp)) < 0) {
			if (!quiet) doperror(NULL, NULL);
			return(NULL);
		}
		else if (!i) {
			trp -> cont &= ~CN_HDOC;
			return(NULL);
		}

		return(trp);
	}
	else if (!s) return(NULL);

	trp -> next = NULL;
	trp -> cont &= CN_INHR;

	exectrapcomm();
	if (s) {
#ifndef	NOALIAS
		for (i = 0; shellalias[i].ident; i++)
			shellalias[i].flags &= ~AL_USED;
#endif
		if (!(trp = analyzeloop(trp, &red, s, quiet))) {
			if (red.filename) free(red.filename);
			return(NULL);
		}
	}

	i = (red.filename) ? strlen(red.filename) : 0;
	if (red.new) trp -> cont |= CN_QUOT;

	if (trp -> cont & (CN_QUOT | CN_META)) {
		rp = (redirectlist *)malloc2(sizeof(redirectlist));
		memcpy((char *)rp, (char *)&red, sizeof(red));
		trp -> next = (syntaxtree *)rp;
		red.filename = NULL;
	}
#if	defined (BASHSTYLE) || !defined (MINIMUMSHELL)
	else if (trp -> cont & CN_SBST) {
		red.filename[i++] = '\n';
		trp = addvar(trp, NULL, NULL, red.filename, &i, 0);
	}
#endif
	else if (getparenttype(trp) == STT_INCASE && (trp -> comm || i))
		syntaxerrno = ER_UNEXPNL;
	else if (addarg(&trp, &red, NULL, &i, 1) < 0);
	else if (!(trp = _addarg(trp, NULL)) || syntaxerrno);
	else if (trp -> comm) trp = linkstree(trp, OP_FG);
	else if (hasparent(trp)) {
#ifndef	MINIMUMSHELL
		if (isopnot(trp -> parent)) syntaxerrno = ER_UNEXPNL;
#endif
		if (!isopfg(trp -> parent) && !isopbg(trp -> parent)
		&& !isopnown(trp -> parent))
			trp -> cont |= CN_STAT;
	}
	if (syntaxerrno) {
		if (!quiet) syntaxerror(red.filename);
		if (red.filename) free(red.filename);
		return(NULL);
	}
	if (red.filename) free(red.filename);
	if ((parent = parentstree(trp)) && isstatement(parent -> comm))
		trp -> cont |= CN_STAT;

	return(trp);
}

static syntaxtree *NEAR analyzeeof(trp)
syntaxtree *trp;
{
	while (trp && trp -> cont) {
#ifdef	BASHSTYLE
		if (trp -> cont & CN_HDOC) trp = analyze(NULL, trp, 0);
#else
	/* bash does not allow unclosed quote */
		if (trp -> cont & (CN_META | CN_QUOT | CN_HDOC))
			trp = analyze(NULL, trp, 0);
	/* bash does not allow the format like as "foo |" */
		else if ((trp -> flags & ST_NEXT) && hasparent(trp)
		&& isoppipe(trp -> parent)) break;
#endif
		else {
			syntaxerrno = ER_UNEXPEOF;
			syntaxerror("");
			return(NULL);
		}
	}
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
		doperror(NULL, NULL);
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
		doperror(NULL, NULL);
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
			execerror((trp -> comm) -> argv[0], ER_NOTIDENT, 0);
			return(-1);
		}
		if (!statementcheck(tmptr -> next, 0)) {
			errno = EINVAL;
			doperror(NULL, NULL);
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
	if ((trp = analyzeeof(trp))) {
#ifdef	_NOUSEHASH
		return(stree);
#else
		if (!hashahead || check_stree(stree) >= 0) return(stree);
#endif
	}
	freestree(stree);
	free(stree);
	return(NULL);
}

#ifdef	DEBUG
static VOID NEAR Xexecve(path, argv, envp, bg)
char *path, *argv[], *envp[];
int bg;
#else
static VOID NEAR Xexecve(path, argv, envp)
char *path, *argv[], *envp[];
#endif
{
	int fd, ret;

	execve(path, argv, envp);
	if (errno != ENOEXEC) {
		if (errno == EACCES) {
			execerror(argv[0], ER_CANNOTEXE, 0);
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
		argvar = argv;
		sourcefile(fd, argv[0], 0);
		ret = ret_status;
	}
#ifdef	DEBUG
	if (!bg) Xexit2(ret);
#endif
	prepareexit(1);
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
	if (ext & CM_BATCH) strcpy(&(path[len]), "BAT");
	else if (ext & CM_EXE) strcpy(&(path[len]), "EXE");
	else strcpy(&(path[len]), "COM");

	return(path);
}

static char **NEAR replacebat(pathp, argv)
char **pathp, **argv;
{
	char *com;
	int i;

	if (!(com = getconstvar("COMSPEC")) && !(com = getconstvar("SHELL")))
		com = "\\COMMAND.COM";

	i = countvar(argv);
	memmove((char *)(&(argv[i + 2])), (char *)(&(argv[i])),
		(i + 1) * sizeof(char *));
	free(argv[2]);
	argv[2] = strdup2(*pathp);
	argv[1] = strdup2("/C");
	argv[0] = *pathp = strdup2(com);
	return(argv);
}
#endif	/* MSDOS */

#if	MSDOS || defined (USEFAKEPIPE)
static int NEAR openpipe(pidp, fdin, new)
p_id_t *pidp;
int fdin, new;
#else
static int NEAR openpipe(pidp, fdin, new, tty, ppid)
p_id_t *pidp;
int fdin, new, tty;
p_id_t ppid;
#endif
{
#if	!MSDOS && !defined (USEFAKEPIPE)
	p_id_t pid;
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
	if (pipe(fildes) < 0) {
# ifdef	FAKEUNINIT
		fd = -1;	/* fake for -Wuninitialized */
# endif
		pid = -1L;
	}
	else if ((pid = makechild(tty, ppid)) < 0L) {
# ifdef	FAKEUNINIT
		fd = -1;	/* fake for -Wuninitialized */
# endif
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
				pid = -1L;
			}
			else {
# ifndef	NOJOB
				stackjob(pid, 0, NULL);
# endif
				if (fildes[0] != fdin) Xdup2(fildes[0], fdin);
				pl -> old = fd;
			}
			safeclose(fildes[0]);
		}
		safeclose(fildes[1]);
	}
	if (pid >= 0L) {
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
	if ((fd = newdup(mktmpfile(pfile))) < 0) {
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
	pl -> pid = *pidp = 0L;
	pl -> next = pipetop;
	pipetop = pl;
	return(dupl);
}

static pipelist **NEAR searchpipe(fd)
int fd;
{
	pipelist **prevp;

	if (fd < 0) return(NULL);
	prevp = &pipetop;
	while (*prevp) {
		if (fd == (*prevp) -> new) break;
		prevp = &((*prevp) -> next);
	}
	if (!*prevp) return(NULL);
	return(prevp);
}

static int NEAR reopenpipe(fd, ret)
int fd, ret;
{
	pipelist *pl, **prevp;
	int dupl;

	if (!(prevp = searchpipe(fd))) return(-1);
	pl = *prevp;

	if (pl -> pid > 0L) return(fd);
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
		safermtmpfile(pl -> file);
		free(pl -> file);
		*prevp = pl -> next;
		free(pl);
		return(-1);
	}
	if (pl -> fd >= 0) {
		if ((dupl = newdup(Xdup(pl -> fd))) < 0) {
			safeclose(fd);
			safermtmpfile(pl -> file);
			free(pl -> file);
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
		safermtmpfile(pl -> file);
		free(pl -> file);
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
	int ret, duperrno;

	if (!(prevp = searchpipe(fd))) return(-1);
	duperrno = errno;
	pl = *prevp;
#if	!MSDOS && !defined (NOJOB)
	if (pl -> pid > 0L && stoppedjob(pl -> pid) > 0) {
		errno = duperrno;
		return(RET_SUCCESS);
	}
#endif

	if (pl -> old >= 0 && pl -> old != pl -> fd) {
		safeclose(pl -> fd);
		Xdup2(pl-> old, pl -> fd);
	}
	if (fd != STDIN_FILENO &&
	fd != STDOUT_FILENO && fd != STDERR_FILENO) {
		if (pl -> fp) safefclose(pl -> fp);
		else safeclose(pl -> new);
	}

	if (pl -> file) {
		if (rmtmpfile(pl -> file) < 0) doperror(NULL, pl -> file);
		free(pl -> file);
		ret = pl -> ret;
	}
#if	!MSDOS
	else if (!(pl -> pid)) {
		prepareexit(1);
		Xexit(RET_SUCCESS);
	}
	else if (pl -> pid > 0L) {
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
		if (ret == 128 + SIGPIPE) ret = RET_SUCCESS;
# endif	/* !NOJOB */
	}
#endif	/* !MSDOS */
	else ret = RET_SUCCESS;

	*prevp = pl -> next;
	free(pl);
	errno = duperrno;

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
			j = strlen(buf);
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

char *evalbackquote(arg)
char *arg;
{
	FILE *fp;
	char *buf;
	ALLOC_T len;
	int duptrapok;

	duptrapok = trapok;
	trapok = 0;
	fp = _dopopen(arg);
	trapok = duptrapok;
	if (!fp) {
		if (errno) doperror(NULL, NULL);
		buf = NULL;
		ret_status = RET_NOTEXEC;
	}
	else if (!(buf = readfile(fileno(fp), &len))) {
		doperror(NULL, NULL);
		ret_status = RET_FATALERR;
	}
	else {
#if	defined (BASHSTYLE) || defined (STRICTPOSIX)
	/* bash & POSIX ignore any following newlines */
		while (len > 0 && buf[--len] == '\n') buf[len] = '\0';
#else
		if (len > 0 && buf[--len] == '\n') buf[len] = '\0';
#endif
		ret_status = closepipe(fileno(fp));
	}
	return(buf);
}

#ifdef	NOALIAS
int checktype(s, idp, func)
char *s;
int *idp, func;
#else
int checktype(s, idp, alias, func)
char *s;
int *idp, alias, func;
#endif
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
	int i, n, len;

	*substp = (char **)malloc2((argc + 1) * sizeof(char *));
	*lenp = (int *)malloc2(argc * sizeof(int));

	for (i = n = 0; i < argc; i++) {
		len = (freeenviron || n >= i) ? identcheck(argv[i], '=') : -1;
		if (len > 0) {
			(*substp)[n] = argv[i];
			(*lenp)[n] = len;
			memmove((char *)(&(argv[i])),
				(char *)(&(argv[i + 1])),
				(argc - i) * sizeof(char *));
			n++;
			argc--;
			i--;
		}
	}
	(*substp)[n] = NULL;
	return(argc);
}

static int NEAR substvar(argv)
char **argv;
{
	char *tmp, *arg;
	int i;

	for (i = 0; argv[i]; i++) {
		trapok = 1;
		tmp = evalarg(argv[i], 0, 1, '\0');
		if (!tmp && i && *(argv[i])) {
			arg = argv[i];
			while (argv[i]) i++;
			freevar(checkshellbuiltinargv(i, duplvar(argv, 2)));
			execerror(arg, ER_BADSUBST, 0);
		}
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
	int i, id, glob;

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
#ifdef	NOALIAS
		*typep = checktype(tmp, &id, 1);
#else
		*typep = checktype(tmp, &id, 0, 1);
#endif
		free(tmp);
	}

	if (noglob) glob = 0;
#ifdef	FD
	else if (*typep == CT_FDINTERNAL) glob = 0;
#endif
	else if (*typep == CT_BUILTIN && (shbuiltinlist[id].flags & BT_NOGLOB))
		glob = 0;
	else glob = 1;

	if (glob) {
#if	!MSDOS && defined (FD) && !defined (_NOKANJIFCONV)
		if (*typep == CT_FDORIGINAL);
		else if (*typep != CT_BUILTIN) nokanjifconv++;
		else if (shbuiltinlist[id].flags & BT_NOKANJIFGET)
			nokanjifconv++;
#endif
#if	MSDOS
		comm -> argc = evalglob(comm -> argc,
			&(comm -> argv), *typep != CT_COMMAND);
#else
		comm -> argc = evalglob(comm -> argc, &(comm -> argv), 1);
#endif
#if	!MSDOS && defined (FD) && !defined (_NOKANJIFCONV)
		if (*typep == CT_FDORIGINAL);
		else if (*typep != CT_BUILTIN) nokanjifconv--;
		else if (shbuiltinlist[id].flags & BT_NOKANJIFGET)
			nokanjifconv--;
#endif
	}
	else {
		i = 0;
#if	MSDOS
		stripquote(comm -> argv[i++], 1);
		if (*typep == CT_COMMAND) while (i < comm -> argc)
			stripquote(comm -> argv[i++], 0);
		else
#endif
		while (i < comm -> argc) stripquote(comm -> argv[i++], 1);
	}
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
		execerror(comm -> argv[0], ER_COMNOFOUND, 0);
		ret_status = RET_NOTFOUND;
		return(NULL);
	}
	if (restricted && (type & CM_FULLPATH)) {
		execerror(comm -> argv[0], ER_RESTRICTED, 0);
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

static int NEAR printredirect(rp, fp)
redirectlist *rp;
FILE *fp;
{
	heredoc_t *hdp;
	int ret;

	if (!rp) return(0);
	ret = printredirect(rp -> next, fp);

	fputc(' ', fp);
	switch (rp -> type & MD_RDWR) {
		case MD_READ:
			if (rp -> fd != STDIN_FILENO) fputlong(rp -> fd, fp);
			fputc('<', fp);
			if (rp -> type & MD_HEREDOC) {
				fputc('<', fp);
				if (rp -> type & MD_APPEND) fputc('-', fp);
			}
			break;
		case MD_WRITE:
			if (rp -> fd != STDOUT_FILENO) fputlong(rp -> fd, fp);
			if (rp -> type & MD_WITHERR) fputc('&', fp);
			fputc('>', fp);
			if (rp -> type & MD_APPEND) fputc('>', fp);
#ifndef	MINIMUMSHELL
			else if (rp -> type & MD_FORCED) fputc('|', fp);
#endif
			break;
		case MD_RDWR:
			if (rp -> fd != STDOUT_FILENO) fputlong(rp -> fd, fp);
			fputs("<>", fp);
			break;
		default:
			break;
	}
	if (!(rp -> filename)) fputc('-', fp);
	else {
		if (rp -> type & MD_FILEDESC) fputc('&', fp);
		else fputc(' ', fp);
		if (!(rp -> type & MD_HEREDOC)) kanjifputs(rp -> filename, fp);
		else {
			hdp = (heredoc_t *)(rp -> filename);
#ifdef	MINIMUMSHELL
			kanjifputs(hdp -> filename, fp);
#else
			kanjifputs(hdp -> eof, fp);
#endif
			ret = 1;
		}
	}
	return(ret);
}

VOID printstree(trp, indent, fp)
syntaxtree *trp;
int indent;
FILE *fp;
#ifndef	MINIMUMSHELL
{
	redirectlist **rlist;
	int i;

	rlist = _printstree(trp, NULL, indent, fp);
	if (rlist) {
		for (i = 0; rlist[i]; i++) printheredoc(rlist[i], fp);
		free(rlist);
	}
}

static VOID NEAR printheredoc(rp, fp)
redirectlist *rp;
FILE *fp;
{
	heredoc_t *hdp;
	int c, fd;

	if (!rp) return;
	printheredoc(rp -> next, fp);
	if (!(rp -> type & MD_HEREDOC)) return;

	hdp = (heredoc_t *)(rp -> filename);
	fd = newdup(Xopen(hdp -> filename, O_BINARY | O_RDONLY, 0666));
	if (fd >= 0) {
		fputc('\n', stdout);
		while ((c = readchar(fd)) != READ_EOF) {
			if (c < 0) break;
			fputc(c, stdout);
		}
		safeclose(fd);
		kanjifputs(hdp -> eof, fp);
	}
	return;
}

static redirectlist **NEAR _printstree(trp, rlist, indent, fp)
syntaxtree *trp;
redirectlist **rlist;
int indent;
FILE *fp;
#endif	/* !MINIMUMSHELL */
{
#ifndef	MINIMUMSHELL
	redirectlist **rlist2;
	int nl;
#endif
	syntaxtree *tmptr;
	int i, j, id, prev, ind2;

#ifdef	MINIMUMSHELL
	if (!trp) return;
#else
	if (!trp) return(rlist);
#endif
	prev = getstatid(tmptr = parentstree(trp)) + 1;

#ifndef	MINIMUMSHELL
	if (isopnot(trp)) {
		fputc('!', fp);
		return(_printstree(trp -> next, rlist, indent, fp));
	}
	rlist2 = NULL;
#endif

	if (trp -> flags & ST_NODE)
#ifdef	MINIMUMSHELL
		printstree((syntaxtree *)(trp -> comm), indent, fp);
#else
		rlist2 = _printstree((syntaxtree *)(trp -> comm),
			NULL, indent, fp);
#endif
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
#ifdef	MINIMUMSHELL
				printstree(statementbody(trp), indent, fp);
#else
				rlist2 = _printstree(statementbody(trp),
					NULL, indent, fp);
#endif
				fputs(" )", fp);
			}
			else if (id == SM_STATEMENT)
#ifdef	MINIMUMSHELL
				printstree(statementbody(trp), indent, fp);
#else
				rlist2 = _printstree(statementbody(trp),
					NULL, indent, fp);
#endif
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
#ifdef	MINIMUMSHELL
				printstree(statementbody(trp), ind2, fp);
#else
				rlist2 = _printstree(statementbody(trp),
					NULL, ind2, fp);
#endif

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

#ifdef	MINIMUMSHELL
		printredirect((trp -> comm) -> redp, fp);
#else
		if (printredirect((trp -> comm) -> redp, fp)) {
			if (!rlist) i = 0;
			else for (i = 0; rlist[i]; i++);
			rlist = (redirectlist **)realloc2(rlist,
				(i + 2) * sizeof(redirectlist *));
			rlist[i++] = (trp -> comm) -> redp;
			rlist[i] = NULL;
		}
#endif
	}

#ifndef	MINIMUMSHELL
	if (rlist2) {
		if (!rlist) i = 0;
		else for (i = 0; rlist[i]; i++);
		for (j = 0; rlist2[j]; j++);
		rlist = (redirectlist **)realloc2(rlist,
			(i + j + 1) * sizeof(redirectlist *));
		for (j = 0; rlist2[j]; j++) rlist[i + j] = rlist2[j];
		rlist[i + j] = NULL;
		free(rlist2);
	}
#endif

	for (id = 0; id < OPELISTSIZ; id++)
		if (trp -> type == opelist[id].op) break;
	if (id < OPELISTSIZ && !isopfg(trp)) {
		fputc(' ', fp);
		fputs(opelist[id].symbol, fp);
	}
#ifndef	MINIMUMSHELL
	if (!rlist || opelist[id].level < 4) nl = 0;
	else {
		for (i = 0; rlist[i]; i++) printheredoc(rlist[i], fp);
		free(rlist);
		rlist = NULL;
		nl = 1;
	}
#endif

	if (trp -> next && (trp -> next) -> comm) {
#ifndef	MINIMUMSHELL
		if (nl) {
			fputc('\n', fp);
			printindent(indent, fp);
		}
		else
#endif
		if (isopfg(trp)) {
#ifdef	BASHSTYLE
	/* bash type pretty print */
			fputc(';', fp);
			printnewline(indent, fp);
#else
			if (indent < 0) fputc(';', fp);
			else {
				fputc('\n', fp);
				printindent(indent, fp);
			}
#endif
		}
		else if (id < OPELISTSIZ) {
#ifndef	BASHSTYLE
	/* bash type pretty print */
			if (opelist[id].level >= 4) printnewline(indent, fp);
			else
#endif
			fputc(' ', fp);
		}
	}
#ifndef	MINIMUMSHELL
	else if (nl);
#endif
	else if (isopfg(trp)) {
#ifdef	BASHSTYLE
	/* bash type pretty print */
		fputc(';', fp);
#else
		if (indent < 0) fputc(';', fp);
#endif
	}

#ifdef	MINIMUMSHELL
	if (trp -> next) printstree(trp -> next, indent, fp);
	fflush(fp);
#else
	if (trp -> next) rlist = _printstree(trp -> next, rlist, indent, fp);
	fflush(fp);
	return(rlist);
#endif
}

static VOID NEAR printfunction(f, fp)
shfunctable *f;
FILE *fp;
{
	kanjifputs(f -> ident, fp);
#ifdef	BASHSTYLE
	/* bash type pretty print */
	fputs(" ()\n", fp);
#else
	fputs("()", fp);
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

#if	defined (FD) || !defined (NOPOSIXUTIL)
int tinygetopt(trp, opt, nump)
syntaxtree *trp;
char *opt;
int *nump;
{
	char **argv;
	int i, n, f, argc;

	argv = (trp -> comm) -> argv;
	argc = (trp -> comm) -> argc;
	f = '\0';
	for (n = 1; n < argc; n++) {
		if (!argv[n] || argv[n][0] != '-') break;
		if (argv[n][1] == '-' && !(argv[n][2])) {
			n++;
			break;
		}

		for (i = 1; argv[n][i]; i++) {
			if (!strchr(opt, argv[n][i])) {
				execerror(argv[n], ER_BADOPTIONS, 0);
				return(-1);
			}
			f = argv[n][i];
		}
	}
	if (nump) *nump = n;
	return(f);
}
#endif	/* FD || !NOPOSIXUTIL */

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
	shellvar = duplvar(shellvar, 0);
	exportvar = duplvar(exportvar, 0);
	exportlist = duplvar(exportlist, 0);
	ronlylist = duplvar(ronlylist, 0);
	breaklevel = continuelevel = 0;
	childdepth++;
	ret = exec_stree(statementbody(trp), 0);
	childdepth--;
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
	p_id_t pid;
	int ret;

	body = statementbody(trp);
	if ((pid = makechild(1, -1L)) < 0L) return(-1);
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
		if (isopbg(trp) || isopnown(trp)) return(RET_SUCCESS);
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
	if (ret_status == RET_SUCCESS) return(exec_stree(body, 0));
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
		if (ret_status != RET_SUCCESS) break;
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
		if (ret_status == RET_SUCCESS) break;
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
	char *tmp, *ident, **argv;
	int i, argc, ret;

	var = statementbody(trp);
	body = statementbody(trp -> next);
	comm = var -> comm;

	ident = comm -> argv[0];
	if (identcheck(ident, '\0') <= 0) {
		execerror(ident, ER_NOTIDENT, 0);
		return(RET_FAIL);
	}
	trp = trp -> next;
	if ((trp -> comm) -> id != SM_IN)
		argv = duplvar(&(argvar[1]), 0);
	else {
		if (!(comm = body -> comm)) {
			errno = EINVAL;
			return(-1);
		}
		trp = trp -> next;

		argv = (char **)malloc2((comm -> argc + 1) * sizeof(char *));
		for (i = 0; i < comm -> argc; i++) {
			tmp = evalvararg(comm -> argv[i], 0, 1, '\0', 0, 0);
			if (!tmp) {
				while (--i >= 0) free(argv[i]);
				free(argv);
				return(RET_FAIL);
			}
			argv[i] = tmp;
		}
		argv[i] = NULL;
		argc = evalifs(comm -> argc, &argv, getifs());
		if (!noglob) {
#if	!MSDOS && defined (FD) && !defined (_NOKANJIFCONV)
			nokanjifconv++;
#endif
			argc = evalglob(argc, &argv, 1);
#if	!MSDOS && defined (FD) && !defined (_NOKANJIFCONV)
			nokanjifconv--;
#endif
		}
		else for (i = 0; i < argc; i++) stripquote(argv[i], 1);
#ifdef	FD
		replaceargs(NULL, NULL, NULL, 0);
		if (replaceargs(&argc, &argv, NULL, -1) < 0) {
			freevar(argv);
			return(-1);
		}
#endif
	}

	if (!(body = statementcheck(trp, SM_DO))) {
		freevar(argv);
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
	freevar(argv);
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
	if (!(key = evalvararg(comm -> argv[0], 0, 1, '\0', 1, 0)))
		return(RET_FAIL);

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
			tmp = evalvararg(comm -> argv[i], 0, 1, '\0', 0, 0);
			if (!tmp) {
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
		execerror((trp -> comm) -> argv[1], ER_BADNUMBER, 0);
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
		execerror((trp -> comm) -> argv[1], ER_BADNUMBER, 0);
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
		execerror(NULL, ER_CANNOTRET, 0);
		return(RET_FAIL);
	}
	if ((trp -> comm) -> argc <= 1) ret = ret_status;
	else if ((ret = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror((trp -> comm) -> argv[1], ER_BADNUMBER, 0);
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

	if (errexit && !path) {
#ifdef	DEBUG
		freevar(evar);
#endif
		Xexit2(RET_FAIL);
	}

	prepareexit(1);
#ifdef	DEBUG
	Xexecve(path, comm -> argv, evar, 0);
#else
	Xexecve(path, comm -> argv, evar);
#endif
	return(RET_NOTEXEC);
}

static int NEAR doexec(trp)
syntaxtree *trp;
{
	command_t *comm;
	redirectlist *rp;

	comm = trp -> comm;
	rp = comm -> redp;
	comm -> redp = NULL;
	if (freerlist(rp, 0, trp -> flags & ST_NOWN) < 0) return(RET_FAIL);
	if (comm -> argc >= 2) {
		if (restricted) {
			execerror(comm -> argv[1], ER_RESTRICTED, 0);
			return(RET_FAIL);
		}
		free(comm -> argv[0]);
		memmove((char *)(&(comm -> argv[0])),
			(char *)(&(comm -> argv[1])),
			(comm -> argc)-- * sizeof(char *));
		searchheredoc(trp, 1);
#ifdef	BASHSTYLE
	/* bash ignores the unexecutable external command */
		return(execpath(comm, 0));
#else
		return(execpath(comm, 1));
#endif
	}

	if (!isopbg(trp) && !isopnown(trp) && comm -> redp) {
		if (definput == ttyio && !isatty(STDIN_FILENO))
			definput = STDIN_FILENO;
		else if (definput == STDIN_FILENO && isatty(STDIN_FILENO))
			definput = ttyio;
	}
	return(RET_SUCCESS);
}

#ifndef	MINIMUMSHELL
static int NEAR dologin(trp)
syntaxtree *trp;
{
# if	MSDOS
	return(RET_SUCCESS);
# else	/* !MSDOS */
	searchheredoc(trp, 1);
	return(execpath(trp -> comm, 0));
# endif	/* !MSDOS */
}

static int NEAR dologout(trp)
syntaxtree *trp;
{
	if ((!loginshell && interactive_io) || exit_status < 0) {
		execerror((trp -> comm) -> argv[0], ER_NOTLOGINSH, 0);
		return(RET_FAIL);
	}
	return(doexit(trp));
}
#endif	/* !MINIMUMSHELL */

static int NEAR doeval(trp)
syntaxtree *trp;
{
	syntaxtree *stree;
	char *cp;
	int ret;

	if ((trp -> comm) -> argc <= 1
	|| !(cp = catvar(&((trp -> comm) -> argv[1]), ' ')))
		return(RET_SUCCESS);

#ifdef	BASHSTYLE
	/* bash displays arguments of "eval", in -v mode */
	if (verboseinput) {
		kanjifputs(cp, stderr);
		fputc('\n', stderr);
		fflush(stderr);
	}
#endif
	trp = stree = newstree(NULL);

#if	!MSDOS && !defined (NOJOB)
	childpgrp = -1L;
#endif
	trp = execline(cp, stree, trp, 1);
	execline((char *)-1, stree, trp, 1);
	ret = (syntaxerrno) ? RET_SYNTAXERR : ret_status;
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
		execerror((trp -> comm) -> argv[1], ER_BADNUMBER, 0);
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
		Xexit2(ret);
	}
	return(RET_SUCCESS);
}

static int NEAR doread(trp)
syntaxtree *trp;
{
	char *cp, *next, *buf, *ifs;
	ALLOC_T i, size;
	int c;

	if ((trp -> comm) -> argc <= 1) {
		execerror(NULL, ER_MISSARG, 0);
		return(RET_FAIL);
	}
	for (i = 1; i < (trp -> comm) -> argc; i++) {
		if (identcheck((trp -> comm) -> argv[i], '\0') <= 0) {
			execerror((trp -> comm) -> argv[i], ER_NOTIDENT, 0);
			return(RET_FAIL);
		}
	}

	buf = c_realloc(NULL, 0, &size);
	trapok = 1;
	readtrap = 1;
	for (i = 0;;) {
		if ((c = readchar(STDIN_FILENO)) < 0) {
			free(buf);
			readtrap = 0;
			doperror(NULL, NULL);
			return(RET_FAIL);
		}
		if (c == READ_EOF || c == '\n') break;
		buf = c_realloc(buf, i, &size);
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
	int i, n, ret;

	if ((trp -> comm) -> argc <= 1) n = 1;
	else if ((n = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror((trp -> comm) -> argv[1], ER_BADNUMBER, 0);
		return(RET_FAIL);
	}
	else if (!n) return(RET_SUCCESS);

	for (i = 0; i < n; i++) {
		if (!argvar[i + 1]) break;
		free(argvar[i + 1]);
	}
	ret = (i >= n) ? RET_SUCCESS : RET_FAIL;
	n = i;
	for (i = 0; argvar[i + n + 1]; i++) argvar[i + 1] = argvar[i + n + 1];
	argvar[i + 1] = NULL;
	if (ret != RET_SUCCESS) execerror(NULL, ER_CANNOTSHIFT, 0);
	return(ret);
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
		i = countvar(var);
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
				fputstr(optionflags[i], 16, stdout);
				fputs((*(setvals[i])) ? "on" : "off", stdout);
				fputc('\n', stdout);
			}
			fflush(stdout);
		}
#ifndef	MINIMUMSHELL
		else if (!strpathcmp(argv[2], "ignoreeof"))
			ignoreeof = (n <= 3) ? 1 : 0;
#endif
#ifdef	FD
		else if (!strpathcmp(argv[2], "physical"))
			physical_path = (n <= 3) ? 1 : 0;
#endif
#if	defined (FD) && !defined (_NOEDITMODE)
		else if (!strpathcmp(argv[2], "vi")
		|| !strpathcmp(argv[2], "emacs")) {
			extern char *editmode;

			setenv2("FD_EDITMODE", argv[2]);
			editmode = getconstvar("FD_EDITMODE");
		}
#endif
		else {
			for (i = 0; i < FLAGSSIZ; i++) {
				if (!optionflags[i]) continue;
				if (!strpathcmp(argv[2], optionflags[i]))
					break;
			}
			if (i >= FLAGSSIZ) {
				execerror(argv[2], ER_BADOPTIONS, 0);
				return(RET_FAIL);
			}
			*(setvals[i]) = (n <= 3) ? 1 : 0;
		}
		n = 3;
	}
	if (n >= argc) return(RET_SUCCESS);

	var = argvar;
	argvar = (char **)malloc2((argc - n + 2) * sizeof(char *));
	argvar[0] = strdup2(var[0]);
	for (i = 1; n < argc; i++, n++) argvar[i] = strdup2(argv[n]);
	argvar[i] = NULL;
	freevar(var);

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
			execerror((trp -> comm) -> argv[i], ER_NOTFOUND, 0);
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
#ifdef	FD
	int opt, dupphysical_path;
#endif
	char *cp, *tmp, *dir, *path, *next;
	int n, dlen, len, size;

#ifdef	FD
	if ((opt = tinygetopt(trp, "LP", &n)) < 0) return(RET_FAIL);
#else
	n = 1;
#endif	/* !FD */

	if (n < (trp -> comm) -> argc) dir = (trp -> comm) -> argv[n];
	else if (!(dir = getconstvar("HOME"))) {
		execerror((trp -> comm) -> argv[0], ER_NOHOMEDIR, 0);
		return(RET_FAIL);
	}
	else if (!*dir) return(RET_SUCCESS);

#ifdef	FD
	dupphysical_path = physical_path;
	if (opt) physical_path = (opt == 'P') ? 1 : 0;
#endif

	if (*dir == _SC_ || !(next = getconstvar("CDPATH"))) {
		if (chdir3(dir) >= 0) {
#ifdef	FD
			physical_path = dupphysical_path;
#endif
			return(RET_SUCCESS);
		}
	}
	else {
		len = strlen(dir);
		size = 0;
		path = NULL;
		for (cp = next; cp; cp = next) {
#if	MSDOS || (defined (FD) && !defined (_NODOSDRIVE))
			if (_dospath(cp)) next = strchr(&(cp[2]), PATHDELIM);
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
			strncpy2(&(path[dlen]), dir, len);
			if (chdir3(path) >= 0) {
				kanjifputs(path, stdout);
				fputc('\n', stdout);
				fflush(stdout);
				free(path);
#ifdef	FD
				physical_path = dupphysical_path;
#endif
				return(RET_SUCCESS);
			}
		}
		if (path) free(path);
	}
	execerror(dir, ER_BADDIR, 0);
#ifdef	FD
	physical_path = dupphysical_path;
#endif
	return(RET_FAIL);
}

/*ARGSUSED*/
static int NEAR dopwd(trp)
syntaxtree *trp;
{
	char buf[MAXPATHLEN];
#ifdef	FD
	int opt;
#endif

#ifdef	FD
	if ((opt = tinygetopt(trp, "LP", NULL)) < 0) return(RET_FAIL);

	if (!((opt) ? (opt == 'P') : physical_path)) strcpy(buf, fullpath);
	else
#endif	/* FD */
	if (!Xgetwd(buf)) {
		execerror((trp -> comm) -> argv[0], ER_CANNOTSTAT, 0);
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
		execerror(fname, ER_RESTRICTED, 0);
		return(RET_FAIL);
	}
	if (n & CM_NOTFOUND) {
#if	defined (BASHSTYLE) && defined (STRICTPOSIX)
		execerror(fname, ER_NOTFOUND, -1);
#else
		execerror(fname, ER_NOTFOUND, 0);
#endif
		return(RET_FAIL);
	}
	if ((fd = newdup(Xopen(fname, O_BINARY | O_RDONLY, 0666))) < 0) {
		doperror((trp -> comm) -> argv[0], fname);
		return(RET_FAIL);
	}
#ifdef	BASHSTYLE
	/* bash sets positional parameters with source arguments */
	if ((trp -> comm) -> argc > 2) {
		char **var, **dupargvar;

		dupargvar = argvar;
		argvar = (char **)malloc2((trp -> comm) -> argc
			* sizeof(char *));
		argvar[0] = strdup2(var[0]);
		for (n = 1; n < (trp -> comm) -> argc - 1; n++)
			argvar[n] = strdup2((trp -> comm) -> argv[n + 1]);
		argvar[n] = NULL;
		var = argvar;
		sourcefile(fd, fname, 0);
		if (var != argvar) freevar(dupargvar);
		else {
			freevar(argvar);
			argvar = dupargvar;
		}
	}
	else
#endif	/* BASHSTYLE */
	sourcefile(fd, fname, 0);
	return(ret_status);
}

#ifdef	MINIMUMSHELL
#define	VALIDEQUAL	'\0'
#else
#define	VALIDEQUAL	'='
#endif

static int NEAR doexport(trp)
syntaxtree *trp;
{
#ifndef	MINIMUMSHELL
	char *tmp;
#endif
	char **argv;
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
		if (!(len = identcheck(argv[n], VALIDEQUAL))) {
			execerror(argv[n], ER_NOTIDENT, 0);
			ret = RET_FAIL;
			ERRBREAK;
		}
#ifndef	MINIMUMSHELL
		else if (len < 0) len = -len;
		else if (_putshellvar(tmp = strdup2(argv[n]), len) < 0) {
			free(tmp);
			ret = RET_FAIL;
			ERRBREAK;
		}
#endif

		for (i = 0; exportlist[i]; i++)
			if (!strnpathcmp(argv[n], exportlist[i], len)
			&& !exportlist[i][len]) break;
		if (!exportlist[i]) {
			exportlist = (char **)realloc2(exportlist,
				(i + 2) * sizeof(char *));
			exportlist[i] = strdupcpy(argv[n], len);
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
#ifndef	MINIMUMSHELL
	char *tmp;
#endif
	char **argv;
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
		if (!(len = identcheck(argv[n], VALIDEQUAL))) {
			execerror(argv[n], ER_NOTIDENT, 0);
			ret = RET_FAIL;
			ERRBREAK;
		}
#ifndef	MINIMUMSHELL
		else if (len < 0) len = -len;
		else if (putshellvar(tmp = strdup2(argv[n]), len) < 0) {
			free(tmp);
			ret = RET_FAIL;
			ERRBREAK;
		}
#endif

		for (i = 0; ronlylist[i]; i++)
			if (!strnpathcmp(argv[n], ronlylist[i], len)
			&& !ronlylist[i][len]) break;
		if (!ronlylist[i]) {
			ronlylist = (char **)realloc2(ronlylist,
				(i + 2) * sizeof(char *));
			ronlylist[i] = strdupcpy(argv[n], len);
			ronlylist[++i] = NULL;
		}
	}
	return(ret);
}

/*ARGSUSED*/
static int NEAR dotimes(trp)
syntaxtree *trp;
{
	int usrtime, systime;
#ifdef	USERESOURCEH
	struct rusage ru;

	getrusage(RUSAGE_CHILDREN, &ru);
	usrtime = ru.ru_utime.tv_sec;
	systime = ru.ru_stime.tv_sec;
#else	/* !USERESOURCEH */
# ifdef	USETIMESH
	struct tms buf;

	times(&buf);
	usrtime = buf.tms_cutime / CLK_TCK;
	if (buf.tms_cutime % CLK_TCK > CLK_TCK / 2) usrtime++;
	systime = buf.tms_cstime / CLK_TCK;
	if (buf.tms_cstime % CLK_TCK > CLK_TCK / 2) systime++;
# else
	usrtime = systime = 0;
# endif
#endif	/* !USERESOURCEH */
	fputlong(usrtime / 60, stdout);
	fputc('m', stdout);
	fputlong(usrtime % 60, stdout);
	fputs("s ", stdout);
	fputlong(systime / 60, stdout);
	fputc('m', stdout);
	fputlong(systime % 60, stdout);
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
	p_id_t pid;
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
			if ((i = getjob(s)) < 0) pid = -1L;
			else {
				j = joblist[i].npipe;
				pid = joblist[i].pids[j];
			}
		}
		else
# endif	/* !NOJOB */
		if ((pid = isnumeric(s)) < 0L) {
			execerror(s, ER_BADNUMBER, 0);
			return(RET_FAIL);
		}
# ifndef	NOJOB
		else {
			checkjob(0);
			if (!joblist || (i = searchjob(pid, &j)) < 0L) {
				pid = -1L;
#  ifdef	FAKEUNINIT
				i = -1;		/* fake for -Wuninitialized */
#  endif
			}
		}

		if (pid < 0L) {
			execerror(s, ER_NOSUCHJOB, 0);
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
#ifdef	BASHSTYLE
		fputoctal(n, 3, stdout);
#else
		fputoctal(n, 4, stdout);
#endif
		fputc('\n', stdout);
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
	execerror(NULL, ER_BADULIMIT, 0);
	return(RET_FAIL);
#else	/* USERESOURCEH || USEULIMITH */
# ifdef	USERESOURCEH
	struct rlimit lim;
	FILE *fp;
	u_long flags;
	int j, n, err, hs, res, inf;
# endif
	long val;
	char **argv;
	int i, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;
# ifdef	USERESOURCEH
	flags = (u_long)0;
	err = hs = 0;
	n = 1;
	if (argc > 1 && argv[1][0] == '-') {
		for (i = 1; !err && argv[1][i]; i++) switch (argv[1][i]) {
			case 'a':
				flags = ((u_long)1 << ULIMITSIZ) - 1;
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
				else flags |= ((u_long)1 << j);
				break;
		}
		n++;
	}
	res = -1;
	if (!flags)
	for (i = 0, flags = (u_long)1; i < ULIMITSIZ; i++, flags <<= 1) {
		if (ulimitlist[i].res == RLIMIT_FSIZE) {
			res = i;
			break;
		}
	}
	else for (i = 0; i < ULIMITSIZ; i++) {
		if (flags & ((u_long)1 << i)) {
			if (res < 0) res = i;
			else {
				res = -1;
				break;
			}
		}
	}

#  ifdef	FAKEUNINIT
	val = 0L;	/* fake for -Wuninitialized */
#  endif
	if (n >= argc) {
#  ifdef	FAKEUNINIT
		inf = 0;	/* fake for -Wuninitialized */
#  endif
		if (!hs) hs = 2;
	}
	else {
		inf = 0;
		if (!hs) hs = (1 | 2);
		if (res < 0) err = 1;
		else if (!strcmp(argv[n], UNLIMITED)) inf = 1;
		else {
			if ((val = isnumeric(argv[n])) < 0L) {
				execerror(argv[n], ER_BADULIMIT, 0);
				return(RET_FAIL);
			}
			val *= ulimitlist[res].unit;
		}
	}
	if (err) {
#  ifdef	BASHSTYLE
		fp = stderr;
#  else
		fp = stdout;
#  endif
		fputs("usage: ", fp);
		kanjifputs(argv[0], fp);
		fputs(" [ -HSa", fp);
		for (j = 0; j < ULIMITSIZ; j++)
			fputc(ulimitlist[j].opt, fp);
		fputs(" ] [ limit ]\n", fp);
		fflush(fp);
#  ifdef	BASHSTYLE
		return(RET_SYNTAXERR);
#  else
		return(RET_SUCCESS);
#  endif
	}

	if (n >= argc) {
		for (i = 0; i < ULIMITSIZ; i++) if (flags & ((u_long)1 << i)) {
			if (res < 0) {
				fputs(ulimitlist[i].mes, stdout);
				fputc(' ', stdout);
			}
			if (getrlimit(ulimitlist[i].res, &lim) < 0) {
				execerror(NULL, ER_BADULIMIT, 0);
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
			execerror(NULL, ER_BADULIMIT, 0);
			return(RET_FAIL);
		}
		if (hs & 1) lim.rlim_max = (inf) ? RLIM_INFINITY : val;
		if (hs & 2) lim.rlim_cur = (inf) ? RLIM_INFINITY : val;
		if (setrlimit(ulimitlist[res].res, &lim) < 0) {
			execerror(NULL, ER_BADULIMIT, 0);
			return(RET_FAIL);
		}
	}
# else	/* !USERESOURCEH */
	if (argc <= 1) {
		if ((val = ulimit(UL_GETFSIZE, 0L)) < 0L) {
			execerror(NULL, ER_BADULIMIT, 0);
			return(RET_FAIL);
		}
		if (val == RLIM_INFINITY) fputs(UNLIMITED, stdout);
		else fputlong(val * 512L, stdout);
		fputc('\n', stdout);
	}
	else {
		if (!strcmp(argv[1], UNLIMITED)) val = RLIM_INFINITY;
		else {
			if ((val = isnumeric(argv[1])) < 0L) {
				execerror(argv[1], ER_BADULIMIT, 0);
				return(RET_FAIL);
			}
			val /= 512L;
		}
		if (ulimit(UL_SETFSIZE, val) < 0L) {
			execerror(NULL, ER_BADULIMIT, 0);
			return(RET_FAIL);
		}
	}
# endif	/* !USERESOURCEH */
	fflush(stdout);
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
			execerror(argv[n], ER_BADNUMBER, 0);
			return(RET_FAIL);
		}
		if (sig >= NSIG) {
			execerror(argv[n], ER_BADTRAP, 0);
			return(RET_FAIL);
		}

		if (!sig) {
			trapmode[0] = (comm) ? TR_TRAP : 0;
			if (trapcomm[0]) free(trapcomm[0]);
			trapcomm[0] = strdup2(comm);
			continue;
		}

		for (i = 0; signallist[i].sig >= 0; i++)
			if (sig == signallist[i].sig) break;
		if (signallist[i].sig < 0
		|| (signallist[i].flags & TR_NOTRAP)) {
			execerror(argv[n], ER_BADTRAP, 0);
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

#if	!MSDOS && !defined (NOJOB)
/*ARGSUSED*/
static int NEAR dojobs(trp)
syntaxtree *trp;
{
	return(posixjobs(trp));
}

/*ARGSUSED*/
static int NEAR dofg(trp)
syntaxtree *trp;
{
	return(posixfg(trp));
}

/*ARGSUSED*/
static int NEAR dobg(trp)
syntaxtree *trp;
{
	return(posixbg(trp));
}

/*ARGSUSED*/
static int NEAR dodisown(trp)
syntaxtree *trp;
{
	char *s;
	int i;

	s = ((trp -> comm) -> argc > 1) ? (trp -> comm) -> argv[1] : NULL;
	checkjob(0);
	if ((i = getjob(s)) < 0) {
		execerror((trp -> comm) -> argv[1], ER_NOSUCHJOB, 0);
		return(RET_FAIL);
	}
	free(joblist[i].pids);
	free(joblist[i].stats);
	if (joblist[i].trp) {
		freestree(joblist[i].trp);
		free(joblist[i].trp);
	}
	joblist[i].pids = NULL;
	return(RET_SUCCESS);
}
#endif	/* !MSDOS && !NOJOB */

int typeone(s, fp)
char *s;
FILE *fp;
{
	hashlist *hp;
	int type, id;

	kanjifputs(s, fp);
#ifdef	NOALIAS
	type = checktype(s, &id, 1);
#else
	type = checktype(s, &id, 1, 1);
#endif

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

static int NEAR dodtype(trp)
syntaxtree *trp;
{
	return(doscomtype((trp -> comm) -> argc, (trp -> comm) -> argv));
}
#endif	/* DOSCOMMAND */

#ifndef	NOALIAS
static int NEAR doalias(trp)
syntaxtree *trp;
{
	return(posixalias(trp));
}

static int NEAR dounalias(trp)
syntaxtree *trp;
{
	return(posixunalias(trp));
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

#ifndef	MINIMUMSHELL
static int NEAR dokill(trp)
syntaxtree *trp;
{
	return(posixkill(trp));
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
	if (!ret && ptr < argc) ret = -1;
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
#endif	/* !MINIMUMSHELL */

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
	return(posixcommand(trp));
}

static int NEAR dogetopts(trp)
syntaxtree *trp;
{
	return(posixgetopts(trp));
}

static int NEAR donewgrp(trp)
syntaxtree *trp;
{
#if	MSDOS
	return(RET_SUCCESS);
#else	/* !MSDOS */
	searchheredoc(trp, 1);
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

#ifndef	MINIMUMSHELL
static int NEAR getworkdir(path)
char *path;
{
# ifdef	FD
	if (underhome(&(path[1]))) path[0] = '~';
	else strcpy(path, fullpath);
# else
	if (!Xgetwd(path)) {
		execerror(NULL, ER_CANNOTSTAT, 0);
		return(-1);
	}
# endif
	return(0);
}

static int NEAR dopushd(trp)
syntaxtree *trp;
{
	char *cp, path[MAXPATHLEN];
	int i;

	if (getworkdir(path) < 0) return(RET_FAIL);
	if ((trp -> comm) -> argc < 2) {
		if (!dirstack || !dirstack[0]) {
			execerror(NULL, ER_DIREMPTY, 0);
			return(RET_FAIL);
		}
		cp = evalpath(strdup2(dirstack[0]), 1);
		i = chdir3(cp);
		free(cp);
		if (i < 0) {
			execerror(dirstack[0], ER_BADDIR, 0);
			return(RET_FAIL);
		}
		free(dirstack[0]);
	}
	else {
		if (chdir3((trp -> comm) -> argv[1]) < 0) {
			execerror((trp -> comm) -> argv[1], ER_BADDIR, 0);
			return(RET_FAIL);
		}
		i = countvar(dirstack);
		dirstack = (char **)realloc2(dirstack,
			(i + 1 + 1) * sizeof(char *));
		memmove((char *)(&(dirstack[1])), (char *)(&(dirstack[0])),
			i * sizeof(char *));
		dirstack[i + 1] = NULL;
	}
	dirstack[0] = strdup2(path);
	dodirs(trp);
	return(RET_SUCCESS);
}

static int NEAR dopopd(trp)
syntaxtree *trp;
{
	char *cp;
	int i;

	if (!dirstack || !dirstack[0]) {
		execerror(NULL, ER_DIREMPTY, 0);
		return(RET_FAIL);
	}
	cp = evalpath(strdup2(dirstack[0]), 1);
	i = chdir3(cp);
	free(cp);
	if (i < 0) {
		execerror(dirstack[0], ER_BADDIR, 0);
		return(RET_FAIL);
	}
	free(dirstack[0]);
	for (i = 0; dirstack[i + 1]; i++) dirstack[i] = dirstack[i + 1];
	dirstack[i] = NULL;
	dodirs(trp);
	return(RET_SUCCESS);
}

/*ARGSUSED*/
static int NEAR dodirs(trp)
syntaxtree *trp;
{
	char path[MAXPATHLEN];
	int i;

	if (getworkdir(path) < 0) return(RET_FAIL);
	kanjifputs(path, stdout);
	if (dirstack) for (i = 0; dirstack[i]; i++) {
		fputc(' ', stdout);
		kanjifputs(dirstack[i], stdout);
	}
	fputc('\n', stdout);
	fflush(stdout);

	return(RET_SUCCESS);
}
#endif	/* !MINIMUMSHELL */

#ifdef	FD
static int NEAR dofd(trp)
syntaxtree *trp;
{
	int mode;

	if (!shellmode || exit_status < 0) {
		execerror((trp -> comm) -> argv[0], ER_RECURSIVEFD, 0);
		return(RET_FAIL);
	}
	else {
		sigvecset(1);
		ttyiomode(0);
		mode = termmode(1);
		shellmode = 0;
		main_fd((trp -> comm) -> argv[1]);
		shellmode = 1;
		termmode(mode);
		stdiomode();
		sigvecset(0);
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

#ifndef	FDSH
char **getsimpleargv(trp)
syntaxtree *trp;
{
	char **argv;

	if ((trp -> flags & ST_NODE) || trp -> cont) return(NULL);
	if (!(trp -> comm) || isstatement(trp -> comm)) return(NULL);
	argv = (trp -> comm) -> argv;
	while (trp -> next) {
		trp = trp -> next;
		if ((trp -> flags & ST_NODE) || trp -> comm) return(NULL);
	}
	return(argv);
}
#endif	/* !FDSH */

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
		printindent(lvl, stdout);
		if (isstatement(trp -> comm)) {
			id = (trp -> comm) -> id;
			i = id - 1;
			if (id == SM_CHILD) fputs("sub shell", stdout);
			else if ((id == SM_STATEMENT))
				fputs("statement", stdout);
			else fputs(statementlist[i].ident, stdout);
			fputs(":\n", stdout);
			show_stree(statementbody(trp), lvl + 1);
		}
		else for (i = 0; i <= (trp -> comm) -> argc; i++) {
			if (!((trp -> comm) -> argv[i])) fputs("NULL", stdout);
			else if (!i && isbuiltin(trp -> comm))
				kanjifputs((trp -> comm) -> argv[i], stdout);
			else {
				fputc('"', stdout);
				kanjifputs((trp -> comm) -> argv[i], stdout);
				fputc('"', stdout);
			}
			fputc((i < (trp -> comm) -> argc) ? ' ' : '\n',
				stdout);
		}
		for (rp = (trp -> comm) -> redp; rp; rp = rp -> next) {
			printindent(lvl, stdout);
			fputs("redirect ", stdout);
			fputlong(rp -> fd, stdout);
			if (!(rp -> filename)) fputs(">-: ", stdout);
			else if (rp -> type & MD_HEREDOC) {
				fputs(">> \"", stdout);
				kanjifputs(((heredoc_t *)(rp -> filename))
					-> eof, stdout);
				fputs("\": ", stdout);
			}
			else {
				fputs("> \"", stdout);
				kanjifputs(rp -> filename, stdout);
				fputs("\": ", stdout);
			}
			fputoctal(rp -> type, 6, stdout);
			fputc('\n', stdout);
		}
	}
	if (trp -> type) {
		for (i = 0; i < OPELISTSIZ; i++)
			if (trp -> type == opelist[i].op) break;
		if (i < OPELISTSIZ) {
			printindent(lvl, stdout);
			fputs(opelist[i].symbol, stdout);
			fputc('\n', stdout);
		}
	}
	if (trp -> next) {
		if (trp -> cont & (CN_QUOT | CN_META)) {
			rp = (redirectlist *)(trp -> next);
			printindent(lvl, stdout);
			fputs("continuing...\"", stdout);
			fputs(rp -> filename, stdout);
			fputs("\"\n", stdout);
		}
# ifndef	MINIMUMSHELL
		else if (trp -> flags & ST_BUSY);
# endif
		else show_stree(trp -> next, lvl);
	}
	fflush(stdout);
}
#endif	/* SHOWSTREE */

static int NEAR setfunc(ident, trp)
char *ident;
syntaxtree *trp;
{
	syntaxtree *functr;
	int i, len;

	len = strlen(ident);
#ifndef	BASHSTYLE
	/* bash distinguishes the same named function and variable */
	if (unset(ident, len) < 0) return(RET_FAIL);
#endif

	trp = trp -> next;
	if (!(functr = statementbody(trp))) return(RET_FAIL);
	(trp -> comm) -> argv = (char **)duplstree(functr, trp);
	functr -> parent = NULL;
	functr -> flags |= ST_TOP;

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
	shellfunc[i].func = functr;
#ifndef	_NOUSEHASH
	if (hashahead) check_stree(functr);
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

#if	MSDOS
int exec_simplecom(trp, type, id)
syntaxtree *trp;
int type, id;
#else
int exec_simplecom(trp, type, id, bg)
syntaxtree *trp;
int type, id, bg;
#endif
{
#if	!MSDOS
	p_id_t pid;
#endif
	command_t *comm;
	char *path;
	int ret;

	comm = trp -> comm;

	if (type == CT_NONE) return(ret_status);
	if (type == CT_BUILTIN) {
		if (!shbuiltinlist[id].func) return(RET_FAIL);
		if (!restricted || !(shbuiltinlist[id].flags & BT_RESTRICT))
			return((*shbuiltinlist[id].func)(trp));
		execerror(comm -> argv[0], ER_RESTRICTED, 0);
		return(RET_FAIL);
	}
#ifdef	FD
	if (type == CT_FDORIGINAL)
		return(execbuiltin(id, comm -> argc, comm -> argv));
	if (type == CT_FDINTERNAL) {
		if (shellmode) return(RET_FAIL);
		resetsignal(0);
		ret = execinternal(id, comm -> argc, comm -> argv);
		setsignal();
		return(ret);
	}
#endif
#if	MSDOS
	if (type == CT_LOGDRIVE) {
		if (setcurdrv(id, 1) >= 0) return(RET_SUCCESS);
		execerror(comm -> argv[0], ER_INVALDRIVE, 0);
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
		execerror(comm -> argv[0], ER_COMNOFOUND, 0);
		return(RET_NOTFOUND);
	}
#else	/* !MSDOS */
# ifndef	BASHSTYLE
	/* bash does fork() all processes even in a sub shell */
	if (childdepth && !(trp -> next)) bg = 1;
# endif
	if (bg || isopbg(trp) || isopnown(trp)) {
		searchheredoc(trp, 1);
# ifdef	DEBUG
		Xexecve(path, comm -> argv, exportvar, 1);
# else
		Xexecve(path, comm -> argv, exportvar);
# endif
	}

	if ((pid = makechild(1, -1L)) < 0L) return(-1);
	if (!pid)
# ifdef	DEBUG
		Xexecve(path, comm -> argv, exportvar, 1);
# else
		Xexecve(path, comm -> argv, exportvar);
# endif
	ret = waitchild(pid, trp);
#endif	/* !MSDOS */

	return(ret);
}

static char **NEAR checkshellbuiltinargv(argc, argv)
int argc;
char **argv;
{
	char *tmp;
	int id, type;

	isshellbuiltin = 0;
#ifdef	BASHSTYLE
	/* bash does not use IFS as a command separator */
	argc = evalifs(argc, &argv, " \t");
#else
	argc = evalifs(argc, &argv, getifs());
#endif
	if (!argc) return(argv);

	stripquote(tmp = strdup2(argv[0]), 1);
#ifdef	NOALIAS
	type = checktype(tmp, &id, 1);
#else
	type = checktype(tmp, &id, 0, 1);
#endif
	free(tmp);
#ifdef	STRICTPOSIX
	if (type == CT_BUILTIN && (shbuiltinlist[id].flags & BT_POSIXSPECIAL))
		isshellbuiltin = 1;
#else
	if (type != CT_NONE && type != CT_COMMAND) isshellbuiltin = 1;
#endif
	return(argv);
}

static int NEAR checkshellbuiltin(trp)
syntaxtree *trp;
{
	syntaxtree *next;
	char **argv, **subst;
	int ret, *len, argc, duperrno;

	duperrno = errno;
	isshellbuiltin = 0;
	if (!trp || !(trp -> comm) || isstatement(trp -> comm)) return(0);
	next = statementcheck(trp -> next, SM_STATEMENT);
	if (getstatid(next) == SM_LPAREN - 1) {
		errno = duperrno;
		return(0);
	}

	argv = duplvar((trp -> comm) -> argv, 2);
	argc = getsubst((trp -> comm) -> argc, argv, &subst, &len);

	if (substvar(argv) < 0) ret = -1;
	else {
		argv = checkshellbuiltinargv(argc, argv);
		ret = 0;
	}

	freevar(subst);
	free(len);
	freevar(argv);
	errno = duperrno;
	return(ret);
}

#if	MSDOS
static int NEAR exec_command(trp)
syntaxtree *trp;
#else
static int NEAR exec_command(trp, bg)
syntaxtree *trp;
int bg;
#endif
{
#ifndef	_NOUSEHASH
	hashlist **htable;
	char *pathvar;
#endif
	command_t *comm;
	char **argv, **subst, **svar, **evar;
	long esize;
	int i, ret, id, type, argc, nsubst, keepvar, *len;

	isshellbuiltin = execerrno = 0;
	comm = trp -> comm;
	argc = comm -> argc;
	argv = comm -> argv;
	if (interrupted) return(RET_INTR);
	if (argc <= 0) return(ret_status);

	type = 0;
	comm -> argv = duplvar(comm -> argv, 2);
	comm -> argc = getsubst(comm -> argc, comm -> argv, &subst, &len);

	id = evalargv(comm, &type, 1);
	if (subst[0] && type == CT_NONE) ret_status = RET_SUCCESS;

	if (id < 0 || (nsubst = substvar(subst)) < 0) {
		freevar(subst);
		free(len);
		comm -> argc = argc;
		freevar(comm -> argv);
		comm -> argv = argv;
		return(RET_FAIL);
	}

	keepvar = 0;
#ifdef	BASHSTYLE
	/* bash treats the substitution before builtin as ineffective */
	if (type == CT_BUILTIN) keepvar = -1;
	else if (type != CT_NONE) keepvar = 1;
#else
	if (type == CT_COMMAND) keepvar = 1;
	/* bash treats the substitution before a function as effective */
	else if (type == CT_FUNCTION) keepvar = -1;
#endif
#ifdef	STRICTPOSIX
	if (type == CT_BUILTIN) {
		if (shbuiltinlist[id].flags & BT_POSIXSPECIAL)
			isshellbuiltin = 1;
		else keepvar = -1;
	}
#else
	if (type != CT_NONE && type != CT_COMMAND) isshellbuiltin = 1;
#endif

	if (keepvar < 0) {
		for (i = 0; subst[i]; i++) free(subst[i]);
		subst[0] = NULL;
		keepvar = nsubst = 0;
	}

	if (keepvar) {
		shellvar = duplvar(svar = shellvar, 0);
		exportvar = duplvar(evar = exportvar, 0);
		esize = exportsize;
#ifndef	_NOUSEHASH
		htable = duplhash(hashtable);
		pathvar = getconstvar("PATH");
#endif
	}
#ifdef	FAKEUNINIT
	else {
		/* fake for -Wuninitialized */
		svar = evar = NULL;
		esize = 0L;
# ifndef	_NOUSEHASH
		htable = NULL;
		pathvar = NULL;
# endif
	}
#endif

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
#ifdef	BASHSTYLE
	/* bash displays "+ " with substitutions, in -x mode */
			fputs("+ ", stderr);
#endif
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

#ifdef	FD
	replaceargs(NULL, NULL, NULL, 0);
#endif
	for (;;) {
#ifdef	FD
		char **dupargv;
		int dupargc;

		dupargv = comm -> argv;
		dupargc = comm -> argc;
		comm -> argv = duplvar(comm -> argv, 2);
		i = replaceargs(&(comm -> argc), &(comm -> argv),
			exportvar, type == CT_COMMAND);
		if (i < 0) {
			ret = i;
			break;
		}
#endif
#if	MSDOS
		ret = exec_simplecom(trp, type, id);
#else
		ret = exec_simplecom(trp, type, id, bg);
#endif
#ifdef	FD
		freevar(comm -> argv);
		comm -> argv = dupargv;
		comm -> argc = dupargc;
		if (i) continue;
#endif
		break;
	}

	if (keepvar) {
#ifndef	_NOUSEHASH
		if (pathvar != getconstvar("PATH")) {
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

#if	MSDOS || defined (USEFAKEPIPE)
static int NEAR exec_process(trp)
syntaxtree *trp;
#else
static int NEAR exec_process(trp, pipein)
syntaxtree *trp;
p_id_t pipein;
#endif
{
#if	!MSDOS
	syntaxtree *tmptr;
	p_id_t pid;
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
		&& (tmptr = getparent(trp)) && isoppipe(tmptr)) ? 1 : 0;
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
		if ((pid = makechild(1, -1L)) < 0L) return(-1);
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
#if	MSDOS
	else ret = exec_command(trp);
#else	/* !MSDOS */
	else if (sub) ret = exec_command(trp, 1);
# ifndef	USEFAKEPIPE
	else if (!pipein) ret = exec_command(trp, 1);
# endif
# ifndef	NOJOB
	else if (ttypgrp < 0L && !(trp -> next))
		ret = exec_command(trp, 1);
# endif
	else ret = exec_command(trp, 0);
#endif	/* !MSDOS */

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
	p_id_t pid;
#endif
	syntaxtree *tmptr;
	redirectlist *errp;
	p_id_t pipein;
	int ret, fd;

	exectrapcomm();
	if (breaklevel || continuelevel) {
		ret_status = RET_SUCCESS;
		return(ret_status);
	}

#ifndef	MINIMUMSHELL
	if (isopnot(trp)) {
		trp = skipfuncbody(trp);
		if (!(trp -> next)) return(RET_FAIL);
		if ((ret = exec_stree(trp -> next, cond)) < 0) return(-1);
		ret_status =
			(ret_status != RET_SUCCESS) ? RET_SUCCESS : RET_FAIL;
		return(ret_status);
	}
#endif

#if	!MSDOS
	if (!isopbg(trp) && !isopnown(trp)) pid = -1L;
	else if ((pid = makechild(0, -1L)) < 0L) return(-1);
	else if (!pid) {
# ifndef	NOJOB
		ttypgrp = -1L;
		if (!isopnown(trp)) stackjob(mypid, 0, trp);
		if (jobok);
		else
# endif
		if ((fd = Xopen(NULLDEVICE, O_BINARY | O_RDONLY, 0666)) >= 0) {
			Xdup2(fd, STDIN_FILENO);
			safeclose(fd);
		}
	}
	else {
		nownstree(trp);
# ifndef	NOJOB
		childpgrp = -1L;
		if (!isopnown(trp)) {
			prevjob = lastjob;
			lastjob = stackjob(pid, 0, trp);
		}
# endif
		if (interactive && !nottyout) {
# ifndef	NOJOB
			if (jobok && !isopnown(trp)) {
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
# endif	/* !NOJOB */
			{
				fputlong(pid, stderr);
				fputc('\n', stderr);
				fflush(stderr);
			}
		}
		lastpid = pid;
		trp = skipfuncbody(trp);
		ret_status = RET_SUCCESS;
		if (!(trp -> next)) return(ret_status);
		return(exec_stree(trp -> next, cond));
	}
#endif	/* !MSDOS */

	tmptr = skipfuncbody(trp);
	if (!isoppipe(tmptr)) {
#ifdef	FAKEUNINIT
		fd = -1;	/* fake for -Wuninitialized */
#endif
		pipein = -1L;
	}
#if	MSDOS || defined (USEFAKEPIPE)
	else if ((fd = openpipe(&pipein, STDIN_FILENO, 0)) < 0) return(-1);
#else
	else if ((fd = openpipe(&pipein, STDIN_FILENO, 0, 1, -1L)) < 0)
		return(-1);
#endif

	if (!(trp -> comm) || pipein > 0L) {
#if	!MSDOS && !defined (NOJOB)
		if (pipein > 0L) stackjob(pipein, 0, trp);
#endif
		ret = ret_status;
	}
	else if (trp -> flags & ST_NODE)
		ret = exec_stree((syntaxtree *)(trp -> comm), cond);
	else if (!((trp -> comm) -> redp)) {
#if	MSDOS || defined (USEFAKEPIPE)
		ret = exec_process(trp);
#else
		ret = exec_process(trp, pipein);
#endif
		ret_status = (ret >= 0) ? ret : RET_FAIL;
	}
	else {
		if (!(errp = doredirect((trp -> comm) -> redp)))
#if	MSDOS || defined (USEFAKEPIPE)
			ret = exec_process(trp);
#else
			ret = exec_process(trp, pipein);
#endif
		else {
			if (checkshellbuiltin(trp) < 0);
			else if (errno < 0);
			else if (errp -> type & MD_HEREDOC)
				doperror(NULL, NULL);
			else {
				char *tmp;

				if (!(errp -> filename)) tmp = strdup2("-");
				else tmp = evalvararg(errp -> filename,
					0, 1, '\0', 1, 0);
				if (tmp && !*tmp && *(errp -> filename)) {
					free(tmp);
					tmp = strdup2(errp -> filename);
				}
				if (tmp) {
					if (errno) doperror(NULL, tmp);
					else execerror(tmp, ER_RESTRICTED, 0);
					free(tmp);
				}
			}
			ret = RET_FAIL;
		}
		closeredirect((trp -> comm) -> redp);
		ret_status = (ret >= 0) ? ret : RET_FAIL;
	}
	trp = tmptr;

	if (returnlevel || ret < 0);
	else if (pipein >= 0L && (fd = reopenpipe(fd, RET_SUCCESS)) < 0) {
		pipein = -1L;
		ret = -1;
	}
	else {
#if	!MSDOS
		if (!pid) {
			searchheredoc(trp, 1);
			prepareexit(1);
			Xexit(RET_SUCCESS);
		}
#endif	/* !MSDOS */

		if (interrupted) {
			breaklevel = loopdepth;
			ret = RET_INTR;
		}
		else if (!(trp -> next));
		else if (errorexit && !cond && isopfg(trp) && ret_status)
			breaklevel = loopdepth;
		else if (isopand(trp) && ret_status) {
			if (errorexit && !cond) {
				breaklevel = loopdepth;
				ret = RET_SUCCESS;
			}
		}
		else if (isopor(trp) && !ret_status);
		else ret = exec_stree(trp -> next, cond);
	}
	if (pipein >= 0L) closepipe(fd);
	exectrapcomm();
	return(ret);
}

static syntaxtree *NEAR execline(command, stree, trp, noexit)
char *command;
syntaxtree *stree, *trp;
int noexit;
{
	int ret;

	if (command == (char *)-1) {
		if (!(trp = analyzeeof(trp))) {
			freestree(stree);
			return(NULL);
		}
	}
	else {
		if (!(trp = analyze(command, trp, 0))) {
			freestree(stree);
			if (errorexit && !noexit) {
				free(stree);
#if	!MSDOS && !defined (NOJOB)
				if (loginshell && interactive_io) killjob();
#endif
				Xexit2(RET_SYNTAXERR);
			}
			return(NULL);
		}
		if (trp -> cont) return(trp);
	}

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

	if (ret < 0 && errno) doperror(NULL, NULL);
	if ((errorexit && ret && !noexit) || terminated) {
		free(stree);
#if	!MSDOS && !defined (NOJOB)
		if (loginshell && interactive_io) killjob();
#endif
		Xexit2(ret);
	}
	return(NULL);
}

static int NEAR exec_line(command)
char *command;
{
	static syntaxtree *stree = NULL;
	static syntaxtree *trp = NULL;
	int ret;

	if (!command) {
		if (stree) {
			freestree(stree);
			free(stree);
			stree = trp = NULL;
		}
		return(0);
	}

	setsignal();
	if (verboseinput && command != (char *)-1) {
		kanjifputs(command, stderr);
		fputc('\n', stderr);
		fflush(stderr);
	}
	if (!stree) trp = stree = newstree(NULL);

#if	!MSDOS && !defined (NOJOB)
	childpgrp = -1L;
#endif
	if ((trp = execline(command, stree, trp, 0))) ret = trp -> cont;
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

#ifndef	FDSH
int dosystem(command)
char *command;
{
	int ret;

	if (!command) return(RET_NULSYSTEM);

	setsignal();
# if	!MSDOS && !defined (NOJOB)
	childpgrp = -1L;
# endif
	ret = _dosystem(command);
	resetsignal(0);

	return(ret);
}
#endif	/* !FDSH */

static FILE *NEAR _dopopen(command)
char *command;
{
	syntaxtree *trp;
	p_id_t pipein;
	int fd;

	trp = (command) ? analyzeline(command) : NULL;

	nottyout++;
#if	MSDOS || defined (USEFAKEPIPE)
	if ((fd = openpipe(&pipein, STDIN_FILENO, 1)) < 0);
#else
	if ((fd = openpipe(&pipein, STDIN_FILENO, 1, 1, mypid)) < 0);
#endif
	else if (pipein) nownstree(trp);
	else {
		if (!trp) ret_status = RET_SYNTAXERR;
		else if (!(trp -> comm)) ret_status = RET_SUCCESS;
		else if (notexec) {
#ifdef	SHOWSTREE
			show_stree(trp, 0);
#endif
			pipein = -1L;
		}
#ifndef	_NOUSEHASH
		else if (hashahead && check_stree(trp) < 0) pipein = -1L;
#endif
		else if (exec_stree(trp, 0) < 0) pipein = -1L;

		searchheredoc(trp, 1);
		if (pipein >= 0L) fd = reopenpipe(fd, ret_status);
		else {
			closepipe(fd);
			fd = -1;
		}
	}

	if (trp) {
		freestree(trp);
		free(trp);
	}
	errorexit = tmperrorexit;
	nottyout--;
	return((fd >= 0) ? fdopenpipe(fd) : NULL);
}

#ifndef	FDSH
FILE *dopopen(command)
char *command;
{
	FILE *fp;

	setsignal();
# if	!MSDOS && !defined (NOJOB)
	childpgrp = -1L;
# endif
	fp = _dopopen(command);
	resetsignal(0);
	return(fp);
}

int dopclose(fp)
FILE *fp;
{
	return(closepipe(fileno(fp)));
}
#endif	/* !FDSH */

static int NEAR sourcefile(fd, fname, verbose)
int fd;
char *fname;
int verbose;
{
	syntaxtree *stree, *trp;
	char *buf;
	int n, ret, dupinteractive;

	dupinteractive = interactive;
	interactive = 0;
	trp = stree = newstree(NULL);
	ret = 0;
	n = errno = 0;
	while ((buf = readline(fd))) {
		if (!trp) trp = stree;
		trp = execline(buf, stree, trp, 1);
		n++;
		if (ret_status == RET_NOTICE || syntaxerrno || execerrno) {
			ret++;
			if (verbose) {
				fputs(fname, stderr);
				fputc(':', stderr);
				fputlong(n, stderr);
				fputs(": ", stderr);
				fputs(buf, stderr);
				fputc('\n', stderr);
				fflush(stderr);
			}
			ret_status = RET_FAIL;
		}
		free(buf);
		if (syntaxerrno) break;
#ifndef	BASHSTYLE
		if (execerrno) break;
#endif
	}
	safeclose(fd);

	if (!ret && errno) {
		doperror(NULL, fname);
		ret++;
		ret_status = RET_FAIL;
	}
	execline((char *)-1, stree, trp, 1);
	if (syntaxerrno) ret_status = RET_SYNTAXERR;
	free(stree);
	interactive = dupinteractive;
	return(ret);
}

int execruncom(fname, verbose)
char *fname;
int verbose;
{
	char **dupargvar;
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
		dupargvar = argvar;
		argvar = (char **)malloc2(2 * sizeof(char *));
		argvar[0] = strdup2(fname);
		argvar[1] = NULL;
		ret = sourcefile(fd, fname, verbose);
		freevar(argvar);
		argvar = dupargvar;
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
	int i, n, len;

	shellname = argv[0];
	if ((name = strrdelim(shellname, 1))) name++;
	else name = shellname;
	if (*name == '-') {
		loginshell = 1;
		name++;
	}

#ifdef	FD
	opentty();
	ttyio = newdup(ttyio);
	inittty(0);
	getterment();
#else
	if ((ttyio = open(TTYNAME, O_RDWR, 0600)) < 0
	&& (ttyio = newdup(Xdup(STDERR_FILENO))) < 0) {
		doperror(NULL, NULL);
		return(-1);
	}
	if (!(ttyout = fdopen(ttyio, "w+"))) ttyout = stderr;
#endif

	definput = STDIN_FILENO;
	interactive =
		(isatty(STDIN_FILENO) && isatty(STDERR_FILENO)) ? 1 : 0;
	orgpid = mypid = getpid();
#if	!MSDOS && !defined (NOJOB)
	ttypgrp = mypid;
	if ((orgpgrp = getpgroup()) < 0L) {
		doperror(NULL, NULL);
		return(-1);
	}
#endif

	if ((n = getoption(argc, argv, envp)) < 0) return(-1);
#if	!defined (MINIMUMSHELL) && defined (BASHSTYLE)
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

	if (definput == STDIN_FILENO && isatty(STDIN_FILENO)) definput = ttyio;

	getvarfunc = getshellvar;
	putvarfunc = putshellvar;
	getretvalfunc = getretval;
	getpidfunc = getorgpid;
	getlastpidfunc = getlastpid;
	getflagfunc = getflagstr;
	checkundeffunc = checkundefvar;
	exitfunc = safeexit;
	backquotefunc = evalbackquote;
#ifndef	MINIMUMSHELL
	posixsubstfunc = evalposixsubst;
#endif
	shellvar = duplvar(envp, 0);
	exportvar = duplvar(envp, 0);
	exportsize = 0L;
	for (i = 0; exportvar[i]; i++)
		exportsize += (long)strlen(exportvar[i]) + 1;
	exportlist = (char **)malloc2((i + 1) * sizeof(char *));
	for (i = 0; exportvar[i]; i++) {
		len = ((cp = strchr(exportvar[i], '=')))
			? cp - exportvar[i] : strlen(exportvar[i]);
		exportlist[i] = strdupcpy(exportvar[i], len);
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

	if (!(cp = getconstvar("PATH"))) setenv2("PATH", DEFPATH);
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
#ifdef	FAKEUNINIT
	home = NULL;	/* fake for -Wuninitialized */
#endif
	if (loginshell && !(home = getconstvar("HOME"))) {
#if	!MSDOS
		if ((pwd = getpwuid(getuid())) && pwd -> pw_dir)
			home = pwd -> pw_dir;
		else
#endif
		home = _SS_;
		setenv2("HOME", home);
	}
#if	!MSDOS && !defined (MINIMUMSHELL)
# ifdef	BASHSTYLE
	setenv2("MAILCHECK", "60");
# else
	setenv2("MAILCHECK", "600");
# endif
	if ((cp = getconstvar("MAILPATH"))) replacemailpath(cp, 1);
	else if ((cp = getconstvar("MAIL"))) replacemailpath(cp, 0);
#endif
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
	for (i = 0; signallist[i].sig >= 0; i++)
		trapmode[signallist[i].sig] = signallist[i].flags;
#if	!MSDOS && !defined (NOJOB)
	if (!interactive) {
		jobok = 0;
		orgpgrp = -1L;
		oldttypgrp = -1L;
	}
	else {
		if (jobok < 0) jobok = 1;
		gettcpgrp(ttyio, oldttypgrp);
		if (!orgpgrp && (setpgroup(0, orgpgrp = mypid) < 0L
		|| settcpgrp(ttyio, orgpgrp) < 0)) {
			doperror(NULL, NULL);
			return(-1);
		}
		gettcpgrp(ttyio, ttypgrp);
# ifdef	SIGTTIN
		while (ttypgrp >= 0L && ttypgrp != orgpgrp) {
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
			doperror(NULL, NULL);
			return(-1);
		}
# endif
		if ((orgpgrp != mypid && setpgroup(0, orgpgrp = mypid) < 0)
		|| (mypid != ttypgrp && gettermio(orgpgrp) < 0)) {
			doperror(NULL, NULL);
			return(-1);
		}
		closeonexec(ttyio);
	}
#endif	/* !MSDOS && !NOJOB */

	if (n > 2) {
		setsignal();
		if (verboseinput) {
			kanjifputs(argv[2], stderr);
#ifdef	BASHSTYLE
	/* bash displays a newline with single string command, in -v mode */
			fputc('\n', stderr);
#endif
			fflush(stderr);
		}
		shellmode = 1;
		n = _dosystem(argv[2]);
		resetsignal(0);
		Xexit2(n);
	}

	if (loginshell && chdir3(home) < 0) {
		fputs("No directory\n", stderr);
		fflush(stderr);
		Xexit2(RET_FAIL);
	}
	if (!(cp = getconstvar("SHELL"))) cp = name;
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
	int cont;

	shellmode = 1;
	setsignal();
	cont = 0;
	if (pseudoexit) exit_status = -1;
	for (;;) {
		trapok = 1;
		if (!interactive) buf = readline(definput);
		else {
#if	!MSDOS
			checkmail(0);
#endif
			ps = (cont) ? getconstvar("PS2") : getconstvar("PS1");
#ifdef	FD
			ttyiomode(1);
			buf = inputshellstr(ps, -1, NULL);
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
				doperror(NULL, NULL);
				exec_line(NULL);
				break;
			}
#ifndef	MINIMUMSHELL
			if (interactive && ignoreeof) {
				if (cont) {
					exec_line(NULL);
					syntaxerrno = ER_UNEXPEOF;
					syntaxerror("");
				}
				else {
					fputs("Use \"", stderr);
					fputs((loginshell) ? "logout" : "exit",
						stderr);
					fputs("\" to leave to the shell.\n",
						stderr);
					fflush(stderr);
				}
				cont = 0;
				continue;
			}
#endif	/* !MINIMUMSHELL */
			if (!cont) break;
			exec_line((char *)-1);
			cont = 0;
			ERRBREAK;
		}
		cont = exec_line(buf);
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
#ifdef	FD
# if	MSDOS
	inittty(1);
# endif
	getwsize(0, 0);
# ifndef	_NOCUSTOMIZE
	saveorigenviron();
# endif
#endif
	ret_status = RET_SUCCESS;
	if (loginshell) {
		execruncom(DEFRUNCOM, 0);
		execruncom(RUNCOMFILE, 0);
	}
#ifdef	FD
	else {
		execruncom(DEFRUNCOM, 1);
		execruncom(FD_RUNCOMFILE, 1);
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
#endif	/* !FD || (FD >= 2 && !_NOORIGSHELL) */
