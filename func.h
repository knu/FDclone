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
#else
#include <sys/time.h>
#include <sys/param.h>
#include <sys/file.h>
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
#endif

#ifdef	USETIMEH
#include <time.h>
#endif

#ifndef	_NODOSDRIVE
#include "dosdisk.h"
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
extern int sigvecset __P_((int));
extern VOID title __P_((VOID_A));
#ifndef	_NOCUSTOMIZE
VOID saveorigenviron __P_((VOID_A));
#endif
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
#if	!MSDOS && !defined (_NOKANJIFCONV)
extern int getkcode __P_((char *));
#endif
extern DIR *_Xopendir __P_((char *, int));
#define	Xopendir(p)	_Xopendir(p, 0)
extern int Xclosedir __P_((DIR *));
extern struct dirent *_Xreaddir __P_((DIR *, int));
#define	Xreaddir(d)	_Xreaddir(d, 0)
extern VOID Xrewinddir __P_((DIR *));
#if	MSDOS
extern int rawchdir __P_((char *));
#else
#define	rawchdir	chdir
#endif
extern int _Xchdir __P_((char *, int));
#define	Xchdir(p)	_Xchdir(p, 0)
extern char *_Xgetwd __P_((char *, int));
#define	Xgetwd(p)	_Xgetwd(p, 0)
extern int Xstat __P_((char *, struct stat *));
extern int _Xlstat __P_((char *, struct stat *, int, int));
#define	Xlstat(p, s)	_Xlstat(p, s, 0, 0)
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
extern int _Xrename __P_((char *, char *, int));
#define	Xrename(f, t)	_Xrename(f, t, 0)
extern int _Xopen __P_((char *, int, int, int));
#define	Xopen(p, f, m)	_Xopen(p, f, m, 0)
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
extern int _Xmkdir __P_((char *, int, int, int));
#define	Xmkdir(p, m)	_Xmkdir(p, m, 0, 0)
extern int _Xrmdir __P_((char *, int, int));
#define	Xrmdir(p)	_Xrmdir(p, 0, 0)
extern FILE *_Xfopen __P_((char *, char *, int));
#define	Xfopen(p, t)	_Xfopen(p, t, 0)
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
#ifdef	_NOORIGSHELL
# if	MSDOS
extern FILE *Xpopen __P_((char *, char *));
extern int Xpclose __P_((FILE *));
# else
# define	Xpopen		popen
# define	Xpclose		pclose
# endif
#endif	/* _NOORIGSHELL */

/* libc.c */
extern int stat2 __P_((char *, struct stat *));
extern char *realpath2 __P_((char *, char *, int));
extern int _chdir2 __P_((char *));
extern int chdir2 __P_((char *));
extern int chdir3 __P_((char *));
extern int mkdir2 __P_((char *, int));
extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strdup2 __P_((char *));
extern char *strchr2 __P_((char *, int));
extern char *strrchr2 __P_((char *, int));
extern char *strcpy2 __P_((char *, char *));
extern char *strncpy2 __P_((char *, char *, int));
extern int strncpy3 __P_((char *, char *, int *, int));
extern char *strstr2 __P_((char *, char *));
extern int strlen2 __P_((char *));
extern int strlen3 __P_((char *));
extern int atoi2 __P_((char *));
extern int putenv2 __P_((char *));
extern int setenv2 __P_((char *, char *));
extern char *getenv2 __P_((char *));
extern int system2 __P_((char *, int));
extern char *getwd2 __P_((VOID_A));
extern time_t time2 __P_((VOID_A));
extern time_t timelocal2 __P_((struct tm *));
extern char *fgets2 __P_((FILE *, int));

