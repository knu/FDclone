/*
 *	apply.c
 *
 *	apply function to files
 */

#include <fcntl.h>
#include "fd.h"
#include "func.h"
#include "kanji.h"

#define	MAXTIMESTR	8
#define	ATTRWIDTH	10

typedef struct _attrib_t {
	short nlink;
	u_int mode;
#ifdef	HAVEFLAGS
	u_long flags;
#endif
#ifndef	_NOEXTRAATTR
	u_int mask;
# ifndef	NOUID
	uid_t uid;
	gid_t gid;
# endif
#endif	/* !_NOEXTRAATTR */
	char timestr[2][MAXTIMESTR + 1];
} attrib_t;

extern reg_t *findregexp;
extern int subwindow;
extern int win_x;
extern int win_y;
extern int lcmdline;
extern int maxcmdline;
extern int mark;
#ifdef	HAVEFLAGS
extern u_long fflaglist[];
#endif
#if	!defined (_USEDOSCOPY) && !defined (_NOEXTRACOPY)
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
static int NEAR islowerdir __P_((VOID_A));
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
static VOID NEAR showmode __P_((attrib_t *, int, int));
static VOID NEAR showattr __P_((namelist *, attrib_t *, int));
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
static int NEAR inputuid __P_((attrib_t *, int));
static int NEAR inputgid __P_((attrib_t *, int));
#endif
static char **NEAR getdirtree __P_((char *, char **, int *, int));
static int NEAR _applydir __P_((char *, int (*)(char *),
		int (*)(char *), int (*)(char *), int, char *, int));
static int forcecpfile __P_((char *));
static int forcecpdir __P_((char *));
static int forcetouchdir __P_((char *));

int copypolicy = 0;
int removepolicy = 0;
char *destpath = NULL;

static short attrnlink = 0;
static u_short attrmode = 0;
#ifdef	HAVEFLAGS
static u_long attrflags = 0;
#endif
#ifndef	_NOEXTRAATTR
static u_short attrmask = 0;
# ifndef	NOUID
static uid_t attruid = (uid_t)-1;
static gid_t attrgid = (gid_t)-1;
# endif
#endif	/* !_NOEXTRAATTR */
static time_t attrtime = 0;
static char *destdir = NULL;
static short destnlink = 0;
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

#ifdef	_USEDOSPATH
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

