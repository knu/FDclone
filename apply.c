/*
 *	apply.c
 *
 *	Apply Function to Files
 */

#include <signal.h>
#include <fcntl.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"

#if	!MSDOS
#include <sys/file.h>
#include <sys/param.h>
#endif

typedef struct _attr_t {
	u_short mode;
#ifdef	HAVEFLAGS
	u_long flags;
#endif
	char timestr[2][9];
} attr_t;

#ifndef	_NODOSDRIVE
extern int preparedrv __P_((int));
extern int shutdrv __P_((int));
#endif

extern reg_t *findregexp;
extern int subwindow;
extern int win_x;
extern int win_y;
extern int lcmdline;
extern int sizeinfo;
extern int mark;
#ifdef	HAVEFLAGS
extern u_long flaglist[];
#endif
#if	!MSDOS && !defined (_NOEXTRACOPY)
extern int inheritcopy;
#endif

#ifndef	EISDIR
#define	EISDIR	EACCES
#endif

#ifdef	_NOEXTRACOPY
#define	MAXCOPYITEM	4
#define	FLAG_SAMEDIR	0
#else
#define	MAXCOPYITEM	5
#define	FLAG_SAMEDIR	8
#endif

static int NEAR issamedir __P_((char *, char *));
static int NEAR islowerdir __P_((char *, char *));
static char *NEAR getdestdir __P_((char *, char *));
static int NEAR getdestpath __P_((char *, char *, struct stat *));
static int NEAR checkdupl __P_((char *, char *, struct stat *, struct stat *));
static int NEAR checkrmv __P_((char *, int));
static int safecopy __P_((char *));
static int safemove __P_((char *));
static int cpdir __P_((char *));
static int touchdir __P_((char *));
#ifndef	_NOEXTRACOPY
static int mvdir __P_((char *));
#endif
static VOID NEAR showattr __P_((namelist *, attr_t *, int y));
static char **NEAR getdirtree __P_((char *, char **, int *, int));
static int NEAR _applydir __P_((char *, int (*)(char *),
		int (*)(char *), int (*)(char *), int, char *));

int copypolicy = 0;
int removepolicy = 0;
char *destpath = NULL;

static u_short attrmode = 0;
#ifdef	HAVEFLAGS
static u_long attrflags = 0;
#endif
static time_t attrtime = 0;
static char *destdir = NULL;
static time_t destmtime = (time_t)-1;
static time_t destatime = (time_t)-1;
#ifndef	_NOEXTRACOPY
static char *forwardpath = NULL;
#endif
#ifndef	_NODOSDRIVE
static int destdrive = -1;
#endif
#if	!defined (_NOEXTRACOPY) && !defined (_NODOSDRIVE)
static int forwarddrive = -1;
#endif


static int NEAR issamedir(path, org)
char *path, *org;
{
	char *cwd;
	int i;

	cwd = getwd2();
	if (!org) org = cwd;
	else if (_chdir2(org) >= 0) {
		org = getwd2();
		if (_chdir2(cwd) < 0) error(cwd);
	}
	else {
		free(cwd);
		return(0);
	}

#if	MSDOS || !defined (_NODOSDRIVE)
	if (dospath(path, NULL) != dospath(org, NULL)) path = NULL;
	else
#endif
	path = (_chdir2(path) >= 0) ? getwd2() : NULL;
	if (_chdir2(cwd) < 0) error(cwd);
	i = (path) ? !strpathcmp(org, path) : 0;
	if (org != cwd) free(org);
	free(cwd);
	if (path) free(path);
	return(i);
}

static int NEAR islowerdir(path, org)
char *path, *org;
{
	char *cp, *top, *cwd, tmp[MAXPATHLEN];
	int i;

	cwd = getwd2();
	if (!org) org = cwd;
	strcpy(tmp, path);
	top = tmp;
#if	MSDOS || !defined (_NODOSDRIVE)
	if (dospath(path, NULL) != dospath(org, NULL)) {
		free(cwd);
		return(0);
	}
	if (_dospath(tmp)) top += 2;
#endif
	while (_chdir2(tmp) < 0) {
		if ((cp = strrdelim(top, 0)) && cp > top) *cp = '\0';
		else {
			path = NULL;
			break;
		}
	}
	i = 0;
	if (path) {
		path = getwd2();
		if (_chdir2(cwd) < 0) error(cwd);
		if (_chdir2(org) >= 0) {
			org = getwd2();
			i = strlen(org);
			if (strnpathcmp(org, path, i) || path[i] != _SC_)
				i = 0;
			free(org);
		}
		if (_chdir2(cwd) < 0) error(cwd);
		free(path);
	}
	free(cwd);
	return(i);
}

