/*
 *	kctype.h
 *
 *	Kanji code type macros
 */

#ifndef	__KCTYPE_H_
#define	__KCTYPE_H_

#include <ctype.h>

#ifndef	__SYS_TYPES_STAT_H_
#define	__SYS_TYPES_STAT_H_
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef	K_EXTERN
#define	K_INTERN
#define	K_INIT(x)		=x
#else
#define	K_EXTERN		extern
#define	K_INIT(x)
#endif

#define	C_BS			'\010'
#define	C_DEL			'\177'
#define	C_EKANA			0x8e

#ifdef	LSI_C
#include <jctype.h>
#define	toupper2(c)		toupper((u_char)(c))
#define	tolower2(c)		tolower((u_char)(c))
#define	isalnum2(c)		isalnum((u_char)(c))
#define	isalpha2(c)		isalpha((u_char)(c))
#define	iscntrl2(c)		iscntrl((u_char)(c))
#define	isdigit2(c)		isdigit((u_char)(c))
#define	isupper2(c)		isupper((u_char)(c))
#define	islower2(c)		islower((u_char)(c))
#define	isxdigit2(c)		isxdigit((u_char)(c))
#define	issjis1(c)		iskanji((u_char)(c))
#define	issjis2(c)		iskanji2((u_char)(c))
#define	iskana2(c)		iskana((u_char)(c))
#define	isspace2(c)		isspace((u_char)(c))
#define	isprint2(c)		isprint((u_char)(c))
#define	isblank2(c)		((c) == ' ' || (c) == '\t')
#else	/* !LSI_C */
K_EXTERN CONST u_char uppercase[256]
#ifdef	K_INTERN
= {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* 0x00 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,	/* 0x10 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x40 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x50 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x60 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x70 */
	0x58, 0x59, 0x5a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,	/* 0x80 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,	/* 0x90 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,	/* 0xa0 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,	/* 0xb0 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,	/* 0xc0 */
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,	/* 0xd0 */
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,	/* 0xe0 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,	/* 0xf0 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
}
#endif	/* K_INTERN */
;
K_EXTERN CONST u_char lowercase[256]
#ifdef	K_INTERN
= {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* 0x00 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,	/* 0x10 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x40 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x50 */
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x60 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x70 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,	/* 0x80 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,	/* 0x90 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,	/* 0xa0 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,	/* 0xb0 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,	/* 0xc0 */
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,	/* 0xd0 */
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,	/* 0xe0 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,	/* 0xf0 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
}
#endif	/* K_INTERN */
;
K_EXTERN CONST u_char ctypetable[256]
#ifdef	K_INTERN
= {
	0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,	/* 0x00 */
	0020, 0260, 0060, 0060, 0060, 0060, 0020, 0020,
	0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,	/* 0x10 */
	0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
	0340, 0100, 0100, 0100, 0100, 0100, 0100, 0100,	/* 0x20 */
	0100, 0100, 0100, 0100, 0100, 0100, 0100, 0100,
	0101, 0101, 0101, 0101, 0101, 0101, 0101, 0101,	/* 0x30 */
	0111, 0101, 0100, 0100, 0100, 0100, 0100, 0100,
	0100, 0112, 0112, 0112, 0112, 0112, 0112, 0102,	/* 0x40 */
	0102, 0102, 0102, 0102, 0102, 0102, 0102, 0102,
	0102, 0102, 0102, 0102, 0102, 0102, 0102, 0102,	/* 0x50 */
	0102, 0102, 0102, 0100, 0100, 0100, 0100, 0100,
	0100, 0114, 0114, 0114, 0114, 0114, 0114, 0104,	/* 0x60 */
	0104, 0104, 0104, 0104, 0104, 0104, 0104, 0104,
	0104, 0104, 0104, 0104, 0104, 0104, 0104, 0104,	/* 0x70 */
	0104, 0104, 0104, 0100, 0100, 0100, 0100, 0020,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x80 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x90 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xa0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xb0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xc0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xd0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xe0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xf0 */
}
#endif	/* K_INTERN */
;
K_EXTERN CONST u_char kctypetable[256]
#ifdef	K_INTERN
= {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
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
	0022, 0022, 0022, 0022, 0022, 0022, 0022,    0,
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
}
#endif	/* K_INTERN */
;
#define	toupper2(c)		(uppercase[(u_char)(c)])
#define	tolower2(c)		(lowercase[(u_char)(c)])
#define	C_DIGIT			0001
#define	C_UPPER			0002
#define	C_LOWER			0004
#define	C_HEX			0010
#define	C_CNTRL			0020
#define	C_SPACE			0040
#define	C_PRINT			0100
#define	C_BLANK			0200
#define	isalnum2(c)		(ctypetable[(u_char)(c)] \
				& (C_DIGIT | C_UPPER | C_LOWER))
#define	isalpha2(c)		(ctypetable[(u_char)(c)] & (C_UPPER | C_LOWER))
#define	iscntrl2(c)		(ctypetable[(u_char)(c)] & C_CNTRL)
#define	isdigit2(c)		(ctypetable[(u_char)(c)] & C_DIGIT)
#define	isupper2(c)		(ctypetable[(u_char)(c)] & C_UPPER)
#define	islower2(c)		(ctypetable[(u_char)(c)] & C_LOWER)
#define	isxdigit2(c)		(ctypetable[(u_char)(c)] & (C_DIGIT | C_HEX))
#define	isspace2(c)		(ctypetable[(u_char)(c)] & C_SPACE)
#define	isprint2(c)		(ctypetable[(u_char)(c)] & C_PRINT)
#define	isblank2(c)		(ctypetable[(u_char)(c)] & C_BLANK)
#define	KC_SJIS1		0001
#define	KC_SJIS2		0002
#define	KC_KANA			0004
#define	KC_EUCJP		0010
#define	KC_JIS			0020
#define	KC_JKANA		0040
#define	iskana2(c)		(kctypetable[(u_char)(c)] & KC_KANA)
#ifndef	issjis1
#define	issjis1(c)		(kctypetable[(u_char)(c)] & KC_SJIS1)
#endif
#ifndef	issjis2
#define	issjis2(c)		(kctypetable[(u_char)(c)] & KC_SJIS2)
#endif
#ifndef	iseuc
#define	iseuc(c)		(kctypetable[(u_char)(c)] & KC_EUCJP)
#endif
#ifndef	isjis
#define	isjis(c)		(kctypetable[(u_char)(c)] & KC_JIS)
#endif
#endif	/* !LSI_C */

#define	ismsb(c)		(((u_char)(c)) & 0x80)
#define	isekana(s, i)		((u_char)((s)[i]) == C_EKANA \
				&& iskana2((s)[(i) + 1]))
