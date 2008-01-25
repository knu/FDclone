/*
 *	types.h
 *
 *	type definitions
 */

#ifndef	__SYS_TYPES_STAT_H_
#define	__SYS_TYPES_STAT_H_
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USEDIRECT
#define	dirent	direct
#endif

#ifdef	NOFILEMODE
#undef	S_IRUSR
#undef	S_IWUSR
#undef	S_IXUSR
#undef	S_IRGRP
#undef	S_IWGRP
#undef	S_IXGRP
#undef	S_IROTH
#undef	S_IWOTH
#undef	S_IXOTH
#define	S_IRUSR	00400
#define	S_IWUSR	00200
#define	S_IXUSR	00100
#define	S_IRGRP	00040
#define	S_IWGRP	00020
#define	S_IXGRP	00010
#define	S_IROTH	00004
#define	S_IWOTH	00002
#define	S_IXOTH	00001
#endif	/* NOFILEMODE */
#define	S_IREAD_ALL	(S_IRUSR | S_IRGRP | S_IROTH)
#define	S_IWRITE_ALL	(S_IWUSR | S_IWGRP | S_IWOTH)
#define	S_IEXEC_ALL	(S_IXUSR | S_IXGRP | S_IXOTH)

#if	!MSDOS && defined (UF_SETTABLE) && defined (SF_SETTABLE)
#define	HAVEFLAGS
# ifndef	UF_NODUMP
# define	UF_NODUMP	0x00000001
# endif
# ifndef	UF_IMMUTABLE
# define	UF_IMMUTABLE	0x00000002
# endif
# ifndef	UF_APPEND
# define	UF_APPEND	0x00000004
# endif
# ifndef	UF_NOUNLINK
# define	UF_NOUNLINK	0x00000010
# endif
# ifndef	SF_ARCHIVED
# define	SF_ARCHIVED	0x00010000
# endif
# ifndef	SF_IMMUTABLE
# define	SF_IMMUTABLE	0x00020000
# endif
# ifndef	SF_APPEND
# define	SF_APPEND	0x00040000
# endif
# ifndef	SF_NOUNLINK
# define	SF_NOUNLINK	0x00080000
# endif
#endif	/* !MSDOS && UF_SETTABLE && SF_SETTABLE */

typedef struct _namelist {
	char *name;
	u_short ent;
	u_short st_mode;
	short st_nlink;
#ifndef	NOUID
	uid_t st_uid;
	gid_t st_gid;
#endif
#if	!defined (NOSYMLINK) && !defined (_NOARCHIVE)
	char *linkname;
#endif
#ifdef	HAVEFLAGS
	u_long st_flags;
#endif
	off_t st_size;
	time_t st_mtim;
	u_char flags;
	u_char tmpflags;
} namelist;

#define	F_ISEXE			0001
#define	F_ISWRI			0002
#define	F_ISRED			0004
#define	F_ISDIR			0010
#define	F_ISLNK			0020
#define	F_ISDEV			0040
#define	F_ISMRK			0001
#define	F_WSMRK			0002
#define	F_ISARG			0004
#define	F_STAT			0010
#define	F_ISCHGDIR		0020
#define	isdir(file)		((file) -> flags & F_ISDIR)
#define	islink(file)		((file) -> flags & F_ISLNK)
#define	isdev(file)		((file) -> flags & F_ISDEV)
#define	isfile(file)		(!((file) -> flags & (F_ISDIR | F_ISDEV)))
#define	isread(file)		((file) -> flags & F_ISRED)
#define	iswrite(file)		((file) -> flags & F_ISWRI)
#define	isexec(file)		((file) -> flags & F_ISEXE)
#define	ismark(file)		((file) -> tmpflags & F_ISMRK)
#define	wasmark(file)		((file) -> tmpflags & F_WSMRK)
#define	isarg(file)		((file) -> tmpflags & F_ISARG)
#define	havestat(file)		((file) -> tmpflags & F_STAT)
#define	ischgdir(file)		((file) -> tmpflags & F_ISCHGDIR)
#define	s_isdir(s)		((((s) -> st_mode) & S_IFMT) == S_IFDIR)
#define	s_isreg(s)		((((s) -> st_mode) & S_IFMT) == S_IFREG)
#define	s_islnk(s)		((((s) -> st_mode) & S_IFMT) == S_IFLNK)
#define	s_isfifo(s)		((((s) -> st_mode) & S_IFMT) == S_IFIFO)

