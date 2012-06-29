/*
 *	rockridg.c
 *
 *	ISO-9660 RockRidge format filter
 */

#include "fd.h"
#include "device.h"
#include "parse.h"
#include "func.h"

#define	TRANSTBLFILE		"TRANS.TBL"
#define	TRANSTBLVAR		1
#define	RR_TRANS		001
#define	RR_LOWER		002
#define	RR_VERNO		004
#define	RR_HYPHN		010

typedef struct _transtable {
	char *org;
	char *alias;
	char *slink;
	r_dev_t rdev;
	char type;
	struct _transtable *next;
} transtable;

#ifndef	_NOROCKRIDGE

static char *NEAR getorgname __P_((char *, int));
static XFILE *NEAR opentranstbl __P_((CONST char *, int, int *));
static transtable *NEAR readtranstbl __P_((CONST char *, int));
static VOID NEAR freetranstbl __P_((transtable *));
static transtable *NEAR inittrans __P_((CONST char *, int));
static VOID NEAR cachetrans __P_((CONST char *, CONST char *));
static char *NEAR transfile __P_((CONST char *, int, char *, int));
static char *NEAR detransfile __P_((CONST char *, int, char *, int));
static char *NEAR detransdir __P_((CONST char *, char *, int));

char *rockridgepath = NULL;
int norockridge = 0;

static transtable *rr_curtbl = NULL;
static char *rr_cwd = NULL;
static char *rr_transcwd = NULL;


static char *NEAR getorgname(name, flags)
char *name;
int flags;
{
	char *cp;
	int len;

	cp = Xstrchr(name, ';');
	len = (cp) ? cp - name : strlen(name);
	if (flags & RR_LOWER) Xstrntolower(name, len);

	if ((flags & RR_VERNO) && cp) {
		if (flags & RR_HYPHN) *cp = '-';
		cp += 2;
	}
	*cp = '\0';

	return(name);
}

static XFILE *NEAR opentranstbl(path, len, flagsp)
CONST char *path;
int len, *flagsp;
{
	XFILE *fp;
	char *cp, *file, buf[MAXPATHLEN];

	if (len + 1 >= MAXPATHLEN - 1) return(NULL);
	Xstrncpy(buf, path, len);
	file = strcatdelim(buf);
	cp = Xstrncpy(file, TRANSTBLFILE, MAXPATHLEN - 1 - (file - buf));

	*flagsp = RR_TRANS;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	len = cp - buf;
	if (len + 1 >= MAXPATHLEN - 1) return(NULL);
	VOID_C Xsnprintf(&(buf[len]), (int)sizeof(buf) - len,
		";%d", TRANSTBLVAR);
	*flagsp |= RR_VERNO;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	buf[len] = '-';
	*flagsp |= RR_HYPHN;
	if ((fp = Xfopen(buf, "r"))) return(fp);

	buf[len] = '\0';
	Xstrtolower(file);
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
CONST char *path;
int len;
{
	transtable *top, **bottom, *new;
	XFILE *fp;
	r_dev_t maj, min;
	char *cp, *eol, *org, *line;
	int l1, l2, flags;

	norockridge++;
#ifdef	FAKEUNINIT
	flags = 0;
#endif
	fp = opentranstbl(path, len, &flags);
	norockridge--;
	if (!fp) return(NULL);

	top = NULL;
	bottom = &top;
	while ((line = Xfgets(fp))) {
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
			Xfree(line);
			continue;
		}

		for (eol = cp; *eol; eol++) if (Xisblank(*eol)) break;
		if (*eol) *(eol++) = '\0';
		if (!*(eol = skipspace(eol))) {
			Xfree(line);
			continue;
		}

		org = getorgname(cp, flags);
		cp = eol;
		while (*eol && !Xisblank(*eol)) eol++;
		l1 = eol - cp;
		if (*eol) *(eol++) = '\0';
		l2 = 0;
		maj = min = (r_dev_t)-1;
		if (*line == 'L') {
			eol = skipspace(eol);
			while (eol[l2] && !Xisblank(eol[l2])) l2++;
			eol[l2++] = '\0';
		}
		else if (*line == 'B' || *line == 'C') {
			eol = skipspace(eol);
			eol = Xsscanf(eol, "%+*d", sizeof(r_dev_t), &maj);
			if (eol) {
				eol = skipspace(eol);
				eol = Xsscanf(eol, "%+*d",
					sizeof(r_dev_t), &min);
				if (!eol) maj = (r_dev_t)-1;
			}
		}

		new = (transtable *)Xmalloc(sizeof(transtable));
		new -> org = Xstrdup(org);
		new -> alias = Xstrndup(cp, l1);
		new -> slink = (l2 > 0) ? Xstrndup(eol, l2 - 1) : NULL;
		new -> type = *line;
		new -> rdev = (maj != (r_dev_t)-1)
			? makedev(maj, min) : (r_dev_t)-1;

		*bottom = new;
		new -> next = NULL;
		bottom = &(new -> next);
		Xfree(line);
	}

	if (top) *bottom = top;
	VOID_C Xfclose(fp);

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
			Xfree(tbl -> org);
			Xfree(tbl -> alias);
			Xfree(tbl -> slink);
			Xfree(tbl);
		}
	}
}

