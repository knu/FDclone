/*
 *	catalog.c
 *
 *	message catalog
 */

#include "fd.h"
#include "termio.h"
#include "func.h"
#include "kanji.h"

#ifdef	USEMMAP
#include <sys/mman.h>
#endif

#define	MAXVERBUF		8
#define	CF_KANJI		0001
#define	getword(s, n)		(((u_short)((s)[(n) + 1]) << 8) | (s)[n])
#define	CATTBL			"fd-cat."

#ifndef	_NOCATALOG
static int NEAR fgetword __P_((u_short *, int));
static int NEAR opencatalog __P_((CONST char *));

char *cattblpath = NULL;
CONST char *catname = NULL;

static u_char *catbuf = NULL;
static ALLOC_T catsize = (ALLOC_T)0;
static off_t catofs = (off_t)0;
static u_short *catindex = NULL;
static u_short catmax = (u_short)0;
static CONST char *cat_ja = "ja";
static CONST char *cat_C = "C";
#endif	/* !_NOCATALOG */


#ifdef	_NOCATALOG
# if	!defined (_NOENGMES) && !defined (_NOJPNMES)
CONST char *mesconv(jpn, eng)
CONST char *jpn, *eng;
{
	int n;

	if (outputkcode == ENG) return(eng);
	n = (messagelang != NOCNV) ? messagelang : outputkcode;
	return((n == ENG) ? eng : jpn);
}
# endif	/* !_NOENGMES && !_NOJPNMES */

#else	/* !_NOCATALOG */

static int NEAR fgetword(wp, fd)
u_short *wp;
int fd;
{
	u_char buf[2];

	if (sureread(fd, buf, 2) != 2) return(-1);
	*wp = getword(buf, 0);

	return(0);
}

static int NEAR opencatalog(lang)
CONST char *lang;
{
	char *cp, path[MAXPATHLEN], file[MAXPATHLEN], ver[MAXVERBUF];
	off_t ofs;
	ALLOC_T size;
	u_short w, flags;
	int n, fd;

	VOID_C Xsnprintf(file, sizeof(file), "%s%s", CATTBL, lang);
	if (!cattblpath || !*cattblpath) Xstrcpy(path, file);
	else strcatdelim2(path, cattblpath, file);

	if ((fd = open(path, O_BINARY | O_RDONLY, 0666)) < 0) return(-1);
	cp = getversion(&n);
	if (read(fd, ver, sizeof(ver)) != sizeof(ver)
	|| strncmp(cp, ver, n) || ver[n]
	|| fgetword(&flags, fd) < 0
	|| ((flags & CF_KANJI) && outputkcode == ENG)
	|| fgetword(&w, fd) < 0 || w != CAT_SUM
	|| fgetword(&w, fd) < 0) {
		VOID_C close(fd);
		return(-1);
	}

	if (!catbuf) /*EMPTY*/;
#ifdef	USEMMAP
	else if (catbuf == (u_char *)MAP_FAILED) /*EMPTY*/;
	else if (catsize) {
		if (munmap(catbuf, catsize) < 0) error("munmap()");
	}
#endif
	else Xfree(catbuf);
	catbuf = NULL;
	catsize = (ALLOC_T)0;
	catofs = (off_t)0;
	catindex = (u_short *)Xrealloc(catindex, w * sizeof(u_short));
	catmax = w;

	catindex[0] = (u_short)0;
	for (w = 1; w < catmax; w++) if (fgetword(&(catindex[w]), fd) < 0) {
		VOID_C close(fd);
		return(-1);
	}
	if (fgetword(&w, fd) < 0) {
		VOID_C close(fd);
		return(-1);
	}
	size = w;

	ofs = sizeof(ver) + 2 + 2 + 2 + 2 * catmax;
#ifdef	USEMMAP
	catbuf = (u_char *)mmap(NULL, (ALLOC_T)ofs + size,
		PROT_READ, MAP_PRIVATE, fd, (off_t)0);
	if (catbuf != (u_char *)MAP_FAILED) {
		catsize = (ALLOC_T)ofs + size;
		catofs = ofs;
	}
	else
#endif
	{
		catbuf = (u_char *)Xmalloc(size);
		if (lseek(fd, ofs, L_SET) < (off_t)0
		|| sureread(fd, catbuf, size) != size) {
			VOID_C close(fd);
			Xfree(catbuf);
			Xfree(catindex);
			catbuf = NULL;
			catindex = NULL;
			catmax = 0;
			return(-1);
		}
	}
	VOID_C close(fd);

	return(0);
}

VOID freecatalog(VOID_A)
{
	if (!catbuf) /*EMPTY*/;
#ifdef	USEMMAP
	else if (catbuf == (u_char *)MAP_FAILED) /*EMPTY*/;
	else if (catsize) VOID_C munmap(catbuf, catsize);
#endif
	else Xfree(catbuf);
	Xfree(catindex);

	catbuf = NULL;
	catsize = (ALLOC_T)0;
	catofs = (off_t)0;
	catindex = NULL;
	catmax = 0;
}

char **listcatalog(VOID_A)
{
	DIR *dirp;
	struct dirent *dp;
	CONST char *cp;
	char **argv;
	int argc;

	if (!cattblpath || !*cattblpath) cp = curpath;
	else cp = cattblpath;

	if (!(dirp = Xopendir(cp))) return(NULL);
	argv = NULL;
	argc = 0;
	while ((dp = Xreaddir(dirp))) {
		cp = dp -> d_name;
		if (strnpathcmp(cp, CATTBL, strsize(CATTBL))) continue;
		cp += strsize(CATTBL);
		if (!strpathcmp(cp, cat_ja)) continue;
		if (!strpathcmp(cp, cat_C)) continue;
		argv = (char **)Xrealloc(argv, (argc + 2) * sizeof(char *));
		argv[argc++] = Xstrdup(cp);
		argv[argc] = NULL;
	}
	VOID_C Xclosedir(dirp);

	return(argv);
}

int chkcatalog(VOID_A)
{
	CONST char *lang;

	if (messagelang != NOCNV) lang = (messagelang == ENG) ? cat_C : cat_ja;
	else if (catname && *catname) lang = catname;
	else lang = (outputkcode == ENG) ? cat_C : cat_ja;

	if (opencatalog(lang) >= 0) return(0);
	if (opencatalog(cat_ja) >= 0) return(0);
	if (opencatalog(cat_C) >= 0) return(0);

	return(-1);
}

CONST char *mesconv(id)
int id;
{
	off_t ofs;

	if (!catbuf && chkcatalog() < 0) return(nullstr);
	if (id < 0 || id >= catmax) return(nullstr);
	ofs = catindex[id] + catofs;

	return((CONST char *)&(catbuf[ofs]));
}
#endif	/* !_NOCATALOG */
