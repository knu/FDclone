/*
 *	command.c
 *
 *	command executing module
 */

#include "fd.h"
#include "parse.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#ifdef	DEP_ORIGSHELL
#include "system.h"
#endif
#ifdef	DEP_PTY
#include "termemu.h"
#endif

#if	defined (_NOEXTRAATTR) || defined (NOUID)
#define	MAXATTRSEL		2
#define	ATTR_X			35
#define	ATTR_Y			L_INFO
#else
#define	MAXATTRSEL		3
#define	ATTR_X			0
#define	ATTR_Y			L_HELP
#endif

extern int curcolumns;
#if	FD >= 3
extern int widedigit;
#endif
extern int mark;
extern off_t marksize;
extern off_t blocksize;
extern int isearch;
extern int fnameofs;
extern int chgorder;
extern int stackdepth;
extern namelist filestack[];
#ifndef	_NOARCHIVE
extern char archivedir[];
#endif
extern int win_x;
extern int win_y;
extern char *destpath;
#ifndef	_NOTRADLAYOUT
extern int tradlayout;
#endif
extern int sizeinfo;
extern int wheader;
extern char fullpath[];
extern int hideclock;
#ifdef	DEP_ORIGSHELL
extern int fdmode;
#endif

static int NEAR dochdir4 __P_((CONST char *, int));
static int cur_up __P_((CONST char *));
static int cur_down __P_((CONST char *));
static int cur_right __P_((CONST char *));
static int cur_left __P_((CONST char *));
static int roll_up __P_((CONST char *));
static int roll_down __P_((CONST char *));
static int cur_top __P_((CONST char *));
static int cur_bottom __P_((CONST char *));
static int fname_right __P_((CONST char *));
static int fname_left __P_((CONST char *));
static int one_column __P_((CONST char *));
static int two_columns __P_((CONST char *));
static int three_columns __P_((CONST char *));
static int five_columns __P_((CONST char *));
static VOID NEAR markcount __P_((VOID_A));
static int mark_file __P_((CONST char *));
static int mark_file2 __P_((CONST char *));
static int mark_file3 __P_((CONST char *));
static int mark_all __P_((CONST char *));
static int mark_reverse __P_((CONST char *));
static reg_ex_t *NEAR prepareregexp __P_((CONST char *, CONST char *));
static int mark_find __P_((CONST char *));
static int in_dir __P_((CONST char *));
static int out_dir __P_((CONST char *));
static int push_file __P_((CONST char *));
static int pop_file __P_((CONST char *));
static int symlink_mode __P_((CONST char *));
static int filetype_mode __P_((CONST char *));
static int dotfile_mode __P_((CONST char *));
static int fileflg_mode __P_((CONST char *));
static int log_dir __P_((CONST char *));
static int log_top __P_((CONST char *));
static VOID NEAR clearscreen __P_((VOID_A));
#ifndef	PAGER
static VOID NEAR dump __P_((CONST char *));
#endif
static int NEAR execenv __P_((CONST char *, CONST char *));
static int NEAR execshell __P_((VOID_A));
static int view_file __P_((CONST char *));
static int edit_file __P_((CONST char *));
static int sort_dir __P_((CONST char *));
#ifndef	_NOWRITEFS
static int write_dir __P_((CONST char *));
#endif
static int reread_dir __P_((CONST char *));
static int help_message __P_((CONST char *));
#ifndef	_NOCUSTOMIZE
static int edit_config __P_((CONST char *));
#endif
#ifdef	DEP_PTY
static int NEAR confirmpty __P_((VOID_A));
#endif
static int quit_system __P_((CONST char *));
static int make_dir __P_((CONST char *));
static int copy_file __P_((CONST char *));
#ifndef	_NOTREE
static int copy_tree __P_((CONST char *));
#endif
static int move_file __P_((CONST char *));
#ifndef	_NOTREE
static int move_tree __P_((CONST char *));
#endif
static int rename_file __P_((CONST char *));
static int delete_file __P_((CONST char *));
static int delete_dir __P_((CONST char *));
static int find_file __P_((CONST char *));
static int find_dir __P_((CONST char *));
static int execute_sh __P_((CONST char *));
static int execute_file __P_((CONST char *));
#ifndef	_NOARCHIVE
static int launch_file __P_((CONST char *));
static int pack_file __P_((CONST char *));
static int unpack_file __P_((CONST char *));
# ifndef	_NOTREE
static int unpack_tree __P_((CONST char *));
# endif
#endif	/* !_NOARCHIVE */
static int info_filesys __P_((CONST char *));
static int NEAR selectattr __P_((CONST char *));
static int attr_file __P_((CONST char *));
#ifndef	_NOEXTRAATTR
static int attr_dir __P_((CONST char *));
#endif
#ifndef	_NOTREE
static int tree_dir __P_((CONST char *));
#endif
#ifndef	_NOARCHIVE
static int backup_tape __P_((CONST char *));
#endif
static int search_forw __P_((CONST char *));
static int search_back __P_((CONST char *));
#ifndef	_NOSPLITWIN
static VOID NEAR duplwin __P_((int));
static VOID NEAR movewin __P_((int));
static int split_window __P_((CONST char *));
static int next_window __P_((CONST char *));
#endif
#ifndef	_NOEXTRAWIN
static int widen_window __P_((CONST char *));
static int narrow_window __P_((CONST char *));
static int kill_window __P_((CONST char *));
#endif
static int warning_bell __P_((CONST char *));
static int no_operation __P_((CONST char *));

#include "functabl.h"

