/*
 *	func.h
 *
 *	Function Prototype Declaration
 */

#ifdef	NOERRNO
extern int errno;
#endif

#if	!defined (ENOTEMPTY) && defined (ENFSNOTEMPTY)
#define	ENOTEMPTY	ENFSNOTEMPTY
#endif

#if	MSDOS
#include "unixemu.h"
#define	strpathcmp	strcasecmp2
#define	strnpathcmp	strnicmp
#else
#include <sys/time.h>
# ifdef	USEDIRECT
# include <sys/dir.h>
#  ifdef	DIRSIZ
#  undef	DIRSIZ
#  endif
# else
# include <dirent.h>
# endif
# ifdef	USEUTIME
# include <utime.h>
# endif
#define	strpathcmp	strcmp
#define	strnpathcmp	strncmp
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#if	(GETTODARGS == 1)
#define	gettimeofday2(tv, tz)	gettimeofday(tv)
#else
#define	gettimeofday2(tv, tz)	gettimeofday(tv, tz)
#endif

#define	BUFUNIT		32
#define	b_size(n, type)	((((n) / BUFUNIT) + 1) * BUFUNIT * sizeof(type))
#define	b_realloc(ptr, n, type) \
			(((n) % BUFUNIT) ? ((type *)(ptr)) \
			: (type *)realloc2(ptr, b_size(n, type)))
#define	c_malloc(size)	(malloc2((size) = BUFUNIT))
#define	c_realloc(ptr, n, size) \
			(((n) + 1 < (size)) \
			? (ptr) : realloc2(ptr, (size) *= 2))

#define	getblock(c)	((((c) + blocksize - 1) / blocksize) * blocksize)

/* main.c */
extern VOID error __P_((char *));
extern VOID sigvecset __P_((void));
extern VOID sigvecreset __P_((void));
extern VOID title __P_((void));
extern int loadruncom __P_((char *, int));

