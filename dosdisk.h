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
#define	DOSMAXPATHLEN	255

#define	NOTINLFN	"\\/:*?\"<>|"
#define	NOTINALIAS	" +,;=[]"
#define	LFNENTSIZ	13
#define	DOSMAXNAMLEN	255

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
#define	DS_IFLFN	017
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
	u_short totalsize;
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
#define	F_VFAT	0100

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

#ifdef	NODNAMLEN
typedef struct _st_dirent {
	char buf[sizeof(struct dirent) + MAXNAMLEN];
} st_dirent;
#else
typedef	struct dirent st_dirent;
#endif

extern int preparedrv __P_((int, VOID (*)__P_((VOID))));
extern int shutdrv __P_((int));
extern DIR *dosopendir __P_((char *));
extern int dosclosedir __P_((DIR *));
extern struct dirent *dosreaddir __P_((DIR *));
extern int dosrewinddir __P_((DIR *));
extern int doschdir __P_((char *));
extern char *dosgetcwd __P_((char *, int));
extern int dosstat __P_((char *, struct stat *));
extern int doslstat __P_((char *, struct stat *));
extern int dosaccess __P_((char *, int));
extern int dossymlink __P_((char *, char *));
extern int dosreadlink __P_((char *, char *, int));
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
extern off_t doslseek __P_((int, off_t, int));
extern int dosmkdir __P_((char *, int));
extern int dosrmdir __P_((char *));
extern int stream2fd __P_((FILE *));
extern FILE *dosfopen __P_((char *, char *));
extern int dosfclose __P_((FILE *));
extern int dosfread __P_((char *, int, int, FILE *));
extern int dosfwrite __P_((char *, int, int, FILE *));
extern int dosfflush __P_((FILE *));
extern int dosfgetc __P_((FILE *));
extern int dosfputc __P_((int, FILE *));
extern char *dosfgets __P_((char *, int, FILE *));
extern int dosfputs __P_((char *, FILE *));
extern int dosallclose __P_((VOID));
