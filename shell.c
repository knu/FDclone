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

#if	MSDOS && !defined (_NOUSELFN)
extern char *shortname __P_((char *, char *));
#endif

#ifndef	_NOORIGSHELL
#include "system.h"
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

#define	MACROMETA	((char)-1)
#define	isneedburst(s, i) \
			((s)[i] == MACROMETA && ((s)[(i) + 1] & F_BURST))
#define	isneedmark(s, i) \
			((s)[i] == MACROMETA && ((s)[(i) + 1] & F_MARK))
#ifdef	ARG_MAX
#define	MAXARGNUM	ARG_MAX
#else
# ifdef	NCARGS
# define	MAXARGNUM	NCARGS
# endif
#endif

static int NEAR checksc __P_((char *, int, char *));
#if	!MSDOS && !defined (_NOKANJICONV)
static int NEAR extconv __P_((char **, int, int, ALLOC_T *, int));
#endif
static int NEAR setarg __P_((char **, int, ALLOC_T *, char *, char *, u_char));
#ifdef	_NOEXTRAMACRO
static int NEAR setargs __P_((char **, int, int, int, ALLOC_T *, macrostat *));
static char *NEAR insertarg __P_((char *, char *, int));
#endif
static char *NEAR addoption __P_((char *, char *, macrostat *, int));
#ifdef	_NOORIGSHELL
static int NEAR system3 __P_((char *, int, int));
static char *NEAR evalargs __P_((char *, int, char *[]));
static char *NEAR evalalias __P_((char *));
#else
#define	system3(c, n, i)	system2(c, n)
#endif

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


static int NEAR checksc(buf, ptr, arg)
char *buf;
int ptr;
char *arg;
{
	if (!arg || !*arg);
	else if (*arg == _SC_ || isdotdir(arg));
#if	MSDOS
	else if (_dospath(arg));
#endif
	else return(ptr);

	if (buf) {
		if (ptr < 2) return(ptr);
		if (buf[ptr - 1] != _SC_ || buf[ptr - 2] != '.') return(ptr);
		if (ptr > 2 && !strchr(IFS_SET, buf[ptr - 3])) return(ptr);
	}

	return(ptr - 2);
}

#if	!MSDOS && !defined (_NOKANJICONV)
static int NEAR extconv(bufp, ptr, eol, sizep, code)
char **bufp;
int ptr, eol;
ALLOC_T *sizep;
int code;
{
# ifndef	_NOKANJIFCONV
	char rpath[MAXPATHLEN];

# endif
	char *cp;
	int len;

# ifndef	_NOKANJIFCONV
	if (code < 0) {
		cp = _evalpath(&((*bufp)[ptr]), &((*bufp)[eol]), 0, 0);
		realpath2(cp, rpath, 1);
		free(cp);
		code = getkcode(rpath);
	}
# endif
	(*bufp)[eol] = '\0';
	len = (eol - ptr) * 3 + 3;
	cp = malloc2(len + 1);
	len = kanjiconv(cp, &((*bufp)[ptr]), len, DEFCODE, code, L_FNAME);

	*bufp = c_realloc(*bufp, ptr + len + 1, sizep);
	memcpy(&((*bufp)[ptr]), cp, len);
	free(cp);
	return(ptr + len);
}
#endif	/* !MSDOS && !_NOKANJICONV */

static int NEAR setarg(bufp, ptr, sizep, dir, arg, flags)
char **bufp;
int ptr;
ALLOC_T *sizep;
char *dir, *arg;
u_char flags;
{
	char *cp, path[MAXPATHLEN], conv[MAXPATHLEN];
	int len, optr;

	if (!arg || !*arg) return(checksc(*bufp, ptr, NULL) - ptr);
	if (!dir || !*dir) {
#if	MSDOS && !defined (_NOUSELFN)
		if ((flags & F_TOSFN) && shortname(arg, path) == path)
			arg = path;
#endif
	}
	else {
		strcatdelim2(path, dir, arg);
		arg = path;
#if	MSDOS && !defined (_NOARCHIVE)
		if (flags & F_ISARCH) for (len = 0; arg[len]; len++) {
			if (iskanji1(arg, len)) len++;
			if (arg[len] == _SC_) arg[len] = '/';
		}
#endif
	}

	if (arg != path) arg = convput(conv, arg, 1, 0, NULL, NULL);
#if	!MSDOS && !defined (_NOKANJIFCONV)
	else arg = kanjiconv2(conv, arg, MAXPATHLEN - 1, DEFCODE, fnamekcode);
#endif
	optr = ptr;
	ptr = checksc(*bufp, ptr, arg);
#ifndef	_NOEXTRAMACRO
	if (flags & (F_BURST | F_MARK)) arg = strdup2(arg);
	else
#endif
	arg = killmeta(arg);

	if ((flags & F_NOEXT) && (cp = strrchr(arg, '.')) && cp != arg)
		len = cp - arg;
	else len = strlen(arg);

	*bufp = c_realloc(*bufp, ptr + len + 1, sizep);
	strncpy(&((*bufp)[ptr]), arg, len);
	free(arg);
	return(len + ptr - optr);
}

