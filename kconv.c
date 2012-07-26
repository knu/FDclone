/*
 *	kconv.c
 *
 *	Kanji convert functions
 */

#define	K_EXTERN
#ifdef	FD
#include "fd.h"
#include "termio.h"
#include "realpath.h"
#include "parse.h"
#include "func.h"
#else
#include "headers.h"
#include "depend.h"
#include "kctype.h"
#include "typesize.h"
#include "string.h"
#endif

#include "unixemu.h"
#include "kconv.h"

#ifdef	DEP_PTY
#include "termemu.h"
#endif
#if	MSDOS
#include <io.h>
#endif

#define	ASCII			000
#define	KANA			001
#define	KANJI			002
#define	JKANA			004
#define	J_UDEF			0x222e	/* GETA */
#define	SJ_UDEF			0x81ac	/* GETA */
#define	U2_UDEF			0x3013	/* GETA */
#define	UNICODETBL		"fd-unicd.tbl"
#define	MINUNICODE		0x00a7
#define	MAXUNICODE		0xffe5
#define	MINKANJI		0x8140
#define	MAXKANJI		0xfc4b
#define	jcnv(c, io)		((((io) == L_FNAME) \
				&& ((c) == '/')) ? ' ' : (c))
#define	jdecnv(c, io)		((((io) == L_FNAME) \
				&& ((c) == ' ')) ? '/' : (c))
#ifndef	FD
#define	sureread		read
#endif
#ifdef	CODEEUC
#define	kencode			ujis2sjis
#define	kdecode			sjis2ujis
#else
#define	kencode			sjis2ujis
#define	kdecode			ujis2sjis
#endif
#ifndef	_NOKANJICONV
#define	HC_HEX			0001
#define	HC_CAP			0002
#define	tobin(c)		h2btable[(u_char)(c)]
#define	tobin2(s, i)		(u_char)((tobin((s)[i]) << 4) \
				| (tobin((s)[(i) + 1])))
#define	HEXTAG			':'
#define	ishex(s, i)	(((s)[i] == HEXTAG) \
			&& (hctypetable[(u_char)(s)[(i) + 1]] & HC_HEX) \
			&& (hctypetable[(u_char)(s)[(i) + 2]] & HC_HEX))
#define	CAPTAG			':'
#define	iscap(s, i)	(((s)[i] == CAPTAG) \
			&& (hctypetable[(u_char)(s)[(i) + 1]] & HC_CAP) \
			&& (hctypetable[(u_char)(s)[(i) + 2]] & HC_HEX))
#endif	/* !_NOKANJICONV */

typedef struct _kconv_t {
	u_short start;
	u_short cnv;
	u_short range;
} kconv_t;

#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
typedef struct _langtable {
	CONST char *ident;
	u_char lang;
} langtable;
#endif

#ifndef	_NOKANJIFCONV
typedef struct _kpathtable {
	char **path;
	u_char code;
} kpathtable;
#endif

#ifdef	DEP_ROCKRIDGE
extern int norockridge;
#endif

#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
static CONST char *NEAR Xstrstr __P_((CONST char *, CONST char *));
#endif
#if	!defined (_NOKANJICONV) || (defined (DEP_DOSEMU) && defined (CODEEUC))
static VOID NEAR sj2j __P_((char *, CONST u_char *));
static VOID NEAR j2sj __P_((char *, CONST u_char *));
#endif
#ifdef	DEP_UNICODE
# ifndef	DEP_EMBEDUNITBL
static int NEAR openunitbl __P_((CONST char *));
static u_char *NEAR newunitbl __P_((ALLOC_T));
# endif
#define	getword(s, n)		(((u_short)((s)[(n) + 1]) << 8) | (s)[n])
#define	skread(f, o, s, n)	(Xlseek(f, o, L_SET) >= (off_t)0 \
				&& sureread(f, s, n) == n)
#endif
#ifndef	_NOKANJICONV
# if	defined (DEP_UNICODE) && !defined (DEP_EMBEDUNITBL)
static int NEAR opennftbl __P_((CONST char *, int, u_int *));
# endif
static int NEAR toenglish __P_((char *, CONST u_char *, int));
static int NEAR tojis7 __P_((char *, CONST u_char *, int, int, int, int));
static int NEAR fromjis __P_((char *, CONST u_char *, int, int));
static int NEAR tojis8 __P_((char *, CONST u_char *, int, int, int, int));
static int NEAR tojunet __P_((char *, CONST u_char *, int, int, int, int));
# ifdef	DEP_UNICODE
static u_int NEAR toucs2 __P_((CONST u_char *, int *));
static VOID NEAR fromucs2 __P_((char *, int *, u_int));
static int NEAR toutf8 __P_((char *, CONST u_char *, int));
static int NEAR fromutf8 __P_((char *, CONST u_char *, int));
static int NEAR toutf8nf __P_((char *, CONST u_char *, int, int));
static int NEAR fromutf8nf __P_((char *, CONST u_char *, int, int));
# endif	/* DEP_UNICODE */
static int NEAR bin2hex __P_((char *, int));
static int NEAR tohex __P_((char *, CONST u_char *, int));
static int NEAR fromhex __P_((char *, CONST u_char *, int));
static int NEAR bin2cap __P_((char *, int));
static int NEAR tocap __P_((char *, CONST u_char *, int));
static int NEAR fromcap __P_((char *, CONST u_char *, int));
static CONST char *NEAR _kanjiconv __P_((char *, CONST char *, int,
		int, int, int *, int));
#endif	/* !_NOKANJICONV */

#ifdef	FD
int noconv = 0;
#endif
#ifndef	_NOKANJIFCONV
int nokanjifconv = 0;
char *sjispath = NULL;
char *eucpath = NULL;
char *jis7path = NULL;
char *jis8path = NULL;
char *junetpath = NULL;
char *ojis7path = NULL;
char *ojis8path = NULL;
char *ojunetpath = NULL;
char *hexpath = NULL;
char *cappath = NULL;
char *utf8path = NULL;
char *utf8macpath = NULL;
char *utf8iconvpath = NULL;
char *noconvpath = NULL;
#endif	/* !_NOKANJIFCONV */
#ifdef	DEP_UNICODE
char *unitblpath = NULL;
int unicodebuffer = 0;
#endif

