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
extern int copypolicy;
extern namelist filestack[];
extern char *archivefile;
extern char *archivedir;
extern char *destpath;
extern int writefs;
extern int savehist;

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
static int mark_file3();
static int mark_all();
static int mark_reverse();
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
bindtable bindlist[MAXBINDTABLE] = {
	{K_UP,		CUR_UP,		255},
	{K_DOWN,	CUR_DOWN,	255},
	{K_RIGHT,	CUR_RIGHT,	255},
	{K_LEFT,	CUR_LEFT,	255},
	{K_NPAGE,	ROLL_UP,	255},
	{K_PPAGE,	ROLL_DOWN,	255},
	{K_F(1),	LOG_DIR,	255},
	{K_F(2),	EXECUTE_FILE,	255},
	{K_F(3),	COPY_FILE,	255},
	{K_F(4),	DELETE_FILE,	255},
	{K_F(5),	RENAME_FILE,	255},
	{K_F(6),	SORT_DIR,	255},
	{K_F(7),	FIND_FILE,	255},
	{K_F(8),	TREE_DIR,	255},
	{K_F(9),	EDIT_FILE,	255},
	{K_F(10),	UNPACK_FILE,	255},
	{K_F(11),	ATTR_FILE,	255},
	{K_F(12),	INFO_FILESYS,	255},
	{K_F(13),	MOVE_FILE,	255},
	{K_F(14),	DELETE_DIR,	255},
	{K_F(15),	MAKE_DIR,	255},
	{K_F(16),	EXECUTE_SH,	255},
	{K_F(17),	WRITE_DIR,	255},
	{K_F(18),	BACKUP_TAPE,	255},
	{K_F(19),	VIEW_FILE,	255},
	{K_F(20),	PACK_FILE,	255},
	{CR,		LAUNCH_FILE,	IN_DIR},
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
	{'b',		BACKUP_TAPE,	255},
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
	{'p',		PACK_FILE,	255},
	{'q',		QUIT_SYSTEM,	255},
	{'r',		RENAME_FILE,	255},
	{'s',		SORT_DIR,	255},
	{'t',		TREE_DIR,	255},
	{'u',		UNPACK_FILE,	255},
	{'v',		VIEW_FILE,	255},
	{'w',		WRITE_DIR,	255},
	{'x',		EXECUTE_FILE,	255},
	{'C',		COPY_TREE,	255},
	{'D',		DELETE_DIR,	255},
	{'F',		FIND_DIR,	255},
	{'H',		DOTFILE_MODE,	255},
	{'L',		LOG_TREE,	255},
	{'M',		MOVE_TREE,	255},
	{'Q',		QUIT_SYSTEM,	255},
	{'S',		SYMLINK_MODE,	255},
	{'T',		FILETYPE_MODE,	255},
	{'U',		UNPACK_TREE,	255},
	{K_HOME,	MARK_ALL,	255},
	{K_END,		MARK_REVERSE,	255},
	{K_BEG,		CUR_TOP,	255},
	{K_EOL,		CUR_BOTTOM,	255},
	{CTRL('@'),	MARK_FILE3,	255},
	{CTRL('L'),	REREAD_DIR,	255},
	{-1,		NO_OPERATION,	255}
};


/*ARGSUSED*/
static int cur_up(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos <= 0) return(0);
	filepos--;
	return(2);
}

/*ARGSUSED*/
static int cur_down(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos >= *maxp - 1) return(0);
	filepos++;
	return(2);
}

/*ARGSUSED*/
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

/*ARGSUSED*/
static int cur_left(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos - FILEPERLOW < 0) return(0);
	else filepos -= FILEPERLOW;
	return(2);
}

/*ARGSUSED*/
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

/*ARGSUSED*/
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

/*ARGSUSED*/
static int cur_top(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos == 0) return(0);
	filepos = 0;
	return(2);
}

/*ARGSUSED*/
static int cur_bottom(list, maxp)
namelist *list;
int *maxp;
{
	if (filepos == *maxp - 1) return(0);
	filepos = *maxp - 1;
	return(2);
}

/*ARGSUSED*/
static int in_dir(list, maxp)
namelist *list;
int *maxp;
{
	if (!archivefile && !strcmp(list[filepos].name, "."))
		return(warning_bell(list, maxp));
	return(5);
}

/*ARGSUSED*/
static int out_dir(list, maxp)
namelist *list;
int *maxp;
{
	if (archivefile) filepos = -1;
	else {
		free(list[filepos].name);
		list[filepos].name = strdup2("..");
	}
	return(5);
}

/*ARGSUSED*/
static int one_column(list, maxp)
namelist *list;
int *maxp;
{
	columns = 1;
	return(2);
}

