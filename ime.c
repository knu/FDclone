/*
 *	ime.c
 *
 *	tiny Input Method Editor for terminal emulation
 */

#include "fd.h"
#include "wait.h"
#include "parse.h"
#include "kconv.h"
#include "roman.h"
#include "func.h"
#include "kanji.h"

#ifdef	DEP_PTY
#include "termemu.h"
#endif

#define	KN_HIRA		0
#define	KN_KATA		1
#define	KN_HAN		2
#define	KN_DIRECT	3
#define	KN_MAX		4
#define	JIS_WIDTH	4
#define	JIS_DIGIT	4
#define	KNJ_COLUMN	16
#define	isvalidbuf(jp)	((jp).buf && (jp).max > 0)
#define	jis_xpos(p, n)	((p) + 8 + 1 + (n) * (JIS_WIDTH + 1))

typedef struct _jisbuf {
	u_short *buf;
	int size;
	int max;
} jisbuf;

#ifdef	DEP_IME

extern int subwindow;
extern int win_x;
extern int win_y;
extern int imekey;
extern romantable *romanlist;
extern int maxromanlist;

static int NEAR inkanjiconv __P_((char *, CONST char *));
static u_int NEAR getdefcode __P_((u_int, int, int));
static VOID NEAR imeputch __P_((int, int));
static int NEAR imeputs __P_((CONST char *));
static int NEAR imeprintf __P_((CONST char *, ...));
static int NEAR jisputs __P_((u_int));
static int NEAR putjisbuf __P_((jisbuf *));
static int NEAR countjisbuf __P_((jisbuf *));
static VOID NEAR addjisbuf __P_((jisbuf *, u_int));
static VOID NEAR copyjisbuf __P_((jisbuf *, CONST u_short *, int));
static u_int NEAR zen2han __P_((u_int));
static u_int NEAR kanabias __P_((u_int));
static int NEAR romanprompt __P_((int, int *));
static int NEAR fixroman __P_((int, int, int));
static int NEAR findroman __P_((int, int, int *, int *));
static int NEAR unlimitroman __P_((int, int *, int *, int *));
static u_int NEAR getjisindex __P_((jisbuf *));
static u_int NEAR getjiscode __P_((int *, jisbuf *));
static VOID NEAR dispjiscode __P_((u_int, int));
static VOID NEAR imeputcursor __P_((int, int, int));
static int NEAR listkanji __P_((int, long, u_short **,
		long *, long *, long *, int []));
static int NEAR selectkanji __P_((long *, u_short **, int));
static int NEAR listjis __P_((int, u_int, int *, int));
static int NEAR selectjis __P_((u_int, int, int));
static int NEAR searchkanji __P_((long *, u_short ***,
		int, int, int, int *, int));
static int NEAR fixkanji __P_((int, int, int, int));
static int NEAR inputkanji __P_((int));
static int NEAR getkanjibuf __P_((char *, u_int));

int ime_cont = 0;
int ime_line = 0;
int *ime_xposp = NULL;
VOID (*ime_locate)__P_((int, int)) = NULL;

