/*
 *	kanji.c
 *
 *	Kanji Convert Function
 */

#define	K_EXTERN

#include <fcntl.h>
#ifdef	FD
#include "fd.h"
#else	/* !FD */
#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "kctype.h"
#include "pathname.h"
#include "term.h"

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#if	MSDOS
#include "unixemu.h"
#else
#include <sys/file.h>
#include <sys/param.h>
#endif
#endif	/* !FD */

#ifdef	USETIMEH
#include <time.h>
#endif

#if	MSDOS
#include <io.h>
#endif

#ifdef	FD
#include "func.h"
extern int norealpath;
# ifndef	_NOROCKRIDGE
extern int norockridge;
# endif
#else	/* !FD */
#define	realpath2(p, r, l)	realpath(p, r)
extern char *malloc2 __P_((ALLOC_T));
extern char *strncpy2 __P_((char *, char *, int));
#define	Xlseek	lseek
# ifndef	_NOKANJIFCONV
extern char *includepath __P_((char *, char *));
# endif
#endif	/* !FD */

#ifndef	L_SET
# ifdef	SEEK_SET
# define	L_SET	SEEK_SET
# else
# define	L_SET	0
# endif
#endif
#ifndef	L_INCR
# ifdef	SEEK_CUR
# define	L_INCR	SEEK_CUR
# else
# define	L_INCR	1
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

#ifndef	_NOKANJIFCONV
typedef struct _kpathtable {
	char **path;
	u_short code;
} kpathtable;
#endif

#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
static char *NEAR strstr2 __P_((char *, char *));
#endif
#if	!defined (_NOKANJICONV) \
|| (defined (FD) && defined (_USEDOSEMU) && defined (CODEEUC))
static VOID NEAR sj2j __P_((char *, u_char *));
static VOID NEAR j2sj __P_((char *, u_char *));
#endif
#if	!defined (_NOKANJICONV) || (defined (FD) && !defined (_NODOSDRIVE))
static int NEAR openunitbl __P_((char *));
static u_char * NEAR tblmalloc __P_((ALLOC_T));
#define	getword(s, n)	(((u_int)((s)[(n) + 1]) << 8) | (s)[n])
#endif
#ifndef	_NOKANJICONV
static int NEAR opennftbl __P_((char *, int, u_int *));
static int NEAR toenglish __P_((char *, u_char *, int));
static int NEAR tojis7 __P_((char *, u_char *, int, int, int, int));
static int NEAR fromjis __P_((char *, u_char *, int, int));
static int NEAR tojis8 __P_((char *, u_char *, int, int, int, int));
static int NEAR tojunet __P_((char *, u_char *, int, int, int, int));
static VOID NEAR ucs2normalization __P_((u_int *, int *, int, u_int, int));
static u_int NEAR ucs2denormalization __P_((u_int *, int *, int));
static u_int NEAR toucs2 __P_((u_char *, int *));
static VOID NEAR fromucs2 __P_((char *, int *, u_int));
static VOID NEAR ucs2toutf8 __P_((char *, int *, u_int));
static u_int NEAR ucs2fromutf8 __P_((u_char *, int *));
static int NEAR toutf8 __P_((char *, u_char *, int));
static int NEAR fromutf8 __P_((char *, u_char *, int));
static int NEAR toutf8nf __P_((char *, u_char *, int, int));
static int NEAR fromutf8nf __P_((char *, u_char *, int, int));
static int NEAR bin2hex __P_((char *, int));
static int NEAR tohex __P_((char *, u_char *, int));
static int NEAR fromhex __P_((char *, u_char *, int));
static int NEAR bin2cap __P_((char *, int));
static int NEAR tocap __P_((char *, u_char *, int));
static int NEAR fromcap __P_((char *, u_char *, int));
static char *NEAR _kanjiconv __P_((char *, char *, int, int, int, int *, int));
#endif	/* !_NOKANJICONV */

int noconv = 0;
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
char *noconvpath = NULL;
#endif	/* !_NOKANJIFCONV */
#if	!defined (_NOKANJICONV) || (defined (FD) && !defined (_NODOSDRIVE))
char *unitblpath = NULL;
int unicodebuffer = 0;
#endif

#if	!defined (_NOKANJICONV) || (defined (FD) && !defined (_NODOSDRIVE))
static u_char *unitblbuf = NULL;
static u_int unitblent = 0;
static CONST kconv_t rsjistable[] = {
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
#endif	/* !_NOKANJICONV || (FD && !_NODOSDRIVE) */
#if	!defined (_NOKANJICONV) \
|| (defined (FD) && defined (_USEDOSEMU) && defined (CODEEUC))
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
#define	CNVTBLSIZ	((int)(sizeof(convtable) / sizeof(kconv_t)))
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
#endif	/* !_NOKANJICONV || (FD && _USEDOSEMU && CODEEUC) */
#ifndef	_NOKANJICONV
static u_int nftblnum = 0;
static u_int nflen = 0;
static u_char **nftblbuf = NULL;
static u_int *nftblent = 0;
#define	NF_MAC		1
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
static CONST u_char b2htable[] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
};
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
#endif	/* !_NOKANJICONV */
#ifndef	_NOKANJIFCONV
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
	{&noconvpath, NOCNV},
};
#define	MAXKPATHLIST	(sizeof(kpathlist) / sizeof(kpathtable))
#endif	/* !_NOKANJIFCONV */

