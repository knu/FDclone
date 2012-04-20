/*
 *	parse.c
 *
 *	command line parser
 */

#ifdef	FD
#include "fd.h"
#include "pathname.h"
#include "term.h"
#include "types.h"
#include "kconv.h"
#else
#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#endif

#include "sysemu.h"
#include "unixemu.h"
#include "realpath.h"
#include "parse.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	USEUNAME
#include <sys/utsname.h>
#endif

#ifdef	FD
# ifndef	DEP_ORIGSHELL
extern int setenv2 __P_((CONST char *, CONST char *, int));
# endif
extern int underhome __P_((char *));
#endif	/* !FD */

#ifdef	FD
extern char fullpath[];
extern short histno[];
extern int physical_path;
#endif

static int NEAR asc2int __P_((int));
#if	defined (FD) && !defined (DEP_ORIGSHELL)
static char *NEAR strtkbrk __P_((CONST char *, CONST char *, int));
static char *NEAR geteostr __P_((CONST char **));
#endif

#ifdef	FD
CONST strtable keyidentlist[] = {
	{K_DOWN,	"DOWN"},
	{K_UP,		"UP"},
	{K_LEFT,	"LEFT"},
	{K_RIGHT,	"RIGHT"},
	{K_HOME,	"HOME"},
	{K_BS,		"BS"},

	{'*',		"ASTER"},
	{'+',		"PLUS"},
	{',',		"COMMA"},
	{'-',		"MINUS"},
	{'.',		"DOT"},
	{'/',		"SLASH"},
	{'0',		"TK0"},
	{'1',		"TK1"},
	{'2',		"TK2"},
	{'3',		"TK3"},
	{'4',		"TK4"},
	{'5',		"TK5"},
	{'6',		"TK6"},
	{'7',		"TK7"},
	{'8',		"TK8"},
	{'9',		"TK9"},
	{'=',		"EQUAL"},
	{K_CR,		"RET"},

	{K_ESC,		"ESC"},
	{'\t',		"TAB"},
	{' ',		"SPACE"},

	{K_DL,		"DELLIN"},
	{K_IL,		"INSLIN"},
	{K_DC,		"DEL"},
	{K_IC,		"INS"},
	{K_EIC,		"EIC"},
	{K_CLR,		"CLR"},
	{K_EOS,		"EOS"},
	{K_EOL,		"EOL"},
	{K_ESF,		"ESF"},
	{K_ESR,		"ESR"},
	{K_NPAGE,	"NPAGE"},
	{K_PPAGE,	"PPAGE"},
	{K_STAB,	"STAB"},
	{K_CTAB,	"CTAB"},
	{K_CATAB,	"CATAB"},
	{K_ENTER,	"ENTER"},
	{K_SRST,	"SRST"},
	{K_RST,		"RST"},
	{K_PRINT,	"PRINT"},
	{K_LL,		"LL"},
	{K_A1,		"A1"},
	{K_A3,		"A3"},
	{K_B2,		"B2"},
	{K_C1,		"C1"},
	{K_C3,		"C3"},
	{K_BTAB,	"BTAB"},
	{K_BEG,		"BEG"},
	{K_CANC,	"CANC"},
	{K_CLOSE,	"CLOSE"},
	{K_COMM,	"COMM"},
	{K_COPY,	"COPY"},
	{K_CREAT,	"CREAT"},
	{K_END,		"END"},
	{K_EXIT,	"EXIT"},
	{K_FIND,	"FIND"},
	{K_HELP,	"HELP"},
	{0,		NULL}
};
#define	KEYIDENTSIZ		(arraysize(keyidentlist) - 1)
#endif	/* FD */

static CONST char escapechar[] = "abefnrtv";
static CONST char escapevalue[] = {
	0x07, 0x08, 0x1b, 0x0c, 0x0a, 0x0d, 0x09, 0x0b,
};


char *skipspace(cp)
CONST char *cp;
{
	while (Xisblank(*cp)) cp++;

	return((char *)cp);
}

