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

#if	MSDOS || defined (__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif

extern char **history[];
extern short histsize[];
extern int columns;
extern int filepos;
extern int sorton;
extern int minfilename;
extern int sizeinfo;
#ifndef	DECLERRLIST
extern char *sys_errlist[];
#endif

static int trquote __P_((char *, int));
#ifdef	CODEEUC
static int vlen __P_((char *, int));
static int rlen __P_((char *, int));
#else
#define	vlen(s, len)	(len)
#define	rlen(s, len)	(len)
#endif
static VOID putcursor __P_((int, int));
static VOID rightcursor __P_((VOID_A));
static VOID leftcursor __P_((VOID_A));
static VOID upcursor __P_((VOID_A));
static VOID downcursor __P_((VOID_A));
static VOID Xlocate __P_((int, int));
static VOID setcursor __P_((int, int, int, int));
static int rightchar __P_((char *, int *, int, int, int, int, int));
static int leftchar __P_((char *, int *, int, int, int, int, int));
static VOID insertchar __P_((char *, int, int, int, int, int, int));
static VOID deletechar __P_((char *, int, int, int, int, int, int));
static VOID insshift __P_((char *, int, int, int));
static VOID delshift __P_((char *, int, int, int));
static VOID truncline __P_((int, int, int, int, int));
static VOID displaystr __P_((char *, int, int, int, int, int));
static int insertstr __P_((char *, int, int, int, int, int, char *, int, int));
#ifndef	_NOCOMPLETE
static VOID selectfile __P_((int, char **));
static int completestr __P_((char *, int, int, int, int, int, int, int));
#endif
static int _inputstr_up __P_((char *, int *, int, int *, int, int, int,
		int *, int, char **));
static int _inputstr_down __P_((char *, int *, int, int *, int, int, int,
		int *, int, char **));
static int _inputstr_delete __P_((char *, int, int, int, int, int));
static int _inputstr_enter __P_((char *, int *, int, int *, int, int, int));
static int _inputstr_input __P_((char *, int *, int, int *, int, int, int,
		int));
static int _inputstr __P_((char *, int, int, int, int, int, int));
static int dispprompt __P_((char *, int));
static char *truncstr __P_((char *));
static VOID yesnomes __P_((char *));
static int selectmes __P_((int, int, int, char *[], int [], int []));

int subwindow = 0;
char *curfilename = NULL;
#ifndef	_NOEDITMODE
char *editmode = NULL;
#endif

#ifndef	_NOEDITMODE
static int emulatekey[] = {
	K_UP, K_DOWN, K_RIGHT, K_LEFT,
	K_IC, K_DC, K_IL, K_DL,
	K_HOME, K_END, K_BEG, K_EOL,
	K_PPAGE, K_NPAGE, K_ENTER, ESC
};
static char emacskey[] = {
	CTRL('P'), CTRL('N'), CTRL('F'), CTRL('B'),
	ESC, CTRL('D'), CTRL('Q'), CTRL('K'),
	ESC, ESC, CTRL('A'), CTRL('E'),
	CTRL('V'), CTRL('Y'), CTRL('O'), CTRL('G')
};
static char vikey[] = {
	'k', 'j', 'l', 'h',
	ESC, 'x', ESC, 'D',
	'g', 'G', '0', '$',
	CTRL('B'), CTRL('F'), 'o', ESC
};
static char wordstarkey[] = {
	CTRL('E'), CTRL('X'), CTRL('D'), CTRL('S'),
	CTRL('V'), CTRL('G'), CTRL(']'), CTRL('Y'),
	CTRL('W'), CTRL('Z'), CTRL('A'), CTRL('F'),
	CTRL('R'), CTRL('C'), CTRL('N'), ESC
};
#endif
#ifndef	_NOCOMPLETE
static namelist *selectlist = NULL;
static int tmpfilepos = -1;
#endif
static int xpos = 0;
static int ypos = 0;

int intrkey(VOID_A)
{
	int c;

	if (kbhit2(0) && ((c = getkey2(0)) == cc_intr || c == ESC)) {
		warning(0, INTR_K);
		return(1);
	}
	return(0);
}

