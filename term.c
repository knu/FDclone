/*
 *	term.c
 *
 *	Terminal Module
 */

#include "machine.h"

#include <stdio.h>
#include <string.h>

#if	MSDOS
#include <dos.h>
#include <stdarg.h>
#ifndef	__GNUC__
#include <signal.h>
#include <sys/types.h>
#include <sys/timeb.h>
#endif
extern int initdir();
#define	TTYNAME		""
#else	/* !MSDOS */
#include <varargs.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>

#ifdef	USETERMIO
#include <termio.h>
#else
#include <sgtty.h>
#endif

#ifdef	USESELECTH
#include <sys/select.h>
#endif
#endif	/* !MSDOS */

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include "term.h"

#define	MAXPRINTBUF	255
#define	SIZEFMT		"\033[%d;%dR"

#if	!MSDOS
extern int tgetent();
extern int tgetnum();
extern int tgetflag();
extern char *tgetstr();
extern char *tgoto();

#ifndef	SENSEPERSEC
#define	SENSEPERSEC	50
#endif
#ifndef	WAITKEYPAD
#define	WAITKEYPAD	360		/* msec */
#endif
#define	STDIN		0
#define	STDOUT		1
#define	STDERR		2
#define	TTYNAME		"/dev/tty"
#define	GETSIZE		"\0337\033[r\033[999;999H\033[6n"

#ifndef	PENDIN
#define	PENDIN		0
#endif
#ifndef	ECHOCTL
#define	ECHOCTL		0
#endif
#ifndef	ECHOKE
#define	ECHOKE		0
#endif

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

static int err2();
static int defaultterm();
static int getxy();
#if	MSDOS
# ifndef	__GNUC__
static int dosgettime();
# endif
#else	/* !MSDOS */
static int tgetstr2();
static int tgetstr3();
static int sortkeyseq();
#endif	/* !MSDOS */

short ospeed;
char PC;
char *BC;
char *UP;
int n_column;
int n_lastcolumn;
int n_line;
int stable_standout;
char *t_init;
char *t_end;
char *t_keypad;
char *t_nokeypad;
char *t_normalcursor;
char *t_highcursor;
char *t_nocursor;
char *t_setcursor;
char *t_resetcursor;
char *t_bell;
char *t_vbell;
char *t_clear;
char *t_normal;
char *t_bold;
char *t_reverse;
char *t_dim;
char *t_blink;
char *t_standout;
char *t_underline;
char *end_standout;
char *end_underline;
char *l_clear;
char *l_insert;
char *l_delete;
char *c_insert;
char *c_delete;
char *c_home;
char *c_locate;
char *c_return;
char *c_newline;
char *c_up;
char *c_down;
char *c_right;
char *c_left;

#if	MSDOS
#ifdef	PC98
static int nextchar = '\0';
static u_char specialkey[] = ":=<;89>\25667bcdefghijk\202\203\204\205\206\207\210\211\212\213?";
#else
static u_char specialkey[] = "HPMKRSGOIQ;<=>?@ABCDTUVWXYZ[\\]\206";
#endif
static int specialkeycode[] = {
	K_UP, K_DOWN, K_RIGHT, K_LEFT,
	K_IC, K_DC, K_HOME, K_END, K_PPAGE, K_NPAGE,
	K_F(1), K_F(2), K_F(3), K_F(4), K_F(5),
	K_F(6), K_F(7), K_F(8), K_F(9), K_F(10),
	K_F(11), K_F(12), K_F(13), K_F(14), K_F(15),
	K_F(16), K_F(17), K_F(18), K_F(19), K_F(20), K_HELP
};
#else	/* !MSDOS */
static char *keyseq[K_MAX - K_MIN + 1];
static u_char keyseqlen[K_MAX - K_MIN + 1];
static short keycode[K_MAX - K_MIN + 1];
static int ttyio;
static FILE *ttyout;
static char *termname;

#ifndef	TIOCSTI
static u_char ungetbuf[10];
static int ungetnum = 0;
#endif
#endif	/* !MSDOS */

static int termflags;
#define	F_INITTTY	001
#define	F_TERMENT	002
#define	F_INITTERM	004


#if	MSDOS
int inittty(reset)
int reset;
{
	termflags |= F_INITTTY;
}

int cooked2()
{
}

