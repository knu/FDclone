/*
 *	FD (File & Directory maintenance tool)
 *
 *	by T.Shirai <shirai@red.nintendo.co.jp>
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"
#include "funcno.h"
#include "version.h"

#include <signal.h>
#include <sys/time.h>

#ifdef	USETIMEH
#include <time.h>
#endif

#if defined (SIGARGINT) || defined (NOVOID)
#define	sigarg_t	int
#else
#define	sigarg_t	void
#endif

extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
extern char *macrolist[];
extern int maxmacro;
extern aliastable aliaslist[];
extern int maxalias;
extern bindtable bindlist[];
extern functable funclist[];
extern char *archivefile;
extern char **sh_history;
extern char *helpindex[];
extern int subwindow;
extern int columns;
extern int dispmode;

#define	CLOCKUPDATE	10	/* sec */

#ifndef	SIGWINCH
# if defined (SIGWINDOW)
# define	SIGWINCH	SIGWINDOW
# else
#  if defined (NSIG)
#  define	SIGWINCH	NSIG
#  else
#  define	SIGWINCH	28
#  endif
# endif
#endif

static VOID abandon();
static VOID signalexit();
static sigarg_t hungup();
static sigarg_t illerror();
static sigarg_t traperror();
#ifdef	SIGIOT
static sigarg_t ioerror();
#else
# ifdef	SIGABRT
static sigarg_t oldabort();
# endif
#endif
#ifdef	SIGEMT
static sigarg_t emuerror();
#endif
static sigarg_t floaterror();
static sigarg_t buserror();
static sigarg_t segerror();
#ifdef	SIGSYS
static sigarg_t syserror();
#endif
static sigarg_t terminate();
#ifdef	SIGXCPU
static sigarg_t xcpuerror();
#endif
#ifdef	SIGXFSZ
static sigarg_t xsizerror();
#endif
static sigarg_t wintr();
static sigarg_t printtime();
static char *skipspace();
static char *geteostr();
static char *getrange();
static char *getenvval();
static int getlaunch();
static int getcommand();
static int getkeybind();
static int getalias();
static int unalias();
static int loadruncom();
static VOID printext();
static int getoption();

int sorttype;
int sorttree;
int writefs;
int minfilename;
int histsize;
int savehist;
int dircountlimit;
char *deftmpdir;
char *language;

static char *progname;
static int timersec = 0;


static VOID abandon(no)
int no;
{
	char *cp, tmpfile[2 + sizeof(int) + 1];

	cp = evalpath(strdup2(deftmpdir));
	sprintf(tmpfile, "fd%d", getpid());
	forcecleandir(cp, tmpfile);
	free(cp);
	exit(no);
}

VOID error(str)
char *str;
{
	if (!str) str = progname;
	endterm();
	inittty(1);
	fputc('\007', stderr);
	perror(str);
	abandon(1);
}

static VOID signalexit(str)
char *str;
{
	endterm();
	inittty(1);
	fprintf(stderr, "\007%s\n", str);
	abandon(1);
}

static sigarg_t hangup()
{
	signal(SIGHUP, SIG_IGN);
	signalexit("Hangup");
}

static sigarg_t illerror()
{
	signal(SIGILL, SIG_IGN);
	signalexit("Illegal instruction");
}

static sigarg_t traperror()
{
	signal(SIGTRAP, SIG_IGN);
	signalexit("Trace/BPT trap");
}

#ifdef	SIGIOT
static sigarg_t ioerror()
{
	signal(SIGIOT, SIG_IGN);
	signalexit("IOT trap");
}
#else
# ifdef	SIGABRT
static sigarg_t oldabort()
{
	signal(SIGABRT, SIG_IGN);
	signalexit("Abort");
}
# endif
#endif

#ifdef	SIGEMT
static sigarg_t emuerror()
{
	signal(SIGEMT, SIG_IGN);
	signalexit("EMT trap");
}
#endif

static sigarg_t floaterror()
{
	signal(SIGFPE, SIG_IGN);
	signalexit("Floating point exception");
}

