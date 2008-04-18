/*
 *	info.c
 *
 *	information module
 */

#include <errno.h>
#include "fd.h"
#include "pathname.h"
#include "funcno.h"
#include "kanji.h"

#ifdef	NOERRNO
extern int errno;
#endif

#ifdef	USESYSDIRH
#include <sys/dir.h>
#endif

#if	MSDOS
#undef	USEVFSH
#undef	USEMNTENTH
#include "unixemu.h"
#else
#include <sys/param.h>
#include <sys/file.h>
#endif

#define	KEYWID			7

#ifdef	USEMNTENTH
#include <mntent.h>
typedef struct mntent		mnt_t;
#define	setmntent2		setmntent
#define	getmntent2(f,m)		getmntent(f)
#define	hasmntopt2(m,o)		strmntopt((m) -> mnt_opts, o)
#define	endmntent2		endmntent
#endif

#ifdef	USEMNTTABH
#include <sys/mnttab.h>
#define	MOUNTED			MNTTAB
typedef struct mnttab		mnt_t;
#define	setmntent2		fopen
#define	getmntent2(f,m)		(getmntent(f, m) ? NULL : m)
#define	hasmntopt2(m,o)		strmntopt((m) -> mnt_mntopts, o)
#define	endmntent2		fclose
#define	mnt_dir			mnt_mountp
#define	mnt_fsname		mnt_special
#define	mnt_type		mnt_fstype
# ifndef	mnt_opts
# define	mnt_opts	mnt_mntopts
# endif
#endif	/* USEMNTTABH */

#ifdef	USEMNTCTL
#include <fshelp.h>
#include <sys/vfs.h>
#include <sys/mntctl.h>
#include <sys/vmount.h>
#endif

#if	!defined (USEMOUNTH) && !defined (USEFSDATA) \
&& (defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTINFOR) || defined (USEMNTINFO))
#include <sys/mount.h>
#endif

#if	defined (USEGETFSSTAT) || defined (USEGETMNT)
#include <sys/fs_types.h>
#endif

#if	defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTCTL) || defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETMNT)
typedef struct _mnt_t {
	CONST char *mnt_fsname;
	CONST char *mnt_dir;
	CONST char *mnt_type;
	CONST char *mnt_opts;
} mnt_t;
static FILE *NEAR setmntent2 __P_((CONST char *, CONST char *));
static mnt_t *NEAR getmntent2 __P_((FILE *, mnt_t *));
#define	hasmntopt2(m,o)		strmntopt((m) -> mnt_opts, o)
# if	defined (USEMNTINFO) || defined (USEGETMNT)
# define	endmntent2(f)
# else
# define	endmntent2	free
# endif
static int mnt_ptr = 0;
static int mnt_size = 0;
#endif	/* USEGETFSSTAT || USEGETVFSTAT || USEMNTCTL \
|| USEMNTINFOR || USEMNTINFO || USEGETMNT */

#ifdef	USEGETFSENT
#include <fstab.h>
typedef struct fstab		mnt_t;
#define	setmntent2(f,m)		(FILE *)(setfsent(), NULL)
#define	getmntent2(f,m)		getfsent()
#define	hasmntopt2(m,o)		strmntopt((m) -> fs_mntops, o)
#define	endmntent2(fp)		endfsent()
#define	mnt_dir			fs_file
#define	mnt_fsname		fs_spec
#define	mnt_type		fs_vfstype
#endif

#if	MSDOS
# ifdef	DOUBLESLASH
# define	MNTDIRSIZ	MAXPATHLEN
# else
# define	MNTDIRSIZ	(3 + 1)
# endif
typedef struct _mnt_t {
	CONST char *mnt_fsname;
	char mnt_dir[MNTDIRSIZ];
	CONST char *mnt_type;
	CONST char *mnt_opts;
} mnt_t;
#define	hasmntopt2(m,o)		strmntopt((m) -> mnt_opts, o)
# ifdef	PC98
# define	PT_FAT12	0x81	/* 0x80 | 0x01 */
# define	PT_FAT16	0x91	/* 0x80 | 0x11 */
# define	PT_FAT32	0xa0	/* 0x80 | 0x20 */
# define	PT_FAT16X	0xa1	/* 0x80 | 0x21 */
# else
# define	PT_FAT12	0x01
# define	PT_FAT16	0x04
# define	PT_FAT16X	0x06
# define	PT_FAT32	0x0b
# define	PT_FAT32LBA	0x0c
# endif
#endif	/* MSDOS */

#if	defined (USESTATVFSH) || defined (USEGETVFSTAT)
#include <sys/statvfs.h>
#endif

#ifdef	USESTATVFSH
# ifdef	USESTATVFS_T
typedef statvfs_t		statfs_t;
# else
typedef struct statvfs		statfs_t;
# endif
#define	statfs2			statvfs
#define	blocksize(fs)		((fs).f_frsize ? (fs).f_frsize : (fs).f_bsize)
#endif	/* USESTATVFSH */

