/*
 *	input.c
 *
 *	input module
 */

#include "fd.h"
#include "func.h"
#include "kanji.h"

#ifdef	_NOORIGSHELL
#include <signal.h>
#else
#include "system.h"
#endif

extern char **history[];
extern short histsize[];
extern int curcolumns;
extern int minfilename;
extern int hideclock;
#ifdef	SIGALRM
extern int noalrm;
#endif
extern char *promptstr;
#ifndef	_NOORIGSHELL
extern int fdmode;
extern char *promptstr2;
#endif
#ifndef	_NOPTY
extern int ptymode;
extern int parentfd;
#endif

#define	LIMITSELECTWARN	100
#define	YESNOSTR	"[Y/N]"
#define	YESNOSIZE	((int)sizeof(YESNOSTR) - 1)
#define	WAITAFTERWARN	360	/* msec */
#define	maxscr()	(maxcol * (maxline - minline) \
			- plen - (n_column - n_lastcolumn))
#define	within(n)	((n) < maxscr())
#define	overflow(n)	((n) > maxscr())
#define	ptr2col(n)	(((n) + plen) % maxcol)
#define	ptr2line(n)	(((n) + plen) / maxcol)
#define	iseol(n)	(within(n) && !(ptr2col(n)))
#define	LEFTMARGIN	0
#define	RIGHTMARGIN	2
#ifdef	SIGALRM
#define	sigalrm(sig)	((!noalrm && (sig)) ? SIGALRM : 0)
#else
#define	sigalrm(sig)	0
#endif

#ifndef	_NOEDITMODE
static int NEAR getemulatekey __P_((int, CONST char []));
#endif
static char *NEAR trquote __P_((char *, int, int *));
static int NEAR vlen __P_((char *, int));
static int NEAR rlen __P_((char *, int));
static int NEAR vonkanji1 __P_((char *, int));
#if	FD >= 2
static VOID NEAR kanjiputs3 __P_((char *, int, int, int, int));
#else
#define	kanjiputs3(s,n,l,p,m)	VOID_C kanjiputs2(s, l, p)
#endif
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
static VOID NEAR rewritecursor __P_((char *, int, int, int));
static VOID NEAR checkcursor __P_((char *, int, int, int));
#endif
static VOID NEAR scrollup __P_((VOID_A));
static VOID NEAR locate2 __P_((int, int));
static VOID NEAR setcursor __P_((char *, int, int, int));
static int NEAR putstr __P_((char *, char *, int, int, int));
static VOID NEAR ringbell __P_((VOID_A));
static VOID NEAR clearline __P_((VOID_A));
static VOID NEAR newline __P_((int));
static int NEAR rightchar __P_((char *, int *, int, int));
static int NEAR leftchar __P_((char *, int *, int, int));
static VOID NEAR insertchar __P_((char *, int, int, int));
static VOID NEAR deletechar __P_((char *, int, int, int));
static VOID NEAR insshift __P_((char *, int, int, int));
static VOID NEAR delshift __P_((char *, int, int, int));
static VOID NEAR truncline __P_((char *, int, int, int));
static VOID NEAR displaystr __P_((char *, int *, int *));
static int NEAR preparestr __P_((char **, int, int *, ALLOC_T *, int, int));
static int NEAR insertstr __P_((char **, int, int,
		ALLOC_T *, char *, int, int));
#ifndef	_NOCOMPLETE
static VOID NEAR selectfile __P_((int, char **));
static int NEAR completestr __P_((char **, int, int, ALLOC_T *,
		int, int, int));
#endif
static int NEAR getkanjikey __P_((char *, int));
static int NEAR copyhist __P_((char **, int *, int *, ALLOC_T *, char *));
static int NEAR _inputstr_up __P_((char **, int *, int, int *,
		ALLOC_T *, int *, int, char **));
static int NEAR _inputstr_down __P_((char **, int *, int, int *,
		ALLOC_T *, int *, int, char **));
static int NEAR _inputstr_delete __P_((char *, int, int));
static int NEAR _inputstr_enter __P_((char **, int *, int, int *, ALLOC_T *));
#if	FD >= 2
static int NEAR _inputstr_case __P_((char *, int *, int, int, int));
static int NEAR search_matchlen __P_((char *, int));
static char *NEAR search_up __P_((char *, int *, int, int,
		int *, int, char **));
static char *NEAR search_down __P_((char *, int *, int, int,
		int *, int, char **));
#endif
static int NEAR _inputstr_input __P_((char **, int *, int, int *,
		ALLOC_T *, char *, int));
static int NEAR _inputstr __P_((char **, ALLOC_T, int, int, int));
static VOID NEAR dispprompt __P_((char *, int));
static char *NEAR truncstr __P_((char *));
static int NEAR yesnomes __P_((char *));
static int NEAR selectcnt __P_((int, char **, int));
static int NEAR selectadj __P_((int, int, char **, char **, int [], int));
static VOID NEAR selectmes __P_((int, int, int,
		char *[], int [], int [], int));

int subwindow = 0;
int win_x = 0;
int win_y = 0;
char *curfilename = NULL;
#ifndef	_NOEDITMODE
char *editmode = NULL;
#endif
int lcmdline = 0;
int maxcmdline = 0;
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
#define	EMULATEKEYSIZ	((int)(sizeof(emulatekey) / sizeof(int)))
static CONST char emacskey[] = {
	K_CTRL('P'), K_CTRL('N'), K_CTRL('F'), K_CTRL('B'),
	K_ESC, K_CTRL('D'), K_CTRL('Q'), K_CTRL('K'),
	K_ESC, K_ESC, K_CTRL('A'), K_CTRL('E'),
	K_CTRL('V'), K_CTRL('Y'), K_CTRL('O'), K_CTRL('G')
};
static CONST char vikey[] = {
	'k', 'j', 'l', 'h',
	K_ESC, 'x', K_ESC, 'D',
	'g', 'G', '0', '$',
	K_CTRL('B'), K_CTRL('F'), 'o', K_ESC
};
static CONST char wordstarkey[] = {
	K_CTRL('E'), K_CTRL('X'), K_CTRL('D'), K_CTRL('S'),
	K_CTRL('V'), K_CTRL('G'), K_CTRL(']'), K_CTRL('Y'),
	K_CTRL('W'), K_CTRL('Z'), K_CTRL('A'), K_CTRL('F'),
	K_CTRL('R'), K_CTRL('C'), K_CTRL('N'), K_ESC
};
static int vistat = 0;
#define	VI_NEXT		001
#define	VI_INSERT	002
#define	VI_ONCE		004
#define	VI_VIMODE	010
#define	isvimode()	((vistat & (VI_VIMODE | VI_INSERT)) == VI_VIMODE)
#endif	/* !_NOEDITMODE */

static int plen = 0;
static int maxcol = 0;
static int minline = 0;
static int maxline = 0;
#ifndef	_NOCOMPLETE
static namelist *selectlist = NULL;
static int maxselect = 0;
static int tmpfileperrow = -1;
static int tmpfilepos = -1;
static int tmpcolumns = -1;
#endif
static int xpos = 0;
static int ypos = 0;
static int overwritemode = 0;
#ifndef	_NOORIGSHELL
static int dumbmode = 0;
static int lastofs2 = 0;
#endif
#if	FD >= 2
static int searchmode = 0;
static char *searchstr = NULL;
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

#ifndef	_NOEDITMODE
static int NEAR getemulatekey(ch, table)
int ch;
CONST char table[];
{
	int i;

	for (i = 0; i < EMULATEKEYSIZ; i++) {
		if (table[i] == K_ESC) continue;
		if (ch == table[i]) return(emulatekey[i]);
	}

	return(ch);
}
#endif

int Xgetkey(sig, eof)
int sig, eof;
{
	static int prev = -1;
	int ch;

	if (sig < 0) {
#ifndef	_NOEDITMODE
		vistat = eof;
#endif
		prev = -1;
		return('\0');
	}

	sig = sigalrm(sig);
	ch = getkey3(sig);
	if (eof && (ch != cc_eof || prev == ch)) eof = 0;
	prev = ch;
	if (eof) return(-1);

#ifndef	_NOEDITMODE
	if (!editmode) /*EMPTY*/;
	else if (!strcmp(editmode, "emacs"))
		ch = getemulatekey(ch, emacskey);
	else if (!strcmp(editmode, "vi")) do {
		vistat |= VI_VIMODE;
		vistat &= ~VI_NEXT;
		if (vistat & VI_INSERT) switch (ch) {
			case K_CTRL('V'):
				if (vistat & VI_ONCE)
					vistat &= ~(VI_INSERT | VI_ONCE);
				ch = K_IL;
				break;
			case K_ESC:
				if (vistat & VI_ONCE) vistat |= VI_NEXT;
				vistat &= ~(VI_INSERT | VI_ONCE);
				ch = K_LEFT;
				break;
			case K_CR:
				vistat &= ~(VI_INSERT | VI_ONCE);
				break;
			case K_BS:
				if (vistat & VI_ONCE)
					vistat &= ~(VI_INSERT | VI_ONCE);
				break;
			default:
				if (vistat & VI_ONCE)
					vistat &= ~(VI_INSERT | VI_ONCE);
				if (ch < K_MIN) break;
				ringbell();
				vistat |= VI_NEXT;
				break;
		}
		else if ((ch = getemulatekey(ch, vikey)) < K_MIN) switch (ch) {
			case ':':
				vistat |= (VI_NEXT | VI_INSERT);
				overwritemode = 0;
				break;
			case 'I':
				vistat |= VI_INSERT;
				overwritemode = 0;
				ch = K_BEG;
				break;
			case 'A':
				vistat |= VI_INSERT;
				overwritemode = 0;
				ch = K_EOL;
				break;
			case 'i':
				vistat |= (VI_NEXT | VI_INSERT);
				overwritemode = 0;
				break;
			case 'a':
				vistat |= VI_INSERT;
				overwritemode = 0;
				ch = K_RIGHT;
				break;
# if	FD >= 2
			case 'R':
				vistat |= (VI_NEXT | VI_INSERT);
				overwritemode = 1;
				break;
			case 'r':
				vistat |= (VI_NEXT | VI_INSERT | VI_ONCE);
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
				ringbell();
				vistat |= VI_NEXT;
				break;
		}
	} while ((vistat & VI_NEXT) && (ch = getkey3(sig)));
	else if (!strcmp(editmode, "wordstar"))
		ch = getemulatekey(ch, wordstarkey);
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
	Xcputs2(buf);
	n = strlen2(buf);
#else
	n = Xkanjiputs(buf);
#endif
	free(buf);

	return(n);
}