reg_ex_t *findregexp = NULL;
#ifndef	_NOWRITEFS
int writefs = 0;
#endif
#if	FD >= 2
int loopcursor = 0;
#endif
int internal_status = FNC_FAIL;
int maxbind = 0;
#if	defined (DEP_DYNAMICLIST) || !defined (_NOCUSTOMIZE)
int origmaxbind = 0;
#endif
#ifdef	DEP_DYNAMICLIST
bindlist_t bindlist = NULL;
origbindlist_t origbindlist
#else	/* !DEP_DYNAMICLIST */
# ifndef	_NOCUSTOMIZE
origbindlist_t origbindlist = NULL;
# endif
bindlist_t bindlist
#endif	/* !DEP_DYNAMICLIST */
= {
	{K_UP,		CUR_UP,		FNO_NONE},
	{K_DOWN,	CUR_DOWN,	FNO_NONE},
	{K_RIGHT,	CUR_RIGHT,	FNO_NONE},
	{K_LEFT,	CUR_LEFT,	FNO_NONE},
	{K_NPAGE,	ROLL_UP,	FNO_NONE},
	{K_PPAGE,	ROLL_DOWN,	FNO_NONE},
#ifdef	_NOTREE
	{K_F(1),	HELP_MESSAGE,	FNO_NONE},
#else
	{K_F(1),	LOG_DIR,	FNO_NONE},
#endif
	{K_F(2),	EXECUTE_FILE,	FNO_NONE},
	{K_F(3),	COPY_FILE,	FNO_NONE},
	{K_F(4),	DELETE_FILE,	FNO_NONE},
	{K_F(5),	RENAME_FILE,	FNO_NONE},
	{K_F(6),	SORT_DIR,	FNO_NONE},
	{K_F(7),	FIND_FILE,	FNO_NONE},
#ifdef	_NOTREE
	{K_F(8),	LOG_DIR,	FNO_NONE},
#else
	{K_F(8),	TREE_DIR,	FNO_NONE},
#endif
	{K_F(9),	EDIT_FILE,	FNO_NONE},
#ifndef	_NOARCHIVE
	{K_F(10),	UNPACK_FILE,	FNO_NONE},
#endif
	{K_F(11),	ATTR_FILE,	FNO_NONE},
	{K_F(12),	INFO_FILESYS,	FNO_NONE},
	{K_F(13),	MOVE_FILE,	FNO_NONE},
	{K_F(14),	DELETE_DIR,	FNO_NONE},
	{K_F(15),	MAKE_DIR,	FNO_NONE},
	{K_F(16),	EXECUTE_SH,	FNO_NONE},
#ifndef	_NOWRITEFS
	{K_F(17),	WRITE_DIR,	FNO_NONE},
#endif
#ifndef	_NOARCHIVE
	{K_F(18),	BACKUP_TAPE,	FNO_NONE},
#endif
	{K_F(19),	VIEW_FILE,	FNO_NONE},
#ifndef	_NOARCHIVE
	{K_F(20),	PACK_FILE,	FNO_NONE},
	{K_CR,		LAUNCH_FILE,	IN_DIR},
#else
	{K_CR,		VIEW_FILE,	IN_DIR},
#endif
	{K_BS,		OUT_DIR,	FNO_NONE},
	{K_DC,		PUSH_FILE,	FNO_NONE},
	{K_IC,		POP_FILE,	FNO_NONE},
	{'\t',		MARK_FILE,	FNO_NONE},
	{K_ESC,		QUIT_SYSTEM,	FNO_NONE},

	{'<',		CUR_TOP,	FNO_NONE},
	{'>',		CUR_BOTTOM,	FNO_NONE},
	{'1',		ONE_COLUMN,	FNO_NONE},
	{'2',		TWO_COLUMNS,	FNO_NONE},
	{'3',		THREE_COLUMNS,	FNO_NONE},
	{'5',		FIVE_COLUMNS,	FNO_NONE},
	{'(',		FNAME_RIGHT,	FNO_NONE},
	{')',		FNAME_LEFT,	FNO_NONE},
	{' ',		MARK_FILE2,	FNO_NONE},
	{'+',		MARK_ALL,	FNO_NONE},
	{'-',		MARK_REVERSE,	FNO_NONE},
	{'*',		MARK_FIND,	FNO_NONE},
	{']',		PUSH_FILE,	FNO_NONE},
	{'[',		POP_FILE,	FNO_NONE},
	{'?',		HELP_MESSAGE,	FNO_NONE},

	{'a',		ATTR_FILE,	FNO_NONE},
#ifndef	_NOARCHIVE
	{'b',		BACKUP_TAPE,	FNO_NONE},
#endif
	{'c',		COPY_FILE,	FNO_NONE},
	{'d',		DELETE_FILE,	FNO_NONE},
	{'e',		EDIT_FILE,	FNO_NONE},
	{'f',		FIND_FILE,	FNO_NONE},
	{'h',		EXECUTE_SH,	FNO_NONE},
	{'i',		INFO_FILESYS,	FNO_NONE},
	{'k',		MAKE_DIR,	FNO_NONE},
	{'l',		LOG_DIR,	FNO_NONE},
	{'\\',		LOG_TOP,	FNO_NONE},
	{'m',		MOVE_FILE,	FNO_NONE},
#ifndef	_NOARCHIVE
	{'p',		PACK_FILE,	FNO_NONE},
#endif
	{'q',		QUIT_SYSTEM,	FNO_NONE},
	{'r',		RENAME_FILE,	FNO_NONE},
	{'s',		SORT_DIR,	FNO_NONE},
#ifndef	_NOTREE
	{'t',		TREE_DIR,	FNO_NONE},
#endif
#ifndef	_NOARCHIVE
	{'u',		UNPACK_FILE,	FNO_NONE},
#endif
	{'v',		VIEW_FILE,	FNO_NONE},
#ifndef	_NOWRITEFS
	{'w',		WRITE_DIR,	FNO_NONE},
#endif
	{'x',		EXECUTE_FILE,	FNO_NONE},
#ifndef	_NOEXTRAATTR
	{'A',		ATTR_DIR,	FNO_NONE},
#endif
#ifndef	_NOTREE
	{'C',		COPY_TREE,	FNO_NONE},
#endif
	{'D',		DELETE_DIR,	FNO_NONE},
#ifndef	_NOCUSTOMIZE
	{'E',		EDIT_CONFIG,	FNO_NONE},
#endif
	{'F',		FIND_DIR,	FNO_NONE},
	{'H',		DOTFILE_MODE,	FNO_NONE},
#ifndef	_NOTREE
	{'L',		LOG_TREE,	FNO_NONE},
	{'M',		MOVE_TREE,	FNO_NONE},
#endif
#ifdef	HAVEFLAGS
	{'O',		FILEFLG_MODE,	FNO_NONE},
#endif
	{'Q',		QUIT_SYSTEM,	FNO_NONE},
	{'S',		SYMLINK_MODE,	FNO_NONE},
	{'T',		FILETYPE_MODE,	FNO_NONE},
#if	!defined (_NOTREE) && !defined (_NOARCHIVE)
	{'U',		UNPACK_TREE,	FNO_NONE},
#endif
#ifndef	_NOSPLITWIN
	{'/',		SPLIT_WINDOW,	FNO_NONE},
	{'^',		NEXT_WINDOW,	FNO_NONE},
#endif
#ifndef	_NOEXTRAWIN
	{'W',		WIDEN_WINDOW,	FNO_NONE},
	{'N',		NARROW_WINDOW,	FNO_NONE},
	{'K',		KILL_WINDOW,	FNO_NONE},
#endif
	{K_HOME,	MARK_ALL,	FNO_NONE},
	{K_END,		MARK_REVERSE,	FNO_NONE},
	{K_BEG,		CUR_TOP,	FNO_NONE},
	{K_EOL,		CUR_BOTTOM,	FNO_NONE},
	{K_HELP,	HELP_MESSAGE,	FNO_NONE},
	{K_CTRL('@'),	MARK_FILE3,	FNO_NONE},
	{K_CTRL('L'),	REREAD_DIR,	FNO_NONE},
	{K_CTRL('R'),	SEARCH_BACK,	FNO_NONE},
	{K_CTRL('S'),	SEARCH_FORW,	FNO_NONE},
	{-1,		NO_OPERATION,	FNO_NONE}
};


static int NEAR dochdir4(path, raw)
CONST char *path;
int raw;
{
	if (chdir4(path, raw, archivefile) < 0) {
		warning(-1, path);
		return(FNC_CANCEL);
	}

	return(FNC_EFFECT);
}

static int cur_up(arg)
CONST char *arg;
{
#if	FD >= 2
	int p;
#endif
	int n, old;

	old = filepos;
	if (!arg || (n = Xatoi(arg)) <= 0) n = 1;
	filepos -= n;
#if	FD >= 2
	if (loopcursor) {
		p = FILEPERPAGE;
		while (filepos < 0 || filepos / p < old / p)
			filepos += p;
		if (filepos >= maxfile) filepos = maxfile - 1;
	}
	else
#endif
	if (filepos < 0) filepos = 0;

	return((filepos != old) ? FNC_UPDATE : FNC_NONE);
}

static int cur_down(arg)
CONST char *arg;
{
#if	FD >= 2
	int p;
#endif
	int n, old;

	old = filepos;
	if (!arg || (n = Xatoi(arg)) <= 0) n = 1;
	filepos += n;
#if	FD >= 2
	if (loopcursor) {
		p = FILEPERPAGE;
		if (filepos >= maxfile) filepos = (filepos / p) * p;
		while (filepos / p > old / p) {
			filepos -= p;
			if (filepos < 0) break;
		}
		if (filepos < 0) filepos = 0;
	}
	else
#endif
	if (filepos >= maxfile) filepos = maxfile - 1;

	return((filepos != old) ? FNC_UPDATE : FNC_NONE);
}

