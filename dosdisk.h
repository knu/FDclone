/*
 *	dosdisk.h
 *
 *	Type Definition for "dosdisk.c"
 */

#include <sys/param.h>

#define	DOSDIRENT	32
#define	SECTCACHESIZE	20
#define	SECTSIZE	512
#define	MAX12BIT	(0xff0 - 0x002)

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

typedef struct _bpb_t {
	u_char dummy[11];
	u_char sectsize[2];
	u_char clustsize;
	u_char bootsect[2];
	u_char nfat;
	u_char maxdir[2];
	u_char total[2];
	u_char media;
	u_char fatsize[2];
	u_char secttrack[2];
	u_char nhead[2];
	u_char dummy2[2];
	u_char bigtotal_l[2];
	u_char bigtotal_h[2];
} bpb_t;

typedef struct _dent_t {
	u_char name[8];
	u_char ext[3];
	u_char attr;
	u_char dummy[10];
	u_char time[2];
	u_char date[2];
	u_char clust[2];
	u_char size_l[2];
	u_char size_h[2];
} dent_t;

#define	DS_IRDONLY	001
#define	DS_IHIDDEN	002
#define	DS_IFSYSTEM	004
#define	DS_IFLABEL	010
#define	DS_IFDIR	020
#define	DS_IARCHIVE	040

typedef struct _cache_t {
	char path[MAXPATHLEN + 1];
	dent_t dent;
	long clust;
	u_short offset;
} cache_t;

typedef struct _devinfo {
	char drive;
	char *name;
	u_char head;
	u_short sect;
	u_short cyl;
} devinfo;

typedef struct _devstat {
	char drive;
	char *fatbuf;		/* ch_name */
	u_char clustsize;	/* ch_head */
	u_short sectsize;	/* ch_sect */
	u_short fatofs;		/* ch_cyl */
	u_short fatsize;
	u_short dirofs;
	u_short dirsize;
	int fd;
	cache_t *dircache;
	u_char flags;
} devstat;

#define	F_RONLY	001
#define	F_16BIT	002
#define	F_8SECT	004
#define	F_DUPL	010
#define	F_CACHE	020
#define	F_WRFAT	040

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

#define	byte2word(p)	((p)[0] + ((p)[1] << 8))
#define	dd2dentp(dd)	(&(devlist[dd].dircache -> dent))
#define	dd2path(dd)	(devlist[dd].dircache -> path)
#define	dd2clust(dd)	(devlist[dd].dircache -> clust)
#define	dd2offset(dd)	(devlist[dd].dircache -> offset)
#define	fd2devp(fd)	(&devlist[dosflist[fd]._file])
#define	fd2dentp(fd)	(dd2dentp(dosflist[fd]._file))
#define	fd2path(fd)	(dd2path(dosflist[fd]._file))
#define	fd2clust(fd)	(dd2clust(dosflist[fd]._file))
#define	fd2offset(fd)	(dd2offset(dosflist[fd]._file))

#define	dosfeof(p)	((((p) -> _flag) & O_IOEOF) != 0)
#define	dosferror(p)	((((p) -> _flag) & O_IOERR) != 0)
#define	dosclearerr(p)	((p) -> _flag &= ~(O_IOERR | O_IOEOF))

typedef struct _dosdirdesc {
	int dd_id;
	int dd_fd;
	long dd_loc;
	long dd_size;
	long dd_bsize;
	long dd_off;
	char *dd_buf;
} dosDIR;

extern int preparedrv();
extern int shutdrv();
extern DIR *dosopendir();
extern int dosclosedir();
extern struct dirent *dosreaddir();
extern int doschdir();
#ifdef	USEGETWD
extern char *dosgetwd();
#else
extern char *dosgetcwd();
#endif
extern int dosstat();
extern int doslstat();
extern int dosaccess();
extern int dossymlink();
extern int dosreadlink();
extern int doschmod();
#ifdef	USEUTIME
extern int dosutime();
#else
extern int dosutimes();
#endif
extern int dosunlink();
extern int dosrename();
extern int dosopen();
extern int dosclose();
extern int dosread();
extern int doswrite();
extern int dosmkdir();
extern int dosrmdir();
extern int stream2fd();
extern FILE *dosfopen();
extern int dosfclose();
extern int dosfread();
extern int dosfwrite();
extern int dosfflush();
extern int dosfgetc();
extern int dosfputc();
extern char *dosfgets();
extern int dosfputc();
extern int dosallclose();
