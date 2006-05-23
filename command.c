/*
 *	command.c
 *
 *	command executing module
 */

#include "fd.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#ifndef	_NOORIGSHELL
#include "system.h"
#endif

#ifndef	_NOPTY
#include "termemu.h"
#endif

#if	defined (_NOEXTRAATTR) || defined (NOUID)
#define	MAXATTRSEL	2
#define	ATTR_X		35
#define	ATTR_Y		L_INFO
#else
#define	MAXATTRSEL	3
#define	ATTR_X		0
#define	ATTR_Y		L_HELP
#endif

extern int curcolumns;
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
#ifndef	_NOTRADLAYOUT
extern int tradlayout;
#endif
extern int sizeinfo;
extern int wheader;
extern char fullpath[];
extern int hideclock;
#ifndef	_NOORIGSHELL
extern int fdmode;
#endif
#ifndef	_NOPTY
extern int ptymode;
extern int ptyinternal;
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
static VOID NEAR clearscreen __P_((VOID_A));
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
static int edit_config __P_((char *));
#endif
#ifndef	_NOPTY
static int confirmpty __P_((VOID_A));
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
# ifndef	_NOTREE
static int unpack_tree __P_((char *));
# endif
#endif	/* !_NOARCHIVE */
static int info_filesys __P_((char *));
static int NEAR selectattr __P_((char *));
static int attr_file __P_((char *));
#ifndef	_NOEXTRAATTR
static int attr_dir __P_((char *));
#endif
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
#ifndef	_NOEXTRAWIN
static int widen_window __P_((char *));
static int narrow_window __P_((char *));
static int kill_window __P_((char *));
#endif
static int warning_bell __P_((char *));
static int no_operation __P_((char *));

#include "functabl.h"

reg_t *findregexp = NULL;
#ifndef	_NOWRITEFS
int writefs = 0;
#endif
#if	FD >= 2
int loopcursor = 0;
#endif
int internal_status = FNC_FAIL;
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
	{K_CR,		LAUNCH_FILE,	IN_DIR},
#else
	{K_CR,		VIEW_FILE,	IN_DIR},
#endif
	{K_BS,		OUT_DIR,	255},
	{K_DC,		PUSH_FILE,	255},
	{K_IC,		POP_FILE,	255},
	{'\t',		MARK_FILE,	255},
	{K_ESC,		QUIT_SYSTEM,	255},

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
#ifndef	_NOEXTRAATTR
	{'A',		ATTR_DIR,	255},
#endif
#ifndef	_NOTREE
	{'C',		COPY_TREE,	255},
#endif
	{'D',		DELETE_DIR,	255},
#ifndef	_NOCUSTOMIZE
	{'E',		EDIT_CONFIG,	255},
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
#ifndef	_NOEXTRAWIN
	{'W',		WIDEN_WINDOW,	255},
	{'N',		NARROW_WINDOW,	255},
	{'K',		KILL_WINDOW,	255},
#endif
	{K_HOME,	MARK_ALL,	255},
	{K_END,		MARK_REVERSE,	255},
	{K_BEG,		CUR_TOP,	255},
	{K_EOL,		CUR_BOTTOM,	255},
	{K_HELP,	HELP_MESSAGE,	255},
	{K_CTRL('@'),	MARK_FILE3,	255},
	{K_CTRL('L'),	REREAD_DIR,	255},
	{K_CTRL('R'),	SEARCH_BACK,	255},
	{K_CTRL('S'),	SEARCH_FORW,	255},
	{-1,		NO_OPERATION,	255}
};


static VOID NEAR replacefname(name)
char *name;
{
	if (filepos < maxfile) free(filelist[filepos].name);
	else for (maxfile = 0; maxfile < filepos; maxfile++)
		if (!filelist[maxfile].name)
			filelist[maxfile].name = strdup2(nullstr);
	filelist[filepos].name = (name) ? name : strdup2(parentpath);
	filelist[filepos].tmpflags |= F_ISCHGDIR;
}

