/*
 *	gentbl.h
 *
 *	definitions & function prototype declarations for "gentbl.c"
 */

#ifndef	__GENTBL_H_
#define	__GENTBL_H_

extern FILE *opentbl __P_((CONST char *));
extern int fputheader __P_((CONST char *, FILE *));
extern int fputbegin __P_((CONST char *, FILE *));
extern int fputend __P_((FILE *));
extern int fputbyte __P_((int, FILE *));
extern int fputword __P_((u_int, FILE *));
extern int fputdword __P_((long, FILE *));
extern int fputbuf __P_((CONST u_char *, ALLOC_T, FILE *));
extern int fputlength __P_((CONST char *, long, FILE *, int));

extern int textmode;

#endif	/* __GENTBL_H_ */
