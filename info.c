/*
 *	info.c
 *
 *	Information Module
 */

#include <errno.h>
#include "fd.h"
#include "term.h"
#include "funcno.h"
#include "kctype.h"
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

#ifdef	USEMNTENTH
#include <mntent.h>
typedef struct mntent	mnt_t;
#define	getmntent2(fp, mntp)	getmntent(fp)
#define	hasmntopt2(mntp, opt)	strmntopt((mntp) -> mnt_opts, opt)
#endif	/* USEMNTENTH */

#ifdef	USEMNTTABH
#include <sys/mnttab.h>
#define	MOUNTED		MNTTAB
typedef struct mnttab	mnt_t;
#define	setmntent		fopen
#define	getmntent2(fp, mntp)	(getmntent(fp, mntp) ? NULL : mntp)
#define	hasmntopt2(mntp, opt)	strmntopt((mntp) -> mnt_mntopts, opt)
#define	endmntent		fclose
#define	mnt_dir		mnt_mountp
#define	mnt_fsname	mnt_special
#define	mnt_type	mnt_fstype
#ifndef	mnt_opts
#define	mnt_opts	mnt_mntopts
#endif
#endif	/* USEMNTTABH */

#ifdef	USEMNTCTL
#include <fshelp.h>
#include <sys/vfs.h>
#include <sys/mntctl.h>
#include <sys/vmount.h>
#endif	/* USEMNTCTL */

#if	!defined (USEMOUNTH) && !defined (USEFSDATA) \
&& (defined (USEGETFSSTAT) || defined (USEMNTINFOR) || defined (USEMNTINFO))
#include <sys/mount.h>
#endif

#if	defined (USEGETFSSTAT) || defined (USEGETMNT)
#include <sys/fs_types.h>
#endif

#if	defined (USEGETFSSTAT) || defined (USEMNTCTL) \
|| defined (USEMNTINFOR) || defined (USEMNTINFO) || defined (USEGETMNT)
typedef struct _mnt_t {
	char *mnt_fsname;
	char *mnt_dir;
	char *mnt_type;
	char *mnt_opts;
} mnt_t;
static FILE *NEAR setmntent __P_((char *, char *));
static mnt_t *NEAR getmntent2 __P_((FILE *, mnt_t *));
#define	hasmntopt2(mntp, opt)	strmntopt((mntp) -> mnt_opts, opt)
#if	defined (USEMNTINFO) || defined (USEGETMNT)
#define	endmntent(fp)
#else
#define	endmntent(fp)		{ if (fp) free(fp); }
#endif
static int mnt_ptr = 0;
static int mnt_size = 0;
#endif

#ifdef	USEGETFSENT
#include <fstab.h>
typedef struct fstab	mnt_t;
#define	setmntent(file, mode)	(FILE *)(setfsent(), NULL)
#define	getmntent2(fp, mntp)	getfsent()
#define	hasmntopt2(mntp, opt)	strmntopt((mntp) -> fs_mntops, opt)
#define	endmntent(fp)		endfsent()
#define	mnt_dir		fs_file
#define	mnt_fsname	fs_spec
#define	mnt_type	fs_vfstype
#endif	/* USEGETFSENT */

#if	MSDOS
typedef struct _mnt_t {
	char *mnt_fsname;
	char mnt_dir[4];
	char *mnt_type;
	char *mnt_opts;
} mnt_t;
#define	hasmntopt2(mntp, opt)	strmntopt((mntp) -> mnt_opts, opt)
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
#endif


#ifdef	USESTATVFSH
#include <sys/statvfs.h>
# ifdef	USESTATVFS_T
typedef statvfs_t	statfs_t;
# else
typedef struct statvfs	statfs_t;
# endif
#define	statfs2		statvfs
#define	blocksize(fs)	((fs).f_frsize ? (fs).f_frsize : (fs).f_bsize)
#endif	/* USESTATVFSH */

