/*
 *	html.c
 *
 *	HTML parsing in RFC1866
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
#include "termio.h"
#include "parse.h"
#include "html.h"
#include "urldisk.h"

#define	HTTPEQUIVSTR		"HTTP-EQUIV"
#define	CONTENTTYPESTR		"Content-Type"
#define	CONTENTSTR		"CONTENT"
#define	CHARSETSTR		"charset="
#define	HREFSTR			"HREF"
#define	DEFTAG(s, l, n, f)	{s, strsize(s), l, n, f}
#define	DEFAMP(s, c)		{s, strsize(s), c}
#define	constequal(s, c)	(!strncasecmp2(s, c, strsize(c)))
#define	iswcin(s)		((s)[0] == '\033' && (s)[1] == '$' && (s)[2])
#define	iswcout(s)		((s)[0] == '\033' && (s)[1] == '(' && (s)[2])

typedef struct _langtable {
	CONST char *ident;
	u_char lang;
} langtable;

typedef struct _htmltag_t {
	CONST char *ident;
	ALLOC_T len;
	char level;
	char next;
	char *(NEAR *func)__P_((htmlstat_t *, char **));
} htmltag_t;

typedef struct _htmlamp_t {
	CONST char *ident;
	ALLOC_T len;
	int ch;
} htmlamp_t;

#ifdef	DEP_HTTPPATH

static VOID NEAR vhtmllog __P_((CONST char *, va_list));
static char *NEAR fgetnext __P_((htmlstat_t *));
static char *NEAR searchchar __P_((CONST char *, int));
static char *NEAR searchstr __P_((htmlstat_t *, ALLOC_T, CONST char *));
static char *NEAR fgethtml __P_((htmlstat_t *));
static char **NEAR getattr __P_((CONST char *, ALLOC_T));
static char **NEAR getattrval __P_((CONST char *, CONST char *, int));
static int NEAR gettagid __P_((CONST char *, ALLOC_T));
static char *NEAR fgetdir __P_((htmlstat_t *));
static VOID NEAR decodeamp __P_((char *));
static char *NEAR metatag __P_((htmlstat_t *, char **));
static char *NEAR breaktag __P_((htmlstat_t *, char **));
static char *NEAR anchortag __P_((htmlstat_t *, char **));
static int NEAR isanchor __P_((htmlstat_t *, CONST char *, CONST char *));
static int NEAR _datestrconv __P_((char *, CONST char *, CONST char *));
static VOID NEAR datestrconv __P_((char *));

char *htmllogfile = NULL;

static CONST langtable langlist[] = {
	{"Shift_JIS", SJIS},
	{"MS_Kanji", SJIS},
	{"csShiftJIS", SJIS},
	{"Windows-31J", SJIS},
	{"csWindows31J", SJIS},
	{"Extended_UNIX_Code_Packed_Format_for_Japanese", EUC},
	{"csEUCPkdFmtJapanese", EUC},
	{"EUC-JP", EUC},
	{"JIS_Encoding", JUNET},
	{"csJISEncoding", JUNET},
	{"ISO-2022-JP", JUNET},
	{"csISO2022JP", JUNET},
	{"ISO-2022-JP-2", JUNET},
	{"csISO2022JP2", JUNET},
	{"JIS_C6226-1978", O_JUNET},
	{"iso-ir-42", O_JUNET},
	{"csISO42JISC62261978", O_JUNET},
	{"UTF-8", UTF8},
	{"US-ASCII", ENG},
	{"us", ENG},
	{"csASCII", ENG},
	{"ISO-8859-1", ENG},
	{"latin1", ENG},
	{"csISOLatin1", ENG},
};
#define	LANGLISTSIZ		arraysize(langlist)
static CONST htmltag_t taglist[] = {
	DEFTAG("!DOCTYPE", HTML_NONE, HTML_NONE, NULL),
	DEFTAG("HTML", HTML_NONE, HTML_HTML, NULL),
	DEFTAG("HEAD", HTML_HTML, HTML_HEAD, NULL),
	DEFTAG("BODY", HTML_HTML, HTML_BODY, NULL),
	DEFTAG("PRE", HTML_BODY, HTML_PRE, breaktag),
	DEFTAG("META", HTML_HEAD, HTML_NONE, metatag),
	DEFTAG("BR", HTML_BODY, HTML_NONE, breaktag),
	DEFTAG("HR", HTML_BODY, HTML_NONE, breaktag),
	DEFTAG("P", HTML_BODY, HTML_NONE, breaktag),
	DEFTAG("TR", HTML_BODY, HTML_NONE, breaktag),
	DEFTAG("A", HTML_BODY, HTML_NONE, anchortag),
};
#define	TAGLISTSIZ		arraysize(taglist)
static CONST htmlamp_t amplist[] = {
	DEFAMP("amp", '&'),
	DEFAMP("lt", '<'),
	DEFAMP("gt", '>'),
	DEFAMP("quot", '"'),
	DEFAMP("nbsp", ' '),
};
#define	AMPLISTSIZ		arraysize(amplist)
static CONST char *yearlist[] = {
	"/",
	"\307\257",					/* EUC-JP */
	"\224\116",					/* Shift_JIS */
	"\345\271\264",					/* UTF-8 */
	"\304\352",					/* GB */
	"\246\176",					/* Big5 */
	NULL
};
static CONST char *monthlist[] = {
	"/",
	"\267\356",					/* EUC-JP */
	"\214\216",					/* Shift_JIS */
	"\346\234\210",					/* UTF-8 */
	"\324\302",					/* GB */
	"\244\353",					/* Big5 */
	NULL
};
static CONST char *daylist[] = {
	"",
	"\306\374",					/* EUC-JP */
	"\223\372",					/* Shift_JIS */
	"\346\227\245",					/* UTF-8 */
	"\310\325",					/* GB */
	"\244\351",					/* Big5 */
	NULL
};
static CONST char *amlist[] = {
	"AM",
	"\270\341\301\260",				/* EUC-JP */
	"\214\337\221\117",				/* Shift_JIS */
	"\345\215\210\345\211\215",			/* UTF-8 (JP) */
	"\311\317\316\347",				/* GB */
	"\244\127\244\310",				/* Big5 */
	"\344\270\212\345\215\210",			/* UTF-8 (CN) */
	NULL
};
static CONST char *pmlist[] = {
	"PM",
	"\270\341\270\345",				/* EUC-JP */
	"\214\337\214\343",				/* Shift_JIS */
	"\345\215\210\345\276\214",			/* UTF-8 (JP) */
	"\317\302\316\347",				/* GB */
	"\244\125\244\310",				/* Big5 */
	"\344\270\213\345\215\210",			/* UTF-8 (CN) */
	NULL
};
static CONST char **datestrlist[] = {
	yearlist, monthlist, daylist, amlist, pmlist,
};
#define	DATESTRLISTSIZ		arraysize(datestrlist)


