/*
 *	kanji.c
 *
 *	Kanji Convert Function
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "machine.h"

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#include "term.h"
#include "kctype.h"
#include "pathname.h"

#if	MSDOS
#include <io.h>
#include "unixemu.h"
#else
#include <sys/file.h>
#include <sys/param.h>
#endif

#if	MSDOS && defined (_NOUSELFN) && !defined (_NODOSDRIVE)
#define	_NODOSDRIVE
#endif

#if	defined (_NOKANJICONV) && !defined (_NOKANJIFCONV)
#define	_NOKANJIFCONV
#endif

#ifndef	L_SET
# ifdef	SEEK_SET
# define	L_SET	SEEK_SET
# else
# define	L_SET	0
# endif
#endif

#define	ASCII		000
#define	KANA		001
#define	KANJI		002
#define	JKANA		004
#define	SJ_UDEF		0x81ac	/* GETA */
#define	UNICODETBL	"fd-unicd.tbl"
#define	MINUNICODE	0x00a7
#define	MAXUNICODE	0xffe5
#define	MINKANJI	0x8140
#define	MAXKANJI	0xfc4b
#define	jcnv(c, io)	((((io) == L_FNAME) && ((c) == '/')) ? ' ' : (c))
#define	jdecnv(c, io)	((((io) == L_FNAME) && ((c) == ' ')) ? '/' : (c))
#ifndef	O_BINARY
#define	O_BINARY	0
#endif

typedef struct _kconv_t {
	u_short start;
	u_short cnv;
	u_short range;
} kconv_t;

extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strncpy3 __P_((char *, char *, int *, int));
extern int strlen3 __P_((char *));
extern VOID error __P_((char *));
extern off_t Xlseek __P_((int, off_t, int));

#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
static char *NEAR strstr2 __P_((char *, char *));
#endif
#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
static VOID NEAR sj2j __P_((char *, u_char *));
static VOID NEAR j2sj __P_((char *, u_char *));
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
static int NEAR toenglish __P_((char *, u_char *, int));
static int NEAR tojis7 __P_((char *, u_char *, int, int, int, int));
static int NEAR fromjis __P_((char *, u_char *, int, int));
# if	defined (FD) && (FD >= 2)
static int NEAR tojis8 __P_((char *, u_char *, int, int, int, int));
static int NEAR tojunet __P_((char *, u_char *, int, int, int, int));
static int NEAR toutf8 __P_((char *, u_char *, int));
static int NEAR fromutf8 __P_((char *, u_char *, int));
#  ifndef	_NOKANJIFCONV
static int NEAR bin2hex __P_((char *, int));
static int NEAR tohex __P_((char *, u_char *, int));
static int NEAR fromhex __P_((char *, u_char *, int));
static int NEAR bin2cap __P_((char *, int));
static int NEAR tocap __P_((char *, u_char *, int));
static int NEAR fromcap __P_((char *, u_char *, int));
#  endif	/* !_NOKANJIFCONV */
# endif	/* FD && (FD >= 2) */
static char *NEAR _kanjiconv __P_((char *, char *, int, int, int, int *, int));
#endif	/* !MSDOS && !_NOKANJICONV */

#if	!MSDOS && !defined (_NOKANJICONV)
int inputkcode = 0;
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
int outputkcode = 0;
#endif
#if	!MSDOS && !defined (_NOKANJIFCONV)
int fnamekcode = 0;
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NODOSDRIVE)
char *unitblpath = NULL;
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

