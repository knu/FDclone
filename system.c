/*
 *	system.c
 *
 *	command line analysis
 */

#ifdef	FD
#include "fd.h"
#include "term.h"
#include "types.h"
#include "kconv.h"
#else
#define	K_EXTERN
#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#endif

#include "dirent.h"
#include "sysemu.h"

#if	!defined (FD) && !defined (MINIMUMSHELL)
#include "time.h"
#endif
#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#if	defined (DEP_ORIGSHELL) && !defined (MINIMUMSHELL)
#include "posixsh.h"
#endif
#ifdef	DEP_PTY
#include "termemu.h"
#endif
#ifdef	DEP_SOCKET
#include "socket.h"
#include "url.h"
#endif
#ifdef	DEP_URLPATH
#include "urldisk.h"
#endif

#if	MSDOS
#include <process.h>
# ifdef	USERESOURCEH
# include <sys/time.h>
# include <sys/resource.h>
# endif
#else	/* !MSDOS */
# ifdef	USERESOURCEH
# include <sys/resource.h>
# endif
# ifdef	USEULIMITH
# include <ulimit.h>
# endif
#endif	/* !MSDOS */

#ifndef	FD
typedef struct _lockbuf_t {
	int fd;
} lockbuf_t;

# ifdef	PATHNOCASE
# define	TMPPREFIX	"TM"
# else
# define	TMPPREFIX	"tm"
# endif
#endif	/* !FD */

#ifdef	GETPGRPVOID
#define	getpgroup		getpgrp
#else
#define	getpgroup()		getpgrp(0)
#endif
#ifdef	USESETPGID
#define	setpgroup		setpgid
#else
#define	setpgroup		setpgrp
#endif

#if	MSDOS
#define	havetty()		(1)
#else
# ifdef	NOJOB
# define	havetty()	(mypid == shellpid)
# else
# define	havetty()	(ttypgrp >= (p_id_t)0 && mypid == ttypgrp)
# endif
#endif

#define	getwaitsig(x)		((WIFSTOPPED(x)) ? WSTOPSIG(x) : \
				((WIFSIGNALED(x)) ? (128 + WTERMSIG(x)) : -1))

#ifndef	RLIM_INFINITY
#define	RLIM_INFINITY		0x7fffffff
#endif
#ifndef	RLIMIT_FSIZE
#define	RLIMIT_FSIZE		255
#endif
#ifndef	UL_GETFSIZE
#define	UL_GETFSIZE		1
#endif
#ifndef	UL_SETFSIZE
#define	UL_SETFSIZE		2
#endif

#if	!defined (RLIMIT_NOFILE) && defined (RLIMIT_OFILE)
#define	RLIMIT_NOFILE		RLIMIT_OFILE
#endif
#if	!defined (RLIMIT_VMEM) && defined (RLIMIT_AS)
#define	RLIMIT_VMEM		RLIMIT_AS
#endif

#if	defined (USESYSCONF) && defined (_SC_CLK_TCK)
#define	CLKPERSEC		sysconf(_SC_CLK_TCK)
#else
# ifdef	CLK_TCK
# define	CLKPERSEC	CLK_TCK
# else
#  ifdef	HZ
#  define	CLKPERSEC	HZ
#  else
#  define	CLKPERSEC	60
#  endif
# endif
#endif

#if	MSDOS
#define	LSH_DEFRC		"\\etc\\profile"
#define	LSH_RCFILE		"~\\profile.rc"
#define	SH_RCFILE		"~\\fdsh.rc"
#else	/* !MSDOS */
#define	LSH_DEFRC		"/etc/profile"
#define	LSH_RCFILE		"~/.profile"
#define	SH_RCFILE		"~/.fdshrc"
#endif	/* !MSDOS */

#define	PS1STR			"$ "
#define	PS1ROOT			"# "
#define	PS2STR			"> "
#define	PS4STR			"+ "
#define	UNLIMITED		"unlimited"
#define	MAXTMPNAMLEN		8
#define	getconstvar(s)		(getshellvar(s, strsize(s)))
#define	constequal(s, c, l)	((l) == strsize(c) && !strnenvcmp(s, c, l))

#ifdef	WITHSOCKREDIR
#define	TYPE_CONNECT		3
#define	TYPE_ACCEPT		4
#define	TYPE_BIND		5
#endif

#ifdef	PSIGNALSTYLE
#define	MESHUP			"Hangup"
#define	MESINT			"Interrupt"
#define	MESQUIT			"Quit"
#define	MESILL			"Illegal instruction"
#define	MESTRAP			"Trace/BPT trap"
#define	MESIOT			"IOT trap"
#define	MESABRT			"Aborted"
#define	MESEMT			"EMT trap"
#define	MESFPE			"Floating point exception"
#define	MESKILL			"Killed"
#define	MESBUS			"Bus error"
#define	MESSEGV			"Segmentation fault"
#define	MESSYS			"Bad system call"
#define	MESPIPE			"Broken pipe"
#define	MESALRM			"Alarm clock"
#define	MESTERM			"Terminated"
#define	MESSTKFLT		"Stack fault"
#define	MESURG			"Urgent I/O condition"
#define	MESSTOP			"Stopped (signal)"
#define	MESTSTP			"Stopped"
#define	MESCONT			"Continued"
#define	MESCHLD			"Child exited"
#define	MESTTIN			"Stopped (tty input)"
#define	MESTTOU			"Stopped (tty output)"
#define	MESIO			"I/O possible"
#define	MESPOLL			"Profiling alarm clock"
#define	MESXCPU			"Cputime limit exceeded"
#define	MESXFSZ			"Filesize limit exceeded"
#define	MESVTALRM		"Virtual timer expired"
#define	MESPROF			"Profiling timer expired"
#define	MESWINCH		"Window changed"
#define	MESLOST			"Resource lost"
#define	MESINFO			"Information request"
#define	MESPWR			"Power failure"
#define	MESUSR1			"User defined signal 1"
#define	MESUSR2			"User defined signal 2"
#else	/* !PSIGNALSTYLE */
#define	MESHUP			"Hangup"
#define	MESINT			""
#define	MESQUIT			"Quit"
#define	MESILL			"Illegal instruction"
#define	MESTRAP			"Trace/BPT trap"
#define	MESIOT			"IOT trap"
#define	MESABRT			"Aborted"
#define	MESEMT			"EMT trap"
#define	MESFPE			"Floating exception"
#define	MESKILL			"Killed"
#define	MESBUS			"Bus error"
#define	MESSEGV			"Memory fault"
#define	MESSYS			"Bad system call"
#define	MESPIPE			""
#define	MESALRM			"Alarm call"
#define	MESTERM			"Terminated"
#define	MESSTKFLT		"Stack fault"
#define	MESURG			"Urgent condition"
#define	MESSTOP			"Stopped"
#define	MESTSTP			"Stopped from terminal"
#define	MESCONT			"Continued"
#define	MESCHLD			"Child terminated"
#define	MESTTIN			"Stopped on terminal input"
#define	MESTTOU			"Stopped on terminal output"
#define	MESIO			"Asynchronous I/O"
#define	MESPOLL			"Profiling alarm clock"
#define	MESXCPU			"Exceeded cpu time limit"
#define	MESXFSZ			"Exceeded file size limit"
#define	MESVTALRM		"Virtual time alarm"
#define	MESPROF			"Profiling time alarm"
#define	MESWINCH		"Window changed"
#define	MESLOST			"Resource lost"
#define	MESINFO			"Information request"
#define	MESPWR			"Power failure"
#define	MESUSR1			"User defined signal 1"
#define	MESUSR2			"User defined signal 2"
#endif	/* !PSIGNALSTYLE */

#ifdef	DEBUG
#define	PSIGNALSTYLE
#define	SHOWSTREE
#endif

#ifdef	DEP_ORIGSHELL

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

#ifdef	FD
extern VOID calcwin __P_((VOID_A));
extern VOID main_fd __P_((char *CONST *, int));
extern VOID checkscreen __P_((int, int));
# ifdef	SIGWINCH
extern VOID pollscreen __P_((int));
# endif
extern int sigvecset __P_((int));
# ifndef	_NOCUSTOMIZE
extern VOID saveorigenviron __P_((VOID_A));
# endif
extern VOID initfd __P_((char *CONST *));
extern VOID prepareexitfd __P_((int));
extern int savestdio __P_((int));
extern int checkbuiltin __P_((CONST char *));
extern int checkinternal __P_((CONST char *));
extern int execbuiltin __P_((int, int, char *CONST []));
extern int execinternal __P_((int, int, char *CONST []));
# ifndef	_NOCOMPLETE
extern int completebuiltin __P_((CONST char *, int, int, char ***));
extern int completeinternal __P_((CONST char *, int, int, char ***));
# endif
extern VOID initenv __P_((VOID_A));
extern VOID evalenv __P_((CONST char *, int));
extern int underhome __P_((char *));
extern int replaceargs __P_((int *, char ***, char *CONST *, int));
extern int replacearg __P_((char **));
extern VOID demacroarg __P_((char **));
extern char *inputshellstr __P_((CONST char *, int, CONST char *));
extern int evalprompt __P_((char **, CONST char *));
#endif	/* !FD */

#ifdef	DOSCOMMAND
extern int doscomdir __P_((int, char *CONST []));
extern int doscommkdir __P_((int, char *CONST []));
extern int doscomrmdir __P_((int, char *CONST []));
extern int doscomerase __P_((int, char *CONST []));
extern int doscomrename __P_((int, char *CONST []));
extern int doscomcopy __P_((int, char *CONST []));
extern int doscomcls __P_((int, char *CONST []));
extern int doscomtype __P_((int, char *CONST []));
#endif	/* DOSCOMMAND */

#ifdef	DEBUG
extern VOID mtrace __P_ ((VOID_A));
extern VOID muntrace __P_ ((VOID_A));
extern char *_mtrace_file;
#endif

#if	MSDOS
extern int setcurdrv __P_((int, int));
#endif

#ifdef	FD
# ifndef	_NOEDITMODE
extern char *editmode;
# endif
extern int internal_status;
extern char fullpath[];
extern char *origpath;
extern int inruncom;
extern int fd_restricted;
extern int fdmode;
extern int physical_path;
extern char *deftmpdir;
#else	/* !FD */
# ifdef	__TURBOC__
extern u_int _stklen = 0x8000;
# endif
# if	MSDOS
# define	deftmpdir	_SS_
# else
# define	deftmpdir	"/tmp"
# endif
int ttyio = -1;
XFILE *ttyout = NULL;
#endif	/* !FD */

#ifndef	MINIMUMSHELL
# ifndef	NOJOB
extern jobtable *joblist;
extern int maxjobs;
# endif
# if	!MSDOS
extern int mailcheck;
# endif
# ifndef	NOALIAS
extern shaliastable *shellalias;
# endif
# ifndef	NOPOSIXUTIL
extern int posixoptind;
# endif
#endif	/* !MINIMUMSHELL */

#ifdef	FD
extern lockbuf_t *lockopen __P_((CONST char *, int, int));
extern VOID lockclose __P_((lockbuf_t *));
extern int mktmpfile __P_((char *));
extern int rmtmpfile __P_((CONST char *));
extern int chdir2 __P_((CONST char *));
extern int chdir3 __P_((CONST char *, int));
extern int setenv2 __P_((CONST char *, CONST char *, int));
extern char *getenv2 __P_((CONST char *));
# ifdef	USESIGACTION
extern sigcst_t signal2 __P_((int, sigcst_t));
# else
# define	signal2		signal
# endif
# if	MSDOS && !defined (BSPATHDELIM)
extern char *adjustpname __P_((char *));
# endif
#else	/* !FD */
static lockbuf_t *NEAR lockopen __P_((CONST char *, int, int));
static VOID NEAR lockclose __P_((lockbuf_t *));
static int NEAR genrand __P_((int));
static char *NEAR genrandname __P_((char *, int));
static int NEAR mktmpfile __P_((char *));
static int NEAR rmtmpfile __P_((CONST char *));
# define	chdir2		Xchdir
int chdir3 __P_((CONST char *, int));
int setenv2 __P_((CONST char *, CONST char *, int));
# ifdef	USESIGACTION
static sigcst_t NEAR signal2 __P_((int, sigcst_t));
# else
# define	signal2		signal
# endif
# if	MSDOS && !defined (BSPATHDELIM)
static char *NEAR adjustpname __P_((char *));
# endif
#endif	/* !FD */

#ifndef	NOJOB
static VOID NEAR notifyjob __P_((VOID_A));
#endif
static VOID NEAR setsignal __P_((VOID_A));
static VOID NEAR resetsignal __P_((int));
static VOID NEAR exectrapcomm __P_((VOID_A));
static VOID NEAR killmyself __P_((int));
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
#ifdef	FD
static VOID NEAR argfputs __P_((CONST char *, XFILE *));
#else
#define	argfputs		Xfputs
#endif
#ifdef	DEP_FILECONV
static int NEAR Kopen __P_((CONST char *, int, int));
#else
#define	Kopen			Xopen
#endif
static VOID NEAR syntaxerror __P_((CONST char *));
#if	!MSDOS && defined (DEP_PTY) && defined (CYGWIN)
static VOID NEAR addmychild __P_((p_id_t));
#endif
#if	!MSDOS
static VOID NEAR setstopsig __P_((int));
static p_id_t NEAR makechild __P_((int, p_id_t, int));
#endif	/* !MSDOS */
static VOID NEAR safermtmpfile __P_((CONST char *));
static int NEAR getoption __P_((int, char *CONST *, int));
static ALLOC_T NEAR c_allocsize __P_((int));
static int NEAR readchar __P_((int));
static char *NEAR readline __P_((int, int));
static char *NEAR readfile __P_((int, ALLOC_T *));
static heredoc_t *NEAR newheredoc __P_((char *, char *, int, int));
static redirectlist *NEAR newrlist __P_((int, char *, int, redirectlist *));
static command_t *NEAR newcomm __P_((VOID_A));
static syntaxtree *NEAR eldeststree __P_((syntaxtree *));
static syntaxtree *NEAR childstree __P_((syntaxtree *, int));
static syntaxtree *NEAR skipfuncbody __P_((syntaxtree *));
static syntaxtree *NEAR insertstree __P_((syntaxtree *, syntaxtree *, int));
static syntaxtree *NEAR linkstree __P_((syntaxtree *, int));
static VOID NEAR nownstree __P_((syntaxtree *));
static int NEAR evalfiledesc __P_((CONST char *));
static int NEAR redmode __P_((int));
static int NEAR cancelredirect __P_((redirectlist *));
static VOID NEAR closeredirect __P_((redirectlist *));
static heredoc_t *NEAR searchheredoc __P_((syntaxtree *, int));
static int NEAR saveheredoc __P_((CONST char *, syntaxtree *));
static int NEAR openheredoc __P_((heredoc_t *, int));
#ifdef	DEP_DOSDRIVE
static int NEAR fdcopy __P_((int, int));
static int NEAR openpseudofd __P_((redirectlist *));
static int NEAR closepseudofd __P_((redirectlist *));
#endif
#ifdef	WITHSOCKREDIR
static int NEAR parsesocket __P_((CONST char *, char **, int *));
#endif
static redirectlist *NEAR doredirect __P_((redirectlist *));
static heredoc_t *NEAR heredoc __P_((char *, int));
static int NEAR redirect __P_((syntaxtree *, int, char *, int));
#if	!MSDOS && defined (MINIMUMSHELL)
static VOID NEAR checkmail __P_((int));
#endif
static int NEAR searchvar __P_((char *CONST *, CONST char *, int, int));
static char *NEAR searchvar2 __P_((int, CONST char **,
		CONST char *, int));
static char **NEAR expandvar __P_((char **, CONST char *, int));
static char *NEAR getvar __P_((char *CONST *, CONST char *, int));
static char **NEAR putvar __P_((char **, char *, int));
static int NEAR checkprimal __P_((CONST char *, int));
static int NEAR checkrestrict __P_((CONST char *, int));
static int NEAR checkronly __P_((CONST char *, int));
static int NEAR _putshellvar __P_((char *, int));
#ifndef	MINIMUMSHELL
static VOID NEAR setshlineno __P_((long));
#endif
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
static int checkundefvar __P_((CONST char *, CONST char *, int));
static VOID safeexit __P_((VOID_A));
static int NEAR getparenttype __P_((syntaxtree *));
static int NEAR parsestatement __P_((syntaxtree **, int, int, int));
static syntaxtree *NEAR _addarg __P_((syntaxtree *, CONST char *));
static int NEAR addarg __P_((syntaxtree **, redirectlist *,
		char *, int *, int));
static int NEAR evalredprefix __P_((syntaxtree **, redirectlist *, int *));
static syntaxtree *NEAR rparen __P_((syntaxtree *));
static syntaxtree *NEAR semicolon __P_((syntaxtree *, redirectlist *,
		CONST char *, int *));
static syntaxtree *NEAR ampersand __P_((syntaxtree *, redirectlist *,
		CONST char *, int *));
static syntaxtree *NEAR vertline __P_((syntaxtree *, CONST char *, int *));
static syntaxtree *NEAR lessthan __P_((syntaxtree *, redirectlist *,
		CONST char *, int *));
static syntaxtree *NEAR morethan __P_((syntaxtree *, redirectlist *,
		CONST char *, int *));
#if	defined (BASHSTYLE) || !defined (MINIMUMSHELL)
static syntaxtree *NEAR endvar __P_((syntaxtree *, redirectlist *,
		CONST char *, int *, int *, ALLOC_T *, int));
static syntaxtree *NEAR addvar __P_((syntaxtree *,
		CONST char *, int *, CONST char *, int *, int));
#endif
static syntaxtree *NEAR normaltoken __P_((syntaxtree *, redirectlist *,
		CONST char *, int *, int *, ALLOC_T *));
static syntaxtree *NEAR casetoken __P_((syntaxtree *, redirectlist *,
		CONST char *, int *, int *));
#if	!defined (BASHBUG) && !defined (MINIMUMSHELL)
static int NEAR cmpstatement __P_((CONST char *, int));
static syntaxtree *NEAR comsubtoken __P_((syntaxtree *, redirectlist *,
		CONST char *, int *, int *, ALLOC_T *));
#endif
static syntaxtree *NEAR analyzeloop __P_((syntaxtree *, redirectlist *,
		CONST char *, int));
static syntaxtree *NEAR analyzeeof __P_((syntaxtree *));
static syntaxtree *NEAR statementcheck __P_((syntaxtree *, int));
static int NEAR check_statement __P_((syntaxtree *));
static int NEAR check_command __P_((syntaxtree *));
static int NEAR check_stree __P_((syntaxtree *));
static syntaxtree *NEAR analyzeline __P_((CONST char *));
#ifdef	DEBUG
static VOID NEAR Xexecve __P_((CONST char *, char *[], char *[], int));
#else
static VOID NEAR Xexecve __P_((CONST char *, char *[], char *[]));
#endif
#if	MSDOS
static char *NEAR addext __P_((char *, int));
static char **NEAR replacebat __P_((char **, char **));
#endif
#ifdef	USEFAKEPIPE
static int NEAR openpipe __P_((p_id_t *, int, int));
#else
static int NEAR openpipe __P_((p_id_t *, int, int, int, p_id_t));
#endif
static pipelist **NEAR searchpipe __P_((int));
static int NEAR reopenpipe __P_((int, int));
static XFILE *NEAR fdopenpipe __P_((int));
#ifdef	DJGPP
static int NEAR closepipe __P_((int, int));
#define	closepipe2(f, d)	closepipe(f, d)
#else
static int NEAR closepipe __P_((int));
#define	closepipe2(f, d)	closepipe(f)
#endif
#ifndef	_NOUSEHASH
static VOID NEAR disphash __P_((VOID_A));
#endif
#ifdef	BASHSTYLE
static char *NEAR quotemeta __P_((char *));
#endif
static int NEAR substvar __P_((char **, int));
static int NEAR evalargv __P_((command_t *, int *, int *));
static char *NEAR evalexternal __P_((command_t *));
static VOID NEAR printindent __P_((int, XFILE *));
static VOID NEAR printnewline __P_((int, XFILE *));
static int NEAR printredirect __P_((redirectlist *, XFILE *));
#ifndef	MINIMUMSHELL
static VOID NEAR printheredoc __P_((redirectlist *, XFILE *));
static redirectlist **NEAR _printstree __P_((syntaxtree *,
		redirectlist **, int, XFILE *));
#endif
static VOID NEAR printshfunc __P_((shfunctable *, XFILE *));
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
static int NEAR expandlist __P_((char ***, CONST char *));
static int NEAR doexport __P_((syntaxtree *));
static int NEAR doreadonly __P_((syntaxtree *));
static int NEAR dotimes __P_((syntaxtree *));
static int NEAR dowait __P_((syntaxtree *));
static int NEAR doumask __P_((syntaxtree *));
static int NEAR doulimit __P_((syntaxtree *));
static int NEAR dotrap __P_((syntaxtree *));
#ifndef	NOJOB
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
static int NEAR dotest __P_((syntaxtree *));
#endif
#ifndef	NOPOSIXUTIL
static int NEAR dofalse __P_((syntaxtree *));
static int NEAR docommand __P_((syntaxtree *));
static int NEAR dogetopts __P_((syntaxtree *));
static int NEAR donewgrp __P_((syntaxtree *));
# if	0				/* exists in FD original builtin */
static int NEAR dofc __P_((syntaxtree *));
# endif
#endif	/* !NOPOSIXUTIL */
#ifndef	MINIMUMSHELL
static int NEAR getworkdir __P_((char *));
static int NEAR dopushd __P_((syntaxtree *));
static int NEAR dopopd __P_((syntaxtree *));
static int NEAR dodirs __P_((syntaxtree *));
static int NEAR doenable __P_((syntaxtree *));
static int NEAR dobuiltin __P_((syntaxtree *));
static int NEAR doaddcr __P_((syntaxtree *));
#endif
#ifdef	WITHSOCKET
static int NEAR doaccept __P_((syntaxtree *));
static int NEAR dosocketinfo __P_((syntaxtree *));
#endif
#ifdef	FD
static int NEAR dofd __P_((syntaxtree *));
#endif
static int NEAR doshfunc __P_((syntaxtree *, int));
#ifdef	SHOWSTREE
static VOID NEAR show_stree __P_((syntaxtree *, int));
#endif
static int NEAR dosetshfunc __P_((CONST char *, syntaxtree *));
static int NEAR exec_statement __P_((syntaxtree *));
static char **NEAR checkshellbuiltinargv __P_((int, char **));
static int NEAR checkshellbuiltin __P_((syntaxtree *));
#if	MSDOS
static int NEAR exec_command __P_((syntaxtree *, int *));
#else
static int NEAR exec_command __P_((syntaxtree *, int *, int));
#endif
#ifdef	USEFAKEPIPE
static int NEAR exec_process __P_((syntaxtree *));
#else
static int NEAR exec_process __P_((syntaxtree *, p_id_t));
#endif
static int NEAR exec_stree __P_((syntaxtree *, int));
static syntaxtree *NEAR execline __P_((CONST char *,
		syntaxtree *, syntaxtree *, int));
static int NEAR exec_line __P_((CONST char *));
static int NEAR _dosystem __P_((CONST char *));
static XFILE *NEAR _dopopen __P_((CONST char *));
#ifndef	FDSH
int dosystem __P_((CONST char *));
XFILE *dopopen __P_((CONST char *));
int dopclose __P_((XFILE *));
#endif
static int NEAR sourcefile __P_((int, CONST char *, int));
#if	MSDOS && !defined (BSPATHDELIM)
static VOID NEAR adjustdelim __P_((char *CONST *));
#endif
static VOID NEAR initrc __P_((int));
#ifdef	FDSH
int main __P_((int, char *CONST [], char *CONST []));
#endif

