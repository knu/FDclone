/*
 *	dosdisk.c
 *
 *	MSDOS Disk Access Module
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include "machine.h"

#ifndef	_NODOSDRIVE

#ifndef	NOUNISTDH
#include <unistd.h>
#endif

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#if	MSDOS
#include "unixemu.h"
#else
#include <sys/file.h>
#include <sys/time.h>
# ifdef	USEUTIME
# include <utime.h>
# endif
# ifdef	USEDIRECT
# include <sys/dir.h>
# define	dirent	direct
# else
# include <dirent.h>
# endif
#endif

#include "dosdisk.h"

#ifdef	NOERRNO
extern int errno;
#endif

#if	!defined (ENOTEMPTY) && defined (ENFSNOTEMPTY)
#define	ENOTEMPTY	ENFSNOTEMPTY
#endif

#ifndef issjis1
#define issjis1(c)	((0x81 <= (c) && (c) <= 0x9f)\
			|| (0xe0 <= (c) && (c) <= 0xfc))
#endif

#define	reterr(c)	{errno = doserrno; return(c);}
#define	S_IEXEC_ALL	(S_IEXEC | (S_IEXEC >> 3) | (S_IEXEC >> 6))

#ifdef	FD
extern int toupper2();
extern int strcasecmp2();
extern time_t timelocal2();
#else
#ifndef	NOTZFILEH
#include <tzfile.h>
#endif
#ifndef	CHAR_BIT
# ifdef	NBBY
# define	CHAR_BIT	NBBY
# else
# define	CHAR_BIT	0x8
# endif
#endif
#define	char2long(cp)	(  ((u_char *)cp)[3] \
			| (((u_char *)cp)[2] << (CHAR_BIT * 1)) \
			| (((u_char *)cp)[1] << (CHAR_BIT * 2)) \
			| (((u_char *)cp)[0] << (CHAR_BIT * 3)) )
static int toupper2();
static int strcasecmp2();
static int tmcmp();
static long gettimezone();
static time_t timelocal2();
#endif

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
static int checksum();
static u_short lfnencode();
static u_short lfndecode();
static int transchar();
static int detranschar();
static int transname();
static int cmpdospath();
static char *getdosname();
static char *putdosname();
static u_short getdosmode();
static u_char putdosmode();
static time_t getdostime();
static u_char *putdostime();
static int getdrive();
static int parsepath();
static int seekdent();
static int readdent();
static struct dirent *_dosreaddir();
static struct dirent *finddir();
static dosDIR *splitpath();
static int getdent();
static int writedent();
static int expanddent();
static int creatdent();
static int unlinklfn();
static int dosfilbuf();
static int dosflsbuf();

devinfo fdtype[MAXDRIVEENTRY] = {
#if	defined (SOLARIS)
# if	defined (i386)
	{'A', "/dev/rfd0a", 2, 18, 80},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0c", 2, 8 + 100, 80},
# endif
#endif
#if	defined (SUN_OS)
# if	defined (sparc)
	{'A', "/dev/rfd0c", 2, 18, 80},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0c", 2, 8 + 100, 80},
# endif
#endif
#if	defined (NEWS_OS3) || defined (NEWS_OS4)
# if	defined (news800) || defined (news900) \
|| defined (news1800) || defined (news1900) || defined (news3800)
	{'A', "/dev/rfh0a", 2, 18, 80},
	{'A', "/dev/rfd0a", 2, 9, 80},
	{'A', "/dev/rfd0a", 2, 8 + 100, 80},
# else
#  if	defined (news3100) || defined (news5000)
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
#if	defined (NEWS_OS6)
	{'A', "/dev/rfloppy/c0d0u0p0", 2, 18, 80},
	{'A', "/dev/rfloppy/c0d0u1p0", 2, 9, 80},
	{'A', "/dev/rfloppy/c0d0u3p0", 2, 8, 80},
#endif
#if	defined (HPUX)
# if	defined (__hp9000s700)
	{'A', "/dev/rfloppy/c201d0s0", 2, 18, 80},
	{'A', "/dev/rfloppy/c201d0s0", 2, 9, 80},
	{'A', "/dev/rfloppy/c201d0s0", 2, 8 + 100, 80},
# endif
#endif
#if	defined (EWSUXV)
	{'A', "/dev/if/f0h18", 2, 18, 80},
	{'A', "/dev/if/f0h8", 2, 8, 77},
	{'A', "/dev/if/f0c15", 2, 15, 80},
	{'A', "/dev/if/f0d9", 2, 9, 80},
	{'A', "/dev/if/f0d8", 2, 8, 80},
#endif
#if	defined (AIX)
	{'A', "/dev/rfd0h", 2, 18, 80},
	{'A', "/dev/rfd0l", 2, 9, 80},
	{'A', "/dev/rfd0l", 2, 8 + 100, 80},
#endif
#if	defined (ULTRIX)
	{'A', "/dev/rfh0a", 2, 18, 80},
	{'A', "/dev/rfd0a", 2, 9, 80},
	{'A', "/dev/rfd0a", 2, 8 + 100, 80},
#endif
#if	defined (LINUX)
	{'A', "/dev/fd0", 2, 18, 80},
	{'A', "/dev/fd0", 2, 9, 80},
	{'A', "/dev/fd0", 2, 8 + 100, 80},
#endif
#if	defined (JCCBSD)
	{'A', "/dev/rfd0a", 2, 18, 80},
	{'A', "/dev/rfd0b", 2, 8, 77},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0d", 2, 8, 80},
#endif
#if	defined (FREEBSD)
	{'A', "/dev/rfd0.1440", 2, 18, 80},
	{'A', "/dev/rfd0.720", 2, 9, 80},
	{'A', "/dev/rfd0.720", 2, 8 + 100, 80},
#endif
#if	defined (NETBSD)
# if	defined (i386)
	{'A', "/dev/rfd0a", 2, 18, 80},
	{'A', "/dev/rfd0a", 2, 15, 80},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0c", 2, 8 + 100, 80},
# endif
#endif
#if	defined (BSDOS)
# if	defined (i386)
	{'A', "/dev/rfd0c", 2, 18, 80},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0c", 2, 8 + 100, 80},
# endif
#endif
#if	defined (ORG_386BSD)
# if	defined (i386)
	{'A', "/dev/fd0a", 2, 18, 80},
	{'A', "/dev/fd0a", 2, 15, 80},
	{'A', "/dev/fd0b", 2, 8, 77},
	{'A', "/dev/fd0c", 2, 9, 80},
	{'A', "/dev/fd0c", 2, 8 + 100, 80},
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
static long lfn_clust = -1;
static u_short lfn_offset;
static int doserrno = 0;


#ifndef	FD
static int toupper2(c)
int c;
{
	return((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
}

static int strcasecmp2(s1, s2)
char *s1, *s2;
{
	int c1, c2;

	while ((c1 = toupper2(*s1)) == (c2 = toupper2(*(s2++))))
		if (!*(s1++)) return(0);
	return(c1 - c2);
}

static int tmcmp(tm1, tm2)
struct tm *tm1, *tm2;
{
	if (tm1 -> tm_year != tm2 -> tm_year)
		return (tm1 -> tm_year - tm2 -> tm_year);
	if (tm1 -> tm_mon != tm2 -> tm_mon)
		return (tm1 -> tm_mon - tm2 -> tm_mon);
	if (tm1 -> tm_mday != tm2 -> tm_mday)
		return (tm1 -> tm_mday - tm2 -> tm_mday);
	if (tm1 -> tm_hour != tm2 -> tm_hour)
		return (tm1 -> tm_hour - tm2 -> tm_hour);
	if (tm1 -> tm_min != tm2 -> tm_min)
		return (tm1 -> tm_min - tm2 -> tm_min);
	return (tm1 -> tm_sec - tm2 -> tm_sec);
}

static long gettimezone(tm, t)
struct tm *tm;
time_t t;
{
#ifdef	NOTMGMTOFF
	struct timeval t_val;
	struct timezone t_zone;
#endif
#ifndef	NOTZFILEH
	struct tzhead head;
#endif
	struct tm tmbuf;
	FILE *fp;
	long tz, tmp, leap, nleap, ntime, ntype, nchar;
	char *cp, buf[MAXPATHLEN + 1];
	u_char c;
	int i;

	memcpy(&tmbuf, tm, sizeof(struct tm));

#ifdef	NOTMGMTOFF
	gettimeofday(&t_val, &t_zone);
	tz = t_zone.tz_minuteswest * 60L;
#else
	tz = -(localtime(&t) -> tm_gmtoff);
#endif

#ifndef	NOTZFILEH
	cp = (char *)getenv("TZ");
	if (!cp || !*cp) cp = TZDEFAULT;
	if (cp[0] == '/') strcpy(buf, cp);
	else {
		strcpy(buf, TZDIR);
		strcat(buf, "/");
		strcat(buf, cp);
	}
	if (!(fp = fopen(buf, "r"))) return(tz);
	if (fread(&head, sizeof(struct tzhead), 1, fp) != 1) {
		fclose(fp);
		return(tz);
	}
#ifdef	USELEAPCNT
	nleap = char2long(head.tzh_leapcnt);
#else
	nleap = char2long(head.tzh_timecnt - 4);
#endif
	ntime = char2long(head.tzh_timecnt);
	ntype = char2long(head.tzh_typecnt);
	nchar = char2long(head.tzh_charcnt);

	for (i = 0; i < ntime; i++) {
		if (fread(buf, sizeof(char), 4, fp) != 1) {
			fclose(fp);
			return(tz);
		}
		tmp = char2long(buf);
		if (tmcmp(&tmbuf, localtime(&tmp)) < 0) break;
	}
	if (i > 0) {
		i--;
		i *= sizeof(char);
		i += sizeof(struct tzhead) + ntime * sizeof(char) * 4;
		if (fseek(fp, i, 0) < 0
		|| fread(&c, sizeof(char), 1, fp) != 1) {
			fclose(fp);
			return(tz);
		}
		i = c;
	}
	i *= sizeof(char) * (4 + 1 + 1);
	i += sizeof(struct tzhead) + ntime * sizeof(char) * (4 + 1);
	if (fseek(fp, i, 0) < 0
	|| fread(buf, sizeof(char), 4, fp) != 1) {
		fclose(fp);
		return(tz);
	}
	tmp = char2long(buf);
	tz = -tmp;

	i = sizeof(struct tzhead) + ntime * sizeof(char) * (4 + 1)
		+ ntype * sizeof(char) * (4 + 1 + 1)
		+ nchar * sizeof(char);
	if (fseek(fp, i, 0) < 0) {
		fclose(fp);
		return(tz);
	}
	leap = 0;
	for (i = 0; i < nleap; i++) {
		if (fread(buf, sizeof(char), 4, fp) != 1) {
			fclose(fp);
			return(tz);
		}
		tmp = char2long(buf);
		if (tmcmp(&tmbuf, localtime(&tmp)) <= 0) break;
		if (fread(buf, sizeof(char), 4, fp) != 1) {
			fclose(fp);
			return(tz);
		}
		leap = char2long(buf);
	}

	tz += leap;
	fclose(fp);
#endif	/* !NOTZFILEH */

	return(tz);
}

