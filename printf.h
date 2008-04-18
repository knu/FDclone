/*
 *	printf.h
 *
 *	definitions & function prototype declarations for "printf.c"
 */

#ifndef	__PRINTF_H_
#define	__PRINTF_H_

#ifndef	__SYS_TYPES_STAT_H_
#define	__SYS_TYPES_STAT_H_
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef	USESTDARGH
#include <stdarg.h>
#define	VA_START(a, f)		va_start(a, f)
#else
#include <varargs.h>
#define	VA_START(a, f)		va_start(a)
#endif

typedef struct _printbuf_t {
	char *buf;
	int ptr;
	int size;
	u_short flags;
} printbuf_t;

#define	VF_NEW			000001
#define	VF_FILE			000002
#define	VF_KANJI		000004
#define	VF_PLUS			000010
#define	VF_MINUS		000020
#define	VF_SPACE		000040
#define	VF_ZERO			000100
#define	VF_THOUSAND		000200
#define	VF_STRICTWIDTH		000400
#define	VF_UNSIGNED		001000

#define	strsize(s)		((int)sizeof(s) - 1)
#define	arraysize(a)		((int)((u_int)sizeof(a) / (u_int)sizeof(*(a))))

#define	MAXLONGWIDTH		20		/* log10(2^64) = 19.266 */
#define	MAXCOLSCOMMA(d)		(MAXLONGWIDTH + (MAXLONGWIDTH / (d)))
#ifndef	BITSPERBYTE
# ifdef	CHAR_BIT
# define	BITSPERBYTE	CHAR_BIT
# else	/* !CHAR_BIT */
#  ifdef	NBBY
#  define	BITSPERBYTE	NBBY
#  else
#  define	BITSPERBYTE	8
#  endif
# endif	/* !CHAR_BIT */
#endif	/* !BITSPERBYTE */
#define	MAXUTYPE(t)		((t)(~(t)0))
#define	MINTYPE(t)		((t)(MAXUTYPE(t) << \
				(BITSPERBYTE * sizeof(t) - 1)))
#define	MAXTYPE(t)		((t)~MINTYPE(t))

#ifdef	HAVELONGLONG
typedef long long		long_t;
typedef unsigned long long	u_long_t;
#else
typedef long			long_t;
typedef unsigned long		u_long_t;
#endif

extern CONST char printfflagchar[];
extern CONST int printfflag[];
extern CONST char printfsizechar[];
extern CONST int printfsize[];

extern int getnum __P_((CONST char *, int *));
extern int setchar __P_((int, printbuf_t *));
extern int vasprintf2 __P_((char **, CONST char *, va_list));
extern int asprintf2 __P_((char **, CONST char *, ...));
extern int vsnprintf2 __P_((char *, int, CONST char *, va_list));
extern int snprintf2 __P_((char *, int, CONST char *, ...));
extern int fprintf2 __P_((FILE *, CONST char *, ...));
extern VOID fputnl __P_((FILE *));
#ifdef	FD
extern VOID kanjifputs __P_((CONST char *, FILE *));
#else
#define	kanjifputs		fputs
#endif

#endif	/* !__PRINTF_H_ */
