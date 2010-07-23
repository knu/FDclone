/*
 *	http.c
 *
 *	HTTP accessing in RFC2616
 */

#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#include "time.h"
#include "sysemu.h"
#include "pathname.h"
#include "termio.h"
#include "parse.h"
#include "lsparse.h"
#include "socket.h"
#include "auth.h"
#include "html.h"
#include "urldisk.h"

#define	HTTP_OPTIONS		0
#define	HTTP_GET		1
#define	HTTP_HEAD		2
#define	HTTP_POST		3
#define	HTTP_PUT		4
#define	HTTP_DELETE		5
#define	HTTP_TRACE		6

#define	HTTPSTR			"HTTP/"
#define	HTTPMAJ			1
#define	HTTPMIN			1
#define	MAXFLUSH		(BUFSIZ * 4)
#define	CLOSESTR		"close"
#define	CHUNKEDSTR		"chunked"
#define	DEFHTTP(s, f)		{s, strsize(s), f}

#define	HTTP_INFO		1
#define	HTTP_SUCCESS		2
#define	HTTP_REDIRECT		3
#define	HTTP_CLIERROR		4
#define	HTTP_SRVERROR		5
#define	HTTP_NOREPLY		999

typedef struct _httpcmd_t {
	int id;
	CONST char *cmd;
} httpcmd_t;

typedef struct _httpfld_t {
	CONST char *ident;
	ALLOC_T len;
	int (NEAR *func)__P_((int, CONST char *));
} httpfld_t;

#ifdef	DEP_HTTPPATH

#ifdef	FD
extern char *getversion __P_((int *));
#endif

static VOID NEAR vhttplog __P_((CONST char *, va_list));
static VOID NEAR httplog __P_((CONST char *, ...));
static char *NEAR httprecv __P_((XFILE *));
static int NEAR httpsend __P_((XFILE *, CONST char *, ...));
static int NEAR getcode __P_((CONST char *, int *));
static VOID NEAR httpflush __P_((int));
static int NEAR server __P_((int, CONST char *));
static int NEAR contentlength __P_((int, CONST char *));
static int NEAR contenttype __P_((int, CONST char *));
static int NEAR lastmodified __P_((int, CONST char *));
static int NEAR location __P_((int, CONST char *));
static int NEAR connection __P_((int, CONST char *));
static int NEAR authenticate __P_((int, CONST char *, int));
static int NEAR www_authenticate __P_((int, CONST char *));
static int NEAR proxy_authenticate __P_((int, CONST char *));
static int NEAR chunked __P_((int, CONST char *));
static int NEAR _httpcommand __P_((int, int, CONST char *));
static int NEAR httpcommand __P_((int, int, CONST char *, int *));
static char *httpfgets __P_((VOID_P));
static int NEAR recvhead __P_((int, CONST char *, int *));
static int NEAR chunkgetc __P_((int));
static int NEAR getchunk __P_((int, int));
static int NEAR gettrailer __P_((int));

char *httpproxy = NULL;
char *httplogfile = NULL;

static char *form_http[] = {
	"%f %d-%m-%y %t %s %*x",			/* Apache */
	"%f %!.! %m %d %{yt} %{sx} %!->! %*{Lx}",	/* FTP via squid */
	"%s %m %d %{yt} %*f",				/* FTP via delegate */
	"%f %m %d %{yt} %*{sLx}",			/* FTP via i-FILTER */
	"%y/ %m/ %d %t %{s/<dir>/} %*f",		/* IIS (JP/CN) */
	"%y/ %m/ %d %p %t %{s/<dir>/} %*f",		/* IIS (TW) */
	"%w, %m %d, %y %t %p %{s/<dir>/} %*f",		/* IIS (US) */
	"%d %m %y %t %{s/<dir>/} %*f",			/* IIS (GB) */
	"%w, %d %m, %y %t %{s/<dir>/} %*f",		/* IIS (HK) */
	"%m/ %d/ %y %p %t %{s/<dir>/} %*f",		/* IIS7 (US) */
	"%d/ %m/ %y %t %{s/<dir>/} %*f",		/* IIS7 (GB) */
	NULL
};
static CONST lsparse_t httpformat = {
	NULL, NULL, form_http, NULL, NULL, 0, 0, LF_NOTRAVERSE
};
static CONST httpcmd_t cmdlist[] = {
	{HTTP_OPTIONS, "OPTIONS"},
	{HTTP_GET, "GET"},
	{HTTP_HEAD, "HEAD"},
	{HTTP_POST, "POST"},
	{HTTP_PUT, "PUT"},
	{HTTP_DELETE, "DELETE"},
	{HTTP_TRACE, "TRACE"},
};
#define	CMDLISTSIZ		arraysize(cmdlist)
static CONST httpfld_t fldlist[] = {
	DEFHTTP("Server", server),
	DEFHTTP("Content-Length", contentlength),
	DEFHTTP("Content-Type", contenttype),
	DEFHTTP("Last-Modified", lastmodified),
	DEFHTTP("Location", location),
	DEFHTTP("Connection", connection),
	DEFHTTP("Proxy-Connection", connection),
	DEFHTTP("WWW-Authenticate", www_authenticate),
	DEFHTTP("Proxy-Authenticate", proxy_authenticate),
	DEFHTTP("Transfer-Encoding", chunked),
};
#define	FLDLISTSIZ		arraysize(fldlist)
static char *datelist[] = {
	"%a, %d %b %Y %H:%M:%S %Z",	/* RFC 822 */
	"%A, %d-%b-%y %H:%M:%S %Z",	/* RFC 850 */
	"%a %b %e %H:%M:%S %Y",		/* asctime() */
};
#define	DATELISTSIZ		arraysize(datelist)


