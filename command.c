/*
 *	command.c
 *
 *	Command Execute Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#if	!MSDOS
#include <sys/file.h>
#endif

#define	getblock(c)	((((c) + blocksize - 1) / blocksize) * blocksize)

extern int columns;
extern int filepos;
extern int mark;
extern long marksize;
extern int isearch;
extern long blocksize;
extern int fnameofs;
extern int dispmode;
extern int sorton;
extern int chgorder;
extern int stackdepth;
extern int copypolicy;
extern namelist filestack[];
#ifndef	_NOARCHIVE
extern char *archivefile;
extern char *archivedir;
#endif
extern char *destpath;
extern int savehist;
extern int sizeinfo;

static VOID replacefname __P_((namelist *, int *, char *));
static int cur_up __P_((namelist *, int *, char *));
static int cur_down __P_((namelist *, int *, char *));
static int cur_right __P_((namelist *, int *, char *));
static int cur_left __P_((namelist *, int *, char *));
static int roll_up __P_((namelist *, int *, char *));
static int roll_down __P_((namelist *, int *, char *));
static int cur_top __P_((namelist *, int *, char *));
static int cur_bottom __P_((namelist *, int *, char *));
static int in_dir __P_((namelist *, int *, char *));
static int out_dir __P_((namelist *, int *, char *));
static int one_column __P_((namelist *, int *, char *));
static int two_columns __P_((namelist *, int *, char *));
static int three_columns __P_((namelist *, int *, char *));
static int five_columns __P_((namelist *, int *, char *));
static int fname_right __P_((namelist *, int *, char *));
static int fname_left __P_((namelist *, int *, char *));
static VOID markcount __P_((VOID_A));
static int mark_file __P_((namelist *, int *, char *));
static int mark_file2 __P_((namelist *, int *, char *));
static int mark_file3 __P_((namelist *, int *, char *));
static int mark_all __P_((namelist *, int *, char *));
static int mark_reverse __P_((namelist *, int *, char *));
static reg_t *prepareregexp __P_((char *, char *));
static int mark_find __P_((namelist *, int *, char *));
static int push_file __P_((namelist *, int *, char *));
static int pop_file __P_((namelist *, int *, char *));
static int symlink_mode __P_((namelist *, int *, char *));
static int filetype_mode __P_((namelist *, int *, char *));
static int dotfile_mode __P_((namelist *, int *, char *));
static int log_dir __P_((namelist *, int *, char *));
static int log_top __P_((namelist *, int *, char *));
#ifndef	PAGER
static VOID dump __P_((char *));
#endif
static int view_file __P_((namelist *, int *, char *));
static int edit_file __P_((namelist *, int *, char *));
static int sort_dir __P_((namelist *, int *, char *));
#ifndef	_NOWRITEFS
static int write_dir __P_((namelist *, int *, char *));
#endif
static int reread_dir __P_((namelist *, int *, char *));
static int help_message __P_((namelist *, int *, char *));
static int quit_system __P_((namelist *, int *, char *));
static int make_dir __P_((namelist *, int *, char *));
static int copy_file __P_((namelist *, int *, char *));
#ifndef	_NOTREE
static int copy_tree __P_((namelist *, int *, char *));
#endif
static int move_file __P_((namelist *, int *, char *));
#ifndef	_NOTREE
static int move_tree __P_((namelist *, int *, char *));
#endif
static int rename_file __P_((namelist *, int *, char *));
static int delete_file __P_((namelist *, int *, char *));
static int delete_dir __P_((namelist *, int *, char *));
static int find_file __P_((namelist *, int *, char *));
static int find_dir __P_((namelist *, int *, char *));
static int execute_sh __P_((namelist *, int *, char *));
static int execute_file __P_((namelist *, int *, char *));
#ifndef	_NOARCHIVE
static int launch_file __P_((namelist *, int *, char *));
static int pack_file __P_((namelist *, int *, char *));
static int unpack_file __P_((namelist *, int *, char *));
#ifndef	_NOTREE
static int unpack_tree __P_((namelist *, int *, char *));
#endif
#endif	/* !_NOARCHIVE */
static int info_filesys __P_((namelist *, int *, char *));
static int attr_file __P_((namelist *, int *, char *));
#ifndef	_NOTREE
static int tree_dir __P_((namelist *, int *, char *));
#endif
#ifndef	_NOARCHIVE
static int backup_tape __P_((namelist *, int *, char *));
#endif
static int search_forw __P_((namelist *, int *, char *));
static int search_back __P_((namelist *, int *, char *));
static int warning_bell __P_((namelist *, int *, char *));
static int no_operation __P_((namelist *, int *, char *));

#include "functabl.h"

