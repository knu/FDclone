/*
 *	dosdisk.c
 *
 *	MSDOS Disk Access Mofule
 */

#include "machine.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#ifdef	USEUTIME
#include <utime.h>
#endif

#ifdef	USEDIRECT
#include <sys/dir.h>
#define	dirent	direct
#else
#include <dirent.h>
#endif

#include "dosdisk.h"

#ifdef	NOERRNO
extern int errno;
#endif

#if !defined (ENOTEMPTY) && defined (ENFSNOTEMPTY)
#define	ENOTEMPTY	ENFSNOTEMPTY
#endif

extern time_t timelocal2();

#define	S_IEXEC_ALL	(S_IEXEC | (S_IEXEC >> 3) | (S_IEXEC >> 6))

static int shiftcache();
static int savecache();
static int loadcache();
static int sectseek();
static int sectread();
static int sectwrite();
static int readfat();
static int writefat();
static u_char *getfatofs();
static long getfat();
static int putfat();
static long clust2sect();
static long newclust();
static long clustread();
static long clustwrite();
static long clustexpand();
static int clustfree();
static int readbpb();
static int opendev();
static int closedev();
static int transchar();
static int transname();
static char *getdosname();
static char *putdosname();
static u_short getdosmode();
static u_char putdosmode();
static time_t getdostime();
static u_char *putdostime();
static int getdrive();
static int parsepath();
static int _dosreaddir();
static dosDIR *splitpath();
static int readdent();
static int writedent();
static int creatdent();
static int dosfilbuf();
static int dosflsbuf();

devinfo fdtype[MAXDRIVEENTRY] = {
#if defined (SUN_OS)
# if defined (sparc)
	{'A', "/dev/rfd0c", 2, 18, 80},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0c", 2, 8 + 100, 80},
# endif
#endif
#if defined (NEWS_OS3) || defined (NEWS_OS4) || defined (NEWS_OS41)
# if defined (news800) || defined (news900) \
|| defined (news1800) || defined (news1900) || defined (news3800)
	{'A', "/dev/rfh0a", 2, 18, 80},
	{'A', "/dev/rfd0a", 2, 9, 80},
	{'A', "/dev/rfd0a", 2, 8 + 100, 80},
# else
#  if defined (news3100) || defined(news5000)
	{'A', "/dev/rfd00a", 2, 18, 80},
	{'A', "/dev/rfd00a", 2, 9, 80},
	{'A', "/dev/rfd00a", 2, 8 + 100, 80},
#  else
	{'A', "/dev/rfd00a", 2, 18, 80},
	{'A', "/dev/rfd01a", 2, 9, 80},
	{'A', "/dev/rfd03a", 2, 8, 80},
#  endif
# endif
#endif
#if defined (NEWS_OS6)
	{'A', "/dev/rfloppy/c0d0u0p0", 2, 18, 80},
	{'A', "/dev/rfloppy/c0d0u1p0", 2, 9, 80},
	{'A', "/dev/rfloppy/c0d0u3p0", 2, 8, 80},
#endif
#if defined (HPUX)
# if defined (__hp9000s700)
	{'A', "/dev/rfloppy/c201d0s0", 2, 18, 80},
	{'A', "/dev/rfloppy/c201d0s0", 2, 9, 80},
	{'A', "/dev/rfloppy/c201d0s0", 2, 8 + 100, 80},
# endif
#endif
#if defined (EWSUXV)
	{'A', "/dev/rif/f0h18", 2, 18, 80},
	{'A', "/dev/rif/f0h8", 2, 8, 77},
	{'A', "/dev/rif/f0c15", 2, 15, 80},
	{'A', "/dev/rif/f0d9", 2, 9, 80},
	{'A', "/dev/rif/f0d8", 2, 8, 80},
#endif
#if defined (AIX)
	{'A', "/dev/rfd0h", 2, 18, 80},
	{'A', "/dev/rfd0l", 2, 9, 80},
	{'A', "/dev/rfd0l", 2, 8 + 100, 80},
#endif
#if defined (ULTRIX)
	{'A', "/dev/rfh0a", 2, 18, 80},
	{'A', "/dev/rfd0a", 2, 9, 80},
	{'A', "/dev/rfd0a", 2, 8 + 100, 80},
#endif
#if defined (LINUX)
	{'A', "/dev/fd0", 2, 18, 80},
	{'A', "/dev/fd0", 2, 9, 80},
	{'A', "/dev/fd0", 2, 8 + 100, 80},
#endif
#if defined (JCCBSD)
	{'A', "/dev/rfd0a", 2, 18, 80},
	{'A', "/dev/rfd0b", 2, 8, 77},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0d", 2, 8, 80},
#endif
#if defined (NETBSD)
# if defined (i386)
	{'A', "/dev/rfd0a", 2, 15, 80},
# endif
#endif
#if defined (BSDOS)
# if defined (i386)
	{'A', "/dev/rfd0c", 2, 18, 80},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0c", 2, 8 + 100, 80},
# endif
#endif
#if defined (ORG_386BSD)
# if defined (i386)
	{'A', "/dev/rfd0a", 2, 15, 80},
	{'A', "/dev/rfd0b", 2, 8, 77},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0c", 2, 8 + 100, 80},
# endif
#endif
	{'\0', NULL, 0, 0, 0}
};

int lastdrive = -1;

static char *curdir['Z' - 'A' + 1];
static cache_t *dentcache['Z' - 'A' + 1];
static devstat devlist[DOSNOFILE];
static int maxdev = 0;
static dosFILE dosflist[DOSNOFILE];
static int maxdosf = 0;
static u_char cachedrive[SECTCACHESIZE];
static long sectno[SECTCACHESIZE];
static char *sectcache[SECTCACHESIZE];
static int maxsectcache = 0;


static int sectseek(devp, sect)
devstat *devp;
long sect;
{
	if (devp -> flags & F_8SECT) sect += sect / 8;
	if (lseek(devp -> fd, sect * (devp -> sectsize), L_SET) < 0) return(-1);
	return(0);
}

static int shiftcache(n)
int n;
{
	char *cp;
	long no;
	int i, drive;

	if (n < 0) {
		i = 0;
		free(sectcache[0]);
	}
	else {
		i = n;
		drive = cachedrive[n];
		no = sectno[n];
		cp = sectcache[n];
	}
	for (; i < maxsectcache - 1; i++) {
		cachedrive[i] = cachedrive[i + 1];
		sectno[i] = sectno[i + 1];
		sectcache[i] = sectcache[i + 1];
	}
	if (n < 0) return(0);
	cachedrive[i] = drive;
	sectno[i] = no;
	sectcache[i] = cp;
	return(1);
}

static int savecache(devp, sect, buf)
devstat *devp;
long sect;
char *buf;
{
	char *cp;
	int i, size;

	size = devp -> sectsize;
	for (i = 0; i < maxsectcache; i++)
	if (devp -> drive == cachedrive[i] && sect == sectno[i]) {
		memcpy(sectcache[i], buf, size);
		shiftcache(i);
		return(size);
	}
	if (!(cp = (char *)malloc(size))) return(-1);
	if (i < SECTCACHESIZE) maxsectcache = i + 1;
	else {
		i--;
		shiftcache(-1);
	}
	cachedrive[i] = devp -> drive;
	sectno[i] = sect;
	sectcache[i] = cp;
	memcpy(sectcache[i], buf, size);
	return(0);
}

static int loadcache(devp, sect, buf)
devstat *devp;
long sect;
char *buf;
{
	int i;

	for (i = 0; i < maxsectcache; i++)
	if (devp -> drive == cachedrive[i] && sect == sectno[i]) {
		memcpy(buf, sectcache[i], devp -> sectsize);
		shiftcache(i);
		return(0);
	}
	return(-1);
}

