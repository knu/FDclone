/*
 *	expfunc.c
 *
 *	Function Expander for the obsolete /bin/sh
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "machine.h"

#define	MAXLINEBUF	255
#define	MAXFUNCNO	32
#define	MAXARGS		10

extern VOID exit __P_((int));

static char *isfunction __P_((char *));
static int entryfunc __P_((char *));
static char *checkhere __P_((char *));
static int getargs __P_((char **, char *[]));
static char *expargs __P_((char *, char *[]));
static int evalline __P_((char *, char *[]));
static char *getline __P_((FILE *));

static int funcno = 0;
static char *func[MAXFUNCNO];
static char *funcbody[MAXFUNCNO];


static char *isfunction(line)
char *line;
{
	char *cp, *top;
	int len;

	if (funcno >= MAXFUNCNO) return(NULL);
	cp = line;
	while (*cp == ' ' || *cp == '\t') cp++;
	top = cp;
	while (*cp == '_' || isalpha(*cp)) cp++;
	if ((len = cp - top) <= 0) return(NULL);
	while (*cp == ' ' || *cp == '\t') cp++;
	if (*(cp++) != '(') return(NULL);
	while (*cp == ' ' || *cp == '\t') cp++;
	if (*(cp++) != ')') return(NULL);
	while (*cp == ' ' || *cp == '\t') cp++;
	if (*(cp++) != '{') return(NULL);
	if (!(func[funcno] = (char *)malloc(len + 1))) exit(1);
	strncpy(func[funcno], top, len);
	func[funcno][len] = '\0';
	funcbody[funcno] = NULL;
	return(cp);
}

static int entryfunc(line)
char *line;
{
	char *cp;
	int i, c, len;

	i = 1;
	c = '\0';
	if (*line == '}') {
		i = 0;
		line = "";
	}
	else for (cp = line; *cp; cp++) {
		if (*cp == '\\' && cp[1]) cp++;
		else if (*cp == c) c = '\0';
		else if ((*cp == '\'' || *cp == '"') && !c) c = *cp;
		else if (*cp == '}' && !c) {
			*cp = '\0';
			i = 0;
		}
	}
	len = strlen(line);
	if (!i) len += 3;
	if (!funcbody[funcno]) {
		funcbody[funcno] = (char *)malloc(len + 2 + 1);
		if (!funcbody[funcno]) exit(1);
		strcpy(funcbody[funcno], "{ ");
	}
	else {
		len += strlen(funcbody[funcno]);
		funcbody[funcno] = (char *)realloc(funcbody[funcno], len + 2);
		if (!funcbody[funcno]) exit(1);
		if (*line) strcat(funcbody[funcno], "\n");
	}
	strcat(funcbody[funcno], line);
	if (!i) strcat(funcbody[funcno++], "; }");
	return(i);
}

static char *checkhere(line)
char *line;
{
	char *cp;
	int c;

	c = '\0';
	for (cp = line; *cp; cp++) {
		if (*cp == '\\' && cp[1]) (cp)++;
		else if (*cp == c) c = '\0';
		else if ((*cp == '\'' || *cp == '"') && !c)
			c = *cp;
		else if (!c && *cp == '<' && cp[1] == '<') break;
	}
	if (*cp != '<') return(NULL);
	cp += 2;
	while (*cp == ' ' || *cp == '\t') cp++;
	line = cp;
	while (*cp && *cp != ' ' && *cp != '\t') cp++;
	c = cp - line;
	if (!(cp = (char *)malloc(c + 1))) exit(1);
	strncpy(cp, line, c);
	cp[c] = '\0';
	return(cp);
}

static int getargs(linep, args)
char **linep;
char *args[];
{
	char *cp, *new;
	int i, c;

	for (i = 0; i < MAXARGS; i++) args[i] = NULL;
	for (i = 0; i < MAXARGS; i++) {
		new = NULL;
		while (**linep == ' ' || **linep == '\t') (*linep)++;
		c = '\0';
		for (cp = *linep; **linep;) {
			if (**linep == '\\') {
				if (*(*linep + 1) == '\n') break;
				else if (*(*linep + 1)) (*linep)++;
				else {
					new = getline(stdin);
					if (cp > *linep) break;
					cp = *linep = new;
					new = NULL;
					continue;
				}
			}
			else if (**linep == c) c = '\0';
			else if ((**linep == '\'' || **linep == '"') && !c)
				c = **linep;
			else if (!c && strchr(" \t\n!&);<>`|}", **linep))
				break;
			(*linep)++;
		}
		c = *linep - cp;
		if (*cp == '"') {
			cp++;
			c--;
			if (*(*linep - 1) == '"') c--;
		}
		if (c > 0) {
			if (!(args[i] = (char *)malloc(c + 1))) exit(1);
			strncpy(args[i], cp, c);
			args[i][c] = '\0';
		}
		if (new) *linep = new;
		else if (**linep == '\\' && *(*linep + 1) == '\n')
			(*linep) += 2;
		else if (**linep != ' ' && **linep != '\t') return(i + 1);
	}
	return(i);
}

static char *expargs(line, args)
char *line;
char *args[];
{
	char *buf;
	int i, j, k, c, cp, tmp, len;

	len = strlen(line);
	if (!(buf = (char *)malloc(len + 1))) exit(1);
	strcpy(buf, line);

	c = '\0';
	for (cp = 0; buf[cp]; cp++) {
		if (buf[cp] == '\\' && buf[cp + 1]) cp++;
		else if (buf[cp] == c) c = '\0';
		else if ((buf[cp] == '\'' || buf[cp] == '"') && !c)
			c = buf[cp];
		else if (buf[cp] == '$' && c != '\'') {
			tmp = cp;
			if (buf[++cp] == '{') cp++;
			if (buf[cp] >= '1' && buf[cp] <= '9') {
				i = buf[cp] - '1';
				if (buf[++cp] == '}') cp++;
				len -= cp - tmp;
				if (!args[i]) {
					for (j = cp; buf[j]; j++)
						buf[tmp + j - cp] = buf[j];
					buf[tmp + j - cp] = '\0';
				}
				else {
					len += strlen(args[i]);
					buf = (char *)realloc(buf, len + 1);
					if (!buf) exit(1);
					k = len - (int)strlen(buf);
					cp += k;
					if (k > 0) for (j = len; j >= cp; j--)
						buf[j] = buf[j - k];
					else for (j = cp; j <= len; j++)
						buf[j] = buf[j - k];
					memcpy(&(buf[tmp]), args[i],
						strlen(args[i]));
				}
				cp--;
			}
		}
	}
	return(buf);
}

static int evalline(line, args)
char *line;
char *args[];
{
	char *cp, *newargs[MAXARGS];
	int i, len;

	if (args) line = expargs(line, args);
	for (cp = line; *cp; cp++) {
		for (i = 0; i < funcno; i++) {
			len = strlen(func[i]);
			if (!strncmp(cp, func[i], len)) break;
		}
		if (i >= funcno) fputc(*cp, stdout);
		else {
			cp += len;
			getargs(&cp, newargs);
			evalline(funcbody[i], newargs);
			for (i = 0; i < MAXARGS; i++)
				if (newargs[i]) free(newargs[i]);
			cp--;
		}
	}
	i = strlen(line);
	if (args) free(line);
	else fputc('\n', stdout);

	return(i);
}

static char *getline(fp)
FILE *fp;
{
	static char buf[MAXLINEBUF + 1];
	char *cp;

	for (;;) {
		*buf = '\0';
		if (!fgets(buf, MAXLINEBUF, fp)) return(NULL);
		if (*(cp = buf + (int)strlen(buf) - 1) == '\n') *(cp--) = '\0';
		for (cp = buf; *cp == ' ' || *cp == '\t'; cp++);
		if (*cp == '#') fprintf(stdout, "%s\n", buf);
		else break;
	}
	return(buf);
}

/*ARGSUSED*/
int main(argc, argv)
int argc;
char *argv[];
{
	char *cp, *heredoc, *line;
	int infunc;

	infunc = 0;
	heredoc = NULL;
	func[0] = "return";
	funcbody[0] = "(exit $1)";
	funcno = 1;

	while (line = getline(stdin)) {
		if (heredoc) {
			fprintf(stdout, "%s\n", line);
			if (!strcmp(line, heredoc)) {
				free(heredoc);
				heredoc = NULL;
			}
		}
		else {
			heredoc = checkhere(line);
			if (infunc) infunc = entryfunc(line);
			else if (cp = isfunction(line))
				infunc = (*cp) ? entryfunc(cp) : 1;
			else evalline(line, NULL);
		}
	}

	return(0);
}
