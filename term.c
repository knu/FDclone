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

#ifdef	USESTDARGH
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef	DEBUG
extern char *_mtrace_file;
#endif

#if	MSDOS
#include <dos.h>
#include <io.h>
# ifdef	PC98
# define	NOTUSEBIOS
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
# else	/* !DJGPP */
# include <signal.h>
# include <sys/types.h>
# include <sys/timeb.h>
typedef union REGS	__dpmi_regs;
# define	intdos2(rp)	int86(0x21, rp, rp)
# define	getkeybuf(o)	(*((u_short far *)MK_FP(KEYBUFWORKSEG, o)))
# define	putkeybuf(o, n)	(*((u_short far *)MK_FP(KEYBUFWORKSEG, o)) = n)
#  ifdef	LSI_C
static int _asm_sti __P_((char *));
static int _asm_cli __P_((char *));
#  define	enable()	_asm_sti("\n\tsti\n")
#  define	disable()	_asm_cli("\n\tcli\n")
#  endif
# endif	/* !DJGPP */
#define	TTYNAME		"CON"
#else	/* !MSDOS */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#define	TTYNAME		"/dev/tty"

typedef struct _keyseq_t {
	u_short code;
	u_char len;
	char *str;
} keyseq_t;

typedef struct _kstree_t {
	int key;
	int num;
	struct _kstree_t *next;
} kstree_t;

#ifdef	USETERMIOS
# ifdef	USETERMIO
# undef	USETERMIO
# endif
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

#if	!defined (USETERMIO) && !defined (USETERMIOS)
#define	USESGTTY
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
#endif	/* !MSDOS */

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include <errno.h>
#ifdef	NOERRNO
extern int errno;
#endif

#include "term.h"

#define	MAXPRINTBUF	255
#define	GETSIZE		"\033[6n"
#define	SIZEFMT		"\033[%d;%dR"

#if	!MSDOS
extern int tgetent __P_((char *, char *));
extern int tgetnum __P_((char *));
extern int tgetflag __P_((char *));
extern char *tgetstr __P_((char *, char **));
extern char *tgoto __P_((char *, int, int));
extern int tputs __P_((char *, int, int (*)__P_((int))));

#define	BUFUNIT		16
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
#define	not(x)		(~(x) & 0xffff)

#ifndef	FREAD
# ifdef	_FREAD
# define	FREAD	_FREAD
# else
# define	FREAD	(O_RDONLY + 1)
# endif
#endif

#ifndef	FD_SET
typedef struct fd_set {
	int fds_bits[1];
} fd_set;
# define	FD_ZERO(p)	(((p) -> fds_bits[0]) = 0)
# define	FD_SET(n, p)	(((p) -> fds_bits[0]) |= (1 << (n)))
#endif
#endif	/* !MSDOS */

static int NEAR err2 __P_((char *));
static int NEAR defaultterm __P_((VOID_A));
static int NEAR maxlocate __P_((int *, int *));
static int NEAR getxy __P_((int *, int *));
#if	MSDOS
# if	!defined (DJGPP) || defined (NOTUSEBIOS) || defined (PC98)
static int NEAR dosgettime __P_((u_char []));
# endif
#else	/* !MSDOS */
# ifdef	USESGTTY
static int NEAR ttymode __P_((int, u_short, u_short, u_short, u_short));
# else
static int NEAR ttymode __P_((int, u_short, u_short, u_short, u_short,
		u_short, u_short, int, int));
# endif
static char *NEAR tgetstr2 __P_((char **, char *));
static char *NEAR tgetstr3 __P_((char **, char *, char *));
static char *NEAR tgetkeyseq __P_((int, char *));
static kstree_t *NEAR newkeyseq __P_((kstree_t *, int));
static int NEAR freekeyseq __P_((kstree_t *, int));
static int NEAR cmpkeyseq __P_((CONST VOID_P, CONST VOID_P));
static int NEAR sortkeyseq __P_((VOID_A));
# ifdef	DEBUG
static int NEAR freeterment __P_((VOID_A));
# endif
#endif	/* !MSDOS */

short ospeed = 0;
char PC ='\0';
char *BC = NULL;
char *UP = NULL;
int n_column = 0;
int n_lastcolumn = 0;
int n_line = 0;
int stable_standout = 0;
char *t_init = NULL;
char *t_end = NULL;
char *t_metamode = NULL;
char *t_nometamode = NULL;
char *t_scroll = NULL;
char *t_keypad = NULL;
char *t_nokeypad = NULL;
char *t_normalcursor = NULL;
char *t_highcursor = NULL;
char *t_nocursor = NULL;
char *t_setcursor = NULL;
char *t_resetcursor = NULL;
char *t_bell = NULL;
char *t_vbell = NULL;
char *t_clear = NULL;
char *t_normal = NULL;
char *t_bold = NULL;
char *t_reverse = NULL;
char *t_dim = NULL;
char *t_blink = NULL;
char *t_standout = NULL;
char *t_underline = NULL;
char *end_standout = NULL;
char *end_underline = NULL;
char *l_clear = NULL;
char *l_insert = NULL;
char *l_delete = NULL;
char *c_insert = NULL;
char *c_delete = NULL;
char *c_store = NULL;
char *c_restore = NULL;
char *c_locate = NULL;
char *c_home = NULL;
char *c_return = NULL;
char *c_newline = NULL;
char *c_scrollforw = NULL;
char *c_scrollrev = NULL;
char *c_up = NULL;
char *c_down = NULL;
char *c_right = NULL;
char *c_left = NULL;
char *c_nup = NULL;
char *c_ndown = NULL;
char *c_nright = NULL;
char *c_nleft = NULL;
u_char cc_intr = CTRL('c');
u_char cc_quit = CTRL('\\');
u_char cc_eof = CTRL('d');
u_char cc_eol = 255;
VOID_T (*keywaitfunc)__P_((VOID_A)) = NULL;
#if	!MSDOS
int usegetcursor = 0;
#endif
int ttyio = -1;
FILE *ttyout = NULL;

