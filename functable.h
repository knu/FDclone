/*
 *	functable.h
 *
 *	Function Status Table
 */

functable funclist[] = {
	{cur_up,	"CUR_UP",	CUP_H,	ARCH},
	{cur_down,	"CUR_DOWN",	CDWN_H,	ARCH},
	{cur_right,	"CUR_RIGHT",	CRIG_H,	ARCH},
	{cur_left,	"CUR_LEFT",	CLEF_H,	ARCH},
	{roll_up,	"ROLL_UP",	RUP_H,	LISTUP | ARCH},
	{roll_down,	"ROLL_DOWN",	RDWN_H,	LISTUP | ARCH},
	{cur_top,	"CUR_TOP",	CTOP_H,	ARCH},
	{cur_bottom,	"CUR_BOTTOM",	CBTM_H,	ARCH},
	{fname_right,	"FNAME_RIGHT",	FNRI_H,	REWRITE | ARCH},
	{fname_left,	"FNAME_LEFT",	FNLE_H,	REWRITE | ARCH},
	{one_column,	"ONE_COLUMN",	COL1_H,	LISTUP | ARCH},
	{two_columns,	"TWO_COLUMNS",	COL2_H,	LISTUP | ARCH},
	{three_columns,	"THREE_COLUMNS",COL3_H,	LISTUP | ARCH},
	{five_columns,	"FIVE_COLUMNS",	COL5_H,	LISTUP | ARCH},
	{mark_file,	"MARK_FILE",	MRK_H,	REWRITE | ARCH},
	{mark_file2,	"MARK_FILE2",	MRK2_H,	REWRITE | ARCH},
	{mark_all,	"MARK_ALL",	MRKA_H,	LISTUP | ARCH},
	{mark_find,	"MARK_FIND",	MRKF_H,	LISTUP | ARCH},
	{in_dir,	"IN_DIR",	IND_H,	KILLSTK | ARCH},
	{out_dir,	"OUT_DIR",	OUTD_H,	KILLSTK | ARCH},
	{push_file,	"PUSH_FILE",	PUSH_H,	REWRITE | LISTUP},
	{pop_file,	"POP_FILE",	POP_H,	REWRITE | LISTUP},
	{symlink_mode,	"SYMLINK_MODE",	SYLN_H,	0},
	{reread_dir,	"REREAD_DIR",	READ_H,	ARCH},
	{log_dir,	"LOG_DIR",	LOGD_H,	KILLSTK},
	{attr_file,	"ATTR_FILE",	ATTR_H,	KILLSTK | REWRITE | LISTUP},
	{execute_file,	"EXECUTE_FILE",	EXEF_H,	KILLSTK | REWRITE | ARCH},
	{info_filesys,	"INFO_FILESYS",	INFO_H,	KILLSTK | LISTUP | ARCH},
	{copy_file,	"COPY_FILE",	COPY_H,	REWRITE | LISTUP},
	{move_file,	"MOVE_FILE",	MOVE_H,	KILLSTK},
	{delete_file,	"DELETE_FILE",	DELF_H,	KILLSTK},
	{delete_dir,	"DELETE_DIR",	DELD_H,	KILLSTK},
	{rename_file,	"RENAME_FILE",	RENM_H,	KILLSTK},
	{make_dir,	"MAKE_DIR",	MKDR_H,	KILLSTK},
	{sort_dir,	"SORT_DIR",	SORT_H,	REWRITE | LISTUP | ARCH},
	{execute_sh,	"EXECUTE_SH",	EXSH_H,	KILLSTK | REWRITE | ARCH},
	{find_file,	"FIND_FILE",	FNDF_H,	KILLSTK | ARCH},
	{write_dir,	"WRITE_DIR",	WRIT_H,	KILLSTK},
	{tree_dir,	"TREE_DIR",	TREE_H,	KILLSTK | REWRITE | LISTUP},
	{backup_tape,	"BACKUP_TAPE",	BACK_H,	KILLSTK | REWRITE},
	{edit_file,	"EDIT_FILE",	EDIT_H,	KILLSTK | REWRITE},
	{view_file,	"VIEW_FILE",	VIEW_H,	REWRITE | LISTUP | ARCH},
	{unpack_file,	"UNPACK_FILE",	UNPK_H,	KILLSTK | REWRITE | ARCH},
	{pack_file,	"PACK_FILE",	PACK_H,	KILLSTK | REWRITE},
	{tree_dir,	"LOG_TREE",	LGTR_H,	KILLSTK | REWRITE | LISTUP},
	{copy_tree,	"COPY_TREE",	CPTR_H,	KILLSTK | REWRITE | LISTUP},
	{move_tree,	"MOVE_TREE",	MVTR_H,	KILLSTK | REWRITE | LISTUP},
	{unpack_tree,	"UNPACK_TREE",	UPTR_H,	KILLSTK | REWRITE | LISTUP | ARCH},
	{find_dir,	"FIND_DIR",	FNDD_H,	KILLSTK | REWRITE | LISTUP},
	{launch_file,	"LAUNCH_FILE",	LAUN_H,	KILLSTK | REWRITE | LISTUP | ARCH},
	{help_message,	"HELP_MESSAGE",	HELP_H,	LISTUP | ARCH},
	{quit_system,	"QUIT_SYSTEM",	QUIT_H,	KILLSTK | ARCH},
	{warning_bell,	"WARNING_BELL",	NULL,	ARCH},
	{no_operation,	"NO_OPERATION",	NULL,	ARCH}
};