static int NEAR asc2int(c)
int c;
{
	if (c < 0) return(-1);
	else if (Xisdigit(c)) return(c - '0');
	else if (Xislower(c)) return(c - 'a' + 10);
	else if (Xisupper(c)) return(c - 'A' + 10);

	return(-1);
}

#ifdef	USESTDARGH
/*VARARGS2*/
char *Xsscanf(CONST char *s, CONST char *fmt, ...)
#else
/*VARARGS2*/
char *Xsscanf(s, fmt, va_alist)
CONST char *s, *fmt;
va_dcl
#endif
{
#ifndef	HAVELONGLONG
	char *buf;
	u_long_t tmp;
	int hi;
#endif
	va_list args;
	CONST char *cp;
	long_t n;
	u_long_t u, mask;
	int i, c, len, base, width, flags;

	if (!s || !fmt) return(NULL);
	VA_START(args, fmt);

	for (i = 0; fmt[i]; i++) {
		if (fmt[i] != '%') {
			if (*(s++) != fmt[i]) {
				s = NULL;
				break;
			}
			continue;
		}

		i++;
		flags = 0;
		for (; fmt[i]; i++) {
			if (!(cp = Xstrchr(printfflagchar, fmt[i]))) break;
			flags |= printfflag[cp - printfflagchar];
		}
		width = getnum(fmt, &i);

		len = sizeof(int);
		for (; fmt[i]; i++) {
			if (!(cp = Xstrchr(printfsizechar, fmt[i]))) break;
			len = printfsize[cp - printfsizechar];
		}
		if (fmt[i] == '*') {
			i++;
			len = va_arg(args, int);
		}

		base = 0;
		switch (fmt[i]) {
			case 'd':
				base = 10;
				break;
			case 'u':
				flags |= VF_UNSIGNED;
				base = 10;
				break;
			case 'o':
				flags |= VF_UNSIGNED;
				base = 8;
				break;
			case 'x':
			case 'X':
				flags |= VF_UNSIGNED;
				base = 16;
				break;
			case 'c':
				if (*(s++) != va_arg(args, int)) s = NULL;
				break;
			case '$':
				if (*s) s = NULL;
				break;
			default:
				if (*(s++) != fmt[i]) s = NULL;
				break;
		}
		if (!s) break;
		if (!base) continue;

		cp = s;
		n = (long_t)0;
		u = (u_long_t)0;
		if (*s == '-') {
			if (!(flags & VF_MINUS)) {
				s = NULL;
				break;
			}
			for (s++; *s; s++) {
#ifdef	LSI_C
			/* for buggy LSI-C */
				long_t minlong;

				minlong = MINTYPE(long_t);
#else
#define	minlong			MINTYPE(long_t)
#endif
				if (width >= 0 && --width < 0) break;
				if ((c = asc2int(*s)) < 0 || c >= base) break;
				if (n == MINTYPE(long_t)) /*EMPTY*/;
				else if (n < minlong / base
				|| (n == minlong / base
				&& -c < MINTYPE(long_t) % base))
					n = MINTYPE(long_t);
				else n = n * base - c;
			}
			if ((flags & VF_UNSIGNED) && n) s = NULL;
		}
		else if (flags & VF_UNSIGNED) {
			for (; *s; s++) {
				if (width >= 0 && --width < 0) break;
				if ((c = asc2int(*s)) < 0 || c >= base) break;
				if (u == MAXUTYPE(u_long_t)) /*EMPTY*/;
				else if (u > MAXUTYPE(u_long_t) / base
				|| (u == MAXUTYPE(u_long_t) / base
				&& c > MAXUTYPE(u_long_t) % base))
					u = MAXTYPE(u_long_t);
				else u = u * base + c;
			}
			if ((flags & VF_PLUS) && !u) s = NULL;
		}
		else {
			for (; *s; s++) {
				if (width >= 0 && --width < 0) break;
				if ((c = asc2int(*s)) < 0 || c >= base) break;
				if (n == MAXTYPE(long_t)) /*EMPTY*/;
				else if (n > MAXTYPE(long_t) / base
				|| (n == MAXTYPE(long_t) / base
				&& c > MAXTYPE(long_t) % base))
					n = MAXTYPE(long_t);
				else n = n * base + c;
			}
			if ((flags & VF_PLUS) && !n) s = NULL;
		}
		if (!s || s <= cp || ((flags & VF_ZERO) && width > 0)) {
			s = NULL;
			break;
		}

		mask = MAXUTYPE(u_long_t);
		if (len < (int)sizeof(u_long_t))
			mask >>= ((int)sizeof(u_long_t) - len) * BITSPERBYTE;
		if (flags & VF_UNSIGNED) {
			if (u & ~mask) {
				u = mask;
				if (flags & VF_STRICTWIDTH) {
					s = NULL;
					break;
				}
			}
		}
		else {
			mask >>= 1;
			if (n >= 0) {
				if ((u_long_t)n & ~mask) {
					n = mask;
					if (flags & VF_STRICTWIDTH) s = NULL;
				}
			}
			else if (((u_long_t)n & ~mask) != ~mask) {
				n = ~mask;
				if (flags & VF_STRICTWIDTH) s = NULL;
			}
			if (!s) break;
			memcpy(&u, &n, sizeof(u));
		}

#ifndef	HAVELONGLONG
		if (len > (int)sizeof(u_long_t)) {
			hi = 0;
			if (!(flags & VF_UNSIGNED)) {
				mask = (MAXUTYPE(u_long_t) >> 1);
				if (u & ~mask) hi = MAXUTYPE(char);
			}
			buf = va_arg(args, char *);
			memset(buf, hi, len);

			tmp = 0x5a;
			cp = (char *)(&tmp);
			if (*cp != 0x5a) buf += len - sizeof(u_long_t);
			memcpy(buf, (char *)(&u), sizeof(u));
		}
		else
#endif	/* !HAVELONGLONG */
		if (len == (int)sizeof(u_long_t))
			*(va_arg(args, u_long_t *)) = u;
#ifdef	HAVELONGLONG
		else if (len == (int)sizeof(u_long))
			*(va_arg(args, u_long *)) = u;
#endif
		else if (len == (int)sizeof(u_short))
			*(va_arg(args, u_short *)) = u;
		else if (len == (int)sizeof(u_char))
			*(va_arg(args, u_char *)) = u;
		else *(va_arg(args, u_int *)) = u;
	}
	va_end(args);

	return((char *)s);
}