static int sectread(devp, sect, buf, n)
devstat *devp;
long sect;
char *buf;
int n;
{
	int i;

	for (i = 0; i < n; i++, sect++, buf += devp -> sectsize) {
		if (loadcache(devp, sect, buf) >= 0) continue;
		if (sectseek(devp, sect) < 0) return(-1);
		while (read(devp -> fd, buf, devp -> sectsize) < 0)
			if (errno != EINTR) return(-1);
		savecache(devp, sect, buf);
	}
	return(n * (devp -> sectsize));
}

static int sectwrite(devp, sect, buf, n)
devstat *devp;
long sect;
char *buf;
int n;
{
	int i;

	if (devp -> flags & F_RONLY) {
		errno = EROFS;
		return(-1);
	}
	for (i = 0; i < n; i++, sect++, buf += devp -> sectsize) {
		savecache(devp, sect, buf);
		if (sectseek(devp, sect) < 0) return(-1);
		if (write(devp -> fd, buf, devp -> sectsize) < 0)
			if (errno != EINTR) return(-1);
	}
	return(n * (devp -> sectsize));
}

static int readfat(devp)
devstat *devp;
{
	return(sectread(devp, devp -> fatofs, devp -> fatbuf, devp -> fatsize));
}

static int writefat(devp)
devstat *devp;
{
	int i;

	i = (devp -> flags & F_WRFAT);
	devp -> flags &= ~F_WRFAT;
	if (!i) return(0);

	if (devp -> flags & F_DUPL) for (i = 0; i < maxdev; i++)
		if (devlist[i].drive == devp -> drive
		&& !(devlist[i].flags & F_DUPL)) {
			devlist[i].flags |= F_WRFAT;
			return(0);
		}

	for (i = devp -> fatofs; i < devp -> dirofs; i += devp -> fatsize)
		if (sectwrite(devp, i, devp -> fatbuf, devp -> fatsize) < 0)
			return(-1);
	return(0);
}

static u_char *getfatofs(devp, clust)
devstat *devp;
long clust;
{
	long ofs;
	int bit;

	bit = (devp -> flags & F_16BIT) ? 4 : 3;
	ofs = (clust * bit) / 2;
	if (ofs < bit || ofs >= (devp -> fatsize) * (devp -> sectsize)) {
		errno = EIO;
		return(NULL);
	}
	return((u_char *)&(devp -> fatbuf[ofs]));
}

static long getfat(devp, clust)
devstat *devp;
long clust;
{
	u_char *fatp;
	long ret;

	if (!(fatp = getfatofs(devp, clust))) return(-1);
	if (devp -> flags & F_16BIT) {
		ret = (*fatp & 0xff) + ((*(fatp + 1) & 0xff) << 8);
		if (ret >= 0x0fff8) ret = 0xffff;
	}
	else {
		if (clust % 2) ret = (((*fatp) & 0xf0) >> 4)
				+ ((*(fatp + 1) & 0xff) << 4);
		else ret= (*fatp & 0xff) + ((*(fatp + 1) & 0x0f) << 8);
		if (ret >= 0x0ff8) ret = 0xffff;
	}
	return(ret);
}

static int putfat(devp, clust, n)
devstat *devp;
long clust, n;
{
	u_char *fatp;

	if (!(fatp = getfatofs(devp, clust))) return(-1);
	if (devp -> flags & F_16BIT) {
		*fatp = n & 0xff;
		*(fatp + 1) = (n >> 8) & 0xff;
	}
	else if (clust % 2) {
		*fatp &= 0x0f;
		*fatp |= (n << 4) & 0xf0;
		*(fatp + 1) = (n >> 4) & 0xff;
	}
	else {
		*fatp = n & 0xff;
		*(fatp + 1) &= 0xf0;
		*(fatp + 1) |= (n >> 8) & 0x0f;
	}
	devp -> flags |= F_WRFAT;
	return(0);
}

static long clust2sect(devp, clust)
devstat *devp;
long clust;
{
	long sect;

	errno = 0;
	if (!clust || clust > 0xffff) {
		sect = (clust) ? clust - 0x10000 : devp -> dirofs;
		if (sect >= (devp -> dirofs) + (devp -> dirsize)) return(-1);
	}
	else {
		if (clust == 0xffff) return(-1);
		sect = ((clust - 2) * (int)(devp -> clustsize))
			+ ((devp -> dirofs) + (devp -> dirsize));
	}
	return(sect);
}

static long newclust(devp)
devstat *devp;
{
	long clust, used;

	for (clust = 2; (used = getfat(devp, clust)) >= 0; clust++)
		if (!used) break;
	if (used < 0) {
		errno = ENOSPC;
		return(-1);
	}
	return(clust);
}

static long clustread(devp, buf, clust)
devstat *devp;
u_char *buf;
long clust;
{
	long sect, next;

	if ((sect = clust2sect(devp, clust)) < 0) return(-1);
	if (sect < (devp -> dirofs) + (devp -> dirsize))
		next = 0x10000 + sect + (devp -> clustsize);
	else next = getfat(devp, clust);

	if (buf && sectread(devp, sect, buf, devp -> clustsize) < 0) return(-1);
	return(next);
}

static long clustwrite(devp, buf, prev)
devstat *devp;
char *buf;
long prev;
{
	long sect, clust;

	if ((clust = newclust(devp)) < 0) return(-1);
	if ((sect = clust2sect(devp, clust)) < 0) {
		errno = EIO;
		return(-1);
	}
	if (buf && sectwrite(devp, sect, buf, devp -> clustsize) < 0)
		return(-1);

	if ((prev && putfat(devp, prev, clust) < 0)
	|| putfat(devp, clust, 0xffff) < 0) return(-1);
	return(clust);
}

static long clustexpand(devp, clust, fill)
devstat *devp;
long clust;
int fill;
{
	char *buf;
	long new, size;

	if (!fill) buf = NULL;
	else {
		size = (devp -> clustsize) * (devp -> sectsize);
		if (!(buf = (char *)malloc(size))) return(-1);
		memset(buf, 0, size);
	}
	new = clustwrite(devp, buf, clust);
	if (buf) free(buf);
	return(new);
}

static int clustfree(devp, clust)
devstat *devp;
long clust;
{
	long next;

	if (clust) for (;;) {
		next = getfat(devp, clust);
		if (putfat(devp, clust, 0) < 0) return(-1);
		if (next == 0xffff) break;
		clust = next;
	}
	return(0);
}

static int readbpb(devp, bpbcache)
devstat *devp;
bpb_t *bpbcache;
{
	bpb_t *bpb;
	char buf[SECTSIZE];
	long total;
	int i, fd, ch_sect;

	if (!(devp -> ch_head) || !(ch_sect = devp -> ch_sect)) return(0);

	if (ch_sect <= 100) devp -> flags &= ~F_8SECT;
	else {
		ch_sect %= 100;
		devp -> flags |= F_8SECT;
	}

	if (bpbcache -> nfat) bpb = bpbcache;
	else {
		devp -> fd = -1;
		bpb = (bpb_t *)buf;
		if ((fd = open(devp -> ch_name, O_RDWR, 0600)) < 0) {
			if (errno == EIO) errno = ENXIO;
			if (errno != EROFS
			|| (fd = open(devp -> ch_name, O_RDONLY, 0600)) < 0)
				return(-1);
			devp -> flags |= F_RONLY;
		}
		if (lseek(fd, 0L, L_SET) < 0) {
			close(fd);
			return(0);
		}

		while ((i = read(fd, buf, SECTSIZE)) < 0 && errno == EINTR);
		if (i < 0) {
			close(fd);
			return(0);
		}
		devp -> fd = fd;
	}

	total = byte2word(bpb -> total);
	if (!total) total = byte2word(bpb -> bigtotal_l)
		+ (byte2word(bpb -> bigtotal_h) << 16);
	if (byte2word(bpb -> secttrack) != ch_sect
	|| byte2word(bpb -> nhead) != devp -> ch_head
	|| total / ((devp -> ch_head) * ch_sect) != devp -> ch_cyl) {
		if (bpb != bpbcache) memcpy(bpbcache, bpb, sizeof(bpb_t));
		return(0);
	}

	devp -> clustsize = bpb -> clustsize;
	devp -> sectsize = byte2word(bpb -> sectsize);
	devp -> fatofs = byte2word(bpb -> bootsect);
	devp -> fatsize = byte2word(bpb -> fatsize);
	devp -> dirofs = (devp -> fatofs) + (devp -> fatsize) * (bpb -> nfat);
	devp -> dirsize = byte2word(bpb -> maxdir) * DOSDIRENT
		/ devp -> sectsize;
	total -= (devp -> dirofs) + (devp -> dirsize);
	if (total / (devp -> clustsize) > MAX12BIT) devp -> flags |= F_16BIT;

	return(1);
}

