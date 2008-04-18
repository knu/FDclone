/*
 *	fd.h
 *
 *	definitions for FD
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "machine.h"

#ifndef	FD
#define	FD			2
#endif
#ifndef	DEFRC
#define	DEFRC			"/etc/fd2rc"
#endif

#if	FD < 2
#define	_NOORIGSHELL
#define	_NOSPLITWIN
#define	_NOPRECEDE
#define	_NOCUSTOMIZE
#define	_NOEXTRACOPY
#define	_NOUSEHASH
#define	_NOORIGGLOB
#define	_NOKANJIFCONV
#define	_NOBROWSE
#define	_NOEXTRAMACRO
#define	_NOTRADLAYOUT
#define	_NOEXTRAWIN
#define	_NOPTY
#define	_NOEXTRAATTR
#define	_NOLOGGING
#define	_NOIME
#endif	/* FD < 2 */

#ifdef	DEBUG
extern VOID mtrace __P_ ((VOID_A));
extern VOID muntrace __P_ ((VOID_A));
extern char *_mtrace_file;
#endif

#if	MSDOS
# ifdef	BSPATHDELIM
# define	FREQFILE	"~\\fd.frq"
#  if	FD >= 2
#  define	FD_RCFILE	"~\\fd2.rc"
#  else
#  define	FD_RCFILE	"~\\fd.rc"
#  endif
# else	/* !BSPATHDELIM */
# define	FREQFILE	"~/fd.frq"
#  if	FD >= 2
#  define	FD_RCFILE	"~/fd2.rc"
#  else
#  define	FD_RCFILE	"~/fd.rc"
#  endif
# endif	/* !BSPATHDELIM */
#define	TMPPREFIX		"FD"
#define	ARCHTMPPREFIX		"AR"
#define	DOSTMPPREFIX		'D'
#else	/* !MSDOS */
#define	FREQFILE		"~/.fd_freq"
# if	FD >= 2
# define	FD_RCFILE	"~/.fd2rc"
# else
# define	FD_RCFILE	"~/.fdrc"
# endif
#define	TMPPREFIX		"fd"
#define	ARCHTMPPREFIX		"ar"
#define	DOSTMPPREFIX		'd'
#endif	/* !MSDOS */

#if	MSDOS && defined (_NOORIGSHELL)
/*	Using COMMAND.COM */
#define	SHELL_OPERAND		"|"
#define	CMDLINE_DELIM		"\t\n <>|"
#else
#define	SHELL_OPERAND		"&;(`|"
#define	CMDLINE_DELIM		"\t\n &;()<>|"
#endif

#define	FDPROG			"fd"
#define	FDSHELL			"fdsh"
#define	FDENV			"FD_"
#define	FDESIZ			strsize(FDENV)

/****************************************************************
 *	If you don't like the following tools as each uses,	*
 *	you should rewrite another suitable command name.	*
 ****************************************************************/
#define	PAGER			"more%K"	/* to view file */
#define	EDITOR			"vi"		/* to edit file */


/****************************************************************
 *	Default value in case if not defined by neither environ	*
 *	variables nor run_com file nor command line option	*
 ****************************************************************/
