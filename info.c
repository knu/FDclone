/*
 *	info.c
 *
 *	Information Module
 */

#include "fd.h"
#include "term.h"
#include "funcno.h"
#include "kanji.h"

#include <mntent.h>
#include <sys/param.h>
#ifdef  USESTATVFS
#include <sys/statvfs.h>
typedef struct statvfs statfs_t;
#define	statfs2		statvfs
#else
#include <sys/vfs.h>
typedef struct statfs statfs_t;
#define	statfs2		statfs
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

void help(mode)
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
struct mntent *mntbuf;
{
	struct mntent *mntp;
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
		while (mntp = getmntent(fp)) {
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
	while (mntp = getmntent(fp))
		if (!strcmp(dir, mntp -> mnt_fsname)) break;
	endmntent(fp);
	if (!mntp) return(0);
	memcpy(mntbuf, mntp, sizeof(struct mntent));

	if (statfs2(mntbuf -> mnt_dir, fsbuf)) error(mntbuf -> mnt_dir);
	return(1);
}

int getblocksize()
{
	statfs_t buf;

	if (statfs2(".", &buf) < 0) error(".");
	return(buf.f_bsize);
}

int writablefs(path)
char *path;
{
	statfs_t fsbuf;
	struct mntent mntbuf;

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
	struct mntent mntbuf;
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
