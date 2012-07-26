/*
 *	system.h
 *
 *	definitions & function prototype declarations for "system.c"
 */

#ifdef	HPUX
/* for TIOCGPGRP & TIOCSPGRP */
#include <bsdtty.h>
#endif

#include "depend.h"
#include "stream.h"

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
/* #define WITHNETWORK		; support network extensions */
/* #define WITHSOCKET		; support network extensions with socket */
/* #define WITHSOCKREDIR	; support network redirections with socket */

#if	defined (BASHSTYLE) && !defined (BASHBUG)
#define	BASHBUG
#endif
#if	(defined (NOSETPGRP) || defined (MINIMUMSHELL)) && !defined (NOJOB)
#define	NOJOB
#endif
#if	defined (MINIMUMSHELL) && !defined (NOALIAS)
#define	NOALIAS
#endif
#if	(MSDOS \
|| ((defined (FD) || defined (WITHNETWORK)) && !defined (_NODOSCOMMAND))) \
&& !defined (DOSCOMMAND)
#define	DOSCOMMAND
#endif
#if	MSDOS && !defined (USEFAKEPIPE)
#define	USEFAKEPIPE
#endif
#if	defined (MINIMUMSHELL) && !defined (NOPOSIXUTIL)
#define	NOPOSIXUTIL
#endif
#if	defined (DEP_SOCKET) && !defined (WITHSOCKET)
#define	WITHSOCKET
#endif
#if	defined (DEP_SOCKREDIR) && !defined (WITHSOCKREDIR)
#define	WITHSOCKREDIR
#endif

#include "pathname.h"
#include "termio.h"
#include "wait.h"

#ifdef	BASHSTYLE
#define	ERRBREAK		continue
#else
#define	ERRBREAK		break
#endif

#define	RET_SUCCESS		0
#define	RET_FAIL		1
#define	RET_SYNTAXERR		2
#define	RET_FATALERR		2
#ifdef	SIGINT
#define	RET_INTR		(SIGINT + 128)
#else
#define	RET_INTR		(2 + 128)
#endif
#define	RET_NOTEXEC		126
#define	RET_NOTFOUND		127
#define	RET_NOTICE		255
#define	RET_NULSYSTEM		256
#define	READ_EOF		0x100
#define	ENVPS1			"PS1"
#define	ENVPS2			"PS2"
#define	ENVPS4			"PS4"
#define	ENVMAIL			"MAIL"
#define	ENVMAILPATH		"MAILPATH"
#define	ENVMAILCHECK		"MAILCHECK"
#define	ENVLINENO		"LINENO"
#define	ENVENV			"ENV"
#define	ENVCDPATH		"CDPATH"
#define	ENVPPID			"PPID"
#define	ENVOPTARG		"OPTARG"
#define	ENVOPTIND		"OPTIND"
#define	ENVSHELL		"SHELL"
#define	ENVCOMSPEC		"COMSPEC"
#define	ENVREPLY		"REPLY"

#if	defined (TIOCGPGRP) && !defined (USETCGETPGRP)
#define	gettcpgrp(f, gp)	((ioctl(f, TIOCGPGRP, gp) < 0) \
				? (*(gp) = (p_id_t)-1) : *(gp))
#else
#define	gettcpgrp(f, gp)	(*(gp) = tcgetpgrp(f))
#endif
#if	defined (TIOCSPGRP) && !defined (USETCGETPGRP)
#define	settcpgrp(f, g)		ioctl(f, TIOCSPGRP, &(g))
#else
#define	settcpgrp		tcsetpgrp
#endif

#if	MSDOS
#define	Xexit			exit
#define	DEFPATH			":"
# ifndef	_PATH_DEVNULL
# define	_PATH_DEVNULL	"NULL"
# endif
#else
#define	Xexit			_exit
#define	DEFPATH			":/bin:/usr/bin"
#define	DEFTERM			"dumb"
# ifndef	_PATH_DEVNULL
# define	_PATH_DEVNULL	"/dev/null"
# endif
#endif

typedef struct _heredoc_t {
	char *eof;
	char *filename;
	char *buf;
	int fd;
#ifndef	USEFAKEPIPE
	p_id_t pipein;
#endif
	u_char flags;
} heredoc_t;

#define	HD_IGNORETAB		0001
#define	HD_QUOTED		0002

