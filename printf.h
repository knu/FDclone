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

extern char *setchar __P_((int, char *, int *, int *, int));
extern int vasprintf2 __P_((char **, CONST char *, va_list));
extern int asprintf2 __P_((char **, CONST char *, ...));
extern char *snprintf2 __P_((char *, int, CONST char *, ...));
extern int fprintf2 __P_((FILE *, CONST char *, ...));
extern VOID fputnl __P_((FILE *));
#ifdef	FD
extern VOID kanjifputs __P_((char *, FILE *));
#else
#define	kanjifputs	fputs
#endif

#endif	/* __PRINTF_H_ */
