/*
 *	info.c
 *
 *	Information Module
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#include <sys/param.h>
#include <sys/file.h>

#ifdef	USEMNTTABH
#include <sys/mnttab.h>
#define	MOUNTED		MNTTAB
typedef struct mnttab	mnt_t;
#define	setmntent		fopen
#define	getmntent2(fp, mntp)	(getmntent(fp, mntp) ? NULL : mntp)
#define	hasmntopt(mntp, opt)	strstr((mntp) -> mnt_mntopts, opt)
#define	endmntent		fclose
#define	mnt_dir		mnt_mountp
#define	mnt_fsname	mnt_special
#define	mnt_type	mnt_fstype
#endif	/* USEMNTTABH */

#ifdef	USEGETFSENT
#include <fstab.h>
typedef struct fstab	mnt_t;
#define	setmntent(file, mode)	(FILE *)(setfsent(), NULL)
#define	getmntent2(fp, mntp)	getfsent()
#define	hasmntopt(mntp, opt)	strstr((mntp) -> fs_mntops, opt)
#define	endmntent(fp)		endfsent()
#define	mnt_dir		fs_file
#define	mnt_fsname	fs_spec
#define	mnt_type	fs_vfstype
#endif	/* USEGETFSENT */

#ifdef	USEMNTCTL
#include <fshelp.h>
#include <sys/vfs.h>
#include <sys/mntctl.h>
#include <sys/vmount.h>
#endif

#if defined (USEMNTINFO) || defined (USEMNTINFOR)
#include <sys/mount.h>
#endif

#ifdef	USEGETMNT
#include <sys/fs_types.h>
#endif

#if defined (USEMNTCTL) || defined (USEMNTINFO) || defined (USEMNTINFOR)\
|| defined (USEGETMNT)
typedef struct _mnt_t {
	char *mnt_fsname;
	char *mnt_dir;
	char *mnt_type;
	char *mnt_opts;
} mnt_t;
static FILE *setmntent();
static mnt_t *getmntent2();
#define	hasmntopt(mntp, opt)	strstr((mntp) -> mnt_opts, opt)
#define	endmntent		free
static int mnt_ptr;
static int mnt_size;
#endif

#ifdef	USEMNTENTH
#include <mntent.h>
typedef struct mntent	mnt_t;
#define	getmntent2(fp, mntp)	getmntent(fp)
#endif	/* USEMNTENTH */


#ifdef	USESTATVFSH
#include <sys/statvfs.h>
typedef struct statvfs	statfs_t;
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
#endif	/* USEFSDATA */

#ifdef	USEFFSIZE
#define	f_bsize		f_fsize
#endif

#if defined (USESTATFSH) || defined (USEVFSH) || defined (USEMOUNTH)
# if (STATFSARGS >= 4)
# define	statfs2(path, buf)	statfs(path, buf, sizeof(statfs_t), 0)
# else
#  if (STATFSARGS == 3)
#  define	statfs2(path, buf)	statfs(path, buf, sizeof(statfs_t))
#  else
#  define	statfs2			statfs
#  endif
# endif
#endif

extern bindtable bindlist[];
extern functable funclist[];
extern char fullpath[];

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

static int code2str();
static int checkline();
static int getfsinfo();
static char *inscomma();
static int info1line();
#ifndef	USEFSDATA
static long calcKB();
#endif


static int code2str(buf, code)
char *buf;
int code;
{
	char *cp;

	buf = buf + strlen(buf);
	if (code == CR) strcpy(buf, "Ret    ");
	else if (code == ESC) strcpy(buf, "Esc    ");
	else if (code == '\t') strcpy(buf, "Tab    ");
	else if (code < ' ') sprintf(buf, "Ctrl-%c ", code + '@');
	else if (code < 0x100) sprintf(buf, "'%c'    ", code);
	else if (code >= K_F(1) && code <= K_F(20))
		sprintf(buf, "F%-6d", code - K_F0);
	else {
		switch (code) {
			case K_UP:
				cp = UPAR_K;
				break;
			case K_DOWN:
				cp = DWNAR_K;
				break;
			case K_RIGHT:
				cp = RIGAR_K;
				break;
			case K_LEFT:
				cp = LEFAR_K;
				break;
			case K_HOME:
				cp = "Home";
				break;
			case K_BS:
				cp = "Bs";
				break;
			case K_DC:
				cp = "Del";
				break;
			case K_IC:
				cp = "Ins";
				break;
			case K_CLR:
				cp = "Clr";
				break;
			case K_NPAGE:
				cp = "PageDn";
				break;
			case K_PPAGE:
				cp = "PageUp";
				break;
			default:
				return(0);
		}
		sprintf(buf, "%-7.7s", cp);
	}
	return(1);
}

