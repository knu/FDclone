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

#ifndef	_NOORIGSHELL
#include "system.h"
#endif

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
extern int lcmdline;
extern int hideclock;
extern int internal_status;

static int NEAR setarg __P_((char *, int, char *, char *, u_char));
static int NEAR setargs __P_((char *, int, u_char));
static int NEAR insertarg __P_((char *, char *, char *, int));
static char *NEAR addoption __P_((char *, char *, macrostat *, int));
#ifdef	_NOORIGSHELL
static int NEAR system3 __P_((char *, int, int));
static char *NEAR evalargs __P_((char *, int, char *[]));
static char *NEAR evalalias __P_((char *));
#else
#define	system3(c, n, i)	system2(c, n)
#endif
static char *parsehist __P_((char *, int *));

#ifdef	_NOORIGSHELL
aliastable aliaslist[MAXALIASTABLE];
int maxalias = 0;
userfunctable userfunclist[MAXFUNCTABLE];
int maxuserfunc = 0;
#else
char *promptstr2 = NULL;
#endif
char *promptstr = NULL;
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
#if	!MSDOS && !defined (_NOKANJIFCONV)
	char conv[MAXPATHLEN];
#endif
	char *cp, path[MAXPATHLEN];
	int l;

	if (!arg || !*arg) return(0);
	if (!dir || !*dir) {
#if	MSDOS && !defined (_NOUSELFN)
		if ((flags & F_TOSFN) && shortname(arg, path))
			arg = strdup2(path);
		else
#endif
		arg = killmeta(arg);
	}
	else {
		strcatdelim2(path, dir, arg);
		arg = killmeta(path);
#if	MSDOS && !defined (_NOARCHIVE)
		if (flags & F_ISARCH) for (l = 0; arg[l]; l++) {
			if (iskanji1(arg, l)) l++;
			if (arg[l] == _SC_) arg[l] = '/';
		}
#endif
	}

#if	!MSDOS && !defined (_NOKANJIFCONV)
	cp = kanjiconv2(conv, arg, MAXPATHLEN - 1, DEFCODE, getkcode(arg));
	if (cp != arg) {
		free(arg);
		arg = strdup2(cp);
	}
#endif
	if ((flags & F_NOEXT) && (cp = strrchr(arg, '.')) && cp != arg)
		l = cp - arg;
	else l = strlen(arg);
	if (ptr + l >= MAXCOMMSTR - 1) l = 0;
	else strncpy(buf + ptr, arg, l);
	free(arg);
	return(l);
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
	dir = (archivefile) ? archivedir : NULL;
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

	arg = killmeta(arg);
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
	free(arg);
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
			line[j++] = command[i];
#if	MSDOS && defined (_NOORIGSHELL)
			if (command[i] == uneval) uneval = 0;
			else if (command[i] == '"' || command[i] == '\'')
				uneval = command[i];
#else
			if (command[i] == PMETA && command[i + 1]
			&& uneval != '\'')
				line[j++] = command[++i];
			else if (command[i] == uneval) uneval = 0;
			else if (command[i] == '"' || command[i] == '\''
			|| command[i] == '`')
				uneval = command[i];
#endif
			continue;
		}

		len = setflag = 0;
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
				else if (c == 'E') tmpcode = EUC;
# if	FD < 2
				else if (c == 'J') tmpcode = JIS7;
# else
				else if (c == '7') tmpcode = JIS7;
				else if (c == '8') tmpcode = JIS8;
				else if (c == 'J') tmpcode = JUNET;
				else if (c == 'H') tmpcode = HEX;
				else if (c == 'C') tmpcode = CAP;
				else if (c == 'U') tmpcode = UTF8;
# endif
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
						DEFCODE, cnvcode, L_FNAME)
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
	if (command[i]) {
		warning(E2BIG, command);
		return(NULL);
	}

#if	!MSDOS && !defined (_NOKANJICONV)
	if (cnvcode != NOCNV) {
		memcpy(tmp, line + cnvptr, j - cnvptr);
		tmp[j - cnvptr] = '\0';
		j = kanjiconv(line + cnvptr, tmp, MAXCOMMSTR - cnvptr,
			DEFCODE, cnvcode, L_FNAME) + cnvptr;
	}