typedef struct _redirectlist {
	int fd;
	char *filename;
	u_short type;
	int new;
	int old;
#ifdef	DEP_DOSDRIVE
	char *fakepipe;
	int dosfd;
#endif
	struct _redirectlist *next;
} redirectlist;

#define	MD_NORMAL		0000
#define	MD_READ			0001
#define	MD_WRITE		0002
#define	MD_RDWR			0003
#define	MD_APPEND		0004
#define	MD_FILEDESC		0010
#define	MD_WITHERR		0020
#define	MD_HEREDOC		0040
#define	MD_FORCED		0100
#define	MD_REST			0200
#define	MD_DUPL			0400

typedef struct _command_t {
	hashlist *hash;
	int argc;
	char **argv;
	redirectlist *redp;
	u_char type;
	u_char id;
} command_t;

#define	CT_STATEMENT		0001
#define	CT_NONE			0002
#define	CT_BUILTIN		0003
#define	CT_COMMAND		0004
#define	CT_FUNCTION		0005
#define	CT_ALIAS		0006
#define	CT_LOGDRIVE		0007
#ifdef	FD
#define	CT_FDORIGINAL		0100
#define	CT_FDINTERNAL		0200
#endif

#define	SM_IF			0001
#define	SM_THEN			0002
#define	SM_ELIF			0003
#define	SM_ELSE			0004
#define	SM_FI			0005
#define	SM_WHILE		0006
#define	SM_UNTIL		0007
#define	SM_DO			0010
#define	SM_DONE			0011
#define	SM_FOR			0012
#define	SM_IN			0013
#define	SM_CASE			0014
#define	SM_INCASE		0015
#define	SM_RPAREN		0016
#define	SM_CASEEND		0017
#define	SM_ESAC			0020
#define	SM_LPAREN		0021
#define	SM_FUNC			0022
#define	SM_LIST			0023
#define	SM_LISTEND		0024
#define	SM_ANOTHER		0075
#define	SM_CHILD		0076
#define	SM_STATEMENT		0077

#define	isstatement(comm)	((comm) && (comm) -> type == CT_STATEMENT)
#define	notstatement(comm)	((comm) && (comm) -> type != CT_STATEMENT)
#define	isbuiltin(comm)		((comm) && (comm) -> type == CT_BUILTIN)
#define	iscommand(comm)		((comm) && (comm) -> type == CT_COMMAND)
#define	ischild(comm)		((comm) && (comm) -> type == CT_STATEMENT \
				&& (comm) -> id == SM_CHILD)

typedef struct _syntaxtree {
	command_t *comm;
	struct _syntaxtree *parent;
	struct _syntaxtree *next;
#ifndef	MINIMUMSHELL
	long lineno;
#endif
	u_char type;
	u_char cont;
	u_char flags;
} syntaxtree;

#define	OP_NONE			0
#define	OP_FG			1
#define	OP_BG			2
#define	OP_AND			3
#define	OP_OR			4
#define	OP_PIPE			5
#define	OP_NOT			6
#define	OP_NOWN			7

#define	CN_ESCAPE		0001
#define	CN_QUOT			0002
#define	CN_STAT			0004
#define	CN_INHR			0170
#define	CN_SBST			0070
#define	CN_VAR			0010
#define	CN_COMM			0020
#define	CN_EXPR			0030
#define	CN_CASE			0040
#define	CN_HDOC			0100

#define	ST_NODE			0001
#define	ST_NEXT			0002
#define	ST_TOP			0004
#define	ST_NOWN			0010
#define	ST_BUSY			0020
#define	ST_HDOC			0040

#ifdef	MINIMUMSHELL
#define	hasparent(trp)		((trp) -> parent)
#define	getparent(trp)		((trp) -> parent)
#else
#define	hasparent(trp)		(!((trp) -> flags & ST_TOP) && (trp) -> parent)
#define	getparent(trp)		(((trp) -> flags & ST_TOP) \
				? NULL : (trp) -> parent)
