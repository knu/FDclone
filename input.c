/*
 *	input.c
 *
 *	input module
 */

#include "fd.h"
#include "wait.h"
#include "parse.h"
#include "kconv.h"
#include "func.h"
#include "kanji.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	DEP_PTY
#include "termemu.h"
#endif

#define	LIMITSELECTWARN		100
#define	YESNOSTR		"[Y/N]"
#define	YESNOSIZE		strsize(YESNOSTR)
#define	WAITAFTERWARN		360	/* msec */
#define	maxscr()		(maxcol * (maxline - minline) \
				- plen - (n_column - n_lastcolumn))
#define	within(n)		((n) < maxscr())
#define	overflow(n)		((n) > maxscr())
#define	ptr2col(n)		(((n) + plen) % maxcol)
#define	ptr2line(n)		(((n) + plen) / maxcol)
#define	iseol(n)		(within(n) && !(ptr2col(n)))
#define	LEFTMARGIN		0
#define	RIGHTMARGIN		2

extern char **history[];
extern short histsize[];
extern int curcolumns;
extern int minfilename;
extern int fnameofs;
extern int hideclock;
extern CONST char *promptstr;
#ifdef	DEP_ORIGSHELL
extern int fdmode;
extern CONST char *promptstr2;
#endif
#ifdef	DEP_IME
extern int ime_cont;
extern int ime_line;
extern int *ime_xposp;
extern VOID (*ime_locate)__P_((int, int));
#endif

#ifdef	DEP_KCONV
static int NEAR getinkcode __P_((VOID_A));
#else
#define	getinkcode()		NOCNV
#endif
#ifndef	_NOEDITMODE
static int NEAR getemulatekey __P_((int, CONST char []));
#endif
#ifdef	DEP_IME
static int NEAR getimebuf __P_((CONST char *, int *));
static int NEAR getime __P_((int, int *, int));
#endif
#ifdef	DEP_KCONV
static int NEAR getkey4 __P_((int, int));
#else
#define	getkey4(s, t)		getkey3(s, getinkcode(), t)
#endif
static int NEAR trquoteone __P_((char **, CONST char *, int *, int));
static char *NEAR trquote __P_((CONST char *, int, int *));
static int NEAR vlen __P_((CONST char *, int));
static int NEAR rlen __P_((CONST char *, int));
static int NEAR vonkanji1 __P_((CONST char *, int));
#if	FD >= 2
static VOID NEAR kanjiputs3 __P_((CONST char *, int, int, int, int));
#else
#define	kanjiputs3(s,n,l,p,m)	VOID_C kanjiputs2(s, l, p)
#endif
static VOID NEAR putcursor __P_((int, int));
static VOID NEAR rightcursor __P_((VOID_A));
static VOID NEAR leftcursor __P_((VOID_A));
static VOID NEAR upcursor __P_((VOID_A));
static VOID NEAR downcursor __P_((VOID_A));
#ifdef	DEP_ORIGSHELL
static VOID NEAR forwcursor __P_((int));
static VOID NEAR backcursor __P_((int));
static VOID NEAR forwline __P_((int));
static VOID NEAR dumbputs __P_((CONST char *, int, int, int, int));
static VOID NEAR rewritecursor __P_((int, int));
static int NEAR checkcursor __P_((int, int));
#endif
static VOID NEAR scrollup __P_((VOID_A));
static VOID NEAR locate2 __P_((int, int));
#ifdef	DEP_IME
static VOID locate3 __P_((int, int));
#endif
static VOID NEAR setcursor __P_((int, int));
static VOID NEAR putstr __P_((int *, int *, int));
static VOID NEAR ringbell __P_((VOID_A));
static VOID NEAR clearline __P_((VOID_A));
static VOID NEAR newline __P_((int));
static VOID NEAR rightchar __P_((VOID_A));
static VOID NEAR leftchar __P_((VOID_A));
static VOID NEAR insertchar __P_((int, int, int));
static VOID NEAR deletechar __P_((int, int, int));
static VOID NEAR insshift __P_((int, int));
static VOID NEAR delshift __P_((int, int));
static VOID NEAR truncline __P_((VOID_A));
static VOID NEAR displaystr __P_((VOID_A));
static VOID NEAR insertbuf __P_((int));
static int NEAR preparestr __P_((int, int));
static int NEAR insertcursor __P_((int *, int *, int, int));
static int NEAR quotemeta __P_((int *, int *, int, int *, int *, int *));
static int NEAR insertstr __P_((CONST char *, int, int, int *, int *));
#ifndef	_NOCOMPLETE
static VOID NEAR selectfile __P_((int, char *CONST *));
static int NEAR completestr __P_((int, int, int));
#endif
#ifdef	DEP_KCONV
static u_int NEAR getucs2 __P_((int));
static VOID NEAR ungetch3 __P_((int));
#endif
static int NEAR getch3 __P_((VOID_A));
static int NEAR getkanjikey __P_((char *, int));
static VOID NEAR copyhist __P_((CONST char *, int));
static VOID NEAR _inputstr_up __P_((int *, int, char **));
static VOID NEAR _inputstr_down __P_((int *, int, char **));
static VOID NEAR _inputstr_delete __P_((VOID_A));
static VOID NEAR _inputstr_enter __P_((VOID_A));
#if	FD >= 2
static VOID NEAR _inputstr_case __P_((int));
static int NEAR search_matchlen __P_((VOID_A));
static char *NEAR search_up __P_((int, int *, int, char **));
static char *NEAR search_down __P_((int, int *, int, char **));
#endif
static VOID NEAR _inputstr_input __P_((CONST char *, int));
static int NEAR _inputstr __P_((int, int, int));
static VOID NEAR dispprompt __P_((CONST char *, int));
static VOID NEAR truncstr __P_((CONST char *));
static int NEAR yesnomes __P_((CONST char *));
static int NEAR selectcnt __P_((int, char *CONST *, int));
static int NEAR selectadj __P_((int, int, CONST char *CONST *,
		char **, int *, int));
static VOID NEAR selectmes __P_((int, int, int,
		char *CONST [], int [], int *, int));

int subwindow = 0;
int win_x = 0;
int win_y = 0;
char *curfilename = NULL;
#ifndef	_NOEDITMODE
char *editmode = NULL;
#endif
int lcmdline = 0;
int maxcmdline = 0;
#ifdef	DEP_ORIGSHELL
int dumbshell = 0;
#endif
#ifdef	DEP_IME
int imekey = -1;
#endif
#ifdef	DEP_URLPATH
int hidepasswd = 0;
#endif

#ifndef	_NOEDITMODE
static CONST int emulatekey[] = {
	K_UP, K_DOWN, K_RIGHT, K_LEFT,
	K_IC, K_DC, K_IL, K_DL,
	K_HOME, K_END, K_BEG, K_EOL,
	K_PPAGE, K_NPAGE, K_ENTER, K_ESC
};
#define	EMULATEKEYSIZ		arraysize(emulatekey)
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
#define	VI_NEXT			001
#define	VI_INSERT		002
#define	VI_ONCE			004
#define	VI_VIMODE		010
#define	isvimode()		((vistat & (VI_VIMODE | VI_INSERT)) \
				== VI_VIMODE)
#endif	/* !_NOEDITMODE */
static int plen = 0;
static int maxcol = 0;
static int minline = 0;
static int maxline = 0;
static char *inputbuf = NULL;
static ALLOC_T inputsize = (ALLOC_T)0;
static int rptr = 0;
static int vptr = 0;
static int inputlen = 0;
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
#ifdef	DEP_ORIGSHELL
static int dumbmode = 0;
static int lastofs2 = 0;
#endif
#if	FD >= 2
static int searchmode = 0;
static char *searchstr = NULL;
#endif
#ifdef	DEP_IME
static int imemode = 0;
#endif
#ifdef	DEP_KCONV
static u_char ungetbuf3[MAXUTF8LEN * MAXNFLEN];
static int ungetnum3 = 0;
#endif


#ifdef	DEP_KCONV
static int NEAR getinkcode(VOID_A)
{
	int code;

	code = (inputkcode != NOCNV) ? inputkcode : DEFCODE;
# ifdef	DEP_PTY
	if (parentfd >= 0 && ptyinkcode != NOCNV) code = ptyinkcode;
# endif

	return(code);
}
#endif	/* DEP_KCONV */

