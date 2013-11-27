/*
 *	func.h
 *
 *	function prototype declarations
 */

#include "depend.h"
#include "sysemu.h"
#include "pathname.h"
#include "term.h"
#include "types.h"
#include "dosdisk.h"
#include "unixemu.h"

#define	getblock(c)		((((c) + blocksize - 1) / blocksize) \
				* blocksize)

#define	isleftshift()		(n_column < WCOLUMNSTD)
#define	isrightomit()		(n_column < WCOLUMNSTD - 1)
#define	ispureshift()		(n_column == WCOLUMNSTD - 1)
#define	isshortwid()		(n_column < WCOLUMNSTD - 1)
#define	iswellomit()		(n_column < WCOLUMNSTD - 2)
#define	ishardomit()		(n_column < WCOLUMNOMIT)
#define	isbestomit()		(n_column < WCOLUMNHARD)

#ifdef	DEP_ORIGSHELL
#define	getconstvar(s)		(getshellvar(s, strsize(s)))
#else
#define	getconstvar(s)		(char *)getenv(s)
#define	getshellvar(s, l)	(char *)getenv(s)
#endif

#ifdef	DEP_PTY
# ifdef	DEP_ORIGSHELL
# define	isptymode()	(ptymode && (fdmode || !shptymode))
# define	isshptymode()	(!fdmode && shptymode)
# else
# define	isptymode()	ptymode
# define	isshptymode()	0
# endif
#else	/* !DEP_PTY */
#define	isptymode()		0
#define	isshptymode()		0
#endif	/* !DEP_PTY */

/* main.c */
extern VOID error __P_((CONST char *));
extern VOID setlinecol __P_((VOID_A));
extern VOID checkscreen __P_((int, int));
#ifdef	SIGWINCH
extern VOID pollscreen __P_((int));
#endif
extern int sigvecset __P_((int));
extern char *getversion __P_((int *));
extern VOID title __P_((VOID_A));
#ifndef	_NOCUSTOMIZE
extern VOID saveorigenviron __P_((VOID_A));
#endif
extern int loadruncom __P_((CONST char *, int));
extern VOID initfd __P_((char *CONST *));
extern VOID prepareexitfd __P_((int));

/* system.c */
#ifdef	DEP_ORIGSHELL
extern int dosystem __P_((CONST char *));
extern XFILE *dopopen __P_((CONST char *));
extern int dopclose __P_((XFILE *));
#else
#define	dosystem		system
#define	dopopen(c)		Xpopen(c, "r")
#define	dopclose		Xpclose
#endif

/* libc.c */
extern int stat2 __P_((CONST char *, struct stat *));
extern int _chdir2 __P_((CONST char *));
extern int chdir2 __P_((CONST char *));
extern int chdir3 __P_((CONST char *, int));
extern int chdir4 __P_((CONST char *, int, CONST char *));
extern int mkdir2 __P_((char *, int));
extern int strncpy2 __P_((char *, CONST char *, int *, int));
extern VOID perror2 __P_((CONST char *));
extern int setenv2 __P_((CONST char *, CONST char *, int));
extern char *getenv2 __P_((CONST char *));
#ifdef	USESIGACTION
extern sigcst_t signal2 __P_((int, sigcst_t));
#else
#define	signal2			signal
#endif
extern int system2 __P_((CONST char *, int));
extern XFILE *popen2 __P_((CONST char *, int));
extern int pclose2 __P_((XFILE *));
extern char *getwd2 __P_((VOID_A));

/* file.c */
#ifdef	DEP_DOSEMU
extern CONST char *nodospath __P_((char *, CONST char *));
#else
#define	nodospath(p, f)		(f)
#endif
#define	fnodospath(p, i)	nodospath(p, filelist[i].name)
extern int getstatus __P_((namelist *));
extern int cmplist __P_((CONST VOID_P, CONST VOID_P));
#ifndef	_NOTREE
extern int cmptree __P_((CONST VOID_P, CONST VOID_P));
#endif
extern struct dirent *searchdir __P_((DIR *, CONST reg_ex_t *, CONST char *));
extern int underhome __P_((char *));
extern int preparedir __P_((CONST char *));
extern lockbuf_t *lockopen __P_((CONST char *, int, int));
extern lockbuf_t *lockfopen __P_((CONST char *, CONST char *, int, int));
extern VOID lockclose __P_((lockbuf_t *));
extern int touchfile __P_((CONST char *, struct stat *));
#ifdef	DEP_PSEUDOPATH
extern int reallstat __P_((CONST char *, struct stat *));
#else
#define	reallstat		Xlstat
#endif
extern VOID lostcwd __P_((char *));
#ifndef	NODIRLOOP
extern int issamebody __P_((CONST char *, CONST char *));
#endif
#ifndef	NOSYMLINK
extern int cpsymlink __P_((CONST char *, CONST char *));
#endif
extern int safecpfile __P_((CONST char *, CONST char *,
		struct stat *, struct stat *));