#endif
#define	statementbody(trp)	((syntaxtree *)(((trp) -> comm) -> argv))
#define	hascomm(trp)		((trp) -> comm && ((trp) -> comm) -> argc >= 0)
#define	isopfg(trp)		((trp) -> type == OP_FG)
#define	isopbg(trp)		((trp) -> type == OP_BG)
#define	isopand(trp)		((trp) -> type == OP_AND)
#define	isopor(trp)		((trp) -> type == OP_OR)
#define	isoppipe(trp)		((trp) -> type == OP_PIPE)
#ifdef	MINIMUMSHELL
#define	isopnot(trp)		(0)
#define	isopnown(trp)		(0)
#else
#define	isopnot(trp)		((trp) -> type == OP_NOT)
#define	isopnown(trp)		((trp) -> type == OP_NOWN)
#endif

typedef struct _shbuiltintable {
	int (NEAR *func)__P_((syntaxtree *));
	CONST char *ident;
	u_char flags;
} shbuiltintable;

#define	BT_NOGLOB		0001
#define	BT_RESTRICT		0002
#define	BT_POSIXSPECIAL		0004
#define	BT_NOKANJIFGET		0010
#define	BT_DISABLE		0020
#define	BT_FILENAME		0040

#define	SMPREV			4
typedef struct _statementtable {
	int (NEAR *func)__P_((syntaxtree *));
	CONST char *ident;
	u_char type;
	u_char prev[SMPREV];
} statementtable;

#define	STT_TYPE		0017
#define	STT_FOR			0001
#define	STT_CASE		0002
#define	STT_IN			0003
#define	STT_INCASE		0004
#define	STT_CASEEND		0005
#define	STT_LIST		0006
#define	STT_LPAREN		0007
#define	STT_FUNC		0010
#define	STT_NEEDLIST		0020
#define	STT_NEEDIDENT		0040
#define	STT_NEEDNONE		0100
#define	STT_NEEDCOMM		0200

typedef struct _opetable {
	u_char op;
	u_char level;
	CONST char *symbol;
} opetable;

typedef struct _pipelist {
	char *file;
	XFILE *fp;
#ifndef	USEFAKEPIPE
	syntaxtree *trp;
#endif
	int fd;
	int new;
	int old;
	int ret;
	p_id_t pid;
	struct _pipelist *next;
} pipelist;

#ifndef	NOJOB
typedef struct _jobtable {
	p_id_t *pids;
	int *stats;
	int *fds;
	int npipe;
	syntaxtree *trp;
	char *tty;
} jobtable;
#endif	/* !NOJOB */

typedef struct _shfunctable {
	char *ident;
	syntaxtree *func;
} shfunctable;

typedef struct _shaliastable {
	char *ident;
	char *comm;
	u_char flags;
} shaliastable;

#define	AL_USED			0001

typedef struct _signaltable {
	int sig;
	int (*func)__P_((VOID_A));
	CONST char *ident;
	CONST char *mes;
	u_char flags;
} signaltable;

#define	TR_STAT			0007
#define	TR_IGN			0001
#define	TR_TERM			0002
#define	TR_STOP			0003
#define	TR_TRAP			0004
#define	TR_BLOCK		0010
#define	TR_NOTRAP		0020
#define	TR_CATCH		0040
#define	TR_READBL		0100

typedef struct _ulimittable {
	u_char opt;
	int res;
	int unit;
	CONST char *mes;
} ulimittable;

typedef struct _shflagtable {
	CONST char *ident;
	int *var;
	u_char letter;
} shflagtable;

extern int shellmode;
extern p_id_t mypid;
extern p_id_t orgpid;
extern p_id_t shellpid;
extern int ret_status;
extern int interactive;
extern int errorexit;
#ifndef	NOJOB
extern int lastjob;
extern int prevjob;
extern int stopped;
extern p_id_t orgpgrp;
extern p_id_t childpgrp;
extern p_id_t ttypgrp;
#endif
extern int interrupted;
extern int nottyout;
extern int syntaxerrno;
#if	!MSDOS
extern int sigconted;
#endif

extern int verboseexec;
extern int notexec;
extern int verboseinput;
extern int terminated;
extern int forcedstdin;
extern int interactive_io;
extern int tmperrorexit;
extern int restricted;
extern int freeenviron;
extern int undeferror;
extern int hashahead;
extern int noglob;
extern int autoexport;
#ifndef	MINIMUMSHELL
extern int noclobber;
extern int ignoreeof;
#endif
#ifndef	NOJOB
extern int bgnotify;
extern int jobok;
#endif
#ifdef	FD
# ifndef	_NOEDITMODE
extern int emacsmode;
extern int vimode;
# endif
# ifdef	DEP_PTY
extern int shptymode;
# endif
# if	!MSDOS
extern int autosavetty;
# endif
#endif	/* FD */
extern int loginshell;
extern int noruncom;

