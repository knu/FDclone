/*
 *	functabl.h
 *
 *	function status table
 */


#ifndef	_TBL_
#define	_TBL_(func, id, hlp, flg)	{func, id, hlp, BIN(flg)}
#endif

#define	BIN(oct)	( (((oct)      ) & 0001) \
			| (((oct) >>  2) & 0002) \
			| (((oct) >>  4) & 0004) \
			| (((oct) >>  6) & 0010) \
			| (((oct) >>  8) & 0020) \
			| (((oct) >> 10) & 0040) \
			| (((oct) >> 12) & 0100) \
			| (((oct) >> 14) & 0200))

functable funclist[] = {
	/*						 +---------- NEEDSTATUS
	 *						 |+--------- RESTRICT
	 *						 ||+-------- NOFILE
	 *						 |||+------- ARCHIVE
	 *						 ||||+------ KILLSTACK
	 *						 |||||+----- RESCREEN
	 *						 ||||||++-- 01: REWRITE
	 *						 ||||||||   10: RELIST
	 *						 ||||||||   11: REWIN
	 *						 ||||||||   */
	_TBL_(cur_up,		"CUR_UP",	CUP_H,	000010000),
	_TBL_(cur_down,		"CUR_DOWN",	CDWN_H,	000010000),
	_TBL_(cur_right,	"CUR_RIGHT",	CRIG_H,	000010000),
	_TBL_(cur_left,		"CUR_LEFT",	CLEF_H,	000010000),
	_TBL_(roll_up,		"ROLL_UP",	RUP_H,	000010010),
	_TBL_(roll_down,	"ROLL_DOWN",	RDWN_H,	000010010),
	_TBL_(cur_top,		"CUR_TOP",	CTOP_H,	000010000),
	_TBL_(cur_bottom,	"CUR_BOTTOM",	CBTM_H,	000010000),
	_TBL_(fname_right,	"FNAME_RIGHT",	FNRI_H,	000010001),
	_TBL_(fname_left,	"FNAME_LEFT",	FNLE_H,	000010001),
	_TBL_(one_column,	"ONE_COLUMN",	COL1_H,	000110111),
	_TBL_(two_columns,	"TWO_COLUMNS",	COL2_H,	000110111),
	_TBL_(three_columns,	"THREE_COLUMNS",COL3_H,	000110111),
	_TBL_(five_columns,	"FIVE_COLUMNS",	COL5_H,	000110111),
	_TBL_(mark_file,	"MARK_FILE",	MRK_H,	010010001),
	_TBL_(mark_file2,	"MARK_FILE2",	MRK2_H,	010010001),
	_TBL_(mark_file3,	"MARK_FILE3",	MRK2_H,	010010001),
	_TBL_(mark_all,		"MARK_ALL",	MRKA_H,	010010010),
	_TBL_(mark_reverse,	"MARK_REVERSE",	MRKR_H,	010010010),
	_TBL_(mark_find,	"MARK_FIND",	MRKF_H,	010010010),
	_TBL_(in_dir,		"IN_DIR",	IND_H,	011011000),
	_TBL_(out_dir,		"OUT_DIR",	OUTD_H,	001111000),
	_TBL_(log_top,		"LOG_TOP",	LOGT_H,	001101000),
	_TBL_(reread_dir,	"REREAD_DIR",	READ_H,	000110000),
	_TBL_(push_file,	"PUSH_FILE",	PUSH_H,	000010110),
	_TBL_(pop_file,		"POP_FILE",	POP_H,	000010110),
	_TBL_(log_dir,		"LOG_DIR",	LOGD_H,	001111000),
	_TBL_(attr_file,	"ATTR_FILE",	ATTR_H,	011001110),
	_TBL_(execute_file,	"EXECUTE_FILE",	EXEF_H,	010011111),
	_TBL_(info_filesys,	"INFO_FILESYS",	INFO_H,	000110010),
	_TBL_(copy_file,	"COPY_FILE",	COPY_H,	011001000),
	_TBL_(move_file,	"MOVE_FILE",	MOVE_H,	011001000),
	_TBL_(delete_file,	"DELETE_FILE",	DELF_H,	011001000),
	_TBL_(delete_dir,	"DELETE_DIR",	DELD_H,	011001000),
	_TBL_(rename_file,	"RENAME_FILE",	RENM_H,	001001000),
	_TBL_(make_dir,		"MAKE_DIR",	MKDR_H,	001101000),
	_TBL_(sort_dir,		"SORT_DIR",	SORT_H,	000010110),
	_TBL_(execute_sh,	"EXECUTE_SH",	EXSH_H,	000111111),
	_TBL_(find_file,	"FIND_FILE",	FNDF_H,	000111000),
#ifndef	_NOWRITEFS
	_TBL_(write_dir,	"WRITE_DIR",	WRIT_H,	001001000),
#endif
#ifndef	_NOTREE
	_TBL_(tree_dir,		"TREE_DIR",	TREE_H,	001101110),
#endif
#ifndef	_NOARCHIVE
	_TBL_(backup_tape,	"BACKUP_TAPE",	BACK_H,	001011000),
#endif
	_TBL_(edit_file,	"EDIT_FILE",	EDIT_H,	011001000),
	_TBL_(view_file,	"VIEW_FILE",	VIEW_H,	010010111),
#ifndef	_NOARCHIVE
	_TBL_(unpack_file,	"UNPACK_FILE",	UNPK_H,	011011000),
	_TBL_(pack_file,	"PACK_FILE",	PACK_H,	001011000),
#endif
#ifndef	_NOTREE
	_TBL_(tree_dir,		"LOG_TREE",	LGTR_H,	001101110),
	_TBL_(copy_tree,	"COPY_TREE",	CPTR_H,	011001110),
	_TBL_(move_tree,	"MOVE_TREE",	MVTR_H,	011001110),
#ifndef	_NOARCHIVE
	_TBL_(unpack_tree,	"UNPACK_TREE",	UPTR_H,	011011110),
#endif
#endif	/* !_NOTREE */
	_TBL_(find_dir,		"FIND_DIR",	FNDD_H,	011001110),
	_TBL_(symlink_mode,	"SYMLINK_MODE",	SYLN_H,	000100000),
	_TBL_(filetype_mode,	"FILETYPE_MODE",FLTY_H,	000110110),
	_TBL_(dotfile_mode,	"DOTFILE_MODE",	DTFL_H,	000101000),
	_TBL_(fileflg_mode,	"FILEFLG_MODE",	FLFL_H,	000100110),
#ifndef	_NOARCHIVE
	_TBL_(launch_file,	"LAUNCH_FILE",	LAUN_H,	010011111),
#endif
	_TBL_(search_forw,	"SEARCH_FORW",	SEAF_H,	000010000),
	_TBL_(search_back,	"SEARCH_BACK",	SEAB_H,	000010000),
#ifndef	_NOSPLITWIN
	_TBL_(split_window,	"SPLIT_WINDOW",	SPWN_H,	000101000),
	_TBL_(next_window,	"NEXT_WINDOW",	NXWN_H,	000111000),
#endif
#ifndef	_NOEXTRAWIN
	_TBL_(widen_window,	"WIDEN_WINDOW",	WDWN_H,	000111000),
	_TBL_(narrow_window,	"NARROW_WINDOW",NRWN_H,	000111000),
	_TBL_(kill_window,	"KILL_WINDOW",	KLWN_H,	000101000),
#endif
#ifndef	_NOCUSTOMIZE
	_TBL_(edit_config,	"EDIT_CONFIG",	EDCF_H,	000111110),
#endif
	_TBL_(help_message,	"HELP_MESSAGE",	HELP_H,	000110110),
	_TBL_(quit_system,	"QUIT_SYSTEM",	QUIT_H,	000111000),
	_TBL_(warning_bell,	"WARNING_BELL",	WARN_H,	000110000),
	_TBL_(no_operation,	"NO_OPERATION",	NOOP_H,	000110000)
};