static VOID NEAR vhttplog(fmt, args)
CONST char *fmt;
va_list args;
{
	XFILE *fp;

	if (!logheader || !htmllogfile) /*EMPTY*/;
	else if (!httplogfile || strpathcmp(httplogfile, htmllogfile))
		htmllog(logheader);
	if (!httplogfile || !isrootdir(httplogfile)) return;
	if (!(fp = Xfopen(httplogfile, "a"))) return;
	if (logheader) {
		Xfputs(logheader, fp);
		Xfree(logheader);
		logheader = NULL;
	}
	VOID_C Xvfprintf(fp, fmt, args);
	VOID_C Xfclose(fp);
}

#ifdef	USESTDARGH
/*VARARGS1*/
static VOID NEAR httplog(CONST char *fmt, ...)
#else
/*VARARGS1*/
static VOID NEAR httplog(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;

	VA_START(args, fmt);
	vhttplog(fmt, args);
	va_end(args);
}

static char *NEAR httprecv(fp)
XFILE *fp;
{
	char *buf;

	buf = Xfgets(fp);
	if (!buf) return(NULL);
	httplog("<-- \"%s\"\n", buf);

	return(buf);
}

#ifdef	USESTDARGH
/*VARARGS2*/
static int NEAR httpsend(XFILE *fp, CONST char *fmt, ...)
#else
/*VARARGS2*/
static int NEAR httpsend(fp, fmt, va_alist)
XFILE *fp;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char buf[URLMAXCMDLINE + 1];
	int n;

	VA_START(args, fmt);
	n = Xsnprintf(buf, sizeof(buf), "--> \"%s\"\n", fmt);
	if (n >= 0) vhttplog(buf, args);

	n = Xvfprintf(fp, fmt, args);
	if (n >= 0) n = fputnl(fp);
	va_end(args);

	return(n);
}

static int NEAR getcode(s, verp)
CONST char *s;
int *verp;
{
	int i, n;

	if (Xstrncasecmp(s, HTTPSTR, strsize(HTTPSTR))) return(-1);
	s += strsize(HTTPSTR);
	if (!(s = Xsscanf(s, "%<d.%<d", &n, &i))) return(-1);
	if (!Xisblank(*(s++))) return(-1);
	if (verp) *verp = n * 100 + i % 100;
	s = skipspace(s);

	for (i = n = 0; i < 3; i++) {
		if (!Xisdigit(s[i])) return(-1);
		n *= 10;
		n += s[i] - '0';
	}

	return(n);
}

static VOID NEAR httpflush(uh)
int uh;
{
	char *cp;
	int fd, duperrno;

	duperrno = errno;
	if (!urlhostlist[uh].fp) return;
	if ((urlhostlist[uh].http) -> flags & HFL_DISCONNECT) {
		safefclose(urlhostlist[uh].fp);
		urlhostlist[uh].fp = NULL;
		errno = duperrno;
		return;
	}
	if ((urlhostlist[uh].http) -> flags & HFL_CHUNKED) /*EMPTY*/;
	else if (!((urlhostlist[uh].http) -> flags & HFL_CLENGTH)) return;
	else if ((urlhostlist[uh].http) -> flags & HFL_BODYLESS) return;
	else if (!((urlhostlist[uh].http) -> clength)) return;

	fd = Xfileno(urlhostlist[uh].fp);
	urlputopenlist(fd, uh, NULL, O_RDONLY);
	putopenfd(DEV_HTTP, fd);
	Xsettimeout(urlhostlist[uh].fp, URLENDTIMEOUT);
	for (;;) {
		cp = httprecv(urlhostlist[uh].fp);
		if (!cp) break;
		Xfree(cp);
	}
	Xsettimeout(urlhostlist[uh].fp, urltimeout);
	VOID_C delopenfd(fd);
	urldelopenlist(fd);
	errno = duperrno;
}