/* file.c */
#if	MSDOS || defined (_NODOSDRIVE)
#define	nodospath(p, f)	(f)
#else
char *nodospath __P_((char *, char *));
#endif
#define	fnodospath(p, i)	nodospath(p, filelist[i].name)
#if	MSDOS
extern int logical_access __P_((u_short));
#else
extern int logical_access __P_((u_short, uid_t, gid_t));
#endif
extern int getstatus __P_((namelist *));
extern int cmplist __P_((CONST VOID_P, CONST VOID_P));
#ifndef	_NOTREE
extern int cmptree __P_((CONST VOID_P, CONST VOID_P));
#endif
extern struct dirent *searchdir __P_((DIR *, reg_t *, char *));
extern int underhome __P_((char *));
extern int preparedir __P_((char *));
extern int touchfile __P_((char *, struct stat *));
extern VOID lostcwd __P_((char *));
extern int safewrite __P_((int, char *, int));
extern int safecpfile __P_((char *, char *, struct stat *, struct stat *));
extern int safemvfile __P_((char *, char *, struct stat *, struct stat *));
extern char *genrandname __P_((char *, int));
extern int mktmpdir __P_((char *));
extern int rmtmpdir __P_((char *));
extern int mktmpfile __P_((char *, char *));
extern int rmtmpfile __P_((char *));
extern VOID removetmp __P_((char *, char *, char *));
extern int forcecleandir __P_((char *, char *));
#ifndef	_NODOSDRIVE
extern int tmpdosdupl __P_((char *, char **, int));
extern int tmpdosrestore __P_((int, char *));
#endif
int isexist __P_((char *));
#ifndef	_NOWRITEFS
extern VOID arrangedir __P_((int));
#endif

/* apply.c */
extern int rmvfile __P_((char *));
extern int rmvdir __P_((char *));
extern int findfile __P_((char *));
extern int finddir __P_((char *));
extern int inputattr __P_((namelist *, int));
extern int setattr __P_((char *));
extern int applyfile __P_((int (*)__P_((char *)), char *));
extern int applydir __P_((char *, int (*)__P_((char *)),
		int (*)__P_((char *)), int (*)__P_((char *)), int, char *));
extern int copyfile __P_((char *, int));
extern int movefile __P_((char *, int));

/* parse.c */
extern char *skipspace __P_((char *));
extern char *evalnumeric __P_((char *, long *, int));
extern char *ascnumeric __P_((char *, long, int, int));
#ifdef	_NOORIGSHELL
extern char *strtkchr __P_((char *, int, int));
extern int getargs __P_((char *, char ***));
extern char *gettoken __P_((char *));
extern char *getenvval __P_((int *, char *[]));
extern char *evalcomstr __P_((char *, char *));
#endif
extern char *evalpaths __P_((char *, int));
#if	MSDOS && defined (_NOORIGSHELL)
#define	killmeta(s)	strdup2(s)
#else
extern char *killmeta __P_((char *));
#endif
#if	!MSDOS && defined (_NOORIGSHELL)
extern VOID adjustpath __P_((VOID_A));
#endif
extern char *includepath __P_((char *, char *, char **));
#if	(FD < 2) && !defined (_NOARCHIVE)
extern char *getrange __P_((char *, u_char *, u_char *, u_char *));
#endif
extern int evalprompt __P_((char *, char *, int));
#ifndef	_NOARCHIVE
extern char *getext __P_((char *, int *));
extern int extcmp __P_((char *, int, char *, int, int));
#endif
extern int getkeycode __P_((char *, int));
extern char *getkeysym __P_((int, int));
extern char *decodestr __P_((char *, int *, int));
#if	!MSDOS && !defined (_NOKEYMAP)
extern char *encodestr __P_((char *, int));
#endif

