/*
 *	parse.c
 *
 *	Commandline Parser
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"
#include "funcno.h"

#if	MSDOS
#include "unixemu.h"
#else
#include <sys/param.h>
# ifndef	_NODOSDRIVE
# include "dosdisk.h"
# endif
#endif

#ifndef	_NOARCHIVE
extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
#endif
extern char *macrolist[];
extern int maxmacro;
extern aliastable aliaslist[];
extern int maxalias;
extern userfunctable userfunclist[];
extern int maxuserfunc;
extern bindtable bindlist[];
extern functable funclist[];
#if	!MSDOS && !defined (_NODOSDRIVE)
extern devinfo fdtype[];
#endif
extern char **sh_history;
extern char *helpindex[];
extern int sorttype;
#ifndef	_NOTREE
extern int sorttree;
#endif
#ifndef	_NOWRITEFS
extern int writefs;
#endif
extern int histsize;
extern int savehist;
extern int minfilename;
#ifndef	_NOTREE
extern int dircountlimit;
#endif
extern int showsecond;
#if	!MSDOS && !defined (_NODOSDRIVE)
extern int dosdrive;
#endif
#ifndef	_NOEDITMODE
extern char *editmode;
#endif
extern char *deftmpdir;
#ifndef	_NOROCKRIDGE
extern char *rockridgepath;
#endif
extern int sizeinfo;
#ifndef	_NOCOLOR
extern int ansicolor;
#endif

static char *geteostr();
static char *gettoken();
static char *getenvval();
#ifndef	_NOARCHIVE
static char *getrange();
static int getlaunch();
#endif
static int getcommand();
static int getkeybind();
static int getalias();
#if	!MSDOS && !defined (_NODOSDRIVE)
static int getdosdrive();
#endif
static int unalias();
static int getuserfunc();
static VOID printext();


char *skipspace(cp)
char *cp;
{
	while (*cp == ' ' || *cp == '\t') cp++;
	return(cp);
}

char *strtkbrk(s, c, evaldq)
char *s, *c;
int evaldq;
{
	char *cp;
	int q;

	q = '\0';
	for (cp = s; *cp; cp++) {
		if (q == '\'' || (evaldq && q == '"')) continue;
		if (*cp == '\\') {
			if (!*(++cp)) return(NULL);
			else if (*cp == '\\' && (strchr(c, '\\'))) return(cp);
			continue;
		}
		if (strchr(c, *cp)) return(cp);
		if (*cp == '\'') switch (q) {
			case '\'':
				q = '\0';
				break;
			case '"':
				break;
			default:
				q = '\'';
				break;
		}
		else if (*cp == '"') switch (q) {
			case '"':
				q = '\0';
				break;
			case '\'':
				break;
			default:
				q = '"';
				break;
		}
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

static char *geteostr(strp)
char **strp;
{
	char *cp, *tmp;
	int c, len;

	cp = *strp;
	if ((c = **strp) != '"' && c != '\'') {
		if (tmp = strtkbrk(*strp, " \t", 0)) len = tmp - *strp;
		else len = strlen(*strp);
		*strp += len;
	}
	else {
		for (*strp = ++cp; *strp = strtkchr(*strp, c, 0); (*strp)++)
			if (*(*strp - 1) != '\\') break;
		if (*strp) len = (*strp)++ - cp;
		else {
			len = strlen(cp);
			*strp = cp + len;
		}
	}
	tmp = (char *)malloc2(len + 1);
	strncpy2(tmp, cp, len);
	if (c != '\'') tmp = evalpath(tmp);
	return(tmp);
}

static char *gettoken(strp, delim)
char **strp, *delim;
{
	char *cp, *tmp;
	int len;

	cp = *strp;
	if (!isalpha(**strp) && **strp != '_') return(NULL);
	for ((*strp)++; isalnum(**strp) || **strp == '_'; (*strp)++);
	if (!strchr(delim, **strp)) return(NULL);

	len = *strp - cp;
	*strp = skipspace(*strp);
	tmp = (char *)malloc2(len + 1);
	strncpy2(tmp, cp, len);
	return(tmp);
}

static char *getenvval(strp)
char **strp;
{
	char *cp;

	cp = *strp;
	if (!(*strp = gettoken(&cp, " \t=")) || *cp != '=') return((char *)-1);
	cp = skipspace(cp + 1);
	if (!*cp) return(NULL);
	cp = geteostr(&cp);
	return(cp);
}

char *evalcomstr(path, delim)
char *path, *delim;
{
	char *cp, *next, *tmp, *epath;
	int len;

	epath = next = NULL;
	len = 0;
	for (cp = path; cp && *cp; cp = next) {
		if ((next = strtkchr(cp, '\'', 0))
		|| (next = strtkbrk(cp, delim, 0))) {
			tmp = _evalpath(cp, next);
			cp = next;
			if (*next != '\'')
				while (*(++next) && strchr(delim, *next));
			else if (next = strtkchr(next + 1, '\'', 0)) next++;
			else next = cp + strlen(cp);
		}
		else {
			next = cp + strlen(cp);
			tmp = _evalpath(cp, next);
			if (!epath) {
				epath = tmp;
				break;
			}
			cp = next;
		}
		epath = (char *)realloc2(epath,
			len + strlen(tmp) + (next - cp) + 1);
		strcpy(epath + len, tmp);
		len += strlen(tmp);
		free(tmp);
		strncpy(epath + len, cp, next - cp);
		len += next - cp;
		epath[len] = '\0';
	}

	return(epath);
}

#if	!MSDOS
char *killmeta(name)
char *name;
{
#ifndef	_NOROCKRIDGE
	char tmp[MAXPATHLEN + 1];
#endif
	char *cp, buf[MAXPATHLEN * 2 + 1];
	int i;
#ifndef	CODEEUC
	int sjis;

	cp = (char *)getenv("LANG");
	sjis = (cp && toupper2(*cp) == 'J'
		&& strchr("AP", toupper2(*(cp + 1))));
#endif

#ifndef	_NOROCKRIDGE
	name = detransfile(name, tmp);
#endif
	for (cp = name, i = 0; *cp; cp++, i++) {
#ifndef	CODEEUC
		if (sjis && iskanji1(*cp)) buf[i++] = *(cp++);
		else
#endif
		if (strchr(METACHAR, *cp)) buf[i++] = '\\';
		buf[i] = *cp;
	}
	buf[i] = '\0';
	return(strdup2(buf));
}

VOID adjustpath()
{
	char *cp, *path;

	if (!(cp = (char *)getenv("PATH"))) return;

	path = evalcomstr(cp, ":");
	if (strpathcmp(path, cp)) {
#ifdef	USESETENV
		if (setenv("PATH", path, 1) < 0) error("PATH");
#else
		cp = (char *)malloc2(strlen(path) + 5 + 1);
		strcpy(cp, "PATH=");
		strcpy(cp + 5, path);
		putenv2(cp);
#endif
	}
	free(path);
}
#endif	/* !MSDOS */

