/*
 *	backend.c
 *
 *	backend of terminal emulation
 */

#include "fd.h"

#ifndef	_NOPTY

#include <signal.h>
#include "termio.h"
#include "func.h"
#include "termemu.h"

#ifndef	_NOORIGSHELL
#include "system.h"
#endif

#define	MAXESCPARAM	16
#define	MAXESCCHAR	4
#define	MAXTABSTOP	255
#define	MAXKANJIBUF	(3 + 2 + 3)

typedef struct _ptyterm_t {
	short cur_x, cur_y;
	short min_x, min_y;
	short max_x, max_y;
	short save_x, save_y;
	short min_scroll, max_scroll;
	short last1, last2;
	short fg, bg;
	u_char escmode;
	u_char nparam;
	u_char nchar;
	u_char ntabstop;
	short escparam[MAXESCPARAM];
	u_char escchar[MAXESCCHAR];
	u_short tabstop[MAXTABSTOP];
	u_short attr;
	u_short termflags;
	char codeselect[MAXESCCHAR + 2];
} ptyterm_t;

#define	A_BOLD		00001
#define	A_REVERSE	00002
#define	A_DIM		00004
#define	A_BLINK		00010
#define	A_STANDOUT	00020
#define	A_UNDERLINE	00040
#define	T_NOAUTOMARGIN	00001
#define	T_MULTIBYTE	00002
#define	T_NOAPPLIKEY	00004
#define	T_NOAPPLICURSOR	00010
#define	T_CODEALT	00020
#define	T_LOCKED	00040

extern int hideclock;
extern int fdmode;
extern int wheader;
extern int emufd;

static ptyterm_t pty[MAXWINDOWS + 1];
static short last_x = (short)-1;
static short last_y = (short)-1;
static short last_fg = (short)-1;
static short last_bg = (short)-1;
static u_short last_attr = (u_short)0;
static u_short last_termflags = (u_short)0;
static char last_codeselect[MAXESCCHAR + 3] = "\033(B";
#ifdef	SIGWINCH
static int ptywinched = 0;
#endif

static VOID NEAR resettermattr __P_((int));
static VOID NEAR resettermcode __P_((int));
static VOID NEAR resettabstop __P_((int));
static VOID NEAR biasxy __P_((int, short *, short *));
static VOID NEAR settermattr __P_((int));
static VOID NEAR settermcode __P_((int));
static VOID NEAR surelocate __P_((int, int));
static VOID NEAR reallocate __P_((int, int, int));
#ifdef	SIGWINCH
static int wintr __P_((VOID_A));
static VOID NEAR evalsignal __P_((VOID_A));
#endif
static int NEAR convkey __P_((int, keyseq_t *));
#ifndef	_NOKANJICONV
static int NEAR ptygetch __P_((int));
static char *NEAR ptykconv __P_((char *, char *, int, int));
#endif
static VOID NEAR evalscroll __P_((int, int, int));
static VOID NEAR evallf __P_((int));
static VOID NEAR evalnormal __P_((int, int));
static VOID NEAR evalcontrol __P_((int, int));
static VOID NEAR evalescape __P_((int, int));
static VOID NEAR evalcsi __P_((int, int, int));
static VOID NEAR evalcodeselect __P_((int, int));
static VOID NEAR evaloutput __P_((int));
static int NEAR chgattr __P_((int, int));
static int NEAR directoutput __P_((int, int));
static int NEAR evalinput __P_((int));


static VOID NEAR resettermattr(w)
int w;
{
	if (w >= 0) {
		pty[w].fg = pty[w].bg = (short)-1;
		pty[w].attr = (u_short)0;
	}
	else if (last_attr || last_fg >= (short)0 || last_bg >= (short)0) {
		putterm(T_NORMAL);
		last_attr = (u_short)0;
		last_fg = last_bg = (short)-1;
	}
}

static VOID NEAR resettermcode(w)
int w;
{
	pty[w].termflags &= ~T_CODEALT;
	strcpy(pty[w].codeselect, "(B");
}

static VOID NEAR resettabstop(w)
int w;
{
	int x;

	pty[w].ntabstop = (u_char)0;
	for (x = pty[w].min_x; x < pty[w].max_x; x += 8) {
		if (pty[w].ntabstop >= (u_char)MAXTABSTOP) break;
		pty[w].tabstop[(pty[w].ntabstop)++] = x;
	}
}

VOID resetptyterm(w, clean)
int w, clean;
{
	int min_x, min_y, max_y;

	last_x = last_y = (short)-1;
	if (w < 0 || w >= MAXWINDOWS + 1) return;

	min_x = pty[w].min_x;
	min_y = pty[w].min_y;
	max_y = pty[w].max_y;

	pty[w].min_x = (short)0;
	pty[w].max_x = n_column;
#ifdef	_NOORIGSHELL
	if (w == MAXWINDOWS)
#else
	if (w == MAXWINDOWS || isshptymode())
#endif
	{
		pty[w].min_y = (short)0;
		pty[w].max_y = n_line;
	}
	else {
		pty[w].min_y = filetop(w);
		pty[w].max_y = pty[w].min_y + winvar[w].v_fileperrow;
	}

	if (clean) {
		pty[w].cur_x = pty[w].save_x = pty[w].min_x;
		pty[w].cur_y = pty[w].save_y = pty[w].max_y - 1;
		pty[w].min_scroll = pty[w].min_y;
		pty[w].max_scroll = pty[w].max_y - 1;
		pty[w].escmode = '\0';
		pty[w].last1 = pty[w].last2 = (short)-1;
		pty[w].termflags = (u_short)0;
		resettermattr(w);
		resettermcode(w);
		resettabstop(w);
	}
	else {
		pty[w].cur_x += pty[w].min_x - min_x;
		pty[w].cur_y += pty[w].min_y - min_y;
		pty[w].save_x += pty[w].save_x - min_x;
		pty[w].save_y += pty[w].save_y - min_y;
		pty[w].min_scroll += pty[w].min_y - min_y;
		pty[w].max_scroll += pty[w].max_y - max_y;
		biasxy(w, &(pty[w].cur_x), &(pty[w].cur_y));
		biasxy(w, &(pty[w].save_x), &(pty[w].save_y));
		biasxy(w, NULL, &(pty[w].min_scroll));
		biasxy(w, NULL, &(pty[w].max_scroll));
	}
}

