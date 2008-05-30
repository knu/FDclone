/*
 *	namelist.h
 *
 *	definitions for listing files
 */

#ifndef	__NAMELIST_H_
#define	__NAMELIST_H_

#include "depend.h"

typedef struct _namelist {
	char *name;
	u_short ent;
	u_short st_mode;
	short st_nlink;
#ifndef	NOUID
	uid_t st_uid;
	gid_t st_gid;
#endif
#if	!defined (NOSYMLINK) && defined (DEP_LSPARSE)
	char *linkname;
#endif
#ifdef	HAVEFLAGS
	u_long st_flags;
#endif
	off_t st_size;
	time_t st_mtim;
	u_char flags;
	u_char tmpflags;
} namelist;

#define	F_ISEXE			0001
#define	F_ISWRI			0002
#define	F_ISRED			0004
#define	F_ISDIR			0010
#define	F_ISLNK			0020
#define	F_ISDEV			0040
#define	F_ISMRK			0001
#define	F_WSMRK			0002
#define	F_ISARG			0004
#define	F_STAT			0010
#define	F_ISCHGDIR		0020
#define	isdir(file)		((file) -> flags & F_ISDIR)
#define	islink(file)		((file) -> flags & F_ISLNK)
#define	isdev(file)		((file) -> flags & F_ISDEV)
#define	isfile(file)		(!((file) -> flags & (F_ISDIR | F_ISDEV)))
#define	isread(file)		((file) -> flags & F_ISRED)
#define	iswrite(file)		((file) -> flags & F_ISWRI)
#define	isexec(file)		((file) -> flags & F_ISEXE)
#define	ismark(file)		((file) -> tmpflags & F_ISMRK)
#define	wasmark(file)		((file) -> tmpflags & F_WSMRK)
#define	isarg(file)		((file) -> tmpflags & F_ISARG)
#define	havestat(file)		((file) -> tmpflags & F_STAT)
#define	ischgdir(file)		((file) -> tmpflags & F_ISCHGDIR)
#define	s_isdir(s)		((((s) -> st_mode) & S_IFMT) == S_IFDIR)
#define	s_isreg(s)		((((s) -> st_mode) & S_IFMT) == S_IFREG)
#define	s_islnk(s)		((((s) -> st_mode) & S_IFMT) == S_IFLNK)
#define	s_isfifo(s)		((((s) -> st_mode) & S_IFMT) == S_IFIFO)
#define	s_ischr(s)		((((s) -> st_mode) & S_IFMT) == S_IFCHR)
#define	s_isblk(s)		((((s) -> st_mode) & S_IFMT) == S_IFBLK)

#endif	/* !__NAMELIST_H_ */
