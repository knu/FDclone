/*
 *	mntinfo.c
 *
 *	mount information module
 */

#include "headers.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#include "pathname.h"
#include "mntinfo.h"
#include "fsinfo.h"

#ifdef	USEREADMTAB
static char *NEAR getmntfield __P_((char **));
#endif

#if	defined (USEGETFSSTAT) || defined (USEGETVFSTAT) \
|| defined (USEMNTCTL) || defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETMNT)
static int mnt_ptr = 0;
static int mnt_size = 0;
#endif


#ifdef	USEMNTCTL
/*ARGSUSED*/
FILE *Xsetmntent(file, mode)
CONST char *file, *mode;
{
	char *buf;

	mntctl(MCTL_QUERY, sizeof(int), (struct vmount *)&mnt_size);
	buf = Xmalloc(mnt_size);
	mntctl(MCTL_QUERY, mnt_size, (struct vmount *)buf);
	mnt_ptr = 0;

	return((FILE *)buf);
}

mnt_t *Xgetmntent(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	struct vfs_ent *entp;
	struct vmount *vmntp;
	char *cp, *buf, *host;
	ALLOC_T len;

	if (mnt_ptr >= mnt_size) return(NULL);
	buf = (char *)fp;
	vmntp = (struct vmount *)&(buf[mnt_ptr]);

	cp = &(buf[mnt_ptr + vmntp -> vmt_data[VMT_OBJECT].vmt_off]);
	len = strlen(cp) + 1;
	if (!(vmntp -> vmt_flags & MNT_REMOTE)) {
		fsname = Xrealloc(fsname, len);
		memcpy(fsname, cp, len);
	}
	else {
		host = &(buf[mnt_ptr
			+ vmntp -> vmt_data[VMT_HOSTNAME].vmt_off]);
		len += strlen(host) + 1;
		fsname = Xrealloc(fsname, len);
		Xstrcpy(Xstrcpy(Xstrcpy(fsname, host), ":"), cp);
	}

	cp = &(buf[mnt_ptr + vmntp -> vmt_data[VMT_STUB].vmt_off]);
	len = strlen(cp) + 1;
	dir = Xrealloc(dir, len);
	memcpy(dir, cp, len);

	entp = getvfsbytype(vmntp -> vmt_gfstype);
	if (entp) {
		cp = entp -> vfsent_name;
		len = strlen(cp) + 1;
		type = Xrealloc(type, len);
		memcpy(type, cp, len);
	}
	else if (type) {
		Xfree(type);
		type = NULL;
	}

	mntp -> Xmnt_fsname = fsname;
	mntp -> Xmnt_dir = dir;
	mntp -> Xmnt_type = (type) ? type : "???";
	mntp -> Xmnt_opts =
		(vmntp -> vmt_flags & MNT_READONLY) ? "ro" : nullstr;
	mnt_ptr += vmntp -> vmt_length;

	return(mntp);
}
#endif	/* USEMNTCTL */

#if	defined (USEMNTINFOR) || defined (USEMNTINFO) \
|| defined (USEGETFSSTAT) || defined (USEGETVFSTAT)

# ifdef	USEGETVFSTAT
# define	f_flags			f_flag
# define	getfsstat2		getvfsstat
typedef struct statvfs		mntinfo_t;
# else
# define	getfsstat2		getfsstat
typedef struct statfs		mntinfo_t;
# endif

# if	!defined (MNT_RDONLY) && defined (M_RDONLY)
# define	MNT_RDONLY		M_RDONLY
# endif

/*ARGSUSED*/
FILE *Xsetmntent(file, mode)
CONST char *file, *mode;
{
# ifndef	USEMNTINFO
	int size;
# endif
	mntinfo_t *buf;

	buf = NULL;
	mnt_ptr = mnt_size = 0;

# ifdef	DEBUG
	_mtrace_file = "getmntinfo(start)";
# endif
# ifdef	USEMNTINFO
	mnt_size = getmntinfo(&buf, MNT_NOWAIT);
# else	/* !USEMNTINFO */
#  ifdef	USEMNTINFOR
	size = 0;
	getmntinfo_r(&buf, MNT_WAIT, &mnt_size, &size);
#  else
	size = (getfsstat2(NULL, 0, MNT_WAIT) + 1) * sizeof(mntinfo_t);
	if (size > 0) {
		buf = (mntinfo_t *)Xmalloc(size);
		mnt_size = getfsstat2(buf, size, MNT_WAIT);
	}
#  endif
# endif	/* !USEMNTINFO */
# ifdef	DEBUG
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "getmntinfo(end)";
		malloc(0);	/* dummy malloc */
	}
# endif

	return((FILE *)buf);
}

