/*
 *	tree.c
 *
 *	Tree Mode Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"

#ifndef	_NOTREE

#include <signal.h>

#if	MSDOS
#include "unixemu.h"
#else
#include <sys/param.h>
#endif

extern char fullpath[];
#ifndef	_NOARCHIVE
extern char *archivefile;
#endif
extern int subwindow;
extern int sorton;
#if	!MSDOS && !defined (_NODOSDRIVE)
extern int lastdrv;
#endif
extern int sizeinfo;

#define	DIRFIELD	3
#define	TREEFIELD	(((dircountlimit > 0) ? (n_column * 3) / 5 : n_column)\
			- 2)
#define	FILEFIELD	((dircountlimit > 0) ? (n_column * 2) / 5 - 3 : 0)
#define	bufptr(buf, y)	(&buf[(y - WHEADER - 1) * (TREEFIELD + 1)])

static int evaldir();
static treelist *maketree();
static int _showtree();
static VOID showtree();
static treelist *_searchtree();
static treelist *searchtree();
static int expandtree();
static int expandall();
static treelist *checkmisc();
static treelist *treeup();
static treelist *treedown();
static VOID freetree();
static treelist *_tree_search();
static int _tree_input();
static char *_tree();

int sorttree;
int dircountlimit;

static int redraw;
static int tr_no;
static int tr_line;
static int tr_top;
static int tr_bottom;


static int evaldir(dir, disp)
char *dir;
int disp;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char path[MAXPATHLEN + 1];
	int i, x, y, w, len, limit;
#if	MSDOS
	char *cp;
#endif

	if ((limit = dircountlimit) <= 0) return(0);

	if (disp) for (i = WHEADER + 1; i < n_line - WFOOTER; i++) {
		locate(TREEFIELD + 2, i);
		putch2('|');
		putterm(l_clear);
	}
	strcpy(path, dir);
#if	MSDOS
	cp = path;
	if (_dospath(cp)) cp += 2;
	if (strcmp(cp, _SS_)) strcat(path, _SS_);
#else
	if (strcmp(path, _SS_)) strcat(path, _SS_);
#endif
	len = strlen(path);
	i = x = 0;
	y = WHEADER + 1;
	w = FILEFIELD / 2;
	if (!(dirp = Xopendir(dir))) return(0);
	while (dp = Xreaddir(dirp)) {
		if (isdotdir(dp -> d_name)) continue;
		strcpy(path + len, dp -> d_name);
		if (limit-- <= 0
		|| (stat2(path, &status) >= 0
		&& (status.st_mode & S_IFMT) == S_IFDIR)) {
			if (!disp) {
				i++;
				break;
			}
		}
		else if (disp) {
			locate(x + TREEFIELD + 4, y);
			kanjiputs2(dp -> d_name, w, 0);
			i++;
			if (++y >= n_line - WFOOTER) {
				y = WHEADER + 1;
				x += w + 1;
				if (x >= w * 2 + 2) break;
			}
		}
	}
	if (disp && !i) {
		locate(x + TREEFIELD + 4, WHEADER + 1);
		kanjiputs2("[No Files]", w, 0);
	}
	Xclosedir(dirp);
	return(i);
}

static treelist *maketree(path, list, level, maxp, maxentp)
char *path;
treelist *list;
int level, *maxp, *maxentp;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char *cp, *dir, *subdir;
	int i, len;

	if ((level + 1) * DIRFIELD + 2 > TREEFIELD || kbhit2(0)) {
		*maxp = -1;
		return(NULL);
	}
#if	MSDOS
	if (_dospath(path)) path += 2;
#endif
	if (*path == _SC_) {
		dir = strdup2(_SS_);
		subdir = path + 1;
		if (!*subdir) subdir = NULL;
	}
#if	!MSDOS && !defined (_NODOSDRIVE)
	else if (_dospath(path)) {
		dir = (char *)malloc2(3 + 1);
		strncpy2(dir, path, 3);
		subdir = path + 3;
		if (!*subdir) subdir = NULL;
	}
#endif
	else {
		len = (cp = strchr(path, _SC_)) ? cp - path : strlen(path);
		dir = (char *)malloc2(len + 1);
		strncpy2(dir, path, len);
		subdir = (cp) ? cp + 1 : NULL;
	}

	if (!subdir) len = 0;
	else len = (cp = strchr(subdir, _SC_)) ? cp - subdir : strlen(subdir);

	*maxp = *maxentp = 0;
	i = _chdir2(dir);
	free(dir);
	if (i < 0 || !(dirp = Xopendir("."))) return(NULL);

	cp = NULL;
	while (dp = Xreaddir(dirp)) {
		if (isdotdir(dp -> d_name)
		|| Xstat(dp -> d_name, &status) < 0
		|| (status.st_mode & S_IFMT) != S_IFDIR) continue;
		list = (treelist *)addlist(list, *maxp,
			maxentp, sizeof(treelist));
		if (!subdir) {
			list[*maxp].name = strdup2(dp -> d_name);
			list[*maxp].next = NULL;
			list[*maxp].max = list[*maxp].maxent =
				(evaldir(dp -> d_name, 0)) ? -1 : 0;
			(*maxp)++;
		}
		else if (!strnpathcmp(dp -> d_name, subdir, len)
		&& strlen(dp -> d_name) == len) {
			list[0].name = strdup2(dp -> d_name);
			list[0].next = maketree(subdir, NULL, level + 1,
				&(list[0].max), &(list[0].maxent));
			if (list[0].max < 0) list[0].maxent = -1;
			if (++(*maxp) >= 2) break;
		}
		else if (!cp) {
			list[1].name = cp = strdup2("...");
			list[1].next = NULL;
			list[1].max = -1;
			list[1].maxent = 0;
			if (++(*maxp) >= 2) break;
		}
	}
	Xclosedir(dirp);
	if (sorttree && sorton) qsort(list, *maxp, sizeof(treelist), cmptree);

#if	MSDOS
	if (_dospath(path)) path += 2;
	if (*path == _SC_) {
#else	/* !MSDOS */
	if (*path == _SC_
#ifndef	_NODOSDRIVE
	|| _dospath(path)
#endif
	) {
#endif	/* !MSDOS */
		if (_chdir2(fullpath) < 0) error(fullpath);
	}
	else if (strcmp(path, ".") && _chdir2("..") < 0) error("..");
	return(list);
}