/*ARGSUSED*/
static int two_columns(list, maxp)
namelist *list;
int *maxp;
{
	columns = 2;
	return(2);
}

/*ARGSUSED*/
static int three_columns(list, maxp)
namelist *list;
int *maxp;
{
	columns = 3;
	return(2);
}

/*ARGSUSED*/
static int five_columns(list, maxp)
namelist *list;
int *maxp;
{
	columns = 5;
	return(2);
}

/*ARGSUSED*/
static int fname_right(list, maxp)
namelist *list;
int *maxp;
{
	if (fnameofs <= 0) return(0);
	fnameofs--;
	return(2);
}

/*ARGSUSED*/
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
		|| Xaccess(list[filepos].name, X_OK) >= 0)) i--;
	}
	if (fnameofs + i >= strlen(list[filepos].name)) return(0);
	fnameofs++;
	return(2);
}

/*ARGSUSED*/
static int mark_file(list, maxp)
namelist *list;
int *maxp;
{
	if (isdir(&list[filepos])) return(0);
	list[filepos].flags ^= F_ISMRK;
	if (ismark(&list[filepos])) mark++;
	else mark--;
	locate(CMARK + 5, LSTATUS);
	cprintf("%4d", mark);
	return(2);
}

/*ARGSUSED*/
static int mark_file2(list, maxp)
namelist *list;
int *maxp;
{
	mark_file(list, maxp);
	if (filepos < *maxp - 1) filepos++;
	return(2);
}

/*ARGSUSED*/
static int mark_file3(list, maxp)
namelist *list;
int *maxp;
{
	mark_file(list, maxp);
	if (filepos < *maxp - 1
	&& filepos / FILEPERPAGE == (filepos + 1) / FILEPERPAGE) filepos++;
	else filepos = (filepos / FILEPERPAGE) * FILEPERPAGE;
	return(2);
}

/*ARGSUSED*/
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
	locate(CMARK + 5, LSTATUS);
	cprintf("%4d", mark);
	return(2);
}

/*ARGSUSED*/
static int mark_reverse(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	mark = 0;
	for (i = 0; i < *maxp; i++) if (!isdir(&list[i]))
		if (list[i].flags ^= F_ISMRK) mark++;
	locate(CMARK + 5, LSTATUS);
	cprintf("%4d", mark);
	return(2);
}

/*ARGSUSED*/
static int mark_find(list, maxp)
namelist *list;
int *maxp;
{
	reg_t *re;
	char *cp, *wild;
	int i;

	if (!(wild = inputstr(FINDF_K, 0, 0, "*", NULL))) return(1);
	if (!*wild || strchr(wild, '/')) {
		warning(ENOENT, wild);
		free(wild);
		return(1);
	}

	cp = cnvregexp(wild, 1);
	re = regexp_init(cp);
	free(wild);
	free(cp);
	for (i = 0; i < *maxp; i++)
		if (!isdir(&list[i]) && regexp_exec(re, list[i].name)) {
			list[i].flags |= F_ISMRK;
			mark++;
		}
	regexp_free(re);
	locate(CMARK + 5, LSTATUS);
	cprintf("%4d", mark);
	return(3);
}

/*ARGSUSED*/
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

/*ARGSUSED*/
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

/*ARGSUSED*/
static int symlink_mode(list, maxp)
namelist *list;
int *maxp;
{
	dispmode ^= F_SYMLINK;
	return(4);
}

/*ARGSUSED*/
static int filetype_mode(list, maxp)
namelist *list;
int *maxp;
{
	dispmode ^= F_FILETYPE;
	return(2);
}

/*ARGSUSED*/
static int dotfile_mode(list, maxp)
namelist *list;
int *maxp;
{
	dispmode ^= F_DOTFILE;
	return(4);
}

/*ARGSUSED*/
static int log_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *path;

	if (!(path = inputstr(LOGD_K, 0, -1, NULL, NULL))
	|| !*(path = evalpath(path))) return(1);
	if (!chdir3(path)) {
		warning(-1, path);
		free(path);
		return(1);
	}
	free(path);
	free(list[filepos].name);
	list[filepos].name = strdup2("..");
	return(4);
}