#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
static CONST langtable langlist[] = {
# ifndef	_NOKANJICONV
	{"sjis", SJIS},
	{"euc", EUC},
	{"ujis", EUC},
	{"utf8-mac", M_UTF8},
	{"mac", M_UTF8},
	{"utf8-iconv", I_UTF8},
	{"utf8", UTF8},
	{"utf-8", UTF8},
	{"ojunet", O_JUNET},
	{"ojis8", O_JIS8},
	{"ojis", O_JIS7},
	{"junet", JUNET},
	{"jis8", JIS8},
	{"jis", JIS7},
	{"hex", HEX},
	{"cap", CAP},
# endif	/* _NOKANJICONV */
# ifndef	_NOENGMES
	{"en", ENG},
	{"POSIX", ENG},
	{"C", ENG},
# endif	/* _NOENGMES */
};
#define	LANGLISTSIZ		arraysize(langlist)
#endif	/* !_NOKANJICONV || (!_NOENGMES && !_NOJPNMES) */
#ifdef	DEP_UNICODE
# ifdef	DEP_EMBEDUNITBL
extern CONST u_char unitblbuf[];
extern u_int unitblent;
# else
static u_char *unitblbuf = NULL;
static u_int unitblent = 0;
# endif
static CONST kconv_t rsjistable[] = {
	{0x8470, 0x8440, 0x0f},		/* Cyrillic small letters */
	{0x8480, 0x844f, 0x12},		/* Why they converted ? */
#define	EXCEPTRUSS		2
	{0x8754, 0xfa4a, 0x0a},		/* Roman numerals */
	{0x8782, 0xfa59, 0x01},		/* numero sign */
	{0x8784, 0xfa5a, 0x01},		/* telephone sign */
	{0x878a, 0xfa58, 0x01},		/* parenthesized ideograph stock */
	{0x8790, 0x81e0, 0x01},		/* nearly equals */
	{0x8791, 0x81df, 0x01},		/* identical to */
	{0x8792, 0x81e7, 0x01},		/* integral */
	{0x8795, 0x81e3, 0x01},		/* square root */
	{0x8796, 0x81db, 0x01},		/* up tack */
	{0x8797, 0x81da, 0x01},		/* angle */
	{0x879a, 0x81e6, 0x01},		/* because */
	{0x879b, 0x81bf, 0x01},		/* intersection */
	{0x879c, 0x81be, 0x01},		/* union */
	{0xed40, 0xfa5c, 0x23},		/* NEC-selected IBM extensions */
	{0xed63, 0xfa80, 0x1c},		/* NEC-selected IBM extensions */
	{0xed80, 0xfa9c, 0x61},		/* NEC-selected IBM extensions */
	{0xede1, 0xfb40, 0x1c},		/* NEC-selected IBM extensions */
	{0xee40, 0xfb5c, 0x23},		/* NEC-selected IBM extensions */
	{0xee63, 0xfb80, 0x1c},		/* NEC-selected IBM extensions */
	{0xee80, 0xfb9c, 0x61},		/* NEC-selected IBM extensions */
	{0xeee1, 0xfc40, 0x0c},		/* NEC-selected IBM extensions */
	{0xeeef, 0xfa40, 0x0a},		/* small Roman numerals */
	{0xeef9, 0x81ca, 0x01},		/* full width not sign */
	{0xeefa, 0xfa55, 0x03},		/* full width broken bar */
					/* full width apostrophe */
					/* full width quotation mark */
	{0xfa54, 0x81ca, 0x01},		/* full width not sign */
	{0xfa5b, 0x81e6, 0x01},		/* because */
};
#define	RSJISTBLSIZ		arraysize(rsjistable)
#endif	/* DEP_UNICODE */
#if	!defined (_NOKANJICONV) || (defined (DEP_DOSEMU) && defined (CODEEUC))
static CONST kconv_t convtable[] = {
	{0xfa40, 0xeeef, 0x0a},		/* small Roman numerals */
	{0xfa4a, 0x8754, 0x0a},		/* Roman numerals */
	{0xfa54, 0x81ca, 0x01},		/* full width not sign */
	{0xfa55, 0xeefa, 0x03},		/* full width broken bar */
					/* full width apostrophe */
					/* full width quotation mark */
	{0xfa58, 0x878a, 0x01},		/* parenthesized ideograph stock */
	{0xfa59, 0x8782, 0x01},		/* numero sign */
	{0xfa5a, 0x8784, 0x01},		/* telephone sign */
	{0xfa5b, 0x81e6, 0x01},		/* because */
	{0xfa5c, 0xed40, 0x23},		/* IBM extensions */
	{0xfa80, 0xed63, 0x1c},		/* IBM extensions */
	{0xfa9c, 0xed80, 0x61},		/* IBM extensions */
	{0xfb40, 0xede1, 0x1c},		/* IBM extensions */
	{0xfb5c, 0xee40, 0x23},		/* IBM extensions */
	{0xfb80, 0xee63, 0x1c},		/* IBM extensions */
	{0xfb9c, 0xee80, 0x61},		/* IBM extensions */
	{0xfc40, 0xeee1, 0x0c}		/* IBM extensions */
};
#define	CNVTBLSIZ		arraysize(convtable)
static CONST u_char sj2jtable1[256] = {
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
static CONST u_char sj2jtable2[256] = {
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
static CONST u_char j2sjtable1[128] = {
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
static CONST u_char j2sjtable2[128] = {
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
static CONST u_char j2sjtable3[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	   0, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5,	/* 0x20 */
	0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad,
	0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5,	/* 0x30 */
	0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd,
	0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5,	/* 0x40 */
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd,
	0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5,	/* 0x50 */
	0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd,
	0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5,	/* 0x60 */
	0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed,
	0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5,	/* 0x70 */
	0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc,    0
};
#endif	/* !_NOKANJICONV || (DEP_DOSEMU && CODEEUC) */
#ifndef	_NOKANJICONV
# ifdef	DEP_UNICODE
#  ifdef	DEP_EMBEDUNITBL
extern int nftblnum;
extern int nflen;
extern CONST u_char *nftblbuf[];
extern u_int nftblent[];
#  else
static int nftblnum = 0;
static int nflen = 0;
static u_char **nftblbuf = NULL;
static u_int *nftblent = 0;
#  endif
# endif	/* DEP_UNICODE */
static CONST u_char hctypetable[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x20 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0x30 */
	0003, 0003, 0000, 0000, 0000, 0000, 0000, 0000,
	0000, 0003, 0003, 0003, 0003, 0003, 0003, 0000,	/* 0x40 */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x50 */
	0000, 0003, 0003, 0003, 0003, 0003, 0003, 0000,	/* 0x60 */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x70 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	/* 0xf0 */
};
static CONST u_char h2btable[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x20 */
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,	/* 0x30 */
	0000, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0000,	/* 0x40 */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x50 */
	0000, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0000,	/* 0x60 */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x70 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	/* 0xf0 */
};
#endif	/* !_NOKANJICONV */
#ifdef	DEP_FILECONV
static CONST kpathtable kpathlist[] = {
	{&sjispath, SJIS},
	{&eucpath, EUC},
	{&jis7path, JIS7},
	{&jis8path, JIS8},
	{&junetpath, JUNET},
	{&ojis7path, O_JIS7},
	{&ojis8path, O_JIS8},
	{&ojunetpath, O_JUNET},
	{&hexpath, HEX},
	{&cappath, CAP},
	{&utf8path, UTF8},
	{&utf8macpath, M_UTF8},
	{&utf8iconvpath, I_UTF8},
	{&noconvpath, NOCNV},
};
#define	KPATHLISTSIZ		arraysize(kpathlist)
#endif	/* DEP_FILECONV */


int onkanji1(s, ptr)
CONST char *s;
int ptr;
{
	int i;

	if (ptr < 0) return(0);
	if (!ptr) return(iskanji1(s, 0));

	for (i = 0; i < ptr; i++) {
		if (!s[i]) return(0);
		else if (iskanji1(s, i)) i++;
#ifdef	CODEEUC
		else VOID_C iskana1(s, &i);
#endif
	}
	if (i > ptr) return(0);

	return(iskanji1(s, i));
}

#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
static CONST char *NEAR Xstrstr(s1, s2)
CONST char *s1, *s2;
{
	int i, c1, c2;

	while (*s1) {
		for (i = 0;; i++) {
			if (!s2[i]) return(s1);
			c1 = s1[i];
			c2 = s2[i];
			if (Xislower(c2)) {
				c1 = Xtoupper(c1);
				c2 = Xtoupper(c2);
			}
			if (c1 != c2) break;
			if (iswchar(s1, 0)) {
				if (!s2[++i]) return(s1);
				if (s1[i] != s2[i]) break;
			}
			if (!s1[i]) break;
		}
		if (iswchar(s1, 0)) s1++;
		s1++;
	}

	return(NULL);
}

/*ARGSUSED*/
int getlang(s, io)
CONST char *s;
int io;
{
	int i, ret;

	ret = NOCNV;
	if (s) for (i = 0; i < LANGLISTSIZ; i++) {
		if (!(kanjiiomode[langlist[i].lang] & io)) continue;
		if (Xstrstr(s, langlist[i].ident)) {
			ret = langlist[i].lang;
			break;
		}
	}

# ifndef	_NOKANJICONV
	if (io == L_INPUT && ret == NOCNV) ret = DEFCODE;
# endif

	return(ret);
}
#endif	/* !_NOKANJICONV || (!_NOENGMES && !_NOJPNMES) */

#if	!defined (_NOKANJICONV) || (defined (DEP_DOSEMU) && defined (CODEEUC))
static VOID NEAR sj2j(buf, s)
char *buf;
CONST u_char *s;
{
	u_int w;
	int n, s1, s2, j1, j2, min, max;

	s1 = (s[0] & 0xff);
	s2 = (s[1] & 0xff);
	if (s1 >= 0xf0) {
		w = (((u_int)s1 << 8) | s2);
		min = -1;
		max = CNVTBLSIZ;
		for (;;) {
			n = (min + max) / 2;
			if (n <= min || n >= max) {
				w = SJ_UDEF;
				kanjierrno = SJIS;
				break;
			}
			if (w >= convtable[n].start + convtable[n].range)
				min = n;
			else if (w < convtable[n].start) max = n;
			else {
				w -= convtable[n].start;
				w += convtable[n].cnv;
				break;
			}
		}
		s1 = ((w >> 8) & 0xff);
		s2 = (w & 0xff);
	}
	j1 = sj2jtable1[s1];
	j2 = sj2jtable2[s2];
	if (!j1 || !j2) {
		j1 = ((J_UDEF >> 8) & 0xff);
		j2 = (J_UDEF & 0xff);
		kanjierrno = SJIS;
	}
	else if (s2 >= 0x9f) j1++;

	buf[0] = j1;
	buf[1] = j2;
}

static VOID NEAR j2sj(buf, s)
char *buf;
CONST u_char *s;
{
	u_int w;
	int i, s1, s2, j1, j2;

	j1 = (s[0] & 0x7f);
	j2 = (s[1] & 0x7f);

	s1 = j2sjtable1[j1];
	s2 = (j1 & 1) ? j2sjtable2[j2] : j2sjtable3[j2];
	if (!s1 || !s2) {
		w = SJ_UDEF;
		kanjierrno = JIS7;
	}
	else w = (((u_int)s1 << 8) | s2);

	for (i = 0; i < CNVTBLSIZ; i++) {
		if (w < convtable[i].cnv
		|| w >= convtable[i].cnv + convtable[i].range)
			continue;
		w -= convtable[i].cnv;
		w += convtable[i].start;
		s1 = ((w >> 8) & 0xff);
		s2 = (w & 0xff);
		break;
	}

	buf[0] = s1;
	buf[1] = s2;
}

int sjis2ujis(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (iswsjis((CONST char *)s, i)) {
			if (j + 2 > max) break;
			sj2j(&(buf[j]), &(s[i++]));
			buf[j++] |= 0x80;
			buf[j] |= 0x80;
		}
		else if (isskana(s, i)) {
			if (j + 2 > max) break;
			buf[j++] = (char)C_EKANA;
			buf[j] = s[i];
		}
		else buf[j] = s[i];
	}

	return(j);
}

int ujis2sjis(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (isweuc((CONST char *)s, i)) {
			if (j + 2 > max) break;
			j2sj(&(buf[j++]), &(s[i++]));
		}
		else if (isekana(s, i)) buf[j] = s[++i];
		else buf[j] = s[i];
	}
	if (kanjierrno) kanjierrno = EUC;

	return(j);
}
#endif	/* !_NOKANJICONV || (DEP_DOSEMU && CODEEUC) */

#ifdef	DEP_UNICODE
# ifndef	DEP_EMBEDUNITBL
static int NEAR openunitbl(file)
CONST char *file;
{
	static int fd = -2;
	u_char buf[2];
	char path[MAXPATHLEN];

	if (!file) {
		if (fd >= 0) VOID_C Xclose(fd);
		fd = -2;
		return(0);
	}

	if (fd >= -1) return(fd);

	if (!unitblpath || !*unitblpath) Xstrcpy(path, file);
	else strcatdelim2(path, unitblpath, file);

#  ifdef	FD
	noconv++;
#  endif
	fd = -1;
	if ((fd = Xopen(path, O_BINARY | O_RDONLY, 0666)) < 0) fd = -1;
	else if (unitblent) /*EMPTY*/;
	else if (sureread(fd, buf, 2) == 2) unitblent = getword(buf, 0);
	else {
		VOID_C Xclose(fd);
		fd = -1;
	}
#  ifdef	FD
	noconv--;
#  endif

	return(fd);
}

static u_char *NEAR newunitbl(size)
ALLOC_T size;
{
	u_char *tbl;

	if ((tbl = (u_char *)malloc(size))) return(tbl);
	VOID_C openunitbl(NULL);
	if (unicodebuffer) unicodebuffer = 0;

	return(NULL);
}

/*ARGSUSED*/
VOID readunitable(nf)
int nf;
{
#  ifndef	_NOKANJICONV
	u_char buf[2], **tblbuf;
	u_int *tblent;
	int i;
#  endif
	u_char *tbl;
	ALLOC_T size;
	int fd;

	if (!unitblbuf) /*EMPTY*/;
#  ifndef	_NOKANJICONV
	else if (nf && !nftblbuf) /*EMPTY*/;
#  endif
	else return;

	if ((fd = openunitbl(UNICODETBL)) < 0) return;
	size = (ALLOC_T)unitblent * 4;

	if (!unitblbuf) {
		if (!(tbl = newunitbl(size))) return;
		if (!skread(fd, (off_t)2, tbl, size)) {
			free(tbl);
			VOID_C openunitbl(NULL);
			return;
		}
		unitblbuf = tbl;
	}

#  ifndef	_NOKANJICONV
	if (nf && !nftblbuf) {
		if (!skread(fd, (off_t)size + 2, buf, 2)) {
			VOID_C openunitbl(NULL);
			return;
		}
		nftblnum = buf[0];
		nflen = buf[1];
		tblbuf = (u_char **)malloc(nftblnum * sizeof(u_char *));
		if (!tblbuf) {
			VOID_C openunitbl(NULL);
			return;
		}
		tblent = (u_int *)malloc(nftblnum * sizeof(u_int));
		if (!tblent) {
			free(tblbuf);
			VOID_C openunitbl(NULL);
			return;
		}

		for (i = 0; i < nftblnum; i++) {
			if (sureread(fd, buf, 2) != 2) {
				while (i > 0) free(tblbuf[--i]);
				free(tblbuf);
				free(tblent);
				VOID_C openunitbl(NULL);
				return;
			}
			tblent[i] = getword(buf, 0);
			size = (ALLOC_T)tblent[i] * (2 + nflen * 2);

			if (!(tblbuf[i] = newunitbl(size))) {
				while (i > 0) free(tblbuf[--i]);
				free(tblbuf);
				free(tblent);
				return;
			}
			if (sureread(fd, tblbuf[i], size) != size) {
				while (i >= 0) free(tblbuf[i--]);
				free(tblbuf);
				free(tblent);
				VOID_C openunitbl(NULL);
				return;
			}
		}
		nftblbuf = tblbuf;
		nftblent = tblent;
	}
#  endif	/* !_NOKANJICONV */

	VOID_C openunitbl(NULL);
}

VOID discardunitable(VOID_A)
{
#  ifndef	_NOKANJICONV
	int i;
#  endif

	if (unitblbuf) free(unitblbuf);
	unitblbuf = NULL;
#  ifndef	_NOKANJICONV
	if (nftblbuf) {
		for (i = 0; i < nftblnum; i++) free(nftblbuf[i]);
		free(nftblbuf);
	}
	nftblbuf = NULL;
	if (nftblent) free(nftblent);
	nftblent = NULL;
#  endif	/* !_NOKANJICONV */
}
# endif	/* !DEP_EMBEDUNITBL */

u_int unifysjis(wc, russ)
u_int wc;
int russ;
{
	int n, min, max;

	wc &= 0xffff;
	min = ((russ) ? 0 : EXCEPTRUSS) - 1;
	max = RSJISTBLSIZ;
	for (;;) {
		n = (min + max) / 2;
		if (n <= min || n >= max) break;
		if (wc >= rsjistable[n].start + rsjistable[n].range)
			min = n;
		else if (wc < rsjistable[n].start) max = n;
		else {
			wc -= rsjistable[n].start;
			wc += rsjistable[n].cnv;
			break;
		}
	}

	return(wc);
}

u_int cnvunicode(wc, encode)
u_int wc;
int encode;
{
# ifndef	DEP_EMBEDUNITBL
	int fd;
# endif
	CONST u_char *cp;
	u_char buf[4];
	u_int r, w, ofs, min, max;

	wc &= 0xffff;
	if (encode < 0) {
# ifndef	DEP_EMBEDUNITBL
		VOID_C openunitbl(NULL);
		if (!unicodebuffer) discardunitable();
# endif
		return(0);
	}

	if (encode) {
		r = U2_UDEF;
		if (wc < 0x0080) return(wc);
		if (wc >= 0x00a1 && wc <= 0x00df)
			return(0xff00 | (wc - 0x00a1 + 0x61));
		if (wc >= 0x8260 && wc <= 0x8279)
			return(0xff00 | (wc - 0x8260 + 0x21));
		if (wc >= 0x8281 && wc <= 0x829a)
			return(0xff00 | (wc - 0x8281 + 0x41));
		if (wc < MINKANJI || wc > MAXKANJI) {
			kanjierrno = SJIS;
			return(r);
		}
		wc = unifysjis(wc, 0);
	}
	else {
		r = SJ_UDEF;
		switch (wc & 0xff00) {
			case 0:
				if ((wc & 0xff) < 0x80) return(wc);
				break;
			case 0xff00:
				w = (wc & 0xff);
				if (w >= 0x21 && w <= 0x3a)
					return(w + 0x8260 - 0x21);
				if (w >= 0x41 && w <= 0x5a)
					return(w + 0x8281 - 0x41);
				if (w >= 0x61 && w <= 0x9f)
					return(w + 0x00a1 - 0x61);
				break;
			default:
				break;
		}
		if (wc < MINUNICODE || wc > MAXUNICODE) {
			kanjierrno = UTF8;
			return(r);
		}
		if (wc == 0x3099) return(0x814a);
		if (wc == 0x309a) return(0x814b);
	}

# ifndef	DEP_EMBEDUNITBL
	if (unicodebuffer && !unitblbuf) readunitable(0);
# endif
	cp = buf;
	ofs = min = max = 0;
# ifndef	DEP_EMBEDUNITBL
	if (unitblbuf)
# endif
	{
		if (encode) {
			cp = unitblbuf;
			for (ofs = 0; ofs < unitblent; ofs++) {
				w = getword(cp, 2);
				if (wc == w) break;
				cp += 4;
			}
		}
		else {
			min = 0;
			max = unitblent + 1;
			for (;;) {
				ofs = (min + max) / 2;
				if (ofs <= min || ofs >= max) break;
				cp = &(unitblbuf[(ofs - 1) * 4]);
				w = getword(cp, 0);
				if (wc > w) min = ofs;
				else if (wc < w) max = ofs;
				else break;
			}
		}
	}
# ifndef	DEP_EMBEDUNITBL
	else if ((fd = openunitbl(UNICODETBL)) < 0) ofs = unitblent;
	else if (encode) {
		if (Xlseek(fd, (off_t)2, L_SET) < (off_t)0) ofs = unitblent;
		else for (ofs = 0; ofs < unitblent; ofs++) {
			if (sureread(fd, buf, 4) != 4) {
				ofs = unitblent;
				break;
			}
			w = getword(cp, 2);
			if (wc == w) break;
		}
	}
	else {
		min = 0;
		max = unitblent + 1;
		for (;;) {
			ofs = (min + max) / 2;
			if (ofs <= min || ofs >= max) break;
			if (!skread(fd, (off_t)(ofs - 1) * 4 + 2, buf, 4)) {
				ofs = min = max = 0;
				break;
			}
			w = getword(cp, 0);
			if (wc > w) min = ofs;
			else if (wc < w) max = ofs;
			else break;
		}
	}
# endif	/* !DEP_EMBEDUNITBL */

	if (encode) {
		if (ofs < unitblent) r = getword(cp, 0);
		else kanjierrno = SJIS;
	}
	else {
		if (ofs > min && ofs < max) r = getword(cp, 2);
		else kanjierrno = UTF8;
	}

	return(r);
}
#endif	/* DEP_UNICODE */

#ifndef	_NOKANJICONV
# if	defined (DEP_UNICODE) && !defined (DEP_EMBEDUNITBL)
static int NEAR opennftbl(file, nf, entp)
CONST char *file;
int nf;
u_int *entp;
{
	u_char buf[2];
	off_t ofs;
	int i, fd;

	if ((fd = openunitbl(file)) < 0) return(-1);
	ofs = (off_t)unitblent * 4 + 2;

	if (!nftblnum) {
		if (!skread(fd, ofs, buf, 2)) return(-1);
		nftblnum = buf[0];
		nflen = buf[1];
	}

	if (nf > nftblnum) return(-1);

	*entp = 0;
	for (i = 0; i < nf; i++) {
		ofs += 2 + (off_t)*entp * (2 + nflen * 2);
		if (!skread(fd, ofs, buf, 2)) return(-1);
		*entp = getword(buf, 0);
	}

	return(fd);
}
# endif	/* DEP_UNICODE && !DEP_EMBEDUNITBL */

static int NEAR toenglish(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (iskanji1((char *)s, i)) {
			if (j + 2 > max) break;
			i++;
			buf[j++] = '?';
			buf[j] = '?';
		}
		else if (iskana1((char *)s, &i)) buf[j] = '?';
		else buf[j] = s[i];
	}

	return(j);
}

static int NEAR tojis7(buf, s, max, knj, asc, io)
char *buf;
CONST u_char *s;
int max, knj, asc, io;
{
	int i, j, len, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (iskanji1((char *)s, i)) {
			len = 5;
			if (mode & KANA) len++;
			if (!(mode & KANJI)) len += 3;
			if (j + len > max) break;
			if (mode & KANA) buf[j++] = '\017';
			mode &= ~KANA;
			if (!(mode & KANJI)) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = knj;
			}
			mode |= KANJI;
# ifdef	CODEEUC
			buf[j++] = (s[i++] & ~0x80);
			buf[j] = (s[i] & ~0x80);
# else
			sj2j(&(buf[j++]), &(s[i++]));
# endif
			buf[j] = jcnv(buf[j], io);
		}
		else if (iskana1((char *)s, &i)) {
			len = 2;
			if (!(mode & KANA)) len++;
			if (mode & KANJI) len += 3;
			if (j + len > max) break;
			if (!(mode & KANA)) buf[j++] = '\016';
			mode |= KANA;
			buf[j] = (s[i] & ~0x80);
			buf[j] = jcnv(buf[j], io);
		}
		else {
			len = 1;
			if (mode & KANA) len++;
			if (mode & KANJI) len += 3;
			if (j + len > max) break;
			if (mode & KANA) buf[j++] = '\017';
			mode &= ~KANA;
			if (mode & KANJI) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = asc;
			}
			mode &= ~KANJI;
			buf[j] = s[i];
		}
	}
	if (mode & KANA) buf[j++] = '\017';
	if (mode & KANJI) {
		buf[j++] = '\033';
		buf[j++] = '(';
		buf[j++] = asc;
	}

	return(j);
}

