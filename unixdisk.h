/*
 *	unixdisk.h
 *
 *	Type Definition for "unixdisk.c"
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <dos.h>

#ifndef	USEUTIME
#include <sys/time.h>
#endif

#ifdef	__GNUC__
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#define	FAR
#define	NOP	0x00
#define	RETF	0xc3
# if	(DJGPP >= 2)
# include <libc/dosio.h>
# else
# define	__dpmi_regs	_go32_dpmi_registers
# define	__dpmi_int	_go32_dpmi_simulate_int
# define	_dos_ds		_go32_info_block.selector_for_linear_memory
# define	__tb	_go32_info_block.linear_address_of_transfer_buffer
# define	__tb_offset	(__tb & 15)
# define	__tb_segment	(__tb / 16)
# endif
#else
typedef union REGS	__dpmi_regs;
#define	__attribute__(x)
#define	FAR	far
#define	NOP	0x90
#define	RETF	0xcb
#endif

#include "unixemu.h"

#define	DATETIMEFORMAT	1
#define	DS_IRDONLY	001
#define	DS_IHIDDEN	002
#define	DS_IFSYSTEM	004
#define	DS_IFLABEL	010
#define	DS_IFDIR	020
#define	DS_IARCHIVE	040
#define	SEARCHATTRS	(DS_IRDONLY | DS_IHIDDEN | DS_IFSYSTEM \
			| DS_IFDIR | DS_IARCHIVE)
#define	PSEUDOINTNO	0x80
#define	MAXINTNO	0xff

struct dosfind_t {
	u_char keyattr __attribute__ ((packed));
	u_char drive __attribute__ ((packed));
	char body[8], ext[3] __attribute__ ((packed));
	char reserve[8] __attribute__ ((packed));
	u_char attr __attribute__ ((packed));
	u_short wrtime, wrdate __attribute__ ((packed));
	u_long size_l __attribute__ ((packed));
	char name[13] __attribute__ ((packed));
};

struct lfnfind_t {
	u_long attr __attribute__ ((packed));
	u_short crtime, crdate, crtime_h1, crtime_h2 __attribute__ ((packed));
	u_short actime, acdate, actime_h1, actime_h2 __attribute__ ((packed));
	u_short wrtime, wrdate, wrtime_h1, wrtime_h2 __attribute__ ((packed));
	u_long size_h, size_l __attribute__ ((packed));
	u_long reserve1, reserve2 __attribute__ ((packed));
	char name[MAXPATHLEN] __attribute__ ((packed));
	char alias[14] __attribute__ ((packed));
};

struct iopacket_t {
	u_long sect __attribute__ ((packed));
	u_short size __attribute__ ((packed));
	u_short buf_off __attribute__ ((packed));
	u_short buf_seg __attribute__ ((packed));
};

extern int getcurdrv __P_((VOID_A));
extern int setcurdrv __P_((int));
extern int getdosver __P_((VOID_A));
extern int supportLFN __P_((char *));
extern char *getcurdir __P_((char *, int));
extern char *shortname __P_((char *, char *));
extern char *unixrealpath __P_((char *, char *));
extern char *preparefile __P_((char *, char *, int));
#ifdef	__GNUC__
extern char *adjustfname __P_((char *));
#endif
#ifndef	_NODOSDRIVE
extern int rawdiskio __P_((int, long, u_char *, int, int));
#endif
#ifdef	NOLFNEMU
#define	unixopendir		opendir
#define	unixclosedir		closedir
#define	unixreaddir		readdir
#define	unixrewinddir		rewinddir
#define	unixrename		rename
#define	unixmkdir		mkdir
#else	/* !NOLFNEMU */
extern DIR *unixopendir __P_((char *));
extern int unixclosedir __P_((DIR *));
extern struct dirent *unixreaddir __P_((DIR *));
extern int unixrewinddir __P_((DIR *));
extern int unixrename __P_((char *, char *));
extern int unixmkdir __P_((char *, int));
#endif	/* !NOLFNEMU */
extern char *unixgetcwd __P_((char *, int));
extern int unixstat __P_((char *, struct stat *));
extern int unixchmod __P_((char *, int));
#ifdef	USEUTIME
extern int unixutime __P_((char *, struct utimbuf *));
#else
extern int unixutimes __P_((char *, struct timeval []));
#endif