/* dosemu.c or unixemu.c */
#if	MSDOS || !defined (_NODOSDRIVE)
extern int _dospath __P_((char *));
extern int dospath __P_((char *, char *));
#endif
#ifndef	_NODOSDRIVE
# if	MSDOS
extern int dospath2 __P_((char *));
extern int dospath3 __P_((char *));
# else
#define	dospath2(path)	dospath(path, NULL)
#define	dospath3(path)	dospath(path, NULL)
# endif
#endif
extern DIR *_Xopendir __P_((char *));
#ifdef	_NOROCKRIDGE
#define	Xopendir	_Xopendir
#else
extern DIR *Xopendir __P_((char *));
#endif
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern VOID Xrewinddir __P_((DIR *));
extern int _Xchdir __P_((char *));
#ifdef	_NOROCKRIDGE
#define	Xchdir		_Xchdir
#else
extern int Xchdir __P_((char *));
#endif
extern char *Xgetcwd __P_((char *, int));
extern int Xstat __P_((char *, struct stat *));
#define	_Xlstat(p, s)	(lstat(p, s) ? -1 : 0)
extern int Xlstat __P_((char *, struct stat *));
extern int Xaccess __P_((char *, int));
extern int Xsymlink __P_((char *, char *));
extern int Xreadlink __P_((char *, char *, int));
extern int Xchmod __P_((char *, int));
#ifdef	USEUTIME
extern int Xutime __P_((char *, struct utimbuf *));
#else
extern int Xutimes __P_((char *, struct timeval []));
#endif
extern int Xunlink __P_((char *));
extern int Xrename __P_((char *, char *));
#if	MSDOS && defined (_NOROCKRIDGE) && defined (_NOUSELFN)
#define	Xopen		open
#else
extern int Xopen __P_((char *, int, int));
#endif
#ifdef	_NODOSDRIVE
#define	Xclose		close
#define	Xread		read
#define	Xwrite		write
#define	Xlseek		lseek
#else
extern int Xclose __P_((int));
extern int Xread __P_((int, char *, int));
extern int Xwrite __P_((int, char *, int));
extern off_t Xlseek __P_((int, off_t, int));
#endif
#if	!defined (LSI_C) && defined (_NODOSDRIVE)
#define	Xdup		dup
#define	Xdup2		dup2
#else
extern int Xdup __P_((int));
extern int Xdup2 __P_((int, int));
#endif
#if	MSDOS && defined (_NOUSELFN)
# ifdef	DJGPP
#define	_Xmkdir(p, m)	(mkdir(p, m) ? -1 : 0)
# else
#define	_Xmkdir(p, m)	(mkdir(p) ? -1 : 0)
# endif
#define	_Xrmdir(p)	(rmdir(p) ? -1 : 0)
#else
# if	!MSDOS && defined (_NODOSDRIVE)
#define	_Xmkdir		mkdir
#define	_Xrmdir		rmdir
# else
extern int _Xmkdir __P_((char *, int));
extern int _Xrmdir __P_((char *));
# endif
#endif
#ifdef	_NOROCKRIDGE
# if	MSDOS && defined (_NOUSELFN)
#  ifdef	DJGPP
#define	Xmkdir(p, m)	(mkdir(p, m) ? -1 : 0)
#  else
#define	Xmkdir(p, m)	(mkdir(p) ? -1 : 0)
#  endif
#define	Xrmdir(p)	(rmdir(p) ? -1 : 0)
# else
#  if	!MSDOS && defined (_NODOSDRIVE)
#define	Xmkdir		mkdir
#define	Xrmdir		rmdir
#  else
#define	Xmkdir		_Xmkdir
#define	Xrmdir		_Xrmdir
#  endif
# endif
#else
extern int Xmkdir __P_((char *, int));
extern int Xrmdir __P_((char *));
#endif
#if	(MSDOS && defined (_NOUSELFN)) || (!MSDOS && defined (_NODOSDRIVE))
#define	_Xfopen		fopen
#else
extern FILE *_Xfopen __P_((char *, char *));
#endif
#ifdef	_NOROCKRIDGE
#define	Xfopen		_Xfopen
#else
extern FILE *Xfopen __P_((char *, char *));
#endif
#ifdef	_NODOSDRIVE
#define	Xfdopen		fdopen
#define	Xfclose		fclose
#define	Xfeof		feof
#define	Xfread		fread
#define	Xfwrite		fwrite
#define	Xfflush		fflush
#define	Xfgetc		fgetc
#define	Xfputc		fputc
#define	Xfgets		fgets
#define	Xfputs		fputs
#else
extern FILE *Xfdopen __P_((int, char *));
extern int Xfclose __P_((FILE *));
extern int Xfeof __P_((FILE *));
extern int Xfread __P_((char *, int, int, FILE *));
extern int Xfwrite __P_((char *, int, int, FILE *));
extern int Xfflush __P_((FILE *));
extern int Xfgetc __P_((FILE *));
extern int Xfputc __P_((int, FILE *));
extern char *Xfgets __P_((char *, int, FILE *));
extern int Xfputs __P_((char *, FILE *));
#endif
#if	MSDOS
extern FILE *Xpopen __P_((char *, char *));
extern int Xpclose __P_((FILE *));
#else
#define	Xpopen		popen
#define	Xpclose		pclose
#endif