static int NEAR fromjis(buf, s, max, io)
char *buf;
CONST u_char *s;
int max, io;
{
# ifndef	CODEEUC
	u_char tmp[MAXKLEN];
# endif
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max; i++)
	switch (s[i]) {
		case '\016':	/* SO */
			mode |= KANA;
			break;
		case '\017':	/* SI */
			mode &= ~KANA;
			break;
		case '\033':	/* ESC */
			if (s[i + 1] == '$') {
				if (s[i + 2] == '@' || s[i + 2] == 'B') {
					mode &= ~JKANA;
					mode |= KANJI;
					i += 2;
					break;
				}
				else if (s[i + 2] == '('
				&& (s[i + 3] == '@' || s[i + 3] == 'B'
				|| s[i + 3] == 'O')) {
					mode &= ~JKANA;
					mode |= KANJI;
					i += 3;
					break;
				}
			}
			else if (s[i + 1] == '(') {
				if (s[i + 2] == 'J' || s[i + 2] == 'B'
				|| s[i + 2] == 'H') {
					mode &= ~(KANJI | JKANA);
					i += 2;
					break;
				}
				else if (s[i + 2] == 'I') {
					mode &= ~KANJI;
					mode |= JKANA;
					i += 2;
					break;
				}
			}
			else if (s[i + 1] == '&') {
				if (s[i + 2] == '@' && s[i + 3] == '\033'
				&& s[i + 4] == '$' && s[i + 5] == 'B') {
					mode &= ~JKANA;
					mode |= KANJI;
					i += 5;
					break;
				}
			}
/*FALLTHRU*/
		default:
			if (mode & (KANA | JKANA)) {
				if (!isjkana(s, i)) buf[j++] = s[i];
				else {
# ifdef	CODEEUC
					if (j + 2 > max) break;
					buf[j++] = (char)C_EKANA;
# endif
					buf[j++] = (jdecnv(s[i], io) | 0x80);
				}
			}
			else if (mode & KANJI) {
				if (j + 2 > max) break;
				if (!isjis(s[i]) || !isjis(s[i + 1])) {
					buf[j++] = s[i++];
					buf[j++] = s[i];
				}
				else {
# ifdef	CODEEUC
					buf[j++] = (s[i++] | 0x80);
					buf[j++] = (jdecnv(s[i], io) | 0x80);
# else
					tmp[0] = s[i++];
					tmp[1] = jdecnv(s[i], io);
					j2sj(&(buf[j]), tmp);
					j += 2;
# endif
				}
			}
# ifdef	CODEEUC
			else if (Xiskana(s[i])) {
				if (j + 2 > max) break;
				buf[j++] = (char)C_EKANA;
				buf[j++] = s[i];
			}
# endif
			else buf[j++] = s[i];
			break;
	}

	return(j);
}

