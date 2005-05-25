/*
 *	func.h
 *
 *	function prototype declarations
 */

#ifdef	NOERRNO
extern int errno;
#endif

#if	!defined (ENOTEMPTY) && defined (ENFSNOTEMPTY)
#define	ENOTEMPTY	ENFSNOTEMPTY
#endif

#if	MSDOS
#include "unixemu.h"
#else	/* !MSDOS */
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
#endif	/* !MSDOS */

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

#define	getblock(c)	((((c) + blocksize - 1) / blocksize) * blocksize)

#define	isleftshift()	(n_column < WCOLUMNSTD)
#define	isrightomit()	(n_column < WCOLUMNSTD - 1)
#define	ispureshift()	(n_column == WCOLUMNSTD - 1)
#define	isshortwid()	(n_column < WCOLUMNSTD - 1)
#define	iswellomit()	(n_column < WCOLUMNSTD - 2)
#define	ishardomit()	(n_column < WCOLUMNOMIT)
#define	isbestomit()	(n_column < WCOLUMNHARD)

#ifdef	_NOORIGSHELL
#define	getconstvar(s)		(char *)getenv(s)
#define	getshellvar(s, l)	(char *)getenv(s)
#else
#define	getconstvar(s)		(getshellvar(s, sizeof(s) - 1))
#endif

#ifdef	USESTRERROR
#define	strerror2		strerror
#else
# ifndef	DECLERRLIST
extern char *sys_errlist[];
# endif
#define	strerror2(n)		(char *)sys_errlist[n]
#endif

/* main.c */
extern VOID error __P_((char *));
extern VOID setlinecol __P_((VOID_A));
extern VOID checkscreen __P_((int, int));
#ifdef	SIGWINCH
extern VOID pollscreen __P_((int));
#endif
extern int sigvecset __P_((int));
extern VOID title __P_((VOID_A));
#ifndef	_NOCUSTOMIZE
extern VOID saveorigenviron __P_((VOID_A));
#endif
extern int loadruncom __P_((char *, int));

/* system.c */
#ifdef	_NOORIGSHELL
#define	dosystem		system
#define	dopopen(c)		Xpopen(c, "r")
#define	dopclose		Xpclose
#else
extern int dosystem __P_((char *));
extern FILE *dopopen __P_((char *));
extern int dopclose __P_((FILE *));
#endif

/* dosemu.c or unixemu.c */
#ifdef	_USEDOSPATH
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
extern DIR *Xopendir __P_((char *));
extern int Xclosedir __P_((DIR *));
extern struct dirent *Xreaddir __P_((DIR *));
extern VOID Xrewinddir __P_((DIR *));
#if	MSDOS
extern int rawchdir __P_((char *));
#else
#define	rawchdir	chdir
#endif
extern int Xchdir __P_((char *));
extern char *Xgetwd __P_((char *));
extern int Xstat __P_((char *, struct stat *));
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
extern int Xopen __P_((char *, int, int));
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
#ifdef	_NODOSDRIVE
#define	Xdup		safe_dup
#define	Xdup2		safe_dup2
#else
extern int Xdup __P_((int));
extern int Xdup2 __P_((int, int));
#endif
extern int Xmkdir __P_((char *, int));
extern int Xrmdir __P_((char *));
extern FILE *Xfopen __P_((char *, char *));
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

/* pty.c */
#ifndef	_NOPTY
extern int Xopenpty __P_((int *, char *, ALLOC_T));
extern int Xlogin_tty __P_((char *, char *, char *));
extern p_id_t Xforkpty __P_((int *, char *, char *));
#endif