static int opendev(drive)
int drive;
{
	bpb_t bpb;
	devstat dev;
	char *cp;
	int i, ret;

	if (!drive) {
		errno = ENODEV;
		return(-1);
	}
	if (maxdev >= DOSNOFILE) {
		errno = EMFILE;
		return(-1);
	}

	for (i = maxdev - 1; i >= 0; i--) {
		if (drive == (int)devlist[i].drive) {
			memcpy(&dev, &devlist[i], sizeof(devstat));
			dev.flags |= F_DUPL;
			dev.flags &= ~(F_CACHE | F_WRFAT);
			break;
		}
	}

	if (i < 0) {
		cp = NULL;
		for (i = 0; fdtype[i].name; i++) {
			if (drive != (int)fdtype[i].drive) continue;
			if (!cp || strcmp(cp, fdtype[i].name)) {
				bpb.nfat = 0;
				memset(&dev, 0, sizeof(devstat));
				dev.fd = -1;
			}
			memcpy(&dev, &fdtype[i], sizeof(devinfo));
			cp = fdtype[i].name;
			if ((ret = readbpb(&dev, &bpb)) < 0) return(-1);
			if (ret > 0) break;
		}
		if (!fdtype[i].name) {
			if ((dev.fd) >= 0) close(dev.fd);
			errno = ENODEV;
			return(-1);
		}
		dev.dircache = NULL;
		dev.fatbuf = (char *)malloc((dev.fatsize) * (dev.sectsize));
		if (!dev.fatbuf || readfat(&dev) < 0) {
			if (dev.fatbuf) free(dev.fatbuf);
			close(dev.fd);
			return(-1);
		}
	}

	memcpy(&devlist[maxdev], &dev, sizeof(devstat));
	return(maxdev++);
}

static int closedev(dd)
int dd;
{
	int i, j, ret;

	if (dd < 0 || dd >= maxdev || !devlist[dd].drive) {
		errno = EBADF;
		return(-1);
	}

	ret = 0;
	if (devlist[dd].flags & F_CACHE) free(devlist[dd].dircache);
	if (!(devlist[dd].flags & F_DUPL)) {
		for (i = 0; i < maxdev; i++) {
			if (i == dd || devlist[i].drive != devlist[dd].drive
			|| !(devlist[i].flags & F_DUPL)) continue;
			if (devlist[i].flags & F_CACHE)
				free(devlist[i].dircache);
			devlist[i].drive = '\0';
		}
		for (i = j = 0; i < maxsectcache; i++) {
			if (cachedrive[i] == devlist[dd].drive)
				free(sectcache[i]);
			else if (cachedrive[i]) continue;
			if (j <= i) j = i + 1;
			for (; j < maxsectcache; j++) {
				if (cachedrive[j] == devlist[dd].drive) {
					free(sectcache[j]);
					cachedrive[j] = '\0';
				}
				else if (cachedrive[j]) break;
			}
			if (j >= maxsectcache) {
				maxsectcache = i;
				break;
			}
			cachedrive[i] = cachedrive[j];
			sectno[i] = sectno[j];
			sectcache[i] = sectcache[j];
			cachedrive[j++] = '\0';
		}
		if (writefat(&devlist[dd]) < 0) ret = -1;
		free(devlist[dd].fatbuf);
		close(devlist[dd].fd);
	}

	devlist[dd].drive = '\0';
	while (maxdev > 0 && !devlist[maxdev - 1].drive) maxdev--;
	return(ret);
}

int preparedrv(drive, func)
int drive;
int (*func)();
{
	int i;

	for (i = maxdev - 1; i >= 0; i--)
		if (drive == (int)devlist[i].drive) return(DOSNOFILE + i);
	if (func) (*func)();
	return(opendev(drive));
}

int shutdrv(dd)
int dd;
{
	if (dd >= DOSNOFILE) return(0);
	return(closedev(dd));
}

static int transchar(c)
int c;
{
	if (islower(c)) return(c + 'A' - 'a');
	if (c == '+') return('`');
	if (c == ',') return('\'');
	if (c == '[') return('&');
	if (c == '.') return('$');
	return(c);
}

static int transname(buf, path, len)
char *buf, *path;
int len;
{
	char *cp;
	int i, j;

	for (i = len - 1; i >= 0; i--) if (path[i] == '.') break;
	if (i <= 0) i = len;
	cp = &path[i];

	for (i = 0, j = 0; i < 8 && &path[j] < cp; i++, j++)
		buf[i] = transchar(path[j]);
	if (++cp < &path[len]) {
		buf[i++] = '.';
		j = cp - path;
		if (len - j > 3) len = j + 3;
		for (; j < len; i++, j++) buf[i] = transchar(path[j]);
	}
	buf[i] = '\0';
	return(i);
}

static char *getdosname(buf, name, ext)
char *buf, *name, *ext;
{
	int i, len;

	for (i = 0; i < 8 && name[i] != ' '; i++) buf[i] = name[i];
	len = i;
	if (*ext != ' ') {
		buf[len++] = '.';
		for (i = 0; i < 3 && ext[i] != ' '; i++) buf[len + i] = ext[i];
		len += i;
	}
	buf[len] = '\0';

	return(buf);
}

static char *putdosname(buf, file)
char *buf, *file;
{
	char *cp;
	int i;

	if ((cp = strrchr(file, '.')) == file) cp = NULL;
	for (i = 0; i < 8; i++)
		buf[i] = (file != cp && *file) ? transchar(*(file++)) : ' ';
	if (cp) cp++;
	for (; i < 11; i++) buf[i] = (cp && *cp) ? transchar(*(cp++)) : ' ';
	buf[i] = '\0';

	return(buf);
}

static u_short getdosmode(attr)
u_char attr;
{
	u_short mode;

	mode = 0;
	if (!(attr & DS_IHIDDEN)) mode |= S_IREAD;
	if (!(attr & DS_IRDONLY)) mode |= S_IWRITE;
	mode |= (mode >> 3) | (mode >> 6);
	if (attr & DS_IFDIR) mode |= S_IFDIR;
	else if (attr & DS_IFLABEL) mode |= S_IFIFO;
	else if (attr & DS_IFSYSTEM) mode |= S_IFSOCK;
	else mode |= S_IFREG;

	return(mode);
}

static u_char putdosmode(mode)
u_short mode;
{
	u_char attr;

	attr = 0;
	if (!(mode & S_IREAD)) attr |= DS_IHIDDEN;
	if (!(mode & S_IWRITE)) attr |= DS_IRDONLY;
	switch (mode & S_IFMT) {
		case S_IFDIR:
			attr |= DS_IFDIR;
			break;
		case S_IFIFO:
			attr |= DS_IFLABEL;
			break;
		case S_IFSOCK:
			attr |= DS_IFSYSTEM;
			break;
		default:
			break;
	}

	return(attr);
}

