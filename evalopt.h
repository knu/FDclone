/*
 *	evalopt.h
 *
 *	definitions & function prototype declarations for "evalopt.c"
 */

#ifndef	__EVALOPT_H_
#define	__EVALOPT_H_

typedef struct _opt_t {
	int opt;
	int *var;
	int val;
	CONST char *argval;
} opt_t;

extern VOID initopt __P_((CONST opt_t *));
extern VOID optusage __P_((CONST char *, CONST char*, CONST opt_t *));
extern int evalopt __P_((int, char *CONST *, CONST opt_t *));

#endif	/* __EVALOPT_H_ */