static int NEAR islowerdir(VOID_A)
{
#ifdef	_USEDOSEMU
	char orgpath[MAXPATHLEN];
#endif
	char *cp, *top, *cwd, *path, *org, tmp[MAXPATHLEN];
	int i;

	cwd = getwd2();
	path = destpath;
	org = filelist[filepos].name;
	if (!org) org = cwd;
#ifdef	_USEDOSEMU
	else org = nodospath(orgpath, org);
#endif
	strcpy(tmp, path);
	top = tmp;
#ifdef	_USEDOSPATH
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
	else if (!(dir = inputstr(mes, 1, -1, NULL, HST_PATH))) return(NULL);
	else if (!*(dir = evalpath(dir, 0))) {
		dir = realloc2(dir, 2);
		dir[0] = '.';
		dir[1] = '\0';
	}
#ifdef	_USEDOSPATH
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
	int i, n, ch, val[MAXCOPYITEM];
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
	if (!s_isdir(stp2)) n = (copypolicy & ~FLAG_SAMEDIR);
	else if (!s_isdir(stp1)) n = 2;
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
			Xlocate(0, L_CMDLINE);
			Xputterm(L_CLEAR);
			cp = SAMEF_K;
			i = strlen2(cp) - (sizeof("%.*s") - 1);
			cp = asprintf3(cp, n_column - i, dest);
			Xkanjiputs(cp);
			free(cp);
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
			ch = selectstr(&i, MAXCOPYITEM, 0, str, val);
			if (ch == K_ESC) return(-1);
			else if (ch != K_CR) return(-2);
#ifndef	_NOEXTRACOPY
			if (i == 5) {
				str[0] = UPDAT_K;
				str[1] = OVERW_K;
				val[0] = 5;
				val[1] = 6;
				if (selectstr(&i, 2, 0, str, val) != K_CR)
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
				lcmdline = L_INFO;
				tmp = inputstr(NEWNM_K, 1, -1, NULL, -1);
				if (!tmp) return(-1);
				strcpy(getbasename(dest), tmp);
				free(tmp);
				if (Xlstat(dest, stp2) < 0) {
					stp2 -> st_mode = S_IWRITE;
					if (errno == ENOENT) return(0);
					warning(-1, dest);
					return(-1);
				}
				if (s_isdir(stp1) && s_isdir(stp2)) return(1);
				Xputterm(T_BELL);
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

/*NOTREACHED*/
	return(0);
}

static int NEAR checkrmv(path, mode)
char *path;
int mode;
{
	char *cp, *str[4];
	int ch, len, val[4];
#if	MSDOS

	if (Xaccess(path, mode) >= 0) return(0);
#else	/* !MSDOS */
	struct stat *stp, st;
	char *tmp, dir[MAXPATHLEN];

# ifndef	_NODOSDRIVE
	if (dospath2(path)) {
		if (Xaccess(path, mode) >= 0) return(0);
		stp = NULL;
	}
	else
# endif
	{
		if (nodoslstat(path, &st) < 0) {
			warning(-1, path);
			return(-1);
		}
		if (s_islnk(&st)) return(0);

		if (!(tmp = strrdelim(path, 0))) strcpy(dir, ".");
		else if (tmp == path) strcpy(dir, _SS_);
		else strncpy2(dir, path, tmp - path);

		if (nodoslstat(dir, &st) < 0) {
			warning(-1, dir);
			return(-1);
		}
		if (Xaccess(path, mode) >= 0) return(0);
		stp = &st;
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
# ifdef	NOUID
		mode = logical_access(stp -> st_mode)
# else
		mode = logical_access(stp -> st_mode,
			stp -> st_uid, stp -> st_gid);
# endif
		if (!(mode & F_ISWRI)) {
			errno = duperrno;
			warning(-1, path);
			return(-1);
		}
	}
#endif	/* !MSDOS */
	if (removepolicy > 0) return(removepolicy - 2);
	Xlocate(0, L_CMDLINE);
	Xputterm(L_CLEAR);
	cp = DELPM_K;
	len = strlen2(cp) - (sizeof("%.*s") - 1);
	cp = asprintf3(cp, n_column - len, path);
	Xkanjiputs(cp);
	free(cp);
	str[0] = ANYES_K;
	str[1] = ANNO_K;
	str[2] = ANALL_K;
	str[3] = ANKEP_K;
	val[0] = 0;
	val[1] = -1;
	val[2] = 2;
	val[3] = 1;
	ch = selectstr(&removepolicy, 4, 0, str, val);
	if (ch == K_ESC) removepolicy = -1;
	else if (ch != K_CR) removepolicy = -2;

	return((removepolicy > 0) ? removepolicy - 2 : removepolicy);
}

static int safecopy(path)
char *path;
{
	struct stat st1, st2;
	char dest[MAXPATHLEN];
	int n;

	if ((n = checkdupl(path, dest, &st1, &st2)) < 0)
		return((n < -1) ? -2 : 1);
	if (safecpfile(path, dest, &st1, &st2) < 0) return(-1);

	return(0);
}

static int safemove(path)
char *path;
{
	struct stat st1, st2;
	char dest[MAXPATHLEN];
	int n;

	if ((n = checkdupl(path, dest, &st1, &st2)) < 0
	|| (n = checkrmv(path, W_OK)) < 0)
		return((n < -1) ? -2 : 1);
	if (safemvfile(path, dest, &st1, &st2) < 0) return(-1);

	return(0);
}

static int cpdir(path)
char *path;
{
	struct stat st1, st2;
	char dest[MAXPATHLEN];

	destnlink = (TCH_MODE | TCH_UID | TCH_GID);
	switch (checkdupl(path, dest, &st1, &st2)) {
		case 2:
		/* Already exist, but not directory */
			if (Xunlink(dest) < 0) return(-1);
/*FALLTHRU*/
		case 0:
		/* Not exist */
#if	MSDOS
			if (Xmkdir(dest, (st1.st_mode & 0777) | S_IWRITE) < 0)
#else
			if (Xmkdir(dest, st1.st_mode & 0777) < 0)
#endif
				return(-1);
			destnlink |= (TCH_ATIME | TCH_MTIME);
			destmtime = st1.st_mtime;
			destatime = st1.st_atime;
			break;
		case -1:
		case -2:
		/* Abandon copy */
			return(-2);
/*NOTREACHED*/
			break;
		default:
		/* Already exist */
			destnlink |= (TCH_ATIME | TCH_MTIME);
			destmtime = st2.st_mtime;
			destatime = st2.st_atime;
			break;
	}

	if (destdir) free(destdir);
	destdir = &(dest[strlen(destpath)]);
	while (*destdir == _SC_) destdir++;
	destdir = strdup2(destdir);

	return(0);
}

/*ARGSUSED*/
static int touchdir(path)
char *path;
{
#if	defined (_USEDOSCOPY) || !defined (_NOEXTRACOPY)
	struct stat st;
	char dest[MAXPATHLEN];

# if	!defined (_USEDOSCOPY)
	if (!inheritcopy) return(0);
# endif
	if (getdestpath(path, dest, &st) < 0) return(-2);
	st.st_nlink = destnlink;
	st.st_mtime = destmtime;
	st.st_atime = destatime;
	if (touchfile(dest, &st) < 0) return(-1);
#endif	/* _USEDOSCOPY || !_NOEXTRACOPY */

	return(0);
}

#ifndef	_NOEXTRACOPY
static int mvdir(path)
char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];
	int n;

	if (getdestpath(path, dest, &st) < 0) return(-2);
	st.st_nlink = destnlink;
	st.st_mtime = destmtime;
	st.st_atime = destatime;
	if (touchfile(dest, &st) < 0) return(-1);
	if ((n = checkrmv(path, R_OK | W_OK | X_OK)) < 0)
		return((n < -1) ? -2 : 1);

	return(Xrmdir(path));
}
#endif	/* !_NOEXTRACOPY */

