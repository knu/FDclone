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

#ifdef	NOVOID
#define	VOID
#define	VOID_T	int
#else
#define	VOID	void
#define	VOID_T	void
#endif

#if	MSDOS
#include <io.h>
#include "unixemu.h"
#include "unixdisk.h"
#include <sys/timeb.h>
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

#ifndef	ENOTEMPTY
# ifdef	ENFSNOTEMPTY
# define	ENOTEMPTY	ENFSNOTEMPTY
# else
# define	ENOTEMPTY	EACCES
# endif
#endif

#ifndef	EPERM
#define	EPERM	EACCES
#endif
#ifndef	EIO
#define	EIO	EACCES
#endif
#ifndef	ENXIO
#define	ENXIO	EACCES
#endif
#ifndef	EFAULT
#define	EFAULT	ENODEV
#endif
#ifndef	ENOSPC
#define	ENOSPC	EACCES
#endif
#ifndef	EISDIR
#define	EISDIR	EACCES
#endif
#ifndef	EROFS
#define	EROFS	EACCES
#endif
#ifndef	ENAMETOOLONG
#define	ENAMETOOLONG	ERANGE
#endif
#ifndef	O_BINARY
#define	O_BINARY	0
#endif

#ifndef issjis1
#define issjis1(c)	((0x81 <= (c) && (c) <= 0x9f)\
			|| (0xe0 <= (c) && (c) <= 0xfc))
#endif

#define	reterr(c)	{errno = doserrno; return(c);}
#define	S_IEXEC_ALL	(S_IEXEC | (S_IEXEC >> 3) | (S_IEXEC >> 6))
#define	UNICODETBL	"unicode.tbl"
#define	MINUNICODE	0x00a7
#define	MAXUNICODE	0xffe5
#define	MINKANJI	0x8140
#define	MAXKANJI	0xeaa4

#ifdef	FD
extern int toupper2 __P_((int));
extern int strcasecmp2 __P_((char *, char *));
extern int isdotdir __P_((char *));
extern time_t timelocal2 __P_((struct tm *));
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
#define	char2long(cp)	(  (long)(((u_char *)cp)[3]) \
			| (long)((((u_char *)cp)[2]) << (CHAR_BIT * 1)) \
			| (long)((((u_char *)cp)[1]) << (CHAR_BIT * 2)) \
			| (long)((((u_char *)cp)[0]) << (CHAR_BIT * 3)) )
static int toupper2 __P_((int));
static int strcasecmp2 __P_((char *, char *));
static int isdotdir __P_((char *));
#if	!MSDOS && !defined (NOTZFILEH)\
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static int tmcmp __P_((struct tm *, struct tm *));
#endif
#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long gettimezone __P_((struct tm *, time_t));
#endif
static time_t timelocal2 __P_((struct tm *));
#endif

#if	!MSDOS
static int sectseek __P_((devstat *, long));
#endif	/* !MSDOS */
static int shiftcache __P_((int));
static int savecache __P_((devstat *, long, u_char *));
static int loadcache __P_((devstat *, long, u_char *));
static int sectread __P_((devstat *, long, u_char *, int));
static int sectwrite __P_((devstat *, long, u_char *, int));
static int readfat __P_((devstat *));
static int writefat __P_((devstat *));
static long getfatofs __P_((devstat *, long));
static u_char *readtmpfat __P_((devstat *, long));
static long getfatent __P_((devstat *, long));
static int putfatent __P_((devstat *, long, long));
static long clust2sect __P_((devstat *, long));
static long newclust __P_((devstat *));
static long clustread __P_((devstat *, u_char *, long));
static long clustwrite __P_((devstat *, u_char *, long));
static long clustexpand __P_((devstat *, long, int));
static int clustfree __P_((devstat *, long));
#if	MSDOS
static int readbpb __P_((devstat *));
#else
static int readbpb __P_((devstat *, bpb_t *));
#endif
static int opendev __P_((int));
static int closedev __P_((int));
static int checksum __P_((char *));
static u_short cnvunicode __P_((u_short, int));
static u_short lfnencode __P_((u_char, u_char));
static u_short lfndecode __P_((u_char, u_char));
static int transchar __P_((int));
static int detranschar __P_((int));
static int cmpdospath __P_((char *, char *, int, int));
static char *getdosname __P_((char *, char *, char *));
static char *putdosname __P_((char *, char *, int));
static u_short getdosmode __P_((u_char));
static u_char putdosmode __P_((u_short));
static time_t getdostime __P_((u_short, u_short));
static u_char *putdostime __P_((u_char *, time_t));
static int getdrive __P_((char *));
static int parsepath __P_((char *, char *, int));
static int seekdent __P_((dosDIR *, dent_t *, long, u_short));
static int readdent __P_((dosDIR *, dent_t *, int));
static dosDIR *_dosopendir __P_((char *, char *, int));
static int _dosclosedir __P_((dosDIR *));
static struct dirent *_dosreaddir __P_((dosDIR *, int));
static struct dirent *finddir __P_((dosDIR *, char *, int, int));
static dosDIR *splitpath __P_((char **, char *, int));
static int getdent __P_((char *, int *));
static int writedent __P_((int));
static int expanddent __P_((int));
static int creatdent __P_((char *, int));
static int unlinklfn __P_((int, long, u_short, int));
static int dosfilbuf __P_((int, int));
static int dosflsbuf __P_((int));

#if	!MSDOS
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
#endif	/* !MSDOS */
char *unitblpath = NULL;

#if	!MSDOS
static char *curdir['Z' - 'A' + 1];
#endif
static cache_t *dentcache['Z' - 'A' + 1];
static devstat devlist[DOSNOFILE];
static int maxdev = 0;
static dosFILE dosflist[DOSNOFILE];
static int maxdosf = 0;
static u_char cachedrive[SECTCACHESIZE];
static long sectno[SECTCACHESIZE];
static u_char *sectcache[SECTCACHESIZE];
static int maxsectcache = 0;
static long lfn_clust = -1;
static u_short lfn_offset = 0;
static int doserrno = 0;
static short sectsizelist[] = SECTSIZE;


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

static int isdotdir(name)
char *name;
{
	if (name[0] == '.'
	&& (!name[1] || (name[1] == '.' && !name[2]))) return(1);
	return(0);
}

#if	!MSDOS && !defined (NOTZFILEH)\
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
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
#endif	/* !MSDOS && !NOTZFILEH && !USEMKTIME && !USETIMELOCAL */

