/*
 *	rockridg.c
 *
 *	ISO-9660 RockRidge Format Filter
 */

#include <ctype.h>
#include "fd.h"
#include "func.h"

#ifndef	_NOROCKRIDGE

extern char fullpath[];

#define	TRANSTBLFILE	"TRANS.TBL"
#define	TRANSTBLVAR	1
#define	RR_TRANS	001
#define	RR_LOWER	002
#define	RR_VERNO	004
#define	RR_HYPHN	010

static char *NEAR getorgname __P_((char *, u_char));
static assoclist *NEAR readtranstbl __P_((VOID_A));
static VOID NEAR freetranstbl __P_((assoclist *));
static char *NEAR _detransfile __P_((char *, char *, int, int));

char *rockridgepath = NULL;

static assoclist *rr_curtbl = NULL;
static char *rr_cwd = NULL;


static char *NEAR getorgname(name, flag)
char *name;
u_char flag;
{
	char buf[MAXNAMLEN + 1];
	int i;

	for (i = 0; i < MAXNAMLEN && name[i] && name[i] != ';'; i++) {
		if (flag & RR_LOWER) buf[i] = tolower2(name[i]);
		else buf[i] = name[i];
	}

	if (i < MAXNAMLEN - 2 && (flag & RR_VERNO) && name[i] == ';') {
		buf[i++] = (flag & RR_HYPHN) ? '-' : ';';
		buf[i] = name[i];
		i++;
	}
	buf[i] = '\0';

	return(strdup2(buf));
}

static assoclist *NEAR readtranstbl(VOID_A)
{
	assoclist *top, **bottom, *new;
	FILE *fp;
	char *cp, *eol, *org, *line, file[MAXNAMLEN + 1];
	u_char flag;
	int i, len;

	for (;;) {
		strcpy(file, TRANSTBLFILE);
		flag = RR_TRANS;
		if ((fp = _Xfopen(file, "r", 1))) break;

		sprintf(&(file[sizeof(TRANSTBLFILE) - 1]), ";%d", TRANSTBLVAR);
		flag |= RR_VERNO;
		if ((fp = _Xfopen(file, "r", 1))) break;

		sprintf(&(file[sizeof(TRANSTBLFILE) - 1]), "-%d", TRANSTBLVAR);
		flag |= RR_HYPHN;
		if ((fp = _Xfopen(file, "r", 1))) break;

		for (i = 0; i < sizeof(TRANSTBLFILE) - 1; i++)
			file[i] = tolower2(TRANSTBLFILE[i]);
		file[i] = '\0';

		flag = RR_TRANS | RR_LOWER;
		if ((fp = _Xfopen(file, "r", 1))) break;

		sprintf(&(file[sizeof(TRANSTBLFILE) - 1]), ";%d", TRANSTBLVAR);
		flag |= RR_VERNO;
		if ((fp = _Xfopen(file, "r", 1))) break;

		sprintf(&(file[sizeof(TRANSTBLFILE) - 1]), "-%d", TRANSTBLVAR);
		flag |= RR_HYPHN;
		if ((fp = _Xfopen(file, "r", 1))) break;

		return(NULL);
	}

	top = NULL;
	bottom = &top;
	for (line = NULL; (line = fgets2(fp, 0)); free(line)) {
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
		new -> assoc = malloc2(len + 1);
		*(new -> assoc) = *line;
		strcpy(&(new -> assoc[1]), cp);

		if (*line == 'L') {
			cp = eol;
			while (*eol && *eol != ' ' && *eol != '\t') eol++;
			*eol = '\0';
			strcpy(new -> assoc + strlen(new -> assoc) + 1, cp);
		}

		*bottom = new;
		bottom = &(new -> next);
		new -> next = NULL;
	}

	if (top) *bottom = top;
	Xfclose(fp);
	return(top);
}

static VOID NEAR freetranstbl(tbl)
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

int transfilelist(VOID_A)
{
	assoclist *tp, *start, *tbl;
	char *cp, rpath[MAXPATHLEN];
	int i;

	if (!includepath(rpath, fullpath, &rockridgepath)) return(0);

	if (rr_cwd && !strpathcmp(rpath, rr_cwd)) tbl = rr_curtbl;
	else tbl = readtranstbl();

	start = tbl;
	if (tbl) for (i = 0; i < maxfile; i++) {
		if (isdotdir(filelist[i].name)) continue;

		tp = start;
		while (tp) {
			if (!strpathcmp(filelist[i].name, tp -> org)) break;
			if ((tp = tp -> next) == start) tp = NULL;
		}
		if (!tp) continue;
		start = tp -> next;

		cp = tp -> assoc;
		if (*(cp++) == 'L') {
			filelist[i].flags |= F_ISLNK;
			filelist[i].st_mode &= ~S_IFMT;
			filelist[i].st_mode |= S_IFLNK;
		}
		free(filelist[i].name);
		filelist[i].name = strdup2(cp);
	}

	if (tbl != rr_curtbl) {
		freetranstbl(rr_curtbl);
		if (rr_cwd) free(rr_cwd);
		rr_curtbl = tbl;
		rr_cwd = strdup2(rpath);
	}
	return(tbl != NULL);
}

