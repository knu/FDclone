/*
 *	input.c
 *
 *	Input Module
 */

#include <signal.h>
#include "fd.h"
#include "func.h"
#include "kanji.h"

#ifndef	_NOORIGSHELL
#include "system.h"
#endif

extern char **history[];
extern short histsize[];
extern int curcolumns;
extern int minfilename;
extern int tradlayout;
extern int sizeinfo;
extern int hideclock;
#ifdef	SIGALRM
extern int noalrm;
#endif
extern char *promptstr;
#ifndef	_NOORIGSHELL
extern char *promptstr2;
#endif

#define	LIMITSELECTWARN	100
#define	YESNOSTR	"[Y/N]"
#define	YESNOSIZE	(sizeof(YESNOSTR) - 1)
#define	WAITAFTERWARN	360	/* msec */
#define	maxscr(p, l)	((l) * n_line - (p) - (n_column - n_lastcolumn))
#define	within(n, p, l)	((n) < maxscr(p, l))
#define	overflow(n, p, l) \
			((n) > maxscr(p, l))
#define	iseol(n, p, l)	(within(n, p, l) && !(((n) + (p)) % (l)))
#define	LEFTMARGIN	0
#define	RIGHTMARGIN	2
#ifdef	SIGALRM
#define	sigalrm(sig)	((!noalrm && (sig)) ? SIGALRM : 0)
#else
#define	sigalrm(sig)	0
#endif

static char *NEAR trquote __P_((char *, int, int *));
static int NEAR vlen __P_((char *, int));
static int NEAR rlen __P_((char *, int));
static int NEAR vonkanji1 __P_((char *, int));
static VOID NEAR putcursor __P_((int, int));
static VOID NEAR rightcursor __P_((VOID_A));
static VOID NEAR leftcursor __P_((VOID_A));
static VOID NEAR upcursor __P_((VOID_A));
static VOID NEAR downcursor __P_((VOID_A));
#ifndef	_NOORIGSHELL
static VOID NEAR forwcursor __P_((int));
static VOID NEAR backcursor __P_((int));
static VOID NEAR forwline __P_((int));
static VOID NEAR dumbputs __P_((char *, int, int, int, int));
static VOID NEAR rewritecursor __P_((char *, int, int, int, int, int));
static VOID NEAR checkcursor __P_((char *, int, int, int, int, int));
static VOID NEAR movecursor __P_((char *, char *, int));
#endif
static VOID NEAR Xlocate __P_((int, int));
static VOID NEAR setcursor __P_((char *, int, int, int, int, int));
static int NEAR putstr __P_((char *, char *, int, int, int, int, int));
static VOID NEAR ringbell __P_((VOID_A));
static VOID NEAR clearline __P_((VOID_A));
static VOID NEAR newline __P_((int));
static int NEAR rightchar __P_((char *, int *, int, int, int, int));
static int NEAR leftchar __P_((char *, int *, int, int, int, int));
static VOID NEAR insertchar __P_((char *, int, int, int, int, int));
static VOID NEAR deletechar __P_((char *, int, int, int, int, int));
static VOID NEAR insshift __P_((char *, int, int, int));
static VOID NEAR delshift __P_((char *, int, int, int));
static VOID NEAR truncline __P_((char *, int, int, int, int, int));
static VOID NEAR displaystr __P_((char *, int *, int *, int, int));
static int NEAR preparestr __P_((char **, int, int *,
		int, ALLOC_T *, int, int, int));
static int NEAR insertstr __P_((char **, int, int,
		int, ALLOC_T *, int, char *, int, int));
#ifndef	_NOCOMPLETE
static VOID NEAR selectfile __P_((int, char **));
static int NEAR completestr __P_((char **, int, int,
		int, ALLOC_T *, int, int, int));
#endif
static int NEAR _inputstr_up __P_((char **, int *, int, int *,
		int, ALLOC_T *, int, int *, int, char **));
static int NEAR _inputstr_down __P_((char **, int *, int, int *,
		int, ALLOC_T *, int, int *, int, char **));
static int NEAR _inputstr_delete __P_((char *, int, int, int, int));
static int NEAR _inputstr_enter __P_((char **, int *, int, int *,
		int, ALLOC_T *, int));
#if	FD >= 2
static int NEAR _inputstr_case __P_((char *, int *, int, int, int, int, int));
#endif
static int NEAR _inputstr_input __P_((char **, int *, int, int *,
		int, ALLOC_T *, int, int));
static int NEAR _inputstr __P_((char **, int, ALLOC_T, int, int, int, int));
static int NEAR dispprompt __P_((char *, int));
static char *NEAR truncstr __P_((char *));
static int NEAR yesnomes __P_((char *));
static int NEAR selectcnt __P_((int, char **, int));
static int NEAR selectadj __P_((int, int, char **, char **, int [], int));
static int NEAR selectmes __P_((int, int, int, char *[], int [], int []));

int subwindow = 0;
int win_x = 0;
int win_y = 0;
char *curfilename = NULL;
#ifndef	_NOEDITMODE
char *editmode = NULL;
#endif
int lcmdline = 0;
int dupn_line = -1;
#ifndef	_NOORIGSHELL
int dumbshell = 0;
#endif

#ifndef	_NOEDITMODE
static CONST int emulatekey[] = {
	K_UP, K_DOWN, K_RIGHT, K_LEFT,
	K_IC, K_DC, K_IL, K_DL,
	K_HOME, K_END, K_BEG, K_EOL,
	K_PPAGE, K_NPAGE, K_ENTER, K_ESC
};
static CONST char emacskey[] = {
	K_CTRL('P'), K_CTRL('N'), K_CTRL('F'), K_CTRL('B'),
	K_ESC, K_CTRL('D'), K_CTRL('Q'), K_CTRL('K'),
	K_ESC, K_ESC, K_CTRL('A'), K_CTRL('E'),
	K_CTRL('V'), K_CTRL('Y'), K_CTRL('O'), K_CTRL('G')
};
#define	EMACSKEYSIZ	((int)(sizeof(emacskey) / sizeof(char)))
static CONST char vikey[] = {
	'k', 'j', 'l', 'h',
	K_ESC, 'x', K_ESC, 'D',
	'g', 'G', '0', '$',
	K_CTRL('B'), K_CTRL('F'), 'o', K_ESC
};
#define	VIKEYSIZ	((int)(sizeof(vikey) / sizeof(char)))
static CONST char wordstarkey[] = {
	K_CTRL('E'), K_CTRL('X'), K_CTRL('D'), K_CTRL('S'),
	K_CTRL('V'), K_CTRL('G'), K_CTRL(']'), K_CTRL('Y'),
	K_CTRL('W'), K_CTRL('Z'), K_CTRL('A'), K_CTRL('F'),
	K_CTRL('R'), K_CTRL('C'), K_CTRL('N'), K_ESC
};
#define	WORDSTARKEYSIZ	((int)(sizeof(wordstarkey) / sizeof(char)))
#endif
#ifndef	_NOCOMPLETE
static namelist *selectlist = NULL;
static int tmpfilepos = -1;
#endif
static int xpos = 0;
static int ypos = 0;
static int overwritemode = 0;
#ifndef	_NOORIGSHELL
static int dumbmode = 0;
static int lastofs2 = 0;
#endif


int intrkey(VOID_A)
{
	int c;

	if (kbhit2(0L) && ((c = getkey2(0)) == cc_intr || c == K_ESC)) {
		warning(0, INTR_K);
		return(1);
	}
	return(0);
}

int Xgetkey(sig, eof)
int sig, eof;
{
#ifndef	_NOEDITMODE
	static int vimode = 0;
	int i;
#endif
	static int prev = -1;
	int ch;

	if (sig < 0) {
#ifndef	_NOEDITMODE
		vimode = 0;
#endif
		prev = -1;
		return('\0');
	}

	sig = sigalrm(sig);
	ch = getkey2(sig);
	if (eof && (ch != cc_eof || prev == ch)) eof = 0;
	prev = ch;
	if (eof) return(-1);

#ifndef	_NOEDITMODE
	if (!editmode) /*EMPTY*/;
	else if (!strcmp(editmode, "emacs")) {
		for (i = 0; i < EMACSKEYSIZ; i++) {
			if (emacskey[i] == K_ESC) continue;
			if (ch == emacskey[i]) return(emulatekey[i]);
		}
	}
	else if (!strcmp(editmode, "vi")) do {
		vimode |= 1;
		if (vimode & 2) switch (ch) {
			case K_CTRL('V'):
				if (vimode & 4) vimode = 1;
				ch = K_IL;
				break;
			case K_ESC:
				vimode = (vimode & 4) ? 0 : 1;
				ch = K_LEFT;
				break;
			case K_CR:
				vimode = 1;
				break;
			case K_BS:
				break;
			default:
				if (vimode & 4) vimode = 1;
				if (ch < K_MIN) break;
				ringbell();
				vimode &= ~1;
				break;
		}
		else {
			for (i = 0; i < VIKEYSIZ; i++) {
				if (vikey[i] == K_ESC) continue;
				if (ch == vikey[i]) return(emulatekey[i]);
			}
			switch (ch) {
				case ':':
					vimode = 2;
					overwritemode = 0;
					break;
				case 'A':
					vimode = 3;
					overwritemode = 0;
					ch = K_EOL;
					break;
				case 'i':
					vimode = 2;
					overwritemode = 0;
					break;
				case 'a':
					vimode = 3;
					overwritemode = 0;
					ch = K_RIGHT;
					break;
# if	FD >= 2
				case 'R':
					vimode = 2;
					overwritemode = 1;
					break;
				case 'r':
					vimode = 6;
					overwritemode = 1;
					break;
# endif	/* FD >= 2 */
				case K_BS:
					ch = K_LEFT;
					break;
				case ' ':
					ch = K_RIGHT;
					break;
				case K_ESC:
				case K_CR:
				case '\t':
					break;
				default:
					if (ch >= K_MIN) break;
					ringbell();
					vimode &= ~1;
					break;
			}
		}
	} while ((!(vimode & 1)) && (ch = getkey2(sig)));
	else if (!strcmp(editmode, "wordstar")) {
		for (i = 0; i < WORDSTARKEYSIZ; i++) {
			if (wordstarkey[i] == K_ESC) continue;
			if (ch == wordstarkey[i]) return(emulatekey[i]);
		}
	}
#endif	/* !_NOEDITMODE */

	return(ch);
}

static char *NEAR trquote(s, len, widthp)
char *s;
int len, *widthp;
{
	char *cp;
	int i, j, v;

	cp = malloc2(len * 4 + 1);
	v = 0;
	for (i = j = 0; i < len; i++) {
		if (i + 1 < len && iskanji1(s, i)) {
			cp[j++] = s[i++];
			cp[j++] = s[i];
			v += 2;
		}
#ifdef	CODEEUC
		else if (i + 1 < len && isekana(s, i)) {
			cp[j++] = s[i++];
			cp[j++] = s[i];
			v++;
		}
#else
		else if (isskana(s, i)) {
			cp[j++] = s[i];
			v++;
		}
#endif
		else if (iscntrl2(s[i])) {
			cp[j++] = '^';
			cp[j++] = (s[i] + '@') & 0x7f;
			v += 2;
		}
		else if (ismsb(s[i])) {
			j += snprintf2(&(cp[j]), len * 4 + 1 - j,
				"\\%03o", s[i] & 0xff);
			v += 4;
		}
		else {
			cp[j++] = s[i];
			v++;
		}
	}
	cp[j] = '\0';
	if (widthp) *widthp = v;

	return(cp);
}