/*ARGSUSED*/
static int log_top(list, maxp)
namelist *list;
int *maxp;
{
	char *path;

	path = strdup2("/");
	if (chdir2(path) < 0) error(path);
	if (findpattern) free(findpattern);
	findpattern = NULL;
	free(list[filepos].name);
	list[filepos].name = path;
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

/*ARGSUSED*/
static int view_file(list, maxp)
namelist *list;
int *maxp;
{
	char *dir;
	int drive;

	drive = 0;
	if (isdir(&list[filepos])
	|| (archivefile && !(dir = tmpunpack(list, *maxp)))
	|| ((drive = dospath("", NULL)) && !(dir = tmpdosdupl(drive,
	list[filepos].name, list[filepos].st_mode)))) return(1);
	if (!execenv("FD_PAGER", list[filepos].name)) {
#ifdef	PAGER
		execmacro(PAGER, list[filepos].name, NULL, 0, 1, 0);
#else
		do {
			dump(list[filepos].name);
		} while(!yesno(PEND_K));
#endif
	}
	if (drive) removetmp(dir, NULL, list[filepos].name);
	else if (archivefile) removetmp(dir, archivedir, list[filepos].name);
	return(2);
}

/*ARGSUSED*/
static int edit_file(list, maxp)
namelist *list;
int *maxp;
{
	char *dir;
	int drive;

	if (isdir(&list[filepos])) return(warning_bell(list, maxp));
	if ((drive = dospath("", NULL)) && !(dir = tmpdosdupl(drive,
	list[filepos].name, list[filepos].st_mode))) return(1);
	if (!execenv("FD_EDITOR", list[filepos].name)) {
#ifdef	EDITOR
		execmacro(EDITOR, list[filepos].name, NULL, 0, 1, 0);
#endif
	}
	if (drive) {
		if (tmpdosrestore(drive, list[filepos].name,
		list[filepos].st_mode) < 0) warning(-1, list[filepos].name);
		removetmp(dir, NULL, list[filepos].name);
	}
	return(4);
}

/*ARGSUSED*/
static int sort_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *str[5];
	int i, tmp1, tmp2, val[5], *dupl;

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

/*ARGSUSED*/
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

/*ARGSUSED*/
static int reread_dir(list, maxp)
namelist *list;
int *maxp;
{
	getwsize(80, WHEADER + WFOOTER + 2);
	return(4);
}

/*ARGSUSED*/
static int help_message(list, maxp)
namelist *list;
int *maxp;
{
	help(archivefile != NULL);
	return(2);
}

/*ARGSUSED*/
static int quit_system(list, maxp)
namelist *list;
int *maxp;
{
	if (!archivefile && !yesno(QUIT_K)) return(1);
	if (savehist > 0) savehistory(sh_history, HISTORYFILE);
	return(-1);
}

/*ARGSUSED*/
static int make_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *path;

	if (!(path = inputstr(MAKED_K, 1, -1, NULL, NULL))
	|| !*(path = evalpath(path))) return(1);
	if (mkdir2(path, 0777) < 0) warning(-1, path);
	free(path);
	return(4);
}

/*ARGSUSED*/
static int copy_file(list, maxp)
namelist *list;
int *maxp;
{
	return(copyfile(list, *maxp, 0));
}

/*ARGSUSED*/
static int copy_tree(list, maxp)
namelist *list;
int *maxp;
{
	return(copyfile(list, *maxp, 1));
}

/*ARGSUSED*/
static int move_file(list, maxp)
namelist *list;
int *maxp;
{
	return(movefile(list, *maxp, 0));
}

/*ARGSUSED*/
static int move_tree(list, maxp)
namelist *list;
int *maxp;
{
	return(movefile(list, *maxp, 1));
}

