/*
 *	input.c
 *
 *	Input Module
 */

#include "fd.h"
#include "term.h"
#include "kctype.h"

#include <varargs.h>
#include <signal.h>

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
static u_char *truncstr();
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
		cx -= 2;
		i = cx % linemax;
		if (i < 0) locate(x + linemax + i, LCMDLINE + cx / linemax);
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
				if (x + linemax < n_column) {
					locate(x + linemax, LCMDLINE + dy);
					if (f1) putterm(c_right);
					else putch(' ');
					for (j = 1; j < ins + f2; j++)
						putch(' ');
				}
				locate(x, LCMDLINE + ++dy);
				for (j = 0; j < ins; j++) putterm(c_insert);
				if (f1) putterm(c_right);
				else putch((char)str[i - ins]);
				for (j = 1; j < ins + f2; j++)
					putch((char)str[i + j - ins]);
				i += linemax;
			}
			locate (x + cx % linemax, LCMDLINE + cx / linemax);
		}
	}
	else {
		for (j = 0; j < ins; j++) putch(' ');

		if (i >= len) i = cx + ins;
		else {
			f1 = (onkanji1(str, i - ins - 1)) ? 1 : 0;
			f2 = (onkanji1(str, i - 1)) ? 1 : 0;

			j = linemax - cx % linemax - ins + f1;
			if (j > 0) cprintf("%-*.*s", j, j, (char *)str + cx);
			if (f2 && !f1) putch(' ');

			locate(x, LCMDLINE + ++dy);
			if (f1) putch(' ');
			else putch((char)str[i - ins]);
			i += linemax;

			while (i < len) {
				f1 = (onkanji1(str, i - ins - 1)) ? 1 : 0;
				f2 = (onkanji1(str, i - 1)) ? 1 : 0;
				j = linemax + f1 - 1;
				cprintf("%-*.*s", j, j,
					(char *)str + i - linemax - ins + 1);
				if (f2 && !f1) putch(' ');

				locate(x, LCMDLINE + ++dy);
				if (f1) putch(' ');
				else putch((char)str[i - ins]);
				i += linemax;
			}
			i -= linemax - 1;
		}

		j = len - i;
		if (j > 0) cprintf("%-*.*s", j, j, (char *)str + i - ins);
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

		if (i < len) {
			while (i < len) {
				f1 = (onkanji1(str, i + del - 1)) ? 1 : 0;
				f2 = (onkanji1(str, i - 1)) ? 1 : 0;
				locate(x + linemax - del, LCMDLINE + dy);
				if (f2) putterm(c_right);
				else putch((char)str[i]);
				for (j = 1; j < del + f1; j++)
					putch((char)str[i + j]);
				locate(x, LCMDLINE + ++dy);
				for (j = 0; j < del; j++) putterm(c_delete);
				if (f1) putch(' ');
				i += linemax;
			}
			locate (x + cx % linemax, LCMDLINE + cx / linemax);
		}
	}
	else {
		if (i >= len) i = cx;
		else {
			f1 = (onkanji1(str, i + del - 1)) ? 1 : 0;
			f2 = (onkanji1(str, i - 1)) ? 1 : 0;

			j = linemax - cx % linemax + f1;
			cprintf("%-*.*s", j, j, (char *)str + cx + del);
			if (f2 && !f1) putch(' ');

			locate(x, LCMDLINE + ++dy);
			if (f1) putch(' ');
			else putch((char)str[i + del]);
			i += linemax;

			while (i < len) {
				f1 = (onkanji1(str, i + del - 1)) ? 1 : 0;
				f2 = (onkanji1(str, i - 1)) ? 1 : 0;
				j = linemax + f1 - 1;
				cprintf("%-*.*s", j, j,
					(char *)str + i - linemax + del + 1);
				if (f2 && !f1) putch(' ');

				locate(x, LCMDLINE + ++dy);
				if (f1) putch(' ');
				else putch((char)str[i - linemax + del]);
				i += linemax;
			}
			i -= linemax - 1;
		}

		j = len - i;
		if (j > 0) {
			cprintf("%-*.*s", j, j, (char *)str + i + del);
			if (!(len % linemax)) locate(x, LCMDLINE + ++dy);
		}
		for (j = 0; j < del; j++) putch(' ');

		locate (x + cx % linemax, LCMDLINE + cx / linemax);
	}

	for (i = cx; i <= len - del; i++) str[i] = str[i + del];
}