#ifdef	USESTATFSH
#include <sys/statfs.h>
#define	f_bavail		f_bfree
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEVFSH
#include <sys/vfs.h>
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEMOUNTH
#include <sys/mount.h>
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEFSDATA
#include <sys/mount.h>
typedef struct fs_data		statfs_t;
#define	statfs2(p, b)		(statfs(p, b) - 1)
#define	blocksize(fs)		1024
#define	f_bsize			fd_req.bsize
#define	f_files			fd_req.gtot
#define	f_blocks		fd_req.btot
#define	f_bfree			fd_req.bfree
#define	f_bavail		fd_req.bfreen
#endif

#ifdef	USEFFSIZE
#define	f_bsize			f_fsize
#endif

#if	defined (USESTATFSH) || defined (USEVFSH) || defined (USEMOUNTH)
# if	(STATFSARGS >= 4)
# define	statfs2(p, b)	statfs(p, b, sizeof(statfs_t), 0)
# else	/* STATFSARGS < 4 */
#  if	(STATFSARGS == 3)
#  define	statfs2(p, b)	statfs(p, b, sizeof(statfs_t))
#  else
#  define	statfs2		statfs
#  endif
# endif	/* STATFSARGS < 4 */
#endif	/* USESTATFSH || USEVFSH || USEMOUNTH */

#if	MSDOS
typedef struct _statfs_t {
	long f_bsize;
	long f_blocks;
	long f_bfree;
	long f_bavail;
	long f_files;
} statfs_t;
extern int unixstatfs __P_((CONST char *, statfs_t *));
#define	statfs2			unixstatfs
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	LINUX
#define	SFN_MSDOSFS
#endif
#if	defined (FREEBSD) && (__FreeBSD__ < 3)
# include <osreldate.h>
# if	defined (__FreeBSD_version) && (__FreeBSD_version < 227000)
# define	SFN_MSDOSFS
# endif
#endif	/* FREEBSD && (__FreeBSD__ < 3) */
#ifdef	NETBSD
# if	defined (NetBSD1_0) || defined (NetBSD1_1)
# define	SFN_MSDOSFS
# endif
#endif	/* NETBSD */

extern VOID error __P_((CONST char *));
extern int _chdir2 __P_((CONST char *));
extern char *strcpy2 __P_((char *, CONST char *));
extern char *strncpy2 __P_((char *, CONST char *, int));
extern char *getwd2 __P_((VOID_A));
extern VOID warning __P_((int, CONST char *));
#ifdef	_USEDOSPATH
extern int dospath __P_((CONST char *, char *));
extern char *gendospath __P_((char *, int, int));
#endif
#if	MSDOS && !defined (_NOUSELFN)
extern int supportLFN __P_((CONST char *));
# ifndef	_NODOSDRIVE
extern int checkdrive __P_((int));
# endif
#endif	/* MSDOS && !_NOUSELFN */
#ifndef	_NODOSDRIVE
extern int dosstatfs __P_((int, char *));
#endif
extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
#if	!MSDOS || !defined (NOFLOCK)
extern int Xstat __P_((CONST char *, struct stat *));
#endif
extern int Xaccess __P_((CONST char *, int));
extern int filetop __P_((int));
extern VOID cputspace __P_((int));
extern VOID cputstr __P_((int, CONST char *));
#ifdef	_NOPTY
#define	Xlocate			locate
#define	Xputterm		putterm
#define	Xputch2			putch2
#define	Xcputs2			cputs2
#define	Xcprintf2		cprintf2
#else
extern VOID Xlocate __P_((int, int));
extern VOID Xputterm __P_((int));
extern VOID Xputch2 __P_((int));
extern VOID Xcputs2 __P_((CONST char *));
extern VOID Xcprintf2 __P_((CONST char *, ...));
#endif

extern bindtable bindlist[];
extern CONST functable funclist[];
extern char fullpath[];
extern char *distributor;
#ifndef	_NODOSDRIVE
extern int needbavail;
#endif

#ifndef	MOUNTED
#define	MOUNTED			"/etc/mtab"
#endif

