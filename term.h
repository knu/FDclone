/*
 *	term.h
 *
 *	Include File for "term.c" using TERMCAP
 */

#define	CR	'\r'
#define	ESC	'\033'

#define	K_MIN	K_DOWN
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
#define	K_CLR	0515
#define	K_NPAGE	0522
#define	K_PPAGE	0523
#define	K_MAX	K_PPAGE

#define	CTRL_A	'\001'
#define	CTRL_B	'\002'
#define	CTRL_C	'\003'
#define	CTRL_D	'\004'
#define	CTRL_E	'\005'
#define	CTRL_F	'\006'
#define	CTRL_G	'\007'
#define	CTRL_H	'\010'
#define	CTRL_I	'\011'
#define	CTRL_J	'\012'
#define	CTRL_K	'\013'
#define	CTRL_L	'\014'
#define	CTRL_M	'\015'
#define	CTRL_N	'\016'
#define	CTRL_O	'\017'
#define	CTRL_P	'\020'
#define	CTRL_Q	'\021'
#define	CTRL_R	'\022'
#define	CTRL_S	'\023'
#define	CTRL_T	'\024'
#define	CTRL_U	'\025'
#define	CTRL_V	'\026'
#define	CTRL_W	'\027'
#define	CTRL_X	'\030'
#define	CTRL_Y	'\031'
#define	CTRL_Z	'\032'

extern int n_column;
extern int n_lastcolumn;
extern int n_line;
extern int stable_standout;
extern char *t_init;
extern char *t_end;
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
extern char *c_up;
extern char *c_down;
extern char *c_right;
extern char *c_left;

#define	putterm(str)	tputs(str, 1, putch)
#define	putterms(str)	tputs(str, n_line, putch)

extern int inittty();
extern int cooked2();
extern int cbreak2();
extern int raw2();
extern int echo2();
extern int noecho2();
extern int nl2();
extern int nonl2();
extern int tabs();
extern int notabs();
extern int keyflush();
extern int exit2();
extern int getterment();
extern int initterm();
extern int endterm();
extern int putch();
extern int cputs();
extern int cprintf();
extern int kbhit2();
extern int getch2();
extern int getkey();
extern int ungetch2();
extern int locate();
extern int tflush();
extern int getwsize();
