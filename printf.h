/*
 *	printf.h
 *
 *	Definition for "printf.c"
 */

#ifndef	__PRINTF_H_
#define	__PRINTF_H_

#ifdef	USESTDARGH
#include <stdarg.h>
#else
#include <varargs.h>
#endif

typedef struct _printbuf_t {
	char *buf;
	int ptr;
	int size;
	int flags;
} printbuf_t;

#define	VF_NEW		000001
#define	VF_FILE		000002
#define	VF_KANJI	000004
#define	VF_PLUS		000010
#define	VF_MINUS	000020
#define	VF_SPACE	000040
#define	VF_ZERO		000100
#define	VF_LONG		000200
#define	VF_OFFT		000400
#define	VF_PIDT		001000
#define	VF_LOFFT	002000
#define	VF_UNSIGNED	004000
#define	VF_THOUSAND	010000
#define	VF_STRICTWIDTH	020000

#define	MAXLONGWIDTH	20		/* log10(2^64) = 19.266 */
#define	MAXCOLSCOMMA(d)	(MAXLONGWIDTH + (MAXLONGWIDTH / (d)))

extern int setchar __P_((int, printbuf_t *));
extern int vasprintf2 __P_((char **, CONST char *, va_list));
extern int asprintf2 __P_((char **, CONST char *, ...));
extern int snprintf2 __P_((char *, int, CONST char *, ...));
extern int fprintf2 __P_((FILE *, CONST char *, ...));
extern VOID fputnl __P_((FILE *));
#ifdef	FD
extern VOID kanjifputs __P_((char *, FILE *));
#else
#define	kanjifputs	fputs
#endif

#endif	/* __PRINTF_H_ */
