/*
 *	term.h
 *
 *	Variables for TERMCAP
 */

#ifndef	__TERM_H_
#define	__TERM_H_

#ifndef	__SYS_TYPES_STAT_H_
#define	__SYS_TYPES_STAT_H_
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifndef	STDIN_FILENO
#define	STDIN_FILENO	0
#endif
#ifndef	STDOUT_FILENO
#define	STDOUT_FILENO	1
#endif
#ifndef	STDERR_FILENO
#define	STDERR_FILENO	2
#endif

typedef struct _keyseq_t {
	short code;
	u_char len;
	char *str;
} keyseq_t;

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

#define	K_CTRL(c)	((c) & 037)

extern int n_column;
extern int n_lastcolumn;
extern int n_line;
extern int stable_standout;

extern char *termstr[];
#define	t_init		termstr[0]
#define	t_end		termstr[1]
#define	t_metamode	termstr[2]
#define	t_nometamode	termstr[3]
#define	t_scroll	termstr[4]
#define	t_keypad	termstr[5]
#define	t_nokeypad	termstr[6]
#define	t_normalcursor	termstr[7]
#define	t_highcursor	termstr[8]
#define	t_nocursor	termstr[9]
#define	t_setcursor	termstr[10]
#define	t_resetcursor	termstr[11]
#define	t_bell		termstr[12]
#define	t_vbell		termstr[13]
#define	t_clear		termstr[14]
#define	t_normal	termstr[15]
#define	t_bold		termstr[16]
#define	t_reverse	termstr[17]
#define	t_dim		termstr[18]
#define	t_blink		termstr[19]
#define	t_standout	termstr[20]
#define	t_underline	termstr[21]
#define	end_standout	termstr[22]
#define	end_underline	termstr[23]
#define	l_clear		termstr[24]
#define	l_insert	termstr[25]
#define	l_delete	termstr[26]
#define	c_insert	termstr[27]
#define	c_delete	termstr[28]
#define	c_locate	termstr[29]
#define	c_home		termstr[30]
#define	c_return	termstr[31]
#define	c_newline	termstr[32]
#define	c_scrollforw	termstr[33]
#define	c_scrollrev	termstr[34]
#define	c_up		termstr[35]
#define	c_down		termstr[36]
#define	c_right		termstr[37]
#define	c_left		termstr[38]
#define	c_nup		termstr[39]
#define	c_ndown		termstr[40]
#define	c_nright	termstr[41]
#define	c_nleft		termstr[42]
#define	MAXTERMSTR	43

extern u_char cc_intr;
extern u_char cc_quit;
extern u_char cc_eof;
extern u_char cc_eol;
extern u_char cc_erase;
extern VOID_T (*keywaitfunc)__P_((VOID_A));
#if	!MSDOS
extern int usegetcursor;
extern int suspended;
#endif
extern int ttyio;
extern int isttyiomode;
extern FILE *ttyout;
extern int dumbterm;

extern int opentty __P_((VOID_A));
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
extern int ttyiomode __P_((int));
extern int stdiomode __P_((VOID_A));
extern int termmode __P_((int));
extern int exit2 __P_((int));
extern int getxy __P_((int *, int *));
extern char *tparamstr __P_((char *, int, int));
extern int getterment __P_((char *));
#if	!MSDOS
extern int freeterment __P_((VOID_A));
extern int setkeyseq __P_((int, char *, int));
extern int getkeyseq __P_((keyseq_t *));
extern keyseq_t *copykeyseq __P_((keyseq_t *));
extern int freekeyseq __P_((keyseq_t *));
#endif
extern int initterm __P_((VOID_A));
extern int endterm __P_((VOID_A));
extern int putch2 __P_((int));
extern int cputs2 __P_((char *));
#if	MSDOS
#define	putterm(s)	cputs2(s)
#define	putterms(s)	cputs2(s)
#else
extern int putterm __P_((char *));
extern int putterms __P_((char *));
#endif
extern int kbhit2 __P_((u_long));
extern int getch2 __P_((VOID_A));
extern int getkey2 __P_((int));
extern int ungetch2 __P_((int));
extern int setscroll __P_((int, int));
extern int locate __P_((int, int));
extern int tflush __P_((VOID_A));
extern char *getwsize __P_((int, int));
extern int cprintf2 __P_((CONST char *, ...));
extern int cputnl __P_((VOID_A));
extern int kanjiputs __P_((char *));
extern int chgcolor __P_((int, int));

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

#endif	/* __TERM_H_ */
