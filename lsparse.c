/*
 *	lsparse.c
 *
 *	parser for file list
 */

#ifdef	FD
#include "fd.h"
#else
#include "headers.h"
#include "depend.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#endif

#include "time.h"
#include "dirent.h"
#include "pathname.h"
#include "unixemu.h"
#include "device.h"
#include "parse.h"
#include "lsparse.h"

#ifdef	FD
#include "kconv.h"
#endif

#define	MAXSCORE		256
#define	IGNORETYPESYM		"-VMNLhC"
#define	SYMLINKSTR		"->"
#define	GNULINKSTR		"link to"
#ifdef	OLDPARSE
#define	LINESEP			'\t'
#else
#define	LINESEP			'\n'
#endif

typedef struct _name_t {
	CONST char *name;
	short len;
} name_t;
#define	DEFNAME(s)		{s, strsize(s)}

#ifdef	DEP_LSPARSE
static int NEAR readattr __P_((namelist *, CONST char *));
static int NEAR readdatestr __P_((CONST char *, CONST char *[], int));
static int NEAR readampm __P_((CONST char *));
#ifdef	OLDPARSE
static int NEAR countfield __P_((CONST char *, CONST u_char [], int, int *));
static char *NEAR getfield __P_((char *, CONST char *,
		int, CONST lsparse_t *, int));
static int NEAR readfileent __P_((namelist *,
		CONST char *, CONST lsparse_t *, int));
#else	/* !OLDPARSE */
static char *NEAR checkspace __P_((CONST char *, int *));
# ifdef	DEP_FILECONV
static char *NEAR readfname __P_((CONST char *, int));
# else
#define	readfname		strndup2
# endif
# ifndef	NOSYMLINK
static char *NEAR readlinkname __P_((CONST char *, int));
# endif
static int NEAR readfileent __P_((namelist *,
		CONST char *, CONST char *, int));
#endif	/* !OLDPARSE */
static int NEAR dircmp __P_((CONST char *, CONST char *));
static char *NEAR pseudodir __P_((namelist *, namelist *, int));
#ifndef	OLDPARSE
static char **NEAR decodevar __P_((char *CONST *));
static int NEAR matchlist __P_((CONST char *, char *CONST *));
#endif

int (*lsintrfunc)__P_((VOID_A)) = NULL;

#ifndef	NOSYMLINK
static CONST name_t linklist[] = {
	DEFNAME(SYMLINKSTR),
	DEFNAME(GNULINKSTR),
};
#define	LINKLISTSIZ		arraysize(linklist)
#endif	/* !NOSYMLINK */
#ifndef	OLDPARSE
static CONST char sympairlist[] = "{}//!!";
static CONST char *autoformat[] = {
# if	MSDOS
	"%*f\n%s %x %x %y-%m-%d %t %a",		/* LHa (v) */
	" %*f\n%s %x %x %d-%m-%y %t %a",	/* RAR (v) */
	"%a %u/%g %s %m %d %t %y %*f",		/* tar (traditional) */
	"%a %u/%g %s %y-%m-%d %t %*f",		/* tar (GNU >=1.12) */
	"%a %l %u %g %s %m %d %{yt} %*f",	/* ls or pax */
	"%a %u/%g %s %m %d %y %t %*f",		/* tar (kmtar) */
	" %s %y-%m-%d %t %*f",			/* zip */
	" %s %x %x %x %y-%m-%d %t %*f",		/* pkunzip */
	" %s %x %x %d %m %y %t %*f",		/* zoo */
	"%1x %12f %s %x %x %y-%m-%d %t %a",	/* LHa (l) */
	" %f %s %x %x %d-%m-%y %t %a",		/* RAR (l) */
# else	/* !MSDOS */
	" %*f\n%s %x %x %d-%m-%y %t %a",	/* RAR (v) */
	"%a %u/%g %s %m %d %t %y %*f",		/* tar (SVR4) */
	"%a %u/%g %s %y-%m-%d %t %*f",		/* tar (GNU >=1.12) */
	"%a %l %u %g %s %m %d %{yt} %*f",	/* ls or pax */
	"%a %u/%g %s %x %m %d %{yt} %*f",	/* LHa (1.14<=) */
	" %s %m-%d-%y %t %*f",			/* zip */
	" %s %x %x %d %m %y %t %*f",		/* zoo */
	"%10a %u/%g %s %m %d %t %y %*f",	/* tar (UXP/DS) */
	"%9a %u/%g %s %m %d %t %y %*f",		/* tar (traditional) */
	"%9a %u/%g %s %x %m %d %{yt} %*f",	/* LHa */
	"%a %u/%g %m %d %t %y %*f",		/* tar (IRIX) */
	" %f %s %x %x %d-%m-%y %t %a",		/* RAR (l) */
# endif	/* !MSDOS */
};
#define	AUTOFORMATSIZ		arraysize(autoformat)
#endif	/* !OLDPARSE */
#endif	/* DEP_LSPARSE */

static CONST char typesymlist[] = "dbclsp";
static CONST u_short typelist[] = {
	S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFSOCK, S_IFIFO
};
#ifdef	DEP_LSPARSE
static CONST char *monthlist[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};
#define	MONTHLISTSIZ		arraysize(monthlist)
static CONST char *weeklist[] = {
	"Sun", "Mon", "Tue",
	"Wed", "Thu", "Fri", "Sat",
};
#define	WEEKLISTSIZ		arraysize(weeklist)
static CONST char *amlist[] = {
	"AM", "a.m.",
};
#define	AMLISTSIZ		arraysize(amlist)
static CONST char *pmlist[] = {
	"PM", "p.m.",
};
#define	PMLISTSIZ		arraysize(pmlist)
static CONST name_t tzlist[] = {
	{"AHST", -10 * 60},
	{"HAST", -10 * 60},
	{"AKST", -9 * 60},
	{"YST", -9 * 60},
	{"YDT", -8 * 60},
	{"PST", -8 * 60},
	{"PDT", -7 * 60},
	{"MST", -7 * 60},
	{"MDT", -6 * 60},
	{"CST", -6 * 60},
	{"CDT", -5 * 60},
	{"EST", -5 * 60},
	{"EDT", -4 * 60},
	{"AST", -4 * 60},
	{"WST", -4 * 60},
	{"ADT", -3 * 60},
	{"WDT", -3 * 60},
	{"NST", -3 * 60 - 30},
	{"NDT", -2 * 60 - 30},
	{"FST", -2 * 60},
	{"FDT", -1 * 60},
	{"UT", 0},
	{"UCT", 0},
	{"UTC", 0},
	{"WET", 0},
	{"BST", +1 * 60},
	{"CET", +1 * 60},
	{"MET", +1 * 60},
	{"EET", +2 * 60},
	{"IST", +3 * 60 + 30},
	{"IDT", +4 * 60 + 30},
	{"SST", +8 * 60},
	{"HKT", +8 * 60},
	{"KST", +9 * 60},
	{"JST", +9 * 60},
	{"KDT", +10 * 60},
	{"NZST", +12 * 60},
};
#define	TZLISTSIZ		arraysize(tzlist)
#endif	/* DEP_LSPARSE */


