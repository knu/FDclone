/*
 *	termemu.h
 *
 *	type definitions for "termemu.c"
 */

typedef struct _ptyinfo_t {
	p_id_t pid;
	char *path;
	int fd;
	int pipe;
	int status;
} ptyinfo_t;

extern ptyinfo_t ptylist[];

#define	TE_PUTCH2		(K_MAX + 1)
#define	TE_CPUTS2		(K_MAX + 2)
#define	TE_PUTTERM		(K_MAX + 3)
#define	TE_PUTTERMS		(K_MAX + 4)
#define	TE_SETSCROLL		(K_MAX + 5)
#define	TE_LOCATE		(K_MAX + 6)
#define	TE_CPUTNL		(K_MAX + 7)
#define	TE_CHGCOLOR		(K_MAX + 8)
#define	TE_MOVECURSOR		(K_MAX + 9)
#define	TE_CHANGEWIN		(K_MAX + 10)
#define	TE_CHANGEWSIZE		(K_MAX + 11)
#define	TE_INSERTWIN		(K_MAX + 12)
#define	TE_DELETEWIN		(K_MAX + 13)
#define	TE_CHANGEKCODE		(K_MAX + 14)
#define	TE_AWAKECHILD		(K_MAX + 99)

#define	TE_SETVAR		1
#define	TE_PUSHVAR		2
#define	TE_POPVAR		3
#define	TE_CHDIR		4
#define	TE_PUTEXPORTVAR		5
#define	TE_PUTSHELLVAR		6
#define	TE_UNSET		7
#define	TE_SETEXPORT		8
#define	TE_SETRONLY		9
#define	TE_SETSHFLAG		10
#define	TE_ADDFUNCTION		11
#define	TE_DELETEFUNCTION	12
#define	TE_ADDALIAS		13
#define	TE_DELETEALIAS		14
#define	TE_SETHISTORY		15
#define	TE_ADDKEYBIND		16
#define	TE_DELETEKEYBIND	17
#define	TE_SETKEYSEQ		18
#define	TE_ADDLAUNCH		19
#define	TE_DELETELAUNCH		20
#define	TE_ADDARCH		21
#define	TE_DELETEARCH		22
#define	TE_INSERTDRV		23
#define	TE_DELETEDRV		24
#define	TE_CHANGESTATUS		99
