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
extern char *archivefile;
extern char *archivedir;

static char *evalcomstr();
static int setarg();
static int setargs();
static int insertarg();


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
	if (noext && (cp = strrchr(arg, '.')) && cp != arg) l = cp - arg;
	else l = strlen(arg);
	if (ptr + d + l >= MAXCOMMSTR) return(0);
	if (d) {
		strcpy(buf + ptr, dir);
		strcat(buf + ptr, "/");
	}
	strncpy(buf + ptr + d, arg, l);
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

	dir = (archivefile) ? archivedir : NULL;
	if (!mark) return(setarg(buf, ptr, dir, list[filepos].name, noext));

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
	return(len);
}

static int insertarg(buf, format, arg, needmark)
char *buf, *format;
char *arg;
int needmark;
{
	char *cp, *src, *body;
	int i, len, ptr, arglen, bodylen;

	if ((cp = strrchr(arg, '.')) && cp != arg) {
		i = cp - arg;
		body = (char *)malloc2(i + 1);
		strncpy2(body, arg, i);
	}
	else body = arg;

	ptr = strlen(format);
	if (ptr > MAXCOMMSTR) return(0);
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
	if (i < needmark) return(0);
	return(1);
}

char *evalcommand(command, arg, list, max, argset)
char *command, *arg;
namelist *list;
int max, argset;
{
	char *com, line[MAXCOMMSTR + 1];
	int i, j, len, flag, noext, needmark;

	flag = noext = needmark = 0;
	command = evalcomstr(command);
	for (i = j = 0; command[i] && j < sizeof(line); i++) {
		if (noext || command[i] == '%')
		switch (toupper2(command[++i])) {
			case '\0':
				i--;
				break;
			case 'P':
				len = setarg(line, j, NULL, fullpath, 0);
				if (!len) {
					free(command);
					return(NULL);
				}
				j += len;
				break;
			case 'C':
				len = setarg(line, j, NULL, arg, 0);
				if (!len) {
					free(command);
					return(NULL);
				}
				j += len;
				argset = 1;
				break;
			case 'X':
				if (strchr("TM", toupper2(command[i + 1]))) {
					noext = 1;
					i--;
					break;
				}
				len = setarg(line, j, NULL, arg, 1);
				if (!len) {
					free(command);
					return(NULL);
				}
				j += len;
				argset = 1;
				break;
			case 'T':
				if (toupper2(command[i + 1]) == 'A') i++;
				if (!list) break;
				len = setargs(line, j, list, max, noext);
				noext = 0;
				if (!len) {
					free(command);
					return(NULL);
				}
				if (len < 0) {
					len = -len;
					if (toupper2(command[i]) == 'A')
						flag |= F_REMAIN;
				}
				j += len;
				argset = 1;
				break;
			case 'M':
				if (!list) break;
				line[j++] = '\0';
				argset = 1;
				needmark++;
				if (noext) line[j++] = '\n';
				noext = 0;
				break;
			case 'R':
				flag |= F_ADDOPTION;
				break;
			case 'K':
				flag |= F_NOCONFIRM;
				break;
			default:
				line[j++] = command[i];
				break;
		}
		else if (command[i] == '\\') {
			line[j++] = command[i];
			if (command[i + 1]) line[j++] = command[++i];
		}
		else line[j++] = command[i];
	}
	if (command[i]) {
		free(command);
		return(NULL);
	}

	if (!argset) {
		line[j++] = ' ';
		strcpy(line + j, arg);
		j += strlen(arg);
	}
	if (list && !needmark && !(flag & F_REMAIN)) {
		for (i = 0; i < max; i++) list[i].flags &= ~F_ISMRK;
		mark = 0;
	}
	com = (char *)malloc2(j + 3);
	*com = flag;
	*(u_char *)(com + 1) = needmark;
	memcpy(com + 2, line, j);
	*(com + 2 + j) = '\0';
	free(command);
	return(com);
}

int execmacro(command, arg, list, max, noconf, argset)
char *command, *arg;
namelist *list;
int max, noconf, argset;
{
	char flag, *cp, *tmp, buf[MAXCOMMSTR + 1];
	int i, needmark, status;

	if (!(tmp = evalcommand(command, arg, list, max, argset))) return(127);
	flag = *tmp;
	needmark = *(u_char *)(tmp + 1);
	if (noconf >= 0 && (flag & F_NOCONFIRM)) noconf = 1 - noconf;
	i = (n_column - 4) * WCMDLINE;
	if (LCMDLINE + WCMDLINE - n_line >= 0) i -= n_column - n_lastcolumn;
	if (i > MAXLINESTR) i = MAXLINESTR;
	if (!needmark) for (;;) {
		cp = tmp + 2;
		if ((flag & F_ADDOPTION) && noconf >= 0 && !argset
		&& strlen(cp) < i) {
			cp = inputstr2("sh#", strlen(cp) + 1, cp, NULL);
			free(tmp);
			if (!cp || !*cp) return(127);
			tmp = cp;
			cp = evalcomstr(tmp);
			free(tmp);
			tmp = NULL;
		}
		status = system2(cp, noconf);
		if (tmp) free(tmp);
		else free(cp);
		if (!(flag & F_REMAIN)
		|| !(tmp = evalcommand(command, arg, list, max, argset)))
			return(status);
		flag = *tmp;
	}

	if (mark <= 0) {
		if (insertarg(buf, tmp + 2, list[filepos].name, needmark))
			status = system2(buf, noconf);
		else status = 127;
	}
	else for (i = 0; i < max; i++) if (ismark(&list[i])) {
		if (insertarg(buf, tmp + 2, list[i].name, needmark))
			status = system2(buf, noconf);
		else status = 127;
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

char **entryhist(hist, str)
char **hist, *str;
{
	int i, siz;

	if ((siz = atoi2(getenv2("FD_HISTSIZE"))) <= 0) siz = HISTSIZE;

	if (!hist) {
		hist = (char **)malloc2(sizeof(char *) * siz);
		for (i = 1; i < siz; i++) hist[i] = NULL;
	}
	else if (!str || !*str) return(hist);
	else {
		if (hist[siz - 1]) free(hist[i]);
		for (i = siz - 1; i > 0; i--) hist[i] = hist[i - 1];
	}

	hist[0] = strdup2(str);
	return(hist);
}

int execinternal(command)
char *command;
{
	int i;

	i = -1;
	locate(0, n_line - 1);
	putterm(l_clear);
	tflush();
	if (!strcmp(command, "printenv")) i = printenv();
	else if (!strcmp(command, "printmacro")) i = printmacro();
	else if (!strcmp(command, "printlaunch")) i = printlaunch();
	else if (!strcmp(command, "printarch")) i = printarch();
	else if (evalconfigline(command) < 0) warning(0, NOCOM_K);
	if (i >= 0) warning(0, HITKY_K);
	return(0);
}