#define	BASICCUSTOM		0
#define	SORTTYPE		0
#define	DISPLAYMODE		0
#define	SORTTREE		0
#define	WRITEFS			0
#define	IGNORECASE		0
#define	INHERITCOPY		0
#define	PROGRESSBAR		0
#define	PRECOPYMENU		0
#define	ADJTTY			0
#define	USEGETCURSOR		0
#define	DEFCOLUMNS		2
#define	MINFILENAME		12
#if	MSDOS
# ifdef	BSPATHDELIM
# define	HISTFILE	"~\\fd.hst"
# else
# define	HISTFILE	"~/fd.hst"
# endif
#else	/* !MSDOS */
#define	HISTFILE		"~/.fd_history"
#endif	/* !MSDOS */
#define	DIRHISTFILE		""
#define	HISTSIZE		50
#define	DIRHIST			50
#define	SAVEHIST		50
#define	SAVEDIRHIST		50
#define	DIRCOUNTLIMIT		50
#define	DOSDRIVE		0
#define	SECOND			0
#define	TRADLAYOUT		0
#define	SIZEINFO		0
#define	FUNCLAYOUT		1005
#define	IMEKEY			-1
#define	IMEBUFFER		0
#define	ANSICOLOR		0
#define	ANSIPALETTE		""
#define	EDITMODE		"emacs"
#define	LOOPCURSOR		0
#if	MSDOS
#define	TMPDIR			"."
#else
#define	TMPDIR			"/tmp"
#endif
#define	TMPUMASK		022
#define	RRPATH			""
#define	PRECEDEPATH		""
#if	FD >= 2
#define	PROMPT			"$ "
#else
#define	PROMPT			"sh#"
#endif
#define	PROMPT2			"> "
#define	DUMBSHELL		0
#define	PTYMODE			0
#define	PTYTERM			"vt100"
#define	PTYMENUKEY		-1
#define	LOGFILE			""
#define	LOGSIZE			1024
#define	USESYSLOG		0
#define	LOGLEVEL		0
#define	ROOTLOGLEVEL		1
#define	THRUARGS		0
#define	UNICODEBUFFER		0
#define	SJISPATH		""
#define	EUCPATH			""
#define	JISPATH			""
#define	JIS8PATH		""
#define	JUNETPATH		""
#define	OJISPATH		""
#define	OJIS8PATH		""
#define	OJUNETPATH		""
#define	HEXPATH			""
#define	CAPPATH			""
#define	UTF8PATH		""
#define	UTF8MACPATH		""
#define	UTF8ICONVPATH		""
#define	NOCONVPATH		""


/****************************************************************
 *	Maximum value of some arrays (to change in compile)	*
 ****************************************************************/
#define	MAXBINDTABLE		256
#define	MAXMACROTABLE		64
#define	MAXLAUNCHTABLE		32
#define	MAXARCHIVETABLE		16
#define	MAXALIASTABLE		256
#define	MAXFUNCTABLE		32
#define	MAXFUNCLINES		16
#if	MSDOS
#define	MAXCOMMSTR		(128 - 2)
#endif
#define	MAXSELECTSTRS		16
#define	MAXSTACK		5
#ifdef	_NOEXTRAWIN
#define	MAXWINDOWS		2
#else
#define	MAXWINDOWS		5
#endif
#define	MAXHISTNO		MAXTYPE(short)
#define	MAXINVOKEARGS		MAXWINDOWS
#define	MAXLOGLEN		255

#ifdef	_NOSPLITWIN
#undef	MAXWINDOWS
#define	MAXWINDOWS	1
#else
# if	MAXWINDOWS <= 1
# define	_NOSPLITWIN
# endif
#endif

#ifdef	_NOSPLITWIN
#define	_NOEXTRAWIN
#endif

#if	MSDOS && defined (_NOUSELFN) && !defined (_NODOSDRIVE)
#define	_NODOSDRIVE
#endif

#if	defined (_NOENGMES) && defined (_NOJPNMES)
#undef	_NOENGMES
#endif

#if	MSDOS || defined (NOSELECT)
#define	_NOPTY
#endif

#ifndef	__FD_PRIMAL__
#include "types.h"
#include "printf.h"
#include "kctype.h"
#include "pathname.h"
#include "term.h"
#endif


/****************************************************************
 *	Screen layout parameter					*
 ****************************************************************/
#define	FILEPERLINE		(curcolumns)
#ifdef	_NOSPLITWIN
#define	fileperrow(w)		(n_line - wheader - WFOOTER)
#else
#define	fileperrow(w)		((n_line - wheader - WFOOTER + 1) / (w) - 1)
#endif
#define	FILEPERPAGE		(FILEPERLINE * FILEPERROW)

