/*
 *	parse.c
 *
 *	Commandline Parser
 */

#include "fd.h"
#include "func.h"

#ifdef	USEUNAME
#include <sys/utsname.h>
#endif

extern char fullpath[];
extern short histno[];
extern int physical_path;

static int NEAR asc2int __P_((int));
#ifdef	_NOORIGSHELL
static char *NEAR strtkbrk __P_((char *, char *, int));
static char *NEAR geteostr __P_((char **));
#else
#include "system.h"
#endif

strtable keyidentlist[] = {
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
#define	KEYIDENTSIZ	((int)(sizeof(keyidentlist) / sizeof(strtable)) - 1)

static CONST char escapechar[] = "abefnrtv";
static CONST char escapevalue[] = {
	0x07, 0x08, 0x1b, 0x0c, 0x0a, 0x0d, 0x09, 0x0b,
};


char *skipspace(cp)
char *cp;
{
	while (*cp == ' ' || *cp == '\t') cp++;
	return(cp);
}

static int NEAR asc2int(c)
int c;
{
	if (c < 0) return(-1);
	else if (isdigit2(c)) return(c - '0');
	else if (islower2(c)) return(c - 'a' + 10);
	else if (isupper2(c)) return(c - 'A' + 10);
	else return(-1);
}

#ifdef	USESTDARGH
/*VARARGS2*/
char *sscanf2(char *s, CONST char *fmt, ...)
#else
/*VARARGS2*/
char *sscanf2(s, fmt, va_alist)
char *s;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *cp;
	long_t n;
	u_long_t u, mask;
	int i, c, len, base, width, flags;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	if (!s || !fmt) return(NULL);
	for (i = 0; fmt[i]; i++) {
		if (fmt[i] != '%') {
			if (*(s++) != fmt[i]) return(NULL);
			continue;
		}

		i++;
		flags = 0;
		for (; fmt[i]; i++) {
			if (!(cp = strchr(printfflagchar, fmt[i]))) break;
			flags |= printfflag[cp - printfflagchar];
		}
		width = getnum(fmt, &i);

		len = sizeof(int);
		for (; fmt[i]; i++) {
			if (!(cp = strchr(printfsizechar, fmt[i]))) break;
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
				if (*(s++) != va_arg(args, int)) return(NULL);
				break;
			case '$':
				if (*s) return(NULL);
				break;
			default:
				if (*(s++) != fmt[i]) return(NULL);
				break;
		}
		if (!base) continue;

		cp = s;
		n = (long_t)0;
		u = (u_long_t)0;
		if (*s == '-') {
			if (!(flags & VF_MINUS)) return(NULL);
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
			if ((flags & VF_UNSIGNED) && n) return(NULL);
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
			if ((flags & VF_PLUS) && !u) return(NULL);
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
			if ((flags & VF_PLUS) && !n) return(NULL);
		}
		if (s <= cp) return(NULL);
		if ((flags & VF_ZERO) && width > 0) return(NULL);

		mask = (MAXUTYPE(u_long_t)
			>> ((sizeof(long_t) - len) * BITSPERBYTE));
		if (flags & VF_UNSIGNED) {
			if (u & ~mask) return(NULL);
		}
		else {
			mask >>= 1;
			if (n >= 0) {
				if ((u_long_t)n & ~mask) return(NULL);
			}
			else if (((u_long_t)n & ~mask) != ~mask) return(NULL);
			memcpy(&u, &n, sizeof(u));
		}

		if (len == sizeof(u_long_t))
			*(va_arg(args, u_long_t *)) = u;
#ifdef	HAVELONGLONG
		else if (len == sizeof(u_long)) *(va_arg(args, u_long *)) = u;
#endif
		else if (len == sizeof(u_short))
			*(va_arg(args, u_short *)) = u;
		else if (len == sizeof(u_char)) *(va_arg(args, u_char *)) = u;
		else *(va_arg(args, u_int *)) = u;
	}
	va_end(args);

	return(s);
}

#ifdef	_NOORIGSHELL
static char *NEAR strtkbrk(s, c, evaldq)
char *s, *c;
int evaldq;
{
	char *cp;
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
		else if (pc == PC_WORD) {
			cp++;
			continue;
		}
		else if (pc == PC_META) {
			cp++;
			if (*cp == PMETA && strchr(c, *cp)) return(cp - 1);
			continue;
		}

		if (strchr(c, *cp)) return(cp);
	}
	return(NULL);
}

