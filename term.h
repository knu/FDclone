/*
 *	term.h
 *
 *	definitions & function prototype declarations for "term.c"
 */

#ifndef	__TERM_H_
#define	__TERM_H_

#include "stream.h"

typedef struct _keyseq_t {
	char *str;
	short code;
	u_char len;
	u_char flags;
} keyseq_t;

#define	KF_DEFINED		0001
#define	KF_HASALTER		0002

#define	GETSIZE			"\033[6n"
#define	SIZEFMT			"\033[%d;%dR"

#define	K_CR			'\r'
#define	K_ESC			'\033'

#define	K_MIN			K_NOKEY
#define	K_NOKEY			0401
#define	K_DOWN			0402
#define	K_UP			0403
#define	K_LEFT			0404
#define	K_RIGHT			0405
#define	K_HOME			0406
#define	K_BS			0407
#define	K_F0			0410
#define	K_F(n)			(K_F0 + (n))
#define	K_DL			0510
#define	K_IL			0511
#define	K_DC			0512
#define	K_IC			0513
#define	K_EIC			0514
#define	K_CLR			0515
#define	K_EOS			0516
#define	K_EOL			0517
#define	K_ESF			0520
#define	K_ESR			0521
#define	K_NPAGE			0522
#define	K_PPAGE			0523
#define	K_STAB			0524
#define	K_CTAB			0525
#define	K_CATAB			0526
#define	K_ENTER			0527
#define	K_SRST			0530
#define	K_RST			0531
#define	K_PRINT			0532
#define	K_LL			0533
#define	K_A1			0534
#define	K_A3			0535
#define	K_B2			0536
#define	K_C1			0537
#define	K_C3			0540
#define	K_BTAB			0541
#define	K_BEG			0542
#define	K_CANC			0543
#define	K_CLOSE			0544
#define	K_COMM			0545
#define	K_COPY			0546
#define	K_CREAT			0547
#define	K_END			0550
#define	K_EXIT			0551
#define	K_FIND			0552
#define	K_HELP			0553
#define	K_MAX			K_HELP
#define	K_TIMEOUT		(K_MAX + 1)

#define	K_METAKEY		01000
#define	K_ALTERNATE		02000
#if	MSDOS
#define	mkmetakey(c)		(K_METAKEY | (Xtolower(c) & 0x7f))
#define	ismetakey(c)		(((c) & K_METAKEY) && Xislower((c) & 0xff))
#else
#define	mkmetakey(c)		(K_METAKEY | ((c) & 0x7f))
#define	ismetakey(c)		(((c) & K_METAKEY) && Xisalpha((c) & 0xff))
#endif
#define	mkekana(c)		(K_METAKEY | ((c) & 0xff))
#define	isekana2(c)		(((c) & K_METAKEY) && Xiskana((c) & 0xff))
#define	alternate(c)		((c) & ~K_ALTERNATE)

#ifndef	K_CTRL
#define	K_CTRL(c)		((c) & 037)
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
#define	T_INIT			0
#define	T_END			1
#define	T_METAMODE		2
#define	T_NOMETAMODE		3
#define	T_SCROLL		4
#define	T_KEYPAD		5
#define	T_NOKEYPAD		6
#define	T_NORMALCURSOR		7
#define	T_HIGHCURSOR		8
#define	T_NOCURSOR		9
#define	T_SETCURSOR		10
#define	T_RESETCURSOR		11
#define	T_BELL			12
#define	T_VBELL			13
#define	T_CLEAR			14
#define	T_NORMAL		15
#define	T_BOLD			16
#define	T_REVERSE		17
#define	T_DIM			18
#define	T_BLINK			19
#define	T_STANDOUT		20
#define	T_UNDERLINE		21
#define	END_STANDOUT		22
#define	END_UNDERLINE		23
#define	L_CLEAR			24
#define	L_CLEARBOL		25
#define	L_INSERT		26
#define	L_DELETE		27
#define	C_INSERT		28
#define	C_DELETE		29
#define	C_LOCATE		30
#define	C_HOME			31
#define	C_RETURN		32
#define	C_NEWLINE		33
#define	C_SCROLLFORW		34
#define	C_SCROLLREV		35
#define	C_UP			36
#define	C_DOWN			37
#define	C_RIGHT			38
#define	C_LEFT			39
#define	C_NUP			40
#define	C_NDOWN			41
#define	C_NRIGHT		42
#define	C_NLEFT			43
#define	T_FGCOLOR		44
#define	T_BGCOLOR		45
#define	MAXTERMSTR		46

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
extern XFILE *ttyout;
extern int dumbterm;

