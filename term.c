/*
 *	term.c
 *
 *	Terminal Module
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include "machine.h"

#if	defined (DJGPP) && (DJGPP >= 2)
#include <time.h>
#endif

#if	MSDOS
#include <dos.h>
#include <io.h>
# ifdef	PC98
# define	NOTUSEBIOS
# else
# define	USEVIDEOBIOS
# define	VIDEOBIOS	0x10
# define	BIOSSEG		0x40
# define	CURRPAGE	0x62
# define	SCRHEIGHT	0x84
# define	SCRWIDTH	0x4a
# endif
# ifdef	DJGPP
# include <dpmi.h>
# include <go32.h>
# include <sys/farptr.h>
#  if	defined (DJGPP) && (DJGPP >= 2)
#  include <libc/dosio.h>
#  else
#  define	__dpmi_regs	_go32_dpmi_registers
#  define	__dpmi_int(v,r)	((r) -> x.ss = (r) -> x.sp = 0, \
				_go32_dpmi_simulate_int(v, r))
#  define	_dos_ds		_go32_info_block.selector_for_linear_memory
#  endif
# define	intdos2(rp)	__dpmi_int(0x21, rp)
# define	getkeybuf(o)	_farpeekw(_dos_ds, KEYBUFWORKSEG * 0x10 + o)
# define	putkeybuf(o, n)	_farpokew(_dos_ds, KEYBUFWORKSEG * 0x10 + o, n)
# define	intbios(v, rp)	__dpmi_int(v, rp)
# define	getbiosbyte(o)	_farpeekb(_dos_ds, BIOSSEG * 0x10 + o)
# else	/* !DJGPP */
typedef union REGS	__dpmi_regs;
# define	intdos2(rp)	int86(0x21, rp, rp)
# define	getkeybuf(o)	(*((u_short far *)MK_FP(KEYBUFWORKSEG, o)))
# define	putkeybuf(o, n)	(*((u_short far *)MK_FP(KEYBUFWORKSEG, o)) = n)
# define	intbios(v, rp)	int86(v, rp, rp)
# define	getbiosbyte(o)	(*((u_char far *)MK_FP(BIOSSEG, o)))
#  ifdef	LSI_C
static int _asm_sti __P_((char *));
static int _asm_cli __P_((char *));
#  define	enable()	_asm_sti("\n\tsti\n")
#  define	disable()	_asm_cli("\n\tcli\n")
#  endif
# endif	/* !DJGPP */
#define	TTYNAME		"CON"
#else	/* !MSDOS */
#include <sys/file.h>
#include <sys/time.h>
#define	TTYNAME		"/dev/tty"

typedef struct _kstree_t {
	int key;
	int num;
	struct _kstree_t *next;
} kstree_t;

#ifdef	USETERMIOS
#include <termios.h>
#include <sys/ioctl.h>	/* for Linux libc6 */
typedef struct termios	termioctl_t;
#define	tioctl(d, r, a)	((r) ? tcsetattr(d, (r) - 1, a) : tcgetattr(d, a))
#define	getspeed(t)	cfgetospeed(&t)
#define	REQGETP		0
#define	REQSETP		(TCSAFLUSH + 1)
#endif

#ifdef	USETERMIO
#include <termio.h>
typedef struct termio	termioctl_t;
#define	tioctl		ioctl
#define	getspeed(t)	((t).c_cflag & CBAUD)
#define	REQGETP		TCGETA
#define	REQSETP		TCSETAF
#endif

#ifdef	USESGTTY
#include <sgtty.h>
typedef struct sgttyb	termioctl_t;
#define	tioctl		ioctl
#define	getspeed(t)	((t).sg_ospeed)
#define	REQGETP		TIOCGETP
#define	REQSETP		TIOCSETP
#endif

#ifdef	USESELECTH
#include <sys/select.h>
#endif

#if	(VEOF == VMIN) || (VEOL == VTIME)
#define	VAL_VMIN	'\004'
#define	VAL_VTIME	255
#else
#define	VAL_VMIN	0
#define	VAL_VTIME	0
#endif
#endif	/* !MSDOS */

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include <errno.h>
#ifdef	NOERRNO
extern int errno;
#endif

#include "term.h"

#define	MAXLONGWIDTH	20		/* log10(2^64) = 19.266 */
#define	BUFUNIT		16
#define	GETSIZE		"\033[6n"
#define	SIZEFMT		"\033[%d;%dR"
#define	VF_PLUS		0001
#define	VF_MINUS	0002
#define	VF_SPACE	0004
#define	VF_ZERO		0010
#define	VF_LONG		0020
#define	VF_UNSIGNED	0040
#define	WINTERMNAME	"iris"
#ifdef	USESTRERROR
#define	strerror2		strerror
#else
# ifndef	DECLERRLIST
extern char *sys_errlist[];
# endif
#define	strerror2(n)		(char *)sys_errlist[n]
#endif

#ifdef	DEBUG
extern VOID muntrace __P_ ((VOID));
extern char *_mtrace_file;
#endif

#if	!MSDOS
# ifdef	USETERMINFO
# include <curses.h>
# include <term.h>
typedef char	tputs_t;
# define	tgetnum2(s)		(s)
# define	tgetflag2(s)		(s)
# define	tgoto2(s, p1, p2)	tparm(s, p2, p1, \
					0, 0, 0, 0, 0, 0, 0)
# define	TERM_pc			pad_char
# define	TERM_bc			NULL
# define	TERM_co			columns
# define	TERM_li			lines
# define	TERM_xn			eat_newline_glitch
# define	TERM_xs			ceol_standout_glitch
# define	TERM_gn			generic_type
# define	TERM_ti			enter_ca_mode
# define	TERM_te			exit_ca_mode
# define	TERM_mm			meta_on
# define	TERM_mo			meta_off
# define	TERM_cs			change_scroll_region
# define	TERM_ks			keypad_xmit
# define	TERM_ke			keypad_local
# define	TERM_ve			cursor_normal
# define	TERM_vs			cursor_visible
# define	TERM_vi			cursor_invisible
# define	TERM_sc			save_cursor
# define	TERM_rc			restore_cursor
# define	TERM_bl			bell
# define	TERM_vb			flash_screen
# define	TERM_cl			clear_screen
# define	TERM_me			exit_attribute_mode
# define	TERM_md			enter_bold_mode
# define	TERM_mr			enter_reverse_mode
# define	TERM_mh			enter_dim_mode
# define	TERM_mb			enter_blink_mode
# define	TERM_so			enter_standout_mode
# define	TERM_us			enter_underline_mode
# define	TERM_se			exit_standout_mode
# define	TERM_ue			exit_underline_mode
# define	TERM_ce			clr_eol
# define	TERM_al			insert_line
# define	TERM_dl			delete_line
# define	TERM_ic			insert_character
# define	TERM_IC			parm_ich
# define	TERM_dc			delete_character
# define	TERM_DC			parm_dch
# define	TERM_cm			cursor_address
# define	TERM_ho			cursor_home
# define	TERM_cr			carriage_return
# define	TERM_nl			NULL
# define	TERM_sf			scroll_forward
# define	TERM_sr			scroll_reverse
# define	TERM_up			cursor_up
# define	TERM_UP			parm_up_cursor
# define	TERM_do			cursor_down
# define	TERM_DO			parm_down_cursor
# define	TERM_nd			cursor_right
# define	TERM_RI			parm_right_cursor
# define	TERM_le			cursor_left
# define	TERM_LE			parm_left_cursor
# define	TERM_ku			key_up
# define	TERM_kd			key_down
# define	TERM_kr			key_right
# define	TERM_kl			key_left
# define	TERM_kh			key_home
# define	TERM_kb			key_backspace
# define	TERM_l1			lab_f1
# define	TERM_l2			lab_f2
# define	TERM_l3			lab_f3
# define	TERM_l4			lab_f4
# define	TERM_l5			lab_f5
# define	TERM_l6			lab_f6
# define	TERM_l7			lab_f7
# define	TERM_l8			lab_f8
# define	TERM_l9			lab_f9
# define	TERM_la			lab_f10
# define	TERM_F1			key_f11
# define	TERM_F2			key_f12
# define	TERM_F3			key_f13
# define	TERM_F4			key_f14
# define	TERM_F5			key_f15
# define	TERM_F6			key_f16
# define	TERM_F7			key_f17
# define	TERM_F8			key_f18
# define	TERM_F9			key_f19
# define	TERM_FA			key_f20
# define	TERM_k1			key_f1
# define	TERM_k2			key_f2
# define	TERM_k3			key_f3
# define	TERM_k4			key_f4
# define	TERM_k5			key_f5
# define	TERM_k6			key_f6
# define	TERM_k7			key_f7
# define	TERM_k8			key_f8
# define	TERM_k9			key_f9
# define	TERM_k10		key_f10
# define	TERM_k0			key_f0
# define	TERM_kL			key_dl
# define	TERM_kA			key_il
# define	TERM_kD			key_dc
# define	TERM_kI			key_ic
# define	TERM_kC			key_clear
# define	TERM_kE			key_eol
# define	TERM_kP			key_ppage
# define	TERM_kN			key_npage
# define	TERM_at8		key_enter
# define	TERM_at1		key_beg
# define	TERM_at7		key_end
# else	/* !USETERMINFO */
typedef int	tputs_t;
extern int tgetent __P_((char *, char *));
extern int tgetnum __P_((char *));
extern int tgetflag __P_((char *));
extern char *tgetstr __P_((char *, char **));
extern char *tgoto __P_((char *, int, int));
extern int tputs __P_((char *, int, int (*)__P_((tputs_t))));
# define	tgetnum2		tgetnum
# define	tgetflag2		tgetflag
# define	tgoto2			tgoto
# define	TERM_pc			"pc"
# define	TERM_bc			"bc"
# define	TERM_co			"co"
# define	TERM_li			"li"
# define	TERM_xn			"xn"
# define	TERM_xs			"xs"
# define	TERM_gn			"gn"
# define	TERM_ti			"ti"
# define	TERM_te			"te"
# define	TERM_mm			"mm"
# define	TERM_mo			"mo"
# define	TERM_cs			"cs"
# define	TERM_ks			"ks"
# define	TERM_ke			"ke"
# define	TERM_ve			"ve"
# define	TERM_vs			"vs"
# define	TERM_vi			"vi"
# define	TERM_sc			"sc"
# define	TERM_rc			"rc"
# define	TERM_bl			"bl"
# define	TERM_vb			"vb"
# define	TERM_cl			"cl"
# define	TERM_me			"me"
# define	TERM_md			"md"
# define	TERM_mr			"mr"
# define	TERM_mh			"mh"
# define	TERM_mb			"mb"
# define	TERM_so			"so"
# define	TERM_us			"us"
# define	TERM_se			"se"
# define	TERM_ue			"ue"
# define	TERM_ce			"ce"
# define	TERM_al			"al"
# define	TERM_dl			"dl"
# define	TERM_ic			"ic"
# define	TERM_IC			"IC"
# define	TERM_dc			"dc"
# define	TERM_DC			"DC"
# define	TERM_cm			"cm"
# define	TERM_ho			"ho"
# define	TERM_cr			"cr"
# define	TERM_nl			"nl"
# define	TERM_sf			"sf"
# define	TERM_sr			"sr"
# define	TERM_up			"up"
# define	TERM_UP			"UP"
# define	TERM_do			"do"
# define	TERM_DO			"DO"
# define	TERM_nd			"nd"
# define	TERM_RI			"RI"
# define	TERM_le			"le"
# define	TERM_LE			"LE"
# define	TERM_ku			"ku"
# define	TERM_kd			"kd"
# define	TERM_kr			"kr"
# define	TERM_kl			"kl"
# define	TERM_kh			"kh"
# define	TERM_kb			"kb"
# define	TERM_l1			"l1"
# define	TERM_l2			"l2"
# define	TERM_l3			"l3"
# define	TERM_l4			"l4"
# define	TERM_l5			"l5"
# define	TERM_l6			"l6"
# define	TERM_l7			"l7"
# define	TERM_l8			"l8"
# define	TERM_l9			"l9"
# define	TERM_la			"la"
# define	TERM_F1			"F1"
# define	TERM_F2			"F2"
# define	TERM_F3			"F3"
# define	TERM_F4			"F4"
# define	TERM_F5			"F5"
# define	TERM_F6			"F6"
# define	TERM_F7			"F7"
# define	TERM_F8			"F8"
# define	TERM_F9			"F9"
# define	TERM_FA			"FA"
# define	TERM_k1			"k1"
# define	TERM_k2			"k2"
# define	TERM_k3			"k3"
# define	TERM_k4			"k4"
# define	TERM_k5			"k5"
# define	TERM_k6			"k6"
# define	TERM_k7			"k7"
# define	TERM_k8			"k8"
# define	TERM_k9			"k9"
# define	TERM_k10		"k;"
# define	TERM_k0			"k0"
# define	TERM_kL			"kL"
# define	TERM_kA			"kA"
# define	TERM_kD			"kD"
# define	TERM_kI			"kI"
# define	TERM_kC			"kC"
# define	TERM_kE			"kE"
# define	TERM_kP			"kP"
# define	TERM_kN			"kN"
# define	TERM_at8		"@8"
# define	TERM_at1		"@1"
# define	TERM_at7		"@7"
# endif	/* !USETERMINFO */