#ifndef	MNTTYPE_43
#define	MNTTYPE_43		"4.3"		/* NEWS-OS 3-4, HP-UX, HI-UX */
#endif
#ifndef	MNTTYPE_42
#define	MNTTYPE_42		"4.2"		/* SunOS 4 */
#endif
#ifndef	MNTTYPE_UFS
#define	MNTTYPE_UFS		"ufs"		/* SVR4, OSF/1, Free/NetBSD */
#endif
#ifndef	MNTTYPE_FFS
#define	MNTTYPE_FFS		"ffs"		/* NetBSD, OpenBSD */
#endif
#ifndef	MNTTYPE_ADVFS
#define	MNTTYPE_ADVFS		"advfs"		/* OSF/1 */
#endif
#ifndef	MNTTYPE_VXFS
#define	MNTTYPE_VXFS		"vxfs"		/* HP-UX */
#endif
#ifndef	MNTTYPE_HFS
#define	MNTTYPE_HFS		"hfs"		/* Darwin, HP-UX */
#endif
#ifndef	MNTTYPE_EXT2
#define	MNTTYPE_EXT2		"ext2"		/* Linux */
#endif
#ifndef	MNTTYPE_EXT3
#define	MNTTYPE_EXT3		"ext3"		/* Linux */
#endif
#ifndef	MNTTYPE_JFS
#define	MNTTYPE_JFS		"jfs"		/* AIX */
#endif
#ifndef	MNTTYPE_EFS
#define	MNTTYPE_EFS		"efs"		/* IRIX */
#endif
#ifndef	MNTTYPE_SYSV
#define	MNTTYPE_SYSV		"sysv"		/* SystemV Rel.3 */
#endif
#ifndef	MNTTYPE_DGUX
#define	MNTTYPE_DGUX		"dg/ux"		/* DG/UX */
#endif
#ifndef	MNTTYPE_UMSDOS
#define	MNTTYPE_UMSDOS		"umsdos"	/* umsdosfs on Linux */
#endif
#ifndef	MNTTYPE_MSDOS
#define	MNTTYPE_MSDOS		"msdos"		/* msdosfs */
#endif
#ifndef	MNTTYPE_MSDOSFS
#define	MNTTYPE_MSDOSFS		"msdosfs"	/* msdosfs on FreeBSD */
#endif
#ifndef	MNTTYPE_VFAT
#define	MNTTYPE_VFAT		"vfat"		/* vfatfs on Linux */
#endif
#ifndef	MNTTYPE_PC
#define	MNTTYPE_PC		"pc"		/* MS-DOS */
#endif
#ifndef	MNTTYPE_DOS7
#define	MNTTYPE_DOS7		"dos7"		/* MS-DOS on Win95 */
#endif
#ifndef	MNTTYPE_FAT12
#define	MNTTYPE_FAT12		"fat12"		/* MS-DOS */
#endif
#ifndef	MNTTYPE_FAT16
#define	MNTTYPE_FAT16		"fat16(16bit sector)"	/* MS-DOS */
#endif
#ifndef	MNTTYPE_FAT16X
#define	MNTTYPE_FAT16X		"fat16(32bit sector)"	/* MS-DOS */
#endif
#ifndef	MNTTYPE_FAT32
#define	MNTTYPE_FAT32		"fat32"		/* Win98 */
#endif
#ifndef	MNTTYPE_SHARED
#define	MNTTYPE_SHARED		"shared"	/* Win98 */
#endif
#define	MNTTYPE_XNFS		"nfs"		/* NFS */

static int NEAR code2str __P_((char *, int));
static int NEAR checkline __P_((int));
VOID help __P_((int));
static int NEAR getfsinfo __P_((CONST char *, statfs_t *, mnt_t *));
static CONST char *NEAR strmntopt __P_((CONST char *, CONST char *));
#ifndef	NOFLOCK
int isnfs __P_((CONST char *));
#endif
int writablefs __P_((CONST char *));
off_t getblocksize __P_((CONST char *));
static int NEAR info1line __P_((int, int, CONST char *,
		off_t, CONST char *, CONST char *));
off_t calcKB __P_((off_t, off_t));
int getinfofs __P_((CONST char *, off_t *, off_t *, off_t *));
int infofs __P_((CONST char *));

static CONST int keycodelist[] = {
	K_HOME, K_END, K_DL, K_IL, K_DC, K_IC,
	K_BEG, K_EOL, K_NPAGE, K_PPAGE, K_CLR, K_ENTER, K_HELP,
	K_BS, '\t', K_CR, K_ESC
};
#define	KEYCODESIZ		arraysize(keycodelist)
static CONST char *keystrlist[] = {
	"Home", "End", "DelLin", "InsLin", "Del", "Ins",
	"Beg", "Eol", "PageDn", "PageUp", "Clr", "Enter", "Help",
	"Bs", "Tab", "Ret", "Esc"
};
static CONST strtable mntlist[] = {
	{FSID_FAT, MNTTYPE_PC},
	{FSID_LFN, MNTTYPE_DOS7},
	{FSID_LFN, MNTTYPE_FAT32},
	{FSID_LFN, MNTTYPE_SHARED},
#if	MSDOS
# ifndef	_NODOSDRIVE
	{FSID_FAT, MNTTYPE_FAT12},
	{FSID_FAT, MNTTYPE_FAT16},
	{FSID_FAT, MNTTYPE_FAT16X},
# endif
#else	/* !MSDOS */
	{FSID_UFS, MNTTYPE_43},
	{FSID_UFS, MNTTYPE_42},
	{FSID_UFS, MNTTYPE_UFS},
	{FSID_UFS, MNTTYPE_FFS},
	{FSID_UFS, MNTTYPE_JFS},
	{FSID_EFS, MNTTYPE_EFS},
	{FSID_SYSV, MNTTYPE_SYSV},
	{FSID_SYSV, MNTTYPE_DGUX},
	{FSID_LINUX, MNTTYPE_EXT2},
	{FSID_LINUX, MNTTYPE_EXT3},
	{FSID_FAT, MNTTYPE_UMSDOS},
# ifdef	SFN_MSDOSFS
	{FSID_FAT, MNTTYPE_MSDOS},
# else
	{FSID_LFN, MNTTYPE_MSDOS},
# endif
	{FSID_LFN, MNTTYPE_MSDOSFS},
	{FSID_LFN, MNTTYPE_VFAT},
	{0, MNTTYPE_ADVFS},
	{0, MNTTYPE_VXFS},
# ifdef	DARWIN
	/* Macintosh HFS+ is pseudo file system covered with skin */
	{0, MNTTYPE_HFS},
# endif
#endif	/* !MSDOS */
};
#define	MNTLISTSIZ		arraysize(mntlist)