extern int safemvfile __P_((CONST char *, CONST char *,
		struct stat *, struct stat *));
extern char *genrandname __P_((char *, int));
extern int mktmpdir __P_((char *));
extern int rmtmpdir __P_((CONST char *));
extern int opentmpfile __P_((CONST char *, int));
extern int mktmpfile __P_((char *));
extern int rmtmpfile __P_((CONST char *));
extern VOID removetmp __P_((char *, CONST char *));
extern int forcecleandir __P_((CONST char *, CONST char *));
#ifdef	DEP_PSEUDOPATH
extern char *dostmpdir __P_((int));
extern int tmpdosdupl __P_((CONST char *, char **, int));
extern int tmpdosrestore __P_((int, CONST char *));
#endif
#ifndef	_NOWRITEFS
extern VOID arrangedir __P_((int));
#endif

/* apply.c */
#ifndef	_NOEXTRACOPY
extern VOID showprogress __P_((off_t *));
extern VOID fshowprogress __P_((CONST char *));
#endif
extern int prepareremove __P_((int, int));
extern int rmvfile __P_((CONST char *));
extern int rmvdir __P_((CONST char *));
extern int findfile __P_((CONST char *));
extern int finddir __P_((CONST char *));
extern int inputattr __P_((namelist *, int));
extern int setattr __P_((CONST char *));
extern int applyfile __P_((int (*)__P_((CONST char *)), CONST char *));
extern int applydir __P_((CONST char *, int (*)__P_((CONST char *)),
		int (*)__P_((CONST char *)), int (*)__P_((CONST char *)),
		int, CONST char *));
extern int copyfile __P_((CONST char *, int));
extern int movefile __P_((CONST char *, int));
extern int forcemovefile __P_((char *));

/* pty.c */
#ifdef	DEP_PTY
extern int Xopenpty __P_((int *, int *, char *, ALLOC_T));
extern int Xlogin_tty __P_((CONST char *, CONST char *, CONST char *));
extern p_id_t Xforkpty __P_((int *, CONST char *, CONST char *));
#endif

#ifdef	DEP_PTY
# ifdef	DEP_ORIGSHELL
# define	ptyusercomm	ptymacro
# else
# define	ptyusercomm(c, a, f) \
				ptymacro(c, a, f | F_EVALMACRO)
# endif
#define	ptysystem(c)		ptymacro(c, NULL, F_DOSYSTEM)
#else	/* !DEP_PTY */
#define	ptymacro		execmacro
#define	ptyusercomm		execusercomm
#define	ptysystem		dosystem
#endif	/* !DEP_PTY */

/* frontend.c */
#ifdef	DEP_PTY
extern int waitstatus __P_((p_id_t, int, int *));
extern VOID Xttyiomode __P_((int));
extern VOID Xstdiomode __P_((VOID_A));
extern int Xtermmode __P_((int));
extern int XXputch __P_((int));
extern VOID XXcputs __P_((CONST char *));
extern VOID Xputterm __P_((int));
extern VOID Xputterms __P_((int));
extern int Xsetscroll __P_((int, int));
extern VOID Xlocate __P_((int, int));
extern VOID Xtflush __P_((VOID_A));
extern int XXcprintf __P_((CONST char *, ...));
extern VOID Xcputnl __P_((VOID_A));
extern int Xkanjiputs __P_((CONST char *));
extern VOID Xattrputs __P_((CONST char *, int));
extern int Xattrprintf __P_((CONST char *, int, ...));
extern int Xattrkanjiputs __P_((CONST char *, int));
extern VOID Xchgcolor __P_((int, int));
extern VOID Xmovecursor __P_((int, int, int));
extern VOID changewin __P_((int, p_id_t));
extern VOID changewsize __P_((int, int));
extern VOID insertwin __P_((int, int));
extern VOID deletewin __P_((int, int));
# ifdef	DEP_KCONV
extern VOID changekcode __P_((VOID_A));
extern VOID changeinkcode __P_((VOID_A));
extern VOID changeoutkcode __P_((VOID_A));
# endif
extern int frontend __P_((VOID_A));
#else	/* !DEP_PTY */
#define	Xttyiomode		ttyiomode
#define	Xstdiomode		stdiomode
#define	Xtermmode		termmode
#define	XXputch			Xputch
#define	XXcputs			Xcputs
#define	Xputterm		putterm
#define	Xputterms		putterms
#define	Xsetscroll		setscroll
#define	Xlocate			locate
#define	Xtflush			tflush
#define	XXcprintf		Xcprintf
#define	Xcputnl			cputnl
#define	Xkanjiputs		kanjiputs
#define	Xattrputs		attrputs
#define	Xattrprintf		attrprintf
#define	Xattrkanjiputs		attrkanjiputs
#define	Xchgcolor		chgcolor
#define	Xmovecursor		movecursor
#endif	/* !DEP_PTY */

