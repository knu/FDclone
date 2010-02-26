/*
 *	dosdisk.c
 *
 *	MSDOS disk accessing module
 */

#define	DEP_UNICODE
#include "headers.h"
#include "depend.h"
#include "kctype.h"
#include "string.h"
#include "time.h"
#include "sysemu.h"
#include "pathname.h"
#include "dosdisk.h"
#include "unixdisk.h"
#include "kconv.h"

#if	MSDOS
#include <io.h>
#include <sys/timeb.h>
#endif
#ifdef	LINUX
#include <mntent.h>
#include <sys/mount.h>
#include <linux/unistd.h>
# ifndef	BLKFLSBUF
# include <linux/fs.h>
# endif
# if	defined (USELLSEEK) && !defined (_syscall5)
# include <sys/syscall.h>
# undef	_syscall5
# endif
#endif	/* LINUX */

#ifdef	HDDMOUNT
#include <sys/ioctl.h>
# ifdef	BSD4
#  ifdef	NETBSD
#  define	OMIT_FSTYPENUMS		/* For NetBSD >=3.1 */
#  endif
# include <sys/disklabel.h>
# else	/* !BSD4 */
#  ifdef	SOLARIS
#  include <sys/dkio.h>
#  include <sys/vtoc.h>
#  endif
#  ifdef	LINUX
#  include <linux/hdreg.h>
#  endif
# endif	/* !BSD4 */
#endif	/* HDDMOUNT */

#ifdef	SOLARIS
#define	DIOCGDINFO		DKIOCGGEOM
#define	disklabel		dk_geom
#define	d_ntracks		dkg_nhead
#define	d_nsectors		dkg_nsect
#endif
#ifdef	LINUX
#define	D_SECSIZE(dl)		512
#define	DIOCGDINFO		HDIO_GETGEO
#define	disklabel		hd_geometry
#define	d_ntracks		heads
#define	d_nsectors		sectors
#else
#define	D_SECSIZE(dl)		(dl).d_secsize
#endif

#ifndef	MOUNTED
#define	MOUNTED			"/etc/mtab"
#endif

#if	defined (USELLSEEK) && !defined (_syscall5)
#define	_llseek(f,h,l,r,w)	syscall(SYS__llseek, f, h, l, r, w)
#endif

#define	KC_SJIS1		0001
#define	KC_SJIS2		0002
#define	KC_EUCJP		0010

#define	EXTCOM			"COM"
#define	EXTEXE			"EXE"
#define	EXTBAT			"BAT"

#define	strsizecmp(s1, s2)	strncmp(s1, s2, sizeof(s1))
#define	ENDCLUST		((long)(0x0fffffff))
#define	ERRCLUST		((long)(0x0ffffff7))
#define	ROOTCLUST		((long)(0x10000000))

#ifndef	_NODOSDRIVE

#if	MSDOS && defined (FD)
extern int _dospath __P_((CONST char *));
#define	__dospath		_dospath
#else
#define	__dospath(p)		((Xisalpha(*(p)) && (p)[1] == ':') ? *(p) : 0)
#endif

#ifdef	FD
extern int newdup __P_((int));
#else
#define	newdup(n)		(n)
#endif

#ifdef	USELLSEEK
static l_off_t NEAR Xllseek __P_((int, l_off_t, int));
#else
#define	Xllseek	lseek
#endif
#if	!MSDOS
static int NEAR sectseek __P_((CONST devstat *, u_long));
#endif
static int NEAR realread __P_((CONST devstat *, u_long, u_char *, int));
static int NEAR realwrite __P_((CONST devstat *, u_long, CONST u_char *, int));
static int NEAR killcache __P_((CONST devstat *, int, int));
static int NEAR flushcache __P_((CONST devstat *));
static int NEAR shiftcache __P_((CONST devstat *, int, int));
static int NEAR cachecpy __P_((int, int, int, int));
static int NEAR uniqcache __P_((CONST devstat *, int, u_long, u_long));
static int NEAR appendcache __P_((int, CONST devstat *,
		u_long, CONST u_char *, int, int));
static int NEAR insertcache __P_((int, CONST devstat *,
		u_long, CONST u_char *, int, int));
static int NEAR overwritecache __P_((int, CONST devstat *,
		u_long, CONST u_char *, int, int));
static int NEAR savecache __P_((CONST devstat *,
		u_long, CONST u_char *, int, int));
static int NEAR loadcache __P_((CONST devstat *, u_long, u_char *, int));
static long NEAR sectread __P_((CONST devstat *, u_long, u_char *, int));
static long NEAR sectwrite __P_((CONST devstat *,
		u_long, CONST u_char *, int));
static int NEAR availfat __P_((CONST devstat *));
static int NEAR readfat __P_((devstat *));
static int NEAR writefat __P_((CONST devstat *));
static long NEAR getfatofs __P_((CONST devstat *, long));
static u_char *NEAR readtmpfat __P_((CONST devstat *, long, int));
static long NEAR getfatent __P_((CONST devstat *, long));
static int NEAR putfatent __P_((devstat *, long, long));
static u_long NEAR clust2sect __P_((CONST devstat *, long));
static long NEAR clust32 __P_((CONST devstat *, CONST dent_t *));
static long NEAR newclust __P_((CONST devstat *));
static long NEAR clustread __P_((CONST devstat *, u_char *, long));
static long NEAR clustwrite __P_((devstat *, CONST u_char *, long));
static long NEAR clustexpand __P_((devstat *, long, int));
static int NEAR clustfree __P_((devstat *, long));
#if	!MSDOS
static int NEAR _readbpb __P_((devstat *, CONST bpb_t *));
#endif
static int NEAR readbpb __P_((devstat *, int));
#ifdef	HDDMOUNT
static l_off_t *NEAR _readpt __P_((l_off_t, l_off_t, int, int, int, int));
#endif
static int NEAR _dosopendev __P_((int));
static int NEAR _dosclosedev __P_((int));
static int NEAR calcsum __P_((CONST u_char *));
static u_int NEAR lfnencode __P_((u_int, u_int));
static u_int NEAR lfndecode __P_((u_int, u_int));
#if	!MSDOS
static int NEAR transchar __P_((int));
static int NEAR detranschar __P_((int));
#endif
static int NEAR sfntranschar __P_((int));
static int NEAR cmpdospath __P_((CONST char *, CONST char *, int, int));
static char *NEAR getdosname __P_((char *, CONST u_char *, CONST u_char *));
static u_char *NEAR putdosname __P_((u_char *, CONST char *, int));
static u_int NEAR getdosmode __P_((u_int));
static u_int NEAR putdosmode __P_((u_int));
static time_t NEAR getdostime __P_((u_int, u_int));
static int NEAR putdostime __P_((u_char *, time_t));
static int NEAR getdrive __P_((CONST char *));
static char *NEAR addpath __P_((char *, int, CONST char *, int));
static int NEAR parsepath __P_((char *, CONST char *, int));
static int NEAR seekdent __P_((dosDIR *, dent_t *, long, long));
static int NEAR readdent __P_((dosDIR *, dent_t *, int));
static dosDIR *NEAR _dosopendir __P_((CONST char *, char *, int));
static int NEAR _dosclosedir __P_((dosDIR *));
static struct dosdirent *NEAR _dosreaddir __P_((dosDIR *, int));
static struct dosdirent *NEAR finddirent __P_((dosDIR *,
		CONST char *, int, int));
static dosDIR *NEAR splitpath __P_((CONST char **, char *, int));
static int NEAR getdent __P_((CONST char *, int *));
static int NEAR writedent __P_((int));
static int NEAR expanddent __P_((int));
static int NEAR creatdent __P_((CONST char *, int));
static int NEAR unlinklfn __P_((int, long, long, int));
static long NEAR dosfilbuf __P_((int, int));
static long NEAR dosflsbuf __P_((int));

