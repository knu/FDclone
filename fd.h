/*
 *	fd.h
 *
 *	Configuration File for FD
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "machine.h"

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
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
#define	_NOEXTRAMAXRO
#endif	/* FD < 2 */

#include "pathname.h"
#include "types.h"

#ifdef	DEBUG
extern VOID mtrace __P_ ((VOID));
extern VOID muntrace __P_ ((VOID));
extern char *_mtrace_file;
#endif


#if	MSDOS
# if	FD >= 2
# define	RUNCOMFILE	"~\\fd2.rc"
# else
# define	RUNCOMFILE	"~\\fd.rc"
# endif
#define	HISTORYFILE	"~\\fd.hst"
#define	TMPPREFIX	"FD"
#define	ARCHTMPPREFIX	"AR"
#define	DOSTMPPREFIX	'D'
#else
# if	FD >= 2
# define	RUNCOMFILE	"~/.fd2rc"
# else
# define	RUNCOMFILE	"~/.fdrc"
# endif
#define	HISTORYFILE	"~/.fd_history"
#define	TMPPREFIX	"fd"
#define	ARCHTMPPREFIX	"ar"
#define	DOSTMPPREFIX	'd'
#endif

#if	MSDOS && defined (_NOORIGSHELL)
/*	Using COMMAND.COM */
#define	SHELL_OPERAND	"|"
#define	CMDLINE_DELIM	"\t\n <>|"
#else
#define	SHELL_OPERAND	"&;(`|"
#define	CMDLINE_DELIM	"\t\n &;()<>|"
#endif

#define	FDSHELL		"fdsh"

/****************************************************************
 *	If you don't like the following tools as each uses,	*
 *	you should rewrite another suitable command name.	*
 ****************************************************************/
#define	PAGER		"more%K"	/* to view file */
#define	EDITOR		"vi"		/* to edit file */


/****************************************************************
 *	Default value in case if not defined by neither environ	*
 *	variables nor run_com file nor command line option	*
 ****************************************************************/
#define	SORTTYPE	0
#define	DISPLAYMODE	0
#define	SORTTREE	0
#define	WRITEFS		0
#define	IGNORECASE	0
#define	INHERITCOPY	0
#define	ADJTTY		0
#define	USEGETCURSOR	0
#define	WINDOWS		1
#define	DEFCOLUMNS	2
#define	MINFILENAME	12
#define	HISTSIZE	50
#define	DIRHIST		50
#define	SAVEHIST	50
#define	DIRCOUNTLIMIT	50
#define	SECOND		0
#define	DOSDRIVE	0
#define	SIZEINFO	0
#define	ANSICOLOR	0
#define	ANSIPALETTE	""
#define	EDITMODE	"emacs"
#if	MSDOS
#define	TMPDIR		"."
#else
#define	TMPDIR		"/tmp"
#endif
#define	RRPATH		""
#define	PRECEDEPATH	""
#define	SJISPATH	""
#define	EUCPATH		""
#define	JISPATH		""
#define	JIS8PATH	""
#define	JUNETPATH	""
#define	OJISPATH	""
#define	OJIS8PATH	""
#define	OJUNETPATH	""
#define	HEXPATH		""
#define	CAPPATH		""
#define	UTF8PATH	""
#define	NOCONVPATH	""
#if	FD >= 2
#define	PROMPT		"$ "
#else
#define	PROMPT		"sh#"
#endif
#define	PROMPT2		"> "
#define	TMPUMASK	022


/****************************************************************
 *	Maximum value of some arrays (to change in compile)	*
 ****************************************************************/
#define	MAXBINDTABLE	256
#define	MAXMACROTABLE	64
#define	MAXLAUNCHTABLE	32
#define	MAXARCHIVETABLE	16
#define	MAXALIASTABLE	256
#define	MAXFUNCTABLE	32
#define	MAXFUNCLINES	16
#if	MSDOS
#define	MAXCOMMSTR	(128 - 2)
#endif
#define	MAXSELECTSTRS	16
#define	MAXSTACK	5
#define	MAXWINDOWS	2

#ifdef	_NOSPLITWIN
#undef	MAXWINDOWS
#define	MAXWINDOWS	1
#else
# if	MAXWINDOWS <= 1
# define	_NOSPLITWIN
# endif
#endif


/****************************************************************
 *	Screen layout parameter					*
 ****************************************************************/
#define	FILEPERLINE	(curcolumns)
#ifndef	_NOSPLITWIN
#define	FILEPERROW	((n_line - WHEADER - WFOOTER + 1) / windows - 1)
#else
#define	FILEPERROW	(n_line - WHEADER - WFOOTER)
#endif
#define	FILEPERPAGE	(FILEPERLINE * FILEPERROW)

#define	WHEADERMIN	3
#define	WHEADERMAX	4
#define	WHEADER		(sizeinfo + WHEADERMIN)
#define	WFOOTER		3
#define	L_TITLE		0
#define	L_SIZE		1
#define	L_STATUS	(sizeinfo + 1)
#define	L_PATH		(sizeinfo + 2)
#ifdef	HAVEFLAGS
#define	WMODELINE	2
#else
#define	WMODELINE	1
#endif
#define	WFILEMINTREE	3
#define	WFILEMINCUSTOM	4
#define	WFILEMINATTR	(WMODELINE + 5)
#define	WFILEMIN	1
#ifndef	_NOSPLITWIN
#define	LFILETOP	(WHEADER + (win * (FILEPERROW + 1)))
#else
#define	LFILETOP	(WHEADER)
#endif
#define	LFILEBOTTOM	(LFILETOP + FILEPERROW)
#define	L_STACK		(n_line - 3)
#define	L_HELP		(n_line - 2)
#define	L_INFO		(n_line - 1)
#define	L_CMDLINE	(n_line - 2)
#define	WCMDLINE	2
#define	L_MESLINE	(n_line - 1)
#define	WCOLUMNSTD	80
#define	WCOLUMNOMIT	58
#define	WCOLUMNHARD	42
#define	WCOLUMNMIN	34
#define	C_PAGE		1
#define	C_MARK		12
#define	C_SORT		27
#define	C_FIND		47
#define	C_SIZE		1
#define	C_TOTAL		37
#define	C_FREE		59

#define	WSIZE		9
#define	WDATE		8
#define	WTIME		5
#define	WSECOND		2
#if	MSDOS
#define	WMODE		5
#else
#define	WMODE		10
#define	WOWNER		8
#define	WGROUP		8
#define	WOWNERMIN	5
#define	WGROUPMIN	5
#endif


/****************************************************************
 *	Restrictions for causation				*
 ****************************************************************/
#ifdef	UNKNOWNFS
#undef	WRITEFS
#define	WRITEFS		2
#endif

#if	MSDOS && defined (_NOUSELFN) && !defined (_NODOSDRIVE)
#define	_NODOSDRIVE
#endif

#if	defined (_NOENGMES) && defined (_NOJPNMES)
#undef	_NOENGMES
#endif

#if	defined (_NOKANJICONV) && !defined (_NOKANJIFCONV)
#define	_NOKANJIFCONV
#endif

#ifdef	_NOORIGSHELL
#define	_NOEXTRAMACRO
#endif
