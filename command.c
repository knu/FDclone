/*
 *	command.c
 *
 *	Command Execute Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "funcno.h"
#include "kctype.h"
#include "kanji.h"

#if	!MSDOS
#include <sys/file.h>
#endif

#ifndef	_NODOSDRIVE
extern int flushdrv __P_((int, VOID_T (*)__P_((VOID_A))));
#endif

extern int columns;
extern int mark;
extern off_t marksize;
extern off_t blocksize;
extern int isearch;
extern int fnameofs;
extern int chgorder;
extern int stackdepth;
extern int removepolicy;
extern namelist filestack[];
#ifndef	_NOARCHIVE
extern char archivedir[];
#endif
extern int win_x;
extern int win_y;
extern char *destpath;
extern int savehist;
extern int sizeinfo;
#ifndef	_NOSPLITWIN
extern char fullpath[];
#endif

static VOID NEAR replacefname __P_((char *));
static int cur_up __P_((char *));
static int cur_down __P_((char *));
static int cur_right __P_((char *));
static int cur_left __P_((char *));
static int roll_up __P_((char *));
static int roll_down __P_((char *));
static int cur_top __P_((char *));
static int cur_bottom __P_((char *));
static int fname_right __P_((char *));
static int fname_left __P_((char *));
static int one_column __P_((char *));
static int two_columns __P_((char *));
static int three_columns __P_((char *));
static int five_columns __P_((char *));
static VOID NEAR markcount __P_((VOID_A));
static int mark_file __P_((char *));
static int mark_file2 __P_((char *));
static int mark_file3 __P_((char *));
static int mark_all __P_((char *));
static int mark_reverse __P_((char *));
static reg_t *NEAR prepareregexp __P_((char *, char *));
static int mark_find __P_((char *));
static int in_dir __P_((char *));
static int out_dir __P_((char *));
static int push_file __P_((char *));
static int pop_file __P_((char *));
static int symlink_mode __P_((char *));
static int filetype_mode __P_((char *));
static int dotfile_mode __P_((char *));
static int fileflg_mode __P_((char *));
static int log_dir __P_((char *));
static int log_top __P_((char *));
#ifndef	PAGER
static VOID NEAR dump __P_((char *));
#endif
static int NEAR execenv __P_((char *, char *));
static int NEAR execshell __P_((VOID_A));
static int view_file __P_((char *));
static int edit_file __P_((char *));
static int sort_dir __P_((char *));
#ifndef	_NOWRITEFS
static int write_dir __P_((char *));
#endif
static int reread_dir __P_((char *));
static int help_message __P_((char *));
#ifndef	_NOCUSTOMIZE
static int edit_runcom __P_((char *));
#endif
static int quit_system __P_((char *));
static int make_dir __P_((char *));
static int copy_file __P_((char *));
#ifndef	_NOTREE
static int copy_tree __P_((char *));
#endif
static int move_file __P_((char *));
#ifndef	_NOTREE
static int move_tree __P_((char *));
#endif
static int rename_file __P_((char *));
static int delete_file __P_((char *));
static int delete_dir __P_((char *));
static int find_file __P_((char *));
static int find_dir __P_((char *));
static int execute_sh __P_((char *));
static int execute_file __P_((char *));
#ifndef	_NOARCHIVE
static int launch_file __P_((char *));
static int pack_file __P_((char *));
static int unpack_file __P_((char *));
#ifndef	_NOTREE
static int unpack_tree __P_((char *));
#endif
#endif	/* !_NOARCHIVE */
static int info_filesys __P_((char *));
static int attr_file __P_((char *));
#ifndef	_NOTREE
static int tree_dir __P_((char *));
#endif
#ifndef	_NOARCHIVE
static int backup_tape __P_((char *));
#endif
static int search_forw __P_((char *));
static int search_back __P_((char *));
#ifndef	_NOSPLITWIN
static VOID NEAR duplwin __P_((int));
static VOID NEAR movewin __P_((int));
static int split_window __P_((char *));
static int next_window __P_((char *));
#endif
static int warning_bell __P_((char *));
static int no_operation __P_((char *));

#include "functabl.h"

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
#ifndef	_NOCUSTOMIZE
	{'E',		EDIT_RUNCOM,	255},
#endif
	{'F',		FIND_DIR,	255},
	{'H',		DOTFILE_MODE,	255},
#ifndef	_NOTREE
	{'L',		LOG_TREE,	255},
	{'M',		MOVE_TREE,	255},