char *findpattern = NULL;
reg_t *findregexp = NULL;
#ifndef	_NOWRITEFS
int writefs = 0;
#endif
bindtable bindlist[MAXBINDTABLE] = {
	{K_UP,		CUR_UP,		255},
	{K_DOWN,	CUR_DOWN,	255},
	{K_RIGHT,	CUR_RIGHT,	255},
	{K_LEFT,	CUR_LEFT,	255},
	{K_NPAGE,	ROLL_UP,	255},
	{K_PPAGE,	ROLL_DOWN,	255},
#ifdef	_NOTREE
	{K_F(1),	HELP_MESSAGE,	255},
#else
	{K_F(1),	LOG_DIR,	255},
#endif
	{K_F(2),	EXECUTE_FILE,	255},
	{K_F(3),	COPY_FILE,	255},
	{K_F(4),	DELETE_FILE,	255},
	{K_F(5),	RENAME_FILE,	255},
	{K_F(6),	SORT_DIR,	255},
	{K_F(7),	FIND_FILE,	255},
#ifdef	_NOTREE
	{K_F(8),	LOG_DIR,	255},
#else
	{K_F(8),	TREE_DIR,	255},
#endif
	{K_F(9),	EDIT_FILE,	255},
#ifndef	_NOARCHIVE
	{K_F(10),	UNPACK_FILE,	255},
#endif
	{K_F(11),	ATTR_FILE,	255},
	{K_F(12),	INFO_FILESYS,	255},
	{K_F(13),	MOVE_FILE,	255},
	{K_F(14),	DELETE_DIR,	255},
	{K_F(15),	MAKE_DIR,	255},
	{K_F(16),	EXECUTE_SH,	255},
#ifndef	_NOWRITEFS
	{K_F(17),	WRITE_DIR,	255},
#endif
#ifndef	_NOARCHIVE
	{K_F(18),	BACKUP_TAPE,	255},
#endif
	{K_F(19),	VIEW_FILE,	255},
#ifndef	_NOARCHIVE
	{K_F(20),	PACK_FILE,	255},
	{CR,		LAUNCH_FILE,	IN_DIR},
#else
	{CR,		VIEW_FILE,	IN_DIR},
#endif
	{K_BS,		OUT_DIR,	255},
	{K_DC,		PUSH_FILE,	255},
	{K_IC,		POP_FILE,	255},
	{'\t',		MARK_FILE,	255},
	{ESC,		QUIT_SYSTEM,	255},

	{'<',		CUR_TOP,	255},
	{'>',		CUR_BOTTOM,	255},
	{'1',		ONE_COLUMN,	255},
	{'2',		TWO_COLUMNS,	255},
	{'3',		THREE_COLUMNS,	255},
	{'5',		FIVE_COLUMNS,	255},
	{'(',		FNAME_RIGHT,	255},
	{')',		FNAME_LEFT,	255},
	{' ',		MARK_FILE2,	255},
	{'+',		MARK_ALL,	255},
	{'-',		MARK_REVERSE,	255},
	{'*',		MARK_FIND,	255},
	{']',		PUSH_FILE,	255},
	{'[',		POP_FILE,	255},
	{'?',		HELP_MESSAGE,	255},

	{'a',		ATTR_FILE,	255},
#ifndef	_NOARCHIVE
	{'b',		BACKUP_TAPE,	255},
#endif
	{'c',		COPY_FILE,	255},
	{'d',		DELETE_FILE,	255},
	{'e',		EDIT_FILE,	255},
	{'f',		FIND_FILE,	255},
	{'h',		EXECUTE_SH,	255},
	{'i',		INFO_FILESYS,	255},
	{'k',		MAKE_DIR,	255},
	{'l',		LOG_DIR,	255},
	{'\\',		LOG_TOP,	255},
	{'m',		MOVE_FILE,	255},
#ifndef	_NOARCHIVE
	{'p',		PACK_FILE,	255},
#endif
	{'q',		QUIT_SYSTEM,	255},
	{'r',		RENAME_FILE,	255},
	{'s',		SORT_DIR,	255},
#ifndef	_NOTREE
	{'t',		TREE_DIR,	255},
#endif
#ifndef	_NOARCHIVE
	{'u',		UNPACK_FILE,	255},
#endif
	{'v',		VIEW_FILE,	255},
#ifndef	_NOWRITEFS
	{'w',		WRITE_DIR,	255},
#endif
	{'x',		EXECUTE_FILE,	255},
#ifndef	_NOTREE
	{'C',		COPY_TREE,	255},
#endif
	{'D',		DELETE_DIR,	255},
	{'F',		FIND_DIR,	255},
	{'H',		DOTFILE_MODE,	255},
#ifndef	_NOTREE
	{'L',		LOG_TREE,	255},
	{'M',		MOVE_TREE,	255},
#endif
	{'Q',		QUIT_SYSTEM,	255},
	{'S',		SYMLINK_MODE,	255},
	{'T',		FILETYPE_MODE,	255},
#if	!defined (_NOTREE) && !defined (_NOARCHIVE)
	{'U',		UNPACK_TREE,	255},
#endif
	{K_HOME,	MARK_ALL,	255},
	{K_END,		MARK_REVERSE,	255},
	{K_BEG,		CUR_TOP,	255},
	{K_EOL,		CUR_BOTTOM,	255},
	{K_HELP,	HELP_MESSAGE,	255},
	{CTRL('@'),	MARK_FILE3,	255},
	{CTRL('L'),	REREAD_DIR,	255},
	{CTRL('R'),	SEARCH_BACK,	255},
	{CTRL('S'),	SEARCH_FORW,	255},
	{-1,		NO_OPERATION,	255}
};