int Xatoi(s)
CONST char *s;
{
	int n;

	if (!Xsscanf(s, "%<d%$", &n)) return(-1);

	return(n);
}

#if	defined (FD) && !defined (DEP_ORIGSHELL)
static char *NEAR strtkbrk(s, c, evaldq)
CONST char *s, *c;
int evaldq;
{
	CONST char *cp;
	int pc, quote;

	for (cp = s, quote = '\0'; *cp; cp++) {
# if	MSDOS
		pc = parsechar(cp, -1, '\0', 0, &quote, NULL);
# else
		pc = parsechar(cp, -1, '\0', EA_BACKQ, &quote, NULL);
# endif
		if (pc == PC_OPQUOTE || pc == PC_CLQUOTE) continue;
		else if (pc == PC_SQUOTE || pc == PC_BQUOTE) continue;
		else if (pc == PC_DQUOTE) {
			if (!evaldq) continue;
		}
		else if (pc == PC_WCHAR) {
			cp++;
			continue;
		}
		else if (pc == PC_ESCAPE) {
			cp++;
			if (*cp == PESCAPE && Xstrchr(c, *cp))
				return((char *)&(cp[-1]));
			continue;
		}

		if (Xstrchr(c, *cp)) return((char *)cp);
	}

	return(NULL);
}

