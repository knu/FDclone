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
#define	JKANA		004
#define	MAXPRINTBUF	1023
#define	SJ_UDEF		0x81ac	/* GETA */

typedef struct _kconv_t {
	u_short start;
	u_short cnv;
	u_short range;
} kconv_t;

extern char *malloc2 __P_((ALLOC_T));
extern char *strncpy3 __P_((char *, char *, int *, int));
extern char *strstr2 __P_((char *, char *));

#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
static VOID NEAR sj2j __P_((char *, u_char *));
static VOID NEAR j2sj __P_((char *, u_char *));
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
static int NEAR toenglish __P_((char *, u_char *, int));
static int NEAR tojis7 __P_((char *, u_char *, int, int, int));
static int NEAR fromjis7 __P_((char *, u_char *, int, int, int));
#endif	/* !MSDOS && !_NOKANJICONV */
#if	!MSDOS && !defined (_NOKANJIFCONV)
static int NEAR tojis8 __P_((char *, u_char *, int, int, int));
static int NEAR fromjis8 __P_((char *, u_char *, int, int, int));
static int NEAR tojunet __P_((char *, u_char *, int, int, int));
static int NEAR fromjunet __P_((char *, u_char *, int, int, int));
static int NEAR bin2hex __P_((char *, int));
static int NEAR tohex __P_((char *, u_char *, int));
static int NEAR fromhex __P_((char *, u_char *, int));
static int NEAR bin2cap __P_((char *, int));
static int NEAR tocap __P_((char *, u_char *, int));
static int NEAR fromcap __P_((char *, u_char *, int));
#endif	/* !MSDOS && !_NOKANJIFCONV */
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
#if	!MSDOS && !defined (_NOKANJIKCONV)
int fnamekcode = 0;
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
	{0xfa40, 0xeeef, 0x0a},
	{0xfa4a, 0x8754, 0x0a},
	{0xfa54, 0x81ca, 0x01},
	{0xfa55, 0xeefa, 0x03},
	{0xfa58, 0x878a, 0x01},
	{0xfa59, 0x8782, 0x01},
	{0xfa5a, 0x8784, 0x01},
	{0xfa5b, 0x81e6, 0x01},
	{0xfa5c, 0xed40, 0x23},
	{0xfa80, 0xed63, 0x1c},
	{0xfa9c, 0xed80, 0x61},
	{0xfb40, 0xede1, 0x1c},
	{0xfb5c, 0xee40, 0x23},
	{0xfb80, 0xee63, 0x1c},
	{0xfb9c, 0xee80, 0x61},
	{0xfc40, 0xeee1, 0x0c}
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
#if	!MSDOS && !defined (_NOKANJIFCONV)
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
#endif	/* !MSDOS && !_NOKANJIFCONV */

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
#if	!MSDOS && !defined (_NOKANJIFCONV)
char *kanjiconv2 __P_((char *, char *, int, int, int));
#endif
int kanjifputs __P_((char *, FILE *));
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
# if	!MSDOS && !defined (_NOKANJIFCONV)
	else if (io == L_FNAME && (strstr2(s, "HEX") || strstr2(s, "hex")))
		ret = HEX;
	else if (io == L_FNAME && (strstr2(s, "CAP") || strstr2(s, "cap")))
		ret = CAP;
# endif	/* !MSDOS && !_NOKANJIFCONV */
# if	!MSDOS && !defined (_NOKANJICONV)
	else if (strstr2(s, "SJIS") || strstr2(s, "sjis")) ret = SJIS;
	else if (strstr2(s, "EUC") || strstr2(s, "euc")
	|| strstr2(s, "UJIS") || strstr2(s, "ujis")) ret = EUC;
# ifndef	_NOKANJIFCONV
	else if (strstr2(s, "OJUNET") || strstr2(s, "ojunet"))
		ret = O_JUNET;
	else if (strstr2(s, "OJIS8") || strstr2(s, "ojis8")) ret = O_JIS8;
	else if (strstr2(s, "OJIS") || strstr2(s, "ojis")) ret = O_JIS7;
	else if (strstr2(s, "JUNET") || strstr2(s, "junet")) ret = JUNET;
	else if (strstr2(s, "JIS8") || strstr2(s, "jis8")) ret = JIS8;
# endif	/* !_NOKANJIFCONV */
	else if (strstr2(s, "JIS") || strstr2(s, "jis")) ret = JIS7;