static int imewin_x = 0;
static int imewin_y = 0;
static int kanamode = KN_HIRA;
static jisbuf rjisbuf = {NULL, 0, 0};
static jisbuf kjisbuf = {NULL, 0, 0};
static CONST u_short jislist[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	0x2121, 0x212a, 0x2149, 0x2174, 0x2170, 0x2173, 0x2175, 0x2147,
	0x214a, 0x214b, 0x2176, 0x215c, 0x2122, 0x213c, 0x2123, 0x213f,
	0x2330, 0x2331, 0x2332, 0x2333, 0x2334, 0x2335, 0x2336, 0x2337,
	0x2338, 0x2339, 0x2127, 0x2128, 0x2163, 0x2161, 0x2164, 0x2129,
	0x2177, 0x2341, 0x2342, 0x2343, 0x2344, 0x2345, 0x2346, 0x2347,
	0x2348, 0x2349, 0x234a, 0x234b, 0x234c, 0x234d, 0x234e, 0x234f,
	0x2350, 0x2351, 0x2352, 0x2353, 0x2354, 0x2355, 0x2356, 0x2357,
	0x2358, 0x2359, 0x235a, 0x2156, 0x2140, 0x2157, 0x2130, 0x2132,
	0x2146, 0x2361, 0x2362, 0x2363, 0x2364, 0x2365, 0x2366, 0x2367,
	0x2368, 0x2369, 0x236a, 0x236b, 0x236c, 0x236d, 0x236e, 0x236f,
	0x2370, 0x2371, 0x2372, 0x2373, 0x2374, 0x2375, 0x2376, 0x2377,
	0x2378, 0x2379, 0x237a, 0x2150, 0x2143, 0x2151, 0x2141, 0x0000,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 */
	0x0000, 0x2123, 0x2156, 0x2157, 0x2122, 0x2126, 0x2572, 0x2521,
	0x2523, 0x2525, 0x2527, 0x2529, 0x2563, 0x2565, 0x2567, 0x2543,
	0x213c, 0x2522, 0x2524, 0x2526, 0x2528, 0x252a, 0x252b, 0x252d,
	0x252f, 0x2531, 0x2533, 0x2535, 0x2537, 0x2539, 0x253b, 0x253d,
	0x253f, 0x2541, 0x2544, 0x2546, 0x2548, 0x254a, 0x254b, 0x254c,
	0x254d, 0x254e, 0x254f, 0x2552, 0x2555, 0x2558, 0x255b, 0x255e,
	0x255f, 0x2560, 0x2561, 0x2562, 0x2564, 0x2566, 0x2568, 0x2569,
	0x256a, 0x256b, 0x256c, 0x256d, 0x256f, 0x2573, 0x212b, 0x212c,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xf0 */
};
static CONST u_short hanlist[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	0x0000, 0xa700, 0xb100, 0xa800, 0xb200, 0xa900, 0xb300, 0xaa00,
	0xb400, 0xab00, 0xb500, 0xb600, 0xb6de, 0xb700, 0xb7de, 0xb800,
	0xb8de, 0xb900, 0xb9de, 0xba00, 0xbade, 0xbb00, 0xbbde, 0xbc00,
	0xbcde, 0xbd00, 0xbdde, 0xbe00, 0xbede, 0xbf00, 0xbfde, 0xc000,
	0xc0de, 0xc100, 0xc1de, 0xaf00, 0xc200, 0xc2de, 0xc300, 0xc3de,
	0xc400, 0xc4de, 0xc500, 0xc600, 0xc700, 0xc800, 0xc900, 0xca00,
	0xcade, 0xcadf, 0xcb00, 0xcbde, 0xcbdf, 0xcc00, 0xccde, 0xccdf,
	0xcd00, 0xcdde, 0xcddf, 0xce00, 0xcede, 0xcedf, 0xcf00, 0xd000,
	0xd100, 0xd200, 0xd300, 0xac00, 0xd400, 0xad00, 0xd500, 0xae00,
	0xd600, 0xd700, 0xc800, 0xd900, 0xda00, 0xdb00, 0xdc00, 0xdc00,
	0xb200, 0xb400, 0xa600, 0xdd00, 0xb3de, 0xb600, 0xb900, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xf0 */
};
static CONST u_char hanlist2[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	0x00, 0x20, 0xa4, 0xa1, 0x2c, 0x2e, 0xa5, 0x3a,	/* 0x20 */
	0x3b, 0x3f, 0x21, 0xde, 0xdf, 0x00, 0x00, 0x00,
	0x5e, 0x00, 0x5f, 0x00, 0x00, 0x00, 0x00, 0x00,	/* 0x30 */
	0x00, 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x2f,
	0x5c, 0x7e, 0x00, 0x7c, 0x00, 0x00, 0x60, 0x27,	/* 0x40 */
	0x22, 0x22, 0x28, 0x29, 0x00, 0x00, 0x5b, 0x5d,
	0x7b, 0x7d, 0x00, 0x00, 0x00, 0x00, 0xa2, 0xa3,	/* 0x50 */
	0x00, 0x00, 0x00, 0x00, 0x2b, 0x2d, 0x00, 0x00,
	0x00, 0x3d, 0x00, 0x3c, 0x3e, 0x00, 0x00, 0x00,	/* 0x60 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c,
	0x24, 0x00, 0x00, 0x25, 0x23, 0x26, 0x2a, 0x40,	/* 0x70 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xf0 */
};
static CONST u_short jisindex[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	0x0000, 0x3021, 0x3021, 0x304a, 0x304a, 0x3126, 0x3126, 0x3141,
	0x3141, 0x3177, 0x3177, 0x323c, 0x323c, 0x346b, 0x346b, 0x3665,
	0x3665, 0x3735, 0x3735, 0x3843, 0x3843, 0x3a33, 0x3a33, 0x3b45,
	0x3b45, 0x3f5a, 0x3f5a, 0x4024, 0x4024, 0x4139, 0x4139, 0x423e,
	0x423e, 0x434d, 0x434d, 0x4445, 0x4445, 0x4445, 0x4462, 0x4462,
	0x4546, 0x4546, 0x4660, 0x4673, 0x4728, 0x4729, 0x4735, 0x4743,
	0x4743, 0x4743, 0x485b, 0x485b, 0x485b, 0x4954, 0x4954, 0x4954,
	0x4a3a, 0x4a3a, 0x4a3a, 0x4a5d, 0x4a5d, 0x4a5d, 0x4b60, 0x4c23,
	0x4c33, 0x4c3d, 0x4c4e, 0x4c69, 0x4c69, 0x4c7b, 0x4c7b, 0x4d3d,
	0x4d3d, 0x4d65, 0x4d78, 0x4e5c, 0x4e61, 0x4f24, 0x4f41, 0x4f41,
	0x304a, 0x3141, 0x5021, 0x5021, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xf0 */
};


static int NEAR inkanjiconv(buf, s)
char *buf;
CONST char *s;
{
	int code;

	code = (inputkcode != NOCNV) ? inputkcode : DEFCODE;
#ifdef	DEP_PTY
	if (parentfd >= 0 && ptyinkcode != NOCNV) code = ptyinkcode;
#endif
	kanjiconv(buf, s, MAXKANJIBUF, DEFCODE, code, L_INPUT);
	if (kanjierrno) return(-1);
	if (!buf[1]) return((u_char)(buf[0]));
	return(0);
}