int getargs(args, argv, max)
char *args, *argv[];
int max;
{
	char *cp;
	int i;

	cp = skipspace(args);
	for (i = 0; i < max && *cp; i++) {
		argv[i] = geteostr(&cp);
		cp = skipspace(cp);
	}
	argv[i] = NULL;

	return(i);
}

#ifndef	_NOARCHIVE
static char *getrange(cp, fp, dp, wp)
char *cp;
u_char *fp, *dp, *wp;
{
	int i;

	*fp = *dp = *wp = 0;

	if (*cp >= '0' && *cp <= '9') *fp = ((i = atoi(cp)) > 0) ? i - 1 : 255;
	while (*cp >= '0' && *cp <= '9') cp++;

	if (*cp == '\'') {
		*dp = *(++cp);
		if (*cp && *(++cp) == '\'') cp++;
	}
	else if (*cp == '[') {
		if ((i = atoi(++cp)) >= 1) *dp = i - 1 + 128;
		while (*cp >= '0' && *cp <= '9') cp++;
		if (*cp == ']') cp++;
	}

	if (*cp == '-') {
		cp++;
		if (*cp >= '0' && *cp <= '9') *wp = atoi(cp) + 128;
		else if (*cp == '\'') *wp = (*(++cp)) % 128;
	}
	return(cp);
}

