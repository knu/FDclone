/*
 *	auth.c
 *
 *	authentication for FTP/HTTP connections in RFC2617
 */

#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "string.h"
#include "malloc.h"
#include "time.h"
#include "pathname.h"
#include "encode.h"
#include "parse.h"
#include "socket.h"
#include "auth.h"

#ifdef	MINIX
#include <minix/minlib.h>
#endif
#ifdef	FDSH
#include "sysemu.h"
#endif
#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	FD
#include "kanji.h"
#endif

#define	REALMSTR		"realm"
#define	NONCESTR		"nonce"
#define	ALGORITHMSTR		"algorithm"
#define	QOPSTR			"qop"
#define	OPAQUESTR		"opaque"
#define	MD5_STRSIZ		(MD5_BUFSIZ * 4 * 2)

typedef struct _digest_t {
	char *realm;
	char *nonce;
	char *algorithm;
	char *qop;
	char *opaque;
	char *cnonce;
} digest_t;

#ifdef	DEP_URLPATH

#ifdef	FD
extern int ttyiomode __P_((int));
extern int stdiomode __P_((VOID_A));
extern char *inputstr __P_((CONST char *, int, int, CONST char *, int));
extern char *inputpass __P_((VOID_A));
#endif

#ifdef	FD
extern int isttyiomode;
#endif

static char **NEAR getdigestfield __P_((CONST char *));
static VOID NEAR getdigestval __P_((char **, CONST char *, CONST char *));
static VOID NEAR freedigest __P_((digest_t *));
static int NEAR genhash __P_((char *, ALLOC_T, CONST char *, ...));
static int NEAR catprintf __P_((char **, int, CONST char *, ...));

static auth_t authlist[AUTHNOFILE];
static int maxauth = 0;


char *authgetuser(VOID_A)
{
#ifdef	FD
	int wastty;
#endif
	char *cp;

#ifdef	FD
	if (!(wastty = isttyiomode)) ttyiomode(1);
	cp = inputstr(USER_K, 1, -1, NULL, -1);
	if (!wastty) stdiomode();
#else
	cp = gets2("User: ");
#endif

	if (cp && !*cp) {
		Xfree(cp);
		cp = NULL;
	}

	return(cp);
}

char *authgetpass(VOID_A)
{
	char *cp;

#ifdef	FD
	cp = inputpass();
#else	/* !FD */
# ifdef	NOGETPASS
	VOID_C Xfputs("Password:", Xstderr);
	VOID_C Xfflush(Xstderr);
	if (!(cp = Xfgets(Xstdin))) cp = Xstrdup(nullstr);
# else
	if (!(cp = getpass("Password:"))) cp = vnullstr;
	cp = Xstrdup(cp);
# endif
#endif	/* !FD */

	return(cp);
}

int authfind(hp, type, index)
urlhost_t *hp;
int type, index;
{
	int i;

	if (index < 0 || index > maxauth) index = maxauth;
	for (i = index - 1; i >= 0; i--) {
		if (type != authlist[i].type) continue;
		if (hp -> port != authlist[i].host.port) continue;
		if (cmpsockaddr(hp -> host, authlist[i].host.host)) continue;
		if (!(hp -> user)) hp -> user = Xstrdup(authlist[i].host.user);
		else if (strcmp(hp -> user, authlist[i].host.user)) continue;

		Xfree(hp -> pass);
		hp -> pass = Xstrdup(authlist[i].host.pass);
		break;
	}

	return(i);
}

VOID authentry(hp, type)
urlhost_t *hp;
int type;
{
	auth_t tmp;
	int i;

	if (!hp || !(hp -> user) || !(hp -> pass)) return;

	for (i = maxauth - 1; i >= 0; i--) {
		if (type != authlist[i].type) continue;
		if (hp -> port != authlist[i].host.port) continue;
		if (cmpsockaddr(hp -> host, authlist[i].host.host)) continue;
		if (strcmp(hp -> user, authlist[i].host.user)) continue;
		if (strcmp(hp -> pass, authlist[i].host.pass)) continue;
		break;
	}

	if (i >= 0) {
		if (i == maxauth - 1) return;
		memcpy((char *)&tmp, (char *)&(authlist[i]), sizeof(tmp));
		memmove((char *)&(authlist[i]),
			(char *)&(authlist[i + 1]),
			(maxauth - i - 1) * sizeof(*authlist));
		memcpy((char *)&(authlist[maxauth - 1]),
			(char *)&tmp, sizeof(tmp));
		return;
	}

	if (maxauth < AUTHNOFILE) i = maxauth++;
	else {
		urlfreehost(&(authlist[0].host));
		memmove((char *)&(authlist[0]),
			(char *)&(authlist[1]),
			(maxauth - 1) * sizeof(*authlist));
		i = maxauth - 1;
	}

	authlist[i].host.user = Xstrdup(hp -> user);
	authlist[i].host.pass = Xstrdup(hp -> pass);
	authlist[i].host.host = Xstrdup(hp -> host);
	authlist[i].host.port = hp -> port;
	authlist[i].type = type;
}