#endif
#ifdef	HAVEFLAGS
	{'O',		FILEFLG_MODE,	255},
#endif
	{'Q',		QUIT_SYSTEM,	255},
	{'S',		SYMLINK_MODE,	255},
	{'T',		FILETYPE_MODE,	255},
#if	!defined (_NOTREE) && !defined (_NOARCHIVE)
	{'U',		UNPACK_TREE,	255},
#endif
#ifndef	_NOSPLITWIN
	{'/',		SPLIT_WINDOW,	255},
	{'^',		NEXT_WINDOW,	255},
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


static VOID NEAR replacefname(name)
char *name;
{
	if (filepos < maxfile) free(filelist[filepos].name);
	else for (maxfile = 0; maxfile < filepos; maxfile++)
		if (!filelist[maxfile].name)
			filelist[maxfile].name = strdup2("");
	filelist[filepos].name = (name) ? name : strdup2("..");
}

static int cur_up(arg)
char *arg;
{
	int n;

	if (filepos <= 0) return(0);
	if (!arg || (n = atoi(arg)) <= 0) n = 1;
	if ((filepos -= n) < 0) filepos = 0;
	return(2);
}

static int cur_down(arg)
char *arg;
{
	int n;

	if (filepos >= maxfile - 1) return(0);
	if (!arg || (n = atoi(arg)) <= 0) n = 1;
	if ((filepos += n) > maxfile - 1) filepos = maxfile - 1;
	return(2);
}

static int cur_right(arg)
char *arg;
{
	int n;

	if (!arg || (n = atoi(arg)) <= 0) n = 1;
	if (filepos + FILEPERLOW * n < maxfile) filepos += FILEPERLOW * n;
	else if (filepos / FILEPERPAGE == (maxfile - 1) / FILEPERPAGE)
		return(0);
	else {
		filepos = ((maxfile - 1) / FILEPERLOW) * FILEPERLOW
			+ filepos % FILEPERLOW;
		if (filepos >= maxfile) {
			filepos -= FILEPERLOW;
			if (filepos / FILEPERPAGE
			< (maxfile - 1) / FILEPERPAGE)
				filepos = maxfile - 1;
		}
	}
	return(2);
}

static int cur_left(arg)
char *arg;
{
	int n;

	if (filepos < FILEPERLOW) return(0);
	if (!arg || (n = atoi(arg)) <= 0) n = 1;
	if (filepos - FILEPERLOW * n >= 0) filepos -= FILEPERLOW * n;
	else filepos = filepos % FILEPERLOW;
	return(2);
}

static int roll_up(arg)
char *arg;
{
	int i;

	i = (filepos / FILEPERPAGE) * FILEPERPAGE;
	if (i + FILEPERPAGE >= maxfile) return(warning_bell(arg));
	filepos = i + FILEPERPAGE;
	return(2);
}

static int roll_down(arg)
char *arg;
{
	int i;

	i = (filepos / FILEPERPAGE) * FILEPERPAGE;
	if (i - FILEPERPAGE < 0) return(warning_bell(arg));
	filepos = i - FILEPERPAGE;
	return(2);
}

/*ARGSUSED*/
static int cur_top(arg)
char *arg;
{
	if (filepos == 0) return(0);
	filepos = 0;
	return(2);
}

/*ARGSUSED*/
static int cur_bottom(arg)
char *arg;
{
	if (filepos == maxfile - 1) return(0);
	filepos = maxfile - 1;
	return(2);
}

/*ARGSUSED*/
static int fname_right(arg)
char *arg;
{
	if (fnameofs <= 0) return(0);
	fnameofs--;
	return(2);
}

/*ARGSUSED*/
static int fname_left(arg)
char *arg;
{
	int i, m;

	if (islink(&(filelist[filepos]))) i = 0;
	else {
		i = calcwidth();
		if (isdisptyp(dispmode)) {
			m = filelist[filepos].st_mode;
			if ((m & S_IFMT) == S_IFDIR || (m & S_IFMT) == S_IFLNK
			|| (m & S_IFMT) == S_IFSOCK || (m & S_IFMT) == S_IFIFO
			|| (m & (S_IXUSR | S_IXGRP | S_IXOTH))) i--;
		}
	}
	if (i >= strlen3(filelist[filepos].name) - fnameofs) return(0);
	fnameofs++;
	return(2);
}

