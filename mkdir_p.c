/*
 *	mkdir_p.c
 *
 *	make directory with parent directories
 */

#define	K_EXTERN
#include "headers.h"
#include "kctype.h"
#include "typesize.h"

#if	MSDOS && !defined (DJGPP)
#undef	MAXPATHLEN
#define	MAXPATHLEN		260
#define	unixmkdir(p, m)		((mkdir(p)) ? -1 : 0)
#else
#define	unixmkdir(p, m)		((mkdir(p,m)) ? -1 : 0)
#endif

static int a2octal __P_((CONST char *));
static char *strrdelim __P_((CONST char *));
static int mkdir_p __P_((CONST char *, int));
int main __P_((int, char *CONST []));


static int a2octal(s)
CONST char *s;
{
	CONST char *cp;
	int n;

	if (!s) return(-1);

	n = 0;
	for (cp = s; *cp; cp++) {
		if (*cp < '0' || *cp > '7') break;
		n *= 8;
		n += *cp - '0';
	}
	if (cp <= s) return(-1);

	return(n);
}

static char *strrdelim(s)
CONST char *s;
{
	CONST char *cp;

	cp = NULL;
	for (; *s; s++) {
		if (*s == _SC_) cp = s;
#ifdef	BSPATHDELIM
		if (iskanji1(s, 0)) s++;
#endif
	}

	return((char *)cp);
}

static int mkdir_p(path, mode)
CONST char *path;
int mode;
{
	char *cp, buf[MAXPATHLEN];
	ALLOC_T len;

	if (unixmkdir(path, mode) >= 0) return(0);
	if (errno != ENOENT) return(-1);

	cp = strrdelim(path);
	if (!cp || cp < path) {
		errno = ENOENT;
		return(-1);
	}
	if ((len = cp - path) >= strsize(buf)) {
		errno = ENAMETOOLONG;
		return(-1);
	}

	memcpy(buf, path, len);
	buf[len] = '\0';
	if (mkdir_p(buf, mode) < 0) return(-1);

	return(unixmkdir(path, mode));
}

int main(argc, argv)
int argc;
char *CONST argv[];
{
	CONST char *cp;
	int n, mode;

	mode = 0777;
	for (n = 1; n < argc; n++) {
		if (argv[n][0] != '-' || !argv[n][1]) break;
		if (argv[n][1] == '-' && !argv[n][2]) {
			n++;
			break;
		}
		if (argv[n][1] == 'm') {
			cp = &(argv[n][2]);
			if (!*cp) cp = argv[++n];
			if ((mode = a2octal(cp)) < 0) {
				n = argc;
				break;
			}
		}
	}

	if (n >= argc || n + 1 < argc) {
		fprintf(stderr, "Usage: mkdir_p [-m <mode>] <directory>\n");
		return(1);
	}
	if (mkdir_p(argv[n], mode) < 0) {
		fprintf(stderr, "%s: cannot make directory.\n", argv[n]);
		return(1);
	}

	return(0);
}