static time_t timelocal2(tm)
struct tm *tm;
{
#ifdef	USEMKTIME
	tm -> tm_isdst = -1;
	return(mktime(tm));
#else
# ifdef	USETIMELOCAL
	return(timelocal(tm));
# else
	time_t d, t;
	int i, y;

	y = (tm -> tm_year < 1900) ? tm -> tm_year + 1900 : tm -> tm_year;

	d = (y - 1970) * 365;
	d += ((y - 1 - 1968) / 4)
		- ((y - 1 - 1900) / 100)
		+ ((y - 1 - 1600) / 400);
	for (i = 1; i < tm -> tm_mon + 1; i++) {
		switch (i) {
			case 2:
				if (!(y % 4)
				&& ((y % 100)
				|| !(y % 400))) d++;
				d += 28;
				break;
			case 4:
			case 6:
			case 9:
			case 11:
				d += 30;
				break;
			default:
				d += 31;
				break;
		}
	}
	d += tm -> tm_mday - 1;
	t = (tm -> tm_hour * 60 + tm -> tm_min) * 60 + tm -> tm_sec;
	t += d * 60 * 60 * 24;
	t += gettimezone(tm, t);

	return(t);
# endif
#endif
}
#endif	/* !FD */

static int sectseek(devp, sect)
devstat *devp;
long sect;
{
	int tmp;

	tmp = errno;
	if (devp -> flags & F_8SECT) sect += sect / 8;
	if (lseek(devp -> fd, sect * (devp -> sectsize), L_SET) < 0) {
		doserrno = errno;
		errno = tmp;
		return(-1);
	}
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
	int i;

	for (i = 0; i < maxsectcache; i++)
	if (devp -> drive == cachedrive[i] && sect == sectno[i]) {
		memcpy(sectcache[i], buf, devp -> sectsize);
		shiftcache(i);
		return(1);
	}
	if (!(cp = (char *)malloc(devp -> sectsize))) return(-1);
	if (i < SECTCACHESIZE) maxsectcache = i + 1;
	else {
		i--;
		shiftcache(-1);
	}
	cachedrive[i] = devp -> drive;
	sectno[i] = sect;
	sectcache[i] = cp;
	memcpy(sectcache[i], buf, devp -> sectsize);
	return(0);
}

