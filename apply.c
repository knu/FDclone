/*
 *	apply.c
 *
 *	Apply Function to Files
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/stat.h>

#ifdef	USETIMEH
#include <time.h>
#endif

#ifdef	USEUTIME
#include <utime.h>
#endif

extern int filepos;
extern reg_t *findregexp;

static int judgecopy();
static int _cpfile();
static VOID showattr();
static int touchfile();

int copypolicy = 0;
char *destpath = NULL;

static u_short attrmode;
static time_t attrtime;


static int judgecopy(file, dest, atimep, mtimep)
char *file, *dest;
time_t *atimep, *mtimep;
{
	struct stat status1, status2;
	char *cp, *tmp, *str[4];
	int val[4];

	strcpy(dest, destpath);
	strcat(dest, "/");
	strcat(dest, file);
	if (lstat(file, &status1) < 0) error(file);
	*atimep = status1.st_atime;
	*mtimep = status1.st_mtime;
	if (lstat(dest, &status2) < 0) {
		if (errno != ENOENT) error(dest);
		return((int)status1.st_mode);
	}

	if (!copypolicy || copypolicy == 2) {
		locate(0, LCMDLINE);
		putterm(l_clear);
		putch('[');
		cp = SAMEF_K;
		kanjiputs2(dest, n_column - strlen(cp) - 1, -1);
		kanjiputs(cp);
	}
	if (!copypolicy) {
		str[0] = UPDAT_K;
		str[1] = RENAM_K;
		str[2] = OVERW_K;
		str[3] = NOCPY_K;
		val[0] = 1;
		val[1] = 2;
		val[2] = 3;
		val[3] = 4;
		copypolicy = 1;
		if (selectstr(&copypolicy, 4, 0, str, val) == ESC)
			copypolicy = 0;
	}
	switch (copypolicy) {
		case 1:
			if (status1.st_mtime < status2.st_mtime) return(-1);
			break;
		case 2:
			cp = strrchr(dest, '/') + 1;
			do {
				if (!(tmp = inputstr2(NEWNM_K,
					-1, NULL, NULL))) return(-1);
				strcpy(cp, tmp);
				free(tmp);
			} while (lstat(dest, &status2) >= 0
			&& (putterm(t_bell) || 1));
			if (errno != ENOENT) error(dest);
			break;
		case 3:
			break;
		default:
			return(-1);
			break;
	}
	return((int)status1.st_mode);
}

static int _cpfile(src, dest, mode)
char *src, *dest;
int mode;
{
	char buf[BUFSIZ];
	int i, fd1, fd2;

	if ((mode & S_IFMT) == S_IFLNK) {
		if ((i = readlink(src, buf, BUFSIZ)) < 0) return(-1);
		buf[i] = '\0';
		return(symlink(buf, dest) < 0);
	}
	if ((fd1 = open(src, O_RDONLY, mode)) < 0) return(-1);
	if ((fd2 = open(dest, O_WRONLY|O_CREAT|O_TRUNC, mode)) < 0) {
		close(fd1);
		return(-1);
	}

	for (;;) {
		while ((i = read(fd1, buf, BUFSIZ)) < 0 && errno == EINTR);
		if (i < BUFSIZ) break;
		if (write(fd2, buf, BUFSIZ) < 0 && errno != EINTR) error(dest);
	}
	if (i < 0) error(src);
	if (i > 0 && write(fd2, buf, i) < 0 && errno != EINTR) error(dest);

	close(fd2);
	close(fd1);
	return(0);
}

int cpfile(path)
char *path;
{
	char dest[MAXPATHLEN + 1];
	int mode, atime, mtime;

	if ((mode = judgecopy(path, dest, &atime, &mtime)) < 0) return(0);
	return(_cpfile(path, dest, mode));
}

int mvfile(path)
char *path;
{
	char dest[MAXPATHLEN + 1];
	int mode, atime, mtime;

	if ((mode = judgecopy(path, dest, &atime, &mtime)) < 0) return(0);
	if (rename(path, dest) < 0) {
		if (errno != EXDEV || (mode & S_IFMT) == S_IFDIR) return(-1);
		if (access2(path, W_OK) < 0) return(1);
		if (_cpfile(path, dest, mode) < 0
		|| unlink(path) < 0) return(-1);
		if ((mode & S_IFMT) != S_IFLNK
		&& touchfile(dest, atime, mtime) < 0) return(-1);
	}
	return(0);
}

int cpdir(path)
char *path;
{
	char dest[MAXPATHLEN + 1];

	strcpy(dest, destpath);
	strcat(dest, "/");
	strcat(dest, path);
	if (mkdir(dest, 0777) < 0 && errno != EEXIST) return(-1);
	return(0);
}

int findfile(path)
char *path;
{
	char *cp;

	if (cp = strrchr(path, '/')) cp++;
	else cp = path;

	if (regexp_exec(findregexp, cp)) {
		if (!(strncmp(path, "./", 2))) path += 2;
		locate(0, LCMDLINE);
		putterm(l_clear);
		putch('[');
		kanjiputs2(path, n_column - 2, -1);
		putch(']');
		if (yesno(FOUND_K)) {
			destpath = strdup2(path);
			return(-2);
		}
	}
	return(0);
}

int finddir(path)
char *path;
{
	char *cp;

	if (cp = strrchr(path, '/')) cp++;
	else cp = path;

	if (regexp_exec(findregexp, cp)) {
		if (yesno(FOUND_K)) {
			destpath = strdup2(path);
			return(-2);
		}
	}
	return(0);
}

static VOID showattr(listp, mode, timestr, y)
namelist *listp;
u_short mode;
char timestr[2][9];
int y;
{
	struct tm *tm;
	char buf[12];

	tm = localtime(&(listp -> st_mtim));

	locate(0, y);
	putterm(l_clear);

	locate(0, ++y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	putch('[');
	kanjiputs2(listp -> name, 16, 0);
	putch(']');
	locate(n_column / 2 + 3, y);
	kanjiputs(TOLD_K);
	locate(n_column / 2 + 13, y);
	kanjiputs(TNEW_K);

	locate(0, ++y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	kanjiputs(TMODE_K);
	locate(n_column / 2, y);
	putmode(buf, listp -> st_mode);
	cputs(buf + 1);
	locate(n_column / 2 + 10, y);
	putmode(buf, mode);
	cputs(buf + 1);

	locate(0, ++y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	kanjiputs(TDATE_K);
	locate(n_column / 2, y);
	cprintf("%02d-%02d-%02d",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
	locate(n_column / 2 + 10, y);
	cputs(timestr[0]);

	locate(0, ++y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	kanjiputs(TTIME_K);
	locate(n_column / 2, y);
	cprintf("%02d:%02d:%02d",
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec);
	locate(n_column / 2 + 10, y);
	cputs(timestr[1]);

	locate(0, ++y);
	putterm(l_clear);
}

int inputattr(listp, flag)
namelist *listp;
u_short flag;
{
	struct tm *tm;
	char buf[12], timestr[2][9];
	u_short tmp;
	int i, ch, x, y, yy, ymin, ymax;

	yy = WHEADER;
	while (n_line - yy < 7) yy--;

	attrmode = listp -> st_mode;
	tm = localtime(&(listp -> st_mtim));
	sprintf(timestr[0], "%02d-%02d-%02d",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
	sprintf(timestr[1], "%02d:%02d:%02d",
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec);
	showattr(listp, attrmode, timestr, yy);
	yy += 2;
	y = ymin = (flag & 1) ? 0 : 1;
	ymax = (flag & 2) ? 2 : 0;
	x = 0;

	do {
		locate(n_column / 2 + 10 + x, yy + y);
		tflush();

		keyflush();
		switch (ch = getkey(0)) {
			case CTRL_P:
			case K_UP:
				if (y > ymin) y--;
				else y = ymax;
				if (y && x >= 8) x = 7;
				break;
			case CTRL_N:
			case K_DOWN:
				if (y < ymax) y++;
				else y = ymin;
				if (y && x >= 8) x = 7;
				break;
			case 'a':
			case 'A':
				if (ymin <= 0) x = y = 0;
				break;
			case 'd':
			case 'D':
				if (ymax >= 1) {
					x = 0;
					y = 1;
				}
				break;
			case 't':
			case 'T':
				if (ymax >= 2) {
					x = 0;
					y = 2;
				}
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (!y) break;
				putch(ch);
				timestr[y - 1][x] = ch;
			case CTRL_F:
			case K_RIGHT:
				if (y) {
					if (x >= 7) {
						if (y == 2) break;
						y++;
						x = 0;
					}
					else {
						x++;
						if (!((x + 1) % 3)) x++;
					}
				}
				else if (x < 8) x++;
				break;
			case CTRL_B:
			case K_LEFT:
				if (y) {
					if (x <= 0) {
						if (y == 1) break;
						y--;
						x = 7;
					}
					else {
						if (!(x % 3)) x--;
						x--;
					}
				}
				else if (x > 0) x--;
				break;
			case CTRL_L:
				showattr(listp, attrmode, timestr, yy);
				break;
			case ' ':
				if (y) break;
				tmp = 1;
				for (i = 8; i > x; i--) tmp <<= 1;
				if (!((x + 1) % 3) && (attrmode & tmp)) {
					i = (x * 2) / 3 + 4;
					if (!(attrmode & (tmp << i))) tmp <<= i;
					else attrmode ^= (tmp << i);
				}
				attrmode ^= tmp;
				locate(n_column / 2 + 10, yy + y);
				putmode(buf, attrmode);
				cputs(buf + 1);
				break;
			default:
				break;
		}
	} while (ch != ESC && ch != CR);

	if (ch == ESC) return(0);

	tm -> tm_year = (timestr[0][0] - '0') * 10 + timestr[0][1] - '0';
	if (tm -> tm_year < 70) tm -> tm_year += 100;
	tm -> tm_mon = (timestr[0][3] - '0') * 10 + timestr[0][4] - '0';
	tm -> tm_mon--;
	tm -> tm_mday = (timestr[0][6] - '0') * 10 + timestr[0][7] - '0';
	tm -> tm_hour = (timestr[1][0] - '0') * 10 + timestr[1][1] - '0';
	tm -> tm_min = (timestr[1][3] - '0') * 10 + timestr[1][4] - '0';
	tm -> tm_sec = (timestr[1][6] - '0') * 10 + timestr[1][7] - '0';

	if (tm -> tm_mon < 0 || tm -> tm_mon > 11
	|| tm -> tm_mday < 1 || tm -> tm_mday > 31
	|| tm -> tm_hour > 23 || tm -> tm_min > 59 || tm -> tm_sec > 59)
		return(-1);

	attrtime = timelocal2(tm);
	return(1);
}

static int touchfile(path, atime, mtime)
char *path;
time_t atime, mtime;
{
#ifdef	USEUTIME
	struct utimbuf times;

	times.actime = atime;
	times.modtime = mtime;
	return(utime(path, &times));
#else
	struct timeval tvp[2];

	tvp[0].tv_sec = atime;
	tvp[0].tv_usec = 0;
	tvp[1].tv_sec = mtime;
	tvp[1].tv_usec = 0;
	return(utimes(path, tvp));
#endif
}

int setattr(path)
char *path;
{
	struct stat status;
	u_short mode;

	if (lstat(path, &status) < 0) error(path);
	if ((status.st_mode & S_IFMT) == S_IFLNK) return(1);

	mode = (status.st_mode & S_IFMT) | (attrmode & ~S_IFMT);
	if (touchfile(path, status.st_atime, attrtime) < 0
	|| chmod(path, mode) < 0) return(-1);
	return(0);
}

int applyfile(list, max, func, endmes)
namelist *list;
int max;
int (*func)();
char *endmes;
{
	int i, ret, tmp, old;

	ret = tmp = old = filepos;
	for (filepos = 0; filepos < max; filepos++) {
		if (!ismark(&list[filepos])) continue;

		movepos(list, max, old, 0);
		if ((i = (*func)(list[filepos].name)) < 0)
			warning(-1, list[filepos].name);
		if (!i && ret == filepos) ret++;

		old = filepos;
	}

	if (endmes) warning(0, endmes);
	filepos = tmp;
	movepos(list, max, old, 0);
	return(ret);
}

int applydir(dir, funcf, funcd1, funcd2, endmes)
char *dir;
int (*funcf)();
int (*funcd1)();
int (*funcd2)();
char *endmes;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat status;
	char path[MAXPATHLEN + 1];
	int i;

	if (!(dirp = opendir(dir))) {
		warning(-1, dir);
		return(-1);
	}

	locate(0, LCMDLINE);
	putterm(l_clear);
	putch('[');
	kanjiputs2(strncmp(dir, "./", 2) ? dir : dir + 2, n_column - 2, -1);
	putch(']');
	tflush();

	if (funcd1 && (i = (*funcd1)(dir)) < 0) {
		if (i == -1) warning(-1, dir);
		return(i);
	}

	while (dp = readdir(dirp)) {
		if (!strcmp(dp -> d_name, ".")
		|| !strcmp(dp -> d_name, "..")) continue;

		strcpy(path, dir);
		strcat(path, "/");
		strcat(path, dp -> d_name);

		if (lstat(path, &status) < 0) warning(-1, path);
		if ((status.st_mode & S_IFMT) == S_IFDIR) {
			if ((i = applydir(path,
				funcf, funcd1, funcd2, NULL)) < -1) return(i);
		}
		else if ((i = (*funcf)(path)) < 0) {
			if (i == -1) warning(-1, path);
			else return(i);
		}
	}
	closedir(dirp);

	if (funcd2 && (i = (*funcd2)(dir)) == -1) warning(-1, dir);
	locate(0, LCMDLINE);
	putterm(l_clear);
	tflush();
	if (endmes) warning(0, endmes);
	return(i);
}
