/*
 *	parse.c
 *
 *	Commandline Parser
 */

#include "fd.h"
#include "func.h"
#include "kctype.h"

#ifdef	USEUNAME
#include <sys/utsname.h>
#endif

#if	!MSDOS
# ifdef	_NODOSDRIVE
# include <sys/param.h>
# else
# include "dosdisk.h"
# endif
#endif

extern char fullpath[];
extern short histno[];
extern int sorttype;
extern int displaymode;
#ifndef	_NOTREE
extern int sorttree;
#endif
#ifndef	_NOWRITEFS
extern int writefs;
#endif
#if	!MSDOS
extern int adjtty;
#endif
extern int defcolumns;
extern int minfilename;
extern short histsize[];
extern int savehist;
#ifndef	_NOTREE
extern int dircountlimit;
#endif
#ifndef	_NODOSDRIVE
extern int dosdrive;
#endif
extern int showsecond;
extern int sizeinfo;
#ifndef	_NOCOLOR
extern int ansicolor;
#endif
#ifndef	_NOEDITMODE
extern char *editmode;
#endif
extern char *deftmpdir;
#ifndef	_NOROCKRIDGE
extern char *rockridgepath;
#endif
#ifndef	_NOPRECEDE
extern char *precedepath;
#endif

char *promptstr = NULL;


char *skipspace(cp)
char *cp;
{
	while (*cp == ' ' || *cp == '\t') cp++;
	return(cp);
}

char *skipnumeric(cp, plus)
char *cp;
int plus;
{
	if (plus < 0 && *cp == '-') cp++;
	else if (plus > 0) {
		if (*cp >= '1' && *cp <= '9') cp++;
		else return(cp);
	}
	while (*cp >= '0' && *cp <= '9') cp++;
	return(cp);
}

char *strtkbrk(s, c, evaldq)
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
#ifdef	CODEEUC
		else if (isekana(cp, 0)) {
			cp++;
			continue;
		}
