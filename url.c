/*
 *	url.c
 *
 *	URL parser
 */

#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "typesize.h"
#include "string.h"
#include "malloc.h"
#include "sysemu.h"
#include "pathname.h"
#include "url.h"

#if	defined (DEP_URLPATH) || defined (DEP_SOCKREDIR)

static scheme_t schemelist[] = {
#define	DEFSCHEME(i, p, t)	{i, strsize(i), p, t}
	DEFSCHEME(SCHEME_FTP, 21, TYPE_FTP),
	DEFSCHEME(SCHEME_HTTP, 80, TYPE_HTTP),
	{NULL, 0, 0, TYPE_UNKNOWN},
};

static CONST u_char uctypetable[256] = {
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0x00 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0x10 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0000, 0001, 0040, 0000, 0001, 0000, 0000,	/* 0x20 */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0030,
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,	/* 0x30 */
	0000, 0000, 0030, 0040, 0001, 0000, 0001, 0040,
	0010, 0000, 0000, 0000, 0000, 0000, 0000, 0000,	/* 0x40 */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,	/* 0x50 */
	0000, 0000, 0000, 0001, 0001, 0001, 0001, 0000,
	0001, 0000, 0000, 0000, 0000, 0000, 0000, 0000,	/* 0x60 */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,	/* 0x70 */
	0000, 0000, 0000, 0001, 0001, 0001, 0000, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0x80 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0x90 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0xa0 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0xb0 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0xc0 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0xd0 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0xe0 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* 0xf0 */
	0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,
};


char *urldecode(s, len)
CONST char *s;
int len;
{
	char *cp;
	int i, j, n, c;

	if (!s) return(NULL);
	if (len < 0) len = strlen(s);
	cp = Xmalloc(len + 1);

	for (i = j = 0; i < len; i++, j++) {
		if (s[i] == '%') {
			cp[j] = '\0';
			for (n = 1; n <= 2; n++) {
				c = Xtolower(s[i + n]);
				if (Xisdigit(c)) c -= '0';
				else if (Xisxdigit(c)) c -= 'a' - 10;
				else break;
				cp[j] = ((cp[j]) << 4) + c;
			}
			if (n >= 2) {
				i += 2;
				continue;
			}
		}
		cp[j] = s[i];
	}
	cp[j++] = '\0';
	cp = Xrealloc(cp, j);

	return(cp);
}

char *urlencode(s, len, mask)
CONST char *s;
int len, mask;
{
	char *cp;
	int i, j;

	if (!s) return(NULL);
	if (len < 0) len = strlen(s);
	cp = Xmalloc(len * 3 + 1);

	for (i = j = 0; i < len; i++, j++) {
		if (uctypetable[(u_char)(s[i])] & mask) {
			cp[j++] = '%';
			cp[j++] = Xtoupper(tohexa((s[i] >> 4) & 0xf));
			cp[j] = Xtoupper(tohexa(s[i] & 0xf));
			continue;
		}
		cp[j] = s[i];
	}
	cp[j++] = '\0';
	cp = Xrealloc(cp, j);

	return(cp);
}

int urlparse(s, scheme, hostp, typep, flags)
CONST char *s;
scheme_t *scheme;
char **hostp;
int *typep, flags;
{
	CONST char *cp, *path;
	int i, n;

	/*
	 * scheme://user:passwd@host:port/path;params?query#fragment
	 * scheme:/path;params?query#fragment
	 */

	if (!scheme) scheme = schemelist;
	if (hostp) *hostp = NULL;
	if (typep) *typep = TYPE_NONURL;
	if (!s) return(-1);

	for (cp = s; *cp; cp++) {
		if (Xisalnum(*cp)) continue;
		if (!Xstrchr(SCHEME_SYMBOLCHAR, *cp)) break;
	}
	n = cp - s;
	if (cp <= s || *(cp++) != ':' || *(cp++) != '/')
		return((flags & UPF_ALLOWNONURL) ? 0 : -1);

	for (i = 0; scheme[i].ident; i++) {
		if (n != scheme[i].len) continue;
		if (!Xstrncasecmp(s, scheme[i].ident, n)) break;
	}
	if (scheme[i].ident) {
		if (typep) *typep = scheme[i].type;
	}
	else {
		if (typep) *typep = TYPE_UNKNOWN;
		if (!(flags & UPF_ALLOWANYSCHEME)) return(-1);
	}

	if (*cp != '/') {
		if (!(flags & UPF_ALLOWABSPATH)) return(-1);
		n = --cp - s;
	}
	else if ((path = Xstrchr(++cp, _SC_))) {
		if (path == cp) return(-1);
		if (hostp) *hostp = Xstrndup(cp, path - cp);
		n = path - s;
	}
	else {
		if (hostp) *hostp = Xstrdup(cp);
		n = strlen(s);
	}

	return(n);
}

