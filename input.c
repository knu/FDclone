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

#if	MSDOS
#include <stdarg.h>
#include "unixemu.h"
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

static int trquote();
static int rightchar();
static int leftchar();
static VOID insertchar();
static VOID deletechar();
static VOID truncline();
static VOID displaystr();
static int insertstr();
#ifndef	_NOCOMPLETE
static VOID selectfile();
static int completestr();
#endif
static int _inputstr_up();
static int _inputstr_down();
static int _inputstr_input();
static int _inputstr();
static char *truncstr();
static VOID yesnomes();
static int selectmes();

int subwindow;
char *curfilename;
#ifndef	_NOEDITMODE
char *editmode = NULL;
#endif
#ifndef	_NOEDITMODE

static int emulatekey[] = {
	K_UP, K_DOWN, K_RIGHT, K_LEFT,
	K_IC, K_DC, K_IL, K_DL,
	K_HOME, K_END, K_BEG, K_EOL,
	K_PPAGE, K_NPAGE, K_ENTER, ESC,
};
static char emacskey[] = {
	CTRL('P'), CTRL('N'), CTRL('F'), CTRL('B'),
	ESC, CTRL('D'), CTRL('Q'), CTRL('K'),
	ESC, ESC, CTRL('A'), CTRL('E'),
	CTRL('V'), CTRL('Y'), CTRL('O'), CTRL('G'),
};
static char vikey[] = {
	'k', 'j', 'l', 'h',
	ESC, 'x', ESC, 'D',
	'g', 'G', '0', '$',
	CTRL('B'), CTRL('F'), 'o', ESC,
};
static char wordstarkey[] = {
	CTRL('E'), CTRL('X'), CTRL('D'), CTRL('S'),
	CTRL('V'), CTRL('G'), CTRL(']'), CTRL('Y'),
	CTRL('W'), CTRL('Z'), CTRL('A'), CTRL('F'),
	CTRL('R'), CTRL('C'), CTRL('N'), ESC,
};
#endif

#ifndef	_NOCOMPLETE
static namelist *selectlist = NULL;
#endif
static int tmpfilepos;

int getkey2(sig)
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

	ch = _getkey2(sig);

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
	} while ((!(vimode & 1)) && (ch = _getkey2(sig)));
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
		return((str[cx] == C_DEL) ? '?' : str[cx] + '@');
	else return(str[cx]);
}

static int rightchar(str, x, cx, len, linemax, max)
char *str;
int x, cx, len, linemax, max;
{
	int i;

	if (iskanji1(str[cx])) {
		if (cx + 1 >= len) {
			putterm(t_bell);
			return(cx);
		}
		cx += 2;
		i = cx % linemax;
		if (i <= 1 && cx < max) locate(x + i, LCMDLINE + cx / linemax);
		else {
			putterm(c_right);
			putterm(c_right);
		}
	}
	else if (str[cx] == QUOTE) {
		cx += 2;
		if ((cx % linemax) > 1 || cx >= max) {
			putterm(c_right);
			putterm(c_right);
		}
		else locate(x + (cx % linemax), LCMDLINE + cx / linemax);
	}
	else {
		if (++cx % linemax || cx >= max) putterm(c_right);
		else locate(x, LCMDLINE + cx / linemax);
	}
	return(cx);
}

/*ARGSUSED*/
static int leftchar(str, x, cx, len, linemax, max)
char *str;
int x, cx, len, linemax, max;
{
	int i;

	if (onkanji1(str, cx - 2)) {
		i = cx % linemax;
		cx -= 2;
		if (i < 2) locate(x + linemax + i - 2, LCMDLINE + cx / linemax);
		else {
			putterm(c_left);
			putterm(c_left);
		}
	}
	else if (cx > 1 && str[cx - 2] == QUOTE) {
		cx -= 2;
		if (((cx + 2) % linemax) > 1) {
			putterm(c_left);
			putterm(c_left);
		}
		else locate(x + (cx % linemax), LCMDLINE + cx / linemax);
	}
	else {
		if (cx-- % linemax) putterm(c_left);
		else locate(x + linemax - 1, LCMDLINE + cx / linemax);
	}
	return(cx);
}

