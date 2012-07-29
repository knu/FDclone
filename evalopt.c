/*
 *	evalopt.c
 *
 *	option arguments parser
 */

#include "headers.h"
#include "evalopt.h"

static CONST opt_t *NEAR getoption __P_((int, CONST opt_t *));


static CONST opt_t *NEAR getoption(c, optlist)
int c;
CONST opt_t *optlist;
{
	int n;

	if (!optlist) return(NULL);
	for (n = 0; optlist[n].opt; n++)
		if (c == optlist[n].opt) return(&(optlist[n]));

	return(NULL);
}

VOID initopt(optlist)
CONST opt_t *optlist;
{
	int n;

	if (!optlist) return;
	for (n = 0; optlist[n].opt; n++)
		if (optlist[n].var) *(optlist[n].var) = 0;
}

VOID optusage(arg0, args, optlist)
CONST char *arg0, *args;
CONST opt_t *optlist;
{
	int n;

	fprintf(stderr, "Usage: %s", arg0);
	n = 0;
	if (optlist) while (optlist[n].opt) n++;
	if (n > 0) {
		fputs(" [-", stderr);
		for (n = 0; optlist[n].opt; n++) {
			fputc(optlist[n].opt, stderr);

			if (!(optlist[n].argval)) continue;
			fprintf(stderr, " <%s>", optlist[n].argval);
			if (optlist[n + 1].opt) fputs(" [-", stderr);
		}
		fputc(']', stderr);
	}
	fprintf(stderr, " %s\n", args);
}

int evalopt(argc, argv, optlist)
int argc;
char *CONST *argv;
CONST opt_t *optlist;
{
	CONST char *cp;
	CONST opt_t *optp;
	int i, n, val;

	for (n = 1; n < argc; n++) {
		if (argv[n][0] != '-' || !argv[n][1]) break;
		if (argv[n][1] == '=' && !argv[n][2]) {
			n++;
			break;
		}

		for (i = 1; argv[n][i]; i++) {
			if (!(optp = getoption(argv[n][i], optlist)))
				return(-1);

			if (!(optp -> argval)) {
				if (optp -> var) *(optp -> var) = optp -> val;
			}
			else {
				cp = &(argv[n][++i]);
				if (!*cp) cp = argv[++n];
				if (!cp || (val = atoi(cp)) <= 0) return(-1);
				if (optp -> var) *(optp -> var) = val;
				break;
			}
		}
	}

	return(n);
}