u_int getfmode(c)
int c;
{
	int i;

	c = tolower2(c);
	for (i = 0; typesymlist[i]; i++)
		if (c == typesymlist[i]) return(typelist[i]);

	return((u_int)-1);
}

int getfsymbol(mode)
u_int mode;
{
	int i;

	for (i = 0; i < arraysize(typelist); i++)
		if ((mode & S_IFMT) == typelist[i]) return(typesymlist[i]);

	return('\0');
}

#ifdef	NOUID
int logical_access(mode)
u_int mode;
#else
int logical_access(mode, uid, gid)
u_int mode;
uid_t uid;
gid_t gid;
#endif
{
	int dir;

	dir = ((mode & S_IFMT) == S_IFDIR);
#ifdef	NOUID
	mode >>= 6;
#else
	if (uid == geteuid()) mode >>= 6;
	else if (gid == getegid() || isgroupmember(gid)) mode >>= 3;
#endif
	if (dir && !(mode & F_ISEXE)) mode &= ~(F_ISRED | F_ISWRI);

	return(mode & 007);
}

#ifdef	DEP_LSPARSE
static int NEAR readattr(tmp, buf)
namelist *tmp;
CONST char *buf;
{
	int i, len;
	u_int n, mode;

	len = strlen(buf);
	if (len < 9) {
		mode = (tmp -> st_mode & S_IFMT) | S_IREAD_ALL | S_IWRITE_ALL;
		while (--len >= 0) {
			if ((n = getfmode(buf[len])) != (u_int)-1) {
				mode &= ~S_IFMT;
				mode |= n;
			}
			else switch (tolower2(buf[len])) {
				case 'a':
					mode |= S_ISVTX;
					break;
				case 'h':
					mode &= ~S_IREAD_ALL;
					break;
				case 'r':
				case 'o':
					mode &= ~S_IWRITE_ALL;
					break;
				case 'w':
				case '-':
				case '.':
				case ' ':
					break;
				case 'v':
					mode &= ~S_IFMT;
					mode |= S_IFIFO;
					break;
				default:
					return(0);
/*NOTREACHED*/
					break;
			}
		}
		if ((mode & S_IFMT) == S_IFDIR) mode |= S_IEXEC_ALL;
	}
	else {
		mode = tmp -> st_mode & S_IFMT;

		i = len - 9;
		if (buf[i] == 'r' || buf[i] == 'R') mode |= S_IRUSR;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'w' || buf[i] == 'W') mode |= S_IWUSR;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'x' || buf[i] == 'X') mode |= S_IXUSR;
		else if (buf[i] == 's') mode |= (S_IXUSR | S_ISUID);
		else if (buf[i] == 'S') mode |= S_ISUID;
		else if (buf[i] != '-') return(0);

		i++;
		if (buf[i] == 'r' || buf[i] == 'R') mode |= S_IRGRP;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'w' || buf[i] == 'W') mode |= S_IWGRP;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'x' || buf[i] == 'X') mode |= S_IXGRP;
		else if (buf[i] == 's') mode |= (S_IXGRP | S_ISGID);
		else if (buf[i] == 'S') mode |= S_ISGID;
		else if (buf[i] != '-') return(0);

		i++;
		if (buf[i] == 'r' || buf[i] == 'R') mode |= S_IROTH;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'w' || buf[i] == 'W') mode |= S_IWOTH;
		else if (buf[i] != '-') return(0);
		i++;
		if (buf[i] == 'x' || buf[i] == 'X') mode |= S_IXOTH;
		else if (buf[i] == 't') mode |= (S_IXOTH | S_ISVTX);
		else if (buf[i] == 'T') mode |= S_ISVTX;
		else if (buf[i] != '-') return(0);

		if (len >= 10 && !strchr2(IGNORETYPESYM, buf[len - 10])) {
			n = getfmode(buf[len - 10]);
			if (n == (u_int)-1) return(0);
			mode &= ~S_IFMT;
			mode |= n;
		}
	}
	tmp -> st_mode = mode;

	return(len);
}

static int NEAR readdatestr(s, list, max)
CONST char *s, *list[];
int max;
{
	int i;

	for (i = 0; i < max; i++) if (!strncasecmp2(s, list[i], 3)) return(i);

	return(-1);
}

static int NEAR readampm(s)
CONST char *s;
{
	int i;

	for (i = 0; i < AMLISTSIZ; i++)
		if (!strcasecmp2(s, amlist[i])) return(0);
	for (i = 0; i < PMLISTSIZ; i++)
		if (!strcasecmp2(s, pmlist[i])) return(1);

	return(-1);
}

VOID initlist(namep, name)
namelist *namep;
CONST char *name;
{
	namep -> name = strdup2(name);
	namep -> ent = 0;
	namep -> st_mode = (S_IREAD_ALL | S_IWUSR | S_IFREG);
	namep -> st_nlink = 1;
#ifndef	NOUID
	namep -> st_uid = (uid_t)-1;
	namep -> st_gid = (gid_t)-1;
#endif
#ifndef	NOSYMLINK
	namep -> linkname = NULL;
#endif
#ifdef	HAVEFLAGS
	namep -> st_flags = 0;
#endif
	namep -> st_size = (off_t)0;
	namep -> st_mtim = (time_t)0;
	namep -> flags = (F_ISRED | F_ISWRI);
	namep -> tmpflags = F_STAT;
}

VOID todirlist(namep, mode)
namelist *namep;
u_int mode;
{
	if (mode == (u_int)-1) mode = S_IEXEC_ALL;
	else {
		namep -> st_mode &= S_IFMT;
		mode &= ~S_IFMT;
	}
	namep -> st_mode &= ~S_IFMT;
	namep -> st_mode |= (S_IFDIR | mode);

	if (!(namep -> st_size)) namep -> st_size = DEV_BSIZE;
	namep -> flags |= (F_ISEXE | F_ISDIR);
}

