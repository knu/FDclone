/*
 *	dosdisk.h
 *
 *	Type Definition for "dosdisk.c"
 */

#if	!MSDOS
#include <sys/param.h>
#endif

#define	DOSDIRENT	32
#define	SECTCACHESIZE	32
#define	SECTSIZE	{512, 1024, 256, 128, 2048, 4096}
#define	STDSECTSIZE	512
#define	SECTCACHEMEM	(SECTCACHESIZE * STDSECTSIZE)
#define	MINCLUST	2
#define	MAX12BIT	(0xff0 - MINCLUST)
#define	DOSMAXPATHLEN	260
#define	MAXFATMEM	(512 * 9)	/* FAT size of 3.5inch 2HD */

#define	NOTINLFN	"\\/:*?\"<>|"
#define	NOTINALIAS	"+,;=[]"
#define	PACKINALIAS	" ."
#define	LFNENTSIZ	13
#define	DOSMAXNAMLEN	255
#define	INHIBITNAME	{ "CON     ", "AUX     ", "PRN     ", \
			  "CLOCK$  ", "CONFIG$ ", "NUL     " }
#define	INHIBITCOM	"COM"
#define	INHIBITCOMMAX	4
#define	INHIBITLPT	"LPT"
#define	INHIBITLPTMAX	9

#define	MAXDRIVEENTRY	32
#define	DOSFDOFFSET	(1 << (8 * sizeof(int) - 2))
#ifdef	NOFILE
#define	DOSNOFILE	NOFILE
#else
#define	DOSNOFILE	64
#endif

#ifdef	SYSVDIRENT
#define	d_fileno	d_ino
#endif

#ifndef	L_SET
# ifdef	SEEK_SET
# define	L_SET	SEEK_SET
# else
# define	L_SET	0
# endif
#endif
#ifndef	L_INCR
# ifdef	SEEK_CUR
# define	L_INCR	SEEK_CUR
# else
# define	L_INCR	1
# endif
#endif
#ifndef	L_XTND
# ifdef	SEEK_END
# define	L_XTND	SEEK_END
# else
# define	L_XTND	2
# endif
#endif

#ifndef	__attribute__
#define	__attribute__(x)
#endif

#if	(defined (i386) || defined (__i386) || defined (__i386__)) \
&& (defined (LINUX) || defined (JCCBSD) || defined (FREEBSD) \
|| defined (NETBSD) ||	defined (BSDOS) || defined (BOW) \
|| defined (ORG_386BSD))
#define	HDDMOUNT
typedef long long	off64_t;
#else
#define	off64_t		off_t
#endif

typedef struct _bpb_t {
	u_char jmpcode[3] __attribute__ ((packed));
	u_char makername[8] __attribute__ ((packed));
	u_char sectsize[2] __attribute__ ((packed));
	u_char clustsize __attribute__ ((packed));
	u_char bootsect[2] __attribute__ ((packed));
	u_char nfat __attribute__ ((packed));
	u_char maxdir[2] __attribute__ ((packed));
	u_char total[2] __attribute__ ((packed));
	u_char media __attribute__ ((packed));
	u_char fatsize[2] __attribute__ ((packed));
	u_char secttrack[2] __attribute__ ((packed));
	u_char nhead[2] __attribute__ ((packed));
	u_char hidden[4] __attribute__ ((packed));
	u_char bigtotal[4] __attribute__ ((packed));
	u_char bigfatsize[4] __attribute__ ((packed));
	u_char exflags[2] __attribute__ ((packed));
	u_char filesys[2] __attribute__ ((packed));
	u_char rootdir[4] __attribute__ ((packed));
	u_char fsinfo[4] __attribute__ ((packed));
	u_char reserved[2] __attribute__ ((packed));
	char fsname[8] __attribute__ ((packed));
} bpb_t;

#define	FS_FAT		"FAT     "
#define	FS_FAT12	"FAT12   "
#define	FS_FAT16	"FAT16   "
#define	FS_FAT32	"FAT32   "

typedef struct _dent_t {
	u_char name[8] __attribute__ ((packed));
	u_char ext[3] __attribute__ ((packed));
	u_char attr __attribute__ ((packed));
	u_char reserved __attribute__ ((packed));
	u_char checksum __attribute__ ((packed));
	u_char ctime[2] __attribute__ ((packed));
	u_char cdate[2] __attribute__ ((packed));
	u_char adate[2] __attribute__ ((packed));
	u_char clust_h[2] __attribute__ ((packed));
	u_char time[2] __attribute__ ((packed));
	u_char date[2] __attribute__ ((packed));
	u_char clust[2] __attribute__ ((packed));
	u_char size[4] __attribute__ ((packed));
} dent_t;