static int NEAR tojis8(buf, s, max, knj, asc, io)
char *buf;
CONST u_char *s;
int max, knj, asc, io;
{
	int i, j, len, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (iskanji1((char *)s, i)) {
			len = 5;
			if (mode != KANJI) len += 3;
			if (j + len > max) break;
			if (mode != KANJI) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = knj;
			}
			mode = KANJI;
# ifdef	CODEEUC
			buf[j++] = (s[i++] & ~0x80);
			buf[j] = (s[i] & ~0x80);
# else
			sj2j(&(buf[j++]), &(s[i++]));
# endif
			buf[j] = jcnv(buf[j], io);
		}
		else {
			len = 1;
			if (mode == KANJI) len += 3;
			if (j + len > max) break;
			if (mode == KANJI) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = asc;
			}
			mode = ASCII;
# ifdef	CODEEUC
			VOID_C iskana1((char *)s, &i);
# endif
			buf[j] = s[i];
		}
	}
	if (mode == KANJI) {
		buf[j++] = '\033';
		buf[j++] = '(';
		buf[j++] = asc;
	}

	return(j);
}

static int NEAR tojunet(buf, s, max, knj, asc, io)
char *buf;
CONST u_char *s;
int max, knj, asc, io;
{
	int i, j, len, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (iskanji1((char *)s, i)) {
			len = 5;
			if (mode != KANJI) len += 3;
			if (j + len > max) break;
			if (mode != KANJI) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = knj;
			}
			mode = KANJI;
# ifdef	CODEEUC
			buf[j++] = (s[i++] & ~0x80);
			buf[j] = (s[i] & ~0x80);
# else
			sj2j(&(buf[j++]), &(s[i++]));
# endif
			buf[j] = jcnv(buf[j], io);
		}
		else if (iskana1((char *)s, &i)) {
			len = 4;
			if (mode != JKANA) len += 3;
			if (j + len > max) break;
			if (mode != JKANA) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = 'I';
			}
			mode = JKANA;
			buf[j] = (s[i] & ~0x80);
			buf[j] = jcnv(buf[j], io);
		}
		else {
			len = 1;
			if (mode != ASCII) len += 3;
			if (j + len > max) break;
			if (mode != ASCII) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = asc;
			}
			mode = ASCII;
			buf[j] = s[i];
		}
	}
	if (mode != ASCII) {
		buf[j++] = '\033';
		buf[j++] = '(';
		buf[j++] = asc;
	}

	return(j);
}

