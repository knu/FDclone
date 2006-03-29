/*
 *	tree.c
 *
 *	tree mode module
 */

#include "fd.h"
#include "func.h"
#include "kanji.h"

#ifndef	_NOTREE

#if	MSDOS && !defined (_NODOSDRIVE)
extern int preparedrv __P_((int));
#endif


extern char fullpath[];
extern int win_x;
extern int win_y;
#ifndef	_NODOSDRIVE
extern int lastdrv;
#endif

#define	DIRFIELD	3
#define	TREEFIELD	(((dircountlimit > 0) \
			? (n_column * 3) / 5 : n_column) - 2)
#define	FILEFIELD	((dircountlimit > 0) ? (n_column * 2) / 5 - 3 : 0)
#define	bufptr(y)	(&(tr_scr[(y - 1) * (TREEFIELD + 1)]))

static int NEAR evaldir __P_((char *, int));
static treelist *NEAR maketree __P_((char *, treelist *, treelist *,
		int, int *));
static int NEAR _showtree __P_((treelist *, int, int, int, int));
static VOID NEAR showtree __P_((VOID_A));
static VOID NEAR treebar __P_((VOID_A));
static treelist *NEAR _searchtree __P_((treelist *, int, int));
static VOID NEAR searchtree __P_((VOID_A));
static int NEAR expandtree __P_((treelist *));
static int NEAR expandall __P_((treelist *));
static int NEAR treeup __P_((VOID_A));
static int NEAR treedown __P_((VOID_A));
static int NEAR freetree __P_((treelist *, int));
static VOID NEAR _tree_search __P_((VOID_A));
static int NEAR _tree_input __P_((VOID_A));
static char *NEAR _tree __P_((VOID_A));

int sorttree = 0;
int dircountlimit = 0;

static int redraw = 0;
static int tr_no = 0;
static int tr_line = 0;
static int tr_top = 0;
static int tr_bottom = 0;
static char *tr_scr = NULL;
static treelist *tr_root = NULL;
static treelist *tr_cur = NULL;


static int NEAR evaldir(dir, disp)
char *dir;
int disp;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	char path[MAXPATHLEN];
	int i, x, y, w, min, len, limit;
#if	MSDOS
	char *cp;
#endif

	if ((limit = dircountlimit) <= 0) return(1);

	min = filetop(win);
	if (disp) for (i = 1; i < FILEPERROW; i++) {
		Xlocate(TREEFIELD + 2, min + i);
		Xputch2('|');
		Xputterm(L_CLEAR);
	}
	strcpy(path, dir);
#if	MSDOS
	cp = path;
	if (_dospath(cp)) cp += 2;
	if (!isrootpath(cp)) strcatdelim(path);
#else
	if (!isrootpath(path)) strcatdelim(path);
#endif
	len = strlen(path);
	i = x = 0;
	y = 1;
	w = FILEFIELD / 2;
	if (!(dirp = Xopendir(dir))) return(0);
	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;
		strcpy(&(path[len]), dp -> d_name);
		if (limit-- <= 0 || (stat2(path, &st) >= 0 && s_isdir(&st))) {
			if (!disp) {
				i++;
				break;
			}
		}
		else if (disp) {
			Xlocate(x + TREEFIELD + 4, min + y);
			cputstr(w, dp -> d_name);
			i++;
			if (++y >= FILEPERROW) {
				y = 1;
				x += w + 1;
				if (x >= w * 2 + 2) break;
			}
		}
	}
	if (disp && !i) {
		Xlocate(x + TREEFIELD + 4, min + 1);
		cputstr(w, "[No Files]");
	}
	Xclosedir(dirp);

	return(i);
}

/*ARGSUSED*/
static treelist *NEAR maketree(path, list, parent, level, maxp)
char *path;
treelist *list, *parent;
int level, *maxp;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	char *cp, *dir, *subdir, cwd[MAXPATHLEN];
#ifdef	_USEDOSEMU
	char tmp[MAXPATHLEN];
#endif
	int i, len;

	if ((level + 1) * DIRFIELD + 2 > TREEFIELD) {
		*maxp = -2;
		return(NULL);
	}
	if (intrkey()) {
		*maxp = -1;
		return(NULL);
	}
	if (!Xgetwd(cwd)) {
		*maxp = 0;
		return(NULL);
	}