static sigarg_t buserror()
{
	signal(SIGBUS, SIG_IGN);
	signalexit("Bus error");
}

static sigarg_t segerror()
{
	signal(SIGSEGV, SIG_IGN);
	signalexit("Segmemtation fault");
}

#ifdef	SIGSYS
static sigarg_t syserror()
{
	signal(SIGSYS, SIG_IGN);
	signalexit("Bad system call");
}
#endif

static sigarg_t terminate()
{
	signal(SIGTERM, SIG_IGN);
	signalexit("Terminated");
}

#ifdef	SIGXCPU
static sigarg_t xcpuerror()
{
	signal(SIGXCPU, SIG_IGN);
	signalexit("Cputime limit exceeded");
}
#endif

#ifdef	SIGXFSZ
static sigarg_t xsizerror()
{
	signal(SIGXFSZ, SIG_IGN);
	signalexit("Filesize limit exceeded");
}
#endif

static sigarg_t wintr()
{
	signal(SIGWINCH, SIG_IGN);
	getwsize(80, WHEADER + WFOOTER + 2);
	title();
	if (archivefile) rewritearc();
	else rewritefile();
	if (subwindow) ungetch2(CTRL_L);
	signal(SIGWINCH, (sigarg_t (*)())wintr);
}

static sigarg_t printtime()
{
	struct timeval t;
	struct timezone tz;
	struct tm *tm;

	signal(SIGALRM, SIG_IGN);
	if (!timersec) {
		gettimeofday(&t, &tz);
		tm = localtime(&(t.tv_sec));

		locate(n_column - 16, LTITLE);
		putterm(t_standout);
		cprintf("%02d-%02d-%02d %02d:%02d",
			tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min);
		putterm(end_standout);
		locate(0, 0);
		tflush();
		timersec = CLOCKUPDATE;
	}
	timersec--;
	signal(SIGALRM, (sigarg_t (*)())printtime);
}

VOID sigvecset()
{
	getwsize(80, WHEADER + WFOOTER + 2);
	signal(SIGWINCH, (sigarg_t (*)())wintr);
	signal(SIGALRM, (sigarg_t (*)())printtime);
}

