/*
 *	mkfuncno.c
 *
 *	Preprocesser for "funcno.h"
 */

#define	__FD_PRIMAL__
#include "fd.h"

#define	_TBL_(func, id, hlp, flg)	{NULL, id, NULL, 0}

#include "types.h"
#include "functabl.h"


/*ARGSUSED*/
int main(argc, argv)
int argc;
char *argv[];
{
	FILE *fp;
	int i, len;

	if (!strcmp(argv[1], "-")) fp = stdout;
	else if (!(fp = fopen(argv[1], "w"))) {
		fprintf(stderr, "Cannot open file.\n");
		return(1);
	}

	fprintf(fp, "/*\n");
	fprintf(fp, " *\t%s\n", (fp != stdout) ? argv[1] : "STDOUT");
	fprintf(fp, " *\n");
	fprintf(fp, " *\tFunction No. Table\n");
	fprintf(fp, " */\n");
	fprintf(fp, "\n");

	for (i = 0; i < sizeof(funclist) / sizeof(functable); i++) {
		fprintf(fp, "#define\t%s\t", funclist[i].ident);
		len = strlen(funclist[i].ident);
		while ((len += 8) < 16) fputc('\t', fp);
		fprintf(fp, "%d\n", i);
	}
	fprintf(fp, "\n#define\tFUNCLISTSIZ\t%d\n", i);

	if (fp != stdout) fclose(fp);

	return(0);
}
