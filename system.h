/*
 *	system.h
 *
 *	type definitions & function prototype declarations for "system.c"
 */

#include "termio.h"
#include "wait.h"
#ifdef	HPUX
/* for TIOCGPGRP & TIOCSPGRP */
#include <bsdtty.h>
#endif

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

#ifdef	BASHSTYLE
#define	BASHBUG
#define	ERRBREAK	continue
#else
#define	ERRBREAK	break
#endif

#define	RET_SUCCESS	0
#define	RET_FAIL	1
#define	RET_SYNTAXERR	2
#define	RET_FATALERR	2
#ifdef	SIGINT
#define	RET_INTR	(SIGINT + 128)
#else
#define	RET_INTR	(2 + 128)
#endif
#define	RET_NOTEXEC	126
#define	RET_NOTFOUND	127
#define	RET_NOTICE	255
#define	RET_NULSYSTEM	256
#define	READ_EOF	0x100

#ifdef	TIOCGPGRP
#define	gettcpgrp(f,g)	((ioctl(f, TIOCGPGRP, g) < 0) \
			? (*(g) = (p_id_t)-1) : *(g))
#else
#define	gettcpgrp(f,g)	(*(g) = tcgetpgrp(f))
#endif
#ifdef	TIOCSPGRP
#define	settcpgrp(f,g)	ioctl(f, TIOCSPGRP, &(g))
#else
#define	settcpgrp	tcsetpgrp
#endif

#if	MSDOS
#define	NOJOB
#define	DOSCOMMAND
#define	USEFAKEPIPE
#define	Xexit		exit
#define	DEFPATH		":"
#else
# if	defined (FD) && !defined (_NODOSCOMMAND)
# define	DOSCOMMAND
# endif
#define	Xexit		_exit
#define	DEFPATH		":/bin:/usr/bin"
#define	DEFTERM		"dumb"
#define	NULLDEVICE	"/dev/null"
#endif

#ifdef	MINIMUMSHELL
#define	NOJOB
#define	NOALIAS
#define	NOPOSIXUTIL
#endif

typedef struct _heredoc_t {
	char *eof;
	char *filename;
	char *buf;
	int fd;
	u_char flags;
} heredoc_t;

#define	HD_IGNORETAB	0001
#define	HD_QUOTED	0002

typedef struct _redirectlist {
	int fd;
	char *filename;
	u_char type;
	int new;
	int old;
#if	defined (FD) && !defined (_NODOSDRIVE)
	char *fakepipe;
	int dosfd;
#endif
	struct _redirectlist *next;
} redirectlist;

#define	MD_NORMAL	0000
#define	MD_READ		0001
#define	MD_WRITE	0002
#define	MD_RDWR		0003
#define	MD_APPEND	0004
#define	MD_FILEDESC	0010
#define	MD_WITHERR	0020
#define	MD_HEREDOC	0040
#define	MD_FORCED	0100
#define	MD_REST		0200

typedef struct _command_t {
	hashlist *hash;
	int argc;
	char **argv;
	redirectlist *redp;
	u_char type;
	u_char id;
} command_t;

#define	CT_STATEMENT	0001
#define	CT_NONE		0002
#define	CT_BUILTIN	0003
#define	CT_COMMAND	0004
#define	CT_FUNCTION	0005
#define	CT_ALIAS	0006
#define	CT_LOGDRIVE	0007
#ifdef	FD
#define	CT_FDORIGINAL	0100
#define	CT_FDINTERNAL	0200
#endif

#define	SM_IF		001
#define	SM_THEN		002
#define	SM_ELIF		003
#define	SM_ELSE		004
#define	SM_FI		005
#define	SM_WHILE	006
#define	SM_UNTIL	007
#define	SM_DO		010
#define	SM_DONE		011
#define	SM_FOR		012
#define	SM_IN		013
#define	SM_CASE		014
#define	SM_INCASE	015
#define	SM_RPAREN	016
#define	SM_CASEEND	017
#define	SM_ESAC		020
#define	SM_LPAREN	021
#define	SM_FUNC		022
#define	SM_LIST		023
#define	SM_LISTEND	024
#define	SM_ANOTHER	075
#define	SM_CHILD	076
#define	SM_STATEMENT	077

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

