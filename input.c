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
#ifndef	_NOARCHIVE
extern char *archivefile;
#endif
extern int columns;
extern int filepos;
extern int sorton;
extern int minfilename;
#if	!MSDOS && !defined (_NOKANJICONV)
extern int inputkcode;
#endif
extern int sizeinfo;
#ifndef	DECLERRLIST
extern char *sys_errlist[];
#endif

static int trquote __P_((char *, int));
#ifdef	CODEEUC
static int vlen __P_((char *, int));
static int rlen __P_((char *, int));
#else
#define	vlen(str, len)	(len)
#define	rlen(str, len)	(len)
#endif
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
static VOID selectfile __P_((char *, int));
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
#endif
static int tmpfilepos = 0;
static int xpos = 0;
static int ypos = 0;

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
				tflush();
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
					tflush();
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

static int trquote(str, cx)
char *str;
int cx;
{
	if (str[cx] == QUOTE) return('^');
	else if (cx > 0 && (str[cx - 1] == QUOTE))
		return((str[cx] + '@') & 0x7f);
	else return(str[cx]);
}

#ifdef	CODEEUC
static int vlen(str, len)
char *str;
int len;
{
	int v, r;

	for (v = r = 0; r < len; v++, r++) if (isekana(str, r)) r++;
	return(v);
}

static int rlen(str, len)
char *str;
int len;
{
	int v, r;

	for (v = r = 0; v < len; v++, r++) if (isekana(str, r)) r++;
	return(r);
}
#endif

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
	if (f) putterm(c_right);
}

static int rightchar(str, cxp, cx2, len, plen, max, linemax)
char *str;
int *cxp, cx2, len, plen, max, linemax;
{
	if (!iskanji1(str[*cxp]) && str[*cxp] != QUOTE) {
#ifdef	CODEEUC
		if (isekana(str, *cxp)) (*cxp)++;
#endif
		(*cxp)++;
		cx2++;
		if (*cxp < max && (cx2 + plen) % linemax < 1)
			setcursor(cx2, plen, max, linemax);
		else putterm(c_right);
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
			putterm(c_right);
			putterm(c_right);
		}
	}
	return(cx2);
}

/*ARGSUSED*/
static int leftchar(str, cxp, cx2, len, plen, max, linemax)
char *str;
int *cxp, cx2, len, plen, max, linemax;
{
#ifdef	CODEEUC
	if (*cxp >= 2 && isekana(str, *cxp - 2)) {
		*cxp -= 2;
		if (((cx2--) + plen) % linemax < 1)
			setcursor(cx2, plen, max, linemax);
		else putterm(c_left);
	}
	else
#endif
	if (cx2 < 2 || (!onkanji1(str, cx2 - 2) && str[*cxp - 2] != QUOTE)) {
		(*cxp)--;
		if (((cx2--) + plen) % linemax < 1)
			setcursor(cx2, plen, max, linemax);
		else putterm(c_left);
	}
	else {
		(*cxp) -= 2;
		cx2 -= 2;
		if ((cx2 + 2 + plen) % linemax < 2)
			setcursor(cx2, plen, max, linemax);
		else {
			putterm(c_left);
			putterm(c_left);
		}
	}
	return(cx2);
}

static VOID insertchar(str, cx, len, plen, max, linemax, ins)
char *str;
int cx, len, plen, max, linemax, ins;
{
	char dupl[MAXLINESTR + 1];
	int dx, dy, i, j, l, f1, ptr;
#if	!MSDOS
	int f2;
#endif

	for (i = 0; i < len - cx; i++) dupl[i] = trquote(str, i + cx);
	dupl[i] = '\0';
#ifdef	CODEEUC
	cx = vlen(str, cx);
	len = vlen(str, len);
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
						putterm(c_right);
						j--;
					}
					while (j--) putch2(' ');
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
		while (j--) putch2(' ');
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