int intrkey(key)
int key;
{
	int c, duperrno;

	duperrno = errno;
	if (!kbhit2(0L)) c = EOF;
	else if ((c = getch2()) == EOF) /*EMPTY*/;
	else if (c != K_ESC) /*EMPTY*/;
	else if (kbhit2(WAITKEYPAD * 1000L)) {
		ungetkey2(c, 0);
		c = EOF;
	}

	if (c == EOF) /*EMPTY*/;
	else if (c == cc_intr || (key >= 0 && c == key)) {
		if (isttyiomode) warning(0, INTR_K);
		else fprintf2(Xstderr, "%k\n", INTR_K);
	}
	else {
		ungetkey2(c, 0);
		c = EOF;
	}
	errno = duperrno;

	return((c == EOF) ? 0 : 1);
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
#endif	/* !_NOEDITMODE */

#ifdef	DEP_IME
static int NEAR getimebuf(buf, ptrp)
CONST char *buf;
int *ptrp;
{
	int c;

	if (getinkcode() != EUC || !isekana(buf, *ptrp))
		c = (u_char)(buf[*ptrp]);
	else c = mkekana((u_char)(buf[++(*ptrp)]));
	(*ptrp)++;

	return(c);
}

static int NEAR getime(sig, chp, nowait)
int sig, *chp, nowait;
{
	static char buf[MAXKANJIBUF + 1] = "";
	static int next = 0;
	static int lastline = 0;
	int x, y, line;

	if (!ime_cont) next = lastline = 0;
	else if (next > 0) {
		if (next >= sizeof(buf) || !(*chp = getimebuf(buf, &next)))
			next = 0;
		else return(0);
	}

	if (nowait) return(-1);

	line = ptr2line(inputlen) + 1;
	if (line < lastline) line = lastline;
	ime_locate = locate3;
	ime_line = lastline = line;
	x = win_x - xpos;
	y = win_y - ypos;
# ifdef	DEP_ORIGSHELL
	if (shellmode) {
		ime_xposp = &win_x;
		x += xpos;
		y += ypos;
	}
# endif
	locate2(0, line);
	setcursor(inputlen, -1);
	Xputterm(L_CLEAR);
# ifdef	DEP_ORIGSHELL
	if (shellmode) /*EMPTY*/;
	else
# endif
	ime_line += ypos;
	locate2(x, y);

	*chp = ime_inputkanji(sig, buf);
	if (*chp < 0 || !ime_cont) {
		locate2(0, line);
		Xputterm(L_CLEAR);
		locate2(x, y);
		Xtflush();
	}
	if (*chp != K_ESC) {
		if (*chp < 0) {
			next = lastline = 0;
			ime_inputkanji(0, NULL);
		}
		else if (!*chp) *chp = getimebuf(buf, &next);
		return(0);
	}

	imemode = lastline = 0;
	return(-1);
}
#endif	/* DEP_IME */

#ifdef	DEP_KCONV
static int NEAR getkey4(sig, timeout)
int sig, timeout;
{
	int n;

	for (n = 0; n < ungetnum3; n++) ungetkey2((int)ungetbuf3[n], 0);
# ifdef	DEP_IME
	if (imemode && !ungetnum3 && getime(sig, &n, 0) >= 0) /*EMPTY*/;
	else
# endif
	n = getkey3(sig, getinkcode(), timeout);

	ungetnum3 = 0;
	return(n);
}
#endif	/* DEP_KCONV */

int Xgetkey(sig, eof, timeout)
int sig, eof, timeout;
{
	static int prev = -1;
	int ch;

	if (sig < 0) {
#ifndef	_NOEDITMODE
		vistat = eof;
#endif
#ifdef	DEP_IME
		imemode = 0;
		ime_inputkanji(0, NULL);
#endif
		prev = -1;
		return('\0');
	}

	sig = sigalrm(sig);
	ch = getkey4(sig, timeout);
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
	} while ((vistat & VI_NEXT) && (ch = getkey4(sig, timeout)) > 0);
	else if (!strcmp(editmode, "wordstar"))
		ch = getemulatekey(ch, wordstarkey);
#endif	/* !_NOEDITMODE */

	return((ch >= 0) ? ch : K_ESC);
}

static int NEAR trquoteone(sp, s, cxp, len)
char **sp;
CONST char *s;
int *cxp, len;
{
	int vw;

	vw = 1;
	if (*cxp + 1 < len && iskanji1(s, *cxp)) {
		*((*sp)++) = s[(*cxp)++];
		vw++;
	}
#ifdef	CODEEUC
	else if (*cxp + 1 < len && isekana(s, *cxp)) *((*sp)++) = s[(*cxp)++];
#else
	else if (isskana(s, *cxp)) /*EMPTY*/;
#endif
	else if (iscntrl2(s[*cxp])) {
		*((*sp)++) = '^';
		*((*sp)++) = (s[(*cxp)++] + '@') & 0x7f;
		return(2);
	}
	else if (ismsb(s[*cxp])) {
		*sp += snprintf2(*sp, 4 + 1, "\\%03o", s[(*cxp)++] & 0xff);
		return(4);
	}

	*((*sp)++) = s[(*cxp)++];
	return(vw);
}

static char *NEAR trquote(s, len, widthp)
CONST char *s;
int len, *widthp;
{
	char *cp, *buf;
	int cx, vw;

	cp = buf = malloc2(len * 4 + 1);
	cx = vw = 0;
	while (cx < len) vw += trquoteone(&cp, s, &cx, len);
	*cp = '\0';
	if (widthp) *widthp = vw;

	return(buf);
}

static int NEAR vlen(s, cx)
CONST char *s;
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
CONST char *s;
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
CONST char *s;
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
CONST char *s;
int len, ptr;
{
	char *buf;
	int n;

	if (len < 0) return(0);
	buf = malloc2(len * KANAWID + 1);
	strncpy3(buf, s, &len, ptr);
#ifdef	DEP_KCONV
	n = Xkanjiputs(buf);
#else
	Xcputs2(buf);
	n = strlen2(buf);
#endif
	free2(buf);

	return(n);
}

#if	FD >= 2
static VOID NEAR kanjiputs3(s, cx2, len2, ptr, top)
CONST char *s;
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

VOID cputspace(n)
int n;
{
	Xcprintf2("%*s", n, nullstr);
}

VOID cputstr(n, s)
int n;
CONST char *s;
{
	if (!s) cputspace(n);
	else Xcprintf2("%-*.*k", n, n, s);
}

