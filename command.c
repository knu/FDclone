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

#include <time.h>
#include <sys/file.h>
#include <sys/stat.h>

extern int columns;
extern int filepos;
extern int mark;
extern int fnameofs;
extern int dispmode;
extern int sorton;
extern int chgorder;
extern int stackdepth;
extern namelist filestack[];
extern char fullpath[];
extern char *archivefile;
extern char *destpath;
extern int writefs;

#ifndef	PAGER
static VOID dump();
#endif
static int cur_up();
static int cur_down();
static int cur_right();
static int cur_left();
static int roll_up();
static int roll_down();
static int cur_top();
static int cur_bottom();
static int in_dir();
static int out_dir();
static int one_column();
static int two_columns();
static int three_columns();
static int five_columns();
static int fname_right();
static int fname_left();
static int mark_file();
static int mark_file2();
static int mark_all();
static int mark_find();
static int push_file();
static int pop_file();
static int symlink_mode();
static int filetype_mode();
static int dotfile_mode();
static int log_dir();
static int log_top();
static int view_file();
static int edit_file();
static int sort_dir();
static int write_dir();
static int reread_dir();
static int help_message();
static int quit_system();
static int make_dir();
static int copy_file();
static int copy_tree();
static int move_file();
static int move_tree();
static int rename_file();
static int delete_file();
static int delete_dir();
static int find_file();
static int find_dir();
static int execute_sh();
static int execute_file();
static int launch_file();
static int pack_file();
static int unpack_file();
static int unpack_tree();
static int info_filesys();
static int attr_file();
static int tree_dir();
static int backup_tape();
static int warning_bell();
static int no_operation();

#include "functable.h"