/* termemu.c */
#ifdef	_NOPTY
#define	ptymacro		execmacro
#define	ptyusercomm		execusercomm
#define	ptysystem		dosystem
#else	/* !_NOPTY */
extern VOID regionscroll __P_((int, int, int, int, int, int));
extern int selectpty __P_((int, int [], char [], int));
extern VOID syncptyout __P_((VOID_A));
extern int recvbuf __P_((int, VOID_P, int));
extern VOID sendbuf __P_((int, VOID_P, int));
extern int recvword __P_((int, int *));
extern VOID sendword __P_((int, int));
extern int recvstring __P_((int, char **));
extern VOID sendstring __P_((int, char *));
extern VOID sendparent __P_((int, ...));
extern int ptymacro __P_((char *, char *, int));
# ifdef	_NOORIGSHELL
# define	ptyusercomm(c,a,f)	ptymacro(c, a, f | F_EVALMACRO)
# else
# define	ptyusercomm		ptymacro
# endif
#define	ptysystem(c)		ptymacro(c, NULL, F_DOSYSTEM)
extern VOID killpty __P_((int, int *));
extern VOID killallpty __P_((VOID_A));
#endif	/* !_NOPTY */

/* frontend.c */
#ifdef	_NOPTY
#define	Xttyiomode		ttyiomode
#define	Xstdiomode		stdiomode
#define	Xtermmode		termmode
#define	Xputch2			putch2
#define	Xcputs2			cputs2
#define	Xputterm		putterm
#define	Xputterms		putterms
#define	Xsetscroll		setscroll
#define	Xlocate			locate
#define	Xtflush			tflush
#define	Xcprintf2		cprintf2
#define	Xcputnl			cputnl
#define	Xkanjiputs		kanjiputs
#define	Xchgcolor		chgcolor
#define	Xmovecursor		movecursor
#else	/* !_NOPTY */
extern int waitstatus __P_((p_id_t, int, int *));
extern VOID Xttyiomode __P_((int));
extern VOID Xstdiomode __P_((VOID_A));
extern int Xtermmode __P_((int));
extern VOID Xputch2 __P_((int));
extern VOID Xcputs2 __P_((char *));
extern VOID Xputterm __P_((int));
extern VOID Xputterms __P_((int));
extern int Xsetscroll __P_((int, int));
extern VOID Xlocate __P_((int, int));
extern VOID Xtflush __P_((VOID_A));
extern int Xcprintf2 __P_((CONST char *, ...));
extern VOID Xcputnl __P_((VOID_A));
extern int Xkanjiputs __P_((char *));
extern VOID Xchgcolor __P_((int, int));
extern VOID Xmovecursor __P_((int, int, int));
extern VOID changewin __P_((int, p_id_t));
extern VOID changewsize __P_((int, int));
extern VOID insertwin __P_((int, int));
extern VOID deletewin __P_((int, int));
# ifndef	_NOKANJICONV
extern VOID changekcode __P_((VOID_A));
# endif
extern int frontend __P_((VOID_A));
#endif	/* !_NOPTY */

/* backend.c */
#ifndef	_NOPTY
extern VOID resetptyterm __P_((int, int));
extern int backend __P_((VOID_A));
#endif

/* libc.c */
extern int stat2 __P_((char *, struct stat *));
extern char *realpath2 __P_((char *, char *, int));
extern int _chdir2 __P_((char *));
extern int chdir2 __P_((char *));
extern int chdir3 __P_((char *, int));
extern int mkdir2 __P_((char *, int));
extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern char *strdup2 __P_((char *));
extern char *strndup2 __P_((char *, int));
extern char *c_realloc __P_((char *, ALLOC_T, ALLOC_T *));
extern char *strchr2 __P_((char *, int));
extern char *strrchr2 __P_((char *, int));
extern char *strcpy2 __P_((char *, char *));
extern char *strncpy2 __P_((char *, char *, int));
extern int strncpy3 __P_((char *, char *, int *, int));
extern int strlen2 __P_((char *));
extern int strlen3 __P_((char *));
extern int atoi2 __P_((char *));
extern char *asprintf3 __P_((CONST char *, ...));
extern VOID perror2 __P_((char *));
extern int setenv2 __P_((char *, char *, int));
extern char *getenv2 __P_((char *));
#ifdef	USESIGACTION
extern sigcst_t signal2 __P_((int, sigcst_t));
#else
#define	signal2	signal
#endif
extern int system2 __P_((char *, int));
extern FILE *popen2 __P_((char *));
extern int pclose2 __P_((FILE *));
extern char *getwd2 __P_((VOID_A));
extern time_t time2 __P_((VOID_A));
extern time_t timelocal2 __P_((struct tm *));
extern char *fgets2 __P_((FILE *, int));

