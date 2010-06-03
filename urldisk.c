/*
 *	urldisk.c
 *
 *	URL disk accessing module
 */

#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "string.h"
#include "malloc.h"
#include "time.h"
#include "sysemu.h"
#include "pathname.h"
#include "termio.h"
#include "realpath.h"
#include "lsparse.h"
#include "socket.h"
#include "urldisk.h"

typedef struct _urlopen_t {
	int fd;
	int uh;
	char *path;
	int flags;
} urlopen_t;

#ifdef	DEP_URLPATH

static VOID NEAR urllog __P_((CONST char *, ...));
static int NEAR validhost __P_((int, int));
static VOID NEAR urlfreestat __P_((int));
static int NEAR validdir __P_((sockDIR *));
static VOID NEAR checkintr __P_((int, int));
static int NEAR urlconnect __P_((urldev_t *));
static int NEAR recvlist __P_((int, CONST char *, int));
static int NEAR recvstatus __P_((int, CONST char *, namelist *, int *, int));
static int NEAR entryorder __P_((int));
static int NEAR _urlclosedev __P_((int, int));
static VOID NEAR copystat __P_((struct stat *, namelist *));
static int NEAR urlgetopenlist __P_((int));
static int NEAR _urlclose __P_((int, CONST char *));

int urloptions = 0;
int urltimeout = 0;
urldev_t urlhostlist[URLNOFILE];
int maxurlhost = 0;
urlstat_t *urlstatlist = NULL;
int maxurlstat = 0;
char *logheader = NULL;

static int urlorder[URLNOFILE];
static int maxurlorder = 0;
static int lastst = -1;
static urlopen_t *urlopenlist = NULL;
static int maxurlopen = 0;


#ifdef	USESTDARGH
/*VARARGS1*/
static VOID NEAR urllog(CONST char *fmt, ...)
#else
/*VARARGS1*/
static VOID NEAR urllog(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	struct tm *tm;
	time_t t;
	char *cp;
	int n;

	Xfree(logheader);
	logheader = NULL;

	t = Xtime(NULL);
	tm = localtime(&t);
	n = Xasprintf(&cp, "[%04d/%02d/%02d %02d:%02d:%02d]: %s",
		tm -> tm_year + 1900, tm -> tm_mon + 1, tm -> tm_mday,
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec, fmt);
	if (n < 0) return;

	VA_START(args, fmt);
	VOID_C Xvasprintf(&logheader, cp, args);
	va_end(args);
	Xfree(cp);
}

VOID urlfreestatlist(n)
int n;
{
	if (n < 0 || n >= maxurlstat) return;
	if (urlstatlist[n].nlink > 0 && --(urlstatlist[n].nlink)) return;
	freelist(urlstatlist[n].list, urlstatlist[n].max);
	Xfree(urlstatlist[n].path);
	urlstatlist[n].uh = -1;
	urlstatlist[n].max = -1;
	urlstatlist[n].list = NULL;
	urlstatlist[n].path = NULL;

	while (maxurlstat > 0 && urlstatlist[maxurlstat - 1].uh < 0)
		maxurlstat--;
	if (!maxurlstat) {
		Xfree(urlstatlist);
		urlstatlist = NULL;
	}
}

static int NEAR validhost(uh, conn)
int uh, conn;
{
	if (uh < 0 || uh >= maxurlhost) return(seterrno(EBADF));
	if (conn && !(urlhostlist[uh].fp)) {
#ifdef	DEP_HTTPPATH
		if (urlhostlist[uh].http
		&& ((urlhostlist[uh].http) -> flags & HFL_DISCONNECT))
			return(urlreconnect(uh));
#endif
		errno = (urlhostlist[uh].flags & UFL_INTRED) ? EINTR : EBADF;
		return(-1);
	}

	return(0);
}

static VOID NEAR urlfreestat(uh)
int uh;
{
	int n;

	if (validhost(uh, 0) < 0) return;
	for (n = 0; n < maxurlstat; n++) if (uh == urlstatlist[n].uh) {
		if (n == lastst) lastst = -1;
		urlstatlist[n].nlink = 0;
		urlfreestatlist(n);
	}
}

static int NEAR validdir(dirp)
sockDIR *dirp;
{
	if (!dirp || dirp -> dd_id != SID_IFURLDRIVE
	|| validhost(dirp -> dd_fd, 0) < 0
	|| dirp -> dd_size < 0 || dirp -> dd_size >= maxurlstat)
		return(seterrno(EINVAL));

	return(0);
}

int urlferror(fp)
XFILE *fp;
{
	return((errno == EINTR) ? -1 : Xferror(fp));
}

static VOID NEAR checkintr(uh, val)
int uh, val;
{
	if (val >= 0 || errno != EINTR || validhost(uh, 0) < 0) return;
	safefclose(urlhostlist[uh].fp);
	urlhostlist[uh].fp = NULL;
	urlhostlist[uh].flags &= ~UFL_LOCKED;
	urlhostlist[uh].flags |= UFL_INTRED;
}