char *strtkchr(s, c, evaldq)
CONST char *s;
int c, evaldq;
{
	char tmp[2];

	tmp[0] = c;
	tmp[1] = '\0';

	return(strtkbrk(s, tmp, evaldq));
}

static char *NEAR geteostr(strp)
CONST char **strp;
{
	CONST char *cp;
	char *tmp;
	int len;

	cp = *strp;
	if ((tmp = strtkbrk(*strp, " \t", 0))) len = tmp - *strp;
	else len = strlen(*strp);
	*strp += len;

	return(Xstrndup(cp, len));
}

int getargs(s, argvp)
CONST char *s;
char ***argvp;
{
	CONST char *cp;
	int i;

	*argvp = (char **)Xmalloc(1 * sizeof(char *));
	cp = skipspace(s);
	for (i = 0; *cp; i++) {
		*argvp = (char **)Xrealloc(*argvp, (i + 2) * sizeof(char *));
		(*argvp)[i] = evalpath(geteostr(&cp), EA_NOUNIQDELIM);
		cp = skipspace(cp);
	}
	(*argvp)[i] = NULL;

	return(i);
}

char *gettoken(s)
CONST char *s;
{
	if (!isidentchar(*s)) return(NULL);
	for (s++; isidentchar2(*s); s++) /*EMPTY*/;

	return((char *)s);
}

char *getenvval(argcp, argv)
int *argcp;
char *CONST argv[];
{
	char *cp;
	int i;

	if (*argcp <= 0) return(vnullstr);
	i = 0;
	if (!isidentchar(argv[i][0])) return(vnullstr);

	for (cp = &(argv[i][1]); *cp; cp++) if (!isidentchar2(*cp)) break;
	cp = skipspace(cp);
	if (!*cp) {
		if (++i >= *argcp) return(vnullstr);
		cp = skipspace(argv[i]);
	}
	if (*cp != '=') return(vnullstr);
	*(cp++) = '\0';

	if (!*cp) {
		if (++i >= *argcp) return(NULL);
		cp = argv[i];
	}
	*argcp = i + 1;

	return(evalpath(Xstrdup(cp), EA_NOUNIQDELIM));
}

char *evalcomstr(path, delim)
CONST char *path, *delim;
{
	CONST char *cp, *next;
	char *new, *epath;
	int i, len, size;

	next = epath = NULL;
	size = 0;
	for (cp = path; cp && *cp; cp = next) {
		if ((next = strtkbrk(cp, delim, 0))) {
			len = next - cp;
			for (i = 1; next[i]; i++)
				if (!Xstrchr(delim, next[i])) break;
		}
		else {
			len = strlen(cp);
			i = 0;
		}
		next = cp + len;
		if (len) {
			new = _evalpath(cp, next, EA_NOEVALQ | EA_NOUNIQDELIM);
# ifdef	DEP_FILECONV
			renewkanjiconv(&new, DEFCODE, getkcode(new), L_FNAME);
# endif
			cp = new;
			len = strlen(cp);
		}
# ifdef	FAKEUNINIT
		else new = NULL;	/* fake for -Wuninitialized */
# endif

		epath = Xrealloc(epath, size + len + i + 1);
		if (len) {
			Xstrcpy(&(epath[size]), cp);
			Xfree(new);
			size += len;
		}
		if (i) {
			Xstrncpy(&(epath[size]), next, i);
			size += i;
			next += i;
		}
	}

	if (!epath) return(Xstrdup(nullstr));
	epath[size] = '\0';

	return(epath);
}
#endif	/* FD && !DEP_ORIGSHELL */