#if	FD >= 2
static VOID NEAR kanjiputs3(s, cx2, len2, ptr, top)
char *s;
int cx2, len2, ptr, top;
{
	int n, width;

	if (!searchmode || ptr + len2 <= top || ptr >= cx2) {
		kanjiputs2(s, len2, ptr);
		return;
	}

	width = cx2 - top;
	n = top - ptr;
	if (n <= 0) width += n;
	else {
		kanjiputs2(s, n, ptr);
		ptr += n;
		len2 -= n;
	}
	Xputterm(T_STANDOUT);
	kanjiputs2(s, width, ptr);
	ptr += width;
	len2 -= width;
	Xputterm(END_STANDOUT);
	if (len2 > 0) kanjiputs2(s, len2, ptr);
}
#endif	/* FD >= 2 */

static VOID NEAR putcursor(c, n)
int c, n;
{
	win_x += n;
	while (n--) Xputch2(c);
#ifndef	_NOORIGSHELL
	if (dumbmode);
	else if (shellmode && win_x >= n_column) {
		Xputch2(' ');
		Xputch2('\r');
		win_x = 0;
		win_y++;
	}
#endif
}

static VOID NEAR rightcursor(VOID_A)
{
	win_x++;
	Xputterm(C_RIGHT);
}

static VOID NEAR leftcursor(VOID_A)
{
	win_x--;
	Xputterm(C_LEFT);
}

static VOID NEAR upcursor(VOID_A)
{
	win_y--;
	Xputterm(C_UP);
}

static VOID NEAR downcursor(VOID_A)
{
	win_y++;
	Xputterm(C_DOWN);
}

#ifndef	_NOORIGSHELL
static VOID NEAR forwcursor(x)
int x;
{
	while (win_x < x) {
		Xputch2(' ');
		win_x++;
	}
}

static VOID NEAR backcursor(x)
int x;
{
	while (win_x > x) {
		Xputch2(C_BS);
		win_x--;
	}
}

static VOID NEAR forwline(x)
int x;
{
	if (win_x < n_column) {
		Xcputnl();
		win_y++;
	}
	else {
		while (win_x >= n_column) {
			win_x -= n_column;
			win_y++;
		}
		Xputch2(' ');
		Xputch2('\r');
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

static VOID NEAR rewritecursor(s, cx, cx2, len)
char *s;
int cx, cx2, len;
{
	char *dupl;
	int i, dx, ocx, ocx2;

	i = (lastofs2) ? 1 : plen;
	dx = cx2 - lastofs2;
	ocx2 = win_x - xpos - i + lastofs2;
	if (dx < 0 || i + dx >= maxcol) displaystr(s, &cx, &len);
	else if (cx2 <= ocx2) backcursor(xpos + i + dx);
	else {
		ocx = rlen(s, ocx2);
		dupl = trquote(&(s[ocx]), cx - ocx, NULL);
		win_x += Xkanjiputs(dupl);
		free(dupl);
	}
}

static VOID NEAR checkcursor(s, cx, cx2, len)
char *s;
int cx, cx2, len;
{
	int i, dx;

	i = (lastofs2) ? 1 : plen;
	dx = cx2 - lastofs2;
	if (i + dx >= maxcol) displaystr(s, &cx, &len);
}
#endif	/* !_NOORIGSHELL */

static VOID NEAR scrollup(VOID_A)
{
#ifdef	_NOPTY
	Xlocate(0, maxline - 1);
	Xputterms(C_SCROLLFORW);
#else
	regionscroll(C_SCROLLFORW, 1, 0, maxline - 1, minline, maxline - 1);
#endif
	ypos--;
	hideclock = 1;
}

static VOID NEAR locate2(x, y)
int x, y;
{
#ifndef	_NOORIGSHELL
	if (shellmode) {
		x += xpos;
		if (win_x >= n_column) {
			Xmovecursor(C_NLEFT, C_LEFT, win_x);
			win_x = 0;
		}
		if (y < win_y) Xmovecursor(C_NUP, C_UP, win_y - y);
		else if (y > win_y) {
			Xmovecursor(-1, C_SCROLLFORW, y - win_y);
			Xputch2('\r');
			win_x = 0;
		}
		if (x < win_x) Xmovecursor(C_NLEFT, C_LEFT, win_x - x);
		else if (x > win_x) Xmovecursor(C_NRIGHT, C_RIGHT, x - win_x);

		win_x = x;
		win_y = y;
		return;
	}
#endif	/* !_NOORIGSHELL */
	while (ypos + y >= maxline) scrollup();
	win_x = xpos + x;
	win_y = ypos + y;
	Xlocate(win_x, win_y);
}

/*ARGSUSED*/
static VOID NEAR setcursor(s, cx, cx2, len)
char *s;
int cx, cx2, len;
{
	int f;

	if (cx < 0) cx = rlen(s, cx2);
	if (cx2 < 0) cx2 = vlen(s, cx);
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		rewritecursor(s, cx, cx2, len);
		return;
	}
#endif
	f = within(cx2) ? 0 : 1;
	cx2 -= f;
	locate2(ptr2col(cx2), ptr2line(cx2));
	if (f) rightcursor();
}

static int NEAR putstr(cp, s, cx, cx2, len)
char *cp, *s;
int cx, cx2, len;
{
	while (*cp) {
		cx2++;
		putcursor(*cp, 1);
		if (iseol(cx2)) setcursor(s, cx, cx2, len);
		cp++;
	}

	return(cx2);
}

static VOID NEAR ringbell(VOID_A)
{
#ifndef	_NOORIGSHELL
	if (dumbmode && dumbterm <= 2) Xputch2('\007');
	else
#endif
	Xputterm(T_BELL);
	Xtflush();
}

static VOID NEAR clearline(VOID_A)
{
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		int x;

		/* abandon last 1 char because of auto newline */
		for (x = win_x; x < n_column - 1; x++) Xputch2(' ');
		for (x = win_x; x < n_column - 1; x++) Xputch2(C_BS);
	}
	else
#endif
	Xputterm(L_CLEAR);
}

static VOID NEAR newline(y)
int y;
{
#ifndef	_NOORIGSHELL
	if (dumbmode);
	else if (shellmode) forwline(xpos);
	else
#endif
	locate2(0, y);
}

static int NEAR rightchar(s, cxp, cx2, len)
char *s;
int *cxp, cx2, len;
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
	if (dumbmode) rewritecursor(s, *cxp, cx2, len);
	else
#endif
	if (within(cx2) && ptr2col(cx2) < vw) setcursor(s, *cxp, cx2, len);
	else while (vw--) rightcursor();

	return(cx2);
}

/*ARGSUSED*/
static int NEAR leftchar(s, cxp, cx2, len)
char *s;
int *cxp, cx2, len;
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
	if (dumbmode) rewritecursor(s, *cxp, cx2, len);
	else
#endif
	if (ptr2col(ocx2) < vw) setcursor(s, *cxp, cx2, len);
	else while (vw--) leftcursor();

	return(cx2);
}

static VOID NEAR insertchar(s, cx, len, ins)
char *s;
int cx, len, ins;
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
	dy = ptr2line(cx2);
	i = (dy + 1) * maxcol - plen;	/* prev. chars including cursor line */
	j = maxcol - ptr2col(cx2);
	if (j > ins) j = ins;		/* inserted columns in cursor line */

#ifndef	_NOORIGSHELL
	if (dumbmode) {
		i = (lastofs2) ? 1 : plen;
		dx = cx2 + ins - lastofs2;
		if (i + dx < maxcol) {
			putcursor(' ', ins);
			dumbputs(dupl, i + cx2, l, maxcol - (i + dx), 0);
		}
	}
	else
#endif	/* !_NOORIGSHELL */
#if	!MSDOS
	if (*termstr[C_INSERT]) {
# ifndef	_NOORIGSHELL
		if (shellmode);
		else
# endif
		if (ypos + dy >= maxline - 1
		&& xpos + ptr2col(cx2) + (len2 - cx2) + j >= n_column) {
			/*
			 * In the case that current line is the last line,
			 * and the end of string will reach over the last line.
			 */
			while (ypos + dy >= maxline - 1) scrollup();
			setcursor(s, cx, cx2, len);
		}
		while (j--) Xputterm(C_INSERT);

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
				if (xpos + maxcol < n_column) {
					locate2(maxcol, dy);
					j = n_column - (xpos + maxcol);
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
				if (ypos + dy >= maxline - 1
				&& xpos + nlen - i >= n_column)
					while (ypos + dy >= maxline - 1)
						scrollup();
				locate2(0, dy);
				for (j = 0; j < ins; j++) Xputterm(C_INSERT);
				l = ins;
				if (ptr < ins) {
					ptr++;
					locate2(ins - ptr, dy);
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
				i += maxcol;
			}
			setcursor(s, cx, cx2, len);
		}
	}
	else
