/*
 *	Configuration File for FD
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include "machine.h"
#include "types.h"
#include "pathname.h"
#include "func.h"

#ifndef	NOSTDLIB
#include <stdlib.h>
#endif


#define	RUNCOMFILE	"~/.fdrc"
#define	CMDLINE_DELIM	" \t|><&!;"

/****************************************************************
 *	If you need to fix following tools, you should		*
 *	comment out below and define suitable command name.	*
 ****************************************************************/
/* #define PAGER	"/usr/local/bin/less"	/* to view file */
/* #define EDITOR	"/usr/ucb/vi"		/* to edit file */


/****************************************************************
 *	Default value in case if not defined by neither environ	*
 *	variables nor run_com file nor command line option	*
 ****************************************************************/
#define	TMPDIR		"/tmp"
#define	HISTSIZE	50
#define	DIRCOUNTLIMIT	50
#define	SORTTYPE	0
#define	WRITEFS		0
#define	ADJTTY		0


/****************************************************************
 *	Maximum value of some arrays (to change in compile)	*
 ****************************************************************/
#define	MAXBINDTABLE	256
#define	MAXMACROTABLE	64
#define	MAXLAUNCHTABLE	32
#define	MAXARCHIVETABLE	16
#define	MAXLINESTR	255
#define	MAXCOMMSTR	1023
#define	MAXSTACK	5


/****************************************************************
 *	Screen layout parameter					*
 ****************************************************************/
#define	FILEPERLINE	(columns)
#define	FILEPERLOW	(n_line - WHEADER - WFOOTER)
#define	FILEPERPAGE	(FILEPERLINE * FILEPERLOW)

#define	WHEADER		3
#define	WFOOTER		3
#define	LTITLE		0
#define	LSTATUS		1
#define	LPATH		2
#define	LSTACK		(n_line - 3)
#define	LHELP		(n_line - 2)
#define	LINFO		(n_line - 1)
#define	LCMDLINE	(n_line - 2)
#define	WCMDLINE	2
#define	LMESLINE	(n_line - 1)
#define	CFIND		47

#define	WSIZE		9
#define	WDATE		8
#define	WTIME		5
#define	WSECOND		2
#define	WMODE		10
#define	WOWNER		8
#define	WGROUP		8