# endif	/* !MSDOS && !_NOKANJICONV */
# ifndef	_NOENGMES
	else if (io == L_OUTPUT
	&& (strstr2(s, "ENG") || strstr2(s, "eng")
	|| !strcmp(s, "C"))) ret = ENG;
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
		else if (iseuc(s[i])) j2sj(&(buf[j++]), &(s[i++]));
		else buf[j] = s[i];
	}
	return(j);
}
#endif	/* !MSDOS && (!_NOKANJICONV || (!_NODOSDRIVE && CODEEUC)) */

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

static int NEAR tojis7(buf, s, max, knj, asc)
char *buf;
u_char *s;
int max, knj, asc;
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

static int NEAR fromjis7(buf, s, max, knj, asc)
char *buf;
u_char *s;
int max, knj, asc;
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
			if (s[i + 1] == '$' && s[i + 2] == knj) {
				mode |= KANJI;
				i += 2;
				break;
			}
			else if (s[i + 1] == '(' && s[i + 2] == asc) {
				mode &= ~KANJI;
				i += 2;
				break;
			}
		default:
			if (mode & KANA) {
				if (!isjkana(s, i)) buf[j++] = s[i];
				else {
# ifdef	CODEEUC
					buf[j++] = 0x8e;
# endif
					buf[j++] = s[i] | 0x80;
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
					buf[j++] = s[i] | 0x80;
# else
					u_char tmp[2];

					tmp[0] = s[i++];
					tmp[1] = s[i];
					j2sj(&(buf[j]), tmp);
					j += 2;
# endif
				}
			}
			else buf[j++] = s[i];
			break;
	}
	return(j);
}
#endif	/* !MSDOS && !_NOKANJICONV */

#if	!MSDOS && !defined (_NOKANJIFCONV)
static int NEAR tojis8(buf, s, max, knj, asc)
char *buf;
u_char *s;
int max, knj, asc;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 7; i++, j++) {
# ifdef	CODEEUC
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
# endif
		if (iskanji1(s, i)) {
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

static int NEAR fromjis8(buf, s, max, knj, asc)
char *buf;
u_char *s;
int max, knj, asc;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 1; i++)
	switch (s[i]) {
		case '\033':	/* ESC */
			if (s[i + 1] == '$' && s[i + 2] == knj) {
				mode = KANJI;
				i += 2;
				break;
			}
			else if (s[i + 1] == '(' && s[i + 2] == asc) {
				mode = ASCII;
				i += 2;
				break;
			}
		default:
			if (mode & KANJI) {
				if (!isjis(s[i]) || !isjis(s[i + 1])) {
					buf[j++] = s[i++];
					buf[j++] = s[i];
				}
				else {
# ifdef	CODEEUC
					buf[j++] = s[i++] | 0x80;
					buf[j++] = s[i] | 0x80;
# else
					u_char tmp[2];

					tmp[0] = s[i++];
					tmp[1] = s[i];
					j2sj(&(buf[j]), tmp);
					j += 2;
# endif
				}
			}
# ifdef	CODEEUC
			else if (iskna(s[i])) {
				buf[j++] = 0x8e;
				buf[j++] = s[i];
			}
# endif
			else buf[j++] = s[i];
			break;
	}
	return(j);
}