static int getlaunch(line)
char *line;
{
	char *cp, *tmp, *ext, *eol;
	int i, ch, n;

	cp = line;
	eol = line + strlen(line);

	tmp = geteostr(&cp);
	ext = cnvregexp(tmp, 0);
	if (*tmp != '*') {
		free(tmp);
		tmp = ext;
		ext = (char *)malloc2(strlen(tmp) + 3);
		*ext = *tmp;
		strcpy(ext + 2, tmp);
		memcpy(ext + 1, "^.*", 3);
	}
	free(tmp);

	cp = skipspace(cp);
	if (*cp == 'A') {
		for (n = 0; n < maxarchive; n++)
			if (!strpathcmp(archivelist[n].ext, ext)) break;
		if (n < maxarchive) {
			free(archivelist[n].ext);
			free(archivelist[n].p_comm);
			if (archivelist[n].u_comm) free(archivelist[n].u_comm);
		}
		else if (maxarchive < MAXARCHIVETABLE) maxarchive++;
		else {
			free(ext);
			return(-1);
		}

		tmp = NULL;
		if ((cp = skipspace(++cp)) >= eol
		|| *(tmp = geteostr(&cp)) == '!') {
			maxarchive--;
			for (; n < maxarchive; n++)
				memcpy(&archivelist[n], &archivelist[n + 1],
					sizeof(archivetable));
			free(ext);
			if (tmp) free(tmp);
			return(0);
		}

		archivelist[n].ext = ext;
		archivelist[n].p_comm = tmp;

		if (cp >= eol || (cp = skipspace(cp)) >= eol)
			archivelist[n].u_comm = NULL;
		else if (*(archivelist[n].u_comm = geteostr(&cp)) == '!') {
			free(archivelist[n].u_comm);
			archivelist[n].u_comm = NULL;
		}
	}
	else {
		for (n = 0; n < maxlaunch; n++)
			if (!strpathcmp(launchlist[n].ext, ext)) break;
		if (n < maxlaunch) {
			free(launchlist[n].ext);
			free(launchlist[n].comm);
		}
		else if (maxlaunch < MAXLAUNCHTABLE) maxlaunch++;
		else {
			free(ext);
			return(-1);
		}

		if (cp >= eol) {
			maxlaunch--;
			for (; n < maxlaunch; n++)
				memcpy(&launchlist[n], &launchlist[n + 1],
					sizeof(launchtable));
			free(ext);
			return(0);
		}

		launchlist[n].ext = ext;
		launchlist[n].comm = geteostr(&cp);
		cp = skipspace(cp);
		if (cp >= eol) launchlist[n].topskip = 255;
		else {
			launchlist[n].topskip = atoi(cp);
			if (!(tmp = strchr(cp, ','))) {
				launchlist[n].topskip = 255;
				return(0);
			}
			cp = skipspace(++tmp);
			launchlist[n].bottomskip = atoi(cp);
			ch = ':';
			for (i = 0; i < MAXLAUNCHFIELD; i++) {
				if (!(tmp = strchr(cp, ch))) {
					launchlist[n].topskip = 255;
					return(0);
				}
				cp = skipspace(++tmp);
				cp = getrange(cp,
					&(launchlist[n].field[i]),
					&(launchlist[n].delim[i]),
					&(launchlist[n].width[i]));
				ch = ',';
			}
			ch = ':';
			for (i = 0; i < MAXLAUNCHSEP; i++) {
				if (!(tmp = strchr(cp, ch))) break;
				cp = skipspace(++tmp);
				launchlist[n].sep[i] =
					((ch = atoi(cp)) >= 0) ? ch - 1 : 255;
				ch = ',';
			}
			for (; i < MAXLAUNCHSEP; i++) launchlist[n].sep[i] = -1;
			if (!(tmp = strchr(cp, ':'))) launchlist[n].lines = 1;
			else {
				cp = skipspace(++tmp);
				if ((ch = atoi(cp)) > 1)
					launchlist[n].lines = ch;
			}
		}
	}
	return(0);
}
#endif	/* !_NOARCHIVE */