static int NEAR server(uh, s)
int uh;
CONST char *s;
{
	char *cp;
	ALLOC_T len;

	len = ((cp = Xstrchr(s, '/'))) ? cp - s : strlen(s);
	(urlhostlist[uh].http) -> server = Xstrndup(s, len);

	return(0);
}

static int NEAR contentlength(uh, s)
int uh;
CONST char *s;
{
	off_t len;

	if (!Xsscanf(s, "%<*d%$", sizeof(len), &len)) return(-1);
	(urlhostlist[uh].http) -> clength = len;
	(urlhostlist[uh].http) -> flags |= HFL_CLENGTH;

	return(0);
}

static int NEAR contenttype(uh, s)
int uh;
CONST char *s;
{
	char **argv;
	ALLOC_T ptr, top;
	int n, argc;

	argv = (char **)Xmalloc(sizeof(*argv));
	argc = 0;
	for (ptr = (ALLOC_T)0; s[ptr]; ptr++) {
		while (Xisblank(s[ptr])) ptr++;
		if (!s[ptr]) break;
		argv = (char **)Xrealloc(argv, (argc + 2) * sizeof(*argv));
		top = ptr;
		for (; s[ptr]; ptr++) if (s[ptr] == ';') break;
		argv[argc++] = Xstrndup(&(s[top]), ptr - top);
		if (!s[ptr]) break;
	}
	argv[argc] = NULL;

	n = getcharset(argv);
	freevar(argv);
	if (n >= 0) (urlhostlist[uh].http) -> charset = n;

	return(0);
}

static int NEAR lastmodified(uh, s)
int uh;
CONST char *s;
{
	struct tm tm;
	time_t t;
	int i, tz;

	for (i = 0; i < DATELISTSIZ; i++)
		if (Xstrptime(s, datelist[i], &tm, &tz) >= 0) break;
	if (i >= DATELISTSIZ || (t = Xtimegm(&tm)) == (time_t)-1)
		return(seterrno(EINVAL));
	(urlhostlist[uh].http) -> mtim = t + tz * 60;
	(urlhostlist[uh].http) -> flags |= HFL_MTIME;

	return(0);
}

static int NEAR location(uh, s)
int uh;
CONST char *s;
{
	urlhost_t tmp;
	char *cp;
	int n, ptr, type;

	if ((ptr = urlparse(s, NULL, &cp, &type)) < 0) return(-1);
	if (ptr > 0) {
		n = urlgethost(cp, &tmp);
		Xfree(cp);
		if (n < 0 || !(tmp.host)) return(-1);
		if (tmp.port < 0) tmp.port = urlgetport(type);

		if (tmp.port != urlhostlist[uh].host.port) ptr = -1;
		else if (cmpsockaddr(tmp.host, urlhostlist[uh].host.host))
			ptr = -1;
	}

	if (ptr >= 0) s += ptr;
	else (urlhostlist[uh].http) -> flags |= HFL_OTHERHOST;
	(urlhostlist[uh].http) -> location = Xstrdup(s);
	(urlhostlist[uh].http) -> flags |= HFL_LOCATION;

	return(0);
}

static int NEAR connection(uh, s)
int uh;
CONST char *s;
{
	if (Xstrcasecmp(s, CLOSESTR)) return(0);
	(urlhostlist[uh].http) -> flags |= HFL_DISCONNECT;

	return(0);
}

static int NEAR authenticate(uh, s, flags)
int uh;
CONST char *s;
int flags;
{
	CONST char *cp;

	for (cp = s; *cp && !Xisblank(*cp); cp++) /*EMPTY*/;
	if (!Xstrncasecmp(s, AUTHBASIC, cp - s))
		(urlhostlist[uh].http) -> digest = NULL;
	else if (!Xstrncasecmp(s, AUTHDIGEST, cp - s))
		(urlhostlist[uh].http) -> digest = Xstrdup(skipspace(++cp));
	else return(0);
	(urlhostlist[uh].http) -> flags |= flags;

	return(0);
}

static int NEAR www_authenticate(uh, s)
int uh;
CONST char *s;
{
	return(authenticate(uh, s, HFL_AUTHED));
}

static int NEAR proxy_authenticate(uh, s)
int uh;
CONST char *s;
{
	return(authenticate(uh, s, HFL_PROXYAUTHED));
}

static int NEAR chunked(uh, s)
int uh;
CONST char *s;
{
	if (Xstrcasecmp(s, CHUNKEDSTR)) return(0);
	(urlhostlist[uh].http) -> chunk = 0;
	(urlhostlist[uh].http) -> flags |= HFL_CHUNKED;

	return(0);
}