/* file.c */
#ifdef	_USEDOSEMU
extern char *nodospath __P_((char *, char *));
#else
#define	nodospath(p, f)	(f)
#endif
#define	fnodospath(p, i)	nodospath(p, filelist[i].name)
#ifdef	NOUID
extern int logical_access __P_((u_int));
#else
extern int logical_access __P_((u_int, uid_t, gid_t));
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
#ifdef	_NODOSDRIVE
#define	nodoslstat	Xlstat
#else
extern int nodoslstat __P_((char *, struct stat *));
#endif
extern VOID lostcwd __P_((char *));
#ifndef	NODIRLOOP
extern int issamebody __P_((char *, char *));
#endif
#ifndef	NOSYMLINK
extern int cpsymlink __P_((char *, char *));
#endif
extern int safecpfile __P_((char *, char *, struct stat *, struct stat *));
extern int safemvfile __P_((char *, char *, struct stat *, struct stat *));
extern char *genrandname __P_((char *, int));
extern int mktmpdir __P_((char *));
extern int rmtmpdir __P_((char *));
extern int mktmpfile __P_((char *));
extern int rmtmpfile __P_((char *));
extern VOID removetmp __P_((char *, char *));
extern int forcecleandir __P_((char *, char *));
#ifndef	_NODOSDRIVE
extern char *dostmpdir __P_((int));
extern int tmpdosdupl __P_((char *, char **, int));
extern int tmpdosrestore __P_((int, char *));
#endif
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
extern int forcemovefile __P_((char *));

/* parse.c */
extern char *skipspace __P_((char *));
extern char *sscanf2 __P_((char *, CONST char *, ...));
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
#ifdef	_NOORIGSHELL
extern VOID adjustpath __P_((VOID_A));
#endif
extern char *includepath __P_((char *, char *));
#if	(FD < 2) && !defined (_NOARCHIVE)
extern char *getrange __P_((char *, int, u_char *, u_char *, u_char *));
#endif
extern int evalprompt __P_((char **, char *));
#ifndef	_NOARCHIVE
extern char *getext __P_((char *, u_char *));
extern int extcmp __P_((char *, int, char *, int, int));
#endif
extern int getkeycode __P_((char *, int));
extern char *getkeysym __P_((int, int));
extern char *decodestr __P_((char *, u_char *, int));
#ifndef	_NOKEYMAP
extern char *encodestr __P_((char *, int));
#endif

