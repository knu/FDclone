/*
 *	realpath.c
 *
 *	alternative pathname parser
 */

#ifdef	FD
#include "fd.h"
#else
#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "string.h"
#endif

#include "sysemu.h"
#include "pathname.h"
#include "unixemu.h"
#include "realpath.h"

#if	MSDOS && !defined (FD)
#include "termio.h"
#endif
#ifdef	DEP_URLPATH
#include "url.h"
#endif

#if	defined (FD) && (defined (DEP_DOSDRIVE) || defined (DEP_FTPPATH))
extern int reallstat __P_((CONST char *, struct stat *));
#else
#define	reallstat		Xlstat
#endif

#if	MSDOS
# ifdef	FD
extern int setcurdrv __P_((int, int));
extern VOID error __P_((CONST char *));
extern VOID lostcwd __P_((char *));
# else
static VOID NEAR error __P_((CONST char *));
# endif
#endif	/* MSDOS */
#ifdef	DEP_DOSEMU
extern char *dosgetcwd __P_((char *, int));
#endif

#ifdef	FD
extern char fullpath[];
#endif
#ifdef	DEP_DOSEMU
extern int lastdrive;
#endif

#ifndef	NOSYMLINK
static int NEAR evallink __P_((char *, int, int));
#endif
#if	defined (DEP_DOSEMU) || defined (DOUBLESLASH) || defined (DEP_URLPATH)
static int NEAR getpathtop __P_((CONST char *, int *));
#endif
static int NEAR _Xrealpath __P_((CONST char *, int, char *, int, int, int));

int norealpath = 0;


#if	MSDOS && !defined (FD)
static VOID NEAR error(s)
CONST char *s;
{
	Xfprintf(Xstderr, "%s: Error(%d).\n", errno);
	exit(2);
}
#endif	/* MSDOS && !FD */

#ifndef	NOSYMLINK
static int NEAR evallink(path, tlen, rlen)
char *path;
int tlen, rlen;
{
	struct stat st;
	char buf[MAXPATHLEN];
	int n, duperrno;

	duperrno = errno;
	if (reallstat(path, &st) < 0) {
		errno = duperrno;
		return(-1);
	}
	if ((st.st_mode & S_IFMT) != S_IFLNK) {
		errno = duperrno;
		return(0);
	}
	if ((n = Xreadlink(path, buf, sizeof(buf) - 1)) < 0) {
		errno = duperrno;
		return(-1);
	}

	buf[n] = '\0';
	if (*buf != _SC_ && rlen) path[rlen] = '\0';
	else {
		copyrootpath(&(path[tlen]));
		rlen = tlen + 1;
	}
	rlen = _Xrealpath(buf, n, path, tlen, rlen, RLP_READLINK);
	errno = duperrno;

	return(rlen);
}
#endif	/* !NOSYMLINK */

#if	defined (DEP_DOSEMU) || defined (DOUBLESLASH) || defined (DEP_URLPATH)
static int NEAR getpathtop(path, drvp)
CONST char *path;
int *drvp;
{
	int n;

	if (drvp) *drvp = 0;

# ifdef	DEP_DOSPATH
	if ((n = _dospath(path))) {
		if (drvp) *drvp = n;
		n = 2;
	}
	else
# endif
# ifdef	DOUBLESLASH
	if ((n = isdslash(path))) /*EMPTY*/;
	else
# endif
# ifdef	DEP_URLPATH
	if ((n = _urlpath(path, NULL, NULL))) /*EMPTY*/;
	else
# endif
	n = 0;

	return(n);
}
#endif	/* DEP_DOSEMU || DOUBLESLASH || DEP_URLPATH */

