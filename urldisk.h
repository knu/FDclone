/*
 *	urldisk.h
 *
 *	definitions & function prototype declarations for "urldisk.c"
 */

#include "dirent.h"
#include "stream.h"
#include "namelist.h"
#include "url.h"
#include "auth.h"

#define	URLNOFILE		16
#define	URLMAXCMDLINE		255
#define	URLENDTIMEOUT		2
#define	FTPANONUSER		"anonymous"
#define	FTPANONPASS		"@"
#define	lazyproxy(uh)		(urlhostlist[uh].prototype == TYPE_HTTP \
				&& urlhostlist[uh].type == TYPE_FTP \
				&& urlhostlist[uh].http \
				&& !((urlhostlist[uh].http) -> server) \
				&& (urlhostlist[uh].http) -> version < 101)

typedef struct _httpstat_t {
	char cwd[MAXPATHLEN];
	int version;
	off_t clength;
	time_t mtim;
	char *server;
	char *location;
	char *digest;
	int charset;
	int chunk;
	int flags;
} httpstat_t;

#define	HFL_CLENGTH		000001
#define	HFL_MTIME		000002
#define	HFL_LOCATION		000004
#define	HFL_DISCONNECT		000010
#define	HFL_AUTHED		000020
#define	HFL_PROXYAUTHED		000040
#define	HFL_CHUNKED		000100
#define	HFL_OTHERHOST		000200
#define	HFL_BODYLESS		000400

typedef struct _urldev_t {
	XFILE *fp;
	urlhost_t host;
	urlhost_t proxy;
	int type;
	int prototype;
#ifdef	DEP_HTTPPATH
	httpstat_t *http;
#endif
	int nlink;
	int options;
	int flags;
} urldev_t;

#define	UOP_NOPASV		000001
#define	UOP_NOPORT		000002
#define	UOP_NOMDTM		000004
#define	UOP_NOFEAT		000010
#define	UOP_FULLSTAT		000020
#define	UOP_NOLISTOPT		000040
#define	UOP_USENLST		000100
#define	UFL_LOCKED		000001
#define	UFL_INTRED		000002
#define	UFL_CLOSED		000004
#define	UFL_PROXIED		000010
#define	UFL_RETRYPASV		000100
#define	UFL_RETRYPORT		000200
#define	UFL_FIXLISTOPT		000400

typedef struct _urlstat_t {
	namelist *list;
	int max;
	int uh;
	int nlink;
	char *path;
	int flags;
} urlstat_t;

#define	UFL_FULLLIST		000001

#define	UGP_SCHEME		000001
#define	UGP_USER		000002
#define	UGP_PASS		000004
#define	UGP_HOST		000010
#define	UGP_CWD			000020
#define	UGP_ANON		000100
#define	UGP_ENCODE		000200