static int loadcache(devp, sect, buf)
devstat *devp;
long sect;
char *buf;
{
	int i;

	for (i = maxsectcache - 1; i >= 0; i--)
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
	int i, tmp;

	tmp = errno;
	for (i = 0; i < n; i++, sect++, buf += devp -> sectsize) {
		if (loadcache(devp, sect, buf) >= 0) continue;
		if (sectseek(devp, sect) < 0) return(-1);
		while (read(devp -> fd, buf, devp -> sectsize) < 0) {
			if (errno == EINTR) errno = tmp;
			else {
				doserrno = errno;
				return(-1);
			}
		}
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
	int i, tmp;

	tmp = errno;
	if (devp -> flags & F_RONLY) {
		doserrno = EROFS;
		return(-1);
	}
	for (i = 0; i < n; i++, sect++, buf += devp -> sectsize) {
		savecache(devp, sect, buf);
		if (sectseek(devp, sect) < 0) return(-1);
		if (write(devp -> fd, buf, devp -> sectsize) < 0) {
			if (errno == EINTR) errno = tmp;
			else {
				doserrno = errno;
				return(-1);
			}
		}
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
		doserrno = EIO;
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

	if (!clust || clust > 0xffff) {
		sect = (clust) ? clust - 0x10000 : devp -> dirofs;
		if (sect >= (devp -> dirofs) + (devp -> dirsize)) {
			doserrno = 0;
			return(-1);
		}
	}
	else {
		if (clust == 0xffff) {
			doserrno = 0;
			return(-1);
		}
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
	if (used < 0 || clust > devp -> totalsize) {
		doserrno = ENOSPC;
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
		doserrno = EIO;
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
	int i, tmp, fd, ch_sect;

	tmp = errno;
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
#ifdef	EFORMAT
			if (errno == EFORMAT) {
				errno = tmp;
				return(0);
			}
#endif
			if (errno == EIO) errno = ENXIO;
			if (errno != EROFS
			|| (fd = open(devp -> ch_name, O_RDONLY, 0600)) < 0) {
				doserrno = errno;
				errno = tmp;
				return(-1);
			}
			devp -> flags |= F_RONLY;
		}
		if (lseek(fd, 0L, L_SET) < 0) {
			close(fd);
			errno = tmp;
			return(0);
		}

		while ((i = read(fd, buf, SECTSIZE)) < 0 && errno == EINTR);
		if (i < 0) {
			close(fd);
			errno = tmp;
			return(0);
		}
		devp -> fd = fd;
	}
	errno = tmp;

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
	devp -> totalsize = total / (devp -> clustsize);
	if (devp -> totalsize > MAX12BIT) devp -> flags |= F_16BIT;

	return(1);
}

static int opendev(drive)
int drive;
{
	bpb_t bpb;
	devstat dev;
	char *cp;
	int i, ret, drv;

	if (!drive) {
		doserrno = ENODEV;
		return(-1);
	}
	if (maxdev >= DOSNOFILE) {
		doserrno = EMFILE;
		return(-1);
	}
	drv = toupper2(drive);

	for (i = maxdev - 1; i >= 0; i--) {
		if (drv == (int)devlist[i].drive) {
			memcpy(&dev, &devlist[i], sizeof(devstat));
			dev.flags |= F_DUPL;
			dev.flags &= ~(F_CACHE | F_WRFAT);
			break;
		}
	}

	if (i < 0) {
		cp = NULL;
		for (i = 0; fdtype[i].name; i++) {
			if (drv != (int)fdtype[i].drive) continue;
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
			doserrno = ENODEV;
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
	if (drive >= 'a' && drive <= 'z') dev.flags |= F_VFAT;
	else dev.flags &= ~F_VFAT;

	memcpy(&devlist[maxdev], &dev, sizeof(devstat));
	return(maxdev++);
}

static int closedev(dd)
int dd;
{
	int i, j, ret;

	if (dd < 0 || dd >= maxdev || !devlist[dd].drive) {
		doserrno = EBADF;
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
		if (toupper2(drive) == (int)devlist[i].drive)
			return(DOSNOFILE + i);
	if (func) (*func)();
	if ((i = opendev(drive)) < 0) reterr(-1);
	return(i);
}

int shutdrv(dd)
int dd;
{
	int i;

	if (dd >= DOSNOFILE) return(0);
	if ((i = closedev(dd)) < 0) reterr(-1);
	return(i);
}

static int checksum(name)
char *name;
{
	int i, sum;

	for (i = sum = 0; i < 8 + 3; i++) {
		sum = (((sum & 0x01) << 7) | ((sum & 0xfe) >> 1));
		sum += name[i];
	}
	return(sum & 0xff);
}

static u_short lfnencode(c1, c2)
u_char c1, c2;
{
	u_short kanji;

	if (!c1) {
		if (strchr(NOTINLFN, c2)) return('_');
		if (c2 < 0x80) return(c2);
		if (c2 > 0xa0 && c2 <= 0xdf) return(0xff00 | (c2 - 0x40));
	}
	else {
		kanji = (c1 << 8) | c2;
		if (kanji >= 0x8260 && kanji <= 0x8279)
			return(0xff00 | (kanji - 0x8260 + 0x21));
		if (kanji >= 0x8281 && kanji <= 0x829a)
			return(0xff00 | (kanji - 0x8281 + 0x41));
	}

	return(0xff00);
}

static u_short lfndecode(c1, c2)
u_char c1, c2;
{
	switch (c1) {
		case 0:
			if (c2 < 0x80) return(c2);
			break;
		case 0xff:
			if (c2 > 0x20 && c2 <= 0x3a) return(c2 + 0x8260 - 0x21);
			if (c2 > 0x40 && c2 <= 0x5a) return(c2 + 0x8281 - 0x41);
			if (c2 > 0x60 && c2 <= 0x9f) return(c2 + 0x40);
			break;
		default:
			break;
	}
	return('?');
}

static int transchar(c)
int c;
{
	if (c == '+') return('`');
	if (c == ',') return('\'');
	if (c == '[') return('&');
	if (c == '.') return('$');
	if (strchr(NOTINLFN, c) || strchr(NOTINALIAS, c)) return('_');
	return(toupper2(c));
}

static int detranschar(c)
int c;
{
	if (c == '`') return('+');
	if (c == '\'') return(',');
	if (c == '&') return('[');
	if (c == '$') return('.');
	return((c >= 'A' && c <= 'Z') ? c + 'a' - 'A' : c);
}

static int transname(buf, path, len)
char *buf, *path;
int len;
{
	char *cp;
	int i, j;

	if (len <= 0) len = strlen(path);
	if (path[0] == '.' && (len == 1 || (len == 2 && path[1] == '.'))) {
		strncpy(buf, path, len);
		buf[len] = '\0';
		return(len);
	}

	for (i = len - 1; i > 0; i--) if (path[i] == '.') break;
	if (i <= 0) i = len;
	cp = &path[i];

	for (i = 0, j = 0; i < 8 && &path[j] < cp; i++, j++) {
		if (!issjis1((u_char)(path[j]))) buf[i] = transchar(path[j]);
		else {
			buf[i++] = path[j++];
			buf[i] = path[j];
		}
	}
	if (++cp < &path[len]) {
		buf[i++] = '.';
		j = cp - path;
		if (len - j > 3) len = j + 3;
		for (; j < len; i++, j++) {
			if (!issjis1((u_char)(path[j])))
				buf[i] = transchar(path[j]);
			else {
				buf[i++] = path[j++];
				buf[i] = path[j];
			}
		}
	}
	buf[i] = '\0';
	return(i);
}

static int cmpdospath(path1, path2, len, dd)
char *path1, *path2;
int len, dd;
{
	int i, c1, c2, kanji2;

	kanji2 = 0;
	if (len < 0) len = strlen(path1) + 1;
	c1 = path1[0];
	c2 = path2[0];
	if (!(devlist[dd].flags & F_VFAT)) for (i = 0; i < len; i++) {
		c1 = path1[i];
		c2 = path2[i];
		if (!c1 || !c2) break;
		if (!kanji2 && c1 != '/' && c1 != '\\') {
			c1 = transchar(c1);
			c2 = transchar(c2);
		}
		if (c1 != c2) break;
		if (issjis1((u_char)(path1[i]))) kanji2 = 1;
	}
	else for (i = 0; i < len; i++) {
		c1 = path1[i];
		c2 = path2[i];
		if (!c1 || !c2) break;
		if (!kanji2 && c1 != '/' && c1 != '\\') {
			if (strchr(NOTINLFN, c1)) c1 = '_';
			else c1 = toupper2(c1);
			if (strchr(NOTINLFN, c2)) c2 = '_';
			else c2 = toupper2(c2);
		}
		if (c1 != c2) break;
		if (issjis1((u_char)(path1[i]))) kanji2 = 1;
	}
	return(c1 - c2);
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
	if (buf[0] == 0x05) ((u_char *)buf)[0] = 0xe5;

	return(buf);
}

static char *putdosname(buf, file, vol)
char *buf, *file;
int vol;
{
	char *cp, num[9];
	int i, j;

	if ((cp = strrchr(file, '.')) == file) cp = NULL;

	for (i = 0; i < 8; i++) {
		if (file == cp || !*file) buf[i] = ' ';
		else if (!issjis1((u_char)(*file)))
			buf[i] = transchar(*(file++));
		else {
			buf[i++] = *(file++);
			buf[i] = *(file++);
		}
	}
	if (vol > 1 || (vol > 0 && *file)) {
		sprintf(num, "%d", vol);
		for (i = 7 - (int)strlen(num); i > 0; i--)
			if (buf[i - 1] != ' ') break;
		buf[i++] = '~';
		for (j = 0; num[j]; j++) buf[i++] = num[j];
	}
	if (cp) cp++;
	for (i = 8; i < 11; i++) {
		if (!cp || !*cp) buf[i] = ' ';
		else if (!issjis1((u_char)(*cp))) buf[i] = transchar(*(cp++));
		else {
			buf[i++] = *(cp++);
			buf[i] = *(cp++);
		}
	}
	buf[i] = '\0';
	if (((u_char *)buf)[0] == 0xe5) buf[0] = 0x05;

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
	if (attr & DS_IFDIR) mode |= (S_IFDIR | S_IEXEC_ALL);
	else if (attr & DS_IFLABEL) mode |= S_IFIFO;
	else if (attr & DS_IFSYSTEM) mode |= S_IFSOCK;
	else mode |= S_IFREG;

	return(mode);
}

static u_char putdosmode(mode)
u_short mode;
{
	u_char attr;

	attr = DS_IARCHIVE;
	if (!(mode & S_IREAD)) attr |= DS_IHIDDEN;
	if (!(mode & S_IWRITE)) attr |= DS_IRDONLY;
	if ((mode & S_IFMT) == S_IFDIR) attr |= DS_IFDIR;
	else if ((mode & S_IFMT) == S_IFIFO) attr |= DS_IFLABEL;
	else if ((mode & S_IFMT) == S_IFSOCK) attr |= DS_IFSYSTEM;

	return(attr);
}

static time_t getdostime(d, t)
u_short d, t;
{
	struct tm tm;

	tm.tm_year = 1980 + ((d >> 9) & 0x7f);
	tm.tm_year -= 1900;
	tm.tm_mon = ((d >> 5) & 0x0f) - 1;
	tm.tm_mday = (d & 0x1f);
	tm.tm_hour = ((t >> 11) & 0x1f);
	tm.tm_min = ((t >> 5) & 0x3f);
	tm.tm_sec = ((t << 1) & 0x3e);

	return(timelocal2(&tm));
}

static u_char *putdostime(buf, clock)
u_char *buf;
time_t clock;
{
	struct timeval t_val;
	struct timezone tz;
	struct tm *tm;
	int d, t;

	if (clock < 0) gettimeofday(&t_val, &tz);
	else t_val.tv_sec = clock;
	tm = localtime(&(t_val.tv_sec));
	d = (((tm -> tm_year - 80) & 0x7f) << 9)
		+ (((tm -> tm_mon + 1) & 0x0f) << 5)
		+ (tm -> tm_mday & 0x1f);
	t = ((tm -> tm_hour & 0x1f) << 11)
		+ ((tm -> tm_min & 0x3f) << 5)
		+ ((tm -> tm_sec & 0x3e) >> 1);

	if (!t && clock < 0) t = 0x0001;
	buf[0] = t & 0xff;
	buf[1] = (t >> 8) & 0xff;
	buf[2] = d & 0xff;
	buf[3] = (d >> 8) & 0xff;

	return(buf);
}

static int getdrive(path)
char *path;
{
	int i, drive;

	drive = *(path++);
	if (!isalpha(drive) || *(path++) != ':') {
		doserrno = ENOENT;
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
	int i, drive;

	cp = buf;
	if ((drive = getdrive(path)) < 0) return(-1);
	i = toupper2(drive) - 'A';
	path += 2;
	if (*path != '/' && *path != '\\' && curdir[i]) {
		strcpy(buf, curdir[i]);
		cp += strlen(buf);
	}

	while (*path) {
		for (i = 0; path[i] && path[i] != '/' && path[i] != '\\'; i++)
			if (issjis1((u_char)(path[i]))) i++;
		if (class && !path[i]) {
			if (!(i == 1 && *path == '.')
			&& !(i == 2 && *path == '.' && *(path + 1) == '.')
			&& cp + i + 1 - buf >= DOSMAXPATHLEN) {
				doserrno = ENAMETOOLONG;
				return(-1);
			}
			*(cp++) = '/';
			strncpy(cp, path, i);
			cp += i;
			if (path[i - 1] == '.') cp--;
		}
		else if (!i || (i == 1 && *path == '.'));
		else if (i == 2 && *path == '.' && *(path + 1) == '.') {
			for (cp--; cp > buf; cp--) if (*cp == '/') break;
			if (cp < buf) cp = buf;
		}
		else {
			if (cp + i + 1 - buf >= DOSMAXPATHLEN) {
				doserrno = ENAMETOOLONG;
				return(-1);
			}
			*(cp++) = '/';
			strncpy(cp, path, i);
			cp += i;
			if (path[i - 1] == '.') cp--;
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
	char *cp, *cachepath, *tmp, buf[DOSMAXPATHLEN + 3 + 1];
	int len, dd, drive;

	if ((drive = parsepath(buf, path, 0)) < 0
	|| (dd = opendev(drive)) < 0) reterr(NULL);

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
		if (!cmpdospath(cp, path, len, dd)
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
		len = cp - path;
		tmp = path;
		path = (*cp) ? cp + 1 : cp;
		if (!len || (len == 1 && *tmp == '.')) continue;

		if (!(dp = finddir(xdirp, tmp, len))) {
			dosclosedir((DIR *)xdirp);
			errno = (doserrno) ? doserrno : ENOENT;
			return(NULL);
		}

		dentp = (dent_t *)&(xdirp -> dd_buf[dp -> d_fileno]);
		xdirp -> dd_top =
		xdirp -> dd_off = byte2word(dentp -> clust);
		xdirp -> dd_loc = 0;
		cachepath += len;
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
	if ((ret = closedev(xdirp -> dd_fd)) < 0) errno = doserrno;
	free(xdirp);
	return(ret);
}

static int seekdent(xdirp, dentp, clust, offset)
dosDIR *xdirp;
dent_t *dentp;
long clust;
u_short offset;
{
	long next;

	if (xdirp -> dd_id != -1) {
		doserrno = EINVAL;
		return(-1);
	}
	xdirp -> dd_off = clust;
	if ((next = clustread(&devlist[xdirp -> dd_fd],
	xdirp -> dd_buf, xdirp -> dd_off)) < 0) return(-1);
	dd2clust(xdirp -> dd_fd) = xdirp -> dd_off;
	xdirp -> dd_off = next;
	dd2offset(xdirp -> dd_fd) = xdirp -> dd_loc = offset;
	if (dentp) memcpy(dentp, (dent_t *)&(xdirp -> dd_buf[xdirp -> dd_loc]),
		sizeof(dent_t));
	xdirp -> dd_loc += DOSDIRENT;
	if (xdirp -> dd_loc >= xdirp -> dd_size) xdirp -> dd_loc = 0;
	return(0);
}

static int readdent(xdirp, dentp, force)
dosDIR *xdirp;
dent_t *dentp;
int force;
{
	long next;

	if (xdirp -> dd_id != -1) {
		doserrno = EINVAL;
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
	if (force || dentp -> name[0]) {
		xdirp -> dd_loc += DOSDIRENT;
		if (xdirp -> dd_loc >= xdirp -> dd_size) xdirp -> dd_loc = 0;
	}
	return(0);
}

static struct dirent *_dosreaddir(xdirp, all)
dosDIR *xdirp;
int all;
{
	dent_t *dentp;
	static st_dirent d;
	struct dirent *dp;
	char *cp, buf[LFNENTSIZ * 2 + 1];
	long loc, clust;
	u_short ch, offset;
	int i, j, cnt, sum;

	dp = (struct dirent *)(&d);
	dentp = dd2dentp(xdirp -> dd_fd);
	dp -> d_name[0] = '\0';
	dp -> d_reclen = DOSDIRENT;
	cnt = -1;
	for (;;) {
		loc = xdirp -> dd_loc;
		if (readdent(xdirp, dentp, 0) < 0 || !(dentp -> name[0])) {
			*dd2path(xdirp -> dd_fd) = '\0';
			doserrno = 0;
			return(NULL);
		}
		if (dentp -> name[0] == 0xe5) {
			dp -> d_name[0] = '\0';
			dp -> d_reclen = DOSDIRENT;
			cnt = -1;
			continue;
		}
		if (dentp -> attr != DS_IFLFN) break;

		cp = (char *)dentp;
		if (cnt < 0) {
			if ((cnt = *cp - '@') < 0) continue;
			lfn_clust = dd2clust(xdirp -> dd_fd);
			lfn_offset = dd2offset(xdirp -> dd_fd);
			sum = dentp -> dummy[1];
		}
		else if (!cnt || cnt != *cp + 1 || sum != dentp -> dummy[1]) {
			cnt = -1;
			dp -> d_name[0] = '\0';
			dp -> d_reclen = DOSDIRENT;
			continue;
		}
		else cnt--;

		if (!(devlist[xdirp -> dd_fd].flags & F_VFAT)) continue;

		dp -> d_reclen += DOSDIRENT;
		for (i = 1, j = 0; i < DOSDIRENT; i += 2, j++) {
			if (cp + i == (char *)&(dentp -> attr)) i += 3;
			else if (cp + i == (char *)(dentp -> clust)) i += 2;
			if (!(ch = lfndecode(cp[i + 1], cp[i]))) break;
			if (ch & 0xff00) buf[j++] = (ch >> 8) & 0xff;
			buf[j] = ch & 0xff;
		}
		i = strlen(dp -> d_name);
		if (i + j > MAXNAMLEN && (j = MAXNAMLEN - i) < 0) j = 0;
		buf[j] = '\0';
		if (j > 0) {
			for (i = strlen(dp -> d_name); i >= 0; i--)
				dp -> d_name[i + j] = dp -> d_name[i];
			memcpy(dp -> d_name, buf, j);
		}
		clust = xdirp -> dd_off;
		offset = xdirp -> dd_loc;
	}
	dp -> d_fileno = loc;
	if (cnt != 1) dp -> d_name[0] = '\0';
	else if (sum != checksum(dd2dentp(xdirp -> dd_fd) -> name)) {
		cnt = -1;
		dp -> d_name[0] = '\0';
	}
	if (cnt <= 0) lfn_clust = -1;
	if (!dp -> d_name[0])
		getdosname(dp -> d_name, dd2dentp(xdirp -> dd_fd) -> name,
			dd2dentp(xdirp -> dd_fd) -> ext);
	else if (all) {
		xdirp -> dd_off = clust;
		xdirp -> dd_loc = offset;
	}
	if (cp = strrchr(dd2path(xdirp -> dd_fd), '/')) cp++;
	else cp = dd2path(xdirp -> dd_fd);
	strcpy(cp, dp -> d_name);

	return(dp);
}

struct dirent *dosreaddir(dirp)
DIR *dirp;
{
	dosDIR *xdirp;
	struct dirent *dp;
	int i;

	xdirp = (dosDIR *)dirp;
	if (!(dp = _dosreaddir(xdirp, 0))) reterr(NULL);
	if (!(devlist[xdirp -> dd_fd].flags & F_VFAT)) {
		for (i = 0; dp -> d_name[i]; i++) {
			if (issjis1((u_char)(dp -> d_name[i]))) i++;
			else dp -> d_name[i] = detranschar(dp -> d_name[i]);
		}
	}
	return(dp);
}

static struct dirent *finddir(xdirp, fname, len)
dosDIR *xdirp;
char *fname;
int len;
{
	struct dirent *dp;
	char tmp[8 + 1 + 3 + 1];

	if (len <= 0) len = strlen(fname);
	if (devlist[xdirp -> dd_fd].flags & F_VFAT) {
		while (dp = _dosreaddir(xdirp, 1))
			if (!cmpdospath(fname, dp -> d_name,
				len, xdirp -> dd_fd)
			&& !(dp -> d_name[len])) return(dp);
	}
	else {
		transname(tmp, fname, len);
		while (dp = _dosreaddir(xdirp, 1))
			if (!strcmp(tmp, dp -> d_name)) return(dp);
	}
	return(NULL);
}

int dosrewinddir(dirp)
DIR *dirp;
{
	dosDIR *xdirp;

	xdirp = (dosDIR *)dirp;
	if (xdirp -> dd_id != -1) {
		errno = EINVAL;
		return(-1);
	}
	xdirp -> dd_loc = 0;
	xdirp -> dd_off = xdirp -> dd_top;
	return(0);
}

int doschdir(path)
char *path;
{
	DIR *dirp;
	char *tmp, buf[2 + DOSMAXPATHLEN + 3 + 1];
	int drv, drive;

	buf[1] = ':';
	if ((drive = parsepath(buf + 2, path, 0)) < 0) reterr(-1);
	buf[0] = drive;
	drv = toupper2(drive);

	if (!(dirp = dosopendir(buf))) {
		if ((path[2] != '/' && path[2] != '\\')
		|| !(tmp = curdir[drv - 'A'])) reterr(-1);
		curdir[drv - 'A'] = NULL;
		dirp = dosopendir(buf);
		curdir[drv - 'A'] = tmp;
		if (!dirp) reterr(-1);
	}
	dosclosedir(dirp);

	if (!(tmp = (char *)malloc(strlen(buf + 2) + 1))) return(-1);
	if (curdir[drv - 'A']) free(curdir[drv - 'A']);
	curdir[drv - 'A'] = tmp;
	strcpy(curdir[drv - 'A'], buf + 2);
	lastdrive = drive;
	return(0);
}

char *dosgetcwd(pathname, size)
char *pathname;
int size;
{
	int i;

	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);

	i = toupper2(lastdrive) - 'A';
	if (lastdrive < 0 || !curdir[i]) {
		errno = EFAULT;
		return(NULL);
	}

	if (strlen(curdir[i]) > size) {
		errno = ERANGE;
		return(NULL);
	}
	pathname[0] = lastdrive;
	pathname[1] = ':';
	strcpy(pathname + 2, curdir[i]);
	return(pathname);
}

static dosDIR *splitpath(pathp)
char **pathp;
{
	dosDIR *xdirp;
	char dir[MAXPATHLEN + 1];
	int i, j;

	strcpy(dir, *pathp);
	for (i = 1, j = 2; dir[j]; j++) {
		if (issjis1((u_char)(dir[j]))) j++;
		else if (dir[j] == '/' || dir[j] == '\\') i = j;
	}

	*pathp += i + 1;
	if (i < 3) i++;
	dir[i] = '\0';

	i = errno;
	if (!(xdirp = (dosDIR *)dosopendir(dir))) {
		doserrno = errno;
		errno = i;
	}
	return(xdirp);
}

static int getdent(path, ddp)
char *path;
int *ddp;
{
	dosDIR *xdirp;
	char buf[2 + DOSMAXPATHLEN + 3 + 1];
	int dd, drive;

	if (ddp) *ddp = -1;
	buf[1] = ':';
	if ((drive = parsepath(buf + 2, path, 1)) < 0
	|| (dd = opendev(drive)) < 0) return(-1);
	if (devlist[dd].dircache
	&& !cmpdospath(dd2path(dd), buf + 3, -1, dd)) return(dd);
	buf[0] = drive;

	path = buf;
	if (!(xdirp = splitpath(&path))) {
		closedev(dd);
		if (doserrno == ENOENT) doserrno = ENOTDIR;
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
	if (!finddir(xdirp, path, 0)) {
		dosclosedir((DIR *)xdirp);
		if (ddp && !doserrno) *ddp = dd;
		else closedev(dd);
		if (!doserrno) doserrno = ENOENT;
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
		doserrno = EIO;
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

static int expanddent(dd)
int dd;
{
	long prev;

	prev = dd2clust(dd);
	if (!prev || prev > 0xffff) {
		doserrno = ENOSPC;
		return(-1);
	}
	if ((dd2clust(dd) = clustexpand(&devlist[dd], prev, 1)) < 0) return(-1);
	memset(dd2dentp(dd), 0, sizeof(dent_t));
	dd2offset(dd) = 0;
	return(0);
}

static int creatdent(path, mode)
char *path;
int mode;
{
	dosDIR *xdirp;
	dent_t *dentp;
	u_char *cp, longfname[DOSMAXNAMLEN + 1];
	char *file, tmp[8 + 1 + 3 + 1], fname[8 + 3];
	long clust;
	u_short c, offset;
	int i, j, n, len, cnt, sum, ret;

	file = path;
	if (!(xdirp = splitpath(&file))) {
		if (doserrno == ENOENT) doserrno = ENOTDIR;
		return(-1);
	}
	if (!*file) {
		dosclosedir((DIR *)xdirp);
		doserrno = EEXIST;
		return(-1);
	}
	if (!(devlist[xdirp -> dd_fd].flags & F_VFAT)) {
		n = 0;
		cnt = 1;
	}
	else {
		n = 1;
		for (i = j = 0; file[i]; i++, j += 2) {
			if (!issjis1((u_char)(file[i])))
				c = lfnencode(0, file[i]);
			else {
				c = lfnencode(file[i], file[i + 1]);
				i++;
			}
			longfname[j] = c & 0xff;
			longfname[j + 1] = (c >> 8) & 0xff;
		}
		cnt = j / (LFNENTSIZ * 2) + 2;
		longfname[j] = longfname[j + 1] = '\0';
		len = j + 2;
	}

	dentp = dd2dentp(xdirp -> dd_fd);
	putdosname(fname, file, n);
	if (!n || !strncmp(getdosname(tmp, fname, fname + 8), file, 8 + 3))
		cnt = 1;
	else sum = checksum(fname);

	i = j = 0;
	clust = -1;
	for (;;) {
		if (readdent(xdirp, dentp, 1) < 0) {
			if (clust >= 0) break;
			if (doserrno || expanddent(xdirp -> dd_fd) < 0) {
				dosclosedir((DIR *)xdirp);
				return(-1);
			}
			break;
		}
		if (!(dentp -> name[0])) break;
		if (dentp -> name[0] == 0xe5) {
			i++;
			if (clust < 0) {
				clust = dd2clust(xdirp -> dd_fd);
				offset = dd2offset(xdirp -> dd_fd);
			}
			continue;
		}
		if (j < i) j = i;
		if (j < cnt) clust = -1;
		i = 0;
		if (dentp -> attr == DS_IFLFN) continue;

		if (!strncmp((char *)(dentp -> name), fname, 8 + 3)) {
			if (!n) {
				dosclosedir((DIR *)xdirp);
				doserrno = EEXIST;
				return(-1);
			}
			n++;
			putdosname(fname, file, n);
			sum = checksum(fname);
			i = j = 0;
			clust = -1;
			dosrewinddir((DIR *)xdirp);
		}
	}

	if (clust >= 0 && seekdent(xdirp, NULL, clust, offset) < 0) {
		dosclosedir((DIR *)xdirp);
		return(-1);
	}
	cp = (u_char *)dentp;
	while (--cnt > 0) {
		memset(dentp, 0, sizeof(dent_t));
		cp[0] = cnt;
		n = (cnt - 1) * (LFNENTSIZ * 2);
		if (n + (LFNENTSIZ * 2) >= len) cp[0] += '@';
		dentp -> dummy[1] = sum;
		dentp -> attr = DS_IFLFN;
		for (i = 1, j = 0; i < DOSDIRENT; i += 2, j += 2) {
			if (cp + i == (u_char *)&(dentp -> attr)) i += 3;
			else if (cp + i == (u_char *)(dentp -> clust)) i += 2;
			if (n + j >= len) cp[i] = cp[i + 1] = (char)0xff;
			else {
				cp[i] = longfname[n + j];
				cp[i + 1] = longfname[n + j + 1];
			}
		}
		if (writedent(xdirp -> dd_fd) < 0
		|| (readdent(xdirp, dentp, 1) < 0
		&& (doserrno || expanddent(xdirp -> dd_fd) < 0))) {
			dosclosedir((DIR *)xdirp);
			return(-1);
		}
	}

	memset(dentp, 0, sizeof(dent_t));
	memcpy(dentp -> name, fname, 8 + 3);
	dentp -> attr = putdosmode(mode);
	putdostime(dentp -> time, -1);
	if (devlist[xdirp -> dd_fd].flags & F_VFAT) {
		dentp -> dummy[2] = dentp -> time[0];
		dentp -> dummy[3] = dentp -> time[1];
		dentp -> dummy[4] = dentp -> dummy[6] = dentp -> date[0];
		dentp -> dummy[5] = dentp -> dummy[7] = dentp -> date[1];
	}
	ret = (writedent(xdirp -> dd_fd) < 0
	|| writefat(&devlist[xdirp -> dd_fd]) < 0) ? -1 : xdirp -> dd_fd;
	free(xdirp -> dd_buf);
	free(xdirp);
	return(ret);
}

static int unlinklfn(dd, clust, offset, sum)
int dd;
long clust;
u_short offset;
int sum;
{
	dosDIR xdir;
	long dupclust;
	u_short dupoffset;

	if (clust < 0) return(0);
	dupclust = dd2clust(dd);
	dupoffset = dd2offset(dd);
	sum &= 0xff;
	xdir.dd_id = -1;
	xdir.dd_fd = dd;
	xdir.dd_loc = 0;
	xdir.dd_size = (devlist[dd].clustsize) * (devlist[dd].sectsize);
	xdir.dd_top =
	xdir.dd_off = clust;
	if (!(xdir.dd_buf = (char *)malloc(xdir.dd_size))) return(-1);

	seekdent(&xdir, dd2dentp(dd), clust, offset);
	*(dd2dentp(dd) -> name) = 0xe5;
	writedent(dd);
	while (readdent(&xdir, dd2dentp(dd), 0) >= 0) {
		if (!dd2dentp(dd) -> name[0] || dd2dentp(dd) -> name[0] == 0xe5
		|| (dd2clust(dd) == dupclust && dd2offset(dd) == dupoffset)
		|| dd2dentp(dd) -> attr != 0x0f
		|| dd2dentp(dd) -> dummy[1] != sum) break;
		*(dd2dentp(dd) -> name) = 0xe5;
		writedent(dd);
	}
	free(xdir.dd_buf);
	return(0);
}

int dosstat(path, buf)
char *path;
struct stat *buf;
{
	char *cp;
	int dd;

	if ((dd = getdent(path, NULL)) < 0) reterr(-1);
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

	if ((buf -> st_mode & S_IFMT) != S_IFDIR
	&& (cp = strrchr(path, '.')) && strlen(++cp) == 3) {
		if (!strcasecmp2(cp, "BAT") || !strcasecmp2(cp, "COM")
		|| !strcasecmp2(cp, "EXE")) buf -> st_mode |= S_IEXEC_ALL;
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

	if (((mode & R_OK) && !(status.st_mode & S_IREAD))
	|| ((mode & W_OK)
		&& (!(status.st_mode & S_IWRITE)
		|| (devlist[status.st_dev].flags & F_RONLY)))
	|| ((mode & X_OK) && !(status.st_mode & S_IEXEC))) {
		errno = EACCES;
		return(-1);
	}
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

	if ((dd = getdent(path, NULL)) < 0) reterr(-1);
	dd2dentp(dd) -> attr = putdosmode(mode);
	if ((ret = writedent(dd)) < 0) errno = doserrno;
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

	if ((dd = getdent(path, NULL)) < 0) reterr(-1);
	putdostime(dd2dentp(dd) -> time, clock);
	if ((ret = writedent(dd)) < 0) errno = doserrno;
	closedev(dd);
	return(ret);
}

int dosunlink(path)
char *path;
{
	int dd, sum, ret;

	if ((dd = getdent(path, NULL)) < 0) reterr(-1);
	if (dd2dentp(dd) -> attr & DS_IFDIR) {
		errno = EPERM;
		return(-1);
	}
	sum = checksum(dd2dentp(dd) -> name);
	*(dd2dentp(dd) -> name) = 0xe5;

	ret = 0;
	if (writedent(dd) < 0
	|| clustfree(&devlist[dd], byte2word(dd2dentp(dd) -> clust)) < 0
	|| unlinklfn(dd, lfn_clust, lfn_offset, sum) < 0
	|| writefat(&devlist[dd]) < 0) {
		errno = doserrno;
		ret = -1;
	}
	closedev(dd);
	return(ret);
}

int dosrename(from, to)
char *from, *to;
{
	char buf[DOSMAXPATHLEN + 3 + 1];
	long clust;
	u_short offset;
	int i, dd, fd, sum, ret;

	if ((i = parsepath(buf, to, 0)) < 0) reterr(-1);
	if ((i & 027) != (getdrive(from) & 027)) {
		errno = EXDEV;
		return(-1);
	}
	if ((dd = getdent(from, NULL)) < 0) reterr(-1);

	clust = lfn_clust;
	offset = lfn_offset;

	i = strlen(dd2path(dd));
	if (!cmpdospath(dd2path(dd), buf + 1, i, dd)
	&& (!buf[i + 1] || buf[i + 1] == '/')) {
		closedev(dd);
		errno = EINVAL;
		return(-1);
	}
	if ((fd = dosopen(to, O_RDWR | O_CREAT | O_EXCL, 0666)) < 0) {
		closedev(dd);
		return(-1);
	}
	sum = checksum(dd2dentp(dd) -> name);
	memcpy(&(fd2dentp(fd) -> attr) , &(dd2dentp(dd) -> attr),
		sizeof(dent_t) - (8 + 3));
	*(dd2dentp(dd) -> name) = 0xe5;

	free(dosflist[fd]._base);
	dosflist[fd]._base = NULL;
	while (maxdosf > 0 && !dosflist[maxdosf - 1]._base) maxdosf--;
	fd2clust(fd) = dosflist[fd]._clust;
	fd2offset(fd) = dosflist[fd]._offset;

	ret = 0;
	if (writedent(dosflist[fd]._file) < 0 || writedent(dd) < 0
	|| unlinklfn(dd, clust, offset, sum) < 0
	|| writefat(&devlist[dd]) < 0) {
		errno = doserrno;
		ret = -1;
	}
	closedev(dosflist[fd]._file);
	closedev(dd);
	return(ret);
}

int dosopen(path, flags, mode)
char *path;
int flags, mode;
{
	int fd, dd, tmp;

	for (fd = 0; fd < maxdosf; fd++) if (!dosflist[fd]._base) break;
	if (fd >= DOSNOFILE) {
		errno = EMFILE;
		return(-1);
	}

	if ((dd = getdent(path, &tmp)) < 0) {
		if (doserrno != ENOENT || tmp < 0 || !(flags & O_CREAT)
		|| (dd = creatdent(path, mode)) < 0) {
			errno = doserrno;
			if (tmp >= 0) closedev(tmp);
			return(-1);
		}
		else {
			if (!(devlist[tmp].flags & F_DUPL))
				devlist[dd].flags &= ~F_DUPL;
			devlist[tmp].flags |= F_DUPL;
			closedev(tmp);
		}
	}
	else if ((flags & O_CREAT) && (flags & O_EXCL)) {
		closedev(dd);
		errno = EEXIST;
		return(-1);
	}

	if (flags & (O_WRONLY | O_RDWR)) {
		tmp = 0;
		if (devlist[dd].flags & F_RONLY) tmp = EROFS;
		else if (dd2dentp(dd) -> attr & DS_IFDIR) tmp = EISDIR;
		if (tmp) {
			errno = tmp;
			closedev(dd);
			return(-1);
		}
	}
	if (flags & O_TRUNC) {
		if ((flags & (O_WRONLY | O_RDWR))
		&& clustfree(&devlist[dd],
		byte2word(dd2dentp(dd) -> clust)) < 0) {
			errno = doserrno;
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
		|| writefat(fd2devp(fd)) < 0) {
			errno = doserrno;
			ret = -1;
		}
	}

	closedev(dosflist[fd]._file);
	return(ret);
}

static int dosfilbuf(fd, wrt)
int fd, wrt;
{
	long new, prev;
	int size;

	size = (wrt) ? dosflist[fd]._bufsize :
		dosflist[fd]._size - dosflist[fd]._loc;
	prev = dosflist[fd]._off;
	if (dosflist[fd]._off = dosflist[fd]._next)
		dosflist[fd]._next = clustread(fd2devp(fd),
			dosflist[fd]._base, dosflist[fd]._next);
	else {
		doserrno = 0;
		dosflist[fd]._next = -1;
	}

	if (dosflist[fd]._next < 0) {
		if (doserrno) return(-1);
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
		doserrno = EIO;
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
		if (dosflist[fd]._cnt <= 0 && dosfilbuf(fd, 0) < 0) reterr(-1);
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
		if (dosflist[fd]._cnt <= 0 && dosfilbuf(fd, 1) < 0) reterr(-1);
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

		if (dosflsbuf(fd) < 0) reterr(-1);
	}

	return(total);
}

off_t doslseek(fd, offset, whence)
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
		errno = doserrno;
		dosflist[fd]._dent.name[0] = 0xe5;
		dosclose(fd + DOSFDOFFSET);
		return(-1);
	}
	dent[0].clust[0] = clust & 0xff;
	dent[0].clust[1] = (clust >> 8) & 0xff;

	memcpy(&dent[1], &(dosflist[fd]._dent), sizeof(dent_t));
	memset(dent[1].name, ' ', 8 + 3);
	dent[1].name[0] =
	dent[1].name[1] = '.';
	clust = fd2clust(fd);
	dent[1].clust[0] = clust & 0xff;
	dent[1].clust[1] = (clust >> 8) & 0xff;

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
	long clust;
	u_short offset;
	int sum, ret;

	if (!(xdirp = (dosDIR *)dosopendir(path))) return(-1);
	memcpy(&cache, devlist[xdirp -> dd_fd].dircache, sizeof(cache_t));

	clust = lfn_clust;
	offset = lfn_offset;

	while (dp = _dosreaddir((DIR *)xdirp, 0))
		if (strcmp(dp -> d_name, ".") && strcmp(dp -> d_name, "..")) {
			dosclosedir((DIR *)xdirp);
			errno = ENOTEMPTY;
			return(-1);
		}

	ret = 0;
	if (doserrno) {
		errno = doserrno;
		ret = -1;
	}
	else {
		memcpy(devlist[xdirp -> dd_fd].dircache, &cache,
			sizeof(cache_t));
		sum = checksum(dd2dentp(xdirp -> dd_fd) -> name);
		*(dd2dentp(xdirp -> dd_fd) -> name) = 0xe5;
		if (writedent(xdirp -> dd_fd) < 0
		|| clustfree(&devlist[xdirp -> dd_fd],
			byte2word(dd2dentp(xdirp -> dd_fd) -> clust)) < 0
		|| unlinklfn(xdirp -> dd_fd, clust, offset, sum) < 0
		|| writefat(&devlist[xdirp -> dd_fd]) < 0) {
			errno = doserrno;
			ret = -1;
		}
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

	if (!type) {
		errno = 0;
		return(NULL);
	}
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

	if ((fd = stream2fd(stream)) < 0) {
		errno = 0;
		return(EOF);
	}
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
			reterr(0);
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
			reterr(0);
		}
		rest = dosflist[fd]._bufsize;
	}
	if (nbytes > 0) {
		if (rest >= dosflist[fd]._bufsize && dosfilbuf(fd, 1) < 0) {
			dosflist[fd]._flag |= O_IOERR;
			reterr(0);
		}
		memcpy(dosflist[fd]._ptr, buf, nbytes);
		dosflist[fd]._ptr += nbytes;
		total += nbytes;
	}

	if (!total) errno = 0;
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
		reterr(EOF);
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
		if (devlist[i].drive && closedev(i) < 0) {
			errno = doserrno;
			ret = -1;
		}
	return(ret);
}
#endif	/* !_NODOSDRIVE */