#ifdef	OLDPARSE
static int NEAR countfield(line, sep, field, eolp)
CONST char *line;
CONST u_char sep[];
int field, *eolp;
{
	int i, j, f, s, sp;

	f = -1;
	s = 1;
	sp = (int)sep[0];

	for (i = j = 0; line[i]; i++) {
		if (sp != SEP_NONE && i >= sp) {
			j++;
			sp = (j < MAXLSPARSESEP) ? (int)sep[j] : SEP_NONE;
			for (; sp == SEP_NONE || i < sp; i++)
				if (!isblank2(line[i])) break;
			if (f < 0) f = 1;
			else f++;
			s = 0;
		}
		else if (isblank2(line[i])) s = 1;
		else if (s) {
			f++;
			s = 0;
		}

		if (field < 0) continue;
		else if (f > field) return(-1);
		else if (f == field) {
			if (eolp) {
				for (j = i; line[j]; j++) {
					if ((sp != SEP_NONE && j >= sp)
					|| isblank2(line[j]))
						break;
				}
				*eolp = j;
			}
			return(i);
		}
	}

	return((field < 0) ? f : -1);
}

static char *NEAR getfield(buf, line, skip, list, no)
char *buf;
CONST char *line;
int skip;
CONST lsparse_t *list;
int no;
{
	CONST char *cp;
	char *tmp;
	int i, f, eol;

	*buf = '\0';
	f = (list -> field[no] != FLD_NONE)
		? (int)(list -> field[no]) - skip : -1;

	if (f < 0 || (i = countfield(line, list -> sep, f, &eol)) < 0)
		return(buf);
	cp = &(line[i]);

	i = (int)(list -> delim[no]);
	if (i >= 128) {
		i -= 128;
		if (&(cp[i]) < &(line[eol])) cp += i;
		else return(buf);
	}
	else if (i && (!(cp = strchr2(cp, i)) || ++cp >= &(line[eol])))
		return(buf);

	i = (int)(list -> width[no]);
	if (i >= 128) i -= 128;
	else if (i) {
		if ((tmp = strchr2(cp, i))) i = tmp - cp;
		else i = &(line[eol]) - cp;
	}
	if (!i || &(cp[i]) > &(line[eol])) i = &(line[eol]) - cp;
	strncpy2(buf, cp, i);

	return(buf);
}

static int NEAR readfileent(tmp, line, list, max)
namelist *tmp;
CONST char *line;
CONST lsparse_t *list;
int max;
{
# ifndef	NOUID
	uidtable *up;
	gidtable *gp;
	uid_t uid;
	gid_t gid;
# endif
	struct tm tm, *tp;
	time_t t;
	off_t n;
	int i, skip;
	char *cp, *buf;

	i = countfield(line, list -> sep, -1, NULL);
	skip = (max > i) ? max - i : 0;
	buf = malloc2(strlen(line) + 1);

	initlist(tmp, NULL);
	tmp -> st_mode = 0;
	tmp -> flags = 0;
	getfield(buf, line, skip, list, F_NAME);
	if (!*buf) {
		free2(buf);
		return(-1);
	}
# if	MSDOS
#  ifdef	BSPATHDELIM
	for (i = 0; buf[i]; i++) if (buf[i] == '/') buf[i] = _SC_;
#  else
	for (i = 0; buf[i]; i++) {
		if (buf[i] == '\\') buf[i] = _SC_;
		if (iskanji1(buf, i)) i++;
	}
#  endif
# endif	/* MSDOS */
	if (isdelim(buf, i = (int)strlen(buf) - 1)) {
		if (i > 0) buf[i] = '\0';
		tmp -> st_mode |= S_IFDIR;
	}
	tmp -> name = strdup2(buf);

	getfield(buf, line, skip, list, F_MODE);
	readattr(tmp, buf);
	if (!(tmp -> st_mode & S_IFMT)) tmp -> st_mode |= S_IFREG;
	if (s_isdir(tmp)) tmp -> flags |= F_ISDIR;
	else if (s_islnk(tmp)) tmp -> flags |= F_ISLNK;
	else if (s_ischr(tmp) || s_isblk(tmp)) tmp -> flags |= F_ISDEV;

# ifndef	NOUID
	getfield(buf, line, skip, list, F_UID);
	if (sscanf2(buf, "%-<*d%$", sizeof(uid), &uid)) tmp -> st_uid = uid;
	else tmp -> st_uid = ((up = finduid(0, buf))) ? up -> uid : (uid_t)-1;
	getfield(buf, line, skip, list, F_GID);
	if (sscanf2(buf, "%-<*d%$", sizeof(gid), &gid)) tmp -> st_gid = gid;
	else tmp -> st_gid = ((gp = findgid(0, buf))) ? gp -> gid : (gid_t)-1;
# endif
	getfield(buf, line, skip, list, F_SIZE);
	tmp -> st_size = (sscanf2(buf, "%qd%$", &n)) ? n : (off_t)0;

	getfield(buf, line, skip, list, F_MON);
	if ((i = readdatestr(buf, monthlist, MONTHLISTSIZ)) >= 0)
		tm.tm_mon = i;
	else if ((i = atoi2(buf)) >= 1 && i < 12) tm.tm_mon = i - 1;
	else tm.tm_mon = 0;
	getfield(buf, line, skip, list, F_DAY);
	tm.tm_mday = ((i = atoi2(buf)) >= 1 && i < 31) ? i : 1;
	getfield(buf, line, skip, list, F_YEAR);
	if (!*buf) tm.tm_year = 1970;
	else if ((i = atoi2(buf)) >= 0) tm.tm_year = i;
	else if (list -> field[F_YEAR] == list -> field[F_TIME]
	&& strchr2(buf, ':')) {
		t = time2();
# ifdef	DEBUG
		_mtrace_file = "localtime(start)";
		tp = localtime(&t);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "localtime(end)";
			malloc(0);	/* dummy malloc */
		}
# else
		tp = localtime(&t);
# endif
		tm.tm_year = tp -> tm_year;
		if (tm.tm_mon > tp -> tm_mon
		|| (tm.tm_mon == tp -> tm_mon && tm.tm_mday > tp -> tm_mday))
			tm.tm_year--;
	}
	else tm.tm_year = 1970;
	if (tm.tm_year < 1900 && (tm.tm_year += 1900) < 1970)
		tm.tm_year += 100;
	tm.tm_year -= 1900;

	getfield(buf, line, skip, list, F_TIME);
	if (!(cp = sscanf2(buf, "%d", &i)) || i > 23)
		tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	else {
		tm.tm_hour = i;
		if (!(cp = sscanf2(cp, ":%d", &i)) || i > 59)
			tm.tm_min = tm.tm_sec = 0;
		else {
			tm.tm_min = i;
			if (!sscanf2(cp, ":%d%$", &i) || i > 60) tm.tm_sec = 0;
			else tm.tm_sec = i;
		}
	}

	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags |= logical_access2(tmp);
	free2(buf);

	return(0);
}

#else	/* !OLDPARSE */

