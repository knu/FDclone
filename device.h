/*
 *	device.h
 *
 *	definitions for device ID
 */

#ifdef	USEMKDEVH
#include <sys/mkdev.h>
#else	/* !USEMKDEVH */
# ifdef	USEMKNODH
# include <sys/mknod.h>
# else	/* !USEMKNODH */
#  ifdef	SVR4
#  include <sys/sysmacros.h>
#  endif
# endif	/* !USEMKNODH */
#endif	/* !USEMKDEVH */

#ifdef	SVR4
#define	BIT_MAJOR		15
#define	BIT_MINOR		18
#else
#define	BIT_MAJOR		8
#define	BIT_MINOR		8
#endif
#define	MASK_MAJOR		(((u_long)1 << BIT_MAJOR) - 1)
#define	MASK_MINOR		(((u_long)1 << BIT_MINOR) - 1)

#ifndef	major
#define	major(n)		((((u_long)(n)) >> BIT_MINOR) & MASK_MAJOR)
#endif
#ifndef	minor
#define	minor(n)		(((u_long)(n)) & MASK_MINOR)
#endif
#ifndef	makedev
#define	makedev(ma, mi)		(((((u_long)(ma)) & MASK_MAJOR) << BIT_MAJOR) \
				| (((u_long)(mi)) & MASK_MINOR))
#endif

#if	MSDOS
typedef short			r_dev_t;
#else
typedef dev_t			r_dev_t;
#endif
