/*
 *	depend.h
 *
 *	definitions for dependency
 */

#if	MSDOS && defined (_NOUSELFN) && !defined (_NODOSDRIVE)
#define	_NODOSDRIVE
#endif

#if	defined (NOMULTIKANJI) && !defined (_NOKANJICONV)
#define	_NOKANJICONV
#endif
#if	(defined (_NOKANJICONV) || (defined (FD) && FD < 2)) \
&& !defined (_NOKANJIFCONV)
#define	_NOKANJIFCONV
#endif
#if	(defined (_NOKANJICONV) || (defined (FD) && FD < 2)) \
&& !defined (_NOIME)
#define	_NOIME
#endif
#if	defined (_NOENGMES) && defined (_NOJPNMES)
#undef	_NOENGMES
#endif
#if	(defined (_NOENGMES) || defined (_NOJPNMES) \
|| (defined (FD) && FD < 3)) \
&& !defined (_NOCATALOG)
#define	_NOCATALOG
#endif

#if	(MSDOS || (defined (FD) && FD < 3)) && !defined (_NOSOCKET)
#define	_NOSOCKET
#endif
#if	defined (_NOSOCKET) && !defined (_NOSOCKREDIR)
#define	_NOSOCKREDIR
#endif
#if	defined (_NOSOCKET) && !defined (_NOFTP)
#define	_NOFTP
#endif
#if	defined (_NOSOCKET) && !defined (_NOHTTP)
#define	_NOHTTP
#endif

#ifdef	WITHNETWORK
#undef	OLDPARSE
#endif
#if	defined (OLDPARSE) && !defined (_NOCUSTOMIZE)
#define	_NOCUSTOMIZE
#endif
#if	defined (OLDPARSE) && !defined (_NOBROWSE)
#define	_NOBROWSE
#endif
#if	defined (OLDPARSE) && !defined (_NOFTP)
#define	_NOFTP
#endif
#if	defined (OLDPARSE) && !defined (_NOHTTP)
#define	_NOHTTP
#endif

#if	MSDOS && !defined (_NOKEYMAP)
#define	_NOKEYMAP
#endif
#if	MSDOS && !defined (_USEDOSCOPY)
#define	_USEDOSCOPY
#endif
#if	(defined (_NOORIGSHELL) || (defined (FD) && FD < 2)) \
&& !defined (_NOEXTRAMACRO)
#define	_NOEXTRAMACRO
#endif
#ifdef	_NOSPLITWIN
#undef	MAXWINDOWS
#define	MAXWINDOWS	1
#else	/* !_NOSPLITWIN */
# if	(MAXWINDOWS <= 1)
# define	_NOSPLITWIN
# endif
#endif	/* !_NOSPLITWIN */
#if	(defined (_NOSPLITWIN) || (defined (FD) && FD < 2)) \
&& !defined (_NOEXTRAWIN)
#define	_NOEXTRAWIN
#endif
#if	(MSDOS || defined (NOSELECT) || (defined(FD) && FD < 2)) \
&& !defined (_NOPTY)
#define	_NOPTY
#endif

#undef	DEP_DOSDRIVE
#undef	DEP_DOSEMU
#undef	DEP_DOSPATH
#undef	DEP_DOSLFN
#undef	DEP_ROCKRIDGE
#undef	DEP_KCONV
#undef	DEP_FILECONV
#undef	DEP_KANJIPATH
#undef	DEP_UNICODE
#undef	DEP_ORIGSTREAM
#undef	DEP_ORIGSHELL
#undef	DEP_PTY
#undef	DEP_IME
#undef	DEP_LOGGING
#undef	DEP_DYNAMICLIST
#undef	DEP_SOCKET
#undef	DEP_SOCKREDIR
#undef	DEP_FTPPATH
#undef	DEP_HTTPPATH
#undef	DEP_URLPATH
#undef	DEP_STREAMTIMEOUT
#undef	DEP_STREAMLOCK
#undef	DEP_STREAMSOCKET
#undef	DEP_STREAMLOG
#undef	DEP_LSPARSE
#undef	DEP_PSEUDOPATH
#undef	DEP_BIASPATH
#undef	DEP_DIRENT
#undef	DEP_PATHTOP