#if	MSDOS
#ifdef	PC98
#define	KEYBUFWORKSEG	0x00
#define	KEYBUFWORKSIZ	0x20
#define	KEYBUFWORKMIN	0x502
#define	KEYBUFWORKMAX	0x522
#define	KEYBUFWORKTOP	0x524
#define	KEYBUFWORKEND	0x526
#define	KEYBUFWORKCNT	0x528
static u_char specialkey[] = "\032:=<;89>\25667bcdefghijk\202\203\204\205\206\207\210\211\212\213?";
static u_char metakey[] = "\035-+\037\022 !\"\027#$%/.\030\031\020\023\036\024\026,\021*\025)";
#else
#define	KEYBUFWORKSEG	0x40
#define	KEYBUFWORKSIZ	0x20
#define	KEYBUFWORKMIN	getkeybuf(0x80)
#define	KEYBUFWORKMAX	(KEYBUFWORKMIN + KEYBUFWORKSIZ)
#define	KEYBUFWORKTOP	(KEYBUFWORKMIN - 4)
#define	KEYBUFWORKEND	(KEYBUFWORKMIN - 2)
static u_char specialkey[] = "\003HPMKRSGOIQ;<=>?@ABCDTUVWXYZ[\\]\206";
static u_char metakey[] = "\0360. \022!\"#\027$%&21\030\031\020\023\037\024\026/\021-\025,";
#endif
#if	defined (PC98) || defined (NOTUSEBIOS)
static int nextchar = '\0';
#endif
#ifdef	NOTUSEBIOS
static u_short keybuftop = 0;
#endif
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
static keyseq_t keyseq[K_MAX - K_MIN + 1];
static kstree_t *keyseqtree = NULL;
#endif	/* !MSDOS */

#if	MSDOS || !defined (TIOCSTI)
static u_char ungetbuf[10];
static int ungetnum = 0;
#endif

static int termflags = 0;
#define	F_INITTTY	001
#define	F_TERMENT	002
#define	F_INITTERM	004


#if	MSDOS
int inittty(reset)
int reset;
{
	static int dupin = -1;
	static int dupout = -1;
	long l;
#ifndef	DJGPP
	static u_char dupbrk;
	union REGS reg;
#endif

	if (ttyio < 0 && (ttyio = open(TTYNAME, O_RDWR, 0600)) < 0)
		ttyio = dup(STDERR_FILENO);
	if (reset && !(termflags & F_INITTTY)) return(0);
	if (!reset) {
#ifdef	NOTUSEBIOS
		if (!keybuftop) keybuftop = getkeybuf(KEYBUFWORKTOP);
#endif
#ifndef	DJGPP
		reg.x.ax = 0x3300;
		int86(0x21, &reg, &reg);
		dupbrk = reg.h.dl;
#endif
		if (((!(l = ftell(stdin)) || l == -1)
		&& ((dupin = dup(STDIN_FILENO)) < 0
		|| dup2(ttyio, STDIN_FILENO) < 0))
		|| ((!(l = ftell(stdout)) || l == -1)
		&& ((dupout = dup(STDOUT_FILENO)) < 0
		|| dup2(ttyio, STDOUT_FILENO) < 0)))
			err2(NULL);
		termflags |= F_INITTTY;
	}
	else {
#ifndef	DJGPP
		reg.x.ax = 0x3301;
		reg.h.dl = dupbrk;
		int86(0x21, &reg, &reg);
#endif
		if ((dupin >= 0 && dup2(dupin, STDIN_FILENO) < 0)
		|| (dupout >= 0 && dup2(dupout, STDOUT_FILENO) < 0)) {
			termflags &= ~F_INITTTY;
			err2(NULL);
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

	if (ttyio < 0 && (ttyio = open(TTYNAME, O_RDWR, 0600)) < 0)
		ttyio = dup(STDERR_FILENO);
	if (reset && !(termflags & F_INITTTY)) return(0);
	if (tioctl(ttyio, REQGETP, &tty) < 0) {
		termflags &= ~F_INITTTY;
		err2(NULL);
	}
	if (!reset) {
		memcpy((char *)&dupttyio, (char *)&tty, sizeof(termioctl_t));
#ifdef	USESGTTY
		if (tioctl(ttyio, TIOCLGET, &dupttyflag) < 0
		|| tioctl(ttyio, TIOCGETC, &cc) < 0) {
			termflags &= ~F_INITTTY;
			err2(NULL);
		}
		cc_intr = cc.t_intrc;
		cc_quit = cc.t_quitc;
		cc_eof = cc.t_eofc;
		cc_eol = cc.t_brkc;
#else
		cc_intr = dupttyio.c_cc[VINTR];
		cc_quit = dupttyio.c_cc[VQUIT];
		cc_eof = dupttyio.c_cc[VEOF];
		cc_eol = dupttyio.c_cc[VEOL];
#endif
		ospeed = getspeed(dupttyio);
		termflags |= F_INITTTY;
	}
	else if (tioctl(ttyio, REQSETP, &dupttyio) < 0
#ifdef	USESGTTY
	|| tioctl(ttyio, TIOCLSET, &dupttyflag) < 0
#endif
	) {
		termflags &= ~F_INITTTY;
		err2(NULL);
	}

	return(0);
}

#ifdef	USESGTTY
static int NEAR ttymode(d, set, reset, lset, lreset)
int d;
u_short set, reset, lset, lreset;
{
	termioctl_t tty;
	int lflag;

	if (ioctl(d, TIOCLGET, &lflag) < 0) err2(NULL);
	if (tioctl(d, REQGETP, &tty) < 0) err2(NULL);
	if (set) tty.sg_flags |= set;
	if (reset) tty.sg_flags &= reset;
	if (set) lflag |= lset;
	if (lreset) lflag &= lreset;
	if (ioctl(d, TIOCLSET, &lflag) < 0) err2(NULL);
#else
static int NEAR ttymode(d, set, reset, iset, ireset, oset, oreset, min, time)
int d;
u_short set, reset, iset, ireset, oset, oreset;
int min, time;
{
	termioctl_t tty;

	if (tioctl(d, REQGETP, &tty) < 0) err2(NULL);
	if (set) tty.c_lflag |= set;
	if (reset) tty.c_lflag &= reset;
	if (iset) tty.c_iflag |= iset;
	if (ireset) tty.c_iflag &= ireset;
	if (oset) tty.c_oflag |= oset;
	if (oreset) tty.c_oflag &= oreset;
	if (min) {
		tty.c_cc[VMIN] = min;
		tty.c_cc[VTIME] = time;
	}
#endif
	if (tioctl(d, REQSETP, &tty) < 0) err2(NULL);
	return(0);
}

int cooked2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, 0, not(CBREAK | RAW), LPASS8, not(LLITOUT | LPENDIN));
#else
	ttymode(ttyio, ISIG | ICANON | IEXTEN, not(PENDIN),
		BRKINT | IXON, not(IGNBRK | ISTRIP),
# if	(VEOF == VMIN) || (VEOL == VTIME)
		OPOST, 0, '\004', 255);
# else
		OPOST, 0, 0, 0);
# endif
#endif
	return(0);
}

