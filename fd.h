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

#define	_NOORIGSHELL
#define	_NOSPLITWIN
#define	_NOPRECEDE
#define	_NOCUSTOMIZE
#define	_NOEXTRACOPY
#define	_NOUSEHASH
#define	_NOORIGGLOB
#define	_NOKANJIFCONV

#include "types.h"
#include "pathname.h"

#ifdef	DEBUG
extern char *_mtrace_file;
#endif


#if	MSDOS
#define	RUNCOMFILE	"~\\fd.rc"
#define	HISTORYFILE	"~\\fd.hst"
#else
#define	RUNCOMFILE	"~/.fdrc"
#define	HISTORYFILE	"~/.fd_history"
#endif

#if	MSDOS && defined (_NOORIGSHELL)
#define	SHELL_OPERAND	"|"
#define	CMDLINE_DELIM	"\t ;<>|"
#else
#define	SHELL_OPERAND	"&;(`|"
#define	CMDLINE_DELIM	"\t &;()<>`|"
#endif

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
#define	COLUMNS		2
#define	MINFILENAME	12
#define	HISTSIZE	50
#define	DIRHIST		50
#define	SAVEHIST	50
#define	DIRCOUNTLIMIT	50
#define	SECOND		0
#define	DOSDRIVE	0
#define	SIZEINFO	0
#define	ANSICOLOR	0
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
#define	PROMPT		"sh#"
#define	PROMPT2		"> "


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
#define	MAXLINESTR	255
#if	MSDOS
#define	MAXCOMMSTR	(128 - 2)
#else
#define	MAXCOMMSTR	1023
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
#define	FILEPERLINE	(columns)
#ifndef	_NOSPLITWIN
#define	FILEPERLOW	((n_line - WHEADER - WFOOTER + 1) / windows - 1)
#else
#define	FILEPERLOW	(n_line - WHEADER - WFOOTER)
#endif
#define	FILEPERPAGE	(FILEPERLINE * FILEPERLOW)

#define	WHEADERMIN	3
#define	WHEADERMAX	4
#define	WHEADER		(sizeinfo + WHEADERMIN)
#define	WFOOTER		3
#define	LTITLE		0
#define	LSIZE		1
#define	LSTATUS		(sizeinfo + 1)
#define	LPATH		(sizeinfo + 2)
#ifndef	_NOSPLITWIN
#define	WFILEMIN	(MAXWINDOWS * (WMODELINE + 6) - 1)
#define	LFILETOP	(WHEADER + (win * (FILEPERLOW + 1)))
#else
#define	WFILEMIN	2
#define	LFILETOP	(WHEADER)
#endif
#define	LFILEBOTTOM	(LFILETOP + FILEPERLOW)
#define	LSTACK		(n_line - 3)
#define	LHELP		(n_line - 2)
#define	LINFO		(n_line - 1)
#define	LCMDLINE	(n_line - 2)
#define	WCMDLINE	2
#define	LMESLINE	(n_line - 1)
#define	CPAGE		1
#define	CMARK		12
#define	CSORT		27
#define	CFIND		47
#define	CSIZE		1
#define	CTOTAL		37
#define	CFREE		59

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
#endif

#ifdef	HAVEFLAGS
#define	WMODELINE	2
#else
#define	WMODELINE	1
#endif

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