static VOID insertchar(str, x, cx, len, linemax, ins)
char *str;
int x, cx, len, linemax, ins;
{
	char dupl[MAXLINESTR + 1];
	int dy, i, j, l, f1, f2, ptr;

	for (i = 0; i < len - cx; i++) dupl[i] = trquote(str, i + cx);
	len += ins;
	dy = cx / linemax;
	i = (dy + 1) * linemax;

#if	!MSDOS
	if (*c_insert) {
		for (j = 0; j < ins; j++) putterm(c_insert);

		if (i < len) {
			while (i < len) {
				ptr = i - 1 - cx;
				if (ptr < ins) f1 = 0;
				else f1 = (onkanji1(dupl, ptr - ins)) ? 1 : 0;
				f2 = (onkanji1(dupl, ptr)) ? 1 : 0;
				if (f1) {
					locate(x + linemax - 1, LCMDLINE + dy);
					putch2(' ');
				}
				if (x + linemax < n_column) {
					locate(x + linemax, LCMDLINE + dy);
					for (j = 1; j < ins; j++) putch2(' ');
				}
				locate(x - 1, LCMDLINE + ++dy);
				for (j = 0; j < ins; j++) putterm(c_insert);
				l = ins;
				if (ptr < ins) {
					ptr++;
					locate(x + ins - ptr, LCMDLINE + dy);
					l = ptr - f2;
					ptr = 0;
				}
				else {
					ptr -= ins;
					if (!f1) dupl[ptr] = ' ';
					if (!f2) l++;
				}
				if (ptr + l > len - cx) l = len - cx - ptr;
				kanjiputs2(dupl + ptr, l, 0);
				i += linemax;
			}
			locate (x + cx % linemax, LCMDLINE + cx / linemax);
		}
	}
	else
#endif	/* !MSDOS */
	{
		for (j = 0; j < ins; j++) putch2(' ');

		f2 = cx;
		j = 0;
		l = i - cx - ins;
		while (i < len) {
			ptr = i - 1 - cx;
			if (ptr < ins) f1 = l = 0;
			else f1 = (onkanji1(dupl, ptr - ins)) ? 1 : 0;
			if ((l -= f1) > 0) kanjiputs2(dupl, l, j);
			if (f1) putch2(' ');

			locate(x - 1, LCMDLINE + ++dy);
			l = linemax;
			if (ptr < ins) {
				putch2(' ');
				ptr++;
				locate(x + ins - ptr, LCMDLINE + dy);
				l -= ptr;
				ptr = 0;
			}
			else {
				ptr -= ins;
				if (!f1) dupl[ptr] = ' ';
			}
			if (ptr + l > len - ins - cx) l = len - ins - cx - ptr;
			j = ptr;
			i += linemax;
		}

		l = len - ins - cx - j;
		if (l > 0) kanjiputs2(dupl, l, j);
		locate (x + cx % linemax, LCMDLINE + cx / linemax);
	}

	for (i = len - ins - 1; i >= cx; i--) str[i + ins] = str[i];
}

