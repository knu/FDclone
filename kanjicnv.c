/*
 *	kanjicnv.c
 *
 *	Tiny Kanji Code Converter
 */

#include <stdio.h>
#include <string.h>
#include "machine.h"

#define	ASCII	000
#define	KANA	001
#define	KANJI	002
#define	JKANA	004

#define	SJIS	010
#define	EUC	020

static VOID fputc2 __P_((int, FILE *));
static VOID convert __P_((int, int, FILE *));
static VOID output __P_((FILE *, int, int));

static int msboff = 0;
static int prefix = 0;
static int removebs = 0;
static int kanjicode = SJIS;


static VOID fputc2(c, fp)
int c;
FILE *fp;
{
	if (msboff && (c & 0x80)) fprintf(fp, "\\%03o", c);
	else fputc(c, fp);
}

static VOID convert(j1, j2, fp)
int j1, j2;
FILE *fp;
{
	int c;

	if (kanjicode == EUC) {
		if (j1) {
			fputc2(j1 | 0x80, fp);
			fputc2(j2 | 0x80, fp);
		}
		else if (j2 <= 0x20 || j2 >= 0x60) fputc2(j2, fp);
		else {
			fputc2(0x8e, fp);
			fputc2(j2 | 0x80, fp);
		}
	}
	else {
		if (j1) {
			c = ((j1 - 1) >> 1) + ((j1 < 0x5f) ? 0x71 : 0xb1);
			fputc2(c, fp);
			c = j2 + ((j1 & 1)
				? ((j2 < 0x60) ? 0x1f : 0x20) : 0x7e);
			fputc2(c, fp);
			if (c == '\\' && (prefix || msboff)) fputc2('\\', fp);
		}
		else if (j2 <= 0x20 || j2 >= 0x60) fputc2(j2, fp);
		else fputc2(j2 | 0x80, fp);
	}
}

static VOID output(fp, c, mode)
FILE *fp;
int c, mode;
{
	static unsigned char buf[2];
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
		fputc2(buf[0], fp);
		bufp = 0;
	}
	kanji1 = mode;

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

	if (!strcmp(argv[i], "-")) in = stdin;
	else if (!(in = fopen(argv[i], "r"))) {
		fprintf(stderr, "\007%s: cannot open.\n", argv[i]);
		return(1);
	}
	if (i + 1 >= argc) out = stdout;
	else if (!(out = fopen(argv[i + 1], "w"))) {
		fprintf(stderr, "\007%s: cannot open.\n", argv[i + 1]);
		fclose(in);
		return(1);
	}

	mode = ASCII;
	esc = kanji = 0;

	while ((c = fgetc(in)) != EOF) {
		switch (c) {
			case '\033':	/* ESC */
				if (esc) output(out, '\033', mode);
				else if (kanji > 0) {
					output(out, '\033', mode);
					output(out, '$', mode);
				}
				else if (kanji < 0) {
					output(out, '\033', mode);
					output(out, '(', mode);
				}
				esc = 1;
				kanji = 0;
				break;
			case '$':
				if (!esc) {
					if (!kanji) output(out, c, mode);
				}
				else {
					mode &= ~JKANA;
					esc = 0;
					kanji = 1;
				}
				break;
			case '(':
				if (!esc) {
					if (!kanji) output(out, c, mode);
				}
				else {
					mode &= ~JKANA;
					esc = 0;
					kanji = -1;
				}
				break;
			case '\016':	/* SO */
				if (esc) output(out, '\033', mode);
				else if (kanji > 0) {
					output(out, '\033', mode);
					output(out, '$', mode);
				}
				else if (kanji < 0) {
					output(out, '\033', mode);
					output(out, '(', mode);
				}
				mode |= KANA;
				esc = kanji = 0;
				break;
			case '\017':	/* SI */
				if (esc) output(out, '\033', mode);
				else if (kanji > 0) {
					output(out, '\033', mode);
					output(out, '$', mode);
				}
				else if (kanji < 0) {
					output(out, '\033', mode);
					output(out, '(', mode);
				}
				mode &= ~KANA;
				esc = kanji = 0;
				break;
			case '\b':
				if (removebs) {
					if (esc) output(out, '\033', mode);
					else if (kanji > 0) {
						output(out, '\033', mode);
						output(out, '$', mode);
					}
					else if (kanji < 0) {
						output(out, '\033', mode);
						output(out, '(', mode);
					}
					output(NULL, EOF, mode);
					esc = kanji = 0;
					break;
				}
/*FALLTHRU*/
			default:
				if (esc) output(out, '\033', mode);
				else if (kanji > 0) mode |= KANJI;
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