#ifdef	_NOTRADLAYOUT
#define	istradlayout()		(0)
#define	hassizeinfo()		(sizeinfo)
#else
#define	istradlayout()		(tradlayout && n_column >= WCOLUMNSTD)
#define	hassizeinfo()		(sizeinfo || istradlayout())
#endif
#define	WHEADERMIN		3
#define	WHEADERMAX		4
#define	WHEADER			(WHEADERMIN + ((hassizeinfo()) ? 1 : 0))
#define	WFOOTER			3
#define	L_TITLE			0
#define	L_SIZE			1
#define	L_STATUS		(sizeinfo + 1)
#define	L_PATH			(sizeinfo + 2)
#define	TL_SIZE			1
#define	TL_STATUS		3
#define	TL_PATH			2
#ifdef	HAVEFLAGS
#define	WMODELINE		2
#else
#define	WMODELINE		1
#endif
#define	WFILEMINTREE		3
#define	WFILEMINCUSTOM		4
#define	WFILEMINATTR		(WMODELINE + 5)
#define	WFILEMIN		1
#if	FD >= 2
#define	MAXHELPINDEX		20
#define	MAXSORTINHERIT		2
#else
#define	MAXHELPINDEX		10
#define	MAXSORTINHERIT		1
#endif
#define	MAXSORTTYPE		5
#define	L_STACK			(n_line - 3)
#define	L_HELP			(n_line - 2)
#define	L_INFO			(n_line - 1)
#define	L_CMDLINE		(n_line - 2)
#define	WCMDLINE		2
#define	L_MESLINE		(n_line - 1)
#define	WCOLUMNSTD		80
#define	WCOLUMNOMIT		58
#define	WCOLUMNHARD		42
#define	WCOLUMNMIN		34
#define	S_BYTES			" bytes"
#define	S_KBYTES		" KB"
#define	S_MBYTES		" MB"
#define	W_BYTES			strsize(S_BYTES)
#define	W_KBYTES		strsize(S_KBYTES)
#define	W_MBYTES		strsize(S_MBYTES)
#define	S_PAGE			"Page:"
#define	S_MARK			"Mark:"
#define	S_INFO			""
#define	S_SORT			"Sort:"
#define	S_FIND			"Find:"
#define	S_PATH			"Path:"
#define	S_BROWSE		"Browse:"
#define	S_ARCH			"Arch:"
#define	S_SIZE			"Size:"
#define	S_TOTAL			"Total:"
#define	S_USED			""
#define	S_FREE			"Free:"
#define	TS_PAGE			"Page:"
#define	TS_MARK			"Marked"
#define	TS_INFO			"Info:"
#define	TS_SORT			""
#define	TS_FIND			""
#define	TS_PATH			"Path="
#define	TS_BROWSE		"Browse="
#define	TS_ARCH			"Arch="
#define	TS_SIZE			"Files "
#define	TS_TOTAL		"Total:"
#define	TS_USED			"Used:"
#define	TS_FREE			"Free:"
#define	W_PAGE			strsize(S_PAGE)
#define	W_MARK			strsize(S_MARK)
#define	W_INFO			strsize(S_INFO)
#define	W_SORT			strsize(S_SORT)
#define	W_FIND			strsize(S_FIND)
#define	W_PATH			strsize(S_PATH)
#define	W_BROWSE		strsize(S_BROWSE)
#define	W_ARCH			strsize(S_ARCH)
#define	W_SIZE			strsize(S_SIZE)
#define	W_TOTAL			strsize(S_TOTAL)
#define	W_USED			strsize(S_USED)
#define	W_FREE			strsize(S_FREE)
#define	TW_PAGE			strsize(TS_PAGE)
#define	TW_MARK			strsize(TS_MARK)
#define	TW_INFO			strsize(TS_INFO)
#define	TW_SORT			strsize(TS_SORT)
#define	TW_FIND			strsize(TS_FIND)
#define	TW_PATH			strsize(TS_PATH)
#define	TW_BROWSE		strsize(TS_BROWSE)
#define	TW_ARCH			strsize(TS_ARCH)
#define	TW_SIZE			strsize(TS_SIZE)
#define	TW_TOTAL		strsize(TS_TOTAL)
#define	TW_USED			strsize(TS_USED)
#define	TW_FREE			strsize(TS_FREE)
#define	D_PAGE			2
#define	D_MARK			4
#define	D_INFO			0
#define	D_SORT			14
#define	D_FIND			(n_column - C_FIND - W_FIND)
#define	D_PATH			(n_column - C_PATH - W_PATH)
#define	D_BROWSE		(n_column - C_PATH - W_BROWSE)
#define	D_ARCH			(n_column - C_PATH - W_ARCH)
#define	D_SIZE			14
#define	D_TOTAL			15
#define	D_USED			0
#define	D_FREE			15
#define	TD_PAGE			2
#define	TD_MARK			4
#define	TD_INFO			(TC_SIZE - TC_INFO - TW_INFO)
#define	TD_SORT			0
#define	TD_FIND			0
#define	TD_PATH			(TC_MARK - TC_PATH - TW_PATH)
#define	TD_BROWSE		(TC_MARK - TC_PATH - TW_BROWSE)
#define	TD_ARCH			(TC_MARK - TC_PATH - TW_ARCH)
#define	TD_SIZE			13
#define	TD_TOTAL		14
#define	TD_USED			13
#define	TD_FREE			14
#define	C_PAGE			((isleftshift()) ? 0 : 1)
#define	C_MARK			(C_PAGE + W_PAGE + D_PAGE + 1 + D_PAGE + 1)
#define	C_INFO			-1
#define	C_SORT			(C_MARK + W_MARK + D_MARK + 1 + D_MARK + 1)
#define	C_FIND			(C_SORT + \
				((ishardomit()) ? 0 : W_SORT + D_SORT + 1))