static int NEAR vlen(s, cx)
char *s;
int cx;
{
	int v, r, rw, vw;

	v = r = 0;
	while (r < cx) {
		if (r + 1 < cx && iskanji1(s, r)) rw = vw = 2;
#ifdef	CODEEUC
		else if (r + 1 < cx && isekana(s, r)) {
			rw = 2;
			vw = 1;
		}
#else
		else if (isskana(s, r)) rw = vw = 1;
#endif
		else if (iscntrl2(s[r])) {
			rw = 1;
			vw = 2;
		}
		else if (ismsb(s[r])) {
			rw = 1;
			vw = 4;
		}
		else rw = vw = 1;

		r += rw;
		v += vw;
	}
	return(v);
}

static int NEAR rlen(s, cx2)
char *s;
int cx2;
{
	int v, r, rw, vw;

	v = r = 0;
	while (v < cx2) {
		if (v + 1 < cx2 && iskanji1(s, r)) rw = vw = 2;
#ifdef	CODEEUC
		else if (isekana(s, r)) {
			rw = 2;
			vw = 1;
		}
#else
		else if (isskana(s, r)) rw = vw = 1;
#endif
		else if (v + 1 < cx2 && iscntrl2(s[r])) {
			rw = 1;
			vw = 2;
		}
		else if (v + 3 < cx2 && ismsb(s[r])) {
			rw = 1;
			vw = 4;
		}
		else rw = vw = 1;

		r += rw;
		v += vw;
	}
	return(r);
}

static int NEAR vonkanji1(s, cx2)
char *s;
int cx2;
{
	int v, r, rw, vw;

	v = r = 0;
	while (v < cx2) {
		if (iskanji1(s, r)) rw = vw = 2;
#ifdef	CODEEUC
		else if (isekana(s, r)) {
			rw = 2;
			vw = 1;
		}
#else
		else if (isskana(s, r)) rw = vw = 1;
#endif
		else if (iscntrl2(s[r])) {
			rw = 1;
			vw = 2;
		}
		else if (ismsb(s[r])) {
			rw = 1;
			vw = 4;
		}
		else rw = vw = 1;

		r += rw;
		v += vw;
	}
	if (v > cx2) return(0);
	return(iskanji1(s, r) ? r + 1 : 0);
}

int kanjiputs2(s, len, ptr)
char *s;
int len, ptr;
{
	char *buf;
	int n;

	if (len < 0) return(0);
	n = len * KANAWID;
	buf = malloc2(n + 1);
	strncpy3(buf, s, &len, ptr);
#ifdef	_NOKANJICONV
	cputs2(buf);
	n = strlen2(buf);
#else
	n = kanjiputs(buf);
#endif
	free(buf);
	return(n);
}

static VOID NEAR putcursor(c, n)
int c, n;
{
	win_x += n;
	while (n--) putch2(c);
#ifndef	_NOORIGSHELL
	if (dumbmode);
	else if (shellmode && win_x >= n_column) {
		putch2(' ');
		putch2('\r');
		win_x = 0;
		win_y++;
	}
#endif
}

static VOID NEAR rightcursor(VOID_A)
{
	win_x++;
	putterm(c_right);
}

static VOID NEAR leftcursor(VOID_A)
{
	win_x--;
	putterm(c_left);
}

static VOID NEAR upcursor(VOID_A)
{
	win_y--;
	putterm(c_up);
}

static VOID NEAR downcursor(VOID_A)
{
	win_y++;
	putterm(c_down);
}

#ifndef	_NOORIGSHELL
static VOID NEAR forwcursor(x)
int x;
{
	while (win_x < x) {
		putch2(' ');
		win_x++;
	}
}

static VOID NEAR backcursor(x)
int x;
{
	while (win_x > x) {
		putch2(C_BS);
		win_x--;
	}
}

static VOID NEAR forwline(x)
int x;
{
	if (win_x < n_column) {
		cputnl();
		win_y++;
	}
	else {
		while (win_x >= n_column) {
			win_x -= n_column;
			win_y++;
		}
		putch2(' ');
		putch2('\r');
	}
	win_x = 0;
	forwcursor(x);
}

static VOID NEAR dumbputs(s, cx2, len2, max, ptr)
char *s;
int cx2, len2, max, ptr;
{
	len2 -= ptr;
	if (max <= 0) clearline();
	else {
		if (len2 < max) {
			kanjiputs2(s, len2, ptr);
			win_x += len2;
		}
		else {
			if (vonkanji1(s, ptr + max - 1)) max++;
			kanjiputs2(s, max, ptr);
			win_x += max;
			putcursor('>', 1);
		}
		clearline();
		backcursor(xpos + cx2 - lastofs2);
	}
}

static VOID NEAR rewritecursor(s, cx, cx2, len, plen, linemax)
char *s;
int cx, cx2, len, plen, linemax;
{
	char *dupl;
	int i, dx, ocx, ocx2;

	i = (lastofs2) ? 1 : plen;
	dx = cx2 - lastofs2;
	ocx2 = win_x - xpos - i + lastofs2;
	if (dx < 0 || i + dx >= linemax)
		displaystr(s, &cx, &len, plen, linemax);
	else if (cx2 <= ocx2) backcursor(xpos + i + dx);
	else {
		ocx = rlen(s, ocx2);
		dupl = trquote(&(s[ocx]), cx - ocx, NULL);
		win_x += kanjiputs(dupl);
		free(dupl);
	}
}

static VOID NEAR checkcursor(s, cx, cx2, len, plen, linemax)
char *s;
int cx, cx2, len, plen, linemax;
{
	int i, dx;

	i = (lastofs2) ? 1 : plen;
	dx = cx2 - lastofs2;
	if (i + dx >= linemax) displaystr(s, &cx, &len, plen, linemax);
}

static VOID NEAR movecursor(s, s2, n)
char *s, *s2;
int n;
{
	char *cp;

	if (!*s || !(cp = tparamstr(s, n, n))) while (n--) putterm(s2);
	else {
		putterm(cp);
		free(cp);
	}
}
#endif	/* !_NOORIGSHELL */

static VOID NEAR Xlocate(x, y)
int x, y;
{
#ifndef	_NOORIGSHELL
	if (shellmode) {
		x += xpos;
		if (win_x >= n_column) {
			movecursor(c_nleft, c_left, win_x);
			win_x = 0;
		}
		if (y < win_y) movecursor(c_nup, c_up, win_y - y);
		else if (y > win_y) {
			movecursor("", c_scrollforw, y - win_y);
			putch2('\r');
			win_x = 0;
		}
		if (x < win_x) movecursor(c_nleft, c_left, win_x - x);
		else if (x > win_x) movecursor(c_nright, c_right, x - win_x);

		win_x = x;
		win_y = y;
		return;
	}
#endif	/* !_NOORIGSHELL */
	while (ypos + y >= n_line) {
		locate(0, n_line - 1);
		putterm(c_scrollforw);
		ypos--;
		hideclock = 1;
	}
	win_x = xpos + x;
	win_y = ypos + y;
	locate(win_x, win_y);
}

/*ARGSUSED*/
static VOID NEAR setcursor(s, cx, cx2, len, plen, linemax)
char *s;
int cx, cx2, len, plen, linemax;
{
	int f;

	if (cx < 0) cx = rlen(s, cx2);
	if (cx2 < 0) cx2 = vlen(s, cx);
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		rewritecursor(s, cx, cx2, len, plen, linemax);
		return;
	}
#endif
	f = within(cx2, plen, linemax) ? 0 : 1;
	cx2 -= f;
	Xlocate((cx2 + plen) % linemax, (cx2 + plen) / linemax);
	if (f) rightcursor();
}

static int NEAR putstr(cp, s, cx, cx2, len, plen, linemax)
char *cp, *s;
int cx, cx2, len, plen, linemax;
{
	while (*cp) {
		cx2++;
		putcursor(*cp, 1);
		if (iseol(cx2, plen, linemax))
			setcursor(s, cx, cx2, len, plen, linemax);
		cp++;
	}
	return(cx2);
}

static VOID NEAR ringbell(VOID_A)
{
#ifndef	_NOORIGSHELL
	if (dumbmode && dumbterm <= 2) putch2('\007');
	else
#endif
	putterm(t_bell);
	tflush();
}

static VOID NEAR clearline(VOID_A)
{
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		int x;

		/* abandon last 1 char because of auto newline */
		for (x = win_x; x < n_column - 1; x++) putch2(' ');
		for (x = win_x; x < n_column - 1; x++) putch2(C_BS);
	}
	else
#endif
	putterm(l_clear);
}

static VOID NEAR newline(y)
int y;
{
#ifndef	_NOORIGSHELL
	if (dumbmode);
	else if (shellmode) forwline(xpos);
	else
#endif
	Xlocate(0, y);
}