# ifdef	DEP_UNICODE
VOID ucs2normalization(buf, ptrp, max, wc, nf)
u_short *buf;
int *ptrp, max;
u_int wc;
int nf;
{
# ifndef	DEP_EMBEDUNITBL
	int fd;
# endif
	CONST u_char *cp;
	u_char *new;
	u_int w, ofs, ent;
	int n;

	cp = new = NULL;
	ofs = ent = (u_int)0;
# ifndef	DEP_EMBEDUNITBL
	if (unicodebuffer && !nftblbuf) readunitable(1);
# endif

	if (wc < MINUNICODE || wc > MAXUNICODE) /*EMPTY*/;
# ifdef	DEP_EMBEDUNITBL
	else if (nf <= nftblnum)
# else
	else if (nftblbuf && nf <= nftblnum)
# endif
	{
		n = 2 + nflen * 2;
		cp = nftblbuf[nf - 1];
		ent = nftblent[nf - 1];
		for (ofs = 0; ofs < ent; ofs++) {
			w = getword(cp, 0);
			if (wc == w) break;
			cp += n;
		}
	}
# ifndef	DEP_EMBEDUNITBL
	else if ((fd = opennftbl(UNICODETBL, nf, &ent)) < 0) ent = (u_int)0;
	else {
		n = 2 + nflen * 2;
		cp = new = (u_char *)malloc(n);
		if (!new) ent = (u_int)0;
		else for (ofs = 0; ofs < ent; ofs++) {
			if (sureread(fd, new, n) != n) {
				ofs = ent;
				break;
			}
			w = getword(cp, 0);
			if (wc == w) break;
		}
	}
# endif	/* !DEP_EMBEDUNITBL */

	if (ofs >= ent) buf[(*ptrp)++] = wc;
	else for (n = 0; n < nflen && *ptrp < max; n++) {
		cp += 2;
		w = getword(cp, 0);
		if (!w) break;
		buf[(*ptrp)++] = w;
	}
	if (new) free(new);
}

