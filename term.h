/*
 *	term.h
 *
 *	Variables for TERMCAP
 */

#ifndef	__TERM_H_
#define	__TERM_H_

#ifndef	VOID_P
#ifdef	NOVOID
#define	VOID
#define	VOID_T	int
#define	VOID_P	char *
#else
#define	VOID	void
#define	VOID_T	void
#define	VOID_P	void *
#endif
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

#define	CR	'\r'
#define	ESC	'\033'

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

#ifdef	CTRL
#undef	CTRL
#endif
#define	CTRL(c)	((c) & 037)

extern int n_column;
extern int n_lastcolumn;
extern int n_line;
extern int stable_standout;
extern char *t_init;
extern char *t_end;
extern char *t_metamode;
extern char *t_nometamode;
extern char *t_scroll;
extern char *t_keypad;
extern char *t_nokeypad;
extern char *t_normalcursor;
extern char *t_highcursor;
extern char *t_nocursor;
extern char *t_setcursor;
extern char *t_resetcursor;
extern char *t_bell;
extern char *t_vbell;
extern char *t_clear;
extern char *t_normal;
extern char *t_bold;
extern char *t_reverse;
extern char *t_dim;
extern char *t_blink;
extern char *t_standout;
extern char *t_underline;
extern char *end_standout;
extern char *end_underline;
extern char *l_clear;
extern char *l_insert;
extern char *l_delete;
extern char *c_insert;
extern char *c_delete;
extern char *c_store;
extern char *c_restore;
extern char *c_home;
extern char *c_locate;
extern char *c_return;
extern char *c_newline;
extern char *c_scrollforw;
extern char *c_scrollrev;
extern char *c_up;
extern char *c_down;
extern char *c_right;
extern char *c_left;
extern char *c_nup;
extern char *c_ndown;
extern char *c_nright;
extern char *c_nleft;
extern u_char cc_intr;
extern u_char cc_quit;
extern u_char cc_eof;
extern u_char cc_eol;
extern VOID_T (*keywaitfunc)__P_((VOID_A));
#if	!MSDOS
extern int usegetcursor;
#endif
extern int ttyio;

#if	MSDOS
#define	putterm(s)	cputs2(s)
#define	putterms(s)	cputs2(s)
#else
#define	putterm(s)	tputs(s, 1, putch3)
#define	putterms(s)	tputs(s, n_line, putch3)
#endif

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
extern int exit2 __P_((int));
extern char *tparamstr __P_((char *, int, int));
extern int getterment __P_((VOID_A));
#if	!MSDOS
extern int setkeyseq __P_((int, char *, int));
extern char *getkeyseq __P_((int, int *));
#endif
extern int initterm __P_((VOID_A));
extern int endterm __P_((VOID_A));
extern int putch2 __P_((int));
#if	!MSDOS
extern int putch3 __P_((int));
#endif
extern int cputs2 __P_((char *));
#if	MSDOS || defined (__STDC__)
extern int cprintf2(CONST char *, ...);
#else
extern int cprintf2 __P_((CONST char *, ...));
#endif
extern int kbhit2 __P_((u_long));
extern int getch2 __P_((VOID_A));
extern int getkey2 __P_((int));
extern int ungetch2 __P_((u_char));
extern int setscroll __P_((int, int));
extern int locate __P_((int, int));
extern int tflush __P_((VOID_A));
extern int getwsize __P_((int, int));
extern int chgcolor __P_((int, int));

#ifndef	SENSEPERSEC
#define	SENSEPERSEC	50
#endif
#ifndef	WAITKEYPAD
#define	WAITKEYPAD	360		/* msec */
#endif
#ifndef	WAITMETA
#define	WAITMETA	60		/* msec */
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
