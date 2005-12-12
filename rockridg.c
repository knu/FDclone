/*
 *	rockridg.c
 *
 *	ISO-9660 RockRidge format filter
 */

#include "fd.h"
#include "func.h"

#ifndef	_NOROCKRIDGE

extern char typesymlist[];
extern u_short typelist[];

#define	TRANSTBLFILE	"TRANS.TBL"
#define	TRANSTBLVAR	1
#define	RR_TRANS	001
#define	RR_LOWER	002
#define	RR_VERNO	004
#define	RR_HYPHN	010

#ifdef	USEMKDEVH
#include <sys/mkdev.h>
#else
# ifdef	USEMKNODH
# include <sys/mknod.h>
# else
#  ifdef	SVR4
#  include <sys/sysmacros.h>
#   ifndef	makedev
#   define	makedev(ma, mi)	(((((unsigned long)(ma)) & 0x3fff) << 18) \
				| (((unsigned long)(mi)) & 0x3ffff))
#   endif
#  else
#   ifndef	makedev
#   define	makedev(ma, mi)	(((((unsigned)(ma)) & 0xff) << 8) \
				| (((unsigned)(mi)) & 0xff))
#   endif
#  endif
# endif
#endif

#if	MSDOS
typedef short	r_dev_t;
#else
typedef dev_t	r_dev_t;
#endif

typedef struct _transtable {
	char *org;
	char *alias;
	char *slink;
	r_dev_t rdev;
	char type;
	struct _transtable *next;
} transtable;

static char *NEAR getorgname __P_((char *, int));
static FILE *NEAR opentranstbl __P_((char *, int, int *));
static transtable *NEAR readtranstbl __P_((char *, int));
static VOID NEAR freetranstbl __P_((transtable *));
static transtable *NEAR inittrans __P_((char *, int));
static VOID NEAR cachetrans __P_((char *, char *));
static char *NEAR transfile __P_((char *, int, char *, int));
static char *NEAR detransfile __P_((char *, int, char *, int));
static char *NEAR detransdir __P_((char *, char *, int));

char *rockridgepath = NULL;
int norockridge = 0;

static transtable *rr_curtbl = NULL;
static char *rr_cwd = NULL;
static char *rr_transcwd = NULL;


static char *NEAR getorgname(name, flags)
char *name;
int flags;
{
	int i;

	for (i = 0; name[i] && name[i] != ';'; i++)
		if (flags & RR_LOWER) name[i] = tolower2(name[i]);

	if ((flags & RR_VERNO) && name[i] == ';') {
		if (flags & RR_HYPHN) name[i] = '-';
		i += 2;
	}
	name[i] = '\0';

	return(name);
}

static FILE *NEAR opentranstbl(path, len, flagsp)
char *path;
int len, *flagsp;
{
	FILE *fp;
	char *cp, *file, buf[MAXPATHLEN];
	int i;

	if (len + 1 >= MAXPATHLEN - 1) return(NULL);
	strncpy2(buf, path, len);
	file = strcatdelim(buf);
	cp = strncpy2(file, TRANSTBLFILE, MAXPATHLEN - 1 - (file - buf));

	*flagsp = RR_TRANS;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	len = cp - buf;
	if (len + 1 >= MAXPATHLEN - 1) return(NULL);
	snprintf2(&(buf[len]), sizeof(buf) - len, ";%d", TRANSTBLVAR);
	*flagsp |= RR_VERNO;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	buf[len] = '-';
	*flagsp |= RR_HYPHN;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	buf[len] = '\0';
	for (i = 0; file[i]; i++) file[i] = tolower2(file[i]);
	*flagsp = RR_TRANS | RR_LOWER;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	buf[len] = ';';
	*flagsp |= RR_VERNO;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	buf[len] = '-';
	*flagsp |= RR_HYPHN;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	return(NULL);
}