extern VOID inittty __P_((int));
extern VOID Xcooked __P_((VOID_A));
extern VOID Xcbreak __P_((VOID_A));
extern VOID Xraw __P_((VOID_A));
extern VOID Xecho __P_((VOID_A));
extern VOID Xnoecho __P_((VOID_A));
extern VOID Xnl __P_((VOID_A));
extern VOID Xnonl __P_((VOID_A));
extern VOID tabs __P_((VOID_A));
extern VOID notabs __P_((VOID_A));
extern VOID keyflush __P_((VOID_A));
#if	!MSDOS
extern int savettyio __P_((int));
#endif
extern VOID ttyiomode __P_((int));
extern VOID stdiomode __P_((VOID_A));
extern int termmode __P_((int));
extern VOID exit2 __P_((int));
extern int getxy __P_((int *, int *));
extern VOID getterment __P_((CONST char *));
#if	!MSDOS
extern VOID freeterment __P_((VOID_A));
extern VOID regetterment __P_((CONST char *, int));
extern VOID setdefterment __P_((VOID_A));
extern VOID setdefkeyseq __P_((VOID_A));
extern int getdefkeyseq __P_((keyseq_t *));
extern VOID setkeyseq __P_((int, char *, int));
extern int getkeyseq __P_((keyseq_t *));
extern keyseq_t *copykeyseq __P_((keyseq_t *));
extern VOID freekeyseq __P_((keyseq_t *));
#endif
extern int tputparam __P_((int, int, int, int));
extern VOID initterm __P_((VOID_A));
extern VOID endterm __P_((VOID_A));
extern VOID putterm __P_((int));
extern int Xputch __P_((int));
extern VOID Xcputs __P_((CONST char *));
extern VOID tputs2 __P_((CONST char *, int));
#if	MSDOS
#define	putterms		putterm
#else
extern VOID putterms __P_((int));
extern VOID checksuspend __P_((VOID_A));
#endif
extern int kbhit2 __P_((long));
extern int Xgetch __P_((VOID_A));
extern int getkey2 __P_((int, int, int));
#if	MSDOS
#define	getkey3			getkey2
#else
extern int getkey3 __P_((int, int, int));
#endif
extern int ungetkey2 __P_((int, int));
extern int setscroll __P_((int, int));
extern VOID locate __P_((int, int));
extern VOID tflush __P_((VOID_A));
extern char *getwsize __P_((int, int));
extern VOID setwsize __P_((int, int, int));
extern int cvasprintf __P_((char **sp, CONST char *fmt, va_list));
extern int Xcprintf __P_((CONST char *, ...));
extern VOID tprintf __P_((CONST char *, int, ...));
extern VOID cputnl __P_((VOID_A));
extern int kanjiputs __P_((CONST char *));
extern VOID attrputs __P_((CONST char *, int));
extern int attrprintf __P_((CONST char *, int, ...));
extern int attrkanjiputs __P_((CONST char *, int));
extern VOID chgcolor __P_((int, int));
extern VOID movecursor __P_((int, int, int));

#ifndef	SENSEPERSEC
#define	SENSEPERSEC		50
#endif
#ifndef	WAITKEYPAD
#define	WAITKEYPAD		360		/* msec */
#endif
#ifndef	WAITTERMINAL
#define	WAITTERMINAL		WAITKEYPAD	/* msec */
#endif
#ifndef	WAITKANJI
#define	WAITKANJI		120		/* msec */
#endif

#define	ANSI_BLACK		0
#define	ANSI_RED		1
#define	ANSI_GREEN		2
#define	ANSI_YELLOW		3
#define	ANSI_BLUE		4
#define	ANSI_MAGENTA		5
#define	ANSI_CYAN		6
#define	ANSI_WHITE		7
#define	ANSI_NORMAL		30
#define	ANSI_REVERSE		40

#endif	/* !__TERM_H_ */