static VOID replacefname(list, maxp, name)
namelist *list;
int *maxp;
char *name;
{
	if (filepos < *maxp) free(list[filepos].name);
	else for (*maxp = 0; *maxp < filepos; (*maxp)++)
		if (!list[*maxp].name) list[*maxp].name = strdup2("");
	list[filepos].name = (name) ? name : strdup2("..");
}

/*ARGSUSED*/
static int cur_up(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int n;

	if (filepos <= 0) return(0);
	if (!arg || (n = atoi(arg)) <= 0) n = 1;
	if ((filepos -= n) < 0) filepos = 0;
	return(2);
}

/*ARGSUSED*/
static int cur_down(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int n;

	if (filepos >= *maxp - 1) return(0);
	if (!arg || (n = atoi(arg)) <= 0) n = 1;
	if ((filepos += n) > *maxp - 1) filepos = *maxp - 1;
	return(2);
}

/*ARGSUSED*/
static int cur_right(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int n;

	if (!arg || (n = atoi(arg)) <= 0) n = 1;
	if (filepos + FILEPERLOW * n < *maxp) filepos += FILEPERLOW * n;
	else if (filepos / FILEPERPAGE == (*maxp - 1) / FILEPERPAGE)
		return(0);
	else {
		filepos = ((*maxp - 1) / FILEPERLOW) * FILEPERLOW
			+ filepos % FILEPERLOW;
		if (filepos >= *maxp) {
			filepos -= FILEPERLOW;
			if (filepos / FILEPERPAGE < (*maxp - 1) / FILEPERPAGE)
				filepos = *maxp - 1;
		}
	}
	return(2);
}

/*ARGSUSED*/
static int cur_left(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int n;

	if (filepos < FILEPERLOW) return(0);
	if (!arg || (n = atoi(arg)) <= 0) n = 1;
	if (filepos - FILEPERLOW * n >= 0) filepos -= FILEPERLOW * n;
	else filepos = filepos % FILEPERLOW;
	return(2);
}

/*ARGSUSED*/
static int roll_up(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	i = (filepos / FILEPERPAGE) * FILEPERPAGE;
	if (i + FILEPERPAGE >= *maxp) return(warning_bell(list, maxp, arg));
	filepos = i + FILEPERPAGE;
	return(2);
}

/*ARGSUSED*/
static int roll_down(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	i = (filepos / FILEPERPAGE) * FILEPERPAGE;
	if (i - FILEPERPAGE < 0) return(warning_bell(list, maxp, arg));
	filepos = i - FILEPERPAGE;
	return(2);
}

/*ARGSUSED*/
static int cur_top(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	if (filepos == 0) return(0);
	filepos = 0;
	return(2);
}

/*ARGSUSED*/
static int cur_bottom(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	if (filepos == *maxp - 1) return(0);
	filepos = *maxp - 1;
	return(2);
}

/*ARGSUSED*/
static int in_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	if (
#ifndef	_NOARCHIVE
	!archivefile &&
#endif
	!strcmp(list[filepos].name, "."))
		return(warning_bell(list, maxp, arg));
	return(5);
}

/*ARGSUSED*/
static int out_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
#ifndef	_NOARCHIVE
	if (archivefile) filepos = -1;
	else
#endif
	replacefname(list, maxp, NULL);
	return(5);
}

/*ARGSUSED*/
static int one_column(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	columns = 1;
	return(2);
}

/*ARGSUSED*/
static int two_columns(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	columns = 2;
	return(2);
}

/*ARGSUSED*/
static int three_columns(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	columns = 3;
	return(2);
}

/*ARGSUSED*/
static int five_columns(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	columns = 5;
	return(2);
}