#endif
		else if (iskanji1(cp, 0)) {
			cp++;
			continue;
		}
		else if (quote == '\'') continue;
		else if (ismeta(cp, 0, quote)) {
			cp++;
			if (*cp == META && strchr(c, *cp)) return(cp - 1);
			continue;
		}
		else if (quote == '`' || (quote == '"' && !evaldq)) continue;
		else if (*cp == '\'' || *cp == '"' || *cp == '`') {
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

char *geteostr(strp)
char **strp;
{
	char *cp, *tmp;
	int len;

	cp = *strp;
	if ((tmp = strtkbrk(*strp, " \t", 0))) len = tmp - *strp;
	else len = strlen(*strp);
	*strp += len;
	tmp = malloc2(len + 1);
	strncpy2(tmp, cp, len);
	return(tmp);
}

char *gettoken(strp, delim)
char **strp, *delim;
{
	char *cp, *tmp;
	int len;

	cp = *strp;
	if (!isalpha(**strp) && **strp != '_') return(NULL);
	for ((*strp)++; isalnum(**strp) || **strp == '_'; (*strp)++);
	if (**strp && delim && !strchr(delim, **strp)) return(NULL);

	len = *strp - cp;
	*strp = skipspace(*strp);
	tmp = (char *)malloc2(len + 1);
	strncpy2(tmp, cp, len);
	return(tmp);
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
		if (*cp != '_' && !isalpha(*cp)
		&& (cp == argv[i] || *cp < '0' || *cp > '9')) break;

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

char *evalcomstr(path, delim, ispath)
char *path, *delim;
int ispath;
{
	char *cp, *next, *tmp, *epath, buf[MAXPATHLEN];
	int len;

	epath = next = NULL;
	len = 0;
	for (cp = path; cp && *cp; cp = next) {
		if ((next = strtkbrk(cp, delim, 0))) {
			tmp = _evalpath(cp, next, 1, 0);
			cp = next;
			while (*(++next) && strchr(delim, *next));
		}
		else {
			next = cp + strlen(cp);
			tmp = _evalpath(cp, next, 1, 0);
			cp = next;
		}
		if (ispath) {
#if	MSDOS || !defined (_NODOSDRIVE)
			if (*(_dospath(tmp) ? tmp + 2 : tmp) == _SC_) {
#else
			if (*tmp == _SC_) {
#endif
				realpath2(tmp, buf, 1);
				free(tmp);
				tmp = buf;
			}
		}
		epath = (char *)realloc2(epath,
			len + strlen(tmp) + (next - cp) + 1);
		len = strcpy2(epath + len, tmp) - epath;
		if (tmp != buf) free(tmp);
		strncpy(epath + len, cp, next - cp);
		len += next - cp;
		epath[len] = '\0';
	}

	return((epath) ? epath : strdup2(""));
}

#if	!MSDOS
char *killmeta(name)
char *name;
{
#ifndef	_NOROCKRIDGE
	char tmp[MAXPATHLEN];
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
	name = detransfile(name, tmp, 0);
#endif
	*buf = (*name == '~') ? '"' : '\0';
	for (cp = name, i = 1; *cp; cp++, i++) {
#ifndef	CODEEUC
		if (sjis && issjis1(*cp) && issjis2(cp[1]))
			buf[i++] = *(cp++);
		else
#endif
		{
			if (isctl(*cp) || strchr(METACHAR, *cp)) *buf = '"';
			if (strchr(DQ_METACHAR, *cp)) buf[i++] = META;
		}
		buf[i] = *cp;
	}
	if (*(cp = buf)) buf[i++] = *cp;
	else cp++;
	buf[i] = '\0';
	return(strdup2(cp));
}

VOID adjustpath(VOID_A)
{
	char *cp, *path;

	if (!(cp = (char *)getenv("PATH"))) return;

	path = evalcomstr(cp, ":", 1);
	if (strpathcmp(path, cp)) {
		cp = (char *)malloc2(strlen(path) + 5 + 1);
		strcpy(strcpy2(cp, "PATH="), path);
		if (putenv2(cp) < 0) error("PATH");
	}
	free(path);
}
#endif	/* !MSDOS */

char *includepath(buf, path, plist)
char *buf, *path, *plist;
{
	char *cp, *eol, tmp[MAXPATHLEN];
	int len;

	if (!plist || !*plist) return(NULL);
	if (!buf) buf = tmp;
	realpath2(path, buf, 1);
	for (cp = plist; cp && *cp; ) {
		for (len = 0; cp[len]; len++) if (cp[len] == ';') break;
		eol = (cp[len]) ? &(cp[len + 1]) : NULL;
		while (len > 0 && cp[len - 1] == _SC_) len--;
#if	MSDOS
		if (onkanji1(cp, len - 1)) len++;
#endif
		if (len > 0 && !strnpathcmp(buf, cp, len)
		&& (!buf[len] || buf[len] == _SC_)) return(buf);
		cp = eol;
	}
	return(NULL);
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

VOID freeargs(argv)
char **argv;
{
	int i;

	for (i = 0; argv[i]; i++) free(argv[i]);
	free(argv);
}

char *catargs(argv, delim)
char *argv[];
int delim;
{
	char *cp;
	int i, len;

	if (!argv) return(NULL);
	for (i = len = 0; argv[i]; i++) len += strlen(argv[i]);
	if (i < 1) return(NULL);
	len += (delim) ? i - 1 : 0;
	cp = (char *)malloc2(len + 1);
	len = strcpy2(cp, argv[0]) - cp;
	for (i = 1; argv[i]; i++) {
		if (delim) cp[len++] = delim;
		len = strcpy2(cp + len, argv[i]) - cp;
	}
	return(cp);
}

#ifndef	_NOARCHIVE
char *getrange(cp, fp, dp, wp)
char *cp;
u_char *fp, *dp, *wp;
{
	int i;

	*fp = *dp = *wp = 0;

	if (*cp < '0' || *cp > '9') return(NULL);
	*fp = ((i = atoi(cp)) > 0) ? i - 1 : 255;
	cp = skipnumeric(cp, 0);

	if (*cp == '[') {
		if ((i = atoi(++cp)) >= 1) *dp = i - 1 + 128;
		cp = skipnumeric(cp, 0);
		if (*(cp++) != ']') return(NULL);
	}
	else if (*cp == '-') {
		if (cp[1] == ',' || cp[1] == ':') *dp = *(cp++);
		else if (cp[1] == '-' && cp[2] && cp[2] != ',' && cp[2] != ':')
			*dp = *(cp++);
	}
	else if (*cp && *cp != ',' && *cp != ':') *dp = *(cp++);

	if (*cp == '-') {
		cp++;
		if (*cp >= '0' && *cp <= '9') {
			*wp = atoi(cp) + 128;
			cp = skipnumeric(cp, 0);
		}
		else if (*cp && *cp != ',' && *cp != ':') *wp = *(cp++) % 128;
		else return(NULL);
	}
	return(cp);
}
#endif

int evalprompt(prompt, max)
char *prompt;
int max;
{
#ifdef	USEUNAME
	struct utsname uts;
#endif
	char *cp, *tmp, line[MAXPATHLEN];
	int i, j, k, len, unprint;

	unprint = 0;
	for (i = j = len = 0; promptstr[i]; i++) {
		cp = NULL;
		*line = '\0';
		if (promptstr[i] != '\\') {
			k = 0;
			line[k++] = promptstr[i];
#ifdef	CODEEUC
			if (isekana(promptstr, i)) line[k++] = promptstr[++i];
			else
#endif
			if (iskanji1(promptstr, i)) line[k++] = promptstr[++i];
			line[k] = '\0';
		}
		else switch (promptstr[++i]) {
			case '\0':
				i--;
				*line = '\\';
				line[1] = '\0';
				break;
			case '!':
				sprintf(line, "%d", histno[0] + 1);
				break;
#if	!MSDOS
			case 'u':
				cp = getpwuid2(getuid());
				break;
			case 'h':
			case 'H':
#ifdef	USEUNAME
				uname(&uts);
				strcpy(line, uts.nodename);
#else
				gethostname(line, MAXPATHLEN);
#endif
				if (promptstr[i] == 'h'
				&& (tmp = strchr(line, '.'))) *tmp = '\0';
				break;
			case '$':
				*line = (getuid()) ? '$' : '#';
				line[1] = '\0';
				break;
#endif
			case 'w':
				cp = fullpath;
				break;
			case 'W':
				tmp = fullpath;
#if	MSDOS || !defined (_NODOSDRIVE)
				if (_dospath(tmp)) tmp += 2;
#endif
				cp = strrdelim(tmp, 0);
				if (cp && (cp != tmp || *(cp + 1))) cp++;
				else cp = tmp;
				break;
			case '~':
				if (underhome(line + 1)) line[0] = '~';
				else cp = fullpath;
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
				if (promptstr[i] < '0' || promptstr[i] > '7') {
					*line = promptstr[i];
					line[1] = '\0';
				}
				else {
					*line = promptstr[i] - '0';
					for (k = 1; k < 3; k++) {
						if (promptstr[i + 1] < '0'
						|| promptstr[i + 1] > '7')
							break;
						*line = *line * 8
							+ promptstr[++i] - '0';
					}
					line[1] = '\0';
				}
				break;
		}
		if (!cp) cp = line;

		if (prompt) while (*cp && j < max) {
			if (unprint) prompt[j] = *cp;
#ifdef	CODEEUC
			else if (isekana(cp, 0)) {
				prompt[j++] = *(cp++);
				prompt[j] = *cp;
				len++;
			}
#endif
			else if (iskanji1(cp, 0)) {
				prompt[j++] = *(cp++);
				prompt[j] = *cp;
				len += 2;
			}
			else if (!isctl(*cp)) {
				prompt[j] = *cp;
				len++;
			}
			else if (j + 1 >= max) {
				prompt[j] = '?';
				len++;
			}
			else {
				prompt[j++] = '^';
				prompt[j] = ((*cp + '@') & 0x7f);
				len += 2;
			}
			cp++;
			j++;
		}
		else while (*cp && j < max) {
			if (unprint);
#ifdef	CODEEUC
			else if (isekana(cp, 0)) {
				j++;
				cp++;
				len++;
			}
#endif
			else if (iskanji1(cp, 0)) {
				j++;
				cp++;
				len += 2;
			}
			else if (!isctl(*cp) || j + 1 >= max) len++;
			else {
				j++;
				len += 2;
			}
			cp++;
			j++;
		}
	}
	if (prompt) prompt[j] = '\0';
	return(len);
}

int evalbool(cp)
char *cp;
{
	if (!cp) return(-1);
	if (!*cp || !atoi2(cp)) return(0);
	return(1);
}

VOID evalenv(VOID_A)
{
#ifndef	_NODOSDRIVE
	char *cp;
#endif

	sorttype = atoi2(getenv2("FD_SORTTYPE"));
	if ((sorttype < 0 || (sorttype & 7) > 5)
	&& (sorttype < 100 || ((sorttype - 100) & 7) > 5))
		sorttype = SORTTYPE;
	displaymode = atoi2(getenv2("FD_DISPLAYMODE"));
#ifdef	HAVEFLAGS
	if (displaymode < 0 || displaymode > 15)
#else
	if (displaymode < 0 || displaymode > 7)
#endif
		displaymode = DISPLAYMODE;
#ifndef	_NOTREE
	if ((sorttree = evalbool(getenv2("FD_SORTTREE"))) < 0)
		sorttree = SORTTREE;
#endif
#ifndef	_NOWRITEFS
	if ((writefs = atoi2(getenv2("FD_WRITEFS"))) < 0) writefs = WRITEFS;
#endif
#if	!MSDOS
	if ((adjtty = evalbool(getenv2("FD_ADJTTY"))) < 0) adjtty = ADJTTY;
#endif
	defcolumns = atoi2(getenv2("FD_COLUMNS"));
	if (defcolumns < 0 || defcolumns > 5 || defcolumns == 4)
		defcolumns = COLUMNS;
	if ((minfilename = atoi2(getenv2("FD_MINFILENAME"))) <= 0)
		minfilename = MINFILENAME;
	if ((histsize[0] = atoi2(getenv2("FD_HISTSIZE"))) < 0)
		histsize[0] = HISTSIZE;
	if ((histsize[1] = atoi2(getenv2("FD_DIRHIST"))) < 0)
		histsize[1] = DIRHIST;
	if ((savehist = atoi2(getenv2("FD_SAVEHIST"))) < 0)
		savehist = SAVEHIST;
#ifndef	_NOTREE
	if ((dircountlimit = atoi2(getenv2("FD_DIRCOUNTLIMIT"))) < 0)
		dircountlimit = DIRCOUNTLIMIT;
#endif
#ifndef	_NODOSDRIVE
	if ((dosdrive = evalbool(cp = getenv2("FD_DOSDRIVE"))) < 0)
		dosdrive = DOSDRIVE;
# if	MSDOS
	if (cp && (cp = strchr(cp, ',')) && !strcmp(++cp, "BIOS"))
		dosdrive |= 2;
# endif
#endif
	if ((showsecond = evalbool(getenv2("FD_SECOND"))) < 0)
		showsecond = SECOND;
	if ((sizeinfo = evalbool(getenv2("FD_SIZEINFO"))) < 0)
		sizeinfo = SIZEINFO;
#ifndef	_NOCOLOR
	if ((ansicolor = atoi2(getenv2("FD_ANSICOLOR"))) < 0)
		ansicolor = ANSICOLOR;
#endif
#ifndef	_NOEDITMODE
	if (!(editmode = getenv2("FD_EDITMODE"))) editmode = EDITMODE;
#endif
	if (deftmpdir) free(deftmpdir);
	if (!(deftmpdir = getenv2("FD_TMPDIR"))) deftmpdir = TMPDIR;
	deftmpdir = evalpath(strdup2(deftmpdir), 1);
#ifndef	_NOROCKRIDGE
	if (rockridgepath) free(rockridgepath);
	if (!(rockridgepath = getenv2("FD_RRPATH"))) rockridgepath = RRPATH;
	rockridgepath = evalcomstr(rockridgepath, ";", 1);
#endif
#ifndef	_NOPRECEDE
	if (precedepath) free(precedepath);
	if (!(precedepath = getenv2("FD_PRECEDEPATH")))
		precedepath = PRECEDEPATH;
	precedepath = evalcomstr(precedepath, ";", 1);
#endif
	if (!(promptstr = getenv2("FD_PROMPT"))) promptstr = PROMPT;
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
	outputkcode = getlang(getenv2("FD_LANGUAGE"), 0);
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
	inputkcode = getlang(getenv2("FD_INPUTKCODE"), 1);
#endif
}