static u_int NEAR getdefcode(c, type, kana)
u_int c;
int type, kana;
{
	CONST char *cp;
	char buf[MAXUTF8LEN + 1], tmp[MAXKLEN + 1];
	int i, code;

	switch (type) {
		case 'J':
			code = EUC;
			kana = 0;
			c ^= 0x8080;
			break;
		case 'E':
			code = EUC;
			break;
		case 'S':
			code = SJIS;
			break;
		case 'K':
			code = EUC;
			kana = 0;
			VOID_C code2kanji(buf, c);
			tmp[0] = (buf[0] & 0xf);
			tmp[1] = ((buf[0] & 0xf0) >> 4);
			if (tmp[0] >= 10 || tmp[1] >= 10) return((u_int)0);
			buf[0] = tmp[1] * 10 + tmp[0];
			tmp[0] = (buf[1] & 0xf);
			tmp[1] = ((buf[1] & 0xf0) >> 4);
			if (tmp[0] >= 10 || tmp[1] >= 10) return((u_int)0);
			buf[1] = tmp[1] * 10 + tmp[0];
			buf[0] += 0xa0;
			buf[1] += 0xa0;
			buf[2] = type = '\0';
			break;
#ifdef	DEP_UNICODE
		case 'U':
			code = UTF8;
			i = ucs2toutf8(buf, 0, c);
			buf[i] = type = '\0';
			break;
#endif
		default:
			return((u_int)0);
/*NOTREACHED*/
			break;
	}

	if (type) VOID_C code2kanji(buf, c);
	cp = kanjiconv2(tmp, buf, MAXKLEN, code, DEFCODE, L_INPUT);
	if (kanjierrno) return((u_int)0);
	i = 0;
	if (!iskanji1(cp, 0) && (!kana || !iskana1(cp, &i))) return((u_int)0);

	return(((u_char)(cp[0]) << 8) | (u_char)(cp[1]));
}

#ifdef	DEP_PTY
u_int ime_getkeycode(s)
CONST char *s;
{
	int n, c;

	if ((n = getkeycode(s, 0)) >= 0) return((u_int)n);
	if (!(n = *(s++)) || !Xsscanf(s, "%<0*x%$", JIS_DIGIT, &c))
		return((u_int)0);

	return(getdefcode(c, n, 1));
}

int ime_inkanjiconv(buf, c)
char *buf;
u_int c;
{
	char tmp[MAXKLEN + 1];

	VOID_C code2kanji(tmp, c);
	return(inkanjiconv(buf, tmp));
}
#endif	/* DEP_PTY */

static VOID NEAR imeputch(c, so)
int c, so;
{
	if (so) Xputterm(T_STANDOUT);
	VOID_C XXputch(c);
	if (ime_xposp) (*ime_xposp)++;
	if (so) Xputterm(END_STANDOUT);
}

static int NEAR imeputs(s)
CONST char *s;
{
	int len;

	len = Xkanjiputs(s);
	if (ime_xposp) *ime_xposp += len;
	return(len);
}

#ifdef	USESTDARGH
/*VARARGS1*/
static int NEAR imeprintf(CONST char *fmt, ...)
#else
/*VARARGS1*/
static int NEAR imeprintf(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *buf;
	int n;

	VA_START(args, fmt);
	n = vasprintf2(&buf, fmt, args);
	va_end(args);

	XXcputs(buf);
	if (ime_xposp) *ime_xposp += n;
	Xfree(buf);

	return(n);
}

static int NEAR jisputs(c)
u_int c;
{
	char buf[MAXKLEN * R_MAXKANA + 1];

	VOID_C jis2str(buf, c);
	return(imeputs(buf));
}

static int NEAR putjisbuf(jp)
jisbuf *jp;
{
	int n, len;

	if (!jp) return(0);
	for (n = len = 0; n < jp -> max; n++) len += jisputs(jp -> buf[n]);

	return(len);
}

static int NEAR countjisbuf(jp)
jisbuf *jp;
{
	char buf[MAXKLEN * R_MAXKANA + 1];
	int n, len;

	if (!jp) return(0);
	for (n = len = 0; n < jp -> max; n++) {
		if (!(jp -> buf[n])) break;
		VOID_C jis2str(buf, jp -> buf[n]);
		len += strlen2(buf);
	}

	return(len);
}

static VOID NEAR addjisbuf(jp, c)
jisbuf *jp;
u_int c;
{
	if (!(jp -> buf) || jp -> max + 1 >= jp -> size) {
		jp -> size += BUFUNIT;
		jp -> buf = (u_short *)Xrealloc(jp -> buf,
			jp -> size * sizeof(u_short));
	}

	jp -> buf[(jp -> max)++] = c;
}

static VOID NEAR copyjisbuf(jp, kbuf, max)
jisbuf *jp;
CONST u_short *kbuf;
int max;
{
	int i;

	jp -> max = 0;
	if (max < 0) for (i = 0; kbuf[i]; i++) addjisbuf(jp, kbuf[i]);
	else for (i = 0; i < max; i++) addjisbuf(jp, kbuf[i]);
}

static u_int NEAR zen2han(c)
u_int c;
{
	u_int n;

	switch (c & 0xff00) {
		case 0x2100:
			if ((n = hanlist2[(u_char)c])) c = n;
			break;
		case 0x2300:
			c &= 0xff;
			break;
		case 0x2400:
		case 0x2500:
			if ((n = hanlist[(u_char)c])) {
				if (!(n & 0xff)) n >>= 8;
				c = n;
			}
			break;
		default:
			break;
	}

	return(c);
}