#if	MSDOS
int dependdosfunc = 0;
#else	/* !MSDOS */
int maxfdtype = 0;
#if	defined (DEP_DYNAMICLIST) || !defined (_NOCUSTOMIZE)
int origmaxfdtype = 0;
#endif
#ifdef	DEP_DYNAMICLIST
fdtype_t fdtype = NULL;
origfdtype_t origfdtype
#else	/* !DEP_DYNAMICLIST */
# ifndef	_NOCUSTOMIZE
origfdtype_t origfdtype = NULL;
# endif
fdtype_t fdtype
#endif	/* !DEP_DYNAMICLIST */
= {
#if	defined (SOLARIS) || defined (SUN_OS)
	{'A', "/dev/rfd0c", 2, 18, 80},
	{'A', "/dev/rfd0c", 2, 9, 80},
	{'A', "/dev/rfd0c", 2, 8 + 100, 80},
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
#if	defined (UXPDS)
	{'A', "/dev/fpd0", 2, 18, 80},
	{'A', "/dev/fpd0", 2, 9, 80},
	{'A', "/dev/fpd0", 2, 8 + 100, 80},
#endif
#if	defined (CYGWIN)
	{'A', "/dev/fd0", 2, 18, 80},
	{'A', "/dev/fd0", 2, 9, 80},
	{'A', "/dev/fd0", 2, 8 + 100, 80},
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
# if	__FreeBSD__ < 5
	{'A', "/dev/rfd0.1440", 2, 18, 80},
	{'A', "/dev/rfd0.720", 2, 9, 80},
	{'A', "/dev/rfd0.720", 2, 8 + 100, 80},
# else
	/* FreeBSD >=5.0 has not any buffered device */
	{'A', "/dev/fd0.1440", 2, 18, 80},
	{'A', "/dev/fd0.720", 2, 9, 80},
	{'A', "/dev/fd0.720", 2, 8 + 100, 80},
# endif
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
#if	defined (OPENBSD)
	{'A', "/dev/rfd0Bc", 2, 18, 80},
	{'A', "/dev/rfd0Fc", 2, 9, 80},
	{'A', "/dev/rfd0Fc", 2, 8 + 100, 80},
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
#if	0
devinfo CONST mediadescr[] = {
	{0xff, "2D-8SECT", 2, 8, 40},
	{0xfe, "1D-8SECT", 1, 8, 40},
	{0xfd, "2D-9SECT", 2, 9, 40},
	{0xfc, "1D-9SECT", 1, 9, 40},
	{0xfb, "2DD-8SECT", 2, 8 + 100, 80},
	{0xf9, "2DD-9SECT", 2, 9, 80},
	{0xf9, "2HD-15SECT", 2, 15, 80},
	{0xf8, "HDD", 0, 0, 0},
	{0xf0, "2HD-18SECT", 2, 18, 80},
	{0x02, "2HD-15SECT", 2, 15, 80},
	{0x01, "2HD-8SECT", 2, 8, 77},
	{0x00, NULL, 0, 0, 0}
};
#endif
#endif	/* !MSDOS */
int lastdrive = -1;
int needbavail = 0;
VOID_T (*doswaitfunc)__P_((VOID_A)) = NULL;
int (*dosintrfunc)__P_((VOID_A)) = NULL;

static char *curdir['Z' - 'A' + 1];
static devstat devlist[DOSNOFILE];
static int maxdev = 0;
static int devorder[DOSNOFILE];
static int maxorder = 0;
static dosFILE dosflist[DOSNOFILE];
static int maxdosf = 0;
static u_char cachedrive[SECTCACHESIZE];
static u_long sectno[SECTCACHESIZE];
static u_char *sectcache[SECTCACHESIZE];
static u_char cachesize[SECTCACHESIZE];
static u_char cachewrite[SECTCACHESIZE];
static int maxsectcache = 0;
static long maxcachemem = 0L;
static long lfn_clust = -1L;
static long lfn_offset = 0L;
static int doserrno = 0;
#if	!MSDOS
static CONST short sectsizelist[] = SECTSIZE;
#define	SLISTSIZ		arraysize(sectsizelist)
#endif
static CONST char *inhibitname[] = INHIBITNAME;
#define	INHIBITNAMESIZ		arraysize(inhibitname)


#ifdef	USELLSEEK
# ifdef	_syscall5
# undef	_llseek
static _syscall5(int, _llseek,
	u_int, fd,
	u_long, ofs_h,
	u_long, ofs_l,
	l_off_t *, result,
	u_int, whence);
# endif

static l_off_t NEAR Xllseek(fd, offset, whence)
int fd;
l_off_t offset;
int whence;
{
	l_off_t result;
	u_long ofs_h, ofs_l;

	ofs_h = (u_long)(offset >> 32);
	ofs_l = (u_long)(offset & (u_long)0xffffffff);
	if (_llseek(fd, ofs_h, ofs_l, &result, whence) < 0)
		return((l_off_t)-1);

	return(result);
}
#endif	/* USELLSEEK */

#if	!MSDOS
static int NEAR sectseek(devp, sect)
CONST devstat *devp;
u_long sect;
{
	l_off_t offset;
	int duperrno;

	duperrno = errno;
	if (devp -> flags & F_8SECT) sect += sect / 8;
	offset = (l_off_t)sect * (l_off_t)(devp -> sectsize);
# ifdef	HDDMOUNT
	offset += devp -> offset;
# endif
	if (Xllseek(devp -> fd, offset, L_SET) < (l_off_t)0) {
		doserrno = errno;
		errno = duperrno;
		return(-1);
	}
	errno = duperrno;

	return(0);
}
#endif	/* !MSDOS */

static int NEAR realread(devp, sect, buf, n)
CONST devstat *devp;
u_long sect;
u_char *buf;
int n;
{
	int duperrno;

	duperrno = errno;
#if	MSDOS
	n = rawdiskio(devp -> drive, sect, buf, n, devp -> sectsize, 0);
	if (n < 0) {
		doserrno = errno;
		return(-1);
	}
#else
	if (sectseek(devp, sect) < 0) return(-1);
	while (read(devp -> fd, buf, (long)n * devp -> sectsize) < 0) {
		if (errno == EINTR) errno = duperrno;
		else {
			doserrno = errno;
			return(-1);
		}
	}
#endif
	errno = duperrno;

	return(0);
}

static int NEAR realwrite(devp, sect, buf, n)
CONST devstat *devp;
u_long sect;
CONST u_char *buf;
int n;
{
	int duperrno;

	duperrno = errno;
#if	MSDOS
	n = rawdiskio(devp -> drive, sect,
		(u_char *)buf, n, devp -> sectsize, 1);
	if (n < 0) {
		doserrno = errno;
		return(-1);
	}
#else
	if (sectseek(devp, sect) < 0) return(-1);
	if (write(devp -> fd, buf, (long)n * devp -> sectsize) < 0) {
		if (errno == EINTR) errno = duperrno;
		else {
			doserrno = errno;
			return(-1);
		}
	}
#endif
	errno = duperrno;

	return(0);
}

static int NEAR killcache(devp, n, size)
CONST devstat *devp;
int n, size;
{
	int i, top;

	top = n - size;
	for (i = n; i > top; i--) {
		if (cachewrite[i]) {
			if (devp -> flags & F_RONLY) {
				doserrno = EROFS;
				n = -1;
			}
			if (realwrite(devp, sectno[i], sectcache[i], 1) < 0)
				n = -1;
		}
		free(sectcache[i]);
		cachewrite[i] = 0;
	}

	return(n);
}

static int NEAR flushcache(devp)
CONST devstat *devp;
{
	int i, n;

	n = 0;
	for (i = maxsectcache - 1; i >= 0; i--) {
		if (cachedrive[i] != devp -> drive) continue;
		if (cachewrite[i] && !(devp -> flags & F_RONLY)
		&& realwrite(devp, sectno[i], sectcache[i], 1) < 0)
			n = -1;
		cachewrite[i] = 0;
	}

	return(n);
}

static int NEAR shiftcache(devp, n, nsect)
CONST devstat *devp;
int n, nsect;
{
	u_char *cachebuf[SECTCACHESIZE], wrtbuf[SECTCACHESIZE];
	u_long no;
	int i, drive, size;

	for (; n < maxsectcache - 1; n++) if (cachesize[n + 1] <= 1) break;
	drive = cachedrive[n];
	no = sectno[n];
	size = cachesize[n];

	if (!devp) {
		for (i = 0; i < maxdev; i++) if (devlist[i].drive == drive) {
			devp = &(devlist[i]);
			break;
		}
	}
	if (nsect < 0) {
		if (killcache(devp, n, size) < 0) return(SECTCACHESIZE + 1);
	}
	else for (i = 0; i < size; i++) {
		cachebuf[i] = sectcache[n - i];
		wrtbuf[i] = cachewrite[n - i];
	}

	for (i = n - size + 1; i < maxsectcache - size; i++) {
		cachedrive[i] = cachedrive[i + size];
		sectno[i] = sectno[i + size];
		sectcache[i] = sectcache[i + size];
		cachesize[i] = cachesize[i + size];
		cachewrite[i] = cachewrite[i + size];
	}
	maxsectcache -= size;
	maxcachemem -= size * devp -> sectsize;

	n = size;
	if (nsect < 0) return(n);

	if (!nsect) nsect = size;
	while (maxsectcache > 0
	&& (maxsectcache + nsect > SECTCACHESIZE
	|| maxcachemem + nsect * devp -> sectsize > SECTCACHEMEM)) {
		if ((i = shiftcache(NULL, 0, -1)) > SECTCACHESIZE)
			return(SECTCACHESIZE + 1);
		n += i;
	}
	maxsectcache += nsect;
	maxcachemem += nsect * devp -> sectsize;
	n -= nsect;

	for (i = 0; i < nsect; i++) {
		cachedrive[maxsectcache - i - 1] = drive;
		sectno[maxsectcache - i - 1] = no + i;
		sectcache[maxsectcache - i - 1] = cachebuf[i];
		cachesize[maxsectcache - i - 1] = nsect - i;
		cachewrite[maxsectcache - i - 1] = wrtbuf[i];
	}

	return(n);
}

static int NEAR cachecpy(n, ofs1, ofs2, size)
int n, ofs1, ofs2, size;
{
	int i;

	if (ofs1 <= ofs2) for (i = 0; i < size; i++) {
		sectcache[n - ofs1 - i] = sectcache[n - ofs2 - i];
		cachewrite[n - ofs1 - i] = cachewrite[n - ofs2 - i];
	}
	else for (i = size - 1; i >= 0; i--) {
		sectcache[n - ofs1 - i] = sectcache[n - ofs2 - i];
		cachewrite[n - ofs1 - i] = cachewrite[n - ofs2 - i];
	}

	return(size);
}

static int NEAR uniqcache(devp, n, min, max)
CONST devstat *devp;
int n;
u_long min, max;
{
	int i, j, size;

	if (min >= max) return(0);
	for (i = n - cachesize[n]; i >= 0; i -= cachesize[i]) {
		if (devp -> drive != cachedrive[i]) continue;
		if (min <= sectno[i] && max >= sectno[i] + cachesize[i])
			return(shiftcache(devp, i, -1));
		if (min > sectno[i] && min < sectno[i] + cachesize[i]) {
			size = min - sectno[i];
			for (j = 0; j < size; j++)
				cachesize[i - j] = size - j;
			return(shiftcache(devp, i - size, -1));
		}
		if (max > sectno[i] && max < sectno[i] + cachesize[i]) {
			size = max - sectno[i];
			for (j = 0; j < size; j++)
				cachesize[i - j] = size - j;
			return(shiftcache(devp, i, -1));
		}
	}

	return(0);
}

static int NEAR appendcache(dst, devp, sect, buf, n, wrt)
int dst;
CONST devstat *devp;
u_long sect;
CONST u_char *buf;
int n, wrt;
{
	u_long min, max;
	int i, size, duperrno;

	duperrno = errno;
	min = sectno[dst];
	max = min + cachesize[dst];
	size = sect + n - min;

	if (devp -> sectsize > STDSECTSIZE
	&& size * devp -> sectsize > SECTCACHEMEM) {
		size = SECTCACHEMEM / devp -> sectsize;
		min = sect + n - size;
		if (killcache(devp, dst, (int)(min - sectno[dst])) < 0)
			return(-1);
		cachecpy(dst, 0, (int)(min - sectno[dst]), (int)(max - min));
	}
	else if (size > SECTCACHESIZE) {
		min += size - SECTCACHESIZE;
		size = SECTCACHESIZE;
		if (killcache(devp, dst, (int)(min - sectno[dst])) < 0)
			return(-1);
		cachecpy(dst, 0, (int)(min - sectno[dst]), (int)(max - min));
	}
	else if (size < cachesize[dst]) size = cachesize[dst];

	if ((i = uniqcache(devp, dst, max, sect + n)) > SECTCACHESIZE)
		return(-1);
	dst -= i;
	sectno[dst] = min;
	if (shiftcache(devp, dst, size) > SECTCACHESIZE) return(-1);

	dst = maxsectcache - 1 - (sect - min);
	for (i = 0; i < n; i++) {
		if (i + sect < max) cachewrite[dst - i] |= wrt;
		else {
			sectcache[dst - i] =
				(u_char *)malloc(devp -> sectsize);
			if (!sectcache[dst - i]) {
				doserrno = errno;
				errno = duperrno;
				return(-1);
			}
			cachewrite[dst - i] = wrt;
		}
		memcpy((char *)sectcache[dst - i],
			(char *)buf, devp -> sectsize);
		buf += devp -> sectsize;
	}

	return(0);
}

static int NEAR insertcache(dst, devp, sect, buf, n, wrt)
int dst;
CONST devstat *devp;
u_long sect;
CONST u_char *buf;
int n, wrt;
{
	u_long min, max;
	int i, size, duperrno;

	duperrno = errno;
	min = sectno[dst];
	max = min + cachesize[dst];
	size = max - sect;

	if (devp -> sectsize > STDSECTSIZE
	&& size * devp -> sectsize > SECTCACHEMEM) {
		size = SECTCACHEMEM / devp -> sectsize;
		max = sect + size;
		i = dst - (max - min);
		if (killcache(devp, i, cachesize[i]) < 0) return(-1);
	}
	else if (size > SECTCACHESIZE) {
		size = SECTCACHESIZE;
		max = sect + size;
		i = dst - (max - min);
		if (killcache(devp, i, cachesize[i]) < 0) return(-1);
	}
	else if (size < cachesize[dst]) size = cachesize[dst];

	if ((i = uniqcache(devp, dst, sect, min)) > SECTCACHESIZE) return(-1);
	dst -= i;
	sectno[dst] = sect;
	if (shiftcache(devp, dst, size) > SECTCACHESIZE) return(-1);

	dst = maxsectcache - 1;
	cachecpy(dst, (int)(min - sect), 0, (int)(max - min));
	for (i = 0; i < n; i++) {
		if (i + sect >= min) cachewrite[dst - i] |= wrt;
		else {
			sectcache[dst - i] =
				(u_char *)malloc(devp -> sectsize);
			if (!sectcache[dst - i]) {
				doserrno = errno;
				errno = duperrno;
				return(-1);
			}
			cachewrite[dst - i] = wrt;
		}
		memcpy((char *)sectcache[dst - i],
			(char *)buf, devp -> sectsize);
		buf += devp -> sectsize;
	}

	return(0);
}

static int NEAR overwritecache(dst, devp, sect, buf, n, wrt)
int dst;
CONST devstat *devp;
u_long sect;
CONST u_char *buf;
int n, wrt;
{
	u_long min, max;
	int i, size, duperrno;

	duperrno = errno;
	min = sectno[dst];
	max = min + cachesize[dst];
	size = n;

	if (size < cachesize[dst]) size = cachesize[dst];
	if ((i = uniqcache(devp, dst, sect, min)) > SECTCACHESIZE) return(-1);
	dst -= i;
	if ((i = uniqcache(devp, dst, max, sect + n)) > SECTCACHESIZE)
		return(-1);
	dst -= i;
	sectno[dst] = sect;
	if (shiftcache(devp, dst, size) > SECTCACHESIZE) return(-1);

	dst = maxsectcache - 1;
	cachecpy(dst, (int)(min - sect), 0, (int)(max - min));
	for (i = 0; i < n; i++) {
		if (i + sect >= min && i + sect < max)
			cachewrite[dst - i] |= wrt;
		else {
			sectcache[dst - i] =
				(u_char *)malloc(devp -> sectsize);
			if (!sectcache[dst - i]) {
				doserrno = errno;
				errno = duperrno;
				return(-1);
			}
			cachewrite[dst - i] = wrt;
		}
		memcpy((char *)sectcache[dst - i],
			(char *)buf, devp -> sectsize);
		buf += devp -> sectsize;
	}

	return(0);
}

static int NEAR savecache(devp, sect, buf, n, wrt)
CONST devstat *devp;
u_long sect;
CONST u_char *buf;
int n, wrt;
{
	int i, duperrno;

	if (n > SECTCACHESIZE || n * devp -> sectsize > SECTCACHEMEM)
		return(-1);

	for (i = maxsectcache - 1; i >= 0; i -= cachesize[i]) {
		if (devp -> drive != cachedrive[i]) continue;
		if (sect >= sectno[i] && sect <= sectno[i] + cachesize[i])
			return(appendcache(i, devp, sect, buf, n, wrt));
		if (sect + n >= sectno[i]
		&& sect + n <= sectno[i] + cachesize[i])
			return(insertcache(i, devp, sect, buf, n, wrt));
		if (sect <= sectno[i]
		&& sect + n >= sectno[i] + cachesize[i])
			return(overwritecache(i, devp, sect, buf, n, wrt));
	}

	duperrno = errno;
	while (maxsectcache > 0
	&& (maxsectcache + n > SECTCACHESIZE
	|| maxcachemem + n * devp -> sectsize > SECTCACHEMEM)) {
		if (shiftcache(NULL, 0, -1) > SECTCACHESIZE) return(-1);
	}
	maxsectcache += n;
	maxcachemem += n * devp -> sectsize;
	for (i = 0; i < n; i++) {
		cachedrive[maxsectcache - i - 1] = devp -> drive;
		sectno[maxsectcache - i - 1] = sect + i;
		sectcache[maxsectcache - i - 1] =
			(u_char *)malloc(devp -> sectsize);
		if (!sectcache[maxsectcache - i - 1]) {
			doserrno = errno;
			errno = duperrno;
			return(-1);
		}
		cachesize[maxsectcache - i - 1] = n - i;
		cachewrite[maxsectcache - i - 1] = wrt;
		memcpy((char *)sectcache[maxsectcache - i - 1],
			(char *)buf, devp -> sectsize);
		buf += devp -> sectsize;
	}

	return(0);
}

static int NEAR loadcache(devp, sect, buf, n)
CONST devstat *devp;
u_long sect;
u_char *buf;
int n;
{
	u_char *cp;
	int i, j, src, size;

	if (n <= 0) return(0);
	for (i = maxsectcache - 1; i >= 0; i -= cachesize[i]) {
		if (devp -> drive != cachedrive[i]) continue;
		if (sect >= sectno[i]
		&& sect + n <= sectno[i] + cachesize[i]) {
			src = i - (sect - sectno[i]);
			for (j = 0; j < n; j++) {
				memcpy((char *)buf,
					(char *)sectcache[src - j],
					devp -> sectsize);
				buf += devp -> sectsize;
			}
			if (shiftcache(devp, i, 0) > SECTCACHESIZE) return(-1);
			return(0);
		}
		if (sect >= sectno[i] && sect < sectno[i] + cachesize[i]) {
			size = sectno[i] + cachesize[i] - sect;
			src = i - (sect - sectno[i]);
			cp = buf + (long)size * devp -> sectsize;
			for (j = 0; j < size; j++) {
				memcpy((char *)buf,
					(char *)sectcache[src - j],
					devp -> sectsize);
				buf += devp -> sectsize;
			}
			if (shiftcache(devp, i, 0) > SECTCACHESIZE) return(-1);
			return(loadcache(devp, sect + size, cp, n - size));
		}
		if (sect + n > sectno[i]
		&& sect + n <= sectno[i] + cachesize[i]) {
			size = sect + n - sectno[i];
			src = i;
			cp = buf + (long)(n - size) * devp -> sectsize;
			if (loadcache(devp, sect, buf, n - size) < 0)
				return(-1);
			for (j = 0; j < size; j++) {
				memcpy((char *)cp,
					(char *)sectcache[src - j],
					devp -> sectsize);
				cp += devp -> sectsize;
			}
			if (shiftcache(devp, i, 0) > SECTCACHESIZE) return(-1);
			return(0);
		}
		if (sect <= sectno[i]
		&& sect + n >= sectno[i] + cachesize[i]) {
			size = sectno[i] - sect;
			src = i;
			cp = buf + (long)size * devp -> sectsize;
			if (loadcache(devp, sect, buf, size) < 0) return(-1);
			for (j = 0; j < cachesize[i]; j++) {
				memcpy((char *)cp,
					(char *)sectcache[src - j],
					devp -> sectsize);
				cp += devp -> sectsize;
			}
			if (shiftcache(devp, i, 0) > SECTCACHESIZE) return(-1);
			size = sect + n - sectno[i] + cachesize[i];
			cp += (long)cachesize[i] * devp -> sectsize;
			return(loadcache(devp, sect + size, cp, n - size) < 0);
		}
	}
	if (realread(devp, sect, buf, n) < 0) return(-1);
	savecache(devp, sect, buf, n, 0);

	return(0);
}

static long NEAR sectread(devp, sect, buf, n)
CONST devstat *devp;
u_long sect;
u_char *buf;
int n;
{
	if (loadcache(devp, sect, buf, n) < 0) return(-1);

	return((long)n * devp -> sectsize);
}

static long NEAR sectwrite(devp, sect, buf, n)
CONST devstat *devp;
u_long sect;
CONST u_char *buf;
int n;
{
	if (devp -> flags & F_RONLY) {
		doserrno = EROFS;
		return(-1);
	}
	if (savecache(devp, sect, buf, n, 1) < 0
	&& realwrite(devp, sect, buf, n) < 0)
		return(-1);

	return((long)n * devp -> sectsize);
}

static int NEAR availfat(devp)
CONST devstat *devp;
{
	u_char *buf;
	u_long i, n, clust;
	int j, gap, duperrno;

	if (!needbavail || devp -> availsize != (u_long)0xffffffff)
		return(0);

	duperrno = errno;
	if (!(buf = (u_char *)malloc(devp -> sectsize))) {
		doserrno = errno;
		errno = duperrno;
		return(-1);
	}

	if (doswaitfunc) (*doswaitfunc)();
	n = 0;
	clust = devp -> totalsize + 2;
	if (devp -> flags & F_FAT32)
	for (i = 0; i < devp -> fatsize; i++) {
		if (dosintrfunc && (*dosintrfunc)()) {
			free(buf);
			return(0);
		}
		if (sectread(devp, devp -> fatofs + i, buf, 1) < 0L) {
			free(buf);
			return(-1);
		}
		for (j = 0; j < devp -> sectsize; j += 4) {
			if (clust) clust--;
			else break;
			if (!buf[j] && !buf[j + 1]
			&& !buf[j + 2] && !buf[j + 3])
				n++;
		}
		if (!clust) break;
	}
	else if (devp -> flags & F_16BIT)
	for (i = 0; i < devp -> fatsize; i++) {
		if (dosintrfunc && (*dosintrfunc)()) {
			free(buf);
			return(0);
		}
		if (sectread(devp, devp -> fatofs + i, buf, 1) < 0L) {
			free(buf);
			return(-1);
		}
		for (j = 0; j < devp -> sectsize; j += 2) {
			if (clust) clust--;
			else break;
			if (!buf[j] && !buf[j + 1]) n++;
		}
		if (!clust) break;
	}
	else for (i = gap = 0; i < devp -> fatsize; i++) {
		if (dosintrfunc && (*dosintrfunc)()) {
			free(buf);
			return(0);
		}
		if (sectread(devp, devp -> fatofs + i, buf, 1) < 0L) {
			free(buf);
			return(-1);
		}
		if ((gap & 4)) {
			if (clust) clust--;
			else break;
			if (((gap & 3) == 2 && !buf[0])
			|| ((gap & 3) == 1 && !(buf[0] & 0x0f)))
				n++;
		}
		for (j = (gap & 3); j < (devp -> sectsize - 1) * 2; j += 3) {
			if (clust) clust--;
			else break;
			if (j % 2) {
				if (!(buf[j / 2] & 0xf0) && !buf[j / 2 + 1])
					n++;
			}
			else {
				if (!buf[j / 2] && !(buf[j / 2 + 1] & 0x0f))
					n++;
			}
		}
		if (!clust) break;
		if (j >= devp -> sectsize * 2) gap = 0;
		else if (j % 2) gap = (buf[j / 2] & 0xf0) ? 6 : 2;
		else gap = (buf[j / 2]) ? 5 : 1;
	}

	for (i = 0; i < maxdev; i++)
		if (devlist[i].drive == devp -> drive)
			devlist[i].availsize = n;
	free(buf);

	return(0);
}

static int NEAR readfat(devp)
devstat *devp;
{
	long size;
	int duperrno;

	duperrno = errno;
	size = (long)(devp -> fatsize) * (long)(devp -> sectsize);
	if (size > MAXFATMEM) devp -> fatbuf = NULL;
	else if ((devp -> fatbuf = (char *)malloc(size))
	&& sectread(devp, devp -> fatofs,
	(u_char *)(devp -> fatbuf), devp -> fatsize) < 0L) {
		doserrno = errno;
		free(devp -> fatbuf);
		errno = duperrno;
		return(-1);
	}
	errno = duperrno;

	return(0);
}

static int NEAR writefat(devp)
CONST devstat *devp;
{
	char *buf;
	long i;
	int n;

	n = 0;
	buf = devp -> fatbuf;
	for (i = 0; i < maxdev; i++) {
		if (devlist[i].drive != devp -> drive) continue;
		n |= devlist[i].flags & F_WRFAT;
		if (!buf) buf = devlist[i].fatbuf;
		devlist[i].flags &= ~F_WRFAT;
	}
	if (!n || !buf) return(0);

	for (i = devp -> fatofs; i < devp -> dirofs; i += devp -> fatsize)
		if (sectwrite(devp, i, (u_char *)buf, devp -> fatsize) < 0L)
			return(-1);

	return(0);
}

static long NEAR getfatofs(devp, clust)
CONST devstat *devp;
long clust;
{
	long ofs, bit;

	if (!clust) clust = devp -> rootdir;
	bit = (devp -> flags & F_FAT32)
		? 8 : ((devp -> flags & F_16BIT) ? 4 : 3);
	ofs = (clust * bit) / 2;
	if (ofs < bit
	|| ofs >= (long)(devp -> fatsize) * (long)(devp -> sectsize)) {
		doserrno = EIO;
		return(-1L);
	}

	return(ofs);
}

static u_char *NEAR readtmpfat(devp, ofs, nsect)
CONST devstat *devp;
long ofs;
int nsect;
{
	u_char *buf;
	int duperrno;

	duperrno = errno;
	if (!(buf = (u_char *)malloc((devp -> sectsize) * nsect))) {
		doserrno = errno;
		errno = duperrno;
		return(NULL);
	}
	ofs /= (devp -> sectsize);
	if (sectread(devp, (u_long)(devp -> fatofs) + ofs, buf, nsect) < 0L) {
		free(buf);
		return(NULL);
	}

	return(buf);
}

static long NEAR getfatent(devp, clust)
CONST devstat *devp;
long clust;
{
	u_char *fatp, *buf;
	long ofs;
	int nsect;

	if ((ofs = getfatofs(devp, clust)) < 0L) return(-1L);
	if ((devp -> fatbuf)) {
		buf = NULL;
		fatp = (u_char *)&(devp -> fatbuf[ofs]);
	}
	else {
		nsect = 1;
		if (!(devp -> flags & (F_16BIT | F_FAT32))
		&& (ofs % devp -> sectsize) >= devp -> sectsize - 1)
			nsect = 2;
		if (!(buf = readtmpfat(devp, ofs, nsect))) return(-1L);
		fatp = buf + (ofs % (devp -> sectsize));
	}

	if (devp -> flags & F_FAT32) {
		ofs = (*fatp & 0xff) + ((u_long)(*(fatp + 1) & 0xff) << 8)
			+ ((u_long)(*(fatp + 2) & 0xff) << 16)
			+ ((u_long)(*(fatp + 3) & 0xff) << 24);
		if (ofs >= (u_long)0x0ffffff8) ofs = ENDCLUST;
	}
	else if (devp -> flags & F_16BIT) {
		ofs = (*fatp & 0xff) + ((long)(*(fatp + 1) & 0xff) << 8);
		if (ofs >= 0x0fff8) ofs = ENDCLUST;
		else if (ofs == 0x0fff7) ofs = ERRCLUST;
	}
	else {
		if (clust % 2)
			ofs = (((*fatp) & 0xf0) >> 4)
				+ ((long)(*(fatp + 1) & 0xff) << 4);
		else ofs = (*fatp & 0xff) + ((long)(*(fatp + 1) & 0x0f) << 8);
		if (ofs >= 0x0ff8) ofs = ENDCLUST;
		else if (ofs == 0x0ff7) ofs = ERRCLUST;
	}
	if (buf) free(buf);

	return(ofs);
}

static int NEAR putfatent(devp, clust, n)
devstat *devp;
long clust, n;
{
	u_char *fatp, *buf;
	u_long avail;
	long l, ofs;
	int i, old, nsect;

	if ((ofs = getfatofs(devp, clust)) < 0L) return(-1);
	if ((devp -> fatbuf)) {
		buf = NULL;
#ifdef	FAKEUNINIT
		nsect = 0;	/* fake for -Wuninitialized */
#endif
		fatp = (u_char *)&(devp -> fatbuf[ofs]);
	}
	else {
		nsect = 1;
		if (!(devp -> flags & (F_16BIT | F_FAT32))
		&& (ofs % devp -> sectsize) >= devp -> sectsize - 1)
			nsect = 2;
		if (!(buf = readtmpfat(devp, ofs, nsect))) return(-1);
		fatp = buf + (ofs % (devp -> sectsize));
	}
	if (devp -> flags & F_FAT32) {
		old = (*fatp || *(fatp + 1) || *(fatp + 2) || *(fatp + 3));
		*fatp = n & 0xff;
		*(fatp + 1) = (n >> 8) & 0xff;
		*(fatp + 2) = (n >> 16) & 0xff;
		*(fatp + 3) = (n >> 24) & 0xff;
	}
	else if (devp -> flags & F_16BIT) {
		old = (*fatp || *(fatp + 1));
		*fatp = n & 0xff;
		*(fatp + 1) = (n >> 8) & 0xff;
	}
	else if (clust % 2) {
		old = ((*fatp & 0xf0) || *(fatp + 1));
		*fatp &= 0x0f;
		*fatp |= (n << 4) & 0xf0;
		*(fatp + 1) = (n >> 4) & 0xff;
	}
	else {
		old = (*fatp || (*(fatp + 1) & 0x0f));
		*fatp = n & 0xff;
		*(fatp + 1) &= 0xf0;
		*(fatp + 1) |= (n >> 8) & 0x0f;
	}
	if (!buf) devp -> flags |= F_WRFAT;
	else {
		for (l = devp -> fatofs; l < devp -> dirofs;
		l += devp -> fatsize) {
			if (sectwrite(devp, l + (ofs / (devp -> sectsize)),
			buf, nsect) < 0L) {
				free(buf);
				return(-1);
			}
		}
		free(buf);
	}

	if (devp -> availsize != (u_long)0xffffffff) {
		avail = devp -> availsize;
		if (!n && old) avail++;
		else if (n && !old && avail) avail--;
		if (avail != devp -> availsize)
			for (i = 0; i < maxdev; i++)
				if (devlist[i].drive == devp -> drive)
					devlist[i].availsize = avail;
	}

	return(0);
}

static u_long NEAR clust2sect(devp, clust)
CONST devstat *devp;
long clust;
{
	u_long sect;

	if (devp -> flags & F_FAT32) {
		if (!clust) clust = devp -> rootdir;
	}
	else if (!clust || clust >= ROOTCLUST) {
		sect = (clust) ? clust - ROOTCLUST : devp -> dirofs;
		if (sect >= devp -> dirofs + (u_long)(devp -> dirsize)) {
			doserrno = 0;
			return((u_long)0);
		}
		return(sect);
	}

	if (clust == ENDCLUST) {
		doserrno = 0;
		return((u_long)0);
	}
	if (clust == ERRCLUST) {
		doserrno = EIO;
		return((u_long)0);
	}
	sect = ((clust - MINCLUST) * (u_long)(devp -> clustsize))
		+ (devp -> dirofs + (u_long)(devp -> dirsize));

	return(sect);
}

static long NEAR clust32(devp, dentp)
CONST devstat *devp;
CONST dent_t *dentp;
{
	long clust;

	clust = (long)byte2word(dentp -> clust);
	if (devp -> flags & F_FAT32)
		clust += (long)byte2word(dentp -> clust_h) << 16;

	return(clust);
}

static long NEAR newclust(devp)
CONST devstat *devp;
{
	long clust, used;

	for (clust = MINCLUST; (used = getfatent(devp, clust)) >= 0L; clust++)
		if (!used) break;
	if (used < 0 || clust > devp -> totalsize) {
		doserrno = ENOSPC;
		return(-1L);
	}

	return(clust);
}

static long NEAR clustread(devp, buf, clust)
CONST devstat *devp;
u_char *buf;
long clust;
{
	u_long sect;
	long next;

	if (!(sect = clust2sect(devp, clust))) return(-1L);
	if (!(devp -> flags & F_FAT32)
	&& sect < devp -> dirofs + (u_long)(devp -> dirsize))
		next = ROOTCLUST + sect + (long)(devp -> clustsize);
	else next = getfatent(devp, clust);

	if (buf && sectread(devp, sect, buf, devp -> clustsize) < 0L)
		return(-1L);

	return(next);
}

static long NEAR clustwrite(devp, buf, prev)
devstat *devp;
CONST u_char *buf;
long prev;
{
	u_long sect;
	long clust;

	if ((clust = newclust(devp)) < 0L) return(-1L);
	if (!(sect = clust2sect(devp, clust))) {
		doserrno = EIO;
		return(-1L);
	}
	if (buf && sectwrite(devp, sect, buf, devp -> clustsize) < 0L)
		return(-1L);

	if ((prev && putfatent(devp, prev, clust) < 0)
	|| putfatent(devp, clust, ENDCLUST) < 0) return(-1L);

	return(clust);
}

static long NEAR clustexpand(devp, clust, fill)
devstat *devp;
long clust;
int fill;
{
	u_char *buf;
	long n, new, size;
	int duperrno;

	if (!fill) buf = NULL;
	else {
		duperrno = errno;
		size = (long)(devp -> clustsize) * (long)(devp -> sectsize);
		if (!(buf = (u_char *)malloc(size))) {
			doserrno = errno;
			errno = duperrno;
			return(-1L);
		}
		for (n = 0; n < size; n++) buf[n] = 0;
	}
	new = clustwrite(devp, buf, clust);
	if (buf) free(buf);

	return(new);
}

static int NEAR clustfree(devp, clust)
devstat *devp;
long clust;
{
	long next;

	if (clust) for (;;) {
		next = getfatent(devp, clust);
		if (next < 0L || putfatent(devp, clust, 0) < 0) return(-1);
		if (next == ENDCLUST || next == ERRCLUST) break;
		clust = next;
	}

	return(0);
}

#if	MSDOS
static int NEAR readbpb(devp, drv)
devstat *devp;
int drv;
#else
static int NEAR _readbpb(devp, bpbcache)
devstat *devp;
CONST bpb_t *bpbcache;
#endif
{
#if	!MSDOS
	int i, cc, fd, nh, ns, nsect;
#endif
#ifdef	LINUX
	struct stat st1, st2;
	struct mntent *mntp;
	FILE *fp;
#endif
	CONST bpb_t *bpb;
	char *buf;
	long total;
	int duperrno;

	duperrno = errno;
#if	MSDOS
	if (!(buf = (char *)malloc(MAXSECTSIZE))) {
		doserrno = errno;
		errno = duperrno;
		return(-1);
	}
	bpb = (bpb_t *)buf;
	if (rawdiskio(drv, 0, (u_char *)buf, 1, MAXSECTSIZE, 0) < 0) {
		doserrno = errno;
		free(buf);
		errno = duperrno;
		return(-1);
	}
	memset((char *)devp, 0, sizeof(*devp));
	devp -> drive = drv;
#else	/* !MSDOS */
	buf = NULL;
	if (!(devp -> ch_head) || !(nsect = devp -> ch_sect)) return(0);

# ifdef	HDDMOUNT
	if (!(devp -> ch_cyl));
	else
# endif
	if (nsect <= 100) devp -> flags &= ~F_8SECT;
	else {
		nsect %= 100;
		devp -> flags |= F_8SECT;
	}

	if (bpbcache -> nfat) bpb = bpbcache;
	else {
		devp -> fd = -1;
		i = (O_BINARY | O_RDWR);
# ifdef	HDDMOUNT
		if (!(devp -> ch_cyl) && Xtoupper(devp -> ch_head) != 'W') {
			i = (O_BINARY | O_RDONLY);
			devp -> flags |= F_RONLY;
		}
# endif
# ifdef	LINUX
		if (i == (O_BINARY | O_RDWR)) {
			if (stat(devp -> ch_name, &st1)) {
				doserrno = errno;
				errno = duperrno;
				return(-1);
			}
			if ((fp = setmntent(MOUNTED, "r"))) {
				while ((mntp = getmntent(fp))) {
#  if	1
					if (strstr(mntp -> mnt_opts, "ro"))
						continue;
#  endif
					if (stat(mntp -> mnt_fsname, &st2))
						continue;
					if (st1.st_ino == st2.st_ino) {
						i = (O_BINARY | O_RDONLY);
						devp -> flags |= F_RONLY;
						break;
					}
				}
				endmntent(fp);
			}
		}
# endif	/* LINUX */
		if ((fd = open(devp -> ch_name, i, 0666)) < 0) {
# ifdef	EFORMAT
			if (errno == EFORMAT) {
				errno = duperrno;
				return(0);
			}
# endif
			if (errno == EIO) errno = ENODEV;
			if ((errno != EROFS && errno != EACCES)
			|| i == (O_BINARY | O_RDONLY)
			|| (fd = open(devp -> ch_name,
			O_BINARY | O_RDONLY, 0666)) < 0) {
				doserrno = errno;
				errno = duperrno;
				return(-1);
			}
			devp -> flags |= F_RONLY;
		}
# if	defined (LINUX) && defined (BLKFLSBUF)
		VOID_C ioctl(fd, BLKFLSBUF, 0);
# endif

		cc = 0;
		for (i = 0; i < SLISTSIZ; i++)
			if (cc < sectsizelist[i]) cc = sectsizelist[i];
		if (!(buf = (char *)malloc(cc))) {
			VOID_C close(fd);
			doserrno = errno;
			errno = duperrno;
			return(-1);
		}
		for (i = 0; i < SLISTSIZ; i++) {
# ifdef	HDDMOUNT
			if (Xllseek(fd, devp -> offset, L_SET) < (l_off_t)0)
# else
			if (Xllseek(fd, (l_off_t)0, L_SET) < (l_off_t)0)
# endif
			{
				i = SLISTSIZ;
				break;
			}
			while ((cc = read(fd, buf, sectsizelist[i])) < 0) {
				if (dosintrfunc && (*dosintrfunc)()) {
					VOID_C close(fd);
					doserrno = EINTR;
					errno = duperrno;
					return(-1);
				}
				if (errno != EINTR) break;
			}
			if (cc >= 0) break;
		}
		if (i >= SLISTSIZ) {
			VOID_C close(fd);
			free(buf);
			errno = duperrno;
			return(0);
		}
		bpb = (bpb_t *)buf;
		devp -> fd = newdup(fd);
	}
#endif	/* !MSDOS */

	total = byte2word(bpb -> total);
	if (!total) total = byte2dword(bpb -> bigtotal);
#if	!MSDOS
# ifdef	HDDMOUNT
	if (devp -> ch_cyl)
# endif
	{
		nh = byte2word(bpb -> nhead);
		ns = byte2word(bpb -> secttrack);
		if (!nh || !ns || nh != devp -> ch_head || ns != nsect
		|| (total + (nh * ns / 2)) / (nh * ns) != devp -> ch_cyl) {
			if (bpb != bpbcache)
				memcpy((char *)bpbcache,
					(char *)bpb, sizeof(*bpbcache));
			if (buf) free(buf);
			errno = duperrno;
			return(0);
		}
	}
#endif	/* !MSDOS */

	devp -> clustsize = bpb -> clustsize;
	devp -> sectsize = byte2word(bpb -> sectsize);
	if (!(devp -> clustsize) || !(devp -> sectsize)) {
		if (buf) free(buf);
		errno = duperrno;
		return(-1);
	}

	devp -> fatofs = byte2word(bpb -> bootsect);
	devp -> fatsize = byte2word(bpb -> fatsize);
	devp -> dirsize = (u_long)byte2word(bpb -> maxdir) * DOSDIRENT
		/ (u_long)(devp -> sectsize);

	if (devp -> fatsize && devp -> dirsize) devp -> rootdir = 0L;
	else {
		if (!(devp -> fatsize))
			devp -> fatsize = byte2dword(bpb -> bigfatsize);
		devp -> dirsize = 0;
		devp -> rootdir = byte2dword(bpb -> rootdir);
		devp -> flags |= F_FAT32;
	}

	devp -> dirofs = devp -> fatofs + (devp -> fatsize * bpb -> nfat);
	total -= devp -> dirofs + (u_long)(devp -> dirsize);
	devp -> totalsize = total / (devp -> clustsize);
	devp -> availsize = (u_long)0xffffffff;

	if (!(devp -> flags & F_FAT32)) {
		if (!strsizecmp(bpb -> fsname, FS_FAT)
		|| !strsizecmp(bpb -> fsname, FS_FAT12))
			devp -> flags &= ~F_16BIT;
		else if (!strsizecmp(bpb -> fsname, FS_FAT16)
		|| devp -> totalsize > MAX12BIT)
			devp -> flags |= F_16BIT;
	}

	if (buf) free(buf);
	errno = duperrno;

	return(1);
}

#if	!MSDOS
static int NEAR readbpb(devp, drv)
devstat *devp;
int drv;
{
	bpb_t bpb;
	CONST char *cp;
	int i, n;

	cp = NULL;
	devp -> fd = -1;
	for (i = 0; i < maxfdtype; i++) {
		if (drv != (int)fdtype[i].drive) continue;
		if (!cp || strcmp(cp, fdtype[i].name)) {
			bpb.nfat = 0;
			if ((devp -> fd) >= 0) VOID_C close(devp -> fd);
			memset((char *)devp, 0, sizeof(*devp));
			devp -> fd = -1;
		}
		memcpy((char *)devp, (char *)&(fdtype[i]), sizeof(*fdtype));
		cp = fdtype[i].name;
		if ((n = _readbpb(devp, &bpb)) < 0) return(-1);
		if (n > 0) break;
	}
	if (i >= maxfdtype) {
		if ((devp -> fd) >= 0) VOID_C close(devp -> fd);
		doserrno = ENODEV;
		return(-1);
	}

	return(0);
}
#endif	/* !MSDOS */

#ifdef	HDDMOUNT
static l_off_t *NEAR _readpt(offset, extoffset, fd, head, sect, secsiz)
l_off_t offset, extoffset;
int fd, head, sect, secsiz;
{
	partition_t *pt;
	partition98_t *pt98;
	u_char *cp, *buf;
	l_off_t ofs, *sp, *tmp, *slice;
	int i, n, nslice, pofs, ps, pn, beg, siz;

	if (head) {
		ps = PART98_SIZE;
		pofs = PART98_TABLE;
		pn = PART98_NUM;
	}
	else {
		ps = PART_SIZE;
		pofs = PART_TABLE;
		pn = PART_NUM;
	}

	beg = (pofs / DEV_BSIZE) * DEV_BSIZE;
	siz = ((pofs + ps * pn - 1) / DEV_BSIZE + 1) * DEV_BSIZE - beg;

	if (!(slice = (l_off_t *)malloc(2 * sizeof(l_off_t)))) return(NULL);
	slice[nslice = 0] = (l_off_t)secsiz;
	slice[++nslice] = (l_off_t)0;

	if (Xllseek(fd, offset + beg, L_SET) < (l_off_t)0) return(slice);
	if (!(buf = (u_char *)malloc(siz))) {
		free(slice);
		return(NULL);
	}

	while ((i = read(fd, buf, siz)) < 0 && errno == EINTR);
	if (i < 0
	|| (!head && (buf[siz - 2] != 0x55 || buf[siz - 1] != 0xaa))) {
		free(buf);
		return(slice);
	}
	cp = &(buf[pofs - beg]);

	for (i = 0; i < pn; i++, cp += ps) {
		if (head) {
			pt98 = (partition98_t *)cp;
			if (pt98 -> filesys != PT98_FAT12
			&& pt98 -> filesys != PT98_FAT16
			&& pt98 -> filesys != PT98_FAT16X
			&& pt98 -> filesys != PT98_FAT32)
				continue;
			ofs = byte2word(pt98 -> s_cyl);
			ofs = ofs * head + pt98 -> s_head;
			ofs = ofs * sect + pt98 -> s_sect;
			ofs *= secsiz;
		}
		else {
			pt = (partition_t *)cp;
			ofs = byte2dword(pt -> f_sect);
			ofs *= secsiz;
			if (pt -> filesys == PT_EXTEND
			|| pt -> filesys == PT_EXTENDLBA) {
				if (extoffset) ofs += extoffset;
				else extoffset = ofs;

				sp = _readpt(ofs, extoffset, fd, 0, 0, secsiz);
				if (!sp) {
					free(buf);
					free(slice);
					return(NULL);
				}
				for (n = 0; sp[n + 1]; n++) /*EMPTY*/;
				if (!n) {
					free(sp);
					continue;
				}

				siz = (nslice + n + 1) * sizeof(l_off_t);
				if (!(tmp = (l_off_t *)realloc(slice, siz))) {
					free(buf);
					free(slice);
					free(sp);
					return(NULL);
				}
				slice = tmp;

				memcpy((char *)&(slice[nslice]),
					(char *)&(sp[1]), n * sizeof(l_off_t));
				slice[nslice += n] = (l_off_t)0;
				free(sp);
				continue;
			}
			else if (pt -> filesys != PT_FAT12
			&& pt -> filesys != PT_FAT16
			&& pt -> filesys != PT_FAT16X
			&& pt -> filesys != PT_FAT32
			&& pt -> filesys != PT_FAT32LBA
			&& pt -> filesys != PT_FAT16XLBA)
				continue;

			ofs += offset;
		}

		siz = (nslice + 1 + 1) * sizeof(l_off_t);
		if (!(tmp = (l_off_t *)realloc(slice, siz))) {
			free(buf);
			free(slice);
			return(NULL);
		}
		slice = tmp;
		slice[nslice++] = ofs;
		slice[nslice] = (l_off_t)0;
	}
	free(buf);

	return(slice);
}

l_off_t *readpt(devfile, pc98)
CONST char *devfile;
int pc98;
{
# ifdef	DIOCGDINFO
	struct disklabel dl;
	l_off_t *slice;
	int fd, head, sect, size;
# endif
# ifdef	SOLARIS
	struct vtoc toc;
# endif

# ifdef	DIOCGDINFO
	if ((fd = open(devfile, O_BINARY | O_RDONLY, 0666)) < 0) {
		if (errno == EIO) errno = ENODEV;
		return(NULL);
	}

	/* hack for disk initialize */
	read(fd, &head, sizeof(head));

	head = sect = 0;
	if (ioctl(fd, DIOCGDINFO, &dl) < 0) {
		VOID_C close(fd);
		return(NULL);
	}
	if (pc98) {
		head = dl.d_ntracks;
		sect = dl.d_nsectors;
	}

#  ifdef	SOLARIS
	{
		if (ioctl(fd, DKIOCGVTOC, &toc) < 0) {
			VOID_C close(fd);
			return(NULL);
		}
		size = toc.v_sectorsz;
	}
#  else
	size = D_SECSIZE(dl);
#  endif
	slice = _readpt((l_off_t)0, (l_off_t)0, fd, head, sect, size);

	VOID_C close(fd);

	return(slice);
# else	/* !DIOCGDINFO */
	errno = ENODEV;

	return(NULL);
# endif	/* !DIOCGDINFO */
}
#endif	/* HDDMOUNT */

static int NEAR _dosopendev(drive)
int drive;
{
	devstat dev;
	int i, new, drv;

	if (!drive) {
		doserrno = ENODEV;
		return(-1);
	}
	for (new = 0; new < maxdev; new++) if (!(devlist[new].drive)) break;
	if (new >= DOSNOFILE || maxorder >= DOSNOFILE) {
		doserrno = EMFILE;
		return(-1);
	}
	drv = Xtoupper(drive);

	for (i = maxorder - 1; i >= 0; i--)
		if (drv == (int)devlist[devorder[i]].drive) break;
	if (i >= 0) {
		memcpy((char *)&dev,
			(char *)&(devlist[devorder[i]]), sizeof(dev));
		dev.flags |= F_DUPL;
		dev.flags &= ~(F_CACHE | F_WRFAT);
	}
	else {
		if (readbpb(&dev, drv) < 0) return(-1);
		dev.dircache = NULL;
		if (readfat(&dev) < 0) {
#if	!MSDOS
			VOID_C close(dev.fd);
#endif
			return(-1);
		}
	}
	if (Xislower(drive)) dev.flags |= F_VFAT;
	else dev.flags &= ~F_VFAT;

	memcpy((char *)&(devlist[new]), (char *)&dev, sizeof(*devlist));
	devlist[new].nlink = 1;
	if (new >= maxdev) maxdev++;
	devorder[maxorder++] = new;

	return(new);
}

static int NEAR _dosclosedev(dd)
int dd;
{
	int i, n, duperrno;

	if (dd < 0 || dd >= maxdev || !devlist[dd].drive) {
		doserrno = EBADF;
		return(-1);
	}

	if (devlist[dd].nlink > 0) devlist[dd].nlink--;
	if (devlist[dd].nlink > 0) return(0);

	duperrno = errno;
	n = 0;
	if (!(devlist[dd].flags & F_DUPL)) {
		for (i = 0; i < maxdev; i++)
			if (i != dd && devlist[i].drive == devlist[dd].drive
			&& (devlist[i].flags & F_DUPL))
				break;
		if (i < maxdev) {
			devlist[i].flags &= ~F_DUPL;
			if ((devlist[dd].flags & F_CACHE)
			&& !(devlist[i].flags & F_CACHE)) {
				devlist[i].dircache = devlist[dd].dircache;
				devlist[i].flags |= F_CACHE;
				devlist[dd].flags &= ~F_CACHE;
			}
			devlist[i].flags |= (devlist[dd].flags & F_WRFAT);

			devlist[dd].flags |= F_DUPL;
			devlist[dd].flags &= ~F_WRFAT;
		}
	}

	if (writefat(&(devlist[dd])) < 0) n = -1;
	if (devlist[dd].flags & F_CACHE) free(devlist[dd].dircache);

	if (devlist[dd].flags & F_DUPL) flushcache(&(devlist[dd]));
	else {
		for (i = maxsectcache - 1; i >= 0; i -= cachesize[i]) {
			if (cachedrive[i] == devlist[dd].drive
			&& shiftcache(&(devlist[dd]), i, -1) > SECTCACHESIZE)
				n = -1;
		}
		if (devlist[dd].fatbuf) free(devlist[dd].fatbuf);
#if	!MSDOS
		VOID_C close(devlist[dd].fd);
#endif
	}

	for (i = maxorder - 1; i >= 0; i--) if (devorder[i] == dd) break;
	if (i >= 0) memmove((char *)&(devorder[i]),
		(char *)&(devorder[i + 1]),
		(--maxorder - i) * sizeof(*devorder));

	devlist[dd].drive = '\0';
	while (maxdev > 0 && !devlist[maxdev - 1].drive) maxdev--;
	errno = duperrno;

	return(n);
}

int dosopendev(drive)
int drive;
{
	int i, drv;

	drv = Xtoupper(drive);
	for (i = maxdev - 1; i >= 0; i--)
		if (drv == (int)devlist[i].drive) break;
	if (i >= 0) {
		devlist[i].nlink++;
		return(i);
	}

	if (doswaitfunc) (*doswaitfunc)();
	if ((i = _dosopendev(drive)) < 0) return(seterrno(doserrno));

	return(i);
}

VOID dosclosedev(dd)
int dd;
{
	int duperrno;

	duperrno = errno;
	VOID_C _dosclosedev(dd);
	errno = duperrno;
}

int flushdrv(drive, func)
int drive;
VOID_T (*func)__P_((VOID_A));
{
	devstat dev;
	u_long avail;
	int i, n, dd, drv, duperrno;

	drv = Xtoupper(drive);
	duperrno = errno;
	n = 0;
	dd = -1;
	for (i = maxorder - 1; i >= 0; i--) {
		if (drv != (int)devlist[devorder[i]].drive) continue;
		if (writefat(&(devlist[devorder[i]])) < 0) {
			duperrno = doserrno;
			n = -1;
		}
		if (!(devlist[devorder[i]].flags & F_DUPL)) dd = devorder[i];
		if (devlist[devorder[i]].flags & F_CACHE) {
			free(devlist[devorder[i]].dircache);
			devlist[devorder[i]].dircache = NULL;
			devlist[devorder[i]].flags &= ~F_CACHE;
		}
	}
	if (dd < 0) {
		if (func) (*func)();
		errno = duperrno;
		return(n);
	}

	for (i = maxsectcache - 1; i >= 0; i -= cachesize[i]) {
		if (drv != cachedrive[i]) continue;
		if (shiftcache(&(devlist[dd]), i, -1) > SECTCACHESIZE) {
			duperrno = doserrno;
			n = -1;
		}
	}
	if (devlist[dd].fatbuf) free(devlist[dd].fatbuf);
	if (devlist[dd].dircache) free(devlist[dd].dircache);
	avail = devlist[dd].availsize;
#if	!MSDOS
	VOID_C close(devlist[dd].fd);
#endif
	if (func) (*func)();
#if	!MSDOS
	sync();
	sync();
#endif
	if (readbpb(&dev, drv) < 0) {
		duperrno = doserrno;
		n = -1;
	}
	else {
		dev.dircache = NULL;
		dev.availsize = avail;
		if (readfat(&dev) < 0) {
#if	!MSDOS
			VOID_C close(dev.fd);
#endif
			return(-1);
		}
	}
	if (Xislower(drive)) dev.flags |= F_VFAT;
	else dev.flags &= ~F_VFAT;

	memcpy((char *)&(devlist[dd]), (char *)&dev, sizeof(*devlist));
	errno = duperrno;

	return(n);
}

static int NEAR calcsum(name)
CONST u_char *name;
{
	int i, sum;

	for (i = sum = 0; i < 8 + 3; i++) {
		sum = (((sum & 0x01) << 7) | ((sum & 0xfe) >> 1));
		sum += name[i];
	}

	return(sum & 0xff);
}

static u_int NEAR lfnencode(c1, c2)
u_int c1, c2;
{
	c1 &= 0xff;
	c2 &= 0xff;
#if	MSDOS
	if (!c1 && (c2 < ' ' || Xstrchr(NOTINLFN, c2))) return(0xffff);
#else
	if (!c1 && (c2 < ' ' || Xstrchr(NOTINLFN, c2))) return('_');
#endif

	return(cnvunicode((c1 << 8) | c2, 1));
}

static u_int NEAR lfndecode(c1, c2)
u_int c1, c2;
{
	c1 &= 0xff;
	c2 &= 0xff;

	return(cnvunicode((c1 << 8) | c2, 0));
}

#if	!MSDOS
static int NEAR transchar(c)
int c;
{
	if (c == '+') return('`');
	if (c == ',') return('\'');
	if (c == '[') return('&');
	if (c == '.') return('$');

	return('\0');
}

static int NEAR detranschar(c)
int c;
{
	if (c == '`') return('+');
	if (c == '\'') return(',');
	if (c == '&') return('[');
	if (c == '$') return('.');

	return('\0');
}
#endif	/* !MSDOS */

static int NEAR sfntranschar(c)
int c;
{
	if ((u_char)c < ' ' || Xstrchr(NOTINLFN, c)) return(-2);
	if (Xstrchr(NOTINALIAS, c)) return(-1);
	if (Xstrchr(PACKINALIAS, c)) return(0);

	return(Xtoupper(c));
}

static int NEAR cmpdospath(path1, path2, len, part)
CONST char *path1, *path2;
int len, part;
{
	CONST char *cp;
	u_int w1, w2;
	int i, c1, c2;

	if (len < 0) len = strlen(path1);
	if ((cp = strrdelim(path1, 1))) cp++;
	else cp = path1;

	if (!isdotdir(cp))
		while (len > 0 && Xstrchr(PACKINALIAS, path1[len - 1])) len--;
	for (i = 0; i < len; i++) {
		c1 = (u_char)(path1[i]);
		c2 = (u_char)(path2[i]);
		if (!issjis1(c1) || !issjis2(c2)) {
			if (!c1 || !c2 || Xtoupper(c1) != Xtoupper(c2))
				return(c1 - c2);
		}
		else {
			if (++i >= len) return(-path2[i]);
			if (!path1[i] || !path2[i])
				return(path1[i] - path2[i]);

			w1 = unifysjis((c1 << 8) | (u_char)(path1[i]), 0);
			w2 = unifysjis((c2 << 8) | (u_char)(path2[i]), 0);
			if (w1 != w2) return(w1 - w2);
		}
	}
	if (!path2[i]) return(0);
	else if (part && path2[i] == _SC_) return(0);

	return(-path2[i]);
}

static char *NEAR getdosname(buf, name, ext)
char *buf;
CONST u_char *name, *ext;
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

static u_char *NEAR putdosname(buf, file, vol)
u_char *buf;
CONST char *file;
int vol;
{
	CONST char *cp, *eol;
	char num[7];
	u_int w;
	int i, j, c, cnv;

	if (isdotdir(file)) {
		Xstrcpy((char *)buf, file);
		return(buf);
	}

	for (i = strlen(file); i > 0; i--)
		if (!Xstrchr(PACKINALIAS, file[i - 1])) break;
	if (i <= 0) return(NULL);
	eol = &(file[i]);
	for (cp = eol - 1; cp > file; cp--) if (*cp == '.') break;
	if (cp <= file) cp = NULL;

	cnv = 0;
	for (i = 0; i < 8; i++) {
		if (file == cp || file == eol || !*file) buf[i] = ' ';
		else if (issjis1(*file) && issjis2(file[1])) {
			w = ((u_int)(*file) << 8) | (u_char)(file[1]);
			w = unifysjis(w, vol);
			buf[i++] = (w >> 8) & 0xff;
			buf[i] = w & 0xff;
			file += 2;
		}
#if	!MSDOS
		else if (!vol && (c = transchar(*file)) > 0) {
			buf[i] = c;
			file++;
		}
#endif
		else {
			if ((c = sfntranschar(*(file++))) > 0) buf[i] = c;
#if	MSDOS
			else if (c < -1) {
				dependdosfunc = 1;
				return(NULL);
			}
#endif
			else {
				cnv = 1;
				if (!c) i--;
				else buf[i] = '_';
			}
		}
	}

	if (file != cp && file < eol && *file) cnv = 1;
	if (cp) cp++;
	for (i = 8; i < 11; i++) {
		if (!cp || cp == eol || !*cp) buf[i] = ' ';
		else if (issjis1(*cp) && issjis2(cp[1])) {
			buf[i++] = *((u_char *)(cp++));
			buf[i] = *((u_char *)(cp++));
		}
#if	!MSDOS
		else if (!vol && (c = transchar(*cp)) > 0) {
			buf[i] = c;
			cp++;
		}
#endif
		else {
			if ((c = sfntranschar(*(cp++))) > 0) buf[i] = c;
#if	MSDOS
			else if (c < -1) {
				dependdosfunc = 1;
				return(NULL);
			}
#endif
			else {
				cnv = 1;
				if (!c) i--;
				else buf[i] = '_';
			}
		}
	}
	if (cp && cp < eol && *cp) cnv = 1;
	buf[i] = '\0';

	if (vol > 1 || (vol > 0 && cnv)) {
		for (j = 0; j < arraysize(num); j++) {
			if (!vol) break;
			num[j] = (vol % 10) + '0';
			vol /= 10;
		}
		for (i = arraysize(num) - j; i > 0; i--)
			if (buf[i - 1] != ' ') break;
		buf[i++] = '~';
		while (j-- > 0) buf[i++] = num[j];
	}
	if (buf[0] == 0xe5) buf[0] = 0x05;

	return(buf);
}

static u_int NEAR getdosmode(attr)
u_int attr;
{
	u_int mode;

	mode = 0;
	if (!(attr & DS_IHIDDEN)) mode |= S_IRUSR;
	if (!(attr & DS_IRDONLY)) mode |= S_IWUSR;
	mode |= (mode >> 3) | (mode >> 6);
#if	MSDOS
	if (attr & DS_IARCHIVE) mode |= S_ISVTX;
#endif
	if (attr & DS_IFDIR) mode |= (S_IFDIR | S_IEXEC_ALL);
	else if (attr & DS_IFLABEL) mode |= S_IFIFO;
	else if (attr & DS_IFSYSTEM) mode |= S_IFSOCK;
	else mode |= S_IFREG;

	return(mode);
}

static u_int NEAR putdosmode(mode)
u_int mode;
{
	u_int attr;

	attr = 0;
#if	MSDOS
	if (mode & S_ISVTX) attr |= DS_IARCHIVE;
#endif
	if (!(mode & S_IRUSR)) attr |= DS_IHIDDEN;
	if (!(mode & S_IWUSR)) attr |= DS_IRDONLY;
	if ((mode & S_IFMT) == S_IFDIR) attr |= DS_IFDIR;
	else {
#if	!MSDOS
		attr |= DS_IARCHIVE;
#endif
		if ((mode & S_IFMT) == S_IFIFO) attr |= DS_IFLABEL;
		else if ((mode & S_IFMT) == S_IFSOCK) attr |= DS_IFSYSTEM;
	}

	return(attr);
}

static time_t NEAR getdostime(d, t)
u_int d, t;
{
	struct tm tm;

	tm.tm_year = 1980 + ((d >> 9) & 0x7f);
	tm.tm_year -= 1900;
	tm.tm_mon = ((d >> 5) & 0x0f) - 1;
	tm.tm_mday = (d & 0x1f);
	tm.tm_hour = ((t >> 11) & 0x1f);
	tm.tm_min = ((t >> 5) & 0x3f);
	tm.tm_sec = ((t << 1) & 0x3e);

	return(Xtimelocal(&tm));
}

static int NEAR putdostime(buf, tim)
u_char *buf;
time_t tim;
{
#if	MSDOS
	struct timeb buffer;
#else
	struct timeval t_val;
	struct timezone tz;
#endif
	struct tm *tm;
	time_t tmp;
	u_int d, t;
	int mt;

	if (tim != (time_t)-1) {
		tmp = tim;
		mt = 0;
	}
	else {
#if	MSDOS
		ftime(&buffer);
		tmp = (time_t)(buffer.time);
		mt = (int)(buffer.millitm);
#else
		Xgettimeofday(&t_val, &tz);
		tmp = (time_t)(t_val.tv_sec);
		mt = (int)(t_val.tv_usec / 1000);
#endif
	}
	tm = localtime(&tmp);
	d = (((tm -> tm_year - 80) & 0x7f) << 9)
		+ (((tm -> tm_mon + 1) & 0x0f) << 5)
		+ (tm -> tm_mday & 0x1f);
	t = ((tm -> tm_hour & 0x1f) << 11)
		+ ((tm -> tm_min & 0x3f) << 5)
		+ ((tm -> tm_sec & 0x3e) >> 1);

	if (!t && tim == (time_t)-1) t = 0x0001;
	buf[0] = t & 0xff;
	buf[1] = (t >> 8) & 0xff;
	buf[2] = d & 0xff;
	buf[3] = (d >> 8) & 0xff;

	return(mt);
}

static int NEAR getdrive(path)
CONST char *path;
{
	int i, drive;

	if (!(drive = __dospath(path))) {
		doserrno = ENOENT;
		return(-1);
	}

	if (lastdrive < 0) {
		for (i = 0; i < 'Z' - 'A' + 1; i++) curdir[i] = NULL;
		lastdrive = drive;
	}

	return(drive);
}

static char *NEAR addpath(buf, ptr, path, len)
char *buf;
int ptr;
CONST char *path;
int len;
{
	char *cp;

	cp = &(buf[ptr]);
	if (len && path[len - 1] == '.') len--;
	if (ptr + 1 + len >= DOSMAXPATHLEN - 2 - 1) {
		doserrno = ENAMETOOLONG;
		return(NULL);
	}
	*(cp++) = _SC_;
	memcpy(cp, path, len);
	cp += len;

	return(cp);
}

static int NEAR parsepath(buf, path, class)
char *buf;
CONST char *path;
int class;
{
	char *cp;
	int i, drv, drive;

	cp = buf;
	if ((drive = getdrive(path)) < 0) return(-1);
	drv = Xtoupper(drive) - 'A';
	path += 2;
#if	MSDOS
	if (*path != '/' && *path != '\\') {
		*buf = _SC_;
		if (checkdrive(drv) <= 0) {
			if (!unixgetcurdir(&(buf[1]), drv + 1)) buf[1] = '\0';
		}
		else if (curdir[drv]) Xstrcpy(&(buf[1]), curdir[drv]);
		else *buf = '\0';
#else
	if (*path != '/' && *path != '\\' && curdir[drv] && *(curdir[drv])) {
		*buf = _SC_;
		Xstrcpy(&(buf[1]), curdir[drv]);
#endif
		cp += strlen(buf);
	}

	while (*path) {
		for (i = 0; path[i] && path[i] != '/' && path[i] != '\\'; i++)
			if (issjis1(path[i]) && issjis2(path[i + 1])) i++;
		if (class && !path[i]) {
			if ((i == 1 && path[0] == '.')
			|| (i == 2 && path[0] == '.' && path[1] == '.')) {
				*(cp++) = _SC_;
				memcpy(cp, path, i);
				cp += i;
			}
			else if (!(cp = addpath(buf, cp - buf, path, i)))
				return(-1);
		}
		else if (!i || (i == 1 && path[0] == '.')) /*EMPTY*/;
		else if (i == 2 && path[0] == '.' && path[1] == '.') {
			cp = strrdelim2(buf, cp);
			if (!cp) cp = buf;
		}
		else if (!(cp = addpath(buf, cp - buf, path, i))) return(-1);
		if (*(path += i)) path++;
	}
	if (cp == buf) *(cp++) = _SC_;
	else if (isdelim(buf, cp - 1 - buf)) cp--;
	*cp = '\0';

	return(drive);
}

static dosDIR *NEAR _dosopendir(path, resolved, needlfn)
CONST char *path;
char *resolved;
int needlfn;
{
	dosDIR *xdirp;
	struct dosdirent *dp;
	dent_t *dentp;
	cache_t *cache;
	CONST char *cp;
	char *cachepath, name[8 + 1 + 3 + 1], buf[DOSMAXPATHLEN - 2];
	int len, dd, drive, flen, rlen, duperrno;

	rlen = 0;
	duperrno = errno;
	if ((drive = parsepath(buf, path, 0)) < 0
	|| (dd = _dosopendev(drive)) < 0)
		return(NULL);

	path = buf + 1;
	if (!devlist[dd].dircache) {
		cache = NULL;
		cp = NULL;
	}
	else {
		cache = devlist[dd].dircache;
		cp = cache -> path;
	}
	if (!(devlist[dd].dircache = (cache_t *)malloc(sizeof(cache_t)))) {
		dosclosedev(dd);
		doserrno = errno;
		errno = duperrno;
		return(NULL);
	}
	devlist[dd].flags |= F_CACHE;
	cachepath = dd2path(dd);

	if (!(xdirp = (dosDIR *)malloc(sizeof(dosDIR)))) {
		dosclosedev(dd);
		doserrno = errno;
		errno = duperrno;
		return(NULL);
	}
	xdirp -> dd_id = DID_IFDOSDRIVE;
	xdirp -> dd_fd = dd;
	xdirp -> dd_top = xdirp -> dd_off = 0L;
	xdirp -> dd_loc = 0L;
	xdirp -> dd_size = (long)(devlist[dd].clustsize)
		* (long)(devlist[dd].sectsize);
	if (!(xdirp -> dd_buf = (char *)malloc(xdirp -> dd_size))) {
		VOID_C _dosclosedir(xdirp);
		doserrno = errno;
		errno = duperrno;
		return(NULL);
	}

	if (resolved) rlen = gendospath(resolved, drive, _SC_) - resolved;

	if (cp && (len = strlen(cp)) > 0) {
		if (isdelim(cp, len - 1)) len--;
		if (!cmpdospath(cp, path, len, 1)) {
			xdirp -> dd_top = xdirp -> dd_off =
				clust32(&(devlist[dd]), &(cache -> dent));
			memcpy((char *)devlist[dd].dircache,
				(char *)cache, sizeof(*cache));
			cachepath += len;
			*(cachepath++) = _SC_;
			path += len;
			if (*path) path++;
			if (resolved) {
				if (rlen > 3) resolved[rlen++] = _SC_;
				memcpy(&(resolved[rlen]), cp, len);
				rlen += len;
				resolved[rlen] = '\0';
			}
		}
	}
	*cachepath = '\0';

	while (*path) {
		cp = path;
		if ((path = strdelim(path, 0))) len = (path++) - cp;
		else {
			len = strlen(cp);
			path = cp + len;
		}
		if (!len || (len == 1 && *cp == '.')) continue;

		if (!(dp = finddirent(xdirp, cp, len, needlfn))) {
			errno = (doserrno) ? doserrno : ENOENT;
			VOID_C _dosclosedir(xdirp);
			doserrno = errno;
			errno = duperrno;
			return(NULL);
		}
		if (resolved) {
			if (needlfn) cp = dp -> d_name;
			else {
				getdosname(name,
					dd2dentp(xdirp -> dd_fd) -> name,
					dd2dentp(xdirp -> dd_fd) -> ext);
				cp = name;
			}
			flen = strlen(cp);
			if (rlen + 1 + flen >= DOSMAXPATHLEN) {
				VOID_C _dosclosedir(xdirp);
				doserrno = ENAMETOOLONG;
				errno = duperrno;
				return(NULL);
			}
			if (rlen > 3) resolved[rlen++] = _SC_;
			Xstrcpy(&(resolved[rlen]), cp);
			rlen += flen;
		}

		dentp = (dent_t *)&(xdirp -> dd_buf[dp -> d_fileno]);
		xdirp -> dd_top = xdirp -> dd_off =
			clust32(&(devlist[dd]), dentp);
		xdirp -> dd_loc = 0L;
		cachepath += len;
		*(cachepath++) = _SC_;
		*cachepath = '\0';
	}

	if (*(dd2path(dd)) && !(dd2dentp(dd) -> attr & DS_IFDIR)) {
		VOID_C _dosclosedir(xdirp);
		doserrno = ENOTDIR;
		errno = duperrno;
		return(NULL);
	}
	errno = duperrno;

	return(xdirp);
}

static int NEAR _dosclosedir(xdirp)
dosDIR *xdirp;
{
	int n, duperrno;

	duperrno = errno;
	if (xdirp -> dd_id != DID_IFDOSDRIVE) return(0);
	if (xdirp -> dd_buf) free(xdirp -> dd_buf);
	n = _dosclosedev(xdirp -> dd_fd);
	free(xdirp);
	errno = duperrno;

	return(n);
}

DIR *dosopendir(path)
CONST char *path;
{
	dosDIR *xdirp;

	if (!(xdirp = _dosopendir(path, NULL, 0))) {
		errno = doserrno;
		return(NULL);
	}

	return((DIR *)xdirp);
}

int dosclosedir(dirp)
DIR *dirp;
{
	if (_dosclosedir((dosDIR *)dirp) < 0) return(seterrno(doserrno));

	return(0);
}

static int NEAR seekdent(xdirp, dentp, clust, offset)
dosDIR *xdirp;
dent_t *dentp;
long clust, offset;
{
	long next;

	if (xdirp -> dd_id != DID_IFDOSDRIVE) {
		doserrno = EINVAL;
		return(-1);
	}
	xdirp -> dd_off = clust;
	next = clustread(&(devlist[xdirp -> dd_fd]),
		(u_char *)(xdirp -> dd_buf), xdirp -> dd_off);
	if (next < 0L) return(-1);
	dd2clust(xdirp -> dd_fd) = xdirp -> dd_off;
	xdirp -> dd_off = next;
	dd2offset(xdirp -> dd_fd) = xdirp -> dd_loc = offset;
	if (dentp)
		memcpy((char *)dentp,
			(char *)&(xdirp -> dd_buf[xdirp -> dd_loc]),
			sizeof(*dentp));
	xdirp -> dd_loc += DOSDIRENT;
	if (xdirp -> dd_loc >= xdirp -> dd_size) xdirp -> dd_loc = 0L;

	return(0);
}

static int NEAR readdent(xdirp, dentp, force)
dosDIR *xdirp;
dent_t *dentp;
int force;
{
	long next;

	if (xdirp -> dd_id != DID_IFDOSDRIVE) {
		doserrno = EINVAL;
		return(-1);
	}
	if (!xdirp -> dd_loc) {
		next = clustread(&(devlist[xdirp -> dd_fd]),
			(u_char *)(xdirp -> dd_buf), xdirp -> dd_off);
		if (next < 0L) return(-1);
		dd2clust(xdirp -> dd_fd) = xdirp -> dd_off;
		xdirp -> dd_off = next;
	}

	dd2offset(xdirp -> dd_fd) = xdirp -> dd_loc;
	memcpy((char *)dentp,
		(char *)&(xdirp -> dd_buf[xdirp -> dd_loc]), sizeof(*dentp));
	if (force || dentp -> name[0]) {
		xdirp -> dd_loc += DOSDIRENT;
		if (xdirp -> dd_loc >= xdirp -> dd_size) xdirp -> dd_loc = 0L;
	}

	return(0);
}

static struct dosdirent *NEAR _dosreaddir(xdirp, all)
dosDIR *xdirp;
int all;
{
	dent_t *dentp;
	static st_dosdirent d;
	struct dosdirent *dp;
	char *cp, buf[LFNENTSIZ * 2 + 1];
	long loc, clust, offset;
	u_int ch;
	int i, j, cnt, sum;

	dp = (struct dosdirent *)&d;
	dentp = dd2dentp(xdirp -> dd_fd);
	dp -> d_name[0] = '\0';
	dp -> d_reclen = DOSDIRENT;
	cnt = -1;
	clust = -1;
	offset = 0L;
	sum = -1;
	for (;;) {
		loc = xdirp -> dd_loc;
		if (readdent(xdirp, dentp, 0) < 0 || !(dentp -> name[0])) {
			*dd2path(xdirp -> dd_fd) = '\0';
			doserrno = 0;
			cnvunicode(0, -1);
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
		if (cnt > 0 && cnt == *cp + 1 && sum == dentp -> checksum)
			cnt--;
		else {
			dp -> d_name[0] = '\0';
			dp -> d_reclen = DOSDIRENT;
			if ((cnt = *cp - '@') < 0) continue;
			lfn_clust = dd2clust(xdirp -> dd_fd);
			lfn_offset = dd2offset(xdirp -> dd_fd);
			sum = dentp -> checksum;
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
			memmove(&(dp -> d_name[j]),
				dp -> d_name, strlen(dp -> d_name) + 1);
			memcpy(dp -> d_name, buf, j);
		}
		clust = xdirp -> dd_off;
		offset = xdirp -> dd_loc;
	}
	dp -> d_fileno = loc;
	if (cnt != 1) dp -> d_name[0] = '\0';
	else if (sum != calcsum(dd2dentp(xdirp -> dd_fd) -> name)) {
		cnt = -1;
		dp -> d_name[0] = '\0';
	}
	if (cnt <= 0) lfn_clust = -1;
	if (!dp -> d_name[0]) {
		getdosname(dp -> d_name, dd2dentp(xdirp -> dd_fd) -> name,
			dd2dentp(xdirp -> dd_fd) -> ext);
#if	MSDOS
		dp -> d_alias[0] = '\0';
#endif
	}
	else {
#if	MSDOS
		getdosname(dp -> d_alias, dd2dentp(xdirp -> dd_fd) -> name,
			dd2dentp(xdirp -> dd_fd) -> ext);
#endif
		if (all) {
			xdirp -> dd_off = clust;
			xdirp -> dd_loc = offset;
		}
	}
	if ((cp = strrdelim(dd2path(xdirp -> dd_fd), 0))) cp++;
	else cp = dd2path(xdirp -> dd_fd);
	Xstrcpy(cp, dp -> d_name);
	cnvunicode(0, -1);

	return(dp);
}

struct dosdirent *dosreaddir(dirp)
DIR *dirp;
{
	dosDIR *xdirp;
	struct dosdirent *dp;
	int i;
#if	!MSDOS
	int c;
#endif

	xdirp = (dosDIR *)dirp;
	if (!(dp = _dosreaddir(xdirp, 0))) {
		errno = doserrno;
		return(NULL);
	}
	if (!(devlist[xdirp -> dd_fd].flags & F_VFAT)) {
		for (i = 0; dp -> d_name[i]; i++) {
			if (issjis1(dp -> d_name[i])
			&& issjis2(dp -> d_name[i + 1]))
				i++;
#if	!MSDOS
			else if ((c = detranschar(dp -> d_name[i])))
				dp -> d_name[i] = c;
			else dp -> d_name[i] = Xtolower(dp -> d_name[i]);
#endif
		}
	}

	return(dp);
}

static struct dosdirent *NEAR finddirent(xdirp, fname, len, needlfn)
dosDIR *xdirp;
CONST char *fname;
int len, needlfn;
{
	struct dosdirent *dp;
	u_char dosname[8 + 3 + 1];
	char tmp[8 + 1 + 3 + 1];

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
			if (!cmpdospath(tmp, dp -> d_name, len, 0)) return(dp);
	}

	return(NULL);
}

int dosrewinddir(dirp)
DIR *dirp;
{
	dosDIR *xdirp;

	xdirp = (dosDIR *)dirp;
	if (xdirp -> dd_id != DID_IFDOSDRIVE) return(seterrno(EINVAL));
	xdirp -> dd_loc = 0L;
	xdirp -> dd_off = xdirp -> dd_top;

	return(0);
}

int doschdir(path)
CONST char *path;
{
	dosDIR *xdirp;
	char *tmp, buf[DOSMAXPATHLEN];
	int drv, drive, needlfn;

	if ((drive = getdrive(path)) < 0) return(seterrno(doserrno));
	drv = Xtoupper(drive);

	needlfn = Xislower(drive);
	if (!(xdirp = _dosopendir(path, buf, needlfn))) {
		if (path[2] == '/' || path[2] == '\\'
		|| !(tmp = curdir[drv - 'A']))
			return(seterrno(doserrno));
		curdir[drv - 'A'] = NULL;
		xdirp = _dosopendir(path, buf, needlfn);
		curdir[drv - 'A'] = tmp;
		if (!xdirp) return(seterrno(doserrno));
	}
	VOID_C _dosclosedir(xdirp);

	if (!buf[2]) buf[3] = '\0';
	if (!(tmp = (char *)malloc(strlen(&(buf[3])) + 1))) return(-1);
	if (curdir[drv - 'A']) free(curdir[drv - 'A']);
	curdir[drv - 'A'] = tmp;
	Xstrcpy(curdir[drv - 'A'], &(buf[3]));
	lastdrive = drive;

	return(0);
}

char *dosgetcwd(pathname, size)
char *pathname;
int size;
{
	char *cp;
	int i;

	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);

	i = Xtoupper(lastdrive) - 'A';
	if (lastdrive < 0) {
		errno = EFAULT;
		return(NULL);
	}
	if (!(cp = curdir[i])) {
		errno = EFAULT;
		return(NULL);
	}

	if (strlen(cp) + 3 > size) {
		errno = ERANGE;
		return(NULL);
	}
	Xstrcpy(gendospath(pathname, lastdrive, _SC_), cp);

	return(pathname);
}

static dosDIR *NEAR splitpath(pathp, resolved, needlfn)
CONST char **pathp;
char *resolved;
int needlfn;
{
	char dir[DOSMAXPATHLEN];
	int i, j;

	Xstrcpy(dir, *pathp);
	for (i = 1, j = 2; dir[j]; j++) {
		if (issjis1(dir[j]) && issjis2(dir[j + 1])) j++;
		else if (dir[j] == '/' || dir[j] == '\\') i = j;
	}

	*pathp += i + 1;
	if (i < 3) dir[i++] = _SC_;
	dir[i] = '\0';

	if (i <= 3 && isdotdir(*pathp)) {
		doserrno = ENOENT;
		return(NULL);
	}

	return(_dosopendir(dir, resolved, needlfn));
}

static int NEAR getdent(path, ddp)
CONST char *path;
int *ddp;
{
	dosDIR *xdirp;
	char buf[DOSMAXPATHLEN];
	int dd, drive, duperrno;

	if (ddp) *ddp = -1;
	if ((drive = parsepath(&(buf[2]), path, 1)) < 0
	|| (dd = _dosopendev(drive)) < 0)
		return(-1);
	if (devlist[dd].dircache
	&& buf[2] && buf[3] && !cmpdospath(dd2path(dd), &(buf[3]), -1, 0))
		return(dd);
	buf[0] = drive;
	buf[1] = ':';

	duperrno = errno;
	path = buf;
	if (!(xdirp = splitpath(&path, NULL, 0))) {
		errno = (doserrno) ? doserrno : ENOTDIR;
		dosclosedev(dd);
		doserrno = errno;
		errno = duperrno;
		return(-1);
	}
	if (!(devlist[dd].dircache = (cache_t *)malloc(sizeof(cache_t)))) {
		VOID_C _dosclosedir(xdirp);
		dosclosedev(dd);
		doserrno = errno;
		errno = duperrno;
		return(-1);
	}
	devlist[dd].flags |= F_CACHE;

	memcpy((char *)(devlist[dd].dircache),
		(char *)(devlist[xdirp -> dd_fd].dircache), sizeof(cache_t));
	if (!*path || (isdotdir(path) && path <= &(buf[4]))) {
		VOID_C _dosclosedir(xdirp);
		dd2dentp(dd) -> attr |= DS_IFDIR;
		errno = duperrno;
		return(dd);
	}
	if (!finddirent(xdirp, path, 0, 0)) {
		errno = (doserrno) ? doserrno : ENOENT;
		VOID_C _dosclosedir(xdirp);
		if (ddp && !doserrno) *ddp = dd;
		else dosclosedev(dd);
		doserrno = errno;
		errno = duperrno;
		return(-1);
	}

	memcpy((char *)(devlist[dd].dircache),
		(char *)(devlist[xdirp -> dd_fd].dircache), sizeof(cache_t));
	VOID_C _dosclosedir(xdirp);
	errno = duperrno;

	return(dd);
}

static int NEAR writedent(dd)
int dd;
{
	u_char *buf;
	u_long sect;
	long offset;
	int n;

	if (!(sect = clust2sect(&(devlist[dd]), dd2clust(dd)))) {
		doserrno = EIO;
		return(-1);
	}
	offset = dd2offset(dd);
	while (offset >= (long)(devlist[dd].sectsize)) {
		sect++;
		offset -= (long)(devlist[dd].sectsize);
	}
	if (!(buf = (u_char *)malloc(devlist[dd].sectsize))) return(-1);
	n = 0;
	if (sectread(&(devlist[dd]), sect, buf, 1) < 0L) n = -1;
	else {
		memcpy((char *)&(buf[offset]),
			(char *)dd2dentp(dd), sizeof(dent_t));
		if (sectwrite(&(devlist[dd]), sect, buf, 1) < 0L) n = -1;
	}
	free(buf);

	return(n);
}

static int NEAR expanddent(dd)
int dd;
{
	long prev;

	prev = dd2clust(dd);
	if (!(devlist[dd].flags & F_FAT32) && (!prev || prev >= ROOTCLUST)) {
		doserrno = ENOSPC;
		return(-1);
	}
	if ((dd2clust(dd) = clustexpand(&(devlist[dd]), prev, 1)) < 0L)
		return(-1);
	memset((char *)dd2dentp(dd), 0, sizeof(dent_t));
	dd2offset(dd) = 0L;

	return(0);
}

static int NEAR creatdent(path, mode)
CONST char *path;
int mode;
{
	dosDIR *xdirp;
	dent_t *dentp;
	u_char *cp, fname[8 + 3 + 1], longfname[DOSMAXNAMLEN + 1];
	CONST char *file;
	char tmp[8 + 1 + 3 + 1], buf[DOSMAXPATHLEN];
	long clust, offset;
	u_int c;
	int i, j, n, len, cnt, sum, lfn;

	if ((n = parsepath(&(buf[2]), path, 1)) < 0) return(-1);
	buf[0] = n;
	buf[1] = ':';
	file = buf;

	if (!(xdirp = splitpath(&file, NULL, 0))) {
		if (doserrno == ENOENT) doserrno = ENOTDIR;
		return(-1);
	}
	if (!*file) {
		VOID_C _dosclosedir(xdirp);
		doserrno = EEXIST;
		return(-1);
	}
	if (isdotdir(file)) {
		VOID_C _dosclosedir(xdirp);
		doserrno = EACCES;
		return(-1);
	}

	if (!(devlist[xdirp -> dd_fd].flags & F_VFAT)) {
		lfn = 0;
		n = 0;
		cnt = 1;
		len = 0;
	}
	else {
		lfn = 1;
		n = -1;
		for (i = j = 0; file[i]; i++, j += 2) {
			if (!Xstrchr(PACKINALIAS, file[i])) n = -1;
			else if (n < 0) n = i;
			if (!issjis1(file[i]) || !issjis2(file[i + 1]))
				c = lfnencode(0, file[i]);
			else {
				c = lfnencode(file[i], file[i + 1]);
				lfn = 2;
				i++;
			}
			longfname[j] = c & 0xff;
			longfname[j + 1] = (c >> 8) & 0xff;
		}
		cnvunicode(0, -1);
		if (n >= 0) j = n * 2;
		n = 1;
		cnt = j / (LFNENTSIZ * 2);
		len = j;
		if (j > cnt * LFNENTSIZ * 2) {
			longfname[j] = longfname[j + 1] = '\0';
			len += 2;
			cnt++;
		}
		cnt++;
	}

	sum = -1;
	dentp = dd2dentp(xdirp -> dd_fd);
	if (!putdosname(fname, file, n)) {
		VOID_C _dosclosedir(xdirp);
		doserrno = EACCES;
		return(-1);
	}

	for (i = 0; i < INHIBITNAMESIZ; i++)
		if (!strncmp((char *)fname, inhibitname[i], 8)) break;
	if (i < INHIBITNAMESIZ
	|| (!strncmp((char *)fname, INHIBITCOM, strsize(INHIBITCOM))
	&& fname[strsize(INHIBITCOM)] > '0'
	&& fname[strsize(INHIBITCOM)] <= '0' + INHIBITCOMMAX
	&& fname[strsize(INHIBITCOM) + 1] == ' ')
	|| (!strncmp((char *)fname, INHIBITLPT, strsize(INHIBITLPT))
	&& fname[strsize(INHIBITLPT)] > '0'
	&& fname[strsize(INHIBITLPT)] <= '0' + INHIBITLPTMAX
	&& fname[strsize(INHIBITLPT) + 1] == ' ')) {
		VOID_C _dosclosedir(xdirp);
		doserrno = EINVAL;
		return(-1);
	}

	if (lfn == 1 && !strcmp(getdosname(tmp, fname, fname + 8), file)) {
		lfn = 0;
		cnt = 1;
	}
	if (lfn) sum = calcsum(fname);

	i = j = 0;
	clust = -1;
	offset = 0L;
	for (;;) {
		if (readdent(xdirp, dentp, 1) < 0) {
			if (clust >= 0) break;
			if (doserrno || expanddent(xdirp -> dd_fd) < 0) {
				*dd2path(xdirp -> dd_fd) = '\0';
				VOID_C _dosclosedir(xdirp);
				return(-1);
			}
			clust = dd2clust(xdirp -> dd_fd);
			offset = 0L;
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

		if (!strncmp((char *)(dentp -> name), (char *)fname, 8 + 3)) {
			if (!n) {
				*dd2path(xdirp -> dd_fd) = '\0';
				VOID_C _dosclosedir(xdirp);
				doserrno = EEXIST;
				return(-1);
			}
			n++;
			if (!putdosname(fname, file, n)) {
				*dd2path(xdirp -> dd_fd) = '\0';
				VOID_C _dosclosedir(xdirp);
				doserrno = EACCES;
				return(-1);
			}
			sum = calcsum(fname);
			i = j = 0;
			clust = -1;
			dosrewinddir((DIR *)xdirp);
		}
	}

	if (clust >= 0 && seekdent(xdirp, NULL, clust, offset) < 0) {
		*dd2path(xdirp -> dd_fd) = '\0';
		VOID_C _dosclosedir(xdirp);
		return(-1);
	}
	cp = (u_char *)dentp;
	while (--cnt > 0) {
		memset((char *)dentp, 0, sizeof(*dentp));
		cp[0] = cnt;
		n = (cnt - 1) * (LFNENTSIZ * 2);
		if (n + (LFNENTSIZ * 2) >= len) cp[0] += '@';
		dentp -> checksum = sum;
		dentp -> attr = DS_IFLFN;
		for (i = 1, j = 0; i < DOSDIRENT; i += 2, j += 2) {
			if (cp + i == (u_char *)&(dentp -> attr)) i += 3;
			else if (cp + i == (u_char *)(dentp -> clust)) i += 2;
			if (n + j >= len) cp[i] = cp[i + 1] = 0xff;
			else {
				cp[i] = longfname[n + j];
				cp[i + 1] = longfname[n + j + 1];
			}
		}
		if (writedent(xdirp -> dd_fd) < 0
		|| (readdent(xdirp, dentp, 1) < 0
		&& (doserrno || expanddent(xdirp -> dd_fd) < 0))) {
			*dd2path(xdirp -> dd_fd) = '\0';
			VOID_C _dosclosedir(xdirp);
			return(-1);
		}
	}

	memset((char *)dentp, 0, sizeof(*dentp));
	memcpy(dentp -> name, (char *)fname, 8 + 3);
	dentp -> attr = putdosmode((u_short)mode) | DS_IARCHIVE;
	i = putdostime(dentp -> time, -1);
	if (devlist[xdirp -> dd_fd].flags & F_VFAT) {
		dentp -> checksum = i / 10;
		dentp -> ctime[0] = dentp -> time[0];
		dentp -> ctime[1] = dentp -> time[1];
		dentp -> cdate[0] = dentp -> adate[0] = dentp -> date[0];
		dentp -> cdate[1] = dentp -> adate[1] = dentp -> date[1];
	}
	if (writedent(xdirp -> dd_fd) < 0
	|| writefat(&(devlist[xdirp -> dd_fd])) < 0)
		n = -1;
	else {
		n = xdirp -> dd_fd;
		if ((cp = (u_char *)strrdelim(dd2path(n), 0))) cp++;
		else cp = (u_char *)dd2path(n);
		Xstrcpy((char *)cp, file);
	}
	free(xdirp -> dd_buf);
	free(xdirp);

	return(n);
}

static int NEAR unlinklfn(dd, clust, offset, sum)
int dd;
long clust, offset;
int sum;
{
	dosDIR xdir;
	long dupclust, dupoffset;

	if (clust < 0) return(0);
	dupclust = dd2clust(dd);
	dupoffset = dd2offset(dd);
	sum &= 0xff;
	xdir.dd_id = DID_IFDOSDRIVE;
	xdir.dd_fd = dd;
	xdir.dd_loc = 0L;
	xdir.dd_size = (long)(devlist[dd].clustsize)
		* (long)(devlist[dd].sectsize);
	xdir.dd_top = xdir.dd_off = clust;
	if (!(xdir.dd_buf = (char *)malloc(xdir.dd_size))) return(-1);

	seekdent(&xdir, dd2dentp(dd), clust, offset);
	*(dd2dentp(dd) -> name) = 0xe5;
	writedent(dd);
	while (readdent(&xdir, dd2dentp(dd), 0) >= 0) {
		if (!dd2dentp(dd) -> name[0] || dd2dentp(dd) -> name[0] == 0xe5
		|| (dd2clust(dd) == dupclust && dd2offset(dd) == dupoffset)
		|| dd2dentp(dd) -> attr != 0x0f
		|| dd2dentp(dd) -> checksum != sum)
			break;
		*(dd2dentp(dd) -> name) = 0xe5;
		writedent(dd);
	}
	free(xdir.dd_buf);

	return(0);
}

#if	MSDOS
char *dosshortname(path, alias)
CONST char *path;
char *alias;
{
	dosDIR *xdirp;
	char *cp, buf[DOSMAXPATHLEN], name[8 + 1 + 3 + 1];
	int dd, drive;

	name[0] = '\0';
	if ((drive = parsepath(&(buf[2]), path, 1)) < 0
	|| (dd = _dosopendev(drive)) < 0) {
		errno = doserrno;
		return(NULL);
	}
	if (devlist[dd].dircache
	&& buf[2] && buf[3] && !cmpdospath(dd2path(dd), &(buf[3]), -1, 0))
		getdosname(name, dd2dentp(dd) -> name, dd2dentp(dd) -> ext);
	buf[0] = drive;
	buf[1] = ':';
	path = buf;

	*alias = '\0';
	if (!(xdirp = splitpath(&path, alias, 0))) {
		errno = (doserrno) ? doserrno : ENOTDIR;
		dosclosedev(dd);
		return(NULL);
	}
	cp = strcatdelim(alias);
	if (!*path);
	else if (*name) Xstrcpy(cp, name);
	else if (isdotdir(path)) Xstrcpy(cp, path);
	else if (finddirent(xdirp, path, 0, 0))
		getdosname(cp, dd2dentp(xdirp -> dd_fd) -> name,
			dd2dentp(xdirp -> dd_fd) -> ext);
	else {
		errno = (doserrno) ? doserrno : ENOENT;
		alias = NULL;
	}
	VOID_C _dosclosedir(xdirp);
	dosclosedev(dd);

	return(alias);
}

char *doslongname(path, resolved)
CONST char *path;
char *resolved;
{
	dosDIR *xdirp;
	struct dosdirent *dp;
	char *cp, buf[DOSMAXPATHLEN];
	int dd, drive;

	if ((drive = parsepath(&(buf[2]), path, 1)) < 0
	|| (dd = _dosopendev(drive)) < 0) {
		errno = doserrno;
		return(NULL);
	}
	buf[0] = drive;
	buf[1] = ':';
	path = buf;

	*resolved = '\0';
	if (!(xdirp = splitpath(&path, resolved, 1))) {
		errno = (doserrno) ? doserrno : ENOTDIR;
		dosclosedev(dd);
		return(NULL);
	}
	cp = strcatdelim(resolved);
	if (!*path);
	else if ((dp = finddirent(xdirp, path, 0, 1)))
		Xstrcpy(cp, dp -> d_name);
	else {
		errno = (doserrno) ? doserrno : ENOENT;
		resolved = NULL;
	}
	VOID_C _dosclosedir(xdirp);
	dosclosedev(dd);

	return(resolved);
}
#endif	/* MSDOS */

int dosstatfs(drive, buf)
int drive;
char *buf;
{
	long block, total, avail;
	int dd;

	if ((dd = _dosopendev(drive)) < 0) return(seterrno(doserrno));
	block = (long)(devlist[dd].clustsize) * (long)(devlist[dd].sectsize);
	total = (long)(devlist[dd].totalsize);
	if (availfat(&(devlist[dd])) < 0) {
		dosclosedev(dd);
		return(seterrno(doserrno));
	}
	avail = (long)(devlist[dd].availsize);

	*((long *)&(buf[0 * sizeof(long)])) = block;
	*((long *)&(buf[1 * sizeof(long)])) = total;
	*((long *)&(buf[2 * sizeof(long)])) = avail;
	buf[3 * sizeof(long)] = 0;
	if (devlist[dd].flags & F_FAT32) buf[3 * sizeof(long)] |= 001;
	dosclosedev(dd);

	return(0);
}

int dosstat(path, stp)
CONST char *path;
struct stat *stp;
{
	char *cp;
	int dd;

	if ((dd = getdent(path, NULL)) < 0) return(seterrno(doserrno));
	stp -> st_dev = dd;
	stp -> st_ino = clust32(&(devlist[dd]), dd2dentp(dd));
	stp -> st_mode = getdosmode(dd2dentp(dd) -> attr);
	stp -> st_nlink = 1;
	stp -> st_uid = (uid_t)-1;
	stp -> st_gid = (gid_t)-1;
	stp -> st_size = byte2dword(dd2dentp(dd) -> size);
	stp -> st_atime =
	stp -> st_mtime =
	stp -> st_ctime = getdostime(byte2word(dd2dentp(dd) -> date),
		byte2word(dd2dentp(dd) -> time));
#if	!MSDOS && defined (UF_SETTABLE) && defined (SF_SETTABLE)
	stp -> st_flags = (u_long)0;
#endif
	dosclosedev(dd);

	if ((stp -> st_mode & S_IFMT) != S_IFDIR
	&& (cp = Xstrrchr(path, '.')) && strlen(++cp) == 3) {
		if (!Xstrcasecmp(cp, EXTCOM)
		|| !Xstrcasecmp(cp, EXTEXE) || !Xstrcasecmp(cp, EXTBAT))
			stp -> st_mode |= S_IEXEC_ALL;
	}

	return(0);
}

int doslstat(path, stp)
CONST char *path;
struct stat *stp;
{
	return(dosstat(path, stp));
}

int dosaccess(path, mode)
CONST char *path;
int mode;
{
	struct stat st;
	char buf[DOSMAXPATHLEN - 2];
	int dd, drive;

	if (dosstat(path, &st) < 0) {
		if (errno != ENOENT) return(-1);
		if ((drive = parsepath(buf, path, 1)) < 0
		|| (dd = _dosopendev(drive)) < 0)
			return(seterrno(ENOENT));
		dosclosedev(dd);
		if (*buf != _SC_ || isdotdir(&(buf[1])) != 2)
			return(seterrno(ENOENT));
		st.st_dev = dd;
		st.st_mode = (S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR);
	}

	if (((mode & R_OK) && !(st.st_mode & S_IRUSR))
	|| ((mode & W_OK)
		&& (!(st.st_mode & S_IWUSR)
		|| (devlist[st.st_dev].flags & F_RONLY)))
	|| ((mode & X_OK) && !(st.st_mode & S_IXUSR)))
		return(seterrno(EACCES));

	return(0);
}

#ifndef	NOSYMLINK
/*ARGSUSED*/
int dossymlink(name1, name2)
CONST char *name1, *name2;
{
	return(seterrno(EACCES));
}

/*ARGSUSED*/
int dosreadlink(path, buf, bufsiz)
CONST char *path;
char *buf;
int bufsiz;
{
	return(seterrno(EINVAL));
}
#endif	/* !NOSYMLINK */

int doschmod(path, mode)
CONST char *path;
int mode;
{
	int n, dd;

	if ((dd = getdent(path, NULL)) < 0) return(seterrno(doserrno));
	dd2dentp(dd) -> attr = putdosmode((u_short)mode);
	if ((n = writedent(dd)) < 0) errno = doserrno;
	dosclosedev(dd);

	return(n);
}

#ifdef	USEUTIME
int dosutime(path, times)
CONST char *path;
CONST struct utimbuf *times;
{
	time_t t = times -> modtime;
#else
int dosutimes(path, tvp)
CONST char *path;
CONST struct timeval *tvp;
{
	time_t t = tvp[1].tv_sec;
#endif
	int n, dd;

	if ((dd = getdent(path, NULL)) < 0) return(seterrno(doserrno));
	putdostime(dd2dentp(dd) -> time, t);
	if ((n = writedent(dd)) < 0) errno = doserrno;
	dosclosedev(dd);

	return(n);
}

int dosunlink(path)
CONST char *path;
{
	int n, dd, sum;

	if ((dd = getdent(path, NULL)) < 0) return(seterrno(doserrno));
	if (dd2dentp(dd) -> attr & DS_IFDIR) return(seterrno(EPERM));
	sum = calcsum(dd2dentp(dd) -> name);
	*(dd2dentp(dd) -> name) = 0xe5;
	*dd2path(dd) = '\0';

	n = 0;
	if (writedent(dd) < 0
	|| clustfree(&(devlist[dd]), clust32(&(devlist[dd]), dd2dentp(dd))) < 0
	|| unlinklfn(dd, lfn_clust, lfn_offset, sum) < 0
	|| writefat(&(devlist[dd])) < 0) {
		errno = doserrno;
		n = -1;
	}
	dosclosedev(dd);

	return(n);
}

int dosrename(from, to)
CONST char *from, *to;
{
	char buf[DOSMAXPATHLEN];
	long clust, rmclust, offset;
	int i, n, dd, fd, sum;

	if ((i = parsepath(buf, to, 0)) < 0) return(seterrno(doserrno));
	if (Xtoupper(i) != Xtoupper(getdrive(from))) return(seterrno(EXDEV));
	if ((dd = getdent(from, NULL)) < 0) return(seterrno(doserrno));

	clust = lfn_clust;
	offset = lfn_offset;

	i = strlen(dd2path(dd));
	if (buf[0] && buf[1] && !cmpdospath(dd2path(dd), &(buf[1]), i, 1)) {
		dosclosedev(dd);
		return(seterrno(EINVAL));
	}
	if (*(dd2dentp(dd) -> name) == '.') {
		dosclosedev(dd);
		return(seterrno(EACCES));
	}
	if ((fd = dosopen(to, O_BINARY | O_RDWR | O_CREAT, 0666)) < 0) {
		dosclosedev(dd);
		return(-1);
	}
	fd -= DOSFDOFFSET;
	rmclust = clust32(fd2devp(fd), fd2dentp(fd));
	sum = calcsum(dd2dentp(dd) -> name);
	memcpy((char *)&(fd2dentp(fd) -> attr),
		(char *)&(dd2dentp(dd) -> attr),
		(int)sizeof(dent_t) - (8 + 3));
	*(dd2dentp(dd) -> name) = 0xe5;
	*dd2path(dd) = '\0';

	free(dosflist[fd]._base);
	dosflist[fd]._base = NULL;
	while (maxdosf > 0 && !dosflist[maxdosf - 1]._base) maxdosf--;
	fd2clust(fd) = dosflist[fd]._clust;
	fd2offset(fd) = dosflist[fd]._offset;

	n = 0;
	if (writedent(dosflist[fd]._file) < 0 || writedent(dd) < 0
	|| unlinklfn(dd, clust, offset, sum) < 0
	|| (rmclust && clustfree(&(devlist[dd]), rmclust) < 0)
	|| writefat(&(devlist[dd])) < 0) {
		errno = doserrno;
		n = -1;
	}
	dosclosedev(dosflist[fd]._file);
	dosclosedev(dd);

	return(n);
}

int dosopen(path, flags, mode)
CONST char *path;
int flags, mode;
{
	int fd, dd, tmp;

	for (fd = 0; fd < maxdosf; fd++) if (!dosflist[fd]._base) break;
	if (fd >= DOSNOFILE) return(seterrno(EMFILE));

	if ((dd = getdent(path, &tmp)) < 0) {
		if (doserrno != ENOENT || tmp < 0 || !(flags & O_CREAT)
		|| (dd = creatdent(path, mode & 0777)) < 0) {
			errno = doserrno;
			if (tmp >= 0) dosclosedev(tmp);
			return(-1);
		}
		else {
			if (!(devlist[tmp].flags & F_DUPL))
				devlist[dd].flags &= ~F_DUPL;
			devlist[tmp].flags |= F_DUPL;
			dosclosedev(tmp);
		}
	}
	else if ((flags & O_CREAT) && (flags & O_EXCL)) {
		dosclosedev(dd);
		return(seterrno(EEXIST));
	}

	if ((flags & O_ACCMODE) != O_RDONLY) {
		tmp = 0;
		if (devlist[dd].flags & F_RONLY) tmp = EROFS;
		else if (dd2dentp(dd) -> attr & DS_IFDIR) tmp = EISDIR;
		if (tmp) {
			errno = tmp;
			dosclosedev(dd);
			return(-1);
		}
	}
	if (flags & O_TRUNC) {
		if ((flags & O_ACCMODE) != O_RDONLY
		&& clustfree(&(devlist[dd]),
		clust32(&(devlist[dd]), dd2dentp(dd))) < 0) {
			errno = doserrno;
			dosclosedev(dd);
			return(-1);
		}
		dd2dentp(dd) -> size[0] =
		dd2dentp(dd) -> size[1] =
		dd2dentp(dd) -> size[2] =
		dd2dentp(dd) -> size[3] = 0;
		dd2dentp(dd) -> clust[0] =
		dd2dentp(dd) -> clust[1] =
		dd2dentp(dd) -> clust_h[0] =
		dd2dentp(dd) -> clust_h[1] = 0;
	}

	dosflist[fd]._base = (char *)malloc((long)(devlist[dd].clustsize)
			* (long)(devlist[dd].sectsize));
	if (!dosflist[fd]._base) {
		dosclosedev(dd);
		return(-1);
	}

	dosflist[fd]._cnt = 0L;
	dosflist[fd]._ptr = dosflist[fd]._base;
	dosflist[fd]._bufsize =
		(long)(devlist[dd].clustsize) * (long)(devlist[dd].sectsize);
	dosflist[fd]._flag = flags;
	dosflist[fd]._file = dd;
	dosflist[fd]._top =
	dosflist[fd]._next = clust32(&(devlist[dd]), dd2dentp(dd));
	dosflist[fd]._off = 0L;
	dosflist[fd]._loc = (off_t)0;
	dosflist[fd]._size = (off_t)byte2dword(dd2dentp(dd) -> size);

	memcpy((char *)&(dosflist[fd]._dent),
		(char *)dd2dentp(dd), sizeof(dent_t));
	dosflist[fd]._clust = dd2clust(dd);
	dosflist[fd]._offset = dd2offset(dd);
	if (fd >= maxdosf) maxdosf = fd + 1;
	if ((flags & O_ACCMODE) != O_RDONLY && (flags & O_APPEND))
		doslseek(fd + DOSFDOFFSET, (off_t)0, L_XTND);

	return(fd + DOSFDOFFSET);
}

int dosclose(fd)
int fd;
{
	int n;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf || !dosflist[fd]._base)
		return(seterrno(EBADF));
	free(dosflist[fd]._base);
	dosflist[fd]._base = NULL;
	while (maxdosf > 0 && !dosflist[maxdosf - 1]._base) maxdosf--;

	n = 0;
	if ((dosflist[fd]._flag & O_ACCMODE) != O_RDONLY) {
		dosflist[fd]._dent.size[0] = dosflist[fd]._size & 0xff;
		dosflist[fd]._dent.size[1] = (dosflist[fd]._size >> 8) & 0xff;
		dosflist[fd]._dent.size[2] = (dosflist[fd]._size >> 16) & 0xff;
		dosflist[fd]._dent.size[3] = (dosflist[fd]._size >> 24) & 0xff;

		if (!(dosflist[fd]._dent.attr & DS_IFDIR))
			putdostime(dosflist[fd]._dent.time, -1);
		memcpy((char *)fd2dentp(fd),
			(char *)&(dosflist[fd]._dent), sizeof(dent_t));
		fd2clust(fd) = dosflist[fd]._clust;
		fd2offset(fd) = dosflist[fd]._offset;
		if (writedent(dosflist[fd]._file) < 0
		|| writefat(fd2devp(fd)) < 0) {
			errno = doserrno;
			n = -1;
		}
	}
	dosclosedev(dosflist[fd]._file);

	return(n);
}

static long NEAR dosfilbuf(fd, wrt)
int fd, wrt;
{
	long size, new, prev;

	size = dosflist[fd]._bufsize;
	if (!wrt && (off_t)size > dosflist[fd]._size - dosflist[fd]._loc)
		size = dosflist[fd]._size - dosflist[fd]._loc;
	prev = dosflist[fd]._off;
	if ((dosflist[fd]._off = dosflist[fd]._next))
		dosflist[fd]._next = clustread(fd2devp(fd),
			(u_char *)(dosflist[fd]._base), dosflist[fd]._next);
	else {
		doserrno = 0;
		dosflist[fd]._next = -1L;
	}

	if (dosflist[fd]._next < 0L) {
		if (doserrno) return(-1L);
		if (!wrt) return(0L);
		if ((new = clustexpand(fd2devp(fd), prev, 0)) < 0L)
			return(-1L);
		if (!dosflist[fd]._off) {
			dosflist[fd]._dent.clust[0] = new & 0xff;
			dosflist[fd]._dent.clust[1] = (new >> 8) & 0xff;
			if (fd2devp(fd) -> flags & F_FAT32) {
				dosflist[fd]._dent.clust_h[0] =
					(new >> 16) & 0xff;
				dosflist[fd]._dent.clust_h[1] =
					(new >> 24) & 0xff;
			}
		}
		dosflist[fd]._off = new;
		dosflist[fd]._next = ENDCLUST;
		memset(dosflist[fd]._base, 0, dosflist[fd]._bufsize);
	}
	dosflist[fd]._cnt = size;
	dosflist[fd]._ptr = dosflist[fd]._base;

	return(size);
}

static long NEAR dosflsbuf(fd)
int fd;
{
	u_long sect;
	long l;

	if (!(sect = clust2sect(fd2devp(fd), dosflist[fd]._off))) {
		doserrno = EIO;
		return(-1L);
	}
	l = sectwrite(fd2devp(fd), sect,
		(u_char *)(dosflist[fd]._base), fd2devp(fd) -> clustsize);
	if (l < 0L) return(-1L);

	return(dosflist[fd]._bufsize);
}

int dosread(fd, buf, nbytes)
int fd;
char *buf;
int nbytes;
{
	long size, total;
	int wrt;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf
	|| (buf && (dosflist[fd]._flag & O_ACCMODE) == O_WRONLY))
		return(seterrno(EBADF));
	wrt = ((dosflist[fd]._flag & O_ACCMODE) != O_RDONLY) ? 1 : 0;

	total = 0;
	while (nbytes > 0 && dosflist[fd]._loc < dosflist[fd]._size) {
		if (dosflist[fd]._cnt <= 0L && dosfilbuf(fd, wrt) < 0L)
			return(seterrno(doserrno));
		if (!(size = dosflist[fd]._cnt)) return(total);

		if (size > nbytes) size = nbytes;
		memcpy(buf, dosflist[fd]._ptr, size);
		buf += size;
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
CONST char *buf;
int nbytes;
{
	long size, total;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf
	|| (dosflist[fd]._flag & O_ACCMODE) == O_RDONLY)
		return(seterrno(EBADF));

	total = 0;
	while (nbytes > 0) {
		if (dosflist[fd]._cnt <= 0L && dosfilbuf(fd, 1) < 0L) {
#if	MSDOS
			if (doserrno == ENOSPC) return(total);
#endif
			return(seterrno(doserrno));
		}
		size = dosflist[fd]._cnt;

		if (size > nbytes) size = nbytes;
		memcpy(dosflist[fd]._ptr, buf, size);
		buf += size;
		dosflist[fd]._ptr += size;
		if ((dosflist[fd]._cnt -= size) < 0L) dosflist[fd]._cnt = 0L;
		if ((dosflist[fd]._loc += size) > dosflist[fd]._size)
			dosflist[fd]._size = dosflist[fd]._loc;
		nbytes -= size;
		total += size;

		if (dosflsbuf(fd) < 0L) return(seterrno(doserrno));
	}

	return(total);
}

off_t doslseek(fd, offset, whence)
int fd;
off_t offset;
int whence;
{
	long size;
	int wrt;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf) return(seterrno(EBADF));
	wrt = ((dosflist[fd]._flag & O_ACCMODE) != O_RDONLY) ? 1 : 0;

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
			return(seterrno(EINVAL));
/*NOTREACHED*/
			break;
	}

	if (offset >= dosflist[fd]._loc) offset -= dosflist[fd]._loc;
	else {
		dosflist[fd]._cnt = 0L;
		dosflist[fd]._ptr = dosflist[fd]._base;
		dosflist[fd]._loc = (off_t)0;
		dosflist[fd]._off = 0L;
		dosflist[fd]._next = dosflist[fd]._top;
	}

	while (offset > (off_t)0) {
		if (!wrt && dosflist[fd]._loc >= dosflist[fd]._size) break;
		if (dosflist[fd]._cnt <= 0L && dosfilbuf(fd, wrt) < 0L)
			return(seterrno(doserrno));
		if (!(size = dosflist[fd]._cnt)) break;

		if (size > offset) size = offset;
		dosflist[fd]._ptr += size;
		dosflist[fd]._cnt -= size;
		dosflist[fd]._loc += size;
		if (dosflist[fd]._size < dosflist[fd]._loc)
			dosflist[fd]._size = dosflist[fd]._loc;
		offset -= size;
	}

	return(dosflist[fd]._loc);
}

