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

#ifdef	USEDIRECT
#include <sys/dir.h>
#else
#include <dirent.h>
#endif

/* main.c */
extern VOID error();
extern VOID sigvecset();
extern VOID sigvecreset();
extern VOID title();
extern int evalconfigline();
extern int printmacro();
extern int printlaunch();
extern int printarch();
extern int printalias();
extern int printdrive();
extern int printhist();
extern VOID evalenv();

/* dosemu.c */
extern int _dospath();
extern int dospath();
extern DIR *Xopendir();
extern int Xclosedir();
extern struct dirent *Xreaddir();
extern int Xchdir();
#ifdef	USEGETWD
extern char *Xgetwd();
#else
extern char *Xgetcwd();
#endif
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
extern int Xlseek();
extern int Xmkdir();
extern int Xrmdir();
extern FILE *Xfopen();
extern int Xfclose();
extern int Xfread();
extern int Xfwrite();
extern int Xfflush();
extern int Xfgetc();
extern int Xfputc();
extern char *Xfgets();
extern int Xfputs();
extern char *tmpdosdupl();
extern int tmpdosrestore();

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
#ifdef	NOSTRSTR
extern char *strstr();
#endif
extern int atoi2();
#ifndef	USESETENV
extern int putenv2();
#endif
extern int setenv2();
extern char *getenv2();
extern int printenv();
extern int system2();
extern int system3();
extern char *getwd2();
extern char *getpwuid2();
extern char *getgrgid2();
extern time_t timelocal2();

/* input.c */
extern int getkey2();
extern char *inputstr();
extern int yesno();
extern VOID warning();
extern int selectstr();

/* shell.c */
extern char *evalcommand();
extern int execmacro();
extern int execenv();
extern int execshell();
extern char **entryhist();
extern char **loadhistory();
extern int savehistory();
extern int execinternal();
extern VOID adjustpath();
extern char *evalalias();
extern int completealias();

/* info.c */
extern VOID help();
extern int getblocksize();
extern int writablefs();
extern int infofs();

/* kanji.c */
extern int getlang();
extern int onkanji1();
extern char *mesconv();
extern int kanjiconv();
extern int kanjiputs();
extern int kanjiprintf();
extern int kanjiputs2();

/* file.c */
extern int getstatus();
extern char *putmode();
extern int cmplist();
extern int cmptree();
extern struct dirent *searchdir();
extern int underhome();
extern int copyfile();
extern int movefile();
extern int mktmpdir();
extern int rmtmpdir();
extern int forcecleandir();
extern VOID arrangedir();

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
extern VOID rewritearc();
extern int launcher();
extern int pack();
extern int unpack();
extern char *tmpunpack();
extern VOID removetmp();
extern int backup();

/* tree.c */
extern char *tree();

/* command.c */

/* browse.c */
extern VOID helpbar();
extern VOID statusbar();
extern VOID infobar();
extern VOID waitmes();
extern int calcwidth();
extern VOID putname();
extern int listupfile();
extern VOID rewritefile();
extern VOID movepos();
extern VOID main_fd();