#define	C_PATH			((isleftshift()) ? 0 : 1)
#define	C_SIZE			((isleftshift()) ? 0 : 1)
#define	C_TOTAL			(C_SIZE + W_SIZE + D_SIZE + 1 + D_SIZE + 2)
#define	C_USED			-1
#define	C_FREE			(C_TOTAL + W_TOTAL + D_TOTAL + 1)
#define	TC_PAGE			2
#define	TC_MARK			(n_column - TD_MARK - TW_MARK - TD_SIZE - 2)
#define	TC_INFO			2
#define	TC_SORT			-1
#define	TC_FIND			-1
#define	TC_PATH			2
#define	TC_SIZE			(n_column - TD_MARK - TW_SIZE - TD_SIZE - 2)
#define	TC_TOTAL		(TC_PAGE + TW_PAGE + TD_PAGE + 1 + TD_PAGE + 3)
#define	TC_USED			(TC_TOTAL + TW_TOTAL + TD_TOTAL + 3)
#define	TC_FREE			(TC_USED + TW_USED + TD_USED + 3)

#define	WSIZE			9
#define	WSIZE2			8
#define	TWSIZE2			10
#define	WDATE			8
#define	WTIME			5
#define	WSECOND			2
#if	MSDOS
#define	WMODE			5
#else
#define	WMODE			10
#endif
#define	TWMODE			9
#define	WNLINK			2
#ifndef	NOUID
#define	WOWNER			8
#define	WGROUP			8
#define	WOWNERMIN		5
#define	WGROUPMIN		5
#endif


/****************************************************************
 *	Restrictions for causation				*
 ****************************************************************/
#ifdef	UNKNOWNFS
#undef	WRITEFS
#define	WRITEFS			2
#endif

#if	MSDOS
#define	_NOKEYMAP
#endif

#ifdef	_NOORIGSHELL
#define	_NOEXTRAMACRO
#endif

#if	!MSDOS && !defined (_NODOSDRIVE)
#define	_USEDOSEMU
#endif

#if	MSDOS || !defined (_NODOSDRIVE)
#define	_USEDOSPATH
#endif

#if	!defined (_NOKANJICONV) && !defined (_NOUNICODE) \
|| !defined (_NODOSDRIVE)
#define	_USEUNICODE
#endif

#if	MSDOS
#define	_USEDOSCOPY
#endif

#ifdef	_NOKANJICONV
#define	_NOIME
#endif