/* libc.c */
extern int rename2 __P_((char *, char *));
extern int stat2 __P_((char *, struct stat *));
extern char *realpath2 __P_((char *, char *, int));
extern int _chdir2 __P_((char *));
extern int chdir2 __P_((char *));
extern char *chdir3 __P_((char *));
extern int mkdir2 __P_((char *, int));
extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strdup2 __P_((char *));
extern int toupper2 __P_((int));
extern char *strchr2 __P_((char *, int));
extern char *strrchr2 __P_((char *, int));
extern char *strcpy2 __P_((char *, char *));
extern char *strncpy2 __P_((char *, char *, int));
extern int strncpy3 __P_((char *, char *, int *, int));
extern int strcasecmp2 __P_((char *, char *));
extern char *strstr2 __P_((char *, char *));
extern int strlen2 __P_((char *));
extern int strlen3 __P_((char *));
extern int atoi2 __P_((char *));
extern int putenv2 __P_((char *));
extern int setenv2 __P_((char *, char *));
extern char *getenv2 __P_((char *));
#ifdef	DEBUG
extern VOID freeenv __P_((VOID_A));
#endif
extern int printenv __P_((int, char *[], int));
extern int system2 __P_((char *, int));
extern char *getwd2 __P_((VOID_A));
#if	!MSDOS
extern char *getpwuid2 __P_((uid_t));
extern char *getgrgid2 __P_((gid_t));
#endif
extern time_t timelocal2 __P_((struct tm *));
extern char *fgets2 __P_((FILE *));

/* file.c */
#if	MSDOS
extern int logical_access __P_((u_short));
#else
extern int logical_access __P_((u_short, uid_t, gid_t));
#endif
extern int getstatus __P_((namelist *));
extern int isdotdir __P_((char *));
extern int cmplist __P_((CONST VOID_P, CONST VOID_P));
#ifndef	_NOTREE
extern int cmptree __P_((CONST VOID_P, CONST VOID_P));
#endif
extern struct dirent *searchdir __P_((DIR *, reg_t *, char *));
extern int underhome __P_((char *));
extern int preparedir __P_((char *));
extern int touchfile __P_((char *, time_t, time_t));
extern int cpfile __P_((char *, char *, struct stat *));
extern int mvfile __P_((char *, char *, struct stat *));
extern int mktmpdir __P_((char *));
extern int rmtmpdir __P_((char *));
extern VOID removetmp __P_((char *, char *, char *));
extern int forcecleandir __P_((char *, char *));
#ifndef	_NODOSDRIVE
extern int tmpdosdupl __P_((char *, char **, namelist *, int, int));
extern int tmpdosrestore __P_((int, char *));
#endif
#ifndef	_NOWRITEFS
extern VOID arrangedir __P_((namelist *, int, int));
#endif

/* apply.c */
extern int rmvfile __P_((char *));
extern int rmvdir __P_((char *));
extern int findfile __P_((char *));
extern int finddir __P_((char *));
extern int inputattr __P_((namelist *, u_short));
extern int setattr __P_((char *));
extern int applyfile __P_((namelist *, int, int (*)__P_((char *)), char *));
extern int applydir __P_((char *, int (*)__P_((char *)),
		int (*)__P_((char *)), int, char *));
extern int copyfile __P_((namelist *, int, char *, int));
extern int movefile __P_((namelist *, int, char *, int));

/* parse.c */
extern char *skipspace __P_((char *));
extern char *skipnumeric __P_((char *, int));
extern char *strtkbrk __P_((char *, char *, int));
extern char *strtkchr __P_((char *, int, int));
extern char *geteostr __P_((char **));
extern char *gettoken __P_((char **, char *));
extern char *getenvval __P_((int *, char *[]));
extern char *evalcomstr __P_((char *, char *, int));
#if	!MSDOS
extern char *killmeta __P_((char *));
extern VOID adjustpath __P_((VOID_A));
#endif
extern char *includepath __P_((char *, char *, char *));
extern int getargs __P_((char *, char ***));
extern VOID freeargs __P_((char **));
extern char *catargs __P_((char *[], int));
#ifndef	_NOARCHIVE
extern char *getrange __P_((char *, u_char *, u_char *, u_char *));
#endif
extern int evalprompt __P_((char *, int));
extern int evalbool __P_((char *));
extern VOID evalenv __P_((VOID_A));

/* builtin.c */
extern char *getkeysym __P_((int));
extern int isinternal __P_((char *, int));
extern int execbuiltin __P_((char *, namelist *, int *, int));
#ifndef	_NOCOMPLETE
extern int completebuiltin __P_((char *, int, char ***));
#endif
#ifdef	DEBUG
extern VOID freedefine __P_((VOID_A));
#endif

