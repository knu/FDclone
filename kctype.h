/*
 *	Kanji Code Type Macros
 */

#include <ctype.h>

#ifndef	issjis1
#define	issjis1(c)	((0x81 <= (c) && (c) <= 0x9f)\
			|| (0xe0 <= (c) && (c) <= 0xfc))
#endif

#ifndef	iseuc
#define	iseuc(c)	(0xa1 <= (c) && (c) <= 0xfe)
#endif

extern int inputkcode;
extern int outputkcode;

#define	NOCNV	0
#define	ENG	1
#define	JIS7	2
#define	SJIS	3
#define	EUC	4

#ifdef	CODEEUC
#define	DEFCODE	EUC
#define	iskanji1	iseuc
#else
#define	DEFCODE	SJIS
#define	iskanji1	issjis1
#endif

#define	isinkanji1(c)	((inputkcode == EUC) ? iseuc(c) : issjis1(c))
#define	isekana(str, i)	(str[i] == 0x8e && str[++(i)])
#define	isskana(str, i)	(str[i] >= 0xa1 && str[i] <= 0xdf)