extern CONST statementtable statementlist[];
extern CONST signaltable signallist[];

#ifndef	MINIMUMSHELL
extern int catchsignal __P_((int));
#endif
extern VOID prepareexit __P_((int));
extern VOID Xexit2 __P_((int));
extern VOID execerror __P_((char *CONST *, CONST char *, int, int));
extern VOID doperror __P_((CONST char *, CONST char *));
extern int isnumeric __P_((CONST char *));
#if	!MSDOS
extern VOID dispsignal __P_((int, int, XFILE *));
extern int waitjob __P_((p_id_t, wait_pid_t *, int));
extern int waitchild __P_((p_id_t, syntaxtree *));
#endif
extern VOID setshflag __P_((int, int));
extern char *evalvararg __P_((char *, int, int));
extern VOID freeheredoc __P_((heredoc_t *, int));
extern VOID freerlist __P_((redirectlist *, int));
extern VOID freecomm __P_((command_t *, int));
extern syntaxtree *newstree __P_((syntaxtree *));
extern VOID freestree __P_((syntaxtree *));
extern syntaxtree *parentstree __P_((syntaxtree *));
#if	!MSDOS
extern VOID cmpmail __P_((CONST char *, CONST char *, time_t *));
#endif
extern int identcheck __P_((CONST char *, int));
extern char *getshellvar __P_((CONST char *, int));
extern int putexportvar __P_((char *, int));
extern int putshellvar __P_((char *, int));
extern int unset __P_((CONST char *, int));
#ifdef	MINIMUMSHELL
extern syntaxtree * duplstree __P_((syntaxtree *, syntaxtree *));
#define	duplstree2(tr, p, t)	duplstree(tr, p)
#else
extern syntaxtree * duplstree __P_((syntaxtree *, syntaxtree *, long));
#define	duplstree2		duplstree
#endif
extern int getstatid __P_((syntaxtree *trp));
#if	defined (BASHSTYLE) || !defined (MINIMUMSHELL)
extern syntaxtree *startvar __P_((syntaxtree *, redirectlist *,
		CONST char *, int *, int *, int));
#endif
extern syntaxtree *analyze __P_((CONST char *, syntaxtree *, int));
#ifdef	DJGPP
extern int closepipe __P_((int, int));
#define	closepipe2		closepipe
#else
extern int closepipe __P_((int));
#define	closepipe2(f, d)	closepipe(f)
#endif
extern char *evalbackquote __P_((CONST char *));
#ifdef	NOALIAS
extern int checktype __P_((CONST char *, int *, int));
#define	checktype2(s, i, a, f)	checktype(s, i, f)
#else
extern int checktype __P_((CONST char *, int *, int, int));
#define	checktype2		checktype
#endif
#if	defined (FD) && !defined (_NOCOMPLETE)
extern int completeshellvar __P_((CONST char *, int, int, char ***));
extern int completeshellcomm __P_((CONST char *, int, int, char ***));
#endif
extern int getsubst __P_((int, char **, char ***, int **));
extern VOID printstree __P_((syntaxtree *, int, XFILE *));
extern int tinygetopt __P_((syntaxtree *, CONST char *, int *));
extern int setexport __P_((CONST char *));
extern int setronly __P_((CONST char *));
extern int typeone __P_((CONST char *, XFILE *));
#ifndef	FDSH
extern char **getsimpleargv __P_((syntaxtree *));
#endif
extern VOID setshfunc __P_((char *, syntaxtree *));
extern int unsetshfunc __P_((CONST char *, int));
#if	MSDOS
extern int exec_simplecom __P_((syntaxtree *, int, int));
#define	exec_simplecom2(tr, t, i, b) \
				exec_simplecom(tr, t, i)
#else
extern int exec_simplecom __P_((syntaxtree *, int, int, int));
#define	exec_simplecom2		exec_simplecom
#endif
extern int execruncom __P_((CONST char *, int));
extern VOID setshellvar __P_((char *CONST *));
extern int prepareterm __P_((VOID_A));
extern int initshell __P_((int, char *CONST *));
extern int shell_loop __P_((int));
extern int main_shell __P_((int, char *CONST *, char *CONST *));
