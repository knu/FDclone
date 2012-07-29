/*
 *	mkcat.c
 *
 *	message catalog generator
 */

#define	K_EXTERN
#include "headers.h"
#include "kctype.h"
#include "typesize.h"
#include "version.h"
#include "evalopt.h"
#include "gentbl.h"

#define	MAXLINEBUF		255
#define	MAXVERBUF		8
#define	MAXCATALOG		2
#define	CF_KANJI		0001
#define	CF_NOSUM		0100
#define	DEFINESTR		"define"
#define	MESCONVSTR		"mesconv2"
#define	MESLISTSTR		"meslist"
#define	CATSUMSTR		"CAT_SUM"
#define	VERSIONSTR		"#version\t"
#define	SUMSTR			"#sum\t"
#define	BEGINSTR		"#begin\t"
#define	ENDSTR			"#end"

static VOID_P NEAR Xrealloc __P_((VOID_P, ALLOC_T));
static char *NEAR fgets2 __P_((FILE *));
static char *NEAR getversion __P_((VOID_A));
static CONST char *NEAR getnum __P_((CONST char *, u_int *));
static int NEAR geteol __P_((CONST char *));
static VOID NEAR putstr __P_((CONST char *, int, FILE *));
static int NEAR mkcat __P_((FILE *, FILE *));
static int NEAR addbuf __P_((u_int, CONST char *));
static VOID NEAR cleanbuf __P_((VOID_A));
static int NEAR cnvcat __P_((FILE *, FILE *));
int main __P_((int, char *CONST []));

static char **meslist = NULL;
static u_int maxmes = (u_int)0;
static int n_column = 0;
static CONST char escapechar[] = "abefnrtv";
static CONST char escapevalue[] = {
	0x07, 0x08, 0x1b, 0x0c, 0x0a, 0x0d, 0x09, 0x0b,
};
static CONST opt_t optlist[] = {
	{'c', &n_column, 0, "column"},
	{'\0', NULL, 0, NULL},
};


static VOID_P NEAR Xrealloc(ptr, size)
VOID_P ptr;
ALLOC_T size;
{
	if (!size) return(NULL);
	return((ptr) ? realloc(ptr, size) : malloc(size));
}

static char *NEAR fgets2(fp)
FILE *fp;
{
	char *cp, *tmp, buf[MAXLINEBUF + 1];
	ALLOC_T i, len, size;
	int cont;

	cp = NULL;
	size = (ALLOC_T)0;
	for (;;) {
		if (!fgets(buf, sizeof(buf), fp)) return(cp);
		len = strlen(buf);
		if (!len || buf[len - 1] != '\n') cont = 1;
		else {
			cont = 0;
			len--;
		}

		if (!(tmp = (char *)Xrealloc(cp, size + len + 1))) {
			free(cp);
			return(NULL);
		}
		cp = tmp;
		memcpy(&(cp[size]), buf, len);
		cp[size += len] = '\0';

		if (cont) continue;
		if (!len) break;
		for (i = 0; i < len - 1; i++) if (iskanji1(buf, i)) i++;
		if (i >= len || buf[i] != '\\') break;
		size--;
	}

	return(cp);
}

static char *NEAR getversion(VOID_A)
{
	static char buf[MAXVERBUF];
	char *cp;
	int n;

	if (!(cp = strchr(version, ' '))) cp = version;
	else while (*cp == ' ') cp++;
	memset(buf, '\0', sizeof(buf));
	for (n = 0; cp[n]; n++) {
		if (n >= sizeof(buf) - 1 || cp[n] == ' ') break;
		buf[n] = cp[n];
	}

	return(buf);
}

static CONST char *NEAR getnum(cp, wp)
CONST char *cp;
u_int *wp;
{
	long n;
	char *tmp;

	if (!Xisdigit(*cp)) return(NULL);
	n = strtol(cp, &tmp, 10);
	if (n < 0L || n >= MAXUTYPE(u_int) || !tmp || tmp <= cp) return(NULL);
	if (wp) *wp = (u_int)n;

	return(tmp);
}

static int NEAR geteol(s)
CONST char *s;
{
	int i;

	for (i = 0; s[i]; i++) {
		if (s[i] == '\\') {
			if (!s[++i]) return(-1);
		}
		else if (iskanji1(s, i)) i++;
		else if (s[i] == '"') break;
	}
	if (!s[i]) return(-1);

	return(i);
}