#if	MSDOS
	if (_dospath(path)) path += 2;
#endif
	if (*path == _SC_) {
		dir = strdup2(rootpath);
		subdir = path + 1;
		if (!*subdir) subdir = NULL;
	}
#ifdef	_USEDOSEMU
	else if (_dospath(path)) {
		dir = strndup2(path, 3);
		subdir = path + 3;
		if (!*subdir) subdir = NULL;
	}
#endif
	else {
		len = (cp = strdelim(path, 0)) ? cp - path : strlen(path);
		dir = strndup2(path, len);
		subdir = (cp) ? cp + 1 : NULL;
	}

	if (!subdir) len = 0;
	else len = (cp = strdelim(subdir, 0)) ? cp - subdir : strlen(subdir);

	*maxp = 0;
	i = _chdir2(dir);
	free(dir);
	if (i < 0 || !(dirp = Xopendir(curpath))) return(NULL);

	i = 0;
	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)
		|| Xstat(nodospath(tmp, dp -> d_name), &st) < 0
		|| !s_isdir(&st))
			continue;
		list = b_realloc(list, *maxp, treelist);
		if (!subdir) {
			list[*maxp].name = strdup2(dp -> d_name);
			list[*maxp].sub = NULL;
			list[*maxp].max = 0;
#ifndef	NODIRLOOP
			list[*maxp].dev = st.st_dev;
			list[*maxp].ino = st.st_ino;
			list[*maxp].parent = parent;
#endif
			(*maxp)++;
		}
		else if (!strnpathcmp(dp -> d_name, subdir, len)
		&& strlen(dp -> d_name) == len) {
			list[0].name = strdup2(dp -> d_name);
			list[0].sub = &(list[0]);
			list[0].max = 0;
#ifndef	NODIRLOOP
			list[0].dev = st.st_dev;
			list[0].ino = st.st_ino;
			list[0].parent = parent;
#endif
			if (++(*maxp) >= 2) break;
		}
		else {
			if (!(*maxp)) {
				list[0].name = NULL;
				list[0].sub = NULL;
				list[0].max = -1;
#ifndef	NODIRLOOP
				list[0].dev = 0;
				list[0].ino = 0;
				list[0].parent = NULL;
#endif
			}
			if (!i) {
				i++;
				list[1].name = NULL;
				list[1].sub = NULL;
				list[1].max = -1;
#ifndef	NODIRLOOP
				list[1].dev = 0;
				list[1].ino = 0;
				list[1].parent = NULL;
#endif
				if (++(*maxp) >= 2) break;
			}
		}
	}
	Xclosedir(dirp);
	if (list) for (i = 0; i < *maxp; i++) {
#ifndef	NODIRLOOP
		treelist *lp;

		if (!(list[i].name)) lp = NULL;
		else for (lp = parent; lp; lp = lp -> parent)
			if (lp -> dev == list[i].dev
			&& lp -> ino == list[i].ino)
				break;
		if (lp) {
			list[i].sub = NULL;
			list[i].max = 0;
		}
		else
#endif
		if (list[i].sub)
			list[i].sub = maketree(subdir, NULL, &(list[i]),
				level + 1, &(list[i].max));
		else {
			if (list[i].max >= 0 && list[i].name
			&& evaldir(nodospath(tmp, list[i].name), 0))
				list[i].max = -1;
		}
	}
	if (sorttree && sorton) qsort(list, *maxp, sizeof(treelist), cmptree);

#if	MSDOS
	if (_dospath(path)) path += 2;
#endif
#if	MSDOS || defined (_NODOSDRIVE)
	if (*path == _SC_)
#else
	if (*path == _SC_ || _dospath(path))
#endif
	{
		if (_chdir2(fullpath) < 0) error(fullpath);
	}
	else if (isdotdir(path) != 2 && _chdir2(cwd) < 0) error(parentpath);

	return(list);
}