static int NEAR rightchar(s, cxp, cx2, len, plen, linemax)
char *s;
int *cxp, cx2, len, plen, linemax;
{
	int rw, vw;

	if (*cxp + 1 < len && iskanji1(s, *cxp)) rw = vw = 2;
#ifdef	CODEEUC
	else if (*cxp + 1 < len && isekana(s, *cxp)) {
		rw = 2;
		vw = 1;
	}
#else
	else if (isskana(s, *cxp)) rw = vw = 1;
#endif
	else if (iscntrl2(s[*cxp])) {
		rw = 1;
		vw = 2;
	}
	else if (ismsb(s[*cxp])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	if (*cxp + rw > len) {
		ringbell();
		return(cx2);
	}
	*cxp += rw;
	cx2 += vw;
#ifndef	_NOORIGSHELL
	if (dumbmode) rewritecursor(s, *cxp, cx2, len, plen, linemax);
	else
#endif
	if (within(cx2, plen, linemax) && (cx2 + plen) % linemax < vw)
		setcursor(s, *cxp, cx2, len, plen, linemax);
	else while (vw--) rightcursor();

	return(cx2);
}

/*ARGSUSED*/
static int NEAR leftchar(s, cxp, cx2, len, plen, linemax)
char *s;
int *cxp, cx2, len, plen, linemax;
{
	int rw, vw, ocx2;

	if (*cxp >= 2 && onkanji1(s, *cxp - 2)) rw = vw = 2;
#ifdef	CODEEUC
	else if (*cxp >= 2 && isekana(s, *cxp - 2)) {
		rw = 2;
		vw = 1;
	}
#else
	else if (isskana(s, *cxp - 1)) rw = vw = 1;
#endif
	else if (iscntrl2(s[*cxp - 1])) {
		rw = 1;
		vw = 2;
	}
	else if (ismsb(s[*cxp - 1])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	ocx2 = cx2;
	*cxp -= rw;
	cx2 -= vw;
#ifndef	_NOORIGSHELL
	if (dumbmode) rewritecursor(s, *cxp, cx2, len, plen, linemax);
	else
#endif
	if ((ocx2 + plen) % linemax < vw)
		setcursor(s, *cxp, cx2, len, plen, linemax);
	else while (vw--) leftcursor();

	return(cx2);
}

static VOID NEAR insertchar(s, cx, len, plen, linemax, ins)
char *s;
int cx, len, plen, linemax, ins;
{
	char *dupl;
	int cx2, dx, dy, i, j, l, f1, ptr, len2, nlen;
#if	!MSDOS
	int f2;
#endif

	dupl = trquote(&(s[cx]), len - cx, &l);
	cx2 = vlen(s, cx);
	len2 = cx2 + l;
	nlen = len2 + ins;
	dy = (cx2 + plen) / linemax;
	i = (dy + 1) * linemax - plen;	/* prev. chars including cursor line */
	j = linemax - (cx2 + plen) % linemax;
	if (j > ins) j = ins;		/* inserted columns in cursor line */

#ifndef	_NOORIGSHELL
	if (dumbmode) {
		i = (lastofs2) ? 1 : plen;
		dx = cx2 + ins - lastofs2;
		if (i + dx < linemax) {
			putcursor(' ', ins);
			dumbputs(dupl, i + cx2, l, linemax - (i + dx), 0);
		}
	}
	else
#endif	/* !_NOORIGSHELL */
#if	!MSDOS
	if (*c_insert) {
# ifndef	_NOORIGSHELL
		if (shellmode);
		else
# endif
		if (ypos + dy >= n_line - 1
		&& xpos + (cx2 + plen) % linemax + (len2 - cx2) + j
		>= n_column) {
			/*
			 * In the case that current line is the last line,
			 * and the end of string will reach over the last line.
			 */
			while (ypos + dy >= n_line - 1) {
				locate(0, n_line - 1);
				putterm(c_scrollforw);
				ypos--;
			}
			hideclock = 1;
			setcursor(s, cx, cx2, len, plen, linemax);
		}
		while (j--) putterm(c_insert);

		if (i < nlen) {
			while (i < nlen) {
				ptr = i - 1 - cx2;
					/* rest columns in cursor line */
				/*
				 * Whether if the end of line is kanji 1st byte
				 * f1: after inserted
				 * f2: before inserted
				 */
				if (ptr < ins) f1 = 0;
				else f1 = vonkanji1(dupl, ptr - ins);
				f2 = (vonkanji1(dupl, ptr)) ? 1 : 0;
				if (xpos + linemax < n_column) {
					Xlocate(linemax, dy);
					j = n_column - (xpos + linemax);
					if (j > ins) j = ins;
					if (f2) j++;
					if (f1 && j-- > 0) rightcursor();
					if (j > 0) putcursor(' ', j);
				}

				dy++;
# ifndef	_NOORIGSHELL
				if (shellmode);
				else
# endif
				if (ypos + dy >= n_line - 1
				&& xpos + nlen - i >= n_column) {
					while (ypos + dy >= n_line - 1) {
						locate(0, n_line - 1);
						putterm(c_scrollforw);
						ypos--;
					}
					hideclock = 1;
				}
				Xlocate(0, dy);
				for (j = 0; j < ins; j++) putterm(c_insert);
				l = ins;
				if (ptr < ins) {
					ptr++;
					Xlocate(ins - ptr, dy);
					l = ptr + f2;
					ptr = 0;
				}
				else {
					ptr -= ins - 1;
					if (f1) dupl[f1 - 1] = dupl[f1] = ' ';
					if (f2) l++;
				}
				if (ptr + l > nlen - cx2) l = nlen - cx2 - ptr;
				kanjiputs2(dupl, l, ptr);
				win_x += l;
				i += linemax;
			}
			setcursor(s, cx, cx2, len, plen, linemax);
		}
	}
	else
#endif	/* !MSDOS */
	{
		putcursor(' ', j);
		j = 0;
		l = i - cx2 - ins;	/* rest chars following inserted str */
		dx = (cx2 + ins + plen) % linemax;
					/* cursor column after inserted */

		while (i < nlen) {
			ptr = i - 1 - cx2;
					/* rest columns in cursor line */
			/*
			 * Whether if the end of line is kanji 1st byte
			 * f1: after inserted
			 */
			if (ptr < ins) f1 = l = 0;
			else f1 = vonkanji1(dupl, ptr - ins);
#ifndef	_NOORIGSHELL
			if (shellmode);
			else
#endif
			if (ypos + dy >= n_line - 1) {
				while (ypos + dy >= n_line - 1) {
					locate(0, n_line - 1);
					putterm(c_scrollforw);
					ypos--;
				}
				hideclock = 1;
				Xlocate(dx, dy);
			}
			if (f1) l++;
			if (l > 0) {
				kanjiputs2(dupl, l, j);
				win_x += l;
			}

			dy++;
			if (f1) newline(dy);
			else {
				putcursor(' ', 1);
				Xlocate(0, dy);
			}

			l = linemax;
			if (ptr < ins - 1) {
				putcursor(' ', 1);
				ptr++;
				Xlocate(ins - ptr, dy);
				l -= ins - ptr;
				ptr = 0;
			}
			else {
				ptr -= ins - 1;
				if (f1) dupl[f1 - 1] = dupl[f1] = ' ';
			}
			if (ptr + l > len2 - cx2) l = len2 - cx2 - ptr;
			j = ptr;
			i += linemax;
			dx = 0;
		}

		l = len2 - cx2 - j;
		if (l > 0) {
			kanjiputs2(dupl, l, j);
			win_x += l;
		}
		setcursor(s, cx, cx2, len, plen, linemax);
	}
#ifndef	_NOORIGSHELL
	if (dumbmode);
	else if (shellmode) {
		if (((nlen + plen) % linemax) == 1) {
			setcursor(s, -1, nlen, len, plen, linemax);
			putterm(l_clear);
			setcursor(s, cx, cx2, len, plen, linemax);
		}
	}
	else
#endif
	if (i == nlen && within(i, plen, linemax) && ypos + dy >= n_line - 1) {
		while (ypos + dy >= n_line - 1) {
			locate(0, n_line - 1);
			putterm(c_scrollforw);
			ypos--;
		}
		hideclock = 1;
		setcursor(s, cx, cx2, len, plen, linemax);
	}
	free(dupl);
}

static VOID NEAR deletechar(s, cx, len, plen, linemax, del)
char *s;
int cx, len, plen, linemax, del;
{
	char *dupl;
	int cx2, dy, i, j, l, f1, ptr, len2, nlen;
#if	!MSDOS
	int f2;
#endif

	dupl = trquote(&(s[cx]), len - cx, &l);
	cx2 = vlen(s, cx);
	len2 = cx2 + l;
	nlen = len2 - del;
	dy = (cx2 + plen) / linemax;
	i = (dy + 1) * linemax - plen;	/* prev. chars including cursor line */

#ifndef	_NOORIGSHELL
	if (dumbmode) {
		i = (lastofs2) ? 1 : plen;
		j = cx2 - lastofs2;
		dumbputs(dupl, i + cx2, l, linemax - (i + j), del);
	}
	else
#endif	/* !_NOORIGSHELL */
#if	!MSDOS
	if (*c_delete) {
		j = linemax - (cx2 + plen) % linemax;
		if (j > del) j = del;	/* deleted columns in cursor line */
		while (j--) putterm(c_delete);

		if (i < len2) {
			while (i < len2) {
				ptr = i - 1 - cx2;
					/* rest columns in cursor line */
				/*
				 * Whether if the end of line is kanji 1st byte
				 * f1: after deleted
				 */
				if (ptr >= nlen - cx2) f1 = 0;
				else f1 = (vonkanji1(dupl, ptr + del)) ? 1 : 0;
				f2 = (vonkanji1(dupl, ptr)) ? 1 : 0;
				j = linemax - del + f2;
				if (j < linemax - (++ptr)) {
					j = linemax - ptr;
					l = ptr + f1;
					ptr = del;
				}
				else {
					l = del - f2 + f1;
					ptr += f2;
				}
				Xlocate(j, dy);
				if (ptr + l > len2) l = len2 - ptr;
				if (l > 0) {
					kanjiputs2(dupl, l, ptr);
					win_x += l;
				}
				if (!f1) putcursor(' ', 1);
				Xlocate(0, ++dy);
				for (j = 0; j < del; j++) putterm(c_delete);
				if (f1) putcursor(' ', 1);
				i += linemax;
			}
			setcursor(s, cx, cx2, len, plen, linemax);
		}
	}
	else
#endif	/* !MSDOS */
	{
		j = del;
		l = i - cx2;	/* rest chars following cursor */

		while (i < len2) {
			ptr = i - 1 - cx2;
				/* rest columns in cursor line */
			/*
			 * Whether if the end of line is kanji 1st byte
			 * f1: after deleted
			 */
			if (ptr >= nlen - cx2) f1 = 0;
			else f1 = vonkanji1(dupl, ptr + del);
			if (f1) l++;
			if (l > 0) {
				kanjiputs2(dupl, l, j);
				win_x += l;
			}

			dy++;
			if (f1) newline(dy);
			else {
				putcursor(' ', 1);
				Xlocate(0, dy);
			}

			l = linemax;
			ptr += del + 1;
			if (f1) dupl[f1 - 1] = dupl[f1] = ' ';
			if (ptr + l > len2 - cx2) l = len2 - cx2 - ptr;
			j = ptr;
			i += linemax;
		}

		l = len2 - cx2 - j;
		if (l > 0) {
			kanjiputs2(dupl, l, j);
			win_x += l;
		}
		clearline();
		setcursor(s, cx, cx2, len, plen, linemax);
	}
	free(dupl);
}

static VOID NEAR insshift(s, cx, len, ins)
char *s;
int cx, len, ins;
{
	int i;

	for (i = len - 1; i >= cx; i--) s[i + ins] = s[i];
}

static VOID NEAR delshift(s, cx, len, del)
char *s;
int cx, len, del;
{
	int i;

	for (i = cx; i < len - del; i++) s[i] = s[i + del];
}

static VOID NEAR truncline(s, cx, cx2, len, plen, linemax)
char *s;
int cx, cx2, len, plen, linemax;
{
	int dy, i, len2;

	len2 = vlen(s, len);
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		clearline();
		return;
	}
#endif
	putterm(l_clear);

	dy = (cx2 + plen) / linemax;
	i = (dy + 1) * linemax - plen;
	if (i < len2) {
#ifndef	_NOORIGSHELL
		if (shellmode) while (i < len2) {
			Xlocate(0, ++dy);
			putterm(l_clear);
			i += linemax;
		}
		else
#endif
		while (i < len2) {
			if (ypos + ++dy >= n_line) break;
			locate(xpos, ypos + dy);
			putterm(l_clear);
			i += linemax;
		}
		setcursor(s, cx, cx2, len, plen, linemax);
	}
}

static VOID NEAR displaystr(s, cxp, lenp, plen, linemax)
char *s;
int *cxp, *lenp, plen, linemax;
{
	char *dupl;
	int i, x, y, cx2, len2, max, vi, width, f;

	dupl = trquote(s, *lenp, &len2);
	cx2 = vlen(s, *cxp);

#ifndef	_NOORIGSHELL
	if (dumbmode) {
		backcursor(0);
		forwcursor(xpos);
		i = plen;
		x = cx2 - linemax;
		y = (linemax + 1) / 2;
		for (lastofs2 = 0; lastofs2 <= i + x; lastofs2 += y) i = 1;
		if (!lastofs2) dispprompt(NULL, 0);
		else {
			putcursor('<', 1);
			if ((f = vonkanji1(dupl, lastofs2 - 1)))
				dupl[f - 1] = dupl[f] = ' ';
		}
		dumbputs(dupl, i + cx2, len2, linemax - i, lastofs2);
		tflush();
		free(dupl);
		return;
	}
#endif

	dispprompt(NULL, 0);
	Xlocate(plen, 0);
	max = maxscr(plen, linemax);
	if (len2 > max) {
		len2 = max;
		*lenp = rlen(s, max);
		if (cx2 >= len2) {
			cx2 = len2;
			*cxp = *lenp;
		}
	}
	vi = (t_nocursor && *t_nocursor && t_normalcursor && *t_normalcursor);
	if (vi) putterms(t_nocursor);
	i = x = y = 0;
	width = linemax - plen;
	while (i + width < len2) {
		/*
		 * Whether if the end of line is kanji 1st byte
		 */
		f = (vonkanji1(dupl, i + width - 1)) ? 1 : 0;
		clearline();
		if (stable_standout) putterm(end_standout);

#ifndef	_NOORIGSHELL
		if (shellmode);
		else
#endif
		if (ypos + y >= n_line - 1) {
			while (ypos + y >= n_line - 1) {
				locate(0, n_line - 1);
				putterm(c_scrollforw);
				ypos--;
			}
			hideclock = 1;
			Xlocate(x, y);
		}
		if (width + f > 0) {
			kanjiputs2(dupl, width + f, i);
			win_x += width + f;
		}

		i += width + f;
		width = linemax - f;
		x = f;
		y++;

		if (f) {
			newline(y);
			putcursor(' ', 1);
		}
		else {
			putcursor(' ', 1);
			Xlocate(0, y);
		}
	}
	clearline();
	if (stable_standout) putterm(end_standout);
	kanjiputs2(dupl, len2 - i, i);
	win_x += len2 - i;
#ifndef	_NOORIGSHELL
	if (shellmode);
	else
#endif
	while (win_y < n_line - 1) {
		locate(xpos, ++win_y);
		putterm(l_clear);
	}
	if (vi) putterms(t_normalcursor);
	setcursor(s, *cxp, cx2, *lenp, plen, linemax);
	tflush();
	free(dupl);
}

static int NEAR preparestr(sp, cx, lenp, plen, sizep, linemax, rins, vins)
char **sp;
int cx, *lenp, plen;
ALLOC_T *sizep;
int linemax, rins, vins;
{
	int rw, vw;

	if (overflow(vlen(*sp, *lenp) + vins, plen, linemax)) return(-1);

	if (!overwritemode) {
		insertchar(*sp, cx, *lenp, plen, linemax, vins);
		*sp = c_realloc(*sp, *lenp + rins, sizep);
		insshift(*sp, cx, *lenp, rins);
		*lenp += rins;
		return(0);
	}

	if (cx >= *lenp) rw = vw = 0;
	else if (cx + 1 < *lenp && iskanji1(*sp, cx)) rw = vw = 2;
#ifdef	CODEEUC
	else if (cx + 1 < *lenp && isekana(*sp, cx)) {
		rw = 2;
		vw = 1;
	}
#else
	else if (isskana(*sp, cx)) rw = vw = 1;
#endif
	else if (iscntrl2((*sp)[cx])) {
		rw = 1;
		vw = 2;
	}
	else if (ismsb((*sp)[cx])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	if (!vw) /*EMPTY*/;
	else if (vins > vw)
		insertchar(*sp, cx, *lenp, plen, linemax, vins - vw);
	else if (vins < vw)
		deletechar(*sp, cx, *lenp, plen, linemax, vw - vins);

	if (!rw) /*EMPTY*/;
	else if (rins > rw) {
		*sp = c_realloc(*sp, *lenp + rins - rw, sizep);
		insshift(*sp, cx, *lenp, rins - rw);
	}
	else if (rins < rw) delshift(*sp, cx, *lenp, rw - rins);

	*lenp += rins - rw;
	return(0);
}

static int NEAR insertstr(sp, cx, len,
	plen, sizep, linemax, strins, ins, quote)
char **sp;
int cx, len, plen;
ALLOC_T *sizep;
int linemax;
char *strins;
int ins, quote;
{
	char *dupl;
	int i, j, cx2, dx, dy, f, l, ptr, len2, max;

	len2 = vlen(*sp, len);
	max = maxscr(plen, linemax);
	if (len2 + vlen(strins, ins) > max) ins = rlen(strins, max - len2);

	if (ins > 0) {
		if (onkanji1(strins, ins - 1)) ins--;
#ifdef	CODEEUC
		else if (isekana(strins, ins - 1)) ins--;
#endif
	}
	if (ins <= 0) return(0);

	dupl = malloc2(ins * 2 + 1);
	insertchar(*sp, cx, len, plen, linemax, vlen(strins, ins));
	*sp = c_realloc(*sp, len + ins, sizep);
	insshift(*sp, cx, len, ins);
	for (i = 0; i < ins; i++) (*sp)[cx + i] = ' ';
	len += ins;
	for (i = j = 0; i < ins; i++, j++) {
#ifdef	CODEEUC
		if (isekana(strins, i)) {
			dupl[j] = (*sp)[cx + j] = strins[i];
			j++;
			dupl[j] = (*sp)[cx + j] = strins[++i];
		}
		else
#endif
		if (iskanji1(strins, i)) {
			dupl[j] = (*sp)[cx + j] = strins[i];
			j++;
			dupl[j] = (*sp)[cx + j] = strins[++i];
		}
#ifdef	FAKEMETA
		else if (strchr(DQ_METACHAR, strins[i])
		|| (!quote && strchr(METACHAR, strins[i]))) {
#else	/* !FAKEMETA */
		else if ((quote == '\'' && strins[i] == '\'')
		|| (quote == '"' && strins[i] == '!')) {
			f = 3;
			if (!strins[i + 1]) f--;
			insertchar(*sp, cx, len, plen, linemax, f);
			*sp = c_realloc(*sp, len + f, sizep);
			insshift(*sp, cx + j, len, f);
			dupl[j] = (*sp)[cx + j] = quote;
			j++;
			dupl[j] = (*sp)[cx + j] = PMETA;
			j++;
			dupl[j] = (*sp)[cx + j] = strins[i];
			j++;
			dupl[j] = (*sp)[cx + j] = quote;
		}
		else if ((quote == '"' && strchr(DQ_METACHAR, strins[i]))
		|| (!quote && strchr(METACHAR, strins[i]))) {
#endif	/* !FAKEMETA */
			insertchar(*sp, cx, len, plen, linemax, 1);
			*sp = c_realloc(*sp, len + 1, sizep);
			insshift(*sp, cx + j, len, 1);
#ifdef	FAKEMETA
			dupl[j] = (*sp)[cx + j] = strins[i];
#else
			dupl[j] = (*sp)[cx + j] = PMETA;
#endif
			j++;
			dupl[j] = (*sp)[cx + j] = strins[i];
		}
		else dupl[j] = (*sp)[cx + j] = strins[i];
	}
	dupl[j] = '\0';

	cx2 = vlen(*sp, cx);
	len2 = vlen(dupl, j);
	dx = (cx2 + plen) % linemax;
	dy = (cx2 + plen) / linemax;
	i = (dy + 1) * linemax - plen - cx2;
	ptr = 0;
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		i = (lastofs2) ? 1 : plen;
		dx = cx2 - lastofs2 + len2;
		if (i + dx >= linemax) {
			cx += j;
			displaystr(*sp, &cx, &len, plen, linemax);
			ptr = len2;
		}
	}
	else
#endif
	while (i < len2) {
		/*
		 * Whether if the end of line is kanji 1st byte
		 * f: after inserted
		 */
		f = vonkanji1(dupl, i - 1);
#ifndef	_NOORIGSHELL
		if (shellmode);
		else
#endif
		if (ypos + dy >= n_line - 1) {
			while (ypos + dy >= n_line - 1) {
				locate(0, n_line - 1);
				putterm(c_scrollforw);
				ypos--;
			}
			hideclock = 1;
			Xlocate(dx, dy);
		}
		l = i - ptr;
		if (f) l++;
		kanjiputs2(dupl, l, ptr);
		win_x += l;
		newline(++dy);
		if (f) dupl[f - 1] = dupl[f] = ' ';
		ptr = i;
		dx = 0;
		i += linemax;
	}
	if (len2 > ptr) {
		kanjiputs2(dupl, len2 - ptr, ptr);
		win_x += len2 - ptr;
	}
	free(dupl);
	return(j);
}

#ifndef	_NOCOMPLETE
static VOID NEAR selectfile(argc, argv)
int argc;
char **argv;
{
	static int maxselect, tmpcolumns;
	int i, len, maxlen, dupminfilename, dupcolumns, dupsorton;

	dupminfilename = minfilename;
	dupcolumns = curcolumns;
	minfilename = n_column;

	if (argv) {
		selectlist = (namelist *)malloc2(argc * sizeof(namelist));
		maxlen = 0;
		for (i = 0; i < argc; i++) {
			memset((char *)&(selectlist[i]), 0, sizeof(namelist));
			selectlist[i].name = strdup2(argv[i]);
			selectlist[i].flags = (F_ISRED | F_ISWRI);
			selectlist[i].tmpflags = F_STAT;
			len = strlen2(argv[i]);
			if (maxlen < len) maxlen = len;
		}
		dupsorton = sorton;
		sorton = 1;
		qsort(selectlist, argc, sizeof(namelist), cmplist);
		sorton = dupsorton;

		if (lcmdline < 0) {
			int j, n;

			n = 1;
			cputnl();
			if (argc < LIMITSELECTWARN) i = 'Y';
			else {
				cprintf2("There are %d possibilities.", argc);
				cputs2("  Do you really");
				cputnl();
				cputs2("wish to see them all? (y or n)");
				tflush();
				for (;;) {
					i = toupper2(getkey2(0));
					if (i == 'Y' || i == 'N') break;
					ringbell();
					tflush();
				}
				cputnl();
				n += 2;
			}

			if (i == 'N') /*EMPTY*/;
			else if (n_column < maxlen + 2) {
				n += argc;
				for (i = 0; i < argc; i++) {
					kanjiputs(selectlist[i].name);
					cputnl();
				}
			}
			else {
				tmpcolumns = n_column / (maxlen + 2);
				len = (argc + tmpcolumns - 1) / tmpcolumns;
				n += len;
				for (i = 0; i < len; i++) {
					for (j = i; j < argc; j += len)
						cprintf2("%-*.*k",
							maxlen + 2, maxlen + 2,
							selectlist[j].name);
					cputnl();
				}
			}

# ifndef	_NOORIGSHELL
			if (dumbmode || shellmode) {
				win_x = xpos;
				win_y = 0;
			}
			else
# endif
			if (ypos + n < n_line - 1) ypos += n;
			else ypos = n_line - 1;
			for (i = 0; i < argc; i++) free(selectlist[i].name);
			free(selectlist);
			selectlist = NULL;
			minfilename = dupminfilename;
			curcolumns = dupcolumns;

			return;
		}

		if ((n_column / 5) - 2 - 1 >= maxlen) tmpcolumns = 5;
		else if ((n_column / 3) - 2 - 1 >= maxlen) tmpcolumns = 3;
		else if ((n_column / 2) - 2 - 1 >= maxlen) tmpcolumns = 2;
		else tmpcolumns = 1;
		curcolumns = tmpcolumns;
		tmpfilepos = listupfile(selectlist, argc, NULL);
		maxselect = argc;
	}
	else if (argc < 0) {
		for (i = 0; i < maxselect; i++) free(selectlist[i].name);
		free(selectlist);
		selectlist = NULL;
	}
	else {
		curcolumns = tmpcolumns;
		if (tmpfilepos >= maxselect) tmpfilepos %= (tmpfilepos - argc);
		else if (tmpfilepos < 0) tmpfilepos = maxselect - 1
			- ((maxselect - 1 - argc) % (argc - tmpfilepos));
		if (argc / FILEPERPAGE != tmpfilepos / FILEPERPAGE)
			tmpfilepos = listupfile(selectlist, maxselect,
				selectlist[tmpfilepos].name);
		else if (argc != tmpfilepos) {
			putname(selectlist, argc, -1);
			putname(selectlist, tmpfilepos, 1);
		}
	}

	minfilename = dupminfilename;
	curcolumns = dupcolumns;
}

static int NEAR completestr(sp, cx, len, plen, sizep, linemax, comline, cont)
char **sp;
int cx, len, plen;
ALLOC_T *sizep;
int linemax, comline, cont;
{
# ifndef	FAKEMETA
	int hadmeta;
# endif
# ifndef	_NOORIGSHELL
	int vartop;
# endif
	char *cp, *tmp, **argv;
	int i, l, pc, ins, top, fix, argc, quote, quoted, hasmeta;

	if (selectlist && cont > 0) {
		selectfile(tmpfilepos++, NULL);
		setcursor(*sp, cx, -1, len, plen, linemax);
		return(0);
	}

	quote = '\0';
# ifndef	FAKEMETA
	hadmeta =
# endif
# ifndef	_NOORIGSHELL
	vartop =
# endif
	top = quoted = 0;
	for (i = 0; i < cx; i++) {
# ifdef	FAKEMETA
		pc = parsechar(&((*sp)[i]), cx, '$', EA_NOEVALQ, &quote, NULL);
# else
		pc = parsechar(&((*sp)[i]), cx, '$', EA_BACKQ, &quote, NULL);
# endif
		if (pc == PC_CLQUOTE) quoted = i;
		else if (pc == PC_WORD) i++;
# ifndef	_NOORIGSHELL
		else if (pc == '$') vartop = i + 1;
# endif
# ifndef	FAKEMETA
		else if (pc == PC_META) {
			i++;
			hadmeta++;
		}
		else if (pc == PC_OPQUOTE) {
			if ((*sp)[i] == '`') top = i + 1;
		}
# endif
		else if (pc != PC_NORMAL) /*EMPTY*/;
		else if ((*sp)[i] == '=' || strchr(CMDLINE_DELIM, (*sp)[i]))
			top = i + 1;
	}
	if (comline && top > 0) {
		for (i = top - 1; i >= 0; i--)
			if ((*sp)[i] != ' ' && (*sp)[i] != '\t') break;
		if (i >= 0 && !strchr(SHELL_OPERAND, (*sp)[i])) comline = 0;
	}
# ifndef	_NOORIGSHELL
	if (vartop) {
		i = top + 1;
		if (quote) i++;
		if (vartop != i || vartop >= cx || !isidentchar((*sp)[vartop]))
			vartop = 0;
		else {
			for (i = vartop; i < cx; i++)
				if (!isidentchar2((*sp)[i])) break;
			if (i < cx) vartop = 0;
			else top = vartop;
		}
	}
# endif	/* !_NOORIGSHELL */

	cp = strndup2(&((*sp)[top]), cx - top);
# ifndef	_NOORIGSHELL
	if (vartop);
	else
# endif
	cp = evalpath(cp, 0);
	hasmeta = 0;
	for (i = 0; cp[i]; i++) {
		if (strchr(METACHAR, cp[i])) {
			hasmeta = 1;
			break;
		}
		if (iskanji1(cp, i)) i++;
	}

	if (selectlist && cont < 0) {
		argv = (char **)malloc2(1 * sizeof(char *));
		argv[0] = strdup2(selectlist[tmpfilepos].name);
		argc = 1;
	}
# ifndef	_NOORIGSHELL
	else if (vartop) {
		argv = NULL;
		l = strlen(cp);
		argc = completeshellvar(cp, l, 0, &argv);
	}
# endif
	else {
		argv = NULL;
		argc = 0;
		l = strlen(cp);
		if (comline && !strdelim(cp, 1)) {
# ifdef	_NOORIGSHELL
			argc = completeuserfunc(cp, l, argc, &argv);
			argc = completealias(cp, l, argc, &argv);
			argc = completebuiltin(cp, l, argc, &argv);
			argc = completeinternal(cp, l, argc, &argv);
# else
			argc = completeshellcomm(cp, l, argc, &argv);
# endif
		}
# ifndef	_NOARCHIVE
		if (archivefile && !comline && *cp != _SC_)
			argc = completearch(cp, l, argc, &argv);
		else
# endif
		argc = completepath(cp, l, argc, &argv, comline);
		if (!argc && comline)
			argc = completepath(cp, l, argc, &argv, 0);
	}

	ins = strlen(getbasename(cp));
	free(cp);
	if (!argc) {
		ringbell();
		if (argv) free(argv);
		return(0);
	}

	cp = findcommon(argc, argv);
	fix = '\0';
	if (argc == 1 && cp)
		fix = ((tmp = strrdelim(cp, 0)) && !tmp[1]) ? _SC_ : ' ';

	if (!cp || ((ins = (int)strlen(cp) - ins) <= 0 && fix != ' ')) {
		if (cont <= 0) {
			ringbell();
			l = 0;
		}
		else {
			selectfile(argc, argv);
			if (lcmdline < 0)
				displaystr(*sp, &cx, &len, plen, linemax);
			setcursor(*sp, cx, -1, len, plen, linemax);
			l = -1;
		}
		for (i = 0; i < argc; i++) free(argv[i]);
		free(argv);
		if (cp) free(cp);
		return(l);
	}
	for (i = 0; i < argc; i++) free(argv[i]);
	free(argv);

	l = 0;
	if (!hasmeta) for (i = 0; cp[i]; i++) {
		if (strchr(METACHAR, cp[i])) {
			hasmeta = 1;
			break;
		}
		if (iskanji1(cp, i)) i++;
	}

	if (hasmeta) {
		char *home;
		int hlen;

		if (quote || quoted > top || (*sp)[top] != '~') {
			home = NULL;
			i = hlen = 0;
		}
		else {
			tmp = &((*sp)[top]);
			(*sp)[len] = '\0';
			home = malloc2(len - top + 1);
			hlen = evalhome(&home, 0, &tmp);
			i = ++tmp - &((*sp)[top]);
		}

		if (quote) /*EMPTY*/;
		else if (quoted > top) {
			quote = (*sp)[quoted];
			setcursor(*sp, quoted, -1, len, plen, linemax);
			deletechar(*sp, quoted, len, plen, linemax, 1);
			delshift(*sp, quoted, len--, 1);
			l--;
			setcursor(*sp, --cx, -1, len, plen, linemax);
		}
		else if (!overflow(vlen(*sp, len - i) + vlen(home, hlen),
		plen, linemax)) {
			if (home) {
				setcursor(*sp, top, -1, len, plen, linemax);
				deletechar(*sp, top, len, plen, linemax, i);
				delshift(*sp, top, len, i);
				len -= i;
				l -= i;
				cx -= i;
				i = insertstr(sp, top, len, plen, sizep,
					linemax, home, hlen, '\0');
				len += i;
				l += i;
				cx += i;
				free(home);
			}
# ifndef	FAKEMETA
			if (hadmeta) for (i = top; i < cx; i++) {
				pc = parsechar(&((*sp)[i]), cx,
					'\0', 0, &quote, NULL);
				if (pc == PC_WORD) {
					i++;
					continue;
				}
				if (pc != PC_META) continue;
				if (strchr(DQ_METACHAR, (*sp)[i + 1]))
					continue;
				setcursor(*sp, i, -1, len, plen, linemax);
				deletechar(*sp, i, len, plen, linemax, 1);
				delshift(*sp, i, len--, 1);
				l--;
				cx--;
			}
# endif	/* !FAKEMETA */
			setcursor(*sp, top, -1, len, plen, linemax);
			insertchar(*sp, top, len, plen, linemax, 1);
			*sp = c_realloc(*sp, len + 2, sizep);
			insshift(*sp, top, len++, 1);
			l++;
			(*sp)[top] = quote = '"';
			putcursor(quote, 1);
			setcursor(*sp, ++cx, -1, len, plen, linemax);
		}
		else hasmeta = 0;
	}

	tmp = cp + (int)strlen(cp) - ins;
	if (fix == _SC_) ins--;
	i = insertstr(sp, cx, len, plen, sizep, linemax, tmp, ins, quote);
	len += i;
	l += i;
	if (fix && !overflow(vlen(*sp, len), plen, linemax)) {
		cx += i;
		if (quote && (fix != _SC_ || hasmeta)) {
			insertchar(*sp, cx, len, plen, linemax, 1);
			*sp = c_realloc(*sp, len + 2, sizep);
			insshift(*sp, cx, len++, 1);
			l++;
			(*sp)[cx++] = quote;
			putcursor(quote, 1);
		}
		if (!overflow(vlen(*sp, len), plen, linemax)) {
			insertchar(*sp, cx, len, plen, linemax, 1);
			*sp = c_realloc(*sp, len + 1, sizep);
			insshift(*sp, cx, len, 1);
			l++;
			(*sp)[cx] = fix;
			putcursor(fix, 1);
		}
	}

	free(cp);
	return(l);
}
#endif	/* !_NOCOMPLETE */

static int NEAR _inputstr_up(sp, cxp, cx2, lenp,
	plen, sizep, linemax, histnop, h, tmp)
char **sp;
int *cxp, cx2, *lenp, plen;
ALLOC_T *sizep;
int linemax, *histnop, h;
char **tmp;
{
#ifndef	_NOORIGSHELL
	int len2;
#endif
	int i, j;

	keyflush();
#ifndef	_NOCOMPLETE
	if (h >= 0 && selectlist) {
		selectfile(tmpfilepos--, NULL);
		setcursor(*sp, *cxp, cx2, *lenp, plen, linemax);
	}
	else
#endif
#ifdef	_NOORIGSHELL
	if (cx2 < linemax)
#else
	if (dumbmode || cx2 < linemax)
#endif
	{
		if (h < 0 || !history[h] || *histnop >= (int)histsize[h]
		|| !history[h][*histnop]) {
			ringbell();
			return(cx2);
		}
		if (!*tmp) {
			(*sp)[*lenp] = '\0';
			*tmp = strdup2(*sp);
		}
		for (i = j = 0; history[h][*histnop][i]; i++) {
			*sp = c_realloc(*sp, j, sizep);
			(*sp)[j++] = history[h][*histnop][i];
		}
		(*sp)[j] = '\0';
#ifndef	_NOORIGSHELL
		len2 = vlen(*sp, *lenp);
		if (dumbmode) {
			setcursor(*sp, 0, 0, *lenp, plen, linemax);
			clearline();
		}
#endif
		*cxp = *lenp = j;
		displaystr(*sp, cxp, lenp, plen, linemax);
#ifndef	_NOORIGSHELL
		if (dumbmode);
		else if (shellmode) {
			int y1, y2;

			y1 = (len2 + plen) / linemax;
			y2 = (vlen(*sp, *lenp) + plen) / linemax;
			if (y1 > y2) {
				while (y1 > y2) {
					Xlocate(0, y1--);
					putterm(l_clear);
				}
				setcursor(*sp, *cxp, -1, *lenp, plen, linemax);
			}
		}
#endif
		cx2 = vlen(*sp, *cxp);
		(*histnop)++;
	}
	else {
		if (!(within(cx2, plen, linemax))
		&& !((cx2 + plen) % linemax)) {
			leftcursor();
			cx2--;
		}
		cx2 -= linemax;
		*cxp = rlen(*sp, cx2);
		upcursor();
		if (onkanji1(*sp, *cxp - 1)) {
			leftcursor();
			cx2--;
			(*cxp)--;
		}
	}
	return(cx2);
}

static int NEAR _inputstr_down(sp, cxp, cx2, lenp,
	plen, sizep, linemax, histnop, h, tmp)
char **sp;
int *cxp, cx2, *lenp, plen;
ALLOC_T *sizep;
int linemax, *histnop, h;
char **tmp;
{
	int i, j, len2;

	len2 = vlen(*sp, *lenp);
	keyflush();
#ifndef	_NOCOMPLETE
	if (h >= 0 && selectlist) {
		selectfile(tmpfilepos++, NULL);
		setcursor(*sp, *cxp, cx2, *lenp, plen, linemax);
	}
	else
#endif
#ifdef	_NOORIGSHELL
	if (cx2 + linemax > len2 || (cx2 + linemax == len2
	&& !within(len2, plen, linemax) && !((len2 + plen) % linemax)))
#else
	if (dumbmode || (cx2 + linemax > len2 || (cx2 + linemax == len2
	&& !within(len2, plen, linemax) && !((len2 + plen) % linemax))))
#endif
	{
		if (h < 0 || !history[h] || *histnop <= 0) {
			ringbell();
			return(cx2);
		}
		if (--(*histnop) > 0) {
			for (i = j = 0; history[h][*histnop - 1][i]; i++) {
				*sp = c_realloc(*sp, j, sizep);
				(*sp)[j++] = history[h][*histnop - 1][i];
			}
			(*sp)[j] = '\0';
		}
		else if (!*tmp) j = 0;
		else {
			j = strlen(*tmp);
			*sp = c_realloc(*sp, j, sizep);
			strncpy(*sp, *tmp, j);
			free(*tmp);
			*tmp = NULL;
		}
#ifndef	_NOORIGSHELL
		len2 = vlen(*sp, *lenp);
		if (dumbmode) {
			setcursor(*sp, 0, 0, *lenp, plen, linemax);
			clearline();
		}
#endif
		*cxp = *lenp = j;
		displaystr(*sp, cxp, lenp, plen, linemax);
#ifndef	_NOORIGSHELL
		if (dumbmode);
		else if (shellmode) {
			int y1, y2;

			y1 = (len2 + plen) / linemax;
			y2 = (vlen(*sp, *lenp) + plen) / linemax;
			if (y1 > y2) {
				while (y1 > y2) {
					Xlocate(0, y1--);
					putterm(l_clear);
				}
				setcursor(*sp, *cxp, -1, *lenp, plen, linemax);
			}
		}
#endif
		cx2 = vlen(*sp, *cxp);
	}
	else {
		cx2 += linemax;
		*cxp = rlen(*sp, cx2);
		downcursor();
		if (onkanji1(*sp, *cxp - 1)) {
			leftcursor();
			cx2--;
			(*cxp)--;
		}
	}
	return(cx2);
}

static int NEAR _inputstr_delete(s, cx, len, plen, linemax)
char *s;
int cx, len, plen, linemax;
{
	int rw, vw;

	if (cx >= len) {
		ringbell();
		return(len);
	}

	if (iskanji1(s, cx)) {
		if (cx + 1 >= len) {
			ringbell();
			return(len);
		}
		rw = vw = 2;
	}
#ifdef	CODEEUC
	else if (isekana(s, cx)) {
		if (cx + 1 >= len) {
			ringbell();
			return(len);
		}
		rw = 2;
		vw = 1;
	}
#else
	else if (isskana(s, cx)) rw = vw = 1;
#endif
	else if (iscntrl2(s[cx])) {
		rw = 1;
		vw = 2;
	}
	else if (ismsb(s[cx])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	deletechar(s, cx, len, plen, linemax, vw);
	delshift(s, cx, len, rw);
	return(len -= rw);
}

static int NEAR _inputstr_enter(sp, cxp, cx2, lenp, plen, sizep, linemax)
char **sp;
int *cxp, cx2, *lenp, plen;
ALLOC_T *sizep;
int linemax;
{
	int i, quote;

	if (!curfilename) {
		ringbell();
		return(cx2);
	}

	quote = 0;
	keyflush();
	if (curfilename[i = 0] != '~') for (; curfilename[i]; i++) {
		if (strchr(METACHAR, curfilename[i])) break;
		if (iskanji1(curfilename, i)) i++;
	}
	if (curfilename[i]
	&& !overflow(vlen(*sp, *lenp) + vlen(curfilename, strlen(curfilename)),
	plen, linemax)) {
		insertchar(*sp, *cxp, *lenp, plen, linemax, 1);
		*sp = c_realloc(*sp, *lenp + 2, sizep);
		insshift(*sp, *cxp, (*lenp)++, 1);
		(*sp)[(*cxp)++] = quote = '"';
		putcursor(quote, 1);
	}
	i = insertstr(sp, *cxp, *lenp, plen, sizep,
		linemax, curfilename, strlen(curfilename), quote);
	if (!i) {
		ringbell();
		return(cx2);
	}
	*cxp += i;
	*lenp += i;
	if (quote) {
		insertchar(*sp, *cxp, *lenp, plen, linemax, 1);
		*sp = c_realloc(*sp, *lenp + 2, sizep);
		insshift(*sp, *cxp, (*lenp)++, 1);
		(*sp)[(*cxp)++] = quote;
		putcursor(quote, 1);
	}
	cx2 = vlen(*sp, *cxp);
	if (iseol(cx2, plen, linemax))
		setcursor(*sp, *cxp, cx2, *lenp, plen, linemax);
	return(cx2);
}

#if	FD >= 2
static int NEAR _inputstr_case(s, cxp, cx2, len, plen, linemax, upper)
char *s;
int *cxp, cx2, len, plen, linemax, upper;
{
	int ch;

	keyflush();
	if (*cxp >= len) {
		ringbell();
		return(cx2);
	}
	if (iskanji1(s, *cxp));
# ifdef	CODEEUC
	else if (isekana(s, *cxp));
# endif
	else {
		ch = (upper) ? toupper2(s[*cxp]) : tolower2(s[*cxp]);
		if (ch != s[*cxp]) {
			s[(*cxp)++] = ch;
			cx2++;
			putcursor(ch, 1);
#ifndef	_NOORIGSHELL
			if (dumbmode)
				checkcursor(s, *cxp, cx2, len, plen, linemax);
			else
#endif
			if (within(cx2, plen, linemax)
			&& (cx2 + plen) % linemax < 1)
				setcursor(s, *cxp, cx2, len, plen, linemax);
			else win_x++;
			return(cx2);
		}
	}

	return(rightchar(s, cxp, cx2, len, plen, linemax));
}
#endif	/* FD >= 2 */

static int NEAR _inputstr_input(sp, cxp, cx2, lenp, plen, sizep, linemax, ch)
char **sp;
int *cxp, cx2, *lenp, plen;
ALLOC_T *sizep;
int linemax, ch;
{
#ifndef	_NOKANJICONV
	char tmpkanji[3];
#endif
	int rw, vw, ch2;

#ifndef	_NOKANJICONV
	if (inputkcode == EUC && isekana2(ch)) {
		rw = KANAWID;
		vw = 1;
		ch = C_EKANA;
		ch2 = (ch & 0xff);
		if (preparestr(sp, *cxp, lenp,
		plen, sizep, linemax, rw, vw) < 0) {
			ringbell();
			keyflush();
			return(cx2);
		}
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		kanjiconv(&((*sp)[*cxp]), tmpkanji, 2,
			inputkcode, DEFCODE, L_INPUT);
		cprintf2("%.*k", 1, &((*sp)[*cxp]));
		win_x += 1;
		*cxp += KANAWID;
	}
	else
#else	/* _NOKANJICONV */
# ifdef	CODEEUC
	if (isekana2(ch)) {
		rw = KANAWID;
		vw = 1;
		ch = C_EKANA;
		ch2 = (ch & 0xff);
		if (preparestr(sp, *cxp, lenp,
		plen, sizep, linemax, rw, vw) < 0) {
			ringbell();
			keyflush();
			return(cx2);
		}
		(*sp)[(*cxp)++] = ch;
		(*sp)[(*cxp)++] = ch2;
		putch2(ch);
		putcursor(ch2, 1);
	}
	else
# endif
#endif	/* _NOKANJICONV */
	if (isinkanji1(ch)) {
		rw = vw = 2;
		ch2 = (kbhit2(WAITMETA * 1000L)) ? getkey2(0) : '\0';
		if (!isinkanji2(ch2)
		|| preparestr(sp, *cxp, lenp,
		plen, sizep, linemax, rw, vw) < 0) {
			ringbell();
			keyflush();
			return(cx2);
		}

#ifdef	_NOKANJICONV
		(*sp)[*cxp] = ch;
		(*sp)[*cxp + 1] = ch2;
#else
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		kanjiconv(&((*sp)[*cxp]), tmpkanji, 2,
			inputkcode, DEFCODE, L_INPUT);
#endif
		cprintf2("%.*k", 2, &((*sp)[*cxp]));
		win_x += 2;
		*cxp += 2;
	}
	else {
		rw = vw = 1;
		if (ch >= K_MIN || iscntrl2(ch) || ismsb(ch)
		|| preparestr(sp, *cxp, lenp,
		plen, sizep, linemax, rw, vw) < 0) {
			ringbell();
			keyflush();
			return(cx2);
		}
		(*sp)[(*cxp)++] = ch;
		putcursor(ch, 1);
	}
	cx2 = vlen(*sp, *cxp);

#ifndef	_NOORIGSHELL
	if (dumbmode) checkcursor(*sp, *cxp, cx2, *lenp, plen, linemax);
	else
#endif
	if (within(cx2, plen, linemax) && ((cx2 + plen) % linemax) < vw)
		setcursor(*sp, *cxp, cx2, *lenp, plen, linemax);
	return(cx2);
}

static int NEAR _inputstr(sp, plen, size, linemax, def, comline, h)
char **sp;
int plen;
ALLOC_T size;
int linemax, def, comline, h;
{
#if	!MSDOS
	keyseq_t key;
#endif
	char *tmphist, buf[5];
	int i, ch, ch2, cx, cx2, ocx2, len, hist, quote, sig;

	subwindow = 1;

#ifndef	_NOORIGSHELL
	if (dumbmode) linemax -= RIGHTMARGIN;
	if (shellmode) sig = 0;
	else
#endif
	sig = 1;
	Xgetkey(-1, 0);
#ifndef	_NOCOMPLETE
	tmpfilepos = -1;
#endif
	cx = len = strlen(*sp);
	if (def >= 0 && def < linemax) {
		while (def > len) {
			*sp = c_realloc(*sp, len, &size);
			(*sp)[len++] = ' ';
		}
		cx = def;
	}
	displaystr(*sp, &cx, &len, plen, linemax);
	cx2 = vlen(*sp, cx);
	keyflush();
	hist = 0;
	tmphist = NULL;
	quote = 0;
	ch = -1;

	do {
		tflush();
		ch2 = ch;
		ocx2 = cx2;
		if (!quote) {
#ifndef	_NOORIGSHELL
			if (shellmode && !len) {
				if ((ch = Xgetkey(sig, 1)) < 0) {
					ch = K_ESC;
					break;
				}
			}
			else
#endif
			ch = Xgetkey(sig, 0);
#ifndef	_NOORIGSHELL
			if (shellmode && !comline && ch == cc_intr) break;
#endif
		}
		else {
			i = ch = getkey2(sigalrm(sig));
			quote = 0;
#if	MSDOS
			switch (i) {
				case K_BS:
					i = C_BS;
					break;
				case K_DC:
					i = C_DEL;
					break;
				default:
					break;
			}
#else
			key.code = i;
			if (getkeyseq(&key) >= 0 && key.len == 1)
				i = *(key.str);
#endif
#ifndef	_NOKANJICONV
			if (inputkcode == EUC && isekana2(i)) /*EMPTY*/;
			else
#else
# ifdef	CODEEUC
			if (isekana2(i)) /*EMPTY*/;
			else
# endif
#endif
			if (i >= K_MIN) {
				ringbell();
				continue;
			}
			else if (iscntrl2(i)) {
				keyflush();
				ch = '\0';
				if (!i) continue;
				if (preparestr(sp, cx, &len,
				plen, &size, linemax, 1, 2) < 0) {
					ringbell();
					continue;
				}
				(*sp)[cx++] = i;
				snprintf2(buf, sizeof(buf), "^%c",
					(i + '@') & 0x7f);
				cx2 = putstr(buf, *sp, cx, cx2,
					len, plen, linemax);
				continue;
			}
#ifndef	_NOKANJICONV
			else if (inputkcode != EUC && iskana2(i)) /*EMPTY*/;
#else
# ifndef	CODEEUC
			else if (iskana2(i)) /*EMPTY*/;
# endif
#endif
			else if (isinkanji1(i) && kbhit2(WAITMETA * 1000L))
				/*EMPTY*/;
			else if (ismsb(i)) {
				keyflush();
				ch = '\0';
				if (preparestr(sp, cx, &len,
				plen, &size, linemax, 1, 4) < 0) {
					ringbell();
					continue;
				}
				(*sp)[cx++] = i;
				snprintf2(buf, sizeof(buf), "\\%03o", i);
				cx2 = putstr(buf, *sp, cx, cx2,
					len, plen, linemax);
				continue;
			}
		}
		switch (ch) {
			case K_RIGHT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (h >= 0 && selectlist) {
					i = tmpfilepos;
					tmpfilepos += FILEPERROW;
					selectfile(i, NULL);
					ocx2 = -1;
				}
				else
#endif
				if (cx >= len) ringbell();
				else ocx2 = cx2 = rightchar(*sp, &cx, cx2,
					len, plen, linemax);
				break;
			case K_LEFT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (h >= 0 && selectlist) {
					i = tmpfilepos;
					tmpfilepos -= FILEPERROW;
					selectfile(i, NULL);
					ocx2 = -1;
				}
				else
#endif
				if (cx <= 0) ringbell();
				else ocx2 = cx2 = leftchar(*sp, &cx, cx2,
					len, plen, linemax);
				break;
			case K_BEG:
				keyflush();
				cx = cx2 = 0;
				break;
			case K_EOL:
				keyflush();
				cx = len;
				cx2 = vlen(*sp, cx);
				break;
			case K_BS:
				keyflush();
				if (cx <= 0) {
					ringbell();
					break;
				}
				ocx2 = cx2 = leftchar(*sp, &cx, cx2,
					len, plen, linemax);
				len = _inputstr_delete(*sp, cx,
					len, plen, linemax);
				break;
			case K_DC:
				keyflush();
				len = _inputstr_delete(*sp, cx,
					len, plen, linemax);
				break;
			case K_DL:
				keyflush();
				if (cx < len)
					truncline(*sp, cx, cx2,
						len, plen, linemax);
				len = cx;
				break;
			case K_CTRL('L'):
				keyflush();
#ifndef	_NOORIGSHELL
				if (dumbmode)
					rewritecursor(*sp, 0, 0,
						len, plen, linemax);
				else if (shellmode) {
					Xlocate(0, 0);
					putterm(l_clear);
				}
				else
#endif
				{
					if (dupn_line >= 0
					&& dupn_line != n_line)
						ypos += n_line - dupn_line;
					dupn_line = n_line;
					for (i = 0; i < WCMDLINE; i++) {
						if (ypos + i >= n_line) break;
						locate(xpos, ypos + i);
						putterm(l_clear);
					}
				}
				displaystr(*sp, &cx, &len, plen, linemax);
				break;
			case K_UP:
				ocx2 = cx2 = _inputstr_up(sp, &cx, cx2, &len,
					plen, &size, linemax,
					&hist, h, &tmphist);
				break;
			case K_DOWN:
				ocx2 = cx2 = _inputstr_down(sp, &cx, cx2, &len,
					plen, &size, linemax,
					&hist, h, &tmphist);
				break;
			case K_IL:
				keyflush();
				quote = 1;
				break;
			case K_ENTER:
				ocx2 = cx2 = _inputstr_enter(sp, &cx, cx2,
					&len, plen, &size, linemax);
				break;
#if	FD >= 2
			case K_IC:
				keyflush();
				overwritemode = 1 - overwritemode;
				break;
			case K_PPAGE:
				ocx2 = cx2 = _inputstr_case(*sp, &cx, cx2,
					len, plen, linemax, 0);
				break;
			case K_NPAGE:
				ocx2 = cx2 = _inputstr_case(*sp, &cx, cx2,
					len, plen, linemax, 1);
				break;
#endif	/* FD >= 2 */
#ifndef	_NOCOMPLETE
			case '\t':
				keyflush();
				if (h < 0 || selectlist) {
					ringbell();
					break;
				}
				i = completestr(sp, cx, len,
					plen, &size, linemax,
					comline, (ch2 == ch) ? 1 : 0);
				if (i <= 0) {
					if (!i) ringbell();
					break;
				}
				cx += i;
				len += i;
				cx2 = vlen(*sp, cx);
				if (iseol(cx2, plen, linemax))
					ocx2 = -1;
				break;
#endif	/* !_NOCOMPLETE */
			case K_CR:
				keyflush();
#ifndef	_NOCOMPLETE
				if (h < 0 || !selectlist) break;
				i = completestr(sp, cx, len,
					plen, &size, linemax, 0, -1);
				cx += i;
				len += i;
				cx2 = vlen(*sp, cx);
				if (iseol(cx2, plen, linemax))
					ocx2 = -1;
				ch = '\0';
#endif	/* !_NOCOMPLETE */
				break;
			case K_ESC:
				keyflush();
				break;
			default:
				ocx2 = cx2 = _inputstr_input(sp, &cx, cx2,
					&len, plen, &size, linemax, ch);
				break;
		}
#ifndef	_NOCOMPLETE
		if (h >= 0 && selectlist && ch != '\t'
		&& ch != K_RIGHT && ch != K_LEFT
		&& ch != K_UP && ch != K_DOWN) {
			selectfile(-1, NULL);
			rewritefile(0);
			ocx2 = -1;
		}
		if (ocx2 != cx2) setcursor(*sp, cx, cx2, len, plen, linemax);
#endif	/* !_NOCOMPLETE */
		if (ch == K_ESC) {
#ifndef	_NOORIGSHELL
			if (shellmode);
			else
#endif
			break;
		}
	} while (ch != K_CR);

#ifndef	_NOCOMPLETE
	if (selectlist) selectfile(-1, NULL);
#endif
	setcursor(*sp, len, -1, len, plen, linemax);
	subwindow = 0;
	Xgetkey(-1, 0);
	if (tmphist) free(tmphist);

	i = 0;
#ifndef	_NOORIGSHELL
	if (shellmode && ch == cc_intr) len = 0;
	else
#endif
	if (ch == K_ESC) {
		len = 0;
		if (hideclock) i = 1;
	}
	(*sp)[len] = '\0';

	tflush();
	hideclock = 0;
	if (i) rewritefile(1);
	return(ch);
}

static int NEAR dispprompt(s, set)
char *s;
int set;
{
	static char *prompt = NULL;
	char *buf;
	int len;

	if (set) prompt = s;
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		backcursor(0);
		forwcursor(xpos);
		win_y = 0;
	}
	else if (shellmode) Xlocate(0, 0);
	else
#endif
	locate(xpos, ypos);
	if (prompt && *prompt) {
#ifndef	_NOORIGSHELL
		if (dumbmode || shellmode) len = kanjiputs(prompt);
		else
#endif
		{
			putch2(' ');
			putterm(t_standout);
			len = 1 + kanjiputs(prompt);
			putterm(end_standout);
		}
	}
	else {
#ifdef	_NOORIGSHELL
		putch2(' ');
		putterm(t_standout);
#endif
		len = evalprompt(&buf, promptstr);
		kanjiputs(buf);
		free(buf);
#ifdef	_NOORIGSHELL
		len++;
		putterm(end_standout);
#else
		if (dumbmode);
		else
#endif
		putterm(t_normal);
	}
	win_x += len;
	return(len);
}

char *inputstr(prompt, delsp, ptr, def, h)
char *prompt;
int delsp, ptr;
char *def;
int h;
{
	char *input;
	ALLOC_T size;
	int i, j, len, ch, comline, dupwin_x, dupwin_y;

	dupwin_x = win_x;
	dupwin_y = win_y;
	win_x = 0;
	xpos = LEFTMARGIN;
	dupn_line = n_line;
#ifndef	_NOORIGSHELL
	dumbmode = (dumbterm || dumbshell) ? !termmode(-1) : 0;
	if (dumbmode || shellmode) lcmdline = -1;
#endif

	if (!lcmdline) ypos = L_CMDLINE;
	else if (lcmdline > 0) ypos = lcmdline;
	else ypos = n_line - 1;

#ifndef	_NOORIGSHELL
	if (dumbmode || shellmode) win_y = 0;
	else
#endif
	{
		win_y = ypos;
		for (i = 0; i < WCMDLINE; i++) {
			if (ypos + i >= n_line) break;
			locate(0, ypos + i);
			putterm(l_clear);
		}
	}
	len = dispprompt(prompt, 1);
	tflush();

	input = c_realloc(NULL, 0, &size);
	if (!def) *input = '\0';
	else {
		j = 0;
		ch = '\0';
		if (!prompt) {
			if (def[i = 0] != '~') for (; def[i]; i++) {
				if (strchr(METACHAR, def[i])) break;
				if (iskanji1(def, i)) i++;
			}
			if (def[i]) {
				input[j++] = ch = '"';
				if (ptr) ptr++;
			}
		}
		for (i = 0; def[i]; i++, j++) {
			input = c_realloc(input, j + 2, &size);
#ifdef	CODEEUC
			if (isekana(def, i)) {
				input[j++] = def[i++];
				if (ptr >= j) ptr++;
			}
			else
#endif
			if (iskanji1(def, i)) input[j++] = def[i++];
			else if (!prompt && strchr(DQ_METACHAR, def[i])) {
#ifdef	FAKEMETA
				input[j++] = def[i];
#else
				input[j++] = PMETA;
#endif
				if (ptr >= j) ptr++;
			}
			input[j] = def[i];
		}
		if (ch) {
			input[j++] = ch;
			if (ptr >= j) ptr++;
		}
		input[j] = '\0';
	}

#ifndef	_NOSPLITWIN
	if (h == 1 && windows > 1) {
		if ((i = win - 1) < 0) i = windows - 1;
		entryhist(1, winvar[i].v_fullpath, 1);
	}
#endif

	comline = (h == 0) ? 1 : 0;
#ifndef	_NOORIGSHELL
	if (promptstr == promptstr2) comline = 0;
	lastofs2 = 0;
#endif
	ch = _inputstr(&input, len, size,
		n_column - 1 - xpos, ptr, comline, h);
	win_x = dupwin_x;
	win_y = dupwin_y;

#ifndef	_NOORIGSHELL
	if (dumbmode || shellmode) prompt = NULL;
	dumbmode = 0;
#endif
	if (!prompt || !*prompt) {
		if (ch != K_ESC) {
			if (ch == cc_intr) {
				cputs2("^C");
				ch = K_ESC;
			}
			cputnl();
		}
	}
	else {
		for (i = 0; i < WCMDLINE; i++) {
			if (ypos + i >= n_line) break;
			locate(0, ypos + i);
			putterm(l_clear);
		}
		locate(win_x, win_y);
		tflush();
		if ((!lcmdline && ypos < L_CMDLINE)
		|| (lcmdline > 0 && ypos < lcmdline + n_line)) rewritefile(1);
	}
	lcmdline = 0;

	if (ch == K_ESC) {
		free(input);
		return(NULL);
	}

	len = strlen(input);
	if (delsp && len > 0 && input[len - 1] == ' ' && yesno(DELSP_K)) {
		for (len--; len > 0 && input[len - 1] == ' '; len--);
		input[len] = '\0';
	}
	return(input);
}

static char *NEAR truncstr(s)
char *s;
{
	int len;
	char *cp, *tmp;

	if ((len = strlen2(s) + YESNOSIZE - n_lastcolumn) <= 0
	|| !(cp = strchr2(s, '[')) || !(tmp = strchr2(cp, ']'))) return(s);

	cp++;
	len = tmp - cp - len;
	if (len <= 0) len = 0;
	else if (onkanji1(cp, len - 1)) len--;
#ifdef	CODEEUC
	else if (isekana(cp, len - 1)) len--;
#endif
	strcpy(&(cp[len]), tmp);
	return(s);
}

static int NEAR yesnomes(mes)
char *mes;
{
	int len;

#ifndef	_NOORIGSHELL
	if (dumbmode) forwline(xpos);
	else if (shellmode) Xlocate(0, 0);
	else
#endif
	locate(xpos, ypos);
#ifndef	_NOORIGSHELL
	if (dumbmode);
	else
#endif
	{
		putterm(l_clear);
		putterm(t_standout);
	}
	len = kanjiputs2(mes, n_lastcolumn - YESNOSIZE, -1);
	win_x += len;
	cputs2(YESNOSTR);
	win_x += YESNOSIZE;
#ifndef	_NOORIGSHELL
	if (dumbmode);
	else
#endif
	putterm(end_standout);
	tflush();
	return(len);
}

#ifdef	USESTDARGH
/*VARARGS1*/
int yesno(CONST char *fmt, ...)
#else
/*VARARGS1*/
int yesno(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	int len, ch, x, dupwin_x, dupwin_y, duperrno, ret;
	char *buf;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif
	len = vasprintf2(&buf, fmt, args);
	va_end(args);
	if (len < 0) error("malloc()");

	dupwin_x = win_x;
	dupwin_y = win_y;
	duperrno = errno;
	win_x = xpos = 0;
#ifndef	_NOORIGSHELL
	dumbmode = (dumbterm || dumbshell) ? !termmode(-1) : 0;
	if (dumbmode || shellmode) lcmdline = -1;
#endif

	if (!lcmdline) ypos = L_MESLINE;
	else if (lcmdline > 0) ypos = lcmdline;
	else ypos = n_line - 1;

#ifndef	_NOORIGSHELL
	if (dumbmode || shellmode) win_y = 0;
	else
#endif
	win_y = ypos;

	truncstr(buf);
	len = strlen2(buf);
	ret = yesnomes(buf);
	if (ret < len) len = ret;
	if (win_x >= n_column) win_x = n_column - 1;

	subwindow = 1;
	Xgetkey(-1, 0);

	ret = 1;
	x = len + 1;
	do {
		keyflush();
#ifndef	_NOORIGSHELL
		if (dumbmode) {
			if (xpos + x < win_x) backcursor(xpos + x);
			else while (xpos + x > win_x) {
				putch2(YESNOSTR[win_x - xpos - len]);
				win_x++;
			}
		}
		else
#endif
		Xlocate(x, 0);
		tflush();
		switch (ch = Xgetkey(1, 0)) {
			case 'y':
			case 'Y':
				ret = 1;
				ch = K_CR;
				break;
			case 'n':
			case 'N':
			case ' ':
			case K_ESC:
				ret = 0;
				ch = K_CR;
				break;
			case K_RIGHT:
				ret = 0;
				break;
			case K_LEFT:
				ret = 1;
				break;
			case K_CTRL('L'):
				if (!lcmdline) ypos = L_MESLINE;
				else if (lcmdline > 0) ypos = lcmdline;
				else ypos = n_line - 1;
				yesnomes(buf);
				break;
			default:
				break;
		}
		x = len + 1 + (1 - ret) * 2;
	} while (ch != K_CR);
	free(buf);

#ifndef	_NOORIGSHELL
	if (dumbmode) cputnl();
	else
#endif
	if (lcmdline) {
		Xlocate(x, 0);
		putch2((ret) ? 'Y' : 'N');
		cputnl();
	}
	else {
		Xlocate(0, 0);
		putterm(l_clear);
	}
#ifndef	_NOORIGSHELL
	dumbmode = 0;
#endif
	tflush();
	lcmdline = 0;

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;
	Xgetkey(-1, 0);

	errno = duperrno;
	return(ret);
}

VOID warning(no, s)
int no;
char *s;
{
	char *tmp, *err;
	int len, wastty, dupwin_x, dupwin_y;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;

	err = strerror2((no < 0) ? errno : no);
	tmp = NULL;
	if (!s) s = err;
	else if (no) {
		len = n_lastcolumn - strlen2(err) - 3;
		tmp = malloc2(n_lastcolumn * KANAWID + 1);
		strncpy3(tmp, s, &len, -1);
		strcat(tmp, ": ");
		strcat(tmp, err);
		s = tmp;
	}
	ringbell();

	locate(0, L_MESLINE);
	putterm(l_clear);
	putterm(t_standout);
	win_x = kanjiputs2(s, n_lastcolumn, -1);
	win_y = L_MESLINE;
	putterm(end_standout);
	tflush();

	if (win_x >= n_lastcolumn) win_x = n_lastcolumn - 1;

	if (!(wastty = isttyiomode)) ttyiomode(1);
	keyflush();
	do {
#ifdef	SIGALRM
		getkey2((noalrm) ? 0 : SIGALRM);
#else
		getkey2(0);
#endif
	} while (kbhit2(WAITAFTERWARN * 1000L));
	if (!wastty) stdiomode();

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;

	if (no) rewritefile(1);
	else {
		locate(0, L_MESLINE);
		putterm(l_clear);
		locate(win_x, win_y);
		tflush();
	}

	if (tmp) free(tmp);
	hideclock = 0;
}

static int NEAR selectcnt(max, str, multi)
int max;
char **str;
int multi;
{
	int i, len;

	for (i = len = 0; i < max; i++) if (str[i]) {
		len += strlen2(str[i]) + 1;
		if (multi) len++;
	}
	return(len);
}

static int NEAR selectadj(max, x, str, tmpstr, xx, multi)
int max, x;
char **str, **tmpstr;
int xx[], multi;
{
	char *cp;
	int i, len, maxlen;

	for (i = 0; i < max; i++) {
		if (tmpstr[i]) free(tmpstr[i]);
		tmpstr[i] = strdup2(str[i]);
	}
	len = selectcnt(max, tmpstr, multi);

	if (x + len < n_lastcolumn) /*EMPTY*/;
	else if ((x = n_lastcolumn - 1 - len) >= 0) /*EMPTY*/;
	else {
		x = maxlen = 0;
		str = (char **)malloc2(max * sizeof(char *));
		for (i = 0; i < max; i++) {
			if (!(cp = tmpstr[i])) {
				str[i] = NULL;
				continue;
			}
			if (isupper2(*cp) && cp[1] == ':')
				for (cp += 2; *cp == ' '; cp++);
			len = strlen3(cp);
			if (len > maxlen) maxlen = len;
			str[i] = strdup2(cp);
		}

		for (; maxlen > 0; maxlen--) {
			for (i = 0; i < max; i++) if (str[i]) {
				len = maxlen;
				strncpy3(tmpstr[i], str[i], &len, -1);
			}
			if (x + selectcnt(max, tmpstr, multi) < n_lastcolumn)
				break;
		}
		for (i = 0; i < max; i++) if (str[i]) free(str[i]);
		free(str);
		if (maxlen <= 0) return(-1);
	}

	xx[0] = 0;
	for (i = 0; i < max; i++) {
		if (!tmpstr[i]) {
			xx[i + 1] = xx[i];
			continue;
		}
		xx[i + 1] = xx[i] + strlen2(tmpstr[i]) + 1;
		if (multi) (xx[i + 1])++;
	}

	return(x);
}

static int NEAR selectmes(num, max, x, str, val, xx)
int num, max, x;
char *str[];
int val[], xx[];
{
	int i, new;

	locate(x, L_MESLINE);
	putterm(l_clear);
	new = (num < 0) ? -1 - num : -1;
	for (i = 0; i < max; i++) {
		if (!str[i]) continue;
		locate(x + xx[i] + 1, L_MESLINE);
		if (num < 0) putch2((val[i]) ? '*' : ' ');
		if (i != new && val[i] != num) kanjiputs(str[i]);
		else {
			new = i;
			putterm(t_standout);
			kanjiputs(str[i]);
			putterm(end_standout);
		}
	}
	return(new);
}

int selectstr(num, max, x, str, val)
int *num, max, x;
char *str[];
int val[];
{
	char **tmpstr;
	int i, ch, old, new, tmpx, dupwin_x, dupwin_y, *xx, *initial;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	Xgetkey(-1, 0);

	xx = (int *)malloc2((max + 1) * sizeof(int));
	initial = (int *)malloc2(max * sizeof(int));
	tmpstr = (char **)malloc2(max * sizeof(char *));

	for (i = 0; i < max; i++) {
		tmpstr[i] = NULL;
		initial[i] = (str[i] && isupper2(*(str[i]))) ? *str[i] : -1;
	}
	tmpx = selectadj(max, x, str, tmpstr, xx, (num) ? 0 : 1);

	new = (num) ? *num : -1;
	if ((new = selectmes(new, max, tmpx, tmpstr, val, xx)) < 0) new = 0;

	win_y = L_MESLINE;
	do {
		win_x = tmpx + xx[new + 1];
		locate(win_x, win_y);
		tflush();
		old = new;

		keyflush();
		switch (ch = Xgetkey(1, 0)) {
			case K_RIGHT:
				for (new++; new != old; new++) {
					if (new >= max) new = 0;
					if (tmpstr[new]) break;
				}
				break;
			case K_LEFT:
				for (new--; new != old; new--) {
					if (new < 0) new = max - 1;
					if (tmpstr[new]) break;
				}
				break;
			case K_CTRL('L'):
				win_y = L_MESLINE;
				tmpx = selectadj(max, x,
					str, tmpstr, xx, (num) ? 0 : 1);
				if (num) selectmes(val[new], max, tmpx,
					tmpstr, val, xx);
				else selectmes(-1 - new, max, tmpx,
					tmpstr, val, xx);
				break;
			case ' ':
				if (num) break;
				val[new] = (val[new]) ? 0 : 1;
				locate(tmpx + xx[new] + 1, L_MESLINE);
				putch2((val[new]) ? '*' : ' ');
				break;
			default:
				if (!isalpha2(ch)) break;
				ch = toupper2(ch);
				for (i = 0; i < max; i++)
					if (ch == initial[i]) break;
				if (i >= max) break;
				new = i;
				if (num) {
					ch = K_CR;
					break;
				}
				val[new] = (val[new]) ? 0 : 1;
				locate(tmpx + xx[new] + 1, L_MESLINE);
				putch2((val[new]) ? '*' : ' ');
				break;
		}
		if (new != old) {
			i = tmpx + 1;
			if (!num) i++;
			locate(i + xx[new], L_MESLINE);
			putterm(t_standout);
			kanjiputs(tmpstr[new]);
			putterm(end_standout);
			locate(i + xx[old], L_MESLINE);
			if (stable_standout) putterm(end_standout);
			else kanjiputs(tmpstr[old]);
		}
	} while (ch != K_ESC && ch != K_CR);

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;
	Xgetkey(-1, 0);

	if (stable_standout) {
		locate(tmpx + 1, L_MESLINE);
		putterm(l_clear);
	}
	if (num) {
		if (ch == K_ESC) new = -1;
		else *num = val[new];
		for (i = 0; i < max; i++) {
			if (!tmpstr[i]) continue;
			locate(tmpx + xx[i] + 1, L_MESLINE);
			if (i == new) kanjiputs(tmpstr[i]);
			else cprintf2("%*s", strlen2(tmpstr[i]), "");
		}
	}
	else {
		if (ch == K_ESC) for (i = 0; i < max; i++) val[i] = 0;
		else {
			for (i = 0; i < max; i++) if (val[i]) break;
			if (i >= max) ch = K_ESC;
		}
		for (i = 0; i < max; i++) {
			if (!tmpstr[i]) continue;
			locate(tmpx + xx[i] + 1, L_MESLINE);
			putch2(' ');
			if (val[i]) kanjiputs(tmpstr[i]);
			else cprintf2("%*s", strlen2(tmpstr[i]), "");
		}
	}
	locate(win_x, win_y);
	tflush();
	free(xx);
	free(initial);
	for (i = 0; i < max; i++) if (tmpstr[i]) free(tmpstr[i]);
	free(tmpstr);
	return(ch);
}
