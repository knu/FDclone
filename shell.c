/*
 *	shell.c
 *
 *	Shell Command Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "funcno.h"
#include "kanji.h"

#if	MSDOS
# ifndef	NOLFNEMU
extern char *shortname __P_((char *, char *));
# endif
#else
#include <sys/param.h>
#endif

extern int filepos;
extern int mark;
extern long marksize;
extern char fullpath[];
extern char *findpattern;
#ifndef	_NOARCHIVE
extern char *archivefile;
extern char *archivedir;
#endif
extern functable funclist[];

static int setarg __P_((char *, int, char *, char *, int));
static int setargs __P_((char *, int, namelist *, int, int));
static int insertarg __P_((char *, char *, char *, int));
static char *addoption __P_((char *, char *, namelist *, int, macrostat *));
static int dosystem __P_((char *, namelist *, int *, int));
static char *evalargs __P_((char *, int, char *[]));
static char *histfgets __P_((char *, int, FILE *));
static char *evalalias __P_((char *));

aliastable aliaslist[MAXALIASTABLE];
int maxalias = 0;
userfunctable userfunclist[MAXFUNCTABLE];
int maxuserfunc = 0;
char **history[2] = {NULL, NULL};
short histsize[2] = {0, 0};
short histno[2] = {0, 0};
int savehist = 0;

static int n_args = 0;
static short histbufsize[2] = {0, 0};


static int setarg(buf, ptr, dir, arg, noext)
char *buf;
int ptr;
char *dir, *arg;
int noext;
{
	char *cp;
	int d, l;

	if (dir && *dir) d = strlen(dir) + 1;
	else d = 0;
	arg = killmeta(arg);
	if (noext && (cp = strrchr(arg, '.')) && cp != arg) l = cp - arg;
	else l = strlen(arg);
	if (ptr + d + l >= MAXCOMMSTR) {
		free(arg);
		return(0);
	}
	if (d) {
		strcpy(buf + ptr, dir);
		strcat(buf + ptr, _SS_);
	}
	strncpy(buf + ptr + d, arg, l);
	free(arg);
	return(d + l);
}

static int setargs(buf, ptr, list, max, noext)
char *buf;
int ptr;
namelist *list;
int max, noext;
{
	int i, n, len;
	char *dir;

#ifdef	_NOARCHIVE
	dir = NULL;
#else
	dir = (archivefile) ? killmeta(archivedir) : NULL;
#endif

	if (!n_args) {
		len = setarg(buf, ptr, dir, list[filepos].name, noext);
		if (dir) free(dir);
		return(len);
	}

	for (i = n = len = 0; i < max; i++) if (isarg(&list[i])) {
		n = setarg(buf, ptr + len, dir, list[i].name, noext);
		if (!n) break;
		list[i].flags &= ~F_ISARG;
		n_args--;
		len += n;
		buf[ptr + len++] = ' ';
	}
	if (len > 0 && buf[ptr + len - 1] == ' ') len--;
	if (!n) len = -len;
	if (dir) free(dir);
	return(len);
}

static int insertarg(buf, format, arg, needmark)
char *buf, *format, *arg;
int needmark;
{
	char *cp, *src, *body;
	int i, len, ptr;

	arg = killmeta(arg);
	if ((cp = strrchr(arg, '.')) && cp != arg) {
		i = cp - arg;
		body = (char *)malloc2(i + 1);
		strncpy2(body, arg, i);
	}
	else body = arg;

	ptr = strlen(format);
	if (ptr > MAXCOMMSTR) {
		if (body != arg) free(body);
		free(arg);
		return(0);
	}
	strcpy(buf, format);
	cp = format + ptr + 1;
	for (i = 0; i < needmark; i++) {
		if (*cp != '\n') src = arg;
		else {
			cp++;
			src = body;
		}
		len = strlen(src);
		if (ptr + len > MAXCOMMSTR) break;
		strcpy(buf + ptr, src);
		ptr += len;
		len = strlen(cp);
		if (ptr + len > MAXCOMMSTR) break;
		strcpy(buf + ptr, cp);
		ptr += len;
		cp += len + 1;
	}
	if (body != arg) free(body);
	free(arg);
	if (i < needmark) return(0);
	return(1);
}

char *evalcommand(command, arg, list, max, stp)
char *command, *arg;
namelist *list;
int max;
macrostat *stp;
{
#if	MSDOS && !defined (NOLFNEMU)
	char buf[MAXPATHLEN + 1];
#endif
	macrostat st;
	char *cp, line[MAXCOMMSTR + 1];
	int i, j, len, noext, argset, uneval;

	if (stp) argset = (stp -> flags) & F_ARGSET;
	else {
		stp = &st;
		argset = 0;
	}
	stp -> addopt = -1;
	stp -> needmark = 0;
	noext = 0;
	uneval = '\0';

	for (i = j = 0; command[i] && j < sizeof(line) / sizeof(char); i++) {
		if (!uneval && (noext || command[i] == '%'))
		switch (toupper2(command[++i])) {
			case '\0':
				i--;
				break;
			case 'P':
				len = setarg(line, j, NULL, fullpath, 0);
				if (!len) return(NULL);
				j += len;
				break;
			case 'C':
				len = setarg(line, j, NULL, arg, 0);
				if (!len) return(NULL);
				j += len;
				argset = 1;
				break;
			case 'X':
				if (command[i + 1]
#if	MSDOS && !defined (NOLFNEMU)
				&& strchr("TMS", toupper2(command[i + 1]))) {
#else
				&& strchr("TM", toupper2(command[i + 1]))) {
#endif
					noext = 1;
					i--;
					break;
				}
				len = setarg(line, j, NULL, arg, 1);
				if (!len) return(NULL);
				j += len;
				argset = 1;
				break;
			case 'T':
				if (toupper2(command[i + 1]) == 'A') i++;
				if (!list) break;
				len = setargs(line, j, list, max, noext);
				noext = 0;
				if (!len) return(NULL);
				if (len < 0) {
					len = -len;
					if (toupper2(command[i]) == 'A')
						stp -> flags |= F_REMAIN;
				}
				j += len;
				argset = 1;
				break;
			case 'M':
				if (!list) break;
				line[j++] = '\0';
				argset = 1;
				(stp -> needmark)++;
				if (noext) line[j++] = '\n';
				noext = 0;
				break;
			case 'N':
				argset = 1;
				break;
			case 'R':
				stp -> addopt = j;
				break;
			case 'K':
				stp -> flags |= F_NOCONFIRM;
				break;
#if	MSDOS && !defined (NOLFNEMU)
			case 'S':
				if (!(cp = shortname(arg, buf))) cp = arg;
				len = setarg(line, j, NULL, cp, noext);
				if (!len) return(NULL);
				j += len;
				argset = 1;
				noext = 0;
				break;
#endif
			default:
				line[j++] = command[i];
				noext = 0;
				break;
		}
		else {
			line[j++] = command[i];
#if	!MSDOS
			if (command[i] == '\\' && command[i + 1]
			&& uneval != '\'')
				line[j++] = command[++i];
			else
#endif
			if (command[i] == uneval) uneval = 0;
			else if (command[i] == '"' || command[i] == '\''
			|| command[i] == '`')
				uneval = command[i];
		}
	}
	if (command[i]) return(NULL);

	if (!argset) {
		line[j++] = ' ';
		arg = killmeta(arg);
		strcpy(line + j, arg);
		j += strlen(arg);
		free(arg);
	}
	if (list && !(stp -> needmark) && !((stp -> flags) & F_REMAIN)) {
		for (i = 0; i < max; i++) list[i].flags &= ~F_ISARG;
		n_args = 0;
	}
	if (((stp -> flags) & F_REMAIN) && !n_args) stp -> flags &= ~F_REMAIN;

	cp = (char *)malloc2(j + 1);
	memcpy(cp, line, j);
	cp[j] = '\0';
	return(cp);
}

static char *addoption(command, arg, list, max, stp)
char *command, *arg;
namelist *list;
int max;
macrostat *stp;
{
	char *cp, line[MAXLINESTR + 1];
	int i, j, n, p, len;

	while (stp -> addopt >= 0) {
		len = cmdlinelen(-1) + 1;
		n = stp -> needmark;
		p = -1;
		for (i = j = 0; i < len; i++, j++) {
			line[i] = command[j];
			if (j >= stp -> addopt && p < 0) p = i;
			if (command[j] == '%') {
				if (++i >= len) break;
				line[i] = '%';
			}
			else if (!command[j]) {
				if (n-- <= 0) break;
				line[i] = '%';
				if (command[j + 1] == '\n') {
					if (++i >= len) break;
					line[i] = 'X';
					j++;
				}
				if (++i >= len) break;
				line[i] = 'M';
			}
		}
		if (i >= len) return(command);

		free(command);
		if (p < 0) p = strlen(line);
		cp = inputstr("", 0, p, line, 0);
		if (!cp) return(NULL);
		if (!*cp) {
			free(cp);
			return(NULL);
		}
		command = evalcommand(cp, arg, list, max, stp);
		free(cp);
		if (!command) return((char *)-1);
		if (!*command) return(NULL);
	}
	return(command);
}

static int dosystem(command, list, maxp, noconf)
char *command;
namelist *list;
int *maxp, noconf;
{
	char *cp, *tmp;
	int n;

	while (*command == ' ' || *command == '\t') command++;
	sigvecreset();
	if ((cp = evalalias(command))) command = cp;

	if ((n = execbuiltin(command, list, maxp, 1)) < -1) {
		tmp = evalcomstr(command, CMDLINE_DELIM);
		system2(tmp, noconf);
		free(tmp);
	}
	sigvecset();
	if (cp) free(cp);
	return(n);
}

int execmacro(command, arg, list, maxp, noconf, argset)
char *command, *arg;
namelist *list;
int *maxp, noconf, argset;
{
	macrostat st;
	char *cp, *tmp, buf[MAXCOMMSTR + 1];
	int i, max, s, status;

	max = (maxp) ? *maxp : 0;
	status = -2;
	for (i = 0; i < max; i++) {
		if (ismark(&list[i])) list[i].flags |= F_ISARG;
		else list[i].flags &= ~F_ISARG;
	}
	n_args = mark;

	st.flags = (argset) ? F_ARGSET : 0;
	if (!(tmp = evalcommand(command, arg, list, max, &st))) return(-1);
	if (noconf >= 0 && (st.flags & F_NOCONFIRM)) noconf = 1 - noconf;
	st.flags |= F_ARGSET;
	if (noconf >= 0 && !argset) {
		cp = addoption(tmp, arg, list, max, &st);
		if (cp != tmp) status = 2;
		if (!cp || cp == (char *)-1) return((cp) ? -1 : 2);
		tmp = cp;
	}
	if (!st.needmark) for (;;) {
		s = dosystem(tmp, list, maxp, noconf);
		if (s >= status || s == -1) status = s;
		free(tmp);
		tmp = NULL;

		if (!argset) st.flags &= ~F_ARGSET;
		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand(command, arg, list, max, &st)))
			break;
		if (noconf >= 0 && !argset) {
			cp = addoption(tmp, arg, list, max, &st);
			if (cp != tmp && status >= 0 && status < 2) status = 2;
			if (!cp || cp == (char *)-1) return(status);
			tmp = cp;
		}
	}
	else if (n_args <= 0) {
		if (insertarg(buf, tmp, list[filepos].name, st.needmark))
			status = dosystem(buf, list, maxp, noconf);
		else status = -2;
	}
	else for (i = 0; i < max; i++) if (isarg(&list[i])) {
		if (insertarg(buf, tmp, list[i].name, st.needmark))
			status = dosystem(buf, list, maxp, noconf);
		else status = -2;
	}
	if (tmp) free(tmp);
	if (status >= -1) return(status);
	if (list) {
		for (i = 0; i < max; i++) list[i].flags &= ~(F_ISARG | F_ISMRK);
		mark = 0;
		marksize = 0;
	}
	return(4);
}

int execenv(env, arg)
char *env, *arg;
{
	char *command;

	if (!(command = getenv2(env))) return(0);
	putterms(t_clear);
	tflush();
	execmacro(command, arg, NULL, NULL, 1, 0);
	return(1);
}

int execshell(VOID_A)
{
	char *sh;
	int status;

#if	MSDOS
	if (!(sh = getenv2("FD_SHELL"))
	&& !(sh = getenv2("FD_COMSPEC"))) sh = "command.com";
#else
	if (!(sh = getenv2("FD_SHELL"))) sh = "/bin/sh";
#endif
	putterms(t_end);
	putterms(t_nokeypad);
	tflush();
	sigvecreset();
	cooked2();
	echo2();
	nl2();
	tabs();
	kanjiputs(SHEXT_K);
	tflush();
	status = system(sh);
	raw2();
	noecho2();
	nonl2();
	notabs();
	sigvecset();
	putterms(t_keypad);
	putterms(t_init);

	return(status);
}

static char *evalargs(command, argc, argv)
char *command;
int argc;
char *argv[];
{
	char *cp, line[MAXCOMMSTR + 1];
	int i, j, k, n;

	i = j = 0;
	while ((cp = strtkchr(&command[i], '$', 1))) {
		while (&command[i] < cp && j < MAXCOMMSTR)
			line[j++] = command[i++];
		if (command[++i] < '0' || command[i] > '9') {
			line[j++] = '$';
			line[j++] = command[i++];
		}
		else if ((n = command[i++] - '0') < argc)
			for (k = 0; argv[n][k]; k++) line[j++] = argv[n][k];
	}
	while (command[i]) line[j++] = command[i++];
	line[j] = '\0';

	return(strdup2(line));
}

int execusercomm(command, arg, list, maxp, noconf, argset)
char *command, *arg;
namelist *list;
int *maxp, noconf, argset;
{
	char *cp, *argv[MAXARGS + 2];
	int i, j, s, status, argc;

	argc = getargs(command, argv, MAXARGS + 1);
	for (i = 0; i < maxuserfunc; i++)
		if (!strpathcmp(argv[0], userfunclist[i].func)) break;
	if (i >= maxuserfunc)
		status = execmacro(command, arg, list, maxp, noconf, argset);
	else {
		status = -1;
		for (j = 0; userfunclist[i].comm[j]; j++) {
			cp = evalargs(userfunclist[i].comm[j], argc, argv);
			s = execmacro(cp, arg, list, maxp, noconf, argset);
			if (s >= status) status = s;
			free(cp);
		}
		if (status < 0) status = 4;
	}
	for (i = 0; i < argc; i++) free(argv[i]);
	return(status);
}

int entryhist(n, str, uniq)
int n;
char *str;
int uniq;
{
	int i, size;

	size = histsize[n];
	if (!history[n]) {
		history[n] = (char **)malloc2(sizeof(char *) * (size + 1));
		for (i = 0; i <= size; i++) history[n][i] = NULL;
		histbufsize[n] = size;
		histno[n] = 0;
	}
	else if (size > histbufsize[n]) {
		history[n] = (char **)realloc2(history[n],
			sizeof(char *) * (size + 1));
		for (i = histbufsize[n] + 1; i <= size; i++)
			history[n][i] = NULL;
		histbufsize[n] = size;
	}

	if (!str || !*str) return(0);

	if (histno[n]++ >= ~(short)(1L << (BITSPERBYTE * sizeof(short) - 1)))
		histno[n] = 0;

	if (uniq) {
		for (i = 0; i <= size; i++) {
			if (!history[n][i]) continue;
#if	MSDOS
			if (*str != *(history[n][i])) continue;
#endif
			if (!strpathcmp(str, history[n][i])) break;
		}
		if (i < size) size = i;
	}

	if (history[n][size]) free(history[n][size]);
	for (i = size; i > 0; i--) history[n][i] = history[n][i - 1];
	history[n][0] = strdup2(str);
	return(1);
}

static char *histfgets(s, n, fp)
char *s;
int n;
FILE *fp;
{
	int i, c;

	for (i = 0; i < n - 1; i++) {
		if ((c = fgetc(fp)) == EOF) {
			if (!i) return(NULL);
			break;
		}
		else if (c == '\n') break;
		else if (!c) c = '\n';
		s[i] = c;
	}
	s[i] = '\0';
	return(s);
}

int loadhistory(n, file)
int n;
char *file;
{
	FILE *fp;
	char *cp, line[MAXLINESTR + 1];
	int i, j, size;

	cp = evalpath(strdup2(file), 1);
	fp = fopen(cp, "r");
	free(cp);
	if (!fp) return(-1);

	size = histsize[n];
	history[n] = (char **)malloc2(sizeof(char *) * (size + 1));
	histbufsize[n] = size;
	histno[n] = 0;

	i = 0;
	while (histfgets(line, MAXLINESTR, fp)) {
		if (histno[n]++ >=
		~(short)(1L << (BITSPERBYTE * sizeof(short) - 1)))
			histno[n] = 0;
		for (j = i; j > 0; j--) history[n][j] = history[n][j - 1];
		history[n][0] = strdup2(line);
		if (i < size) i++;
	}
	fclose(fp);

	for (; i <= size; i++) history[n][i] = NULL;
	return(0);
}

int savehistory(n, file)
int n;
char *file;
{
	FILE *fp;
	char *cp, *nl;
	int i, size;

	if (!history[n] || !history[n][0]) return(-1);
	cp = evalpath(strdup2(file), 1);
	fp = fopen(cp, "w");
	free(cp);
	if (!fp) return(-1);

	size = (savehist > histsize[n]) ? histsize[n] : savehist;
	for (i = size - 1; i >= 0; i--) if (history[n][i] && *history[n][i]) {
		for (cp = history[n][i]; (nl = strchr(cp, '\n')); cp = nl) {
			fwrite(cp, sizeof(char), nl++ - cp, fp);
			fputc('\0', fp);
		}
		fputs(cp, fp);
		fputc('\n', fp);
	}
	fclose(fp);

	return(0);
}

int dohistory(argc, argv, list, maxp)
int argc;
char *argv[];
namelist *list;
int *maxp;
{
	char *cp, *tmp;
	int i, n, size;

	size = histsize[0];
	if (argv[0][1] == '!') {
		cp = &argv[0][2];
		n = 3;
	}
	else if (argv[0][1] == '-') {
		cp = skipnumeric(&argv[0][2], 0);
		n = atoi(&argv[0][2]);
	}
	else if (argv[0][1] >= '0' && argv[0][1] <= '9') {
		cp = skipnumeric(&argv[0][1], 0);
		n = histno[0] - atoi(&argv[0][1]);
	}
	else {
		i = strlen(&argv[0][1]);
		for (n = 1; n <= size; n++) {
			if (!history[0][n]) break;
			cp = skipspace(history[0][n]);
			if (!strnpathcmp(&argv[0][1], cp, i)) break;
		}
		cp = &argv[0][i + 1];
	}
	free(history[0][0]);
	if (*cp || n <= 0 || n > size || !history[0][n]) {
		for (i = 0; i < size && history[0][i]; i++)
			history[0][i] = history[0][i + 1];
		if (i >= size) history[0][size] = NULL;
		cprintf2("%s: Event not found.\r\n", &argv[0][1]);
		warning(0, HITKY_K);
		return(0);
	}
	tmp = catargs(argc - 1, &argv[1], ' ');
	if (!tmp) cp = strdup2(history[0][n]);
	else {
		cp = (char *)malloc2(strlen(history[0][n]) + strlen(tmp) + 2);
		strcpy(cp, history[0][n]);
		strcat(cp, " ");
		strcat(cp, tmp);
	}
	history[0][0] = cp;
	kanjiputs2(cp, -1, -1);
	cputs2("\r\n");
	n = dosystem(cp, list, maxp, 0);
	free(tmp);
	return(n);
}

static char *evalalias(command)
char *command;
{
	char *cp;
	int i, len;

	len = (cp = strpbrk(command, " \t")) ? cp - command : strlen(command);

	for (i = 0; i < maxalias; i++)
		if (!strnpathcmp(command, aliaslist[i].alias, len)
		&& !aliaslist[i].alias[len]) break;
	if (i >= maxalias) return(NULL);

	cp = (char *)malloc2((int)strlen(command) - len
		+ strlen(aliaslist[i].comm) + 1);
	strcpy(cp, aliaslist[i].comm);
	strcat(cp, command + len);
	return(cp);
}

#ifndef	_NOCOMPLETE
int completealias(com, matchno, matchp)
char *com;
int matchno;
char **matchp;
{
	int i, len, ptr, size;

	if (strchr(com, _SC_)) return(0);

	size = lastpointer(*matchp, matchno) - *matchp;
	len = strlen(com);
	for (i = 0; i < maxalias; i++) {
		if (strnpathcmp(com, aliaslist[i].alias, len)) continue;
		ptr = size;
		size += strlen(aliaslist[i].alias) + 1;
		*matchp = (char *)realloc2(*matchp, size);
		strcpy(*matchp + ptr, aliaslist[i].alias);
		matchno++;
	}
	return(matchno);
}

int completeuserfunc(com, matchno, matchp)
char *com;
int matchno;
char **matchp;
{
	int i, len, ptr, size;

	if (strchr(com, _SC_)) return(0);

	size = lastpointer(*matchp, matchno) - *matchp;
	len = strlen(com);
	for (i = 0; i < maxuserfunc; i++) {
		if (strnpathcmp(com, userfunclist[i].func, len)) continue;
		ptr = size;
		size += strlen(userfunclist[i].func) + 1;
		*matchp = (char *)realloc2(*matchp, size);
		strcpy(*matchp + ptr, userfunclist[i].func);
		matchno++;
	}
	return(matchno);
}
#endif	/* !_NOCOMPLETE */