static VOID deletechar(str, x, cx, len, linemax, del)
char *str;
int x, cx, len, linemax, del;
{
	char dupl[MAXLINESTR + 1];
	int dy, i, j, l, f1, ptr;
#if	!MSDOS
	int f2;
#endif

	for (i = 0; i < len - cx; i++) dupl[i] = trquote(str, i + cx);
	len -= del;
	dy = cx / linemax;
	i = (dy + 1) * linemax;

#if	!MSDOS
	if (*c_delete) {
		for (j = 0; j < del; j++) putterm(c_delete);

		if (i < len + del) {
			while (i < len + del) {
				ptr = i - 1 - cx;
				if (ptr >= len - cx) f1 = 0;
				else f1 = (onkanji1(dupl, ptr + del)) ? 1 : 0;
				f2 = (onkanji1(dupl, ptr)) ? 1 : 0;
				j = linemax - del - f2;
				if (j < linemax - (++ptr)) {
					j = linemax - ptr;
					l = ptr - f1;
					ptr = del;
				}
				else {
					l = del + f2 - f1;
					ptr -= f2;
				}
				locate(x + j, LCMDLINE + dy);
				if (ptr + l > len) l = len - ptr;
				if (l > 0) kanjiputs2(dupl, l, ptr);
				if (f1) putch2(' ');
				locate(x - 1, LCMDLINE + ++dy);
				for (j = 0; j < del; j++) putterm(c_delete);
				if (!f1) putch2(' ');
				i += linemax;
			}
			locate (x + cx % linemax, LCMDLINE + cx / linemax);
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
			if ((l -= f1) > 0) kanjiputs2(dupl, l, j);
			if (f1) putch2(' ');

			locate(x - 1, LCMDLINE + ++dy);
			l = linemax + 1;
			if (ptr < del) {
				putch2(' ');
				ptr = del;
				l--;
			}
			else {
				ptr += del;
				if (!f1) dupl[ptr] = ' ';
			}
			if (ptr + l > len + del - cx) l = len + del - cx - ptr;
			j = ptr;
			i += linemax;
		}

		l = len + del - cx - j;
		if (l > 0) kanjiputs2(dupl, l, j);
		putterm(l_clear);
		locate (x + cx % linemax, LCMDLINE + cx / linemax);
	}

	for (i = cx; i < len; i++) str[i] = str[i + del];
}

static VOID truncline(x, cx, len, linemax)
int x, cx, len, linemax;
{
	int dy, i;

	putterm(l_clear);

	dy = cx / linemax;
	if ((dy + 1) * linemax < len) {
		for (i = (dy + 1) * linemax; i < len; i += linemax) {
			locate(x - 1, LCMDLINE + ++dy);
			putterm(l_clear);
		}
		locate (x + cx % linemax, LCMDLINE + cx / linemax);
	}
}

static VOID displaystr(str, x, cx, len, max, linemax)
char *str;
int x, cx, len, max, linemax;
{
	char *dupl;
	int i, y, width;

	locate(x, LCMDLINE);
	str[len] = '\0';
	dupl = (char *)malloc2(strlen(str) + 1);
	for (i = 0; str[i]; i++) dupl[i] = trquote(str, i);
	dupl[i] = '\0';
	width = linemax;
	for (i = 0, y = 1; i + linemax < len; i += width, y++) {
		width = linemax;
		if (onkanji1(dupl, i + linemax - 1)) width--;
		putterm(l_clear);
		if (stable_standout) putterm(end_standout);
		kanjiputs2(dupl, width, i);
		locate(x - 1, LCMDLINE + y);
		if (width == linemax) putch2(' ');
	}
	putterm(l_clear);
	if (stable_standout) putterm(end_standout);
	kanjiputs(dupl + i);
	for (; y * linemax < max; y++) {
		locate(x - 1, LCMDLINE + y);
		putterm(l_clear);
	}
	locate(x + cx % linemax, LCMDLINE + cx / linemax);
	tflush();
	free(dupl);
}