static int checkline(y)
int y;
{
	if (y >= LSTACK) {
		locate(0, y);
		putterm(l_clear);
		warning(0, HITKY_K);
		y = WHEADER + 1;
	}
	return(y);
}

VOID help(mode)
int mode;
{
	char buf[20];
	int i, j, c, x, y;

	x = 0;
	y = WHEADER;
	locate(0, y++);
	putterm(l_clear);

	for (i = 0; i < NO_OPERATION; i++) {
		if (!funclist[i].hmes) continue;
		locate(x * (n_column / 2), y);
		if (x ^= 1) putterm(l_clear);
		else y++;
		if (mode && !(funclist[i].stat & ARCH)) continue;

		c = 0;
		*buf = '\0';
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key > 0; j++)
			if (i == (int)(bindlist[j].f_func)) {
				if ((c += code2str(buf,
					(int)(bindlist[j].key))) >= 2) break;
			}
		if (c < 2)
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key > 0; j++)
			if (i == (int)(bindlist[j].d_func)) {
				if ((c += code2str(buf,
					(int)(bindlist[j].key))) >= 2) break;
			}
		if (!c) continue;

		cputs("  ");
		if (c < 2) cprintf("%-7.7s", " ");
		kanjiputs(buf);
		cputs(": ");
		kanjiputs(mesconv(funclist[i].hmes, funclist[i].hmes_eng));

		y = checkline(y);
	}
	if (y > WHEADER + 1) {
		if (x) y++;
		for (; y <= LSTACK; y++) {
			locate(0, y);
			putterm(l_clear);
		}
		warning(0, HITKY_K);
	}
}

#ifdef	USEMNTCTL
static FILE *setmntent(file, mode)
char *file, *mode;
{
	char *buf;

	mntctl(MCTL_QUERY, sizeof(int), (struct vmount *)(&mnt_size));
	buf = (char *)malloc2(mnt_size);
	mntctl(MCTL_QUERY, mnt_size, (struct vmount *)buf);
	mnt_ptr = 0;
	return((FILE *)buf);
}

static mnt_t *getmntent2(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	static char *opts = NULL;
	struct vfs_ent *entp;
	struct vmount *vmntp;
	char *cp, *buf, *host;
	int len;

	if (mnt_ptr >= mnt_size) return(NULL);
	buf = (char *)fp;
	vmntp = (struct vmount *)(&buf[mnt_ptr]);

	cp = &buf[mnt_ptr + vmntp -> vmt_data[VMT_OBJECT].vmt_off];
	len = strlen(cp) + 1;
	if (!(vmntp -> vmt_flags & MNT_REMOTE))
		*(fsname = (char *)realloc2(fsname, len)) = '\0';
	else {
		host = &buf[mnt_ptr + vmntp -> vmt_data[VMT_HOSTNAME].vmt_off];
		len += strlen(host) + 1;
		fsname = (char *)realloc2(fsname, len);
		strcpy(fsname, host);
		strcat(fsname, ":");
	}
	strcat(fsname, cp);

	cp = &buf[mnt_ptr + vmntp -> vmt_data[VMT_STUB].vmt_off];
	len = strlen(cp) + 1;
	dir = (char *)realloc2(dir, len);
	strcpy(dir, cp);

	entp = (char *)getvfsbytype(vmntp -> vmt_gfstype);
	if (entp) {
		cp = entp -> vfsent_name;
		len = strlen(cp) + 1;
		type = (char *)realloc2(type, len);
		strcpy(type, cp);
	}
	else if (type) {
		free(type);
		type = NULL;
	}

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = type;
	mntp -> mnt_opts = (opts) ? opts : "";

	mnt_ptr += vmntp -> vmt_length;
	return(mntp);
}
#endif	/* USEMNTCTL */