#define	TERMCAPSIZE	2048

#ifndef	PENDIN
#define	PENDIN		0
#endif
#ifndef	IEXTEN
#define	IEXTEN		0
#endif
#ifndef	ECHOCTL
#define	ECHOCTL		0
#endif
#ifndef	ECHOKE
#define	ECHOKE		0
#endif
#ifndef	OCRNL
#define	OCRNL		0
#endif
#ifndef	ONOCR
#define	ONOCR		0
#endif
#ifndef	ONLRET
#define	ONLRET		0
#endif
#ifndef	TAB3
#define	TAB3		OXTABS
#endif
#ifndef	VINTR
#define	VINTR		0
#endif
#ifndef	VQUIT
#define	VQUIT		1
#endif
#ifndef	VEOF
#define	VEOF		4
#endif
#ifndef	VEOL
#define	VEOL		5
#endif

#ifndef	FREAD
# ifdef	_FREAD
# define	FREAD	_FREAD
# else
# define	FREAD	(O_RDONLY + 1)
# endif
#endif
#endif	/* !MSDOS */

#ifndef	FD_SET
typedef struct fd_set {
	u_int fds_bits[1];
} fd_set;
# define	FD_ZERO(p)	(((p) -> fds_bits[0]) = 0)
# define	FD_SET(n, p)	(((p) -> fds_bits[0]) |= ((u_int)1 << (n)))
#endif	/* !FD_SET */

#define	MAXFDSET	256

#ifndef	LSI_C
#define	safe_dup	dup
#define	safe_dup2	dup2
#else
static int NEAR safe_dup __P_((int));
static int NEAR safe_dup2 __P_((int, int));
#endif
#if	MSDOS
static int NEAR newdup __P_((int));
#endif
static int NEAR err2 __P_((char *));
#if	!MSDOS
static char *NEAR tstrdup __P_((char *));
#endif
static int NEAR defaultterm __P_((VOID_A));
static int NEAR maxlocate __P_((int *, int *));
static int NEAR getnum __P_((CONST char *, int *));
static char *NEAR setchar __P_((int, char *, int *, int *));
static char *NEAR setint __P_((long, int,
		char *, int *, int *, int, int, int));
static char *NEAR setstr __P_((char *, char *, int *, int *, int, int, int));
#if	MSDOS
# ifdef	USEVIDEOBIOS
static int NEAR bioslocate __P_((int, int));
static int NEAR biosscroll __P_((int, int, int, int, int));
static int NEAR biosputch __P_((int, int));
static int NEAR bioscurstype __P_((int));
static int NEAR chgattr __P_((int));
static int NEAR evalcsi __P_((char *));
# endif
# if	!defined (DJGPP) || defined (NOTUSEBIOS) || defined (PC98)
static int NEAR dosgettime __P_((u_char []));
# endif
#else	/* !MSDOS */
# ifdef	USESGTTY
static int NEAR ttymode __P_((int, int, int, int));
# else
static int NEAR ttymode __P_((int, int, int, int, int, int, int, int));
# endif
static char *NEAR tgetstr2 __P_((char **, char *));
static char *NEAR tgetstr3 __P_((char **, char *, char *));
static char *NEAR tgetkeyseq __P_((int, char *));
static kstree_t *NEAR newkeyseqtree __P_((kstree_t *, int));
static int NEAR freekeyseqtree __P_((kstree_t *, int));
static int NEAR cmpkeyseq __P_((CONST VOID_P, CONST VOID_P));
static int NEAR sortkeyseq __P_((VOID_A));
static int putch3 __P_((tputs_t));
#endif	/* !MSDOS */

#ifdef	LSI_C
extern u_char _openfile[];
#endif

#if	!MSDOS && !defined (USETERMINFO)
# ifdef	NOTERMVAR
# define	T_EXTERN
# else
# define	T_EXTERN	extern
# endif
T_EXTERN short ospeed;
T_EXTERN char PC;
T_EXTERN char *BC;
T_EXTERN char *UP;
#endif
int n_column = -1;
int n_lastcolumn = -1;
int n_line = -1;
int stable_standout = 0;
char *termstr[MAXTERMSTR];
u_char cc_intr = K_CTRL('C');
u_char cc_quit = K_CTRL('\\');
#if	MSDOS
u_char cc_eof = K_CTRL('Z');
#else
u_char cc_eof = K_CTRL('D');
#endif
u_char cc_eol = 255;
u_char cc_erase = K_CTRL('H');
VOID_T (*keywaitfunc)__P_((VOID_A)) = NULL;
#if	!MSDOS
int usegetcursor = 0;
int suspended = 0;
#endif
int ttyio = -1;
int isttyiomode = 0;
FILE *ttyout = NULL;
int dumbterm = 0;

#if	MSDOS
# ifdef	PC98
#define	KEYBUFWORKSEG	0x00
#define	KEYBUFWORKSIZ	0x20
#define	KEYBUFWORKMIN	0x502
#define	KEYBUFWORKMAX	0x522
#define	KEYBUFWORKTOP	0x524
#define	KEYBUFWORKEND	0x526
#define	KEYBUFWORKCNT	0x528
static u_char specialkey[] = "\032:=<;89>\25667bcdefghijk\202\203\204\205\206\207\210\211\212\213?";
static u_char metakey[] = "\035-+\037\022 !\"\027#$%/.\030\031\020\023\036\024\026,\021*\025)";
# else	/* !PC98 */
#define	KEYBUFWORKSEG	0x40
#define	KEYBUFWORKSIZ	0x20
#define	KEYBUFWORKMIN	getkeybuf(0x80)
#define	KEYBUFWORKMAX	(KEYBUFWORKMIN + KEYBUFWORKSIZ)
#define	KEYBUFWORKTOP	(KEYBUFWORKMIN - 4)
#define	KEYBUFWORKEND	(KEYBUFWORKMIN - 2)
static u_char specialkey[] = "\003HPMKRSGOIQ;<=>?@ABCDTUVWXYZ[\\]\206";
static u_char metakey[] = "\0360. \022!\"#\027$%&21\030\031\020\023\037\024\026/\021-\025,";
# endif	/* !PC98 */
# if	defined (PC98) || defined (NOTUSEBIOS)
static int nextchar = '\0';
# endif
# ifdef	NOTUSEBIOS
static u_short keybuftop = 0;
# endif
# ifdef	USEVIDEOBIOS
static u_short videoattr = 0x07;
# endif
static int specialkeycode[] = {
	0,
	K_UP, K_DOWN, K_RIGHT, K_LEFT,
	K_IC, K_DC, K_HOME, K_END, K_PPAGE, K_NPAGE,
	K_F(1), K_F(2), K_F(3), K_F(4), K_F(5),
	K_F(6), K_F(7), K_F(8), K_F(9), K_F(10),
	K_F(11), K_F(12), K_F(13), K_F(14), K_F(15),
	K_F(16), K_F(17), K_F(18), K_F(19), K_F(20), K_HELP
};
#define	SPECIALKEYSIZ	((int)(sizeof(specialkeycode) / sizeof(int)))
#else	/* !MSDOS */
static char *dumblist[] = {"dumb", "un", "unknown"};
#define	DUMBLISTSIZE	(sizeof(dumblist) / sizeof(char *))
static keyseq_t keyseq[K_MAX - K_MIN + 1];
static kstree_t *keyseqtree = NULL;
static char *defkeyseq[K_MAX - K_MIN + 1] = {
	NULL,			/* K_NOKEY */
	"\033OB",		/* K_DOWN */
	"\033OA",		/* K_UP */
	"\033OD",		/* K_LEFT */
	"\033OC",		/* K_RIGHT */
	"\033[4~",		/* K_HOME */
	"\010",			/* K_BS */
	NULL,			/* K_F0 */
	"\033[11~",		/* K_F(1) */
	"\033[12~",		/* K_F(2) */
	"\033[13~",		/* K_F(3) */
	"\033[14~",		/* K_F(4) */
	"\033[15~",		/* K_F(5) */
	"\033[17~",		/* K_F(6) */
	"\033[18~",		/* K_F(7) */
	"\033[19~",		/* K_F(8) */
	"\033[20~",		/* K_F(9) */
	"\033[21~",		/* K_F(10) */
	"\033[23~",		/* K_F(11) */
	"\033[24~",		/* K_F(12) */
	"\033[25~",		/* K_F(13) */
	"\033[26~",		/* K_F(14) */
	"\033[28~",		/* K_F(15) */
	"\033[29~",		/* K_F(16) */
	"\033[31~",		/* K_F(17) */
	"\033[32~",		/* K_F(18) */
	"\033[33~",		/* K_F(19) */
	"\033[34~",		/* K_F(20) */
	"\033OP",		/* K_F(21) */
	"\033OQ",		/* K_F(22) */
	"\033OR",		/* K_F(23) */
	"\033OS",		/* K_F(24) */
	"\033OT",		/* K_F(25) */
	"\033OU",		/* K_F(26) */
	"\033OV",		/* K_F(27) */
	"\033OW",		/* K_F(28) */
	"\033OX",		/* K_F(29) */
	"\033OY",		/* K_F(30) */

	NULL,			/* 0447 */
	NULL,			/* 0450 */
	NULL,			/* 0451 */
	NULL,			/* 0452 */
	NULL,			/* 0453 */
	NULL,			/* 0454 */
	NULL,			/* 0455 */
	NULL,			/* 0456 */
	NULL,			/* 0457 */
	NULL,			/* 0460 */
	NULL,			/* 0461 */

	"\033Oj",		/* K_F('*') */
	"\033Ok",		/* K_F('+') */
	"\033Ol",		/* K_F(',') */
	"\033Om",		/* K_F('-') */
	"\033On",		/* K_F('.') */
	"\033Oo",		/* K_F('/') */
	"\033Op",		/* K_F('0') */
	"\033Oq",		/* K_F('1') */
	"\033Or",		/* K_F('2') */
	"\033Os",		/* K_F('3') */
	"\033Ot",		/* K_F('4') */
	"\033Ou",		/* K_F('5') */
	"\033Ov",		/* K_F('6') */
	"\033Ow",		/* K_F('7') */
	"\033Ox",		/* K_F('8') */
	"\033Oy",		/* K_F('9') */

	NULL,			/* 0502 */
	NULL,			/* 0503 */
	NULL,			/* 0504 */

	"\033OX",		/* K_F('=') */

	NULL,			/* 0506 */

	"\033OM",		/* K_F('?') */

	"",			/* K_DL */
	"",			/* K_IL */
	"\177",			/* K_DC */
	"\033[2~",		/* K_IC */
	NULL,			/* K_EIC */
	"",			/* K_CLR */
	NULL,			/* K_EOS */
	NULL,			/* K_EOL */
	NULL,			/* K_ESF */
	NULL,			/* K_ESR */
	"\033[5~",		/* K_PPAGE */
	"\033[6~",		/* K_NPAGE */
	NULL,			/* K_STAB */
	NULL,			/* K_CTAB */
	NULL,			/* K_CATAB */
	"\033[9~",		/* K_ENTER */
	NULL,			/* K_SRST */
	NULL,			/* K_RST */
	NULL,			/* K_PRINT */
	NULL,			/* K_LL */
	NULL,			/* K_A1 */
	NULL,			/* K_A3 */
	NULL,			/* K_B2 */
	NULL,			/* K_C1 */
	NULL,			/* K_C3 */
	NULL,			/* K_BTAB */
	NULL,			/* K_BEG */
	NULL,			/* K_CANC */
	NULL,			/* K_CLOSE */
	NULL,			/* K_COMM */
	NULL,			/* K_COPY */
	NULL,			/* K_CREAT */
	"\033[1~",		/* K_END */
	NULL,			/* K_EXIT */
	NULL,			/* K_FIND */
	NULL,			/* K_HELP */
};
#endif	/* !MSDOS */

#if	MSDOS || !defined (TIOCSTI)
static u_char ungetbuf[10];
static int ungetnum = 0;
#endif