static char *NEAR checkspace(s, scorep)
CONST char *s;
int *scorep;
{
	int i;

	if (isblank2(*s)) {
		(*scorep)++;
		s = skipspace(&(s[1]));
	}
	for (i = 0; s[i]; i++) {
		if (isblank2(s[i])) {
			(*scorep)++;
			for (i++; isblank2(s[i]); i++) /*EMPTY*/;
			if (!s[i]) break;
			*scorep += 4;
		}
	}

	return(strndup2(s, i));
}

# ifdef	DEP_FILECONV
static char *NEAR readfname(s, len)
CONST char *s;
int len;
{
	char *cp, *tmp;

	cp = strndup2(s, len);
	tmp = newkanjiconv(cp, fnamekcode, DEFCODE, L_FNAME);
	if (tmp != cp) free2(cp);

	return(tmp);
}
# endif	/* DEP_FILECONV */

# ifndef	NOSYMLINK
static char *NEAR readlinkname(s, len)
CONST char *s;
int len;
{
	int i;

	for (i = 0; i < LINKLISTSIZ; i++) {
		if (linklist[i].len > len) continue;
		if (!strncmp(s, linklist[i].name, linklist[i].len)) {
			s += linklist[i].len;
			return(skipspace(s));
		}
	}

	return(NULL);
}
# endif	/* !NOSYMLINK */