int urlgetreply(uh, sp)
int uh;
char **sp;
{
	int n;

	if (validhost(uh, 1) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpgetreply(urlhostlist[uh].fp, sp);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpgetreply(uh, sp);
			break;
#endif
		default:
			n = 0;
			break;
	}
	checkintr(uh, n);

	return(n);
}

#ifdef	USESTDARGH
/*VARARGS3*/
int urlcommand(int uh, char **sp, int cmd, ...)
#else
/*VARARGS3*/
int urlcommand(uh, sp, cmd, va_alist)
int uh;
char **sp;
int cmd;
va_dcl
#endif
{
	va_list args;
	int n;

	if (validhost(uh, 1) < 0) return(-1);
	VA_START(args, cmd);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = vftpcommand(uh, sp, cmd, args);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = vhttpcommand(uh, sp, cmd, args);
			break;
#endif
		default:
			n = 0;
			break;
	}
	va_end(args);

	return(n);
}

static int NEAR urlconnect(devp)
urldev_t *devp;
{
	urlhost_t *hp;
	XFILE *fp;
	char *cp, *url;
	int s, type;

	if (devp -> proxy.port >= 0)
		hp = (devp -> flags & UFL_PROXIED)
			? &(devp -> proxy) : &(devp -> host);
	else {
		switch (devp -> type) {
#ifdef	DEP_FTPPATH
			case TYPE_FTP:
				url = ftpproxy;
				break;
#endif
#ifdef	DEP_HTTPPATH
			case TYPE_HTTP:
				url = httpproxy;
				break;
#endif
			default:
				url = NULL;
				break;
		}

		type = TYPE_UNKNOWN;
		if (!urlparse(url, NULL, &cp, &type)
		|| urlgethost(cp, &(devp -> proxy)) < 0
		|| !(devp -> proxy.host)) {
			hp = &(devp -> host);
			devp -> proxy.port = 0;
			devp -> prototype = devp -> type;
		}
		else {
			hp = &(devp -> proxy);
			switch (type) {
#ifdef	DEP_FTPPATH
				case TYPE_FTP:
					if (devp -> type != TYPE_FTP)
						hp = NULL;
					break;
#endif
#ifdef	DEP_HTTPPATH
				case TYPE_HTTP:
					break;
#endif
				default:
					hp = NULL;
					break;
			}
			if (!hp) return(seterrno(ENOENT));
			devp -> prototype = type;
			devp -> flags |= UFL_PROXIED;
			if (hp -> port < 0) hp -> port = urlgetport(type);
		}
		Xfree(cp);
	}

	s = sockconnect(hp -> host, hp -> port, urltimeout, SCK_LOWDELAY);
	if (s < 0 || !(fp = Xfdopen(s, "r+"))) {
		urlfreehost(&(devp -> proxy));
		safeclose(s);
		return(-1);
	}
	Xsettimeout(fp, urltimeout);
	Xsetflags(fp, XF_CRNL | XF_NONBLOCK);

#ifdef	DEP_FTPPATH
	if (devp -> prototype == TYPE_FTP) {
		Xsetflags(fp, XF_TELNET);
		if (ftpseterrno(ftpgetreply(fp, NULL)) < 0) {
			urlfreehost(&(devp -> proxy));
			safefclose(fp);
			return(-1);
		}
	}
#endif
	devp -> fp = fp;

	return(0);
}

int urlnewstatlist(uh, list, max, path)
int uh;
namelist *list;
int max;
CONST char *path;
{
	int i;

	for (i = 0; i < maxurlstat; i++) if (urlstatlist[i].uh < 0) break;
	if (i >= maxurlstat)
		urlstatlist = (urlstat_t *)Xrealloc(urlstatlist,
			++maxurlstat * sizeof(*urlstatlist));
	urlstatlist[i].list = list;
	urlstatlist[i].max = max;
	urlstatlist[i].uh = uh;
	urlstatlist[i].nlink = 1;
	urlstatlist[i].path = Xstrdup(path);
	urlstatlist[i].flags = 0;

	return(i);
}