/* builtin.c */
extern VOID printlaunchcomm __P_((int, int, FILE *));
extern VOID printarchcomm __P_((int, FILE *));
extern int freemacro __P_((int));
extern VOID printmacro __P_((int, FILE *));
#if	!MSDOS && !defined (_NODOSDRIVE)
extern int searchdrv __P_((devinfo *, int, char *, int, int, int, int));
extern int deletedrv __P_((int));
extern int insertdrv __P_((int, int, char *, int, int, int));
extern VOID printsetdrv __P_((int, int, FILE *));
#endif
#if	!MSDOS && !defined (_NOKEYMAP)
extern VOID printkeymap __P_((int, char *, int, FILE *));
#endif
#ifdef	_NOORIGSHELL
extern VOID printalias __P_((int, FILE *));
extern VOID printfunction __P_((int, int, FILE *));
#endif
extern int checkbuiltin __P_((char *));
extern int checkinternal __P_((char *));
extern int execbuiltin __P_((int, int, char *[]));
extern int execinternal __P_((int, int, char *[]));
#ifdef	_NOORIGSHELL
extern int execpseudoshell __P_((char *, int, int));
#endif
#ifndef	_NOCOMPLETE
extern int completebuiltin __P_((char *, int, int, char ***));
extern int completeinternal __P_((char *, int, int, char ***));
#endif
#ifdef	DEBUG
extern VOID freedefine __P_((VOID_A));
#endif

#define	BL_SET		"set"
#define	BL_PENV		"printenv"
#define	BL_LAUNCH	"launch"
#define	BL_ARCH		"arch"
#define	BL_PLAUNCH	"printlaunch"
#define	BL_PARCH	"printarch"
#define	BL_BIND		"bind"
#define	BL_PBIND	"printbind"
#define	BL_SDRIVE	"setdrv"
#define	BL_UDRIVE	"unsetdrv"
#define	BL_PDRIVE	"printdrv"
#define	BL_KEYMAP	"keymap"
#define	BL_GETKEY	"getkey"
#define	BL_HISTORY	"history"
#define	BL_FC		"fc"
#define	BL_CHECKID	"checkid"
#define	BL_KCONV	"kconv"
#define	BL_EVALMACRO	"evalmacro"
#define	BL_ALIAS	"alias"
#define	BL_UALIAS	"unalias"
#define	BL_FUNCTION	"function"
#define	BL_EXPORT	"export"
#define	BL_CHDIR	"chdir"
#define	BL_CD		"cd"
#define	BL_SOURCE	"source"
#define	BL_DOT		"."

/* shell.c */
extern char *evalcommand __P_((char *, char *, macrostat *, int));
extern char *inputshellstr __P_((char *, int, char *));
extern char *inputshellloop __P_((int, char *));
extern int execmacro __P_((char *, char *, int, int, int));
#ifdef	_NOORIGSHELL
extern int execusercomm __P_((char *, char *, int, int, int));
#else
#define	execusercomm	execmacro
#endif
extern int entryhist __P_((int, char *, int));
extern char *removehist __P_((int));
extern int loadhistory __P_((int, char *));
extern int savehistory __P_((int, char *));
extern int parsehist __P_((char *, int *));
extern char *evalhistory __P_((char *));
#ifdef	DEBUG
extern VOID freehistory __P_((int));
#endif
#if	!defined (_NOCOMPLETE) && defined (_NOORIGSHELL)
extern int completealias __P_((char *, int, int, char ***));
extern int completeuserfunc __P_((char *, int, int, char ***));
#endif

/* kanji.c */
extern int onkanji1 __P_((char *, int));
#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
extern int getlang __P_((char *, int));
#endif
#if	!MSDOS && (!defined (_NOKANJICONV) \
|| (!defined (_NODOSDRIVE) && defined (CODEEUC)))
extern int sjis2ujis __P_((char *, u_char *, int));
extern int ujis2sjis __P_((char *, u_char *, int));
#endif
#if	(!MSDOS && defined (FD) && (FD >= 2) && !defined (_NOKANJICONV)) \
|| !defined (_NODOSDRIVE)
extern VOID readunitable __P_((VOID_A));
extern VOID discardunitable __P_((VOID_A));
extern u_short unifysjis __P_((u_short, int));
extern u_short cnvunicode __P_((u_short, int));
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
extern int kanjiconv __P_((char *, char *, int, int, int, int));
# ifndef	_NOKANJIFCONV
extern char *kanjiconv2 __P_((char *, char *, int, int, int));
# endif
#endif
extern int kanjiputs __P_((char *));
extern int kanjifputs __P_((char *, FILE *));
extern int kanjiprintf __P_((CONST char *, ...));
extern int kanjiputs2 __P_((char *, int, int));