char *findpattern = NULL;
reg_t *findregexp = NULL;
char **sh_history = NULL;
char **path_history = NULL;
bindtable bindlist[MAXBINDTABLE] = {
	{K_UP,		CUR_UP,		-1},
	{K_DOWN,	CUR_DOWN,	-1},
	{K_RIGHT,	CUR_RIGHT,	-1},
	{K_LEFT,	CUR_LEFT,	-1},
	{K_NPAGE,	ROLL_UP,	-1},
	{K_PPAGE,	ROLL_DOWN,	-1},
	{K_F(1),	LOG_DIR,	-1},
	{K_F(2),	EXECUTE_FILE,	-1},
	{K_F(3),	COPY_FILE,	-1},
	{K_F(4),	DELETE_FILE,	-1},
	{K_F(5),	RENAME_FILE,	-1},
	{K_F(6),	SORT_DIR,	-1},
	{K_F(7),	FIND_FILE,	-1},
	{K_F(8),	TREE_DIR,	-1},
	{K_F(9),	EDIT_FILE,	-1},
	{K_F(10),	UNPACK_FILE,	-1},
	{K_F(11),	ATTR_FILE,	-1},
	{K_F(12),	INFO_FILESYS,	-1},
	{K_F(13),	MOVE_FILE,	-1},
	{K_F(14),	DELETE_DIR,	-1},
	{K_F(15),	MAKE_DIR,	-1},
	{K_F(16),	EXECUTE_SH,	-1},
	{K_F(17),	WRITE_DIR,	-1},
	{K_F(18),	BACKUP_TAPE,	-1},
	{K_F(19),	VIEW_FILE,	-1},
	{K_F(20),	PACK_FILE,	-1},
	{CR,		LAUNCH_FILE,	IN_DIR},
	{K_BS,		OUT_DIR,	-1},
	{K_DC,		PUSH_FILE,	-1},
	{K_IC,		POP_FILE,	-1},
	{'\t',		MARK_FILE,	-1},
	{ESC,		QUIT_SYSTEM,	-1},

	{'<',		CUR_TOP,	-1},
	{'>',		CUR_BOTTOM,	-1},
	{'1',		ONE_COLUMN,	-1},
	{'2',		TWO_COLUMNS,	-1},
	{'3',		THREE_COLUMNS,	-1},
	{'5',		FIVE_COLUMNS,	-1},
	{'(',		FNAME_RIGHT,	-1},
	{')',		FNAME_LEFT,	-1},
	{' ',		MARK_FILE2,	-1},
	{'+',		MARK_ALL,	-1},
	{'*',		MARK_FIND,	-1},
	{']',		PUSH_FILE,	-1},
	{'[',		POP_FILE,	-1},
	{'?',		HELP_MESSAGE,	-1},

	{'a',		ATTR_FILE,	-1},
	{'b',		BACKUP_TAPE,	-1},
	{'c',		COPY_FILE,	-1},
	{'d',		DELETE_FILE,	-1},
	{'e',		EDIT_FILE,	-1},
	{'f',		FIND_FILE,	-1},
	{'h',		EXECUTE_SH,	-1},
	{'i',		INFO_FILESYS,	-1},
	{'k',		MAKE_DIR,	-1},
	{'l',		LOG_DIR,	-1},
	{'\\',		LOG_TOP,	-1},
	{'m',		MOVE_FILE,	-1},
	{'p',		PACK_FILE,	-1},
	{'q',		QUIT_SYSTEM,	-1},
	{'r',		RENAME_FILE,	-1},
	{'s',		SORT_DIR,	-1},
	{'t',		TREE_DIR,	-1},
	{'u',		UNPACK_FILE,	-1},
	{'v',		VIEW_FILE,	-1},
	{'w',		WRITE_DIR,	-1},
	{'x',		EXECUTE_FILE,	-1},
	{'C',		COPY_TREE,	-1},
	{'D',		DELETE_DIR,	-1},
	{'F',		FIND_DIR,	-1},
	{'H',		DOTFILE_MODE,	-1},
	{'L',		LOG_TREE,	-1},
	{'M',		MOVE_TREE,	-1},
	{'Q',		QUIT_SYSTEM,	-1},
	{'S',		SYMLINK_MODE,	-1},
	{'T',		FILETYPE_MODE,	-1},
	{'U',		UNPACK_TREE,	-1},
	{CTRL_A,	CUR_TOP,	-1},
	{CTRL_B,	CUR_LEFT,	-1},
	{CTRL_C,	ROLL_UP,	-1},
	{CTRL_E,	CUR_BOTTOM,	-1},
	{CTRL_F,	CUR_RIGHT,	-1},
	{CTRL_L,	REREAD_DIR,	-1},
	{CTRL_N,	CUR_DOWN,	-1},
	{CTRL_P,	CUR_UP,		-1},
	{CTRL_R,	ROLL_DOWN,	-1},
	{CTRL_V,	ROLL_UP,	-1},
	{CTRL_Y,	ROLL_DOWN,	-1},
	{-1,		NO_OPERATION,	-1}
};


static int cur_up(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos <= 0) return(0);
	filepos--;
	return(2);
}

static int cur_down(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos >= *maxp - 1) return(0);
	filepos++;
	return(2);
}

static int cur_right(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos + FILEPERLOW >= *maxp) {
		if (filepos / FILEPERPAGE == (*maxp - 1) / FILEPERPAGE)
			return(0);
		else filepos = *maxp - 1;
	}
	else filepos += FILEPERLOW;
	return(2);
}

static int cur_left(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos - FILEPERLOW < 0) return(0);
	else filepos -= FILEPERLOW;
	return(2);
}

static int roll_up(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	i = (filepos / FILEPERPAGE) * FILEPERPAGE;
	if (i + FILEPERPAGE >= *maxp) return(warning_bell(list, maxp));
	filepos = i + FILEPERPAGE;
	return(2);
}

static int roll_down(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	i = (filepos / FILEPERPAGE) * FILEPERPAGE;
	if (i - FILEPERPAGE < 0) return(warning_bell(list, maxp));
	filepos = i - FILEPERPAGE;
	return(2);
}