/*ARGSUSED*/
static int one_column(arg)
char *arg;
{
	columns = 1;
	return(2);
}

/*ARGSUSED*/
static int two_columns(arg)
char *arg;
{
	columns = 2;
	return(2);
}

/*ARGSUSED*/
static int three_columns(arg)
char *arg;
{
	columns = 3;
	return(2);
}

/*ARGSUSED*/
static int five_columns(arg)
char *arg;
{
	columns = 5;
	return(2);
}

static VOID NEAR markcount(VOID_A)
{
	char buf[14 + 1];

	locate(CMARK + 5, LSTATUS);
	cprintf2("%4d", mark);
	if (sizeinfo) {
		locate(CSIZE + 5, LSIZE);
		cprintf2("%14.14s", inscomma(buf, marksize, 3, 14));
	}
}

/*ARGSUSED*/
static int mark_file(arg)
char *arg;
{
	if (isdir(&(filelist[filepos]))) return(0);
	filelist[filepos].tmpflags ^= F_ISMRK;
	if (ismark(&(filelist[filepos]))) {
		mark++;
		if (isfile(&(filelist[filepos])))
			marksize += getblock(filelist[filepos].st_size);
	}
	else {
		mark--;
		if (isfile(&(filelist[filepos])))
			marksize -= getblock(filelist[filepos].st_size);
	}
	markcount();
	return(2);
}

static int mark_file2(arg)
char *arg;
{
	mark_file(arg);
	if (filepos < maxfile - 1) filepos++;
	return(2);
}

static int mark_file3(arg)
char *arg;
{
	mark_file(arg);
	if (filepos < maxfile - 1
	&& filepos / FILEPERPAGE == (filepos + 1) / FILEPERPAGE) filepos++;
	else filepos = (filepos / FILEPERPAGE) * FILEPERPAGE;
	return(2);
}

static int mark_all(arg)
char *arg;
{
	int i;

	if ((arg && atoi(arg) == 0) || (!arg && mark)) {
		for (i = 0; i < maxfile; i++) filelist[i].tmpflags &= ~F_ISMRK;
		mark = 0;
		marksize = 0;
	}
	else {
		mark = 0;
		marksize = 0;
		for (i = 0; i < maxfile; i++) if (!isdir(&(filelist[i]))) {
			filelist[i].tmpflags |= F_ISMRK;
			mark++;
			if (isfile(&(filelist[i])))
				marksize += getblock(filelist[i].st_size);
		}
	}
	markcount();
	return(2);
}

/*ARGSUSED*/
static int mark_reverse(arg)
char *arg;
{
	int i;

	mark = 0;
	marksize = 0;
	for (i = 0; i < maxfile; i++) if (!isdir(&(filelist[i])))
		if ((filelist[i].tmpflags ^= F_ISMRK) & F_ISMRK) {
			mark++;
			if (isfile(&(filelist[i])))
				marksize += getblock(filelist[i].st_size);
		}
	markcount();
	return(2);
}

static reg_t *NEAR prepareregexp(mes, arg)
char *mes, *arg;
{
	reg_t *re;
	char *wild;

	if (arg && *arg) wild = strdup2(arg);
	else if (!(wild = inputstr(mes, 0, 0, "*", -1))) return(NULL);
	if (!*wild || strdelim(wild, 1)) {
		warning(ENOENT, wild);
		free(wild);
		return(NULL);
	}

	re = regexp_init(wild, -1);
	free(wild);
	return(re);
}

static int mark_find(arg)
char *arg;
{
	reg_t *re;
	int i;

	if (!(re = prepareregexp(FINDF_K, arg))) return(1);
	for (i = 0; i < maxfile; i++)
		if (!isdir(&(filelist[i])) && !ismark(&(filelist[i]))
		&& regexp_exec(re, filelist[i].name, 1)) {
			filelist[i].tmpflags |= F_ISMRK;
			mark++;
			if (isfile(&(filelist[i])))
				marksize += getblock(filelist[i].st_size);
		}
	regexp_free(re);
	markcount();
	return(3);
}

static int in_dir(arg)
char *arg;
{
	if (
#ifndef	_NOARCHIVE
	!archivefile &&
#endif
	!strcmp(filelist[filepos].name, "."))
		return(warning_bell(arg));
	return(5);
}

/*ARGSUSED*/
static int out_dir(arg)
char *arg;
{
#ifndef	_NOARCHIVE
	if (archivefile) filepos = -1;
	else
#endif
	replacefname(NULL);
	return(5);
}

