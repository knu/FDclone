/*
 *	apply.c
 *
 *	apply function to files
 */

#include "fd.h"
#include "time.h"
#include "realpath.h"
#include "parse.h"
#include "func.h"
#include "kanji.h"

#define	MAXTIMESTR		8
#define	ATTRWIDTH		10
#define	AT_MODE			0
#define	AT_FLAG			1
#define	AT_DATE			2
#define	AT_TIME			3
#define	AW_FILENAME		16
#define	AW_OMITNAME		12
#define	AW_MODE			(WMODE - 1)
#define	AW_FLAG			8
#define	AW_DATE			8
#define	AW_TIME			8
#if	MSDOS
#define	DIRENTSIZ(s)		DOSDIRENT
#else
#define	DIRENTSIZ(s)		(sizeof(struct dirent) \
				- DNAMESIZE + strlen(s) + 1)
#endif
#ifdef	_NOEXTRACOPY
#define	MAXCOPYITEM		4
#define	FLAG_SAMEDIR		0
#else
#define	MAXCOPYITEM		5
#define	FLAG_SAMEDIR		8
#endif

typedef struct _attrib_t {
	short nlink;
	u_int mode;
#ifdef	HAVEFLAGS
	u_long flags;
#endif
#ifndef	_NOEXTRAATTR
	u_int mask;
# ifndef	NOUID
	u_id_t uid;
	g_id_t gid;
# endif
#endif	/* !_NOEXTRAATTR */
	char timestr[2][MAXTIMESTR + 1];
} attrib_t;

extern reg_ex_t *findregexp;
extern int subwindow;
extern int win_x;
extern int win_y;
extern int lcmdline;
extern int maxcmdline;
extern int mark;
#ifdef	HAVEFLAGS
extern CONST u_long fflaglist[];
#endif
#if	!defined (_USEDOSCOPY) && !defined (_NOEXTRACOPY)
extern int inheritcopy;
#endif

static int NEAR issamedir __P_((CONST char *, CONST char *));
static int NEAR islowerdir __P_((VOID_A));
static char *NEAR getdestdir __P_((CONST char *, CONST char *));
static int NEAR getdestpath __P_((CONST char *, char *, struct stat *));
static int NEAR getcopypolicy __P_((CONST char *));
static int NEAR getremovepolicy __P_((CONST char *, int));
static int NEAR checkdupl __P_((CONST char *, char *,
		struct stat *, struct stat *));
static int NEAR checkrmv __P_((CONST char *, int));
#ifndef	_NOEXTRACOPY
static int NEAR isxdev __P_((CONST char *, struct stat *));
static VOID NEAR addcopysize __P_((off_t *));
static VOID NEAR _showprogress __P_((int));
static int countcopysize __P_((CONST char *));
static int countmovesize __P_((CONST char *));
static int NEAR prepareprogress __P_((int, int (*)(CONST char *)));
#endif
static int NEAR preparecopy __P_((int, int));
static int NEAR preparemove __P_((int, int));
static int safecopy __P_((CONST char *));
static int safemove __P_((CONST char *));
static int cpdir __P_((CONST char *));
static int touchdir __P_((CONST char *));
#ifndef	_NOEXTRACOPY
static int mvdir1 __P_((CONST char *));
static int mvdir2 __P_((CONST char *));
static int countremovesize __P_((CONST char *));
#endif
static VOID NEAR showmode __P_((attrib_t *, int, int));
static int NEAR getattrcolumn __P_((int *));
static int NEAR getattrtype __P_((int, int *));
static VOID NEAR showattr __P_((namelist *, attrib_t *, int));
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
static int NEAR inputuid __P_((attrib_t *, int));
static int NEAR inputgid __P_((attrib_t *, int));
#endif
static char **NEAR getdirtree __P_((char *, char **, int *, int));
static int NEAR _applydir __P_((CONST char *, int (*)(CONST char *),
		int (*)(CONST char *), int (*)(CONST char *),
		int, CONST char *, int));
static int forcecpfile __P_((CONST char *));
static int forcecpdir __P_((CONST char *));
static int forcetouchdir __P_((CONST char *));

char *destpath = NULL;
#ifndef	_NOEXTRACOPY
int progressbar = 0;
int precopymenu = 0;
#endif

static int copypolicy = 0;
static int removepolicy = 0;
static short attrnlink = 0;
static u_short attrmode = 0;
#ifdef	HAVEFLAGS
static u_long attrflags = 0;
#endif
#ifndef	_NOEXTRAATTR
static u_short attrmask = 0;
# ifndef	NOUID
static u_id_t attruid = (u_id_t)-1;
static g_id_t attrgid = (g_id_t)-1;
# endif
#endif	/* !_NOEXTRAATTR */
static time_t attrtime = (time_t)0;
static char *destdir = NULL;
static short destnlink = (short)0;
static u_short destmode = (u_short)0;
static time_t destmtime = (time_t)-1;
static time_t destatime = (time_t)-1;
#ifndef	_NOEXTRACOPY
static char *forwardpath = NULL;
#endif
#ifdef	DEP_PSEUDOPATH
static int destdrive = -1;
#endif
#if	!defined (_NOEXTRACOPY) && defined (DEP_DOSDRIVE)
static int forwarddrive = -1;
#endif
#ifndef	_NOEXTRACOPY
static int copycolumn = -1;
static int copyline = 0;
static long copyunit = 0L;
static long maxcopyunit = 0L;
static off_t copysize = (off_t)0;
static off_t maxcopysize = (off_t)0;
#endif


static int NEAR issamedir(path, org)
CONST char *path, *org;
{
	char path2[MAXPATHLEN], org2[MAXPATHLEN];

	org = (org) ? getrealpath(org, org2, NULL) : Xgetwd(org2);
	if (org != org2) return(0);
#ifdef	DEP_DOSPATH
	if (dospath(path, NULL) != dospath(org2, NULL)) return(0);
#endif
	if (getrealpath(path, path2, NULL) != path2) return(0);
	if (strpathcmp(path2, org2)) return(0);

	return(1);
}

static int NEAR islowerdir(VOID_A)
{
#ifdef	DEP_DOSEMU
	char orgpath[MAXPATHLEN];
#endif
	CONST char *org;
	char *cp, *top, path[MAXPATHLEN], buf[MAXPATHLEN];
	int len;

	if (!isdir(&(filelist[filepos])) || islink(&(filelist[filepos])))
		return(0);
	org = filelist[filepos].name;
#ifdef	DEP_DOSEMU
	org = nodospath(orgpath, org);
#endif
	Xstrcpy(path, destpath);
	top = path;
#ifdef	DEP_DOSPATH
	if (dospath(path, NULL) != dospath(org, NULL)) return(0);
	if (_dospath(path)) top += 2;
#endif

	while ((cp = (char *)getrealpath(path, buf, NULL)) != buf) {
		if (cp != path) return(0);
		cp = strrdelim(top, 0);
		if (!cp || cp <= top) return(0);
		*cp = '\0';
	}

	if (getrealpath(org, path, NULL) != path) return(0);
	len = strlen(path);
	if (strnpathcmp(path, buf, len) || buf[len] != _SC_) len = 0;

	return(len);
}