static int cur_top(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos == 0) return(0);
	filepos = 0;
	return(2);
}

static int cur_bottom(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos == *maxp - 1) return(0);
	filepos = *maxp - 1;
	return(2);
}

static int in_dir(list, maxp)
namelist *list;
int *maxp;
{
	if (!archivefile && !strcmp(list[filepos].name, "."))
		return(warning_bell(list, maxp));
	return(5);
}

static int out_dir(list, maxp)
namelist *list;
int *maxp;
{
	free(list[filepos].name);
	list[filepos].name = strdup2("..");
	return(5);
}

static int one_column(list, maxp)
namelist *list;
int *maxp;
{
	columns = 1;
	return(2);
}

static int two_columns(list, maxp)
namelist *list;
int *maxp;
{
	columns = 2;
	return(2);
}

static int three_columns(list, maxp)
namelist *list;
int *maxp;
{
	columns = 3;
	return(2);
}

static int five_columns(list, maxp)
namelist *list;
int *maxp;
{
	columns = 5;
	return(2);
}

static int fname_right(list, maxp)
namelist *list;
int *maxp;
{
	if (fnameofs <= 0) return(0);
	fnameofs--;
	return(2);
}

static int fname_left(list, maxp)
namelist *list;
int *maxp;
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
		|| access(list[filepos].name, X_OK) >= 0)) i--;
	}
	if (fnameofs >= strlen(list[filepos].name) - i) return(0);
	fnameofs++;
	return(2);
}

static int mark_file(list, maxp)
namelist *list;
int *maxp;
{
	if (isdir(&list[filepos])) return(0);
	list[filepos].flags ^= F_ISMRK;
	if (ismark(&list[filepos])) mark++;
	else mark--;
	locate(17, LSTATUS);
	cprintf("%4d", mark);
	return(2);
}

static int mark_file2(list, maxp)
namelist *list;
int *maxp;
{
	mark_file(list, maxp);
	if (filepos < *maxp - 1) filepos++;
	return(2);
}

static int mark_all(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	if (mark) {
		for (i = 0; i < *maxp; i++) list[i].flags &= ~F_ISMRK;
		mark = 0;
	}
	else {
		mark = 0;
		for (i = 0; i < *maxp; i++) if (!isdir(&list[i])) {
			list[i].flags |= F_ISMRK;
			mark++;
		}
	}
	locate(17, LSTATUS);
	cprintf("%4d", mark);
	return(2);
}

static int mark_find(list, maxp)
namelist *list;
int *maxp;
{
	reg_t *re;
	char *cp, *wild;
	int i;

	if (!(wild = inputstr2(FINDF_K, 0, "*", NULL))) return(1);
	if (!*wild || strchr(wild, '/')) {
		warning(ENOENT, wild);
		free(wild);
		return(1);
	}

	cp = cnvregexp(wild);
	re = regexp_init(cp);
	free(wild);
	free(cp);
	for (i = 0; i < *maxp; i++)
		if (!isdir(&list[i]) && regexp_exec(re, list[i].name)) {
			list[i].flags |= F_ISMRK;
			mark++;
		}
	regexp_free(re);
	locate(17, LSTATUS);
	cprintf("%4d", mark);
	return(3);
}

static int push_file(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	if (stackdepth >= MAXSTACK
	|| !strcmp(list[filepos].name, ".")
	|| !strcmp(list[filepos].name, "..")) return(0);
	memcpy(&filestack[stackdepth++], &list[filepos], sizeof(namelist));
	(*maxp)--;
	for (i = filepos; i < *maxp; i++)
		memcpy(&list[i], &list[i + 1], sizeof(namelist));
	if (filepos >= *maxp) filepos = *maxp - 1;
	return(2);
}

static int pop_file(list, maxp)
namelist *list;
int *maxp;
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