#ifndef	FD
int onkanji1 __P_((char *, int));
#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
int getlang __P_((char *, int));
#endif
#if	!defined (_NOENGMES) && !defined (_NOJPNMES)
char *mesconv __P_((char *, char *));
#endif
#if	!defined (_NOKANJICONV) \
|| (defined (FD) && defined (_USEDOSEMU) && defined (CODEEUC))
int sjis2ujis __P_((char *, u_char *, int));
int ujis2sjis __P_((char *, u_char *, int));
#endif
#if	!defined (_NOKANJICONV) || (defined (FD) && !defined (_NODOSDRIVE))
VOID readunitable __P_((int));
VOID discardunitable __P_((VOID_A));
u_int unifysjis __P_((u_int, int));
u_int cnvunicode __P_((u_int, int));
#endif
#ifndef	_NOKANJICONV
int kanjiconv __P_((char *, char *, int, int, int, int));
char *kanjiconv2 __P_((char *, char *, int, int, int, int));
char *newkanjiconv __P_((char *, int, int, int));
#endif	/* !_NOKANJICONV */
#ifndef	_NOKANJIFCONV
int getkcode __P_((char *));
#endif
char *convget __P_((char *, char *, int));
char *convput __P_((char *, char *, int, int, char *, int *));
#endif	/* !FD */


int onkanji1(s, ptr)
char *s;
int ptr;
{
	int i;

	if (ptr < 0) return(0);
	if (!ptr) return(iskanji1(s, 0));

	for (i = 0; i < ptr; i++) {
		if (!s[i]) return(0);
		else if (iskanji1(s, i)) i++;
#ifdef	CODEEUC
		else if (isekana(s, i)) i++;
#endif
	}
	if (i > ptr) return(0);
	return(iskanji1(s, i));
}