VOID authfree(VOID_A)
{
	int i;

	for (i = 0; i < maxauth; i++) urlfreehost(&(authlist[i].host));
	maxauth = 0;
}

static char **NEAR getdigestfield(s)
CONST char *s;
{
	CONST char *cp;
	char **argv;
	int quote, argc;

	argv = (char **)Xmalloc(sizeof(*argv));
	argc = 0;

	for (;;) {
		s = skipspace(s);
		if (!*s) break;
		argv = (char **)Xrealloc(argv, (argc + 2) * sizeof(*argv));
		quote = '\0';
		for (cp = s; *cp; cp++) {
			if (*cp == quote) quote = '\0';
			else if (quote) /*EMPTY*/;
			else if (*cp == '"') quote = *cp;
			else if (*cp == ',') break;
		}
		argv[argc++] = Xstrndup(s, cp - s);
		if (*cp) cp++;
		s = cp;
	}
	argv[argc] = NULL;

	return(argv);
}

static VOID NEAR getdigestval(valp, s, name)
char **valp;
CONST char *s, *name;
{
	CONST char *cp;
	ALLOC_T len;
	int quote;

	if (!valp || *valp) return;
	len = strlen(name);
	if (Xstrncasecmp(s, name, len)) return;
	s += len;
	if (*(s++) != '=') return;

	quote = (*s == '"') ? *(s++) : '\0';
	for (cp = s; *cp; cp++) if (*cp == quote) break;

	*valp = Xstrndup(s, cp - s);
}

static VOID NEAR freedigest(dp)
digest_t *dp;
{
	int duperrno;

	if (!dp) return;

	duperrno = errno;
	Xfree(dp -> realm);
	Xfree(dp -> nonce);
	Xfree(dp -> algorithm);
	Xfree(dp -> qop);
	Xfree(dp -> opaque);
	Xfree(dp -> cnonce);
	errno = duperrno;
}

#ifdef	USESTDARGH
/*VARARGS3*/
static int NEAR genhash(char *buf, ALLOC_T size, CONST char *fmt, ...)
#else
/*VARARGS3*/
static int NEAR genhash(buf, size, fmt, va_alist)
char *buf;
ALLOC_T size;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	u_char md5[MD5_BUFSIZ * 4];
	char *cp;
	ALLOC_T ptr, len;
	int n;

	VA_START(args, fmt);
	n = Xvasprintf(&cp, fmt, args);
	va_end(args);
	if (n < 0) return(-1);

	len = sizeof(md5);
	md5encode(md5, &len, (u_char *)cp, (ALLOC_T)-1);
	Xfree(cp);
	cp = buf;
	*buf = '\0';
	for (ptr = (ALLOC_T)0; ptr < len; ptr++) {
		n = Xsnprintf(cp, size, "%<02x", (int)(md5[ptr]));
		if (n < 0) break;
		cp += n;
		size -= n;
	}

	return(0);
}

#ifdef	USESTDARGH
/*VARARGS3*/
static int NEAR catprintf(char **sp, int len, CONST char *fmt, ...)
#else
/*VARARGS3*/
static int NEAR catprintf(sp, len, fmt, va_alist)
char **sp;
int len;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *cp;
	int n;

	if (len < 0) return(-1);
	VA_START(args, fmt);
	n = Xvasprintf(&cp, fmt, args);
	va_end(args);
	if (n < 0) {
		Xfree(*sp);
		*sp = NULL;
		return(-1);
	}

#ifdef	CODEEUC
	n = strlen(cp);