/*ARGSUSED*/
static int push_file(arg)
char *arg;
{
	int i;

	if (stackdepth >= MAXSTACK || isdotdir(filelist[filepos].name))
		return(0);
	memcpy((char *)&(filestack[stackdepth++]),
		(char *)&(filelist[filepos]), sizeof(namelist));
	maxfile--;
	for (i = filepos; i < maxfile; i++)
		memcpy((char *)&(filelist[i]),
			(char *)&(filelist[i + 1]), sizeof(namelist));
	if (filepos >= maxfile) filepos = maxfile - 1;
	return(2);
}

/*ARGSUSED*/
static int pop_file(arg)
char *arg;
{
	int i;

	if (stackdepth <= 0) return(0);
	for (i = maxfile; i > filepos + 1; i--)
		memcpy((char *)&(filelist[i]),
			(char *)&(filelist[i - 1]), sizeof(namelist));
	maxfile++;
	memcpy((char *)&(filelist[filepos + 1]),
		(char *)&(filestack[--stackdepth]), sizeof(namelist));
	chgorder = 1;
	return(2);
}

/*ARGSUSED*/
static int symlink_mode(arg)
char *arg;
{
	dispmode ^= F_SYMLINK;
	return(4);
}

/*ARGSUSED*/
static int filetype_mode(arg)
char *arg;
{
	dispmode ^= F_FILETYPE;
	return(2);
}

/*ARGSUSED*/
static int dotfile_mode(arg)
char *arg;
{
	dispmode ^= F_DOTFILE;
	return(4);
}

/*ARGSUSED*/
static int fileflg_mode(arg)
char *arg;
{
	dispmode ^= F_FILEFLAG;
	return(2);
}

static int log_dir(arg)
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
	replacefname(NULL);
	return(4);
}

/*ARGSUSED*/
static int log_top(arg)
char *arg;
{
	char *path;

	path = strdup2(_SS_);
	if (chdir2(path) < 0) error(path);
	if (findpattern) free(findpattern);
	findpattern = NULL;
	replacefname(path);
	return(4);
}

#ifndef	PAGER
static VOID NEAR dump(file)
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
	i = strlen3(file);
	if (i + strlen(buf) > n_lastcolumn) i = n_lastcolumn - strlen(buf);
	prompt = malloc2(i * KANAWID + strlen(buf) + 1);
	strncpy3(prompt, file, &i, 0);
	strcat(prompt, buf);
	buf = malloc2(n_column + 2);

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

static int NEAR execenv(env, arg)
char *env, *arg;
{
	char *command;

	if (!(command = getenv2(env))) return(0);
	putterms(t_clear);
	tflush();
	execmacro(command, arg, 1, 0, 1);
	return(1);
}

static int NEAR execshell(VOID_A)
{
	char *sh;
	int ret;

#if	MSDOS
	if (!(sh = getenv2("FD_SHELL"))
	&& !(sh = getenv2("FD_COMSPEC"))) sh = "command.com";
#else
	if (!(sh = getenv2("FD_SHELL"))) sh = "/bin/sh";
#endif
	putterms(t_end);
	putterms(t_nokeypad);
	tflush();
	sigvecreset();
	cooked2();
	echo2();
	nl2();
	tabs();
	kanjiputs(SHEXT_K);
	tflush();
	ret = system(sh);
	raw2();
	noecho2();
	nonl2();
	notabs();
	sigvecset();
	putterms(t_keypad);
	putterms(t_init);

	return(ret);
}

/*ARGSUSED*/
static int view_file(arg)
char *arg;
{
#if	!defined (_NOARCHIVE) || !defined (_NODOSDRIVE)
	char *dir = NULL;
#endif
#ifndef	_NODOSDRIVE
	int drive = 0;
#endif

	if (isdir(&(filelist[filepos]))
#ifndef	_NOARCHIVE
	|| (archivefile && !(dir = tmpunpack(1)))
#endif
#ifndef	_NODOSDRIVE
	|| (drive = tmpdosdupl("", &dir, 1)) < 0
#endif
	) return(1);

	if (!execenv("FD_PAGER", filelist[filepos].name)) {
#ifdef	PAGER
		execmacro(PAGER, filelist[filepos].name, 1, 0, 1);
#else
		do {
			dump(filelist[filepos].name);
		} while(!yesno(PEND_K));
#endif
	}
#ifndef	_NODOSDRIVE
	if (drive) removetmp(dir, NULL, filelist[filepos].name);
	else
#endif
#ifndef	_NOARCHIVE
	if (archivefile) removetmp(dir, archivedir, filelist[filepos].name);
	else
#endif
	;
	return(2);
}

