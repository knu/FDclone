/*
 *	fsinfo.h
 *
 *	definitions for file system information
 */

#ifndef	__FSINFO_H_
#define	__FSINFO_H_

#if	(!defined (USEMOUNTH) && !defined (USEFSDATA) \
&& (defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTINFOR) || defined (USEMNTINFO))) \
|| defined (USEMOUNTH) || defined (USEFSDATA)
#include <sys/mount.h>
#endif
#if	defined (USEGETFSSTAT) || defined (USEGETMNT)
#include <sys/fs_types.h>
#endif
#ifdef	USEGETFSENT
#include <fstab.h>
#endif
#if	defined (USESTATVFSH) || defined (USEGETVFSTAT)
#include <sys/statvfs.h>
#endif
#ifdef	USESTATFSH
#include <sys/statfs.h>
#endif
#ifdef	USEVFSH
#include <sys/vfs.h>
#endif

#ifdef	USESTATVFSH
# ifdef	USESTATVFS_T
typedef statvfs_t		statfs_t;
# else
typedef struct statvfs		statfs_t;
# endif
#define	Xstatfs			statvfs
#define	blocksize(fs)		((fs).f_frsize ? (fs).f_frsize : (fs).f_bsize)
#endif	/* USESTATVFSH */

#ifdef	USESTATFSH
#define	Xf_bavail		f_bfree
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEVFSH
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEMOUNTH
typedef struct statfs		statfs_t;
#define	blocksize(fs)		(fs).f_bsize
#endif

#ifdef	USEFSDATA
typedef struct fs_data		statfs_t;
#define	Xf_bsize		fd_req.bsize
#define	Xf_files		fd_req.gtot
#define	Xf_blocks		fd_req.btot
#define	Xf_bfree		fd_req.bfree
#define	Xf_bavail		fd_req.bfreen
#define	Xstatfs(p, b)		(statfs(p, b) - 1)
#define	blocksize(fs)		1024
#endif

#ifdef	USEFFSIZE
#define	Xf_bsize		f_fsize
#endif

#ifndef	Xf_bsize
#define	Xf_bsize		f_bsize
#endif
#ifndef	Xf_files
#define	Xf_files		f_files
#endif
#ifndef	Xf_blocks
#define	Xf_blocks		f_blocks
#endif
#ifndef	Xf_bfree
#define	Xf_bfree		f_bfree
#endif
#ifndef	Xf_bavail
#define	Xf_bavail		f_bavail
#endif

#if	defined (USESTATFSH) || defined (USEVFSH) || defined (USEMOUNTH)
# if	(STATFSARGS >= 4)
# define	Xstatfs(p, b)	statfs(p, b, sizeof(statfs_t), 0)
# else	/* STATFSARGS < 4 */
#  if	(STATFSARGS == 3)
#  define	Xstatfs(p, b)	statfs(p, b, sizeof(statfs_t))
#  else	/* STATFSARGS != 3 */
#   ifdef	USEFSTATFS
extern int Xstatfs __P_((CONST char *, statfs_t *));
#   else
#   define	Xstatfs		statfs
#   endif
#  endif	/* STATFSARGS != 3 */
# endif	/* STATFSARGS < 4 */
#endif	/* USESTATFSH || USEVFSH || USEMOUNTH */

#if	MSDOS
typedef struct _statfs_t {
	long Xf_bsize;
	long Xf_blocks;
	long Xf_bfree;
	long Xf_bavail;
	long Xf_files;
} statfs_t;
#define	Xstatfs			unixstatfs
#define	blocksize(fs)		(fs).Xf_bsize
#endif

#endif	/* !__FSINFO_H_ */
