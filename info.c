/*
 *	info.c
 *
 *	information module
 */

#include "fd.h"
#include "dirent.h"
#include "sysemu.h"
#include "pathname.h"
#include "term.h"
#include "types.h"
#include "funcno.h"
#include "kanji.h"

#ifdef	USEMNTENTH
#include <mntent.h>
#endif
#ifdef	USEMNTTABH
#include <sys/mnttab.h>
#endif
#ifdef	USEMNTCTL
#include <fshelp.h>
#include <sys/vfs.h>
#include <sys/mntctl.h>
#include <sys/vmount.h>
#endif
#if	(!defined (USEMOUNTH) && !defined (USEFSDATA) \
&& (defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTINFOR) || defined (USEMNTINFO))) \
|| defined (USEMOUNTH) || defined (USEFSDATA)
#include <sys/mount.h>
#endif
#if	defined (USEGETFSSTAT) || defined (USEGETMNT)
#include <sys/fs_types.h>
#endif
#ifdef	USEGETFSENT
#include <fstab.h>
#endif
#if	defined (USESTATVFSH) || defined (USEGETVFSTAT)
#include <sys/statvfs.h>
#endif
#ifdef	USESTATFSH
#include <sys/statfs.h>
#endif
#ifdef	USEVFSH
#include <sys/vfs.h>
#endif

#define	KEYWID			7

#ifdef	USEMNTENTH
typedef struct mntent		mnt_t;
#define	Xsetmntent		setmntent
#define	Xgetmntent(f,m)		getmntent(f)
#define	Xhasmntopt(m,o)		strmntopt((m) -> mnt_opts, o)
#define	Xendmntent		endmntent
#endif

#ifdef	USEMNTTABH
#define	MOUNTED			MNTTAB
typedef struct mnttab		mnt_t;
#define	Xsetmntent		fopen
#define	Xgetmntent(f,m)		(getmntent(f, m) ? NULL : m)
#define	Xhasmntopt(m,o)		strmntopt((m) -> mnt_mntopts, o)
#define	Xendmntent		fclose
#define	mnt_dir			mnt_mountp
#define	mnt_fsname		mnt_special
#define	mnt_type		mnt_fstype
# ifndef	mnt_opts
# define	mnt_opts	mnt_mntopts
# endif
#endif	/* USEMNTTABH */

#if	defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTCTL) || defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETMNT) || defined (MINIX)
typedef struct _mnt_t {
	CONST char *mnt_fsname;
	CONST char *mnt_dir;
	CONST char *mnt_type;
	CONST char *mnt_opts;
} mnt_t;
#define	Xhasmntopt(m,o)		strmntopt((m) -> mnt_opts, o)
# ifdef	MINIX
# define	Xendmntent	fclose
# else	/* !MINIX */
#  if	defined (USEMNTINFO) || defined (USEGETMNT)
#  define	Xendmntent(f)
#  else
#  define	Xendmntent	Xfree
#  endif
# endif	/* !MINIX */
#endif	/* USEGETFSSTAT || USEGETVFSTAT || USEMNTCTL \
|| USEMNTINFOR || USEMNTINFO || USEGETMNT || MINIX */

#ifdef	USEGETFSENT
typedef struct fstab		mnt_t;
#define	Xsetmntent(f,m)		(FILE *)(setfsent(), NULL)
#define	Xgetmntent(f,m)		getfsent()
#define	Xhasmntopt(m,o)		strmntopt((m) -> fs_mntops, o)
#define	Xendmntent(fp)		endfsent()
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
#define	Xhasmntopt(m,o)		strmntopt((m) -> mnt_opts, o)
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
#define	f_bavail		f_bfree
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEVFSH
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEMOUNTH
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEFSDATA
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
#  else	/* STATFSARGS != 3 */
#   ifdef	USEFSTATFS
static int NEAR statfs2 __P_((CONST char *, statfs_t *));
#   else
#   define	statfs2		statfs
#   endif
#  endif	/* STATFSARGS != 3 */
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