/* backend.c */
#ifdef	DEP_PTY
extern VOID resetptyterm __P_((int, int));
extern int backend __P_((VOID_A));
#endif

/* builtin.c */
#ifndef	_NOARCHIVE
extern VOID freelaunch __P_((lsparse_t *));
extern int searchlaunch __P_((CONST lsparse_t *, CONST lsparse_t *, int));
extern int parselaunch __P_((int, char *CONST [], lsparse_t *));
extern VOID addlaunch __P_((int, lsparse_t *));
extern VOID deletelaunch __P_((int));
extern VOID freearch __P_((archive_t *));
extern int searcharch __P_((CONST archive_t *, CONST archive_t *, int));
extern int parsearch __P_((int, char *CONST [], archive_t *));
extern VOID addarch __P_((int, archive_t *));
extern VOID deletearch __P_((int));
extern VOID printlaunchcomm __P_((CONST lsparse_t *, int, int, int, XFILE *));
extern VOID printarchcomm __P_((CONST archive_t *, int, int, XFILE *));
# ifndef	_NOBROWSE
extern VOID freebrowse __P_((lsparse_t *));
# endif
#endif
extern int ismacro __P_((int));
extern CONST char *getmacro __P_((int));
extern int freemacro __P_((int));
extern int searchkeybind __P_((CONST bindtable *, CONST bindtable *, int));
extern int parsekeybind __P_((int, char *CONST [], bindtable *));
extern int addkeybind __P_((int, CONST bindtable *, char *, char *, char *));
extern VOID deletekeybind __P_((int));
extern char *gethelp __P_((CONST bindtable *));
extern VOID printmacro __P_((CONST bindtable *, int, int, XFILE *));
#ifdef	DEP_DOSEMU
extern int searchdrv __P_((CONST devinfo *, CONST devinfo *, int, int));
extern int deletedrv __P_((int));
extern int insertdrv __P_((int, devinfo *));
extern VOID printsetdrv __P_((CONST devinfo *, int, int, int, XFILE *));
extern int parsesetdrv __P_((int, char *CONST [], devinfo *));
#endif
#ifndef	_NOKEYMAP
extern int parsekeymap __P_((int, char *CONST [], keyseq_t *));
extern VOID printkeymap __P_((keyseq_t *, int, XFILE *));
#endif
#if	!MSDOS
extern int savestdio __P_((int));
#endif
#ifndef	DEP_ORIGSHELL
extern int searchalias __P_((CONST char *, int));
extern int addalias __P_((char *, char *));
extern int deletealias __P_((CONST char *));
extern VOID printalias __P_((int, XFILE *));
extern int searchfunction __P_((CONST char *));
extern int addfunction __P_((char *, char **));
extern int deletefunction __P_((CONST char *));
extern VOID printfunction __P_((int, int, XFILE *));
#endif	/* !DEP_ORIGSHELL */
extern int checkbuiltin __P_((CONST char *));
extern int checkinternal __P_((CONST char *));
extern int execbuiltin __P_((int, int, char *CONST []));
extern int execinternal __P_((int, int, char *CONST []));
#ifndef	DEP_ORIGSHELL
extern int execpseudoshell __P_((CONST char *, int));
#endif
#ifndef	_NOCOMPLETE
extern int completebuiltin __P_((CONST char *, int, int, char ***));
extern int completeinternal __P_((CONST char *, int, int, char ***));
#endif
#ifdef	DEBUG
extern VOID freedefine __P_((VOID_A));
#endif