int cbreak2()
{
}

int raw2()
{
}

int echo2()
{
}

int noecho2()
{
}

int nl2()
{
}

int nonl2()
{
}

int tabs()
{
}

int notabs()
{
}

int keyflush()
{
#ifdef	PC98
	unsigned short far *keybuf = MK_FP(0x40, 0x124);

	keybuf[1] = keybuf[0];
	keybuf[2] = 0;
#else
	while (kbhit()) getch();
#endif
}

#else	/* !MSDOS */

int inittty(reset)
int reset;
{
#ifdef	USETERMIO
	static struct termio dupttyio;
#else
	static struct sgttyb dupttyio;
	static int dupttyflag;
#endif
	u_long request;

	if (reset && !(termflags & F_INITTTY)) return;
#ifdef	USETERMIO
	request = (reset) ? TCSETAW : TCGETA;
#else
	request = (reset) ? TIOCLSET : TIOCLGET;
	if (ioctl(ttyio, request, &dupttyflag) < 0) err2(NULL);
	request = (reset) ? TIOCSETP : TIOCGETP;
#endif
	if (ioctl(ttyio, request, &dupttyio) < 0) err2(NULL);
	if (!reset) {
#ifdef	USETERMIO
		ospeed = dupttyio.c_cflag & CBAUD;
#else
		ospeed = dupttyio.sg_ospeed;
#endif
		termflags |= F_INITTTY;
	}
}