char *strtkchr(s, c, evaldq)
char *s;
int c, evaldq;
{
	char tmp[2];

	tmp[0] = c;
	tmp[1] = '\0';
	return(strtkbrk(s, tmp, evaldq));
}

static char *NEAR geteostr(strp)
char **strp;
{
	char *cp, *tmp;
	int len;

	cp = *strp;
	if ((tmp = strtkbrk(*strp, " \t", 0))) len = tmp - *strp;
	else len = strlen(*strp);
	*strp += len;
	return(strndup2(cp, len));
}

int getargs(s, argvp)
char *s, ***argvp;
{
	char *cp;
	int i;

	*argvp = (char **)malloc2(1 * sizeof(char *));
	cp = skipspace(s);
	for (i = 0; *cp; i++) {
		*argvp = (char **)realloc2(*argvp, (i + 2) * sizeof(char *));
		(*argvp)[i] = evalpath(geteostr(&cp), EA_NOUNIQDELIM);
		cp = skipspace(cp);
	}
	(*argvp)[i] = NULL;
	return(i);
}

char *gettoken(s)
char *s;
{
	if (!isidentchar(*s)) return(NULL);
	for (s++; isidentchar2(*s); s++);
	return(s);
}

char *getenvval(argcp, argv)
int *argcp;
char *argv[];
{
	char *cp;
	int i;

	if (*argcp <= 0) return((char *)-1);
	i = 0;
	if (!isidentchar(argv[i][0])) return((char *)-1);

	for (cp = &(argv[i][1]); *cp; cp++) if (!isidentchar2(*cp)) break;
	cp = skipspace(cp);
	if (!*cp) {
		if (++i >= *argcp) return((char *)-1);
		cp = skipspace(argv[i]);
	}
	if (*cp != '=') return((char *)-1);
	*(cp++) = '\0';

	if (!*cp) {
		if (++i >= *argcp) return((char *)NULL);
		cp = argv[i];
	}
	*argcp = i + 1;
	return(evalpath(strdup2(cp), EA_NOUNIQDELIM));
}

char *evalcomstr(path, delim)
char *path, *delim;
{
# ifndef	_NOKANJIFCONV
	char *tmp;
# endif
	char *cp, *next, *epath;
	int i, len, size;

	epath = next = NULL;
	size = 0;
	for (cp = path; cp && *cp; cp = next) {
		if ((next = strtkbrk(cp, delim, 0))) {
			len = next - cp;
			for (i = 1; next[i] && strchr(delim, next[i]); i++);
		}
		else {
			len = strlen(cp);
			i = 0;
		}
		next = cp + len;
		if (len) {
			cp = _evalpath(cp, next, EA_NOEVALQ | EA_NOUNIQDELIM);
# ifndef	_NOKANJIFCONV
			tmp = newkanjiconv(cp, DEFCODE, getkcode(cp), L_FNAME);
			if (cp != tmp) free(cp);
			cp = tmp;
# endif
			len = strlen(cp);
		}

		epath = realloc2(epath, size + len + i + 1);
		if (len) {
			strcpy(&(epath[size]), cp);
			free(cp);
			size += len;
		}
		if (i) {
			strncpy(&(epath[size]), next, i);
			size += i;
			next += i;
		}
	}

	if (!epath) return(strdup2(""));
	epath[size] = '\0';
	return(epath);
}
#endif	/* _NOORIGSHELL */

char *evalpaths(paths, delim)
char *paths;
int delim;
{
	char *cp, *tmp, *next, *epath, buf[MAXPATHLEN];
	int len, size;

	epath = next = NULL;
	size = 0;
	for (cp = paths; cp; cp = next) {
#ifdef	_USEDOSPATH
		if (_dospath(cp)) next = strchr(&(cp[2]), delim);
		else
#endif
		next = strchr(cp, delim);
		len = (next) ? (next++) - cp : strlen(cp);
		if (len) {
			tmp = _evalpath(cp, cp + len, 0);
			if (!isrootdir(cp)) cp = tmp;
			else cp = realpath2(tmp, buf, 1);
			len = strlen(cp);
		}
#ifdef	FAKEUNINIT
		else tmp = NULL;	/* fake for -Wuninitialized */
#endif
		epath = realloc2(epath, size + len + 1 + 1);
		if (len) {
			strcpy(&(epath[size]), cp);
			free(tmp);
		}
		size += len;
		if (next) epath[size++] = delim;
	}

	if (!epath) return(strdup2(""));
	epath[size] = '\0';
	return(epath);
}

