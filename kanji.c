/*
 *	kanji.c
 *
 *	Kanji Convert Function
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "machine.h"

#ifndef NOUNISTDH
#include <unistd.h>
#endif

#ifndef NOSTDLIBH
#include <stdlib.h>
#endif

#include "term.h"
#include "kctype.h"

#ifdef	USESTDARGH
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define	ASCII		000
#define	KANA		001
#define	KANJI		002
#define	MAXPRINTBUF	1023
#define	SJ_UDEF		0x81ac	/* GETA */

typedef struct _kconv_t {
	u_short start;
	u_short end;
	u_short cnv;
} kconv_t;

extern char *malloc2 __P_((ALLOC_T));
extern char *strncpy3 __P_((char *, char *, int *, int));
extern char *strstr2 __P_((char *, char *));

#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
static VOID NEAR sjis2jis __P_((char *, u_char *));
static VOID NEAR jis2sjis __P_((char *, u_char *));
static int NEAR tojis7 __P_((char *, u_char *, int));
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
static char *NEAR _kanjiconv __P_((char *, char *, int, int, int, int *));
#endif	/* !MSDOS && !_NOKANJICONV */

#if	!MSDOS && !defined (_NOKANJICONV)
int inputkcode = 0;
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
int outputkcode = 0;
#endif
#ifndef	LSI_C
u_char kctypetable[256] = {
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,	/* 0x00 */
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,	/* 0x10 */
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
	   0, 0060, 0060, 0060, 0060, 0060, 0060, 0060,	/* 0x20 */
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,	/* 0x30 */
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,	/* 0x40 */
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,	/* 0x50 */
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,	/* 0x60 */
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,	/* 0x70 */
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0200,
	0002, 0003, 0003, 0003, 0003, 0003, 0003, 0003,	/* 0x80 */
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,	/* 0x90 */
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,
	0002, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xa0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xb0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xc0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xd0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,	/* 0xe0 */
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,	/* 0xf0 */
	0013, 0013, 0013, 0013, 0013, 0010, 0010,    0
};
#endif	/* !LSI_C */

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
static u_char sj2jtable1[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x20 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x30 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x40 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x50 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x60 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x70 */
	   0, 0x21, 0x23, 0x25, 0x27, 0x29, 0x2b, 0x2d,	/* 0x80 */
	0x2f, 0x31, 0x33, 0x35, 0x37, 0x39, 0x3b, 0x3d,
	0x3f, 0x41, 0x43, 0x45, 0x47, 0x49, 0x4b, 0x4d,	/* 0x90 */
	0x4f, 0x51, 0x53, 0x55, 0x57, 0x59, 0x5b, 0x5d,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 */
	0x5f, 0x61, 0x63, 0x65, 0x67, 0x69, 0x6b, 0x6d,	/* 0xe0 */
	0x6f, 0x71, 0x73, 0x75, 0x77, 0x79, 0x7b, 0x7d,
	0x7f, 0x81, 0x83, 0x85, 0x87, 0x89, 0x8b, 0x8d,	/* 0xf0 */
	0x8f, 0x91, 0x93, 0x95, 0x97,    0,    0,    0
};
static u_char sj2jtable2[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x20 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x30 */
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,	/* 0x40 */
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,	/* 0x50 */
	0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,	/* 0x60 */
	0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,	/* 0x70 */
	0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x80 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x90 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x21,
	0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,	/* 0xa0 */
	0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
	0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,	/* 0xb0 */
	0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41,
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,	/* 0xc0 */
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,	/* 0xd0 */
	0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61,
	0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,	/* 0xe0 */
	0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
	0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,	/* 0xf0 */
	0x7a, 0x7b, 0x7c, 0x7d, 0x7e,    0,    0,    0
};
static u_char j2sjtable1[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	   0, 0x81, 0x81, 0x82, 0x82, 0x83, 0x83, 0x84,	/* 0x20 */
	0x84, 0x85, 0x85, 0x86, 0x86, 0x87, 0x87, 0x88,
	0x88, 0x89, 0x89, 0x8a, 0x8a, 0x8b, 0x8b, 0x8c,	/* 0x30 */
	0x8c, 0x8d, 0x8d, 0x8e, 0x8e, 0x8f, 0x8f, 0x90,
	0x90, 0x91, 0x91, 0x92, 0x92, 0x93, 0x93, 0x94,	/* 0x40 */
	0x94, 0x95, 0x95, 0x96, 0x96, 0x97, 0x97, 0x98,
	0x98, 0x99, 0x99, 0x9a, 0x9a, 0x9b, 0x9b, 0x9c,	/* 0x50 */
	0x9c, 0x9d, 0x9d, 0x9e, 0x9e, 0x9f, 0x9f, 0xe0,
	0xe0, 0xe1, 0xe1, 0xe2, 0xe2, 0xe3, 0xe3, 0xe4,	/* 0x60 */
	0xe4, 0xe5, 0xe5, 0xe6, 0xe6, 0xe7, 0xe7, 0xe8,
	0xe8, 0xe9, 0xe9, 0xea, 0xea, 0xeb, 0xeb, 0xec,	/* 0x70 */
	0xec, 0xed, 0xed, 0xee, 0xee, 0xef, 0xef,    0
};
static u_char j2sjtable2[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	   0, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,	/* 0x20 */
	0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
	0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,	/* 0x30 */
	0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
	0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,	/* 0x40 */
	0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
	0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,	/* 0x50 */
	0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,	/* 0x60 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,	/* 0x70 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,    0
};
#endif	/* !MSDOS && (!_NOKANJICONV || (!_NODOSDRIVE && CODEEUC)) */

int onkanji1 __P_((char *, int));
#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
int getlang __P_((char *, int));
#endif
#if	!defined (_NOENGMES) && !defined (_NOJPNMES)
char *mesconv __P_((char *, char *));
#endif
#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
int sjis2ujis __P_((char *, u_char *, int));
int ujis2sjis __P_((char *, u_char *, int));
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
int kanjiconv __P_((char *, char *, int, int, int));
#endif
int kanjiputs __P_((char *));
int kanjiprintf __P_((CONST char *, ...));
int kanjiputs2 __P_((char *, int, int));


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

#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
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
#endif	/* (!MSDOS && !_NOKANJICONV) || (!_NOENGMES && !_NOJPNMES) */

#if	!defined (_NOENGMES) && !defined (_NOJPNMES)
char *mesconv(jpn, eng)
char *jpn, *eng;
{
	return((outputkcode == ENG) ? eng : jpn);
}
#endif

#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
static VOID NEAR sjis2jis(buf, s)
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
	j1 = sj2jtable1[s1];
	j2 = sj2jtable2[s2];
	if (s2 >= 0x9f) j1++;
	buf[0] = j1;
	buf[1] = j2;
}

