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

#define	ASCII	000
#define	KANA	001
#define	KANJI	002

#define	SJ_UDEF	0x81ac	/* GETA */

typedef struct _kconv_t {
	u_short start;
	u_short end;
	u_short cnv;
} kconv_t;

#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
static u_short sjis2jis __P_((char *, u_char *));
static u_short jis2sjis __P_((char *, u_char *));
#endif

#if	!MSDOS && !defined (_NOKANJICONV)
int inputkcode = 0;
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
int outputkcode = 0;
#endif

#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
static kconv_t convtable[] = {
	{0xfa40, 0xfa49, 0xeeef},
	{0xfa4a, 0xfa53, 0x8754},
	{0xfa54, 0xfa54, 0x81ca},
	{0xfa55, 0xfa57, 0xeefa},
	{0xfa58, 0xfa58, 0x878a},
	{0xfa59, 0xfa59, 0x8782},
	{0xfa5a, 0xfa5a, 0x8784},
	{0xfa5b, 0xfa5b, 0x81e6},
	{0xfa5c, 0xfa7e, 0xed40},
	{0xfa80, 0xfa9b, 0xed63},
	{0xfa9c, 0xfafc, 0xed80},
	{0xfb40, 0xfb5b, 0xede1},
	{0xfb5c, 0xfb7e, 0xee40},
	{0xfb80, 0xfb9b, 0xee63},
	{0xfb9c, 0xfbfc, 0xee80},
	{0xfc40, 0xfc4b, 0xeee1}
};
#define	CNVTBLSIZ	(sizeof(convtable) / sizeof(kconv_t))
#endif	/* !MSDOS && (!_NOKANJICONV || (!_NODOSDRIVE && CODEEUC)) */


int onkanji1(s, ptr)
char *s;
int ptr;
{
	int i, j;

	if (ptr < 0) return(0);
	if (!ptr) return(iskanji1(s[0]));

	for (i = j = 0; i < ptr; i++, j++) {
		if (!s[j]) return(0);
#ifdef	CODEEUC
		if (isekana(s, j)) j++;
		else
#endif
		if (iskanji1(s[j])) {
			i++;
			j++;
		}
	}
	if (i > ptr) return(0);
	return(iskanji1(s[j]));
}

#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
/*ARGSUSED*/
int getlang(str, in)
char *str;
int in;
{
	int ret;

	if (!str) ret = NOCNV;
#if	!MSDOS && !defined (_NOKANJICONV)
	else if (strstr2(str, "SJIS") || strstr2(str, "sjis")) ret = SJIS;
	else if (strstr2(str, "EUC") || strstr2(str, "euc")
	|| strstr2(str, "UJIS") || strstr2(str, "ujis")) ret = EUC;
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

#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
static u_short sjis2jis(buf, str)
char *buf;
u_char *str;
{
	u_short w;
	int i, s1, s2, j1, j2;

	s1 = str[0] & 0xff;
	s2 = str[1] & 0xff;
	w = ((u_short)s1 << 8) | (u_char)s2;
	if (w >= 0xf000) {
		for (i = 0; i < CNVTBLSIZ; i++)
			if (w >= convtable[i].start && w <= convtable[i].end)
				break;
		if (i >= CNVTBLSIZ) w = SJ_UDEF;
		else {
			w -= convtable[i].start;
			w += convtable[i].cnv;
		}
		s1 = (w >> 8) & 0xff;
		s2 = w & 0xff;
	}
	j1 = s1 * 2 - ((s1 <= 0x9f) ? 0xe1 : 0x161);
	if (s2 < 0x9f) j2 = s2 - ((s2 > 0x7f) ? 0x20 : 0x1f);
	else {
		j1++;
		j2 = s2 - 0x7e;
	}
	buf[0] = j1;
	buf[1] = j2;
	return(w);
}

static u_short jis2sjis(buf, str)
char *buf;
u_char *str;
{
	u_short w;
	int s1, s2, j1, j2;

	j1 = str[0] & 0x7f;
	j2 = str[1] & 0x7f;

	s1 = ((j1 - 1) >> 1) + ((j1 < 0x5f) ? 0x71 : 0xb1);
	if (j1 & 1) s2 = j2 + ((j2 < 0x60) ? 0x1f : 0x20);
	else s2 = j2 + 0x7e;
	w = (s1 << 8) | s2;
	buf[0] = s1;
	buf[1] = s2;
	return(w);
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
			sjis2jis(&(buf[j]), &(str[i++]));
			buf[j++] |= 0x80;
			buf[j] |= 0x80;
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
		if (isekana(str, i)) buf[j] = str[++i];
		else if (iseuc(str[i])) jis2sjis(&(buf[j++]), &(str[i++]));
		else buf[j] = str[i];
	}
	return(j);
}
#endif	/* !MSDOS && (!_NOKANJICONV || (!_NODOSDRIVE && CODEEUC)) */

#if	!MSDOS && !defined (_NOKANJICONV)
int jis7(buf, str, incode)
char *buf;
u_char *str;
int incode;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; str[i] && j < MAXLINESTR - 7 - 1; i++, j++) {
		if ((incode == EUC) ? isekana(str, i) : isskana(str, i)) {
			if (incode == EUC) i++;
			if (!(mode & KANA)) buf[j++] = '\016';
			mode |= KANA;
			buf[j] = str[i] & ~0x80;
			continue;
		}
		if (mode & KANA) buf[j++] = '\017';
		mode &= ~KANA;
		if ((incode == EUC) ? iseuc(str[i]) : issjis1(str[i])) {
			if (!(mode & KANJI)) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = 'B';
			}
			mode |= KANJI;
			if (incode != EUC) sjis2jis(&(buf[j++]), &(str[i++]));
			else {
				buf[j++] = str[i++] & ~0x80;
				buf[j] = str[i] & ~0x80;
			}
		}
		else {
			if (mode & KANJI) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = 'B';
			}
			mode &= ~KANJI;
			buf[j] = str[i];
		}
	}
	if (mode & KANA) buf[j++] = '\017';
	if (mode & KANJI) {
		buf[j++] = '\033';
		buf[j++] = '(';
		buf[j++] = 'B';
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

	strncpy3(dupl, s, &len, ptr);
	kanjiputs(dupl);
	return(len);
}