u_int ucs2denormalization(buf, ptrp, nf)
CONST u_short *buf;
int *ptrp, nf;
{
# ifndef	DEP_EMBEDUNITBL
	off_t top;
	u_int ent;
	int fd;
# endif
	CONST u_char *cp;
	u_char *new;
	u_int w, ofs, min, max;
	int i, j, n;

	cp = new = NULL;
	ofs = min = max = (u_int)0;
	i = 0;
# ifndef	DEP_EMBEDUNITBL
	if (unicodebuffer && !nftblbuf) readunitable(1);
# endif

	if (buf[*ptrp] < MINUNICODE || buf[*ptrp] > MAXUNICODE) /*EMPTY*/;
# ifdef	DEP_EMBEDUNITBL
	else if (nf <= nftblnum)
# else
	else if (nftblbuf && nf <= nftblnum)
# endif
	{
		n = 2 + nflen * 2;
		max = nftblent[nf - 1] + 1;
		for (;;) {
			ofs = (min + max) / 2;
			if (ofs <= min || ofs >= max) break;
			cp = &(nftblbuf[nf - 1][(ofs - 1) * n]);
			w = 0xffff;
			for (i = 0, j = 2; i < nflen; i++, j += 2) {
				if (!(w = getword(cp, j))) /*EMPTY*/;
				else if (buf[*ptrp + i] > w) min = ofs;
				else if (buf[*ptrp + i] < w) max = ofs;
				else continue;
				break;
			}
			if (!w) break;
		}
	}
# ifndef	DEP_EMBEDUNITBL
	else if ((fd = opennftbl(UNICODETBL, nf, &ent)) < 0
	|| (top = Xlseek(fd, (off_t)0, L_INCR)) < (off_t)0)
		/*EMPTY*/;
	else {
		n = 2 + nflen * 2;
		max = ent + 1;
		cp = new = (u_char *)malloc(n);
		if (!new) max = (u_int)0;
		else for (;;) {
			ofs = (min + max) / 2;
			if (ofs <= min || ofs >= max) break;
			if (!skread(fd, (off_t)(ofs - 1) * n + top, new, n)) {
				ofs = min = max = (u_int)0;
				break;
			}
			w = 0xffff;
			for (i = 0, j = 2; i < nflen; i++, j += 2) {
				if (!(w = getword(cp, j))) /*EMPTY*/;
				else if (buf[*ptrp + i] > w) min = ofs;
				else if (buf[*ptrp + i] < w) max = ofs;
				else continue;
				break;
			}
			if (!w) break;
		}
	}
# endif	/* !DEP_EMBEDUNITBL */

	if (ofs <= min || ofs >= max) w = buf[(*ptrp)++];
	else {
		w = getword(cp, 0);
		*ptrp += i;
	}
	if (new) free(new);

	return(w);
}

static u_int NEAR toucs2(s, ptrp)
CONST u_char *s;
int *ptrp;
{
# ifdef	CODEEUC
	u_char tmp[MAXKLEN];
# endif
	u_int w;

	if (iskanji1((char *)s, *ptrp)) {
# ifdef	CODEEUC
		j2sj((char *)tmp, &(s[*ptrp]));
		if (kanjierrno) kanjierrno = EUC;
		*ptrp += 2;
		w = (((u_int)(tmp[0]) << 8) | tmp[1]);
# else
		w = (((u_int)(s[*ptrp]) << 8) | s[*ptrp + 1]);
		*ptrp += 2;
# endif
	}
	else {
		VOID_C iskana1((char *)s, ptrp);
		w = s[(*ptrp)++];
	}

	return(cnvunicode(w, 1));
}

static VOID NEAR fromucs2(buf, ptrp, wc)
char *buf;
int *ptrp;
u_int wc;
{
# ifdef	CODEEUC
	u_char tmp[MAXKLEN];
# endif
	int c1, c2;

	wc = cnvunicode(wc, 0);
	c1 = ((wc >> 8) & 0xff);
	c2 = (wc & 0xff);
	if (wc > 0xa0 && wc <= 0xdf) {
# ifdef	CODEEUC
		buf[(*ptrp)++] = (char)C_EKANA;
# endif
		buf[(*ptrp)++] = wc;
	}
	else if (issjis1(c1) && issjis2(c2)) {
# ifdef	CODEEUC
		tmp[0] = c1;
		tmp[1] = c2;
		sj2j(&(buf[*ptrp]), tmp);
		buf[(*ptrp)++] |= 0x80;
		buf[(*ptrp)++] |= 0x80;
# else
		buf[(*ptrp)++] = c1;
		buf[(*ptrp)++] = c2;
# endif
	}
	else buf[(*ptrp)++] = wc;
}

int ucs2toutf8(buf, ptr, wc)
char *buf;
int ptr;
u_int wc;
{
	if (wc < 0x80) buf[ptr++] = wc;
	else if (wc < 0x800) {
		buf[ptr++] = (0xc0 | (wc >> 6));
		buf[ptr++] = (0x80 | (wc & 0x3f));
	}
	else {
		buf[ptr++] = (0xe0 | (wc >> 12));
		buf[ptr++] = (0x80 | ((wc >> 6) & 0x3f));
		buf[ptr++] = (0x80 | (wc & 0x3f));
	}

	return(ptr);
}

u_int ucs2fromutf8(s, ptrp)
CONST u_char *s;
int *ptrp;
{
	u_int w;
	int ptr;

	ptr = (ptrp) ? *ptrp : 0;
	w = s[ptr++];
	if (w < 0x80) /*EMPTY*/;
	else if (isutf2(w, s[ptr]))
		w = (((w & 0x1f) << 6) | (s[ptr++] & 0x3f));
	else if (isutf3(w, s[ptr], s[ptr + 1])) {
		w = (((w & 0x0f) << 6) | (s[ptr++] & 0x3f));
		w = ((w << 6) | (s[ptr++] & 0x3f));
	}
	else {
		w = U2_UDEF;
		kanjierrno = UTF8;
	}
	if (ptrp) *ptrp = ptr;

	return(w);
}

static int NEAR toutf8(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
	u_int c;
	int i, j, len;

	i = j = 0;
	while (s[i] && j < max) {
		c = toucs2(s, &i);
		len = 1;
		if (c >= 0x80) len++;
		if (c >= 0x800) len++;
		if (j + len > max) break;
		j = ucs2toutf8(buf, j, c);
	}
	cnvunicode(0, -1);
	if (kanjierrno) kanjierrno = DEFCODE;

	return(j);
}

static int NEAR fromutf8(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
	int i, j;

	i = j = 0;
	while (s[i] && j + 1 < max) fromucs2(buf, &j, ucs2fromutf8(s, &i));
	cnvunicode(0, -1);

	return(j);
}

static int NEAR toutf8nf(buf, s, max, nf)
char *buf;
CONST u_char *s;
int max, nf;
{
	u_short *u1, *u2;
	int i, j, len;

	if (!(u1 = (u_short *)malloc((max + 1) * sizeof(u_short))))
		return(toutf8(buf, s, max));
	if (!(u2 = (u_short *)malloc((max + 1) * sizeof(u_short)))) {
		free(u1);
		return(toutf8(buf, s, max));
	}

	for (i = j = 0; s[i] && j < max; j++) u1[j] = toucs2(s, &i);
	u1[j] = (u_short)0;
	for (i = j = 0; u1[i] && j < max; i++)
		ucs2normalization(u2, &j, max, u1[i], nf);
	u2[j] = (u_short)0;
	for (i = j = 0; u2[i] && j < max; i++) {
		len = 1;
		if (u2[i] >= 0x80) len++;
		if (u2[i] >= 0x800) len++;
		if (j + len > max) break;
		j = ucs2toutf8(buf, j, u2[i]);
	}

	free(u1);
	free(u2);
	cnvunicode(0, -1);
	if (kanjierrno) kanjierrno = DEFCODE;

	return(j);
}

static int NEAR fromutf8nf(buf, s, max, nf)
char *buf;
CONST u_char *s;
int max, nf;
{
	u_short *u1, *u2;
	int i, j;

	if (!(u1 = (u_short *)malloc((max + 1) * sizeof(u_short))))
		return(fromutf8(buf, s, max));
	if (!(u2 = (u_short *)malloc((max + 1) * sizeof(u_short)))) {
		free(u1);
		return(fromutf8(buf, s, max));
	}

	for (i = j = 0; s[i] && j < max; j++) u1[j] = ucs2fromutf8(s, &i);
	u1[j] = (u_short)0;
	for (i = j = 0; u1[i] && j < max; j++)
		u2[j] = ucs2denormalization(u1, &i, nf);
	u2[j] = (u_short)0;
	for (i = j = 0; u2[i] && j + 1 < max; i++) fromucs2(buf, &j, u2[i]);

	free(u1);
	free(u2);
	cnvunicode(0, -1);

	return(j);
}
# endif	/* DEP_UNICODE */