static int cur_right(arg)
CONST char *arg;
{
	int n, r, p, old;

	old = filepos;
	if (!arg || (n = Xatoi(arg)) <= 0) n = 1;
	r = FILEPERROW;
	p = FILEPERPAGE;
	filepos += r * n;

#if	FD >= 2
	if (loopcursor) {
		if (filepos >= maxfile)
			filepos = (filepos / p) * p + (old % r);
		while (filepos / p > old / p) {
			filepos -= p;
			if (filepos < 0) break;
		}
		if (filepos < 0) filepos = 0;
	}
	else
#endif
	if (filepos < maxfile) /*EMPTY*/;
	else if (old / p == (maxfile - 1) / p) filepos = old;
	else {
		filepos = ((maxfile - 1) / r) * r + (old % r);
		if (filepos >= maxfile) {
			filepos -= r;
			if (filepos / p < (maxfile - 1) / p)
				filepos = maxfile - 1;
		}
	}

	return((filepos != old) ? FNC_UPDATE : FNC_NONE);
}

static int cur_left(arg)
CONST char *arg;
{
#if	FD >= 2
	int p;
#endif
	int n, r, old;

	old = filepos;
	if (!arg || (n = Xatoi(arg)) <= 0) n = 1;
	r = FILEPERROW;
	filepos -= r * n;

#if	FD >= 2
	if (loopcursor) {
		p = FILEPERPAGE;
		while (filepos < 0 || filepos / p < old / p)
			filepos += p;
		if (filepos >= maxfile) {
			filepos = (maxfile - 1) / r * r + (old % r);
			if (filepos >= maxfile) filepos -= r;
		}
	}
	else
#endif
	if (filepos >= 0) /*EMPTY*/;
	else filepos = old % r;

	return((filepos != old) ? FNC_UPDATE : FNC_NONE);
}

static int roll_up(arg)
CONST char *arg;
{
	int n, p, old;

	old = filepos;
	if (!arg || (n = Xatoi(arg)) <= 0) n = 1;
	p = FILEPERPAGE;
	filepos = (filepos / p + n) * p;
	if (filepos >= maxfile) {
		filepos = old;
		return(warning_bell(arg));
	}

	return(FNC_UPDATE);
}