static int NEAR readfileent(tmp, line, form, skip)
namelist *tmp;
CONST char *line, *form;
int skip;
{
# ifndef	NOUID
	uidtable *up;
	gidtable *gp;
	uid_t uid;
	gid_t gid;
# endif
# ifndef	NOSYMLINK
	CONST char *line2, *lname;
# endif
	struct tm tm, *tp;
	time_t t;
	off_t size, frac, unit;
	CONST char *s, *lastform;
	char *cp, *buf, *eol, *rawbuf;
	u_int mode;
	int i, n, ch, l, len, pm, maj, min, hit, err, err2, retr, score;

	if (skip && skip > strlen(form)) return(-1);

	initlist(tmp, NULL);
	tmp -> st_mode &= ~S_IFREG;
	tmp -> flags = 0;
	tm.tm_year = tm.tm_mon = tm.tm_mday = -1;
	tm.tm_hour = tm.tm_min = tm.tm_sec = -1;
	pm = maj = min = -1;
	mode = (u_int)-1;
	score = 0;

	while (*(lastform = form)) if (isblank2(*form)) {
		line = skipspace(line);
		form++;
	}
	else if (*form == '\n') {
		if (*line != *(form++)) score += 20;
		else line++;
		line = skipspace(line);
	}
	else if (*form != '%' || *(++form) == '%') {
		ch = *(form++);
		if (skip) {
			skip--;
			continue;
		}
		if (*line != ch) score += 10;
		else line++;
	}
	else {
		ch = '\0';
		if (*form == '*') {
			form++;
			for (i = 0; line[i]; i++) if (line[i] == '\n') break;
			len = i;
		}
		else if (!(cp = sscanf2(form, "%+d", &len))) len = -1;
		else {
			form = cp;
			for (i = 0; i < len; i++) if (!line[i]) break;
			len = i;
			ch = -1;
		}

		for (n = 0; n < strsize(sympairlist); n++)
			if (*form == sympairlist[n++]) break;

		s = form++;
		if (n >= strsize(sympairlist)) l = 1;
		else {
			form = strchr2(form, sympairlist[n]);
			if (!form || form <= &(s[1])) {
				free2(tmp -> name);
# ifndef	NOSYMLINK
				free2(tmp -> linkname);
# endif
				return(-1);
			}
			l = ++form - s;
			if (*s == '{') {
				s++;
				l -= 2;
			}
		}

		if (skip) {
			skip--;
			continue;
		}

		if (len < 0) {
			if (!isblank2(*form)
			&& (*form != '%' || form[1] == '%'))
				ch = *form;
			for (len = 0; line[len]; len++) {
				if (ch) {
					if (ch == line[len]) break;
				}
				else if (isblank2(line[len])) break;
			}
		}

		rawbuf = strndup2(line, len);
		hit = err = err2 = retr = 0;
		buf = checkspace(rawbuf, &err);
		for (i = 0; i < l; i++) switch (s[i]) {
			case 'a':
				if (readattr(tmp, buf)) {
					hit++;
					mode = tmp -> st_mode;
				}
				break;
			case 'l':
				if ((n = atoi2(buf)) < 0) break;
				tmp -> st_nlink = n;
				hit++;
				break;
			case 'u':
# ifndef	NOUID
				cp = sscanf2(buf, "%-<*d%$",
					sizeof(uid), &uid);
				if (cp) tmp -> st_uid = uid;
				else if ((up = finduid(0, buf)))
					tmp -> st_uid = up -> uid;
# endif
				hit++;
				break;
			case 'g':
# ifndef	NOUID
				cp = sscanf2(buf, "%-<*d%$",
					sizeof(gid), &gid);
				if (cp) tmp -> st_gid = gid;
				else if ((gp = findgid(0, buf)))
					tmp -> st_gid = gp -> gid;
# endif
				hit++;
				break;
			case 's':
				if (buf[0] == '-' && !buf[1]) {
					hit++;
					break;
				}
				if (!(cp = sscanf2(buf, "%qd", &size))) break;
				unit = (off_t)1024 * (off_t)1024 * (off_t)1024;
				frac = (off_t)0;
				if (*cp == '.') {
					for (cp++; isdigit2(*cp); cp++) {
						unit /= (off_t)10;
						frac += unit * (*cp - '0');
					}
				}
				n = toupper2(*cp);
				if (n == 'K') {
					size *= (off_t)1024;
					size += frac /
						((off_t)1024 * (off_t)1024);
				}
				else if (n == 'M') {
					size *= (off_t)1024 * (off_t)1024;
					size += frac / (off_t)1024;
				}
				else if (n == 'G') {
					size *= (off_t)1024 *
						(off_t)1024 * (off_t)1024;
					size += frac;
				}
				else n = '\0';
				if (n && toupper2(*(++cp)) == 'B') cp++;
				if (*cp) err2++;
				tmp -> st_size = size;
				hit++;
				break;
			case 'y':
				if ((n = atoi2(buf)) < 0) break;
				tm.tm_year = n;
				hit++;
				break;
			case 'm':
				n = readdatestr(buf, monthlist, MONTHLISTSIZ);
				if (n >= 0) tm.tm_mon = n;
				else if ((n = atoi2(buf)) >= 1 && n <= 12)
					tm.tm_mon = n - 1;
				else break;
				hit++;
				break;
			case 'd':
				if ((n = atoi2(buf)) < 1 || n > 31) break;
				tm.tm_mday = n;
				hit++;
				break;
			case 'w':
				n = readdatestr(buf, weeklist, WEEKLISTSIZ);
				if (n < 0) break;
				hit++;
				break;
			case 't':
				if (!(cp = sscanf2(buf, "%d", &n)) || n > 23)
					break;
				tm.tm_hour = n;
				hit++;
				if (!(cp = sscanf2(cp, ":%d", &n)) || n > 59) {
					tm.tm_min = tm.tm_sec = 0;
					err2++;
					break;
				}
				tm.tm_min = n;
				if (*cp != ':') n = 0;
				else if (!(cp = sscanf2(&(cp[1]), "%d", &n))
				|| n > 60) {
					tm.tm_sec = 0;
					break;
				}
				tm.tm_sec = n;
				if (!*cp) /*EMPTY*/;
				else if ((n = readampm(cp)) >= 0) pm = n;
				else err2++;
				break;
			case 'p':
				if ((n = readampm(buf)) < 0) break;
				pm = n;
				hit++;
				break;
			case 'B':
				if ((n = atoi2(buf)) < 0) break;
				maj = n;
				hit++;
				break;
			case 'b':
				if ((n = atoi2(buf)) < 0) break;
				min = n;
				hit++;
				break;
			case 'L':
				if (hit) break;
				eol = &(rawbuf[len]);
				for (n = 0; n < len; n++) {
					cp = &(rawbuf[n]);
# ifdef	BSPATHDELIM
					if (*cp == '/') *cp = _SC_;
# else	/* !BSPATHDELIM */
#  if	MSDOS
					if (*cp == '\\') *cp = _SC_;
					if (iskanji1(rawbuf, n)) {
						n++;
						continue;
					}
#  endif
# endif	/* !BSPATHDELIM */
					if (!isblank2(*cp)) continue;
					cp = skipspace(&(cp[1]));
					if (cp >= eol) {
						if (!ch) n = len;
						break;
					}
				}
				while (isdelim(rawbuf, n - 1)) n--;
				if (n <= 0) break;
# ifndef	NOSYMLINK
				free2(tmp -> linkname);
				tmp -> linkname = readfname(rawbuf, n);
# endif
				tmp -> st_mode &= ~S_IFMT;
				tmp -> st_mode |= S_IFLNK;
				hit++;
				err = 0;
				break;
			case '/':
				n = l - ++i;
				if ((cp = memchr2(&(s[i]), '/', n)))
					n = cp - &(s[i]);
				if (strncasecmp2(buf, &(s[i]), n) || buf[n])
					/*EMPTY*/;
				else {
					todirlist(tmp, mode);
					hit++;
				}
				i += n;
				break;
			case '!':
				n = l - ++i;
				if ((cp = memchr2(&(s[i]), '!', n)))
					n = cp - &(s[i]);
				if (strncasecmp2(buf, &(s[i]), n) || buf[n])
					retr++;
				else form = lastform;
				hit++;
				i += n;
				break;
			case 'f':
				eol = &(rawbuf[len]);
# ifndef	NOSYMLINK
				lname = NULL;
# endif
				for (n = 0; n < len; n++) {
					cp = &(rawbuf[n]);
# ifdef	BSPATHDELIM
					if (*cp == '/') *cp = _SC_;
# else	/* !BSPATHDELIM */
#  if	MSDOS
					if (*cp == '\\') *cp = _SC_;
					if (iskanji1(rawbuf, n)) {
						n++;
						continue;
					}
#  endif
# endif	/* !BSPATHDELIM */
					if (!isblank2(*cp)) continue;
					cp = skipspace(&(cp[1]));
					if (cp >= eol) {
						if (!ch) n = len;
						break;
					}
# ifndef	NOSYMLINK
					lname = readlinkname(cp, eol - cp);
					if (lname) break;
# endif
				}
# ifndef	NOSYMLINK
				line2 = &(line[n]);
# endif
				if (isdelim(rawbuf, n - 1)) {
					todirlist(tmp, mode);
					while (isdelim(rawbuf, --n - 1))
						/*EMPTY*/;
				}
				if (n <= 0) break;
				free2(tmp -> name);
				tmp -> name = readfname(rawbuf, n);
				hit++;
				err = 0;
# ifndef	NOSYMLINK
				if (!lname) break;
				line2 = skipspace(line2);
				if (ch && line2 >= &(line[len])) break;
				lname = &(line[lname - rawbuf]);
				for (n = 0; lname[n]; n++)
					if (isblank2(lname[n])) break;
				if (n) {
					free2(tmp -> linkname);
					tmp -> linkname = readfname(lname, n);
					tmp -> st_mode &= ~S_IFMT;
					tmp -> st_mode |= S_IFLNK;
					n = &(lname[n]) - line;
					if (n > len) len = n;
				}
# endif
				break;
			case 'x':
				hit++;
				err = 0;
				break;
			default:
				hit = -1;
				break;
		}

		free2(buf);
		free2(rawbuf);

		if (hit < 0) {
			free2(tmp -> name);
# ifndef	NOSYMLINK
			free2(tmp -> linkname);
# endif
			return(hit);
		}

		if (!hit) score += 5;
		else if (hit <= err2) score += err2;
		score += err;
		if (!retr) {
			line += len;
			if (!ch) line = skipspace(line);
		}
	}

	if (score >= MAXSCORE || !(tmp -> name) || !*(tmp -> name)) {
		free2(tmp -> name);
# ifndef	NOSYMLINK
		free2(tmp -> linkname);
# endif
		return(MAXSCORE);
	}

	if (!(tmp -> st_mode & S_IFMT)) tmp -> st_mode |= S_IFREG;
	if (s_isdir(tmp)) tmp -> flags |= F_ISDIR;
	else if (s_islnk(tmp)) tmp -> flags |= F_ISLNK;
	else if (s_ischr(tmp) || s_isblk(tmp)) {
		tmp -> flags |= F_ISDEV;
		if (maj >= 0 || min >= 0) {
			if (maj < 0) maj = 0;
			if (min < 0) min = 0;
			tmp -> st_size = makedev(maj, min);
		}
	}

	if (tm.tm_year < 0) {
		t = time2();
# ifdef	DEBUG
		_mtrace_file = "localtime(start)";
		tp = localtime(&t);
		if (_mtrace_file) _mtrace_file = NULL;
		else {
			_mtrace_file = "localtime(end)";
			malloc(0);	/* dummy malloc */
		}
# else
		tp = localtime(&t);
# endif
		tm.tm_year = tp -> tm_year;
		if (tm.tm_mon < 0 || tm.tm_mday < 0) tm.tm_year = 1970 - 1900;
		else if (tm.tm_mon > tp -> tm_mon
		|| (tm.tm_mon == tp -> tm_mon && tm.tm_mday > tp -> tm_mday))
			tm.tm_year--;
	}
	else {
		if (tm.tm_year < 1900 && (tm.tm_year += 1900) < 1970)
			tm.tm_year += 100;
		tm.tm_year -= 1900;
	}
	if (tm.tm_mon < 0) tm.tm_mon = 0;
	if (tm.tm_mday < 0) tm.tm_mday = 1;
	if (tm.tm_hour < 0) tm.tm_hour = 0;
	else if (pm > 0) tm.tm_hour = (tm.tm_hour % 12) + 12;
	else if (!pm) tm.tm_hour %= 12;
	if (tm.tm_min < 0) tm.tm_min = 0;
	if (tm.tm_sec < 0) tm.tm_sec = 0;
	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags |= logical_access2(tmp);

	return(score);
}
#endif	/* !OLDPARSE */

