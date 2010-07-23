/*
 *	shell.c
 *
 *	shell command module
 */

#include "fd.h"
#include "realpath.h"
#include "parse.h"
#include "kconv.h"
#include "func.h"
#include "kanji.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	DEP_PTY
#include "termemu.h"
#endif

typedef struct _localetable {
	CONST char *env;
	CONST char *val;
	char *org;
} localetable;

#if	0
#define	MACROMETA		((char)-1)
#endif
#if	defined (USESYSCONF) && defined (_SC_ARG_MAX)
#define	MAXARGNUM		sysconf(_SC_ARG_MAX)
#else
# ifdef	ARG_MAX
# define	MAXARGNUM	ARG_MAX
# else
#  ifdef	NCARGS
#  define	MAXARGNUM	NCARGS
#  endif
# endif
#endif

#ifdef	DEP_DOSLFN
extern char *shortname __P_((CONST char *, char *));
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

static int NEAR checksc __P_((CONST char *, int, CONST char *));
#ifdef	DEP_KCONV
static int NEAR extconv __P_((char **, int, int, ALLOC_T *, int));
#endif
static int NEAR isneedargs __P_((CONST char *, int, int *));
static int NEAR setarg __P_((char **, int, ALLOC_T *,
		CONST char *, CONST char *, int));
static int NEAR flag2str __P_((char *, int, int));
static int NEAR skipquote __P_((CONST char *, int *));
#if	!defined (MACROMETA) || !defined (_NOEXTRAMACRO)
static char *NEAR _restorearg __P_((CONST char *));
static char *NEAR _demacroarg __P_((char *));
#endif
#ifdef	_NOEXTRAMACRO
static int NEAR setargs __P_((char **, int, ALLOC_T *,
		int, int, int, macrostat *, int));
static char *NEAR insertarg __P_((CONST char *, CONST char *, int));
#else
static int NEAR _replaceargs __P_((int *, char ***, char *CONST *, int));
#endif
static char *NEAR addoption __P_((char *, CONST char *, macrostat *));
#ifndef	DEP_ORIGSHELL
static int NEAR system3 __P_((CONST char *, int));
static char *NEAR evalargs __P_((CONST char *, int, char *CONST *));
static char *NEAR evalalias __P_((CONST char *));
#else
#define	system3			system2
#endif

#ifdef	DEP_ORIGSHELL
CONST char *promptstr2 = NULL;
#else
aliastable aliaslist[MAXALIASTABLE];
int maxalias = 0;
userfunctable userfunclist[MAXFUNCTABLE];
int maxuserfunc = 0;
#endif
CONST char *promptstr = NULL;
char **history[2] = {NULL, NULL};
char *histfile[2] = {NULL, NULL};
short histsize[2] = {0, 0};
short histno[2] = {0, 0};
short savehist[2] = {0, 0};
int n_args = 0;

static short histbufsize[2] = {0, 0};
static localetable localelist[] = {
	{"LC_COLLATE", "C", NULL},
	{"LC_CTYPE", "", NULL},
	{"LC_MESSAGES", "C", NULL},
	{"LC_MONETARY", "C", NULL},
	{"LC_NUMERIC", "C", NULL},
	{"LC_TIME", "C", NULL},
	{"LC_ALL", NULL, NULL},
	{"LANG", "C", NULL},
};
#define	LOCALELISTSIZ		arraysize(localelist)


static int NEAR checksc(buf, ptr, arg)
CONST char *buf;
int ptr;
CONST char *arg;
{
	if (!arg || !*arg) /*EMPTY*/;
	else if (*arg == _SC_ || isdotdir(arg)) /*EMPTY*/;
#if	MSDOS
	else if (_dospath(arg)) /*EMPTY*/;
#endif
	else return(ptr);

	if (buf) {
		if (ptr < 2) return(ptr);
		if (buf[ptr - 1] != _SC_ || buf[ptr - 2] != '.') return(ptr);
		if (ptr > 2 && !Xstrchr(IFS_SET, buf[ptr - 3])) return(ptr);
	}

	return(ptr - 2);
}

#ifdef	DEP_KCONV
static int NEAR extconv(bufp, ptr, eol, sizep, code)
char **bufp;
int ptr, eol;
ALLOC_T *sizep;
int code;
{
# ifdef	DEP_FILECONV
	char rpath[MAXPATHLEN];
# endif
	char *cp;
	int len;

# ifdef	DEP_FILECONV
	if (code < 0) {
		cp = _evalpath(&((*bufp)[ptr]), &((*bufp)[eol]),
			EA_NOEVALQ | EA_NOUNIQDELIM);
		Xrealpath(cp, rpath, RLP_READLINK);
		Xfree(cp);
		code = getkcode(rpath);
	}
# endif
	(*bufp)[eol] = '\0';
	cp = newkanjiconv(&((*bufp)[ptr]), DEFCODE, code, L_FNAME);
	if (cp == &((*bufp)[ptr])) return(eol);
	len = strlen(cp);

	*bufp = c_realloc(*bufp, ptr + len + 1, sizep);
	memcpy(&((*bufp)[ptr]), cp, len);
	Xfree(cp);

	return(ptr + len);
}
#endif	/* DEP_KCONV */

static int NEAR isneedargs(s, n, flagsp)
CONST char *s;
int n, *flagsp;
{
	int i;

	i = n;
	*flagsp = 0;
#ifdef	MACROMETA
	if (s[i++] != MACROMETA) return(0);
	*flagsp = (u_char)(s[i++]);
#else	/* !MACROMETA */
	if (s[i++] != '%') return(0);
	if (s[i] == '%') return(2);
	if (s[i] == 'X') {
		i++;
		*flagsp |= F_NOEXT;
	}
# if	MSDOS
	if (s[i] == 'S') {
		i++;
		*flagsp |= F_TOSFN;
	}
# endif
	if (s[i] == 'M') {
		i++;
		*flagsp |= F_MARK;
	}
	else if (s[i] == 'T') {
		i++;
		*flagsp |= F_BURST;
		if (s[i] == 'A') {
			i++;
			*flagsp |= F_REMAIN;
		}
	}
	else return(0);
#endif	/* !MACROMETA */

	return(i - n);
}