static int termflags = 0;
#define	F_INITTTY	001
#define	F_TERMENT	002
#define	F_INITTERM	004
#define	F_TTYCHANGED	010
#define	F_RESETTTY	(F_INITTTY | F_TTYCHANGED)
static char *deftermstr[MAXTERMSTR] = {
#ifdef	PC98
	"\033[>1h",		/* t_init */
	"\033[>1l",		/* t_end */
	"\033)3",		/* t_metamode */
	"\033)0",		/* t_nometamode */
#else
	"",			/* t_init */
	"",			/* t_end */
	"",			/* t_metamode */
	"",			/* t_nometamode */
#endif
#if	MSDOS
	"",			/* t_scroll */
	"",			/* t_keypad */
	"",			/* t_nokeypad */
	"\033[>5l",		/* t_normalcursor */
	"\033[>5l",		/* t_highcursor */
	"\033[>5h",		/* t_nocursor */
	"\033[s",		/* t_setcursor */
	"\033[u",		/* t_resetcursor */
#else	/* !MSDOS */
	"\033[%i%d;%dr",	/* t_scroll */
# ifdef	BOW
	/* hack for bowpad */
	"",			/* t_keypad */
	"",			/* t_nokeypad */
	"",			/* t_normalcursor */
	"",			/* t_highcursor */
	"",			/* t_nocursor */
# else
	"\033[?1h\033=",	/* t_keypad */
	"\033[?1l\033>",	/* t_nokeypad */
	"\033[?25h",		/* t_normalcursor */
	"\033[?25h",		/* t_highcursor */
	"\033[?25l",		/* t_nocursor */
# endif
	"\0337",		/* t_setcursor */
	"\0338",		/* t_resetcursor */
#endif	/* !MSDOS */
	"\007",			/* t_bell */
	"\007",			/* t_vbell */
	"\033[;H\033[2J",	/* t_clear */
	"\033[m",		/* t_normal */
	"\033[1m",		/* t_bold */
	"\033[7m",		/* t_reverse */
	"\033[2m",		/* t_dim */
	"\033[5m",		/* t_blink */
	"\033[7m",		/* t_standout */
	"\033[4m",		/* t_underline */
	"\033[m",		/* end_standout */
	"\033[m",		/* end_underline */
	"\033[K",		/* l_clear */
	"\033[L",		/* l_insert */
	"\033[M",		/* l_delete */
	"",			/* c_insert */
	"",			/* c_delete */
#if	MSDOS
	"\033[%d;%dH",		/* c_locate */
#else
	"\033[%i%d;%dH",	/* c_locate */
#endif
	"\033[H",		/* c_home */
	"\r",			/* c_return */
	"\n",			/* c_newline */
	"\n",			/* c_scrollforw */
	"",			/* c_scrollrev */
	"\033[A",		/* c_up */
	"\012",			/* c_down */
	"\033[C",		/* c_right */
	"\010",			/* c_left */
	"\033[%dA",		/* c_nup */
	"\033[%dB",		/* c_ndown */
	"\033[%dC",		/* c_nright */
	"\033[%dD",		/* c_nleft */
};


#ifdef	LSI_C
static int NEAR safe_dup(oldd)
int oldd;
{
	int fd;

	if ((fd = dup(oldd)) < 0) return(-1);
	if (fd < SYS_OPEN && oldd >= 0 && oldd < SYS_OPEN)
		_openfile[fd] = _openfile[oldd];
	return(fd);
}

static int NEAR safe_dup2(oldd, newd)
int oldd, newd;
{
	int fd;

	if (oldd == newd) return(newd);
	if ((fd = dup2(oldd, newd)) < 0) return(-1);
	if (newd >= 0 && newd < SYS_OPEN && oldd >= 0 && oldd < SYS_OPEN)
		_openfile[newd] = _openfile[oldd];
	return(fd);
}
#endif	/* LSI_C */

int opentty(VOID_A)
{
#ifdef	SELECTRWONLY
	if (ttyio < 0 && (ttyio = open(TTYNAME, O_RDONLY, 0600)) < 0
#else
	if (ttyio < 0 && (ttyio = open(TTYNAME, O_RDWR, 0600)) < 0
#endif
	&& (ttyio = safe_dup(STDERR_FILENO)) < 0)
		err2("dup()");
	return(ttyio);
}

#if	MSDOS
static int NEAR newdup(fd)
int fd;
{
	struct stat st;
	int n;

	if (fd < 0
	|| fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
		return(fd);

	for (n = 20 - 1; n > fd; n--) if (fstat(n, &st) != 0) break;
	if (n <= fd || safe_dup2(fd, n) < 0) return(fd);
	close(fd);
	return(n);
}

int inittty(reset)
int reset;
{
#ifndef	DJGPP
	static u_char dupbrk;
	union REGS reg;
#endif
	static int dupin = -1;
	static int dupout = -1;
	int fd;

	if (!reset) {
		opentty();
#ifdef	NOTUSEBIOS
		if (!keybuftop) keybuftop = getkeybuf(KEYBUFWORKTOP);
#endif
#ifndef	DJGPP
		reg.x.ax = 0x3300;
		int86(0x21, &reg, &reg);
		dupbrk = reg.h.dl;
#endif
		if (!isatty(STDIN_FILENO)) {
			if (dupin < 0)
				dupin = newdup(safe_dup(STDIN_FILENO));
			if (dupin < 0 || safe_dup2(ttyio, STDIN_FILENO) < 0)
				err2("dup2()");
		}
		if (!isatty(STDOUT_FILENO)) {
			if (dupout < 0)
				dupout = newdup(safe_dup(STDOUT_FILENO));
			if (dupout < 0 || safe_dup2(ttyio, STDOUT_FILENO) < 0)
				err2("dup2()");
		}
		termflags |= F_INITTTY;
	}
	else {
		if (!(termflags & F_INITTTY)) return(0);
#ifndef	DJGPP
		reg.x.ax = 0x3301;
		reg.h.dl = dupbrk;
		int86(0x21, &reg, &reg);
#endif
		if (dupin >= 0) {
			fd = safe_dup2(dupin, STDIN_FILENO);
			close(dupin);
			dupin = -1;
			if (fd < 0) {
				termflags &= ~F_INITTTY;
				err2("dup2()");
			}
		}
		if (dupout >= 0) {
			fd = safe_dup2(dupout, STDOUT_FILENO);
			close(dupout);
			dupout = -1;
			if (fd < 0) {
				termflags &= ~F_INITTTY;
				err2("dup2()");
			}
		}
	}
	return(0);
}

int cooked2(VOID_A)
{
#ifndef	DJGPP
	union REGS reg;

	reg.x.ax = 0x3301;
	reg.h.dl = 1;
	int86(0x21, &reg, &reg);
#endif
	return(0);
}

int cbreak2(VOID_A)
{
#ifndef	DJGPP
	union REGS reg;

	reg.x.ax = 0x3301;
	reg.h.dl = 0;
	int86(0x21, &reg, &reg);
#endif
	return(0);
}

int raw2(VOID_A)
{
#ifndef	DJGPP
	union REGS reg;

	reg.x.ax = 0x3301;
	reg.h.dl = 0;
	int86(0x21, &reg, &reg);
#endif
	return(0);
}

int echo2(VOID_A)
{
	return(0);
}

int noecho2(VOID_A)
{
	return(0);
}

int nl2(VOID_A)
{
	return(0);
}

int nonl2(VOID_A)
{
	return(0);
}

int tabs(VOID_A)
{
	return(0);
}

int notabs(VOID_A)
{
	return(0);
}

int keyflush(VOID_A)
{
	__dpmi_regs reg;

	disable();
	reg.x.ax = 0x0c00;
	intdos2(&reg);
#ifdef	NOTUSEBIOS
	keybuftop = getkeybuf(KEYBUFWORKTOP);
#endif
	enable();
	return(0);
}

#else	/* !MSDOS */

int inittty(reset)
int reset;
{
#ifdef	USESGTTY
	static int dupttyflag;
	struct tchars cc;
#endif
	static termioctl_t dupttyio;
	termioctl_t tty;

	if (!reset) opentty();
	else if (ttyio < 0 || (termflags & F_RESETTTY) != F_RESETTTY)
		return(0);
	if (tioctl(ttyio, REQGETP, &tty) < 0) {
		termflags &= ~F_INITTTY;
		close(ttyio);
		ttyio = -1;
		err2("ioctl()");
	}
	if (!reset) {
		memcpy((char *)&dupttyio, (char *)&tty, sizeof(termioctl_t));
#ifdef	USESGTTY
		if (ioctl(ttyio, TIOCLGET, &dupttyflag) < 0
		|| ioctl(ttyio, TIOCGETC, &cc) < 0) {
			termflags &= ~F_INITTTY;
			err2("ioctl()");
		}
		cc_intr = cc.t_intrc;
		cc_quit = cc.t_quitc;
		cc_eof = cc.t_eofc;
		cc_eol = cc.t_brkc;
		if (cc_erase != 255) cc_erase = dupttyio.sg_erase;
#else
		cc_intr = dupttyio.c_cc[VINTR];
		cc_quit = dupttyio.c_cc[VQUIT];
		cc_eof = dupttyio.c_cc[VEOF];
		cc_eol = dupttyio.c_cc[VEOL];
		if (cc_erase != 255) cc_erase = dupttyio.c_cc[VERASE];
#endif
#ifndef	USETERMINFO
		ospeed = getspeed(dupttyio);
#endif
		termflags |= F_INITTTY;
	}
	else if (tioctl(ttyio, REQSETP, &dupttyio) < 0
#ifdef	USESGTTY
	|| ioctl(ttyio, TIOCLSET, &dupttyflag) < 0
#endif
	) {
		termflags &= ~F_INITTTY;
		close(ttyio);
		ttyio = -1;
		err2("ioctl()");
	}

	return(0);
}

#ifdef	USESGTTY
static int NEAR ttymode(set, reset, lset, lreset)
int set, reset, lset, lreset;
{
	termioctl_t tty;
	int lflag;

	if (!(termflags & F_INITTTY)) return(-1);
	if (ioctl(ttyio, TIOCLGET, &lflag) < 0) err2("ioctl()");
	if (tioctl(ttyio, REQGETP, &tty) < 0) err2("ioctl()");
	if (set) tty.sg_flags |= set;
	if (reset) tty.sg_flags &= ~reset;
	if (lset) lflag |= lset;
	if (lreset) lflag &= ~lreset;
	if (ioctl(ttyio, TIOCLSET, &lflag) < 0) err2("ioctl()");
#else
static int NEAR ttymode(set, reset, iset, ireset, oset, oreset, vmin, vtime)
int set, reset, iset, ireset, oset, oreset, vmin, vtime;
{
	termioctl_t tty;

	if (!(termflags & F_INITTTY)) return(-1);
	if (tioctl(ttyio, REQGETP, &tty) < 0) err2("ioctl()");
	if (set) tty.c_lflag |= set;
	if (reset) tty.c_lflag &= ~reset;
	if (iset) tty.c_iflag |= iset;
	if (ireset) tty.c_iflag &= ~ireset;
	if (oset) tty.c_oflag |= oset;
	if (oreset) tty.c_oflag &= ~oreset;
	if (vmin) {
		tty.c_cc[VMIN] = vmin;
		tty.c_cc[VTIME] = vtime;
	}
#endif
	if (tioctl(ttyio, REQSETP, &tty) < 0) err2("ioctl()");
	termflags |= F_TTYCHANGED;
	return(0);
}

int cooked2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(0, CBREAK | RAW, LPASS8, LLITOUT | LPENDIN);
#else
	ttymode(ISIG | ICANON | IEXTEN, PENDIN,
		BRKINT | IXON, IGNBRK | ISTRIP,
		OPOST, 0, VAL_VMIN, VAL_VTIME);
#endif
	return(0);
}

int cbreak2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(CBREAK, 0, LLITOUT, 0);
#else
	ttymode(ISIG | IEXTEN, ICANON, BRKINT | IXON, IGNBRK, OPOST, 0, 1, 0);
#endif
	return(0);
}

int raw2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(RAW, 0, LLITOUT, 0);
#else
	ttymode(0, ISIG | ICANON | IEXTEN,
		IGNBRK, BRKINT | IXON, 0, OPOST, 1, 0);
#endif
	return(0);
}

int echo2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ECHO, 0, LCRTBS | LCRTERA | LCRTKIL | LCTLECH, 0);
#else
	ttymode(ECHO | ECHOE | ECHOCTL | ECHOKE, ECHONL, 0, 0, 0, 0, 0, 0);
#endif
	return(0);
}

int noecho2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(0, ECHO, 0, LCRTBS | LCRTERA);
#else
	ttymode(0, ECHO | ECHOE | ECHOK | ECHONL, 0, 0, 0, 0, 0, 0);
#endif
	return(0);
}

int nl2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(CRMOD, 0, 0, 0);
#else
	ttymode(0, 0, ICRNL, 0, ONLCR, OCRNL | ONOCR | ONLRET, 0, 0);
#endif
	return(0);
}

int nonl2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(0, CRMOD, 0, 0);
#else
	ttymode(0, 0, 0, ICRNL, 0, ONLCR, 0, 0);
#endif
	return(0);
}