static transtable *NEAR inittrans(path, len)
CONST char *path;
int len;
{
	transtable *tp;

	if (len < 0) len = strlen(path);
	if (rr_cwd && !strnpathcmp(path, rr_cwd, len) && !rr_cwd[len])
		return(rr_curtbl);
	if (!(tp = readtranstbl(path, len))) return(NULL);

	freetranstbl(rr_curtbl);
	rr_curtbl = tp;
	Xfree(rr_cwd);
	rr_cwd = Xstrndup(path, len);
	Xfree(rr_transcwd);
	rr_transcwd = NULL;

	return(rr_curtbl);
}

static VOID NEAR cachetrans(path, trans)
CONST char *path, *trans;
{
	CONST char *cp1, *cp2;
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
	Xfree(rr_transcwd);
	rr_transcwd = Xstrndup(trans, cp2 - trans);
}

static char *NEAR transfile(file, len, buf, ptr)
CONST char *file;
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
			buf = Xstrncpy(&(buf[ptr]), tp -> alias,
				MAXPATHLEN - 1 - ptr);
			return(buf);
		}
		if ((tp = tp -> next) == rr_curtbl) break;
	}

	return(NULL);
}

char *transpath(path, buf)
CONST char *path;
char *buf;
{
	CONST char *cp;
	char *tmp, *next;
	int ptr, len, dlen;

	if (!(cp = includepath(path, rockridgepath)) || !*cp)
		return((char *)path);

	if (!rr_cwd || !rr_transcwd
	|| strnpathcmp(path, rr_cwd, len = strlen(rr_cwd))) {
		ptr = cp - path;
		Xstrncpy(buf, path, ptr);
	}
	else {
		Xstrcpy(buf, rr_transcwd);
		if (!path[len]) return(buf);
		ptr = strlen(buf);
		cp = &(path[len]);
	}

	while (cp) {
		dlen = cp++ - path;
		buf[ptr++] = _SC_;
		if ((next = strdelim(cp, 0))) len = next - cp;
		else len = strlen(cp);

		if (!inittrans(path, dlen)) tmp = NULL;
		else tmp = transfile(cp, len, buf, ptr);
		if (!tmp) tmp = Xstrncpy(&(buf[ptr]), cp, len);
		ptr = tmp - buf;
		cp = next;
	}
	cachetrans(path, buf);

	return(buf);
}

static char *NEAR detransfile(file, len, buf, ptr)
CONST char *file;
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
			buf = Xstrncpy(&(buf[ptr]), tp -> org,
				MAXPATHLEN - 1 - ptr);
			return(buf);
		}
		if ((tp = tp -> next) == rr_curtbl) break;
	}

	return(NULL);
}

static char *NEAR detransdir(dir, buf, ptr)
CONST char *dir;
char *buf;
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
		if (!cp) cp = Xstrncpy(&(buf[ptr]), dir, flen);
		ptr = cp - buf;
		dir = next;
	}

	return(&(buf[ptr]));
}

char *detranspath(path, buf)
CONST char *path;
char *buf;
{
	CONST char *cp;
	int ptr, len;

	if (!path) {
		freetranstbl(rr_curtbl);
		rr_curtbl = NULL;
		Xfree(rr_cwd);
		rr_cwd = NULL;
		Xfree(rr_transcwd);
		rr_transcwd = NULL;
		return(NULL);
	}

	if (!(cp = includepath(path, rockridgepath)) || !*cp)
		return((char *)path);

	if (!rr_transcwd || !rr_cwd
	|| strnpathcmp(path, rr_transcwd, len = strlen(rr_transcwd))) {
		ptr = cp++ - path;
		Xstrncpy(buf, path, ptr);
	}
	else {
		Xstrcpy(buf, rr_cwd);
		if (!path[len]) return(buf);
		ptr = strlen(buf);
		cp = &(path[len + 1]);
	}

	detransdir(cp, buf, ptr);
	cachetrans(buf, path);

	return(buf);
}

int rrlstat(path, stp)
CONST char *path;
struct stat *stp;
{
	transtable *tp;
	char *cp;
	u_int mode;

	if (!(cp = strrdelim(path, 0)) || !inittrans(path, cp++ - path))
		return(-1);

	tp = rr_curtbl;
	for (;;) {
		if (!strpathcmp(cp, tp -> org)) break;
		if ((tp = tp -> next) == rr_curtbl) return(-1);
	}
	rr_curtbl = tp;

	if ((mode = getfmode(tp -> type)) == (u_int)-1) mode = S_IFREG;
	stp -> st_mode &= ~S_IFMT;
	stp -> st_mode |= mode;
	if ((mode == S_IFBLK || mode == S_IFCHR) && tp -> rdev != (r_dev_t)-1)
		stp -> st_rdev = tp -> rdev;

	return(0);
}

int rrreadlink(path, buf, bufsiz)
CONST char *path;
char *buf;
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
	if (len > bufsiz) len = bufsiz;
	memcpy(buf, tp -> slink, len);

	return(len);
}
#endif	/* !_NOROCKRIDGE */