static int _showtree(buf, list, max, nest, y)
char *buf;
treelist *list;
int max, nest, y;
{
	char *cp;
	int i, j, w, tmp;

	w = TREEFIELD - (nest * DIRFIELD) - 1;
	tmp = y;
	for (i = 0; i < max; i++) {
		if (nest > 0) {
			for (j = tmp; j < y; j++)
			if (j > WHEADER && j < n_line - WFOOTER)
				*(bufptr(buf, j) + (nest - 1) * DIRFIELD) = '|';
		}
		if (y > WHEADER && y < n_line - WFOOTER) {
			cp = bufptr(buf, y);
			if (nest > 0)
				memcpy(&cp[(nest - 1) * DIRFIELD], "+--", 3);
			if ((tmp = strlen(list[i].name)) > w) tmp = w;
			memcpy(&cp[nest * DIRFIELD], list[i].name, tmp);
			if (list[i].maxent < 0) cp[nest * DIRFIELD + tmp] = '>';
		}
		y++;
		tmp = y;
		if (w >= DIRFIELD && list[i].next)
			y = _showtree(buf, list[i].next, list[i].max,
				nest + 1, y);
	}
	return(y);
}

static VOID showtree(path, buf, list, max, nest)
char *path, *buf;
treelist *list;
int max, nest;
{
	int i;

	for (i = WHEADER; i <= n_line - WFOOTER; i++) {
		locate(0, i);
		putterm(l_clear);
	}
	for (i = WHEADER + 1; i < n_line - WFOOTER; i++)
		sprintf(bufptr(buf, i), "%-*.*s", TREEFIELD, TREEFIELD, " ");
	_showtree(buf, list, max, nest, tr_top);
	for (i = WHEADER + 1; i < n_line - WFOOTER; i++) {
		locate(1, i);
		if (i == tr_line) putterm(t_standout);
		kanjiputs(bufptr(buf, i));
		if (i == tr_line) putterm(end_standout);
	}
	evaldir(path, 1);
	keyflush();
}