#ifdef	USETERMIO
static int ttymode(d, set, reset, iset, ireset, oset, oreset, min, time)
int d;
u_short set, reset, iset, ireset, oset, oreset;
int min, time;
{
	struct termio tty;

	if (ioctl(d, TCGETA, &tty) < 0) err2(NULL);
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
	if (ioctl(d, TCSETAW, &tty) < 0) err2(NULL);
#else
static int ttymode(d, set, reset, lset, lreset)
int d;
u_short set, reset, lset, lreset;
{
	struct sgttyb tty;
	int lflag;

	if (ioctl(d, TIOCGETP, &tty) < 0) err2(NULL);
	if (ioctl(d, TIOCLGET, &lflag) < 0) err2(NULL);
	if (set) tty.sg_flags |= set;
	if (reset) tty.sg_flags &= reset;
	if (set) lflag |= lset;
	if (lreset) lflag &= lreset;
	if (ioctl(d, TIOCSETP, &tty) < 0) err2(NULL);
	if (ioctl(d, TIOCLSET, &lflag) < 0) err2(NULL);
#endif
}

int cooked2()
{
#ifdef	USETERMIO
	ttymode(ttyio, ISIG | ICANON, ~PENDIN,
		BRKINT | IGNPAR | IXON | IXANY | IXOFF, ~IGNBRK,
		OPOST, 0, '\004', 255);
#else
	ttymode(ttyio, 0, ~(CBREAK | RAW), 0, ~(LLITOUT | LPENDIN));
#endif
}

int cbreak2()
{
#ifdef	USETERMIO
	ttymode(ttyio, ISIG | ICANON, 0, BRKINT, ~(IXON | IGNBRK),
		0, ~OPOST, 1, 0);
#else
	ttymode(ttyio, CBREAK, 0, LLITOUT, 0);
#endif
}

int raw2()
{
#ifdef	USETERMIO
	ttymode(ttyio, 0, ~(ISIG | ICANON), IGNBRK, ~IXON, 0, ~OPOST, 1, 0);
#else
	ttymode(ttyio, RAW, 0, LLITOUT, 0);
#endif
}

int echo2()
{
#ifdef	USETERMIO
	ttymode(ttyio, ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE, ~ECHONL,
		0, 0, 0, 0, 0, 0);
#else
	ttymode(ttyio, ECHO, 0, LCRTBS | LCRTERA | LCRTKIL | LCTLECH, 0);
#endif
}

int noecho2()
{
#ifdef	USETERMIO
	ttymode(ttyio, 0, ~(ECHO | ECHOE | ECHOK | ECHONL), 0, 0, 0, 0, 0, 0);
#else
	ttymode(ttyio, 0, ~ECHO, 0, ~(LCRTBS | LCRTERA));
#endif
}

int nl2()
{
#ifdef	USETERMIO
	ttymode(ttyio, 0, 0, ICRNL, 0, ONLCR, ~(OCRNL | ONOCR | ONLRET), 0, 0);
#else
	ttymode(ttyio, CRMOD, 0, 0, 0);
#endif
}

int nonl2()
{
#ifdef	USETERMIO
	ttymode(ttyio, 0, 0, 0, ~ICRNL, 0, ~ONLCR, 0, 0);
#else
	ttymode(ttyio, 0, ~CRMOD, 0, 0);
#endif
}

int tabs()
{
#ifdef	USETERMIO
	ttymode(ttyio, 0, 0, 0, 0, 0, ~TAB3, 0, 0);
#else
	ttymode(ttyio, 0, ~XTABS, 0, 0);
#endif
}

int notabs()
{
#ifdef	USETERMIO
	ttymode(ttyio, 0, 0, 0, 0, TAB3, 0, 0, 0);
#else
	ttymode(ttyio, XTABS, 0, 0, 0);
#endif
}

int keyflush()
{
#ifdef	USETERMIO
	ioctl(ttyio, TCFLSH, 0);
#else
	int i = FREAD;
	ioctl(ttyio, TIOCFLUSH, &i);
#endif
}
#endif	/* !MSDOS */	

int exit2(no)
int no;
{
	if (termflags & F_TERMENT) putterm(t_normal);
	endterm();
	inittty(1);
	keyflush();
	exit(no);
}

static int err2(mes)
char *mes;
{
	if (termflags & F_TERMENT) putterm(t_normal);
	endterm();
	inittty(1);
	fprintf(stderr, "\007\n");
	if (mes) fprintf(stderr, "%s\n", mes);
	else perror(TTYNAME);
	exit(127);
}

static int defaultterm()
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
	t_init = "";
	t_end = "";
#if	MSDOS
	t_keypad = "";
	t_nokeypad = "";
	t_normalcursor = "\033[>5l";
	t_highcursor = "\033[>5l";
	t_nocursor = "\033[>5h";
	t_setcursor = "\033[s";
	t_resetcursor = "\033[u";
#else
	t_keypad = "\033[?1h\033=";
	t_nokeypad = "\033[?1l\033>";
	t_normalcursor = "\033[?25h";
	t_highcursor = "\033[?25h";
	t_nocursor = "\033[?25l";
	t_setcursor = "\0337";
	t_resetcursor = "\0338";
#endif
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
	c_home = "\033[H";
#ifdef	MSDOS
	c_locate = "\033[%d;%dH";
#else
	c_locate = "\033[%i%d;%dH";
#endif
	c_return = "\r";
	c_newline = "\n";
	c_up = "\033[A";
	c_down = "\012";
	c_right = "\033[C";
	c_left = "\010";

#if	!MSDOS
	for (i = 0; i <= K_MAX - K_MIN; i++) keyseq[i] = NULL;
	keyseq[K_UP - K_MIN] = "\033OA";
	keyseq[K_DOWN - K_MIN] = "\033OB";
	keyseq[K_RIGHT - K_MIN] = "\033OC";
	keyseq[K_LEFT - K_MIN] = "\033OD";
	keyseq[K_HOME - K_MIN] = "\033[4~";
	keyseq[K_BS - K_MIN] = "\010";
	keyseq[K_F(1) - K_MIN] = "\033[11~";
	keyseq[K_F(2) - K_MIN] = "\033[12~";
	keyseq[K_F(3) - K_MIN] = "\033[13~";
	keyseq[K_F(4) - K_MIN] = "\033[14~";
	keyseq[K_F(5) - K_MIN] = "\033[15~";
	keyseq[K_F(6) - K_MIN] = "\033[17~";
	keyseq[K_F(7) - K_MIN] = "\033[18~";
	keyseq[K_F(8) - K_MIN] = "\033[19~";
	keyseq[K_F(9) - K_MIN] = "\033[20~";
	keyseq[K_F(10) - K_MIN] = "\033[21~";
	keyseq[K_F(11) - K_MIN] = "\033[23~";
	keyseq[K_F(12) - K_MIN] = "\033[24~";
	keyseq[K_F(13) - K_MIN] = "\033[25~";
	keyseq[K_F(14) - K_MIN] = "\033[26~";
	keyseq[K_F(15) - K_MIN] = "\033[28~";
	keyseq[K_F(16) - K_MIN] = "\033[29~";
	keyseq[K_F(17) - K_MIN] = "\033[31~";
	keyseq[K_F(18) - K_MIN] = "\033[32~";
	keyseq[K_F(19) - K_MIN] = "\033[33~";
	keyseq[K_F(20) - K_MIN] = "\033[34~";
	keyseq[K_F(21) - K_MIN] = "\033OP";
	keyseq[K_F(22) - K_MIN] = "\033OQ";
	keyseq[K_F(23) - K_MIN] = "\033OR";
	keyseq[K_F(24) - K_MIN] = "\033OS";
	keyseq[K_F(25) - K_MIN] = "\033OT";
	keyseq[K_F(26) - K_MIN] = "\033OU";
	keyseq[K_F(27) - K_MIN] = "\033OV";
	keyseq[K_F(28) - K_MIN] = "\033OW";
	keyseq[K_F(29) - K_MIN] = "\033OX";
	keyseq[K_F(30) - K_MIN] = "\033OY";

	keyseq[K_F('*') - K_MIN] = "\033Oj";
	keyseq[K_F('+') - K_MIN] = "\033Ok";
	keyseq[K_F(',') - K_MIN] = "\033Ol";
	keyseq[K_F('-') - K_MIN] = "\033Om";
	keyseq[K_F('.') - K_MIN] = "\033On";
	keyseq[K_F('/') - K_MIN] = "\033Oo";
	keyseq[K_F('0') - K_MIN] = "\033Op";
	keyseq[K_F('1') - K_MIN] = "\033Oq";
	keyseq[K_F('2') - K_MIN] = "\033Or";
	keyseq[K_F('3') - K_MIN] = "\033Os";
	keyseq[K_F('4') - K_MIN] = "\033Ot";
	keyseq[K_F('5') - K_MIN] = "\033Ou";
	keyseq[K_F('6') - K_MIN] = "\033Ov";
	keyseq[K_F('7') - K_MIN] = "\033Ow";
	keyseq[K_F('8') - K_MIN] = "\033Ox";
	keyseq[K_F('9') - K_MIN] = "\033Oy";
	keyseq[K_F('=') - K_MIN] = "\033OX";
	keyseq[K_F('?') - K_MIN] = "\033OM";
	keyseq[K_DL - K_MIN] = "";
	keyseq[K_IL - K_MIN] = "";
	keyseq[K_DC - K_MIN] = "\177";
	keyseq[K_IC - K_MIN] = "\033[2~";
	keyseq[K_CLR - K_MIN] = "";
	keyseq[K_PPAGE - K_MIN] = "\033[5~";
	keyseq[K_NPAGE - K_MIN] = "\033[6~";
	keyseq[K_ENTER - K_MIN] = "\033[9~";
	keyseq[K_END - K_MIN] = "\033[1~";
#endif
}