static char *NEAR getdestdir(mes, arg)
char *mes, *arg;
{
	char *dir;
#ifndef	_NODOSDRIVE
	int drive;
#endif

	if (arg && *arg) dir = strdup2(arg);
	else if (!(dir = inputstr(mes, 1, -1, NULL, 1))) return(NULL);
	else if (!*(dir = evalpath(dir, 1))) {
		dir = realloc2(dir, 2);
		dir[0] = '.';
		dir[1] = '\0';
	}
#if	MSDOS || !defined (_NODOSDRIVE)
	else if (_dospath(dir) && !dir[2]) {
		dir = realloc2(dir, 4);
		dir[2] = '.';
		dir[3] = '\0';
	}
#endif

#ifndef	_NODOSDRIVE
	destdrive = -1;
	if ((drive = dospath3(dir))
	&& (destdrive = preparedrv(drive)) < 0) {
		warning(-1, dir);
		free(dir);
		return(NULL);
	}
#endif
	if (preparedir(dir) < 0) {
		warning(-1, dir);
		free(dir);
#ifndef	_NODOSDRIVE
		if (destdrive >= 0) shutdrv(destdrive);
#endif
		return(NULL);
	}
	return(dir);
}

static int NEAR getdestpath(file, dest, stp)
char *file, *dest;
struct stat *stp;
{
	char *cp, *tmp;
	int n;

	strcpy(dest, destpath);
	cp = file;
	if (destdir) {
		for (n = 0; (tmp = strdelim(cp, 0)); n++) cp = tmp + 1;
		for (tmp = destdir; n > 0 && (tmp = strdelim(tmp, 0)); tmp++)
			n--;
		if (tmp) *tmp = '\0';
		if (*destdir) strcpy(strcatdelim(dest), destdir);
	}
	strcpy(strcatdelim(dest), cp);

	if (Xlstat(file, stp) < 0) {
		warning(-1, file);
		return(-1);
	}
	return(0);
}

static int NEAR checkdupl(file, dest, stp1, stp2)
char *file, *dest;
struct stat *stp1, *stp2;
{
	char *cp, *tmp, *str[MAXCOPYITEM];
	int i, n, val[MAXCOPYITEM];
#ifndef	_NOEXTRACOPY
	char path[MAXPATHLEN];
#endif
#if	!defined (_NOEXTRACOPY) && !defined (_NODOSDRIVE)
	int dupdestdrive;
#endif

	if (getdestpath(file, dest, stp1) < 0) return(-1);
	if (Xlstat(dest, stp2) < 0) {
		stp2 -> st_mode = S_IWRITE;
		if (errno == ENOENT) return(0);
		warning(-1, dest);
		return(-1);
	}
	if ((stp2 -> st_mode & S_IFMT) != S_IFDIR)
		n = (copypolicy & ~FLAG_SAMEDIR);
	else if ((stp1 -> st_mode & S_IFMT) != S_IFDIR) n = 2;
#ifdef	_NOEXTRACOPY
	else return(1);
#else
	else if (!(copypolicy & FLAG_SAMEDIR)) return(1);
	else {
		n = (copypolicy &= ~FLAG_SAMEDIR);
		copypolicy = 0;
	}
#endif

