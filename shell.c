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

extern int mark;
extern off_t marksize;
extern char fullpath[];
#ifndef	_NOARCHIVE
extern char archivedir[];
#endif

static int NEAR setarg __P_((char *, int, char *, char *, u_char));
static int NEAR setargs __P_((char *, int, u_char));
static int NEAR insertarg __P_((char *, char *, char *, int));
static char *NEAR addoption __P_((char *, char *, macrostat *, int));
static int NEAR dosystem __P_((char *, int, int));
static char *NEAR evalargs __P_((char *, int, char *[]));
static char *NEAR evalalias __P_((char *));

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


static int NEAR setarg(buf, ptr, dir, arg, flags)
char *buf;
int ptr;
char *dir, *arg;
u_char flags;
{
#if	MSDOS && !defined (_NOUSELFN)
	char tmp[MAXPATHLEN];
#endif
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

static int NEAR setargs(buf, ptr, flags)
char *buf;
int ptr;
u_char flags;
{
	int i, n, len, total;
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

	if (!n_args) {
		len = setarg(buf, ptr, dir, filelist[filepos].name, flags);
		total = len;
	}
	else for (i = n = total = 0; i < maxfile; i++) {
		if (!isarg(&(filelist[i]))) continue;
		if (n++) buf[ptr + total++] = ' ';
		len = setarg(buf, ptr + total, dir, filelist[i].name, flags);
		if (!len) {
			if (--n) total--;
			break;
		}
		filelist[i].tmpflags &= ~F_ISARG;
		n_args--;
		total += len;
	}
#if	!MSDOS
	if (dir) free(dir);
#endif
	return(total);
}

static int NEAR insertarg(buf, format, arg, needmark)
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

char *evalcommand(command, arg, stp, ignorelist)
char *command, *arg;
macrostat *stp;
int ignorelist;
{
#if	!MSDOS && !defined (_NOKANJICONV)
	char tmp[MAXCOMMSTR + 1];
	int cnvcode = NOCNV;
	int tmpcode, cnvptr;
#endif
	macrostat st;
	char *cp, line[MAXCOMMSTR + 1];
	int i, j, c, len, uneval, setflag;
	u_char flags;

	if (stp) flags = stp -> flags;
	else {
		stp = &st;
		flags = 0;
	}
	stp -> addopt = -1;
	stp -> needmark = 0;
	uneval = '\0';
#if	!MSDOS && !defined (_NOKANJICONV)
	cnvptr = 0;
#endif

	for (i = j = 0; command[i]; i++) {
		if (j >= MAXCOMMSTR) break;
		c = (uneval) ? '\0' : command[i];
		if (flags & (F_NOEXT | F_TOSFN));
		else if (c == '%') c = command[++i];
		else {
			line[j++] = c;
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
			continue;
		}

		setflag = 0;
		len = 0;
		switch (toupper2(c)) {
			case '\0':
				i--;
				break;
			case 'P':
				len = setarg(line, j, NULL, fullpath, flags);
				if (!len) c = -1;
				break;
#if	MSDOS && !defined (_NOUSELFN)
			case 'S':
				flags |= F_TOSFN;
				c = toupper2(command[i + 1]);
				if (c == 'T' || c == 'M') {
					setflag++;
					break;
				}
#endif
			case 'C':
				len = setarg(line, j, NULL, arg, flags);
				if (!len) c = -1;
				flags |= F_ARGSET;
				break;
			case 'X':
				flags |= F_NOEXT;
				c = toupper2(command[i + 1]);
#if	MSDOS && !defined (_NOUSELFN)
				if (c == 'T' || c == 'M' || c == 'S') {
#else
				if (c == 'T' || c == 'M') {
#endif
					setflag++;
					break;
				}
				len = setarg(line, j, NULL, arg, flags);
				if (!len) c = -1;
				flags |= F_ARGSET;
				break;
			case 'T':
				c = toupper2(command[i + 1]);
				if (c == 'A') {
					flags |= F_REMAIN;
					i++;
				}
				if (!ignorelist) {
					len = setargs(line, j, flags);
					if (!len) c = -1;
					flags |= F_ARGSET;
				}
				break;
			case 'M':
				if (!ignorelist) {
					line[j++] = '\0';
					(stp -> needmark)++;
					line[j++] = flags;
					flags |= F_ARGSET;
				}
				break;
			case 'N':
				flags |= F_ARGSET;
				break;
			case 'R':
				stp -> addopt = j;
				break;
			case 'K':
				flags |= F_NOCONFIRM;
				break;
#if	!MSDOS && !defined (_NOKANJICONV)
			case 'J':
				c = toupper2(command[i + 1]);
				if (c == 'S') tmpcode = SJIS;
				else if (c == 'E' || c == 'U')
					tmpcode = EUC;
				else if (c == 'J') tmpcode = JIS7;
				else {
					line[j++] = command[i];
					break;
				}

				i++;
				if (cnvcode != NOCNV) {
					memcpy(tmp, line + cnvptr,
						j - cnvptr);
					tmp[j - cnvptr] = '\0';
					j = kanjiconv(line + cnvptr,
						tmp, MAXCOMMSTR - cnvptr,
						DEFCODE, cnvcode)
						+ cnvptr;
					if (cnvcode == tmpcode) {
						cnvcode = NOCNV;
						break;
					}
				}
				cnvcode = tmpcode;
				cnvptr = j;
				break;
#endif
			default:
				line[j++] = command[i];
				break;
		}
		if (c < 0) break;
		if (!setflag) flags &= ~(F_NOEXT | F_TOSFN);
		j += len;
	}
	if (command[i]) return(NULL);

#if	!MSDOS && !defined (_NOKANJICONV)
	if (cnvcode != NOCNV) {
		memcpy(tmp, line + cnvptr, j - cnvptr);
		tmp[j - cnvptr] = '\0';
		j = kanjiconv(line + cnvptr, tmp,
			MAXCOMMSTR - cnvptr, DEFCODE, cnvcode) + cnvptr;
	}
#endif

	if (!(flags & F_ARGSET)) {
		line[j++] = ' ';
		line[j++] = '.';
		line[j++] = _SC_;
#if	!MSDOS
		arg = killmeta(arg);
#endif
		j = strcpy2(line + j, arg) - line;
#if	!MSDOS
		free(arg);
#endif
	}
	if (!ignorelist && !(stp -> needmark) && !(flags & F_REMAIN)) {
		for (i = 0; i < maxfile; i++) filelist[i].tmpflags &= ~F_ISARG;
		n_args = 0;
	}
	if ((flags & F_REMAIN) && !n_args) flags &= ~F_REMAIN;
	stp -> flags = flags;

	cp = malloc2(j + 1);
	memcpy(cp, line, j);
	cp[j] = '\0';
	return(cp);
}

static char *NEAR addoption(command, arg, stp, ignorelist)
char *command, *arg;
macrostat *stp;
int ignorelist;
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
		command = evalcommand(cp, arg, stp, ignorelist);
		free(cp);
		if (!command) return((char *)-1);
		if (!*command) return(NULL);
	}
	return(command);
}