typedef struct _strtable {
	u_short no;
	CONST char *str;
} strtable;

typedef struct _lockbuf_t {
	int fd;
	FILE *fp;
	char *name;
	u_char flags;
} lockbuf_t;

#define	LCK_FLOCK		0001
#define	LCK_INVALID		0002
#define	LCK_STREAM		0004
#define	LCK_MAXRETRY		32

typedef struct _bindtable {
	short key;
	u_char f_func;
	u_char d_func;
} bindtable;

#define	FNO_NONE		((u_char)255)
#define	FNO_SETMACRO		((u_char)254)
#define	ffunc(n)		(bindlist[n].f_func)
#define	dfunc(n)		(bindlist[n].d_func)
#define	hasdfunc(n)		(dfunc(n) != FNO_NONE)

typedef struct _functable {
	int (*func)__P_((CONST char *));
	CONST char *ident;
#ifndef	_NOJPNMES
	CONST char *hmes;
#endif
#if	!defined (_NOENGMES) || defined (_NOJPNMES)
	CONST char *hmes_eng;
#endif
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

#if	defined (FD) && (FD < 2) && !defined (OLDPARSE)
#define	OLDPARSE
#endif

#ifndef	_NOARCHIVE
#define	MAXLAUNCHFIELD		9
#define	MAXLAUNCHSEP		3
typedef struct _launchtable {
	char *ext;
	char *comm;
# ifndef	OLDPARSE
	char **format;
	char **lignore;
	char **lerror;
# endif
	u_char topskip;
	u_char bottomskip;
# ifdef	OLDPARSE
	u_char field[MAXLAUNCHFIELD];
	u_char delim[MAXLAUNCHFIELD];
	u_char width[MAXLAUNCHFIELD];
	u_char sep[MAXLAUNCHSEP];
	u_char lines;
# endif
	u_char flags;
} launchtable;

#define	F_MODE			0
#define	F_UID			1
#define	F_GID			2
#define	F_SIZE			3
#define	F_YEAR			4
#define	F_MON			5
#define	F_DAY			6
#define	F_TIME			7
#define	F_NAME			8
#define	LF_IGNORECASE		0001
#define	LF_DIRLOOP		0002
#define	LF_DIRNOPREP		0004
#define	LF_FILELOOP		0010
#define	LF_FILENOPREP		0020
#define	SKP_NONE		((u_char)255)
#define	FLD_NONE		((u_char)255)
#define	SEP_NONE		((u_char)255)

typedef struct _archivetable {
	char *ext;
	char *p_comm;
	char *u_comm;
	u_char flags;
} archivetable;

#define	AF_IGNORECASE		0001	/* must be the same as LF_IGNORECASE */
#endif	/* !_NOARCHIVE */

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
	launchtable *v_launchp;
	namelist *v_arcflist;
	int v_maxarcf;
# if	(!MSDOS || !defined (_NOUSELFN)) && !defined (_NODOSDRIVE)
	int v_archdrive;
# endif
# ifndef	_NOBROWSE
	launchtable *v_browselist;
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
#if	defined (_NOSPLITWIN) && defined (_NOPTY)
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
# if	(!MSDOS || !defined (_NOUSELFN)) && !defined (_NODOSDRIVE)
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

#ifdef	_NOORIGSHELL
typedef struct _aliastable {
	char *alias;
	char *comm;
} aliastable;

typedef struct _userfunctable {
	char *func;
	char **comm;
} userfunctable;
#endif	/* _NOORIGSHELL */

typedef struct _builtintable {
	int (NEAR *func)__P_((int, char *CONST []));
	char *ident;
} builtintable;

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
#define	FSID_FAT		4
#define	FSID_LFN		5
#define	FSID_LINUX		6
#define	FSID_DOSDRIVE		7

#define	LCK_READ		0
#define	LCK_WRITE		1
#define	LCK_UNLOCK		2

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

#define	HST_COM			0
#define	HST_PATH		1
#define	HST_USER		2
#define	HST_GROUP		3
#define	nohist(n)		((n) != HST_COM && (n) != HST_PATH)
#define	completable(n)		((n) >= 0)

#define	_LOG_WARNING_		0
#define	_LOG_NOTICE_		1
#define	_LOG_INFO_		2
#define	_LOG_DEBUG_		3