static treelist *_searchtree(path, list, max, nest)
char *path;
treelist *list;
int max, nest;
{
	treelist *lp, *tmplp;
	int i, j, w, len;

	lp = NULL;
	w = TREEFIELD - (nest * DIRFIELD) - 1;
	for (i = 0; i < max; i++) {
		if (tr_bottom == tr_line) {
			lp = (treelist *)-1;
			tr_no = i;
			strcpy(path, list[i].name);
		}
		tr_bottom++;
		if (w >= DIRFIELD && list[i].next) {
			tmplp = _searchtree(path, list[i].next, list[i].max,
				nest + 1);
			if (tmplp) {
				lp = (tmplp == (treelist *)-1)
					? &list[i] : tmplp;
				len = strlen(list[i].name);
#if	MSDOS
				if (!_dospath(list[i].name)
				|| list[i].name[2] != _SC_) len++;
				for (j = strlen(path); j >= 0; j--)
					path[j + len] = path[j];
				memcpy(path, list[i].name, len - 1);
#else
				if (*list[i].name != _SC_
#ifndef	_NODOSDRIVE
				&& !_dospath(list[i].name)
#endif
				) len++;
				for (j = strlen(path); j >= 0; j--)
					path[j + len] = path[j];
				if (*list[i].name != _SC_)
					memcpy(path, list[i].name, len - 1);
#endif
				path[len - 1] = _SC_;
			}
		}
	}
	return(lp);
}

static treelist *searchtree(path, list, max, nest)
char *path;
treelist *list;
int max, nest;
{
	treelist *lp;

	tr_bottom = tr_top;
	lp = _searchtree(path, list, max, nest);
	return((lp == (treelist *)-1) ? list : lp);
}

static int expandtree(list, path)
treelist *list;
char *path;
{
	treelist *lp, *lptmp;
	char *cp;
	int i;

	if (list -> next) {
		lp = (treelist *)malloc2(sizeof(treelist));
		memcpy(lp, &(list -> next[0]), sizeof(treelist));
		for (i = 1; i < list -> max; i++) free(list -> next[i].name);
	}

	if (_chdir2(path) < 0) {
		if (list -> next) error(path);
		list -> max = list -> maxent = 0;
		return(1);
	}
	for (cp = path, i = 0; cp = strchr(cp, _SC_); cp++, i++);
	lptmp = maketree(".", list -> next, i,
		&(list -> max), &(list -> maxent));
	if (_chdir2(fullpath) < 0) error(fullpath);
	if (list -> max < 0) {
		if (list -> next) {
			list -> max = 2;
			list -> next[1].name = strdup2("...");
			free(lp);
		}
		return(0);
	}
	if (list -> next) {
		for (i = 0; i < list -> max; i++)
			if (!strpathcmp(lp -> name, lptmp[i].name)) break;
		if (i < list -> max) {
			free(lptmp[i].name);
			for (; i > 0; i--) memcpy(&lptmp[i], &lptmp[i - 1],
					sizeof(treelist));
			memcpy(&lptmp[0], lp, sizeof(treelist));
		}
		free(lp);
	}
	list -> next = lptmp;
	return(1);
}