static int getcommand(cp)
char **cp;
{
	char *tmp;
	int n;

	if (**cp == '"' || **cp == '\'') {
		if (maxmacro >= MAXMACROTABLE) n = -1;
		else {
			macrolist[maxmacro] = geteostr(cp);
			n = NO_OPERATION + ++maxmacro;
		}
	}
	else {
		if (!(tmp = strpbrk(*cp, " \t"))) tmp = *cp + strlen(*cp);
		*tmp = '\0';
		for (n = 0; n < NO_OPERATION; n++)
			if (!strcmp(*cp, funclist[n].ident)) break;
		if (n >= NO_OPERATION) n = -1;
		*cp = tmp;
	}
	return(n);
}

static int getkeybind(line)
char *line;
{
	char *cp, *eol;
	int i, j, ch, n1, n2;

	for (eol = line, ch = '\0'; *eol; eol++) {
		if (*eol == '\\' && *(eol + 1)) eol++;
		else if (*eol == ch) ch = '\0';
		else if ((*eol == '"' || *eol == '\'') && !ch) ch = *eol;
		else if (*eol == ';' && !ch) break;
	}

	cp = line + 1;
	switch (ch = *(cp++)) {
		case '^':
			if (*cp == '\'') break;
			if (*cp < '@' || *cp > '_') return(-1);
			ch = toupper2(*(cp++)) - '@';
			break;
		case 'F':
			if (*cp == '\'') break;
			if ((i = atoi2(cp)) < 1 || i > 20) return(-1);
			for (; *cp >= '0' && *cp <= '9'; cp++);
			ch = K_F(i);
		default:
			break;
	}

	if (ch == '\033' || *(cp++) != '\'') return(-1);

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
		if (ch == (int)(bindlist[i].key)) break;
	if (i >= MAXBINDTABLE - 1) return(-1);
	if (bindlist[i].key < 0)
		memcpy(&bindlist[i + 1], &bindlist[i], sizeof(bindtable));
	else {
		n1 = bindlist[i].f_func;
		n2 = bindlist[i].d_func;
		bindlist[i].f_func = bindlist[i].d_func = NO_OPERATION;
		if (n1 <= NO_OPERATION) n1 = 255;
		else {
			free(macrolist[n1 - NO_OPERATION - 1]);
			maxmacro--;
			for (j = n1 - NO_OPERATION - 1; j < maxmacro; j++)
				memcpy(&macrolist[j], &macrolist[j + 1],
					sizeof(char *));
		}
		if (n2 <= NO_OPERATION || n2 >= 255) n2 = 255;
		else {
			free(macrolist[n2 - NO_OPERATION - 1]);
			maxmacro--;
			for (j = n2 - NO_OPERATION - 1; j < maxmacro; j++)
				memcpy(&macrolist[j], &macrolist[j + 1],
					sizeof(char *));
		}
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key >= 0; j++) {
			if (bindlist[j].f_func > n2) bindlist[j].f_func--;
			if (bindlist[j].f_func >= n1) bindlist[j].f_func--;
			if (bindlist[j].d_func < 255) {
				if (bindlist[j].d_func > n2)
					bindlist[j].d_func--;
				if (bindlist[j].d_func >= n1)
					bindlist[j].d_func--;
			}
		}
	}

	if ((cp = skipspace(cp)) >= eol) {
		for (; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			memcpy(&bindlist[i], &bindlist[i + 1],
				sizeof(bindtable));
		return(0);
	}
	if ((n1 = getcommand(&cp)) < 0) return(-1);
	n2 = ((cp = skipspace(cp)) < eol) ? getcommand(&cp) : -1;
	if (*(eol++) == ';' && ch >= K_F(1) && ch <= K_F(10)) {
		free(helpindex[ch - K_F(1)]);
		helpindex[ch - K_F(1)] = geteostr(&eol);
	}

	bindlist[i].key = (short)ch;
	bindlist[i].f_func = (u_char)n1;
	bindlist[i].d_func = (n2 >= 0) ? (u_char)n2 : 255;
	return(0);
}