static time_t getdostime(date, time)
int date, time;
{
	struct tm tm;

	tm.tm_year = 1980 + ((date >> 9) & 0x7f);
	tm.tm_mon = (date >> 5) & 0x0f - 1;
	tm.tm_mday = date & 0x1f;
	tm.tm_hour = (time >> 11) & 0x1f;
	tm.tm_min = (time >> 5) & 0x3f;
	tm.tm_sec = (time << 1) & 0x3e;

	return(timelocal2(&tm));
}

static u_char *putdostime(buf, clock)
u_char *buf;
time_t clock;
{
	struct timeval t;
	struct timezone tz;
	struct tm *tm;
	int date, time;

	if (clock < 0) gettimeofday(&t, &tz);
	else t.tv_sec = clock;
	tm = localtime(&(t.tv_sec));
	date = (((tm -> tm_year - 80) & 0x7f) << 9)
		+ (((tm -> tm_mon + 1) & 0x0f) << 5)
		+ (tm -> tm_mday & 0x1f);
	time = ((tm -> tm_hour & 0x1f) << 11)
		+ ((tm -> tm_min & 0x3f) << 5)
		+ ((tm -> tm_sec & 0x3e) >> 1);

	if (!time && clock < 0) time = 0x0001;
	buf[0] = time & 0xff;
	buf[1] = (time >> 8) & 0xff;
	buf[2] = date & 0xff;
	buf[3] = (date >> 8) & 0xff;

	return(buf);
}

static int getdrive(path)
char *path;
{
	int i, drive;

	if (islower(drive = *(path++))) drive += 'A' - 'a';
	if (!isalpha(drive) || *(path++) != ':') {
		errno = ENOENT;
		return(-1);
	}

	if (lastdrive < 0) {
		for (i = 0; i < 'Z' - 'A' + 1; i++) {
			curdir[i] = NULL;
			dentcache[i] = NULL;
		}
		lastdrive = drive;
	}

	return(drive);
}

static int parsepath(buf, path, class)
char *buf, *path;
int class;
{
	char *cp;
	int i, len, drive;

	cp = buf;
	if ((drive = getdrive(path)) < 0) return(-1);
	path += 2;
	if (*path != '/' && *path != '\\' && curdir[drive - 'A']) {
		strcpy(buf, curdir[drive - 'A']);
		cp += strlen(buf);
	}

	while (*path) {
		for (i = 0; path[i] && path[i] != '/' && path[i] != '\\'; i++);
		if (class && !path[i]) {
			*(cp++) = '/';
			if (i == 1 && *path == '.') *(cp++) = '.';
			else if (i == 2 && *path == '.' && *(path + 1) == '.') {
				*(cp++) = '.';
				*(cp++) = '.';
			}
			else {
				len = transname(cp, path, i);
				cp += len;
			}
		}
		else if (!i || (i == 1 && *path == '.'));
		else if (i == 2 && *path == '.' && *(path + 1) == '.') {
			for (cp--; cp > buf; cp--) if (*(cp - 1) == '/') break;
			if (cp < buf) cp = buf;
		}
		else {
			*(cp++) = '/';
			len = transname(cp, path, i);
			cp += len;
		}
		if (*(path += i)) path++;
	}
	if (cp == buf) *(cp++) = '/';
	else if (cp - 1 > buf && *(cp - 1) == '/') cp--;
	*cp = '\0';

	return(drive);
}

DIR *dosopendir(path)
char *path;
{
	dosDIR *xdirp;
	struct dirent *dp;
	dent_t *dentp;
	cache_t *cache;
	char *cp, *cachepath, tmp[13], buf[MAXPATHLEN + 1];
	int len, dd, drive;

	if ((drive = parsepath(buf, path, 0)) < 0
	|| (dd = opendev(drive)) < 0) return(NULL);

	path = buf + 1;
	if (!devlist[dd].dircache) cp = NULL;
	else {
		cache = devlist[dd].dircache;
		cp = cache -> path;
	}
	if (!(devlist[dd].dircache = (cache_t *)malloc(sizeof(cache_t)))) {
		closedev(dd);
		return(NULL);
	}
	devlist[dd].flags |= F_CACHE;
	cachepath = dd2path(dd);

	if (!(xdirp = (dosDIR *)malloc(sizeof(dosDIR)))) {
		closedev(dd);
		return(NULL);
	}
	xdirp -> dd_id = -1;
	xdirp -> dd_fd = dd;
	xdirp -> dd_off = 0;
	xdirp -> dd_loc = 0;
	xdirp -> dd_size = (devlist[dd].clustsize) * (devlist[dd].sectsize);
	if (!(xdirp -> dd_buf = (char *)malloc(xdirp -> dd_size))) {
		dosclosedir((DIR *)xdirp);
		return(NULL);
	}

	if (cp && (len = strlen(cp)) > 0) {
		if (cp[len - 1] == '/') len--;
		if (!strncmp(cp, path, len)
		&& (!path[len] || path[len] == '/')) {
			xdirp -> dd_off = byte2word(cache -> dent.clust);
			memcpy(devlist[dd].dircache, cache, sizeof(cache_t));
			cachepath += len;
			*(cachepath++) = '/';
			path += len;
			if (*path) path++;
		}
	}
	*cachepath = '\0';

	while (*path) {
		if (!(cp = strchr(path, '/'))) cp = path + strlen(path);
		if (cp > path + 8 + 1 + 3) {
			dosclosedir((DIR *)xdirp);
			errno = ENAMETOOLONG;
			return(NULL);
		}
		strncpy(tmp, path, cp - path);
		tmp[cp - path] = '\0';
		path = (*cp) ? cp + 1 : cp;
		if (!*tmp || !strcmp(tmp, ".")) continue;

		while (dp = dosreaddir((DIR *)xdirp))
			if (!strcmp(tmp, dp -> d_name)) break;
		if (!dp) {
			dosclosedir((DIR *)xdirp);
			if (!errno) errno = ENOENT;
			return(NULL);
		}

		dentp = (dent_t *)&(xdirp -> dd_buf[dp -> d_fileno]);
		xdirp -> dd_off = byte2word(dentp -> clust);
		xdirp -> dd_loc = 0;
		cachepath += strlen(tmp);
		*(cachepath++) = '/';
		*cachepath = '\0';
	}

	if (*(dd2path(dd)) && !(dd2dentp(dd) -> attr & DS_IFDIR)) {
		dosclosedir((DIR *)xdirp);
		errno = ENOTDIR;
		return(NULL);
	}
	return((DIR *)xdirp);
}

int dosclosedir(dirp)
DIR *dirp;
{
	dosDIR *xdirp;
	int ret;

	xdirp = (dosDIR *)dirp;
	if (xdirp -> dd_id != -1) return(0);
	if (xdirp -> dd_buf) free(xdirp -> dd_buf);
	ret = closedev(xdirp -> dd_fd);
	free(xdirp);
	return(ret);
}

static int _dosreaddir(xdirp, dentp)
dosDIR *xdirp;
dent_t *dentp;
{
	long next;

	if (xdirp -> dd_id != -1) {
		errno = EINVAL;
		return(-1);
	}
	if (!xdirp -> dd_loc) {
		if ((next = clustread(&devlist[xdirp -> dd_fd],
		xdirp -> dd_buf, xdirp -> dd_off)) < 0) return(-1);
		dd2clust(xdirp -> dd_fd) = xdirp -> dd_off;
		xdirp -> dd_off = next;
	}

	dd2offset(xdirp -> dd_fd) = xdirp -> dd_loc;
	memcpy(dentp, (dent_t *)&(xdirp -> dd_buf[xdirp -> dd_loc]),
		sizeof(dent_t));
	if (dentp -> name[0]) {
		xdirp -> dd_loc += DOSDIRENT;
		if (xdirp -> dd_loc >= xdirp -> dd_size) xdirp -> dd_loc = 0;
	}
	return(0);
}

