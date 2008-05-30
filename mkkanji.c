/*
 *	mkkanji.c
 *
 *	preprocesser for "kanji.h"
 */

#include "headers.h"
#include "typesize.h"

#define	MAXLINEBUF		255
#define	MESCONVSTR		"mesconv"
#define	MESLISTSTR		"meslist"
#define	MESCONVSTR2		"mesconv2"
#define	MESLISTSTR2		"meslist"

int main __P_((int, char *CONST []));


int main(argc, argv)
int argc;
char *CONST argv[];
{
	FILE *fpin, *fpout;
	char *cp, *s, buf[MAXLINEBUF + 1];
	long n;
	u_short sum;
	int i, len, quote;

	if (argc <= 1 || argc > 3) {
		fprintf(stderr, "Usage: mkkanji <infile> [<outfile>]\n");
		return(1);
	}
	if (!strcmp(argv[1], "-")) fpin = stdin;
	else if (!(fpin = fopen(argv[1], "r"))) {
		fprintf(stderr, "%s: cannot open.\n", argv[1]);
		return(1);
	}
	if (argc <= 2) fpout = stdout;
	else if (!(fpout = fopen(argv[2], "w"))) {
		fprintf(stderr, "%s: cannot open.\n", argv[2]);
		fclose(fpin);
		return(1);
	}

	n = 0L;
	sum = (u_short)0;
	while (fgets(buf, sizeof(buf), fpin)) {
		s = NULL;
		len = 0;
		for (cp = buf; cp; cp = strpbrk(cp, " \t")) {
			while (*cp == ' ' || *cp == '\t') cp++;
			if (!*cp) break;
			if (!strncmp(cp, MESCONVSTR, strsize(MESCONVSTR))) {
				s = MESCONVSTR2;
				len = strsize(MESCONVSTR);
				break;
			}
			if (!strncmp(cp, MESLISTSTR, strsize(MESLISTSTR))) {
				s = MESLISTSTR2;
				len = strsize(MESLISTSTR);
				break;
			}
		}
		if (s) {
			for (i = len; cp[i]; i++)
				if (cp[i] != ' ' && cp[i] != '\t') break;
			if (cp[i] != '(') s = NULL;
			else {
				len = cp - buf;
				cp += ++i;
				while (*cp == ' ' || *cp == '\t') cp++;
			}
		}

		if (!s) fputs(buf, fpout);
		else if (++n >= MAXUTYPE(u_short)) {
			fprintf(stderr, "too many entries.\n");
			fclose(fpin);
			fclose(fpout);
			return(1);
		}
		else {
			quote = '\0';
			for (i = 0; cp[i]; i++) {
				if (cp[i] == quote) quote = '\0';
				else if (quote) sum += cp[i];
				else if (cp[i] == '"') quote = cp[i];
			}
			fprintf(fpout, "%-.*s%s(%3ld, %s", len, buf, s, n, cp);
		}
	}
	fprintf(fpout, "\n#define\tCAT_SUM\t%u\n", (u_int)sum & 0xffff);

	return(0);
}