static int getxy(s, format, yp, xp)
char *s, *format;
int *yp, *xp;
{
	char *cp;
	int i, j, tmp, count, *val[2];

	count = 0;
	val[0] = yp;
	val[1] = xp;

	for (i = 0, j = 0; s[i] && format[j]; i++, j++) {
		if (format[j] == '%' && format[++j] == 'd') {
			for (cp = &s[i]; *cp && *cp >= '0' && *cp <= '9'; cp++);
			if (cp == &s[i]) break;
			tmp = *cp;
			*cp = '\0';
			*val[count++] = atoi(&s[i]);
			*cp = tmp;
		}
		else if (s[i] != format[j]) j--;
	}
	return(count);
}

#if	MSDOS
int getterment()
{
	defaultterm();
	termflags |= F_TERMENT;
}

#else	/* !MSDOS */

static int tgetstr2(term, str)
char **term;
char *str;
{
	char strbuf[1024];
	char *p, *cp;

	p = strbuf;
	if (cp = tgetstr(str, &p)) {
		if (!(*term = (char *)malloc(strlen(cp) + 1))) err2(NULL);
		strcpy(*term, cp);
	}
}

static int tgetstr3(term, str1, str2)
char **term;
char *str1, *str2;
{
	char strbuf[1024];
	char *p, *cp;

	p = strbuf;
	if (!(cp = tgetstr(str1, &p)) && (cp = tgetstr(str2, &p)))
		cp = tgoto(cp, 1, 1);
	if (cp) {
		if (!(*term = (char *)malloc(strlen(cp) + 1))) err2(NULL);
		strcpy(*term, cp);
	}
}

