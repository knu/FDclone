/*
 *	rockridg.c
 *
 *	ISO-9660 RockRidge Format Filter
 */

#include <ctype.h>
#include "fd.h"
#include "func.h"

#ifndef	_NOROCKRIDGE

#if	MSDOS
#include "unixemu.h"
#else
#include <sys/param.h>
#endif

extern char fullpath[];

char *rockridgepath;

#define TRANSTBLFILE	"TRANS.TBL"
#define TRANSTBLVAR	1
#define	RR_TRANS	001
#define	RR_LOWER	002
#define	RR_VERNO	004
#define	RR_HYPHN	010

static int isrockridge();
static char *getorgname();
static assoclist *readtranstbl();
static VOID freetranstbl();
static char *_detransfile();

static assoclist *rr_curtbl = NULL;
static char *rr_cwd = NULL;


static int isrockridge(path)
char *path;
{
	char *cp, *eol;
	int len;

	for (cp = rockridgepath; cp && *cp; ) {
		if (!(eol = strchr(cp, ';'))) len = strlen(cp);
		else {
			len = eol - cp;
			eol++;
		}
		while (len > 0 && cp[len] == _SC_) len--;
		if (len > 0 && !strnpathcmp(path, cp, len) &&
		(!path[len] || path[len] == _SC_)) return(1);
		cp = eol;
	}
	return(0);
}

static char *getorgname(name, stat)
char *name;
u_char stat;
{
	char buf[MAXPATHLEN + 1];
	int i;

	for (i = 0; i < MAXPATHLEN && name[i] && name[i] != ';'; i++) {
		buf[i] = name[i];
		if ((stat & RR_LOWER) && buf[i] >= 'A' && buf[i] <= 'Z')
			buf[i] += 'a' - 'A';
	}

	if ((stat & RR_VERNO) && name[i] == ';') {
		buf[i++] = (stat & RR_HYPHN) ? '-' : ';';
		buf[i] = name[i];
		i++;
	}
	buf[i] = '\0';

	return(strdup2(buf));
}

static assoclist *readtranstbl()
{
	assoclist *top, **bottom, *new;
	FILE *fp;
	char *cp, *eol, line[MAXPATHLEN * 2 + 1];
	u_char stat;
	int i, len;

	for (;;) {
		strcpy(line, TRANSTBLFILE);
		stat = RR_TRANS;
		if (fp = _Xfopen(line, "r")) break;

		sprintf(line + sizeof(TRANSTBLFILE), ";%d", TRANSTBLVAR);
		stat |= RR_VERNO;
		if (fp = _Xfopen(line, "r")) break;

		sprintf(line + sizeof(TRANSTBLFILE), "-%d", TRANSTBLVAR);
		stat |= RR_HYPHN;
		if (fp = _Xfopen(line, "r")) break;

		for (i = 0; i < sizeof(TRANSTBLFILE); i++) {
			line[i] = TRANSTBLFILE[i];
			if (line[i] >= 'A' && line[i] <= 'Z')
				line[i] += 'a' - 'A';
		}
		line[i] = '\0';

		stat = RR_TRANS | RR_LOWER;
		if (fp = _Xfopen(line, "r")) break;

		sprintf(line + sizeof(TRANSTBLFILE), ";%d", TRANSTBLVAR);
		stat |= RR_VERNO;
		if (fp = _Xfopen(line, "r")) break;

		sprintf(line + sizeof(TRANSTBLFILE), "-%d", TRANSTBLVAR);
		stat |= RR_HYPHN;
		if (fp = _Xfopen(line, "r")) break;

		return(NULL);
	}

	top = NULL;
	bottom = &top;
	while (Xfgets(line, MAXPATHLEN * 2, fp)) {
		if (cp = strchr(line, '\n')) *cp = '\0';
		cp = line;
		switch (*cp) {
			case 'F':
			case 'D':
			case 'L':
			case 'M':
				cp++;
				break;
			default:
				*cp = '\0';
				break;
		}
		while (*cp == ' ' || *cp == '\t') cp++;
		if (!*cp) continue;

		for (eol = cp; *eol && *eol != ' ' && *eol != '\t'; eol++);
		*(eol++) = '\0';
		while (*eol == ' ' || *eol == '\t') eol++;
		if (!*eol) continue;

		new = (assoclist *)malloc2(sizeof(assoclist));
		new -> org = getorgname(cp, stat);
		cp = eol;
		while (*eol && *eol != ' ' && *eol != '\t') eol++;
		if (*eol) *(eol++) = '\0';
		len = 1 + strlen(cp);
		if (*line == 'L') {
			while (*eol == ' ' || *eol == '\t') eol++;
			if (!*eol) {
				free(new -> org);
				free(new);
				continue;
			}
			len += 1 + strlen(eol);
		}

		new -> assoc = (char *)malloc2(len + 1);
		*(new -> assoc) = *line;
		strcpy(new -> assoc + 1, cp);

		if (*line == 'L') {
			cp = eol;
			while (*eol && *eol != ' ' && *eol != '\t') eol++;
			*eol = '\0';
			strcpy(new -> assoc + strlen(new -> assoc) + 1 + 1, cp);
		}

		*bottom = new;
		bottom = &(new -> next);
		new -> next = NULL;
	}

	if (top) *bottom = top;
	Xfclose(fp);
	return(top);
}

