/*
 *	term.c
 *
 *	Terminal Module
 */

#include "machine.h"
#include <stdio.h>
#include <string.h>
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

#include "term.h"

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
#define	MAXPRINTBUF	255
#define	STDIN		0
#define	STDOUT		1
#define	STDERR		2
#define	TTYNAME		"/dev/tty"
#define	GETSIZE		"\0337\033[r\033[999;999H\033[6n"
#define	SIZEFMT		"\033[%d;%dR"

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

static int err2();
static int defaultterm();
static int tgetstr2();
static int tgetstr3();
static int sortkeyseq();
static int realscanf();

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
char *c_store;
char *c_restore;
char *c_home;
char *c_locate;
char *c_return;
char *c_newline;
char *c_up;
char *c_down;
char *c_right;
char *c_left;

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

static int termflags;
#define	F_INITTTY	001
#define	F_TERMENT	002
#define	F_INITTERM	004


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
	int i;

	BC = "\010";
	UP = "\033[A";
	n_column = n_lastcolumn = 80;
	n_line = 24;
	t_init = "";
	t_end = "";
	t_keypad = "\033[?1h\033=";
	t_nokeypad = "\033[?1l\033>";
	t_normalcursor = "\033[?25h";
	t_highcursor = "\033[?25h";
	t_nocursor = "\033[?25l";
	t_setcursor = "\0337";
	t_resetcursor = "\0338";
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
	c_store = "\033[s";
	c_restore = "\033[u";
	c_home = "\033[H";
	c_locate = "\033[%i%d;%dH";
	c_return = "\r";
	c_newline = "\n";
	c_up = "\033[A";
	c_down = "\012";
	c_right = "\033[C";
	c_left = "\010";

	for (i = 0; i <= K_MAX - K_MIN; i++) keyseq[i] = NULL;
	keyseq[K_UP - K_MIN] = "\033OA";
	keyseq[K_DOWN - K_MIN] = "\033OB";
	keyseq[K_RIGHT - K_MIN] = "\033OC";
	keyseq[K_LEFT - K_MIN] = "\033OD";
	keyseq[K_HOME - K_MIN] = "\033[1~";
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
	keyseq[K_END - K_MIN] = "\033[4~";
}

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
	tgetstr2(&c_store, "sc");
	tgetstr2(&c_restore, "rc");
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

int initterm()
{
	if (!(termflags & F_TERMENT)) getterment();
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

int putch(c)
int c;
{
	return(fputc(c, ttyout));
}

int cputs(str)
char *str;
{
	return(fputs(str, ttyout));
}

#ifndef	NOVSPRINTF
/*VARARGS1*/
int cprintf(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list args;
	int len;
	char buf[MAXPRINTBUF + 1];

	va_start(args);
	len = vsprintf(buf, fmt, args);
	va_end(args);
#else
int cprintf(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
{
	int len;
	char buf[MAXPRINTBUF + 1];

	len = sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
#endif

	fputs(buf, ttyout);
	return(len);
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

int getkey(sig)
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
	if (ungetnum < sizeof(ungetbuf) - 1) ungetnum++;
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

static int realscanf(s, format, yp, xp)
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

		if (realscanf(buf, size, &y, &x) != 2) {
			x = y = -1;
		}
	}

	if (x > 0) {
		n_lastcolumn = (n_lastcolumn < n_column) ? x - 1 : x;
		n_column = x;
	}
	if (y > 0) n_line = y;

	if (n_column < xmax) err2("Column size too small");
	if (n_line < ymax) err2("Line size too small");
}