char *evalpaths(paths, delim)
CONST char *paths;
int delim;
{
	CONST char *cp, *next;
	char *new, *epath, buf[MAXPATHLEN];
	int len, size;

	next = epath = NULL;
	size = 0;
	for (cp = paths; cp; cp = next) {
		new = NULL;
		next = cp;
#ifdef	DEP_DOSPATH
		if (_dospath(next)) next += 2;
#endif
		next = Xstrchr(next, delim);
		len = (next) ? (next++) - cp : strlen(cp);
		if (len) {
			new = _evalpath(cp, &(cp[len]), 0);
			cp = (isrootdir(new))
				? Xrealpath(new, buf, RLP_READLINK) : new;
			len = strlen(cp);
		}

		epath = Xrealloc(epath, size + len + 1 + 1);
		memcpy(&(epath[size]), cp, len);
		Xfree(new);
		size += len;
		if (next) epath[size++] = delim;
	}

	if (!epath) return(Xstrdup(nullstr));
	epath[size] = '\0';

	return(epath);
}

#if	!MSDOS || !defined (FD) || defined (DEP_ORIGSHELL)
char *killmeta(name)
CONST char *name;
{
	CONST char *cp;
	char *buf, *new;
	int i;

	buf = Xmalloc(strlen(name) * 2 + 2 + 1);
	*buf = (*name == '~') ? '"' : '\0';
	for (cp = name, i = 1; *cp; cp++, i++) {
		if (iswchar(cp, 0)) buf[i++] = *(cp++);
		else if (*cp == PESCAPE) {
			*buf = '"';
			if (Xstrchr(DQ_METACHAR, *(cp + 1)))
				buf[i++] = PESCAPE;
		}
		else if (Xstrchr(METACHAR, *cp)) {
			*buf = '"';
			if (Xstrchr(DQ_METACHAR, *cp)) buf[i++] = PESCAPE;
		}
		buf[i] = *cp;
	}
	if (*(cp = buf)) buf[i++] = *cp;
	else cp++;
	buf[i] = '\0';
	new = Xstrdup(cp);
	Xfree(buf);

	return(new);
}
#endif	/* !MSDOS || !FD || DEP_ORIGSHELL */

#if	defined (FD) && !defined (DEP_ORIGSHELL)
VOID adjustpath(VOID_A)
{
	char *cp, *path;

	if (!(cp = (char *)getenv(ENVPATH))) return;

	path = evalpaths(cp, PATHDELIM);
	if (strpathcmp(path, cp)) setenv2(ENVPATH, path, 1);
	Xfree(path);
}
#endif	/* FD && !DEP_ORIGSHELL */

#ifdef	FD
char *includepath(path, plist)
CONST char *path, *plist;
{
	CONST char *cp, *next;
	char *tmp;
	int len;

	if (!plist || !*plist) return(NULL);
	next = plist;
	for (cp = next; cp && *cp; cp = next) {
# ifdef	DEP_DOSPATH
		if (_dospath(cp)) next += 2;
# endif
		next = Xstrchr(next, PATHDELIM);
		len = (next) ? (next++) - cp : strlen(cp);
		if ((tmp = underpath(path, cp, len))) return(tmp);
	}

	return(NULL);
}
#endif	/* FD */

#if	defined (OLDPARSE) && !defined (_NOARCHIVE)
char *getrange(cp, delim, fp, dp, wp)
CONST char *cp;
int delim;
u_char *fp, *dp, *wp;
{
	char *tmp;
	u_char c;

	*fp = *dp = *wp = 0;

	if (!(cp = Xsscanf(cp, "%c%Cu", delim, &c))) return(NULL);
	*fp = (c) ? c - 1 : FLD_NONE;

	if (*cp == '[') {
		if (!(cp = Xsscanf(++cp, "%Cu]", &c))) return(NULL);
		if (c && c <= MAXUTYPE(u_char) - 128 + 1) *dp = c - 1 + 128;
	}
	else if (*cp == '-') {
		if (cp[1] == ',' || cp[1] == ':') *dp = *(cp++);
		else if (cp[1] == '-' && cp[2] && cp[2] != ',' && cp[2] != ':')
			*dp = *(cp++);
	}
	else if (*cp && *cp != ',' && *cp != ':') *dp = *(cp++);

	if (*cp == '-') {
		if ((tmp = Xsscanf(++cp, "%Cu", &c))) {
			cp = tmp;
			if (c && c <= MAXUTYPE(u_char) - 128) *wp = c + 128;
		}
		else if (*cp && *cp != ',' && *cp != ':') *wp = *(cp++) % 128;
		else return(NULL);
	}

	return((char *)cp);
}
#endif	/* OLDPARSE && !_NOARCHIVE */