static int roll_down(arg)
CONST char *arg;
{
	int n, p, old;

	old = filepos;
	if (!arg || (n = Xatoi(arg)) <= 0) n = 1;
	p = FILEPERPAGE;
	filepos = (filepos / p - n) * p;
	if (filepos < 0) {
		filepos = old;
		return(warning_bell(arg));
	}

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int cur_top(arg)
CONST char *arg;
{
	if (filepos <= 0) return(FNC_NONE);
	filepos = 0;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int cur_bottom(arg)
CONST char *arg;
{
	if (filepos >= maxfile - 1) return(FNC_NONE);
	filepos = maxfile - 1;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int fname_right(arg)
CONST char *arg;
{
	if (fnameofs <= 0) return(FNC_NONE);
	fnameofs--;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int fname_left(arg)
CONST char *arg;
{
#ifndef	NOSYMLINK
# ifdef	DEP_DOSEMU
	char path[MAXPATHLEN];
# endif
	char tmp[MAXPATHLEN];
#endif	/* !NOSYMLINK */
	int i, m;

#ifndef	NOSYMLINK
	if (islink(&(filelist[filepos]))) {
# ifndef	_NOARCHIVE
		if (archivefile) {
			if (!(filelist[filepos].linkname)) i = 0;
			else {
				i = strlen(filelist[filepos].linkname);
				if (i > strsize(tmp)) i = strsize(tmp);
				Xstrncpy(tmp, filelist[filepos].linkname, i);
			}
		}
		else
# endif
		i = Xreadlink(fnodospath(path, filepos), tmp, strsize(tmp));
		tmp[i] = '\0';
		i = 1 - (strlen3(tmp) + 4);
	}
	else
#endif	/* !NOSYMLINK */
	{
		i = calcwidth();
		if (isdisptyp(dispmode)) {
			m = filelist[filepos].st_mode;
			if ((m & S_IFMT) == S_IFDIR || (m & S_IFMT) == S_IFLNK
			|| (m & S_IFMT) == S_IFSOCK || (m & S_IFMT) == S_IFIFO
			|| (m & S_IEXEC_ALL))
				i--;
		}
	}
	if (i >= strlen3(filelist[filepos].name) - fnameofs) return(FNC_NONE);
	fnameofs++;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int one_column(arg)
CONST char *arg;
{
	curcolumns = 1;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int two_columns(arg)
CONST char *arg;
{
	curcolumns = 2;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int three_columns(arg)
CONST char *arg;
{
	curcolumns = 3;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int five_columns(arg)
CONST char *arg;
{
	curcolumns = 5;

	return(FNC_UPDATE);
}

static VOID NEAR markcount(VOID_A)
{
#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		Xlocate(TC_MARK, TL_PATH);
		VOID_C XXcprintf("%<*d", TD_MARK, mark);
		Xlocate(TC_MARK + TD_MARK + TW_MARK, TL_PATH);
		VOID_C XXcprintf("%<'*qd", TD_SIZE, marksize);

		return;
	}
#endif
	Xlocate(C_MARK + W_MARK, L_STATUS);
	VOID_C XXcprintf("%<*d", D_MARK, mark);
	if (sizeinfo) {
		Xlocate(C_SIZE + W_SIZE, L_SIZE);
		VOID_C XXcprintf("%<'*qd", D_SIZE, marksize);
	}
}

/*ARGSUSED*/
static int mark_file(arg)
CONST char *arg;
{
	if (isdir(&(filelist[filepos]))) return(FNC_NONE);
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

	return(FNC_UPDATE);
}

static int mark_file2(arg)
CONST char *arg;
{
	mark_file(arg);
	if (filepos < maxfile - 1) filepos++;

	return(FNC_UPDATE);
}

static int mark_file3(arg)
CONST char *arg;
{
	mark_file(arg);
	if (filepos < maxfile - 1
	&& filepos / FILEPERPAGE == (filepos + 1) / FILEPERPAGE)
		filepos++;
	else filepos = (filepos / FILEPERPAGE) * FILEPERPAGE;

	return(FNC_UPDATE);
}

static int mark_all(arg)
CONST char *arg;
{
	int i;

	if ((arg && Xatoi(arg) == 0) || (!arg && mark)) {
		for (i = 0; i < maxfile; i++) filelist[i].tmpflags &= ~F_ISMRK;
		mark = 0;
		marksize = (off_t)0;
	}
	else {
		mark = 0;
		marksize = (off_t)0;
		for (i = 0; i < maxfile; i++) if (!isdir(&(filelist[i]))) {
			filelist[i].tmpflags |= F_ISMRK;
			mark++;
			if (isfile(&(filelist[i])))
				marksize += getblock(filelist[i].st_size);
		}
	}
	markcount();

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int mark_reverse(arg)
CONST char *arg;
{
	int i;

	mark = 0;
	marksize = (off_t)0;
	for (i = 0; i < maxfile; i++) if (!isdir(&(filelist[i])))
		if ((filelist[i].tmpflags ^= F_ISMRK) & F_ISMRK) {
			mark++;
			if (isfile(&(filelist[i])))
				marksize += getblock(filelist[i].st_size);
		}
	markcount();

	return(FNC_UPDATE);
}

static reg_ex_t *NEAR prepareregexp(mes, arg)
CONST char *mes, *arg;
{
	reg_ex_t *re;
	char *wild;

	if (arg && *arg) wild = Xstrdup(arg);
	else if (!(wild = inputstr(mes, 0, 0, "*", -1))) return(NULL);
	if (!*wild || strdelim(wild, 1)) {
		warning(ENOENT, wild);
		Xfree(wild);
		return(NULL);
	}

	re = regexp_init(wild, -1);
	Xfree(wild);

	return(re);
}

static int mark_find(arg)
CONST char *arg;
{
	reg_ex_t *re;
	int i;

	if (!(re = prepareregexp(FINDF_K, arg))) return(FNC_CANCEL);
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

	return(FNC_HELPSPOT);
}

static int in_dir(arg)
CONST char *arg;
{
	if (!isdir(&(filelist[filepos]))) return(warning_bell(arg));
#ifndef	_NOARCHIVE
	else if (archivefile) /*EMPTY*/;
#endif
	else if (isdotdir(filelist[filepos].name) == 2)
		return(warning_bell(arg));

	return(dochdir4(filelist[filepos].name, 1));
}

/*ARGSUSED*/
static int out_dir(arg)
CONST char *arg;
{
	CONST char *path;

#ifndef	_NOARCHIVE
	if (archivefile) path = NULL;
	else
#endif
	path = parentpath;

	return(dochdir4(path, 1));
}

/*ARGSUSED*/
static int push_file(arg)
CONST char *arg;
{
	int i;

	if (stackdepth >= MAXSTACK || isdotdir(filelist[filepos].name))
		return(FNC_NONE);
	memcpy((char *)&(filestack[stackdepth++]),
		(char *)&(filelist[filepos]), sizeof(namelist));
	maxfile--;
	for (i = filepos; i < maxfile; i++)
		memcpy((char *)&(filelist[i]),
			(char *)&(filelist[i + 1]), sizeof(namelist));
	if (filepos >= maxfile) filepos = maxfile - 1;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int pop_file(arg)
CONST char *arg;
{
	int i;

	if (stackdepth <= 0) return(FNC_NONE);
	for (i = maxfile; i > filepos + 1; i--)
		memcpy((char *)&(filelist[i]),
			(char *)&(filelist[i - 1]), sizeof(namelist));
	maxfile++;
	memcpy((char *)&(filelist[filepos + 1]),
		(char *)&(filestack[--stackdepth]), sizeof(namelist));
	chgorder = 1;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int symlink_mode(arg)
CONST char *arg;
{
	dispmode ^= F_SYMLINK;

	return(FNC_EFFECT);
}

/*ARGSUSED*/
static int filetype_mode(arg)
CONST char *arg;
{
	dispmode ^= F_FILETYPE;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int dotfile_mode(arg)
CONST char *arg;
{
	dispmode ^= F_DOTFILE;

	return(FNC_EFFECT);
}

/*ARGSUSED*/
static int fileflg_mode(arg)
CONST char *arg;
{
	dispmode ^= F_FILEFLAG;

	return(FNC_UPDATE);
}

static int log_dir(arg)
CONST char *arg;
{
	char *path;
	int no;

	if (arg && *arg) path = Xstrdup(arg);
	else if (!(path = inputstr(LOGD_K, 0, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
	else if (!*(path = evalpath(path, 0))) {
		Xfree(path);
		return(FNC_CANCEL);
	}

	no = dochdir4(path, 0);
	Xfree(path);

	return(no);
}

/*ARGSUSED*/
static int log_top(arg)
CONST char *arg;
{
	return(dochdir4(rootpath, 1));
}

static VOID NEAR clearscreen(VOID_A)
{
#ifdef	DEP_PTY
	int i, yy;

	if (isptymode()) {
		yy = filetop(win);
		for (i = 0; i < FILEPERROW; i++) {
			Xlocate(0, yy + i);
			Xputterm(L_CLEAR);
		}
	}
	else
#endif	/* DEP_PTY */
	Xputterms(T_CLEAR);
	Xtflush();
}

#ifndef	PAGER
static VOID NEAR dump(file)
CONST char *file;
{
	XFILE *fp;
	char *buf, *prompt;
	int i, len, min, max;

	if (!(fp = Xfopen(file, "r"))) {
		warning(-1, file);
		return;
	}
	clearscreen();

	min = 0;
	max = n_line - 1;
# ifdef	DEP_PTY
	if (isptymode()) {
		min = filetop(win);
		max = min + FILEPERROW;
	}
# endif

	Xtflush();
	buf = NEXT_K;
	i = strlen3(file);
	len = strlen2(buf);
	if (i + len > n_lastcolumn) i = n_lastcolumn - len;
	prompt = Xmalloc(i * KANAWID + strlen(buf) + 1);
	VOID_C strncpy2(prompt, file, &i, 0);
	strcat(prompt, buf);

	i = min;
	while ((buf = Xfgets(fp))) {
		Xlocate(0, i);
		Xputterm(L_CLEAR);
		VOID_C XXcprintf("%^.*k", n_column, buf);
		Xfree(buf);
		if (++i >= max) {
			i = min;
			hideclock = 1;
			if (!yesno(prompt)) break;
		}
	}

	if (Xfeof(fp)) {
		while (i < max) {
			Xlocate(0, i++);
			Xputterm(L_CLEAR);
		}
	}
	VOID_C Xfclose(fp);
	Xfree(prompt);
}
#endif	/* !PAGER */

static int NEAR execenv(env, arg)
CONST char *env, *arg;
{
	char *command;

	if (!(command = getenv2(env)) || !*command) return(0);
	clearscreen();
	ptymacro(command, arg, F_NOCONFIRM | F_IGNORELIST);

	return(1);
}

static int NEAR execshell(VOID_A)
{
#if	MSDOS
	char *sh2;
#endif
#ifdef	DEP_PTY
	int x, y, min, max;
#endif
	char *sh;
	int mode, ret, wastty;

#if	MSDOS
	if (!(sh = getenv2("FD_SHELL"))) sh = "COMMAND.COM";
#else
	if (!(sh = getenv2("FD_SHELL"))) sh = "/bin/sh";
#endif
#ifdef	DEP_ORIGSHELL
	else if (!strpathcmp(getshellname(sh, NULL, NULL), FDSHELL)) sh = NULL;
#endif

#if	MSDOS
# ifdef	DEP_ORIGSHELL
	if (!sh) /*EMPTY*/;
	else
# endif
	if ((sh2 = getenv2("FD_COMSPEC"))) sh = sh2;
#endif	/* MSDOS */

#ifdef	DEP_ORIGSHELL
	if (!sh && !fdmode) {
		warning(0, RECUR_K);
		return(-1);
	}
#endif

	sigvecset(0);
	mode = Xtermmode(0);
	wastty = isttyiomode;
	VOID_C Xkanjiputs(SHEXT_K);
#ifdef	DEP_PTY
	if (isptymode()) {
		min = filetop(win);
		max = min + FILEPERROW;
		y = max;
		if (!isttyiomode) Xttyiomode(0);
		if (getxy(&x, &y) < 0 || --y >= max) y = max - 1;
		regionscroll(C_SCROLLFORW, 1, 0, y, min, max - 1);
	}
	else
#endif	/* DEP_PTY */
	Xcputnl();
	Xtflush();
	if (isttyiomode) Xstdiomode();
	ret = ptysystem(sh);
#ifndef	DEP_ORIGSHELL
	checkscreen(-1, -1);
#endif
	if (wastty) Xttyiomode(wastty - 1);
	VOID_C Xtermmode(mode);
	sigvecset(1);

	return(ret);
}

/*ARGSUSED*/
static int view_file(arg)
CONST char *arg;
{
#if	!defined (_NOARCHIVE) || defined (DEP_PSEUDOPATH)
	char *dir;
#endif
#ifdef	DEP_PSEUDOPATH
	int drive;
#endif

#if	!defined (_NOARCHIVE) || defined (DEP_PSEUDOPATH)
	dir = NULL;
#endif
#ifdef	DEP_PSEUDOPATH
	drive = 0;
#endif
	if (isdir(&(filelist[filepos]))) return(FNC_CANCEL);
#ifndef	_NOARCHIVE
	else if (archivefile) {
		if (!(dir = tmpunpack(1))) return(FNC_CANCEL);
	}
#endif
#ifdef	DEP_PSEUDOPATH
	else if ((drive = tmpdosdupl(nullstr, &dir, 1)) < 0)
		return(FNC_CANCEL);
#endif

	if (!execenv("FD_PAGER", filelist[filepos].name)) {
#ifdef	PAGER
		ptymacro(PAGER, filelist[filepos].name,
			F_NOCONFIRM | F_IGNORELIST);
#else
		do {
			dump(filelist[filepos].name);
			hideclock = 1;
		} while (!yesno(PEND_K));
#endif
	}

#ifdef	DEP_PSEUDOPATH
	if (drive) removetmp(dir, filelist[filepos].name);
	else
#endif
#ifndef	_NOARCHIVE
	if (archivefile) removetmp(dir, NULL);
	else
#endif
	/*EMPTY*/;

	return(FNC_UPDATE);
}

static int edit_file(arg)
CONST char *arg;
{
#ifdef	DEP_PSEUDOPATH
	char *dir;
	int drive;

	dir = NULL;
#endif

	if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
#ifndef	_NOARCHIVE
	if (archivefile) return(FNC_CANCEL);
#endif
#ifdef	DEP_PSEUDOPATH
	if ((drive = tmpdosdupl(nullstr, &dir, 1)) < 0) return(FNC_CANCEL);
#endif
	if (!execenv("FD_EDITOR", filelist[filepos].name)) {
#ifdef	EDITOR
		ptymacro(EDITOR, filelist[filepos].name,
			F_NOCONFIRM | F_IGNORELIST);
#endif
	}
#ifdef	DEP_PSEUDOPATH
	if (drive) {
		if (tmpdosrestore(drive, filelist[filepos].name) < 0)
			warning(-1, filelist[filepos].name);
		removetmp(dir, filelist[filepos].name);
	}
#endif

	return(FNC_EFFECT);
}

static int sort_dir(arg)
CONST char *arg;
{
	CONST char *str[6];
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
		i = Xatoi(arg);
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
		if (selectstr(&tmp1, i, 0, str, val) != K_CR)
			return(FNC_CANCEL);
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
		if (selectstr(&tmp2, 2, 56, str, val) != K_CR)
			return(FNC_CANCEL);
		sorton = tmp1 + tmp2;
		dupl = (int *)Xmalloc(maxfile * sizeof(int));
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
		Xfree(dupl);
		chgorder = 1;
	}

	return(FNC_UPDATE);
}

#ifndef	_NOWRITEFS
static int write_dir(arg)
CONST char *arg;
{
	int fs;

	if (writefs >= 2 || findpattern) return(warning_bell(arg));
	if ((fs = writablefs(curpath)) <= 0) {
		warning(0, NOWRT_K);
		return(FNC_CANCEL);
	}
	if (!yesno(WRTOK_K)) return(FNC_CANCEL);
	if (underhome(NULL) <= 0 && !yesno(HOMOK_K)) return(FNC_CANCEL);
	arrangedir(fs);
	chgorder = 0;

	return(FNC_EFFECT);
}
#endif	/* !_NOWRITEFS */

/*ARGSUSED*/
static int reread_dir(arg)
CONST char *arg;
{
#ifdef	DEP_DOSDRIVE
	int drive;
#endif

	checkscreen(WCOLUMNMIN, WHEADERMAX + WFOOTER + WFILEMIN);
#ifdef	DEP_DOSDRIVE
	if ((drive = dospath3(nullstr))) flushdrv(drive, NULL);
#endif

	return(FNC_EFFECT);
}

#ifndef	_NOCUSTOMIZE
static int edit_config(arg)
CONST char *arg;
{
	if (!arg && FILEPERROW < WFILEMINCUSTOM) {
		warning(0, NOROW_K);
		return(FNC_CANCEL);
	}
	if (customize(arg) <= 0) return(FNC_CANCEL);

	return(FNC_EFFECT);
}
#endif

/*ARGSUSED*/
static int help_message(arg)
CONST char *arg;
{
#ifdef	_NOARCHIVE
	help(0);
#else
	help((archivefile) ? 1 : 0);
#endif

	return(FNC_UPDATE);
}

#ifdef	DEP_PTY
static int NEAR confirmpty(VOID_A)
{
	int i;

	VOID_C checkallpty();
	for (i = 0; i < MAXWINDOWS; i++) if (ptylist[i].pid) break;
	if (i < MAXWINDOWS && !yesno(KILL_K)) return(1);

	return(0);
}
#endif	/* DEP_PTY */

/*ARGSUSED*/
static int quit_system(arg)
CONST char *arg;
{
#ifdef	DEP_ORIGSHELL
	CONST char *str[3];
	int val[3];
#endif
	int n;

#ifndef	_NOARCHIVE
	if (archivefile) return(FNC_QUIT);
#endif
	n = 0;

#ifdef	DEP_ORIGSHELL
	if (!fdmode) {
		Xlocate(0, L_MESLINE);
		Xputterm(L_CLEAR);
		VOID_C Xkanjiputs(QUIT_K);
		val[0] = 0;
		val[1] = 1;
		val[2] = 2;
		str[0] = QYES_K;
		str[1] = QNO_K;
		str[2] = QCHG_K;
		if (selectstr(&n, 3, 20, str, val) != K_CR || n == 1)
			return(FNC_CANCEL);
	}
	else
#endif	/* DEP_ORIGSHELL */
	if (!yesno(QUIT_K)) return(FNC_CANCEL);

#ifdef	DEP_PTY
	if (confirmpty()) return(FNC_CANCEL);
#endif

	return((n) ? FNC_FAIL : FNC_QUIT);
}

static int make_dir(arg)
CONST char *arg;
{
	char *path;

	if (arg && *arg) path = Xstrdup(arg);
	else if (!(path = inputstr(MAKED_K, 1, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
	else if (!*(path = evalpath(path, 0))) {
		Xfree(path);
		return(FNC_CANCEL);
	}

	if (mkdir2(path, 0777) < 0) warning(-1, path);
	Xfree(path);

	return(FNC_EFFECT);
}

static int copy_file(arg)
CONST char *arg;
{
	return(copyfile(arg, 0));
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int copy_tree(arg)
CONST char *arg;
{
	return(copyfile(NULL, 1));
}
#endif	/* !_NOTREE */

static int move_file(arg)
CONST char *arg;
{
	return(movefile(arg, 0));
}

#ifndef	_NOTREE
/*ARGSUSED*/
static int move_tree(arg)
CONST char *arg;
{
	return(movefile(NULL, 1));
}
#endif	/* !_NOTREE */

static int rename_file(arg)
CONST char *arg;
{
#ifdef	DEP_DOSEMU
	char path[MAXPATHLEN];
#endif
	char *file;

	if (isdotdir(filelist[filepos].name)) return(warning_bell(arg));
	if (arg && *arg) {
		file = Xstrdup(arg);
		errno = EEXIST;
		if (Xaccess(file, F_OK) >= 0 || errno != ENOENT) {
			warning(-1, file);
			Xfree(file);
			return(FNC_CANCEL);
		}
	}
	else for (;;) {
		file = inputstr(NEWNM_K, 1, 0, filelist[filepos].name, -1);
		if (!file || !*file) return(FNC_CANCEL);
		if (Xaccess(file, F_OK) < 0) {
			if (errno == ENOENT) break;
			warning(-1, file);
		}
		else warning(0, WRONG_K);
		Xfree(file);
	}

	if (Xrename(fnodospath(path, filepos), file) < 0) {
		warning(-1, file);
		Xfree(file);
		return(FNC_CANCEL);
	}
	if (strdelim(file, 0)) {
		Xfree(file);
		file = Xstrdup(parentpath);
	}
	setlastfile(file);

	return(FNC_EFFECT);
}

static int delete_file(arg)
CONST char *arg;
{
	CONST char *cp;
	int len;

	if (mark > 0) {
		if (!yesno(DELMK_K)) return(FNC_CANCEL);
		if (prepareremove(0, mark) < 0) return(FNC_CANCEL);
		filepos = applyfile(rmvfile, NULL);
	}
	else if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
	else {
		cp = DELFL_K;
		len = strlen2(cp) - strsize("%.*s");
		if (!yesno(cp, n_lastcolumn - len, filelist[filepos].name))
			return(FNC_CANCEL);
		if (prepareremove(0, 1) < 0) return(FNC_CANCEL);
		filepos = applyfile(rmvfile, NULL);
	}
	if (filepos >= maxfile && (filepos -= 2) < 0) filepos = 0;

	return(FNC_EFFECT);
}

static int delete_dir(arg)
CONST char *arg;
{
#if	!defined (NOSYMLINK) && defined (DEP_DOSEMU)
	char path[MAXPATHLEN];
#endif
	CONST char *cp;
	int ret, len;

	if (!isdir(&(filelist[filepos])) || isdotdir(filelist[filepos].name))
		return(warning_bell(arg));
	cp = DELDR_K;
	len = strlen2(cp) - strsize("%.*s");
	if (!yesno(cp, n_lastcolumn - len, filelist[filepos].name))
		return(FNC_CANCEL);
	cp = filelist[filepos].name;
#ifndef	NOSYMLINK
	if (islink(&(filelist[filepos]))) {
		if (prepareremove(0, 1) < 0) return(FNC_CANCEL);
		ret = rmvfile(fnodospath(path, filepos));
		if (ret == APL_ERROR) warning(-1, cp);
	}
	else
#endif
	{
		if (prepareremove(1, 1) < 0) return(FNC_CANCEL);
		ret = applydir(cp, rmvfile, NULL, rmvdir, ORD_NOPREDIR, NULL);
	}
	if (ret == APL_OK) filepos++;
	if (filepos >= maxfile && (filepos -= 2) < 0) filepos = 0;

	return(FNC_EFFECT);
}

static int find_file(arg)
CONST char *arg;
{
	char *wild;

	if (arg) wild = Xstrdup(arg);
	else if (!(wild = inputstr(FINDF_K, 0, 0, "*", -1)))
		return(FNC_CANCEL);

#ifndef	_NOARCHIVE
	if (*wild == '/') /*EMPTY*/;
	else
#endif
	if (strdelim(wild, 1)) {
		warning(ENOENT, wild);
		Xfree(wild);
		return(FNC_CANCEL);
	}
	Xfree(findpattern);
	if (*wild) findpattern = wild;
	else {
		Xfree(wild);
		findpattern = NULL;
	}

	return(FNC_EFFECT);
}

static int find_dir(arg)
CONST char *arg;
{
	CONST char *cp;
	char *tmp;

	if (!(findregexp = prepareregexp(FINDD_K, arg))) return(FNC_CANCEL);
	destpath = NULL;
	if (isdir(&(filelist[filepos]))) cp = filelist[filepos].name;
	else cp = curpath;
	VOID_C applydir(cp, findfile, finddir, NULL, ORD_NORMAL, NOFND_K);
	regexp_free(findregexp);
	if (!destpath) return(FNC_CANCEL);

	if (!(tmp = strrdelim(destpath, 0))) tmp = destpath;
	else {
		*(tmp++) = '\0';
		if (dochdir4(destpath, 1) == FNC_CANCEL) {
			Xfree(destpath);
			return(FNC_CANCEL);
		}
	}

	setlastfile(tmp);
	Xfree(destpath);

	return(FNC_EFFECT);
}

int evalstatus(n)
int n;
{
	if (n < 0) return(FNC_CANCEL);
	if (internal_status == FNC_QUIT || internal_status > FNC_CANCEL)
		return(internal_status);

	return(FNC_EFFECT);
}

static int execute_sh(arg)
CONST char *arg;
{
	char *com;
	int ret;

	if (arg) com = Xstrdup(arg);
	else {
		Xttyiomode(1);
		com = inputshellloop(-1, NULL);
		Xttyiomode(0);
		if (!com) return(FNC_CANCEL);
	}
	if (*com) {
		ret = ptyusercomm(com,
			(filepos < maxfile) ? filelist[filepos].name : NULL,
			F_ARGSET);
		ret = evalstatus(ret);
	}
	else {
		execshell();
		ret = FNC_EFFECT;
	}
	Xfree(com);

	return(ret);
}

/*ARGSUSED*/
static int execute_file(arg)
CONST char *arg;
{
#if	!defined (_NOARCHIVE) || defined (DEP_PSEUDOPATH)
	char *dir;
#endif
#ifndef	CWDINPATH
	char *tmp;
#endif
#ifdef	DEP_PSEUDOPATH
	int drive;
#endif
	char *com;
	int ret, len;

#if	!defined (_NOARCHIVE) && !defined (_NOBROWSE)
	if (browselist) return(warning_bell(arg));
#endif

#if	!defined (_NOARCHIVE) || defined (DEP_PSEUDOPATH)
	dir = NULL;
#endif
#ifdef	DEP_PSEUDOPATH
	drive = 0;
#endif
#ifdef	CWDINPATH
	len = (!isdir(&(filelist[filepos])) && isexec(&(filelist[filepos])))
		? strlen(filelist[filepos].name) + 1 : 0;
	Xttyiomode(1);
	com = inputshellloop(len, filelist[filepos].name);
	Xttyiomode(0);
#else
	if (isdir(&(filelist[filepos])) || !isexec(&(filelist[filepos]))) {
		len = 0;
		tmp = filelist[filepos].name;
	}
	else {
		len = strlen(filelist[filepos].name) + 2 + 1;
		tmp = Xmalloc(len + 1);
		tmp[0] = '.';
		tmp[1] = _SC_;
		Xstrcpy(&(tmp[2]), filelist[filepos].name);
	}
	Xttyiomode(1);
	com = inputshellloop(len, tmp);
	Xttyiomode(0);
	if (tmp != filelist[filepos].name) Xfree(tmp);
#endif
	if (!com) return(FNC_CANCEL);
	if (!*com) {
		execshell();
		Xfree(com);
		return(FNC_EFFECT);
	}

#ifndef	_NOARCHIVE
	if (archivefile) {
		if (!(dir = tmpunpack(1))) ret = -1;
		else {
			ret = ptyusercomm(com, filelist[filepos].name,
				F_ARGSET | F_IGNORELIST);
			removetmp(dir, NULL);
		}
	}
	else
#endif
#ifdef	DEP_PSEUDOPATH
	if ((drive = tmpdosdupl(nullstr, &dir, 1)) < 0) ret = -1;
	else if (drive) {
		ret = ptyusercomm(com, filelist[filepos].name,
			F_ARGSET | F_IGNORELIST);
		removetmp(dir, filelist[filepos].name);
	}
	else
#endif
	{
		ret = ptyusercomm(com, filelist[filepos].name, F_ARGSET);
	}
	ret = evalstatus(ret);
	Xfree(com);

	return(ret);
}

#ifndef	_NOARCHIVE
static int launch_file(arg)
CONST char *arg;
{
	if (!launcher()) return(view_file(arg));

	return(FNC_EFFECT);
}

static int pack_file(arg)
CONST char *arg;
{
	char *file;
	int ret;

	if (arg && *arg) file = Xstrdup(arg);
	else if (!(file = inputstr(PACK_K, 1, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
	else if (!*(file = evalpath(file, 0))) {
		Xfree(file);
		return(FNC_CANCEL);
	}

	ret = pack(file);
	Xfree(file);
	if (ret < 0) {
		Xputterm(T_BELL);
		return(FNC_CANCEL);
	}
	if (!ret) return(FNC_CANCEL);

	return(FNC_EFFECT);
}

static int unpack_file(arg)
CONST char *arg;
{
#ifdef	DEP_DOSEMU
	char path[MAXPATHLEN];
#endif
	int ret;

	if (archivefile) ret = unpack(archivefile, NULL, arg,
		0, F_ARGSET | F_ISARCH | F_NOADDOPT);
	else if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
	else ret = unpack(fnodospath(path, filepos), NULL, arg,
		0, F_ARGSET | F_ISARCH | F_NOADDOPT | F_IGNORELIST);
	if (ret < 0) return(warning_bell(arg));
	if (!ret) return(FNC_CANCEL);

	return(FNC_EFFECT);
}

#ifndef	_NOTREE
static int unpack_tree(arg)
CONST char *arg;
{
#ifdef	DEP_DOSEMU
	char path[MAXPATHLEN];
#endif
	int ret;

	if (archivefile) ret = unpack(archivefile, NULL, NULL,
		1, F_ARGSET | F_ISARCH | F_NOADDOPT);
	else if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
	else ret = unpack(fnodospath(path, filepos), NULL, NULL,
		1, F_ARGSET | F_ISARCH | F_NOADDOPT | F_IGNORELIST);
	if (ret <= 0) {
		if (ret < 0) warning_bell(arg);
		return(FNC_HELPSPOT);
	}

	return(FNC_EFFECT);
}
#endif	/* !_NOTREE */
#endif	/* !_NOARCHIVE */

static int info_filesys(arg)
CONST char *arg;
{
	char *path;
	int ret;

	if (arg && *arg) path = Xstrdup(arg);
	else if (!(path = inputstr(FSDIR_K, 0, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
	else if (!*(path = evalpath(path, 0))) {
		Xfree(path);
		path = Xstrdup(curpath);
	}

	ret = infofs(path);
	Xfree(path);
	if (!ret) return(FNC_CANCEL);

	return(FNC_HELPSPOT);
}

static int NEAR selectattr(s)
CONST char *s;
{
	CONST char *str[MAXATTRSEL];
	int n, val[MAXATTRSEL];

	str[0] = CMODE_K;
	str[1] = CDATE_K;
	val[0] = ATR_MODEONLY;
	val[1] = ATR_TIMEONLY;
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
	str[2] = COWNR_K;
	val[2] = ATR_OWNERONLY;
#endif

	n = ATR_MODEONLY;
	Xlocate(0, ATTR_Y);
	Xputterm(L_CLEAR);
#if	defined (_NOEXTRAATTR) || defined (NOUID)
	VOID_C Xkanjiputs(s);
#else
	VOID_C Xattrkanjiputs(s, 1);
#endif
	if (selectstr(&n, MAXATTRSEL, ATTR_X, str, val) != K_CR) return(-1);

	return(n);
}

/*ARGSUSED*/
static int attr_file(arg)
CONST char *arg;
{
	int i, n, flag;

	if (mark > 0) {
		for (i = 0; i < maxfile; i++)
			if (ismark(&(filelist[i]))) break;
		if (i >= maxfile) i = filepos;
		if ((flag = selectattr(ATTRM_K)) < 0) return(FNC_CANCEL);
		flag |= ATR_MULTIPLE;
	}
	else {
#ifndef	NOSYMLINK
		if (islink(&(filelist[filepos]))) {
			warning(0, ILLNK_K);
			return(FNC_CANCEL);
		}
#endif
		i = filepos;
		flag = 0;
	}

	while ((n = inputattr(&(filelist[i]), flag)) < 0) warning(0, ILTMS_K);
	if (!n) {
		if (FILEPERROW < WFILEMINATTR) return(FNC_EFFECT);
		return(FNC_UPDATE);
	}
	applyfile(setattr, NULL);

	return(FNC_EFFECT);
}

#ifndef	_NOEXTRAATTR
/*ARGSUSED*/
static int attr_dir(arg)
CONST char *arg;
{
	int n, flag;

	if (!isdir(&(filelist[filepos]))) return(warning_bell(arg));

	if ((flag = selectattr(ATTRD_K)) < 0) return(FNC_CANCEL);
	flag |= (ATR_MULTIPLE | ATR_RECURSIVE);
	while ((n = inputattr(&(filelist[filepos]), flag)) < 0)
		warning(0, ILTMS_K);
	if (!n) {
		if (FILEPERROW < WFILEMINATTR) return(FNC_EFFECT);
		return(FNC_UPDATE);
	}
	VOID_C applydir(filelist[filepos].name, setattr,
		NULL, setattr, ORD_NOPREDIR, NULL);

	return(FNC_EFFECT);
}
#endif	/* !_NOEXTRAATTR */

#ifndef	_NOTREE
/*ARGSUSED*/
static int tree_dir(arg)
CONST char *arg;
{
	char *path;
	int no;

	if (!(path = tree(0, NULL))) return(FNC_UPDATE);
	no = dochdir4(path, 1);
	if (no == FNC_CANCEL) no = FNC_UPDATE;
	Xfree(path);

	return(no);
}
#endif	/* !_NOTREE */

#ifndef	_NOARCHIVE
static int backup_tape(arg)
CONST char *arg;
{
	char *dir, *dev;
#ifdef	DEP_PSEUDOPATH
	int drive;
#endif
	int ret;

#if	!defined (_NOARCHIVE) && !defined (_NOBROWSE)
	if (browselist) return(warning_bell(arg));
#endif

	dir = NULL;
#ifdef	DEP_PSEUDOPATH
	drive = 0;
#endif
	if (arg && *arg) dev = Xstrdup(arg);
#if	MSDOS
	else if (!(dev = inputstr(BKUP_K, 1, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
#else
	else if (!(dev = inputstr(BKUP_K, 1, 5, "/dev/", HST_PATH)))
		return(FNC_CANCEL);
#endif
	else if (!*(dev = evalpath(dev, 0))) {
		Xfree(dev);
		return(FNC_CANCEL);
	}
	else if (archivefile) {
		if (!(dir = tmpunpack(0))) {
			Xfree(dev);
			return(FNC_CANCEL);
		}
	}
#ifdef	DEP_PSEUDOPATH
	else if ((drive = tmpdosdupl(nullstr, &dir, 0)) < 0) {
		Xfree(dev);
		return(FNC_CANCEL);
	}
#endif

	ret = backup(dev);
#ifdef	DEP_PSEUDOPATH
	if (drive) removetmp(dir, NULL);
	else
#endif
	if (archivefile) removetmp(dir, NULL);

	Xfree(dev);
	if (ret <= 0) return(FNC_CANCEL);

	return(FNC_EFFECT);
}
#endif	/* !_NOARCHIVE */

/*ARGSUSED*/
static int search_forw(arg)
CONST char *arg;
{
	isearch = 1;
	Xlocate(0, L_HELP);
	Xputterm(L_CLEAR);
	win_x = Xattrkanjiputs(SEAF_K, 1);
	win_y = L_HELP;

	return(FNC_NONE);
}

/*ARGSUSED*/
static int search_back(arg)
CONST char *arg;
{
	isearch = -1;
	Xlocate(0, L_HELP);
	Xputterm(L_CLEAR);
	win_x = Xattrkanjiputs(SEAB_K, 1);
	win_y = L_HELP;

	return(FNC_NONE);
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
	i = ((maxfile) ? maxfile : 1) * sizeof(namelist);
	filelist = addlist(filelist, maxfile, &maxent);
	memcpy((char *)filelist, (char *)(winvar[oldwin].v_filelist), i);
	for (i = 0; i < winvar[oldwin].v_maxfile; i++)
		filelist[i].name = Xstrdup(winvar[oldwin].v_filelist[i].name);
	lastfile = Xstrdup(winvar[oldwin].v_lastfile);
	filepos = winvar[oldwin].v_filepos;
	sorton = winvar[oldwin].v_sorton;
	dispmode = winvar[oldwin].v_dispmode;
}

static VOID NEAR movewin(oldwin)
int oldwin;
{
	Xfree(winvar[oldwin].v_fullpath);
	winvar[oldwin].v_fullpath = Xstrdup(fullpath);
	if (winvar[win].v_fullpath) Xstrcpy(fullpath, winvar[win].v_fullpath);
# ifndef	_NOARCHIVE
	Xfree(winvar[oldwin].v_archivedir);
	winvar[oldwin].v_archivedir = Xstrdup(archivedir);
	if (winvar[win].v_archivedir)
		Xstrcpy(archivedir, winvar[win].v_archivedir);
# endif
}

int nextwin(VOID_A)
{
	int oldwin;

	if (windows <= 1) return(-1);

	if (lastfile || internal_status == FNC_EFFECT) getfilelist();
	oldwin = win;
	if (++win >= windows) win = 0;
	duplwin(oldwin);
	movewin(oldwin);
	if (chdir3(fullpath, 1) < 0) lostcwd(fullpath);

	return(0);
}

/*ARGSUSED*/
static int split_window(arg)
CONST char *arg;
{
	winvartable tmp;
	int i, oldwin;

	oldwin = win;
	if (windows >= MAXWINDOWS) {
# ifdef	_NOEXTRAWIN
#  ifdef	DEP_PTY
		if (confirmpty()) return(FNC_CANCEL);
#  endif
		if (win > 0) {
			shutwin(0);
			win = 0;
			duplwin(oldwin);
		}
		for (i = 1; i < windows; i++) shutwin(i);
#  ifdef	DEP_PTY
		killallpty();
#  endif
		windows = 1;
		calcwin();
# else	/* !_NOEXTRAWIN */
		warning(0, NOWIN_K);
		return(FNC_CANCEL);
# endif	/* !_NOEXTRAWIN */
	}
# ifdef	_NOEXTRAWIN
	else if (fileperrow(windows + 1) < WFILEMIN)
# else
	else if (FILEPERROW < WFILEMIN * 2 + 1)
# endif
	{
		warning(0, NOROW_K);
		return(FNC_CANCEL);
	}
	else {
		win++;
		memcpy((char *)&tmp, (char *)&(winvar[windows]),
			sizeof(winvartable));
		memmove((char *)&(winvar[win + 1]), (char *)&(winvar[win]),
			(windows++ - win) * sizeof(winvartable));
		memcpy((char *)&(winvar[win]), (char *)&tmp,
			sizeof(winvartable));

# ifdef	_NOEXTRAWIN
		calcwin();
# else
		i = (winvar[oldwin].v_fileperrow - 1) / 2;
		FILEPERROW = (winvar[oldwin].v_fileperrow - 1) - i;
		winvar[oldwin].v_fileperrow = i;
# endif

# ifdef	DEP_PTY
		insertwin(win, windows);
		changewsize(wheader, windows);
# endif
		duplwin(oldwin);
		movewin(oldwin);
	}

	return(FNC_EFFECT);
}

/*ARGSUSED*/
static int next_window(arg)
CONST char *arg;
{
	if (nextwin() < 0) return(FNC_NONE);
# ifdef	DEP_PTY
	if (!ptyinternal && ptylist[win].pid && ptylist[win].status < 0) {
		rewritefile(0);
		VOID_C frontend();
		if (internal_status > FNC_FAIL) return(internal_status);
	}
# endif

	return(FNC_EFFECT);
}
#endif	/* !_NOSPLITWIN */

#ifndef	_NOEXTRAWIN
static int widen_window(arg)
CONST char *arg;
{
	int n, next;

	if (windows <= 1) return(FNC_NONE);
	if (!arg || (n = Xatoi(arg)) <= 0) n = 1;
	if ((next = win + 1) >= windows) next = 0;
	if (winvar[next].v_fileperrow - n < WFILEMIN)
		n = winvar[next].v_fileperrow - WFILEMIN;
	if (n <= 0) return(warning_bell(arg));

	winvar[next].v_fileperrow -= n;
	winvar[win].v_fileperrow += n;
# ifdef	DEP_PTY
	changewsize(wheader, windows);
# endif

	return(FNC_EFFECT);
}

static int narrow_window(arg)
CONST char *arg;
{
	int n, next;

	if (windows <= 1) return(FNC_NONE);
	if (!arg || (n = Xatoi(arg)) <= 0) n = 1;
	if ((next = win + 1) >= windows) next = 0;
	if (FILEPERROW - n < WFILEMIN) n = FILEPERROW - WFILEMIN;
	if (n <= 0) return(warning_bell(arg));

	winvar[next].v_fileperrow += n;
	winvar[win].v_fileperrow -= n;
# ifdef	DEP_PTY
	changewsize(wheader, windows);
# endif

	return(FNC_EFFECT);
}

/*ARGSUSED*/
static int kill_window(arg)
CONST char *arg;
{
	winvartable tmp;
	int prev;

	if (windows <= 1) return(FNC_NONE);
	memcpy((char *)&tmp, (char *)&(winvar[win]), sizeof(winvartable));
	memmove((char *)&(winvar[win]), (char *)&(winvar[win + 1]),
		(--windows - win) * sizeof(winvartable));
	memcpy((char *)&(winvar[windows]), (char *)&tmp, sizeof(winvartable));

	if ((prev = win - 1) < 0) prev = windows - 1;
	winvar[prev].v_fileperrow += winvar[windows].v_fileperrow + 1;

# ifdef	DEP_PTY
	deletewin(win, windows);
	changewsize(wheader, windows);
# endif

	win = prev;
	duplwin(windows);
	movewin(windows);
	shutwin(windows);
	if (chdir3(fullpath, 1) < 0) lostcwd(fullpath);

# ifdef	DEP_PTY
	if (!ptyinternal && ptylist[win].pid && ptylist[win].status < 0) {
		rewritefile(0);
		VOID_C frontend();
		if (internal_status > FNC_FAIL) return(internal_status);
	}
# endif

	return(FNC_EFFECT);
}
#endif	/* !_NOEXTRAWIN */

/*ARGSUSED*/
static int warning_bell(arg)
CONST char *arg;
{
	Xputterm(T_BELL);

	return(FNC_NONE);
}

/*ARGSUSED*/
static int no_operation(arg)
CONST char *arg;
{
	return(FNC_NONE);
}