static int getalias(line)
char *line;
{
	char *cp;
	int i;

	cp = line;

	if (!(line = gettoken(&cp, " \t="))) return(-1);
	if (*cp == '=') cp = skipspace(cp + 1);

	for (i = 0; i < maxalias; i++)
		if (!strcmp(aliaslist[i].alias, line)) break;
	if (!*cp) {
		if (i < maxalias) kanjiprintf("%s\r\n", aliaslist[i].comm);
		free(line);
		return(1);
	}

	if (i < maxalias) {
		free(aliaslist[i].comm);
		free(aliaslist[i].alias);
	}
	else if (maxalias < MAXALIASTABLE) maxalias++;
	else {
		free(line);
		return(-1);
	}

	aliaslist[i].alias = line;
	aliaslist[i].comm = geteostr(&cp);
	return(0);
}

static int unalias(line)
char *line;
{
	reg_t *re;
	char *cp;
	int i, j, n;

	cp = cnvregexp(line, 0);
	re = regexp_init(cp);
	free(cp);
	for (i = 0, n = 0; i < maxalias; i++)
		if (regexp_exec(re, aliaslist[i].alias)) {
			free(aliaslist[i].alias);
			free(aliaslist[i].comm);
			maxalias--;
			for (j = i; j < maxalias; j++)
				memcpy(&aliaslist[j], &aliaslist[j + 1],
					sizeof(aliastable));
			i--;
			n++;
		}
	regexp_free(re);
	return((n) ? 0 : -1);
}

#if	!MSDOS && !defined (_NODOSDRIVE)
static int getdosdrive(line)
char *line;
{
	char *cp, *tmp;
	int i, drive, head, sect, cyl;

	for (i = 0; fdtype[i].name; i++);
	if (i >= MAXDRIVEENTRY - 1) return(-1);
	drive = toupper2(*line);
	cp = skipspace(line + 2);
	if (!*(tmp = geteostr(&cp))) {
		free(tmp);
		return(-1);
	}
	cp = skipspace(cp);
	head = atoi(cp);
	if (head <= 0 || !(cp = strchr(cp, ','))) {
		free(tmp);
		return(-1);
	}
	cp = skipspace(cp + 1);
	sect = atoi(cp);
	if (sect <= 0 || !(cp = strchr(cp, ','))) {
		free(tmp);
		return(-1);
	}
	cp = skipspace(cp + 1);
	cyl = atoi(cp);
	if (cyl <= 0) {
		free(tmp);
		return(-1);
	}
	if (*(line + 1) == '!') {
		for (i = 0; fdtype[i].name; i++)
		if (drive == fdtype[i].drive
		&& head == fdtype[i].head
		&& sect == fdtype[i].sect
		&& cyl == fdtype[i].cyl
		&& !strcmp(tmp, fdtype[i].name)) break;
		free(tmp);
		if (!fdtype[i].name) return(-1);
		free(fdtype[i].name);
		for (; fdtype[i + 1].name; i++) {
			fdtype[i].drive = fdtype[i + 1].drive;
			fdtype[i].name = fdtype[i + 1].name;
			fdtype[i].head = fdtype[i + 1].head;
			fdtype[i].sect = fdtype[i + 1].sect;
			fdtype[i].cyl = fdtype[i + 1].cyl;
		}
		fdtype[i].name = NULL;
		return(0);
	}

	fdtype[i].drive = drive;
	fdtype[i].name = tmp;
	fdtype[i].head = head;
	fdtype[i].sect = sect;
	fdtype[i].cyl = cyl;

	return(0);
}
#endif	/* !MSDOS && !_NODOSDRIVE */