#if	!MSDOS || !defined (_NOORIGSHELL)
char *killmeta(name)
char *name;
{
	char *cp, *buf;
	int i;

	buf = malloc2(strlen(name) * 2 + 2 + 1);
	*buf = (*name == '~') ? '"' : '\0';
	for (cp = name, i = 1; *cp; cp++, i++) {
		if (iskanji1(cp, 0)) buf[i++] = *(cp++);
# ifdef	CODEEUC
		else if (isekana(cp, 0)) buf[i++] = *(cp++);
# endif
		else if (*cp == PMETA) {
			*buf = '"';
			if (strchr(DQ_METACHAR, *(cp + 1))) buf[i++] = PMETA;
		}
		else if (strchr(METACHAR, *cp)) {
			*buf = '"';
			if (strchr(DQ_METACHAR, *cp)) buf[i++] = PMETA;
		}
		buf[i] = *cp;
	}
	if (*(cp = buf)) buf[i++] = *cp;
	else cp++;
	buf[i] = '\0';
	cp = strdup2(cp);
	free(buf);
	return(cp);
}
#endif	/* !MSDOS || !_NOORIGSHELL */

#ifdef	_NOORIGSHELL
VOID adjustpath(VOID_A)
{
	char *cp, *path;

	if (!(cp = getconstvar("PATH"))) return;

	path = evalpaths(cp, PATHDELIM);
	if (strpathcmp(path, cp)) setenv2("PATH", path, 1);
	free(path);
}
#endif	/* _NOORIGSHELL */

char *includepath(path, plist)
char *path, *plist;
{
	char *cp, *tmp, *next;
	int len;

	if (!plist || !*plist) return(NULL);
	next = plist;
	for (cp = next; cp && *cp; cp = next) {
#ifdef	_USEDOSPATH
		if (_dospath(cp)) next = strchr(&(cp[2]), PATHDELIM);
		else
#endif
		next = strchr(cp, PATHDELIM);
		len = (next) ? (next++) - cp : strlen(cp);
		if ((tmp = underpath(path, cp, len))) return(tmp);
	}
	return(NULL);
}

#if	(FD < 2) && !defined (_NOARCHIVE)
char *getrange(cp, delim, fp, dp, wp)
char *cp;
int delim;
u_char *fp, *dp, *wp;
{
	char *tmp;
	u_char c;

	*fp = *dp = *wp = 0;

	if (!(cp = sscanf2(cp, "%c%Cu", delim, &c))) return(NULL);
	*fp = (c) ? c - 1 : 255;

	if (*cp == '[') {
		if (!(cp = sscanf2(++cp, "%Cu]", &c))) return(NULL);
		if (c && c <= MAXUTYPE(u_char) - 128 + 1) *dp = c - 1 + 128;
	}
	else if (*cp == '-') {
		if (cp[1] == ',' || cp[1] == ':') *dp = *(cp++);
		else if (cp[1] == '-' && cp[2] && cp[2] != ',' && cp[2] != ':')
			*dp = *(cp++);
	}
	else if (*cp && *cp != ',' && *cp != ':') *dp = *(cp++);

	if (*cp == '-') {
		if ((tmp = sscanf2(++cp, "%Cu", &c))) {
			cp = tmp;
			if (c && c <= MAXUTYPE(u_char) - 128) *wp = c + 128;
		}
		else if (*cp && *cp != ',' && *cp != ':') *wp = *(cp++) % 128;
		else return(NULL);
	}
	return(cp);
}
#endif	/* (FD < 2) && !_NOARCHIVE */

