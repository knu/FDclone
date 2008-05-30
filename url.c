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

#ifdef	DEP_URLPATH

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
	cp = malloc2(len + 1);

	for (i = j = 0; i < len; i++, j++) {
		if (s[i] == '%') {
			cp[j] = '\0';
			for (n = 1; n <= 2; n++) {
				c = tolower2(s[i + n]);
				if (isdigit2(c)) c -= '0';
				else if (isxdigit2(c)) c -= 'a' - 10;
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
	cp = realloc2(cp, j);

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
	cp = malloc2(len * 3 + 1);

	for (i = j = 0; i < len; i++, j++) {
		if (uctypetable[(u_char)(s[i])] & mask) {
			cp[j++] = '%';
			cp[j++] = toupper2(tohexa((s[i] >> 4) & 0xf));
			cp[j] = toupper2(tohexa(s[i] & 0xf));
			continue;
		}
		cp[j] = s[i];
	}
	cp[j++] = '\0';
	cp = realloc2(cp, j);

	return(cp);
}

int urlparse(s, scheme, hostp, typep)
CONST char *s;
scheme_t *scheme;
char **hostp;
int *typep;
{
	CONST char *cp, *path;
	int i, n, type;

	/*
	 * scheme://user:passwd@host:port/path;params?query#fragment
	 */

	type = TYPE_UNKNOWN;
	if (!scheme) scheme = schemelist;
	if (hostp) *hostp = NULL;
	if (typep) *typep = type;
	if (!s) return(0);

	if (scheme == (scheme_t *)nullstr) {
		for (cp = s; *cp; cp++)
			if (!isalnum2(*cp) && *cp != '-' && *cp != '-') break;
		if (cp <= s) return(0);
	}
	else {
		for (i = 0; scheme[i].ident; i++)
			if (!strncasecmp2(s, scheme[i].ident, scheme[i].len))
				break;
		if (!(scheme[i].ident)) return(0);
		cp = &(s[scheme[i].len]);
		type = scheme[i].type;
	}
	if (*(cp++) != ':' || *(cp++) != '/' || *(cp++) != '/') return(0);

	if ((path = strchr2(cp, _SC_))) {
		if (path == cp) return(0);
		if (hostp) *hostp = strndup2(cp, path - cp);
		n = path - s;
	}
	else {
		if (hostp) *hostp = strdup2(cp);
		n = strlen(s);
	}
	if (typep) *typep = type;

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

VOID urlfreehost(hp)
urlhost_t *hp;
{
	if (!hp) return;

	free2(hp -> user);
	free2(hp -> pass);
	free2(hp -> host);
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

	if ((host = strchr2(s, '@'))) {
		user = s;
		ulen = host++ - s;
	}
	else {
		ulen = 0;
		user = NULL;
		host = s;
	}

	n = 0;
	cp = (user) ? memchr2(user, ':', ulen) : NULL;
	if (!cp) hp -> pass = NULL;
	else {
		len = ulen - (cp - user) - 1;
		ulen = cp - user;
		if (len <= 0) hp -> pass = NULL;
		else if (!(hp -> pass = urldecode(++cp, len))) n = -1;
	}

	if (!(cp = strchr2(host, ':'))) {
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
		pp -> path = strdup2(s);
		pp -> params = pp -> query = pp -> fragment = NULL;
		return(0);
	}
	pp -> path = strndup2(s, cp - s);
	s = &(cp[1]);

	if (*cp != ';') pp -> params = NULL;
	else {
		if (!(cp = strpbrk(s, "?#"))) {
			pp -> params = strdup2(s);
			pp -> query = pp -> fragment = NULL;
			return(0);
		}
		pp -> params = strndup2(s, cp - s);
		s = &(cp[1]);
	}

	if (*cp != '?') pp -> query = NULL;
	else {
		if (!(cp = strchr2(s, '#'))) {
			pp -> query = strdup2(s);
			pp -> fragment = NULL;
			return(0);
		}
		pp -> query = strndup2(s, cp - s);
		s = &(cp[1]);
	}

	if (*cp != '#') pp -> fragment = NULL;
	else pp -> fragment = strdup2(s);

	return(0);
}
#endif	/* DEP_URLPATH */