static VOID NEAR vhtmllog(fmt, args)
CONST char *fmt;
va_list args;
{
	XFILE *fp;

	if (!htmllogfile || !isrootdir(htmllogfile)) return;
	if (!(fp = Xfopen(htmllogfile, "a"))) return;
	VOID_C vfprintf2(fp, fmt, args);
	Xfclose(fp);
}

#ifdef	USESTDARGH
/*VARARGS1*/
VOID htmllog(CONST char *fmt, ...)
#else
/*VARARGS1*/
VOID htmllog(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;

	VA_START(args, fmt);
	vhtmllog(fmt, args);
	va_end(args);
}

static char *NEAR fgetnext(hp)
htmlstat_t *hp;
{
	char *buf;
	ALLOC_T len;

	buf = Xfgets(hp -> fp);
	if (!buf) return(NULL);
	htmllog("%s\n", buf);

	len = strlen(buf);
	if (!(hp -> max)) {
		free2(hp -> buf);
		hp -> buf = buf;
	}
	else {
		hp -> buf = realloc2(hp -> buf, hp -> max + 1 + len + 1);
		hp -> buf[hp -> max++] = ' ';
		strncpy2(&(hp -> buf[hp -> max]), buf, len);
		free2(buf);
		buf = &(hp -> buf[hp -> max]);
	}
	hp -> max += len;

	return(buf);
}

