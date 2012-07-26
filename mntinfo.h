/*
 *	mntinfo.h
 *
 *	definitions & function prototype declarations for "mntinfo.c"
 */

#ifndef	__MNTINFO_H_
#define	__MNTINFO_H_

#ifdef	USEMNTENTH
#include <mntent.h>
#endif
#ifdef	USEMNTTABH
#include <sys/mnttab.h>
#endif
#ifdef	USEMNTCTL
#include <fshelp.h>
#include <sys/vfs.h>
#include <sys/mntctl.h>
#include <sys/vmount.h>
#endif

#define	ETCMTAB			"/etc/mtab"
#define	PROCMOUNTS		"/proc/mounts"
#define	DEVROOT			"/dev/root"
#define	ROOTFS			"rootfs"

#ifdef	USEMNTENTH
typedef struct mntent		mnt_t;
#define	Xmnt_fsname		mnt_fsname
#define	Xmnt_dir		mnt_dir
#define	Xmnt_type		mnt_type
#define	Xmnt_opts		mnt_opts
#define	Xsetmntent		setmntent
#define	Xgetmntent(f, m)	getmntent(f)
#define	Xendmntent		endmntent
#endif

#ifdef	USEMNTTABH
#define	MOUNTED			MNTTAB
typedef struct mnttab		mnt_t;
#define	Xmnt_fsname		mnt_special
#define	Xmnt_dir		mnt_mountp
#define	Xmnt_type		mnt_fstype
#define	Xmnt_opts		mnt_mntopts
#define	Xsetmntent		fopen
#define	Xgetmntent(f, m)	(getmntent(f, m) ? NULL : m)
#define	Xendmntent		fclose
#endif

#if	defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTCTL) || defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETMNT) || defined (USEREADMTAB)
typedef struct _mnt_t {
	CONST char *Xmnt_fsname;
	CONST char *Xmnt_dir;
	CONST char *Xmnt_type;
	CONST char *Xmnt_opts;
} mnt_t;
# ifdef	USEREADMTAB
# define	Xendmntent	fclose
# else	/* !USEREADMTAB */
#  if	defined (USEMNTINFO) || defined (USEGETMNT)
#  define	Xendmntent(f)
#  else
#  define	Xendmntent	Xfree
#  endif
# endif	/* !USEREADMTAB */
#endif	/* USEGETFSSTAT || USEGETVFSTAT || USEMNTCTL \
|| USEMNTINFOR || USEMNTINFO || USEGETMNT || USEREADMTAB */

#ifdef	USEGETFSENT
typedef struct fstab		mnt_t;
#define	Xmnt_fsname		fs_spec
#define	Xmnt_dir		fs_file
#define	Xmnt_type		fs_vfstype
#define	Xmnt_opts		fs_mntops
#define	Xsetmntent(f, m)	(FILE *)setfsent()
#define	Xgetmntent(f, m)	getfsent()
#define	Xendmntent(fp)		endfsent()
#endif

#if	MSDOS
# ifdef	DOUBLESLASH
# define	MNTDIRSIZ	MAXPATHLEN
# else
# define	MNTDIRSIZ	(3 + 1)
# endif
typedef struct _mnt_t {
	CONST char *Xmnt_fsname;
	char Xmnt_dir[MNTDIRSIZ];
	CONST char *Xmnt_type;
	CONST char *Xmnt_opts;
} mnt_t;
#endif	/* MSDOS */

#ifdef	USEPROCMNT
#undef	MOUNTED
#define	MOUNTED			PROCMOUNTS
#endif
#ifndef	MOUNTED
#define	MOUNTED			ETCMTAB
#endif

#if	defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTCTL) || defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETMNT) || defined (USEREADMTAB)
extern FILE *Xsetmntent __P_((CONST char *, CONST char *));
extern mnt_t *Xgetmntent __P_((FILE *, mnt_t *));
#endif
extern char *Xhasmntopt __P_((mnt_t *, CONST char *));

#endif	/* !__MNTINFO_H_ */