static int insertstr(str, x, cx, len, linemax, max, insstr, ins)
char *str;
int x, cx, len, linemax, max;
char *insstr;
int ins;
{
	char dupl[MAXLINESTR + 1];
	int i, dy, f, ptr;

	if (len + ins > max) ins = max - len;
	if (onkanji1(insstr, ins - 1)) ins--;
	insertchar(str, x, cx, len, linemax, ins);
	for (i = 0; i < ins; i++) dupl[i] = str[cx + i] = insstr[i];

	dy = cx / linemax;
	i = (dy + 1) * linemax - cx;
	len = ins;
	ptr = 0;
	while (i < len) {
		f = (onkanji1(dupl, i - 1)) ? 1 : 0;
		kanjiputs2(dupl, i - ptr - f, ptr);
		locate(x - 1, LCMDLINE + ++dy);
		if (!f) dupl[i - 1] = ' ';
		ptr = i - 1;
		i += linemax;
	}
	if (len > ptr) kanjiputs2(dupl, len - ptr, ptr);
	return(ins);
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
			len = strlen(strs);
			if (maxlen < len) maxlen = len;
			strs += len + 1;
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

static int completestr(str, x, cx, len, linemax, max, comline, cont)
char *str;
int x, cx, len, linemax, max, comline, cont;
{
	char *cp1, *cp2, *match;
	int i, ins;

	if (selectlist && cont > 0) {
		selectfile(NULL, tmpfilepos++);
		locate(x + cx % linemax, LCMDLINE + cx / linemax);
		return(0);
	}

	for (i = cx; i > 0; i--)
		if (strchr(CMDLINE_DELIM, str[i - 1])) break;
	if (i > 1 && str[i - 1] == ':' && isalpha(str[i - 2])) i -= 2;
	if (i == cx) {
		putterm(t_bell);
		return(0);
	}
	if (i > 0) comline = 0;

	cp1 = (char *)malloc2(cx - i + 1);
	strncpy2(cp1, str + i, cx - i);
	cp2 = evalpath(cp1);

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

	if (cp1 = strrchr(cp2, _SC_)) cp1++;
	else cp1 = cp2;
	ins = strlen(cp1);
	free(cp2);
	if (!i) {
		putterm(t_bell);
		free(match);
		return(0);
	}

	cp1 = findcommon(match, i);
	if (!cp1 || (ins = (int)strlen(cp1) - ins) <= 0) {
		if (cont <= 0) {
			putterm(t_bell);
			return(0);
		}
		selectfile(match, i);
		locate(x + cx % linemax, LCMDLINE + cx / linemax);
		free(match);
		free(cp1);
		return(0);
	}

	cp2 = cp1 + (int)strlen(cp1) - ins;
	insertstr(str, x, cx, len, linemax, max, cp2, ins);

	free(cp1);
	return(ins);
}
#endif	/* !_NOCOMPLETE */

static int _inputstr_up(str, x, cx, lenp, max, linemax, histnop, h, tmp)
char *str;
int x, cx, *lenp, max, linemax, *histnop, h;
char **tmp;
{
	keyflush();
#ifndef	_NOCOMPLETE
	if (selectlist) {
		selectfile(NULL, tmpfilepos--);
		locate(x + cx % linemax, LCMDLINE + cx / linemax);
	}
	else
#endif
	if (cx < linemax) {
		if (h < 0 || *histnop >= histsize[h]
		|| !history[h][*histnop]) {
			putterm(t_bell);
			return(cx);
		}
		if (!(*tmp)) {
			str[*lenp] = '\0';
			*tmp = strdup2(str);
		}
		strcpy(str, history[h][*histnop]);
		*lenp = strlen(str);
		cx = *lenp;
		displaystr(str, x, cx, *lenp, max, linemax);
		(*histnop)++;
	}
	else {
		cx -= linemax;
		putterm(c_up);
		if (onkanji1(str, cx - 1)) {
			putterm(c_left);
			cx--;
		}
	}
	return(cx);
}

static int _inputstr_down(str, x, cx, lenp, max, linemax, histnop, h, tmp)
char *str;
int x, cx, *lenp, max, linemax, *histnop, h;
char **tmp;
{
	keyflush();
#ifndef	_NOCOMPLETE
	if (selectlist) {
		selectfile(NULL, tmpfilepos++);
		locate(x + cx % linemax, LCMDLINE + cx / linemax);
	}
	else
#endif
	if (cx + linemax > *lenp) {
		if (h < 0 || *histnop <= 0) {
			putterm(t_bell);
			return(cx);
		}
		if (--(*histnop) > 0) strcpy(str, history[h][*histnop - 1]);
		else {
			strcpy(str, *tmp);
			free(*tmp);
			*tmp = NULL;
		}
		*lenp = strlen(str);
		cx = *lenp;
		displaystr(str, x, cx, *lenp, max, linemax);
	}
	else {
		cx += linemax;
		putterm(c_down);
		if (onkanji1(str, cx - 1)) {
			putterm(c_left);
			cx--;
		}
	}
	return(cx);
}

static int _inputstr_input(str, x, cx, lenp, max, linemax, ch)
char *str;
int x, cx, *lenp, max, linemax, ch;
{
#if	!MSDOS && !defined (_NOKANJICONV)
	char tmpkanji[3];
#endif
	int i, ch2;

	if (isinkanji1(ch)) {
		ch2 = _getkey2(0);
		if (*lenp + 1 >= max) {
			putterm(t_bell);
			keyflush();
			return(cx);
		}
		insertchar(str, x, cx, *lenp, linemax, 2);

#if	MSDOS || defined (_NOKANJICONV)
		str[cx] = ch;
		str[cx + 1] = ch2;
#else
		tmpkanji[0] = ch;
		tmpkanji[1] = ch2;
		tmpkanji[2] = '\0';
		kanjiconv(&str[cx], tmpkanji, inputkcode, DEFCODE);
#endif
		*lenp += 2;
		cx += 2;
		i = (cx % linemax);
		if (i == 1) locate(x - 1, LCMDLINE + cx / linemax);
		kanjiputs2(str, 2, cx - 2);
		if (!i && *lenp < max) locate(x, LCMDLINE + cx / linemax);
	}
	else {
		if (ch < ' ' || ch >= K_MIN || *lenp >= max) {
			putterm(t_bell);
			keyflush();
			return(cx);
		}
		insertchar(str, x, cx, *lenp, linemax, 1);
		(*lenp)++;
		str[cx++] = ch;
		putch2(ch);
		if (!(cx % linemax) && *lenp < max)
			locate(x, LCMDLINE + cx / linemax);
	}
	return(cx);
}

static int _inputstr(str, x, max, linemax, def, comline, h)
char *str;
int x, max, linemax, def, comline, h;
{
	char *tmphist;
	int len, cx, i, hist, ch, ch2, quote;

	subwindow = 1;
#ifndef	_NOEDITMODE
	getkey2(-1);
#endif
	tmpfilepos = -1;
	cx = len = strlen(str);
	if (def >= 0 && def < linemax) {
		while (def > len) str[len++] = ' ';
		cx = def;
	}
	displaystr(str, x, cx, len, max, linemax);
	keyflush();
	hist = 0;
	tmphist = NULL;
	quote = 0;
	ch = -1;

	do {
		tflush();
		ch2 = ch;
		if (!quote) ch = getkey2(0);
		else {
			ch = _getkey2(0);
			quote = 0;
			if (ch < ' ' || ch == K_DC) {
				keyflush();
				if (len + 1 >= max) {
					putterm(t_bell);
					continue;
				}
				insertchar(str, x, cx, len, linemax, 2);
				len += 2;
				str[cx++] = QUOTE;
				putch2('^');
				if (!(cx % linemax) && len - 1 < max)
					locate(x, LCMDLINE
						+ cx / linemax);
				if (ch == K_DC) {
					str[cx++] = C_DEL;
					ch = '?';
				}
				else {
					str[cx++] = ch;
					ch += '@';
				}
				putch2(ch);
				if (!(cx % linemax) && len < max)
					locate(x, LCMDLINE
						+ cx / linemax);
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
					locate(x + cx % linemax,
						LCMDLINE + cx / linemax);
				}
				else
#endif
				if (cx >= len) putterm(t_bell);
				else cx = rightchar(str, x, cx,
					len, linemax, max);
				break;
			case K_LEFT:
				keyflush();
#ifndef	_NOCOMPLETE
				if (selectlist) {
					i = tmpfilepos;
					tmpfilepos -= FILEPERLOW;
					selectfile(NULL, i);
					locate(x + cx % linemax,
						LCMDLINE + cx / linemax);
				}
				else
#endif
				if (cx <= 0) putterm(t_bell);
				else cx = leftchar(str, x, cx,
					len, linemax, max);
				break;
			case K_BEG:
				keyflush();
				locate(x, LCMDLINE);
				cx = 0;
				break;
			case K_EOL:
				keyflush();
				cx = len;
				if (cx < max) locate(x + cx % linemax,
					LCMDLINE + cx / linemax);
				else {
					locate(x + cx % linemax + linemax - 1,
						LCMDLINE + cx / linemax - 1);
					putterm(c_right);
				}
				break;
			case K_BS:
				keyflush();
				if (cx <= 0) {
					putterm(t_bell);
					break;
				}
				cx = leftchar(str, x, cx, len, linemax, max);
			case K_DC:
				keyflush();
				if (cx >= len) {
					putterm(t_bell);
					break;
				}
				if (str[cx] != QUOTE
				&& !iskanji1(str[cx])) i = 1;
				else {
					i = 2;
					if (cx + 1 >= len) {
						putterm(t_bell);
						break;
					}
				}
				deletechar(str, x, cx, len, linemax, i);
				len -= i;
				break;
			case K_DL:
				keyflush();
				if (cx >= len) putterm(t_bell);
				else {
					truncline(x, cx, len, linemax);
					len = cx;
				}
				break;
			case CTRL('L'):
				keyflush();
				for (i = 0; i < WCMDLINE; i++) {
					locate(x, LCMDLINE + i);
					putterm(l_clear);
				}
				displaystr(str, x, cx, len, max, linemax);
				break;
			case K_UP:
				cx = _inputstr_up(str, x, cx, &len,
					max, linemax, &hist, h, &tmphist);
				break;
			case K_DOWN:
				cx = _inputstr_down(str, x, cx, &len,
					max, linemax, &hist, h, &tmphist);
				break;
			case K_IL:
				keyflush();
				quote = 1;
				break;
			case K_ENTER:
				keyflush();
				i = strlen(curfilename);
				insertstr(str, x, cx,
					len, linemax, max, curfilename, i);
				cx += i;
				len += i;
				if (!(cx % linemax) && len < max)
					locate(x, LCMDLINE + cx / linemax);
				break;
#ifndef	_NOCOMPLETE
			case '\t':
				keyflush();
				if (selectlist) {
					putterm(t_bell);
					break;
				}
				i = completestr(str, x, cx, len, linemax, max,
					comline, (ch2 == ch) ? 1 : 0);
				cx += i;
				len += i;
				if (!(cx % linemax) && len < max)
					locate(x, LCMDLINE + cx / linemax);
				break;
#endif
			case CR:
				keyflush();
#ifndef	_NOCOMPLETE
				if (!selectlist) break;
				i = completestr(str, x, cx, len, linemax, max,
					0, -1);
				cx += i;
				len += i;
				if (!(cx % linemax) && len < max)
					locate(x, LCMDLINE + cx / linemax);
				ch = '\0';
#endif
				break;
			case ESC:
				keyflush();
				break;
			default:
				cx = _inputstr_input(str, x, cx, &len,
					max, linemax, ch);
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
			locate(x + cx % linemax, LCMDLINE + cx / linemax);
		}
#endif	/* !_NOCOMPLETE */
	} while (ch != ESC && ch != CR);

	subwindow = 0;
#ifndef	_NOEDITMODE
	getkey2(-1);
#endif
	if (tmphist) free(tmphist);

	if (ch == ESC) len = 0;
	str[len] = '\0';

	tflush();
	return(ch);
}

char *inputstr(prompt, delsp, ptr, def, h)
char *prompt;
int delsp, ptr;
char *def;
int h;
{
	char *dupl, input[MAXLINESTR + 1];
	int i, j, len, ch;

	for (i = 0; i < WCMDLINE; i++) {
		locate(0, LCMDLINE + i);
		putterm(l_clear);
	}
	locate(0, LCMDLINE);
	putch2(' ');
	putterm(t_standout);
	kanjiputs(prompt);
	putterm(end_standout);
	tflush();

	len = strlen(prompt);
	if (def) strcpy(input, def);
	else *input = '\0';
	i = (n_column - len - 1) * WCMDLINE;
	if (LCMDLINE + WCMDLINE - n_line >= 0) i -= n_column - n_lastcolumn;
	if (i > MAXLINESTR) i = MAXLINESTR;
	ch = _inputstr(input, len + 1, i, n_column - len - 1,
		ptr, h == 0, h);
	for (i = 0; i < WCMDLINE; i++) {
		locate(0, LCMDLINE + i);
		putterm(l_clear);
	}

	if (ch == ESC) return(NULL);

	len = strlen(input);
	if (delsp && len > 0 && input[len - 1] == ' ' && yesno(DELSP_K)) {
		for (len--; len > 0 && input[len - 1] == ' '; len--);
		input[len] = '\0';
	}
	if (h == 0) entryhist(h, input, 0);
	tflush();
	dupl = (char *)malloc2(len + 1);
	for (i = 0, j = 0; input[i]; i++, j++) {
		if (input[i] == QUOTE) i++;
		dupl[j] = input[i];
	}
	dupl[j] = '\0';
	return(dupl);
}

static char *truncstr(str)
char *str;
{
	int len;
	char *cp1, *cp2;

	if ((len = (int)strlen(str) + 5 - n_lastcolumn) <= 0
	|| !(cp1 = strchr2(str, '['))
	|| !(cp2 = strchr2(cp1, ']'))) return(str);

	cp1++;
	len = cp2 - cp1 - len;
	if (len < 0) len = 0;
	else if (onkanji1(cp1, len - 1)) len--;
	strcpy(cp1 + len, cp2);
	return(str);
}

static VOID yesnomes(mes)
char *mes;
{
	locate(0, LMESLINE);
	putterm(l_clear);
	putterm(t_standout);
	kanjiputs(mes);
	cputs2("[Y/N]");
	putterm(end_standout);
	tflush();
}

#if	MSDOS
/*VARARGS1*/
int yesno(const char *fmt, ...)
{
	va_list args;
	int len, ch, ret = 1;
	char buf[MAXLINESTR + 1];

	subwindow = 1;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
#else	/* !MSDOS */
#ifndef	NOVSPRINTF
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
#else	/* !NOVSPRINTF */
int yesno(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
{
	int len, ch, ret = 1;
	char buf[MAXLINESTR + 1];

	subwindow = 1;
	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
#endif	/* !NOVSPRINTF */
#endif	/* !MSDOS */
#ifndef	_NOEDITMODE
	getkey2(-1);
#endif
	truncstr(buf);

	len = strlen(buf);
	yesnomes(buf);

	do {
		keyflush();
		locate(len + 1 + (1 - ret) * 2, LMESLINE);
		tflush();
		switch (ch = getkey2(0)) {
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
	getkey2(-1);
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
		tmp = (char *)malloc2(strlen(str) + strlen(err) + 3);
		strcpy(tmp, str);
		if (strlen(str) > len) {
			if (onkanji1(str, len - 1)) len--;
			tmp[len] = '\0';
		}
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
	_getkey2(SIGALRM);
	subwindow = 0;

	locate(0, LMESLINE);
	putterm(l_clear);
	tflush();

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
	getkey2(-1);
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
		switch (ch = getkey2(SIGALRM)) {
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
	getkey2(-1);
#endif
	if (stable_standout) {
		locate(x + 1, LMESLINE);
		putterm(l_clear);
	}
	for (i = 0; i < max; i++) {
		locate(x + xx[i] + 1, LMESLINE);
		if (i == new) kanjiputs(str[i]);
		else cprintf2("%*s", strlen(str[i]), " ");
	}
	if (ch != ESC) *num = val[new];
	tflush();
	return(ch);
}