int Xgetkey(sig)
int sig;
{
	int ch;
#ifndef	_NOEDITMODE
	static int vimode = 0;
	int i;

	if (sig < 0) {
		vimode = 0;
		return('\0');
	}
#endif

	ch = getkey2(sig);

#ifndef	_NOEDITMODE
	if (!editmode) return(ch);
	else if (!strcmp(editmode, "emacs")) {
		for (i = 0; i < sizeof(emacskey) / sizeof(char); i++) {
			if (emacskey[i] == ESC) continue;
			if (ch == emacskey[i]) return(emulatekey[i]);
		}
	}
	else if (!strcmp(editmode, "vi")) do {
		vimode |= 1;
		if (vimode & 2) switch (ch) {
			case CTRL('V'):
				ch = K_IL;
				break;
			case ESC:
				ch = K_LEFT;
			case CR:
				vimode = 1;
				break;
			case K_BS:
				break;
			default:
				if (ch < K_MIN) break;
				putterm(t_bell);
				vimode &= ~1;
				break;
		}
		else {
			for (i = 0; i < sizeof(vikey) / sizeof(char); i++) {
				if (vikey[i] == ESC) continue;
				if (ch == vikey[i]) return(emulatekey[i]);
			}
			switch (ch) {
				case ':':
					vimode = 2;
					break;
				case 'A':
					vimode = 3;
					ch = K_EOL;
					break;
				case 'i':
					vimode = 2;
					break;
				case 'a':
					vimode = 3;
					ch = K_RIGHT;
					break;
				case K_BS:
					ch = K_LEFT;
					break;
				case ' ':
					ch = K_RIGHT;
					break;
				case ESC:
				case CR:
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
		for (i = 0; i < sizeof(wordstarkey) / sizeof(char); i++) {
			if (wordstarkey[i] == ESC) continue;
			if (ch == wordstarkey[i]) return(emulatekey[i]);
		}
	}
#endif	/* !_NOEDITMODE */

	return(ch);
}

static int trquote(s, cx)
char *s;
int cx;
{
	if (s[cx] == QUOTE) return('^');
	else if (cx > 0 && (s[cx - 1] == QUOTE))
		return((s[cx] + '@') & 0x7f);
	else return(s[cx]);
}

#ifdef	CODEEUC
static int vlen(s, len)
char *s;
int len;
{
	int v, r;

	for (v = r = 0; r < len; v++, r++) if (isekana(s, r)) r++;
	return(v);
}

static int rlen(s, len)
char *s;
int len;
{
	int v, r;

	for (v = r = 0; v < len; v++, r++) if (isekana(s, r)) r++;
	return(r);
}
#endif

static VOID putcursor(c, n)
int c, n;
{
	while (n--) putch2(c);
}

static VOID rightcursor(VOID_A)
{
	putterm(c_right);
}

static VOID leftcursor(VOID_A)
{
	putterm(c_left);
}

static VOID upcursor(VOID_A)
{
	putterm(c_up);
}

static VOID downcursor(VOID_A)
{
	putterm(c_down);
}

static VOID Xlocate(x, y)
int x, y;
{
	while (ypos + y >= n_line) {
		locate(0, n_line - 1);
		putterm(c_scrollforw);
		ypos--;
	}
	locate(xpos + x, ypos + y);
}

static VOID setcursor(cx, plen, max, linemax)
int cx, plen, max, linemax;
{
	int f;

	f = (cx < max) ? 0 : 1;
	cx -= f;
	Xlocate((cx + plen) % linemax, (cx + plen) / linemax);
	if (f) rightcursor();
}

static int rightchar(s, cxp, cx2, len, plen, max, linemax)
char *s;
int *cxp, cx2, len, plen, max, linemax;
{
	if (!iskanji1(s, *cxp) && s[*cxp] != QUOTE) {
#ifdef	CODEEUC
		if (isekana(s, *cxp)) (*cxp)++;
#endif
		(*cxp)++;
		cx2++;
		if (*cxp < max && (cx2 + plen) % linemax < 1)
			setcursor(cx2, plen, max, linemax);
		else rightcursor();
	}
	else {
		if (*cxp + 1 >= len) {
			putterm(t_bell);
			return(cx2);
		}
		*cxp += 2;
		cx2 += 2;
		if (*cxp < max && (cx2 + plen) % linemax < 2)
			setcursor(cx2, plen, max, linemax);
		else {
			rightcursor();
			rightcursor();
		}
	}
	return(cx2);
}

/*ARGSUSED*/
static int leftchar(s, cxp, cx2, len, plen, max, linemax)
char *s;
int *cxp, cx2, len, plen, max, linemax;
{
#ifdef	CODEEUC
	if (*cxp >= 2 && isekana(s, *cxp - 2)) {
		*cxp -= 2;
		if (((cx2--) + plen) % linemax < 1)
			setcursor(cx2, plen, max, linemax);
		else leftcursor();
	}
	else
#endif
	if (cx2 < 2 || (!onkanji1(s, cx2 - 2) && s[*cxp - 2] != QUOTE)) {
		(*cxp)--;
		if (((cx2--) + plen) % linemax < 1)
			setcursor(cx2, plen, max, linemax);
		else leftcursor();
	}
	else {
		(*cxp) -= 2;
		cx2 -= 2;
		if ((cx2 + 2 + plen) % linemax < 2)
			setcursor(cx2, plen, max, linemax);
		else {
			leftcursor();
			leftcursor();
		}
	}
	return(cx2);
}

static VOID insertchar(s, cx, len, plen, max, linemax, ins)
char *s;
int cx, len, plen, max, linemax, ins;
{
	char dupl[MAXLINESTR + 1];
	int dx, dy, i, j, l, f1, ptr;
#if	!MSDOS
	int f2;
#endif

	for (i = 0; i < len - cx; i++) dupl[i] = trquote(s, i + cx);
	dupl[i] = '\0';
#ifdef	CODEEUC
	cx = vlen(s, cx);
	len = vlen(s, len);
#endif
	len += ins;
	dy = (cx + plen) / linemax;
	i = (dy + 1) * linemax - plen;
	j = linemax - (cx + plen) % linemax;
	if (j > ins) j = ins;

#if	!MSDOS
	if (*c_insert) {
		if (ypos + dy >= n_line - 1
		&& xpos + (cx + plen) % linemax + len - ins - cx + j
		>= n_column) {
			while (ypos + dy >= n_line - 1) {
				locate(0, n_line - 1);
				putterm(c_scrollforw);
				ypos--;
			}
			setcursor(cx, plen, max, linemax);
		}
		while (j--) putterm(c_insert);

		if (i < len) {
			while (i < len) {
				ptr = i - 1 - cx;
				if (ptr < ins) f1 = 0;
				else f1 = (onkanji1(dupl, ptr - ins)) ? 1 : 0;
				f2 = (onkanji1(dupl, ptr)) ? 1 : 0;
				if (xpos + linemax < n_column) {
					Xlocate(linemax, dy);
					j = n_column - xpos - linemax;
					if (j > ins) j = ins;
					if (f2) j++;
					if (f1) {
						rightcursor();
						j--;
					}
					putcursor(' ', j);
				}
				if (ypos + ++dy >= n_line - 1
				&& xpos + len - i >= n_column) {
					while (ypos + dy >= n_line - 1) {
						locate(0, n_line - 1);
						putterm(c_scrollforw);
						ypos--;
					}
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
					if (f1) dupl[rlen(dupl, ptr)] = ' ';
					if (f2) l++;
				}
				if (ptr + l > len - cx) l = len - cx - ptr;
				kanjiputs2(dupl, l, ptr);
				i += linemax;
			}
			setcursor(cx, plen, max, linemax);
		}
	}
	else
#endif	/* !MSDOS */
	{
		putcursor(' ', j);
		j = 0;
		l = i - cx - ins;
		dx = (cx + ins + plen) % linemax;

		while (i < len) {
			ptr = i - 1 - cx;
			if (ptr < ins) f1 = l = 0;
			else f1 = (onkanji1(dupl, ptr - ins)) ? 1 : 0;
			if (ypos + dy >= n_line - 1) {
				while (ypos + dy >= n_line - 1) {
					locate(0, n_line - 1);
					putterm(c_scrollforw);
					ypos--;
				}
				Xlocate(dx, dy);
			}
			if ((l += f1) > 0) kanjiputs2(dupl, l, j);
			if (!f1) putch2(' ');

			Xlocate(0, ++dy);
			l = linemax;
			if (ptr < ins - 1) {
				putch2(' ');
				ptr++;
				Xlocate(ins - ptr, dy);
				l -= ins - ptr;
				ptr = 0;
			}
			else {
				ptr -= ins - 1;
				if (f1) dupl[rlen(dupl, ptr)] = ' ';
			}
			if (ptr + l > len - ins - cx) l = len - ins - cx - ptr;
			j = ptr;
			i += linemax;
			dx = 0;
		}

		l = len - ins - cx - j;
		if (l > 0) kanjiputs2(dupl, l, j);
		setcursor(cx, plen, max, linemax);
	}
	if (i == len && len < max && ypos + dy >= n_line - 1) {
		while (ypos + dy >= n_line - 1) {
			locate(0, n_line - 1);
			putterm(c_scrollforw);
			ypos--;
		}
		setcursor(cx, plen, max, linemax);
	}
}

static VOID deletechar(s, cx, len, plen, max, linemax, del)
char *s;
int cx, len, plen, max, linemax, del;
{
	char dupl[MAXLINESTR + 1];
	int dy, i, j, l, f1, ptr;
#if	!MSDOS
	int f2;
#endif

	for (i = 0; i < len - cx; i++) dupl[i] = trquote(s, i + cx);
	dupl[i] = '\0';
#ifdef	CODEEUC
	cx = vlen(s, cx);
	len = vlen(s, len);
#endif
	len -= del;
	dy = (cx + plen) / linemax;
	i = (dy + 1) * linemax - plen;

#if	!MSDOS
	if (*c_delete) {
		j = linemax - (cx + plen) % linemax;
		if (j > del) j = del;
		while (j--) putterm(c_delete);

		if (i < len + del) {
			while (i < len + del) {
				ptr = i - 1 - cx;
				if (ptr >= len - cx) f1 = 0;
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
				if (l > 0) kanjiputs2(dupl, l, ptr);
				if (!f1) putch2(' ');
				Xlocate(0, ++dy);
				for (j = 0; j < del; j++) putterm(c_delete);
				if (f1) putch2(' ');
				i += linemax;
			}
			setcursor(cx, plen, max, linemax);
		}
	}
	else
#endif	/* !MSDOS */
	{
		j = del;
		l = i - cx;

		while (i < len + del) {
			ptr = i - 1 - cx;
			if (ptr >= len - cx) f1 = 0;
			else f1 = (onkanji1(dupl, ptr + del)) ? 1 : 0;
			if ((l += f1) > 0) kanjiputs2(dupl, l, j);
			if (!f1) putch2(' ');

			Xlocate(0, ++dy);
			l = linemax + 1;
			ptr += del + 1;
			if (f1) dupl[rlen(dupl, ptr)] = ' ';
			if (ptr + l > len + del - cx) l = len + del - cx - ptr;
			j = ptr;
			i += linemax;
		}

		l = len + del - cx - j;
		if (l > 0) kanjiputs2(dupl, l, j);
		putterm(l_clear);
		setcursor(cx, plen, max, linemax);
	}
}

static VOID insshift(s, cx, len, ins)
char *s;
int cx, len, ins;
{
	int i;

	for (i = len - 1; i >= cx; i--) s[i + ins] = s[i];
}

static VOID delshift(s, cx, len, del)
char *s;
int cx, len, del;
{
	int i;

	for (i = cx; i < len - del; i++) s[i] = s[i + del];
}

static VOID truncline(cx, len, plen, max, linemax)
int cx, len, plen, max, linemax;
{
	int dy, i;

	putterm(l_clear);

	dy = (cx + plen) / linemax;
	i = (dy + 1) * linemax - plen;
	if (i < len) {
		while (i < len) {
			if (ypos + ++dy >= n_line) break;
			locate(xpos, ypos + dy);
			putterm(l_clear);
			i += linemax;
		}
		setcursor(cx, plen, max, linemax);
	}
}

static VOID displaystr(s, cx, len, plen, max, linemax)
char *s;
int cx, len, plen, max, linemax;
{
	char *dupl;
	int i, y, width;

	Xlocate(plen, 0);
	dupl = malloc2(len + 1);
	for (i = 0; i < len; i++) dupl[i] = trquote(s, i);
	dupl[i] = '\0';
#ifdef	CODEEUC
	cx = vlen(s, cx);
	len = vlen(s, len);
#endif
	width = linemax - plen;
	for (i = 0, y = 1; i + width < len; y++) {
		putterm(l_clear);
		if (stable_standout) putterm(end_standout);
		if (onkanji1(dupl, i + width - 1)) {
			kanjiputs2(dupl, ++width, i);
			Xlocate(0, y);
			putch2(' ');
		}
		else {
			kanjiputs2(dupl, width, i);
			putch2(' ');
			Xlocate(0, y);
		}
		i += width;
		width = linemax;
	}
	putterm(l_clear);
	if (stable_standout) putterm(end_standout);
	kanjiputs2(dupl, width, i);
	for (; y * linemax - plen < max; y++) {
		if (ypos + y >= n_line) break;
		locate(xpos, ypos + y);
		putterm(l_clear);
	}
	setcursor(cx, plen, max, linemax);
	tflush();
	free(dupl);
}

static int insertstr(s, cx, len, plen, max, linemax, insstr, ins, quote)
char *s;
int cx, len, plen, max, linemax;
char *insstr;
int ins, quote;
{
	char dupl[MAXLINESTR + 1];
	int i, j, dy, f, ptr;

	if (len + ins > max) ins = max - len;
	if (ins > 0 && (onkanji1(insstr, vlen(insstr, ins) - 1)
#ifdef	CODEEUC
	|| isekana(insstr, ins - 1)
#endif
	)) ins--;
	if (ins <= 0) return(0);
	insertchar(s, cx, len, plen, max, linemax, vlen(insstr, ins));
	insshift(s, cx, len, ins);
	for (i = j = 0; i < ins; i++, j++) {
		if (!isctl(insstr[i]) && (
#if	!MSDOS
		quote != '"' ||
#endif
		!strchr(DQ_METACHAR, insstr[i])))
			dupl[j] = s[cx + j] = insstr[i];
		else if (len + ins + j - i >= max)
			dupl[j] = s[cx + j] = '?';
		else if (!isctl(insstr[i])) {
#if	MSDOS
			dupl[j] = s[cx + j] =
#else
			dupl[j] = s[cx + j] = META;
#endif
			dupl[j + 1] = s[cx + j + 1] = insstr[i];
			j++;
		}
		else {
			dupl[j] = '^';
			s[cx + j] = QUOTE;
			dupl[++j] = (insstr[i] + '@') & 0x7f;
			s[cx + j] = insstr[i];
		}
	}
	dupl[j] = '\0';

#ifdef	CODEEUC
	cx = vlen(s, cx);
#endif
	len = vlen(dupl, j);
	dy = (cx + plen) / linemax;
	i = (dy + 1) * linemax - plen - cx;
	ptr = 0;
	cx = (cx + plen) % linemax;
	while (i < len) {
		f = (onkanji1(dupl, i - 1)) ? 1 : 0;
		if (ypos + dy >= n_line - 1) {
			while (ypos + dy >= n_line - 1) {
				locate(0, n_line - 1);
				putterm(c_scrollforw);
				ypos--;
			}
			Xlocate(cx, dy);
		}
		kanjiputs2(dupl, i - ptr + f, ptr);
		Xlocate(0, ++dy);
		if (f) dupl[rlen(dupl, i)] = ' ';
		ptr = i;
		cx = 0;
		i += linemax;
	}
	if (len > ptr) kanjiputs2(dupl, len - ptr, ptr);
	return(j);
}

#ifndef	_NOCOMPLETE
static VOID selectfile(argc, argv)
int argc;
char **argv;
{
	static int maxselect, tmpcolumns;
	int i, len, maxlen, dupminfilename, dupcolumns, dupsorton;

	dupminfilename = minfilename;
	dupcolumns = columns;
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

		if ((n_column / 5) - 2 - 1 >= maxlen) tmpcolumns = 5;
		else if ((n_column / 3) - 2 - 1 >= maxlen) tmpcolumns = 3;
		else if ((n_column / 2) - 2 - 1 >= maxlen) tmpcolumns = 2;
		else tmpcolumns = 1;
		columns = tmpcolumns;
		tmpfilepos = listupfile(selectlist, argc, NULL);
		maxselect = argc;
	}
	else if (argc < 0) {
		for (i = 0; i < maxselect; i++) free(selectlist[i].name);
		free(selectlist);
		selectlist = NULL;
	}
	else {
		columns = tmpcolumns;
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
	columns = dupcolumns;
}

static int completestr(s, cx, len, plen, max, linemax, comline, cont)
char *s;
int cx, len, plen, max, linemax, comline, cont;
{
	char *cp1, *cp2, **argv;
	int i, l, ins, top, fix, argc, quote;

	if (selectlist && cont > 0) {
		selectfile(tmpfilepos++, NULL);
		setcursor(vlen(s, cx), plen, max, linemax);
		return(0);
	}

	quote = '\0';
	for (i = top = 0; i < cx; i++) {
		if (quote) {
			if (s[i] == quote) quote = '\0';
		}
		else if (s[i] == '"'
#if	!MSDOS
		|| s[i] == '\''
#endif
		) quote = s[i];
		else if (strchr(CMDLINE_DELIM, s[i])) top = i + 1;
	}
	if (top > 1 && s[top - 1] == ':' && isalpha(s[top - 2])) top -= 2;
	if (top == cx) {
		putterm(t_bell);
		return(0);
	}
	if (top > 0) comline = 0;

	cp1 = malloc2(cx - top + 1);
	strncpy2(cp1, s + top, cx - top);
	cp2 = evalpath(cp1, 1);

	if (selectlist && cont < 0) {
		argv = (char **)malloc2(1 * sizeof(char *));
		argv[0] = strdup2(selectlist[tmpfilepos].name);
		argc = 1;
	}
	else {
		argv = NULL;
		argc = 0;
		if (comline) {
			argc = completeuserfunc(cp2, argc, &argv);
			argc = completealias(cp2, argc, &argv);
			argc = completebuiltin(cp2, argc, &argv);
		}
		argc = completepath(cp2, comline, argc, &argv);
		if (!argc && comline) argc = completepath(cp2, 0, argc, &argv);
	}

	if ((cp1 = strrdelim(cp2, 1))) cp1++;
	else cp1 = cp2;
	ins = strlen(cp1);
	free(cp2);
	if (!argc) {
		putterm(t_bell);
		if (argv) free(argv);
		return(0);
	}

	cp1 = findcommon(argc, argv);
	fix = 0;
	if (argc == 1 && cp1) {
		if (isdelim(cp1, (int)strlen(cp1) - 1)) fix--;
		else fix++;
	}

	if (!cp1 || ((ins = (int)strlen(cp1) - ins) <= 0 && fix <= 0)) {
		if (cont <= 0) putterm(t_bell);
		else {
			selectfile(argc, argv);
			setcursor(vlen(s, cx), plen, max, linemax);
		}
		for (i = 0; i < argc; i++) free(argv[i]);
		free(argv);
		if (cp1) free(cp1);
		return(0);
	}
	for (i = 0; i < argc; i++) free(argv[i]);
	free(argv);

	l = 0;
	if (!quote && len < max) {
		for (i = 0; cp1[i]; i++)
			if (isctl(cp1[i]) || strchr(METACHAR, cp1[i])) break;
		if (cp1[i]) {
			setcursor(vlen(s, top), plen, max, linemax);
			insertchar(s, top, len, plen, max, linemax, 1);
			insshift(s, top, len++, 1);
			l++;
			s[top] = quote = '"';
			putch2(quote);
			setcursor(vlen(s, ++cx), plen, max, linemax);
		}
	}

	cp2 = cp1 + (int)strlen(cp1) - ins;
	if (quote && fix < 0 && len + 1 < max) {
		i = insertstr(s, cx, len, plen,
			max, linemax, cp2, ins - 1, quote);
		l += i;
		cx += i;
		insertchar(s, cx, len, plen, max, linemax, 1);
		insshift(s, cx, len++, 1);
		l++;
		s[cx++] = quote;
		putcursor(quote, 1);
		insertchar(s, cx, len, plen, max, linemax, 1);
		insshift(s, cx, len, 1);
		l++;
		s[cx] = _SC_;
		putcursor(_SC_, 1);
	}
	else {
		i = insertstr(s, cx, len, plen,
			max, linemax, cp2, ins, quote);
		l += i;
		if (fix > 0 && (len += i) < max) {
			cx += i;
			if (quote && len + 1 < max) {
				insertchar(s, cx, len, plen,
					max, linemax, 1);
				insshift(s, cx, len++, 1);
				l++;
				s[cx++] = quote;
				putcursor(quote, 1);
			}
			insertchar(s, cx, len, plen, max, linemax, 1);
			insshift(s, cx, len, 1);
			l++;
			s[cx] = ' ';
			putcursor(' ', 1);
		}
	}

	free(cp1);
	return(l);
}
#endif	/* !_NOCOMPLETE */

static int _inputstr_up(s, cxp, cx2, lenp, plen, max, linemax,
	histnop, h, tmp)
char *s;
int *cxp, cx2, *lenp, plen, max, linemax, *histnop, h;
char **tmp;
{
	int i, j;

	keyflush();
#ifndef	_NOCOMPLETE
	if (selectlist) {
		selectfile(tmpfilepos--, NULL);
		setcursor(cx2, plen, max, linemax);
	}
	else
#endif
	if (cx2 < linemax) {
		if (h < 0 || !history[h] || *histnop >= histsize[h]
		|| !history[h][*histnop]) {
			putterm(t_bell);
			return(cx2);
		}
		if (!(*tmp)) {
			s[*lenp] = '\0';
			*tmp = strdup2(s);
		}
		for (i = j = 0; history[h][*histnop][i]; i++) {
			if (isctl(history[h][*histnop][i])) s[j++] = QUOTE;
			s[j++] = history[h][*histnop][i];
		}
		s[j] = '\0';
		*lenp = strlen(s);
		*cxp = *lenp;
		displaystr(s, *cxp, *lenp, plen, max, linemax);
		cx2 = vlen(s, *cxp);
		(*histnop)++;
	}
	else {
		if (*cxp >= max && !((cx2 + plen) % linemax)) {
			leftcursor();
			cx2--;
		}
		cx2 -= linemax;
		*cxp = rlen(s, cx2);
		upcursor();
		if (onkanji1(s, cx2 - 1)) {
			leftcursor();
			(*cxp)--;
		}
	}
	return(cx2);
}

static int _inputstr_down(s, cxp, cx2, lenp, plen, max, linemax,
	histnop, h, tmp)
char *s;
int *cxp, cx2, *lenp, plen, max, linemax, *histnop, h;
char **tmp;
{
	int i, j, len;

	len = vlen(s, *lenp);
	keyflush();
#ifndef	_NOCOMPLETE
	if (selectlist) {
		selectfile(tmpfilepos++, NULL);
		setcursor(cx2, plen, max, linemax);
	}
	else
#endif
	if (cx2 + linemax > len || (cx2 + linemax == len
	&& *lenp >= max && !((len + plen) % linemax))) {
		if (h < 0 || !history[h] || *histnop <= 0) {
			putterm(t_bell);
			return(cx2);
		}
		if (--(*histnop) > 0) {
			for (i = j = 0; history[h][*histnop - 1][i]; i++) {
				if (isctl(history[h][*histnop - 1][i]))
					s[j++] = QUOTE;
				s[j++] = history[h][*histnop - 1][i];
			}
			s[j] = '\0';
		}
		else {
			strcpy(s, *tmp);
			free(*tmp);
			*tmp = NULL;
		}
		*lenp = strlen(s);
		*cxp = *lenp;
		displaystr(s, *cxp, *lenp, plen, max, linemax);
		cx2 = vlen(s, *cxp);
	}
	else {
		cx2 += linemax;
		*cxp = rlen(s, cx2);
		downcursor();
		if (onkanji1(s, cx2 - 1)) {
			leftcursor();
			(*cxp)--;
		}
	}
	return(cx2);
}

static int _inputstr_delete(s, cx, len, plen, max, linemax)
char *s;
int cx, len, plen, max, linemax;
{
	int n, n2;

	if (cx >= len) {
		putterm(t_bell);
		return(len);
	}
#ifdef	CODEEUC
	if (isekana(s, cx)) {
		n = 2;
		n2 = 1;
	}
	else
#endif
	if (!iskanji1(s, cx) && s[cx] != QUOTE) n = n2 = 1;
	else {
		if (cx + 1 >= len) {
			putterm(t_bell);
			return(len);
		}
		n = n2 = 2;
	}
	deletechar(s, cx, len, plen, max, linemax, n2);
	delshift(s, cx, len, n);
	return(len -= n);
}

static int _inputstr_enter(s, cxp, cx2, lenp, plen, max, linemax)
char *s;
int *cxp, cx2, *lenp, plen, max, linemax;
{
	int i, quote;

	quote = 0;
	keyflush();
	if (curfilename[i = 0] != '~') for (; curfilename[i]; i++)
		if (isctl(curfilename[i]) || strchr(METACHAR, curfilename[i]))
			break;
	if (curfilename[i] && *lenp + strlen2(curfilename) + 2 <= max) {
		insertchar(s, *cxp, *lenp, plen, max, linemax, 1);
		insshift(s, *cxp, (*lenp)++, 1);
		s[(*cxp)++] = quote = '"';
		putcursor(quote, 1);
	}
	i = insertstr(s, *cxp, *lenp, plen, max, linemax, curfilename,
		strlen(curfilename), quote);
	if (!i) {
		putterm(t_bell);
		return(cx2);
	}
	*cxp += i;
	*lenp += i;
	if (quote) {
		insertchar(s, *cxp, *lenp, plen, max, linemax, 1);
		insshift(s, *cxp, (*lenp)++, 1);
		s[(*cxp)++] = quote;
		putcursor(quote, 1);
	}
	cx2 = vlen(s, *cxp);
	if (*cxp < max && !((cx2 + plen) % linemax))
		setcursor(cx2, plen, max, linemax);
	return(cx2);
}

static int _inputstr_input(s, cxp, cx2, lenp, plen, max, linemax, ch)
char *s;
int *cxp, cx2, *lenp, plen, max, linemax, ch;
{
#if	!MSDOS && !defined (_NOKANJICONV)
	char tmpkanji[3];
#endif
	int w, ch2;

	w = 1;
#if	!MSDOS && !defined (_NOKANJICONV)
	if (inputkcode == EUC && ch == 0x8e) {
		ch2 = getkey2(0);
		if (ch2 < 0xa1 || ch2 > 0xdf) {
			putterm(t_bell);
			keyflush();
			return(cx2);
		}
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		insertchar(s, *cxp, *lenp, plen, max, linemax, 1);
		insshift(s, *cxp, *lenp, KANAWID);
		kanjiconv(&(s[*cxp]), tmpkanji, inputkcode, DEFCODE);
		kanjiputs2(&(s[*cxp]), 1, -1);
		*lenp += KANAWID;
		*cxp += KANAWID;
	}
	else
#else
# ifdef	CODEEUC
	if (ch == 0x8e) {
		ch2 = getkey2(0);
		if (ch2 < 0xa1 || ch2 > 0xdf) {
			putterm(t_bell);
			keyflush();
			return(cx2);
		}
		insertchar(s, *cxp, *lenp, plen, max, linemax, 1);
		insshift(s, *cxp, *lenp, KANAWID);
		s[(*cxp)++] = ch;
		s[(*cxp)++] = ch2;
		*lenp += KANAWID;
		putch2(ch);
		putcursor(ch2, 1);
	}
	else
# endif
#endif
	if (isinkanji1(ch)) {
		ch2 = getkey2(0);
		if (*lenp + 1 >= max) {
			putterm(t_bell);
			keyflush();
			return(cx2);
		}
		insertchar(s, *cxp, *lenp, plen, max, linemax, 2);
		insshift(s, *cxp, *lenp, 2);

#if	MSDOS || defined (_NOKANJICONV)
		s[*cxp] = ch;
		s[*cxp + 1] = ch2;
#else
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		kanjiconv(&(s[*cxp]), tmpkanji, inputkcode, DEFCODE);
#endif
		kanjiputs2(&(s[*cxp]), 2, -1);
		*lenp += 2;
		*cxp += 2;
		w = 2;
	}
	else {
		if (ch < ' ' || ch >= K_MIN || ch == C_DEL || *lenp >= max) {
			putterm(t_bell);
			keyflush();
			return(cx2);
		}
		insertchar(s, *cxp, *lenp, plen, max, linemax, 1);
		insshift(s, *cxp, *lenp, 1);
		(*lenp)++;
		s[(*cxp)++] = ch;
		putcursor(ch, 1);
	}
	cx2 = vlen(s, *cxp);
	if (*cxp < max && ((cx2 + plen) % linemax) < w)
		setcursor(cx2, plen, max, linemax);
	return(cx2);
}

static int _inputstr(s, plen, max, linemax, def, comline, h)
char *s;
int plen, max, linemax, def, comline, h;
{
	char *tmphist;
	int len, cx, cx2, ocx2, i, hist, ch, ch2, quote;
#if	!MSDOS
	char *cp;
	int l;
#endif

	subwindow = 1;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif
#ifndef	_NOCOMPLETE
	tmpfilepos = -1;
#endif
	cx = len = strlen(s);
	if (def >= 0 && def < linemax) {
		while (def > len) s[len++] = ' ';
		cx = def;
	}
	displaystr(s, cx, len, plen, max, linemax);
	cx2 = vlen(s, cx);
	keyflush();
	hist = 0;
	tmphist = NULL;
	quote = 0;
	ch = -1;

	do {
		tflush();
		ch2 = ch;
		ocx2 = cx2;
		if (!quote) ch = Xgetkey(0);
		else {
			i = ch = getkey2(0);
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
			if (i < ' ' || i == C_DEL) {
				keyflush();
				ch = '\0';
				if (!i) continue;
				if (len + 1 >= max) {
					putterm(t_bell);
					continue;
				}
				insertchar(s, cx, len, plen,
					max, linemax, 2);
				insshift(s, cx, len, 2);
				len += 2;
				s[cx++] = QUOTE;
				cx2++;
				putcursor('^', 1);
				if (cx < max && !((cx2 + plen) % linemax))
					setcursor(cx2, plen, max, linemax);
				s[cx++] = i;
				cx2++;
				putcursor((i + '@') & 0x7f, 1);
				if (cx < max && !((cx2 + plen) % linemax))
					setcursor(cx2, plen, max, linemax);
				continue;
			}
		}
		switch (ch) {
			case K_RIGHT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (selectlist) {
					i = tmpfilepos;
					tmpfilepos += FILEPERLOW;
					selectfile(i, NULL);
					ocx2 = -1;
				}
				else
#endif
				if (cx >= len) putterm(t_bell);
				else ocx2 = cx2 = rightchar(s, &cx, cx2,
					len, plen, max, linemax);
				break;
			case K_LEFT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (selectlist) {
					i = tmpfilepos;
					tmpfilepos -= FILEPERLOW;
					selectfile(i, NULL);
					ocx2 = -1;
				}
				else
#endif
				if (cx <= 0) putterm(t_bell);
				else ocx2 = cx2 = leftchar(s, &cx, cx2,
					len, plen, max, linemax);
				break;
			case K_BEG:
				keyflush();
				cx = cx2 = 0;
				break;
			case K_EOL:
				keyflush();
				cx = len;
				cx2 = vlen(s, cx);
				break;
			case K_BS:
				keyflush();
				if (cx <= 0) {
					putterm(t_bell);
					break;
				}
				ocx2 = cx2 = leftchar(s, &cx, cx2,
					len, plen, max, linemax);
				len = _inputstr_delete(s, cx,
					len, plen, max, linemax);
				break;
			case K_DC:
				keyflush();
				len = _inputstr_delete(s, cx,
					len, plen, max, linemax);
				break;
			case K_DL:
				keyflush();
				if (cx >= len) putterm(t_bell);
				else {
					truncline(cx2, vlen(s, len), plen,
						max, linemax);
					len = cx;
				}
				break;
			case CTRL('L'):
				keyflush();
				Xlocate(plen, 0);
				putterm(l_clear);
				for (i = 0; i < WCMDLINE; i++) {
					if (ypos + i >= n_line) break;
					locate(xpos, ypos + i);
					putterm(l_clear);
				}
				dispprompt(NULL, 0);
				displaystr(s, cx, len, plen, max, linemax);
				break;
			case K_UP:
				ocx2 = cx2 = _inputstr_up(s, &cx, cx2,
					&len, plen, max, linemax,
					&hist, h, &tmphist);
				break;
			case K_DOWN:
				ocx2 = cx2 = _inputstr_down(s, &cx, cx2,
					&len, plen, max, linemax,
					&hist, h, &tmphist);
				break;
			case K_IL:
				keyflush();
				quote = 1;
				break;
			case K_ENTER:
				ocx2 = cx2 = _inputstr_enter(s, &cx, cx2,
					&len, plen, max, linemax);
				break;
#ifndef	_NOCOMPLETE
			case '\t':
				keyflush();
				if (selectlist) {
					putterm(t_bell);
					break;
				}
				i = completestr(s, cx, len, plen,
					max, linemax, comline,
					(ch2 == ch) ? 1 : 0);
				if (!i) {
					putterm(t_bell);
					break;
				}
				cx += i;
				len += i;
				cx2 = vlen(s, cx);
				if (cx < max && !((cx2 + plen) % linemax))
					ocx2 = -1;
				break;
#endif
			case CR:
				keyflush();
#ifndef	_NOCOMPLETE
				if (!selectlist) break;
				i = completestr(s, cx, len, plen,
					max, linemax, 0, -1);
				cx += i;
				len += i;
				cx2 = vlen(s, cx);
				if (cx < max && !((cx2 + plen) % linemax))
					ocx2 = -1;
				ch = '\0';
#endif
				break;
			case ESC:
				keyflush();
				break;
			default:
				ocx2 = cx2 = _inputstr_input(s, &cx, cx2,
					&len, plen, max, linemax, ch);
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
		if (ocx2 != cx2) setcursor(cx2, plen, max, linemax);
#endif	/* !_NOCOMPLETE */
	} while (ch != ESC && ch != CR);

	subwindow = 0;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif
	if (tmphist) free(tmphist);

	if (ch == ESC) len = 0;
	s[len] = '\0';

	tflush();
	return(ch);
}

int cmdlinelen(plen)
int plen;
{
	int i;

	if (plen < 0) plen = evalprompt(NULL, MAXLINESTR) + 1;
	i = (n_column - 1) * WCMDLINE - plen;
	if (LCMDLINE + WCMDLINE - n_line >= 0) i -= n_column - n_lastcolumn;
	if (i > MAXLINESTR) i = MAXLINESTR;
	return(i);
}

static int dispprompt(s, set)
char *s;
int set;
{
	static char *prompt = NULL;
	char buf[MAXLINESTR + 1];
	int len;

	if (set) prompt = s;
	locate(xpos, ypos);
	putch2(' ');
	putterm(t_standout);
	if (prompt && *prompt) len = kanjiputs(prompt) + 1;
	else {
		len = evalprompt(buf, MAXLINESTR) + 1;
		kanjiputs(buf);
		putterm(t_normal);
	}
	putterm(end_standout);
	return(len);
}

char *inputstr(prompt, delsp, ptr, def, h)
char *prompt;
int delsp, ptr;
char *def;
int h;
{
	char *dupl, input[MAXLINESTR + 1];
	int i, j, len, ch;

	xpos = 0;
	ypos = LCMDLINE;
	for (i = 0; i < WCMDLINE; i++) {
		if (ypos + i >= n_line) break;
		locate(0, ypos + i);
		putterm(l_clear);
	}
	len = dispprompt(prompt, 1);
	tflush();

	if (!def) *input = '\0';
	else {
		j = 0;
		ch = '\0';
		if (!prompt) {
			if (def[i = 0] != '~') for (; def[i]; i++)
				if (isctl(def[i]) || strchr(METACHAR, def[i]))
					break;
			if (def[i]) {
				input[j++] = ch = '"';
				if (ptr) ptr++;
			}
		}
		for (i = 0; def[i]; i++, j++) {
			if (isctl(def[i])) {
				input[j++] = QUOTE;
				if (ptr >= j) ptr++;
			}
			else if (!prompt && strchr(DQ_METACHAR, def[i])) {
#if	MSDOS
				input[j++] = def[i];
#else
				input[j++] = META;
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
	ch = _inputstr(input, len, cmdlinelen(len),
		n_column - 1, ptr, h == 0, h);
	for (i = 0; i < WCMDLINE; i++) {
		if (ypos + i >= n_line) break;
		locate(0, ypos + i);
		putterm(l_clear);
	}
	if (ypos < LCMDLINE) rewritefile(1);

	if (ch == ESC) return(NULL);

	len = strlen(input);
	if (delsp && len > 0 && input[len - 1] == ' ' && yesno(DELSP_K)) {
		for (len--; len > 0 && input[len - 1] == ' '; len--);
		input[len] = '\0';
	}
	tflush();
	dupl = malloc2(len + 1);
	for (i = j = 0; input[i]; i++, j++) {
		if (input[i] == QUOTE) i++;
		dupl[j] = input[i];
	}
	dupl[j] = '\0';
	if (h == 0) entryhist(h, dupl, 0);
	return(dupl);
}

static char *truncstr(s)
char *s;
{
	int len;
	char *cp1, *cp2;

	if ((len = (int)strlen2(s) + 5 - n_lastcolumn) <= 0
	|| !(cp1 = strchr2(s, '['))
	|| !(cp2 = strchr2(cp1, ']'))) return(s);

	cp1++;
	len = cp2 - cp1 - len;
	if (len <= 0) len = 0;
	else if (onkanji1(cp1, vlen(cp1, len) - 1)
#ifdef	CODEEUC
	|| isekana(cp1, len - 1)
#endif
	) len--;
	strcpy(cp1 + len, cp2);
	return(s);
}

static VOID yesnomes(mes)
char *mes;
{
	locate(0, LMESLINE);
	putterm(l_clear);
	putterm(t_standout);
	kanjiputs2(mes, n_lastcolumn, -1);
	cputs2("[Y/N]");
	putterm(end_standout);
	tflush();
}

#if	MSDOS || defined (__STDC__)
/*VARARGS1*/
int yesno(CONST char *fmt, ...)
#else
# ifndef	NOVSPRINTF
/*VARARGS1*/
int yesno(fmt, va_alist)
char *fmt;
va_dcl
# else
int yesno(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
# endif
#endif
{
#if	MSDOS || defined (__STDC__) || !defined (NOVSPRINTF)
	va_list args;
#endif
	int len, ch, duperrno, ret = 1;
	char buf[MAXLINESTR + 1];

	duperrno = errno;
	subwindow = 1;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif

#if	MSDOS || defined (__STDC__)
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
#else
# ifndef	NOVSPRINTF
	va_start(args);
	vsprintf(buf, fmt, args);
	va_end(args);
# else
	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
# endif
#endif

	truncstr(buf);
	len = strlen2(buf);
	yesnomes(buf);

	do {
		keyflush();
		locate(len + 1 + (1 - ret) * 2, LMESLINE);
		tflush();
		switch (ch = Xgetkey(0)) {
			case 'y':
			case 'Y':
				ret = 1;
				ch = CR;
				break;
			case 'n':
			case 'N':
			case ' ':
			case ESC:
				ret = 0;
				ch = CR;
				break;
			case K_RIGHT:
				ret = 0;
				break;
			case K_LEFT:
				ret = 1;
				break;
			case CTRL('L'):
				yesnomes(buf);
				break;
			default:
				break;
		}
	} while (ch != CR);

	subwindow = 0;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif

	locate(0, LMESLINE);
	putterm(l_clear);

	tflush();
	errno = duperrno;
	return(ret);
}

VOID warning(no, s)
int no;
char *s;
{
	char *tmp, *err;
	int len;

	subwindow = 1;

	if (no < 0) no = errno;
	err = (char *)sys_errlist[no];
	if (!s) tmp = err;
	else if (!no) {
		tmp = s;
		s = NULL;
	}
	else {
		len = n_lastcolumn - (int)strlen(err) - 3;
		tmp = (char *)malloc2(strlen2(s) + strlen(err) + 3);
		strncpy3(tmp, s, &len, -1);
		strcat(tmp, ": ");
		strcat(tmp, err);
	}
	putterm(t_bell);

	locate(0, LMESLINE);
	putterm(l_clear);
	putterm(t_standout);
	kanjiputs(tmp);
	putterm(end_standout);
	tflush();

	keyflush();
	getkey2(SIGALRM);
	subwindow = 0;

	if (no) rewritefile(1);
	else {
		locate(0, LMESLINE);
		putterm(l_clear);
		tflush();
	}

	if (s) free(tmp);
}

static int selectmes(num, max, x, str, val, xx)
int num, max, x;
char *str[];
int val[], xx[];
{
	int i, new;

	locate(x, LMESLINE);
	putterm(l_clear);
	new = -1;
	for (i = 0; i < max; i++) {
		locate(x + xx[i] + 1, LMESLINE);
		if (val[i] != num) kanjiputs(str[i]);
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
	int i, ch, old, new, xx[MAXSELECTSTRS], initial[MAXSELECTSTRS];

	subwindow = 1;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif

	xx[0] = 0;
	for (i = 0; i < max; i++) {
		initial[i] = (*str[i] >= 'A' && *str[i] <= 'Z') ? *str[i] : -1;
		xx[i + 1] = xx[i] + strlen(str[i]) + 1;
	}
	new = selectmes(*num, max, x, str, val, xx);

	do {
		locate(x + xx[new + 1], LMESLINE);
		tflush();
		old = new;

		keyflush();
		switch (ch = Xgetkey(SIGALRM)) {
			case K_RIGHT:
				if (old < max - 1) new = old + 1;
				else new = 0;
				break;
			case K_LEFT:
				if (old > 0) new = old - 1;
				else new = max - 1;
				break;
			case CTRL('L'):
				selectmes(val[new], max, x, str, val, xx);
				break;
			default:
				for (i = 0; i < max; i++)
					if (toupper2(ch) == initial[i]) {
						new = i;
						ch = CR;
						break;
					}
				break;
		}
		if (new != old) {
			locate(x + xx[new] + 1, LMESLINE);
			putterm(t_standout);
			kanjiputs(str[new]);
			putterm(end_standout);
			locate(x + xx[old] + 1, LMESLINE);
			if (stable_standout) putterm(end_standout);
			else kanjiputs(str[old]);
		}
	} while (ch != ESC && ch != CR);

	subwindow = 0;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif

	if (stable_standout) {
		locate(x + 1, LMESLINE);
		putterm(l_clear);
	}
	for (i = 0; i < max; i++) {
		locate(x + xx[i] + 1, LMESLINE);
		if (i == new) kanjiputs(str[i]);
		else cprintf2("%*s", (int)strlen(str[i]), " ");
	}
	if (ch != ESC) *num = val[new];
	tflush();
	return(ch);
}
