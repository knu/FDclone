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
#include "unixdisk.h"
#include "mntinfo.h"
#include "fsinfo.h"

#define	KEYWID			7

#if	MSDOS
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
#ifndef	MNTTYPE_EXT4
#define	MNTTYPE_EXT4		"ext4"		/* Linux */
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
extern int XXputch __P_((int));
extern VOID XXcputs __P_((CONST char *));
extern int XXcprintf __P_((CONST char *, ...));
extern int Xattrprintf __P_((CONST char *, int, ...));
#else
#define	Xlocate			locate
#define	Xputterm		putterm
#define	XXputch			Xputch
#define	XXcputs			Xcputs
#define	XXcprintf		Xcprintf
#define	Xattrprintf		attrprintf
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
static int NEAR getfsinfo __P_((CONST char *, statfs_t *, mnt_t *));
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
	{FSID_LINUX, MNTTYPE_EXT4},
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
		VOID_C Xattrprintf(" Distributed by: %s ", 1, distributor);
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

	mntbuf -> Xmnt_fsname = "MSDOS";
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
		Xstrncpy(mntbuf -> Xmnt_dir, path, len);
	}
	else
# endif
	VOID_C gendospath(mntbuf -> Xmnt_dir, drive, _SC_);

# ifdef	DEP_DOSLFN
	switch (supportLFN(mntbuf -> Xmnt_dir)) {
		case 3:
			mntbuf -> Xmnt_type = MNTTYPE_SHARED;
			break;
		case 2:
			mntbuf -> Xmnt_type = MNTTYPE_FAT32;
			break;
		case 1:
			mntbuf -> Xmnt_type = MNTTYPE_DOS7;
			break;
#  ifdef	DEP_DOSDRIVE
		case -1:
			mntbuf -> Xmnt_fsname = "LFN emurate";
			mntbuf -> Xmnt_type = MNTTYPE_DOS7;
			break;
		case -2:
			mntbuf -> Xmnt_fsname = "BIOS raw";
			i = checkdrive(Xtoupper(mntbuf -> Xmnt_dir[0]) - 'A');
			if (i == PT_FAT12) mntbuf -> Xmnt_type = MNTTYPE_FAT12;
			else if (i == PT_FAT16)
				mntbuf -> Xmnt_type = MNTTYPE_FAT16;
			else if (i == PT_FAT16X)
				mntbuf -> Xmnt_type = MNTTYPE_FAT16X;
			else mntbuf -> Xmnt_type = MNTTYPE_FAT32;
			break;
#  endif
		case -4:
			return(seterrno(ENOENT));
/*NOTREACHED*/
			break;
		default:
			mntbuf -> Xmnt_type = MNTTYPE_PC;
			break;
	}
# else	/* !DEP_DOSLFN */
	mntbuf -> Xmnt_type = MNTTYPE_PC;
# endif	/* !DEP_DOSLFN */
	mntbuf -> Xmnt_opts = nullstr;

	if (Xstatfs(mntbuf -> Xmnt_dir, fsbuf) < 0) return(-1);
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
		mntbuf -> Xmnt_fsname = vnullstr;
		mntbuf -> Xmnt_dir = dosmntdir;
		dosmntdir[0] = drv;
		Xstrcpy(&(dosmntdir[1]), ":\\");
		mntbuf -> Xmnt_type =
			(Xislower(drv)) ? MNTTYPE_DOS7 : MNTTYPE_PC;
		mntbuf -> Xmnt_opts = vnullstr;
		if (dosstatfs(drv, buf) < 0) return(-1);
		if (buf[3 * sizeof(long)] & 001)
			mntbuf -> Xmnt_type = MNTTYPE_FAT32;
		fsbuf -> Xf_bsize = *((long *)&(buf[0 * sizeof(long)]));
#  ifdef	USEFSDATA
		fsbuf -> fd_req.btot = calcKB((off_t)(fsbuf -> Xf_bsize),
			(off_t)(*((long *)&(buf[1 * sizeof(long)]))));
		fsbuf -> fd_req.bfree =
		fsbuf -> fd_req.bfreen = calcKB((off_t)(fsbuf -> Xf_bsize),
			(off_t)(*((long *)&(buf[2 * sizeof(long)]))));
#  else	/* !USEFSDATA */
#   ifdef	USESTATVFSH
		fsbuf -> f_frsize = 0L;
