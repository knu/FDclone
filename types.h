/*
 *	Type Definition
 */

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
#define	F_ISMRK	004

#define	isdir(file)		((file) -> flags & F_ISDIR)
#define	islink(file)		((file) -> flags & F_ISLNK)
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
	char f_func;
	char d_func;
} bindtable;

typedef struct _functable {
	int (*func)();
	char *ident;
	char *hmes;
	u_char stat;
} functable;

#define	REWRITE	001
#define	LISTUP	002
#define	KILLSTK	004
#define	ARCH	010

#define	MAXLAUNCHFIELD	9
#define	MAXLAUNCHSEP	3
typedef struct _launchtable {
	char *ext;
	char *comm;
	char topskip;
	char bottomskip;
	char field[MAXLAUNCHFIELD];
	char delim[MAXLAUNCHFIELD];
	u_char width[MAXLAUNCHFIELD];
	char sep[MAXLAUNCHSEP];
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

#define	F_NOCONFIRM	001
#define	F_ADDOPTION	002
#define	F_REMAIN	004
