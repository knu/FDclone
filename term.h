/*
 *	term.h
 *
 *	definitions & function prototype declarations for "term.c"
 */

#ifndef	__TERM_H_
#define	__TERM_H_

#ifndef	__SYS_TYPES_STAT_H_
#define	__SYS_TYPES_STAT_H_
#include <sys/types.h>
#include <sys/stat.h>
#endif

typedef struct _keyseq_t {
	short code;
	u_char len;
	char *str;
} keyseq_t;

#define	GETSIZE	"\033[6n"
#define	SIZEFMT	"\033[%d;%dR"

#define	K_CR	'\r'
#define	K_ESC	'\033'

#define	K_MIN	K_NOKEY
#define	K_NOKEY	0401
#define	K_DOWN	0402
#define	K_UP	0403
#define	K_LEFT	0404
#define	K_RIGHT	0405
#define	K_HOME	0406
#define	K_BS	0407
#define	K_F0	0410
#define	K_F(n)	(K_F0 + (n))
#define	K_DL	0510
#define	K_IL	0511
#define	K_DC	0512
#define	K_IC	0513
#define	K_EIC	0514
#define	K_CLR	0515
#define	K_EOS	0516
#define	K_EOL	0517
#define	K_ESF	0520
#define	K_ESR	0521
#define	K_NPAGE	0522
#define	K_PPAGE	0523
#define	K_STAB	0524
#define	K_CTAB	0525
#define	K_CATAB	0526
#define	K_ENTER	0527
#define	K_SRST	0530
#define	K_RST	0531
#define	K_PRINT	0532
#define	K_LL	0533
#define	K_A1	0534
#define	K_A3	0535
#define	K_B2	0536
#define	K_C1	0537
#define	K_C3	0540
#define	K_BTAB	0541
#define	K_BEG	0542
#define	K_CANC	0543
#define	K_CLOSE	0544
#define	K_COMM	0545
#define	K_COPY	0546
#define	K_CREAT	0547
#define	K_END	0550
#define	K_EXIT	0551
#define	K_FIND	0552
#define	K_HELP	0553
#define	K_MAX	K_HELP

#define	K_METAKEY	01000
#define	K_ALTERNATE	02000
#if	MSDOS
#define	mkmetakey(c)	(K_METAKEY | (tolower2(c) & 0x7f))
#define	ismetakey(c)	(((c) & K_METAKEY) && islower2((c) & 0xff))
#else
#define	mkmetakey(c)	(K_METAKEY | ((c) & 0x7f))
#define	ismetakey(c)	(((c) & K_METAKEY) && isalpha2((c) & 0xff))
#endif
#define	mkekana(c)	(K_METAKEY | ((c) & 0xff))
#define	isekana2(c)	(((c) & K_METAKEY) && iskana2((c) & 0xff))
#define	alternate(c)	((c) & ~K_ALTERNATE)

#ifndef	K_CTRL
#define	K_CTRL(c)	((c) & 037)
#endif

extern int n_column;
extern int n_lastcolumn;
extern int n_line;
extern int stable_standout;

#if	MSDOS
extern CONST char *termstr[];
#else
extern char *termstr[];
#endif
#define	T_INIT		0
#define	T_END		1
#define	T_METAMODE	2
#define	T_NOMETAMODE	3
#define	T_SCROLL	4
#define	T_KEYPAD	5
#define	T_NOKEYPAD	6
#define	T_NORMALCURSOR	7
#define	T_HIGHCURSOR	8
#define	T_NOCURSOR	9
#define	T_SETCURSOR	10
#define	T_RESETCURSOR	11
#define	T_BELL		12
#define	T_VBELL		13
#define	T_CLEAR		14
#define	T_NORMAL	15
#define	T_BOLD		16
#define	T_REVERSE	17
#define	T_DIM		18
#define	T_BLINK		19
#define	T_STANDOUT	20
#define	T_UNDERLINE	21
#define	END_STANDOUT	22
#define	END_UNDERLINE	23
#define	L_CLEAR		24
#define	L_CLEARBOL	25
#define	L_INSERT	26
#define	L_DELETE	27
#define	C_INSERT	28
#define	C_DELETE	29
#define	C_LOCATE	30
#define	C_HOME		31
#define	C_RETURN	32
#define	C_NEWLINE	33
#define	C_SCROLLFORW	34
#define	C_SCROLLREV	35
#define	C_UP		36
#define	C_DOWN		37
#define	C_RIGHT		38
#define	C_LEFT		39
#define	C_NUP		40
#define	C_NDOWN		41
#define	C_NRIGHT	42
#define	C_NLEFT		43
#define	T_FGCOLOR	44
#define	T_BGCOLOR	45
#define	MAXTERMSTR	46