#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long gettimezone(tm, t)
struct tm *tm;
time_t t;
{
#if	MSDOS
	struct timeb buffer;

	ftime(&buffer);
	return(buffer.timezone);
#else	/* !MSDOS */
#ifdef	NOTMGMTOFF
	struct timeval t_val;
	struct timezone t_zone;
#endif
#ifndef	NOTZFILEH
	struct tzhead head;
#endif
	struct tm tmbuf;
	FILE *fp;
	time_t tmp;
	long i, tz, leap, nleap, ntime, ntype, nchar;
	char *cp, buf[MAXPATHLEN + 1];
	u_char c;

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
	if (cp[0] == _SC_) strcpy(buf, cp);
	else {
		strcpy(buf, TZDIR);
		strcat(buf, _SS_);
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
		if (fread(buf, sizeof(char), 4, fp) != 4) {
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
	|| fread(buf, sizeof(char), 4, fp) != 4) {
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
		if (fread(buf, sizeof(char), 4, fp) != 4) {
			fclose(fp);
			return(tz);
		}
		tmp = char2long(buf);
		if (tmcmp(&tmbuf, localtime(&tmp)) <= 0) break;
		if (fread(buf, sizeof(char), 4, fp) != 4) {
			fclose(fp);
			return(tz);
		}
		leap = char2long(buf);
	}

	tz += leap;
	fclose(fp);
#endif	/* !NOTZFILEH */

	return(tz);
#endif	/* !MSDOS */
}
#endif	/* !USEMKTIME && !USETIMELOCAL */

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

#if	!MSDOS
static int sectseek(devp, sect)
devstat *devp;
long sect;
{
	int tmperrno;

	tmperrno = errno;
	if (devp -> flags & F_8SECT) sect += sect / 8;
	if (lseek(devp -> fd, sect * (long)(devp -> sectsize), L_SET) < 0) {
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}
	errno = tmperrno;
	return(0);
}
#endif	/* !MSDOS */

static int shiftcache(n)
int n;
{
	u_char *cp;
	long no;
	int i, drive;

	cp = NULL;
	no = 0;
	drive = 0;
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
u_char *buf;
{
	u_char *cp;
	int i, tmperrno;

	tmperrno = errno;
	for (i = 0; i < maxsectcache; i++)
	if (devp -> drive == cachedrive[i] && sect == sectno[i]) {
		memcpy((char *)sectcache[i], (char *)buf, devp -> sectsize);
		shiftcache(i);
		return(1);
	}
	if (!(cp = (u_char *)malloc(devp -> sectsize))) {
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}
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
u_char *buf;
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
u_char *buf;
int n;
{
	int i, tmperrno;

	tmperrno = errno;
	for (i = 0; i < n; i++, sect++, buf += devp -> sectsize) {
		if (loadcache(devp, sect, buf) >= 0) continue;
#if	MSDOS
		if (rawdiskio(devp -> drive, sect, buf,
		devp -> sectsize, 0) < 0) {
			doserrno = errno;
			errno = tmperrno;
			return(-1);
		}
#else
		if (sectseek(devp, sect) < 0) return(-1);
		while (read(devp -> fd, buf, devp -> sectsize) < 0) {
			if (errno == EINTR) errno = tmperrno;
			else {
				doserrno = errno;
				return(-1);
			}
		}
#endif
		savecache(devp, sect, buf);
	}
	errno = tmperrno;
	return(n * (long)(devp -> sectsize));
}

static int sectwrite(devp, sect, buf, n)
devstat *devp;
long sect;
u_char *buf;
int n;
{
	int i, tmperrno;

	if (devp -> flags & F_RONLY) {
		doserrno = EROFS;
		return(-1);
	}
	tmperrno = errno;
	for (i = 0; i < n; i++, sect++, buf += devp -> sectsize) {
		savecache(devp, sect, buf);
#if	MSDOS
		if (rawdiskio(devp -> drive, sect, buf,
		devp -> sectsize, 1) < 0) {
			doserrno = errno;
			errno = tmperrno;
			return(-1);
		}
#else
		if (sectseek(devp, sect) < 0) return(-1);
		if (write(devp -> fd, buf, devp -> sectsize) < 0) {
			if (errno == EINTR) errno = tmperrno;
			else {
				doserrno = errno;
				return(-1);
			}
		}
#endif
	}
	errno = tmperrno;
	return(n * (long)(devp -> sectsize));
}

static int readfat(devp)
devstat *devp;
{
	return(sectread(devp, devp -> fatofs,
		devp -> fatbuf, devp -> fatsize));
}

static int writefat(devp)
devstat *devp;
{
	long i;

	i = (devp -> flags & F_WRFAT);
	devp -> flags &= ~F_WRFAT;
	if (!i || !devp -> fatbuf) return(0);

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

static long getfatofs(devp, clust)
devstat *devp;
long clust;
{
	long ofs, bit;

	bit = (devp -> flags & F_16BIT) ? 4 : 3;
	ofs = (clust * bit) / 2;
	if (ofs < bit
	|| ofs >= (long)(devp -> fatsize) * (long)(devp -> sectsize)) {
		doserrno = EIO;
		return(-1);
	}
	return(ofs);
}

static u_char *readtmpfat(devp, ofs)
devstat *devp;
long ofs;
{
	u_char *buf;
	int tmperrno;

	tmperrno = errno;
	if (!(buf = (u_char *)malloc((devp -> sectsize) * 2))) {
		doserrno = errno;
		errno = tmperrno;
		return(NULL);
	}
	ofs /= (devp -> sectsize);
	if (sectread(devp, (long)(devp -> fatofs) + ofs, buf, 2) < 0) {
		free(buf);
		return(NULL);
	}
	return(buf);
}

static long getfatent(devp, clust)
devstat *devp;
long clust;
{
	u_char *fatp, *buf;
	long ofs;

	if ((ofs = getfatofs(devp, clust)) < 0) return(-1);
	if ((devp -> fatbuf)) {
		buf = NULL;
		fatp = (u_char *)&(devp -> fatbuf[ofs]);
	}
	else {
		if (!(buf = readtmpfat(devp, ofs))) return(-1);
		fatp = buf + (ofs % (devp -> sectsize));
	}
	if (devp -> flags & F_16BIT) {
		ofs = (*fatp & 0xff) + ((long)(*(fatp + 1) & 0xff) << 8);
		if (ofs >= 0x0fff8) ofs = 0xffff;
	}
	else {
		if (clust % 2) ofs = (((*fatp) & 0xf0) >> 4)
				+ ((long)(*(fatp + 1) & 0xff) << 4);
		else ofs = (*fatp & 0xff) + ((long)(*(fatp + 1) & 0x0f) << 8);
		if (ofs >= 0x0ff8) ofs = 0xffff;
	}
	if (buf) free(buf);
	return(ofs);
}

static int putfatent(devp, clust, n)
devstat *devp;
long clust, n;
{
	u_char *fatp, *buf;
	long ofs;

	if ((ofs = getfatofs(devp, clust)) < 0) return(-1);
	if ((devp -> fatbuf)) {
		buf = NULL;
		fatp = (u_char *)&(devp -> fatbuf[ofs]);
	}
	else {
		if (!(buf = readtmpfat(devp, ofs))) return(-1);
		fatp = buf + (ofs % (devp -> sectsize));
	}
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
	if (!buf) devp -> flags |= F_WRFAT;
	else {
		long i;

		for (i = devp -> fatofs; i < devp -> dirofs;
		i += devp -> fatsize) {
			if (sectwrite(devp, i + (ofs / (devp -> sectsize)),
			buf, 2) < 0) {
				free(buf);
				return(-1);
			}
		}
		free(buf);
	}
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
		sect = ((clust - 2) * (long)(devp -> clustsize))
			+ ((long)(devp -> dirofs) + (long)(devp -> dirsize));
	}
	return(sect);
}

static long newclust(devp)
devstat *devp;
{
	long clust, used;

	for (clust = 2; (used = getfatent(devp, clust)) >= 0; clust++)
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
	if (sect < (long)(devp -> dirofs) + (long)(devp -> dirsize))
		next = 0x10000 + sect + (long)(devp -> clustsize);
	else next = getfatent(devp, clust);

	if (buf && sectread(devp, sect, buf, devp -> clustsize) < 0) return(-1);
	return(next);
}

static long clustwrite(devp, buf, prev)
devstat *devp;
u_char *buf;
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

	if ((prev && putfatent(devp, prev, clust) < 0)
	|| putfatent(devp, clust, 0xffff) < 0) return(-1);
	return(clust);
}

static long clustexpand(devp, clust, fill)
devstat *devp;
long clust;
int fill;
{
	u_char *buf;
	long new, size;
	int tmperrno;

	if (!fill) buf = NULL;
	else {
		tmperrno = errno;
		size = (long)(devp -> clustsize) * (long)(devp -> sectsize);
		if (!(buf = (u_char *)malloc(size))) {
			doserrno = errno;
			errno = tmperrno;
			return(-1);
		}
		memset((char *)buf, 0, size);
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
		next = getfatent(devp, clust);
		if (putfatent(devp, clust, 0) < 0) return(-1);
		if (next == 0xffff) break;
		clust = next;
	}
	return(0);
}

#if	MSDOS
static int readbpb(devp)
devstat *devp;
#else
static int readbpb(devp, bpbcache)
devstat *devp;
bpb_t *bpbcache;
#endif
{
#if	!MSDOS
	int fd, nsect;
#endif
	bpb_t *bpb;
	char *buf;
	long total;
	int i, cc, tmperrno;

	tmperrno = errno;
#if	MSDOS
	cc = 0;
	for (i = 0; i < (sizeof(sectsizelist) / sizeof(short)); i++)
		if (cc < sectsizelist[i]) cc = sectsizelist[i];
	if (!(buf = (char *)malloc(cc))) {
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}
	bpb = (bpb_t *)buf;
	if (rawdiskio(devp -> drive, 0, buf, cc, 0) < 0) {
		doserrno = errno;
		free(buf);
		errno = tmperrno;
		return(-1);
	}
#else	/* !MSDOS */
	buf = NULL;
	if (!(devp -> ch_head) || !(nsect = devp -> ch_sect)) return(0);

	if (nsect <= 100) devp -> flags &= ~F_8SECT;
	else {
		nsect %= 100;
		devp -> flags |= F_8SECT;
	}

	if (bpbcache -> nfat) bpb = bpbcache;
	else {
		devp -> fd = -1;
		if ((fd = open(devp -> ch_name,
		O_RDWR | O_BINARY, 0600)) < 0) {
#ifdef	EFORMAT
			if (errno == EFORMAT) {
				errno = tmperrno;
				return(0);
			}
#endif
			if (errno == EIO) errno = ENXIO;
			if (errno != EROFS
			|| (fd = open(devp -> ch_name,
			O_RDONLY | O_BINARY, 0600)) < 0) {
				doserrno = errno;
				errno = tmperrno;
				return(-1);
			}
			devp -> flags |= F_RONLY;
		}
		if (lseek(fd, 0L, L_SET) < 0) {
			close(fd);
			errno = tmperrno;
			return(0);
		}

		cc = 0;
		for (i = 0; i < (sizeof(sectsizelist) / sizeof(short)); i++)
			if (cc < sectsizelist[i]) cc = sectsizelist[i];
		if (!(buf = (char *)malloc(cc))) {
			close(fd);
			errno = tmperrno;
			return(0);
		}
		for (i = 0; i < (sizeof(sectsizelist) / sizeof(short)); i++) {
			while ((cc = read(fd, buf, sectsizelist[i])) < 0
			&& errno == EINTR);
			if (cc >= 0) break;
		}
		if (i >= (sizeof(sectsizelist) / sizeof(short))) {
			close(fd);
			free(buf);
			errno = tmperrno;
			return(0);
		}
		bpb = (bpb_t *)buf;
		devp -> fd = fd;
	}
#endif	/* !MSDOS */

	total = byte2word(bpb -> total);
	if (!total) total = (long)byte2word(bpb -> bigtotal_l)
		+ ((long)byte2word(bpb -> bigtotal_h) << 16);
#if	!MSDOS
	if (byte2word(bpb -> secttrack) != nsect
	|| byte2word(bpb -> nhead) != devp -> ch_head
	|| total / ((devp -> ch_head) * nsect) != devp -> ch_cyl) {
		if (bpb != bpbcache) memcpy(bpbcache, bpb, sizeof(bpb_t));
		if (buf) free(buf);
		errno = tmperrno;
		return(0);
	}
#endif

	devp -> clustsize = bpb -> clustsize;
	devp -> sectsize = byte2word(bpb -> sectsize);
	devp -> fatofs = byte2word(bpb -> bootsect);
	devp -> fatsize = byte2word(bpb -> fatsize);
	devp -> dirofs = (devp -> fatofs) + (devp -> fatsize) * (bpb -> nfat);
	devp -> dirsize = (long)byte2word(bpb -> maxdir) * DOSDIRENT
		/ (long)(devp -> sectsize);
	total -= (long)(devp -> dirofs) + (long)(devp -> dirsize);
	devp -> totalsize = total / (devp -> clustsize);
	if (devp -> totalsize > MAX12BIT) devp -> flags |= F_16BIT;

	if (buf) free(buf);
	errno = tmperrno;
	return(1);
}

static int opendev(drive)
int drive;
{
#if	!MSDOS
	bpb_t bpb;
	char *cp;
	long size;
	int ret, tmperrno;
#endif
	devstat dev;
	int i, drv;

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
#if	MSDOS
		memset(&dev, 0, sizeof(devstat));
		dev.drive = drv;
		if (readbpb(&dev) < 0) return(-1);
#else
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
#endif
		dev.dircache = NULL;
#if	MSDOS
		dev.fatbuf = NULL;
#else
		tmperrno = errno;
		size = (long)(dev.fatsize) * (long)(dev.sectsize);
		if (size > MAXFATMEM) dev.fatbuf = NULL;
		else if ((dev.fatbuf = (char *)malloc(size))
		&& readfat(&dev) < 0) {
			doserrno = errno;
			free(dev.fatbuf);
			close(dev.fd);
			errno = tmperrno;
			return(-1);
		}
#endif
	}
	if (drive >= 'a' && drive <= 'z') dev.flags |= F_VFAT;
	else dev.flags &= ~F_VFAT;

	memcpy(&devlist[maxdev], &dev, sizeof(devstat));
	return(maxdev++);
}

static int closedev(dd)
int dd;
{
#if	!MSDOS
	int j;
#endif
	int i, ret, tmperrno;

	if (dd < 0 || dd >= maxdev || !devlist[dd].drive) {
		doserrno = EBADF;
		return(-1);
	}

	tmperrno = errno;
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
#if	!MSDOS
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
#endif
		if (writefat(&devlist[dd]) < 0) ret = -1;
		if (devlist[dd].fatbuf) free(devlist[dd].fatbuf);
#if	!MSDOS
		close(devlist[dd].fd);
#endif
	}

	devlist[dd].drive = '\0';
	while (maxdev > 0 && !devlist[maxdev - 1].drive) maxdev--;
	errno = tmperrno;
	return(ret);
}

