/*
 *	mkfuncno.c
 *
 *	Preprocesser for "funcno.h"
 */

#include <stdio.h>
#include <string.h>


int main(argc, argv)
int argc;
char *argv[];
{
	FILE *in, *out;
	int c, lebel, cp, no, isstr;
	char buf[32];

	in = fopen(argv[1], "r");
	out = fopen(argv[2], "w");

	lebel = no = isstr = 0;
	cp = -1;

	fprintf(out, "/*\n");
	fprintf(out, " *\t%s\n", argv[1]);
	fprintf(out, " *\n");
	fprintf(out, " *\tFunction No. Table\n");
	fprintf(out, " */\n");
	fprintf(out, "\n");

	while ((c = fgetc(in)) != EOF) {
		switch (c) {
			case '\t':
			case ' ':
				break;
			case '{':
				lebel++;
				break;
			case '}':
				lebel--;
				break;
			case '"':
				if (lebel != 2) break;
				isstr = 1 - isstr;
				if (isstr) cp = 0;
				else {
					buf[cp] = '\0';
					fprintf(out, "#define\t%s\t", buf);
					while ((cp += 8) < 16) fputc('\t', out);
					fprintf(out, "%d\n", no);
					no++;
					cp = -1;
				}
				break;
			default:
				if (cp >= 0) buf[cp++] = c;
				break;
		}
	}

	fclose(out);
	fclose(in);

	exit(0);
}
