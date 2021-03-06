/*
 *	mkfuncno.c
 *
 *	preprocesser for "funcno.h"
 */

#define	__FD_PRIMAL__
#include "fd.h"
#include "types.h"
#ifdef	_NOCATALOG
#define	_TBL_(fn, id, hl, fl)	{NULL, id, NULL, 0}
#else
#define	_TBL_(fn, id, hl, fl)	{NULL, id, 0, 0}
#endif
#include "functabl.h"

int main __P_((int, char *CONST []));


/*ARGSUSED*/
int main(argc, argv)
int argc;
char *CONST argv[];
{
	FILE *fp;
	int i, len;

	if (!strcmp(argv[1], "-")) fp = stdout;
	else if (!(fp = fopen(argv[1], "w"))) {
		fprintf(stderr, "%s: Cannot open.\n", argv[1]);
		return(1);
	}

	fprintf(fp, "/*\n");
	fprintf(fp, " *\t%s\n", (fp != stdout) ? argv[1] : "STDOUT");
	fprintf(fp, " *\n");
	fprintf(fp, " *\tfunction No. table\n");
	fprintf(fp, " */\n");
	fprintf(fp, "\n");

	for (i = 0; i < (int)sizeof(funclist) / sizeof(functable); i++) {
		fprintf(fp, "#define\t%s\t", funclist[i].ident);
		len = strlen(funclist[i].ident);
		while ((len += 8) < 16) fputc('\t', fp);
		fprintf(fp, "\t%d\n", i);
	}
	fprintf(fp, "\n#define\tFUNCLISTSIZ\t\t%d\n", i);

	if (fp != stdout) VOID_C fclose(fp);

	return(0);
}