#ifdef	_NOEXTRAMACRO
/*ARGSUSED*/
static int NEAR setargs(bufp, ptr, blen, eol, sizep, stp)
char **bufp;
int ptr, blen, eol;
ALLOC_T *sizep;
macrostat *stp;
{
# ifdef	MAXCOMMSTR
	int optr;
# endif
	char *cp, *s, *dir, *tmp;
	int i, n, len, flen, rlen;
	u_char flags;

# ifdef	_NOARCHIVE
	dir = NULL;
# else
	dir = (archivefile) ? archivedir : NULL;
# endif

	len = eol - ptr;
	tmp = malloc2(len + 1);
	memcpy(tmp, &((*bufp)[ptr]), len);
	tmp[len] = '\0';
	cp = tmp + blen;
	flags = *(++cp);
	for (s = ++cp; *s; s++) {
		if (strchr(CMDLINE_DELIM, *s)) break;
		if (iskanji1(s, 0)) s++;
	}
	flen = s - cp;
	rlen = len - (s - tmp);

	ptr += blen;
	if (!n_args) {
		len = setarg(bufp, ptr, sizep,
			dir, filelist[filepos].name, flags);
		ptr += len;
# ifdef	MAXCOMMSTR
		if (ptr > MAXCOMMSTR || ptr + flen > MAXCOMMSTR) {
			free(tmp);
			return(-1);
		}
# endif
		*bufp = c_realloc(*bufp, ptr + flen + 1, sizep);
		strncpy(&((*bufp)[ptr]), cp, flen);
		ptr += flen;
	}
	else for (i = n = 0; i < maxfile; i++) {
		if (!isarg(&(filelist[i]))) continue;
# ifdef	MAXCOMMSTR
		optr = ptr;
# endif
		if (n) {
# ifdef	MAXCOMMSTR
			if (ptr + blen + 1 > MAXCOMMSTR) break;
# endif
			*bufp = c_realloc(*bufp, ptr + blen + 1 + 1, sizep);
			(*bufp)[ptr++] = ' ';
			strncpy(&((*bufp)[ptr]), tmp, blen);
			ptr += blen;
		}
		len = setarg(bufp, ptr, sizep, dir, filelist[i].name, flags);
		ptr += len;
# ifdef	MAXCOMMSTR
		if (ptr > MAXCOMMSTR || ptr + flen > MAXCOMMSTR) {
			if (!n) {
				free(tmp);
				return(-1);
			}
			ptr = optr;
			break;
		}
# endif
		*bufp = c_realloc(*bufp, ptr + flen + 1, sizep);
		strncpy(&((*bufp)[ptr]), cp, flen);
		ptr += flen;
		filelist[i].tmpflags &= ~F_ISARG;
		n_args--;
		n++;
	}

# ifdef	MAXCOMMSTR
	if (ptr + rlen > MAXCOMMSTR) {
		for (i = MAXCOMMSTR - ptr; i < rlen; i++)
			if (isneedmark(s, i)) {
				(stp -> needmark)--;
				i++;
			}
		rlen = MAXCOMMSTR - ptr;
	}
# endif
	*bufp = c_realloc(*bufp, ptr + rlen + 1, sizep);
	memcpy(&((*bufp)[ptr]), s, rlen);
	ptr += rlen;
	free(tmp);
	return(ptr - eol);
}