mnt_t *Xgetmntent(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
# if	defined (USEMNTINFO) || defined (USEGETVFSTAT)
#  ifdef	USEVFCNAME
	struct vfsconf *conf;
#  define	getvfsbynumber(n) \
				((conf = getvfsbytype(n)) \
				? conf -> vfc_name : NULL)
#  else	/* !USEVFCNAME */
#   ifdef	USEFFSTYPE
#   define	getvfsbynumber(n) \
				(buf[mnt_ptr].f_fstypename)
#   else	/* !USEFFSTYPE */
#    ifdef	INITMOUNTNAMES
	static char *mnt_names[] = INITMOUNTNAMES;
#    define	getvfsbynumber(n) \
				(((n) <= MOUNT_MAXTYPE) \
				? mnt_names[n] : NULL)
#    else
#    define	getvfsbynumber(n) \
				(NULL)
#    endif
#   endif	/* !USEFFSTYPE */
#  endif	/* !USEVFCNAME */
# else	/* !USEMNTINFO && !USEGETVFSTAT */
#  ifdef	USEGETFSSTAT
#  define	getvfsbynumber(n) \
				(((n) <= MOUNT_MAXTYPE) \
				? mnt_names[n] : NULL)
#  endif
# endif	/* !USEMNTINFO && !USEGETVFSTAT */
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	mntinfo_t *buf;
	char *cp;
	ALLOC_T len;

	if (mnt_ptr >= mnt_size) return(NULL);
	buf = (mntinfo_t *)fp;

# ifdef	DEBUG
	_mtrace_file = "getmntent(start)";
# endif
	len = strlen(buf[mnt_ptr].f_mntfromname) + 1;
	fsname = Xrealloc(fsname, len);
	memcpy(fsname, buf[mnt_ptr].f_mntfromname, len);

	len = strlen(buf[mnt_ptr].f_mntonname) + 1;
	dir = Xrealloc(dir, len);
	memcpy(dir, buf[mnt_ptr].f_mntonname, len);

	cp = (char *)getvfsbynumber(buf[mnt_ptr].f_type);
	if (cp) {
		len = strlen(cp) + 1;
		type = Xrealloc(type, len);
		memcpy(type, cp, len);
	}
	else if (type) {
		Xfree(type);
		type = NULL;
	}
# ifdef	DEBUG
	if (_mtrace_file) _mtrace_file = NULL;
	else {
		_mtrace_file = "getmntent(end)";
		malloc(0);	/* dummy malloc */
	}
# endif

	mntp -> Xmnt_fsname = fsname;
	mntp -> Xmnt_dir = dir;
	mntp -> Xmnt_type = (type) ? type : "???";
	mntp -> Xmnt_opts =
		(buf[mnt_ptr].f_flags & MNT_RDONLY) ? "ro" : nullstr;
	mnt_ptr++;

	return(mntp);
}
#endif	/* USEMNTINFOR || USEMNTINFO || USEGETFSSTAT || USEGETVFSTAT */

#ifdef	USEGETMNT
/*ARGSUSED*/
FILE *Xsetmntent(file, mode)
CONST char *file, *mode;
{
	mnt_ptr = 0;

	return((FILE *)1);
}

/*ARGSUSED*/
mnt_t *Xgetmntent(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char *fsname = NULL;
	static char *dir = NULL;
	static char *type = NULL;
	struct fs_data buf;
	ALLOC_T len;

	if (getmnt(&mnt_ptr, &buf, sizeof(buf), NOSTAT_MANY, NULL) <= 0)
		return(NULL);

	len = strlen(buf.fd_req.devname) + 1;
	fsname = Xrealloc(fsname, len);
	memcpy(fsname, buf.fd_req.devname, len);

	len = strlen(buf.fd_req.path) + 1;
	dir = Xrealloc(dir, len);
	memcpy(dir, buf.fd_req.path, len);

	len = strlen(gt_names[buf.fd_req.fstype]) + 1;
	type = Xrealloc(type, len);
	memcpy(type, gt_names[buf.fd_req.fstype], len);

	mntp -> Xmnt_fsname = fsname;
	mntp -> Xmnt_dir = dir;
	mntp -> Xmnt_type = type;
	mntp -> Xmnt_opts = (buf.fd_req.flags & M_RONLY) ? "ro" : nullstr;

	return(mntp);
}
#endif	/* USEGETMNT */

#ifdef	USEREADMTAB
static char *NEAR getmntfield(cpp)
char **cpp;
{
	char *s;

	if (!cpp || !*cpp || !**cpp) return(vnullstr);

	while (Xisspace(**cpp)) (*cpp)++;
	if (!**cpp) return(vnullstr);
	s = *cpp;

	while (**cpp && !Xisspace(**cpp)) (*cpp)++;
	if (**cpp) *((*cpp)++) = '\0';

	return(s);
}

FILE *Xsetmntent(file, mode)
CONST char *file, *mode;
{
	return(fopen(file, mode));
}

mnt_t *Xgetmntent(fp, mntp)
FILE *fp;
mnt_t *mntp;
{
	static char buf[BUFSIZ];
	char *cp;

	if (!(cp = fgets(buf, sizeof(buf), fp))) return(NULL);

	mntp -> Xmnt_fsname = mntp -> Xmnt_dir =
	mntp -> Xmnt_type = mntp -> Xmnt_opts = NULL;

	mntp -> Xmnt_fsname = getmntfield(&cp);
	mntp -> Xmnt_dir = getmntfield(&cp);
	mntp -> Xmnt_type = getmntfield(&cp);
	mntp -> Xmnt_opts = getmntfield(&cp);

	return(mntp);
}
#endif	/* USEREADMTAB */

char *Xhasmntopt(mntp, opt)
mnt_t *mntp;
CONST char *opt;
{
	CONST char *cp;
	ALLOC_T len;

	len = strlen(opt);
	for (cp = mntp -> Xmnt_opts; cp && *cp;) {
		if (!strncmp(cp, opt, len) && (!cp[len] || cp[len] == ','))
			return((char *)cp);
		if ((cp = Xstrchr(cp, ','))) cp++;
	}

	return(NULL);
}
