/*
 *	kctype.h
 *
 *	Kanji Code Type Macros
 */

#include <ctype.h>

#define	QUOTE		('^' - '@')
#define	C_BS		'\010'
#define	C_DEL		'\177'

#ifdef	LSI_C
#include <jctype.h>
#define	issjis1(c)	iskanji(c)
#define	issjis2(c)	iskanji2(c)
#define	isctl(c)	iscntrl(c)
#define	iskna(c)	iskana(c)
#else
#define	isctl(c)	((u_char)(c) < ' ' || (u_char)(c) == C_DEL)
#define	iskna(c)	((u_char)(c) >= 0xa1 && (u_char)(c) <= 0xdf)
#endif

#ifndef	issjis1
#define	issjis1(c)	(((u_char)(c) >= 0x81 && (u_char)(c) <= 0x9f) \
			|| ((u_char)(c) >= 0xe0 && (u_char)(c) <= 0xfc))
#endif
#ifndef	issjis2
#define	issjis2(c)	((u_char)(c) >= 0x40 && (u_char)(c) <= 0xfc \
			&& (u_char)(c) != 0x7f)
#endif

#ifndef	iseuc
#define	iseuc(c)	((u_char)(c) >= 0xa1 && (u_char)(c) <= 0xfe)
#endif

#define	isekana(s, i)	((u_char)((s)[i]) == 0x8e && iskna((s)[(i) + 1]))
#define	isskana(s, i)	iskna((s)[i])

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

#define	L_INPUT		0
#define	L_OUTPUT	1

#ifdef	CODEEUC
#define	iskanji1(s, i)	(iseuc((s)[i]) && iseuc((s)[(i) + 1]))
#else
#define	iskanji1(s, i)	(issjis1((s)[i]) && issjis2((s)[(i) + 1]))
#endif

#if	MSDOS
#define	isinkanji1(c)	issjis1(c)
#else	/* !MSDOS */

#ifdef	_NOKANJICONV
# ifdef	CODEEUC
# define	isinkanji1(c)	iseuc(c)
# else
# define	isinkanji1(c)	issjis1(c)
# endif
#else	/* !_NOKANJICONV */
#define	isinkanji1(c)	((inputkcode == EUC) ? iseuc(c) : issjis1(c))
#endif	/* !_NOKANJICONV */
#endif	/* !MSDOS */