int getprintable(buf, size, s, ptr, rwp)
char *buf;
ALLOC_T size;
CONST char *s;
ALLOC_T ptr;
int *rwp;
{
	char tmp[2 + 1];
	int rw;

	getcharwidth(s, ptr, &rw, NULL);
	memcpy(tmp, &(s[ptr]), rw);
	tmp[rw] = '\0';
	if (rwp) *rwp = rw;

	return(Xsnprintf(buf, size, "%^s", tmp));
}

int evalprompt(bufp, prompt)
char **bufp;
CONST char *prompt;
{
#ifndef	NOUID
	uidtable *up;
#endif
#if	!MSDOS && defined (USEUNAME)
	struct utsname uts;
#endif
	char *cp, *tmp, *new, line[MAXPATHLEN];
	ALLOC_T size;
	int i, j, k, rw, v, len, unprint;

	cp = Xstrdup(prompt);
#if	defined (FD) && !defined (DEP_ORIGSHELL)
	prompt = new = evalpath(cp, EA_NOUNIQDELIM);
#else
	prompt = new = evalvararg(cp, EA_BACKQ | EA_KEEPESCAPE, 0);
	Xfree(cp);
#endif
	unprint = 0;
#ifdef	FAKEUNINIT
	size = 0;	/* fake for -Wuninitialized */
#endif
	*bufp = c_realloc(NULL, 0, &size);
	for (i = j = len = 0; prompt[i]; i++) {
		cp = NULL;
		line[0] = '\0';
		if (prompt[i] != ESCAPE) {
			k = 0;
			line[k++] = prompt[i];
			if (iswchar(prompt, i)) line[k++] = prompt[++i];
			line[k] = '\0';
		}
		else switch (prompt[++i]) {
			case '\0':
				i--;
				*line = ESCAPE;
				line[1] = '\0';
				break;
#ifdef	FD
			case '!':
				VOID_C Xsnprintf(line, sizeof(line), "%d",
					(int)histno[0] + 1);
				break;
#endif
#ifndef	NOUID
			case 'u':
				if ((up = finduid(getuid(), NULL)))
					cp = up -> name;
				break;
#endif
#if	!MSDOS
			case 'h':
			case 'H':
# ifdef	USEUNAME
				uname(&uts);
				Xstrcpy(line, uts.nodename);
# else
				gethostname(line, MAXPATHLEN);
# endif
				if (prompt[i] == 'h'
				&& (tmp = Xstrchr(line, '.')))
					*tmp = '\0';
				break;
			case '$':
				*line = (getuid()) ? '$' : '#';
				line[1] = '\0';
				break;
#endif	/* !MSDOS */
			case '~':
#ifdef	FD
				if (underhome(&(line[1]))) {
					line[0] = '~';
					break;
				}
#endif
/*FALLTHRU*/
			case 'w':
#ifdef	FD
				if (!physical_path || !Xgetwd(line))
					cp = fullpath;
#else
				if (!Xgetwd(line)) cp = vnullstr;
#endif
				break;
			case 'W':
#ifdef	FD
				if (!physical_path || !Xgetwd(line))
					tmp = fullpath;
#else
				if (!Xgetwd(line)) tmp = vnullstr;
#endif
				else tmp = line;
#ifdef	DEP_DOSPATH
				if (_dospath(tmp)) tmp += 2;
#endif
				cp = strrdelim(tmp, 0);
				if (cp && (cp != tmp || cp[1])) cp++;
				else cp = tmp;
				break;
			case 'e':
				*line = '\033';
				line[1] = '\0';
				break;
			case '[':
				unprint = 1;
				break;
			case ']':
				unprint = 0;
				break;
			default:
				tmp = Xsscanf(&(prompt[i]), "%<3Co", line);
				if (tmp) i = (tmp - prompt) - 1;
				else *line = prompt[i];
				line[1] = '\0';
				break;
		}
		if (!cp) cp = line;

		while (*cp) {
			*bufp = c_realloc(*bufp, j + MAXCHARWID - 1, &size);
			if (unprint) {
				(*bufp)[j] = *cp;
				rw = v = 1;
			}
			else {

				v = getprintable(&((*bufp)[j]), MAXCHARWID + 1,
					cp, 0, &rw);
				len += v;
#ifdef	CODEEUC
				v = strlen(&((*bufp)[j]));
#endif
			}
			cp += rw;
			j += v;
		}
	}
	(*bufp)[j] = '\0';
	Xfree(new);

	return(len);
}