static VOID deletechar(str, cx, len, plen, max, linemax, del)
char *str;
int cx, len, plen, max, linemax, del;
{
	char dupl[MAXLINESTR + 1];
	int dy, i, j, l, f1, ptr;
#if	!MSDOS
	int f2;
#endif

	for (i = 0; i < len - cx; i++) dupl[i] = trquote(str, i + cx);
	dupl[i] = '\0';
#ifdef	CODEEUC
	cx = vlen(str, cx);
	len = vlen(str, len);
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

static VOID insshift(str, cx, len, ins)
char *str;
int cx, len, ins;
{
	int i;

	for (i = len - 1; i >= cx; i--) str[i + ins] = str[i];
}

static VOID delshift(str, cx, len, del)
char *str;
int cx, len, del;
{
	int i;

	for (i = cx; i < len - del; i++) str[i] = str[i + del];
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

static VOID displaystr(str, cx, len, plen, max, linemax)
char *str;
int cx, len, plen, max, linemax;
{
	char *dupl;
	int i, y, width;

	Xlocate(plen, 0);
	dupl = (char *)malloc2(len + 1);
	for (i = 0; i < len; i++) dupl[i] = trquote(str, i);
	dupl[i] = '\0';
#ifdef	CODEEUC
	cx = vlen(str, cx);
	len = vlen(str, len);
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

static int insertstr(str, cx, len, plen, max, linemax, insstr, ins, quote)
char *str;
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
	insertchar(str, cx, len, plen, max, linemax, vlen(insstr, ins));
	insshift(str, cx, len, ins);
	for (i = j = 0; i < ins; i++, j++) {
		if ((u_char)(insstr[i]) >= ' ' && insstr[i] != C_DEL
		&& (
#if	!MSDOS
		quote != '"' ||
#endif
		!strchr(DQ_METACHAR, insstr[i])))
			dupl[j] = str[cx + j] = insstr[i];
		else if (len + ins + j - i >= max)
			dupl[j] = str[cx + j] = '?';
		else if ((u_char)(insstr[i]) >= ' ' && insstr[i] != C_DEL) {
#if	MSDOS
			dupl[j] = str[cx + j] =
#else
			dupl[j] = str[cx + j] = '\\';
#endif
			dupl[j + 1] = str[cx + j + 1] = insstr[i];
			j++;
		}
		else {
			dupl[j] = '^';
			str[cx + j] = QUOTE;
			dupl[++j] = (insstr[i] + '@') & 0x7f;
			str[cx + j] = insstr[i];
		}
	}
	dupl[j] = '\0';

#ifdef	CODEEUC
	cx = vlen(str, cx);
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
static VOID selectfile(strs, max)
char *strs;
int max;
{
	static int maxselect, tmpcolumns;
	int i, len, maxlen, dupminfilename, dupcolumns, dupsorton;

	dupminfilename = minfilename;
	dupcolumns = columns;
	minfilename = n_column;

	if (strs) {
		selectlist = (namelist *)malloc2(max * sizeof(namelist));
		maxlen = 0;
		for (i = 0; i < max; i++) {
			memset(&selectlist[i], 0, sizeof(namelist));
			selectlist[i].name = strdup2(strs);
			selectlist[i].flags = (F_ISRED | F_ISWRI);
			len = strlen3(strs);
			if (maxlen < len) maxlen = len;
			while (*strs++);
		}
		dupsorton = sorton;
		sorton = 1;
		qsort(selectlist, max, sizeof(namelist), cmplist);
		sorton = dupsorton;

		if ((n_column / 5) - 2 - 1 >= maxlen) tmpcolumns = 5;
		else if ((n_column / 3) - 2 - 1 >= maxlen) tmpcolumns = 3;
		else if ((n_column / 2) - 2 - 1 >= maxlen) tmpcolumns = 2;
		else tmpcolumns = 1;
		columns = tmpcolumns;
		tmpfilepos = listupfile(selectlist, max, NULL);
		maxselect = max;
	}
	else if (max < 0) {
		for (i = 0; i < maxselect; i++) free(selectlist[i].name);
		free(selectlist);
		selectlist = NULL;
	}
	else {
		columns = tmpcolumns;
		if (tmpfilepos >= maxselect) tmpfilepos %= (tmpfilepos - max);
		else if (tmpfilepos < 0) tmpfilepos = maxselect - 1
			- ((maxselect - 1 - max) % (max - tmpfilepos));
		if (max / FILEPERPAGE != tmpfilepos / FILEPERPAGE)
			tmpfilepos = listupfile(selectlist, maxselect,
				selectlist[tmpfilepos].name);
		else if (max != tmpfilepos) {
			putname(selectlist, max, -1);
			putname(selectlist, tmpfilepos, 1);
		}
	}

	minfilename = dupminfilename;
	columns = dupcolumns;
}

static int completestr(str, cx, len, plen, max, linemax, comline, cont)
char *str;
int cx, len, plen, max, linemax, comline, cont;
{
	char *cp1, *cp2, *match;
	int i, l, ins, top, fix, quote;

	if (selectlist && cont > 0) {
		selectfile(NULL, tmpfilepos++);
		setcursor(vlen(str, cx), plen, max, linemax);
		return(0);
	}

	quote = '\0';
	for (i = top = 0; i < cx; i++) {
		if (quote) {
			if (str[i] == quote) quote = '\0';
		}
		else if (str[i] == '"'
#if	!MSDOS
		|| str[i] == '\''
#endif
		) quote = str[i];
		else if (strchr(CMDLINE_DELIM, str[i])) top = i + 1;
	}
	if (top > 1 && str[top - 1] == ':' && isalpha(str[top - 2])) top -= 2;
	if (top == cx) {
		putterm(t_bell);
		return(0);
	}
	if (top > 0) comline = 0;

	cp1 = (char *)malloc2(cx - top + 1);
	strncpy2(cp1, str + top, cx - top);
	cp2 = evalpath(cp1, 1);

	if (selectlist && cont < 0) {
		match = strdup2(selectlist[tmpfilepos].name);
		i = 1;
	}
	else {
		match = NULL;
		i = 0;
		if (comline) {
			i = completeuserfunc(cp2, i, &match);
			i = completealias(cp2, i, &match);
			i = completebuiltin(cp2, i, &match);
		}
		i = completepath(cp2, i, &match, comline, 0);
		if (!i && comline) {
			match = NULL;
			i = completepath(cp2, 0, &match, 0, 0);
		}
	}

	if ((cp1 = strrdelim(cp2, 1))) cp1++;
#if	MSDOS || !defined (_NODOSDRIVE)
	else if (_dospath(cp2)) cp1 = cp2 + 2;
#endif
	else cp1 = cp2;
	ins = strlen(cp1);
	free(cp2);
	if (!i) {
		putterm(t_bell);
		if (match) free(match);
		return(0);
	}

	cp1 = findcommon(match, i);
	fix = 0;
	if (i == 1 && cp1 && !isdelim(cp1, (int)strlen(cp1) - 1)) fix++;

	if (!cp1 || ((ins = (int)strlen(cp1) - ins) <= 0 && !fix)) {
		if (cont <= 0) putterm(t_bell);
		else {
			selectfile(match, i);
			setcursor(vlen(str, cx), plen, max, linemax);
		}
		free(match);
		if (cp1) free(cp1);
		return(0);
	}
	free(match);

	l = 0;
	if (!quote && len < max) {
		for (i = 0; cp1[i]; i++)
			if ((u_char)(cp1[i]) < ' ' || cp1[i] == C_DEL
			|| strchr(METACHAR, cp1[i])) break;
		if (cp1[i]) {
			setcursor(vlen(str, top), plen, max, linemax);
			insertchar(str, top, len, plen, max, linemax, 1);
			insshift(str, top, len++, 1);
			l++;
			str[top] = quote = '"';
			putch2(quote);
			setcursor(vlen(str, ++cx), plen, max, linemax);
		}
	}

	cp2 = cp1 + (int)strlen(cp1) - ins;
	i = insertstr(str, cx, len, plen, max, linemax, cp2, ins, quote);
	l += i;
	if (fix && (len += i) < max) {
		cx += i;
		if (quote && len + 1 < max) {
			insertchar(str, cx, len, plen, max, linemax, 1);
			insshift(str, cx, len++, 1);
			l++;
			str[cx++] = quote;
			putch2(quote);
		}
		insertchar(str, cx, len, plen, max, linemax, 1);
		insshift(str, cx, len, 1);
		l++;
		str[cx] = ' ';
		putch2(' ');
	}

	free(cp1);
	return(l);
}
#endif	/* !_NOCOMPLETE */

static int _inputstr_up(str, cxp, cx2, lenp, plen, max, linemax,
	histnop, h, tmp)
char *str;
int *cxp, cx2, *lenp, plen, max, linemax, *histnop, h;
char **tmp;
{
	int i, j;

	keyflush();
#ifndef	_NOCOMPLETE
	if (selectlist) {
		selectfile(NULL, tmpfilepos--);
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
			str[*lenp] = '\0';
			*tmp = strdup2(str);
		}
		for (i = j = 0; history[h][*histnop][i]; i++) {
			if ((u_char)(history[h][*histnop][i]) < ' '
			|| history[h][*histnop][i] == C_DEL) str[j++] = QUOTE;
			str[j++] = history[h][*histnop][i];
		}
		str[j] = '\0';
		*lenp = strlen(str);
		*cxp = *lenp;
		displaystr(str, *cxp, *lenp, plen, max, linemax);
		cx2 = vlen(str, *cxp);
		(*histnop)++;
	}
	else {
		if (*cxp >= max && !((cx2 + plen) % linemax)) {
			putterm(c_left);
			cx2--;
		}
		cx2 -= linemax;
		*cxp = rlen(str, cx2);
		putterm(c_up);
		if (onkanji1(str, cx2 - 1)) {
			putterm(c_left);
			(*cxp)--;
		}
	}
	return(cx2);
}

static int _inputstr_down(str, cxp, cx2, lenp, plen, max, linemax,
	histnop, h, tmp)
char *str;
int *cxp, cx2, *lenp, plen, max, linemax, *histnop, h;
char **tmp;
{
	int i, j, len;

	len = vlen(str, *lenp);
	keyflush();
#ifndef	_NOCOMPLETE
	if (selectlist) {
		selectfile(NULL, tmpfilepos++);
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
				if ((u_char)(history[h][*histnop - 1][i]) < ' '
				|| history[h][*histnop - 1][i] == C_DEL)
					str[j++] = QUOTE;
				str[j++] = history[h][*histnop - 1][i];
			}
			str[j] = '\0';
		}
		else {
			strcpy(str, *tmp);
			free(*tmp);
			*tmp = NULL;
		}
		*lenp = strlen(str);
		*cxp = *lenp;
		displaystr(str, *cxp, *lenp, plen, max, linemax);
		cx2 = vlen(str, *cxp);
	}
	else {
		cx2 += linemax;
		*cxp = rlen(str, cx2);
		putterm(c_down);
		if (onkanji1(str, cx2 - 1)) {
			putterm(c_left);
			(*cxp)--;
		}
	}
	return(cx2);
}