/*ARGSUSED*/
static int fname_right(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	if (fnameofs <= 0) return(0);
	fnameofs--;
	return(2);
}

/*ARGSUSED*/
static int fname_left(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	if (islink(&list[filepos])) i = 0;
	else {
		i = calcwidth();
		if (isdisptyp(dispmode)
		&& ((list[filepos].st_mode & S_IFMT) == S_IFDIR
		|| (list[filepos].st_mode & S_IFMT) == S_IFLNK
		|| (list[filepos].st_mode & S_IFMT) == S_IFSOCK
		|| (list[filepos].st_mode & S_IFMT) == S_IFIFO
		|| (list[filepos].st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))))
			i--;
	}
	if (i >= strlen2(list[filepos].name + fnameofs)) return(0);
	fnameofs++;
	return(2);
}

static VOID markcount(VOID_A)
{
	char buf[16];

	locate(CMARK + 5, LSTATUS);
	cprintf2("%4d", mark);
	if (sizeinfo) {
		locate(CSIZE + 5, LSIZE);
		cprintf2("%14.14s", inscomma(buf, marksize, 3));
	}
}

/*ARGSUSED*/
static int mark_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	if (isdir(&list[filepos])) return(0);
	list[filepos].flags ^= F_ISMRK;
	if (ismark(&list[filepos])) {
		mark++;
		marksize += getblock(list[filepos].st_size);
	}
	else {
		mark--;
		marksize -= getblock(list[filepos].st_size);
	}
	markcount();
	return(2);
}

/*ARGSUSED*/
static int mark_file2(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	mark_file(list, maxp, arg);
	if (filepos < *maxp - 1) filepos++;
	return(2);
}

/*ARGSUSED*/
static int mark_file3(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	mark_file(list, maxp, arg);
	if (filepos < *maxp - 1
	&& filepos / FILEPERPAGE == (filepos + 1) / FILEPERPAGE) filepos++;
	else filepos = (filepos / FILEPERPAGE) * FILEPERPAGE;
	return(2);
}

/*ARGSUSED*/
static int mark_all(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	if ((arg && atoi(arg) == 0) || (!arg && mark)) {
		for (i = 0; i < *maxp; i++) list[i].flags &= ~F_ISMRK;
		mark = 0;
		marksize = 0;
	}
	else {
		mark = 0;
		marksize = 0;
		for (i = 0; i < *maxp; i++) if (!isdir(&list[i])) {
			list[i].flags |= F_ISMRK;
			mark++;
			marksize += getblock(list[i].st_size);
		}
	}
	markcount();
	return(2);
}

/*ARGSUSED*/
static int mark_reverse(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	mark = 0;
	marksize = 0;
	for (i = 0; i < *maxp; i++) if (!isdir(&list[i]))
		if ((list[i].flags ^= F_ISMRK) & F_ISMRK) {
			mark++;
			marksize += getblock(list[i].st_size);
		}
	markcount();
	return(2);
}

static reg_t *prepareregexp(mes, arg)
char *mes, *arg;
{
	reg_t *re;
	char *cp, *wild;

	if (arg && *arg) wild = strdup2(arg);
	else if (!(wild = inputstr(mes, 0, 0, "*", -1))) return(NULL);
	if (!*wild || strchr(wild, _SC_)) {
		warning(ENOENT, wild);
		free(wild);
		return(NULL);
	}

	cp = cnvregexp(wild, 1);
	re = regexp_init(cp);
	free(wild);
	free(cp);

	return(re);
}

/*ARGSUSED*/
static int mark_find(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	reg_t *re;
	int i;

	if (!(re = prepareregexp(FINDF_K, arg))) return(1);
	for (i = 0; i < *maxp; i++)
		if (!isdir(&list[i]) && !ismark(&list[i])
		&& regexp_exec(re, list[i].name)) {
			list[i].flags |= F_ISMRK;
			mark++;
			marksize += getblock(list[i].st_size);
		}
	regexp_free(re);
	markcount();
	return(3);
}

/*ARGSUSED*/
static int push_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	if (stackdepth >= MAXSTACK || isdotdir(list[filepos].name)) return(0);
	memcpy(&filestack[stackdepth++], &list[filepos], sizeof(namelist));
	(*maxp)--;
	for (i = filepos; i < *maxp; i++)
		memcpy(&list[i], &list[i + 1], sizeof(namelist));
	if (filepos >= *maxp) filepos = *maxp - 1;
	return(2);
}

/*ARGSUSED*/
static int pop_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	if (stackdepth <= 0) return(0);
	for (i = *maxp; i > filepos + 1; i--)
		memcpy(&list[i], &list[i - 1], sizeof(namelist));
	(*maxp)++;
	memcpy(&list[filepos + 1], &filestack[--stackdepth], sizeof(namelist));
	chgorder = 1;
	return(2);
}