#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
static char *NEAR strstr2(s1, s2)
char *s1, *s2;
{
	int i, c1, c2;

	while (*s1) {
		for (i = 0;; i++) {
			if (!s2[i]) return(s1);
			c1 = toupper2(s1[i]);
			c2 = toupper2(s2[i]);
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
# ifndef	_NOKANJICONV
	else if (io == L_FNAME && strstr2(s, "hex")) ret = HEX;
	else if (io == L_FNAME && strstr2(s, "cap")) ret = CAP;
	else if (strstr2(s, "sjis")) ret = SJIS;
	else if (strstr2(s, "euc") || strstr2(s, "ujis")) ret = EUC;
	else if (strstr2(s, "utf8-mac") || strstr2(s, "mac")) ret = M_UTF8;
	else if (strstr2(s, "utf8") || strstr2(s, "utf-8")) ret = UTF8;
	else if (strstr2(s, "ojunet")) ret = O_JUNET;
	else if (strstr2(s, "ojis8")) ret = O_JIS8;
	else if (strstr2(s, "ojis")) ret = O_JIS7;
	else if (strstr2(s, "junet")) ret = JUNET;
	else if (strstr2(s, "jis8")) ret = JIS8;
	else if (strstr2(s, "jis")) ret = JIS7;
# endif	/* !_NOKANJICONV */
# ifndef	_NOENGMES
	else if (io == L_OUTPUT && (strstr2(s, "eng") || !strcmp(s, "C")))
		ret = ENG;
# endif
	else ret = NOCNV;

# ifndef	_NOKANJICONV
	if (io == L_INPUT) {
#  ifdef	CODEEUC
		if (ret != SJIS) ret = EUC;
#  else
		if (ret != EUC) ret = SJIS;
#  endif
	}
# endif	/* !_NOKANJICONV */
	return(ret);
}
#endif	/* !_NOKANJICONV || (!_NOENGMES && !_NOJPNMES) */

#if	!defined (_NOENGMES) && !defined (_NOJPNMES)
char *mesconv(jpn, eng)
char *jpn, *eng;
{
	return((outputkcode == ENG) ? eng : jpn);
}
#endif

#if	!defined (_NOKANJICONV) \
|| (defined (FD) && defined (_USEDOSEMU) && defined (CODEEUC))
static VOID NEAR sj2j(buf, s)
char *buf;
u_char *s;
{
	int s1, s2, j1, j2;

	s1 = s[0] & 0xff;
	s2 = s[1] & 0xff;
	if (s1 >= 0xf0) {
		u_int w;
		int i;

		w = (((u_int)s1 << 8) | s2);
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
	u_int w;
	int i, s1, s2, j1, j2;

	j1 = s[0] & 0x7f;
	j2 = s[1] & 0x7f;

	s1 = j2sjtable1[j1];
	s2 = (j1 & 1) ? j2sjtable2[j2] : (j2 + 0x7e);
	w = (((u_int)s1 << 8) | s2);

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
		if (issjis1(s[i]) && issjis2(s[i + 1])) {
			sj2j(&(buf[j]), &(s[i++]));
			buf[j++] |= 0x80;
			buf[j] |= 0x80;
		}
		else if (isskana(s, i)) {
			buf[j++] = (char)C_EKANA;
			buf[j] = s[i];
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
		if (iseuc(s[i]) && iseuc(s[i + 1]))
			j2sj(&(buf[j++]), &(s[i++]));
		else if (isekana(s, i)) buf[j] = s[++i];
		else buf[j] = s[i];
	}
	return(j);
}
#endif	/* !_NOKANJICONV || (FD && _USEDOSEMU && CODEEUC) */

#if	!defined (_NOKANJICONV) || (defined (FD) && !defined (_NODOSDRIVE))
static int NEAR openunitbl(file)
char *file;
{
	static int fd = -1;
	u_char buf[2];
	char path[MAXPATHLEN];

	if (!file) {
		if (fd >= 0) close(fd);
		fd = -1;
		return(0);
	}

	if (fd >= 0) return(fd);

	if (!unitblpath || !*unitblpath) strcpy(path, file);
	else strcatdelim2(path, unitblpath, file);

	if ((fd = open(path, O_BINARY | O_RDONLY, 0600)) < 0) return(-1);
	if (!unitblent) {
		if (read(fd, buf, 2) != 2) {
			close(fd);
			fd = -1;
			return(-1);
		}
		unitblent = getword(buf, 0);
	}
	return(fd);
}

static u_char *NEAR tblmalloc(size)
ALLOC_T size;
{
	u_char *tbl;

	if ((tbl = (u_char *)malloc(size))) return(tbl);
	openunitbl(NULL);
	if (unicodebuffer) unicodebuffer = 0;
	return(NULL);
}

/*ARGSUSED*/
VOID readunitable(nf)
int nf;
{
	u_char *tbl;
	ALLOC_T size;
	int fd;

	if (unitblbuf) {
# ifndef	_NOKANJICONV
		if (nf && !nftblbuf) /*EMPTY*/;
		else
# endif
		return;
	}
	if ((fd = openunitbl(UNICODETBL)) < 0) return;
	size = (ALLOC_T)unitblent * 4;

	if (!unitblbuf) {
		if (!(tbl = tblmalloc(size))) return;
		if (Xlseek(fd, (off_t)2, L_SET) < (off_t)0
		|| read(fd, tbl, size) != size) {
			free(tbl);
			openunitbl(NULL);
			return;
		}
		unitblbuf = tbl;
	}

# ifndef	_NOKANJICONV
	if (nf && !nftblbuf) {
		u_char buf[2], **tblbuf;
		u_int *tblent;
		int i;

		if (Xlseek(fd, (off_t)size + 2, L_SET) < (off_t)0
		|| read(fd, buf, 2) != 2) {
			openunitbl(NULL);
			return;
		}
		nftblnum = buf[0];
		nflen = buf[1];
		tblbuf = (u_char **)malloc2(nftblnum * sizeof(u_char *));
		tblent = (u_int *)malloc2(nftblnum * sizeof(u_int));

		for (i = 0; i < nftblnum; i++) {
			if (read(fd, buf, 2) != 2) {
				while (i > 0) free(tblbuf[--i]);
				free(tblbuf);
				free(tblent);
				openunitbl(NULL);
				return;
			}
			tblent[i] = getword(buf, 0);
			size = (ALLOC_T)tblent[i] * (2 + nflen * 2);

			if (!(tblbuf[i] = tblmalloc(size))) {
				while (i > 0) free(tblbuf[--i]);
				free(tblbuf);
				free(tblent);
				return;
			}
			if (read(fd, tblbuf[i], size) != size) {
				while (i >= 0) free(tblbuf[i--]);
				free(tblbuf);
				free(tblent);
				openunitbl(NULL);
				return;
			}
		}
		nftblbuf = tblbuf;
		nftblent = tblent;
	}
# endif	/* !_NOKANJICONV */
	openunitbl(NULL);
}

VOID discardunitable(VOID_A)
{
	if (unitblbuf) {
		free(unitblbuf);
		unitblbuf = NULL;
	}
# ifndef	_NOKANJICONV
	if (nftblbuf) {
		int i;

		for (i = 0; i < nftblnum; i++)
			if (nftblbuf[i]) free(nftblbuf[i]);
		free(nftblbuf);
		nftblbuf = NULL;
	}
	if (nftblent) {
		free(nftblent);
		nftblent = NULL;
	}
# endif	/* !_NOKANJICONV */
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
	u_char *cp, buf[4];
	u_int r, w, min, max, ofs;
	int fd;

	wc &= 0xffff;
	if (encode < 0) {
		openunitbl(NULL);
		if (!unicodebuffer) discardunitable();
		return(0);
	}

	if (encode) {
		r = 0xff00;
		if (wc < 0x0080) return(wc);
		if (wc >= 0x00a1 && wc <= 0x00df)
			return(0xff00 | (wc - 0x00a1 + 0x61));
		if (wc >= 0x8260 && wc <= 0x8279)
			return(0xff00 | (wc - 0x8260 + 0x21));
		if (wc >= 0x8281 && wc <= 0x829a)
			return(0xff00 | (wc - 0x8281 + 0x41));
		if (wc < MINKANJI || wc > MAXKANJI) return(r);
		wc = unifysjis(wc, 0);
	}
	else {
		r = '?';
		switch (wc & 0xff00) {
			case 0:
				if ((wc & 0xff) < 0x80) return(wc);
				break;
			case 0xff00:
				w = wc & 0xff;
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
		if (wc < MINUNICODE || wc > MAXUNICODE) return(r);
	}

	if (unicodebuffer && !unitblbuf) readunitable(0);
	cp = buf;
	ofs = min = max = 0;
	if (unitblbuf) {
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
			ofs = unitblent / 2 + 1;
			for (;;) {
				if (ofs == min || ofs == max) break;
				cp = &(unitblbuf[(ofs - 1) * 4]);
				w = getword(cp, 0);
				if (wc == w) break;
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
	}
	else if ((fd = openunitbl(UNICODETBL)) < 0) ofs = unitblent;
	else if (encode) {
		if (Xlseek(fd, (off_t)2, L_SET) < (off_t)0) ofs = unitblent;
		else for (ofs = 0; ofs < unitblent; ofs++) {
			if (read(fd, cp, 4) != 4) {
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
		ofs = unitblent / 2 + 1;
		for (;;) {
			if (ofs == min || ofs == max) break;
			if (Xlseek(fd, (off_t)(ofs - 1) * 4 + 2, L_SET)
			< (off_t)0
			|| read(fd, cp, 4) != 4) {
				ofs = min = max = 0;
				break;
			}
			w = getword(cp, 0);
			if (wc == w) break;
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

	if (encode) {
		if (ofs < unitblent) r = getword(cp, 0);
	}
	else {
		if (ofs > min && ofs < max) r = getword(cp, 2);
	}

	return(r);
}
#endif	/* !_NOKANJICONV || (FD && !_NODOSDRIVE) */

#ifndef	_NOKANJICONV
static int NEAR opennftbl(file, nf, entp)
char *file;
int nf;
u_int *entp;
{
	u_char buf[2];
	off_t ofs;
	int i, fd;

	if ((fd = openunitbl(file)) < 0) return(-1);
	ofs = (off_t)unitblent * 4 + 2;

	if (!nftblnum) {
		if (Xlseek(fd, ofs, L_SET) < (off_t)0
		|| read(fd, buf, 2) != 2) return(-1);
		nftblnum = buf[0];
		nflen = buf[1];
	}

	if (nf > nftblnum) return(-1);

	*entp = 0;
	for (i = 0; i < nf; i++) {
		ofs += 2 + (off_t)*entp * (2 + nflen * 2);
		if (Xlseek(fd, ofs, L_SET) < (off_t)0
		|| read(fd, buf, 2) != 2) return(-1);
		*entp = getword(buf, 0);
	}

	return(fd);
}

static int NEAR toenglish(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max - 1; i++, j++) {
		if (iskanji1((char *)s, i)) {
			i++;
			buf[j++] = '?';
			buf[j] = '?';
		}
# ifdef	CODEEUC
		else if (isekana(s, i)) {
			i++;
			buf[j] = '?';
		}
# else
		else if (isskana(s, i)) buf[j] = '?';
# endif
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
		if (iskanji1((char *)s, i)) {
			if (mode & KANA) buf[j++] = '\017';
			mode &= ~KANA;
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
# ifdef	CODEEUC
		else if (isekana(s, i)) {
			i++;
# else
		else if (isskana(s, i)) {
# endif
			if (!(mode & KANA)) buf[j++] = '\016';
			mode |= KANA;
			buf[j] = s[i] & ~0x80;
			buf[j] = jcnv(buf[j], io);
		}
		else {
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
					buf[j++] = (char)C_EKANA;
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
# ifdef	CODEEUC
			else if (iskana2(s[i])) {
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
u_char *s;
int max, knj, asc, io;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 7; i++, j++) {
		if (iskanji1((char *)s, i)) {
			if (mode != KANJI) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = knj;
			}
			mode = KANJI;
# ifdef	CODEEUC
			buf[j++] = s[i++] & ~0x80;
			buf[j] = s[i] & ~0x80;
# else
			sj2j(&(buf[j++]), &(s[i++]));
# endif
			buf[j] = jcnv(buf[j], io);
		}
		else {
			if (mode == KANJI) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = asc;
			}
			mode = ASCII;
# ifdef	CODEEUC
			if (isekana(s, i)) i++;
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
u_char *s;
int max, knj, asc, io;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 7; i++, j++) {
		if (iskanji1((char *)s, i)) {
			if (mode != KANJI) {
				buf[j++] = '\033';
				buf[j++] = '$';
				buf[j++] = knj;
			}
			mode = KANJI;
# ifdef	CODEEUC
			buf[j++] = s[i++] & ~0x80;
			buf[j] = s[i] & ~0x80;
# else
			sj2j(&(buf[j++]), &(s[i++]));
# endif
			buf[j] = jcnv(buf[j], io);
		}
# ifdef	CODEEUC
		else if (isekana(s, i)) {
			i++;
# else
		else if (isskana(s, i)) {
# endif
			if (mode != JKANA) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = 'I';
			}
			mode = JKANA;
			buf[j] = s[i] & ~0x80;
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

static VOID NEAR ucs2normalization(buf, ptrp, max, wc, nf)
u_int *buf;
int *ptrp, max;
u_int wc;
int nf;
{
	u_char *cp, *new;
	u_int w, ofs, ent;
	int n, fd;

	cp = new = NULL;
	ofs = ent = 0;
	if (unicodebuffer && !nftblbuf) readunitable(1);

	if (wc < MINUNICODE || wc > MAXUNICODE);
	else if (nftblbuf && nf <= nftblnum) {
		n = 2 + nflen * 2;
		cp = nftblbuf[nf - 1];
		ent = nftblent[nf - 1];
		for (ofs = 0; ofs < ent; ofs++) {
			w = getword(cp, 0);
			if (wc == w) break;
			cp += n;
		}
	}
	else if ((fd = opennftbl(UNICODETBL, nf, &ent)) < 0) ent = 0;
	else {
		n = 2 + nflen * 2;
		cp = new = (u_char *)malloc2(n);
		for (ofs = 0; ofs < ent; ofs++) {
			if (read(fd, cp, n) != n) {
				ofs = ent;
				break;
			}
			w = getword(cp, 0);
			if (wc == w) break;
		}
	}

	if (ofs >= ent) buf[(*ptrp)++] = wc;
	else for (n = 0; n < nflen && *ptrp < max; n++) {
		cp += 2;
		w = getword(cp, 0);
		if (!w) break;
		buf[(*ptrp)++] = w;
	}
	if (new) free(new);
}

static u_int NEAR ucs2denormalization(buf, ptrp, nf)
u_int *buf;
int *ptrp, nf;
{
	u_char *cp, *new;
	off_t top;
	u_int w, min, max, ofs, ent;
	int i, j, n, fd;

	cp = new = NULL;
	ofs = min = max = 0;
	i = 0;
	if (unicodebuffer && !nftblbuf) readunitable(1);

	if (buf[*ptrp] < MINUNICODE || buf[*ptrp] > MAXUNICODE);
	else if (nftblbuf && nf <= nftblnum) {
		n = 2 + nflen * 2;
		max = nftblent[nf - 1] + 1;
		ofs = nftblent[nf - 1] / 2 + 1;
		for (;;) {
			if (ofs == min || ofs == max) break;
			cp = &(nftblbuf[nf - 1][(ofs - 1) * n]);
			w = 0xffff;
			for (i = 0, j = 2; i < nflen; i++, j += 2) {
				w = getword(cp, j);
				if (!w) break;
				if (buf[*ptrp + i] != w) break;
			}
			if (!w) break;
			else if (buf[*ptrp + i] < w) {
				max = ofs;
				ofs = (ofs + min) / 2;
			}
			else {
				min = ofs;
				ofs = (ofs + max) / 2;
			}
		}
	}
	else if ((fd = opennftbl(UNICODETBL, nf, &ent)) < 0
	|| (top = Xlseek(fd, (off_t)0, L_INCR)) < (off_t)0) /*EMPTY*/;
	else {
		n = 2 + nflen * 2;
		cp = new = (u_char *)malloc2(n);
		max = ent + 1;
		ofs = ent / 2 + 1;
		for (;;) {
			if (ofs == min || ofs == max) break;
			if (Xlseek(fd, (off_t)(ofs - 1) * n + top, L_SET)
			< (off_t)0
			|| read(fd, cp, n) != n) {
				ofs = min = max = 0;
				break;
			}
			w = 0xffff;
			for (i = 0, j = 2; i < nflen; i++, j += 2) {
				w = getword(cp, j);
				if (!w) break;
				if (buf[*ptrp + i] != w) break;
			}
			if (!w) break;
			else if (buf[*ptrp + i] < w) {
				max = ofs;
				ofs = (ofs + min) / 2;
			}
			else {
				min = ofs;
				ofs = (ofs + max) / 2;
			}
		}
	}

	if (ofs == min || ofs == max) w = buf[(*ptrp)++];
	else {
		w = getword(cp, 0);
		*ptrp += i;
	}
	if (new) free(new);
	return(w);
}

static u_int NEAR toucs2(s, ptrp)
u_char *s;
int *ptrp;
{
	u_int w;

# ifdef	CODEEUC
	if (iseuc(s[*ptrp]) && iseuc(s[*ptrp + 1])) {
		u_char tmp[2];

		j2sj((char *)tmp, &(s[*ptrp]));
		*ptrp += 2;
		w = (((u_int)(tmp[0]) << 8) | tmp[1]);
	}
	else if (isekana(s, *ptrp)) {
		w = s[++(*ptrp)];
		(*ptrp)++;
	}
# else
	if (issjis1(s[*ptrp]) && issjis2(s[*ptrp + 1])) {
		w = (((u_int)(s[*ptrp]) << 8) | s[*ptrp + 1]);
		*ptrp += 2;
	}
	else if (isskana(s, *ptrp)) w = s[(*ptrp)++];
# endif
	else w = s[(*ptrp)++];
	return(cnvunicode(w, 1));
}

static VOID NEAR fromucs2(buf, ptrp, wc)
char *buf;
int *ptrp;
u_int wc;
{
	int c1, c2;

	wc = cnvunicode(wc, 0);
	c1 = (wc >> 8) & 0xff;
	c2 = wc & 0xff;
	if (wc > 0xa0 && wc <= 0xdf) {
# ifdef	CODEEUC
		buf[(*ptrp)++] = (char)C_EKANA;
# endif
		buf[(*ptrp)++] = wc;
	}
	else if (issjis1(c1) && issjis2(c2)) {
# ifdef	CODEEUC
		u_char tmp[2];

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

static VOID NEAR ucs2toutf8(buf, ptrp, wc)
char *buf;
int *ptrp;
u_int wc;
{
	if (wc < 0x80) buf[(*ptrp)++] = wc;
	else if (wc < 0x800) {
		buf[(*ptrp)++] = 0xc0 | (wc >> 6);
		buf[(*ptrp)++] = 0x80 | (wc & 0x3f);
	}
	else {
		buf[(*ptrp)++] = 0xe0 | (wc >> 12);
		buf[(*ptrp)++] = 0x80 | ((wc >> 6) & 0x3f);
		buf[(*ptrp)++] = 0x80 | (wc & 0x3f);
	}
}

static u_int NEAR ucs2fromutf8(s, ptrp)
u_char *s;
int *ptrp;
{
	u_int w;

	w = s[(*ptrp)++];
	if (w < 0x80);
	else if ((w & 0xe0) == 0xc0 && (s[(*ptrp)++] & 0xc0) == 0x80)
		w = ((w & 0x1f) << 6) | (s[(*ptrp)++] & 0x3f);
	else {
		w = ((w & 0x0f) << 6) | (s[(*ptrp)++] & 0x3f);
		w = (w << 6) | (s[(*ptrp)++] & 0x3f);
	}
	return(w);
}

static int NEAR toutf8(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j;

	i = j = 0;
	while (s[i] && j < max - 3) ucs2toutf8(buf, &j, toucs2(s, &i));
	cnvunicode(0, -1);
	return(j);
}

static int NEAR fromutf8(buf, s, max)
char *buf;
u_char *s;
int max;
{
	int i, j;

	i = j = 0;
	while (s[i] && j < max - 2) fromucs2(buf, &j, ucs2fromutf8(s, &i));
	cnvunicode(0, -1);
	return(j);
}

static int NEAR toutf8nf(buf, s, max, nf)
char *buf;
u_char *s;
int max, nf;
{
	u_int *u1, *u2;
	int i, j;

	u1 = (u_int *)malloc2((max + 1) * sizeof(u_int));
	u2 = (u_int *)malloc2((max + 1) * sizeof(u_int));

	for (i = j = 0; s[i] && j < max; j++) u1[j] = toucs2(s, &i);
	u1[j] = 0;
	for (i = j = 0; u1[i] && j < max; i++)
		ucs2normalization(u2, &j, max, u1[i], nf);
	u2[j] = 0;
	for (i = j = 0; u2[i] && j < max - 3; i++) ucs2toutf8(buf, &j, u2[i]);

	free(u1);
	free(u2);
	cnvunicode(0, -1);
	return(j);
}

static int NEAR fromutf8nf(buf, s, max, nf)
char *buf;
u_char *s;
int max, nf;
{
	u_int *u1, *u2;
	int i, j;

	u1 = (u_int *)malloc2((max + 1) * sizeof(u_int));
	u2 = (u_int *)malloc2((max + 1) * sizeof(u_int));

	for (i = j = 0; s[i] && j < max; j++) u1[j] = ucs2fromutf8(s, &i);
	u1[j] = 0;
	for (i = j = 0; u1[i] && j < max; j++)
		u2[j] = ucs2denormalization(u1, &i, nf);
	u2[j] = 0;
	for (i = j = 0; u2[i] && j < max - 2; i++) fromucs2(buf, &j, u2[i]);

	free(u1);
	free(u2);
	cnvunicode(0, -1);
	return(j);
}

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
# ifdef	CODEEUC
		if (iseuc(s[i]) && iseuc(s[i + 1])) {
			u_char tmp[2];

			j2sj((char *)tmp, &(s[i++]));
			j += bin2hex(&(buf[j]), tmp[0]);
			j += bin2hex(&(buf[j]), tmp[1]) - 1;
		}
		else if (isekana(s, i)) j += bin2hex(&(buf[j]), s[++i]) - 1;
# else
		if (issjis1(s[i]) && issjis2(s[i + 1])) {
			j += bin2hex(&(buf[j]), s[i++]);
			j += bin2hex(&(buf[j]), s[i]) - 1;
		}
		else if (isskana(s, i)) j += bin2hex(&(buf[j]), s[i]) - 1;
# endif
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
			if (iskana2(c1)) {
# ifdef	CODEEUC
				buf[j++] = (char)C_EKANA;
# endif
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
# ifdef	CODEEUC
					u_char tmp[2];

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
u_char *s;
int max;
{
	int i, j;

	for (i = j = 0; s[i] && j < max - 2; i++, j++) {
# ifdef	CODEEUC
		if (iseuc(s[i]) && iseuc(s[i + 1])) {
			u_char tmp[2];

			j2sj((char *)tmp, &(s[i++]));
			j += bin2cap(&(buf[j]), tmp[0]);
			j += bin2cap(&(buf[j]), tmp[1]) - 1;
		}
		else if (isekana(s, i)) j += bin2cap(&(buf[j]), s[++i]) - 1;
# else
		if (issjis1(s[i]) && issjis2(s[i + 1])) {
			j += bin2cap(&(buf[j]), s[i++]);
			j += bin2cap(&(buf[j]), s[i]) - 1;
		}
		else if (isskana(s, i)) j += bin2cap(&(buf[j]), s[i]) - 1;
# endif
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
			if (iskana2(c1)) {
# ifdef	CODEEUC
				buf[j++] = (char)C_EKANA;
# endif
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
# ifdef	CODEEUC
					u_char tmp[2];

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
				case O_JIS7:
				case JIS8:
				case O_JIS8:
				case JUNET:
				case O_JUNET:
					*lenp = fromjis(buf, (u_char *)s,
						max, io);
					break;
				case UTF8:
					*lenp = fromutf8(buf, (u_char *)s,
						max);
					break;
				case M_UTF8:
					*lenp = fromutf8nf(buf, (u_char *)s,
						max, NF_MAC);
					break;
				case HEX:
					*lenp = fromhex(buf, (u_char *)s, max);
					break;
				case CAP:
					*lenp = fromcap(buf, (u_char *)s, max);
					break;
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
		case M_UTF8:
			*lenp = toutf8nf(buf, (u_char *)s, max, NF_MAC);
			break;
		case HEX:
			*lenp = tohex(buf, (u_char *)s, max);
			break;
		case CAP:
			*lenp = tocap(buf, (u_char *)s, max);
			break;
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

char *kanjiconv2(buf, s, max, in, out, io)
char *buf, *s;
int max, in, out, io;
{
	int len;

	if (_kanjiconv(buf, s, max, in, out, &len, io) != buf) return(s);
	buf[len] = '\0';
	return(buf);
}

char *newkanjiconv(s, in, out, io)
char *s;
int in, out, io;
{
	char *buf;
	int len;

	if (!s) return(NULL);
	len = strlen(s) * 3 + 3;
	buf = malloc2(len + 1);
	if (kanjiconv2(buf, s, len, in, out, io) != buf) {
		free(buf);
		return(s);
	}
	return(buf);
}
#endif	/* !_NOKANJICONV */

#ifndef	_NOKANJIFCONV
int getkcode(path)
char *path;
{
	int i;

# if	defined (FD) && defined (_NODOSEMU)
	if (_dospath(path)) return(SJIS);
# endif
	for (i = 0; i < MAXKPATHLIST; i++) {
		if (includepath(path, *(kpathlist[i].path)))
			return(kpathlist[i].code);
	}
	return(fnamekcode);
}
#endif	/* !_NOKANJIFCONV */

/*ARGSUSED*/
char *convget(buf, path, dos)
char *buf, *path;
int dos;
{
#if	defined (FD) && !defined (_NOROCKRIDGE)
	char rbuf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	int fgetok;
#endif
	char *cp;

	if (noconv) return(path);
#ifndef	_NOKANJIFCONV
	fgetok = (nokanjifconv) ? 0 : 1;
#endif

	cp = path;
#if	defined (FD) && !defined (_NOROCKRIDGE)
	if (!norockridge) cp = transpath(cp, rbuf);
#endif
#if	defined (FD) && defined (_NODOSEMU)
	if (dos) {
# ifdef	CODEEUC
		buf[sjis2ujis(buf, (u_char *)cp, MAXPATHLEN - 1)] = '\0';
		cp = buf;
# endif
# ifndef	_NOKANJIFCONV
		fgetok = 0;
# endif
	}
#endif	/* FD && _USEDOSEMU */
#ifndef	_NOKANJIFCONV
	if (fgetok) cp = kanjiconv2(buf, cp,
		MAXPATHLEN - 1, getkcode(cp), DEFCODE, L_FNAME);
#endif
#if	defined (FD) && !defined (_NOROCKRIDGE)
	if (cp == rbuf) {
		strcpy(buf, rbuf);
		return(buf);
	}
#endif
	return(cp);
}

/*ARGSUSED*/
char *convput(buf, path, needfile, rdlink, rrreal, codep)
char *buf, *path;
int needfile, rdlink;
char *rrreal;
int *codep;
{
#if	!defined (_NOKANJIFCONV) || (defined (FD) && defined (_USEDOSEMU))
	char kbuf[MAXPATHLEN];
#endif
#if	defined (FD) && !defined (_NOROCKRIDGE)
	char rbuf[MAXPATHLEN];
#endif
#ifndef	_NOKANJIFCONV
	int fputok;
#endif
	char *cp, *file, rpath[MAXPATHLEN];
	int n;

	if (rrreal) *rrreal = '\0';
	if (codep) *codep = NOCNV;

	if (noconv || isdotdir(path)) {
#if	defined (FD) && defined (_USEDOSEMU)
		if (dospath(path, buf)) return(buf);
#endif
		return(path);
	}
#ifndef	_NOKANJIFCONV
	fputok = (nokanjifconv) ? 0 : 1;
#endif

	if (needfile && strdelim(path, 0)) needfile = 0;
#ifdef	FD
	if (norealpath) cp = path;
	else
#endif
	{
		if ((file = strrdelim(path, 0))) {
			n = file - path;
			if (file++ == isrootdir(path)) n++;
			strncpy2(rpath, path, n);
		}
#if	defined (FD) && defined (_USEDOSEMU)
		else if ((n = _dospath(path))) {
			file = path + 2;
			rpath[0] = n;
			rpath[1] = ':';
			rpath[2] = '.';
			rpath[3] = '\0';
		}
#endif
		else {
			file = path;
			strcpy(rpath, ".");
		}
		realpath2(rpath, rpath, 1);
		cp = strcatdelim(rpath);
		strncpy2(cp, file, MAXPATHLEN - 1 - (cp - rpath));
		cp = rpath;
	}
#if	defined (FD) && defined (_USEDOSEMU)
	if ((n = dospath(cp, kbuf))) {
		cp = kbuf;
		if (codep) *codep = SJIS;
# ifndef	_NOKANJIFCONV
		fputok = 0;
# endif
	}
#endif
#ifndef	_NOKANJIFCONV
	if (fputok) {
		int c;

		c = getkcode(cp);
		if (codep) *codep = c;
		cp = kanjiconv2(kbuf, cp, MAXPATHLEN - 1, DEFCODE, c, L_FNAME);
	}
#endif
#if	defined (FD) && !defined (_NOROCKRIDGE)
	if (!norockridge && (cp = detranspath(cp, rbuf)) == rbuf) {
		if (rrreal) strcpy(rrreal, rbuf);
		if (rdlink && rrreadlink(cp, buf, MAXPATHLEN - 1) >= 0) {
			if (needfile && strdelim(buf, 0)) needfile = 0;
			if (*buf == _SC_ || !(cp = strrdelim(rbuf, 0)))
				cp = buf;
			else {
				cp++;
				strncpy2(cp, buf, MAXPATHLEN - (cp - rbuf));
				cp = rbuf;
			}
			realpath2(cp, rpath, 1);
			cp = detranspath(rpath, rbuf);
		}
	}
#endif
	if (cp == path) return(path);
	if (needfile && (file = strrdelim(cp, 0))) file++;
	else file = cp;
#if	defined (FD) && defined (_USEDOSEMU)
	if (n && !_dospath(file)) {
		buf[0] = n;
		buf[1] = ':';
		strcpy(&(buf[2]), file);
	}
	else
#endif
	strcpy(buf, file);
	return(buf);
}