static int sortkeyseq()
{
	int i, j, tmp;
	char *str;

	for (i = 0; i <= K_MAX - K_MIN; i++) keycode[i] = K_MIN + i;
	for (i = 1; i <= 10; i++) keycode[K_F(i + 20) - K_MIN] = K_F(i);

	keycode[K_F('*') - K_MIN] = '*';
	keycode[K_F('+') - K_MIN] = '+';
	keycode[K_F(',') - K_MIN] = ',';
	keycode[K_F('-') - K_MIN] = '-';
	keycode[K_F('.') - K_MIN] = '.';
	keycode[K_F('/') - K_MIN] = '/';
	keycode[K_F('0') - K_MIN] = '0';
	keycode[K_F('1') - K_MIN] = '1';
	keycode[K_F('2') - K_MIN] = '2';
	keycode[K_F('3') - K_MIN] = '3';
	keycode[K_F('4') - K_MIN] = '4';
	keycode[K_F('5') - K_MIN] = '5';
	keycode[K_F('6') - K_MIN] = '6';
	keycode[K_F('7') - K_MIN] = '7';
	keycode[K_F('8') - K_MIN] = '8';
	keycode[K_F('9') - K_MIN] = '9';
	keycode[K_F('=') - K_MIN] = '=';
	keycode[K_F('?') - K_MIN] = CR;

	for (i = 0; i < K_MAX - K_MIN; i++)
		for (j = i + 1; j <= K_MAX - K_MIN; j++)
			if (!keyseq[j]
			|| (keyseq[i] && strcmp(keyseq[i], keyseq[j]) > 0)) {
				tmp = keycode[i];
				str = keyseq[i];
				keycode[i] = keycode[j];
				keyseq[i] = keyseq[j];
				keycode[j] = tmp;
				keyseq[j] = str;
			}

	for (i = 0; i <= K_MAX - K_MIN; i++)
		keyseqlen[i] = (keyseq[i]) ? strlen(keyseq[i]) : 0;
}