VOID httpreset(uh, level)
int uh, level;
{
	if (!(urlhostlist[uh].http)) return;
	if (level > 0) {
		Xfree((urlhostlist[uh].http) -> server);
		Xfree((urlhostlist[uh].http) -> location);
		Xfree((urlhostlist[uh].http) -> digest);
	}
	(urlhostlist[uh].http) -> version = -1;
	(urlhostlist[uh].http) -> clength = (off_t)0;
	(urlhostlist[uh].http) -> server =
	(urlhostlist[uh].http) -> location =
	(urlhostlist[uh].http) -> digest = NULL;
	(urlhostlist[uh].http) -> charset = NOCNV;
	(urlhostlist[uh].http) -> chunk = -1;
	(urlhostlist[uh].http) -> flags = 0;
	if (level > 1) {
		Xfree(urlhostlist[uh].http);
		urlhostlist[uh].http = NULL;
	}
}

int httpgetreply(uh, sp)
int uh;
char **sp;
{
	char *cp, *tmp, *buf;
	ALLOC_T len, size;
	int i, code;

	if (!(urlhostlist[uh].http)) return(seterrno(EINVAL));
	httpreset(uh, 1);

	buf = httprecv(urlhostlist[uh].fp);
	if (!buf) {
		if (!urlferror(urlhostlist[uh].fp)) return(HTTP_NOREPLY);
		return(-1);
	}
	if ((code = getcode(buf, &i)) < 0) {
		Xfree(buf);
		return(seterrno(EINVAL));
	}
	(urlhostlist[uh].http) -> version = i;

	size = strlen(buf);
	for (;;) {
		cp = httprecv(urlhostlist[uh].fp);
		if (!cp) {
			Xfree(buf);
			if (!urlferror(urlhostlist[uh].fp))
				return(HTTP_NOREPLY);
			return(-1);
		}
		if (!*cp) {
			Xfree(cp);
			break;
		}

		for (i = 0; i < FLDLISTSIZ; i++) {
			if (Xstrncasecmp(cp, fldlist[i].ident, fldlist[i].len))
				continue;
			if (cp[fldlist[i].len] == ':') break;
		}
		if (i < FLDLISTSIZ) {
			tmp = skipspace(&(cp[fldlist[i].len + 1]));
			VOID_C (*(fldlist[i].func))(uh, tmp);
		}

		len = strlen(cp);
		buf = Xrealloc(buf, size + 1 + len + 1);
		buf[size++] = '\n';
		memcpy(&(buf[size]), cp, len + 1);
		size += len;

		Xfree(cp);
	}
	if (sp) *sp = buf;
	else Xfree(buf);

	return(code);
}

int httpseterrno(n)
int n;
{
	if (n < 0) return(-1);
	if (n / 100 != HTTP_SUCCESS) return(seterrno(EACCES));

	return(0);
}

int vhttpcommand(uh, sp, cmd, args)
int uh;
char **sp;
int cmd;
va_list args;
{
	CONST char *s, *path, *auth, *proxyauth;
	char *new, buf[URLMAXCMDLINE + 1];
	int n, flags;

	if (sp) *sp = NULL;
	if (!(urlhostlist[uh].http) || cmd < 0 || cmd >= CMDLISTSIZ)
		return(seterrno(EINVAL));
	path = va_arg(args, CONST char *);
	auth = va_arg(args, CONST char *);
	proxyauth = va_arg(args, CONST char *);

	new = NULL;
	if ((urlhostlist[uh].flags & UFL_PROXIED)
	&& !urlparse(path, (scheme_t *)nullstr, NULL, NULL)) {
		flags = (UGP_SCHEME | UGP_HOST | UGP_ENCODE);
		if (urlhostlist[uh].type == TYPE_FTP) flags |= UGP_USER;
		n = urlgenpath(uh, buf, sizeof(buf), flags);
		if (n < 0) return(-1);
		n = Xasprintf(&new, "%s%s", buf, path);
		if (n < 0) return(-1);
		path = new;
	}
	if (urlgenpath(uh, buf, sizeof(buf), UGP_HOST | UGP_ENCODE) < 0) {
		Xfree(new);
		return(-1);
	}

	for (;;) {
		n = httpsend(urlhostlist[uh].fp,
			"%s %s %s%d.%d",
			cmdlist[cmd].cmd, path, HTTPSTR, HTTPMAJ, HTTPMIN);
		if (n < 0) break;

#ifdef	FD
		s = getversion(&n);
		n = httpsend(urlhostlist[uh].fp,
			"User-Agent: FDclone/%-.*s", n, s);
		if (n < 0) break;
#endif
		s = (urlhostlist[uh].flags & UFL_PROXIED) ? "Proxy-" : nullstr;
		n = httpsend(urlhostlist[uh].fp, "Host: %s", buf);
		if (n < 0) break;
		n = httpsend(urlhostlist[uh].fp, "Accept: */*");
		if (n < 0) break;
		n = httpsend(urlhostlist[uh].fp,
			"%sConnection: Keep-Alive", s);
		if (n < 0) break;

		if (auth) {
			n = httpsend(urlhostlist[uh].fp,
				"Authorization: %s", auth);
			if (n < 0) break;
		}
		if (proxyauth) {
			n = httpsend(urlhostlist[uh].fp,
				"Proxy-Authorization: %s", proxyauth);
			if (n < 0) break;
		}

		n = httpsend(urlhostlist[uh].fp, nullstr);
		if (n < 0) break;

		n = urlgetreply(uh, sp);
		if (cmd == HTTP_HEAD)
			(urlhostlist[uh].http) -> flags |= HFL_BODYLESS;
		if (n != HTTP_NOREPLY) break;

		if (urlreconnect(uh) < 0) break;
	}
	Xfree(new);

	return(n);
}