static int NEAR setarg(bufp, ptr, sizep, dir, arg, flags)
char **bufp;
int ptr;
ALLOC_T *sizep;
CONST char *dir, *arg;
int flags;
{
	char *cp, *new, path[MAXPATHLEN], conv[MAXPATHLEN];
	int len, optr;

	if (!arg || !*arg) return(checksc(*bufp, ptr, NULL) - ptr);
	if (!dir) {
#ifdef	DEP_DOSLFN
		if ((flags & F_TOSFN) && shortname(arg, path) == path)
			arg = path;
		else
#endif
		arg = convput(conv, arg, 1, 0, NULL, NULL);
	}
	else {
		strcatdelim2(path, dir, arg);
		arg = path;
#if	defined (BSPATHDELIM) && !defined (_NOARCHIVE)
		if (flags & F_ISARCH) for (len = 0; path[len]; len++) {
			if (iskanji1(path, len)) len++;
			if (path[len] == _SC_) path[len] = '/';
		}
#endif
#ifdef	DEP_FILECONV
		arg = kanjiconv2(conv, arg,
			MAXPATHLEN - 1, DEFCODE, fnamekcode, L_FNAME);
#endif
	}

	optr = ptr;
	ptr = checksc(*bufp, ptr, arg);
	arg = new = killmeta(arg);

	if ((flags & F_NOEXT) && (cp = Xstrrchr(arg, '.')) && cp != arg)
		len = cp - arg;
	else len = strlen(arg);

	*bufp = c_realloc(*bufp, ptr + len + 1, sizep);
	memcpy(&((*bufp)[ptr]), arg, len);
	Xfree(new);

	return(len + ptr - optr);
}

static int NEAR flag2str(s, ptr, flags)
char *s;
int ptr, flags;
{
	s[ptr++] = '%';
	if (flags & F_NOEXT) s[ptr++] = 'X';
#if	MSDOS
	if (flags & F_TOSFN) s[ptr++] = 'S';
#endif
	if (flags & F_MARK) s[ptr++] = 'M';
	else if (flags & F_BURST) {
		s[ptr++] = 'T';
		if (flags & F_REMAIN) s[ptr++] = 'A';
	}
	else s[ptr++] = '%';

	return(ptr);
}

static int NEAR skipquote(s, ptrp)
CONST char *s;
int *ptrp;
{
	static int q = '\0';
	int pc;

	if (!*ptrp) q = '\0';
	pc = parsechar(&(s[*ptrp]), -1, '\0', 0, &q, NULL);
	if (pc == PC_NORMAL) return(0);
	if (pc == PC_WCHAR || pc == PC_ESCAPE) {
		(*ptrp)++;
		return(2);
	}

	return(1);
}

#if	!defined (MACROMETA) || !defined (_NOEXTRAMACRO)
static char *NEAR _restorearg(s)
CONST char *s;
{
	char *buf;
	int i, j, m, flags;

	if (!s) return(NULL);
	i = strlen(s);
# ifdef	MACROMETA
	buf = Xmalloc(i * 3 + 1);	/* MACROMETA+flags -> %XSTA (x2.5) */
# else
	buf = Xmalloc(i + 1);
# endif
	flags = 0;
	for (i = j = 0; s[i]; i++) {
# ifdef	MACROMETA
		if (!(m = isneedargs(s, i, &flags))) buf[j++] = s[i];
		else {
			i += m - 1;
			j = flag2str(buf, j, flags);
		}
# else
		if ((m = skipquote(s, &i)) == 2) buf[j++] = s[i - 1];
		else if (!m && s[i] == '%' && s[i + 1] == '%') {
			i++;
			flags++;
		}
		buf[j++] = s[i];
# endif
	}
	if (!flags) {
		Xfree(buf);
		return((char *)s);
	}
	buf[j] = '\0';

	return(buf);
}

static char *NEAR _demacroarg(s)
char *s;
{
	char *cp;

	if ((cp = _restorearg(s)) != s) Xfree(s);

	return(cp);
}
#endif	/* !MACROMETA || !_NOEXTRAMACRO */

#ifdef	_NOEXTRAMACRO
/*ARGSUSED*/
static int NEAR setargs(bufp, ptr, sizep, blen, mlen, eol, stp, flags)
char **bufp;
int ptr;
ALLOC_T *sizep;
int blen, mlen, eol;
macrostat *stp;
int flags;
{
# ifdef	MAXCOMMSTR
	int m, f, optr;
# endif
	char *cp, *s, *dir, *tmp;
	int i, n, len, flen, rlen;

# ifdef	_NOARCHIVE
	dir = NULL;
# else
	dir = (archivefile) ? archivedir : NULL;
# endif

	len = eol - ptr;
	tmp = Xstrndup(&((*bufp)[ptr]), len);
	tmp[len] = '\0';
	cp = &(tmp[blen + mlen]);
	for (s = cp; *s; s++) {
		if (Xstrchr(CMDLINE_DELIM, *s)) break;
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
		if (ptr + flen > MAXCOMMSTR) {
			Xfree(tmp);
			return(-1);
		}
# endif
		*bufp = c_realloc(*bufp, ptr + flen + 1, sizep);
		memcpy(&((*bufp)[ptr]), cp, flen);
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
			memcpy(&((*bufp)[ptr]), tmp, blen);
			ptr += blen;
		}
		len = setarg(bufp, ptr, sizep, dir, filelist[i].name, flags);
		ptr += len;
# ifdef	MAXCOMMSTR
		if (ptr + flen > MAXCOMMSTR) {
			if (!n) {
				Xfree(tmp);
				return(-1);
			}
			ptr = optr;
			break;
		}
# endif
		*bufp = c_realloc(*bufp, ptr + flen + 1, sizep);
		memcpy(&((*bufp)[ptr]), cp, flen);
		ptr += flen;
		filelist[i].tmpflags &= ~F_ISARG;
		n_args--;
		n++;
	}

# ifdef	MAXCOMMSTR
	if (ptr + rlen > MAXCOMMSTR) {
		cp = &(s[MAXCOMMSTR - ptr]);
		len = rlen - (MAXCOMMSTR - ptr);
		for (i = 0; i < len; i++) {
#  ifndef	MACROMETA
			if (skipquote(cp, &i)) continue;
#  endif
			if ((m = isneedargs(cp, i, &f))) {
				if (f & F_MARK) (stp -> needmark)--;
				i += m - 1;
			}
		}
		rlen = MAXCOMMSTR - ptr;
	}
# endif	/* MAXCOMMSTR */
	*bufp = c_realloc(*bufp, ptr + rlen + 1, sizep);
	memcpy(&((*bufp)[ptr]), s, rlen);
	ptr += rlen;
	Xfree(tmp);

	return(ptr - eol);
}