int evalprompt(bufp, prompt)
char **bufp, *prompt;
{
#ifndef	NOUID
	uidtable *up;
#endif
#if	!MSDOS && defined (USEUNAME)
	struct utsname uts;
#endif
	char *cp, *tmp, line[MAXPATHLEN];
	ALLOC_T size;
	int i, j, k, len, unprint;

#ifdef	_NOORIGSHELL
	prompt = evalpath(strdup2(prompt), EA_NOUNIQDELIM);
#else
	prompt = evalvararg(prompt, '\0', EA_BACKQ | EA_KEEPMETA, 0);
#endif
	unprint = 0;
#ifdef	FAKEUNINIT
	size = 0;	/* fake for -Wuninitialized */
#endif
	*bufp = c_realloc(NULL, 0, &size);
	for (i = j = len = 0; prompt[i]; i++) {
		cp = NULL;
		line[0] = '\0';
		if (prompt[i] != META) {
			k = 0;
			line[k++] = prompt[i];
			if (iskanji1(prompt, i)) line[k++] = prompt[++i];
#ifdef	CODEEUC
			else if (isekana(prompt, i)) line[k++] = prompt[++i];
#endif
			line[k] = '\0';
		}
		else switch (prompt[++i]) {
			case '\0':
				i--;
				*line = META;
				line[1] = '\0';
				break;
			case '!':
				snprintf2(line, sizeof(line), "%d",
					(int)histno[0] + 1);
				break;
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
				strcpy(line, uts.nodename);
# else
				gethostname(line, MAXPATHLEN);
# endif
				if (prompt[i] == 'h'
				&& (tmp = strchr(line, '.')))
					*tmp = '\0';
				break;
			case '$':
				*line = (getuid()) ? '$' : '#';
				line[1] = '\0';
				break;
#endif
			case 'w':
				if (!physical_path || !Xgetwd(line))
					cp = fullpath;
				break;
			case 'W':
				if (!physical_path || !Xgetwd(line))
					tmp = fullpath;
				else tmp = line;
#ifdef	_USEDOSPATH
				if (_dospath(tmp)) tmp += 2;
#endif
				cp = strrdelim(tmp, 0);
				if (cp && (cp != tmp || cp[1])) cp++;
				else cp = tmp;
				break;
			case '~':
				if (underhome(&(line[1]))) line[0] = '~';
				else if (!physical_path || !Xgetwd(line))
					cp = fullpath;
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
				tmp = sscanf2(&(prompt[i]), "%3Co", line);
				if (tmp) i = (tmp - prompt) - 1;
				else *line = prompt[i];
				line[1] = '\0';
				break;
		}
		if (!cp) cp = line;

		while (*cp) {
			*bufp = c_realloc(*bufp, j + 1, &size);
			if (unprint) (*bufp)[j] = *cp;
			else if (iskanji1(cp, 0)) {
				(*bufp)[j++] = *(cp++);
				(*bufp)[j] = *cp;
				len += 2;
			}
#ifdef	CODEEUC
			else if (isekana(cp, 0)) {
				(*bufp)[j++] = *(cp++);
				(*bufp)[j] = *cp;
				len++;
			}
#endif
			else if (!iscntrl2(*cp)) {
				(*bufp)[j] = *cp;
				len++;
			}
			else {
				(*bufp)[j++] = '^';
				(*bufp)[j] = ((*cp + '@') & 0x7f);
				len += 2;
			}
			cp++;
			j++;
		}
	}
	(*bufp)[j] = '\0';
	free(prompt);
	return(len);
}

#ifndef	_NOARCHIVE
char *getext(ext, flagsp)
char *ext;
u_char *flagsp;
{
	char *tmp;

	*flagsp = 0;
# if	FD >= 2
	if (*ext == '/') {
		ext++;
		*flagsp |= LF_IGNORECASE;
	}
# endif

	if (*ext == '*') tmp = strdup2(ext);
	else {
		tmp = malloc2(strlen(ext) + 2);
		*tmp = '*';
		strcpy(&(tmp[1]), ext);
	}
	return(tmp);
}

/*ARGSUSED*/
int extcmp(ext1, flags1, ext2, flags2, strict)
char *ext1;
int flags1;
char *ext2;
int flags2, strict;
{
	if (*ext1 == '*') ext1++;
	if (*ext2 == '*') ext2++;
	if (!strict && *ext1 != '.' && *ext2 == '.') ext2++;
# ifndef	PATHNOCASE
	if ((flags1 & LF_IGNORECASE) || (flags2 & LF_IGNORECASE))
		return(strcasecmp2(ext1, ext2));
# endif
	return(strpathcmp(ext1, ext2));
}
#endif	/* !_NOARCHIVE */