static transtable *NEAR readtranstbl(path, len)
char *path;
int len;
{
	transtable *top, **bottom, *new;
	FILE *fp;
	r_dev_t maj, min;
	char *cp, *eol, *org, *line;
	int l1, l2, flags;

	norockridge++;
	fp = opentranstbl(path, len, &flags);
	norockridge--;
	if (!fp) return(NULL);

	top = NULL;
	bottom = &top;
	while ((line = fgets2(fp, 0))) {
		cp = line;
		switch (*cp) {
			case 'F':
			case 'D':
			case 'B':
			case 'C':
			case 'L':
			case 'S':
			case 'P':
				cp++;
				break;
			default:
				*cp = '\0';
				break;
		}
		if (!*(cp = skipspace(cp))) {
			free(line);
			continue;
		}

		for (eol = cp; *eol && *eol != ' ' && *eol != '\t'; eol++);
		if (*eol) *(eol++) = '\0';
		if (!*(eol = skipspace(eol))) {
			free(line);
			continue;
		}

		org = getorgname(cp, flags);
		cp = eol;
		while (*eol && *eol != ' ' && *eol != '\t') eol++;
		l1 = eol - cp;
		if (*eol) *(eol++) = '\0';
		l2 = 0;
		maj = min = (r_dev_t)-1;
		if (*line == 'L') {
			eol = skipspace(eol);
			while (eol[l2] && eol[l2] != ' ' && eol[l2] != '\t')
				l2++;
			eol[l2++] = '\0';
		}
		else if (*line == 'B' || *line == 'C') {
			eol = skipspace(eol);
			eol = sscanf2(eol, "%+*d", sizeof(r_dev_t), &maj);
			if (eol) {
				eol = skipspace(eol);
				eol = sscanf2(eol, "%+*d",
					sizeof(r_dev_t), &min);
				if (!eol) maj = (r_dev_t)-1;
			}
		}

		new = (transtable *)malloc2(sizeof(transtable));
		new -> org = strdup2(org);
		new -> alias = strndup2(cp, l1);
		new -> slink = (l2 > 0) ? strndup2(eol, l2 - 1) : NULL;
		new -> type = *line;
		new -> rdev = (maj >= (r_dev_t)0)
			? makedev(maj, min) : (r_dev_t)-1;

		*bottom = new;
		new -> next = NULL;
		bottom = &(new -> next);
		free(line);
	}

	if (top) *bottom = top;
	Xfclose(fp);

	return(top);
}

static VOID NEAR freetranstbl(tbl)
transtable *tbl;
{
	transtable *tp;

	if (tbl) {
		tp = tbl -> next;
		tbl -> next = NULL;
		while (tp) {
			tbl = tp;
			tp = tbl -> next;
			free(tbl -> org);
			free(tbl -> alias);
			if (tbl -> slink) free(tbl -> slink);
			free(tbl);
		}
	}
}

static transtable *NEAR inittrans(path, len)
char *path;
int len;
{
	transtable *tp;

	if (len < 0) len = strlen(path);
	if (rr_cwd && !strnpathcmp(path, rr_cwd, len) && !rr_cwd[len])
		return(rr_curtbl);
	if (!(tp = readtranstbl(path, len))) return(NULL);

	freetranstbl(rr_curtbl);
	rr_curtbl = tp;
	if (rr_cwd) free(rr_cwd);
	rr_cwd = strndup2(path, len);
	if (rr_transcwd) free(rr_transcwd);
	rr_transcwd = NULL;

	return(rr_curtbl);
}

static VOID NEAR cachetrans(path, trans)
char *path, *trans;
{
	char *cp1, *cp2;
	int len;

	if (!rr_cwd) return;
	cp1 = path;
	cp2 = trans;
	len = strlen(rr_cwd);
	for (;;) {
		if (!(cp1 = strdelim(cp1, 0)) || cp1 - path > len) return;
		if (!(cp2 = strdelim(cp2, 0))) return;
		if (cp1 - path == len) break;
		cp1++;
		cp2++;
	}
	if (strnpathcmp(path, rr_cwd, len)) return;
	if (rr_transcwd) free(rr_transcwd);
	rr_transcwd = strndup2(trans, cp2 - trans);
}

static char *NEAR transfile(file, len, buf, ptr)
char *file;
int len;
char *buf;
int ptr;
{
	transtable *tp;

	if (len < 0) len = strlen(file);
	tp = rr_curtbl;
	for (;;) {
		if (!strnpathcmp(file, tp -> org, len) && !(tp -> org[len])) {
			rr_curtbl = tp;
			buf = strncpy2(&(buf[ptr]), tp -> alias,
				MAXPATHLEN - 1 - ptr);
			return(buf);
		}
		if ((tp = tp -> next) == rr_curtbl) break;
	}

	return(NULL);
}