static int NEAR dosystem(command, noconf, ignorelist)
char *command;
int noconf, ignorelist;
{
	char *cp, *tmp;
	int n;

	while (*command == ' ' || *command == '\t') command++;
	sigvecreset();
	if ((cp = evalalias(command))) command = cp;

	if ((n = execbuiltin(command, 1, ignorelist)) < -1) {
		tmp = evalcomstr(command, CMDLINE_DELIM, 0);
		system2(tmp, noconf);
		free(tmp);
	}
	sigvecset();
	if (cp) free(cp);
	return(n);
}

int execmacro(command, arg, noconf, argset, ignorelist)
char *command, *arg;
int noconf, argset, ignorelist;
{
	macrostat st;
	char *cp, *tmp, **argv, buf[MAXCOMMSTR + 1];
	int i, max, r, ret;

	max = (ignorelist) ? 0 : maxfile;
	ret = -2;
	for (i = 0; i < max; i++) {
		if (ismark(&(filelist[i]))) filelist[i].tmpflags |= F_ISARG;
		else filelist[i].tmpflags &= ~F_ISARG;
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

	if (!(tmp = evalcommand(command, arg, &st, ignorelist))) return(-2);
	if (noconf >= 0 && (st.flags & F_NOCONFIRM)) noconf = 1 - noconf;
	st.flags |= F_ARGSET;
	if (noconf >= 0 && !argset) {
		cp = addoption(tmp, arg, &st, ignorelist);
		if (cp != tmp) ret = 2;
		if (!cp || cp == (char *)-1) return((cp) ? -2 : 2);
		tmp = cp;
	}
	if (!st.needmark) for (;;) {
		r = dosystem(tmp, noconf, ignorelist);
		free(tmp);
		tmp = NULL;
		if (!argset) st.flags &= ~F_ARGSET;

		if (r == -1) {
			ret = r;
			break;
		}
		if (r >= ret) ret = r;

		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand(command, arg, &st, ignorelist)))
			break;
		if (noconf >= 0 && !argset) {
			cp = addoption(tmp, arg, &st, ignorelist);
			if (cp != tmp && ret >= 0 && ret < 2) ret = 2;
			if (!cp || cp == (char *)-1) return(ret);
			tmp = cp;
		}
	}
	else if (n_args <= 0) {
		if (insertarg(buf, tmp, filelist[filepos].name, st.needmark))
			ret = dosystem(buf, noconf, ignorelist);
		else ret = -2;
	}
	else for (i = 0; i < max; i++) if (isarg(&(filelist[i]))) {
		if (insertarg(buf, tmp, filelist[i].name, st.needmark))
			ret = dosystem(buf, noconf, ignorelist);
		else ret = -2;
	}
	if (tmp) free(tmp);
	if (ret >= -1) return(ret);
	if (!ignorelist) {
		for (i = 0; i < max; i++)
			filelist[i].tmpflags &= ~(F_ISARG | F_ISMRK);
		mark = 0;
		marksize = 0;
	}
	return(4);
}