extern VOID error __P_((CONST char *));
extern int _chdir2 __P_((CONST char *));
extern VOID warning __P_((int, CONST char *));
#ifdef	DEP_DOSLFN
extern int supportLFN __P_((CONST char *));
# ifdef	DEP_DOSDRIVE
extern int checkdrive __P_((int));
# endif
#endif	/* DEP_DOSLFN */
#ifdef	DEP_DOSDRIVE
extern int dosstatfs __P_((int, char *));
#endif
extern int filetop __P_((int));
extern VOID cputspace __P_((int));
extern VOID cputstr __P_((int, CONST char *));
#ifdef	DEP_PTY
extern VOID Xlocate __P_((int, int));
extern VOID Xputterm __P_((int));
extern VOID XXputch __P_((int));
extern VOID XXcputs __P_((CONST char *));
extern int XXcprintf __P_((CONST char *, ...));
#else
#define	Xlocate			locate
#define	Xputterm		putterm
#define	XXputch			Xputch
#define	XXcputs			Xcputs
#define	XXcprintf		Xcprintf
#endif

extern bindlist_t bindlist;
extern int maxbind;
extern CONST functable funclist[];
extern char fullpath[];
extern char *distributor;
#ifdef	DEP_DOSDRIVE
extern int needbavail;
#endif

static int NEAR code2str __P_((char *, int));
static int NEAR checkline __P_((int));
VOID help __P_((int));
#if	defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTCTL) || defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETMNT) || MINIX
static FILE *NEAR Xsetmntent __P_((CONST char *, CONST char *));
static mnt_t *NEAR Xgetmntent __P_((FILE *, mnt_t *));
#endif
#ifdef	MINIX
static char *NEAR getmntfield __P_((char **));
#endif
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
# ifdef	DEP_DOSDRIVE
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
#if	defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTCTL) || defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETMNT)
static int mnt_ptr = 0;
static int mnt_size = 0;
#endif