static int edit_file(arg)
char *arg;
{
#ifndef	_NODOSDRIVE
	char *dir = NULL;
	int drive = 0;
#endif

	if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
#ifndef	_NOARCHIVE
	if (archivefile) return(1);
#endif
#ifndef	_NODOSDRIVE
	if ((drive = tmpdosdupl("", &dir, 1)) < 0) return(1);
#endif
	if (!execenv("FD_EDITOR", filelist[filepos].name)) {
#ifdef	EDITOR
		execmacro(EDITOR, filelist[filepos].name, 1, 0, 1);
#endif
	}
#ifndef	_NODOSDRIVE
	if (drive) {
		if (tmpdosrestore(drive, filelist[filepos].name) < 0)
			warning(-1, filelist[filepos].name);
		removetmp(dir, NULL, filelist[filepos].name);
	}
#endif
	return(4);
}

static int sort_dir(arg)
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
		if (tmp1) i = 6;
		else {
			i = 5;
			tmp1 = val[0];
		}
		if (selectstr(&tmp1, i, 0, str, val) == ESC) return(1);
	}

	if (!tmp1) {
		sorton = 0;
		qsort(filelist, maxfile, sizeof(namelist), cmplist);
	}
	else {
		str[0] = OINC_K;
		str[1] = ODEC_K;
		val[0] = 0;
		val[1] = 8;
		if (selectstr(&tmp2, 2, 56, str, val) == ESC)
			return(1);
		sorton = tmp1 + tmp2;
		dupl = (int *)malloc2(maxfile * sizeof(int));
		for (i = 0; i < maxfile; i++) {
			dupl[i] = filelist[i].ent;
			filelist[i].ent = i;
		}
		waitmes();
		qsort(filelist, maxfile, sizeof(namelist), cmplist);
		for (i = 0; i < maxfile; i++) {
			tmp1 = filelist[i].ent;
			filelist[i].ent = dupl[tmp1];
		}
		free(dupl);
		chgorder = 1;
	}
	return(2);
}

#ifndef	_NOWRITEFS
static int write_dir(arg)
char *arg;
{
	int i;

	if (writefs >= 2 || findpattern) return(warning_bell(arg));
	if ((i = writablefs(".")) <= 0) {
		warning(0, NOWRT_K);
		return(1);
	}
	if (!yesno(WRTOK_K)) return(1);
	if (underhome(NULL) <= 0 && !yesno(HOMOK_K)) return(1);
	arrangedir(i);
	chgorder = 0;
	return(4);
}
#endif

/*ARGSUSED*/
static int reread_dir(arg)
char *arg;
{
#ifndef	_NODOSDRIVE
	int drive;
#endif

	getwsize(80, WHEADERMAX + WFOOTER + WFILEMIN);
#ifndef	_NODOSDRIVE
	if ((drive = dospath3(""))) flushdrv(drive, NULL);
#endif
	return(4);
}

#ifndef	_NOCUSTOMIZE
/*ARGSUSED*/
static int edit_runcom(arg)
char *arg;
{
	if (customize() >= 0) evalenv();
	return(4);
}
#endif

/*ARGSUSED*/
static int help_message(arg)
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
static int quit_system(arg)
char *arg;
{
#ifndef	_NOARCHIVE
	if (archivefile) return(-1);
#endif
	if (!yesno(QUIT_K)) return(1);
	if (savehist > 0) savehistory(0, HISTORYFILE);
	return(-1);
}

static int make_dir(arg)
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