/* builtin.c */
#ifndef	_NOARCHIVE
extern VOID freelaunch __P_((launchtable *));
extern int searchlaunch __P_((launchtable *, int, launchtable *));
extern int parselaunch __P_((int, char *[], launchtable *));
extern VOID addlaunch __P_((int, launchtable *));
extern VOID deletelaunch __P_((int));
extern VOID freearch __P_((archivetable *));
extern int searcharch __P_((archivetable *, int, archivetable *));
extern int parsearch __P_((int, char *[], archivetable *));
extern VOID addarch __P_((int, archivetable *));
extern VOID deletearch __P_((int));
extern VOID printlaunchcomm __P_((launchtable *, int, int, int, FILE *));
extern VOID printarchcomm __P_((archivetable *, int, int, FILE *));
# ifndef	_NOBROWSE
extern VOID freebrowse __P_((launchtable *));
# endif
#endif
extern int ismacro __P_((int));
extern int freemacro __P_((int));
extern int searchkeybind __P_((bindtable *, bindtable *));
extern int parsekeybind __P_((int, char *[], bindtable *));
extern int addkeybind __P_((int, bindtable *, char *, char *, char *));
extern VOID deletekeybind __P_((int));
extern char *gethelp __P_((bindtable *));
extern VOID printmacro __P_((bindtable *, int, int, FILE *));
#ifdef	_USEDOSEMU
extern int searchdrv __P_((devinfo *, devinfo *, int));
extern int deletedrv __P_((int));
extern int insertdrv __P_((int, devinfo *));
extern VOID printsetdrv __P_((devinfo [], int, int, int, FILE *));
extern int parsesetdrv __P_((int, char *[], devinfo *));
#endif
#ifndef	_NOKEYMAP
extern int parsekeymap __P_((int, char *[], keyseq_t *));
extern VOID printkeymap __P_((keyseq_t *, int, FILE *));
#endif
#ifdef	_NOORIGSHELL
extern int searchalias __P_((char *, int));
extern int addalias __P_((char *, char *));
extern int deletealias __P_((char *));
extern VOID printalias __P_((int, FILE *));
extern int searchfunction __P_((char *));
extern int addfunction __P_((char *, char **));
extern int deletefunction __P_((char *));
extern VOID printfunction __P_((int, int, FILE *));
#endif	/* _NOORIGSHELL */
extern int checkbuiltin __P_((char *));
extern int checkinternal __P_((char *));
extern int execbuiltin __P_((int, int, char *[]));
extern int execinternal __P_((int, int, char *[]));
#ifdef	_NOORIGSHELL
extern int execpseudoshell __P_((char *, int));
#endif
#ifndef	_NOCOMPLETE
extern int completebuiltin __P_((char *, int, int, char ***));
extern int completeinternal __P_((char *, int, int, char ***));
#endif
#ifdef	DEBUG
extern VOID freedefine __P_((VOID_A));
#endif

#define	BL_SET		"set"
#define	BL_UNSET	"unset"
#define	BL_PENV		"printenv"
#define	BL_LAUNCH	"launch"
#define	BL_ARCH		"arch"
#define	BL_BROWSE	"browse"
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
#define	BL_EVALMACRO	"evalmacro"
#define	BL_KCONV	"kconv"
#define	BL_READLINE	"readline"
#define	BL_YESNO	"yesno"
#define	BL_ALIAS	"alias"
#define	BL_UALIAS	"unalias"
#define	BL_FUNCTION	"function"
#define	BL_EXPORT	"export"
#define	BL_CHDIR	"chdir"
#define	BL_CD		"cd"
#define	BL_SOURCE	"source"
#define	BL_DOT		"."

/* shell.c */
extern char *restorearg __P_((char *));
extern char *evalcommand __P_((char *, char *, macrostat *));
extern int replaceargs __P_((int *, char ***, char **, int));
extern int replacearg __P_((char **));
extern VOID demacroarg __P_((char **));
extern char *inputshellstr __P_((char *, int, char *));
extern char *inputshellloop __P_((int, char *));
extern int isinternalcomm __P_((char *));
extern int execmacro __P_((char *, char *, int));
extern FILE *popenmacro __P_((char *, char *, int));
#ifdef	_NOORIGSHELL
extern int execusercomm __P_((char *, char *, int));
#else
#define	execusercomm	execmacro
#endif
extern int entryhist __P_((int, char *, int));
extern char *removehist __P_((int));
extern int loadhistory __P_((int, char *));
extern VOID convhistory __P_((char *, FILE *));
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
#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
extern int getlang __P_((char *, int));
#endif
#if	!defined (_NOKANJICONV) \
|| (defined (_USEDOSEMU) && defined (CODEEUC))
extern int sjis2ujis __P_((char *, u_char *, int));
extern int ujis2sjis __P_((char *, u_char *, int));
#endif
#if	!defined (_NOKANJICONV) || !defined (_NODOSDRIVE)
extern VOID readunitable __P_((int));
extern VOID discardunitable __P_((VOID_A));
extern u_int unifysjis __P_((u_int, int));
extern u_int cnvunicode __P_((u_int, int));
#endif
#ifndef	_NOKANJICONV
extern int kanjiconv __P_((char *, char *, int, int, int, int));
extern char *kanjiconv2 __P_((char *, char *, int, int, int, int));
extern char *newkanjiconv __P_((char *, int, int, int));
#endif
#ifndef	_NOKANJIFCONV
extern int getkcode __P_((char *));
#endif
extern char *convget __P_((char *, char *, int));
extern char *convput __P_((char *, char *, int, int, char *, int *));