int rmvfile(path)
char *path;
{
	int n;

	if ((n = checkrmv(path, W_OK)) < 0) return((n < -1) ? -2 : 1);

	return(Xunlink(path));
}

int rmvdir(path)
char *path;
{
	int n;

	if ((n = checkrmv(path, R_OK | W_OK | X_OK)) < 0)
		return((n < -1) ? -2 : 1);

	return(Xrmdir(path));
}

int findfile(path)
char *path;
{
	if (regexp_exec(findregexp, getbasename(path), 1)) {
		if (path[0] == '.' && path[1] == _SC_) path += 2;
		Xlocate(0, L_CMDLINE);
		Xputterm(L_CLEAR);
		Xcprintf2("[%.*k]", n_column - 2, path);
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
	if (regexp_exec(findregexp, getbasename(path), 1)) {
		if (yesno(FOUND_K)) {
			destpath = strdup2(path);
			return(-2);
		}
	}

	return(0);
}

static VOID NEAR showmode(attr, x, y)
attrib_t *attr;
int x, y;
{
#ifndef	_NOEXTRAATTR
# if	!MSDOS
	u_int tmp;
# endif
	char mask[WMODE + 1];
	int i;
#endif	/* !_NOEXTRAATTR */
	char buf[WMODE + 1];

	Xlocate(x, y);
	putmode(buf, attr -> mode, 1);
#ifndef	_NOEXTRAATTR
	putmode(mask, attr -> mask, 1);
	for (i = 0; buf[i] && mask[i]; i++) {
		if (mask[i] != '-') buf[i] = '*';
# if	!MSDOS
		else if (!((i + 1) % 3)) {
			tmp = (1 << (12 + 3 - ((i + 1) / 3)));
			if (attr -> mode & tmp) {
				if (buf[i] == '-') buf[i] = '!';
				else buf[i] = 'X';
			}
		}
# endif
	}
#endif	/* !_NOEXTRAATTR */
	Xcputs2(buf);
}

static VOID NEAR showattr(namep, attr, yy)
namelist *namep;
attrib_t *attr;
int yy;
{
	struct tm *tm;
	char buf[WMODE + 1];
	int x1, x2, y, w;

	tm = localtime(&(namep -> st_mtim));
	if (isbestomit()) {
		x1 = n_column / 2 - 16;
		x2 = n_column / 2 - 3;
		w = 12;
	}
	else {
		x1 = n_column / 2 - 20;
		x2 = n_column / 2;
		w = 16;
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
		if (iswellomit()) {
			x1 -= WOWNER / 2;
			x2 -= WOWNER / 2;
		}
#endif
	}
	y = yy;

	Xlocate(0, y);
	Xputterm(L_CLEAR);

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	Xcprintf2("[%-*.*k]", w, w, namep -> name);
	Xlocate(x2 + 3, y);
	Xkanjiputs(TOLD_K);
	Xlocate(x2 + 13, y);
	Xkanjiputs(TNEW_K);

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	Xkanjiputs(TMODE_K);
	Xlocate(x2, y);
	putmode(buf, namep -> st_mode, 1);
	Xcputs2(buf);
	showmode(attr, x2 + ATTRWIDTH, y);

#ifdef	HAVEFLAGS
	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	Xkanjiputs(TFLAG_K);
	Xlocate(x2, y);
	putflags(buf, namep -> st_flags);
	Xcputs2(buf);
	Xlocate(x2 + ATTRWIDTH, y);
	putflags(buf, attr -> flags);
	Xcputs2(buf);
#endif

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	Xkanjiputs(TDATE_K);
	Xlocate(x2, y);
	Xcprintf2("%02d-%02d-%02d",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
	Xlocate(x2 + ATTRWIDTH, y);
	Xcputs2(attr -> timestr[0]);

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	Xkanjiputs(TTIME_K);
	Xlocate(x2, y);
	Xcprintf2("%02d:%02d:%02d",
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec);
	Xlocate(x2 + ATTRWIDTH, y);
	Xcputs2(attr -> timestr[1]);

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);

#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
	if (ishardomit()) return;

	y = yy + 1;
	x2 += 20;
	Xlocate(x2, y++);
	Xkanjiputs(TOWN_K);
	Xlocate(x2, y++);
	putowner(buf, attr -> uid);
	Xputch2('<');
	if (attr -> nlink & TCH_UID) Xputterm(T_STANDOUT);
	Xkanjiputs(buf);
	if (attr -> nlink & TCH_UID) Xputterm(END_STANDOUT);
	Xputch2('>');
# ifdef	HAVEFLAGS
	y++;
# endif
	Xlocate(x2, y++);
	Xkanjiputs(TGRP_K);
	Xlocate(x2, y++);
	putgroup(buf, attr -> gid);
	Xputch2('<');
	if (attr -> nlink & TCH_GID) Xputterm(T_STANDOUT);
	Xkanjiputs(buf);
	if (attr -> nlink & TCH_GID) Xputterm(END_STANDOUT);
	Xputch2('>');
#endif	/* !_NOEXTRAATTR && !NOUID */
}

#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
static int NEAR inputuid(attr, yy)
attrib_t *attr;
int yy;
{
	uidtable *up;
	char *cp, *s, buf[MAXLONGWIDTH + 1];
	uid_t uid;

	up = finduid(attr -> uid, NULL);
	if (up) cp = up -> name;
	else {
		snprintf2(buf, sizeof(buf), "%-d", (int)(attr -> uid));
		cp = buf;
	}

	yy += 2 + WMODELINE + 2;
	lcmdline = yy;
	maxcmdline = 1;
	if (!(s = inputstr(AOWNR_K, 0, -1, cp, HST_USER))) return(-1);
	if ((cp = sscanf2(s, "%-*d%$", sizeof(uid_t), &uid))) /*EMPTY*/;
	else if ((up = finduid(0, s))) uid = up -> uid;
	else {
		lcmdline = yy;
		warning(ENOENT, s);
		free(s);
		return(-1);
	}

	free(s);
	if (uid != attr -> uid) {
		attr -> uid = uid;
		attr -> nlink |= TCH_UID;
	}
	return(0);
}

static int NEAR inputgid(attr, yy)
attrib_t *attr;
int yy;
{
	gidtable *gp;
	char *cp, *s, buf[MAXLONGWIDTH + 1];
	gid_t gid;

	gp = findgid(attr -> gid, NULL);
	if (gp) cp = gp -> name;
	else {
		snprintf2(buf, sizeof(buf), "%-d", (int)(attr -> gid));
		cp = buf;
	}

	yy += 2 + WMODELINE + 2;
	lcmdline = yy;
	maxcmdline = 1;
	if (!(s = inputstr(AGRUP_K, 0, -1, cp, HST_GROUP))) return(-1);
	if ((cp = sscanf2(s, "%-*d%$", sizeof(gid_t), &gid))) /*EMPTY*/;
	else if ((gp = findgid(0, s))) gid = gp -> gid;
	else {
		lcmdline = yy;
		warning(ENOENT, s);
		free(s);
		return(-1);
	}

	free(s);
	if (gid != attr -> gid) {
		attr -> gid = gid;
		attr -> nlink |= TCH_GID;
	}
	return(0);
}
#endif	/* !_NOEXTRAATTR && !NOUID */

int inputattr(namep, flag)
namelist *namep;
int flag;
{
#if	!MSDOS
	u_int tmp;
#endif
#ifdef	HAVEFLAGS
	char buf[WMODE + 1];
#endif
	struct tm *tm;
	attrib_t attr;
	u_int mask;
	int ch, x, y, xx, yy, ymin, ymax, dupwin_x, dupwin_y, excl;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	Xgetkey(-1, 0);

	yy = filetop(win);
	while (yy + WMODELINE + 5 > n_line - 1) yy--;
	if (yy <= L_TITLE) yy = L_TITLE + 1;
	xx = n_column / 2 + ((isbestomit()) ? 7 : ATTRWIDTH);

	excl = (flag & ATR_EXCLUSIVE);
	attr.nlink = TCH_CHANGE;
	if (flag & ATR_MULTIPLE) attr.nlink |= (TCH_MODE | TCH_MTIME);
	attr.mode = namep -> st_mode;
#ifndef	_NOEXTRAATTR
	attr.mode &= ~S_IFMT;
	attr.mask = 0;
# if	!MSDOS
	if ((flag & ATR_RECURSIVE) && excl == ATR_MODEONLY) {
		for (x = 0; x < 3; x++) attr.mode |= (1 << (x + 12));
		attr.nlink |= TCH_MODEEXE;
	}
# endif
# ifndef	NOUID
	attr.uid = namep -> st_uid;
	attr.gid = namep -> st_gid;
# endif
#endif	/* !_NOEXTRAATTR */
#ifdef	HAVEFLAGS
	attr.flags = namep -> st_flags;
#endif
	tm = localtime(&(namep -> st_mtim));
	snprintf2(attr.timestr[0], sizeof(attr.timestr[0]), "%02d-%02d-%02d",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
	snprintf2(attr.timestr[1], sizeof(attr.timestr[1]), "%02d:%02d:%02d",
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec);
	showattr(namep, &attr, yy);
	y = ymin = (excl == ATR_TIMEONLY) ? WMODELINE : 0;
	ymax = (excl == ATR_MODEONLY) ? 0 : WMODELINE + 1;
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
	if (excl == ATR_OWNERONLY) x = ATTRWIDTH + 1;
	else
#endif
	x = 0;

	do {
		win_x = xx + x;
		win_y = yy + y + 2;
		Xlocate(win_x, win_y);
		Xtflush();

		keyflush();
#if	MSDOS
		if (x == 3) mask = S_ISVTX;
		else
#endif
		mask = (1 << (8 - x));
		switch (ch = Xgetkey(1, 0)) {
			case K_UP:
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				if (x > ATTRWIDTH) {
					if (y) y = 0;
					else y = WMODELINE + 1;
					break;
				}
#endif
				if (y > ymin) y--;
				else y = ymax;
				if (y && x >= 8) x = 7;
				if (!y && x >= WMODE - 1) x = WMODE - 2;
				break;
			case K_DOWN:
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				if (x > ATTRWIDTH) {
					if (y) y = 0;
					else y = WMODELINE + 1;
					break;
				}
#endif
				if (y < ymax) y++;
				else y = ymin;
				if (y && x >= 8) x = 7;
				if (!y && x >= WMODE - 1) x = WMODE - 2;
				break;
			case 'a':
			case 'A':
				if (excl && excl != ATR_MODEONLY) break;
				x = y = 0;
				break;
			case 'd':
			case 'D':
				if (excl && excl != ATR_TIMEONLY) break;
				x = 0;
				y = WMODELINE;
				break;
			case 't':
			case 'T':
				if (excl && excl != ATR_TIMEONLY) break;
				x = 0;
				y = WMODELINE + 1;
				break;
#ifdef	HAVEFLAGS
			case 'f':
			case 'F':
				if (excl) break;
				x = 0;
				y = WMODELINE - 1;
				break;
#endif
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
			case 'o':
			case 'O':
				if (excl && excl != ATR_OWNERONLY) break;
				x = ATTRWIDTH + 1;
				y = 0;
				break;
			case 'g':
			case 'G':
				if (excl && excl != ATR_OWNERONLY) break;
				x = ATTRWIDTH + 1;
				y = WMODELINE + 1;
				break;
#endif	/* !_NOEXTRAATTR && !NOUID */
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
				Xputch2(ch);
				attr.timestr[y - WMODELINE][x] = ch;
				attr.nlink |= TCH_MTIME;
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
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
						if (!excl
						&& y == WMODELINE + 1) {
							x = ATTRWIDTH + 1;
							break;
						}
#endif
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
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				else if (!excl) x = ATTRWIDTH + 1;
#endif
				break;
			case K_BS:
				if (y < WMODELINE) break;
/*FALLTHRU*/
			case K_LEFT:
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				if (x > ATTRWIDTH) {
					if (excl || ch == K_BS) break;
					else if (y) x = 7;
					else x = WMODE - 2;
				}
				else
#endif
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
			case K_CTRL('L'):
				yy = filetop(win);
				while (yy + WMODELINE + 5 > n_line - 1) yy--;
				if (yy <= L_TITLE) yy = L_TITLE + 1;
				xx = n_column / 2
					+ ((isbestomit()) ? 7 : ATTRWIDTH);
				showattr(namep, &attr, yy);
				break;
			case ' ':
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				if (x > ATTRWIDTH) {
					if (y) inputgid(&attr, yy);
					else inputuid(&attr, yy);
					showattr(namep, &attr, yy);
					break;
				}
#endif
#ifdef	HAVEFLAGS
				if (y == WMODELINE - 1) {
					attr.flags ^= fflaglist[x];
					Xlocate(xx, yy + y + 2);
					putflags(buf, attr.flags);
					Xcputs2(buf);
					attr.nlink |= TCH_FLAGS;
					break;
				}
#endif
				if (y) break;
#if	MSDOS
				if (x == 2) break;
#else	/* !MSDOS */
				if (!((x + 1) % 3) && (attr.mode & mask)) {
					tmp = (1 << (12 - ((x + 1) / 3)));
# ifndef	_NOEXTRAATTR
					if (flag & ATR_RECURSIVE) tmp <<= 3;
# endif
					if (attr.mode & tmp) {
# ifndef	_NOEXTRAATTR
						if (flag & ATR_RECURSIVE)
							/*EMPTY*/;
						else
# endif
						attr.mode ^= tmp;
					}
# ifndef	_NOEXTRAATTR
					else if (flag & ATR_RECURSIVE) {
						mask = (tmp >> 3);
						if (attr.mode & mask)
							attr.mode ^= tmp;
					}
# endif
					else mask = tmp;
				}
# ifndef	_NOEXTRAATTR
				else {
					tmp = (1 << (15 - ((x + 1) / 3)));
					if (attr.mode & tmp) mask = tmp;
				}
# endif
#endif	/* !MSDOS */
				attr.mode ^= mask;
				showmode(&attr, xx, yy + y + 2);
				attr.nlink |= TCH_MODE;
				break;
#ifndef	_NOEXTRAATTR
			case 'm':
			case 'M':
# ifndef	NOUID
				if (x > ATTRWIDTH) break;
# endif
				if (y || !(flag & ATR_MULTIPLE)) break;
# if	MSDOS
				if (x == 2) break;
# else
				if (!((x + 1) % 3))
					mask |= (1 << (12 - ((x + 1) / 3)));
# endif
				attr.mask ^= mask;
				showmode(&attr, xx, yy + y + 2);
				attr.nlink |= TCH_MASK;
				break;
#endif	/* !_NOEXTRAATTR */
			default:
				break;
		}
	} while (ch != K_ESC && ch != K_CR);

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;
	Xgetkey(-1, 0);

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

	mask = (TCH_CHANGE | TCH_MASK | TCH_MODEEXE);
	switch (excl) {
		case ATR_MODEONLY:
			mask |= TCH_MODE;
			break;
		case ATR_TIMEONLY:
			mask |= TCH_MTIME;
			break;
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
		case ATR_OWNERONLY:
			mask |= (TCH_UID | TCH_GID);
			break;
#endif
		default:
			if (attrmode != namep -> st_mode) mask |= TCH_MODE;
#ifdef	HAVEFLAGS
			if (attrflags != namep -> st_flags) mask |= TCH_FLAGS;
#endif
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
			if (attruid != namep -> st_uid) mask |= TCH_UID;
			if (attrgid != namep -> st_gid) mask |= TCH_GID;
#endif
			if (attrtime != namep -> st_mtim) mask |= TCH_MTIME;
			break;
	}
	attrnlink = (attr.nlink & mask);
	attrmode = attr.mode;
#ifdef	HAVEFLAGS
	attrflags = attr.flags;
#endif
#ifndef	_NOEXTRAATTR
	attrmask = attr.mask;
# ifndef	NOUID
	attruid = attr.uid;
	attrgid = attr.gid;
# endif
#endif	/* !_NOEXTRAATTR */
	attrtime = timelocal2(tm);

	return(1);
}

int setattr(path)
char *path;
{
	struct stat st;

	st.st_nlink = attrnlink;
	st.st_mode = attrmode;
#ifdef	HAVEFLAGS
	st.st_flags = attrflags;
#endif
#ifndef	_NOEXTRAATTR
	st.st_size = (off_t)attrmask;
# ifndef	NOUID
	st.st_uid = attruid;
	st.st_gid = attrgid;
# endif
#endif	/* !_NOEXTRAATTR */
	st.st_mtime = attrtime;

	return(touchfile(path, &st));
}

int applyfile(func, endmes)
int (*func)__P_((char *));
char *endmes;
{
#ifdef	_USEDOSEMU
	char path[MAXPATHLEN];
#endif
	int i, ret, old, dupfilepos;

	dupfilepos = filepos;
	ret = old = filepos;

	if (mark <= 0) {
		i = (*func)(fnodospath(path, filepos));
		if (i == -1) warning(-1, filelist[filepos].name);
		else if (!i) ret++;
		return(ret);
	}

	helpbar();
	for (filepos = 0; filepos < maxfile; filepos++) {
		if (intrkey()) {
			endmes = NULL;
			break;
		}
		if (!ismark(&(filelist[filepos]))) continue;

		movepos(old, 0);
		Xlocate(win_x, win_y);
		Xtflush();
		i = (*func)(fnodospath(path, filepos));
		if (i < -1) {
			endmes = NULL;
			break;
		}
		if (i < 0) warning(-1, filelist[filepos].name);
		else if (!i && ret == filepos) ret++;

		old = filepos;
	}

	if (endmes) warning(0, endmes);
	filepos = dupfilepos;
	movepos(old, 0);
	Xlocate(win_x, win_y);
	Xtflush();

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
	int d;

	if (!(dirp = Xopendir(dir))) return(dirlist);
	cp = strcatdelim(dir);
	while ((dp = Xreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;
		strcpy(cp, dp -> d_name);

		if (Xlstat(dir, &st) < 0 || !s_isdir(&st)) continue;
		dirlist = b_realloc(dirlist, *maxp, char *);
		dirlist[(*maxp)++] = strdup2(dir);
		if (!(d = depth)) /*EMPTY*/;
		else if (d > 1) d--;
		else continue;
		dirlist = getdirtree(dir, dirlist, maxp, d);
	}
	if (cp > dir) *(cp - 1) = '\0';
	Xclosedir(dirp);

	return(dirlist);
}

static int NEAR _applydir(dir, funcf, funcd1, funcd2, order, endmes, verbose)
char *dir;
int (*funcf)__P_((char *));
int (*funcd1)__P_((char *));
int (*funcd2)__P_((char *));
int order;
char *endmes;
int verbose;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	time_t dupmtime, dupatime;
	char *cp, *fname, path[MAXPATHLEN], **dirlist;
	int ret, ndir, max, dupnlink;

	if (intrkey()) return(-2);

	if (verbose) {
		Xlocate(0, L_CMDLINE);
		Xputterm(L_CLEAR);
		cp = (dir[0] == '.' && dir[1] == _SC_) ? dir + 2 : dir;
		Xcprintf2("[%.*k]", n_column - 2, cp);
		Xtflush();
	}
#ifdef	FAKEUNINIT
	else cp = NULL;	/* fake for -Wuninitialized */
#endif

	if (!funcd1) order = (funcd2) ? ORD_NOPREDIR : ORD_NODIR;
	strcpy(path, dir);

	ret = max = 0;
	if (!order) dirlist = NULL;
	else dirlist = getdirtree(path,
		NULL, &max, (order == ORD_LOWER) ? 0 : 1);
	fname = strcatdelim(path);

	destnlink = 0;
	if ((order == ORD_NORMAL || order == ORD_LOWER)
	&& (ret = (*funcd1)(dir)) < 0) {
		if (ret == -1) warning(-1, dir);
		if (dirlist) {
			for (ndir = 0; ndir < max; ndir++) free(dirlist[ndir]);
			free(dirlist);
		}
		return(ret);
	}
	dupnlink = destnlink;
	dupmtime = destmtime;
	dupatime = destatime;

	if (order != ORD_NODIR) for (ndir = 0; ndir < max; ndir++) {
		if (order != ORD_LOWER)
			ret = _applydir(dirlist[ndir], funcf,
				funcd1, funcd2, order, NULL, verbose);
		else if ((ret = (*funcd1)(dirlist[ndir])) < 0) {
			if (ret == -1) warning(-1, dir);
			ret = -2;
		}
		else ret = _applydir(dirlist[ndir], funcf,
			funcd1, funcd2, ORD_NODIR, NULL, verbose);
		if (ret < -1) {
			for (; ndir < max; ndir++) free(dirlist[ndir]);
			free(dirlist);
			return(ret);
		}
		free(dirlist[ndir]);

		if (verbose) {
			Xlocate(0, L_CMDLINE);
			Xputterm(L_CLEAR);
			Xcprintf2("[%.*k]", n_column - 2, cp);
			Xtflush();
		}
	}
	if (dirlist) free(dirlist);

	if (!(dirp = Xopendir(dir))) warning(-1, dir);
	else {
		while ((dp = Xreaddir(dirp))) {
			if (intrkey()) {
				ret = -2;
				break;
			}
			if (isdotdir(dp -> d_name)) continue;
			strcpy(fname, dp -> d_name);

			if (Xlstat(path, &st) < 0) warning(-1, path);
			else if (s_isdir(&st)) continue;
			else if ((ret = (*funcf)(path)) < 0) {
				if (ret < -1) break;
				warning(-1, path);
			}
		}
		Xclosedir(dirp);
		if (ret < -1) return(ret);
	}

	destnlink = dupnlink;
	destmtime = dupmtime;
	destatime = dupatime;
	if (funcd2 && (ret = (*funcd2)(dir)) < 0) {
		if (ret < -1) return(ret);
		warning(-1, dir);
	}
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
	int verbose;

	verbose = 1;
	if (!dir) {
		dir = ".";
		verbose = 0;
	}
	else if (isdotdir(dir) == 1) {
		realpath2(dir, path, 0);
		dir = path;
	}
#ifdef	_USEDOSEMU
	else dir = nodospath(path, dir);
#endif

	return(_applydir(dir, funcf, funcd1, funcd2, order, endmes, verbose));
}

/*ARGSUSED*/
int copyfile(arg, tr)
char *arg;
int tr;
{
	int order;

	if (!mark && isdotdir(filelist[filepos].name)) {
		Xputterm(T_BELL);
		return(FNC_NONE);
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

	if (!destpath) return((tr) ? FNC_UPDATE : FNC_CANCEL);
	destdir = NULL;
	copypolicy = (issamedir(destpath, NULL)) ? (FLAG_SAMEDIR | 2) : 0;
#ifndef	_NODOSDRIVE
	if (dospath3("")) waitmes();
#endif
	if (mark > 0
	|| (!isdir(&(filelist[filepos])) || islink(&(filelist[filepos]))))
		applyfile(safecopy, ENDCP_K);
	else {
#ifdef	_NOEXTRACOPY
		if (copypolicy) {
			warning(EEXIST, filelist[filepos].name);
			free(destpath);
			return((tr) ? FNC_UPDATE : FNC_CANCEL);
		}
#endif
		order = (islowerdir()) ? ORD_LOWER : ORD_NORMAL;
		applydir(filelist[filepos].name, safecopy,
			cpdir, touchdir, order, ENDCP_K);
	}

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

	return(FNC_EFFECT);
}

/*ARGSUSED*/
int movefile(arg, tr)
char *arg;
int tr;
{
#ifdef	_USEDOSEMU
	char path[MAXPATHLEN];
#endif
#ifndef	_NOEXTRACOPY
	int order;
#endif
	int i;

	if (!mark && isdotdir(filelist[filepos].name)) {
		Xputterm(T_BELL);
		return(FNC_NONE);
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

	if (!destpath || issamedir(destpath, NULL))
		return((tr) ? FNC_UPDATE : FNC_CANCEL);
	destdir = NULL;
	copypolicy = removepolicy = 0;
	if (mark > 0) filepos = applyfile(safemove, ENDMV_K);
	else if (islowerdir()) warning(EINVAL, filelist[filepos].name);
	else {
		i = safemove(fnodospath(path, filepos));
		if (!i) filepos++;
		else if (i == -1) {
#ifdef	_NOEXTRACOPY
			warning(-1, filelist[filepos].name);
#else	/* !_NOEXTRACOPY */
			if (errno != EXDEV
			|| !isdir(&(filelist[filepos]))
			|| islink(&(filelist[filepos])))
				warning(-1, filelist[filepos].name);
			else {
				order = (islowerdir())
					? ORD_LOWER : ORD_NORMAL;
				i = applydir(filelist[filepos].name, safemove,
					cpdir, mvdir, order, ENDMV_K);
				if (i >= 0) filepos++;
			}
#endif	/* !_NOEXTRACOPY */
		}
	}

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

	return(FNC_EFFECT);
}

static int forcecpfile(path)
char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];

	if (getdestpath(path, dest, &st) < 0) return(0);
	if (safecpfile(path, dest, &st, NULL) < 0) return(-1);
#ifdef	HAVEFLAGS
	st.st_flags = (u_long)-1;
#endif

	st.st_nlink = (TCH_MODE | TCH_UID | TCH_GID | TCH_ATIME | TCH_MTIME);
	return(touchfile(dest, &st));
}

static int forcecpdir(path)
char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];
	int mode;

	if (isdotdir(path)) return(0);
	if (getdestpath(path, dest, &st) < 0) return(0);
	mode = (st.st_mode & 0777);
#if	MSDOS
	mode |= S_IWRITE;
#endif
	if (Xmkdir(dest, mode) >= 0) return(0);
	if (errno != EEXIST) return(-1);
	if (stat2(dest, &st) >= 0) {
		if (s_isdir(&st)) return(0);
		if (Xunlink(dest) >= 0 && Xmkdir(dest, mode) >= 0) return(0);
	}

	errno = EEXIST;

	return(-1);
}

static int forcetouchdir(path)
char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];

	if (isdotdir(path)) return(0);
	if (getdestpath(path, dest, &st) < 0) return(-2);
#ifdef	HAVEFLAGS
	st.st_flags = (u_long)-1;
#endif

	st.st_nlink = (TCH_MODE | TCH_UID | TCH_GID | TCH_ATIME | TCH_MTIME);
	return(touchfile(dest, &st));
}

int forcemovefile(dest)
char *dest;
{
	int ret;

	destpath = dest;
	ret = applydir(NULL, forcecpfile, forcecpdir, forcetouchdir, 1, NULL);
	destpath = NULL;

	return(ret);
}