static char *NEAR searchchar(s, c)
CONST char *s;
int c;
{
	int wc;

	wc = 0;
	for (;;) {
		if (!*s) return(NULL);

		if (iswcin(s)) {
			s += 2;
			wc = 1;
		}
		else if (iswcout(s)) {
			s += 2;
			wc = 0;
		}
		else if (wc) s++;
		else if (*s == c) break;
		s++;
	}

	return((char *)s);
}

static char *NEAR searchstr(hp, ptr, s)
htmlstat_t *hp;
ALLOC_T ptr;
CONST char *s;
{
	char *cp;
	ALLOC_T len;
	int quote, wc;

	len = strlen(s);
	cp = &(hp -> buf[ptr]);
	quote = '\0';
	wc = 0;
	for (;;) {
		if (*cp) /*EMPTY*/;
		else if (htmllvl(hp) == HTML_PRE && *s == '<') {
			hp -> flags |= HTML_NEWLINE;
			return(NULL);
		}
		else if (!(cp = fgetnext(hp))) return(NULL);

		if (iswcin(cp)) {
			cp += 2;
			wc = 1;
		}
		else if (iswcout(cp)) {
			cp += 2;
			wc = 0;
		}
		else if (wc) cp++;
		else if (*cp == quote) quote = '\0';
		else if (quote) /*EMPTY*/;
		else if (*cp == '"' || *cp == '\'') quote = *cp;
		else if (!strncmp(cp, s, len)) break;
		cp++;
	}

	return(&(cp[len - 1]));
}

static char *NEAR fgethtml(hp)
htmlstat_t *hp;
{
	char *cp;
	ALLOC_T top;

	hp -> flags &= HTML_LVL;

	if (hp -> buf && !(hp -> buf[hp -> ptr])) {
		free2(hp -> buf);
		hp -> buf = NULL;
		hp -> ptr = (ALLOC_T)0;
	}
	if (!(hp -> buf) || !(hp -> max)) {
		hp -> max = (ALLOC_T)0;
		if (!fgetnext(hp)) return(NULL);
	}

	top = hp -> ptr;
	if (hp -> buf[top] != '<') {
		if (!(cp = searchstr(hp, top, "<")))
			cp = &(hp -> buf[hp -> max]);
		hp -> ptr = cp - (hp -> buf);
		hp -> len = hp -> ptr - top;
		return(&(hp -> buf[top]));
	}

	top++;
	if (constequal(&(hp -> buf[top]), "!--")) {
		cp = searchstr(hp, top, "-->");
		if (!cp) {
			free2(hp -> buf);
			hp -> buf = NULL;
			hp -> max = (ALLOC_T)0;
			return(NULL);
		}
		hp -> flags |= HTML_COMMENT;
	}
	else {
		if (hp -> buf[top] == '/') {
			hp -> flags |= HTML_CLOSE;
			top++;
		}
		if (!(cp = searchstr(hp, top, ">")))
			cp = &(hp -> buf[hp -> max]);
		else if (!cp[1] && htmllvl(hp) == HTML_PRE)
			hp -> flags |= HTML_NEWLINE;
	}

	hp -> ptr = cp - (hp -> buf);
	hp -> len = hp -> ptr - top;
	if (*cp) hp -> ptr++;
	hp -> flags |= HTML_TAG;

	return(&(hp -> buf[top]));
}