static VOID NEAR biasxy(w, xp, yp)
int w;
short *xp, *yp;
{
	if (xp) {
		if (*xp < pty[w].min_x) *xp = pty[w].min_x;
		else if (*xp >= pty[w].max_x) *xp = pty[w].max_x - 1;
	}
	if (yp) {
		if (*yp < pty[w].min_y) *yp = pty[w].min_y;
		else if (*yp >= pty[w].max_y) *yp = pty[w].max_y - 1;
	}
}

static VOID NEAR settermattr(w)
int w;
{
	char *cp;

	if ((last_fg >= (short)0 && pty[w].fg < (short)0)
	|| (last_bg >= (short)0 && pty[w].bg < (short)0))
		resettermattr(-1);

	if (last_attr != pty[w].attr) {
		putterm(T_NORMAL);
		if (pty[w].attr & A_BOLD) putterm(T_BOLD);
		if (pty[w].attr & A_REVERSE) putterm(T_REVERSE);
		if (pty[w].attr & A_DIM) putterm(T_DIM);
		if (pty[w].attr & A_BLINK) putterm(T_BLINK);
		if (pty[w].attr & A_STANDOUT) putterm(T_STANDOUT);
		if (pty[w].attr & A_UNDERLINE) putterm(T_UNDERLINE);
		last_attr = pty[w].attr;
	}

	if (last_fg != pty[w].fg && pty[w].fg >= (short)0) {
		if ((cp = tparamstr(termstr[T_FGCOLOR], pty[w].fg, 0))) {
			tputs2(cp, 1);
			free(cp);
		}
		last_fg = pty[w].fg;
	}
	if (last_bg != pty[w].bg && pty[w].bg >= (short)0) {
		if ((cp = tparamstr(termstr[T_BGCOLOR], pty[w].bg, 0))) {
			tputs2(cp, 1);
			free(cp);
		}
		last_bg = pty[w].bg;
	}
}

static VOID NEAR settermcode(w)
int w;
{
	if ((last_termflags & T_CODEALT) != (pty[w].termflags & T_CODEALT)) {
		if (pty[w].termflags & T_CODEALT) {
			putch2('\016');
			last_termflags |= T_CODEALT;
		}
		else {
			putch2('\017');
			last_termflags &= ~T_CODEALT;
		}
	}
	if (strcmp(&(last_codeselect[1]), pty[w].codeselect)) {
		last_codeselect[0] = '\033';
		strcpy(&(last_codeselect[1]), pty[w].codeselect);
		tputs2(last_codeselect, 1);
	}
}

static VOID NEAR surelocate(w, forced)
int w, forced;
{
	if (!forced && pty[w].cur_x == last_x && pty[w].cur_y == last_y)
		return;

	resettermattr(-1);
	locate(pty[w].cur_x, pty[w].cur_y);
	last_x = pty[w].cur_x;
	last_y = pty[w].cur_y;
}

static VOID NEAR reallocate(w, x, y)
{
	if (pty[w].attr || pty[w].fg >= (short)0 || pty[w].bg >= (short)0) {
		surelocate(w, 1);
		settermattr(w);
	}

	pty[w].cur_x = x;
	pty[w].cur_y = y;
	biasxy(w, &(pty[w].cur_x), &(pty[w].cur_y));
	locate(pty[w].cur_x, pty[w].cur_y);
	tflush();
	last_x = pty[w].cur_x;
	last_y = pty[w].cur_y;
}

#ifdef	SIGWINCH
static int wintr(VOID_A)
{
	int duperrno;

	duperrno = errno;
	signal2(SIGWINCH, SIG_IGN);
	pollscreen(1);
	ptywinched++;
	signal2(SIGWINCH, (sigcst_t)wintr);
	errno = duperrno;

	return(0);
}

static VOID NEAR evalsignal(VOID_A)
{
	int i, xmax, ymax;

	if (!ptywinched) return;

	pollscreen(0);
	for (i = 0; i < MAXWINDOWS; i++) {
		xmax = pty[i].max_x - pty[i].min_x;
		ymax = pty[i].max_y - pty[i].min_y;
		resetptyterm(i, 0);
		VOID_C setwsize(ptylist[i].fd,
			pty[i].max_x - pty[i].min_x,
			pty[i].max_y - pty[i].min_y);

		if (xmax != pty[i].max_x - pty[i].min_x
		|| ymax != pty[i].max_y - pty[i].min_y) {
			pty[i].cur_x = pty[i].save_x = pty[i].min_x;
			pty[i].cur_y = pty[i].save_y = pty[i].max_y - 1;
		}
	}
	resetptyterm(i, 0);
	ptywinched = 0;
}
#endif	/* SIGWINCH */

static int NEAR convkey(w, kp)
int w;
keyseq_t *kp;
{
	kp -> str = NULL;
	if (pty[w].termflags & T_NOAPPLICURSOR) switch (kp -> code) {
		case K_UP:
			kp -> str = "\033[A";
			break;
		case K_DOWN:
			kp -> str = "\033[B";
			break;
		case K_RIGHT:
			kp -> str = "\033[C";
			break;
		case K_LEFT:
			kp -> str = "\033[D";
			break;
		default:
			break;
	}

	if (kp -> str) {
		kp -> len = (u_char)strlen(kp -> str);
		return(0);
	}

	if ((pty[w].termflags & T_NOAPPLIKEY)
	&& (kp -> code >= K_F('*') && kp -> code <= K_DL)) {
		if (kp -> code == K_F('?')) kp -> code = K_CR;
		else kp -> code -= K_F0;
		kp -> len = (u_char)0;
		return(-1);
	}

	return((getdefkeyseq(kp) < 0) ? getkeyseq(kp) : 0);
}

