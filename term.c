/*
 *	term.c
 *
 *	Terminal Module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <varargs.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include "machine.h"
#include "term.h"

#ifdef	USETERMIO
#include <termio.h>
#else
#include <sgtty.h>
#endif

extern int tgetent();
extern int tgetnum();
extern char *tgetstr();
extern char *tgoto();

#define	SENSEPERSEC	50
#define	WAITKEYPAD	360		/* msec */
#define	MAXPRINTBUF	255
#define	STDIN		0
#define	STDOUT		1
#define	STDERR		2
#define	TTYNAME		"/dev/tty"
#define	GETSIZE		"\0337\033[r\033[999;999H\033[6n"
#define	SIZEFMT		"\033[%d;%dR"

static void err2();
static void defaultterm();
static void tgetstr2();
static void tgetstr3();
static void sortkeyseq();
static int realscanf();

short ospeed;
char PC;
int n_column;
int n_line;
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
static unsigned char keyseqlen[K_MAX - K_MIN + 1];
static short keycode[K_MAX - K_MIN + 1];
static int ttyio;

#ifndef	TIOCSTI
static char ungetbuf[10];
static int ungetnum = 0;
#endif

static int termflags;
#define	F_INITTTY	001
#define	F_TERMENT	002
#define	F_INITTERM	004