int tabs(VOID_A)
{
#ifdef	USESGTTY
	ttymode(0, XTABS, 0, 0);
#else
	ttymode(0, 0, 0, 0, 0, TAB3, 0, 0);
#endif
	return(0);
}

int notabs(VOID_A)
{
#ifdef	USESGTTY
	ttymode(XTABS, 0, 0, 0);
#else
	ttymode(0, 0, 0, 0, TAB3, 0, 0, 0);
#endif
	return(0);
}

int keyflush(VOID_A)
{
#ifdef	USESGTTY
	int i = FREAD;

	ioctl(ttyio, TIOCFLUSH, &i);
#else	/* !USESGTTY */
# ifdef	USETERMIOS
	tcflush(ttyio, TCIFLUSH);
# else
	ioctl(ttyio, TCFLSH, 0);
# endif
#endif	/* !USESGTTY */
	return(0);
}
#endif	/* !MSDOS */

int ttyiomode(isnl)
int isnl;
{
#if	MSDOS
	raw2();
#else	/* !MSDOS */
# ifdef	USESGTTY
	raw2();
	noecho2();
	nonl2();
	notabs();
# else	/* !USESGTTY */
	if (isnl) ttymode(0, (ISIG|ICANON|IEXTEN) | (ECHO|ECHOE|ECHOK|ECHONL),
		IGNBRK, (BRKINT|IXON) | ICRNL,
		OPOST | ONLCR | TAB3, 0, 1, 0);
	else ttymode(0, (ISIG|ICANON|IEXTEN) | (ECHO|ECHOE|ECHOK|ECHONL),
		IGNBRK, (BRKINT|IXON) | ICRNL,
		TAB3, OPOST | ONLCR, 1, 0);
# endif	/* !USESGTTY */
#endif	/* !MSDOS */
	if (!dumbterm) {
		putterms(t_keypad);
		tflush();
	}
	isttyiomode = isnl + 1;
	return(0);
}

int stdiomode(VOID_A)
{
	int isnl;

	isnl = (isttyiomode) ? isttyiomode - 1 : 0;
#if	MSDOS
	cooked2();
#else	/* !MSDOS */
# ifdef	USESGTTY
	cooked2();
	echo2();
	nl2();
	tabs();
	if (dumbterm > 2) ttymode(0, ECHO | CRMOD, 0, 0);
# else	/* !USESGTTY */
	if (isnl) ttymode((ISIG|ICANON|IEXTEN) | (ECHO|ECHOE|ECHOCTL|ECHOKE),
		PENDIN | ECHONL,
		(BRKINT|IXON) | ICRNL, (IGNBRK|ISTRIP),
		0, (OCRNL|ONOCR|ONLRET) | TAB3,
		VAL_VMIN, VAL_VTIME);
	else ttymode((ISIG|ICANON|IEXTEN) | (ECHO|ECHOE|ECHOCTL|ECHOKE),
		PENDIN | ECHONL,
		(BRKINT|IXON) | ICRNL, (IGNBRK|ISTRIP),
		OPOST | ONLCR, (OCRNL|ONOCR|ONLRET) | TAB3,
		VAL_VMIN, VAL_VTIME);
	if (dumbterm > 2) ttymode(0, ECHO,
		0, ICRNL, 0, ONLCR, VAL_VMIN, VAL_VTIME);
# endif	/* !USESGTTY */
#endif	/* !MSDOS */
	if (!dumbterm) {
		putterms(t_nokeypad);
		tflush();
	}
	isttyiomode = 0;
	return(isnl);
}

int termmode(init)
int init;
{
	static int mode = 0;
	int oldmode;

	oldmode = mode;
	if (init >= 0 && mode != init) {
		putterms((init) ? t_init : t_end);
		tflush();
		mode = init;
	}
	return(oldmode);
}

int exit2(no)
int no;
{
	if (!dumbterm && (termflags & F_TERMENT)) putterm(t_normal);
	endterm();
	inittty(1);
	keyflush();
#ifdef	DEBUG
# if	!MSDOS
	freeterment();
# endif
	if (ttyout && ttyout != stderr) {
		if (fileno(ttyout) == ttyio) ttyio = -1;
		fclose(ttyout);
	}
	if (ttyio >= 0) close(ttyio);
	muntrace();
#endif
	exit(no);
/*NOTREACHED*/
	return(0);
}

static int NEAR err2(mes)
char *mes;
{
	int duperrno;

	duperrno = errno;
	if (termflags & F_INITTTY) {
		if (!dumbterm && (termflags & F_TERMENT)) putterm(t_normal);
		endterm();
		cooked2();
		echo2();
		nl2();
		tabs();
	}
	if (dumbterm <= 2) fputc('\007', stderr);
	fputc('\n', stderr);
	fputs(mes, stderr);

	if (duperrno) {
		fputs(": ", stderr);
		fputs(strerror2(duperrno), stderr);
	}
	fputc('\n', stderr);
	inittty(1);
	exit(2);
/*NOTREACHED*/
	return(0);
}

#if	!MSDOS
static char *NEAR tstrdup(s)
char *s;
{
	char *cp;

	if (!s) s = "";
	if (!(cp = (char *)malloc(strlen(s) + 1))) err2("malloc()");
	strcpy(cp, s);
	return(cp);
}
#endif	/* !MSDOS */

static int NEAR defaultterm(VOID_A)
{
	int i;

	if (dumbterm > 1) {
#if	MSDOS
		for (i = 0; i < MAXTERMSTR; i++) termstr[i] = "";
#else	/* !MSDOS */
		for (i = 0; i < MAXTERMSTR; i++) termstr[i] = tstrdup(NULL);
# ifndef	USETERMINFO
		PC = '\0';
		BC = tstrdup(NULL);
		UP = tstrdup(NULL);
# endif
		for (i = 0; i <= K_MAX - K_MIN; i++) {
			if (!(defkeyseq[i])) keyseq[i].str = NULL;
			else keyseq[i].str = tstrdup(NULL);
		}
#endif	/* !MSDOS */
	}
	else {
#if	MSDOS
		for (i = 0; i < MAXTERMSTR; i++) termstr[i] = deftermstr[i];
#else	/* !MSDOS */
		for (i = 0; i < MAXTERMSTR; i++)
			termstr[i] = tstrdup(deftermstr[i]);
# ifndef	USETERMINFO
		PC = '\0';
		BC = tstrdup("\010");
		UP = tstrdup("\033[A");
# endif
		for (i = 0; i <= K_MAX - K_MIN; i++) {
			if (!(defkeyseq[i])) keyseq[i].str = NULL;
			else keyseq[i].str = tstrdup(defkeyseq[i]);
		}
#endif	/* !MSDOS */
	}

#if	!MSDOS
	for (i = 0; i <= K_MAX - K_MIN; i++) keyseq[i].code = K_MIN + i;
	for (i = 21; i <= 30; i++)
		keyseq[K_F(i) - K_MIN].code = K_F(i - 20) & 01000;
	for (i = 31; K_F(i) < K_DL; i++)
		if (defkeyseq[K_F(i) - K_MIN]) keyseq[K_F(i) - K_MIN].code = i;
	keyseq[K_F('?') - K_MIN].code = K_CR;
#endif	/* !MSDOS */
	return(0);
}

#if	MSDOS && defined (USEVIDEOBIOS)
static int NEAR maxlocate(yp, xp)
int *yp, *xp;
{
	*xp = getbiosbyte(SCRWIDTH);
	*yp = getbiosbyte(SCRHEIGHT) + 1;
	return(0);
}

int getxy(yp, xp)
int *yp, *xp;
{
	__dpmi_regs reg;

	reg.x.ax = 0x0300;
	reg.h.bh = getbiosbyte(CURRPAGE);
	intbios(VIDEOBIOS, &reg);
	*xp = reg.h.dl + 1;
	*yp = reg.h.dh + 1;
	return(0);
}

#else	/* !MSDOS || !USEVIDEOBIOS */

static int NEAR maxlocate(yp, xp)
int *yp, *xp;
{
# if	MSDOS
	char *cp;
# endif
	int i, sc;

	sc = (t_setcursor && *t_setcursor && t_resetcursor && *t_resetcursor);
	if (sc) putterms(t_setcursor);
# if	MSDOS
	if ((cp = tparamstr(c_locate, 0, 999))) {
		for (i = 0; cp[i]; i++) bdos(0x06, cp[i], 0);
		free(cp);
	}
	if ((cp = tparamstr(c_ndown, 999, 999))) {
		for (i = 0; cp[i]; i++) bdos(0x06, cp[i], 0);
		free(cp);
	}
# else
	locate(998, 998);
	tflush();
# endif
	i = getxy(yp, xp);
	if (sc) putterms(t_resetcursor);
	return(i);
}

int getxy(yp, xp)
int *yp, *xp;
{
	char *format, buf[sizeof(SIZEFMT) + 4];
	int i, j, k, tmp, count, *val[2];

	format = SIZEFMT;
	keyflush();
# if	MSDOS
	for (i = 0; i < sizeof(GETSIZE) - 1; i++) bdos(0x06, GETSIZE[i], 0);
# else
	if (!usegetcursor) return(-1);
	write(ttyio, GETSIZE, sizeof(GETSIZE) - 1);
# endif

	do {
		if (!kbhit2(WAITKEYPAD * 1000L)) return(-1);
# if	MSDOS
		buf[0] = bdos(0x07, 0x00, 0);
# else
		buf[0] = getch2();
# endif
	} while (buf[0] != format[0]);
	for (i = 1; i < sizeof(buf) - 1; i++) {
		if (!kbhit2(WAITKEYPAD * 1000L)) return(-1);
# if	MSDOS
		buf[i] = bdos(0x07, 0x00, 0);
# else
		buf[i] = getch2();
# endif
		if (buf[i] == format[sizeof(SIZEFMT) - 2]) break;
	}
	keyflush();
	buf[++i] = '\0';

	count = 0;
	val[0] = yp;
	val[1] = xp;

	for (i = j = 0; format[i] && buf[j]; i++) {
		if (format[i] == '%' && format[++i] == 'd' && count < 2) {
			tmp = 0;
			k = j;
			for (; buf[j]; j++) {
				if (!buf[j] || buf[j] == format[i + 1]) break;
				tmp = tmp * 10 + buf[j] - '0';
			}
			if (j == k) break;
			*val[count++] = tmp;
		}
		else if (format[i] != buf[j++]) break;
	}
	if (count != 2) return(-1);
	return(0);
}
#endif	/* !MSDOS || !USEVIDEOBIOS */

static int NEAR getnum(s, ptrp)
CONST char *s;
int *ptrp;
{
	int i, n;

	i = *ptrp;
	for (n = 0; isdigit(s[i]); i++) n = n * 10 + (s[i] - '0');
	if (i <= *ptrp) n = -1;
	*ptrp = i;
	return(n);
}

static char *NEAR setchar(n, buf, ptrp, sizep)
int n;
char *buf;
int *ptrp, *sizep;
{
	char *tmp;

	if (*ptrp + 1 >= *sizep) {
		*sizep += BUFUNIT;
		if ((tmp = (char *)realloc(buf, *sizep))) buf = tmp;
		else {
			free(buf);
			return(NULL);
		}
	}
	buf[(*ptrp)++] = n;
	return(buf);
}

static char *NEAR setint(n, base, buf, ptrp, sizep, flags, width, prec)
long n;
int base;
char *buf;
int *ptrp, *sizep, flags, width, prec;
{
	char num[MAXLONGWIDTH + 1];
	u_long ul;
	int i, len, sign, cap, bit;

	cap = 0;
	if (base >= 256) {
		cap++;
		base -= 256;
	}

	ul = (u_long)n;
	bit = 0;
	if (base != 10) {
		sign = 0;
		flags &= ~(VF_PLUS | VF_SPACE);
		i = 1;
		for (i = 1; i < base; i <<= 1) bit++;
		base--;
	}
	else if (n >= 0 || (flags & VF_UNSIGNED))
		sign = (flags & VF_PLUS) ? 1 : 0;
	else {
		sign = -1;
		n = -n;
	}
	if (sign || (flags & VF_SPACE)) width--;
	if ((flags & VF_ZERO) && prec < 0) prec = width;
	len = 0;
	while (len < sizeof(num) / sizeof(char)) {
		if (!bit) i = (((flags & VF_UNSIGNED) ? ul : n) % base) + '0';
		else if ((i = (ul & base)) < 10) i += '0';
		else if (cap) i += 'A' - 10;
		else i += 'a' - 10;

		num[len++] = i;
		if (bit) {
			ul >>= bit;
			if (!ul) break;
		}
		else if (flags & VF_UNSIGNED) {
			ul /= base;
			if (!ul) break;
		}
		else {
			n /= base;
			if (!n) break;
		}
	}
	if (!(flags & VF_MINUS) && width >= 0 && len < width) {
		while (len < width--) {
			if (prec >= 0 && prec > width) break;
			if (!(buf = setchar(' ', buf, ptrp, sizep)))
				return(NULL);
		}
	}

	i = '\0';
	if (sign) i = (sign > 0) ? '+' : '-';
	else if (flags & VF_SPACE) i = ' ';
	if (i && !(buf = setchar(i, buf, ptrp, sizep))) return(NULL);

	if (prec >= 0 && len < prec) {
		while (len < prec--) {
			width--;
			if (!(buf = setchar('0', buf, ptrp, sizep)))
				return(NULL);
		}
	}
	for (i = 0; i < len; i++) {
		if (!(buf = setchar(num[len - i - 1], buf, ptrp, sizep)))
			return(NULL);
	}

	if (width >= 0) for (; i < width; i++) {
		if (!(buf = setchar(' ', buf, ptrp, sizep))) return(NULL);
	}

	return(buf);
}