#ifndef	_NOKANJICONV
static int NEAR ptygetch(fd)
int fd;
{
	u_short ch;

	if (recvbuf(fd, &ch, sizeof(ch)) < 0 || (ch & 0xff00)) return(-1);
	return(ch);
}

static char *NEAR ptykconv(buf, buf2, incode, outcode)
char *buf, *buf2;
int incode, outcode;
{
	char *cp, *tmp;

	tmp = kanjiconv2(buf2, buf, MAXKANJIBUF, incode, DEFCODE, L_TERMINAL);
	if (kanjierrno) return(buf);
	cp = (tmp == buf) ? buf2 : buf;
	cp = kanjiconv2(cp, tmp, MAXKANJIBUF, DEFCODE, outcode, L_TERMINAL);
	if (kanjierrno) return(tmp);

	return(cp);
}
#endif	/* !_NOKANJICONV */

static VOID NEAR evalscroll(w, n, c)
int w, n, c;
{
	resettermattr(-1);
	regionscroll(n, c, pty[w].cur_x, pty[w].cur_y,
		pty[w].min_scroll, pty[w].max_scroll);
	tflush();
	last_x = pty[w].cur_x;
	last_y = pty[w].cur_y;
}

static VOID NEAR evallf(w)
int w;
{
	if (pty[w].cur_y < pty[w].max_y
	&& pty[w].cur_y == pty[w].max_scroll)
		evalscroll(w, C_SCROLLFORW, 1);
	else reallocate(w, pty[w].cur_x, pty[w].cur_y + 1);
}

static VOID NEAR evalnormal(w, c)
int w, c;
{
#ifndef	_NOKANJICONV
	char buf[MAXKANJIBUF + 1], buf2[MAXKANJIBUF + 1];
	int len, code, incode, outcode;
#endif
	int width;

#ifndef	_NOKANJICONV
	outcode = (outputkcode != NOCNV) ? outputkcode : DEFCODE;
	incode = (w < MAXWINDOWS && ptylist[w].outcode != NOCNV)
		? ptylist[w].outcode : outcode;
	code = (incode != M_UTF8) ? incode : UTF8;
#endif

	if ((pty[w].termflags & T_NOAUTOMARGIN)
	&& pty[w].cur_x >= pty[w].max_x - 1)
		return;

	width = 1;
	if (pty[w].last1 >= (short)0) {
		if (pty[w].termflags & T_MULTIBYTE) width++;
#ifdef	_NOKANJICONV
# ifdef	CODEEUC
		else if (pty[w].last1 == C_EKANA) /*EMPTY*/;
# endif
#else	/* !_NOKANJICONV */
		else if (code == EUC) {
			if (pty[w].last1 != C_EKANA) width++;
		}
		else if (code == UTF8) {
			if (!ismsb(c)) pty[w].last1 = pty[w].last2 = (short)-1;
			else if (pty[w].last2 >= (short)0) width++;
			else if (!isutf2(pty[w].last1, c)) {
				pty[w].last2 = c;
				return;
			}
		}
#endif	/* !_NOKANJICONV */
		else width++;
	}
#ifdef	_NOKANJICONV
# ifdef	CODEEUC
	else if (c == C_EKANA || iseuc(c))
# else
	else if (issjis1(c))
# endif
#else	/* !_NOKANJICONV */
	else if ((pty[w].termflags & T_MULTIBYTE)
	|| (code == EUC && (c == C_EKANA || iseuc(c)))
	|| (code == SJIS && issjis1(c))
	|| (code == UTF8 && ismsb(c)))
#endif	/* !_NOKANJICONV */
	{
		pty[w].last1 = c;
		pty[w].last2 = (short)-1;
		return;
	}

	if (pty[w].cur_x + width > pty[w].max_x) {
		pty[w].cur_x = pty[w].min_x;
		evallf(w);
	}

	surelocate(w, 0);
	settermattr(w);
	settermcode(w);
#ifndef	_NOKANJICONV
	if (!(pty[w].termflags & T_MULTIBYTE) && incode != outcode) {
		len = 0;
		if (pty[w].last1 >= (short)0) buf[len++] = pty[w].last1;
		if (pty[w].last2 >= (short)0) buf[len++] = pty[w].last2;
		if (!len) putch2(c);
		else {
			buf[len++] = c;
			buf[len] = '\0';
			cputs2(ptykconv(buf, buf2, incode, outcode));
		}
	}
	else
#endif	/* !_NOKANJICONV */
	{
		if (pty[w].last1 >= (short)0) putch2(pty[w].last1);
		if (pty[w].last2 >= (short)0) putch2(pty[w].last2);
		putch2(c);
	}
	tflush();
	pty[w].cur_x += width;
	last_x += width;

	if (n_lastcolumn < n_column && pty[w].cur_x >= pty[w].max_x) {
		pty[w].cur_x = pty[w].min_x;
		evallf(w);
	}

	pty[w].last1 = pty[w].last2 = (short)-1;
}

static VOID NEAR evalcontrol(w, c)
int w, c;
{
	int i;

	switch (c) {
		case '\b':
			reallocate(w, pty[w].cur_x - 1, pty[w].cur_y);
			break;
		case '\t':
			for (i = 0; i < pty[w].ntabstop; i++)
				if (pty[w].cur_x < pty[w].tabstop[i]) break;
			i = (i < pty[w].ntabstop)
				? pty[w].tabstop[i] : pty[w].max_x - 1;
			reallocate(w, i, pty[w].cur_y);
			break;
		case '\r':
			reallocate(w, pty[w].min_x, pty[w].cur_y);
			break;
		case '\n':
		case '\v':
		case '\f':
			evallf(w);
			break;
		case '\007':
			putterm(T_BELL);
			tflush();
			break;
		case '\033':
			pty[w].escmode = c;
			break;
		case '\016':
			pty[w].termflags |= T_CODEALT;
			break;
		case '\017':
			pty[w].termflags &= ~T_CODEALT;
			break;
		default:
			break;
	}
}