static int NEAR dircmp(s1, s2)
CONST char *s1, *s2;
{
	int i, j;

	for (i = j = 0; s2[j]; i++, j++) {
		if (s1[i] == s2[j]) {
#ifdef	BSPATHDELIM
			if (iskanji1(s1, i) && (s1[++i] != s2[++j] || !s2[j]))
				return(s1[i] - s2[j]);
#endif
			continue;
		}
		if (s1[i] != _SC_ || s2[j] != _SC_) return(s1[i] - s2[j]);
		while (s1[i + 1] == _SC_) i++;
		while (s2[j + 1] == _SC_) j++;
	}

	return(s1[i]);
}

int dirmatchlen(s1, s2)
CONST char *s1, *s2;
{
	int i, j;

	for (i = j = 0; s1[i]; i++, j++) {
		if (s1[i] == s2[j]) {
#ifdef	BSPATHDELIM
			if (iskanji1(s1, i)) {
				if (s1[++i] != s2[++j]) return(0);
				if (s1[i]) continue;
				if (s2[j]) j++;
				break;
			}
#endif
			continue;
		}
		if (s1[i] != _SC_ || s2[j] != _SC_) return(0);
		while (s1[i + 1] == _SC_) i++;
		while (s2[j + 1] == _SC_) j++;
	}
	if (s2[j] && s2[j] != _SC_) {
		for (j = 0; s1[j] == _SC_; j++) /*EMPTY*/;
		return((i == j) ? 1 : 0);
	}
	if (i && s2[j]) while (s2[j + 1] == _SC_) j++;

	return(j);
}

static char *NEAR pseudodir(namep, list, max)
namelist *namep, *list;
int max;
{
	char *cp, *next;
	int i, len;
	u_short ent;

	for (cp = namep -> name; (next = strdelim(cp, 0)); cp = ++next) {
		while (next[1] == _SC_) next++;
		if (!next[1]) break;
		len = next - namep -> name;
		if (!len) len++;
		for (i = 0; i < max; i++) {
			if (isdir(&(list[i]))
			&& len == dirmatchlen(list[i].name, namep -> name))
				break;
		}
		if (i >= max) return(strndup2(namep -> name, len));
		if (strncmp(list[i].name, namep -> name, len)) {
			list[i].name = realloc2(list[i].name, len + 1);
			strncpy2(list[i].name, namep -> name, len);
		}
	}

	if (isdir(namep) && !isdotdir(namep -> name))
	for (i = 0; i < max; i++) {
		if (isdir(&(list[i]))
		&& !dircmp(list[i].name, namep -> name)) {
			cp = list[i].name;
			ent = list[i].ent;
			memcpy((char *)&(list[i]),
				(char *)namep, sizeof(*list));
			list[i].name = cp;
			list[i].ent = ent;
			return(NULL);
		}
	}

	return(namep -> name);
}

#ifndef	OLDPARSE
static char **NEAR decodevar(argv)
char *CONST *argv;
{
	char **new;
	int n, max;

	max = countvar(argv);
	new = (char **)malloc2((max + 1) * sizeof(char *));
	for (n = 0; n < max; n++) new[n] = decodestr(argv[n], NULL, 0);
	new[n] = NULL;

	return(new);
}

static int NEAR matchlist(s, argv)
CONST char *s;
char *CONST *argv;
{
# ifndef	PATHNOCASE
	int duppathignorecase;
# endif
	reg_t *re;
	CONST char *s1, *s2;
	char *new;
	int i, len, ret;

	if (!argv) return(0);
# ifndef	PATHNOCASE
	duppathignorecase = pathignorecase;
	pathignorecase = 0;
# endif

	ret = 0;
	for (i = 0; !ret && argv[i]; i++) {
		s1 = argv[i];
		s2 = s;
		if (!isblank2(s1[0])) s2 = skipspace(s2);
		len = strlen(s1);
		if (!len || isblank2(s1[len - 1])) new = strdup2(s2);
		else {
			len = strlen(s2);
			for (len--; len >= 0; len--)
				if (!isblank2(s2[len])) break;
			new = strndup2(s2, ++len);
		}
		re = regexp_init(s1, -1);
		ret = regexp_exec(re, new, 0);
		free2(new);
		regexp_free(re);
	}
# ifndef	PATHNOCASE
	pathignorecase = duppathignorecase;
# endif

	return(ret);
}
#endif	/* !OLDPARSE */

