/*
 *	types.h
 *
 *	Type Definition
 */

#ifdef	NOUID_T
typedef u_short	uid_t;
typedef u_short	gid_t;
#endif

#ifdef	NOVOID
#define	VOID
#define	VOID_T	int
#define	VOID_P	char *
#else
#define	VOID	void
#define	VOID_T	void
#define	VOID_P	void *
#endif

#ifdef	USEDIRECT
#define	dirent	direct
#endif

#ifdef	NOFILEMODE
# ifdef	S_IRUSR
# undef	S_IRUSR
# endif
# ifdef	S_IWUSR
# undef	S_IWUSR
# endif
# ifdef	S_IXUSR
# undef	S_IXUSR
# endif
# ifdef	S_IRGRP
# undef	S_IRGRP
# endif
# ifdef	S_IWGRP
# undef	S_IWGRP
# endif
# ifdef	S_IXGRP
# undef	S_IXGRP
# endif
# ifdef	S_IROTH
# undef	S_IROTH
# endif
# ifdef	S_IWOTH
# undef	S_IWOTH
# endif
# ifdef	S_IXOTH
# undef	S_IXOTH
# endif
#define	S_IRUSR	00400
#define	S_IWUSR	00200
#define	S_IXUSR	00100
#define	S_IRGRP	00040
#define	S_IWGRP	00020
#define	S_IXGRP	00010
#define	S_IROTH	00004
#define	S_IWOTH	00002
#define	S_IXOTH	00001
#endif

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
#endif

#ifndef	BITSPERBYTE
#define	BITSPERBYTE	8
#endif

typedef struct _namelist {
	char *name;
	u_short ent;
	u_short st_mode;
	short st_nlink;
#if	!MSDOS
	uid_t st_uid;
	gid_t st_gid;
#endif
#ifdef	HAVEFLAGS
	u_long st_flags;
#endif
	off_t st_size;
	time_t st_mtim;
	u_char flags;
	u_char tmpflags;
} namelist;

#define	F_ISEXE	0001
#define	F_ISWRI	0002
#define	F_ISRED	0004
#define	F_ISDIR	0010
#define	F_ISLNK	0020
#define	F_ISDEV	0040
#define	F_ISMRK	0001
#define	F_WSMRK	0002
#define	F_ISARG	0004
#define	F_STAT	0010

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

typedef struct _assoclist {
	char *org;
	char *assoc;
	struct _assoclist *next;
} assoclist;

typedef struct _strtable {
	u_short no;
	char *str;
} strtable;

typedef struct _bindtable {
	short key;
	u_char f_func;
	u_char d_func;
} bindtable;

typedef struct _functable {
	int (*func)__P_((char *));
	char *ident;
#ifndef	_NOJPNMES
	char *hmes;
#endif
#ifndef	_NOENGMES
	char *hmes_eng;
#endif
	u_char stat;
} functable;

#define	REWRITE	001
#define	RELIST	002
#define	REWIN	003
#define	REWRITEMODE	003
#define	RESCRN	004
#define	KILLSTK	010
#define	ARCH	020
#define	NO_FILE	040

#ifndef	_NOARCHIVE
#define	MAXLAUNCHFIELD	9
#define	MAXLAUNCHSEP	3
typedef struct _launchtable {
	char *ext;
	char *comm;
	u_char topskip;
	u_char bottomskip;
	u_char field[MAXLAUNCHFIELD];
	u_char delim[MAXLAUNCHFIELD];
	u_char width[MAXLAUNCHFIELD];
	u_char sep[MAXLAUNCHSEP];
	u_char lines;
} launchtable;

#define	F_MODE	0
#define	F_UID	1
#define	F_GID	2
#define	F_SIZE	3
#define	F_YEAR	4
#define	F_MON	5
#define	F_DAY	6
#define	F_TIME	7
#define	F_NAME	8

typedef struct _archivetable {
	char *ext;
	char *p_comm;
	char *u_comm;
} archivetable;
#endif

#ifndef	_NOTREE
typedef struct _treelist {
	char *name;
	int max;
#if	!MSDOS
	dev_t dev;
	ino_t ino;
	struct _treelist *parent;
#endif
	struct _treelist *sub;
} treelist;
#endif

typedef struct _winvartable {
#ifndef	_NOARCHIVE
	struct _winvartable *v_archduplp;
	char *v_archivedir;
	char *v_archivefile;
	char *v_archtmpdir;
	launchtable *v_launchp;
	namelist *v_arcflist;
	int v_maxarcf;
# ifndef	_NODOSDRIVE
	int v_archdrive;
# endif
#endif
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
} winvartable;

extern winvartable winvar[];
#ifdef	_NOSPLITWIN
#define	win	0
#else
extern int windows;
extern int win;
#endif
#ifndef	_NOARCHIVE
#define	archduplp	(winvar[win].v_archduplp)
#define	archivefile	(winvar[win].v_archivefile)
#define	archtmpdir	(winvar[win].v_archtmpdir)
#define	launchp		(winvar[win].v_launchp)
#define	arcflist	(winvar[win].v_arcflist)
#define	maxarcf		(winvar[win].v_maxarcf)
# ifndef	_NODOSDRIVE
#define	archdrive	(winvar[win].v_archdrive)
# endif
#endif
#ifndef	_NOTREE
#define	treepath	(winvar[win].v_treepath)
#endif
#define	findpattern	(winvar[win].v_findpattern)
#define	filelist	(winvar[win].v_filelist)
#define	maxfile		(winvar[win].v_maxfile)
#define	maxent		(winvar[win].v_maxent)
#define	filepos		(winvar[win].v_filepos)
#define	sorton		(winvar[win].v_sorton)
#define	dispmode	(winvar[win].v_dispmode)

typedef struct _macrostat {
	short addopt;
	short needmark;
	u_char flags;
} macrostat;

#define	F_NOCONFIRM	001
#define	F_ARGSET	002
#define	F_REMAIN	004
#define	F_NOEXT		010
#define	F_TOSFN		020
#define	F_ISARCH	040

typedef struct _aliastable {
	char *alias;
	char *comm;
} aliastable;

typedef struct _userfunctable {
	char *func;
	char **comm;
} userfunctable;

typedef struct _builtintable {
	int (*func)__P_((int, char *[], int));
	char *ident;
} builtintable;

#define	F_SYMLINK	001
#define	F_FILETYPE	002
#define	F_DOTFILE	004
#define	F_FILEFLAG	010

#define	isdisptyp(n)		((n) & F_FILETYPE)
#define	ishidedot(n)		((n) & F_DOTFILE)
#ifdef	_NOARCHIVE
#define	isdisplnk(n)		((n) & F_SYMLINK)
#define	isfileflg(n)		((n) & F_FILEFLAG)
#else
#define	isdisplnk(n)		(!archivefile && ((n) & F_SYMLINK))
#define	isfileflg(n)		(!archivefile && ((n) & F_FILEFLAG))
#endif