/*ARGSUSED*/
static int NEAR _Xrealpath(path, plen, resolved, tlen, rlen, flags)
CONST char *path;
int plen;
char *resolved;
int tlen, rlen, flags;
{
#ifndef	NOSYMLINK
	int n;
#endif
	char *cp, *top;
	int len;

	if (plen <= 0) return(rlen);
	else if (path[0] != '.') /*EMPTY*/;
	else if (plen == 1) return(rlen);
	else if (path[1] != '.') /*EMPTY*/;
	else if (plen == 2) {
		top = &(resolved[tlen]);
		cp = strrdelim(top, 0);
		if (!cp || cp <= top) {
			copyrootpath(top);
			return(++tlen);
		}
		*cp = '\0';
		return(cp - top);
	}

	if ((cp = Xmemchr(path, _SC_, plen))) {
		len = cp++ - path;
		rlen = _Xrealpath(path, len, resolved, tlen, rlen, flags);
		plen -= ++len;
		return(_Xrealpath(cp, plen, resolved, tlen, rlen, flags));
	}

	len = Xsnprintf(&(resolved[rlen]), MAXPATHLEN - rlen,
		"%s%-.*s", (rlen > tlen + 1) ? _SS_ : nullstr, plen, path);
#ifndef	NOSYMLINK
	if ((flags & RLP_READLINK)
	&& (n = evallink(resolved, tlen, rlen)) > 0) {
		rlen = n;
		len = 0;
	}
#endif
	rlen += len;

	return(rlen);
}

char *Xrealpath(path, resolved, flags)
CONST char *path;
char *resolved;
int flags;
{
#ifdef	DEP_DOSEMU
	int duplastdrive;
#endif
#if	defined (DEP_DOSPATH) || defined (DOUBLESLASH) || defined (DEP_URLPATH)
	int drv;
#endif
#if	MSDOS
	int drive;
#endif
	char *cp, tmp[MAXPATHLEN];
	int tlen, rlen;

	Xstrcpy(tmp, path);
	path = tmp;

	cp = NULL;
	tlen = rlen = 0;
#ifdef	DEP_DOSPATH
	drv = 0;
#endif
#if	defined (DEP_DOSEMU) || defined (DOUBLESLASH) || defined (DEP_URLPATH)
	if (!(flags & RLP_PSEUDOPATH) && (tlen = getpathtop(path, &drv))) {
# ifdef	DEP_DOSEMU
		if (drv) {
			if (path[tlen] != _SC_) {
				duplastdrive = lastdrive;
				lastdrive = drv;
				cp = dosgetcwd(resolved, MAXPATHLEN - 1);
				lastdrive = duplastdrive;
			}
		}
		else
# endif
		rlen = Xsnprintf(resolved, MAXPATHLEN,
			"%-.*s%c", tlen, path, _SC_);
		path += tlen;
		flags &= ~RLP_READLINK;
	}
	else
#endif	/* DEP_DOSEMU || DOUBLESLASH || DEP_URLPATH */
	{
#if	MSDOS
		tlen = 2;
		drive = dospath(nullstr, NULL);
		if ((drv = _dospath(path))) path += 2;
		else drv = drive;
#endif

		if (*path == _SC_) /*EMPTY*/;
#if	MSDOS
		else if (Xtoupper(drv) != Xtoupper(drive)) {
			if (setcurdrv(drv, 0) >= 0) {
				cp = Xgetwd(resolved);
				if (setcurdrv(drive, 0) < 0)
					error("setcurdrv()");
# ifdef	FD
				if (!cp) lostcwd(resolved);
# else
				if (!cp) /*EMPTY*/;
				else
# endif
				cp = resolved;
			}
		}
#endif	/* MSDOS */
#ifdef	FD
		else if (!(flags & RLP_READLINK)
		&& resolved != fullpath && *fullpath) {
			cp = Xstrcpy(resolved, fullpath);
# if	defined (DEP_DOSEMU) || defined (DOUBLESLASH) || defined (DEP_URLPATH)
			tlen = getpathtop(resolved, NULL);
# endif
		}
#endif	/* FD */
		else cp = Xgetwd(resolved);
	}

	if (rlen > 0) /*EMPTY*/;
	else if (cp) rlen = strlen(cp);
#ifdef	DEP_DOSPATH
	else if (drv) {
		cp = gendospath(resolved, drv, _SC_);
		rlen = cp - resolved;
	}
#endif
	else {
		copyrootpath(resolved);
		rlen = 1;
	}

	norealpath++;
	VOID_C _Xrealpath(path, strlen(path),
		resolved, tlen, strlen(resolved), flags);
	norealpath--;

	return(resolved);
}