int dosftruncate(fd, size)
int fd;
off_t size;
{
	off_t offset;
	int n;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf) return(seterrno(EBADF));
	if ((dosflist[fd]._flag & O_ACCMODE) == O_RDONLY)
		return(seterrno(EINVAL));

	n = 0;
	offset = dosflist[fd]._loc;
	if (doslseek(fd + DOSFDOFFSET, size, L_SET) < (off_t)0) n = -1;
	else if (dosflist[fd]._next != ENDCLUST) {
		clustfree(fd2devp(fd), dosflist[fd]._next);
		dosflist[fd]._next = ENDCLUST;
	}
	dosflist[fd]._size = size;
	doslseek(fd + DOSFDOFFSET, offset, L_SET);

	return(n);
}

int dosmkdir(path, mode)
CONST char *path;
int mode;
{
	dent_t dent[2];
	long clust;
	int fd, tmp;

	fd = dosopen(path, O_BINARY | O_RDWR | O_CREAT | O_EXCL, mode & 0777);
	if (fd < 0) return(-1);

	fd -= DOSFDOFFSET;
	dosflist[fd]._dent.attr |= DS_IFDIR;
	dosflist[fd]._dent.attr &= ~DS_IARCHIVE;
	memcpy((char *)&(dent[0]),
		(char *)&(dosflist[fd]._dent), sizeof(*dent));
	memset(dent[0].name, ' ', 8 + 3);
	dent[0].name[0] = '.';
	if ((clust = newclust(fd2devp(fd))) < 0L) {
		errno = doserrno;
		dosflist[fd]._dent.name[0] = 0xe5;
		*fd2path(fd) = '\0';
		VOID_C dosclose(fd + DOSFDOFFSET);
		return(-1);
	}
	dent[0].clust[0] = clust & 0xff;
	dent[0].clust[1] = (clust >> 8) & 0xff;
	if (fd2devp(fd) -> flags & F_FAT32) {
		dent[0].clust_h[0] = (clust >> 16) & 0xff;
		dent[0].clust_h[1] = (clust >> 24) & 0xff;
	}

	memcpy((char *)&(dent[1]),
		(char *)&(dosflist[fd]._dent), sizeof(*dent));
	memset(dent[1].name, ' ', 8 + 3);
	dent[1].name[0] =
	dent[1].name[1] = '.';
	clust = fd2clust(fd);
	dent[1].clust[0] = clust & 0xff;
	dent[1].clust[1] = (clust >> 8) & 0xff;
	if (fd2devp(fd) -> flags & F_FAT32) {
		dent[1].clust_h[0] = (clust >> 16) & 0xff;
		dent[1].clust_h[1] = (clust >> 24) & 0xff;
	}

	if (doswrite(fd + DOSFDOFFSET, (char *)dent, 2 * sizeof(*dent)) < 0) {
		tmp = errno;
		if ((clust = clust32(fd2devp(fd), &(dosflist[fd]._dent))))
			clustfree(fd2devp(fd), clust);
		dosflist[fd]._dent.name[0] = 0xe5;
		*fd2path(fd) = '\0';
		VOID_C dosclose(fd + DOSFDOFFSET);
		errno = tmp;
		return(-1);
	}

	dosflist[fd]._size = (off_t)0;
	VOID_C dosclose(fd + DOSFDOFFSET);

	return(0);
}