static int NEAR code2str(buf, code)
char *buf;
int code;
{
	int i;

	buf = buf + strlen(buf);
	if (code >= K_F(1) && code <= K_F(20))
		VOID_C Xsnprintf(buf, KEYWID + 1, "F%-6d", code - K_F0);
	else if ((code & ~0x7f) == 0x80 && Xisalpha(code & 0x7f))
		VOID_C Xsnprintf(buf, KEYWID + 1, "Alt-%c  ", code & 0x7f);
	else if (code == K_UP)
		VOID_C Xsnprintf(buf, KEYWID + 1,
			"%-*.*s", KEYWID, KEYWID, UPAR_K);
	else if (code == K_DOWN)
		VOID_C Xsnprintf(buf, KEYWID + 1,
			"%-*.*s", KEYWID, KEYWID, DWNAR_K);
	else if (code == K_RIGHT)
		VOID_C Xsnprintf(buf, KEYWID + 1,
			"%-*.*s", KEYWID, KEYWID, RIGAR_K);
	else if (code == K_LEFT)
		VOID_C Xsnprintf(buf, KEYWID + 1,
			"%-*.*s", KEYWID, KEYWID, LEFAR_K);
	else {
		for (i = 0; i < KEYCODESIZ; i++)
			if (code == keycodelist[i]) break;
		if (i < KEYCODESIZ)
			VOID_C Xsnprintf(buf, KEYWID + 1,
				"%-*.*s", KEYWID, KEYWID, keystrlist[i]);
#ifdef	CODEEUC
		else if (isekana2(code))
			VOID_C Xsnprintf(buf, KEYWID + 1 + 1,
				"'%c%c'    ", C_EKANA, code & 0xff);
#endif
		else if (code > MAXUTYPE(u_char)) return(0);
#ifndef	CODEEUC
		else if (Xiskana(code))
			VOID_C Xsnprintf(buf, KEYWID + 1, "'%c'    ", code);
#endif
		else if (Xiscntrl(code))
			VOID_C Xsnprintf(buf, KEYWID + 1,
				"Ctrl-%c ", (code + '@') & 0x7f);
		else if (!ismsb(code))
			VOID_C Xsnprintf(buf, KEYWID + 1, "'%c'    ", code);
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
	CONST char *cp;
	char buf[(KEYWID + 1 + 1) * 2 + 1];
	int i, j, c, x, y, yy;

	if (distributor && *distributor) {
		i = n_column - (int)strlen(distributor) - 24;
		Xlocate(i, L_HELP);
		VOID_C XXputch('[');
		Xputterm(T_STANDOUT);
		VOID_C XXcprintf(" Distributed by: %s ", distributor);
		Xputterm(END_STANDOUT);
		VOID_C XXputch(']');
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
		for (j = 0; j < maxbind; j++) {
			if (i != ffunc(j)) continue;
			c += code2str(buf, (int)(bindlist[j].key));
			if (c >= 2) break;
		}
		if (c < 2) for (j = 0; j < maxbind; j++) {
			if (i != dfunc(j)) continue;
			c += code2str(buf, (int)(bindlist[j].key));
			if (c >= 2) break;
		}
		if (!c) continue;

		XXcputs("  ");
		if (c < 2) cputspace(KEYWID);
		cp = mesconv2(funclist[i].hmes_no,
			funclist[i].hmes, funclist[i].hmes_eng);
		VOID_C XXcprintf("%k: %k", buf, cp);

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
static FILE *NEAR Xsetmntent(file, mode)
CONST char *file, *mode;
{
	char *buf;

	mntctl(MCTL_QUERY, sizeof(int), (struct vmount *)&mnt_size);
	buf = Xmalloc(mnt_size);
	mntctl(MCTL_QUERY, mnt_size, (struct vmount *)buf);
	mnt_ptr = 0;

	return((FILE *)buf);
}

static mnt_t *NEAR Xgetmntent(fp, mntp)
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
		fsname = Xrealloc(fsname, len);
		memcpy(fsname, cp, len);
	}
	else {
		host = &(buf[mnt_ptr
			+ vmntp -> vmt_data[VMT_HOSTNAME].vmt_off]);
		len += strlen(host) + 1;
		fsname = Xrealloc(fsname, len);
		Xstrcpy(Xstrcpy(Xstrcpy(fsname, host), ":"), cp);
	}

	cp = &(buf[mnt_ptr + vmntp -> vmt_data[VMT_STUB].vmt_off]);
	len = strlen(cp) + 1;
	dir = Xrealloc(dir, len);
	memcpy(dir, cp, len);

	entp = getvfsbytype(vmntp -> vmt_gfstype);
	if (entp) {
		cp = entp -> vfsent_name;
		len = strlen(cp) + 1;
		type = Xrealloc(type, len);
		memcpy(type, cp, len);
	}
	else if (type) {
		Xfree(type);
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

# ifdef	USEGETVFSTAT
# define	f_flags			f_flag
# define	getfsstat2		getvfsstat
typedef struct statvfs		mntinfo_t;
# else
# define	getfsstat2		getfsstat
typedef struct statfs		mntinfo_t;
# endif

# if	!defined (MNT_RDONLY) && defined (M_RDONLY)
# define	MNT_RDONLY		M_RDONLY
# endif

/*ARGSUSED*/
static FILE *NEAR Xsetmntent(file, mode)
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
		buf = (mntinfo_t *)Xmalloc(size);
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

static mnt_t *NEAR Xgetmntent(fp, mntp)
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
	fsname = Xrealloc(fsname, len);
	memcpy(fsname, buf[mnt_ptr].f_mntfromname, len);

	len = strlen(buf[mnt_ptr].f_mntonname) + 1;
	dir = Xrealloc(dir, len);
	memcpy(dir, buf[mnt_ptr].f_mntonname, len);

	cp = (char *)getvfsbynumber(buf[mnt_ptr].f_type);
	if (cp) {
		len = strlen(cp) + 1;
		type = Xrealloc(type, len);
		memcpy(type, cp, len);
	}
	else if (type) {
		Xfree(type);
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
static FILE *NEAR Xsetmntent(file, mode)
CONST char *file, *mode;
{
	mnt_ptr = 0;

	return((FILE *)1);
}

/*ARGSUSED*/
static mnt_t *NEAR Xgetmntent(fp, mntp)
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
	fsname = Xrealloc(fsname, len);
	memcpy(fsname, buf.fd_req.devname, len);

	len = strlen(buf.fd_req.path) + 1;
	dir = Xrealloc(dir, len);
	memcpy(dir, buf.fd_req.path, len);

	len = strlen(gt_names[buf.fd_req.fstype]) + 1;
	type = Xrealloc(type, len);
	memcpy(type, gt_names[buf.fd_req.fstype], len);

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = type;
	mntp -> mnt_opts = (buf.fd_req.flags & M_RONLY) ? "ro" : nullstr;

	return(mntp);
}
#endif	/* USEGETMNT */

#ifdef	MINIX
static FILE *NEAR Xsetmntent(file, mode)
CONST char *file, *mode;
{
	return(fopen(file, mode));
}

static char *NEAR getmntfield(cpp)
char **cpp;
{
	char *s;

	if (!cpp || !*cpp || !**cpp) return(vnullstr);

	while (Xisspace(**cpp)) (*cpp)++;
	if (!**cpp) return(vnullstr);
	s = *cpp;

	while (**cpp && !Xisspace(**cpp)) (*cpp)++;
	if (**cpp) *((*cpp)++) = '\0';

	return(s);
}

static mnt_t *NEAR Xgetmntent(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char buf[BUFSIZ];
	char *cp;

	if (!(cp = fgets(buf, sizeof(buf), fp))) return(NULL);

	mntp -> mnt_fsname = mntp -> mnt_dir =
	mntp -> mnt_type = mntp -> mnt_opts = NULL;

	mntp -> mnt_fsname = getmntfield(&cp);
	mntp -> mnt_dir = getmntfield(&cp);
	mntp -> mnt_type = getmntfield(&cp);
	mntp -> mnt_opts = getmntfield(&cp);

	return(mntp);
}
#endif	/* MINIX */

#ifdef	USEFSTATFS
static int NEAR statfs2(path, buf)
CONST char *path;
statfs_t *buf;
{
	int n, fd;

	if ((fd = Xopen(path, O_RDONLY, 0666)) < 0) return(-1);
	n = fstatfs(fd, buf);
	Xclose(fd);

	return(n);
}
#endif	/* USEFSTATFS */

static int NEAR getfsinfo(path, fsbuf, mntbuf)
CONST char *path;
statfs_t *fsbuf;
mnt_t *mntbuf;
{
#if	MSDOS
# ifdef	DEP_DOSDRIVE
	int i;
# endif
	char buf[MAXPATHLEN];
	int drive;
#else	/* !MSDOS */
# ifdef	DEP_DOSDRIVE
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
		Xstrncpy(mntbuf -> mnt_dir, path, len);
	}
	else
# endif
	VOID_C gendospath(mntbuf -> mnt_dir, drive, _SC_);

# ifdef	DEP_DOSLFN
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
#  ifdef	DEP_DOSDRIVE
		case -1:
			mntbuf -> mnt_fsname = "LFN emurate";
			mntbuf -> mnt_type = MNTTYPE_DOS7;
			break;
		case -2:
			mntbuf -> mnt_fsname = "BIOS raw";
			i = checkdrive(Xtoupper(mntbuf -> mnt_dir[0]) - 'A');
			if (i == PT_FAT12) mntbuf -> mnt_type = MNTTYPE_FAT12;
			else if (i == PT_FAT16)
				mntbuf -> mnt_type = MNTTYPE_FAT16;
			else if (i == PT_FAT16X)
				mntbuf -> mnt_type = MNTTYPE_FAT16X;
			else mntbuf -> mnt_type = MNTTYPE_FAT32;
			break;
#  endif
		case -4:
			return(seterrno(ENOENT));
/*NOTREACHED*/
			break;
		default:
			mntbuf -> mnt_type = MNTTYPE_PC;
			break;
	}
# else	/* !DEP_DOSLFN */
	mntbuf -> mnt_type = MNTTYPE_PC;
# endif	/* !DEP_DOSLFN */
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
# ifdef	DEP_DOSDRIVE
	else if ((drv = dospath(path, NULL))) {
		mntbuf -> mnt_fsname = vnullstr;
		mntbuf -> mnt_dir = dosmntdir;
		dosmntdir[0] = drv;
		Xstrcpy(&(dosmntdir[1]), ":\\");
		mntbuf -> mnt_type =
			(Xislower(drv)) ? MNTTYPE_DOS7 : MNTTYPE_PC;
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
#   ifndef	NOFBLOCKS
		fsbuf -> f_blocks = *((long *)&(buf[1 * sizeof(long)]));
#   endif
#   ifndef	NOFBFREE
		fsbuf -> f_bfree =
		fsbuf -> f_bavail = *((long *)&(buf[2 * sizeof(long)]));
#   endif
#  endif	/* !USEFSDATA */
#  ifndef	NOFFILES
		fsbuf -> f_files = -1L;
#  endif

		return(0);
	}
# endif	/* DEP_DOSDRIVE */
	else if ((cp = getrealpath(path, dir, NULL)) != dir) return(-1);

	if (cp == dir) {
		match = (ALLOC_T)0;

		if (!(fp = Xsetmntent(MOUNTED, "rb"))) return(-1);
		for (;;) {
# ifdef	DEBUG
			_mtrace_file = "getmntent(start)";
			mntp = Xgetmntent(fp, &mnt);
			if (_mtrace_file) _mtrace_file = NULL;
			else {
				_mtrace_file = "getmntent(end)";
				malloc(0);	/* dummy malloc */
			}
			if (!mntp) break;
# else
			if (!(mntp = Xgetmntent(fp, &mnt))) break;
# endif
			len = strlen(mntp -> mnt_dir);
			if (len < match || strncmp(mntp -> mnt_dir, dir, len)
			|| (mntp -> mnt_dir[len - 1] != _SC_
			&& dir[len] && dir[len] != _SC_))
				continue;
			match = len;
			Xstrcpy(fsname, mntp -> mnt_fsname);
		}
		Xendmntent(fp);

		if (!match) return(seterrno(ENOENT));
		cp = fsname;
	}

	if (!(fp = Xsetmntent(MOUNTED, "rb"))) return(-1);
	while ((mntp = Xgetmntent(fp, &mnt)))
		if (!strcmp(cp, mntp -> mnt_fsname)) break;
	Xendmntent(fp);
	if (!mntp) return(seterrno(ENOENT));
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
		if ((cp = Xstrchr(cp, ','))) cp++;
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
		Xstrncpy(buf, path, cp - path);
		path = buf;
	}
	if (getfsinfo(path, NULL, &mntbuf) < 0) return(-1);
	if (Xstrncasecmp(mntbuf.mnt_type, MNTTYPE_XNFS, strsize(MNTTYPE_XNFS)))
		return(0);

	return(1);
}
#endif	/* NOFLOCK */