#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NODOSDRIVE)
static u_char *unitblbuf = NULL;
static u_short unitblent = 0;
static kconv_t rsjistable[] = {
	{0x8470, 0x8440, 0x0f},		/* Cyrillic small letters */
	{0x8480, 0x844f, 0x12},		/* Why they converted ? */
#define	EXCEPTRUSS	2
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
#define	RSJISTBLSIZ	((int)(sizeof(rsjistable) / sizeof(kconv_t)))
#endif	/* (!MSDOS && !_NOKANJICONV) || !_NODOSDRIVE */
#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
static kconv_t convtable[] = {
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
#define	CNVTBLSIZ	((int)(sizeof(convtable) / sizeof(kconv_t)))
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
#if	!MSDOS && !defined (_NOKANJIFCONV) && defined (FD) && (FD >= 2)
static u_char hctypetable[256] = {
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
static u_char h2btable[256] = {
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
static u_char b2htable[256] = "0123456789abcdef";
#define	HC_HEX		0001
#define	HC_CAP		0002

#define	tobin(c)	h2btable[(u_char)(c)]
#define	tohexa(c)	b2htable[(u_char)(c)]
#define	tobin2(s, i)	(u_char)((tobin((s)[i]) << 4) | (tobin((s)[(i) + 1])))

#define	HEXTAG		':'
#define	ishex(s, i)	(((s)[i] == HEXTAG) \
			&& (hctypetable[(u_char)(s)[(i) + 1]] & HC_HEX) \
			&& (hctypetable[(u_char)(s)[(i) + 2]] & HC_HEX))
#define	CAPTAG		':'
#define	iscap(s, i)	(((s)[i] == CAPTAG) \
			&& (hctypetable[(u_char)(s)[(i) + 1]] & HC_CAP) \
			&& (hctypetable[(u_char)(s)[(i) + 2]] & HC_HEX))
#endif	/* !MSDOS && !_NOKANJIFCONV && FD && FD >= 2 */

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
#if	(!MSDOS && defined (FD) && (FD >= 2) && !defined (_NOKANJICONV)) \
|| !defined (_NODOSDRIVE)
VOID readunitable __P_((VOID_A));
VOID discardunitable __P_((VOID_A));
u_int unifysjis __P_((u_int, int));
u_int cnvunicode __P_((u_int, int));
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
int kanjiconv __P_((char *, char *, int, int, int, int));
# ifndef	_NOKANJIFCONV
char *kanjiconv2 __P_((char *, char *, int, int, int));
# endif
#endif
int kanjifputs __P_((char *, FILE *));
int kanjiputs __P_((char *));
int kanjiprintf __P_((CONST char *, ...));
int kanjiputs2 __P_((char *, int, int));


int onkanji1(s, ptr)
char *s;
int ptr;
{
	int i;

	if (ptr < 0) return(0);
	if (!ptr) return(iskanji1(s, 0));

	for (i = 0; i < ptr; i++) {
		if (!s[i]) return(0);
#ifdef	CODEEUC
		if (isekana(s, i)) i++;
		else
#endif
		if (iskanji1(s, i)) i++;
	}
	if (i > ptr) return(0);
	return(iskanji1(s, i));
}

#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
static char *NEAR strstr2(s1, s2)
char *s1, *s2;
{
	int i, c1, c2;

	while (*s1) {
		for (i = 0;; i++) {
			if (!s2[i]) return(s1);
			c1 = toupper(s1[i]);
			c2 = toupper(s2[i]);
			if (c1 != c2) break;
#ifndef	CODEEUC
			if (issjis1(c1)) {
				if (!s2[++i]) return(s1);
				if (s1[i] != s2[i]) break;
			}
#endif
			if (!s1[i]) break;
		}
#ifndef	CODEEUC
		if (issjis1(*s1)) s1++;
#endif
		s1++;
	}
	return(NULL);
}

/*ARGSUSED*/
int getlang(s, io)
char *s;
int io;
{
	int ret;

	if (!s) ret = NOCNV;
# if	!MSDOS && !defined (_NOKANJICONV)
#  ifndef	_NOKANJIFCONV
	else if (io == L_FNAME && strstr2(s, "hex")) ret = HEX;
	else if (io == L_FNAME && strstr2(s, "cap")) ret = CAP;
#  endif	/* !_NOKANJIFCONV */
	else if (strstr2(s, "sjis")) ret = SJIS;
	else if (strstr2(s, "euc") || strstr2(s, "ujis")) ret = EUC;
#  if	defined (FD) && (FD >= 2)
	else if (strstr2(s, "utf8") || strstr2(s, "utf-8")) ret = UTF8;
	else if (strstr2(s, "ojunet")) ret = O_JUNET;
	else if (strstr2(s, "ojis8")) ret = O_JIS8;
	else if (strstr2(s, "ojis")) ret = O_JIS7;
	else if (strstr2(s, "junet")) ret = JUNET;
	else if (strstr2(s, "jis8")) ret = JIS8;
#  endif	/* FD && (FD >= 2) */
	else if (strstr2(s, "jis")) ret = JIS7;
# endif	/* !MSDOS && !_NOKANJICONV */
# ifndef	_NOENGMES
	else if (io == L_OUTPUT && (strstr2(s, "eng") || !strcmp(s, "C")))
		ret = ENG;
# endif
	else ret = NOCNV;

# if	!MSDOS && !defined (_NOKANJICONV)
	if (io == L_INPUT) {
#  ifdef	CODEEUC
		if (ret != SJIS) ret = EUC;
#  else
		if (ret != EUC) ret = SJIS;
#  endif
	}
# endif	/* !MSDOS && !_NOKANJICONV */
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
static VOID NEAR sj2j(buf, s)
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
			if (w >= convtable[i].start
			&& w < convtable[i].start + convtable[i].range)
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

static VOID NEAR j2sj(buf, s)
char *buf;
u_char *s;
{
	u_short w;
	int i, s1, s2, j1, j2;

	j1 = s[0] & 0x7f;
	j2 = s[1] & 0x7f;

	s1 = j2sjtable1[j1];
	s2 = (j1 & 1) ? j2sjtable2[j2] : (j2 + 0x7e);
	w = (s1 << 8) | s2;

	for (i = 0; i < CNVTBLSIZ; i++)
	if (w >= convtable[i].cnv
	&& w < convtable[i].cnv + convtable[i].range) {
		w -= convtable[i].cnv;
		w += convtable[i].start;
		s1 = (w >> 8) & 0xff;
		s2 = w & 0xff;
		break;
	}

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
			sj2j(&(buf[j]), &(s[i++]));
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
		else if (iseuc(s[i]) && iseuc(s[i + 1]))
			j2sj(&(buf[j++]), &(s[i++]));
		else buf[j] = s[i];
	}
	return(j);
}
#endif	/* !MSDOS && (!_NOKANJICONV || (!_NODOSDRIVE && CODEEUC)) */

#if	(!MSDOS && defined (FD) && (FD >= 2) && !defined (_NOKANJICONV)) \
|| !defined (_NODOSDRIVE)
VOID readunitable(VOID_A)
{
	char path[MAXPATHLEN];
	u_char buf[2];
	long size;
	int fd;

	if (!unitblpath || !*unitblpath) strcpy(path, UNICODETBL);
	else strcatdelim2(path, unitblpath, UNICODETBL);

	if ((fd = open(path, O_BINARY | O_RDONLY, 0600)) < 0) return;
	if (read(fd, buf, 2) != 2) {
		close(fd);
		return;
	}
	unitblent = (((u_short)(buf[1]) << 8) | buf[0]);
	size = (long)unitblent * 4;

	unitblbuf = (u_char *)realloc2(unitblbuf, size);
	if (read(fd, unitblbuf, size) != size) discardunitable();
	close(fd);
}

VOID discardunitable(VOID_A)
{
	if (unitblbuf) {
		free(unitblbuf);
		unitblbuf = NULL;
	}
}

u_int unifysjis(wc, russ)
u_int wc;
int russ;
{
	int i;

	wc &= 0xffff;
	for (i = ((russ) ? 0 : EXCEPTRUSS); i < RSJISTBLSIZ; i++)
		if (wc >= rsjistable[i].start
		&& wc < rsjistable[i].start + rsjistable[i].range)
			break;
	if (i < RSJISTBLSIZ) {
		wc -= rsjistable[i].start;
		wc += rsjistable[i].cnv;
	}
	return(wc);
}

u_int cnvunicode(wc, encode)
u_int wc;
int encode;
{
	static int fd = -1;
	u_char buf[4];
	char path[MAXPATHLEN];
	u_short min, max, ofs;
	u_int r, w;

	wc &= 0xffff;
	if (encode < 0) {
		if (fd >= 0) close(fd);
		fd = -1;
		return(0);
	}

	if (encode) {
		r = 0xff00;
		if (wc < 0x80) return(wc);
		if (wc > 0xa0 && wc <= 0xdf) return(0xff00 | (wc - 0x40));
		if (wc >= 0x8260 && wc <= 0x8279)
			return(0xff00 | (wc - 0x8260 + 0x21));
		if (wc >= 0x8281 && wc <= 0x829a)
			return(0xff00 | (wc - 0x8281 + 0x41));
		if (wc < MINKANJI || wc > MAXKANJI) return(r);
	}
	else {
		r = '?';
		switch (wc & 0xff00) {
			case 0:
				if ((wc & 0xff) < 0x80) return(wc);
				break;
			case 0xff00:
				w = wc & 0xff;
				if (w > 0x20 && w <= 0x3a)
					return(w + 0x8260 - 0x21);
				if (w > 0x40 && w <= 0x5a)
					return(w + 0x8281 - 0x41);
				if (w > 0x60 && w <= 0x9f)
					return(w + 0x40);
				break;
			default:
				break;
		}
		if (wc < MINUNICODE || wc > MAXUNICODE) return(r);
	}

	if (unitblbuf) {
		u_char *cp;

		if (encode) {
			wc = unifysjis(wc, 0);
			cp = unitblbuf;
			for (ofs = 0; ofs < unitblent; ofs++) {
				w = (((u_short)(cp[3]) << 8) | cp[2]);
				if (wc == w) {
					r = (((u_short)(cp[1]) << 8) | cp[0]);
					break;
				}
				cp += 4;
			}
		}
		else {
			min = 0;
			max = unitblent + 1;
			ofs = unitblent / 2 + 1;
			for (;;) {
				if (ofs == min || ofs == max) break;
				cp = &(unitblbuf[(ofs - 1) * 4]);
				w = (((u_short)(cp[1]) << 8) | cp[0]);
				if (wc == w) {
					r = (((u_short)(cp[3]) << 8) | cp[2]);
					break;
				}
				else if (wc < w) {
					max = ofs;
					ofs = (ofs + min) / 2;
				}
				else {
					min = ofs;
					ofs = (ofs + max) / 2;
				}
			}
		}

		return(r);
	}

	if (fd < 0) {
		if (!unitblpath || !*unitblpath) strcpy(path, UNICODETBL);
		else strcatdelim2(path, unitblpath, UNICODETBL);

		if ((fd = open(path, O_BINARY | O_RDONLY, 0600)) < 0)
			return(r);
		if (!unitblent) {
			if (read(fd, buf, 2) != 2) {
				close(fd);
				fd = -1;
				return(r);
			}
			unitblent = (((u_short)(buf[1]) << 8) | buf[0]);
		}
	}

	if (encode) {
		if (Xlseek(fd, (off_t)2, L_SET) < (off_t)0) return(r);
		wc = unifysjis(wc, 0);
		for (ofs = 0; ofs < unitblent; ofs++) {
			if (read(fd, buf, 4) != 4) break;
			w = (((u_short)(buf[3]) << 8) | buf[2]);
			if (wc == w) {
				r = (((u_short)(buf[1]) << 8) | buf[0]);
				break;
			}
		}
	}
	else {
		min = 0;
		max = unitblent + 1;
		ofs = unitblent / 2 + 1;
		for (;;) {
			if (ofs == min || ofs == max) break;
			if (Xlseek(fd, (off_t)(ofs - 1) * 4 + 2, L_SET)
			< (off_t)0
			|| read(fd, buf, 4) != 4) break;
			w = (((u_short)(buf[1]) << 8) | buf[0]);
			if (wc == w) {
				r = (((u_short)(buf[3]) << 8) | buf[2]);
				break;
			}
			else if (wc < w) {
				max = ofs;
				ofs = (ofs + min) / 2;
			}
			else {
				min = ofs;
				ofs = (ofs + max) / 2;
			}
		}
	}

	return(r);
}
#endif	/* (!MSDOS && FD && (FD >= 2) && !_NOKANJICONV) || !_NODOSDRIVE */

#if	!MSDOS && !defined (_NOKANJICONV)
static int NEAR toenglish(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max - 1; i++, j++) {
# ifdef	CODEEUC
		if (isekana(s, i)) {
			i++;
			buf[j] = '?';
		}
# else
		if (isskana(s, i)) buf[j] = '?';
# endif
		else if (iskanji1(s, i)) {
			i++;
			buf[j++] = '?';
			buf[j] = '?';
		}
		else buf[j] = s[i];
	}
	return(j);
}

static int NEAR tojis7(buf, s, max, knj, asc, io)
char *buf;
u_char *s;
int max, knj, asc, io;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 7; i++, j++) {
# ifdef	CODEEUC
		if (isekana(s, i)) {
			i++;
# else
		if (isskana(s, i)) {
# endif
			if (!(mode & KANA)) buf[j++] = '\016';
			mode |= KANA;
			buf[j] = s[i] & ~0x80;
			buf[j] = jcnv(buf[j], io);
			continue;
		}
		if (mode & KANA) buf[j++] = '\017';
		mode &= ~KANA;
		if (iskanji1(s, i)) {
			if (!(mode & KANJI)) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = knj;
			}
			mode |= KANJI;
# ifdef	CODEEUC
			buf[j++] = s[i++] & ~0x80;
			buf[j] = s[i] & ~0x80;
# else
			sj2j(&(buf[j++]), &(s[i++]));
# endif
			buf[j] = jcnv(buf[j], io);
		}
		else {
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
u_char *s;
int max, io;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 1; i++)
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
					buf[j++] = 0x8e;
# endif
					buf[j++] = jdecnv(s[i], io) | 0x80;
				}
			}
			else if (mode & KANJI) {
				if (!isjis(s[i]) || !isjis(s[i + 1])) {
					buf[j++] = s[i++];
					buf[j++] = s[i];
				}
				else {
# ifdef	CODEEUC
					buf[j++] = s[i++] | 0x80;
					buf[j++] = jdecnv(s[i], io) | 0x80;
# else
					u_char tmp[2];

					tmp[0] = s[i++];
					tmp[1] = jdecnv(s[i], io);
					j2sj(&(buf[j]), tmp);
					j += 2;
# endif
				}
			}