static char *NEAR setstr(s, buf, ptrp, sizep, flags, width, prec)
char *s, *buf;
int *ptrp, *sizep, flags, width, prec;
{
	int i, len;

	if (s) {
		len = strlen(s);
		if (prec >= 0 && len > prec) len = prec;
	}
	else {
		s = "(null)";
		len = strlen(s);
		if (prec >= 0 && len > prec) len = 0;
	}

	if (!(flags & VF_MINUS) && width >= 0 && len < width) {
		while (len < width--) {
			if (!(buf = setchar(' ', buf, ptrp, sizep)))
				return(NULL);
		}
	}
	for (i = 0; i < len; i++) {
		if (!(buf = setchar(s[i], buf, ptrp, sizep)))
			return(NULL);
	}

	if (width >= 0) for (; i < width; i++) {
		if (!(buf = setchar(' ', buf, ptrp, sizep)))
			return(NULL);
	}

	return(buf);
}

int vasprintf2(sp, fmt, args)
char **sp;
CONST char *fmt;
va_list args;
{
	char *buf;
	long l;
	int i, j, base, size, flags, width, prec;

	if (!(buf = (char *)malloc(size = BUFUNIT))) return(-1);
	for (i = j = 0; fmt[i]; i++) {
		if (fmt[i] != '%') {
			if (!(buf = setchar(fmt[i], buf, &j, &size)))
				return(-1);
			continue;
		}

		i++;
		flags = 0;
		width = prec = -1;
		for (;;) {
			if (fmt[i] == '+') flags |= VF_PLUS;
			else if (fmt[i] == '-') flags |= VF_MINUS;
			else if (fmt[i] == ' ') flags |= VF_SPACE;
			else if (fmt[i] == '0') flags |= VF_ZERO;
			else break;

			i++;
		}

		if (fmt[i] != '*') width = getnum(fmt, &i);
		else {
			i++;
			width = (int)va_arg(args, int);
		}
		if (fmt[i] == '.') {
			i++;
			if (fmt[i] != '*') prec = getnum(fmt, &i);
			else {
				i++;
				prec = va_arg(args, int);
			}
		}
		if (fmt[i] == 'l') {
			i++;
			flags |= VF_LONG;
		}

		base = 0;
		switch (fmt[i]) {
			case 'd':
				base = 10;
				break;
			case 's':
				buf = setstr(va_arg(args, char *),
					buf, &j, &size, flags, width, prec);
				break;
			case 'c':
				buf = setchar(va_arg(args, int),
					buf, &j, &size);
				break;
			case 'p':
				flags |= VF_LONG;
/*FALLTHRU*/
			case 'x':
				base = 16;
				break;
			case 'X':
				base = 16 + 256;
				break;
			case 'o':
				base = 8;
				break;
			case 'u':
				flags |= VF_UNSIGNED;
				base = 10;
				break;
			default:
				buf = setchar(fmt[i], buf, &j, &size);
				break;
		}

		if (base) {
			if (flags & VF_LONG) l = va_arg(args, long);
			else l = (long)va_arg(args, int);
			buf = setint(l, base,
				buf, &j, &size, flags, width, prec);
		}

		if (!buf) return(-1);
	}
	va_end(args);

	buf[j] = '\0';
	if (sp) *sp = buf;
	else free(buf);
	return(j);
}

#ifdef	USESTDARGH
/*VARARGS2*/
int asprintf2(char **sp, CONST char *fmt, ...)
#else
/*VARARGS2*/
int asprintf2(sp, fmt, va_alist)
char **sp;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	int n;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	n = vasprintf2(sp, fmt, args);
	va_end(args);
	if (n < 0) err2("malloc()");
	return(n);
}

#if	MSDOS
char *tparamstr(s, arg1, arg2)
char *s;
int arg1, arg2;
{
	char *cp;

	asprintf2(&cp, s, arg1, arg2);
	return(cp);
}

/*ARGSUSED*/
int getterment(s)
char *s;
{
	if (termflags & F_TERMENT) return(-1);
	defaultterm();
	if (n_column < 0) n_column = 80;
	n_lastcolumn = n_column - 1;
	if (n_line < 0) n_line = 25;
	termflags |= F_TERMENT;
	return(0);
}

#else	/* !MSDOS */

char *tparamstr(s, arg1, arg2)
char *s;
int arg1, arg2;
{
	char *buf;
# ifdef	USETERMINFO
#  ifdef	DEBUG
	if (!s) return(NULL);
	_mtrace_file = "tparm(start)";
	s = tparm(s, arg1, arg2, 0, 0, 0, 0, 0, 0, 0);
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "tparm(end)";
		malloc(0);	/* dummy alloc */
	}
	if (!s) return(NULL);
#  else
	if (!s || !(s = tparm(s, arg1, arg2, 0, 0, 0, 0, 0, 0, 0)))
		return(NULL);
#  endif
	buf = tstrdup(s);
# else	/* !USETERMINFO */
	char *tmp;
	int i, j, n, len, size, args[2];

	if (!s) return(NULL);
	if (!(buf = (char *)malloc(size = BUFUNIT))) err2("malloc()");
	args[0] = arg1;
	args[1] = arg2;

	for (i = j = n = 0; s[i]; i++) {
		tmp = NULL;
		if (s[i] != '%' || s[++i] == '%') {
			if (!(buf = setchar(s[i], buf, &j, &size)))
				err2("realloc()");
		}
		else if (n >= 2) {
			free(buf);
			return(NULL);
		}
		else switch (s[i]) {
			case 'd':
				asprintf2(&tmp, "%d", args[n++]);
				break;
			case '2':
				asprintf2(&tmp, "%02d", args[n++]);
				break;
			case '3':
				asprintf2(&tmp, "%03d", args[n++]);
				break;
			case '.':
				asprintf2(&tmp, "%c", args[n++]);
				break;
			case '+':
				if (!s[++i]) {
					free(buf);
					return(NULL);
				}
				asprintf2(&tmp, "%c", args[n++] + s[i]);
				break;
			case '>':
				if (!s[++i] || !s[i + 1]) {
					free(buf);
					return(NULL);
				}
				if (args[n] > s[i++]) args[n] += s[i];
				break;
			case 'r':
				len = args[0];
				args[0] = args[1];
				args[1] = len;
				break;
			case 'i':
				args[0]++;
				args[1]++;
				break;
			case 'n':
				args[0] ^= 0140;
				args[1] ^= 0140;
				break;
			case 'B':
				args[n] = ((args[n] / 10) << 4)
					| (args[n] % 10);
				break;
			case 'D':
				args[n] -= 2 * (args[n] % 16);
				break;
			default:
				free(buf);
				return(NULL);
/*NOTREACHED*/
				break;
		}

		if (tmp) {
			len = strlen(tmp);
			if (j + len + 1 >= size) {
				size += BUFUNIT;
				if (!(buf = (char *)realloc(buf, size)))
					err2("realloc()");
			}
			memcpy(&(buf[j]), tmp, len);
			free(tmp);
			j += len;
		}
	}

	buf[j] = '\0';
# endif	/* !USETERMINFO */
	return(buf);
}

static char *NEAR tgetstr2(term, s)
char **term, *s;
{
# ifndef	USETERMINFO
	char strbuf[TERMCAPSIZE];
	char *p;

	p = strbuf;
	s = tgetstr(s, &p);
# endif
	if (s) {
		if (*term) free(*term);
		*term = tstrdup(s);
	}
	return(*term);
}

static char *NEAR tgetstr3(term, str1, str2)
char **term, *str1, *str2;
{
# ifdef	USETERMINFO
	if (str1) str1 = tstrdup(str1);
	else str1 = tparamstr(str2, 1, 1);
# else
	char *p, strbuf[TERMCAPSIZE];

	p = strbuf;

	if ((str1 = tgetstr(str1, &p))) str1 = tstrdup(str1);
	else str1 = tparamstr(tgetstr(str2, &p), 1, 1);
# endif
	if (str1) {
		if (*term) free(*term);
		*term = str1;
	}
	return(*term);
}

static char *NEAR tgetkeyseq(n, s)
int n;
char *s;
{
	char *cp;
	int i, j;

	n -= K_MIN;
	cp = NULL;
	if (!tgetstr2(&cp, s)) return(NULL);
	if (keyseq[n].str) {
		free(keyseq[n].str);
		keyseq[n].str = NULL;
	}
	for (i = 0; i <= K_MAX - K_MIN; i++) {
		if (!(keyseq[i].str)) continue;
		for (j = 0; cp[j]; j++)
			if ((cp[j] & 0x7f) != (keyseq[i].str[j] & 0x7f))
				break;
		if (!cp[j]) {
			free(keyseq[i].str);
			keyseq[i].str = NULL;
		}
	}
	keyseq[n].str = cp;
	return(cp);
}

static kstree_t *NEAR newkeyseqtree(parent, num)
kstree_t *parent;
int num;
{
	kstree_t *new;
	int i, n;

	if (!parent || !(parent -> next))
		new = (kstree_t *)malloc(sizeof(kstree_t) * num);
	else new = (kstree_t *)realloc(parent -> next, sizeof(kstree_t) * num);
	if (!new) err2("realloc()");

	if (!parent) n = 0;
	else {
		n = parent -> num;
		parent -> num = num;
		parent -> next = new;
	}

	for (i = n; i < num; i++) {
		new[i].key = -1;
		new[i].num = 0;
		new[i].next = (kstree_t *)NULL;
	}

	return(new);
}

static int NEAR freekeyseqtree(list, n)
kstree_t *list;
int n;
{
	int i;

	if (!list) return(-1);
	for (i = list[n].num - 1; i >= 0; i--) freekeyseqtree(list[n].next, i);
	if (!n) free(list);
	return(0);
}

static int NEAR cmpkeyseq(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	if (!((keyseq_t *)vp1) -> str) return(-1);
	if (!((keyseq_t *)vp2) -> str) return(1);
	return(strcmp(((keyseq_t *)vp1) -> str, ((keyseq_t *)vp2) -> str));
}

static int NEAR sortkeyseq(VOID_A)
{
	kstree_t *p;
	int i, j, k;

	qsort(keyseq, K_MAX - K_MIN + 1, sizeof(keyseq_t), cmpkeyseq);
	if (keyseqtree) freekeyseqtree(keyseqtree, 0);

	keyseqtree = newkeyseqtree(NULL, 1);

	for (i = 0; i <= K_MAX - K_MIN; i++) {
		p = keyseqtree;
		for (j = 0; j < keyseq[i].len; j++) {
			for (k = 0; k < p -> num; k++) {
				if (keyseq[i].str[j]
				== keyseq[p -> next[k].key].str[j]) break;
			}
			if (k >= p -> num) {
				newkeyseqtree(p, k + 1);
				p -> next[k].key = i;
			}
			p = &(p -> next[k]);
		}
	}

	return(0);
}

int getterment(s)
char *s;
{
# ifdef	IRIX
	/* for STUPID winterm entry */
	int winterm;
# endif
# ifndef	USETERMINFO
	char buf[TERMCAPSIZE];
# endif
	char *cp, *term;
	int i, j, dumb, dupdumbterm;

	if (termflags & F_TERMENT) return(-1);
	if (!ttyout) {
		if (!(ttyout = fdopen(ttyio, "w+"))
		&& !(ttyout = fopen(TTYNAME, "w+"))) ttyout = stderr;
	}
	dupdumbterm = dumbterm;
	dumbterm = dumb = 0;
	term = (s) ? s : (char *)getenv("TERM");
	if (!term || !*term) {
		dumbterm = 1;
		dumb = DUMBLISTSIZE;
	}
	else {
		for (i = 0; i < DUMBLISTSIZE; i++)
			if (!strcmp(term, dumblist[i])) break;
		if (i < DUMBLISTSIZE) dumbterm = 1;
	}
# ifdef	IRIX
	winterm = !strncmp(term, WINTERMNAME, sizeof(WINTERMNAME) - 1);
# endif

	for (;;) {
		if (dumb) term = dumblist[--dumb];
# ifdef	USETERMINFO
#  ifdef	DEBUG
		_mtrace_file = "setupterm(start)";
		setupterm(term, fileno(ttyout), &i);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "setupterm(end)";
			malloc(0);	/* dummy alloc */
		}
#  else
		setupterm(term, fileno(ttyout), &i);
#  endif
		if (i == 1) break;
# else	/* !USETERMINFO */
#  ifdef	DEBUG
		_mtrace_file = "tgetent(start)";
		i = tgetent(buf, term);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "tgetent(end)";
			malloc(0);	/* dummy malloc */
		}
