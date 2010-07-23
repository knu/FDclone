/*
 *	roman.c
 *
 *	Roman translation
 */

#include "headers.h"
#include "depend.h"
#include "kctype.h"
#include "typesize.h"
#include "string.h"
#include "kconv.h"
#include "roman.h"

#define	R_MAXVOWEL		5
#define	DEFRM(s,c)		{s, strsize(s), \
				{((c) >> 8) & 0xff, (c) & 0xff}}
#define	DEFRM2(s,c1,c2)		{s, strsize(s), {(c1), (c2)}}

typedef struct _voweltable {
	int key;
	CONST u_char code[R_MAXVOWEL];
} voweltable;

typedef struct _catromantable {
	CONST char str[R_MAXROMAN + 1];
	int key;
	CONST u_short code;
} catromantable;

static int cmproman __P_((CONST VOID_P, CONST VOID_P));
static int NEAR chgroman __P_((CONST char *, int, CONST u_short *));
static int NEAR catroman __P_((int, int, CONST char *));
static u_int NEAR jis2defcode __P_((u_int));
static u_int NEAR defcode2jis __P_((CONST char *));

romantable *romanlist = NULL;
int maxromanlist = 0;

static CONST voweltable vowellist[] = {
	{'\0', {'a', 'i', 'u', 'e', 'o'}},
	{'y', {0x63, 0x23, 0x65, 0x27, 0x67}},
	{'h', {0x63, 0x00, 0x65, 0x27, 0x67}},
	{'l', {0x21, 0x23, 0x00, 0x27, 0x29}},
	{'w', {0x21, 0x23, 0x25, 0x27, 0x29}},
};
#define	VOWELLISTSIZ		arraysize(vowellist)
static CONST romantable origromanlist[] = {
	DEFRM("a", 0x2200),
	DEFRM("i", 0x2400),
	DEFRM("u", 0x2600),
	DEFRM("e", 0x2800),
	DEFRM("o", 0x2a00),

	DEFRM("ka", 0x2b00),	DEFRM("ga", 0x2c00),
	DEFRM("ki", 0x2d00),	DEFRM("gi", 0x2e00),
	DEFRM("ku", 0x2f00),	DEFRM("gu", 0x3000),
	DEFRM("ke", 0x3100),	DEFRM("ge", 0x3200),
	DEFRM("ko", 0x3300),	DEFRM("go", 0x3400),

	DEFRM("sa", 0x3500),	DEFRM("za", 0x3600),
	DEFRM("si", 0x3700),	DEFRM("zi", 0x3800),
	DEFRM("su", 0x3900),	DEFRM("zu", 0x3a00),
	DEFRM("se", 0x3b00),	DEFRM("ze", 0x3c00),
	DEFRM("so", 0x3d00),	DEFRM("zo", 0x3e00),

	DEFRM("ta", 0x3f00),	DEFRM("da", 0x4000),
	DEFRM("ti", 0x4100),	DEFRM("di", 0x4200),
	DEFRM("tu", 0x4400),	DEFRM("du", 0x4500),
	DEFRM("te", 0x4600),	DEFRM("de", 0x4700),
	DEFRM("to", 0x4800),	DEFRM("do", 0x4900),

	DEFRM("na", 0x4a00),
	DEFRM("ni", 0x4b00),
	DEFRM("nu", 0x4c00),
	DEFRM("ne", 0x4d00),
	DEFRM("no", 0x4e00),

	DEFRM("ha", 0x4f00),	DEFRM("ba", 0x5000),	DEFRM("pa", 0x5100),
	DEFRM("hi", 0x5200),	DEFRM("bi", 0x5300),	DEFRM("pi", 0x5400),
	DEFRM("hu", 0x5500),	DEFRM("bu", 0x5600),	DEFRM("pu", 0x5700),
	DEFRM("he", 0x5800),	DEFRM("be", 0x5900),	DEFRM("pe", 0x5a00),
	DEFRM("ho", 0x5b00),	DEFRM("bo", 0x5c00),	DEFRM("po", 0x5d00),

	DEFRM("ma", 0x5e00),
	DEFRM("mi", 0x5f00),
	DEFRM("mu", 0x6000),
	DEFRM("me", 0x6100),
	DEFRM("mo", 0x6200),

	DEFRM("ya", 0x6400),
	DEFRM("yi", 0x2400),
	DEFRM("yu", 0x6600),
	DEFRM("ye", 0x2427),
	DEFRM("yo", 0x6800),

	DEFRM("ra", 0x6900),
	DEFRM("ri", 0x6a00),
	DEFRM("ru", 0x6b00),
	DEFRM("re", 0x6c00),
	DEFRM("ro", 0x6d00),

	DEFRM("wa", 0x6f00),	DEFRM("wya", 0x6f63),
	DEFRM("wi", 0x2623),	DEFRM("wyi", 0x7000),
	DEFRM("wu", 0x2600),	DEFRM("wyu", 0x6f65),
	DEFRM("we", 0x2627),	DEFRM("wye", 0x7100),
	DEFRM("wo", 0x7200),	DEFRM("wyo", 0x6f67),

	DEFRM("nn", 0x7300),	DEFRM("xn", 0x7300),
	DEFRM("n'", 0x7300),
	DEFRM("xtu", 0x4300),	DEFRM("ltu", 0x4300),
	DEFRM("xtsu", 0x4300),	DEFRM("ltsu", 0x4300),
	DEFRM("xwa", 0x6e00),	DEFRM("lwa", 0x6e00),

	DEFRM2("xka", 0x2575, 0x00),	DEFRM2("lka", 0x2575, 0x00),
	DEFRM2("xke", 0x2576, 0x00),	DEFRM2("lke", 0x2576, 0x00),

	DEFRM("ca", 0x2b00),
	DEFRM("ci", 0x3700),
	DEFRM("cu", 0x2f00),
	DEFRM("ce", 0x3b00),
	DEFRM("co", 0x3300),
};
#define	ROMANLISTSIZ		arraysize(origromanlist)
static CONST catromantable catromanlist[] = {
	{"ky",	'y', 0x242d},	{"gy",	'y', 0x242e},
	{"sy",	'y', 0x2437},	{"zy",	'y', 0x2438},
	{"ty",	'y', 0x2441},	{"dy",	'y', 0x2442},
	{"ny",	'y', 0x244b},
	{"hy",	'y', 0x2452},	{"by",	'y', 0x2453},	{"py",	'y', 0x2454},
	{"my",	'y', 0x245f},
	{"xy",	'y', 0x0000},	{"ly",	'y', 0x0000},
	{"ry",	'y', 0x246a},
	{"sh",	'h', 0x2437},	{"j",	'h', 0x2438},
	{"jy",	'y', 0x2438},
	{"ch",	'h', 0x2441},
	{"cy",	'y', 0x2441},
	{"ts",	'l', 0x2444},
	{"th",	'y', 0x2446},	{"dh",	'y', 0x2447},
	{"wh",	'l', 0x2426},
	{"f",	'l', 0x2455},	{"v",	'l', 0x2574},
	{"fy",	'y', 0x2455},	{"vy",	'y', 0x2574},
	{"q",	'l', 0x242f},
	{"qy",	'y', 0x242f},
	{"x",	'w', 0x0000},	{"l",	'w', 0x0000},
	{"kw",	'w', 0x242f},	{"gw",	'w', 0x2430},
	{"qw",	'w', 0x242f},
	{"sw",	'w', 0x2439},	{"zw",	'w', 0x243a},
	{"tw",	'w', 0x2448},	{"dw",	'w', 0x2449},
	{"hw",	'w', 0x2455},	{"bw",	'w', 0x2456},	{"pw",	'w', 0x2457},
	{"fw",	'w', 0x2455},	{"vw",	'w', 0x2574},
};
#define	CATROMANLISTSIZ		arraysize(catromanlist)


