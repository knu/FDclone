/*
 *	functabl.h
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
	{one_column,	"ONE_COLUMN",	COL1_H,	RELIST | ARCH | NO_FILE},
	{two_columns,	"TWO_COLUMNS",	COL2_H,	RELIST | ARCH | NO_FILE},
	{three_columns,	"THREE_COLUMNS",COL3_H,	RELIST | ARCH | NO_FILE},
	{five_columns,	"FIVE_COLUMNS",	COL5_H,	RELIST | ARCH | NO_FILE},
	{mark_file,	"MARK_FILE",	MRK_H,	REWRITE | ARCH},
	{mark_file2,	"MARK_FILE2",	MRK2_H,	REWRITE | ARCH},
	{mark_all,	"MARK_ALL",	MRKA_H,	LISTUP | ARCH},
	{mark_reverse,	"MARK_REVERSE",	MRKR_H,	LISTUP | ARCH},
	{in_dir,	"IN_DIR",	IND_H,	KILLSTK | ARCH},
	{out_dir,	"OUT_DIR",	OUTD_H,	KILLSTK | ARCH | NO_FILE},
	{log_top,	"LOG_TOP",	LOGT_H,	KILLSTK | NO_FILE},
	{reread_dir,	"REREAD_DIR",	READ_H,	ARCH | NO_FILE},
	{push_file,	"PUSH_FILE",	PUSH_H,	RELIST},
	{pop_file,	"POP_FILE",	POP_H,	RELIST},
	{log_dir,	"LOG_DIR",	LOGD_H,	KILLSTK | NO_FILE},
	{attr_file,	"ATTR_FILE",	ATTR_H,	KILLSTK | RELIST},
	{execute_file,	"EXECUTE_FILE",	EXEF_H,	KILLSTK | REWRITE | ARCH},
	{info_filesys,	"INFO_FILESYS",	INFO_H,	KILLSTK | LISTUP | ARCH},
	{copy_file,	"COPY_FILE",	COPY_H,	RELIST},
	{move_file,	"MOVE_FILE",	MOVE_H,	KILLSTK},
	{delete_file,	"DELETE_FILE",	DELF_H,	KILLSTK},
	{delete_dir,	"DELETE_DIR",	DELD_H,	KILLSTK},
	{rename_file,	"RENAME_FILE",	RENM_H,	KILLSTK},
	{make_dir,	"MAKE_DIR",	MKDR_H,	KILLSTK | NO_FILE},
	{sort_dir,	"SORT_DIR",	SORT_H,	RELIST | ARCH},
	{execute_sh,	"EXECUTE_SH",	EXSH_H,	KILLSTK | RELIST | ARCH},
	{find_file,	"FIND_FILE",	FNDF_H,	KILLSTK | ARCH | NO_FILE},
#ifndef	_NOWRITEFS
	{write_dir,	"WRITE_DIR",	WRIT_H,	KILLSTK},
#endif
#ifndef	_NOTREE
	{tree_dir,	"TREE_DIR",	TREE_H,	KILLSTK | RELIST | NO_FILE},
#endif
#ifndef	_NOARCHIVE
	{backup_tape,	"BACKUP_TAPE",	BACK_H,	KILLSTK | REWRITE},
#endif
	{edit_file,	"EDIT_FILE",	EDIT_H,	KILLSTK | REWRITE},
	{view_file,	"VIEW_FILE",	VIEW_H,	RELIST | ARCH},
#ifndef	_NOARCHIVE
	{unpack_file,	"UNPACK_FILE",	UNPK_H,	KILLSTK | REWRITE | ARCH},
	{pack_file,	"PACK_FILE",	PACK_H,	KILLSTK | REWRITE},
#endif
#ifndef	_NOTREE
	{tree_dir,	"LOG_TREE",	LGTR_H,	KILLSTK | RELIST | NO_FILE},
	{copy_tree,	"COPY_TREE",	CPTR_H,	KILLSTK | RELIST},
	{move_tree,	"MOVE_TREE",	MVTR_H,	KILLSTK | RELIST},
#ifndef	_NOARCHIVE
	{unpack_tree,	"UNPACK_TREE",	UPTR_H,	KILLSTK | RELIST | ARCH},
#endif
#endif	/* !_NOTREE */
	{find_dir,	"FIND_DIR",	FNDD_H,	KILLSTK | RELIST},
	{symlink_mode,	"SYMLINK_MODE",	SYLN_H,	NO_FILE},
	{filetype_mode,	"FILETYPE_MODE",FLTY_H,	RELIST | ARCH | NO_FILE},
	{dotfile_mode,	"DOTFILE_MODE",	DTFL_H,	KILLSTK | NO_FILE},
	{mark_find,	"MARK_FIND",	MRKF_H,	LISTUP | ARCH},
	{mark_file3,	"MARK_FILE3",	MRK2_H,	REWRITE | ARCH},
#ifndef	_NOARCHIVE
	{launch_file,	"LAUNCH_FILE",	LAUN_H,	KILLSTK | RELIST | ARCH},
#endif
	{search_forw,	"SEARCH_FORW",	SEAF_H,	ARCH},
	{search_back,	"SEARCH_BACK",	SEAB_H,	ARCH},
	{help_message,	"HELP_MESSAGE",	HELP_H,	RELIST | ARCH | NO_FILE},
	{quit_system,	"QUIT_SYSTEM",	QUIT_H,	KILLSTK | ARCH | NO_FILE},
	{warning_bell,	"WARNING_BELL",	NULL_H,	ARCH | NO_FILE},
	{no_operation,	"NO_OPERATION",	NULL_H,	ARCH | NO_FILE}
};