static VOID NEAR evalescape(w, c)
int w, c;
{
	int i;

	pty[w].escmode = '\0';
	switch (c) {
		case '7':
			pty[w].save_x = pty[w].cur_x;
			pty[w].save_y = pty[w].cur_y;
			break;
		case '8':
			pty[w].cur_x = pty[w].save_x;
			pty[w].cur_y = pty[w].save_y;
			biasxy(w, &(pty[w].cur_x), &(pty[w].cur_y));
			surelocate(w, 1);
			tflush();
			break;
		case '=':
			pty[w].termflags &= ~T_NOAPPLIKEY;
			break;
		case '>':
			pty[w].termflags |= T_NOAPPLIKEY;
			break;
		case 'D':
			evallf(w);
			break;
		case 'E':
			pty[w].cur_x = pty[w].min_x;
			evallf(w);
			break;
		case 'H':
			if (pty[w].ntabstop >= (u_char)MAXTABSTOP) break;
			for (i = 0; i < pty[w].ntabstop; i++)
				if (pty[w].cur_x <= pty[w].tabstop[i]) break;
			if (i >= pty[w].ntabstop)
				pty[w].tabstop[(pty[w].ntabstop)++] =
					pty[w].cur_x;
			else if (pty[w].cur_x == pty[w].tabstop[i]) break;
			else {
				memmove(&(pty[w].tabstop[i + 1]),
					&(pty[w].tabstop[i]),
					((pty[w].ntabstop)++ - i)
						* sizeof(u_short));
				pty[w].tabstop[i] = pty[w].cur_x;
			}
			break;
		case 'M':
			if (pty[w].cur_y >= pty[w].min_y
			&& pty[w].cur_y == pty[w].min_scroll)
				evalscroll(w, L_INSERT, 1);
			else reallocate(w, pty[w].cur_x, pty[w].cur_y - 1);
			break;
		case 'c':
			pty[w].termflags = (u_short)0;
			resettermattr(w);
			resettermcode(w);
			resettabstop(w);
			surelocate(w, 1);
			tflush();
			pty[w].min_scroll = pty[w].min_y;
			pty[w].max_scroll = pty[w].max_y - 1;
			break;
		case '[':
		case '$':
		case '(':
		case ')':
		case '*':
		case '+':
		case '&':
			pty[w].escmode = c;
			pty[w].nparam = pty[w].nchar = (u_char)0;
			pty[w].escparam[0] = (short)-1;
			break;
		default:
			break;
	}
}