static char *NEAR getdestdir(mes, arg)
CONST char *mes, *arg;
{
	char *dir;

	if (arg && *arg) dir = Xstrdup(arg);
	else if (!(dir = inputstr(mes, 1, -1, NULL, HST_PATH))) return(NULL);
	else if (!*(dir = evalpath(dir, 0))) {
		dir = Xrealloc(dir, 2);
		dir[0] = '.';
		dir[1] = '\0';
	}
#ifdef	DEP_DOSPATH
	else if (_dospath(dir) && !dir[2]) {
		dir = Xrealloc(dir, 4);
		dir[2] = '.';
		dir[3] = '\0';
	}
#endif

#ifdef	DEP_PSEUDOPATH
	if ((destdrive = preparedrv(dir, NULL, NULL)) < 0) {
		warning(-1, dir);
		Xfree(dir);
		return(NULL);
	}
#endif
	if (preparedir(dir) < 0) {
		warning(-1, dir);
		Xfree(dir);
#ifdef	DEP_DOSDRIVE
		shutdrv(destdrive);
		destdrive = -1;
#endif
		return(NULL);
	}

	return(dir);
}

static int NEAR getdestpath(file, dest, stp)
CONST char *file;
char *dest;
struct stat *stp;
{
	CONST char *cp;
	char *tmp;
	int n;

	Xstrcpy(dest, destpath);
	cp = file;
	if (destdir) {
		for (n = 0; (tmp = strdelim(cp, 0)); n++) cp = tmp + 1;
		for (tmp = destdir; n > 0 && (tmp = strdelim(tmp, 0)); tmp++)
			n--;
		if (tmp) *tmp = '\0';
		if (*destdir) Xstrcpy(strcatdelim(dest), destdir);
	}
	Xstrcpy(strcatdelim(dest), cp);

	if (Xlstat(file, stp) < 0) {
		warning(-1, file);
		return(-1);
	}

	return(0);
}

static int NEAR getcopypolicy(s)
CONST char *s;
{
#ifndef	_NOEXTRACOPY
# ifdef	DEP_PSEUDOPATH
	int dupdestdrive;
# endif
	char *cp;
#endif	/* !_NOEXTRACOPY */
	CONST char *str[MAXCOPYITEM];
	int n, ch, val[MAXCOPYITEM];

	for (;;) {
		Xlocate(0, L_CMDLINE);
		Xputterm(L_CLEAR);
		VOID_C Xkanjiputs(s);

		str[0] = UPDAT_K;
		val[0] = CPP_UPDATE;
		str[1] = RENAM_K;
		val[1] = CPP_RENAME;
		str[2] = OVERW_K;
		val[2] = CPP_OVERWRITE;
		str[3] = NOCPY_K;
		val[3] = CPP_NOCOPY;
#ifndef	_NOEXTRACOPY
		str[4] = FORWD_K;
		val[4] = CPP_FORWUPDATE;
#endif

		n = CPP_UPDATE;
		ch = selectstr(&n, MAXCOPYITEM, 0, str, val);
		if (ch == K_ESC) return(CHK_ERROR);
		else if (ch != K_CR) return(CHK_CANCEL);
#ifndef	_NOEXTRACOPY
		if (n == CPP_FORWUPDATE) {
			str[0] = UPDAT_K;
			str[1] = OVERW_K;
			val[0] = CPP_FORWUPDATE;
			val[1] = CPP_FORWOVERWRITE;
			if (selectstr(&n, 2, 0, str, val) != K_CR) continue;
# ifdef	DEP_PSEUDOPATH
			dupdestdrive = destdrive;
			destdrive = -1;
# endif

			if (!(cp = getdestdir(FRWDD_K, NULL))) continue;
# ifdef	DEP_PSEUDOPATH
#  if	!defined (_NOEXTRACOPY) && defined (DEP_DOSDRIVE)
			forwarddrive = destdrive;
#  endif
			destdrive = dupdestdrive;
# endif	/* DEP_PSEUDOPATH */
			if (issamedir(cp, NULL) || issamedir(cp, destpath)) {
				warning(EINVAL, cp);
				Xfree(cp);
				continue;
			}
			forwardpath = cp;
		}
#endif	/* !_NOEXTRACOPY */

		copypolicy = n;
		break;
	}

	return(copypolicy);
}

static int NEAR getremovepolicy(s, pre)
CONST char *s;
int pre;
{
	CONST char *str[4];
	int i, n, ch, val[4];

	Xlocate(0, L_CMDLINE);
	Xputterm(L_CLEAR);
	VOID_C Xkanjiputs(s);

	i = 0;
	if (pre) {
		str[i] = ANCNF_K;
		val[i++] = CHK_OK;
	}
	else {
		str[i] = ANYES_K;
		val[i++] = CHK_OK;
		str[i] = ANNO_K;
		val[i++] = CHK_ERROR;
	}
	str[i] = ANALL_K;
	val[i++] = RMP_REMOVEALL;
	str[i] = ANKEP_K;
	val[i++] = RMP_KEEPALL;
	n = CHK_OK;
	ch = selectstr(&n, i, 0, str, val);
	if (ch == K_ESC) return(CHK_ERROR);
	else if (ch != K_CR) return(CHK_CANCEL);
	else if (n <= 0) return(n);
	removepolicy = n;

	return(removepolicy);
}

static int NEAR checkdupl(file, dest, stp1, stp2)
CONST char *file;
char *dest;
struct stat *stp1, *stp2;
{
#ifndef	_NOEXTRACOPY
	char path[MAXPATHLEN];
#endif
	CONST char *s;
	char *cp, *tmp;
	int i, n;

	if (getdestpath(file, dest, stp1) < 0) return(CHK_ERROR);
	if (Xlstat(dest, stp2) < 0) {
		stp2 -> st_mode = S_IWUSR;
		if (errno == ENOENT) return(CHK_OK);
		warning(-1, dest);
		return(CHK_ERROR);
	}
	if (!s_isdir(stp2)) n = (copypolicy & ~FLAG_SAMEDIR);
	else if (!s_isdir(stp1)) n = CPP_RENAME;
#ifdef	_NOEXTRACOPY
	else return(CHK_EXIST);
#else
	else if (!(copypolicy & FLAG_SAMEDIR)) return(CHK_EXIST);
	else {
		n = (copypolicy &= ~FLAG_SAMEDIR);
		copypolicy = 0;
	}
#endif