/*ARGSUSED*/
static int NEAR recvlist(uh, path, cacheonly)
int uh;
CONST char *path;
int cacheonly;
{
	namelist *tmp, *list;
	int i, j, n, max;

	for (i = maxurlstat - 1; i >= 0; i--) {
		if (urlstatlist[i].uh < 0) continue;
		if (!(urlstatlist[i].path) || !(urlstatlist[i].list)) continue;
		if (uh != urlstatlist[i].uh) continue;
		if (!strpathcmp(path, urlstatlist[i].path)) break;
	}

	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			if (i >= 0) {
				urlstatlist[i].nlink++;
				return(i);
			}
			max = ftprecvlist(uh, path, &list);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			if (i >= 0) {
				if (cacheonly
				|| (urlstatlist[i].flags & UFL_FULLLIST)) {
					urlstatlist[i].nlink++;
					return(i);
				}
			}
			if (cacheonly) return(seterrno(ENOENT));
			max = httprecvlist(uh, path, &list);
			break;
#endif
		default:
			max = -1;
			break;
	}
	if (max < 0) return(-1);

	if (i < 0) {
		if ((n = urlnewstatlist(uh, list, max, path)) < 0) {
			freelist(list, max);
			return(-1);
		}
	}
	else {
		n = i;
		for (i = 0; i < urlstatlist[n].max; i++) {
			tmp = &(urlstatlist[n].list[i]);
			for (j = 0; j < max; j++)
				if (!strpathcmp(tmp -> name, list[j].name))
					break;
			if (j >= max) continue;
			if (ismark(tmp)) list[j].st_size = tmp -> st_size;
			if (wasmark(tmp)) list[j].st_mtim = tmp -> st_mtim;
		}
		freelist(urlstatlist[n].list, urlstatlist[n].max);
		urlstatlist[n].list = list;
		urlstatlist[n].max = max;
	}
	urlstatlist[n].flags |= UFL_FULLLIST;

	return(n);
}

CONST char *urlsplitpath(buf, size, path)
char *buf;
ALLOC_T size;
CONST char *path;
{
	CONST char *cp;

	if (*path != _SC_) {
		errno = EINVAL;
		return(NULL);
	}
	else if (!(cp = strrdelim(&(path[1]), 0))) {
		if (buf) copyrootpath(buf);
		cp = (path[1]) ? &(path[1]) : curpath;
	}
	else if (buf && Xsnprintf(buf, size, "%-.*s", cp - path, path) < 0)
		return(NULL);
	else if (!*(++cp)) cp = curpath;

	return(cp);
}

static int NEAR recvstatus(uh, path, namep, entp, cacheonly)
int uh;
CONST char *path;
namelist *namep;
int *entp, cacheonly;
{
	CONST char *cp;
	char buf[MAXPATHLEN];
	int i, n;

	if (!(cp = urlsplitpath(buf, sizeof(buf), path))) return(-1);
	if (namep) {
		initlist(namep, NULL);
		if (cp == curpath) todirlist(namep, (u_int)-1);
	}

	if ((n = recvlist(uh, buf, cacheonly)) < 0) i = -1;
	else {
		for (i = 0; i < urlstatlist[n].max; i++)
			if (!strpathcmp(cp, urlstatlist[n].list[i].name))
				break;
		if (i >= urlstatlist[n].max) i = seterrno(ENOENT);
	}
	if (i < 0 && cp != curpath) {
		if (n >= 0 && urlstatlist[n].flags & UFL_FULLLIST) {
			urlfreestatlist(n);
			return(-1);
		}
#ifdef	DEP_HTTPPATH
		if (cacheonly && urlhostlist[uh].prototype == TYPE_HTTP) {
			n = httprecvstatus(uh, path, namep, n, &i);
			if (n < 0) return(-1);
			if (entp) *entp = i;
			return(n + 1);
		}
#endif
		return(-1);
	}
	if (entp) *entp = i;

	if (namep && i >= 0) {
		memcpy((char *)namep,
			(char *)&(urlstatlist[n].list[i]), sizeof(*namep));
#ifndef	NOSYMLINK
		namep -> linkname = Xstrdup(urlstatlist[n].list[i].linkname);
#endif
	}

	return((n >= 0) ? n + 1 : 0);
}

int urltracelink(uh, path, namep, entp)
int uh;
CONST char *path;
namelist *namep;
int *entp;
{
#ifndef	NOSYMLINK
	CONST char *cp;
	char buf[MAXPATHLEN], resolved[MAXPATHLEN];
#endif
	int n, cacheonly;

	cacheonly = 1;
	for (;;) {
		n = recvstatus(uh, path, namep, entp, cacheonly);
		if (n < 0) return(-1);

#ifdef	DEP_HTTPPATH
		/*
		 * Some FTP proxy ignores any '/' at the end of pathname,
		 * so that the additional '/' cannot tell if it is a directory.
		 * Then we need to scan its parent directory.
		 */
		if (cacheonly && lazyproxy(uh)
		&& n > 0 && !(urlstatlist[n - 1].flags & UFL_FULLLIST)) {
			cacheonly = 0;
			urlfreestatlist(--n);
			continue;
		}
#endif	/* DEP_HTTPPATH */
#ifdef	NOSYMLINK
		break;
#else	/* !NOSYMLINK */
		if (!islink(namep)) break;

		if (--n >= 0) urlfreestatlist(n);
		if (*(namep -> linkname) == _SC_) n = 0;
		else if ((cp = strrdelim(path, 0)) && cp > path) n = cp - path;
		else {
			path = rootpath;
			n = 1;
		}
		cp = (n) ? _SS_ : nullstr;
		n = Xsnprintf(buf, sizeof(buf),
			"%-.*s%s%s", n, path, cp, namep -> linkname);
		if (n < 0) return(-1);
		VOID_C Xrealpath(buf, resolved, RLP_PSEUDOPATH);
		path = resolved;
		Xfree(namep -> linkname);
#endif	/* !NOSYMLINK */
	}

	return(n);
}

