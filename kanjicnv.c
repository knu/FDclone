/*
 *	kanjicnv.c
 *
 *	Tiny Kanji Code Converter
 */

#include <stdio.h>
#include <string.h>

#define	ASCII	000
#define	KANA	001
#define	KANJI	002
#define	JKANA	004

#define	SJIS	010
#define	EUC	020

#ifdef	__STDC__
#define	__P_(args)	args
#define	VOID		void
#else
#define	__P_(args)	()
#define	VOID
#endif

static int fputs2 __P_((char *, FILE *));
static char *convert __P_((int, int));
static VOID output __P_((FILE *, int, int));

static int msboff = 0;
static int prefix = 0;
static int removebs = 0;
static int kanjicode = SJIS;


static int fputs2(s, fp)
char *s;
FILE *fp;
{
	int c;

	if (!msboff) return(fputs(s, fp));
	while ((c = *(unsigned char *)(s++))) {
		if (c & 0x80) fprintf(fp, "\\%o", c);
		else fputc(c, fp);
	}
	return(0);
}

static char *convert(j1, j2)
int j1, j2;
{
	static unsigned char cnv[4];

	if (kanjicode == EUC) {
		cnv[1] = j2 | 0x80;
		if (j1) cnv[0] = j1 | 0x80;
		else if (j2 > ' ') cnv[0] = 0x8e;
		else {
			cnv[0] = j2;
			cnv[1] = '\0';
		}
		cnv[2] = '\0';
	}
	else if (j1) {
		cnv[0] = ((j1 - 1) >> 1) + ((j1 < 0x5f) ? 0x71 : 0xb1);
		cnv[1] = j2 + ((j1 & 1) ? ((j2 < 0x60) ? 0x1f : 0x20) : 0x7e);
		if (cnv[1] == '\\' && prefix) {
			cnv[2] = '\\';
			cnv[3] = '\0';
		}
		else cnv[2] = '\0';
	}
	else if (j2 >= 0x60) cnv[0] = '\0';
	else {
		cnv[0] = (j2 > ' ') ? j2 | 0x80 : j2;
		cnv[1] = '\0';
	}
	return((char *)cnv);
}

static VOID output(fp, c, mode)
FILE *fp;
int c, mode;
{
	static unsigned char buf[2];
	static int kanji1 = 0;
	static int bufp = 0;

	if (!fp) bufp = kanji1 = 0;

	if (bufp > 1) {
		fputs2(convert(buf[0], buf[1]), fp);
		bufp = 0;
		kanji1 = mode;
	}
	else if (bufp > 0 && (kanji1 & (KANA | JKANA))) {
		fputs2(convert(0, buf[0]), fp);
		bufp = 0;
		kanji1 = mode;
	}
	else if (bufp > 0 && !(kanji1 & KANJI)) {
		fputc(buf[0], fp);
		bufp = 0;
		kanji1 = mode;
	}
	else if (!bufp) kanji1 = mode;

	if (c != EOF) buf[bufp++] = c;
}

int main(argc, argv)
int argc;
char *argv[];
{
	FILE *in, *out;
	int i, j, c, mode, esc, kanji;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-' || !argv[i][1]) break;
		if (argv[i][1] == '-' && !argv[i][2]) {
			i++;
			break;
		}
		for (j = 1; argv[i][j]; j++) switch (argv[i][j]) {
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
	if (i >= argc) {
		fprintf(stderr,
			"\007Usage: kanjicnv [-7bces] <infile> [<outfile>]\n");
		return(1);
	}

	in = strcmp(argv[i], "-") ? fopen(argv[i], "r") : stdin;
	out = (i + 1 < argc) ? fopen(argv[i + 1], "w") : stdout;

	mode = ASCII;
	esc = kanji = 0;

	while ((c = fgetc(in)) != EOF) {
		switch (c) {
			case '\033':	/* ESC */
				esc = 1;
				break;
			case '$':
				if (!esc) output(out, c, mode);
				else {
					mode &= ~JKANA;
					kanji = 1;
					esc = 0;
				}
				break;
			case '(':
				if (!esc) output(out, c, mode);
				else {
					mode &= ~JKANA;
					kanji = -1;
					esc = 0;
				}
				break;
			case '\016':	/* SO */
				mode |= KANA;
				break;
			case '\017':	/* SI */
				mode &= ~KANA;
				break;
			case '\b':
				if (removebs) {
					output(NULL, EOF, mode);
					break;
				}
			default:
				if (esc) output(out, '\033', mode);
				if (kanji > 0) mode |= KANJI;
				else if (kanji < 0) {
					if (c == 'I') mode |= JKANA;
					else mode &= ~KANJI;
				}
				else output(out, c, mode);
				esc = kanji = 0;
				break;
		}
	}
	output(out, EOF, mode);

	fclose(out);
	fclose(in);

	return(0);
}