int getkeycode(cp, identonly)
char *cp;
int identonly;
{
	char *tmp;
	int i, ch;

	ch = (*(cp++) & 0xff);
	if (!*cp) {
		if (identonly) return(-1);
		return(ch);
	}
	switch (ch) {
		case '\\':
			if (identonly) return(-1);
			if ((tmp = sscanf2(cp, "%3o", &ch))) cp = tmp;
			else {
				for (i = 0; escapechar[i]; i++)
					if (*cp == escapechar[i]) break;
				ch = (escapechar[i]) ? escapevalue[i] : *cp;
				cp++;
			}
			break;
		case '^':
			if (identonly) return(-1);
			ch = toupper2(*(cp++));
			if (ch < '?' || ch > '_') return(-1);
			ch = ((ch - '@') & 0x7f);
			break;
		case '@':
			if (identonly) return(-1);
			ch = (isalpha2(*cp)) ? mkmetakey(*(cp++)) : -1;
			break;
#ifdef	CODEEUC
		case C_EKANA:
			if (identonly) return(-1);
			ch = (iskana2(*cp)) ? mkekana(*(cp++)) : -1;
			break;
#endif
		case 'F':
			if ((i = atoi2(cp)) >= 1 && i <= 20) return(K_F(i));
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

char *getkeysym(c, tenkey)
int c, tenkey;
{
	static char buf[5];
	int i, j;

	i = 0;
	if (c >= K_F(1) && c <= K_F(20))
		i = snprintf2(buf, sizeof(buf), "F%d", c - K_F0);
	else if (ismetakey(c)) {
		buf[i++] = '@';
		buf[i++] = c & 0x7f;
	}
#ifdef	CODEEUC
	else if (isekana2(c)) {
		buf[i++] = (char)C_EKANA;
		buf[i++] = c & 0xff;
	}
#endif
	else if (c > (int)MAXUTYPE(u_char)) {
		for (j = 0; j < KEYIDENTSIZ; j++)
			if ((u_short)(c) == keyidentlist[j].no) break;
		if (j < KEYIDENTSIZ) {
			if (tenkey || c == ' ' || iscntrl2(c)
			|| keyidentlist[j].no >= K_MIN)
				return(keyidentlist[j].str);
		}

		buf[i++] = '?';
		buf[i++] = '?';
	}
#ifndef	CODEEUC
	else if (iskana2(c)) buf[i++] = c;
#endif
	else if (iscntrl2(c))
		i = snprintf2(buf, sizeof(buf), "^%c", (c + '@') & 0x7f);
	else if (ismsb(c)) i = snprintf2(buf, sizeof(buf), "\\%03o", c);
	else buf[i++] = c;

	buf[i] = '\0';
	return(buf);
}

char *decodestr(s, lenp, evalhat)
char *s;
u_char *lenp;
int evalhat;
{
	char *cp, *tmp;
	int i, j, n;

	cp = malloc2(strlen(s) + 1);
	for (i = j = 0; s[i]; i++, j++) {
		if (s[i] == '\\') {
			i++;
			if ((tmp = sscanf2(&(s[i]), "%3Co", &(cp[j])))) {
				i = (tmp - s) - 1;
				continue;
			}
			for (n = 0; escapechar[n]; n++)
				if (s[i] == escapechar[n]) break;
			cp[j] = (escapechar[n]) ? escapevalue[n] : s[i];
		}
		else if (evalhat && s[i] == '^'
		&& (n = toupper2(s[i + 1])) >= '?' && n <= '_') {
			i++;
			cp[j] = ((n - '@') & 0x7f);
		}
		else cp[j] = s[i];
	}

	s = malloc2(j + 1);
	memcpy(s, cp, j);
	s[j] = '\0';
	free(cp);
	if (lenp) *lenp = j;
	return(s);
}

#ifndef	_NOKEYMAP
char *encodestr(s, len)
char *s;
int len;
{
	char *cp;
	int i, j, n;

	cp = malloc2(len * 4 + 1);
	j = 0;
	if (s) for (i = 0; i < len; i++) {
		if (iskanji1(s, i)) cp[j++] = s[i++];
# ifdef	CODEEUC
		else if (isekana(s, i)) cp[j++] = s[i++];
# else
		else if (isskana(s, i)) /*EMPTY*/;
# endif
		else if (iscntrl2(s[i]) || ismsb(s[i])) {
			for (n = 0; escapechar[n]; n++)
				if (s[i] == escapevalue[n]) break;
			if (escapechar[n]) {
				cp[j++] = '\\';
				cp[j++] = escapechar[n];
			}
			else if (iscntrl2(s[i]))
				j = snprintf2(&(cp[j]), len * 4 + 1 - j,
					"^%c", (s[i] + '@') & 0x7f);
			else j += snprintf2(&(cp[j]), len * 4 + 1 - j,
				"\\%03o", s[i] & 0xff);
			continue;
		}
		cp[j++] = s[i];
	}
	cp[j] = '\0';
	return(cp);
}
#endif	/* !_NOKEYMAP */