char *transpath(path, buf)
char *path, *buf;
{
	char *cp, *tmp, *next;
	int ptr, len, dlen;

	if (!(cp = includepath(path, rockridgepath)) || !*cp) return(path);

	if (!rr_cwd || !rr_transcwd
	|| strnpathcmp(path, rr_cwd, len = strlen(rr_cwd))) {
		ptr = cp - path;
		strncpy2(buf, path, ptr);
	}
	else {
		strcpy(buf, rr_transcwd);
		if (!path[len]) return(buf);
		ptr = strlen(buf);
		cp = path + len;
	}

	while (cp) {
		dlen = cp++ - path;
		buf[ptr++] = _SC_;
		if ((next = strdelim(cp, 0))) len = next - cp;
		else len = strlen(cp);

		if (!inittrans(path, dlen)) tmp = NULL;
		else tmp = transfile(cp, len, buf, ptr);
		if (!tmp) tmp = strncpy2(&(buf[ptr]), cp, len);
		ptr = tmp - buf;
		cp = next;
	}
	cachetrans(path, buf);

	return(buf);
}

static char *NEAR detransfile(file, len, buf, ptr)
char *file;
int len;
char *buf;
int ptr;
{
	transtable *tp;

	if (len < 0) len = strlen(file);
	tp = rr_curtbl;
	for (;;) {
		if (!strnpathcmp(file, tp -> alias, len)
		&& !(tp -> alias[len])) {
			rr_curtbl = tp;
			buf = strncpy2(&(buf[ptr]), tp -> org,
				MAXPATHLEN - 1 - ptr);
			return(buf);
		}
		if ((tp = tp -> next) == rr_curtbl) break;
	}

	return(NULL);
}

static char *NEAR detransdir(dir, buf, ptr)
char *dir, *buf;
int ptr;
{
	char *cp, *next;
	int dlen, flen;

	while (dir) {
		dlen = ptr;
		buf[ptr++] = _SC_;
		if ((next = strdelim(dir, 0))) flen = next++ - dir;
		else flen = strlen(dir);

		if (!inittrans(buf, dlen)) cp = NULL;
		else cp = detransfile(dir, flen, buf, ptr);
		if (!cp) cp = strncpy2(&(buf[ptr]), dir, flen);
		ptr = cp - buf;
		dir = next;
	}

	return(&(buf[ptr]));
}

char *detranspath(path, buf)
char *path, *buf;
{
	char *cp;
	int ptr, len;

	if (!path) {
		freetranstbl(rr_curtbl);
		rr_curtbl = NULL;
		if (rr_cwd) free(rr_cwd);
		rr_cwd = NULL;
		if (rr_transcwd) free(rr_transcwd);
		rr_transcwd = NULL;
		return(NULL);
	}

	if (!(cp = includepath(path, rockridgepath)) || !*cp) return(path);

	if (!rr_transcwd || !rr_cwd
	|| strnpathcmp(path, rr_transcwd, len = strlen(rr_transcwd))) {
		ptr = cp++ - path;
		strncpy2(buf, path, ptr);
	}
	else {
		strcpy(buf, rr_cwd);
		if (!path[len]) return(buf);
		ptr = strlen(buf);
		cp = path + len + 1;
	}

	detransdir(cp, buf, ptr);
	cachetrans(buf, path);

	return(buf);
}

int rrlstat(path, stp)
char *path;
struct stat *stp;
{
	transtable *tp;
	char *cp;
	u_short mode;
	int i, n;

	if (!(cp = strrdelim(path, 0)) || !inittrans(path, cp++ - path))
		return(-1);

	tp = rr_curtbl;
	for (;;) {
		if (!strpathcmp(cp, tp -> org)) break;
		if ((tp = tp -> next) == rr_curtbl) return(-1);
	}
	rr_curtbl = tp;

	n = tolower2(tp -> type);
	for (i = 0; typesymlist[i]; i++) if (n == typesymlist[i]) break;
	if (!typesymlist[i]) mode = S_IFREG;
	else mode = typelist[i];

	stp -> st_mode &= ~S_IFMT;
	stp -> st_mode |= mode;
	if ((mode == S_IFBLK || mode == S_IFCHR) && tp -> rdev != (r_dev_t)-1)
		stp -> st_rdev = tp -> rdev;

	return(0);
}

int rrreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
	transtable *tp;
	char *cp;
	int len;

	if (!(cp = strrdelim(path, 0)) || !inittrans(path, cp++ - path))
		return(-1);

	tp = rr_curtbl;
	for (;;) {
		if (!strpathcmp(cp, tp -> org)) break;
		if ((tp = tp -> next) == rr_curtbl) return(-1);
	}
	rr_curtbl = tp;

	if (tp -> type != 'L' || !(tp -> slink)) return(-1);
	len = strlen(tp -> slink);
	strncpy(buf, tp -> slink, bufsiz);
	if (len > bufsiz) len = bufsiz;

	return(len);
}
#endif	/* !_NOROCKRIDGE */