static int _inputstr_delete(str, cx, len, plen, max, linemax)
char *str;
int cx, len, plen, max, linemax;
{
	int n, n2;

	if (cx >= len) {
		putterm(t_bell);
		return(len);
	}
#ifdef	CODEEUC
	if (isekana(str, cx)) {
		n = 2;
		n2 = 1;
	}
	else
#endif
	if (str[cx] != QUOTE && !iskanji1(str[cx])) n = n2 = 1;
	else {
		if (cx + 1 >= len) {
			putterm(t_bell);
			return(len);
		}
		n = n2 = 2;
	}
	deletechar(str, cx, len, plen, max, linemax, n2);
	delshift(str, cx, len, n);
	return(len -= n);
}

static int _inputstr_enter(str, cxp, cx2, lenp, plen, max, linemax)
char *str;
int *cxp, cx2, *lenp, plen, max, linemax;
{
	int i, quote;

	quote = 0;
	keyflush();
	for (i = 0; curfilename[i]; i++)
		if ((u_char)(curfilename[i]) < ' '
		|| curfilename[i] == C_DEL
		|| strchr(METACHAR, curfilename[i]))
			break;
	if (curfilename[i] && *lenp + strlen2(curfilename) + 2 <= max) {
		insertchar(str, *cxp, *lenp, plen, max, linemax, 1);
		insshift(str, *cxp, (*lenp)++, 1);
		str[(*cxp)++] = quote = '"';
		putch2(quote);
	}
	i = insertstr(str, *cxp, *lenp, plen, max, linemax, curfilename,
		strlen(curfilename), quote);
	if (!i) {
		putterm(t_bell);
		return(cx2);
	}
	*cxp += i;
	*lenp += i;
	if (quote) {
		insertchar(str, *cxp, *lenp, plen, max, linemax, 1);
		insshift(str, *cxp, (*lenp)++, 1);
		str[(*cxp)++] = quote;
		putch2(quote);
	}
	cx2 = vlen(str, *cxp);
	if (*cxp < max && !((cx2 + plen) % linemax))
		setcursor(cx2, plen, max, linemax);
	return(cx2);
}

