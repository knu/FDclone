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
#ifndef	_NOTREE
extern int sorttree;
#endif
#ifndef	_NOWRITEFS
extern int writefs;
#endif
extern short histsize[];
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
extern char *promptstr;


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

char *geteostr(strp, eval)
char **strp;
int eval;
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
	if (eval && c != '\'') tmp = evalpath(tmp);
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

char *getenvval(strp)
char **strp;
{
	char *cp;

	cp = *strp;
	if (!(*strp = gettoken(&cp, " \t="))) return((char *)-1);
	if (*cp != '=') {
		free(*strp);
		*strp = NULL;
		return((char *)-1);
	}
	cp = skipspace(cp + 1);
	if (!*cp) return(NULL);
	cp = geteostr(&cp, 1);
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
			tmp = _evalpath(cp, next, 1);
			cp = next;
			if (*next != '\'')
				while (*(++next) && strchr(delim, *next));
			else if (next = strtkchr(next + 1, '\'', 0)) next++;
			else next = cp + strlen(cp);
		}
		else {
			next = cp + strlen(cp);
			tmp = _evalpath(cp, next, 1);
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
	name = detransfile(name, tmp, 0);
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

VOID adjustpath(VOID)
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
		argv[i] = geteostr(&cp, 1);
		cp = skipspace(cp);
	}
	argv[i] = NULL;

	return(i);
}

char *catargs(argc, argv, delim)
int argc;
char *argv[];
int delim;
{
	char *cp;
	int i, len;

	if (argc < 1) return(NULL);
	len = (delim) ? argc - 1 : 0;
	for (i = 0; i < argc; i++) len += strlen(argv[i]);
	cp = (char *)malloc2(len + 1);
	strcpy(cp, argv[0]);
	len = strlen(argv[0]);
	for (i = 1; i < argc; i++) {
		if (delim) cp[len++] = delim;
		strcpy(cp + len, argv[i]);
		len += strlen(argv[i]);
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

	if (*cp == '\'') {
		*dp = *(++cp);
		if (!*(cp++) || *(cp++) != '\'') return(NULL);
	}
	else if (*cp == '[') {
		if ((i = atoi(++cp)) >= 1) *dp = i - 1 + 128;
		cp = skipnumeric(cp, 0);
		if (*(cp++) != ']') return(NULL);
	}

	if (*cp == '-') {
		cp++;
		if (*cp >= '0' && *cp <= '9') {
			*wp = atoi(cp) + 128;
			cp = skipnumeric(cp, 0);
		}
		else if (*cp == '\'') {
			*wp = (*(++cp)) % 128;
			if (!*(cp++) || *(cp++) != '\'') return(NULL);
		}
	}
	return(cp);
}
#endif

char *evalprompt(VOID)
{
#ifdef	USEUNAME
	struct utsname uts;
#endif
	static char *prompt = NULL;
	char *cp, line[MAXLINESTR + 1];
	int i, j;

	if (!prompt) free(prompt);
	for (i = j = 0; promptstr[i]; i++) {
		if (promptstr[i] != '\\') {
			line[j++] = promptstr[i];
			continue;
		}
		switch (promptstr[++i]) {
			case '\0':
				i--;
				line[j++] = '\\';
				break;
			case '!':
				sprintf(&line[j], "%d", histno[0] + 1);
				j += strlen(&line[j]);
				break;
#if	!MSDOS
			case 'u':
				cp = getpwuid2(getuid());
				while (*cp) line[j++] = *(cp++);
				break;
			case 'h':
			case 'H':
#ifdef	USEUNAME
				uname(&uts);
				strcpy(&line[j], uts.nodename);
#else
				gethostname(&line[j], MAXLINESTR - j);
#endif
				if (promptstr[i] == 'h'
				&& (cp = strchr(&line[j], '.'))) *cp = '\0';
				j += strlen(&line[j]);
				break;
			case '$':
				line[j++] = (getuid()) ? '$' : '#';
				break;
#endif
			case 'w':
				for (cp = fullpath; *cp; cp++) line[j++] = *cp;
				break;
			case 'W':
				cp = strrchr(fullpath + 1, _SC_);
				if (cp) cp++;
				else cp = fullpath;
				while (*cp) line[j++] = *(cp++);
				break;
			case '~':
				if (underhome(&line[j + 1])) line[j++] = '~';
				else strcpy(&line[j], fullpath);
				j += strlen(&line[j]);
				break;
			default:
				line[j++] = promptstr[i];
				break;
		}
	}
	line[j] = '\0';
	prompt = (char *)strdup2(line);
	return(prompt);
}

int evalbool(cp)
char *cp;
{
	if (!cp) return(-1);
	if (!*cp || !atoi2(cp)) return(0);
	return(1);
}

VOID evalenv(VOID)
{
	sorttype = atoi2(getenv2("FD_SORTTYPE"));
	if ((sorttype < 0 || (sorttype & 7) > 5)
	&& (sorttype < 100 || ((sorttype - 100) & 7) > 5))
#if	((SORTTYPE < 0) || ((SORTTYPE & 7) > 5)) \
&& ((SORTTYPE < 100) || (((SORTTYPE - 100) & 7) > 5))
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
	if ((histsize[0] = atoi2(getenv2("FD_HISTSIZE"))) < 0)
		histsize[0] = HISTSIZE;
	if ((histsize[1] = atoi2(getenv2("FD_DIRHIST"))) < 0)
		histsize[1] = DIRHIST;
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
	if ((ansicolor = atoi2(getenv2("FD_ANSICOLOR"))) < 0)
		ansicolor = ANSICOLOR;
#endif
	if (!(promptstr = getenv2("FD_PROMPT"))) promptstr = PROMPT;
}
