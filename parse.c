/*
 *	parse.c
 *
 *	Commandline Parser
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"

#ifdef	USEUNAME
#include <sys/utsname.h>
#endif

extern char fullpath[];
extern short histno[];
extern int physical_path;

#ifdef	_NOORIGSHELL
static char *NEAR strtkbrk __P_((char *, char *, int));
static char *NEAR geteostr __P_((char **));
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

static char escapechar[] = "abefnrtv";
static char escapevalue[] = {0x07, 0x08, 0x1b, 0x0c, 0x0a, 0x0d, 0x09, 0x0b};


char *skipspace(cp)
char *cp;
{
	while (*cp == ' ' || *cp == '\t') cp++;
	return(cp);
}

char *evalnumeric(cp, np, plus)
char *cp;
long *np;
int plus;
{
	char *top;
	long n;

	top = cp;
	n = 0;
	if (plus < 0 && *cp == '-') {
		for (cp++; isdigit(*cp); cp++) {
			if (n == MINTYPE(long));
			else if (n <= MINTYPE(long) / 10
			&& *cp >= MINTYPE(long) % 10 + '0')
				n = MINTYPE(long);
			else n = n * 10 - (*cp - '0');
		}
	}
	else {
		if (plus > 0) {
			if (*cp < '1' || *cp > '9') return(NULL);
			n = *(cp++) - '0';
		}
		for (; isdigit(*cp); cp++) {
			if (n == MAXTYPE(long));
			else if (n >= MAXTYPE(long) / 10
			&& *cp >= MAXTYPE(long) % 10 + '0')
				n = MAXTYPE(long);
			else n = n * 10 + (*cp - '0');
		}
	}
	if (cp <= top) return(NULL);
	if (np) *np = n;
	return(cp);
}

/*
 *	ascnumeric(buf, n, 0, max): same as sprintf(buf, "%d", n)
 *	ascnumeric(buf, n, max + 1, max): same as sprintf(buf, "%0*d", max, n)
 *	ascnumeric(buf, n, max, max): same as sprintf(buf, "%*d", max, n)
 *	ascnumeric(buf, n, -1, max): same as sprintf(buf, "%-*d", max, n)
 *	ascnumeric(buf, n, x, max): like as sprintf(buf, "%*d", max, n)
 *	ascnumeric(buf, n, -x, max): like as sprintf(buf, "%-*d", max, n)
 */
char *ascnumeric(buf, n, digit, max)
char *buf;
long n;
int digit, max;
{
	char tmp[MAXLONGWIDTH * 2 + 1];
	int i, j, d;

	i = j = 0;
	d = digit;
	if (digit < 0) digit = -digit;
	if (n < 0) tmp[i++] = '?';
	else if (!n) tmp[i++] = '0';
	else {
		for (;;) {
			tmp[i++] = '0' + n % 10;
			if (!(n /= 10) || i >= max) break;
			if (digit > 1 && ++j >= digit) {
				if (i >= max - 1) break;
				tmp[i++] = ',';
				j = 0;
			}
		}
		if (n) for (j = 0; j < i; j++) if (tmp[j] != ',') tmp[j] = '9';
	}

	if (d <= 0) j = 0;
	else if (d > max) for (j = 0; j < max - i; j++) buf[j] = '0';
	else for (j = 0; j < max - i; j++) buf[j] = ' ';
	while (i--) buf[j++] = tmp[i];
	if (d < 0) for (; j < max; j++) buf[j] = ' ';
	buf[j] = '\0';

	return(buf);
}

