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
#define	VOID_P	char *
#else
#define	VOID	void
#define	VOID_T	void
#define	VOID_P	void *
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

#ifdef	HDDMOUNT
#include <sys/ioctl.h>
#define	D_SECSIZE(dl)	(dl).d_secsize
# ifdef	BSD4
# include <sys/disklabel.h>
# else
#  ifdef	SOLARIS
#  include <sys/dkio.h>
#  include <sys/vtoc.h>
#  define	DIOCGDINFO	DKIOCGGEOM
#  define	disklabel	dk_geom
#  define	d_ntracks	dkg_nhead
#  define	d_nsectors	dkg_nsect
#  endif
#  ifdef	LINUX
#  include <linux/hdreg.h>
#  define	DIOCGDINFO	HDIO_GETGEO
#  define	disklabel	hd_geometry
#  define	d_ntracks	heads
#  define	d_nsectors	sectors
#  undef	D_SECSIZE(dl)
#  define	D_SECSIZE(dl)	512
#  endif
# endif
#endif

#ifdef	LINUX
#include <mntent.h>
#include <sys/mount.h>
#include <linux/unistd.h>
# ifndef	BLKFLSBUF
# include <linux/fs.h>
# endif
# ifndef	MOUNTED
# define	MOUNTED		"/etc/mtab"
# endif
#endif

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

#ifdef	LSI_C
#include <jctype.h>
#define	toupper2	toupper
#define	tolower2	tolower
#define	issjis1		iskanji
#define	issjis2		iskanji2
#else	/* !LSI_C */
#define	toupper2(c)	(uppercase[(u_char)(c)])
#define	tolower2(c)	(lowercase[(u_char)(c)])
#define	KC_SJIS1	0001
#define	KC_SJIS2	0002
#define	KC_EUCJP	0010

# ifndef	issjis1
# define	USEKCTYPETABLE
# define	issjis1(c)	(kctypetable[(u_char)(c)] & KC_SJIS1)
# endif
# ifndef	issjis2
# define	USEKCTYPETABLE
# define	issjis2(c)	(kctypetable[(u_char)(c)] & KC_SJIS2)
# endif

# ifndef	iseuc
# define	USEKCTYPETABLE
# define	iseuc(c)	(kctypetable[(u_char)(c)] & KC_EUCJP)
# endif
#endif	/* !LSI_C */

#ifdef	CODEEUC
#define	iskanji1(s, i)	(iseuc((s)[i]) && iseuc((s)[(i) + 1]))
#else
#define	iskanji1(s, i)	(issjis1((s)[i]) && issjis2((s)[(i) + 1]))
#endif

#define	reterr(c)	{errno = doserrno; return(c);}
#define	S_IEXEC_ALL	(S_IEXEC | (S_IEXEC >> 3) | (S_IEXEC >> 6))
#define	ENDCLUST	((u_long)(0x0fffffff))
#define	ERRCLUST	((u_long)(0x0ffffff7))
#define	ROOTCLUST	((u_long)(0x10000000))

#if	(GETTODARGS == 1)
#define	gettimeofday2(tv, tz)	gettimeofday(tv)
#else
#define	gettimeofday2(tv, tz)	gettimeofday(tv, tz)
#endif

#ifdef	FD
extern int _dospath __P_((char *));
extern int isdelim __P_((char *, int));
#if	MSDOS || !defined (_NODOSDRIVE)
extern char *strdelim __P_((char *, int));
extern char *strrdelim __P_((char *, int));
#else
#define	strdelim(s, d)	strchr(s, _SC_)
#define	strrdelim(s, d)	strrchr(s, _SC_)
#endif
extern char *strrdelim2 __P_((char *, char *));
#if	MSDOS
extern char *strcatdelim __P_((char *));
#endif
extern char *strcatdelim2 __P_((char *, char *, char *));
#if	MSDOS
#define	strcasecmp2	stricmp
#else
extern int strcasecmp2 __P_((char *, char *));
#endif
extern int isdotdir __P_((char *));
extern time_t timelocal2 __P_((struct tm *));
extern VOID readunitable __P_((VOID_A));
extern VOID discardunitable __P_((VOID_A));
extern u_short unifysjis __P_((u_short, int));
extern u_short cnvunicode __P_((u_short, int));
#else	/* !FD */
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
#define	char2long(cp)	( (long) (((u_char *)(cp))[3]) \
			| (long)((((u_char *)(cp))[2]) << (CHAR_BIT * 1)) \
			| (long)((((u_char *)(cp))[1]) << (CHAR_BIT * 2)) \
			| (long)((((u_char *)(cp))[0]) << (CHAR_BIT * 3)) )
#define	_dospath(s)	((isalpha(*(s)) && (s)[1] == ':') ? *(s) : 0)
static int NEAR isdelim __P_((char *, int));
#if	MSDOS
static char *NEAR strdelim __P_((char *, int));
static char *NEAR strrdelim __P_((char *, int));
#else
#define	strdelim(s, d)	strchr(s, _SC_)
#define	strrdelim(s, d)	strrchr(s, _SC_)
#endif
static char *NEAR strrdelim2 __P_((char *, char *));
#if	MSDOS
static char *NEAR strcatdelim __P_((char *));
#endif
static char *NEAR strcatdelim2 __P_((char *, char *, char *));
#if	MSDOS
#define	strcasecmp2	stricmp
#else
static int NEAR strcasecmp2 __P_((char *, char *));
#endif
static int NEAR isdotdir __P_((char *));
#if	!MSDOS && !defined (NOTZFILEH) \
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static int NEAR tmcmp __P_((struct tm *, struct tm *));
#endif
#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long NEAR gettimezone __P_((struct tm *, time_t));
#endif
static time_t NEAR timelocal2 __P_((struct tm *));
static VOID NEAR readunitable __P_((VOID_A));
static VOID NEAR discardunitable __P_((VOID_A));
static u_short NEAR unifysjis __P_((u_short, int));
static u_short NEAR cnvunicode __P_((u_short, int));
#define	UNICODETBL	"fd-unicd.tbl"
#define	MINUNICODE	0x00a7
#define	MAXUNICODE	0xffe5
#define	MINKANJI	0x8140
#define	MAXKANJI	0xfc4b
#endif	/* !FD */

#ifdef	USELLSEEK
static off64_t NEAR lseek64 __P_((int, off64_t, int));
#else
#define	lseek64	lseek
#endif
#if	!MSDOS
static int NEAR sectseek __P_((devstat *, u_long));
#endif
static int NEAR realread __P_((devstat *, u_long, u_char *, int));
static int NEAR realwrite __P_((devstat *, u_long, u_char *, int));
static int NEAR killcache __P_((devstat *, int, int));
static int NEAR flushcache __P_((devstat *));
static int NEAR shiftcache __P_((devstat *, int, int));
static int NEAR cachecpy __P_((int, int, int, int));
static int NEAR uniqcache __P_((devstat *, int, u_long, u_long));
static int NEAR appendcache __P_((int, devstat *, u_long, u_char *, int, int));
static int NEAR insertcache __P_((int, devstat *, u_long, u_char *, int, int));
static int NEAR overwritecache __P_((int, devstat *,
		u_long, u_char *, int, int));
static int NEAR savecache __P_((devstat *, u_long, u_char *, int, int));
static int NEAR loadcache __P_((devstat *, u_long, u_char *, int));
static long NEAR sectread __P_((devstat *, u_long, u_char *, int));
static long NEAR sectwrite __P_((devstat *, u_long, u_char *, int));
static int NEAR availfat __P_((devstat *));
#if	!MSDOS
static int NEAR readfat __P_((devstat *));
#endif
static int NEAR writefat __P_((devstat *));
static long NEAR getfatofs __P_((devstat *, long));
static u_char *NEAR readtmpfat __P_((devstat *, long, int));
static long NEAR getfatent __P_((devstat *, long));
static int NEAR putfatent __P_((devstat *, long, long));
static u_long NEAR clust2sect __P_((devstat *, long));
static long NEAR clust32 __P_((devstat *, dent_t *));
static long NEAR newclust __P_((devstat *));
static long NEAR clustread __P_((devstat *, u_char *, long));
static long NEAR clustwrite __P_((devstat *, u_char *, long));
static long NEAR clustexpand __P_((devstat *, long, int));
static int NEAR clustfree __P_((devstat *, long));
#if	!MSDOS
static int NEAR _readbpb __P_((devstat *, bpb_t *));
#endif
static int NEAR readbpb __P_((devstat *, int));
#ifdef	HDDMOUNT
static off64_t *NEAR _readpt __P_((off64_t, off64_t, int, int, int, int));
#endif
static int NEAR opendev __P_((int));
static int NEAR closedev __P_((int));
static int NEAR checksum __P_((char *));
static u_short NEAR lfnencode __P_((u_char, u_char));
static u_short NEAR lfndecode __P_((u_char, u_char));
#if	!MSDOS
static int NEAR transchar __P_((int));
static int NEAR detranschar __P_((int));
#endif
static int NEAR sfntranschar __P_((int));
static int NEAR cmpdospath __P_((char *, char *, int, int));
static char *NEAR getdosname __P_((char *, char *, char *));
static char *NEAR putdosname __P_((char *, char *, int));
static u_short NEAR getdosmode __P_((u_char));
static u_char NEAR putdosmode __P_((u_short));
static time_t NEAR getdostime __P_((u_short, u_short));
static int NEAR putdostime __P_((u_char *, time_t));
static int NEAR getdrive __P_((char *));
static int NEAR parsepath __P_((char *, char *, int));
static int NEAR seekdent __P_((dosDIR *, dent_t *, long, u_short));
static int NEAR readdent __P_((dosDIR *, dent_t *, int));
static dosDIR *NEAR _dosopendir __P_((char *, char *, int));
static int NEAR _dosclosedir __P_((dosDIR *));
static struct dirent *NEAR _dosreaddir __P_((dosDIR *, int));
static struct dirent *NEAR finddir __P_((dosDIR *, char *, int, int));
static dosDIR *NEAR splitpath __P_((char **, char *, int));
static int NEAR getdent __P_((char *, int *));
static int NEAR writedent __P_((int));
static int NEAR expanddent __P_((int));
static int NEAR creatdent __P_((char *, int));
static int NEAR unlinklfn __P_((int, long, u_short, int));
static int NEAR dosfilbuf __P_((int, int));
static int NEAR dosflsbuf __P_((int));
static int NEAR type2flags __P_((char *));