#ifdef	USESTATFSH
#include <sys/statfs.h>
#define	f_bavail	f_bfree
typedef struct statfs	statfs_t;
#define	blocksize(fs)	(fs).f_bsize
#endif	/* USESTATFSH */

#ifdef	USEVFSH
#include <sys/vfs.h>
typedef struct statfs	statfs_t;
#define	blocksize(fs)	(fs).f_bsize
#endif	/* USEVFSH */

#ifdef	USEMOUNTH
#include <sys/mount.h>
typedef struct statfs	statfs_t;
#define	blocksize(fs)	(fs).f_bsize
#endif	/* USEMOUNTH */

#ifdef	USEFSDATA
#include <sys/mount.h>
typedef struct fs_data	statfs_t;
#define	statfs2(path, buf)	(statfs(path, buf) - 1)
#define	blocksize(fs)		1024
#define	f_bsize		fd_req.bsize
#define	f_files		fd_req.gtot
#define	f_blocks	fd_req.btot
#define	f_bfree		fd_req.bfree
#define	f_bavail	fd_req.bfreen
#endif	/* USEFSDATA */

#ifdef	USEFFSIZE
#define	f_bsize		f_fsize
#endif

#if	defined (USESTATFSH) || defined (USEVFSH) || defined (USEMOUNTH)
# if	(STATFSARGS >= 4)
# define	statfs2(path, buf)	statfs(path, buf, sizeof(statfs_t), 0)
# else
#  if	(STATFSARGS == 3)
#  define	statfs2(path, buf)	statfs(path, buf, sizeof(statfs_t))
#  else
#  define	statfs2			statfs
#  endif
# endif
#endif

#if	MSDOS
typedef struct _statfs_t {
	long	f_bsize;
	long	f_blocks;
	long	f_bfree;
	long	f_bavail;
	long	f_files;
} statfs_t;
extern int unixstatfs __P_((char *, statfs_t *));
#define	statfs2		unixstatfs
#define	blocksize(fs)	(fs).f_bsize
#endif

extern VOID error __P_((char *));
extern int _chdir2 __P_((char *));
extern char *strcpy2 __P_((char *, char *));
extern int snprintf2 __P_((char *, int, CONST char *, ...));
extern char *getwd2 __P_((VOID_A));
extern VOID warning __P_((int, char *));
#if	MSDOS || !defined (_NODOSDRIVE)
extern int dospath __P_((char *, char *));
#endif
#if	MSDOS && !defined (_NOUSELFN)
# ifdef	LSI_C
# define	toupper2	toupper
# else	/* !LSI_C */
# define	toupper2(c)	(uppercase[(u_char)(c)])
extern u_char uppercase[256];
# endif	/* !LSI_C */
extern int supportLFN __P_((char *));
# ifndef	_NODOSDRIVE
extern int checkdrive __P_((int));
# endif
#endif	/* MSDOS && !_NOUSELFN */
#ifndef	_NODOSDRIVE
extern int dosstatfs __P_((int, char *));
#endif
extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *ascnumeric __P_((char *, off_t, int, int));
extern int kanjiputs __P_((char *));
extern int kanjiputs2 __P_((char *, int, int));
extern int Xaccess __P_((char *, int));
extern int kanjiprintf __P_((CONST char *, ...));

extern bindtable bindlist[];
extern functable funclist[];
extern char fullpath[];
extern int sizeinfo;
extern char *distributor;
#ifndef	_NODOSDRIVE
extern int needbavail;
#endif

#define	KEYWID		7

#ifndef	MOUNTED
#define	MOUNTED		"/etc/mtab"
#endif

