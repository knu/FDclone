/*
 *	url.h
 *
 *	definitions & function prototype declarations for "url.c"
 */

#ifndef	__URL_H_
#define	__URL_H_

#define	URL_UNSAFE		0001
#define	URL_UNSAFEUSER		0011
#define	URL_UNSAFEHOST		0021
#define	URL_UNSAFEPATH		0041

typedef struct _scheme_t {
	CONST char *ident;
	int len;
	int port;
	int type;
} scheme_t;

#define	SCHEME_FTP		"ftp"
#define	SCHEME_HTTP		"http"
#define	TYPE_UNKNOWN		0
#define	TYPE_FTP		1
#define	TYPE_HTTP		2

typedef struct _urlhost_t {
	char *user;
	char *pass;
	char *host;
	int port;
} urlhost_t;

typedef struct _urlpath_t {
	char *path;
	char *params;
	char *query;
	char *fragment;
} urlpath_t;

#if	defined (DEP_URLPATH) || defined (DEP_SOCKREDIR)
extern char *urldecode __P_((CONST char *, int));
extern char *urlencode __P_((CONST char *, int, int));
extern int urlparse __P_((CONST char *, scheme_t *, char **, int *));
extern int urlgetport __P_((int));
extern CONST char *urlgetscheme __P_((int));
extern VOID urlfreehost __P_((urlhost_t *));
extern int urlgethost __P_((CONST char *, urlhost_t *));
extern int urlgetpath __P_((CONST char *, urlpath_t *));
#endif

#endif	/* !__URL_H_ */