static u_int NEAR kanabias(c)
u_int c;
{
	if (c) switch (kanamode) {
		case KN_KATA:
			if ((c & 0xff00) == 0x2400) c = (0x2500 | (c & 0xff));
			break;
		case KN_HAN:
		case KN_DIRECT:
			c = zen2han(c);
			break;
		default:
			break;
	}

	return(c);
}

static int NEAR romanprompt(plen, llenp)
int plen, *llenp;
{
	CONST char *cp;
	int llen;

	if (plen) (*ime_locate)(plen, ime_line);
	else {
		(*ime_locate)(0, ime_line);
		switch (kanamode) {
			case KN_HIRA:
				cp = IMERM_K;
				break;
			case KN_KATA:
				cp = IMEKT_K;
				break;
			case KN_HAN:
				cp = IMEHN_K;
				break;
			default:
				cp = IMEDI_K;
				break;
		}
		plen = imeputs(cp);
	}

	if (isvalidbuf(kjisbuf)) llen = putjisbuf(&kjisbuf);
	else if (isvalidbuf(rjisbuf)) {
		Xputterm(T_STANDOUT);
		llen = putjisbuf(&rjisbuf);
		Xputterm(END_STANDOUT);
	}
	else llen = 0;

	Xputterm(L_CLEAR);
	Xtflush();
	if (llenp) *llenp = llen;

	return(plen);
}

static int NEAR fixroman(ptr, min, max)
int ptr, min, max;
{
	u_int n, c;
	int i;

	if (min + 1 >= max || !ptr) return(-1);

	for (i = 0; i < ptr; i++) {
		c = romanlist[min + 1].str[i];
		if (c == 'n' && ptr == 1) c = J_NN;
		else if ((n = jislist[(u_char)c])) c = n;
		addjisbuf(&rjisbuf, kanabias(c));
	}

	return(0);
}

static int NEAR findroman(c, ptr, minp, maxp)
int c, ptr, *minp, *maxp;
{
	int n, min, max;

	min = *minp;
	max = *maxp;
	for (;;) {
		n = (min + max) / 2;
		if (n <= min || n >= max) return(-1);
		if (ptr >= romanlist[n].len) min = n;
		else if (c > romanlist[n].str[ptr]) min = n;
		else if (c < romanlist[n].str[ptr]) max = n;
		else break;
	}

	for (min = n - 1; min > *minp; min--) {
		if (ptr >= romanlist[min].len) break;
		if (c != romanlist[min].str[ptr]) break;
	}
	for (max = n + 1; max < *maxp; max++) {
		if (ptr >= romanlist[max].len) break;
		if (c != romanlist[max].str[ptr]) break;
	}
	*minp = min;
	*maxp = max;

	return(min + 1);
}

static int NEAR unlimitroman(ptr, llenp, minp, maxp)
int ptr, *llenp, *minp, *maxp;
{
	int i, n, c, min, max;

	min = -1;
	max = maxromanlist;
	if (ptr) {
		if (--ptr) {
			min = *minp;
			max = *maxp;
			i = ptr - 1;
			if ((n = min + 1) >= max || i >= romanlist[n].len)
				return(-1);
			c = romanlist[n].str[i];
			for (; min >= 0; min--) {
				if (i >= romanlist[min].len) break;
				if (c != romanlist[min].str[i]) break;
			}
			for (; max < maxromanlist; max++) {
				if (i >= romanlist[max].len) break;
				if (c != romanlist[max].str[i]) break;
			}
		}
	}
	else if (isvalidbuf(kjisbuf)) {
		kjisbuf.max = 0;
		VOID_C romanprompt(0, llenp);
	}
	else if (isvalidbuf(rjisbuf)) {
		(rjisbuf.max)--;
		*llenp = countjisbuf(&rjisbuf);
	}
	else ptr = -2;

	*minp = min;
	*maxp = max;

	return(ptr);
}

static u_int NEAR getjisindex(jp)
jisbuf *jp;
{
	u_int c;

	c = jp -> buf[0];
	if (!(c & ~0xff)) {
		if (!(c = jislist[(u_char)c])) return(c);
	}
	else if ((c & 0x8080) == 0x8080) {
		if (!(c = jislist[(u_char)(c >> 8)])) return(c);
	}

	if ((c & 0xff00) == 0x2400 || (c & 0xff00) == 0x2500)
		if (jisindex[(u_char)c]) c = jisindex[(u_char)c];

	return(c);
}

static u_int NEAR getjiscode(typep, jp)
int *typep;
jisbuf *jp;
{
	char buf[MAXKLEN + 1];
	u_int n, c;
	u_short w;
	int i;

	if (jp -> max != 1 + JIS_DIGIT) return((u_int)0);

	*typep = jp -> buf[0] - 0x2300;
	n = (u_int)0;
	for (i = 1; i <= JIS_DIGIT; i++) {
		if ((c = zen2han(jp -> buf[i])) & ~0xff) return((u_int)0);
		c = Xtolower(c);
		if (Xisdigit(c)) c -= '0';
		else if (Xisxdigit(c)) c -= 'a' - 10;
		else return((u_int)0);
		n = (n << 4) + c;
	}

	if (!(n = getdefcode(n, *typep, 0))) return((u_int)0);
	VOID_C code2kanji(buf, n);
	VOID_C str2jis(&w, 1, buf);

	return(w);
}

