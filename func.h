/*
 *	func.h
 *
 *	Function Prototype Declaration
 */

#ifdef	NOERRNO
extern int errno;
#endif

#if !defined (ENOTEMPTY) && defined (ENFSNOTEMPTY)
#define	ENOTEMPTY	ENFSNOTEMPTY
#endif

#if	MSDOS
#include "unixemu.h"
#define	strpathcmp	strcasecmp2
#define	strnpathcmp	strnicmp
#else
# ifdef	USEDIRECT
# include <sys/dir.h>
#  ifdef	DIRSIZ
#  undef	DIRSIZ  
#  endif
# else
# include <dirent.h>
# endif
#define	strpathcmp	strcmp
#define	strnpathcmp	strncmp
#endif

/* main.c */
extern VOID error();
extern VOID sigvecset();
extern VOID sigvecreset();
extern VOID title();
extern int loadruncom();

/* dosemu.c or unixemu.c */
#if	MSDOS || !defined (_NODOSDRIVE)
extern int _dospath();
extern int dospath();
#endif
extern DIR *_Xopendir();
extern DIR *Xopendir();
extern int Xclosedir();
extern struct dirent *Xreaddir();
extern VOID Xrewinddir();
extern int _Xchdir();
extern int Xchdir();
extern char *Xgetcwd();
extern int Xstat();
extern int Xlstat();
extern int Xaccess();
extern int Xsymlink();
extern int Xreadlink();
extern int Xchmod();
#ifdef	USEUTIME
extern int Xutime();
#else
extern int Xutimes();
#endif
extern int Xunlink();
extern int Xrename();
extern int Xopen();
extern int Xclose();
extern int Xread();
extern int Xwrite();
extern off_t Xlseek();
extern int _Xmkdir();
extern int Xmkdir();
extern int _Xrmdir();
extern int Xrmdir();
extern FILE *_Xfopen();
extern FILE *Xfopen();
extern int Xfclose();
extern int Xfeof();
extern int Xfread();
extern int Xfwrite();
extern int Xfflush();
extern int Xfgetc();
extern int Xfputc();
extern char *Xfgets();
extern int Xfputs();
#if	MSDOS
extern FILE *Xpopen();
extern int Xpclose();
#else	/* !MSDOS */
#ifndef _NODOSDRIVE
extern char *tmpdosdupl();
extern int tmpdosrestore();
#endif
#endif	/* !MSDOS */

/* rockridge.c */
#ifndef	_NOROCKRIDGE
extern char *transfile();
extern char *detransfile();
#endif

/* parse.c */
extern char *skipspace();
extern char *strtkbrk();
extern char *strtkchr();
extern char *evalcomstr();
#if	!MSDOS
extern char *killmeta();
extern VOID adjustpath();
#endif
extern int getargs();
extern int evalconfigline();
extern int printmacro();
#ifndef	_NOARCHIVE
extern int printlaunch();
extern int printarch();
#endif
extern int printalias();
extern int printuserfunc();
#if	!MSDOS && !defined (_NODOSDRIVE)
extern int printdrive();
#endif
extern int printhist();
extern int evalbool();
extern VOID evalenv();

/* libc.c */
extern int access2();
extern int unlink2();
extern int rmdir2();
extern int rename2();
extern int stat2();
extern char *realpath2();
extern int _chdir2();
extern int chdir2();
extern char *chdir3();
extern int mkdir2();
extern VOID_P malloc2();
extern VOID_P realloc2();
extern char *strdup2();
extern VOID_P addlist();
extern int toupper2();
extern char *strchr2();
extern char *strrchr2();
extern char *strncpy2();
extern int strncpy3();
extern int strcasecmp2();
extern char *strstr2();
extern int atoi2();
#ifndef	USESETENV
extern int putenv2();
#endif
extern int setenv2();
extern char *getenv2();
extern int printenv();
extern int system2();
extern char *getwd2();
#if	!MSDOS
extern char *getpwuid2();
extern char *getgrgid2();
#endif
extern time_t timelocal2();

/* input.c */
extern int getkey2();
extern char *inputstr();
#if	MSDOS
extern int yesno(const char *, ...);
#else
extern int yesno();
#endif
extern VOID warning();
extern int selectstr();

/* shell.c */
extern char *evalcommand();
extern int execmacro();
extern int execenv();
extern int execshell();
extern int execusercomm();
extern char **entryhist();
extern char **loadhistory();
extern int savehistory();
#ifndef	_NOCOMPLETE
extern int completealias();
extern int completeuserfunc();
#endif

/* info.c */
extern VOID help();
extern long getblocksize();
extern int writablefs();
extern char *inscomma();
extern VOID getinfofs();
extern int infofs();

/* kanji.c */
extern int onkanji1();
#if	(!MSDOS && !defined (_NOKANJICONV)) || !defined (_NOENGMES)
extern int getlang();
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
extern int jis7();
extern int sjis2ujis();
extern int ujis2sjis();
extern int kanjiconv();
#endif
extern int kanjiputs();
#if	MSDOS
extern int kanjiprintf(const char *, ...);
#else
extern int kanjiprintf();
#endif
extern int kanjiputs2();

/* file.c */
extern int logical_access();
extern int getstatus();
extern char *putmode();
extern int isdotdir();
extern int cmplist();
#ifndef	_NOTREE
extern int cmptree();
#endif
extern struct dirent *searchdir();
extern int underhome();
extern int copyfile();
extern int movefile();
extern int mktmpdir();
extern int rmtmpdir();
extern VOID removetmp();
extern int forcecleandir();
#ifndef	_NOWRITEFS
extern VOID arrangedir();
#endif

/* apply.c */
extern int _cpfile();
extern int cpfile();
extern int mvfile();
extern int cpdir();
extern int findfile();
extern int finddir();
extern int inputattr();
extern int setattr();
extern int applyfile();
extern int applydir();

/* archive.c */
#ifndef	_NOARCHIVE
extern VOID rewritearc();
extern int launcher();
extern int pack();
extern int unpack();
extern char *tmpunpack();
extern int backup();
#endif

/* tree.c */
#ifndef	_NOTREE
extern char *tree();
#endif

/* command.c */

/* browse.c */
extern VOID helpbar();
extern VOID statusbar();
extern VOID sizebar();
extern VOID infobar();
extern VOID waitmes();
extern int calcwidth();
extern VOID putname();
extern int listupfile();
extern VOID rewritefile();
extern VOID movepos();
extern VOID main_fd();