int preparedrv(drive, func)
int drive;
VOID_T (*func)__P_((VOID_A));
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

static u_short cnvunicode(wc, encode)
u_short wc;
int encode;
{
	u_char buf[4];
	char path[MAXPATHLEN + 1];
	u_short r, w, min, max, ofs, total;
	int fd;

	if (!unitblpath || !*unitblpath) strcpy(path, UNICODETBL);
	else {
		strcpy(path, unitblpath);
		strcat(path, _SS_);
		strcat(path, UNICODETBL);
	}
	if ((fd = open(path, O_RDONLY | O_BINARY, 0600)) < 0) return(0);

	if (read(fd, buf, 2) != 2) return(0);
	total = (((u_short)(buf[1]) << 8) | buf[0]);

	r = 0;
	if (encode) for (ofs = 0; ofs < total; ofs++) {
		if (read(fd, buf, 4) != 4) break;
		w = (((u_short)(buf[3]) << 8) | buf[2]);
		if (wc == w) {
			r = (((u_short)(buf[1]) << 8) | buf[0]);
			break;
		}
	}
	else {
		min = 0;
		max = total + 1;
		ofs = total / 2 + 1;
		for (;;) {
			if (ofs == min || ofs == max) break;
			if (lseek(fd, (long)(ofs - 1) * 4 + 2, 0) < 0
			|| read(fd, buf, 4) != 4) break;
			w = (((u_short)(buf[1]) << 8) | buf[0]);
			if (wc == w) {
				r = (((u_short)(buf[3]) << 8) | buf[2]);
				break;
			}
			else if (wc < w) {
				max = ofs;
				ofs = (ofs + min) / 2;
			}
			else {
				min = ofs;
				ofs = (ofs + max) / 2;
			}
		}
	}

	close(fd);
	return(r);
}

