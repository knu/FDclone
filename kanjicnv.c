/*
 *	kanjicnv.c
 *
 *	tiny Kanji code converter
 */

#include "headers.h"
#include "evalopt.h"

#if	defined (UTF8DOC) && defined (DEFKCODE)
#define	K_EXTERN
#include "kctype.h"
#endif

#define	ASCII			000
#define	KANA			001
#define	KANJI			002
#define	JKANA			004

#define	K_SJIS			010
#define	K_EUC			020
#define	K_UTF8			040

#define	UTF8_MAC		1
#define	UTF8_ICONV		2
#define	U2_UDEF			0x3013	/* GETA */
#define	MINUNICODE		0x00a7
#define	MAXUNICODE		0xffe5
#define	MINKANJI		0x8140
#define	MAXKANJI		0xfc4b
#define	getword(s, n)		(((u_short)((s)[(n) + 1]) << 8) | (s)[n])

#ifdef	UTF8DOC
extern CONST u_char unitblbuf[];
extern u_int unitblent;
extern int nftblnum;
extern int nflen;
extern CONST u_char *nftblbuf[];
extern u_int nftblent[];
#endif

static VOID NEAR Xfputc __P_((int, FILE *));
#ifdef	UTF8DOC
# ifdef	DEFKCODE
static CONST char *NEAR Xstrstr __P_((CONST char *, CONST char *));
# endif
static u_int NEAR cnvunicode __P_((u_int));
static VOID NEAR fputucs2 __P_((u_int, FILE *));
static VOID NEAR ucs2normalization __P_((u_int, int, FILE *));
static VOID NEAR convertutf8 __P_((u_int, FILE *));
#endif	/* UTF8DOC */
static VOID NEAR convert __P_((int, int, FILE *));
static VOID NEAR output __P_((FILE *, int, int));
int main __P_((int, char *CONST []));

static int msboff = 0;
static int prefix = 0;
static int removebs = 0;
static int kanjicode = K_SJIS;
static CONST opt_t optlist[] = {
	{'7', &msboff, 1, NULL},
	{'b', &removebs, 1, NULL},
	{'c', &prefix, 1, NULL},
	{'e', &kanjicode, K_EUC, NULL},
	{'s', &kanjicode, K_SJIS, NULL},
#ifdef	UTF8DOC
	{'u', &kanjicode, K_UTF8, NULL},
#endif
	{'\0', NULL, 0, NULL},
};


static VOID NEAR Xfputc(c, fp)
int c;
FILE *fp;
{
	if (msboff && (c & 0x80)) fprintf(fp, "\\%03o", c);
	else fputc(c, fp);
}

#ifdef	UTF8DOC
# ifdef	DEFKCODE
static CONST char *NEAR Xstrstr(s1, s2)
CONST char *s1, *s2;
{
	int i, c1, c2;

	while (*s1) {
		for (i = 0;; i++) {
			if (!s2[i]) return(s1);
			c1 = s1[i];
			c2 = s2[i];
			if (Xislower(c2)) {
				c1 = Xtoupper(c1);
				c2 = Xtoupper(c2);
			}
			if (c1 != c2) break;
			if (iswchar(s1, 0)) {
				if (!s2[++i]) return(s1);
				if (s1[i] != s2[i]) break;
			}
			if (!s1[i]) break;
		}
		if (iswchar(s1, 0)) s1++;
		s1++;
	}

	return(NULL);
}
# endif	/* DEFKCODE */

static u_int NEAR cnvunicode(wc)
u_int wc;
{
	CONST u_char *cp;
	u_int w, ofs, min, max;

	if (wc < 0x0080) return(wc);
	if (wc >= 0x00a1 && wc <= 0x00df)
		return(0xff00 | (wc - 0x00a1 + 0x61));
	if (wc >= 0x8260 && wc <= 0x8279)
		return(0xff00 | (wc - 0x8260 + 0x21));
	if (wc >= 0x8281 && wc <= 0x829a)
		return(0xff00 | (wc - 0x8281 + 0x41));
	if (wc < MINKANJI || wc > MAXKANJI) return(U2_UDEF);

# if	0
	wc = unifysjis(wc, 0);
# endif

	ofs = min = max = 0;
	cp = unitblbuf;
	for (ofs = 0; ofs < unitblent; ofs++) {
		w = getword(cp, 2);
		if (wc == w) return(getword(cp, 0));
		cp += 4;
	}

	return(U2_UDEF);
}