/*ARGSUSED*/
static int rename_file(list, maxp)
namelist *list;
int *maxp;
{
	char *file;

	if (!strcmp(list[filepos].name, ".")
	|| !strcmp(list[filepos].name, "..")) return(warning_bell(list, maxp));
	for (;;) {
		if (!(file = inputstr(NEWNM_K, 1, 0, list[filepos].name, NULL)))
			return(1);
		file = evalpath(file);
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
	free(list[filepos].name);
	if (!strchr(file, '/')) list[filepos].name = file;
	else {
		free(file);
		list[filepos].name = strdup2("..");
	}
	return(4);
}

/*ARGSUSED*/
static int delete_file(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	copypolicy = 0;
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

/*ARGSUSED*/
static int delete_dir(list, maxp)
namelist *list;
int *maxp;
{
	int i;

	if (!isdir(&list[filepos])
	|| !strcmp(list[filepos].name, ".")
	|| !strcmp(list[filepos].name, "..")) return(warning_bell(list, maxp));
	if (!yesno(DELDR_K, list[filepos].name)) return(1);
	copypolicy = 0;
	if (islink(&list[filepos])) {
		if ((i = unlink2(list[filepos].name)) < 0)
			warning(-1, list[filepos].name);
		if (!i) filepos++;
	}
	else if (!applydir(list[filepos].name, unlink2, NULL, rmdir2, NULL))
		filepos++;
	if (filepos >= *maxp && (filepos -= 2) < 0) filepos = 0;
	return(4);
}

/*ARGSUSED*/
static int find_file(list, maxp)
namelist *list;
int *maxp;
{
	char *wild;

	if (!(wild = inputstr(FINDF_K, 0, 0, "*", NULL))) return(1);
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

/*ARGSUSED*/
static int find_dir(list, maxp)
namelist *list;
int *maxp;
{
	char *cp, *wild;

	if (!(wild = inputstr(FINDD_K, 0, 0, "*", NULL))) return(1);
	if (!*wild || strchr(wild, '/')) {
		warning(ENOENT, wild);
		free(wild);
		return(1);
	}
	destpath = NULL;
	cp = cnvregexp(wild, 1);
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

/*ARGSUSED*/
static int execute_sh(list, maxp)
namelist *list;
int *maxp;
{
	char *com;

	if (!(com = inputstr("sh#", 0, -1, NULL, &sh_history))) return(1);
	if (*com) execmacro(com, list[filepos].name, list, *maxp, 0, 1);
	else execshell();
	free(com);
	return(4);
}

/*ARGSUSED*/
static int execute_file(list, maxp)
namelist *list;
int *maxp;
{
	char *com, *dir;
	int len, drive;

	len = (Xaccess(list[filepos].name, X_OK) >= 0) ?
		strlen(list[filepos].name) + 1 : 0;
	if (!(com = inputstr("sh#", 0, len, list[filepos].name, &sh_history)))
		return(1);
	if (!*com) {
		execshell();
		free(com);
		return(4);
	}

	drive = 0;
	if ((archivefile && !(dir = tmpunpack(list, *maxp)))
	|| ((drive = dospath("", NULL)) && !(dir = tmpdosdupl(drive,
	list[filepos].name, list[filepos].st_mode)))) {
		free(com);
		return(1);
	}
	if (drive) {
		execmacro(com, list[filepos].name, NULL, 0, 0, 1);
		removetmp(dir, NULL, list[filepos].name);
	}
	else if (archivefile) {
		execmacro(com, list[filepos].name, NULL, 0, 0, 1);
		removetmp(dir, archivedir, list[filepos].name);
	}
	else execmacro(com, list[filepos].name, list, *maxp, 0, 1);
	free(com);
	return(4);
}

/*ARGSUSED*/
static int launch_file(list, maxp)
namelist *list;
int *maxp;
{
	if (launcher(list, *maxp) < 0) return(view_file(list, maxp));
	return(4);
}

/*ARGSUSED*/
static int pack_file(list, maxp)
namelist *list;
int *maxp;
{
	char *file;
	int i;

	if (!(file = inputstr(PACK_K, 1, -1, NULL, NULL))
	|| !*(file = evalpath(file))) return(1);
	i = pack(file, list, *maxp);
	free(file);
	if (i < 0) {
		putterm(t_bell);
		return(1);
	}
	return(4);
}

/*ARGSUSED*/
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

/*ARGSUSED*/
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

/*ARGSUSED*/
static int info_filesys(list, maxp)
namelist *list;
int *maxp;
{
	char *path;
	int i;

	if (!(path = inputstr(FSDIR_K, 0, -1, NULL, NULL))) return(1);
	if (!*(path = evalpath(path))) {
		free(path);
		path = strdup2(".");
	}
	i = infofs(path);
	free(path);
	if (!i) return(1);
	return(3);
}

/*ARGSUSED*/
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
		if (selectstr(&flag, 2, 35, str, val) == ESC) return(1);
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
	if (!n) return(2);

	if (mark > 0) applyfile(list, *maxp, setattr, NULL);
	else if (setattr(list[filepos].name) < 0)
		warning(-1, list[filepos].name);
	return(4);
}

/*ARGSUSED*/
static int tree_dir(list, maxp)
namelist *list;
int *maxp;
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
	free(list[filepos].name);
	list[filepos].name = strdup2("..");
	return(4);
}

/*ARGSUSED*/
static int backup_tape(list, maxp)
namelist *list;
int *maxp;
{
	char *dev;
	int i;

	if (!(dev = inputstr(BKUP_K, 1, 5, "/dev/", NULL))
	|| !*(dev = evalpath(dev))) return(1);
	i = backup(dev, list, *maxp);
	free(dev);
	if (i <= 0) return(1);
	return(4);
}

/*ARGSUSED*/
static int warning_bell(list, maxp)
namelist *list;
int *maxp;
{
	putterm(t_bell);
	return(0);
}

/*ARGSUSED*/
static int no_operation(list, maxp)
namelist *list;
int *maxp;
{
	return(0);
}