#   endif
#   ifndef	NOFBLOCKS
		fsbuf -> Xf_blocks = *((long *)&(buf[1 * sizeof(long)]));
#   endif
#   ifndef	NOFBFREE
		fsbuf -> Xf_bfree =
		fsbuf -> Xf_bavail = *((long *)&(buf[2 * sizeof(long)]));
#   endif
#  endif	/* !USEFSDATA */
#  ifndef	NOFFILES
		fsbuf -> Xf_files = -1L;
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
			len = strlen(mntp -> Xmnt_dir);
			if (len < match || strncmp(mntp -> Xmnt_dir, dir, len)
			|| (mntp -> Xmnt_dir[len - 1] != _SC_
			&& dir[len] && dir[len] != _SC_))
				continue;
# ifdef	USEPROCMNT
			if (match && !strpathcmp(ROOTFS, mntp -> Xmnt_fsname))
				continue;
# endif
			match = len;
			Xstrcpy(fsname, mntp -> Xmnt_fsname);
		}
		Xendmntent(fp);

		if (!match) return(seterrno(ENOENT));
		cp = fsname;
	}

	if (!(fp = Xsetmntent(MOUNTED, "rb"))) return(-1);
	while ((mntp = Xgetmntent(fp, &mnt)))
		if (!strpathcmp(cp, mntp -> Xmnt_fsname)) break;
	Xendmntent(fp);
	if (!mntp) return(seterrno(ENOENT));
	memcpy((char *)mntbuf, (char *)mntp, sizeof(mnt_t));

	if (Xstatfs(mntbuf -> Xmnt_dir, fsbuf) >= 0) /*EMPTY*/;
	else if (path == cp || Xstatfs(path, fsbuf) < 0) return(-1);
#endif	/* !MSDOS */

	return(0);
}

#ifndef	NOFLOCK
int isnfs(path)
CONST char *path;
{
	mnt_t mntbuf;
	struct stat st;
	char *cp, buf[MAXPATHLEN];
	int n;

	if (Xstat(path, &st) < 0) return(-1);
	if (!s_isdir(&st)) {
		if (!(cp = strrdelim(path, 1))) return(-1);
		Xstrncpy(buf, path, cp - path);
		path = buf;
	}
	if (getfsinfo(path, NULL, &mntbuf) < 0) return(-1);
	n = Xstrncasecmp(mntbuf.Xmnt_type,
		MNTTYPE_XNFS, strsize(MNTTYPE_XNFS));

	return((n == 0) ? -1 : 0);
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
		if (!strcmp(mntbuf.Xmnt_type, mntlist[i].str))
			return(mntlist[i].no);

	return(0);
}

/*ARGSUSED*/
off_t getblocksize(dir)
CONST char *dir;
{
#if	MSDOS
	statfs_t fsbuf;

	if (Xstatfs(dir, &fsbuf) < 0) return((off_t)1024);

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
		if (Xstatfs(dir, &fsbuf) >= 0)
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
	*totalp = fsbuf.Xf_blocks;
#endif
#ifdef	NOFBFREE
	*freep = (off_t)0;
#else
	*freep = fsbuf.Xf_bavail;
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

	y = info1line(yy, y, FSNAM_K, (off_t)0, mntbuf.Xmnt_fsname, NULL);
	y = info1line(yy, y, FSMNT_K, (off_t)0, mntbuf.Xmnt_dir, NULL);
	y = info1line(yy, y, FSTYP_K, (off_t)0, mntbuf.Xmnt_type, NULL);
#ifndef	NOFBLOCKS
	y = info1line(yy, y, FSTTL_K,
		calcKB((off_t)(fsbuf.Xf_blocks), (off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
#endif
#ifndef	NOFBFREE
	y = info1line(yy, y, FSUSE_K,
		calcKB((off_t)(fsbuf.Xf_blocks - fsbuf.Xf_bfree),
			(off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(yy, y, FSAVL_K,
		calcKB((off_t)(fsbuf.Xf_bavail), (off_t)blocksize(fsbuf)),
		NULL, "Kbytes");
#endif
	y = info1line(yy, y, FSBSZ_K, (off_t)(fsbuf.Xf_bsize), NULL, "bytes");
#ifndef	NOFFILES
	y = info1line(yy, y, FSINO_K, (off_t)(fsbuf.Xf_files), NULL, UNIT_K);
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