static VOID freetranstbl(tbl)
assoclist *tbl;
{
	assoclist *tp;

	if (tbl) {
		tp = tbl -> next;
		tbl -> next = NULL;
		while (tp) {
			tbl = tp;
			tp = tbl -> next;
			free(tbl -> org);
			free(tbl -> assoc);
			free(tbl);
		}
	}
}

int transfilelist(list, max)
namelist *list;
int max;
{
	assoclist *tp, *start, *tbl;
	char *cp, rpath[MAXPATHLEN + 1];
	int i;

	if (!rockridgepath || !*rockridgepath) return(0);
	realpath2(fullpath, rpath);
	if (!isrockridge(rpath)) return(0);

	if (rr_cwd && !strpathcmp(rpath, rr_cwd)) tbl = rr_curtbl;
	else if (!(tbl = readtranstbl())) return(0);

	start = tbl;
	for (i = 0; i < max; i++) {
		if (isdotdir(list[i].name)) continue;

		tp = start;
		while (tp) {
			if (!strpathcmp(list[i].name, tp -> org)) break;
			if ((tp = tp -> next) == start) tp = NULL;
		}
		if (!tp) continue;
		if (!(start = tp -> next)) start = tbl;

		cp = tp -> assoc;
		if (*(cp++) == 'L') {
			list[i].flags |= F_ISLNK;
			list[i].st_mode &= ~S_IFMT;
			list[i].st_mode |= S_IFLNK;
		}
		free(list[i].name);
		list[i].name = strdup2(cp);
	}

	if (tbl != rr_curtbl) {
		freetranstbl(rr_curtbl);
		if (rr_cwd) free(rr_cwd);
		rr_curtbl = tbl;
		rr_cwd = strdup2(rpath);
	}
	return(1);
}

char *transfile(file, buf)
char *file, *buf;
{
	assoclist *tp, *start, *tbl;
	char *cp, rpath[MAXPATHLEN + 1];

	if (!rockridgepath || !*rockridgepath) return(file);
	realpath2(fullpath, rpath);
	if (!isrockridge(rpath)) return(file);

	if (rr_cwd && !strpathcmp(rpath, rr_cwd)) tbl = rr_curtbl;
	else if (!(tbl = readtranstbl())) return(file);

	start = tp = tbl;
	while (tp) {
		if (!strpathcmp(file, tp -> org)) break;
		if ((tp = tp -> next) == start) tp = NULL;
	}
	if (!tp) cp = file;
	else {
		strcpy(buf, tp -> assoc + 1);
		cp = buf;
		start = tp -> next;
	}

	if (tbl != rr_curtbl) {
		freetranstbl(rr_curtbl);
		if (rr_cwd) free(rr_cwd);
		rr_cwd = strdup2(rpath);
	}
	rr_curtbl = start;
	return(cp);
}

static char *_detransfile(path, buf, rdlink)
char *path, *buf;
int rdlink;
{
	assoclist *tp, *start, *tbl;
	char *cp, dir[MAXPATHLEN + 1];

	if (!(cp = strrchr(path, _SC_)) || cp == path) return(NULL);
	strncpy2(dir, path, (cp++) - path);
	if (!_detransfile(dir, buf, 1)) strcpy(buf, dir);

	if (rr_cwd && !strpathcmp(buf, rr_cwd)) tbl = rr_curtbl;
	else {
		_Xchdir(buf);
		if (!(tbl = readtranstbl())) return(NULL);
	}

	start = tp = tbl;
	while (tp) {
		if (!strpathcmp(cp, tp -> assoc + 1)) break;
		if ((tp = tp -> next) == start) tp = NULL;
	}
	if (!tp) cp = NULL;
	else {
		start = tp -> next;
		strcat(buf, _SS_);
		if (!rdlink || *(tp -> assoc) != 'L') strcat(buf, tp -> org);
		else {
			for (cp = tp -> assoc + 1; *cp; cp++);
			cp++;
			strcat(buf, cp);
			detransfile(buf, buf, 1);
		}
		cp = buf;
	}

	if (tbl != rr_curtbl) freetranstbl(tbl);
	else rr_curtbl = start;
	return(cp);
}

char *detransfile(path, buf, rdlink)
char *path, *buf;
int rdlink;
{
	char *cwd, rpath[MAXPATHLEN + 1];

	if (!rockridgepath || !*rockridgepath) return(path);
	realpath2(path, rpath);
	if (!isrockridge(rpath)) return(path);

	cwd = getwd2();
	if (_detransfile(rpath, buf, rdlink)) path = realpath2(buf, buf);
	_Xchdir(cwd);
	free(cwd);
	return(path);
}
#endif	/* !_NOROCKRIDGE */