#endif
	*sp = Xrealloc(*sp, len + n + 1);
	memcpy(&((*sp)[len]), cp, n);
	len += n;
	(*sp)[len] = '\0';
	Xfree(cp);

	return(len);
}

char *authencode(hp, digest, method, path)
urlhost_t *hp;
CONST char *digest, *method, *path;
{
	static int count = 0;
	digest_t auth;
	char *cp, *buf, **argv, res[MD5_STRSIZ + 1];
	char a1[MD5_STRSIZ + 1], a2[MD5_STRSIZ + 1], cnonce[MD5_STRSIZ + 1];
	ALLOC_T size;
	int n;

	if (!digest) {
		n = Xasprintf(&cp, "%s:%s", hp -> user, hp -> pass);
		if (n < 0) return(NULL);
#ifdef	CODEEUC
		n = strlen(cp);
#endif
		size = ((n - 1) / BASE64_ORGSIZ + 1)
			* BASE64_ENCSIZ;
		buf = Xmalloc(strsize(AUTHBASIC) + 1 + size + 1);
		memcpy(buf, AUTHBASIC, strsize(AUTHBASIC));
		buf[strsize(AUTHBASIC)] = ' ';
		n = base64encode(&(buf[strsize(AUTHBASIC) + 1]), size + 1,
			(u_char *)cp, n);
		Xfree(cp);
		if (n < 0) {
			Xfree(buf);
			return(NULL);
		}
		return(buf);
	}

	argv = getdigestfield(digest);
	auth.realm = auth.nonce =
	auth.algorithm = auth.qop = auth.opaque = auth.cnonce = NULL;
	for (n = 0; argv[n]; n++) {
		getdigestval(&(auth.realm), argv[n], REALMSTR);
		getdigestval(&(auth.nonce), argv[n], NONCESTR);
		getdigestval(&(auth.algorithm), argv[n], ALGORITHMSTR);
		getdigestval(&(auth.qop), argv[n], QOPSTR);
		getdigestval(&(auth.opaque), argv[n], OPAQUESTR);
	}
	freevar(argv);

	if (!auth.realm || !auth.nonce) {
		freedigest(&auth);
		errno = EINVAL;
		return(NULL);
	}

	n = genhash(a1, sizeof(a1),
		"%s:%s:%s", hp -> user, auth.realm, hp -> pass);
	if (n < 0) {
		freedigest(&auth);
		return(NULL);
	}
	n = genhash(a2, sizeof(a2), "%s:%s", method, path);
	if (n < 0) {
		freedigest(&auth);
		return(NULL);
	}
	if (!auth.qop)
		n = genhash(res, sizeof(res), "%s:%s:%s", a1, auth.nonce, a2);
	else {
		n = genhash(cnonce, sizeof(cnonce),
			"%s:%d", hp -> host, Xtime(NULL));
		if (n < 0) {
			freedigest(&auth);
			return(NULL);
		}
		n = genhash(res, sizeof(res), "%s:%s:%08x:%s:%s:%s",
			a1, auth.nonce, ++count, cnonce, auth.qop, a2);
	}
	if (n < 0) {
		freedigest(&auth);
		return(NULL);
	}

	buf = NULL;
	n = 0;
	n = catprintf(&buf, n, "%s username=\"%s\"", AUTHDIGEST, hp -> user);
	n = catprintf(&buf, n, ", %s=\"%s\"", REALMSTR, auth.realm);
	n = catprintf(&buf, n, ", %s=\"%s\"", NONCESTR, auth.nonce);
	n = catprintf(&buf, n, ", uri=\"%s\"", path);
	if (auth.algorithm) n = catprintf(&buf, n,
		", %s=\"%s\"", ALGORITHMSTR, auth.algorithm);
	if (auth.qop) {
		n = catprintf(&buf, n, ", %s=\"%s\"", QOPSTR, auth.qop);
		n = catprintf(&buf, n, ", nc=%08x", count);
		n = catprintf(&buf, n, ", cnonce=\"%s\"", cnonce);
	}
	if (auth.opaque) n = catprintf(&buf, n,
		", %s=\"%s\"", OPAQUESTR, auth.opaque);
	n = catprintf(&buf, n, ", response=\"%s\"", res);
	freedigest(&auth);

	return(buf);
}
#endif	/* DEP_URLPATH */
