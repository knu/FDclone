/*
 *	system.h
 *
 *	Type Definition & Function Prototype Declaration for "system.c"
 */

#ifdef	BASHSTYLE
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

#if	MSDOS || defined (DEBUG)
#define	DOSCOMMAND
#endif

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
	u_char flags;
} syntaxtree;

#define	ST_TYPE	0007
#define	ST_CONT	0070
#define	ST_META	0010
#define	ST_QUOT	0020
#define	ST_STAT	0040
#define	ST_NODE	0100
#define	ST_NEXT	0200

#define	OP_NONE	0
#define	OP_FG	1
#define	OP_BG	2
#define	OP_AND	3
#define	OP_OR	4
#define	OP_PIPE	5
#define	OP_NOT	6
#define	OP_NOWN	7

typedef struct _shbuiltintable {
	int (NEAR *func)__P_((syntaxtree *));
	char *ident;
	u_char flags;
} shbuiltintable;

#define	BT_NOGLOB	0001
#define	BT_RESTRICT	0002
#define	BT_POSIXSPECIAL	0004

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
	long pid;
	struct _pipelist *next;
} pipelist;

typedef struct _jobtable {
	long *pids;
	int *stats;
	int npipe;
	syntaxtree *trp;
} jobtable;

typedef struct _shfunctable {
	char *ident;
	syntaxtree *func;
} shfunctable;

typedef struct _aliastable {
	char *ident;
	char *comm;
	u_char flags;
} aliastable;

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

typedef struct _ulimittable {
	u_char opt;
	int res;
	int unit;
	char *mes;
} ulimittable;

extern char *ifs_set;
extern char **shellvar;
extern char **envvar;

extern VOID prepareexit __P_((int));
#ifndef	NOJOB
extern VOID killjob __P_((VOID_A));
#endif
extern char *readline __P_((int));
extern syntaxtree *newstree __P_((syntaxtree *));
extern VOID freestree __P_((syntaxtree *));
extern char *getshellvar __P_((char *, int));
extern int putexportvar __P_((char *, int));
extern int putshellvar __P_((char *, int));
extern syntaxtree *analyze __P_((char *, syntaxtree *, int));
#if	defined (FD) && !defined (_NOCOMPLETE)
extern int completeshellcomm __P_((char *, int, int, char ***));
#endif
extern int getsubst __P_((int, char **, char ***, int **));
extern char **getsimpleargv __P_((syntaxtree *));
extern int exec_line __P_((char *));
extern int dosystem __P_((char *));
extern FILE *dopopen __P_((char *));
extern int dopclose __P_((FILE *));
extern int execruncom __P_((char *, int));
extern int initshell __P_((int, char *[], char *[]));
extern int shell_loop __P_((int));
extern int main_shell __P_((int, char *[], char *[]));