#define	OP_NONE	0
#define	OP_FG	1
#define	OP_BG	2
#define	OP_AND	3
#define	OP_OR	4
#define	OP_PIPE	5
#define	OP_NOT	6
#define	OP_NOWN	7

#define	CN_META	0001
#define	CN_QUOT	0002
#define	CN_STAT	0004
#define	CN_INHR	0170
#define	CN_SBST	0070
#define	CN_VAR	0010
#define	CN_COMM	0020
#define	CN_EXPR	0030
#define	CN_CASE	0040
#define	CN_HDOC	0100

#define	ST_NODE	0001
#define	ST_NEXT	0002
#define	ST_TOP	0004
#define	ST_NOWN	0010
#define	ST_BUSY	0020
#define	ST_HDOC	0040

#ifdef	MINIMUMSHELL
#define	hasparent(trp)	((trp) -> parent)
#define	getparent(trp)	((trp) -> parent)
#else
#define	hasparent(trp)	(!((trp) -> flags & ST_TOP) && (trp) -> parent)
#define	getparent(trp)	(((trp) -> flags & ST_TOP) ? NULL : (trp) -> parent)
#endif
#define	statementbody(trp) \
			((syntaxtree *)(((trp) -> comm) -> argv))
#define	hascomm(trp)	((trp) -> comm && ((trp) -> comm) -> argc >= 0)
#define	isopfg(trp)	((trp) -> type == OP_FG)
#define	isopbg(trp)	((trp) -> type == OP_BG)
#define	isopand(trp)	((trp) -> type == OP_AND)
#define	isopor(trp)	((trp) -> type == OP_OR)
#define	isoppipe(trp)	((trp) -> type == OP_PIPE)
#ifdef	MINIMUMSHELL
#define	isopnot(trp)	(0)
#define	isopnown(trp)	(0)
#else
#define	isopnot(trp)	((trp) -> type == OP_NOT)
#define	isopnown(trp)	((trp) -> type == OP_NOWN)
#endif

typedef struct _shbuiltintable {
	int (NEAR *func)__P_((syntaxtree *));
	char *ident;
	u_char flags;
} shbuiltintable;

#define	BT_NOGLOB	0001
#define	BT_RESTRICT	0002
#define	BT_POSIXSPECIAL	0004
#define	BT_NOKANJIFGET	0010
#define	BT_DISABLE	0020
#define	BT_FILENAME	0040

#define	SMPREV	4
typedef struct _statementtable {
	int (NEAR *func)__P_((syntaxtree *));
	char *ident;
	u_char type;
	u_char prev[SMPREV];
} statementtable;

#define	STT_TYPE	0017
#define	STT_FOR		0001
#define	STT_CASE	0002
#define	STT_IN		0003
#define	STT_INCASE	0004
#define	STT_CASEEND	0005
#define	STT_LIST	0006
#define	STT_LPAREN	0007
#define	STT_FUNC	0010
#define	STT_NEEDLIST	0020
#define	STT_NEEDIDENT	0040
#define	STT_NEEDNONE	0100
#define	STT_NEEDCOMM	0200

typedef struct _opetable {
	u_char op;
	u_char level;
	char *symbol;
} opetable;

