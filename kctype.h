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

#ifdef	CODEEUC
#define	iskanji1	iseuc
#else
#define	iskanji1	issjis1
#endif
