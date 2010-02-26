/*
 *	types.h
 *
 *	type definitions
 */

#include "depend.h"
#include "typesize.h"
#include "stream.h"
#include "lsparse.h"

typedef struct _strtable {
	u_short no;
	CONST char *str;
} strtable;

typedef struct _lockbuf_t {
	int fd;
	XFILE *fp;
	char *name;
	u_char flags;
} lockbuf_t;

#define	LCK_FLOCK		0001
#define	LCK_INVALID		0002
#define	LCK_STREAM		0004
#define	LCK_MAXRETRY		32

#ifdef	DEP_DYNAMICLIST
typedef u_short			funcno_t;
#else
typedef u_char			funcno_t;
#endif

typedef struct _bindtable {
	short key;
	funcno_t f_func;
	funcno_t d_func;
} bindtable;

#define	FNO_NONE		MAXUTYPE(funcno_t)
#define	FNO_SETMACRO		(MAXUTYPE(funcno_t) - 1)
#define	ffunc(n)		(bindlist[n].f_func)
#define	dfunc(n)		(bindlist[n].d_func)
#define	hasdfunc(n)		(dfunc(n) != FNO_NONE)

typedef struct _functable {
	int (*func)__P_((CONST char *));
	CONST char *ident;
#ifdef	_NOCATALOG
# ifndef	_NOJPNMES
	CONST char *hmes;
# endif
# ifndef	_NOENGMES
	CONST char *hmes_eng;
# endif
#else	/* !_NOCATALOG */
	int hmes_no;
#endif	/* !_NOCATALOG */
	u_char status;
} functable;

#define	FN_REWRITE		0001
#define	FN_RELIST		0002
#define	FN_REWIN		0003
#define	FN_REWRITEMODE		0003
#define	FN_RESCREEN		0004
#define	FN_KILLSTACK		0010
#define	FN_ARCHIVE		0020
#define	FN_NOFILE		0040
#define	FN_RESTRICT		0100
#define	FN_NEEDSTATUS		0200
#define	rewritemode(n)		((n) & FN_REWRITEMODE)

#ifndef	_NOARCHIVE
typedef struct _archive_t {
	char *ext;
	char *p_comm;
	char *u_comm;
	u_char flags;
} archive_t;

#define	AF_IGNORECASE		0001	/* must be the same as LF_IGNORECASE */
#endif

#ifndef	_NOTREE
typedef struct _treelist {
	char *name;
	int max;
# ifndef	NODIRLOOP
	dev_t dev;
	ino_t ino;
	struct _treelist *parent;
# endif
	struct _treelist *sub;
} treelist;
#endif	/* !_NOTREE */

typedef struct _winvartable {
#ifndef	_NOARCHIVE
	struct _winvartable *v_archduplp;
	char *v_archivedir;
	char *v_archivefile;
	char *v_archtmpdir;
	lsparse_t *v_launchp;
	namelist *v_arcflist;
	int v_maxarcf;
# ifdef	DEP_PSEUDOPATH
	int v_archdrive;
# endif
# ifndef	_NOBROWSE
	lsparse_t *v_browselist;
	int v_browselevel;
# endif
#endif	/* !_NOARCHIVE */
#ifndef	_NOTREE
	char *v_treepath;
#endif
	char *v_fullpath;
	char *v_findpattern;
	namelist *v_filelist;
	int v_maxfile;
	int v_maxent;
	int v_filepos;
	int v_sorton;
	int v_dispmode;
	int v_fileperrow;
} winvartable;

extern winvartable winvar[];
#ifdef	_NOSPLITWIN
#define	windows			1
#else
extern int windows;
#endif
#if	defined (_NOSPLITWIN) && !defined (DEP_PTY)
#define	win			0
#else
extern int win;
#endif
#ifndef	_NOARCHIVE
#define	archduplp		(winvar[win].v_archduplp)
#define	archivefile		(winvar[win].v_archivefile)
#define	archtmpdir		(winvar[win].v_archtmpdir)
#define	launchp			(winvar[win].v_launchp)
#define	arcflist		(winvar[win].v_arcflist)
#define	maxarcf			(winvar[win].v_maxarcf)
# ifdef	DEP_PSEUDOPATH
# define	archdrive	(winvar[win].v_archdrive)
# endif
# ifndef	_NOBROWSE
# define	browselist	(winvar[win].v_browselist)
# define	browselevel	(winvar[win].v_browselevel)
# endif
#endif	/* !_NOARCHIVE */
#ifndef	_NOTREE
#define	treepath		(winvar[win].v_treepath)
#endif
#define	findpattern		(winvar[win].v_findpattern)
#define	filelist		(winvar[win].v_filelist)
#define	maxfile			(winvar[win].v_maxfile)
#define	maxent			(winvar[win].v_maxent)
#define	filepos			(winvar[win].v_filepos)
#define	sorton			(winvar[win].v_sorton)
#define	dispmode		(winvar[win].v_dispmode)
#define	FILEPERROW		(winvar[win].v_fileperrow)

typedef struct _macrostat {
	short addopt;
	short needburst;
	short needmark;
	u_short flags;
} macrostat;