struct dirent *dosreaddir(dirp)
DIR *dirp;
{
	dosDIR *xdirp;
#ifdef	NODNAMLEN
	static char d[sizeof(struct dirent) + MAXNAMLEN];
#else
	static struct dirent d;
#endif
	struct dirent *dp;
	char *cp;
	long loc;

	dp = (struct dirent *)(&d);
	xdirp = (dosDIR *)dirp;
	errno = 0;
	for (;;) {
		loc = xdirp -> dd_loc;
		if (_dosreaddir(xdirp, dd2dentp(xdirp -> dd_fd)) < 0)
			return(NULL);
		if (!(dd2dentp(xdirp -> dd_fd) -> name[0])) return(NULL);
		else if (dd2dentp(xdirp -> dd_fd) -> name[0] != 0xe5) break;
	}
	dp -> d_fileno = loc;
	dp -> d_reclen = DOSDIRENT;
	getdosname(dp -> d_name, dd2dentp(xdirp -> dd_fd) -> name,
		dd2dentp(xdirp -> dd_fd) -> ext);
	if (cp = strrchr(dd2path(xdirp -> dd_fd), '/')) cp++;
	else cp = dd2path(xdirp -> dd_fd);
	strcpy(cp, dp -> d_name);

	return(dp);
}

int doschdir(path)
char *path;
{
	DIR *dirp;
	char *tmp, buf[MAXPATHLEN + 1];
	int drive;

	buf[1] = ':';
	if ((drive = parsepath(buf + 2, path, 0)) < 0) return(-1);
	buf[0] = drive;

	if (!(dirp = dosopendir(buf))) {
		if ((path[2] != '/' && path[2] != '\\')
		|| !(tmp = curdir[drive - 'A'])) return(-1);
		curdir[drive - 'A'] = NULL;
		if (!(dirp = dosopendir(buf))) {
			curdir[drive - 'A'] = tmp;
			return(-1);
		}
		free(tmp);
	}
	dosclosedir(dirp);

	if (curdir[drive - 'A']) free(curdir[drive - 'A']);
	curdir[drive - 'A'] = (char *)malloc(strlen(buf + 2) + 1);
	strcpy(curdir[drive - 'A'], buf + 2);
	lastdrive = drive;
	return(0);
}

#ifdef	USEGETWD
char *dosgetwd(pathname)
char *pathname;
{
	int size = MAXPATHLEN;
#else
char *dosgetcwd(pathname, size)
char *pathname;
int size;
{
	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);
#endif
	if (lastdrive < 0 || !curdir[lastdrive - 'A']) {
		errno = EFAULT;
		return(NULL);
	}

	if (strlen(curdir[lastdrive - 'A']) > size) {
		errno = ERANGE;
		return(NULL);
	}
	pathname[0] = lastdrive;
	pathname[1] = ':';
	strcpy(pathname + 2, curdir[lastdrive - 'A']);
	return(pathname);
}

static dosDIR *splitpath(pathp)
char **pathp;
{
	char dir[MAXPATHLEN + 1];
	int i;

	strcpy(dir, *pathp);
	for (i = strlen(dir) - 1; i >= 2; i--)
		if (dir[i] == '/' || dir[i] == '\\') break;
	*pathp += i + 1;
	if (i < 3) i++;
	dir[i] = '\0';

	return((dosDIR *)dosopendir(dir));
}

static int readdent(path, ddp)
char *path;
int *ddp;
{
	dosDIR *xdirp;
	struct dirent *dp;
	char buf[MAXPATHLEN + 1];
	int dd, drive;

	buf[1] = ':';
	if ((drive = parsepath(buf + 2, path, 1)) < 0
	|| (dd = opendev(drive)) < 0) return(NULL);
	if (devlist[dd].dircache && !strcmp(dd2path(dd), buf + 2))
		return(dd);
	buf[0] = drive;

	path = buf;
	if (ddp) *ddp = -1;
	if (!(xdirp = splitpath(&path))) {
		closedev(dd);
		if (errno == ENOENT) errno = ENOTDIR;
		return(-1);
	}
	if (!(devlist[dd].dircache = (cache_t *)malloc(sizeof(cache_t)))) {
		dosclosedir((DIR *)xdirp);
		closedev(dd);
		return(-1);
	}
	devlist[dd].flags |= F_CACHE;

	memcpy(devlist[dd].dircache, devlist[xdirp -> dd_fd].dircache,
		sizeof(cache_t));
	if (!*path) {
		dosclosedir((DIR *)xdirp);
		dd2dentp(dd) -> attr |= DS_IFDIR;
		return(dd);
	}
	while (dp = dosreaddir((DIR *)xdirp))
		if (!strcmp(path, dp -> d_name)) break;
	if (!dp) {
		dosclosedir((DIR *)xdirp);
		if (ddp && !errno) *ddp = dd;
		else closedev(dd);
		if (!errno) errno = ENOENT;
		return(-1);
	}

	memcpy(devlist[dd].dircache, devlist[xdirp -> dd_fd].dircache,
		sizeof(cache_t));
	dosclosedir((DIR *)xdirp);
	return(dd);
}

static int writedent(dd)
int dd;
{
	char *buf;
	long sect, offset;
	int ret;

	if ((sect = clust2sect(&devlist[dd], dd2clust(dd))) < 0) {
		errno = EIO;
		return(-1);
	}
	offset = dd2offset(dd);
	while (offset >= devlist[dd].sectsize) {
		sect++;
		offset -= devlist[dd].sectsize;
	}
	if (!(buf = (char *)malloc(devlist[dd].sectsize))) return(-1);
	if ((ret = sectread(&devlist[dd], sect, buf, 1)) >= 0) {
		memcpy(buf + offset, dd2dentp(dd), sizeof(dent_t));
		ret = sectwrite(&devlist[dd], sect, buf, 1);
	}
	free(buf);
	return(ret);
}

static int creatdent(path, mode)
char *path;
int mode;
{
	dosDIR *xdirp;
	char *file, tmp[13];
	long prev;
	int ret;

	file = path;
	if (!(xdirp = splitpath(&file))) {
		if (errno == ENOENT) errno = ENOTDIR;
		return(-1);
	}
	if (!*file) {
		dosclosedir((DIR *)xdirp);
		errno = EEXIST;
		return(-1);
	}
	for (;;) {
		if (_dosreaddir(xdirp, dd2dentp(xdirp -> dd_fd)) < 0) {
			if (errno) {
				dosclosedir((DIR *)xdirp);
				return(-1);
			}
			prev = dd2clust(xdirp -> dd_fd);
			if (!prev || prev > 0xffff) {
				dosclosedir((DIR *)xdirp);
				errno = ENOSPC;
				return(-1);
			}
			dd2clust(xdirp -> dd_fd) =
				clustexpand(&devlist[xdirp -> dd_fd], prev, 1);
			if (dd2clust(xdirp -> dd_fd) < 0) {
				dosclosedir((DIR *)xdirp);
				return(-1);
			}
			memset(dd2dentp(xdirp -> dd_fd), 0, sizeof(dent_t));
			dd2offset(xdirp -> dd_fd) = 0;
			break;
		}
		if (!(dd2dentp(xdirp -> dd_fd) -> name[0])
		|| dd2dentp(xdirp -> dd_fd) -> name[0] == 0xe5) break;

		getdosname(tmp, dd2dentp(xdirp -> dd_fd) -> name,
			dd2dentp(xdirp -> dd_fd) -> ext);
		if (!strcmp(file, tmp)) {
			dosclosedir((DIR *)xdirp);
			errno = EEXIST;
			return(-1);
		}
	}

	memset(dd2dentp(xdirp -> dd_fd), 0, sizeof(dent_t));
	putdosname(dd2dentp(xdirp -> dd_fd) -> name, file);
	dd2dentp(xdirp -> dd_fd) -> attr = putdosmode(mode);
	putdostime(dd2dentp(xdirp -> dd_fd) -> time, -1);
	ret = (writedent(xdirp -> dd_fd) < 0
	|| writefat(&devlist[xdirp -> dd_fd]) < 0) ? -1 : xdirp -> dd_fd;
	free(xdirp -> dd_buf);
	free(xdirp);
	return(ret);
}

