/*
 *	shell.c
 *
 *	Shell Command Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"

#if	MSDOS
extern char *shortname __P_((char *, char *));
#else
#include <sys/param.h>
#endif

extern int filepos;
extern int mark;
extern off_t marksize;
extern char fullpath[];
extern functable funclist[];
#ifndef	_NOARCHIVE
extern char *archivefile;
extern char *archivedir;
#endif

static int setarg __P_((char *, int, char *, char *, u_char));
static int setargs __P_((char *, int, namelist *, int, u_char));
static int insertarg __P_((char *, char *, char *, int));
static char *addoption __P_((char *, char *, namelist *, int, macrostat *));
static int dosystem __P_((char *, namelist *, int *, int));
static char *evalargs __P_((char *, int, char *[]));
static char *evalalias __P_((char *));

aliastable aliaslist[MAXALIASTABLE];
int maxalias = 0;
userfunctable userfunclist[MAXFUNCTABLE];
int maxuserfunc = 0;
char **history[2] = {NULL, NULL};
short histsize[2] = {0, 0};
short histno[2] = {0, 0};
int savehist = 0;
int n_args = 0;

static short histbufsize[2] = {0, 0};


static int setarg(buf, ptr, dir, arg, flags)
char *buf;
int ptr;
char *dir, *arg;
u_char flags;
{
	char *cp;
	int d, l;

	d = l = 0;
	if (dir && *dir) d = strlen(dir) + 1;
#if	!MSDOS
	arg = killmeta(arg);
#endif
	l = strlen(arg);
	if (ptr + d + l >= MAXCOMMSTR - 1) d = l = 0;
	if (d) {
#if	MSDOS && !defined (_NOARCHIVE)
		if ((flags & F_ISARCH) && l) {
			if (_dospath(dir)) {
				buf[ptr] = dir[0];
				buf[ptr + 1] = dir[1];
				d = 2;
			}
			else d = 0;
			if (dir[d] == _SC_ && !dir[d + 1])
				buf[ptr + (d++)] = '/';
			else {
				cp = NULL;
				for (; dir[d]; d++) {
					if (dir[d] == _SC_) {
						buf[ptr + d] = '/';
						if (!cp) cp = &(buf[ptr + d]);
						continue;
					}
					buf[ptr + d] = dir[d];
					cp = NULL;
					if (iskanji1(dir, d)) {
						if (!dir[++d]) break;
						buf[ptr + d] = dir[d];
					}
				}
				if (!cp) buf[ptr + (d++)] = '/';
			}
		}
		else
#endif
		d = strcatdelim2(buf + ptr, dir, NULL) - (buf + ptr);
	}
#if	MSDOS && !defined (_NOUSELFN)
	if ((flags & F_TOSFN) && l) {
		char tmp[MAXPATHLEN];

		strcpy(buf + ptr + d, arg);
		if (shortname(buf + ptr, tmp)) {
			arg = tmp;
			d = 0;
			l = strlen(tmp);
			if (ptr + l >= MAXCOMMSTR - 1) l = 0;
		}
	}
#endif
	if ((flags & F_NOEXT) && (cp = strrchr(arg, '.')) && cp != arg)
		l = cp - arg;
	strncpy(buf + ptr + d, arg, l);
#if	!MSDOS
	free(arg);
#endif
	return(d + l);
}

static int setargs(buf, ptr, list, max, flags)
char *buf;
int ptr;
namelist *list;
int max;
u_char flags;
{
	int i, n, len;
	char *dir;

#ifdef	_NOARCHIVE
	dir = NULL;
#else
# if	MSDOS
	dir = (archivefile) ? archivedir : NULL;
# else
	dir = (archivefile) ? killmeta(archivedir) : NULL;
# endif
#endif

	if (!n_args) len = setarg(buf, ptr, dir, list[filepos].name, flags);
	else {
		for (i = n = len = 0; i < max; i++) if (isarg(&(list[i]))) {
			n = setarg(buf, ptr + len, dir, list[i].name, flags);
			if (!n) break;
			list[i].tmpflags &= ~F_ISARG;
			n_args--;
			len += n;
			buf[ptr + len++] = ' ';
		}
		if (len > 0 && buf[ptr + len - 1] == ' ') len--;
		if (!n) len = -len;
	}
#if	!MSDOS
	if (dir) free(dir);
#endif
	return(len);
}

static int insertarg(buf, format, arg, needmark)
char *buf, *format, *arg;
int needmark;
{
#if	MSDOS && !defined (_NOUSELFN)
	char tmp[MAXPATHLEN];
#endif
	char *cp, *src, *ins;
	int i, len, ptr;

#if	!MSDOS
	arg = killmeta(arg);
#endif

#if	MSDOS && !defined (_NOUSELFN)
	*tmp = '\0';
#endif
	ptr = strlen(format);
	if (ptr > MAXCOMMSTR) i = 0;
	else {
		strcpy(buf, format);
		src = format + ptr + 1;
		for (i = 0; i < needmark; i++) {
			ins = arg;
#if	MSDOS && !defined (_NOUSELFN)
			if (*src & F_TOSFN) {
				if (!*tmp && !shortname(arg, tmp))
					strcpy(tmp, arg);
				ins = tmp;
			}
#endif
			if ((*src & F_NOEXT) && (cp = strrchr(ins, '.')))
				len = cp - ins;
			else len = strlen(ins);
			if (ptr + len > MAXCOMMSTR) break;
			strncpy(buf + ptr, ins, len);
			ptr += len;
			len = strlen(++src);
			if (ptr + len > MAXCOMMSTR) break;
			strcpy(buf + ptr, src);
			ptr += len;
			src += len + 1;
		}
	}
#if	!MSDOS
	free(arg);
#endif
	if (i < needmark) return(0);
	return(1);
}

char *evalcommand(command, arg, list, max, stp)
char *command, *arg;
namelist *list;
int max;
macrostat *stp;
{
#if	!MSDOS && !defined (_NOKANJICONV)
	char tmpkanji[MAXCOMMSTR + 1];
	int cnvcode = NOCNV;
	int tmpcode;
	int cnvptr = -1;
#endif
	macrostat st;
	char *cp, line[MAXCOMMSTR + 1];
	int i, j, c, len, uneval;
	u_char flags;

	if (stp) flags = stp -> flags;
	else {
		stp = &st;
		flags = 0;
	}
	stp -> addopt = -1;
	stp -> needmark = 0;
	uneval = '\0';

	for (i = j = 0; command[i] && j < MAXCOMMSTR; i++) {
		if (!uneval
		&& ((flags & (F_NOEXT | F_TOSFN)) || command[i] == '%'))
		switch ((c = toupper2(command[++i]))) {
			case '\0':
				i--;
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
			case 'P':
				len = setarg(line, j, NULL, fullpath, flags);
				if (!len) return(NULL);
				j += len;
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
#if	MSDOS && !defined (_NOUSELFN)
			case 'S':
				flags |= F_TOSFN;
				c = toupper2(command[i + 1]);
				if (c == 'T' || c == 'M') {
					i--;
					break;
				}
#endif
			case 'C':
				len = setarg(line, j, NULL, arg, flags);
				if (!len) return(NULL);
				j += len;
				flags |= F_ARGSET;
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
			case 'X':
				flags |= F_NOEXT;
				c = toupper2(command[i + 1]);
#if	MSDOS && !defined (_NOUSELFN)
				if (c == 'T' || c == 'M' || c == 'S') {
#else
				if (c == 'T' || c == 'M') {
#endif
					i--;
					break;
				}
				len = setarg(line, j, NULL, arg, flags);
				if (!len) return(NULL);
				j += len;
				flags |= F_ARGSET;
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
			case 'T':
				if ((c = toupper2(command[i + 1])) == 'A') i++;
				if (list) {
					len = setargs(line, j,
						list, max, flags);
					if (!len) return(NULL);
					if (len < 0) {
						len = -len;
						if (c == 'A')
							flags |= F_REMAIN;
					}
					j += len;
					flags |= F_ARGSET;
				}
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
			case 'M':
				if (list) {
					line[j++] = '\0';
					(stp -> needmark)++;
					line[j++] = flags;
					flags |= F_ARGSET;
				}
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
			case 'N':
				flags |= F_ARGSET;
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
			case 'R':
				stp -> addopt = j;
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
			case 'K':
				flags |= F_NOCONFIRM;
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
#if	!MSDOS && !defined (_NOKANJICONV)
			case 'J':
				c = toupper2(command[i + 1]);
				flags &= ~(F_NOEXT | F_TOSFN);
				if (c == 'S') tmpcode = SJIS;
				else if (c == 'E' || c == 'U') tmpcode = EUC;
				else if (c == 'J') tmpcode = JIS7;
				else {
					line[j++] = command[i];
					break;
				}

				i++;
				if (cnvcode && cnvptr >= 0) {
					memcpy(tmpkanji, line + cnvptr,
						j - cnvptr);
					tmpkanji[j - cnvptr] = '\0';
					j = kanjiconv(line + cnvptr, tmpkanji,
						DEFCODE, cnvcode) + cnvptr;
					if (cnvcode == tmpcode) {
						cnvcode = NOCNV;
						cnvptr = -1;
						break;
					}
				}
				cnvcode = tmpcode;
				cnvptr = j;
				break;
#endif
			default:
				line[j++] = command[i];
				flags &= ~(F_NOEXT | F_TOSFN);
				break;
		}
		else {
			line[j++] = command[i];
#if	!MSDOS
			if (command[i] == META && command[i + 1]
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

#if	!MSDOS && !defined (_NOKANJICONV)
	if (cnvcode && cnvptr >= 0) {
		memcpy(tmpkanji, line + cnvptr, j - cnvptr);
		tmpkanji[j - cnvptr] = '\0';
		j = kanjiconv(line + cnvptr, tmpkanji, DEFCODE, cnvcode)
			+ cnvptr;
	}
#endif

	if (!(flags & F_ARGSET)) {
		line[j++] = ' ';
#if	!MSDOS
		arg = killmeta(arg);
#endif
		j = strcpy2(line + j, arg) - line;
#if	!MSDOS
		free(arg);
#endif
	}
	if (list && !(stp -> needmark) && !(flags & F_REMAIN)) {
		for (i = 0; i < max; i++) list[i].tmpflags &= ~F_ISARG;
		n_args = 0;
	}
	if ((flags & F_REMAIN) && !n_args) flags &= ~F_REMAIN;
	stp -> flags = flags;

	cp = malloc2(j + 1);
	strncpy2(cp, line, j);
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
				j++;
				if (command[j] & F_NOEXT) {
					if (++i >= len) break;
					line[i] = 'X';
				}
#if	MSDOS && !defined (_NOUSELFN)
				if (command[j] & F_TOSFN) {
					if (++i >= len) break;
					line[i] = 'S';
				}
#endif
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
		tmp = evalcomstr(command, CMDLINE_DELIM, 0);
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
	char *cp, *tmp, **argv, buf[MAXCOMMSTR + 1];
	int i, max, r, ret;

	max = (maxp) ? *maxp : 0;
	ret = -2;
	for (i = 0; i < max; i++) {
		if (ismark(&(list[i]))) list[i].tmpflags |= F_ISARG;
		else list[i].tmpflags &= ~F_ISARG;
	}
	n_args = mark;

	if (argset) st.flags = F_ARGSET;
	else {
		st.flags = 0;
		if (getargs(command, &argv) > 0
		&& isinternal(argv[0], 1) >= 0) st.flags = F_ARGSET;
		freeargs(argv);
	}
	if (noconf < 0) st.flags |= F_ISARCH;

	if (!(tmp = evalcommand(command, arg, list, max, &st))) return(-2);
	if (noconf >= 0 && (st.flags & F_NOCONFIRM)) noconf = 1 - noconf;
	st.flags |= F_ARGSET;
	if (noconf >= 0 && !argset) {
		cp = addoption(tmp, arg, list, max, &st);
		if (cp != tmp) ret = 2;
		if (!cp || cp == (char *)-1) return((cp) ? -2 : 2);
		tmp = cp;
	}
	if (!st.needmark) for (;;) {
		r = dosystem(tmp, list, maxp, noconf);
		free(tmp);
		tmp = NULL;
		if (!argset) st.flags &= ~F_ARGSET;

		if (r == -1) {
			ret = r;
			break;
		}
		if (r >= ret) ret = r;

		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand(command, arg, list, max, &st)))
			break;
		if (noconf >= 0 && !argset) {
			cp = addoption(tmp, arg, list, max, &st);
			if (cp != tmp && ret >= 0 && ret < 2) ret = 2;
			if (!cp || cp == (char *)-1) return(ret);
			tmp = cp;
		}
	}
	else if (n_args <= 0) {
		if (insertarg(buf, tmp, list[filepos].name, st.needmark))
			ret = dosystem(buf, list, maxp, noconf);
		else ret = -2;
	}
	else for (i = 0; i < max; i++) if (isarg(&(list[i]))) {
		if (insertarg(buf, tmp, list[i].name, st.needmark))
			ret = dosystem(buf, list, maxp, noconf);
		else ret = -2;
	}
	if (tmp) free(tmp);
	if (ret >= -1) return(ret);
	if (list) {
		for (i = 0; i < max; i++)
			list[i].tmpflags &= ~(F_ISARG | F_ISMRK);
		mark = 0;
		marksize = 0;
	}
	return(4);
}

static char *evalargs(command, argc, argv)
char *command;
int argc;
char *argv[];
{
	char *cp, line[MAXCOMMSTR + 1];
	int i, j, k, n;

	i = j = 0;
	while ((cp = strtkchr(&(command[i]), '$', 1))) {
		while (&(command[i]) < cp && j < MAXCOMMSTR)
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
	char *cp, **argv;
	int i, j, r, ret, argc;

	if (!(argc = getargs(command, &argv))) i = maxuserfunc;
	else for (i = 0; i < maxuserfunc; i++)
		if (!strpathcmp(argv[0], userfunclist[i].func)) break;
	if (i >= maxuserfunc)
		ret = execmacro(command, arg, list, maxp, noconf, argset);
	else {
		ret = -2;
		for (j = 0; userfunclist[i].comm[j]; j++) {
			cp = evalargs(userfunclist[i].comm[j], argc, argv);
			r = execmacro(cp, arg, list, maxp, noconf, argset);
			free(cp);
			if (r == -1) {
				ret = r;
				break;
			}
			if (r >= ret) ret = r;
		}
		if (ret < -1) ret = 4;
	}
	freeargs(argv);
	return(ret);
}

int entryhist(n, s, uniq)
int n;
char *s;
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

	if (!s || !*s) return(0);

	if (histno[n]++ >= ~(short)(1L << (BITSPERBYTE * sizeof(short) - 1)))
		histno[n] = 0;

	if (uniq) {
		for (i = 0; i <= size; i++) {
			if (!history[n][i]) continue;
#if	MSDOS
			if (*s != *(history[n][i])) continue;
#endif
			if (!strpathcmp(s, history[n][i])) break;
		}
		if (i < size) size = i;
	}

	if (history[n][size]) free(history[n][size]);
	for (i = size; i > 0; i--) history[n][i] = history[n][i - 1];
	history[n][0] = strdup2(s);
	return(1);
}

int loadhistory(n, file)
int n;
char *file;
{
	FILE *fp;
	char *cp, *line;
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
	while ((line = fgets2(fp))) {
		if (histno[n]++ >=
		~(short)(1L << (BITSPERBYTE * sizeof(short) - 1)))
			histno[n] = 0;
		for (j = i; j > 0; j--) history[n][j] = history[n][j - 1];
		history[n][0] = line;
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

int dohistory(argv, list, maxp)
char *argv[];
namelist *list;
int *maxp;
{
	char *cp, *tmp;
	int i, n, size;

	size = histsize[0];
	if (argv[0][1] == '!') {
		cp = &(argv[0][2]);
		n = 1;
	}
	else if (argv[0][1] == '-') {
		cp = skipnumeric(&(argv[0][2]), 0);
		n = atoi(&(argv[0][2]));
	}
	else if (argv[0][1] >= '0' && argv[0][1] <= '9') {
		cp = skipnumeric(&(argv[0][1]), 0);
		n = histno[0] - atoi(&(argv[0][1]));
	}
	else {
		i = strlen(&(argv[0][1]));
		for (n = 1; n <= size; n++) {
			if (!history[0][n]) break;
			cp = skipspace(history[0][n]);
			if (!strnpathcmp(&(argv[0][1]), cp, i)) break;
		}
		cp = &(argv[0][i + 1]);
	}
	free(history[0][0]);
	if (*cp || n <= 0 || n > size || !history[0][n]) {
		for (i = 0; i < size && history[0][i]; i++)
			history[0][i] = history[0][i + 1];
		if (i >= size) history[0][size] = NULL;
		cprintf2("%s: Event not found.\r\n", &(argv[0][1]));
		warning(0, HITKY_K);
		return(0);
	}
	tmp = catargs(&(argv[1]), ' ');
	if (!tmp) cp = strdup2(history[0][n]);
	else {
		cp = malloc2(strlen(history[0][n]) + strlen(tmp) + 2);
		strcpy(strcpy2(strcpy2(cp, history[0][n]), " "), tmp);
	}
	history[0][0] = cp;
	kanjiputs2(cp, -1, -1);
	cputs2("\r\n");
	n = dosystem(cp, list, maxp, 0);
	free(tmp);
	return(n);
}

#ifdef	DEBUG
VOID freehistory(n)
int n;
{
	int i;

	if (!history[n] || !history[n][0]) return;
	for (i = 0; i < histsize[n]; i++)
		if (history[n][i]) free(history[n][i]);
	free(history[n]);
}
#endif

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

	cp = malloc2((int)strlen(command) - len
		+ strlen(aliaslist[i].comm) + 1);
	strcpy(strcpy2(cp, aliaslist[i].comm), command + len);
	return(cp);
}

#ifndef	_NOCOMPLETE
int completealias(com, argc, argvp)
char *com;
int argc;
char ***argvp;
{
	int i, len;

	if (strdelim(com, 1)) return(argc);
	len = strlen(com);
	for (i = 0; i < maxalias; i++) {
		if (strnpathcmp(com, aliaslist[i].alias, len)
		|| finddupl(aliaslist[i].alias, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(aliaslist[i].alias);
	}
	return(argc);
}

int completeuserfunc(com, argc, argvp)
char *com;
int argc;
char ***argvp;
{
	int i, len;

	if (strdelim(com, 1)) return(argc);
	len = strlen(com);
	for (i = 0; i < maxuserfunc; i++) {
		if (strnpathcmp(com, userfunclist[i].func, len)
		|| finddupl(userfunclist[i].func, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(userfunclist[i].func);
	}
	return(argc);
}
#endif	/* !_NOCOMPLETE */
