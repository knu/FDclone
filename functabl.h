/*
 *	functabl.h
 *
 *	Function Status Table
 */


#ifndef	_TBL_
#define	_TBL_(func, id, hlp, flg)	{func, id, hlp, flg}
#endif

functable funclist[] = {
	_TBL_(cur_up,		"CUR_UP",	CUP_H,	ARCH),
	_TBL_(cur_down,		"CUR_DOWN",	CDWN_H,	ARCH),
	_TBL_(cur_right,	"CUR_RIGHT",	CRIG_H,	ARCH),
	_TBL_(cur_left,		"CUR_LEFT",	CLEF_H,	ARCH),
	_TBL_(roll_up,		"ROLL_UP",	RUP_H,	LISTUP|ARCH),
	_TBL_(roll_down,	"ROLL_DOWN",	RDWN_H,	LISTUP|ARCH),
	_TBL_(cur_top,		"CUR_TOP",	CTOP_H,	ARCH),
	_TBL_(cur_bottom,	"CUR_BOTTOM",	CBTM_H,	ARCH),
	_TBL_(fname_right,	"FNAME_RIGHT",	FNRI_H,	REWRITE|ARCH),
	_TBL_(fname_left,	"FNAME_LEFT",	FNLE_H,	REWRITE|ARCH),
	_TBL_(one_column,	"ONE_COLUMN",	COL1_H,	RELIST|ARCH|NO_FILE),
	_TBL_(two_columns,	"TWO_COLUMNS",	COL2_H,	RELIST|ARCH|NO_FILE),
	_TBL_(three_columns,	"THREE_COLUMNS",COL3_H,	RELIST|ARCH|NO_FILE),
	_TBL_(five_columns,	"FIVE_COLUMNS",	COL5_H,	RELIST|ARCH|NO_FILE),
	_TBL_(mark_file,	"MARK_FILE",	MRK_H,	REWRITE|ARCH),
	_TBL_(mark_file2,	"MARK_FILE2",	MRK2_H,	REWRITE|ARCH),
	_TBL_(mark_all,		"MARK_ALL",	MRKA_H,	LISTUP|ARCH),
	_TBL_(mark_reverse,	"MARK_REVERSE",	MRKR_H,	LISTUP|ARCH),
	_TBL_(in_dir,		"IN_DIR",	IND_H,	KILLSTK|ARCH),
	_TBL_(out_dir,		"OUT_DIR",	OUTD_H,	KILLSTK|ARCH|NO_FILE),
	_TBL_(log_top,		"LOG_TOP",	LOGT_H,	KILLSTK|NO_FILE),
	_TBL_(reread_dir,	"REREAD_DIR",	READ_H,	ARCH|NO_FILE),
	_TBL_(push_file,	"PUSH_FILE",	PUSH_H,	RELIST),
	_TBL_(pop_file,		"POP_FILE",	POP_H,	RELIST),
	_TBL_(log_dir,		"LOG_DIR",	LOGD_H,	KILLSTK|NO_FILE),
	_TBL_(attr_file,	"ATTR_FILE",	ATTR_H,	KILLSTK|RELIST),
	_TBL_(execute_file,	"EXECUTE_FILE",	EXEF_H,	KILLSTK|REWRITE|ARCH),
	_TBL_(info_filesys,	"INFO_FILESYS",	INFO_H,	KILLSTK|LISTUP|ARCH),
	_TBL_(copy_file,	"COPY_FILE",	COPY_H,	RELIST),
	_TBL_(move_file,	"MOVE_FILE",	MOVE_H,	KILLSTK),
	_TBL_(delete_file,	"DELETE_FILE",	DELF_H,	KILLSTK),
	_TBL_(delete_dir,	"DELETE_DIR",	DELD_H,	KILLSTK),
	_TBL_(rename_file,	"RENAME_FILE",	RENM_H,	KILLSTK),
	_TBL_(make_dir,		"MAKE_DIR",	MKDR_H,	KILLSTK|NO_FILE),
	_TBL_(sort_dir,		"SORT_DIR",	SORT_H,	RELIST|ARCH),
	_TBL_(execute_sh,	"EXECUTE_SH",	EXSH_H,	KILLSTK|RELIST|ARCH),
	_TBL_(find_file,	"FIND_FILE",	FNDF_H,	KILLSTK|ARCH|NO_FILE),
#ifndef	_NOWRITEFS
	_TBL_(write_dir,	"WRITE_DIR",	WRIT_H,	KILLSTK),
#endif
#ifndef	_NOTREE
	_TBL_(tree_dir,		"TREE_DIR",	TREE_H,	KILLSTK|RELIST|NO_FILE),
#endif
#ifndef	_NOARCHIVE
	_TBL_(backup_tape,	"BACKUP_TAPE",	BACK_H,	KILLSTK|REWRITE),
#endif
	_TBL_(edit_file,	"EDIT_FILE",	EDIT_H,	KILLSTK|REWRITE),
	_TBL_(view_file,	"VIEW_FILE",	VIEW_H,	RELIST|ARCH),
#ifndef	_NOARCHIVE
	_TBL_(unpack_file,	"UNPACK_FILE",	UNPK_H,	KILLSTK|REWRITE|ARCH),
	_TBL_(pack_file,	"PACK_FILE",	PACK_H,	KILLSTK|REWRITE),
#endif
#ifndef	_NOTREE
	_TBL_(tree_dir,		"LOG_TREE",	LGTR_H,	KILLSTK|RELIST|NO_FILE),
	_TBL_(copy_tree,	"COPY_TREE",	CPTR_H,	KILLSTK|RELIST),
	_TBL_(move_tree,	"MOVE_TREE",	MVTR_H,	KILLSTK|RELIST),
#ifndef	_NOARCHIVE
	_TBL_(unpack_tree,	"UNPACK_TREE",	UPTR_H,	KILLSTK|RELIST|ARCH),
#endif
#endif	/* !_NOTREE */
	_TBL_(find_dir,		"FIND_DIR",	FNDD_H,	KILLSTK|RELIST),
	_TBL_(symlink_mode,	"SYMLINK_MODE",	SYLN_H,	NO_FILE),
	_TBL_(filetype_mode,	"FILETYPE_MODE",FLTY_H,	RELIST|ARCH|NO_FILE),
	_TBL_(dotfile_mode,	"DOTFILE_MODE",	DTFL_H,	KILLSTK|NO_FILE),
	_TBL_(mark_find,	"MARK_FIND",	MRKF_H,	LISTUP|ARCH),
	_TBL_(mark_file3,	"MARK_FILE3",	MRK2_H,	REWRITE|ARCH),
#ifndef	_NOARCHIVE
	_TBL_(launch_file,	"LAUNCH_FILE",	LAUN_H,	KILLSTK|RELIST|ARCH),
#endif
	_TBL_(search_forw,	"SEARCH_FORW",	SEAF_H,	ARCH),
	_TBL_(search_back,	"SEARCH_BACK",	SEAB_H,	ARCH),
	_TBL_(help_message,	"HELP_MESSAGE",	HELP_H,	RELIST|ARCH|NO_FILE),
	_TBL_(quit_system,	"QUIT_SYSTEM",	QUIT_H,	KILLSTK|ARCH|NO_FILE),
	_TBL_(warning_bell,	"WARNING_BELL",	NULL_H,	ARCH|NO_FILE),
	_TBL_(no_operation,	"NO_OPERATION",	NULL_H,	ARCH|NO_FILE)
};