static int cur_up(arg)
char *arg;
{
	int n, old;

	old = filepos;
	if (!arg || (n = atoi2(arg)) <= 0) n = 1;
	filepos -= n;
#if	FD >= 2
	if (loopcursor) {
		int p;

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
char *arg;
{
	int n, old;

	old = filepos;
	if (!arg || (n = atoi2(arg)) <= 0) n = 1;
	filepos += n;
#if	FD >= 2
	if (loopcursor) {
		int p;

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
char *arg;
{
	int n, r, p, old;

	old = filepos;
	if (!arg || (n = atoi2(arg)) <= 0) n = 1;
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
char *arg;
{
	int n, r, old;

	old = filepos;
	if (!arg || (n = atoi2(arg)) <= 0) n = 1;
	r = FILEPERROW;
	filepos -= r * n;

#if	FD >= 2
	if (loopcursor) {
		int p;

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
char *arg;
{
	int n, p, old;

	old = filepos;
	if (!arg || (n = atoi2(arg)) <= 0) n = 1;
	p = FILEPERPAGE;
	filepos = (filepos / p + n) * p;
	if (filepos >= maxfile) {
		filepos = old;
		return(warning_bell(arg));
	}

	return(FNC_UPDATE);
}

static int roll_down(arg)
char *arg;
{
	int n, p, old;

	old = filepos;
	if (!arg || (n = atoi2(arg)) <= 0) n = 1;
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
char *arg;
{
	if (filepos == 0) return(FNC_NONE);
	filepos = 0;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int cur_bottom(arg)
char *arg;
{
	if (filepos == maxfile - 1) return(FNC_NONE);
	filepos = maxfile - 1;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int fname_right(arg)
char *arg;
{
	if (fnameofs <= 0) return(FNC_NONE);
	fnameofs--;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int fname_left(arg)
char *arg;
{
	int i, m;

#ifndef	NOSYMLINK
	if (islink(&(filelist[filepos]))) {
# ifndef	_NODOSDRIVE
		char path[MAXPATHLEN];
# endif
		char tmp[MAXPATHLEN];

# ifndef	_NOARCHIVE
		if (archivefile) {
			if (!(filelist[filepos].linkname)) i = 0;
			else {
				i = strlen(filelist[filepos].linkname);
				if (i > strsize(tmp)) i = strsize(tmp);
				strncpy(tmp, filelist[filepos].linkname, i);
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
char *arg;
{
	curcolumns = 1;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int two_columns(arg)
char *arg;
{
	curcolumns = 2;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int three_columns(arg)
char *arg;
{
	curcolumns = 3;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int five_columns(arg)
char *arg;
{
	curcolumns = 5;

	return(FNC_UPDATE);
}

static VOID NEAR markcount(VOID_A)
{
#ifndef	_NOTRADLAYOUT
	if (istradlayout()) {
		Xlocate(TC_MARK, TL_PATH);
		Xcprintf2("%<*d", TD_MARK, mark);
		Xlocate(TC_MARK + TD_MARK + TW_MARK, TL_PATH);
		Xcprintf2("%<'*qd", TD_SIZE, marksize);

		return;
	}
#endif
	Xlocate(C_MARK + W_MARK, L_STATUS);
	Xcprintf2("%<*d", D_MARK, mark);
	if (sizeinfo) {
		Xlocate(C_SIZE + W_SIZE, L_SIZE);
		Xcprintf2("%<'*qd", D_SIZE, marksize);
	}
}

/*ARGSUSED*/
static int mark_file(arg)
char *arg;
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
char *arg;
{
	mark_file(arg);
	if (filepos < maxfile - 1) filepos++;

	return(FNC_UPDATE);
}

static int mark_file3(arg)
char *arg;
{
	mark_file(arg);
	if (filepos < maxfile - 1
	&& filepos / FILEPERPAGE == (filepos + 1) / FILEPERPAGE)
		filepos++;
	else filepos = (filepos / FILEPERPAGE) * FILEPERPAGE;

	return(FNC_UPDATE);
}

static int mark_all(arg)
char *arg;
{
	int i;

	if ((arg && atoi2(arg) == 0) || (!arg && mark)) {
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
char *arg;
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
char *arg;
{
#ifndef	_NOARCHIVE
	if (archivefile) /*EMPTY*/;
	else
#endif
	if (!isdir(&(filelist[filepos]))
	|| isdotdir(filelist[filepos].name) == 2)
		return(warning_bell(arg));

	return(FNC_CHDIR);
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

	return(FNC_CHDIR);
}

/*ARGSUSED*/
static int push_file(arg)
char *arg;
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
char *arg;
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
char *arg;
{
	dispmode ^= F_SYMLINK;

	return(FNC_EFFECT);
}

/*ARGSUSED*/
static int filetype_mode(arg)
char *arg;
{
	dispmode ^= F_FILETYPE;

	return(FNC_UPDATE);
}

/*ARGSUSED*/
static int dotfile_mode(arg)
char *arg;
{
	dispmode ^= F_DOTFILE;

	return(FNC_EFFECT);
}

/*ARGSUSED*/
static int fileflg_mode(arg)
char *arg;
{
	dispmode ^= F_FILEFLAG;

	return(FNC_UPDATE);
}

static int log_dir(arg)
char *arg;
{
#ifndef	_NOARCHIVE
	char *cp, dupfullpath[MAXPATHLEN];
#endif
	char *path;

	if (arg && *arg) path = strdup2(arg);
	else if (!(path = inputstr(LOGD_K, 0, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
	else if (!*(path = evalpath(path, 0))) {
		free(path);
		return(FNC_CANCEL);
	}
#ifndef	_NOARCHIVE
	if (archivefile && *path != '/') {
		if (!(cp = archchdir(path))) {
			warning(-1, path);
			free(path);
			return(FNC_CANCEL);
		}
		free(path);
		if (cp != (char *)-1) filelist[filepos].name = cp;
		else escapearch();
		return(FNC_EFFECT);
	}
	else
#endif
	if (chdir3(path, 0) < 0) {
		warning(-1, path);
		free(path);
		return(FNC_CANCEL);
	}
	free(path);
#ifndef	_NOARCHIVE
	if (archivefile) {
		strcpy(dupfullpath, fullpath);
		while (archivefile) {
# ifdef	_NOBROWSE
			escapearch();
# else
			do {
				escapearch();
			} while (browselist);
# endif
		}
		strcpy(fullpath, dupfullpath);
	}
#endif
	replacefname(NULL);

	return(FNC_EFFECT);
}

/*ARGSUSED*/
static int log_top(arg)
char *arg;
{
	char *path;

	path = strdup2(rootpath);
	if (chdir3(path, 1) < 0) error(path);
	replacefname(path);

	return(FNC_EFFECT);
}

static VOID NEAR clearscreen(VOID_A)
{
#ifndef	_NOPTY
	int i, yy;

	if (isptymode()) {
		yy = filetop(win);
		for (i = 0; i < FILEPERROW; i++) {
			Xlocate(0, yy + i);
			Xputterm(L_CLEAR);
		}
	}
	else
#endif	/* !_NOPTY */
	Xputterms(T_CLEAR);
	Xtflush();
}

#ifndef	PAGER
static VOID NEAR dump(file)
char *file;
{
	FILE *fp;
	char *buf, *prompt;
	int i, len, min, max;

	if (!(fp = Xfopen(file, "r"))) {
		warning(-1, file);
		return;
	}
	clearscreen();

	min = 0;
	max = n_line - 1;
# ifndef	_NOPTY
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
	prompt = malloc2(i * KANAWID + strlen(buf) + 1);
	strncpy3(prompt, file, &i, 0);
	strcat(prompt, buf);

	i = min;
	while ((buf = fgets2(fp, 0))) {
		Xlocate(0, i);
		Xputterm(L_CLEAR);
		Xcprintf2("%.*k", n_column, buf);
		free(buf);
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
	Xfclose(fp);
	free(prompt);
}
#endif	/* !PAGER */

static int NEAR execenv(env, arg)
char *env, *arg;
{
	char *command;

	if (!(command = getenv2(env)) || !*command) return(0);
	clearscreen();
	ptymacro(command, arg, F_NOCONFIRM | F_IGNORELIST);

	return(1);
}

static int NEAR execshell(VOID_A)
{
#ifndef	_NOPTY
	int x, y, min, max;
#endif
	char *sh;
	int mode, ret, wastty;

	sh = getenv2("FD_SHELL");
#if	MSDOS
	if (!sh) sh = getenv2("FD_COMSPEC");
#endif
	if (!sh) {
# if	MSDOS
		sh = "COMMAND.COM";
# else
		sh = "/bin/sh";
# endif
	}
#ifndef	_NOORIGSHELL
	else if (!strpathcmp(getshellname(sh, NULL, NULL), FDSHELL)) {
		if (!fdmode) {
			warning(0, RECUR_K);
			return(-1);
		}
		sh = NULL;
	}
#endif

	sigvecset(0);
	mode = Xtermmode(0);
	wastty = isttyiomode;
	kanjiputs(SHEXT_K);
#ifndef	_NOPTY
	if (isptymode()) {
		min = filetop(win);
		max = min + FILEPERROW;
		y = max;
		if (!isttyiomode) Xttyiomode(0);
		if (getxy(&x, &y) < 0 || --y >= max) y = max - 1;
		regionscroll(C_SCROLLFORW, 1, 0, y, min, max - 1);
	}
	else
#endif	/* !_NOPTY */
	Xcputnl();
	Xtflush();
	if (isttyiomode) Xstdiomode();
	ret = ptysystem(sh);
#ifdef	_NOORIGSHELL
	checkscreen(-1, -1);
#endif
	if (wastty) Xttyiomode(wastty - 1);
	Xtermmode(mode);
	sigvecset(1);

	return(ret);
}

/*ARGSUSED*/
static int view_file(arg)
char *arg;
{
#if	!defined (_NOARCHIVE) || !defined (_NODOSDRIVE)
	char *dir;
#endif
#ifndef	_NODOSDRIVE
	int drive;
#endif

#if	!defined (_NOARCHIVE) || !defined (_NODOSDRIVE)
	dir = NULL;
#endif
#ifndef	_NODOSDRIVE
	drive = 0;
#endif
	if (isdir(&(filelist[filepos]))) return(FNC_CANCEL);
#ifndef	_NOARCHIVE
	else if (archivefile) {
		if (!(dir = tmpunpack(1))) return(FNC_CANCEL);
	}
#endif
#ifndef	_NODOSDRIVE
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

#ifndef	_NODOSDRIVE
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
char *arg;
{
#ifndef	_NODOSDRIVE
	char *dir;
	int drive;

	dir = NULL;
#endif

	if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
#ifndef	_NOARCHIVE
	if (archivefile) return(FNC_CANCEL);
#endif
#ifndef	_NODOSDRIVE
	if ((drive = tmpdosdupl(nullstr, &dir, 1)) < 0) return(FNC_CANCEL);
#endif
	if (!execenv("FD_EDITOR", filelist[filepos].name)) {
#ifdef	EDITOR
		ptymacro(EDITOR, filelist[filepos].name,
			F_NOCONFIRM | F_IGNORELIST);
#endif
	}
#ifndef	_NODOSDRIVE
	if (drive) {
		if (tmpdosrestore(drive, filelist[filepos].name) < 0)
			warning(-1, filelist[filepos].name);
		removetmp(dir, filelist[filepos].name);
	}
#endif

	return(FNC_EFFECT);
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
		i = atoi2(arg);
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

	return(FNC_UPDATE);
}

#ifndef	_NOWRITEFS
static int write_dir(arg)
char *arg;
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
char *arg;
{
#ifndef	_NODOSDRIVE
	int drive;
#endif

	checkscreen(WCOLUMNMIN, WHEADERMAX + WFOOTER + WFILEMIN);
#ifndef	_NODOSDRIVE
	if ((drive = dospath3(nullstr))) flushdrv(drive, NULL);
#endif

	return(FNC_EFFECT);
}

#ifndef	_NOCUSTOMIZE
/*ARGSUSED*/
static int edit_config(arg)
char *arg;
{
	if (FILEPERROW < WFILEMINCUSTOM) {
		warning(0, NOROW_K);
		return(FNC_CANCEL);
	}
	customize();

	return(FNC_EFFECT);
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

	return(FNC_UPDATE);
}

#ifndef	_NOPTY
static int confirmpty(VOID_A)
{
	int i;

	VOID_C checkallpty();
	for (i = 0; i < MAXWINDOWS; i++) if (ptylist[i].pid) break;
	if (i < MAXWINDOWS && !yesno(KILL_K)) return(1);

	return(0);
}
#endif	/* !_NOPTY */

/*ARGSUSED*/
static int quit_system(arg)
char *arg;
{
	int n;

#ifndef	_NOARCHIVE
	if (archivefile) return(FNC_QUIT);
#endif
	n = 0;

#ifndef	_NOORIGSHELL
	if (!fdmode) {
		char *str[3];
		int val[3];

		Xlocate(0, L_MESLINE);
		Xputterm(L_CLEAR);
		Xkanjiputs(QUIT_K);
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
#endif	/* _NOORIGSHELL */
	if (!yesno(QUIT_K)) return(FNC_CANCEL);

#ifndef	_NOPTY
	if (confirmpty()) return(FNC_CANCEL);
#endif

	return((n) ? FNC_FAIL : FNC_QUIT);
}

static int make_dir(arg)
char *arg;
{
	char *path;

	if (arg && *arg) path = strdup2(arg);
	else if (!(path = inputstr(MAKED_K, 1, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
	else if (!*(path = evalpath(path, 0))) {
		free(path);
		return(FNC_CANCEL);
	}

	if (mkdir2(path, 0777) < 0) warning(-1, path);
	free(path);

	return(FNC_EFFECT);
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
#ifdef	_USEDOSEMU
	char path[MAXPATHLEN];
#endif
	char *file;

	if (isdotdir(filelist[filepos].name)) return(warning_bell(arg));
	if (arg && *arg) {
		file = strdup2(arg);
		errno = EEXIST;
		if (Xaccess(file, F_OK) >= 0 || errno != ENOENT) {
			warning(-1, file);
			free(file);
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
		free(file);
	}

	if (Xrename(fnodospath(path, filepos), file) < 0) {
		warning(-1, file);
		free(file);
		return(FNC_CANCEL);
	}
	if (strdelim(file, 0)) {
		free(file);
		file = strdup2(parentpath);
	}
	replacefname(file);

	return(FNC_EFFECT);
}

static int delete_file(arg)
char *arg;
{
	char *cp;
	int len;

	removepolicy = 0;
	if (mark > 0) {
		if (!yesno(DELMK_K)) return(FNC_CANCEL);
		filepos = applyfile(rmvfile, NULL);
	}
	else if (isdir(&(filelist[filepos]))) return(warning_bell(arg));
	else {
		cp = DELFL_K;
		len = strlen2(cp) - strsize("%.*s");
		if (!yesno(cp, n_lastcolumn - len, filelist[filepos].name))
			return(FNC_CANCEL);
		filepos = applyfile(rmvfile, NULL);
	}
	if (filepos >= maxfile && (filepos -= 2) < 0) filepos = 0;

	return(FNC_EFFECT);
}

static int delete_dir(arg)
char *arg;
{
	char *cp;
	int len;

	if (!isdir(&(filelist[filepos])) || isdotdir(filelist[filepos].name))
		return(warning_bell(arg));
	cp = DELDR_K;
	len = strlen2(cp) - strsize("%.*s");
	if (!yesno(cp, n_lastcolumn - len, filelist[filepos].name))
		return(FNC_CANCEL);
	removepolicy = 0;
	cp = filelist[filepos].name;
#ifndef	NOSYMLINK
	if (islink(&(filelist[filepos]))) {
# ifndef	_NODOSDRIVE
		char path[MAXPATHLEN];
# endif
		int ret;

		ret = rmvfile(fnodospath(path, filepos));
		if (ret < 0) warning(-1, cp);
		else if (!ret) filepos++;
	}
	else
#endif
	if (!applydir(cp, rmvfile, NULL, rmvdir, ORD_NOPREDIR, NULL))
		filepos++;
	if (filepos >= maxfile && (filepos -= 2) < 0) filepos = 0;

	return(FNC_EFFECT);
}

static int find_file(arg)
char *arg;
{
	char *wild;

	if (arg && *arg) wild = strdup2(arg);
	else if (!(wild = inputstr(FINDF_K, 0, 0, "*", -1)))
		return(FNC_CANCEL);

#ifndef	_NOARCHIVE
	if (*wild == '/') /*EMPTY*/;
	else
#endif
	if (strdelim(wild, 1)) {
		warning(ENOENT, wild);
		free(wild);
		return(FNC_CANCEL);
	}
	if (findpattern) free(findpattern);
	if (*wild) findpattern = wild;
	else {
		free(wild);
		findpattern = NULL;
	}

	return(FNC_EFFECT);
}

static int find_dir(arg)
char *arg;
{
	char *cp;

	if (!(findregexp = prepareregexp(FINDD_K, arg))) return(FNC_CANCEL);
	destpath = NULL;
	cp = isdir(&(filelist[filepos])) ? filelist[filepos].name : curpath;
	applydir(cp, findfile, finddir, NULL, ORD_NORMAL, NOFND_K);
	regexp_free(findregexp);
	if (!destpath) return(FNC_CANCEL);

	if (!(cp = strrdelim(destpath, 0))) cp = destpath;
	else {
		*(cp++) = '\0';
		chdir3(destpath, 1);
	}

	replacefname(strdup2(cp));
	free(destpath);

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
char *arg;
{
	char *com;
	int ret;

	if (arg) com = strdup2(arg);
	else {
		Xttyiomode(1);
		com = inputshellloop(-1, NULL);
		Xttyiomode(0);
		if (!com) return(FNC_CANCEL);
	}
	if (*com) {
		ret = ptyusercomm(com, filelist[filepos].name, F_ARGSET);
		ret = evalstatus(ret);
	}
	else {
		execshell();
		ret = FNC_EFFECT;
	}
	free(com);

	return(ret);
}

/*ARGSUSED*/
static int execute_file(arg)
char *arg;
{
#if	!defined (_NOARCHIVE) || !defined (_NODOSDRIVE)
	char *dir;
#endif
#ifndef	CWDINPATH
	char *tmp;
#endif
#ifndef	_NODOSDRIVE
	int drive;
#endif
	char *com;
	int ret, len;

#if	!defined (_NOARCHIVE) && !defined (_NOBROWSE)
	if (browselist) return(warning_bell(arg));
#endif

#if	!defined (_NOARCHIVE) || !defined (_NODOSDRIVE)
	dir = NULL;
#endif
#ifndef	_NODOSDRIVE
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
		tmp = malloc2(len + 1);
		tmp[0] = '.';
		tmp[1] = _SC_;
		strcpy(&(tmp[2]), filelist[filepos].name);
	}
	Xttyiomode(1);
	com = inputshellloop(len, tmp);
	Xttyiomode(0);
	if (tmp != filelist[filepos].name) free(tmp);
#endif
	if (!com) return(FNC_CANCEL);
	if (!*com) {
		execshell();
		free(com);
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
#ifndef	_NODOSDRIVE
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
	free(com);

	return(ret);
}

#ifndef	_NOARCHIVE
static int launch_file(arg)
char *arg;
{
	if (!launcher()) return(view_file(arg));

	return(FNC_EFFECT);
}

static int pack_file(arg)
char *arg;
{
	char *file;
	int ret;

	if (arg && *arg) file = strdup2(arg);
	else if (!(file = inputstr(PACK_K, 1, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
	else if (!*(file = evalpath(file, 0))) {
		free(file);
		return(FNC_CANCEL);
	}

	ret = pack(file);
	free(file);
	if (ret < 0) {
		Xputterm(T_BELL);
		return(FNC_CANCEL);
	}
	if (!ret) return(FNC_CANCEL);

	return(FNC_EFFECT);
}

static int unpack_file(arg)
char *arg;
{
#ifdef	_USEDOSEMU
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
char *arg;
{
#ifdef	_USEDOSEMU
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
char *arg;
{
	char *path;
	int ret;

	if (arg && *arg) path = strdup2(arg);
	else if (!(path = inputstr(FSDIR_K, 0, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
	else if (!*(path = evalpath(path, 0))) {
		free(path);
		path = strdup2(curpath);
	}

	ret = infofs(path);
	free(path);
	if (!ret) return(FNC_CANCEL);

	return(FNC_HELPSPOT);
}

static int NEAR selectattr(s)
char *s;
{
	char *str[MAXATTRSEL];
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
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
	Xputterm(T_STANDOUT);
#endif
	Xkanjiputs(s);
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
	Xputterm(END_STANDOUT);
#endif
	if (selectstr(&n, MAXATTRSEL, ATTR_X, str, val) != K_CR) return(-1);

	return(n);
}

/*ARGSUSED*/
static int attr_file(arg)
char *arg;
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
char *arg;
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
	applydir(filelist[filepos].name, setattr,
		NULL, setattr, ORD_NOPREDIR, NULL);

	return(FNC_EFFECT);
}
#endif	/* !_NOEXTRAATTR */

#ifndef	_NOTREE
/*ARGSUSED*/
static int tree_dir(arg)
char *arg;
{
	char *path;

	if (!(path = tree(0, NULL))) return(FNC_UPDATE);
	if (chdir3(path, 1) < 0) {
		warning(-1, path);
		free(path);
		return(FNC_UPDATE);
	}
	free(path);
	if (findpattern) {
		free(findpattern);
		findpattern = NULL;
	}
	replacefname(NULL);

	return(FNC_EFFECT);
}
#endif	/* !_NOTREE */

#ifndef	_NOARCHIVE
static int backup_tape(arg)
char *arg;
{
	char *dir, *dev;
#ifndef	_NODOSDRIVE
	int drive;
#endif
	int ret;

#if	!defined (_NOARCHIVE) && !defined (_NOBROWSE)
	if (browselist) return(warning_bell(arg));
#endif

	dir = NULL;
#ifndef	_NODOSDRIVE
	drive = 0;
#endif
	if (arg && *arg) dev = strdup2(arg);
#if	MSDOS
	else if (!(dev = inputstr(BKUP_K, 1, -1, NULL, HST_PATH)))
		return(FNC_CANCEL);
#else
	else if (!(dev = inputstr(BKUP_K, 1, 5, "/dev/", HST_PATH)))
		return(FNC_CANCEL);
#endif
	else if (!*(dev = evalpath(dev, 0))) {
		free(dev);
		return(FNC_CANCEL);
	}
	else if (archivefile) {
		if (!(dir = tmpunpack(0))) {
			free(dev);
			return(FNC_CANCEL);
		}
	}
#ifndef	_NODOSDRIVE
	else if ((drive = tmpdosdupl(nullstr, &dir, 0)) < 0) {
		free(dev);
		return(FNC_CANCEL);
	}
#endif

	ret = backup(dev);
#ifndef	_NODOSDRIVE
	if (drive) removetmp(dir, NULL);
	else
#endif
	if (archivefile) removetmp(dir, NULL);

	free(dev);
	if (ret <= 0) return(FNC_CANCEL);

	return(FNC_EFFECT);
}
#endif	/* !_NOARCHIVE */

/*ARGSUSED*/
static int search_forw(arg)
char *arg;
{
	isearch = 1;
	Xlocate(0, L_HELP);
	Xputterm(L_CLEAR);
	Xputterm(T_STANDOUT);
	win_x = Xkanjiputs(SEAF_K);
	win_y = L_HELP;
	Xputterm(END_STANDOUT);

	return(FNC_NONE);
}

/*ARGSUSED*/
static int search_back(arg)
char *arg;
{
	isearch = -1;
	Xlocate(0, L_HELP);
	Xputterm(L_CLEAR);
	Xputterm(T_STANDOUT);
	win_x = Xkanjiputs(SEAB_K);
	win_y = L_HELP;
	Xputterm(END_STANDOUT);

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
	addlist();
	memcpy((char *)filelist, (char *)(winvar[oldwin].v_filelist), i);
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
# ifndef	_NOARCHIVE
	if (winvar[oldwin].v_archivedir) free(winvar[oldwin].v_archivedir);
	winvar[oldwin].v_archivedir = strdup2(archivedir);
	if (winvar[win].v_archivedir)
		strcpy(archivedir, winvar[win].v_archivedir);
# endif
}

int nextwin(VOID_A)
{
	int oldwin;

	if (windows <= 1) return(-1);
	oldwin = win;
	if (++win >= windows) win = 0;
	duplwin(oldwin);
	movewin(oldwin);
	if (chdir3(fullpath, 1) < 0) lostcwd(fullpath);

	return(0);
}

/*ARGSUSED*/
static int split_window(arg)
char *arg;
{
	winvartable tmp;
	int i, oldwin;

	oldwin = win;
	if (windows >= MAXWINDOWS) {
# ifdef	_NOEXTRAWIN
#  ifndef	_NOPTY
		if (confirmpty()) return(FNC_CANCEL);
#  endif
		if (win > 0) {
			shutwin(0);
			win = 0;
			duplwin(oldwin);
		}
		for (i = 1; i < windows; i++) shutwin(i);
#  ifndef	_NOPTY
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
	else if (winvar[win].v_fileperrow < WFILEMIN * 2 + 1)
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
		winvar[win].v_fileperrow =
			(winvar[oldwin].v_fileperrow - 1) - i;
		winvar[oldwin].v_fileperrow = i;
# endif

# ifndef	_NOPTY
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
char *arg;
{
	if (nextwin() < 0) return(FNC_NONE);
# ifndef	_NOPTY
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
char *arg;
{
	int n, next;

	if (windows <= 1) return(FNC_NONE);
	if (!arg || (n = atoi2(arg)) <= 0) n = 1;
	if ((next = win + 1) >= windows) next = 0;
	if (winvar[next].v_fileperrow - n < WFILEMIN)
		n = winvar[next].v_fileperrow - WFILEMIN;
	if (n <= 0) return(warning_bell(arg));

	winvar[next].v_fileperrow -= n;
	winvar[win].v_fileperrow += n;
# ifndef	_NOPTY
	changewsize(wheader, windows);
# endif

	return(FNC_EFFECT);
}

static int narrow_window(arg)
char *arg;
{
	int n, next;

	if (windows <= 1) return(FNC_NONE);
	if (!arg || (n = atoi2(arg)) <= 0) n = 1;
	if ((next = win + 1) >= windows) next = 0;
	if (FILEPERROW - n < WFILEMIN) n = FILEPERROW - WFILEMIN;
	if (n <= 0) return(warning_bell(arg));

	winvar[next].v_fileperrow += n;
	winvar[win].v_fileperrow -= n;
# ifndef	_NOPTY
	changewsize(wheader, windows);
# endif

	return(FNC_EFFECT);
}

/*ARGSUSED*/
static int kill_window(arg)
char *arg;
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

# ifndef	_NOPTY
	deletewin(win, windows);
	changewsize(wheader, windows);
# endif

	win = prev;
	duplwin(windows);
	movewin(windows);
	shutwin(windows);
	if (chdir3(fullpath, 1) < 0) lostcwd(fullpath);

# ifndef	_NOPTY
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
char *arg;
{
	Xputterm(T_BELL);

	return(FNC_NONE);
}

/*ARGSUSED*/
static int no_operation(arg)
char *arg;
{
	return(FNC_NONE);
}