static VOID NEAR evalcsi(w, c, fd)
int w, c, fd;
{
	char buf[sizeof(SIZEFMT) + (MAXLONGWIDTH - 2) * 2];
	short min, max;
	int i, n, x, y;

	if (pty[w].escmode == '[') pty[w].escmode = ']';
	if (c >= '0' && c <= '9') {
		if (pty[w].nparam >= MAXESCPARAM) return;
		if (pty[w].escparam[pty[w].nparam] < (short)0)
			pty[w].escparam[pty[w].nparam] = (short)0;
		if (pty[w].escparam[pty[w].nparam] < (short)1000) {
			pty[w].escparam[pty[w].nparam] *= 10;
			pty[w].escparam[pty[w].nparam] += c - '0';
		}
		return;
	}
	else if (c == ';') {
		if (++(pty[w].nparam) >= MAXESCPARAM)
			pty[w].nparam = MAXESCPARAM;
		else pty[w].escparam[pty[w].nparam] = (short)-1;
		return;
	}
	else if (c >= ' ' && c <= '/') {
		if (pty[w].nchar < MAXESCCHAR)
			pty[w].escchar[(pty[w].nchar)++] = c;
		return;
	}

	switch(c) {
		case 'A':
			if (pty[w].escmode != ']') break;
			n = pty[w].escparam[0];
			if (n <= 0) n = 1;
			reallocate(w, pty[w].cur_x, pty[w].cur_y - n);
			break;
		case 'B':
			if (pty[w].escmode != ']') break;
			n = pty[w].escparam[0];
			if (n <= 0) n = 1;
			reallocate(w, pty[w].cur_x, pty[w].cur_y + n);
			break;
		case 'C':
			if (pty[w].escmode != ']') break;
			n = pty[w].escparam[0];
			if (n <= 0) n = 1;
			reallocate(w, pty[w].cur_x + n, pty[w].cur_y);
			break;
		case 'D':
			if (pty[w].escmode != ']') break;
			n = pty[w].escparam[0];
			if (n <= 0) n = 1;
			reallocate(w, pty[w].cur_x - n, pty[w].cur_y);
			break;
		case 'H':
		case 'f':
			if (pty[w].escmode != ']') break;
			x = pty[w].min_x + ((pty[w].nparam <= (u_char)0)
				? 0 : pty[w].escparam[1] - 1);
			y = pty[w].min_y + (pty[w].escparam[0] - 1);
			reallocate(w, x, y);
			pty[w].save_x = pty[w].cur_x;
			pty[w].save_y = pty[w].cur_y;
			break;
		case 'm':
			if (pty[w].escmode != ']') break;
			for (i = 0; i <= pty[w].nparam; i++)
			switch (pty[w].escparam[i]) {
				case -1:
				case 0:
					pty[w].attr = (u_short)0;
					pty[w].fg = pty[w].bg = (short)-1;
					break;
				case 1:
					pty[w].attr |= A_BOLD;
					break;
				case 2:
					pty[w].attr |= A_DIM;
					break;
				case 4:
					pty[w].attr |= A_UNDERLINE;
					break;
				case 5:
					pty[w].attr |= A_BLINK;
					break;
				case 7:
					pty[w].attr |= A_REVERSE;
					break;
				case 22:
					pty[w].attr &= ~A_BOLD;
					break;
				case 24:
					pty[w].attr &= ~A_UNDERLINE;
					break;
				case 25:
					pty[w].attr &= ~A_BLINK;
					break;
				case 27:
					pty[w].attr &= ~A_REVERSE;
					break;
				default:
					n = pty[w].escparam[i];
					if (n >= 30 && n <= 37)
						pty[w].fg = n % 10;
					else if (n >= 40 && n <= 47)
						pty[w].bg = n % 10;
					break;
			}
			tflush();
			break;
		case 's':
			if (pty[w].escmode != ']') break;
			pty[w].save_x = pty[w].cur_x;
			pty[w].save_y = pty[w].cur_y;
			break;
		case 'u':
			if (pty[w].escmode != ']') break;
			pty[w].cur_x = pty[w].save_x;
			pty[w].cur_y = pty[w].save_y;
			biasxy(w, &(pty[w].cur_x), &(pty[w].cur_y));
			surelocate(w, 1);
			tflush();
			break;
		case 'L':
			if (pty[w].escmode != ']') break;
			n = pty[w].escparam[0];
			if (n <= 0) n = 1;
			else if (n > pty[w].max_y - pty[w].min_y)
				n = pty[w].max_y - pty[w].min_y;
			evalscroll(w, L_INSERT, n);
			break;
		case 'M':
			if (pty[w].escmode != ']') break;
			n = pty[w].escparam[0];
			if (n <= 0) n = 1;
			else if (n > pty[w].max_y - pty[w].min_y)
				n = pty[w].max_y - pty[w].min_y;
			evalscroll(w, L_DELETE, n);
			break;
		case 'J':
			if (pty[w].escmode != ']') break;
			switch (pty[w].escparam[0]) {
				case -1:
				case 0:
					min = pty[w].cur_y;
					max = pty[w].max_y;
					break;
				case 1:
					min = pty[w].min_y;
					max = pty[w].cur_y + 1;
					break;
				case 2:
					min = pty[w].min_y;
					max = pty[w].max_y;
					break;
				default:
					min = max = (short)-1;
					break;
			}
			if (min < (short)0 || max < (short)0) break;
			for (i = min; i < max; i++) {
				locate(pty[w].min_x, i);
				putterm(L_CLEAR);
			}
			surelocate(w, 1);
			tflush();
			break;
		case 'K':
			if (pty[w].escmode != ']'
			&& pty[w].escmode != '?')
				break;
			switch (pty[w].escparam[0]) {
				case -1:
				case 0:
					surelocate(w, 1);
					putterm(L_CLEAR);
					break;
				case 1:
					surelocate(w, 1);
					putterm(L_CLEARBOL);
					break;
				case 2:
					locate(pty[w].min_x, pty[w].cur_y);
					putterm(L_CLEAR);
					surelocate(w, 1);
					break;
				default:
					break;
			}
			tflush();
			break;
		case '@':
			if (pty[w].escmode != ']') break;
			n = pty[w].escparam[0];
			if (n <= 0) n = 1;
			surelocate(w, 1);
			if (*termstr[C_INSERT]) while (n--) putterm(C_INSERT);
			else while (n--) tputs2("\033[@", 1);
			tflush();
			break;
		case 'P':
			if (pty[w].escmode != ']') break;
			n = pty[w].escparam[0];
			if (n <= 0) n = 1;
			surelocate(w, 1);
			if (*termstr[C_DELETE]) while (n--) putterm(C_DELETE);
			else while (n--) tputs2("\033[P", 1);
			tflush();
			break;
		case 'h':
			if (pty[w].escmode != '?') break;
			for (i = 0; i <= pty[w].nparam; i++)
			switch (pty[w].escparam[i]) {
				case 1:
					pty[w].termflags &= ~T_NOAPPLICURSOR;
					break;
				case 3:
					resettabstop(w);
					pty[w].min_scroll = pty[w].min_y;
					pty[w].max_scroll = pty[w].max_y - 1;
					break;
				case 7:
					pty[w].termflags &= ~T_NOAUTOMARGIN;
					break;
				case 25:
					putterm(T_NORMALCURSOR);
					tflush();
					break;
				default:
					break;
			}
			break;
		case 'l':
			if (pty[w].escmode != '?') break;
			for (i = 0; i <= pty[w].nparam; i++)
			switch (pty[w].escparam[i]) {
				case 1:
					pty[w].termflags |= T_NOAPPLICURSOR;
					break;
				case 3:
					resettabstop(w);
					pty[w].min_scroll = pty[w].min_y;
					pty[w].max_scroll = pty[w].max_y - 1;
					break;
				case 7:
					pty[w].termflags |= T_NOAUTOMARGIN;
					break;
				case 25:
					putterm(T_NOCURSOR);
					tflush();
					break;
				default:
					break;
			}
			break;
		case 'g':
			if (pty[w].escmode != ']') break;
			switch (pty[w].escparam[0]) {
				case 0:
					if (pty[w].ntabstop <= (u_char)0)
						break;
					for (i = 0; i < pty[w].ntabstop; i++)
						if (pty[w].cur_x
						<= pty[w].tabstop[i])
							break;
					if (pty[w].cur_x != pty[w].tabstop[i])
						break;
					memmove(&(pty[w].tabstop[i]),
						&(pty[w].tabstop[i + 1]),
						(--(pty[w].ntabstop) - i)
						* sizeof(u_short));
					break;
				case 3:
					pty[w].ntabstop = (u_char)0;
					break;
				default:
					break;
			}
			break;
		case 'n':
			if (pty[w].escmode != ']') break;
			i = 0;
			switch (pty[w].escparam[0]) {
				case 5:
					i = snprintf2(buf, sizeof(buf),
						"\033[0n");
					break;
				case 6:
					i = snprintf2(buf, sizeof(buf),
						SIZEFMT,
						pty[w].cur_y
						- pty[w].min_y + 1,
						pty[w].cur_x
						- pty[w].min_x + 1);
					break;
				case 99:
					buf[i++] = '\n';
					break;
				default:
					break;
			}
			if (i > 0) sendbuf(fd, buf, i);
			break;
		case 'r':
			if (pty[w].escmode != ']') break;
			min = pty[w].min_y + (pty[w].escparam[0] - 1);
			if (pty[w].nparam <= (u_char)0
			|| pty[w].escparam[1] <= (short)0)
				max = pty[w].max_y - 1;
			else max = pty[w].min_y + (pty[w].escparam[1] - 1);
			biasxy(w, NULL, &min);
			biasxy(w, NULL, &max);
			if (min < max) {
				pty[w].min_scroll = min;
				pty[w].max_scroll = max;
			}
			break;
		default:
			break;
	}

	pty[w].escmode = '\0';
}