int code2kanji(buf, c)
char *buf;
u_int c;
{
	int n;

	n = 0;
	if ((buf[n] = ((c >> 8) & 0xff))) n++;
	buf[n++] = (c & 0xff);
	buf[n] = '\0';

	return(n);
}

static int cmproman(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	romantable *rp1, *rp2;

	rp1 = (romantable *)vp1;
	rp2 = (romantable *)vp2;

	return(strcmp(rp1 -> str, rp2 -> str));
}

int searchroman(s, len)
CONST char *s;
int len;
{
	int i, n, min, max;

	min = -1;
	max = maxromanlist;
	for (;;) {
		n = (min + max) / 2;
		if (n <= min || n >= max) return(-1);
		if ((i = strncmp(s, romanlist[n].str, len)) > 0) min = n;
		else if (i < 0 || len < romanlist[n].len) max = n;
		else break;
	}

	return(n);
}

static int NEAR chgroman(str, len, kbuf)
CONST char *str;
int len;
CONST u_short *kbuf;
{
	romantable *tmp;
	int n;

	if ((n = searchroman(str, len)) < 0) {
		n = maxromanlist;
		tmp = (romantable *)realloc(romanlist,
			(maxromanlist + 1) * sizeof(romantable));
		if (!tmp) return(-1);
		romanlist = tmp;
		maxromanlist++;
	}
	else if (!kbuf) {
		memmove((char *)(&romanlist[n]), (char *)(&romanlist[n + 1]),
			(--maxromanlist - n) * sizeof(romantable));
		return(0);
	}

	Xstrcpy(romanlist[n].str, str);
	romanlist[n].len = len;
	memcpy((char *)romanlist[n].code, (char *)kbuf,
		R_MAXKANA * sizeof(u_short));

	return(0);
}

