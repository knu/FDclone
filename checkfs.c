/*
 *	checkfs.c
 *
 *	File System Checker
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "machine.h"

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include <sys/file.h>
#include <sys/param.h>

#ifdef	USEDIRECT
#include <sys/dir.h>
#define	dirent	direct
#else
#include <dirent.h>
#endif

#ifdef	NOVOID
#define VOID
#else
typedef void	VOID;
#endif

#ifdef	NOERRNO
extern int errno;
#endif

#define	MAXFILE		64
#define	MAXNAME		64

#ifdef	USERAND48
#define	random		lrand48
#define	srandom		srand48
#endif

typedef struct _strlist {
	char *str;
	struct _strlist *next;
} strlist;

static VOID error();
static VOID touch();
static char *randomname();
static VOID touchfiles();
static VOID shufflefiles();
static int _cleandir();
static int cleandir();
static int getblocksize();
static int getdirsize();
static char *getentnum();
static char *maketmp();
static char *maketmplen();
static VOID checkboundary();
static int getdirent();
static int getnamlen();
static VOID checkreplace();
static strlist *fillspace();
static VOID writedir();
static VOID checkdir();
static VOID cmpdir();
static int cmpfile();

char *progname;
char *filelist[MAXFILE];
int blocksize;
int boundary;
int dirsiz;
int ptrsiz;
int headbyte;
int nameoffset;


static VOID error(str)
char *str;
{
	if (!str) str = progname;
	perror(str);
	exit(-1);
}

static VOID touch(file)
char *file;
{
	FILE *fp;

	if (!(fp = fopen(file, "w"))) error(file);
	fclose(fp);
}

static char *randomname()
{
	int i, len;
	char *buf;

	len = (random() % MAXNAME) + 1;
	if (!(buf = (char *)malloc(len + 1))) error(NULL);

	for (i = 0; i < len; i++)
		buf[i] = (random() % 26) + 'a';
	buf[i] = '\0';

	return(buf);
}

static VOID touchfiles()
{
	int i, j;
	char *fn;

	for (i = 0; i < MAXFILE; i++) {
		do {
			fn = randomname();
			for (j = 0; j < i; j++)
				if (!strcmp(fn, filelist[j])) {
					free(fn);
					fn = NULL;
					break;
				}
		} while (!fn);
		touch(fn);
		filelist[i] = fn;
	}
}

static VOID shufflefiles(n)
int n;
{
	int i, j, r;
	char *fn;

	for (i = 0; i < n; i++) {
		r = random() % MAXFILE;
		do {
			fn = randomname();
			for (j = 0; j < MAXFILE; j++)
				if (!strcmp(fn, filelist[j])) {
					free(fn);
					fn = NULL;
					break;
				}
		} while (!fn);
		if (rename(filelist[r], fn) < 0) error(fn);
		free(filelist[r]);
		filelist[r] = fn;
	}
}

static int _cleandir(dir)
char *dir;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char path[MAXPATHLEN + 1];

	if (!(dirp = opendir(dir))) return(-1);

	while (dp = readdir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;

		strcpy(path, dir);
		strcat(path, "/");
		strcat(path, dp -> d_name);

		if (lstat(path, &status) < 0) error(path);
		if ((status.st_mode & S_IFMT) == S_IFDIR) _cleandir(path);
		else if (unlink(path) < 0) error(path);
	}
	closedir(dirp);
	if (rmdir(dir) < 0) error(dir);
	return(0);
}

static int cleandir(dir)
char *dir;
{
	_cleandir(dir);
	if (mkdir(dir, 0777)) error(dir);
	return(0);
}

static int getblocksize(dir)
char *dir;
{
#ifdef	DEV_BSIZE
	return(DEV_BSIZE);
#else
	struct stat buf;

	cleandir(dir);
	if (stat(dir, &buf) < 0) error(dir);
	return((int)buf.st_size);
#endif
}

static int getdirsize(dir)
char *dir;
{
	struct stat status;

	if (lstat(dir, &status) < 0) error(dir);
	return(status.st_size);
}

static char *getentnum(dir)
char *dir;
{
	FILE *fp;
	char *tmp, *buf;
	int i, n;

	n = getdirsize(dir) / blocksize;
	if (!(tmp = (char *)malloc(n + 1))) error(NULL);
	if (headbyte > 0) {
		if (!(buf = (char *)malloc(headbyte))) error(NULL);
		fp = fopen(dir, "r");
		for (i = 0; i < n; i++) {
			fseek(fp, i * blocksize, 0);
			if (fread(buf, 1, headbyte, fp) < headbyte) error(dir);
			tmp[i] = buf[3] + 1;
		}
		fclose(fp);
		free(buf);
	}
	else for (i = 0; i < n; i++) tmp[i] = 0;
	return(tmp);
}

static char *maketmp(path, p, n)
char *path;
int p, n;
{
	int i;

	i = n / (10 + 'Z' - 'A' + 1 + 'z' - 'a' + 1);
	if (i < 10) path[p] = i + '0';
	else if ((i -= 10) + 'A' <= 'Z') path[p] = i + 'A';
	else if ((i -= 'Z' - 'A' + 1) + 'a' <= 'z') path[p] = i + 'a';
	i = n % (10 + 'Z' - 'A' + 1 + 'z' - 'a' + 1);
	if (i < 10) path[p + 1] = i + '0';
	else if ((i -= 10) + 'A' <= 'Z') path[p + 1] = i + 'A';
	else if ((i -= 'Z' - 'A' + 1) + 'a' <= 'z') path[p + 1] = i + 'a';
	path[p + 2] = '\0';
	return(path);
}

static char *maketmplen(path, p, len, ch)
char *path;
int p, len;
int ch;
{
	int i;

	for (i = 0; i < len; i++) path[p + i] = ch;
	path[p + i] = '\0';
	return(path);
}

static VOID checkboundary(dir)
char *dir;
{
	char path[MAXPATHLEN + 1];
	int i, fnamp, len, ent, max, minent;

	strcpy(path, dir);
	strcat(path, "/");
	fnamp = strlen(path);

	i = 0;
	cleandir(dir);
	do {
		touch(maketmp(path, fnamp, ++i));
	} while (getdirsize(dir) == blocksize);
	max = i - 1;
	minent = blocksize / (max + 2);
	printf("Blocksize: %d, Maxfile: %d, Entsize: %d.%03d\n",
		blocksize, max + 2,
		minent, (blocksize * 1000 / (max + 2)) % 1000);

	cleandir(dir);
	for (i = 0; i < max - 1; i++) touch(maketmp(path, fnamp, i));
	i = 2;
	do {
		touch(maketmplen(path, fnamp, ++i, '_'));
		if (unlink(path) < 0) error(path);
	} while (getdirsize(dir) == blocksize);
	len = i - 1;
	ent = blocksize - minent * (max + 1);
	printf("LastEntChar: %d, LastEntSize: %d\n", len, ent);

	cleandir(dir);
	for (i = 0; i < max - 2; i++) touch(maketmp(path, fnamp, i));
	touch(maketmplen(path, fnamp, len, '_'));
	i = 2;
	do {
		touch(maketmplen(path, fnamp, ++i, '#'));
		if (unlink(path) < 0) error(path);
	} while (getdirsize(dir) == blocksize);

	cleandir(dir);
	touch(maketmplen(path, fnamp, i, '#'));
	for (i = 0; i < max - 2; i++) touch(maketmp(path, fnamp, i));
	i = 0;
	do {
		touch(maketmplen(path, fnamp, ++i, '_'));
		if (unlink(path) < 0) error(path);
	} while (getdirsize(dir) == blocksize);
	boundary = len - i + 1;
	printf("LastEntChar: %d-%d, Boundary: %d\n",
		i, len, boundary);

	for (i = 0; i < boundary; i++) {
		if ((len + i) / boundary == (len - boundary + 1 + i) / boundary)
			 break;
	}
	nameoffset = i;
	dirsiz = minent - ((2 + nameoffset) / boundary + 1) * boundary;
	if (ptrsiz = dirsiz % boundary) dirsiz -= ptrsiz;
	ent = blocksize - getdirent(2) * max - getdirent(1)
		- ptrsiz * (max + 1);
	headbyte = ent - ((len + nameoffset) / boundary + 1) * boundary
		- dirsiz - ptrsiz;
	printf("Ent = ((strlen(filename) + %d) / %d + 1) * %d + %d + %d\n",
		nameoffset, boundary, boundary, dirsiz, ptrsiz);
	printf("Header = %d(bytes)\n", headbyte);
}

static int getdirent(len)
int len;
{
	return(((len + nameoffset) / boundary + 1) * boundary + dirsiz);
}

static int getnamlen(ent)
int ent;
{
	return(((ent - dirsiz) / boundary - 1) * boundary
		+ boundary - 1 - nameoffset);
}

static VOID checkreplace(dir)
char *dir;
{
	DIR *dirp;
	struct dirent *dp;
	char path[MAXPATHLEN + 1];
	int i, fnamp, len, minent, topent, totalent;

	minent = getdirent(1) + ptrsiz;
	totalent = headbyte + getdirent(1) + getdirent(2) + ptrsiz * 2;
	cleandir(dir);
	if (chdir(dir) < 0) error(dir);

	strcpy(path, "tmp");

	for (i = 0;
	totalent + (getdirent(5) + ptrsiz) * 2 + minent < blocksize; i++) {
		touch(maketmp(path, 3, i));
		totalent += getdirent(5) + ptrsiz;
	}
	len = getnamlen(blocksize - totalent - minent - ptrsiz);
	touch(maketmplen(path, 0, len, 'X'));
	strcpy(path, "0/");
	fnamp = strlen(path);

	if (mkdir("0", 0777)) error("0");
	if (!(dirp = opendir("."))) error(NULL);

	topent = i = 0;
	while (dp = readdir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;
		if (!strcmp(dp -> d_name, "0")) topent = i;
		else {
			strcpy(path + fnamp, dp -> d_name);
			if (rename(dp -> d_name, path) < 0) error(path);
		}
		i++;
	}
	closedir(dirp);

	if (topent > 0) if (rename("0", "1") < 0) error("1");

	touch("#");
	i = getdirsize(".");
	printf("Dirsize: %d (%s)\n", i, (i == blocksize) ? "OK" : "NG");
	if (chdir("..") < 0) error(NULL);
}

static strlist *fillspace(dummy, ent, ch)
strlist *dummy;
int ent;
int ch;
{
	strlist *new;
	int len;

	if ((len = getnamlen(ent)) <= 0) return(dummy);

	if (!(new = (strlist *)malloc(sizeof(strlist)))) error(NULL);
	if (!(new -> str = (char *)malloc(len + 1))) error(NULL);
	new -> next = dummy;
	touch(maketmplen(new -> str, 0, len, ch));
	return(new);
}

static VOID writedir()
{
	DIR *dirp;
	struct dirent *dp;
	strlist *strp, *dummy = NULL;
	char *tmpdir, *entnum, path[MAXPATHLEN + 1];
	int i, ch, fnamp, len, ent, block, ptr, totalent, totalptr;

	len = strlen(filelist[0]);
	maketmplen(path, 0, len, 'X');
	if (!(tmpdir = (char *)malloc(strlen(path) + 1))) error(NULL);
	strcpy(tmpdir, path);

	if (mkdir(tmpdir, 0777)) error(tmpdir);
	strcat(path, "/");
	fnamp = strlen(path);

	if (!(dirp = opendir("."))) error(NULL);
	i = 0;
	while (dp = readdir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;
		if (*(dp -> d_name) == *(tmpdir)) ent = i;
		else {
			strcpy(path + fnamp, dp -> d_name);
			if (rename(dp -> d_name, path) < 0) error(path);
		}
		i++;
	}
	closedir(dirp);

	if (ent > 0) {
		maketmplen(path, 0, len, 'Y');
		if (rename(tmpdir, path) < 0) error(path);
		strcpy(tmpdir, path);
		strcat(path, "/");
	}

	entnum = getentnum(".");
	block = ptr = 0;
	totalent = headbyte + getdirent(1) + getdirent(2) + getdirent(len);
	totalptr = entnum[block];
	ptr = 3;
	ch = 'A';
	for (i = 1; i < MAXFILE; i++) {
#ifndef	SVR3FS
		if (((totalptr > ptr + 1) ? totalptr : ptr + 1) * ptrsiz
		+ totalent + getdirent(strlen(filelist[i])) > blocksize) {
			if (totalptr < ptr + 1) totalptr = ptr + 1;
			dummy = fillspace(dummy,
				blocksize - totalent - totalptr * ptrsiz, ch++);
			ptr = 0;
			totalent = headbyte;
			totalptr = entnum[++block];
		}
#endif
		strcpy(path + fnamp, filelist[i]);
		if (rename(path, filelist[i]) < 0) error(path);
		totalent += getdirent(strlen(filelist[i]));
		if (totalptr < ++ptr) totalptr = ptr;
	}
	free(entnum);

	maketmplen(path, 0, len, 'Z');
	if (rename(tmpdir, path) < 0) error(path);
	strcpy(tmpdir, path);
	strcat(path, "/");
	strcpy(path + fnamp, filelist[0]);
	if (rename(path, filelist[0]) < 0) error(path);

	if (rmdir(tmpdir) < 0) error(tmpdir);
	free(tmpdir);
	while (dummy) {
		if (unlink(dummy -> str) < 0) error(dummy -> str);
		free(dummy -> str);
		strp = dummy;
		dummy = strp -> next;
		free(strp);
	}
}

static VOID checkdir(dir)
char *dir;
{
	DIR *dirp;
	struct dirent *dp;
	int i, j;
	char *cp;

	i = 0;
	if (!(dirp = opendir(dir))) error(dir);
	while (dp = readdir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;
		if (i >= MAXFILE) printf("Excess File: %s\n", dp -> d_name);
		else if (strcmp(filelist[i], dp -> d_name)) {
			printf("Replacement on <%s>, <%s>.\n",
				filelist[i], dp -> d_name);
			for (j = i + 1; j < MAXFILE; j++)
				if (!strcmp(filelist[j], dp -> d_name)) break;
			if (j < MAXFILE) {
				cp = filelist[j];
				for (; j > i; j--)
					filelist[j] = filelist[j - 1];
				filelist[i] = cp;
				i++;
			}
		}
		else i++;
	}
	closedir(dirp);

	if (i < MAXFILE) printf("Lost Files: %d\n", MAXFILE - i);
}

static VOID cmpdir(dir)
char *dir;
{
	DIR *dirp;
	struct dirent *dp;
	char prev[MAXNAMLEN + 1];

	prev[0] = '\0';
	if (!(dirp = opendir(dir))) error(dir);
	while (dp = readdir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;
		if (strcmp(dp -> d_name, prev) < 0)
			printf("Replacement on <%s>, <%s>.\n",
				prev, dp -> d_name);
		strcpy(prev, dp -> d_name);
	}
	closedir(dirp);
}

static int cmpfile(file1, file2)
char **file1, **file2;
{
	return(strcmp(*file1, *file2));
}

int main(argc, argv)
int argc;
char *argv[];
{
	long seed;
	char *wdir;
	int i, level;

	if (progname = strrchr(argv[0], '/')) progname++;
	else progname = argv[0];

	seed = time(0);
	level = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') switch (argv[i][1]) {
			case 's':
				i++;
				seed = atoi(argv[i]);
				break;
			case 'l':
				i++;
				level = atoi(argv[i]);
				break;
			default:
				break;
		}
		else break;
	}
	if (i < argc) wdir = argv[i];
	else wdir = "CheckDir";

	printf("#### File System Check on <%s> with seed [%d] ####\n",
		wdir, seed);

	srandom(seed);

	if (level == 0) {
		blocksize = getblocksize(wdir);
		checkboundary(wdir);
	}
	else {
		blocksize = 512;
#ifdef	IRIXFS
		boundary = 2;
		dirsiz = 4;
		ptrsiz = 1;
		headbyte = 4;
#else
		boundary = 4;
		dirsiz = 8;
		ptrsiz = 0;
		headbyte = 0;
#endif
		nameoffset = 0;
	}

	if (level <= 1) checkreplace(wdir);

	if (level <= 5) {
		cleandir(wdir);
		if (chdir(wdir) < 0) error(wdir);

		printf("#### Make Files ####\n");
		touchfiles();
		printf("#### Shuffle Files ####\n");
		shufflefiles(MAXFILE * 2);
	}

	if (level <= 4) {
		qsort(filelist, MAXFILE, sizeof(char *), cmpfile);
		printf("#### Write Dir ####\n");
		writedir();
		if (chdir("..")) error(NULL);
	}

	if (level <= 3) {
		printf("#### Check Dir ####\n");
		checkdir(wdir);
	}

	if (level > 5) {
		printf("#### Compare Dir ####\n");
		cmpdir(wdir);
	}

	for (i = 0; i < MAXFILE; i++) free(filelist[i]);
	exit(0);
}