#if	defined (FD) && !defined (_NOARCHIVE)
char *getext(ext, flagsp)
CONST char *ext;
u_char *flagsp;
{
	char *tmp;

	*flagsp = 0;
# ifndef	OLDPARSE
	if (*ext == '/') {
		ext++;
		*flagsp |= LF_IGNORECASE;
	}
# endif

	if (*ext == '*') tmp = Xstrdup(ext);
	else {
		tmp = Xmalloc(strlen(ext) + 2);
		*tmp = '*';
		Xstrcpy(&(tmp[1]), ext);
	}

	return(tmp);
}

/*ARGSUSED*/
int extcmp(ext1, flags1, ext2, flags2, strict)
CONST char *ext1;
int flags1;
CONST char *ext2;
int flags2, strict;
{
	if (*ext1 == '*') ext1++;
	if (*ext2 == '*') ext2++;
	if (!strict && *ext1 != '.' && *ext2 == '.') ext2++;
# ifndef	PATHNOCASE
	if ((flags1 & LF_IGNORECASE) || (flags2 & LF_IGNORECASE))
		return(Xstrcasecmp(ext1, ext2));
# endif

	return(strpathcmp(ext1, ext2));
}
#endif	/* FD && !_NOARCHIVE */

#ifdef	FD
int getkeycode(cp, identonly)
CONST char *cp;
int identonly;
{
	char *tmp;
	int i, ch;

	if (!cp || !*cp) return(-1);
	ch = (*(cp++) & 0xff);
	if (!*cp) {
		if (identonly) return(-1);
		return(ch);
	}
	switch (ch) {
		case '\\':
			if (identonly) return(-1);
			if ((tmp = Xsscanf(cp, "%3o", &ch))) cp = tmp;
			else {
				for (i = 0; escapechar[i]; i++)
					if (*cp == escapechar[i]) break;
				ch = (escapechar[i]) ? escapevalue[i] : *cp;
				cp++;
			}
			break;
		case '^':
			if (identonly) return(-1);
			ch = Xtoupper(*(cp++));
			if (ch < '?' || ch > '_') return(-1);
			ch = ((ch - '@') & 0x7f);
			break;
		case '@':
			if (identonly) return(-1);
			ch = (Xisalpha(*cp)) ? mkmetakey(*(cp++)) : -1;
			break;
# ifdef	CODEEUC
		case C_EKANA:
			if (identonly) return(-1);
			ch = (Xiskana(*cp)) ? mkekana(*(cp++)) : -1;
			break;
# endif
		case 'F':
			if ((i = Xatoi(cp)) >= 1 && i <= 20) return(K_F(i));
/*FALLTHRU*/
		default:
			cp--;
			for (i = 0; i < KEYIDENTSIZ; i++)
				if (!strcmp(keyidentlist[i].str, cp))
					return(keyidentlist[i].no);
			ch = -1;
			break;
	}
	if (*cp) ch = -1;

	return(ch);
}