#define	DS_IRDONLY	001
#define	DS_IHIDDEN	002
#define	DS_IFSYSTEM	004
#define	DS_IFLABEL	010
#define	DS_IFLFN	017
#define	DS_IFDIR	020
#define	DS_IARCHIVE	040

typedef struct _cache_t {
	char path[MAXPATHLEN + 1];
	dent_t dent;
	long clust;
	u_short offset;
} cache_t;

#if	!MSDOS
typedef struct _devinfo {
	u_char drive;
	char *name;
	u_char head;
	u_short sect;
	u_short cyl;
# ifdef	HDDMOUNT
	off64_t offset;
# endif
} devinfo;
#endif

typedef struct _devstat {
	u_char drive;
	char *fatbuf;		/* ch_name */
	u_char clustsize;	/* ch_head */
	u_short sectsize;	/* ch_sect */
	u_short fatofs;		/* ch_cyl */
#ifdef	HDDMOUNT
	off64_t offset;
#endif
	u_long fatsize;
	u_short dirofs;
	u_short dirsize;
	u_long totalsize;
	u_long availsize;
	long rootdir;
#if	!MSDOS
	int fd;
#endif
	cache_t *dircache;
	u_char flags;
} devstat;

#define	F_RONLY	0001
#define	F_16BIT	0002
#define	F_8SECT	0004
#define	F_DUPL	0010
#define	F_CACHE	0020
#define	F_WRFAT	0040
#define	F_VFAT	0100
#define	F_FAT32	0200

#define	ch_name	fatbuf
#define	ch_head	clustsize
#define	ch_sect	sectsize
#define	ch_cyl	fatofs

typedef struct _dosiobuf {
	int _cnt;
	char *_ptr;
	char *_base;
	int _bufsize;
	short _flag;
	int _file;
	long _top;
	long _next;
	long _off;
	long _loc;
	long _size;
	dent_t _dent;
	long _clust;
	u_short _offset;
} dosFILE;

#define	O_IOEOF	020000
#define	O_IOERR	040000

#define	byte2word(p)	((p)[0] + ((u_short)(p)[1] << 8))
#define	byte2dword(p)	((p)[0] + ((u_long)(p)[1] << 8) \
				+ ((u_long)(p)[2] << 16) \
				+ ((u_long)(p)[3] << 24))
#define	dd2dentp(dd)	(&(devlist[dd].dircache -> dent))
#define	dd2path(dd)	(devlist[dd].dircache -> path)
#define	dd2clust(dd)	(devlist[dd].dircache -> clust)
#define	dd2offset(dd)	(devlist[dd].dircache -> offset)
#define	fd2devp(fd)	(&(devlist[dosflist[fd]._file]))
#define	fd2dentp(fd)	(dd2dentp(dosflist[fd]._file))
#define	fd2path(fd)	(dd2path(dosflist[fd]._file))
#define	fd2clust(fd)	(dd2clust(dosflist[fd]._file))
#define	fd2offset(fd)	(dd2offset(dosflist[fd]._file))

#define	dosfeof(p)	(((((dosFILE *)(p)) -> _flag) & O_IOEOF) != 0)
#define	dosferror(p)	(((((dosFILE *)(p)) -> _flag) & O_IOERR) != 0)
#define	dosclearerr(p)	(((dosFILE *)(p)) -> _flag &= ~(O_IOERR | O_IOEOF))

typedef struct _dosdirdesc {
	int dd_id;
	int dd_fd;
	long dd_loc;
	long dd_size;
	long dd_top;
	long dd_off;
	char *dd_buf;
} dosDIR;

#ifdef	HDDMOUNT
typedef struct _partition_t {
	u_char boot __attribute__ ((packed));
	u_char s_head __attribute__ ((packed));
	u_char s_sect __attribute__ ((packed));
	u_char s_cyl __attribute__ ((packed));
	u_char filesys __attribute__ ((packed));
	u_char e_head __attribute__ ((packed));
	u_char e_sect __attribute__ ((packed));
	u_char e_cyl __attribute__ ((packed));
	u_char f_sect[4] __attribute__ ((packed));
	u_char t_sect[4] __attribute__ ((packed));
} partition_t;
#define	PART_SIZE	sizeof(partition_t)
#define	PART_TABLE	0x01be
#define	PART_NUM	4

