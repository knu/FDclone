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

#ifdef	__GNUC__
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
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
			| DS_IFLABEL | DS_IFDIR | DS_IARCHIVE)

struct dosfind_t {
	u_char	keyattr __attribute__ ((packed));
	u_char	drive __attribute__ ((packed));
	char	body[8], ext[3] __attribute__ ((packed));
	char	reserve[8] __attribute__ ((packed));
	u_char	attr __attribute__ ((packed));
	u_short	wrtime, wrdate __attribute__ ((packed));
	u_long	size_l __attribute__ ((packed));
	char	name[13] __attribute__ ((packed));
};

struct lfnfind_t {
	u_long	attr __attribute__ ((packed));
	u_short	crtime, crdate, crtime_h1, crtime_h2 __attribute__ ((packed));
	u_short	actime, acdate, actime_h1, actime_h2 __attribute__ ((packed));
	u_short	wrtime, wrdate, wrtime_h1, wrtime_h2 __attribute__ ((packed));
	u_long	size_h, size_l __attribute__ ((packed));
	u_long	reserve1, reserve2 __attribute__ ((packed));
	char	name[MAXPATHLEN] __attribute__ ((packed));
	char	alias[14] __attribute__ ((packed));
};

extern int getcurdrv();
extern int setcurdrv();
extern int supportLFN();
extern char *shortname();
extern char *unixrealpath();
extern char *preparefile();
#ifdef	__GNUC__
extern char *adjustfname();
#endif
#ifdef	NOLFNEMU
#define	unixopendir		opendir
#define	unixclosedir		closedir
#define	unixreaddir		readdir
#define	unixseekdir		seekdir
#define	unixrename		rename
#define	unixmkdir		mkdir
#else	/* !NOLFNEMU */
extern DIR *unixopendir();
extern int unixclosedir();
extern struct dirent *unixreaddir();
extern int unixseekdir();
extern int unixrename();
extern int unixmkdir();
#endif	/* !NOLFNEMU */
extern char *unixgetcwd();
extern int unixstat();
extern int unixchmod();
