/*
 *	unixdisk.h
 *
 *	Type Definition for "unixdisk.c"
 */

#include "machine.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <dos.h>
#include <sys/types.h>
#include <sys/stat.h>

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
	u_char	keyattr;
	u_char	drive;
	char	body[8], ext[3];
	u_long	reserve1, reserve2;
	u_char	attr;
	u_short	wrtime, wrdate;
	u_long	size_l;
	char	name[13];
};

struct lfnfind_t {
	u_long	attr;
	u_short	crtime, crdate, crtime_h1, crtime_h2;
	u_short	actime, acdate, actime_h1, actime_h2;
	u_short	wrtime, wrdate, wrtime_h1, wrtime_h2;
	u_long	size_h, size_l;
	u_long	reserve1, reserve2;
	char	name[MAXPATHLEN];
	char	alias[14];
};

extern int getcurdrv();
extern int setcurdrv();
extern int supportLFN();
extern char *unixrealpath();
extern char *preparefile();
#ifdef	__GNUC__
extern char *adjustfname();
#define	unixopendir		opendir
#define	unixclosedir		closedir
#define	unixreaddir		readdir
#define	unixrewinddir		rewinddir
#define	unixgetcwd		getcwd
#define	unixrename		rename
#define	unixmkdir		mkdir
#else	/* !__GNUC__ */
extern DIR *unixopendir();
extern int unixclosedir();
extern struct dirent *unixreaddir();
extern int unixrewinddir();
extern char *unixgetcwd();
extern int unixrename();
extern int unixmkdir();
#endif	/* !__GNUC__ */
extern int unixstat();
extern int unixchmod();