	for (;;) {
		if (!n || n == CPP_RENAME) {
			s = SAMEF_K;
			i = strlen2(s) - strsize("%.*s");
			cp = asprintf2(s, n_column - i, dest);

#ifndef	_NOEXTRACOPY
			copycolumn = -1;
#endif
			if (!n) n = getcopypolicy(cp);
			else {
				Xlocate(0, L_CMDLINE);
				Xputterm(L_CLEAR);
				VOID_C Xkanjiputs(cp);
			}
			Xfree(cp);
			if (n < 0) return(n);
		}
		switch (n) {
			case CPP_UPDATE:
				if (stp1 -> st_mtime < stp2 -> st_mtime)
					return(CHK_ERROR);
				return(CHK_EXIST);
/*NOTREACHED*/
				break;
			case CPP_RENAME:
				lcmdline = L_INFO;
				tmp = inputstr(NEWNM_K, 1, -1, NULL, -1);
				if (!tmp) return(CHK_ERROR);
				Xstrcpy(getbasename(dest), tmp);
				Xfree(tmp);
				if (Xlstat(dest, stp2) < 0) {
					stp2 -> st_mode = S_IWUSR;
					if (errno == ENOENT) return(CHK_OK);
					warning(-1, dest);
					return(CHK_ERROR);
				}
				if (s_isdir(stp1) && s_isdir(stp2))
					return(CHK_EXIST);
				Xputterm(T_BELL);
				break;
			case CPP_OVERWRITE:
				return(CHK_OVERWRITE);
/*NOTREACHED*/
				break;
#ifndef	_NOEXTRACOPY
			case CPP_FORWUPDATE:
				if (stp1 -> st_mtime < stp2 -> st_mtime)
					return(CHK_ERROR);
/*FALLTHRU*/
			case CPP_FORWOVERWRITE:
				strcatdelim2(path, forwardpath, file);
				cp = strrdelim(path, 0);
				*cp = '\0';
				if (mkdir2(path, 0777) < 0
				&& errno != EEXIST) {
					warning(-1, path);
					return(CHK_ERROR);
				}
				*cp = _SC_;
				if (safemvfile(dest, path, stp2, NULL) < 0) {
					warning(-1, path);
					return(CHK_ERROR);
				}
				return(CHK_OVERWRITE);
/*NOTREACHED*/
				break;
#endif	/* !_NOEXTRACOPY */
			default:
				return(CHK_ERROR);
/*NOTREACHED*/
				break;
		}
	}

/*NOTREACHED*/
	return(CHK_OK);
}

static int NEAR checkrmv(path, mode)
CONST char *path;
int mode;
{
#if	!MSDOS
	struct stat *stp, st;
	char *tmp, dir[MAXPATHLEN];
	int duperrno;
#endif
	CONST char *s;
	char *cp;
	int n, len;

#if	MSDOS
	if (Xaccess(path, mode) >= 0) return(CHK_OK);
#else	/* !MSDOS */
# ifdef	DEP_DOSDRIVE
	if (dospath2(path)) {
		if (Xaccess(path, mode) >= 0) return(CHK_OK);
		stp = NULL;
	}
	else
# endif
# ifdef	DEP_URLPATH
	if (urlpath(path, NULL, NULL, NULL)) {
		if (Xaccess(path, mode) >= 0) return(CHK_OK);
		stp = NULL;
	}
	else
# endif
	{
		if (reallstat(path, &st) < 0) {
			warning(-1, path);
			return(CHK_ERROR);
		}
		if (s_islnk(&st)) return(CHK_OK);

		if (!(tmp = strrdelim(path, 0))) copycurpath(dir);
		else if (tmp == path) copyrootpath(dir);
		else Xstrncpy(dir, path, tmp - path);

		if (reallstat(dir, &st) < 0) {
			warning(-1, dir);
			return(CHK_ERROR);
		}
		if (Xaccess(path, mode) >= 0) return(CHK_OK);
		stp = &st;
	}
#endif	/* !MSDOS */

	if (errno == ENOENT) return(CHK_OK);
	if (errno != EACCES) {
		warning(-1, path);
		return(CHK_ERROR);
	}

#if	!MSDOS
	if (stp) {
		duperrno = errno;
		mode = logical_access2(stp);
		if (!(mode & F_ISWRI)) {
			errno = duperrno;
			warning(-1, path);
			return(CHK_ERROR);
		}
	}
#endif	/* !MSDOS */

	if (removepolicy > 0) n = removepolicy;
	else {
		s = DELPM_K;
		len = strlen2(s) - strsize("%.*s");
		cp = asprintf2(s, n_column - len, path);
#ifndef	_NOEXTRACOPY
		copycolumn = -1;
#endif
		n = getremovepolicy(cp, 0);
		Xfree(cp);
	}
	if (n > 0) n -= RMP_BIAS;

	return(n);
}

#ifndef	_NOEXTRACOPY
/*ARGSUSED*/
static int NEAR isxdev(path, stp)
CONST char *path;
struct stat *stp;
{
# if	!MSDOS
	struct stat st1, st2;
# endif

# ifdef	DEP_DOSPATH
	if (dospath(path, NULL) != dospath(destpath, NULL)) {
		if (stp && Xlstat(path, stp) < 0) return(-1);
		return(1);
	}
# endif

# if	!MSDOS
	if (!stp) stp = &st1;
	if (Xlstat(path, stp) < 0 || Xlstat(destpath, &st2) < 0) return(-1);
	if (stp -> st_dev != st2.st_dev) return(1);
# endif

	return(0);
}

static VOID NEAR addcopysize(sizep)
off_t *sizep;
{
	if (*sizep <= MAXTYPE(off_t) - maxcopysize) maxcopysize += *sizep;
	else {
		maxcopysize -= MAXTYPE(off_t) - *sizep;
		maxcopyunit++;
	}
}

static VOID NEAR _showprogress(n)
int n;
{
	int i, max, duperrno;

	duperrno = errno;
	max = n_column - 1;
	Xlocate(0, copyline);
	for (i = 0; i < n; i++) VOID_C XXputch('o');
	for (; i < max; i++) VOID_C XXputch('.');
	Xlocate(n, copyline);
	Xtflush();
	errno = duperrno;
}

VOID showprogress(sizep)
off_t *sizep;
{
	off_t size;
	long unit;
	int n, max;

	if (!maxcopysize && !maxcopyunit) return;

	size = *sizep;
	max = n_column - 1;
	for (n = 0; n < max; n++) {
		if (size <= MAXTYPE(off_t) - copysize) copysize += size;
		else {
			copysize -= MAXTYPE(off_t) - size + 1;
			copyunit++;
		}
	}

	size = copysize;
	unit = copyunit;
	for (n = 0; n < max; n++) {
		if (maxcopysize <= size) size -= maxcopysize;
		else if (!unit) break;
		else {
			size += MAXTYPE(off_t) - maxcopysize + 1;
			unit--;
		}
		if (unit < maxcopyunit) break;
		unit -= maxcopyunit;
	}

	if (n > copycolumn) _showprogress(copycolumn = n);
}

