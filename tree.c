/*
 *	tree.c
 *
 *	Tree Mode Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"

#include <signal.h>
#include <sys/stat.h>
#include <sys/param.h>

extern char fullpath[];
extern char *archivefile;
extern int subwindow;
extern int sorton;
extern int lastdrv;

#define	DIRFIELD	3
#define	TREEFIELD	(((dircountlimit > 0) ? (n_column * 3) / 5 : n_column)\
			- 2)
#define	FILEFIELD	((dircountlimit > 0) ? (n_column * 2) / 5 - 3 : 0)
#define	bufptr(buf, y)	(&buf[(y - WHEADER - 1) * (TREEFIELD + 1)])

static int includedir();
static int includefile();
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
static char *_tree();

int sorttree;
int dircountlimit;


static int includedir(dir)
char *dir;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char path[MAXPATHLEN + 1];
	int i, len, limit;

	if ((limit = dircountlimit) <= 0) return(1);
	strcpy(path, dir);
	strcat(path, "/");
	len = strlen(path);
	i = 0;
	if (!(dirp = Xopendir(dir))) return(0);
	while (dp = Xreaddir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;
		strcpy(path + len, dp -> d_name);
		if (limit-- <= 0
		|| (Xstat(path, &status) == 0
		&& (status.st_mode & S_IFMT) == S_IFDIR)) {
			i++;
			break;
		}
	}
	Xclosedir(dirp);
	return(i);
}

static int includefile(dir)
char *dir;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char path[MAXPATHLEN + 1];
	int i, x, y, w, len, limit;

	if ((limit = dircountlimit) <= 0) return(0);

	for (i = WHEADER + 1; i < n_line - WFOOTER; i++) {
		locate(TREEFIELD + 2, i);
		putch('|');
		putterm(l_clear);
	}
	strcpy(path, dir);
	strcat(path, "/");
	len = strlen(path);
	i = x = 0;
	y = WHEADER + 1;
	w = FILEFIELD / 2;
	if (!(dirp = Xopendir(dir))) return(0);
	while (dp = Xreaddir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;
		strcpy(path + len, dp -> d_name);
		if (limit-- > 0
		&& stat2(path, &status) == 0
		&& (status.st_mode & S_IFMT) != S_IFDIR) {
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
	if (limit == dircountlimit) {
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
	if (*path == '/') {
		dir = strdup2("/");
		subdir = path + 1;
		if (!*subdir) subdir = NULL;
	}
	else if (_dospath(path)) {
		dir = (char *)malloc2(3 + 1);
		strncpy2(dir, path, 3);
		subdir = path + 3;
		if (!*subdir) subdir = NULL;
	}
	else {
		len = (cp = strchr(path, '/')) ? cp - path : strlen(path);
		dir = (char *)malloc2(len + 1);
		strncpy2(dir, path, len);
		subdir = (cp) ? cp + 1 : NULL;
	}

	if (!subdir) len = 0;
	else len = (cp = strchr(subdir, '/')) ? cp - subdir : strlen(subdir);

	*maxp = *maxentp = 0;
	i = _chdir2(dir);
	free(dir);
	if (i < 0 || !(dirp = Xopendir("."))) return(NULL);

	cp = NULL;
	while (dp = Xreaddir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")
		|| Xstat(dp -> d_name, &status) < 0
		|| (status.st_mode & S_IFMT) != S_IFDIR) continue;
		list = (treelist *)addlist(list, *maxp,
			maxentp, sizeof(treelist));
		if (!subdir) {
			list[*maxp].name = strdup2(dp -> d_name);
			list[*maxp].next = NULL;
			list[*maxp].max = list[*maxp].maxent =
				(includedir(dp -> d_name)) ? -1 : 0;
			(*maxp)++;
		}
		else if (!strncmp(dp -> d_name, subdir, len)
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

	if (*path == '/' || _dospath(path)) {
		if (_chdir2(fullpath) < 0) error(fullpath);
	}
	else if (_chdir2("..") < 0) error("..");
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

static VOID showtree(path, buf, list, max, nest, top, y)
char *path, *buf;
treelist *list;
int max, nest, top, y;
{
	int i;

	for (i = WHEADER; i <= n_line - WFOOTER; i++) {
		locate(0, i);
		putterm(l_clear);
	}
	for (i = WHEADER + 1; i < n_line - WFOOTER; i++)
		sprintf(bufptr(buf, i), "%-*.*s",
			TREEFIELD, TREEFIELD, " ");
	_showtree(buf, list, max, nest, top);
	for (i = WHEADER + 1; i < n_line - WFOOTER; i++) {
		locate(1, i);
		if (i == y) putterm(t_standout);
		kanjiputs(bufptr(buf, i));
		if (i == y) putterm(end_standout);
	}
	includefile(path);
	keyflush();
}

static treelist *_searchtree(path, list, max, ip, nest, yp, y)
char *path;
treelist *list;
int max, *ip, nest, *yp, y;
{
	treelist *lp, *tmplp;
	int i, j, w, len;

	lp = NULL;
	w = TREEFIELD - (nest * DIRFIELD) - 1;
	for (i = 0; i < max; i++) {
		if (*yp == y) {
			lp = (treelist *)-1;
			*ip = i;
			strcpy(path, list[i].name);
		}
		(*yp)++;
		if (w >= DIRFIELD && list[i].next) {
			tmplp = _searchtree(path, list[i].next, list[i].max,
				ip, nest + 1, yp, y);
			if (tmplp) {
				lp = (tmplp == (treelist *)-1) ? &list[i]
					: tmplp;
				len = strlen(list[i].name);
				if (*list[i].name != '/'
				&& !_dospath(list[i].name)) len++;
				for (j = strlen(path); j >= 0; j--)
					path[j + len] = path[j];
				if (*list[i].name != '/')
					memcpy(path, list[i].name, len - 1);
				path[len - 1] = '/';
			}
		}
	}
	return(lp);
}

static treelist *searchtree(path, list, max, ip, nest, top, bottomp, y)
char *path;
treelist *list;
int max, *ip, nest, top, *bottomp, y;
{
	treelist *lp;

	*bottomp = top;
	lp = _searchtree(path, list, max, ip, nest, bottomp, y);
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
	for (cp = path, i = 0; cp = strchr(cp, '/'); cp++, i++);
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
			if (!strcmp(lp -> name, lptmp[i].name)) break;
		if (i < list -> max) {
			free(lptmp[i].name);
			for (; i > 0; i--) {
				memcpy(&lptmp[i], &lptmp[i - 1],
					sizeof(treelist));
			}
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
		strcat(path, "/");
		cp++;
	}
	for (i = 0; i < list -> max; i++) {
		strcpy(cp, list -> next[i].name);
		if (!expandall(&(list -> next[i]), path)) return(0);
	}
	return(1);
}

static treelist *checkmisc(list, lp, ip, path, top, bottomp, y)
treelist *list, *lp;
int *ip;
char *path;
int top, *bottomp, y;
{
	char *cp;

	if (!lp || !(lp -> next)
	|| lp -> next[*ip].max >= 0 || lp -> next[*ip].maxent < 0) return(NULL);

	waitmes();
	if ((cp = strrchr(path, '/')) == strchr(path, '/')) cp++;
	*cp = '\0';
	if (!(expandtree(lp, path))) return((treelist *)-1);
	return(searchtree(path, list, 1, ip, 0, top, bottomp, y));
}

static treelist *treeup(yp, topp, bottomp, path, list, ip)
int *yp, *topp, *bottomp;
char *path;
treelist *list;
int *ip;
{
	treelist *lp;

	if (*yp > WHEADER + 1) (*yp)--;
	else if ((*topp) <= WHEADER) (*topp)++;
	else return(NULL);
	lp = searchtree(path, list, 1, ip, 0, *topp, bottomp, *yp);
	return(lp);
}

static treelist *treedown(yp, topp, bottomp, path, list, ip, redrawp)
int *yp, *topp, *bottomp;
char *path;
treelist *list;
int *ip, *redrawp;
{
	treelist *lp, *lptmp;
	int oy, otop;

	oy = *yp;
	otop = *topp;
	if (*yp < *bottomp - 1 && *yp < n_line - WFOOTER - 2) (*yp)++;
	else if (*bottomp >= n_line - WFOOTER) (*topp)--;
	else return(NULL);
	lp = searchtree(path, list, 1, ip, 0, *topp, bottomp, *yp);
	lptmp = checkmisc(list, lp, ip, path, *topp, bottomp, *yp);
	if (lptmp) {
		if ((lp = lptmp) != (treelist *)-1) *redrawp = 1;
		else {
			*yp = oy;
			*topp = otop;
			searchtree(path, list, 1, ip, 0,
				*topp, bottomp, *yp);
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

static char *_tree()
{
	treelist *list, *lp, *olp, *lptmp;
	char *cp, *cwd, path[MAXPATHLEN + 1];
	int ch, i, tmp, half, y, oy, top, otop, bottom, redraw;


	keyflush();
	waitmes();
	list = (treelist *)malloc2(sizeof(treelist));
	if (dospath("", path)) {
		list[0].name = (char *)malloc2(3 + 1);
		strncpy2(list[0].name, path, 3);
	}
	else {
		strcpy(path, fullpath);
		list[0].name = strdup2("/");
	}
	list[0].next = maketree(path, NULL, 0,
		&(list[0].max), &(list[0].maxent));

	y = 0;
	cwd = path;
	if (_dospath(cwd)) cwd += 2;
	if (strcmp(cwd, "/"))
		for (cp = cwd; cp = strchr(cp, '/'); cp++, y++)
			if ((y + 1) * DIRFIELD + 2 > TREEFIELD) break;
	y += (top = WHEADER + 1);
	if (y >= n_line - WFOOTER - 2) {
		top += n_line - WFOOTER - 2 - y;
		y = n_line - WFOOTER - 2;
	}

	cp = (char *)malloc2((n_line - WHEADER - WFOOTER - 1)
		* (TREEFIELD + 1));
	lp = searchtree(path, list, 1, &i, 0, top, &bottom, y);
	showtree(path, cp, list, 1, 0, top, y);
	do {
		locate(1, WHEADER);
		cputs("Tree=");
		kanjiputs2(path, n_column - 6, 0);
		locate(0, LMESLINE);
		putterm(l_clear);
		locate(0, 0);
		tflush();
		oy = y;
		otop = top;
		olp = lp;
		redraw = 0;
		switch (ch = getkey2(SIGALRM)) {
			case K_UP:
				lptmp = treeup(&y, &top, &bottom,
					path, list, &i);
				if (lptmp) lp = lptmp;
				break;
			case K_DOWN:
				lptmp = treedown(&y, &top, &bottom,
					path, list, &i, &redraw);
				if (lptmp) lp = lptmp;
				break;
			case K_PPAGE:
				half = (n_line - WHEADER - WFOOTER - 1) / 2;
				y = WHEADER + half + 1;
				if (top + half > WHEADER + 1)
					half = WHEADER - top + 1;
				if (half > 0) top += half;
				lp = searchtree(path, list, 1,
					&i, 0, top, &bottom, y);
				break;
			case K_NPAGE:
				half = (n_line - WHEADER - WFOOTER - 1) / 2;
				tmp = half + (WHEADER + half + 1) - y;
				while (tmp-- > 0) {
					lptmp = treedown(&y, &top, &bottom,
						path, list, &i, &redraw);
					if (lptmp) lp = lptmp;
					else break;
				}
				tmp = y - (WHEADER + half + 1);
				y = WHEADER + half + 1;
				if (bottom - tmp < n_line - WFOOTER - 1)
					tmp = bottom - n_line + WFOOTER + 1;
				top -= tmp;
				lp = searchtree(path, list, 1,
					&i, 0, top, &bottom, y);
				break;
			case K_BEG:
			case K_HOME:
			case '<':
				y = top = WHEADER + 1;
				lp = searchtree(path, list, 1,
					&i, 0, top, &bottom, y);
				break;
			case K_EOL:
			case K_END:
			case '>':
				for (;;) {
					lptmp = treedown(&y, &top, &bottom,
						path, list, &i, &redraw);
					if (lptmp) lp = lptmp;
					else break;
				}
				break;
			case '?':
				while (strcmp(fullpath, path)) {
					lptmp = treeup(&y, &top, &bottom,
						path, list, &i);
					if (lptmp) lp = lptmp;
					else break;
				}
				if (strcmp(fullpath, path)) {
					top = otop;
					y = oy;
					lp = searchtree(path, list, 1,
						&i, 0, top, &bottom, y);
					do {
						lptmp = treedown(&y, &top,
							&bottom, path,
							list, &i, &redraw);
						if (lptmp) lp = lptmp;
						else break;
					} while (strcmp(fullpath, path));
				}
				break;
			case '(':
				tmp = i - 1;
				do {
					lptmp = treeup(&y, &top, &bottom,
						path, list, &i);
					if (lptmp) lp = lptmp;
					else break;
				} while (tmp >= 0 && (lp != olp || i != tmp));
				break;
			case ')':
				tmp = i + 1;
				do {
					lptmp = treedown(&y, &top, &bottom,
						path, list, &i, &redraw);
					if (lptmp) lp = lptmp;
					else break;
				} while (tmp < olp -> max
				&& (lp != olp || i != tmp));
				break;
			case '\t':
				if (!lp || !lp -> next
				|| lp -> next[i].maxent >= 0) break;
				waitmes();
				expandall(&(lp -> next[i]), path);
				lp = searchtree(path, list, 1,
					&i, 0, top, &bottom, y);
				redraw = 1;
				break;
			case K_RIGHT:
				if (!lp || !lp -> next
				|| lp -> next[i].maxent >= 0) break;
				waitmes();
				expandtree(&(lp -> next[i]), path);
				lp = searchtree(path, list, 1,
					&i, 0, top, &bottom, y);
				redraw = 1;
				break;
			case K_LEFT:
			case K_BS:
				do {
					lptmp = treeup(&y, &top, &bottom,
						path, list, &i);
					if (lptmp) lp = lptmp;
					else break;
				} while (&(lp -> next[i]) != olp);
				break;
			case 'l':
				if (!(cwd = inputstr(LOGD_K, 0, -1, NULL, NULL))
				|| !*(cwd = evalpath(cwd))) break;
				if (chdir2(cwd) >= 0) {
					free(cwd);
					free(cp);
					freetree(list, 1);
					return(fullpath);
				}
				warning(-1, cwd);
				free(cwd);
				break;
			case CTRL('L'):
				cp = (char *)realloc2(cp, (n_line - WHEADER
					- WFOOTER - 1) * (TREEFIELD + 1));
				lp = searchtree(path, list, 1,
					&i, 0, top, &bottom, y);
				redraw = 1;
				break;
			case ESC:
				break;
			default:
				if (ch < 'A' || ch > 'Z') break;
				if (y == bottom - 1) {
					y = top = WHEADER + 1;
					lp = searchtree(path, list, 1,
						&i, 0, top, &bottom, y);
				}
				else do {
					lptmp = treedown(&y, &top, &bottom,
						path, list, &i, &redraw);
					if (lptmp) lp = lptmp;
					else break;
				} while (toupper2(*(lp -> next[i].name)) != ch);
				break;
		}
		if (redraw || top != otop)
			showtree(path, cp, list, 1, 0, top, y);
		else if (oy != y) {
			locate(1, y);
			putterm(t_standout);
			kanjiputs(bufptr(cp, y));
			putterm(end_standout);
			locate(1, oy);
			if (stable_standout) putterm(end_standout);
			else kanjiputs(bufptr(cp, oy));
			includefile(path);
		}
	} while (ch != ESC && ch != CR);

	free(cp);
	freetree(list, 1);
	if (ch == ESC) return(NULL);
	return(strdup2(path));
}

char *tree(cleanup, ddp)
int cleanup, *ddp;
{
	char *path, *treepath;

	subwindow = 1;
	getkey2(-1);
	treepath = strdup2(fullpath);
	do {
		path = _tree();
	} while (path == fullpath);
	if (ddp) {
		*ddp = lastdrv;
		lastdrv = -1;
		if (chdir2(treepath) < 0) error(treepath);
	}
	free(treepath);

	if (cleanup) {
		if (archivefile) rewritearc(1);
		rewritefile(1);
	}
	subwindow = 0;
	getkey2(-1);
	return(path);
}