static int NEAR _httpcommand(uh, cmd, path)
int uh, cmd;
CONST char *path;
{
	urlhost_t *hp;
	httpstat_t *http;
	char *path2, *auth, *proxyauth;
	int i, n, cnt, dupcmd;

	i = -1;
	cnt = 0;
	dupcmd = cmd;
	hp = NULL;
	http = urlhostlist[uh].http;
	path2 = urlencode(path, -1, URL_UNSAFEPATH);
	auth = proxyauth = NULL;
	for (;;) {
		n = urlcommand(uh, NULL, cmd, path2, auth, proxyauth);
		if (n >= 0 && n != 401 && hp == &(urlhostlist[uh].host))
			authentry(hp, urlhostlist[uh].type);
		if (urlhostlist[uh].type != TYPE_FTP) {
			Xfree(urlhostlist[uh].host.user);
			urlhostlist[uh].host.user = NULL;
		}
		Xfree(urlhostlist[uh].host.pass);
		urlhostlist[uh].host.pass = NULL;

		/* Some buggy Apache cannot authorize except with GET method */
		if (!(urlhostlist[uh].flags & UFL_PROXIED)
		&& http -> digest && cmd == HTTP_HEAD)
			cmd = HTTP_GET;

		if (n == 401) {
			hp = &(urlhostlist[uh].host);
			if (!(http -> flags & HFL_AUTHED)) break;
			i = authfind(hp, urlhostlist[uh].type, i);
			if (i < 0) {
				i = 0;
				if (urlhostlist[uh].type != TYPE_FTP) {
					hp -> user = authgetuser();
					if (!(hp -> user)) break;
				}
				hp -> pass = authgetpass();
				if (urlhostlist[uh].type == TYPE_FTP) {
					if (*(hp -> pass)) cnt = 0;
					else if (cnt++ > 0) break;
				}
			}
			Xfree(auth);
			auth = authencode(&(urlhostlist[uh].host),
				http -> digest, cmdlist[cmd].cmd, path2);
			if (!auth) break;
		}
		else if (n == 407) {
			hp = NULL;
			if (!(http -> flags & HFL_PROXYAUTHED)) break;
			if (!(urlhostlist[uh].proxy.user)
			|| !(urlhostlist[uh].proxy.pass))
				break;
			if (proxyauth) break;
			proxyauth = authencode(&(urlhostlist[uh].proxy),
				http -> digest, cmdlist[cmd].cmd, path2);
			if (!proxyauth) break;
			i = -1;
		}
		else break;

		httpflush(uh);
	}
	if (n >= 0 && dupcmd == HTTP_HEAD && cmd == HTTP_GET) httpflush(uh);
	Xfree(path2);
	Xfree(auth);
	Xfree(proxyauth);

	return(n);
}

static int NEAR httpcommand(uh, cmd, path, isdirp)
int uh, cmd;
CONST char *path;
int *isdirp;
{
	CONST char *cp, *s;
	char buf[MAXPATHLEN];
	int n;

	for (;;) {
		s = path;
		if (isdirp) {
			if (*isdirp < 1 && (cp = strrdelim(path, 0))
			&& (!*(++cp) || isdotdir(cp))) {
				if (!*isdirp) return(seterrno(EISDIR));
				*isdirp = 1;
			}
			if (*isdirp) {
				VOID_C strcatdelim2(buf, path, NULL);
				s = buf;
			}
		}

		n = _httpcommand(uh, cmd, s);
		if (n / 100 == HTTP_SUCCESS) {
			if (isdirp && s == buf && !lazyproxy(uh)) *isdirp = 1;
			break;
		}
		if (n == 404 && isdirp && s == buf) {
			if (*isdirp > 0) break;
			httpflush(uh);
			if ((n = _httpcommand(uh, cmd, path)) < 0)
				return(-1);
			if (n / 100 == HTTP_SUCCESS) {
				*isdirp = 0;
				break;
			}
		}
		if (n != 301 && n != 302) break;
		if (!((urlhostlist[uh].http) -> flags & HFL_LOCATION)) break;
		if ((urlhostlist[uh].http) -> flags & HFL_OTHERHOST) break;
		path = (urlhostlist[uh].http) -> location;
		httpflush(uh);
	}

	return(n);
}