static int getuserfunc(line)
char *line;
{
	char *cp, *tmp[MAXFUNCLINES + 1];
	int i, j, len;

	cp = line;

	if (!(line = gettoken(&cp, " \t({"))) return(-1);
	if (*cp == '(') {
		if (cp = strtkchr(cp, ')', 0)) cp = skipspace(cp + 1);
		else {
			free(line);
			return(-1);
		}
	}

	for (i = 0; i < maxuserfunc; i++)
		if (!strcmp(userfunclist[i].func, line)) break;
	if (!*cp) {
		if (i < maxuserfunc) for (j = 0; userfunclist[i].comm[j]; j++)
			kanjiprintf("%s\r\n", userfunclist[i].comm[j]);
		free(line);
		return(1);
	}

	if (i < maxuserfunc) {
		for (j = 0; userfunclist[i].comm[j]; j++)
			free(userfunclist[i].comm[j]);
		free(userfunclist[i].comm);
		free(userfunclist[i].func);
	}
	else if (maxuserfunc < MAXFUNCTABLE) maxuserfunc++;
	else {
		free(line);
		return(-1);
	}

	userfunclist[i].func = line;
	if (*cp != '{') {
		userfunclist[i].comm = (char **)malloc2(sizeof(char *) * 2);
		userfunclist[i].comm[0] = geteostr(&cp);
		userfunclist[i].comm[1] = NULL;
		return(0);
	}

	cp = skipspace(cp + 1);
	if (line = strtkchr(cp, '}', 0)) *line = '\0';
	if (!*cp) {
		free(userfunclist[i].func);
		maxuserfunc--;
		for (; i < maxuserfunc; i++) {
			userfunclist[i].comm = userfunclist[i + 1].comm;
			userfunclist[i].func = userfunclist[i + 1].func;
		}
		return(0);
	}

	for (j = 0; j < MAXFUNCLINES && *cp; j++) {
		if (!(line = strtkchr(cp, ';', 0))) {
			tmp[j++] = strdup2(cp);
			break;
		}
		len = line - cp;
		tmp[j] = (char *)malloc2(len + 1);
		strncpy2(tmp[j], cp, len);
		cp = skipspace(line + 1);
	}
	userfunclist[i].comm = (char **)malloc2(sizeof(char *) * (j + 1));
	tmp[j] = NULL;
	for (; j >= 0; j--) userfunclist[i].comm[j] = tmp[j];

	return(0);
}