static int symlink_mode(list, maxp)
namelist *list;
int *maxp;
{
	dispmode ^= F_SYMLINK;
	return(4);
}

static int filetype_mode(list, maxp)
namelist *list;
int *maxp;
{
	dispmode ^= F_FILETYPE;
	return(3);
}

static int dotfile_mode(list, maxp)
namelist *list;
int *maxp;
{
	dispmode ^= F_DOTFILE;
	return(4);
}

static int log_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *path;

	if (!(path = inputstr2(LOGD_K, -1, NULL, path_history))) return(1);
	path_history = entryhist(path_history, path);
	path = evalpath(path);
	if (!strcmp(path, "?")) {
		free(list[filepos].name);
		list[filepos].name = path;
		return(5);
	}
	if (*path && chdir2(path) < 0) {
		warning(-1, path);
		free(path);
		return(1);
	}
	if (!strcmp(path, ".")) {
		free(path);
		path = getwd2();
		strcpy(fullpath, path);
	}
	else {
		if (findpattern) free(findpattern);
		findpattern = NULL;
	}
	free(path);
	free(list[filepos].name);
	list[filepos].name = strdup2("..");
	return(4);
}

static int log_top(list, maxp)
namelist *list;
int *maxp;
{
	if (chdir2("/") < 0) error("/");
	if (findpattern) free(findpattern);
	findpattern = NULL;
	free(list[filepos].name);
	list[filepos].name = strdup2("..");
	return(4);
}

#ifndef	PAGER
static VOID dump(file)
char *file;
{
	FILE *fp;
	char *buf, *prompt;
	int i;

	if (!(fp = fopen(file, "r"))) {
		warning(-1, file);
		return;
	}
	putterms(t_clear);
	tflush();
	buf = NEXT_K;
	i = strlen(file);
	if (i + strlen(buf) > n_lastcolumn) i = n_lastcolumn - strlen(buf);
	prompt = (char *)malloc2(i + strlen(buf) + 1);
	strncpy3(prompt, file, i, 0);
	strcat(prompt, buf);
	buf = (char *)malloc2(n_column + 2);

	i = 0;
	while (fgets(buf, n_column + 1, fp)) {
		locate(0, i);
		putterm(l_clear);
		kanjiputs(buf);
		if (++i >= n_line - 1) {
			i = 0;
			if (!yesno(prompt)) break;
		}
	}

	if (feof(fp)) {
		for (; i < n_line + 1; i++) {
			locate(0, i);
			putterm(l_clear);
		}
	}
	fclose(fp);
	free(buf);
	free(prompt);
}
#endif

static int view_file(list, maxp)
namelist *list;
int *maxp;
{
	char *dir;

	if (isdir(&list[filepos])
	|| (archivefile && !(dir = tmpunpack(list, *maxp)))) return(0);
	if (!execenv("FD_PAGER", list[filepos].name)) {
#ifdef	PAGER
		execmacro(PAGER, list[filepos].name, NULL, 0, 0, 0);
#else
		do {
			dump(list[filepos].name);
		} while(!yesno(PEND_K));
#endif
	}
	if (archivefile) removetmp(dir, list[filepos].name);
	return(2);
}

static int edit_file(list, maxp)
namelist *list;
int *maxp;
{
	if (isdir(&list[filepos])) return(warning_bell(list, maxp));
	if (!execenv("FD_EDITOR", list[filepos].name)) {
#ifdef	EDITOR
		execmacro(EDITOR, list[filepos].name, NULL, 0, 0, 0);
#endif
	}
	return(4);
}

