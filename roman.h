/*
 *	roman.h
 *
 *	Roman translation tables
 */

#define	R_MAXROMAN	4
#define	R_MAXKANA	2
#define	J_MIN		0x2121
#define	J_MAX		0x7e7e
#define	J_CHO		0x213c
#define	J_TSU		0x2443
#define	J_NN		0x2473
#define	VALIDJIS(c)	(iseuc((((c) >> 8) & 0xff) ^ 0x80) \
			&& iseuc(((c) & 0xff) ^ 0x80))

typedef struct _romantable {
	char str[R_MAXROMAN + 1];
	ALLOC_T len;
	u_short code[R_MAXKANA];
} romantable;

extern int code2kanji __P_((char *, u_int));
extern int searchroman __P_((char *, int));
extern VOID initroman __P_((VOID_A));
extern int jis2str __P_((char *, u_int));
extern int str2jis __P_((u_short *, int, char *));
extern int addroman __P_((char *, char *));
extern VOID freeroman __P_((int));