static int NEAR _showtree(list, max, nest, min, y)
treelist *list;
int max, nest, min, y;
{
	char *cp;
	int i, j, w, tmp;

	w = TREEFIELD - (nest * DIRFIELD) - 1;
	tmp = y;
	for (i = 0; i < max; i++) {
		if (nest > 0) {
			for (j = tmp; j < y; j++) {
				if (j <= 0 || j >= FILEPERROW) continue;
				*(bufptr(j) + (nest - 1) * DIRFIELD) = '|';
			}
		}
		if (y > 0 && y < FILEPERROW) {
			cp = bufptr(y);
			if (nest > 0)
				memcpy(&(cp[(nest - 1) * DIRFIELD]), "+--", 3);
			if (!(list[i].name)) {
				tmp = (w > 3) ? 3 : w;
				memcpy(&(cp[nest * DIRFIELD]), "...", tmp);
			}
			else {
				tmp = strlen(list[i].name);
				if (tmp > w) tmp = w;
				memcpy(&(cp[nest * DIRFIELD]),
					list[i].name, tmp);
				if (list[i].max < 0)
					cp[nest * DIRFIELD + tmp] = '>';
			}
		}
		y++;
		tmp = y;
		if (w >= DIRFIELD && list[i].sub)
			y = _showtree(list[i].sub, list[i].max,
				nest + 1, min, y);
	}

	return(y);
}

static VOID NEAR showtree(VOID_A)
{
	int i, min;

	min = filetop(win);
	for (i = 0; i < FILEPERROW; i++) {
		Xlocate(0, min + i);
		Xputterm(L_CLEAR);
	}
	for (i = 1; i < FILEPERROW; i++) {
		memset(bufptr(i), ' ', TREEFIELD);
		bufptr(i)[TREEFIELD] = '\0';
	}
	_showtree(tr_root -> sub, 1, 0, min, tr_top - min);
	for (i = 1; i < FILEPERROW; i++) {
		Xlocate(1, min + i);
		if (min + i == tr_line) Xputterm(T_STANDOUT);
		cputstr(TREEFIELD, bufptr(i));
		if (min + i == tr_line) Xputterm(END_STANDOUT);
	}
	evaldir(treepath, 1);
	keyflush();
}

static VOID NEAR treebar(VOID_A)
{
	Xlocate(1, filetop(win));
	Xcprintf2("Tree=%-*.*k", n_column - 6, n_column - 6, treepath);
	Xlocate(0, L_MESLINE);
	Xputterm(L_CLEAR);
}

VOID rewritetree(VOID_A)
{
	tr_scr = realloc2(tr_scr, (FILEPERROW - 1) * (TREEFIELD + 1));
	searchtree();
	showtree();
	treebar();
	Xtflush();
}

static treelist *NEAR _searchtree(list, max, nest)
treelist *list;
int max, nest;
{
	treelist *lp, *tmplp;
	int i, w, len;

	lp = NULL;
	w = TREEFIELD - (nest * DIRFIELD) - 1;
	for (i = 0; i < max; i++) {
		if (tr_bottom == tr_line) {
			lp = tr_root;
			tr_no = i;
			if (list[i].name) strcpy(treepath, list[i].name);
			else *treepath = '\0';
		}
		tr_bottom++;
		if (w >= DIRFIELD && list[i].sub) {
			tmplp = _searchtree(list[i].sub, list[i].max,
				nest + 1);
			if (tmplp) {
				lp = (tmplp == tr_root) ? &(list[i]) : tmplp;
				len = strlen(list[i].name);
#if	MSDOS
				if (!_dospath(list[i].name)
				|| list[i].name[2] != _SC_)
					len++;
				memmove(&(treepath[len]), treepath,
					strlen(treepath) + 1);
				memcpy(treepath, list[i].name, len - 1);
#else	/* !MSDOS */
# ifdef	_NODOSDRIVE
				if (*list[i].name != _SC_) len++;
# else
				if (*list[i].name != _SC_
				&& !_dospath(list[i].name))
					len++;
# endif
				memmove(&(treepath[len]), treepath,
					strlen(treepath) + 1);
				if (*list[i].name != _SC_)
					memcpy(treepath, list[i].name,
						len - 1);
#endif	/* !MSDOS */
				treepath[len - 1] = _SC_;
			}
		}
	}

	return(lp);
}

static VOID NEAR searchtree(VOID_A)
{
	tr_bottom = tr_top;
	tr_cur = _searchtree(tr_root -> sub, 1, 0);
}