#ifndef	MNTTYPE_43
#define	MNTTYPE_43	"4.3"	/* NEWS-OS 3-4, HP-UX, HI-UX */
#endif
#ifndef	MNTTYPE_42
#define	MNTTYPE_42	"4.2"	/* SunOS 4 */
#endif
#ifndef	MNTTYPE_UFS
#define	MNTTYPE_UFS	"ufs"	/* SVR4, OSF/1, FreeBSD, NetBSD */
#endif
#ifndef	MNTTYPE_FFS
#define	MNTTYPE_FFS	"ffs"	/* NetBSD, OpenBSD */
#endif
#ifndef	MNTTYPE_ADVFS
#define	MNTTYPE_ADVFS	"advfs"	/* OSF/1 */
#endif
#ifndef	MNTTYPE_VXFS
#define	MNTTYPE_VXFS	"vxfs"	/* HP-UX */
#endif
#ifndef	MNTTYPE_HFS
#define	MNTTYPE_HFS	"hfs"	/* Darwin, HP-UX */
#endif
#ifndef	MNTTYPE_EXT2
#define	MNTTYPE_EXT2	"ext2"	/* Linux */
#endif
#ifndef	MNTTYPE_JFS
#define	MNTTYPE_JFS	"jfs"	/* AIX */
#endif
#ifndef	MNTTYPE_EFS
#define	MNTTYPE_EFS	"efs"	/* IRIX */
#endif
#ifndef	MNTTYPE_SYSV
#define	MNTTYPE_SYSV	"sysv"	/* SystemV Rel.3 */
#endif
#ifndef	MNTTYPE_DGUX
#define	MNTTYPE_DGUX	"dg/ux"	/* DG/UX */
#endif
#ifndef	MNTTYPE_MSDOS
#define	MNTTYPE_MSDOS	"msdos"	/* msdosfs */
#endif
#ifndef	MNTTYPE_UMSDOS
#define	MNTTYPE_UMSDOS	"umsdos"	/* umsdosfs */
#endif
#ifndef	MNTTYPE_VFAT
#define	MNTTYPE_VFAT	"vfat"	/* vfatfs */
#endif
#ifndef	MNTTYPE_PC
#define	MNTTYPE_PC	"pc"	/* MS-DOS */
#endif
#ifndef	MNTTYPE_DOS7
#define	MNTTYPE_DOS7	"dos7"	/* MS-DOS on Win95 */
#endif
#ifndef	MNTTYPE_FAT12
#define	MNTTYPE_FAT12	"fat12"	/* MS-DOS */
#endif
#ifndef	MNTTYPE_FAT16
#define	MNTTYPE_FAT16	"fat16(16bit sector)"	/* MS-DOS */
#endif
#ifndef	MNTTYPE_FAT16X
#define	MNTTYPE_FAT16X	"fat16(32bit sector)"	/* MS-DOS */
#endif
#ifndef	MNTTYPE_FAT32
#define	MNTTYPE_FAT32	"fat32"	/* Win98 */
#endif

static int NEAR code2str __P_((char *, int));
static int NEAR checkline __P_((int));
VOID help __P_((int));
static int NEAR getfsinfo __P_((char *, statfs_t *, mnt_t *));
static char *NEAR strmntopt __P_((char *, char *));
int writablefs __P_((char *));
off_t getblocksize __P_((char *));
static int NEAR info1line __P_((int, char *, off_t, char *, char *));
off_t calcKB __P_((off_t, off_t));
int getinfofs __P_((char *, off_t *, off_t *, off_t *));
int infofs __P_((char *));

static int keycodelist[] = {
	K_HOME, K_END, K_DL, K_IL, K_DC, K_IC,
	K_BEG, K_EOL, K_NPAGE, K_PPAGE, K_CLR, K_ENTER, K_HELP,
	K_BS, '\t', K_CR, K_ESC
};
#define	KEYCODESIZ	((int)(sizeof(keycodelist) / sizeof(int)))
static char *keystrlist[] = {
	"Home", "End", "DelLin", "InsLin", "Del", "Ins",
	"Beg", "Eol", "PageDn", "PageUp", "Clr", "Enter", "Help",
	"Bs", "Tab", "Ret", "Esc"
};