static int NEAR code2str(buf, code)
char *buf;
int code;
{
	int i;

	buf = buf + strlen(buf);
	if (code >= K_F(1) && code <= K_F(20))
		snprintf2(buf, KEYWID + 1, "F%-6d", code - K_F0);
	else if ((code & ~0x7f) == 0x80 && isalpha2(code & 0x7f))
		snprintf2(buf, KEYWID + 1, "Alt-%c  ", code & 0x7f);
	else if (code == K_UP)
		snprintf2(buf, KEYWID + 1, "%-*.*s", KEYWID, KEYWID, UPAR_K);
	else if (code == K_DOWN)
		snprintf2(buf, KEYWID + 1, "%-*.*s", KEYWID, KEYWID, DWNAR_K);
	else if (code == K_RIGHT)
		snprintf2(buf, KEYWID + 1, "%-*.*s", KEYWID, KEYWID, RIGAR_K);
	else if (code == K_LEFT)
		snprintf2(buf, KEYWID + 1, "%-*.*s", KEYWID, KEYWID, LEFAR_K);
	else {
		for (i = 0; i < KEYCODESIZ; i++)
			if (code == keycodelist[i]) break;
		if (i < KEYCODESIZ)
			snprintf2(buf, KEYWID + 1, "%-*.*s",
				KEYWID, KEYWID, keystrlist[i]);
#ifdef	CODEEUC
		else if (isekana2(code))
			snprintf2(buf, KEYWID + 1 + 1,
				"'%c%c'    ", C_EKANA, code & 0xff);
#endif
		else if (code > MAXUTYPE(u_char)) return(0);
#ifndef	CODEEUC
		else if (iskana2(code))
			snprintf2(buf, KEYWID + 1, "'%c'    ", code);
#endif
		else if (iscntrl2(code))
			snprintf2(buf, KEYWID + 1,
				"Ctrl-%c ", (code + '@') & 0x7f);
		else if (!ismsb(code))
			snprintf2(buf, KEYWID + 1, "'%c'    ", code);
		else return(0);
	}

	return(1);
}

static int NEAR checkline(y)
int y;
{
	if (y >= FILEPERROW) {
		warning(0, HITKY_K);
		y = 1;
	}

	return(y);
}

VOID help(arch)
int arch;
{
	char buf[(KEYWID + 1 + 1) * 2 + 1];
	int i, j, c, x, y, yy;

	if (distributor && *distributor) {
		i = n_column - (int)strlen(distributor) - 24;
		Xlocate(i, L_HELP);
		Xputch2('[');
		Xputterm(T_STANDOUT);
		Xcprintf2(" Distributed by: %s ", distributor);
		Xputterm(END_STANDOUT);
		Xputch2(']');
	}

	yy = filetop(win);
	x = y = 0;
	Xlocate(0, yy + y++);
	Xputterm(L_CLEAR);

	for (i = 0; i < FUNCLISTSIZ ; i++) {
		if (i == NO_OPERATION || i == WARNING_BELL) continue;
		Xlocate(x * (n_column / 2), yy + y);
		if (x ^= 1) Xputterm(L_CLEAR);
		else y++;
		if (arch && !(funclist[i].status & FN_ARCHIVE)) continue;

		c = 0;
		buf[0] = '\0';
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key >= 0; j++) {
			if (i != ffunc(j)) continue;
			c += code2str(buf, (int)(bindlist[j].key));
			if (c >= 2) break;
		}
		if (c < 2)
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key >= 0; j++) {
			if (i != dfunc(j)) continue;
			c += code2str(buf, (int)(bindlist[j].key));
			if (c >= 2) break;
		}
		if (!c) continue;

		Xcputs2("  ");
		if (c < 2) cputspace(KEYWID);
		Xcprintf2("%k: %k",
			buf, mesconv(funclist[i].hmes, funclist[i].hmes_eng));

		y = checkline(y);
	}
	if (y > 1) {
		if (x) y++;
		for (; y < FILEPERROW; y++) {
			Xlocate(0, yy + y);
			Xputterm(L_CLEAR);
		}
		warning(0, HITKY_K);
	}
}

#ifdef	USEMNTCTL
/*ARGSUSED*/
static FILE *NEAR setmntent2(file, mode)
CONST char *file, *mode;
{
	char *buf;

	mntctl(MCTL_QUERY, sizeof(int), (struct vmount *)&mnt_size);
	buf = malloc2(mnt_size);
	mntctl(MCTL_QUERY, mnt_size, (struct vmount *)buf);
	mnt_ptr = 0;

	return((FILE *)buf);
}