static char **NEAR getattr(s, len)
CONST char *s;
ALLOC_T len;
{
	char **argv;
	int quote, wc, argc;
	ALLOC_T ptr, top;

	argv = (char **)malloc2(sizeof(*argv));
	argc = 0;

	for (ptr = (ALLOC_T)0; ptr < len; ptr++) {
		for (; ptr < len; ptr++) if (!isblank2(s[ptr])) break;
		if (ptr >= len) break;
		argv = (char **)realloc2(argv, (argc + 2) * sizeof(*argv));
		top = ptr;
		quote = '\0';
		wc = 0;
		for (; ptr < len; ptr++) {
			if (iswcin(&(s[ptr]))) {
				ptr += (ALLOC_T)2;
				wc = 1;
			}
			else if (iswcout(&(s[ptr]))) {
				ptr += (ALLOC_T)2;
				wc = 0;
			}
			else if (wc) /*EMPTY*/;
			else if (s[ptr] == quote) quote = '\0';
			else if (quote) /*EMPTY*/;
			else if (s[ptr] == '"' || s[ptr] == '\'')
				quote = s[ptr];
			else if (isblank2(s[ptr])) break;
		}
		argv[argc++] = strndup2(&(s[top]), ptr - top);
	}
	argv[argc] = NULL;

	return(argv);
}

static char **NEAR getattrval(s, name, delim)
CONST char *s, *name;
int delim;
{
	char **argv;
	ALLOC_T ptr, len, top;
	int quote, argc;

	len = strlen(name);
	if (strncasecmp2(s, name, len)) return(NULL);
	s += len;
	if (*(s++) != '=') return(NULL);

	argv = (char **)malloc2(sizeof(*argv));
	argc = 0;

	quote = (*s == '"' || *s == '\'') ? *(s++) : '\0';
	for (ptr = (ALLOC_T)0; s[ptr]; ptr++) {
		while (isblank2(s[ptr])) ptr++;
		if (!s[ptr]) break;
		argv = (char **)realloc2(argv, (argc + 2) * sizeof(*argv));
		top = ptr;
		for (; s[ptr]; ptr++)
			if (s[ptr] == delim || s[ptr] == quote) break;
		argv[argc++] = strndup2(&(s[top]), ptr - top);
		if (!s[ptr] || s[ptr] == quote) break;
	}
	argv[argc] = NULL;

	return(argv);
}

static int NEAR gettagid(s, len)
CONST char *s;
ALLOC_T len;
{
	int n;

	for (n = 0; n < TAGLISTSIZ; n++) {
		if (strncasecmp2(s, taglist[n].ident, taglist[n].len))
			continue;
		if (len == taglist[n].len || isblank2(s[taglist[n].len]))
			return(n);
	}

	return(-1);
}

static char *NEAR fgetdir(hp)
htmlstat_t *hp;
{
	char *cp, **argv;
	int n;

	for (;;) {
		if (!(cp = fgethtml(hp))) return(NULL);
		if (hp -> flags & HTML_COMMENT) continue;
		if (!(hp -> flags & HTML_TAG)) {
			if (htmllvl(hp) < HTML_BODY) continue;
			break;
		}

		n = gettagid(cp, hp -> len);
		if (htmllvl(hp) > HTML_NONE || (hp -> flags & HTML_CLOSE))
			/*EMPTY*/;
		else if (n < 0 || taglist[n].level == HTML_BODY) {
			hp -> flags &= ~HTML_LVL;
			hp -> flags |= HTML_BODY;
		}

		if (n >= 0) {
			if (taglist[n].next == HTML_NONE) {
				if (htmllvl(hp) < taglist[n].level) n = -1;
			}
			else if (hp -> flags & HTML_CLOSE) {
				if (htmllvl(hp) == taglist[n].next) {
					hp -> flags &= ~HTML_LVL;
					hp -> flags |= taglist[n].level;
				}
			}
			else {
				if (htmllvl(hp) == taglist[n].level) {
					hp -> flags &= ~HTML_LVL;
					hp -> flags |= taglist[n].next;
				}
			}

			if (n >= 0 && taglist[n].func) {
				cp += taglist[n].len;
				argv = getattr(cp, hp -> len);
				cp = (*(taglist[n].func))(hp, argv);
				freevar(argv);
				if (cp) break;
			}
		}
	}

	return(cp);
}