static int NEAR expandtree(list)
treelist *list;
{
	treelist *lp, *lptmp;
	char *cp;
	int i;

	lp = NULL;
	if (list -> sub) {
		lp = (treelist *)malloc2(sizeof(treelist));
		memcpy((char *)lp, (char *)&(list -> sub[0]),
			sizeof(treelist));
		for (i = 1; i < list -> max; i++)
			if ((list -> sub[i]).name) {
				free((list -> sub[i]).name);
				(list -> sub[i]).name = NULL;
			}
	}

	if (_chdir2(treepath) < 0) {
		if (list -> sub) error(treepath);
		list -> max = 0;
		return(1);
	}
	for (cp = treepath, i = 0; (cp = strdelim(cp, 0)); cp++, i++);
	lptmp = maketree(curpath, list -> sub, list, i, &(list -> max));
	if (_chdir2(fullpath) < 0) lostcwd(fullpath);
	if (list -> max < 0) {
		i = (list -> max < -1) ? 1 : 0;
		if (!(list -> sub)) list -> max = -1;
		else {
			list -> max = 2;
			(list -> sub[1]).name = NULL;
			free(lp);
		}
		return(i);
	}
	if (list -> sub) {
		if (!(lp -> name)) i = list -> max;
		else for (i = 0; i < list -> max; i++)
			if (lptmp[i].name
			&& !strpathcmp(lp -> name, lptmp[i].name))
				break;
		if (i < list -> max) {
			free(lptmp[i].name);
			for (; i > 0; i--) memcpy((char *)&(lptmp[i]),
					(char *)&(lptmp[i - 1]),
					sizeof(treelist));
			memcpy((char *)&(lptmp[0]), (char *)lp,
				sizeof(treelist));
		}
		free(lp);
	}
	list -> sub = lptmp;

	return(1);
}

static int NEAR expandall(list)
treelist *list;
{
	char *cp;
	int i;

	if (!list || list -> max >= 0) return(1);
	if (!expandtree(list)) return(0);
	if (!(list -> sub)) return(1);
	cp = treepath + strlen(treepath);
	if (cp > treepath + 1) {
		strcatdelim(treepath);
		cp++;
	}
	for (i = 0; i < list -> max; i++) {
		strcpy(cp, (list -> sub[i]).name);
		if (!expandall(&(list -> sub[i]))) return(0);
	}

	return(1);
}

static int NEAR treeup(VOID_A)
{
	int min;

	min = filetop(win);
	if (tr_line > min + 1) tr_line--;
	else if (tr_top <= min) tr_top++;
	else return(-1);
	searchtree();

	return(0);
}

static int NEAR treedown(VOID_A)
{
	char *cp;
	int min, oy, otop;

	min = filetop(win);
	oy = tr_line;
	otop = tr_top;
	if (tr_line < tr_bottom - 1 && tr_line < min + FILEPERROW - 2)
		tr_line++;
	else if (tr_bottom >= min + FILEPERROW) tr_top--;
	else return(-1);
	searchtree();

	if (!tr_cur || !(tr_cur -> sub)
	|| (tr_cur -> sub[tr_no]).max >= 0
	|| ((tr_cur -> sub[tr_no]).name))
		return(0);

	waitmes();
	if ((cp = strrdelim(treepath, 0)) == strdelim(treepath, 0)) cp++;
	*cp = '\0';
	if (!expandtree(tr_cur)) {
		tr_line = oy;
		tr_top = otop;
		searchtree();
		return(-1);
	}
	searchtree();
	redraw = 1;

	return(0);
}

static int NEAR freetree(list, max)
treelist *list;
int max;
{
	int i, n;

	for (i = n = 0; i < max; i++) {
		n++;
		if (list[i].name) free(list[i].name);
		if (list[i].sub) n += freetree(list[i].sub, list[i].max);
	}
	free(list);

	return(n);
}

static VOID NEAR _tree_search(VOID_A)
{
	int oy, otop;

	oy = tr_line;
	otop = tr_top;

	while (strpathcmp(fullpath, treepath)) if (treeup() < 0) {
		tr_top = otop;
		tr_line = oy;
		searchtree();
		do {
			if (treedown() < 0) break;
		} while (strpathcmp(fullpath, treepath));
		break;
	}
}

