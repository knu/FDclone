/*
 *	kctype.h
 *
 *	Kanji Code Type Macros
 */

#include <ctype.h>

#ifndef	issjis1
#define	issjis1(c)	((0x81 <= (c) && (c) <= 0x9f) \
			|| (0xe0 <= (c) && (c) <= 0xfc))
#endif

#ifndef	iseuc
#define	iseuc(c)	(0xa1 <= (c) && (c) <= 0xfe)
#endif

#define	isctl(c)	((u_char)(c) < ' ' || (c) == C_DEL)
#define	iskna(c)	((u_char)(c) >= 0xa1 && (u_char)(c) <= 0xdf)
#define	isekana(str, i)	((u_char)(str[i]) == 0x8e && iskna(str[(i) + 1]))
#define	isskana(str, i)	iskna(str[i])

#define	NOCNV	0
#define	ENG	1
#define	SJIS	2
#define	EUC	3
#define	JIS7	4

#ifdef	CODEEUC
#define	DEFCODE	EUC
#define	KANAWID	2
#else
#define	DEFCODE	SJIS
#define	KANAWID	1
#endif

#if	!MSDOS && !defined (_NOKANJICONV)
extern int inputkcode;
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
extern int outputkcode;
#endif

#if	MSDOS
#define	iskanji1(c)	issjis1((u_char)(c))
#define	isinkanji1(c)	issjis1((u_char)(c))
#else	/* !MSDOS */
#ifdef	CODEEUC
#define	iskanji1(c)	iseuc((u_char)(c))
#else
#define	iskanji1(c)	issjis1((u_char)(c))
#endif

#ifdef	_NOKANJICONV
#define	isinkanji1(c)	iskanji1(c)
#else
#define	isinkanji1(c)	((inputkcode == EUC) \
			? iseuc((u_char)(c)) : issjis1((u_char)(c)))
#endif
#endif	/* !MSDOS */