static int expandall(list, path)
treelist *list;
char *path;
{
	char *cp;
	int i;

	if (!list || list -> maxent >= 0) return(1);
	if (!expandtree(list, path)) return(0);
	if (!(list -> next)) return(1);
	cp = path + strlen(path);
	if (cp > path + 1) {
		strcat(path, _SS_);
		cp++;
	}
	for (i = 0; i < list -> max; i++) {
		strcpy(cp, list -> next[i].name);
		if (!expandall(&(list -> next[i]), path)) return(0);
	}
	return(1);
}

static treelist *checkmisc(list, lp, path)
treelist *list, *lp;
char *path;
{
	char *cp;

	if (!lp || !(lp -> next)
	|| lp -> next[tr_no].max >= 0
	|| lp -> next[tr_no].maxent < 0) return(NULL);

	waitmes();
	if ((cp = strrchr(path, _SC_)) == strchr(path, _SC_)) cp++;
	*cp = '\0';
	if (!(expandtree(lp, path))) return((treelist *)-1);
	return(searchtree(path, list, 1, 0));
}

static treelist *treeup(path, list)
char *path;
treelist *list;
{
	treelist *lp;

	if (tr_line > WHEADER + 1) tr_line--;
	else if (tr_top <= WHEADER) tr_top++;
	else return(NULL);
	lp = searchtree(path, list, 1, 0);
	return(lp);
}

static treelist *treedown(path, list)
char *path;
treelist *list;
{
	treelist *lp, *lptmp;
	int oy, otop;

	oy = tr_line;
	otop = tr_top;
	if (tr_line < tr_bottom - 1 && tr_line < n_line - WFOOTER - 2)
		tr_line++;
	else if (tr_bottom >= n_line - WFOOTER) tr_top--;
	else return(NULL);
	lp = searchtree(path, list, 1, 0);
	lptmp = checkmisc(list, lp, path);
	if (lptmp) {
		if ((lp = lptmp) != (treelist *)-1) redraw = 1;
		else {
			tr_line = oy;
			tr_top = otop;
			searchtree(path, list, 1, 0);
			lp = NULL;
		}
	}
	return(lp);
}

static VOID freetree(list, max)
treelist *list;
int max;
{
	int i;

	for (i = 0; i < max; i++) {
		free(list[i].name);
		if (list[i].next) freetree(list[i].next, list[i].max);
	}
	free(list);
}

static treelist *_tree_search(path, list, lp)
char *path;
treelist *list, *lp;
{
	treelist *lptmp;
	int oy, otop;

	oy = tr_line;
	otop = tr_top;

	while (strpathcmp(fullpath, path)) {
		lptmp = treeup(path, list);
		if (lptmp) lp = lptmp;
		else break;
	}
	if (strpathcmp(fullpath, path)) {
		tr_top = otop;
		tr_line = oy;
		lp = searchtree(path, list, 1, 0);
		do {
			lptmp = treedown(path, list);
			if (lptmp) lp = lptmp;
			else break;
		} while (strpathcmp(fullpath, path));
	}

	return(lp);
}