VOID fshowprogress(path)
CONST char *path;
{
	CONST char *cp;
	off_t size;

	if (!(cp = strrdelim(path, 1))) cp = path;
	size = (off_t)DIRENTSIZ(cp);
	showprogress(&size);
}

static int countcopysize(path)
CONST char *path;
{
	struct stat st;
	CONST char *cp;
	off_t size;

	if (!(cp = strrdelim(path, 1))) cp = path;
	size = (off_t)DIRENTSIZ(cp);
	addcopysize(&size);
	if (Xlstat(path, &st) < 0) return(APL_ERROR);
	size = (off_t)(st.st_size);
	addcopysize(&size);

	return(APL_OK);
}

static int countmovesize(path)
CONST char *path;
{
	struct stat st;
	CONST char *cp;
	off_t size;
	int n;

	if (!(cp = strrdelim(path, 1))) cp = path;
	size = (off_t)DIRENTSIZ(cp);
	addcopysize(&size);
	if ((n = isxdev(path, &st)) < 0) return(APL_ERROR);
	else if (n && !s_isdir(&st)) {
		size = (off_t)(st.st_size);
		addcopysize(&size);
	}

	return(APL_OK);
}

static int NEAR prepareprogress(isdir, func)
int isdir;
int (*func)__P_((CONST char *));
{
	int ret;

	copycolumn = -1;
	copyline = 0;
	copyunit = maxcopyunit = 0L;
	copysize = maxcopysize = (off_t)0;
	if (progressbar) {
		if (!isdir) {
			copyline = L_CMDLINE;
			VOID_C applyfile(func, NULL);
		}
		else {
			copyline = L_STACK;
			ret = applydir(filelist[filepos].name, func,
				NULL, func, ORD_NOPREDIR, NULL);
			if (ret == APL_ERROR) return(-1);
			if (ret == APL_CANCEL) {
				maxcopyunit = 0L;
				maxcopysize = (off_t)0;
			}
		}
	}

	return(0);
}
#endif	/* !_NOEXTRACOPY */

/*ARGSUSED*/
static int NEAR preparecopy(isdir, narg)
int isdir, narg;
{
	copypolicy = (issamedir(destpath, NULL))
		? (FLAG_SAMEDIR | CPP_RENAME) : 0;
#ifndef	_NOEXTRACOPY
	if (precopymenu && !copypolicy && (isdir || narg > 1))
		if (getcopypolicy(PRECP_K) < 0) return(-1);
	if (prepareprogress(isdir, countcopysize)) return(-1);
#endif	/* !_NOEXTRACOPY */
#ifdef	DEP_DOSDRIVE
	if (dospath3(nullstr)) waitmes();
#endif

	return(0);
}

/*ARGSUSED*/
static int NEAR preparemove(isdir, narg)
int isdir, narg;
{
	copypolicy = removepolicy = 0;
#ifndef	_NOEXTRACOPY
	if (precopymenu && (isdir || narg > 1)) {
		if (getcopypolicy(PRECP_K) < 0) return(-1);
		if (getremovepolicy(PRERM_K, 1) < 0) return(-1);
	}
	if (prepareprogress(isdir, countmovesize) < 0) return(-1);
#endif	/* !_NOEXTRACOPY */

	return(0);
}

static int safecopy(path)
CONST char *path;
{
	struct stat st1, st2;
	char dest[MAXPATHLEN];
	int n;

	if ((n = checkdupl(path, dest, &st1, &st2)) < 0)
		return((n == CHK_CANCEL) ? APL_CANCEL : APL_IGNORE);
	if (safecpfile(path, dest, &st1, &st2) < 0) return(APL_ERROR);

	return(APL_OK);
}

static int safemove(path)
CONST char *path;
{
	struct stat st1, st2;
	char dest[MAXPATHLEN];
	int n;

	if ((n = checkdupl(path, dest, &st1, &st2)) < 0
	|| (n = checkrmv(path, W_OK)) < 0)
		return((n == CHK_CANCEL) ? APL_CANCEL : APL_IGNORE);
	if (safemvfile(path, dest, &st1, &st2) < 0) return(APL_ERROR);

	return(APL_OK);
}

static int cpdir(path)
CONST char *path;
{
	struct stat st1, st2;
	char dest[MAXPATHLEN];

	switch (checkdupl(path, dest, &st1, &st2)) {
		case CHK_OVERWRITE:
		/* Already exist, but not directory */
			if (Xunlink(dest) < 0) return(APL_ERROR);
/*FALLTHRU*/
		case CHK_OK:
		/* Not exist */
			if (Xmkdir(dest, st1.st_mode & 0777) < 0)
				return(APL_ERROR);
			destnlink = (TCH_UID | TCH_GID
				| TCH_ATIME | TCH_MTIME);
			destmode = st1.st_mode;
			destmtime = st1.st_mtime;
			destatime = st1.st_atime;
			break;
		case CHK_ERROR:
		case CHK_CANCEL:
		/* Abandon copy */
			return(APL_CANCEL);
/*NOTREACHED*/
			break;
		default:
		/* Already exist */
			destnlink = (TCH_ATIME | TCH_MTIME);
			destmode = st2.st_mode;
			destmtime = st2.st_mtime;
			destatime = st2.st_atime;
			break;
	}

	if (!(destmode & S_IWUSR)) {
		st1.st_nlink = TCH_MODE;
		st1.st_mode = (destmode | S_IWUSR);
		if (touchfile(dest, &st1) >= 0) destnlink |= TCH_MODE;
	}

	Xfree(destdir);
	destdir = &(dest[strlen(destpath)]);
	while (*destdir == _SC_) destdir++;
	destdir = Xstrdup(destdir);

	return(APL_OK);
}

/*ARGSUSED*/
static int touchdir(path)
CONST char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];

	if (getdestpath(path, dest, &st) < 0) return(APL_CANCEL);
	st.st_nlink = (destnlink | TCH_IGNOREERR);
	st.st_mode = destmode;
	st.st_mtime = destmtime;
	st.st_atime = destatime;
#ifndef	_USEDOSCOPY
# ifndef	_NOEXTRACOPY
	if (inheritcopy) /*EMPTY*/;
	else
# endif
	st.st_nlink &= ~(TCH_ATIME | TCH_MTIME);
#endif	/* !_USEDOSCOPY */
	if (touchfile(dest, &st) < 0) return(APL_ERROR);

	return(APL_OK);
}

#ifndef	_NOEXTRACOPY
static int mvdir1(path)
CONST char *path;
{
	struct stat st;
	int n;

	if ((n = checkrmv(path, R_OK | W_OK | X_OK)) < 0) {
		errno = EACCES;
		return((n == CHK_CANCEL) ? APL_CANCEL : APL_ERROR);
	}
	if ((n = cpdir(path)) < 0) return(n);
	if (Xlstat(path, &st) < 0) return(APL_ERROR);
	if (!(st.st_mode & S_IWUSR)) {
		st.st_nlink = TCH_MODE;
		st.st_mode |= S_IWUSR;
		if (touchfile(path, &st) < 0) return(APL_ERROR);
	}

	return(n);
}