#endif

	if (!(flags & F_ARGSET)) {
#if	!MSDOS && !defined (_NOKANJIFCONV)
		char conv[MAXPATHLEN];

		arg = kanjiconv2(conv, arg, MAXPATHLEN - 1,
			DEFCODE, getkcode(arg));
#endif
		line[j++] = ' ';
		cp = malloc2(strlen(arg) + 2 + 1);
		cp[0] = '.';
		cp[1] = _SC_;
		strcpy(&(cp[2]), arg);
		arg = killmeta(cp);
		free(cp);
		len = strlen(arg);
		if (j + len < MAXCOMMSTR - 1) {
			strncpy(&(line[j]), arg, len);
			j += len;
		}
		free(arg);
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

char *inputshellstr(prompt, ptr, def)
char *prompt;
int ptr;
char *def;
{
	char *cp, *tmp, *duppromptstr;

	duppromptstr = promptstr;
	if (prompt) {
		promptstr = prompt;
		lcmdline = n_line - 1;
	}
	cp = inputstr(NULL, 0, ptr, def, 0);
	promptstr = duppromptstr;
	if (!cp) return((char *)-1);

	stdiomode();
	tmp = evalhistory(cp);
	ttyiomode();
	if (!tmp) {
		free(cp);
		return(NULL);
	}
	entryhist(0, tmp, 0);
	return(tmp);
}

char *inputshellloop(ptr, def)
int ptr;
char *def;
{
#ifndef	_NOORIGSHELL
	syntaxtree *trp, *stree;
	char *buf;
	int l, len;
#endif
	char *cp;

	if ((cp = inputshellstr(NULL, ptr, def)) == (char *)-1) return(NULL);
	else if (!cp) {
		hideclock = 1;
		warning(0, HITKY_K);
		rewritefile(1);
		return(NULL);
	}

#ifndef	_NOORIGSHELL
	len = strlen(cp);
	buf = malloc2(len + 1);
	strcpy(buf, cp);
	trp = stree = newstree(NULL);
	for (;;) {
		if (cp) {
			trp = analyze(cp, trp, 1);
			free(cp);
			if (!trp || !(trp -> flags & ST_CONT)) break;
		}

		lcmdline = n_line - 1;
		hideclock = 1;
		if ((cp = inputshellstr(promptstr2, -1, NULL)) == (char *)-1)
			break;
		else if (!cp) continue;

		l = strlen(cp);
		buf = realloc2(buf, len + l + 1 + 1);
		buf[len++] = '\n';
		strcpy(&(buf[len]), cp);
		len += l;
	}
	freestree(stree);
	free(stree);
	cp = buf;
#endif	/* !_NOORIGSHELL */

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
		free(command);

		if (i >= len) {
			warning(0, INOV_K);
			return(NULL);
		}

		if (p < 0) p = strlen(line);
		cp = inputstr("", 0, p, line, 0);
		if (!cp) return(NULL);
		if (!*cp) {
			free(cp);
			return(NULL);
		}
		command = evalcommand(cp, arg, stp, ignorelist);
		free(cp);
		if (!command) return(NULL);
		if (!*command) {
			free(command);
			return(NULL);
		}
	}
	return(command);
}

#ifdef	_NOORIGSHELL
static int NEAR system3(command, noconf, ignorelist)
char *command;
int noconf, ignorelist;
{
	char *cp, *tmp;
	int n;

	while (*command == ' ' || *command == '\t') command++;
	if ((cp = evalalias(command))) command = cp;

	sigvecreset();
	if ((n = execpseudoshell(command, 1, ignorelist)) < 0) {
		tmp = evalcomstr(command, CMDLINE_DELIM);
		n = system2(tmp, noconf);
		free(tmp);
	}
	sigvecset();
	if (cp) free(cp);
	return(n);
}
#endif	/* _NOORIGSHELL */