static VOID NEAR putstr(s, len, fp)
CONST char *s;
int len;
FILE *fp;
{
	int i, c, n;

	for (i = 0; i < len; i++) {
		if (iskanji1(s, i)) {
			fputc(s[i], fp);
			fputc(s[++i], fp);
		}
		else if (s[i] != '\\') fputc(s[i], fp);
		else {
			i++;
			c = 0;
			for (n = 0; n < 3; n++) {
				if (s[i + n] < '0' || s[i + n] > '7') break;
				c = c * 8 + s[i + n] - '0';
			}
			if (n > 0) i += n - 1;
			else {
				c = s[i];
				for (n = 0; escapechar[n]; n++)
					if (c == escapechar[n]) break;
				if (escapechar[n]) c = escapevalue[n];
			}
			fputc(c, fp);
		}
	}

	fputc('\n', fp);
}

static int NEAR mkcat(fpin, fpout)
FILE *fpin, *fpout;
{
	CONST char *cp, *tmp, *ptr[MAXCATALOG];
	char *buf;
	u_int w;
	int i, c, len[MAXCATALOG];

	fprintf(fpout, "%s%s\n", VERSIONSTR, getversion());
	for (; (buf = fgets2(fpin)); free(buf)) {
		cp = buf;
		if (*(cp++) != '#') continue;
		while (Xisblank(*cp)) cp++;
		if (strncmp(cp, DEFINESTR, strsize(DEFINESTR))) continue;
		cp += strsize(DEFINESTR);
		if (!Xisblank(*cp)) continue;
		for (cp++; Xisblank(*cp); cp++) /*EMPTY*/;
		tmp = cp;
		if (!Xisalpha(*(cp++))) continue;
		while (Xisalnum(*cp) || *cp == '_') cp++;
		if (!Xisblank(*cp)) continue;
		i = cp - tmp;
		for (cp++; Xisblank(*cp); cp++) /*EMPTY*/;
		if (i == strsize(CATSUMSTR) && !strncmp(tmp, CATSUMSTR, i)) {
			if (!(cp = getnum(cp, &w)) || *cp) continue;
			fprintf(fpout, "%s%u\n", SUMSTR, w);
			continue;
		}

		if (!strncmp(cp, MESCONVSTR, strsize(MESCONVSTR)))
			cp += strsize(MESCONVSTR);
		else if (!strncmp(cp, MESLISTSTR, strsize(MESLISTSTR)))
			cp += strsize(MESLISTSTR);
		else continue;

		while (Xisblank(*cp)) cp++;
		if (*(cp++) != '(') continue;
		while (Xisblank(*cp)) cp++;
		if (!(cp = getnum(cp, &w)) || *(cp++) != ',') continue;

		for (i = 0; i < MAXCATALOG; i++) {
			while (Xisblank(*cp)) cp++;
			if (*(cp++) != '"') break;
			if ((len[i] = geteol(cp)) < 0) break;
			ptr[i] = cp;
			cp += len[i] + 1;
			while (Xisblank(*cp)) cp++;
			c = (i < MAXCATALOG - 1) ? ',' : ')';
			if (*(cp++) != c) break;
		}
		if (i < MAXCATALOG) continue;

		fprintf(fpout, "%s%u\n", BEGINSTR, w);
		putstr(ptr[n_column - 1], len[n_column - 1], fpout);
		fprintf(fpout, "%s\n", ENDSTR);
	}

	return(0);
}

static int NEAR addbuf(n, s)
u_int n;
CONST char *s;
{
	char *cp, **tmp;
	ALLOC_T len, size;

	if (n >= maxmes) {
		tmp = (char **)Xrealloc(meslist, (n + 1) * sizeof(char *));
		if (!tmp) return(-1);
		meslist = tmp;
		while (maxmes <= n) meslist[maxmes++] = NULL;
	}

	len = strlen(s);
	if (!meslist[n]) {
		size = (ALLOC_T)0;
		cp = (char *)malloc(len + 1);
		if (!cp) return(-1);
	}
	else {
		size = strlen(meslist[n]);
		cp = (char *)Xrealloc(meslist[n], size + 1 + len + 1);
		if (!cp) return(-1);
		cp[size++] = '\n';
	}
	memcpy(&(cp[size]), s, len + 1);
	meslist[n] = cp;

	return(0);
}

static VOID NEAR cleanbuf(VOID_A)
{
	u_int n;

	if (meslist) {
		for (n = 0; n < maxmes; n++)
			if (meslist[n]) free(meslist[n]);
		free(meslist);
	}
	maxmes = (u_int)0;
	meslist = NULL;
}

