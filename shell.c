/*
 *	shell.c
 *
 *	Shell Command Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"

extern int filepos;
extern int mark;
extern char fullpath[];
extern char *findpattern;
extern char **sh_history;
extern char *archivefile;
extern char *archivedir;
extern int histsize;
extern int savehist;

static char *evalcomstr();
static char *killmeta();
static int setarg();
static int setargs();
static int insertarg();
static int dohistory();

int maxalias;
aliastable aliaslist[MAXALIASTABLE];


static char *evalcomstr(path)
char *path;
{
	char *cp, *next, *tmp, *epath;
	int len;

	epath = NULL;
	len = 0;
	for (cp = path; cp && *cp; cp = next) {
		if ((next = strchr(cp, '\''))
		|| (next = strpbrk(cp, CMDLINE_DELIM))) {
			tmp = _evalpath(cp, next);
			cp = next;
			if (*next != '\'') {
				while (*(++next)
				&& strchr(CMDLINE_DELIM, *next));
			}
			else if (next = strchr(next + 1, '\'')) next++;
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
			len + strlen(tmp) + next - cp + 1);
		strcpy(epath + len, tmp);
		len += strlen(tmp);
		free(tmp);
		strncpy(epath + len, cp, next - cp);
		len += next - cp;
		epath[len] = '\0';
	}

	return(epath);
}

static char *killmeta(name)
char *name;
{
	char *cp, *buf;
	int i;

	buf = (char *)malloc2(strlen(name) * 2 + 1);
	for (cp = name, i = 0; *cp; cp++, i++) {
		if (strchr(METACHAR, *cp)) buf[i++] = '\\';
		buf[i] = *cp;
	}
	buf[i] = '\0';
	return(buf);
}

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
		strcat(buf + ptr, "/");
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

	dir = (archivefile) ? killmeta(archivedir) : NULL;
	if (!mark) {
		len = setarg(buf, ptr, dir, list[filepos].name, noext);
		if (dir) free(dir);
		return(len);
	}

	for (i = len = 0; i < max; i++) if (ismark(&list[i])) {
		n = setarg(buf, ptr + len, dir, list[i].name, noext);
		if (!n) break;
		list[i].flags &= ~F_ISMRK;
		mark--;
		len += n;
		buf[ptr + len++] = ' ';
	}
	if (len > 0 && buf[ptr + len - 1] == ' ') len--;
	if (!n) len = -len;
	if (dir) free(dir);
	return(len);
}

static int insertarg(buf, format, arg, needmark)
char *buf, *format;
char *arg;
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
	macrostat st;
	char line[MAXCOMMSTR + 1];
	int i, j, len, noext, argset, uneval;

	if (stp) argset = (stp -> flags) & F_ARGSET;
	else {
		stp = &st;
		argset = 0;
	}
	stp -> addoption = -1;
	stp -> needmark = 0;
	noext = 0;
	uneval = '\0';

	for (i = j = 0; command[i] && j < sizeof(line); i++) {
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
				&& strchr("TM", toupper2(command[i + 1]))) {
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
				stp -> addoption = j;
				break;
			case 'K':
				stp -> flags |= F_NOCONFIRM;
				break;
			default:
				line[j++] = command[i];
				break;
		}
		else {
			line[j++] = command[i];
			if (command[i] == '\\' && command[i + 1])
				line[j++] = command[++i];
			else if (command[i] == uneval) uneval = 0;
			else if (command[i] == '"' || command[i] == '\'')
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
		for (i = 0; i < max; i++) list[i].flags &= ~F_ISMRK;
		mark = 0;
	}
	if (((stp -> flags) & F_REMAIN) && mark == 0) stp -> flags &= ~F_REMAIN;
	*(line + j) = '\0';
	return(evalcomstr(line));
}

int execmacro(command, arg, list, max, noconf, argset)
char *command, *arg;
namelist *list;
int max, noconf, argset;
{
	macrostat st;
	char *cp, *tmp, buf[MAXCOMMSTR + 1];
	int i, status;

	st.flags = (argset) ? F_ARGSET : 0;
	if (!(tmp = evalcommand(command, arg, list, max, &st))) return(-1);
	if (noconf >= 0 && (st.flags & F_NOCONFIRM)) noconf = 1 - noconf;
	i = (n_column - 4) * WCMDLINE;
	if (LCMDLINE + WCMDLINE - n_line >= 0) i -= n_column - n_lastcolumn;
	if (i > MAXLINESTR) i = MAXLINESTR;
	if (!st.needmark) for (;;) {
		if (st.addoption >= 0 && noconf >= 0 && !argset
		&& strlen(tmp) < i) {
			cp = inputstr2("sh#", st.addoption, tmp, &sh_history);
			free(tmp);
			if (!cp || !*cp) return(-1);
			tmp = evalcomstr(cp);
			free(cp);
		}
		status = system3(tmp, noconf);
		free(tmp);
		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand(command, arg, list, max, &st)))
			return(status);
	}

	if (mark <= 0) {
		if (insertarg(buf, tmp, list[filepos].name, st.needmark))
			status = system3(buf, noconf);
		else status = -1;
	}
	else for (i = 0; i < max; i++) if (ismark(&list[i])) {
		if (insertarg(buf, tmp, list[i].name, st.needmark))
			status = system3(buf, noconf);
		else status = -1;
	}
	for (i = 0; i < max; i++) list[i].flags &= ~F_ISMRK;
	mark = 0;
	free(tmp);
	return(status);
}

int execenv(env, arg)
char *env, *arg;
{
	char *command;

	if (!(command = getenv2(env))) return(0);
	putterms(t_clear);
	tflush();
	execmacro(command, arg, NULL, 0, 1, 0);
	return(1);
}

int execshell()
{
	char *sh;
	int status;

	if (!(sh = getenv2("FD_SHELL"))) sh = "/bin/sh";
	putterms(t_end);
	putterms(t_nokeypad);
	tflush();
	sigvecreset();
	cooked2();
	echo2();
	nl2();
	tabs();
	kanjiputs(SHEXT_K);
	status = system2(sh);
	raw2();
	noecho2();
	nonl2();
	notabs();
	sigvecset();
	putterms(t_keypad);
	putterms(t_init);

	return(status);
}

char **entryhist(hist, str)
char **hist, *str;
{
	int i;

	if (!hist) {
		hist = (char **)malloc2(sizeof(char *) * (histsize + 2));
		for (i = 0; i <= histsize; i++) hist[i + 1] = NULL;
		hist[0] = (char *)histsize;
	}
	else if (histsize > (int)hist[0]) {
		hist = (char **)realloc2(hist, sizeof(char *) * (histsize + 2));
		for (i = (int)hist[0]; i <= histsize; i++) hist[i + 1] = NULL;
		hist[0] = (char *)histsize;
	}

	if (!str || !*str) return(hist);
	else {
		if (hist[histsize]) free(hist[histsize]);
		for (i = histsize; i > 1; i--) hist[i] = hist[i - 1];
	}

	hist[1] = strdup2(str);
	return(hist);
}

char **loadhistory(file)
char *file;
{
	FILE *fp;
	u_char line[MAXLINESTR + 1];
	char *cp, **hist;
	int i, j, len;

	cp = evalpath(strdup2(file));
	fp = fopen(cp, "r");
	free(cp);
	if (!fp) return(NULL);

	hist = (char **)malloc2(sizeof(char *) * (histsize + 2));
	hist[0] = (char *)histsize;

	i = 1;
	while (fgets((char *)line, MAXLINESTR, fp)) {
		if (cp = strchr((char *)line, '\n')) *cp = '\0';
		for (j = i; j > 1; j--) hist[j] = hist[j - 1];
		for (j = len = 0; line[j]; j++, len++)
			if (line[j] < ' ' || line[j] == C_DEL) len++;
		hist[1] = (char *)malloc2(len + 1);
		for (j = len = 0; line[j]; j++, len++) {
			if (line[j] < ' ' || line[j] == C_DEL)
				hist[1][len++] = QUOTE;
			hist[1][len] = (char)(line[j]);
		}
		hist[1][len] = '\0';
		if (i < histsize) i++;
	}
	fclose(fp);
	if (i < histsize) for (; i <= histsize; i++) hist[i] = NULL;

	return(hist);
}

int savehistory(hist, file)
char **hist, *file;
{
	FILE *fp;
	char *cp, line[MAXLINESTR + 1];
	int i, j, len, size;

	if (!hist || !hist[1]) return(-1);
	cp = evalpath(strdup2(file));
	fp = fopen(cp, "w");
	free(cp);
	if (!fp) return(-1);

	size = (histsize < savehist) ? histsize : savehist;
	for (i = size; i >= 1; i--) if (hist[i] && *hist[i]) {
		for (j = 0, len = 0; hist[i][j]; j++)
			if (hist[i][j] != QUOTE) line[len++] = hist[i][j];
		line[len] = '\0';
		fputs(line, fp);
		fputc('\n', fp);
	}
	fclose(fp);

	return(0);
}

static int dohistory(command)
char *command;
{
	char *cp;
	int i, j, n;

	if (*command == '!') {
		cp = command + 1;
		i = 2;
	}
	else if (*command == '-') {
		for (cp = command + 1; *cp >= '0' && *cp <= '9'; cp++);
		i = atoi(command + 1);
		n = -(i++);
	}
	else {
		for (cp = command; *cp >= '0' && *cp <= '9'; cp++);
		n = atoi(command);
		for (i = histsize, j = 0; i >= 1; i--)
			if (sh_history[i] && ++j == n) break;
	}
	free(sh_history[1]);
	if (i <= 1 || !sh_history[i]) {
		for (i = 1; i < histsize && sh_history[i]; i++)
			sh_history[i] = sh_history[i + 1];
		if (i >= histsize) sh_history[histsize] = NULL;
		cprintf("%d: Event not found.\r\n", n);
		return(-1);
	}
	command = (char *)malloc2(strlen(sh_history[i]) + strlen(cp) + 1);
	strcpy(command, sh_history[i]);
	strcat(command, cp);
	sh_history[1] = strdup2(command);
	cprintf("%s\r\n", command);
	cooked2();
	echo2();
	nl2();
	tabs();
	system2(command);
	free(command);
	return(0);
}

int execinternal(command)
char *command;
{
	char *cp;
	int i;

	raw2();
	noecho2();
	nonl2();
	notabs();

	i = -1;
	locate(0, n_line - 1);
	putterm(l_clear);
	tflush();

	while (*command == ' ' || *command == '\t') command++;
	for (cp = command + strlen(command) - 1;
		cp >= command && (*cp == ' ' || *cp == '\t'); cp--);
	*(cp + 1) = '\0';
	if (!(cp = strpbrk(command, " \t"))) cp = command + strlen(command);
	if (!strcmp(command, "printenv")) i = printenv();
	else if (!strcmp(command, "printmacro")) i = printmacro();
	else if (!strcmp(command, "printlaunch")) i = printlaunch();
	else if (!strcmp(command, "printarch")) i = printarch();
	else if (!strcmp(command, "alias")) i = printalias();
	else if (!strcmp(command, "printdrive")) i = printdrive();
	else if (!strcmp(command, "history")) i = printhist();
	else if (!strncmp(command, "cd", cp - command)) {
		while (*cp == ' ' || *cp == '\t') cp++;
		chdir3(cp);
	}
	else if (*command == '!' || *command == '-'
	|| (*command >= '0' && *command <= '9')) i = dohistory(command);
	else if (evalconfigline(command) < 0) warning(0, NOCOM_K);
	else {
		adjustpath();
		evalenv();
	}
	return(0);
}

VOID adjustpath()
{
	char *cp, *path;

	if (!(cp = (char *)getenv("PATH"))) return;

	path = evalcomstr(cp);
	if (strcmp(path, cp)) {
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

char *evalalias(command)
char *command;
{
	char *cp;
	int i, len;

	for (cp = command; *cp && *cp != ' ' && *cp != '\t'; cp++);
	len = cp - command;
	for (i = 0; i < maxalias; i++)
		if (!strncmp(command, aliaslist[i].alias, len)
		&& !aliaslist[i].alias[len]) break;
	if (i >= maxalias) return(NULL);

	cp = (char *)malloc2(strlen(command) - len
		+ strlen(aliaslist[i].comm) + 1);
	strcpy(cp, aliaslist[i].comm);
	strcat(cp, command + len);
	return(cp);
}

int completealias(com, matchno, matchp)
char *com;
int matchno;
char **matchp;
{
	int i, len, ptr, size;

	if (strchr(com, '/')) return(0);

	size = lastpointer(*matchp, matchno) - *matchp;
	len = strlen(com);
	for (i = 0; i < maxalias; i++) {
		if (strncmp(com, aliaslist[i].alias, len)) continue;
		ptr = size;
		size += strlen(aliaslist[i].alias) + 1;
		*matchp = (char *)realloc2(*matchp, size);
		strcpy(*matchp + ptr, aliaslist[i].alias);
		matchno++;
	}
	return(matchno);
}