static VOID truncline(x, cx, len, linemax)
int x, cx, len, linemax;
{
	int dy, i;

	putterm(l_clear);

	dy = cx / linemax;
	if ((dy + 1) * linemax < len) {
		for (i = (dy + 1) * linemax; i < len; i += linemax) {
			locate(x, LCMDLINE + ++dy);
			putterm(l_clear);
		}
		locate (x + cx % linemax, LCMDLINE + cx / linemax);
	}
}

static VOID displaystr(str, x, len, linemax)
u_char *str;
int x, len, linemax;
{
	int cx, i;

	locate(x, LCMDLINE);
	putterm(l_clear);
	str[len] = '\0';
	if (len) {
		i = linemax;
		for (cx = 0; cx + linemax < len; cx += i) {
			i = linemax;
			if (onkanji1(str, cx + linemax - 1)) i++;
			cprintf("%-*.*s", i, i, (char *)str + cx);
			locate(x + i - linemax, LCMDLINE + cx / linemax + 1);
		}
		cputs((char *)str + cx);
	}
	tflush();
}

static int completestr(str, x, cx, len, linemax, max)
u_char *str;
int x, cx, len, linemax, max;
{
	u_char *cp1, *cp2;
	int i, ins;

	for (i = cx; i > 0; i--)
		if (strchr(CMDLINE_DELIM, (char)str[i - 1])) break;
	if (i == cx) {
		putterm(t_bell);
		return(0);
	}

	cp1 = (u_char *)malloc2(cx - i + 1);
	strncpy2((char *)cp1, (char *)str + i, cx - i);
	cp2 = (u_char *)evalpath(cp1);
	cp1 = (u_char *)completepath(cp2);

	if (!cp1 || (ins = strlen((char *)cp1) - strlen((char *)cp2)) <= 0) {
		putterm(t_bell);
		if (cp1) free(cp1);
		free(cp2);
		return(0);
	}
	free(cp2);

	if (len + ins > max) ins = max - len;

	insertchar(str, x, cx, len, linemax, ins);

	cp2 = cp1 + strlen((char *)cp1) - ins;
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
u_char *hist[];
{
	int len, cx, i, histno, ch, ch2;
	u_char *tmphist;

	subwindow = 1;
	cx = len = strlen((char *)str);
	displaystr(str, x, len, linemax);
	if (def >= 0 && def < linemax) {
		while (def > len) str[len++] = ' ';
		locate(x + (cx = def), LCMDLINE);
	}
	keyflush();
	histno = 0;
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
					locate(0, LCMDLINE + i);
					putterm(l_clear);
				}
				displaystr(str, x, len, linemax);
				locate(x + cx % linemax,
					LCMDLINE + cx / linemax);
				break;
			case CTRL_P:
			case K_UP:
				keyflush();
				if (hist) {
					if (!hist[histno]) {
						putterm(t_bell);
						break;
					}
					if (!tmphist) tmphist =
						(u_char *)strdup2((char *)str);
					strcpy((char *)str,
						(char *)hist[histno]);
					len = strlen((char *)str);
					cx = len;
					displaystr(str, x, len, linemax);
					histno++;
					break;
				}
				if (cx < linemax) break;
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
				if (hist) {
					if (histno <= 0) {
						putterm(t_bell);
						break;
					}
					if (--histno > 0) strcpy((char *)str,
						(char *)hist[histno - 1]);
					else {
						strcpy((char *)str,
							(char *)tmphist);
						free(tmphist);
						tmphist = NULL;
					}
					len = strlen((char *)str);
					cx = len;
					displaystr(str, x, len, linemax);
					break;
				}
				if (cx + linemax >= len) break;
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
					putch((char)ch);
					putch((char)ch2);
					i = (cx % linemax);
					if (i < 2 && len < max)
						locate(x + i, LCMDLINE
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
u_char *prompt;
int ptr;
u_char *def, *hist[];
{
	u_char input[MAXLINESTR + 1];
	int i, len, ch;

	for (i = 0; i < WCMDLINE; i++) {
		locate(0, LCMDLINE + i);
		putterm(l_clear);
	}
	locate(0, LCMDLINE);
	putch(' ');
	putterm(t_standout);
	cputs((char *)prompt);
	putterm(end_standout);
	tflush();

	len = strlen((char *)prompt);
	if (def) strcpy((char *)input, (char *)def);
	else *input = '\0';
	i = (n_column - len - 1) * WCMDLINE;
	if (i > MAXLINESTR) i = MAXLINESTR;
	ch = inputstr(input, len + 1, i, n_column - len - 1, ptr, hist);
	for (i = 0; i < WCMDLINE; i++) {
		locate(0, LCMDLINE + i);
		putterm(l_clear);
	}

	if (ch == ESC) return(NULL);

	tflush();
	return(strdup2((char *)input));
}

static u_char *truncstr(str)
u_char *str;
{
	int len;
	u_char *cp1, *cp2;

	if ((len = strlen((char *)str) + 5 - n_column) <= 0
	|| !(cp1 = strchr2(str, '['))
	|| !(cp2 = strchr2(cp1, ']'))) return(str);

	cp1++;
	len = cp2 - cp1 - len;
	if (len < 0) len = 0;
	else if (onkanji1(cp1, len - 1)) len--;
	strcpy((char *)cp1 + len, (char *)cp2);
	return(str);
}

static VOID yesnomes(mes, len)
char *mes;
int len;
{
	locate(0, LMESLINE);
	putterm(l_clear);
	putterm(t_standout);
	cputs(mes);
	locate(len, LMESLINE);
	cputs("[Y/N]");
	putterm(end_standout);
	tflush();
}

int yesno(fmt, va_alist)
u_char *fmt;
va_dcl
{
	va_list args;
	int len, ch, ret = 1;
	u_char buf[MAXLINESTR + 1];

	subwindow = 1;
	va_start(args);
	vsprintf((char *)buf, (char *)fmt, args);
	va_end(args);
	truncstr(buf);

	len = strlen((char *)buf);
	yesnomes((char *)buf, len);

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
				yesnomes((char *)buf, len);
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
u_char *str;
{
	u_char *tmp, *err;
	int len;

	subwindow = 1;
	if (no < 0) no = errno;
	err = (u_char *)sys_errlist[no];
	if (!str) tmp = err;
	else if (!no) {
		tmp = str;
		str = NULL;
	}
	else {
		len = n_column - strlen((char *)err) - 3;
		tmp = (u_char *)malloc2(strlen((char *)str)
			+ strlen((char *)err) + 3);
		strcpy((char *)tmp, (char *)str);
		if (strlen((char *)str) > len) {
			if (onkanji1(str, len - 1)) len--;
			tmp[len] = '\0';
		}
		strcat((char *)tmp, ": ");
		strcat((char *)tmp, (char *)err);
	}
	putterm(t_bell);

	locate(0, LMESLINE);
	putterm(l_clear);
	putterm(t_standout);
	cputs((char *)tmp);
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
u_char *str[];
int val[], xx[];
{
	int i, new;

	locate(x, LMESLINE);
	putterm(l_clear);
	for (i = 0; i < max; i++) {
		locate(x + xx[i] + 1, LMESLINE);
		if (val[i] != num) cputs((char *)str[i]);
		else {
			new = i;
			putterm(t_standout);
			cputs((char *)str[i]);
			putterm(end_standout);
		}
	}
	return(new);
}

int selectstr(num, max, x, str, val)
int *num, max, x;
u_char *str[];
int val[];
{
	int i, ch, old, new, xx[10];

	subwindow = 1;
	xx[0] = 0;
	for (i = 0; i < max; i++)
		xx[i + 1] = xx[i] + strlen((char *)str[i]) + 1;
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
				break;
		}
		if (new != old) {
			putterm(t_standout);
			locate(x + xx[new] + 1, LMESLINE);
			cputs((char *)str[new]);
			putterm(end_standout);
			locate(x + xx[old] + 1, LMESLINE);
			cputs((char *)str[old]);
		}
	} while (ch != ESC && ch != CR);

	subwindow = 0;
	for (i = 0; i < max; i++) {
		locate(x + xx[i] + 1, LMESLINE);
		if (i == new) cputs((char *)str[i]);
		else cprintf("%*s", strlen((char *)str[i]), " ");
	}
	if (ch != ESC) *num = val[new];
	tflush();
	return(ch);
}