/* shell.c */
extern char *evalcommand __P_((char *, char *, namelist *, int, macrostat *));
extern int execmacro __P_((char *, char *, namelist *, int *, int, int));
extern int execusercomm __P_((char *, char *, namelist *, int *, int, int));
extern int entryhist __P_((int, char *, int));
extern int loadhistory __P_((int, char *));
extern int savehistory __P_((int, char *));
extern int dohistory __P_((char *[], namelist *, int *));
#ifdef	DEBUG
extern VOID freehistory __P_((int));
#endif
#ifndef	_NOCOMPLETE
extern int completealias __P_((char *, int, char ***));
extern int completeuserfunc __P_((char *, int, char ***));
#endif

/* kanji.c */
extern int onkanji1 __P_((char *, int));
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
extern int getlang __P_((char *, int));
#endif
#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
extern int sjis2ujis __P_((char *, u_char *));
extern int ujis2sjis __P_((char *, u_char *));
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
extern int kanjiconv __P_((char *, char *, int, int));
#endif
extern int kanjiputs __P_((char *));
#if	MSDOS || defined (__STDC__)
extern int kanjiprintf(CONST char *, ...);
#else
extern int kanjiprintf __P_((CONST char *, ...));
#endif
extern int kanjiputs2 __P_((char *, int, int));

/* input.c */
extern int intrkey __P_((VOID_A));
extern int Xgetkey __P_((int));
extern int cmdlinelen __P_((int));
extern char *inputstr __P_((char *, int, int, char *, int));
#if	MSDOS || defined (__STDC__)
extern int yesno(CONST char *, ...);
#else
extern int yesno __P_((CONST char *, ...));
#endif
extern VOID warning __P_((int, char *));
extern int selectstr __P_((int *, int, int, char *[], int []));

/* info.c */
extern VOID help __P_((int));
extern int writablefs __P_((char *));
extern long getblocksize __P_((char *));
extern char *inscomma __P_((char *, off_t, int, int));
extern VOID getinfofs __P_((char *, long *, long *));
extern int infofs __P_((char *));

/* rockridge.c */
#ifndef	_NOROCKRIDGE
extern int transfilelist __P_((namelist *, int));
extern char *transfile __P_((char *, char *));
extern char *detransfile __P_((char *, char *, int));
#endif

/* archive.c */
#ifndef	_NOARCHIVE
extern VOID rewritearc __P_((int));
extern int launcher __P_((namelist *, int));
extern int pack __P_((char *, namelist *, int));
extern int unpack __P_((char *, char *, namelist *, int, char *, int));
extern char *tmpunpack __P_((namelist *, int, int));
extern int backup __P_((char *, namelist *, int));
extern int searcharc __P_((char *, namelist *, int, int));
#endif

/* tree.c */
#ifndef	_NOTREE
extern VOID rewritetree __P_((VOID_A));
extern char *tree __P_((int, int *));
#endif

/* command.c */

/* browse.c */
extern VOID helpbar __P_((VOID_A));
extern VOID statusbar __P_((int));
extern VOID sizebar __P_((VOID_A));
extern char *putmode __P_((char *, u_short));
#ifdef	HAVEFLAGS
extern char *putflags __P_((char *, u_long));
#endif
extern VOID infobar __P_((namelist *, int));
extern VOID waitmes __P_((VOID_A));
extern int calcwidth __P_((VOID_A));
extern VOID putname __P_((namelist *, int, int));
extern int listupfile __P_((namelist *, int, char *));
extern VOID movepos __P_((namelist *, int, int, u_char));
extern VOID rewritefile __P_((int));
extern int searchmove __P_((namelist *, int, int, char *));
extern VOID addlist __P_((VOID_A));
extern VOID main_fd __P_((char *));