static int NEAR _tree_input(VOID_A)
{
	treelist *old;
	char *cwd;
	int ch, min, tmp, half;

	keyflush();
	Xgetkey(-1, 0);
	ch = Xgetkey(1, 0);
	Xgetkey(-1, 0);

	old = tr_cur;
	min = filetop(win);
	switch (ch) {
		case K_UP:
			treeup();
			break;
		case K_DOWN:
			treedown();
			break;
		case K_PPAGE:
			half = (FILEPERROW - 1) / 2;
			tmp = min + half + 1 - tr_line;
			if (tr_top + half > min + 1) half = min - tr_top + 1;
			if (half > 0) tr_top += half;
			for (;tmp > 0; tmp--) if (treedown() < 0) break;
			tr_line += tmp;
			searchtree();
			break;
		case K_NPAGE:
			half = (FILEPERROW - 1) / 2;
			tmp = half + (min + half + 1) - tr_line;
			while (tmp-- > 0) if (treedown() < 0) break;
			tmp = tr_line - (min + half + 1);
			tr_line = min + half + 1;
			if (tr_bottom - tmp < min + FILEPERROW - 1)
				tmp = tr_bottom - min + FILEPERROW + 1;
			tr_top -= tmp;
			searchtree();
			break;
		case K_BEG:
		case K_HOME:
		case '<':
			tr_line = tr_top = min + 1;
			searchtree();
			break;
		case K_EOL:
		case K_END:
		case '>':
			while (treedown() >= 0);
			break;
		case '?':
			_tree_search();
			break;
		case '(':
			tmp = tr_no - 1;
			do {
				if (treeup() < 0) break;
			} while (tmp >= 0 && (tr_cur != old || tr_no != tmp));
			break;
		case ')':
			tmp = tr_no + 1;
			do {
				if (treedown() < 0) break;
			} while (tmp < old -> max
			&& (tr_cur != old || tr_no != tmp));
			break;
		case '\t':
			if (!tr_cur || !(tr_cur -> sub)
			|| (tr_cur -> sub[tr_no]).max >= 0)
				break;
			waitmes();
			expandall(&(tr_cur -> sub[tr_no]));
			searchtree();
			redraw = 1;
			break;
		case K_RIGHT:
			if (!tr_cur || !(tr_cur -> sub)) break;
			if ((tr_cur -> sub[tr_no]).max >= 0) {
				if (!((tr_cur -> sub[tr_no]).sub)) break;
				if (treedown() < 0) break;
			}
			waitmes();
			expandtree(&(tr_cur -> sub[tr_no]));
			searchtree();
			redraw = 1;
			break;
		case K_LEFT:
			if (tr_cur && (tr_cur -> sub)
			&& (tr_cur -> sub[tr_no]).sub) {
				tmp = freetree((tr_cur -> sub[tr_no]).sub,
					(tr_cur -> sub[tr_no]).max);
				tr_bottom -= tmp;
				(tr_cur -> sub[tr_no]).max = -1;
				(tr_cur -> sub[tr_no]).sub = NULL;
				redraw = 1;
				break;
			}
/*FALLTHRU*/
		case K_BS:
			do {
				if (treeup() < 0) break;
			} while (&(tr_cur -> sub[tr_no]) != old);
			break;
		case 'l':
			if (!(cwd = inputstr(LOGD_K, 0, -1, NULL, HST_PATH))
			|| !*(cwd = evalpath(cwd, 0)))
				break;
			if (chdir2(cwd) >= 0) {
				free(cwd);
				break;
			}
			warning(-1, cwd);
			ch = '\0';
			free(cwd);
			break;
		case K_CTRL('L'):
			rewritefile(1);
			break;
		case K_ESC:
			break;
		default:
			if (!isupper2(ch)) break;
			if (tr_line == tr_bottom - 1) {
				tr_line = tr_top = min + 1;
				searchtree();
			}
			else do {
				if (treedown() < 0) break;
				tmp = toupper2(*((tr_cur -> sub[tr_no]).name));
			} while (ch != tmp);
			break;
	}

	return(ch);
}

