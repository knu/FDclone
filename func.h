/*
 *	func.h
 *
 *	Function Prototype Declaration
 */

#ifdef	NOERRNO
extern int errno;
#endif

#ifdef	USEDIRECT
#include <sys/dir.h>
#else
#include <dirent.h>
#endif

/* main.c */
extern VOID error();
extern VOID usage();
extern VOID sigset();
extern VOID sigreset();
extern VOID title();
extern int evalconfigline();
extern int printmacro();
extern int printlaunch();
extern int printarch();

/* libc.c */
extern int access2();
extern int unlink2();
extern int rmdir2();
extern int rename2();
extern int stat2();
extern int chdir2();
extern int mkdir2();
extern int mkdir3();
extern VOID_P malloc2();
extern VOID_P realloc2();
extern char *strdup2();
extern VOID_P addlist();
extern int toupper2();
extern char *strchr2();
extern char *strrchr2();
extern char *strncpy2();
extern char *strncpy3();
#ifdef	NOSTRSTR
extern char *strstr();
#endif
extern int atoi2();
#ifdef	NOPUTENV
extern int putenv();
#endif
extern int setenv2();
extern char *getenv2();
extern int printenv();
extern int system2();
extern char *getwd2();
extern char *getpwuid2();
extern char *getgrgid2();
extern time_t timelocal2();

/* input.c */
extern int inputstr();
extern char *inputstr2();
extern int yesno();
extern VOID warning();
extern int selectstr();

/* shell.c */
extern char *evalcommand();
extern int execmacro();
extern int execenv();
extern char **entryhist();
extern int execinternal();

/* info.c */
extern VOID help();
extern int writablefs();
extern int infofs();

/* kanji.c */
extern int onkanji1();
extern char *mesconv();
extern int kanjiputs();
extern int kanjiprintf();
extern int kanjiputs2();

/* file.c */
extern VOID getstatus();
extern char *putmode();
extern int cmplist();
extern struct dirent *searchdir();
extern int underhome();
extern int copyfile();
extern int movefile();
extern VOID arrangedir();

/* apply.c */
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
extern int calcwidth();
extern int listupfile();
extern VOID rewritefile();
extern VOID movepos();
extern VOID main_fd();