static int NEAR catroman(key, top, s)
int key, top;
CONST char *s;
{
	u_short kbuf[R_MAXKANA];
	char str[R_MAXROMAN + 1];
	int i, j, n, len;

	for (n = 1; n < VOWELLISTSIZ; n++) if (key == vowellist[n].key) break;
	if (n >= VOWELLISTSIZ) return(-1);

	for (i = 0; s[i]; i++) {
		if (i >= strsize(str) - 1) break;
		str[i] = s[i];
	}
	len = i;
	str[len + 1] = '\0';

	for (i = 0; i < R_MAXVOWEL; i++) {
		str[len] = vowellist[0].code[i];
		memset((char *)kbuf, 0, sizeof(kbuf));
		j = 0;
		if (top) kbuf[j++] = top;
		kbuf[j] = vowellist[n].code[i];
		if (kbuf[j]) kbuf[j] |= 0x2400;

		if (chgroman(str, len + 1, kbuf) < 0) return(-1);
	}

	return(0);
}

VOID initroman(VOID_A)
{
	int i, n;

	if (romanlist) return;
	if (!(romanlist = (romantable *)malloc(sizeof(origromanlist)))) return;

	memcpy((char *)romanlist, (char *)origromanlist,
		sizeof(origromanlist));
	for (n = 0; n < ROMANLISTSIZ; n++) {
		for (i = 0; i < R_MAXKANA; i++) {
			if (!romanlist[n].code[i]) continue;
			if (romanlist[n].code[i] & ~0xff) continue;
			romanlist[n].code[i] |= 0x2400;
		}
	}

	maxromanlist = ROMANLISTSIZ;
	for (i = 0; i < CATROMANLISTSIZ; i++) {
		n = catroman(catromanlist[i].key,
			catromanlist[i].code, catromanlist[i].str);
		if (n < 0) break;
	}

	qsort(romanlist, maxromanlist, sizeof(romantable), cmproman);
}

static u_int NEAR jis2defcode(c)
u_int c;
{
#ifndef	CODEEUC
	CONST char *cp;
	char buf[MAXKLEN + 1], tmp[MAXKLEN + 1];
#endif

	if (!(c & ~0xff)) {
#ifdef	CODEEUC
		if (Xiskana(c)) c |= (C_EKANA << 8);
#endif
		return(c);
	}

#ifdef	CODEEUC
	return(c | 0x8080);
#else
	VOID_C code2kanji(tmp, c | 0x8080);
	cp = kanjiconv2(buf, tmp, MAXKLEN, EUC, DEFCODE, L_INPUT);
	if (kanjierrno) return(0);

	return(((u_char)(cp[0]) << 8) | (u_char)(cp[1]));
#endif	/* !CODEEUC */
}

static u_int NEAR defcode2jis(buf)
CONST char *buf;
{
#ifndef	CODEEUC
	char tmp[MAXKLEN + 1];

	buf = kanjiconv2(tmp, buf, MAXKLEN, DEFCODE, EUC, L_INPUT);
	if (kanjierrno) return((u_int)0);
#endif
	return((((u_char)(buf[0]) << 8) | (u_char)(buf[1])) & ~0x8080);
}

int jis2str(buf, c)
char *buf;
u_int c;
{
	int n;

	if ((c & 0x8080) != 0x8080) n = 0;
	else {
		n = code2kanji(buf, jis2defcode((c >> 8) & 0xff));
		c &= 0xff;
	}
	n += code2kanji(&(buf[n]), jis2defcode(c));

	return(n);
}

int str2jis(kbuf, max, buf)
u_short *kbuf;
int max;
CONST char *buf;
{
	u_short w;
	int i, j;

	for (i = j = 0; buf[i] && j < max; i++) {
		if (iskanji1(buf, i)) w = defcode2jis(&(buf[i++]));
#ifdef	CODEEUC
		else if (isekana(buf, i)) w = buf[++i];
#endif
		else w = buf[i];
		kbuf[j++] = w;
	}

	return(j);
}

int addroman(s, buf)
CONST char *s, *buf;
{
#ifdef	DEP_FILECONV
	char tmp[R_MAXKANA * 3 + 1];
#endif
	char str[R_MAXROMAN + 1];
	u_short kbuf[R_MAXKANA];
	int i, j, len;

	initroman();

	if (!s) return(-1);
	for (i = j = 0; s[i]; i++) {
		if (!Xisprint(s[i])) continue;
		if (ismsb(s[i])) return(-1);
		if (j >= strsize(str)) break;
		str[j++] = Xtolower(s[i]);
	}
	if (!j) return(-1);
	str[j] = '\0';
	len = j;

	if (!buf) chgroman(str, len, NULL);
	else {
#ifdef	DEP_FILECONV
		buf = kanjiconv2(tmp, buf,
			strsize(tmp), defaultkcode, DEFCODE, L_FNAME);
#endif
		memset((char *)kbuf, 0, sizeof(kbuf));
		if (!str2jis(kbuf, arraysize(kbuf), buf)) return(-1);
		chgroman(str, len, kbuf);
	}
	qsort(romanlist, maxromanlist, sizeof(romantable), cmproman);

	return(0);
}

VOID freeroman(n)
int n;
{
	romantable *tmp;

	maxromanlist = 0;
	if (n) {
		tmp = (romantable *)realloc(romanlist, n * sizeof(romantable));
		if (tmp) romanlist = tmp;
	}
	else if (romanlist) {
		tmp = romanlist;
		romanlist = NULL;
		free(tmp);
	}
}