/*ARGSUSED*/
static int symlink_mode(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	dispmode ^= F_SYMLINK;
	return(4);
}

/*ARGSUSED*/
static int filetype_mode(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	dispmode ^= F_FILETYPE;
	return(2);
}

/*ARGSUSED*/
static int dotfile_mode(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	dispmode ^= F_DOTFILE;
	return(4);
}

/*ARGSUSED*/
static int log_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *path;

	if (arg && *arg) path = strdup2(arg);
	else if (!(path = inputstr(LOGD_K, 0, -1, NULL, 1))
	|| !*(path = evalpath(path, 1))) return(1);
	if (!chdir3(path)) {
		warning(-1, path);
		free(path);
		return(1);
	}
	free(path);
	replacefname(list, maxp, NULL);
	return(4);
}

/*ARGSUSED*/
static int log_top(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *path;

	path = strdup2(_SS_);
	if (chdir2(path) < 0) error(path);
	if (findpattern) free(findpattern);
	findpattern = NULL;
	replacefname(list, maxp, path);
	return(4);
}

#ifndef	PAGER
static VOID dump(file)
char *file;
{
	FILE *fp;
	char *buf, *prompt;
	int i;

	if (!(fp = Xfopen(file, "r"))) {
		warning(-1, file);
		return;
	}
	putterms(t_clear);
	tflush();
	buf = NEXT_K;
	i = strlen2(file);
	if (i + strlen(buf) > n_lastcolumn) i = n_lastcolumn - strlen(buf);
	prompt = (char *)malloc2(i + strlen(buf) + 1);
	strncpy3(prompt, file, i, 0);
	strcat(prompt, buf);
	buf = (char *)malloc2(n_column + 2);

	i = 0;
	while (Xfgets(buf, n_column + 1, fp)) {
		locate(0, i);
		putterm(l_clear);
		kanjiputs2(buf, n_column, -1);
		if (++i >= n_line - 1) {
			i = 0;
			if (!yesno(prompt)) break;
		}
	}

	if (Xfeof(fp)) {
		while (i < n_line + 1) {
			locate(0, i++);
			putterm(l_clear);
		}
	}
	Xfclose(fp);
	free(buf);
	free(prompt);
}
#endif

/*ARGSUSED*/
static int view_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
#if	!MSDOS || !defined (_NOARCHIVE)
	char *dir = NULL;
#endif
#if	!MSDOS
	int drive = 0;
#endif

	if (isdir(&list[filepos])
#ifndef	_NOARCHIVE
	|| (archivefile && !(dir = tmpunpack(list, *maxp)))
#endif
#if	!MSDOS && !defined (_NODOSDRIVE)
	|| ((drive = dospath("", NULL)) && !(dir = tmpdosdupl(drive,
	list[filepos].name, list[filepos].st_mode)))
#endif
	) return(1);
	if (!execenv("FD_PAGER", list[filepos].name)) {
#ifdef	PAGER
		execmacro(PAGER, list[filepos].name, NULL, NULL, 1, 0);
#else
		do {
			dump(list[filepos].name);
		} while(!yesno(PEND_K));
#endif
	}
#if	!MSDOS
	if (drive) removetmp(dir, NULL, list[filepos].name);
	else
#endif
#ifndef	_NOARCHIVE
	if (archivefile) removetmp(dir, archivedir, list[filepos].name);
#endif
	return(2);
}

/*ARGSUSED*/
static int edit_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
#if	!MSDOS && !defined (_NODOSDRIVE)
	char *dir = NULL;
	int drive;
#endif

	if (isdir(&list[filepos])) return(warning_bell(list, maxp, arg));
#if	!MSDOS && !defined (_NODOSDRIVE)
	if ((drive = dospath("", NULL)) && !(dir = tmpdosdupl(drive,
	list[filepos].name, list[filepos].st_mode))) return(1);
#endif
	if (!execenv("FD_EDITOR", list[filepos].name)) {
#ifdef	EDITOR
		execmacro(EDITOR, list[filepos].name, NULL, NULL, 1, 0);
#endif
	}
#if	!MSDOS && !defined (_NODOSDRIVE)
	if (drive) {
		if (tmpdosrestore(drive, list[filepos].name,
		list[filepos].st_mode) < 0) warning(-1, list[filepos].name);
		removetmp(dir, NULL, list[filepos].name);
	}
#endif
	return(4);
}