int cbreak2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, CBREAK, 0, LLITOUT, 0);
#else
	ttymode(ttyio, ISIG | IEXTEN, not(ICANON),
		BRKINT | IXON, not(IGNBRK), OPOST, 0, 1, 0);
#endif
	return(0);
}

int raw2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, RAW, 0, LLITOUT, 0);
#else
	ttymode(ttyio, 0, not(ISIG | ICANON | IEXTEN),
		IGNBRK, not(BRKINT | IXON), 0, not(OPOST), 1, 0);
#endif
	return(0);
}

int echo2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, ECHO, 0, LCRTBS | LCRTERA | LCRTKIL | LCTLECH, 0);
#else
	ttymode(ttyio, ECHO | ECHOE | ECHOCTL | ECHOKE, not(ECHONL),
		0, 0, 0, 0, 0, 0);
#endif
	return(0);
}

int noecho2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, 0, not(ECHO), 0, not(LCRTBS | LCRTERA));
#else
	ttymode(ttyio, 0, not(ECHO | ECHOE | ECHOK | ECHONL), 0, 0,
		0, 0, 0, 0);
#endif
	return(0);
}

int nl2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, CRMOD, 0, 0, 0);
#else
	ttymode(ttyio, 0, 0, ICRNL, 0,
		ONLCR, not(OCRNL | ONOCR | ONLRET), 0, 0);
#endif
	return(0);
}

int nonl2(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, 0, not(CRMOD), 0, 0);
#else
	ttymode(ttyio, 0, 0, 0, not(ICRNL), 0, not(ONLCR), 0, 0);
#endif
	return(0);
}

int tabs(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, 0, not(XTABS), 0, 0);
#else
	ttymode(ttyio, 0, 0, 0, 0, 0, not(TAB3), 0, 0);
#endif
	return(0);
}

int notabs(VOID_A)
{
#ifdef	USESGTTY
	ttymode(ttyio, XTABS, 0, 0, 0);
#else
	ttymode(ttyio, 0, 0, 0, 0, TAB3, 0, 0, 0);
#endif
	return(0);
}

int keyflush(VOID_A)
{
#ifdef	USESGTTY
	int i = FREAD;
	tioctl(ttyio, TIOCFLUSH, &i);
#else	/* !USESGTTY */
# ifdef	USETERMIOS
	tcflush(ttyio, TCIFLUSH);
# else
	tioctl(ttyio, TCFLSH, 0);
# endif
#endif	/* !USESGTTY */
	return(0);
}
#endif	/* !MSDOS */

int ttyiomode(VOID_A)
{
	raw2();
	noecho2();
	nonl2();
	notabs();
	return(0);
}

int stdiomode(VOID_A)
{
	cooked2();
	echo2();
	nl2();
	tabs();
	return(0);
}

int exit2(no)
int no;
{
	if (termflags & F_TERMENT) putterm(t_normal);
	endterm();
	inittty(1);
	keyflush();
#ifdef	DEBUG
# if	!MSDOS
	freeterment();
# endif
	if (ttyio >= 0) close(ttyio);
	if (ttyout && ttyout != stdout) fclose(ttyout);
	muntrace();
#endif
	exit(no);
	return(0);
}

static int NEAR err2(mes)
char *mes;
{
	int duperrno;

	duperrno = errno;
	if (termflags & F_TERMENT) putterm(t_normal);
	endterm();
	inittty(1);
	fprintf(stderr, "\007\n");
	errno = duperrno;
	if (mes) fprintf(stderr, "%s\n", mes);
	else perror(TTYNAME);
	exit(2);
	return(0);
}