#if	MSDOS
int dependdosfunc = 0;
#else
devinfo fdtype[MAXDRIVEENTRY] = {
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
devinfo mediadescr[] = {
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
static long maxcachemem = 0;
static long lfn_clust = -1;
static u_short lfn_offset = 0;
static int doserrno = 0;
#if	!MSDOS
static short sectsizelist[] = SECTSIZE;
#define	SLISTSIZ	((int)(sizeof(sectsizelist) / sizeof(short)))
#endif
static char *inhibitname[] = INHIBITNAME;
#define	INHIBITNAMESIZ	((int)(sizeof(inhibitname) / sizeof(char *)))
#ifndef	FD
typedef struct _kconv_t {
	u_short start;
	u_short cnv;
	u_short range;
} kconv_t;
char *unitblpath = NULL;
static u_char *unitblbuf = NULL;
static u_short unitblent = 0;
static kconv_t rsjistable[] = {
	{0x8470, 0x8440, 0x0f},		/* strange Russian char */
	{0x8480, 0x844f, 0x12},		/* Why they converted ? */
#define	EXCEPTRUSS	2
	{0x8754, 0xfa4a, 0x0a},
	{0x8782, 0xfa59, 0x01},
	{0x8784, 0xfa5a, 0x01},
	{0x878a, 0xfa58, 0x01},
	{0x8790, 0x81e0, 0x01},
	{0x8791, 0x81df, 0x01},
	{0x8792, 0x81e7, 0x01},
	{0x8795, 0x81e3, 0x01},
	{0x8796, 0x81db, 0x01},
	{0x8797, 0x81da, 0x01},
	{0x879a, 0x81e6, 0x01},
	{0x879b, 0x81bf, 0x01},
	{0x879c, 0x81be, 0x01},
	{0xed40, 0xfa5c, 0x23},
	{0xed63, 0xfa80, 0x1c},
	{0xed80, 0xfa9c, 0x61},
	{0xede1, 0xfb40, 0x1c},
	{0xee40, 0xfb5c, 0x23},
	{0xee63, 0xfb80, 0x1c},
	{0xee80, 0xfb9c, 0x61},
	{0xeee1, 0xfc40, 0x0c},
	{0xeeef, 0xfa40, 0x0a},
	{0xeef9, 0x81ca, 0x01},
	{0xeefa, 0xfa55, 0x03},
	{0xfa54, 0x81ca, 0x01},
	{0xfa5b, 0x81e6, 0x01},
};
#define	RSJISTBLSIZ	((int)(sizeof(rsjistable) / sizeof(kconv_t)))
#endif	/* !FD */
#ifndef	LSI_C
# ifdef	FD
extern u_char uppercase[256];
extern u_char lowercase[256];
#  ifdef	USEKCTYPETABLE
extern u_char kctypetable[256];
#  endif
# else	/* !FD */
static u_char uppercase[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* 0x00 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,	/* 0x10 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x40 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,	/* 0x50 */
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x60 */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	/* 0x70 */
	0x48, 0x49, 0x4a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,	/* 0x80 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,	/* 0x90 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,	/* 0xa0 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,	/* 0xb0 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,	/* 0xc0 */
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,	/* 0xd0 */
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,	/* 0xe0 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,	/* 0xf0 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
static u_char lowercase[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* 0x00 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,	/* 0x10 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,	/* 0x20 */
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	/* 0x30 */
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x40 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x50 */
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,	/* 0x60 */
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,	/* 0x70 */
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,	/* 0x80 */
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,	/* 0x90 */
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,	/* 0xa0 */
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,	/* 0xb0 */
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,	/* 0xc0 */
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,	/* 0xd0 */
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,	/* 0xe0 */
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,	/* 0xf0 */
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
#  ifdef	USEKCTYPETABLE
static u_char kctypetable[256] = {
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,	/* 0x00 */
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,	/* 0x10 */
	0200, 0200, 0200, 0200, 0200, 0200, 0200, 0200,
	   0, 0060, 0060, 0060, 0060, 0060, 0060, 0060,	/* 0x20 */
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,	/* 0x30 */
	0060, 0060, 0060, 0060, 0060, 0060, 0060, 0060,
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,	/* 0x40 */
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,	/* 0x50 */
	0062, 0062, 0062, 0062, 0062, 0062, 0062, 0062,
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,	/* 0x60 */
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0022,	/* 0x70 */
	0022, 0022, 0022, 0022, 0022, 0022, 0022, 0200,
	0002, 0003, 0003, 0003, 0003, 0003, 0003, 0003,	/* 0x80 */
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,	/* 0x90 */
	0003, 0003, 0003, 0003, 0003, 0003, 0003, 0003,
	0002, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xa0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xb0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xc0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,	/* 0xd0 */
	0016, 0016, 0016, 0016, 0016, 0016, 0016, 0016,
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,	/* 0xe0 */
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,
	0013, 0013, 0013, 0013, 0013, 0013, 0013, 0013,	/* 0xf0 */
	0013, 0013, 0013, 0013, 0013, 0010, 0010,    0
};
#  endif	/* USEKCTYPETABLE */
# endif	/* !FD */
#endif	/* !LSI_C */


#ifndef	FD
static int NEAR isdelim(s, ptr)
char *s;
int ptr;
{
#if	MSDOS
	int i;
#endif

	if (ptr < 0 || s[ptr] != _SC_) return(0);
#if	MSDOS
	if (--ptr < 0) return(1);
	if (!ptr) return(!iskanji1(s, 0));

	for (i = 0; s[i] && i < ptr; i++) if (iskanji1(s, i)) i++;
	if (!s[i] || i > ptr) return(1);
	return(!iskanji1(s, i));
#else
	return(1);
#endif
}

#if	MSDOS
static char *NEAR strdelim(s, d)
char *s;
int d;
{
	int i;

	if (d && _dospath(s)) return(s + 1);
	for (i = 0; s[i]; i++) {
		if (s[i] == _SC_) return(&(s[i]));
		if (iskanji1(s, i)) i++;
	}
	return(NULL);
}

static char *NEAR strrdelim(s, d)
char *s;
int d;
{
	char *cp;
	int i;

	if (d && _dospath(s)) cp = s + 1;
	else cp = NULL;
	for (i = 0; s[i]; i++) {
		if (s[i] == _SC_) cp = &(s[i]);
		if (iskanji1(s, 1)) i++;
	}
	return(cp);
}
#endif	/* MSDOS */

static char *NEAR strrdelim2(s, eol)
char *s, *eol;
{
#if	MSDOS
	char *cp;
	int i;

	cp = NULL;
	for (i = 0; s[i] && &(s[i]) < eol; i++) {
		if (s[i] == _SC_) cp = &(s[i]);
		if (iskanji1(s, 1)) i++;
	}
	return(cp);
#else
	for (eol--; eol >= s; eol--) if (*eol == _SC_) return(eol);
	return(NULL);
#endif
}

#if	MSDOS
static char *NEAR strcatdelim(s)
char *s;
{
	char *cp;
	int i;

	if (_dospath(s)) i = 2;
	else i = 0;
	if (s[i] == _SC_ && !s[i + 1]) return(&(s[i + 1]));

	cp = NULL;
	for (; s[i]; i++) {
		if (s[i] == _SC_ && !cp) cp = &(s[i]);
		else cp = NULL;
		if (iskanji1(s, 1)) i++;
	}
	if (!cp) *(cp = &(s[i])) = _SC_;
	*(++cp) = '\0';
	return(cp);
}
#endif	/* MSDOS */

static char *NEAR strcatdelim2(buf, s1, s2)
char *buf, *s1, *s2;
{
	char *cp;
	int i, len;

	if (_dospath(s1)) {
		buf[0] = s1[0];
		buf[1] = s1[1];
		i = 2;
	}
	else i = 0;
	if (s1[i] == _SC_ && !s1[i + 1]) *(cp = &(buf[i])) = _SC_;
	else {
		cp = NULL;
		for (; s1[i]; i++) {
			buf[i] = s1[i];
			if (s1[i] == _SC_) {
				if (!cp) cp = &(buf[i]);
				continue;
			}
			cp = NULL;
#if	MSDOS
			if (iskanji1(s1, i)) {
				if (!s1[++i]) break;
				buf[i] = s1[i];
			}
#endif
		}
		if (!cp) {
			cp = &(buf[i]);
			if (i >= MAXPATHLEN - 1) {
				*cp = '\0';
				return(cp);
			}
			*cp = _SC_;
		}
	}
	if (s2) {
		len = MAXPATHLEN - 1 - (cp - buf);
		for (i = 0; s2[i] && i < len; i++) *(++cp) = s2[i];
	}
	*(++cp) = '\0';
	return(cp);
}

#if	!MSDOS
static int NEAR strcasecmp2(s1, s2)
char *s1, *s2;
{
	int c1, c2;

	for (;;) {
		c1 = toupper2(*s1);
		c2 = toupper2(*s2);
		if (c1 != c2) return(c1 - c2);
# ifndef	CODEEUC
		if (issjis1(c1)) {
			s1++;
			s2++;
			if (*s1 != *s2) return(*s1 - *s2);
		}
# endif
		if (!*s1) break;
		s1++;
		s2++;
	}
	return(0);
}
#endif	/* !MSDOS */

static int NEAR isdotdir(name)
char *name;
{
	if (name[0] == '.'
	&& (!name[1] || (name[1] == '.' && !name[2]))) return(1);
	return(0);
}

#if	!MSDOS && !defined (NOTZFILEH) \
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static int NEAR tmcmp(tm1, tm2)
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
static long NEAR gettimezone(tm, t)
struct tm *tm;
time_t t;
{
#if	MSDOS
	struct timeb buffer;

	ftime(&buffer);
	return((long)buffer.timezone * 60L);
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
	char *cp, buf[MAXPATHLEN];
	u_char c;

	memcpy((char *)&tmbuf, (char *)tm, sizeof(struct tm));

#ifdef	NOTMGMTOFF
	gettimeofday2(&t_val, &t_zone);
	tz = t_zone.tz_minuteswest * 60L;
#else
	tz = -(localtime(&t) -> tm_gmtoff);
#endif

#ifndef	NOTZFILEH
	cp = (char *)getenv("TZ");
	if (!cp || !*cp) cp = TZDEFAULT;
	if (*cp == _SC_) strcpy(buf, cp);
	else strcatdelim2(buf, TZDIR, cp);
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

static time_t NEAR timelocal2(tm)
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

	d = ((long)y - 1970) * 365;
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
	t = ((long)(tm -> tm_hour) * 60L + tm -> tm_min) * 60L + tm -> tm_sec;
	t += d * 60L * 60L * 24L;
	t += gettimezone(tm, t);

	return(t);
# endif
#endif
}

static VOID NEAR readunitable(VOID_A)
{
	char path[MAXPATHLEN];
	u_char *tmp, buf[2];
	long size;
	int fd;

	if (!unitblpath || !*unitblpath) strcpy(path, UNICODETBL);
	else strcatdelim2(path, unitblpath, UNICODETBL);

	if ((fd = open(path, O_RDONLY | O_BINARY, 0600)) < 0) return;
	if (read(fd, buf, 2) != 2) {
		close(fd);
		return;
	}
	unitblent = (((u_short)(buf[1]) << 8) | buf[0]);
	size = (long)unitblent * 4;

	if (!unitblbuf) tmp = (u_char *)malloc(size);
	else tmp = (u_char *)realloc(unitblbuf, size);
	if (!tmp) {
		close(fd);
		return;
	}
	unitblbuf = tmp;
	if (read(fd, unitblbuf, size) != size) discardunitable();
	close(fd);
}

static VOID NEAR discardunitable(VOID_A)
{
	if (unitblbuf) {
		free(unitblbuf);
		unitblbuf = NULL;
	}
}

static u_short NEAR unifysjis(wc, russ)
u_short wc;
int russ;
{
	int i;

	for (i = ((russ) ? 0 : EXCEPTRUSS); i < RSJISTBLSIZ; i++)
		if (wc >= rsjistable[i].start
		&& wc < rsjistable[i].start + rsjistable[i].range)
			break;
	if (i < RSJISTBLSIZ) {
		wc -= rsjistable[i].start;
		wc += rsjistable[i].cnv;
	}
	return(wc);
}

static u_short NEAR cnvunicode(wc, encode)
u_short wc;
int encode;
{
	static int fd = -1;
	u_char buf[4];
	char path[MAXPATHLEN];
	u_short r, w, min, max, ofs;

	if (encode < 0) {
		if (fd >= 0) close(fd);
		fd = -1;
		return(0);
	}

	if (encode) {
		r = 0xff00;
		if (wc < 0x80) return(wc);
		if (wc > 0xa0 && wc <= 0xdf) return(0xff00 | (wc - 0x40));
		if (wc >= 0x8260 && wc <= 0x8279)
			return(0xff00 | (wc - 0x8260 + 0x21));
		if (wc >= 0x8281 && wc <= 0x829a)
			return(0xff00 | (wc - 0x8281 + 0x41));
		if (wc < MINKANJI || wc > MAXKANJI) return(r);
	}
	else {
		r = '?';
		switch (wc & 0xff00) {
			case 0:
				if ((wc & 0xff) < 0x80) return(wc);
				break;
			case 0xff00:
				w = wc & 0xff;
				if (w > 0x20 && w <= 0x3a)
					return(w + 0x8260 - 0x21);
				if (w > 0x40 && w <= 0x5a)
					return(w + 0x8281 - 0x41);
				if (w > 0x60 && w <= 0x9f)
					return(w + 0x40);
				break;
			default:
				break;
		}
		if (wc < MINUNICODE || wc > MAXUNICODE) return(r);
	}

	if (unitblbuf) {
		u_char *cp;

		if (encode) {
			wc = unifysjis(wc, 0);
			cp = unitblbuf;
			for (ofs = 0; ofs < unitblent; ofs++) {
				w = (((u_short)(cp[3]) << 8) | cp[2]);
				if (wc == w) {
					r = (((u_short)(cp[1]) << 8) | cp[0]);
					break;
				}
				cp += 4;
			}
		}
		else {
			min = 0;
			max = unitblent + 1;
			ofs = unitblent / 2 + 1;
			for (;;) {
				if (ofs == min || ofs == max) break;
				cp = &(unitblbuf[(ofs - 1) * 4]);
				w = (((u_short)(cp[1]) << 8) | cp[0]);
				if (wc == w) {
					r = (((u_short)(cp[3]) << 8) | cp[2]);
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

		return(r);
	}

	if (fd < 0) {
		if (!unitblpath || !*unitblpath) strcpy(path, UNICODETBL);
		else strcatdelim2(path, unitblpath, UNICODETBL);

		if ((fd = open(path, O_RDONLY | O_BINARY, 0600)) < 0)
			return(r);
		if (!unitblent) {
			if (read(fd, buf, 2) != 2) {
				close(fd);
				fd = -1;
				return(r);
			}
			unitblent = (((u_short)(buf[1]) << 8) | buf[0]);
		}
	}

	if (encode) {
		if (lseek(fd, (off_t)2, L_SET) < 0) return(r);
		wc = unifysjis(wc, 0);
		for (ofs = 0; ofs < unitblent; ofs++) {
			if (read(fd, buf, 4) != 4) break;
			w = (((u_short)(buf[3]) << 8) | buf[2]);
			if (wc == w) {
				r = (((u_short)(buf[1]) << 8) | buf[0]);
				break;
			}
		}
	}
	else {
		min = 0;
		max = unitblent + 1;
		ofs = unitblent / 2 + 1;
		for (;;) {
			if (ofs == min || ofs == max) break;
			if (lseek(fd, (off_t)(ofs - 1) * 4 + 2, L_SET) < 0
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

	return(r);
}
#endif	/* !FD */

#ifdef	USELLSEEK
static _syscall5(int, _llseek,
	unsigned int, fd,
	unsigned long, ofs_h,
	unsigned long, ofs_l,
	off64_t *, result,
	unsigned int, whence);

static off64_t NEAR lseek64(fd, offset, whence)
int fd;
off64_t offset;
int whence;
{
	off64_t result;
	u_long ofs_h, ofs_l;

	ofs_h = (u_long)(offset >> 32);
	ofs_l = (u_long)(offset & (u_long)0xffffffff);
	if (_llseek(fd, ofs_h, ofs_l, &result, whence) < 0)
		return((off64_t)-1);
	return(result);
}
#endif	/* USELLSEEK */

#if	!MSDOS
static int NEAR sectseek(devp, sect)
devstat *devp;
u_long sect;
{
	off64_t offset;
	int tmperrno;

	tmperrno = errno;
	if (devp -> flags & F_8SECT) sect += sect / 8;
	offset = (off64_t)sect * (off64_t)(devp -> sectsize);
# ifdef	HDDMOUNT
	offset += devp -> offset;
# endif
	if (lseek64(devp -> fd, offset, L_SET) < 0) {
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}
	errno = tmperrno;
	return(0);
}
#endif	/* !MSDOS */

static int NEAR realread(devp, sect, buf, n)
devstat *devp;
u_long sect;
u_char *buf;
int n;
{
	int tmperrno;

	tmperrno = errno;
#if	MSDOS
	if (rawdiskio(devp -> drive, sect, buf, n, devp -> sectsize, 0) < 0) {
		doserrno = errno;
		return(-1);
	}
#else
	if (sectseek(devp, sect) < 0) return(-1);
	while (read(devp -> fd, buf, (long)n * devp -> sectsize) < 0) {
		if (errno == EINTR) errno = tmperrno;
		else {
			doserrno = errno;
			return(-1);
		}
	}
#endif
	errno = tmperrno;
	return(0);
}

static int NEAR realwrite(devp, sect, buf, n)
devstat *devp;
u_long sect;
u_char *buf;
int n;
{
	int tmperrno;

	tmperrno = errno;
#if	MSDOS
	if (rawdiskio(devp -> drive, sect, buf, n, devp -> sectsize, 1) < 0) {
		doserrno = errno;
		return(-1);
	}
#else
	if (sectseek(devp, sect) < 0) return(-1);
	if (write(devp -> fd, buf, (long)n * devp -> sectsize) < 0) {
		if (errno == EINTR) errno = tmperrno;
		else {
			doserrno = errno;
			return(-1);
		}
	}
#endif
	errno = tmperrno;
	return(0);
}

static int NEAR killcache(devp, n, size)
devstat *devp;
int n, size;
{
	int i, ret;

	ret = 0;
	for (i = 0; i < size; i++) {
		if (cachewrite[n - i]) {
			if (devp -> flags & F_RONLY) {
				doserrno = EROFS;
				ret = -1;
			}
			if (realwrite(devp, sectno[n - i],
			sectcache[n - i], 1) < 0)
				ret = -1;
		}
		free(sectcache[n - i]);
		cachewrite[n - i] = 0;
	}
	return(ret);
}

static int NEAR flushcache(devp)
devstat *devp;
{
	int i, ret;

	ret = 0;
	for (i = maxsectcache - 1; i >= 0; i--) {
		if (cachedrive[i] != devp -> drive) continue;
		if (cachewrite[i] && !(devp -> flags & F_RONLY)
		&& realwrite(devp, sectno[i], sectcache[i], 1) < 0) ret = -1;
		cachewrite[i] = 0;
	}
	return(ret);
}

static int NEAR shiftcache(devp, n, nsect)
devstat *devp;
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
int n;
int ofs1, ofs2, size;
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
devstat *devp;
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
devstat *devp;
u_long sect;
u_char *buf;
int n, wrt;
{
	u_long min, max;
	int i, size, tmperrno;

	tmperrno = errno;
	min = sectno[dst];
	max = min + cachesize[dst];
	size = sect + n - min;

	if (devp -> sectsize > STDSECTSIZE
	&& size * devp -> sectsize > SECTCACHEMEM) {
		size = SECTCACHEMEM / devp -> sectsize;
		min = sect + n - size;
		if (killcache(devp, dst, min - sectno[dst]) < 0) return(-1);
		cachecpy(dst, 0, min - sectno[dst], max - min);
	}
	else if (size > SECTCACHESIZE) {
		min += size - SECTCACHESIZE;
		size = SECTCACHESIZE;
		if (killcache(devp, dst, min - sectno[dst]) < 0) return(-1);
		cachecpy(dst, 0, min - sectno[dst], max - min);
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
				errno = tmperrno;
				return(-1);
			}
			cachewrite[dst - i] = wrt;
		}
		memcpy((char *)sectcache[dst - i], (char *)buf,
			devp -> sectsize);
		buf += devp -> sectsize;
	}
	return(0);
}

static int NEAR insertcache(dst, devp, sect, buf, n, wrt)
int dst;
devstat *devp;
u_long sect;
u_char *buf;
int n, wrt;
{
	u_long min, max;
	int i, size, tmperrno;

	tmperrno = errno;
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
	cachecpy(dst, min - sect, 0, max - min);
	for (i = 0; i < n; i++) {
		if (i + sect >= min) cachewrite[dst - i] |= wrt;
		else {
			sectcache[dst - i] =
				(u_char *)malloc(devp -> sectsize);
			if (!sectcache[dst - i]) {
				doserrno = errno;
				errno = tmperrno;
				return(-1);
			}
			cachewrite[dst - i] = wrt;
		}
		memcpy((char *)sectcache[dst - i], (char *)buf,
			devp -> sectsize);
		buf += devp -> sectsize;
	}
	return(0);
}

static int NEAR overwritecache(dst, devp, sect, buf, n, wrt)
int dst;
devstat *devp;
u_long sect;
u_char *buf;
int n, wrt;
{
	u_long min, max;
	int i, size, tmperrno;

	tmperrno = errno;
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
	cachecpy(dst, min - sect, 0, max - min);
	for (i = 0; i < n; i++) {
		if (i + sect >= min && i + sect < max)
			cachewrite[dst - i] |= wrt;
		else {
			sectcache[dst - i] =
				(u_char *)malloc(devp -> sectsize);
			if (!sectcache[dst - i]) {
				doserrno = errno;
				errno = tmperrno;
				return(-1);
			}
			cachewrite[dst - i] = wrt;
		}
		memcpy((char *)sectcache[dst - i], (char *)buf,
			devp -> sectsize);
		buf += devp -> sectsize;
	}
	return(0);
}

static int NEAR savecache(devp, sect, buf, n, wrt)
devstat *devp;
u_long sect;
u_char *buf;
int n, wrt;
{
	int i, tmperrno;

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

	tmperrno = errno;
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
			errno = tmperrno;
			return(-1);
		}
		cachesize[maxsectcache - i - 1] = n - i;
		cachewrite[maxsectcache - i - 1] = wrt;
		memcpy((char *)sectcache[maxsectcache - i - 1], (char *)buf,
			devp -> sectsize);
		buf += devp -> sectsize;
	}
	return(0);
}

static int NEAR loadcache(devp, sect, buf, n)
devstat *devp;
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
				memcpy((char *)buf, (char *)sectcache[src - j],
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
				memcpy((char *)buf, (char *)sectcache[src - j],
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
				memcpy((char *)cp, (char *)sectcache[src - j],
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
				memcpy((char *)cp, (char *)sectcache[src - j],
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
devstat *devp;
u_long sect;
u_char *buf;
int n;
{
	if (loadcache(devp, sect, buf, n) < 0) return(-1);
	return((long)n * devp -> sectsize);
}

static long NEAR sectwrite(devp, sect, buf, n)
devstat *devp;
u_long sect;
u_char *buf;
int n;
{
	if (devp -> flags & F_RONLY) {
		doserrno = EROFS;
		return(-1);
	}
	if (savecache(devp, sect, buf, n, 1) < 0
	&& realwrite(devp, sect, buf, n) < 0) return(-1);
	return((long)n * devp -> sectsize);
}

static int NEAR availfat(devp)
devstat *devp;
{
	u_char *buf;
	u_long i, n, clust;
	int j, gap, tmperrno;

	if (!needbavail || devp -> availsize != (u_long)0xffffffff)
		return(0);

	tmperrno = errno;
	if (!(buf = (u_char *)malloc(devp -> sectsize))) {
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}

	if (doswaitfunc) (*doswaitfunc)();
	n = 0;
	clust = devp -> totalsize + 2;;
	if (devp -> flags & F_FAT32)
	for (i = 0; i < devp -> fatsize; i++) {
		if (dosintrfunc && (*dosintrfunc)()) {
			free(buf);
			return(0);
		}
		if (sectread(devp, devp -> fatofs + i, buf, 1) < 0) {
			free(buf);
			return(-1);
		}
		for (j = 0; j < devp -> sectsize; j += 4) {
			if (clust) clust--;
			else break;
			if (!buf[j] && !buf[j + 1]
			&& !buf[j + 2] && !buf[j + 3]) n++;
		}
		if (!clust) break;
	}
	else if (devp -> flags & F_16BIT)
	for (i = 0; i < devp -> fatsize; i++) {
		if (dosintrfunc && (*dosintrfunc)()) {
			free(buf);
			return(0);
		}
		if (sectread(devp, devp -> fatofs + i, buf, 1) < 0) {
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
		if (sectread(devp, devp -> fatofs + i, buf, 1) < 0) {
			free(buf);
			return(-1);
		}
		if ((gap & 4)) {
			if (((gap & 3) == 2 && !buf[0])
			|| ((gap & 3) == 1 && !(buf[0] & 0x0f))) n++;
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
		if (j < devp -> sectsize * 2) {
			if (j % 2) {
				gap = (buf[j / 2] & 0xf0) ? 2 : 6;
			}
			else {
				gap = (buf[j / 2]) ? 1 : 5;
			}
		}
	}

	for (i = 0; i < maxdev; i++)
		if (devlist[i].drive == devp -> drive)
			devlist[i].availsize = n;
	free(buf);
	return(0);
}

#if	!MSDOS
static int NEAR readfat(devp)
devstat *devp;
{
	long size;
	int tmperrno;

	tmperrno = errno;
	size = (long)(devp -> fatsize) * (long)(devp -> sectsize);
	if (size > MAXFATMEM) devp -> fatbuf = NULL;
	else if ((devp -> fatbuf = (char *)malloc(size))
	&& sectread(devp, devp -> fatofs,
	(u_char *)(devp -> fatbuf), devp -> fatsize) < 0) {
		doserrno = errno;
		free(devp -> fatbuf);
		errno = tmperrno;
		return(-1);
	}

	errno = tmperrno;
	return(0);
}
#endif

static int NEAR writefat(devp)
devstat *devp;
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
		if (sectwrite(devp, i, (u_char *)buf, devp -> fatsize) < 0)
			return(-1);
	return(0);
}

static long NEAR getfatofs(devp, clust)
devstat *devp;
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
		return(-1);
	}
	return(ofs);
}

static u_char *NEAR readtmpfat(devp, ofs, nsect)
devstat *devp;
long ofs;
int nsect;
{
	u_char *buf;
	int tmperrno;

	tmperrno = errno;
	if (!(buf = (u_char *)malloc((devp -> sectsize) * nsect))) {
		doserrno = errno;
		errno = tmperrno;
		return(NULL);
	}
	ofs /= (devp -> sectsize);
	if (sectread(devp, (u_long)(devp -> fatofs) + ofs, buf, nsect) < 0) {
		free(buf);
		return(NULL);
	}
	return(buf);
}

static long NEAR getfatent(devp, clust)
devstat *devp;
long clust;
{
	u_char *fatp, *buf;
	long ofs;
	int nsect;

	if ((ofs = getfatofs(devp, clust)) < 0) return(-1);
	if ((devp -> fatbuf)) {
		buf = NULL;
		fatp = (u_char *)&(devp -> fatbuf[ofs]);
	}
	else {
		nsect = 1;
		if (!(devp -> flags & (F_16BIT | F_FAT32))
		&& (ofs % devp -> sectsize) >= devp -> sectsize - 1) nsect = 2;
		if (!(buf = readtmpfat(devp, ofs, nsect))) return(-1);
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
		if (clust % 2) ofs = (((*fatp) & 0xf0) >> 4)
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
	long ofs;
	int old, nsect;

	if ((ofs = getfatofs(devp, clust)) < 0) return(-1);
	if ((devp -> fatbuf)) {
		buf = NULL;
		fatp = (u_char *)&(devp -> fatbuf[ofs]);
	}
	else {
		nsect = 1;
		if (!(devp -> flags & (F_16BIT | F_FAT32))
		&& (ofs % devp -> sectsize) >= devp -> sectsize - 1) nsect = 2;
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
		long i;

		for (i = devp -> fatofs; i < devp -> dirofs;
		i += devp -> fatsize) {
			if (sectwrite(devp, i + (ofs / (devp -> sectsize)),
			buf, nsect) < 0) {
				free(buf);
				return(-1);
			}
		}
		free(buf);
	}

	if (devp -> availsize != (u_long)0xffffffff) {
		u_long avail;
		int i;

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
devstat *devp;
long clust;
{
	u_long sect;

	if (devp -> flags & F_FAT32) {
		if (!clust) clust = devp -> rootdir;
	}
	else if (!clust || clust >= ROOTCLUST) {
		sect = (clust) ? clust - ROOTCLUST : devp -> dirofs;
		if (sect >= (devp -> dirofs) + (devp -> dirsize)) {
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
		+ ((u_long)(devp -> dirofs) + (u_long)(devp -> dirsize));
	return(sect);
}

static long NEAR clust32(devp, dentp)
devstat *devp;
dent_t *dentp;
{
	long clust;

	clust = (long)byte2word(dentp -> clust);
	if (devp -> flags & F_FAT32)
		clust += (long)byte2word(dentp -> clust_h) << 16;
	return(clust);
}

static long NEAR newclust(devp)
devstat *devp;
{
	long clust, used;

	for (clust = MINCLUST; (used = getfatent(devp, clust)) >= 0; clust++)
		if (!used) break;
	if (used < 0 || clust > devp -> totalsize) {
		doserrno = ENOSPC;
		return(-1);
	}
	return(clust);
}

static long NEAR clustread(devp, buf, clust)
devstat *devp;
u_char *buf;
long clust;
{
	u_long sect;
	long next;

	if (!(sect = clust2sect(devp, clust))) return(-1);
	if (!(devp -> flags & F_FAT32)
	&& sect < (long)(devp -> dirofs) + (long)(devp -> dirsize))
		next = ROOTCLUST + sect + (long)(devp -> clustsize);
	else next = getfatent(devp, clust);

	if (buf && sectread(devp, sect, buf, devp -> clustsize) < 0)
		return(-1);
	return(next);
}

static long NEAR clustwrite(devp, buf, prev)
devstat *devp;
u_char *buf;
long prev;
{
	u_long sect;
	long clust;

	if ((clust = newclust(devp)) < 0) return(-1);
	if (!(sect = clust2sect(devp, clust))) {
		doserrno = EIO;
		return(-1);
	}
	if (buf && sectwrite(devp, sect, buf, devp -> clustsize) < 0)
		return(-1);

	if ((prev && putfatent(devp, prev, clust) < 0)
	|| putfatent(devp, clust, ENDCLUST) < 0) return(-1);
	return(clust);
}

static long NEAR clustexpand(devp, clust, fill)
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

static int NEAR clustfree(devp, clust)
devstat *devp;
long clust;
{
	long next;

	if (clust) for (;;) {
		if ((next = getfatent(devp, clust)) < 0
		|| putfatent(devp, clust, 0) < 0) return(-1);
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
bpb_t *bpbcache;
#endif
{
#if	!MSDOS
	int i, cc, fd, nsect;
#endif
	bpb_t *bpb;
	char *buf;
	long total;
	int tmperrno;

	tmperrno = errno;
#if	MSDOS
	if (!(buf = (char *)malloc(MAXSECTSIZE))) {
		doserrno = errno;
		errno = tmperrno;
		return(-1);
	}
	bpb = (bpb_t *)buf;
	if (rawdiskio(drv, 0, buf, 1, MAXSECTSIZE, 0) < 0) {
		doserrno = errno;
		free(buf);
		errno = tmperrno;
		return(-1);
	}
	memset((char *)devp, 0, sizeof(devstat));
	devp -> drive = drv;
#else	/* !MSDOS */
	buf = NULL;
	if (!(devp -> ch_head) || !(nsect = devp -> ch_sect)) return(0);

#ifdef	HDDMOUNT
	if (!(devp -> ch_cyl));
	else
#endif
	if (nsect <= 100) devp -> flags &= ~F_8SECT;
	else {
		nsect %= 100;
		devp -> flags |= F_8SECT;
	}

	if (bpbcache -> nfat) bpb = bpbcache;
	else {
		devp -> fd = -1;
		i = O_RDWR | O_BINARY;
#ifdef	HDDMOUNT
		if (!(devp -> ch_cyl) && toupper2(devp -> ch_head) != 'W') {
			i = O_RDONLY | O_BINARY;
			devp -> flags |= F_RONLY;
		}
#endif
#ifdef	LINUX
		if (i == O_RDWR | O_BINARY) {
			struct stat st1, st2;
			struct mntent *mntp;
			FILE *fp;

			if (stat(devp -> ch_name, &st1) < 0) {
				doserrno = errno;
				errno = tmperrno;
				return(-1);
			}
			if ((fp = setmntent(MOUNTED, "r"))) {
				while ((mntp = getmntent(fp))) {
#if	1
					if (strstr(mntp -> mnt_opts, "ro"))
						continue;
#endif
					if (stat(mntp -> mnt_fsname, &st2) < 0)
						continue;
					if (st1.st_ino == st2.st_ino) {
						i = O_RDONLY | O_BINARY;
						devp -> flags |= F_RONLY;
						break;
					}
				}
				endmntent(fp);
			}
		}
#endif	/* LINUX */
		if ((fd = open(devp -> ch_name, i, 0600)) < 0) {
#ifdef	EFORMAT
			if (errno == EFORMAT) {
				errno = tmperrno;
				return(0);
			}
#endif
			if (errno == EIO) errno = ENXIO;
			if ((errno != EROFS && errno != EACCES)
			|| i == (O_RDONLY | O_BINARY)
			|| (fd = open(devp -> ch_name,
			O_RDONLY | O_BINARY, 0600)) < 0) {
				doserrno = errno;
				errno = tmperrno;
				return(-1);
			}
			devp -> flags |= F_RONLY;
		}
#if	defined (LINUX) && defined (BLKFLSBUF)
		ioctl(fd, BLKFLSBUF, 0);
#endif

		cc = 0;
		for (i = 0; i < SLISTSIZ; i++)
			if (cc < sectsizelist[i]) cc = sectsizelist[i];
		if (!(buf = (char *)malloc(cc))) {
			close(fd);
			doserrno = errno;
			errno = tmperrno;
			return(-1);
		}
		for (i = 0; i < SLISTSIZ; i++) {
#ifdef	HDDMOUNT
			if (lseek64(fd, devp -> offset, L_SET) < 0) {
#else
			if (lseek64(fd, (off64_t)0, L_SET) < 0) {
#endif
				i = SLISTSIZ;
				break;
			}
			while ((cc = read(fd, buf, sectsizelist[i])) < 0
			&& errno == EINTR);
			if (cc >= 0) break;
		}
		if (i >= SLISTSIZ) {
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
	if (!total) total = byte2dword(bpb -> bigtotal);
#if	!MSDOS
# ifdef	HDDMOUNT
	if (devp -> ch_cyl)
# endif
	{
		int nh, ns;

		nh = byte2word(bpb -> nhead);
		ns = byte2word(bpb -> secttrack);
		if (!nh || !ns || nh != devp -> ch_head || ns != nsect
		|| (total + (nh * ns / 2)) / (nh * ns) != devp -> ch_cyl) {
			if (bpb != bpbcache)
				memcpy((char *)bpbcache,
					(char *)bpb, sizeof(bpb_t));
			if (buf) free(buf);
			errno = tmperrno;
			return(0);
		}
	}
#endif

	devp -> clustsize = bpb -> clustsize;
	devp -> sectsize = byte2word(bpb -> sectsize);
	if (!(devp -> clustsize) || !(devp -> sectsize)) {
		if (buf) free(buf);
		errno = tmperrno;
		return(-1);
	}

	devp -> fatofs = byte2word(bpb -> bootsect);
	devp -> fatsize = byte2word(bpb -> fatsize);
	devp -> dirsize = (long)byte2word(bpb -> maxdir) * DOSDIRENT
		/ (long)(devp -> sectsize);

	if (devp -> fatsize && devp -> dirsize) devp -> rootdir = 0;
	else {
		if (!(devp -> fatsize))
			devp -> fatsize = byte2dword(bpb -> bigfatsize);
		devp -> dirsize = 0;
		devp -> rootdir = byte2dword(bpb -> rootdir);
		devp -> flags |= F_FAT32;
	}

	devp -> dirofs = (devp -> fatofs) + (devp -> fatsize) * (bpb -> nfat);
	total -= (long)(devp -> dirofs) + (long)(devp -> dirsize);
	devp -> totalsize = total / (devp -> clustsize);
	devp -> availsize = (u_long)0xffffffff;

	if (!(devp -> flags & F_FAT32)) {
		if (!strncmp(bpb -> fsname, FS_FAT, sizeof(bpb -> fsname))
		|| !strncmp(bpb -> fsname, FS_FAT12, sizeof(bpb -> fsname)))
			devp -> flags &= ~F_16BIT;
		else if (!strncmp(bpb -> fsname, FS_FAT16,
		sizeof(bpb -> fsname))
		|| devp -> totalsize > MAX12BIT)
			devp -> flags |= F_16BIT;
	}

	if (buf) free(buf);
	errno = tmperrno;
	return(1);
}

#if	!MSDOS
static int NEAR readbpb(devp, drv)
devstat * devp;
int drv;
{
	bpb_t bpb;
	char *cp;
	int i, ret;

	cp = NULL;
	devp -> fd = -1;
	for (i = 0; fdtype[i].name; i++) {
		if (drv != (int)fdtype[i].drive) continue;
		if (!cp || strcmp(cp, fdtype[i].name)) {
			bpb.nfat = 0;
			if ((devp -> fd) >= 0) close(devp -> fd);
			memset((char *)devp, 0, sizeof(devstat));
			devp -> fd = -1;
		}
		memcpy((char *)devp, (char *)&(fdtype[i]), sizeof(devinfo));
		cp = fdtype[i].name;
		if ((ret = _readbpb(devp, &bpb)) < 0) return(-1);
		if (ret > 0) break;
	}
	if (!fdtype[i].name) {
		if ((devp -> fd) >= 0) close(devp -> fd);
		doserrno = ENODEV;
		return(-1);
	}
	return(0);
}
#endif	/* !MSDOS */

#ifdef	HDDMOUNT
static off64_t *NEAR _readpt(offset, extoffset, fd, head, sect, secsiz)
off64_t offset, extoffset;
int fd, head, sect, secsiz;
{
	u_char *cp, *buf;
	off64_t ofs, *tmp, *slice;
	int i, nslice, pofs, ps, pn, beg, siz;

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

	if (!(slice = (off64_t *)malloc(2 * sizeof(off64_t)))) return(NULL);
	slice[nslice = 0] = (off64_t)secsiz;
	slice[++nslice] = (off64_t)0;

	if (lseek64(fd, offset + beg, L_SET) < 0) return(slice);
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
			partition98_t *pt;

			pt = (partition98_t *)cp;
			if (pt -> filesys != PT98_FAT12
			&& pt -> filesys != PT98_FAT16
			&& pt -> filesys != PT98_FAT16X
			&& pt -> filesys != PT98_FAT32) continue;
			ofs = byte2word(pt -> s_cyl);
			ofs = ofs * head + pt -> s_head;
			ofs = ofs * sect + pt -> s_sect;
			ofs *= secsiz;
		}
		else {
			partition_t *pt;
			off64_t *sp;
			int n;

			pt = (partition_t *)cp;
			ofs = byte2dword(pt -> f_sect);
			ofs *= secsiz;
			if (pt -> filesys == PT_EXTEND
			|| pt -> filesys == PT_EXTENDLBA) {
				if (extoffset) ofs += extoffset;
				else extoffset = ofs;

				if (!(sp = _readpt(ofs, extoffset,
				fd, 0, 0, secsiz))) {
					free(buf);
					free(slice);
					return(NULL);
				}
				for (n = 0; sp[n + 1]; n++);
				if (!n) {
					free(sp);
					continue;
				}

				siz = (nslice + n + 1) * sizeof(off64_t);
				if (!(tmp = (off64_t *)realloc(slice, siz))) {
					free(buf);
					free(slice);
					free(sp);
					return(NULL);
				}
				slice = tmp;

				memcpy((char *)&(slice[nslice]),
					(char *)&(sp[1]), n * sizeof(off64_t));
				slice[nslice += n] = (off64_t)0;
				free(sp);
				continue;
			}
			else if (pt -> filesys != PT_FAT12
			&& pt -> filesys != PT_FAT16
			&& pt -> filesys != PT_FAT16X
			&& pt -> filesys != PT_FAT32
			&& pt -> filesys != PT_FAT32LBA
			&& pt -> filesys != PT_FAT16XLBA) continue;

			ofs += offset;
		}

		siz = (nslice + 1 + 1) * sizeof(off64_t);
		if (!(tmp = (off64_t *)realloc(slice, siz))) {
			free(buf);
			free(slice);
			return(NULL);
		}
		slice = tmp;
		slice[nslice++] = ofs;
		slice[nslice] = (off64_t)0;
	}
	free(buf);

	return(slice);
}

off64_t *readpt(devfile, pc98)
char *devfile;
int pc98;
{
# ifdef	DIOCGDINFO
	struct disklabel dl;
	off64_t *slice;
	int fd, head, sect, size;

	if ((fd = open(devfile, O_RDONLY | O_BINARY, 0600)) < 0) {
		if (errno == EIO) errno = ENXIO;
		return(NULL);
	}

	/* hack for disk initialize */
	read(fd, &head, sizeof(head));

	head = sect = 0;
	if (ioctl(fd, DIOCGDINFO, &dl) < 0) {
		close(fd);
		return(NULL);
	}
	if (pc98) {
		head = dl.d_ntracks;
		sect = dl.d_nsectors;
	}

#  ifdef	SOLARIS
	{
		struct vtoc toc;

		if (ioctl(fd, DKIOCGVTOC, &toc) < 0) {
			close(fd);
			return(NULL);
		}
		size = toc.v_sectorsz;
	}
#  else
	size = D_SECSIZE(dl);
#  endif
	slice = _readpt((off64_t)0, (off64_t)0, fd, head, sect, size);

	close(fd);
	return(slice);
# else	/* !DIOCGDINFO */
	errno = ENODEV;
	return(NULL);
# endif	/* !DIOCGDINFO */
}
#endif	/* HDDMOUNT */

static int NEAR opendev(drive)
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
	drv = toupper2(drive);

	for (i = maxorder - 1; i >= 0; i--)
		if (drv == (int)devlist[devorder[i]].drive) break;
	if (i >= 0) {
		memcpy((char *)&dev,
			(char *)&(devlist[devorder[i]]), sizeof(devstat));
		dev.flags |= F_DUPL;
		dev.flags &= ~(F_CACHE | F_WRFAT);
	}
	else {
		if (readbpb(&dev, drv) < 0) return(-1);
		dev.dircache = NULL;
#if	MSDOS
		dev.fatbuf = NULL;
#else
		if (readfat(&dev) < 0) {
			close(dev.fd);
			return(-1);
		}
#endif
	}
	if (drive >= 'a' && drive <= 'z') dev.flags |= F_VFAT;
	else dev.flags &= ~F_VFAT;

	memcpy((char *)&(devlist[new]), (char *)&dev, sizeof(devstat));
	if (new >= maxdev) maxdev++;
	devorder[maxorder++] = new;
	return(new);
}

static int NEAR closedev(dd)
int dd;
{
	int i, ret, tmperrno;

	if (dd < 0 || dd >= maxdev || !devlist[dd].drive) {
		doserrno = EBADF;
		return(-1);
	}

	tmperrno = errno;
	ret = 0;
	if (!(devlist[dd].flags & F_DUPL)) {
		for (i = 0; i < maxdev; i++)
			if (i != dd && devlist[i].drive == devlist[dd].drive
			&& (devlist[i].flags & F_DUPL)) break;
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

	if (writefat(&(devlist[dd])) < 0) ret = -1;
	if (devlist[dd].flags & F_CACHE) free(devlist[dd].dircache);

	if (devlist[dd].flags & F_DUPL) flushcache(&(devlist[dd]));
	else {
		for (i = maxsectcache - 1; i >= 0; i -= cachesize[i]) {
			if (cachedrive[i] == devlist[dd].drive
			&& shiftcache(&(devlist[dd]), i, -1) > SECTCACHESIZE)
				ret = -1;
		}
		if (devlist[dd].fatbuf) free(devlist[dd].fatbuf);
#if	!MSDOS
		close(devlist[dd].fd);
#endif
	}

	for (i = maxorder - 1; i >= 0; i--) if (devorder[i] == dd) break;
	if (i >= 0) {
		maxorder--;
		for (; i < maxorder; i++) devorder[i] = devorder[i + 1];
	}

	devlist[dd].drive = '\0';
	while (maxdev > 0 && !devlist[maxdev - 1].drive) maxdev--;
	errno = tmperrno;
	return(ret);
}

int preparedrv(drive)
int drive;
{
	int i, drv;

	drv = toupper2(drive);
	for (i = maxdev - 1; i >= 0; i--)
		if (drv == (int)devlist[i].drive) return(DOSNOFILE + i);
	if (doswaitfunc) (*doswaitfunc)();
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

int flushdrv(drive, func)
int drive;
VOID_T (*func)__P_((VOID_A));
{
	devstat dev;
	u_long avail;
	int i, dd, drv, ret, tmperrno;

	drv = toupper2(drive);
	tmperrno = errno;
	ret = 0;
	dd = -1;
	for (i = maxorder - 1; i >= 0; i--) {
		if (drv != (int)devlist[devorder[i]].drive) continue;
		if (writefat(&(devlist[devorder[i]])) < 0) {
			tmperrno = doserrno;
			ret = -1;
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
		errno = tmperrno;
		return(ret);
	}

	for (i = maxsectcache - 1; i >= 0; i -= cachesize[i]) {
		if (drv != cachedrive[i]) continue;
		if (shiftcache(&(devlist[dd]), i, -1) > SECTCACHESIZE) {
			tmperrno = doserrno;
			ret = -1;
		}
	}
	if (devlist[dd].fatbuf) free(devlist[dd].fatbuf);
	if (devlist[dd].dircache) free(devlist[dd].dircache);
	avail = devlist[dd].availsize;
#if	!MSDOS
	close(devlist[dd].fd);
#endif
	if (func) (*func)();
#if	!MSDOS
	sync();
	sync();
#endif
	if (readbpb(&dev, drv) < 0) {
		tmperrno = doserrno;
		ret = -1;
	}
	else {
		dev.dircache = NULL;
		dev.availsize = avail;
#if	MSDOS
		dev.fatbuf = NULL;
#else
		if (readfat(&dev) < 0) {
			close(dev.fd);
			return(-1);
		}
#endif
	}
	if (drive >= 'a' && drive <= 'z') dev.flags |= F_VFAT;
	else dev.flags &= ~F_VFAT;

	memcpy((char *)&(devlist[dd]), (char *)&dev, sizeof(devstat));
	errno = tmperrno;
	return(ret);
}

static int NEAR checksum(name)
char *name;
{
	int i, sum;

	for (i = sum = 0; i < 8 + 3; i++) {
		sum = (((sum & 0x01) << 7) | ((sum & 0xfe) >> 1));
		sum += name[i];
	}
	return(sum & 0xff);
}

static u_short NEAR lfnencode(c1, c2)
u_char c1, c2;
{
#if	MSDOS
	if (!c1 && (c2 < ' ' || strchr(NOTINLFN, c2))) return(0xffff);
#else
	if (!c1 && (c2 < ' ' || strchr(NOTINLFN, c2))) return('_');
#endif
	return(cnvunicode(((u_short)c1 << 8) | c2, 1));
}

static u_short NEAR lfndecode(c1, c2)
u_char c1, c2;
{
	return(cnvunicode(((u_short)c1 << 8) | c2, 0));
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
#endif

static int NEAR sfntranschar(c)
int c;
{
	if ((u_char)c < ' ' || strchr(NOTINLFN, c)) return(-2);
	if (strchr(NOTINALIAS, c)) return(-1);
	if (strchr(PACKINALIAS, c)) return(0);
	return(toupper2(c));
}

static int NEAR cmpdospath(path1, path2, len, part)
char *path1, *path2;
int len, part;
{
	u_short w1, w2;
	int i, c1, c2;

	if (len < 0) len = strlen(path1);
	while (len > 0 && strchr(PACKINALIAS, path1[len - 1])) len--;
	for (i = 0; i < len; i++) {
		c1 = (u_char)(path1[i]);
		c2 = (u_char)(path2[i]);
		if (!issjis1(c1) || !issjis2(c2)) {
			if (!c1 || !c2 || toupper2(c1) != toupper2(c2))
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

static char *NEAR putdosname(buf, file, vol)
char *buf, *file;
int vol;
{
	char *cp, *eol, num[9];
	u_short w;
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
		else if (issjis1(*file) && issjis2(file[1])) {
			w = ((u_short)(*file) << 8) | (u_char)(file[1]);
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
			buf[i++] = *(cp++);
			buf[i] = *(cp++);
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
		sprintf(num, "%d", vol);
		for (i = 7 - (int)strlen(num); i > 0; i--)
			if (buf[i - 1] != ' ') break;
		buf[i++] = '~';
		for (j = 0; num[j]; j++) buf[i++] = num[j];
	}
	if (((u_char *)buf)[0] == 0xe5) buf[0] = 0x05;

	return(buf);
}

static u_short NEAR getdosmode(attr)
u_char attr;
{
	u_short mode;

	mode = 0;
#if	MSDOS
	if (attr & DS_IARCHIVE) mode |= S_ISVTX;
#endif
	if (!(attr & DS_IHIDDEN)) mode |= S_IREAD;
	if (!(attr & DS_IRDONLY)) mode |= S_IWRITE;
	mode |= (mode >> 3) | (mode >> 6);
	if (attr & DS_IFDIR) mode |= (S_IFDIR | S_IEXEC_ALL);
	else if (attr & DS_IFLABEL) mode |= S_IFIFO;
	else if (attr & DS_IFSYSTEM) mode |= S_IFSOCK;
	else mode |= S_IFREG;

	return(mode);
}

static u_char NEAR putdosmode(mode)
u_short mode;
{
	u_char attr;

	attr = 0;
#if	MSDOS
	if (mode & S_ISVTX) attr |= DS_IARCHIVE;
#endif
	if (!(mode & S_IREAD)) attr |= DS_IHIDDEN;
	if (!(mode & S_IWRITE)) attr |= DS_IRDONLY;
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

static int NEAR putdostime(buf, clock)
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
	u_short d, t;
	int mt;

	if (clock != (time_t)-1) {
		tmp = clock;
		mt = 0;
	}
	else {
#if	MSDOS
		ftime(&buffer);
		tmp = (time_t)(buffer.time);
		mt = (int)(buffer.millitm);
#else
		gettimeofday2(&t_val, &tz);
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

	if (!t && clock < 0) t = 0x0001;
	buf[0] = t & 0xff;
	buf[1] = (t >> 8) & 0xff;
	buf[2] = d & 0xff;
	buf[3] = (d >> 8) & 0xff;

	return(mt);
}

static int NEAR getdrive(path)
char *path;
{
	int i, drive;

	if (!(drive = _dospath(path))) {
		doserrno = ENOENT;
		return(-1);
	}

	if (lastdrive < 0) {
		for (i = 0; i < 'Z' - 'A' + 1; i++) curdir[i] = NULL;
		lastdrive = drive;
	}

	return(drive);
}

static int NEAR parsepath(buf, path, class)
char *buf, *path;
int class;
{
	char *cp;
	int i, n, drv, drive;

	cp = buf;
	if ((drive = getdrive(path)) < 0) return(-1);
	drv = toupper2(drive) - 'A';
	path += 2;
#if	MSDOS
	if (*path != '/' && *path != '\\') {
		*buf = _SC_;
		if (checkdrive(drv) <= 0) unixgetcurdir(buf + 1, drv + 1);
		else if (curdir[drv]) strcpy(buf + 1, curdir[drv]);
		else *buf = '\0';
#else
	if (*path != '/' && *path != '\\' && curdir[drv]) {
		*buf = _SC_;
		strcpy(buf + 1, curdir[drv]);
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
				strncpy(cp, path, i);
				cp += i;
			}
			else {
				n = i;
				if (i && path[i - 1] == '.') n--;
				if (cp + 1 + n - buf
				>= DOSMAXPATHLEN - 2 - 1) {
					doserrno = ENAMETOOLONG;
					return(-1);
				}
				*(cp++) = _SC_;
				strncpy(cp, path, n);
				cp += n;
			}
		}
		else if (!i || (i == 1 && path[0] == '.'));
		else if (i == 2 && path[0] == '.' && path[1] == '.') {
			cp = strrdelim2(buf, cp);
			if (!cp) cp = buf;
		}
		else {
			n = i;
			if (path[i - 1] == '.') n--;
			if (cp + 1 + n - buf >= DOSMAXPATHLEN - 2 - 1) {
				doserrno = ENAMETOOLONG;
				return(-1);
			}
			*(cp++) = _SC_;
			strncpy(cp, path, n);
			cp += n;
		}
		if (*(path += i)) path++;
	}
	if (cp == buf) *(cp++) = _SC_;
	else if (isdelim(buf, cp - 1 - buf)) cp--;
	*cp = '\0';

	return(drive);
}

static dosDIR *NEAR _dosopendir(path, resolved, needlfn)
char *path, *resolved;
int needlfn;
{
	dosDIR *xdirp;
	struct dirent *dp;
	dent_t *dentp;
	cache_t *cache;
	char *cp, *cachepath, buf[DOSMAXPATHLEN - 2];
	int len, dd, drive, rlen, tmperrno;

	rlen = 0;
	tmperrno = errno;
	if ((drive = parsepath(buf, path, 0)) < 0
	|| (dd = opendev(drive)) < 0) return(NULL);

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
		if (isdelim(cp, len - 1)) len--;
		if (!cmpdospath(cp, path, len, 1)) {
			xdirp -> dd_top =
			xdirp -> dd_off =
				clust32(&(devlist[dd]), &(cache -> dent));
			memcpy((char *)devlist[dd].dircache, (char *)cache,
				sizeof(cache_t));
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
		cp = path;
		if ((path = strdelim(path, 0))) len = (path++) - cp;
		else {
			len = strlen(cp);
			path = cp + len;
		}
		if (!len || (len == 1 && *cp == '.')) continue;

		if (!(dp = finddir(xdirp, cp, len, needlfn))) {
			errno = (doserrno) ? doserrno : ENOENT;
			_dosclosedir(xdirp);
			doserrno = errno;
			errno = tmperrno;
			return(NULL);
		}
		if (resolved) {
			char name[8 + 1 + 3 + 1];
			int flen;

			if (needlfn) cp = dp -> d_name;
			else {
				getdosname(name,
					dd2dentp(xdirp -> dd_fd) -> name,
					dd2dentp(xdirp -> dd_fd) -> ext);
				cp = name;
			}
			flen = strlen(cp);
			if (rlen + 1 + flen >= DOSMAXPATHLEN) {
				_dosclosedir(xdirp);
				doserrno = ENAMETOOLONG;
				errno = tmperrno;
				return(NULL);
			}
			resolved[rlen++] = _SC_;
			strcpy(resolved + rlen, cp);
			rlen += flen;
		}

		dentp = (dent_t *)&(xdirp -> dd_buf[dp -> d_fileno]);
		xdirp -> dd_top =
		xdirp -> dd_off = clust32(&(devlist[dd]), dentp);
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

static int NEAR _dosclosedir(xdirp)
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

static int NEAR seekdent(xdirp, dentp, clust, offset)
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
	if ((next = clustread(&(devlist[xdirp -> dd_fd]),
	xdirp -> dd_buf, xdirp -> dd_off)) < 0) return(-1);
	dd2clust(xdirp -> dd_fd) = xdirp -> dd_off;
	xdirp -> dd_off = next;
	dd2offset(xdirp -> dd_fd) = xdirp -> dd_loc = offset;
	if (dentp) memcpy((char *)dentp,
		(char *)&(xdirp -> dd_buf[xdirp -> dd_loc]), sizeof(dent_t));
	xdirp -> dd_loc += DOSDIRENT;
	if (xdirp -> dd_loc >= xdirp -> dd_size) xdirp -> dd_loc = 0;
	return(0);
}

static int NEAR readdent(xdirp, dentp, force)
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
		if ((next = clustread(&(devlist[xdirp -> dd_fd]),
		xdirp -> dd_buf, xdirp -> dd_off)) < 0) return(-1);
		dd2clust(xdirp -> dd_fd) = xdirp -> dd_off;
		xdirp -> dd_off = next;
	}

	dd2offset(xdirp -> dd_fd) = xdirp -> dd_loc;
	memcpy((char *)dentp,
		(char *)&(xdirp -> dd_buf[xdirp -> dd_loc]), sizeof(dent_t));
	if (force || dentp -> name[0]) {
		xdirp -> dd_loc += DOSDIRENT;
		if (xdirp -> dd_loc >= xdirp -> dd_size) xdirp -> dd_loc = 0;
	}
	return(0);
}

static struct dirent *NEAR _dosreaddir(xdirp, all)
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

	dp = (struct dirent *)&d;
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
	if ((cp = strrdelim(dd2path(xdirp -> dd_fd), 0))) cp++;
	else cp = dd2path(xdirp -> dd_fd);
	strcpy(cp, dp -> d_name);

	cnvunicode(0, -1);
	return(dp);
}

struct dirent *dosreaddir(dirp)
DIR *dirp;
{
	dosDIR *xdirp;
	struct dirent *dp;
	int i;
#if	!MSDOS
	int c;
#endif

	xdirp = (dosDIR *)dirp;
	if (!(dp = _dosreaddir(xdirp, 0))) reterr(NULL);
	if (!(devlist[xdirp -> dd_fd].flags & F_VFAT)) {
		for (i = 0; dp -> d_name[i]; i++) {
			if (issjis1(dp -> d_name[i])
			&& issjis2(dp -> d_name[i + 1])) i++;
#if	!MSDOS
			else if ((c = detranschar(dp -> d_name[i])))
				dp -> d_name[i] = c;
			else dp -> d_name[i] = tolower2(dp -> d_name[i]);
#endif
		}
	}
	return(dp);
}

static struct dirent *NEAR finddir(xdirp, fname, len, needlfn)
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
			if (!cmpdospath(tmp, dp -> d_name, len, 0)) return(dp);
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
	dosDIR *xdirp;
	char *tmp, buf[DOSMAXPATHLEN];
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
	char *cp;
	int i;

	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);

	i = toupper2(lastdrive) - 'A';
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
	pathname[0] = lastdrive;
	pathname[1] = ':';
	pathname[2] = _SC_;
	strcpy(pathname + 3, cp);
	return(pathname);
}

static dosDIR *NEAR splitpath(pathp, resolved, needlfn)
char **pathp, *resolved;
int needlfn;
{
	char dir[DOSMAXPATHLEN];
	int i, j;

	strcpy(dir, *pathp);
	for (i = 1, j = 2; dir[j]; j++) {
		if (issjis1(dir[j]) && issjis2(dir[j + 1])) j++;
		else if (dir[j] == '/' || dir[j] == '\\') i = j;
	}

	*pathp += i + 1;
	if (i < 3) dir[i++] = _SC_;
	dir[i] = '\0';

	return(_dosopendir(dir, resolved, needlfn));
}

static int NEAR getdent(path, ddp)
char *path;
int *ddp;
{
	dosDIR *xdirp;
	char buf[DOSMAXPATHLEN];
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

	memcpy((char *)(devlist[dd].dircache),
		(char *)(devlist[xdirp -> dd_fd].dircache),
		sizeof(cache_t));
	if (!*path || (isdotdir(path) && path <= buf + 4)) {
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

	memcpy((char *)(devlist[dd].dircache),
		(char *)(devlist[xdirp -> dd_fd].dircache),
		sizeof(cache_t));
	_dosclosedir(xdirp);
	errno = tmperrno;
	return(dd);
}

static int NEAR writedent(dd)
int dd;
{
	char *buf;
	u_long sect;
	long ret, offset;

	if (!(sect = clust2sect(&(devlist[dd]), dd2clust(dd)))) {
		doserrno = EIO;
		return(-1);
	}
	offset = dd2offset(dd);
	while (offset >= devlist[dd].sectsize) {
		sect++;
		offset -= devlist[dd].sectsize;
	}
	if (!(buf = (char *)malloc(devlist[dd].sectsize))) return(-1);
	if ((ret = sectread(&(devlist[dd]), sect, buf, 1)) >= 0) {
		memcpy(buf + offset, (char *)dd2dentp(dd), sizeof(dent_t));
		ret = sectwrite(&(devlist[dd]), sect, buf, 1);
	}
	free(buf);
	return(ret < 0 ? -1 : 0);
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
	if ((dd2clust(dd) = clustexpand(&(devlist[dd]), prev, 1)) < 0)
		return(-1);
	memset((char *)dd2dentp(dd), 0, sizeof(dent_t));
	dd2offset(dd) = 0;
	return(0);
}

static int NEAR creatdent(path, mode)
char *path;
int mode;
{
	dosDIR *xdirp;
	dent_t *dentp;
	u_char *cp, longfname[DOSMAXNAMLEN + 1];
	char *file, tmp[8 + 1 + 3 + 1], fname[8 + 3 + 1];
	char buf[DOSMAXPATHLEN];
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
	if (isdotdir(file)) {
		_dosclosedir(xdirp);
		doserrno = EACCES;
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
			if (!issjis1(file[i]) || !issjis2(file[i + 1]))
				c = lfnencode(0, file[i]);
			else {
				c = lfnencode(file[i], file[i + 1]);
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
		_dosclosedir(xdirp);
		doserrno = EACCES;
		return(-1);
	}

	for (i = 0; i < INHIBITNAMESIZ; i++)
		if (!strncmp(fname, inhibitname[i], 8)) break;
	if (i < INHIBITNAMESIZ
	|| (!strncmp(fname, INHIBITCOM, sizeof(INHIBITCOM) - 2)
	&& fname[sizeof(INHIBITCOM) - 1] > '0'
	&& fname[sizeof(INHIBITCOM) - 1] <= '0' + INHIBITCOMMAX
	&& fname[sizeof(INHIBITCOM)] == ' ')
	|| (!strncmp(fname, INHIBITLPT, sizeof(INHIBITLPT) - 2)
	&& fname[sizeof(INHIBITLPT) - 1] > '0'
	&& fname[sizeof(INHIBITLPT) - 1] <= '0' + INHIBITLPTMAX
	&& fname[sizeof(INHIBITLPT)] == ' ')) {
		_dosclosedir(xdirp);
		doserrno = EINVAL;
		return(-1);
	}

	if (!n || !strcmp(getdosname(tmp, fname, fname + 8), file)) cnt = 1;
	else sum = checksum(fname);

	i = j = 0;
	clust = -1;
	offset = 0;
	for (;;) {
		if (readdent(xdirp, dentp, 1) < 0) {
			if (clust >= 0) break;
			if (doserrno || expanddent(xdirp -> dd_fd) < 0) {
				*dd2path(xdirp -> dd_fd) = '\0';
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
				*dd2path(xdirp -> dd_fd) = '\0';
				_dosclosedir(xdirp);
				doserrno = EEXIST;
				return(-1);
			}
			n++;
			if (!putdosname(fname, file, n)) {
				*dd2path(xdirp -> dd_fd) = '\0';
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
		*dd2path(xdirp -> dd_fd) = '\0';
		_dosclosedir(xdirp);
		return(-1);
	}
	cp = (u_char *)dentp;
	while (--cnt > 0) {
		memset((char *)dentp, 0, sizeof(dent_t));
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
			_dosclosedir(xdirp);
			return(-1);
		}
	}

	memset((char *)dentp, 0, sizeof(dent_t));
	memcpy(dentp -> name, fname, 8 + 3);
	dentp -> attr = putdosmode(mode) | DS_IARCHIVE;
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
		ret = -1;
	else {
		ret = xdirp -> dd_fd;
		if ((cp = (u_char *)strrdelim(dd2path(ret), 0))) cp++;
		else cp = (u_char *)dd2path(ret);
		strcpy((char *)cp, file);
	}
	free(xdirp -> dd_buf);
	free(xdirp);
	return(ret);
}

static int NEAR unlinklfn(dd, clust, offset, sum)
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
		|| dd2dentp(dd) -> checksum != sum) break;
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
	char *cp, buf[DOSMAXPATHLEN], name[8 + 1 + 3 + 1];
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
	cp = strcatdelim(alias);
	if (!*path);
	else if (*name) strcpy(cp, name);
	else if (isdotdir(path)) strcpy(cp, path);
	else if (finddir(xdirp, path, 0, 0))
		getdosname(cp, dd2dentp(xdirp -> dd_fd) -> name,
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
	char *cp, buf[DOSMAXPATHLEN];
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
	cp = strcatdelim(resolved);
	if (!*path);
	else if ((dp = finddir(xdirp, path, 0, 1))) strcpy(cp, dp -> d_name);
	else {
		errno = (doserrno) ? doserrno : ENOENT;
		resolved = NULL;
	}
	_dosclosedir(xdirp);
	closedev(dd);
	return(resolved);
}
#endif	/* MSDOS */

int dosstatfs(drive, buf)
int drive;
char *buf;
{
	long block, total, avail;
	int dd;

	if ((dd = opendev(drive)) < 0) reterr(-1);
	block = (long)(devlist[dd].clustsize) * (long)(devlist[dd].sectsize);
	total = (long)(devlist[dd].totalsize);
	if (availfat(&(devlist[dd])) < 0) {
		closedev(dd);
		reterr(-1);
	}
	avail = (long)(devlist[dd].availsize);

	*((long *)&(buf[sizeof(long) * 0])) = block;
	*((long *)&(buf[sizeof(long) * 1])) = total;
	*((long *)&(buf[sizeof(long) * 2])) = avail;
	buf[sizeof(long) * 3] = 0;
	if (devlist[dd].flags & F_FAT32) buf[sizeof(long) * 3] |= 001;
	closedev(dd);
	return(0);
}

int dosstat(path, stp)
char *path;
struct stat *stp;
{
	char *cp;
	int dd;

	if ((dd = getdent(path, NULL)) < 0) reterr(-1);
	stp -> st_dev = dd;
	stp -> st_ino = clust32(&(devlist[dd]), dd2dentp(dd));
	stp -> st_mode = getdosmode(dd2dentp(dd) -> attr);
	stp -> st_nlink = 1;
	stp -> st_uid = -1;
	stp -> st_gid = -1;
	stp -> st_size = byte2dword(dd2dentp(dd) -> size);
	stp -> st_atime =
	stp -> st_mtime =
	stp -> st_ctime = getdostime(byte2word(dd2dentp(dd) -> date),
		byte2word(dd2dentp(dd) -> time));
#if	!MSDOS && defined (UF_SETTABLE) && defined (SF_SETTABLE)
	stp -> st_flags = 0;
#endif
	closedev(dd);

	if ((stp -> st_mode & S_IFMT) != S_IFDIR
	&& (cp = strrchr(path, '.')) && strlen(++cp) == 3) {
		if (!strcasecmp2(cp, "BAT") || !strcasecmp2(cp, "COM")
		|| !strcasecmp2(cp, "EXE")) stp -> st_mode |= S_IEXEC_ALL;
	}
	return(0);
}

int doslstat(path, stp)
char *path;
struct stat *stp;
{
	return(dosstat(path, stp));
}

int dosaccess(path, mode)
char *path;
int mode;
{
	struct stat st;

	if (dosstat(path, &st) < 0) return(-1);

	if (((mode & R_OK) && !(st.st_mode & S_IREAD))
	|| ((mode & W_OK)
		&& (!(st.st_mode & S_IWRITE)
		|| (devlist[st.st_dev].flags & F_RONLY)))
	|| ((mode & X_OK) && !(st.st_mode & S_IEXEC))) {
		errno = EACCES;
		return(-1);
	}
	return(0);
}

#if	!MSDOS
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
#endif	/* !MSDOS */

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
	*dd2path(dd) = '\0';

	ret = 0;
	if (writedent(dd) < 0
	|| clustfree(&(devlist[dd]), clust32(&(devlist[dd]), dd2dentp(dd))) < 0
	|| unlinklfn(dd, lfn_clust, lfn_offset, sum) < 0
	|| writefat(&(devlist[dd])) < 0) {
		errno = doserrno;
		ret = -1;
	}
	closedev(dd);
	return(ret);
}

int dosrename(from, to)
char *from, *to;
{
	char buf[DOSMAXPATHLEN];
	long clust, rmclust;
	u_short offset;
	int i, dd, fd, sum, ret;

	if ((i = parsepath(buf, to, 0)) < 0) reterr(-1);
	if (toupper2(i) != toupper2(getdrive(from))) {
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
	if (*(dd2dentp(dd) -> name) == '.') {
		closedev(dd);
		errno = EACCES;
		return(-1);
	}
	if ((fd = dosopen(to, O_RDWR | O_CREAT, 0666)) < 0) {
		closedev(dd);
		return(-1);
	}
	fd -= DOSFDOFFSET;
	rmclust = clust32(fd2devp(fd), fd2dentp(fd));
	sum = checksum(dd2dentp(dd) -> name);
	memcpy((char *)&(fd2dentp(fd) -> attr),
		(char *)&(dd2dentp(dd) -> attr),
		sizeof(dent_t) - (8 + 3));
	*(dd2dentp(dd) -> name) = 0xe5;
	*dd2path(dd) = '\0';

	free(dosflist[fd]._base);
	dosflist[fd]._base = NULL;
	while (maxdosf > 0 && !dosflist[maxdosf - 1]._base) maxdosf--;
	fd2clust(fd) = dosflist[fd]._clust;
	fd2offset(fd) = dosflist[fd]._offset;

	ret = 0;
	if (writedent(dosflist[fd]._file) < 0 || writedent(dd) < 0
	|| unlinklfn(dd, clust, offset, sum) < 0
	|| (rmclust && clustfree(&(devlist[dd]), rmclust) < 0)
	|| writefat(&(devlist[dd])) < 0) {
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
		&& clustfree(&(devlist[dd]),
		clust32(&(devlist[dd]), dd2dentp(dd))) < 0) {
			errno = doserrno;
			closedev(dd);
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
	dosflist[fd]._next = clust32(&(devlist[dd]), dd2dentp(dd));
	dosflist[fd]._off = 0;
	dosflist[fd]._loc = 0;
	dosflist[fd]._size = byte2dword(dd2dentp(dd) -> size);

	memcpy((char *)&(dosflist[fd]._dent), (char *)dd2dentp(dd),
		sizeof(dent_t));
	dosflist[fd]._clust = dd2clust(dd);
	dosflist[fd]._offset = dd2offset(dd);
	if (fd >= maxdosf) maxdosf = fd + 1;
	if ((flags & (O_WRONLY | O_RDWR)) && (flags & O_APPEND))
		doslseek(fd + DOSFDOFFSET, (off64_t)0, L_XTND);
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
		dosflist[fd]._dent.size[0] = dosflist[fd]._size & 0xff;
		dosflist[fd]._dent.size[1] = (dosflist[fd]._size >> 8) & 0xff;
		dosflist[fd]._dent.size[2] = (dosflist[fd]._size >> 16) & 0xff;
		dosflist[fd]._dent.size[3] = (dosflist[fd]._size >> 24) & 0xff;

		if (!(dosflist[fd]._dent.attr & DS_IFDIR))
			putdostime(dosflist[fd]._dent.time, -1);
		memcpy((char *)fd2dentp(fd), (char *)&(dosflist[fd]._dent),
			sizeof(dent_t));
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

static int NEAR dosfilbuf(fd, wrt)
int fd, wrt;
{
	long size, new, prev;

	size = (wrt) ? dosflist[fd]._bufsize :
		(dosflist[fd]._size - dosflist[fd]._loc);
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
	if (size > dosflist[fd]._bufsize) size = dosflist[fd]._bufsize;
	dosflist[fd]._cnt = size;
	dosflist[fd]._ptr = dosflist[fd]._base;

	return((int)size);
}

static int NEAR dosflsbuf(fd)
int fd;
{
	u_long sect;

	if (!(sect = clust2sect(fd2devp(fd), dosflist[fd]._off))) {
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
	int wrt;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf
	|| (buf && (dosflist[fd]._flag & O_WRONLY))) {
		errno = EBADF;
		return(-1);
	}
	wrt = (dosflist[fd]._flag & (O_WRONLY | O_RDWR)) ? 1 : 0;

	total = 0;
	while (nbytes > 0 && dosflist[fd]._loc < dosflist[fd]._size) {
		if (dosflist[fd]._cnt <= 0 && dosfilbuf(fd, wrt) < 0)
			reterr(-1);
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
		if (dosflist[fd]._cnt <= 0 && dosfilbuf(fd, 1) < 0) {
#if	MSDOS
			if (doserrno = ENOSPC) return(total);
#endif
			reterr(-1);
		}
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

off64_t doslseek(fd, offset, whence)
int fd;
off64_t offset;
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

	if (dosread(fd + DOSFDOFFSET, NULL, (int)offset) < 0) return(-1);
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
	dosflist[fd]._dent.attr &= ~DS_IARCHIVE;
	memcpy((char *)&(dent[0]), (char *)&(dosflist[fd]._dent),
		sizeof(dent_t));
	memset(dent[0].name, ' ', 8 + 3);
	dent[0].name[0] = '.';
	if ((clust = newclust(fd2devp(fd))) < 0) {
		errno = doserrno;
		dosflist[fd]._dent.name[0] = 0xe5;
		*fd2path(fd) = '\0';
		dosclose(fd + DOSFDOFFSET);
		return(-1);
	}
	dent[0].clust[0] = clust & 0xff;
	dent[0].clust[1] = (clust >> 8) & 0xff;
	if (fd2devp(fd) -> flags & F_FAT32) {
		dent[0].clust_h[0] = (clust >> 16) & 0xff;
		dent[0].clust_h[1] = (clust >> 24) & 0xff;
	}

	memcpy((char *)&(dent[1]), (char *)&(dosflist[fd]._dent),
		sizeof(dent_t));
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

	if (doswrite(fd + DOSFDOFFSET, (char *)dent, sizeof(dent_t) * 2) < 0) {
		tmp = errno;
		if ((clust = clust32(fd2devp(fd), &(dosflist[fd]._dent))))
			clustfree(fd2devp(fd), clust);
		dosflist[fd]._dent.name[0] = 0xe5;
		*fd2path(fd) = '\0';
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
	memcpy((char *)&cache, (char *)(devlist[xdirp -> dd_fd].dircache),
		sizeof(cache_t));

	clust = lfn_clust;
	offset = lfn_offset;

	while ((dp = _dosreaddir(xdirp, 0)))
		if (!isdotdir(dp -> d_name)) {
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
		memcpy((char *)(devlist[xdirp -> dd_fd].dircache),
			(char *)&cache, sizeof(cache_t));
		sum = checksum(dd2dentp(xdirp -> dd_fd) -> name);
		*(dd2dentp(xdirp -> dd_fd) -> name) = 0xe5;
		*dd2path(xdirp -> dd_fd) = '\0';
		if (writedent(xdirp -> dd_fd) < 0
		|| clustfree(&(devlist[xdirp -> dd_fd]),
			clust32(&(devlist[xdirp -> dd_fd]),
			dd2dentp(xdirp -> dd_fd))) < 0
		|| unlinklfn(xdirp -> dd_fd, clust, offset, sum) < 0
		|| writefat(&(devlist[xdirp -> dd_fd])) < 0) {
			errno = doserrno;
			ret = -1;
		}
	}
	_dosclosedir(xdirp);
	return(ret);
}

int dosfileno(stream)
FILE *stream;
{
	int fd;

	for (fd = 0; fd < maxdosf; fd++) {
		if (!dosflist[fd]._base) continue;
		if (stream == (FILE *)&(dosflist[fd])) return(fd);
	}
	return(-1);
}

static int NEAR type2flags(type)
char *type;
{
	int flags;

	switch (*type) {
		case 'r':
			if (*(type + 1) != '+') flags = O_RDONLY;
			else flags = O_RDWR;
			break;
		case 'w':
			if (*(type + 1) != '+') flags = O_WRONLY;
			else flags = O_RDWR;
			flags |= O_CREAT | O_TRUNC;
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
	return(flags);
}

FILE *dosfopen(path, type)
char *path, *type;
{
	int fd;

	if (!type) {
		errno = 0;
		return(NULL);
	}

	if ((fd = dosopen(path, type2flags(type), 0666)) < 0) return(NULL);
	return((FILE *)&(dosflist[fd - DOSFDOFFSET]));
}

FILE *dosfdopen(fd, type)
int fd;
char *type;
{
	int flags;

	fd -= DOSFDOFFSET;
	if (fd < 0 || fd >= maxdosf || !dosflist[fd]._base) {
		errno = EBADF;
		return(NULL);
	}
	flags = type2flags(type);
	if ((dosflist[fd]._flag & O_RDWR) != O_RDWR
	&& (flags & (O_RDONLY | O_WRONLY | O_RDWR))
	!= (dosflist[fd]._flag & (O_RDONLY | O_WRONLY | O_RDWR))) {
		errno = EINVAL;
		return(NULL);
	}
	if ((flags & (O_WRONLY | O_RDWR)) && (flags & O_APPEND))
		doslseek(fd + DOSFDOFFSET, (off64_t)0, L_XTND);
	return((FILE *)&(dosflist[fd]));
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
	long rest;
	int fd, ret;

	if ((fd = dosfileno(stream)) < 0) {
		errno = EINVAL;
		return(0);
	}
	if (dosflist[fd]._flag & O_WRONLY) {
		errno = EBADF;
		dosflist[fd]._flag |= O_IOERR;
		return(0);
	}

	rest = dosflist[fd]._bufsize
		- (dosflist[fd]._ptr - dosflist[fd]._base);
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
	long nbytes, total, rest;
	int fd;

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

	rest = dosflist[fd]._bufsize
		- (dosflist[fd]._ptr - dosflist[fd]._base);
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
	long rest;
	int fd;

	if ((fd = dosfileno(stream)) < 0) {
		errno = EINVAL;
		return(EOF);
	}
	rest = dosflist[fd]._bufsize
		- (dosflist[fd]._ptr - dosflist[fd]._base);
	if (rest != dosflist[fd]._cnt) {
		if (dosflsbuf(fd) < 0) {
			dosflist[fd]._flag |= O_IOERR;
			reterr(EOF);
		}
		dosflist[fd]._cnt = rest;
	}
	if (flushcache(&(devlist[dosflist[fd]._file])) < 0) reterr(EOF);
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
#endif	/* !_NODOSDRIVE */