static int NEAR code2str(buf, code)
char *buf;
int code;
{
	buf = buf + strlen(buf);
	if (code >= K_F(1) && code <= K_F(20))
		snprintf2(buf, KEYWID + 1, "F%-6d", code - K_F0);
	else if ((code & ~0x7f) == 0x80 && isalpha(code & 0x7f))
		snprintf2(buf, KEYWID + 1, "Alt-%c  ", code & 0x7f);
	else if (code == K_UP)
		snprintf2(buf, KEYWID + 1, "%-7.7s", UPAR_K);
	else if (code == K_DOWN)
		snprintf2(buf, KEYWID + 1, "%-7.7s", DWNAR_K);
	else if (code == K_RIGHT)
		snprintf2(buf, KEYWID + 1, "%-7.7s", RIGAR_K);
	else if (code == K_LEFT)
		snprintf2(buf, KEYWID + 1, "%-7.7s", LEFAR_K);
	else {
		int i;

		for (i = 0; i < KEYCODESIZ; i++)
			if (code == keycodelist[i]) break;
		if (i < KEYCODESIZ)
			snprintf2(buf, KEYWID + 1, "%-7.7s", keystrlist[i]);
#ifndef	CODEEUC
		else if (iskna(code))
			snprintf2(buf, KEYWID + 1, "'%c'    ", code);
#endif
		else if (isctl(code))
			snprintf2(buf, KEYWID + 1,
				"Ctrl-%c ", (code + '@') & 0x7f);
		else if (code < K_MIN && !ismsb(code))
			snprintf2(buf, KEYWID + 1, "'%c'    ", code);
		else return(0);
	}
	return(1);
}

static int NEAR checkline(y)
int y;
{
	if (y >= LFILEBOTTOM) {
		warning(0, HITKY_K);
		y = LFILETOP + 1;
	}
	return(y);
}

VOID help(mode)
int mode;
{
	char buf[20];
	int i, j, c, x, y;

	if (distributor && *distributor) {
		i = n_column - (int)strlen(distributor) - 24;
		locate(i, L_HELP);
		putch2('[');
		putterm(t_standout);
		cprintf2(" Distributed by: %s ", distributor);
		putterm(end_standout);
		putch2(']');
	}

	x = 0;
	y = LFILETOP;
	locate(0, y++);
	putterm(l_clear);

	for (i = 0; i < FUNCLISTSIZ ; i++) {
		if (i == NO_OPERATION || i == WARNING_BELL) continue;
		locate(x * (n_column / 2), y);
		if (x ^= 1) putterm(l_clear);
		else y++;
		if (mode && !(funclist[i].status & ARCH)) continue;

		c = 0;
		*buf = '\0';
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key >= 0; j++)
			if (i == (int)(bindlist[j].f_func)) {
				if ((c += code2str(buf,
					(int)(bindlist[j].key))) >= 2) break;
			}
		if (c < 2)
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key >= 0; j++)
			if (i == (int)(bindlist[j].d_func)) {
				if ((c += code2str(buf,
					(int)(bindlist[j].key))) >= 2) break;
			}
		if (!c) continue;

		cputs2("  ");
		if (c < 2) cprintf2("%-7.7s", " ");
		kanjiputs(buf);
		cputs2(": ");
		kanjiputs(mesconv(funclist[i].hmes, funclist[i].hmes_eng));

		y = checkline(y);
	}
	if (y > LFILETOP + 1) {
		if (x) y++;
		for (; y < LFILEBOTTOM; y++) {
			locate(0, y);
			putterm(l_clear);
		}
		warning(0, HITKY_K);
	}
}

#ifdef	USEMNTCTL
/*ARGSUSED*/
static FILE *NEAR setmntent(file, mode)
char *file, *mode;
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
	int len;

	if (mnt_ptr >= mnt_size) return(NULL);
	buf = (char *)fp;
	vmntp = (struct vmount *)&(buf[mnt_ptr]);

	cp = &(buf[mnt_ptr + vmntp -> vmt_data[VMT_OBJECT].vmt_off]);
	len = strlen(cp) + 1;
	if (!(vmntp -> vmt_flags & MNT_REMOTE)) {
		fsname = realloc2(fsname, len);
		strcpy(fsname, cp);
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
	strcpy(dir, cp);

	entp = getvfsbytype(vmntp -> vmt_gfstype);
	if (entp) {
		cp = entp -> vfsent_name;
		len = strlen(cp) + 1;
		type = realloc2(type, len);
		strcpy(type, cp);
	}
	else if (type) {
		free(type);
		type = NULL;
	}

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = (type) ? type : "???";
	mntp -> mnt_opts = (vmntp -> vmt_flags & MNT_READONLY) ? "ro" : "";

	mnt_ptr += vmntp -> vmt_length;
	return(mntp);
}
#endif	/* USEMNTCTL */