/*ARGSUSED*/
static int sort_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *str[6];
	int i, tmp1, tmp2, val[6], *dupl;

	str[0] = ONAME_K;
	str[1] = OEXT_K;
	str[2] = OSIZE_K;
	str[3] = ODATE_K;
	str[4] = OLEN_K;
	str[5] = ORAW_K;
	val[0] = 1;
	val[1] = 2;
	val[2] = 3;
	val[3] = 4;
	val[4] = 5;
	val[5] = 0;

	tmp1 = sorton & 7;
	tmp2 = sorton & ~7;
	if (arg && *arg) {
		i = atoi(arg);
		if ((i >= 0 && i <= 13) || (i & 7) <= 5) {
			tmp1 = i & 7;
			tmp2 = i & ~7;
		}
	}
	else {
		i = (tmp1) ? 6 : 5;
		if (!tmp1) tmp1 = val[0];
		if (selectstr(&tmp1, i, 0, str, val) == ESC) return(1);
	}

	if (!tmp1) {
		sorton = 0;
		qsort(list, *maxp, sizeof(namelist), cmplist);
	}
	else {
		str[0] = OINC_K;
		str[1] = ODEC_K;
		val[0] = 0;
		val[1] = 8;
		if (selectstr(&tmp2, 2, 56, str, val) == ESC)
			return(1);
		sorton = tmp1 + tmp2;
		dupl = (int *)malloc2(*maxp * sizeof(int));
		for (i = 0; i < *maxp; i++) {
			dupl[i] = list[i].ent;
			list[i].ent = i;
		}
		qsort(list, *maxp, sizeof(namelist), cmplist);
		for (i = 0; i < *maxp; i++) {
			tmp1 = list[i].ent;
			list[i].ent = dupl[tmp1];
		}
		free(dupl);
		chgorder = 1;
	}
	return(2);
}

#ifndef	_NOWRITEFS
/*ARGSUSED*/
static int write_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	if (writefs >= 2 || findpattern) return(warning_bell(list, maxp, arg));
	if ((i = writablefs(".")) <= 0) {
		warning(0, NOWRT_K);
		return(1);
	}
	if (!yesno(WRTOK_K)) return(1);
	if (underhome(NULL) <= 0 && !yesno(HOMOK_K)) return(1);
	arrangedir(list, *maxp, i);
	chgorder = 0;
	return(4);
}
#endif

/*ARGSUSED*/
static int reread_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	getwsize(80, WHEADERMAX + WFOOTER + 2);
	return(4);
}

/*ARGSUSED*/
static int help_message(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
#ifdef	_NOARCHIVE
	help(0);
#else
	help(archivefile != NULL);
#endif
	return(2);
}

/*ARGSUSED*/
static int quit_system(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	if (
#ifndef	_NOARCHIVE
	!archivefile &&
#endif
	!yesno(QUIT_K)) return(1);
	if (savehist > 0) savehistory(0, HISTORYFILE);
	return(-1);
}

/*ARGSUSED*/
static int make_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *path;

	if (arg && *arg) path = strdup2(arg);
	else if (!(path = inputstr(MAKED_K, 1, -1, NULL, 1))
	|| !*(path = evalpath(path, 1))) return(1);
	if (mkdir2(path, 0777) < 0) warning(-1, path);
	free(path);
	return(4);
}

/*ARGSUSED*/
static int copy_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	return(copyfile(list, *maxp, arg, 0));
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int copy_tree(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	return(copyfile(list, *maxp, NULL, 1));
}
#endif	/* !_NOTREE */

/*ARGSUSED*/
static int move_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	return(movefile(list, *maxp, arg, 0));
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int move_tree(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	return(movefile(list, *maxp, NULL, 1));
}
#endif	/* !_NOTREE */

/*ARGSUSED*/
static int rename_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *file;

	if (isdotdir(list[filepos].name)) return(warning_bell(list, maxp, arg));
	if (arg && *arg) {
		file = strdup2(arg);
		errno = EEXIST;
		if (Xaccess(file, F_OK) >= 0 || errno != ENOENT) {
			warning(-1, file);
			free(file);
			return(1);
		}
	}
	else for (;;) {
		if (!(file = inputstr(NEWNM_K, 1, 0, list[filepos].name, -1))
		|| !*(file = evalpath(file, 1)))
			return(1);
		if (Xaccess(file, F_OK) < 0) {
			if (errno == ENOENT) break;
			warning(-1, file);
		}
		else warning(0, WRONG_K);
		free(file);
	}

	if (Xrename(list[filepos].name, file) < 0) {
		warning(-1, file);
		free(file);
		return(1);
	}
	if (strchr(file, _SC_)) {
		free(file);
		file = strdup2("..");
	}
	replacefname(list, maxp, file);
	return(4);
}

