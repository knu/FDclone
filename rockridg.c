/*
 *	rockridg.c
 *
 *	ISO-9660 RockRidge Format Filter
 */

#include <ctype.h>
#include "fd.h"
#include "func.h"

#ifndef	_NOROCKRIDGE

#if	!MSDOS
#include <sys/param.h>
#endif

extern char fullpath[];

#define TRANSTBLFILE	"TRANS.TBL"
#define TRANSTBLVAR	1
#define	RR_TRANS	001
#define	RR_LOWER	002
#define	RR_VERNO	004
#define	RR_HYPHN	010

static char *getorgname __P_((char *, u_char));
static assoclist *readtranstbl __P_((VOID_A));
static VOID freetranstbl __P_((assoclist *));
static char *_detransfile __P_((char *, char *, int));

char *rockridgepath = NULL;

static assoclist *rr_curtbl = NULL;
static char *rr_cwd = NULL;


static char *getorgname(name, flag)
char *name;
u_char flag;
{
	char buf[MAXPATHLEN + 1];
	int i;

	for (i = 0; i < MAXPATHLEN && name[i] && name[i] != ';'; i++) {
		buf[i] = name[i];
		if ((flag & RR_LOWER) && buf[i] >= 'A' && buf[i] <= 'Z')
			buf[i] += 'a' - 'A';
	}

	if ((flag & RR_VERNO) && name[i] == ';') {
		buf[i++] = (flag & RR_HYPHN) ? '-' : ';';
		buf[i] = name[i];
		i++;
	}
	buf[i] = '\0';

	return(strdup2(buf));
}

static assoclist *readtranstbl(VOID_A)
{
	assoclist *top, **bottom, *new;
	FILE *fp;
	char *cp, *eol, *org, *line, file[MAXNAMLEN + 1];
	u_char flag;
	int i, len;

	for (;;) {
		strcpy(file, TRANSTBLFILE);
		flag = RR_TRANS;
		if ((fp = _Xfopen(file, "r"))) break;

		sprintf(file + sizeof(TRANSTBLFILE), ";%d", TRANSTBLVAR);
		flag |= RR_VERNO;
		if ((fp = _Xfopen(file, "r"))) break;

		sprintf(file + sizeof(TRANSTBLFILE), "-%d", TRANSTBLVAR);
		flag |= RR_HYPHN;
		if ((fp = _Xfopen(file, "r"))) break;

		for (i = 0; i < sizeof(TRANSTBLFILE) - 1; i++) {
			file[i] = TRANSTBLFILE[i];
			if (file[i] >= 'A' && file[i] <= 'Z')
				file[i] += 'a' - 'A';
		}
		file[i] = '\0';

		flag = RR_TRANS | RR_LOWER;
		if ((fp = _Xfopen(file, "r"))) break;

		sprintf(file + sizeof(TRANSTBLFILE), ";%d", TRANSTBLVAR);
		flag |= RR_VERNO;
		if ((fp = _Xfopen(file, "r"))) break;

		sprintf(file + sizeof(TRANSTBLFILE), "-%d", TRANSTBLVAR);
		flag |= RR_HYPHN;
		if ((fp = _Xfopen(file, "r"))) break;

		return(NULL);
	}

	top = NULL;
	bottom = &top;
	for (line = NULL; (line = fgets2(fp)); free(line)) {
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

		org = getorgname(cp, flag);
		cp = eol;
		while (*eol && *eol != ' ' && *eol != '\t') eol++;
		if (*eol) *(eol++) = '\0';
		len = 1 + strlen(cp);
		if (*line == 'L') {
			while (*eol == ' ' || *eol == '\t') eol++;
			if (!*eol) {
				free(org);
				continue;
			}
			len += 1 + strlen(eol);
		}

		new = (assoclist *)malloc2(sizeof(assoclist));
		new -> org = org;
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

	if (!includepath(rpath, rockridgepath)) return(0);

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

	if (!includepath(rpath, rockridgepath)) return(file);

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

	if (!(cp = strrdelim(path, 0)) || cp == path) return(NULL);
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
		strcatdelim(buf);
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

	if (!includepath(rpath, rockridgepath)) return(path);

	cwd = getwd2();
	if (_detransfile(rpath, buf, rdlink)) path = realpath2(buf, buf);
	_Xchdir(cwd);
	free(cwd);
	return(path);
}
#endif	/* !_NOROCKRIDGE */