static int NEAR bin2hex(buf, c)
char *buf;
int c;
{
	int i;

	i = 0;
	buf[i++] = ':';
	buf[i++] = tohexa((c >> 4) & 0xf);
	buf[i++] = tohexa(c & 0xf);

	return(i);
}

static int NEAR tohex(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
# ifdef	CODEEUC
	u_char tmp[MAXKLEN];
# endif
	int i, j;

	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (iskanji1((char *)s, i)) {
			if (j + 6 > max) break;
# ifdef	CODEEUC
			j2sj((char *)tmp, &(s[i++]));
			j += bin2hex(&(buf[j]), tmp[0]);
			j += bin2hex(&(buf[j]), tmp[1]) - 1;
# else
			j += bin2hex(&(buf[j]), s[i++]);
			j += bin2hex(&(buf[j]), s[i]) - 1;
# endif
		}
		else if (iskana1((char *)s, &i)) {
			if (j + 3 > max) break;
			j += bin2hex(&(buf[j]), s[i]) - 1;
		}
		else buf[j] = s[i];
	}
	if (kanjierrno) kanjierrno = DEFCODE;

	return(j);
}

static int NEAR fromhex(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
# ifdef	CODEEUC
	u_char tmp[MAXKLEN];
# endif
	int i, j, c1, c2;

	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (!ishex(s, i)) buf[j] = s[i];
		else {
			c1 = tobin2(s, i + 1);
			i += 2;
			if (Xiskana(c1)) {
# ifdef	CODEEUC
				if (j + 2 > max) break;
				buf[j++] = (char)C_EKANA;
# endif
				buf[j] = c1;
			}
			else if (issjis1(c1)) {
				if (j + 2 > max) break;
				i++;
				if (!ishex(s, i)) c2 = s[i];
				else {
					c2 = tobin2(s, i + 1);
					i += 2;
				}
				if (!issjis2(c2)) {
					buf[j++] = c1;
					buf[j] = c2;
				}
				else {
# ifdef	CODEEUC
					tmp[0] = c1;
					tmp[1] = c2;
					sj2j(&(buf[j]), tmp);
					buf[j++] |= 0x80;
					buf[j] |= 0x80;
# else
					buf[j++] = c1;
					buf[j] = c2;
# endif
				}
			}
			else buf[j] = c1;
		}
	}

	return(j);
}

static int NEAR bin2cap(buf, c)
char *buf;
int c;
{
	int i;

	i = 0;
	if (c < 0x80) buf[i++] = c;
	else {
		buf[i++] = ':';
		buf[i++] = tohexa((c >> 4) & 0xf);
		buf[i++] = tohexa(c & 0xf);
	}

	return(i);
}

static int NEAR tocap(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
# ifdef	CODEEUC
	u_char tmp[MAXKLEN];
# endif
	int i, j, len;

	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (iskanji1((char *)s, i)) {
			len = 4;
# ifdef	CODEEUC
			j2sj((char *)tmp, &(s[i++]));
			if (ismsb(tmp[1])) len += 2;
			if (j + len > max) break;
			j += bin2cap(&(buf[j]), tmp[0]);
			j += bin2cap(&(buf[j]), tmp[1]) - 1;
# else
			if (ismsb(s[i + 1])) len += 2;
			if (j + len > max) break;
			j += bin2cap(&(buf[j]), s[i++]);
			j += bin2cap(&(buf[j]), s[i]) - 1;
# endif
		}
		else if (iskana1((char *)s, &i)) {
			if (j + 3 > max) break;
			j += bin2cap(&(buf[j]), s[i]) - 1;
		}
		else buf[j] = s[i];
	}
	if (kanjierrno) kanjierrno = DEFCODE;

	return(j);
}

static int NEAR fromcap(buf, s, max)
char *buf;
CONST u_char *s;
int max;
{
# ifdef	CODEEUC
	u_char tmp[MAXKLEN];
# endif
	int i, j, c1, c2;

	for (i = j = 0; s[i] && j < max; i++, j++) {
		if (!iscap(s, i)) buf[j] = s[i];
		else {
			c1 = tobin2(s, i + 1);
			i += 2;
			if (Xiskana(c1)) {
# ifdef	CODEEUC
				if (j + 2 > max) break;
				buf[j++] = (char)C_EKANA;
# endif
				buf[j] = c1;
			}
			else if (issjis1(c1)) {
				if (j + 2 > max) break;
				i++;
				if (!iscap(s, i)) c2 = s[i];
				else {
					c2 = tobin2(s, i + 1);
					i += 2;
				}
				if (!issjis2(c2)) {
					buf[j++] = c1;
					buf[j] = c2;
				}
				else {
# ifdef	CODEEUC
					tmp[0] = c1;
					tmp[1] = c2;
					sj2j(&(buf[j]), tmp);
					buf[j++] |= 0x80;
					buf[j] |= 0x80;
# else
					buf[j++] = c1;
					buf[j] = c2;
# endif
				}
			}
			else buf[j] = c1;
		}
	}

	return(j);
}

static CONST char *NEAR _kanjiconv(buf, s, max, in, out, lenp, io)
char *buf;
CONST char *s;
int max, in, out, *lenp, io;
{
	kanjierrno = 0;

	if (in == out || in == NOCNV || out == NOCNV) return(s);
	switch (out) {
		case ENG:
			*lenp = toenglish(buf, (u_char *)s, max);
			break;
		case JIS7:
			*lenp = tojis7(buf, (u_char *)s, max, 'B', 'B', io);
			break;
		case O_JIS7:
			*lenp = tojis7(buf, (u_char *)s, max, '@', 'J', io);
			break;
		case JIS8:
			*lenp = tojis8(buf, (u_char *)s, max, 'B', 'B', io);
			break;
		case O_JIS8:
			*lenp = tojis8(buf, (u_char *)s, max, '@', 'J', io);
			break;
		case JUNET:
			*lenp = tojunet(buf, (u_char *)s, max, 'B', 'B', io);
			break;
		case O_JUNET:
			*lenp = tojunet(buf, (u_char *)s, max, '@', 'J', io);
			break;
# ifdef	DEP_UNICODE
		case UTF8:
			*lenp = toutf8(buf, (u_char *)s, max);
			break;
		case M_UTF8:
		case I_UTF8:
			*lenp = toutf8nf(buf, (u_char *)s, max, out - UTF8);
			break;
# endif
		case HEX:
			*lenp = tohex(buf, (u_char *)s, max);
			break;
		case CAP:
			*lenp = tocap(buf, (u_char *)s, max);
			break;
		case SECCODE:
			*lenp = kencode(buf, (u_char *)s, max);
			break;
		case DEFCODE:
			switch (in) {
				case JIS7:
				case O_JIS7:
				case JIS8:
				case O_JIS8:
				case JUNET:
				case O_JUNET:
					*lenp = fromjis(buf, (u_char *)s,
						max, io);
					break;
# ifdef	DEP_UNICODE
				case UTF8:
					*lenp = fromutf8(buf, (u_char *)s,
						max);
					break;
				case M_UTF8:
				case I_UTF8:
					*lenp = fromutf8nf(buf, (u_char *)s,
						max, in - UTF8);
					break;
# endif
				case HEX:
					*lenp = fromhex(buf, (u_char *)s, max);
					break;
				case CAP:
					*lenp = fromcap(buf, (u_char *)s, max);
					break;
				case SECCODE:
					*lenp = kdecode(buf, (u_char *)s, max);
					break;
				default:
					return(s);
/*NOTREACHED*/
					break;
			}
			break;
		default:
			return(s);
/*NOTREACHED*/
			break;
	}
	if (io == L_FNAME && kanjierrno) return(s);

	return(buf);
}