int shellmode = 0;
p_id_t mypid = (p_id_t)-1;
p_id_t orgpid = (p_id_t)-1;
p_id_t shellpid = (p_id_t)-1;
int ret_status = RET_SUCCESS;
int interactive = 0;
int errorexit = 0;
#ifdef	DEP_PTY
int shptymode = 0;
#define	isshptymode()		(!fdmode && shptymode)
#endif
#ifndef	NOJOB
int lastjob = -1;
int prevjob = -1;
int stopped = 0;
p_id_t orgpgrp = (p_id_t)-1;
p_id_t childpgrp = (p_id_t)-1;
p_id_t ttypgrp = (p_id_t)-1;
#endif
int interrupted = 0;
int nottyout = 0;
int syntaxerrno = 0;
#if	!MSDOS
int sigconted = 0;
#endif
CONST statementtable statementlist[] = {
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
#define	STATEMENTSIZ		arraysize(statementlist)
CONST signaltable signallist[] = {
#ifdef	SIGHUP
	{SIGHUP, trap_hup, "HUP", MESHUP, TR_TERM | TR_BLOCK},
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
# if	MSDOS
	{SIGCONT, trap_cont, "CONT", MESCONT, TR_IGN},
# else
	{SIGCONT, trap_cont, "CONT", MESCONT, TR_IGN | TR_BLOCK},
# endif
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
# ifdef	FD
	{SIGWINCH, trap_winch, "WINCH", MESWINCH, TR_IGN | TR_BLOCK},
# else
	{SIGWINCH, trap_winch, "WINCH", MESWINCH, TR_IGN},
# endif
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
int ignoreeof = 0;
#endif
#ifndef	NOJOB
int bgnotify = 0;
int jobok = -1;
#endif
#ifdef	FD
# ifndef	_NOEDITMODE
int emacsmode = 0;
int vimode = 0;
# endif
# ifdef	DEP_PTY
int tmpshptymode = 0;
# endif
# if	!MSDOS
int autosavetty = 0;
# endif
#endif	/* FD */
int loginshell = 0;
int noruncom = 0;

static char **shellvar = NULL;
static char **exportvar = NULL;
static u_long exportsize = (u_long)0;
static char **exportlist = NULL;
static char **ronlylist = NULL;
static char *shellname = NULL;
static int definput = -1;
static int exit_status = RET_SUCCESS;
#if	!MSDOS
static sigmask_t oldsigmask;
#endif
static p_id_t lastpid = (p_id_t)-1;
#ifndef	NOJOB
static p_id_t oldttypgrp = (p_id_t)-1;
#endif
static int setsigflag = 0;
static int trapok = 0;
static shfunctable *shellfunc = NULL;
static pipelist *pipetop = NULL;
static int childdepth = 0;
static int loopdepth = 0;
static int breaklevel = 0;
static int continuelevel = 0;
static int shfunclevel = 0;
static int returnlevel = 0;
#ifndef	MINIMUMSHELL
static char **dirstack = NULL;
static long shlineno = 0L;
#endif
static int isshellbuiltin = 0;
static int execerrno = 0;
#if	!MSDOS && defined (DEP_PTY) && defined (CYGWIN)
static p_id_t *mychildren = (p_id_t *)NULL;
#endif
static CONST char *CONST syntaxerrstr[] = {
	NULL,
#define	ER_UNEXPTOK		1
	"unexpected token",
#define	ER_UNEXPNL		2
	"unexpected newline or `;'",
#define	ER_UNEXPEOF		3
	"unexpected end of file",
};
#define	SYNTAXERRSIZ		arraysize(syntaxerrstr)
static CONST char *CONST execerrstr[] = {
	NULL,
#define	ER_COMNOFOUND		1
	"command not found",
#define	ER_NOTFOUND		2
	"not found",
#define	ER_CANNOTEXE		3
	"cannot execute",
#define	ER_NOTIDENT		4
	"is not an identifier",
#define	ER_BADSUBST		5
	"bad substitution",
#define	ER_BADNUMBER		6
	"bad number",
#define	ER_BADDIR		7
	"bad directory",
#define	ER_CANNOTRET		8
	"cannot return when not in function",
#define	ER_CANNOTSTAT		9
	"cannot stat .",
#define	ER_CANNOTUNSET		10
	"cannot unset",
#define	ER_ISREADONLY		11
	"is read only",
#define	ER_CANNOTSHIFT		12
	"cannot shift",
#define	ER_BADOPTIONS		13
	"bad option(s)",
#define	ER_PARAMNOTSET		14
	"parameter not set",
#define	ER_RESTRICTED		15
	"restricted",
#define	ER_BADULIMIT		16
	"bad ulimit",
#define	ER_BADTRAP		17
	"bad trap",
#define	ER_NUMOUTRANGE		18
	"number out of range",
#define	ER_NOHOMEDIR		19
	"no home directory",
#ifdef	NOALIAS
	NULL,
#else
#define	ER_NOTALIAS		20
	"is not an alias",
#endif
#ifdef	NOPOSIXUTIL
	NULL,
#else
#define	ER_MISSARG		21
	"missing argument",
#endif
#ifdef	NOJOB
	NULL, NULL,
#else
#define	ER_NOSUCHJOB		22
	"no such job",
#define	ER_TERMINATED		23
	"job has terminated",
#endif
#ifdef	MINIMUMSHELL
	NULL, NULL, NULL,
#else
#define	ER_NOTLOGINSH		24
	"not login shell",
#define	ER_DIREMPTY		25
	"directory stack empty",
#define	ER_UNKNOWNSIG		26
	"unknown signal; kill -l lists signals",
#endif
#if	!MSDOS
	NULL,
#else
#define	ER_INVALDRIVE		27
	"invalid drive specification",
#endif
#ifdef	WITHSOCKET
#define	ER_NOTSOCKET		28
	"not socket",
#define	ER_INVALSOCKET		29
	"invalid socket",
#else
	NULL, NULL,
#endif
#ifdef	FD
#define	ER_RECURSIVEFD		30
	"recursive call for FDclone",
#define	ER_INVALTERMFD		31
	"invalid terminal for FDclone",
#endif
};
#define	EXECERRSIZ		arraysize(execerrstr)
static CONST opetable opelist[] = {
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
#define	OPELISTSIZ		arraysize(opelist)
#if	!defined (BASHBUG) && !defined (MINIMUMSHELL)
static CONST opetable delimlist[] = {
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
#define	DELIMLISTSIZ		arraysize(delimlist)
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
	{dochdir, "cd", BT_RESTRICT | BT_FILENAME},
	{dopwd, "pwd", 0},
	{dosource, ".", BT_POSIXSPECIAL | BT_FILENAME},
#ifndef	MINIMUMSHELL
	{dosource, "source", BT_FILENAME},
#endif
	{doexport, "export", BT_POSIXSPECIAL},
	{doreadonly, "readonly", BT_POSIXSPECIAL},
	{dotimes, "times", BT_POSIXSPECIAL},
	{dowait, "wait", 0},
	{doumask, "umask", 0},
	{doulimit, "ulimit", 0},
	{dotrap, "trap", BT_POSIXSPECIAL},
#ifndef	NOJOB
	{dojobs, "jobs", 0},
	{dofg, "fg", 0},
	{dobg, "bg", 0},
	{dodisown, "disown", 0},
#endif
	{dotype, "type", 0},
#ifdef	DOSCOMMAND
	{donull, "rem", 0},
	{dodir, "dir", BT_NOGLOB | BT_FILENAME},
	{dochdir, "chdir", BT_RESTRICT | BT_FILENAME},
# if	MSDOS
	{domkdir, "mkdir", BT_RESTRICT | BT_FILENAME},
	{dormdir, "rmdir", BT_RESTRICT | BT_FILENAME},
# endif
	{domkdir, "md", BT_RESTRICT | BT_FILENAME},
	{dormdir, "rd", BT_RESTRICT | BT_FILENAME},
	{doerase, "erase", BT_NOGLOB | BT_RESTRICT | BT_FILENAME},
	{doerase, "del", BT_NOGLOB | BT_RESTRICT | BT_FILENAME},
	{dorename, "rename", BT_NOGLOB | BT_RESTRICT | BT_FILENAME},
	{dorename, "ren", BT_NOGLOB | BT_RESTRICT | BT_FILENAME},
	{docopy, "copy", BT_NOGLOB | BT_RESTRICT | BT_FILENAME},
	{docls, "cls", 0},
	{dodtype, "dtype", BT_FILENAME},
#endif	/* DOSCOMMAND */
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
# if	0				/* exists in FD original builtin */
	{dofc, "fc", 0},
# endif
#endif
#ifndef	MINIMUMSHELL
	{dopushd, "pushd", BT_RESTRICT | BT_FILENAME},
	{dopopd, "popd", 0},
	{dodirs, "dirs", 0},
	{doenable, "enable", 0},
	{dobuiltin, "builtin", 0},
	{doaddcr, "addcr", 0},
#endif
#ifdef	WITHSOCKET
	{doaccept, "accept", BT_RESTRICT},
	{dosocketinfo, "socketinfo", BT_RESTRICT},
#endif
#ifdef	FD
	{dofd, "fd", BT_FILENAME},
#endif
};
#define	SHBUILTINSIZ		arraysize(shbuiltinlist)
static CONST char *primalvar[] = {
	ENVPATH, ENVPS1, ENVPS2, ENVIFS,
#if	!MSDOS && !defined (MINIMUMSHELL)
	ENVMAILCHECK, ENVPPID,
#endif
};
#define	PRIMALVARSIZ		arraysize(primalvar)
static CONST char *restrictvar[] = {
	ENVPATH,
#ifdef	FD
	"FD_SHELL",
#else
	ENVSHELL,
#endif
#ifndef	MINIMUMSHELL
	ENVENV,
#endif
};
#define	RESTRICTVARSIZ		arraysize(restrictvar)
#if	MSDOS && !defined (BSPATHDELIM)
static CONST char *adjustvar[] = {
	ENVPATH, ENVHOME,
# ifndef	MINIMUMSHELL
	ENVENV,
# endif
# ifdef	FD
	ENVPWD,
	"FD_TMPDIR",
	"FD_PAGER", "FD_EDITOR", "FD_SHELL",
#  ifndef	NOPOSIXUTIL
	"FD_FCEDIT",
#  endif
#  if	MSDOS
	"FD_COMSPEC",
#  endif
#  ifdef	DEP_FILECONV
	"FD_HISTFILE", "FD_RRPATH", "FD_PRECEDEPATH",
	"FD_SJISPATH", "FD_EUCPATH",
	"FD_JISPATH", "FD_JIS8PATH", "FD_JUNETPATH",
	"FD_OJISPATH", "FD_OJIS8PATH", "FD_OJUNETPATH",
	"FD_HEXPATH", "FD_CAPPATH",
	"FD_UTF8PATH", "FD_UTF8MACPATH", "FD_UTF8ICONVPATH",
	"FD_NOCONVPATH",
#  endif
# else	/* !FD */
	ENVSHELL,
#  if	MSDOS
	ENVCOMSPEC,
#  endif
# endif	/* !FD */
};
#define	ADJUSTVARSIZ		arraysize(adjustvar)
#endif	/* MSDOS && !BSPATHDELIM */
static CONST shflagtable shflaglist[] = {
	{"xtrace", &verboseexec, 'x'},
	{"noexec", &notexec, 'n'},
	{"verbose", &verboseinput, 'v'},
	{"onecmd", &terminated, 't'},
	{NULL, &forcedstdin, 's'},
	{NULL, &interactive_io, 'i'},
	{"errexit", &tmperrorexit, 'e'},
	{NULL, &restricted, 'r'},
	{"keyword", &freeenviron, 'k'},
	{"nounset", &undeferror, 'u'},
	{"hashahead", &hashahead, 'h'},
	{"noglob", &noglob, 'f'},
	{"allexport", &autoexport, 'a'},
#ifndef	MINIMUMSHELL
	{"noclobber", &noclobber, 'C'},
	{"ignoreeof", &ignoreeof, '\0'},
#endif
#ifndef	NOJOB
	{"notify", &bgnotify, 'b'},
	{"monitor", &jobok, 'm'},
#endif
#ifdef	FD
	{"physical", &physical_path, 'P'},
# ifndef	_NOEDITMODE
	{"emacs", &emacsmode, '\0'},
	{"vi", &vimode, '\0'},
# endif
# ifdef	DEP_PTY
	{"ptyshell", &tmpshptymode, 'T'},
# endif
# if	!MSDOS
	{"autosavetty", &autosavetty, 'S'},
# endif
#endif	/* FD */
	{NULL, &loginshell, 'l'},
	{NULL, &noruncom, 'N'},
};
#define	FLAGSSIZ		arraysize(shflaglist)
#ifdef	USERESOURCEH
static CONST ulimittable ulimitlist[] = {
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
#define	ULIMITSIZ		arraysize(ulimitlist)
#endif	/* USERESOURCEH */
static int trapmode[NSIG];
static char *trapcomm[NSIG];
static sigarg_t (*oldsigfunc[NSIG])__P_((sigfnc_t));
#ifdef	WITHSOCKREDIR
static scheme_t schemelist[] = {
#define	DEFSCHEME(i, t)		{i, strsize(i), -1, t}
	DEFSCHEME("connect", TYPE_CONNECT),
	DEFSCHEME("accept", TYPE_ACCEPT),
	DEFSCHEME("bind", TYPE_BIND),
	{NULL, 0, 0},
};
#endif


#ifndef	FD
static lockbuf_t *NEAR lockopen(path, flags, mode)
CONST char *path;
int flags, mode;
{
	lockbuf_t *lck;
	int fd;

	if ((fd = newdup(Xopen(path, flags, mode))) >= 0) /*EMPTY*/;
	else if (errno != ENOENT) return(NULL);
	lck = (lockbuf_t *)malloc2(sizeof(lockbuf_t));
	lck -> fd = fd;

	return(lck);
}

static VOID NEAR lockclose(lck)
lockbuf_t *lck;
{
	if (lck) {
		if (lck -> fd >= 0) VOID_C Xclose(lck -> fd);
		free2(lck);
	}
}

static int NEAR genrand(max)
int max;
{
# ifdef	MINIMUMSHELL
	static u_int last = 0;
	static int init = 0;

	if (!init) {
		init++;
		last = getpid();
	}
	last = last * 12345 + 101;

	return(last % max);
# else	/* !MINIMUMSHELL */
	static long last = -1L;
	time_t now;

	if (last < 0L) {
		now = time2();
		last = ((now & 0xff) << 16) + (now & ~0xff) + getpid();
	}

	do {
		last = last * (u_long)1103515245 + 12345;
	} while (last < 0L);

	return((last / 65537L) % max);
# endif	/* !MINIMUMSHELL */
}

static char *NEAR genrandname(buf, len)
char *buf;
int len;
{
	static char seq[] = {
# ifdef	PATHNOCASE
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
		for (i = 0; i < arraysize(seq); i++) {
			j = genrand(arraysize(seq));
			c = seq[i];
			seq[i] = seq[j];
			seq[j] = c;
		}
	}
	else {
		for (i = 0; i < len; i++) {
			j = genrand(arraysize(seq));
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

	strcpy2(path, deftmpdir);
	cp = strcatdelim(path);
	n = strsize(TMPPREFIX);
	strncpy2(cp, TMPPREFIX, n);
	len = strsize(path) - (cp - path);
	if (len > MAXTMPNAMLEN) len = MAXTMPNAMLEN;
	len -= n;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(cp + n, len);
		fd = Xopen(path, O_BINARY | O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd >= 0) {
			strcpy2(file, path);
			return(fd);
		}
		if (errno != EEXIST) break;
	}

	return(-1);
}

static int NEAR rmtmpfile(file)
CONST char *file;
{
	if (Xunlink(file) < 0 && errno != ENOENT) return(-1);

	return(0);
}

/*ARGSUSED*/
int chdir3(dir, raw)
CONST char *dir;
int raw;
{
	if (chdir2(dir) < 0) return(-1);
# ifndef	_NOUSEHASH
	searchhash(NULL, nullstr, nullstr);
# endif

	return(0);
}

int setenv2(name, value, export)
CONST char *name, *value;
int export;
{
	char *cp;
	int len;

	len = strlen(name);
	if (!value) return(unset(name, len));
	else {
		cp = malloc2(len + strlen(value) + 2);
		memcpy(cp, name, len);
		cp[len] = '=';
		strcpy2(&(cp[len + 1]), value);
	}
	if (((export) ? putexportvar(cp, len) : putshellvar(cp, len)) < 0) {
		free2(cp);
		return(-1);
	}

	return(0);
}

# ifdef	USESIGACTION
static sigcst_t NEAR signal2(sig, func)
int sig;
sigcst_t func;
{
	struct sigaction act, oact;

	act.sa_handler = func;
#ifdef	SA_INTERRUPT
	act.sa_flags = SA_INTERRUPT;
#else
	act.sa_flags = 0;
#endif
	sigemptyset(&(act.sa_mask));
	sigemptyset(&(oact.sa_mask));
	if (sigaction(sig, &act, &oact) < 0) return(SIG_ERR);

	return(oact.sa_handler);
}
# endif	/* USESIGACTION */

# if	MSDOS && !defined (BSPATHDELIM)
static char *NEAR adjustpname(path)
char *path;
{
	int i;

	for (i = 0; path[i]; i++) {
		if (path[i] == '\\') path[i] = _SC_;
		else if (iskanji1(path, i)) i++;
	}

	return(path);
}
# endif	/* MSDOS && !BSPATHDELIM */
#endif	/* !FD */

#ifndef	NOJOB
static VOID NEAR notifyjob(VOID_A)
{
	XFILE *fp;
	int fd;

	if ((fd = Xdup(STDERR_FILENO)) < 0) fp = Xstderr;
	else if (!(fp = Xfdopen(fd, "a"))) {
		VOID_C Xclose(fd);
		fp = Xstderr;
	}
	else Xsetlinebuf(fp);
	checkjob(fp);
	safefclose(fp);
}
#endif	/* !NOJOB */

static VOID NEAR setsignal(VOID_A)
{
	int i, sig;

	if (setsigflag++) return;
#if	defined (FD) && defined (SIGALRM)
	noalrm++;
#endif
	for (i = 0; signallist[i].sig >= 0; i++) {
		sig = signallist[i].sig;
		if (signallist[i].flags & TR_BLOCK) /*EMPTY*/;
		else if (signallist[i].flags & TR_READBL) /*EMPTY*/;
		else if ((trapmode[sig] & TR_STAT) != TR_TRAP) continue;
		else if ((signallist[i].flags & TR_STAT) == TR_STOP) continue;

		signal2(sig, (sigcst_t)(signallist[i].func));
	}
}

static VOID NEAR resetsignal(forced)
int forced;
{
#if	!MSDOS && defined (NOJOB)
	p_id_t tmp;
#endif
#if	defined (SIGCHLD) && !defined (NOJOB)
	int n;
#endif
	int i, duperrno;

	duperrno = errno;
#if	!MSDOS
# ifdef	NOJOB
	do {
		tmp = Xwait3(NULL, WNOHANG | WUNTRACED, NULL);
	} while (tmp > (p_id_t)0 || (tmp < (p_id_t)0 && errno == EINTR));
# else
	notifyjob();
	stopped = 0;
# endif
#endif	/* !MSDOS */
	if (havetty() && interrupted && interactive && !nottyout) {
		Xfflush(Xstdout);
		VOID_C fputnl(Xstderr);
	}
	interrupted = 0;
	if (setsigflag) {
		if (forced) setsigflag = 1;
		if (!(--setsigflag)) {
#if	defined (FD) && defined (SIGALRM)
			noalrm--;
#endif
			for (i = 0; i < NSIG; i++) {
#if	defined (SIGCHLD) && !defined (NOJOB)
				if (i == SIGCHLD && bgnotify) {
					for (n = 0; n < maxjobs; n++)
						if (joblist[n].pids) break;
					if (n < maxjobs) continue;
				}
#endif
				if (oldsigfunc[i] != SIG_ERR)
					signal2(i, oldsigfunc[i]);
			}
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
	sigcst_t ofunc;
#else
	sigmask_t mask, omask;
#endif
	int i, dupinterrupted;

#if	!MSDOS
	if (!havetty()) return;
#endif
	for (i = 0; i < NSIG; i++) {
		if (!(trapmode[i] & TR_CATCH)) continue;
		trapmode[i] &= ~TR_CATCH;
		if (!trapcomm[i] || !*(trapcomm[i])) continue;
#if	MSDOS
		ofunc = signal2(i, SIG_IGN);
#else
		Xsigemptyset(mask);
		Xsigaddset(mask, i);
		Xsigblock(omask, mask);
#endif
		dupinterrupted = interrupted;
		interrupted = 0;
		_dosystem(trapcomm[i]);
		interrupted = dupinterrupted;
#if	MSDOS
		signal2(i, ofunc);
#else
		Xsigsetmask(omask);
#endif
	}
}

static VOID NEAR killmyself(sig)
int sig;
{
	prepareexit(0);
	signal2(sig, SIG_DFL);
	VOID_C kill(mypid, sig);

	exit(sig + 128);
}

static int NEAR trap_common(sig)
int sig;
{
#if	!MSDOS && defined (DEP_PTY) && defined (CYGWIN)
	int n;
#endif
#if	!MSDOS
	sigmask_t mask, omask;
#endif
	int i, trapped, flags, duperrno, duptrapok;

	duperrno = errno;
	duptrapok = trapok;
	trapok = -1;
#if	!MSDOS
	Xsigemptyset(mask);
	Xsigaddset(mask, sig);
	Xsigblock(omask, mask);		/* for obsolete signal() */
#endif

	for (i = 0; signallist[i].sig >= 0; i++)
		if (sig == signallist[i].sig) break;
	if (signallist[i].sig < 0) {
		signal2(sig, SIG_DFL);
#if	!MSDOS
		Xsigsetmask(omask);
#endif
		errno = duperrno;
		trapok = duptrapok;
		VOID_C kill(mypid, sig);
		return(0);
	}

	trapped = 0;
	flags = signallist[i].flags;

	if (mypid != shellpid) {
		if ((flags & TR_BLOCK) && (flags & TR_STAT) == TR_TERM)
			trapped = -1;
	}
	else if ((trapmode[sig] & TR_STAT) == TR_TRAP) trapped = 1;
	else if (flags & TR_BLOCK) /*EMPTY*/;
	else if ((flags & TR_STAT) == TR_TERM) trapped = -1;

	if (trapped > 0) {
		trapmode[sig] |= TR_CATCH;
		if (duptrapok > 0) exectrapcomm();
	}
	else if (trapped < 0 && *(signallist[i].mes)) {
		Xfputs(signallist[i].mes, Xstderr);
		VOID_C fputnl(Xstderr);
	}

#if	!MSDOS && defined (DEP_PTY) && defined (CYGWIN)
	if (mychildren && (flags & TR_STAT) == TR_TERM) {
		for (n = 0; mychildren[n] >= (p_id_t)0; n++) {
			VOID_C kill(mychildren[n], sig);
# ifdef	SIGCONT
			VOID_C kill(mychildren[n], SIGCONT);
# endif
		}
		free2(mychildren);
		mychildren = NULL;
	}
#endif	/* !MSDOS && DEP_PTY && CYGWIN */

	if (trapped < 0) {
#if	!MSDOS
		Xsigsetmask(omask);
#endif
		killmyself(sig);
	}

	switch (sig) {
#ifdef	SIGHUP
		case SIGHUP:
			if (trapped) break;
# ifndef	NOJOB
			if (havetty()) killjob();
# endif
			if (oldsigfunc[sig] && oldsigfunc[sig] != SIG_ERR
			&& oldsigfunc[sig] != SIG_DFL
			&& oldsigfunc[sig] != SIG_IGN) {
# ifdef	SIGFNCINT
				(*oldsigfunc[sig])(sig);
# else
				(*oldsigfunc[sig])();
# endif
			}
#if	!MSDOS
			Xsigsetmask(omask);
#endif
			killmyself(sig);
			break;
#endif	/* SIGHUP */
#if	!MSDOS && defined (SIGCONT)
		case SIGCONT:
# ifdef	FD
			suspended = 1;
# endif
			sigconted = 1;
			break;
#endif	/* !MSDOS && SIGCONT */
#ifdef	SIGINT
		case SIGINT:
			interrupted = 1;
			break;
#endif
#if	defined (SIGCHLD) && (!defined (NOJOB) || defined (SYSV))
		case SIGCHLD:
# ifndef	NOJOB
			if (bgnotify) {
				notifyjob();
				break;
			}
# endif
# ifdef	SYSV
	/* TIPS for obsolete SystemV signal() */
			while (waitjob(-1, NULL, WNOHANG | WUNTRACED) > 0);
# endif
			break;
#endif	/* SIGCHLD && (!NOJOB || SYSV) */
#if	defined (FD) && defined (SIGWINCH)
		case SIGWINCH:
			pollscreen(1);
			break;
#endif	/* FD && SIGWINCH */
		default:
			break;
	}

	if (signallist[i].func) signal2(sig, (sigcst_t)(signallist[i].func));
#if	!MSDOS
	Xsigsetmask(omask);
#endif
	errno = duperrno;
	trapok = duptrapok;

	return(trapped);
}

#ifdef	SIGHUP
static int trap_hup(VOID_A)
{
	return(trap_common(SIGHUP));
}
#endif

#ifdef	SIGINT
static int trap_int(VOID_A)
{
	return(trap_common(SIGINT));
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
#endif	/* SIGIOT */

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
#endif	/* SIGIO */

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
	exectrapcomm();
	if (mypid == orgpid && !noexit && (trapmode[0] & TR_STAT) == TR_TRAP) {
		trapmode[0] = 0;
		if (trapcomm[0] && *(trapcomm[0])) _dosystem(trapcomm[0]);
	}
#ifndef	NOJOB
	if (ttyio >= 0 && ttypgrp >= (p_id_t)0 && oldttypgrp >= (p_id_t)0)
		settcpgrp(ttyio, oldttypgrp);
#endif
#ifdef	DEP_URLPATH
	urlallclose();
#endif

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
	exportsize = (u_long)0;
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
		signal2(i, SIG_IGN);
		free2(trapcomm[i]);
		trapcomm[i] = NULL;
		signal2(i, oldsigfunc[i]);
	}
# ifndef	NOJOB
	if (joblist) {
		for (i = 0; i < maxjobs; i++) {
			if (!(joblist[i].pids)) continue;
			free2(joblist[i].pids);
			free2(joblist[i].stats);
			if (joblist[i].trp) {
				freestree(joblist[i].trp);
				free2(joblist[i].trp);
			}
			joblist[i].pids = NULL;
		}
		maxjobs = 0;
		free2(joblist);
		joblist = NULL;
	}
# endif	/* !NOJOB */
# if	defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) \
|| defined (DEP_PSEUDOPATH)
	freeopenlist();
# endif
#endif	/* DEBUG */
	errno = duperrno;
}

VOID Xexit2(n)
int n;
{
	if (mypid == orgpid) {
#ifndef	NOJOB
		if (loginshell && interactive_io) killjob();
#endif
#ifdef	FD
		if (havetty() && interactive && !nottyout) {
			if (!dumbterm) putterm(T_NORMAL);
			endterm();
			inittty(1);
			keyflush();
		}
#endif
		exec_line(NULL);
		_dosystem(NULL);
	}
	prepareexit(0);
#ifdef	FD
	if (mypid == orgpid) prepareexitfd(n);
#endif

#ifdef	DEBUG
	if (havetty() && mypid == orgpid) {
# if	!MSDOS && defined (FD)
		freeterment();
# endif
		closetty(&ttyio, &ttyout);
		muntrace();
	}
#endif	/* DEBUG */

	exit(n);
}

#ifdef	FD
static VOID NEAR argfputs(s, fp)
CONST char *s;
XFILE *fp;
{
# ifdef	DEP_FILECONV
	char *cp;
# endif

# ifdef	DEP_FILECONV
	cp = newkanjiconv(s, defaultkcode, DEFCODE, L_FNAME);
	fprintf2(fp, "%a", cp);
	if (cp != s) free2(cp);
# else
	fprintf2(fp, "%a", s);
# endif
}
#endif	/* FD */

#ifdef	DEP_FILECONV
static int NEAR Kopen(path, flags, mode)
CONST char *path;
int flags, mode;
{
	char buf[MAXPATHLEN];

	path = kanjiconv2(buf, path,
		MAXPATHLEN - 1, defaultkcode, DEFCODE, L_FNAME);

	return(Xopen(path, flags, mode));
}
#endif	/* DEP_FILECONV */

static VOID NEAR syntaxerror(s)
CONST char *s;
{
	if (syntaxerrno <= 0 || syntaxerrno >= SYNTAXERRSIZ) return;
#ifndef	BASHSTYLE
	/* bash shows its name, even in interective shell */
	if (interactive) /*EMPTY*/;
	else
#endif
	if (argvar && argvar[0]) fprintf2(Xstderr, "%k: ", argvar[0]);
#ifndef	MINIMUMSHELL
	if (!interactive && shlineno > 0L)
		fprintf2(Xstderr, "%ld: ", shlineno);
#endif
	if (s) fprintf2(Xstderr, "%a: ",
		(*s && syntaxerrno != ER_UNEXPNL) ? s : "syntax error");
	if (syntaxerrstr[syntaxerrno])
		Xfputs(syntaxerrstr[syntaxerrno], Xstderr);
	VOID_C fputnl(Xstderr);
	ret_status = RET_SYNTAXERR;
	if (errorexit) Xexit2(RET_SYNTAXERR);
	safeexit();
}

/*ARGSUSED*/
VOID execerror(argv, s, n, noexit)
char *CONST *argv;
CONST char *s;
int n, noexit;
{
	if (n == ER_BADSUBST
	&& (execerrno == ER_ISREADONLY || execerrno == ER_PARAMNOTSET))
		return;

	if (!n || n >= EXECERRSIZ) return;
#ifndef	BASHSTYLE
	/* bash shows its name, even in interective shell */
	if (interactive) /*EMPTY*/;
	else
#endif
	if (argvar && argvar[0]) fprintf2(Xstderr, "%k: ", argvar[0]);
#ifndef	MINIMUMSHELL
	if (!interactive && shlineno > 0L)
		fprintf2(Xstderr, "%ld: ", shlineno);
#endif
	if (argv && argv[0]) fprintf2(Xstderr, "%k: ", argv[0]);
	if (s) fprintf2(Xstderr, "%a: ", s);
	if (execerrstr[n]) Xfputs(execerrstr[n], Xstderr);
	VOID_C fputnl(Xstderr);
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
	else if (noexit) /*EMPTY*/;
#if	!defined (BASHSTYLE) || defined (STRICTPOSIX)
	/* bash does not exit on error, in non interactive shell */
	else if (isshellbuiltin) safeexit();
#endif
}

VOID doperror(command, s)
CONST char *command, *s;
{
	int duperrno;

	duperrno = errno;
	if (errno < 0) return;
	if (!command) command = (argvar && argvar[0]) ? argvar[0] : shellname;
	fprintf2(Xstderr, "%k: ", command);
	if (s) fprintf2(Xstderr, "%a: ", s);
	Xfputs(strerror2(duperrno), Xstderr);
	VOID_C fputnl(Xstderr);
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
CONST char *s;
{
	int i, n;

	i = 0;
	if ((n = getnum(s, &i)) < 0) return(-1);
#ifndef	BASHSTYLE
	/* bash always treats non numeric character as error */
	if (s[i]) return(-1);
#endif

	return(n);
}

#if	!MSDOS
VOID dispsignal(sig, width, fp)
int sig, width;
XFILE *fp;
{
	int i;

	for (i = 0; signallist[i].sig >= 0; i++)
		if (sig == signallist[i].sig) break;
	if (signallist[i].sig < 0) {
		if ((width -= strsize("Signal ")) < 0) width = 0;
		fprintf2(fp, "Signal %-*d", width, sig);
	}
	else if (!width) Xfputs(signallist[i].mes, fp);
	else fprintf2(fp, "%-*.*s", width, width, signallist[i].mes);
}

int waitjob(pid, wp, opt)
p_id_t pid;
wait_pid_t *wp;
int opt;
{
# if	!defined (NOJOB) || defined (SYSV)
	static wait_pid_t *waitlist = NULL;
	static p_id_t *pidlist = NULL;
	static int maxtrap = 0;
# endif
# ifndef	NOJOB
	int i, j, sig;
# endif
	sigmask_t mask, omask;
	wait_pid_t w;
	p_id_t tmp;
	int ret;

	Xsigemptyset(mask);
# ifdef	SIGCHLD
	Xsigaddset(mask, SIGCHLD);
# endif
	Xsigblock(omask, mask);
	for (;;) {
# ifndef	JOBTEST
		tmp = (pid < (p_id_t)0)
			? Xwait3(&w, opt, NULL) : Xwait4(pid, &w, opt, NULL);
# else	/* JOBTEST */
		tmp = Xwait3(&w, opt, NULL);
		if (pid >= (p_id_t)0 && tmp >= (p_id_t)0 && pid != tmp) {
			if ((i = searchjob(tmp, &j)) < 0) continue;
			sig = joblist[i].stats[j];
			joblist[i].stats[j] = getwaitsig(w);
#  if	defined (SIGCONT) && (defined (SIGTTIN) || defined (SIGTTOU))
			if (sig == joblist[i].stats[j]) sig = 0;
#   ifdef	SIGTTIN
			else if (joblist[i].stats[j] == SIGTTIN) /*EMPTY*/;
#   endif
#   ifdef	SIGTTOU
			else if (joblist[i].stats[j] == SIGTTOU) /*EMPTY*/;
#   endif
			else sig = 0;

			if (sig) {
				usleep(100000L);
				VOID_C kill(tmp, SIGCONT);
			}
#  endif	/* SIGCONT  && (SIGTTIN || SIGTTOU) */
			continue;
		}
# endif	/* JOBTEST */
		if (tmp >= (p_id_t)0 || errno != EINTR) break;
	}

# if	!defined (NOJOB) || defined (SYSV)
	if (pid >= (p_id_t)0) {
		if (tmp < (p_id_t)0 && errno == ECHILD) {
			while (maxtrap-- > 0) if (pid == pidlist[maxtrap]) {
				tmp = pidlist[maxtrap];
				w = waitlist[maxtrap];
				break;
			}
		}
		free2(waitlist);
		waitlist = NULL;
		free2(pidlist);
		pidlist = NULL;
		maxtrap = 0;
	}
	else if (tmp >= (p_id_t)0) {
		waitlist = (wait_pid_t *)realloc2(waitlist,
			(maxtrap + 1) * sizeof(wait_pid_t));
		pidlist = (p_id_t *)realloc2(pidlist,
			(maxtrap + 1) * sizeof(p_id_t));
		pidlist[maxtrap] = tmp;
		waitlist[maxtrap] = w;
		maxtrap++;
	}
# endif	/* !NOJOB || SYSV */
	Xsigsetmask(omask);

	if (!tmp) return(0);
	else if (tmp < (p_id_t)0) return(-1);
	else {
		ret = (pid < (p_id_t)0 || tmp == pid) ? 1 : 0;
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
			dispjob(i, Xstderr);
	}
# endif
	if (wp) *wp = w;

	return(ret);
}

#if	!MSDOS && defined (DEP_PTY) && defined (CYGWIN)
static VOID NEAR addmychild(pid)
p_id_t pid;
{
	int n;

	n = 0;
	if (mychildren) while (mychildren[n] >= (p_id_t)0) n++;
	mychildren = (p_id_t *)realloc2(mychildren, (n + 2) * sizeof(p_id_t));
	mychildren[n++] = pid;
	mychildren[n] = (p_id_t)-1;
}
#endif	/* !MSDOS && DEP_PTY && CYGWIN */

static VOID NEAR setstopsig(valid)
int valid;
{
	sigcst_t func;
	int i;

	for (i = 0; signallist[i].sig >= 0; i++) {
		func = (valid) ? SIG_DFL : (sigcst_t)(signallist[i].func);
		if ((signallist[i].flags & TR_STAT) == TR_STOP) {
			if (valid < 0) func = SIG_IGN;
		}
		else if (!(signallist[i].flags & TR_BLOCK)) continue;

		signal2(signallist[i].sig, func);
	}
}

/*ARGSUSED*/
static p_id_t NEAR makechild(tty, ppid, stop)
int tty;
p_id_t ppid;
int stop;
{
# ifdef	DEBUG
	extern VOID (*__free_hook) __P_((VOID_P));
	extern VOID_P (*__malloc_hook) __P_((ALLOC_T));
	extern VOID_P (*__realloc_hook) __P_((VOID_P, ALLOC_T));
# endif
	sigmask_t mask, omask;
	p_id_t pid;
	int i;

	Xsigfillset(mask);
	Xsigblock(omask, mask);
	if (!(pid = Xfork())) {
# ifdef	DEBUG
		__free_hook = NULL;
		__malloc_hook = NULL;
		__realloc_hook = NULL;
# endif

		memcpy((char *)&omask, (char *)&oldsigmask, sizeof(sigmask_t));
# ifdef	SIGCHLD
		Xsigdelset(omask, SIGCHLD);
# endif
		mypid = getpid();
		if (!stop) {
# ifdef	NOJOB
			if (!loginshell) stop = -1;
# else
			if (!jobok) stop = -1;
			else if (tty && childpgrp != orgpgrp) stop = 1;
# endif
		}
		setstopsig(stop);
	}
	Xsigsetmask(omask);
	if (pid < (p_id_t)0) return(pid);

# ifndef	NOJOB
	if (childpgrp < (p_id_t)0)
		childpgrp = (ppid >= (p_id_t)0) ? ppid : (pid) ? pid : mypid;
	if (pid) {
		if (jobok) VOID_C setpgroup(pid, childpgrp);
		if (tty && ttypgrp >= (p_id_t)0) ttypgrp = childpgrp;
#  if	defined (DEP_PTY) && defined (CYGWIN)
		if (parentfd >= 0) addmychild(pid);
#  endif
	}
	else {
		if (jobok && setpgroup(mypid, childpgrp) < 0) {
			doperror(NULL, "fatal error");
			prepareexit(-1);
			Xexit(RET_FATALERR);
		}
		if (tty) gettermio(childpgrp, jobok);
#  if	defined (DEP_PTY) && defined (CYGWIN)
		free2(mychildren);
		mychildren = NULL;
#  endif
	}
# endif	/* !NOJOB */

	if (!pid && shellfunc) for (i = 0; shellfunc[i].ident; i++)
		nownstree(shellfunc[i].func);

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
# endif	/* !NOJOB */
	wait_pid_t w;
	int ret;

	if (trapok >= 0) trapok = 1;
	sigconted = 0;
	for (;;) {
		if (interrupted) {
			if (trapok >= 0) trapok = 0;
# ifdef	FD
			checkscreen(-1, -1);
# endif
			return(RET_INTR);
		}
		if ((ret = waitjob(pid, &w, WUNTRACED)) < 0) break;
		if (!ret) continue;
		if (!WIFSTOPPED(w)) break;

		ret = WSTOPSIG(w);
		if (sigconted) {
			sigconted = 0;
# ifdef	SIGSTOP
			if (ret == SIGSTOP) continue;
# endif
		}

# ifdef	NOJOB
#  ifdef	SIGCONT
		if (loginshell) VOID_C kill(pid, SIGCONT);
#  endif
# else	/* !NOJOB */
#  ifdef	BASHBUG
	/* bash cannot be suspended with +m mode */
		if (!jobok) continue;
#  endif
		if (mypid != orgpgrp) continue;

		if (trapok >= 0) trapok = 0;
		gettermio(orgpgrp, jobok);
#  ifdef	FD
		checkscreen(-1, -1);
#  endif
		prevjob = lastjob;
		lastjob = stackjob(pid, ret, trp);
		breaklevel = loopdepth;
		stopped = 1;
		if (!jobok) return(RET_SUCCESS);
		if (tioctl(ttyio, REQGETP, &tty) >= 0) {
			free2(joblist[lastjob].tty);
			joblist[lastjob].tty =
				(termioctl_t *)malloc2(sizeof(tty));
			memcpy((char *)(joblist[lastjob].tty), (char *)&tty,
				sizeof(tty));
#  ifndef	FD
#   ifdef	USESGTTY
			tty.sg_flags |= ECHO | CRMOD;
			tty.sg_flags &= ~(RAW | CBREAK | XTABS);
#   else
			tty.c_lflag |= (TIO_ICOOKED | TIO_LECHO);
			tty.c_lflag &= ~(PENDIN | ECHONL);
			tty.c_iflag |= (TIO_ICOOKED | ICRNL);
			tty.c_iflag &= TIO_INOCOOKED;
			tty.c_oflag |= TIO_ONL;
			tty.c_oflag &= (TIO_ONONL & ~TAB3);
#   endif
			tioctl(ttyio, REQSETP, &tty);
#  endif	/* !FD */
		}
#  ifdef	USESGTTY
		if (ioctl(ttyio, TIOCLGET, &lflag) >= 0) {
			free2(joblist[lastjob].ttyflag);
			joblist[lastjob].ttyflag =
				(int *)malloc2(sizeof(lflag));
			memcpy((char *)(joblist[lastjob].ttyflag),
				(char *)&lflag, sizeof(lflag));
#   ifndef	FD
			lflag |= LPASS8 | LCRTBS | LCRTERA | LCRTKIL | LCTLECH;
			lflag &= ~(LLITOUT | LPENDIN);
			ioctl(ttyio, TIOCLSET, &lflag);
#   endif
		}
#  endif	/* USESGTTY */
		if (interactive && !nottyout) {
#  ifdef	FD
			stdiomode();
#  endif
			dispjob(lastjob, Xstderr);
		}

		return(RET_SUCCESS);
# endif	/* !NOJOB */
	}
	if (trapok >= 0) trapok = 0;

# ifndef	NOJOB
	if (mypid == orgpgrp) {
		gettermio(orgpgrp, jobok);
#  ifdef	FD
		checkscreen(-1, -1);
#  endif
		childpgrp = (p_id_t)-1;
	}
# endif	/* !NOJOB */
	if (ret < 0) {
		if (!trp && errno == ECHILD) ret = errno = 0;
	}
	else if (WIFSIGNALED(w)) {
		ret = WTERMSIG(w);

# ifndef	NOJOB
		if ((i = searchjob(pid, NULL)) >= 0)
			Xkillpg(joblist[i].pids[0], ret);
# endif
# ifdef	SIGINT
		if (ret == SIGINT) interrupted = 1;
		else
# endif
# if	defined (SIGPIPE) && !defined (PSIGNALSTYLE)
		if (ret == SIGPIPE) /*EMPTY*/;
		else
# endif
		if (!nottyout) {
			if (!interactive && argvar && argvar[0])
				fprintf2(Xstderr, "%k: %id ", argvar[0], pid);
			dispsignal(ret, 0, Xstderr);
			VOID_C fputnl(Xstderr);
		}
		ret += 128;
	}
	else ret = (WEXITSTATUS(w) & 0377);

	if (ret < 0) return(-1);
# ifndef	NOJOB
	if ((i = searchjob(pid, &j)) >= 0 && j == joblist[i].npipe) {
		free2(joblist[i].pids);
		free2(joblist[i].stats);
		if (joblist[i].trp) {
			freestree(joblist[i].trp);
			free2(joblist[i].trp);
		}
		joblist[i].pids = NULL;
	}
# endif	/* !NOJOB */

	return((int)ret);
}
#endif	/* !MSDOS */

static VOID NEAR safermtmpfile(file)
CONST char *file;
{
	int duperrno;

	duperrno = errno;
	rmtmpfile(file);
	errno = duperrno;
}

VOID setshflag(n, val)
int n, val;
{
#if	defined (FD) && !defined (_NOEDITMODE)
	CONST char *cp;
#endif

	*(shflaglist[n].var) = val;

#if	defined (FD) && !defined (_NOEDITMODE)
	if (shflaglist[n].var == &emacsmode) cp = "emacs";
	else if (shflaglist[n].var == &vimode) cp = "vi";
	else cp = NULL;

	if (cp) {
		emacsmode = vimode = 0;
		if (editmode && !strcmp(editmode, cp)) cp = NULL;

		/*
		 * The stupid HP cc says that
		 * ternary operator with NULL is const char *,
		 * not to be converted to char *.
		 */
		if (!val) cp = (cp) ? NULL : nullstr;

		if (cp) {
			setenv2("FD_EDITMODE", cp, 0);
			editmode = getconstvar("FD_EDITMODE");
		}
	}
#endif	/* FD && !_NOEDITMODE */
#ifdef	DEP_PTY
	sendparent(TE_SETSHFLAG, n, val);
#endif
}

static int NEAR getoption(argc, argv, isopt)
int argc;
char *CONST *argv;
int isopt;
{
	u_long flags;
	CONST char *arg;
	int i, j, com;

#if	defined (FD) && !defined (_NOEDITMODE)
	emacsmode = vimode = 0;
	if (editmode) {
		if (!strcmp(editmode, "emacs")) emacsmode = 1;
		else if (!strcmp(editmode, "vi")) vimode = 1;
	}
#endif

	if (argc <= 1) return(1);
	arg = argv[1];
	if (arg[0] != '-' && arg[0] != '+') return(1);
	else if (arg[1] == '-') return(2);

	com = 0;
	flags = (u_long)0;
	for (i = 1; arg[i]; i++) {
		if (isopt) {
#ifdef	FD
			if (fdmode) /*EMPTY*/;
			else
#endif
			if (arg[i] == 'c' && !com && argc > 2) {
				com = 1;
				continue;
			}
		}
		else {
			if (arg[i] == 'o' && !com) {
				com = (arg[0] == '-') ? 1 : 2;
				continue;
			}
		}
		for (j = 0; j < FLAGSSIZ; j++) {
			if (!isopt && !(shflaglist[j].ident)) continue;
			if (arg[i] == shflaglist[j].letter) break;
		}
		if (j < FLAGSSIZ) flags |= ((u_long)1 << j);
		else if (arg[0] == '-') {
			execerror(argv, arg, ER_BADOPTIONS, 0);
			return(-1);
		}
	}

	for (j = 0; j < FLAGSSIZ; j++) {
		if (flags & (u_long)1) setshflag(j, (arg[0] == '-') ? 1 : 0);
		flags >>= 1;
	}

	return(com + 2);
}

static ALLOC_T NEAR c_allocsize(n)
int n;
{
	ALLOC_T size;

	n++;
	for (size = BUFUNIT; size < n; size *= 2) /*EMPTY*/;

	return(size);
}

static int NEAR readchar(fd)
int fd;
{
	u_char ch;
	int n;

	if (trapok >= 0) trapok = 1;
#ifdef	USESIGACTION
	n = sureread(fd, &ch, sizeof(ch));
#else
	n = checkread(fd, &ch, sizeof(ch), 0);
#endif
	if (n > 0) n = (int)ch;
	else if (!n) n = READ_EOF;
	if (trapok >= 0) trapok = 0;

	return(n);
}

/*ARGSUSED*/
static char *NEAR readline(fd, opt)
int fd, opt;
{
	char *cp;
	ALLOC_T i, size;
	int c;

	cp = c_realloc(NULL, 0, &size);
	for (i = 0; (c = readchar(fd)) != '\n'; i++) {
		if (c < 0) {
			free2(cp);
			return(NULL);
		}
		if (c == READ_EOF) {
			if (i) break;
			free2(cp);
			return(vnullstr);
		}
		cp = c_realloc(cp, i, &size);
		cp[i] = c;
	}
#ifndef	USECRNL
	if (opt != 'N') /*EMPTY*/;
	else
#endif
	if (c == '\n' && i > 0 && cp[i - 1] == '\r') i--;
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
			free2(cp);
			return(NULL);
		}
#ifdef	USECRNL
		if (c == '\n' && i > 0 && cp[i - 1] == '\r') i--;
#endif
		cp = c_realloc(cp, i, &size);
		cp[i] = c;
	}
	if (lenp) *lenp = i;
	cp[i++] = '\0';

	return(realloc2(cp, i));
}

char *evalvararg(arg, flags, noexit)
char *arg;
int flags, noexit;
{
	char *tmp;

	if ((tmp = evalarg(arg, flags))) return(tmp);
#if	defined (BASHSTYLE) && defined (STRICTPOSIX)
	if (!noexit) noexit = -1;
#endif
	if (*arg) execerror(NULL, arg, ER_BADSUBST, noexit);

	return(NULL);
}

static heredoc_t *NEAR newheredoc(eof, path, fd, flags)
char *eof, *path;
int fd, flags;
{
	heredoc_t *new;

	new = (heredoc_t *)malloc2(sizeof(heredoc_t));
	new -> eof = eof;
	new -> filename = path;
	new -> buf = NULL;
	new -> fd = fd;
#ifndef	USEFAKEPIPE
	new -> pipein = (p_id_t)-1;
#endif
	new -> flags = flags;

	return(new);
}

VOID freeheredoc(hdp, nown)
heredoc_t *hdp;
int nown;
{
	if (!hdp) return;
	free2(hdp -> eof);
	safeclose(hdp -> fd);
	if (hdp -> filename) {
		if (!nown) safermtmpfile(hdp -> filename);
		free2(hdp -> filename);
	}
	free2(hdp -> buf);
	free2(hdp);
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
	new -> type = (u_short)type;
	new -> new = new -> old = -1;
#ifdef	DEP_DOSDRIVE
	new -> fakepipe = NULL;
	new -> dosfd = -1;
#endif
	new -> next = next;

	return(new);
}

VOID freerlist(rp, nown)
redirectlist *rp;
int nown;
{
	if (!rp) return;
	freerlist(rp -> next, nown);

	if (rp -> type & MD_HEREDOC)
		freeheredoc((heredoc_t *)(rp -> filename), nown);
	else free2(rp -> filename);
	free2(rp);
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

VOID freecomm(comm, nown)
command_t *comm;
int nown;
{
	int i;

	if (!comm) return;
	if (comm -> argv) {
		if (isstatement(comm))
			freestree((syntaxtree *)(comm -> argv));
		else for (i = 0; i <= comm -> argc; i++)
			free2(comm -> argv[i]);
		free2(comm -> argv);
	}
	if (comm -> redp) {
		closeredirect(comm -> redp);
		freerlist(comm -> redp, nown);
	}
	free2(comm);
}

syntaxtree *newstree(parent)
syntaxtree *parent;
{
	syntaxtree *new;

	new = (syntaxtree *)malloc2(sizeof(syntaxtree));
	new -> comm = NULL;
	new -> parent = parent;
	new -> next = NULL;
#ifndef	MINIMUMSHELL
	new -> lineno = shlineno;
#endif
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
#ifndef	MINIMUMSHELL
	redirectlist *rp;
#endif
	int duperrno;

	if (!trp) return;

	duperrno = errno;
	if (trp -> comm) {
		if (!(trp -> flags & ST_NODE))
			freecomm(trp -> comm, trp -> flags & ST_NOWN);
		else {
			freestree((syntaxtree *)(trp -> comm));
			free2(trp -> comm);
		}
		trp -> comm = NULL;
	}

	if (trp -> next) {
#ifndef	MINIMUMSHELL
		if (trp -> flags & ST_BUSY) {
			rp = (redirectlist *)(trp -> next);
			free2(rp -> filename);
			freestree((syntaxtree *)(rp -> next));
			free2(rp -> next);
		}
		else
#endif
		if (!(trp -> cont & (CN_QUOT | CN_ESCAPE)))
			freestree(trp -> next);
		else free2(((redirectlist *)(trp -> next)) -> filename);
		free2(trp -> next);
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

	if (!l1) /*EMPTY*/;
#ifndef	BASHSTYLE
	/* bash does not allow the format like as "foo | ; bar" */
	else if (parent && isoppipe(parent) && l1 > l2) /*EMPTY*/;
#endif
#ifndef	MINIMUMSHELL
	else if (type == OP_NOT && (!l2 || l1 < l2)) /*EMPTY*/;
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

	if (!l1 || !l2) /*EMPTY*/;
	else if (l1 < l2) {
		new = newstree(trp);
		new -> comm = trp -> comm;
		new -> next = trp -> next;
#ifndef	MINIMUMSHELL
		new -> lineno = trp -> lineno;
#endif
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
#ifndef	MINIMUMSHELL
			new -> lineno = trp -> lineno;
#endif
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
#ifndef	MINIMUMSHELL
	trp -> lineno = shlineno;
#endif
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

	if (!(trp -> comm)) /*EMPTY*/;
	else if (trp -> flags & ST_NODE)
		nownstree((syntaxtree *)(trp -> comm));
	else if (isstatement(trp -> comm)) nownstree(statementbody(trp));

	trp -> flags |= ST_NOWN;
	nownstree(trp -> next);
}

static int NEAR evalfiledesc(s)
CONST char *s;
{
	int i, n;

	i = 0;
	if ((n = getnum(s, &i)) < 0) return(-1);

	i = Xgetdtablesize();
	if (n > i) n = i;

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
		mode |= (O_WRONLY | O_CREAT);
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

static int NEAR cancelredirect(rp)
redirectlist *rp;
{
	if (!rp) return(0);
#ifdef	DEP_DOSDRIVE
	if (rp -> fakepipe) {
		errno = EBADF;
		doperror(NULL, rp -> filename);
		return(-1);
	}
#endif
	if (cancelredirect(rp -> next) < 0) return(-1);

	if ((rp -> type & MD_WITHERR) && rp -> fd != STDOUT_FILENO) {
		safeclose(rp -> fd);
		rp -> fd = STDOUT_FILENO;
	}

	if (rp -> old >= 0) safeclose(rp -> old);
	if (rp -> new >= 0) {
		if (rp -> type & MD_HEREDOC) closepipe2(rp -> new, rp -> fd);
		else if (!(rp -> type & MD_FILEDESC)) safeclose(rp -> new);
	}
#ifdef	DEP_DOSDRIVE
	closepseudofd(rp);
#endif
	rp -> old = rp -> new = -1;

	return(0);
}

static VOID NEAR closeredirect(rp)
redirectlist *rp;
{
#ifndef	USEFAKEPIPE
	heredoc_t *hdp;
#endif

	if (!rp) return;
	closeredirect(rp -> next);
	if (rp -> type & MD_DUPL) return;

	if ((rp -> type & MD_WITHERR) && rp -> fd != STDOUT_FILENO) {
		if (rp -> fd != STDERR_FILENO) {
			Xdup2(rp -> fd, STDERR_FILENO);
			safeclose(rp -> fd);
		}
		rp -> fd = STDOUT_FILENO;
	}

	if (rp -> old >= 0) {
		Xdup2(rp -> old, rp -> fd);
		safeclose(rp -> old);
	}
	if (rp -> new >= 0) {
		if (rp -> type & MD_HEREDOC) {
#ifndef	USEFAKEPIPE
			hdp = (heredoc_t *)(rp -> filename);
			if (hdp -> pipein >= (p_id_t)0)
				VOID_C waitchild(hdp -> pipein, NULL);
#endif
			closepipe2(rp -> new, -1);
		}
		else if (!(rp -> type & MD_FILEDESC)) safeclose(rp -> new);
	}
#ifdef	DEP_DOSDRIVE
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

	if (!(trp -> comm)) /*EMPTY*/;
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
		if (!rm && !(trp -> cont & CN_HDOC)) /*EMPTY*/;
		else for (rp = (trp -> comm) -> redp; rp; rp = rp -> next) {
			if (!(rp -> type & MD_HEREDOC)) continue;
			if (!(hdp = (heredoc_t *)(rp -> filename))) continue;

			if (!rm) {
				if (hdp -> fd >= 0) hit = hdp;
			}
			else if (hdp -> filename) {
				safermtmpfile(hdp -> filename);
				free2(hdp -> filename);
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
CONST char *s;
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
			strcpy2(&(cp[len]), s);
		}
	}

	if (cp) {
		if (!(hdp -> flags & HD_QUOTED)) {
			for (i = 0; cp[i]; i++) {
				if (cp[i] == PESCAPE) {
					if (!cp[i + 1]) break;
					i++;
				}
				else if (iswchar(cp, i)) i++;
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
			free2(cp);
			s = cp = NULL;
		}
	}
#ifdef	FAKEUNINIT
	else i = 0;	/* fake for -Wuninitialized */
#endif

	ret = 1;
	if (cp) {
#ifdef	FD
		demacroarg(&cp);
#endif
		len = strlen(cp);
		cp[len++] = '\n';
		len -= i;
		if (surewrite(hdp -> fd, &(cp[i]), len) < 0) ret = -1;
		free2(cp);
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
	char *cp, *buf;
	p_id_t pipein;
	int fd, fdin, ret;

	fdin = newdup(Xopen(hdp -> filename, O_BINARY | O_RDONLY, 0666));
	if (fdin < 0) return(-1);
#ifdef	USEFAKEPIPE
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

	if (pipein > (p_id_t)0) {
#ifndef	USEFAKEPIPE
		hdp -> pipein = pipein;
#endif
		safeclose(fdin);
		return(fd);
	}

	ret = RET_SUCCESS;
	while ((buf = readline(fdin, '\0')) != vnullstr) {
		if (!buf) {
			closepipe2(fd, -1);
			safeclose(fdin);
			return(-1);
		}

		if (!(hdp -> flags & HD_QUOTED) && ret == RET_SUCCESS) {
			cp = evalvararg(buf,
				EA_STRIPESCAPE
				| EA_NOEVALQ | EA_NOEVALDQ | EA_BACKQ, 1);
			if (!cp) ret = RET_FAIL;
			free2(buf);
			buf = cp;
		}
		if (ret == RET_SUCCESS) {
			Xfputs(buf, Xstdout);
			VOID_C fputnl(Xstdout);
		}
		free2(buf);
	}
	safeclose(fdin);
	Xfflush(Xstdout);
	if ((fd = reopenpipe(fd, ret)) < 0) return(-1);
	if (ret != RET_SUCCESS) {
		closepipe2(fd, -1);
		return(seterrno(-1));
	}

	return(fd);
}

#ifdef	DEP_DOSDRIVE
static int NEAR fdcopy(fdin, fdout)
int fdin, fdout;
{
	u_char ch;
	int n;

	for (;;) {
		if ((n = sureread(fdin, &ch, sizeof(ch))) <= 0) break;
		if (surewrite(fdout, &ch, sizeof(ch)) < 0) return(-1);
	}

	return(n);
}

static int NEAR openpseudofd(rp)
redirectlist *rp;
{
# ifndef	USEFAKEPIPE
	p_id_t pid;
	int fds[2];
# endif
	char pfile[MAXPATHLEN];
	int fd;

	if (!(rp -> type & MD_RDWR) || (rp -> type & MD_RDWR) == MD_RDWR)
		return(-1);

# ifndef	USEFAKEPIPE
	if (pipe(fds) < 0) /*EMPTY*/;
	else if ((pid = makechild(0, mypid, 0)) < (p_id_t)0) {
		safeclose(fds[0]);
		safeclose(fds[1]);
		return(-1);
	}
	else if (pid) {
		safeclose(rp -> new);
		if (rp -> type & MD_READ) {
			safeclose(fds[1]);
			rp -> new = newdup(fds[0]);
		}
		else {
			safeclose(fds[0]);
			rp -> new = newdup(fds[1]);
		}
		return(0);
	}
	else {
		if (rp -> type & MD_READ) {
			safeclose(fds[0]);
			fds[1] = newdup(fds[1]);
			fdcopy(rp -> new, fds[1]);
			safeclose(rp -> new);
			safeclose(fds[1]);
		}
		else {
			safeclose(fds[1]);
			fds[0] = newdup(fds[0]);
			fdcopy(fds[0], rp -> new);
			safeclose(rp -> new);
			safeclose(fds[0]);
		}
		prepareexit(1);
		Xexit(RET_SUCCESS);
	}
# endif	/* !USEFAKEPIPE */

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
	free2(rp -> fakepipe);
	rp -> fakepipe = NULL;
	rp -> dosfd = -1;

	return(0);
}
#endif	/* DEP_DOSDRIVE */

#ifdef	WITHSOCKREDIR
static int NEAR parsesocket(s, addrp, portp)
CONST char *s;
char **addrp;
int *portp;
{
	urlhost_t host;
	char *cp;
	int n, scheme;

	if (!(n = urlparse(s, schemelist, &cp, &scheme))) return(0);
	if (s[n] == '/') n++;
	if (s[n]) {
		free2(cp);
		return(0);
	}
	n = urlgethost(cp, &host);
	free2(cp);
	if (n < 0) return(-1);

	if (host.user || host.pass) n = -1;
	switch (scheme) {
		case TYPE_CONNECT:
			if (!(host.host)) n = -1;
			if (host.port < 0) n = -1;
			break;
		case TYPE_ACCEPT:
		case TYPE_BIND:
			if (host.port < 0) host.port = 0;
			break;
		default:
			n = -1;
			break;
	}

	if (n < 0) {
		urlfreehost(&host);
		return(seterrno(EINVAL));
	}
	*addrp = host.host;
	*portp = host.port;

	return(scheme);
}
#endif	/* WITHSOCKREDIR */

static redirectlist *NEAR doredirect(rp)
redirectlist *rp;
{
#ifdef	WITHSOCKREDIR
	char *addr;
	int s, port, scheme, opt;
#endif
	redirectlist *errp;
	char *tmp;
	int n, type;

#ifdef	FD
	rp -> type &= ~MD_REST;
#endif
	if (rp -> next) {
		if ((errp = doredirect(rp -> next))) return(errp);
#ifdef	FD
		if (((rp -> next) -> type) & MD_REST) rp -> type |= MD_REST;
#endif
	}

	type = rp -> type;
	if (!(rp -> filename)) tmp = NULL;
	else if (type & MD_HEREDOC) tmp = rp -> filename;
	else {
		tmp = evalvararg(rp -> filename, EA_BACKQ | EA_STRIPQLATER, 0);
		if (!tmp) {
			errno = -1;
			return(rp);
		}
		else if ((type & MD_FILEDESC) && tmp[0] == '-' && !tmp[1]) {
			free2(tmp);
			tmp = NULL;
			type &= ~MD_FILEDESC;
		}
#ifdef	FD
		else if ((n = replacearg(&tmp)) < 0) {
			free2(tmp);
			return(rp);
		}
		else if (n) rp -> type |= MD_REST;
#endif
	}

	if (!tmp) /*EMPTY*/;
	else if (type & MD_HEREDOC) {
		if ((rp -> new = openheredoc((heredoc_t *)tmp, rp -> fd)) < 0)
			return(rp);
	}
	else if (type & MD_FILEDESC) {
		rp -> new = evalfiledesc(tmp);
		free2(tmp);
		if (rp -> new < 0) {
			errno = EBADF;
			return(rp);
		}
		if (isvalidfd(rp -> new) < 0) {
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
		free2(tmp);
		errno = 0;
		return(rp);
	}
	else {
#ifdef	WITHSOCKREDIR
		if ((scheme = parsesocket(tmp, &addr, &port))) {
			if (scheme < 0) return(rp);
			opt = SCK_LOWDELAY;
			switch (scheme) {
				case TYPE_CONNECT:
					s = sockconnect(addr, port, 0, opt);
					break;
				case TYPE_ACCEPT:
					s = sockbind(addr, port,
						opt | SCK_REUSEADDR);
					if (s >= 0) s = sockaccept(s, opt);
					break;
				case TYPE_BIND:
					s = sockbind(addr, port, opt);
					break;
				default:
					s = seterrno(EINVAL);
					break;
			}
			rp -> new = s;
			free2(addr);
		}
		else
#endif	/* WITHSOCKREDIR */
		rp -> new = Kopen(tmp, redmode(type), 0666);
		rp -> new = newdup(rp -> new);
		free2(tmp);
		if (rp -> new < 0) return(rp);
#if	MSDOS && !defined (LSI_C)
# ifdef	DJGPP
		if (isatty(rp -> new)) /*EMPTY*/;
		else
# endif
		setmode(rp -> new, O_BINARY);
#endif	/* MSDOS && !LSI_C */
	}

#if	!MSDOS
	if (rp -> new != STDIN_FILENO
	&& rp -> new != STDOUT_FILENO
	&& rp -> new != STDERR_FILENO)
		closeonexec(rp -> new);
#endif

	if (isvalidfd(rp -> fd) < 0) rp -> old = -1;
	else if ((rp -> old = newdup(Xdup(rp -> fd))) < 0) return(rp);
#if	!MSDOS
	else if (rp -> old != STDIN_FILENO
	&& rp -> old != STDOUT_FILENO
	&& rp -> old != STDERR_FILENO)
		closeonexec(rp -> old);
#endif

	if (rp -> new != rp -> fd) {
		if (rp -> new < 0) VOID_C Xclose(rp -> fd);
		else {
#ifdef	DEP_DOSDRIVE
			if (chkopenfd(rp -> new) == DEV_DOS) openpseudofd(rp);
#endif
			if (Xdup2(rp -> new, rp -> fd) < 0) return(rp);
		}
	}

	if ((type & MD_WITHERR) && rp -> new != STDERR_FILENO
	&& rp -> fd == STDOUT_FILENO) {
		if ((n = newdup(Xdup(STDERR_FILENO))) < 0
		|| Xdup2(rp -> new, STDERR_FILENO) < 0) {
			safeclose(n);
			return(rp);
		}
		rp -> fd = n;
#if	!MSDOS
		if (rp -> fd != STDIN_FILENO
		&& rp -> fd != STDOUT_FILENO
		&& rp -> fd != STDERR_FILENO)
			closeonexec(rp -> fd);
#endif
	}

	return(NULL);
}

static heredoc_t *NEAR heredoc(eof, ignoretab)
char *eof;
int ignoretab;
{
	char *cp, path[MAXPATHLEN];
	int fd, flags;

	flags = (ignoretab) ? HD_IGNORETAB : 0;

	cp = strdup2(eof);
	if (stripquote(cp, EA_STRIPQ)) flags |= HD_QUOTED;
#ifndef	BASHSTYLE
	/* bash allows no variables as the EOF identifier */
	free2(cp);
	if (!(cp = evalvararg(eof, EA_STRIPQLATER, 0))) {
		errno = -1;
		return(NULL);
	}
#endif
#ifdef	FD
	demacroarg(&cp);
#endif

	if ((fd = newdup(mktmpfile(path))) < 0) {
		free2(cp);
		return(NULL);
	}

	return(newheredoc(cp, strdup2(path), fd, flags));
}

static int NEAR redirect(trp, from, to, type)
syntaxtree *trp;
int from;
char *to;
int type;
{
	redirectlist *rp;
	char *cp;

	if (to && !*to) {
		syntaxerrno = ER_UNEXPTOK;
		return(-1);
	}

	if (!(type & MD_HEREDOC)) cp = strdup2(to);
	else {
		if (!(cp = (char *)heredoc(to, type & MD_APPEND))) {
			doperror(NULL, deftmpdir);
			return(-1);
		}
		trp -> cont |= CN_HDOC;
	}
	if (from < 0) from = (type & MD_READ) ? STDIN_FILENO : STDOUT_FILENO;

	if (!(trp -> comm)) trp -> comm = newcomm();
	rp = newrlist(from, cp, type, (trp -> comm) -> redp);
	(trp -> comm) -> redp = rp;

	return(0);
}

#if	!MSDOS
/*ARGSUSED*/
VOID cmpmail(path, msg, mtimep)
CONST char *path, *msg;
time_t *mtimep;
{
	struct stat st;

	if (!path || Xstat(path, &st) < 0) return;
	if (st.st_size > 0 && *mtimep && st.st_mtime > *mtimep) {
# ifdef	MINIMUMSHELL
		Xfputs("you have mail", Xstderr);
# else
		kanjifputs((msg) ? msg : "you have mail", Xstderr);
# endif
		VOID_C fputnl(Xstderr);
	}
	*mtimep = st.st_mtime;
}

# ifdef	MINIMUMSHELL
static VOID NEAR checkmail(reset)
int reset;
{
	static time_t mtime = 0;

	if (reset) mtime = 0;
	else cmpmail(getconstvar(ENVMAIL), NULL, &mtime);
}
# endif	/* !MINIMUMSHELL */
#endif	/* !MSDOS */

int identcheck(ident, delim)
CONST char *ident;
int delim;
{
	int i;

	if (!ident || !*ident || !isidentchar(*ident)) return(0);
	for (i = 1; ident[i]; i++) if (!isidentchar2(ident[i])) break;

	return((ident[i] == delim) ? i : ((ident[i]) ? 0 : -i));
}

static int NEAR searchvar(var, ident, len, c)
char *CONST *var;
CONST char *ident;
int len, c;
{
	int i;

	if (!var) return(-1);
	for (i = 0; var[i]; i++)
		if (!strnenvcmp(ident, var[i], len) && var[i][len] == c)
			return(i);

	return(-1);
}

static char *NEAR searchvar2(max, var, ident, len)
int max;
CONST char **var, *ident;
int len;
{
	int i;

	if (!var) return(NULL);
	for (i = 0; i < max; i++) {
		if (!var[i]) continue;
		if (!strnenvcmp(ident, var[i], len) && !var[i][len])
			return((char *)var[i]);
#ifdef	FD
		if (!strnenvcmp(var[i], FDENV, FDESIZ)
		&& !strnenvcmp(ident, &(var[i][FDESIZ]), len)
		&& !var[i][FDESIZ + len])
			return((char *)&(var[i][FDESIZ]));
#endif
	}

	return(NULL);
}

static char **NEAR expandvar(var, ident, len)
char **var;
CONST char *ident;
int len;
{
	int i;

	if (searchvar(var, ident, len, '\0') >= 0) return(var);
	i = countvar(var);
	var = (char **)realloc2(var, (i + 2) * sizeof(char *));
	var[i] = strndup2(ident, len);
	var[++i] = NULL;

	return(var);
}

static char *NEAR getvar(var, ident, len)
char *CONST *var;
CONST char *ident;
int len;
{
	int i;

	if (len < 0) len = strlen(ident);
	if ((i = searchvar(var, ident, len, '=')) < 0) return(NULL);

	return(&(var[i][len + 1]));
}

char *getshellvar(ident, len)
CONST char *ident;
int len;
{
	return(getvar(shellvar, ident, len));
}

static char **NEAR putvar(var, s, len)
char **var, *s;
int len;
{
#if	MSDOS
	char *cp;
#endif
	u_long size;
	int i, export;

	export = (var == exportvar);
	i = searchvar(var, s, len, '=');

	if (i >= 0) {
		if (export) {
			size = (u_long)strlen(var[i]) + 1;
			if (size >= exportsize) exportsize = (u_long)0;
			else exportsize -= size;
		}
		free2(var[i]);
		if (!s[len]) {
			for (; var[i + 1]; i++) var[i] = var[i + 1];
			var[i] = NULL;
			return(var);
		}
	}
	else if (!s[len]) return(var);
	else {
		i = countvar(var);
		var = (char **)realloc2(var, (i + 2) * sizeof(char *));
		var[i + 1] = NULL;
	}

	var[i] = s;
#ifdef	ENVNOCASE
	for (i = 0; i < len; i++) s[i] = toupper2(s[i]);
#endif
	if (export) {
		exportsize += (u_long)strlen(s) + 1;
#if	MSDOS
		if (constequal(s, ENVPATH, len)) {
			cp = &(s[len + 1]);
			if (_dospath(cp)) cp += 2;
			for (; *cp; cp++) {
				if (*cp == PATHDELIM) {
					*cp = ';';
					if (_dospath(cp + 1)) cp += 2;
				}
# ifndef	BSPATHDELIM
				else if (*cp == _SC_) *cp = '\\';
# endif
			}
		}
#endif	/* MSDOS */
	}

	return(var);
}

static int NEAR checkprimal(s, len)
CONST char *s;
int len;
{
	char *cp;

	if ((cp = searchvar2(PRIMALVARSIZ, primalvar, s, len))) {
		execerror(NULL, cp, ER_CANNOTUNSET, 0);
		return(seterrno(EPERM));
	}

	return(0);
}

static int NEAR checkrestrict(s, len)
CONST char *s;
int len;
{
	char *cp;

	if (!restricted) return(0);

	if ((cp = searchvar2(RESTRICTVARSIZ, restrictvar, s, len))) {
		execerror(NULL, cp, ER_RESTRICTED, 0);
		return(seterrno(EPERM));
	}

	return(0);
}

static int NEAR checkronly(s, len)
CONST char *s;
int len;
{
	int i;

	if ((i = searchvar(ronlylist, s, len, '\0')) >= 0) {
		execerror(NULL, ronlylist[i], ER_ISREADONLY, 0);
		return(seterrno(EACCES));
	}

	return(0);
}

static int NEAR _putshellvar(s, len)
char *s;
int len;
{
#if	!MSDOS && defined (FD)
	keyseq_t *keymap;
	CONST char *term;
#endif
	CONST char *cp;

	cp = &(s[len + 1]);

	if (checkrestrict(s, len) < 0) return(-1);
#ifdef	BASHSTYLE
	/* bash distinguishes the same named function and variable */
	else if (checkronly(s, len) < 0) return(-1);
#else
	else if (unsetshfunc(s, len) < 0) return(-1);
#endif
#ifndef	_NOUSEHASH
	else if (constequal(s, ENVPATH, len)) searchhash(NULL, NULL, NULL);
#endif
#if	!MSDOS
# ifdef	MINIMUMSHELL
	else if (constequal(s, ENVMAIL, len)) checkmail(1);
# else	/* !MINIMUMSHELL */
	else if (!strnenvcmp(s, ENVMAIL, strsize(ENVMAIL))) {
		if (len == strsize(ENVMAIL)) {
			if (!getconstvar(ENVMAILPATH)) replacemailpath(cp, 0);
		}
		else if (constequal(s, ENVMAILPATH, len))
			replacemailpath(cp, 1);
		else if (constequal(s, ENVMAILCHECK, len)) {
			if ((mailcheck = isnumeric(cp)) < 0) mailcheck = 0;
		}
	}
# endif	/* !MINIMUMSHELL */
# ifdef	FD
	else if (constequal(s, ENVTERM, len)) {
		keymap = copykeyseq(NULL);
		regetterment(cp, interactive);
		if (dumbterm > 1 && (!shellmode || exit_status < 0)) {
			if (!(term = getconstvar(ENVTERM))) term = nullstr;
			regetterment(term, interactive);
			execerror(NULL, cp, ER_INVALTERMFD, 1);
			copykeyseq(keymap);
			freekeyseq(keymap);
			return(seterrno(EINVAL));
		}
		freekeyseq(keymap);
	}
# endif	/* FD */
#endif	/* !MSDOS */
#ifndef	NOPOSIXUTIL
	else if (constequal(s, ENVOPTIND, len)) {
		if ((posixoptind = isnumeric(cp)) <= 1) posixoptind = 0;
	}
#endif
#ifndef	MINIMUMSHELL
	else if (constequal(s, ENVLINENO, len)) shlineno = -1L;
#endif
#if	!defined (FD) && defined (DEP_URLPATH)
	else if (constequal(s, "URLTIMEOUT", len)) {
		if ((urltimeout = isnumeric(cp)) < 0) urltimeout = 0;
	}
	else if (constequal(s, "URLOPTIONS", len)) {
		if ((urloptions = isnumeric(cp)) < 0) urloptions = 0;
	}
#endif
#if	!defined (FD) && defined (DEP_FTPPATH)
	else if (constequal(s, "FTPADDRESS", len)) ftpaddress = (char *)cp;
	else if (constequal(s, "FTPPROXY", len)) ftpproxy = (char *)cp;
	else if (constequal(s, "FTPLOGFILE", len)) ftplogfile = (char *)cp;
#endif
#if	!defined (FD) && defined (DEP_HTTPPATH)
	else if (constequal(s, "HTTPPROXY", len)) httpproxy = (char *)cp;
	else if (constequal(s, "HTTPLOGFILE", len)) httplogfile = (char *)cp;
	else if (constequal(s, "HTMLLOGFILE", len)) htmllogfile = (char *)cp;
#endif

	shellvar = putvar(shellvar, s, len);
#ifdef	FD
	evalenv(s, len);
#endif

	return(0);
}

int putexportvar(s, len)
char *s;
int len;
{
	char *cp;

	if (len < 0) {
		if (!(cp = strchr2(s, '='))) return(0);
		len = cp - s;
	}

	if (_putshellvar(s, len) < 0) return(-1);
	exportlist = expandvar(exportlist, s, len);
	exportvar = putvar(exportvar, strdup2(s), len);
#ifdef	DEP_PTY
	sendparent(TE_PUTEXPORTVAR, s, len);
#endif

	return(0);
}

int putshellvar(s, len)
char *s;
int len;
{
	char *cp;

	if (autoexport) return(putexportvar(s, len));
	if (len < 0) {
		if (!(cp = strchr2(s, '='))) return(0);
		len = cp - s;
	}

	if (_putshellvar(s, len) < 0) return(-1);
	if (searchvar(exportlist, s, len, '\0') >= 0)
		exportvar = putvar(exportvar, strdup2(s), len);
#ifdef	DEP_PTY
	sendparent(TE_PUTSHELLVAR, s, len);
#endif

	return(0);
}

int unset(ident, len)
CONST char *ident;
int len;
{
#if	!MSDOS && !defined (MINIMUMSHELL)
	CONST char *cp;
#endif
	int i;

	if (checkprimal(ident, len) < 0 || checkrestrict(ident, len) < 0
	|| checkronly(ident, len) < 0)
		return(-1);
#if	!MSDOS
# ifndef	MINIMUMSHELL
	else if (!strnenvcmp(ident, ENVMAIL, strsize(ENVMAIL))) {
		if (len == strsize(ENVMAIL)) {
			if (!getconstvar(ENVMAILPATH)) checkmail(1);
		}
		else if (constequal(ident, ENVMAILPATH, len)) {
			cp = getconstvar(ENVMAIL);
			if (cp) replacemailpath(cp, 0);
			else checkmail(1);
		}
	}
# endif	/* !MINIMUMSHELL */
# ifdef	FD
	else if (constequal(ident, ENVTERM, len))
		regetterment(nullstr, interactive);
# endif
#endif	/* !MSDOS */
#ifndef	NOPOSIXUTIL
	else if (constequal(ident, ENVOPTIND, len)) posixoptind = 0;
#endif
#ifndef	MINIMUMSHELL
	else if (constequal(ident, ENVLINENO, len)) shlineno = -1L;
#endif
#if	!defined (FD) && defined (DEP_URLPATH)
	else if (constequal(ident, "URLTIMEOUT", len)) urltimeout = 0;
	else if (constequal(ident, "URLOPTIONS", len)) urloptions = 0;
#endif
#if	!defined (FD) && defined (DEP_FTPPATH)
	else if (constequal(ident, "FTPADDRESS", len)) ftpaddress = NULL;
	else if (constequal(ident, "FTPPROXY", len)) ftpproxy = NULL;
	else if (constequal(ident, "FTPLOGFILE", len)) ftplogfile = NULL;
#endif
#if	!defined (FD) && defined (DEP_HTTPPATH)
	else if (constequal(ident, "HTTPPROXY", len)) httpproxy = NULL;
	else if (constequal(ident, "HTTPLOGFILE", len)) httplogfile = NULL;
#endif

	shellvar = putvar(shellvar, (char *)ident, len);
	exportvar = putvar(exportvar, (char *)ident, len);

	if ((i = searchvar(exportlist, ident, len, '\0')) >= 0) {
		free2(exportlist[i]);
		for (; exportlist[i + 1]; i++)
			exportlist[i] = exportlist[i + 1];
		exportlist[i] = NULL;
	}
#ifdef	FD
	evalenv(ident, len);
#endif
#ifdef	DEP_PTY
	sendparent(TE_UNSET, ident, len);
#endif

	return(0);
}

#ifndef	MINIMUMSHELL
static VOID NEAR setshlineno(n)
long n;
{
	char tmp[strsize(ENVLINENO) + 1 + MAXLONGWIDTH + 1];

	if (shlineno < 0L) return;

	shlineno = n;
	snprintf2(tmp, sizeof(tmp), "%s=%ld", ENVLINENO, n);
	shellvar = putvar(shellvar, strdup2(tmp), strsize(ENVLINENO));
}
#endif	/* !MINIMUMSHELL */

static heredoc_t *NEAR duplheredoc(hdp)
heredoc_t *hdp;
{
	heredoc_t *new;

	if (!hdp) return(NULL);
	new = newheredoc(strdup2(hdp -> eof), strdup2(hdp -> filename),
		hdp -> fd, hdp -> flags);
	new -> buf = strdup2(hdp -> buf);

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
	new = newrlist(rp -> fd, filename, rp -> type | MD_DUPL, next);
	new -> new = rp -> new;
	new -> old = rp -> old;
#ifdef	DEP_DOSDRIVE
	new -> fakepipe = rp -> fakepipe;
	new -> dosfd = rp -> dosfd;
#endif

	return(new);
}

#ifdef	MINIMUMSHELL
syntaxtree *duplstree(trp, parent)
syntaxtree *trp, *parent;
#else
syntaxtree *duplstree(trp, parent, top)
syntaxtree *trp, *parent;
long top;
#endif
{
	syntaxtree *new;
	redirectlist *rp;
	command_t *comm;

	new = newstree(parent);
	new -> type = trp -> type;
	new -> cont = trp -> cont;
	new -> flags = (trp -> flags | ST_NOWN);
#ifndef	MINIMUMSHELL
	new -> lineno = trp -> lineno - top;
#endif
	if ((comm = trp -> comm)) {
		if (trp -> flags & ST_NODE)
#ifdef	MINIMUMSHELL
			new -> comm = (command_t *)duplstree(
				(syntaxtree *)comm, new);
#else
			new -> comm = (command_t *)duplstree(
				(syntaxtree *)comm, new, top);
#endif
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
#ifdef	MINIMUMSHELL
					(char **)duplstree((syntaxtree *)
						(comm -> argv), new);
#else
					(char **)duplstree((syntaxtree *)
						(comm -> argv), new, top);
#endif
			}
			(new -> comm) -> redp = duplrlist(comm -> redp);
		}
	}
	if (trp -> next) {
		if (!(trp -> cont & (CN_QUOT | CN_ESCAPE)))
#ifdef	MINIMUMSHELL
			new -> next = duplstree(trp -> next, new);
#else
			new -> next = duplstree(trp -> next, new, top);
#endif
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
	else for (n = 0; func[n].ident; n++) /*EMPTY*/;
	dupl = (shfunctable *)malloc2((n + 1) * sizeof(shfunctable));
	for (i = 0; i < n; i++) {
		dupl[i].ident = strdup2(func[i].ident);
#ifdef	MINIMUMSHELL
		dupl[i].func = duplstree(func[i].func, NULL);
#else
		dupl[i].func = duplstree(func[i].func, NULL, 0L);
#endif
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
			free2(func[i].ident);
			freestree(func[i].func);
			free2(func[i].func);
		}
		free2(func);
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

	return((ifs = getconstvar(ENVIFS)) ? ifs : IFS_SET);
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
		if (*(shflaglist[i].var) && shflaglist[i].letter)
			cp[j++] = shflaglist[i].letter;
	cp[j] = '\0';

	return(cp);
}

static int checkundefvar(cp, arg, len)
CONST char *cp, *arg;
int len;
{
	char *new;

	if (cp || !undeferror) return(0);
	new = strndup2(arg, len);
	execerror(NULL, new, ER_PARAMNOTSET, 0);
	free2(new);

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

	if (!trp || (trp -> flags & ST_NODE) || !isstatement(trp -> comm)
	|| (id = (trp -> comm) -> id) <= 0 || id > STATEMENTSIZ)
		return(-1);

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

		if (!(statementlist[no].type & STT_CASEEND)
		&& (type & STT_NEEDLIST) && !isopnot(statementbody(tmptr))
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
CONST char *arg;
{
	syntaxtree *tmptr;
	command_t *comm;
	int id;

	if (trp -> flags & ST_NODE) return(trp);

	comm = trp -> comm;
	if (ischild(comm)) /*EMPTY*/;
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

		if ((type & STT_NEEDIDENT) && tmptr == getparent(*trpp))
			/*EMPTY*/;
		else if (!((*trpp) -> comm) || ischild((*trpp) -> comm)) {
			for (i = 0; i < STATEMENTSIZ; i++)
			if (!strcommcmp(tok, statementlist[i].ident)) {
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
	else if (redirect(*trpp, rp -> fd, tok, type) < 0) return(-1);

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
CONST char *s;
int *ptrp;
{
	char tmptok[3];
	int i, id;

	id = getstatid(parentstree(trp));

	if (s[*ptrp + 1] != ';') {
		if (!(trp -> comm) && !(trp -> flags & ST_NEXT) && id >= 0) {
			syntaxerrno = ER_UNEXPNL;
			return(trp);
		}
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
CONST char *s;
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
CONST char *s;
int *ptrp;
{
#ifndef	MINIMUMSHELL
	int n;
#endif

	trp = _addarg(trp, NULL);
	switch (s[*ptrp + 1]) {
		case '|':
			(*ptrp)++;
			trp = linkstree(trp, OP_OR);
			break;
#ifndef	MINIMUMSHELL
		case '&':
			(*ptrp)++;
			n = redirect(trp,
				STDERR_FILENO, "1", MD_WRITE | MD_FILEDESC);
			if (n >= 0) trp = linkstree(trp, OP_PIPE);
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
CONST char *s;
int *ptrp;
{
	int n;

	rp -> type = MD_READ;
	n = 0;
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
			if (s[*ptrp + 1] == '-') n++;
			else if (s[*ptrp + 1] == '&') {
				(*ptrp)++;
				if (s[*ptrp + 1] == '-') n++;
				else rp -> type |= MD_FILEDESC;
			}
			break;
#endif	/* !MINIMUMSHELL */
		case '&':
			(*ptrp)++;
			if (s[*ptrp + 1] == '-') n++;
			else rp -> type |= MD_FILEDESC;
			break;
		case '-':
			n++;
			break;
		default:
			break;
	}
	if (n) {
		(*ptrp)++;
		if (redirect(trp, rp -> fd, NULL, rp -> type) >= 0)
			rp -> type = 0;
	}

	return(trp);
}

static syntaxtree *NEAR morethan(trp, rp, s, ptrp)
syntaxtree *trp;
redirectlist *rp;
CONST char *s;
int *ptrp;
{
	int n;

	rp -> type = MD_WRITE;
	n = 0;
	switch (s[*ptrp + 1]) {
		case '>':
			(*ptrp)++;
			rp -> type |= MD_APPEND;
			break;
#ifndef	MINIMUMSHELL
		case '<':
			if (rp -> fd < 0) rp -> fd = STDOUT_FILENO;
			(*ptrp)++;
			rp -> type |= MD_READ;
			if (s[*ptrp + 1] == '-') n++;
			else if (s[*ptrp + 1] == '&') {
				(*ptrp)++;
				if (s[*ptrp + 1] == '-') n++;
				else rp -> type |= MD_FILEDESC;
			}
			break;
#endif	/* !MINIMUMSHELL */
		case '&':
			(*ptrp)++;
			if (s[*ptrp + 1] == '-') n++;
			else rp -> type |= MD_FILEDESC;
			break;
		case '-':
			n++;
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
	if (n) {
		(*ptrp)++;
		if (redirect(trp, rp -> fd, NULL, rp -> type) >= 0)
			rp -> type = 0;
	}

	return(trp);
}

#if	defined (BASHSTYLE) || !defined (MINIMUMSHELL)
syntaxtree *startvar(trp, rp, s, ptrp, tptrp, n)
syntaxtree *trp;
redirectlist *rp;
CONST char *s;
int *ptrp, *tptrp, n;
{
	syntaxtree *new;
	redirectlist *newrp;
	char *cp;

	cp = malloc2(*tptrp + n + 1);
	strncpy2(cp, rp -> filename, *tptrp);
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
CONST char *s;
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
	memcpy(cp, rp -> filename, len);
	*tptrp += len;
	strncpy2(&(cp[*tptrp]), &(s[*ptrp]), n);
	*tptrp += n;
	*ptrp += n - 1;
	free2(rp -> filename);
	rp -> filename = cp;

	free2(rp -> next);
	rp -> next = NULL;
	free2(tmptr);

	return(trp);
}

static syntaxtree *NEAR addvar(trp, s, ptrp, tok, tptrp, n)
syntaxtree *trp;
CONST char *s;
int *ptrp;
CONST char *tok;
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
	strncpy2(&(rp -> filename[len]), tok, *tptrp);
	len += *tptrp;
	*tptrp = 0;
	if (n > 0) {
		strncpy2(&(rp -> filename[len]), &(s[*ptrp]), n);
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
CONST char *s;
int *ptrp, *tptrp;
ALLOC_T *sp;
{
	char tmptok[2];
	ALLOC_T size;
	int i;

	switch (s[*ptrp]) {
		case '{':
			if (!strchr2(IFS_SET, s[*ptrp + 1])) {
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
				strncpy2(rp -> filename, &(s[i]), *tptrp);
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
			if (!strchr2(IFS_SET, s[*ptrp]))
				rp -> filename[(*tptrp)++] = s[*ptrp];
			else addarg(&trp, rp, NULL, tptrp, 0);
			break;
	}

	return(trp);
}

static syntaxtree *NEAR casetoken(trp, rp, s, ptrp, tptrp)
syntaxtree *trp;
redirectlist *rp;
CONST char *s;
int *ptrp, *tptrp;
{
	char tmptok[2];
	int i, stype;

	switch (s[*ptrp]) {
		case ')':
			if (!*tptrp) /*EMPTY*/;
			else if (addarg(&trp, rp, NULL, tptrp, 0) < 0) break;
			trp = _addarg(trp, NULL);
			trp = linkstree(trp, OP_NONE);
			i = 0;
			tmptok[i++] = s[*ptrp];
			addarg(&trp, rp, tmptok, &i, 0);
			break;
		case '|':
			if (!*tptrp) /*EMPTY*/;
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
#ifdef	STRICTPOSIX
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
			if (!strchr2(IFS_SET, s[*ptrp]))
				rp -> filename[(*tptrp)++] = s[*ptrp];
			else if (*tptrp > 0) {
				addarg(&trp, rp, NULL, tptrp, 0);
				do {
					(*ptrp)++;
				} while (isblank2(s[*ptrp]));

				/* for "esac " */
				if ((stype = getparenttype(trp)) != STT_INCASE
				&& stype != STT_CASEEND)
					(*ptrp)--;
				else if (!s[*ptrp] || s[*ptrp] == '\n')
					syntaxerrno = ER_UNEXPNL;
				else if (s[*ptrp] == ')' || s[*ptrp] == '|')
					(*ptrp)--;
				else if (s[*ptrp] == PESCAPE
				&& (!s[*ptrp + 1] || s[*ptrp] == '\n'))
					(*ptrp)--;
				else syntaxerrno = ER_UNEXPTOK;
			}
			break;
	}

	return(trp);
}

#if	!defined (BASHBUG) && !defined (MINIMUMSHELL)
static int NEAR cmpstatement(s, id)
CONST char *s;
int id;
{
	int len;

	if (--id < 0) return(-1);
	len = strlen(statementlist[id].ident);
	if (strncommcmp(s, statementlist[id].ident, len)) return(-1);

	return(len);
}

static syntaxtree *NEAR comsubtoken(trp, rp, s, ptrp, tptrp, sp)
syntaxtree *trp;
redirectlist *rp;
CONST char *s;
int *ptrp, *tptrp;
ALLOC_T *sp;
{
	int i, len;

	if (!*tptrp) {
		if ((len = cmpstatement(&(s[*ptrp]), SM_CASE)) >= 0
		&& (!s[*ptrp + len] || strchr2(IFS_SET, s[*ptrp + len]))) {
			trp = startvar(trp, rp, s, ptrp, tptrp, len);
			trp -> cont = CN_CASE;
			return(trp);
		}
		if ((trp -> cont & CN_SBST) == CN_CASE
		&& (len = cmpstatement(&(s[*ptrp]), SM_ESAC)) >= 0
		&& (!s[*ptrp + len] || strchr2(IFS_SET, s[*ptrp + len])
		|| s[*ptrp + len] == ';' || s[*ptrp + len] == ')')) {
			trp = endvar(trp, rp, s, ptrp, tptrp, sp, len);
			return(trp);
		}
		if (strchr2(IFS_SET, s[*ptrp])) {
			for (len = 1; s[*ptrp + len]; len++)
				if (!strchr2(IFS_SET, s[*ptrp + len])) break;
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
			strncpy2(&(rp -> filename[*tptrp]), &(s[*ptrp]), len);
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
CONST char *s;
int quiet;
{
#if	!defined (BASHSTYLE) && !defined (MINIMUMSHELL)
	syntaxtree *tmptr;
#endif
	char *cp;
	ALLOC_T size;
	int i, j, n, pc, stype, hdoc;

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

#ifndef	MINIMUMSHELL
		if (s[i] == '\n') setshlineno(shlineno + 1L);
#endif
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

		pc = parsechar(&(s[i]), -1, '$', EA_BACKQ | EA_EOLESCAPE,
#ifdef	NESTINGQUOTE
			&(rp -> new), &(rp -> old));
#else
			&(rp -> new), NULL);
#endif

		if (pc == PC_OPQUOTE || pc == PC_CLQUOTE || pc == PC_SQUOTE)
			rp -> filename[j++] = s[i];
#ifdef	BASHSTYLE
		else if (pc == PC_BQUOTE) rp -> filename[j++] = s[i];
#endif
		else if (pc == PC_WCHAR) {
			rp -> filename[j++] = s[i++];
			rp -> filename[j++] = s[i];
		}
		else if (pc == PC_ESCAPE) {
			if (s[++i] != '\n' || rp -> new) {
				rp -> filename[j++] = PESCAPE;
				if (s[i]) rp -> filename[j++] = s[i];
				else {
					trp -> cont |= CN_ESCAPE;
					break;
				}
			}
		}
		else if (pc == '$') {
			if (s[i + 1] == '{') {
#ifdef	BASHSTYLE
	/* bash treats any meta character in ${} as just a character */
				trp = startvar(trp, rp, s, &i, &j, 2);
#else	/* !BASHSTYLE */
# ifndef	MINIMUMSHELL
				if (rp -> new == '"')
					trp = startvar(trp, rp, s, &i, &j, 2);
				else
# endif
				{
					rp -> filename[j++] = s[i++];
					rp -> filename[j++] = s[i];
				}
#endif	/* !BASHSTYLE */
				trp -> cont = CN_VAR;
			}
#ifndef	MINIMUMSHELL
			else if (s[i + 1] == '(') {
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
			else rp -> filename[j++] = s[i];
		}
		else if (s[i] == '}' && (trp -> cont & CN_SBST) == CN_VAR) {
#ifdef	BASHSTYLE
	/* bash treats any meta character in ${} as just a character */
			trp = endvar(trp, rp, s, &i, &j, &size, 1);
#else	/* !BASHSTYLE */
# ifndef	MINIMUMSHELL
			tmptr = trp -> parent;
			if (tmptr) tmptr = tmptr -> next;
			if (tmptr && ((redirectlist *)tmptr) -> new == '"')
				trp = endvar(trp, rp, s, &i, &j, &size, 1);
			else
# endif
			{
				rp -> filename[j++] = s[i];
				trp -> cont &= ~CN_VAR;
			}
#endif	/* !BASHSTYLE */
		}
#ifndef	MINIMUMSHELL
		else if ((trp -> cont & CN_SBST) == CN_VAR)
			rp -> filename[j++] = s[i];
#endif
#ifndef	BASHSTYLE
		else if (pc == PC_BQUOTE) rp -> filename[j++] = s[i];
#endif
		else if (pc == PC_DQUOTE) rp -> filename[j++] = s[i];
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
			if (strchr2(IFS_SET, s[i])) {
				if (j < 0) j = 0;
				rp -> filename[j] = '\0';
				syntaxerror(rp -> filename);
				return(NULL);
			}
			for (j = i + 1; s[j]; j++) {
				if (strchr2(IFS_SET, s[j])) break;
				else if (iswchar(s, j)) j++;
			}
			cp = strndup2(&(s[i]), j - i);
			syntaxerror(cp);
			free2(cp);
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
#endif

	return(trp);
}

syntaxtree *analyze(s, trp, quiet)
CONST char *s;
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
	red.new = red.old = '\0';
#ifdef	DEP_DOSDRIVE
	red.fakepipe = NULL;
	red.dosfd = -1;
#endif
	red.next = NULL;

	if (trp -> cont & (CN_QUOT | CN_ESCAPE)) {
		memcpy((char *)&red, (char *)(trp -> next), sizeof(red));
		free2(trp -> next);

		i = strlen(red.filename);
		if (i > 0) {
			if ((trp -> cont & CN_ESCAPE) && red.new != '\'') i--;
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
			free2(red.filename);
			return(NULL);
		}
	}

	i = (red.filename) ? strlen(red.filename) : 0;
	if (red.new) trp -> cont |= CN_QUOT;

	if (trp -> cont & (CN_QUOT | CN_ESCAPE)) {
		rp = (redirectlist *)malloc2(sizeof(redirectlist));
		memcpy((char *)rp, (char *)&red, sizeof(red));
		trp -> next = (syntaxtree *)rp;
		red.filename = NULL;
	}
#if	defined (BASHSTYLE) || !defined (MINIMUMSHELL)
	else if (trp -> cont & CN_SBST) {
		red.filename[i++] = '\n';
		red.filename[i] = '\0';
		trp = addvar(trp, NULL, NULL, red.filename, &i, 0);
	}
#endif
	else if (getparenttype(trp) == STT_INCASE && (trp -> comm || i))
		syntaxerrno = ER_UNEXPNL;
	else if (addarg(&trp, &red, NULL, &i, 1) < 0) /*EMPTY*/;
	else if (!(trp = _addarg(trp, NULL)) || syntaxerrno) /*EMPTY*/;
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
		free2(red.filename);
		return(NULL);
	}
	free2(red.filename);
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
		if (trp -> cont & (CN_ESCAPE | CN_QUOT | CN_HDOC))
			trp = analyze(NULL, trp, 0);
	/* bash does not allow the format like as "foo |" */
		else if ((trp -> flags & ST_NEXT) && hasparent(trp)
		&& isoppipe(trp -> parent))
			break;
#endif
		else {
			syntaxerrno = ER_UNEXPEOF;
			syntaxerror(nullstr);
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
	if ((id = evalargv(comm, &type, NULL)) >= 0 && type == CT_COMMAND)
		id = searchhash(&(comm -> hash), comm -> argv[0], NULL);
	freevar(subst);
	free2(len);
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
#ifndef	BASHSTYLE
	/* bash allows any character in the function identifier */
		if (identcheck((trp -> comm) -> argv[0], '\0') <= 0) {
			execerror(NULL,
				(trp -> comm) -> argv[0], ER_NOTIDENT, 0);
			return(-1);
		}
#endif
		if (!statementcheck(tmptr -> next, 0)) {
			doperror(NULL, NULL);
			return(-1);
		}
		trp = trp -> next;
	}
	else if (check_command(trp) < 0) return(-1);

	return((trp -> next) ? check_stree(trp -> next) : 0);
}

static syntaxtree *NEAR analyzeline(command)
CONST char *command;
{
#ifndef	MINIMUMSHELL
	long dupshlineno;
#endif
	syntaxtree *trp, *stree;

#ifndef	MINIMUMSHELL
	dupshlineno = shlineno;
	setshlineno(1L);
#endif
	stree = newstree(NULL);
	trp = analyze(command, stree, 0);
	if (!(trp = analyzeeof(trp))) /*EMPTY*/;
#ifndef	_NOUSEHASH
	else if (hashahead && check_stree(stree) < 0) trp = NULL;
#endif
	if (!trp) {
		freestree(stree);
		free2(stree);
		stree = NULL;
	}
#ifndef	MINIMUMSHELL
	setshlineno(dupshlineno);
#endif

	return(stree);
}

#ifdef	DEBUG
static VOID NEAR Xexecve(path, argv, envp, bg)
CONST char *path;
char *argv[], *envp[];
int bg;
#else
static VOID NEAR Xexecve(path, argv, envp)
CONST char *path;
char *argv[], *envp[];
#endif
{
#ifdef	DEP_PTY
	char *cp;
	int len;
#endif
	int fd, ret;

#ifdef	DEP_PTY
	if (parentfd >= 0 && ptyterm && *ptyterm) {
		len = strsize(ENVTERM);
		cp = malloc2(len + strlen(ptyterm) + 2);
		memcpy(cp, ENVTERM, len);
		cp[len] = '=';
		strcpy2(&(cp[len + 1]), ptyterm);
		envp = putvar(envp, cp, len);
	}
#endif	/* DEP_PTY */

	execve(path, argv, envp);
	if (errno != ENOEXEC) {
		if (errno == EACCES) {
			execerror(NULL, argv[0], ER_CANNOTEXE, 1);
			ret = RET_NOTEXEC;
		}
		else {
			doperror(NULL, argv[0]);
			ret = RET_FAIL;
		}
	}
	else if ((fd = newdup(Kopen(path, O_TEXT | O_RDONLY, 0666))) < 0) {
		doperror(NULL, argv[0]);
		ret = RET_NOTEXEC;
	}
	else {
		argvar = argv;
		sourcefile(fd, argv[0], 0);
		safeclose(fd);
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
	if (ext & CM_BATCH) strcpy2(&(path[len]), EXTBAT);
	else if (ext & CM_EXE) strcpy2(&(path[len]), EXTEXE);
	else strcpy2(&(path[len]), EXTCOM);

	return(path);
}

static char **NEAR replacebat(pathp, argv)
char **pathp, **argv;
{
	char *com;
	int i;

# ifdef	FD
	if (!(com = getenv2("FD_COMSPEC")) && !(com = getenv2("FD_SHELL")))
# else
	if (!(com = getconstvar(ENVCOMSPEC)) && !(com = getconstvar(ENVSHELL)))
# endif
# ifdef	BSPATHDELIM
		com = "\\COMMAND.COM";
# else
		com = "/COMMAND.COM";
# endif

	i = countvar(argv);
	memmove((char *)(&(argv[i + 2])), (char *)(&(argv[i])),
		(i + 1) * sizeof(char *));
	free2(argv[2]);
	argv[2] = strdup2(*pathp);
	argv[1] = strdup2("/C");
	argv[0] = *pathp = strdup2(com);

	return(argv);
}
#endif	/* MSDOS */

#ifdef	USEFAKEPIPE
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
#ifndef	USEFAKEPIPE
	p_id_t pid;
	int fds[2];
#endif
	pipelist *pl;
	char pfile[MAXPATHLEN];
	int fd, dupl;

	pl = (pipelist *)malloc2(sizeof(pipelist));
	pl -> file = NULL;
	pl -> fp = NULL;
	pl -> fd = fdin;
	pl -> old = -1;

#ifndef	USEFAKEPIPE
	if (pipe(fds) < 0) {
# ifdef	FAKEUNINIT
		fd = -1;	/* fake for -Wuninitialized */
# endif
		pid = (p_id_t)-1;
	}
	else if ((pid = makechild(tty, ppid, -1)) < (p_id_t)0) {
# ifdef	FAKEUNINIT
		fd = -1;	/* fake for -Wuninitialized */
# endif
		safeclose(fds[0]);
		safeclose(fds[1]);
	}
	else if (!pid) {
		if ((fd = newdup(Xdup(STDOUT_FILENO))) < 0
		|| fds[1] == STDOUT_FILENO
		|| Xdup2(fds[1], STDOUT_FILENO) < 0) {
			prepareexit(1);
			Xexit(RET_NOTEXEC);
		}
		safeclose(fds[0]);
		safeclose(fds[1]);
		pl -> old = fd;
	}
	else {
		if (new) fd = newdup(fds[0]);
		else {
			if ((fd = newdup(Xdup(fdin))) < 0
			|| fds[0] == fdin || Xdup2(fds[0], fdin) < 0) {
				safeclose(fd);
# ifndef	NOJOB
				if (stoppedjob(pid)) /*EMPTY*/;
				else
# endif
				{
					VOID_C kill(pid, SIGPIPE);
					while (!waitjob(pid, NULL, WUNTRACED))
						if (interrupted) break;
				}
				pid = (p_id_t)-1;
			}
			else {
# ifndef	NOJOB
				stackjob(pid, 0, NULL);
# endif
				pl -> old = fd;
			}
			safeclose(fds[0]);
		}
		safeclose(fds[1]);
	}
	if (pid >= (p_id_t)0) {
		pl -> new = fd;
		pl -> pid = *pidp = pid;
		pl -> next = pipetop;
		pipetop = pl;
		return(fd);
	}
#endif	/* !USEFAKEPIPE */

	if ((dupl = newdup(Xdup(STDOUT_FILENO))) < 0) {
		free2(pl);
		return(-1);
	}
	if ((fd = newdup(mktmpfile(pfile))) < 0
	|| fd == STDOUT_FILENO || Xdup2(fd, STDOUT_FILENO) < 0) {
		free2(pl);
		safeclose(fd);
		safeclose(dupl);
		return(-1);
	}
	safeclose(fd);

	if (new) pl -> fd = -1;
	pl -> file = strdup2(pfile);
	pl -> new = pl -> old = dupl;
	pl -> pid = *pidp = (p_id_t)0;
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

	if (pl -> pid > (p_id_t)0) return(fd);
	if (pl -> old >= 0) {
		Xdup2(pl -> old, STDOUT_FILENO);
		safeclose(pl -> old);
		pl -> old = -1;
	}
	if (!(pl -> file)) {
		prepareexit(1);
		Xexit(ret);
	}

	pl -> ret = ret;
	if ((fd = newdup(Xopen(pl -> file, O_BINARY | O_RDONLY, 0666))) < 0) {
		safermtmpfile(pl -> file);
		free2(pl -> file);
		*prevp = pl -> next;
		free2(pl);
		return(-1);
	}
	if (pl -> fd >= 0) {
		if ((dupl = newdup(Xdup(pl -> fd))) < 0
		|| fd == pl -> fd || Xdup2(fd, pl -> fd) < 0) {
			safeclose(fd);
			safermtmpfile(pl -> file);
			free2(pl -> file);
			*prevp = pl -> next;
			free2(pl);
			return(-1);
		}
		safeclose(fd);
		fd = pl -> old = dupl;
	}
	pl -> new = fd;

	return(fd);
}

static XFILE *NEAR fdopenpipe(fd)
int fd;
{
	pipelist *pl, **prevp;
	XFILE *fp;

	if (!(prevp = searchpipe(fd))) return(NULL);
	pl = *prevp;
	if (!(fp = Xfdopen(fd, "rb"))) {
		safeclose(fd);
		safermtmpfile(pl -> file);
		free2(pl -> file);
		*prevp = pl -> next;
		free2(pl);
		return(NULL);
	}
	pl -> fp = fp;

	return(fp);
}

#ifdef	DJGPP
static int NEAR closepipe(fd, dupl)
int fd, dupl;
#else
static int NEAR closepipe(fd)
int fd;
#endif
{
	pipelist *pl, **prevp;
	int ret, duperrno;

	if (!(prevp = searchpipe(fd))) return(-1);
	duperrno = errno;
	pl = *prevp;
#ifndef	NOJOB
	if (pl -> pid > (p_id_t)0 && stoppedjob(pl -> pid) > 0) {
		errno = duperrno;
		return(RET_SUCCESS);
	}
#endif

	if (pl -> old >= 0) {
		Xdup2(pl-> old, pl -> fd);
		safeclose(pl -> old);
	}
	if (fd != STDIN_FILENO &&
	fd != STDOUT_FILENO && fd != STDERR_FILENO) {
		if (pl -> fp) safefclose(pl -> fp);
		else safeclose(pl -> new);
	}

	ret = RET_SUCCESS;
	if (pl -> file) {
		if (rmtmpfile(pl -> file) < 0) {
#ifdef	DJGPP
			if (errno == EACCES && dupl >= 0 && Xclose(dupl) >= 0
			&& rmtmpfile(pl -> file) >= 0)
				/*EMPTY*/;
			else
#endif
			doperror(NULL, pl -> file);
		}
		free2(pl -> file);
		ret = pl -> ret;
	}
#if	!MSDOS
	else if (!(pl -> pid)) {
		prepareexit(1);
		Xexit(RET_SUCCESS);
	}
	else if (pl -> pid > (p_id_t)0) {
# ifdef	NOJOB
		wait_pid_t w;

		VOID_C kill(pl -> pid, SIGPIPE);
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

	*prevp = pl -> next;
	free2(pl);
	errno = duperrno;

	return(ret);
}

#ifndef	_NOUSEHASH
static VOID NEAR disphash(VOID_A)
{
	hashlist *hp;
	char buf[7 + 1];
	int i, j;

	Xfputs("hits    cost    command", Xstdout);
	VOID_C fputnl(Xstdout);
	if (hashtable) for (i = 0; i < MAXHASH; i++)
		for (hp = hashtable[i]; hp; hp = hp -> next) {
			snprintf2(buf, sizeof(buf), "%d", hp -> hits);
			j = strlen(buf);
			buf[j++] = (hp -> type & CM_RECALC) ? '*' : ' ';
			while (j < 7) buf[j++] = ' ';
			buf[j] = '\0';
			fprintf2(Xstdout, "%s %-7d %k",
				buf, hp -> cost, hp -> path);
			VOID_C fputnl(Xstdout);
		}
}
#endif	/* !_NOUSEHASH */

char *evalbackquote(arg)
CONST char *arg;
{
	XFILE *fp;
	char *buf;
	ALLOC_T len;
	int duptrapok;

	duptrapok = trapok;
	trapok = -1;
	fp = _dopopen(arg);
	trapok = duptrapok;
	if (!fp) {
		if (errno) doperror(NULL, NULL);
		buf = NULL;
		ret_status = RET_NOTEXEC;
	}
	else if (!(buf = readfile(Xfileno(fp), &len))) {
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
		ret_status = closepipe2(Xfileno(fp), -1);
	}

	return(buf);
}

#ifdef	NOALIAS
int checktype(s, idp, func)
CONST char *s;
int *idp, func;
#else
int checktype(s, idp, alias, func)
CONST char *s;
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
			if (!strcommcmp(s, shellalias[i].ident)) {
				if (idp) *idp = i;
				return(CT_ALIAS);
			}
		}
#endif
#ifdef	STRICTPOSIX
		for (i = 0; i < SHBUILTINSIZ; i++) {
# ifndef	MINIMUMSHELL
			if (shbuiltinlist[i].flags & BT_DISABLE) continue;
# endif
			if (!(shbuiltinlist[i].flags & BT_POSIXSPECIAL))
				continue;
			if (!strcommcmp(s, shbuiltinlist[i].ident)) {
				if (idp) *idp = i;
				return(CT_BUILTIN);
			}
		}
#endif
		if (func && shellfunc) {
			for (i = 0; shellfunc[i].ident; i++)
			if (!strcommcmp(s, shellfunc[i].ident)) {
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
#ifndef	MINIMUMSHELL
			if (shbuiltinlist[i].flags & BT_DISABLE) continue;
#endif
#ifdef	STRICTPOSIX
			if (shbuiltinlist[i].flags & BT_POSIXSPECIAL)
				continue;
#endif
			if (!strcommcmp(s, shbuiltinlist[i].ident)) {
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
int completeshellvar(s, len, argc, argvp)
CONST char *s;
int len, argc;
char ***argvp;
{
	char *cp;
	int i;

	if (shellvar) for (i = 0; shellvar[i]; i++) {
		if (strnenvcmp(s, shellvar[i], len)
		|| !(cp = strchr2(shellvar[i], '=')))
			continue;
		cp = strndup2(shellvar[i], cp - shellvar[i]);
		if (finddupl(cp, argc, *argvp)) free2(cp);
		else {
			*argvp = (char **)realloc2(*argvp,
				(argc + 1) * sizeof(char *));
			(*argvp)[argc++] = cp;
		}
	}

	return(argc);
}

int completeshellcomm(s, len, argc, argvp)
CONST char *s;
int len, argc;
char ***argvp;
{
	int i;

# ifndef	NOALIAS
	for (i = 0; shellalias[i].ident; i++) {
		if (strncommcmp(s, shellalias[i].ident, len)
		|| finddupl(shellalias[i].ident, argc, *argvp))
			continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(shellalias[i].ident);
	}
# endif	/* !NOALIAS */
	for (i = 0; i < STATEMENTSIZ; i++) {
		if (strncommcmp(s, statementlist[i].ident, len)
		|| finddupl(statementlist[i].ident, argc, *argvp))
			continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(statementlist[i].ident);
	}
	for (i = 0; i < SHBUILTINSIZ; i++) {
# ifndef	MINIMUMSHELL
		if (shbuiltinlist[i].flags & BT_DISABLE) continue;
# endif
		if (strncommcmp(s, shbuiltinlist[i].ident, len)
		|| finddupl(shbuiltinlist[i].ident, argc, *argvp))
			continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(shbuiltinlist[i].ident);
	}
# ifdef	FD
	argc = completebuiltin(s, len, argc, argvp);
	if (!shellmode) argc = completeinternal(s, len, argc, argvp);
# endif
	if (shellfunc) for (i = 0; shellfunc[i].ident; i++) {
		if (strncommcmp(s, shellfunc[i].ident, len)
		|| finddupl(shellfunc[i].ident, argc, *argvp))
			continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(shellfunc[i].ident);
	}

	return(argc);
}
#endif	/* !FDSH && !_NOCOMPLETE */

#ifdef	BASHSTYLE
static char *NEAR quotemeta(s)
char *s;
{
	char *cp, *buf;
	ALLOC_T ptr, len;

	cp = strchr(s, '=');
	if (!cp || !strpbrk(++cp, METACHAR)) return(s);
	len = strlen(s) + 2;
	buf = malloc2(len + 1);
	ptr = cp - s;
	memcpy(buf, s, ptr);
	buf[ptr++] = '\'';
	while (*cp) {
		if (*cp != '\'') buf[ptr++] = *(cp++);
		else {
			len += 3;
			buf = realloc2(buf, len + 1);
			buf[ptr++] = '\'';
			buf[ptr++] = PESCAPE;
			buf[ptr++] = *(cp++);
			buf[ptr++] = '\'';
		}
	}
	buf[ptr++] = '\'';
	buf[ptr] = '\0';
	free2(s);

	return(buf);
}
#endif	/* BASHSTYLE */

int getsubst(argc, argv, substp, lenp)
int argc;
char **argv, ***substp;
int **lenp;
{
	int i, n, len;

	*substp = (char **)malloc2((argc + 1) * sizeof(char *));
	*lenp = (int *)malloc2(argc * sizeof(int));

	len = 1;
	for (i = n = 0; i < argc; i++) {
		len = (freeenviron || len > 0) ? identcheck(argv[i], '=') : 0;
		if (len > 0) {
			(*substp)[n] = argv[i];
			(*lenp)[n] = len;
			memmove((char *)(&(argv[i])),
				(char *)(&(argv[i + 1])),
				(argc-- - i) * sizeof(char *));
			n++;
			i--;
		}
	}
	(*substp)[n] = NULL;

	return(argc);
}

static int NEAR substvar(argv, flags)
char **argv;
int flags;
{
	char *tmp, *arg;
	int i;

	for (i = 0; argv[i]; i++) {
		if (trapok >= 0) trapok = 1;
		tmp = evalarg(argv[i], flags);
		if (trapok >= 0) trapok = 0;
		if (!tmp) {
			arg = argv[i];
			if (i && *arg && (flags & EA_EVALIFS)) {
				while (argv[i]) i++;
				argv = duplvar(argv, 2);
				argv = checkshellbuiltinargv(i, argv);
				freevar(argv);
			}
			execerror(NULL, arg, ER_BADSUBST, 0);
			return(-1);
		}
		free2(argv[i]);
		argv[i] = tmp;
	}

	return(i);
}

static int NEAR evalargv(comm, typep, contp)
command_t *comm;
int *typep, *contp;
{
	char *tmp;
	int i, id, glob;

	if (!comm) return(-1);
	else if (comm -> type) {
		*typep = comm -> type;
		return(comm -> id);
	}

	if (contp && substvar(comm -> argv, EA_BACKQ | EA_EVALIFS) < 0)
		return(-1);

	if (trapok >= 0) trapok = 1;
#ifdef	BASHSTYLE
	/* bash does not use IFS as a command separator */
	comm -> argc = evalifs(comm -> argc, &(comm -> argv), IFS_SET);
#else
	comm -> argc = evalifs(comm -> argc, &(comm -> argv), getifs());
#endif
	if (!(comm -> argc)) {
		*typep = CT_NONE;
		id = 0;
	}
	else {
		stripquote(tmp = strdup2(comm -> argv[0]), EA_STRIPQ);
#ifdef	NOALIAS
		*typep = checktype(tmp, &id, 1);
#else
		*typep = checktype(tmp, &id, 0, 1);
#endif
		free2(tmp);
	}

#ifdef	FD
	if (contp) {
		i = replaceargs(&(comm -> argc), &(comm -> argv),
			exportvar, (*typep == CT_COMMAND) ? 1 : 0);
		if (i < 0) return(-1);
		*contp = i;
	}
#endif

	if (noglob) glob = 0;
#ifdef	FD
	else if (*typep == CT_FDINTERNAL) glob = 0;
#endif
	else if (*typep != CT_BUILTIN) glob = 1;
	else if (shbuiltinlist[id].flags & BT_NOGLOB) glob = 0;
	else glob = 1;

	if (glob) {
#ifdef	DEP_FILECONV
		if (*typep == CT_FDORIGINAL) i = 0;
		else if (*typep != CT_BUILTIN) i = 1;
		else if (shbuiltinlist[id].flags & BT_NOKANJIFGET) i = 1;
		else i = 0;
		if (i) nokanjifconv++;
#endif
#if	MSDOS
		comm -> argc = evalglob(comm -> argc, &(comm -> argv),
			(*typep == CT_COMMAND) ? 0 : EA_STRIPQ);
#else
		comm -> argc = evalglob(comm -> argc, &(comm -> argv),
			EA_STRIPQ);
#endif
#ifdef	DEP_FILECONV
		if (i) nokanjifconv--;
#endif
	}
	else {
		i = 0;
#if	MSDOS
		stripquote(comm -> argv[i++], EA_STRIPQ);
		if (*typep == CT_COMMAND) while (i < comm -> argc)
			stripquote(comm -> argv[i++], 0);
		else
#endif
		while (i < comm -> argc)
			stripquote(comm -> argv[i++], EA_STRIPQ);
	}
	if (trapok >= 0) trapok = 0;

	return(id);
}

static char *NEAR evalexternal(comm)
command_t *comm;
{
	char *path;
	int type;

	type = searchhash(&(comm -> hash), comm -> argv[0], NULL);
	if (type & CM_NOTFOUND) {
		execerror(NULL, comm -> argv[0], ER_COMNOFOUND, 1);
		ret_status = RET_NOTFOUND;
		return(NULL);
	}
	if (restricted && (type & CM_FULLPATH)) {
		execerror(NULL, comm -> argv[0], ER_RESTRICTED, 2);
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
XFILE *fp;
{
	while (n-- > 0) Xfputs("  ", fp);
}

static VOID NEAR printnewline(n, fp)
int n;
XFILE *fp;
{
	if (n < 0) Xfputc(' ', fp);
	else {
		VOID_C fputnl(fp);
		printindent(n, fp);
	}
}

static int NEAR printredirect(rp, fp)
redirectlist *rp;
XFILE *fp;
{
	heredoc_t *hdp;
	int ret;

	if (!rp) return(0);
	ret = printredirect(rp -> next, fp);

	Xfputc(' ', fp);
	switch (rp -> type & MD_RDWR) {
		case MD_READ:
			if (rp -> fd != STDIN_FILENO)
				fprintf2(fp, "%d", rp -> fd);
			Xfputc('<', fp);
			if (rp -> type & MD_HEREDOC) {
				Xfputc('<', fp);
				if (rp -> type & MD_APPEND) Xfputc('-', fp);
			}
			break;
		case MD_WRITE:
			if (rp -> fd != STDOUT_FILENO)
				fprintf2(fp, "%d", rp -> fd);
			if (rp -> type & MD_WITHERR) Xfputc('&', fp);
			Xfputc('>', fp);
			if (rp -> type & MD_APPEND) Xfputc('>', fp);
#ifndef	MINIMUMSHELL
			else if (rp -> type & MD_FORCED) Xfputc('|', fp);
#endif
			break;
		case MD_RDWR:
			if (rp -> fd != STDOUT_FILENO)
				fprintf2(fp, "%d", rp -> fd);
			Xfputs("<>", fp);
			break;
		default:
			break;
	}
	if (!(rp -> filename)) Xfputc('-', fp);
	else {
		if (rp -> type & MD_FILEDESC) Xfputc('&', fp);
		else Xfputc(' ', fp);
		if (!(rp -> type & MD_HEREDOC)) argfputs(rp -> filename, fp);
		else {
			hdp = (heredoc_t *)(rp -> filename);
#ifdef	MINIMUMSHELL
			argfputs(hdp -> filename, fp);
#else
			argfputs(hdp -> eof, fp);
#endif
			ret = 1;
		}
	}

	return(ret);
}

VOID printstree(trp, indent, fp)
syntaxtree *trp;
int indent;
XFILE *fp;
#ifndef	MINIMUMSHELL
{
	redirectlist **rlist;
	int i;

	rlist = _printstree(trp, NULL, indent, fp);
	if (rlist) {
		for (i = 0; rlist[i]; i++) printheredoc(rlist[i], fp);
		free2(rlist);
	}
}

static VOID NEAR printheredoc(rp, fp)
redirectlist *rp;
XFILE *fp;
{
	heredoc_t *hdp;
	char *buf;
	int fd;

	if (!rp) return;
	printheredoc(rp -> next, fp);
	if (!(rp -> type & MD_HEREDOC)) return;

	hdp = (heredoc_t *)(rp -> filename);
	fd = newdup(Xopen(hdp -> filename, O_BINARY | O_RDONLY, 0666));
	if (fd >= 0) {
		VOID_C fputnl(Xstdout);
		while ((buf = readline(fd, '\0')) != vnullstr) {
			if (!buf) break;
			Xfputs(buf, Xstdout);
			VOID_C fputnl(Xstdout);
			free2(buf);
		}
		safeclose(fd);
		argfputs(hdp -> eof, fp);
	}
}

static redirectlist **NEAR _printstree(trp, rlist, indent, fp)
syntaxtree *trp;
redirectlist **rlist;
int indent;
XFILE *fp;
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
		Xfputc('!', fp);
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
				Xfputs(statementlist[i].ident, fp);
			}
		}
	}
	else {
		if (isstatement(trp -> comm)) {
			id = (trp -> comm) -> id;
			i = id - 1;
			if (id == SM_CHILD) {
				Xfputs("( ", fp);
#ifdef	MINIMUMSHELL
				printstree(statementbody(trp), indent, fp);
#else
				rlist2 = _printstree(statementbody(trp),
					NULL, indent, fp);
#endif
				Xfputs(" )", fp);
			}
			else if (id == SM_STATEMENT)
#ifdef	MINIMUMSHELL
				printstree(statementbody(trp), indent, fp);
#else
				rlist2 = _printstree(statementbody(trp),
					NULL, indent, fp);
#endif
			else {
				Xfputs(statementlist[i].ident, fp);
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
						Xfputc(' ', fp);
						break;
				}

				tmptr = statementbody(trp);
				if (ind2 < 0) ind2 = indent;
				else {
					if (indent < 0) ind2 = indent;
					else ind2 += indent;
					if (tmptr && tmptr -> comm)
						printnewline(ind2, fp);
				}
#ifdef	MINIMUMSHELL
				printstree(tmptr, ind2, fp);
#else
				rlist2 = _printstree(tmptr, NULL, ind2, fp);
#endif

				switch (id) {
					case SM_IF:
					case SM_ELIF:
					case SM_WHILE:
					case SM_UNTIL:
					case SM_IN:
#ifdef	BASHSTYLE
	/* bash type pretty print */
						Xfputc(' ', fp);
						break;
#endif
					case SM_THEN:
					case SM_RPAREN:
						if (!(trp -> next)) break;
						printnewline(indent, fp);
						break;
					case SM_FOR:
					case SM_CASE:
						Xfputc(' ', fp);
						break;
					default:
						break;
				}
			}
		}
		else if ((trp -> comm) -> argc > 0) {
			argfputs((trp -> comm) -> argv[0], fp);
			for (i = 1; i < (trp -> comm) -> argc; i++) {
				Xfputc(' ', fp);
				if (prev == SM_INCASE || prev == SM_CASEEND)
					Xfputs("| ", fp);
				argfputs((trp -> comm) -> argv[i], fp);
			}
		}

#ifdef	MINIMUMSHELL
		printredirect((trp -> comm) -> redp, fp);
#else
		if (printredirect((trp -> comm) -> redp, fp)) {
			if (!rlist) i = 0;
			else for (i = 0; rlist[i]; i++) /*EMPTY*/;
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
		else for (i = 0; rlist[i]; i++) /*EMPTY*/;
		for (j = 0; rlist2[j]; j++) /*EMPTY*/;
		rlist = (redirectlist **)realloc2(rlist,
			(i + j + 1) * sizeof(redirectlist *));
		for (j = 0; rlist2[j]; j++) rlist[i + j] = rlist2[j];
		rlist[i + j] = NULL;
		free2(rlist2);
	}
#endif

	for (id = 0; id < OPELISTSIZ; id++)
		if (trp -> type == opelist[id].op) break;
	if (id < OPELISTSIZ && !isopfg(trp))
		fprintf2(fp, " %s", opelist[id].symbol);
#ifndef	MINIMUMSHELL
	if (!rlist || opelist[id].level < 4) nl = 0;
	else {
		for (i = 0; rlist[i]; i++) printheredoc(rlist[i], fp);
		free2(rlist);
		rlist = NULL;
		nl = 1;
	}
#endif

	if (trp -> next && (trp -> next) -> comm) {
#ifndef	MINIMUMSHELL
		if (nl) {
			VOID_C fputnl(fp);
			printindent(indent, fp);
		}
		else
#endif
		if (isopfg(trp)) {
#ifdef	BASHSTYLE
	/* bash type pretty print */
			Xfputc(';', fp);
			printnewline(indent, fp);
#else
			if (indent < 0) Xfputc(';', fp);
			else {
				VOID_C fputnl(fp);
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
			Xfputc(' ', fp);
		}
	}
#ifndef	MINIMUMSHELL
	else if (nl) /*EMPTY*/;
#endif
	else if (isopfg(trp)) {
#ifdef	BASHSTYLE
	/* bash type pretty print */
		Xfputc(';', fp);
#else
		if (indent < 0) Xfputc(';', fp);
#endif
	}

#ifdef	MINIMUMSHELL
	if (trp -> next) printstree(trp -> next, indent, fp);
	Xfflush(fp);
#else
	if (trp -> next) rlist = _printstree(trp -> next, rlist, indent, fp);
	Xfflush(fp);

	return(rlist);
#endif
}

static VOID NEAR printshfunc(f, fp)
shfunctable *f;
XFILE *fp;
{
#ifdef	BASHSTYLE
	/* bash type pretty print */
	fprintf2(fp, "%k ()\n", f -> ident);
#else
	fprintf2(fp, "%k()", f -> ident);
#endif
	if (getstatid(statementcheck(f -> func, SM_STATEMENT)) == SM_LIST - 1)
		printstree(f -> func, 0, fp);
	else {
		Xfputs("{\n", fp);
		if (f -> func) {
			printindent(1, fp);
			printstree(f -> func, 1, fp);
		}
		Xfputs("\n}", fp);
	}
}

int tinygetopt(trp, opt, nump)
syntaxtree *trp;
CONST char *opt;
int *nump;
{
	char **argv;
	int i, n, f, argc, noopt;

	argv = (trp -> comm) -> argv;
	argc = (trp -> comm) -> argc;
	f = '\0';
	noopt = 0;
	if (*opt == '-') {
		opt++;
		noopt++;
	}

	for (n = 1; n < argc; n++) {
		if (!argv[n] || argv[n][0] != '-') break;
		if (argv[n][1] == '-' && !(argv[n][2])) {
			n++;
			break;
		}

		for (i = 1; argv[n][i]; i++) {
			if (strchr2(opt, argv[n][i])) /*EMPTY*/;
			else if (noopt) break;
			else {
				execerror(argv, argv[n], ER_BADOPTIONS, 2);
				return(-1);
			}
			f = argv[n][i];
		}
		if (!f) break;
	}
	if (nump) *nump = n;

	return(f);
}

static int NEAR dochild(trp)
syntaxtree *trp;
{
#if	MSDOS
# ifdef	DEP_PSEUDOPATH
	int drv;
# endif
	char **svar, **evar, **elist, **rlist, cwd[MAXPATHLEN];
	u_long esize;
	int blevel, clevel;
#else	/* !MSDOS */
	syntaxtree *body;
	p_id_t pid;
#endif	/* !MSDOS */
	int ret;

#if	MSDOS
	if (!Xgetwd(cwd)) return(RET_FAIL);
# ifdef	DEP_PSEUDOPATH
	if ((drv = preparedrv(cwd, NULL, NULL)) < 0) {
		doperror(NULL, cwd);
		return(RET_FAIL);
	}
# endif
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
	if (chdir3(cwd, 1) < 0) ret = RET_FAIL;
# ifdef	DEP_PSEUDOPATH
	shutdrv(drv);
# endif

	return(ret);
#else	/* !MSDOS */
	body = statementbody(trp);
	if ((pid = makechild(1, (p_id_t)-1, 0)) < (p_id_t)0) return(-1);
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
#endif	/* !MSDOS */
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

	return(seterrno(EINVAL));
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
		execerror(NULL, ident, ER_NOTIDENT, 0);
		return(RET_FAIL);
	}
	trp = trp -> next;
	if ((trp -> comm) -> id != SM_IN)
		argv = duplvar(&(argvar[1]), 0);
	else {
		if (!(comm = body -> comm)) return(seterrno(EINVAL));
		trp = trp -> next;

		argv = (char **)malloc2((comm -> argc + 1) * sizeof(char *));
		for (i = 0; i < comm -> argc; i++) {
			tmp = evalvararg(comm -> argv[i], EA_BACKQ, 0);
			if (!tmp) {
				while (--i >= 0) free2(argv[i]);
				free2(argv);
				return(RET_FAIL);
			}
			argv[i] = tmp;
		}
		argv[i] = NULL;
		argc = evalifs(comm -> argc, &argv, getifs());
		if (!noglob) {
#ifdef	DEP_FILECONV
			nokanjifconv++;
#endif
			argc = evalglob(argc, &argv, EA_STRIPQ);
#ifdef	DEP_FILECONV
			nokanjifconv--;
#endif
		}
		else for (i = 0; i < argc; i++) stripquote(argv[i], EA_STRIPQ);
#ifdef	FD
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
		if (setenv2(ident, argv[i], 0) < 0) break;
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
	key = evalvararg(comm -> argv[0], EA_BACKQ | EA_STRIPQLATER, 0);
	if (!key) return(RET_FAIL);
#ifdef	FD
	demacroarg(&key);
#endif

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
			tmp = evalvararg(comm -> argv[i], EA_BACKQ, 0);
			if (!tmp) {
				ret = RET_FAIL;
				break;
			}
#ifdef	FD
			demacroarg(&tmp);
#endif
			re = regexp_init(tmp, -1);
			free2(tmp);
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
	free2(key);

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
		execerror((trp -> comm) -> argv,
			(trp -> comm) -> argv[1], ER_BADNUMBER, 0);
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
		execerror((trp -> comm) -> argv,
			(trp -> comm) -> argv[1], ER_BADNUMBER, 0);
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

	if (!shfunclevel) {
		execerror((trp -> comm) -> argv, NULL, ER_CANNOTRET, 0);
		return(RET_FAIL);
	}
	if ((trp -> comm) -> argc <= 1) ret = ret_status;
	else if ((ret = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror((trp -> comm) -> argv,
			(trp -> comm) -> argv[1], ER_BADNUMBER, 0);
		ret = RET_FAIL;
#ifdef	BASHSTYLE
	/* bash ignores "return -1" */
		return(ret);
#endif
	}
	returnlevel = shfunclevel;

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

	if (errexit && !path) {
#ifdef	DEBUG
		freevar(evar);
#endif
		Xexit2(RET_FAIL);
	}

#ifndef	NOJOB
	if (loginshell && interactive_io) killjob();
#endif
	prepareexit(1);
#ifdef	FD
	prepareexitfd(0);
#endif
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

	comm = trp -> comm;
	if (cancelredirect(comm -> redp) < 0) return(RET_FAIL);
	if (comm -> argc >= 2) {
		if (restricted) {
			execerror(comm -> argv,
				comm -> argv[1], ER_RESTRICTED, 2);
			return(RET_FAIL);
		}
		free2(comm -> argv[0]);
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
		if (ttyio < 0) /*EMPTY*/;
		else if (definput == ttyio && !isatty(STDIN_FILENO))
			definput = STDIN_FILENO;
		else if (definput == STDIN_FILENO && isatty(STDIN_FILENO))
			definput = ttyio;
	}

	return(RET_SUCCESS);
}

#ifndef	MINIMUMSHELL
/*ARGSUSED*/
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
		execerror((trp -> comm) -> argv, NULL, ER_NOTLOGINSH, 0);
		return(RET_FAIL);
	}

	return(doexit(trp));
}
#endif	/* !MINIMUMSHELL */

static int NEAR doeval(trp)
syntaxtree *trp;
{
#ifndef	MINIMUMSHELL
	long dupshlineno;
#endif
	syntaxtree *stree;
	char *cp;
	int ret;

	if ((trp -> comm) -> argc <= 1
	|| !(cp = catvar(&((trp -> comm) -> argv[1]), ' ')))
		return(RET_SUCCESS);

#ifdef	BASHSTYLE
	/* bash displays arguments of "eval", in -v mode */
	if (verboseinput) {
		argfputs(cp, Xstderr);
		VOID_C fputnl(Xstderr);
	}
#endif
#ifndef	MINIMUMSHELL
	dupshlineno = shlineno;
	setshlineno(1L);
#endif
	trp = stree = newstree(NULL);

#ifndef	NOJOB
	childpgrp = (p_id_t)-1;
#endif
	trp = execline(cp, stree, trp, 1);
	execline((char *)-1, stree, trp, 1);
	ret = (syntaxerrno) ? RET_SYNTAXERR : ret_status;
	free2(stree);
	free2(cp);
#ifndef	MINIMUMSHELL
	setshlineno(dupshlineno);
#endif

	return(ret);
}

static int NEAR doexit(trp)
syntaxtree *trp;
{
	int ret;

	if ((trp -> comm) -> argc <= 1) ret = ret_status;
	else if ((ret = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror((trp -> comm) -> argv,
			(trp -> comm) -> argv[1], ER_BADNUMBER, 0);
		ret = RET_FAIL;
#ifdef	BASHSTYLE
	/* bash ignores "exit -1" */
		return(ret);
#endif
	}
	if (exit_status < 0) exit_status = ret;
	else Xexit2(ret);

	return(RET_SUCCESS);
}

static int NEAR doread(trp)
syntaxtree *trp;
{
	char *cp, *next, *buf, *ifs;
	int n, top, opt, ret;

	if ((opt = tinygetopt(trp, "N", &top)) < 0) return(RET_FAIL);

	for (n = top; n < (trp -> comm) -> argc; n++) {
		if (identcheck((trp -> comm) -> argv[n], '\0') <= 0) {
			execerror((trp -> comm) -> argv,
				(trp -> comm) -> argv[n], ER_NOTIDENT, 0);
			return(RET_FAIL);
		}
	}

	ifs = getifs();
	ret = RET_SUCCESS;
	buf = readline(STDIN_FILENO, opt);
	if (buf == vnullstr) ret = RET_FAIL;
	else if (!buf) {
		if (errno != EINTR) doperror((trp -> comm) -> argv[0], NULL);
		ret = RET_FAIL;
	}
	else if ((trp -> comm) -> argc > top) {
		cp = buf;
		for (n = top; n < (trp -> comm) -> argc - 1; n++) {
			if (!*cp) next = cp;
			else if (!(next = strpbrk(cp, ifs)))
				next = cp + strlen(cp);
			else do {
				*(next++) = '\0';
			} while (strchr2(ifs, *next));
			if (setenv2((trp -> comm) -> argv[n], cp, 0) < 0) {
				ret = RET_FAIL;
				cp = NULL;
				break;
			}
			cp = next;
		}
		if (cp && setenv2((trp -> comm) -> argv[n], cp, 0) < 0)
			ret = RET_FAIL;
	}
#ifdef	BASHSTYLE
	/* bash set the variable REPLY without any argument */
	else if (setenv2(ENVREPLY, buf, 0) < 0) ret = RET_FAIL;
#endif
	if (buf != vnullstr) free2(buf);

	return(ret);
}

static int NEAR doshift(trp)
syntaxtree *trp;
{
	int i, n, ret;

	if ((trp -> comm) -> argc <= 1) n = 1;
	else if ((n = isnumeric((trp -> comm) -> argv[1])) < 0) {
		execerror((trp -> comm) -> argv,
			(trp -> comm) -> argv[1], ER_BADNUMBER, 0);
		return(RET_FAIL);
	}
	else if (!n) return(RET_SUCCESS);

	for (i = 0; i < n; i++) {
		if (!argvar[i + 1]) break;
		free2(argvar[i + 1]);
	}
	ret = (i >= n) ? RET_SUCCESS : RET_FAIL;
	n = i;
	for (i = 0; argvar[i + n + 1]; i++) argvar[i + 1] = argvar[i + n + 1];
	argvar[i + 1] = NULL;
	if (ret != RET_SUCCESS)
		execerror((trp -> comm) -> argv, NULL, ER_CANNOTSHIFT, 0);

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
#ifdef	BASHSTYLE
	/* bash's "set" quotes the value which includes any meta chacters */
			var[i] = quotemeta(var[i]);
#endif
			kanjifputs(var[i], Xstdout);
			VOID_C fputnl(Xstdout);
		}
		freevar(var);

		func = duplfunc(shellfunc);
		for (i = 0; func[i].ident; i++) /*EMPTY*/;
		if (i > 1) qsort(func, i, sizeof(shfunctable), cmpfunc);
		for (i = 0; func[i].ident; i++) {
			printshfunc(&(func[i]), Xstdout);
			VOID_C fputnl(Xstdout);
		}
		freefunc(func);
		return(RET_SUCCESS);
	}

	if ((n = getoption(argc, argv, 0)) < 0) return(RET_FAIL);
#ifdef	BASHSTYLE
	/* bash makes -e option effective immediately */
	errorexit = tmperrorexit;
#endif
	if (n > 2) {
		if (argc <= 2) {
			for (i = 0; i < FLAGSSIZ; i++) {
				if (!(shflaglist[i].ident)) continue;
				fprintf2(Xstdout, "%-16.16s%s",
					shflaglist[i].ident,
					(*(shflaglist[i].var)) ? "on" : "off");
				VOID_C fputnl(Xstdout);
			}
		}
		else {
			for (i = 0; i < FLAGSSIZ; i++) {
				if (!(shflaglist[i].ident)) continue;
				if (!strcmp(argv[2], shflaglist[i].ident))
					break;
			}
			if (i >= FLAGSSIZ) {
				execerror(argv, argv[2], ER_BADOPTIONS, 0);
				return(RET_FAIL);
			}

			setshflag(i, (n <= 3) ? 1 : 0);
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
#ifdef	DEP_PTY
	sendparent(TE_SETVAR, &argvar, argvar);
#endif

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
		else n = unsetshfunc(argv[i], len);
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
			execerror((trp -> comm) -> argv,
				(trp -> comm) -> argv[i], ER_NOTFOUND, 1);
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
	else if (!(dir = getconstvar(ENVHOME))) {
		execerror((trp -> comm) -> argv,
			(trp -> comm) -> argv[0], ER_NOHOMEDIR, 0);
		return(RET_FAIL);
	}
	else if (!*dir) return(RET_SUCCESS);

#ifdef	FD
	dupphysical_path = physical_path;
	if (opt) physical_path = (opt == 'P') ? 1 : 0;
#endif

	if (isrootdir(dir)) next = NULL;
#ifdef	DEP_DOSPATH
	else if (_dospath(dir)) next = NULL;
#endif
#ifdef	FD
# ifdef	DEP_DOSDRIVE
	else if (dir[0] && !dir[1] && strchr2(".?-@", dir[0])) next = NULL;
# else
	else if (dir[0] && !dir[1] && strchr2(".?-", dir[0])) next = NULL;
# endif
#endif	/* FD */
	else if ((next = getconstvar(ENVCDPATH))) {
		tmp = ((cp = strdelim(dir, 0)))
			? strndup2(dir, cp - dir) : dir;
		if (isdotdir(tmp)) next = NULL;
		if (tmp != dir) free2(tmp);
	}

	if (!next) {
		if (chdir3(dir, 0) >= 0) {
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
#ifdef	DEP_DOSPATH
			if (_dospath(cp)) next = strchr2(&(cp[2]), PATHDELIM);
			else
#endif
			next = strchr2(cp, PATHDELIM);
			dlen = (next) ? (next++) - cp : strlen(cp);
			if (!dlen) tmp = NULL;
			else {
				tmp = _evalpath(cp, cp + dlen, 0);
				dlen = strlen(tmp);
			}
			if (dlen + len + 1 + 1 > size) {
				size = dlen + len + 1 + 1;
				path = realloc2(path, size);
			}
			if (tmp) {
				strncpy2(path, tmp, dlen);
				free2(tmp);
				dlen = strcatdelim(path) - path;
			}
			strncpy2(&(path[dlen]), dir, len);
			if (chdir3(path, 1) >= 0) {
				kanjifputs(path, Xstdout);
				VOID_C fputnl(Xstdout);
				free2(path);
#ifdef	FD
				physical_path = dupphysical_path;
#endif
				return(RET_SUCCESS);
			}
		}
		free2(path);
	}
	execerror((trp -> comm) -> argv, dir, ER_BADDIR, 0);
#ifdef	FD
	physical_path = dupphysical_path;
#endif

	return(RET_FAIL);
}

static int NEAR dopwd(trp)
syntaxtree *trp;
{
#ifdef	FD
	int opt;
#endif
	char buf[MAXPATHLEN];

#ifdef	FD
	if ((opt = tinygetopt(trp, "LP", NULL)) < 0) return(RET_FAIL);

	if (!((opt) ? (opt == 'P') : physical_path)) strcpy2(buf, fullpath);
	else
#endif
	if (!Xgetwd(buf)) {
		execerror((trp -> comm) -> argv, NULL, ER_CANNOTSTAT, 0);
		return(RET_FAIL);
	}
	kanjifputs(buf, Xstdout);
	VOID_C fputnl(Xstdout);

	return(RET_SUCCESS);
}

static int NEAR dosource(trp)
syntaxtree *trp;
{
#ifdef	BASHSTYLE
	char **var, **dupargvar;
#endif
	hashlist *hp;
	CONST char *fname;
	int n, fd;

	if ((trp -> comm) -> argc <= 1) return(RET_SUCCESS);
	fname = (trp -> comm) -> argv[1];
	n = searchhash(&hp, fname, NULL);
	if (restricted && (n & CM_FULLPATH)) {
		execerror((trp -> comm) -> argv, fname, ER_RESTRICTED, 2);
		return(RET_FAIL);
	}
	if (n & CM_NOTFOUND) {
		execerror((trp -> comm) -> argv, fname, ER_NOTFOUND, 1);
		return(RET_FAIL);
	}
	if ((fd = newdup(Kopen(fname, O_TEXT | O_RDONLY, 0666))) < 0) {
		doperror((trp -> comm) -> argv[0], fname);
		return(RET_FAIL);
	}
#ifdef	BASHSTYLE
	/* bash sets positional parameters with source arguments */
	if ((trp -> comm) -> argc > 2) {
		dupargvar = argvar;
		argvar = (char **)malloc2((trp -> comm) -> argc
			* sizeof(char *));
		argvar[0] = strdup2(dupargvar[0]);
		for (n = 1; n < (trp -> comm) -> argc - 1; n++)
			argvar[n] = strdup2((trp -> comm) -> argv[n + 1]);
		argvar[n] = NULL;
		var = argvar;
		sourcefile(fd, fname, 0);
		safeclose(fd);
		if (var != argvar) freevar(dupargvar);
		else {
			freevar(argvar);
			argvar = dupargvar;
		}
	}
	else
#endif	/* BASHSTYLE */
	{
		sourcefile(fd, fname, 0);
		safeclose(fd);
	}

	return(ret_status);
}

static int NEAR expandlist(varp, ident)
char ***varp;
CONST char *ident;
{
#ifndef	MINIMUMSHELL
	char *cp;
#endif
	int len;

#ifdef	MINIMUMSHELL
	len = identcheck(ident, '\0');
#else
	len = identcheck(ident, '=');
#endif
	if (!len) {
		execerror(NULL, ident, ER_NOTIDENT, 0);
		return(-1);
	}
#ifndef	MINIMUMSHELL
	else if (len < 0) len = -len;
	else if (_putshellvar(cp = strdup2(ident), len) < 0) {
		free2(cp);
		return(-1);
	}
#endif
	*varp = expandvar(*varp, ident, len);

	return(len);
}

int setexport(ident)
CONST char *ident;
{
	int i, len;

	if ((len = expandlist(&exportlist, ident)) < 0) return(-1);
	if ((i = searchvar(shellvar, ident, len, '=')) >= 0)
		exportvar = putvar(exportvar, strdup2(shellvar[i]), len);

	return(len);
}

int setronly(ident)
CONST char *ident;
{
	return(expandlist(&ronlylist, ident));
}

static int NEAR doexport(trp)
syntaxtree *trp;
{
	char **argv;
	int i, n, ret;

	argv = (trp -> comm) -> argv;
	if ((trp -> comm) -> argc <= 1) {
		for (i = 0; exportlist[i]; i++) {
			fprintf2(Xstdout, "export %k", exportlist[i]);
			VOID_C fputnl(Xstdout);
		}
		return(RET_SUCCESS);
	}

	ret = RET_SUCCESS;
	for (n = 1; n < (trp -> comm) -> argc; n++) {
		if (setexport(argv[n]) < 0) {
			ret = RET_FAIL;
			ERRBREAK;
		}
#ifdef	DEP_PTY
		sendparent(TE_SETEXPORT, argv[n]);
#endif
	}

	return(ret);
}

static int NEAR doreadonly(trp)
syntaxtree *trp;
{
	char **argv;
	int i, n, ret;

	argv = (trp -> comm) -> argv;
	if ((trp -> comm) -> argc <= 1) {
		for (i = 0; ronlylist[i]; i++) {
			fprintf2(Xstdout, "readonly %k", ronlylist[i]);
			VOID_C fputnl(Xstdout);
		}
		return(RET_SUCCESS);
	}

	ret = RET_SUCCESS;
	for (n = 1; n < (trp -> comm) -> argc; n++) {
		if (setronly(argv[n]) < 0) {
			ret = RET_FAIL;
			ERRBREAK;
		}
#ifdef	DEP_PTY
		sendparent(TE_SETRONLY, argv[n]);
#endif
	}

	return(ret);
}

/*ARGSUSED*/
static int NEAR dotimes(trp)
syntaxtree *trp;
{
	time_t usrtime, systime;
#ifdef	USEGETRUSAGE
	struct rusage ru;

	getrusage(RUSAGE_CHILDREN, &ru);
	usrtime = ru.ru_utime.tv_sec;
	systime = ru.ru_stime.tv_sec;
#else	/* !USEGETRUSAGE */
# ifdef	USETIMES
	struct tms buf;
	int clk;

	times(&buf);
	clk = CLKPERSEC;
	usrtime = buf.tms_cutime / clk;
	if (buf.tms_cutime % clk > clk / 2) usrtime++;
	systime = buf.tms_cstime / clk;
	if (buf.tms_cstime % clk > clk / 2) systime++;
# else
	usrtime = systime = (time_t)0;
# endif
#endif	/* !USEGETRUSAGE */
	fprintf2(Xstdout, "%dm%ds %dm%ds",
		(int)(usrtime / 60), (int)(usrtime % 60),
		(int)(systime / 60), (int)(systime % 60));
	VOID_C fputnl(Xstdout);

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
	int i, j;
# endif

	if ((trp -> comm) -> argc <= 1) {
# ifdef	NOJOB
		pid = lastpid;
# else
		for (;;) {
			for (i = 0; i < maxjobs; i++) {
				if (!(joblist[i].pids)) continue;
				j = joblist[i].npipe;
				if (!(joblist[i].stats[j])) break;
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
			checkjob(NULL);
			if ((i = getjob(s)) < 0) pid = (p_id_t)-1;
			else {
				j = joblist[i].npipe;
				pid = joblist[i].pids[j];
			}
		}
		else
# endif	/* !NOJOB */
		if ((pid = isnumeric(s)) < (p_id_t)0) {
			execerror((trp -> comm) -> argv, s, ER_BADNUMBER, 0);
			return(RET_FAIL);
		}
# ifndef	NOJOB
		else {
			checkjob(NULL);
			if (!joblist || (i = searchjob(pid, &j)) < 0) {
				pid = (p_id_t)-1;
#  ifdef	FAKEUNINIT
				i = -1;		/* fake for -Wuninitialized */
#  endif
			}
		}

		if (pid < (p_id_t)0) {
			execerror((trp -> comm) -> argv, s, ER_NOSUCHJOB, 2);
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
#ifdef	BASHSTYLE
		fprintf2(Xstdout, "%03o", n & 0777);
#else
		fprintf2(Xstdout, "%04o", n & 0777);
#endif
		VOID_C fputnl(Xstdout);
	}
	else {
		s = (trp -> comm) -> argv[1];
		n = 0;
		for (i = 0; s[i] >= '0' && s[i] <= '7'; i++)
			n = (n << 3) + s[i] - '0';
#ifdef	BASHSTYLE
		if (n & ~0777) {
			execerror((trp -> comm) -> argv, s, ER_NUMOUTRANGE, 0);
			return(RET_FAIL);
		}
#else
		n &= 0777;
#endif
		umask(n);
	}

	return(RET_SUCCESS);
}

/*ARGSUSED*/
static int NEAR doulimit(trp)
syntaxtree *trp;
{
#if	!defined (USERESOURCEH) && !defined (USEULIMITH)
	execerror((trp -> comm) -> argv, NULL, ER_BADULIMIT, 1);

	return(RET_FAIL);
#else	/* USERESOURCEH || USEULIMITH */
# ifdef	USERESOURCEH
	struct rlimit lim;
	XFILE *fp;
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
				execerror(argv, argv[n], ER_BADULIMIT, 1);
				return(RET_FAIL);
			}
			val *= ulimitlist[res].unit;
		}
	}
	if (err) {
#  ifdef	BASHSTYLE
		fp = Xstderr;
#  else
		fp = Xstdout;
#  endif
		fprintf2(fp, "usage: %k [ -HSa", argv[0]);
		for (j = 0; j < ULIMITSIZ; j++)
			Xfputc(ulimitlist[j].opt, fp);
		Xfputs(" ] [ limit ]", fp);
		VOID_C fputnl(fp);
#  ifdef	BASHSTYLE
		return(RET_SYNTAXERR);
#  else
		return(RET_SUCCESS);
#  endif
	}

	if (n >= argc) {
		for (i = 0; i < ULIMITSIZ; i++) if (flags & ((u_long)1 << i)) {
			if (res < 0)
				fprintf2(Xstdout, "%s ", ulimitlist[i].mes);
			if (getrlimit(ulimitlist[i].res, &lim) < 0) {
				execerror(argv, NULL, ER_BADULIMIT, 1);
				return(RET_FAIL);
			}
			if (hs & 2) {
				if (lim.rlim_cur == RLIM_INFINITY)
					Xfputs(UNLIMITED, Xstdout);
				else if (lim.rlim_cur) fprintf2(Xstdout, "%ld",
					lim.rlim_cur / ulimitlist[i].unit);
			}
			if (hs & 1) {
				if (hs & 2) Xfputc(':', Xstdout);
				if (lim.rlim_max == RLIM_INFINITY)
					Xfputs(UNLIMITED, Xstdout);
				else if (lim.rlim_max) fprintf2(Xstdout, "%ld",
					lim.rlim_max / ulimitlist[i].unit);
			}
			VOID_C fputnl(Xstdout);
		}
	}
	else {
		if (getrlimit(ulimitlist[res].res, &lim) < 0) {
			execerror(argv, NULL, ER_BADULIMIT, 1);
			return(RET_FAIL);
		}
		if (hs & 1) lim.rlim_max = (inf) ? RLIM_INFINITY : val;
		if (hs & 2) lim.rlim_cur = (inf) ? RLIM_INFINITY : val;
		if (setrlimit(ulimitlist[res].res, &lim) < 0) {
			execerror(argv, NULL, ER_BADULIMIT, 1);
			return(RET_FAIL);
		}
	}
# else	/* !USERESOURCEH */
	if (argc <= 1) {
		if ((val = ulimit(UL_GETFSIZE, 0L)) < 0L) {
			execerror(argv, NULL, ER_BADULIMIT, 1);
			return(RET_FAIL);
		}
		if (val == RLIM_INFINITY) Xfputs(UNLIMITED, Xstdout);
		else fprintf2(Xstdout, "%ld", val * 512L);
		VOID_C fputnl(Xstdout);
	}
	else {
		if (!strcmp(argv[1], UNLIMITED)) val = RLIM_INFINITY;
		else {
			if ((val = isnumeric(argv[1])) < 0L) {
				execerror(argv, argv[1], ER_BADULIMIT, 1);
				return(RET_FAIL);
			}
			val /= 512L;
		}
		if (ulimit(UL_SETFSIZE, val) < 0L) {
			execerror(argv, NULL, ER_BADULIMIT, 1);
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
			fprintf2(Xstdout, "%d: %k", i, trapcomm[i]);
			VOID_C fputnl(Xstdout);
		}
		return(RET_SUCCESS);
	}

	n = 1;
	comm = (isnumeric(argv[n]) < 0) ? argv[n++] : NULL;

	for (; n < (trp -> comm) -> argc; n++) {
		if ((sig = isnumeric(argv[n])) < 0) {
			execerror(argv, argv[n], ER_BADNUMBER, 0);
			return(RET_FAIL);
		}
		if (sig >= NSIG) {
			execerror(argv, argv[n], ER_BADTRAP, 0);
			return(RET_FAIL);
		}

		if (!sig) {
			trapmode[0] = (comm) ? TR_TRAP : 0;
			free2(trapcomm[0]);
			trapcomm[0] = strdup2(comm);
			continue;
		}

		for (i = 0; signallist[i].sig >= 0; i++)
			if (sig == signallist[i].sig) break;
		if (signallist[i].sig < 0
		|| (signallist[i].flags & TR_NOTRAP)) {
			execerror(argv, argv[n], ER_BADTRAP, 0);
			return(RET_FAIL);
		}

		signal2(sig, SIG_IGN);
		free2(trapcomm[sig]);
		if (!comm) {
			trapmode[sig] = signallist[i].flags;
			if (trapmode[sig] & TR_BLOCK)
				signal2(sig, (sigcst_t)(signallist[i].func));
			else signal2(sig, oldsigfunc[sig]);
			trapcomm[sig] = NULL;
		}
		else {
			trapmode[sig] = TR_TRAP;
			trapcomm[sig] = strdup2(comm);
			if ((signallist[i].flags & TR_STAT) == TR_STOP)
				signal2(sig, SIG_DFL);
			else signal2(sig, (sigcst_t)(signallist[i].func));
		}
	}

	return(RET_SUCCESS);
}

#ifndef	NOJOB
static int NEAR dojobs(trp)
syntaxtree *trp;
{
	return(posixjobs(trp));
}

static int NEAR dofg(trp)
syntaxtree *trp;
{
	return(posixfg(trp));
}

static int NEAR dobg(trp)
syntaxtree *trp;
{
	return(posixbg(trp));
}

static int NEAR dodisown(trp)
syntaxtree *trp;
{
	char *s;
	int i;

	s = ((trp -> comm) -> argc > 1) ? (trp -> comm) -> argv[1] : NULL;
	checkjob(NULL);
	if ((i = getjob(s)) < 0) {
		execerror((trp -> comm) -> argv,
			(trp -> comm) -> argv[1],
			(i < -1) ? ER_TERMINATED : ER_NOSUCHJOB, 2);
		return(RET_FAIL);
	}
	free2(joblist[i].pids);
	free2(joblist[i].stats);
	if (joblist[i].trp) {
		freestree(joblist[i].trp);
		free2(joblist[i].trp);
	}
	joblist[i].pids = NULL;

	return(RET_SUCCESS);
}
#endif	/* !NOJOB */

int typeone(s, fp)
CONST char *s;
XFILE *fp;
{
	hashlist *hp;
	int type, id;

	kanjifputs(s, fp);
#ifdef	NOALIAS
	type = checktype(s, &id, 1);
#else
	type = checktype(s, &id, 1, 1);
#endif

	if (type == CT_BUILTIN) Xfputs(" is a shell builtin", fp);
#ifdef	FD
	else if (type == CT_FDORIGINAL) Xfputs(" is a FDclone builtin", fp);
	else if (type == CT_FDINTERNAL)
		Xfputs(" is a FDclone internal", fp);
#endif
#if	MSDOS
	else if (type == CT_LOGDRIVE)
		Xfputs(" is a change drive command", fp);
#endif
	else if (type == CT_FUNCTION) {
		Xfputs(" is a function\n", fp);
		printshfunc(&(shellfunc[id]), fp);
	}
#ifndef	NOALIAS
	else if (type == CT_ALIAS)
		fprintf2(fp, " is a aliased to `%k'", shellalias[id].comm);
#endif
	else {
		type = searchhash(&hp, s, NULL);
#ifdef	_NOUSEHASH
		if (!(type & CM_FULLPATH)) s = (char *)hp;
#else
		if (type & CM_HASH) s = hp -> path;
#endif
		if (restricted && (type & CM_FULLPATH))
			Xfputs(": restricted", fp);
		else if ((type & CM_NOTFOUND) || Xaccess(s, X_OK) < 0) {
			Xfputs(" not found", fp);
			VOID_C fputnl(fp);
			return(-1);
		}
		else fprintf2(fp, " is %k", s);
	}
	VOID_C fputnl(fp);

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
		if (typeone((trp -> comm) -> argv[i], Xstdout) >= 0)
#ifdef	BASHSTYLE
			ret = RET_SUCCESS;
#else
			/*EMPTY*/;
		else ret = RET_FAIL;
#endif
	}

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
	int i, n, opt;

	n = 1;
	if ((opt = tinygetopt(trp, "-nN", NULL)) < 0) return(RET_FAIL);
	if (opt) n++;

	for (i = n; i < (trp -> comm) -> argc; i++) {
		if (i > n) Xfputc(' ', Xstdout);
		if ((trp -> comm) -> argv[i])
			argfputs((trp -> comm) -> argv[i], Xstdout);
	}
	if (opt == 'n') /*EMPTY*/;
	else if (opt == 'N') Xfputs("\r\n", Xstdout);
	else VOID_C fputnl(Xstdout);
	Xfflush(Xstdout);

	return(RET_SUCCESS);
}

#ifndef	MINIMUMSHELL
static int NEAR dokill(trp)
syntaxtree *trp;
{
	return(posixkill(trp));
}

static int NEAR dotest(trp)
syntaxtree *trp;
{
	return(posixtest(trp));
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

/*ARGSUSED*/
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

# if	0				/* exists in FD original builtin */
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
	else strcpy2(path, fullpath);
# else
	if (!Xgetwd(path)) {
		execerror(NULL, NULL, ER_CANNOTSTAT, 0);
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
			execerror((trp -> comm) -> argv, NULL, ER_DIREMPTY, 2);
			return(RET_FAIL);
		}
		cp = evalpath(strdup2(dirstack[0]), 0);
		i = chdir3(cp, 1);
		free2(cp);
		if (i < 0) {
			execerror((trp -> comm) -> argv,
				dirstack[0], ER_BADDIR, 0);
			return(RET_FAIL);
		}
		free2(dirstack[0]);
# ifdef	DEP_PTY
		sendparent(TE_POPVAR, &dirstack);
# endif
	}
	else {
		if (chdir3((trp -> comm) -> argv[1], 0) < 0) {
			execerror((trp -> comm) -> argv,
				(trp -> comm) -> argv[1], ER_BADDIR, 0);
			return(RET_FAIL);
		}
		i = countvar(dirstack);
		dirstack = (char **)realloc2(dirstack,
			(i + 1 + 1) * sizeof(char *));
		memmove((char *)&(dirstack[1]), (char *)&(dirstack[0]),
			i * sizeof(char *));
		dirstack[i + 1] = NULL;
	}
	dirstack[0] = strdup2(path);
	dodirs(trp);
# ifdef	DEP_PTY
	sendparent(TE_PUSHVAR, &dirstack, path);
# endif

	return(RET_SUCCESS);
}

static int NEAR dopopd(trp)
syntaxtree *trp;
{
	char *cp;
	int i;

	if (!dirstack || !dirstack[0]) {
		execerror((trp -> comm) -> argv, NULL, ER_DIREMPTY, 2);
		return(RET_FAIL);
	}
	cp = evalpath(strdup2(dirstack[0]), 0);
	i = chdir3(cp, 1);
	free2(cp);
	if (i < 0) {
		execerror((trp -> comm) -> argv, dirstack[0], ER_BADDIR, 0);
		return(RET_FAIL);
	}
	i = countvar(dirstack);
	free2(dirstack[0]);
	memmove((char *)&(dirstack[0]), (char *)&(dirstack[1]),
		i * sizeof(char *));
	dodirs(trp);
# ifdef	DEP_PTY
	sendparent(TE_POPVAR, &dirstack);
# endif

	return(RET_SUCCESS);
}

/*ARGSUSED*/
static int NEAR dodirs(trp)
syntaxtree *trp;
{
	char path[MAXPATHLEN];
	int i;

	if (getworkdir(path) < 0) return(RET_FAIL);
	kanjifputs(path, Xstdout);
	if (dirstack)
		for (i = 0; dirstack[i]; i++)
			fprintf2(Xstdout, " %k", dirstack[i]);
	VOID_C fputnl(Xstdout);

	return(RET_SUCCESS);
}

static int NEAR doenable(trp)
syntaxtree *trp;
{
	char **argv;
	int i, j, n, ret;

	argv = (trp -> comm) -> argv;
	n = 1;
	if ((trp -> comm) -> argc > 1 && argv[n][0] == '-'
	&& argv[n][1] == 'n' && !(argv[n][2]))
		n++;

	ret = RET_SUCCESS;
	if (n >= (trp -> comm) -> argc) for (j = 0; j < SHBUILTINSIZ; j++) {
		if (n == 1) {
			if (shbuiltinlist[j].flags & BT_DISABLE) continue;
		}
		else {
			if (!(shbuiltinlist[j].flags & BT_DISABLE)) continue;
		}

		for (i = 0; i < (trp -> comm) -> argc; i++)
			fprintf2(Xstdout, "%k ", argv[i]);
		fprintf2(Xstdout, "%k\n", shbuiltinlist[j].ident);
	}
	else for (i = n; i < (trp -> comm) -> argc; i++) {
		for (j = 0; j < SHBUILTINSIZ; j++)
			if (!strcommcmp(argv[i], shbuiltinlist[j].ident))
				break;
		if (j >= SHBUILTINSIZ) {
			execerror(argv, argv[i], ER_COMNOFOUND, 1);
			ret = RET_FAIL;
			continue;
		}

		if (n == 1) shbuiltinlist[j].flags &= ~BT_DISABLE;
		else shbuiltinlist[j].flags |= BT_DISABLE;
	}

	return(ret);
}

static int NEAR dobuiltin(trp)
syntaxtree *trp;
{
	char **argv;
	int i, ret, argc;

	argc = (trp -> comm) -> argc;
	argv = (trp -> comm) -> argv;

	if (interrupted) return(RET_INTR);
	if (argc <= 1) return(ret_status);

	for (i = 0; i < SHBUILTINSIZ; i++) {
		if (shbuiltinlist[i].flags & BT_DISABLE) continue;
		if (!strcommcmp(argv[1], shbuiltinlist[i].ident)) break;
	}

	if (i >= SHBUILTINSIZ) {
		execerror(argv, argv[1], ER_COMNOFOUND, 1);
		return(RET_FAIL);
	}

	if (restricted && (shbuiltinlist[i].flags & BT_RESTRICT)) {
		execerror(argv, argv[1], ER_RESTRICTED, 0);
		return(RET_FAIL);
	}
	if (!shbuiltinlist[i].func) return(RET_FAIL);
	(trp -> comm) -> argc--;
	(trp -> comm) -> argv++;
	ret = (*(shbuiltinlist[i].func))(trp);

	(trp -> comm) -> argc = argc;
	(trp -> comm) -> argv = argv;

	return(ret);
}

static int NEAR doaddcr(trp)
syntaxtree *trp;
{
	CONST char *fname;
	char *buf;
	int n, fd, ret, opt;

	if ((opt = tinygetopt(trp, "1", &n)) < 0) return(RET_FAIL);
	fname = (trp -> comm) -> argv[n];

	if (!fname) fd = STDIN_FILENO;
	else if ((fd = newdup(Kopen(fname, O_TEXT | O_RDONLY, 0666))) < 0) {
		doperror((trp -> comm) -> argv[0], fname);
		return(RET_FAIL);
	}

	ret = RET_SUCCESS;
	while ((buf = readline(fd, 'N')) != vnullstr) {
		if (!buf) {
			if (errno != EINTR)
				doperror((trp -> comm) -> argv[0], fname);
			ret = RET_FAIL;
			break;
		}

		Xfputs(buf, Xstdout);
		free2(buf);
		Xfputs("\r\n", Xstdout);
		Xfflush(Xstdout);

		if (opt == '1') break;
	}
	safeclose(fd);

	return(ret);
}
#endif	/* !MINIMUMSHELL */

#ifdef	WITHSOCKET
static int NEAR doaccept(trp)
syntaxtree *trp;
{
	CONST char *cp;
	int s, fd;

	if (!(cp = (trp -> comm) -> argv[1])) s = STDIN_FILENO;
	else if ((s = isnumeric(cp)) < 0) {
		execerror((trp -> comm) -> argv, cp, ER_BADNUMBER, 0);
		return(RET_FAIL);
	}

	if (issocket(s) < 0) {
		execerror((trp -> comm) -> argv, cp, ER_NOTSOCKET, 2);
		return(RET_FAIL);
	}
	if ((fd = sockaccept(s, SCK_THROUGHPUT)) < 0) {
		doperror((trp -> comm) -> argv[0], cp);
		return(RET_FAIL);
	}
	s = Xdup2(fd, s);
	safeclose(fd);
	if (s < 0) {
		doperror((trp -> comm) -> argv[0], cp);
		return(RET_FAIL);
	}

	return(RET_SUCCESS);
}

static int NEAR dosocketinfo(trp)
syntaxtree *trp;
{
	char addr[SCK_ADDRSIZE + 1];
	CONST char *cp;
	int n, s, opt, port;

	if ((opt = tinygetopt(trp, "apAP", &n)) < 0) return(RET_FAIL);

	if (!(cp = (trp -> comm) -> argv[n])) s = STDIN_FILENO;
	else if ((s = isnumeric(cp)) < 0) {
		execerror((trp -> comm) -> argv, cp, ER_BADNUMBER, 0);
		return(RET_FAIL);
	}

	if (issocket(s) < 0) {
		execerror((trp -> comm) -> argv, NULL, ER_NOTSOCKET, 2);
		return(RET_FAIL);
	}
	if (!isupper2(opt)) {
		if (getsockinfo(s, addr, sizeof(addr), &port, 1) >= 0) {
			if (opt == 'a') fprintf2(Xstdout, "%s\n", addr);
			else if (opt == 'p') fprintf2(Xstdout, "%d\n", port);
			else fprintf2(Xstdout, "Remote: %s:%d\n", addr, port);
		}
# ifdef	ENOTCONN
		else if (errno == ENOTCONN) {
			if (opt) fprintf2(Xstdout, "%s\n", nullstr);
			else fprintf2(Xstdout, "Remote: (not connected)\n");
		}
# endif
		else {
			execerror((trp -> comm) -> argv,
				NULL, ER_INVALSOCKET, 2);
			return(RET_FAIL);
		}
	}
	if (!islower2(opt)) {
		if (getsockinfo(s, addr, sizeof(addr), &port, 0) >= 0) {
			if (opt == 'A') fprintf2(Xstdout, "%s\n", addr);
			else if (opt == 'P') fprintf2(Xstdout, "%d\n", port);
			else fprintf2(Xstdout, "Local: %s:%d\n", addr, port);
		}
		else {
			execerror((trp -> comm) -> argv,
				NULL, ER_INVALSOCKET, 2);
			return(RET_FAIL);
		}
	}

	return(RET_SUCCESS);
}
#endif	/* WITHSOCKET */

#ifdef	FD
static int NEAR dofd(trp)
syntaxtree *trp;
{
	int n, mode;

	if (!shellmode || exit_status < 0) {
		execerror((trp -> comm) -> argv, NULL, ER_RECURSIVEFD, 0);
		return(RET_FAIL);
	}
	else if (dumbterm > 1) {
		execerror((trp -> comm) -> argv,
			getconstvar(ENVTERM), ER_INVALTERMFD, 1);
		return(RET_FAIL);
	}
	else if (!interactive || nottyout) {
		execerror((trp -> comm) -> argv, NULL, ER_INVALTERMFD, 1);
		return(RET_FAIL);
	}
	else {
		n = sigvecset(1);
		ttyiomode(0);
		mode = termmode(1);
		shellmode = 0;
		main_fd(&((trp -> comm) -> argv[1]), 1);
		shellmode = 1;
		termmode(mode);
		stdiomode();
		VOID_C fputnl(Xstderr);
		sigvecset(n);
	}

	return(RET_SUCCESS);
}
#endif	/* FD */

static int NEAR doshfunc(trp, no)
syntaxtree *trp;
int no;
{
#ifndef	MINIMUMSHELL
	long dupshlineno;
#endif
	char **avar;
	int ret;

#ifndef	MINIMUMSHELL
	dupshlineno = shlineno;
#endif
	avar = argvar;
	argvar = duplvar((trp -> comm) -> argv, 0);
	shfunclevel++;
	ret = exec_stree(shellfunc[no].func, 0);
	if (returnlevel >= shfunclevel) returnlevel = 0;
	shfunclevel--;
	freevar(argvar);
	argvar = avar;
#ifndef	MINIMUMSHELL
	setshlineno(dupshlineno);
#endif

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

	if (!trp) {
		printindent(lvl, Xstdout);
		Xfputs("(null):", Xstdout);
		VOID_C fputnl(Xstdout);
		return;
	}

	if (trp -> flags & ST_NODE) {
		printindent(lvl, Xstdout);
		Xfputs("node:\n", Xstdout);
		show_stree((syntaxtree *)(trp -> comm), lvl + 1);
	}
	else if (!(trp -> comm)) {
		printindent(lvl, Xstdout);
		Xfputs("body: NULL", Xstdout);
		VOID_C fputnl(Xstdout);
	}
	else {
		printindent(lvl, Xstdout);
		if (isstatement(trp -> comm)) {
			id = (trp -> comm) -> id;
			i = id - 1;
			if (id == SM_CHILD) Xfputs("sub shell", Xstdout);
			else if ((id == SM_STATEMENT))
				Xfputs("statement", Xstdout);
			else Xfputs(statementlist[i].ident, Xstdout);
			Xfputs(":\n", Xstdout);
			show_stree(statementbody(trp), lvl + 1);
		}
		else for (i = 0; i <= (trp -> comm) -> argc; i++) {
			if (!((trp -> comm) -> argv)
			|| !((trp -> comm) -> argv[i]))
				Xfputs("NULL", Xstdout);
			else if (!i && isbuiltin(trp -> comm))
				argfputs((trp -> comm) -> argv[i], Xstdout);
			else fprintf2(Xstdout, "\"%a\"",
				(trp -> comm) -> argv[i]);
			Xfputc((i < (trp -> comm) -> argc) ? ' ' : '\n',
				Xstdout);
		}
		for (rp = (trp -> comm) -> redp; rp; rp = rp -> next) {
			printindent(lvl, Xstdout);
			fprintf2(Xstdout, "redirect %d", rp -> fd);
			if (!(rp -> filename)) Xfputs(">-: ", Xstdout);
			else if (rp -> type & MD_HEREDOC)
				fprintf2(Xstdout, ">> \"%a\": ",
					((heredoc_t *)(rp -> filename))
						-> eof);
			else fprintf2(Xstdout, "> \"%a\": ", rp -> filename);
			fprintf2(Xstdout, "%06o", (int)(rp -> type));
			VOID_C fputnl(Xstdout);
		}
	}
	if (trp -> type) {
		for (i = 0; i < OPELISTSIZ; i++)
			if (trp -> type == opelist[i].op) break;
		if (i < OPELISTSIZ) {
			printindent(lvl, Xstdout);
			Xfputs(opelist[i].symbol, Xstdout);
			VOID_C fputnl(Xstdout);
		}
	}
	if (trp -> next) {
		if (trp -> cont & (CN_QUOT | CN_ESCAPE)) {
			rp = (redirectlist *)(trp -> next);
			printindent(lvl, Xstdout);
			fprintf2(Xstdout, "continuing...\"%a\"",
				rp -> filename);
			VOID_C fputnl(Xstdout);
		}
# ifndef	MINIMUMSHELL
		else if (trp -> flags & ST_BUSY);
# endif
		else show_stree(trp -> next, lvl);
	}
}
#endif	/* SHOWSTREE */

VOID setshfunc(ident, trp)
char *ident;
syntaxtree *trp;
{
	int i;

	for (i = 0; shellfunc && shellfunc[i].ident; i++)
		if (!strcommcmp(ident, shellfunc[i].ident)) break;
	if (shellfunc && shellfunc[i].ident) {
		free2(shellfunc[i].ident);
		freestree(shellfunc[i].func);
		free2(shellfunc[i].func);
	}
	else {
		shellfunc = (shfunctable *)realloc2(shellfunc,
			(i + 2) * sizeof(shfunctable));
		shellfunc[i + 1].ident = NULL;
	}
	shellfunc[i].ident = ident;
	shellfunc[i].func = trp;
#ifndef	_NOUSEHASH
	if (hashahead) check_stree(trp);
#endif
}

static int NEAR dosetshfunc(ident, trp)
CONST char *ident;
syntaxtree *trp;
{
	syntaxtree *functr;
	char *new;
	int len;

#ifndef	BASHSTYLE
	/* bash allows any character in the function identifier */
	if (identcheck(ident, '\0') <= 0) {
		execerror(NULL, ident, ER_NOTIDENT, 0);
		return(-1);
	}
#endif
	new = strdup2(ident);
	stripquote(new, EA_STRIPQ);
	len = strlen(new);
#ifndef	BASHSTYLE
	/* bash distinguishes the same named function and variable */
	if (unset(new, len) < 0) {
		free2(new);
		return(RET_FAIL);
	}
#endif

	trp = trp -> next;
	if (!(functr = statementbody(trp))) {
		free2(new);
		return(RET_FAIL);
	}
#ifdef	MINIMUMSHELL
	(trp -> comm) -> argv = (char **)duplstree(functr, NULL);
#else
	(trp -> comm) -> argv =
		(char **)duplstree(functr, NULL, functr -> lineno);
#endif
	functr -> flags |= ST_TOP;
	setshfunc(new, functr);
#ifdef	DEP_PTY
	if (parentfd >= 0 && mypid == shellpid) {
		sendparent(TE_ADDFUNCTION, ident, functr);
		nownstree(functr);
	}
#endif

	return(RET_SUCCESS);
}

int unsetshfunc(ident, len)
CONST char *ident;
int len;
{
	int i;

	if (checkronly(ident, len) < 0) return(-1);
	if (!shellfunc) return(0);
	for (i = 0; shellfunc[i].ident; i++)
		if (!strncommcmp(ident, shellfunc[i].ident, len)
		&& !shellfunc[i].ident[len])
			break;
	if (!shellfunc[i].ident) return(0);
	free2(shellfunc[i].ident);
	freestree(shellfunc[i].func);
	free2(shellfunc[i].func);
	for (; shellfunc[i + 1].ident; i++) {
		shellfunc[i].ident = shellfunc[i + 1].ident;
		shellfunc[i].func = shellfunc[i + 1].func;
	}
	shellfunc[i].ident = NULL;
#ifdef	DEP_PTY
	sendparent(TE_DELETEFUNCTION, ident, len);
#endif

	return(0);
}

static int NEAR exec_statement(trp)
syntaxtree *trp;
{
	int id;

	if ((id = getstatid(trp = statementbody(trp))) < 0)
		return(seterrno(EINVAL));

	return((*(statementlist[id].func))(trp));
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
#ifdef	DEP_FILECONV
	char *cp;
	int i;
#endif
#if	!MSDOS
	p_id_t pid;
#endif
	command_t *comm;
	char *path;
	int ret;

	comm = trp -> comm;

#ifdef	DEP_FILECONV
	if (defaultkcode == NOCNV || defaultkcode == DEFCODE) /*EMPTY*/;
	else if (type == CT_FDORIGINAL || type == CT_FDINTERNAL
	|| (type == CT_BUILTIN && (shbuiltinlist[id].flags & BT_FILENAME))) {
		for (i = 1; i < comm -> argc; i++) {
			cp = newkanjiconv(comm -> argv[i],
				defaultkcode, DEFCODE, L_FNAME);
			if (cp != comm -> argv[i]) free2(comm -> argv[i]);
			comm -> argv[i] = cp;
		}
	}
#endif

	if (type == CT_NONE) return(ret_status);
	if (type == CT_BUILTIN) {
		if (!shbuiltinlist[id].func) return(RET_FAIL);
		if (!restricted || !(shbuiltinlist[id].flags & BT_RESTRICT))
			return((*(shbuiltinlist[id].func))(trp));
		execerror(comm -> argv, NULL, ER_RESTRICTED, 0);
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
		execerror(comm -> argv, NULL, ER_INVALDRIVE, 2);
		return(RET_FAIL);
	}
#endif
	if (type == CT_FUNCTION) {
		if (!shellfunc[id].func) return(RET_FAIL);
		return(doshfunc(trp, id));
	}

	if (!(path = evalexternal(trp -> comm))) return(ret_status);
#if	MSDOS
	if ((ret = spawnve(P_WAIT, path, comm -> argv, exportvar)) < 0
	&& errno == ENOENT) {
		execerror(comm -> argv, NULL, ER_COMNOFOUND, 1);
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

	if ((pid = makechild(1, (p_id_t)-1, 0)) < (p_id_t)0) return(-1);
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

	stripquote(tmp = strdup2(argv[0]), EA_STRIPQ);
#ifdef	NOALIAS
	type = checktype(tmp, &id, 1);
#else
	type = checktype(tmp, &id, 0, 1);
#endif
	free2(tmp);
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

	if (substvar(argv, EA_BACKQ | EA_EVALIFS) < 0) ret = -1;
	else {
		argv = checkshellbuiltinargv(argc, argv);
		ret = 0;
	}

	freevar(subst);
	free2(len);
	freevar(argv);
	errno = duperrno;

	return(ret);
}

#if	MSDOS
static int NEAR exec_command(trp, contp)
syntaxtree *trp;
int *contp;
#else
static int NEAR exec_command(trp, contp, bg)
syntaxtree *trp;
int *contp, bg;
#endif
{
#ifndef	_NOUSEHASH
	hashlist **htable;
	char *pathvar;
#endif
	command_t *comm;
	char *ps, **argv, **subst, **svar, **evar;
	u_long esize;
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

	id = evalargv(comm, &type, contp);
	if (id < 0 || (nsubst = substvar(subst, EA_BACKQ)) < 0) {
		freevar(subst);
		free2(len);
		comm -> argc = argc;
		freevar(comm -> argv);
		comm -> argv = argv;
		return(RET_FAIL);
	}
	if (nsubst > 0 && type == CT_NONE) ret_status = RET_SUCCESS;

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
		for (i = 0; subst[i]; i++) free2(subst[i]);
		subst[0] = NULL;
		keepvar = nsubst = 0;
	}

	if (keepvar) {
		shellvar = duplvar(svar = shellvar, 0);
		exportvar = duplvar(evar = exportvar, 0);
		esize = exportsize;
#ifndef	_NOUSEHASH
		htable = duplhash(hashtable);
		pathvar = getconstvar(ENVPATH);
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

#ifdef	MINIMUMSHELL
	ps = PS4STR;
#else	/* !MINIMUMSHELL */
# ifdef	FD
	if ((ps = getconstvar(ENVPS4))) evalprompt(&ps, ps);
# else
	if ((ps = getconstvar(ENVPS4))) ps = evalvararg(ps, EA_BACKQ, 0);
# endif
#endif	/* !MINIMUMSHELL */
	while (nsubst--) {
#ifdef	FD
		demacroarg(&(subst[nsubst]));
#endif
		stripquote(subst[nsubst], EA_STRIPQ);
		if (putshellvar(subst[nsubst], len[nsubst]) < 0) {
			while (nsubst >= 0) free2(subst[nsubst--]);
			free2(subst);
			free2(len);
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
			if (ps) Xfputs(ps, Xstderr);
			argfputs(subst[nsubst], Xstderr);
#else
			argfputs(subst[nsubst], Xstderr);
			if (nsubst) Xfputc(' ', Xstderr);
			else
#endif
			VOID_C fputnl(Xstderr);
		}
	}
	free2(subst);
	free2(len);

	if (verboseexec && comm -> argc) {
		if (ps) Xfputs(ps, Xstderr);
		argfputs(comm -> argv[0], Xstderr);
		for (i = 1; i < comm -> argc; i++)
			fprintf2(Xstderr, " %a", comm -> argv[i]);
		VOID_C fputnl(Xstderr);
	}
#ifndef	MINIMUMSHELL
	free2(ps);
#endif

#if	MSDOS
	ret = exec_simplecom(trp, type, id);
#else
	ret = exec_simplecom(trp, type, id, bg);
#endif

	if (keepvar) {
#ifndef	_NOUSEHASH
		if (pathvar != getconstvar(ENVPATH)) {
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
		internal_status = FNC_FAIL;
#endif

	comm -> argc = argc;
	freevar(comm -> argv);
	comm -> argv = argv;
#if	!MSDOS && defined (FD)
	if (autosavetty) savestdio(0);
#endif

	return(ret);
}

#ifdef	USEFAKEPIPE
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
	int bg, sub, stop;
#endif
#ifndef	MINIMUMSHELL
	long dupshlineno;
#endif
	syntaxtree *next;
	int cont, ret;

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
	sub = stop = 0;
# if	!defined (BASHSTYLE) && !defined (USEFAKEPIPE)
	/* bash does not treat the end of pipeline as sub shell */
	if (!(tmptr -> next) && (tmptr -> flags & ST_NEXT)
	&& (tmptr = getparent(trp)) && isoppipe(tmptr)) {
		sub = 1;
		stop = -1;
	}
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
		if ((pid = makechild(1, (p_id_t)-1, stop)) < (p_id_t)0)
			return(-1);
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
	else if (next) ret = dosetshfunc((trp -> comm) -> argv[0], next);
	else for (;;) {
		cont = 0;
#ifndef	MINIMUMSHELL
		dupshlineno = shlineno;
		setshlineno(trp -> lineno);
#endif
#if	MSDOS
		ret = exec_command(trp, &cont);
#else	/* !MSDOS */
		if (sub) bg = 1;
# ifndef	USEFAKEPIPE
		else if (!pipein) bg = 1;
# endif
# ifndef	NOJOB
		else if (ttypgrp < (p_id_t)0 && !(trp -> next)) bg = 1;
# endif
		else bg = 0;

		ret = exec_command(trp, &cont, bg);
#endif	/* !MSDOS */
#ifndef	MINIMUMSHELL
		setshlineno(dupshlineno);
#endif
		if (ret < 0) break;
#ifdef	FD
		if (cont) continue;
#endif
		break;
	}

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
	char *tmp;
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

	fd = -1;
#if	!MSDOS
	if (!isopbg(trp) && !isopnown(trp)) pid = (p_id_t)-1;
	else if ((pid = makechild(0, (p_id_t)-1, 0)) < (p_id_t)0) return(-1);
	else if (!pid) {
		nownstree(trp -> next);
# ifndef	NOJOB
		ttypgrp = (p_id_t)-1;
		if (!isopnown(trp)) stackjob(mypid, 0, trp);
		if (jobok) /*EMPTY*/;
		else
# endif
		fd = Xopen(_PATH_DEVNULL, O_BINARY | O_RDONLY, 0666);
		if (fd >= 0) {
			Xdup2(fd, STDIN_FILENO);
			safeclose(fd);
		}
	}
	else {
		tmptr = trp -> next;
		trp -> next = NULL;
		nownstree(trp);
		trp -> next = tmptr;
# ifndef	NOJOB
		childpgrp = (p_id_t)-1;
		if (!isopnown(trp)) {
			prevjob = lastjob;
			lastjob = stackjob(pid, 0, trp);
		}
# endif
		if (interactive && !nottyout) {
# ifndef	NOJOB
			if (jobok && !isopnown(trp)) {
				if (mypid == orgpgrp) {
					fprintf2(Xstderr, "[%d] %id",
						lastjob + 1, pid);
					VOID_C fputnl(Xstderr);
				}
			}
			else
# endif	/* !NOJOB */
			{
				fprintf2(Xstderr, "%id", pid);
				VOID_C fputnl(Xstderr);
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
	if (!isoppipe(tmptr)) pipein = (p_id_t)-1;
#ifdef	USEFAKEPIPE
	else if ((fd = openpipe(&pipein, STDIN_FILENO, 0)) < 0) return(-1);
#else
	else if ((fd = openpipe(&pipein, STDIN_FILENO, 0, 1, (p_id_t)-1)) < 0)
		return(-1);
#endif

#ifdef	FD
	replaceargs(NULL, NULL, NULL, 0);
#endif

	if (!(trp -> comm) || pipein > (p_id_t)0) {
#ifndef	NOJOB
		if (pipein > (p_id_t)0) stackjob(pipein, 0, trp);
#endif
		ret = ret_status;
	}
	else if (trp -> flags & ST_NODE)
		ret = exec_stree((syntaxtree *)(trp -> comm), cond);
	else if (!((trp -> comm) -> redp)) {
#ifdef	USEFAKEPIPE
		ret = exec_process(trp);
#else
		ret = exec_process(trp, pipein);
#endif
		ret_status = (ret >= 0) ? ret : RET_FAIL;
	}
	else for (;;) {
		if (!(errp = doredirect((trp -> comm) -> redp)))
#ifdef	USEFAKEPIPE
			ret = exec_process(trp);
#else
			ret = exec_process(trp, pipein);
#endif
		else {
			if (checkshellbuiltin(trp) < 0) /*EMPTY*/;
			else if (errno < 0) /*EMPTY*/;
			else if (errp -> type & MD_HEREDOC)
				doperror(NULL, NULL);
			else {
				if (!(errp -> filename)) tmp = strdup2("-");
				else tmp = evalvararg(errp -> filename,
					EA_BACKQ | EA_STRIPQLATER, 0);
				if (tmp && !*tmp && *(errp -> filename)) {
					free2(tmp);
					tmp = strdup2(errp -> filename);
				}
				if (tmp) {
					if (errno) doperror(NULL, tmp);
					else execerror(NULL,
						tmp, ER_RESTRICTED, 2);
					free2(tmp);
				}
			}
			ret = RET_FAIL;
		}
		closeredirect((trp -> comm) -> redp);
		ret_status = (ret >= 0) ? ret : RET_FAIL;
#ifdef	FD
		if (!errp && ((trp -> comm) -> redp) -> type & MD_REST)
			continue;
#endif
		break;
	}
	trp = tmptr;

	if (returnlevel || ret < 0) /*EMPTY*/;
	else if (pipein >= (p_id_t)0
	&& (fd = reopenpipe(fd, RET_SUCCESS)) < 0) {
		pipein = (p_id_t)-1;
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
		else if (!(trp -> next)) /*EMPTY*/;
		else if (errorexit && !cond && isopfg(trp) && ret_status)
			breaklevel = loopdepth;
		else if (isopand(trp) && ret_status) {
			if (errorexit && !cond) {
				breaklevel = loopdepth;
				ret = RET_SUCCESS;
			}
		}
		else if (isopor(trp) && !ret_status) /*EMPTY*/;
		else ret = exec_stree(trp -> next, cond);
	}
	if (pipein >= (p_id_t)0) closepipe2(fd, -1);
	exectrapcomm();

	return(ret);
}

static syntaxtree *NEAR execline(command, stree, trp, noexit)
CONST char *command;
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
				free2(stree);
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
		free2(stree);
		Xexit2(ret);
	}

	return(NULL);
}

static int NEAR exec_line(command)
CONST char *command;
{
	static syntaxtree *stree = NULL;
	static syntaxtree *trp = NULL;
	int ret;

	if (!command) {
		freestree(stree);
		free2(stree);
		stree = trp = NULL;
		return(0);
	}

	setsignal();
	if (verboseinput && command != (char *)-1) {
		argfputs(command, Xstderr);
		VOID_C fputnl(Xstderr);
	}
	if (!stree) trp = stree = newstree(NULL);

#ifndef	NOJOB
	childpgrp = (p_id_t)-1;
#endif
	if ((trp = execline(command, stree, trp, 0))) ret = trp -> cont;
	else {
		free2(stree);
		stree = trp = NULL;
		ret = 0;
	}
	resetsignal(0);

	return(ret);
}

static int NEAR _dosystem(command)
CONST char *command;
{
	static syntaxtree *trp = NULL;
	int ret;

	if (!command) ret = 0;
	else if (!(trp = analyzeline(command))) ret = RET_SYNTAXERR;
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
		errorexit = tmperrorexit;
	}
	freestree(trp);
	free2(trp);
	trp = NULL;

	return((ret >= 0) ? ret : RET_FAIL);
}

#ifndef	FDSH
int dosystem(command)
CONST char *command;
{
	int ret;

	shellpid = mypid;
# ifndef	NOJOB
	orgpgrp = mypid;
	childpgrp = (p_id_t)-1;
# endif
# ifdef	DEP_PTY
	if (!command) return(shell_loop((isshptymode()) ? 0 : 1));
# else
	if (!command) return(shell_loop(1));
# endif
	setsignal();
	ret = _dosystem(command);
	resetsignal(0);

	return(ret);
}
#endif	/* !FDSH */

static XFILE *NEAR _dopopen(command)
CONST char *command;
{
#ifdef	CYGWIN
	struct timeval tv;
#endif
	syntaxtree *trp;
	p_id_t pipein;
	int fd;

	trp = (command) ? analyzeline(command) : NULL;

	nottyout++;
#ifdef	USEFAKEPIPE
	if ((fd = openpipe(&pipein, STDIN_FILENO, 1)) < 0) /*EMPTY*/;
#else
	if ((fd = openpipe(&pipein, STDIN_FILENO, 1, 1, mypid)) < 0) /*EMPTY*/;
#endif
	else if (pipein) {
#ifdef	CYGWIN
	/* a trick for buggy terminal emulation */
		tv.tv_sec = 0;
		tv.tv_usec = 500000L;	/* maybe enough waiting limit */
		VOID_C readselect(1, &fd, NULL, &tv);
#endif
	}
	else {
		nownstree(trp);
		if (!trp) ret_status = RET_SYNTAXERR;
		else if (!(trp -> comm)) ret_status = RET_SUCCESS;
		else if (notexec) {
#ifdef	SHOWSTREE
			show_stree(trp, 0);
#endif
			pipein = (p_id_t)-1;
		}
#ifndef	_NOUSEHASH
		else if (hashahead && check_stree(trp) < 0)
			pipein = (p_id_t)-1;
#endif
		else if (exec_stree(trp, 0) < 0) pipein = (p_id_t)-1;

		searchheredoc(trp, 1);
		if (pipein >= (p_id_t)0) fd = reopenpipe(fd, ret_status);
		else {
			closepipe2(fd, -1);
			fd = -1;
		}
	}

	if (trp) {
		freestree(trp);
		free2(trp);
	}
	errorexit = tmperrorexit;
	nottyout--;

	return((fd >= 0) ? fdopenpipe(fd) : NULL);
}

#ifndef	FDSH
XFILE *dopopen(command)
CONST char *command;
{
	XFILE *fp;

	shellpid = mypid;
# ifndef	NOJOB
	orgpgrp = mypid;
	childpgrp = (p_id_t)-1;
# endif
	setsignal();
	fp = _dopopen(command);
	resetsignal(0);

	return(fp);
}

int dopclose(fp)
XFILE *fp;
{
	return(closepipe2(Xfileno(fp), -1));
}
#endif	/* !FDSH */

static int NEAR sourcefile(fd, fname, verbose)
int fd;
CONST char *fname;
int verbose;
{
#ifndef	MINIMUMSHELL
	long dupshlineno;
#endif
	syntaxtree *stree, *trp;
	char *buf;
	long n;
	int ret, dupinteractive;

	dupinteractive = interactive;
	interactive = 0;
#ifndef	MINIMUMSHELL
	dupshlineno = shlineno;
	setshlineno(1L);
#endif
	trp = stree = newstree(NULL);
	n = 0L;
	ret = errno = 0;
#if	!MSDOS
	closeonexec(fd);
#endif
	while ((buf = readline(fd, '\0')) != vnullstr) {
		if (!buf) {
			if (errno != EINTR) doperror(NULL, fname);
			ret++;
			ret_status = RET_FAIL;
			break;
		}

		if (!trp) trp = stree;
		trp = execline(buf, stree, trp, 1);
		n++;
		if (ret_status == RET_NOTICE || syntaxerrno || execerrno) {
			ret++;
			if (verbose) {
				fprintf2(Xstderr, "%k:%ld: %s", fname, n, buf);
				VOID_C fputnl(Xstderr);
			}
			ret_status = RET_FAIL;
		}
		free2(buf);
		if (syntaxerrno) break;
#ifndef	BASHSTYLE
		if (execerrno) break;
#endif
#ifndef	MINIMUMSHELL
		setshlineno(shlineno + 1L);
#endif
	}

	execline((char *)-1, stree, trp, 1);
	if (syntaxerrno) ret_status = RET_SYNTAXERR;
	free2(stree);
	interactive = dupinteractive;
#ifndef	MINIMUMSHELL
	setshlineno(dupshlineno);
#endif

	return(ret);
}

int execruncom(fname, verbose)
CONST char *fname;
int verbose;
{
#ifdef	MINIMUMSHELL
	char *cp, path[MAXPATHLEN];
#endif
	lockbuf_t *lck;
	char *new, **dupargvar;
	int ret, duprestricted;

	setsignal();
#ifdef	MINIMUMSHELL
	if (*fname == '~' && fname[1] == _SC_ && (cp = getconstvar(ENVHOME))) {
		strcatdelim2(path, cp, &(fname[2]));
		fname = path;
	}
#endif
	new = strdup2(fname);
#if	MSDOS && !defined (BSPATHDELIM)
	new = adjustpname(new);
#endif
	fname = new = evalpath(new, 0);
	ret = RET_SUCCESS;
	if (noruncom || !isrootdir(fname)) lck = NULL;
	else if (!(lck = lockopen(fname, O_TEXT | O_RDONLY, 0666))) {
		doperror(NULL, fname);
		ret = RET_FAIL;
	}
	else if (lck -> fd >= 0) {
#ifdef	FD
		inruncom = 1;
#endif
		duprestricted = restricted;
		restricted = 0;
		dupargvar = argvar;
		argvar = (char **)malloc2(2 * sizeof(char *));
		argvar[0] = strdup2(fname);
		argvar[1] = NULL;
		ret = sourcefile(lck -> fd, fname, verbose);
		freevar(argvar);
		argvar = dupargvar;
		restricted = duprestricted;
#ifdef	FD
		inruncom = 0;
#endif
	}
	lockclose(lck);
	resetsignal(0);
	free2(new);

	return(ret);
}

#if	MSDOS && !defined (BSPATHDELIM)
static VOID NEAR adjustdelim(var)
char *CONST *var;
{
	char *cp;
	int i;

	if (!var) return;
	for (i = 0; var[i]; i++) {
		if (!(cp = strchr2(var[i], '='))) continue;
		if (searchvar2(ADJUSTVARSIZ, adjustvar, var[i], cp - var[i]))
			adjustpname(cp);
	}
}
#endif	/* MSDOS && !BSPATHDELIM */

VOID setshellvar(envp)
char *CONST *envp;
{
	char *cp;
	int i, len;

	shellvar = duplvar(envp, 0);
	exportvar = duplvar(envp, 0);
#if	MSDOS && !defined (BSPATHDELIM)
	adjustdelim(shellvar);
	adjustdelim(exportvar);
#endif
	exportsize = (u_long)0;
	for (i = 0; exportvar[i]; i++)
		exportsize += (u_long)strlen(exportvar[i]) + 1;
	exportlist = (char **)malloc2((i + 1) * sizeof(char *));
	for (i = 0; exportvar[i]; i++) {
		len = ((cp = strchr2(exportvar[i], '=')))
			? cp - exportvar[i] : strlen(exportvar[i]);
		exportlist[i] = strndup2(exportvar[i], len);
	}
	exportlist[i] = NULL;
	ronlylist = duplvar(NULL, 0);
}

int prepareterm(VOID_A)
{
#ifdef	FD
	CONST char *term;
#endif

	if (ttyio >= 0 && ttyout) return(0);

	if (opentty(&ttyio, &ttyout) < 0) {
		closetty(&ttyio, &ttyout);
		nottyout++;
		if (interactive) return(-1);
	}

#ifndef	NOJOB
	if (!interactive) oldttypgrp = (p_id_t)-1;
	else {
		gettcpgrp(ttyio, &oldttypgrp);
		if (oldttypgrp < (p_id_t)0) {
			closetty(&ttyio, &ttyout);
			if ((ttyio = newdup(Xdup(STDIN_FILENO))) < 0
			|| opentty(&ttyio, &ttyout) < 0) {
				closetty(&ttyio, &ttyout);
				return(-1);
			}
		}
	}
#endif	/* !NOJOB */

#ifdef	FD
	if (ttyio >= 0) inittty(0);
	if (!(term = getconstvar(ENVTERM))) term = nullstr;
	getterment(term);
#endif

	return(0);
}

static VOID NEAR initrc(verbose)
int verbose;
{
#if	!defined (MINIMUMSHELL) \
|| (MSDOS && defined (FD) && !defined (BSPATHDELIM))
	CONST char *cp;
#endif

#ifdef	FD
	execruncom(DEFRC, verbose);
#else
	if (loginshell) {
		execruncom(LSH_DEFRC, verbose);
		execruncom(LSH_RCFILE, verbose);
	}
#endif
#ifndef	MINIMUMSHELL
# if	!MSDOS
	if (getuid() != geteuid() || getgid() != getegid()) /*EMPTY*/;
	else
# endif
	if ((cp = getconstvar(ENVENV))) execruncom(cp, verbose);
#endif	/* !MINIMUMSHELL */
#ifdef	FD
	if (loginshell) execruncom(SH_RCFILE, verbose);
	else execruncom(FD_RCFILE, verbose);
# ifdef	DEP_PTY
	if (interactive) shptymode = tmpshptymode;
# endif
#endif	/* FD */
}

int initshell(argc, argv)
int argc;
char *CONST *argv;
{
#if	!MSDOS
# if	!defined (NOJOB) && defined (NTTYDISC) && defined (ldisc)
	ldiscioctl_t tty;
# endif
# ifndef	MINIMUMSHELL
	char buf[MAXLONGWIDTH + 1];
# endif
	sigmask_t mask;
#endif	/* !MSDOS */
	CONST char *cp, *cp2;
	char *tmp;
	int i, n, isstdin, tmprestricted;

	shellname = argv[0];
	getshellname(getbasename(shellname), &loginshell, &tmprestricted);

	definput = STDIN_FILENO;
	if (!interactive && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
		interactive = 1;
	orgpid = shellpid = mypid = getpid();
#ifndef	NOJOB
	ttypgrp = mypid;
	if ((orgpgrp = getpgroup()) < (p_id_t)0) {
		doperror(NULL, NULL);
		return(-1);
	}
#endif

	if ((n = getoption(argc, argv, 1)) < 0) return(-1);
#if	!defined (MINIMUMSHELL) && defined (BASHSTYLE)
	noclobber = 0;
#endif
	errorexit = tmperrorexit;
#ifdef	DEP_PTY
	shptymode = tmpshptymode;
#endif
	isstdin = forcedstdin;

	if (n > 2) {
		shellmode = 1;
		interactive = interactive_io;
	}
	else if (n >= argc || isstdin) isstdin = 1;
	else {
		definput = newdup(Xopen(argv[n], O_BINARY | O_RDONLY, 0666));
		if (definput < 0) {
			fprintf2(Xstderr, "%k: cannot open", argv[n]);
			VOID_C fputnl(Xstderr);
			return(-1);
		}
		if (!interactive_io || !isatty(definput)) {
			interactive = 0;
#if	!MSDOS
			closeonexec(definput);
#endif
		}
		else {
			if (Xdup2(definput, STDIN_FILENO) < 0) {
				fprintf2(Xstderr, "%k: cannot open", argv[n]);
				VOID_C fputnl(Xstderr);
				return(-1);
			}
			safeclose(definput);
			definput = STDIN_FILENO;
		}
	}
	interactive_io = interactive;

	if (prepareterm() < 0) {
		doperror(NULL, NULL);
		return(-1);
	}
#if	!MSDOS && defined (FD)
	if (autosavetty) savestdio(0);
#endif

	Xsetbuf(Xstdin);
	Xsetlinebuf(Xstderr);
	Xsetlinebuf(Xstdout);
#if	!MSDOS
	Xsigemptyset(mask);
	Xsigblock(oldsigmask, mask);
#endif	/* !MSDOS */

	if (ttyio >= 0 && definput == STDIN_FILENO && isatty(STDIN_FILENO))
		definput = ttyio;

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
	shellfunc = (shfunctable *)malloc2(1 * sizeof(shfunctable));
	shellfunc[0].ident = NULL;
#ifndef	NOALIAS
	shellalias = (shaliastable *)malloc2(1 * sizeof(shaliastable));
	shellalias[0].ident = NULL;
#endif
#ifndef	_NOUSEHASH
	searchhash(NULL, NULL, NULL);
#endif
	if ((!interactive || isstdin) && n < argc) {
		argvar = (char **)malloc2((argc - n + 1 + isstdin)
			* sizeof(char *));
		for (i = 0; i < argc - n + 1; i++)
			argvar[i + isstdin] = strdup2(argv[i + n]);
		if (isstdin) argvar[0] = strdup2(argv[0]);
	}
	else {
		argvar = (char **)malloc2(2 * sizeof(char *));
		argvar[0] = strdup2(argv[0]);
		argvar[1] = NULL;
	}

	if (!(tmp = getconstvar(ENVPATH))) setenv2(ENVPATH, DEFPATH, 1);
#if	MSDOS
	else for (i = 0; tmp[i]; i++) if (tmp[i] == ';') tmp[i] = PATHDELIM;
#else
	if (!(tmp = getconstvar(ENVTERM))) setenv2(ENVTERM, DEFTERM, 1);
#endif

	if (interactive) {
		if (getconstvar(ENVPS1)) /*EMPTY*/;
#if	!MSDOS
		else if (!getuid()) setenv2(ENVPS1, PS1ROOT, 0);
#endif
		else setenv2(ENVPS1, PS1STR, 0);
		if (!getconstvar(ENVPS2)) setenv2(ENVPS2, PS2STR, 0);
	}
	else {
		for (i = 0; i < PRIMALVARSIZ; i++)
			if (!strenvcmp(primalvar[i], ENVPS1)
			|| !strenvcmp(primalvar[i], ENVPS2))
				primalvar[i] = NULL;
		unset(ENVPS1, strsize(ENVPS1));
		unset(ENVPS2, strsize(ENVPS2));
	}
#ifndef	MINIMUMSHELL
	if (!getconstvar(ENVPS4)) setenv2(ENVPS4, PS4STR, 0);
	setshlineno(1L);
# if	!MSDOS
	snprintf2(buf, sizeof(buf), "%id", getppid());
	setenv2(ENVPPID, buf, 0);
# endif
#endif
	setenv2(ENVIFS, IFS_SET, 0);
	if (loginshell) {
#ifndef	NOUID
		getlogininfo(&cp, &cp2);
#endif
		if (!getconstvar(ENVHOME)) {
#ifndef	NOUID
			if (cp) /*EMPTY*/;
			else
#endif
			cp = rootpath;
			setenv2(ENVHOME, cp, 1);
		}
		if (!getconstvar(ENVSHELL)) {
#ifndef	NOUID
			if (cp2) /*EMPTY*/;
			else
#endif
			cp2 = shellname;
			setenv2(ENVSHELL, cp2, 1);
		}
	}
#if	!MSDOS && !defined (MINIMUMSHELL)
# ifdef	BASHSTYLE
	if (!getconstvar(ENVMAILCHECK)) setenv2(ENVMAILCHECK, "60", 0);
# else
	if (!getconstvar(ENVMAILCHECK)) setenv2(ENVMAILCHECK, "600", 0);
# endif
	if ((cp = getconstvar(ENVMAILPATH))) replacemailpath(cp, 1);
	else if ((cp = getconstvar(ENVMAIL))) replacemailpath(cp, 0);
#endif	/* !MSDOS && !MINIMUMSHELL */
#ifndef	NOPOSIXUTIL
	setenv2(ENVOPTIND, "1", 0);
#endif

	for (i = 0; i < NSIG; i++) {
		trapmode[i] = 0;
		trapcomm[i] = NULL;
		if ((oldsigfunc[i] = (sigcst_t)signal2(i, SIG_DFL)) == SIG_ERR)
			oldsigfunc[i] = SIG_DFL;
		else signal2(i, oldsigfunc[i]);
	}
	for (i = 0; signallist[i].sig >= 0; i++)
		trapmode[signallist[i].sig] = signallist[i].flags;
#ifndef	NOJOB
	if (!interactive || ttyio < 0) jobok = 0;
	else {
		if (!orgpgrp) {
			orgpgrp = mypid;
			if (setpgroup(0, mypid) < (p_id_t)0
			|| settcpgrp(ttyio, orgpgrp) < 0) {
				doperror(NULL, NULL);
				return(-1);
			}
		}
		gettcpgrp(ttyio, &ttypgrp);
# ifdef	SIGTTIN
		while (ttypgrp >= (p_id_t)0 && ttypgrp != orgpgrp) {
			signal2(SIGTTIN, SIG_DFL);
			VOID_C kill(0, SIGTTIN);
			signal2(SIGTTIN, oldsigfunc[SIGTTIN]);
			gettcpgrp(ttyio, &ttypgrp);
		}
# endif
# if	defined (NTTYDISC) && defined (ldisc)
		if (tioctl(ttyio, REQGETD, &tty) < 0) {
			doperror(NULL, NULL);
			return(-1);
		}
		else if (ldisc(tty) != NTTYDISC && (ldisc(tty) = NTTYDISC) > 0
		&& tioctl(ttyio, REQSETD, &tty) < 0) {
#  ifdef	USESGTTY
			doperror(NULL, NULL);
			return(-1);
#  else	/* !USESGTTY */
#   ifdef	ENODEV
			if (errno == ENODEV) /*EMPTY*/;
			else
#   endif
			jobok = 0;
#  endif	/* !USESGTTY */
		}
# endif	/* NTTYDISC && ldisc */
		if (orgpgrp != mypid) {
			orgpgrp = mypid;
			if (setpgroup(0, mypid) < (p_id_t)0
			|| (mypid != ttypgrp && gettermio(orgpgrp, 1) < 0)) {
				doperror(NULL, NULL);
				return(-1);
			}
		}

		if (jobok < 0) jobok = 1;
	}
#endif	/* !NOJOB */
#if	!MSDOS
	closeonexec(ttyio);
#endif

	if (n > 2) {
#ifdef	FD
		initenv();
# ifdef	DEP_PTY
		if (isshptymode()) checkscreen(0, 0);
# endif
# ifndef	_NOCUSTOMIZE
		saveorigenviron();
# endif
#endif	/* FD */
		initrc(0);
#ifdef	FD
		initfd(argv);
#endif
		if (verboseinput) {
			kanjifputs(argv[2], Xstderr);
#ifdef	BASHSTYLE
	/* bash displays a newline with single string command, in -v mode */
			VOID_C fputnl(Xstderr);
#endif
			Xfflush(Xstderr);
		}
#ifdef	DEP_PTY
		if (isshptymode()) {
			shellpid = (p_id_t)-1;
			n = ptymacro(argv[2], NULL, F_DOSYSTEM);
		}
		else
#endif
		{
			setsignal();
			n = _dosystem(argv[2]);
			resetsignal(0);
		}
		Xexit2(n);
	}

	n = 0;
#ifdef	BASHSTYLE
	/* bash changes the current directory if the variable PWD is set */
	if ((cp = getconstvar(ENVPWD))) n = chdir2(cp);
	else
#endif
	if (loginshell && interactive) n = chdir2(getconstvar(ENVHOME));
	if (n < 0) {
#ifdef	FD
		initfd(argv);
#endif
		Xfputs("No directory", Xstderr);
		VOID_C fputnl(Xstderr);
		Xexit2(RET_FAIL);
	}

	if (!restricted) restricted = tmprestricted;
	if (!restricted) {
#ifdef	FD
		cp = getenv2("FD_SHELL");
#else
		cp = getconstvar(ENVSHELL);
#endif
		if (cp) getshellname(getbasename(cp), NULL, &restricted);
	}
#ifdef	FD
	fd_restricted = restricted;
#endif

	return(0);
}

int shell_loop(pseudoexit)
int pseudoexit;
{
#ifdef	DEP_FILECONV
	char *cp;
#endif
	char *ps, *buf;
	int cont;

	shellmode = 1;
	setsignal();
	cont = 0;
	if (pseudoexit) exit_status = -1;
	for (;;) {
		if (trapok >= 0) trapok = 1;
		if (!interactive) buf = readline(definput, '\0');
		else {
#if	!MSDOS
			checkmail(0);
#endif
			ps = (cont)
				? getconstvar(ENVPS2) : getconstvar(ENVPS1);
#ifdef	FD
			if (dumbterm > 1) {
				evalprompt(&buf, ps);
				kanjifputs(buf, Xstderr);
				free2(buf);
				Xfflush(Xstderr);
				buf = readline(definput, '\0');
			}
			else {
				ttyiomode(1);
				buf = inputshellstr(ps, -1, NULL);
				stdiomode();
				if (!buf) continue;
			}
#else	/* !FD */
			kanjifputs(ps, Xstderr);
			Xfflush(Xstderr);
			buf = readline(definput, '\0');
#endif	/* !FD */
		}
		if (trapok >= 0) trapok = 0;
		if (!buf) {
			if (errno != EINTR) doperror(NULL, "stdin");
			exec_line(NULL);
			break;
		}
		if (buf == vnullstr) {
#ifndef	MINIMUMSHELL
			if (interactive && ignoreeof) {
				if (cont) {
					exec_line(NULL);
					syntaxerrno = ER_UNEXPEOF;
					syntaxerror(nullstr);
				}
				else {
					fprintf2(Xstderr,
					"Use \"%s\" to leave to the shell.",
						(loginshell)
							? "logout" : "exit");
					VOID_C fputnl(Xstderr);
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

#ifdef	DEP_FILECONV
		cp = newkanjiconv(buf, DEFCODE, defaultkcode, L_FNAME);
		if (cp != buf) free2(buf);
		buf = cp;
#endif
		cont = exec_line(buf);
		free2(buf);
		if (pseudoexit && exit_status >= 0) break;
#ifndef	MINIMUMSHELL
		setshlineno(shlineno + 1L);
#endif
	}
	resetsignal(0);
	if (exit_status >= 0) ret_status = exit_status;
	shellmode = 0;

	return(ret_status);
}

int main_shell(argc, argv, envp)
int argc;
char *CONST *argv, *CONST *envp;
{
	shellmode = 1;
	setshellvar(envp);
	if (initshell(argc, argv) < 0) return(RET_FAIL);
#ifdef	FD
	initenv();
# if	MSDOS
	inittty(1);
# endif
	if (interactive) checkscreen(0, 0);
# ifndef	_NOCUSTOMIZE
	saveorigenviron();
# endif
#endif	/* FD */
	ret_status = RET_SUCCESS;
	initrc(!loginshell);
#ifdef	FD
	initfd(argv);
#endif
#ifdef	DEP_PTY
	if (isshptymode()) {
		shellpid = (p_id_t)-1;
		ret_status = ptymacro(NULL, NULL, F_DOSYSTEM);
	}
	else
#endif
	shell_loop(0);

	return(ret_status);
}

#ifdef	FDSH
int main(argc, argv, envp)
int argc;
char *CONST argv[], *CONST envp[];
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
#endif	/* DEP_ORIGSHELL */