static char *httpfgets(vp)
VOID_P vp;
{
	char *buf;

	if (!(buf = htmlfgets((htmlstat_t *)vp))) return(NULL);
	httplog("    \"%s\"\n", buf);

	return(buf);
}

int httprecvlist(uh, path, listp)
int uh;
CONST char *path;
namelist **listp;
{
	htmlstat_t html;
	char *cp;
	int n, fd, max, isdir;

	if (!(urlhostlist[uh].http)) return(seterrno(EINVAL));
	isdir = 1;
	n = httpcommand(uh, HTTP_GET, path, &isdir);
	if (httpseterrno(n) < 0) {
		if (n >= 0) httpflush(uh);
		return(-1);
	}

	*listp = NULL;
	httplog("<-- (body)\n");
	htmlinit(&html, urlhostlist[uh].fp, path);
	fd = Xfileno(urlhostlist[uh].fp);
	urlputopenlist(fd, uh, NULL, O_RDONLY);
	putopenfd(DEV_HTTP, fd);
	max = lsparse(&html, &httpformat, listp, httpfgets);
	VOID_C delopenfd(fd);
	urldelopenlist(fd);
	if ((urlhostlist[uh].http) -> charset != NOCNV) /*EMPTY*/;
	else if (html.charset != NOCNV)
		(urlhostlist[uh].http) -> charset = html.charset;
	else (urlhostlist[uh].http) -> charset = urlkcode;
	htmlfree(&html);
	httpflush(uh);
	if (*listp) for (n = 0; n < max; n++) {
		cp = (*listp)[n].name;
		if (!cp) continue;
		(*listp)[n].name = urldecode(cp, -1);
		Xfree(cp);
	}

	/*
	 * Some FTP proxy will forced to disconnect socket,
	 * even if it says "Proxy-Connection: keep-alive".
	 */
	if (urlhostlist[uh].type == TYPE_FTP) {
		(urlhostlist[uh].http) -> flags |= HFL_DISCONNECT;
		safefclose(urlhostlist[uh].fp);
		urlhostlist[uh].fp = NULL;
	}

	return(max);
}

static int NEAR recvhead(uh, path, isdirp)
int uh;
CONST char *path;
int *isdirp;
{
	int n, cmd;

	cmd = (urlhostlist[uh].flags & UFL_PROXIED) ? HTTP_GET : HTTP_HEAD;
	if ((n = httpcommand(uh, cmd, path, isdirp)) < 0) return(-1);

	if (cmd == HTTP_HEAD) httpflush(uh);
	else {
		(urlhostlist[uh].http) -> flags |= HFL_DISCONNECT;
		safefclose(urlhostlist[uh].fp);
		urlhostlist[uh].fp = NULL;
	}

	return(n);
}

int httprecvstatus(uh, path, namep, st, entp)
int uh;
CONST char *path;
namelist *namep;
int st, *entp;
{
	namelist *tmp;
	CONST char *cp;
	char buf[MAXPATHLEN];
	int n, isdir;

	isdir = -1;
	if (!(urlhostlist[uh].http)) return(seterrno(EINVAL));
	if (!entp) /*EMPTY*/;
	else if (st < 0 || st >= maxurlstat) {
		if (!(cp = urlsplitpath(buf, sizeof(buf), path))) return(-1);
		*entp = 0;
		st = urlnewstatlist(uh, addlist(NULL, *entp, NULL), 1, buf);
		initlist(&(urlstatlist[st].list[*entp]), cp);
	}
	else if (*entp < 0 || *entp >= urlstatlist[st].max) {
		if (urlstatlist[st].flags & UFL_FULLLIST)
			return(seterrno(ENOENT));
		if (!(cp = urlsplitpath(NULL, (ALLOC_T)0, path))) return(-1);
		*entp = (urlstatlist[st].max)++;
		urlstatlist[st].list =
			addlist(urlstatlist[st].list, *entp, NULL);
		initlist(&(urlstatlist[st].list[*entp]), cp);
	}
	else if (!(urlhostlist[uh].options & UOP_FULLSTAT)) return(st);
	else isdir = (isdir(&(urlstatlist[st].list[*entp]))) ? 1 : 0;

	tmp = (entp) ? &(urlstatlist[st].list[*entp]): NULL;
	if (tmp && ismark(tmp) && wasmark(tmp)) return(st);

	n = recvhead(uh, path, &isdir);
	if (httpseterrno(n) < 0) {
		if (n >= 0) {
			if (namep) namep -> tmpflags |= (F_ISMRK | F_WSMRK);
			if (tmp) tmp -> tmpflags |= (F_ISMRK | F_WSMRK);
		}
		return(-1);
	}

	if (isdir > -1) /*EMPTY*/;
	else if ((urlhostlist[uh].http) -> flags & (HFL_CLENGTH | HFL_MTIME))
		/*EMPTY*/;
	else if (urlhostlist[uh].type != TYPE_FTP)
		isdir = 1;
	if (isdir > 0) {
		if (namep) todirlist(namep, (u_int)-1);
		if (tmp) todirlist(tmp, (u_int)-1);
	}

	if ((urlhostlist[uh].http) -> flags & HFL_CLENGTH) {
		if (namep && !ismark(namep))
			namep -> st_size = (urlhostlist[uh].http) -> clength;
		if (tmp && !ismark(tmp))
			tmp -> st_size = (urlhostlist[uh].http) -> clength;
	}
	if ((urlhostlist[uh].http) -> flags & HFL_MTIME) {
		if (namep && !wasmark(namep))
			namep -> st_mtim = (urlhostlist[uh].http) -> mtim;
		if (tmp && !wasmark(tmp))
			tmp -> st_mtim = (urlhostlist[uh].http) -> mtim;
	}
	if (namep) namep -> tmpflags |= (F_ISMRK | F_WSMRK);
	if (tmp) tmp -> tmpflags |= (F_ISMRK | F_WSMRK);

	return(st);
}