#  else
		i = tgetent(buf, term);
#  endif
		if (i > 0) break;
# endif	/* !USETERMINFO */
		else if (dumb) continue;	/* retry for libncurses */
		else if (s) {
			dumbterm = 2;
			break;
		}
		else {
			errno = 0;
# ifdef	USETERMINFO
			err2("No TERMINFO is prepared");
# else
			err2("No TERMCAP is prepared");
# endif
		}
	}

	if (dupdumbterm >= 2 || !strcmp(term, "emacs") || getenv("EMACS"))
		dumbterm = 3;

	defaultterm();
	termflags |= F_TERMENT;
	if (dumbterm > 1) {
		for (i = 0; i <= K_MAX - K_MIN; i++)
			keyseq[i].len = (keyseq[i].str)
				? strlen(keyseq[i].str) : 0;
		sortkeyseq();
		return(-1);
	}

# ifndef	USETERMINFO
	cp = NULL;
	if (tgetstr2(&cp, TERM_pc)) {
		PC = *cp;
		free(cp);
	}
	tgetstr2(&BC, TERM_bc);
	tgetstr2(&UP, TERM_up);
# endif

	cp = NULL;
	if (tgetstr2(&cp, TERM_ku) || tgetstr2(&cp, TERM_kd)
	|| tgetstr2(&cp, TERM_kr) || tgetstr2(&cp, TERM_kl)) {
		free(cp);
		*t_keypad = *t_nokeypad = '\0';
		for (i = K_DOWN; i <= K_RIGHT; i++) {
			if (keyseq[i - K_MIN].str) {
				 free(keyseq[i - K_MIN].str);
				 keyseq[i - K_MIN].str = NULL;
			}
		}
	}

	if (n_column < 0 && (n_column = tgetnum2(TERM_co)) < 0) n_column = 80;
	n_lastcolumn = n_column;
	if (!tgetflag2(TERM_xn)) n_lastcolumn--;
	if (n_line < 0 && (n_line = tgetnum2(TERM_li)) < 0) n_column = 24;
	stable_standout = tgetflag2(TERM_xs);
	if (dumbterm < 2 && tgetflag2(TERM_gn)) dumbterm = 2;
	tgetstr2(&t_init, TERM_ti);
	tgetstr2(&t_end, TERM_te);
	tgetstr2(&t_metamode, TERM_mm);
	tgetstr2(&t_nometamode, TERM_mo);
	tgetstr2(&t_scroll, TERM_cs);
	tgetstr2(&t_keypad, TERM_ks);
	tgetstr2(&t_nokeypad, TERM_ke);
# ifdef	IRIX
	if (winterm);
	else
# endif
	{
		tgetstr2(&t_normalcursor, TERM_ve);
		tgetstr2(&t_highcursor, TERM_vs);
		tgetstr2(&t_nocursor, TERM_vi);
	}
	tgetstr2(&t_setcursor, TERM_sc);
	tgetstr2(&t_resetcursor, TERM_rc);
	tgetstr2(&t_bell, TERM_bl);
	tgetstr2(&t_vbell, TERM_vb);
	tgetstr2(&t_clear, TERM_cl);
	tgetstr2(&t_normal, TERM_me);
	tgetstr2(&t_bold, TERM_md);
	tgetstr2(&t_reverse, TERM_mr);
	tgetstr2(&t_dim, TERM_mh);
	tgetstr2(&t_blink, TERM_mb);
	tgetstr2(&t_standout, TERM_so);
	tgetstr2(&t_underline, TERM_us);
	tgetstr2(&end_standout, TERM_se);
	tgetstr2(&end_underline, TERM_ue);
	tgetstr2(&l_clear, TERM_ce);
	tgetstr2(&l_insert, TERM_al);
	tgetstr2(&l_delete, TERM_dl);
	tgetstr3(&c_insert, TERM_ic, TERM_IC);
	tgetstr3(&c_delete, TERM_dc, TERM_DC);
	tgetstr2(&c_locate, TERM_cm);
	tgetstr2(&c_home, TERM_ho);
	tgetstr2(&c_return, TERM_cr);
	tgetstr2(&c_newline, TERM_nl);
	tgetstr2(&c_scrollforw, TERM_sf);
	tgetstr2(&c_scrollrev, TERM_sr);
	tgetstr3(&c_up, TERM_up, TERM_UP);
	tgetstr3(&c_down, TERM_do, TERM_DO);
	tgetstr3(&c_right, TERM_nd, TERM_RI);
	tgetstr3(&c_left, TERM_le, TERM_LE);
	tgetstr2(&c_nup, TERM_UP);
	tgetstr2(&c_ndown, TERM_DO);
	tgetstr2(&c_nright, TERM_RI);
	tgetstr2(&c_nleft, TERM_LE);

	tgetkeyseq(K_UP, TERM_ku);
	tgetkeyseq(K_DOWN, TERM_kd);
	tgetkeyseq(K_RIGHT, TERM_kr);
	tgetkeyseq(K_LEFT, TERM_kl);
	tgetkeyseq(K_HOME, TERM_kh);
	tgetkeyseq(K_BS, TERM_kb);
	tgetkeyseq(K_F(1), TERM_l1);
	tgetkeyseq(K_F(2), TERM_l2);
	tgetkeyseq(K_F(3), TERM_l3);
	tgetkeyseq(K_F(4), TERM_l4);
	tgetkeyseq(K_F(5), TERM_l5);
	tgetkeyseq(K_F(6), TERM_l6);
	tgetkeyseq(K_F(7), TERM_l7);
	tgetkeyseq(K_F(8), TERM_l8);
	tgetkeyseq(K_F(9), TERM_l9);
	tgetkeyseq(K_F(10), TERM_la);
	tgetkeyseq(K_F(11), TERM_F1);
	tgetkeyseq(K_F(12), TERM_F2);
	tgetkeyseq(K_F(13), TERM_F3);
	tgetkeyseq(K_F(14), TERM_F4);
	tgetkeyseq(K_F(15), TERM_F5);
	tgetkeyseq(K_F(16), TERM_F6);
	tgetkeyseq(K_F(17), TERM_F7);
	tgetkeyseq(K_F(18), TERM_F8);
	tgetkeyseq(K_F(19), TERM_F9);
	tgetkeyseq(K_F(20), TERM_FA);
	tgetkeyseq(K_F(21), TERM_k1);
	tgetkeyseq(K_F(22), TERM_k2);
	tgetkeyseq(K_F(23), TERM_k3);
	tgetkeyseq(K_F(24), TERM_k4);
	tgetkeyseq(K_F(25), TERM_k5);
	tgetkeyseq(K_F(26), TERM_k6);
	tgetkeyseq(K_F(27), TERM_k7);
	tgetkeyseq(K_F(28), TERM_k8);
	tgetkeyseq(K_F(29), TERM_k9);
	tgetkeyseq(K_F(30), TERM_k10);
	tgetkeyseq(K_F(30), TERM_k0);
	tgetkeyseq(K_DL, TERM_kL);
	tgetkeyseq(K_IL, TERM_kA);
	tgetkeyseq(K_DC, TERM_kD);
	tgetkeyseq(K_IC, TERM_kI);
	tgetkeyseq(K_CLR, TERM_kC);
	tgetkeyseq(K_EOL, TERM_kE);
	tgetkeyseq(K_PPAGE, TERM_kP);
	tgetkeyseq(K_NPAGE, TERM_kN);
# ifdef	IRIX
	if (winterm);
	else
# endif
	tgetkeyseq(K_ENTER, TERM_at8);
	tgetkeyseq(K_BEG, TERM_at1);
	tgetkeyseq(K_END, TERM_at7);

	for (i = 0; i <= K_MAX - K_MIN; i++) {
		if (!(keyseq[i].str)) keyseq[i].len = 0;
		else {
			for (j = 0; keyseq[i].str[j]; j++)
				keyseq[i].str[j] &= 0x7f;
			keyseq[i].len = j;
			if (cc_erase != 255 && j == 1
			&& keyseq[i].str[0] == cc_erase)
				cc_erase = 255;
		}
	}
	sortkeyseq();

	return(0);
}

int freeterment(VOID_A)
{
	int i;

	if (!(termflags & F_TERMENT)) return(-1);

# ifndef	USETERMINFO
	if (BC) free(BC);
	if (UP) free(UP);
# endif
	for (i = 0; i < MAXTERMSTR; i++) if (termstr[i]) {
		free(termstr[i]);
		termstr[i] = NULL;
	}
	for (i = 0; i <= K_MAX - K_MIN; i++)
		if (keyseq[i].str) free(keyseq[i].str);
	if (keyseqtree) {
		freekeyseqtree(keyseqtree, 0);
		keyseqtree = NULL;
	}

	termflags &= ~F_TERMENT;
	return(0);
}

int setkeyseq(n, str, len)
int n;
char *str;
int len;
{
	int i;

	for (i = 0; i <= K_MAX - K_MIN; i++) if (keyseq[i].code == n) {
		if (keyseq[i].str) free(keyseq[i].str);
		keyseq[i].str = str;
		keyseq[i].len = len;
		break;
	}
	if (i > K_MAX - K_MIN) return(-1);

	if (str) {
		for (i = 0; i <= K_MAX - K_MIN; i++) {
			if ((keyseq[i].code & 0777) == n
			|| !(keyseq[i].str) || keyseq[i].len != len)
				continue;
			if (!memcmp(str, keyseq[i].str, len)) {
				free(keyseq[i].str);
				keyseq[i].str = NULL;
				keyseq[i].len = 0;
			}
		}
		if (cc_erase != 255 && str[0] == cc_erase && !(str[1]))
			cc_erase = 255;
	}
	sortkeyseq();
	return(0);
}

char *getkeyseq(n, lenp)
int n, *lenp;
{
	int i;

	for (i = 0; i <= K_MAX - K_MIN; i++) if (keyseq[i].code == n) {
		if (lenp) *lenp = keyseq[i].len;
		return((keyseq[i].str) ? keyseq[i].str : "");
	}
	if (lenp) *lenp = 0;
	return(NULL);
}

keyseq_t *copykeyseq(list)
keyseq_t *list;
{
	int i;

	if (list) {
		for (i = 0; i <= K_MAX - K_MIN; i++) {
			keyseq[i].code = list[i].code;
			keyseq[i].len = list[i].len;
			if (keyseq[i].str) free(keyseq[i].str);
			if (!(list[i].len)) keyseq[i].str = NULL;
			else {
				keyseq[i].str =
					(char *)malloc(list[i].len + 1);
				if (!keyseq[i].str) err2("malloc()");
				memcpy(keyseq[i].str, list[i].str, list[i].len);
			}
		}
		sortkeyseq();
	}
	else {
		list = (keyseq_t *)malloc((K_MAX - K_MIN + 1)
			* sizeof(keyseq_t));
		if (!list) err2("malloc()");
		for (i = 0; i <= K_MAX - K_MIN; i++) {
			list[i].code = keyseq[i].code;
			list[i].len = keyseq[i].len;
			if (!(keyseq[i].len)) list[i].str = NULL;
			else {
				list[i].str =
					(char *)malloc(keyseq[i].len + 1);
				if (!list[i].str) err2("malloc()");
				memcpy(list[i].str, keyseq[i].str,
					keyseq[i].len);
			}
		}
	}

	return(list);
}

int freekeyseq(list)
keyseq_t *list;
{
	int i;

	if (!list) return(-1);
	for (i = 0; i <= K_MAX - K_MIN; i++)
		if (list[i].str) free(list[i].str);
	free(list);
	return(0);
}
#endif	/* !MSDOS */

int initterm(VOID_A)
{
	if (!(termflags & F_TERMENT)) getterment(NULL);
	if (!dumbterm) {
		putterms(t_keypad);
		termmode(1);
		tflush();
	}
	termflags |= F_INITTERM;
	return(0);
}

int endterm(VOID_A)
{
	if (!(termflags & F_INITTERM)) return(-1);
	if (!dumbterm) {
		putterms(t_nokeypad);
		termmode(0);
		tflush();
	}
	termflags &= ~F_INITTERM;
	return(0);
}

#if	MSDOS
# ifdef	USEVIDEOBIOS
static int NEAR bioslocate(x, y)
int x, y;
{
	__dpmi_regs reg;

	reg.x.ax = 0x0200;
	reg.h.bh = getbiosbyte(CURRPAGE);
	reg.h.dh = y - 1;
	reg.h.dl = x - 1;
	intbios(VIDEOBIOS, &reg);
	return(0);
}