static mnt_t *NEAR getmntent2(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	struct vfs_ent *entp;
	struct vmount *vmntp;
	char *cp, *buf, *host;
	ALLOC_T len;

	if (mnt_ptr >= mnt_size) return(NULL);
	buf = (char *)fp;
	vmntp = (struct vmount *)&(buf[mnt_ptr]);

	cp = &(buf[mnt_ptr + vmntp -> vmt_data[VMT_OBJECT].vmt_off]);
	len = strlen(cp) + 1;
	if (!(vmntp -> vmt_flags & MNT_REMOTE)) {
		fsname = realloc2(fsname, len);
		memcpy(fsname, cp, len);
	}
	else {
		host = &(buf[mnt_ptr
			+ vmntp -> vmt_data[VMT_HOSTNAME].vmt_off]);
		len += strlen(host) + 1;
		fsname = realloc2(fsname, len);
		strcpy(strcpy2(strcpy2(fsname, host), ":"), cp);
	}

	cp = &(buf[mnt_ptr + vmntp -> vmt_data[VMT_STUB].vmt_off]);
	len = strlen(cp) + 1;
	dir = realloc2(dir, len);
	memcpy(dir, cp, len);

	entp = getvfsbytype(vmntp -> vmt_gfstype);
	if (entp) {
		cp = entp -> vfsent_name;
		len = strlen(cp) + 1;
		type = realloc2(type, len);
		memcpy(type, cp, len);
	}
	else if (type) {
		free(type);
		type = NULL;
	}

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = (type) ? type : "???";
	mntp -> mnt_opts =
		(vmntp -> vmt_flags & MNT_READONLY) ? "ro" : nullstr;
	mnt_ptr += vmntp -> vmt_length;

	return(mntp);
}
#endif	/* USEMNTCTL */

#if	defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETFSSTAT) || defined (USEGETVFSTAT)

#ifdef	USEGETVFSTAT
#define	f_flags			f_flag
#define	getfsstat2		getvfsstat
typedef struct statvfs		mntinfo_t;
#else
#define	getfsstat2		getfsstat
typedef struct statfs		mntinfo_t;
#endif

#if	!defined (MNT_RDONLY) && defined (M_RDONLY)
#define	MNT_RDONLY		M_RDONLY
#endif

/*ARGSUSED*/
static FILE *NEAR setmntent2(file, mode)
CONST char *file, *mode;
{
# ifndef	USEMNTINFO
	int size;
# endif
	mntinfo_t *buf;

	buf = NULL;
	mnt_ptr = mnt_size = 0;

# ifdef	DEBUG
	_mtrace_file = "getmntinfo(start)";
# endif
# ifdef	USEMNTINFO
	mnt_size = getmntinfo(&buf, MNT_NOWAIT);
# else	/* !USEMNTINFO */
#  ifdef	USEMNTINFOR
	size = 0;
	getmntinfo_r(&buf, MNT_WAIT, &mnt_size, &size);
#  else
	size = (getfsstat2(NULL, 0, MNT_WAIT) + 1) * sizeof(mntinfo_t);
	if (size > 0) {
		buf = (mntinfo_t *)malloc2(size);
		mnt_size = getfsstat2(buf, size, MNT_WAIT);
	}
#  endif
# endif	/* !USEMNTINFO */
# ifdef	DEBUG
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "getmntinfo(end)";
		malloc(0);	/* dummy malloc */
	}
# endif

	return((FILE *)buf);
}

static mnt_t *NEAR getmntent2(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
# if	defined (USEMNTINFO) || defined (USEGETVFSTAT)
#  ifdef	USEVFCNAME
	struct vfsconf *conf;
#  define	getvfsbynumber(n) \
				((conf = getvfsbytype(n)) \
				? conf -> vfc_name : NULL)
#  else	/* !USEVFCNAME */
#   ifdef	USEFFSTYPE
#   define	getvfsbynumber(n) \
				(buf[mnt_ptr].f_fstypename)
#   else	/* !USEFFSTYPE */
#    ifdef	INITMOUNTNAMES
	static char *mnt_names[] = INITMOUNTNAMES;
#    define	getvfsbynumber(n) \
				(((n) <= MOUNT_MAXTYPE) \
				? mnt_names[n] : NULL)
#    else
#    define	getvfsbynumber(n) \
				(NULL)
#    endif
#   endif	/* !USEFFSTYPE */
#  endif	/* !USEVFCNAME */
# else	/* !USEMNTINFO && !USEGETVFSTAT */
#  ifdef	USEGETFSSTAT
#  define	getvfsbynumber(n) \
				(((n) <= MOUNT_MAXTYPE) \
				? mnt_names[n] : NULL)
#  endif
# endif	/* !USEMNTINFO && !USEGETVFSTAT */
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	mntinfo_t *buf;
	char *cp;
	ALLOC_T len;

	if (mnt_ptr >= mnt_size) return(NULL);
	buf = (mntinfo_t *)fp;

# ifdef	DEBUG
	_mtrace_file = "getmntent(start)";
# endif
	len = strlen(buf[mnt_ptr].f_mntfromname) + 1;
	fsname = realloc2(fsname, len);
	memcpy(fsname, buf[mnt_ptr].f_mntfromname, len);

	len = strlen(buf[mnt_ptr].f_mntonname) + 1;
	dir = realloc2(dir, len);
	memcpy(dir, buf[mnt_ptr].f_mntonname, len);

	cp = (char *)getvfsbynumber(buf[mnt_ptr].f_type);
	if (cp) {
		len = strlen(cp) + 1;
		type = realloc2(type, len);
		memcpy(type, cp, len);
	}
	else if (type) {
		free(type);
		type = NULL;
	}
# ifdef	DEBUG
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "getmntent(end)";
		malloc(0);	/* dummy malloc */
	}