int httpchdir(uh, path)
int uh;
CONST char *path;
{
	namelist tmp;
	int n;

	if (!urlhostlist[uh].http) return(seterrno(EINVAL));
	if (!strpathcmp(path, (urlhostlist[uh].http) -> cwd)) return(0);
	n = urltracelink(uh, path, &tmp, NULL);
	if (n < 0) return(-1);
	if (--n >= 0) urlfreestatlist(n);
#ifndef	NOSYMLINK
	Xfree(tmp.linkname);
#endif
	if (!isdir(&tmp)) return(seterrno(ENOTDIR));
	Xstrncpy((urlhostlist[uh].http) -> cwd, path, MAXPATHLEN - 1);

	return(0);
}

char *httpgetcwd(uh, path, size)
int uh;
char *path;
ALLOC_T size;
{
	int n, flags;

	if (!urlhostlist[uh].http) {
		errno = EINVAL;
		return(NULL);
	}
	flags = (UGP_SCHEME | UGP_HOST | UGP_CWD);
	if (urlhostlist[uh].type == TYPE_FTP) flags |= UGP_USER;
	n = urlgenpath(uh, path, size, flags);
	if (n < 0) return(NULL);

	return(path);
}

/*ARGSUSED*/
int httpchmod(uh, path, mode)
int uh;
CONST char *path;
int mode;
{
	return(seterrno(EACCES));
}

/*ARGSUSED*/
int httpunlink(uh, path)
int uh;
CONST char *path;
{
	return(seterrno(EACCES));
}

/*ARGSUSED*/
int httprename(uh, from, to)
int uh;
CONST char *from, *to;
{
	if (!strpathcmp(from, to)) return(0);

	return(seterrno(EACCES));
}

int httpopen(uh, path, flags)
int uh;
CONST char *path;
int flags;
{
	int n, fd, isdir;

	if (!urlhostlist[uh].http) return(seterrno(EINVAL));
	switch (flags & O_ACCMODE) {
		case O_RDONLY:
			break;
		default:
			return(seterrno(EACCES));
/*NOTREACHED*/
			break;
	}

	isdir = 0;
	n = httpcommand(uh, HTTP_GET, path, &isdir);
	if (httpseterrno(n) < 0) {
		if (n >= 0) httpflush(uh);
		return(-1);
	}
	fd = Xfileno(urlhostlist[uh].fp);

	return(fd);
}

int httpclose(uh, fd)
int uh, fd;
{
	char buf[BUFSIZ];
	int n;

	if (!urlhostlist[uh].http) return(seterrno(EINVAL));
	if (!(urlhostlist[uh].fp)) return(0);

	n = 0;
	if (((urlhostlist[uh].http) -> flags & HFL_CLENGTH)) {
		while ((urlhostlist[uh].http) -> clength > (off_t)0) {
			if ((urlhostlist[uh].http) -> clength > MAXFLUSH)
				break;
			n = checkread(fd, buf, sizeof(buf), URLENDTIMEOUT);
			if (n <= 0) break;
			(urlhostlist[uh].http) -> clength -= (off_t)n;
		}
		if ((urlhostlist[uh].http) -> clength > (off_t)0) {
			safefclose(urlhostlist[uh].fp);
			urlhostlist[uh].fp = NULL;
		}
	}
	httpflush(uh);

	return(n);
}

