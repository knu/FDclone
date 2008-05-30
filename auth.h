/*
 *	auth.h
 *
 *	definitions & function prototype declarations for "auth.c"
 */

#include "url.h"

#ifndef	__AUTH_H_
#define	__AUTH_H_

#define	AUTHNOFILE		16
#define	AUTHBASIC		"Basic"
#define	AUTHDIGEST		"Digest"

typedef struct _auth_t {
	urlhost_t host;
	int type;
	int nlink;
} auth_t;

char *authgetuser __P_((VOID_A));
char *authgetpass __P_((VOID_A));
int authfind __P_((urlhost_t *, int, int));
VOID authentry __P_((urlhost_t *, int));
VOID authfree __P_((VOID_A));
char *authencode __P_((urlhost_t *,
		CONST char *, CONST char *, CONST char *));

#endif	/* !__AUTH_H_ */