int execmacro(command, arg, noconf, argset, ignorelist)
char *command, *arg;
int noconf, argset, ignorelist;
{
	static char *duparg = NULL;
	macrostat st;
	char *tmp, **argv, buf[MAXCOMMSTR + 1];
	int i, max, r, ret, status;

	if (arg) duparg = arg;
	max = (ignorelist) ? 0 : maxfile;
	ret = 0;
	status = internal_status = -2;
	for (i = 0; i < max; i++) {
		if (ismark(&(filelist[i]))) filelist[i].tmpflags |= F_ISARG;
		else filelist[i].tmpflags &= ~F_ISARG;
	}
	n_args = mark;

	st.flags = F_ARGSET;
	if (!argset) {
#ifdef	_NOORIGSHELL
		if (getargs(command, &argv) > 0 && checkinternal(argv[0]) < 0)
			st.flags = 0;
		freevar(argv);
#else	/* !_NOORIGSHELL */
		syntaxtree *stree;
		char **subst;
		int *len;

		stree = newstree(NULL);
		if (analyze(command, stree, 1)) {
			if ((argv = getsimpleargv(stree))) {
				i = (stree -> comm) -> argc;
				(stree -> comm) -> argc =
					getsubst(i, argv, &subst, &len);
				stripquote(argv[0], 1);
				if (argv[0] && checkinternal(argv[0]) < 0)
					st.flags = 0;
				freevar(subst);
				free(len);
			}
		}
		freestree(stree);
		free(stree);
#endif	/* !_NOORIGSHELL */
	}
	if (noconf < 0) st.flags |= F_ISARCH;

	if (!(tmp = evalcommand(command, duparg, &st, ignorelist))) {
		if (arg) duparg = NULL;
		return(-1);
	}
	if (noconf >= 0 && (st.flags & F_NOCONFIRM)) noconf = 1 - noconf;
	st.flags |= F_ARGSET;
	if (noconf >= 0 && !argset) {
		if (!(tmp = addoption(tmp, duparg, &st, ignorelist))) {
			if (arg) duparg = NULL;
			return(-1);
		}
	}
	if (!st.needmark) for (;;) {
		r = system3(tmp, noconf, ignorelist);
		free(tmp);
		tmp = NULL;
		if (!argset) st.flags &= ~F_ARGSET;

		if (r > ret && (ret = r) >= 127) break;
		if (internal_status < -1) status = 4;
		else if (internal_status > status) status = internal_status;

		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand(command, duparg, &st, ignorelist)))
			break;
		if (noconf >= 0 && !argset) {
			if (!(tmp = addoption(tmp, duparg, &st, ignorelist)))
				break;
		}
	}
	else if (n_args <= 0) {
		if (insertarg(buf, tmp, filelist[filepos].name, st.needmark)) {
			ret = system3(buf, noconf, ignorelist);
			status = internal_status;
		}
	}
	else for (i = 0; i < max; i++) if (isarg(&(filelist[i]))) {
		if (insertarg(buf, tmp, filelist[i].name, st.needmark)) {
			r = system3(buf, noconf, ignorelist);
			if (r > ret && (ret = r) >= 127) break;
			if (internal_status < -1) status = 4;
			else if (internal_status > status)
				status = internal_status;
		}
	}
	if (tmp) free(tmp);
	if ((internal_status = status) < -1 && !ignorelist) {
		for (i = 0; i < max; i++)
			filelist[i].tmpflags &= ~(F_ISARG | F_ISMRK);
		mark = 0;
		marksize = 0;
	}
	if (arg) duparg = NULL;
	return(ret);
}