#define	BL_SET			"set"
#define	BL_UNSET		"unset"
#define	BL_PENV			"printenv"
#define	BL_LAUNCH		"launch"
#define	BL_ARCH			"arch"
#define	BL_BROWSE		"browse"
#define	BL_PLAUNCH		"printlaunch"
#define	BL_PARCH		"printarch"
#define	BL_BIND			"bind"
#define	BL_PBIND		"printbind"
#define	BL_SDRIVE		"setdrv"
#define	BL_UDRIVE		"unsetdrv"
#define	BL_PDRIVE		"printdrv"
#define	BL_KEYMAP		"keymap"
#define	BL_GETKEY		"getkey"
#define	BL_HISTORY		"history"
#define	BL_FC			"fc"
#define	BL_CHECKID		"checkid"
#define	BL_EVALMACRO		"evalmacro"
#define	BL_KCONV		"kconv"
#define	BL_GETFREQ		"getfreq"
#define	BL_SETFREQ		"setfreq"
#define	BL_READLINE		"readline"
#define	BL_YESNO		"yesno"
#define	BL_SAVETTY		"savetty"
#define	BL_SETROMAN		"setroman"
#define	BL_PRINTROMAN		"printroman"
#define	BL_ALIAS		"alias"
#define	BL_UALIAS		"unalias"
#define	BL_FUNCTION		"function"
#define	BL_EXPORT		"export"
#define	BL_CHDIR		"chdir"
#define	BL_CD			"cd"
#define	BL_SOURCE		"source"
#define	BL_DOT			"."

/* shell.c */
extern char *restorearg __P_((CONST char *));
extern char *evalcommand __P_((CONST char *, CONST char *, macrostat *));
extern int replaceargs __P_((int *, char ***, char *CONST *, int));
extern int replacearg __P_((char **));
extern VOID demacroarg __P_((char **));
extern char *inputshellstr __P_((CONST char *, int, CONST char *));
extern char *inputshellloop __P_((int, CONST char *));
extern int isinternalcomm __P_((CONST char *));
extern int execmacro __P_((CONST char *, CONST char *, int));
extern XFILE *popenmacro __P_((CONST char *, CONST char *, int));
#ifdef	DEP_ORIGSHELL
#define	execusercomm		execmacro
#else
extern int execusercomm __P_((CONST char *, CONST char *, int));
#endif
extern int entryhist __P_((CONST char *, int));
extern char *removehist __P_((int));
extern int loadhistory __P_((int));
extern int savehistory __P_((int));
extern int parsehist __P_((CONST char *, int *, int));
extern char *evalhistory __P_((char *));
#ifdef	DEBUG
extern VOID freehistory __P_((int));
#endif
#if	!defined (_NOCOMPLETE) && !defined (DEP_ORIGSHELL)
extern int completealias __P_((CONST char *, int, int, char ***));
extern int completeuserfunc __P_((CONST char *, int, int, char ***));
#endif

/* input.c */
extern int intrkey __P_((int));
extern int Xgetkey __P_((int, int, int));
extern int kanjiputs2 __P_((CONST char *, int, int));
extern VOID cputspace __P_((int));
extern VOID cputstr __P_((int, CONST char *));
extern VOID attrputstr __P_((int, CONST char *, int));
extern char *inputstr __P_((CONST char *, int, int, CONST char *, int));
extern int yesno __P_((CONST char *, ...));
extern VOID warning __P_((int, CONST char *));
extern int selectstr __P_((int *, int, int, CONST char *CONST [], int []));
#ifdef	DEP_URLPATH
extern char *inputpass __P_((VOID_A));
#endif

/* ime.c */
#ifdef	DEP_IME
# ifdef	DEP_PTY
extern u_int ime_getkeycode __P_((CONST char *));
extern int ime_inkanjiconv __P_((char *, u_int));
# endif
extern int ime_inputkanji __P_((int, char *));
# ifdef	DEBUG
extern VOID ime_freebuf __P_((VOID_A));
# endif
#endif	/* DEP_IME */

/* dict.c */
#ifdef	DEP_IME
extern VOID discarddicttable __P_((VOID_A));
extern VOID saveuserfreq __P_((CONST u_short *, CONST u_short *));
extern VOID freekanjilist __P_((u_short **));
extern u_short **searchdict __P_((u_short *, int));
# if	FD >= 3
extern int fgetuserfreq __P_((CONST char *, XFILE *));
extern int fputuserfreq __P_((CONST char *, XFILE *));
# endif
#endif	/* DEP_IME */

