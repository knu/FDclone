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
#define	issjis1		iskanji
#define	issjis2		iskanji2
#define	isctl		iscntrl
#define	iskna		iskana
#else	/* !LSI_C */
extern u_char kctypetable[256];
#define	KC_SJIS1	0001
#define	KC_SJIS2	0002
#define	KC_KANA		0004
#define	KC_EUCJP	0010
#define	KC_JIS		0020
#define	KC_JKANA	0040
#define	KC_CNTRL	0200
#define	isctl(c)	(kctypetable[(u_char)(c)] & KC_CNTRL)
#define	iskna(c)	(kctypetable[(u_char)(c)] & KC_KANA)

# ifndef	issjis1
# define	issjis1(c)	(kctypetable[(u_char)(c)] & KC_SJIS1)
# endif
# ifndef	issjis2
# define	issjis2(c)	(kctypetable[(u_char)(c)] & KC_SJIS2)
# endif

# ifndef	iseuc
# define	iseuc(c)	(kctypetable[(u_char)(c)] & KC_EUCJP)
# endif

# ifndef	isjis
# define	isjis(c)	(kctypetable[(u_char)(c)] & KC_JIS)
# endif
#endif	/* !LSI_C */

#define	isekana(s, i)	((u_char)((s)[i]) == 0x8e && iskna((s)[(i) + 1]))
#define	isskana(s, i)	iskna((s)[i])
#define	isjkana(s, i)	(kctypetable[(u_char)((s)[i])] & KC_JKANA)

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
#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
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