CONST char *getkeysym(c, tenkey)
int c, tenkey;
{
	static char buf[MAXCHARWID + 1];
	int i;

	for (i = 0; i < KEYIDENTSIZ; i++)
		if ((u_short)(c) == keyidentlist[i].no) break;
	if (i < KEYIDENTSIZ) {
		if (c > (int)MAXUTYPE(u_char)
		|| tenkey || c == ' ' || Xiscntrl(c))
			return(keyidentlist[i].str);
	}

	i = 0;
	if (c >= K_F(1) && c <= K_F(20))
		VOID_C Xsnprintf(buf, sizeof(buf), "F%d", c - K_F0);
	else if (ismetakey(c)) {
		buf[i++] = '@';
		buf[i++] = c & 0x7f;
	}
# ifdef	CODEEUC
	else if (isekana2(c)) {
		buf[i++] = (char)C_EKANA;
		buf[i++] = c & 0xff;
	}
# else
	else if (Xiskana(c)) buf[i++] = c;
# endif
	else if (c > (int)MAXUTYPE(u_char)) {
		buf[i++] = '?';
		buf[i++] = '?';
	}
	else if (Xiscntrl(c) || ismsb(c)) {
		for (i = 0; escapechar[i]; i++)
			if (c == escapevalue[i]) break;
		if (escapechar[i])
			VOID_C Xsnprintf(buf, sizeof(buf),
				"\\%c", escapechar[i]);
		else VOID_C Xsnprintf(buf, sizeof(buf), "%^c", c);
	}
	else buf[i++] = c;
	if (i > 0) buf[i] = '\0';

	return(buf);
}
#endif	/* FD */

char *decodestr(s, lenp, evalhat)
CONST char *s;
u_char *lenp;
int evalhat;
{
	char *cp, *tmp;
	int i, j, n;

	cp = Xmalloc(strlen(s) + 1);
	for (i = j = 0; s[i]; i++, j++) {
		if (s[i] == '\\') {
			i++;
			if ((tmp = Xsscanf(&(s[i]), "%<3Co", &(cp[j])))) {
				i = (tmp - s) - 1;
				continue;
			}
			for (n = 0; escapechar[n]; n++)
				if (s[i] == escapechar[n]) break;
			cp[j] = (escapechar[n]) ? escapevalue[n] : s[i];
		}
		else if (evalhat && s[i] == '^'
		&& (n = Xtoupper(s[i + 1])) >= '?' && n <= '_') {
			i++;
			cp[j] = ((n - '@') & 0x7f);
		}
		else cp[j] = s[i];
	}

	tmp = Xmalloc(j + 1);
	memcpy(tmp, cp, j);
	tmp[j] = '\0';
	Xfree(cp);
	if (lenp) *lenp = j;

	return(tmp);
}

#if	defined (FD) && !defined (_NOKEYMAP)
char *encodestr(s, len)
CONST char *s;
int len;
{
	char *cp, *tmp;
	ALLOC_T size;
	int i, n;

	size = len * MAXCHARWID + 1;
	cp = tmp = Xmalloc(size);
	if (s) for (i = 0; i < len; i++) {
		if (iswchar(s, i)) {
			*(cp++) = s[i++];
			size--;
		}
# ifndef	CODEEUC
		else if (isskana(s, i)) /*EMPTY*/;
# endif
		else if (Xiscntrl(s[i]) || ismsb(s[i])) {
			for (n = 0; escapechar[n]; n++)
				if (s[i] == escapevalue[n]) break;
			if (escapechar[n]) {
				*(cp++) = '\\';
				*(cp++) = escapechar[n];
				size -= 2;
			}
			else {
				n = Xsnprintf(cp, size, "%^c", s[i]);
#ifdef	CODEEUC
				n = strlen(cp);
#endif
				cp += n;
				size -= n;
			}
			continue;
		}
		*(cp++) = s[i];
		size--;
	}
	*cp = '\0';

	return(tmp);
}
#endif	/* FD && !_NOKEYMAP */
