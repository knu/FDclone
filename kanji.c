/*
 *	kanji.c
 *
 *	Kanji Convert Function
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"

#include <varargs.h>

#define	ASCII	0
#define	KANA	1
#define	KANJI	2

static int jis7();
static int sjis2euc();
static int euc2sjis();

int inputkcode;
int outputkcode;


int getlang(str, in)
char *str;
int in;
{
	int ret;

	if (!str) ret = NOCNV;
	else if (strstr(str, "SJIS") || strstr(str, "sjis")) ret = SJIS;
	else if (strstr(str, "EUC") || strstr(str, "euc")) ret = EUC;
	else if (strstr(str, "JIS") || strstr(str, "jis")) ret = JIS7;
	else if (strstr(str, "ENG") || strstr(str, "eng") || !strcmp(str, "C"))
		ret = ENG;
	else ret = NOCNV;

	if (in) {
#ifdef	CODEEUC
		if (ret != SJIS) ret = EUC;
#else
		if (ret != EUC) ret = SJIS;
#endif
	}
	return(ret);
}

char *mesconv(jpn, eng)
char *jpn, *eng;
{
	return((outputkcode == ENG) ? eng : jpn);
}

int onkanji1(s, ptr)
u_char *s;
int ptr;
{
	int i;

	if (ptr < 0) return(0);
	if (!ptr) return(iskanji1((int)s[0]));

	for (i = 0; i < ptr; i++) if (iskanji1((int)s[i])) i++;
	if (i > ptr) return(0);
	return(iskanji1((int)s[ptr]));
}

static int jis7(buf, str, incode)
char *buf;
u_char *str;
int incode;
{
	int i, j, mode;

	mode = ASCII;
	for (i = 0, j = 0; str[i] && j < MAXLINESTR - 8; i++, j++) {
		if ((incode == EUC) ? isekana(str, i) : isskana(str, i)) {
			if (!(mode & KANA)) buf[j++] = '\016';
			mode |= KANA;
			buf[j] = str[i] & ~0x80;
			continue;
		}
		if (mode & KANA) buf[j++] = '\017';
		mode &= ~KANA;
		if ((incode == EUC) ? iseuc(str[i]) : issjis1(str[i])) {
			if (!(mode & KANJI)) {
				memcpy(&buf[j], "\033$B", 3);
				j += 3;
			}
			mode |= KANJI;
			if (incode == EUC) {
				buf[j++] = str[i++] & ~0x80;
				buf[j] = str[i] & ~0x80;
			}
			else {
				buf[j++] = str[i] * 2
					- ((str[i] <= 0x9f) ? 0xe1 : 0x161);
				buf[j] = str[++i];
				if (str[i] < 0x9f)
					buf[j] -= (str[i] > 0x7f) ? 0x20 : 0x1f;
				else {
					buf[j] -= 0x7e;
					buf[j - 1]++;
				}
			}
		}
		else {
			if (mode & KANJI) {
				memcpy(&buf[j], "\033(B", 3);
				j += 3;
			}
			mode &= ~KANJI;
			buf[j] = str[i];
		}
	}
	if (mode & KANA) buf[j++] = '\017';
	if (mode & KANJI) {
		memcpy(&buf[j], "\033(B", 3);
		j += 3;
	}
	return(j);
}

static int sjis2euc(buf, str)
char *buf;
u_char *str;
{
	int i, j;

	for (i = 0, j = 0; str[i] && j < MAXLINESTR; i++, j++) {
		if (isskana(str, i)) {
			buf[j++] = 0x8e;
			buf[j] = str[i];
		}
		else if (issjis1(str[i])) {
			buf[j++] = str[i] * 2
				- ((str[i] <= 0x9f) ? 0xe1 : 0x161) + 0x80;
			buf[j] = str[++i];
			if (str[i] < 0x9f)
				buf[j] += (str[i] > 0x7f) ? 0x60 : 0x61;
			else {
				buf[j] += 2;
				buf[j - 1]++;
			}
		}
		else buf[j] = str[i];
	}
	return(j);
}

static int euc2sjis(buf, str)
char *buf;
u_char *str;
{
	int i, j;

	for (i = 0, j = 0; str[i] && j < MAXLINESTR; i++, j++) {
		if (isekana(str, i)) buf[j] = str[i];
		else if (iseuc(str[i])) {
			buf[j++] = ((str[i] - 0x81) >> 1)
				+ ((str[i] < 0xde) ? 0x71 : 0xb1);
			buf[j] = str[++i] - 0x80;
			if (str[i - 1] & 1)
				buf[j] += (str[i] < 0xe0) ? 0x1f : 0x20;
			else buf[j] += 0x7e;
		}
		else buf[j] = str[i];
	}
	return(j);
}

int kanjiconv(buf, str, in, out)
char *buf;
u_char *str;
int in, out;
{
	int len;

	len = strlen(str);
	switch (out) {
		case JIS7:
			len = jis7(buf, str, in);
			break;
		case SJIS:
			if (in == EUC) len = euc2sjis(buf, str);
			else memcpy(buf, str, len);
			break;
		case EUC:
			if (in == SJIS) len = sjis2euc(buf, str, strlen(str));
			else memcpy(buf, str, len);
			break;
		default:
			memcpy(buf, str, len);
			break;
	}
	return(len);
}

int kanjiputs(str)
char *str;
{
	char buf[MAXLINESTR + 1];
	int i;

	i = kanjiconv(buf, str, DEFCODE, outputkcode);
	buf[i] = '\0';
	cputs(buf);
	return(strlen(buf));
}

#ifndef	NOVSPRINTF
/*VARARGS1*/
int kanjiprintf(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list args;
	char buf[MAXLINESTR + 1];

	va_start(args);
	vsprintf(buf, fmt, args);
	va_end(args);
#else
int kanjiprintf(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
{
	char buf[MAXLINESTR + 1];

	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
#endif
	return(kanjiputs(buf));
}

int kanjiputs2(s, len, ptr)
char *s;
int len, ptr;
{
	char dupl[MAXLINESTR + 1];
	int i, p;

	p = ptr;
	if (ptr < 0) ptr = 0;
	for (i = 0; i < len && s[ptr + i]; i++);
	if (i < len) {
		kanjiputs(s + ptr);
		if (p >= 0) for (; i < len; i++) putch(' ');
	}
	else {
		memcpy(dupl, s + ptr, len);
		dupl[len] = '\0';
		if (onkanji1((u_char *)dupl, len - 1)) dupl[len - 1] = ' ';
		kanjiputs(dupl);
	}
	return(len);
}