static VOID NEAR fputucs2(wc, fp)
u_int wc;
FILE *fp;
{
	if (wc < 0x80) Xfputc(wc, fp);
	else if (wc < 0x800) {
		Xfputc(0xc0 | (wc >> 6), fp);
		Xfputc(0x80 | (wc & 0x3f), fp);
	}
	else {
		Xfputc(0xe0 | (wc >> 12), fp);
		Xfputc(0x80 | ((wc >> 6) & 0x3f), fp);
		Xfputc(0x80 | (wc & 0x3f), fp);
	}
}

static VOID NEAR ucs2normalization(wc, nf, fp)
u_int wc;
int nf;
FILE *fp;
{
	CONST u_char *cp;
	u_int w, ofs, ent;
	int n;

	if (nf <= 0 || nf > nftblnum || wc < MINUNICODE || wc > MAXUNICODE) {
		cp = NULL;
		ofs = ent = (u_int)0;
	}
	else {
		n = 2 + nflen * 2;
		cp = nftblbuf[nf - 1];
		ent = nftblent[nf - 1];
		for (ofs = 0; ofs < ent; ofs++) {
			w = getword(cp, 0);
			if (wc == w) break;
			cp += n;
		}
	}

	if (ofs >= ent) fputucs2(wc, fp);
	else for (n = 0; n < nflen; n++) {
		cp += 2;
		w = getword(cp, 0);
		if (!w) break;
		fputucs2(w, fp);;
	}
}

static VOID NEAR convertutf8(wc, fp)
u_int wc;
FILE *fp;
{
	int nf;

# ifdef	DEFKCODE
	if (Xstrstr(DEFKCODE, "utf8-mac")) nf = UTF8_MAC;
	else if (Xstrstr(DEFKCODE, "utf8-iconv")) nf = UTF8_ICONV;
	else
# endif
	nf = 0;

	ucs2normalization(cnvunicode(wc), nf, fp);
}
#endif	/* UTF8DOC */

static VOID NEAR convert(j1, j2, fp)
int j1, j2;
FILE *fp;
{
	int c1, c2;

	c1 = c2 = '\0';
	if (kanjicode == K_EUC) {
		if (j1) {
			c1 = (j1 | 0x80);
			c2 = (j2 | 0x80);
		}
		else if (j2 <= 0x20 || j2 >= 0x60) c2 = j2;
		else {
			c1 = 0x8e;
			c2 = (j2 | 0x80);
		}
	}
	else {
		if (j1) {
			c1 = ((j1 - 1) >> 1) + ((j1 < 0x5f) ? 0x71 : 0xb1);
			c2 = j2 + ((j1 & 1)
				? ((j2 < 0x60) ? 0x1f : 0x20) : 0x7e);
		}
		else if (j2 <= 0x20 || j2 >= 0x60) c2 = j2;
		else c2 = (j2 | 0x80);
#ifdef	UTF8DOC
		if (kanjicode == K_UTF8) {
			convertutf8(((u_int)c1 << 8) | c2, fp);
			c1 = c2 = '\0';
		}
#endif
	}

	if (c1) Xfputc(c1, fp);
	if (c2) Xfputc(c2, fp);
	if (c2 == '\\' && (prefix || msboff)) Xfputc('\\', fp);
}

