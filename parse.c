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
#ifndef	_NODOSDRIVE
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
	int q;

	q = '\0';
	for (cp = s; *cp; cp++) {
		if (*cp == '\\' && q != '\'') {
			if (!*(++cp)) return(NULL);
			else if (*cp == '\\' && (strchr(c, '\\')))
				return(cp - 1);
			continue;
		}
		if (q) {
			if (*cp == q) q = '\0';
			if (q == '\'' || q == '`' || !evaldq) continue;
		}
		else if (*cp == '\'' || *cp == '"' || *cp == '`') q = *cp;
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
	tmp = (char *)malloc2(len + 1);
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
	return(_evalpath(cp, NULL, 1, 1));
}

char *evalcomstr(path, delim)
char *path, *delim;
{
	char *cp, *next, *tmp, *epath;
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
	char *cp, buf[MAXPATHLEN * 2 + 2 + 1];
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
	*buf = '\0';
	for (cp = name, i = 1; *cp; cp++, i++) {
#ifndef	CODEEUC
		if (sjis && iskanji1(*cp)) buf[i++] = *(cp++);
		else
#endif
		{
			if ((u_char)(*cp) < ' ' || *cp == C_DEL
			|| strchr(METACHAR, *cp)) *buf = '"';
			if (strchr(DQ_METACHAR, *cp))
				buf[i++] = '\\';
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

	path = evalcomstr(cp, ":");
	if (strpathcmp(path, cp)) {
		cp = (char *)malloc2(strlen(path) + 5 + 1);
		strcpy(cp, "PATH=");
		strcpy(cp + 5, path);
		if (putenv2(cp) < 0) error("PATH");
	}
	free(path);
}
#endif	/* !MSDOS */

char *includepath(path, plist)
char *path, *plist;
{
	char *cp, *eol, buf[MAXPATHLEN + 1];
	int len;

	if (!plist || !*plist) return(NULL);
	if (!path) path = buf;
	realpath2(fullpath, path);
	for (cp = plist; cp && *cp; ) {
		for (len = 0; cp[len]; len++) if (cp[len] == ';') break;
		eol = (cp[len]) ? &(cp[len + 1]) : NULL;
		while (len > 0 && cp[len - 1] == _SC_) len--;
#if	MSDOS
		if (onkanji1(cp, len - 1)) len++;
#endif
		if (len > 0 && !strnpathcmp(path, cp, len)
		&& (!path[len] || path[len] == _SC_)) return(path);
		cp = eol;
	}
	return(NULL);
}

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

int evalprompt(prompt, max)
char *prompt;
int max;
{
#ifdef	USEUNAME
	struct utsname uts;
#endif
	char *cp, line[MAXPATHLEN + 1];
	int i, j, k, len, unprint;

	unprint = 0;
	for (i = j = len = 0; promptstr[i]; i++) {
		cp = NULL;
		*line = '\0';
		if (promptstr[i] != '\\') {
			*line = promptstr[i];
			line[1] = '\0';
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
				&& (cp = strchr(line, '.'))) *cp = '\0';
				cp = line;
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
				cp = strrdelim(fullpath + 1, 0);
				if (cp) cp++;
				else cp = fullpath;
				break;
			case '~':
				if (underhome(line)) {
					if (j < max) {
						if (prompt) prompt[j] = '~';
						j++;
					}
					if (!unprint) len++;
				}
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
			else if ((u_char)(*cp) >= ' ' && *cp != C_DEL) {
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
			else if (((u_char)(*cp) >= ' ' && *cp != C_DEL)
			|| j + 1 >= max) len++;
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
#ifndef	_NODOSDRIVE
	if ((dosdrive = evalbool(cp = getenv2("FD_DOSDRIVE"))) < 0)
		dosdrive = DOSDRIVE;
# if	MSDOS
	if (cp && (cp = strchr(cp, ',')) && !strcmp(++cp, "BIOS"))
		dosdrive |= 2;
# endif
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
	rockridgepath = evalcomstr(rockridgepath, ";");
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
	inputkcode = getlang(getenv2("FD_INPUTKCODE"), 1);
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
	outputkcode = getlang(getenv2("FD_LANGUAGE"), 0);
#endif
	if ((sizeinfo = evalbool(getenv2("FD_SIZEINFO"))) < 0)
		sizeinfo = SIZEINFO;
#ifndef	_NOCOLOR
	if ((ansicolor = atoi2(getenv2("FD_ANSICOLOR"))) < 0)
		ansicolor = ANSICOLOR;
#endif
#ifndef	_NOPRECEDE
	if (precedepath) free(precedepath);
	if (!(precedepath = getenv2("FD_PRECEDEPATH")))
		precedepath = PRECEDEPATH;
	precedepath = evalcomstr(precedepath, ";");
#endif
	if (!(promptstr = getenv2("FD_PROMPT"))) promptstr = PROMPT;
}