/* input.c */
extern int intrkey __P_((VOID_A));
extern int Xgetkey __P_((int, int));
extern int cmdlinelen __P_((int));
extern char *inputstr __P_((char *, int, int, char *, int));
extern int yesno __P_((CONST char *, ...));
extern VOID warning __P_((int, char *));
extern int selectstr __P_((int *, int, int, char *[], int []));

/* info.c */
extern VOID help __P_((int));
extern int writablefs __P_((char *));
extern long getblocksize __P_((char *));
extern long calcKB __P_((long, long));
extern int getinfofs __P_((char *, long *, long *, long *));
extern int infofs __P_((char *));

/* rockridge.c */
#ifndef	_NOROCKRIDGE
extern int transfilelist __P_((VOID_A));
extern char *transfile __P_((char *, char *));
extern char *detransfile __P_((char *, char *, int));
#else
#define	transfile(p, b)		(p)
#define	detransfile(p, b, l)	(p)
#endif

/* archive.c */
#ifndef	_NOARCHIVE
extern VOID poparchdupl __P_((VOID_A));
extern VOID archbar __P_((char *, char *));
extern VOID copyarcf __P_((reg_t *, char *));
extern char *archchdir __P_((char *));
# ifndef	_NOCOMPLETE
extern int completearch __P_((char *, int, int, char ***));
# endif
extern int launcher __P_((VOID_A));
extern int pack __P_((char *));
extern int unpack __P_((char *, char *, char *, int, int));
extern char *tmpunpack __P_((int));
extern int backup __P_((char *));
extern int searcharc __P_((char *, namelist *, int, int));
#endif

/* tree.c */
#ifndef	_NOTREE
extern VOID rewritetree __P_((VOID_A));
extern char *tree __P_((int, int *));
#endif

/* custom.c */
extern VOID evalenv __P_((VOID_A));
#ifdef	DEBUG
extern VOID freeenvpath __P_((VOID_A));
#endif
#ifndef	_NOCUSTOMIZE
VOID freestrarray __P_((char **, int));
char **copystrarray __P_((char **, char **, int *, int));
extern bindtable *copybind __P_((bindtable *, bindtable *));
# if	!MSDOS && !defined (_NOKEYMAP)
extern VOID freekeymap __P_((keymaptable *));
extern keymaptable *copykeymap __P_((keymaptable *));
# endif
# ifndef	_NOARCHIVE
extern VOID freelaunch __P_((launchtable *, int));
extern launchtable *copylaunch __P_((launchtable *, launchtable *,
	int *, int));
extern VOID freearch __P_((archivetable *, int));
extern archivetable *copyarch __P_((archivetable *, archivetable *,
	int *, int));
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
extern VOID freedrive __P_((devinfo *));
extern devinfo *copydrive __P_((devinfo *, devinfo *));
# endif
extern VOID rewritecust __P_((int));
extern int customize __P_((VOID_A));
#endif	/* !_NOCUSTOMIZE */

/* command.c */

/* browse.c */
extern VOID helpbar __P_((VOID_A));
extern char *putmode __P_((char *, u_short));
#ifdef	HAVEFLAGS
extern char *putflags __P_((char *, u_long));
#endif
extern VOID waitmes __P_((VOID_A));
extern int calcwidth __P_((VOID_A));
extern int putname __P_((namelist *, int, int));
extern int listupfile __P_((namelist *, int, char *));
#ifndef	_NOSPLITWIN
extern int shutwin __P_((int));
#endif
extern VOID movepos __P_((int, u_char));
extern VOID rewritefile __P_((int));
extern VOID addlist __P_((VOID_A));
extern VOID main_fd __P_((char *));