extern u_char cc_intr;
extern u_char cc_quit;
extern u_char cc_eof;
extern u_char cc_eol;
extern u_char cc_erase;
extern int (*keywaitfunc)__P_((VOID_A));
#if	!MSDOS
extern int usegetcursor;
extern int suspended;
extern char *duptty[2];
#endif
extern int ttyio;
extern int isttyiomode;
extern FILE *ttyout;
extern int dumbterm;

extern int inittty __P_((int));
extern int cooked2 __P_((VOID_A));
extern int cbreak2 __P_((VOID_A));
extern int raw2 __P_((VOID_A));
extern int echo2 __P_((VOID_A));
extern int noecho2 __P_((VOID_A));
extern int nl2 __P_((VOID_A));
extern int nonl2 __P_((VOID_A));
extern int tabs __P_((VOID_A));
extern int notabs __P_((VOID_A));
extern int keyflush __P_((VOID_A));
#if	!MSDOS
extern int savettyio __P_((int));
#endif
extern int ttyiomode __P_((int));
extern int stdiomode __P_((VOID_A));
extern int termmode __P_((int));
extern int exit2 __P_((int));
extern int getxy __P_((int *, int *));
extern char *tparamstr __P_((CONST char *, int, int));
extern int getterment __P_((CONST char *));
#if	!MSDOS
extern int freeterment __P_((VOID_A));
extern int regetterment __P_((CONST char *, int));
extern int setdefterment __P_((VOID_A));
extern int setdefkeyseq __P_((VOID_A));
extern int getdefkeyseq __P_((keyseq_t *));
extern int setkeyseq __P_((int, char *, int));
extern int getkeyseq __P_((keyseq_t *));
extern keyseq_t *copykeyseq __P_((keyseq_t *));
extern int freekeyseq __P_((keyseq_t *));
#endif
extern int initterm __P_((VOID_A));
extern int endterm __P_((VOID_A));
extern int putterm __P_((int));
extern int putch2 __P_((int));
extern int cputs2 __P_((CONST char *));
#if	MSDOS
#define	tputs2(s,n)	cputs2(s)
#define	putterms	putterm
#else
extern int tputs2 __P_((CONST char *, int));
extern int putterms __P_((int));
extern VOID checksuspend __P_((VOID_A));
#endif
extern int kbhit2 __P_((long));
extern int getch2 __P_((VOID_A));
extern int getkey2 __P_((int, int));
#if	MSDOS
#define	getkey3		getkey2
#else
extern int getkey3 __P_((int, int));
#endif
extern int ungetkey2 __P_((int));
extern int setscroll __P_((int, int));
extern int locate __P_((int, int));
extern int tflush __P_((VOID_A));
extern char *getwsize __P_((int, int));
extern int setwsize __P_((int, int, int));
extern int cprintf2 __P_((CONST char *, ...));
extern int cputnl __P_((VOID_A));
extern int kanjiputs __P_((CONST char *));
extern int chgcolor __P_((int, int));
extern int movecursor __P_((int, int, int));

#ifndef	SENSEPERSEC
#define	SENSEPERSEC	50
#endif
#ifndef	WAITKEYPAD
#define	WAITKEYPAD	360		/* msec */
#endif
#ifndef	WAITMETA
#define	WAITMETA	120		/* msec */
#endif

#define	ANSI_BLACK	0
#define	ANSI_RED	1
#define	ANSI_GREEN	2
#define	ANSI_YELLOW	3
#define	ANSI_BLUE	4
#define	ANSI_MAGENTA	5
#define	ANSI_CYAN	6
#define	ANSI_WHITE	7
#define	ANSI_NORMAL	30
#define	ANSI_REVERSE	40

#endif	/* !__TERM_H_ */