static VOID NEAR evalcodeselect(w, c)
int w, c;
{
	if (c >= ' ' && c <= '/') {
		if (pty[w].nchar < MAXESCCHAR)
			pty[w].escchar[(pty[w].nchar)++] = c;
		return;
	}

	if (c < 0x30 || c > 0x7e) return;

	switch (pty[w].escmode) {
		case '$':
			if (pty[w].nchar > (u_char)1 || (pty[w].nchar
			&& (pty[w].escchar[0] < '('
			|| pty[w].escchar[0] > '+'))) {
				pty[w].escmode = '\0';
				return;
			}
			pty[w].termflags |= T_MULTIBYTE;
			break;
		default:
			if (pty[w].nchar) {
				pty[w].escmode = '\0';
				return;
			}
			pty[w].termflags &= ~T_MULTIBYTE;
			break;
	}

	pty[w].codeselect[0] = pty[w].escmode;
	memcpy(&(pty[w].codeselect[1]), pty[w].escchar, pty[w].nchar);
	pty[w].codeselect[++(pty[w].nchar)] = c;
	pty[w].codeselect[++(pty[w].nchar)] = '\0';

	pty[w].escmode = '\0';
}

static VOID NEAR evaloutput(w)
int w;
{
	u_char uc;

	if (pty[w].termflags & T_LOCKED) return;
	if (recvbuf(ptylist[w].fd, &uc, sizeof(uc)) < 0) return;

	switch (pty[w].escmode) {
		case '\033':
			evalescape(w, uc);
			break;
		case '[':
			if (uc == '<' || uc == '?') {
				pty[w].escmode = uc;
				break;
			}
/*FALLTHRU*/
		case ']':
		case '>':
		case '?':
			evalcsi(w, uc, ptylist[w].fd);
			break;
		case '$':
		case '(':
		case ')':
		case '*':
		case '+':
		case '&':
			evalcodeselect(w, uc);
			break;
		default:
			if (uc < ' ' || uc == '\177') evalcontrol(w, uc);
			else evalnormal(w, uc);
			break;
	}
}

static int NEAR chgattr(n, c)
int n, c;
{
	switch (n) {
		case L_CLEAR:
			surelocate(MAXWINDOWS, 1);
			putterm(L_CLEAR);
			break;
		case T_NORMAL:
			pty[MAXWINDOWS].attr = (u_short)0;
			pty[MAXWINDOWS].fg = pty[MAXWINDOWS].bg = (short)-1;
			break;
		case T_BOLD:
			pty[MAXWINDOWS].attr |= A_BOLD;
			break;
		case T_REVERSE:
			pty[MAXWINDOWS].attr |= A_REVERSE;
			break;
		case T_DIM:
			pty[MAXWINDOWS].attr |= A_DIM;
			break;
		case T_BLINK:
			pty[MAXWINDOWS].attr |= A_BLINK;
			break;
		case T_STANDOUT:
			pty[MAXWINDOWS].attr |= A_STANDOUT;
			break;
		case T_UNDERLINE:
			pty[MAXWINDOWS].attr |= A_UNDERLINE;
			break;
		case END_STANDOUT:
			pty[MAXWINDOWS].attr &= ~A_STANDOUT;
			break;
		case END_UNDERLINE:
			pty[MAXWINDOWS].attr &= ~A_UNDERLINE;
			break;
		case L_INSERT:
		case L_DELETE:
		case C_SCROLLFORW:
		case C_SCROLLREV:
			evalscroll(MAXWINDOWS, n, c);
			break;
		case C_INSERT:
		case C_DELETE:
			surelocate(MAXWINDOWS, 1);
			while (c--) putterm(n);
			tflush();
			break;
		case C_HOME:
			pty[MAXWINDOWS].cur_x =
			pty[MAXWINDOWS].cur_y = (short)0;
			reallocate(MAXWINDOWS, 0, 0);
			break;
		case C_RETURN:
			reallocate(MAXWINDOWS,
				pty[MAXWINDOWS].min_x, pty[MAXWINDOWS].cur_y);
			break;
		case C_NEWLINE:
			while (c--) evallf(MAXWINDOWS);
			break;
		case C_UP:
		case C_NUP:
			reallocate(MAXWINDOWS,
				pty[MAXWINDOWS].cur_x,
				pty[MAXWINDOWS].cur_y - c);
			break;
		case C_DOWN:
		case C_NDOWN:
			reallocate(MAXWINDOWS,
				pty[MAXWINDOWS].cur_x,
				pty[MAXWINDOWS].cur_y + c);
			break;
		case C_RIGHT:
		case C_NRIGHT:
			reallocate(MAXWINDOWS,
				pty[MAXWINDOWS].cur_x + c,
				pty[MAXWINDOWS].cur_y);
			break;
		case C_LEFT:
		case C_NLEFT:
			reallocate(MAXWINDOWS,
				pty[MAXWINDOWS].cur_x - c,
				pty[MAXWINDOWS].cur_y);
			break;
		default:
			return(n);
/*NOTREACHED*/
			break;
	}

	return(-1);
}