static int sort_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *str[5];
	int i, tmp1, tmp2, val[5], *dup;

	str[0] = ONAME_K;
	str[1] = OEXT_K;
	str[2] = OSIZE_K;
	str[3] = ODATE_K;
	str[4] = ORAW_K;
	val[0] = 1;
	val[1] = 2;
	val[2] = 3;
	val[3] = 4;
	val[4] = 0;
	tmp1 = sorton & 7;
	tmp2 = sorton & ~7;
	i = (tmp1) ? 5 : 4;
	if (!tmp1) tmp1 = val[0];
	if (selectstr(&tmp1, i, 0, str, val) == ESC) return(1);

	if (!tmp1) {
		sorton = 0;
		qsort(list, *maxp, sizeof(namelist), cmplist);
	}
	else {
		str[0] = OINC_K;
		str[1] = ODEC_K;
		val[0] = 0;
		val[1] = 8;
		if (selectstr(&tmp2, 2, 48, str, val) == ESC)
			return(1);
		sorton = tmp1 + tmp2;
		dup = (int *)malloc(*maxp * sizeof(int));
		for (i = 0; i < *maxp; i++) {
			dup[i] = list[i].ent;
			list[i].ent = i;
		}
		qsort(list, *maxp, sizeof(namelist), cmplist);
		for (i = 0; i < *maxp; i++) {
			tmp1 = list[i].ent;
			list[i].ent = dup[tmp1];
		}
		free(dup);
		chgorder = 1;
	}
	return(3);
}

static int write_dir(list, maxp)
namelist *list;
int *maxp;
{
#if (WRITEFS >= 2)
	return(warning_bell(list, maxp));
#else
	int i;

	if (writefs >= 2 || findpattern) return(warning_bell(list, maxp));
	if ((i = writablefs(".")) <= 0) {
		warning(0, NOWRT_K);
		return(1);
	}
	if (!yesno(WRTOK_K)) return(1);
	if (underhome() <= 0 && !yesno(HOMOK_K)) return(1);
	arrangedir(list, *maxp, i);
	chgorder = 0;
	return(4);
#endif
}

static int reread_dir(list, maxp)
namelist *list;
int *maxp;
{
	getwsize(80, WHEADER + WFOOTER + 2);
	return(4);
}

static int help_message(list, maxp)
namelist *list;
int *maxp;
{
	help(archivefile != NULL);
	return(3);
}

static int quit_system(list, maxp)
namelist *list;
int *maxp;
{
	if (!archivefile && !yesno(QUIT_K)) return(1);
	return(-1);
}

static int make_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *path;

	if (!(path = evalpath(inputstr2(MAKED_K, -1, NULL, NULL)))) return(1);
	if (*path && mkdir2(path, 0777) < 0) warning(-1, path);
	free(path);
	return(4);
}

static int copy_file(list, maxp)
namelist *list;
int *maxp;
{
	return(copyfile(list, *maxp, 0));
}

static int copy_tree(list, maxp)
namelist *list;
int *maxp;
{
	return(copyfile(list, *maxp, 1));
}

static int move_file(list, maxp)
namelist *list;
int *maxp;
{
	return(movefile(list, *maxp, 0));
}

static int move_tree(list, maxp)
namelist *list;
int *maxp;
{
	return(movefile(list, *maxp, 1));
}

static int rename_file(list, maxp)
namelist *list;
int *maxp;
{
	struct stat status;
	char *file;

	if (!strcmp(list[filepos].name, ".")
	|| !strcmp(list[filepos].name, "..")) return(warning_bell(list, maxp));
	for (;;) {
		if (!(file = evalpath(inputstr2(NEWNM_K, 0,
			list[filepos].name, NULL)))) return(1);
		if (lstat(file, &status) < 0) {
			if (errno == ENOENT) break;
			warning(-1, file);
		}
		else warning(0, WRONG_K);
		free(file);
	}
	if (rename(list[filepos].name, file) < 0) {
		warning(-1, file);
		free(file);
		return(1);
	}
	free(list[filepos].name);
	if (!strchr(file, '/')) list[filepos].name = file;
	else {
		free(file);
		list[filepos].name = strdup2("..");
	}
	return(4);
}

