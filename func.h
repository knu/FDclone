/*
 *	func.h
 *
 *	Function Prototype Declaration
 */

/* main.c */
extern void error();
extern void usage();
extern void title();
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
extern void *malloc2();
extern void *realloc2();
extern char *strdup2();
extern void *addlist();
extern int onkanji1();
extern u_char *strchr2();
extern u_char *strrchr2();
extern char *strncpy2();
extern char *strncpy3();
extern void cputs2();
extern int atoi2();
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
extern void warning();
extern int selectstr();

/* shell.c */
extern char *evalcommand();
extern int execmacro();
extern int execenv();
extern char **entryhist();
extern int execinternal();

/* info.c */
extern void help();
extern int getblocksize();
extern int writablefs();
extern int infofs();

/* file.c */
extern void getstatus();
extern char *putmode();
extern int cmplist();
extern struct dirent *searchdir();
extern int underhome();
extern int copyfile();
extern int movefile();
extern void arrangedir();

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
extern void removetmp();
extern int backup();

/* tree.c */
extern char *tree();

/* command.c */

/* browse.c */
extern void helpbar();
extern void statusbar();
extern int calcwidth();
extern int listupfile();
extern void rewritefile();
extern void movepos();
extern void main_fd();