static char *NEAR insertarg(format, arg, needmark)
char *format, *arg;
int needmark;
{
# if	MSDOS && !defined (_NOUSELFN)
	char *org, alias[MAXPATHLEN];
# endif
	char *cp, *src, *ins, *buf, conv[MAXPATHLEN];
	ALLOC_T size;
	int i, j, len;

# if	MSDOS && !defined (_NOUSELFN)
	org = arg;
	*alias = '\0';
# endif
	arg = convput(conv, arg, 1, 0, NULL, NULL);
	for (j = 0; format[j]; j++) if (isneedmark(format, j)) break;
	buf = c_realloc(NULL, 0, &size);
# ifdef	MAXCOMMSTR
	if (j > MAXCOMMSTR) i = j = 0;
	else
# endif
	{
		buf = c_realloc(buf, j + 1, &size);
		memcpy(buf, format, j);
		src = format + j + 1;
		for (i = 0; i < needmark; i++) {
			ins = arg;
# if	MSDOS && !defined (_NOUSELFN)
			if (*src & F_TOSFN) {
				if (!*alias) {
					if (shortname(org, alias) == alias)
						org = alias;
					else strcpy(alias, org);
				}
				ins = alias;
			}
# endif
			j = checksc(buf, j, ins);
			cp = killmeta(ins);

			if ((*src & F_NOEXT) && (cp = strrchr(cp, '.')))
				len = cp - cp;
			else len = strlen(cp);
# ifdef	MAXCOMMSTR
			if (j + len > MAXCOMMSTR) break;
# endif
			buf = c_realloc(buf, j + len + 1, &size);
			strncpy(&(buf[j]), cp, len);
			free(cp);
			j += len;
			src++;
			for (len = 0; src[len]; len++)
				if (isneedmark(src, len)) break;
# ifdef	MAXCOMMSTR
			if (j + len > MAXCOMMSTR) break;
# endif
			buf = c_realloc(buf, j + len + 1, &size);
			memcpy(&(buf[j]), src, len);
			j += len;
			src += len + 1;
		}
	}
	buf[j] = '\0';
	if (i < needmark) {
		free(buf);
		return(NULL);
	}
	return(buf);
}
#endif	/* _NOEXTRAMACRO */

