/*
 *	Type Definition
 */

#ifdef	NOUID_T
typedef u_short	uid_t;
typedef u_short	gid_t;
#endif

#ifdef	NOVOID
#define	VOID
#define VOID_P	char *
#else
#define VOID	void
#define VOID_P	void *
#endif

#ifdef	USEDIRECT
#define	dirent	direct
#endif

#ifdef	NOFILEMODE
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

typedef struct _namelist {
	char *name;
	u_short ent;
	u_short st_mode;
	short st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	off_t st_size;
	time_t st_mtim;
	u_char flags;
} namelist;

#define	F_ISDIR	001
#define	F_ISLNK	002
#define	F_ISDEV	004
#define	F_ISMRK	010

#define	isdir(file)		((file) -> flags & F_ISDIR)
#define	islink(file)		((file) -> flags & F_ISLNK)
#define	isdev(file)		((file) -> flags & F_ISDEV)
#define	ismark(file)		((file) -> flags & F_ISMRK)

typedef struct _assoclist {
	char *org;
	char *assoc;
	struct _assoclist *next;
} assoclist;

typedef struct _strtable {
	int no;
	char *str;
} strtable;

typedef struct _bindtable {
	short key;
	u_char f_func;
	u_char d_func;
} bindtable;

typedef struct _functable {
	int (*func)();
	char *ident;
	char *hmes;
	char *hmes_eng;
	u_char stat;
} functable;

#define	REWRITE	001
#define	LISTUP	002
#define	RELIST	(REWRITE | LISTUP)
#define	KILLSTK	004
#define	ARCH	010
#define	NO_FILE	020

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

typedef struct _treelist {
	char *name;
	int max;
	int maxent;
	struct _treelist *next;
} treelist;

typedef struct _macrostat {
	short addoption;
	short needmark;
	u_char flags;
} macrostat;

#define	F_NOCONFIRM	001
#define	F_ARGSET	002
#define	F_REMAIN	004

typedef struct _aliastable {
	char *alias;
	char *comm;
} aliastable;

#define	F_SYMLINK	001
#define	F_FILETYPE	002
#define	F_DOTFILE	004

#define	isdisplnk(n)		((n) & F_SYMLINK)
#define	isdisptyp(n)		((n) & F_FILETYPE)
#define	ishidedot(n)		((n) & F_DOTFILE)

#define	QUOTE	('^' - '@')
#define	C_DEL	'\177'