static int NEAR cnvcat(fpin, fpout)
FILE *fpin, *fpout;
{
	CONST char *cp;
	char *buf, *ver;
	long n;
	u_int i, w, sum, flags;

	ver = NULL;
	w = sum = (u_int)0;
	flags = CF_NOSUM;
	for (; (buf = fgets2(fpin)); free(buf)) {
		cp = buf;
		if (w) {
			if (!strcmp(buf, ENDSTR)) w = (u_int)0;
			else if (addbuf(w, buf) < 0) {
				fprintf(stderr, "Cannot allocate memory.\n");
				cleanbuf();
				return(-1);
			}
			else if (!(flags & CF_KANJI)) {
				for (i = 0; buf[i]; i++)
					if (ismsb(buf[i])) break;
				if (buf[i]) flags |= CF_KANJI;
			}
		}
		else if (!strncmp(buf, VERSIONSTR, strsize(VERSIONSTR))) {
			cp += strsize(VERSIONSTR);
			ver = getversion();
			if (strcmp(cp, ver)) {
				fprintf(stderr, "%s: Illegal version.\n", cp);
				cleanbuf();
				return(-1);
			}
		}
		else if (!strncmp(buf, SUMSTR, strsize(SUMSTR))) {
			cp += strsize(SUMSTR);
			if (!(cp = getnum(cp, &sum)) || *cp || !sum) {
				fprintf(stderr,
					"%s: Illegal check sum.\n", cp);
				cleanbuf();
				return(-1);
			}
			flags &= ~CF_NOSUM;
		}
		else if (!strncmp(buf, BEGINSTR, strsize(BEGINSTR))) {
			cp += strsize(BEGINSTR);
			if (!(cp = getnum(cp, &w)) || *cp || !w) {
				fprintf(stderr, "%s: Illegal ID.\n", cp);
				cleanbuf();
				return(-1);
			}
		}
	}

	if (!ver) {
		fprintf(stderr, "No version exists.\n");
		cleanbuf();
		return(-1);
	}
	if (flags & CF_NOSUM) {
		fprintf(stderr, "No check sum exists.\n");
		cleanbuf();
		return(-1);
	}

	if (!buf) {
		if (fwrite(ver, MAXVERBUF, 1, fpout) != 1
		|| fputword(flags, fpout) < 0 || fputword(sum, fpout) < 0
		|| fputword(maxmes, fpout) < 0) {
			fprintf(stderr, "Cannot write.\n");
			cleanbuf();
			return(-1);
		}
		n = 0L;
		for (i = 0; i < maxmes; i++) {
			n += (meslist[i]) ? strlen(meslist[i]) : 0;
			n++;
			if (n >= MAXUTYPE(u_short)) {
				fprintf(stderr, "Too mary messages.\n");
				cleanbuf();
				return(-1);
			}
			if (fputword(n, fpout) < 0) {
				fprintf(stderr, "Cannot write.\n");
				cleanbuf();
				return(-1);
			}
		}
		for (i = 0; i < maxmes; i++) {
			cp = (meslist[i]) ? meslist[i] : "";
			n = strlen(cp);
			if (fwrite(cp, n + 1, 1, fpout) != 1) {
				fprintf(stderr, "Cannot write.\n");
				cleanbuf();
				return(-1);
			}
		}
	}

	cleanbuf();
	if (buf) return(-1);
	return(0);
}

int main(argc, argv)
int argc;
char *CONST argv[];
{
	FILE *fpin, *fpout;
	int n;

	initopt(optlist);
	n = evalopt(argc, argv, optlist);
	if (n >= argc || n + 2 < argc) {
		optusage(argv[0], "<infile> [<outfile>]", optlist);
		return(1);
	}
	if (n_column > MAXCATALOG) {
		fprintf(stderr, "%d: Column too large.\n", n_column);
		return(1);
	}

	if (!strcmp(argv[n], "-")) fpin = stdin;
	else if (!(fpin = fopen(argv[n], "r"))) {
		fprintf(stderr, "%s: Cannot open.\n", argv[n]);
		return(1);
	}
	if (n + 1 >= argc) fpout = stdout;
	else if (!(fpout = opentbl(argv[n + 1]))) {
		VOID_C fclose(fpin);
		return(1);
	}

	if (n_column) n = mkcat(fpin, fpout);
	else n = cnvcat(fpin, fpout);
	VOID_C fclose(fpin);
	VOID_C fclose(fpout);
	if (n < 0) return(1);

	return(0);
}