static int NEAR defaultterm(VOID_A)
{
#if	!MSDOS
	int i;
#endif

	BC = "\010";
	UP = "\033[A";
	n_column = 80;
#if	MSDOS
	n_lastcolumn = 79;
	n_line = 25;
#else
	n_lastcolumn = 80;
	n_line = 24;
#endif
#ifdef	PC98
	t_init = "\033[>1h";
	t_end = "\033[>1l";
	t_metamode = "\033)3";
	t_nometamode = "\033)0";
#else
	t_init = "";
	t_end = "";
	t_metamode = "";
	t_nometamode = "";
#endif
#if	MSDOS
	t_scroll = "";
	t_keypad = "";
	t_nokeypad = "";
	t_normalcursor = "\033[>5l";
	t_highcursor = "\033[>5l";
	t_nocursor = "\033[>5h";
	t_setcursor = "\033[s";
	t_resetcursor = "\033[u";
#else	/* !MSDOS */
	t_scroll = "\033[%i%d;%dr";
	t_keypad = "\033[?1h\033=";
	t_nokeypad = "\033[?1l\033>";
	t_normalcursor = "\033[?25h";
	t_highcursor = "\033[?25h";
	t_nocursor = "\033[?25l";
	t_setcursor = "\0337";
	t_resetcursor = "\0338";
#endif	/* !MSDOS */
	t_bell = "\007";
	t_vbell = "\007";
	t_clear = "\033[;H\033[2J";
	t_normal = "\033[m";
	t_bold = "\033[1m";
	t_reverse = "\033[7m";
	t_dim = "\033[2m";
	t_blink = "\033[5m";
	t_standout = "\033[7m";
	t_underline = "\033[4m";
	end_standout = "\033[m";
	end_underline = "\033[m";
	l_clear = "\033[K";
	l_insert = "\033[L";
	l_delete = "\033[M";
	c_insert = "";
	c_delete = "";
#ifdef	MSDOS
	c_store = "\033[s";
	c_restore = "\033[u";
	c_locate = "\033[%d;%dH";
#else
	c_store = "\0337";
	c_restore = "\0338";
	c_locate = "\033[%i%d;%dH";
#endif
	c_home = "\033[H";
	c_return = "\r";
	c_newline = "\n";
	c_scrollforw = "\n";
	c_scrollrev = "";
	c_up = "\033[A";
	c_down = "\012";
	c_right = "\033[C";
	c_left = "\010";
	c_nup = "\033[%dA";
	c_ndown = "\033[%dB";
	c_nright = "\033[%dC";
	c_nleft = "\033[%dD";

#if	!MSDOS
	for (i = 0; i <= K_MAX - K_MIN; i++) keyseq[i].str = NULL;
	keyseq[K_UP - K_MIN].str = "\033OA";
	keyseq[K_DOWN - K_MIN].str = "\033OB";
	keyseq[K_RIGHT - K_MIN].str = "\033OC";
	keyseq[K_LEFT - K_MIN].str = "\033OD";
	keyseq[K_HOME - K_MIN].str = "\033[4~";
	keyseq[K_BS - K_MIN].str = "\010";
	keyseq[K_F(1) - K_MIN].str = "\033[11~";
	keyseq[K_F(2) - K_MIN].str = "\033[12~";
	keyseq[K_F(3) - K_MIN].str = "\033[13~";
	keyseq[K_F(4) - K_MIN].str = "\033[14~";
	keyseq[K_F(5) - K_MIN].str = "\033[15~";
	keyseq[K_F(6) - K_MIN].str = "\033[17~";
	keyseq[K_F(7) - K_MIN].str = "\033[18~";
	keyseq[K_F(8) - K_MIN].str = "\033[19~";
	keyseq[K_F(9) - K_MIN].str = "\033[20~";
	keyseq[K_F(10) - K_MIN].str = "\033[21~";
	keyseq[K_F(11) - K_MIN].str = "\033[23~";
	keyseq[K_F(12) - K_MIN].str = "\033[24~";
	keyseq[K_F(13) - K_MIN].str = "\033[25~";
	keyseq[K_F(14) - K_MIN].str = "\033[26~";
	keyseq[K_F(15) - K_MIN].str = "\033[28~";
	keyseq[K_F(16) - K_MIN].str = "\033[29~";
	keyseq[K_F(17) - K_MIN].str = "\033[31~";
	keyseq[K_F(18) - K_MIN].str = "\033[32~";
	keyseq[K_F(19) - K_MIN].str = "\033[33~";
	keyseq[K_F(20) - K_MIN].str = "\033[34~";
	keyseq[K_F(21) - K_MIN].str = "\033OP";
	keyseq[K_F(22) - K_MIN].str = "\033OQ";
	keyseq[K_F(23) - K_MIN].str = "\033OR";
	keyseq[K_F(24) - K_MIN].str = "\033OS";
	keyseq[K_F(25) - K_MIN].str = "\033OT";
	keyseq[K_F(26) - K_MIN].str = "\033OU";
	keyseq[K_F(27) - K_MIN].str = "\033OV";
	keyseq[K_F(28) - K_MIN].str = "\033OW";
	keyseq[K_F(29) - K_MIN].str = "\033OX";
	keyseq[K_F(30) - K_MIN].str = "\033OY";

	keyseq[K_F('*') - K_MIN].str = "\033Oj";
	keyseq[K_F('+') - K_MIN].str = "\033Ok";
	keyseq[K_F(',') - K_MIN].str = "\033Ol";
	keyseq[K_F('-') - K_MIN].str = "\033Om";
	keyseq[K_F('.') - K_MIN].str = "\033On";
	keyseq[K_F('/') - K_MIN].str = "\033Oo";
	keyseq[K_F('0') - K_MIN].str = "\033Op";
	keyseq[K_F('1') - K_MIN].str = "\033Oq";
	keyseq[K_F('2') - K_MIN].str = "\033Or";
	keyseq[K_F('3') - K_MIN].str = "\033Os";
	keyseq[K_F('4') - K_MIN].str = "\033Ot";
	keyseq[K_F('5') - K_MIN].str = "\033Ou";
	keyseq[K_F('6') - K_MIN].str = "\033Ov";
	keyseq[K_F('7') - K_MIN].str = "\033Ow";
	keyseq[K_F('8') - K_MIN].str = "\033Ox";
	keyseq[K_F('9') - K_MIN].str = "\033Oy";
	keyseq[K_F('=') - K_MIN].str = "\033OX";
	keyseq[K_F('?') - K_MIN].str = "\033OM";
	keyseq[K_DL - K_MIN].str = "";
	keyseq[K_IL - K_MIN].str = "";
	keyseq[K_DC - K_MIN].str = "\177";
	keyseq[K_IC - K_MIN].str = "\033[2~";
	keyseq[K_CLR - K_MIN].str = "";
	keyseq[K_PPAGE - K_MIN].str = "\033[5~";
	keyseq[K_NPAGE - K_MIN].str = "\033[6~";
	keyseq[K_ENTER - K_MIN].str = "\033[9~";
	keyseq[K_END - K_MIN].str = "\033[1~";
#endif	/* !MSDOS */
	return(0);
}