int urlreconnect(uh)
int uh;
{
	safefclose(urlhostlist[uh].fp);
	urlhostlist[uh].fp = NULL;

	return(urlconnect(&(urlhostlist[uh])));
}

static int NEAR entryorder(uh)
int uh;
{
	int n;

	for (n = maxurlorder - 1; n >= 0; n--) if (urlorder[n] == uh) break;
	if (n >= 0) {
		if (n == maxurlorder - 1) return(0);
		memmove((char *)&(urlorder[n]),
			(char *)&(urlorder[n + 1]),
			(maxurlorder - n - 1) * sizeof(*urlorder));
		urlorder[maxurlorder - 1] = uh;
		return(0);
	}

	if (maxurlorder >= URLNOFILE) return(seterrno(EINVAL));
	urlorder[maxurlorder++] = uh;

	return(0);
}

int urlopendev(host, type)
CONST char *host;
int type;
{
	urlhost_t *hp;
	urldev_t tmp;
	int n, uh;

	if (urlgethost(host, &(tmp.host)) < 0) return(-1);
	if (!(tmp.host.host)) {
		urlfreehost(&(tmp.host));
		return(seterrno(EINVAL));
	}
	if (tmp.host.port < 0) tmp.host.port = urlgetport(type);

	for (n = maxurlorder - 1; n >= 0; n--) {
		uh = urlorder[n];
		hp = &(urlhostlist[uh].host);
		if (urlhostlist[uh].flags & (UFL_CLOSED | UFL_LOCKED))
			continue;
		if (type != urlhostlist[uh].type) continue;
		if (tmp.host.port != hp -> port) continue;
		if (cmpsockaddr(tmp.host.host, hp -> host)) continue;

		switch (type) {
#ifdef	DEP_FTPPATH
			case TYPE_FTP:
				if (!(tmp.host.user)) {
					if (hp -> user) continue;
				}
				else if (!(hp -> user)) continue;
				else if (strcmp(tmp.host.user, hp -> user))
					continue;
				break;
#endif
#ifdef	DEP_HTTPPATH
			case TYPE_HTTP:
				Xfree(tmp.host.user);
				tmp.host.user = NULL;
				Xfree(tmp.host.pass);
				tmp.host.pass = NULL;
				break;
#endif
			default:
				return(seterrno(ENOENT));
/*NOTREACHED*/
				break;
		}

		urlfreehost(&(tmp.host));
		if (urlhostlist[uh].flags & UFL_INTRED)
			return(seterrno(EINTR));
		if (!(urlhostlist[uh].fp) && urlreconnect(uh) < 0) return(-1);
		urlhostlist[uh].nlink++;
		if (entryorder(uh) < 0) return(_urlclosedev(uh, -1));

		return(uh);
	}

	for (uh = 0; uh < maxurlhost; uh++)
		if (urlhostlist[uh].flags & UFL_CLOSED) break;
	if (uh >= URLNOFILE) {
		urlfreehost(&(tmp.host));
		return(seterrno(EMFILE));
	}

	if (!logheader) urllog("connecting %s...\n", tmp.host.host);
	tmp.fp = NULL;
	tmp.proxy.user = tmp.proxy.pass = tmp.proxy.host = NULL;
	tmp.proxy.port = -1;
	tmp.type = type;
	tmp.nlink = 1;
	tmp.options = urloptions;
	tmp.flags = 0;
#ifdef	DEP_HTTPPATH
	tmp.http = NULL;
#endif
	if (urlconnect(&tmp) < 0) {
		urlfreehost(&(tmp.host));
		urlfreehost(&(tmp.proxy));
		return(-1);
	}

	memcpy((char *)&(urlhostlist[uh]), (char *)&tmp, sizeof(*urlhostlist));
	if (uh >= maxurlhost) maxurlhost++;
	if (entryorder(uh) < 0) return(_urlclosedev(uh, -1));

	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			hp = &(urlhostlist[uh].host);
			if (hp -> user && !(hp -> pass)) {
				n = authfind(hp, type, -1);
				if (n < 0) hp -> pass = authgetpass();
			}
			if (ftplogin(uh) < 0) return(_urlclosedev(uh, -1));
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			urlhostlist[uh].http =
				(httpstat_t *)Xmalloc(sizeof(httpstat_t));
			copyrootpath((urlhostlist[uh].http) -> cwd);
			httpreset(uh, 0);
			break;
#endif
		default:
			break;
	}

	return(uh);
}