static VOID NEAR output(fp, c, mode)
FILE *fp;
int c, mode;
{
	static u_char buf[2];
	static int kanji1 = 0;
	static int bufp = 0;

	if (!fp) {
		bufp = kanji1 = 0;
		return;
	}

	if (!bufp) /*EMPTY*/;
	else if (bufp > 1) {
		convert(buf[0], buf[1], fp);
		bufp = 0;
	}
	else if (kanji1 & (KANA | JKANA)) {
		convert(0, buf[0], fp);
		bufp = 0;
	}
	else if (!(kanji1 & KANJI)) {
		Xfputc(buf[0], fp);
		bufp = 0;
	}
	kanji1 = mode;

	if (c != EOF) buf[bufp++] = c;
}

int main(argc, argv)
int argc;
char *CONST argv[];
{
	FILE *fpin, *fpout;
	int n, c, mode, esc, kanji;

	initopt(optlist);
	n = evalopt(argc, argv, optlist);
	if (n >= argc || n + 2 < argc) {
		optusage(argv[0], "<infile> [<outfile>]", optlist);
		return(1);
	}

	if (!strcmp(argv[n], "-")) fpin = stdin;
	else if (!(fpin = fopen(argv[n], "r"))) {
		fprintf(stderr, "%s: cannot open.\n", argv[n]);
		return(1);
	}
	if (n + 1 >= argc) fpout = stdout;
	else if (!(fpout = fopen(argv[n + 1], "w"))) {
		fprintf(stderr, "%s: cannot open.\n", argv[n + 1]);
		VOID_C fclose(fpin);
		return(1);
	}

	mode = ASCII;
	esc = kanji = 0;

	while ((c = fgetc(fpin)) != EOF) {
		switch (c) {
			case '\033':	/* ESC */
				if (esc) output(fpout, '\033', mode);
				else if (kanji > 0) {
					output(fpout, '\033', mode);
					output(fpout, '$', mode);
				}
				else if (kanji < 0) {
					output(fpout, '\033', mode);
					output(fpout, '(', mode);
				}
				esc = 1;
				kanji = 0;
				break;
			case '$':
				if (!esc) {
					if (!kanji) output(fpout, c, mode);
				}
				else {
					mode &= ~JKANA;
					esc = 0;
					kanji = 1;
				}
				break;
			case '(':
				if (!esc) {
					if (!kanji) output(fpout, c, mode);
				}
				else {
					mode &= ~JKANA;
					esc = 0;
					kanji = -1;
				}
				break;
			case '\016':	/* SO */
				if (esc) output(fpout, '\033', mode);
				else if (kanji > 0) {
					output(fpout, '\033', mode);
					output(fpout, '$', mode);
				}
				else if (kanji < 0) {
					output(fpout, '\033', mode);
					output(fpout, '(', mode);
				}
				mode |= KANA;
				esc = kanji = 0;
				break;
			case '\017':	/* SI */
				if (esc) output(fpout, '\033', mode);
				else if (kanji > 0) {
					output(fpout, '\033', mode);
					output(fpout, '$', mode);
				}
				else if (kanji < 0) {
					output(fpout, '\033', mode);
					output(fpout, '(', mode);
				}
				mode &= ~KANA;
				esc = kanji = 0;
				break;
			case '\b':
				if (removebs) {
					if (esc) output(fpout, '\033', mode);
					else if (kanji > 0) {
						output(fpout, '\033', mode);
						output(fpout, '$', mode);
					}
					else if (kanji < 0) {
						output(fpout, '\033', mode);
						output(fpout, '(', mode);
					}
					output(NULL, EOF, mode);
					esc = kanji = 0;
					break;
				}
/*FALLTHRU*/
			default:
				if (esc) output(fpout, '\033', mode);
				else if (kanji > 0) mode |= KANJI;
				else if (kanji < 0) {
					if (c == 'I') mode |= JKANA;
					else mode &= ~KANJI;
				}
				else output(fpout, c, mode);
				esc = kanji = 0;
				break;
		}
	}
	output(fpout, EOF, mode);

	VOID_C fclose(fpout);
	VOID_C fclose(fpin);

	return(0);
}
