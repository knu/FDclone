/*
 *	kanjicnv.c
 *
 *	tiny Kanji code converter
 */

#include "headers.h"

#define	ASCII			000
#define	KANA			001
#define	KANJI			002
#define	JKANA			004

#define	SJIS			010
#define	EUC			020

static VOID NEAR Xfputc __P_((int, FILE *));
static VOID NEAR convert __P_((int, int, FILE *));
static VOID NEAR output __P_((FILE *, int, int));
int main __P_((int, char *CONST []));

static int msboff = 0;
static int prefix = 0;
static int removebs = 0;
static int kanjicode = SJIS;


static VOID NEAR Xfputc(c, fp)
int c;
FILE *fp;
{
	if (msboff && (c & 0x80)) fprintf(fp, "\\%03o", c);
	else fputc(c, fp);
}

static VOID NEAR convert(j1, j2, fp)
int j1, j2;
FILE *fp;
{
	int c;

	if (kanjicode == EUC) {
		if (j1) {
			Xfputc(j1 | 0x80, fp);
			Xfputc(j2 | 0x80, fp);
		}
		else if (j2 <= 0x20 || j2 >= 0x60) Xfputc(j2, fp);
		else {
			Xfputc(0x8e, fp);
			Xfputc(j2 | 0x80, fp);
		}
	}
	else {
		if (j1) {
			c = ((j1 - 1) >> 1) + ((j1 < 0x5f) ? 0x71 : 0xb1);
			Xfputc(c, fp);
			c = j2 + ((j1 & 1)
				? ((j2 < 0x60) ? 0x1f : 0x20) : 0x7e);
			Xfputc(c, fp);
			if (c == '\\' && (prefix || msboff)) Xfputc('\\', fp);
		}
		else if (j2 <= 0x20 || j2 >= 0x60) Xfputc(j2, fp);
		else Xfputc(j2 | 0x80, fp);
	}
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
	int i, n, c, mode, esc, kanji;

	for (n = 1; n < argc; n++) {
		if (argv[n][0] != '-' || !argv[n][1]) break;
		if (argv[n][1] == '-' && !argv[n][2]) {
			n++;
			break;
		}
		for (i = 1; argv[n][i]; i++) switch (argv[n][i]) {
			case '7':
				msboff = 1;
				break;
			case 'e':
				kanjicode = EUC;
				break;
			case 's':
				kanjicode = SJIS;
				break;
			case 'c':
				prefix = 1;
				break;
			case 'b':
				removebs = 1;
				break;
			default:
				break;
		}
	}
	if (n >= argc || n + 2 < argc) {
		fprintf(stderr,
			"Usage: kanjicnv [-7bces] <infile> [<outfile>]\n");
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