#endif	/* !MSDOS */
	{
		putcursor(' ', j);
		j = 0;
		l = i - cx2 - ins;	/* rest chars following inserted str */
		dx = ptr2col(cx2 + ins);
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
			if (ypos + dy >= maxline - 1) {
				while (ypos + dy >= maxline - 1) scrollup();
				locate2(dx, dy);
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
				locate2(0, dy);
			}

			l = maxcol;
			if (ptr < ins - 1) {
				putcursor(' ', 1);
				ptr++;
				locate2(ins - ptr, dy);
				l -= ins - ptr;
				ptr = 0;
			}
			else {
				ptr -= ins - 1;
				if (f1) dupl[f1 - 1] = dupl[f1] = ' ';
			}
			if (ptr + l > len2 - cx2) l = len2 - cx2 - ptr;
			j = ptr;
			i += maxcol;
			dx = 0;
		}

		l = len2 - cx2 - j;
		if (l > 0) {
			kanjiputs2(dupl, l, j);
			win_x += l;
		}
		setcursor(s, cx, cx2, len);
	}
#ifndef	_NOORIGSHELL
	if (dumbmode);
	else if (shellmode) {
		if (ptr2col(nlen) == 1) {
			setcursor(s, -1, nlen, len);
			Xputterm(L_CLEAR);
			setcursor(s, cx, cx2, len);
		}
	}
	else
#endif
	if (i == nlen && within(i) && ypos + dy >= maxline - 1) {
		while (ypos + dy >= maxline - 1) scrollup();
		setcursor(s, cx, cx2, len);
	}
	free(dupl);
}

static VOID NEAR deletechar(s, cx, len, del)
char *s;
int cx, len, del;
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
	dy = ptr2line(cx2);
	i = (dy + 1) * maxcol - plen;	/* prev. chars including cursor line */

#ifndef	_NOORIGSHELL
	if (dumbmode) {
		i = (lastofs2) ? 1 : plen;
		j = cx2 - lastofs2;
		dumbputs(dupl, i + cx2, l, maxcol - (i + j), del);
	}
	else
#endif	/* !_NOORIGSHELL */
#if	!MSDOS
	if (*termstr[C_DELETE]) {
		j = maxcol - ptr2col(cx2);
		if (j > del) j = del;	/* deleted columns in cursor line */
		while (j--) Xputterm(C_DELETE);

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
				j = maxcol - del + f2;
				if (j < maxcol - (++ptr)) {
					j = maxcol - ptr;
					l = ptr + f1;
					ptr = del;
				}
				else {
					l = del - f2 + f1;
					ptr += f2;
				}
				locate2(j, dy);
				if (ptr + l > len2) l = len2 - ptr;
				if (l > 0) {
					kanjiputs2(dupl, l, ptr);
					win_x += l;
				}
				if (!f1) putcursor(' ', 1);
				locate2(0, ++dy);
				for (j = 0; j < del; j++) Xputterm(C_DELETE);
				if (f1) putcursor(' ', 1);
				i += maxcol;
			}
			setcursor(s, cx, cx2, len);
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
				locate2(0, dy);
			}

			l = maxcol;
			ptr += del + 1;
			if (f1) dupl[f1 - 1] = dupl[f1] = ' ';
			if (ptr + l > len2 - cx2) l = len2 - cx2 - ptr;
			j = ptr;
			i += maxcol;
		}

		l = len2 - cx2 - j;
		if (l > 0) {
			kanjiputs2(dupl, l, j);
			win_x += l;
		}
		clearline();
		setcursor(s, cx, cx2, len);
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

static VOID NEAR truncline(s, cx, cx2, len)
char *s;
int cx, cx2, len;
{
	int dy, i, len2;

	len2 = vlen(s, len);
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		clearline();
		return;
	}
#endif
	Xputterm(L_CLEAR);

	dy = ptr2line(cx2);
	i = (dy + 1) * maxcol - plen;
	if (i < len2) {
#ifndef	_NOORIGSHELL
		if (shellmode) while (i < len2) {
			locate2(0, ++dy);
			Xputterm(L_CLEAR);
			i += maxcol;
		}
		else
#endif
		while (i < len2) {
			if (ypos + ++dy >= maxline) break;
			Xlocate(xpos, ypos + dy);
			Xputterm(L_CLEAR);
			i += maxcol;
		}
		setcursor(s, cx, cx2, len);
	}
}

static VOID NEAR displaystr(s, cxp, lenp)
char *s;
int *cxp, *lenp;
{
#if	FD >= 2
	int top;
#endif
	char *dupl;
	int i, x, y, cx2, len2, max, vi, width, f;

	dupl = trquote(s, *lenp, &len2);
	cx2 = vlen(s, *cxp);
#if	FD >= 2
	top = vlen(s, search_matchlen(s, *cxp));
#endif

#ifndef	_NOORIGSHELL
	if (dumbmode) {
		backcursor(0);
		forwcursor(xpos);
		dispprompt(NULL, -1);
		i = plen;
		x = cx2 - maxcol;
		y = (maxcol + 1) / 2;
		for (lastofs2 = 0; lastofs2 <= i + x; lastofs2 += y) {
# if	FD >= 2
			if (searchmode) i = plen + 1;
			else
# endif
			i = 1;
		}
		if (!lastofs2) dispprompt(NULL, 0);
		else {
# if	FD >= 2
			if (searchmode) dispprompt(NULL, 0);
# endif
			putcursor('<', 1);
			if ((f = vonkanji1(dupl, lastofs2 - 1)))
				dupl[f - 1] = dupl[f] = ' ';
		}
		dumbputs(dupl, i + cx2, len2, maxcol - i, lastofs2);
		Xtflush();
		free(dupl);
		return;
	}
#endif	/* !_NOORIGSHELL */

	dispprompt(NULL, 0);
	locate2(plen, 0);
	max = maxscr();
	if (len2 > max) {
		len2 = max;
		*lenp = rlen(s, max);
		if (cx2 >= len2) {
			cx2 = len2;
			*cxp = *lenp;
		}
	}
	vi = (termstr[T_NOCURSOR] && *termstr[T_NOCURSOR]
		&& termstr[T_NORMALCURSOR] && *termstr[T_NORMALCURSOR]);
	if (vi) Xputterm(T_NOCURSOR);
	i = x = y = 0;
	width = maxcol - plen;
	while (i + width < len2) {
		/*
		 * Whether if the end of line is kanji 1st byte
		 */
		f = (vonkanji1(dupl, i + width - 1)) ? 1 : 0;
		clearline();
		if (stable_standout) Xputterm(END_STANDOUT);

#ifndef	_NOORIGSHELL
		if (shellmode);
		else
#endif
		if (ypos + y >= maxline - 1) {
			while (ypos + y >= maxline - 1) scrollup();
			locate2(x, y);
		}
		if (width + f > 0) {
			kanjiputs3(dupl, cx2, width + f, i, top);
			win_x += width + f;
		}

		i += width + f;
		width = maxcol - f;
		x = f;
		y++;

		if (f) {
			newline(y);
			putcursor(' ', 1);
		}
		else {
			putcursor(' ', 1);
			locate2(0, y);
		}
	}
	clearline();
	if (stable_standout) Xputterm(END_STANDOUT);
	kanjiputs3(dupl, cx2, len2 - i, i, top);
	win_x += len2 - i;
#ifndef	_NOORIGSHELL
	if (shellmode);
	else
#endif
	while (win_y < maxline - 1) {
		Xlocate(xpos, ++win_y);
		Xputterm(L_CLEAR);
	}
	if (vi) Xputterm(T_NORMALCURSOR);
	setcursor(s, *cxp, cx2, *lenp);
	Xtflush();
	free(dupl);
}