	for (;;) {
		if (!n || n == 2) {
			locate(0, LCMDLINE);
			putterm(l_clear);
			putch2('[');
			cp = SAMEF_K;
			kanjiputs2(dest, n_column - (int)strlen(cp) - 1, -1);
			kanjiputs(cp);
		}
		if (!n) {
			str[0] = UPDAT_K;
			val[0] = 1;
			str[1] = RENAM_K;
			val[1] = 2;
			str[2] = OVERW_K;
			val[2] = 3;
			str[3] = NOCPY_K;
			val[3] = 4;
#ifndef	_NOEXTRACOPY
			str[4] = FORWD_K;
			val[4] = 5;
#endif
			i = 1;
			if (selectstr(&i, MAXCOPYITEM, 0, str, val) == K_ESC)
				return(-1);
#ifndef	_NOEXTRACOPY
			if (i == 5) {
				str[0] = UPDAT_K;
				str[1] = OVERW_K;
				val[0] = 5;
				val[1] = 6;
				if (selectstr(&i, 2, 0, str, val) == K_ESC)
					continue;
# ifndef	_NODOSDRIVE
				dupdestdrive = destdrive;
# endif

				if (!(tmp = getdestdir(FRWDD_K, NULL)))
					continue;
# ifndef	_NODOSDRIVE
				forwarddrive = destdrive;
				destdrive = dupdestdrive;
# endif
				if (issamedir(tmp, NULL)
				|| issamedir(tmp, destpath)) {
					warning(EINVAL, tmp);
					free(tmp);
					continue;
				}
				forwardpath = tmp;
			}
#endif	/* !_NOEXTRACOPY */
			copypolicy = n = i;
		}
		switch (n) {
			case 1:
				if (stp1 -> st_mtime < stp2 -> st_mtime)
					return(-1);
				return(2);
/*NOTREACHED*/
				break;
			case 2:
				if ((cp = strrdelim(dest, 1))) cp++;
				else cp = dest;
				lcmdline = LINFO - n_line;
				if (!(tmp = inputstr(NEWNM_K, 1,
					-1, NULL, -1))) return(-1);
				strcpy(cp, tmp);
				free(tmp);
				if (Xlstat(dest, stp2) < 0) {
					stp2 -> st_mode = S_IWRITE;
					if (errno == ENOENT) return(0);
					warning(-1, dest);
					return(-1);
				}
				if ((stp1 -> st_mode & S_IFMT) == S_IFDIR
				&& (stp2 -> st_mode & S_IFMT) == S_IFDIR)
					return(1);
				putterm(t_bell);
				break;
			case 3:
				return(2);
/*NOTREACHED*/
				break;
#ifndef	_NOEXTRACOPY
			case 5:
				if (stp1 -> st_mtime < stp2 -> st_mtime)
					return(-1);
/*FALLTHRU*/
			case 6:
				strcatdelim2(path, forwardpath, file);
				cp = strrdelim(path, 0);
				*cp = '\0';
				if (mkdir2(path, 0777) < 0
				&& errno != EEXIST) {
					warning(-1, path);
					return(-1);
				}
				*cp = _SC_;
				if (safemvfile(dest, path, stp2, NULL) < 0) {
					warning(-1, path);
					return(-1);
				}
				return(2);
/*NOTREACHED*/
				break;
#endif	/* !_NOEXTRACOPY */
			default:
				return(-1);
/*NOTREACHED*/
				break;
		}
	}
}

static int NEAR checkrmv(path, mode)
char *path;
int mode;
{
	char *cp, *str[4];
	int val[4];
#if	MSDOS

	if (Xaccess(path, mode) >= 0) return(0);
#else	/* !MSDOS */
	struct stat *stp, st;

# ifndef	_NODOSDRIVE
	if (dospath2(path)) {
		if (Xaccess(path, mode) >= 0) return(0);
		stp = NULL;
	}
	else
# endif
	{
		char *tmp, dir[MAXPATHLEN];

		if (_Xlstat(path, &st) < 0) {
			warning(-1, path);
			return(-1);
		}
		if ((st.st_mode & S_IFMT) == S_IFLNK) return(0);

		if (!(tmp = strrdelim(path, 0))) strcpy(dir, ".");
		else if (tmp == path) strcpy(dir, _SS_);
		else strncpy2(dir, path, tmp - path);

		if (_Xlstat(dir, &st) < 0) {
			warning(-1, dir);
			return(-1);
		}
		stp = &st;
		if (access(path, mode) >= 0) return(0);
	}
#endif	/* !MSDOS */
	if (errno == ENOENT) return(0);
	if (errno != EACCES) {
		warning(-1, path);
		return(-1);
	}
#if	!MSDOS
	if (stp) {
		int duperrno;

		duperrno = errno;
		mode = logical_access(stp -> st_mode,
			stp -> st_uid, stp -> st_gid);
		if (!(mode & F_ISWRI)) {
			errno = duperrno;
			warning(-1, path);
			return(-1);
		}
	}
#endif
	if (removepolicy > 0) return(removepolicy - 2);
	locate(0, LCMDLINE);
	putterm(l_clear);
	putch2('[');
	cp = DELPM_K;
	kanjiputs2(path, n_column - (int)strlen(cp) - 1, -1);
	kanjiputs(cp);
	str[0] = ANYES_K;
	str[1] = ANNO_K;
	str[2] = ANALL_K;
	str[3] = ANKEP_K;
	val[0] = 0;
	val[1] = -1;
	val[2] = 2;
	val[3] = 1;
	if (selectstr(&removepolicy, 4, 0, str, val) == K_ESC)
		removepolicy = -1;
	return((removepolicy > 0) ? removepolicy - 2 : removepolicy);
}

