/*
 *	kanjicnv.c
 *
 *	Tiny Kanji Code Converter
 */

#include <stdio.h>
#include <string.h>
#include "machine.h"

#define	ASCII	0
#define	KANA	1
#define	KANJI	2

#ifdef	CPP7BIT
static int fputs2();
static int through;
#else
#define	fputs2	fputs
#endif

static unsigned char *convert();
static int output();


#ifdef	CPP7BIT
static int fputs2(s, fp)
char *s;
FILE *fp;
{
	unsigned char c;

	if (through) return(fputs(s, fp));
	while (c = *(unsigned char *)(s++)) {
		if (c & 0x80) fprintf(fp, "\\%o", c);
		else fputc(c, fp);
	}
}
#endif

static unsigned char *convert(j1, j2)
int j1, j2;
{
	static unsigned char cnv[4];

#ifdef	CODEEUC
	cnv[0] = (j1) ? j1 | 0x80 : 0x8e;
	cnv[1] = j2 | 0x80;
	cnv[2] = '\0';
#else
	if (j1) {
		cnv[0] = ((j1 - 1) >> 1) + ((j1 < 0x5e) ? 0x71 : 0xb1);
		cnv[1] = j2 + ((j1 & 1) ? ((j2 < 0x60) ? 0x1f : 0x20) : 0x7e);
		if (cnv[1] == '\\') {
			cnv[2] = '\\';
			cnv[3] = '\0';
		}
		else cnv[2] = '\0';
	}
	else {
		cnv[0] = j2 | 0x80;
		cnv[1] = '\0';
	}
#endif
	return(cnv);
}

static int output(fp, c, mode)
FILE *fp;
int c;
int mode;
{
	static int kanji1 = 0;

	switch (mode) {
		case ASCII:
			fputc(c, fp);
			kanji1 = 0;
			break;
		case KANJI:
			if (kanji1) {
				fputs2((char *)convert(kanji1, c), fp);
				kanji1 = 0;
			}
			else kanji1 = c;
			break;
		default:
			fputs2((char *)convert(0, c), fp);
			kanji1 = 0;
			break;
	}
}

int main(argc, argv)
int argc;
char *argv[];
{
	FILE *in, *out;
	int c, mode, esc, kanji;

#ifdef	CPP7BIT
	char *cp;

	through = ((cp = strrchr(argv[2], '.')) && !strcmp(++cp, "h")) ? 0 : 1;
#endif

	in = fopen(argv[1], "r");
	out = fopen(argv[2], "w");

	mode = ASCII;
	esc = kanji = 0;

	while ((c = fgetc(in)) != EOF) {
		switch (c) {
			case '\033':	/* ESC */
				esc = 1;
				break;
			case '$':
				if (esc) {
					kanji = 1;
					esc = 0;
				}
				else output(out, c, mode);
				break;
			case '(':
				if (esc) {
					kanji = -1;
					esc = 0;
				}
				else output(out, c, mode);
				break;
			case '\016':	/* SI */
				mode |= KANA;
				break;
			case '\017':	/* SO */
				mode &= ~KANA;
				break;
			default:
				if (esc) output(out, '\033', mode);
				if (kanji > 0) mode |= KANJI;
				else if (kanji < 0) mode &= ~KANJI;
				else output(out, c, mode);
				esc = kanji = 0;
				break;
		}
	}

	fclose(out);
	fclose(in);

	exit(0);
}
