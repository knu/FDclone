/*
 *	archive.c
 *
 *	Archive File Access Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "kanji.h"

#ifndef	_NOARCHIVE

#ifdef	_NOORIGSHELL
#define	dopclose	Xpclose
#else
#include "system.h"
#endif

#ifndef	_NODOSDRIVE
extern int shutdrv __P_((int));
# if	MSDOS
extern int checkdrive __P_((int));
# endif
#endif
#if	MSDOS
extern char *unixgetcurdir __P_((char *, int));
#endif

extern int mark;
extern int stackdepth;
extern int sizeinfo;
extern off_t totalsize;
extern off_t blocksize;
extern namelist filestack[];
extern char fullpath[];
extern char typesymlist[];
extern u_short typelist[];
extern char *deftmpdir;
extern char *tmpfilename;
extern u_short today[];
#ifndef	_NOCOLOR
extern int ansicolor;
#endif
extern int n_args;
extern int hideclock;

#if	FD >= 2
#if	MSDOS
#define	PM_LHA	"%f\n%s %x %x %y-%m-%d %t", 5, 2
#define	PM_TAR	"%a %u/%g %s %m %d %t %y %f", 0, 0
#else	/* !MSDOS */
#define	PM_LHA	"%9a %u/%g %s %x %m %d %{yt} %f", 2, 2
# ifdef	UXPDS
#define	PM_TAR	"%10a %u/%g %s %m %d %t %y %f", 0, 0
# else	/* !UXPDS */
#  ifdef	TARFROMPAX
#define	PM_TAR	"%a %x %u %g %s %m %d %{yt} %f", 0, 0
#  else	/* !TARFROMPAX */
#   ifdef	TARUSESPACE
#define	PM_TAR	"%a %u/%g %s %m %d %t %y %f", 0, 0
#   else	/* !TARUSESPACE */
#define	PM_TAR	"%9a %u/%g %s %m %d %t %y %f", 0, 0
#   endif	/* !TARUSESPACE */
#  endif	/* !TARFROMPAX */
# endif	/* !UXPDS */
#endif	/* !MSDOS */
#define	PM_NULL	NULL, 0, 0
#define	LINESEP	'\n'
#define	isarchbr(l)	((l) -> format)
#else	/* FD < 2 */
#if	MSDOS
#define	PM_LHA	5, 2, \
		{255, 255, 255, 1, 4, 4, 4, 5, 0}, \
		{0, 0, 0, 0, 0, '-', 128 + 6, 0, 0}, \
		{0, 0, 0, 0, '-', '-', 0, 0, 0}, \
		{255, 255, 255}, 2
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 5, 3, 4, 6, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{0, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#else	/* !MSDOS */
#define	PM_LHA	2, 2, \
		{0, 1, 1, 2, 6, 4, 5, 6, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{9, 255, 255}, 1
# ifdef	UXPDS
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 10, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{10, 255, 255}, 1
# else	/* !UXPDS */
#  ifdef	TARFROMPAX
#define	PM_TAR	0, 0, \
		{0, 2, 3, 4, 7, 5, 6, 7, 8}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#  else	/* !TARFROMPAX */
#   ifdef	TARUSESPACE
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{0, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#   else	/* !TARUSESPACE */
#define	PM_TAR	0, 0, \
		{0, 1, 1, 2, 6, 3, 4, 5, 7}, \
		{0, 0, '/', 0, 0, 0, 0, 0, 0}, \
		{128 + 9, '/', 0, 0, 0, 0, 0, 0, 0}, \
		{9, 255, 255}, 1
#   endif	/* !TARUSESPACE */
#  endif	/* !TARFROMPAX */
# endif	/* !UXPDS */
#endif	/* !MSDOS */
#define	PM_NULL	255, 0, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{0, 0, 0, 0, 0, 0, 0, 0, 0}, \
		{255, 255, 255}, 1
#define	LINESEP	'\t'
#define	isarchbr(l)	(((l) -> topskip) < 255)
#endif	/* FD < 2 */

static VOID NEAR pusharchdupl __P_((VOID_A));
#if	FD >= 2
static int NEAR readfileent __P_((namelist *, char *, char *, int));
#else
static int NEAR countfield __P_((char *, u_char [], int, int *));
static char *NEAR getfield __P_((char *, char *, int, launchtable *, int));
static int NEAR readfileent __P_((namelist *, char *, launchtable *, int));
#endif
static int NEAR dircmp __P_((char *, char *));
static int NEAR dirmatchlen __P_((char *, char *));
static char *NEAR pseudodir __P_((namelist *));
static int NEAR readarchive __P_((char *, launchtable *));
static char *NEAR searcharcdir __P_((char *, int));
static char *NEAR genfullpath __P_((char *, char *));

int maxlaunch = 0;
int maxarchive = 0;
launchtable launchlist[MAXLAUNCHTABLE] = {
#if	MSDOS
	{"*.lzh",	"lha v %S",		PM_LHA, 0},
	{"*.tar",	"tar tvf %S", 		PM_TAR, 0},
	{"*.tar.Z",	"gzip -cd %S | tar tvf -",	PM_TAR, 0},
	{"*.tar.gz",	"gzip -cd %S | tar tvf -",	PM_TAR, 0},
	{"*.tar.bz2",	"bzip2 -cd %S | tar tvf -",	PM_TAR, 0},
	{"*.taz",	"gzip -cd %S | tar tvf -",	PM_TAR, 0},
	{"*.tgz",	"gzip -cd %S | tar tvf -",	PM_TAR, 0},
#else
	{"*.lzh",	"lha l",		PM_LHA, 0},
	{"*.tar",	"tar tvf", 		PM_TAR, 0},
	{"*.tar.Z",	"zcat %C | tar tvf -",	PM_TAR, 0},
	{"*.tar.gz",	"gzip -cd %C | tar tvf -",	PM_TAR, 0},
	{"*.tar.bz2",	"bzip2 -cd %C | tar tvf -",	PM_TAR, 0},
	{"*.taZ",	"zcat %C | tar tvf -",	PM_TAR, 0},
	{"*.taz",	"gzip -cd %C | tar tvf -",	PM_TAR, 0},
	{"*.tgz",	"gzip -cd %C | tar tvf -",	PM_TAR, 0},
#endif
	{NULL,		NULL,			PM_NULL, 0}
};
archivetable archivelist[MAXARCHIVETABLE] = {
#if	MSDOS
	{"*.lzh",	"lha a %S %TA",		"lha x %S %TA", 0},
	{"*.tar",	"tar cf %C %T",		"tar xf %C %TA", 0},
	{"*.tar.Z",	"tar cf - %T | compress -c > %C",
					"gzip -cd %S | tar xf - %TA", 0},
	{"*.tar.gz",	"tar cf - %T | gzip -c > %C",
					"gzip -cd %S | tar xf - %TA", 0},
	{"*.tar.bz2",	"tar cf - %T | bzip2 -c > %C",
					"bzip2 -cd %S | tar xf - %TA", 0},
	{"*.taz",	"tar cf - %T | compress -c > %C",
					"gzip -cd %S | tar xf - %TA", 0},
	{"*.tgz",	"tar cf - %T | gzip -c > %C",
					"gzip -cd %S | tar xf - %TA", 0},
#else
	{"*.lzh",	"lha aq %C %TA",	"lha xq %C %TA", 0},
	{"*.tar",	"tar cf %C %T",		"tar xf %C %TA", 0},
	{"*.tar.Z",	"tar cf - %T | compress -c > %C",
					"zcat %C | tar xf - %TA", 0},
	{"*.tar.gz",	"tar cf - %T | gzip -c > %C",
					"gzip -cd %C | tar xf - %TA", 0},
	{"*.tar.bz2",	"tar cf - %T | bzip2 -c > %C",
					"bzip2 -cd %C | tar xf - %TA", 0},
	{"*.taZ",	"tar cf - %T | compress -c > %C",
					"zcat %C | tar xf - %TA", 0},
	{"*.taz",	"tar cf - %T | gzip -c > %C",
					"gzip -cd %C | tar xf - %TA", 0},
	{"*.tgz",	"tar cf - %T | gzip -c > %C",
					"gzip -cd %C | tar xf - %TA", 0},
#endif
	{NULL,		NULL,			NULL, 0}
};
char archivedir[MAXPATHLEN];

#if	FD >= 2
static char *autoformat[] = {
# if	MSDOS
	"%f\n%s %x %x %y-%m-%d %t",		/* LHa (-v) */
	" %f %s %x %x %y-%m-%d %t",		/* LHa (-l) */
	"%a %u/%g %s %m %d %t %y %f",		/* tar (traditional) */
	"%a %u/%g %s %y-%m-%d %t %f",		/* tar (GNU 1.12<=) */
	" %s %y-%m-%d %t %f",			/* zip */
	" %s %x %x %x %y-%m-%d %t %f",		/* pkunzip */
# else
	"%9a %u/%g %s %m %d %t %y %f",		/* tar (traditional) */
	"%a %u/%g %s %m %d %t %y %f",		/* tar (SVR4) */
	"%a %u/%g %s %y-%m-%d %t %f",		/* tar (GNU 1.12<=) */
	"%10a %u/%g %s %m %d %t %y %f",		/* tar (UXP/DS) */
	"%9a %u/%g %s %x %m %d %{yt} %f",	/* LHa */
	"%a %u/%g %s %x %m %d %{yt} %f",	/* LHa (1.14<=) */
	"%a %x %u %g %s %m %d %{yt} %f",	/* pax */
	" %s %m-%d-%y %t %f",			/* zip */
# endif
	" %s %x %x %d %m %y %t %f",		/* zoo */
};
#define	AUTOFORMATSIZ	((int)(sizeof(autoformat) / sizeof(char *)))
#endif

static VOID NEAR pusharchdupl(VOID_A)
{
	winvartable *new;

	new = (winvartable *)malloc2(sizeof(winvartable));

	new -> v_archduplp = archduplp;
	new -> v_archivedir = NULL;
	new -> v_archivefile = archivefile;
	new -> v_archtmpdir = archtmpdir;
	new -> v_launchp = launchp;
	new -> v_arcflist = arcflist;
	new -> v_maxarcf = maxarcf;
#ifndef	_NODOSDRIVE
	new -> v_archdrive = archdrive;
#endif
#ifndef	_NOTREE
	new -> v_treepath = treepath;
#endif
	new -> v_fullpath = NULL;
	new -> v_findpattern = findpattern;
	new -> v_filelist = filelist;
	new -> v_maxfile = maxfile;
	new -> v_maxent = maxent;
	new -> v_filepos = filepos;
	new -> v_sorton = sorton;
	archduplp = new;
}

VOID poparchdupl(VOID_A)
{
	winvartable *old;
	char *cp, *dir;
	int i;
#ifndef	_NODOSDRIVE
	int drive;
#endif

	if (!(old = archduplp)) return;

	cp = archivefile;
	dir = archtmpdir;
#ifndef	_NODOSDRIVE
	drive = archdrive;
#endif
	for (i = 0; i < maxarcf; i++) free(arcflist[i].name);
	free(arcflist);

	archduplp = old -> v_archduplp;
	if (!(old -> v_archivedir)) *archivedir = '\0';
	else {
		strcpy(archivedir, old -> v_archivedir);
		free(old -> v_archivedir);
	}
	archivefile = old -> v_archivefile;
	archtmpdir = old -> v_archtmpdir;
	launchp = old -> v_launchp;
	arcflist = old -> v_arcflist;
	maxarcf = old -> v_maxarcf;
#ifndef	_NODOSDRIVE
	archdrive = old -> v_archdrive;
#endif
#ifndef	_NOTREE
	treepath = old -> v_treepath;
#endif
	if (findpattern) free(findpattern);
	findpattern = old -> v_findpattern;
	filepos = old -> v_filepos;
	sorton = old -> v_sorton;

	free(filelist);
	filelist = old -> v_filelist;
	maxfile = old -> v_maxfile;
	maxent = old -> v_maxent;

#ifndef	_NODOSDRIVE
	if (drive > 0) {
		strcpy(fullpath, old -> v_fullpath);
		free(old -> v_fullpath);
		removetmp(dir, NULL, cp);
	}
	else
#endif
	if (archivefile) {
		strcpy(fullpath, old -> v_fullpath);
		free(old -> v_fullpath);
		removetmp(dir, archivedir, cp);
	}

	free(cp);
	free(old);
}

#if	FD >= 2
static int NEAR readfileent(tmp, line, form, skip)
namelist *tmp;
char *line, *form;
int skip;
{
	struct tm tm;
#if	!MSDOS
	uidtable *up;
	gidtable *gp;
#endif
	long n;
	u_short mode;
	int i, ch, l, len;
	char *cp, *next, *buf;

	tmp -> name = NULL;
	tmp -> st_mode = 0644;
#if	!MSDOS
	tmp -> st_uid = (uid_t)-1;
	tmp -> st_gid = (gid_t)-1;
#endif
	tmp -> st_size = 0L;
	tmp -> flags = 0;
	tmp -> tmpflags = F_STAT;
	tm.tm_year = tm.tm_mon = tm.tm_mday = -1;
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;

	while (*form) if (strchr(IFS_SET, *form)) {
		while (*line && strchr(IFS_SET, *line)) line++;
		form++;
	}
	else if (*form != '%') {
		if (skip) {
			skip--;
			form++;
			continue;
		}
		if (*line != *(form++)) {
			if (tmp -> name) free(tmp -> name);
			return(0);
		}
		line++;
	}
	else {
		if (!(next = evalnumeric(++form, &n, 1))) len = -1;
		else {
			form = next;
			for (i = 0; i < n; i++) if (!line[i]) break;
			len = i;
		}

		if (*form != '{') {
			l = 1;
			next = form + 1;
		}
		else {
			next = strchr2(++form, '}');
			if (!next || next <= form) {
				if (tmp -> name) free(tmp -> name);
				return(-1);
			}
			l = next++ - form;
		}

		if (skip) {
			skip--;
			form = next;
			continue;
		}

		ch = -1;
		if (len >= 0);
		else if (l == 1 && *form == '%') {
			if (*line != *form) {
				if (tmp -> name) free(tmp -> name);
				return(0);
			}
			l = 0;
			len = 1;
		}
		else {
			ch = *next;
			if (strchr(IFS_SET, ch)
			|| (ch == '%' && *(next + 1) != '%')) ch = '\0';
			for (len = 0; line[len]; len++) {
				if (ch) {
					if (ch == line[len]) break;
				}
				else if (strchr(IFS_SET, line[len])) break;
			}
		}

		buf = strdupcpy(line, len);
		while (l--) switch (form[l]) {
			case 'a':
				if (len <= 0) break;
				tmp -> st_mode &=
					~(0777 | S_ISUID | S_ISGID | S_ISVTX);
				if ((i = len - 9) < 0) i = 0;
				if (i + 2 < len && line[i + 2] == 's')
					tmp -> st_mode |= S_ISUID;
				if (i + 5 < len && line[i + 5] == 's')
					tmp -> st_mode |= S_ISGID;
				if (i + 8 < len && line[i + 8] == 's')
					tmp -> st_mode |= S_ISVTX;
				mode = 0;
				for (; i < len; i++) {
					mode *= 2;
					if (line[i] != '-') mode |= 1;
				}
				tmp -> st_mode |= mode;

				if (len <= 9) i = -1;
				else for (i = 0; typesymlist[i]; i++)
					if (line[len - 10] == typesymlist[i])
						break;
				if (i >= 0 && typesymlist[i]) {
					tmp -> st_mode &= ~S_IFMT;
					tmp -> st_mode |= typelist[i];
				}
				else if (!(tmp -> st_mode & S_IFMT))
					tmp -> st_mode |= S_IFREG;
				break;
			case 'u':
#if	!MSDOS
				if (evalnumeric(buf, &n, -1))
					tmp -> st_uid = n;
				else if (up = finduid(0, buf))
					tmp -> st_uid = up -> uid;
				else tmp -> st_uid = (uid_t)-1;
#endif
				break;
			case 'g':
#if	!MSDOS
				if (evalnumeric(buf, &n, -1))
					tmp -> st_gid = n;
				else if (gp = findgid(0, buf))
					tmp -> st_gid = gp -> gid;
				else tmp -> st_gid = (gid_t)-1;
#endif
				break;
			case 's':
				if (evalnumeric(buf, &n, 0))
					tmp -> st_size = n;
				else tmp -> st_size = 0L;
				break;
			case 'y':
				if ((i = atoi2(buf)) >= 0) tm.tm_year = i;
				break;
			case 'm':
				if (!strncmp(line, "Jan", 3)) tm.tm_mon = 0;
				else if (!strncmp(line, "Feb", 3))
					tm.tm_mon = 1;
				else if (!strncmp(line, "Mar", 3))
					tm.tm_mon = 2;
				else if (!strncmp(line, "Apr", 3))
					tm.tm_mon = 3;
				else if (!strncmp(line, "May", 3))
					tm.tm_mon = 4;
				else if (!strncmp(line, "Jun", 3))
					tm.tm_mon = 5;
				else if (!strncmp(line, "Jul", 3))
					tm.tm_mon = 6;
				else if (!strncmp(line, "Aug", 3))
					tm.tm_mon = 7;
				else if (!strncmp(line, "Sep", 3))
					tm.tm_mon = 8;
				else if (!strncmp(line, "Oct", 3))
					tm.tm_mon = 9;
				else if (!strncmp(line, "Nov", 3))
					tm.tm_mon = 10;
				else if (!strncmp(line, "Dec", 3))
					tm.tm_mon = 11;
				else if ((i = atoi2(buf)) >= 1 && i <= 12)
					tm.tm_mon = i - 1;
				break;
			case 'd':
				if ((i = atoi2(buf)) >= 1 && i <= 31)
					tm.tm_mday = i;
				break;
			case 't':
				if (!(cp = evalnumeric(buf, &n, 0))
				|| n > 23) {
					tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
					break;
				}
				tm.tm_hour = n;
				if (*cp != ':'
				|| !(cp = evalnumeric(++cp, &n, 0))
				|| n > 59) {
					tm.tm_min = tm.tm_sec = 0;
					break;
				}
				tm.tm_min = n;
				if (*cp != ':'
				|| !(cp = evalnumeric(++cp, &n, 0))
				|| n > 59) {
					tm.tm_sec = 0;
					break;
				}
				tm.tm_sec = n;
				break;
			case 'f':
#if	MSDOS
				for (i = 0; i < len; i++)
					if (buf[i] == '/') buf[i] = _SC_;
#endif
				if (isdelim(buf, len - 1)) {
					tmp -> st_mode &= ~S_IFMT;
					tmp -> st_mode |= S_IFDIR;
					if (len > 1) buf[len - 1] = '\0';
				}
				if (tmp -> name) free(tmp -> name);
				tmp -> name = strdup2(buf);
				break;
			case 'x':
				break;
			default:
				free(buf);
				if (tmp -> name) free(tmp -> name);
				return(-1);
/*NOTREACHED*/
				break;
		}
		free(buf);
		line += len;
		if (!ch) while (*line && strchr(IFS_SET, *line)) line++;
		form = next;
	}

	if (!(tmp -> name)) return((skip) ? -1 : 0);
	else if (!*(tmp -> name)) {
		free(tmp -> name);
		return((skip) ? -1 : 0);
	}

	if ((tmp -> st_mode & S_IFMT) == S_IFDIR) tmp -> flags |= F_ISDIR;
	else if ((tmp -> st_mode & S_IFMT) == S_IFLNK) tmp -> flags |= F_ISLNK;
	if (tm.tm_year < 0) {
		tm.tm_year = today[0];
		if (tm.tm_mon < 0 || tm.tm_mday < 0);
		else if (tm.tm_mon > today[1]
		|| (tm.tm_mon == today[1] && tm.tm_mday > today[2]))
			tm.tm_year--;
	}
	else {
		if (tm.tm_year < 1900 && (tm.tm_year += 1900) < 1970)
			tm.tm_year += 100;
		tm.tm_year -= 1900;
	}
	if (tm.tm_mon < 0) tm.tm_mon = 0;
	if (tm.tm_mday < 0) tm.tm_mday = 1;

	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags |=
#if	MSDOS
		logical_access(tmp -> st_mode);
#else
		logical_access(tmp -> st_mode, tmp -> st_uid, tmp -> st_gid);
#endif

	return(1);
}

#else	/* FD < 2 */

static int NEAR countfield(line, sep, field, eolp)
char *line;
u_char sep[];
int field, *eolp;
{
	int i, j, f, s, sp;

	f = -1;
	s = 1;
	sp = (int)sep[0];

	for (i = j = 0; line[i]; i++) {
		if (sp < 255 && i >= sp) {
			j++;
			sp = (j < MAXLAUNCHSEP) ? (int)sep[j] : 255;
			for (; sp == 255 || i < sp; i++)
				if (!strchr(" \t", line[i])) break;
			if (f < 0) f = 1;
			else f++;
			s = 0;
		}
		else if (strchr(" \t", line[i])) s = 1;
		else if (s) {
			f++;
			s = 0;
		}

		if (field < 0) continue;
		else if (f > field) return(-1);
		else if (f == field) {
			if (eolp) {
				for (j = i; line[j]; j++) {
					if ((sp < 255 && j >= sp)
					|| strchr(" \t", line[j])) break;
				}
				*eolp = j;
			}
			return(i);
		}
	}
	return((field < 0) ? f : -1);
}

static char *NEAR getfield(buf, line, skip, list, no)
char *buf, *line;
int skip;
launchtable *list;
int no;
{
	char *cp, *tmp;
	int i, f, eol;

	*buf = '\0';
	f = (list -> field[no] < 255) ? (int)(list -> field[no]) - skip : -1;

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

	return(strncpy2(buf, cp, i));
}

static int NEAR readfileent(tmp, line, list, max)
namelist *tmp;
char *line;
launchtable *list;
int max;
{
	struct tm tm;
#if	!MSDOS
	uidtable *up;
	gidtable *gp;
#endif
	long n;
	u_short mode;
	int i, ofs, skip;
	char *cp, *buf;

	i = countfield(line, list -> sep, -1, NULL);
	skip = (max > i) ? max - i : 0;
	buf = malloc2(strlen(line) + 1);

	tmp -> flags = 0;
	tmp -> tmpflags = F_STAT;
	tmp -> st_mode = 0;
	getfield(buf, line, skip, list, F_NAME);
	if (!*buf) {
		free(buf);
		return(-1);
	}
#if	MSDOS
	for (i = 0; buf[i]; i++) if (buf[i] == '/') buf[i] = _SC_;
#endif
	if (isdelim(buf, i = (int)strlen(buf) - 1)) {
		if (i > 0) buf[i] = '\0';
		tmp -> st_mode |= S_IFDIR;
	}
	tmp -> name = strdup2(buf);

	getfield(buf, line, skip, list, F_MODE);
	for (i = ofs = strlen(buf); i < 9; i++) buf[i] = '\0';
	if ((ofs -= 9) < 0) ofs = 0;
	if (!*buf) mode = 0644;
	else for (i = 0, mode = 0; i < 9; i++) {
		mode *= 2;
		if (buf[ofs + i] && buf[ofs + i] != '-') mode |= 1;
	}
	tmp -> st_mode |= mode;
	if (buf[ofs + 2] == 's') tmp -> st_mode |= S_ISUID;
	if (buf[ofs + 5] == 's') tmp -> st_mode |= S_ISGID;
	if (buf[ofs + 8] == 't') tmp -> st_mode |= S_ISVTX;

	if (ofs <= 0) i = -1;
	else {
		for (i = 0; typesymlist[i]; i++)
			if (buf[ofs - 1] == typesymlist[i]) break;
	}
	if (i >= 0 && typesymlist[i]) {
		tmp -> st_mode &= ~S_IFMT;
		tmp -> st_mode |= typelist[i];
	}
	else if (!(tmp -> st_mode & S_IFMT)) tmp -> st_mode |= S_IFREG;
	if ((tmp -> st_mode & S_IFMT) == S_IFDIR) tmp -> flags |= F_ISDIR;
	else if ((tmp -> st_mode & S_IFMT) == S_IFLNK) tmp -> flags |= F_ISLNK;

#if	!MSDOS
	getfield(buf, line, skip, list, F_UID);
	if ((cp = evalnumeric(buf, &n, -1)) && !*cp) tmp -> st_uid = n;
	else tmp -> st_uid = (up = finduid(0, buf)) ? up -> uid : (uid_t)-1;
	getfield(buf, line, skip, list, F_GID);
	if ((cp = evalnumeric(buf, &n, -1)) && !*cp) tmp -> st_gid = n;
	else tmp -> st_gid = (gp = findgid(0, buf)) ? gp -> gid : (gid_t)-1;
#endif
	getfield(buf, line, skip, list, F_SIZE);
	tmp -> st_size = ((cp = evalnumeric(buf, &n, 0)) && !*cp) ? n : 0L;

	getfield(buf, line, skip, list, F_MON);
	if (!strncmp(buf, "Jan", 3)) tm.tm_mon = 0;
	else if (!strncmp(buf, "Feb", 3)) tm.tm_mon = 1;
	else if (!strncmp(buf, "Mar", 3)) tm.tm_mon = 2;
	else if (!strncmp(buf, "Apr", 3)) tm.tm_mon = 3;
	else if (!strncmp(buf, "May", 3)) tm.tm_mon = 4;
	else if (!strncmp(buf, "Jun", 3)) tm.tm_mon = 5;
	else if (!strncmp(buf, "Jul", 3)) tm.tm_mon = 6;
	else if (!strncmp(buf, "Aug", 3)) tm.tm_mon = 7;
	else if (!strncmp(buf, "Sep", 3)) tm.tm_mon = 8;
	else if (!strncmp(buf, "Oct", 3)) tm.tm_mon = 9;
	else if (!strncmp(buf, "Nov", 3)) tm.tm_mon = 10;
	else if (!strncmp(buf, "Dec", 3)) tm.tm_mon = 11;
	else if ((i = atoi2(buf)) >= 1 && i < 12) tm.tm_mon = i - 1;
	else tm.tm_mon = 0;
	getfield(buf, line, skip, list, F_DAY);
	tm.tm_mday = ((i = atoi2(buf)) >= 1 && i < 31) ? i : 1;
	getfield(buf, line, skip, list, F_YEAR);
	if (!*buf) tm.tm_year = 1970;
	else if ((cp = evalnumeric(buf, &n, 0)) && !*cp) tm.tm_year = n;
	else if (list -> field[F_YEAR] == list -> field[F_TIME]
	&& strchr(buf, ':')) {
		tm.tm_year = today[0];
		if (tm.tm_mon > today[1]
		|| (tm.tm_mon == today[1] && tm.tm_mday > today[2]))
			tm.tm_year--;
	}
	else tm.tm_year = 1970;
	if (tm.tm_year < 1900 && (tm.tm_year += 1900) < 1970)
		tm.tm_year += 100;
	tm.tm_year -= 1900;

	getfield(buf, line, skip, list, F_TIME);
	if (!(cp = evalnumeric(buf, &n, 0)) || n > 23)
		tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	else {
		tm.tm_hour = n;
		if (*cp != ':' || !(cp = evalnumeric(++cp, &n, 0)) || n > 59)
			tm.tm_min = tm.tm_sec = 0;
		else {
			tm.tm_min = n;
			if (*cp != ':' || (n = atoi2(++cp)) < 0 || n > 59)
				tm.tm_sec = 0;
			else tm.tm_sec = n;
		}
	}

	tmp -> st_mtim = timelocal2(&tm);
	tmp -> flags |=
#if	MSDOS
		logical_access(tmp -> st_mode);
#else
		logical_access(tmp -> st_mode, tmp -> st_uid, tmp -> st_gid);
#endif

	free(buf);
	return(0);
}
#endif	/* FD < 2 */

VOID archbar(file, dir)
char *file, *dir;
{
	char *arch;

	arch = malloc2(strlen(fullpath) + strlen(file) + strlen(dir) + 3);
	strcpy(strcpy2(strcatdelim2(arch, fullpath, file), ":"), dir);

	locate(0, LPATH);
	putterm(l_clear);
	putch2(' ');
	putterm(t_standout);
	cputs2("Arch:");
	putterm(end_standout);
	kanjiputs2(arch, n_column - 6, 0);
	free(arch);

	tflush();
}

static int NEAR dircmp(s1, s2)
char *s1, *s2;
{
	int i, j;

	for (i = j = 0; s2[j]; i++, j++) {
		if (s1[i] == s2[j]) {
#if	MSDOS
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

static int NEAR dirmatchlen(s1, s2)
char *s1, *s2;
{
	int i, j;

	for (i = j = 0; s1[i]; i++, j++) {
		if (s1[i] == s2[j]) {
#if	MSDOS
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
		for (j = 0; s1[j] == _SC_; j++);
		return((i == j) ? 1 : 0);
	}
	if (i && s2[j]) while (s2[j + 1] == _SC_) j++;
	return(j);
}

static char *NEAR pseudodir(namep)
namelist *namep;
{
	char *cp, *tmp;
	int i, len;
	u_short ent;

	cp = namep -> name;
	while ((tmp = strdelim(cp, 0))) {
		while (*(tmp + 1) == _SC_) tmp++;
		if (!*(tmp + 1)) break;
		len = tmp - (namep -> name);
		if (!len) len++;
		for (i = 0; i < maxfile; i++) {
			if (isdir(&(filelist[i]))
			&& len == dirmatchlen(filelist[i].name, namep -> name))
				break;
		}
		if (i >= maxfile) return(strdupcpy(namep -> name, len));
		if (strncmp(filelist[i].name, namep -> name, len)) {
			filelist[i].name = realloc2(filelist[i].name, len + 1);
			strncpy2(filelist[i].name, namep -> name, len);
		}
		cp = tmp + 1;
	}
	if (isdir(namep)) for (i = 0; i < maxfile; i++) {
		if (isdir(&(filelist[i]))
		&& !dircmp(filelist[i].name, namep -> name)) {
			tmp = filelist[i].name;
			ent = filelist[i].ent;
			memcpy((char *)&(filelist[i]), (char *)namep,
				sizeof(namelist));
			filelist[i].name = tmp;
			filelist[i].ent = ent;
			return(NULL);
		}
	}
	return(namep -> name);
}

static int NEAR readarchive(file, list)
char *file;
launchtable *list;
{
	namelist tmp;
	FILE *fp;
	char *cp, *buf, *dir, *line;
	int i, c, no, len;
#if	FD >= 2
	char *form;
	int skip;
#else
	int max;
#endif

	if (!(cp = evalcommand(list -> comm, file, NULL, 1))) return(-1);
	waitmes();
	locate(0, n_line - 1);
	tflush();
#ifdef	_NOORIGSHELL
# ifdef	DEBUG
	_mtrace_file = "popen(start)";
	fp = Xpopen(cp, "r");
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "popen(end)";
		malloc(0);	/* dummy malloc */
	}
# else
	fp = Xpopen(cp, "r");
# endif
#else	/* !_NOORIGSHELL */
	fp = dopopen(cp);
#endif	/* !_NOORIGSHELL */
	free(cp);
	if (!fp) {
		warning(-1, list -> comm);
		return(-1);
	}

#if	FD < 2
	max = 0;
	for (i = 0; i < MAXLAUNCHFIELD; i++)
		if (list -> field[i] < 255 && (int)(list -> field[i]) > max)
			max = (int)(list -> field[i]);
#endif

	for (i = 0; i < (int)(list -> topskip); i++) {
		if (!(line = fgets2(fp, 0))) break;
		free(line);
	}

	maxfile = 0;
	addlist();
	filelist[0].name = strdup2("..");
	filelist[0].flags = F_ISDIR;
	filelist[0].tmpflags = F_STAT;
	maxfile++;
	tmp.st_nlink = 1;
#ifdef	HAVEFLAGS
	tmp.st_flags = 0;
#endif

#if	FD >= 2
	skip = 0;
#else
	i = 0;
#endif
	no = len = 0;
	buf = NULL;
#if	FD >= 2
	cp = form = decodestr(list -> format, NULL, 0);
#endif
	while ((line = fgets2(fp, 0))) {
		if (intrkey()) {
			dopclose(fp);
#if	FD >= 2
			free(form);
#endif
			free(line);
			if (buf) free(buf);
			for (i = 0; i < maxfile; i++) free(filelist[i].name);
			return(-1);
		}

		no++;
		c = strlen(line);
		if (!buf) buf = line;
		else {
			buf = realloc2(buf, len + c + 1 + 1);
			buf[len++] = LINESEP;
			memcpy(&(buf[len]), line, c);
			free(line);
		}
		len += c;

#if	FD >= 2
		if ((cp = strchr(cp, '\n'))) {
			cp++;
			continue;
		}

		cp = form;
		buf[len] = '\0';
		len = 0;

		if (!(c = readfileent(&tmp, buf, form, skip))) {
			for (skip = 0; skip < AUTOFORMATSIZ; skip++) {
				cp = autoformat[skip];
				if ((c = readfileent(&tmp, buf, cp, 0)))
					break;
			}
			if (skip < AUTOFORMATSIZ) {
				free(form);
				cp = form = strdup2(cp);
				skip = 0;
			}
			else for (skip = 1; ; skip++)
				if ((c = readfileent(&tmp, buf, form, skip)))
					break;
		}
#else	/* FD < 2 */
		if (++i < list -> lines) continue;

		buf[len] = '\0';
		i = len = 0;

		c = readfileent(&tmp, buf, list, max);
#endif	/* FD < 2 */
		free(buf);
		buf = NULL;
		if (c < 0) continue;
		while ((dir = pseudodir(&tmp)) && dir != tmp.name) {
			addlist();
			memcpy((char *)&(filelist[maxfile]), (char *)&tmp,
				sizeof(namelist));
			filelist[maxfile].st_mode &= ~S_IFMT;
			filelist[maxfile].st_mode |= S_IFDIR;
			filelist[maxfile].st_size = 0;
			filelist[maxfile].flags |= F_ISDIR;
			filelist[maxfile].name = dir;
			filelist[maxfile].ent = no;
			maxfile++;
		}
		if (!dir) {
			free(tmp.name);
			continue;
		}

		addlist();
		memcpy((char *)&(filelist[maxfile]), (char *)&tmp,
			sizeof(namelist));
		filelist[maxfile].ent = no;
		maxfile++;
	}
#if	FD >= 2
	free(form);
#endif

	no -= (int)(list -> bottomskip);
	for (i = maxfile - 1; i > 0; i--) {
		if (filelist[i].ent <= no) break;
		free(filelist[i].name);
	}
	maxfile = i + 1;
	for (i = 0; i < maxfile; i++) filelist[i].ent = i;
	if (maxfile <= 1) {
		maxfile = 0;
		free(filelist[0].name);
		filelist[0].name = NOFIL_K;
		filelist[0].st_nlink = -1;
		filelist[0].flags = 0;
		filelist[0].tmpflags = F_STAT;
	}

	if ((i = dopclose(fp))) {
		if (i > 0) {
			locate(0, n_line - 1);
			cputs2("\r\n");
			tflush();
			hideclock = 1;
			warning(0, UNPNG_K);
		}
		for (i = 0; i < maxfile; i++) free(filelist[i].name);
		return(-1);
	}
	return(maxfile);
}

VOID copyarcf(re, arcre)
reg_t *re;
char *arcre;
{
	char *cp, *tmp;
	int i, j, len;

	for (i = 1; i < maxarcf; i++) {
		if (!*archivedir) len = 0;
		else if (!(len = dirmatchlen(archivedir, arcflist[i].name)))
			continue;

		cp = arcflist[i].name + len;
		if (len > 0 && (len > 1 || *archivedir != _SC_)) cp++;
		if (!arcflist[i].name[len]) {
			memcpy((char *)&(filelist[0]),
				(char *)&(arcflist[i]), sizeof(namelist));
			filelist[0].name = arcflist[0].name;
			continue;
		}
		if ((tmp = strdelim(cp, 0))) while (*(++tmp) == _SC_);
		if ((!tmp || !*tmp)
		&& (!re || regexp_exec(re, cp, 1))
		&& (!arcre || searcharc(arcre, arcflist, maxarcf, i))) {
			for (j = 0; j < stackdepth; j++)
				if (!strcmp(cp, filestack[j].name)) break;
			if (j < stackdepth) continue;

			memcpy((char *)&(filelist[maxfile]),
				(char *)&(arcflist[i]), sizeof(namelist));
			filelist[maxfile].name = cp;
			if (isfile(&(filelist[maxfile])))
				totalsize +=
					getblock(filelist[maxfile].st_size);
			maxfile++;
		}
	}
}

static char *NEAR searcharcdir(file, flen)
char *file;
int flen;
{
	char *cp, *tmp;
	int i, len;

	errno = ENOENT;
	for (i = 1; i < maxarcf; i++) {
		if (!*archivedir) len = 0;
		else if (!(len = dirmatchlen(archivedir, arcflist[i].name)))
			continue;

		cp = arcflist[i].name + len;
		if (len > 0 && (len > 1 || *archivedir != _SC_)) cp++;
		if (!arcflist[i].name[len]) {
			if (file) continue;
		}
		else {
			if (!file) continue;
			if (!(tmp = strdelim(cp, 0))) len = strlen(cp);
			else {
				len = (tmp == cp) ? 1 : tmp - cp;
				while (*(++tmp) == _SC_);
			}
			if ((tmp && *tmp) || len != flen
			|| strnpathcmp(file, cp, flen))
				continue;
			if (!isdir(&(arcflist[i]))) {
				errno = ENOTDIR;
				continue;
			}
		}
		return(arcflist[i].name);
	}
	return(NULL);
}

char *archchdir(path)
char *path;
{
	char *cp, *file, duparcdir[MAXPATHLEN];
	int len;

	if (findpattern) free(findpattern);
	findpattern = NULL;
	if (!path || !*path) path = "..";
	strcpy(duparcdir, archivedir);
	do {
		if (*path == _SC_) len = 1;
		else if ((cp = strdelim(path, 0))) len = cp - path;
		else len = strlen(path);
		if (len != 2 || strncmp(path, "..", len)) {
			if (!searcharcdir(path, len)) {
				strcpy(archivedir, duparcdir);
				return(NULL);
			}
			if (*(cp = archivedir)) cp = strcatdelim(archivedir);
			strncpy2(cp, path, len);
			file = "..";
		}
		else if (!*archivedir) return((char *)-1);
		else {
			if (!(file = searcharcdir(NULL, 0))) {
				strcpy(archivedir, duparcdir);
				return(NULL);
			}
			cp = file + (int)strlen(file) - 1;
			while (cp > file && *cp == _SC_) cp--;
#if	MSDOS
			if (onkanji1(file, cp - file)) cp++;
#endif
			cp = strrdelim2(file, cp);
			if (!cp) *archivedir = '\0';
			else {
				if (cp == file) strcpy(archivedir, _SS_);
				else strncpy2(archivedir, file, cp - file);
				file = cp + 1;
			}
		}
		path += len;
		while (*path == _SC_) path++;
	} while (*path);

	return(file);
}

#ifndef	_NOCOMPLETE
int completearch(path, flen, argc, argvp)
char *path;
int flen, argc;
char ***argvp;
{
	char *cp, *tmp, *new, *file, dir[MAXPATHLEN], duparcdir[MAXPATHLEN];
	int i, len;

#if	MSDOS || !defined (_NODOSDRIVE)
	if (_dospath(path)) return(argc);
#endif

	strcpy(duparcdir, archivedir);
	if (!(file = strrdelim(path, 0))) file = path;
	else {
		strncpy2(dir, path, (file == path) ? 1 : file - path);
		if (!(cp = archchdir(dir)) || cp == (char *)-1) {
			strcpy(archivedir, duparcdir);
			return(argc);
		}
		flen -= ++file - path;
	}

	for (i = 1; i < maxarcf; i++) {
		if (!*archivedir) len = 0;
		else if (!(len = dirmatchlen(archivedir, arcflist[i].name)))
			continue;

		cp = arcflist[i].name + len;
		if (len > 0 && (len > 1 || *archivedir != _SC_)) cp++;
		if (!arcflist[i].name[len]) continue;

		if (!(tmp = strdelim(cp, 0))) len = strlen(cp);
		else {
			len = (tmp == cp) ? 1 : tmp - cp;
			while (*(++tmp) == _SC_);
		}
		if ((tmp && *tmp) || strnpathcmp(file, cp, flen)) continue;

		new = malloc2(len + 1 + 1);
		strncpy(new, cp, len);
		if (isdir(&(arcflist[i]))) new[len++] = _SC_;
		new[len] = '\0';
		if (finddupl(new, argc, *argvp)) {
			free(new);
			continue;
		}

		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = new;
	}
	strcpy(archivedir, duparcdir);
	return(argc);
}
#endif	/* !_NOCOMPLETE */

int launcher(VOID_A)
{
	reg_t *re;
	char *dir;
	int i, n;
#ifndef	_NODOSDRIVE
	int drive;
#endif
#if	!MSDOS
	int dupignorecase;
#endif

#ifndef	_NODOSDRIVE
	drive = 0;
#endif
	for (i = 0; i < maxlaunch; i++) {
#if	!MSDOS
		dupignorecase = pathignorecase;
		if (launchlist[i].flags & LF_IGNORECASE) pathignorecase = 1;
#endif
		re = regexp_init(launchlist[i].ext, -1);
		n = regexp_exec(re, filelist[filepos].name, 0);
		regexp_free(re);
#if	!MSDOS
		pathignorecase = dupignorecase;
#endif
		if (n) break;
	}
	if (i >= maxlaunch) return(-1);

	dir = NULL;
	if ((archivefile && !(dir = tmpunpack(1)))
#ifndef	_NODOSDRIVE
	|| (drive = tmpdosdupl("", &dir, 1)) < 0
#endif
	) {
		putterm(t_bell);
		return(1);
	}
	if (!isarchbr(&(launchlist[i]))) {
		execusercomm(launchlist[i].comm, filelist[filepos].name,
			1, 0, 1);
#ifndef	_NODOSDRIVE
		if (drive) removetmp(dir, NULL, filelist[filepos].name);
		else
#endif
		if (archivefile)
			removetmp(dir, archivedir, filelist[filepos].name);
		return(0);
	}

	pusharchdupl();
#ifndef	_NODOSDRIVE
	if (drive) {
		archduplp -> v_fullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
	}
	else
#endif
	if (archivefile) {
		archduplp -> v_fullpath = strdup2(fullpath);
		strcpy(fullpath, dir);
		if (*archivedir) {
			strcpy(strcatdelim(fullpath), archivedir);
			archduplp -> v_archivedir = strdup2(archivedir);
		}
	}
	archivefile = strdup2(filelist[filepos].name);
	*archivedir = '\0';
	archtmpdir = dir;
#ifndef	_NODOSDRIVE
	archdrive = drive;
#endif
#ifndef	_NOTREE
	treepath = NULL;
#endif
	findpattern = NULL;
	launchp = &(launchlist[i]);
	filelist = NULL;
	maxent = 0;
	sorton = 0;

	if (readarchive(archivefile, launchp) < 0) {
		poparchdupl();
		return(0);
	}

	arcflist = (namelist *)malloc2(((maxfile) ? maxfile : 1)
		* sizeof(namelist));
	for (i = 0; i < maxfile; i++)
		memcpy((char *)&(arcflist[i]), (char *)&(filelist[i]),
			sizeof(namelist));
	maxarcf = maxfile;
	maxfile = 0;
	filepos = -1;

	return(0);
}

static char *NEAR genfullpath(path, file)
char *path, *file;
{
	char *cp;
#if	MSDOS || !defined (_NODOSDRIVE)
	int drive;
#endif

	cp = file;
#if	MSDOS || !defined (_NODOSDRIVE)
	if (_dospath(cp)) cp += 2;
#endif
	if (*cp == _SC_) {
		strcpy(path, file);
		return(path);
	}
#ifndef	_NODOSDRIVE
	else if ((drive = dospath2(fullpath))) {
		if (!deftmpdir || !*deftmpdir
		|| !tmpfilename || !*tmpfilename) {
			warning(ENOENT, file);
			return(NULL);
		}

		realpath2(deftmpdir, path, 1);
		free(deftmpdir);
# if	MSDOS
		if (checkdrive(toupper2(path[0]) - 'A'))
		if (!_Xgetwd(path, 1)) {
			lostcwd(path);
			deftmpdir = NULL;
			return(NULL);
		}
# endif
		deftmpdir = strdup2(path);
		strcpy(strcatdelim(path), tmpfilename);
		cp = strcatdelim(path);
		*(cp++) = 'D';
		*(cp++) = drive;
		*cp = '\0';
	}
#endif	/* !_NODOSDRIVE */
#if	MSDOS
	else if (cp > file
	&& (drive = toupper2(*file)) != toupper2(*fullpath)) {
		path[0] = *file;
		path[1] = ':';
		path[2] = _SC_;
		if (!unixgetcurdir(&(path[3]), drive - 'A' + 1)) return(NULL);
	}
#endif
	else strcpy(path, fullpath);
	strcpy(strcatdelim(path), cp);
	return(path);
}

int pack(arc)
char *arc;
{
	winvartable *wvp;
	reg_t *re;
	char *duparchivefile, path[MAXPATHLEN], dupfullpath[MAXPATHLEN];
	int i, n, ret;
#if	!MSDOS
	int dupignorecase;
#endif

	for (i = 0; i < maxarchive; i++) {
		if (!archivelist[i].p_comm) continue;
#if	!MSDOS
		dupignorecase = pathignorecase;
		if (archivelist[i].flags & AF_IGNORECASE) pathignorecase = 1;
#endif
		re = regexp_init(archivelist[i].ext, -1);
		n = regexp_exec(re, arc, 0);
		regexp_free(re);
#if	!MSDOS
		pathignorecase = dupignorecase;
#endif
		if (n) break;
	}
	if (i >= maxarchive) return(-1);

	strcpy(dupfullpath, fullpath);
	for (wvp = archduplp; wvp; wvp = wvp -> v_archduplp)
		if (wvp -> v_fullpath) strcpy(fullpath, wvp -> v_fullpath);
	if (!genfullpath(path, arc)) {
		strcpy(fullpath, dupfullpath);
		return(0);
	}
	waitmes();
	duparchivefile = archivefile;
	archivefile = NULL;
	locate(0, LMESLINE);
	tflush();
	ret = execusercomm(archivelist[i].p_comm, path, -1, 1, 0);
	if (ret > 0) {
		if (ret < 127) {
			hideclock = 1;
			warning(0, HITKY_K);
		}
		rewritefile(1);
	}
	archivefile = duparchivefile;
	strcpy(fullpath, dupfullpath);
	return((ret) ? 0 : 1);
}

/*ARGSUSED*/
int unpack(arc, dir, arg, tr, ignorelist)
char *arc, *dir;
char *arg;
int tr, ignorelist;
{
	reg_t *re;
	char *tmp, *tmpdir, path[MAXPATHLEN];
	int i, n, ret;
#ifndef	_NODOSDRIVE
	namelist alist[1], *dupfilelist;
	int dd, drive, dupmaxfile, dupfilepos;
#endif
#if	!MSDOS
	int dupignorecase;
#endif

	for (i = 0; i < maxarchive; i++) {
		if (!archivelist[i].u_comm) continue;
#if	!MSDOS
		dupignorecase = pathignorecase;
		if (archivelist[i].flags & AF_IGNORECASE) pathignorecase = 1;
#endif
		re = regexp_init(archivelist[i].ext, -1);
		n = regexp_exec(re, arc, 0);
		regexp_free(re);
#if	!MSDOS
		pathignorecase = dupignorecase;
#endif
		if (n) break;
	}
	if (i >= maxarchive) return(-1);

	tmpdir = NULL;
	if (dir) strcpy(path, dir);
	else {
#ifndef	_NOTREE
		if (tr) {
# ifdef	_NODOSDRIVE
			dir = tree(0, (int *)1);
# else
			dir = tree(0, &dd);
			if (dd >= 0) shutdrv(dd);
# endif
		}
		else
#endif	/* !_NOTREE */
		{
			if (arg && *arg) dir = strdup2(arg);
			else dir = inputstr(UNPAC_K, 1, -1, NULL, 1);
			dir = evalpath(dir, 1);
		}
		if (!dir) return(0);
		if (!*dir) strcpy(path, ".");
		else {
			tmp = strcpy2(path, dir);
#if	MSDOS || !defined (_NODOSDRIVE)
			if (_dospath(dir) && !dir[2]) strcpy(tmp, ".");
#endif
		}
		free(dir);
		dir = NULL;
	}
#ifndef	_NODOSDRIVE
	if (dospath2(path)) {
		warning(ENOENT, path);
		return(0);
	}

	dupfilelist = filelist;
	dupmaxfile = maxfile;
	dupfilepos = filepos;
	alist[0].name = arc;
	alist[0].st_mode = 0700;
	filelist = alist;
	maxfile = 1;
	filepos = 0;
	drive = tmpdosdupl(arc, &tmpdir, 0);
	filelist = dupfilelist;
	maxfile = dupmaxfile;
	filepos = dupfilepos;
	if (drive < 0) {
		warning(-1, arc);
		return(0);
	}
#endif
	if ((!dir && preparedir(path) < 0) || _chdir2(path) < 0) {
		warning(-1, path);
		return(0);
	}

	if (!genfullpath(path, arc)) return(0);
	waitmes();
	locate(0, LMESLINE);
	tflush();
	ret = execusercomm(archivelist[i].u_comm, path, -1, 1, ignorelist);
	if (ret > 0) {
		if (ret < 127) {
			hideclock = 1;
			warning(0, HITKY_K);
		}
		rewritefile(1);
	}
	if (tmpdir) removetmp(tmpdir, NULL, arc);
	if (_chdir2(fullpath) < 0) lostcwd(fullpath);
	return((ret) ? 0 : 1);
}

char *tmpunpack(single)
int single;
{
	winvartable *wvp;
	char *subdir, path[MAXPATHLEN];
	int i, ret, dupmark;

	for (i = 0, wvp = archduplp; wvp; i++, wvp = wvp -> v_archduplp);
#ifndef	_NOSPLITWIN
	if (windows > 1) sprintf(path, "L%d-%d", i, win);
	else
#endif
	sprintf(path, "L%d", i);
	if (mktmpdir(path) < 0) {
		warning(-1, path);
		return(NULL);
	}

	for (i = 0; i < maxfile; i++)
		if (ismark(&(filelist[i]))) filelist[i].tmpflags |= F_WSMRK;
	dupmark = mark;
	if (single) mark = 0;
	ret = unpack(archivefile, path, NULL, 0, 0);
	mark = dupmark;
	for (i = 0; i < maxfile; i++) {
		if (wasmark(&(filelist[i]))) filelist[i].tmpflags |= F_ISMRK;
		filelist[i].tmpflags &= ~F_WSMRK;
	}

	if (_chdir2(path) < 0) error(path);
	subdir = archivedir;
	while (*subdir == _SC_) subdir++;
	if (ret < 0) putterm(t_bell);
	else if (ret > 0) {
#if	!MSDOS && !defined (_NODOSDRIVE)
		char tmp[MAXPATHLEN];
#endif

		if (*subdir && _chdir2(subdir) < 0) {
			warning(-1, subdir);
			ret = 0;
		}
		else if (single || mark <= 0) {
			if (Xaccess(fnodospath(tmp, filepos), F_OK) < 0)
				warning(-1, filelist[filepos].name);
			else return(strdup2(path));
		}
		else {
			for (i = 0; i < maxfile; i++) {
				if (!ismark(&(filelist[i]))) continue;
				if (Xaccess(fnodospath(tmp, i), F_OK) < 0) {
					warning(-1, filelist[i].name);
					break;
				}
			}
			if (i >= maxfile) return(strdup2(path));
		}
	}
	if (ret <= 0) removetmp(strdup2(path), NULL, NULL);
	else if (single || mark <= 0)
		removetmp(strdup2(path), subdir, filelist[filepos].name);
	else removetmp(strdup2(path), subdir, "");
	return(NULL);
}

int backup(dev)
char *dev;
{
	macrostat st;
	char *tmp;
	int i;

	waitmes();
	for (i = 0; i < maxfile; i++)
		if (ismark(&(filelist[i]))) filelist[i].tmpflags |= F_ISARG;
	n_args = mark;

	st.flags = 0;
	if (!(tmp = evalcommand("tar cf %C %TA", dev, &st, 0))) return(0);
	for (;;) {
		system2(tmp, -1);
		free(tmp);
		if (!(st.flags & F_REMAIN)
		|| !(tmp = evalcommand("tar rf %C %TA", dev, NULL, 0)))
			break;
	}
	for (i = 0; i < maxfile; i++)
		filelist[i].tmpflags &= ~(F_ISARG | F_ISMRK);
	return(1);
}

int searcharc(regstr, flist, maxf, n)
char *regstr;
namelist *flist;
int maxf, n;
{
	namelist tmp;
	launchtable *list;
	namelist *dupfilelist;
	reg_t *re;
	FILE *fp;
	char *cp, *buf, *dir, *file, *line;
	int i, c, no, len, match, dupmaxfile, dupfilepos;
#if	FD >= 2
	char *form;
	int skip;
#else
	int max;
#endif
#if	!MSDOS
	int dupignorecase;
#endif

	if (n < 0) {
		file = flist -> name;
		if (isdir(flist)) return(0);
	}
	else {
		file = flist[n].name;
		if (isdir(&(flist[n]))) return(0);
	}

	for (i = 0; i < maxlaunch; i++) {
		if (!isarchbr(&(launchlist[i]))) continue;
#if	!MSDOS
		dupignorecase = pathignorecase;
		if (launchlist[i].flags & LF_IGNORECASE) pathignorecase = 1;
#endif
		re = regexp_init(launchlist[i].ext, -1);
		c = regexp_exec(re, file, 0);
		regexp_free(re);
#if	!MSDOS
		pathignorecase = dupignorecase;
#endif
		if (c) break;
	}
	if (i >= maxlaunch) return(0);

	list = &(launchlist[i]);
	if (n >= 0) {
		dupfilelist = filelist;
		dupmaxfile = maxfile;
		dupfilepos = filepos;
		filelist = flist;
		maxfile = maxf;
		filepos = n;
		dir = tmpunpack(1);
		filelist = dupfilelist;
		maxfile = dupmaxfile;
		filepos = dupfilepos;
		if (!dir) return(0);
	}

	if (!(cp = evalcommand(list -> comm, file, NULL, 1))) {
		if (n >= 0) removetmp(dir, NULL, file);
		return(0);
	}
	locate(0, n_line - 1);
	tflush();
#ifdef	_NOORIGSHELL
# ifdef	DEBUG
	_mtrace_file = "popen(start)";
	fp = Xpopen(cp, "r");
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "popen(end)";
		malloc(0);	/* dummy malloc */
	}
# else
	fp = Xpopen(cp, "r");
# endif
#else	/* !_NOORIGSHELL */
	fp = dopopen(cp);
#endif	/* !_NOORIGSHELL */
	free(cp);
	if (!fp) {
		warning(-1, list -> comm);
		return(-1);
	}

	locate(0, LMESLINE);
	putterm(l_clear);
	putterm(t_standout);
	kanjiputs(SEACH_K);
	putterm(end_standout);
	kanjiputs(file);
	tflush();

#if	FD < 2
	max = 0;
	for (i = 0; i < MAXLAUNCHFIELD; i++)
		if (list -> field[i] < 255 && (int)(list -> field[i]) > max)
			max = (int)(list -> field[i]);
#endif

	for (i = 0; i < (int)(list -> topskip); i++) {
		if (!(line = fgets2(fp, 0))) break;
		free(line);
	}

#if	FD >= 2
	skip = 0;
#else
	i = 0;
#endif
	no = len = match = 0;
	buf = NULL;
#if	FD >= 2
	cp = form = decodestr(list -> format, NULL, 0);
#endif
	re = regexp_init(regstr, -1);
	while ((line = fgets2(fp, 0))) {
		if (match && match <= no - (int)(list -> bottomskip)) {
			free(line);
			if (buf) free(buf);
			break;
		}
		if (intrkey()) {
			dopclose(fp);
#if	FD >= 2
			free(form);
#endif
			free(line);
			if (buf) free(buf);
			regexp_free(re);
			if (n >= 0) removetmp(dir, NULL, file);
			return(-1);
		}

		no++;
		c = strlen(line);
		if (!buf) buf = line;
		else {
			buf = realloc2(buf, len + c + 1 + 1);
			buf[len++] = LINESEP;
			memcpy(&(buf[len]), line, c);
			free(line);
		}
		len += c;

#if	FD >= 2
		if ((cp = strchr(cp, '\n'))) {
			cp++;
			continue;
		}

		cp = form;
		buf[len] = '\0';
		len = 0;

		c = readfileent(&tmp, buf, form, skip);
		if (!c && !skip) {
			do {
				skip++;
				c = readfileent(&tmp, buf, form, skip);
			} while (!c);
		}
#else	/* FD < 2 */
		if (++i < list -> lines) continue;

		buf[len] = '\0';
		i = len = 0;

		c = readfileent(&tmp, buf, list, max);
#endif	/* FD < 2 */
		free(buf);
		buf = NULL;
		if (c < 0) continue;
		if (regexp_exec(re, tmp.name, 1)) match = no;
		free(tmp.name);
	}
#if	FD >= 2
	free(form);
#endif
	regexp_free(re);
	if (n >= 0) removetmp(dir, NULL, file);

	if (dopclose(fp)) return(0);
	if (match > no - (int)(list -> bottomskip)) match = 0;

	return(match);
}
#endif	/* !_NOARCHIVE */
