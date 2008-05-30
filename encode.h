/*
 *	encode.h
 *
 *	definitions & function prototype declarations for "md5.c" & "base64.c"
 */

#include "stream.h"

#define	MD5_BUFSIZ		(128 / 32)
#define	MD5_BLOCKS		16
#define	MD5_FILBUFSIZ		512
#define	BASE64_ORGSIZ		3
#define	BASE64_ENCSIZ		4

typedef struct _md5_t {
	u_long cl, ch;
	u_long sum[MD5_BUFSIZ];
	u_long x[MD5_BLOCKS];
	int n, b;
} md5_t;

extern VOID md5encode __P_((u_char *, ALLOC_T *, CONST u_char *, ALLOC_T));
extern int md5fencode __P_((u_char *, ALLOC_T *, XFILE *));
extern int base64encode __P_((char *, ALLOC_T, CONST u_char *, ALLOC_T));
extern int base64decode __P_((u_char *, ALLOC_T *, CONST char *, ALLOC_T));