char *transfile(file, buf)
char *file, *buf;
{
	assoclist *tp, *start, *tbl;
	char *cp, rpath[MAXPATHLEN];

	if (!includepath(rpath, fullpath, &rockridgepath)) return(file);

	if (rr_cwd && !strpathcmp(rpath, rr_cwd)) tbl = rr_curtbl;
	else tbl = readtranstbl();

	start = tp = tbl;
	if (!tbl) cp = file;
	else {
		while (tp) {
			if (!strpathcmp(file, tp -> org)) break;
			if ((tp = tp -> next) == start) tp = NULL;
		}
		if (!tp) cp = file;
		else {
			strncpy2(buf, &(tp -> assoc[1]), MAXNAMLEN);
			cp = buf;
			start = tp -> next;
		}
	}

	if (tbl != rr_curtbl) {
		freetranstbl(rr_curtbl);
		if (rr_cwd) free(rr_cwd);
		rr_cwd = strdup2(rpath);
	}
	rr_curtbl = start;
	return(cp);
}

static char *NEAR _detransfile(path, buf, rdlink, plen)
char *path, *buf;
int rdlink, plen;
{
	assoclist *tp, *start, *tbl;
	char *cp, *tmp, *dupl, dir[MAXPATHLEN];
	int len;

	if (!(cp = strrdelim(path, 0)) || cp == path) return(NULL);
	len = (cp++) - path;
	strncpy2(dir, path, len);
	if (len < plen) return(NULL);
	if (!_detransfile(dir, buf, 1, plen)) strcpy(buf, dir);

	if (rr_cwd && !strpathcmp(buf, rr_cwd)) {
		tbl = NULL;
		dupl = NULL;
	}
	else {
		_Xchdir(buf, 1);
		tbl = rr_curtbl;
		dupl = rr_cwd;
		rr_curtbl = readtranstbl();
		rr_cwd = strdup2(buf);
	}

	start = tp = rr_curtbl;
	if (!start) cp = NULL;
	else {
		while (tp) {
			if (!strpathcmp(cp, &(tp -> assoc[1]))) break;
			if ((tp = tp -> next) == start) tp = NULL;
		}
		if (!tp) cp = NULL;
		else {
			rr_curtbl = tp -> next;
			tmp = strcatdelim(buf);
			len = MAXPATHLEN - 1 - (tmp - buf);
			if (!rdlink || *(tp -> assoc) != 'L')
				strncpy2(tmp, tp -> org, len);
			else {
				cp = tp -> assoc + strlen(tp -> assoc) + 1;
				strncpy2(tmp, cp, len);
				cp = detransfile(buf, dir, 1);
				if (cp != buf) strcpy(buf, dir);
			}
			cp = buf;
		}
	}

	if (tbl) {
		freetranstbl(rr_curtbl);
		if (rr_cwd) free(rr_cwd);
		rr_curtbl = tbl;
		rr_cwd = dupl;
	}
	return(cp);
}

char *detransfile(path, buf, rdlink)
char *path, *buf;
int rdlink;
{
	char *cp, *tmp, cwd[MAXPATHLEN], rpath[MAXPATHLEN];
	int len;

	if (!(cp = includepath(rpath, path, &rockridgepath))
	|| !_Xgetwd(cwd, 1))
		return(path);

#if	MSDOS || !defined (_NODOSDRIVE)
	if (_dospath(cp)) tmp = strchr(&(cp[2]), PATHDELIM);
	else
#endif
	tmp = strchr(cp, PATHDELIM);
	len = (tmp) ? tmp - cp : strlen(cp);
	while (len > 1 && cp[len - 1] == _SC_) len--;
#if	MSDOS
	if (onkanji1(cp, len - 1)) len++;
#endif

	if (_detransfile(rpath, buf, rdlink, len))
		path = realpath2(buf, buf, rdlink);
	_Xchdir(cwd, 1);
	return(path);
}
#endif	/* !_NOROCKRIDGE */