int getterment()
{
	char *cp, buf[1024];

	if (!(termname = (char *)getenv("TERM"))) termname = "unknown";
	if (tgetent(buf, termname) <= 0) err2("No TERMCAP prepared");

	if ((ttyio = open(TTYNAME, O_RDWR, 0600)) < 0) ttyio = STDERR;
	if (!(ttyout = fopen(TTYNAME, "w+"))) ttyout = stdout;

	defaultterm();
	cp = "";
	tgetstr2(&cp, "pc");
	PC = *cp;
	tgetstr2(&BC, "bc");
	tgetstr2(&UP, "up");

	n_column = n_lastcolumn = tgetnum("co");
	n_line = tgetnum("li");
	if (!tgetflag("xn")) n_lastcolumn--;
	stable_standout = tgetflag("xs");
	tgetstr2(&t_init, "ti");
	tgetstr2(&t_end, "te");
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
	tgetstr2(&c_home, "ho");
	tgetstr2(&c_locate, "cm");
	tgetstr2(&c_return, "cr");
	tgetstr2(&c_newline, "nl");
	tgetstr2(&c_up, "up");
	tgetstr2(&c_down, "do");
	tgetstr2(&c_right, "nd");
	tgetstr2(&c_left, "le");

	tgetstr2(&keyseq[K_UP - K_MIN], "ku");
	tgetstr2(&keyseq[K_DOWN - K_MIN], "kd");
	tgetstr2(&keyseq[K_RIGHT - K_MIN], "kr");
	tgetstr2(&keyseq[K_LEFT - K_MIN], "kl");
	tgetstr2(&keyseq[K_HOME - K_MIN], "kh");
	tgetstr2(&keyseq[K_BS - K_MIN], "kb");
	tgetstr2(&keyseq[K_F(1) - K_MIN], "l1");
	tgetstr2(&keyseq[K_F(2) - K_MIN], "l2");
	tgetstr2(&keyseq[K_F(3) - K_MIN], "l3");
	tgetstr2(&keyseq[K_F(4) - K_MIN], "l4");
	tgetstr2(&keyseq[K_F(5) - K_MIN], "l5");
	tgetstr2(&keyseq[K_F(6) - K_MIN], "l6");
	tgetstr2(&keyseq[K_F(7) - K_MIN], "l7");
	tgetstr2(&keyseq[K_F(8) - K_MIN], "l8");
	tgetstr2(&keyseq[K_F(9) - K_MIN], "l9");
	tgetstr2(&keyseq[K_F(10) - K_MIN], "la");
	tgetstr2(&keyseq[K_F(21) - K_MIN], "k1");
	tgetstr2(&keyseq[K_F(22) - K_MIN], "k2");
	tgetstr2(&keyseq[K_F(23) - K_MIN], "k3");
	tgetstr2(&keyseq[K_F(24) - K_MIN], "k4");
	tgetstr2(&keyseq[K_F(25) - K_MIN], "k5");
	tgetstr2(&keyseq[K_F(26) - K_MIN], "k6");
	tgetstr2(&keyseq[K_F(27) - K_MIN], "k7");
	tgetstr2(&keyseq[K_F(28) - K_MIN], "k8");
	tgetstr2(&keyseq[K_F(29) - K_MIN], "k9");
	tgetstr2(&keyseq[K_F(30) - K_MIN], "k0");
	tgetstr2(&keyseq[K_DL - K_MIN], "kL");
	tgetstr2(&keyseq[K_IL - K_MIN], "kA");
	tgetstr2(&keyseq[K_DC - K_MIN], "kD");
	tgetstr2(&keyseq[K_IC - K_MIN], "kI");
	tgetstr2(&keyseq[K_CLR - K_MIN], "kC");
	tgetstr2(&keyseq[K_EOL - K_MIN], "kE");
	tgetstr2(&keyseq[K_PPAGE - K_MIN], "kP");
	tgetstr2(&keyseq[K_NPAGE - K_MIN], "kN");
	tgetstr2(&keyseq[K_ENTER - K_MIN], "@8");
	tgetstr2(&keyseq[K_BEG - K_MIN], "@1");
	tgetstr2(&keyseq[K_END - K_MIN], "@7");
	sortkeyseq();

	termflags |= F_TERMENT;
}
#endif	/* !MSDOS */

int initterm()
{
	if (!(termflags & F_TERMENT)) getterment();
#if	MSDOS && defined (__GNUC__)
	textmode(BW80);
#endif
	putterms(t_keypad);
	putterms(t_init);
	tflush();
	termflags |= F_INITTERM;
}

int endterm()
{
	if (!(termflags & F_INITTERM)) return;
	putterms(t_nokeypad);
	putterms(t_end);
	tflush();
	termflags &= ~F_INITTERM;
}

#if	MSDOS

int putch2(c)
int c;
{
	return(putch(c));
}

int cputs2(s)
char *s;
{
#ifndef	__GNUC__
	char *size, buf[sizeof(SIZEFMT) + 4];
	int y;
#endif
	int i, x;

	for (i = 0; s[i]; i++) {
		if (s[i] != '\t') {
			putch(s[i]);
			continue;
		}
#ifdef	__GNUC__
		x = wherex();
#else
		cputs("\033[6n");
		size = SIZEFMT;
			
		for (x = 0; (buf[x] = getch()) != size[sizeof(SIZEFMT) - 2];
			x++);
		buf[++x] = '\0';
		if (getxy(buf, size, &y, &x) != 2) x = 1;
#endif
		do {
			putch(' ');
		} while (x++ % 8);
	}
}

/*VARARGS1*/
int cprintf2(const char *fmt, ...)
{
	va_list args;
	char buf[MAXPRINTBUF + 1];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
	return(cputs2(buf));
}

int kbhit2()
{
#ifdef	PC98
	union REGS regs;

	if (nextchar) return(1);

	regs.h.ah = 0x01;
	int86(0x18, &regs, &regs);
	return(regs.h.bh != 0);
#else
	return(kbhit());
#endif
}

int getch2()
{
#ifdef	PC98
	union REGS regs;
	int ch;

	if (nextchar) {
		ch = nextchar;
		nextchar = '\0';
		return(ch);
	}

	regs.h.ah = 0x00;
	int86(0x18, &regs, &regs);

	if (!(ch = regs.h.al)) nextchar = regs.h.ah;
	return(ch);
#else
	return((u_char)getch());
#endif
}

