/*
 *	sysemu.h
 *
 *	definitions & function prototype declarations for "sysemu.c"
 */

#include "depend.h"
#include "dirent.h"
#include "stream.h"

#define	DEV_NORMAL		0
#define	DEV_DOS			1
#define	DEV_URL			2
#define	DEV_HTTP		3

struct utimes_t {
	time_t actime;
	time_t modtime;
};

extern int seterrno __P_((int));
#if	MSDOS && defined (DJGPP)
extern int dos_putpath __P_((CONST char *, int));
#endif
#if	MSDOS && !defined (FD)
extern int getcurdrv __P_((VOID_A));
extern int setcurdrv __P_((int, int));
extern char *unixrealpath __P_((CONST char *, char *));
#endif
#ifdef	DEP_DOSPATH
extern int _dospath __P_((CONST char *));
extern int dospath __P_((CONST char *, char *));
#endif
#ifdef	DEP_URLPATH
extern int _urlpath __P_((CONST char *, char **, int *));
extern int urlpath __P_((CONST char *, char **, char *, int *));
#endif
#ifdef	DEP_DOSDRIVE
# if	MSDOS
extern int dospath2 __P_((CONST char *));
extern int dospath3 __P_((CONST char *));
# else
#define	dospath2(path)		dospath(path, NULL)
#define	dospath3(path)		dospath(path, NULL)
# endif
#endif	/* DEP_DOSDRIVE */
#if	(defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE) \
|| defined (DEP_PSEUDOPATH)) \
&& defined (DEBUG)
extern VOID freeopenlist(VOID_A);
#endif
#ifdef	DEP_PSEUDOPATH
extern int checkdrv __P_((int, int *));
extern int preparedrv __P_((CONST char *, int *, char *));
extern VOID shutdrv __P_((int));
#endif
#ifdef	DEP_DOSPATH
extern u_int getunixmode __P_((u_int));
extern time_t getunixtime __P_((u_int, u_int));
extern u_short getdosmode __P_((u_int));
extern VOID getdostime __P_((u_short *, u_short *, time_t));
#endif
#if	defined (DEP_DIRENT)
extern DIR *Xopendir __P_((CONST char *));
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern VOID Xrewinddir __P_((DIR *));
#else
#define	Xopendir		opendir
#define	Xclosedir		closedir
#define	Xreaddir		readdir
#define	Xrewinddir		rewinddir
#endif
#if	MSDOS
extern int rawchdir __P_((CONST char *));
#else
#define	rawchdir(p)		((chdir(p)) ? -1 : 0)
#endif
extern int Xchdir __P_((CONST char *));
extern char *Xgetwd __P_((char *));
#ifdef	DEP_DIRENT
extern int Xstat __P_((CONST char *, struct stat *));
extern int Xlstat __P_((CONST char *, struct stat *));
#else
#define	Xstat(p, s)		((stat(p, s)) ? -1 : 0)
#define	Xlstat(p, s)		((lstat(p, s)) ? -1 : 0)
#endif
#ifdef	DEP_BIASPATH
extern int Xaccess __P_((CONST char *, int));
extern int Xsymlink __P_((CONST char *, CONST char *));
extern int Xreadlink __P_((CONST char *, char *, int));
extern int Xchmod __P_((CONST char *, int));
#else
#define	Xaccess(p, m)		((access(p, m)) ? -1 : 0)
#define	Xsymlink(o, n)		((symlink(o, n)) ? -1 : 0)
#define	Xreadlink		readlink
#define	Xchmod(p, m)		((chmod(p, m)) ? -1 : 0)
#endif
extern int rawutimes __P_((CONST char *, CONST struct utimes_t *));
extern int Xutimes __P_((CONST char *, CONST struct utimes_t *));
#ifdef	DEP_BIASPATH
# ifdef	HAVEFLAGS
extern int Xchflags __P_((CONST char *, u_long));
# endif
# ifndef	NOUID
extern int Xchown __P_((CONST char *, u_id_t, g_id_t));
# endif
extern int Xunlink __P_((CONST char *));
extern int Xrename __P_((CONST char *, CONST char *));
extern int Xopen __P_((CONST char *, int, int));
#else	/* !DEP_BIASPATH */
#define	Xchflags(p, f)		((chflags(p, f)) ? -1 : 0)
#define	Xchown(p, u, g)		((chown(p, u, g)) ? -1 : 0)
#define	Xunlink(p)		((unlink(p)) ? -1 : 0)
#define	Xrename(f, t)		((rename(f, t)) ? -1 : 0)
#define	Xopen			open
#endif	/* !DEP_BIASPATH */
#ifdef	DEP_PSEUDOPATH
extern VOID putopenfd __P_((int, int));
extern int chkopenfd __P_((int));
extern int delopenfd __P_((int));
extern int Xclose __P_((int));
extern int Xread __P_((int, char *, int));
extern int Xwrite __P_((int, CONST char *, int));
extern off_t Xlseek __P_((int, off_t, int));
#ifndef	NOFTRUNCATE
extern int Xftruncate __P_((int, off_t));
#endif
extern int Xdup __P_((int));
extern int Xdup2 __P_((int, int));
#else
#define	Xclose(f)		((close(f)) ? -1 : 0)
#define	Xread			read
#define	Xwrite			write
#define	Xlseek			lseek
#define	Xftruncate		ftruncate
#define	Xdup			safe_dup
#define	Xdup2			safe_dup2
#endif
extern int Xmkdir __P_((CONST char *, int));
#ifdef	DEP_BIASPATH
extern int Xrmdir __P_((CONST char *));
#else
#define	Xrmdir(p)		((rmdir(p)) ? -1 : 0)
#endif
#ifdef	NOFLOCK
#define	Xflock(f, o)		(0)
#else
extern int Xflock __P_((int, int));
#endif
#ifdef	NOSELECT
#define	checkread(f, b, n, t)	sureread(f, b, n)
#else
extern int checkread __P_((int, VOID_P, int, int));
#endif

#ifdef	DEP_PSEUDOPATH
extern int lastdrv;
#endif
#ifdef	DEP_DOSDRIVE
extern int dosdrive;
#endif
#ifdef	DEP_URLPATH
extern int urldrive;
extern int urlkcode;
#endif
#if	defined (FD) && !defined (NOSELECT)
extern int (*readintrfunc)__P_((VOID_A));
#endif