static int NEAR preparestr(sp, cx, lenp, sizep, rins, vins)
char **sp;
int cx, *lenp;
ALLOC_T *sizep;
int rins, vins;
{
	int rw, vw;

	if (overflow(vlen(*sp, *lenp) + vins)) return(-1);

	if (!overwritemode) {
		insertchar(*sp, cx, *lenp, vins);
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
	else if (vins > vw) insertchar(*sp, cx, *lenp, vins - vw);
	else if (vins < vw) deletechar(*sp, cx, *lenp, vw - vins);

	if (!rw) /*EMPTY*/;
	else if (rins > rw) {
		*sp = c_realloc(*sp, *lenp + rins - rw, sizep);
		insshift(*sp, cx, *lenp, rins - rw);
	}
	else if (rins < rw) delshift(*sp, cx, *lenp, rw - rins);

	*lenp += rins - rw;

	return(0);
}

static int NEAR insertstr(sp, cx, len, sizep, strins, ins, quote)
char **sp;
int cx, len;
ALLOC_T *sizep;
char *strins;
int ins, quote;
{
	char *dupl;
	int i, j, cx2, dx, dy, f, l, ptr, len2, max;

	len2 = vlen(*sp, len);
	max = maxscr();
	if (len2 + vlen(strins, ins) > max) ins = rlen(strins, max - len2);

	if (ins > 0) {
		if (onkanji1(strins, ins - 1)) ins--;
#ifdef	CODEEUC
		else if (isekana(strins, ins - 1)) ins--;
#endif
	}
	if (ins <= 0) return(0);

	dupl = malloc2(ins * 2 + 1);
	insertchar(*sp, cx, len, vlen(strins, ins));
	*sp = c_realloc(*sp, len + ins, sizep);
	insshift(*sp, cx, len, ins);
	for (i = 0; i < ins; i++) (*sp)[cx + i] = ' ';
	len += ins;
	for (i = j = 0; i < ins; i++, j++) {
		if (quote < '\0') dupl[j] = (*sp)[cx + j] = strins[i];
#ifdef	CODEEUC
		else if (isekana(strins, i)) {
			dupl[j] = (*sp)[cx + j] = strins[i];
			j++;
			dupl[j] = (*sp)[cx + j] = strins[++i];
		}
#endif
		else if (iskanji1(strins, i)) {
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
			insertchar(*sp, cx, len, f);
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
			insertchar(*sp, cx, len, 1);
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
	dx = ptr2col(cx2);
	dy = ptr2line(cx2);
	i = (dy + 1) * maxcol - plen - cx2;
	ptr = 0;
#ifndef	_NOORIGSHELL
	if (dumbmode) {
		i = (lastofs2) ? 1 : plen;
		dx = cx2 - lastofs2 + len2;
		if (i + dx >= maxcol) {
			cx += j;
			displaystr(*sp, &cx, &len);
			ptr = len2;
		}
	}
	else
#endif	/* !_NOORIGSHELL */
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
		if (ypos + dy >= maxline - 1) {
			while (ypos + dy >= maxline - 1) scrollup();
			locate2(dx, dy);
		}
		l = i - ptr;
		if (f) l++;
		kanjiputs2(dupl, l, ptr);
		win_x += l;
		newline(++dy);
		if (f) dupl[f - 1] = dupl[f] = ' ';
		ptr = i;
		dx = 0;
		i += maxcol;
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
	char *cp;
	int i, len, maxlen, duprow, dupminfilename, dupcolumns, dupdispmode;

	duprow = FILEPERROW;
	dupminfilename = minfilename;
	dupcolumns = curcolumns;
	dupdispmode = dispmode;
	minfilename = n_column;
	dispmode = 2;

	if (argv) {
		selectlist = (namelist *)malloc2(argc * sizeof(namelist));
		maxlen = 0;
		for (i = 0; i < argc; i++) {
			memset((char *)&(selectlist[i]), 0, sizeof(namelist));
			cp = strrdelim(argv[i], 0);
			if (cp && !cp[1]) len = cp - argv[i];
			else {
				len = strlen(argv[i]);
				cp = NULL;
			}

			selectlist[i].name = strndup2(argv[i], len);
			selectlist[i].flags = (F_ISRED | F_ISWRI);
			selectlist[i].st_mode = (cp) ? S_IFDIR : S_IFREG;
			selectlist[i].tmpflags = F_STAT;
			len = strlen2(argv[i]);
			if (maxlen < len) maxlen = len;
		}
		i = sorton;
		sorton = 1;
		qsort(selectlist, argc, sizeof(namelist), cmplist);
		sorton = i;

		if (lcmdline < 0) {
			int j, n;

			n = 1;
			Xcputnl();
			if (argc < LIMITSELECTWARN) i = 'Y';
			else {
				Xcprintf2("There are %d possibilities.", argc);
				Xcputs2("  Do you really");
				Xcputnl();
				Xcputs2("wish to see them all? (y or n)");
				Xtflush();
				for (;;) {
					if ((i = getkey2(0)) < K_MIN) {
						i = toupper2(i);
						if (i == 'Y' || i == 'N')
							break;
					}
					ringbell();
					Xtflush();
				}
				Xcputnl();
				n += 2;
			}

			if (i == 'N') /*EMPTY*/;
			else if (n_column < maxlen + 2) {
				n += argc;
				for (i = 0; i < argc; i++) {
					Xkanjiputs(selectlist[i].name);
					Xcputnl();
				}
			}
			else {
				tmpcolumns = n_column / (maxlen + 2);
				len = (argc + tmpcolumns - 1) / tmpcolumns;
				n += len;
				for (i = 0; i < len; i++) {
					for (j = i; j < argc; j += len)
						Xcprintf2("%-*.*k",
							maxlen + 2, maxlen + 2,
							selectlist[j].name);
					Xcputnl();
				}
			}

# ifndef	_NOORIGSHELL
			if (dumbmode || shellmode) {
				win_x = xpos;
				win_y = 0;
			}
			else
# endif
			if (ypos + n < maxline - 1) ypos += n;
			else ypos = maxline - 1;
			for (i = 0; i < argc; i++) free(selectlist[i].name);
			free(selectlist);
			selectlist = NULL;
			FILEPERROW = duprow;
			minfilename = dupminfilename;
			curcolumns = dupcolumns;
			dispmode = dupdispmode;

			return;
		}

		i = filetop(win);
		if (maxcmdline) tmpfileperrow = maxcmdline;
		else if (ypos >= i + FILEPERROW) tmpfileperrow = FILEPERROW;
		else {
			tmpfileperrow = ypos - i;
			if (tmpfileperrow < WFILEMIN) tmpfileperrow = WFILEMIN;
		}
		if ((n_column / 5) - 2 - 1 >= maxlen) tmpcolumns = 5;
		else if ((n_column / 3) - 2 - 1 >= maxlen) tmpcolumns = 3;
		else if ((n_column / 2) - 2 - 1 >= maxlen) tmpcolumns = 2;
		else tmpcolumns = 1;
		FILEPERROW = tmpfileperrow;
		curcolumns = tmpcolumns;
		tmpfilepos = listupfile(selectlist, argc, NULL, 1);
		maxselect = argc;
	}
	else if (argc < 0) {
		for (i = 0; i < maxselect; i++) free(selectlist[i].name);
		free(selectlist);
		selectlist = NULL;
	}
	else {
		FILEPERROW = tmpfileperrow;
		curcolumns = tmpcolumns;
		if (tmpfilepos >= maxselect) tmpfilepos %= (tmpfilepos - argc);
		else if (tmpfilepos < 0)
			tmpfilepos = maxselect - 1
				- ((maxselect - 1 - argc)
				% (argc - tmpfilepos));
		if (argc / FILEPERPAGE != tmpfilepos / FILEPERPAGE)
			tmpfilepos = listupfile(selectlist, maxselect,
				selectlist[tmpfilepos].name, 1);
		else if (argc != tmpfilepos) {
			putname(selectlist, argc, -1);
			putname(selectlist, tmpfilepos, 1);
		}
	}

	FILEPERROW = duprow;
	minfilename = dupminfilename;
	curcolumns = dupcolumns;
	dispmode = dupdispmode;
}

/*ARGSUSED*/
static int NEAR completestr(sp, cx, len, sizep, comline, cont, h)
char **sp;
int cx, len;
ALLOC_T *sizep;
int comline, cont, h;
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
		setcursor(*sp, cx, -1, len);
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
# ifndef	NOUID
	if (h == HST_USER || h == HST_GROUP) quote = -1;
	else
# endif
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
	if (vartop) /*EMPTY*/;
	else
# endif
# ifndef	NOUID
	if (h == HST_USER || h == HST_GROUP) /*EMPTY*/;
	else
# endif
	cp = evalpath(cp, 0);
	hasmeta = 0;
# ifndef	NOUID
	if (h == HST_USER || h == HST_GROUP) /*EMPTY*/;
	else
# endif
	for (i = 0; cp[i]; i++) {
		if (strchr(METACHAR, cp[i])) {
			hasmeta = 1;
			break;
		}
		if (iskanji1(cp, i)) i++;
	}

	if (selectlist && cont < 0) {
		argv = (char **)malloc2(1 * sizeof(char *));
		l = strlen(selectlist[tmpfilepos].name);
		i = (s_isdir(&(selectlist[tmpfilepos]))) ? 1 : 0;
		argv[0] = (char *)malloc2(l + i + 1);
		memcpy(argv[0], selectlist[tmpfilepos].name, l);
		if (i) argv[0][l] = '/';
		argv[0][l + i] = '\0';
		argc = 1;
	}
# ifndef	NOUID
	else if (h == HST_USER) {
		argv = NULL;
		l = strlen(cp);
		argc = completeuser(cp, l, 0, &argv, 0);
	}
	else if (h == HST_GROUP) {
		argv = NULL;
		l = strlen(cp);
		argc = completegroup(cp, l, 0, &argv);
	}
# endif
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
	if (argc != 1 || !cp) fix = '\0';
# ifndef	NOUID
	else if (h == HST_USER || h == HST_GROUP) fix = ' ';
# endif
	else fix = ((tmp = strrdelim(cp, 0)) && !tmp[1]) ? _SC_ : ' ';

	if (!cp || ((ins = (int)strlen(cp) - ins) <= 0 && fix != ' ')) {
		if (cont <= 0) {
			ringbell();
			l = 0;
		}
		else {
			selectfile(argc, argv);
			if (lcmdline < 0) displaystr(*sp, &cx, &len);
			setcursor(*sp, cx, -1, len);
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
# ifndef	NOUID
	if (h == HST_USER || h == HST_GROUP) /*EMPTY*/;
	else
# endif
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
			setcursor(*sp, quoted, -1, len);
			deletechar(*sp, quoted, len, 1);
			delshift(*sp, quoted, len--, 1);
			l--;
			setcursor(*sp, --cx, -1, len);
		}
		else if (!overflow(vlen(*sp, len - i) + vlen(home, hlen))) {
			if (home) {
				setcursor(*sp, top, -1, len);
				deletechar(*sp, top, len, i);
				delshift(*sp, top, len, i);
				len -= i;
				l -= i;
				cx -= i;
				i = insertstr(sp, top, len,
					sizep, home, hlen, '\0');
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
				setcursor(*sp, i, -1, len);
				deletechar(*sp, i, len, 1);
				delshift(*sp, i, len--, 1);
				l--;
				cx--;
			}
# endif	/* !FAKEMETA */
			setcursor(*sp, top, -1, len);
			insertchar(*sp, top, len, 1);
			*sp = c_realloc(*sp, len + 2, sizep);
			insshift(*sp, top, len++, 1);
			l++;
			(*sp)[top] = quote = '"';
			putcursor(quote, 1);
			setcursor(*sp, ++cx, -1, len);
		}
		else hasmeta = 0;
	}

	tmp = cp + (int)strlen(cp) - ins;
	if (fix == _SC_) ins--;
	i = insertstr(sp, cx, len, sizep, tmp, ins, quote);
	len += i;
	l += i;
	if (fix && !overflow(vlen(*sp, len))) {
		cx += i;
		if (quote > '\0' && (fix != _SC_ || hasmeta)) {
			insertchar(*sp, cx, len, 1);
			*sp = c_realloc(*sp, len + 2, sizep);
			insshift(*sp, cx, len++, 1);
			l++;
			(*sp)[cx++] = quote;
			putcursor(quote, 1);
		}
# ifndef	NOUID
		if (h == HST_USER || h == HST_GROUP) /*EMPTY*/;
		else
# endif
		if (!overflow(vlen(*sp, len))) {
			insertchar(*sp, cx, len, 1);
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

static int NEAR getkanjikey(buf, ch)
char *buf;
int ch;
{
#ifndef	_NOKANJICONV
	char tmpkanji[4];
	int n, code;
#endif
	int ch2;

#ifdef	_NOKANJICONV
# ifdef	CODEEUC
	if (isekana2(ch)) {
		buf[0] = (char)C_EKANA;
		buf[1] = (ch * 0xff);
		buf[2] = '\0';
		return(1);
	}
# else
	if (iskana2(ch)) {
		buf[0] = ch;
		buf[1] = '\0';
		return(1);
	}
# endif
#else	/* !_NOKANJICONV */
# ifndef	_NOPTY
	if (parentfd >= 0) code = ptyinkcode;
	else
# endif
	code = inputkcode;
	if (code == EUC && isekana2(ch)) {
		tmpkanji[0] = (char)C_EKANA;
		tmpkanji[1] = (ch & 0xff);
		tmpkanji[2] = '\0';
		kanjiconv(buf, tmpkanji, 2, code, DEFCODE, L_INPUT);
		return(1);
	}
	if (code == SJIS && iskana2(ch)) {
		tmpkanji[0] = ch;
		tmpkanji[1] = '\0';
		kanjiconv(buf, tmpkanji, 2, code, DEFCODE, L_INPUT);
		return(1);
	}
	if (code == UTF8 || code == M_UTF8) {
		if ((ch & 0xff00) || !ismsb(ch)) /*EMPTY*/;
		else if (!kbhit2(WAITMETA * 1000L)
		|| (ch2 = getch2()) == EOF) {
			buf[0] = '\0';
			return(-1);
		}
		else if (isutf2(ch, ch2)) {
			tmpkanji[0] = ch;
			tmpkanji[1] = ch2;
			tmpkanji[2] = '\0';
			n = kanjiconv(buf, tmpkanji, 2,
				code, DEFCODE, L_INPUT);
			return(n);
		}
		else if (!kbhit2(WAITMETA * 1000L) || (n = getch2()) == EOF) {
			buf[0] = '\0';
			return(-1);
		}
		else {
			tmpkanji[0] = ch;
			tmpkanji[1] = ch2;
			tmpkanji[2] = n;
			tmpkanji[3] = '\0';
			n = kanjiconv(buf, tmpkanji, 3,
				code, DEFCODE, L_INPUT);
# ifdef	CODEEUC
			if (isekana(buf, 0)) n = 1;
# endif
			return(n);
		}
	}
#endif	/* !_NOKANJICONV */

	if (isinkanji1(ch, code)) {
		if (!kbhit2(WAITMETA * 1000L)
		|| (ch2 = getch2()) == EOF || !isinkanji2(ch2, code)) {
			buf[0] = '\0';
			return(-1);
		}
#ifdef	_NOKANJICONV
		buf[0] = ch;
		buf[1] = ch2;
		buf[2] = '\0';
#else
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		kanjiconv(buf, tmpkanji, 2, code, DEFCODE, L_INPUT);
#endif
		return(2);
	}

	if (ch >= K_MIN) {
		buf[0] = '\0';
		return(-1);
	}

	buf[0] = ch;
	buf[1] = '\0';

	return((iscntrl2(ch) || ismsb(ch)) ? 0 : 1);
}

static int NEAR copyhist(sp, cxp, lenp, sizep, hist)
char **sp;
int *cxp, *lenp;
ALLOC_T *sizep;
char *hist;
{
#ifndef	_NOORIGSHELL
	int y1, y2, len2;

	len2 = vlen(*sp, *lenp);
	y1 = ptr2line(len2);
	if (dumbmode) {
		setcursor(*sp, 0, 0, *lenp);
		clearline();
	}
#endif	/* !_NOORIGSHELL */

	if (sizep) {
		*lenp = (hist) ? strlen(hist) : 0;
		if (*cxp < 0 || *cxp > *lenp) *cxp = *lenp;
		*sp = c_realloc(*sp, *lenp, sizep);
		if (hist) memcpy(*sp, hist, *lenp + 1);
		else **sp = '\0';
	}
	displaystr(*sp, cxp, lenp);

#ifndef	_NOORIGSHELL
	if (dumbmode) /*EMPTY*/;
	else if (shellmode) {
		y2 = ptr2line(vlen(*sp, *lenp));
		if (y1 > y2) {
			while (y1 > y2) {
				locate2(0, y1--);
				Xputterm(L_CLEAR);
			}
			setcursor(*sp, *cxp, -1, *lenp);
		}
	}
#endif	/* !_NOORIGSHELL */

	return(vlen(*sp, *cxp));
}

static int NEAR _inputstr_up(sp, cxp, cx2, lenp, sizep, histnop, h, tmp)
char **sp;
int *cxp, cx2, *lenp;
ALLOC_T *sizep;
int *histnop, h;
char **tmp;
{
	keyflush();
#ifndef	_NOCOMPLETE
	if (completable(h) && selectlist) {
		selectfile(tmpfilepos--, NULL);
		setcursor(*sp, *cxp, cx2, *lenp);
	}
	else
#endif
#ifdef	_NOORIGSHELL
	if (cx2 < maxcol)
#else
	if (dumbmode || cx2 < maxcol)
#endif
	{
		if (nohist(h) || !history[h] || *histnop >= (int)histsize[h]
		|| !history[h][*histnop]) {
			ringbell();
			return(cx2);
		}

		if (!*tmp) {
			(*sp)[*lenp] = '\0';
			*tmp = strdup2(*sp);
		}
		*cxp = -1;
		cx2 = copyhist(sp, cxp, lenp, sizep, history[h][(*histnop)++]);
	}
	else {
		if (!within(cx2) && !ptr2col(cx2)) {
			leftcursor();
			cx2--;
		}
		cx2 -= maxcol;
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

static int NEAR _inputstr_down(sp, cxp, cx2, lenp, sizep, histnop, h, tmp)
char **sp;
int *cxp, cx2, *lenp;
ALLOC_T *sizep;
int *histnop, h;
char **tmp;
{
	int len2;

	len2 = vlen(*sp, *lenp);
	keyflush();
#ifndef	_NOCOMPLETE
	if (completable(h) && selectlist) {
		selectfile(tmpfilepos++, NULL);
		setcursor(*sp, *cxp, cx2, *lenp);
	}
	else
#endif
#ifdef	_NOORIGSHELL
	if (cx2 + maxcol > len2 || (cx2 + maxcol == len2
	&& !within(len2) && !ptr2col(len2)))
#else
	if (dumbmode || (cx2 + maxcol > len2 || (cx2 + maxcol == len2
	&& !within(len2) && !ptr2col(len2))))
#endif
	{
		if (nohist(h) || !history[h] || *histnop <= 0) {
			ringbell();
			return(cx2);
		}

		*cxp = -1;
		if (--(*histnop) > 0)
			cx2 = copyhist(sp, cxp, lenp,
				sizep, history[h][*histnop - 1]);
		else {
			cx2 = copyhist(sp, cxp, lenp, sizep, *tmp);
			if (*tmp) free(*tmp);
			*tmp = NULL;
		}
	}
	else {
		cx2 += maxcol;
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

static int NEAR _inputstr_delete(s, cx, len)
char *s;
int cx, len;
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

	deletechar(s, cx, len, vw);
	delshift(s, cx, len, rw);

	return(len -= rw);
}

static int NEAR _inputstr_enter(sp, cxp, cx2, lenp, sizep)
char **sp;
int *cxp, cx2, *lenp;
ALLOC_T *sizep;
{
	int i, len, quote;

	if (!curfilename) {
		ringbell();
		return(cx2);
	}

	quote = '\0';
	keyflush();
	if (curfilename[i = 0] != '~') for (; curfilename[i]; i++) {
		if (strchr(METACHAR, curfilename[i])) break;
		if (iskanji1(curfilename, i)) i++;
	}
	len = strlen(curfilename);
	if (curfilename[i]
	&& !overflow(vlen(*sp, *lenp) + vlen(curfilename, len))) {
		insertchar(*sp, *cxp, *lenp, 1);
		*sp = c_realloc(*sp, *lenp + 2, sizep);
		insshift(*sp, *cxp, (*lenp)++, 1);
		(*sp)[(*cxp)++] = quote = '"';
		putcursor(quote, 1);
	}
	i = insertstr(sp, *cxp, *lenp, sizep, curfilename, len, quote);
	if (!i) {
		ringbell();
		return(cx2);
	}
	*cxp += i;
	*lenp += i;
	if (quote > '\0') {
		insertchar(*sp, *cxp, *lenp, 1);
		*sp = c_realloc(*sp, *lenp + 2, sizep);
		insshift(*sp, *cxp, (*lenp)++, 1);
		(*sp)[(*cxp)++] = quote;
		putcursor(quote, 1);
	}
	cx2 = vlen(*sp, *cxp);
	if (iseol(cx2)) setcursor(*sp, *cxp, cx2, *lenp);

	return(cx2);
}

#if	FD >= 2
static int NEAR _inputstr_case(s, cxp, cx2, len, upper)
char *s;
int *cxp, cx2, len, upper;
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
# ifndef	_NOORIGSHELL
			if (dumbmode) checkcursor(s, *cxp, cx2, len);
			else
# endif
			if (within(cx2) && ptr2col(cx2) < 1)
				setcursor(s, *cxp, cx2, len);
			return(cx2);
		}
	}

	return(rightchar(s, cxp, cx2, len));
}

static int NEAR search_matchlen(s, cx)
char *s;
int cx;
{
	int n;

	if (searchstr) {
		n = cx - strlen(searchstr);
		if (n < 0) n = 0;
		for (; n < cx; n++)
			if (!memcmp(searchstr, &(s[n]), cx - n)) return(n);
	}

	return(cx);
}

static char *NEAR search_up(s, cxp, len, bias, histnop, h, tmp)
char *s;
int *cxp, len, bias, *histnop, h;
char **tmp;
{
	int n, cx, hlen, slen;

	searchmode = -1;
	if (!searchstr) return(NULL);

	slen = strlen(searchstr);
	cx = search_matchlen(s, *cxp) - bias;
	if (cx > len - slen) cx = len - slen;
	for (; cx >= 0; cx--) {
		if (strncmp(&(s[cx]), searchstr, slen)) continue;
		*cxp = cx + slen;
		return(NULL);
	}

	if (nohist(h) || !history[h]) {
		ringbell();
		searchmode = -2;
		return(NULL);
	}

	for (n = *histnop; n < (int)histsize[h]; n++) {
		if (!history[h][n]) break;
		hlen = strlen(history[h][n]);
		for (cx = hlen - slen; cx >= 0; cx--) {
			if (strncmp(&(history[h][n][cx]), searchstr, slen))
				continue;

			if (!*tmp) {
				s[len] = '\0';
				*tmp = strdup2(s);
			}
			*cxp = cx + slen;
			*histnop = n + 1;
			return(history[h][n]);
		}
	}

	ringbell();
	searchmode = -2;

	return(NULL);
}

static char *NEAR search_down(s, cxp, len, bias, histnop, h, tmp)
char *s;
int *cxp, len, bias, *histnop, h;
char **tmp;
{
	int n, cx, hlen, slen;

	searchmode = 1;
	if (!searchstr) return(NULL);

	slen = strlen(searchstr);
	cx = search_matchlen(s, *cxp) + bias;
	for (; cx <= len - slen; cx++) {
		if (strncmp(&(s[cx]), searchstr, slen)) continue;
		*cxp = cx + slen;
		return(NULL);
	}

	if (nohist(h) || !history[h]) {
		ringbell();
		searchmode = 2;
		return(NULL);
	}

	for (n = *histnop - 2; n >= 0; n--) {
		hlen = strlen(history[h][n]);
		for (cx = 0; cx <= hlen - slen; cx++) {
			if (strncmp(&(history[h][n][cx]), searchstr, slen))
				continue;

			*cxp = cx + slen;
			*histnop = n + 1;
			return(history[h][n]);
		}
	}

	if (*tmp) {
		hlen = strlen(*tmp);
		for (cx = 0; cx <= hlen - slen; cx++) {
			if (strncmp(&((*tmp)[cx]), searchstr, slen)) continue;

			*cxp = cx + slen;
			*histnop = 0;
			return(*tmp);
		}
	}

	ringbell();
	searchmode = 2;

	return(NULL);
}
#endif	/* FD >= 2 */

static int NEAR _inputstr_input(sp, cxp, cx2, lenp, sizep, buf, vw)
char **sp;
int *cxp, cx2, *lenp;
ALLOC_T *sizep;
char *buf;
int vw;
{
	int rw;

	rw = strlen(buf);
	if (vw <= 0 || preparestr(sp, *cxp, lenp, sizep, rw, vw) < 0) {
		ringbell();
		keyflush();
		return(cx2);
	}
	memcpy(&((*sp)[*cxp]), buf, rw);
	*cxp += rw;
	Xcprintf2("%.*k", vw, buf);
	win_x += vw;
	cx2 = vlen(*sp, *cxp);

#ifndef	_NOORIGSHELL
	if (dumbmode) checkcursor(*sp, *cxp, cx2, *lenp);
	else
#endif
	if (within(cx2) && ptr2col(cx2) < vw) setcursor(*sp, *cxp, cx2, *lenp);

	return(cx2);
}

static int NEAR _inputstr(sp, size, def, comline, h)
char **sp;
ALLOC_T size;
int def, comline, h;
{
#if	!MSDOS
	keyseq_t key;
#endif
#if	FD >= 2
	ALLOC_T searchsize;
#endif
	char *tmphist, buf[5];
	int i, n, ch, ch2, cx, cx2, ocx2, len, hist, quote, sig;

	subwindow = 1;

#ifndef	_NOORIGSHELL
	if (dumbmode) maxcol -= RIGHTMARGIN;
	if (shellmode) sig = 0;
	else
#endif
	sig = 1;
	Xgetkey(-1, VI_INSERT);
#ifndef	_NOCOMPLETE
	tmpfilepos = -1;
#endif
	cx = len = strlen(*sp);
	if (def >= 0 && def < maxcol) {
		while (def > len) {
			*sp = c_realloc(*sp, len, &size);
			(*sp)[len++] = ' ';
		}
		cx = def;
	}
	displaystr(*sp, &cx, &len);
	cx2 = vlen(*sp, cx);
	keyflush();
	hist = 0;
	tmphist = NULL;
	quote = 0;
	ch = -1;
#if	FD >= 2
	searchmode = 0;
	searchstr = NULL;
	searchsize = (ALLOC_T)0;
#endif

	do {
		Xtflush();
		ch2 = ch;
		ocx2 = cx2;
		if (quote) {
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

			n = getkanjikey(buf, i);
			if (n) {
				ocx2 = cx2 = _inputstr_input(sp, &cx, cx2,
					&len, &size, buf, n);
				continue;
			}

			keyflush();
			ch = '\0';
			if (!*buf) continue;

			n = (iscntrl2(buf[0])) ? 2 : 4;
			if (preparestr(sp, cx, &len, &size, 1, n) < 0) {
				ringbell();
				continue;
			}

			(*sp)[cx++] = buf[0];
			if (n == 2)
				snprintf2(buf, sizeof(buf), "^%c",
					(i + '@') & 0x7f);
			else snprintf2(buf, sizeof(buf), "\\%03o", i);
			cx2 = putstr(buf, *sp, cx, cx2, len);

			continue;
		}

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

#if	FD >= 2
		if (searchmode) {
			ALLOC_T *sizep;
			char *cp;

			cp = NULL;
			n = 0;
			switch (ch) {
				case K_CTRL('S'):
					keyflush();
					searchmode = 1;
					n++;
					break;
				case K_CTRL('R'):
					keyflush();
					searchmode = -1;
					n++;
					break;
				case K_BS:
					keyflush();
					if (searchstr
					&& (i = strlen(searchstr)) > 0)
						searchstr[i - 1] = '\0';
					else ringbell();
					break;
				default:
					i = getkanjikey(buf, ch);
					if (i <= 0) {
						keyflush();
						searchmode = 0;
						if (searchstr) free(searchstr);
						searchstr = NULL;
						searchsize = (ALLOC_T)0;
						break;
					}

					i = (searchstr)
						? strlen(searchstr) : 0;
					searchstr = c_realloc(searchstr,
						i + 1, &searchsize);
					searchstr[i++] = buf[0];
					if (buf[1]) searchstr[i++] = buf[1];
					searchstr[i] = '\0';
					break;
			}

			if (searchmode < 0)
				cp = search_up(*sp, &cx, len, n,
					&hist, h, &tmphist);
			else if (searchmode > 0)
				cp = search_down(*sp, &cx, len, n,
					&hist, h, &tmphist);
			sizep = (cp) ? &size : NULL;
			ocx2 = cx2 = copyhist(sp, &cx, &len, sizep, cp);

			if (searchmode) continue;
		}
#endif	/* FD >= 2 */

		switch (ch) {
			case K_RIGHT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (completable(h) && selectlist) {
					i = tmpfilepos;
					tmpfilepos += tmpfileperrow;
					selectfile(i, NULL);
					ocx2 = -1;
				}
				else
#endif
				if (cx >= len) ringbell();
#ifndef	_NOEDITMODE
				else if (isvimode() && len && cx >= len - 1)
					ringbell();
#endif
				else ocx2 = cx2 =
					rightchar(*sp, &cx, cx2, len);
				break;
			case K_LEFT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (completable(h) && selectlist) {
					i = tmpfilepos;
					tmpfilepos -= tmpfileperrow;
					selectfile(i, NULL);
					ocx2 = -1;
				}
				else
#endif
				if (cx <= 0) ringbell();
				else ocx2 = cx2 = leftchar(*sp, &cx, cx2, len);
				break;
			case K_BEG:
				keyflush();
				cx = cx2 = 0;
				break;
			case K_EOL:
				keyflush();
				cx = len;
#ifndef	_NOEDITMODE
				if (isvimode() && len) cx--;
#endif
				cx2 = vlen(*sp, cx);
				break;
			case K_BS:
				keyflush();
				if (cx <= 0) {
					ringbell();
					break;
				}
				ocx2 = cx2 = leftchar(*sp, &cx, cx2, len);
				len = _inputstr_delete(*sp, cx, len);
				break;
			case K_DC:
				keyflush();
				len = _inputstr_delete(*sp, cx, len);
#ifndef	_NOEDITMODE
				if (isvimode() && len && cx >= len)
					ocx2 = cx2 =
						leftchar(*sp, &cx, cx2, len);
#endif
				break;
			case K_DL:
				keyflush();
				if (cx < len) truncline(*sp, cx, cx2, len);
				len = cx;
#ifndef	_NOEDITMODE
				if (isvimode() && len)
					ocx2 = cx2 =
						leftchar(*sp, &cx, cx2, len);
#endif
				break;
			case K_CTRL('L'):
				keyflush();
#ifndef	_NOORIGSHELL
				if (dumbmode) rewritecursor(*sp, 0, 0, len);
				else if (shellmode) {
					locate2(0, 0);
					Xputterm(L_CLEAR);
				}
				else
#endif
				{
#ifndef	_NOPTY
					if (minline > 0) {
						i = filetop(win) + FILEPERROW;
						if (maxline != i) {
							ypos += i - maxline;
							maxline = i;
						}
					}
					else
#endif
					if (maxline != n_line) {
						ypos += n_line - maxline;
						maxline = n_line;
					}

					for (i = 0; i < WCMDLINE; i++) {
						if (ypos + i >= maxline) break;
						Xlocate(xpos, ypos + i);
						Xputterm(L_CLEAR);
					}
				}
				displaystr(*sp, &cx, &len);
				break;
			case K_UP:
				ocx2 = cx2 = _inputstr_up(sp, &cx, cx2, &len,
					&size, &hist, h, &tmphist);
				break;
			case K_DOWN:
				ocx2 = cx2 = _inputstr_down(sp, &cx, cx2, &len,
					&size, &hist, h, &tmphist);
				break;
			case K_IL:
				keyflush();
				quote = 1;
				break;
			case K_ENTER:
				ocx2 = cx2 = _inputstr_enter(sp, &cx, cx2,
					&len, &size);
				break;
#if	FD >= 2
			case K_IC:
				keyflush();
				overwritemode = 1 - overwritemode;
				break;
			case K_PPAGE:
				ocx2 = cx2 = _inputstr_case(*sp, &cx, cx2,
					len, 0);
				break;
			case K_NPAGE:
				ocx2 = cx2 = _inputstr_case(*sp, &cx, cx2,
					len, 1);
				break;
			case K_CTRL('S'):
				keyflush();
				if (nohist(h) || !history[h]) {
					ringbell();
					break;
				}
				searchmode = 1;
				if (searchstr) free(searchstr);
				searchstr = NULL;
				searchsize = (ALLOC_T)0;
				copyhist(sp, &cx, &len, NULL, NULL);
				break;
			case K_CTRL('R'):
				keyflush();
				if (nohist(h) || !history[h]) {
					ringbell();
					break;
				}
				searchmode = -1;
				if (searchstr) free(searchstr);
				searchstr = NULL;
				searchsize = (ALLOC_T)0;
				copyhist(sp, &cx, &len, NULL, NULL);
				break;
#endif	/* FD >= 2 */
#ifndef	_NOCOMPLETE
			case '\t':
				keyflush();
				if (!completable(h) || selectlist) {
					ringbell();
					break;
				}
				i = completestr(sp, cx, len, &size,
					comline, (ch2 == ch) ? 1 : 0, h);
				if (i <= 0) {
					if (!i) ringbell();
					break;
				}
				cx += i;
				len += i;
				cx2 = vlen(*sp, cx);
				if (iseol(cx2)) ocx2 = -1;
				break;
#endif	/* !_NOCOMPLETE */
			case K_CR:
				keyflush();
#ifndef	_NOCOMPLETE
				if (!completable(h) || !selectlist) break;
				i = completestr(sp, cx, len, &size, 0, -1, h);
				cx += i;
				len += i;
				cx2 = vlen(*sp, cx);
				if (iseol(cx2)) ocx2 = -1;
				ch = '\0';
#endif	/* !_NOCOMPLETE */
				break;
			case K_ESC:
				keyflush();
				break;
			default:
				i = getkanjikey(buf, ch);
				ocx2 = cx2 = _inputstr_input(sp, &cx, cx2,
					&len, &size, buf, i);
				break;
		}
#ifndef	_NOCOMPLETE
		if (completable(h) && selectlist && ch != '\t'
		&& ch != K_RIGHT && ch != K_LEFT
		&& ch != K_UP && ch != K_DOWN) {
			selectfile(-1, NULL);
			if (!maxcmdline) rewritefile(0);
			else {
				n = filetop(win);
				for (i = 0; i < maxcmdline; i++) {
					Xlocate(0, n + i);
					Xputterm(L_CLEAR);
				}
			}
			n = ypos;
			if (!lcmdline) ypos = L_CMDLINE + maxline - n_line;
			else if (lcmdline > 0) ypos = lcmdline;
			else ypos = maxline - 1;
			while (ypos > n) scrollup();
			displaystr(*sp, &cx, &len);
			ocx2 = -1;
		}
		if (ocx2 != cx2) setcursor(*sp, cx, cx2, len);
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
	setcursor(*sp, len, -1, len);
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
		if (maxcmdline) i = 0;
#ifndef	_NOPTY
		else if (minline > 0) i = 1;
#endif
		else if (hideclock) i = 1;
	}
	(*sp)[len] = '\0';

	Xtflush();
	hideclock = 0;
	if (i) rewritefile(1);

	return(ch);
}

static VOID NEAR dispprompt(s, set)
char *s;
int set;
{
	static char *prompt = NULL;
	char *buf;

	if (set > 0) prompt = s;
#if	FD >= 2
	else if (searchmode > 0) s = (searchmode > 1) ? SEAFF_K : SEAF_K;
	else if (searchmode < 0) s = (searchmode < -1) ? SEABF_K : SEAB_K;
#endif
	else if (!s) s = prompt;

	if (set < 0) {
		if (s && *s) {
			plen = fprintf2(NULL, "%k", s);
#ifndef	_NOORIGSHELL
			if (dumbmode || shellmode) /*EMPTY*/;
			else
#endif
			plen++;
		}
		else {
			plen = evalprompt(&buf, promptstr);
			free(buf);
#ifdef	_NOORIGSHELL
			plen++;
#endif
		}
		return;
	}

#ifndef	_NOORIGSHELL
	if (dumbmode) {
		backcursor(0);
		forwcursor(xpos);
		win_y = 0;
	}
	else if (shellmode) locate2(0, 0);
	else
#endif
	Xlocate(xpos, ypos);
	if (s && *s) {
#ifndef	_NOORIGSHELL
		if (dumbmode || shellmode) plen = Xkanjiputs(s);
		else
#endif
		{
			Xputch2(' ');
			Xputterm(T_STANDOUT);
			plen = 1 + Xkanjiputs(s);
			Xputterm(END_STANDOUT);
		}
	}
	else {
#ifdef	_NOORIGSHELL
		Xputch2(' ');
		Xputterm(T_STANDOUT);
#endif
		plen = evalprompt(&buf, promptstr);
		Xkanjiputs(buf);
		free(buf);
#ifdef	_NOORIGSHELL
		plen++;
		Xputterm(END_STANDOUT);
#else
		if (dumbmode);
		else
#endif
		Xputterm(T_NORMAL);
	}

	win_x += plen;
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
#ifndef	_NOORIGSHELL
	dumbmode = (dumbterm || dumbshell) ? !Xtermmode(-1) : 0;
	if (dumbmode || shellmode) lcmdline = -1;
#endif

	if (!lcmdline) ypos = L_CMDLINE;
	else if (lcmdline > 0) ypos = lcmdline;
	else ypos = n_line - 1;

	maxcol = n_column - 1 - xpos;
	if (maxcmdline) {
		minline = ypos;
		maxline = ypos + maxcmdline;
	}
	else {
		minline = 0;
		maxline = n_line;
	}
#ifndef	_NOPTY
# ifndef	_NOORIGSHELL
	if (!fdmode && shellmode) /*EMPTY*/;
	else
# endif
	if (isptymode() && parentfd < 0) {
		minline = filetop(win);
		maxline = minline + FILEPERROW;
		ypos -= n_line - maxline;
	}
#endif

#ifndef	_NOORIGSHELL
	if (dumbmode || shellmode) win_y = 0;
	else
#endif
	{
		win_y = ypos;
		for (i = 0; i < WCMDLINE; i++) {
			if (ypos + i >= maxline) break;
			Xlocate(0, ypos + i);
			Xputterm(L_CLEAR);
		}
	}
	dispprompt(prompt, 1);
	Xtflush();

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
	if (h == HST_PATH && windows > 1) {
		if ((i = win - 1) < 0) i = windows - 1;
		entryhist(1, winvar[i].v_fullpath, 1);
	}
#endif

	comline = (h == HST_COM) ? 1 : 0;
#ifndef	_NOORIGSHELL
	if (promptstr == promptstr2) comline = 0;
	lastofs2 = 0;
#endif
	ch = _inputstr(&input, size, ptr, comline, h);
	win_x = dupwin_x;
	win_y = dupwin_y;
	len = strlen(input);

#ifndef	_NOORIGSHELL
	if (dumbmode || shellmode) prompt = NULL;
	dumbmode = 0;
#endif
	if (!prompt || !*prompt) {
		if (ch != K_ESC) {
			if (ch == cc_intr) {
				Xcputs2("^C");
				ch = K_ESC;
			}
#ifndef	_NOPTY
			if (minline > 0
			&& ypos + ptr2line(vlen(input, len)) >= maxline - 1)
				scrollup();
			else
#endif
			Xcputnl();
		}
	}
	else {
		for (i = 0; i < WCMDLINE; i++) {
			if (ypos + i >= maxline) break;
			Xlocate(0, ypos + i);
			Xputterm(L_CLEAR);
		}
		Xlocate(win_x, win_y);
		Xtflush();
		if (maxcmdline) /*EMPTY*/;
		else if ((!lcmdline && ypos < L_CMDLINE)
		|| (lcmdline > 0 && ypos < lcmdline + maxline))
			rewritefile(1);
	}
	lcmdline = maxcmdline = 0;

	if (ch == K_ESC) {
		free(input);
		return(NULL);
	}

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
	|| !(cp = strchr2(s, '[')) || !(tmp = strchr2(cp, ']')))
		return(s);

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
	else if (shellmode) locate2(0, 0);
	else
#endif
	Xlocate(xpos, ypos);
#ifndef	_NOORIGSHELL
	if (dumbmode) /*EMPTY*/;
	else
#endif
	{
		Xputterm(L_CLEAR);
		Xputterm(T_STANDOUT);
	}
	len = kanjiputs2(mes, n_lastcolumn - YESNOSIZE, -1);
	win_x += len;
	Xcputs2(YESNOSTR);
	win_x += YESNOSIZE;
#ifndef	_NOORIGSHELL
	if (dumbmode) /*EMPTY*/;
	else
#endif
	Xputterm(END_STANDOUT);
	Xtflush();

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

	VA_START(args, fmt);
	len = vasprintf2(&buf, fmt, args);
	va_end(args);
	if (len < 0) error("malloc()");

	dupwin_x = win_x;
	dupwin_y = win_y;
	duperrno = errno;
	win_x = xpos = 0;
#ifndef	_NOORIGSHELL
	dumbmode = (dumbterm || dumbshell) ? !Xtermmode(-1) : 0;
	if (dumbmode || shellmode) lcmdline = -1;
#endif

	minline = 0;
	maxline = n_line;
	if (!lcmdline) ypos = L_MESLINE;
	else if (lcmdline > 0) ypos = lcmdline;
	else ypos = maxline - 1;

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
				Xputch2(YESNOSTR[win_x - xpos - len]);
				win_x++;
			}
		}
		else
#endif
		locate2(x, 0);
		Xtflush();
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
				maxline = n_line;
				if (!lcmdline) ypos = L_MESLINE;
				else if (lcmdline > 0) ypos = lcmdline;
				else ypos = maxline - 1;
				yesnomes(buf);
				break;
			default:
				break;
		}
		x = len + 1 + (1 - ret) * 2;
	} while (ch != K_CR);
	free(buf);

#ifndef	_NOORIGSHELL
	if (dumbmode) Xcputnl();
	else
#endif
	if (lcmdline) {
		locate2(x, 0);
		Xputch2((ret) ? 'Y' : 'N');
		Xcputnl();
	}
	else {
		locate2(0, 0);
		Xputterm(L_CLEAR);
	}
#ifndef	_NOORIGSHELL
	dumbmode = 0;
#endif
	Xtflush();
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
	int y, len, wastty, dupwin_x, dupwin_y;

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

	y = (lcmdline) ? lcmdline : L_MESLINE;
	Xlocate(0, y);
	Xputterm(L_CLEAR);
	Xputterm(T_STANDOUT);
	win_x = kanjiputs2(s, n_lastcolumn, -1);
	win_y = y;
	Xputterm(END_STANDOUT);
	Xtflush();

	if (win_x >= n_lastcolumn) win_x = n_lastcolumn - 1;

	if (!(wastty = isttyiomode)) Xttyiomode(1);
	keyflush();
	do {
		getkey2(sigalrm(1));
	} while (kbhit2(WAITAFTERWARN * 1000L));
	if (!wastty) Xstdiomode();

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;

	if (no && !lcmdline) rewritefile(1);
	else {
		Xlocate(0, y);
		Xputterm(L_CLEAR);
		Xlocate(win_x, win_y);
		Xtflush();
	}

	if (tmp) free(tmp);
	hideclock = lcmdline = 0;
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

static VOID NEAR selectmes(num, max, x, str, val, xx, multi)
int num, max, x;
char *str[];
int val[], xx[], multi;
{
	int i;

	Xlocate(x, L_MESLINE);
	Xputterm(L_CLEAR);
	for (i = 0; i < max; i++) {
		if (!str[i]) continue;
		Xlocate(x + xx[i] + 1, L_MESLINE);
		if (multi) Xputch2((val[i]) ? '*' : ' ');
		if (i != num) Xkanjiputs(str[i]);
		else {
			Xputterm(T_STANDOUT);
			Xkanjiputs(str[i]);
			Xputterm(END_STANDOUT);
		}
	}
}

int selectstr(num, max, x, str, val)
int *num, max, x;
char *str[];
int val[];
{
	char **tmpstr;
	int i, ch, old, new, multi, tmpx, dupwin_x, dupwin_y, *xx, *initial;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	Xgetkey(-1, 0);

	xx = (int *)malloc2((max + 1) * sizeof(int));
	initial = (int *)malloc2(max * sizeof(int));
	tmpstr = (char **)malloc2(max * sizeof(char *));

	new = 0;
	multi = (num) ? 0 : 1;
	for (i = 0; i < max; i++) {
		tmpstr[i] = NULL;
		initial[i] = (str[i] && isupper2(*(str[i]))) ? *str[i] : -1;
		if (num && val[i] == *num) new = i;
	}
	tmpx = selectadj(max, x, str, tmpstr, xx, multi);
	selectmes(new, max, tmpx, tmpstr, val, xx, multi);

	win_y = L_MESLINE;
	do {
		win_x = tmpx + xx[new + 1];
		Xlocate(win_x, win_y);
		Xtflush();
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
					str, tmpstr, xx, multi);
				selectmes(new, max, tmpx,
					tmpstr, val, xx, multi);
				break;
			case ' ':
				if (num) break;
				val[new] = (val[new]) ? 0 : 1;
				Xlocate(tmpx + xx[new] + 1, L_MESLINE);
				Xputch2((val[new]) ? '*' : ' ');
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
				Xlocate(tmpx + xx[new] + 1, L_MESLINE);
				Xputch2((val[new]) ? '*' : ' ');
				break;
		}
		if (new != old) {
			i = tmpx + 1;
			if (!num) i++;
			Xlocate(i + xx[new], L_MESLINE);
			Xputterm(T_STANDOUT);
			Xkanjiputs(tmpstr[new]);
			Xputterm(END_STANDOUT);
			Xlocate(i + xx[old], L_MESLINE);
			if (stable_standout) Xputterm(END_STANDOUT);
			else Xkanjiputs(tmpstr[old]);
		}
	} while (ch != K_ESC && ch != K_CR && ch != cc_intr);

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;
	Xgetkey(-1, 0);

	if (stable_standout) {
		Xlocate(tmpx + 1, L_MESLINE);
		Xputterm(L_CLEAR);
	}
	if (num) {
		if (ch != K_CR) new = -1;
		else *num = val[new];
		for (i = 0; i < max; i++) {
			if (!tmpstr[i]) continue;
			Xlocate(tmpx + xx[i] + 1, L_MESLINE);
			if (i == new) Xkanjiputs(tmpstr[i]);
			else Xcprintf2("%*s", strlen2(tmpstr[i]), "");
		}
	}
	else {
		if (ch != K_CR) for (i = 0; i < max; i++) val[i] = 0;
		else {
			for (i = 0; i < max; i++) if (val[i]) break;
			if (i >= max) ch = K_ESC;
		}
		for (i = 0; i < max; i++) {
			if (!tmpstr[i]) continue;
			Xlocate(tmpx + xx[i] + 1, L_MESLINE);
			Xputch2(' ');
			if (val[i]) Xkanjiputs(tmpstr[i]);
			else Xcprintf2("%*s", strlen2(tmpstr[i]), "");
		}
	}
	Xlocate(win_x, win_y);
	Xtflush();
	free(xx);
	free(initial);
	for (i = 0; i < max; i++) if (tmpstr[i]) free(tmpstr[i]);
	free(tmpstr);

	return(ch);
}