void inittty(reset)
int reset;
{
	unsigned long request;
#ifdef	USETERMIO
	static struct termio dupttyio;

	request = (reset) ? TCSETAW : TCGETA;
#else
	static struct sgttyb dupttyio;

	request = (reset) ? TIOCSETP : TIOCGETP;
#endif
	if (reset && !(termflags & F_INITTTY)) return;
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

static void ttymode(d, mode, set)
int d;
short mode;
int set;
{
#ifdef	USETERMIO
	struct termio tty;

	if (ioctl(d, TCGETA, &tty) < 0) err2(NULL);
	if (set) tty.c_lflag |= mode;
	else tty.c_lflag &= ~mode;
	if (ioctl(d, TCSETAW, &tty) < 0) err2(NULL);
#else
	struct sgttyb tty;

	if (ioctl(d, TIOCGETP, &tty) < 0) err2(NULL);
	if (set) tty.sg_flags |= mode;
	else tty.sg_flags &= ~mode;
	if (ioctl(d, TIOCSETP, &tty) < 0) err2(NULL);
#endif
}

void cooked2()
{
#ifdef	USETERMIO
	struct termio tty;

	if (ioctl(ttyio, TCGETA, &tty) < 0) err2(NULL);
	tty.c_iflag &= ~IGNBRK;
	tty.c_iflag |= IXON;
	tty.c_oflag |= OPOST;
	tty.c_oflag &= ~(OCRNL|ONOCR|ONLRET);
	tty.c_lflag |= ISIG|ICANON;
	tty.c_cc[VMIN] = 4;
	tty.c_cc[VTIME] = 255;
	if (ioctl(ttyio, TCSETAW, &tty) < 0) err2(NULL);
#else
	ttymode(ttyio, CBREAK | RAW, 0);
#endif
}

void cbreak2()
{
#ifdef	USETERMIO
	struct termio tty;

	if (ioctl(ttyio, TCGETA, &tty) < 0) err2(NULL);
	tty.c_iflag &= ~IGNBRK;
	tty.c_iflag &= ~IXON;
	tty.c_oflag |= OPOST;
	tty.c_oflag &= ~(OCRNL|ONOCR|ONLRET);
	tty.c_lflag |= ISIG|ICANON;
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;
	if (ioctl(ttyio, TCSETAW, &tty) < 0) err2(NULL);
#else
	ttymode(ttyio, CBREAK, 1);
#endif
}

void raw2()
{
#ifdef	USETERMIO
	struct termio tty;

	if (ioctl(ttyio, TCGETA, &tty) < 0) err2(NULL);
	tty.c_iflag |= IGNBRK;
	tty.c_iflag &= ~IXON;
	tty.c_oflag &= ~OPOST;
	tty.c_lflag &= ~(ISIG|ICANON);
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;
	if (ioctl(ttyio, TCSETAW, &tty) < 0) err2(NULL);
#else
	ttymode(ttyio, RAW, 1);
#endif
}

void echo2()
{
#ifdef	USETERMIO
	ttymode(ttyio, ECHO|ECHOE|ECHOK|ECHONL, 1);
#else
	ttymode(ttyio, ECHO, 1);
#endif
}

void noecho2()
{
#ifdef	USETERMIO
	ttymode(ttyio, ECHO|ECHOE|ECHOK|ECHONL, 0);
#else
	ttymode(ttyio, ECHO, 0);
#endif
}

void nl2()
{
#ifdef	USETERMIO
	struct termio tty;

	if (ioctl(ttyio, TCGETA, &tty) < 0) err2(NULL);
	tty.c_iflag |= ICRNL;
	tty.c_oflag |= ONLCR;
	if (ioctl(ttyio, TCSETAW, &tty) < 0) err2(NULL);
#else
	ttymode(ttyio, CRMOD, 1);
#endif
}

void nonl2()
{
#ifdef	USETERMIO
	struct termio tty;

	if (ioctl(ttyio, TCGETA, &tty) < 0) err2(NULL);
	tty.c_iflag &= ~ICRNL;
	tty.c_oflag &= ~ONLCR;
	if (ioctl(ttyio, TCSETAW, &tty) < 0) err2(NULL);
#else
	ttymode(ttyio, CRMOD, 0);
#endif
}

void tabs()
{
#ifdef	USETERMIO
	struct termio tty;

	if (ioctl(ttyio, TCGETA, &tty) < 0) err2(NULL);
	tty.c_oflag &= ~TAB3;
	if (ioctl(ttyio, TCSETAW, &tty) < 0) err2(NULL);
#else
	ttymode(ttyio, XTABS, 0);
#endif
}

void notabs()
{
#ifdef	USETERMIO
	struct termio tty;

	if (ioctl(ttyio, TCGETA, &tty) < 0) err2(NULL);
	tty.c_oflag |= TAB3;
	if (ioctl(ttyio, TCSETAW, &tty) < 0) err2(NULL);
#else
	ttymode(ttyio, XTABS, 1);
#endif
}

void keyflush()
{
#ifdef	USETERMIO
	ioctl(ttyio, TCFLSH, 0);
#else
	int i = FREAD;
	ioctl(ttyio, TIOCFLUSH, &i);
#endif
}

void exit2(no)
int no;
{
	if (termflags & F_TERMENT) putterm(t_normal);
	endterm();
	inittty(1);
	keyflush();
	exit(no);
}

static void err2(mes)
char *mes;
{
	endterm();
	inittty(1);
	cprintf("\007\n");
	if (mes) cprintf("%s\n", mes);
	else perror(TTYNAME);
	exit2(1);
}

static void defaultterm()
{
	int i;

	n_column = 80;
	n_line = 24;
	t_keypad = "\033[?1h\033=";
	t_nokeypad = "\033[?1l\033>";
	t_normalcursor = "\033[?25h";
	t_highcursor = "\033[?25h";
	t_nocursor = "\033[?25l";
	t_setcursor = "\0337";
	t_resetcursor = "\0338";
	t_bell = "\007";
	t_vbell = "\007";
	t_clear = "\033[2J";
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
	c_insert = "\033[@";
	c_delete = "\033[P";
	c_store = "\033[s";
	c_restore = "\033[u";
	c_home = "\033[H";
	c_locate = "\033[%d;%dH";
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
	keyseq[K_DC - K_MIN] = "\177";
	keyseq[K_IC - K_MIN] = "\033[2~";
	keyseq[K_PPAGE - K_MIN] = "\033[5~";
	keyseq[K_NPAGE - K_MIN] = "\033[6~";
}

static void tgetstr2(term, str)
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

static void tgetstr3(term, str1, str2)
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

static void sortkeyseq()
{
	int i, j, tmp;
	char *str;

	for (i = 0; i <= K_MAX - K_MIN; i++) keycode[i] = K_MIN + i;

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

void getterment()
{
	char buf[1024];
	char *cp;

	if (!(cp = getenv("TERM"))) cp = "unknown";
	if (tgetent(buf, cp) <= 0) err2("No TERMCAP prepared");

	if ((ttyio = open(TTYNAME, O_RDWR)) < 0) ttyio = STDERR;

	defaultterm();
	n_column = tgetnum("co");
	n_line = tgetnum("li");
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
	tgetstr2(&keyseq[K_DC - K_MIN], "kD");
	tgetstr2(&keyseq[K_PPAGE - K_MIN], "kP");
	tgetstr2(&keyseq[K_NPAGE - K_MIN], "kN");
	sortkeyseq();

	cp = "";
	tgetstr2(&cp, "pc");
	PC = *cp;

	termflags |= F_TERMENT;
}

void initterm()
{
	if (!(termflags & F_TERMENT)) getterment();
	putterms(t_keypad);
	putterms(t_init);
	tflush();
	termflags |= F_INITTERM;
}

void endterm()
{
	if (!(termflags & F_INITTERM)) return;
	putterms(t_nokeypad);
	putterms(t_end);
	tflush();
	termflags &= ~F_INITTERM;
}

int putch(c)
char c;
{
	return(fputc(c, stdout));
}

int cputs(str)
char *str;
{
	return(fputs(str, stdout));
}

int cprintf(fmt, va_alist)
unsigned char *fmt;
va_dcl
{
	va_list args;
	int len;
	char buf[MAXPRINTBUF + 1];

	va_start(args);
	len = vsprintf(buf, (char *)fmt, args);
	va_end(args);

	fputs(buf, stdout);
	return(len);
}

int kbhit2(usec)
unsigned long usec;
{
	fd_set readfds;
	struct timeval timeout;

	timeout.tv_sec = 0L;
	timeout.tv_usec = usec;
	FD_SET(STDIN, &readfds);

	return (select(1, &readfds, NULL, NULL, &timeout));
}

int getch2()
{
	unsigned char ch;

	read(ttyio, &ch, sizeof(unsigned char));
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
	for (i = start = 0;;i++) {
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

void ungetch2(c)
char c;
{
#ifdef	TIOCSTI
	ioctl(ttyio, TIOCSTI, &c);
#else
	ungetbuf[ungetnum] = c;
	if (ungetnum < sizeof(ungetbuf) - 1) ungetnum++;
#endif
}

void locate(x, y)
int x, y;
{
	putterms(tgoto(c_locate, x, y));
}

void tflush()
{
	fflush(stdout);
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

void getwsize(xmax, ymax)
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

	if (x < 0 || y < 0) {
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

	if (x > 0) n_column = x;
	if (y > 0) n_line = y;

	if (n_column < xmax) err2("Column size too small");
	if (n_line < ymax) err2("Line size too small");
}