static int safecopy(path)
char *path;
{
	char dest[MAXPATHLEN];
	struct stat st1, st2;

	if (checkdupl(path, dest, &st1, &st2) < 0) return(0);
	if (safecpfile(path, dest, &st1, &st2) < 0) return(-1);
	return(0);
}

static int safemove(path)
char *path;
{
	char dest[MAXPATHLEN];
	struct stat st1, st2;

	if (checkdupl(path, dest, &st1, &st2) < 0) return(0);
	if (checkrmv(path, W_OK) < 0) return(1);
	if (safemvfile(path, dest, &st1, &st2) < 0) return(-1);
	return(0);
}

static int cpdir(path)
char *path;
{
	char dest[MAXPATHLEN];
	struct stat st1, st2;

	switch (checkdupl(path, dest, &st1, &st2)) {
		case 2:
		/* Already exist, but not directory */
			if (Xunlink(dest) < 0) return(-1);
/*FALLTHRU*/
		case 0:
		/* Not exist */
#if	MSDOS
			if (Xmkdir(dest, st1.st_mode | S_IWRITE) < 0)
#else
			if (Xmkdir(dest, st1.st_mode) < 0)
#endif
				return(-1);
			destmtime = st1.st_mtime;
			destatime = st1.st_atime;
			break;
		case -1:
		/* Abandon copy */
			return(-2);
/*NOTREACHED*/
			break;
		default:
		/* Already exist */
			destmtime = st2.st_mtime;
			destatime = st2.st_atime;
			break;
	}
	if (destdir) free(destdir);
	destdir = strdup2(dest + strlen(destpath) + 1);
	return(0);
}

/*ARGSUSED*/
static int touchdir(path)
char *path;
{
#if	MSDOS || !defined (_NOEXTRACOPY)
	char dest[MAXPATHLEN];
	struct stat st;

# if	!MSDOS
	if (!inheritcopy) return(0);
# endif
	if (getdestpath(path, dest, &st) < 0) return(-2);
	if (destmtime != (time_t)-1 || destatime != (time_t)-1) {
		st.st_mtime = destmtime;
		st.st_atime = destatime;
	}
	if (touchfile(dest, &st) < 0) return(-1);
#endif	/* MSDOS || !_NOEXTRACOPY */
	return(0);
}

#ifndef	_NOEXTRACOPY
static int mvdir(path)
char *path;
{
	char dest[MAXPATHLEN];
	struct stat st;

	if (getdestpath(path, dest, &st) < 0) return(-2);
	if (destmtime != (time_t)-1 || destatime != (time_t)-1) {
		st.st_mtime = destmtime;
		st.st_atime = destatime;
	}
	if (touchfile(dest, &st) < 0) return(-1);
	if (checkrmv(path, R_OK | W_OK | X_OK) < 0) return(1);
	return(Xrmdir(path));
}
#endif	/* !_NOEXTRACOPY */

int rmvfile(path)
char *path;
{
	if (checkrmv(path, W_OK) < 0) return(1);
	return(Xunlink(path));
}

int rmvdir(path)
char *path;
{
	if (checkrmv(path, R_OK | W_OK | X_OK) < 0) return(1);
	return(Xrmdir(path));
}