static u_short lfnencode(c1, c2)
u_char c1, c2;
{
	u_short kanji, unicode;

	if (!c1) {
#if	MSDOS
		if (strchr(NOTINLFN, c2)) return(0xffff);
#else
		if (strchr(NOTINLFN, c2)) return('_');
#endif
		if (c2 < 0x80) return(c2);
		if (c2 > 0xa0 && c2 <= 0xdf) return(0xff00 | (c2 - 0x40));
	}
	else {
		kanji = ((u_short)c1 << 8) | c2;
		if (kanji < MINKANJI || kanji > MAXKANJI) return(0xff00);
		if ((unicode = cnvunicode(kanji, 1))) return(unicode);
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
	u_short kanji, unicode;

	switch (c1) {
		case 0:
			if (c2 < 0x80) return(c2);
			break;
		case 0xff:
			if (c2 > 0x20 && c2 <= 0x3a)
				return(c2 + 0x8260 - 0x21);
			if (c2 > 0x40 && c2 <= 0x5a)
				return(c2 + 0x8281 - 0x41);
			if (c2 > 0x60 && c2 <= 0x9f) return(c2 + 0x40);
			break;
		default:
			break;
	}
	unicode = ((u_short)c1 << 8) | c2;
	if (unicode < MINUNICODE || unicode > MAXUNICODE) return('?');
	if ((kanji = cnvunicode(unicode, 0))) return(kanji);
	return('?');
}

static int transchar(c)
int c;
{
#if	!MSDOS
	if (c == '+') return('`');
	if (c == ',') return('\'');
	if (c == '[') return('&');
	if (c == '.') return('$');
#endif
	if (strchr(NOTINLFN, c)) return(-2);
	if (strchr(NOTINALIAS, c)) return(-1);
	if (strchr(PACKINALIAS, c)) return(0);
	return(toupper2(c));
}

static int detranschar(c)
int c;
{
#if	!MSDOS
	if (c == '`') return('+');
	if (c == '\'') return(',');
	if (c == '&') return('[');
	if (c == '$') return('.');
#endif
	return(c);
}

static int cmpdospath(path1, path2, len, part)
char *path1, *path2;
int len, part;
{
	int i, c1, c2;

	if (len < 0) len = strlen(path1);
	for (i = 0; i < len; i++) {
		c1 = toupper2(path1[i]);
		c2 = toupper2(path2[i]);
		if (!c1 || !c2 || c1 != c2) return(c1 - c2);
	}
	if (!path2[i]) return(0);
	else if (part && path2[i] == _SC_) return(0);
	return(-path2[i]);
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
	char *cp, *eol, num[9];
	int i, j, c, cnv;

	if (isdotdir(file)) {
		strcpy(buf, file);
		return(buf);
	}

	for (i = strlen(file); i > 0; i--)
		if (!strchr(PACKINALIAS, file[i - 1])) break;
	if (i <= 0) return(NULL);
	eol = &(file[i]);
	for (cp = eol - 1; cp > file; cp--) if (*cp == '.') break;
	if (cp <= file) cp = NULL;

	cnv = 0;
	for (i = 0; i < 8; i++) {
		if (file == cp || file == eol || !*file) buf[i] = ' ';
		else if (issjis1((u_char)(*file))) {
			buf[i++] = *(file++);
			buf[i] = *(file++);
		}
		else if ((c = transchar(*(file++))) > 0) buf[i] = c;
		else {
			cnv = 1;
#if	MSDOS
			if (c < -1) return(NULL);
#endif
			if (!c) i--;
			else buf[i] = '_';
		}
	}

	if (file != cp && file < eol && *file) cnv = 1;
	if (cp) cp++;
	for (i = 8; i < 11; i++) {
		if (!cp || cp == eol || !*cp) buf[i] = ' ';
		else if (issjis1((u_char)(*cp))) {
			buf[i++] = *(cp++);
			buf[i] = *(cp++);
		}
		else if ((c = transchar(*(cp++))) > 0) buf[i] = c;
		else {
			cnv = 1;
#if	MSDOS
			if (c < -1) return(NULL);
#endif
			if (!c) i--;
			else buf[i] = '_';
		}
	}
	if (cp && cp < eol && *cp) cnv = 1;
	buf[i] = '\0';

	if (vol > 1 || (vol > 0 && cnv)) {
		sprintf(num, "%d", vol);
		for (i = 7 - (int)strlen(num); i > 0; i--)
			if (buf[i - 1] != ' ') break;
		buf[i++] = '~';
		for (j = 0; num[j]; j++) buf[i++] = num[j];
	}
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
#if	MSDOS
	struct timeb buffer;
#else
	struct timeval t_val;
	struct timezone tz;
#endif
	struct tm *tm;
	time_t tmp;
	int d, t;

	if (clock >= 0) tmp = clock;
	else {
#if	MSDOS
		ftime(&buffer);
		tmp = (time_t)(buffer.time);
#else
		gettimeofday(&t_val, &tz);
		tmp = (time_t)(t_val.tv_sec);
#endif
	}
	tm = localtime(&tmp);
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
	int drive;

	drive = *(path++);
	if (!isalpha(drive) || *(path++) != ':') {
		doserrno = ENOENT;
		return(-1);
	}

#if	!MSDOS
	if (lastdrive < 0) {
		int i;

		for (i = 0; i < 'Z' - 'A' + 1; i++) {
			curdir[i] = NULL;
			dentcache[i] = NULL;
		}
		lastdrive = drive;
	}
#endif

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
#if	MSDOS
	if (*path != '/' && *path != '\\') {
		*buf = _SC_;
		getcurdir(buf + 1, i + 1);
#else
	if (*path != '/' && *path != '\\' && curdir[i]) {
		*buf = _SC_;
		strcpy(buf + 1, curdir[i]);
#endif
		cp += strlen(buf);
	}

	while (*path) {
		for (i = 0; path[i] && path[i] != '/' && path[i] != '\\'; i++)
			if (issjis1((u_char)(path[i]))) i++;
		if (class && !path[i]) {
			if ((i == 1 && *path == '.')
			|| (i == 2 && *path == '.' && *(path + 1) == '.')) {
				*(cp++) = _SC_;
				strncpy(cp, path, i);
				cp += i;
			}
			else if (cp + i + 1 - buf >= DOSMAXPATHLEN) {
				doserrno = ENAMETOOLONG;
				return(-1);
			}
			else {
				*(cp++) = _SC_;
				strncpy(cp, path, i);
				cp += i;
				if (path[i - 1] == '.') cp--;
			}
		}
		else if (!i || (i == 1 && *path == '.'));
		else if (i == 2 && *path == '.' && *(path + 1) == '.') {
			for (cp--; cp > buf; cp--) if (*cp == _SC_) break;
			if (cp < buf) cp = buf;
		}
		else {
			if (cp + i + 1 - buf >= DOSMAXPATHLEN) {
				doserrno = ENAMETOOLONG;
				return(-1);
			}
			*(cp++) = _SC_;
			strncpy(cp, path, i);
			cp += i;
			if (path[i - 1] == '.') cp--;
		}
		if (*(path += i)) path++;
	}
	if (cp == buf) *(cp++) = _SC_;
	else if (cp - 1 > buf && *(cp - 1) == _SC_) cp--;
	*cp = '\0';

	return(drive);
}

static dosDIR *_dosopendir(path, resolved, needlfn)
char *path, *resolved;
int needlfn;
{
	dosDIR *xdirp;
	struct dirent *dp;
	dent_t *dentp;
	cache_t *cache;
	char *cp, *cachepath, *tmp, buf[DOSMAXPATHLEN + 3 + 1];
	int len, dd, drive, rlen, tmperrno;

	tmperrno = errno;
	if ((drive = parsepath(buf, path, 0)) < 0
	|| (dd = opendev(drive)) < 0) return(NULL);

	path = buf + 1;
	cache = NULL;
	if (!devlist[dd].dircache) cp = NULL;
	else {
		cache = devlist[dd].dircache;
		cp = cache -> path;
	}
	if (!(devlist[dd].dircache = (cache_t *)malloc(sizeof(cache_t)))) {
		closedev(dd);
		doserrno = errno;
		errno = tmperrno;
		return(NULL);
	}
	devlist[dd].flags |= F_CACHE;
	cachepath = dd2path(dd);

	if (!(xdirp = (dosDIR *)malloc(sizeof(dosDIR)))) {
		closedev(dd);
		doserrno = errno;
		errno = tmperrno;
		return(NULL);
	}
	xdirp -> dd_id = -1;
	xdirp -> dd_fd = dd;
	xdirp -> dd_top =
	xdirp -> dd_off = 0;
	xdirp -> dd_loc = 0;
	xdirp -> dd_size = (long)(devlist[dd].clustsize)
		* (long)(devlist[dd].sectsize);
	if (!(xdirp -> dd_buf = (char *)malloc(xdirp -> dd_size))) {
		_dosclosedir(xdirp);
		doserrno = errno;
		errno = tmperrno;
		return(NULL);
	}

	if (resolved) {
		resolved[0] = drive;
		resolved[1] = ':';
		resolved[2] = '\0';
		rlen = 2;
	}

	if (cp && (len = strlen(cp)) > 0) {
		if (cp[len - 1] == _SC_) len--;
		if (!cmpdospath(cp, path, len, 1)) {
			xdirp -> dd_top =
			xdirp -> dd_off = byte2word(cache -> dent.clust);
			memcpy(devlist[dd].dircache, cache, sizeof(cache_t));
			cachepath += len;
			*(cachepath++) = _SC_;
			path += len;
			if (*path) path++;
			if (resolved) {
				resolved[rlen++] = _SC_;
				strncpy(resolved + rlen, cp, len);
				rlen += len;
				resolved[rlen] = '\0';
			}
		}
	}
	*cachepath = '\0';

	while (*path) {
		if (!(cp = strchr(path, _SC_))) cp = path + strlen(path);
		len = cp - path;
		tmp = path;
		path = (*cp) ? cp + 1 : cp;
		if (!len || (len == 1 && *tmp == '.')) continue;

		if (!(dp = finddir(xdirp, tmp, len, needlfn))) {
			errno = (doserrno) ? doserrno : ENOENT;
			_dosclosedir(xdirp);
			doserrno = errno;
			errno = tmperrno;
			return(NULL);
		}
		if (resolved) {
			resolved[rlen++] = _SC_;
			if (rlen + strlen(dp -> d_name) >= DOSMAXPATHLEN) {
				_dosclosedir(xdirp);
				doserrno = ENAMETOOLONG;
				errno = tmperrno;
				return(NULL);
			}
			if (needlfn) strcpy(resolved + rlen, dp -> d_name);
			else getdosname(resolved + rlen,
				dd2dentp(xdirp -> dd_fd) -> name,
				dd2dentp(xdirp -> dd_fd) -> ext);
			while (resolved[rlen]) rlen++;
		}

		dentp = (dent_t *)&(xdirp -> dd_buf[dp -> d_fileno]);
		xdirp -> dd_top =
		xdirp -> dd_off = byte2word(dentp -> clust);
		xdirp -> dd_loc = 0;
		cachepath += len;
		*(cachepath++) = _SC_;
		*cachepath = '\0';
	}

	if (*(dd2path(dd)) && !(dd2dentp(dd) -> attr & DS_IFDIR)) {
		_dosclosedir(xdirp);
		doserrno = ENOTDIR;
		errno = tmperrno;
		return(NULL);
	}
	errno = tmperrno;
	return(xdirp);
}

static int _dosclosedir(xdirp)
dosDIR *xdirp;
{
	int ret, tmperrno;

	tmperrno = errno;
	if (xdirp -> dd_id != -1) return(0);
	if (xdirp -> dd_buf) free(xdirp -> dd_buf);
	ret = closedev(xdirp -> dd_fd);
	free(xdirp);
	errno = tmperrno;
	return(ret);
}

DIR *dosopendir(path)
char *path;
{
	dosDIR *xdirp;

	if (!(xdirp = _dosopendir(path, NULL, 0))) reterr(NULL);
	return((DIR *)xdirp);
}

int dosclosedir(dirp)
DIR *dirp;
{
	if (_dosclosedir((dosDIR *)dirp) < 0) reterr(-1);
	return(0);
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
	clust = -1;
	offset = 0;
	sum = -1;
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
		if (cnt > 0 && cnt == *cp + 1 && sum == dentp -> dummy[1])
			cnt--;
		else {
			dp -> d_name[0] = '\0';
			dp -> d_reclen = DOSDIRENT;
			if ((cnt = *cp - '@') < 0) continue;
			lfn_clust = dd2clust(xdirp -> dd_fd);
			lfn_offset = dd2offset(xdirp -> dd_fd);
			sum = dentp -> dummy[1];
		}

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
	if ((cp = strrchr(dd2path(xdirp -> dd_fd), _SC_))) cp++;
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

static struct dirent *finddir(xdirp, fname, len, needlfn)
dosDIR *xdirp;
char *fname;
int len, needlfn;
{
	struct dirent *dp;
	char tmp[8 + 1 + 3 + 1], dosname[8 + 3 + 1];

	if (len <= 0) len = strlen(fname);
	if (devlist[xdirp -> dd_fd].flags & F_VFAT) {
		if (needlfn) while ((dp = _dosreaddir(xdirp, 1))) {
			getdosname(tmp, dd2dentp(xdirp -> dd_fd) -> name,
				dd2dentp(xdirp -> dd_fd) -> ext);
			if (!cmpdospath(fname, tmp, len, 0)
			|| !cmpdospath(fname, dp -> d_name, len, 0))
				return(dp);
		}
		else while ((dp = _dosreaddir(xdirp, 1)))
			if (!cmpdospath(fname, dp -> d_name, len, 0))
				return(dp);
	}
	else {
		if (!putdosname(dosname, fname, 0)) {
			doserrno = ENOENT;
			return(NULL);
		}
		getdosname(tmp, dosname, dosname + 8);
		while ((dp = _dosreaddir(xdirp, 1)))
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

#if	!MSDOS
int doschdir(path)
char *path;
{
	dosDIR *xdirp;
	char *tmp, buf[DOSMAXPATHLEN + 3 + 1];
	int drv, drive, needlfn;

	if ((drive = getdrive(path)) < 0) reterr(-1);
	drv = toupper2(drive);

	needlfn = (drive >= 'a' && drive <= 'z');
	if (!(xdirp = _dosopendir(path, buf, needlfn))) {
		if (path[2] == '/' || path[2] == '\\'
		|| !(tmp = curdir[drv - 'A'])) reterr(-1);
		curdir[drv - 'A'] = NULL;
		xdirp = _dosopendir(path, buf, needlfn);
		curdir[drv - 'A'] = tmp;
		if (!xdirp) reterr(-1);
	}
	_dosclosedir(xdirp);

	if (!buf[2]) buf[3] = '\0';
	if (!(tmp = (char *)malloc(strlen(buf + 3) + 1))) return(-1);
	if (curdir[drv - 'A']) free(curdir[drv - 'A']);
	curdir[drv - 'A'] = tmp;
	strcpy(curdir[drv - 'A'], buf + 3);
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
	pathname[2] = _SC_;
	strcpy(pathname + 3, curdir[i]);
	return(pathname);
}
#endif	/* !MSDOS */

static dosDIR *splitpath(pathp, resolved, needlfn)
char **pathp, *resolved;
int needlfn;
{
	char dir[MAXPATHLEN + 1];
	int i, j;

	strcpy(dir, *pathp);
	for (i = 1, j = 2; dir[j]; j++) {
		if (issjis1((u_char)(dir[j]))) j++;
		else if (dir[j] == '/' || dir[j] == '\\') i = j;
	}

	*pathp += i + 1;
	if (i < 3) dir[i++] = _SC_;
	dir[i] = '\0';

	return(_dosopendir(dir, resolved, needlfn));
}

static int getdent(path, ddp)
char *path;
int *ddp;
{
	dosDIR *xdirp;
	char buf[2 + DOSMAXPATHLEN + 3 + 1];
	int dd, drive, tmperrno;

	if (ddp) *ddp = -1;
	if ((drive = parsepath(buf + 2, path, 1)) < 0
	|| (dd = opendev(drive)) < 0) return(-1);
	if (devlist[dd].dircache
	&& buf[2] && buf[3] && !cmpdospath(dd2path(dd), buf + 3, -1, 0))
		return(dd);
	buf[0] = drive;
	buf[1] = ':';

	tmperrno = errno;
	path = buf;
	if (!(xdirp = splitpath(&path, NULL, 0))) {
		errno = (doserrno) ? doserrno : ENOTDIR;
		closedev(dd);
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}
	if (!(devlist[dd].dircache = (cache_t *)malloc(sizeof(cache_t)))) {
		_dosclosedir(xdirp);
		closedev(dd);
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}
	devlist[dd].flags |= F_CACHE;

	memcpy(devlist[dd].dircache, devlist[xdirp -> dd_fd].dircache,
		sizeof(cache_t));
	if (!*path) {
		_dosclosedir(xdirp);
		dd2dentp(dd) -> attr |= DS_IFDIR;
		errno = tmperrno;
		return(dd);
	}
	if (!finddir(xdirp, path, 0, 0)) {
		errno = (doserrno) ? doserrno : ENOENT;
		_dosclosedir(xdirp);
		if (ddp && !doserrno) *ddp = dd;
		else closedev(dd);
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}

	memcpy(devlist[dd].dircache, devlist[xdirp -> dd_fd].dircache,
		sizeof(cache_t));
	_dosclosedir(xdirp);
	errno = tmperrno;
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
	char *file, tmp[8 + 1 + 3 + 1], fname[8 + 3 + 1];
	char buf[2 + DOSMAXPATHLEN + 3 + 1];
	long clust;
	u_short c, offset;
	int i, j, n, len, cnt, sum, ret;

	if ((i = parsepath(buf + 2, path, 1)) < 0) return(-1);
	buf[0] = i;
	buf[1] = ':';
	file = buf;
	if (!(xdirp = splitpath(&file, NULL, 0))) {
		if (doserrno == ENOENT) doserrno = ENOTDIR;
		return(-1);
	}
	if (!*file) {
		_dosclosedir(xdirp);
		doserrno = EEXIST;
		return(-1);
	}
	if (!(devlist[xdirp -> dd_fd].flags & F_VFAT)) {
		n = 0;
		cnt = 1;
		len = 0;
	}
	else {
		n = -1;
		for (i = j = 0; file[i]; i++, j += 2) {
			if (!strchr(PACKINALIAS, file[i])) n = -1;
			else if (n < 0) n = i;
			if (!issjis1((u_char)(file[i])))
				c = lfnencode(0, file[i]);
			else {
				c = lfnencode(file[i], file[i + 1]);
				i++;
			}
			longfname[j] = c & 0xff;
			longfname[j + 1] = (c >> 8) & 0xff;
		}
		if (n >= 0) j = n * 2;
		n = 1;
		cnt = j / (LFNENTSIZ * 2) + 2;
		longfname[j] = longfname[j + 1] = '\0';
		len = j + 2;
	}

	sum = -1;
	dentp = dd2dentp(xdirp -> dd_fd);
	if (!putdosname(fname, file, n)) {
		_dosclosedir(xdirp);
		doserrno = EACCES;
		return(-1);
	}
	if (!n || !strncmp(getdosname(tmp, fname, fname + 8), file, 8 + 3))
		cnt = 1;
	else sum = checksum(fname);

	i = j = 0;
	clust = -1;
	offset = 0;
	for (;;) {
		if (readdent(xdirp, dentp, 1) < 0) {
			if (clust >= 0) break;
			if (doserrno || expanddent(xdirp -> dd_fd) < 0) {
				_dosclosedir(xdirp);
				return(-1);
			}
			clust = dd2clust(xdirp -> dd_fd);
			offset = 0;
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
				_dosclosedir(xdirp);
				doserrno = EEXIST;
				return(-1);
			}
			n++;
			if (!putdosname(fname, file, n)) {
				_dosclosedir(xdirp);
				doserrno = EACCES;
				return(-1);
			}
			sum = checksum(fname);
			i = j = 0;
			clust = -1;
			dosrewinddir((DIR *)xdirp);
		}
	}

	if (clust >= 0 && seekdent(xdirp, NULL, clust, offset) < 0) {
		_dosclosedir(xdirp);
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
			_dosclosedir(xdirp);
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
	xdir.dd_size = (long)(devlist[dd].clustsize)
		* (long)(devlist[dd].sectsize);
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

#if	MSDOS
char *dosshortname(path, alias)
char *path, *alias;
{
	dosDIR *xdirp;
	char buf[2 + DOSMAXPATHLEN + 3 + 1], name[8 + 1 + 3 + 1];
	int dd, drive;

	name[0] = '\0';
	if ((drive = parsepath(buf + 2, path, 1)) < 0
	|| (dd = opendev(drive)) < 0) reterr(NULL);
	if (devlist[dd].dircache
	&& buf[2] && buf[3] && !cmpdospath(dd2path(dd), buf + 3, -1, 0))
		getdosname(name, dd2dentp(dd) -> name, dd2dentp(dd) -> ext);
	buf[0] = drive;
	buf[1] = ':';
	path = buf;

	*alias = '\0';
	if (!(xdirp = splitpath(&path, alias, 0))) {
		errno = (doserrno) ? doserrno : ENOTDIR;
		closedev(dd);
		return(NULL);
	}
	strcat(alias, _SS_);
	if (!*path);
	else if (*name) strcpy(alias + strlen(alias), name);
	else if (!strcmp(path, ".") || !strcmp(path, ".."))
		strcat(alias, path);
	else if (finddir(xdirp, path, 0, 0))
		getdosname(alias + strlen(alias),
			dd2dentp(xdirp -> dd_fd) -> name,
			dd2dentp(xdirp -> dd_fd) -> ext);
	else {
		errno = (doserrno) ? doserrno : ENOENT;
		alias = NULL;
	}
	_dosclosedir(xdirp);
	closedev(dd);
	return(alias);
}

char *doslongname(path, resolved)
char *path, *resolved;
{
	dosDIR *xdirp;
	struct dirent *dp;
	char buf[2 + DOSMAXPATHLEN + 3 + 1];
	int dd, drive;

	if ((drive = parsepath(buf + 2, path, 1)) < 0
	|| (dd = opendev(drive)) < 0) reterr(NULL);
	buf[0] = drive;
	buf[1] = ':';
	path = buf;

	*resolved = '\0';
	if (!(xdirp = splitpath(&path, resolved, 1))) {
		errno = (doserrno) ? doserrno : ENOTDIR;
		closedev(dd);
		return(NULL);
	}
	strcat(resolved, _SS_);
	if (!*path);
	else if ((dp = finddir(xdirp, path, 0, 1)))
		strcat(resolved + strlen(resolved), dp -> d_name);
	else {
		errno = (doserrno) ? doserrno : ENOENT;
		resolved = NULL;
	}
	_dosclosedir(xdirp);
	closedev(dd);
	return(resolved);
}
#else	/* MSDOS */
int dosstatfs(drive, buf)
int drive;
char *buf;
{
	long n, clust, block, total, avail;
	int dd;

	if ((dd = opendev(drive)) < 0) reterr(-1);
	block = (long)(devlist[dd].clustsize) * (long)(devlist[dd].sectsize);
	total = (long)(devlist[dd].totalsize);
	avail = 0;
	for (clust = 2; clust < total; clust++) {
		if ((n = getfatent(&devlist[dd], clust)) < 0) break;
		if (!n) avail++;
	}

	*((long *)(&buf[sizeof(long) * 0])) = block;
	*((long *)(&buf[sizeof(long) * 1])) = total;
	*((long *)(&buf[sizeof(long) * 2])) = avail;
	return(0);
}
#endif	/* MSDOS */

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
	buf -> st_size = (long)byte2word(dd2dentp(dd) -> size_l)
		+ ((long)byte2word(dd2dentp(dd) -> size_h) << 16);
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

#if	!MSDOS
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
#endif	/* !MSDOS */

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
	if (buf[0] && buf[1] && !cmpdospath(dd2path(dd), buf + 1, i, 1)) {
		closedev(dd);
		errno = EINVAL;
		return(-1);
	}
	if ((fd = dosopen(to, O_RDWR | O_CREAT | O_EXCL, 0666)) < 0) {
		closedev(dd);
		return(-1);
	}
	fd -= DOSFDOFFSET;
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

	dosflist[fd]._base = (char *)malloc((long)(devlist[dd].clustsize)
			* (long)(devlist[dd].sectsize));
	if (!dosflist[fd]._base) {
		closedev(dd);
		return(-1);
	}

	dosflist[fd]._cnt = 0;
	dosflist[fd]._ptr = dosflist[fd]._base;
	dosflist[fd]._bufsize =
		(long)(devlist[dd].clustsize) * (long)(devlist[dd].sectsize);
	dosflist[fd]._flag = flags;
	dosflist[fd]._file = dd;
	dosflist[fd]._top =
	dosflist[fd]._next = byte2word(dd2dentp(dd) -> clust);
	dosflist[fd]._off = 0;
	dosflist[fd]._loc = 0;
	dosflist[fd]._size = (long)byte2word(dd2dentp(dd) -> size_l)
		+ (long)(byte2word(dd2dentp(dd) -> size_h) << 16);

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
	if ((dosflist[fd]._off = dosflist[fd]._next))
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

	if (doswrite(fd + DOSFDOFFSET, (char *)dent, sizeof(dent_t) * 2) < 0) {
		tmp = errno;
		if ((clust = byte2word(dosflist[fd]._dent.clust)))
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

	if (!(xdirp = _dosopendir(path, NULL, 0))) reterr(-1);
	memcpy(&cache, devlist[xdirp -> dd_fd].dircache, sizeof(cache_t));

	clust = lfn_clust;
	offset = lfn_offset;

	while ((dp = _dosreaddir(xdirp, 0)))
		if (strcmp(dp -> d_name, ".") && strcmp(dp -> d_name, "..")) {
			_dosclosedir(xdirp);
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
	_dosclosedir(xdirp);
	return(ret);
}

#if	!MSDOS
int dosfileno(stream)
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

	if ((fd = dosfileno(stream)) < 0) {
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

	if ((fd = dosfileno(stream)) < 0) {
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
	ret = dosread(fd + DOSFDOFFSET, buf, (long)size * (long)nitems);

	if (ret < 0) {
		dosflist[fd]._flag |= O_IOERR;
		return(0);
	}
	if (ret < (long)size * (long)nitems) dosflist[fd]._flag |= O_IOEOF;
	return(ret);
}

int dosfwrite(buf, size, nitems, stream)
char *buf;
int size, nitems;
FILE *stream;
{
	long nbytes, total;
	int fd, rest;

	if ((fd = dosfileno(stream)) < 0) {
		errno = EINVAL;
		return(0);
	}
	if (!(dosflist[fd]._flag & (O_WRONLY | O_RDWR))) {
		errno = EBADF;
		dosflist[fd]._flag |= O_IOERR;
		return(0);
	}
	total = 0;
	nbytes = (long)size * (long)nitems;

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

	if ((fd = dosfileno(stream)) < 0) {
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

int dosallclose(VOID_A)
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
#endif	/* !MSDOS */
#endif	/* !_NODOSDRIVE */
