/*
 *	input.c
 *
 *	Input Module
 */

#include <signal.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"

#ifndef	_NOORIGSHELL
#include "system.h"
#endif

extern char **history[];
extern short histsize[];
extern int curcolumns;
extern int minfilename;
extern int sizeinfo;
extern int hideclock;
#ifdef	SIGALRM
extern int noalrm;
#endif
extern char *promptstr;
#ifndef	_NOORIGSHELL
extern char *promptstr2;
#endif
#ifndef	DECLERRLIST
extern char *sys_errlist[];
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

static char *NEAR trquote __P_((char *, int, int *));
static int NEAR vlen __P_((char *, int));
static int NEAR rlen __P_((char *, int));
static VOID NEAR putcursor __P_((int, int));
static VOID NEAR rightcursor __P_((VOID_A));
static VOID NEAR leftcursor __P_((VOID_A));
static VOID NEAR upcursor __P_((VOID_A));
static VOID NEAR downcursor __P_((VOID_A));
#ifndef	_NOORIGSHELL
static VOID NEAR movecursor __P_((char *, char *, int));
#endif
static VOID NEAR Xlocate __P_((int, int));
static VOID NEAR setcursor __P_((int, int, int));
static int NEAR rightchar __P_((char *, int *, int, int, int, int));
static int NEAR leftchar __P_((char *, int *, int, int, int, int));
static VOID NEAR insertchar __P_((char *, int, int, int, int, int));
static VOID NEAR deletechar __P_((char *, int, int, int, int, int));
static VOID NEAR insshift __P_((char *, int, int, int));
static VOID NEAR delshift __P_((char *, int, int, int));
static VOID NEAR truncline __P_((int, int, int, int));
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

