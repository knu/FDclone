/*
 *	kanji.c
 *
 *	Kanji Convert Function
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"

#if	MSDOS || defined (__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define	ASCII	0
#define	KANA	1
#define	KANJI	2

#if	!MSDOS && !defined (_NOKANJICONV)
int inputkcode = 0;
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
int outputkcode = 0;
#endif


int onkanji1(s, ptr)
char *s;
int ptr;
{
	int i;

	if (ptr < 0) return(0);
	if (!ptr) return(iskanji1(s[0]));

	for (i = 0; i < ptr; i++) if (iskanji1(s[i])) i++;
	if (i > ptr) return(0);
	return(iskanji1(s[ptr]));
}

#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
/* ARGSUSED */
int getlang(str, in)
char *str;
int in;
{
	int ret;

	if (!str) ret = NOCNV;
#if	!MSDOS && !defined (_NOKANJICONV) 
	else if (strstr2(str, "SJIS") || strstr2(str, "sjis")) ret = SJIS;
	else if (strstr2(str, "EUC") || strstr2(str, "euc")) ret = EUC;
	else if (strstr2(str, "JIS") || strstr2(str, "jis")) ret = JIS7;
#endif
#ifndef	_NOENGMES
	else if (strstr2(str, "ENG") || strstr2(str, "eng")
	|| !strcmp(str, "C")) ret = ENG;
#endif
	else ret = NOCNV;

#if	!MSDOS && !defined (_NOKANJICONV) 
	if (in) {
#ifdef	CODEEUC
		if (ret != SJIS) ret = EUC;
#else
		if (ret != EUC) ret = SJIS;
#endif
	}
#endif	/* !MSDOS && !_NOKANJICONV */
	return(ret);
}
#endif	/* (!MSDOS && !_NOKANJICONV) || !_NOENGMES */

#ifndef	_NOENGMES
char *mesconv(jpn, eng)
char *jpn, *eng;
{
	return((outputkcode == ENG) ? eng : jpn);
}
#endif

#if	!MSDOS && !defined (_NOKANJICONV) 
int jis7(buf, str, incode)
char *buf;
u_char *str;
int incode;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; str[i] && j < MAXLINESTR - 8; i++, j++) {
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

int sjis2ujis(buf, str)
char *buf;
u_char *str;
{
	int i, j;

	for (i = j = 0; str[i] && j < MAXLINESTR - 1; i++, j++) {
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

int ujis2sjis(buf, str)
char *buf;
u_char *str;
{
	int i, j;

	for (i = j = 0; str[i] && j < MAXLINESTR - 1; i++, j++) {
		if (isekana(str, i)) buf[j] = str[i];
		else if (iseuc(str[i])) {
			buf[j++] = ((str[i] - 0x81) >> 1)
				+ ((str[i] < 0xdf) ? 0x71 : 0xb1);
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
char *buf, *str;
int in, out;
{
	int len;

	len = strlen(str);
	switch (out) {
		case JIS7:
			len = jis7(buf, (u_char *)str, in);
			break;
		case SJIS:
			if (in == EUC) len = ujis2sjis(buf, (u_char *)str);
			else memcpy(buf, str, len);
			break;
		case EUC:
			if (in == SJIS) len = sjis2ujis(buf, (u_char *)str);
			else memcpy(buf, str, len);
			break;
		default:
			memcpy(buf, str, len);
			break;
	}
	return(len);
}
#endif	/* !MSDOS && !_NOKANJICONV */

int kanjiputs(str)
char *str;
{
#if	MSDOS || defined (_NOKANJICONV)
	cputs2(str);
	return(strlen(str));
#else
	char buf[MAXLINESTR + 1];
	int i;

	i = kanjiconv(buf, str, DEFCODE, outputkcode);
	buf[i] = '\0';
	cputs2(buf);
	return(i);
#endif
}

#if	MSDOS || defined (__STDC__)
/*VARARGS1*/
int kanjiprintf(CONST char *fmt, ...)
{
	va_list args;
	char buf[MAXLINESTR + 1];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
#else	/* !MSDOS && !__STDC__ */
# ifndef	NOVSPRINTF
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
# else	/* NOVSPRINTF */
int kanjiprintf(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
{
	char buf[MAXLINESTR + 1];

	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
# endif	/* NOVSPRINTF */
#endif	/* !MSDOS && !__STDC__ */
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
		if (p >= 0) for (; i < len; i++) putch2(' ');
	}
	else {
		memcpy(dupl, s + ptr, len);
		dupl[len] = '\0';
		if (onkanji1(dupl, len - 1)) dupl[len - 1] = ' ';
		kanjiputs(dupl);
	}
	return(len);
}