# endif

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = (type) ? type : "???";
	mntp -> mnt_opts =
		(buf[mnt_ptr].f_flags & MNT_RDONLY) ? "ro" : nullstr;
	mnt_ptr++;

	return(mntp);
}
#endif	/* USEMNTINFOR || USEMNTINFO || USEGETFSSTAT || USEGETVFSTAT */

#ifdef	USEGETMNT
/*ARGSUSED*/
static FILE *NEAR setmntent2(file, mode)
CONST char *file, *mode;
{
	mnt_ptr = 0;

	return((FILE *)1);
}

/*ARGSUSED*/
static mnt_t *NEAR getmntent2(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	struct fs_data buf;
	ALLOC_T len;

	if (getmnt(&mnt_ptr, &buf, sizeof(buf), NOSTAT_MANY, NULL) <= 0)
		return(NULL);

	len = strlen(buf.fd_req.devname) + 1;
	fsname = realloc2(fsname, len);
	memcpy(fsname, buf.fd_req.devname, len);

	len = strlen(buf.fd_req.path) + 1;
	dir = realloc2(dir, len);
	memcpy(dir, buf.fd_req.path, len);

	len = strlen(gt_names[buf.fd_req.fstype]) + 1;
	type = realloc2(type, len);
	memcpy(type, gt_names[buf.fd_req.fstype], len);

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = type;
	mntp -> mnt_opts = (buf.fd_req.flags & M_RONLY) ? "ro" : nullstr;

	return(mntp);
}
#endif	/* USEGETMNT */