/* info.c */
extern VOID help __P_((int));
#ifndef	NOFLOCK
extern int isnfs __P_((CONST char *));
#endif
extern int writablefs __P_((CONST char *));
extern off_t getblocksize __P_((CONST char *));
extern off_t calcKB __P_((off_t, off_t));
extern int getinfofs __P_((CONST char *, off_t *, off_t *, off_t *));
extern int infofs __P_((CONST char *));

/* rockridge.c */
#ifndef	_NOROCKRIDGE
extern char *transpath __P_((CONST char *, char *));
extern char *detranspath __P_((CONST char *, char *));
extern int rrlstat __P_((CONST char *, struct stat *));
extern int rrreadlink __P_((CONST char *, char *, int));
#endif

/* tree.c */
#ifndef	_NOTREE
extern VOID rewritetree __P_((VOID_A));
extern char *tree __P_((int, int *));
#endif

/* archive.c */
#ifndef	_NOARCHIVE
extern VOID escapearch __P_((int));
extern VOID archbar __P_((CONST char *, CONST char *));
extern VOID copyarcf __P_((CONST reg_ex_t *, CONST char *));
extern CONST char *archchdir __P_((CONST char *));
# ifndef	_NOCOMPLETE
extern int completearch __P_((CONST char *, int, int, char ***));
# endif
extern int launcher __P_((VOID_A));
extern int dolaunch __P_((lsparse_t *, int));
extern int pack __P_((CONST char *));
extern int unpack __P_((CONST char *, CONST char *, CONST char *, int, int));
extern char *tmpunpack __P_((int));
extern int backup __P_((CONST char *));
extern int searcharcf __P_((CONST char *, namelist *, int, int));
#endif

/* custom.c */
extern VOID initenv __P_((VOID_A));
extern VOID evalenv __P_((CONST char *, int));
#ifdef	DEBUG
extern VOID freeenvpath __P_((VOID_A));
#endif
extern VOID freestrarray __P_((char **, int));
extern char **copystrarray __P_((char **, char *CONST *, int *, int));
#if	!defined (_NOCUSTOMIZE) || defined (DEP_DYNAMICLIST)
extern bindtable *copybind __P_((bindtable *, CONST bindtable *, int *, int));
#endif
#ifndef	_NOARCHIVE
extern VOID freelaunchlist __P_((lsparse_t *, int));
extern lsparse_t *copylaunch __P_((lsparse_t *, CONST lsparse_t *,
		int *, int));
extern VOID freearchlist __P_((archive_t *, int));
extern archive_t *copyarch __P_((archive_t *, CONST archive_t *, int *, int));
#endif
#ifdef	DEP_DOSEMU
extern VOID freedosdrive __P_((devinfo *, int));
extern devinfo *copydosdrive __P_((devinfo *, CONST devinfo *, int *, int));
#endif
#ifndef	_NOCUSTOMIZE
extern VOID rewritecust __P_((VOID_A));
extern int customize __P_((CONST char *));
#endif

/* command.c */
extern int evalstatus __P_((int));
#ifndef	_NOSPLITWIN
extern int nextwin __P_((VOID_A));
#endif

/* browse.c */
extern VOID helpbar __P_((VOID_A));
extern int putmode __P_((char *, u_int, int));
#ifdef	HAVEFLAGS
extern int putflags __P_((char *, u_long));
#endif
#ifndef	NOUID
extern int putowner __P_((char *, u_id_t));
extern int putgroup __P_((char *, g_id_t));
#endif
extern VOID waitmes __P_((VOID_A));
extern int filetop __P_((int));
extern int calcwidth __P_((VOID_A));
extern int putname __P_((namelist *, int, int));
extern VOID setlastfile __P_((CONST char *));
extern int listupfile __P_((namelist *, int, CONST char *, int));
#ifndef	_NOSPLITWIN
extern int shutwin __P_((int));
#endif
extern VOID calcwin __P_((VOID_A));
extern VOID movepos __P_((int, int));
extern VOID rewritefile __P_((int));
extern VOID getfilelist __P_((VOID_A));
extern int dointernal __P_((int, CONST char *, int, int *));
extern VOID main_fd __P_((char *CONST *, int));