int evalconfigline(line)
char *line;
{
	char *cp, *tmp;
	int n;

	n = 0;
	cp = strtkbrk(line, " \t", 0);
	if (!strncmp(line, "export", 6) && cp == line + 6) {
		tmp = skipspace(cp + 1);
		if ((cp = getenvval(&tmp)) == (char *)-1) return(-1);
#ifdef	USESETENV
		if (!cp) unsetenv(tmp);
		else {
			if (setenv(tmp, cp, 1) < 0) error(cp);
			free(cp);
		}
#else
		strcpy(line, tmp);
		strcat(line, "=");
		if (cp) {
			strcat(line, cp);
			free(cp);
		}
		cp = strdup2(line);
		if (putenv2(cp)) error(cp);
#endif
		free(tmp);
	}
	else if (!strncmp(line, "alias", 5) && cp == line + 5)
		n = getalias(skipspace(cp + 1));
	else if (!strncmp(line, "unalias", 7) && cp == line + 7)
		unalias(skipspace(cp + 1));
	else if (!strncmp(line, "source", 6) && cp == line + 6) {
		tmp = skipspace(cp + 1);
		if (cp = strtkbrk(tmp, " \t", 0)) *cp = '\0';
		loadruncom(tmp);
	}
	else if (!strncmp(line, "function", 8) && cp == line + 8)
		n = getuserfunc(skipspace(cp + 1));
#if	!MSDOS && !defined (_NODOSDRIVE)
	else if (isalpha(*line) && (*(line + 1) == ':' || *(line + 1) == '!'))
		getdosdrive(line);
#endif
	else if (isalpha(*line) || *line == '_') {
		if ((cp = getenvval(&line)) == (char *)-1) return(-1);
		if (setenv2(line, cp, 1) < 0) error(line);
		if (cp) free(cp);
		free(line);
	}
#ifndef	_NOARCHIVE
	else if (*line == '"') getlaunch(line);
#endif
	else if (*line == '\'') getkeybind(line);
	else return(-1);
	return(n);
}