static char *NEAR evalargs(command, argc, argv)
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
			if (j > MAXCOMMSTR - 2) break;
			line[j++] = '$';
			line[j++] = command[i++];
		}
		else if ((n = command[i++] - '0') < argc)
			for (k = 0; argv[n][k] && j < MAXCOMMSTR; k++)
				line[j++] = argv[n][k];
	}
	while (command[i] && j < MAXCOMMSTR) line[j++] = command[i++];
	line[j] = '\0';

	return(strdup2(line));
}

int execusercomm(command, arg, noconf, argset, ignorelist)
char *command, *arg;
int noconf, argset, ignorelist;
{
	char *cp, **argv;
	int i, j, r, ret, argc;

	if (!(argc = getargs(command, &argv))) i = maxuserfunc;
	else for (i = 0; i < maxuserfunc; i++)
		if (!strpathcmp(argv[0], userfunclist[i].func)) break;
	if (i >= maxuserfunc)
		ret = execmacro(command, arg, noconf, argset, ignorelist);
	else {
		ret = -2;
		for (j = 0; userfunclist[i].comm[j]; j++) {
			cp = evalargs(userfunclist[i].comm[j], argc, argv);
			r = execmacro(cp, arg, noconf, argset, ignorelist);
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

int dohistory(argv)
char *argv[];
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
		free(tmp);
	}
	history[0][0] = cp;
	kanjiputs2(cp, -1, -1);
	cputs2("\r\n");
	n = execusercomm(cp, filelist[filepos].name, 0, 1, 0);
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

static char *NEAR evalalias(command)
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