#  ifdef	CODEEUC
			else if (iskna(s[i])) {
				buf[j++] = 0x8e;
				buf[j++] = s[i];
			}
#  endif
			else buf[j++] = s[i];
			break;
	}
	return(j);
}

# if	defined (FD) && (FD >= 2)
static int NEAR tojis8(buf, s, max, knj, asc, io)
char *buf;
u_char *s;
int max, knj, asc, io;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 7; i++, j++) {
#  ifdef	CODEEUC
		if (isekana(s, i)) {
			if (mode == KANJI) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = asc;
			}
			mode = ASCII;
			buf[j] = s[++i];
		}
		else
#  endif
		if (iskanji1(s, i)) {
			if (mode != KANJI) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = knj;
			}
			mode = KANJI;
#  ifdef	CODEEUC
			buf[j++] = s[i++] & ~0x80;
			buf[j] = s[i] & ~0x80;
#  else
			sj2j(&(buf[j++]), &(s[i++]));
#  endif
			buf[j] = jcnv(buf[j], io);
		}
		else {
			if (mode == KANJI) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = asc;
			}
			mode = ASCII;
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
u_char *s;
int max, knj, asc, io;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 7; i++, j++) {
#  ifdef	CODEEUC
		if (isekana(s, i)) {
			i++;
#  else
		if (isskana(s, i)) {
#  endif
			if (mode != JKANA) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = 'I';
			}
			mode = JKANA;
			buf[j] = s[i] & ~0x80;
			buf[j] = jcnv(buf[j], io);
		}
		else if (iskanji1(s, i)) {
			if (mode != KANJI) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = knj;
			}
			mode = KANJI;
#  ifdef	CODEEUC
			buf[j++] = s[i++] & ~0x80;
			buf[j] = s[i] & ~0x80;
#  else
			sj2j(&(buf[j++]), &(s[i++]));
#  endif
			buf[j] = jcnv(buf[j], io);
		}
		else {
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

static int NEAR toutf8(buf, s, max)
char *buf;
u_char *s;
int max;
{
	u_short w;
	int i, j;

	for (i = j = 0; s[i] && j < max - 3; i++, j++) {
#  ifdef	CODEEUC
		if (isekana(s, i)) w = s[++i];
		else if (iseuc(s[i]) && iseuc(s[i + 1])) {
			u_char tmp[2];

			j2sj((char *)tmp, &(s[i++]));
			w = (tmp[0] << 8) | tmp[1];
		}
#  else
		if (isskana(s, i)) w = s[i];
		else if (issjis1(s[i]) && issjis2(s[i + 1]))
			w = (s[0] << 8) | s[1];
#  endif
		else w = s[i];
		w = cnvunicode(w, 1);

		if (w < 0x80) buf[j] = w;
		else if (w < 0x800) {
			buf[j] = 0xc0 | (w >> 6);
			buf[j] = 0x80 | (w & 0x3f);
		}
		else {
			buf[j++] = 0xe0 | (w >> 12);
			buf[j++] = 0x80 | ((w >> 6) & 0x3f);
			buf[j] = 0x80 | (w & 0x3f);
		}
	}
	cnvunicode(0, -1);
	return(j);
}

static int NEAR fromutf8(buf, s, max)
char *buf;
u_char *s;
int max;
{
	u_short w;
	int i, j, c1, c2;

	for (i = j = 0; s[i] && j < max - 2; i++, j++) {
		w = s[i];
		if (w < 0x80);
		else if ((w & 0xe0) == 0xc0 && (s[i + 1] & 0xc0) == 0x80)
			w = ((w & 0x1f) << 6) | (s[++i] & 0x3f);
		else {
			w = ((w & 0x0f) << 6) | (s[++i] & 0x3f);
			w = (w << 6) | (s[++i] & 0x3f);
		}

		w = cnvunicode(w, 0);
		c1 = (w >> 8) & 0xff;
		c2 = w & 0xff;
		if (w > 0xa0 && w <= 0xdf) {
#  ifdef	CODEEUC
			buf[j++] = 0x8e;
#  endif
			buf[j] = w;
		}
		else if (issjis1(c1) && issjis2(c2)) {
#  ifdef	CODEEUC
			u_char tmp[2];

			tmp[0] = c1;
			tmp[1] = c2;
			sj2j(&(buf[j]), tmp);
			buf[j++] |= 0x80;
			buf[j] |= 0x80;
#  else
			buf[j++] = c1;
			buf[j] = c2;
#  endif
		}
		else buf[j] = w;
	}
	cnvunicode(0, -1);
	return(j);
}

#  ifndef	_NOKANJIFCONV
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
u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max - 2; i++, j++) {
#   ifdef	CODEEUC
		if (isekana(s, i)) j += bin2hex(&(buf[j]), s[++i]) - 1;
		else if (iseuc(s[i]) && iseuc(s[i + 1])) {
			u_char tmp[2];

			j2sj((char *)tmp, &(s[i++]));
			j += bin2hex(&(buf[j]), tmp[0]);
			j += bin2hex(&(buf[j]), tmp[1]) - 1;
		}
#   else
		if (isskana(s, i)) j += bin2hex(&(buf[j]), s[i]) - 1;
		else if (issjis1(s[i]) && issjis2(s[i + 1])) {
			j += bin2hex(&(buf[j]), s[i++]);
			j += bin2hex(&(buf[j]), s[i]) - 1;
		}
#   endif
		else buf[j] = s[i];
	}
	return(j);
}