int httpfstat(uh, stp)
int uh;
struct stat *stp;
{
	if (!urlhostlist[uh].http) return(seterrno(EINVAL));
	if ((urlhostlist[uh].http) -> flags & HFL_CLENGTH)
		stp -> st_size = (urlhostlist[uh].http) -> clength;
	else stp -> st_size = (off_t)-1;
	if ((urlhostlist[uh].http) -> flags & HFL_MTIME)
		stp -> st_mtime = (urlhostlist[uh].http) -> mtim;

	return(0);
}

static int NEAR chunkgetc(fd)
int fd;
{
	u_char uc;
	int n;

	for (;;) {
		if ((n = read(fd, &uc, sizeof(uc))) > 0) break;
		else if (!n) continue;
#ifdef	EAGAIN
		else if (errno == EAGAIN) continue;
#endif
#ifdef	EWOULDBLOCK
		else if (errno == EWOULDBLOCK) continue;
#endif
		else if (errno != EINTR) return(-1);
	}

	return((int)uc);
}

static int NEAR getchunk(uh, fd)
int uh, fd;
{
	int n, c, last, ext, chunk;

	if ((urlhostlist[uh].http) -> chunk) return(0);

	n = ext = chunk = 0;
	last = '\0';
	for (;;) {
		if ((c = chunkgetc(fd)) < 0) return(-1);
		if (c == '\r') {
			if ((c = chunkgetc(fd)) < 0) return(-1);
			if (c != '\n' || !n) return(seterrno(EINVAL));
			break;
		}
		if (c == ';') ext++;
		if (!ext) {
			c = Xtolower(c);
			if (Xisdigit(c)) c -= '0';
			else if (Xisxdigit(c)) c -= 'a' - 10;
			else return(seterrno(EINVAL));
			chunk = (chunk << 4) + c;
			n++;
		}
		last = c;
	}
	(urlhostlist[uh].http) -> chunk = chunk;

	return(0);
}

static int NEAR gettrailer(fd)
int fd;
{
	int n, c, last;

	n = 0;
	last = '\0';
	for (;;) {
		if ((c = chunkgetc(fd)) < 0) return(-1);
		if (c == '\r') {
			if ((c = chunkgetc(fd)) < 0) return(-1);
			if (c != '\n') return(seterrno(EINVAL));
			break;
		}
		n++;
		last = c;
	}

	return(n);
}

int httpread(uh, fd, buf, nbytes)
int uh, fd;
char *buf;
int nbytes;
{
	int n;

	if (!urlhostlist[uh].http) return(seterrno(EINVAL));
	if ((urlhostlist[uh].http) -> flags & HFL_CHUNKED) {
		if ((urlhostlist[uh].http) -> chunk < 0) return(0);
		if (getchunk(uh, fd) < 0) return(-1);
		if (!((urlhostlist[uh].http) -> chunk)) {
			(urlhostlist[uh].http) -> chunk = -1;
			for (;;) {
				if ((n = gettrailer(fd)) < 0) return(-1);
				if (!n) break;
			}
			return(0);
		}
		if (nbytes > (urlhostlist[uh].http) -> chunk)
			nbytes = (urlhostlist[uh].http) -> chunk;
	}
	if ((urlhostlist[uh].http) -> flags & HFL_CLENGTH) {
		if (!((urlhostlist[uh].http) -> clength)) return(0);
		if (nbytes > (urlhostlist[uh].http) -> clength)
			nbytes = (urlhostlist[uh].http) -> clength;
	}

	if ((n = read(fd, buf, nbytes)) < 0) return(n);
	nbytes = n;

	if ((urlhostlist[uh].http) -> flags & HFL_CLENGTH)
		(urlhostlist[uh].http) -> clength -= (off_t)nbytes;
	if ((urlhostlist[uh].http) -> flags & HFL_CHUNKED) {
		(urlhostlist[uh].http) -> chunk -= nbytes;
		if (!((urlhostlist[uh].http) -> chunk)) {
			if ((n = gettrailer(fd)) < 0) return(-1);
			if (n) return(seterrno(EINVAL));
		}
	}

	return(nbytes);
}

/*ARGSUSED*/
int httpwrite(uh, fd, buf, nbytes)
int uh, fd;
CONST char *buf;
int nbytes;
{
	if (!urlhostlist[uh].http) return(seterrno(EINVAL));

	return(seterrno(EACCES));
}

/*ARGSUSED*/
int httpmkdir(uh, path)
int uh;
CONST char *path;
{
	return(seterrno(EACCES));
}

/*ARGSUSED*/
int httprmdir(uh, path)
int uh;
CONST char *path;
{
	return(seterrno(EACCES));
}
#endif	/* DEP_HTTPPATH */