int kanjiconv(buf, s, max, in, out, io)
char *buf;
CONST char *s;
int max, in, out, io;
{
	int len;

	if (_kanjiconv(buf, s, max, in, out, &len, io) != buf)
		for (len = 0; s[len]; len++) buf[len] = s[len];
	buf[len] = '\0';

	return(len);
}

CONST char *kanjiconv2(buf, s, max, in, out, io)
char *buf;
CONST char *s;
int max, in, out, io;
{
	int len;

	if (_kanjiconv(buf, s, max, in, out, &len, io) != buf) return(s);
	buf[len] = '\0';

	return(buf);
}

char *newkanjiconv(s, in, out, io)
CONST char *s;
int in, out, io;
{
	char *buf;
	int len;

	if (!s) return(NULL);
	len = strlen(s) * 3 + 3;
	if (!(buf = malloc(len + 1))) return((char *)s);
	if (kanjiconv2(buf, s, len, in, out, io) != buf) {
		free(buf);
		return((char *)s);
	}

	return(buf);
}

VOID renewkanjiconv(sp, in, out, io)
char **sp;
int in, out, io;
{
	char *buf;

	buf = newkanjiconv(*sp, in, out, io);
	if (buf != *sp) {
		free(*sp);
		*sp = buf;
	}
}
#endif	/* !_NOKANJICONV */

#ifdef	DEP_FILECONV
int getkcode(path)
CONST char *path;
{
	int i;

# ifdef	DEP_DOSEMU
	if (_dospath(path)) return(SJIS);
# endif
	for (i = 0; i < KPATHLISTSIZ; i++) {
		if (includepath(path, *(kpathlist[i].path)))
			return(kpathlist[i].code);
	}

	return(fnamekcode);
}
#endif	/* DEP_FILECONV */

#ifndef	_NOKANJICONV
int getoutputkcode(VOID_A)
{
# ifdef	DEP_PTY
	if (parentfd >= 0 && ptyoutkcode != NOCNV) return(ptyoutkcode);
# endif
	return(outputkcode);
}
#endif	/* !_NOKANJICONV */

#ifdef	FD
/*ARGSUSED*/
char *convget(buf, path, drv)
char *buf, *path;
int drv;
{
# ifdef	DEP_ROCKRIDGE
	char rbuf[MAXPATHLEN];
# endif
# ifdef	DEP_FILECONV
	int c, fgetok;
# endif
	CONST char *cp;

# ifdef	DOUBLESLASH
	if (path[0] == _SC_ && path[1] == _SC_ && !isdslash(path))
		memmove(path, &(path[1]), strlen(&(path[1])) + 1);
# endif

	if (noconv) return(path);
# ifdef	DEP_FILECONV
	fgetok = (nokanjifconv) ? 0 : 1;
# endif

	cp = path;
# ifdef	DEP_ROCKRIDGE
	if (!norockridge) cp = transpath(cp, rbuf);
# endif
# ifdef	DEP_DOSEMU
	if (drv == DEV_DOS) {
#  ifdef	CODEEUC
		buf[sjis2ujis(buf, (u_char *)cp, MAXPATHLEN - 1)] = '\0';
		cp = buf;
#  endif
#  ifdef	DEP_FILECONV
		fgetok = 0;
#  endif
	}
# endif	/* DEP_DOSEMU */
# ifdef	DEP_FILECONV
	if (fgetok) {
#  ifdef	DEP_URLPATH
		if (drv == DEV_URL) c = urlkcode;
		else
#  endif
		c = getkcode(cp);
		cp = kanjiconv2(buf, cp, MAXPATHLEN - 1, c, DEFCODE, L_FNAME);
	}
# endif	/* DEP_FILECONV */
# ifdef	DEP_ROCKRIDGE
	if (cp == rbuf) {
		Xstrcpy(buf, rbuf);
		return(buf);
	}
# endif

	return((char *)cp);
}

/*ARGSUSED*/
char *convput(buf, path, needfile, rdlink, rrreal, codep)
char *buf;
CONST char *path;
int needfile, rdlink;
char *rrreal;
int *codep;
{
# if	defined (DEP_FILECONV) || defined (DEP_DOSEMU)
	char kbuf[MAXPATHLEN];
# endif
# ifdef	DEP_ROCKRIDGE
	char rbuf[MAXPATHLEN];
# endif
# ifdef	DEP_FILECONV
	int c, fputok;
# endif
# ifdef	DEP_URLPATH
	int url;
# endif
	CONST char *cp, *file;
	char *tmp, rpath[MAXPATHLEN];
	int n;

	if (rrreal) *rrreal = '\0';
	if (codep) *codep = NOCNV;

	if (noconv || isdotdir(path)) {
# ifdef	DEP_DOSEMU
		if (dospath(path, buf)) return(buf);
# endif
# ifdef	DEP_URLPATH
		if (urlpath(path, NULL, buf, NULL)) return(buf);
# endif
		return((char *)path);
	}
# ifdef	DEP_FILECONV
	fputok = (nokanjifconv) ? 0 : 1;
# endif

# ifdef	DEP_URLPATH
	url = _urlpath(path, NULL, NULL);
# endif
	if (needfile && strdelim(path, 0)) needfile = 0;
	if (norealpath) cp = path;
	else {
		if ((file = strrdelim(path, 0))) {
# ifdef	DOUBLESLASH
			if ((n = isdslash(path)) && file < &(path[n]))
				file = &(path[n]);
			else
# endif
# ifdef	DEP_URLPATH
			if (url && file < &(path[url]))
				file = &(path[n = url]);
			else
# endif
			{
				n = file - path;
				if (file++ == isrootdir(path)) n++;
			}
			Xstrncpy(rpath, path, n);
		}
# ifdef	DEP_DOSEMU
		else if ((n = _dospath(path))) {
			file = &(path[2]);
			VOID_C gendospath(rpath, n, '.');
		}
# endif
		else {
			file = path;
			copycurpath(rpath);
		}
		Xrealpath(rpath, rpath, RLP_READLINK);
		tmp = strcatdelim(rpath);
		Xstrncpy(tmp, file, MAXPATHLEN - 1 - (tmp - rpath));
		cp = rpath;
	}

# ifdef	DEP_DOSEMU
	if ((n = dospath(cp, kbuf))) {
		cp = kbuf;
		if (codep) *codep = SJIS;
#  ifdef	DEP_FILECONV
		fputok = 0;
#  endif
	}
# endif	/* DEP_DOSEMU */
# ifdef	DEP_FILECONV
	if (fputok) {
#  ifdef	DEP_URLPATH
		if (url) c = urlkcode;
		else
#  endif
		c = getkcode(cp);
		if (codep) *codep = c;
		cp = kanjiconv2(kbuf, cp, MAXPATHLEN - 1, DEFCODE, c, L_FNAME);
	}
# endif	/* DEP_FILECONV */
# ifdef	DEP_ROCKRIDGE
	if (!norockridge && (cp = detranspath(cp, rbuf)) == rbuf) {
		if (rrreal) Xstrcpy(rrreal, rbuf);
		if (rdlink && rrreadlink(cp, buf, MAXPATHLEN - 1) >= 0) {
			if (needfile && strdelim(buf, 0)) needfile = 0;
			if (*buf == _SC_ || !(tmp = strrdelim(rbuf, 0)))
				cp = buf;
			else {
				tmp++;
				Xstrncpy(tmp, buf, MAXPATHLEN - (tmp - rbuf));
				cp = rbuf;
			}
			Xrealpath(cp, rpath, RLP_READLINK);
			cp = detranspath(rpath, rbuf);
		}
	}
# endif
	if (cp == path) return((char *)path);
	if (needfile && (file = strrdelim(cp, 0))) file++;
	else file = cp;

# ifdef	DEP_DOSEMU
	if (Xisalpha(n) && !_dospath(file)) tmp = gendospath(buf, n, '\0');
	else
# endif
	tmp = buf;
	Xstrcpy(tmp, file);

	return(buf);
}
#endif	/* FD */