VOID sigvecreset()
{
	signal(SIGWINCH, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
}

VOID title()
{
	char *cp, *eol;

	locate(0, LTITLE);
	putterm(t_standout);
	cputs("  FD (File & Directory tool) Ver. ");
	cp = strchr(version, ' ');
	while (*(++cp) == ' ');
	if (!(eol = strchr(cp, ' '))) eol = cp + strlen(cp);
	cprintf("%-*.*s", eol - cp, eol - cp, cp);
	cprintf("%-*.*s", n_column - 34 - (eol - cp),
		n_column - 34 - (eol - cp), "  (c)1995 T.Shirai  ");
	putterm(end_standout);
	timersec = 0;
	printtime();
}

static char *skipspace(cp)
char *cp;
{
	while (*cp && (*cp == ' ' || *cp == '\t')) cp++;
	return(cp);
}

static char *geteostr(cp)
char **cp;
{
	char *tmp;
	int c;

	tmp = *cp;
	if ((c = **cp) == '"' || c == '\'') {
		for (*cp = ++tmp; *cp = strchr(*cp, c); (*cp)++)
			if (*(*cp - 1) != '\\') {
				*((*cp)++) = '\0';
				tmp = strdup2(tmp);
				if (c != '\'') tmp = evalpath(tmp);
				return(tmp);
			}
	}
	*cp = tmp + strlen(tmp);
	tmp = strdup2(tmp);
	if (c != '\'') tmp = evalpath(tmp);
	return(tmp);
}

static char *getrange(cp, fp, dp, wp)
char *cp;
u_char *fp, *dp, *wp;
{
	int i;

	*fp = *wp = 0;
	*dp = '\0';

	if (*cp >= '0' && *cp <= '9') *fp = ((i = atoi(cp)) > 0) ? i - 1 : 255;
	while (*cp >= '0' && *cp <= '9') cp++;

	if (*cp == '\'') {
		*dp = *(++cp);
		if (*cp && *(++cp) == '\'') cp++;
	}

	if (*cp == '-') {
		cp++;
		if (*cp >= '0' && *cp <= '9') *wp = atoi(cp) + 128;
		else if (*cp == '\'') *wp = (*(++cp)) % 128;
	}
	return(cp);
}

static char *getenvval(str)
char *str;
{
	char *cp;
	int ch;

	if (cp = strpbrk(str, "= \t")) {
		ch = *cp;
		*cp = '\0';
		if (ch == '=' || (cp = strchr(++cp, '='))) cp++;
	}
	if (cp) {
		cp = skipspace(cp);
		if (!*cp) return(NULL);
		cp = geteostr(&cp);
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
		strcpy(ext + 2, tmp);
		memcpy(ext, "^.*", 3);
	}
	free(tmp);

	cp = skipspace(cp);
	if (*cp == 'A') {
		for (n = 0; n < maxarchive; n++)
			if (!strcmp(archivelist[n].ext, ext)) break;
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

		if ((cp = skipspace(++cp)) >= eol) {
			maxarchive--;
			for (; n < maxarchive; n++)
				memcpy(&archivelist[n], &archivelist[n + 1],
					sizeof(archivetable));
			free(ext);
			return(0);
		}

		archivelist[n].ext = ext;
		archivelist[n].p_comm = geteostr(&cp);

		if (cp >= eol || (cp = skipspace(cp)) >= eol)
			archivelist[n].u_comm = NULL;
		else archivelist[n].u_comm = geteostr(&cp);
	}
	else {
		for (n = 0; n < maxlaunch; n++)
			if (!strcmp(launchlist[n].ext, ext)) break;
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
		}
	}
	return(0);
}

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
	char *cp, *tmp;
	int i;

	cp = line;

	if (!isalpha(*cp) && *cp != '_') return(-1);
	for (cp++; isalnum(*cp) || *cp == '_'; cp++);
	if (!strchr(" \t=", *cp)) return(-1);
	*cp = '\0';
	cp = skipspace(cp + 1);
	if (*cp == '=') cp = skipspace(cp + 1);

	tmp = geteostr(&cp);
	for (i = 0; i < maxalias; i++)
		if (!strcmp(aliaslist[i].alias, line)) break;
	if (!*tmp) {
		if (i < maxalias) kanjiprintf("%s\r\n", aliaslist[i].comm);
		free(tmp);
		return(0);
	}
	if (i == maxalias) {
		if (maxalias < MAXALIASTABLE) maxalias++;
		else {
			free(tmp);
			return(-1);
		}
	}
	aliaslist[i].alias = strdup2(line);
	aliaslist[i].comm = tmp;
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

int evalconfigline(line)
char *line;
{
	char *cp, *tmp;

	cp = strpbrk(line, " \t");
	if (!strncmp(line, "export", 6) && cp == line + 6) {
		tmp = skipspace(cp + 1);
		cp = getenvval(tmp);
#ifdef	USESETENV
		if (!cp) unsetenv(tmp);
		else {
			if (setenv(tmp, cp, 1) < 0) error(cp);
			free(cp);
		}
#else
		strcat(tmp, "=");
		if (cp) {
			strcat(tmp, cp);
			free(cp);
		}
		cp = strdup2(tmp);
		if (putenv2(cp)) error(cp);
#endif
	}
	else if (!strncmp(line, "alias", 5) && cp == line + 5)
		getalias(skipspace(cp + 1));
	else if (!strncmp(line, "unalias", 7) && cp == line + 7)
		unalias(skipspace(cp + 1));
	else if (!strncmp(line, "source", 6) && cp == line + 6) {
		tmp = skipspace(cp + 1);
		if (cp = strpbrk(tmp, " \t")) *cp = '\0';
		loadruncom(tmp);
	}
	else if (isalpha(*line) || *line == '_') {
		if (setenv2(line, cp = getenvval(line), 1) < 0) error(line);
		if (cp) free(cp);
	}
	else if (*line == '"') getlaunch(line);
	else if (*line == '\'') getkeybind(line);
	else return(-1);
	return(0);
}