#ifdef	_NOORIGSHELL
static char *NEAR strtkbrk(s, c, evaldq)
char *s, *c;
int evaldq;
{
	char *cp;
	int quote;

	for (cp = s, quote = '\0'; *cp; cp++) {
		if (*cp == quote) {
			quote = '\0';
			continue;
		}
# ifdef	CODEEUC
		else if (isekana(cp, 0)) {
			cp++;
			continue;
		}
# endif
		else if (iskanji1(cp, 0)) {
			cp++;
			continue;
		}
		else if (quote == '\'') continue;
		else if (ismeta(cp, 0, quote)) {
			cp++;
			if (*cp == PMETA && strchr(c, *cp)) return(cp - 1);
			continue;
		}
# if	MSDOS
		else if (quote == '"' && !evaldq) continue;
		else if (*cp == '\'' || *cp == '"') {
# else
		else if (quote == '`' || (quote == '"' && !evaldq)) continue;
		else if (*cp == '\'' || *cp == '"' || *cp == '`') {
# endif
			quote = *cp;
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
	return(strdupcpy(cp, len));
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
		(*argvp)[i] = evalpath(geteostr(&cp), 0);
		cp = skipspace(cp);
	}
	(*argvp)[i] = NULL;
	return(i);
}

char *gettoken(s)
char *s;
{
	if (!isidentchar(*s)) return(NULL);
	for (s++; isidentchar(*s) || isdigit(*s); s++);
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
	for (cp = argv[i]; *cp; cp++)
		if (!isidentchar(*cp) && (cp == argv[i] || !isdigit(*cp)))
			break;

	if (cp == argv[i]) return((char *)-1);
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
	return(evalpath(strdup2(cp), 0));
}

char *evalcomstr(path, delim)
char *path, *delim;
{
# if	!MSDOS && !defined (_NOKANJIFCONV)
	char *buf;
# endif
	char *cp, *next, *tmp, *epath;
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
			tmp = _evalpath(cp, next, 0, 0);
# if	!MSDOS && !defined (_NOKANJIFCONV)
			len = strlen(tmp) * 3 + 3;
			buf = malloc2(len + 1);
			cp = kanjiconv2(buf, tmp,
				len, DEFCODE, getkcode(tmp));
# else
			cp = tmp;
# endif
			len = strlen(cp);
		}
# ifdef	FAKEUNINIT
		else {
			/* fake for -Wuninitialized */
#  if	!MSDOS && !defined (_NOKANJIFCONV)
			buf =
#  endif
			tmp = NULL;
		}
# endif

		epath = (char *)realloc2(epath, size + len + i + 1);
		if (len) {
			strcpy(&(epath[size]), cp);
# if	!MSDOS && !defined (_NOKANJIFCONV)
			free(buf);
# endif
			free(tmp);
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
#if	MSDOS || !defined (_NODOSDRIVE)
		if (_dospath(cp)) next = strchr(&(cp[2]), delim);
		else
#endif
		next = strchr(cp, delim);
		len = (next) ? (next++) - cp : strlen(cp);
		if (len) {
			tmp = _evalpath(cp, cp + len, 1, 1);
			if (!isrootdir(cp)) cp = tmp;
			else cp = realpath2(tmp, buf, 1);
			len = strlen(cp);
		}
#ifdef	FAKEUNINIT
		else tmp = NULL;	/* fake for -Wuninitialized */
#endif
		epath = (char *)realloc2(epath, size + len + 1 + 1);
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
	char *cp, buf[MAXPATHLEN * 2 + 1];
	int i;
# ifndef	CODEEUC
	int sjis;

	cp = (char *)getenv("LANG");
	sjis = (cp && toupper2(*cp) == 'J' && strchr("AP", toupper2(cp[1])));
# endif

	*buf = (*name == '~') ? '"' : '\0';
	for (cp = name, i = 1; *cp; cp++, i++) {
# ifndef	CODEEUC
		if (sjis && issjis1(cp[0]) && issjis2(cp[1]))
			buf[i++] = *(cp++);
		else
# endif
		if (strchr(METACHAR, *cp)) {
			*buf = '"';
			if (strchr(DQ_METACHAR, *cp)) buf[i++] = PMETA;
		}
		buf[i] = *cp;
	}
	if (*(cp = buf)) buf[i++] = *cp;
	else cp++;
	buf[i] = '\0';
	return(strdup2(cp));
}
#endif	/* !MSDOS || !_NOORIGSHELL */

#if	!MSDOS && defined (_NOORIGSHELL)
VOID adjustpath(VOID_A)
{
	char *cp, *path;

	if (!(cp = (char *)getenv("PATH"))) return;

	path = evalpaths(cp, PATHDELIM);
	if (strpathcmp(path, cp)) {
		cp = (char *)malloc2(strlen(path) + 5 + 1);
		strcpy(strcpy2(cp, "PATH="), path);
		if (putenv2(cp) < 0) error("PATH");
	}
	free(path);
}
#endif	/* !MSDOS && _NOORIGSHELL */

char *includepath(path, plist)
char *path, *plist;
{
	char *cp, *next;
	int len;

	if (!plist || !*plist) return(NULL);
	next = plist;
	for (cp = next; cp && *cp; cp = next) {
#if	MSDOS || !defined (_NODOSDRIVE)
		if (_dospath(cp)) next = strchr(&(cp[2]), PATHDELIM);
		else
#endif
		next = strchr(cp, PATHDELIM);
		len = (next) ? (next++) - cp : strlen(cp);
		while (len > 1 && cp[len - 1] == _SC_) len--;
#if	MSDOS
		if (onkanji1(cp, len - 1)) len++;
#endif
		if (len > 0 && !strnpathcmp(path, cp, len)
		&& (!path[len] || path[len] == _SC_)) return(path + len);
	}
	return(NULL);
}

#if	(FD < 2) && !defined (_NOARCHIVE)
char *getrange(cp, fp, dp, wp)
char *cp;
u_char *fp, *dp, *wp;
{
	char *tmp;
	long n;

	*fp = *dp = *wp = 0;

	if (!(cp = evalnumeric(cp, &n, 0))) return(NULL);
	*fp = (n > 0) ? n - 1 : 255;

	if (*cp == '[') {
		if (!(cp = evalnumeric(++cp, &n, 0))) return(NULL);
		if (n > 0) *dp = n - 1 + 128;
		if (*(cp++) != ']') return(NULL);
	}
	else if (*cp == '-') {
		if (cp[1] == ',' || cp[1] == ':') *dp = *(cp++);
		else if (cp[1] == '-' && cp[2] && cp[2] != ',' && cp[2] != ':')
			*dp = *(cp++);
	}
	else if (*cp && *cp != ',' && *cp != ':') *dp = *(cp++);

	if (*cp == '-') {
		if ((tmp = evalnumeric(++cp, &n, 0))) {
			cp = tmp;
			*wp = n + 128;
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
#if	!MSDOS
	uidtable *up;
#endif
#ifdef	USEUNAME
	struct utsname uts;
#endif
	char *cp, *tmp, line[MAXPATHLEN];
	ALLOC_T size;
	int i, j, k, len, unprint;

	unprint = 0;
#ifdef	FAKEUNINIT
	size = 0;	/* fake for -Wuninitialized */
#endif
	*bufp = c_realloc(NULL, 0, &size);
	for (i = j = len = 0; prompt[i]; i++) {
		cp = NULL;
		*line = '\0';
		if (prompt[i] != META) {
			k = 0;
			line[k++] = prompt[i];
#ifdef	CODEEUC
			if (isekana(prompt, i)) line[k++] = prompt[++i];
			else
#endif
			if (iskanji1(prompt, i)) line[k++] = prompt[++i];
			line[k] = '\0';
		}
		else switch (prompt[++i]) {
			case '\0':
				i--;
				*line = META;
				line[1] = '\0';
				break;
			case '!':
				ascnumeric(line, (long)(histno[0]) + 1L,
					0, MAXPATHLEN - 1);
				break;
#if	!MSDOS
			case 'u':
				if ((up = finduid(getuid(), NULL)))
					cp = up -> name;
				break;
			case 'h':
			case 'H':
#ifdef	USEUNAME
				uname(&uts);
				strcpy(line, uts.nodename);
#else
				gethostname(line, MAXPATHLEN);
#endif
				if (prompt[i] == 'h'
				&& (tmp = strchr(line, '.'))) *tmp = '\0';
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
#if	MSDOS || !defined (_NODOSDRIVE)
				if (_dospath(tmp)) tmp += 2;
#endif
				cp = strrdelim(tmp, 0);
				if (cp && (cp != tmp || cp[1])) cp++;
				else cp = tmp;
				break;
			case '~':
				if (underhome(line + 1)) line[0] = '~';
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
				if (prompt[i] < '0' || prompt[i] > '7') {
					*line = prompt[i];
					line[1] = '\0';
				}
				else {
					*line = prompt[i] - '0';
					for (k = 1; k < 3; k++) {
						if (prompt[i + 1] < '0'
						|| prompt[i + 1] > '7')
							break;
						*line = *line * 8
							+ prompt[++i] - '0';
					}
					line[1] = '\0';
				}
				break;
		}
		if (!cp) cp = line;

		while (*cp) {
			*bufp = c_realloc(*bufp, j + 1, &size);
			if (unprint) (*bufp)[j] = *cp;
#ifdef	CODEEUC
			else if (isekana(cp, 0)) {
				(*bufp)[j++] = *(cp++);
				(*bufp)[j] = *cp;
				len++;
			}
#endif
			else if (iskanji1(cp, 0)) {
				(*bufp)[j++] = *(cp++);
				(*bufp)[j] = *cp;
				len += 2;
			}
			else if (!isctl(*cp)) {
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
	return(len);
}

#if	FD >= 2
char **duplvar(var, margin)
char **var;
int margin;
{
	char **dupl;
	int i, n;

	if (margin < 0) {
		if (!var) return(NULL);
		margin = 0;
	}
	n = countvar(var);
	dupl = (char **)malloc2((n + margin + 1) * sizeof(char *));
	for (i = 0; i < n; i++) dupl[i] = strdup2(var[i]);
	dupl[i] = NULL;
	return(dupl);
}
#endif	/* FD >= 2 */

#ifndef	_NOARCHIVE
/*ARGSUSED*/
char *getext(ext, flagsp)
char *ext;
u_char *flagsp;
{
	char *tmp;

	*flagsp = 0;
	if (*ext == '/') {
		ext++;
# if	!MSDOS
		*flagsp |= LF_IGNORECASE;
# endif
	}

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
# if	!MSDOS
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
	long n;
	int i, ch;

	ch = *(cp++);
	if (!*cp) {
		if (identonly) return(-1);
		return(ch);
	}
	switch (ch) {
		case '\\':
			if (identonly) return(-1);
			if (*cp >= '0' && *cp <= '7') {
				ch = *(cp++) - '0';
				for (i = 1; i < 3; i++) {
					if (*cp < '0' || *cp > '7') break;
					ch = ch * 8 + *(cp++) - '0';
				}
			}
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
#if	MSDOS
			ch = (isalpha(*cp)) ? (tolower2(*(cp++)) | 0x80) : -1;
#else
			ch = (isalpha(*cp)) ? (*(cp++) | 0x80) : -1;
#endif
			break;
		case 'F':
			if ((n = atoi2(cp)) >= 1 && n <= 20) return(K_F(n));
/*FALLTHRU*/
		default:
			cp--;
			for (i = 0; i < KEYIDENTSIZ; i++)
				if (!strcmp(keyidentlist[i].str, cp)) break;
			if (i >= KEYIDENTSIZ) ch = -1;
			else {
				ch = keyidentlist[i].no;
				cp += strlen(cp);
			}
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
	if (c >= K_F(1) && c <= K_F(20)) {
		c -= K_F0;
		buf[i++] = 'F';
		if (c >= 10) buf[i++] = (c / 10) + '0';
		buf[i++] = (c % 10) + '0';
	}
	else if ((c & ~0x7f) == 0x80 && isalpha(c & 0x7f)) {
		buf[i++] = '@';
		buf[i++] = c & 0x7f;
	}
	else {
		for (j = 0; j < KEYIDENTSIZ; j++)
			if ((u_short)(c) == keyidentlist[j].no) break;
		if (j < KEYIDENTSIZ) {
			if (tenkey || c == ' ' || isctl(c)
			|| keyidentlist[j].no >= K_MIN)
				return(keyidentlist[j].str);
		}

		if (c >= K_MIN) {
			buf[i++] = '?';
			buf[i++] = '?';
		}
#ifndef	CODEEUC
		else if (iskna(c)) buf[i++] = c;
#endif
		else if (isctl(c)) {
			buf[i++] = '^';
			buf[i++] = (c + '@') & 0x7f;
		}
		else if (ismsb(c)) {
			buf[i++] = '\\';
			buf[i++] = (c / (8 * 8)) + '0';
			buf[i++] = ((c % (8 * 8)) / 8) + '0';
			buf[i++] = (c % 8) + '0';
		}
		else buf[i++] = c;
	}
	buf[i] = '\0';
	return(buf);
}

char *decodestr(s, lenp, evalhat)
char *s;
int *lenp, evalhat;
{
	char *cp;
	int i, j, n;

	cp = malloc2(strlen(s) + 1);
	for (i = j = 0; s[i]; i++, j++) {
		if (s[i] == '\\') {
			i++;
			if (s[i] >= '0' && s[i] <= '7') {
				cp[j] = s[i] - '0';
				for (n = 1; n < 3; n++) {
					if (s[i + 1] < '0' || s[i + 1] > '7')
						break;
					cp[j] = cp[j] * 8 + (s[++i] - '0');
				}
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

#if	!MSDOS && !defined (_NOKEYMAP)
char *encodestr(s, len)
char *s;
int len;
{
	char *cp;
	int i, j, n;

	cp = malloc2(len * 4 + 1);
	j = 0;
	if (s) for (i = 0; i < len; i++) {
#ifdef	CODEEUC
		if (isekana(s, i)) cp[j++] = s[i++];
		else
#else
		if (iskna(s[i]));
		else
#endif
		if (iskanji1(s, i)) cp[j++] = s[i++];
		else if (isctl(s[i]) || ismsb(s[i])) {
			for (n = 0; escapechar[n]; n++)
				if (s[i] == escapevalue[n]) break;
			if (escapechar[n]) {
				cp[j++] = '\\';
				cp[j++] = escapechar[n];
			}
			else if (isctl(s[i])) {
				cp[j++] = '^';
				cp[j++] = ((s[i] + '@') & 0x7f);
			}
			else {
				int c;

				c = s[i] & 0xff;
				cp[j++] = '\\';
				cp[j++] = (c / (8 * 8)) + '0';
				cp[j++] = ((c % (8 * 8)) / 8) + '0';
				cp[j++] = (c % 8) + '0';
			}
			continue;
		}
		cp[j++] = s[i];
	}
	cp[j] = '\0';
	return(cp);
}
#endif	/* !MSDOS && !_NOKEYMAP */