static int NEAR directoutput(fd, n)
int fd, n;
{
	ptyinfo_t tmp;
	char *s, *arg;
	p_id_t pid;
	short w1, w2, row[MAXWINDOWS];
	int i;

	if (recvbuf(fd, &w1, sizeof(w1)) < 0) return(0);

	switch (n) {
		case TE_PUTCH2:
			surelocate(MAXWINDOWS, 0);
			settermattr(MAXWINDOWS);
			settermcode(MAXWINDOWS);
			putch2(w1);
			(pty[MAXWINDOWS].cur_x)++;
			last_x++;
			tflush();
			break;
		case TE_CPUTS2:
			s = malloc2(w1 + 1);
			if (recvbuf(fd, s, w1) >= 0) {
				surelocate(MAXWINDOWS, 0);
				settermattr(MAXWINDOWS);
				settermcode(MAXWINDOWS);
				s[w1] = '\0';
				cputs2(s);
				pty[MAXWINDOWS].cur_x += w1;
				last_x += w1;
				tflush();
			}
			free(s);
			break;
		case TE_PUTTERM:
			if ((n = chgattr(w1, 1)) < 0) break;
			surelocate(MAXWINDOWS, 0);
			putterm(n);
			tflush();
			break;
		case TE_PUTTERMS:
			if ((n = chgattr(w1, 1)) < 0) break;
			surelocate(MAXWINDOWS, 0);
			putterms(n);
			tflush();
			break;
		case TE_SETSCROLL:
			if (recvbuf(fd, &w2, sizeof(w2)) < 0) break;

			if (w2 <= (short)0) w2 = pty[MAXWINDOWS].max_y - 1;
			biasxy(MAXWINDOWS, NULL, &w1);
			biasxy(MAXWINDOWS, NULL, &w2);
			if (w1 < w2) {
				pty[MAXWINDOWS].min_scroll = w1;
				pty[MAXWINDOWS].max_scroll = w2;
			}
			break;
		case TE_LOCATE:
			if (recvbuf(fd, &w2, sizeof(w2)) < 0) break;

			reallocate(MAXWINDOWS, w1, w2);
			for (i = 0; i < MAXWINDOWS; i++) {
				if (ptylist[i].pid) continue;
				pty[i].cur_x = w1;
				pty[i].cur_y = w2;
				biasxy(i, &(pty[i].cur_x), &(pty[i].cur_y));
			}
			break;
		case TE_CPUTNL:
			pty[MAXWINDOWS].cur_x = (short)0;
			evallf(MAXWINDOWS);
			break;
		case TE_CHGCOLOR:
			if (recvword(fd, &n) < 0) break;

			if (n) {
				pty[MAXWINDOWS].fg = (w1 == ANSI_BLACK)
					? ANSI_WHITE : ANSI_BLACK;
				pty[MAXWINDOWS].bg = w1;
			}
			else {
				pty[MAXWINDOWS].fg = w1;
				pty[MAXWINDOWS].bg = (short)-1;
			}
			break;
		case TE_MOVECURSOR:
			if (recvbuf(fd, &w2, sizeof(w2)) < 0
			|| recvword(fd, &n) < 0)
				break;
			if (w1 < (short)0) w1 = w2;
			if ((n = chgattr(w1, n)) < 0) break;
			surelocate(MAXWINDOWS, 0);
			putterms(n);
			tflush();
			break;
		case TE_CHANGEWIN:
			if (recvbuf(fd, &pid, sizeof(pid)) < 0) break;

			win = w1;
			if (pid < (p_id_t)0) break;
			resetptyterm(win, (ptylist[win].pid) ? 0 : 1);
			biasxy(win, &(pty[win].cur_x), &(pty[win].cur_y));
			ptylist[win].pid = pid;
			surelocate(win, 1);
			tflush();
			if (pid) {
				VOID_C setwsize(ptylist[win].fd,
					pty[win].max_x - pty[win].min_x,
					pty[win].max_y - pty[win].min_y);
				break;
			}
			for (i = 0; i < MAXWINDOWS; i++)
				if (ptylist[i].pid) break;
			if (i >= MAXWINDOWS) return(-1);
			break;
		case TE_CHANGEWSIZE:
			if (recvword(fd, &n) < 0) break;
			if (n > MAXWINDOWS) n = MAXWINDOWS;

			for (i = 0; i < n; i++)
				if (recvbuf(fd, &(row[i]), sizeof(row[i])) < 0)
					break;

			wheader = w1;
#ifndef	_NOSPLITWIN
			windows = n;
#endif
			for (i = 0; i < n; i++) {
				winvar[i].v_fileperrow = row[i];
				resetptyterm(i, 0);
				VOID_C setwsize(ptylist[i].fd,
					pty[i].max_x - pty[i].min_x,
					pty[i].max_y - pty[i].min_y);
			}
			break;
		case TE_INSERTWIN:
			if (recvword(fd, &n) < 0) break;
			memcpy((char *)&tmp, (char *)&(ptylist[n - 1]),
				sizeof(ptyinfo_t));
			memmove((char *)&(ptylist[w1 + 1]),
				(char *)&(ptylist[w1]),
				(n - 1 - w1) * sizeof(ptyinfo_t));
			memcpy((char *)&(ptylist[w1]), (char *)&tmp,
				sizeof(ptyinfo_t));
			memmove((char *)&(pty[w1 + 1]), (char *)&(pty[w1]),
				(n - 1 - w1) * sizeof(ptyterm_t));
			resetptyterm(w1, 1);
			return(1);
/*NOTREACHED*/
			break;
		case TE_DELETEWIN:
			if (recvword(fd, &n) < 0) break;
			memcpy((char *)&tmp, (char *)&(ptylist[w1]),
				sizeof(ptyinfo_t));
			memmove((char *)&(ptylist[w1]),
				(char *)&(ptylist[w1 + 1]),
				(n - w1) * sizeof(ptyinfo_t));
			memcpy((char *)&(ptylist[n]), (char *)&tmp,
				sizeof(ptyinfo_t));
			memmove((char *)&(pty[w1]), (char *)&(pty[w1 + 1]),
				(n - w1) * sizeof(ptyterm_t));
			return(1);
/*NOTREACHED*/
			break;
		case TE_LOCKBACK:
			pty[w1].termflags |= T_LOCKED;
			sendbuf(ptylist[w1].fd, "\n", 1);
			break;
		case TE_UNLOCKBACK:
			pty[w1].termflags &= ~T_LOCKED;
			sendbuf(ptylist[w1].fd, "\n", 1);
			break;
#ifndef	_NOKANJICONV
		case TE_CHANGEKCODE:
			if (recvbuf(fd, &w2, sizeof(w2)) < 0) break;
			inputkcode = w1;
			outputkcode = w2;
			break;
		case TE_CHANGEINKCODE:
			if (recvbuf(fd, &w2, sizeof(w2)) < 0) break;
			ptylist[w1].incode = (char)w2;
			break;
		case TE_CHANGEOUTKCODE:
			if (recvbuf(fd, &w2, sizeof(w2)) < 0) break;
			ptylist[w1].outcode = (char)w2;
			break;
#endif	/* !_NOKANJICONV */
		case TE_AWAKECHILD:
			if (recvbuf(fd, &n, sizeof(n)) < 0
			|| recvstring(fd, &s) < 0)
				break;
			if (recvstring(fd, &arg) < 0) {
				if (s) free(s);
				break;
			}
			resetptyterm(w1, 1);

			sendbuf(ptylist[w1].fd, &n, sizeof(n));
			sendstring(ptylist[w1].fd, s);
			sendstring(ptylist[w1].fd, arg);
			if (s) free(s);
			if (arg) free(arg);
			break;
		default:
			break;
	}

	return(0);
}