#if	defined (FD) && !defined (_NODOSDRIVE)
#define	DEP_DOSDRIVE
#endif
#if	!MSDOS && defined (DEP_DOSDRIVE)
#define	DEP_DOSEMU
#endif
#if	MSDOS || defined (DEP_DOSDRIVE)
#define	DEP_DOSPATH
#endif
#if	MSDOS && defined (FD) && !defined (_NOUSELFN)
#define	DEP_DOSLFN
#endif
#if	defined (FD) && !defined (_NOROCKRIDGE)
#define	DEP_ROCKRIDGE
#endif

#if	defined (FD) && !defined (_NOKANJICONV)
#define	DEP_KCONV
#endif
#if	defined (FD) && !defined (_NOKANJIFCONV)
#define	DEP_FILECONV
#endif
#if	!defined (NOMULTIKANJI) && defined (DEP_FILECONV)
#define	DEP_KANJIPATH
#endif
#if	(defined (DEP_KCONV) && !defined (_NOUNICODE)) \
|| defined (DEP_DOSDRIVE)
#define	DEP_UNICODE
#endif
#ifndef	MINIMUMSHELL
#define	DEP_ORIGSTREAM
#endif
#if	defined (FDSH) || (defined (FD) && FD >= 2 && !defined (_NOORIGSHELL))
#define	DEP_ORIGSHELL
#endif
#if	defined (FD) && FD >= 2 && !defined (_NOPTY)
#define	DEP_PTY
#endif
#if	defined (FD) && FD >= 2 && !defined (_NOIME)
#define	DEP_IME
#endif
#if	defined (FD) && FD >= 2 && !defined (_NOLOGGING)
#define	DEP_LOGGING
#endif
#if	defined (FD) && FD >= 3 && !defined (_NODYNAMICLIST)
#define	DEP_DYNAMICLIST
#endif

#if	!MSDOS \
&& (defined (WITHNETWORK) || (defined (FD) && !defined (_NOSOCKET)))
#define	DEP_SOCKET
#endif
#if	!MSDOS \
&& (defined (WITHNETWORK) || (defined (FD) && !defined (_NOSOCKREDIR)))
#define	DEP_SOCKREDIR
#endif
#if	!MSDOS \
&& (defined (WITHNETWORK) || (defined (FD) && !defined (_NOFTP)))
#define	DEP_FTPPATH
#endif
#if	!MSDOS \
&& (defined (WITHNETWORK) || (defined (FD) && !defined (_NOHTTP)))
#define	DEP_HTTPPATH
#endif
#if	defined (DEP_FTPPATH) || defined (DEP_HTTPPATH)
#define	DEP_URLPATH
#endif
#if	defined (MH) || (defined (DEP_URLPATH) && !defined (NOSELECT))
#define	DEP_STREAMTIMEOUT
#endif
#if	defined (MH) && !defined (NOFLOCK)
#define	DEP_STREAMLOCK
#endif
#if	defined (MH) || defined (DEP_SOCKET)
#define	DEP_STREAMSOCKET
#endif
#ifdef	MH
#define	DEP_STREAMLOG
#endif

#if	!defined (FD) || !defined (_NOARCHIVE) || defined (DEP_FTPPATH)
#define	DEP_LSPARSE
#endif
#if	defined (DEP_DOSDRIVE) || defined (DEP_URLPATH)
#define	DEP_PSEUDOPATH
#endif
#if	defined (DEP_PSEUDOPATH) || defined (DEP_DOSLFN) \
|| defined (DEP_KANJIPATH) || defined (DEP_ROCKRIDGE)
#define	DEP_BIASPATH
#endif
#if	MSDOS || defined (DEP_BIASPATH)
#define	DEP_DIRENT
#endif
#if	defined (DEP_DOSEMU) || defined (DOUBLESLASH) || defined (DEP_URLPATH)
#define	DEP_PATHTOP
#endif