static int NEAR _urlclosedev(uh, ret)
int uh, ret;
{
	int n, duperrno;

	duperrno = errno;
	if (uh < 0 || uh >= maxurlhost) return(ret);
	if (urlhostlist[uh].flags & UFL_CLOSED) return(ret);

	urlhostlist[uh].flags &= ~UFL_LOCKED;
#ifdef	DEP_HTTPPATH
	httpreset(uh, 1);
#endif
	if (urlhostlist[uh].nlink > 0 && --(urlhostlist[uh].nlink)) {
		errno = duperrno;
		return(ret);
	}

	urlfreestat(uh);
	if (urlhostlist[uh].fp) {
#ifdef	DEP_FTPPATH
		if (urlhostlist[uh].prototype == TYPE_FTP) {
			n = (ret >= 0) ? ftpquit(uh) : ftpabort(uh);
			if (n < 0) ret = n;
		}
#endif
		safefclose(urlhostlist[uh].fp);
		urlhostlist[uh].fp = NULL;
	}
#ifdef	DEP_HTTPPATH
	httpreset(uh, 2);
#endif
	urlhostlist[uh].flags |= UFL_CLOSED;

	for (n = 0; n < maxurlorder; n++) if (urlorder[n] == uh) break;
	if (n < maxurlorder)
		memmove((char *)&(urlorder[n]),
			(char *)&(urlorder[n + 1]),
			(--maxurlorder - n) * sizeof(*urlorder));

	urlfreehost(&(urlhostlist[uh].host));
	urlfreehost(&(urlhostlist[uh].proxy));
	while (maxurlhost > 0) {
		if (!(urlhostlist[maxurlhost - 1].flags & UFL_CLOSED))
			break;
		maxurlhost--;
	}
	errno = duperrno;

	return(ret);
}

VOID urlclosedev(uh)
int uh;
{
	VOID_C _urlclosedev(uh, 0);
}

int urlgenpath(uh, buf, size, flags)
int uh;
char *buf;
ALLOC_T size;
int flags;
{
	CONST char *s;
	char *cp, *pass;
	int n, len, port;

	n = len = 0;
	if (flags & UGP_SCHEME) {
		s = urlgetscheme(urlhostlist[uh].type);
		if (!s) return(seterrno(EINVAL));
		n = Xsnprintf(buf, size, "%s://", s);
		if (n < 0) return(-1);
#ifdef	CODEEUC
		n = strlen(buf);
#endif
		len += n;
	}

	if (flags & UGP_USER) {
		cp = urlhostlist[uh].host.user;
		pass = urlhostlist[uh].host.pass;
		if ((flags & UGP_ANON) && !cp && (!(flags & UGP_PASS) || pass))
			cp = FTPANONUSER;

		buf += n;
		size -= n;
		if (flags & UGP_ENCODE) {
			cp = urlencode(cp, -1, URL_UNSAFEUSER);
			pass = urlencode(pass, -1, URL_UNSAFEUSER);
		}
		if (!cp) buf[n = 0] = '\0';
		else if ((flags & UGP_PASS) && pass)
			n = Xsnprintf(buf, size, "%s:%s", cp, pass);
		else n = Xsnprintf(buf, size, "%s", cp);

		if (flags & UGP_ENCODE) {
			Xfree(cp);
			Xfree(pass);
		}
		if (n < 0) return(-1);
#ifdef	CODEEUC
		n = strlen(buf);
#endif
		len += n;

		if ((flags & UGP_HOST) && n) {
			buf += n;
			size -= n;
			n = Xsnprintf(buf, size, "@");
			if (n < 0) return(-1);
#ifdef	CODEEUC
			n = strlen(buf);
#endif
			len += n;
		}
	}

	if (flags & UGP_HOST) {
		cp = urlhostlist[uh].host.host;
		if (!cp) return(seterrno(EINVAL));
		port = urlhostlist[uh].host.port;

		buf += n;
		size -= n;
		if (flags & UGP_ENCODE) cp = urlencode(cp, -1, URL_UNSAFEHOST);
		if (port == urlgetport(urlhostlist[uh].type))
			n = Xsnprintf(buf, size, "%s", cp);
		else n = Xsnprintf(buf, size, "%s:%d", cp, port);

		if (flags & UGP_ENCODE) Xfree(cp);
		if (n < 0) return(-1);
#ifdef	CODEEUC
		n = strlen(buf);
#endif
		len += n;
	}

#ifdef	DEP_HTTPPATH
	if ((flags & UGP_CWD) && urlhostlist[uh].http) {
		s = (urlhostlist[uh].http) -> cwd;
		if (!*s) s = rootpath;

		buf += n;
		size -= n;
		n = Xsnprintf(buf, size, "%s", s);
		if (n < 0) return(-1);
#ifdef	CODEEUC
		n = strlen(buf);
#endif
		len += n;
	}
#endif

	return(len);
}

int urlchdir(uh, path)
int uh;
CONST char *path;
{
	int n;

	urllog("chdir(\"%s\")\n", path);
	if (validhost(uh, 0) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpchdir(uh, path);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpchdir(uh, path);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}
	if (n < 0) return(-1);
	if ((n = recvlist(uh, path, 0)) >= 0) {
		if (lastst >= 0) urlfreestatlist(lastst);
		lastst = n;
	}

	return(0);
}