/*ARGSUSED*/
static int delete_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	copypolicy = 0;
	if (mark > 0) {
		if (!yesno(DELMK_K)) return(1);
		filepos = applyfile(list, *maxp, unlink2, NULL);
	}
	else if (isdir(&list[filepos])) return(warning_bell(list, maxp, arg));
	else {
		if (!yesno(DELFL_K, list[filepos].name)) return(1);
		if ((i = unlink2(list[filepos].name)) < 0)
			warning(-1, list[filepos].name);
		if (!i) filepos++;
	}
	if (filepos >= *maxp && (filepos -= 2) < 0) filepos = 0;
	return(4);
}

/*ARGSUSED*/
static int delete_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	if (!isdir(&list[filepos]) || isdotdir(list[filepos].name))
		return(warning_bell(list, maxp, arg));
	if (!yesno(DELDR_K, list[filepos].name)) return(1);
	copypolicy = 0;
#if	!MSDOS
	if (islink(&list[filepos])) {
		int i;

		if ((i = unlink2(list[filepos].name)) < 0)
			warning(-1, list[filepos].name);
		if (!i) filepos++;
	}
	else
#endif
	if (!applydir(list[filepos].name, unlink2, NULL, rmdir2, NULL))
		filepos++;
	if (filepos >= *maxp && (filepos -= 2) < 0) filepos = 0;
	return(4);
}

/*ARGSUSED*/
static int find_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *wild;

	if (arg && *arg) wild = strdup2(arg);
	else if (!(wild = inputstr(FINDF_K, 0, 0, "*", -1))) return(1);
	if (strchr(wild, _SC_)) {
		warning(ENOENT, wild);
		free(wild);
		return(1);
	}
	if (findpattern) free(findpattern);
	if (*wild) findpattern = wild;
	else {
		free(wild);
		findpattern = NULL;
	}
	return(4);
}

/*ARGSUSED*/
static int find_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *cp;

	if (!(findregexp = prepareregexp(FINDD_K, arg))) return(1);
	destpath = NULL;
	cp = isdir(&list[filepos]) ? list[filepos].name : ".";
	applydir(cp, findfile, finddir, NULL, NOFND_K);
	regexp_free(findregexp);
	if (!destpath) return(1);

	if ((cp = strrchr(destpath, _SC_))) {
		*(cp++) = '\0';
		chdir2(destpath);
	}
	else cp = destpath;

	replacefname(list, maxp, strdup2(cp));
	free(destpath);
	return(4);
}

/*ARGSUSED*/
static int execute_sh(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *com;
	int i;

	if (arg) com = strdup2(arg);
	else if (!(com = inputstr(NULL, 0, -1, NULL, 0)))
		return(1);
	if (*com) i = execusercomm(com, list[filepos].name, list, maxp, 0, 1);
	else {
		execshell();
		i = 4;
	}
	free(com);
	return(i);
}

/*ARGSUSED*/
static int execute_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
#if	!MSDOS || !defined (_NOARCHIVE)
	char *dir = NULL;
#endif
	char *com;
	int i, len;
#if	!MSDOS
	int drive = 0;
#endif

	len = (isexec(&list[filepos])) ? strlen(list[filepos].name) + 1 : 0;
	if (!(com = inputstr(NULL, 0, len, list[filepos].name, 0)))
		return(1);
	if (!*com) {
		execshell();
		free(com);
		return(4);
	}

#if	(!MSDOS && !defined (_NODOSDRIVE)) || !defined (_NOARCHIVE)
	if (
#ifndef	_NOARCHIVE
	(archivefile && !(dir = tmpunpack(list, *maxp)))
#endif
#if	(!MSDOS && !defined (_NODOSDRIVE)) && !defined (_NOARCHIVE)
	||
#endif
#if	!MSDOS && !defined (_NODOSDRIVE)
	((drive = dospath("", NULL)) && !(dir = tmpdosdupl(drive,
	list[filepos].name, list[filepos].st_mode)))
#endif
	) {
		free(com);
		return(1);
	}
#endif	/* (!MSDOS && !_NODOSDRIVE) || !_NOARCHIVE */
#if	!MSDOS
	if (drive) {
		i = execusercomm(com, list[filepos].name, NULL, NULL, 0, 1);
		removetmp(dir, NULL, list[filepos].name);
	}
	else
#endif
#ifndef	_NOARCHIVE
	if (archivefile) {
		i = execusercomm(com, list[filepos].name, NULL, NULL, 0, 1);
		removetmp(dir, archivedir, list[filepos].name);
	}
	else
#endif
	i = execusercomm(com, list[filepos].name, list, maxp, 0, 1);
	free(com);
	return(i);
}