static int NEAR maxlocate(yp, xp)
int *yp, *xp;
{
	int i;
#if	MSDOS
	char *cp;

	if (t_setcursor && t_resetcursor) putterms(t_setcursor);
	if ((cp = tparamstr(c_locate, 0, 999))) {
		for (i = 0; cp[i]; i++) bdos(0x06, cp[i], 0);
		free(cp);
	}
	if ((cp = tparamstr(c_ndown, 999, 999))) {
		for (i = 0; cp[i]; i++) bdos(0x06, cp[i], 0);
		free(cp);
	}
#else
	if (t_setcursor && t_resetcursor) putterms(t_setcursor);
	locate(998, 998);
	tflush();
#endif
	i = getxy(yp, xp);
	if (t_setcursor && t_resetcursor) putterms(t_resetcursor);
	return(i);
}

static int NEAR getxy(yp, xp)
int *yp, *xp;
{
	char *format, buf[sizeof(SIZEFMT) + 4];
	int i, j, k, tmp, count, *val[2];

	format = SIZEFMT;
#if	MSDOS
	for (i = 0; i < sizeof(GETSIZE) - 1; i++) bdos(0x06, GETSIZE[i], 0);
#else
	write(ttyio, GETSIZE, sizeof(GETSIZE) - 1);
#endif

	for (i = 0; i < sizeof(buf) - 1; i++) {
#if	MSDOS
		buf[i] = bdos(0x07, 0x00, 0);
#else
		buf[i] = getch2();
#endif
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
	return(count);
}

#if	MSDOS
char *tparamstr(s, arg1, arg2)
char *s;
int arg1, arg2;
{
	char *cp, buf[MAXPRINTBUF + 1];

	sprintf(buf, s, arg1, arg2);
	if (!(cp = (char *)malloc(strlen(buf) + 1))) err2(NULL);
	strcpy(cp, buf);
	return(cp);
}

int getterment(VOID_A)
{
	if (termflags & F_TERMENT) return(-1);
	defaultterm();
	termflags |= F_TERMENT;
	return(0);
}

#else	/* !MSDOS */

char *tparamstr(s, arg1, arg2)
char *s;
int arg1, arg2;
{
	char *buf;
	int i, j, n, sw, size, args[2];

	if (!s) return(NULL);
	if (!(buf = (char *)malloc(size = BUFUNIT))) err2(NULL);
	args[0] = arg1;
	args[1] = arg2;

	for (i = j = n = 0; s[i]; i++) {
		if (j + 5 >= size) {
			size += BUFUNIT;
			if (!(buf = (char *)realloc(buf, size))) err2(NULL);
		}
		if (s[i] != '%') buf[j++] = s[i];
		else if (s[++i] == '%') buf[j++] = '%';
		else if (n >= 2) {
			free(buf);
			return(NULL);
		}
		else switch (s[i]) {
			case 'd':
				sprintf(buf + j, "%d", args[n++]);
				j += strlen(buf + j);
				break;
			case '2':
				sprintf(buf + j, "%02d", args[n++]);
				j += 2;
				break;
			case '3':
				sprintf(buf + j, "%03d", args[n++]);
				j += 3;
				break;
			case '.':
				sprintf(buf + (j++), "%c", args[n++]);
				break;
			case '+':
				if (!s[++i]) {
					free(buf);
					return(NULL);
				}
				sprintf(buf + (j++), "%c", args[n++] + s[i]);
				break;
			case '>':
				if (!s[++i] || !s[i + 1]) {
					free(buf);
					return(NULL);
				}
				if (args[n] > s[i++]) args[n] += s[i];
				break;
			case 'r':
				sw = args[0];
				args[0] = args[1];
				args[1] = sw;
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
	}

	buf[j] = '\0';
	return(buf);
}

static char *NEAR tgetstr2(term, s)
char **term, *s;
{
	char strbuf[TERMCAPSIZE];
	char *p, *cp;

	p = strbuf;
	if ((cp = tgetstr(s, &p)) || (cp = *term)) {
		if (!(*term = (char *)malloc(strlen(cp) + 1))) err2(NULL);
		strcpy(*term, cp);
	}
	return(*term);
}

static char *NEAR tgetstr3(term, str1, str2)
char **term, *str1, *str2;
{
	char strbuf[TERMCAPSIZE];
	char *p, *cp;

	p = strbuf;
	if (!(cp = tgetstr(str1, &p))
	&& (cp = tparamstr(tgetstr(str2, &p), 1, 1))) *term = cp;
	else if (cp || (cp = *term)) {
		if (!(*term = (char *)malloc(strlen(cp) + 1))) err2(NULL);
		strcpy(*term, cp);
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

static kstree_t *NEAR newkeyseq(parent, num)
kstree_t *parent;
int num;
{
	kstree_t *new;
	int i, n;

	if (!parent || !(parent -> next))
		new = (kstree_t *)malloc(sizeof(kstree_t) * num);
	else new = (kstree_t *)realloc(parent -> next, sizeof(kstree_t) * num);
	if (!new) err2(NULL);

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

static int NEAR freekeyseq(list, n)
kstree_t *list;
int n;
{
	int i;

	if (!list) return(-1);
	for (i = list[n].num - 1; i >= 0; i--) freekeyseq(list[n].next, i);
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
	if (keyseqtree) freekeyseq(keyseqtree, 0);

	keyseqtree = newkeyseq(NULL, 1);

	for (i = 0; i <= K_MAX - K_MIN; i++) {
		p = keyseqtree;
		for (j = 0; j < keyseq[i].len; j++) {
			for (k = 0; k < p -> num; k++) {
				if (keyseq[i].str[j]
				== keyseq[p -> next[k].key].str[j]) break;
			}
			if (k >= p -> num) {
				newkeyseq(p, k + 1);
				p -> next[k].key = i;
			}
			p = &(p -> next[k]);
		}
	}

	return(0);
}

int getterment(VOID_A)
{
	char *cp, *termname, buf[TERMCAPSIZE];
	int i, j;

	if (termflags & F_TERMENT) return(-1);
	if (!(termname = (char *)getenv("TERM"))) termname = "unknown";
# ifdef	DEBUG
	_mtrace_file = "tgetent(start)";
	i = tgetent(buf, termname);
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "tgetent(end)";
		malloc(0);	/* dummy malloc */
	}
	if (i <= 0) err2("No TERMCAP is prepared");
# else
	if (tgetent(buf, termname) <= 0) err2("No TERMCAP is prepared");
# endif

	if (!(ttyout = fdopen(ttyio, "w+"))) ttyout = stdout;

	defaultterm();
	cp = "";
	tgetstr2(&cp, "pc");
	PC = *cp;
	free(cp);
	tgetstr2(&BC, "bc");
	tgetstr2(&UP, "up");

	cp = NULL;
	if (tgetstr2(&cp, "ku") || tgetstr2(&cp, "kd")
	|| tgetstr2(&cp, "kr") || tgetstr2(&cp, "kl")) {
		free(cp);
		t_keypad = t_nokeypad = "";
		keyseq[K_UP - K_MIN].str =
		keyseq[K_DOWN - K_MIN].str =
		keyseq[K_RIGHT - K_MIN].str =
		keyseq[K_LEFT - K_MIN].str = NULL;
	}

	n_column = n_lastcolumn = tgetnum("co");
	n_line = tgetnum("li");
	if (!tgetflag("xn")) n_lastcolumn--;
	stable_standout = tgetflag("xs");
	tgetstr2(&t_init, "ti");
	tgetstr2(&t_end, "te");
	tgetstr2(&t_metamode, "mm");
	tgetstr2(&t_nometamode, "mo");
	tgetstr2(&t_scroll, "cs");
	tgetstr2(&t_keypad, "ks");
	tgetstr2(&t_nokeypad, "ke");
	tgetstr2(&t_normalcursor, "ve");
	tgetstr2(&t_highcursor, "vs");
	tgetstr2(&t_nocursor, "vi");
	tgetstr2(&t_setcursor, "sc");
	tgetstr2(&t_resetcursor, "rc");
	tgetstr2(&t_bell, "bl");
	tgetstr2(&t_vbell, "vb");
	tgetstr2(&t_clear, "cl");
	tgetstr2(&t_normal, "me");
	tgetstr2(&t_bold, "md");
	tgetstr2(&t_reverse, "mr");
	tgetstr2(&t_dim, "mh");
	tgetstr2(&t_blink, "mb");
	tgetstr2(&t_standout, "so");
	tgetstr2(&t_underline, "us");
	tgetstr2(&end_standout, "se");
	tgetstr2(&end_underline, "ue");
	tgetstr2(&l_clear, "ce");
	tgetstr2(&l_insert, "al");
	tgetstr2(&l_delete, "dl");
	tgetstr3(&c_insert, "ic", "IC");
	tgetstr3(&c_delete, "dc", "DC");
	tgetstr2(&c_store, "sc");
	tgetstr2(&c_restore, "rc");
	tgetstr2(&c_locate, "cm");
	tgetstr2(&c_home, "ho");
	tgetstr2(&c_return, "cr");
	tgetstr2(&c_newline, "nl");
	tgetstr2(&c_scrollforw, "sf");
	tgetstr2(&c_scrollrev, "sr");
	tgetstr3(&c_up, "up", "UP");
	tgetstr3(&c_down, "do", "DO");
	tgetstr3(&c_right, "nd", "RI");
	tgetstr3(&c_left, "le", "LE");
	tgetstr2(&c_nup, "UP");
	tgetstr2(&c_ndown, "DO");
	tgetstr2(&c_nright, "RI");
	tgetstr2(&c_nleft, "LE");

	for (i = 0; i <= K_MAX - K_MIN; i++) if (keyseq[i].str) {
		cp = (char *)malloc(strlen(keyseq[i].str) + 1);
		if (!cp) err2(NULL);
		strcpy(cp, keyseq[i].str);
		keyseq[i].str = cp;
	}

	tgetkeyseq(K_UP, "ku");
	tgetkeyseq(K_DOWN, "kd");
	tgetkeyseq(K_RIGHT, "kr");
	tgetkeyseq(K_LEFT, "kl");
	tgetkeyseq(K_HOME, "kh");
	tgetkeyseq(K_BS, "kb");
	tgetkeyseq(K_F(1), "l1");
	tgetkeyseq(K_F(2), "l2");
	tgetkeyseq(K_F(3), "l3");
	tgetkeyseq(K_F(4), "l4");
	tgetkeyseq(K_F(5), "l5");
	tgetkeyseq(K_F(6), "l6");
	tgetkeyseq(K_F(7), "l7");
	tgetkeyseq(K_F(8), "l8");
	tgetkeyseq(K_F(9), "l9");
	tgetkeyseq(K_F(10), "la");
	tgetkeyseq(K_F(21), "k1");
	tgetkeyseq(K_F(22), "k2");
	tgetkeyseq(K_F(23), "k3");
	tgetkeyseq(K_F(24), "k4");
	tgetkeyseq(K_F(25), "k5");
	tgetkeyseq(K_F(26), "k6");
	tgetkeyseq(K_F(27), "k7");
	tgetkeyseq(K_F(28), "k8");
	tgetkeyseq(K_F(29), "k9");
	tgetkeyseq(K_F(30), "k0");
	tgetkeyseq(K_DL, "kL");
	tgetkeyseq(K_IL, "kA");
	tgetkeyseq(K_DC, "kD");
	tgetkeyseq(K_IC, "kI");
	tgetkeyseq(K_CLR, "kC");
	tgetkeyseq(K_EOL, "kE");
	tgetkeyseq(K_PPAGE, "kP");
	tgetkeyseq(K_NPAGE, "kN");
	tgetkeyseq(K_ENTER, "@8");
	tgetkeyseq(K_BEG, "@1");
	tgetkeyseq(K_END, "@7");

	for (i = 0; i <= K_MAX - K_MIN; i++) keyseq[i].code = K_MIN + i;
	for (i = 11; i <= 20; i++)
		keyseq[K_F(i + 10) - K_MIN].code = K_F(i - 10);
	for (i = 31; K_F(i) < K_DL; i++)
		if (keyseq[K_F(i) - K_MIN].str)
			keyseq[K_F(i) - K_MIN].code = i;
	keyseq[K_F('?') - K_MIN].code = CR;

	for (i = 0; i <= K_MAX - K_MIN; i++) {
		if (!(keyseq[i].str)) keyseq[i].len = 0;
		else {
			for (j = 0; keyseq[i].str[j]; j++)
				keyseq[i].str[j] &= 0x7f;
			keyseq[i].len = j;
		}
	}
	sortkeyseq();

	termflags |= F_TERMENT;
	return(0);
}

# ifdef	DEBUG
static int NEAR freeterment(VOID_A)
{
	int i;

	if (!(termflags & F_TERMENT)) return(-1);

	if (BC) free(BC);
	if (UP) free(UP);
	if (t_init) free(t_init);
	if (t_end) free(t_end);
	if (t_metamode) free(t_metamode);
	if (t_nometamode) free(t_nometamode);
	if (t_scroll) free(t_scroll);
	if (t_keypad) free(t_keypad);
	if (t_nokeypad) free(t_nokeypad);
	if (t_normalcursor) free(t_normalcursor);
	if (t_highcursor) free(t_highcursor);
	if (t_nocursor) free(t_nocursor);
	if (t_setcursor) free(t_setcursor);
	if (t_resetcursor) free(t_resetcursor);
	if (t_bell) free(t_bell);
	if (t_vbell) free(t_vbell);
	if (t_clear) free(t_clear);
	if (t_normal) free(t_normal);
	if (t_bold) free(t_bold);
	if (t_reverse) free(t_reverse);
	if (t_dim) free(t_dim);
	if (t_blink) free(t_blink);
	if (t_standout) free(t_standout);
	if (t_underline) free(t_underline);
	if (end_standout) free(end_standout);
	if (end_underline) free(end_underline);
	if (l_clear) free(l_clear);
	if (l_insert) free(l_insert);
	if (l_delete) free(l_delete);
	if (c_insert) free(c_insert);
	if (c_delete) free(c_delete);
	if (c_store) free(c_store);
	if (c_restore) free(c_restore);
	if (c_locate) free(c_locate);
	if (c_home) free(c_home);
	if (c_return) free(c_return);
	if (c_newline) free(c_newline);
	if (c_scrollforw) free(c_scrollforw);
	if (c_scrollrev) free(c_scrollrev);
	if (c_up) free(c_up);
	if (c_down) free(c_down);
	if (c_right) free(c_right);
	if (c_left) free(c_left);
	if (c_nup) free(c_nup);
	if (c_ndown) free(c_ndown);
	if (c_nright) free(c_nright);
	if (c_nleft) free(c_nleft);

	for (i = 0; i <= K_MAX - K_MIN; i++)
		if (keyseq[i].str) free(keyseq[i].str);
	if (keyseqtree) freekeyseq(keyseqtree, 0);

	termflags &= ~F_TERMENT;
	return(0);
}
# endif	/* DEBUG */

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

	if (str) for (i = 0; i <= K_MAX - K_MIN; i++) {
		if (keyseq[i].code == n
		|| !(keyseq[i].str) || keyseq[i].len != len)
			continue;
		if (!memcmp(str, keyseq[i].str, len)) {
			free(keyseq[i].str);
			keyseq[i].str = NULL;
			keyseq[i].len = 0;
		}
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
#endif	/* !MSDOS */

int initterm(VOID_A)
{
	if (!(termflags & F_TERMENT)) getterment();
	putterms(t_keypad);
	putterms(t_init);
	tflush();
	termflags |= F_INITTERM;
	return(0);
}

int endterm(VOID_A)
{
	if (!(termflags & F_INITTERM)) return(-1);
	putterms(t_nokeypad);
	putterms(t_end);
	tflush();
	termflags &= ~F_INITTERM;
	return(0);
}

#if	MSDOS

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

	for (i = 0; s[i]; i++) {
		if (s[i] != '\t') {
			bdos(0x06, s[i], 0);
			continue;
		}

		keyflush();
		if (getxy(&y, &x) != 2) x = 1;
		do {
			bdos(0x06, ' ', 0);
		} while (x++ % 8);
	}
	return(0);
}

/*ARGSUSED*/
int kbhit2(usec)
u_long usec;
{
#if	defined (NOTUSEBIOS) || !defined (DJGPP) || (DJGPP < 2)
	union REGS reg;
#else
# ifndef	PC98
	fd_set readfds;
	struct timeval timeout;
# endif
#endif

	if (ungetnum > 0) return(1);
#ifdef	NOTUSEBIOS
	if (nextchar) return(1);
	reg.x.ax = 0x4406;
	reg.x.bx = STDIN_FILENO;
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
	timeout.tv_sec = 0L;
	timeout.tv_usec = usec;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	return(select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout));
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
u_char c;
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

int getwsize(xmax, ymax)
int xmax, ymax;
{
	int x, y;

	if (xmax <= 0 || ymax <= 0) return(-1);
	keyflush();
	if (maxlocate(&y, &x) != 2) x = y = -1;

	if (x > 0) {
		n_lastcolumn = (n_lastcolumn < n_column) ? x - 1 : x;
		n_column = x;
	}
	if (y > 0) n_line = y;

	if (n_column < xmax) err2("Column size too small");
	if (n_line < ymax) err2("Line size too small");

	return(0);
}

#else	/* !MSDOS */

int putch2(c)
int c;
{
	return(fputc(c, ttyout));
}

int putch3(c)
int c;
{
	return(fputc(c & 0x7f, ttyout));
}

int cputs2(str)
char *str;
{
	return(fputs(str, ttyout));
}

int kbhit2(usec)
u_long usec;
{
#ifdef	NOSELECT
	return((usec) ? 1 : 0);
#else
	fd_set readfds;
	struct timeval timeout;

	timeout.tv_sec = 0L;
	timeout.tv_usec = usec;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	return(select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout));
#endif
}

int getch2(VOID_A)
{
	u_char ch;
	int i;

	while ((i = read(ttyio, &ch, sizeof(u_char))) < 0 && errno == EINTR);
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
		if (keywaitfunc) (*keywaitfunc)();
#ifndef	TIOCSTI
		if (ungetnum > 0) return((int)ungetbuf[--ungetnum]);
#endif
	} while (!key);

	key = ch = getch2();
	if (!(p = keyseqtree)) return(key);

	if (key == ESC) {
		if (!kbhit2(WAITKEYPAD * 1000L)) return(key);
		ch = getch2();
		if (isalpha(ch) && !kbhit2(WAITMETA * 1000L))
			return(ch | 0x80);
		for (j = 0; j < p -> num; j++)
			if (key == keyseq[p -> next[j].key].str[0]) break;
		if (j >= p -> num) return(key);
		p = &(p -> next[j]);
		if (keyseq[p -> key].len == 1) return(keyseq[p -> key].code);
	}
	else {
		for (j = 0; j < p -> num; j++)
			if (key == keyseq[p -> next[j].key].str[0]) break;
		if (j >= p -> num) return(key);
		p = &(p -> next[j]);
		if (keyseq[p -> key].len == 1) return(keyseq[p -> key].code);
		if (!kbhit2(WAITKEYPAD * 1000L)) return(key);
		ch = getch2();
	}

	for (i = 1; p && p -> next; i++) {
		for (j = 0; j < p -> num; j++)
			if (ch == keyseq[p -> next[j].key].str[i]) break;
		if (j >= p -> num) return(key);
		p = &(p -> next[j]);
		if (keyseq[p -> key].len == i + 1)
			return(keyseq[p -> key].code);
		if (!kbhit2(WAITKEYPAD * 1000L)) return(key);
		ch = getch2();
	}
	return(key);
}

int ungetch2(c)
u_char c;
{
#ifdef	TIOCSTI
	ioctl(ttyio, TIOCSTI, &c);
#else
	if (ungetnum >= sizeof(ungetbuf) / sizeof(u_char) - 1) return(EOF);
	ungetbuf[ungetnum++] = c;
#endif
	return(c);
}

int setscroll(s, e)
int s, e;
{
	char *cp;

	if ((cp = tparamstr(t_scroll, s, e))) {
		putterms(cp);
		free(cp);
	}
	return(0);
}

int locate(x, y)
int x, y;
{
#ifdef	DEBUG
	char *cp;

	_mtrace_file = "tgoto(start)";
	cp = tgoto(c_locate, x, y);
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "tgoto(end)";
		malloc(0);	/* dummy malloc */
	}
	putterms(cp);
#else
	putterms(tgoto(c_locate, x, y));
#endif
	return(0);
}

int tflush(VOID_A)
{
	fflush(ttyout);
	return(0);
}

int getwsize(xmax, ymax)
int xmax, ymax;
{
	int x, y, tx, ty;
#ifdef	TIOCGWINSZ
	struct winsize ws;
#else
#ifdef	WIOCGETD
	struct uwdate ws;
#else
#ifdef	TIOCGSIZE
	struct ttysize ts;
#endif
#endif
#endif

	if (xmax <= 0 || ymax <= 0) return(-1);

	x = y = -1;
#ifdef	TIOCGWINSZ
	if (ioctl(ttyio, TIOCGWINSZ, &ws) >= 0) {
		if (ws.ws_col > 0) x = ws.ws_col;
		if (ws.ws_row > 0) y = ws.ws_row;
	}
#else
#ifdef	WIOCGETD
	ws.uw_hs = ws.uw_vs = 0;
	if (ioctl(ttyio, WIOCGETD, &ws) >= 0) {
		if (ws.uw_width > 0 && ws.uw_hs > 0)
			x = ws.uw_width / ws.uw_hs;
		if (ws.uw_height > 0 && ws.uw_vs > 0)
			y = ws.uw_height / ws.uw_vs;
	}
#else
#ifdef	TIOCGSIZE
	if (ioctl(ttyio, TIOCGSIZE, &ts) >= 0) {
		if (ts.ts_cols > 0) x = ts.ts_cols;
		if (ts.ts_lines > 0) y = ts.ts_lines;
	}
#endif
#endif
#endif

	if (usegetcursor || x < 0 || y < 0) {
		setscroll(-1, -1);
		if (maxlocate(&ty, &tx) == 2) {
			x = tx;
			y = ty;
#ifdef	TIOCGWINSZ
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
#else
#ifdef	WIOCGETD
			if (ws.uw_hs > 0 && ws.uw_vs > 0) {
				ws.uw_width = x * ws.uw_hs;
				ws.uw_height = y * ws.uw_vs;
				ioctl(ttyio, WIOCSETD, &ws);
			}
#else
#ifdef	TIOCGSIZE
			ts.ts_cols = x;
			ts.ts_lines = y;
			ioctl(ttyio, TIOCSSIZE, &ts);
#endif
#endif
#endif
		}
	}

	if (x > 0) {
		n_lastcolumn = (n_lastcolumn < n_column) ? x - 1 : x;
		n_column = x;
	}
	if (y > 0) n_line = y;

	if (n_column < xmax) err2("Column size too small");
	if (n_line < ymax) err2("Line size too small");

	setscroll(-1, n_line - 1);
	return(0);
}
#endif	/* !MSDOS */

#ifdef	USESTDARGH
/*VARARGS1*/
int cprintf2(CONST char *fmt, ...)
{
	va_list args;
	char buf[MAXPRINTBUF + 1];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
#else	/* !USESTDARGH */
# ifndef	NOVSPRINTF
/*VARARGS1*/
int cprintf2(fmt, va_alist)
CONST char *fmt;
va_dcl
{
	va_list args;
	char buf[MAXPRINTBUF + 1];

	va_start(args);
	vsprintf(buf, fmt, args);
	va_end(args);
# else	/* NOVSPRINTF */
int cprintf2(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
{
	char buf[MAXPRINTBUF + 1];

	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
# endif	/* NOVSPRINTF */
#endif	/* !USESTDARGH */
	return(cputs2(buf));
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