extern VOID urlfreestatlist __P_((int));
extern int urlferror __P_((XFILE *));
extern int urlgetreply __P_((int, char **));
extern int urlcommand __P_((int, char **, int, ...));
extern int urlnewstatlist __P_((int, namelist *, int, CONST char *));
extern CONST char *urlsplitpath __P_((char *, ALLOC_T, CONST char *));
extern int urltracelink __P_((int, CONST char *, namelist *, int *));
extern int urlreconnect __P_((int));
extern int urlopendev __P_((CONST char *, int));
extern VOID urlclosedev __P_((int));
extern int urlgenpath __P_((int, char *, ALLOC_T, int));
extern int urlchdir __P_((int, CONST char *));
extern char *urlgetcwd __P_((int, char *, ALLOC_T));
extern DIR *urlopendir __P_((CONST char *, int, CONST char *));
extern int urlclosedir __P_((DIR *));
extern struct dirent *urlreaddir __P_((DIR *));
extern VOID urlrewinddir __P_((DIR *));
extern int urlstat __P_((CONST char *, int, CONST char *, struct stat *));
extern int urllstat __P_((CONST char *, int, CONST char *, struct stat *));
extern int urlaccess __P_((CONST char *, int, CONST char *, int));
extern int urlreadlink __P_((CONST char *, int, CONST char *, char *, int));
extern int urlchmod __P_((CONST char *, int, CONST char *, int));
extern int urlunlink __P_((CONST char *, int, CONST char *));
extern int urlrename __P_((CONST char *, int, CONST char *, CONST char *));
extern VOID urlputopenlist __P_((int, int, CONST char *, int));
extern int urldelopenlist __P_((int));
extern int urlopen __P_((CONST char *, int, CONST char *, int));
extern int urlclose __P_((int));
extern int urlfstat __P_((int, struct stat *));
extern int urlselect __P_((int));
extern int urlread __P_((int, char *, int));
extern int urlwrite __P_((int, CONST char *, int));
extern int urldup2 __P_((int, int));
extern int urlmkdir __P_((CONST char *, int, CONST char *));
extern int urlrmdir __P_((CONST char *, int, CONST char *));
extern VOID urlallclose __P_((VOID_A));
#ifdef	DEP_FTPPATH
extern int ftpgetreply __P_((XFILE *, char **));
extern int ftpseterrno __P_((int));
extern int vftpcommand __P_((int, char **, int, va_list));
extern int ftpquit __P_((int));
extern int ftpabort __P_((int));
extern int ftplogin __P_((int));
extern int ftprecvlist __P_((int, CONST char *, namelist **));
extern int ftprecvstatus __P_((int, CONST char *, namelist *, int, int));
extern int ftpchdir __P_((int, CONST char *));
extern char *ftpgetcwd __P_((int, char *, ALLOC_T));
extern int ftpchmod __P_((int, CONST char *, int));
extern int ftpunlink __P_((int, CONST char *));
extern int ftprename __P_((int, CONST char *, CONST char *));
extern int ftpopen __P_((int, CONST char *, int));
extern int ftpclose __P_((int, int));
extern int ftpfstat __P_((int, struct stat *));
extern int ftpread __P_((int, int, char *, int));
extern int ftpwrite __P_((int, int, CONST char *, int));
extern int ftpmkdir __P_((int, CONST char *));
extern int ftprmdir __P_((int, CONST char *));
#endif	/* DEP_FTPPATH */
#ifdef	DEP_HTTPPATH
extern VOID httpreset __P_((int, int));
extern int httpgetreply __P_((int, char **));
extern int httpseterrno __P_((int));
extern int vhttpcommand __P_((int, char **, int, va_list));
extern int httprecvlist __P_((int, CONST char *, namelist **));
extern int httprecvstatus __P_((int, CONST char *, namelist *, int, int *));
extern int httpchdir __P_((int, CONST char *));
extern char *httpgetcwd __P_((int, char *, ALLOC_T));
extern int httpchmod __P_((int, CONST char *, int));
extern int httpunlink __P_((int, CONST char *));
extern int httprename __P_((int, CONST char *, CONST char *));
extern int httpopen __P_((int, CONST char *, int));
extern int httpclose __P_((int, int));
extern int httpfstat __P_((int, struct stat *));
extern int httpread __P_((int, int, char *, int));
extern int httpwrite __P_((int, int, CONST char *, int));
extern int httpmkdir __P_((int, CONST char *));
extern int httprmdir __P_((int, CONST char *));
#endif	/* DEP_HTTPPATH */

extern int urloptions;
extern int urltimeout;
extern int urlkcode;
extern urldev_t urlhostlist[];
extern int maxurlhost;
extern urlstat_t *urlstatlist;
extern int maxurlstat;
extern char *logheader;
#ifdef	DEP_FTPPATH
extern char *ftpaddress;
extern char *ftpproxy;
extern char *ftplogfile;
#endif
#ifdef	DEP_HTTPPATH
extern char *httpproxy;
extern char *httplogfile;
extern char *htmllogfile;
#endif