#if defined (USEMNTINFO) || defined (USEMNTINFOR)
static FILE *setmntent(file, mode)
char *file, *mode;
{
	struct statfs *buf;
	int size;

	buf = NULL;
	mnt_ptr = mnt_size = size = 0;
#ifdef	USEMNTINFO
	mnt_size = getmntinfo(&buf, MNT_NOWAIT);
#else
	getmntinfo_r(&buf, MNT_WAIT, &mnt_size, &size);
#endif
	return((FILE *)buf);
}

#ifdef	USEMNTINFOR
#define	MNT_RDONLY	M_RDONLY
#endif

static mnt_t *getmntent2(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	struct statfs *buf;
#ifdef	USEMNTINFO
	struct vfsconf *conf;
#endif
	char *cp;
	int len;

	if (mnt_ptr >= mnt_size) return(NULL);
	buf = (struct statfs *)fp;

	len = strlen(buf[mnt_ptr].f_mntfromname) + 1;
	fsname = (char *)realloc2(fsname, len);
	strcpy(fsname, buf[mnt_ptr].f_mntfromname);

	len = strlen(buf[mnt_ptr].f_mntonname) + 1;
	dir = (char *)realloc2(dir, len);
	strcpy(dir, buf[mnt_ptr].f_mntonname);

#ifdef	USEMNTINFO
	conf = getvfsbytype(buf[mnt_ptr].f_type);
	cp = conf -> vfc_name;
#else
	cp = getvfsbynumber(buf[mnt_ptr].f_type);
#endif
	if (cp) {
		len = strlen(cp) + 1;
		type = (char *)realloc2(type, len);
		strcpy(type, cp);
	}
	else if (type) {
		free(type);
		type = NULL;
	}

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = type;
	mntp -> mnt_opts = (buf[mnt_ptr].f_flags & MNT_RDONLY) ? "ro" : "";

	mnt_ptr++;
	return(mntp);
}
#endif	/* USEMNTINFO || USEMNTINFOR */

#ifdef	USEGETMNT
static FILE *setmntent(file, mode)
char *file, *mode;
{
	mnt_ptr = 0;
	return(NULL);
}

static mnt_t *getmntent2(fp, mntp)
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
	fsname = (char *)realloc2(fsname, len);
	strcpy(fsname, buf.fd_req.devname);

	len = strlen(buf.fd_req.path) + 1;
	dir = (char *)realloc2(dir, len);
	strcpy(dir, buf.fd_req.path);

	len = strlen(gt_names[buf.fd_req.fstype]) + 1;
	type = (char *)realloc2(type, len);
	strcpy(type, gt_names[buf.fd_req.fstype]);

	mntp -> mnt_fsname = fsname;
	mntp -> mnt_dir = dir;
	mntp -> mnt_type = type;
	mntp -> mnt_opts = (buf.fd_req.flags & M_RONLY) ? "ro" : "";

	return(mntp);
}

#undef	endmntent
#define	endmntent(fp)
#endif	/* USEGETMNT */

static int getfsinfo(path, fsbuf, mntbuf)
char *path;
statfs_t *fsbuf;
mnt_t *mntbuf;
{
	mnt_t *mntp, mnt;
	FILE *fp;
	char *dir, fsname[MAXPATHLEN + 1];
	int len, match;

	dir = NULL;
	if (!strncmp(path, "/dev/", 4)) {
		if (chdir(path) < 0) dir = path;
	}
	else if (chdir(path) < 0) return(0);

	if (!dir) {
		dir = getwd2();
		if (chdir(fullpath) < 0) error(fullpath);
		match = 0;

		fp = setmntent(MOUNTED, "r");
		while (mntp = getmntent2(fp, &mnt)) {
			if ((len = strlen(mntp -> mnt_dir)) < match
			|| strncmp(mntp -> mnt_dir, dir, len)
			|| (mntp -> mnt_dir[len - 1] != '/'
			&& dir[len] && dir[len] != '/')) continue;
			match = len;
			strcpy(fsname, mntp -> mnt_fsname);
		}
		endmntent(fp);

		free(dir);
		if (!match) return(0);
		dir = fsname;
	}
	fp = setmntent(MOUNTED, "r");
	while (mntp = getmntent2(fp, &mnt))
		if (!strcmp(dir, mntp -> mnt_fsname)) break;
	endmntent(fp);
	if (!mntp) return(0);
	memcpy(mntbuf, mntp, sizeof(mnt_t));

	if (statfs2(mntbuf -> mnt_dir, fsbuf) < 0
	&& (path == dir || statfs2(path, fsbuf) < 0))
		memset((char *)fsbuf, 0xff, sizeof(statfs_t));
	return(1);
}