#if	defined (USEMNTINFOR) || defined (USEMNTINFO) || defined (USEGETFSSTAT)
/*ARGSUSED*/
static FILE *NEAR setmntent(file, mode)
char *file, *mode;
{
	struct statfs *buf;
	int size;

	buf = NULL;
	mnt_ptr = mnt_size = size = 0;
#ifdef	USEMNTINFO
	mnt_size = getmntinfo(&buf, MNT_NOWAIT);
#else
# ifdef	USEMNTINFOR
	getmntinfo_r(&buf, MNT_WAIT, &mnt_size, &size);
# else
	size = (getfsstat(NULL, 0, MNT_WAIT) + 1) * sizeof(struct statfs);
	if (size > 0) {
		buf = (struct statfs *)malloc2(mnt_size);
		mnt_size = getfsstat(buf, mnt_size, MNT_WAIT);
	}
# endif
#endif
	return((FILE *)buf);
}

#if	!defined (MNT_RDONLY) && defined (M_RDONLY)
#define	MNT_RDONLY	M_RDONLY
#endif

static mnt_t *NEAR getmntent2(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	struct statfs *buf;
#ifdef	USEMNTINFO
# ifdef	USEVFCNAME
	struct vfsconf *conf;
# define	getvfsbynumber(n)	((conf = getvfsbytype(n)) \
					? conf -> vfc_name : NULL)
# else	/* !USEVFCNAME */
#  ifdef	USEFFSTYPE
#  define	getvfsbynumber(n)	(buf[mnt_ptr].f_fstypename)
#  else
#   ifdef	INITMOUNTNAMES
	static char *mnt_names[] = INITMOUNTNAMES;
#   define	getvfsbynumber(n)	(((n) <= MOUNT_MAXTYPE) \
					? mnt_names[n] : NULL)
#   else
#   define	getvfsbynumber(n)	(NULL)
#   endif
#  endif
# endif	/* !USEVFCNAME */
#else	/* !USEMNTINFO */
# ifdef	USEGETFSSTAT
# define	getvfsbynumber(n)	(((n) <= MOUNT_MAXTYPE) \
					? mnt_names[n] : NULL)
# endif
#endif	/* !USEMNTINFO */
	char *cp;
	int len;

	if (mnt_ptr >= mnt_size) return(NULL);
	buf = (struct statfs *)fp;

	len = strlen(buf[mnt_ptr].f_mntfromname) + 1;
	fsname = realloc2(fsname, len);
	strcpy(fsname, buf[mnt_ptr].f_mntfromname);

	len = strlen(buf[mnt_ptr].f_mntonname) + 1;
	dir = realloc2(dir, len);
	strcpy(dir, buf[mnt_ptr].f_mntonname);

	cp = (char *)getvfsbynumber(buf[mnt_ptr].f_type);
	if (cp) {
		len = strlen(cp) + 1;
		type = realloc2(type, len);
		strcpy(type, cp);
	}
	else if (type) {
		free(type);
		type = NULL;
	}

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = (type) ? type : "???";
	mntp -> mnt_opts = (buf[mnt_ptr].f_flags & MNT_RDONLY) ? "ro" : "";

	mnt_ptr++;
	return(mntp);
}
#endif	/* USEMNTINFOR || USEMNTINFO || USEGETFSSTAT */