static int mvdir2(path)
CONST char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];

	if (getdestpath(path, dest, &st) < 0) return(APL_CANCEL);
	st.st_nlink = (destnlink | TCH_IGNOREERR);
	st.st_mode = destmode;
	st.st_mtime = destmtime;
	st.st_atime = destatime;
	if (touchfile(dest, &st) < 0) return(APL_ERROR);
	if (Xrmdir(path) < 0) return(APL_ERROR);
#ifndef	_NOEXTRACOPY
	fshowprogress(path);
#endif

	return(APL_OK);
}

static int countremovesize(path)
CONST char *path;
{
	CONST char *cp;
	off_t size;

	if (!(cp = strrdelim(path, 1))) cp = path;
	size = (off_t)DIRENTSIZ(cp);
	addcopysize(&size);

	return(APL_OK);
}
#endif	/* !_NOEXTRACOPY */

/*ARGSUSED*/
int prepareremove(isdir, narg)
int isdir, narg;
{
	removepolicy = 0;
#ifndef	_NOEXTRACOPY
	if (precopymenu && (isdir || narg > 1))
		if (getremovepolicy(PRERM_K, 1) < 0) return(-1);
	if (prepareprogress(isdir, countremovesize)) return(-1);
#endif	/* !_NOEXTRACOPY */

	return(0);
}

int rmvfile(path)
CONST char *path;
{
	int n;

	if ((n = checkrmv(path, W_OK)) < 0)
		return((n == CHK_CANCEL) ? APL_CANCEL : APL_IGNORE);
	if (Xunlink(path) < 0) return(APL_ERROR);
#ifndef	_NOEXTRACOPY
	fshowprogress(path);
#endif

	return(APL_OK);
}

int rmvdir(path)
CONST char *path;
{
	int n;

	if ((n = checkrmv(path, R_OK | W_OK | X_OK)) < 0)
		return((n == CHK_CANCEL) ? APL_CANCEL : APL_IGNORE);
	if (Xrmdir(path) < 0) return(APL_ERROR);
#ifndef	_NOEXTRACOPY
	fshowprogress(path);
#endif

	return(APL_OK);
}

int findfile(path)
CONST char *path;
{
	if (regexp_exec(findregexp, getbasename(path), 1)) {
		if (path[0] == '.' && path[1] == _SC_) path += 2;
		Xlocate(0, L_CMDLINE);
		Xputterm(L_CLEAR);
		VOID_C XXcprintf("[%^.*k]", n_column - 2, path);
		if (yesno(FOUND_K)) {
			destpath = Xstrdup(path);
			return(APL_CANCEL);
		}
	}

	return(APL_OK);
}

int finddir(path)
CONST char *path;
{
	if (regexp_exec(findregexp, getbasename(path), 1)) {
		if (yesno(FOUND_K)) {
			destpath = Xstrdup(path);
			return(APL_CANCEL);
		}
	}

	return(APL_OK);
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
	XXcputs(buf);
}

static int NEAR getattrcolumn(xp)
int *xp;
{
	int x1, x2;

	x1 = n_column / 2 - 20;
	x2 = n_column / 2;

	if (isbestomit()) {
		x1 += AW_FILENAME - AW_OMITNAME;
		x2 -= 3;
	}
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
	else if (iswellomit()) {
		x1 -= WOWNER / 2;
		x2 -= WOWNER / 2;
	}
#endif
	if (xp) *xp = x1;

	return(x2);
}

static int NEAR getattrtype(y, wp)
int y, *wp;
{
	int typ, w;

	if (!y) {
		typ = AT_MODE;
		w = AW_MODE;
	}
#ifdef	HAVEFLAGS
	else if (y == WMODELINE - 1) {
		typ = AT_FLAG;
		w = AW_FLAG;
	}
#endif
	else if (y == WMODELINE) {
		typ = AT_DATE;
		w = AW_DATE;
	}
	else {
		typ = AT_TIME;
		w = AW_TIME;
	}
	if (wp) *wp = w;

	return(typ);
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
	x2 = getattrcolumn(&x1);
	w = (isbestomit()) ? AW_OMITNAME : AW_FILENAME;
	y = yy;

	Xlocate(0, y);
	Xputterm(L_CLEAR);

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	VOID_C XXcprintf("[%^-*.*k]", w, w, namep -> name);
	Xlocate(x2 + 3, y);
	VOID_C Xkanjiputs(TOLD_K);
	Xlocate(x2 + 13, y);
	VOID_C Xkanjiputs(TNEW_K);

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	VOID_C Xkanjiputs(TMODE_K);
	Xlocate(x2, y);
	putmode(buf, namep -> st_mode, 1);
	XXcputs(buf);
	showmode(attr, x2 + ATTRWIDTH, y);

#ifdef	HAVEFLAGS
	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	VOID_C Xkanjiputs(TFLAG_K);
	Xlocate(x2, y);
	putflags(buf, namep -> st_flags);
	XXcputs(buf);
	Xlocate(x2 + ATTRWIDTH, y);
	putflags(buf, attr -> flags);
	XXcputs(buf);
#endif

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	VOID_C Xkanjiputs(TDATE_K);
	Xlocate(x2, y);
	VOID_C XXcprintf("%02d-%02d-%02d",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
	Xlocate(x2 + ATTRWIDTH, y);
	XXcputs(attr -> timestr[0]);

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);
	Xlocate(x1, y);
	VOID_C Xkanjiputs(TTIME_K);
	Xlocate(x2, y);
	VOID_C XXcprintf("%02d:%02d:%02d",
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec);
	Xlocate(x2 + ATTRWIDTH, y);
	XXcputs(attr -> timestr[1]);

	Xlocate(0, ++y);
	Xputterm(L_CLEAR);

#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
	if (ishardomit()) return;

	y = yy + 1;
	x2 += 20;
	Xlocate(x2, y++);
	VOID_C Xkanjiputs(TOWN_K);
	Xlocate(x2, y++);
	putowner(buf, attr -> uid);
	VOID_C XXputch('<');
	Xattrkanjiputs(buf, attr -> nlink & TCH_UID);
	VOID_C XXputch('>');
# ifdef	HAVEFLAGS
	y++;
# endif
	Xlocate(x2, y++);
	VOID_C Xkanjiputs(TGRP_K);
	Xlocate(x2, y++);
	putgroup(buf, attr -> gid);
	VOID_C XXputch('<');
	Xattrkanjiputs(buf, attr -> nlink & TCH_GID);
	VOID_C XXputch('>');
#endif	/* !_NOEXTRAATTR && !NOUID */
}