int dosstat(path, buf)
char *path;
struct stat *buf;
{
	char *cp, ext[4];
	int dd;

	if ((dd = readdent(path, NULL)) < 0) return(-1);
	buf -> st_dev = dd;
	buf -> st_ino = byte2word(dd2dentp(dd) -> clust);
	buf -> st_mode = getdosmode(dd2dentp(dd) -> attr);
	buf -> st_nlink = 1;
	buf -> st_uid =
	buf -> st_gid = -1;
	buf -> st_size = byte2word(dd2dentp(dd) -> size_l)
		+ (byte2word(dd2dentp(dd) -> size_h) << 16);
	buf -> st_atime =
	buf -> st_mtime =
	buf -> st_ctime = getdostime(byte2word(dd2dentp(dd) -> date),
		byte2word(dd2dentp(dd) -> time));
	closedev(dd);

	if ((buf -> st_mode & S_IFMT) == S_IFDIR) buf -> st_mode |= S_IEXEC_ALL;
	else if ((cp = strrchr(path, '.')) && strlen(++cp) == 3) {
		strcpy(ext, cp);
		for (cp = ext; *cp; cp++) if (islower(*cp)) (*cp) += 'A' - 'a';
		if (!strcmp(ext, "BAT") || !strcmp(ext, "COM")
		|| !strcmp(ext, "EXE")) buf -> st_mode |= S_IEXEC_ALL;
	}
	return(0);
}

int doslstat(path, buf)
char *path;
struct stat *buf;
{
	return(dosstat(path, buf));
}

int dosaccess(path, mode)
char *path;
int mode;
{
	struct stat status;

	if (dosstat(path, &status) < 0) return(-1);

	errno = EACCES;
	if (((mode & R_OK) && !(status.st_mode & S_IREAD))
	|| ((mode & W_OK)
		&& (!(status.st_mode & S_IWRITE)
		|| (devlist[status.st_dev].flags & F_RONLY)))
	|| ((mode & X_OK) && !(status.st_mode & S_IEXEC))) return(-1);
	return(0);
}

/*ARGSUSED*/
int dossymlink(name1, name2)
char *name1, *name2;
{
	errno = EINVAL;
	return(-1);
}

/*ARGSUSED*/
int dosreadlink(path, buf, bufsiz)
char *path, *buf;
int bufsiz;
{
	errno = EINVAL;
	return(-1);
}

int doschmod(path, mode)
char *path;
int mode;
{
	int dd, ret;

	if ((dd = readdent(path, NULL)) < 0) return(-1);
	dd2dentp(dd) -> attr = putdosmode(mode);
	ret = (writedent(dd) < 0);
	closedev(dd);
	return(ret);
}

#ifdef	USEUTIME
int dosutime(path, times)
char *path;
struct utimbuf *times;
{
	time_t clock = times -> modtime;
#else
int dosutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
	time_t clock = tvp[1].tv_sec;
#endif
	int dd, ret;

	if ((dd = readdent(path, NULL)) < 0) return(-1);
	putdostime(dd2dentp(dd) -> time, clock);
	ret = (writedent(dd) < 0);
	closedev(dd);
	return(ret);
}

int dosunlink(path)
char *path;
{
	int dd, ret;

	if ((dd = readdent(path, NULL)) < 0) return(-1);
	if (dd2dentp(dd) -> attr & DS_IFDIR) {
		errno = EPERM;
		return(-1);
	}
	*(dd2dentp(dd) -> name) = 0xe5;
	ret = (writedent(dd) < 0
	|| clustfree(&devlist[dd], byte2word(dd2dentp(dd) -> clust)) < 0
	|| writefat(&devlist[dd]) < 0) ? -1 : 0;
	closedev(dd);
	return(ret);
}

int dosrename(from, to)
char *from, *to;
{
	char buf[MAXPATHLEN + 1];
	int i, dd, fd, ret;

	if ((i = parsepath(buf, to, 0)) < 0) return(-1);
	if (i != getdrive(from)) {
		errno = EXDEV;
		return(-1);
	}
	if ((dd = readdent(from, NULL)) < 0) return(-1);
	
	i = strlen(dd2path(dd));
	if (!strncmp(dd2path(dd), buf + 1, i)
	&& (!buf[i + 1] || buf[i + 1] == '/')) {
		closedev(dd);
		errno = EINVAL;
		return(-1);
	}
	if ((fd = dosopen(to, O_RDWR | O_CREAT | O_EXCL, 0666)) < 0) {
		closedev(dd);
		return (-1);
	}
	memcpy(&(fd2dentp(fd) -> attr) , &(dd2dentp(dd) -> attr),
		sizeof(dent_t) - (8 + 3));
	*(dd2dentp(dd) -> name) = 0xe5;

	free(dosflist[fd]._base);
	dosflist[fd]._base = NULL;
	while (maxdosf > 0 && !dosflist[maxdosf - 1]._base) maxdosf--;
	fd2clust(fd) = dosflist[fd]._clust;
	fd2offset(fd) = dosflist[fd]._offset;

	ret = (writedent(dosflist[fd]._file) < 0 || writedent(dd) < 0
	|| writefat(&devlist[dd]) < 0) ? -1 : 0;
	closedev(dosflist[fd]._file);
	closedev(dd);
	return(ret);
}

int dosopen(path, flags, mode)
char *path;
int flags, mode;
{
	int fd, dd, dummy;

	for (fd = 0; fd < maxdosf; fd++) if (!dosflist[fd]._base) break;
	if (fd >= DOSNOFILE) {
		errno = EMFILE;
		return(-1);
	}

	if ((dd = readdent(path, &dummy)) < 0) {
		if (errno != ENOENT || dummy < 0 || !(flags & O_CREAT)
		|| (dd = creatdent(path, mode)) < 0) {
			if (dummy >= 0) closedev(dummy);
			return(-1);
		}
		else {
			if (!(devlist[dummy].flags & F_DUPL))
				devlist[dd].flags &= ~F_DUPL;
			devlist[dummy].flags |= F_DUPL;
			closedev(dummy);
		}
	}
	else if ((flags & O_CREAT) && (flags & O_EXCL)) {
		closedev(dd);
		errno = EEXIST;
		return(-1);
	}

	if (flags & (O_WRONLY | O_RDWR)) {
		errno = 0;
		if (devlist[dd].flags & F_RONLY) errno = EROFS;
		else if (dd2dentp(dd) -> attr & DS_IFDIR) errno = EISDIR;
		if (errno) {
			closedev(dd);
			return(-1);
		}
	}
	if (flags & O_TRUNC) {
		if ((flags & (O_WRONLY | O_RDWR))
		&& clustfree(&devlist[dd],
		byte2word(dd2dentp(dd) -> clust)) < 0) {
			closedev(dd);
			return(-1);
		}
		dd2dentp(dd) -> size_l[0] =
		dd2dentp(dd) -> size_l[1] =
		dd2dentp(dd) -> size_h[0] =
		dd2dentp(dd) -> size_h[1] = 0;
		dd2dentp(dd) -> clust[0] =
		dd2dentp(dd) -> clust[1] = 0;
	}

	dosflist[fd]._base =
		(char *)malloc(devlist[dd].clustsize * devlist[dd].sectsize);
	if (!dosflist[fd]._base) {
		closedev(dd);
		return(-1);
	}

	dosflist[fd]._cnt = 0;
	dosflist[fd]._ptr = dosflist[fd]._base;
	dosflist[fd]._bufsize =
		devlist[dd].clustsize * devlist[dd].sectsize;
	dosflist[fd]._flag = flags;
	dosflist[fd]._file = dd;
	dosflist[fd]._top =
	dosflist[fd]._next = byte2word(dd2dentp(dd) -> clust);
	dosflist[fd]._off = 0;
	dosflist[fd]._loc = 0;
	dosflist[fd]._size = byte2word(dd2dentp(dd) -> size_l)
		+ (byte2word(dd2dentp(dd) -> size_h) << 16);

	memcpy(&(dosflist[fd]._dent), dd2dentp(dd), sizeof(dent_t));
	dosflist[fd]._clust = dd2clust(dd);
	dosflist[fd]._offset = dd2offset(dd);

	if ((flags & (O_WRONLY | O_RDWR)) && (flags & O_APPEND))
		doslseek(fd, 0L, L_XTND);

	if (fd >= maxdosf) maxdosf = fd + 1;
	return(fd + DOSFDOFFSET);
}