#define	isskana(s, i)		iskana2((s)[i])
#define	isjkana(s, i)		(kctypetable[(u_char)((s)[i])] & KC_JKANA)

#define	isutf2(c1, c2)		((((u_char)(c1) & 0xe0) == 0xc0) \
				&& ((u_char)(c2) & 0xc0) == 0x80)
#define	isutf3(c1, c2, c3) \
				(((u_char)(c1) & 0xf0) == 0xe0 \
				&& ((u_char)(c2) & 0xc0) == 0x80 \
				&& ((u_char)(c3) & 0xc0) == 0x80)
#define	iswucs2(u)		(((u) & 0xff00) \
				&& ((u) < 0xff61 || (u) > 0xff9f))

#define	NOCNV			0
#define	ENG			1
#define	SJIS			2
#define	EUC			3
#define	JIS7			4
#define	O_JIS7			5
#define	JIS8			6
#define	O_JIS8			7
#define	JUNET			8
#define	O_JUNET			9
#define	HEX			10
#define	CAP			11
#define	UTF8			12
#define	M_UTF8			13
#define	I_UTF8			14

#ifdef	CODEEUC
#define	DEFCODE			EUC
#define	SECCODE			SJIS
#define	KANAWID			2
#else
#define	DEFCODE			SJIS
#define	SECCODE			EUC
#define	KANAWID			1
#endif

#define	MAXKANJIBUF		(3 + 2 + 3)
#define	MAXKLEN			2
#define	MAXUTF8LEN		3
#define	MAXNFLEN		4

#ifdef	NOMULTIKANJI
#define	_NOKANJICONV
#define	_NOKANJIFCONV
#endif