char *urlgetcwd(uh, path, size)
int uh;
char *path;
ALLOC_T size;
{
	urllog("getcwd()\n");
	if (validhost(uh, 0) < 0) return(NULL);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			path = ftpgetcwd(uh, path, size);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			path = httpgetcwd(uh, path, size);
			break;
#endif
		default:
			errno = ENOENT;
			path = NULL;
			break;
	}
	if (!path) return(NULL);

	return(path);
}

DIR *urlopendir(host, type, path)
CONST char *host;
int type;
CONST char *path;
{
	sockDIR *sockdirp;
	int n, uh;

	urllog("opendir(\"%s\")\n", path);
	if (*path != _SC_) {
		errno = EINVAL;
		return(NULL);
	}
	if ((uh = urlopendev(host, type)) < 0) return(NULL);
	if ((n = recvlist(uh, path, 0)) < 0) {
		VOID_C _urlclosedev(uh, -1);
		return(NULL);
	}
	sockdirp = (sockDIR *)Xmalloc(sizeof(*sockdirp));
	sockdirp -> dd_id = SID_IFURLDRIVE;
	sockdirp -> dd_fd = uh;
	sockdirp -> dd_loc = 0L;
	sockdirp -> dd_size = n;

	return((DIR *)sockdirp);
}

int urlclosedir(dirp)
DIR *dirp;
{
	sockDIR *sockdirp;
	int uh;

	urllog("closedir()\n");
	sockdirp = (sockDIR *)dirp;
	if (validdir(sockdirp) < 0) return(-1);
	uh = sockdirp -> dd_fd;
	urlfreestatlist(sockdirp -> dd_size);
	Xfree(sockdirp);
	urlclosedev(uh);

	return(0);
}

struct dirent *urlreaddir(dirp)
DIR *dirp;
{
	static st_dirent buf;
	sockDIR *sockdirp;
	namelist *list;
	int n, max;

	urllog("readdir()\n");
	sockdirp = (sockDIR *)dirp;
	if (validdir(sockdirp) < 0) return(NULL);
	list = urlstatlist[sockdirp -> dd_size].list;
	max = urlstatlist[sockdirp -> dd_size].max;
	if (sockdirp -> dd_loc >= max) {
		errno = 0;
		return(NULL);
	}

	memset((char *)&buf, 0, sizeof(buf));
	n = (sockdirp -> dd_loc)++;
	Xstrncpy(((struct dirent *)&buf) -> d_name, list[n].name, MAXNAMLEN);

	return((struct dirent *)&buf);
}

VOID urlrewinddir(dirp)
DIR *dirp;
{
	sockDIR *sockdirp;

	urllog("rewinddir()\n");
	sockdirp = (sockDIR *)dirp;
	if (validdir(sockdirp) < 0) return;
	sockdirp -> dd_loc = 0L;
}

static VOID NEAR copystat(stp, namep)
struct stat *stp;
namelist *namep;
{
	memset((char *)stp, 0, sizeof(*stp));

#ifndef	NODIRLOOP
	stp -> st_dev = (dev_t)-1;
	stp -> st_ino = (ino_t)-1;
#endif
	stp -> st_mode = namep -> st_mode;
	stp -> st_nlink = namep -> st_nlink;
#ifndef	NOUID
	stp -> st_uid = namep -> st_uid;
	stp -> st_gid = namep -> st_gid;
#endif
#ifdef	HAVEFLAGS
	stp -> st_flags = namep -> st_flags;
#endif
	stp -> st_size = namep -> st_size;
	memcpy((char *)&(stp -> st_atime),
		(char *)&(namep -> st_mtim), sizeof(stp -> st_atime));
	memcpy((char *)&(stp -> st_mtime),
		(char *)&(namep -> st_mtim), sizeof(stp -> st_mtime));
	memcpy((char *)&(stp -> st_ctime),
		(char *)&(namep -> st_mtim), sizeof(stp -> st_ctime));
	stp -> st_blksize = DEV_BSIZE;

	if (isdev(namep)) {
		stp -> st_size = (off_t)0;
		stp -> st_rdev = namep -> st_size;
	}
}

int urlstat(host, type, path, stp)
CONST char *host;
int type;
CONST char *path;
struct stat *stp;
{
	namelist tmp;
	int i, n, uh;

	urllog("stat(\"%s\")\n", path);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	if ((n = urltracelink(uh, path, &tmp, &i)) < 0)
		return(_urlclosedev(uh, -1));
	if (--n >= 0) {
		switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
			case TYPE_FTP:
				VOID_C ftprecvstatus(uh, path, &tmp, n, i);
				break;
#endif
#ifdef	DEP_HTTPPATH
			case TYPE_HTTP:
				VOID_C httprecvstatus(uh, path, &tmp, n, &i);
				break;
#endif
			default:
				break;
		}
		urlfreestatlist(n);
	}

	copystat(stp, &tmp);
#ifndef	NOSYMLINK
	Xfree(tmp.linkname);
