/*
 *	input.c
 *
 *	Input Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"

#include <varargs.h>
#include <signal.h>

extern int histsize;
#ifndef	DECLERRLIST
extern char *sys_errlist[];
#endif

static int rightchar();
static int leftchar();
static VOID insertchar();
static VOID deletechar();
static VOID truncline();
static VOID displaystr();
static int completestr();
static char *truncstr();
static VOID yesnomes();
static int selectmes();

int subwindow;


static int rightchar(str, x, cx, len, linemax, max)
u_char *str;
int x, cx, len, linemax, max;
{
	int i;

	if (iskanji1((int)str[cx])) {
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
	else {
		if (++cx % linemax || cx >= max) putterm(c_right);
		else locate(x, LCMDLINE + cx / linemax);
	}
	return(cx);
}

static int leftchar(str, x, cx, len, linemax, max)
u_char *str;
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
	else {
		if (cx-- % linemax) putterm(c_left);
		else locate(x + linemax - 1, LCMDLINE + cx / linemax);
	}
	return(cx);
}

static VOID insertchar(str, x, cx, len, linemax, ins)
u_char *str;
int x, cx, len, linemax, ins;
{
	int dy, i, j, f1, f2;

	len += ins;
	dy = cx / linemax;
	i = (dy + 1) * linemax;

	if (*c_insert) {
		for (j = 0; j < ins; j++) putterm(c_insert);

		if (i < len) {
			while (i < len) {
				f1 = (onkanji1(str, i - ins - 1)) ? 1 : 0;
				f2 = (onkanji1(str, i - 1)) ? 1 : 0;
				if (f1) {
					locate(x + linemax - 1, LCMDLINE + dy);
					putch(' ');
				}
				if (x + linemax < n_column) {
					locate(x + linemax, LCMDLINE + dy);
					for (j = 1; j < ins; j++) putch(' ');
				}
				locate(x - 1, LCMDLINE + ++dy);
				for (j = 0; j < ins; j++) putterm(c_insert);
				putch((f1) ? (char)str[i - ins - 1] : ' ');
				for (j = 0; j < ins - f2; j++)
					putch((char)str[i + j - ins]);
				i += linemax;
			}
			locate (x + cx % linemax, LCMDLINE + cx / linemax);
		}
	}
	else {
		for (j = 0; j < ins; j++) putch(' ');

		if (i >= len) f2 = cx;
		else {
			j = linemax - cx % linemax - ins;
			f2 = cx;
			while (i < len) {
				f1 = (onkanji1(str, i - ins - 1)) ? 1 : 0;
				if ((j -= f1) > 0)
					kanjiprintf("%-*.*s", j, j, &str[f2]);
				if (f1) putch(' ');

				locate(x - 1, LCMDLINE + ++dy);
				putch((f1) ? (char)str[i - ins - 1] : ' ');
				j = linemax - ins;
				f2 = i - ins;
				i += linemax;
			}
		}

		j = len - f2 - ins;
		if (j > 0) kanjiprintf("%-*.*s", j, j, &str[f2]);
		locate (x + cx % linemax, LCMDLINE + cx / linemax);
	}

	for (i = len - ins - 1; i >= cx; i--) str[i + ins] = str[i];
}

static VOID deletechar(str, x, cx, len, linemax, del)
u_char *str;
int x, cx, len, linemax, del;
{
	int dy, i, j, f1, f2;

	len -= del;
	dy = cx / linemax;
	i = (dy + 1) * linemax;

	if (*c_delete) {
		for (j = 0; j < del; j++) putterm(c_delete);

		if (i < len + del) {
			while (i < len + del) {
				f1 = (onkanji1(str, i + del - 1)) ? 1 : 0;
				f2 = (onkanji1(str, i - 1)) ? 1 : 0;
				locate(x + linemax - del - f2, LCMDLINE + dy);
				for (j = -f2; j < del - f1; j++)
					putch((char)str[i + j]);
				if (f1) putch(' ');
				locate(x - 1, LCMDLINE + ++dy);
				for (j = 0; j < del; j++) putterm(c_delete);
				if (!f1) putch(' ');
				i += linemax;
			}
			locate (x + cx % linemax, LCMDLINE + cx / linemax);
		}
	}
	else {
		if (i >= len) f2 = cx + del;
		else {
			j = linemax - cx % linemax;
			f2 = cx + del;

			while (i < len) {
				f1 = (onkanji1(str, i + del - 1)) ? 1 : 0;
				if ((j -= f1) > 0)
					kanjiprintf("%-*.*s", j, j, &str[f2]);
				if (f1) putch(' ');

				locate(x - 1, LCMDLINE + ++dy);
				putch((f1) ? (char)str[i + del - 1] : ' ');
				j = linemax - 1;
				f2 = i + del;
				i += linemax;
			}
		}

		j = len - f2 + del;
		if (j > 0) kanjiprintf("%-*.*s", j, j, &str[f2]);
		else --dy;
		for (j = 0; j < del; j++) {
			if (!((len + j) % linemax)) locate(x, LCMDLINE + ++dy);
			putch(' ');
		}

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
u_char *str;
int x, cx, len, max, linemax;
{
	int i, y, width;

	locate(x, LCMDLINE);
	str[len] = '\0';
	width = linemax;
	for (i = 0, y = 1; i + linemax < len; i += width, y++) {
		width = linemax;
		if (onkanji1(str, i + linemax - 1)) width--;
		putterm(l_clear);
		if (stable_standout) putterm(end_standout);
		kanjiprintf("%-*.*s", width, width, (char *)str + i);
		locate(x - 1, LCMDLINE + y);
		if (width == linemax) putch(' ');
	}
	putterm(l_clear);
	if (stable_standout) putterm(end_standout);
	kanjiputs((char *)str + i);
	for (; y * linemax < max; y++) {
		locate(x, LCMDLINE + y);
		putterm(l_clear);
	}
	locate(x + cx % linemax, LCMDLINE + cx / linemax);
	tflush();
}

static int completestr(str, x, cx, len, linemax, max)
u_char *str;
int x, cx, len, linemax, max;
{
	char *cp1, *cp2;
	int i, ins;

	for (i = cx; i > 0; i--)
		if (strchr(CMDLINE_DELIM, (char)str[i - 1])) break;
	if (i == cx) {
		putterm(t_bell);
		return(0);
	}

	cp1 = (char *)malloc2(cx - i + 1);
	strncpy2(cp1, (char *)str + i, cx - i);
	cp2 = evalpath(cp1);
	cp1 = completepath(cp2);

	if (!cp1 || (ins = strlen(cp1) - strlen(cp2)) <= 0) {
		putterm(t_bell);
		if (cp1) free(cp1);
		free(cp2);
		return(0);
	}
	free(cp2);

	if (len + ins > max) ins = max - len;

	insertchar(str, x, cx, len, linemax, ins);

	cp2 = cp1 + strlen(cp1) - ins;
	for (i = 0; i < ins; i++) {
		str[cx] = *(cp2++);
		putch((char)str[cx]);
		if (!(++cx % linemax) && len < max)
			locate(x, LCMDLINE + cx / linemax);
	}

	free(cp1);
	return(ins);
}

int inputstr(str, x, max, linemax, def, hist)
u_char *str;
int x, max, linemax, def;
char *hist[];
{
	int len, cx, i, histno, ch, ch2;
	char *tmphist;

	subwindow = 1;
	cx = len = strlen((char *)str);
	if (def >= 0 && def < linemax) {
		while (def > len) str[len++] = ' ';
		cx = def;
	}
	displaystr(str, x, cx, len, max, linemax);
	keyflush();
	histno = 1;
	tmphist = NULL;

	do {
		tflush();
		switch (ch = getkey(0)) {
			case CTRL_F:
			case K_RIGHT:
				keyflush();
				if (cx >= len) putterm(t_bell);
				else cx = rightchar(str, x, cx,
					len, linemax, max);
				break;
			case CTRL_B:
			case K_LEFT:
				keyflush();
				if (cx <= 0) putterm(t_bell);
				else cx = leftchar(str, x, cx,
					len, linemax, max);
				break;
			case CTRL_A:
				keyflush();
				locate(x, LCMDLINE);
				cx = 0;
				break;
			case CTRL_E:
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
			case CTRL_D:
				keyflush();
				if (cx >= len) {
					putterm(t_bell);
					break;
				}
				if (!iskanji1((int)str[cx])) i = 1;
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
			case CTRL_K:
				keyflush();
				if (cx >= len) putterm(t_bell);
				else {
					truncline(x, cx, len, linemax);
					len = cx;
				}
				break;
			case CTRL_L:
				keyflush();
				for (i = 0; i < WCMDLINE; i++) {
					locate(x, LCMDLINE + i);
					putterm(l_clear);
				}
				displaystr(str, x, cx, len, max, linemax);
				break;
			case CTRL_P:
			case K_UP:
				keyflush();
				if (cx < linemax) {
					if (!hist || histno > histsize
					|| !hist[histno]) {
						putterm(t_bell);
						break;
					}
					if (!tmphist) {
						str[len] = '\0';
						tmphist = strdup2((char *)str);
					}
					strcpy((char *)str, hist[histno]);
					len = strlen((char *)str);
					cx = len;
					displaystr(str, x, cx,
						len, max, linemax);
					histno++;
					break;
				}
				cx -= linemax;
				putterm(c_up);
				if (onkanji1(str, cx - 1)) {
					putterm(c_left);
					cx--;
				}
				break;
			case CTRL_N:
			case K_DOWN:
				keyflush();
				if (cx + linemax > len) {
					if (!hist || histno <= 1) {
						putterm(t_bell);
						break;
					}
					if (--histno > 1) strcpy((char *)str,
						hist[histno - 1]);
					else {
						strcpy((char *)str, tmphist);
						free(tmphist);
						tmphist = NULL;
					}
					len = strlen((char *)str);
					cx = len;
					displaystr(str, x, cx,
						len, max, linemax);
					break;
				}
				cx += linemax;
				putterm(c_down);
				if (onkanji1(str, cx - 1)) {
					putterm(c_left);
					cx--;
				}
				break;
			case '\t':
				keyflush();
				i = completestr(str, x, cx, len, linemax, max);
				cx += i;
				len += i;
				break;
			case CR:
			case ESC:
				keyflush();
				break;
			default:
				if (iskanji1((int)ch)) {
					ch2 = getkey(0);
					if (len + 1 >= max) {
						putterm(t_bell);
						keyflush();
						break;
					}
					insertchar(str, x, cx, len, linemax, 2);
					len += 2;
					str[cx++] = (u_char)ch;
					str[cx++] = (u_char)ch2;
					i = (cx % linemax);
					if (i == 1)
						locate(x - 1, LCMDLINE
							+ cx / linemax);
					putch((char)ch);
					putch((char)ch2);
					if (!i && len < max)
						locate(x, LCMDLINE
							+ cx / linemax);
				}
				else {
					if (ch < ' ' || len >= max) {
						putterm(t_bell);
						keyflush();
						break;
					}
					insertchar(str, x, cx, len, linemax, 1);
					len++;
					str[cx++] = (u_char)ch;
					putch((char)ch);
					if (!(cx % linemax) && len < max)
						locate(x, LCMDLINE
							+ cx / linemax);
				}
				break;
		}
	} while (ch != ESC && ch != CR);

	subwindow = 0;
	if (tmphist) free(tmphist);

	if (ch == ESC) len = 0;
	str[len] = '\0';

	tflush();
	return(ch);
}

char *inputstr2(prompt, ptr, def, hist)
char *prompt;
int ptr;
char *def, *hist[];
{
	char input[MAXLINESTR + 1];
	int i, len, ch;

	for (i = 0; i < WCMDLINE; i++) {
		locate(0, LCMDLINE + i);
		putterm(l_clear);
	}
	locate(0, LCMDLINE);
	putch(' ');
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
	ch = inputstr(input, len + 1, i, n_column - len - 1, ptr, hist);
	for (i = 0; i < WCMDLINE; i++) {
		locate(0, LCMDLINE + i);
		putterm(l_clear);
	}

	if (ch == ESC) return(NULL);

	tflush();
	return(strdup2(input));
}

static char *truncstr(str)
char *str;
{
	int len;
	char *cp1, *cp2;

	if ((len = strlen(str) + 5 - n_lastcolumn) <= 0
	|| !(cp1 = strchr2(str, '['))
	|| !(cp2 = strchr2((u_char *)cp1, ']'))) return(str);

	cp1++;
	len = cp2 - cp1 - len;
	if (len < 0) len = 0;
	else if (onkanji1((u_char *)cp1, len - 1)) len--;
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
	cputs("[Y/N]");
	putterm(end_standout);
	tflush();
}

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
	truncstr(buf);

	len = strlen(buf);
	yesnomes(buf);

	do {
		keyflush();
		locate(len + 1 + (1 - ret) * 2, LMESLINE);
		tflush();
		switch (ch = getkey(0)) {
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
			case CTRL_F:
			case K_RIGHT:
				ret = 0;
				break;
			case CTRL_B:
			case K_LEFT:
				ret = 1;
				break;
			case CTRL_L:
				yesnomes(buf);
				break;
			default:
				break;
		}
	} while (ch != CR);

	subwindow = 0;
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
		len = n_lastcolumn - strlen(err) - 3;
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
	getkey(SIGALRM);
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
	xx[0] = 0;
	for (i = 0; i < max; i++) {
		initial[i] = (isupper(*str[i])) ? *str[i] : -1;
		xx[i + 1] = xx[i] + strlen(str[i]) + 1;
	}
	new = selectmes(*num, max, x, str, val, xx);

	do {
		locate(x + xx[new + 1], LMESLINE);
		tflush();
		old = new;

		keyflush();
		switch (ch = getkey(SIGALRM)) {
			case CTRL_F:
			case K_RIGHT:
				if (old < max - 1) new = old + 1;
				else new = 0;
				break;
			case CTRL_B:
			case K_LEFT:
				if (old > 0) new = old - 1;
				else new = max - 1;
				break;
			case CTRL_L:
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
	if (stable_standout) {
		locate(x + 1, LMESLINE);
		putterm(l_clear);
	}
	for (i = 0; i < max; i++) {
		locate(x + xx[i] + 1, LMESLINE);
		if (i == new) kanjiputs(str[i]);
		else cprintf("%*s", strlen(str[i]), " ");
	}
	if (ch != ESC) *num = val[new];
	tflush();
	return(ch);
}