static int NEAR getfsinfo(path, fsbuf, mntbuf)
CONST char *path;
statfs_t *fsbuf;
mnt_t *mntbuf;
{
#if	MSDOS
# ifndef	_NODOSDRIVE
	int i;
# endif
	char buf[MAXPATHLEN];
	int drive;
#else	/* !MSDOS */
# ifndef	_NODOSDRIVE
	static char dosmntdir[4];
	char buf[3 * sizeof(long) + 1];
	int drv;
# endif
	mnt_t *mntp;
	FILE *fp;
	char fsname[MAXPATHLEN], dir[MAXPATHLEN];
	ALLOC_T match;
#endif	/* !MSDOS */
#if	!MSDOS || defined (DOUBLESLASH)
	CONST char *cp;
	ALLOC_T len;
#endif
	statfs_t fs;
	mnt_t mnt;

#if	MSDOS
	if (!fsbuf) fsbuf = &fs;
	if (!mntbuf) mntbuf = &mnt;

	mntbuf -> mnt_fsname = "MSDOS";
	drive = dospath(path, buf);
# ifdef	DOUBLESLASH
	if (drive == '_') {
		if (*buf) path = buf;
		len = isdslash(path);
		if ((cp = strdelim(&(path[len]), 0))) {
			len = cp - path;
			if (!(cp = strdelim(&(path[len + 1]), 0)))
				cp += strlen(cp);
			len = cp - path;
		}
		strncpy2(mntbuf -> mnt_dir, path, len);
	}
	else
# endif
	VOID_C gendospath(mntbuf -> mnt_dir, drive, _SC_);

# ifdef	_NOUSELFN
	mntbuf -> mnt_type = MNTTYPE_PC;
# else	/* !_NOUSELFN */
	switch (supportLFN(mntbuf -> mnt_dir)) {
		case 3:
			mntbuf -> mnt_type = MNTTYPE_SHARED;
			break;
		case 2:
			mntbuf -> mnt_type = MNTTYPE_FAT32;
			break;
		case 1:
			mntbuf -> mnt_type = MNTTYPE_DOS7;
			break;
#  ifndef	_NODOSDRIVE
		case -1:
			mntbuf -> mnt_fsname = "LFN emurate";
			mntbuf -> mnt_type = MNTTYPE_DOS7;
			break;
		case -2:
			mntbuf -> mnt_fsname = "BIOS raw";
			i = checkdrive(toupper2(mntbuf -> mnt_dir[0]) - 'A');
			if (i == PT_FAT12) mntbuf -> mnt_type = MNTTYPE_FAT12;
			else if (i == PT_FAT16)
				mntbuf -> mnt_type = MNTTYPE_FAT16;
			else if (i == PT_FAT16X)
				mntbuf -> mnt_type = MNTTYPE_FAT16X;
			else mntbuf -> mnt_type = MNTTYPE_FAT32;
			break;
#  endif
		case -4:
			errno = ENOENT;
			return(-1);
/*NOTREACHED*/
			break;
		default:
			mntbuf -> mnt_type = MNTTYPE_PC;
			break;
	}
# endif	/* !_NOUSELFN */
	mntbuf -> mnt_opts = nullstr;

	if (statfs2(mntbuf -> mnt_dir, fsbuf) < 0) return(-1);
#else	/* !MSDOS */
	if (!fsbuf) fsbuf = &fs;
	if (!mntbuf) mntbuf = &mnt;

	cp = NULL;
	if (!strncmp(path, "/dev/", strsize("/dev/"))) {
		cp = getrealpath(path, dir, NULL);
		if (cp != dir && cp != path) return(-1);
	}
# ifndef	_NODOSDRIVE
	else if ((drv = dospath(path, NULL))) {
		mntbuf -> mnt_fsname = vnullstr;
		mntbuf -> mnt_dir = dosmntdir;
		dosmntdir[0] = drv;
		strcpy(&(dosmntdir[1]), ":\\");
		mntbuf -> mnt_type =
			(islower2(drv)) ? MNTTYPE_DOS7 : MNTTYPE_PC;
		mntbuf -> mnt_opts = vnullstr;
		if (dosstatfs(drv, buf) < 0) return(-1);
		if (buf[3 * sizeof(long)] & 001)
			mntbuf -> mnt_type = MNTTYPE_FAT32;
		fsbuf -> f_bsize = *((long *)&(buf[0 * sizeof(long)]));
#  ifdef	USEFSDATA
		fsbuf -> fd_req.btot = calcKB((off_t)(fsbuf -> f_bsize),
			(off_t)(*((long *)&(buf[1 * sizeof(long)]))));
		fsbuf -> fd_req.bfree =
		fsbuf -> fd_req.bfreen = calcKB((off_t)(fsbuf -> f_bsize),
			(off_t)(*((long *)&(buf[2 * sizeof(long)]))));
#  else	/* !USEFSDATA */
#   ifdef	USESTATVFSH
		fsbuf -> f_frsize = 0L;
#   endif
		fsbuf -> f_blocks = *((long *)&(buf[1 * sizeof(long)]));
		fsbuf -> f_bfree =
		fsbuf -> f_bavail = *((long *)&(buf[2 * sizeof(long)]));
#  endif	/* !USEFSDATA */
		fsbuf -> f_files = -1L;

		return(0);
	}
# endif	/* !_NODOSDRIVE */
	else if ((cp = getrealpath(path, dir, NULL)) != dir) return(-1);

	if (cp == dir) {
		match = (ALLOC_T)0;

		if (!(fp = setmntent2(MOUNTED, "rb"))) return(-1);
		for (;;) {
# ifdef	DEBUG
			_mtrace_file = "getmntent(start)";
			mntp = getmntent2(fp, &mnt);
			if (_mtrace_file) _mtrace_file = NULL;
			else {
				_mtrace_file = "getmntent(end)";
				malloc(0);	/* dummy malloc */
			}
			if (!mntp) break;
# else
			if (!(mntp = getmntent2(fp, &mnt))) break;
# endif
			len = strlen(mntp -> mnt_dir);
			if (len < match || strncmp(mntp -> mnt_dir, dir, len)
			|| (mntp -> mnt_dir[len - 1] != _SC_
			&& dir[len] && dir[len] != _SC_))
				continue;
			match = len;
			strcpy(fsname, mntp -> mnt_fsname);
		}
		endmntent2(fp);

		if (!match) {
			errno = ENOENT;
			return(-1);
		}
		cp = fsname;
	}

	if (!(fp = setmntent2(MOUNTED, "rb"))) return(-1);
	while ((mntp = getmntent2(fp, &mnt)))
		if (!strcmp(cp, mntp -> mnt_fsname)) break;
	endmntent2(fp);
	if (!mntp) {
		errno = ENOENT;
		return(-1);
	}
	memcpy((char *)mntbuf, (char *)mntp, sizeof(mnt_t));

	if (statfs2(mntbuf -> mnt_dir, fsbuf) >= 0) /*EMPTY*/;
	else if (path == cp || statfs2(path, fsbuf) < 0) return(-1);
#endif	/* !MSDOS */

	return(0);
}

static CONST char *NEAR strmntopt(s1, s2)
CONST char *s1, *s2;
{
	CONST char *cp;
	ALLOC_T len;

	len = strlen(s2);
	for (cp = s1; cp && *cp;) {
		if (!strncmp(cp, s2, len) && (!cp[len] || cp[len] == ','))
			return(cp);
		if ((cp = strchr(cp, ','))) cp++;
	}

	return(NULL);
}

#ifndef	NOFLOCK
int isnfs(path)
CONST char *path;
{
	mnt_t mntbuf;
	struct stat st;
	char *cp, buf[MAXPATHLEN];

	if (Xstat(path, &st) < 0) return(-1);
	if (!s_isdir(&st)) {
		if (!(cp = strrdelim(path, 1))) return(-1);
		strncpy2(buf, path, cp - path);
		path = buf;
	}
	if (getfsinfo(path, NULL, &mntbuf) < 0) return(-1);
	if (strncasecmp2(mntbuf.mnt_type, MNTTYPE_XNFS, strsize(MNTTYPE_XNFS)))
		return(0);

	return(1);
}
#endif	/* NOFLOCK */