#define	PT_FAT12	0x01
#define	PT_FAT16	0x04
#define	PT_EXTEND	0x05
#define	PT_FAT16X	0x06
#define	PT_NTFS		0x07
#define	PT_FAT32	0x0b
#define	PT_FAT32LBA	0x0c
#define	PT_FAT16XLBA	0x0e
#define	PT_EXTENDLBA	0x0f
#define	PT_LINUX	0x83
#define	PT_386BSD	0xa5
#define	PT_OPENBSD	0xa6
#define	PT_NETBSD	0xa9

typedef struct _partition98_t {
	u_char boot __attribute__ ((packed));
	u_char filesys __attribute__ ((packed));
	u_char reserved[2] __attribute__ ((packed));
	u_char ipl_sect __attribute__ ((packed));
	u_char ipl_head __attribute__ ((packed));
	u_char ipl_cyl[2] __attribute__ ((packed));
	u_char s_sect __attribute__ ((packed));
	u_char s_head __attribute__ ((packed));
	u_char s_cyl[2] __attribute__ ((packed));
	u_char e_sect __attribute__ ((packed));
	u_char e_head __attribute__ ((packed));
	u_char e_cyl[2] __attribute__ ((packed));
	u_char name[16] __attribute__ ((packed));
} partition98_t;
#define	PART98_SIZE	sizeof(partition98_t)
#define	PART98_TABLE	0x0200
#define	PART98_NUM	16

#define	PT98_FAT12	0x81	/* 0x80 | 0x01 */
#define	PT98_FAT16	0x91	/* 0x80 | 0x11 */
#define	PT98_FREEBSD	0x94	/* 0x80 | 0x14 */
#define	PT98_FAT16X	0xa1	/* 0x80 | 0x21 */
#define	PT98_NTFS	0xb1	/* 0x80 | 0x31 */
#define	PT98_386BSD	0xc4	/* 0x80 | 0x44 */
#define	PT98_FAT32	0xe1	/* 0x80 | 0x61 */
#define	PT98_LINUX	0xe2	/* 0x80 | 0x62 */
#endif	/* HDDMOUNT */

typedef struct _sfntable_t {
	u_short start;
	u_short end;
	u_short sfn;
} sfntable_t;

#ifdef	NODNAMLEN
typedef struct _st_dirent {
	char buf[sizeof(struct dirent) + MAXNAMLEN];
} st_dirent;
#else
typedef struct dirent st_dirent;
#endif

#ifdef	HDDMOUNT
extern off64_t *readpt __P_((char *, int));
#endif
extern int preparedrv __P_((int));
extern int shutdrv __P_((int));
extern DIR *dosopendir __P_((char *));
extern int dosclosedir __P_((DIR *));
extern struct dirent *dosreaddir __P_((DIR *));
extern int dosrewinddir __P_((DIR *));
extern int doschdir __P_((char *));
extern char *dosgetcwd __P_((char *, int));
#if	MSDOS
extern char *dosshortname __P_((char *, char *));
extern char *doslongname __P_((char *, char *));
#endif
extern int dosstatfs __P_((int, char *));
extern int dosstat __P_((char *, struct stat *));
extern int doslstat __P_((char *, struct stat *));
extern int dosaccess __P_((char *, int));
#if	!MSDOS
extern int dossymlink __P_((char *, char *));
extern int dosreadlink __P_((char *, char *, int));
#endif	/* !MSDOS */
extern int doschmod __P_((char *, int));
#ifdef	USEUTIME
extern int dosutime __P_((char *, struct utimbuf *));
#else
extern int dosutimes __P_((char *, struct timeval []));
#endif
extern int dosunlink __P_((char *));
extern int dosrename __P_((char *, char *));
extern int dosopen __P_((char *, int, int));
extern int dosclose __P_((int));
extern int dosread __P_((int, char *, int));
extern int doswrite __P_((int, char *, int));
extern off64_t doslseek __P_((int, off64_t, int));
extern int dosmkdir __P_((char *, int));
extern int dosrmdir __P_((char *));
extern int dosfileno __P_((FILE *));
extern FILE *dosfopen __P_((char *, char *));
extern int dosfclose __P_((FILE *));
extern int dosfread __P_((char *, int, int, FILE *));
extern int dosfwrite __P_((char *, int, int, FILE *));
extern int dosfflush __P_((FILE *));
extern int dosfgetc __P_((FILE *));
extern int dosfputc __P_((int, FILE *));
extern char *dosfgets __P_((char *, int, FILE *));
extern int dosfputs __P_((char *, FILE *));
extern int dosallclose __P_((VOID_A));