#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
static int NEAR inputuid(attr, yy)
attrib_t *attr;
int yy;
{
	uidtable *up;
	char *cp, *s, buf[MAXLONGWIDTH + 1];
	u_id_t uid;

	up = finduid(attr -> uid, NULL);
	if (up) cp = up -> name;
	else {
		VOID_C Xsnprintf(buf, sizeof(buf), "%-d", (int)(attr -> uid));
		cp = buf;
	}

	yy += 2 + WMODELINE + 2;
	lcmdline = yy;
	maxcmdline = 1;
	if (!(s = inputstr(AOWNR_K, 0, -1, cp, HST_USER))) return(-1);
	if ((cp = Xsscanf(s, "%-<*d%$", sizeof(u_id_t), &uid))) /*EMPTY*/;
	else if ((up = finduid((u_id_t)0, s))) uid = up -> uid;
	else {
		lcmdline = yy;
		warning(ENOENT, s);
		Xfree(s);
		return(-1);
	}

	Xfree(s);
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
	g_id_t gid;

	gp = findgid(attr -> gid, NULL);
	if (gp) cp = gp -> name;
	else {
		VOID_C Xsnprintf(buf, sizeof(buf), "%-d", (int)(attr -> gid));
		cp = buf;
	}

	yy += 2 + WMODELINE + 2;
	lcmdline = yy;
	maxcmdline = 1;
	if (!(s = inputstr(AGRUP_K, 0, -1, cp, HST_GROUP))) return(-1);
	if ((cp = Xsscanf(s, "%-<*d%$", sizeof(g_id_t), &gid))) /*EMPTY*/;
	else if ((gp = findgid((g_id_t)0, s))) gid = gp -> gid;
	else {
		lcmdline = yy;
		warning(ENOENT, s);
		Xfree(s);
		return(-1);
	}

	Xfree(s);
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
	attrib_t attr;
	struct tm *tm;
	time_t t;
	u_int mask;
	int ch, x, y, xx, yy, ymin, ymax, typ, w, dupwin_x, dupwin_y, excl;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	Xgetkey(-1, 0, 0);

	yy = filetop(win);
	while (yy + WMODELINE + 5 > n_line - 1) yy--;
	if (yy <= L_TITLE) yy = L_TITLE + 1;
	xx = getattrcolumn(NULL) + ATTRWIDTH;

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
	VOID_C Xsnprintf(attr.timestr[0], sizeof(attr.timestr[0]),
		"%02d-%02d-%02d",
		tm -> tm_year % 100, tm -> tm_mon + 1, tm -> tm_mday);
	VOID_C Xsnprintf(attr.timestr[1], sizeof(attr.timestr[1]),
		"%02d:%02d:%02d",
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
		switch (ch = Xgetkey(1, 0, 0)) {
			case K_UP:
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				if (x > ATTRWIDTH) {
					y = (y) ? 0 : WMODELINE + 1;
					break;
				}
#endif
				if (y > ymin) y--;
				else y = ymax;
				VOID_C getattrtype(y, &w);
				if (x >= w) x = w - 1;
				break;
			case K_DOWN:
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				if (x > ATTRWIDTH) {
					y = (y) ? 0 : WMODELINE + 1;
					break;
				}
#endif
				if (y < ymax) y++;
				else y = ymin;
				VOID_C getattrtype(y, &w);
				if (x >= w) x = w - 1;
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
				if (ishardomit()) break;
				x = ATTRWIDTH + 1;
				y = 0;
				break;
			case 'g':
			case 'G':
				if (excl && excl != ATR_OWNERONLY) break;
				if (ishardomit()) break;
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
				VOID_C XXputch(ch);
				attr.timestr[y - WMODELINE][x] = ch;
				attr.nlink |= TCH_MTIME;
/*FALLTHRU*/
			case K_RIGHT:
				typ = getattrtype(y, &w);
				if (x < w - 1) {
					x++;
					switch (typ) {
						case AT_DATE:
						case AT_TIME:
							if ((++x) % 3) x--;
							break;
						default:
							break;
					}
				}
				else switch (typ) {
					case AT_DATE:
						y++;
						x = 0;
						break;
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
					case AT_MODE:
					case AT_TIME:
						if (excl) break;
						if (ishardomit()) break;
						x = ATTRWIDTH + 1;
						break;
#endif
					default:
						break;
				}
				break;
			case K_BS:
				if (y < WMODELINE) break;
/*FALLTHRU*/
			case K_LEFT:
				typ = getattrtype(y, &w);
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				if (x > ATTRWIDTH) {
					if (excl || ch == K_BS) break;
					if (ishardomit()) break;
					x = w - 1;
				}
				else
#endif
				if (x > 0) {
					switch (typ) {
						case AT_DATE:
						case AT_TIME:
							if (!(x % 3)) x--;
							break;
						default:
							break;
					}
					x--;
				}
				else switch (typ) {
					case AT_TIME:
						y--;
						x = w - 1;
						break;
					default:
						break;
				}
				break;
			case K_CTRL('L'):
				yy = filetop(win);
				while (yy + WMODELINE + 5 > n_line - 1) yy--;
				if (yy <= L_TITLE) yy = L_TITLE + 1;
				xx = getattrcolumn(NULL) + ATTRWIDTH;
				showattr(namep, &attr, yy);
				break;
			case ' ':
				typ = getattrtype(y, &w);
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
				if (x > ATTRWIDTH) {
					switch (typ) {
						case AT_MODE:
							inputuid(&attr, yy);
							break;
						case AT_TIME:
							inputgid(&attr, yy);
							break;
						default:
							break;
					}
					showattr(namep, &attr, yy);
					break;
				}
#endif
#ifdef	HAVEFLAGS
				if (typ == AT_FLAG) {
					attr.flags ^= fflaglist[x];
					Xlocate(xx, yy + y + 2);
					putflags(buf, attr.flags);
					XXcputs(buf);
					attr.nlink |= TCH_FLAGS;
					break;
				}
#endif
				if (typ != AT_MODE) break;
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
				if (!(flag & ATR_MULTIPLE)) break;
				if (getattrtype(y, NULL) != AT_MODE) break;
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
	Xgetkey(-1, 0, 0);

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
	|| tm -> tm_hour > 23 || tm -> tm_min > 59 || tm -> tm_sec > 60)
		return(-1);
	if ((t = Xtimelocal(tm)) == (time_t)-1) return(-1);

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
			if (attr.mode != (namep -> st_mode & ~S_IFMT))
				mask |= TCH_MODE;
#ifdef	HAVEFLAGS
			if (attr.flags != namep -> st_flags) mask |= TCH_FLAGS;
#endif
#if	!defined (_NOEXTRAATTR) && !defined (NOUID)
			if (attr.uid != namep -> st_uid) mask |= TCH_UID;
			if (attr.gid != namep -> st_gid) mask |= TCH_GID;
#endif
			if (t != namep -> st_mtim) mask |= TCH_MTIME;
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
	attrtime = t;

	return(1);
}

int setattr(path)
CONST char *path;
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
	if (touchfile(path, &st) < 0) return(APL_ERROR);

	return(APL_OK);
}