static char *NEAR _tree(VOID_A)
{
#ifndef	NODIRLOOP
	struct stat st;
# ifndef	_NODOSDRIVE
	char tmp[MAXPATHLEN];
# endif
#endif
	char *cp, *cwd, path[MAXPATHLEN];
	int ch, min, oy, otop;

	keyflush();
	waitmes();
	tr_root = (treelist *)malloc2(sizeof(treelist));
	tr_root -> name = NULL;
	tr_root -> max = 1;
	tr_root -> sub = tr_cur = (treelist *)malloc2(sizeof(treelist));
#ifndef	NODIRLOOP
	tr_root -> ino = 0;
	tr_root -> parent = NULL;
#endif
	treepath = path;
#if	MSDOS
	tr_cur[0].name = malloc2(3 + 1);
	strcpy(path, fullpath);
	strncpy2(tr_cur[0].name, path, 3);
#else	/* !MSDOS */
# ifdef	_NODOSDRIVE
	strcpy(path, fullpath);
	tr_cur[0].name = strdup2(rootpath);
# else
	if (dospath(nullstr, path)) tr_cur[0].name = strndup2(path, 3);
	else {
		strcpy(path, fullpath);
		tr_cur[0].name = strdup2(rootpath);
	}
# endif
#endif	/* !MSDOS */

#ifndef	NODIRLOOP
	if (Xstat(nodospath(tmp, tr_cur[0].name), &st) < 0)
		tr_cur[0].dev = tr_cur[0].ino = 0;
	else {
		tr_cur[0].dev = st.st_dev;
		tr_cur[0].ino = st.st_ino;
	}
	tr_cur[0].parent = NULL;
#endif
	tr_cur[0].sub = maketree(path, NULL, &(tr_cur[0]),
		0, &(tr_cur[0].max));

	tr_line = 0;
#if	MSDOS
	cwd = path + 2;
#else	/* !MSDOS */
	cwd = path;
# ifndef	_NODOSDRIVE
	if (_dospath(cwd)) cwd += 2;
# endif
#endif	/* !MSDOS */
	if (isrootpath(cwd)) /*EMPTY*/;
	else for (cp = cwd; (cp = strdelim(cp, 0)); cp++, tr_line++)
		if ((tr_line + 1) * DIRFIELD + 2 > TREEFIELD
		|| !(tr_cur = &(tr_cur -> sub[0])))
			break;
	min = filetop(win);
	tr_line += (tr_top = min + 1);
	if (tr_line >= min + FILEPERROW - 2) {
		tr_top -= tr_line - (min + FILEPERROW - 2);
		tr_line = min + FILEPERROW - 2;
	}

	tr_scr = malloc2((FILEPERROW - 1) * (TREEFIELD + 1));
	searchtree();
	showtree();
	win_x = 0;
	do {
		treebar();
		oy = tr_line;
		otop = tr_top;
		redraw = 0;
		win_y = tr_line;
		Xlocate(win_x, win_y);
		Xtflush();

		if ((ch = _tree_input()) == 'l') {
			free(tr_scr);
			freetree(tr_root, 1);
			return(fullpath);
		}

		if (redraw || tr_top != otop) showtree();
		else if (oy != tr_line) {
			Xlocate(1, tr_line);
			Xputterm(T_STANDOUT);
			cputstr(TREEFIELD, bufptr(tr_line - min));
			Xputterm(END_STANDOUT);
			Xlocate(1, oy);
			if (stable_standout) Xputterm(END_STANDOUT);
			else cputstr(TREEFIELD, bufptr(oy - min));
			evaldir(path, 1);
		}
	} while (ch != K_ESC && ch != K_CR);

	free(tr_scr);
	freetree(tr_root, 1);
	treepath = NULL;
	if (ch == K_ESC) return(NULL);

	return(strdup2(path));
}

/*ARGSUSED*/
char *tree(cleanup, ddp)
int cleanup, *ddp;
{
	char *path, *dupfullpath;

	if (FILEPERROW < WFILEMINTREE) {
		warning(0, NOROW_K);
		return(NULL);
	}
	dupfullpath = strdup2(fullpath);
	do {
		path = _tree();
	} while (path == fullpath);
	if (ddp) {
#ifndef	_NODOSDRIVE
# if	MSDOS
		if (lastdrv < 0) *ddp = preparedrv(dospath(path, NULL));
		else
# endif
		*ddp = lastdrv;
		lastdrv = -1;
#endif
	}
	if (chdir2(dupfullpath) < 0) lostcwd(NULL);
	free(dupfullpath);

	if (cleanup) rewritefile(1);

	return(path);
}
#endif	/* !_NOTREE */