#ifndef	__GNUC__
static int dosgettime(tbuf)
u_char tbuf[];
{
	union REGS regs;

	regs.x.ax = 0x2c00;
	intdos(&regs, &regs);
	tbuf[0] = regs.h.ch;
	tbuf[1] = regs.h.cl;
	tbuf[2] = regs.h.dh;
}
#endif

/*ARGSUSED*/
int _getkey2(sig)
int sig;
{
#ifndef	__GNUC__
	static u_char tbuf1[3] = {0xff, 0xff, 0xff};
	u_char tbuf2[3];
#endif
	int i, ch;

#ifdef	__GNUC__
	while (!kbhit2());
#else
	if (tbuf1[0] == 0xff) dosgettime(tbuf1);
	while (!kbhit()) {
		dosgettime(tbuf2);
		if (memcmp(tbuf1, tbuf2, sizeof(tbuf1))) {
			if (sig) raise(sig);
			memcpy(tbuf1, tbuf2, sizeof(tbuf1));
		}
	}
#endif
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
#if	defined (__GNUC__) && !defined (PC98)
	else {
#else
	else if (kbhit2()) {
#endif
		ch = getch2();
		for (i = 0; i < sizeof(specialkey) / sizeof(u_char); i++)
			if (ch == specialkey[i]) return(specialkeycode[i]);
		ch = K_NOKEY;
	}
	return(ch);
}

int ungetch2(c)
u_char c;
{
	return(ungetch(c));
}

#ifdef	__GNUC__
int putterm(str)
char *str;
{
	static int ox = 1;
	static int oy = 1;
	static u_char attr = 0x07;
	int x, y;

	if (str == t_normalcursor) _setcursortype(_NORMALCURSOR);
	else if (str == t_highcursor) _setcursortype(_SOLIDCURSOR);
	else if (str == t_nocursor) _setcursortype(_NOCURSOR);
	else if (str == t_setcursor) {
		ox = wherex();
		oy = wherey();
		attr = ScreenAttrib;
	}
	else if (str == t_resetcursor) {
		gotoxy(ox, oy);
		textattr(attr);
		ox = oy = 1;
		attr = 0x07;
	}
	else if (str == t_clear) clrscr();
	else if (str == t_normal) normvideo();
	else if (str == t_bold) textattr(0x0f);
	else if (str == t_reverse) textattr(0x70);
	else if (str == t_dim) textattr(0x08);
	else if (str == t_blink) textattr(0x02);
	else if (str == t_standout) textattr(0x70);
	else if (str == t_underline) textattr(0x0e);
	else if (str == end_standout) normvideo();
	else if (str == end_underline) normvideo();
	else if (str == l_clear) clreol();
	else if (str == l_insert) insline();
	else if (str == l_delete) delline();
	else if (str == c_home) gotoxy(1, 1);
	else if (str == c_up) {
		if ((y = wherey() - 1) > 0) gotoxy(wherex(), y);
	}
	else if (str == c_right) {
		x = wherex();
		y = wherey();
		if (x < ((y < n_line) ? n_column : n_lastcolumn))
			gotoxy(++x, y);
	}
	else cputs2(str);
}
#endif

int locate(x, y)
int x, y;
{
#ifdef	__GNUC__
	gotoxy(++x, ++y);
#else
	cprintf2(c_locate, ++y, ++x);
#endif
}

int tflush()
{
}

int getwsize(xmax, ymax)
int xmax, ymax;
{
}

#else	/* !MSDOS */

int putch2(c)
int c;
{
	return(fputc(c, ttyout));
}

int cputs2(str)
char *str;
{
	return(fputs(str, ttyout));
}

#ifndef	NOVSPRINTF
/*VARARGS1*/
int cprintf2(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list args;
	char buf[MAXPRINTBUF + 1];

	va_start(args);
	vsprintf(buf, fmt, args);
	va_end(args);
#else
int cprintf2(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
{
	char buf[MAXPRINTBUF + 1];

	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
#endif

	fputs(buf, ttyout);
	return(strlen(buf));
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
	FD_SET(STDIN, &readfds);

	return(select(1, &readfds, NULL, NULL, &timeout));
#endif
}

int getch2()
{
	u_char ch;

	read(ttyio, &ch, sizeof(u_char));
	return((int)ch);
}

int _getkey2(sig)
int sig;
{
	static int count = SENSEPERSEC;
	int i, j, ch, key, start;

	do {
		key = kbhit2(1000000L / SENSEPERSEC);
		if (sig && !(--count)) {
			count = SENSEPERSEC;
			kill(getpid(), sig);
		}
#ifndef	TIOCSTI
		if (ungetnum > 0) {
			ch = (int)ungetbuf[--ungetnum];
			return(ch);
		}
#endif
	} while (!key);

	key = ch = getch2();
	for (i = start = 0; ; i++) {
		for (j = start, start = -1; j <= K_MAX - K_MIN; j++) {
			if (keyseqlen[j] > i && ch == keyseq[j][i]) {
				if (keyseqlen[j] == i + 1) return(keycode[j]);
				if (start < 0) start = j;
			}
			else if (start >= 0) break;
		}
		if (start < 0 || !kbhit2(WAITKEYPAD * 1000L)) return(key);
		ch = getch2();
	}
}

int ungetch2(c)
u_char c;
{
#ifdef	TIOCSTI
	ioctl(ttyio, TIOCSTI, &c);
#else
	ungetbuf[ungetnum] = c;
	if (ungetnum < sizeof(ungetbuf) / sizeof(u_char) - 1) ungetnum++;
#endif
}

int locate(x, y)
int x, y;
{
	putterms(tgoto(c_locate, x, y));
}

int tflush()
{
	fflush(ttyout);
}

int getwsize(xmax, ymax)
int xmax, ymax;
{
	FILE *fp;
	char *size, buf[sizeof(SIZEFMT) + 4];
	int x, y;
	int i, fd;
#ifdef	TIOCGWINSZ
	struct winsize ws;
#else
#ifdef	WIOCGETD
	struct uwdate ws;
#endif
#endif

	x = y = -1;

#ifdef	TIOCGWINSZ
	if (ioctl(ttyio, TIOCGWINSZ, &ws) >= 0) {
		if (ws.ws_col > 0) x = ws.ws_col;
		if (ws.ws_row > 0) y = ws.ws_row;
	}
#else
#ifdef	WIOCGETD
	if (ioctl(ttyio, WIOCGETD, &ws) >= 0) {
		if (ws.uw_width > 0) x = ws.uw_width / ws.uw_hs;
		if (ws.uw_height > 0) y = ws.uw_height / ws.uw_vs;
	}
#endif
#endif

	if ((x < 0 || y < 0)
	&& !strncmp(termname + 1, "term", 4) && strchr("xkjhtm", *termname)) {
		if (!(fp = fopen(TTYNAME, "r+"))) err2("Can't open terminal");
		fd = fileno(fp);

		size = SIZEFMT;

		write(fd, GETSIZE, sizeof(GETSIZE) - 1);
		for (i = 0; (buf[i] = fgetc(fp)) != size[sizeof(SIZEFMT) - 2];
			i++);
		buf[++i] = '\0';
		fclose(fp);

		if (getxy(buf, size, &y, &x) != 2) x = y = -1;
	}

	if (x > 0) {
		n_lastcolumn = (n_lastcolumn < n_column) ? x - 1 : x;
		n_column = x;
	}
	if (y > 0) n_line = y;

	if (n_column < xmax) err2("Column size too small");
	if (n_line < ymax) err2("Line size too small");
}
#endif	/* !MSDOS */

int chgcolor(color, reverse)
int color, reverse;
{
#if	MSDOS && defined (__GNUC__)
	int code;

	code = (color & 0x0f);
	if (reverse) {
		code <<= 4;
		if (color == ANSI_BLACK) code |= ANSI_WHITE;
	}
	textattr(code);
#else
	if (!reverse) cprintf2("\033[%dm", color + ANSI_NORMAL);
	else if (color == ANSI_BLACK)
		cprintf2("\033[%dm\033[%dm",
			color + ANSI_REVERSE, ANSI_WHITE + ANSI_NORMAL);
	else cprintf2("\033[%dm\033[%dm",
			ANSI_BLACK + ANSI_NORMAL, color + ANSI_REVERSE);
#endif
}