#ifdef	USEGETMNT
/*ARGSUSED*/
static FILE *NEAR setmntent(file, mode)
char *file, *mode;
{
	mnt_ptr = 0;
	return(NULL);
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
	int len;

	if (getmnt(&mnt_ptr, &buf, sizeof(struct fs_data),
		NOSTAT_MANY, NULL) <= 0) return(NULL);

	len = strlen(buf.fd_req.devname) + 1;
	fsname = realloc2(fsname, len);
	strcpy(fsname, buf.fd_req.devname);

	len = strlen(buf.fd_req.path) + 1;
	dir = realloc2(dir, len);
	strcpy(dir, buf.fd_req.path);

	len = strlen(gt_names[buf.fd_req.fstype]) + 1;
	type = realloc2(type, len);
	strcpy(type, gt_names[buf.fd_req.fstype]);

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = type;
	mntp -> mnt_opts = (buf.fd_req.flags & M_RONLY) ? "ro" : "";

	return(mntp);
}
#endif	/* USEGETMNT */

static int NEAR getfsinfo(path, fsbuf, mntbuf)
char *path;
statfs_t *fsbuf;
mnt_t *mntbuf;
{
#if	MSDOS
# ifndef	_NODOSDRIVE
	int i;
# endif

	mntbuf -> mnt_fsname = "MSDOS";
	mntbuf -> mnt_dir[0] = dospath(path, NULL);
	mntbuf -> mnt_dir[1] = ':';
	mntbuf -> mnt_dir[2] = _SC_;
	mntbuf -> mnt_dir[3] = '\0';
# ifdef	_NOUSELFN
	mntbuf -> mnt_type = MNTTYPE_PC;
# else	/* !_NOUSELFN */
	switch (supportLFN(mntbuf -> mnt_dir)) {
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
	mntbuf -> mnt_opts = "";

	if (statfs2(mntbuf -> mnt_dir, fsbuf) < 0) {
		if (errno == ENOENT) return(-1);
		memset((char *)fsbuf, 0xff, sizeof(statfs_t));
	}
#else	/* !MSDOS */
# if	!defined (USEMNTENTH) && !defined (USEGETFSENT)
	mnt_t mnt;
# endif
	mnt_t *mntp;
	FILE *fp;
	char *dir, fsname[MAXPATHLEN];
	int len, match;
#ifndef	_NODOSDRIVE
	int drv;
#endif

	dir = NULL;
	if (!strncmp(path, "/dev/", 4)) {
		if (_chdir2(path) < 0) dir = path;
	}
#ifndef	_NODOSDRIVE
	else if ((drv = dospath(path, NULL))) {
		static char dosmntdir[4];
		char buf[sizeof(long) * 3 + 1];

		mntbuf -> mnt_fsname = "";
		mntbuf -> mnt_dir = dosmntdir;
		dosmntdir[0] = drv;
		strcpy(&(dosmntdir[1]), ":\\");
		mntbuf -> mnt_type = (drv >= 'a' && drv <= 'z')
			? MNTTYPE_DOS7 : MNTTYPE_PC;
		mntbuf -> mnt_opts = "";
		if (dosstatfs(drv, buf) < 0) return(-1);
		if (buf[sizeof(long) * 3] & 001)
			mntbuf -> mnt_type = MNTTYPE_FAT32;
		fsbuf -> f_bsize = *((long *)&(buf[sizeof(long) * 0]));
# ifdef	USEFSDATA
		fsbuf -> fd_req.btot = calcKB((off_t)(fsbuf -> f_bsize),
			(off_t)(*((long *)&(buf[sizeof(long) * 1]))));
		fsbuf -> fd_req.bfree =
		fsbuf -> fd_req.bfreen = calcKB((off_t)(fsbuf -> f_bsize),
			(off_t)(*((long *)&(buf[sizeof(long) * 2]))));
# else	/* !USEFSDATA */
#  ifdef	USESTATVFSH
		fsbuf -> f_frsize = 0;
#  endif
		fsbuf -> f_blocks = *((long *)&(buf[sizeof(long) * 1]));
		fsbuf -> f_bfree =
		fsbuf -> f_bavail = *((long *)&(buf[sizeof(long) * 2]));
# endif	/* !USEFSDATA */
		fsbuf -> f_files = -1;
		return(0);
	}
#endif	/* !_NODOSDRIVE */
	else if (_chdir2(path) < 0) return(-1);

	if (!dir) {
		dir = getwd2();
		if (_chdir2(fullpath) < 0) error(fullpath);
		match = 0;

		fp = setmntent(MOUNTED, "r");
		for (;;) {
#ifdef	DEBUG
			_mtrace_file = "getmntent(start)";
			mntp = getmntent2(fp, &mnt);
			if (_mtrace_file) _mtrace_file = NULL;
			else {
				_mtrace_file = "getmntent(end)";
				malloc(0);	/* dummy malloc */
			}
			if (!mntp) break;
#else
			if (!(mntp = getmntent2(fp, &mnt))) break;
#endif
			if ((len = strlen(mntp -> mnt_dir)) < match
			|| strncmp(mntp -> mnt_dir, dir, len)
			|| (mntp -> mnt_dir[len - 1] != _SC_
			&& dir[len] && dir[len] != _SC_)) continue;
			match = len;
			strcpy(fsname, mntp -> mnt_fsname);
		}
		endmntent(fp);

		free(dir);
		if (!match) {
			errno = ENOENT;
			return(-1);
		}
		dir = fsname;
	}
	fp = setmntent(MOUNTED, "r");
	while ((mntp = getmntent2(fp, &mnt)))
		if (!strcmp(dir, mntp -> mnt_fsname)) break;
	endmntent(fp);
	if (!mntp) {
		errno = ENOENT;
		return(-1);
	}
	memcpy((char *)mntbuf, (char *)mntp, sizeof(mnt_t));

	if (statfs2(mntbuf -> mnt_dir, fsbuf) < 0
	&& (path == dir || statfs2(path, fsbuf) < 0))
		memset((char *)fsbuf, 0xff, sizeof(statfs_t));
#endif	/* !MSDOS */
	return(0);
}

static char *NEAR strmntopt(s1, s2)
char *s1, *s2;
{
	char *cp;
	int len;

	len = strlen(s2);
	for (cp = s1; cp && *cp;) {
		if (!strncmp(cp, s2, len) && (!cp[len] || cp[len] == ','))
			return(cp);
		if ((cp = strchr(cp, ','))) cp++;
	}
	return(NULL);
}

int writablefs(path)
char *path;
{
	statfs_t fsbuf;
	mnt_t mntbuf;
#if	!MSDOS && !defined (_NODOSDRIVE)
	int drv;
#endif

	if (Xaccess(path, R_OK | W_OK | X_OK) < 0) return(-1);
#if	!MSDOS && !defined (_NODOSDRIVE)
	if ((drv = dospath(path, NULL)))
		return((drv >= 'A' && drv <= 'Z') ? 4 : 5);
#endif
	if (getfsinfo(path, &fsbuf, &mntbuf) < 0
	|| hasmntopt2(&mntbuf, "ro")) return(0);

	if (!strcmp(mntbuf.mnt_type, MNTTYPE_PC)) return(4);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_DOS7)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_FAT32)) return(5);