int printmacro()
{
	int i, n;

	n = 0;
	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (bindlist[i].f_func <= NO_OPERATION
		&& (bindlist[i].d_func <= NO_OPERATION
		|| bindlist[i].d_func == 255)) continue;
		if (bindlist[i].key < ' ')
			cprintf2("'^%c'\t", bindlist[i].key + '@');
		else if (bindlist[i].key >= K_F(1))
			cprintf2("'F%d'\t", bindlist[i].key - K_F0);
		else cprintf2("'%c'\t", bindlist[i].key);
		if (bindlist[i].f_func <= NO_OPERATION)
			cputs2(funclist[bindlist[i].f_func].ident);
		else kanjiprintf("\"%s\"",
			macrolist[bindlist[i].f_func - NO_OPERATION - 1]);
		if (bindlist[i].d_func <= NO_OPERATION)
			cprintf2("\t%s", funclist[bindlist[i].d_func].ident);
		else if (bindlist[i].d_func < 255) kanjiprintf("\t\"%s\"",
			macrolist[bindlist[i].d_func - NO_OPERATION - 1]);
		cputs2("\r\n");
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

static VOID printext(ext)
char *ext;
{
	char *cp;

	for (cp = ext + 1; *cp; cp++) {
		if (*cp == '\\') {
			if (!*(++cp)) break;
			putch2((int)(*(u_char *)cp));
		}
		else if (*cp == '.' && *(cp + 1) != '*') putch2('?');
		else if (!strchr(".^$", *cp)) putch2((int)(*(u_char *)cp));
	}
}

#ifndef	_NOARCHIVE
int printlaunch()
{
	int i, n;

	n = 0;
	for (i = 0; i < maxlaunch; i++) {
		putch2('"');
		printext(launchlist[i].ext);
		kanjiprintf("\"\t\"%s\"", launchlist[i].comm);
		if (launchlist[i].topskip < 255) cputs2(" (Arch)");
		cputs2("\r\n");
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

int printarch()
{
	int i, n;

	n = 0;
	for (i = 0; i < maxarchive; i++) {
		putch2('"');
		printext(archivelist[i].ext);
		kanjiprintf("\"\tA \"%s\"", archivelist[i].p_comm);
		kanjiprintf("\t\"%s\"\r\n", archivelist[i].u_comm);
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}
#endif

int printalias()
{
	int i, n;

	n = 0;
	for (i = 0; i < maxalias; i++) {
		kanjiprintf("%s\t\"%s\"\r\n",
			aliaslist[i].alias, aliaslist[i].comm);
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

int printuserfunc()
{
	int i, j, n;

	n = 0;
	for (i = 0; i < maxuserfunc; i++) {
		kanjiprintf("%s() {", userfunclist[i].func);
		for (j = 0; userfunclist[i].comm[j]; j++)
			cprintf2(" %s;", userfunclist[i].comm[j]);
		cprintf2(" }\r\n");
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

#if	!MSDOS && !defined (_NODOSDRIVE)
int printdrive()
{
	int i, n;

	n = 0;
	for (i = 0; fdtype[i].name; i++) {
		kanjiprintf("%c:\t\"%s\"\t(%1d,%3d,%3d)\r\n",
			fdtype[i].drive, fdtype[i].name,
			fdtype[i].head, fdtype[i].sect, fdtype[i].cyl);
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}
#endif !MSDOS && !_NODOSDRIVE

int printhist()
{
	int i, n;

	n = 0;
	for (i = histsize; i >= 1; i--) {
		if (!sh_history[i]) continue;
		kanjiprintf("%5d  %s\r\n", n + 1, sh_history[i]);
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

int evalbool(cp)
char *cp;
{
	if (!cp) return(-1);
	if (!*cp || !atoi2(cp)) return(0);
	return(1);
}

VOID evalenv()
{
	sorttype = atoi2(getenv2("FD_SORTTYPE"));
	if ((sorttype < 0 || sorttype > 12)
	&& (sorttype < 100 || sorttype > 112))
#if ((SORTTYPE < 0) || (SORTTYPE > 12)) \
&& ((SORTTYPE < 100) || (SORTTYPE > 112))
		sorttype = 0;
#else
		sorttype = SORTTYPE;
#endif
#ifndef	_NOTREE
	if ((sorttree = evalbool(getenv2("FD_SORTTREE"))) < 0)
		sorttree = SORTTREE;
#endif
#ifndef	_NOWRITEFS
	if ((writefs = atoi2(getenv2("FD_WRITEFS"))) < 0) writefs = WRITEFS;
#endif
	if ((histsize = atoi2(getenv2("FD_HISTSIZE"))) < 0) histsize = HISTSIZE;
	if ((savehist = atoi2(getenv2("FD_SAVEHIST"))) < 0) savehist = SAVEHIST;
	if ((minfilename = atoi2(getenv2("FD_MINFILENAME"))) <= 0)
		minfilename = MINFILENAME;
#ifndef	_NOTREE
	if ((dircountlimit = atoi2(getenv2("FD_DIRCOUNTLIMIT"))) < 0)
		dircountlimit = DIRCOUNTLIMIT;
#endif
	if ((showsecond = evalbool(getenv2("FD_SECOND"))) < 0)
		showsecond = SECOND;
#if	!MSDOS && !defined (_NODOSDRIVE)
	if ((dosdrive = evalbool(getenv2("FD_DOSDRIVE"))) < 0)
		dosdrive = DOSDRIVE;
#endif
#ifndef	_NOEDITMODE
	if (!(editmode = getenv2("FD_EDITMODE"))) editmode = EDITMODE;
#endif
	if (!(deftmpdir = getenv2("FD_TMPDIR"))) deftmpdir = TMPDIR;
	deftmpdir = evalpath(strdup2(deftmpdir));
#ifndef	_NOROCKRIDGE
	if (!(rockridgepath = getenv2("FD_RRPATH"))) rockridgepath = RRPATH;
	rockridgepath = evalcomstr(rockridgepath, ";");
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
	inputkcode = getlang(getenv2("FD_INPUTKCODE"), 1);
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
	outputkcode = getlang(getenv2("FD_LANGUAGE"), 0);
#endif
	if ((sizeinfo = evalbool(getenv2("FD_SIZEINFO"))) <= 0)
		sizeinfo = SIZEINFO;
#ifndef	_NOCOLOR
	if ((ansicolor = evalbool(getenv2("FD_ANSICOLOR"))) <= 0)
		ansicolor = ANSICOLOR;
#endif
}