int applyfile(func, endmes)
int (*func)__P_((CONST char *));
CONST char *endmes;
{
#ifdef	DEP_DOSEMU
	char path[MAXPATHLEN];
#endif
	int i, ret, old, dupfilepos;

	dupfilepos = filepos;
	ret = old = filepos;

	if (mark <= 0) {
		i = (*func)(fnodospath(path, filepos));
		if (i == APL_ERROR) warning(-1, filelist[filepos].name);
		else if (i == APL_OK) ret++;
#ifndef	_NOEXTRACOPY
		if (endmes && copyline > 0) _showprogress(n_column);
#endif
		return(ret);
	}

	helpbar();
	for (filepos = 0; filepos < maxfile; filepos++) {
		if (intrkey(K_ESC)) {
			endmes = NULL;
			break;
		}
		if (!ismark(&(filelist[filepos]))) continue;

		movepos(old, 0);
		Xlocate(win_x, win_y);
		Xtflush();
		i = (*func)(fnodospath(path, filepos));
		if (i == APL_CANCEL) {
			endmes = NULL;
			break;
		}
		else if (i == APL_ERROR) warning(-1, filelist[filepos].name);
		else if (i == APL_OK && ret == filepos) ret++;

		old = filepos;
	}

	if (endmes) {
#ifndef	_NOEXTRACOPY
		if (copyline > 0) _showprogress(n_column);
#endif
		warning(0, endmes);
	}
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
		if (strcatpath(dir, cp, dp -> d_name) < 0) continue;

		if (Xlstat(dir, &st) < 0 || !s_isdir(&st)) continue;
		dirlist = b_realloc(dirlist, *maxp, char *);
		dirlist[(*maxp)++] = Xstrdup(dir);
		if (!(d = depth)) /*EMPTY*/;
		else if (d > 1) d--;
		else continue;
		dirlist = getdirtree(dir, dirlist, maxp, d);
	}
	if (cp > dir) *(cp - 1) = '\0';
	VOID_C Xclosedir(dirp);

	return(dirlist);
}

static int NEAR _applydir(dir, funcf, funcd1, funcd2, order, endmes, verbose)
CONST char *dir;
int (*funcf)__P_((CONST char *));
int (*funcd1)__P_((CONST char *));
int (*funcd2)__P_((CONST char *));
int order;
CONST char *endmes;
int verbose;
{
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	time_t dupmtime, dupatime;
	u_short dupmode;
	CONST char *cp;
	char *fname, path[MAXPATHLEN], **dirlist;
	int ret, ndir, max, dupnlink;

	if (intrkey(K_ESC)) return(APL_CANCEL);

	if (verbose) {
		Xlocate(0, L_CMDLINE);
		Xputterm(L_CLEAR);
		cp = (dir[0] == '.' && dir[1] == _SC_) ? &(dir[2]) : dir;
		VOID_C XXcprintf("[%^.*k]", n_column - 2, cp);
		Xtflush();
	}
#ifdef	FAKEUNINIT
	else cp = NULL;	/* fake for -Wuninitialized */
#endif

	if (!funcd1) order = (funcd2) ? ORD_NOPREDIR : ORD_NODIR;
	Xstrcpy(path, dir);

	ret = APL_OK;
	max = 0;
	if (!order) dirlist = NULL;
	else dirlist = getdirtree(path,
		NULL, &max, (order == ORD_LOWER) ? 0 : 1);
	fname = strcatdelim(path);

	destnlink = 0;
	if ((order == ORD_NORMAL || order == ORD_LOWER)
	&& (ret = (*funcd1)(dir)) < 0) {
		if (ret == APL_ERROR) warning(-1, dir);
		if (dirlist) {
			for (ndir = 0; ndir < max; ndir++)
				Xfree(dirlist[ndir]);
			Xfree(dirlist);
		}
		return(ret);
	}
	dupnlink = destnlink;
	dupmode = destmode;
	dupmtime = destmtime;
	dupatime = destatime;

	if (order != ORD_NODIR) for (ndir = 0; ndir < max; ndir++) {
		if (order != ORD_LOWER)
			ret = _applydir(dirlist[ndir], funcf,
				funcd1, funcd2, order, NULL, verbose);
		else if ((ret = (*funcd1)(dirlist[ndir])) < 0) {
			if (ret == APL_ERROR) warning(-1, dir);
			ret = APL_CANCEL;
		}
		else ret = _applydir(dirlist[ndir], funcf,
			funcd1, funcd2, ORD_NODIR, NULL, verbose);
		if (ret == APL_CANCEL) {
			for (; ndir < max; ndir++) Xfree(dirlist[ndir]);
			Xfree(dirlist);
			return(ret);
		}
		Xfree(dirlist[ndir]);

		if (verbose) {
			Xlocate(0, L_CMDLINE);
			Xputterm(L_CLEAR);
			VOID_C XXcprintf("[%^.*k]", n_column - 2, cp);
			Xtflush();
		}
	}
	Xfree(dirlist);

	if (!(dirp = Xopendir(dir))) warning(-1, dir);
	else {
		while ((dp = Xreaddir(dirp))) {
			if (intrkey(K_ESC)) {
				ret = APL_CANCEL;
				break;
			}
			if (isdotdir(dp -> d_name)) continue;
			if (strcatpath(path, fname, dp -> d_name) < 0)
				continue;

			if (Xlstat(path, &st) < 0) warning(-1, path);
			else if (s_isdir(&st)) continue;
			else if ((ret = (*funcf)(path)) < 0) {
				if (ret == APL_CANCEL) break;
				warning(-1, path);
			}
		}
		VOID_C Xclosedir(dirp);
		if (ret == APL_CANCEL) return(ret);
	}

	destnlink = dupnlink;
	destmode = dupmode;
	destmtime = dupmtime;
	destatime = dupatime;
	if (funcd2 && (ret = (*funcd2)(dir)) < 0) {
		if (ret == APL_CANCEL) return(ret);
		warning(-1, dir);
	}
	if (endmes) {
#ifndef	_NOEXTRACOPY
		if (copyline > 0) _showprogress(n_column);
#endif
		warning(0, endmes);
	}

	return(ret);
}

int applydir(dir, funcf, funcd1, funcd2, order, endmes)
CONST char *dir;
int (*funcf)__P_((CONST char *));
int (*funcd1)__P_((CONST char *));
int (*funcd2)__P_((CONST char *));
int order;
CONST char *endmes;
{
	char path[MAXPATHLEN];
	int verbose;

	verbose = 1;
	if (!dir) {
		dir = curpath;
		verbose = 0;
	}
	else if (isdotdir(dir) == 1) {
		Xrealpath(dir, path, 0);
		dir = path;
	}
#ifdef	DEP_DOSEMU
	else dir = nodospath(path, dir);
#endif

	return(_applydir(dir, funcf, funcd1, funcd2, order, endmes, verbose));
}