int findfile(path)
char *path;
{
	char *cp;

	if ((cp = strrdelim(path, 1))) cp++;
	else cp = path;

	if (regexp_exec(findregexp, cp, 1)) {
		if (path[0] == '.' && path[1] == _SC_) path += 2;
		locate(0, LCMDLINE);
		putterm(l_clear);
		putch2('[');
		kanjiputs2(path, n_column - 2, -1);
		putch2(']');
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

	if ((cp = strrdelim(path, 1))) cp++;
	else cp = path;

	if (regexp_exec(findregexp, cp, 1)) {
		if (yesno(FOUND_K)) {
			destpath = strdup2(path);
			return(-2);
		}
	}
	return(0);
}

static VOID NEAR showattr(listp, attr, y)
namelist *listp;
attr_t *attr;
int y;
{
	struct tm *tm;
	char buf[WMODE + 1];

	tm = localtime(&(listp -> st_mtim));

	locate(0, y);
	putterm(l_clear);

	locate(0, ++y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	putch2('[');
	kanjiputs2(listp -> name, 16, 0);
	putch2(']');
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
	cputs2(buf + 1);
	locate(n_column / 2 + 10, y);
	putmode(buf, attr -> mode);
	cputs2(buf + 1);

#ifdef	HAVEFLAGS
	locate(0, ++y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	kanjiputs(TFLAG_K);
	locate(n_column / 2, y);
	putflags(buf, listp -> st_flags);
	cputs2(buf);
	locate(n_column / 2 + 10, y);
	putflags(buf, attr -> flags);
	cputs2(buf);
#endif

	locate(0, ++y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	kanjiputs(TDATE_K);
	locate(n_column / 2, y);
	cprintf2("%02d-%02d-%02d",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
	locate(n_column / 2 + 10, y);
	cputs2(attr -> timestr[0]);

	locate(0, ++y);
	putterm(l_clear);
	locate(n_column / 2 - 20, y);
	kanjiputs(TTIME_K);
	locate(n_column / 2, y);
	cprintf2("%02d:%02d:%02d",
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec);
	locate(n_column / 2 + 10, y);
	cputs2(attr -> timestr[1]);

	locate(0, ++y);
	putterm(l_clear);
}

int inputattr(listp, flag)
namelist *listp;
u_short flag;
{
	struct tm *tm;
	attr_t attr;
	char buf[WMODE + 1];
	u_short tmp;
	int i, ch, x, y, yy, ymin, ymax, dupwin_x, dupwin_y;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif

	yy = LFILETOP;
#ifndef	_NOSPLITWIN
	while (n_line - yy < WMODELINE + 6) yy--;
#endif

	attr.mode = listp -> st_mode;
#ifdef	HAVEFLAGS
	attr.flags = listp -> st_flags;
#endif
	tm = localtime(&(listp -> st_mtim));
	sprintf(attr.timestr[0], "%02d-%02d-%02d",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
	sprintf(attr.timestr[1], "%02d:%02d:%02d",
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec);
	showattr(listp, &attr, yy);
	y = ymin = (flag & 1) ? 0 : WMODELINE;
	ymax = (flag & 2) ? WMODELINE + 1 : 0;
	x = 0;

	do {
		win_x = n_column / 2 + 10 + x;
		win_y = yy + y + 2;
		locate(win_x, win_y);
		tflush();

		keyflush();
		switch (ch = Xgetkey(SIGALRM)) {
			case K_UP:
				if (y > ymin) y--;
				else y = ymax;
				if (y && x >= 8) x = 7;
				if (!y && x >= WMODE - 1) x = WMODE - 2;
				break;
			case K_DOWN:
				if (y < ymax) y++;
				else y = ymin;
				if (y && x >= 8) x = 7;
				if (!y && x >= WMODE - 1) x = WMODE - 2;
				break;
			case 'a':
			case 'A':
				if (ymin <= 0) x = y = 0;
				break;
			case 'd':
			case 'D':
				if (ymax >= WMODELINE) {
					x = 0;
					y = WMODELINE;
				}
				break;
			case 't':
			case 'T':
				if (ymax >= WMODELINE + 1) {
					x = 0;
					y = WMODELINE + 1;
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
				if (y < WMODELINE) break;
				putch2(ch);
				attr.timestr[y - WMODELINE][x] = ch;
/*FALLTHRU*/
			case K_RIGHT:
#ifdef	HAVEFLAGS
				if (y == WMODELINE - 1) {
					if (x < 7) x++;
				}
				else
#endif
				if (y) {
					if (x >= 7) {
						if (y == WMODELINE + 1) break;
						y++;
						x = 0;
					}
					else {
						x++;
						if (!((x + 1) % 3)) x++;
					}
				}
				else if (x < WMODE - 2) x++;
				break;
			case K_LEFT:
#ifdef	HAVEFLAGS
				if (y == WMODELINE - 1) {
					if (x > 0) x--;
				}
				else
#endif
				if (y) {
					if (x <= 0) {
						if (y == WMODELINE) break;
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
			case CTRL('L'):
				showattr(listp, &attr, yy);
				break;
			case ' ':
#ifdef	HAVEFLAGS
				if (y == WMODELINE - 1) {
					attr.flags ^= flaglist[x];
					locate(n_column / 2 + 10, yy + y + 2);
					putflags(buf, attr.flags);
					cputs2(buf);
					break;
				}
#endif
				if (y) break;
#if	MSDOS
				if (x == 2) break;
				else if (x == 3) attr.mode ^= S_ISVTX;
				else {
					tmp = 1;
					for (i = 8; i > x; i--) tmp <<= 1;
					attr.mode ^= tmp;
				}
#else
				tmp = 1;
				for (i = 8; i > x; i--) tmp <<= 1;
				if (!((x + 1) % 3) && (attr.mode & tmp)) {
					i = (x * 2) / 3 + 4;
					if (!(attr.mode & (tmp << i)))
						tmp <<= i;
					else attr.mode ^= (tmp << i);
				}
				attr.mode ^= tmp;
#endif
				locate(n_column / 2 + 10, yy + y + 2);
				putmode(buf, attr.mode);
				cputs2(buf + 1);
				break;
			default:
				break;
		}
	} while (ch != K_ESC && ch != K_CR);

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;
#ifndef	_NOEDITMODE
	Xgetkey(-1);
#endif

	if (ch == K_ESC) return(0);

	tm -> tm_year = (attr.timestr[0][0] - '0') * 10
		+ attr.timestr[0][1] - '0';
	if (tm -> tm_year < 70) tm -> tm_year += 100;
	tm -> tm_mon = (attr.timestr[0][3] - '0') * 10
		+ attr.timestr[0][4] - '0';
	tm -> tm_mon--;
	tm -> tm_mday = (attr.timestr[0][6] - '0') * 10
		+ attr.timestr[0][7] - '0';
	tm -> tm_hour = (attr.timestr[1][0] - '0') * 10
		+ attr.timestr[1][1] - '0';
	tm -> tm_min = (attr.timestr[1][3] - '0') * 10
		+ attr.timestr[1][4] - '0';
	tm -> tm_sec = (attr.timestr[1][6] - '0') * 10
		+ attr.timestr[1][7] - '0';

	if (tm -> tm_mon < 0 || tm -> tm_mon > 11
	|| tm -> tm_mday < 1 || tm -> tm_mday > 31
	|| tm -> tm_hour > 23 || tm -> tm_min > 59 || tm -> tm_sec > 59)
		return(-1);

	attrmode = (flag & 1) ? attr.mode : 0xffff;
#ifdef	HAVEFLAGS
	attrflags = (flag & 1) ? attr.flags : 0xffffffff;
#endif
	attrtime = (flag & 2) ? timelocal2(tm) : (time_t)-1;
	if (flag == 3) {
		if (attrmode == listp -> st_mode) attrmode = 0xffff;
#ifdef	HAVEFLAGS
		if (attrflags == listp -> st_flags) attrflags = 0xffffffff;
#endif
		if (attrtime == listp -> st_mtim) attrtime = (time_t)-1;
	}
	return(1);
}

int setattr(path)
char *path;
{
	struct stat st;

	st.st_mtime = attrtime;
#ifdef	HAVEFLAGS
	st.st_flags = attrflags;
#endif
	st.st_mode = attrmode;
#if	!MSDOS
	st.st_gid = (gid_t)-1;
#endif
	return(touchfile(path, &st));
}

int applyfile(func, endmes)
int (*func)__P_((char *));
char *endmes;
{
	int i, ret, old, dupfilepos;

	dupfilepos = filepos;
	ret = old = filepos;
	helpbar();
	for (filepos = 0; filepos < maxfile; filepos++) {
		if (intrkey()) {
			endmes = NULL;
			break;
		}
		if (!ismark(&(filelist[filepos]))) continue;

		movepos(old, 0);
		locate(win_x, win_y);
		tflush();
		if ((i = (*func)(filelist[filepos].name)) < 0)
			warning(-1, filelist[filepos].name);
		if (!i && ret == filepos) ret++;

		old = filepos;
	}

	if (endmes) warning(0, endmes);
	filepos = dupfilepos;
	movepos(old, 0);
	locate(win_x, win_y);
	tflush();
	return(ret);
}

static char **NEAR getdirtree(dir, dirlist, maxp, depth)
char *dir, **dirlist;
int *maxp, depth;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	char *cp;

	if (!(dirp = Xopendir(dir))) {
		warning(-1, dir);
		if (dirlist) free(dirlist);
		return(NULL);
	}
	cp = strcatdelim(dir);
	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;
		strcpy(cp, dp -> d_name);

		if (Xlstat(dir, &st) >= 0
		&& (st.st_mode & S_IFMT) == S_IFDIR) {
			dirlist = b_realloc(dirlist, *maxp, char *);
			dirlist[(*maxp)++] = strdup2(dir);
			if (!depth || depth > 1) {
				dirlist = getdirtree(dir,
					dirlist, maxp, --depth);
				if (!dirlist) return(NULL);
			}
		}
	}
	Xclosedir(dirp);
	return(dirlist);
}

static int NEAR _applydir(dir, funcf, funcd1, funcd2, order, endmes)
char *dir;
int (*funcf)__P_((char *));
int (*funcd1)__P_((char *));
int (*funcd2)__P_((char *));
int order;
char *endmes;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	time_t dupmtime, dupatime;
	char *cp, *fname, path[MAXPATHLEN], **dirlist;
	int ret, ndir, max;

	if (intrkey()) return(-2);

	locate(0, LCMDLINE);
	putterm(l_clear);
	putch2('[');
	cp = (dir[0] == '.' && dir[1] == _SC_) ? dir + 2 : dir;
	kanjiputs2(cp, n_column - 2, -1);
	putch2(']');
	tflush();

	if (!funcd1 && !funcd2) order = 0;
	fname = strcatdelim2(path, dir, NULL);

	ret = max = 0;
	dirlist = getdirtree(path, NULL, &max, (order == 2) ? 0 : 1);

	destmtime = destatime = (time_t)-1;
	if ((order == 1 || order == 2) && (ret = (*funcd1)(dir)) < 0) {
		if (ret == -1) warning(-1, dir);
		if (dirlist) {
			for (ndir = 0; ndir < max; ndir++) free(dirlist[ndir]);
			free(dirlist);
		}
		return(ret);
	}
	dupmtime = destmtime;
	dupatime = destatime;

	if (order) for (ndir = 0; ndir < max; ndir++) {
		if (order != 2) ret = _applydir(dirlist[ndir], funcf,
				funcd1, funcd2, order, NULL);
		else if ((ret = (*funcd1)(dirlist[ndir])) < 0) {
			if (ret == -1) warning(-1, dir);
			ret = -2;
		}
		else ret = _applydir(dirlist[ndir], funcf,
			funcd1, funcd2, 0, NULL);
		if (ret < -1) {
			for (; ndir < max; ndir++) free(dirlist[ndir]);
			free(dirlist);
			return(ret);
		}
		free(dirlist[ndir]);
		locate(0, LCMDLINE);
		putterm(l_clear);
		putch2('[');
		kanjiputs2(cp, n_column - 2, -1);
		putch2(']');
		tflush();
	}
	if (dirlist) free(dirlist);

	if (!(dirp = Xopendir(dir))) {
		warning(-1, dir);
		return(-1);
	}
	while ((dp = Xreaddir(dirp))) {
		if (intrkey()) return(-2);
		if (isdotdir(dp -> d_name)) continue;
		strcpy(fname, dp -> d_name);

		if (Xlstat(path, &st) < 0) warning(-1, path);
		else if ((st.st_mode & S_IFMT) == S_IFDIR) continue;
		else if ((ret = (*funcf)(path)) < 0) {
			if (ret == -1) warning(-1, path);
			else return(ret);
		}
	}
	Xclosedir(dirp);

	destmtime = dupmtime;
	destatime = dupatime;
	if (funcd2 && (ret = (*funcd2)(dir)) == -1) warning(-1, dir);
	if (endmes) warning(0, endmes);
	return(ret);
}

int applydir(dir, funcf, funcd1, funcd2, order, endmes)
char *dir;
int (*funcf)__P_((char *));
int (*funcd1)__P_((char *));
int (*funcd2)__P_((char *));
int order;
char *endmes;
{
	char path[MAXPATHLEN];

	if (dir[0] == '.' && dir[1] == '.' && !dir[2]) {
		realpath2(dir, path, 0);
		dir = path;
	}
	return(_applydir(dir, funcf, funcd1, funcd2, order, endmes));
}

/*ARGSUSED*/
int copyfile(arg, tr)
char *arg;
int tr;
{
	if (!mark && isdotdir(filelist[filepos].name)) {
		putterm(t_bell);
		return(0);
	}
#ifdef	_NOTREE
	destpath = getdestdir(COPYD_K, arg);
#else
# ifdef	_NODOSDRIVE
	destpath = (tr) ? tree(1, (int *)1) : getdestdir(COPYD_K, arg);
# else
	destpath = (tr) ? tree(1, &destdrive) : getdestdir(COPYD_K, arg);
# endif
#endif	/* !_NOTREE */

	if (!destpath) return((tr) ? 2 : 1);
	destdir = NULL;
	copypolicy = (issamedir(destpath, NULL)) ? (FLAG_SAMEDIR | 2) : 0;
#ifndef	_NODOSDRIVE
	if (dospath3("")) waitmes();
#endif
	if (mark > 0) applyfile(safecopy, ENDCP_K);
	else if (isdir(&(filelist[filepos]))
	&& !islink(&(filelist[filepos]))) {
#ifdef	_NOEXTRACOPY
		if (copypolicy) {
			warning(EEXIST, filelist[filepos].name);
			free(destpath);
			return((tr) ? 2 : 1);
		}
#endif
		applydir(filelist[filepos].name, safecopy, cpdir, touchdir,
			(islowerdir(destpath, filelist[filepos].name)) ? 2 : 1,
			ENDCP_K);
	}
	else if (safecopy(filelist[filepos].name) < 0)
		warning(-1, filelist[filepos].name);
	free(destpath);
	if (destdir) free(destdir);
#ifndef	_NOEXTRACOPY
	if (forwardpath) free(forwardpath);
	forwardpath = NULL;
#endif
#ifndef	_NODOSDRIVE
	if (destdrive >= 0) shutdrv(destdrive);
#endif
#if	!defined (_NOEXTRACOPY) && !defined (_NODOSDRIVE)
	if (forwarddrive >= 0) shutdrv(forwarddrive);
	forwarddrive = -1;
#endif
	return(4);
}

/*ARGSUSED*/
int movefile(arg, tr)
char *arg;
int tr;
{
	int i;

	if (!mark && isdotdir(filelist[filepos].name)) {
		putterm(t_bell);
		return(0);
	}
#ifdef	_NOTREE
	destpath = getdestdir(MOVED_K, arg);
#else
# ifdef	_NODOSDRIVE
	destpath = (tr) ? tree(1, (int *)1) : getdestdir(MOVED_K, arg);
# else
	destpath = (tr) ? tree(1, &destdrive) : getdestdir(MOVED_K, arg);
# endif
#endif	/* !_NOTREE */

	if (!destpath || issamedir(destpath, NULL)) return((tr) ? 2 : 1);
	destdir = NULL;
	copypolicy = removepolicy = 0;
	if (mark > 0) filepos = applyfile(safemove, ENDMV_K);
	else if (islowerdir(destpath, filelist[filepos].name))
		warning(EINVAL, filelist[filepos].name);
	else if ((i = safemove(filelist[filepos].name)) < 0) {
#ifdef	_NOEXTRACOPY
		warning(-1, filelist[filepos].name);
#else
		if (errno != EXDEV
		|| !isdir(&(filelist[filepos]))
		|| islink(&(filelist[filepos])))
			warning(-1, filelist[filepos].name);
		else if (applydir(filelist[filepos].name,
			safemove, cpdir, mvdir,
			(islowerdir(destpath, filelist[filepos].name)) ? 2 : 1,
			ENDMV_K) >= 0) filepos++;
#endif
	}
	else if (!i) filepos++;
	if (filepos >= maxfile) filepos = maxfile - 1;
	free(destpath);
	if (destdir) free(destdir);
#ifndef	_NOEXTRACOPY
	if (forwardpath) free(forwardpath);
	forwardpath = NULL;
#endif
#ifndef	_NODOSDRIVE
	if (destdrive >= 0) shutdrv(destdrive);
#endif
#if	!defined (_NOEXTRACOPY) && !defined (_NODOSDRIVE)
	if (forwarddrive >= 0) shutdrv(forwarddrive);
	forwarddrive = -1;
#endif
	return(4);
}