int writablefs(path)
CONST char *path;
{
#ifdef	DEP_DOSEMU
	int drv;
#endif
	mnt_t mntbuf;
	int i;

	if (Xaccess(path, R_OK | W_OK | X_OK) < 0) return(-1);
#ifdef	DEP_DOSEMU
	if ((drv = dospath(path, NULL)))
		return((Xisupper(drv)) ? FSID_FAT : FSID_DOSDRIVE);
#endif
	if (getfsinfo(path, NULL, &mntbuf) < 0) return(0);
	if (Xhasmntopt(&mntbuf, "ro")) return(0);

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
# ifndef	NOSTBLKSIZE
		if (Xstat(dir, &st) >= 0) return((off_t)(st.st_blksize));
# endif
	}

	return(DEV_BSIZE);
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
	VOID_C XXcprintf("%-20.20k", ind);
	Xlocate(n_column / 2 + 2, yy + y);
	if (s) {
		width = n_column - (n_column / 2 + 2);
		cputstr(width, s);
	}
	else VOID_C XXcprintf("%<'*qd %k", MAXCOLSCOMMA(3), n, unit);

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

#ifdef	DEP_DOSDRIVE
	needbavail++;
#endif
	n = getfsinfo(path, &fsbuf, NULL);
#ifdef	DEP_DOSDRIVE
	needbavail--;