static int _tree_input(path, cpp, list, lpp)
char *path, **cpp;
treelist *list, **lpp;
{
	treelist *olp, *lptmp;
	char *cwd;
	int ch, tmp, half;

	olp = *lpp;

	switch (ch = Xgetkey(SIGALRM)) {
		case K_UP:
			lptmp = treeup(path, list);
			if (lptmp) *lpp = lptmp;
			break;
		case K_DOWN:
			lptmp = treedown(path, list);
			if (lptmp) *lpp = lptmp;
			break;
		case K_PPAGE:
			half = (n_line - WHEADER - WFOOTER - 1) / 2;
			tr_line = WHEADER + half + 1;
			if (tr_top + half > WHEADER + 1)
				half = WHEADER - tr_top + 1;
			if (half > 0) tr_top += half;
			*lpp = searchtree(path, list, 1, 0);
			break;
		case K_NPAGE:
			half = (n_line - WHEADER - WFOOTER - 1) / 2;
			tmp = half + (WHEADER + half + 1) - tr_line;
			while (tmp-- > 0) {
				lptmp = treedown(path, list);
				if (lptmp) *lpp = lptmp;
				else break;
			}
			tmp = tr_line - (WHEADER + half + 1);
			tr_line = WHEADER + half + 1;
			if (tr_bottom - tmp < n_line - WFOOTER - 1)
				tmp = tr_bottom - n_line + WFOOTER + 1;
			tr_top -= tmp;
			*lpp = searchtree(path, list, 1, 0);
			break;
		case K_BEG:
		case K_HOME:
		case '<':
			tr_line = tr_top = WHEADER + 1;
			*lpp = searchtree(path, list, 1, 0);
			break;
		case K_EOL:
		case K_END:
		case '>':
			for (;;) {
				lptmp = treedown(path, list);
				if (lptmp) *lpp = lptmp;
				else break;
			}
			break;
		case '?':
			*lpp = _tree_search(path, list, *lpp);
			break;
		case '(':
			tmp = tr_no - 1;
			do {
				lptmp = treeup(path, list);
				if (lptmp) *lpp = lptmp;
				else break;
			} while (tmp >= 0 && (*lpp != olp || tr_no != tmp));
			break;
		case ')':
			tmp = tr_no + 1;
			do {
				lptmp = treedown(path, list);
				if (lptmp) *lpp = lptmp;
				else break;
			} while (tmp < olp -> max
			&& (*lpp != olp || tr_no != tmp));
			break;
		case '\t':
			if (!(*lpp) || !((*lpp) -> next)
			|| (*lpp) -> next[tr_no].maxent >= 0) break;
			waitmes();
			expandall(&((*lpp) -> next[tr_no]), path);
			*lpp = searchtree(path, list, 1, 0);
			redraw = 1;
			break;
		case K_RIGHT:
			if (!(*lpp) || !((*lpp) -> next)
			|| (*lpp) -> next[tr_no].maxent >= 0) {
				if (!((*lpp) -> next[tr_no].next)) break;
				lptmp = treedown(path, list);
				if (lptmp) *lpp = lptmp;
				break;
			}
			waitmes();
			expandtree(&((*lpp) -> next[tr_no]), path);
			*lpp = searchtree(path, list, 1, 0);
			redraw = 1;
			break;
		case K_LEFT:
			if ((*lpp) && ((*lpp) -> next)
			&& (*lpp) -> next[tr_no].next) {
				freetree((*lpp) -> next[tr_no].next,
					(*lpp) -> next[tr_no].max);
				(*lpp) -> next[tr_no].max =
				(*lpp) -> next[tr_no].maxent = -1;
				(*lpp) -> next[tr_no].next = NULL;
				redraw = 1;
				break;
			}
		case K_BS:
			do {
				lptmp = treeup(path, list);
				if (lptmp) *lpp = lptmp;
				else break;
			} while (&((*lpp) -> next[tr_no]) != olp);
			break;
		case 'l':
			ch = '\0';
			if (!(cwd = inputstr(LOGD_K, 0, -1, NULL, 1))
			|| !*(cwd = evalpath(cwd))) break;
			if (chdir2(cwd) >= 0) {
				free(cwd);
				return('l');
			}
			warning(-1, cwd);
			free(cwd);
			break;
		case CTRL('L'):
			*cpp = (char *)realloc2(*cpp, (n_line - WHEADER
				- WFOOTER - 1) * (TREEFIELD + 1));
			*lpp = searchtree(path, list, 1, 0);
			redraw = 1;
			break;
		case ESC:
			break;
		default:
			if (ch < 'A' || ch > 'Z') break;
			if (tr_line == tr_bottom - 1) {
				tr_line = tr_top = WHEADER + 1;
				*lpp = searchtree(path, list, 1, 0);
			}
			else do {
				lptmp = treedown(path, list);
				if (lptmp) *lpp = lptmp;
				else break;
			} while (toupper2(*((*lpp) -> next[tr_no].name)) != ch);
			break;
	}

	return(ch);
}