static VOID NEAR decodeamp(s)
char *s;
{
	char *cp, *tmp;
	ALLOC_T ptr, len;
	int i, c;

	if (!s) return;
	len = strlen(s);
	for (cp = s; (cp = searchchar(cp, '&')); cp++) {
		tmp = &(cp[1]);
		for (i = 0; i < AMPLISTSIZ; i++)
			if (!strncmp(tmp, amplist[i].ident, amplist[i].len))
				break;
		if (i < AMPLISTSIZ) {
			c = amplist[i].ch;
			ptr = amplist[i].len;
		}
		else if (*tmp != '#') continue;
		else {
			c = 0;
			for (ptr = (ALLOC_T)1; ptr <= (ALLOC_T)3; ptr++) {
				if (!isdigit2(tmp[ptr] )) break;
				c = c * 10 + tmp[ptr] - '0';
			}
		}
		if (tmp[ptr++] != ';') continue;
		*cp = c;
		len -= ptr;
		memmove(tmp, &(tmp[ptr]), len - (tmp - s) + 1);
	}
}

int getcharset(argv)
char *CONST *argv;
{
	char *cp;
	int n;

	cp = NULL;
	for (n = 0; argv[n]; n++) if (constequal(argv[n], CHARSETSTR)) {
		cp = strdup2(&(argv[n][strsize(CHARSETSTR)]));
		break;
	}
	if (!cp) return(-1);

	for (n = 0; n < LANGLISTSIZ; n++)
		if (!strcasecmp2(cp, langlist[n].ident)) break;
	free2(cp);
	if (n >= LANGLISTSIZ) return(-1);

	return((int)(langlist[n].lang));
}

static char *NEAR metatag(hp, argv)
htmlstat_t *hp;
char **argv;
{
	char *cp, **var;
	int n;

	var = NULL;
	for (n = 0; argv[n]; n++)
		if ((var = getattrval(argv[n], HTTPEQUIVSTR, ';'))) break;
	if (!var) return(NULL);

	cp = NULL;
	for (n = 0; var[n]; n++) if (!strcasecmp2(var[n], CONTENTTYPESTR)) {
		cp = var[n];
		break;
	}
	freevar(var);
	if (!cp) return(NULL);

	var = NULL;
	for (n = 0; argv[n]; n++)
		if ((var = getattrval(argv[n], CONTENTSTR, ';'))) break;
	if (!var) return(NULL);

	n = getcharset(var);
	freevar(var);
	if (n >= 0) hp -> charset = n;

	return(NULL);
}

static char *NEAR breaktag(hp, argv)
htmlstat_t *hp;
char **argv;
{
	hp -> flags |= HTML_BREAK;

	return(vnullstr);
}

static char *NEAR anchortag(hp, argv)
htmlstat_t *hp;
char **argv;
{
	char *cp, *new, **var;
	int n;

	hp -> flags |= HTML_ANCHOR;
	if (hp -> flags & HTML_CLOSE) return(vnullstr);

	var = NULL;
	for (n = 0; argv[n]; n++)
		if ((var = getattrval(argv[n], HREFSTR, '\0'))) break;
	if (!var || searchchar(var[0], '?')) {
		freevar(var);
		return(NULL);
	}

	if (!(cp = searchchar(var[0], '#'))) new = strdup2(var[0]);
	else new = strndup2(var[0], cp - var[0]);
	decodeamp(new);
	cp = urldecode(new, -1);
	free2(new);
	freevar(var);

	return(cp);
}

VOID htmlinit(hp, fp, path)
htmlstat_t *hp;
XFILE *fp;
CONST char *path;
{
	if (!hp) return;

	hp -> fp = fp;
	hp -> buf = NULL;
	hp -> path = strdup2(path);
	hp -> ptr = hp -> len = hp -> max = (ALLOC_T)0;
	hp -> charset = NOCNV;
	hp -> flags = 0;
}