static char *NEAR insertarg(format, arg, needmark)
CONST char *format, *arg;
int needmark;
{
# ifdef	DEP_DOSLFN
	CONST char *org;
	char alias[MAXPATHLEN];
# endif
	CONST char *src, *ins;
	char *cp, *new, *buf, conv[MAXPATHLEN];
	ALLOC_T size;
	int i, j, m, len, flags;

# ifdef	DEP_DOSLFN
	org = arg;
	alias[0] = '\0';
# endif
	arg = convput(conv, arg, 1, 0, NULL, NULL);
# ifdef	FAKEUNINIT
	m = 0;	/* fake for -Wuninitialized */
# endif
	for (j = 0; format[j]; j++) {
# ifndef	MACROMETA
		if (skipquote(format, &j)) continue;
# endif
		if ((m = isneedargs(format, j, &flags))) {
			if (flags & F_MARK) break;
			j += m - 1;
		}
	}
	if (!(format[j])) return(Xstrdup(format));
	buf = c_realloc(NULL, 0, &size);
# ifdef	MAXCOMMSTR
	if (j > MAXCOMMSTR) i = j = 0;
	else
# endif
	{
		buf = c_realloc(buf, j + 1, &size);
		memcpy(buf, format, j);
		src = &(format[j + m]);
		for (i = 0; i < needmark; i++) {
			ins = arg;
# ifdef	DEP_DOSLFN
			if (flags & F_TOSFN) {
				if (!alias[0]) {
					if (shortname(org, alias) == alias)
						org = alias;
					else Xstrcpy(alias, org);
				}
				ins = alias;
			}
# endif
			j = checksc(buf, j, ins);
			ins = new = killmeta(ins);

			if ((flags & F_NOEXT) && (cp = Xstrrchr(ins, '.')))
				len = cp - ins;
			else len = strlen(ins);
# ifdef	MAXCOMMSTR
			if (j + len > MAXCOMMSTR) break;
# endif
			buf = c_realloc(buf, j + len + 1, &size);
			Xstrncpy(&(buf[j]), ins, len);
			Xfree(new);
			j += len;
			for (len = 0; src[len]; len++) {
# ifndef	MACROMETA
				if (skipquote(src, &len)) continue;
# endif
				if ((m = isneedargs(src, len, &flags))) {
					if (flags & F_MARK) break;
					len += m - 1;
				}
			}
# ifdef	MAXCOMMSTR
			if (j + len > MAXCOMMSTR) break;
# endif
			buf = c_realloc(buf, j + len + 1, &size);
			memcpy(&(buf[j]), src, len);
			j += len;
			src += len + m;
		}
	}
	buf[j] = '\0';
	if (i < needmark) {
		Xfree(buf);
		return(NULL);
	}

	return(buf);
}
#endif	/* _NOEXTRAMACRO */