/*ARGSUSED*/
int copyfile(arg, tr)
CONST char *arg;
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
# ifdef	DEP_PSEUDOPATH
	destpath = (tr) ? tree(1, &destdrive) : getdestdir(COPYD_K, arg);
# else
	destpath = (tr) ? tree(1, NULL) : getdestdir(COPYD_K, arg);
# endif
#endif	/* !_NOTREE */

	if (!destpath) return((tr) ? FNC_UPDATE : FNC_CANCEL);
	destdir = NULL;
	if (mark > 0
	|| (!isdir(&(filelist[filepos])) || islink(&(filelist[filepos])))) {
		if (preparecopy(0, mark) < 0) {
			Xfree(destpath);
			return(FNC_CANCEL);
		}
		VOID_C applyfile(safecopy, ENDCP_K);
	}
#ifdef	_NOEXTRACOPY
	else if (issamedir(destpath, NULL)) {
		warning(EEXIST, filelist[filepos].name);
		Xfree(destpath);
		return((tr) ? FNC_UPDATE : FNC_CANCEL);
	}
#endif
	else {
		if (preparecopy(1, 1) < 0) {
			Xfree(destpath);
			return(FNC_CANCEL);
		}
		order = (islowerdir()) ? ORD_LOWER : ORD_NORMAL;
		VOID_C applydir(filelist[filepos].name, safecopy,
			cpdir, touchdir, order, ENDCP_K);
	}

	Xfree(destpath);
	Xfree(destdir);
#ifndef	_NOEXTRACOPY
	Xfree(forwardpath);
	forwardpath = NULL;
#endif
#ifdef	DEP_PSEUDOPATH
	shutdrv(destdrive);
	destdrive = -1;
#endif
#if	!defined (_NOEXTRACOPY) && defined (DEP_DOSDRIVE)
	shutdrv(forwarddrive);
	forwarddrive = -1;
#endif

	return(FNC_EFFECT);
}

/*ARGSUSED*/
int movefile(arg, tr)
CONST char *arg;
int tr;
{
#ifdef	DEP_DOSEMU
	char path[MAXPATHLEN];
#endif
	int ret;

	if (!mark && isdotdir(filelist[filepos].name)) {
		Xputterm(T_BELL);
		return(FNC_NONE);
	}
#ifdef	_NOTREE
	destpath = getdestdir(MOVED_K, arg);
#else
# ifdef	DEP_PSEUDOPATH
	destpath = (tr) ? tree(1, &destdrive) : getdestdir(MOVED_K, arg);
# else
	destpath = (tr) ? tree(1, NULL) : getdestdir(MOVED_K, arg);
# endif
#endif	/* !_NOTREE */

	if (!destpath) return((tr) ? FNC_UPDATE : FNC_CANCEL);
	if (issamedir(destpath, NULL)) {
		Xfree(destpath);
		return((tr) ? FNC_UPDATE : FNC_CANCEL);
	}
	destdir = NULL;
	if (mark > 0) {
		if (preparemove(0, mark) < 0) {
			Xfree(destpath);
			return(FNC_CANCEL);
		}
		filepos = applyfile(safemove, ENDMV_K);
	}
	else if (islowerdir()) warning(EINVAL, filelist[filepos].name);
	else {
#ifndef	_NOEXTRACOPY
		if (!isdir(&(filelist[filepos]))
		|| islink(&(filelist[filepos])))
			ret = 0;
		else ret = isxdev(fnodospath(path, filepos), NULL);
		if (ret < 0) warning(-1, filelist[filepos].name);
		else if (ret) {
			if (preparemove(1, 1) < 0) {
				Xfree(destpath);
				return(FNC_CANCEL);
			}
# ifdef	DEP_DOSDRIVE
			if (dospath3(nullstr)) waitmes();
# endif
			ret = applydir(filelist[filepos].name, safemove,
				mvdir1, mvdir2, ORD_NORMAL, ENDMV_K);
		}
		else
#endif	/* !_NOEXTRACOPY */
		{
			if (preparemove(0, 1) < 0) {
				Xfree(destpath);
				return(FNC_CANCEL);
			}
			ret = safemove(fnodospath(path, filepos));
			if (ret == APL_ERROR)
				warning(-1, filelist[filepos].name);
		}
		if (ret == APL_OK) filepos++;
	}

	if (filepos >= maxfile) filepos = maxfile - 1;
	Xfree(destpath);
	Xfree(destdir);
#ifndef	_NOEXTRACOPY
	Xfree(forwardpath);
	forwardpath = NULL;
#endif
#ifdef	DEP_PSEUDOPATH
	shutdrv(destdrive);
	destdrive = -1;
#endif
#if	!defined (_NOEXTRACOPY) && defined (DEP_DOSDRIVE)
	shutdrv(forwarddrive);
	forwarddrive = -1;
#endif

	return(FNC_EFFECT);
}

static int forcecpfile(path)
CONST char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];

	if (getdestpath(path, dest, &st) < 0) return(APL_OK);
	if (safecpfile(path, dest, &st, NULL) < 0) return(APL_ERROR);
#ifdef	HAVEFLAGS
	st.st_flags = (u_long)-1;
#endif

	st.st_nlink = (TCH_MODE | TCH_UID | TCH_GID
		| TCH_ATIME | TCH_MTIME | TCH_IGNOREERR);
	if (touchfile(dest, &st) < 0) return(APL_ERROR);

	return(APL_OK);
}

static int forcecpdir(path)
CONST char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];
	int mode;

	if (isdotdir(path)) return(APL_OK);
	if (getdestpath(path, dest, &st) < 0) return(APL_OK);
	mode = ((st.st_mode & 0777) | S_IWUSR);
	if (Xmkdir(dest, mode) >= 0) return(APL_OK);
	if (errno != EEXIST) return(APL_ERROR);
	if (stat2(dest, &st) >= 0) {
		if (s_isdir(&st)) return(APL_OK);
		if (Xunlink(dest) >= 0 && Xmkdir(dest, mode) >= 0)
			return(APL_OK);
	}

	errno = EEXIST;

	return(APL_ERROR);
}

static int forcetouchdir(path)
CONST char *path;
{
	struct stat st;
	char dest[MAXPATHLEN];

	if (isdotdir(path)) return(APL_OK);
	if (getdestpath(path, dest, &st) < 0) return(APL_CANCEL);
#ifdef	HAVEFLAGS
	st.st_flags = (u_long)-1;
#endif

	st.st_nlink = (TCH_MODE | TCH_UID | TCH_GID
		| TCH_ATIME | TCH_MTIME | TCH_IGNOREERR);
	if (touchfile(dest, &st) < 0) return(APL_ERROR);

	return(APL_OK);
}

int forcemovefile(dest)
char *dest;
{
	int ret;

	destpath = dest;
	ret = applydir(NULL, forcecpfile,
		forcecpdir, forcetouchdir, ORD_NORMAL, NULL);
	destpath = NULL;

	return(ret);
}
