/*
 *	mkfuncno.c
 *
 *	Preprocesser for "funcno.h"
 */

#include <stdio.h>
#include <string.h>

static int readfunctable();


static int readfunctable(in, out)
FILE *in, *out;
{
	int c, lebel, cp, no, isstr, done;
	char buf[32];

	lebel = no = isstr = done = 0;
	cp = -1;

	while ((c = fgetc(in)) != EOF) {
		if (isstr) {
			if (c == '"') {
				isstr = 0;
				if (cp >= 0) {
					buf[cp] = '\0';
					fprintf(out, "#define\t%s\t", buf);
					while ((cp += 8) < 16) fputc('\t', out);
					fprintf(out, "%d\n", no);
					no++;
					cp = -1;
					done = 1;
				}
			}
			else if (cp >= 0) buf[cp++] = c;
		}
		else switch (c) {
			case '\t':
			case ' ':
				break;
			case '{':
				if (++lebel == 2) cp = 0;
				break;
			case '}':
				if (--lebel == 0 && done) return(0);
				break;
			case '"':
				isstr = 1;
				break;
			default:
				break;
		}
	}
	return(EOF);
}

int main(argc, argv)
int argc;
char *argv[];
{
	FILE *in, *out;

	in = (strcmp(argv[1], "-")) ? fopen(argv[1], "r") : stdin;
	out = (strcmp(argv[2], "-")) ? fopen(argv[2], "w") : stdout;

	fprintf(out, "/*\n");
	fprintf(out, " *\t%s\n", (out != stdout) ? argv[2] : "STDOUT");
	fprintf(out, " *\n");
	fprintf(out, " *\tFunction No. Table\n");
	fprintf(out, " */\n");
	fprintf(out, "\n");

	readfunctable(in, out);

	if (out != stdout) fclose(out);
	if (in != stdin) fclose(in);

	exit(0);
}