static int NEAR tojunet(buf, s, max, knj, asc)
char *buf;
u_char *s;
int max, knj, asc;
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
			if (mode != JKANA) {
				buf[j++] = '\033';
				buf[j++] = '(';
				buf[j++] = 'I';
			}
			mode = JKANA;
			buf[j] = s[i] & ~0x80;
		}
		else if (iskanji1(s, i)) {
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

static int NEAR fromjunet(buf, s, max, knj, asc)
char *buf;
u_char *s;
int max, knj, asc;
{
	int i, j, mode;

	mode = ASCII;
	for (i = j = 0; s[i] && j < max - 1; i++)
	switch (s[i]) {
		case '\033':	/* ESC */
			if (s[i + 1] == '$' && s[i + 2] == knj) {
				mode = KANJI;
				i += 2;
				break;
			}
			else if (s[i + 1] == '(') {
				if (s[i + 2] == 'I') {
					mode = JKANA;
					i += 2;
					break;
				}
				else if (s[i + 2] == asc) {
					mode = ASCII;
					i += 2;
					break;
				}
			}
		default:
			if (mode == JKANA) {
				if (!isjkana(s, i)) buf[j++] = s[i];
				else {
# ifdef	CODEEUC
					buf[j++] = 0x8e;
# endif
					buf[j++] = s[i] | 0x80;
				}
			}
			else if (mode == KANJI) {
				if (!isjis(s[i]) || !isjis(s[i + 1])) {
					buf[j++] = s[i++];
					buf[j++] = s[i];
				}
				else {
# ifdef	CODEEUC
					buf[j++] = s[i++] | 0x80;
					buf[j++] = s[i] | 0x80;
# else
					u_char tmp[2];

					tmp[0] = s[i];
					tmp[1] = s[i];
					j2sj(&(buf[j]), tmp);
					j += 2;
# endif
				}
			}
			else buf[j++] = s[i];
			break;
	}
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
		if (isekana(s, i)) j += bin2hex(&(buf[j]), s[++i]) - 1;
		else if (iseuc(s[i])) {
			u_char tmp[2];

			j2sj(tmp, &(s[i++]));
			j += bin2hex(&(buf[j]), tmp[0]);
			j += bin2hex(&(buf[j]), tmp[1]) - 1;
		}
# else
		if (isskana(s, i)) j += bin2hex(&(buf[j]), s[i]) - 1;
		else if (issjis1(s[i]) && issjis2(s[i + 1])) {
			j += bin2hex(&(buf[j]), s[i++]);
			j += bin2hex(&(buf[j]), s[i]) - 1;
		}
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
			if (iskna(c1)) {
# ifdef	CODEEUC
				buf[j++] = 0x8e;
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
		if (isekana(s, i)) j += bin2cap(&(buf[j]), s[++i]) - 1;
		else if (iseuc(s[i])) {
			u_char tmp[2];

			j2sj(tmp, &(s[i++]));
			j += bin2cap(&(buf[j]), tmp[0]);
			j += bin2cap(&(buf[j]), tmp[1]) - 1;
		}
# else
		if (isskana(s, i)) j += bin2cap(&(buf[j]), s[i]) - 1;
		else if (issjis1(s[i]) && issjis2(s[i + 1])) {
			j += bin2cap(&(buf[j]), s[i++]);
			j += bin2cap(&(buf[j]), s[i]) - 1;
		}
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
			if (iskna(c1)) {
# ifdef	CODEEUC
				buf[j++] = 0x8e;
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
#endif	/* !MSDOS && !_NOKANJIFCONV */

#if	!MSDOS && !defined (_NOKANJICONV)
static char *NEAR _kanjiconv(buf, s, max, in, out, lenp)
char *buf, *s;
int max, in, out, *lenp;
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
					*lenp = fromjis7(buf, (u_char *)s,
						max, 'B', 'B');
					break;
# if	!MSDOS && !defined (_NOKANJIFCONV)
				case O_JIS7:
					*lenp = fromjis7(buf, (u_char *)s,
						max, '@', 'J');
					break;
				case JIS8:
					*lenp = fromjis8(buf, (u_char *)s,
						max, 'B', 'B');
					break;
				case O_JIS8:
					*lenp = fromjis8(buf, (u_char *)s,
						max, '@', 'J');
					break;
				case JUNET:
					*lenp = fromjunet(buf, (u_char *)s,
						max, 'B', 'B');
					break;
				case O_JUNET:
					*lenp = fromjunet(buf, (u_char *)s,
						max, '@', 'J');
					break;
				case HEX:
					*lenp = fromhex(buf, (u_char *)s, max);
					break;
				case CAP:
					*lenp = fromcap(buf, (u_char *)s, max);
					break;
# endif	/* !MSDOS && !_NOKANJIFCONV */
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
			*lenp = tojis7(buf, (u_char *)s, max, 'B', 'B');
			break;
# if	!MSDOS && !defined (_NOKANJIFCONV)
		case O_JIS7:
			*lenp = tojis7(buf, (u_char *)s, max, '@', 'J');
			break;
		case JIS8:
			*lenp = tojis8(buf, (u_char *)s, max, 'B', 'B');
			break;
		case O_JIS8:
			*lenp = tojis8(buf, (u_char *)s, max, '@', 'J');
			break;
		case JUNET:
			*lenp = tojunet(buf, (u_char *)s, max, 'B', 'B');
			break;
		case O_JUNET:
			*lenp = tojunet(buf, (u_char *)s, max, '@', 'J');
			break;
		case HEX:
			*lenp = tohex(buf, (u_char *)s, max);
			break;
		case CAP:
			*lenp = tocap(buf, (u_char *)s, max);
			break;
# endif	/* !MSDOS && !_NOKANJIFCONV */
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

#if	!MSDOS && !defined (_NOKANJIFCONV)
char *kanjiconv2(buf, s, max, in, out)
char *buf, *s;
int max, in, out;
{
	char *cp;
	int len;

	if ((cp = _kanjiconv(buf, s, max, in, out, &len)) == buf)
		cp[len] = '\0';
	return(cp);
}
#endif	/* !MSDOS && !_NOKANJIFCONV */

int kanjifputs(s, fp)
char *s;
FILE *fp;
{
#if	MSDOS || defined (_NOKANJICONV)
	return(fputs(s, fp));
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