typedef struct _pipelist {
	char *file;
	FILE *fp;
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
	int npipe;
	syntaxtree *trp;
	termioctl_t *tty;
# ifdef	USESGTTY
	int *ttyflag;
# endif
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

#define	AL_USED		0001

typedef struct _signaltable {
	int sig;
	int (*func)__P_((VOID_A));
	char *ident;
	char *mes;
	u_char flags;
} signaltable;

#define	TR_STAT		0007
#define	TR_IGN		0001
#define	TR_TERM		0002
#define	TR_STOP		0003
#define	TR_TRAP		0004
#define	TR_BLOCK	0010
#define	TR_NOTRAP	0020
#define	TR_CATCH	0040
#define	TR_READBL	0100

typedef struct _ulimittable {
	u_char opt;
	int res;
	int unit;
	char *mes;
} ulimittable;

typedef struct _shflagtable {
	char *ident;
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
#endif	/* FD */
extern int loginshell;
extern int noruncom;

extern statementtable statementlist[];
extern signaltable signallist[];

extern VOID prepareexit __P_((int));
extern VOID Xexit2 __P_((int));
extern VOID execerror __P_((char *, int, int));
extern VOID doperror __P_((char *, char *));
extern int isnumeric __P_((char *));
#if	!MSDOS
extern VOID dispsignal __P_((int, int, FILE *));
extern int waitjob __P_((p_id_t, wait_pid_t *, int));
extern int waitchild __P_((p_id_t, syntaxtree *));
#endif
extern VOID setshflag __P_((int, int));
extern char *evalvararg __P_((char *, int, int, int));
extern VOID freeheredoc __P_((heredoc_t *, int));
extern VOID freerlist __P_((redirectlist *, int));
extern VOID freecomm __P_((command_t *, int));
extern syntaxtree *newstree __P_((syntaxtree *));
extern VOID freestree __P_((syntaxtree *));
extern syntaxtree *parentstree __P_((syntaxtree *));
#if	!MSDOS
extern VOID cmpmail __P_((char *, char *, time_t *));
#endif
extern int identcheck __P_((char *, int));
extern char *getshellvar __P_((char *, int));
extern int putexportvar __P_((char *, int));
extern int putshellvar __P_((char *, int));
extern int unset __P_((char *, int));
#ifdef	MINIMUMSHELL
extern syntaxtree * duplstree __P_((syntaxtree *, syntaxtree *));
#else
extern syntaxtree * duplstree __P_((syntaxtree *, syntaxtree *, long));
#endif
extern int getstatid __P_((syntaxtree *trp));
#if	defined (BASHSTYLE) || !defined (MINIMUMSHELL)
extern syntaxtree *startvar __P_((syntaxtree *, redirectlist *,
		char *, int *, int *, int));
#endif
extern syntaxtree *analyze __P_((char *, syntaxtree *, int));
extern char *evalbackquote __P_((char *));
#ifdef	NOALIAS
extern int checktype __P_((char *, int *, int));
#else
extern int checktype __P_((char *, int *, int, int));
#endif
#if	defined (FD) && !defined (_NOCOMPLETE)
extern int completeshellvar __P_((char *, int, int, char ***));
extern int completeshellcomm __P_((char *, int, int, char ***));
#endif
extern int getsubst __P_((int, char **, char ***, int **));
extern VOID printstree __P_((syntaxtree *, int, FILE *));
#if	defined (FD) || !defined (NOPOSIXUTIL)
extern int tinygetopt __P_((syntaxtree *, char *, int *));
#endif
extern int setexport __P_((char *));
extern int setronly __P_((char *));
extern int typeone __P_((char *, FILE *));
#ifndef	FDSH
extern char **getsimpleargv __P_((syntaxtree *));
#endif
extern VOID setshfunc __P_((char *, syntaxtree *));
extern int unsetshfunc __P_((char *, int));
#if	MSDOS
extern int exec_simplecom __P_((syntaxtree *, int, int));
#else
extern int exec_simplecom __P_((syntaxtree *, int, int, int));
#endif
extern int execruncom __P_((char *, int));
extern VOID setshellvar __P_((char *[]));
extern int prepareterm __P_((VOID_A));
extern int initshell __P_((int, char *[]));
extern int shell_loop __P_((int));
extern int main_shell __P_((int, char *[], char *[]));