static int copy_file(arg)
char *arg;
{
	return(copyfile(arg, 0));
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int copy_tree(arg)
char *arg;
{
	return(copyfile(NULL, 1));
}
#endif	/* !_NOTREE */

static int move_file(arg)
char *arg;
{
	return(movefile(arg, 0));
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int move_tree(arg)
char *arg;
{
	return(movefile(NULL, 1));
}
#endif	/* !_NOTREE */

static int rename_file(arg)
char *arg;
{
	char *file;

	if (isdotdir(filelist[filepos].name)) return(warning_bell(arg));
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
		if (!(file =
		inputstr(NEWNM_K, 1, 0, filelist[filepos].name, -1))
		|| !*(file = evalpath(file, 1)))
			return(1);
		if (Xaccess(file, F_OK) < 0) {
			if (errno == ENOENT) break;
			warning(-1, file);
		}
		else warning(0, WRONG_K);
		free(file);
	}

	if (Xrename(filelist[filepos].name, file) < 0) {
		warning(-1, file);
		free(file);
		return(1);
	}
	if (strdelim(file, 0)) {
		free(file);
		file = strdup2("..");
	}
	replacefname(file);
	return(4);
}

static int delete_file(arg)
char *arg;
{
	int i;

	removepolicy = 0;
	if (mark > 0) {
		if (!yesno(DELMK_K)) return(1);
		filepos = applyfile(rmvfile, NULL);
	}
	else if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
	else {
		if (!yesno(DELFL_K, filelist[filepos].name)) return(1);
		if ((i = rmvfile(filelist[filepos].name)) < 0)
			warning(-1, filelist[filepos].name);
		if (!i) filepos++;
	}
	if (filepos >= maxfile && (filepos -= 2) < 0) filepos = 0;
	return(4);
}

static int delete_dir(arg)
char *arg;
{
	if (!isdir(&(filelist[filepos])) || isdotdir(filelist[filepos].name))
		return(warning_bell(arg));
	if (!yesno(DELDR_K, filelist[filepos].name)) return(1);
	removepolicy = 0;
#if	!MSDOS
	if (islink(&(filelist[filepos]))) {
		int i;

		if ((i = rmvfile(filelist[filepos].name)) < 0)
			warning(-1, filelist[filepos].name);
		if (!i) filepos++;
	}
	else
#endif
	if (!applydir(filelist[filepos].name, rmvfile, NULL, rmvdir, 3, NULL))
		filepos++;
	if (filepos >= maxfile && (filepos -= 2) < 0) filepos = 0;
	return(4);
}

static int find_file(arg)
char *arg;
{
	char *wild;

	if (arg && *arg) wild = strdup2(arg);
	else if (!(wild = inputstr(FINDF_K, 0, 0, "*", -1))) return(1);

	if (
#ifndef	_NOARCHIVE
	*wild != '/' &&
#endif
	strdelim(wild, 1)) {
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

static int find_dir(arg)
char *arg;
{
	char *cp;

	if (!(findregexp = prepareregexp(FINDD_K, arg))) return(1);
	destpath = NULL;
	cp = isdir(&(filelist[filepos])) ? filelist[filepos].name : ".";
	applydir(cp, findfile, finddir, NULL, 1, NOFND_K);
	regexp_free(findregexp);
	if (!destpath) return(1);

	if (!(cp = strrdelim(destpath, 0))) cp = destpath;
	else {
		*(cp++) = '\0';
		chdir2(destpath);
	}

	replacefname(strdup2(cp));
	free(destpath);
	return(4);
}

static int execute_sh(arg)
char *arg;
{
	char *com;
	int i;

	if (arg) com = strdup2(arg);
	else if (!(com = inputstr(NULL, 0, -1, NULL, 0))) return(1);
	if (*com) {
		i = execusercomm(com, filelist[filepos].name, 0, 1, 0);
		if (i < -1) i = 4;
	}
	else {
		execshell();
		i = 4;
	}
	free(com);
	return(i);
}

/*ARGSUSED*/
static int execute_file(arg)
char *arg;
{
#if	!defined (_NOARCHIVE) || !defined (_NODOSDRIVE)
	char *dir = NULL;
#endif
	char *com;
#if	!MSDOS
	char *tmp;
#endif
	int i, len;
#ifndef	_NODOSDRIVE
	int drive = 0;
#endif

#if	MSDOS
	len = (!isdir(&(filelist[filepos])) && isexec(&(filelist[filepos])))
		? strlen(filelist[filepos].name) + 1 : 0;
	com = inputstr(NULL, 0, len, filelist[filepos].name, 0);
#else
	if (isdir(&(filelist[filepos])) || !isexec(&(filelist[filepos]))) {
		len = 0;
		tmp = filelist[filepos].name;
	}
	else {
		len = strlen(filelist[filepos].name) + 2 + 1;
		tmp = malloc2(len + 1);
		tmp[0] = '.';
		tmp[1] = _SC_;
		strcpy(&(tmp[2]), filelist[filepos].name);
	}
	com = inputstr(NULL, 0, len, tmp, 0);
	if (tmp != filelist[filepos].name) free(tmp);
#endif
	if (!com) return(1);
	if (!*com) {
		execshell();
		free(com);
		return(4);
	}

#ifndef	_NOARCHIVE
	if (archivefile) {
		if (!(dir = tmpunpack(1))) i = 1;
		else {
			i = execusercomm(com, filelist[filepos].name, 0, 1, 1);
			removetmp(dir, archivedir, filelist[filepos].name);
		}
	}
	else
#endif
#ifndef	_NODOSDRIVE
	if ((drive = tmpdosdupl("", &dir, 1)) < 0) i = 1;
	else if (drive) {
		i = execusercomm(com, filelist[filepos].name, 0, 1, 1);
		removetmp(dir, NULL, filelist[filepos].name);
	}
	else
#endif
	{
		i = execusercomm(com, filelist[filepos].name, 0, 1, 0);
	}
	if (i < -1) i = 4;
	free(com);
	return(i);
}

#ifndef	_NOARCHIVE
static int launch_file(arg)
char *arg;
{
	if (launcher() < 0) return(view_file(arg));
	return(4);
}

static int pack_file(arg)
char *arg;
{
#if	!defined (_NOARCHIVE) || !defined (_NODOSDRIVE)
	char *dir = NULL;
#endif
#ifndef	_NODOSDRIVE
	int drive = 0;
#endif
	char *file;
	int i;

	if (arg && *arg) file = strdup2(arg);
	else if (!(file = inputstr(PACK_K, 1, -1, NULL, 1))
	|| !*(file = evalpath(file, 1))
#ifndef	_NOARCHIVE
	|| (archivefile && !(dir = tmpunpack(0)))
#endif
#ifndef	_NODOSDRIVE
	|| (drive = tmpdosdupl("", &dir, 0)) < 0
#endif
	) return(1);

	i = pack(file);
#ifndef	_NODOSDRIVE
	if (drive) removetmp(dir, NULL, "");
	else
#endif
#ifndef	_NOARCHIVE
	if (archivefile) removetmp(dir, archivedir, "");
	else
#endif
	;

	free(file);
	if (i < 0) {
		putterm(t_bell);
		return(1);
	}
	return(4);
}

static int unpack_file(arg)
char *arg;
{
	int i;

	if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
	if (archivefile) i = unpack(archivefile, NULL, arg, 0, 0);
	else i = unpack(filelist[filepos].name, NULL, arg, 0, 1);
	if (i < 0) return(warning_bell(arg));
	if (!i) return(1);
	return(4);
}

#ifndef	_NOTREE
static int unpack_tree(arg)
char *arg;
{
	int i;

	if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
	if (archivefile) i = unpack(archivefile, NULL, NULL, 1, 0);
	else i = unpack(filelist[filepos].name, NULL, NULL, 1, 1);
	if (i <= 0) {
		if (i < 0) warning_bell(arg);
		return(3);
	}
	return(4);
}
#endif	/* !_NOTREE */
#endif	/* !_NOARCHIVE */

static int info_filesys(arg)
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
static int attr_file(arg)
char *arg;
{
	char *str[2];
	int i, n, flag, val[2];

	str[0] = CMODE_K;
	str[1] = CDATE_K;
	val[0] = 1;
	val[1] = 2;

	if (mark > 0) {
		for (i = 0; i < maxfile; i++)
			if (ismark(&(filelist[i]))) break;
		if (i >= maxfile) i = filepos;
		flag = 1;
		locate(0, LINFO);
		putterm(l_clear);
		kanjiputs(ATTRM_K);
		if (selectstr(&flag, 2, 35, str, val) == ESC) return(1);
	}
	else {
#if	!MSDOS
		if (islink(&(filelist[filepos]))) {
			warning(0, ILLNK_K);
			return(1);
		}
#endif
		i = filepos;
		flag = 3;
	}

	while ((n = inputattr(&(filelist[i]), flag)) < 0) warning(0, ILTMS_K);
	if (!n) return(2);

	if (mark > 0) applyfile(setattr, NULL);
	else if (setattr(filelist[filepos].name) < 0)
		warning(-1, filelist[filepos].name);
	return(4);
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int tree_dir(arg)
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
	replacefname(NULL);
	return(4);
}
#endif	/* !_NOTREE */

#ifndef	_NOARCHIVE
static int backup_tape(arg)
char *arg;
{
#if	!defined (_NOARCHIVE) || !defined (_NODOSDRIVE)
	char *dir = NULL;
#endif
#ifndef	_NODOSDRIVE
	int drive = 0;
#endif
	char *dev;
	int i;

	if (arg && *arg) dev = strdup2(arg);
#if	MSDOS
	else if (!(dev = inputstr(BKUP_K, 1, -1, NULL, 1))
#else
	else if (!(dev = inputstr(BKUP_K, 1, 5, "/dev/", 1))
#endif
	|| !*(dev = evalpath(dev, 1))
#ifndef	_NOARCHIVE
	|| (archivefile && !(dir = tmpunpack(0)))
#endif
#ifndef	_NODOSDRIVE
	|| (drive = tmpdosdupl("", &dir, 0)) < 0
#endif
	) return(1);

	i = backup(dev);
#ifndef	_NODOSDRIVE
	if (drive) removetmp(dir, NULL, "");
	else
#endif
#ifndef	_NOARCHIVE
	if (archivefile) removetmp(dir, archivedir, "");
	else
#endif
	;

	free(dev);
	if (i <= 0) return(1);
	return(4);
}
#endif	/* !_NOARCHIVE */

/*ARGSUSED*/
static int search_forw(arg)
char *arg;
{
	isearch = 1;
	locate(0, LHELP);
	putterm(l_clear);
	putterm(t_standout);
	win_x = kanjiputs(SEAF_K);
	win_y = LHELP;
	putterm(end_standout);
	return(0);
}

/*ARGSUSED*/
static int search_back(arg)
char *arg;
{
	isearch = -1;
	locate(0, LHELP);
	putterm(l_clear);
	putterm(t_standout);
	win_x = kanjiputs(SEAB_K);
	win_y = LHELP;
	putterm(end_standout);
	return(0);
}

#ifndef	_NOSPLITWIN
static VOID NEAR duplwin(oldwin)
int oldwin;
{
	int i;

	for (i = 0; i < winvar[oldwin].v_maxfile; i++)
		winvar[oldwin].v_filelist[i].tmpflags
			&= ~(F_ISMRK | F_WSMRK | F_ISARG);
	if (filelist) return;
	maxfile = winvar[oldwin].v_maxfile;
	addlist();
	memcpy((char *)filelist, (char *)(winvar[oldwin].v_filelist),
		winvar[oldwin].v_maxfile * sizeof(namelist));
	for (i = 0; i < winvar[oldwin].v_maxfile; i++)
		filelist[i].name = strdup2(winvar[oldwin].v_filelist[i].name);
	filepos = winvar[oldwin].v_filepos;
	sorton = winvar[oldwin].v_sorton;
	dispmode = winvar[oldwin].v_dispmode;
}

static VOID NEAR movewin(oldwin)
int oldwin;
{
	if (winvar[oldwin].v_fullpath) free(winvar[oldwin].v_fullpath);
	winvar[oldwin].v_fullpath = strdup2(fullpath);
	if (winvar[win].v_fullpath) strcpy(fullpath, winvar[win].v_fullpath);
#ifndef	_NOARCHIVE
	if (winvar[oldwin].v_archivedir) free(winvar[oldwin].v_archivedir);
	winvar[oldwin].v_archivedir = strdup2(archivedir);
	if (winvar[win].v_archivedir)
		strcpy(archivedir, winvar[win].v_archivedir);
#endif
}

/*ARGSUSED*/
static int split_window(arg)
char *arg;
{
	int i, oldwin;

	oldwin = win;
	if (windows < MAXWINDOWS) {
		win = windows++;
		duplwin(oldwin);
		movewin(oldwin);
	}
	else {
		if (win > 0) {
			shutwin(0);
			win = 0;
			duplwin(oldwin);
		}
		for (i = 1; i < windows; i++) shutwin(i);
		windows = 1;
	}
	return(4);
}

/*ARGSUSED*/
static int next_window(arg)
char *arg;
{
	int oldwin;

	if (windows <= 1) return(0);
	oldwin = win;
	if (++win >= windows) win = 0;
	duplwin(oldwin);
	movewin(oldwin);
	if (chdir2(fullpath) < 0) lostcwd(fullpath);
	return(4);
}
#endif

/*ARGSUSED*/
static int warning_bell(arg)
char *arg;
{
	putterm(t_bell);
	return(0);
}

/*ARGSUSED*/
static int no_operation(arg)
char *arg;
{
	return(0);
}