static int NEAR fromhex(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j, c1, c2;

	for (i = j = 0; s[i] && j < max - 1; i++, j++) {
		if (!ishex(s, i)) buf[j] = s[i];
		else {
			c1 = tobin2(s, i + 1);
			i += 2;
			if (iskna(c1)) {
#   ifdef	CODEEUC
				buf[j++] = 0x8e;
#   endif
				buf[j] = c1;
			}
			else if (issjis1(c1)) {
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
#   ifdef	CODEEUC
					u_char tmp[2];

					tmp[0] = c1;
					tmp[1] = c2;
					sj2j(&(buf[j]), tmp);
					buf[j++] |= 0x80;
					buf[j] |= 0x80;
#   else
					buf[j++] = c1;
					buf[j] = c2;
#   endif
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
u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max - 2; i++, j++) {
#   ifdef	CODEEUC
		if (isekana(s, i)) j += bin2cap(&(buf[j]), s[++i]) - 1;
		else if (iseuc(s[i]) && iseuc(s[i + 1])) {
			u_char tmp[2];

			j2sj((char *)tmp, &(s[i++]));
			j += bin2cap(&(buf[j]), tmp[0]);
			j += bin2cap(&(buf[j]), tmp[1]) - 1;
		}
#   else
		if (isskana(s, i)) j += bin2cap(&(buf[j]), s[i]) - 1;
		else if (issjis1(s[i]) && issjis2(s[i + 1])) {
			j += bin2cap(&(buf[j]), s[i++]);
			j += bin2cap(&(buf[j]), s[i]) - 1;
		}
#   endif
		else buf[j] = s[i];
	}
	return(j);
}

static int NEAR fromcap(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j, c1, c2;

	for (i = j = 0; s[i] && j < max - 1; i++, j++) {
		if (!iscap(s, i)) buf[j] = s[i];
		else {
			c1 = tobin2(s, i + 1);
			i += 2;
			if (iskna(c1)) {
#   ifdef	CODEEUC
				buf[j++] = 0x8e;
#   endif
				buf[j] = c1;
			}
			else if (issjis1(c1)) {
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
#   ifdef	CODEEUC
					u_char tmp[2];

					tmp[0] = c1;
					tmp[1] = c2;
					sj2j(&(buf[j]), tmp);
					buf[j++] |= 0x80;
					buf[j] |= 0x80;
#   else
					buf[j++] = c1;
					buf[j] = c2;
#   endif
				}
			}
			else buf[j] = c1;
		}
	}
	return(j);
}
#  endif	/* !_NOKANJIFCONV */
# endif	/* FD && (FD >= 2) */

static char *NEAR _kanjiconv(buf, s, max, in, out, lenp, io)
char *buf, *s;
int max, in, out, *lenp, io;
{
	if (in == out || in == NOCNV || out == NOCNV) return(s);
	switch (out) {
# ifdef	CODEEUC
		case SJIS:
			*lenp = ujis2sjis(buf, (u_char *)s, max);
			break;
		case EUC:
			switch (in) {
				case SJIS:
					*lenp = sjis2ujis(buf, (u_char *)s,
						max);
# else
		case EUC:
			*lenp = sjis2ujis(buf, (u_char *)s, max);
			break;
		case SJIS:
			switch (in) {
				case EUC:
					*lenp = ujis2sjis(buf, (u_char *)s,
						max);
# endif
					break;
				case JIS7:
# if	defined (FD) && (FD >= 2)
				case O_JIS7:
				case JIS8:
				case O_JIS8:
				case JUNET:
				case O_JUNET:
# endif	/* FD && (FD >= 2) */
					*lenp = fromjis(buf, (u_char *)s,
						max, io);
					break;
# if	defined (FD) && (FD >= 2)
				case UTF8:
					*lenp = fromutf8(buf, (u_char *)s,
						max);
					break;
#  ifndef	_NOKANJIFCONV
				case HEX:
					*lenp = fromhex(buf, (u_char *)s, max);
					break;
				case CAP:
					*lenp = fromcap(buf, (u_char *)s, max);
					break;
#  endif	/* !_NOKANJIFCONV */
# endif	/* FD && (FD >= 2) */
				default:
					return(s);
/*NOTREACHED*/
					break;
			}
			break;
		case ENG:
			*lenp = toenglish(buf, (u_char *)s, max);
			break;
		case JIS7:
			*lenp = tojis7(buf, (u_char *)s, max, 'B', 'B', io);
			break;
# if	defined (FD) && (FD >= 2)
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
		case UTF8:
			*lenp = toutf8(buf, (u_char *)s, max);
			break;
#  ifndef	_NOKANJIFCONV
		case HEX:
			*lenp = tohex(buf, (u_char *)s, max);
			break;
		case CAP:
			*lenp = tocap(buf, (u_char *)s, max);
			break;
#  endif	/* !_NOKANJIFCONV */
# endif	/* FD && (FD >= 2) */
		default:
			return(s);
/*NOTREACHED*/
			break;
	}
	return(buf);
}

int kanjiconv(buf, s, max, in, out, io)
char *buf, *s;
int max, in, out, io;
{
	int len;

	if (_kanjiconv(buf, s, max, in, out, &len, io) != buf)
		for (len = 0; s[len]; len++) buf[len] = s[len];
	return(len);
}

# ifndef	_NOKANJIFCONV
char *kanjiconv2(buf, s, max, in, out)
char *buf, *s;
int max, in, out;
{
	char *cp;
	int len;

	if ((cp = _kanjiconv(buf, s, max, in, out, &len, L_FNAME)) == buf)
		cp[len] = '\0';
	return(cp);
}
# endif	/* !_NOKANJIFCONV */
#endif	/* !MSDOS && !_NOKANJICONV */

int kanjifputs(s, fp)
char *s;
FILE *fp;
{
#if	MSDOS || defined (_NOKANJICONV)
	return(fputs(s, fp));
#else
	char *cp, *buf;
	int len, max;

	len = strlen(s);
	max = len * 3 + 3;
	buf = malloc2(max + 1);
	cp = _kanjiconv(buf, s, max, DEFCODE, outputkcode, &len, L_OUTPUT);
	if (cp == buf) {
		buf[len] = '\0';
		s = buf;
	}
	if (fputs(s, fp) < 0) len = -1;
	free(buf);
	return(len);
#endif
}

int kanjiputs(s)
char *s;
{
#if	MSDOS || defined (_NOKANJICONV)
	cputs2(s);
	return(strlen(s));
#else
	char *cp, *buf;
	int len, max;

	len = strlen(s);
	max = len * 3 + 3;
	buf = malloc2(max + 1);
	cp = _kanjiconv(buf, s, max, DEFCODE, outputkcode, &len, L_OUTPUT);
	if (cp == buf) {
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
#else
/*VARARGS1*/
int kanjiprintf(fmt, va_alist)
char *fmt;
va_dcl
#endif
{
	va_list args;
	char *buf;
	int n;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	n = vasprintf2(&buf, fmt, args);
	va_end(args);
	if (n < 0) error("malloc()");
	n = strlen3(buf);
	kanjiputs(buf);
	free(buf);
	return(n);
}

int kanjiputs2(s, len, ptr)
char *s;
int len, ptr;
{
	char *buf;
	int n;

	if (len >= 0) buf = malloc2(len * KANAWID + 1);
	else if (ptr < 0) buf = malloc2(strlen(s) + 1);
	else buf = malloc2(strlen(&(s[ptr])) + 1);
	strncpy3(buf, s, &len, ptr);
	n = strlen3(buf);
	kanjiputs(buf);
	free(buf);
	return(n);
}