int parsefilelist(vp, list, namep, linenop, func)
VOID_P vp;
CONST lsparse_t *list;
namelist *namep;
int *linenop;
char *(*func)__P_((VOID_P));
{
#ifdef	OLDPARSE
	int max;
#else
	static char **formlist = NULL;
	static char **lign = NULL;
	static char **lerr = NULL;
	namelist tmp;
	CONST char *form;
	char *form0, *form2;
	short *scorelist;
	int nf, na, ret, skip;
#endif
	static char **lvar = NULL;
	char *cp;
	int i, score, len, nline, needline;

	if (!vp) {
		freevar(lvar);
		lvar = NULL;
#ifndef	OLDPARSE
		freevar(formlist);
		freevar(lign);
		freevar(lerr);
		if (!list || !(list -> format)) formlist = lign = lerr = NULL;
		else {
			formlist = decodevar(list -> format);
			lign = decodevar(list -> lignore);
			lerr = decodevar(list -> lerror);
		}
#endif
		return(0);
	}

#ifndef	OLDPARSE
	if (!formlist || !*formlist) return(-3);
#endif
	if (!(nline = countvar(lvar))) {
		lvar = (char **)realloc2(lvar, (nline + 2) * sizeof(char *));
		if (!(lvar[nline] = (*func)(vp))) return(-1);
		lvar[++nline] = NULL;
	}

	if (lsintrfunc && (*lsintrfunc)()) return(-2);
#ifndef	OLDPARSE
	if (matchlist(lvar[0], lign)) {
		free2(lvar[0]);
		memmove((char *)(&(lvar[0])),
			(char *)(&(lvar[1])), nline * sizeof(*lvar));
		(*linenop)++;
		return(0);
	}
	if (matchlist(lvar[0], lerr)) return(-3);
#endif

#ifdef	OLDPARSE
	max = 0;
	for (i = 0; i < MAXLSPARSEFIELD; i++) {
		if (list -> field[i] == FLD_NONE) continue;
		if ((int)(list -> field[i]) > max)
			max = (int)(list -> field[i]);
	}
#else
	nf = countvar(formlist);
	scorelist = (short *)malloc2(nf * sizeof(short));
	for (i = 0; i < nf; i++) scorelist[i] = MAXSCORE;
	nf = na = skip = 0;
	form0 = NULL;
	ret = 1;
#endif
	namep -> name = NULL;
#ifndef	NOSYMLINK
	namep -> linkname = NULL;
#endif

	needline = 0;
	for (;;) {
#ifdef	OLDPARSE
		needline = list -> lines;
#else	/* !OLDPARSE */
# ifdef	FAKEUNINIT
		form2 = NULL;		/* fake for -Wuninitialized */
# endif
		if (formlist[nf]) form = form2 = formlist[nf];
		else if (scorelist[0] < MAXSCORE) {
			ret = scorelist[0] + 1;
			score = 0;
			break;
		}
		else if (na < AUTOFORMATSIZ) form = autoformat[na++];
		else {
			if (!form0)
				form0 = decodestr(list -> format[0], NULL, 0);
			form = form0;
			skip++;
		}

		needline = 1;
		for (i = 0; form[i]; i++) if (form[i] == '\n') needline++;
#endif	/* !OLDPARSE */

		if (nline < needline) {
			lvar = (char **)realloc2(lvar,
				(needline + 1) * sizeof(char *));
			for (; nline < needline; nline++)
				if (!(lvar[nline] = (*func)(vp))) break;
			lvar[nline] = NULL;
			if (nline < needline) {
#ifndef	OLDPARSE
				if (needline > 1 && na < AUTOFORMATSIZ) {
					if (!formlist[nf]) continue;
					free2(formlist[nf]);
					for (i = nf; formlist[i]; i++)
						formlist[i] = formlist[i + 1];
					continue;
				}
				free2(namep -> name);
				namep -> name = NULL;
# ifndef	NOSYMLINK
				free2(namep -> linkname);
				namep -> linkname = NULL;
# endif
				free2(form0);
				free2(scorelist);
#endif	/* !OLDPARSE */
				*linenop += nline;
				return(-1);
			}
		}

		for (i = len = 0; i < needline; i++)
			len += strlen(lvar[i]) + 1;
		cp = malloc2(len + 1);
		for (i = len = 0; i < needline; i++) {
			strcpy2(&(cp[len]), lvar[i]);
			len += strlen(lvar[i]);
			cp[len++] = LINESEP;
		}
		if (len > 0) len--;
		cp[len] = '\0';

#ifdef	OLDPARSE
		score = readfileent(namep, cp, list, max);
		free2(cp);
		break;
/*NOTREACHED*/
#else	/* !OLDPARSE */
		score = readfileent(&tmp, cp, form, skip);
		free2(cp);

		if (score < 0) {
			if (formlist[nf]) {
				free2(formlist[nf]);
				for (i = nf; formlist[i]; i++)
					formlist[i] = formlist[i + 1];
			}
			else if (na >= AUTOFORMATSIZ) break;
			continue;
		}

		if (!formlist[nf]) {
			if (score < scorelist[0]) i = 0;
		}
		else {
			for (i = 0; i < nf; i++) if (score < scorelist[i]) {
				memmove((char *)(&(formlist[i + 1])),
					(char *)(&(formlist[i])),
					(nf - i) * sizeof(*formlist));
				memmove((char *)(&(scorelist[i + 1])),
					(char *)(&(scorelist[i])),
					(nf - i) * sizeof(*scorelist));
				break;
			}
			scorelist[i] = score;
			formlist[i] = form2;
			nf++;
		}

		if (score >= MAXSCORE);
		else if (!i) {
			free2(namep -> name);
# ifndef	NOSYMLINK
			free2(namep -> linkname);
# endif
			memcpy((char *)namep, (char *)&tmp, sizeof(*namep));
		}
		else {
			free2(tmp.name);
# ifndef	NOSYMLINK
			free2(tmp.linkname);
# endif
		}

		if (!score) break;
#endif	/* !OLDPARSE */
	}

#ifndef	OLDPARSE
	if (score < 0) {
		free2(namep -> name);
		namep -> name = NULL;
# ifndef	NOSYMLINK
		free2(namep -> linkname);
		namep -> linkname = NULL;
# endif
	}
#endif	/* !OLDPARSE */
	if (lvar) {
		for (i = 0; i < needline; i++) free2(lvar[i]);
		if (nline + 1 > needline)
			memmove((char *)(&(lvar[0])),
				(char *)(&(lvar[needline])),
				(nline - needline + 1) * sizeof(*lvar));
	}
	*linenop += needline;

#ifdef	OLDPARSE
	return((score) ? 0 : 1);
#else
	free2(form0);
	free2(scorelist);
	return((score) ? 0 : ret);
#endif
}

namelist *addlist(list, max, entp)
namelist *list;
int max, *entp;
{
	int ent;

	if (!entp) ent = max + 1;
	else {
		if (max < *entp) return(list);
		ent = *entp = ((max + 1) / BUFUNIT + 1) * BUFUNIT;
	}
	list = (namelist *)realloc2(list, ent * sizeof(namelist));

	return(list);
}

VOID freelist(list, max)
namelist *list;
int max;
{
	int i;

	if (list) {
		for (i = 0; i < max; i++) {
			free2(list[i].name);
#ifndef	NOSYMLINK
			free2(list[i].linkname);
#endif
		}
		free2(list);
	}
}