int dosrmdir(path)
CONST char *path;
{
	dosDIR *xdirp;
	struct dosdirent *dp;
	cache_t cache;
	long clust, offset;
	int n, sum;

	if (!(xdirp = _dosopendir(path, NULL, 0))) return(seterrno(doserrno));
	memcpy((char *)&cache,
		(char *)(devlist[xdirp -> dd_fd].dircache), sizeof(cache));

	clust = lfn_clust;
	offset = lfn_offset;

	while ((dp = _dosreaddir(xdirp, 0)))
		if (!isdotdir(dp -> d_name)) {
			VOID_C _dosclosedir(xdirp);
			return(seterrno(ENOTEMPTY));
		}

	n = 0;
	if (doserrno) {
		errno = doserrno;
		n = -1;
	}
	else {
		memcpy((char *)(devlist[xdirp -> dd_fd].dircache),
			(char *)&cache, sizeof(cache));
		sum = calcsum(dd2dentp(xdirp -> dd_fd) -> name);
		*(dd2dentp(xdirp -> dd_fd) -> name) = 0xe5;
		*dd2path(xdirp -> dd_fd) = '\0';
		if (writedent(xdirp -> dd_fd) < 0
		|| clustfree(&(devlist[xdirp -> dd_fd]),
			clust32(&(devlist[xdirp -> dd_fd]),
			dd2dentp(xdirp -> dd_fd))) < 0
		|| unlinklfn(xdirp -> dd_fd, clust, offset, sum) < 0
		|| writefat(&(devlist[xdirp -> dd_fd])) < 0) {
			errno = doserrno;
			n = -1;
		}
	}
	VOID_C _dosclosedir(xdirp);

	return(n);
}

int dosflushbuf(fd)
int fd;
{
	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf || !dosflist[fd]._base)
		return(seterrno(EBADF));
	if (flushcache(&(devlist[dosflist[fd]._file])) < 0)
		return(seterrno(doserrno));

	return(0);
}

int dosallclose(VOID_A)
{
	int i, n;

	n = 0;
	for (i = maxdosf - 1; i >= 0; i--)
		if (dosflist[i]._base && dosclose(i + DOSFDOFFSET) < 0)
			n = -1;
	for (i = maxdev - 1; i >= 0; i--) {
		if (!(devlist[i].drive)) continue;
		devlist[i].nlink = 0;
		if (_dosclosedev(i) < 0) n = seterrno(doserrno);
	}

	return(n);
}
#endif	/* !_NODOSDRIVE */