#if	defined (_NOKANJICONV) && !defined (_NOKANJIFCONV)
#define	_NOKANJIFCONV
#endif

#if	!defined (_NOKANJICONV) || (defined (FD) && !defined (_NODOSDRIVE))
K_EXTERN int kanjierrno K_INIT(0);
#endif
#ifndef	_NOKANJIFCONV
K_EXTERN int defaultkcode K_INIT(NOCNV);
#endif
#ifdef	_NOKANJICONV
#define	inputkcode		NOCNV
#else
K_EXTERN int inputkcode K_INIT(NOCNV);
#endif
#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
K_EXTERN int outputkcode K_INIT(NOCNV);
#endif
#if	!defined (_NOKANJICONV) && !defined (_NOPTY)
K_EXTERN int ptyinkcode K_INIT(NOCNV);
K_EXTERN int ptyoutkcode K_INIT(NOCNV);
#endif
#ifndef	_NOKANJIFCONV
K_EXTERN int fnamekcode K_INIT(NOCNV);
#endif
#if	!defined (_NOENGMES) && !defined (_NOJPNMES)
K_EXTERN int messagelang K_INIT(NOCNV);
#endif

#define	L_INPUT			0001
#define	L_OUTPUT		0002
#define	L_FNAME			0004
#define	L_TERMINAL		0010
#define	L_MESSAGE		0020

K_EXTERN CONST char kanjiiomode[]
# ifdef	K_INTERN
= {
	          L_OUTPUT | L_FNAME | L_TERMINAL | L_MESSAGE,	/* NOCNV */
	          L_OUTPUT                        | L_MESSAGE,	/* ENG */
	L_INPUT | L_OUTPUT | L_FNAME | L_TERMINAL,		/* SJIS */
	L_INPUT | L_OUTPUT | L_FNAME | L_TERMINAL,		/* EUC */
	          L_OUTPUT | L_FNAME,				/* JIS7 */
	          L_OUTPUT | L_FNAME,				/* O_JIS7 */
	          L_OUTPUT | L_FNAME,				/* JIS8 */
	          L_OUTPUT | L_FNAME,				/* O_JIS8 */
	          L_OUTPUT | L_FNAME,				/* JUNET */
	          L_OUTPUT | L_FNAME,				/* O_JUNET */
	                     L_FNAME,				/* HEX */
	                     L_FNAME,				/* CAP */
	L_INPUT | L_OUTPUT | L_FNAME | L_TERMINAL,		/* UTF8 */
	L_INPUT | L_OUTPUT | L_FNAME | L_TERMINAL,		/* M_UTF8 */
	L_INPUT | L_OUTPUT | L_FNAME | L_TERMINAL,		/* I_UTF8 */
}
# endif	/* K_INTERN */
;

K_EXTERN int iskanji1 __P_((CONST char *, int));
#ifdef	K_INTERN
int iskanji1(s, i)
CONST char *s;
int i;
{
# ifdef	CODEEUC
	return(iseuc(s[i]) && iseuc(s[++i]));
# else
	return(issjis1(s[i]) && issjis2(s[++i]));
# endif
}
#endif	/* K_INTERN */

K_EXTERN int iskana1 __P_((CONST char *, int *));
#ifdef	K_INTERN
int iskana1(s, ip)
CONST char *s;
int *ip;
{
# ifdef	CODEEUC
	int n;

	if ((n = isekana(s, *ip))) (*ip)++;
	return(n);
# else
	return(isskana(s, *ip));
# endif
}
#endif	/* K_INTERN */

#ifdef	_NOKANJICONV
# ifdef	CODEEUC
# define	isinkanji1(c,k)	iseuc(c)
# define	isinkanji2(c,k)	iseuc(c)
# else
# define	isinkanji1(c,k)	issjis1(c)
# define	isinkanji2(c,k)	issjis2(c)
# endif
#else	/* !_NOKANJICONV */
#define	isinkanji1(c,k)		(((k) == EUC) ? iseuc(c) : \
				(((k) == SJIS) ? issjis1(c) : 0))
#define	isinkanji2(c,k)		(((k) == EUC) ? iseuc(c) : \
				(((k) == SJIS) ? issjis2(c) : 0))
#endif	/* !_NOKANJICONV */

#endif	/* !__KCTYPE_H_ */