char *evalcommand(command, arg, stp, ignorelist)
char *command, *arg;
macrostat *stp;
int ignorelist;
{
#if	!MSDOS && !defined (_NOKANJICONV)
	int cnvcode = NOCNV;
	int tmpcode, cnvptr;
#endif
	macrostat st;
	char *cp, *line;
	ALLOC_T size;
	int i, j, c, len, quote, setflag;
	u_char flags;

	if (stp) flags = stp -> flags;
	else {
		stp = &st;
		flags = 0;
	}
	stp -> addopt = -1;
	stp -> needmark = stp -> needburst = 0;
	quote = '\0';
#if	!MSDOS && !defined (_NOKANJICONV)
	cnvptr = 0;
#endif

	line = c_realloc(NULL, 0, &size);
	for (i = j = 0; command[i]; i++) {
		line = c_realloc(line, j + 1, &size);

		c = '\0';
		if (command[i] == quote) quote = '\0';
#ifdef	CODEEUC
		else if (isekana(command, i)) line[j++] = command[i++];
#endif
		else if (iskanji1(command, i)) line[j++] = command[i++];
		else if (quote == '\'');
		else if (ismeta(command, i, quote)) line[j++] = command[i++];
		else if (quote);
		else if (command[i] == '\'' || command[i] == '"')
			quote = command[i];
		else if (command[i] == '%') c = command[++i];
		else if (flags & (F_NOEXT | F_TOSFN)) c = command[i];

		if (!c) {
			line[j++] = command[i];
			continue;
		}

		len = setflag = 0;
		switch (toupper2(c)) {
			case 'P':
				len = setarg(&line, j, &size,
					NULL, fullpath, flags);
				break;
#if	MSDOS
			case 'S':
				flags |= F_TOSFN;
				c = toupper2(command[i + 1]);
				if (c == 'T' || c == 'M') {
					setflag++;
					break;
				}
/*FALLTHRU*/
#endif
			case 'C':
				len = setarg(&line, j, &size,
					NULL, arg, flags);
				flags |= F_ARGSET;
				break;
			case 'X':
				flags |= F_NOEXT;
				c = toupper2(command[i + 1]);
#if	MSDOS
				if (c == 'T' || c == 'M' || c == 'S') {
#else
				if (c == 'T' || c == 'M') {
#endif
					setflag++;
					break;
				}
				len = setarg(&line, j, &size,
					NULL, arg, flags);
				flags |= F_ARGSET;
				break;
			case 'T':
				c = toupper2(command[i + 1]);
				if (c == 'A') {
					flags |= F_REMAIN;
					i++;
				}
				if (!ignorelist) {
					line[j++] = MACROMETA;
					(stp -> needburst)++;
					line[j++] = flags | F_BURST;
					flags |= F_ARGSET;
				}
				break;
			case 'M':
				if (!ignorelist) {
					line[j++] = MACROMETA;
					(stp -> needmark)++;
					line[j++] = flags | F_MARK;
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
# else	/* FD >= 2 */
				else if (c == '7') tmpcode = JIS7;
				else if (c == '8') tmpcode = JIS8;
				else if (c == 'J') tmpcode = JUNET;
				else if (c == 'H') tmpcode = HEX;
				else if (c == 'C') tmpcode = CAP;
				else if (c == 'U') tmpcode = UTF8;
#  ifndef	_NOKANJIFCONV
				else if (c == 'A') tmpcode = -1;
#  endif
# endif	/* FD >= 2 */
				else {
					line[j++] = command[i];
					break;
				}

				i++;
				if (cnvcode != NOCNV) {
					j = extconv(&line, cnvptr, j,
						&size, cnvcode);
					if (cnvcode == tmpcode) {
						cnvcode = NOCNV;
						break;
					}
				}
				cnvcode = tmpcode;
				cnvptr = j;
				break;
#endif	/* !MSDOS && !_NOKANJICONV */
			default:
				line[j++] = command[i];
				break;
		}
		if (!setflag) flags &= ~(F_NOEXT | F_TOSFN);
		j += len;
	}
#if	defined (_NOEXTRAMACRO) && defined (MAXCOMMSTR)
	if (j > MAXCOMMSTR) {
		if (isttyiomode) warning(E2BIG, command);
		else {
			errno = E2BIG;
			perror2(command);
		}
		free(line);
		return(NULL);
	}
#endif

#if	!MSDOS && !defined (_NOKANJICONV)
	if (cnvcode != NOCNV) j = extconv(&line, cnvptr, j, &size, cnvcode);
#endif

#ifdef	_NOEXTRAMACRO
	if (stp -> needburst) for (i = c = 0; i < j; i++) {
		if (strchr(CMDLINE_DELIM, line[i])) c = -1;
		else if (c < 0) c = i;
		if (!isneedburst(line, i)) {
			if (iskanji1(line, i)) i++;
			continue;
		}

		if (stp -> needburst <= 0) {
			memmove(&(line[i]), &(line[i + 2]), j - i - 2);
			j -= 2;
			i--;
			continue;
		}

		(stp -> needburst)--;
		len = setargs(&line, c, i - c, j, &size, stp);
		if (len < 0) {
			for (i += 2; i < j; i++) if (isneedmark(line, i)) {
				(stp -> needmark)--;
				i++;
			}
			j = c;
			break;
		}
		j += len;
		i += len + 1;
	}
#endif	/* _NOEXTRAMACRO */

	if (!(flags & F_ARGSET) && arg && *arg) {
		char conv[MAXPATHLEN];

		arg = convput(conv, arg, 1, 1, NULL, NULL);
		if (checksc(NULL, 0, arg) < 0) cp = NULL;
		else {
			cp = malloc2(strlen(arg) + 2 + 1);
			cp[0] = '.';
			cp[1] = _SC_;
			strcpy(&(cp[2]), arg);
			arg = cp;
		}
		arg = killmeta(arg);
		if (cp) free(cp);

		len = strlen(arg);
#if	defined (_NOEXTRAMACRO) && defined (MAXCOMMSTR)
		if (j + len + 1 > MAXCOMMSTR);
		else
#endif
		{
			line = c_realloc(line, j + len + 1, &size);
			line[j++] = ' ';
			strncpy(&(line[j]), arg, len);
			j += len;
		}
		free(arg);
	}
	if (!ignorelist && !(stp -> needburst) && !(stp -> needmark)) {
		for (i = 0; i < maxfile; i++) filelist[i].tmpflags &= ~F_ISARG;
		n_args = 0;
	}
	if ((flags & F_REMAIN) && !n_args) flags &= ~F_REMAIN;
	stp -> flags = flags;

	cp = strdupcpy(line, j);
	free(line);
	return(cp);
}

/*ARGSUSED*/
int replaceargs(argcp, argvp, env, iscomm)
int *argcp;
char ***argvp, **env;
int iscomm;
{
#ifdef	_NOEXTRAMACRO
	return(0);
#else	/* !_NOEXTRAMACRO */
	static int lastptr = 0;
	char *cp, *arg, *buf, *dir, **argv, **argv2;
	ALLOC_T size;
	int i, j, n, argc, argc2, len, maxarg, min, next, ret;
	u_char flags;

	if (!argcp || !argvp) {
		lastptr = 0;
		return(0);
	}

	argc = *argcp;
	argv = *argvp;
	ret = 0;

	if (!filelist) return(ret);

# ifdef	_NOARCHIVE
	dir = NULL;
# else
	dir = (archivefile) ? archivedir : NULL;
# endif

	maxarg = i = 0;
	for (n = 0; n < argc; n++) {
		if (n) maxarg++;
		for (len = 0; argv[n][len]; len++) {
			if (!isneedburst(argv[n], len)
			&& !isneedmark(argv[n], len))
				maxarg++;
			else {
				len++;
				i++;
			}
		}
	}
	if (!i) return(ret);

# ifdef	MAXCOMMSTR
	if (iscomm > 0 && maxarg > MAXCOMMSTR) {
		errno = E2BIG;
		return(-1);
	}
# else
#  ifdef	MAXARGNUM
	maxarg = MAXARGNUM - 2048;
	if (maxarg > 20 * 1024) maxarg = 20 * 1024;
	maxarg -= countvar(env);
	if (iscomm > 0 && argc > maxarg) {
		errno = E2BIG;
		return(-1);
	}
#  endif
# endif

	argc2 = 0;
	argv2 = NULL;
	buf = c_realloc(NULL, 0, &size);

	min = -1;
	for (next = lastptr; next < maxfile; next++)
	if (isarg(&(filelist[next]))) {
		if (min < 0) min = next;
		else break;
	}
	arg = (min >= 0) ? filelist[min].name : filelist[filepos].name;

	for (n = 0; n < argc; n++) {
		flags = 0;
		for (i = j = 0; argv[n][i]; i++) {
			buf = c_realloc(buf, j + 1, &size);
			if (!isneedburst(argv[n], i)
			&& (iscomm >= 0 || !isneedmark(argv[n], i)))
				buf[j++] = argv[n][i];
			else {
				flags = argv[n][++i];
				cp = (iscomm < 0 || n) ? arg : NULL;

				len = setarg(&buf, j, &size, dir, cp, flags);
# ifdef	MAXCOMMSTR
				maxarg += len;
				if (iscomm > 0 && maxarg > MAXCOMMSTR) {
					free(buf);
					freevar(argv2);
					errno = E2BIG;
					return(-1);
				}
# endif
				j += len;
			}
		}
		buf[j++] = '\0';
		argv2 = (char **)realloc2(argv2, (argc2 + 2) * sizeof(char *));
		argv2[argc2++] = strdup2(buf);
		argv2[argc2] = NULL;
		if (min < 0 || !flags || (iscomm >= 0 && !n)) continue;

		while (next < maxfile) {
# if	!defined (MAXCOMMSTR) && defined (MAXARGNUM)
			if (iscomm > 0 && argc + (argc2 - n) + 1 > maxarg) {
				if (flags & F_REMAIN) ret++;
				break;
			}
# endif
			arg = filelist[next].name;
			for (i = j = 0; argv[n][i]; i++) {
				buf = c_realloc(buf, j + 1, &size);
				if (!isneedburst(argv[n], i)
				&& (iscomm >= 0 || !isneedmark(argv[n], i)))
					buf[j++] = argv[n][i];
				else {
					flags = argv[n][++i];

					len = setarg(&buf, j, &size,
						dir, arg, flags);
# ifdef	MAXCOMMSTR
					maxarg += len;
					if (iscomm > 0 && maxarg > MAXCOMMSTR)
						break;
# endif
					j += len;
				}
			}
# ifdef	MAXCOMMSTR
			if (argv[n][i]) {
				if (flags & F_REMAIN) ret++;
				break;
			}
# endif
			buf[j++] = '\0';
			argv2 = (char **)realloc2(argv2,
				(argc2 + 2) * sizeof(char *));
			argv2[argc2++] = strdup2(buf);
			argv2[argc2] = NULL;
			for (next++; next < maxfile; next++)
				if (isarg(&(filelist[next]))) break;
		}
	}
	if (ret) lastptr = next;
	freevar(argv);
	*argcp = argc = argc2;
	*argvp = argv = argv2;

	min = -1;
	for (next = lastptr; next < maxfile; next++)
	if (isarg(&(filelist[next]))) {
		if (min < 0) min = next;
		else break;
	}
	arg = (min >= 0) ? filelist[min].name : filelist[filepos].name;

	flags = 0;
	for (n = 0; n < argc; n++) {
		for (i = j = 0; argv[n][i]; i++) {
			buf = c_realloc(buf, j + 1, &size);
			if (!isneedmark(argv[n], i)) buf[j++] = argv[n][i];
			else {
				flags = argv[n][++i];
				cp = (iscomm < 0 || n) ? arg : NULL;
				if (min >= 0 && next < maxfile) ret++;

				len = setarg(&buf, j, &size, NULL, cp, flags);
# ifdef	MAXCOMMSTR
				maxarg += len;
				if (iscomm > 0 && maxarg > MAXCOMMSTR) {
					free(buf);
					freevar(argv);
					errno = E2BIG;
					return(-1);
				}
# endif
				j += len;
			}
		}
		buf[j++] = '\0';
		free(argv[n]);
		argv[n] = strdup2(buf);
	}
	if (flags) lastptr = next;
	free(buf);
	return(ret);
#endif	/* !_NOEXTRAMACRO */
}

char *inputshellstr(prompt, ptr, def)
char *prompt;
int ptr;
char *def;
{
	char *cp, *tmp, *duppromptstr;
	int nl;

	duppromptstr = promptstr;
	if (prompt) promptstr = prompt;
	cp = inputstr(NULL, 0, ptr, def, 0);
	promptstr = duppromptstr;
	if (!cp) return((char *)-1);

	nl = stdiomode();
	tmp = evalhistory(cp);
	ttyiomode(nl);
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

	cp = inputshellstr(NULL, ptr, def);
	if (cp == (char *)-1) return(NULL);
	else if (!cp) {
		hideclock = 1;
		warning(0, HITKY_K);
		rewritefile(1);
		return(NULL);
	}

#ifndef	_NOORIGSHELL
	l = -1;
	len = strlen(cp);
	buf = malloc2(len + 1);
	strcpy(buf, cp);
	trp = stree = newstree(NULL);
	for (;;) {
		if (cp) {
			trp = analyze(cp, trp, 1);
			free(cp);
			if (!trp || !(trp -> cont)) break;
		}

		lcmdline = -1;
		hideclock = 1;
		cp = inputshellstr(promptstr2, -1, NULL);
		if (cp == (char *)-1) break;
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
	if (l >= 0) rewritefile(1);
#endif	/* !_NOORIGSHELL */

	return(cp);
}

static char *NEAR addoption(command, arg, stp, ignorelist)
char *command, *arg;
macrostat *stp;
int ignorelist;
{
	char *cp, *line;
	ALLOC_T size;
	int i, j, nb, nm, p, flags, quote;

	nb = stp -> needburst;
	nm = stp -> needmark;
	p = -1;
	line = c_realloc(NULL, 0, &size);
	quote = '\0';
	for (i = j = 0; command[i]; i++) {
		if (i >= stp -> addopt && p < 0) p = j;
		flags = 0;

		line = c_realloc(line, j + 1, &size);
		if (command[i] == quote) quote = '\0';
#ifdef	CODEEUC
		else if (isekana(command, i)) line[j++] = command[i++];
#endif
		else if (iskanji1(command, i)) line[j++] = command[i++];
		else if (quote == '\'');
		else if (ismeta(command, i, quote)) line[j++] = command[i++];
		else if (quote);
		else if (command[i] == '\'' || command[i] == '"')
			quote = command[i];
		else if (command[i] == '%') line[j++] = command[i];
		else if (isneedburst(command, i)) {
			if (nb > 0) {
				nb--;
				flags = command[++i];
			}
		}
		else if (isneedmark(command, i)) {
			if (nm > 0) {
				nm--;
				flags = command[++i];
			}
		}

		if (!flags) {
			line[j++] = command[i];
			continue;
		}

		line = c_realloc(line, j + 4, &size);
		line[j++] = '%';
		if (flags & F_NOEXT) line[j++] = 'X';
#if	MSDOS
		if (flags & F_TOSFN) line[j++] = 'S';
#endif
		if (flags & F_MARK) line[j++] = 'M';
		else if (flags & F_BURST) {
			line[j++] = 'T';
			if (flags & F_REMAIN) line[j++] = 'A';
		}
	}
	line[j] = '\0';
	free(command);

	if (p < 0) p = strlen(line);
	if (!(i = isttyiomode)) ttyiomode(1);
	cp = inputstr("", 0, p, line, 0);
	if (!i) stdiomode();
	free(line);
	if (!cp) {
		if (!i) {
			fputc('\n', stdout);
			fflush(stdout);
		}
		return(NULL);
	}
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
	return(command);
}

#ifdef	_NOORIGSHELL
static int NEAR system3(command, noconf, ignorelist)
char *command;
int noconf, ignorelist;
{
	char *cp, *tmp;
	int n, ret;

	while (*command == ' ' || *command == '\t') command++;
	if ((cp = evalalias(command))) command = cp;

	n = sigvecset(0);
	if ((ret = execpseudoshell(command, 1, ignorelist)) < 0) {
		tmp = evalcomstr(command, CMDLINE_DELIM);
		ret = system2(tmp, noconf);
		free(tmp);
	}
	sigvecset(n);
	if (cp) free(cp);
	return(ret);
}
#endif	/* _NOORIGSHELL */

int isinternalcomm(command)
char *command;
{
#ifndef	_NOORIGSHELL
	syntaxtree *stree;
	char **subst;
	int i, *len;
#endif
	char **argv;
	int ret;

	ret = 1;
#ifdef	_NOORIGSHELL
	if (getargs(command, &argv) > 0 && checkinternal(argv[0]) < 0) ret = 0;
	freevar(argv);
#else	/* !_NOORIGSHELL */
	stree = newstree(NULL);
	if (analyze(command, stree, 1)) {
		if ((argv = getsimpleargv(stree))) {
			i = (stree -> comm) -> argc;
			(stree -> comm) -> argc =
				getsubst(i, argv, &subst, &len);
			stripquote(argv[0], 1);
			if (argv[0] && checkinternal(argv[0]) < 0) ret = 0;
			freevar(subst);
			free(len);
		}
	}
	freestree(stree);
	free(stree);
#endif	/* !_NOORIGSHELL */

	return(ret);
}

int execmacro(command, arg, noconf, argset, ignorelist)
char *command, *arg;
int noconf, argset, ignorelist;
{
#ifdef	_NOEXTRAMACRO
	char *buf;
	int r, status;
#endif
	static char *duparg = NULL;
	macrostat st;
	char *tmp;
	int i, haslist, ret;

	if (arg) duparg = arg;
	haslist = (filelist && !ignorelist);
	ret = 0;
	internal_status = -2;
	if (!haslist) n_args = 0;
	else {
		for (i = 0; i < maxfile; i++) {
			if (ismark(&(filelist[i])))
				filelist[i].tmpflags |= F_ISARG;
			else filelist[i].tmpflags &= ~F_ISARG;
		}
		n_args = mark;
	}

	st.flags = (argset || isinternalcomm(command)) ? F_ARGSET : 0;
	if (noconf < 0) st.flags |= F_ISARCH;

	if (!(tmp = evalcommand(command, duparg, &st, ignorelist))) {
		if (arg) duparg = NULL;
		return(-1);
	}
	if (noconf >= 0 && (st.flags & F_NOCONFIRM)) noconf = 1 - noconf;
	st.flags |= F_ARGSET;
	while (st.addopt >= 0 && noconf >= 0 && argset <= 0)
		if (!(tmp = addoption(tmp, duparg, &st, ignorelist))) {
			if (arg) duparg = NULL;
			return(-1);
		}

#ifdef	_NOEXTRAMACRO
	status = internal_status;
	if (!haslist || !st.needmark) for (;;) {
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
		while (st.addopt >= 0 && noconf >= 0 && argset <= 0)
			if (!(tmp = addoption(tmp, duparg, &st, ignorelist)))
				break;
	}
	else if (n_args <= 0) {
		buf = insertarg(tmp, filelist[filepos].name, st.needmark);
		if (buf) {
			ret = system3(buf, noconf, ignorelist);
			free(buf);
			status = internal_status;
		}
	}
	else for (i = 0; i < maxfile; i++) if (isarg(&(filelist[i]))) {
		buf = insertarg(tmp, filelist[i].name, st.needmark);
		if (buf) {
			r = system3(buf, noconf, ignorelist);
			free(buf);
			if (r > ret && (ret = r) >= 127) break;
			if (internal_status < -1) status = 4;
			else if (internal_status > status)
				status = internal_status;
		}
	}
	internal_status = status;
#else	/* !_NOEXTRAMACRO */
	ret = system3(tmp, noconf, ignorelist);
	if (internal_status < -1) internal_status = 4;
#endif	/* !_NOEXTRAMACRO */

	if (tmp) free(tmp);
	if (haslist && internal_status < -1) {
		for (i = 0; i < maxfile; i++)
			filelist[i].tmpflags &= ~(F_ISARG | F_ISMRK);
		mark = marksize = 0;
	}
	if (arg) duparg = NULL;
	return(ret);
}

FILE *popenmacro(command, arg, argset)
char *command, *arg;
int argset;
{
	macrostat st;
	FILE *fp;
	char *tmp;

	internal_status = -2;
	st.flags = (argset || isinternalcomm(command)) ? F_ARGSET : 0;

	if (!(tmp = evalcommand(command, arg, &st, 1))) return(NULL);
	fp = popen2(tmp, "r");
	free(tmp);
	return(fp);
}

#ifdef	_NOORIGSHELL
static char *NEAR evalargs(command, argc, argv)
char *command;
int argc;
char *argv[];
{
	char *cp, *line;
	ALLOC_T size;
	int i, j, n, len;

	i = j = 0;
	line = c_realloc(NULL, 0, &size);
	while ((cp = strtkchr(&(command[i]), '$', 1))) {
		len = cp - &(command[i]);
		line = c_realloc(line, j + len + 2, &size);
		memcpy(&(line[j]), &(command[i]), len);
		i += len + 1;
		j += len;
		if (!isdigit(command[i])) {
			line[j++] = '$';
			line[j++] = command[i++];
		}
		else if ((n = command[i++] - '0') < argc) {
			len = strlen(argv[n]);
			line = c_realloc(line, j + len, &size);
			memcpy(&(line[j]), argv[n], len);
			j += len;
		}
	}
	len = strlen(&(command[i]));
	line = c_realloc(line, j + len, &size);
	memcpy(&(line[j]), &(command[i]), len);
	j += len;
	line[j] = '\0';

	return(line);
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
	strcpy(strcpy2(cp, aliaslist[i].comm), &(command[len]));
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

	if (histno[n]++ >= MAXHISTNO) histno[n] = 0;

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

char *removehist(n)
int n;
{
	char *tmp;
	int i, size;

	size = histsize[n];
	if (!history[n] || size > histbufsize[n] || size <= 0) return(NULL);
	if (--histno[n] < 0) histno[n] = MAXHISTNO;
	tmp = history[n][0];
	for (i = 0; i < size; i++) history[n][i] = history[n][i + 1];
	history[n][size--] = NULL;
	return(tmp);
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
		if (histno[n]++ >= MAXHISTNO) histno[n] = 0;
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
	char *cp, *eol;
	int i, size;

	if (!history[n] || !history[n][0]) return(-1);
	cp = evalpath(strdup2(file), 1);
	fp = fopen(cp, "w");
	free(cp);
	if (!fp) return(-1);

	size = (savehist > histsize[n]) ? histsize[n] : savehist;
	for (i = size - 1; i >= 0; i--) if (history[n][i] && *history[n][i]) {
		for (cp = history[n][i]; (eol = strchr(cp, '\n')); cp = eol) {
			fwrite(cp, sizeof(char), eol++ - cp, fp);
			fputc('\0', fp);
		}
		fputs(cp, fp);
		fputc('\n', fp);
	}
	fclose(fp);

	return(0);
}

int parsehist(str, ptrp)
char *str;
int *ptrp;
{
	char *cp;
	long no;
	int i, n, ptr, size;

	ptr = (ptrp) ? *ptrp : 0;
	size = histsize[0];
	if (str[ptr] == '!') n = 0;
	else if (str[ptr] == '=' || strchr(IFS_SET, str[ptr])) n = ptr = -1;
	else if ((cp = evalnumeric(&(str[ptr]), &no, -1))) {
		if (!no) n = -1;
		else if (no < 0) {
			if (-1L - no > (long)MAXHISTNO) n = -1;
			else n = -1L - no;
		}
		else if (no - 1L > (long)MAXHISTNO) n = -1;
		else if (no <= (long)(histno[0])) n = histno[0] - no;
		else n = MAXHISTNO - (no - histno[0]) + 1;
		ptr = (cp - str) - 1;
	}
	else {
		for (i = 0; str[ptr + i]; i++) {
			if (strchr(CMDLINE_DELIM, str[ptr + i])) break;
			if (iskanji1(str, ptr + i)) i++;
		}
		if (!i) n = ptr = -1;
		else {
			for (n = 0; n < size; n++) {
				if (!history[0][n]) break;
				cp = skipspace(history[0][n]);
				if (!strnpathcmp(&(str[ptr]), cp, i)) break;
			}
			ptr += i - 1;
		}
	}

	if (ptrp) *ptrp = ptr;
	if (n < 0 || n >= size) return(-1);
	if (!ptrp) {
		while (!history[0][n]) if (--n < 0) return(-1);
	}
	return((int)n);
}

char *evalhistory(command)
char *command;
{
	char *cp;
	ALLOC_T size;
	int i, j, n, len, quote, hit;

	hit = 0;
	cp = c_realloc(NULL, 0, &size);
	for (i = j = 0, quote = '\0'; command[i]; i++) {
		cp = c_realloc(cp, j + 1, &size);
		if (command[i] == quote) quote = '\0';
#ifdef	CODEEUC
		else if (isekana(command, i)) cp[j++] = command[i++];
#endif
		else if (iskanji1(command, i)) cp[j++] = command[i++];
		else if (quote);
		else if (ismeta(command, i, quote)) cp[j++] = command[i++];
		else if (command[i] == '\'') quote = command[i];
		else if (command[i] == '!') {
			len = i++;
			if ((n = parsehist(command, &i)) < 0) {
				if (i < 0) {
					cp[j++] = '!';
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
			len = strlen(history[0][n]);
			cp = c_realloc(cp, j + len + 1, &size);
			strncpy(&cp[j], history[0][n], len);
			j += len;
			continue;
		}
		cp[j++] = command[i];
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