#if	!MSDOS
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_43)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_42)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_UFS)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_FFS)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_JFS)) return(1);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_EFS)) return(2);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_SYSV)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_DGUX)) return(3);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_EXT2)) return(6);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_MSDOS))
# ifdef	LINUX
		return(4);
# else
		return(5);
# endif
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_UMSDOS)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_VFAT)) return(5);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_ADVFS)) return(0);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_VXFS)) return(0);
#endif
#ifdef	DARWIN
	/* Macintosh HFS+ is pseudo file system covered with skin */
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_HFS)) return(0);
#endif
	return(0);
}

/*ARGSUSED*/
off_t getblocksize(dir)
char *dir;
{
#if	MSDOS
	statfs_t fsbuf;

	if (statfs2(dir, &fsbuf) < 0) return((off_t)1024);
	return((off_t)blocksize(fsbuf));
#else	/* !MSDOS */
	statfs_t fsbuf;
	mnt_t mntbuf;
#ifndef	DEV_BSIZE
	struct stat st;
#endif

	if (!strcmp(dir, ".") && getfsinfo(dir, &fsbuf, &mntbuf) >= 0)
		return((off_t)blocksize(fsbuf));
#ifdef	DEV_BSIZE
	return(DEV_BSIZE);
#else
	if (Xstat(dir, &st) < 0) error(dir);
	return((off_t)(st.st_size));
#endif
#endif	/* !MSDOS */
}