int lsparse(vp, list, listp, func)
VOID_P vp;
CONST lsparse_t *list;
namelist **listp;
char *(*func) __P_((VOID_P));
{
	namelist tmp;
	char *cp, *dir;
	int n, no, max, ent, score;

	for (n = 0; n < (int)(list -> topskip); n++) {
		if (!(cp = (*func)(vp))) break;
		free2(cp);
	}

	max = ent = 0;
	*listp = addlist(*listp, max, &ent);
	initlist(&((*listp)[0]), parentpath);
	todirlist(&((*listp)[0]), (u_int)-1);
	max++;

	no = 0;
#ifdef	HAVEFLAGS
	tmp.st_flags = (u_long)0;
#endif
	score = -1;
	parsefilelist(NULL, list, NULL, NULL, NULL);
	for (;;) {
		n = parsefilelist(vp, list, &tmp, &no, func);
		if (n < 0) {
			if (n == -1) break;
			parsefilelist(NULL, NULL, NULL, NULL, NULL);
			freelist(*listp, max);
			*listp = NULL;
			return(n);
		}
		if (!n) continue;
		else if (score < 0) score = n;
		else if (n > score) continue;

		for (;;) {
			dir = pseudodir(&tmp, *listp, max);
			if (!dir || dir == tmp.name) break;
			*listp = addlist(*listp, max, &ent);
			memcpy((char *)&((*listp)[max]),
				(char *)&tmp, sizeof(**listp));
			todirlist(&((*listp)[max]), (u_int)-1);
			(*listp)[max].name = dir;
#ifndef	NOSYMLINK
			(*listp)[max].linkname = NULL;
#endif
			(*listp)[max].ent = no;
			max++;
		}
		if (!dir) {
			free2(tmp.name);
#ifndef	NOSYMLINK
			free2(tmp.linkname);
#endif
			continue;
		}

		if (isdotdir(tmp.name) != 1
		|| max <= 0 || isdotdir((*listp)[0].name) != 1)
			*listp = addlist(*listp, max, &ent);
		else {
			free2((*listp)[0].name);
#ifndef	NOSYMLINK
			free2((*listp)[0].linkname);
#endif
			memmove((char *)&((*listp)[0]),
				(char *)&((*listp)[1]),
				--max * sizeof(**listp));
		}

		memcpy((char *)&((*listp)[max]),
			(char *)&tmp, sizeof(**listp));
		(*listp)[max].ent = no;
		max++;
	}
	parsefilelist(NULL, NULL, NULL, NULL, NULL);

	no -= (int)(list -> bottomskip);
	for (n = max - 1; n > 0; n--) {
		if ((*listp)[n].ent <= no) break;
		free2((*listp)[n].name);
#ifndef	NOSYMLINK
		free2((*listp)[n].linkname);
#endif
	}
	max = n + 1;
	for (n = 0; n < max; n++) (*listp)[n].ent = n;

	return(max);
}

int strptime2(s, fmt, tm, tzp)
CONST char *s, *fmt;
struct tm *tm;
int *tzp;
{
	char *cp;
	int n, c, len, cent, hour, pm, tz;

	cent = hour = tz = 0;
	pm = -1;
	tm -> tm_year = tm -> tm_mon = tm -> tm_mday =
	tm -> tm_hour = tm -> tm_min = tm -> tm_sec = 0;

	for (; *fmt; fmt++) {
		if (*fmt == ' ') {
			if (*s == ' ') for (s++; *s == ' '; s++) /*EMPTY*/;
			continue;
		}
		if (*fmt != '%' || *(++fmt) == '%') {
			if (*(s++) != *fmt) return(-1);
			continue;
		}

		c = (fmt[1] != '%' || fmt[2] == '%') ? fmt[1] : ' ';
		for (len = 0; s[len]; len++) if (s[len] == c) break;
		cp = strndup2(s, len);
		s += len;
		n = 0;
		switch (*fmt) {
			case 'a':
			case 'A':
				n = readdatestr(cp, weeklist, WEEKLISTSIZ);
#if	0
				if (n >= 0) tm -> tm_wday = n;
#endif
				break;
			case 'b':
			case 'B':
			case 'h':
				n = readdatestr(cp, monthlist, MONTHLISTSIZ);
				if (n >= 0) tm -> tm_mon = n;
				break;
			case 'C':
				if ((n = atoi2(cp)) < 19 || n > 99) n = -1;
				else cent = n;
				break;
			case 'd':
			case 'e':
				if ((n = atoi2(cp)) < 0 || n > 31) n = -1;
				else tm -> tm_mday = n;
				break;
			case 'H':
				if ((n = atoi2(cp)) < 0 || n > 23) n = -1;
				else tm -> tm_hour = n;
				break;
			case 'I':
				if ((n = atoi2(cp)) < 1 || n > 12) n = -1;
				else hour = n;
				break;
			case 'm':
				if ((n = atoi2(cp)) < 1 || n > 12) n = -1;
				else tm -> tm_mon = n - 1;
				break;
			case 'M':
				if ((n = atoi2(cp)) < 0 || n > 59) n = -1;
				else tm -> tm_min = n;
				break;
			case 'p':
				n = readampm(cp);
				if (n >= 0) pm = n;
				break;
			case 'S':
				if ((n = atoi2(cp)) < 0 || n > 60) n = -1;
				else tm -> tm_sec = n;
				break;
			case 'u':
				if ((n = atoi2(cp)) < 1 || n > 7) n = -1;
#if	0
				else tm -> tm_wday = n % 7;
#endif
				break;
			case 'w':
				if ((n = atoi2(cp)) < 0 || n > 6) n = -1;
#if	0
				else tm -> tm_wday = n;
#endif
				break;
			case 'y':
				if ((n = atoi2(cp)) < 0 || n > 99) n = -1;
				else {
					if (n < 69) n += 100;
					tm -> tm_year = n;
				}
				break;
			case 'Y':
				if ((n = atoi2(cp)) < 0 || n > 9999) n = -1;
				else tm -> tm_year = n - 1900;
				break;
			case 'Z':
				tz = 0;
				if (*cp == '-') tz = -1;
				else if (*cp == '+') tz = 1;
				if (tz) {
					if ((n = atoi2(&(cp[1]))) < 0) break;
					n = (n / 100) * 60 + (n % 100);
					tz *= n;
					break;
				}
				else if (!strncmp(cp, "GMT", strsize("GMT"))) {
					n = strsize("GMT");
					if (!cp[n]) break;
					else if (cp[n] == '-') tz = -1;
					else if (cp[n] == '+') tz = 1;
					else {
						n = -1;
						break;
					}
					n++;
					if ((n = atoi2(&(cp[n]))) < 0) break;
					tz *= n * 60;
					break;
				}
				for (n = 0; n < TZLISTSIZ; n++)
					if (!strcmp(cp, tzlist[n].name)) break;
				if (n >= TZLISTSIZ) {
					n = -1;
					break;
				}
				tz = tzlist[n].len;
				break;
			default:
				n = -1;
				break;
		}
		free2(cp);
		if (n < 0) return(-1);
	}

	if (cent) tm -> tm_year = cent * 100 - 1900 + (tm -> tm_year % 100);
	if (hour) {
		if (pm > 0) hour = (hour % 12) + 12;
		else if (!pm) hour %= 12;
		tm -> tm_hour = hour;
	}
	if (tzp) *tzp = tz;

	return(0);
}
#endif	/* DEP_LSPARSE */