static int delete_file(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	if (mark > 0) {
		if (!yesno(DELMK_K)) return(1);
		filepos = applyfile(list, *maxp, unlink2, NULL);
	}
	else if (isdir(&list[filepos])) return(warning_bell(list, maxp));
	else {
		if (!yesno(DELFL_K, list[filepos].name)) return(1);
		if ((i = unlink2(list[filepos].name)) < 0)
			warning(-1, list[filepos].name);
		if (!i) filepos++;
	}
	if (filepos >= *maxp && (filepos -= 2) < 0) filepos = 0;
	return(4);
}

static int delete_dir(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	if (!isdir(&list[filepos])
	|| !strcmp(list[filepos].name, ".")
	|| !strcmp(list[filepos].name, "..")) return(warning_bell(list, maxp));
	if (!yesno(DELDR_K, list[filepos].name)) return(1);
	if (islink(&list[filepos])) {
		if ((i = unlink2(list[filepos].name)) < 0)
			warning(-1, list[filepos].name);
		if (!i) filepos++;
	}
	else if (!applydir(list[filepos].name, unlink2, NULL, rmdir2, NULL))
		filepos++;
	if (filepos >= *maxp) filepos -= 2;
	return(4);
}

static int find_file(list, maxp)
namelist *list;
int *maxp;
{
	char *wild;

	if (!(wild = inputstr2(FINDF_K, 0, "*", NULL))) return(1);
	if (strchr(wild, '/')) {
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

static int find_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *cp, *wild;

	if (!(wild = inputstr2(FINDD_K, 0, "*", NULL))) return(1);
	if (!*wild || strchr(wild, '/')) {
		warning(ENOENT, wild);
		free(wild);
		return(1);
	}
	destpath = NULL;
	cp = cnvregexp(wild);
	findregexp = regexp_init(cp);
	free(wild);
	free(cp);
	cp = isdir(&list[filepos]) ? list[filepos].name : ".";
	applydir(cp, findfile, finddir, NULL, NOFND_K);
	regexp_free(findregexp);
	if (!destpath) return(1);

	if (cp = strrchr(destpath, '/')) {
		*(cp++) = '\0';
		chdir2(destpath);
	}
	else cp = destpath;

	free(list[filepos].name);
	list[filepos].name = strdup2(cp);
	free(destpath);
	return(4);
}

static int execute_sh(list, maxp)
namelist *list;
int *maxp;
{
	char *com;

	if (!(com = inputstr2("sh#", -1, NULL, sh_history))) return(1);
	sh_history = entryhist(sh_history, com);
	if (!*com) {
		free(com);
		if (!(com = getenv2("FD_SHELL"))) com = "/bin/sh";
		putterms(t_end);
		putterms(t_nokeypad);
		tflush();
		sigvecreset();
		cooked2();
		echo2();
		nl2();
		kanjiputs(SHEXT_K);
		system(com);
		raw2();
		noecho2();
		nonl2();
		sigvecset();
		putterms(t_keypad);
		putterms(t_init);
	}
	else {
		if (*com == '!') execinternal(com + 1);
		else execmacro(com, list[filepos].name, list, *maxp, 0, 1);
		free(com);
	}
	return(4);
}

static int execute_file(list, maxp)
namelist *list;
int *maxp;
{
	char *com, *dir;
	int len;

	len = (access(list[filepos].name, X_OK) >= 0) ?
		strlen(list[filepos].name) + 1 : 0;
	if (!(com = inputstr2("sh#", len, list[filepos].name, sh_history)))
		return(1);
	if (!*com) {
		free(com);
		return(1);
	}
	sh_history = entryhist(sh_history, com);
	if (archivefile && !(dir = tmpunpack(list, *maxp))) {
		free(com);
		return(1);
	}
	if (!archivefile) execmacro(com, list[filepos].name, list, *maxp, 0, 1);
	else {
		execmacro(com, list[filepos].name, NULL, 0, 0, 1);
		removetmp(dir, list[filepos].name);
	}
	free(com);
	return(4);
}

static int launch_file(list, maxp)
namelist *list;
int *maxp;
{
	char *path;

	if (launcher(list, *maxp) < 0) return(view_file(list, maxp));
	return(4);
}

static int pack_file(list, maxp)
namelist *list;
int *maxp;
{
	char *file;
	int i;

	if (!(file = inputstr2(PACK_K, -1, NULL, path_history))) return(1);
	path_history = entryhist(path_history, file);
	file = evalpath(file);
	i = pack(file, list, *maxp);
	free(file);
	if (i < 0) {
		putterm(t_bell);
		return(1);
	}
	return(4);
}

static int unpack_file(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	if (isdir(&list[filepos])) return(warning_bell(list, maxp));
	if (archivefile) i = unpack(archivefile, NULL, list, *maxp, 0);
	else i = unpack(list[filepos].name, NULL, NULL, 0, 0);
	if (i < 0) return(warning_bell(list, maxp));
	if (!i) return(1);
	return(4);
}

static int unpack_tree(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	if (isdir(&list[filepos])) return(warning_bell(list, maxp));
	if (archivefile) i = unpack(archivefile, NULL, list, *maxp, 1);
	else i = unpack(list[filepos].name, NULL, NULL, 0, 1);
	if (i <= 0) {
		if (i < 0) warning_bell(list, maxp);
		return(3);
	}
	return(4);
}

static int info_filesys(list, maxp)
namelist *list;
int *maxp;
{
	char *path;
	int i;

	if (!(path = inputstr2(FSDIR_K, -1, NULL, path_history))) return(1);
	path_history = entryhist(path_history, path);
	if (*path) path = evalpath(path);
	else {
		free(path);
		path = strdup2(".");
	}
	i = infofs(path);
	free(path);
	if (!i) return(1);
	return(3);
}

static int attr_file(list, maxp)
namelist *list;
int *maxp;
{
	char *str[2];
	int i, n, flag, val[2];

	str[0] = CMODE_K;
	str[1] = CDATE_K;
	val[0] = 1;
	val[1] = 2;

	if (mark > 0) {
		for (i = 0; i < *maxp; i++) if (ismark(&list[i])) break;
		if (i < *maxp) i = filepos;
		flag = 1;
		locate(0, LINFO);
		putterm(l_clear);
		kanjiputs(ATTRM_K);
		if (selectstr(&flag, 2, 35, str, val) == ESC)
			return(1);
	}
	else {
		if (islink(&list[filepos])) {
			warning(0, ILLNK_K);
			return(1);
		}
		i = filepos;
		flag = 3;
	}

	while ((n = inputattr(&list[i], flag)) < 0) warning(0, ILTMS_K);
	if (!n) return(3);

	if (mark > 0) applyfile(list, *maxp, setattr, NULL);
	else if (setattr(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	return(4);
}

static int tree_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *path;

	if (!(path = tree())) return(3);
	if (chdir2(path) < 0) {
		warning(-1, path);
		free(path);
		return(3);
	}
	free(path);
	if (findpattern) free(findpattern);
	findpattern = NULL;
	free(list[filepos].name);
	list[filepos].name = strdup2("..");
	return(4);
}

static int backup_tape(list, maxp)
namelist *list;
int *maxp;
{
	char *dev;
	int i;

	if (!(dev = inputstr2(BKUP_K, 5, "/dev/", path_history))) return(1);
	path_history = entryhist(path_history, dev);
	dev = evalpath(dev);
	i = backup(dev, list, *maxp);
	free(dev);
	if (i <= 0) return(1);
	return(4);
}

static int warning_bell(list, maxp)
namelist *list;
int *maxp;
{
	putterm(t_bell);
	return(0);
}

static int no_operation(list, maxp)
namelist *list;
int *maxp;
{
	return(0);
}