static int NEAR biosscroll(d, sx, sy, ex, ey)
int d, sx, sy, ex, ey;
{
	__dpmi_regs reg;

	if (sx > ex || sy > ey) return(0);
	if (d >= 0) {
		reg.h.ah = 0x06;
		reg.h.al = d;
	}
	else {
		reg.h.ah = 0x07;
		reg.h.al = -d;
	}
	reg.h.bh = videoattr;
	reg.h.ch = sy - 1;
	reg.h.cl = sx - 1;
	reg.h.dh = ey - 1;
	reg.h.dl = ex - 1;
	intbios(VIDEOBIOS, &reg);
	return(0);
}

static int NEAR biosputch(c, n)
int c, n;
{
	__dpmi_regs reg;

	reg.h.ah = 0x09;
	reg.h.al = (c & 0xff);
	reg.h.bh = getbiosbyte(CURRPAGE);
	reg.h.bl = videoattr;
	reg.x.cx = n;
	intbios(VIDEOBIOS, &reg);
	return(c);
}

static int NEAR bioscurstype(n)
int n;
{
	__dpmi_regs reg;

	reg.x.ax = 0x0300;
	reg.h.bh = getbiosbyte(CURRPAGE);
	intbios(VIDEOBIOS, &reg);
	reg.x.ax = 0x0100;
	reg.x.cx &= 0x1f1f;
	reg.x.cx |= (n & 0x6000);
	intbios(VIDEOBIOS, &reg);
	return(0);
}

int putch2(c)
int c;
{
	static int needscroll = 0;
	int n, x, y, w, h;

	if (c == '\007') {
		bdos(0x06, '\007', 0);
		return(c);
	}

	getxy(&y, &x);
	w = getbiosbyte(SCRWIDTH);
	h = getbiosbyte(SCRHEIGHT) + 1;

	if (c == '\b') {
		if (x > 1) x--;
		needscroll = 0;
	}
	else if (c == '\t') {
		if (x >= w && y >= h && needscroll) {
			x = 1;
			biosscroll(1, 1, 1, w, h);
			bioslocate(x, y);
		}
		needscroll = 0;

		n = 8 - ((x - 1) % 8);
		if (x + n <= w) x += n;
		else {
			n = w - x + 1;
			x = 1;
			if (y < h) y++;
			else {
				x = w;
				needscroll = 1;
			}
		}
		biosputch(' ', n);
	}
	else if (c == '\r') {
		x = 1;
		needscroll = 0;
	}
	else if (c == '\n') {
		if (y < h) y++;
		else biosscroll(1, 1, 1, w, h);
		needscroll = 0;
	}
	else {
		if (x >= w && y >= h && needscroll) {
			x = 1;
			biosscroll(1, 1, 1, w, h);
			bioslocate(x, y);
		}
		needscroll = 0;

		if (x < w) x++;
		else {
			x = 1;
			if (y < h) y++;
			else {
				x = w;
				needscroll = 1;
			}
		}
		biosputch(c, 1);
	}

	bioslocate(x, y);
	return(c);
}

static int NEAR chgattr(n)
int n;
{
	switch (n) {
		case 0:
			videoattr = 0x07;
			break;
		case 1:
			videoattr |= 0x08;
			break;
		case 4:
			videoattr &= 0xf0;
			videoattr |= 0x01;
			break;
		case 5:
			videoattr |= 0x80;
			break;
		case 7:
			videoattr = 0x70;
			break;
		case 8:
			videoattr = 0x08;
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			videoattr &= 0xf0;
			n -= 30;
			if (n & 1) videoattr |= 0x04;
			if (n & 2) videoattr |= 0x02;
			if (n & 4) videoattr |= 0x01;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			videoattr &= 0x0f;
			n -= 40;
			if (n & 1) videoattr |= 0x40;
			if (n & 2) videoattr |= 0x20;
			if (n & 4) videoattr |= 0x10;
			break;
		default:
			break;
	}

	return(videoattr);
}

static int NEAR evalcsi(s)
char *s;
{
	static int savex, savey;
	int i, x, y, w, h, n1, n2;

	i = 0;
	if (s[i] == '>') {
		n1 = -1;

		if (isdigit(s[++i])) {
			n1 = s[i++] - '0';
			while (isdigit(s[i])) n1 = n1 * 10 + s[i++] - '0';
		}

		if (s[i] == 'h') {
			if (n1 == 5) bioscurstype(0x2000);
			else i = -1;
		}
		else if (s[i] == 'l') {
			if (n1 == 5) bioscurstype(0x0000);
			else i = -1;
		}
		else i = -1;
	}
	else {
		getxy(&y, &x);
		w = getbiosbyte(SCRWIDTH);
		h = getbiosbyte(SCRHEIGHT) + 1;
		n1 = n2 = -1;

		if (isdigit(s[i])) {
			n1 = s[i++] - '0';
			while (isdigit(s[i])) n1 = n1 * 10 + s[i++] - '0';
		}
		if (s[i] == ';' && isdigit(s[++i])) {
			n2 = s[i++] - '0';
			while (isdigit(s[i])) n2 = n2 * 10 + s[i++] - '0';
		}

		switch (s[i]) {
			case 'A':
				if (n1 <= 0) n1 = 1;
				if ((y -= n1) <= 0) y = 1;
				bioslocate(x, y);
				break;
			case 'B':
				if (n1 <= 0) n1 = 1;
				if ((y += n1) > h) y = h;
				bioslocate(x, y);
				break;
			case 'C':
				if (n1 <= 0) n1 = 1;
				if ((x += n1) > w) x = w;
				bioslocate(x, y);
				break;
			case 'D':
				if (n1 <= 0) n1 = 1;
				if ((x -= n1) <= 0) x = 1;
				bioslocate(x, y);
				break;
			case 'H':
			case 'f':
				x = n2;
				y = n1;
				if (x <= 0) x = 1;
				else if (x > w) x = w;
				if (y <= 0) y = 1;
				else if (y > h) y = h;
				bioslocate(x, y);
				break;
			case 'm':
				if (n1 < 0) chgattr(0);
				else {
					chgattr(n1);
					if (n2 >= 0) chgattr(n2);
				}
				break;
			case 's':
				savex = x;
				savey = y;
				break;
			case 'u':
				x = savex;
				y = savey;
				if (x <= 0) x = 1;
				else if (x > w) x = w;
				if (y <= 0) y = 1;
				else if (y > h) y = h;
				bioslocate(savex, savey);
				break;
			case 'L':
				if (n1 <= 0) n1 = 1;
				biosscroll(-n1, 1, y, w, h);
				bioslocate(1, y);
				break;
			case 'M':
				if (n1 <= 0) n1 = 1;
				biosscroll(n1, 1, y, w, h);
				bioslocate(1, y);
				break;
			case 'J':
				switch (n1) {
					case -1:
					case 0:
						biosscroll(0, x, y, w, y);
						biosscroll(0, 1, y + 1, w, h);
						break;
					case 1:
						biosscroll(0, 1, y, x, y);
						biosscroll(0, 1, 1, w, y - 1);
						break;
					case 2:
						biosscroll(0, 1, 1, w, h);
						bioslocate(1, 1);
						break;
					default:
						break;
				}
				break;
			case 'K':
				switch (n1) {
					case -1:
					case 0:
						biosscroll(0, x, y, w, y);
						break;
					case 1:
						biosscroll(0, 1, y, x, y);
						break;
					case 2:
						biosscroll(0, 1, y, w, y);
						break;
					default:
						break;
				}
				break;
			default:
				i = -1;
				break;
		}
	}

	return(i);
}

int cputs2(s)
char *s;
{
	int i, n;

	if (s) for (i = 0; s[i]; i++) {
		if (s[i] != '\033' || s[i + 1] != '[') putch2(s[i]);
		else if ((n = evalcsi(&s[i + 2])) >= 0) i += n + 2;
	}
	return(0);
}
# else	/* !USEVIDEOBIOS */

int putch2(c)
int c;
{
	bdos(0x06, c & 0xff, 0);
	return(c);
}

int cputs2(s)
char *s;
{
	int i, x, y;

	if (s) for (i = 0; s[i]; i++) {
		if (s[i] != '\t') {
			bdos(0x06, s[i], 0);
			continue;
		}

		keyflush();
		if (getxy(&y, &x) < 0) x = 1;
		do {
			bdos(0x06, ' ', 0);
		} while (x++ % 8);
	}
	return(0);
}
# endif	/* !USEVIDEOBIOS */

/*ARGSUSED*/
int kbhit2(usec)
u_long usec;
{
#if	defined (NOTUSEBIOS) || !defined (DJGPP) || (DJGPP < 2)
	union REGS reg;
#else
# ifndef	PC98
	fd_set readfds;
	struct timeval tv;
	int n, fd;
# endif
#endif

	if (ungetnum > 0) return(1);
#ifdef	NOTUSEBIOS
	if (nextchar) return(1);
	reg.x.ax = 0x4406;
	reg.x.bx = ttyio;
	putterms(t_metamode);
	int86(0x21, &reg, &reg);
	putterms(t_nometamode);
	return((reg.x.flags & 1) ? 0 : reg.h.al);
#else	/* !NOTUSEBIOS */
# ifdef	PC98
	if (nextchar) return(1);
	reg.h.ah = 0x01;
	int86(0x18, &reg, &reg);
	return(reg.h.bh != 0);
# else	/* !PC98 */
#  if	defined (DJGPP) && (DJGPP >= 2)
	fd = (ttyio < MAXFDSET || (n = safe_dup(ttyio)) < 0) ? ttyio : n;
	tv.tv_sec = usec / 1000000L;
	tv.tv_usec = usec % 1000000L;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	do {
		n = select(fd + 1, &readfds, NULL, NULL, &tv);
	} while (n < 0 && errno == EINTR);
	if (fd != ttyio) close(fd);
	if (n < 0) err2(TTYNAME);
	return(n);
#  else	/* !DJGPP || DJGPP < 2 */
	reg.h.ah = 0x01;
	int86(0x16, &reg, &reg);
	return((reg.x.flags & 0x40) ? 0 : 1);
#  endif	/* !DJGPP || DJGPP < 2 */
# endif	/* !PC98 */
#endif	/* !NOTUSEBIOS */
}

int getch2(VOID_A)
{
#ifdef	NOTUSEBIOS
	u_short key, top;
#endif
#if	defined (PC98) && !defined (NOTUSEBIOS)
	union REGS reg;
#endif
#if	defined (PC98) || defined (NOTUSEBIOS)
	int ch;

	if (nextchar) {
		ch = nextchar;
		nextchar = '\0';
		return(ch);
	}
#endif
	if (ungetnum > 0) return((int)ungetbuf[--ungetnum]);

#ifndef	NOTUSEBIOS
# ifdef	PC98
	reg.h.ah = 0x00;
	int86(0x18, &reg, &reg);

	if (!(ch = reg.h.al)) nextchar = reg.h.ah;
	return(ch);
# else	/* !PC98 */
	return(bdos(0x07, 0x00, 0) & 0xff);
# endif	/* !PC98 */
#else	/* NOTUSEBIOS */
	disable();
	top = keybuftop;
	for (;;) {
		if (top == getkeybuf(KEYBUFWORKEND)) {
			key = 0xffff;
			break;
		}
		key = getkeybuf(top);
# ifdef	PC98
		if ((ch = (key & 0xff))) {
			if (ch < 0x80 || (ch > 0x9f && ch < 0xe0)) break;
			key &= 0xff00;
			if (strchr(metakey, (key >> 8))) break;
		}
# else
		if ((ch = (key & 0xff)) && ch != 0xe0 && ch != 0xf0) break;
		key &= 0xff00;
# endif
		if (strchr(specialkey, (key >> 8))) break;
		if ((top += 2) >= KEYBUFWORKMAX) top = KEYBUFWORKMIN;
	}
	putterms(t_metamode);
	ch = (bdos(0x07, 0x00, 0) & 0xff);
	putterms(t_nometamode);
	keybuftop = getkeybuf(KEYBUFWORKTOP);
	if (!(key & 0xff)) {
		while (kbhit2(1000000L / SENSEPERSEC)) {
			if (keybuftop != getkeybuf(KEYBUFWORKTOP)) break;
			putterms(t_metamode);
			bdos(0x07, 0x00, 0);
			putterms(t_nometamode);
		}
		ch = '\0';
		nextchar = (key >> 8);
	}
	enable();
	return(ch);
#endif	/* NOTUSEBIOS */
}

#if	!defined (DJGPP) || defined (NOTUSEBIOS) || defined (PC98)
static int NEAR dosgettime(tbuf)
u_char tbuf[];
{
	union REGS reg;

	reg.x.ax = 0x2c00;
	intdos(&reg, &reg);
	tbuf[0] = reg.h.ch;
	tbuf[1] = reg.h.cl;
	tbuf[2] = reg.h.dh;
	return(0);
}
#endif