static VOID NEAR jis2sjis(buf, s)
char *buf;
u_char *s;
{
	u_short w;
	int s1, s2, j1, j2;

	j1 = s[0] & 0x7f;
	j2 = s[1] & 0x7f;

	s1 = j2sjtable1[j1];
	s2 = (j1 & 1) ? j2sjtable2[j2] : (j2 + 0x7e);
	w = (s1 << 8) | s2;
	buf[0] = s1;
	buf[1] = s2;
}

int sjis2ujis(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max - 1; i++, j++) {
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

int ujis2sjis(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max - 1; i++, j++) {
		if (isekana(s, i)) buf[j] = s[++i];
		else if (iseuc(s[i])) jis2sjis(&(buf[j++]), &(s[i++]));
		else buf[j] = s[i];
	}
	return(j);
}
#endif	/* !MSDOS && (!_NOKANJICONV || (!_NODOSDRIVE && CODEEUC)) */

#if	!MSDOS && !defined (_NOKANJICONV)
static int NEAR tojis7(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 7; i++, j++) {
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

static char *NEAR _kanjiconv(buf, s, max, in, out, lenp)
char *buf, *s;
int max, in, out, *lenp;
{
	if (in == out || in == NOCNV || out == NOCNV) return(s);
	switch (out) {
		case JIS7:
			*lenp = tojis7(buf, (u_char *)s, max);
			break;
		case SJIS:
			*lenp = ujis2sjis(buf, (u_char *)s, max);
			break;
		case EUC:
			*lenp = sjis2ujis(buf, (u_char *)s, max);
			break;
		default:
			return(s);
/*NOTREACHED*/
			break;
	}
	return(buf);
}

int kanjiconv(buf, s, max, in, out)
char *buf, *s;
int max, in, out;
{
	int len;

	if (_kanjiconv(buf, s, max, in, out, &len) != buf)
		for (len = 0; s[len]; len++) buf[len] = s[len];
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
	char *buf;
	int len, max;

	len = strlen(s);
	max = len * 2 + 3;
	buf = malloc2(max + 1);
	if (_kanjiconv(buf, s, max, DEFCODE, outputkcode, &len) == buf) {
		buf[len] = '\0';
		s = buf;
	}
	cputs2(s);
	free(buf);
	return(len);
#endif
}

#ifdef	USESTDARGH
/*VARARGS1*/
int kanjiprintf(CONST char *fmt, ...)
{
	va_list args;
	char buf[MAXPRINTBUF + 1];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
#else	/* !USESTDARGH */
# ifndef	NOVSPRINTF
/*VARARGS1*/
int kanjiprintf(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list args;
	char buf[MAXPRINTBUF + 1];

	va_start(args);
	vsprintf(buf, fmt, args);
	va_end(args);
# else	/* NOVSPRINTF */
int kanjiprintf(fmt, arg1, arg2, arg3, arg4, arg5, arg6)
char *fmt;
{
	char buf[MAXPRINTBUF + 1];

	sprintf(buf, fmt, arg1, arg2, arg3, arg4, arg5, arg6);
# endif	/* NOVSPRINTF */
#endif	/* !USESTDARGH */
	return(kanjiputs(buf));
}

int kanjiputs2(s, len, ptr)
char *s;
int len, ptr;
{
	char *dupl;

	if (len >= 0) dupl = malloc2(len + 1);
	else dupl = malloc2(strlen(&(s[ptr])) + 1);
	strncpy3(dupl, s, &len, ptr);
	kanjiputs(dupl);
	free(dupl);
	return(len);
}