static VOID NEAR dispjiscode(c, type)
u_int c;
int type;
{
	CONST char *cp;
	char buf[MAXUTF8LEN + 1], tmp[MAXKLEN * R_MAXKANA + 1];

	cp = NULL;
	kanjierrno = 0;
	switch (type) {
		case 'J':
			break;
		case 'E':
			c |= 0x8080;
			break;
		case 'S':
			VOID_C jis2str(tmp, c);
			cp = kanjiconv2(buf, tmp,
				MAXKLEN, DEFCODE, SJIS, L_INPUT);
			break;
		case 'K':
			VOID_C code2kanji(buf, c - 0x2020);
			buf[0] = (((buf[0] / 10) << 4) | (buf[0] % 10));
			buf[1] = (((buf[1] / 10) << 4) | (buf[1] % 10));
			cp = buf;
			break;
#ifdef	DEP_UNICODE
		case 'U':
			VOID_C jis2str(tmp, c);
			cp = kanjiconv2(buf, tmp,
				MAXUTF8LEN, DEFCODE, UTF8, L_INPUT);
			c = ucs2fromutf8((u_char *)cp, NULL);
			cp = NULL;
			break;
#endif
		default:
			kanjierrno++;
			break;
	}

	if (cp)	c = (((u_char)(cp[0]) << 8) | (u_char)(cp[1]));
	if (kanjierrno) VOID_C imeprintf("[%c????]:", type);
	else VOID_C imeprintf("[%c%0*X]:", type, JIS_DIGIT, c);
	kanjierrno = 0;
}

static VOID NEAR imeputcursor(xpos, n, so)
int xpos, n, so;
{
	(*ime_locate)(xpos, ime_line);
	n += (n < 10) ? '0' : 'A' - 10;
	imeputch(n, so);
#ifdef	DEP_PTY
	if (ptylist[win].pid) /*EMPTY*/;
	else
#endif
	(*ime_locate)(imewin_x, imewin_y);
	Xtflush();
}

static int NEAR listkanji(plen, argc, argv, minp, maxp, prevp, xpos)
int plen;
long argc;
u_short **argv;
long *minp, *maxp, *prevp;
int xpos[];
{
	char buf[MAXKLEN * R_MAXKANA + 1];
	long n, min, max, prev;
	int i, x, col, len;

	if (plen) (*ime_locate)(plen, ime_line);
	else {
		(*ime_locate)(0, ime_line);
		plen = imeputs(IMEKJ_K);
	}
	Xputterm(L_CLEAR);

	col = 0;
	x = plen + 1;
	min = 0L;
	prev = -1L;
	for (n = 0L; argv[n]; n++) {
		for (i = len = 0; argv[n][i]; i++) {
			VOID_C jis2str(buf, argv[n][i]);
			len += strlen2(buf);
		}

		if (col >= KNJ_COLUMN || x + 2 + len > n_lastcolumn) {
			if (argc >= min && argc < n) break;
			col = 0;
			x = plen + 1;
			prev = min;
			min = n;
		}
		xpos[col++] = x;
		x += 2 + len + 1;
	}
	max = n;
	if (min >= max) return(-1);

	col = 0;
	for (n = min; n < max; n++) {
		(*ime_locate)(xpos[col], ime_line);
		VOID_C imeprintf("%1X.", col++);
		for (i = 0; argv[n][i]; i++) jisputs(argv[n][i]);
	}
	Xtflush();
	if (minp) *minp = min;
	if (maxp) *maxp = max;
	if (prevp) *prevp = prev;

	return(plen);
}