static int _inputstr_input(str, cxp, cx2, lenp, plen, max, linemax, ch)
char *str;
int *cxp, cx2, *lenp, plen, max, linemax, ch;
{
#if	!MSDOS && !defined (_NOKANJICONV)
	char tmpkanji[3];
#endif
	int ch2;

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
		insertchar(str, *cxp, *lenp, plen, max, linemax, 1);
#ifdef	CODEEUC
		insshift(str, *cxp, *lenp, 2);
		kanjiconv(&str[*cxp], tmpkanji, inputkcode, DEFCODE);
		kanjiputs2(&str[*cxp], 1, -1);
		*lenp += 2;
		*cxp += 2;
#else
		insshift(str, *cxp, *lenp, 1);
		kanjiconv(&str[*cxp], tmpkanji, inputkcode, DEFCODE);
		kanjiputs2(&str[*cxp], 1, -1);
		*lenp += 1;
		*cxp += 1;
#endif
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
		insertchar(str, *cxp, *lenp, plen, max, linemax, 1);
		insshift(str, *cxp, *lenp, 2);
		str[(*cxp)++] = ch;
		str[(*cxp)++] = ch2;
		*lenp += 2;
		putch2(ch);
		putch2(ch2);
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
		insertchar(str, *cxp, *lenp, plen, max, linemax, 2);
		insshift(str, *cxp, *lenp, 2);

#if	MSDOS || defined (_NOKANJICONV)
		str[*cxp] = ch;
		str[*cxp + 1] = ch2;
#else
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		kanjiconv(&str[*cxp], tmpkanji, inputkcode, DEFCODE);
#endif
		kanjiputs2(&str[*cxp], 2, -1);
		*lenp += 2;
		*cxp += 2;
	}
	else {
		if (ch < ' ' || ch >= K_MIN || ch == C_DEL || *lenp >= max) {
			putterm(t_bell);
			keyflush();
			return(cx2);
		}
		insertchar(str, *cxp, *lenp, plen, max, linemax, 1);
		insshift(str, *cxp, *lenp, 1);
		(*lenp)++;
		str[(*cxp)++] = ch;
		putch2(ch);
	}
	cx2 = vlen(str, *cxp);
	if (*cxp < max && !((cx2 + plen) % linemax))
		setcursor(cx2, plen, max, linemax);
	return(cx2);
}

