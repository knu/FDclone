/*
 *	realpath.h
 *
 *	definitions & function prototype declarations for "realpath.c"
 */

#define	RLP_READLINK		0001
#define	RLP_PSEUDOPATH		0002

extern char *Xrealpath __P_((CONST char *, char *, int));

extern int norealpath;
