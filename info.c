/*
 *	info.c
 *
 *	Information Module
 */

#include "fd.h"
#include "term.h"
#include "funcno.h"
#include "kanji.h"

#include <sys/param.h>
#include <sys/file.h>

#if defined (USEMNTTAB)
#include <sys/mnttab.h>
typedef struct mnttab	mnt_t;
#define	setmntent		fopen
#define	getmntent2(fp, mnt)	(getmntent(fp, &mnt) ? NULL : &mnt)
#define	hasmntopt(mntp, opt)	strstr((mntp) -> mnt_mntopts, opt)
#define	endmntent		fclose
#define	mnt_dir		mnt_mountp
#define	mnt_fsname	mnt_special
#define	mnt_type	mnt_fstype
#else
# if defined (USEFSTABH)
# include <fstab.h>
typedef struct fstab	mnt_t;
# define	setmntent(filep, type)	(FILE *)(setfsent(), NULL)
# define	getmntent2(fp, mnt)	getfsent()
#  if defined (NOMNTOPS)
#  define	hasmntopt(mntp, opt)	strstr((mntp) -> fs_type, opt)
#  else
#  define	hasmntopt(mntp, opt)	strstr((mntp) -> fs_mntops, opt)
#  endif
# define	endmntent(fp)		endfsent()
# define	mnt_dir		fs_file
# define	mnt_fsname	fs_spec
#  ifdef	USENFSVFS
#  define	mnt_type	fs_type
#  else
#  define	mnt_type	fs_vfstype
#  endif
# else
# include <mntent.h>
typedef struct mntent	mnt_t;
# define	getmntent2(fp, mnt)	getmntent(fp)
# endif
#endif

#if defined (USESTATVFS)
#include <sys/statvfs.h>
typedef struct statvfs	statfs_t;
#define	statfs2			statvfs
#else
# if defined (USESTATFS)
# include <sys/statfs.h>
typedef struct statfs	statfs_t;
# define	statfs2(path, buf)	statfs(path, buf, sizeof(statfs_t), 0)
# define	f_bavail	f_bfree
# else
#  if defined (USEMOUNTH) || defined (USENFSVFS)
#  include <sys/mount.h>
#  else
#  include <sys/vfs.h>
#  endif
#  ifdef	USENFSVFS
#  include <netinet/in.h>
#  include <nfs/nfs_clnt.h>
#  include <nfs/vfs.h>
#  endif
typedef struct statfs	statfs_t;
# define	statfs2			statfs
# endif
#endif

#ifdef	USESYSDIR
#include <sys/dir.h>
#endif

extern bindtable bindlist[];
extern functable funclist[];
extern char fullpath[];

#ifndef	MNTTAB
#define	MNTTAB		"/etc/fstab"
#endif
#ifndef	MNTTYPE_NFS
#define	MNTTYPE_NFS	"nfs"
#endif

static int code2str();
static int checkline();
static int getfsinfo();
static char *inscomma();
static int info1line();


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
			if (i == bindlist[j].f_func) {
				if ((c += code2str(buf,
					bindlist[j].key)) >= 2) break;
			}
		if (c < 2)
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key > 0; j++)
			if (i == bindlist[j].d_func) {
				if ((c += code2str(buf,
					bindlist[j].key)) >= 2) break;
			}
		if (!c) continue;

		cputs("  ");
		if (c < 2) cprintf("%-7.7s", " ");
		cputs(buf);
		cputs(": ");
		cputs(funclist[i].hmes);

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

		fp = setmntent(MNTTAB, "r");
		while (mntp = getmntent2(fp, mnt)) {
			if ((len = strlen(mntp -> mnt_dir)) < match
			|| strncmp(mntp -> mnt_dir, dir, len)) continue;
			match = len;
			strcpy(fsname, mntp -> mnt_fsname);
		}
		endmntent(fp);

		free(dir);
		if (!match) return(0);
		dir = fsname;
	}
	fp = setmntent(MNTTAB, "r");
	while (mntp = getmntent2(fp, mnt))
		if (!strcmp(dir, mntp -> mnt_fsname)) break;
	endmntent(fp);
	if (!mntp) return(0);
	memcpy(mntbuf, mntp, sizeof(mnt_t));

	if (statfs2(mntbuf -> mnt_dir, fsbuf)) error(mntbuf -> mnt_dir);
	return(1);
}

int getblocksize()
{
#ifdef	DIRBLKSIZ
        return(DIRBLKSIZ);
#else
# ifdef	DEV_BSIZE
        return(DEV_BSIZE);
# else
	statfs_t buf;

	if (statfs2(".", &buf) < 0) error(".");
	return(buf.f_bsize);
# endif
#endif
}

int writablefs(path)
char *path;
{
	statfs_t fsbuf;
	mnt_t mntbuf;

	if (access(path, R_OK | W_OK | X_OK) < 0) return(-1);
	if (!getfsinfo(path, &fsbuf, &mntbuf)
	|| hasmntopt(&mntbuf, "ro")
	|| !strcmp(mntbuf.mnt_type, MNTTYPE_NFS)) return(0);
	return(1);
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
	cprintf("%-20.20s", ind);
	locate(n_column / 2 + 2, y);
	if (str) {
		width = n_column - (n_column / 2 + 2);
		cputs2(str, width, 0);
	}
	else {
		cprintf("%12.12s", inscomma(buf, n, 3));
		cprintf(" %s", unit);
	}
	return(checkline(++y));
}

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
	y = info1line(y, FSTTL_K,
		fsbuf.f_blocks * fsbuf.f_bsize / 1024, NULL, "Kbytes");
	y = info1line(y, FSUSE_K,
		(fsbuf.f_blocks - fsbuf.f_bfree) * fsbuf.f_bsize / 1024,
		NULL, "Kbytes");
	y = info1line(y, FSAVL_K,
		fsbuf.f_bavail * fsbuf.f_bsize / 1024, NULL, "Kbytes");
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