#ifndef	_NOARCHIVE
/*ARGSUSED*/
static int launch_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	if (launcher(list, *maxp) < 0) return(view_file(list, maxp, arg));
	return(4);
}

/*ARGSUSED*/
static int pack_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *file;
	int i;

	if (arg && *arg) file = strdup2(arg);
	else if (!(file = inputstr(PACK_K, 1, -1, NULL, 1))
	|| !*(file = evalpath(file, 1))) return(1);
	i = pack(file, list, *maxp);
	free(file);
	if (i < 0) {
		putterm(t_bell);
		return(1);
	}
	return(4);
}

/*ARGSUSED*/
static int unpack_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	if (isdir(&list[filepos])) return(warning_bell(list, maxp, arg));
	if (archivefile) i = unpack(archivefile, NULL, list, *maxp, arg, 0);
	else i = unpack(list[filepos].name, NULL, NULL, 0, arg, 0);
	if (i < 0) return(warning_bell(list, maxp, arg));
	if (!i) return(1);
	return(4);
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int unpack_tree(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	int i;

	if (isdir(&list[filepos])) return(warning_bell(list, maxp, arg));
	if (archivefile) i = unpack(archivefile, NULL, list, *maxp, NULL, 1);
	else i = unpack(list[filepos].name, NULL, NULL, 0, NULL, 1);
	if (i <= 0) {
		if (i < 0) warning_bell(list, maxp, arg);
		return(3);
	}
	return(4);
}
#endif	/* !_NOTREE */
#endif	/* !_NOARCHIVE */

/*ARGSUSED*/
static int info_filesys(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *path;
	int i;

	if (arg && *arg) path = evalpath(strdup2(arg), 1);
	else if (!(path = inputstr(FSDIR_K, 0, -1, NULL, 1))) return(1);
	else if (!*(path = evalpath(path, 1))) {
		free(path);
		path = strdup2(".");
	}
	i = infofs(path);
	free(path);
	if (!i) return(1);
	return(3);
}

/*ARGSUSED*/
static int attr_file(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *str[2];
	int i, n, flag, val[2];

	str[0] = CMODE_K;
	str[1] = CDATE_K;
	val[0] = 1;
	val[1] = 2;

	if (mark > 0) {
		for (i = 0; i < *maxp; i++) if (ismark(&list[i])) break;
		if (i >= *maxp) i = filepos;
		flag = 1;
		locate(0, LINFO);
		putterm(l_clear);
		kanjiputs(ATTRM_K);
		if (selectstr(&flag, 2, 35, str, val) == ESC) return(1);
	}
	else {
#if	!MSDOS
		if (islink(&list[filepos])) {
			warning(0, ILLNK_K);
			return(1);
		}
#endif
		i = filepos;
		flag = 3;
	}

	while ((n = inputattr(&list[i], flag)) < 0) warning(0, ILTMS_K);
	if (!n) return(2);

	if (mark > 0) applyfile(list, *maxp, setattr, NULL);
	else if (setattr(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	return(4);
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int tree_dir(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *path;

	if (!(path = tree(0, NULL))) return(2);
	if (chdir2(path) < 0) {
		warning(-1, path);
		free(path);
		return(2);
	}
	free(path);
	if (findpattern) free(findpattern);
	findpattern = NULL;
	replacefname(list, maxp, NULL);
	return(4);
}
#endif	/* !_NOTREE */

#ifndef	_NOARCHIVE
/*ARGSUSED*/
static int backup_tape(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	char *dev;
	int i;

	if (arg && *arg) dev = strdup2(arg);
#if	MSDOS
	else if (!(dev = inputstr(BKUP_K, 1, -1, NULL, 1))
#else
	else if (!(dev = inputstr(BKUP_K, 1, 5, "/dev/", 1))
#endif
	|| !*(dev = evalpath(dev, 1))) return(1);
	i = backup(dev, list, *maxp);
	free(dev);
	if (i <= 0) return(1);
	return(4);
}
#endif	/* !_NOARCHIVE */

/*ARGSUSED*/
static int search_forw(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	isearch = 1;
	locate(0, LHELP);
	putterm(l_clear);
	putterm(t_standout);
	cputs2(SEAF_K);
	putterm(end_standout);
	return(0);
}

/*ARGSUSED*/
static int search_back(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	isearch = -1;
	locate(0, LHELP);
	putterm(l_clear);
	putterm(t_standout);
	cputs2(SEAB_K);
	putterm(end_standout);
	return(0);
}

/*ARGSUSED*/
static int warning_bell(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	putterm(t_bell);
	return(0);
}

/*ARGSUSED*/
static int no_operation(list, maxp, arg)
namelist *list;
int *maxp;
char *arg;
{
	return(0);
}