/* input.c */
extern int intrkey __P_((VOID_A));
extern int Xgetkey __P_((int, int));
extern int kanjiputs2 __P_((char *, int, int));
extern char *inputstr __P_((char *, int, int, char *, int));
extern int yesno __P_((CONST char *, ...));
extern VOID warning __P_((int, char *));
extern int selectstr __P_((int *, int, int, char *[], int []));

/* info.c */
extern VOID help __P_((int));
extern int writablefs __P_((char *));
extern off_t getblocksize __P_((char *));
extern off_t calcKB __P_((off_t, off_t));
extern int getinfofs __P_((char *, off_t *, off_t *, off_t *));
extern int infofs __P_((char *));

/* rockridge.c */
#ifndef	_NOROCKRIDGE
extern char *transpath __P_((char *, char *));
extern char *detranspath __P_((char *, char *));
extern int rrlstat __P_((char *, struct stat *));
extern int rrreadlink __P_((char *, char *, int));
#endif

/* archive.c */
#ifndef	_NOARCHIVE
extern VOID escapearch __P_((VOID_A));
extern VOID archbar __P_((char *, char *));
extern VOID copyarcf __P_((reg_t *, char *));
extern char *archchdir __P_((char *));
# ifndef	_NOCOMPLETE
extern int completearch __P_((char *, int, int, char ***));
# endif
extern int launcher __P_((VOID_A));
extern int dolaunch __P_((launchtable *, int));
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
extern VOID freestrarray __P_((char **, int));
extern char **copystrarray __P_((char **, char **, int *, int));
extern bindtable *copybind __P_((bindtable *, bindtable *));
# ifndef	_NOARCHIVE
extern VOID freelaunchlist __P_((launchtable *, int));
extern launchtable *copylaunch __P_((launchtable *, launchtable *,
	int *, int));
extern VOID freearchlist __P_((archivetable *, int));
extern archivetable *copyarch __P_((archivetable *, archivetable *,
	int *, int));
# endif
# ifdef	_USEDOSEMU
extern VOID freedosdrive __P_((devinfo *));
extern devinfo *copydosdrive __P_((devinfo *, devinfo *));
# endif
extern VOID rewritecust __P_((VOID_A));
extern int customize __P_((VOID_A));
#endif	/* !_NOCUSTOMIZE */

/* command.c */
#ifndef	_NOSPLITWIN
extern int nextwin __P_((VOID_A));
#endif

/* browse.c */
extern VOID helpbar __P_((VOID_A));
extern int putmode __P_((char *, u_int, int));
#ifdef	HAVEFLAGS
extern int putflags __P_((char *, u_long));
#endif
extern VOID waitmes __P_((VOID_A));
extern int filetop __P_((int));
extern int calcwidth __P_((VOID_A));
extern int putname __P_((namelist *, int, int));
extern int listupfile __P_((namelist *, int, char *, int));
#ifndef	_NOSPLITWIN
extern int shutwin __P_((int));
#endif
extern VOID calcwin __P_((VOID_A));
extern VOID movepos __P_((int, int));
extern VOID rewritefile __P_((int));
extern VOID addlist __P_((VOID_A));
extern VOID main_fd __P_((char **));