static char *_tree()
{
	treelist *list, *lp;
	char *cp, *cwd, path[MAXPATHLEN + 1];
	int ch, oy, otop;

	keyflush();
	waitmes();
	list = (treelist *)malloc2(sizeof(treelist));
#if	MSDOS
	list[0].name = (char *)malloc2(3 + 1);
	strcpy(path, fullpath);
	strncpy2(list[0].name, path, 3);
#else	/* !MSDOS */
#ifndef	_NODOSDRIVE
	if (dospath("", path)) {
		list[0].name = (char *)malloc2(3 + 1);
		strncpy2(list[0].name, path, 3);
	}
	else
#endif
	{
		strcpy(path, fullpath);
		list[0].name = strdup2(_SS_);
	}
#endif	/* !MSDOS */
	list[0].next = maketree(path, NULL, 0,
		&(list[0].max), &(list[0].maxent));

	tr_line = 0;
#if	MSDOS
	cwd = path + 2;
#else	/* !MSDOS */
	cwd = path;
#ifndef	_NODOSDRIVE
	if (_dospath(cwd)) cwd += 2;
#endif
#endif	/* !MSDOS */
	if (strcmp(cwd, _SS_))
		for (cp = cwd; cp = strchr(cp, _SC_); cp++, tr_line++)
			if ((tr_line + 1) * DIRFIELD + 2 > TREEFIELD) break;
	tr_line += (tr_top = WHEADER + 1);
	if (tr_line >= n_line - WFOOTER - 2) {
		tr_top += n_line - WFOOTER - 2 - tr_line;
		tr_line = n_line - WFOOTER - 2;
	}

	cp = (char *)malloc2((n_line - WHEADER - WFOOTER - 1)
		* (TREEFIELD + 1));
	lp = searchtree(path, list, 1, 0);
	showtree(path, cp, list, 1, 0);
	do {
		locate(1, WHEADER);
		cputs2("Tree=");
		kanjiputs2(path, n_column - 6, 0);
		locate(0, LMESLINE);
		putterm(l_clear);
		locate(0, 0);
		tflush();
		oy = tr_line;
		otop = tr_top;
		redraw = 0;

		if ((ch = _tree_input(path, &cp, list, &lp)) == 'l') {
			free(cp);
			freetree(list, 1);
			return(fullpath);
		}

		if (redraw || tr_top != otop)
			showtree(path, cp, list, 1, 0);
		else if (oy != tr_line) {
			locate(1, tr_line);
			putterm(t_standout);
			kanjiputs(bufptr(cp, tr_line));
			putterm(end_standout);
			locate(1, oy);
			if (stable_standout) putterm(end_standout);
			else kanjiputs(bufptr(cp, oy));
			evaldir(path, 1);
		}
	} while (ch != ESC && ch != CR);

	free(cp);
	freetree(list, 1);
	if (ch == ESC) return(NULL);
	return(strdup2(path));
}

/*ARGSUSED*/
char *tree(cleanup, ddp)
int cleanup, *ddp;
{
	char *path, *treepath;

	subwindow = 1;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif
	treepath = strdup2(fullpath);
	do {
		path = _tree();
	} while (path == fullpath);
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (ddp) {
		*ddp = lastdrv;
		lastdrv = -1;
		if (chdir2(treepath) < 0) error(treepath);
	}
#endif
	free(treepath);

	if (cleanup) {
#ifndef	_NOARCHIVE
		if (archivefile) rewritearc(1);
#endif
		rewritefile(1);
	}
	subwindow = 0;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif
	return(path);
}
#endif	/* !_NOTREE */