static int _inputstr(str, plen, max, linemax, def, comline, h)
char *str;
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
	tmpfilepos = -1;
	cx = len = strlen(str);
	if (def >= 0 && def < linemax) {
		while (def > len) str[len++] = ' ';
		cx = def;
	}
	displaystr(str, cx, len, plen, max, linemax);
	cx2 = vlen(str, cx);
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
				insertchar(str, cx, len, plen,
					max, linemax, 2);
				insshift(str, cx, len, 2);
				len += 2;
				str[cx++] = QUOTE;
				cx2++;
				putch2('^');
				if (cx < max && !((cx2 + plen) % linemax))
					setcursor(cx2, plen, max, linemax);
				str[cx++] = i;
				cx2++;
				putch2((i + '@') & 0x7f);
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
					selectfile(NULL, i);
					ocx2 = -1;
				}
				else
#endif
				if (cx >= len) putterm(t_bell);
				else ocx2 = cx2 = rightchar(str, &cx, cx2,
					len, plen, max, linemax);
				break;
			case K_LEFT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (selectlist) {
					i = tmpfilepos;
					tmpfilepos -= FILEPERLOW;
					selectfile(NULL, i);
					ocx2 = -1;
				}
				else
#endif
				if (cx <= 0) putterm(t_bell);
				else ocx2 = cx2 = leftchar(str, &cx, cx2,
					len, plen, max, linemax);
				break;
			case K_BEG:
				keyflush();
				cx = cx2 = 0;
				break;
			case K_EOL:
				keyflush();
				cx = len;
				cx2 = vlen(str, cx);
				break;
			case K_BS:
				keyflush();
				if (cx <= 0) {
					putterm(t_bell);
					break;
				}
				ocx2 = cx2 = leftchar(str, &cx, cx2,
					len, plen, max, linemax);
				len = _inputstr_delete(str, cx,
					len, plen, max, linemax);
				break;
			case K_DC:
				keyflush();
				len = _inputstr_delete(str, cx,
					len, plen, max, linemax);
				break;
			case K_DL:
				keyflush();
				if (cx >= len) putterm(t_bell);
				else {
					truncline(cx2, vlen(str, len), plen,
						max, linemax);
					len = cx;
				}
				break;
			case CTRL('L'):
				keyflush();
				Xlocate(plen, 0);
				putterm(l_clear);
				for (i = 1; i < WCMDLINE; i++) {
					if (ypos + i >= n_line) break;
					locate(xpos, ypos + i);
					putterm(l_clear);
				}
				displaystr(str, cx, len, plen, max, linemax);
				break;
			case K_UP:
				ocx2 = cx2 = _inputstr_up(str, &cx, cx2,
					&len, plen, max, linemax,
					&hist, h, &tmphist);
				break;
			case K_DOWN:
				ocx2 = cx2 = _inputstr_down(str, &cx, cx2,
					&len, plen, max, linemax,
					&hist, h, &tmphist);
				break;
			case K_IL:
				keyflush();
				quote = 1;
				break;
			case K_ENTER:
				ocx2 = cx2 = _inputstr_enter(str, &cx, cx2,
					&len, plen, max, linemax);
				break;