static int loadruncom(file)
char *file;
{
	FILE *fp;
	char *cp, line[MAXLINESTR + 1];

	cp = evalpath(strdup2(file));
	fp = fopen(cp, "r");
	free(cp);
	if (!fp) return(-1);
	while (fgets(line, MAXLINESTR, fp)) {
		if (!(cp = strchr(line, '\n'))) cp = line + strlen(line);
		for (cp--; cp >= line && (*cp == ' ' || *cp == '\t'); cp--);
		*(cp + 1) = '\0';
		if (*line && *line != ';' && *line != '#')
			evalconfigline(skipspace(line));
	}
	fclose(fp);
	return(0);
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
			cprintf("'^%c'\t", bindlist[i].key + '@');
		else if (bindlist[i].key >= K_F(1))
			cprintf("'F%d'\t", bindlist[i].key - K_F0);
		else cprintf("'%c'\t", bindlist[i].key);
		if (bindlist[i].f_func <= NO_OPERATION)
			cputs(funclist[bindlist[i].f_func].ident);
		else kanjiprintf("\"%s\"",
			macrolist[bindlist[i].f_func - NO_OPERATION - 1]);
		if (bindlist[i].d_func <= NO_OPERATION)
			cprintf("\t%s", funclist[bindlist[i].d_func].ident);
		else if (bindlist[i].d_func < 255) kanjiprintf("\t\"%s\"",
			macrolist[bindlist[i].d_func - NO_OPERATION - 1]);
		cputs("\r\n");
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

static VOID printext(ext)
char *ext;
{
	char *cp;

	for (cp = ext; *cp; cp++) {
		if (*cp == '\\') {
			if (!*(++cp)) break;
			putch((int)(*(u_char *)cp));
		}
		else if (*cp == '.' && *(cp + 1) != '*') putch('?');
		else if (!strchr(".^$", *cp)) putch((int)(*(u_char *)cp));
	}
}

int printlaunch()
{
	int i, n;

	n = 0;
	for (i = 0; i < maxlaunch; i++) {
		putch('"');
		printext(launchlist[i].ext);
		kanjiprintf("\"\t\"%s\"", launchlist[i].comm);
		if (launchlist[i].topskip < 255) cputs(" (Arch)");
		cputs("\r\n");
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

int printarch()
{
	int i, n;

	n = 0;
	for (i = 0; i < maxarchive; i++) {
		putch('"');
		printext(archivelist[i].ext);
		kanjiprintf("\"\tA \"%s\"", archivelist[i].p_comm);
		kanjiprintf("\t\"%s\"\r\n", archivelist[i].u_comm);
		if (!(++n % (n_line - 1))) warning(0, HITKY_K);
	}
	return(n);
}

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
	if ((sorttree = atoi2(getenv2("FD_SORTTREE"))) < 0) sorttree = SORTTREE;
	if ((writefs = atoi2(getenv2("FD_WRITEFS"))) < 0) writefs = WRITEFS;
	if ((histsize = atoi2(getenv2("FD_HISTSIZE"))) < 0) histsize = HISTSIZE;
	if ((savehist = atoi2(getenv2("FD_SAVEHIST"))) < 0) savehist = SAVEHIST;
	if ((minfilename = atoi2(getenv2("FD_MINFILENAME"))) <= 0)
		minfilename = MINFILENAME;
	if ((dircountlimit = atoi2(getenv2("FD_DIRCOUNTLIMIT"))) < 0)
		dircountlimit = DIRCOUNTLIMIT;
	if (!(deftmpdir = getenv2("FD_TMPDIR"))) deftmpdir = TMPDIR;
	language = getenv2("FD_LANGUAGE");
}

static int getoption(argc, argv)
int argc;
char *argv[];
{
	char *cp, *env;
	int i, ch;

	for (i = 1; i < argc; i++) {
		env = argv[i] + 1;
		if (*argv[i] != '-' || !isupper(*env)) return(i);

		if (cp = strpbrk(env, "= \t")) {
			ch = *cp;
			*cp = '\0';
			if (ch == '=' || (cp = strchr(++cp, '='))
			|| (++i < argc && (cp = strchr(argv[i], '=')))) cp++;
			else i--;
		}

		if (cp) {
			while (*cp && (*cp == ' ' || *cp == '\t')) cp++;
			if (setenv2(env, cp, 1) < 0) error(env);
		} else setenv2(env, NULL, 1);
	}
	return(i);
}

int main(argc, argv)
int argc;
char *argv[];
{
	int i;

	if (progname = strrchr(argv[0], '/')) progname++;
	else progname = argv[0];

	inittty(0);
	raw2();
	noecho2();
	nonl2();
	notabs();
	getterment();
	initterm();
	sigvecset();

	signal(SIGHUP, (sigarg_t (*)())hangup);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGILL, (sigarg_t (*)())illerror);
	signal(SIGTRAP, (sigarg_t (*)())traperror);
#ifdef	SIGIOT
	signal(SIGIOT, (sigarg_t (*)())ioerror);
# else
# ifdef	SIGABRT
	signal(SIGABRT, (sigarg_t (*)())oldabort);
# endif
#endif
#ifdef	SIGEMT
	signal(SIGEMT, (sigarg_t (*)())emuerror);
#endif
	signal(SIGFPE, (sigarg_t (*)())floaterror);
	signal(SIGBUS, (sigarg_t (*)())buserror);
	signal(SIGSEGV, (sigarg_t (*)())segerror);
#ifdef	SIGSYS
	signal(SIGSYS, (sigarg_t (*)())syserror);
#endif
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, (sigarg_t (*)())terminate);
	signal(SIGTSTP, SIG_IGN);
#ifdef	SIGXCPU
	signal(SIGXCPU, (sigarg_t (*)())xcpuerror);
#endif
#ifdef	SIGXFSZ
	signal(SIGXFSZ, (sigarg_t (*)())xsizerror);
#endif

	for (maxlaunch = 0; maxlaunch < MAXLAUNCHTABLE; maxlaunch++)
		if (!launchlist[maxlaunch].ext) break;
		else {
			launchlist[maxlaunch].ext =
				strdup2(launchlist[maxlaunch].ext);
			launchlist[maxlaunch].comm =
				strdup2(launchlist[maxlaunch].comm);
		}
	for (maxarchive = 0; maxarchive < MAXARCHIVETABLE; maxarchive++)
		if (!archivelist[maxarchive].ext) break;
		else {
			archivelist[maxarchive].ext =
				strdup2(archivelist[maxarchive].ext);
			archivelist[maxarchive].p_comm =
				strdup2(archivelist[maxarchive].p_comm);
			archivelist[maxarchive].u_comm =
				strdup2(archivelist[maxarchive].u_comm);
		}
	for (i = 0; i < 10; i++) helpindex[i] = strdup2(helpindex[i]);
	maxalias = 0;

	loadruncom(DEFRUNCOM);
	loadruncom(RUNCOMFILE);
	i = getoption(argc, argv);
	adjustpath();
	evalenv();
	if ((dispmode = atoi2(getenv2("FD_DISPLAYMODE"))) < 0)
#if (DISPLAYMODE < 0) || (DISPLAYMODE > 7)
		dispmode = 0;
#else
		dispmode = DISPLAYMODE;
#endif
	columns = atoi2(getenv2("FD_COLUMNS"));
	if (columns < 1 || columns == 4 || columns > 5)
#if (COLUMNS < 1) || (COLUMNS == 4) || (COLUMNS > 5)
		columns = 2;
#else
		columns = COLUMNS;
#endif
#if (ADJTTY == 0)
	if (atoi2(getenv2("FD_ADJTTY")) > 0) {
#else
	{
#endif
		cooked2();
		echo2();
		nl2();
		tabs();
		inittty(0);
		raw2();
		noecho2();
		nonl2();
		notabs();
	}

	sh_history = loadhistory(HISTORYFILE);
	putterms(t_clear);
	title();

	main_fd(argv[i]);

	exit2(0);
}