VOID htmlfree(hp)
htmlstat_t *hp;
{
	hp -> fp = NULL;
	free2(hp -> buf);
	hp -> buf = NULL;
	free2(hp -> path);
	hp -> path = NULL;
	hp -> max = (ALLOC_T)0;
}

static int NEAR isanchor(hp, s1, s2)
htmlstat_t *hp;
CONST char *s1, *s2;
{
	char *cp, buf[MAXPATHLEN];
	ALLOC_T len;
	int n;

	if (!s2) return(-1);
	len = strlen(s1);
	if (strnpathcmp(s1, s2, len)) {
		cp = strcatdelim2(buf, hp -> path, s1);
		len = cp - buf;
		if (strnpathcmp(buf, s2, len)) return(-1);
	}

	n = (s2[len] == _SC_) ? 1 : 0;
	len += n;
	if (s2[len] && s2[len] != ';') return(-1);

	return(n);
}

static int NEAR _datestrconv(s, from, to)
char *s;
CONST char *from, *to;
{
	char *cp;
	int i, conv;

	cp = s;
	conv = 0;
	while (*s) {
		for (i = 0; from[i]; i++) if (s[i] != from[i]) break;
		if (from[i]) *(cp++) = *(s++);
		else {
			s += i;
			for (i = 0; to[i]; i++) *(cp++) = to[i];
			conv++;
		}
	}
	*cp = '\0';

	return(conv);
}

static VOID NEAR datestrconv(s)
char *s;
{
	int i, j, n;

	for (i = 0; i < DATESTRLISTSIZ; i++) {
		for (j = 1; datestrlist[i][j]; j++) {
			n = _datestrconv(s,
				datestrlist[i][j], datestrlist[i][0]);
			if (n) break;
		}
	}
}

char *htmlfgets(hp)
htmlstat_t *hp;
{
	char *cp, *buf, *top, *new, *url;
	ALLOC_T len, size;
	int n, dir, anchored;

	buf = url = NULL;
	size = (ALLOC_T)0;
	anchored = 0;
	while ((cp = fgetdir(hp))) {
		if (!(hp -> flags & HTML_TAG)) {
			top = cp;
			cp = skipspace(cp);
			len = hp -> len - (cp - top);

			if (len) {
				new = strndup2(cp, len);
				n = isanchor(hp, new, url);
				if (n >= 0) cp = new;
				else {
					decodeamp(new);
					cp = skipspace(new);
					n = isanchor(hp, cp, url);
				}

				if (n < 0) {
					datestrconv(cp);
					dir = 0;
				}
				else {
					cp = urlencode(cp, -1, URL_UNSAFEPATH);
					free2(new);
					new = cp;
					dir = n;
					anchored++;
				}
				free2(url);
				url = NULL;

				if (size) buf[size++] = '\t';
				len = strlen(cp);
				buf = realloc2(buf, size + len + dir + 1);
				memcpy(&(buf[size]), cp, len);
				size += len;
				if (dir) buf[size++] = _SC_;
				buf[size] = '\0';
				free2(new);
			}
		}
		else {
			if (hp -> flags & HTML_ANCHOR) {
				free2(url);
				url = NULL;
				if (cp == vnullstr) /*EMPTY*/;
				else if (urlparse(cp, NULL, NULL, NULL))
					free2(cp);
				else url = cp;
			}
		}

		if (!(hp -> flags & (HTML_BREAK | HTML_NEWLINE))) continue;
		if (buf && anchored) break;
		free2(buf);
		free2(url);
		buf = url = NULL;
		size = (ALLOC_T)0;
	}

	free2(url);
	decodeamp(buf);
	if (!anchored) {
		free2(buf);
		buf = NULL;
	}

	return(buf);
}
#endif	/* DEP_HTTPPATH */