static int NEAR info1line(y, ind, n, s, unit)
int y;
char *ind;
off_t n;
char *s, *unit;
{
	char buf[MAXCOLSCOMMA(3) + 1];
	int width;

	locate(0, y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	kanjiprintf("%-20.20s", ind);
	locate(n_column / 2 + 2, y);
	if (s) {
		width = n_column - (n_column / 2 + 2);
		kanjiputs2(s, width, 0);
	}
	else {
		cputs2(ascnumeric(buf, n, 3, MAXCOLSCOMMA(3)));
		kanjiprintf(" %s", unit);
	}
	return(checkline(++y));
}

off_t calcKB(block, byte)
off_t block, byte;
{
	if (block < (off_t)0 || byte <= (off_t)0) return((off_t)-1);
	if (byte == (off_t)1024) return(block);
	else if (byte > 1024) {
		byte = (byte + (off_t)512L) / (off_t)1024;
		return(block * byte);
	}
	else {
		byte = ((off_t)1024 + (byte / (off_t)2)) / byte;
		return(block / byte);
	}
}

int getinfofs(path, totalp, freep, bsizep)
char *path;
off_t *totalp, *freep, *bsizep;
{
	statfs_t fsbuf;
	mnt_t mntbuf;

#ifndef	_NODOSDRIVE
	needbavail = 1;
#endif
	if (getfsinfo(path, &fsbuf, &mntbuf) < 0) return(-1);
	*totalp = fsbuf.f_blocks;
	*freep = fsbuf.f_bavail;
	*bsizep = blocksize(fsbuf);
#ifndef	_NODOSDRIVE
	needbavail = 0;
#endif
	return(0);
}

int infofs(path)
char *path;
{
	statfs_t fsbuf;
	mnt_t mntbuf;
	int y;

#ifndef	_NODOSDRIVE
	needbavail = 1;
#endif
	if (getfsinfo(path, &fsbuf, &mntbuf) < 0) {
		warning(ENOTDIR, path);
#ifndef	_NODOSDRIVE
		needbavail = 0;
#endif
		return(0);
	}
	y = LFILETOP;
	locate(0, y++);
	putterm(l_clear);

	y = info1line(y, FSNAM_K, (off_t)0, mntbuf.mnt_fsname, NULL);
	y = info1line(y, FSMNT_K, (off_t)0, mntbuf.mnt_dir, NULL);
	y = info1line(y, FSTYP_K, (off_t)0, mntbuf.mnt_type, NULL);
	y = info1line(y, FSTTL_K,
		calcKB((off_t)(fsbuf.f_blocks), (off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(y, FSUSE_K,
		calcKB((off_t)(fsbuf.f_blocks - fsbuf.f_bfree),
			(off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(y, FSAVL_K,
		calcKB((off_t)(fsbuf.f_bavail), (off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(y, FSBSZ_K, (off_t)(fsbuf.f_bsize), NULL, "bytes");
	y = info1line(y, FSINO_K, (off_t)(fsbuf.f_files), NULL, UNIT_K);
	if (y > LFILETOP + 1) {
		for (; y < LFILEBOTTOM; y++) {
			locate(0, y);
			putterm(l_clear);
		}
		warning(0, HITKY_K);
	}
#ifndef	_NODOSDRIVE
	needbavail = 0;
#endif
	return(1);
}
