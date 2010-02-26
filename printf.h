/*
 *	printf.h
 *
 *	definitions & function prototype declarations for "printf.c"
 */

#ifndef	__PRINTF_H_
#define	__PRINTF_H_

#include "stream.h"

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

extern int getnum __P_((CONST char *, int *));
extern int setchar __P_((int, printbuf_t *));
#ifndef	MINIMUMSHELL
extern int Xvasprintf __P_((char **, CONST char *, va_list));
extern int Xasprintf __P_((char **, CONST char *, ...));
#endif
extern int Xvsnprintf __P_((char *, int, CONST char *, va_list));
extern int Xsnprintf __P_((char *, int, CONST char *, ...));
extern int Xvfprintf __P_((XFILE *, CONST char *, va_list));
extern int Xfprintf __P_((XFILE *, CONST char *, ...));
extern int fputnl __P_((XFILE *));
#ifdef	FD
extern VOID kanjifputs __P_((CONST char *, XFILE *));
#else
#define	kanjifputs		Xfputs
#endif

extern CONST char printfflagchar[];
extern CONST int printfflag[];
extern CONST char printfsizechar[];
extern CONST int printfsize[];

#endif	/* !__PRINTF_H_ */