int dosclose(fd)
int fd;
{
	int ret;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf || !dosflist[fd]._base) {
		errno = EBADF;
		return(-1);
	}
	free(dosflist[fd]._base);
	dosflist[fd]._base = NULL;
	while (maxdosf > 0 && !dosflist[maxdosf - 1]._base) maxdosf--;

	ret = 0;
	if (dosflist[fd]._flag & (O_WRONLY | O_RDWR)) {
		dosflist[fd]._dent.size_l[0] =
			dosflist[fd]._size & 0xff;
		dosflist[fd]._dent.size_l[1] =
			(dosflist[fd]._size >> 8) & 0xff;
		dosflist[fd]._dent.size_h[0] =
			(dosflist[fd]._size >> 16) & 0xff;
		dosflist[fd]._dent.size_h[1] =
			(dosflist[fd]._size >> 24) & 0xff;
		
		if (!(dosflist[fd]._dent.attr & DS_IFDIR))
			putdostime(dosflist[fd]._dent.time, -1);
		memcpy(fd2dentp(fd), &(dosflist[fd]._dent), sizeof(dent_t));
		fd2clust(fd) = dosflist[fd]._clust;
		fd2offset(fd) = dosflist[fd]._offset;
		if (writedent(dosflist[fd]._file) < 0
		|| writefat(fd2devp(fd)) < 0) ret = -1;
	}

	closedev(dosflist[fd]._file);
	return(ret);
}

static int dosfilbuf(fd, wrt)
int fd, wrt;
{
	int size, new, prev;

	size = (wrt) ? dosflist[fd]._bufsize :
		dosflist[fd]._size - dosflist[fd]._loc;
	errno = 0;
	prev = dosflist[fd]._off;
	if (!(dosflist[fd]._off = dosflist[fd]._next))
		dosflist[fd]._next = -1;
	else dosflist[fd]._next = clustread(fd2devp(fd),
		dosflist[fd]._base, dosflist[fd]._next);
	if (dosflist[fd]._next < 0) {
		if (errno) return(-1);
		if (!wrt) return(0);
		if ((new = clustexpand(fd2devp(fd), prev, 0)) < 0) return(-1);
		if (!dosflist[fd]._off) {
			dosflist[fd]._dent.clust[0] = new & 0xff;
			dosflist[fd]._dent.clust[1] = (new >> 8) & 0xff;
		}
		dosflist[fd]._off = new;
		dosflist[fd]._next = 0xffff;
		memset(dosflist[fd]._base, 0, dosflist[fd]._bufsize);
	}
	if (size > dosflist[fd]._bufsize) size = dosflist[fd]._bufsize;
	dosflist[fd]._cnt = size;
	dosflist[fd]._ptr = dosflist[fd]._base;

	return(size);
}

static int dosflsbuf(fd)
int fd;
{
	int sect;

	if ((sect = clust2sect(fd2devp(fd), dosflist[fd]._off)) < 0) {
		errno = EIO;
		return(-1);
	}
	if (sectwrite(fd2devp(fd), sect, dosflist[fd]._base,
	fd2devp(fd) -> clustsize) < 0) return(-1);

	return(dosflist[fd]._bufsize);
}

int dosread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	long size, total;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf
	|| (buf && (dosflist[fd]._flag & O_WRONLY))) {
		errno = EBADF;
		return(-1);
	}

	total = 0;
	while (nbytes > 0 && dosflist[fd]._loc < dosflist[fd]._size) {
		if (dosflist[fd]._cnt <= 0 && dosfilbuf(fd, 0) < 0) return(-1);
		if (!(size = dosflist[fd]._cnt)) return(total);

		if (size > nbytes) size = nbytes;
		if (buf) {
			memcpy(buf, dosflist[fd]._ptr, size);
			buf += size;
		}
		dosflist[fd]._ptr += size;
		dosflist[fd]._cnt -= size;
		dosflist[fd]._loc += size;
		nbytes -= size;
		total += size;
	}

	return(total);
}

int doswrite(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	long size, total;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf
	|| !(dosflist[fd]._flag & (O_WRONLY | O_RDWR))) {
		errno = EBADF;
		return(-1);
	}

	total = 0;
	while (nbytes > 0) {
		if (dosflist[fd]._cnt <= 0 && dosfilbuf(fd, 1) < 0) return(-1);
		size = dosflist[fd]._cnt;

		if (size > nbytes) size = nbytes;
		memcpy(dosflist[fd]._ptr, buf, size);
		buf += size;
		dosflist[fd]._ptr += size;
		if ((dosflist[fd]._cnt -= size) < 0) dosflist[fd]._cnt = 0;
		if ((dosflist[fd]._loc += size) > dosflist[fd]._size)
			dosflist[fd]._size = dosflist[fd]._loc;
		nbytes -= size;
		total += size;

		if (dosflsbuf(fd) < 0) return(-1);
	}

	return(total);
}

int doslseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf) {
		errno = EBADF;
		return(-1);
	}
	switch (whence) {
		case L_SET:
			break;
		case L_INCR:
			offset += dosflist[fd]._loc;
			break;
		case L_XTND:
			offset += dosflist[fd]._size;
			break;
		default:
			errno = EINVAL;
			return(-1);
/*NOTREACHED*/
			break;
	}

	if (offset >= dosflist[fd]._loc) offset -= dosflist[fd]._loc;
	else {
		dosflist[fd]._cnt = 0;
		dosflist[fd]._ptr = dosflist[fd]._base;
		dosflist[fd]._loc = 0;
		dosflist[fd]._off = 0;
		dosflist[fd]._next = dosflist[fd]._top;
	}

	if (dosread(fd, NULL, offset) < 0) return(-1);
	return(dosflist[fd]._loc);
}

int dosmkdir(path, mode)
char *path;
int mode;
{
	dent_t dent[2];
	long clust;
	int fd, tmp;

	if ((fd = dosopen(path, O_RDWR | O_CREAT | O_EXCL, mode)) < 0)
		return(-1);

	fd -= DOSFDOFFSET;
	dosflist[fd]._dent.attr |= DS_IFDIR;
	memcpy(&dent[0], &(dosflist[fd]._dent), sizeof(dent_t));
	memset(dent[0].name, ' ', 8 + 3);
	dent[0].name[0] = '.';
	if ((clust = newclust(fd2devp(fd))) < 0) {
		dosflist[fd]._dent.name[0] = 0xe5;
		dosclose(fd + DOSFDOFFSET);
		return(-1);
	}
	dent[0].clust[0] = clust & 0xff;
	dent[0].clust[1] = (clust << 8) & 0xff;

	memcpy(&dent[1], &(dosflist[fd]._dent), sizeof(dent_t));
	memset(dent[1].name, ' ', 8 + 3);
	dent[1].name[0] =
	dent[1].name[1] = '.';
	clust = fd2clust(fd);
	dent[1].clust[0] = clust & 0xff;
	dent[1].clust[1] = (clust << 8) & 0xff;

	if (doswrite(fd + DOSFDOFFSET, &dent[0], sizeof(dent_t) * 2) < 0) {
		tmp = errno;
		if (clust = byte2word(dosflist[fd]._dent.clust))
			clustfree(fd2devp(fd), clust);
		dosflist[fd]._dent.name[0] = 0xe5;
		dosclose(fd + DOSFDOFFSET);
		errno = tmp;
		return(-1);
	}

	dosflist[fd]._size = 0;
	dosclose(fd + DOSFDOFFSET);
	return(0);
}