#endif
	urlclosedev(uh);

	return(0);
}

int urllstat(host, type, path, stp)
CONST char *host;
int type;
CONST char *path;
struct stat *stp;
{
	namelist tmp;
	int i, n, uh;

	urllog("lstat(\"%s\")\n", path);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	if ((n = recvstatus(uh, path, &tmp, &i, 1)) < 0)
		return(_urlclosedev(uh, -1));
	if (--n >= 0) {
		switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
			case TYPE_FTP:
				VOID_C ftprecvstatus(uh, path, &tmp, n, i);
				break;
#endif
#ifdef	DEP_HTTPPATH
			case TYPE_HTTP:
				VOID_C httprecvstatus(uh, path, &tmp, n, &i);
				break;
#endif
			default:
				break;
		}
		urlfreestatlist(n);
	}

	copystat(stp, &tmp);
#ifndef	NOSYMLINK
	Xfree(tmp.linkname);
#endif
	urlclosedev(uh);

	return(0);
}

int urlaccess(host, type, path, mode)
CONST char *host;
int type;
CONST char *path;
int mode;
{
	namelist tmp;
	int n, uh;

	urllog("access(\"%s\")\n", path);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	if ((n = urltracelink(uh, path, &tmp, NULL)) < 0)
		return(_urlclosedev(uh, -1));
	if (--n >= 0) urlfreestatlist(n);
#ifndef	NOSYMLINK
	Xfree(tmp.linkname);
#endif
	urlclosedev(uh);

	if (((mode & R_OK) && !(tmp.flags & F_ISRED))
	|| ((mode & W_OK) && !(tmp.flags & F_ISWRI))
	|| ((mode & X_OK) && !(tmp.flags & F_ISEXE)))
		return(seterrno(EACCES));

	return(0);
}

int urlreadlink(host, type, path, buf, bufsiz)
CONST char *host;
int type;
CONST char *path;
char *buf;
int bufsiz;
{
	namelist tmp;
	int n, uh;

	urllog("readlink(\"%s\")\n", path);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	if ((n = recvstatus(uh, path, &tmp, NULL, 0)) < 0)
		return(_urlclosedev(uh, -1));
	if (--n >= 0) urlfreestatlist(n);
	if (!islink(&tmp)) n = seterrno(EINVAL);
	else {
#ifdef	NOSYMLINK
		n = 0;
#else
		for (n = 0; n < bufsiz && tmp.linkname[n]; n++)
			buf[n] = tmp.linkname[n];
#endif
	}

#ifndef	NOSYMLINK
	Xfree(tmp.linkname);
#endif
	urlclosedev(uh);

	return(n);
}

int urlchmod(host, type, path, mode)
CONST char *host;
int type;
CONST char *path;
int mode;
{
	int n, uh;

	urllog("chmod(\"%s\", %03o)\n", path, mode);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpchmod(uh, path, mode);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpchmod(uh, path, mode);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}
	urlclosedev(uh);

	return(n);
}

int urlunlink(host, type, path)
CONST char *host;
int type;
CONST char *path;
{
	int n, uh;

	urllog("unlink(\"%s\")\n", path);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpunlink(uh, path);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpunlink(uh, path);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}
	urlclosedev(uh);

	return(n);
}

int urlrename(host, type, from, to)
CONST char *host;
int type;
CONST char *from, *to;
{
	int n, uh;

	urllog("rename(\"%s\", \"%s\")\n", from, to);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftprename(uh, from, to);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httprename(uh, from, to);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}
	urlclosedev(uh);

	return(n);
}

static int NEAR urlgetopenlist(fd)
int fd;
{
	int n;

	for (n = maxurlopen - 1; n >= 0; n--)
		if (fd == urlopenlist[n].fd) return(n);

	return(seterrno(EINVAL));
}

VOID urlputopenlist(fd, uh, path, flags)
int fd, uh;
CONST char *path;
int flags;
{
	int n;

	if ((n = urlgetopenlist(fd)) >= 0) Xfree(urlopenlist[n].path);
	else {
		n = maxurlopen++;
		urlopenlist = (urlopen_t *)Xrealloc(urlopenlist,
			maxurlopen * sizeof(*urlopenlist));
	}

	urlopenlist[n].fd = fd;
	urlopenlist[n].uh = uh;
	urlopenlist[n].path = Xstrdup(path);
	urlopenlist[n].flags = flags;

	if (validhost(uh, 0) >= 0) urlhostlist[uh].flags |= UFL_LOCKED;
}

int urldelopenlist(fd)
int fd;
{
	int n, uh;

	if ((n = urlgetopenlist(fd)) < 0) return(-1);
	uh = urlopenlist[n].uh;
	Xfree(urlopenlist[n].path);
	memmove((char *)&(urlopenlist[n]), (char *)&(urlopenlist[n + 1]),
		(--maxurlopen - n) * sizeof(*urlopenlist));
	if (maxurlopen <= 0) {
		maxurlopen = 0;
		Xfree(urlopenlist);
		urlopenlist = NULL;
	}

	if (validhost(uh, 0) >= 0) {
		for (n = 0; n < maxurlopen; n++)
			if (uh == urlopenlist[n].uh) break;
		if (n >= maxurlopen) urlhostlist[uh].flags &= ~UFL_LOCKED;
	}

	return(uh);
}