char *evalcommand(command, arg, stp)
CONST char *command, *arg;
macrostat *stp;
{
#ifdef	DEP_KCONV
	int code, cnvcode, defcode, cnvptr;
#endif
#ifdef	_NOEXTRAMACRO
	int m, f;
#endif
	macrostat st;
	char *cp, *line, *new, conv[MAXPATHLEN];
	ALLOC_T size;
	int i, j, c, pc, len, setflag, flags;

	if (stp) flags = stp -> flags;
	else {
		stp = &st;
		flags = 0;
	}
	stp -> addopt = -1;
	stp -> needmark = stp -> needburst = 0;
#ifdef	DEP_KCONV
# ifdef	DEP_FILECONV
	cnvcode = defcode = (flags & F_NOKANJICONV) ? NOCNV : defaultkcode;
# else
	cnvcode = defcode = NOCNV;
# endif
	cnvptr = 0;
#endif	/* DEP_KCONV */

	line = c_realloc(NULL, 0, &size);
	for (i = j = 0; command[i]; i++) {
		line = c_realloc(line, j + 1, &size);

		c = '\0';
		if ((pc = skipquote(command, &i)) == 1) /*EMPTY*/;
		else if (pc) line[j++] = command[i - 1];
		else if (command[i] == '%') c = command[++i];
		else if (flags & (F_NOEXT | F_TOSFN)) c = command[i];

		if (!c) {
			line[j++] = command[i];
			continue;
		}

#ifdef	DEP_KCONV
		if (cnvcode != NOCNV)
			j = extconv(&line, cnvptr, j, &size, cnvcode);
#endif
		len = setflag = 0;
		switch (c) {
			case 'P':
				len = setarg(&line, j, &size,
					NULL, fullpath, flags);
				break;
#if	MSDOS
			case 'S':
				flags |= F_TOSFN;
				c = command[i + 1];
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
				c = command[i + 1];
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
				c = command[i + 1];
				if (c == 'A') {
					flags |= F_REMAIN;
					i++;
				}
				if (!(flags & F_IGNORELIST)) {
#ifdef	MACROMETA
					line[j++] = MACROMETA;
					line[j++] = flags | F_BURST;
#else
					j = flag2str(line, j, flags | F_BURST);
#endif
					(stp -> needburst)++;
					flags |= F_ARGSET;
				}
				break;
			case 'M':
				if (!(flags & F_IGNORELIST)) {
#ifdef	MACROMETA
					line[j++] = MACROMETA;
					line[j++] = flags | F_MARK;
#else
					j = flag2str(line, j, flags | F_MARK);
#endif
					(stp -> needmark)++;
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
				if (!(flags & F_ISARCH)) flags ^= F_NOCONFIRM;
				break;
#ifdef	DEP_KCONV
			case 'J':
				c = command[i + 1];
				if (c == 'S') code = SJIS;
				else if (c == 'E') code = EUC;
# if	FD < 2
				else if (c == 'J') code = JIS7;
# else	/* FD >= 2 */
				else if (c == '7') code = JIS7;
				else if (c == '8') code = JIS8;
				else if (c == 'J') code = JUNET;
				else if (c == 'H') code = HEX;
				else if (c == 'C') code = CAP;
				else if (c == 'U') code = UTF8;
				else if (c == 'M') code = M_UTF8;
				else if (c == 'I') code = I_UTF8;
#  ifdef	DEP_FILECONV
				else if (c == 'A') code = -1;
#  endif
# endif	/* FD >= 2 */
				else {
					line[j++] = command[i];
					break;
				}

				i++;
				cnvcode = (cnvcode == code) ? defcode : code;
				break;
#endif	/* DEP_KCONV */
			case '%':
#ifndef	MACROMETA
				line[j++] = '%';
#endif
				line[j++] = c;
				break;
			default:
				line[j++] = '%';
				line[j++] = c;
				break;
		}
		if (!setflag) flags &= ~(F_NOEXT | F_TOSFN);
		j += len;
#ifdef	DEP_KCONV
		cnvptr = j;
#endif
	}
#if	defined (_NOEXTRAMACRO) && defined (MAXCOMMSTR)
	if (j > MAXCOMMSTR) {
		if (isttyiomode) warning(E2BIG, command);
		else {
			errno = E2BIG;
			perror2(command);
		}
		Xfree(line);
		return(NULL);
	}
#endif

#ifdef	DEP_KCONV
	if (cnvcode != NOCNV) j = extconv(&line, cnvptr, j, &size, cnvcode);
#endif

#ifdef	_NOEXTRAMACRO
	if (stp -> needburst) for (i = c = 0; i < j; i++) {
# ifndef	MACROMETA
		if (skipquote(line, &i)) continue;
# endif
		if (Xstrchr(CMDLINE_DELIM, line[i])) c = -1;
		else if (c < 0) c = i;
		if (!(m = isneedargs(line, i, &f))) continue;
		else if (!(f & F_BURST)) {
			i += m - 1;
			continue;
		}

		if (stp -> needburst <= 0) {
			memmove(&(line[i]), &(line[i + m]), j - i - m);
			j -= m;
			i--;
			continue;
		}

		(stp -> needburst)--;
		len = setargs(&line, c, &size, i - c, m, j, stp, f);
		if (len < 0) {
			for (i += m; i < j; i++) {
# ifndef	MACROMETA
				if (skipquote(line, &i)) continue;
# endif
				if ((m = isneedargs(line, i, &f))) {
					if (f & F_MARK) (stp -> needmark)--;
					i += m - 1;
				}
			}
			j = c;
			break;
		}
		j += len;
		i += len + 1;
	}
#endif	/* _NOEXTRAMACRO */

	if (!(flags & F_ARGSET) && arg && *arg) {
		arg = convput(conv, arg, 1, 1, NULL, NULL);
		if (checksc(NULL, 0, arg) < 0) cp = NULL;
		else {
			cp = Xmalloc(strlen(arg) + 2 + 1);
			cp[0] = '.';
			cp[1] = _SC_;
			Xstrcpy(&(cp[2]), arg);
			arg = cp;
		}
		arg = new = killmeta(arg);
		Xfree(cp);

		len = strlen(arg);
#if	defined (_NOEXTRAMACRO) && defined (MAXCOMMSTR)
		if (j + len + 1 > MAXCOMMSTR) /*EMPTY*/;
		else
#endif
		{
			line = c_realloc(line, j + len + 1, &size);
			line[j++] = ' ';
			Xstrncpy(&(line[j]), arg, len);
			j += len;
		}
		Xfree(new);
	}
	if (!(flags & F_IGNORELIST)
	&& !(stp -> needburst) && !(stp -> needmark)) {
		for (i = 0; i < maxfile; i++) filelist[i].tmpflags &= ~F_ISARG;
		n_args = 0;
	}
	if ((flags & F_REMAIN) && !n_args) flags &= ~F_REMAIN;
	stp -> flags = flags;

	cp = Xstrndup(line, j);
	Xfree(line);

	return(cp);
}

#ifdef	_NOEXTRAMACRO
/*ARGSUSED*/
int replaceargs(argcp, argvp, env, iscomm)
int *argcp;
char ***argvp, *CONST *env;
int iscomm;
{
	return(0);
}

/*ARGSUSED*/
int replacearg(argp)
char **argp;
{
	return(0);
}

#else	/* !_NOEXTRAMACRO */

static int NEAR _replaceargs(argcp, argvp, env, iscomm)
int *argcp;
char ***argvp, *CONST *env;
int iscomm;
{
	static int lastptr = 0;
	char *cp, *arg, *buf, *dir, **argv, **argv2;
	ALLOC_T size;
	int i, j, n, m, argc, argc2, len, maxarg, min, next, ret, hit, flags;

	if (!argcp || !argvp) {
		lastptr = 0;
		return(-1);
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

# ifdef	MAXCOMMSTR
	maxarg = 0;
# endif
	for (n = i = 0; n < argc; n++) {
# ifdef	MAXCOMMSTR
		if (n) maxarg++;
# endif
		for (len = 0; argv[n][len]; len++) {
# ifndef	MACROMETA
			if ((m = skipquote(argv[n], &len))) {
#  ifdef	MAXCOMMSTR
				maxarg += m;
#  endif
				continue;
			}
# endif	/* !MACROMETA */
			if ((m = isneedargs(argv[n], len, &flags))) {
				if (flags) i++;
				len += m - 1;
			}
# ifdef	MAXCOMMSTR
			else maxarg++;
# endif
		}
	}
	if (!i) return(ret);

# ifdef	MAXCOMMSTR
	if (iscomm > 0 && maxarg > MAXCOMMSTR) return(seterrno(E2BIG));
# else	/* !MAXCOMMSTR */
#  ifdef	MAXARGNUM
	maxarg = MAXARGNUM - 2048;
	if (maxarg > 20 * 1024) maxarg = 20 * 1024;
	maxarg -= countvar(env);
	if (iscomm > 0 && argc > maxarg) return(seterrno(E2BIG));
#  endif
# endif	/* !MAXCOMMSTR */

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
		hit = 0;
		for (i = j = 0; argv[n][i]; i++) {
# ifndef	MACROMETA
			if ((m = skipquote(argv[n], &i))) {
				buf = c_realloc(buf, j + m - 1, &size);
				memcpy(&(buf[j]), &(argv[n][i - m + 1]), m);
				j += m;
				continue;
			}
# endif
			m = isneedargs(argv[n], i, &flags);
			if (iscomm < 0 && (flags & F_MARK)) flags |= F_BURST;

			if (!m) {
				buf = c_realloc(buf, j, &size);
				buf[j++] = argv[n][i];
			}
			else if (!(flags & F_BURST)) {
				buf = c_realloc(buf, j + m - 1, &size);
				memcpy(&(buf[j]), &(argv[n][i]), m);
				i += m - 1;
				j += m;
			}
			else {
				hit++;
				i += m - 1;
				cp = (iscomm < 0 || n) ? arg : NULL;

				len = setarg(&buf, j, &size, dir, cp, flags);
# ifdef	MAXCOMMSTR
				maxarg += len;
				if (iscomm > 0 && maxarg > MAXCOMMSTR) {
					Xfree(buf);
					freevar(argv2);
					return(seterrno(E2BIG));
				}
# endif
				j += len;
			}
		}
		buf[j++] = '\0';
		argv2 = (char **)Xrealloc(argv2, (argc2 + 2) * sizeof(char *));
		argv2[argc2++] = Xstrdup(buf);
		argv2[argc2] = NULL;
		if (min < 0 || !hit || (iscomm >= 0 && !n)) continue;

		while (next < maxfile) {
# if	!defined (MAXCOMMSTR) && defined (MAXARGNUM)
			if (iscomm > 0 && argc + (argc2 - n) + 1 > maxarg) {
				if (flags & F_REMAIN) ret++;
				break;
			}
# endif
			arg = filelist[next].name;
			for (i = j = 0; argv[n][i]; i++) {
# ifndef	MACROMETA
				if ((m = skipquote(argv[n], &i))) {
					buf = c_realloc(buf, j + m - 1, &size);
					memcpy(&(buf[j]),
						&(argv[n][i - m + 1]), m);
					j += m;
					continue;
				}
# endif
				m = isneedargs(argv[n], i, &flags);
				if (iscomm < 0 && (flags & F_MARK))
					flags |= F_BURST;

				if (!m) {
					buf = c_realloc(buf, j, &size);
					buf[j++] = argv[n][i];
				}
				else if (!(flags & F_BURST)) {
					buf = c_realloc(buf, j + m - 1, &size);
					memcpy(&(buf[j]), &(argv[n][i]), m);
					i += m - 1;
					j += m;
				}
				else {
					i += m - 1;

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
			argv2 = (char **)Xrealloc(argv2,
				(argc2 + 2) * sizeof(char *));
			argv2[argc2++] = Xstrdup(buf);
			argv2[argc2] = NULL;
			for (next++; next < maxfile; next++)
				if (isarg(&(filelist[next]))) break;
		}
	}
	if (ret) lastptr = next;
	freevar(argv);
	*argcp = argc = argc2;
	*argvp = argv = argv2;

	if (iscomm < 0) {
		Xfree(buf);
		return(ret);
	}

	min = -1;
	for (next = lastptr; next < maxfile; next++)
	if (isarg(&(filelist[next]))) {
		if (min < 0) min = next;
		else break;
	}
	arg = (min >= 0) ? filelist[min].name : filelist[filepos].name;

	hit = 0;
	for (n = 0; n < argc; n++) {
		for (i = j = 0; argv[n][i]; i++) {
# ifndef	MACROMETA
			if ((m = skipquote(argv[n], &i))) {
				buf = c_realloc(buf, j + m - 1, &size);
				memcpy(&(buf[j]), &(argv[n][i - m + 1]), m);
				j += m;
				continue;
			}
# endif
			if (!(m = isneedargs(argv[n], i, &flags))) {
				buf = c_realloc(buf, j, &size);
				buf[j++] = argv[n][i];
			}
			else if (!(flags & F_MARK)) {
				buf = c_realloc(buf, j + m - 1, &size);
				memcpy(&(buf[j]), &(argv[n][i]), m);
				i += m - 1;
				j += m;
			}
			else {
				hit++;
				i += m - 1;
				cp = (!env || n) ? arg : NULL;
				if (min >= 0 && next < maxfile) ret++;

				len = setarg(&buf, j, &size, NULL, cp, flags);
# ifdef	MAXCOMMSTR
				maxarg += len;
				if (iscomm > 0 && maxarg > MAXCOMMSTR) {
					Xfree(buf);
					freevar(argv);
					return(seterrno(E2BIG));
				}
# endif
				j += len;
			}
		}
		buf[j++] = '\0';
		Xfree(argv[n]);
		argv[n] = Xstrdup(buf);
	}
	if (hit) lastptr = next;
	Xfree(buf);

	return(ret);
}

int replaceargs(argcp, argvp, env, iscomm)
int *argcp;
char ***argvp, *CONST *env;
int iscomm;
{
# ifndef	MACROMETA
	int n, ret;
# endif

# ifdef	MACROMETA
	return(_replaceargs(argcp, argvp, env, iscomm));
# else
	if ((ret = _replaceargs(argcp, argvp, env, iscomm)) < 0) return(-1);
	for (n = 0; n < *argcp; n++) (*argvp)[n] = _demacroarg((*argvp)[n]);

	return(ret);
# endif
}

int replacearg(argp)
char **argp;
{
	char **argv;
	int n, argc;

	argv = (char **)Xmalloc(2 * sizeof(char *));
	argv[0] = *argp;
	argv[1] = NULL;
	argc = 1;
	n = replaceargs(&argc, &argv, NULL, 0);
	*argp = argv[0];
	Xfree(argv);

	return(n);
}
#endif	/* !_NOEXTRAMACRO */

char *restorearg(s)
CONST char *s;
{
#if	defined (MACROMETA) && !defined (_NOEXTRAMACRO)
	return(_restorearg(s));
#else
	return((char *)s);
#endif
}

/*ARGSUSED*/
VOID demacroarg(argp)
char **argp;
{
#if	defined (MACROMETA) && !defined (_NOEXTRAMACRO)
	*argp = _demacroarg(*argp);
#endif
}

char *inputshellstr(prompt, ptr, def)
CONST char *prompt;
int ptr;
CONST char *def;
{
	CONST char *duppromptstr;
	char *cp, *tmp;
	int wastty;

	duppromptstr = promptstr;
	if (prompt) promptstr = prompt;
	cp = inputstr(NULL, 0, ptr, def, HST_COMM);
	promptstr = duppromptstr;
	if (!cp) return(vnullstr);

	if ((wastty = isttyiomode)) Xstdiomode();
	tmp = evalhistory(cp);
	if (wastty) Xttyiomode(wastty - 1);
	if (!tmp) {
		Xfree(cp);
		return(NULL);
	}
	entryhist(tmp, HST_COMM);

	return(tmp);
}

char *inputshellloop(ptr, def)
int ptr;
CONST char *def;
{
#ifdef	DEP_ORIGSHELL
	syntaxtree *trp, *stree;
	char *buf;
	int l, len;
#endif
	char *cp;

	cp = inputshellstr(NULL, ptr, def);
	if (cp == vnullstr) return(NULL);
	else if (!cp) {
		hideclock = 1;
		warning(0, HITKY_K);
		rewritefile(1);
		return(NULL);
	}

#ifdef	DEP_ORIGSHELL
	l = -1;
	len = strlen(cp);
	buf = Xmalloc(len + 1);
	Xstrcpy(buf, cp);
	trp = stree = newstree(NULL);
	for (;;) {
		if (cp) {
			trp = analyze(cp, trp, 1);
			Xfree(cp);
			if (!trp || !(trp -> cont)) break;
		}

		lcmdline = -1;
		hideclock = 1;
		cp = inputshellstr(promptstr2, -1, NULL);
		if (cp == vnullstr) break;
		else if (!cp) continue;

		l = strlen(cp);
		buf = Xrealloc(buf, len + l + 1 + 1);
		buf[len++] = '\n';
		Xstrcpy(&(buf[len]), cp);
		len += l;
	}
	freestree(stree);
	Xfree(stree);
	cp = buf;
	if (l >= 0) rewritefile(1);
#endif	/* DEP_ORIGSHELL */

	return(cp);
}

static char *NEAR addoption(command, arg, stp)
char *command;
CONST char *arg;
macrostat *stp;
{
	char *cp;
	int p, wastty;
#ifdef	MACROMETA
	char *line;
	ALLOC_T size;
	int i, j, pc, m, nb, nm, flags;

	nb = stp -> needburst;
	nm = stp -> needmark;
	p = -1;
	line = c_realloc(NULL, 0, &size);
	for (i = j = 0; command[i]; i++) {
		if (i >= stp -> addopt && p < 0) p = j;
		m = flags = 0;

		line = c_realloc(line, j + 1, &size);
		if ((pc = skipquote(command, &i)) == 1);
		else if (pc) line[j++] = command[i - 1];
		else if (command[i] == '%') line[j++] = command[i];
		else if ((m = isneedargs(command, i, &flags))) {
			if (flags & F_BURST) {
				if (nb > 0) nb--;
				else flags = 0;
			}
			else if (flags & F_MARK) {
				if (nm > 0) nm--;
				else flags = 0;
			}
		}

		if (!flags) {
			if (!m) line[j++] = command[i];
			else {
				line = c_realloc(line, j + m - 1, &size);
				memcpy(&(line[j]), &(command[i]), m);
				i += m - 1;
				j += m;
			}
			continue;
		}

		i += m - 1;
		line = c_realloc(line, j + 4, &size);
		j = flag2str(line, j, flags);
	}
	line[j] = '\0';
	Xfree(command);

	if (p < 0) p = strlen(line);
	command = line;
#else	/* MACROMETA */
	p = stp -> addopt;
#endif	/* MACROMETA */

	if (!(wastty = isttyiomode)) Xttyiomode(1);
	cp = inputstr(nullstr, 0, p, command, HST_COMM);
	if (!wastty) Xstdiomode();
	Xfree(command);
	if (!cp) {
		if (!wastty) VOID_C fputnl(Xstdout);
		return(NULL);
	}
	if (!*cp) {
		Xfree(cp);
		return(NULL);
	}
	command = evalcommand(cp, arg, stp);
	Xfree(cp);
	if (!command) return(NULL);
	if (!*command) {
		Xfree(command);
		return(NULL);
	}

	return(command);
}

#ifndef	DEP_ORIGSHELL
static int NEAR system3(command, flags)
CONST char *command;
int flags;
{
	char *cp, *tmp;
	int n, ret;

	command = skipspace(command);
	if ((cp = evalalias(command))) command = cp;

	n = sigvecset(0);
	if ((ret = execpseudoshell(command, flags)) < 0) {
		tmp = evalcomstr(command, CMDLINE_DELIM);
		ret = system2(tmp, flags);
		Xfree(tmp);
	}
	sigvecset(n);
	Xfree(cp);

	return(ret);
}
#endif	/* !DEP_ORIGSHELL */

int isinternalcomm(command)
CONST char *command;
{
#ifdef	DEP_ORIGSHELL
	syntaxtree *stree;
	char **subst;
	int i, *len;
#endif
	char **argv;
	int ret;

	ret = 1;
#ifdef	DEP_ORIGSHELL
	stree = newstree(NULL);
	if (analyze(command, stree, 1)) {
		if ((argv = getsimpleargv(stree))) {
			i = (stree -> comm) -> argc;
			(stree -> comm) -> argc =
				getsubst(i, argv, &subst, &len);
			stripquote(argv[0], EA_STRIPQ);
			if (argv[0] && checkinternal(argv[0]) < 0) ret = 0;
			freevar(subst);
			Xfree(len);
		}
	}
	freestree(stree);
	Xfree(stree);
#else	/* !DEP_ORIGSHELL */
	if (getargs(command, &argv) > 0 && checkinternal(argv[0]) < 0) ret = 0;
	freevar(argv);
#endif	/* !DEP_ORIGSHELL */

	return(ret);
}

int execmacro(command, arg, flags)
CONST char *command, *arg;
int flags;
{
#ifdef	_NOEXTRAMACRO
	char *buf;
	int r, status;
#endif
	static CONST char *duparg = NULL;
	macrostat st;
	char *tmp;
	int i, haslist, ret;

	if (arg) duparg = arg;
	haslist = (filelist && !(flags & F_IGNORELIST));
	ret = 0;
	internal_status = FNC_FAIL;

	if (!haslist || !mark) n_args = 0;
	else {
		for (i = 0; i < maxfile; i++) {
			if (ismark(&(filelist[i])))
				filelist[i].tmpflags |= F_ISARG;
			else filelist[i].tmpflags &= ~F_ISARG;
		}
		n_args = mark;
	}

	st.flags = flags;
	if (isinternalcomm(command)) st.flags |= F_ARGSET;

	if (!(tmp = evalcommand(command, duparg, &st))) {
		if (arg) duparg = NULL;
		return(-1);
	}
	st.flags |= F_ARGSET;
	while (st.addopt >= 0 && !(st.flags & F_NOADDOPT)) {
		if (!(tmp = addoption(tmp, duparg, &st))) {
			if (arg) duparg = NULL;
			return(-1);
		}
	}

#ifdef	_NOEXTRAMACRO
	status = internal_status;
	if (!haslist || !st.needmark) for (;;) {
# ifndef	MACROMETA
		tmp = _demacroarg(tmp);
# endif
		r = system3(tmp, st.flags);
		Xfree(tmp);
		tmp = NULL;
		if (!(flags & F_ARGSET)) st.flags &= ~F_ARGSET;

		if (r > ret && (ret = r) >= 127) break;
		if (internal_status <= FNC_FAIL) status = FNC_EFFECT;
		else if (internal_status > status) status = internal_status;

		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand(command, duparg, &st)))
			break;
		while (st.addopt >= 0 && !(st.flags & F_NOADDOPT))
			if (!(tmp = addoption(tmp, duparg, &st))) break;
	}
	else if (n_args <= 0) {
		buf = insertarg(tmp, filelist[filepos].name, st.needmark);
		if (buf) {
# ifndef	MACROMETA
			buf = _demacroarg(buf);
# endif
			ret = system3(buf, st.flags);
			Xfree(buf);
			status = internal_status;
		}
	}
	else for (i = 0; i < maxfile; i++) if (isarg(&(filelist[i]))) {
		buf = insertarg(tmp, filelist[i].name, st.needmark);
		if (buf) {
# ifndef	MACROMETA
			buf = _demacroarg(buf);
# endif
			r = system3(buf, st.flags);
			Xfree(buf);
			if (r > ret && (ret = r) >= 127) break;
			if (internal_status <= FNC_FAIL) status = FNC_EFFECT;
			else if (internal_status > status)
				status = internal_status;
		}
	}
	internal_status = status;
#else	/* !_NOEXTRAMACRO */
	ret = system3(tmp, st.flags);
	if (internal_status <= FNC_FAIL) internal_status = FNC_EFFECT;
#endif	/* !_NOEXTRAMACRO */

	Xfree(tmp);
	if (haslist && internal_status <= FNC_FAIL) {
		for (i = 0; i < maxfile; i++)
			filelist[i].tmpflags &= ~(F_ISARG | F_ISMRK);
		mark = 0;
		marksize = (off_t)0;
	}
	if (arg) duparg = NULL;

	return(ret);
}

XFILE *popenmacro(command, arg, flags)
CONST char *command, *arg;
int flags;
{
	macrostat st;
	XFILE *fp;
	char *tmp, *lang;
	int i;

	internal_status = FNC_FAIL;
	st.flags = flags;
	if (isinternalcomm(command)) st.flags |= F_ARGSET;

	if (!(tmp = evalcommand(command, arg, &st))) return(NULL);
	for (i = 0; i < LOCALELISTSIZ; i++) {
		localelist[i].org = Xstrdup(getenv2(localelist[i].env));
		if (!(localelist[i].val) || *(localelist[i].val)) {
			setenv2(localelist[i].env, localelist[i].val, 1);
			continue;
		}
		lang = getenv2("LC_ALL");
		if (!lang || !*lang) lang = getenv2(localelist[i].env);
		if (!lang || !*lang) lang = getenv2("LANG");
		setenv2(localelist[i].env, lang, 1);
	}
	fp = popen2(tmp, flags);
	for (i = 0; i < LOCALELISTSIZ; i++) {
		setenv2(localelist[i].env, localelist[i].org, 1);
		Xfree(localelist[i].org);
	}
	Xfree(tmp);

	return(fp);
}

#ifndef	DEP_ORIGSHELL
static char *NEAR evalargs(command, argc, argv)
CONST char *command;
int argc;
char *CONST *argv;
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
		if (!Xisdigit(command[i])) {
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

int execusercomm(command, arg, flags)
CONST char *command, *arg;
int flags;
{
	char *cp, **argv;
	int i, j, r, ret, status, argc;

	argc = getargs(command, &argv);
	i = (argc) ? searchfunction(argv[0]) : maxuserfunc;
	if (i >= maxuserfunc) ret = execmacro(command, arg, flags);
	else {
		ret = 0;
		status = internal_status = FNC_FAIL;
		for (j = 0; userfunclist[i].comm[j]; j++) {
			cp = evalargs(userfunclist[i].comm[j], argc, argv);
			r = execmacro(cp, arg, flags);
			Xfree(cp);
			if (r > ret && (ret = r) >= 127) break;
			if (internal_status <= FNC_FAIL) status = FNC_EFFECT;
			else if (internal_status > status)
				status = internal_status;
		}
		internal_status = status;
	}
	freevar(argv);

	return(ret);
}

static char *NEAR evalalias(command)
CONST char *command;
{
	char *cp;
	int i, len;

	len = (cp = strpbrk(command, " \t")) ? cp - command : strlen(command);

	if ((i = searchalias(command, len)) >= maxalias) return(NULL);
	cp = Xmalloc((int)strlen(command) - len
		+ strlen(aliaslist[i].comm) + 1);
	Xstrcpy(Xstrcpy(cp, aliaslist[i].comm), &(command[len]));

	return(cp);
}
#endif	/* !DEP_ORIGSHELL */

int entryhist(s, flags)
CONST char *s;
int flags;
{
	char *new;
	int i, n, size;

	n = (flags & HST_TYPE);
	size = (int)histsize[n];
	if (!history[n]) {
		history[n] = (char **)Xmalloc((size + 1) * sizeof(char *));
		for (i = 0; i <= size; i++) history[n][i] = NULL;
		histbufsize[n] = size;
		histno[n] = (short)0;
	}
	else if (size > histbufsize[n]) {
		history[n] = (char **)Xrealloc(history[n],
			(size + 1) * sizeof(char *));
		for (i = histbufsize[n] + 1; i <= size; i++)
			history[n][i] = NULL;
		histbufsize[n] = size;
	}

	if (!s || !*s) return(0);
	new = (n == 1) ? killmeta(s) : Xstrdup(s);

	if (histno[n]++ >= MAXHISTNO) histno[n] = (short)0;

	if (flags & HST_UNIQ) {
		for (i = 0; i <= size; i++) {
			if (!history[n][i]) continue;
#if	defined (PATHNOCASE) && defined (DEP_DOSPATH)
			if (n == 1 && *new != *(history[n][i])) continue;
#endif
			if (!strpathcmp(new, history[n][i])) break;
		}
		if (i < size) size = i;
	}

	Xfree(history[n][size]);
	for (i = size; i > 0; i--) history[n][i] = history[n][i - 1];
	*(history[n]) = new;
#ifdef	DEP_PTY
	sendparent(TE_SETHISTORY, s, flags);
#endif

	return(1);
}

char *removehist(n)
int n;
{
	char *tmp;
	int i, size;

	size = (int)histsize[n];
	if (!history[n] || size > histbufsize[n] || size <= 0) return(NULL);
	if (--histno[n] < (short)0) histno[n] = MAXHISTNO;
	tmp = *(history[n]);
	for (i = 0; i < size; i++) history[n][i] = history[n][i + 1];
	history[n][size--] = NULL;

	return(tmp);
}

int loadhistory(n)
int n;
{
	lockbuf_t *lck;
	char *line;
	int i, j, size;

	if (!histfile[n] || !*(histfile[n])) return(0);
	lck = lockfopen(histfile[n], "r", O_BINARY | O_RDONLY);
	if (!lck || !(lck -> fp)) {
		lockclose(lck);
		return(-1);
	}

	size = (int)histsize[n];
	history[n] = (char **)Xmalloc((size + 1) * sizeof(char *));
	histbufsize[n] = size;
	histno[n] = (short)0;

	i = -1;
	Xsetflags(lck -> fp, XF_NULLCONV);
	while ((line = Xfgets(lck -> fp))) {
		if (histno[n]++ >= MAXHISTNO) histno[n] = (short)0;
		if (i < size) i++;
		else Xfree(history[n][i]);
		for (j = i; j > 0; j--) history[n][j] = history[n][j - 1];
		*(history[n]) = line;
	}
	lockclose(lck);

	for (i++; i <= size; i++) history[n][i] = NULL;

	return(0);
}

VOID convhistory(s, fp)
CONST char *s;
XFILE *fp;
{
	char *eol;

	if (!s || !*s) return;

	while ((eol = Xstrchr(s, '\n'))) {
		VOID_C Xfwrite(s, eol++ - s, fp);
		VOID_C Xfputc('\0', fp);
		s = eol;
	}
	VOID_C Xfputs(s, fp);
	VOID_C Xfputc('\n', fp);
}

int savehistory(n)
int n;
{
	lockbuf_t *lck;
	int i, size;

	if (!histfile[n] || !*(histfile[n]) || savehist[n] <= 0) return(0);
	if (!history[n] || !*(history[n])) return(-1);
	lck = lockfopen(histfile[n], "w",
		O_BINARY | O_WRONLY | O_CREAT | O_TRUNC);
	if (!lck || !(lck -> fp)) {
		lockclose(lck);
		return(-1);
	}

	size = (savehist[n] > histsize[n])
		? (int)histsize[n] : (int)savehist[n];
	for (i = size - 1; i >= 0; i--) convhistory(history[n][i], lck -> fp);
	lockclose(lck);

	return(0);
}

int parsehist(str, ptrp, quote)
CONST char *str;
int *ptrp, quote;
{
	char *cp;
	int i, n, pc, ptr, size;

	ptr = (ptrp) ? *ptrp : 0;
	size = (int)histsize[0];
	if (str[ptr] == '!') n = 0;
	else if (str[ptr] == '=' || Xstrchr(IFS_SET, str[ptr])) n = ptr = -1;
	else if ((cp = Xsscanf(&(str[ptr]), "%-d", &n))) {
		if (!n) n = -1;
		else if (n < 0) {
			if ((n = -1 - n) > (int)MAXHISTNO) n = -1;
		}
		else if (n - 1 > (int)MAXHISTNO) n = -1;
		else if (n <= (int)(histno[0])) n = (int)histno[0] - n;
		else n = MAXHISTNO - (n - (int)histno[0]) + 1;
		ptr = (cp - str) - 1;
	}
	else {
		for (i = 0; str[ptr + i]; i++) {
			pc = parsechar(&(str[ptr + i]), -1,
				'!', EA_FINDDELIM, &quote, NULL);
			if (pc == PC_WCHAR || pc == PC_ESCAPE) i++;
			else if (pc != PC_NORMAL && pc != PC_DQUOTE) break;
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
	if (ptrp) {
		if (!history[0][n]) return(-1);
	}
	else {
		while (!history[0][n]) if (--n < 0) return(-1);
	}

	return((int)n);
}

char *evalhistory(command)
char *command;
{
	char *cp;
	ALLOC_T size;
	int i, j, n, pc, len, quote, hit;

	hit = 0;
	cp = c_realloc(NULL, 0, &size);
	for (i = j = 0, quote = '\0'; command[i]; i++) {
		cp = c_realloc(cp, j + 1, &size);
		pc = parsechar(&(command[i]), -1, '!', 0, &quote, NULL);
		if (pc == PC_WCHAR || pc == PC_ESCAPE) cp[j++] = command[i++];
		else if (pc == '!') {
			len = i++;
			if ((n = parsehist(command, &i, quote)) < 0) {
				if (i < 0) {
					cp[j++] = '!';
					i = len;
					continue;
				}
				command[++i] = '\0';
				VOID_C Xfprintf(Xstderr,
					"%k: Event not found.\n",
					&(command[len]));
				Xfree(cp);
				return(NULL);
			}
			hit++;
			len = strlen(history[0][n]);
			cp = c_realloc(cp, j + len + 1, &size);
			Xstrncpy(&cp[j], history[0][n], len);
			j += len;
			continue;
		}
		cp[j++] = command[i];
	}
	cp[j] = '\0';
	if (hit) {
		kanjifputs(cp, Xstderr);
		VOID_C fputnl(Xstderr);
		Xfree(command);
		return(cp);
	}
	Xfree(cp);

	return(command);
}

#ifdef	DEBUG
VOID freehistory(n)
int n;
{
	int i;

	if (!history[n] || !*(history[n])) return;
	for (i = 0; i <= histbufsize[n]; i++) Xfree(history[n][i]);
	Xfree(history[n]);
}
#endif

#if	!defined (_NOCOMPLETE) && !defined (DEP_ORIGSHELL)
int completealias(com, len, argc, argvp)
CONST char *com;
int len, argc;
char ***argvp;
{
	int i;

	for (i = 0; i < maxalias; i++) {
		if (strncommcmp(com, aliaslist[i].alias, len)) continue;
		argc = addcompletion(aliaslist[i].alias, NULL, argc, argvp);
	}

	return(argc);
}

int completeuserfunc(com, len, argc, argvp)
CONST char *com;
int len, argc;
char ***argvp;
{
	int i;

	for (i = 0; i < maxuserfunc; i++) {
		if (strncommcmp(com, userfunclist[i].func, len)) continue;
		argc = addcompletion(userfunclist[i].func, NULL, argc, argvp);
	}

	return(argc);
}
#endif	/* !_NOCOMPLETE && !DEP_ORIGSHELL */