int urlgetport(type)
int type;
{
	int i;

	for (i = 0; schemelist[i].ident; i++)
		if (type == schemelist[i].type) break;
	if (!(schemelist[i].ident)) return(-1);

	return(schemelist[i].port);
}

CONST char *urlgetscheme(type)
int type;
{
	int i;

	for (i = 0; schemelist[i].ident; i++)
		if (type == schemelist[i].type) break;
	if (!(schemelist[i].ident)) return(NULL);

	return(schemelist[i].ident);
}

int isurl(s, flags)
CONST char *s;
int flags;
{
	int n;

	n = urlparse(s, NULL, NULL, NULL, flags);

	return((n > 0) ? n : 0);
}

VOID urlfreehost(hp)
urlhost_t *hp;
{
	if (!hp) return;

	Xfree(hp -> user);
	Xfree(hp -> pass);
	Xfree(hp -> host);
	hp -> user = hp -> pass = hp -> host = NULL;
	hp -> port = -1;
}

int urlgethost(s, hp)
CONST char *s;
urlhost_t *hp;
{
	CONST char *cp, *host, *user;
	int i, n, len, hlen, ulen;

	if (!s) return(0);

	if ((host = Xstrchr(s, '@'))) {
		user = s;
		ulen = host++ - s;
	}
	else {
		ulen = 0;
		user = NULL;
		host = s;
	}

	n = 0;
	cp = (user) ? Xmemchr(user, ':', ulen) : NULL;
	if (!cp) hp -> pass = NULL;
	else {
		len = ulen - (cp - user) - 1;
		ulen = cp - user;
		if (len <= 0) hp -> pass = NULL;
		else if (!(hp -> pass = urldecode(++cp, len))) n = -1;
	}

	if (!(cp = Xstrchr(host, ':'))) {
		hlen = strlen(host);
		hp -> port = -1;
	}
	else {
		hlen = cp++ - host;
		if (!*cp) hp -> port = -1;
		else {
			i = 0;
			hp -> port = getnum(cp, &i);
			if (hp -> port < 0 || cp[i]) n = seterrno(EINVAL);
		}
	}

	if (!host || hlen <= 0) hp -> host = NULL;
	else if (!(hp -> host = urldecode(host, hlen))) n = -1;
	if (!user || ulen <= 0) hp -> user = NULL;
	else if (!(hp -> user = urldecode(user, ulen))) n = -1;

	if (n < 0) {
		urlfreehost(hp);
		return(-1);
	}

	return(0);
}

int urlgetpath(s, pp)
CONST char *s;
urlpath_t *pp;
{
	CONST char *cp;

	if (!s || *s != _SC_) return(seterrno(EINVAL));
	pp -> params = pp -> query = pp -> fragment = NULL;

	if (!(cp = strpbrk(s, ";?#"))) {
		pp -> path = Xstrdup(s);
		pp -> params = pp -> query = pp -> fragment = NULL;
		return(0);
	}
	pp -> path = Xstrndup(s, cp - s);
	s = &(cp[1]);

	if (*cp != ';') pp -> params = NULL;
	else {
		if (!(cp = strpbrk(s, "?#"))) {
			pp -> params = Xstrdup(s);
			pp -> query = pp -> fragment = NULL;
			return(0);
		}
		pp -> params = Xstrndup(s, cp - s);
		s = &(cp[1]);
	}

	if (*cp != '?') pp -> query = NULL;
	else {
		if (!(cp = Xstrchr(s, '#'))) {
			pp -> query = Xstrdup(s);
			pp -> fragment = NULL;
			return(0);
		}
		pp -> query = Xstrndup(s, cp - s);
		s = &(cp[1]);
	}

	if (*cp != '#') pp -> fragment = NULL;
	else pp -> fragment = Xstrdup(s);

	return(0);
}
#endif	/* DEP_URLPATH || DEP_SOCKREDIR */