#ifdef	_NOORIGSHELL
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
	int i, j, r, ret, status, argc;

	if (!(argc = getargs(command, &argv))) i = maxuserfunc;
	else for (i = 0; i < maxuserfunc; i++)
		if (!strpathcmp(argv[0], userfunclist[i].func)) break;
	if (i >= maxuserfunc)
		ret = execmacro(command, arg, noconf, argset, ignorelist);
	else {
		ret = 0;
		status = internal_status = -2;
		for (j = 0; userfunclist[i].comm[j]; j++) {
			cp = evalargs(userfunclist[i].comm[j], argc, argv);
			r = execmacro(cp, arg, noconf, argset, ignorelist);
			free(cp);
			if (r > ret && (ret = r) >= 127) break;
			if (internal_status < -1) status = 4;
			else if (internal_status > status)
				status = internal_status;
		}
		internal_status = status;
	}
	freevar(argv);
	return(ret);
}

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
#endif	/* _NOORIGSHELL */

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

	if (histno[n]++ >= MAXTYPE(short)) histno[n] = 0;

	if (uniq) {
		for (i = 0; i <= size; i++) {
			if (!history[n][i]) continue;
#if	MSDOS
			if (n == 1 && *s != *(history[n][i])) continue;
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

	i = -1;
	while ((line = fgets2(fp, 1))) {
		if (histno[n]++ >= MAXTYPE(short)) histno[n] = 0;
		if (i < size) i++;
		else free(history[n][i]);
		for (j = i; j > 0; j--) history[n][j] = history[n][j - 1];
		history[n][0] = line;
	}
	fclose(fp);

	for (i++; i <= size; i++) history[n][i] = NULL;
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

static char *parsehist(str, ptrp)
char *str;
int *ptrp;
{
	char *cp;
	long n;
	int i, size;

	size = histsize[0];
	if (str[*ptrp] == '!') n = 0;
	else if (str[*ptrp] == '=' || strchr(IFS_SET, str[*ptrp])) {
		*ptrp = -1;
		return(NULL);
	}
	else if ((cp = evalnumeric(&(str[*ptrp]), &n, -1))) {
		if (n < 0) n--;
		else n = histno[0] - n;
		*ptrp = cp - str - 1;
	}
	else {
		for (i = 0; str[*ptrp + i]; i++) {
			if (strchr(CMDLINE_DELIM, str[*ptrp + i])) break;
			if (iskanji1(str, *ptrp + i)) i++;
		}
		if (!i) {
			*ptrp = -1;
			return(NULL);
		}
		for (n = 0; n < size; n++) {
			if (!history[0][n]) break;
			cp = skipspace(history[0][n]);
			if (!strnpathcmp(&(str[*ptrp]), cp, i)) break;
		}
		*ptrp += i - 1;
	}

	if (n < 0 || n >= size) return(NULL);
	return(history[0][n]);
}

char *evalhistory(command)
char *command;
{
	char *cp, *tmp;
	int i, j, len, size, quote, hit;

	hit = 0;
	cp = c_malloc(size);
	for (i = j = 0, quote = '\0'; command[i]; i++, j++) {
		cp = c_realloc(cp, j + 1, size);
		if (command[i] == quote) quote = '\0';
#ifdef	CODEEUC
		else if (isekana(command, i)) cp[j++] = command[i++];
#endif
		else if (iskanji1(command, i)) cp[j++] = command[i++];
		else if (ismeta(command, i, quote)) cp[j++] = command[i++];
		else if (quote);
		else if (command[i] == '\'') quote = command[i];
		else if (command[i] == '!') {
			len = i++;
			if (!(tmp = parsehist(command, &i))) {
				if (i < 0) {
					cp[j] = '!';
					i = len;
					continue;
				}
				command[++i] = '\0';
				fputs(&(command[len]), stderr);
				fputs(": Event not found.\n", stderr);
				free(cp);
				return(NULL);
			}
			hit++;
			len = strlen(tmp);
			cp = c_realloc(cp, j + len, size);
			strncpy(&cp[j], tmp, len);
			j += len - 1;
			continue;
		}
		cp[j] = command[i];
	}
	cp[j] = '\0';
	if (hit) {
		kanjifputs(cp, stderr);
		fputc('\n', stderr);
		free(command);
		return(cp);
	}
	free(cp);
	return(command);
}

#ifdef	DEBUG
VOID freehistory(n)
int n;
{
	int i;

	if (!history[n] || !history[n][0]) return;
	for (i = 0; i <= histbufsize[n]; i++)
		if (history[n][i]) free(history[n][i]);
	free(history[n]);
}
#endif

#if	!defined (_NOCOMPLETE) && defined (_NOORIGSHELL)
int completealias(com, len, argc, argvp)
char *com;
int len, argc;
char ***argvp;
{
	int i;

	for (i = 0; i < maxalias; i++) {
		if (strnpathcmp(com, aliaslist[i].alias, len)
		|| finddupl(aliaslist[i].alias, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(aliaslist[i].alias);
	}
	return(argc);
}

int completeuserfunc(com, len, argc, argvp)
char *com;
int len, argc;
char ***argvp;
{
	int i;

	for (i = 0; i < maxuserfunc; i++) {
		if (strnpathcmp(com, userfunclist[i].func, len)
		|| finddupl(userfunclist[i].func, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(userfunclist[i].func);
	}
	return(argc);
}
#endif	/* !_NOCOMPLETE && _NOORIGSHELL */