#endif
	if (n < 0) {
		*bsizep = getblocksize(path);
		return(-1);
	}

#ifdef	NOFBLOCKS
	*totalp = (off_t)0;
#else
	*totalp = fsbuf.f_blocks;
#endif
#ifdef	NOFBFREE
	*freep = (off_t)0;
#else
	*freep = fsbuf.f_bavail;
#endif
	*bsizep = blocksize(fsbuf);

	return(0);
}

int infofs(path)
CONST char *path;
{
	statfs_t fsbuf;
	mnt_t mntbuf;
	int y, yy;

#ifdef	DEP_DOSDRIVE
	needbavail++;
#endif
	if (getfsinfo(path, &fsbuf, &mntbuf) < 0) {
		warning(ENOTDIR, path);
#ifdef	DEP_DOSDRIVE
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
#ifndef	NOFBLOCKS
	y = info1line(yy, y, FSTTL_K,
		calcKB((off_t)(fsbuf.f_blocks), (off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
#endif
#ifndef	NOFBFREE
	y = info1line(yy, y, FSUSE_K,
		calcKB((off_t)(fsbuf.f_blocks - fsbuf.f_bfree),
			(off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(yy, y, FSAVL_K,
		calcKB((off_t)(fsbuf.f_bavail), (off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
#endif
	y = info1line(yy, y, FSBSZ_K, (off_t)(fsbuf.f_bsize), NULL, "bytes");
#ifndef	NOFFILES
	y = info1line(yy, y, FSINO_K, (off_t)(fsbuf.f_files), NULL, UNIT_K);
#endif
	if (y > 1) {
		for (; y < FILEPERROW; y++) {
			Xlocate(0, yy + y);
			Xputterm(L_CLEAR);
		}
		warning(0, HITKY_K);
	}
#ifdef	DEP_DOSDRIVE
	needbavail--;
#endif

	return(1);
}