#ifndef	_NOEDITMODE
static int emulatekey[] = {
	K_UP, K_DOWN, K_RIGHT, K_LEFT,
	K_IC, K_DC, K_IL, K_DL,
	K_HOME, K_END, K_BEG, K_EOL,
	K_PPAGE, K_NPAGE, K_ENTER, K_ESC
};
static char emacskey[] = {
	K_CTRL('P'), K_CTRL('N'), K_CTRL('F'), K_CTRL('B'),
	K_ESC, K_CTRL('D'), K_CTRL('Q'), K_CTRL('K'),
	K_ESC, K_ESC, K_CTRL('A'), K_CTRL('E'),
	K_CTRL('V'), K_CTRL('Y'), K_CTRL('O'), K_CTRL('G')
};
#define	EMACSKEYSIZ	((int)(sizeof(emacskey) / sizeof(char)))
static char vikey[] = {
	'k', 'j', 'l', 'h',
	K_ESC, 'x', K_ESC, 'D',
	'g', 'G', '0', '$',
	K_CTRL('B'), K_CTRL('F'), 'o', K_ESC
};
#define	VIKEYSIZ	((int)(sizeof(vikey) / sizeof(char)))
static char wordstarkey[] = {
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


int intrkey(VOID_A)
{
	int c;

	if (kbhit2(0) && ((c = getkey2(0)) == cc_intr || c == K_ESC)) {
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

#ifdef	SIGALRM
	sig = (noalrm) ? 0 : SIGALRM;
#else
	sig = 0;
#endif
	ch = getkey2(sig);
	if (eof && (ch != cc_eof || prev == ch)) eof = 0;
	prev = ch;
	if (eof) return(-1);

#ifndef	_NOEDITMODE
	if (!editmode) return(ch);
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
				putterm(t_bell);
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
					putterm(t_bell);
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
	int c, i, j, v;

	cp = malloc2(len * 4 + 1);
	v = 0;
	for (i = j = 0; i < len; i++) {
#ifdef	CODEEUC
		if (i + 1 < len && isekana(s, i)) {
			cp[j++] = s[i++];
			cp[j++] = s[i];
			v++;
		}
#else
		if (iskna(s[i])) {
			cp[j++] = s[i];
			v++;
		}
#endif
		else if (i + 1 < len && iskanji1(s, i)) {
			cp[j++] = s[i++];
			cp[j++] = s[i];
			v += 2;
		}
		else if (i + 1 < len && s[i] == QUOTE) {
			cp[j++] = '^';
			cp[j++] = (s[++i] + '@') & 0x7f;
			v += 2;
		}
		else if (ismsb(s[i])) {
			c = s[i] & 0xff;
			cp[j++] = '\\';
			cp[j++] = (c / (8 * 8)) + '0';
			cp[j++] = ((c % (8 * 8)) / 8) + '0';
			cp[j++] = (c % 8) + '0';
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

static int NEAR vlen(s, len)
char *s;
int len;
{
	int v, r, rw, vw;

	v = r = 0;
	while (r < len) {
#ifdef	CODEEUC
		if (r + 1 < len && isekana(s, r)) {
			rw = 2;
			vw = 1;
		}
#else
		if (iskna(s[r])) rw = vw = 1;
#endif
		else if (r + 1 < len && (iskanji1(s, r) || s[r] == QUOTE))
			rw = vw = 2;
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

static int NEAR rlen(s, len)
char *s;
int len;
{
	int v, r, rw, vw;

	v = r = 0;
	while (v < len) {
#ifdef	CODEEUC
		if (isekana(s, r)) {
			rw = 2;
			vw = 1;
		}
#else
		if (iskna(s[r])) rw = vw = 1;
#endif
		else if (v + 1 < len && (iskanji1(s, r) || s[r] == QUOTE))
			rw = vw = 2;
		else if (v + 3 < len && ismsb(s[r])) {
			rw = 1;
			vw = 4;
		}
		else rw = vw = 1;

		r += rw;
		v += vw;
	}
	return(r);
}

static VOID NEAR putcursor(c, n)
int c, n;
{
	win_x += n;
	while (n--) putch2(c);
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
		if (x < win_x) movecursor(c_nleft, c_left, win_x - x);
		else if (x > win_x) movecursor(c_nright, c_right, x - win_x);
		if (y < win_y) movecursor(c_nup, c_up, win_y - y);
		else if (y > win_y) movecursor("", c_scrollforw, y - win_y);

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

static VOID NEAR setcursor(cx, plen, linemax)
int cx, plen, linemax;
{
	int f;

	f = within(cx, plen, linemax) ? 0 : 1;
	cx -= f;
	Xlocate((cx + plen) % linemax, (cx + plen) / linemax);
	if (f) rightcursor();
}

static int NEAR rightchar(s, cxp, cx2, len, plen, linemax)
char *s;
int *cxp, cx2, len, plen, linemax;
{
	int rw, vw;

#ifdef	CODEEUC
	if (*cxp + 1 < len && isekana(s, *cxp)) {
		rw = 2;
		vw = 1;
	}
#else
	if (iskna(s[*cxp])) rw = vw = 1;
#endif
	else if (*cxp + 1 < len && (iskanji1(s, *cxp) || s[*cxp] == QUOTE))
		rw = vw = 2;
	else if (ismsb(s[*cxp])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	if (*cxp + rw > len) {
		putterm(t_bell);
		return(cx2);
	}
	*cxp += rw;
	cx2 += vw;
	if (within(cx2, plen, linemax) && (cx2 + plen) % linemax < vw)
		setcursor(cx2, plen, linemax);
	else while (vw--) rightcursor();

	return(cx2);
}

/*ARGSUSED*/
static int NEAR leftchar(s, cxp, cx2, len, plen, linemax)
char *s;
int *cxp, cx2, len, plen, linemax;
{
	int rw, vw, ocx2;

#ifdef	CODEEUC
	if (*cxp >= 2 && isekana(s, *cxp - 2)) {
		rw = 2;
		vw = 1;
	}
#else
	if (iskna(s[*cxp - 1])) rw = vw = 1;
#endif
	else if (cx2 >= 2 && onkanji1(s, cx2 - 2)) rw = vw = 2;
	else if (*cxp >= 2 && s[*cxp - 2] == QUOTE) rw = vw = 2;
	else if (ismsb(s[*cxp - 1])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	ocx2 = cx2;
	*cxp -= rw;
	cx2 -= vw;
	if ((ocx2 + plen) % linemax < vw) setcursor(cx2, plen, linemax);
	else while (vw--) leftcursor();

	return(cx2);
}

static VOID NEAR insertchar(s, cx, len, plen, linemax, ins)
char *s;
int cx, len, plen, linemax, ins;
{
	char *dupl;
	int dx, dy, i, j, l, f1, ptr, nlen;
#if	!MSDOS
	int f2;
#endif

	dupl = trquote(&(s[cx]), len - cx, &l);
	cx = vlen(s, cx);
	len = cx + l;
	nlen = len + ins;
	dy = (cx + plen) / linemax;
	i = (dy + 1) * linemax - plen;	/* prev. chars including cursor line */
	j = linemax - (cx + plen) % linemax;
	if (j > ins) j = ins;		/* inserted columns in cursor line */

#if	!MSDOS
	if (*c_insert) {
# ifndef	_NOORIGSHELL
		if (shellmode);
		else
# endif
		if (ypos + dy >= n_line - 1
		&& xpos + (cx + plen) % linemax + (len - cx) + j >= n_column) {
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
			setcursor(cx, plen, linemax);
		}
		while (j--) putterm(c_insert);

		if (i < nlen) {
			while (i < nlen) {
				ptr = i - 1 - cx;
					/* rest columns in cursor line */
				/*
				 * Whether if the end of line is kanji 1st byte
				 * f1: after inserted
				 * f2: before inserted
				 */
				if (ptr < ins) f1 = 0;
				else f1 = (onkanji1(dupl, ptr - ins)) ? 1 : 0;
				f2 = (onkanji1(dupl, ptr)) ? 1 : 0;
				if (xpos + linemax < n_column) {
					Xlocate(linemax, dy);
					j = n_column - (xpos + linemax);
					if (j > ins) j = ins;
					if (f1) {
						rightcursor();
						j--;
					}
					putcursor(' ', j);
				}
# ifndef	_NOORIGSHELL
				if (shellmode) dy++;
				else
# endif
				if (ypos + ++dy >= n_line - 1
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
					if (f1) dupl[ptr] = ' ';
					if (f2) l++;
				}
				if (ptr + l > nlen - cx) l = nlen - cx - ptr;
				kanjiputs2(dupl, l, ptr);
				win_x += l;
				i += linemax;
			}
			setcursor(cx, plen, linemax);
		}
	}
	else
#endif	/* !MSDOS */
	{
		putcursor(' ', j);
		j = 0;
		l = i - cx - ins;	/* rest chars following inserted str */
		dx = (cx + ins + plen) % linemax;
					/* cursor column after inserted */

		while (i < nlen) {
			ptr = i - 1 - cx;
					/* rest columns in cursor line */
			/*
			 * Whether if the end of line is kanji 1st byte
			 * f1: after inserted
			 */
			if (ptr < ins) f1 = l = 0;
			else f1 = (onkanji1(dupl, ptr - ins)) ? 1 : 0;
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
			if ((l += f1) > 0) {
				kanjiputs2(dupl, l, j);
				win_x += l;
				if (!f1) putcursor(' ', 1);
			}

			Xlocate(0, ++dy);
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
				if (f1) dupl[ptr] = ' ';
			}
			if (ptr + l > len - cx) l = len - cx - ptr;
			j = ptr;
			i += linemax;
			dx = 0;
		}

		l = len - cx - j;
		if (l > 0) {
			kanjiputs2(dupl, l, j);
			win_x += l;
		}
		setcursor(cx, plen, linemax);
	}
#ifndef	_NOORIGSHELL
	if (shellmode) {
		if (((nlen + plen) % linemax) == 1) {
			setcursor(nlen, plen, linemax);
			putterm(l_clear);
			setcursor(cx, plen, linemax);
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
		setcursor(cx, plen, linemax);
	}
	free(dupl);
}

static VOID NEAR deletechar(s, cx, len, plen, linemax, del)
char *s;
int cx, len, plen, linemax, del;
{
	char *dupl;
	int dy, i, j, l, f1, ptr, nlen;
#if	!MSDOS
	int f2;
#endif

	dupl = trquote(&(s[cx]), len - cx, &l);
	cx = vlen(s, cx);
	len = cx + l;
	nlen = len - del;
	dy = (cx + plen) / linemax;
	i = (dy + 1) * linemax - plen;	/* prev. chars including cursor line */

#if	!MSDOS
	if (*c_delete) {
		j = linemax - (cx + plen) % linemax;
		if (j > del) j = del;	/* deleted columns in cursor line */
		while (j--) putterm(c_delete);

		if (i < len) {
			while (i < len) {
				ptr = i - 1 - cx;
					/* rest columns in cursor line */
				/*
				 * Whether if the end of line is kanji 1st byte
				 * f1: after deleted
				 */
				if (ptr >= nlen - cx) f1 = 0;
				else f1 = (onkanji1(dupl, ptr + del)) ? 1 : 0;
				f2 = (onkanji1(dupl, ptr)) ? 1 : 0;
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
				if (ptr + l > len) l = len - ptr;
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
			setcursor(cx, plen, linemax);
		}
	}
	else
#endif	/* !MSDOS */
	{
		j = del;
		l = i - cx;	/* rest chars following cursor */

		while (i < len) {
			ptr = i - 1 - cx;
				/* rest columns in cursor line */
			/*
			 * Whether if the end of line is kanji 1st byte
			 * f1: after deleted
			 */
			if (ptr >= nlen - cx) f1 = 0;
			else f1 = (onkanji1(dupl, ptr + del)) ? 1 : 0;
			if ((l += f1) > 0) {
				kanjiputs2(dupl, l, j);
				win_x += l;
			}
			if (!f1) putcursor(' ', 1);

			Xlocate(0, ++dy);
			l = linemax + 1;
			ptr += del + 1;
			if (f1) dupl[rlen(dupl, ptr)] = ' ';
			if (ptr + l > len - cx) l = len - cx - ptr;
			j = ptr;
			i += linemax;
		}

		l = len - cx - j;
		if (l > 0) {
			kanjiputs2(dupl, l, j);
			win_x += l;
		}
		putterm(l_clear);
		setcursor(cx, plen, linemax);
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

static VOID NEAR truncline(cx, len, plen, linemax)
int cx, len, plen, linemax;
{
	int dy, i;

	putterm(l_clear);

	dy = (cx + plen) / linemax;
	i = (dy + 1) * linemax - plen;
	if (i < len) {
#ifndef	_NOORIGSHELL
		if (shellmode) while (i < len) {
			Xlocate(0, ++dy);
			putterm(l_clear);
			i += linemax;
		}
		else
#endif
		while (i < len) {
			if (ypos + ++dy >= n_line) break;
			locate(xpos, ypos + dy);
			putterm(l_clear);
			i += linemax;
		}
		setcursor(cx, plen, linemax);
	}
}

static VOID NEAR displaystr(s, cxp, lenp, plen, linemax)
char *s;
int *cxp, *lenp, plen, linemax;
{
	char *dupl;
	int i, y, cx, len, max, vi, width, f;

	Xlocate(plen, 0);
	dupl = trquote(s, *lenp, &len);
	cx = vlen(s, *cxp);
	max = maxscr(plen, linemax);
	if (len > max) {
		len = max;
		*lenp = rlen(s, max);
		if (cx >= len) {
			cx = len;
			*cxp = *lenp;
		}
	}
	width = linemax - plen;
	vi = (t_nocursor && *t_nocursor && t_normalcursor && *t_normalcursor);
	if (vi) putterms(t_nocursor);
	for (i = 0, y = 1; i + width < len; y++) {
		/*
		 * Whether if the end of line is kanji 1st byte
		 */
		f = (onkanji1(dupl, i + width - 1)) ? 1 : 0;
		putterm(l_clear);
		if (stable_standout) putterm(end_standout);

		kanjiputs2(dupl, width + f, i);
		win_x += width + f;
		if (!f) putcursor(' ', 1);
#ifndef	_NOORIGSHELL
		if (shellmode);
		else
#endif
		if (ypos + y >= n_line) {
			locate(0, n_line - 1);
			putterm(c_scrollforw);
			ypos--;
			hideclock = 1;
		}
		Xlocate(0, y);
		if (f) putcursor(' ', 1);
		i += width + f;
		width = linemax - f;
	}
	putterm(l_clear);
	if (stable_standout) putterm(end_standout);
	kanjiputs2(dupl, len - i, i);
	win_x += len + i;
#ifndef	_NOORIGSHELL
	if (shellmode);
	else
#endif
	while (ypos + y < n_line) {
		locate(xpos, ypos + y++);
		putterm(l_clear);
	}
	if (vi) putterms(t_normalcursor);
	setcursor(cx, plen, linemax);
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
#ifdef	CODEEUC
	else if (cx + 1 < *lenp && isekana(*sp, cx)) {
		rw = 2;
		vw = 1;
	}
#else
	else if (iskna((*sp)[cx])) rw = vw = 1;
#endif
	else if (cx + 1 < *lenp && (iskanji1(*sp, cx) || (*sp)[cx] == QUOTE))
		rw = vw = 2;
	else if (ismsb((*sp)[cx])) {
		rw = 1;
		vw = 4;
	}
	else rw = vw = 1;

	if (!vw);
	else if (vins > vw)
		insertchar(*sp, cx, *lenp, plen, linemax, vins - vw);
	else if (vins < vw)
		deletechar(*sp, cx, *lenp, plen, linemax, vw - vins);

	if (!rw);
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
	int i, j, dy, f, ptr, len2, max;

	len2 = vlen(*sp, len);
	max = maxscr(plen, linemax);
	if (len2 + vlen(strins, ins) > max) ins = rlen(strins, max - len2);

	if (ins > 0) {
#ifdef	CODEEUC
		if (isekana(strins, ins - 1)) ins--;
		else
#endif
		if (onkanji1(strins, vlen(strins, ins) - 1)) ins--;
	}
	if (ins <= 0) return(0);

	dupl = malloc2(ins * 2 + 1);
	insertchar(*sp, cx, len, plen, linemax, vlen(strins, ins));
	*sp = c_realloc(*sp, len + ins, sizep);
	insshift(*sp, cx, len, ins);
	for (i = 0; i < ins; i++) (*sp)[cx + i] = ' ';
	len += ins;
	for (i = j = 0; i < ins; i++, j++) {
		if (isctl(strins[i])) {
			insertchar(*sp, cx, len, plen, linemax, 1);
			*sp = c_realloc(*sp, len + 1, sizep);
			insshift(*sp, cx + j, len, 1);
			dupl[j] = '^';
			(*sp)[cx + j] = QUOTE;
			j++;
			dupl[j] = (strins[i] + '@') & 0x7f;
			(*sp)[cx + j] = strins[i];
		}
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
#endif	/* !FAKEMETA */
		|| (!quote && strchr(METACHAR, strins[i]))) {
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

	cx = vlen(*sp, cx);
	len = vlen(dupl, j);
	dy = (cx + plen) / linemax;
	i = (dy + 1) * linemax - plen - cx;
	ptr = 0;
	cx = (cx + plen) % linemax;
	while (i < len) {
		/*
		 * Whether if the end of line is kanji 1st byte
		 * f: after inserted
		 */
		f = (onkanji1(dupl, i - 1)) ? 1 : 0;
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
			Xlocate(cx, dy);
		}
		kanjiputs2(dupl, i - ptr + f, ptr);
		win_x += i - ptr + f;
		Xlocate(0, ++dy);
		if (f) dupl[rlen(dupl, i)] = ' ';
		ptr = i;
		cx = 0;
		i += linemax;
	}
	if (len > ptr) {
		kanjiputs2(dupl, len - ptr, ptr);
		win_x += len - ptr;
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
			len = strlen3(argv[i]);
			if (maxlen < len) maxlen = len;
		}
		dupsorton = sorton;
		sorton = 1;
		qsort(selectlist, argc, sizeof(namelist), cmplist);
		sorton = dupsorton;

		if (lcmdline < 0) {
			char buf[MAXLONGWIDTH + 1];
			int j, n;

			n = 1;
			cputs2("\r\n");
			if (argc < LIMITSELECTWARN) i = 'Y';
			else {
				ascnumeric(buf, argc, 0, MAXLONGWIDTH);
				cputs2("There are ");
				cputs2(buf);
				cputs2(" possibilities.  ");
				cputs2("Do you really\r\n");
				cputs2("wish to see them all? (y or n)");
				tflush();
				for (;;) {
					i = toupper2(getkey2(0));
					if (i == 'Y' || i == 'N') break;
					putterm(t_bell);
					tflush();
				}
				cputs2("\r\n");
				n += 2;
			}

			if (i == 'N');
			else if (n_column < maxlen + 2) {
				n += argc;
				for (i = 0; i < argc; i++) {
					kanjiputs(selectlist[i].name);
					cputs2("\r\n");
				}
			}
			else {
				tmpcolumns = n_column / (maxlen + 2);
				len = (argc + tmpcolumns - 1) / tmpcolumns;
				n += len;
				for (i = 0; i < len; i++) {
					for (j = i; j < argc; j += len)
						kanjiputs2(selectlist[j].name,
							maxlen + 2, 0);
					cputs2("\r\n");
				}
			}

# ifndef	_NOORIGSHELL
			if (shellmode) {
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
	int bq, hadmeta;
# endif
	char *cp1, *cp2, **argv;
	int i, l, ins, top, fix, argc, quote, quoted, hasmeta;

	if (selectlist && cont > 0) {
		selectfile(tmpfilepos++, NULL);
		setcursor(vlen(*sp, cx), plen, linemax);
		return(0);
	}

# ifndef	FAKEMETA
	bq = hadmeta = 0;
# endif
	quote = '\0';
	quoted = 0;
	for (i = top = 0; i < cx; i++) {
		if ((*sp)[i] == quote) {
			if (quote == '"') quoted = i;
# ifndef	FAKEMETA
			if (quote == '\'') quoted = i;
# endif
			quote = '\0';
		}
		else if (iskanji1(*sp, i)) i++;
# ifndef	FAKEMETA
		else if (quote == '\'');
		else if (isnmeta(*sp, i, quote, cx)) {
			i++;
			hadmeta++;
		}
# endif
		else if (quote);
		else if ((*sp)[i] == '"') quote = (*sp)[i];
# ifndef	FAKEMETA
		else if ((*sp)[i] == '\'') quote = (*sp)[i];
		else if ((*sp)[i] == '`') {
			if ((bq = 1 - bq)) top = i + 1;
		}
# endif
		else if ((*sp)[i] == '=' || strchr(CMDLINE_DELIM, (*sp)[i]))
			top = i + 1;
	}
	if (comline && top > 0) {
		for (i = top - 1; i >= 0; i--)
			if ((*sp)[i] != ' ' && (*sp)[i] != '\t') break;
		if (i >= 0 && !strchr(SHELL_OPERAND, (*sp)[i])) comline = 0;
	}

	cp1 = strdupcpy(&((*sp)[top]), cx - top);
	cp1 = evalpath(cp1, 1);
	hasmeta = 0;
	for (i = 0; cp1[i]; i++) {
		if (strchr(METACHAR, cp1[i])) {
			hasmeta = 1;
			break;
		}
		if (iskanji1(cp1, i)) i++;
	}

	if (selectlist && cont < 0) {
		argv = (char **)malloc2(1 * sizeof(char *));
		argv[0] = strdup2(selectlist[tmpfilepos].name);
		argc = 1;
	}
	else {
		argv = NULL;
		argc = 0;
		l = strlen(cp1);
		if (comline && !strdelim(cp1, 1)) {
# ifdef	_NOORIGSHELL
			argc = completeuserfunc(cp1, l, argc, &argv);
			argc = completealias(cp1, l, argc, &argv);
			argc = completebuiltin(cp1, l, argc, &argv);
			argc = completeinternal(cp1, l, argc, &argv);
# else
			argc = completeshellcomm(cp1, l, argc, &argv);
# endif
		}
# ifndef	_NOARCHIVE
		if (archivefile && !comline && *cp1 != _SC_)
			argc = completearch(cp1, l, argc, &argv);
		else
# endif
		argc = completepath(cp1, l, argc, &argv, comline);
		if (!argc && comline)
			argc = completepath(cp1, l, argc, &argv, 0);
	}

	if ((cp2 = strrdelim(cp1, 1))) cp2++;
	else cp2 = cp1;
	ins = strlen(cp2);
	free(cp1);
	if (!argc) {
		putterm(t_bell);
		if (argv) free(argv);
		return(0);
	}

	cp1 = findcommon(argc, argv);
	fix = '\0';
	if (argc == 1 && cp1)
		fix = ((cp2 = strrdelim(cp1, 0)) && !cp2[1]) ? _SC_ : ' ';

	if (!cp1 || ((ins = (int)strlen(cp1) - ins) <= 0 && fix != ' ')) {
		if (cont <= 0) {
			putterm(t_bell);
			l = 0;
		}
		else {
			selectfile(argc, argv);
			if (lcmdline < 0) {
				dispprompt(NULL, 0);
				displaystr(*sp, &cx, &len, plen, linemax);
			}
			setcursor(vlen(*sp, cx), plen, linemax);
			l = -1;
		}
		for (i = 0; i < argc; i++) free(argv[i]);
		free(argv);
		if (cp1) free(cp1);
		return(l);
	}
	for (i = 0; i < argc; i++) free(argv[i]);
	free(argv);

	l = 0;
	if (!hasmeta) for (i = 0; cp1[i]; i++) {
		if (strchr(METACHAR, cp1[i])) {
			hasmeta = 1;
			break;
		}
		if (iskanji1(cp1, i)) i++;
	}

	if (hasmeta) {
		char *tmp, *home;
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

		if (quote);
		else if (quoted > top) {
			quote = (*sp)[quoted];
			setcursor(vlen(*sp, quoted), plen, linemax);
			deletechar(*sp, quoted, len, plen, linemax, 1);
			delshift(*sp, quoted, len--, 1);
			l--;
			setcursor(vlen(*sp, --cx), plen, linemax);
		}
		else if (!overflow(vlen(*sp, len - i) + vlen(home, hlen),
		plen, linemax)) {
			if (home) {
				setcursor(vlen(*sp, top), plen, linemax);
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
				if (iskanji1(*sp, i)) {
					i++;
					continue;
				}
				if (!isnmeta(*sp, i, '\0', cx)) continue;
				if (strchr(DQ_METACHAR, (*sp)[i + 1]))
					continue;
				setcursor(vlen(*sp, i), plen, linemax);
				deletechar(*sp, i, len, plen, linemax, 1);
				delshift(*sp, i, len--, 1);
				l--;
				cx--;
			}
# endif	/* !FAKEMETA */
			setcursor(vlen(*sp, top), plen, linemax);
			insertchar(*sp, top, len, plen, linemax, 1);
			*sp = c_realloc(*sp, len + 2, sizep);
			insshift(*sp, top, len++, 1);
			l++;
			(*sp)[top] = quote = '"';
			putcursor(quote, 1);
			setcursor(vlen(*sp, ++cx), plen, linemax);
		}
		else hasmeta = 0;
	}

	cp2 = cp1 + (int)strlen(cp1) - ins;
	if (fix == _SC_) ins--;
	i = insertstr(sp, cx, len, plen, sizep, linemax, cp2, ins, quote);
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

	free(cp1);
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
	int len;
#endif
	int i, j;

	keyflush();
#ifndef	_NOCOMPLETE
	if (selectlist) {
		selectfile(tmpfilepos--, NULL);
		setcursor(cx2, plen, linemax);
	}
	else
#endif
	if (cx2 < linemax) {
		if (h < 0 || !history[h] || *histnop >= histsize[h]
		|| !history[h][*histnop]) {
			putterm(t_bell);
			return(cx2);
		}
		if (!*tmp) {
			(*sp)[*lenp] = '\0';
			*tmp = strdup2(*sp);
		}
		for (i = j = 0; history[h][*histnop][i]; i++) {
			*sp = c_realloc(*sp, j, sizep);
			if (isctl(history[h][*histnop][i])) (*sp)[j++] = QUOTE;
			(*sp)[j++] = history[h][*histnop][i];
		}
		(*sp)[j] = '\0';
#ifndef	_NOORIGSHELL
		len = *lenp;
#endif
		*cxp = *lenp = j;
		displaystr(*sp, cxp, lenp, plen, linemax);
#ifndef	_NOORIGSHELL
		if (shellmode) {
			int y1, y2;

			y1 = (len + plen) / linemax;
			y2 = (*lenp + plen) / linemax;
			if (y1 > y2) {
				while (y1 > y2) {
					Xlocate(0, y1--);
					putterm(l_clear);
				}
				setcursor(*cxp, plen, linemax);
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
		if (onkanji1(*sp, cx2 - 1)) {
			leftcursor();
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
	int i, j, len;

	len = vlen(*sp, *lenp);
	keyflush();
#ifndef	_NOCOMPLETE
	if (selectlist) {
		selectfile(tmpfilepos++, NULL);
		setcursor(cx2, plen, linemax);
	}
	else
#endif
	if (cx2 + linemax > len || (cx2 + linemax == len
	&& !within(len, plen, linemax) && !((len + plen) % linemax))) {
		if (h < 0 || !history[h] || *histnop <= 0) {
			putterm(t_bell);
			return(cx2);
		}
		if (--(*histnop) > 0) {
			for (i = j = 0; history[h][*histnop - 1][i]; i++) {
				*sp = c_realloc(*sp, j, sizep);
				if (isctl(history[h][*histnop - 1][i]))
					(*sp)[j++] = QUOTE;
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
		len = *lenp;
#endif
		*cxp = *lenp = j;
		displaystr(*sp, cxp, lenp, plen, linemax);
#ifndef	_NOORIGSHELL
		if (shellmode) {
			int y1, y2;

			y1 = (len + plen) / linemax;
			y2 = (*lenp + plen) / linemax;
			if (y1 > y2) {
				while (y1 > y2) {
					Xlocate(0, y1--);
					putterm(l_clear);
				}
				setcursor(*cxp, plen, linemax);
			}
		}
#endif
		cx2 = vlen(*sp, *cxp);
	}
	else {
		cx2 += linemax;
		*cxp = rlen(*sp, cx2);
		downcursor();
		if (onkanji1(*sp, cx2 - 1)) {
			leftcursor();
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
		putterm(t_bell);
		return(len);
	}

#ifdef	CODEEUC
	if (isekana(s, cx)) {
		rw = 2;
		vw = 1;
	}
#else
	if (iskna(s[cx])) rw = vw = 1;
#endif
	else if (iskanji1(s, cx) || s[cx] == QUOTE) {
		if (cx + 1 >= len) {
			putterm(t_bell);
			return(len);
		}
		rw = vw = 2;
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
		putterm(t_bell);
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
		putterm(t_bell);
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
	if (iseol(cx2, plen, linemax)) setcursor(cx2, plen, linemax);
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
		putterm(t_bell);
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
			if (within(cx2, plen, linemax)
			&& (cx2 + plen) % linemax < 1)
				setcursor(cx2, plen, linemax);
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
#if	!MSDOS && !defined (_NOKANJICONV)
	char tmpkanji[3];
#endif
	int rw, vw, ch2;

#if	!MSDOS && !defined (_NOKANJICONV)
	if (inputkcode == EUC && ch == 0x8e) {
		rw = KANAWID;
		vw = 1;
		ch2 = (kbhit2(WAITMETA * 1000L)) ? getkey2(0) : '\0';
		if (!iskna(ch2)
		|| preparestr(sp, *cxp, lenp,
		plen, sizep, linemax, rw, vw) < 0) {
			putterm(t_bell);
			keyflush();
			return(cx2);
		}
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		kanjiconv(&((*sp)[*cxp]), tmpkanji, 2,
			inputkcode, DEFCODE, L_INPUT);
		kanjiputs2(&((*sp)[*cxp]), 1, -1);
		win_x += 1;
		*cxp += KANAWID;
	}
	else
#else
# ifdef	CODEEUC
	if (ch == 0x8e) {
		rw = KANAWID;
		vw = 1;
		ch2 = (kbhit2(WAITMETA * 1000L)) ? getkey2(0) : '\0';
		if (!iskna(ch2)
		|| preparestr(sp, *cxp, lenp,
		plen, sizep, linemax, rw, vw) < 0) {
			putterm(t_bell);
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
#endif
	if (isinkanji1(ch)) {
		rw = vw = 2;
		ch2 = (kbhit2(WAITMETA * 1000L)) ? getkey2(0) : '\0';
		if (!isinkanji2(ch2)
		|| preparestr(sp, *cxp, lenp,
		plen, sizep, linemax, rw, vw) < 0) {
			putterm(t_bell);
			keyflush();
			return(cx2);
		}

#if	MSDOS || defined (_NOKANJICONV)
		(*sp)[*cxp] = ch;
		(*sp)[*cxp + 1] = ch2;
#else
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		kanjiconv(&((*sp)[*cxp]), tmpkanji, 2,
			inputkcode, DEFCODE, L_INPUT);
#endif
		kanjiputs2(&((*sp)[*cxp]), 2, -1);
		win_x += 2;
		*cxp += 2;
	}
	else {
		rw = vw = 1;
		if (ch >= K_MIN || isctl(ch) || ismsb(ch)
		|| preparestr(sp, *cxp, lenp,
		plen, sizep, linemax, rw, vw) < 0) {
			putterm(t_bell);
			keyflush();
			return(cx2);
		}
		(*sp)[(*cxp)++] = ch;
		putcursor(ch, 1);
	}
	cx2 = vlen(*sp, *cxp);
	if (within(cx2, plen, linemax) && ((cx2 + plen) % linemax) < vw)
		setcursor(cx2, plen, linemax);
	return(cx2);
}

static int NEAR _inputstr(sp, plen, size, linemax, def, comline, h)
char **sp;
int plen;
ALLOC_T size;
int linemax, def, comline, h;
{
#if	!MSDOS
	char *cp;
	int l;
#endif
	char *tmphist;
	int i, ch, ch2, cx, cx2, ocx2, len, hist, quote, sig;

	subwindow = 1;

#ifndef	_NOORIGSHELL
	if (shellmode) sig = 0;
	else
#endif
#ifdef	SIGALRM
	if (!noalrm) sig = SIGALRM;
	else
#endif
	sig = 0;
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
			i = ch = getkey2(sig);
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
			if ((cp = getkeyseq(i, &l)) && l == 1) i = *cp;
#endif
			if (i >= K_MIN) {
				putterm(t_bell);
				continue;
			}
			else if (isctl(i)) {
				keyflush();
				ch = '\0';
				if (!i) continue;
				if (preparestr(sp, cx, &len,
				plen, &size, linemax, 2, 2) < 0) {
					putterm(t_bell);
					continue;
				}
				(*sp)[cx++] = QUOTE;
				cx2++;
				putcursor('^', 1);
				if (iseol(cx2, plen, linemax))
					setcursor(cx2, plen, linemax);
				(*sp)[cx++] = i;
				cx2++;
				putcursor((i + '@') & 0x7f, 1);
				if (iseol(cx2, plen, linemax))
					setcursor(cx2, plen, linemax);
				continue;
			}
#if	!MSDOS && !defined (_NOKANJICONV)
			else if (inputkcode == EUC
			&& i == 0x8e && kbhit2(WAITMETA * 1000L));
			else if (inputkcode != EUC && iskna(i));
#else
# ifdef	CODEEUC
			else if (i == 0x8e && kbhit2(WAITMETA * 1000L));
# else
			else if (iskna(i));
# endif
#endif
			else if (isinkanji1(i) && kbhit2(WAITMETA * 1000L));
			else if (ismsb(i)) {
				keyflush();
				ch = '\0';
				if (preparestr(sp, cx, &len,
				plen, &size, linemax, 1, 4) < 0) {
					putterm(t_bell);
					continue;
				}
				(*sp)[cx++] = i;
				cx2++;
				putcursor('\\', 1);
				if (iseol(cx2, plen, linemax))
					setcursor(cx2, plen, linemax);
				cx2++;
				putcursor((i / (8 * 8)) + '0', 1);
				if (iseol(cx2, plen, linemax))
					setcursor(cx2, plen, linemax);
				cx2++;
				putcursor(((i % (8 * 8)) / 8) + '0', 1);
				if (iseol(cx2, plen, linemax))
					setcursor(cx2, plen, linemax);
				cx2++;
				putcursor((i % 8) + '0', 1);
				if (iseol(cx2, plen, linemax))
					setcursor(cx2, plen, linemax);
				continue;
			}
		}
		switch (ch) {
			case K_RIGHT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (selectlist) {
					i = tmpfilepos;
					tmpfilepos += FILEPERROW;
					selectfile(i, NULL);
					ocx2 = -1;
				}
				else
#endif
				if (cx >= len) putterm(t_bell);
				else ocx2 = cx2 = rightchar(*sp, &cx, cx2,
					len, plen, linemax);
				break;
			case K_LEFT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (selectlist) {
					i = tmpfilepos;
					tmpfilepos -= FILEPERROW;
					selectfile(i, NULL);
					ocx2 = -1;
				}
				else
#endif
				if (cx <= 0) putterm(t_bell);
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
					putterm(t_bell);
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
					truncline(cx2, vlen(*sp, len),
						plen, linemax);
				len = cx;
				break;
			case K_CTRL('L'):
				keyflush();
#ifndef	_NOORIGSHELL
				if (shellmode) {
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
				dispprompt(NULL, 0);
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
				if (selectlist) {
					putterm(t_bell);
					break;
				}
				i = completestr(sp, cx, len,
					plen, &size, linemax,
					comline, (ch2 == ch) ? 1 : 0);
				if (i <= 0) {
					if (!i) putterm(t_bell);
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
				if (!selectlist) break;
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
		if (selectlist && ch != '\t'
		&& ch != K_RIGHT && ch != K_LEFT
		&& ch != K_UP && ch != K_DOWN) {
			selectfile(-1, NULL);
			rewritefile(0);
			ocx2 = -1;
		}
		if (ocx2 != cx2) setcursor(cx2, plen, linemax);
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
	setcursor(vlen(*sp, len), plen, linemax);
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
	if (shellmode) Xlocate(0, 0);
	else
#endif
	locate(xpos, ypos);
	if (prompt && *prompt) {
#ifndef	_NOORIGSHELL
		if (shellmode) len = kanjiputs(prompt);
		else
#endif
		{
			putch2(' ');
			putterm(t_standout);
			len = kanjiputs(prompt) + 1;
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
	char *dupl, *input;
	ALLOC_T size;
	int i, j, len, ch, comline, dupwin_x, dupwin_y;

	dupwin_x = win_x;
	dupwin_y = win_y;
	win_x = xpos = 0;
	dupn_line = n_line;
#ifndef	_NOORIGSHELL
	if (shellmode) lcmdline = -1;
#endif

	if (!lcmdline) ypos = L_CMDLINE;
	else if (lcmdline > 0) ypos = lcmdline;
	else ypos = n_line - 1;

#ifndef	_NOORIGSHELL
	if (shellmode) win_y = 0;
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
			if (isctl(def[i])) {
				input[j++] = QUOTE;
				if (ptr >= j) ptr++;
			}
#ifdef	CODEEUC
			else if (isekana(def, i)) {
				input[j++] = def[i++];
				if (ptr >= j) ptr++;
			}
#endif
			else if (iskanji1(def, i)) input[j++] = def[i++];
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
#endif
	ch = _inputstr(&input, len, size, n_column - 1, ptr, comline, h);
	win_x = dupwin_x;
	win_y = dupwin_y;

#ifndef	_NOORIGSHELL
	if (shellmode) prompt = NULL;
#endif
	if (!prompt || !*prompt) {
		if (ch != K_ESC) {
			if (ch == cc_intr) {
				cputs2("^C");
				ch = K_ESC;
			}
			cputs2("\r\n");
			tflush();
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
	dupl = malloc2(len + 1);
	for (i = j = 0; input[i]; i++, j++) {
		if (input[i] == QUOTE) i++;
		dupl[j] = input[i];
	}
	dupl[j] = '\0';
	free(input);
	return(dupl);
}

static char *NEAR truncstr(s)
char *s;
{
	int len;
	char *cp1, *cp2;

	if ((len = (int)strlen2(s) + YESNOSIZE - n_lastcolumn) <= 0
	|| !(cp1 = strchr2(s, '[')) || !(cp2 = strchr2(cp1, ']'))) return(s);

	cp1++;
	len = cp2 - cp1 - len;
	if (len <= 0) len = 0;
#ifdef	CODEEUC
	else if (isekana(cp1, len - 1)) len--;
#endif
	else if (onkanji1(cp1, vlen(cp1, len) - 1)) len--;
	strcpy(&(cp1[len]), cp2);
	return(s);
}

static int NEAR yesnomes(mes)
char *mes;
{
	int len;

#ifndef	_NOORIGSHELL
	if (shellmode) Xlocate(0, 0);
	else
#endif
	locate(xpos, ypos);
	putterm(l_clear);
	putterm(t_standout);
	len = kanjiputs2(mes, n_lastcolumn - YESNOSIZE, -1);
	win_x += len;
	cputs2(YESNOSTR);
	win_x += YESNOSIZE;
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
char *fmt;
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
	if (shellmode) lcmdline = -1;
#endif

	if (!lcmdline) ypos = L_MESLINE;
	else if (lcmdline > 0) ypos = lcmdline;
	else ypos = n_line - 1;

#ifndef	_NOORIGSHELL
	if (shellmode) win_y = 0;
	else
#endif
	win_y = ypos;

	truncstr(buf);
	len = strlen3(buf);
	ret = yesnomes(buf);
	if (ret < len) len = ret;
	if (win_x >= n_column) win_x = n_column - 1;

	subwindow = 1;
	Xgetkey(-1, 0);

	ret = 1;
	x = len + 1;
	do {
		keyflush();
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

	if (lcmdline) {
		Xlocate(x, 0);
		putch2((ret) ? 'Y' : 'N');
		cputs2("\r\n");
	}
	else {
		Xlocate(0, 0);
		putterm(l_clear);
	}
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

#if	MSDOS
	err = strerror((no < 0) ? errno : no);
#else
	err = (char *)sys_errlist[(no < 0) ? errno : no];
#endif

	tmp = NULL;
	if (!s) s = err;
	else if (no) {
		len = n_lastcolumn - (int)strlen(err) - 3;
		tmp = (char *)malloc2(strlen2(s) + strlen(err) + 3);
		strncpy3(tmp, s, &len, -1);
		strcat(tmp, ": ");
		strcat(tmp, err);
		s = tmp;
	}
	putterm(t_bell);

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
		len += strlen(str[i]) + 1;
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

	if (x + len < n_lastcolumn);
	else if ((x = n_lastcolumn - 1 - len) >= 0);
	else {
		x = maxlen = 0;
		str = (char **)malloc2(max * sizeof(char *));
		for (i = 0; i < max; i++) {
			if (!(cp = tmpstr[i])) {
				str[i] = NULL;
				continue;
			}
			if (*cp >= 'A' && *cp <= 'Z' && cp[1] == ':')
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
		xx[i + 1] = xx[i] + strlen3(tmpstr[i]) + 1;
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
		initial[i] = (str[i] && *(str[i]) >= 'A' && *(str[i]) <= 'Z')
			? *str[i] : -1;
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
				ch = toupper2(ch);
				if (ch < 'A' || ch > 'Z') break;
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
			else cprintf2("%*s", strlen3(tmpstr[i]), " ");
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
			if (val[i]) kanjiputs(tmpstr[i]);
			else cprintf2("%*s", strlen3(tmpstr[i]), " ");
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