static int NEAR evalinput(fd)
int fd;
{
#ifdef	_NOKANJICONV
	char buf[2];
#else
	char buf[MAXKANJIBUF + 1], buf2[MAXKANJIBUF + 1];
	int cnv, code, incode, outcode;
#endif
	keyseq_t key;
	int n;

	if (recvbuf(fd, &(key.code), sizeof((key.code))) < 0) return(0);

#ifndef	_NOKANJICONV
	cnv = 0;
	incode = (inputkcode != NOCNV) ? inputkcode : DEFCODE;
	outcode = (win < MAXWINDOWS && ptylist[win].incode != NOCNV)
		? ptylist[win].incode : incode;
	code = (incode != M_UTF8) ? incode : UTF8;
#endif

	if (ismetakey(key.code)) {
		key.len = (u_char)2;
		key.str = buf;
		buf[0] = K_ESC;
		buf[1] = (key.code & 0xff);
	}
#if	!defined (_NOKANJICONV) || defined (CODEEUC)
# ifdef	_NOKANJICONV
	else if (isekana2(key.code)) {
# else
	else if (inputkcode == EUC && isekana2(key.code)) {
		if (incode != outcode) cnv++;
# endif
		key.len = (u_char)2;
		key.str = buf;
		buf[0] = C_EKANA;
		buf[1] = (key.code & 0xff);
	}
#endif	/* !_NOKANJICONV || CODEEUC */
	else if (!(key.code & 01000) && key.code > K_MAX) {
		n = directoutput(fd, key.code);
		if (win < MAXWINDOWS) {
			surelocate(win, 1);
			last_x = last_y = (short)-1;
			tflush();
		}

		return(n);
	}
	else if (convkey(win, &key) < 0 || !(key.len)) {
		key.len = (u_char)1;
		key.str = buf;
		buf[0] = key.code;
#ifndef	_NOKANJICONV
		if (incode == outcode) /*EMPTY*/;
		else if (inputkcode == SJIS && iskana2(key.code)) cnv++;
		else if (code == UTF8) {
			if (!ismsb(key.code)) /*EMPTY*/;
			else if ((n = ptygetch(fd)) >= 0) {
				buf[1] = n;
				(key.len)++;
				if (isutf2(buf[0], buf[1])) cnv++;
				else if ((n = ptygetch(fd)) >= 0) {
					cnv++;
					buf[2] = n;
					(key.len)++;
				}
			}
		}
		else if (isinkanji1(key.code) && (n = ptygetch(fd)) >= 0) {
			cnv++;
			buf[1] = n;
			(key.len)++;
		}
#endif	/* !_NOKANJICONV */
	}

#ifndef	_NOKANJICONV
	if (cnv) {
		buf[key.len] = '\0';
		key.str = ptykconv(buf, buf2, incode, outcode);
		key.len = (u_char)strlen(key.str);
	}
#endif

	if (win < MAXWINDOWS) sendbuf(ptylist[win].fd, key.str, key.len);

	return(0);
}

int backend(VOID_A)
{
	char result[MAXWINDOWS + 1];
	int i, n, fds[MAXWINDOWS];

	hideclock = -1;
	dumbterm = 1;
	sigvecset(0);
#ifdef	SIGWINCH
	ptywinched = 0;
	pollscreen(-1);
	signal2(SIGWINCH, (sigcst_t)wintr);
#endif

	ttyiomode(0);

	n = 0;
	while (n >= 0) {
#ifdef	SIGWINCH
		evalsignal();
#endif

		for (i = 0; i < MAXWINDOWS; i++) fds[i] = ptylist[i].fd;
		if (selectpty(emufd, fds, result, -1) <= 0) continue;

		if (result[MAXWINDOWS] && (n = evalinput(emufd)) > 0) continue;

		for (i = 0; i < MAXWINDOWS; i++)
			if (result[i] && ptylist[i].pid) evaloutput(i);
	}

	surelocate(MAXWINDOWS, 0);
	settermattr(MAXWINDOWS);
	settermcode(MAXWINDOWS);
	tflush();

	return(0);
}
#endif	/* !_NOPTY */
