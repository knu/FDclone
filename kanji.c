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
static VOID sjis2jis __P_((char *, u_char *));
static VOID jis2sjis __P_((char *, u_char *));
static int tojis7 __P_((char *, u_char *));
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
	if (!ptr) return(iskanji1(s, 0));

	for (i = j = 0; i < ptr; i++, j++) {
		if (!s[j]) return(0);
#ifdef	CODEEUC
		if (isekana(s, j)) j++;
		else
#endif
		if (iskanji1(s, j)) {
			i++;
			j++;
		}
	}
	if (i > ptr) return(0);
	return(iskanji1(s, j));
}

#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
/*ARGSUSED*/
int getlang(s, io)
char *s;
int io;
{
	int ret;

	if (!s) ret = NOCNV;
#if	!MSDOS && !defined (_NOKANJICONV)
	else if (strstr2(s, "SJIS") || strstr2(s, "sjis")) ret = SJIS;
	else if (strstr2(s, "EUC") || strstr2(s, "euc")
	|| strstr2(s, "UJIS") || strstr2(s, "ujis")) ret = EUC;
	else if (strstr2(s, "JIS") || strstr2(s, "jis")) ret = JIS7;
#endif
#ifndef	_NOENGMES
	else if (io == L_OUTPUT
	&& (strstr2(s, "ENG") || strstr2(s, "eng")
	|| !strcmp(s, "C"))) ret = ENG;
#endif
	else ret = NOCNV;

#if	!MSDOS && !defined (_NOKANJICONV)
	if (io == L_INPUT) {
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
static VOID sjis2jis(buf, s)
char *buf;
u_char *s;
{
	int s1, s2, j1, j2;

	s1 = s[0] & 0xff;
	s2 = s[1] & 0xff;
	if (s1 >= 0xf0) {
		u_short w;
		int i;

		w = ((u_short)s1 << 8) | (u_char)s2;
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
}

static VOID jis2sjis(buf, s)
char *buf;
u_char *s;
{
	u_short w;
	int s1, s2, j1, j2;

	j1 = s[0] & 0x7f;
	j2 = s[1] & 0x7f;

	s1 = ((j1 - 1) >> 1) + ((j1 < 0x5f) ? 0x71 : 0xb1);
	s2 = j2 + ((j1 & 1) ? ((j2 < 0x60) ? 0x1f : 0x20) : 0x7e);
	w = (s1 << 8) | s2;
	buf[0] = s1;
	buf[1] = s2;
}

int sjis2ujis(buf, s)
char *buf;
u_char *s;
{
	int i, j;

	for (i = j = 0; s[i] && j < MAXLINESTR - 1; i++, j++) {
		if (isskana(s, i)) {
			buf[j++] = 0x8e;
			buf[j] = s[i];
		}
		else if (issjis1(s[i]) && issjis2(s[i + 1])) {
			sjis2jis(&(buf[j]), &(s[i++]));
			buf[j++] |= 0x80;
			buf[j] |= 0x80;
		}
		else buf[j] = s[i];
	}
	return(j);
}

int ujis2sjis(buf, s)
char *buf;
u_char *s;
{
	int i, j;

	for (i = j = 0; s[i] && j < MAXLINESTR - 1; i++, j++) {
		if (isekana(s, i)) buf[j] = s[++i];
		else if (iseuc(s[i])) jis2sjis(&(buf[j++]), &(s[i++]));
		else buf[j] = s[i];
	}
	return(j);
}
#endif	/* !MSDOS && (!_NOKANJICONV || (!_NODOSDRIVE && CODEEUC)) */

#if	!MSDOS && !defined (_NOKANJICONV)
static int tojis7(buf, s)
char *buf;
u_char *s;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < MAXLINESTR - 7 - 1; i++, j++) {
#ifdef	CODEEUC
		if (isekana(s, i)) {
			i++;
#else
		if (isskana(s, i)) {
#endif
			if (!(mode & KANA)) buf[j++] = '\016';
			mode |= KANA;
			buf[j] = s[i] & ~0x80;
			continue;
		}
		if (mode & KANA) buf[j++] = '\017';
		mode &= ~KANA;
		if (iskanji1(s, i)) {
			if (!(mode & KANJI)) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = 'B';
			}
			mode |= KANJI;
#ifdef	CODEEUC
			buf[j++] = s[i++] & ~0x80;
			buf[j] = s[i] & ~0x80;
#else
			sjis2jis(&(buf[j++]), &(s[i++]));
#endif
		}
		else {
			if (mode & KANJI) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = 'B';
			}
			mode &= ~KANJI;
			buf[j] = s[i];
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

int kanjiconv(buf, s, in, out)
char *buf, *s;
int in, out;
{
	int len;

	len = strlen(s);
	switch (out) {
		case JIS7:
			len = tojis7(buf, (u_char *)s);
			break;
		case SJIS:
			if (in == EUC) len = ujis2sjis(buf, (u_char *)s);
			else memcpy(buf, s, len);
			break;
		case EUC:
			if (in == SJIS) len = sjis2ujis(buf, (u_char *)s);
			else memcpy(buf, s, len);
			break;
		default:
			memcpy(buf, s, len);
			break;
	}
	return(len);
}
#endif	/* !MSDOS && !_NOKANJICONV */

int kanjiputs(s)
char *s;
{
#if	MSDOS || defined (_NOKANJICONV)
	cputs2(s);
	return(strlen(s));
#else
	char buf[MAXLINESTR + 1];
	int i;

	i = kanjiconv(buf, s, DEFCODE, outputkcode);
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