int writablefs(path)
char *path;
{
	statfs_t fsbuf;
	mnt_t mntbuf;

	if (access(path, R_OK | W_OK | X_OK) < 0) return(-1);
	if (!getfsinfo(path, &fsbuf, &mntbuf)
	|| hasmntopt(&mntbuf, "ro")) return(0);

	if (!strcmp(mntbuf.mnt_type, MNTTYPE_43)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_42)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_UFS)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_EXT2)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_JFS)) return(1);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_EFS)) return(2);
	else if (!strcmp(mntbuf.mnt_type, MNTTYPE_SYSV)
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_DGUX)) return(3);
	return(0);
}

static char *inscomma(buf, n, digit)
char *buf;
int n, digit;
{
	char tmp[12];
	int i, j, len;

	j = 0;
	if (n < 0) buf[j++] = '?';
	else {
		sprintf(tmp, "%d", n);
		len = strlen(tmp);
		for (i = j = 0; tmp[i]; i++) {
			buf[j++] = tmp[i];
			if (--len && !(len % digit)) buf[j++] = ',';
		}
	}
	buf[j] = '\0';
	return(buf);
}

static int info1line(y, ind, n, str, unit)
int y;
char *ind;
int n;
char *str, *unit;
{
	char buf[16];
	int width;

	locate(0, y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	kanjiprintf("%-20.20s", ind);
	locate(n_column / 2 + 2, y);
	if (str) {
		width = n_column - (n_column / 2 + 2);
		kanjiputs2(str, width, 0);
	}
	else {
		cprintf("%12.12s", inscomma(buf, n, 3));
		kanjiprintf(" %s", unit);
	}
	return(checkline(++y));
}

#ifndef	USEFSDATA
static long calcKB(block, byte)
long block, byte;
{
	if (block < 0 || byte <= 0) return(-1);
	if (byte >= 1024) {
		byte = (byte + 512) / 1024;
		return(block * byte);
	}
	else {
		byte = (1024 + (byte / 2)) / byte;
		return(block / byte);
	}
}
#endif

int infofs(path)
char *path;
{
	statfs_t fsbuf;
	mnt_t mntbuf;
	int y;

	if (!getfsinfo(path, &fsbuf, &mntbuf)) {
		warning(ENOTDIR, path);
		return(0);
	}
	y = WHEADER;
	locate(0, y++);
	putterm(l_clear);

	y = info1line(y, FSNAM_K, 0, mntbuf.mnt_fsname, NULL);
	y = info1line(y, FSMNT_K, 0, mntbuf.mnt_dir, NULL);
	y = info1line(y, FSTYP_K, 0, mntbuf.mnt_type, NULL);
#ifdef	USEFSDATA
	y = info1line(y, FSTTL_K, fsbuf.fd_req.btot, NULL, "Kbytes");
	y = info1line(y, FSUSE_K,
		fsbuf.fd_req.btot - fsbuf.fd_req.bfree, NULL, "Kbytes");
	y = info1line(y, FSAVL_K, fsbuf.fd_req.bfreen, NULL, "Kbytes");
#else
	y = info1line(y, FSTTL_K,
		calcKB(fsbuf.f_blocks, blocksize(fsbuf)), NULL, "Kbytes");
	y = info1line(y, FSUSE_K,
		calcKB(fsbuf.f_blocks - fsbuf.f_bfree, blocksize(fsbuf)),
		NULL, "Kbytes");
	y = info1line(y, FSAVL_K,
		calcKB(fsbuf.f_bavail, blocksize(fsbuf)), NULL, "Kbytes");
#endif
	y = info1line(y, FSBSZ_K, fsbuf.f_bsize, NULL, "bytes");
	y = info1line(y, FSINO_K, fsbuf.f_files, NULL, UNIT_K);
	if (y > WHEADER + 1) {
		for (; y <= LSTACK; y++) {
			locate(0, y);
			putterm(l_clear);
		}
		warning(0, HITKY_K);
	}
	return(1);
}