int dosrmdir(path)
char *path;
{
	dosDIR *xdirp;
	struct dirent *dp;
	cache_t cache;
	int ret;

	if (!(xdirp = (dosDIR *)dosopendir(path))) return(-1);
	memcpy(&cache, devlist[xdirp -> dd_fd].dircache, sizeof(cache_t));

	while (dp = dosreaddir((DIR *)xdirp))
		if (strcmp(dp -> d_name, ".") && strcmp(dp -> d_name, "..")) {
			dosclosedir((DIR *)xdirp);
			errno = ENOTEMPTY;
			return(-1);
		}
	if (errno) ret = -1;
	else {
		memcpy(devlist[xdirp -> dd_fd].dircache, &cache,
			sizeof(cache_t));
		*(dd2dentp(xdirp -> dd_fd) -> name) = 0xe5;
		ret = (writedent(xdirp -> dd_fd) < 0
		|| clustfree(&devlist[xdirp -> dd_fd],
			byte2word(dd2dentp(xdirp -> dd_fd) -> clust)) < 0
		|| writefat(&devlist[xdirp -> dd_fd]) < 0) ? -1 : 0;
	}
	dosclosedir((DIR *)xdirp);
	return(ret);
}

int stream2fd(stream)
FILE *stream;
{
	int fd;

	for (fd = 0; fd < DOSNOFILE; fd++)
		if (stream == (FILE *)(&dosflist[fd])) break;
	if (fd >= DOSNOFILE) return(-1);
	return(fd);
}

FILE *dosfopen(path, type)
char *path, *type;
{
	int fd, flags;

	errno = 0;
	if (!type) return(NULL);
	switch(*type) {
		case 'r':
			if (*(type + 1) != '+') flags = O_RDONLY;
			else flags = O_RDWR;
			break;
		case 'w':
			if (*(type + 1) != '+') flags = O_WRONLY;
			else flags = O_RDWR | O_TRUNC;
			flags |= O_CREAT;
			break;
		case 'a':
			if (*(type + 1) != '+') flags = O_WRONLY;
			else flags = O_RDWR;
			flags |= O_APPEND | O_CREAT;
			break;
		default:
			flags = 0;
			break;
	}

	if ((fd = dosopen(path, flags, 0666)) < 0) return(NULL);
	return((FILE *)(&dosflist[fd - DOSFDOFFSET]));
}

int dosfclose(stream)
FILE *stream;
{
	int fd;

	errno = 0;
	if ((fd = stream2fd(stream)) < 0) return(EOF);
	return((dosclose(fd + DOSFDOFFSET) < 0) ? EOF : 0);
}

int dosfread(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	int fd, ret, rest;

	if ((fd = stream2fd(stream)) < 0) {
		errno = EINVAL;
		return(0);
	}
	if (dosflist[fd]._flag & O_WRONLY) {
		errno = EBADF;
		dosflist[fd]._flag |= O_IOERR;
		return(0);
	}

	rest = dosflist[fd]._bufsize - (dosflist[fd]._ptr - dosflist[fd]._base);
	if (rest < dosflist[fd]._cnt) {
		if (dosflsbuf(fd) < 0) {
			dosflist[fd]._flag |= O_IOERR;
			return(0);
		}
		dosflist[fd]._cnt = rest;
	}
	ret = dosread(fd + DOSFDOFFSET, buf, size * nitems);

	if (ret < 0) {
		dosflist[fd]._flag |= O_IOERR;
		return(0);
	}
	if (ret < size * nitems) dosflist[fd]._flag |= O_IOEOF;
	return(ret);
}

int dosfwrite(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	int fd, rest, nbytes, total;

	if ((fd = stream2fd(stream)) < 0) {
		errno = EINVAL;
		return(0);
	}
	if (!(dosflist[fd]._flag & (O_WRONLY | O_RDWR))) {
		errno = EBADF;
		dosflist[fd]._flag |= O_IOERR;
		return(0);
	}
	errno = 0;
	total = 0;
	nbytes = size * nitems;

	rest = dosflist[fd]._bufsize - (dosflist[fd]._ptr - dosflist[fd]._base);
	while (nbytes >= rest) {
		memcpy(dosflist[fd]._ptr, buf, rest);
		buf += rest;
		dosflist[fd]._cnt = 0;
		dosflist[fd]._ptr = dosflist[fd]._base;
		if ((dosflist[fd]._loc += size) > dosflist[fd]._size)
			dosflist[fd]._size = dosflist[fd]._loc;
		nbytes -= rest;
		total += rest;
		if (dosflsbuf(fd) < 0) {
			dosflist[fd]._flag |= O_IOERR;
			return(0);
		}
		rest = dosflist[fd]._bufsize;
	}
	if (nbytes > 0) {
		if (rest >= dosflist[fd]._bufsize && dosfilbuf(fd, 1) < 0) {
			dosflist[fd]._flag |= O_IOERR;
			return(0);
		}
		memcpy(dosflist[fd]._ptr, buf, nbytes);
		dosflist[fd]._ptr += nbytes;
		total += nbytes;
	}
	return(total);
}

int dosfflush(stream)
FILE *stream;
{
	int fd, rest;

	if ((fd = stream2fd(stream)) < 0) {
		errno = EINVAL;
		return(EOF);
	}
	rest = dosflist[fd]._bufsize - (dosflist[fd]._ptr - dosflist[fd]._base);
	if (rest == dosflist[fd]._cnt) return(0);
	if (dosflsbuf(fd) < 0) {
		dosflist[fd]._flag |= O_IOERR;
		return(EOF);
	}
	dosflist[fd]._cnt = rest;
	return(0);
}

int dosfgetc(stream)
FILE *stream;
{
	u_char ch;

	return(dosfread(&ch, sizeof(u_char), 1, stream) ? (int)ch : EOF);
}

int dosfputc(c, stream)
int c;
FILE *stream;
{
	u_char ch;

	ch = (u_char)c;
	return(dosfwrite(&ch, sizeof(u_char), 1, stream) ? (int)ch : EOF);
}

char *dosfgets(s, n, stream)
char *s;
int n;
FILE *stream;
{
	int i, c;

	for (i = 0; i < n - 1; i++) {
		if ((c = dosfgetc(stream)) == EOF) return(NULL);
		if (c == '\r') continue;
		s[i] = c;
		if (s[i] == '\n') {
			i++;
			break;
		}
	}
	s[i] = '\0';
	return(s);
}

int dosfputs(s, stream)
char *s;
FILE *stream;
{
	int i;

	for (i = 0; s[i]; i++) {
		if (s[i] == '\n' && dosfputc('\r', stream) == EOF) return(EOF);
		if (dosfputc(s[i], stream) == EOF) return(EOF);
	}
	return(0);
}

int dosallclose()
{
	int i, ret;

	ret = 0;
	for (i = maxdosf - 1; i >= 0; i--)
		if (dosflist[i]._base && dosclose(i + DOSFDOFFSET) < 0)
			ret = -1;
	for (i = maxdev - 1; i >= 0; i--)
		if (devlist[i].drive && closedev(i) < 0) ret = -1;
	return(ret);
}