static VOID NEAR putcursor(c, n)
int c, n;
{
	win_x += n;
	while (n--) Xputch2(c);
#ifdef	DEP_ORIGSHELL
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

#ifdef	DEP_ORIGSHELL
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
CONST char *s;
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

static VOID NEAR rewritecursor(cx, cx2)
int cx, cx2;
{
	char *dupl;
	int i, dx, ocx, ocx2;

	i = (lastofs2) ? 1 : plen;
	dx = cx2 - lastofs2;
	ocx2 = win_x - xpos - i + lastofs2;
	if (checkcursor(cx, cx2) < 0) /*EMPTY*/;
	else if (cx2 <= ocx2) backcursor(xpos + i + dx);
	else {
		ocx = rlen(inputbuf, ocx2);
		dupl = trquote(&(inputbuf[ocx]), cx - ocx, NULL);
		win_x += Xkanjiputs(dupl);
		free2(dupl);
	}
}

static int NEAR checkcursor(cx, cx2)
int cx, cx2;
{
	int i, dx, orptr, ovptr;

	i = (lastofs2) ? 1 : plen;
	dx = cx2 - lastofs2;
	if (dx >= 0 && i + dx < maxcol) return(0);

	orptr = rptr;
	ovptr = vptr;
	rptr = cx;
	vptr = cx2;
	displaystr();
	rptr = orptr;
	vptr = ovptr;

	return(-1);
}
#endif	/* DEP_ORIGSHELL */

static VOID NEAR scrollup(VOID_A)
{
#ifdef	DEP_PTY
	regionscroll(C_SCROLLFORW, 1, 0, maxline - 1, minline, maxline - 1);
#else
	Xlocate(0, maxline - 1);
	Xputterms(C_SCROLLFORW);
#endif
	ypos--;
	hideclock = 1;
}

static VOID NEAR locate2(x, y)
int x, y;
{
#ifdef	DEP_ORIGSHELL
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
#endif	/* DEP_ORIGSHELL */
	while (ypos + y >= maxline) scrollup();
	win_x = xpos + x;
	win_y = ypos + y;
	Xlocate(win_x, win_y);
}

#ifdef	DEP_IME
static VOID locate3(x, y)
int x, y;
{
# ifdef	DEP_ORIGSHELL
	if (shellmode) locate2(x, y);
	else
# endif
	Xlocate(win_x = x, win_y = y);
}
#endif	/* DEP_IME */

static VOID NEAR setcursor(cx, cx2)
int cx, cx2;
{
	int f;

	if (cx < 0) cx = rlen(inputbuf, cx2);
	if (cx2 < 0) cx2 = vlen(inputbuf, cx);
#ifdef	DEP_ORIGSHELL
	if (dumbmode) {
		rewritecursor(cx, cx2);
		return;
	}
#endif
	f = within(cx2) ? 0 : 1;
	cx2 -= f;
	locate2(ptr2col(cx2), ptr2line(cx2));
	if (f) rightcursor();
}

static VOID NEAR putstr(cxp, cxp2, ins)
int *cxp, *cxp2, ins;
{
	char *cp, tmp[4 + 1];
	int cx, vw;

	while (ins > 0) {
		cp = tmp;
		cx = *cxp;
		VOID_C trquoteone(&cp, inputbuf, &cx, inputlen);
		*cp = '\0';
		cp = tmp;
		while (*cp) {
			vw = (iskanji1(cp, 0)) ? 2 : 1;
			Xcprintf2("%.*k", vw, cp);
			cp += vw;
			win_x += vw;
			*cxp2 += vw;
#ifdef	DEP_ORIGSHELL
			if (dumbmode) VOID_C checkcursor(cx, *cxp2);
			else
#endif
			if (within(*cxp2) && ptr2col(*cxp2) < vw)
				setcursor(cx, *cxp2);
		}
		ins -= cx - *cxp;
		*cxp = cx;
	}
}

static VOID NEAR ringbell(VOID_A)
{
#ifdef	DEP_ORIGSHELL
	if (dumbmode && dumbterm <= 2) Xputch2('\007');
	else
#endif
	Xputterm(T_BELL);
	Xtflush();
}

static VOID NEAR clearline(VOID_A)
{
#ifdef	DEP_ORIGSHELL
	int x;
#endif

#ifdef	DEP_ORIGSHELL
	if (dumbmode) {
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
#ifdef	DEP_ORIGSHELL
	if (dumbmode);
	else if (shellmode) forwline(xpos);
	else
#endif
	locate2(0, y);
}

static VOID NEAR rightchar(VOID_A)
{
	int rw, vw;

	if (rptr + 1 < inputlen && iskanji1(inputbuf, rptr)) rw = vw = 2;
#ifdef	CODEEUC
	else if (rptr + 1 < inputlen && isekana(inputbuf, rptr)) {
		rw = 2;
		vw = 1;
	}
#else
	else if (isskana(inputbuf, rptr)) rw = vw = 1;
#endif
	else if (iscntrl2(inputbuf[rptr])) {
		rw = 1;
		vw = 2;
	}
	else if (ismsb(inputbuf[rptr])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	if (rptr + rw > inputlen) {
		ringbell();
		return;
	}
	rptr += rw;
	vptr += vw;
#ifdef	DEP_ORIGSHELL
	if (dumbmode) rewritecursor(rptr, vptr);
	else
#endif
	if (within(vptr) && ptr2col(vptr) < vw) setcursor(rptr, vptr);
	else while (vw--) rightcursor();
}

static VOID NEAR leftchar(VOID_A)
{
	int rw, vw, ovptr;

	if (rptr >= 2 && onkanji1(inputbuf, rptr - 2)) rw = vw = 2;
#ifdef	CODEEUC
	else if (rptr >= 2 && isekana(inputbuf, rptr - 2)) {
		rw = 2;
		vw = 1;
	}
#else
	else if (isskana(inputbuf, rptr - 1)) rw = vw = 1;
#endif
	else if (iscntrl2(inputbuf[rptr - 1])) {
		rw = 1;
		vw = 2;
	}
	else if (ismsb(inputbuf[rptr - 1])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	ovptr = vptr;
	rptr -= rw;
	vptr -= vw;
#ifdef	DEP_ORIGSHELL
	if (dumbmode) rewritecursor(rptr, vptr);
	else
#endif
	if (ptr2col(ovptr) < vw) setcursor(rptr, vptr);
	else while (vw--) leftcursor();
}

static VOID NEAR insertchar(cx, cx2, ins)
int cx, cx2, ins;
{
	char *dupl;
	int dx, dy, i, j, l, f1, ptr, len2, nlen;
#if	!MSDOS
	int f2;
#endif

	dupl = trquote(&(inputbuf[cx]), inputlen - cx, &l);
	len2 = cx2 + l;
	nlen = len2 + ins;
	dy = ptr2line(cx2);
	i = (dy + 1) * maxcol - plen;	/* prev. chars including cursor line */
	j = maxcol - ptr2col(cx2);
	if (j > ins) j = ins;		/* inserted columns in cursor line */

#ifdef	DEP_ORIGSHELL
	if (dumbmode) {
		i = (lastofs2) ? 1 : plen;
		dx = cx2 + ins - lastofs2;
		if (i + dx < maxcol) {
			putcursor(' ', ins);
			dumbputs(dupl, i + cx2, l, maxcol - (i + dx), 0);
		}
	}
	else
#endif	/* DEP_ORIGSHELL */
#if	!MSDOS
	if (*termstr[C_INSERT]) {
# ifdef	DEP_ORIGSHELL
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
			setcursor(cx, cx2);
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
# ifdef	DEP_ORIGSHELL
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
			setcursor(cx, cx2);
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
#ifdef	DEP_ORIGSHELL
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
		setcursor(cx, cx2);
	}
#ifdef	DEP_ORIGSHELL
	if (dumbmode);
	else if (shellmode) {
		if (ptr2col(nlen) == 1) {
			setcursor(-1, nlen);
			Xputterm(L_CLEAR);
			setcursor(cx, cx2);
		}
	}
	else
#endif
	if (i == nlen && within(i) && ypos + dy >= maxline - 1) {
		while (ypos + dy >= maxline - 1) scrollup();
		setcursor(cx, cx2);
	}
	free2(dupl);
}

static VOID NEAR deletechar(cx, cx2, del)
int cx, cx2, del;
{
	char *dupl;
	int dy, i, j, l, f1, ptr, len2, nlen;
#if	!MSDOS
	int f2;
#endif

	dupl = trquote(&(inputbuf[cx]), inputlen - cx, &l);
	len2 = cx2 + l;
	nlen = len2 - del;
	dy = ptr2line(cx2);
	i = (dy + 1) * maxcol - plen;	/* prev. chars including cursor line */

#ifdef	DEP_ORIGSHELL
	if (dumbmode) {
		i = (lastofs2) ? 1 : plen;
		j = cx2 - lastofs2;
		dumbputs(dupl, i + cx2, l, maxcol - (i + j), del);
	}
	else
#endif	/* DEP_ORIGSHELL */
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
			setcursor(cx, cx2);
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
		setcursor(cx, cx2);
	}
	free2(dupl);
}

static VOID NEAR insshift(cx, ins)
int cx, ins;
{
	if (cx >= inputlen) return;
	memmove(&(inputbuf[cx + ins]), &(inputbuf[cx]), inputlen - cx);
}

static VOID NEAR delshift(cx, del)
int cx, del;
{
	if (cx >= inputlen - del) return;
	memmove(&(inputbuf[cx]), &(inputbuf[cx + del]), inputlen - del - cx);
}

static VOID NEAR truncline(VOID_A)
{
	int dy, i, len2;

	len2 = vlen(inputbuf, inputlen);
#ifdef	DEP_ORIGSHELL
	if (dumbmode) {
		clearline();
		return;
	}
#endif
	Xputterm(L_CLEAR);

	dy = ptr2line(vptr);
	i = (dy + 1) * maxcol - plen;
	if (i < len2) {
#ifdef	DEP_ORIGSHELL
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
		setcursor(rptr, vptr);
	}
}

static VOID NEAR displaystr(VOID_A)
{
#if	FD >= 2
	int top;
#endif
	char *dupl;
	int i, x, y, len2, max, vi, width, f;

	dupl = trquote(inputbuf, inputlen, &len2);
#if	FD >= 2
	top = vlen(inputbuf, search_matchlen());
#endif

#ifdef	DEP_ORIGSHELL
	if (dumbmode) {
		backcursor(0);
		forwcursor(xpos);
		dispprompt(NULL, -1);
		i = plen;
		x = vptr - maxcol;
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
		dumbputs(dupl, i + vptr, len2, maxcol - i, lastofs2);
		Xtflush();
		free2(dupl);
		return;
	}
#endif	/* DEP_ORIGSHELL */

	dispprompt(NULL, 0);
	locate2(plen, 0);
	max = maxscr();
	if (len2 > max) {
		len2 = max;
		inputlen = rlen(inputbuf, max);
		if (vptr >= len2) {
			vptr = len2;
			rptr = inputlen;
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

#ifdef	DEP_ORIGSHELL
		if (shellmode);
		else
#endif
		if (ypos + y >= maxline - 1) {
			while (ypos + y >= maxline - 1) scrollup();
			locate2(x, y);
		}
		if (width + f > 0) {
			kanjiputs3(dupl, vptr, width + f, i, top);
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
	kanjiputs3(dupl, vptr, len2 - i, i, top);
	win_x += len2 - i;
#ifdef	DEP_ORIGSHELL
	if (shellmode);
	else
#endif
	while (win_y < maxline - 1) {
		Xlocate(xpos, ++win_y);
		Xputterm(L_CLEAR);
	}
	if (vi) Xputterm(T_NORMALCURSOR);
	setcursor(rptr, vptr);
	Xtflush();
	free2(dupl);
}

static VOID NEAR insertbuf(ins)
int ins;
{
	inputbuf = c_realloc(inputbuf, inputlen + ins, &inputsize);
}

static int NEAR preparestr(rins, vins)
int rins, vins;
{
	int rw, vw;

	if (overflow(vlen(inputbuf, inputlen) + vins)) return(-1);

	if (!overwritemode) {
		insertchar(rptr, vptr, vins);
		insertbuf(rins);
		insshift(rptr, rins);
		inputlen += rins;
		return(0);
	}

	if (rptr >= inputlen) rw = vw = 0;
	else if (rptr + 1 < inputlen && iskanji1(inputbuf, rptr)) rw = vw = 2;
#ifdef	CODEEUC
	else if (rptr + 1 < inputlen && isekana(inputbuf, rptr)) {
		rw = 2;
		vw = 1;
	}
#else
	else if (isskana(inputbuf, rptr)) rw = vw = 1;
#endif
	else if (iscntrl2(inputbuf[rptr])) {
		rw = 1;
		vw = 2;
	}
	else if (ismsb(inputbuf[rptr])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	if (!vw) /*EMPTY*/;
	else if (vins > vw) insertchar(rptr, vptr, vins - vw);
	else if (vins < vw) deletechar(rptr, vptr, vw - vins);

	if (!rw) /*EMPTY*/;
	else if (rins > rw) {
		insertbuf(rins - rw);
		insshift(rptr, rins - rw);
	}
	else if (rins < rw) delshift(rptr, rw - rins);

	inputlen += rins - rw;

	return(0);
}

static int NEAR insertcursor(cxp, cxp2, ins, ch)
int *cxp, *cxp2, ins, ch;
{
	if (overflow(vlen(inputbuf, inputlen) + ins)) return(-1);
	setcursor(*cxp, *cxp2);
	insertchar(*cxp, *cxp2, ins);
	insertbuf(ins);
	insshift(*cxp, ins);
	inputlen += ins;
	if (ch) {
		inputbuf[*cxp] = ch;
		putstr(cxp, cxp2, 1);
	}

	return(0);
}

static int NEAR quotemeta(cxp, cxp2, ch, qtopp, qp, qedp)
int *cxp, *cxp2, ch, *qtopp, *qp, *qedp;
{
	int cx2, ocx, ocx2;

	ocx = *cxp;
	ocx2 = *cxp2;
	if (*qtopp < 0) return(0);
	if (*qp) {
		if (*qp != '\'' || ch != '\'') return(0);
		if (insertcursor(cxp, cxp2, 1, *qp) < 0) return(-1);
		*qtopp = *cxp;
	}
	else if (qedp && *qedp > 0 && *qedp < *cxp) {
		if (inputbuf[*qedp] != '\'' || ch != '\'') {
			*qp = inputbuf[*qedp];
			cx2 = vlen(inputbuf, *qedp);
			setcursor(*qedp, cx2);
			deletechar(*qedp, cx2, 1);
			delshift(*qedp, 1);
			(*cxp)--;
			(*cxp2)--;
			if (cxp != &rptr && rptr > *qedp) rptr--;
			if (cxp2 != &vptr && vptr > cx2) vptr--;
			inputlen--;
			*qedp = -1;
			return(0);
		}
		*qtopp = *cxp;
	}

	cx2 = vlen(inputbuf, *qtopp);
	if (insertcursor(qtopp, &cx2, 1, *qp = '"') < 0) return(-1);
	(*cxp)++;
	(*cxp2)++;
	if (cxp != &rptr && rptr >= *qtopp) rptr += *cxp - ocx;
	if (cxp2 != &vptr && vptr >= cx2) vptr += *cxp2 - ocx2;
	*qtopp = -1;
	if (qedp) *qedp = -1;
	return(0);
}

static int NEAR insertstr(strins, ins, qtop, qp, qedp)
CONST char *strins;
int ins, qtop, *qp, *qedp;
{
	int i, n, ch, pc, max;

	n = vlen(inputbuf, inputlen);
	max = maxscr();
	if (n + vlen(strins, ins) > max) ins = rlen(strins, max - n);

	if (ins > 0) {
		if (onkanji1(strins, ins - 1)) ins--;
#ifdef	CODEEUC
		else if (isekana(strins, ins - 1)) ins--;
#endif
	}
	if (ins <= 0) return(ins);

	insertchar(rptr, vptr, vlen(strins, ins));
	insertbuf(ins);
	insshift(rptr, ins);
	for (i = 0; i < ins; i++) inputbuf[rptr + i] = ' ';
	inputlen += ins;
	for (i = 0; i < ins; i++) {
		if (*qp < '\0') pc = PC_NORMAL;
		else pc = parsechar(&(strins[i]), ins - i,
			'!', EA_FINDMETA, qp, NULL);
		if (pc == PC_WCHAR) {
			inputbuf[rptr] = strins[i++];
			inputbuf[rptr + 1] = strins[i];
			putstr(&rptr, &vptr, 2);
		}
#ifndef	FAKEESCAPE
		else if (pc == PC_EXMETA || pc == '!') {
			if (*qp) {
				n = 2;
				ch = *qp;
				*qp = '\0';
			}
			else {
				n = 1;
				ch = '\0';
			}
			if (insertcursor(&rptr, &vptr, n, ch) < 0) return(0);
			inputbuf[rptr] = PESCAPE;
			inputbuf[rptr + 1] = strins[i];
			putstr(&rptr, &vptr, 2);
			qtop = rptr;
		}
#endif	/* !FAKEESCAPE */
		else if (pc == PC_META) {
			n = quotemeta(&rptr, &vptr,
				strins[i], &qtop, qp, qedp);
			if (n < -1) return(0);
			setcursor(rptr, vptr);
			if (*qp == '\'' || !strchr2(DQ_METACHAR, strins[i])) {
				inputbuf[rptr] = strins[i];
				putstr(&rptr, &vptr, 1);
			}
			else {
#ifdef	FAKEESCAPE
				ch = strins[i];
#else
				ch = PESCAPE;
#endif
				if (insertcursor(&rptr, &vptr, 1, ch) < 0)
					return(0);
				inputbuf[rptr] = strins[i];
				putstr(&rptr, &vptr, 1);
			}
		}
		else {
			inputbuf[rptr] = strins[i];
			putstr(&rptr, &vptr, 1);
		}
	}

	return(0);
}

#ifndef	_NOCOMPLETE
static VOID NEAR selectfile(argc, argv)
int argc;
char *CONST *argv;
{
	char *cp;
	int duprow, dupminfilename, dupcolumns, dupdispmode, dupfnameofs;
	int i, j, n, len, maxlen;

	duprow = FILEPERROW;
	dupminfilename = minfilename;
	dupcolumns = curcolumns;
	dupdispmode = dispmode;
	dupfnameofs = fnameofs;
	minfilename = n_column;
	dispmode = F_FILETYPE;
	fnameofs = 0;

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
					i = getkey3(0, getinkcode(), 0);
					if (i < K_MIN) {
						i = toupper2(i);
						if (i == 'Y' || i == 'N')
							break;
					}
					ringbell();
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
						cputstr(maxlen + 2,
							selectlist[j].name);
					Xcputnl();
				}
			}

# ifdef	DEP_ORIGSHELL
			if (dumbmode || shellmode) {
				win_x = xpos;
				win_y = 0;
			}
			else
# endif
			if (ypos + n < maxline - 1) ypos += n;
			else ypos = maxline - 1;
			for (i = 0; i < argc; i++) free2(selectlist[i].name);
			free2(selectlist);
			selectlist = NULL;
			FILEPERROW = duprow;
			minfilename = dupminfilename;
			curcolumns = dupcolumns;
			dispmode = dupdispmode;
			fnameofs = dupfnameofs;

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
		for (i = 0; i < maxselect; i++) free2(selectlist[i].name);
		free2(selectlist);
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
	fnameofs = dupfnameofs;
}

/*ARGSUSED*/
static int NEAR completestr(comline, cont, h)
int comline, cont, h;
{
# ifdef	DEP_ORIGSHELL
	int vartop;
# endif
	char *cp, *tmp, **argv;
	int i, i2, rw, n, ch, pc, ins, top, fix, argc, qtop, quote, quoted;

	if (selectlist && cont > 0) {
		selectfile(tmpfilepos++, NULL);
		setcursor(rptr, vptr);
		ringbell();
		return(-1);
	}

	quote = '\0';
# ifdef	DEP_ORIGSHELL
	vartop =
# endif
	top = 0;
# ifndef	NOUID
	if (h == HST_USER || h == HST_GROUP) quote = -1;
	else
# endif
	for (i = 0; i < rptr; i++) {
# ifdef	FAKEESCAPE
		pc = parsechar(&(inputbuf[i]), rptr - i,
			'$', EA_NOEVALQ, &quote, NULL);
# else
		pc = parsechar(&(inputbuf[i]), rptr - i,
			'$', EA_BACKQ, &quote, NULL);
# endif
		if (pc == PC_WCHAR) i++;
# ifdef	DEP_ORIGSHELL
		else if (pc == '$') vartop = i + 1;
# endif
# ifndef	FAKEESCAPE
		else if (pc == PC_ESCAPE) i++;
		else if (pc == PC_OPQUOTE) {
			if (inputbuf[i] == '`') top = i + 1;
		}
# endif
		else if (pc != PC_NORMAL) /*EMPTY*/;
# ifdef	DEP_DOSPATH
		else if (i == top && _dospath(&(inputbuf[i]))) i++;
# endif
# ifdef	DEP_URLPATH
		else if (i == top
		&& (n = _urlpath(&(inputbuf[i]), NULL, NULL)))
			i += n - 1;
# endif
		else if (inputbuf[i] == ':' || inputbuf[i] == '='
		|| strchr2(CMDLINE_DELIM, inputbuf[i]))
			top = i + 1;
	}
	if (comline && top > 0) {
		for (i = top - 1; i >= 0; i--)
			if (!isblank2(inputbuf[i])) break;
		if (i >= 0 && !strchr2(SHELL_OPERAND, inputbuf[i]))
			comline = 0;
	}
# ifdef	DEP_ORIGSHELL
	if (vartop) {
		i = top + 1;
		if (quote) i++;
		if (vartop != i
		|| (vartop < rptr && !isidentchar(inputbuf[vartop])))
			vartop = 0;
		else {
			for (i = vartop; i < rptr; i++)
				if (!isidentchar2(inputbuf[i])) break;
			if (i < rptr) vartop = 0;
			else top = vartop;
		}
	}
# endif	/* DEP_ORIGSHELL */

	cp = strndup2(&(inputbuf[top]), rptr - top);
# ifdef	DEP_ORIGSHELL
	if (vartop) /*EMPTY*/;
	else
# endif
# ifndef	NOUID
	if (h == HST_USER || h == HST_GROUP) /*EMPTY*/;
	else
# endif
	cp = evalpath(cp, 0);

	if (selectlist && cont < 0) {
		argv = (char **)malloc2(1 * sizeof(char *));
		n = strlen(selectlist[tmpfilepos].name);
		i = (s_isdir(&(selectlist[tmpfilepos]))) ? 1 : 0;
		argv[0] = (char *)malloc2(n + i + 1);
		memcpy(argv[0], selectlist[tmpfilepos].name, n);
		if (i) argv[0][n] = _SC_;
		argv[0][n + i] = '\0';
		argc = 1;
	}
# ifndef	NOUID
	else if (h == HST_USER) {
		argv = NULL;
		n = strlen(cp);
		argc = completeuser(cp, n, 0, &argv, 0);
	}
	else if (h == HST_GROUP) {
		argv = NULL;
		n = strlen(cp);
		argc = completegroup(cp, n, 0, &argv);
	}
# endif
# ifdef	DEP_ORIGSHELL
	else if (vartop) {
		argv = NULL;
		n = strlen(cp);
		argc = completeshellvar(cp, n, 0, &argv);
	}
# endif
	else {
		argv = NULL;
		argc = 0;
		n = strlen(cp);
		if (comline && !strdelim(cp, 1)) {
# ifdef	DEP_ORIGSHELL
			argc = completeshellcomm(cp, n, argc, &argv);
# else
			argc = completeuserfunc(cp, n, argc, &argv);
			argc = completealias(cp, n, argc, &argv);
			argc = completebuiltin(cp, n, argc, &argv);
			argc = completeinternal(cp, n, argc, &argv);
# endif
		}
# ifndef	_NOARCHIVE
		if (archivefile && !comline && *cp != _SC_)
			argc = completearch(cp, n, argc, &argv);
		else
# endif
		argc = completepath(cp, n, argc, &argv, comline);
		if (!argc && comline)
			argc = completepath(cp, n, argc, &argv, 0);
	}

	ins = strlen(getbasename(cp));
	free2(cp);
	if (!argc) {
		free2(argv);
		ringbell();
		return(-1);
	}

	cp = findcommon(argc, argv);
	if (argc != 1 || !cp) fix = '\0';
# ifndef	NOUID
	else if (h == HST_USER || h == HST_GROUP) fix = ' ';
# endif
	else fix = ((tmp = strrdelim(cp, 0)) && !tmp[1]) ? _SC_ : ' ';

	if (!cp || ((ins = (int)strlen(cp) - ins) <= 0 && fix != ' ')) {
		if (cont <= 0) ringbell();
		else {
			selectfile(argc, argv);
			if (lcmdline < 0) displaystr();
			setcursor(rptr, vptr);
		}
		for (i = 0; i < argc; i++) free2(argv[i]);
		free2(argv);
		free2(cp);
		return(-1);
	}
	for (i = 0; i < argc; i++) free2(argv[i]);
	free2(argv);

	qtop = top;
	if (!quote && inputbuf[top] == '~') {
		tmp = strchr2(&(inputbuf[top + 1]), _SC_);
		if (tmp) qtop = tmp - inputbuf + 1;
	}

	tmp = cp + (int)strlen(cp) - ins;
	if (fix == _SC_) ins--;
# ifndef	NOUID
	if (h == HST_USER || h == HST_GROUP) {
		VOID_C insertstr(tmp, ins, qtop, &quote, NULL);
		free2(cp);
		return(0);
	}
# endif

	quote = '\0';
	quoted = 0;
	i = top;
	i2 = vlen(inputbuf, top);
	while (i < rptr) {
		rw = 1;
		pc = parsechar(&(inputbuf[i]), rptr - i, '!', 0, &quote, NULL);
		if (pc == PC_WCHAR) rw = 2;
		else if (pc == PC_CLQUOTE) {
			quoted = i;
			qtop = i + 1;
		}
# ifndef	FAKEESCAPE
		else if (pc == PC_ESCAPE) {
			rw = 2;
			if (inputbuf[i + 1] == '!') qtop = i + 2;
			else if (strchr2(DQ_METACHAR, inputbuf[i + 1])) {
				n = quotemeta(&i, &i2,
					'\0', &qtop, &quote, &quoted);
				if (n < 0) {
					free2(cp);
					return(0);
				}
			}
			else if (inputbuf[i + 1] != '\'') {
				setcursor(i, i2);
				deletechar(i, i2, 1);
				delshift(i, 1);
				rptr--;
				vptr--;
				inputlen--;
				rw = 0;
			}
		}
# endif	/* !FAKEESCAPE */
		else if (pc == '!') {
			if (quote) {
				n = 2;
				ch = quote;
				quote = '\0';
			}
			else {
				n = 1;
				ch = '\0';
			}
			if (insertcursor(&i, &i2, n, ch) < 0) {
				free2(cp);
				return(0);
			}
			inputbuf[i] = PESCAPE;
			inputbuf[i + 1] = '!';
			putstr(&i, &i2, 2);
			rptr += n;
			vptr += n;
			qtop = i;
			rw = 0;
		}
		else if (pc == PC_OPQUOTE || pc == PC_SQUOTE) /*EMPTY*/;
		else if (strchr2(DQ_METACHAR, inputbuf[i])) {
			if (insertcursor(&i, &i2, 1, PESCAPE) < 0) {
				free2(cp);
				return(0);
			}
			rptr++;
			vptr++;
			qtop = i + 1;
		}
		else if (pc != PC_NORMAL) /*EMPTY*/;
		else if (strchr2(METACHAR, inputbuf[i])) {
			n = quotemeta(&i, &i2,
				inputbuf[i], &qtop, &quote, &quoted);
			if (n < 0) {
				free2(cp);
				return(0);
			}
		}
		i += rw;
		i2 += vlen(&(inputbuf[i]), rw);
	}
	setcursor(rptr, vptr);

	n = insertstr(tmp, ins, qtop, &quote, &quoted);
	free2(cp);
	if (n < 0) return(0);
	if (fix) {
		if (quote > '\0' && (fix != _SC_ || quoted < 0)) {
			if (insertcursor(&rptr, &vptr, 1, quote) < 0)
				return(0);
		}
		if (insertcursor(&rptr, &vptr, 1, fix) < 0) return(0);
	}

	return(0);
}
#endif	/* !_NOCOMPLETE */

#ifdef	DEP_KCONV
static u_int NEAR getucs2(ch)
int ch;
{
	char buf[MAXUTF8LEN + 1];
	int n;

	n = 0;
	if (ch & 0xff00) return((u_int)-1);
	buf[n++] = ch;
	if (!ismsb(ch)) /*EMPTY*/;
	else if ((ch = getch3()) == EOF) return((u_int)-1);
	else {
		buf[n++] = ch;
		if (isutf2(buf[0], buf[1])) /*EMPTY*/;
		else if ((ch = getch3()) == EOF) {
			ungetch3(buf[1]);
			return((u_int)-1);
		}
		else buf[n++] = ch;
	}
	buf[n] = '\0';

	return(ucs2fromutf8((u_char *)buf, NULL));
}

static VOID NEAR ungetch3(c)
int c;
{
	if (ungetnum3 >= arraysize(ungetbuf3)) return;
	memmove((char *)&(ungetbuf3[1]), (char *)&(ungetbuf3[0]),
		ungetnum3 * sizeof(u_char));
	ungetbuf3[0] = c;
	ungetnum3++;
}
#endif	/* DEP_KCONV */

static int NEAR getch3(VOID_A)
{
#ifdef	DEP_IME
	int c;
#endif

#ifdef	DEP_KCONV
	if (ungetnum3 > 0) return((int)ungetbuf3[--ungetnum3]);
#endif
#ifdef	DEP_IME
	if (imemode) return((getime(0, &c, 1) >= 0) ? c : EOF);
#endif
	if (!kbhit2(WAITKANJI * 1000L)) return(EOF);
	return(getch2());
}

static int NEAR getkanjikey(buf, ch)
char *buf;
int ch;
{
#ifdef	DEP_KCONV
	u_short ubuf[MAXNFLEN];
	char tmp[MAXUTF8LEN + 1];
	u_int u;
	int i, n, len, code;
#endif
	int ch2;

#ifdef	DEP_KCONV
	code = getinkcode();
	if (code == EUC && isekana2(ch)) {
		tmp[0] = (char)C_EKANA;
		tmp[1] = (ch & 0xff);
		tmp[2] = '\0';
		kanjiconv(buf, tmp, MAXKLEN, code, DEFCODE, L_INPUT);
		return(1);
	}
	if (code == SJIS && iskana2(ch)) {
		tmp[0] = ch;
		tmp[1] = '\0';
		kanjiconv(buf, tmp, MAXKLEN, code, DEFCODE, L_INPUT);
		return(1);
	}
	if (code >= UTF8) {
		i = 0;
		for (;;) {
			if ((u = getucs2(ch)) == (u_int)-1) {
				if (i) ungetch3(ch);
				buf[0] = '\0';
				return(-1);
			}
			ubuf[i++] = u;
			if (i >= MAXNFLEN) break;
			if (code != M_UTF8 || (ch = getch3()) == EOF) break;
		}
		len = i;
		i = 0;
		if (!len) u = ch;
		else if (code != M_UTF8) u = ubuf[i++];
		else u = ucs2denormalization(ubuf, &i, code - UTF8);

		while (i < len) {
			tmp[ucs2toutf8(tmp, 0, ubuf[i++])] = '\0';
			for (n = 0; tmp[n]; n++) ungetch3(tmp[n]);
		}
		tmp[ucs2toutf8(tmp, 0, u)] = '\0';
		n = kanjiconv(buf, tmp, MAXKLEN, code, DEFCODE, L_INPUT);
# ifdef	CODEEUC
		if (isekana(buf, 0)) n = 1;
# endif
		return(n);
	}
#else	/* !DEP_KCONV */
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
#endif	/* !DEP_KCONV */

	if (isinkanji1(ch, code)) {
		if ((ch2 = getch3()) == EOF || !isinkanji2(ch2, code)) {
			buf[0] = '\0';
			return(-1);
		}
#ifdef	DEP_KCONV
		tmp[0] = ch;
		tmp[1] = ch2;
		tmp[2] = '\0';
		kanjiconv(buf, tmp, MAXKLEN, code, DEFCODE, L_INPUT);
#else
		buf[0] = ch;
		buf[1] = ch2;
		buf[2] = '\0';
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

static VOID NEAR copyhist(hist, keep)
CONST char *hist;
int keep;
{
#ifdef	DEP_ORIGSHELL
	int y1, y2, len2;

	len2 = vlen(inputbuf, inputlen);
	y1 = ptr2line(len2);
	if (dumbmode) {
		setcursor(0, 0);
		clearline();
	}
#endif	/* DEP_ORIGSHELL */

	if (!keep) {
		inputlen = (hist) ? strlen(hist) : 0;
		insertbuf(0);
		if (hist) memcpy(inputbuf, hist, inputlen + 1);
		else *inputbuf = '\0';
		if (rptr < 0 || rptr > inputlen) {
			rptr = inputlen;
			vptr = vlen(inputbuf, rptr);
		}
	}
	displaystr();

#ifdef	DEP_ORIGSHELL
	if (dumbmode) /*EMPTY*/;
	else if (shellmode) {
		y2 = ptr2line(vlen(inputbuf, inputlen));
		if (y1 > y2) {
			while (y1 > y2) {
				locate2(0, y1--);
				Xputterm(L_CLEAR);
			}
			setcursor(rptr, vptr);
		}
	}
#endif	/* DEP_ORIGSHELL */
}

static VOID NEAR _inputstr_up(histnop, h, tmp)
int *histnop, h;
char **tmp;
{
	keyflush();
#ifndef	_NOCOMPLETE
	if (completable(h) && selectlist) {
		selectfile(tmpfilepos--, NULL);
		setcursor(rptr, vptr);
	}
	else
#endif
#ifdef	DEP_ORIGSHELL
	if (dumbmode || vptr < maxcol)
#else
	if (vptr < maxcol)
#endif
	{
		if (nohist(h) || !history[h] || *histnop >= (int)histsize[h]
		|| !history[h][*histnop]) {
			ringbell();
			return;
		}

		if (!*tmp) {
			inputbuf[inputlen] = '\0';
			*tmp = strdup2(inputbuf);
		}
		rptr = -1;
		copyhist(history[h][(*histnop)++], 0);
	}
	else {
		if (!within(vptr) && !ptr2col(vptr)) {
			leftcursor();
			vptr--;
		}
		vptr -= maxcol;
		rptr = rlen(inputbuf, vptr);
		upcursor();
		if (onkanji1(inputbuf, rptr - 1)) {
			leftcursor();
			vptr--;
			rptr--;
		}
	}
}

static VOID NEAR _inputstr_down(histnop, h, tmp)
int *histnop, h;
char **tmp;
{
	int len2;

	len2 = vlen(inputbuf, inputlen);
	keyflush();
#ifndef	_NOCOMPLETE
	if (completable(h) && selectlist) {
		selectfile(tmpfilepos++, NULL);
		setcursor(rptr, vptr);
	}
	else
#endif
#ifdef	DEP_ORIGSHELL
	if (dumbmode || (vptr + maxcol > len2 || (vptr + maxcol == len2
	&& !within(len2) && !ptr2col(len2))))
#else
	if (vptr + maxcol > len2 || (vptr + maxcol == len2
	&& !within(len2) && !ptr2col(len2)))
#endif
	{
		if (nohist(h) || !history[h] || *histnop <= 0) {
			ringbell();
			return;
		}

		rptr = -1;
		if (--(*histnop) > 0) copyhist(history[h][*histnop - 1], 0);
		else {
			copyhist(*tmp, 0);
			free2(*tmp);
			*tmp = NULL;
		}
	}
	else {
		vptr += maxcol;
		rptr = rlen(inputbuf, vptr);
		downcursor();
		if (onkanji1(inputbuf, rptr - 1)) {
			leftcursor();
			vptr--;
			rptr--;
		}
	}
}

static VOID NEAR _inputstr_delete(VOID_A)
{
	int rw, vw;

	if (rptr >= inputlen) {
		ringbell();
		return;
	}

	if (iskanji1(inputbuf, rptr)) {
		if (rptr + 1 >= inputlen) {
			ringbell();
			return;
		}
		rw = vw = 2;
	}
#ifdef	CODEEUC
	else if (isekana(inputbuf, rptr)) {
		if (rptr + 1 >= inputlen) {
			ringbell();
			return;
		}
		rw = 2;
		vw = 1;
	}
#else
	else if (isskana(inputbuf, rptr)) rw = vw = 1;
#endif
	else if (iscntrl2(inputbuf[rptr])) {
		rw = 1;
		vw = 2;
	}
	else if (ismsb(inputbuf[rptr])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	deletechar(rptr, vptr, vw);
	delshift(rptr, rw);
	inputlen -= rw;
}

static VOID NEAR _inputstr_enter(VOID_A)
{
	int len, quote;

	if (!curfilename) {
		ringbell();
		return;
	}

	quote = '\0';
	keyflush();
	len = strlen(curfilename);
	if (insertstr(curfilename, len, rptr, &quote, NULL) < 0) {
		ringbell();
		return;
	}
	if (quote > '\0') VOID_C insertcursor(&rptr, &vptr, 1, quote);
	if (iseol(vptr)) setcursor(rptr, vptr);

	return;
}

#if	FD >= 2
static VOID NEAR _inputstr_case(upper)
int upper;
{
	int ch;

	keyflush();
	if (rptr >= inputlen) {
		ringbell();
		return;
	}
	if (!iswchar(inputbuf, rptr)) {
		ch = (upper)
			? toupper2(inputbuf[rptr]) : tolower2(inputbuf[rptr]);
		if (ch != inputbuf[rptr]) {
			inputbuf[rptr++] = ch;
			vptr++;
			putcursor(ch, 1);
# ifdef	DEP_ORIGSHELL
			if (dumbmode) VOID_C checkcursor(rptr, vptr);
			else
# endif
			if (within(vptr) && ptr2col(vptr) < 1)
				setcursor(rptr, vptr);
			return;
		}
	}

	rightchar();
}

static int NEAR search_matchlen(VOID_A)
{
	int n;

	if (searchstr) {
		n = rptr - strlen(searchstr);
		if (n < 0) n = 0;
		for (; n < rptr; n++)
			if (!memcmp(searchstr, &(inputbuf[n]), rptr - n))
				return(n);
	}

	return(rptr);
}

static char *NEAR search_up(bias, histnop, h, tmp)
int bias, *histnop, h;
char **tmp;
{
	int n, cx, hlen, slen;

	searchmode = -1;
	if (!searchstr) return(NULL);

	slen = strlen(searchstr);
	cx = search_matchlen() - bias;
	if (cx > inputlen - slen) cx = inputlen - slen;
	for (; cx >= 0; cx--) {
		if (strncmp(&(inputbuf[cx]), searchstr, slen)) continue;
		rptr = cx + slen;
		vptr = vlen(inputbuf, rptr);
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
				inputbuf[inputlen] = '\0';
				*tmp = strdup2(inputbuf);
			}
			rptr = cx + slen;
			vptr = vlen(history[h][n], rptr);
			*histnop = n + 1;
			return(history[h][n]);
		}
	}

	ringbell();
	searchmode = -2;

	return(NULL);
}

static char *NEAR search_down(bias, histnop, h, tmp)
int bias, *histnop, h;
char **tmp;
{
	int n, cx, hlen, slen;

	searchmode = 1;
	if (!searchstr) return(NULL);

	slen = strlen(searchstr);
	cx = search_matchlen() + bias;
	for (; cx <= inputlen - slen; cx++) {
		if (strncmp(&(inputbuf[cx]), searchstr, slen)) continue;
		rptr = cx + slen;
		vptr = vlen(inputbuf, rptr);
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

			rptr = cx + slen;
			vptr = vlen(history[h][n], rptr);
			*histnop = n + 1;
			return(history[h][n]);
		}
	}

	if (*tmp && *histnop) {
		hlen = strlen(*tmp);
		for (cx = 0; cx <= hlen - slen; cx++) {
			if (strncmp(&((*tmp)[cx]), searchstr, slen)) continue;

			rptr = cx + slen;
			vptr = vlen(*tmp, rptr);
			*histnop = 0;
			return(*tmp);
		}
	}

	ringbell();
	searchmode = 2;

	return(NULL);
}
#endif	/* FD >= 2 */

static VOID NEAR _inputstr_input(buf, vw)
CONST char *buf;
int vw;
{
	int rw;

	rw = strlen(buf);
	if (vw <= 0 || preparestr(rw, vw) < 0) {
		ringbell();
		keyflush();
		return;
	}
	memcpy(&(inputbuf[rptr]), buf, rw);
	rptr += rw;
	Xcprintf2("%.*k", vw, buf);
	win_x += vw;
	vptr = vlen(inputbuf, rptr);
#ifdef	DEP_ORIGSHELL
	if (dumbmode) VOID_C checkcursor(rptr, vptr);
	else
#endif
	if (within(vptr) && ptr2col(vptr) < vw) setcursor(rptr, vptr);
}

static int NEAR _inputstr(def, comline, h)
int def, comline, h;
{
#if	!MSDOS
	keyseq_t key;
#endif
#if	FD >= 2
	ALLOC_T searchsize;
	CONST char *cp;
#endif
	char *tmphist, buf[MAXKLEN + 1];
	int i, n, ch, ch2, ovptr, hist, quote, sig;

	subwindow = 1;

#ifdef	DEP_ORIGSHELL
	if (dumbmode) maxcol -= RIGHTMARGIN;
	if (shellmode) sig = 0;
	else
#endif
	sig = 1;
	Xgetkey(-1, VI_INSERT, 0);
#ifndef	_NOCOMPLETE
	tmpfilepos = -1;
#endif
	rptr = inputlen;
	if (def >= 0 && def < maxcol) {
		while (def > inputlen) {
			insertbuf(0);
			inputbuf[inputlen++] = ' ';
		}
		rptr = def;
	}
	vptr = vlen(inputbuf, rptr);
	displaystr();
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
		ovptr = vptr;
		if (quote) {
			i = ch = getkey2(sigalrm(sig), getinkcode(), 0);
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
				_inputstr_input(buf, n);
				ovptr = vptr;
				continue;
			}

			keyflush();
			ch = '\0';
			if (!*buf) continue;

			n = (iscntrl2(buf[0])) ? 2 : 4;
			if (preparestr(1, n) < 0) {
				ringbell();
				continue;
			}

			inputbuf[rptr] = buf[0];
			putstr(&rptr, &vptr, 1);
			continue;
		}

#ifdef	DEP_ORIGSHELL
		if (shellmode && !inputlen) {
			if ((ch = Xgetkey(sig, 1, 0)) < 0) {
				ch = K_ESC;
				break;
			}
		}
		else
#endif
		ch = Xgetkey(sig, 0, 0);
#ifdef	DEP_ORIGSHELL
		if (shellmode && !comline && ch == cc_intr) break;
#endif

#if	FD >= 2
		if (searchmode) {
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
						free2(searchstr);
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
				cp = search_up(n, &hist, h, &tmphist);
			else if (searchmode > 0)
				cp = search_down(n, &hist, h, &tmphist);
			copyhist(cp, (cp) ? 0 : 1);
			ovptr = vptr;

			if (searchmode) continue;
		}
#endif	/* FD >= 2 */

#ifdef	DEP_IME
		if (!imemode && !selectlist
		&& imekey >= 0 && ch == imekey && ch != K_ESC) {
				keyflush();
# ifdef	DEP_ORIGSHELL
				if (dumbmode) ringbell();
				else
# endif
				imemode = 1;
				continue;
		}
#endif	/* DEP_IME */

		switch (ch) {
			case K_RIGHT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (completable(h) && selectlist) {
					i = tmpfilepos;
					tmpfilepos += tmpfileperrow;
					selectfile(i, NULL);
					ovptr = -1;
				}
				else
#endif
				if (rptr >= inputlen) ringbell();
#ifndef	_NOEDITMODE
				else if (isvimode()
				&& inputlen && rptr >= inputlen - 1)
					ringbell();
#endif
				else {
					rightchar();
					ovptr = vptr;
				}
				break;
			case K_LEFT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (completable(h) && selectlist) {
					i = tmpfilepos;
					tmpfilepos -= tmpfileperrow;
					selectfile(i, NULL);
					ovptr = -1;
				}
				else
#endif
				if (rptr <= 0) ringbell();
				else {
					leftchar();
					ovptr = vptr;
				}
				break;
			case K_BEG:
				keyflush();
				rptr = vptr = 0;
				break;
			case K_EOL:
				keyflush();
				rptr = inputlen;
#ifndef	_NOEDITMODE
				if (isvimode() && inputlen) rptr--;
#endif
				vptr = vlen(inputbuf, rptr);
				break;
			case K_BS:
				keyflush();
				if (rptr <= 0) {
					ringbell();
					break;
				}
				leftchar();
				ovptr = vptr;
				_inputstr_delete();
				break;
			case K_DC:
				keyflush();
				_inputstr_delete();
#ifndef	_NOEDITMODE
				if (isvimode()
				&& inputlen && rptr >= inputlen) {
					leftchar();
					ovptr = vptr;
				}
#endif
				break;
			case K_DL:
				keyflush();
				if (rptr < inputlen) truncline();
				inputlen = rptr;
#ifndef	_NOEDITMODE
				if (isvimode() && inputlen) {
					leftchar();
					ovptr = vptr;
				}
#endif
				break;
			case K_CTRL('L'):
				keyflush();
#ifdef	DEP_ORIGSHELL
				if (dumbmode) rewritecursor(0, 0);
				else if (shellmode) {
					locate2(0, 0);
					Xputterm(L_CLEAR);
				}
				else
#endif
				{
#ifdef	DEP_PTY
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
				displaystr();
				break;
			case K_UP:
				_inputstr_up(&hist, h, &tmphist);
				ovptr = vptr;
				break;
			case K_DOWN:
				_inputstr_down(&hist, h, &tmphist);
				ovptr = vptr;
				break;
			case K_IL:
				keyflush();
				quote = 1;
				break;
			case K_ENTER:
				_inputstr_enter();
				ovptr = vptr;
				break;
#if	FD >= 2
			case K_IC:
				keyflush();
				overwritemode = 1 - overwritemode;
				break;
			case K_PPAGE:
				_inputstr_case(0);
				ovptr = vptr;
				break;
			case K_NPAGE:
				_inputstr_case(1);
				ovptr = vptr;
				break;
			case K_CTRL('S'):
				keyflush();
				if (nohist(h) || !history[h]) {
					ringbell();
					break;
				}
				searchmode = 1;
				free2(searchstr);
				searchstr = NULL;
				searchsize = (ALLOC_T)0;
				copyhist(NULL, 1);
				break;
			case K_CTRL('R'):
				keyflush();
				if (nohist(h) || !history[h]) {
					ringbell();
					break;
				}
				searchmode = -1;
				free2(searchstr);
				searchstr = NULL;
				searchsize = (ALLOC_T)0;
				copyhist(NULL, 1);
				break;
#endif	/* FD >= 2 */
#ifndef	_NOCOMPLETE
			case '\t':
				keyflush();
				if (!completable(h) || selectlist) {
					ringbell();
					break;
				}
				i = completestr(comline,
					(ch2 == ch) ? 1 : 0, h);
				if (i < 0) break;
				if (iseol(vptr)) ovptr = -1;
				break;
#endif	/* !_NOCOMPLETE */
			case K_CR:
				keyflush();
#ifndef	_NOCOMPLETE
				if (!completable(h) || !selectlist) break;
				ch = '\0';
				i = completestr(0, -1, h);
				if (i >= 0 && iseol(vptr)) ovptr = -1;
#endif	/* !_NOCOMPLETE */
				break;
			case K_ESC:
				keyflush();
				break;
			default:
				i = getkanjikey(buf, ch);
				_inputstr_input(buf, i);
				ovptr = vptr;
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
			displaystr();
			ovptr = -1;
		}
		if (ovptr != vptr) setcursor(rptr, vptr);
#endif	/* !_NOCOMPLETE */
		if (ch == K_ESC) {
#ifdef	DEP_ORIGSHELL
			if (shellmode);
			else
#endif
			break;
		}
	} while (ch != K_CR);

#ifndef	_NOCOMPLETE
	if (selectlist) selectfile(-1, NULL);
#endif
	setcursor(inputlen, -1);
	subwindow = 0;
	Xgetkey(-1, 0, 0);
	free2(tmphist);

	i = 0;
#ifdef	DEP_ORIGSHELL
	if (shellmode && ch == cc_intr) inputlen = 0;
	else
#endif
	if (ch == K_ESC) {
		inputlen = 0;
		if (maxcmdline) i = 0;
#ifdef	DEP_PTY
		else if (minline > 0) i = 1;
#endif
		else if (hideclock) i = 1;
	}
	inputbuf[inputlen] = '\0';

	Xtflush();
	hideclock = 0;
	if (i) rewritefile(1);

	return(ch);
}

static VOID NEAR dispprompt(s, set)
CONST char *s;
int set;
{
	static CONST char *prompt = NULL;
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
#ifdef	DEP_ORIGSHELL
			if (dumbmode || shellmode) /*EMPTY*/;
			else
#endif
			plen++;
		}
		else {
			plen = evalprompt(&buf, promptstr);
			free2(buf);
#ifndef	DEP_ORIGSHELL
			plen++;
#endif
		}
		return;
	}

#ifdef	DEP_ORIGSHELL
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
#ifdef	DEP_ORIGSHELL
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
#ifndef	DEP_ORIGSHELL
		Xputch2(' ');
		Xputterm(T_STANDOUT);
#endif
		plen = evalprompt(&buf, promptstr);
		Xkanjiputs(buf);
		free2(buf);
#ifdef	DEP_ORIGSHELL
		if (dumbmode);
		else
#else
		plen++;
		Xputterm(END_STANDOUT);
#endif
		Xputterm(T_NORMAL);
	}

	win_x += plen;
}

char *inputstr(prompt, delsp, ptr, def, h)
CONST char *prompt;
int delsp, ptr;
CONST char *def;
int h;
{
	int i, len, ch, pc, qtop, quote, comline, dupwin_x, dupwin_y;

	dupwin_x = win_x;
	dupwin_y = win_y;
	win_x = 0;
	xpos = LEFTMARGIN;
#ifdef	DEP_ORIGSHELL
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

#ifdef	DEP_PTY
# ifdef	DEP_ORIGSHELL
	if (!fdmode && shellmode) /*EMPTY*/;
	else
# endif
	if (isptymode() && parentfd < 0 && !maxcmdline) {
		minline = filetop(win);
		maxline = minline + FILEPERROW;
		ypos -= n_line - maxline;
	}
#endif	/* DEP_PTY */

#ifdef	DEP_ORIGSHELL
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

	inputbuf = c_realloc(NULL, 0, &inputsize);
	inputlen = 0;
	if (def) {
		quote = '\0';
		qtop = 0;
		for (i = 0; def[i]; i++, inputlen++) {
			insertbuf(3);
			pc = parsechar(&(def[i]), -1,
				'!', EA_FINDMETA, &quote, NULL);
			if (pc == PC_WCHAR) {
#ifdef	CODEEUC
				if (ptr > inputlen && isekana(def, i)) ptr++;
#endif
				inputbuf[inputlen++] = def[i++];
			}
#ifndef	FAKEESCAPE
			else if (prompt) /*EMPTY*/;
			else if (pc == PC_EXMETA || pc == '!') {
				if (quote) {
					if (ptr > inputlen) ptr += 2;
					inputbuf[inputlen++] = quote;
					quote = '\0';
				}
				else {
					if (ptr > inputlen) ptr++;
				}
				inputbuf[inputlen++] = PESCAPE;
				qtop = inputlen + 1;
			}
#endif	/* !FAKEESCAPE */
			else if (pc == PC_META) {
				if (ptr > inputlen) ptr++;
				memmove(&(inputbuf[qtop + 1]),
					&(inputbuf[qtop]), inputlen++ - qtop);
				inputbuf[qtop] = quote = '"';
				if (strchr2(DQ_METACHAR, def[i])) {
					if (ptr > inputlen) ptr++;
#ifdef	FAKEESCAPE
					inputbuf[inputlen++] = def[i];
#else
					inputbuf[inputlen++] = PESCAPE;
#endif
				}
			}
			inputbuf[inputlen] = def[i];
		}
		if (quote) {
			if (ptr > inputlen) ptr++;
			inputbuf[inputlen++] = quote;
		}
	}
	inputbuf[inputlen] = '\0';

#ifndef	_NOSPLITWIN
	if (h == HST_PATH && windows > 1) {
		if ((i = win - 1) < 0) i = windows - 1;
		entryhist(winvar[i].v_fullpath, HST_PATH | HST_UNIQ);
	}
#endif

	comline = (h == HST_COMM) ? 1 : 0;
#ifdef	DEP_ORIGSHELL
	if (promptstr == promptstr2) comline = 0;
	lastofs2 = 0;
#endif
	ch = _inputstr(ptr, comline, h);
	win_x = dupwin_x;
	win_y = dupwin_y;
	len = strlen(inputbuf);

#ifdef	DEP_ORIGSHELL
	if (dumbmode || shellmode) prompt = NULL;
	dumbmode = 0;
#endif
	if (!prompt || !*prompt) {
		if (ch != K_ESC) {
			if (ch == cc_intr) {
				Xcputs2("^C");
				ch = K_ESC;
			}
#ifdef	DEP_PTY
			if (minline > 0
			&& ypos + ptr2line(vlen(inputbuf, len)) >= maxline - 1)
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
		free2(inputbuf);
		return(NULL);
	}

	if (delsp && len > 0 && inputbuf[len - 1] == ' ' && yesno(DELSP_K)) {
		for (len--; len > 0; len--)
			if (inputbuf[len - 1] != ' ') break;
		inputbuf[len] = '\0';
	}

	return(inputbuf);
}

static VOID NEAR truncstr(s)
CONST char *s;
{
	int len;
	char *cp, *tmp;

	if ((len = strlen2(s) + YESNOSIZE - n_lastcolumn) <= 0
	|| !(cp = strchr2(s, '[')) || !(tmp = strchr2(cp, ']')))
		return;

	cp++;
	len = tmp - cp - len;
	if (len <= 0) len = 0;
	else if (onkanji1(cp, len - 1)) len--;
#ifdef	CODEEUC
	else if (isekana(cp, len - 1)) len--;
#endif
	strcpy2(&(cp[len]), tmp);
}

static int NEAR yesnomes(mes)
CONST char *mes;
{
	int len;

#ifdef	DEP_ORIGSHELL
	if (dumbmode) forwline(xpos);
	else if (shellmode) locate2(0, 0);
	else
#endif
	Xlocate(xpos, ypos);
#ifdef	DEP_ORIGSHELL
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
#ifdef	DEP_ORIGSHELL
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
	VOID_C vasprintf3(&buf, fmt, args);
	va_end(args);

	dupwin_x = win_x;
	dupwin_y = win_y;
	duperrno = errno;
	win_x = xpos = 0;
#ifdef	DEP_ORIGSHELL
	dumbmode = (dumbterm || dumbshell) ? !Xtermmode(-1) : 0;
	if (dumbmode || shellmode) lcmdline = -1;
#endif

	minline = 0;
	maxline = n_line;
	if (!lcmdline) ypos = L_MESLINE;
	else if (lcmdline > 0) ypos = lcmdline;
	else ypos = maxline - 1;

#ifdef	DEP_ORIGSHELL
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
	Xgetkey(-1, 0, 0);

	ret = 1;
	x = len + 1;
	do {
		keyflush();
#ifdef	DEP_ORIGSHELL
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
		switch (ch = Xgetkey(1, 0, 0)) {
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
	free2(buf);

#ifdef	DEP_ORIGSHELL
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
#ifdef	DEP_ORIGSHELL
	dumbmode = 0;
#endif
	Xtflush();
	lcmdline = 0;

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;
	Xgetkey(-1, 0, 0);
	errno = duperrno;

	return(ret);
}

VOID warning(no, s)
int no;
CONST char *s;
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
		getkey3(sigalrm(1), getinkcode(), 0);
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

	free2(tmp);
	hideclock = lcmdline = 0;
}

static int NEAR selectcnt(max, str, multi)
int max;
char *CONST *str;
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
CONST char *CONST *str;
char **tmpstr;
int *xx, multi;
{
	char *cp, **new;
	int i, len, maxlen;

	for (i = 0; i < max; i++) {
		free2(tmpstr[i]);
		tmpstr[i] = strdup2(str[i]);
	}
	len = selectcnt(max, tmpstr, multi);

	if (x + len < n_lastcolumn) /*EMPTY*/;
	else if ((x = n_lastcolumn - 1 - len) >= 0) /*EMPTY*/;
	else {
		x = maxlen = 0;
		new = (char **)malloc2(max * sizeof(char *));
		for (i = 0; i < max; i++) {
			if (!(cp = tmpstr[i])) {
				new[i] = NULL;
				continue;
			}
			if (isupper2(*cp) && cp[1] == ':')
				for (cp += 2; *cp == ' '; cp++) /*EMPTY*/;
			len = strlen3(cp);
			if (len > maxlen) maxlen = len;
			new[i] = strdup2(cp);
		}

		for (; maxlen > 0; maxlen--) {
			for (i = 0; i < max; i++) if (new[i]) {
				len = maxlen;
				strncpy3(tmpstr[i], new[i], &len, -1);
			}
			if (x + selectcnt(max, tmpstr, multi) < n_lastcolumn)
				break;
		}
		for (i = 0; i < max; i++) free2(new[i]);
		free2(new);
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
char *CONST str[];
int val[], *xx, multi;
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
CONST char *CONST str[];
int val[];
{
	char **tmpstr;
	int i, ch, old, new, multi, tmpx, dupwin_x, dupwin_y, *xx, *initial;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	Xgetkey(-1, 0, 0);

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
		keyflush();
		win_x = tmpx + xx[new + 1];
		Xlocate(win_x, win_y);
		Xtflush();
		old = new;

		switch (ch = Xgetkey(1, 0, 0)) {
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
	Xgetkey(-1, 0, 0);

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
			else cputspace(strlen2(tmpstr[i]));
		}
	}
	else {
		if (ch != K_CR) for (i = 0; i < max; i++) val[i] = 0;
		for (i = 0; i < max; i++) {
			if (!tmpstr[i]) continue;
			Xlocate(tmpx + xx[i] + 1, L_MESLINE);
			Xputch2(' ');
			if (val[i]) Xkanjiputs(tmpstr[i]);
			else cputspace(strlen2(tmpstr[i]));
		}
	}
	Xlocate(win_x, win_y);
	Xtflush();
	free2(xx);
	free2(initial);
	for (i = 0; i < max; i++) free2(tmpstr[i]);
	free2(tmpstr);

	return(ch);
}

#ifdef	DEP_URLPATH
char *inputpass(VOID_A)
{
	CONST char *cp;
	char *buf, kbuf[MAXKLEN + 1];
	ALLOC_T size;
	int n, ch, x, y, len, max, wastty, dupwin_x, dupwin_y;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	Xgetkey(-1, 0, 0);

	y = L_CMDLINE;
	cp = PASWD_K;
	if (!(wastty = isttyiomode)) {
		x = strlen2(cp);
		fprintf2(Xstderr, "%k", cp);
		Xfflush(Xstderr);
	}
	else {
		Xlocate(0, y);
		Xputterm(L_CLEAR);
		Xputch2(' ');
		Xputterm(T_STANDOUT);
		x = 1 + kanjiputs2(cp, n_column, -1);
		Xputterm(END_STANDOUT);
		win_x = x;
		win_y = y;
		Xtflush();
	}
	max = n_column - 1 - x;

	keyflush();
	buf = c_realloc(NULL, 0, &size);
	len = 0;
	for (;;) {
		if (!wastty) Xttyiomode(1);
		ch = getkey3(sigalrm(1), getinkcode(), 0);
		n = getkanjikey(kbuf, ch);
		if (!wastty) Xstdiomode();

		if (ch == K_CR) break;
		else if (ch == K_BS) {
			if (!len) ch = -1;
			else {
				len--;
				if (!hidepasswd) {
					if (wastty) {
						Xcprintf2("%c %c", C_BS, C_BS);
						Xtflush();
						win_x--;
					}
					else {
						Xfputs("\b \b", Xstderr);
						Xfflush(Xstderr);
					}
				}
			}
		}
		else if (ch == K_ESC) {
			len = 0;
			break;
		}
		else if (n != 1 || ismsb(ch) || kbuf[1]) ch = -1;
		else if (!hidepasswd && len >= max) ch = -1;
		else {
			buf = c_realloc(buf, len + 1, &size);
			buf[len++] = ch;
			if (!hidepasswd) {
				ch = '*';
				if (wastty) {
					Xputch2(ch);
					Xtflush();
					win_x++;
				}
				else {
					Xfputc(ch, Xstderr);
					Xfflush(Xstderr);
				}
			}
		}

		if (ch >= 0) /*EMPTY*/;
		else if (wastty) {
			ringbell();
			Xtflush();
		}
		else {
			Xfputc('\007', Xstderr);
			Xfflush(Xstderr);
		}
	}

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;

	if (wastty) {
		Xlocate(0, y);
		Xputterm(L_CLEAR);
		Xlocate(win_x, win_y);
		Xtflush();
	}
	else {
		VOID_C fputnl(Xstderr);
		Xfflush(Xstderr);
	}

	buf = realloc2(buf, len + 1);
	buf[len] = '\0';

	return(buf);
}
#endif	/* DEP_URLPATH */