static int NEAR selectkanji(argcp, argv, sig)
long *argcp;
u_short **argv;
int sig;
{
	long n, argc, old, min, max, prev;
	int ch, plen, last, xpos[KNJ_COLUMN];

	argc = *argcp;
	plen = listkanji(0, argc, argv, &min, &max, &prev, xpos);
	if (plen < 0) return(-1);

	last = '\0';
	do {
		imeputcursor(xpos[argc - min], argc - min, 1);

		if ((ch = getkey3(sig, inputkcode, 0)) < 0) break;
		old = argc;
		switch (ch) {
			case ' ':
			case K_RIGHT:
				n = argc + 1L;
				if (argv[n]) /*EMPTY*/;
				else if (ch == ' ') n = 0L;
				else {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				argc = n;
				break;
			case K_BS:
			case K_LEFT:
				n = argc - 1L;
				if (n >= 0L) /*EMPTY*/;
				else if (ch == K_BS) {
					ch = K_ESC;
					break;
				}
				else {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				argc = n;
				break;
			case K_UP:
				n = prev;
				if (n < 0L) {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				argc -= min;
				VOID_C listkanji(plen, n, argv,
					&min, &max, &prev, xpos);
				while (n + 1 < max && argc-- > 0L) n++;
				old = argc = n;
				break;
			case K_DOWN:
				n = max;
				if (!(argv[n])) {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				argc -= min;
				VOID_C listkanji(plen, n, argv,
					&min, &max, &prev, xpos);
				while (n + 1 < max && argc-- > 0L) n++;
				old = argc = n;
				break;
			case K_CTRL('L'):
				plen = listkanji(0, argc, argv,
					&min, &max, &prev, xpos);
				if (plen < 0) return(-1);
				break;
			default:
				if (Xisdigit(ch)) ch -= '0';
				else if (Xisupper(ch) && Xisxdigit(ch))
					ch -= 'A' - 10;
				else {
					if (!(ch & ~0xff) && Xisprint(ch)) {
						last = ch;
						ch = K_CR;
					}
					break;
				}

				if (min + ch >= max) {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				argc = min + ch;
				ch = K_CR;
				break;
		}
		if (argc == old) continue;
		else if (argc >= min && argc < max)
			imeputcursor(xpos[old - min], old - min, 0);
		else VOID_C listkanji(plen, argc, argv,
			&min, &max, &prev, xpos);
	} while (ch != K_ESC && ch != K_CR);

	kjisbuf.max = 0;
	if (ch == K_CR) copyjisbuf(&kjisbuf, argv[argc], -1);

	*argcp = argc;
	return(last);
}

static int NEAR listjis(plen, c, colp, type)
int plen;
u_int c;
int *colp, type;
{
	u_int start;
	int i, col;

	if (plen) (*ime_locate)(plen, ime_line);
	else {
		(*ime_locate)(0, ime_line);
		plen = imeputs(IMEJS_K);
	}
	Xputterm(L_CLEAR);

	for (col = 16; col > 1; col /= 2)
		if (jis_xpos(plen, col) <= n_lastcolumn) break;
	if (!col) return(-1);

	(*ime_locate)(plen, ime_line);
	dispjiscode(c, type);

	start = (c / col) * col;
	for (i = 0; i < col; i++, start++) {
		if (!VALIDJIS(start)) continue;
		(*ime_locate)(jis_xpos(plen, i), ime_line);
		VOID_C imeprintf("%1X.", i);
		jisputs(start);
	}
	Xtflush();
	if (colp) *colp = col;

	return(plen);
}

static int NEAR selectjis(c, sig, type)
u_int c;
int sig, type;
{
	u_int i, n, old;
	int ch, plen, last, col;

	if ((plen = listjis(0, c, &col, type)) < 0) return(-1);

	last = '\0';
	do {
		(*ime_locate)(plen, ime_line);
		dispjiscode(c, type);
		imeputcursor(jis_xpos(plen, c % col), c % col, 1);

		if ((ch = getkey3(sig, inputkcode, 0)) < 0) break;
		old = c;
		switch (ch) {
			case ' ':
			case K_RIGHT:
				for (n = c + 1; n <= J_MAX; n++)
					if (VALIDJIS(n)) break;
				if (n <= J_MAX) /*EMPTY*/;
				else if (ch == ' ') n = J_MIN;
				else {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				c = n;
				break;
			case K_BS:
			case K_LEFT:
				for (n = c - 1; n >= J_MIN; n--)
					if (VALIDJIS(n)) break;
				if (n >= J_MIN) /*EMPTY*/;
				else if (ch == K_BS) {
					ch = K_ESC;
					break;
				}
				else {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				c = n;
				break;
			case K_UP:
				for (n = c - col; n >= J_MIN; n -= col) {
					if ((n & 0xff) < 0x20) {
						n -= 0x100;
						n &= 0xff0f;
						n |= 0x0070;
					}
					if (VALIDJIS(n)) break;
					for (i = n + 1; i % col; i++)
						if (VALIDJIS(i)) break;
					if (i % col) {
						n = i;
						break;
					}
					for (i = n - 1; (i + 1) % col; i--)
						if (VALIDJIS(i)) break;
					if (i % col) {
						n = i;
						break;
					}
				}
				if (n < J_MIN) {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				c = n;
				break;
			case K_DOWN:
				for (n = c + col; n <= J_MAX; n += col) {
					if ((n & 0xff) > 0x7f) {
						n += 0x100;
						n &= 0xff0f;
						n |= 0x0020;
					}
					if (VALIDJIS(n)) break;
					for (i = n + 1; i % col; i++)
						if (VALIDJIS(i)) break;
					if (i % col) {
						n = i;
						break;
					}
					for (i = n - 1; (i + 1) % col; i--)
						if (VALIDJIS(i)) break;
					if (i % col) {
						n = i;
						break;
					}
				}
				if (n > J_MAX) {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				c = n;
				break;
			case K_CTRL('L'):
				if ((plen = listjis(0, c, &col, type)) < 0)
					ch = K_ESC;
				break;
			default:
				if (Xisdigit(ch)) ch -= '0';
				else if (Xisupper(ch) && Xisxdigit(ch))
					ch -= 'A' - 10;
				else {
					if (!(ch & ~0xff) && Xisprint(ch)) {
						last = ch;
						ch = K_CR;
					}
					break;
				}

				if (ch >= col) ch = 0;
				else {
					ch += (c / col) * col;
					if (!VALIDJIS(ch)) ch = 0;
				}
				if (!ch) {
					Xputterm(T_BELL);
					Xtflush();
					break;
				}
				c = ch;
				ch = K_CR;
				break;
		}
		if (c == old) continue;
		else if (c / col == old / col)
			imeputcursor(jis_xpos(plen, old % col), old % col, 0);
		else VOID_C listjis(plen, c, NULL, type);
	} while (ch != K_ESC && ch != K_CR);

	kjisbuf.max = 0;
	if (ch == K_CR) addjisbuf(&kjisbuf, c);

	return(last);
}

static int NEAR searchkanji(argcp, argvp, ptr, min, max, lastp, sig)
long *argcp;
u_short ***argvp;
int ptr, min, max, *lastp, sig;
{
	u_short *kbuf, **argv;
	long argc;
	u_int c;
	int i, n, type;

	argc = *argcp;
	argv = *argvp;
	if (isvalidbuf(kjisbuf)) {
		n = 1;
		if (!argv) {
			if (!(c = getjisindex(&kjisbuf))
			|| (i = selectjis(c, sig, 'J')) < 0)
				n = -1;
			else if (i > 0) *lastp = i;
		}
		else if (*argv) {
			if ((i = selectkanji(&argc, argv, sig)) < 0) n = -1;
			else if (i > 0) *lastp = i;
		}
	}
	else if (fixroman(ptr, min, max) < 0 && !isvalidbuf(rjisbuf)) n = 0;
	else {
		freekanjilist(argv);
		argc = 0L;
		n = 1;
		if ((c = getjiscode(&type, &rjisbuf))) {
			if ((i = selectjis(c, sig, type)) < 0) n = -1;
			else if (i > 0) *lastp = i;
		}
		else {
			kbuf = (u_short *)Xmalloc((rjisbuf.max + 1)
				* sizeof(u_short));
			for (i = 0; i < rjisbuf.max; i++) {
				c = rjisbuf.buf[i];
				if (!(c & ~0xff) && !(c = jislist[(u_char)c]))
					c = rjisbuf.buf[i];
				if ((c & 0xff00) == 0x2500
				&& jisindex[(u_char)c])
					c = (0x2400 | (c & 0xff));
				kbuf[i] = c;
			}
			kbuf[rjisbuf.max] = 0;
			if ((argv = searchdict(kbuf, rjisbuf.max))) {
				copyjisbuf(&kjisbuf, argv[argc], -1);
				Xfree(kbuf);
			}
			else {
				copyjisbuf(&kjisbuf, rjisbuf.buf, rjisbuf.max);
				argv = (u_short **)
					Xmalloc(2 * sizeof(short *));
				argv[0] = kbuf;
				argv[1] = NULL;
			}
		}
	}

	*argcp = argc;
	*argvp = argv;
	return(n);
}

static int NEAR fixkanji(ptr, min, max, bias)
int ptr, min, max, bias;
{
	int i;

	if (isvalidbuf(kjisbuf)) return(-1);
	else if (fixroman(ptr, min, max) < 0 && !isvalidbuf(rjisbuf))
		return(-2);

	if (!bias || kanamode == KN_DIRECT) {
		copyjisbuf(&kjisbuf, rjisbuf.buf, rjisbuf.max);
		if (bias) return(-1);
	}
	else for (i = 0; i < rjisbuf.max; i++)
		rjisbuf.buf[i] = kanabias(rjisbuf.buf[i]);
	return(0);
}

static int NEAR inputkanji(sig)
int sig;
{
	static int last = '\0';
	u_short **argv;
	long argc;
	u_int w;
	int i, n, c, min, max, ptr, plen, llen, cont;

	subwindow++;

	min = -1;
	max = maxromanlist;
	c = '\0';
	ptr = 0;
	argc = 0L;
	argv = NULL;
	ime_cont = 1;
	plen = romanprompt(0, &llen);

	for (;;) {
		if (c != ' ') {
			freekanjilist(argv);
			argc = 0L;
			argv = NULL;
		}

		if (last) {
			c = last;
			last = '\0';
		}
		else {
			(*ime_locate)(plen + llen + ptr, ime_line);
			imeputch(' ', 1);
#ifdef	DEP_PTY
			if (ptylist[win].pid) /*EMPTY*/;
			else
#endif
			(*ime_locate)(imewin_x, imewin_y);
			Xtflush();
			if ((c = getkey3(sig, inputkcode, 0)) < 0) break;
			else if (c == imekey) {
				ime_cont = kjisbuf.max = 0;
				c = K_ESC;
				break;
			}

			cont = 0;
			switch (c) {
				case K_BS:
				case K_DC:
					cont = 1;
					n = unlimitroman(ptr,
						&llen, &min, &max);
					if (n < 0) {
						if (n < -1) cont = -1;
						break;
					}
					ptr = n;
					(*ime_locate)(plen + llen + ptr,
						ime_line);
					Xputterm(L_CLEAR);
					Xtflush();
					break;
				case ' ':
					cont = 1;
					n = searchkanji(&argc, &argv,
						ptr, min, max, &last, sig);
					if (n < 0) {
						Xputterm(T_BELL);
						Xtflush();
						break;
					}
					else if (!n) {
						cont = -1;
						if (!(w = jislist[(u_char)c]))
							break;
						w = kanabias(w);
						addjisbuf(&kjisbuf, w);
						c = '\0';
					}
					else if (last) {
						cont = -1;
						c = '\0';
						break;
					}

					ptr = 0;
					min = -1;
					max = maxromanlist;
					VOID_C romanprompt(0, &llen);
					Xtflush();
					break;
				case '\t':
					if (++kanamode >= KN_MAX)
						kanamode = KN_HIRA;
					n = fixkanji(ptr, min, max, 1);
					if (n == -1) {
						cont = -1;
						c = '\0';
						break;
					}
/*FALLTHRU*/
				case K_CTRL('L'):
					cont = 1;
					plen = romanprompt(0, &llen);
					if (min + 1 >= max || !ptr) break;
					Xputterm(T_STANDOUT);
					VOID_C imeprintf("%.*s",
						ptr, romanlist[min + 1].str);
					Xputterm(END_STANDOUT);
					Xtflush();
					break;
				case K_ESC:
					cont = 1;
					if (!isvalidbuf(kjisbuf)) {
						cont = -1;
						ime_cont = 0;
						break;
					}
					kjisbuf.max = 0;
					ptr = 0;
					min = -1;
					max = maxromanlist;
					VOID_C romanprompt(plen, &llen);
					Xtflush();
					break;
				case K_CR:
					cont = 1;
					n = fixkanji(ptr, min, max, 0);
					if (n < 0) {
						cont = -1;
						if (n >= -1) c = '\0';
						break;
					}
					ptr = 0;
					min = -1;
					max = maxromanlist;
					VOID_C romanprompt(plen, &llen);
					Xtflush();
					break;
				default:
					break;
			}
			if (cont < 0) break;
			else if (cont > 0) continue;
			else if (!(c & ~0xff) && Xisprint(c)) /*EMPTY*/;
			else if (!ptr
			&& !isvalidbuf(kjisbuf) && !isvalidbuf(rjisbuf))
				break;
			else {
				Xputterm(T_BELL);
				Xtflush();
				continue;
			}
		}

		if (kanamode == KN_DIRECT) break;
		else if (isvalidbuf(kjisbuf)) {
			last = c;
			c = '\0';
			break;
		}

		(*ime_locate)(plen + llen + ptr, ime_line);
		imeputch(c, 1);
		Xtflush();

		if ((n = findroman(c, ptr, &min, &max)) < 0) {
			w = (ptr == 1 && min + 1 < max)
				? romanlist[min + 1].str[0] : (u_int)0;
			if (w == 'n') w = J_NN;
			else if (w == (u_int)c) w = J_TSU;
			else if (ptr || !(w = jislist[(u_char)c]))
				w = (u_int)0;
			else c = '\0';

			if (w) {
				w = kanabias(w);
				addjisbuf(&rjisbuf, w);
				(*ime_locate)(plen + llen, ime_line);
				Xputterm(T_STANDOUT);
				llen += jisputs(w);
				Xputterm(END_STANDOUT);
				if (c) imeputch(c, 1);
				last = c;
				ptr = 0;
				min = -1;
				max = maxromanlist;
				continue;
			}
			Xputterm(T_BELL);
			Xtflush();
		}
		else if (ptr + 1 < romanlist[n].len) ptr++;
		else {
			(*ime_locate)(plen + llen, ime_line);
			Xputterm(T_STANDOUT);
			for (i = 0; i < R_MAXKANA; i++) {
				if (!(w = romanlist[n].code[i])) break;
				w = kanabias(w);
				addjisbuf(&rjisbuf, w);
				llen += jisputs(w);
			}
			Xputterm(END_STANDOUT);
			ptr = 0;
			min = -1;
			max = maxromanlist;
		}
	}

	if (argv && !c && isvalidbuf(rjisbuf) && isvalidbuf(kjisbuf))
		saveuserfreq(rjisbuf.buf, argv[argc]);
	subwindow--;
	freekanjilist(argv);
	return(c);
}

static int NEAR getkanjibuf(buf, c)
char *buf;
u_int c;
{
	char tmp[MAXKLEN * R_MAXKANA + 1];

	VOID_C jis2str(tmp, c);
	return(inkanjiconv(buf, tmp));
}

int ime_inputkanji(sig, buf)
int sig;
char *buf;
{
	u_int w;
	int c;

	if (!buf) {
		ime_cont = kjisbuf.max = 0;
		VOID_C searchdict(NULL, -1);
		return(0);
	}

	initroman();

	c = -1;
	if (!ime_cont) {
		keyflush();
		kanamode = KN_HIRA;
		kjisbuf.max = 0;
	}
	else if (isvalidbuf(kjisbuf)) {
		memmove((char *)(kjisbuf.buf), (char *)&(kjisbuf.buf[1]),
			(kjisbuf.max)-- * sizeof(u_short));
		if (!(w = kjisbuf.buf[0]) || (c = getkanjibuf(buf, w)) < 0) {
			kjisbuf.max = 0;
			c = -1;
		}
	}
	rjisbuf.max = 0;

	if (c < 0) {
		if (!ime_line) ime_line = L_MESLINE;
		if (!ime_locate) ime_locate = Xlocate;
		imewin_x = win_x;
		imewin_y = win_y;
		for (;;) {
			if ((c = inputkanji(sig))) break;
			else if (!isvalidbuf(kjisbuf)) {
				*buf = '\0';
				break;
			}
			else if ((c = getkanjibuf(buf, kjisbuf.buf[0])) >= 0) {
				(kjisbuf.max)--;
				break;
			}
		}
		win_x = imewin_x;
		win_y = imewin_y;
#ifdef	DEP_PTY
		if (ptylist[win].pid) /*EMPTY*/;
		else
#endif
		(*ime_locate)(win_x, win_y);
	}

	ime_line = 0;
	ime_xposp = NULL;
	ime_locate = NULL;
	if (!ime_cont) VOID_C searchdict(NULL, -1);

	return(c);
}

# ifdef	DEBUG
VOID ime_freebuf(VOID_A)
{
	kjisbuf.max = 0;
	Xfree(kjisbuf.buf);
	kjisbuf.buf = NULL;
	rjisbuf.max = 0;
	Xfree(rjisbuf.buf);
	rjisbuf.buf = NULL;
}
# endif	/* DEBUG */
#endif	/* DEP_IME */