/*ARGSUSED*/
int getkey2(sig)
int sig;
{
#if	!defined (DJGPP) || defined (NOTUSEBIOS) || defined (PC98)
	static u_char tbuf1[3] = {0xff, 0xff, 0xff};
	u_char tbuf2[3];
#endif
#if	defined (DJGPP) && (DJGPP >= 2)
	static int count = SENSEPERSEC;
#endif
	int i, ch;

#if	defined (DJGPP) && !defined (NOTUSEBIOS) && !defined (PC98)
	do {
		i = kbhit2(1000000L / SENSEPERSEC);
# if	defined (DJGPP) && (DJGPP >= 2)
		if (sig && !(--count)) {
			count = SENSEPERSEC;
			raise(sig);
		}
# endif
		if (keywaitfunc) (*keywaitfunc)();
	} while (!i);
#else	/* !DJGPP || NOTUSEBIOS || PC98 */
	if (tbuf1[0] == 0xff) dosgettime(tbuf1);
	while (!kbhit2(1000000L / SENSEPERSEC)) {
		dosgettime(tbuf2);
		if (memcmp((char *)tbuf1, (char *)tbuf2, sizeof(tbuf1))) {
# if	!defined (DJGPP) || (DJGPP >= 2)
			if (sig) raise(sig);
# endif
			memcpy((char *)tbuf1, (char *)tbuf2, sizeof(tbuf1));
		}
		if (keywaitfunc) (*keywaitfunc)();
	}
#endif	/* !DJGPP || NOTUSEBIOS || PC98 */
	ch = getch2();

	if (ch) switch (ch) {
		case '\010':
			ch = K_BS;
			break;
		case '\177':
			ch = K_DC;
			break;
		default:
			break;
	}
	else
#if	defined (PC98) || defined (NOTUSEBIOS) \
|| (defined (DJGPP) && (DJGPP >= 2))
	if (kbhit2(WAITKEYPAD * 1000L))
#endif
	{
		ch = getch2();
		for (i = 0; i < SPECIALKEYSIZ; i++)
			if (ch == specialkey[i]) return(specialkeycode[i]);
		for (i = 0; i <= 'z' - 'a'; i++)
			if (ch == metakey[i]) return(('a' + i) | 0x80);
		ch = K_NOKEY;
	}
	return(ch);
}

int ungetch2(c)
int c;
{
	if (ungetnum >= sizeof(ungetbuf) / sizeof(u_char) - 1) return(EOF);
	ungetbuf[ungetnum++] = c;
	return(c);
}

/*ARGSUSED*/
int setscroll(s, e)
int s, e;
{
	return(0);
}

int locate(x, y)
int x, y;
{
	cprintf2(c_locate, ++y, ++x);
	return(0);
}

int tflush(VOID_A)
{
	return(0);
}

char *getwsize(xmax, ymax)
int xmax, ymax;
{
	int x, y;

	keyflush();
	if (maxlocate(&y, &x) < 0) x = y = -1;

	if (x > 0) {
		n_lastcolumn = (n_lastcolumn < n_column) ? x - 1 : x;
		n_column = x;
	}
	if (y > 0) n_line = y;

	if (n_column <= 0 || (xmax > 0 && n_column < xmax))
		return("Column size too small");
	if (n_line <= 0 || (ymax > 0 && n_line < ymax))
		return("Line size too small");

	return(NULL);
}

#else	/* !MSDOS */

int putch2(c)
int c;
{
	return(fputc(c, ttyout));
}

int cputs2(s)
char *s;
{
	if (!s) return(0);
	return(fputs(s, ttyout));
}

static int putch3(c)
tputs_t c;
{
	return(fputc(c & 0x7f, ttyout));
}

int putterm(s)
char *s;
{
	if (!s) return(0);
	return(tputs(s, 1, putch3));
}

int putterms(s)
char *s;
{
	if (!s) return(0);
	return(tputs(s, n_line, putch3));
}

int kbhit2(usec)
u_long usec;
{
# ifdef	NOSELECT
	return((usec) ? 1 : 0);
# else
	fd_set readfds;
	struct timeval tv;
	int n, fd;

	fd = (ttyio < MAXFDSET || (n = safe_dup(ttyio)) < 0) ? ttyio : n;
	tv.tv_sec = usec / 1000000L;
	tv.tv_usec = usec % 1000000L;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	do {
		n = select(fd + 1, &readfds, NULL, NULL, &tv);
	} while (n < 0 && errno == EINTR);
	if (fd != ttyio) close(fd);
	if (n < 0) err2(TTYNAME);
	return(n);
# endif
}

int getch2(VOID_A)
{
	u_char ch;
	int i;

	do {
		if (suspended) {
			ttyiomode((isttyiomode) ? isttyiomode - 1 : 0);
			suspended = 0;
		}
	} while ((i = read(ttyio, &ch, sizeof(u_char))) < 0 && errno == EINTR);
	if (i < sizeof(u_char)) return(EOF);
	return((int)ch);
}

int getkey2(sig)
int sig;
{
	static int count = SENSEPERSEC;
	kstree_t *p;
	int i, j, ch, key;

	do {
		key = kbhit2(1000000L / SENSEPERSEC);
		if (sig && !(--count)) {
			count = SENSEPERSEC;
			kill(getpid(), sig);
		}
		if (suspended) {
			ttyiomode((isttyiomode) ? isttyiomode - 1 : 0);
			suspended = 0;
		}
		if (keywaitfunc) (*keywaitfunc)();
# ifndef	TIOCSTI
		if (ungetnum > 0) return((int)ungetbuf[--ungetnum]);
# endif
	} while (!key);

	key = ch = getch2();
	if (cc_erase != 255 && key == cc_erase) return(K_BS);
	if (!(p = keyseqtree)) return(key);

	if (key == K_ESC) {
		if (!kbhit2(WAITKEYPAD * 1000L)) return(key);
		ch = getch2();
		if (isalpha(ch) && !kbhit2(WAITMETA * 1000L))
			return(ch | 0x80);
		for (j = 0; j < p -> num; j++)
			if (key == keyseq[p -> next[j].key].str[0]) break;
		if (j >= p -> num) return(key);
		p = &(p -> next[j]);
		if (keyseq[p -> key].len == 1)
			return(keyseq[p -> key].code & 0777);
	}
	else {
		for (j = 0; j < p -> num; j++)
			if (key == keyseq[p -> next[j].key].str[0]) break;
		if (j >= p -> num) return(key);
		p = &(p -> next[j]);
		if (keyseq[p -> key].len == 1)
			return(keyseq[p -> key].code & 0777);
		if (!kbhit2(WAITKEYPAD * 1000L)) return(key);
		ch = getch2();
	}

	for (i = 1; p && p -> next; i++) {
		for (j = 0; j < p -> num; j++)
			if (ch == keyseq[p -> next[j].key].str[i]) break;
		if (j >= p -> num) return(key);
		p = &(p -> next[j]);
		if (keyseq[p -> key].len == i + 1)
			return(keyseq[p -> key].code & 0777);
		if (!kbhit2(WAITKEYPAD * 1000L)) return(key);
		ch = getch2();
	}
	return(key);
}

int ungetch2(c)
int c;
{
# ifdef	TIOCSTI
	u_char ch;

	ch = c;
	ioctl(ttyio, TIOCSTI, &ch);
# else
	if (ungetnum >= sizeof(ungetbuf) / sizeof(u_char) - 1) return(EOF);
	ungetbuf[ungetnum++] = c;
# endif
	return(c);
}

int setscroll(s, e)
int s, e;
{
	char *cp;
	int sc;

	if ((cp = tparamstr(t_scroll, s, e))) {
		sc = (t_setcursor && *t_setcursor
			&& t_resetcursor && *t_resetcursor);
		if (sc) putterms(t_setcursor);
		putterms(cp);
		if (sc) putterms(t_resetcursor);
		free(cp);
	}
	return(0);
}

int locate(x, y)
int x, y;
{
# ifdef	DEBUG
	char *cp;

	_mtrace_file = "tgoto(start)";
	cp = tgoto2(c_locate, x, y);
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "tgoto(end)";
		malloc(0);	/* dummy malloc */
	}
	putterms(cp);
# else
	putterms(tgoto2(c_locate, x, y));
# endif
	return(0);
}

int tflush(VOID_A)
{
	fflush(ttyout);
	return(0);
}

char *getwsize(xmax, ymax)
int xmax, ymax;
{
# ifdef	TIOCGWINSZ
	struct winsize ws;
# else
#  ifdef	WIOCGETD
	struct uwdate ws;
#  else
#   ifdef	TIOCGSIZE
	struct ttysize ts;
#   endif
#  endif
# endif
	int x, y, tx, ty;

	x = y = -1;
# ifdef	TIOCGWINSZ
	if (ioctl(ttyio, TIOCGWINSZ, &ws) >= 0) {
		if (ws.ws_col > 0) x = ws.ws_col;
		if (ws.ws_row > 0) y = ws.ws_row;
	}
# else	/* !TIOCGWINSZ */
#  ifdef	WIOCGETD
	ws.uw_hs = ws.uw_vs = 0;
	if (ioctl(ttyio, WIOCGETD, &ws) >= 0) {
		if (ws.uw_width > 0 && ws.uw_hs > 0)
			x = ws.uw_width / ws.uw_hs;
		if (ws.uw_height > 0 && ws.uw_vs > 0)
			y = ws.uw_height / ws.uw_vs;
	}
#  else	/* !WIOCGETD */
#   ifdef	TIOCGSIZE
	if (ioctl(ttyio, TIOCGSIZE, &ts) >= 0) {
		if (ts.ts_cols > 0) x = ts.ts_cols;
		if (ts.ts_lines > 0) y = ts.ts_lines;
	}
#   endif
#  endif	/* !WIOCGETD */
# endif	/* !TIOCGWINSZ */

	if (dumbterm) /*EMPTY*/;
	else if (usegetcursor || x < 0 || y < 0) {
		setscroll(-1, -1);
		if (maxlocate(&ty, &tx) >= 0) {
			x = tx;
			y = ty;
# ifdef	TIOCGWINSZ
			if (ws.ws_col <= 0 || ws.ws_xpixel <= 0)
				ws.ws_xpixel = 0;
			else ws.ws_xpixel += (x - ws.ws_col)
					* (ws.ws_xpixel / ws.ws_col);
			if (ws.ws_row <= 0 || ws.ws_ypixel <= 0)
				ws.ws_ypixel = 0;
			else ws.ws_ypixel += (y - ws.ws_row)
					* (ws.ws_ypixel / ws.ws_row);
			ws.ws_col = x;
			ws.ws_row = y;
			ioctl(ttyio, TIOCSWINSZ, &ws);
# else	/* !TIOCGWINSZ */
#  ifdef	WIOCGETD
			if (ws.uw_hs > 0 && ws.uw_vs > 0) {
				ws.uw_width = x * ws.uw_hs;
				ws.uw_height = y * ws.uw_vs;
				ioctl(ttyio, WIOCSETD, &ws);
			}
#  else	/* !WIOCGETD */
#   ifdef	TIOCGSIZE
			ts.ts_cols = x;
			ts.ts_lines = y;
			ioctl(ttyio, TIOCSSIZE, &ts);
#   endif
#  endif	/* !WIOCGETD */
# endif	/* !TIOCGWINSZ */
		}
	}

	if (x > 0) {
		n_lastcolumn = (n_lastcolumn < n_column) ? x - 1 : x;
		n_column = x;
	}
	if (y > 0) n_line = y;

	if (n_column <= 0 || (xmax > 0 && n_column < xmax))
		return("Column size too small");
	if (n_line <= 0 || (ymax > 0 && n_line < ymax))
		return("Line size too small");

	if (xmax > 0 && ymax > 0) setscroll(-1, n_line - 1);
	return(NULL);
}
#endif	/* !MSDOS */

#ifdef	USESTDARGH
/*VARARGS1*/
int cprintf2(CONST char *fmt, ...)
#else
/*VARARGS1*/
int cprintf2(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *buf;
	int n;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	n = vasprintf2(&buf, fmt, args);
	va_end(args);
	if (n < 0) err2("malloc()");
	cputs2(buf);
	free(buf);
	return(n);
}

int chgcolor(color, reverse)
int color, reverse;
{
	if (!reverse) cprintf2("\033[%dm", color + ANSI_NORMAL);
	else if (color == ANSI_BLACK)
		cprintf2("\033[%dm\033[%dm",
			ANSI_WHITE + ANSI_NORMAL, color + ANSI_REVERSE);
	else cprintf2("\033[%dm\033[%dm",
			ANSI_BLACK + ANSI_NORMAL, color + ANSI_REVERSE);
	return(0);
}
