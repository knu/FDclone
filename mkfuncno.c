/*
 *	mkfuncno.c
 *
 *	Preprocesser for "funcno.h"
 */

#include <stdio.h>
#include <string.h>

#ifdef	__STDC__
#define	__P_(args)	args
#define	VOID		void
#else
#define	__P_(args)	()
#define	VOID
#endif

static int fgetc2 __P_((FILE *));
static int readfunctable __P_((FILE *, FILE *));


static int fgetc2(fp)
FILE *fp;
{
	static int prev = '\n';
	int c;

	if ((c = fgetc(fp)) == '#' && prev == '\n') {
		while ((c = fgetc(fp)) != EOF && c != '\n');
	}
	prev = c;

	return(c);
}

static int readfunctable(in, out)
FILE *in, *out;
{
	int c, lebel, cp, no, isstr, ignore, done;
	char buf[32];

	lebel = no = isstr = ignore = done = 0;
	cp = -1;

	while ((c = fgetc2(in)) != EOF) {
		if (isstr) {
			if (c == '"') {
				isstr = 0;
				if (cp > 0) {
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
			case '(':
				ignore++;
				break;
			case ')':
				if (ignore) ignore--;
				break;
			case '{':
				if (++lebel == 2 && !ignore) cp = 0;
				break;
			case '}':
				if (--lebel == 0 && done) return(0);
				break;
			case '"':
				if (!ignore) isstr = 1;
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

	if (!in || !out) {
		fprintf(stderr, "Cannot open file.\n");
		return(1);
	}

	fprintf(out, "/*\n");
	fprintf(out, " *\t%s\n", (out != stdout) ? argv[2] : "STDOUT");
	fprintf(out, " *\n");
	fprintf(out, " *\tFunction No. Table\n");
	fprintf(out, " */\n");
	fprintf(out, "\n");

	readfunctable(in, out);
	while (fgetc(in) != EOF);

	if (in != stdin) fclose(in);
	if (out != stdout) fclose(out);

	return(0);
}