#ifndef	_NOCOMPLETE
			case '\t':
				keyflush();
				if (selectlist) {
					putterm(t_bell);
					break;
				}
				i = completestr(str, cx, len, plen,
					max, linemax, comline,
					(ch2 == ch) ? 1 : 0);
				if (!i) {
					putterm(t_bell);
					break;
				}
				cx += i;
				len += i;
				cx2 = vlen(str, cx);
				if (cx < max && !((cx2 + plen) % linemax))
					ocx2 = -1;
				break;
#endif
			case CR:
				keyflush();
#ifndef	_NOCOMPLETE
				if (!selectlist) break;
				i = completestr(str, cx, len, plen,
					max, linemax, 0, -1);
				cx += i;
				len += i;
				cx2 = vlen(str, cx);
				if (cx < max && !((cx2 + plen) % linemax))
					ocx2 = -1;
				ch = '\0';
#endif
				break;
			case ESC:
				keyflush();
				break;
			default:
				ocx2 = cx2 = _inputstr_input(str, &cx, cx2,
					&len, plen, max, linemax, ch);
				break;
		}
#ifndef	_NOCOMPLETE
		if (selectlist && ch != '\t'
		&& ch != K_RIGHT && ch != K_LEFT
		&& ch != K_UP && ch != K_DOWN) {
			selectfile(NULL, -1);
#ifndef	_NOARCHIVE
			if (archivefile) rewritearc(0);
			else
#endif
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
	str[len] = '\0';

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
	locate(xpos, ypos);
	putch2(' ');
	putterm(t_standout);
	if (prompt && *prompt) {
		kanjiputs(prompt);
		len = strlen(prompt) + 1;
	}
	else {
		len = evalprompt(input, MAXLINESTR) + 1;
		kanjiputs(input);
		putterm(t_normal);
	}
	putterm(end_standout);
	tflush();

	if (!def) *input = '\0';
	else {
		j = 0;
		ch = '\0';
		if (!prompt) {
			for (i = 0; def[i]; i++)
				if ((u_char)(def[i]) < ' ' || def[i] == C_DEL
				|| strchr(METACHAR, def[i])) break;
			if (def[i]) {
				input[j++] = ch = '"';
				if (ptr) ptr++;
			}
		}
		for (i = 0; def[i]; i++, j++) {
			if ((u_char)(def[i]) < ' ' || def[i] == C_DEL) {
				input[j++] = QUOTE;
				if (ptr >= j) ptr++;
			}
			else if (!prompt && strchr(DQ_METACHAR, def[i]))  {
#if	MSDOS
				input[j++] = def[i];
#else
				input[j++] = '\\';
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
	if (ypos < LCMDLINE) {
		title();
#ifndef	_NOARCHIVE
		if (archivefile) rewritearc(1);
		else
#endif
		rewritefile(1);
	}

	if (ch == ESC) return(NULL);

	len = strlen(input);
	if (delsp && len > 0 && input[len - 1] == ' ' && yesno(DELSP_K)) {
		for (len--; len > 0 && input[len - 1] == ' '; len--);
		input[len] = '\0';
	}
	tflush();
	dupl = (char *)malloc2(len + 1);
	for (i = j = 0; input[i]; i++, j++) {
		if (input[i] == QUOTE) i++;
		dupl[j] = input[i];
	}
	dupl[j] = '\0';
	if (h == 0) entryhist(h, dupl, 0);
	return(dupl);
}

static char *truncstr(str)
char *str;
{
	int len;
	char *cp1, *cp2;

	if ((len = (int)strlen2(str) + 5 - n_lastcolumn) <= 0
	|| !(cp1 = strchr2(str, '['))
	|| !(cp2 = strchr2(cp1, ']'))) return(str);

	cp1++;
	len = cp2 - cp1 - len;
	if (len <= 0) len = 0;
	else if (onkanji1(cp1, vlen(cp1, len) - 1)
#ifdef	CODEEUC
	|| isekana(cp1, len - 1)
#endif
	) len--;
	strcpy(cp1 + len, cp2);
	return(str);
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
{
	va_list args;
	int len, ch, ret = 1;
	char buf[MAXLINESTR + 1];

	subwindow = 1;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
#else	/* !MSDOS && !__STDC__ */
# ifndef	NOVSPRINTF
/*VARARGS1*/
int yesno(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list args;
	int len, ch, ret = 1;
	char buf[MAXLINESTR + 1];

	subwindow = 1;
	va_start(args);
	vsprintf(buf, fmt, args);
	va_end(args);
# else	/* NOVSPRINTF */
int yesno(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
{
	int len, ch, ret = 1;
	char buf[MAXLINESTR + 1];

	subwindow = 1;
	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
# endif	/* NOVSPRINTF */
#endif	/* !MSDOS  && !__STDC__ */
#ifndef	_NOEDITMODE
	Xgetkey(-1);
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
	return(ret);
}

VOID warning(no, str)
int no;
char *str;
{
	char *tmp, *err;
	int len;

	subwindow = 1;
	if (no < 0) no = errno;
	err = (char *)sys_errlist[no];
	if (!str) tmp = err;
	else if (!no) {
		tmp = str;
		str = NULL;
	}
	else {
		len = n_lastcolumn - (int)strlen(err) - 3;
		tmp = (char *)malloc2(strlen2(str) + strlen(err) + 3);
		strncpy3(tmp, str, &len, -1);
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

	if (!no) {
		locate(0, LMESLINE);
		putterm(l_clear);
		tflush();
	}
	else
#ifndef	_NOARCHIVE
	if (archivefile) rewritearc(1);
	else
#endif
		rewritefile(1);

	if (str) free(tmp);
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
	int i, ch, old, new, xx[10], initial[10];

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
