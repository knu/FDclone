/*
 *	Configuration File for FD
 */

#include "machine.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include "types.h"
#include "pathname.h"


#define	RUNCOMFILE	"~/.fdrc"
#define	HISTORYFILE	"~/.fd_history"
#define	CMDLINE_DELIM	"\t !&:;<>|"
#define	METACHAR	"\t \"#$&'()*:;<>?[\\]^`|~"

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
#define	ADJTTY		0
#define	COLUMNS		2
#define	MINFILENAME	12
#define	HISTSIZE	50
#define	SAVEHIST	50
#define	DIRCOUNTLIMIT	50
#define	SECOND		0
#define	DOSDRIVE	0
#define	SIZEINFO	0
#define	EDITMODE	"emacs"
#define	TMPDIR		"/tmp"
#define	RRPATH		""


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
#define	MAXCOMMSTR	1023
#define	MAXSTACK	5
#define	MAXARGS		10


/****************************************************************
 *	Screen layout parameter					*
 ****************************************************************/
#define	FILEPERLINE	(columns)
#define	FILEPERLOW	(n_line - WHEADER - WFOOTER)
#define	FILEPERPAGE	(FILEPERLINE * FILEPERLOW)

#define	WHEADERMIN	3
#define	WHEADERMAX	4
#define	WHEADER		(sizeinfo + WHEADERMIN)
#define	WFOOTER		3
#define	LTITLE		0
#define	LSIZE		1
#define	LSTATUS		(sizeinfo + 1)
#define	LPATH		(sizeinfo + 2)
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
#define	WMODE		10
#define	WOWNER		8
#define	WGROUP		8

#ifdef	UNKNOWNFS
#undef	WRITEFS
#define	WRITEFS		2
#endif