int writablefs(path)
CONST char *path;
{
#ifdef	_USEDOSEMU
	int drv;
#endif
	mnt_t mntbuf;
	int i;

	if (Xaccess(path, R_OK | W_OK | X_OK) < 0) return(-1);
#ifdef	_USEDOSEMU
	if ((drv = dospath(path, NULL)))
		return((isupper2(drv)) ? FSID_FAT : FSID_DOSDRIVE);
#endif
	if (getfsinfo(path, NULL, &mntbuf) < 0) return(0);
	if (hasmntopt2(&mntbuf, "ro")) return(0);

	for (i = 0; i < MNTLISTSIZ; i++)
		if (!strcmp(mntbuf.mnt_type, mntlist[i].str))
			return(mntlist[i].no);

	return(0);
}

/*ARGSUSED*/
off_t getblocksize(dir)
CONST char *dir;
{
#if	MSDOS
	statfs_t fsbuf;

	if (statfs2(dir, &fsbuf) < 0) return((off_t)1024);

	return((off_t)blocksize(fsbuf));
#else	/* !MSDOS */
	statfs_t fsbuf;
	struct stat st;

	if (isdotdir(dir) != 2) {
		/* In case of the newborn directory */
		if (Xstat(dir, &st) >= 0) return((off_t)(st.st_size));
	}
	else {
# ifdef	CYGWIN
		if (statfs2(dir, &fsbuf) >= 0)
# else
		if (getfsinfo(dir, &fsbuf, NULL) >= 0)
# endif
			return((off_t)blocksize(fsbuf));
		if (Xstat(dir, &st) >= 0) return((off_t)(st.st_blksize));
	}

# ifdef	DEV_BSIZE
	return(DEV_BSIZE);
# else
	return((off_t)512);
# endif
#endif	/* !MSDOS */
}

static int NEAR info1line(yy, y, ind, n, s, unit)
int yy, y;
CONST char *ind;
off_t n;
CONST char *s, *unit;
{
	int width;

	Xlocate(0, yy + y);
	Xputterm(L_CLEAR);
	Xlocate(n_column / 2 - 20, yy + y);
	Xcprintf2("%-20.20k", ind);
	Xlocate(n_column / 2 + 2, yy + y);
	if (s) {
		width = n_column - (n_column / 2 + 2);
		cputstr(width, s);
	}
	else Xcprintf2("%<'*qd %k", MAXCOLSCOMMA(3), n, unit);

	return(checkline(++y));
}

off_t calcKB(block, byte)
off_t block, byte;
{
	if (block < (off_t)0 || byte <= (off_t)0) return((off_t)-1);
	if (byte == (off_t)1024) return(block);
	else if (byte > 1024) {
		byte = (byte + (off_t)512) / (off_t)1024;
		return(block * byte);
	}
	else {
		byte = ((off_t)1024 + (byte / (off_t)2)) / byte;
		return(block / byte);
	}
}

int getinfofs(path, totalp, freep, bsizep)
CONST char *path;
off_t *totalp, *freep, *bsizep;
{
	statfs_t fsbuf;
	int n;

#ifndef	_NODOSDRIVE
	needbavail++;
#endif
	n = getfsinfo(path, &fsbuf, NULL);
#ifndef	_NODOSDRIVE
	needbavail--;
#endif
	if (n < 0) {
		*bsizep = getblocksize(path);
		return(-1);
	}

	*totalp = fsbuf.f_blocks;
	*freep = fsbuf.f_bavail;
	*bsizep = blocksize(fsbuf);

	return(0);
}

int infofs(path)
CONST char *path;
{
	statfs_t fsbuf;
	mnt_t mntbuf;
	int y, yy;

#ifndef	_NODOSDRIVE
	needbavail++;
#endif
	if (getfsinfo(path, &fsbuf, &mntbuf) < 0) {
		warning(ENOTDIR, path);
#ifndef	_NODOSDRIVE
		needbavail--;
#endif
		return(0);
	}
	yy = filetop(win);
	y = 0;
	Xlocate(0, yy + y++);
	Xputterm(L_CLEAR);

	y = info1line(yy, y, FSNAM_K, (off_t)0, mntbuf.mnt_fsname, NULL);
	y = info1line(yy, y, FSMNT_K, (off_t)0, mntbuf.mnt_dir, NULL);
	y = info1line(yy, y, FSTYP_K, (off_t)0, mntbuf.mnt_type, NULL);
	y = info1line(yy, y, FSTTL_K,
		calcKB((off_t)(fsbuf.f_blocks), (off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(yy, y, FSUSE_K,
		calcKB((off_t)(fsbuf.f_blocks - fsbuf.f_bfree),
			(off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(yy, y, FSAVL_K,
		calcKB((off_t)(fsbuf.f_bavail), (off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(yy, y, FSBSZ_K, (off_t)(fsbuf.f_bsize), NULL, "bytes");
	y = info1line(yy, y, FSINO_K, (off_t)(fsbuf.f_files), NULL, UNIT_K);
	if (y > 1) {
		for (; y < FILEPERROW; y++) {
			Xlocate(0, yy + y);
			Xputterm(L_CLEAR);
		}
		warning(0, HITKY_K);
	}
#ifndef	_NODOSDRIVE
	needbavail--;
#endif

	return(1);
}