int urlopen(host, type, path, flags)
CONST char *host;
int type;
CONST char *path;
int flags;
{
	int uh, fd;

	urllog("open(\"%s\")\n", path);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			fd = ftpopen(uh, path, flags);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			fd = httpopen(uh, path, flags);
			break;
#endif
		default:
			fd = seterrno(ENOENT);
			break;
	}
	if (fd < 0) return(_urlclosedev(uh, -1));
	urlputopenlist(fd, uh, path, flags);

	return(fd);
}

static int NEAR _urlclose(fd, func)
int fd;
CONST char *func;
{
	int n, uh;

	urllog("%s(\"%d\")\n", func, fd);
	if ((uh = urldelopenlist(fd)) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpclose(uh, fd);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpclose(uh, fd);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}
	urlclosedev(uh);

	return(n);
}

int urlclose(fd)
int fd;
{
	int n;

	n = Xclose(fd);
	if (_urlclose(fd, "close") < 0) n = -1;

	return(n);
}

int urlfstat(fd, stp)
int fd;
struct stat *stp;
{
	int n, uh;

	urllog("fstat(\"%d\")\n", fd);
	if ((n = urlgetopenlist(fd)) < 0) return(-1);
	uh = urlopenlist[n].uh;
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpfstat(uh, stp);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpfstat(uh, stp);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}

	return(n);
}

int urlselect(fd)
int fd;
{
	int n, uh;

	if ((n = urlgetopenlist(fd)) < 0) return(-1);
	uh = urlopenlist[n].uh;
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			if (!urlhostlist[uh].http) return(seterrno(EINVAL));
			if ((urlhostlist[uh].http) -> flags & HFL_CHUNKED) {
				if ((urlhostlist[uh].http) -> chunk < 0)
					return(0);
			}
			if ((urlhostlist[uh].http) -> flags & HFL_CLENGTH) {
				if (!((urlhostlist[uh].http) -> clength))
					return(0);
			}
			break;
#endif
		default:
			break;
	}

	return(1);
}

int urlread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	int n, uh;

	if (chkopenfd(fd) == DEV_URL) urllog("read(%d, , %d)\n", fd, nbytes);
	if ((n = urlgetopenlist(fd)) < 0) return(-1);
	uh = urlopenlist[n].uh;
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpread(uh, fd, buf, nbytes);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpread(uh, fd, buf, nbytes);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}

	return(n);
}

int urlwrite(fd, buf, nbytes)
int fd;
CONST char *buf;
int nbytes;
{
	int n, uh;

	if (chkopenfd(fd) == DEV_URL) urllog("write(%d, , %d)\n", fd, nbytes);
	if ((n = urlgetopenlist(fd)) < 0) return(-1);
	uh = urlopenlist[n].uh;
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpwrite(uh, fd, buf, nbytes);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpwrite(uh, fd, buf, nbytes);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}

	return(n);
}

int urldup2(old, new)
int old, new;
{
	int n;

	VOID_C _urlclose(new, "dup2");
	if ((n = urlgetopenlist(old)) >= 0)
		urlputopenlist(new, urlopenlist[n].uh,
			urlopenlist[n].path, urlopenlist[n].flags);

	return(new);
}

int urlmkdir(host, type, path)
CONST char *host;
int type;
CONST char *path;
{
	int n, uh;

	urllog("mkdir(\"%s\")\n", path);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftpmkdir(uh, path);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httpmkdir(uh, path);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}
	urlclosedev(uh);

	return(n);
}

int urlrmdir(host, type, path)
CONST char *host;
int type;
CONST char *path;
{
	int n, uh;

	urllog("rmdir(\"%s\")\n", path);
	if ((uh = urlopendev(host, type)) < 0) return(-1);
	switch (urlhostlist[uh].prototype) {
#ifdef	DEP_FTPPATH
		case TYPE_FTP:
			n = ftprmdir(uh, path);
			break;
#endif
#ifdef	DEP_HTTPPATH
		case TYPE_HTTP:
			n = httprmdir(uh, path);
			break;
#endif
		default:
			n = seterrno(ENOENT);
			break;
	}
	urlclosedev(uh);

	return(n);
}

int urlallclose(VOID_A)
{
	int uh, n;

	n = 0;
	for (uh = maxurlhost - 1; uh >= 0; uh--) {
		if (urlhostlist[uh].flags & UFL_CLOSED) continue;
		urlhostlist[uh].nlink = 0;
		if (_urlclosedev(uh, 0) < 0) n = -1;
	}
	maxurlhost = 0;
	Xfree(logheader);
	logheader = NULL;
	authfree();

	return(n);
}
#endif	/* DEP_URLPATH */