#define	F_NOCONFIRM		0000001
#define	F_ARGSET		0000002
#define	F_REMAIN		0000004
#define	F_NOEXT			0000010
#define	F_TOSFN			0000020
#define	F_ISARCH		0000040
#define	F_BURST			0000100
#define	F_MARK			0000200
#define	F_NOADDOPT		0000400
#define	F_IGNORELIST		0001000
#define	F_NOCOMLINE		0002000
#define	F_NOKANJICONV		0004000
#define	F_TTYIOMODE		0010000
#define	F_TTYNL			0020000
#define	F_EVALMACRO		0040000
#define	F_DOSYSTEM		0100000

#ifndef	DEP_ORIGSHELL
typedef struct _aliastable {
	char *alias;
	char *comm;
} aliastable;

typedef struct _userfunctable {
	char *func;
	char **comm;
} userfunctable;
#endif	/* !DEP_ORIGSHELL */

typedef struct _builtintable {
	int (NEAR *func)__P_((int, char *CONST []));
	char *ident;
} builtintable;

#ifdef	DEP_DYNAMICLIST
typedef bindtable *		bindlist_t;
typedef CONST bindtable		origbindlist_t[];
typedef lsparse_t *		launchlist_t;
typedef CONST lsparse_t		origlaunchlist_t[];
typedef archive_t *		archivelist_t;
typedef CONST archive_t		origarchivelist_t[];
typedef char **			macrolist_t;
typedef char **			helpindex_t;
typedef char *			orighelpindex_t[];
#else
typedef bindtable		bindlist_t[MAXBINDTABLE];
typedef bindtable *		origbindlist_t;
typedef lsparse_t		launchlist_t[MAXLAUNCHTABLE];
typedef lsparse_t *		origlaunchlist_t;
typedef archive_t		archivelist_t[MAXARCHIVETABLE];
typedef archive_t *		origarchivelist_t;
typedef char *			macrolist_t[MAXMACROTABLE];
typedef char *			helpindex_t[MAXHELPINDEX];
typedef char **			orighelpindex_t;
#endif

#define	F_SYMLINK		001
#define	F_FILETYPE		002
#define	F_DOTFILE		004
#define	F_FILEFLAG		010

#define	isdisptyp(n)		((n) & F_FILETYPE)
#define	ishidedot(n)		((n) & F_DOTFILE)
#ifdef	_NOARCHIVE
#define	isdisplnk(n)		((n) & F_SYMLINK)
#define	isfileflg(n)		((n) & F_FILEFLAG)
#else
#define	isdisplnk(n)		(!archivefile && ((n) & F_SYMLINK))
#define	isfileflg(n)		(!archivefile && ((n) & F_FILEFLAG))
#endif

#define	FNC_NONE		0
#define	FNC_CANCEL		1
#define	FNC_UPDATE		2
#define	FNC_HELPSPOT		3
#define	FNC_EFFECT		4
#define	FNC_CHDIR		5
#define	FNC_QUIT		(-1)
#define	FNC_FAIL		(-2)

#define	FSID_UFS		1
#define	FSID_EFS		2
#define	FSID_SYSV		3
#define	FSID_LINUX		4
#define	FSID_FAT		5
#define	FSID_LFN		6
#define	FSID_DOSDRIVE		7

#define	CHK_OK			0
#define	CHK_EXIST		1
#define	CHK_OVERWRITE		2
#define	CHK_ERROR		(-1)
#define	CHK_CANCEL		(-2)

#define	APL_OK			0
#define	APL_IGNORE		1
#define	APL_ERROR		(-1)
#define	APL_CANCEL		(-2)

#define	CPP_UPDATE		1
#define	CPP_RENAME		2
#define	CPP_OVERWRITE		3
#define	CPP_NOCOPY		4
#define	CPP_FORWUPDATE		5
#define	CPP_FORWOVERWRITE	6

#define	RMP_BIAS		2
#define	RMP_REMOVEALL		(RMP_BIAS + CHK_OK)
#define	RMP_KEEPALL		(RMP_BIAS + CHK_ERROR)

#define	TCH_MODE		00001
#define	TCH_UID			00002
#define	TCH_GID			00004
#define	TCH_ATIME		00010
#define	TCH_MTIME		00020
#define	TCH_FLAGS		00040
#define	TCH_CHANGE		00100
#define	TCH_MASK		00200
#define	TCH_MODEEXE		00400
#define	TCH_IGNOREERR		01000

#define	ATR_EXCLUSIVE		3
#define	ATR_MODEONLY		1
#define	ATR_TIMEONLY		2
#define	ATR_OWNERONLY		3
#define	ATR_MULTIPLE		4
#define	ATR_RECURSIVE		8

#define	ORD_NODIR		0
#define	ORD_NORMAL		1
#define	ORD_LOWER		2
#define	ORD_NOPREDIR		3

#define	HST_TYPE		0007
#define	HST_COMM		0000
#define	HST_PATH		0001
#define	HST_USER		0002
#define	HST_GROUP		0003
#define	HST_UNIQ		0010
#define	nohist(n)		((n) != HST_COMM && (n) != HST_PATH)
#define	completable(n)		((n) >= 0)
