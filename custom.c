/*
 *	custom.c
 *
 *	Customizer of configurations
 */

#include <fcntl.h>
#include "fd.h"
#include "term.h"
#include "func.h"
#include "funcno.h"
#include "kctype.h"
#include "kanji.h"

#ifdef	_NOORIGSHELL
#define	getshellvar(n, l)	getenv(n)
#else
#include "system.h"
#endif

extern int sorttype;
extern int displaymode;
#ifndef	_NOTREE
extern int sorttree;
#endif
#ifndef	_NOWRITEFS
extern int writefs;
#endif
#if	!MSDOS
# ifndef	_NOEXTRACOPY
extern int inheritcopy;
# endif
extern int adjtty;
#endif
extern int defcolumns;
extern int minfilename;
extern short histsize[];
extern int savehist;
#ifndef	_NOTREE
extern int dircountlimit;
#endif
#ifndef	_NODOSDRIVE
extern int dosdrive;
#endif
extern int showsecond;
extern int sizeinfo;
#ifndef	_NOCOLOR
extern int ansicolor;
#endif
#ifndef	_NOEDITMODE
extern char *editmode;
#endif
extern char *deftmpdir;
#ifndef	_NOROCKRIDGE
extern char *rockridgepath;
#endif
#ifndef	_NOPRECEDE
extern char *precedepath;
#endif
extern char *promptstr;
#ifndef	_NOORIGSHELL
extern char *promptstr2;
#endif
#if	!MSDOS && !defined (_NOKANJIFCONV)
extern char *sjispath;
extern char *eucpath;
extern char *jis7path;
extern char *jis8path;
extern char *junetpath;
extern char *ojis7path;
extern char *ojis8path;
extern char *ojunetpath;
extern char *hexpath;
extern char *cappath;
extern char *utf8path;
extern char *noconvpath;
#endif	/* !MSDOS && !_NOKANJIFCONV */
extern int tmpumask;
#ifndef	_NOCUSTOMIZE
extern int curcolumns;
extern int subwindow;
extern int win_x;
extern int win_y;
extern int calc_x;
extern int calc_y;
extern functable funclist[];
extern char *macrolist[];
extern int maxmacro;
extern char *helpindex[];
extern char **orighelpindex;
extern bindtable bindlist[];
extern bindtable *origbindlist;
extern strtable keyidentlist[];
# if	!MSDOS && !defined (_NOKEYMAP)
extern keymaptable *origkeymaplist;
# endif
# ifndef	_NOARCHIVE
extern launchtable launchlist[];
extern launchtable *origlaunchlist;
extern int maxlaunch;
extern int origmaxlaunch;
extern archivetable archivelist[];
extern archivetable *origarchivelist;
extern int maxarchive;
extern int origmaxarchive;
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
extern devinfo fdtype[];
extern devinfo *origfdtype;
# endif
extern int inruncom;
#endif	/* !_NOCUSTOMIZE */

#define	MAXCUSTOM	7
#define	MAXCUSTNAM	14
#define	MAXCUSTVAL	(n_column - MAXCUSTNAM - 3)
#define	inputcuststr(p, d, s, h) \
			(inputstr(p, d, ((s) ? strlen(s) : 0), s, h))
#define	noselect(n, m, x, s, v) \
			(selectstr(n, m, x, s, v) == K_ESC)
#define	MAXSAVEMENU	5
#define	MAXTNAMLEN	8
#ifndef	O_BINARY
#define	O_BINARY	0
#endif

typedef struct _envtable {
	char *env;
	VOID_P var;
	char *def;
#ifndef	_NOJPNMES
	char *hmes;
#endif
#ifndef	_NOENGMES
	char *hmes_eng;
#endif
	u_char type;
} envtable;

#define	T_BOOL		0
#define	T_SHORT		1
#define	T_INT		2
#define	T_NATURAL	3
#define	T_CHARP		4
#define	T_PATH		5
#define	T_PATHS		6
#define	T_SORT		7
#define	T_DISP		8
#define	T_WRFS		9
#define	T_COLUMN	10
#define	T_DDRV		11
#define	T_COLOR		12
#define	T_EDIT		13
#define	T_KIN		14
#define	T_KOUT		15
#define	T_KNAM		16
#define	T_OCTAL		17

static int NEAR atooctal __P_((char *));
static VOID NEAR _evalenv __P_((int));
#ifndef	_NOCUSTOMIZE
static VOID NEAR custtitle __P_((VOID_A));
static VOID NEAR calcmax __P_((int [], int));
static VOID NEAR envcaption __P_((char *));
static char **NEAR copyenv __P_((char **));
static VOID NEAR cleanupenv __P_((VOID_A));
static char *NEAR ascoctal __P_((int, char *));
static int NEAR dispenv __P_((int));
static int NEAR editenv __P_((int));
static int NEAR dumpenv __P_((char *, FILE *));
# ifndef	_NOORIGSHELL
static int NEAR checkenv __P_((char *, char **, int *, FILE *));
# endif
static VOID NEAR cleanupbind __P_((VOID_A));
static int NEAR dispbind __P_((int));
static int NEAR selectbind __P_((int, int, char *));
static int NEAR editbind __P_((int));
static int NEAR dumpbind __P_((char *, FILE *));
# ifndef	_NOORIGSHELL
static int NEAR checkbind __P_((char *, int, char **, FILE *));
# endif
# if	!MSDOS && !defined (_NOKEYMAP)
static VOID NEAR cleanupkeymap __P_((VOID_A));
static int NEAR dispkeymap __P_((int));
static int NEAR editkeymap __P_((int));
static int NEAR searchkeymap __P_((keymaptable *, int));
static int NEAR dumpkeymap __P_((char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkkeymap __P_((char *, int, char **, FILE *));
#  endif
# endif
# ifndef	_NOARCHIVE
static VOID NEAR cleanuplaunch __P_((VOID_A));
static int NEAR displaunch __P_((int));
static int NEAR editlaunch __P_((int));
static int NEAR searchlaunch __P_((launchtable *, int, launchtable *));
static int NEAR dumplaunch __P_((char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checklaunch __P_((char *, int, char **, FILE *));
#  endif
static VOID NEAR cleanuparch __P_((VOID_A));
static int NEAR disparch __P_((int));
static int NEAR editarch __P_((int));
static int NEAR searcharch __P_((archivetable *, int, archivetable *));
static int NEAR dumparch __P_((char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkarch __P_((char *, int, char **, FILE *));
#  endif
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
static VOID NEAR cleanupdrive __P_((VOID_A));
static int NEAR dispdrive __P_((int));
static int NEAR editdrive __P_((int));
static int NEAR dumpdrive __P_((char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkdrive __P_((char *, int, char **, FILE *));
#  endif
# endif
static int NEAR dispsave __P_((int));
# ifndef	_NOORIGSHELL
static int NEAR overwriteconfig __P_((int *, char *));
# endif	/* !_NOORIGSHELL */
static int NEAR editsave __P_((int));
static VOID NEAR dispname __P_((int, int, int));
static VOID NEAR dispcust __P_((VOID_A));
static VOID NEAR movecust __P_((int, int));
static int NEAR editcust __P_((VOID_A));
#endif	/* !_NOCUSTOMIZE */

#ifndef	_NOCUSTOMIZE
int custno = -1;
#endif

static envtable envlist[] = {
	{"FD_SORTTYPE", &sorttype, (char *)SORTTYPE, STTP_E, T_SORT},
	{"FD_DISPLAYMODE", &displaymode, (char *)DISPLAYMODE, DPMD_E, T_DISP},
#ifndef	_NOTREE
	{"FD_SORTTREE", &sorttree, (char *)SORTTREE, STTR_E, T_BOOL},
#endif
#ifndef	_NOWRITEFS
	{"FD_WRITEFS", &writefs, (char *)WRITEFS, WRFS_E, T_WRFS},
#endif
#if	!MSDOS
# if	FD >= 2
	{"FD_IGNORECASE", &pathignorecase, (char *)IGNORECASE, IGNC_E, T_BOOL},
# endif
# ifndef	_NOEXTRACOPY
	{"FD_INHERITCOPY", &inheritcopy, (char *)INHERITCOPY, IHTM_E, T_BOOL},
# endif
	{"FD_ADJTTY", &adjtty, (char *)ADJTTY, AJTY_E, T_BOOL},
# if	FD >= 2
	{"FD_USEGETCURSOR", &usegetcursor,
		(char *)USEGETCURSOR, UGCS_E, T_BOOL},
# endif
#endif
	{"FD_DEFCOLUMNS", &defcolumns, (char *)DEFCOLUMNS, CLMN_E, T_COLUMN},
	{"FD_MINFILENAME", &minfilename,
		(char *)MINFILENAME, MINF_E, T_NATURAL},
	{"FD_HISTSIZE", &(histsize[0]), (char *)HISTSIZE, HSSZ_E, T_SHORT},
	{"FD_DIRHIST", &(histsize[1]), (char *)DIRHIST, DRHS_E, T_SHORT},
	{"FD_SAVEHIST", &savehist, (char *)SAVEHIST, SVHS_E, T_INT},
#ifndef	_NOTREE
	{"FD_DIRCOUNTLIMIT", &dircountlimit,
		(char *)DIRCOUNTLIMIT, DCLM_E, T_INT},
#endif
#ifndef	_NODOSDRIVE
	{"FD_DOSDRIVE", &dosdrive, (char *)DOSDRIVE, DOSD_E, T_DDRV},
#endif
	{"FD_SECOND", &showsecond, (char *)SECOND, SCND_E, T_BOOL},
	{"FD_SIZEINFO", &sizeinfo, (char *)SIZEINFO, SZIF_E, T_BOOL},
#ifndef	_NOCOLOR
	{"FD_ANSICOLOR", &ansicolor, (char *)ANSICOLOR, ACOL_E, T_COLOR},
#endif
#ifndef	_NOEDITMODE
	{"FD_EDITMODE", &editmode, EDITMODE, EDMD_E, T_EDIT},
#endif
	{"FD_TMPDIR", &deftmpdir, TMPDIR, TMPD_E, T_PATH},
#ifndef	_NOROCKRIDGE
	{"FD_RRPATH", &rockridgepath, RRPATH, RRPT_E, T_PATHS},
#endif
#ifndef	_NOPRECEDE
	{"FD_PRECEDEPATH", &precedepath, PRECEDEPATH, PCPT_E, T_PATHS},
#endif
#if	FD >= 2
	{"FD_PS1", &promptstr, PROMPT, PRMP_E, T_CHARP},
#else
	{"FD_PROMPT", &promptstr, PROMPT, PRMP_E, T_CHARP},
#endif
#ifndef	_NOORIGSHELL
	{"FD_PS2", &promptstr2, PROMPT2, PRM2_E, T_CHARP},
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
	{"FD_LANGUAGE", &outputkcode, (char *)NOCNV, LANG_E, T_KOUT},
#endif
#if	!MSDOS && !defined (_NOKANJICONV)
	{"FD_INPUTKCODE", &inputkcode, (char *)NOCNV, IPKC_E, T_KIN},
#endif
#if	!MSDOS && !defined (_NOKANJIFCONV)
	{"FD_FNAMEKCODE", &fnamekcode, (char *)NOCNV, FNKC_E, T_KNAM},
	{"FD_SJISPATH", &sjispath, SJISPATH, SJSP_E, T_PATHS},
	{"FD_EUCPATH", &eucpath, EUCPATH, EUCP_E, T_PATHS},
	{"FD_JISPATH", &jis7path, JISPATH, JISP_E, T_PATHS},
	{"FD_JIS8PATH", &jis8path, JIS8PATH, JS8P_E, T_PATHS},
	{"FD_JUNETPATH", &junetpath, JUNETPATH, JNTP_E, T_PATHS},
	{"FD_OJISPATH", &ojis7path, OJISPATH, OJSP_E, T_PATHS},
	{"FD_OJIS8PATH", &ojis8path, OJIS8PATH, OJ8P_E, T_PATHS},
	{"FD_OJUNETPATH", &ojunetpath, OJUNETPATH, OJNP_E, T_PATHS},
	{"FD_HEXPATH", &hexpath, JISPATH, HEXP_E, T_PATHS},
	{"FD_CAPPATH", &cappath, CAPPATH, CAPP_E, T_PATHS},
	{"FD_UTF8PATH", &utf8path, UTF8PATH, UTF8P_E, T_PATHS},
	{"FD_NOCONVPATH", &noconvpath, NOCONVPATH, NCVP_E, T_PATHS},
#endif	/* !MSDOS && !_NOKANJIFCONV */
#if	0
	{"FD_TMPUMASK", &tmpumask, (char *)TMPUMASK, TUMSK_E, T_OCTAL},
#endif
};
#define	ENVLISTSIZ	((int)(sizeof(envlist) / sizeof(envtable)))

#ifndef	_NOCUSTOMIZE
# if	!MSDOS && !defined (_NODOSDRIVE)
static devinfo mediadescr[] = {
	{0xf0, "2HD(PC/AT)", 2, 18, 80},
	{0xf9, "2HD(PC98)", 2, 15, 80},
	{0xf9, "2DD(PC/AT)", 2, 9, 80},
	{0xfb, "2DD(PC98)", 2, 8 + 100, 80},
#  ifdef	HDDMOUNT
	{0xf8, "HDD", 'n', 0, 0},
	{0xf8, "HDD(PC98)", 'N', 98, 0},
#  endif
};
#define	MEDIADESCRSIZ	((int)(sizeof(mediadescr) / sizeof(devinfo)))
# endif	/* !MSDOS && !_NODOSDRIVE */
static int cs_item = 0;
static int cs_max = 0;
static int cs_row = 0;
static int *cs_len = NULL;
static char **tmpenvlist = NULL;
static char **tmpmacrolist = NULL;
static int tmpmaxmacro = 0;
static char **tmphelpindex = NULL;
static bindtable *tmpbindlist = NULL;
# if	!MSDOS && !defined (_NOKEYMAP)
static keymaptable *tmpkeymaplist = NULL;
# endif
# ifndef	_NOARCHIVE
static launchtable *tmplaunchlist = NULL;
static int tmpmaxlaunch = 0;
static archivetable *tmparchivelist = NULL;
static int tmpmaxarchive = 0;
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
static devinfo *tmpfdtype = NULL;
# endif
#endif	/* !_NOCUSTOMIZE */


static int NEAR atooctal(s)
char *s;
{
	int n;

	if (!s) return(-1);
	n = 0;
	while (*s) {
		if (*s < '0' || *s > '7') break;
		n = (n << 3) + *s - '0';
		s++;
	}
	if (*s) return(-1);
	n &= 0777;
	return(n);
}

static VOID NEAR _evalenv(no)
int no;
{
	char *cp;
	int n;

	cp = getenv2(envlist[no].env);
	switch (envlist[no].type) {
		case T_DDRV:
#if	MSDOS
			if (!cp) n = (int)(envlist[no].def);
			else {
				char *dupl;

				n = 0;
				dupl = strdup2(cp);
				if ((cp = strchr(dupl, ','))) {
					*(cp++) = '\0';
					if (!strcmp(cp, "BIOS")) n = 2;
				}
				if (*dupl && atoi2(dupl)) n |= 1;
				free(dupl);
			}
			*((int *)(envlist[no].var)) = n;
			break;
#endif
		case T_BOOL:
			if (!cp) n = (int)(envlist[no].def);
			else if (!*cp || !atoi2(cp)) n = 0;
			else n = 1;
			*((int *)(envlist[no].var)) = n;
			break;
		case T_SHORT:
			if ((n = atoi2(cp)) < 0) n = (int)(envlist[no].def);
			*((short *)(envlist[no].var)) = n;
			break;
		case T_INT:
			if ((n = atoi2(cp)) < 0) n = (int)(envlist[no].def);
			*((int *)(envlist[no].var)) = n;
			break;
		case T_NATURAL:
			if ((n = atoi2(cp)) <= 0) n = (int)(envlist[no].def);
			*((int *)(envlist[no].var)) = n;
			break;
		case T_PATH:
			if (!cp) cp = envlist[no].def;
			cp = evalpath(strdup2(cp), 1);
			if (*((char **)(envlist[no].var)))
				free(*((char **)(envlist[no].var)));
			*((char **)(envlist[no].var)) = cp;
			break;
		case T_PATHS:
			if (!cp) cp = envlist[no].def;
			cp = evalpaths(cp, ':');
			if (*((char **)(envlist[no].var)))
				free(*((char **)(envlist[no].var)));
			*((char **)(envlist[no].var)) = cp;
			break;
		case T_SORT:
			if (((n = atoi2(cp)) < 0 || (n & 7) > 5)
			&& (n < 100 || ((n - 100) & 7) > 5))
				n = (int)(envlist[no].def);
			*((int *)(envlist[no].var)) = n;
			break;
		case T_DISP:
#ifdef	HAVEFLAGS
			if ((n = atoi2(cp)) < 0 || n > 15)
#else
			if ((n = atoi2(cp)) < 0 || n > 7)
#endif
				n = (int)(envlist[no].def);
			*((int *)(envlist[no].var)) = n;
			break;
		case T_WRFS:
			if ((n = atoi2(cp)) < 0 || n > 2)
				n = (int)(envlist[no].def);
			*((int *)(envlist[no].var)) = n;
			break;
		case T_COLUMN:
			if ((n = atoi2(cp)) < 0 || n > 5 || n == 4)
				n = (int)(envlist[no].def);
			*((int *)(envlist[no].var)) = n;
			break;
		case T_COLOR:
			if ((n = atoi2(cp)) < 0 || n > 3)
				n = (int)(envlist[no].def);
			*((int *)(envlist[no].var)) = n;
			break;
#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
		case T_KIN:
		case T_KOUT:
		case T_KNAM:
			*((int *)(envlist[no].var)) =
				getlang(cp, envlist[no].type - T_KIN);
			break;
#endif	/* (!MSDOS && !_NOKANJICONV) || (!_NOENGMES && !NOJPNMES) */
		case T_OCTAL:
			if ((n = atooctal(cp)) < 0) n = (int)(envlist[no].def);
			*((int *)(envlist[no].var)) = n;
			break;
		default:
			if (!cp) cp = envlist[no].def;
			*((char **)(envlist[no].var)) = cp;
			break;
	}
}

VOID evalenv(VOID_A)
{
	int i;

	for (i = 0; i < ENVLISTSIZ; i++) _evalenv(i);
}

#ifdef	DEBUG
VOID freeenvpath(VOID_A)
{
	int i;

	for (i = 0; i < ENVLISTSIZ; i++) switch (envlist[i].type) {
		case T_PATH:
		case T_PATHS:
			if (*((char **)(envlist[i].var)))
				free(*((char **)(envlist[i].var)));
			*((char **)(envlist[i].var)) = NULL;
			break;
		default:
			break;
	}
}
#endif

#ifndef	_NOCUSTOMIZE
static VOID NEAR custtitle(VOID_A)
{
	char *str[MAXCUSTOM];
	int i;

	str[0] = TENV_K;
	str[1] = TBIND_K;
	str[2] = TKYMP_K;
	str[3] = TLNCH_K;
	str[4] = TARCH_K;
	str[5] = TSDRV_K;
	str[6] = TSAVE_K;

	locate(0, LFILETOP);
	for (i = 0; i < MAXCUSTOM; i++) {
		if (i != custno) {
			putch2('/');
			kanjiputs(str[i]);
		}
		else {
			putterm(t_standout);
			putch2('/');
			kanjiputs(str[i]);
			putterm(end_standout);
		}
		putch2(' ');
	}
	putterm(l_clear);

	locate(0, LFILETOP + 1);
	putterm(t_standout);
	kanjiputs2("", n_column, 0);
	putterm(end_standout);
}

static VOID NEAR calcmax(max, new)
int max[], new;
{
	int i;

	max[0] = ENVLISTSIZ;
	for (i = 0; bindlist[i].key >= 0; i++);
	max[1] = i + new;
# if	MSDOS || defined (_NOKEYMAP)
	max[2] = new;
# else
	for (i = 0; tmpkeymaplist[i].key > 0; i++);
	max[2] = i;
# endif
# ifdef	_NOARCHIVE
	max[3] = new;
	max[4] = new;
# else
	max[3] = maxlaunch + new;
	max[4] = maxarchive + new;
# endif
# if	MSDOS || defined (_NODOSDRIVE)
	max[5] = new;
# else
	for (i = 0; fdtype[i].name; i++);
	max[5] = i + new;
# endif
	max[6] = (new) ? MAXSAVEMENU : 0;
}

static VOID NEAR envcaption(s)
char *s;
{
	locate(0, L_HELP);
	putterm(l_clear);
	putterm(t_standout);
	kanjiputs(s);
	putterm(end_standout);
}

VOID freestrarray(list, max)
char **list;
int max;
{
	int i;

	for (i = 0; i < max; i++) free(list[i]);
}

char **copystrarray(dest, src, ndestp, nsrc)
char **dest, **src;
int *ndestp, nsrc;
{
	int i;

	if (dest) freestrarray(dest, (ndestp) ? *ndestp : nsrc);
	else if (nsrc > 0) dest = (char **)malloc2(nsrc * sizeof(char *));
	for (i = 0; i < nsrc; i++) dest[i] = strdup2(src[i]);
	if (ndestp) *ndestp = nsrc;
	return(dest);
}

static char **NEAR copyenv(list)
char **list;
{
	int i;

	if (list) {
		for (i = 0; i < ENVLISTSIZ; i++) {
			setenv2(envlist[i].env, list[i * 2]);
			setenv2(&(envlist[i].env[3]), list[i * 2 + 1]);
			_evalenv(i);
		}
	}
	else {
		list = (char **)malloc2(ENVLISTSIZ * 2 * sizeof(char *));
		for (i = 0; i < ENVLISTSIZ; i++) {
			list[i * 2] = strdup2(getshellvar(envlist[i].env, -1));
			list[i * 2 + 1] =
				strdup2(getshellvar(&(envlist[i].env[3]), -1));
		}
	}
	return(list);
}

static VOID NEAR cleanupenv(VOID_A)
{
	char *cp;
	int i;

	for (i = 0; i < ENVLISTSIZ; i++) {
		setenv2(envlist[i].env, NULL);
		cp = (envlist[i].type == T_CHARP) ? envlist[i].def : NULL;
		setenv2(&(envlist[i].env[3]), cp);
		_evalenv(i);
	}
}

static char *NEAR ascoctal(n, buf)
int n;
char *buf;
{
	int i;

# ifdef	BASHSTYLE
	i = 3;
# else
	i = 4;
# endif
	buf[i] = '\0';
	while (--i >= 0) {
		buf[i] = '0' + (n & 7);
		n >>= 3;
	}
	return(buf);
}

static int NEAR dispenv(no)
int no;
{
	char *cp, *tmp, buf[MAXLINESTR + 1], *str[MAXSELECTSTRS];
	int n, p;

	cp = getenv2(envlist[no].env);
	switch (envlist[no].type) {
		case T_BOOL:
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			cp = str[*((int *)(envlist[no].var))];
			break;
		case T_SHORT:
			cp = ascnumeric(buf, *((short *)(envlist[no].var)),
				0, MAXLINESTR);
			break;
		case T_INT:
		case T_NATURAL:
		case T_COLUMN:
			cp = ascnumeric(buf, *((int *)(envlist[no].var)),
				0, MAXLINESTR);
			break;
		case T_SORT:
			n = *((int *)(envlist[no].var));
			str[0] = ORAW_K;
			str[1] = ONAME_K;
			str[2] = OEXT_K;
			str[3] = OSIZE_K;
			str[4] = ODATE_K;
			str[5] = OLEN_K;
			p = (n >= 100) ? 1 : 0;
			n %= 100;
			tmp = strcpy2(buf, &(str[n & 7][3]));

			if (n & 7) {
				str[0] = OINC_K;
				str[1] = ODEC_K;
				tmp = strcpy2(tmp, "(");
				tmp = strcpy2(tmp, &(str[n / 8][3]));
				tmp = strcpy2(tmp, ")");
			}
			if (p) strcpy(strcpy2(tmp, ", "), VSORT_K);
			cp = buf;
			break;
		case T_DISP:
			n = *((int *)(envlist[no].var));
			tmp = strcpy2(buf, VDS1A_K);
			str[0] = VDS10_K;
			str[1] = VDS11_K;
			tmp = strcpy2(tmp, str[n & 1]);
			tmp = strcpy2(strcpy2(tmp, ", "), VDS2A_K);
			str[0] = VDS20_K;
			str[1] = VDS21_K;
			tmp = strcpy2(tmp, str[(n >> 1) & 1]);
			tmp = strcpy2(strcpy2(tmp, ", "), VDS3A_K);
			str[0] = VDS30_K;
			str[1] = VDS31_K;
			tmp = strcpy2(tmp, str[(n >> 2) & 1]);
# ifdef	HAVEFLAGS
			tmp = strcpy2(strcpy2(tmp, ", "), VDS4A_K);
			str[0] = VDS40_K;
			str[1] = VDS41_K;
			strcpy(tmp, str[(n >> 3) & 1]);
# endif
			cp = buf;
			break;
		case T_WRFS:
			str[0] = VWFS0_K;
			str[1] = VWFS1_K;
			str[2] = VWFS2_K;
			cp = str[*((int *)(envlist[no].var))];
			break;
		case T_DDRV:
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
# if	MSDOS
			tmp = strcpy2(buf, str[*((int *)(envlist[no].var))]);
			if (cp && (cp = strchr(cp, ','))
			&& !strcmp(cp + 1, "BIOS")) strcpy(tmp, cp);
			cp = buf;
# else
			cp = str[*((int *)(envlist[no].var))];
# endif
			break;
		case T_COLOR:
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			str[2] = VCOL2_K;
			str[3] = VCOL3_K;
			cp = str[*((int *)(envlist[no].var))];
			break;
# if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
		case T_KIN:
		case T_KOUT:
		case T_KNAM:
			str[NOCNV] = VNCNV_K;
			str[ENG] = VENG_K;
#  if	!MSDOS && !defined (_NOKANJICONV)
			str[SJIS] = "ShiftJIS";
			str[EUC] = "EUC-JP";
			str[JIS7] = "7bit-JIS";
			str[O_JIS7] = "old 7bit-JIS";
			str[JIS8] = "8bit-JIS";
			str[O_JIS8] = "old 8bit-JIS";
			str[JUNET] = "ISO-2022-JP";
			str[O_JUNET] = "old ISO-2022-JP";
			str[HEX] = "HEX";
			str[CAP] = "CAP";
			str[UTF8] = "UTF-8";
#  endif	/* !MSDOS && !_NOKANJICONV */
			cp = str[*((int *)(envlist[no].var))];
			break;
# endif	/* (!MSDOS && !_NOKANJICONV) || (!_NOENGMES && !NOJPNMES) */
		case T_OCTAL:
			cp = ascoctal(*((int *)(envlist[no].var)), buf);
			break;
		default:
			cp = *((char **)(envlist[no].var));
			if (!cp) cp = "<null>";
			break;
	}
	kanjiputs2(cp, MAXCUSTVAL, 0);
	return(strlen3(cp));
}

static int NEAR editenv(no)
int no;
{
	char *cp, buf[MAXLINESTR + 1], *str[MAXSELECTSTRS];
	int n, p, tmp, val[MAXSELECTSTRS];

	for (n = 0; n < MAXSELECTSTRS; n++) val[n] = n;

	switch (envlist[no].type) {
		case T_BOOL:
		case T_DDRV:
			n = *((int *)(envlist[no].var));
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 2, 0, str, val)) return(0);
			ascnumeric(buf, n, 0, MAXLINESTR);
			break;
		case T_SHORT:
			ascnumeric(buf, *((short *)(envlist[no].var)),
				0, MAXLINESTR);
			cp = inputcuststr(&(envlist[no].env[3]), 1, buf, -1);
			if (!cp) return(0);
			n = atoi2(cp);
			free(cp);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			ascnumeric(buf, n, 0, MAXLINESTR);
			break;
		case T_INT:
		case T_NATURAL:
			ascnumeric(buf, *((int *)(envlist[no].var)),
				0, MAXLINESTR);
			cp = inputcuststr(&(envlist[no].env[3]), 1, buf, -1);
			if (!cp) return(0);
			n = atoi2(cp);
			free(cp);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			ascnumeric(buf, n, 0, MAXLINESTR);
			break;
		case T_PATH:
			cp = inputcuststr(&(envlist[no].env[3]), 1,
				*((char **)(envlist[no].var)), 1);
			if (!cp) return(0);
			strcpy(buf, cp);
			free(cp);
			break;
		case T_SORT:
			n = *((int *)(envlist[no].var));
			str[0] = ONAME_K;
			str[1] = OEXT_K;
			str[2] = OSIZE_K;
			str[3] = ODATE_K;
			str[4] = OLEN_K;
			str[5] = ORAW_K;
			val[0] = 1;
			val[1] = 2;
			val[2] = 3;
			val[3] = 4;
			val[4] = 5;
			val[5] = 0;
			p = (n >= 100) ? 100 : 0;
			n %= 100;
			tmp = n & ~7;
			n &= 7;
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 6, 0, str, val)) return(0);
			if (n) {
				str[0] = OINC_K;
				str[1] = ODEC_K;
				val[0] = 0;
				val[1] = 8;
				if (noselect(&tmp, 2, 56, str, val)) return(0);
				n += tmp;
			}
			envcaption(VSORT_K);
			str[0] = VSRT0_K;
			str[1] = VSRT1_K;
			val[0] = 0;
			val[1] = 100;
			if (noselect(&p, 2, 0, str, val)) return(0);
			n += p;
			ascnumeric(buf, n, 0, MAXLINESTR);
			break;
		case T_DISP:
			n = *((int *)(envlist[no].var));
			envcaption(VDS1B_K);
			str[0] = VDS10_K;
			str[1] = VDS11_K;
			tmp = n & 1;
			if (noselect(&tmp, 2, 0, str, val)) return(0);
			n = (n & ~1) | tmp;
			envcaption(VDS2B_K);
			str[0] = VDS20_K;
			str[1] = VDS21_K;
			tmp = (n & 2) >> 1;
			if (noselect(&tmp, 2, 0, str, val)) return(0);
			n = (n & ~2) | (tmp << 1);
			envcaption(VDS3B_K);
			str[0] = VDS30_K;
			str[1] = VDS31_K;
			tmp = (n & 4) >> 2;
			if (noselect(&tmp, 2, 0, str, val)) return(0);
			n = (n & ~4) | (tmp << 2);
# ifdef	HAVEFLAGS
			envcaption(VDS4B_K);
			str[0] = VDS40_K;
			str[1] = VDS41_K;
			tmp = (n & 8) >> 3;
			if (noselect(&tmp, 2, 0, str, val)) return(0);
			n = (n & ~8) | (tmp << 3);
# endif
			ascnumeric(buf, n, 0, MAXLINESTR);
			break;
		case T_WRFS:
			n = *((int *)(envlist[no].var));
			str[0] = VWFS0_K;
			str[1] = VWFS1_K;
			str[2] = VWFS2_K;
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 3, 0, str, val)) return(0);
			ascnumeric(buf, n, 0, MAXLINESTR);
			break;
		case T_COLUMN:
			n = *((int *)(envlist[no].var));
			str[0] = "1";
			str[1] = "2";
			str[2] = "3";
			str[3] = "5";
			val[0] = 1;
			val[1] = 2;
			val[2] = 3;
			val[3] = 5;
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 4, 0, str, val)) return(0);
			ascnumeric(buf, n, 0, MAXLINESTR);
			break;
		case T_COLOR:
			n = *((int *)(envlist[no].var));
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			str[2] = VCOL2_K;
			str[3] = VCOL3_K;
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 4, 0, str, val)) return(0);
			ascnumeric(buf, n, 0, MAXLINESTR);
			break;
		case T_EDIT:
			cp = *((char **)(envlist[no].var));
			str[0] = "emacs";
			str[1] = "wordstar";
			str[2] = "vi";
			if (!cp) n = 0;
			else for (n = 2; n > 0; n--)
				if (!strcmp(cp, str[n])) break;
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 3, 0, str, val)) return(0);
			strcpy(buf, str[n]);
			break;
# if	!MSDOS && !defined (_NOKANJICONV)
		case T_KIN:
			n = *((int *)(envlist[no].var));
			str[0] = "ShiftJIS";
			str[1] = "EUC-JP";
			val[0] = SJIS;
			val[1] = EUC;
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 2, 0, str, val)) return(0);
			str[SJIS] = "sjis";
			str[EUC] = "euc";
			strcpy(buf, str[n]);
			break;
# endif	/* !MSDOS && !_NOKANJICONV */
# if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
		case T_KOUT:
			n = *((int *)(envlist[no].var));
			str[0] = VNCNV_K;
			str[1] = VENG_K;
			val[0] = NOCNV;
			val[1] = ENG;
#  if	MSDOS || defined (_NOKANJICONV)
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 2, 0, str, val)) return(0);
#  else	/* !MSDOS && !_NOKANJICONV */

			str[2] = "SJIS";
			str[3] = "EUC";
			str[4] = "7bit-JIS";
			str[5] = "8bit-JIS";
			str[6] = "ISO-2022-JP";
			str[7] = "UTF-8";
			val[2] = SJIS;
			val[3] = EUC;
			val[4] = JIS7;
			val[5] = JIS8;
			val[6] = JUNET;
			val[7] = UTF8;
			if (n != O_JIS7 && n != O_JIS8 && n != O_JUNET) p = 0;
			else {
				p = 1;
				n--;
			}
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 8, 0, str, val)) return(0);
			if (n >= JIS7 && n <= JUNET) {
				str[0] = VNJIS_K;
				str[1] = VOJIS_K;
				val[0] = 0;
				val[1] = 1;
				if (noselect(&p, 2, 64, str, val)) return(0);
				n += p;
			}
			str[SJIS] = "sjis";
			str[EUC] = "euc";
			str[JIS7] = "jis7";
			str[O_JIS7] = "ojis7";
			str[JIS8] = "jis8";
			str[O_JIS8] = "ojis8";
			str[JUNET] = "junet";
			str[O_JUNET] = "ojunet";
			str[UTF8] = "utf8";
#  endif	/* !MSDOS && !_NOKANJICONV */
			str[NOCNV] = "";
			str[ENG] = "C";
			strcpy(buf, str[n]);
			break;
# endif	/* (!MSDOS && !_NOKANJICONV) || (!_NOENGMES && !NOJPNMES) */
# if	!MSDOS && !defined (_NOKANJIFCONV)
		case T_KNAM:
			n = *((int *)(envlist[no].var));
			str[0] = VNCNV_K;
			str[1] = "SJIS";
			str[2] = "EUC";
			str[3] = "7bit-JIS";
			str[4] = "8bit-JIS";
			str[5] = "ISO-2022-JP";
			str[6] = "HEX";
			str[7] = "CAP";
			str[8] = "UTF-8";
			val[0] = NOCNV;
			val[1] = SJIS;
			val[2] = EUC;
			val[3] = JIS7;
			val[4] = JIS8;
			val[5] = JUNET;
			val[6] = HEX;
			val[7] = CAP;
			val[8] = UTF8;
			if (n != O_JIS7 && n != O_JIS8 && n != O_JUNET) p = 0;
			else {
				p = 1;
				n--;
			}
			envcaption(&(envlist[no].env[3]));
			if (noselect(&n, 9, 0, str, val)) return(0);
			if (n >= JIS7 && n <= JUNET) {
				str[0] = VNJIS_K;
				str[1] = VOJIS_K;
				val[0] = 0;
				val[1] = 1;
				if (noselect(&p, 2, 64, str, val)) return(0);
				n += p;
			}
			str[NOCNV] = "";
			str[SJIS] = "sjis";
			str[EUC] = "euc";
			str[JIS7] = "jis7";
			str[O_JIS7] = "ojis7";
			str[JIS8] = "jis8";
			str[O_JIS8] = "ojis8";
			str[JUNET] = "junet";
			str[O_JUNET] = "ojunet";
			str[HEX] = "hex";
			str[CAP] = "cap";
			str[UTF8] = "utf8";
			strcpy(buf, str[n]);
			break;
# endif	/* !MSDOS && !_NOKANJIFCONV */
		case T_OCTAL:
			ascoctal(*((int *)(envlist[no].var)), buf);
			cp = inputcuststr(&(envlist[no].env[3]), 1, buf, -1);
			if (!cp) return(0);
			n = atooctal(cp);
			free(cp);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			ascoctal(n, buf);
			break;
		default:
			cp = inputcuststr(&(envlist[no].env[3]), 0,
				*((char **)(envlist[no].var)), -1);
			if (!cp) return(0);
			strcpy(buf, cp);
			free(cp);
			break;
	}
	setenv2(envlist[no].env, buf);
	_evalenv(no);
	return(1);
}

static int NEAR dumpenv(flaglist, fp)
char *flaglist;
FILE *fp;
{
	char *cp;
	int i, n;

	for (i = n = 0; i < ENVLISTSIZ; i++) {
		if ((!flaglist || !(flaglist[i] & 2))
		&& (cp = getshellvar(&(envlist[i].env[3]), -1))
		&& (envlist[i].type != T_CHARP
		|| strcmp(cp, envlist[i].def))) n++;
		if ((!flaglist || !(flaglist[i] & 1))
		&& getshellvar(envlist[i].env, -1)) n++;
	}
	if (!n || !fp) return(n);

	fputc('\n', fp);
	fputs("# shell variables definition\n", fp);
	for (i = 0; i < ENVLISTSIZ; i++) {
		if ((!flaglist || !(flaglist[i] & 2))
		&& (cp = getshellvar(&(envlist[i].env[3]), -1))
		&& (envlist[i].type != T_CHARP
		|| strcmp(cp, envlist[i].def))) {
			fputs(&(envlist[i].env[3]), fp);
			fputc('=', fp);
			cp = killmeta(cp);
			fputs(cp, fp);
			free(cp);
			fputc('\n', fp);
		}
		if ((!flaglist || !(flaglist[i] & 1))
		&& (cp = getshellvar(envlist[i].env, -1))) {
			fputs(envlist[i].env, fp);
			fputc('=', fp);
			cp = killmeta(cp);
			fputs(cp, fp);
			free(cp);
			fputc('\n', fp);
		}
	}
	return(n);
}

# ifndef	_NOORIGSHELL
static int NEAR checkenv(flaglist, argv, len, fp)
char *flaglist;
char *argv[];
int *len;
FILE *fp;
{
	char *cp, *ident;
	int i, n, f;

	for (n = 0; argv[n]; n++) {
		for (i = 0; i < ENVLISTSIZ; i++) {
			if (!strnpathcmp(argv[n], &(envlist[i].env[3]), len[n])
			|| !strnpathcmp(argv[n], envlist[i].env, len[n]))
				break;
		}
		if (i < ENVLISTSIZ) break;
	}
	if (!argv[n]) return(0);

	for (n = 0; argv[n]; n++) {
#  ifdef	FAKEUNINIT
		f = 0;	/* fake for -Wuninitialized */
#  endif
		for (i = 0; i < ENVLISTSIZ; i++) {
			ident = envlist[i].env;
			if (!strnpathcmp(argv[n], ident, len[n])) {
				f = 1;
				break;
			}
			else if (!strnpathcmp(argv[n], ident += 3, len[n])) {
				f = 2;
				break;
			}
		}
		if (i >= ENVLISTSIZ) {
			if (n) fputc(' ', fp);
			fputs(argv[n], fp);
		}
		else if (!(flaglist[i] & f)) {
			flaglist[i] |= f;
			if (n) fputc(' ', fp);
			cp = getshellvar(ident, -1);
			fputs(ident, fp);
			fputc('=', fp);
			if (cp) {
				cp = killmeta(cp);
				fputs(cp, fp);
				free(cp);
			}
		}
	}
	fputc('\n', fp);
	return(1);
}
# endif	/* !_NOORIGSHELL */

bindtable *copybind(dest, src)
bindtable *dest, *src;
{
	int i;

	if (!src) {
		if (dest) dest[0].key = -1;
	}
	else {
		for (i = 0; src[i].key >= 0; i++);
		if (dest);
		else dest = (bindtable *)malloc2((i + 1) * sizeof(bindtable));
		memcpy((char *)dest, (char *)src, (i + 1) * sizeof(bindtable));
	}
	return(dest);
}

static VOID NEAR cleanupbind(VOID_A)
{
	freestrarray(macrolist, maxmacro);
	maxmacro = 0;
	copybind(bindlist, origbindlist);
	copystrarray(helpindex, orighelpindex, NULL, 10);
}

static int NEAR dispbind(no)
int no;
{
	char *cp1, *cp2;
	int width;

	if (bindlist[no].key < 0) {
		kanjiputs2("", MAXCUSTVAL, 0);
		return(0);
	}
	if (bindlist[no].f_func < FUNCLISTSIZ)
		cp1 = funclist[bindlist[no].f_func].ident;
	else cp1 = macrolist[bindlist[no].f_func - FUNCLISTSIZ];
	if (bindlist[no].d_func < FUNCLISTSIZ)
		cp2 = funclist[bindlist[no].d_func].ident;
	else if (bindlist[no].d_func < 255)
		cp2 = macrolist[bindlist[no].d_func - FUNCLISTSIZ];
	else cp2 = NULL;
	if (!cp2) kanjiputs2(cp1, MAXCUSTVAL, 0);
	else {
		width = (MAXCUSTVAL - 1) / 2;
		kanjiputs2(cp1, width, 0);
		putterm(t_standout);
		putch2(' ');
		putterm(end_standout);
		kanjiputs2(cp2, MAXCUSTVAL - 1 - width, 0);
	}
	return(strlen3(cp1));
}

static int NEAR selectbind(n, max, prompt)
int n, max;
char *prompt;
{
	namelist *list;
	int i, ch, pos, old;
	int dupwin_x, dupwin_y, dupminfilename, dupcolumns, dupsorton;

	max += FUNCLISTSIZ;
	dupminfilename = minfilename;
	dupcolumns = curcolumns;
	minfilename = n_column;
	curcolumns = 5;

	list = (namelist *)malloc2((FUNCLISTSIZ + 2) * sizeof(namelist));
	for (i = 0; i < FUNCLISTSIZ; i++) {
		memset((char *)&(list[i]), 0, sizeof(namelist));
		list[i].name = strdup2(funclist[i].ident);
		list[i].flags = (F_ISRED | F_ISWRI);
		list[i].ent = i;
		list[i].tmpflags = F_STAT;
	}
	memset((char *)&(list[i]), 0, sizeof(namelist));
	list[i].name = strdup2(USRDF_K);
	list[i].flags = (F_ISRED | F_ISWRI);
	list[i].ent = i;
	list[i].tmpflags = F_STAT;
	i++;
	list[i].name = strdup2(DELKB_K);
	list[i].flags = (F_ISRED | F_ISWRI);
	list[i].ent = i;
	list[i].tmpflags = F_STAT;

	dupsorton = sorton;
	sorton = 1;
	qsort(list, FUNCLISTSIZ, sizeof(namelist), cmplist);
	sorton = dupsorton;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	pos = listupfile(list, max,
		(n < FUNCLISTSIZ)
		? funclist[n].ident : list[FUNCLISTSIZ].name);

	envcaption(prompt);
	do {
		locate(0, L_INFO);
		putterm(l_clear);
		if (list[pos].ent >= FUNCLISTSIZ) kanjiputs(list[pos].name);
		else kanjiputs(mesconv(funclist[list[pos].ent].hmes,
			funclist[list[pos].ent].hmes_eng));
		putch2('.');

		locate(win_x, win_y);
		tflush();
		keyflush();
# ifdef	_NOEDITMODE
		ch = Xgetkey(1, 0);
# else
		Xgetkey(-1, 0);
		ch = Xgetkey(1, 0);
		Xgetkey(-1, 0);
# endif

		old = pos;
		switch (ch) {
			case K_UP:
				if (pos > 0) pos--;
				break;
			case K_DOWN:
				if (pos < max - 1) pos++;
				break;
			case K_RIGHT:
				if (pos <= max - 1 - FILEPERROW)
					pos += FILEPERROW;
				else if (pos / FILEPERPAGE
				!= (max - 1) / FILEPERPAGE)
					pos = max - 1;
				break;
			case K_LEFT:
				if (pos >= FILEPERROW) pos -= FILEPERROW;
				break;
			case K_PPAGE:
				if (pos < FILEPERPAGE) putterm(t_bell);
				else pos = ((pos / FILEPERPAGE) - 1)
					* FILEPERPAGE;
				break;
			case K_NPAGE:
				if (((pos / FILEPERPAGE) + 1) * FILEPERPAGE
				>= max - 1)
					putterm(t_bell);
				else pos = ((pos / FILEPERPAGE) + 1)
					* FILEPERPAGE;
				break;
			case K_BEG:
			case K_HOME:
			case '<':
				pos = 0;
				break;
			case K_EOL:
			case K_END:
			case '>':
				pos = max - 1;
				break;
			case K_CTRL('L'):
				rewritefile(1);
				old = -FILEPERPAGE;
				break;
			default:
				break;
		}

		if (old / FILEPERPAGE != pos / FILEPERPAGE)
			pos = listupfile(list, max, list[pos].name);
		else if (old != pos) {
			putname(list, old, -1);
			win_x = putname(list, pos, 1) + 1;
			win_x += calc_x;
			win_y = calc_y;
		}
	} while (ch != K_ESC && ch != K_CR);

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;
	minfilename = dupminfilename;
	curcolumns = dupcolumns;

	if (pos < FUNCLISTSIZ) pos = list[pos].ent;
	for (i = 0; i < FUNCLISTSIZ + 2; i++) free(list[i].name);
	free(list);

	return ((ch == K_CR) ? pos : -1);
}

static int NEAR editbind(no)
int no;
{
	char *cp, *str, buf[MAXLINESTR + 1];
	int i, n1, n2, key, dupwin_x, dupwin_y;

	if ((key = bindlist[no].key) < 0) {
		if (no >= MAXBINDTABLE - 1) {
			warning(0, OVERF_K);
			return(0);
		}

		dupwin_x = win_x;
		dupwin_y = win_y;
		locate(0, L_INFO);
		putterm(l_clear);
		win_x = kanjiputs(BINDK_K);
		win_y = L_INFO;
		locate(win_x, win_y);
		tflush();
		keyflush();
# ifdef	_NOEDITMODE
		key = Xgetkey(1, 0);
# else
		Xgetkey(-1, 0);
		key = Xgetkey(1, 0);
		Xgetkey(-1, 0);
# endif
		win_x = dupwin_x;
		win_y = dupwin_y;

		for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			if (key == (int)(bindlist[i].key)) break;
		if (bindlist[i].key < 0)
			memcpy((char *)&(bindlist[i + 1]),
				(char *)&(bindlist[i]), sizeof(bindtable));
		no = i;
	}
	if (key == K_ESC) {
		warning(0, ESCNG_K);
		return(0);
	}
	str = getkeysym(key, 0);

	for (;;) {
		if (bindlist[no].key < 0) {
			n1 = FUNCLISTSIZ;
			i = 1;
		}
		else {
			n1 = bindlist[no].f_func;
			i = 2;
		}
		sprintf(buf, BINDF_K, str);
		if ((n1 = selectbind(n1, i, buf)) < 0) return(1);
		else if (n1 < FUNCLISTSIZ) break;
		else if (n1 == FUNCLISTSIZ + 1) {
			if (!yesno(DLBOK_K, str)) return(1);
			freemacro(bindlist[no].f_func);
			freemacro(bindlist[no].d_func);
			if (key >= K_F(1) && key <= K_F(10)) {
				free(helpindex[key - K_F(1)]);
				helpindex[key - K_F(1)] = strdup2("");
			}
			for (i = no; i < MAXBINDTABLE; i++) {
				if (bindlist[i].key < 0) break;
				memcpy((char *)&(bindlist[i]),
					(char *)&(bindlist[i + 1]),
					sizeof(bindtable));
			}
			return(1);
		}
		else if (maxmacro >= MAXMACROTABLE) warning(0, OVERF_K);
		else {
			if (bindlist[no].key < 0
			|| bindlist[no].f_func < FUNCLISTSIZ) cp = NULL;
			else cp = macrolist[bindlist[no].f_func - FUNCLISTSIZ];
			sprintf(buf, BNDFC_K, str);
			if (!(cp = inputcuststr(buf, 0, cp, 0)));
			else if (!*cp) {
				free(cp);
				n1 = NO_OPERATION;
				break;
			}
			else {
				macrolist[maxmacro] = cp;
				n1 = FUNCLISTSIZ + maxmacro++;
				break;
			}
		}
	}

	if (yesno(KBDIR_K)) n2 = 255;
	else for (;;) {
		if (bindlist[no].key < 0) n2 = FUNCLISTSIZ;
		else if ((n2 = bindlist[no].d_func) == 255)
			n2 = bindlist[no].f_func;
		sprintf(buf, BINDD_K, str);
		if ((n2 = selectbind(i = n2, 1, buf)) < 0) {
			freemacro(n1);
			return(1);
		}
		else if (n2 < FUNCLISTSIZ) break;
		else if (maxmacro >= MAXMACROTABLE) warning(0, OVERF_K);
		else {
			if (bindlist[no].key < 0 || i < FUNCLISTSIZ) cp = NULL;
			else cp = macrolist[i - FUNCLISTSIZ];
			sprintf(buf, BNDDC_K, str);
			if (!(cp = inputcuststr(buf, 0, cp, 0)));
			else if (!*cp) {
				free(cp);
				n2 = 255;
				break;
			}
			else {
				macrolist[maxmacro] = cp;
				n2 = FUNCLISTSIZ + maxmacro++;
				break;
			}
		}
	}

	if (key >= K_F(1) && key <= K_F(10)
	&& (cp = inputcuststr(FCOMM_K, 0, helpindex[key - K_F(1)], -1))) {
		free(helpindex[key - K_F(1)]);
		helpindex[key - K_F(1)] = cp;
	}

	if (n2 == n1) n2 = 255;
	if (bindlist[no].key < 0) {
		bindlist[no].f_func = (u_char)n1;
		bindlist[no].d_func = (u_char)n2;
	}
	else {
		i = bindlist[no].f_func;
		bindlist[no].f_func = (u_char)n1;
		freemacro(i);
		i = bindlist[no].d_func;
		bindlist[no].d_func = (u_char)n2;
		freemacro(i);
	}
	bindlist[no].key = (short)key;

	return(1);
}

static int NEAR dumpbind(flaglist, fp)
char *flaglist;
FILE *fp;
{
	int i, j, n;

	for (i = n = 0; bindlist[i].key >= 0; i++) {
		if (flaglist && flaglist[i]) continue;
		if (bindlist[i].f_func < FUNCLISTSIZ
		&& (bindlist[i].d_func >= 255
		|| bindlist[i].d_func < FUNCLISTSIZ)) {
			for (j = 0; origbindlist[j].key >= 0; j++)
				if (bindlist[i].key == origbindlist[j].key)
					break;
			if (origbindlist[j].key >= 0
			&& bindlist[i].f_func == origbindlist[j].f_func
			&& bindlist[i].d_func == origbindlist[j].d_func)
				continue;
		}
		n++;
	}
	if (!n || !fp) return(n);

	fputc('\n', fp);
	fputs("# key bind definition\n", fp);
	for (i = 0; bindlist[i].key >= 0; i++) {
		if (flaglist && flaglist[i]) continue;
		if (bindlist[i].f_func < FUNCLISTSIZ
		&& (bindlist[i].d_func >= 255
		|| bindlist[i].d_func < FUNCLISTSIZ)) {
			for (j = 0; origbindlist[j].key >= 0; j++)
				if (bindlist[i].key == origbindlist[j].key)
					break;
			if (origbindlist[j].key >= 0
			&& bindlist[i].f_func == origbindlist[j].f_func
			&& bindlist[i].d_func == origbindlist[j].d_func)
				continue;
		}

		printmacro(i, fp);
		fputc('\n', fp);
	}
	return(n);
}

# ifndef	_NOORIGSHELL
static int NEAR checkbind(flaglist, argc, argv, fp)
char *flaglist;
int argc;
char *argv[];
FILE *fp;
{
	int i, key;

	if (argc < 3 || strpathcmp(argv[0], BL_BIND)
	|| (key = getkeycode(argv[1], 0)) < 0)
		return(0);
	for (i = 0; bindlist[i].key >= 0; i++)
		if (key == (int)(bindlist[i].key)) break;
	if (bindlist[i].key < 0) return(0);

	if (!flaglist[i]) {
		flaglist[i] = 1;
		printmacro(i, fp);
		fputc('\n', fp);
	}
	return(1);
}
# endif	/* !_NOORIGSHELL */

# if	!MSDOS && !defined (_NOKEYMAP)
VOID freekeymap(list)
keymaptable *list;
{
	int i;

	if (list) {
		for (i = 0; list[i].key >= 0; i++)
			if (list[i].str) free(list[i].str);
		free(list);
	}
}

keymaptable *copykeymap(list)
keymaptable *list;
{
	char *cp;
	int i, n, len;

	if (list) {
		for (i = 0; list[i].key >= 0; i++) {
			len = list[i].len;
			if (!len) setkeyseq(list[i].key, NULL, 0);
			else {
				cp = malloc2(len + 1);
				memcpy(cp, list[i].str, len + 1);
				setkeyseq(list[i].key, cp, len);
			}
		}
	}
	else {
		for (i = 0, n = 20; keyidentlist[i].no > 0; i++)
			if (getkeyseq(keyidentlist[i].no, NULL)) n++;
		list = (keymaptable *)malloc2((n + 1) * sizeof(keymaptable));
		for (i = 1, n = 0; i <= 20; i++) {
			list[n].key = K_F(i);
			if (!(cp = getkeyseq(K_F(i), &len)) || !len) {
				list[n].len = 0;
				list[n++].str = NULL;
			}
			else {
				list[n].len = len;
				list[n].str = malloc2(len + 1);
				memcpy(list[n++].str, cp, len + 1);
			}
		}
		for (i = 0; keyidentlist[i].no > 0; i++) {
			if (!(cp = getkeyseq(keyidentlist[i].no, &len)))
				continue;
			list[n].key = keyidentlist[i].no;
			if (!len) {
				list[n].len = 0;
				list[n++].str = NULL;
			}
			else {
				list[n].len = len;
				list[n].str = malloc2(len + 1);
				memcpy(list[n++].str, cp, len + 1);
			}
		}
		list[n].key = -1;
	}
	return(list);
}

static VOID NEAR cleanupkeymap(VOID_A)
{
	int i;

	for (i = 0; origkeymaplist[i].key >= 0; i++)
		setkeyseq(origkeymaplist[i].key, NULL, 0);
	copykeymap(origkeymaplist);
}

static int NEAR dispkeymap(no)
int no;
{
	char *cp, *str;
	int key, len;

	key = tmpkeymaplist[no].key;
	if (!(cp = getkeyseq(key, &len)) || !len) {
		kanjiputs2("", MAXCUSTVAL, 0);
		return(0);
	}
	str = encodestr(cp, len);
	kanjiputs2(str, MAXCUSTVAL, 0);
	len = strlen3(str);
	free(str);
	return(len);
}

static int NEAR editkeymap(no)
int no;
{
	char *cp, *str, buf[MAXLINESTR + 1];
	int i, len, key, dupwin_x, dupwin_y;

	key = tmpkeymaplist[no].key;
	cp = getkeysym(key, 1);

	dupwin_x = win_x;
	dupwin_y = win_y;
	locate(0, L_INFO);
	putterm(l_clear);
	win_x = kanjiprintf(KYMPK_K, cp);
	win_y = L_INFO;
	locate(win_x, win_y);
	tflush();
	keyflush();

	while (!kbhit2(1000000L / SENSEPERSEC));
	for (len = 0; len < MAXLINESTR; ) {
		buf[len++] = getch2();
		if (!kbhit2(WAITKEYPAD * 1000L)) break;
	}

	win_x = dupwin_x;
	win_y = dupwin_y;

	if (len == 1 && buf[0] == K_ESC) {
		if (!yesno(DELKM_K, cp)) return(0);
		setkeyseq(key, NULL, 0);
		return(1);
	}
	str = encodestr(buf, len);
	i = yesno(SETKM_K, cp, str);
	free(str);
	if (!i) return(0);

	cp = malloc2(len + 1);
	memcpy(cp, buf, len);
	cp[len] = '\0';
	setkeyseq(key, cp, len);
	return(1);
}

static int NEAR searchkeymap(list, key)
keymaptable *list;
int key;
{
	char *cp;
	int i, len;

	for (i = 0; list[i].key >= 0; i++) if (key == list[i].key) break;
	if (list[i].key < 0) return(-1);

	cp = getkeyseq(key, &len);
	if (!len) return((list[i].len) ? -1 : i);
	else if (!(list[i].len)) return(-1);
	else if (!cp || !(list[i].str) || memcmp(cp, list[i].str, len))
		return(-1);

	return(i);
}

static int NEAR dumpkeymap(flaglist, fp)
char *flaglist;
FILE *fp;
{
	char *cp;
	int i, n, key, len;

	for (i = n = 0; tmpkeymaplist[i].key >= 0; i++) {
		if (flaglist && flaglist[i]) continue;
		if (searchkeymap(origkeymaplist, tmpkeymaplist[i].key) >= 0)
			continue;
		n++;
	}
	if (!n || !fp) return(n);

	fputc('\n', fp);
	fputs("# keymap definition\n", fp);
	for (i = 0; tmpkeymaplist[i].key >= 0; i++) {
		if (flaglist && flaglist[i]) continue;
		key = tmpkeymaplist[i].key;
		if (searchkeymap(origkeymaplist, key) >= 0) continue;

		cp = getkeyseq(key, &len);
		printkeymap(key, cp, len, fp);
		fputc('\n', fp);
	}
	return(n);
}

#  ifndef	_NOORIGSHELL
static int NEAR checkkeymap(flaglist, argc, argv, fp)
char *flaglist;
int argc;
char *argv[];
FILE *fp;
{
	char *cp;
	int i, key, len;

	if (argc < 3 || strpathcmp(argv[0], BL_KEYMAP)
	|| (key = getkeycode(argv[1], 1)) < 0)
		return(0);
	for (i = 0; tmpkeymaplist[i].key >= 0; i++)
		if (key == (int)(tmpkeymaplist[i].key)) break;
	if (tmpkeymaplist[i].key < 0) return(0);

	cp = getkeyseq(key, &len);

	if (!flaglist[i]) {
		flaglist[i] = 1;
		printkeymap(key, cp, len, fp);
		fputc('\n', fp);
	}
	return(1);
}
#  endif	/* !_NOORIGSHELL */
# endif	/* !MSDOS && !_NOKEYMAP */

# ifndef	_NOARCHIVE
VOID freelaunch(list, max)
launchtable *list;
int max;
{
	int i;

	for (i = 0; i < max; i++) {
		free(list[i].ext);
		free(list[i].comm);
		if (list[i].format) free(list[i].format);
	}
}

launchtable *copylaunch(dest, src, ndestp, nsrc)
launchtable *dest, *src;
int *ndestp, nsrc;
{
	int i;

	if (dest) freelaunch(dest, *ndestp);
	else if (nsrc > 0)
		dest = (launchtable *)malloc2(nsrc * sizeof(launchtable));
	for (i = 0; i < nsrc; i++) {
		dest[i].ext = strdup2(src[i].ext);
		dest[i].comm = strdup2(src[i].comm);
		dest[i].format = strdup2(src[i].format);
		dest[i].topskip = src[i].topskip;
		dest[i].bottomskip = src[i].bottomskip;
		dest[i].flags = src[i].flags;
	}
	*ndestp = nsrc;
	return(dest);
}

static VOID NEAR cleanuplaunch(VOID_A)
{
	copylaunch(launchlist, origlaunchlist, &maxlaunch, origmaxlaunch);
}

static int NEAR displaunch(no)
int no;
{
	char buf[2 + 1];
	int width;

	if (no >= maxlaunch) {
		kanjiputs2("", MAXCUSTVAL, 0);
		return(0);
	}
	if (!launchlist[no].format)
		kanjiputs2(launchlist[no].comm, MAXCUSTVAL, 0);
	else {
		width = (MAXCUSTVAL - 1 - 6) / 2;
		kanjiputs2(launchlist[no].comm, width, 0);
		putterm(t_standout);
		putch2(' ');
		putterm(end_standout);
		width = MAXCUSTVAL - 1 - 6 - width;
		kanjiputs2(launchlist[no].format, width, 0);
		putterm(t_standout);
		putch2(' ');
		putterm(end_standout);
		ascnumeric(buf, launchlist[no].topskip, -1, 2);
		cputs2(buf);
		putterm(t_standout);
		putch2(' ');
		putterm(end_standout);
		ascnumeric(buf, launchlist[no].bottomskip, -1, 2);
		cputs2(buf);
	}
	return(strlen3(launchlist[no].comm));
}

static int NEAR editlaunch(no)
int no;
{
	char *cp, *ext, *comm, *form, buf[3 + 1];
	int i, top, bottom, flags;

	if (no < maxlaunch) ext = NULL;
	else {
		if (no >= MAXLAUNCHTABLE) {
			warning(0, OVERF_K);
			return(0);
		}

		if (!(cp = inputcuststr(EXTLN_K, 1, NULL, -1))) return(0);
		ext = getext(cp, &flags);
		free(cp);
		if (!ext[0] || !ext[1]) {
			free(ext);
			return(0);
		}

		for (i = 0; i < maxlaunch; i++)
			if (!extcmp(ext, flags,
			launchlist[i].ext, launchlist[i].flags, 1)) break;
		if (i >= maxlaunch) {
			launchlist[i].comm = launchlist[i].format = NULL;
			launchlist[i].topskip = launchlist[i].bottomskip = 0;
			launchlist[i].flags = flags;
		}
		no = i;
	}

	comm = form = NULL;
	top = bottom = 0;
	for (;;) {
		if (comm);
		else if (!(comm =
		inputcuststr(LNCHC_K, 0, launchlist[no].comm, 0))) {
			if (ext) free(ext);
			return(0);
		}
		else if (!*comm) {
			free(comm);
			if (ext) {
				free(ext);
				return(0);
			}

			if (!yesno(DLLOK_K, launchlist[no].ext)) return(0);
			free(launchlist[no].ext);
			free(launchlist[no].comm);
			if (launchlist[no].format) free(launchlist[no].format);
			maxlaunch--;
			for (i = no; i < maxlaunch; i++)
				memcpy((char *)&(launchlist[i]),
				(char *)&(launchlist[i + 1]),
					sizeof(launchtable));
			return(1);
		}
		else if (!yesno(ISARC_K)) break;

		if (!(form =
		inputcuststr(ARCHF_K, 1, launchlist[no].format, -1))) {
			free(comm);
			comm = NULL;
			continue;
		}
		else if (!*form) {
			free(form);
			form = NULL;
			break;
		}

		ascnumeric(buf, launchlist[no].topskip, 0, 3);
		for (;;) {
			if (!(cp = inputcuststr(TOPSK_K, 1, buf, -1))) {
				free(form);
				form = NULL;
				break;
			}
			top = atoi2(cp);
			free(cp);
			if (top >= 0) break;
			warning(0, VALNG_K);
		}
		if (!form) continue;

		ascnumeric(buf, launchlist[no].bottomskip, 0, 3);
		for (;;) {
			if (!(cp = inputcuststr(BTMSK_K, 1, buf, -1))) {
				free(form);
				form = NULL;
				break;
			}
			bottom = atoi2(cp);
			free(cp);
			if (bottom >= 0) break;
			warning(0, VALNG_K);
		}
		if (form) break;
	}

	if (no >= maxlaunch) maxlaunch++;
	else {
		if (!ext) ext = launchlist[no].ext;
		else free(launchlist[no].ext);
		free(launchlist[no].comm);
		if (launchlist[no].format) free(launchlist[no].format);
	}
	launchlist[no].ext = ext;
	launchlist[no].comm = comm;
	launchlist[no].format = form;
	launchlist[no].topskip = top;
	launchlist[no].bottomskip = bottom;
	launchlist[no].flags = flags;
	return(1);
}

static int NEAR searchlaunch(list, max, launch)
launchtable *list;
int max;
launchtable *launch;
{
	int i;

	for (i = 0; i < max; i++)
		if (!extcmp(launch -> ext, 0, list[i].ext, 0, 1)) break;
	if (i >= max || launch -> flags != list[i].flags) return(-1);

	if (!(launch -> format)) {
		if (list[i].format) return(-1);
	}
	else if (!(list[i].format)) return(-1);
	else if (launch -> topskip != list[i].topskip
	|| launch -> bottomskip != list[i].bottomskip
	|| strcmp(launch -> format, list[i].format))
		return(-1);
	if (strcmp(launch -> comm, list[i].comm)) return(-1);

	return(i);
}

static int NEAR dumplaunch(flaglist, fp)
char *flaglist;
FILE *fp;
{
	int i, n;

	for (i = n = 0; i < maxlaunch; i++) {
		if (flaglist && flaglist[i]) continue;
		if (searchlaunch(origlaunchlist, origmaxlaunch,
		&(launchlist[i])) >= 0)
			continue;
		n++;
	}
	if (!n || !fp) return(n);

	fputc('\n', fp);
	fputs("# launcher definition\n", fp);
	for (i = 0; i < maxlaunch; i++) {
		if (flaglist && flaglist[i]) continue;
		if (searchlaunch(origlaunchlist, origmaxlaunch,
		&(launchlist[i])) >= 0)
			continue;
		printlaunchcomm(i, 0, fp);
		fputc('\n', fp);
	}
	return(n);
}

#  ifndef	_NOORIGSHELL
static int NEAR checklaunch(flaglist, argc, argv, fp)
char *flaglist;
int argc;
char *argv[];
FILE *fp;
{
	char *ext;
	int i, flags;

	if (argc < 3 || strpathcmp(argv[0], BL_LAUNCH)) return(0);
	ext = getext(argv[1], &flags);
	for (i = 0; i < maxlaunch; i++)
		if (!extcmp(ext, flags,
		launchlist[i].ext, launchlist[i].flags, 1)) break;
	free(ext);
	if (i >= maxlaunch) return(0);

	if (!flaglist[i]) {
		flaglist[i] = 1;
		printlaunchcomm(i, 0, fp);
		fputc('\n', fp);
	}
	return(1);
}
#  endif	/* !_NOORIGSHELL */

VOID freearch(list, max)
archivetable *list;
int max;
{
	int i;

	for (i = 0; i < max; i++) {
		free(list[i].ext);
		if (list[i].p_comm) free(list[i].p_comm);
		if (list[i].u_comm) free(list[i].u_comm);
	}
}

archivetable *copyarch(dest, src, ndestp, nsrc)
archivetable *dest, *src;
int *ndestp, nsrc;
{
	int i;

	if (dest) freearch(dest, *ndestp);
	else if (nsrc > 0)
		dest = (archivetable *)malloc2(nsrc * sizeof(archivetable));
	for (i = 0; i < nsrc; i++) {
		dest[i].ext = strdup2(src[i].ext);
		dest[i].p_comm = strdup2(src[i].p_comm);
		dest[i].u_comm = strdup2(src[i].u_comm);
		dest[i].flags = src[i].flags;
	}
	*ndestp = nsrc;
	return(dest);
}

static VOID NEAR cleanuparch(VOID_A)
{
	copyarch(archivelist, origarchivelist, &maxarchive, origmaxarchive);
}

static int NEAR disparch(no)
int no;
{
	int width;

	if (no >= maxarchive) {
		kanjiputs2("", MAXCUSTVAL, 0);
		return(0);
	}
	width = (MAXCUSTVAL - 1) / 2;
	if (!archivelist[no].p_comm) kanjiputs2("", width, 0);
	else kanjiputs2(archivelist[no].p_comm, width, 0);
	putterm(t_standout);
	putch2(' ');
	putterm(end_standout);
	if (!archivelist[no].u_comm) kanjiputs2("", MAXCUSTVAL - 1 - width, 0);
	else kanjiputs2(archivelist[no].u_comm, MAXCUSTVAL - 1 - width, 0);
	return(archivelist[no].p_comm ? strlen3(archivelist[no].p_comm) : 0);
}

static int NEAR editarch(no)
int no;
{
	char *cp, *ext, *pcom, *ucom;
	int i, flags;

	if (no < maxarchive) ext = NULL;
	else {
		if (no >= MAXARCHIVETABLE) {
			warning(0, OVERF_K);
			return(0);
		}

		if (!(cp = inputcuststr(EXTAR_K, 1, NULL, -1))) return(0);
		ext = getext(cp, &flags);
		free(cp);
		if (!ext[0] || !ext[1]) {
			free(ext);
			return(0);
		}

		for (i = 0; i < maxarchive; i++)
			if (!extcmp(ext, flags,
			archivelist[i].ext, archivelist[i].flags, 1)) break;
		if (i >= maxarchive) {
			archivelist[i].p_comm = archivelist[i].u_comm = NULL;
			archivelist[i].flags = flags;
		}
		no = i;
	}

	for (;;) {
		if (!(pcom =
		inputcuststr(PACKC_K, 0, archivelist[no].p_comm, 0))) {
			if (ext) free(ext);
			return(0);
		}
		else if (!*pcom) {
			free(pcom);
			pcom = NULL;
		}

		if (!(ucom =
		inputcuststr(UPCKC_K, 0, archivelist[no].u_comm, 0))) {
			if (pcom) free(pcom);
			continue;
		}
		if (!*ucom) {
			free(ucom);
			ucom = NULL;
		}
		break;
	}

	if (!pcom && !ucom) {
		if (ext) {
			free(ext);
			return(0);
		}

		if (!yesno(DLAOK_K, archivelist[no].ext)) return(0);
		free(archivelist[no].ext);
		if (archivelist[no].p_comm) free(archivelist[no].p_comm);
		if (archivelist[no].u_comm) free(archivelist[no].u_comm);
		maxarchive--;
		for (i = no; i < maxarchive; i++)
			memcpy((char *)&(archivelist[i]),
			(char *)&(archivelist[i + 1]),
				sizeof(archivetable));
		return(1);
	}

	if (no >= maxarchive) maxarchive++;
	else {
		if (!ext) ext = archivelist[no].ext;
		else free(archivelist[no].ext);
		if (archivelist[no].p_comm) free(archivelist[no].p_comm);
		if (archivelist[no].u_comm) free(archivelist[no].u_comm);
	}
	archivelist[no].ext = ext;
	archivelist[no].p_comm = pcom;
	archivelist[no].u_comm = ucom;
	archivelist[no].flags = flags;
	return(1);
}

static int NEAR searcharch(list, max, arch)
archivetable *list;
int max;
archivetable *arch;
{
	int i;

	for (i = 0; i < max; i++)
		if (!extcmp(arch -> ext, 0, list[i].ext, 0, 1)) break;
	if (i >= max || arch -> flags != list[i].flags) return(-1);

	if (!arch -> p_comm) {
		if (list[i].p_comm) return(-1);
	}
	else if (!(list[i].p_comm)) return(-1);
	else if (strcmp(arch -> p_comm, list[i].p_comm)) return(-1);
	if (strcmp(arch -> u_comm, list[i].u_comm)) return(-1);

	return(i);
}

static int NEAR dumparch(flaglist, fp)
char *flaglist;
FILE *fp;
{
	int i, n;

	for (i = n = 0; i < maxarchive; i++) {
		if (flaglist && flaglist[i]) continue;
		if (searcharch(origarchivelist, origmaxarchive,
		&(archivelist[i])) >= 0)
			continue;
		n++;
	}
	if (!n || !fp) return(n);

	fputc('\n', fp);
	fputs("# archiver definition\n", fp);
	for (i = 0; i < maxarchive; i++) {
		if (flaglist && flaglist[i]) continue;
		if (searcharch(origarchivelist, origmaxarchive,
		&(archivelist[i])) >= 0)
			continue;
		printarchcomm(i, fp);
		fputc('\n', fp);
	}
	return(n);
}

#  ifndef	_NOORIGSHELL
static int NEAR checkarch(flaglist, argc, argv, fp)
char *flaglist;
int argc;
char *argv[];
FILE *fp;
{
	char *ext;
	int i, flags;

	if (argc < 3 || strpathcmp(argv[0], BL_ARCH)) return(0);
	ext = getext(argv[1], &flags);
	for (i = 0; i < maxarchive; i++)
		if (!extcmp(ext, flags,
		archivelist[i].ext, archivelist[i].flags, 1)) break;
	free(ext);
	if (i >= maxarchive) return(0);

	if (!flaglist[i]) {
		flaglist[i] = 1;
		printarchcomm(i, fp);
		fputc('\n', fp);
	}
	return(1);
}
#  endif	/* _NOORIGSHELL */
# endif	/* !_NOARCHIVE */

# if	!MSDOS && !defined (_NODOSDRIVE)
VOID freedrive(list)
devinfo *list;
{
	int i;

	for (i = 0; list[i].name; i++) free(list[i].name);
}

devinfo *copydrive(dest, src)
devinfo *dest, *src;
{
	int i;

	if (dest) freedrive(dest);
	if (!src) {
		if (dest) dest[0].name = NULL;
	}
	else {
		for (i = 0; src[i].name; i++);
		if (dest);
		else dest = (devinfo *)malloc2((i + 1) * sizeof(devinfo));
		for (i = 0; src[i].name; i++) {
			dest[i].drive = src[i].drive;
			dest[i].name = strdup2(src[i].name);
			dest[i].head = src[i].head;
			dest[i].sect = src[i].sect;
			dest[i].cyl = src[i].cyl;
		}
		dest[i].name = NULL;
	}
	return(dest);
}

static VOID NEAR cleanupdrive(VOID_A)
{
	copydrive(fdtype, origfdtype);
}

static int NEAR dispdrive(no)
int no;
{
	char *cp, buf[sizeof("HDD98 #offset=") + 14];
	int i, len, w, width;

	if (!fdtype[no].name) {
		kanjiputs2("", MAXCUSTVAL, 0);
		return(0);
	}
	width = (MAXCUSTVAL - 1) / 2;
	kanjiputs2(fdtype[no].name, width, 0);
	putterm(t_standout);
	putch2(' ');
	putterm(end_standout);

	width = MAXCUSTVAL - 1 - width;
#  ifdef	HDDMOUNT
	if (!fdtype[no].cyl) {
		strcpy(buf, "HDD");
		if (fdtype[no].head >= 'A' && fdtype[no].head <= 'Z')
			strcat(buf, "98");
		strcat(buf, " #offset=");
		ascnumeric(buf + strlen(buf),
			fdtype[no].offset / fdtype[no].sect, -3, 14);
		kanjiputs2(buf, width, 0);
	}
	else
#  endif	/* HDDMOUNT */
	{
		len = 0;
		for (i = 0; i < MEDIADESCRSIZ; i++) {
			w = strlen(mediadescr[i].name);
			if (len < w) len = w;
		}

		w = (width - len) / 3 - 1;
		cp = malloc2(w + 1);
		cputs2(ascnumeric(cp, fdtype[no].head, -1, w));
		putterm(t_standout);
		putch2(' ');
		putterm(end_standout);
		cputs2(ascnumeric(cp, fdtype[no].sect, -1, w));
		putterm(t_standout);
		putch2(' ');
		putterm(end_standout);
		cputs2(ascnumeric(cp, fdtype[no].cyl, -1, w));
		putterm(t_standout);
		putch2(' ');
		putterm(end_standout);
		free(cp);

		for (i = 0; i < MEDIADESCRSIZ; i++) {
			if (fdtype[no].head == mediadescr[i].head
			&& fdtype[no].sect == mediadescr[i].sect
			&& fdtype[no].cyl == mediadescr[i].cyl) break;
		}
		if (i >= MEDIADESCRSIZ) kanjiputs2("", width - (w + 1) * 3, 0);
		else kanjiputs2(mediadescr[i].name, width - (w + 1) * 3, 0);
	}
	return(strlen3(fdtype[no].name));
}

static int NEAR editdrive(no)
int no;
{
	char *cp, *dev, buf[MAXLINESTR + 1];
	char *str['Z' - 'A' + 1], sbuf['Z' - 'A' + 1][3];
	int i, n, drive, head, sect, cyl, val['Z' - 'A' + 1];

	if (fdtype[no].name) drive = fdtype[no].drive;
	else {
		if (no >= MAXDRIVEENTRY - 1) {
			warning(0, OVERF_K);
			return(0);
		}

		for (drive = n = 0; drive <= 'Z' - 'A'; drive++) {
#  ifdef	HDDMOUNT
			for (i = 0; fdtype[i].name; i++)
				if (drive == fdtype[i].drive && !fdtype[i].cyl)
					break;
			if (fdtype[i].name) continue;
#  endif
			sbuf[n][0] = drive + 'A';
			sbuf[n][1] = ':';
			sbuf[n][2] = '\0';
			str[n] = sbuf[n];
			val[n++] = drive + 'A';
		}

		envcaption(DRNAM_K);
		drive = val[0];
		if (noselect(&drive, n, 0, str, val)) return(0);

		fdtype[no].name = NULL;
	}

	dev = NULL;
#  ifdef	FAKEUNINIT
	head = sect = cyl = -1;	/* fake for -Wuninitialized */
#  endif
	for (;;) {
		sprintf(buf, DRDEV_K, drive);
		if (dev);
		else if (!(dev = inputcuststr(buf, 1, fdtype[no].name, 1)))
			return(0);
		else if (!*dev) {
			if (!(fdtype[no].name)) return(0);

			if (!yesno(DLDOK_K, fdtype[no].drive)) return(0);
			deletedrv(no);
			return(1);
		}

		envcaption(DRMED_K);
		for (i = n = 0; i < MEDIADESCRSIZ; i++) {
#  ifdef	HDDMOUNT
			int j;

			if (!mediadescr[i].cyl) {
				if (fdtype[no].name) continue;
				for (j = 0; fdtype[j].name; j++)
					if (drive == fdtype[j].drive) break;
				if (fdtype[j].name) continue;
			}
#  endif
			str[n] = mediadescr[i].name;
			val[n++] = i;
		}
		str[n] = USRDF_K;
		val[n++] = i;
		i = val[0];
		if (noselect(&i, n, 0, str, val)) {
			free(dev);
			dev = NULL;
			continue;
		}

		if (i < MEDIADESCRSIZ) {
			head = mediadescr[i].head;
			sect = mediadescr[i].sect;
			cyl = mediadescr[i].cyl;
			break;
		}

		for (;;) {
			if (!(fdtype[no].name)) cp = NULL;
			else cp = ascnumeric(buf, fdtype[no].head,
				0, MAXLINESTR);

			if (!(cp = inputcuststr(DRHED_K, 1, cp, -1))) break;
			head = atoi2(cp);
			free(cp);
			if (head > 0) break;
			warning(0, VALNG_K);
		}
		if (!cp) continue;

		for (;;) {
			if (!(fdtype[no].name)) cp = NULL;
			else cp = ascnumeric(buf, fdtype[no].sect,
				0, MAXLINESTR);

			if (!(cp = inputcuststr(DRSCT_K, 1, cp, -1))) break;
			sect = atoi2(cp);
			free(cp);
			if (sect > 0) break;
			warning(0, VALNG_K);
		}
		if (!cp) continue;

		for (;;) {
			if (!(fdtype[no].name)) cp = NULL;
			else cp = ascnumeric(buf, fdtype[no].cyl,
				0, MAXLINESTR);

			if (!(cp = inputcuststr(DRCYL_K, 1, cp, -1))) break;
			cyl = atoi2(cp);
			free(cp);
			if (cyl > 0) break;
			warning(0, VALNG_K);
		}
		if (cp) break;
	}

	n = searchdrv(fdtype, drive, dev, head, sect, cyl, 1);
	if (fdtype[n].name) {
		if (n != no) warning(0, DUPLD_K);
		return(0);
	}

	if (fdtype[no].name) no = deletedrv(no);

	n = insertdrv(no, drive, dev, head, sect, cyl);
	free(dev);
	if (n < 0) {
		if (n < -1) warning(0, OVERF_K);
		else warning(ENODEV, dev);
		return(0);
	}
	return(1);
}

static int NEAR dumpdrive(flaglist, fp)
char *flaglist;
FILE *fp;
{
	int i, j, n;

	for (i = n = 0; fdtype[i].name; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searchdrv(origfdtype, fdtype[i].drive, fdtype[i].name,
			fdtype[i].head, fdtype[i].sect, fdtype[i].cyl, 1);
		if (origfdtype[j].name) continue;
		n++;
	}
	if (!n || !fp) return(n);

	fputc('\n', fp);
	fputs("# MS-DOS drive definition\n", fp);
	for (i = 0; fdtype[i].name; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searchdrv(origfdtype, fdtype[i].drive, fdtype[i].name,
			fdtype[i].head, fdtype[i].sect, fdtype[i].cyl, 1);
		if (origfdtype[j].name) continue;
		printsetdrv(i, 0, fp);
		fputc('\n', fp);
	}
	return(n);
}

#  ifndef	_NOORIGSHELL
static int NEAR checkdrive(flaglist, argc, argv, fp)
char *flaglist;
int argc;
char *argv[];
FILE *fp;
{
	int i, j;

	if (argc < 4 || strpathcmp(argv[0], BL_SDRIVE)) return(0);
	for (i = 0; fdtype[i].name; i++)
		if (toupper2(argv[1][0]) == fdtype[i].drive) break;
	if (!(fdtype[i].name)) return(0);

	for (j = i; fdtype[j].name; j++) {
		if (i == j);
		else if (fdtype[i].drive != fdtype[j].drive) {
#   ifdef	HDDMOUNT
			if (!fdtype[i].cyl) {
				if (strpathcmp(fdtype[i].name, fdtype[j].name)
				|| fdtype[j].drive != fdtype[j - 1].drive + 1)
					break;
			}
			else
#   endif	/* HDDMOUNT */
			break;
		}
		if (!flaglist[j]) {
			flaglist[j] = 1;
			printsetdrv(j, 0, fp);
			fputc('\n', fp);
		}
	}
	return(1);
}
#  endif	/* _NOORIGSHELL */
# endif	/* !MSDOS && !_NODOSDRIVE */

static int NEAR dispsave(no)
int no;
{
	char *mes[MAXSAVEMENU];

	mes[0] = CCNCL_K;
	mes[1] = CCLEA_K;
	mes[2] = CLOAD_K;
	mes[3] = CSAVE_K;
#ifdef	_NOORIGSHELL
	mes[4] = NIMPL_K;
#else
	mes[4] = COVWR_K;
#endif
	kanjiputs2(mes[no], MAXCUSTVAL, 0);
	return(strlen3(mes[no]));
}

# ifndef	_NOORIGSHELL
static int NEAR overwriteconfig(val, file)
int *val;
char *file;
{
	syntaxtree *trp, *stree;
	FILE *fpin, *fpout;
	char *cp, *buf, path[MAXPATHLEN];
	char *flaglist[MAXCUSTOM], **argv, **subst;
	int i, n, len, *slen, fd, argc, max[MAXCUSTOM];

	if (!(fpin = Xfopen(file, "r")) && errno != ENOENT) {
		warning(-1, file);
		return(-1);
	}

	strcpy(path, file);
	if ((cp = strrdelim(path, 1))) cp++;
	else cp = path;
	len = sizeof(path) - 1 - (cp - path);
	if (len > MAXTNAMLEN) len = MAXTNAMLEN;
	genrandname(NULL, 0);

	for (;;) {
		genrandname(cp, len);
		fd = Xopen(path, O_BINARY | O_WRONLY | O_CREAT | O_EXCL, 0666);
		if (fd >= 0) break;
		if (errno != EEXIST) {
			warning(-1, path);
			if (fpin) Xfclose(fpin);
			return(-1);
		}
	}
	if (fd < 0) {
		warning(EEXIST, path);
		if (fpin) Xfclose(fpin);
		return(-1);
	}
	if (!(fpout = Xfdopen(fd, "w"))) {
		warning(-1, path);
		Xclose(fd);
		Xunlink(path);
		if (fpin) Xfclose(fpin);
		return(-1);
	}

	calcmax(max, 0);
	for (i = 0; i < MAXCUSTOM; i++) {
		if (!max[i]) flaglist[i] = NULL;
		else {
			flaglist[i] = malloc2(max[i] * sizeof(char));
			memset(flaglist[i], 0, max[i] * sizeof(char));
		}
	}

	buf = NULL;
	len = 0;
	trp = stree = newstree(NULL);
	if (fpin) while ((cp = fgets2(fpin, 0))) {
		n = strlen(cp);
		if (!buf) {
			buf = strdup2(cp);
			len = n;
		}
		else {
			buf = realloc2(buf, len + n + 1 + 1);
			buf[len++] = '\n';
			strcpy(&(buf[len]), cp);
			len += n;
		}
		trp = analyze(cp, trp, 1);
		free(cp);

		n = 0;
		if (!trp);
		else if (trp -> cont) continue;
		else if ((argv = getsimpleargv(stree))) {
			argc = (stree -> comm) -> argc;
			argc = (stree -> comm) -> argc =
				getsubst(argc, argv, &subst, &slen);
			for (i = 0; i < argc; i++) stripquote(argv[i], 1);

			if (subst[0]) {
				if (!argc && val[0] && flaglist[0])
					n = checkenv(flaglist[0],
						subst, slen, fpout);
			}
			else {
				if (!n && val[1] && flaglist[1])
					n = checkbind(flaglist[1],
						argc, argv, fpout);
#  if	!MSDOS && !defined (_NOKEYMAP)
				if (!n && val[2] && flaglist[2])
					n = checkkeymap(flaglist[2],
						argc, argv, fpout);
#  endif
#  ifndef	_NOARCHIVE
				if (!n && val[3] && flaglist[3])
					n = checklaunch(flaglist[3],
						argc, argv, fpout);
				if (!n && val[4] && flaglist[4])
					n = checkarch(flaglist[4],
						argc, argv, fpout);
#  endif
#  if	!MSDOS && !defined (_NODOSDRIVE)
				if (!n && val[5] && flaglist[5])
					n = checkdrive(flaglist[5],
						argc, argv, fpout);
#  endif
			}

			freevar(subst);
			free(slen);
		}

		freestree(stree);
		trp = stree;
		if (!n) {
			fputs(buf, fpout);
			fputc('\n', fpout);
		}
		free(buf);
		buf = NULL;
		len = 0;
	}
	freestree(stree);
	free(stree);

	if (!fpin) fputs("# configurations by customizer\n", fpout);
	else {
		Xfclose(fpin);
		n = 0;
		if (val[0] && flaglist[0])
			n = dumpenv(flaglist[0], NULL);
		if (!n && val[1] && flaglist[1])
			n += dumpbind(flaglist[1], NULL);
#  if	!MSDOS && !defined (_NOKEYMAP)
		if (!n && val[2] && flaglist[2])
			n += dumpkeymap(flaglist[2], NULL);
#  endif
#  ifndef	_NOARCHIVE
		if (!n && val[3] && flaglist[3])
			n += dumplaunch(flaglist[3], NULL);
		if (!n && val[4] && flaglist[4])
			n += dumparch(flaglist[4], NULL);
#  endif
#  if	!MSDOS && !defined (_NODOSDRIVE)
		if (!n && val[5] && flaglist[5])
			n += dumpdrive(flaglist[5], NULL);
#  endif
		if (n) {
			fputc('\n', fpout);
			fputs("# additional configurations by customizer\n",
				fpout);
		}
	}

	if (val[0] && flaglist[0]) dumpenv(flaglist[0], fpout);
	if (val[1] && flaglist[1]) dumpbind(flaglist[1], fpout);
#  if	!MSDOS && !defined (_NOKEYMAP)
	if (val[2] && flaglist[2]) dumpkeymap(flaglist[2], fpout);
#  endif
#  ifndef	_NOARCHIVE
	if (val[3] && flaglist[3]) dumplaunch(flaglist[3], fpout);
	if (val[4] && flaglist[4]) dumparch(flaglist[4], fpout);
#  endif
#  if	!MSDOS && !defined (_NODOSDRIVE)
	if (val[5] && flaglist[5]) dumpdrive(flaglist[5], fpout);
#  endif
	Xfclose(fpout);
	for (i = 0; i < MAXCUSTOM; i++) if (flaglist[i]) free(flaglist[i]);

	if (Xrename(path, file) < 0) {
		warning(-1, file);
		Xunlink(path);
		return(-1);
	}
	return(1);
}
# endif	/* !_NOORIGSHELL */

static int NEAR editsave(no)
int no;
{
	FILE *fp;
	char *file, *str[MAXCUSTOM - 1];
	int i, n, done, val[MAXCUSTOM - 1];

	for (i = 0; i < MAXCUSTOM - 1; i++) {
		str[i] = NULL;
		val[i] = 1;
	}

	str[0] = TENV_K;
	str[1] = TBIND_K;
# if	!MSDOS && !defined (_NOKEYMAP)
	str[2] = TKYMP_K;
# endif
# ifndef	_NOARCHIVE
	str[3] = TLNCH_K;
	str[4] = TARCH_K;
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
	str[5] = TSDRV_K;
# endif

	done = 0;
	switch (no) {
		case 0:
			envcaption(SREST_K);
			if (noselect(NULL, MAXCUSTOM - 1, 0, str, val)) break;
			done = 1;
			if (val[0]) copyenv(tmpenvlist);
			if (val[1]) {
				copystrarray(macrolist, tmpmacrolist,
					&maxmacro, tmpmaxmacro);
				copystrarray(helpindex, tmphelpindex,
					NULL, 10);
				copybind(bindlist, tmpbindlist);
			}
# if	!MSDOS && !defined (_NOKEYMAP)
			if (val[2]) copykeymap(tmpkeymaplist);
# endif
# ifndef	_NOARCHIVE
			if (val[3]) copylaunch(launchlist, tmplaunchlist,
					&maxlaunch, tmpmaxlaunch);
			if (val[4]) copyarch(archivelist, tmparchivelist,
					&maxarchive, tmpmaxarchive);
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
			if (val[5]) copydrive(fdtype, tmpfdtype);
# endif
			break;
		case 1:
			envcaption(SCLEA_K);
			if (noselect(NULL, MAXCUSTOM - 1, 0, str, val)) break;
			done = 1;
			if (val[0]) cleanupenv();
			if (val[1]) cleanupbind();
# if	!MSDOS && !defined (_NOKEYMAP)
			if (val[2]) cleanupkeymap();
# endif
# ifndef	_NOARCHIVE
			if (val[3]) cleanuplaunch();
			if (val[4]) cleanuparch();
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
			if (val[5]) cleanupdrive();
# endif
			break;
		case 2:
			if (!(file = inputcuststr(FLOAD_K, 1, RUNCOMFILE, 1)))
				break;
			done = 1;
			n = sigvecset(0);
			locate(0, n_line - 1);
			inruncom = 1;
			i = loadruncom(file, 0);
			inruncom = 0;
			if (i) warning(0, HITKY_K);
			sigvecset(n);
			break;
		case 3:
			if (!(file = inputcuststr(FSAVE_K, 1, RUNCOMFILE, 1)))
				break;
			done = 1;
			file = evalpath(file, 1);
# ifdef	FAKEUNINIT
			fp = NULL;	/* fake for -Wuninitialized */
# endif
			if (Xaccess(file, F_OK) >= 0 && yesno(FSVOK_K))
				done = 0;
			else {
				envcaption(SSAVE_K);
				if (noselect(NULL, MAXCUSTOM - 1, 0, str, val))
					done = 0;
				else if (!(fp = Xfopen(file, "w"))) {
					warning(-1, file);
					done = 0;
				}
			}
			free(file);

			if (!done) break;
			fputs("# configurations by customizer\n", fp);
			if (val[0]) dumpenv(NULL, fp);
			if (val[1]) dumpbind(NULL, fp);
# if	!MSDOS && !defined (_NOKEYMAP)
			if (val[2]) dumpkeymap(NULL, fp);
# endif
# ifndef	_NOARCHIVE
			if (val[3]) dumplaunch(NULL, fp);
			if (val[4]) dumparch(NULL, fp);
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
			if (val[5]) dumpdrive(NULL, fp);
# endif
			Xfclose(fp);
			break;
		case 4:
# ifndef	_NOORIGSHELL
			if (!(file = inputcuststr(FOVWR_K, 1, RUNCOMFILE, 1)))
				break;
			done = 1;
			file = evalpath(file, 1);
			if (Xaccess(file, F_OK) < 0 && errno == ENOENT
			&& !yesno(FOVOK_K)) done = 0;
			else {
				envcaption(SOVWR_K);
				if (noselect(NULL, MAXCUSTOM - 1, 0, str, val))
					done = 0;
				else if (overwriteconfig(val, file) < 0)
					done = 0;
			}
			free(file);
			break;
# endif	/* !_NOORIGSHELL */
		default:
			break;
	}
	return(done);
}

static VOID NEAR dispname(no, y, isstandout)
int no, y, isstandout;
{
# if	!defined (_NOARCHIVE) || (!MSDOS && !defined (_NODOSDRIVE))
	char buf[MAXLINESTR + 1];
# endif
	char *cp, *name[MAXSAVEMENU];

	int len;

	cp = NEWET_K;
	switch (custno) {
		case 0:
			cp = &(envlist[no].env[3]);
			break;
		case 1:
			if (bindlist[no].key >= 0)
				cp = getkeysym(bindlist[no].key, 0);
			break;
# if	!MSDOS && !defined (_NOKEYMAP)
		case 2:
			cp = getkeysym(tmpkeymaplist[no].key, 1);
			break;
# endif
# ifndef	_NOARCHIVE
		case 3:
			if (no < maxlaunch) {
				if (!(launchlist[no].flags & LF_IGNORECASE))
					cp = launchlist[no].ext + 1;
				else {
					strncpy2(buf, launchlist[no].ext,
						MAXLINESTR);
					buf[0] = '/';
					cp = buf;
				}
			}
			break;
		case 4:
			if (no < maxarchive) {
				if (!(archivelist[no].flags & AF_IGNORECASE))
					cp = archivelist[no].ext + 1;
				else {
					strncpy2(buf, archivelist[no].ext,
						MAXLINESTR);
					buf[0] = '/';
					cp = buf;
				}
			}
			break;
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
		case 5:
			if (fdtype[no].name) {
				buf[0] = fdtype[no].drive;
				buf[1] = ':';
				buf[2] = '\0';
				cp = buf;
			}
			break;
# endif
		case 6:
			name[0] = "Cancel";
			name[1] = "Clear";
			name[2] = "Load";
			name[3] = "Save";
			name[4] = "Overwrite";
			cp = name[no];
			break;
		default:
			cp = "";
			break;
	}

	locate(0, y);
	putterm(t_standout);
	for (len = strlen3(cp); len <= MAXCUSTNAM; len++) putch2(' ');
	if (isstandout) kanjiputs(cp);
	else {
		putterm(end_standout);
		if (!stable_standout) kanjiputs(cp);
		else locate(MAXCUSTNAM + 1, y);
		putterm(t_standout);
	}
	putch2(' ');
	putterm(end_standout);
}

static VOID NEAR dispcust(VOID_A)
{
	int i, y, start, end;

	y = LFILETOP + 2;
	start = (cs_item / cs_row) * cs_row;
	end = start + cs_row;
	if (end > cs_max) end = cs_max;

	for (i = start; i < end; i++) {
		dispname(i, y++, 1);
		switch (custno) {
			case 0:
				cs_len[i - start] = dispenv(i);
				break;
			case 1:
				cs_len[i - start] = dispbind(i);
				break;
# if	!MSDOS && !defined (_NOKEYMAP)
			case 2:
				cs_len[i - start] = dispkeymap(i);
				break;
# endif
# ifndef	_NOARCHIVE
			case 3:
				cs_len[i - start] = displaunch(i);
				break;
			case 4:
				cs_len[i - start] = disparch(i);
				break;
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
			case 5:
				cs_len[i - start] = dispdrive(i);
				break;
# endif
			case 6:
				cs_len[i - start] = dispsave(i);
				break;
			default:
				cs_len[i - start] = kanjiputs(NIMPL_K);
				kanjiputs2("",
					MAXCUSTVAL - cs_len[i - start], 0);
				break;
		}
		putterm(t_standout);
		putch2(' ');
		putterm(end_standout);

		locate(0, y++);
		putterm(t_standout);
		kanjiputs2("", n_column, 0);
		putterm(end_standout);
	}
	for (; y < LFILEBOTTOM; y++) {
		locate(0, y);
		putterm(t_standout);
		kanjiputs2("", n_column, 0);
		putterm(end_standout);
	}
}

VOID rewritecust(all)
int all;
{
	cs_row = (LFILEBOTTOM - LFILETOP - 2) / 2;
	cs_len = (int *)realloc2(cs_len, cs_row * sizeof(int));
	dispcust();
	movecust(-1, all);
	custtitle();
	tflush();
}

static VOID NEAR movecust(old, all)
int old, all;
{
	char *hmes[MAXCUSTOM];

	if (all > 0) {
		hmes[0] = NULL;
		hmes[1] = HBIND_K;
		hmes[2] = HKYMP_K;
		hmes[3] = HLNCH_K;
		hmes[4] = HARCH_K;
		hmes[5] = HSDRV_K;
		hmes[6] = HSAVE_K;
		locate(0, L_INFO);
		putterm(l_clear);
		if (hmes[custno]) kanjiputs(hmes[custno]);
		else kanjiputs(mesconv(envlist[cs_item].hmes,
			envlist[cs_item].hmes_eng));
	}

	win_y = LFILETOP + 2 + (cs_item % cs_row) * 2;
	dispname(cs_item, win_y, 0);
	win_x = cs_len[cs_item % cs_row] + MAXCUSTNAM + 2;
	if (old >= 0 && old < cs_max)
		dispname(old, LFILETOP + 2 + (old % cs_row) * 2, 1);
	locate(win_x, win_y);
}

static int NEAR editcust(VOID_A)
{
	int i, n;

	switch (custno) {
		case 0:
			n = editenv(cs_item);
			break;
		case 1:
			if ((n = editbind(cs_item))) {
				for (i = 0; bindlist[i].key >= 0; i++);
				cs_max = i + 1;
			}
			break;
# if	!MSDOS && !defined (_NOKEYMAP)
		case 2:
			n = editkeymap(cs_item);
			break;
# endif
# ifndef	_NOARCHIVE
		case 3:
			if ((n = editlaunch(cs_item))) cs_max = maxlaunch + 1;
			break;
		case 4:
			if ((n = editarch(cs_item))) cs_max = maxarchive + 1;
			break;
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
		case 5:
			if ((n = editdrive(cs_item))) {
				for (i = 0; fdtype[i].name; i++);
				cs_max = i + 1;
			}
			break;
# endif
		case 6:
			n = editsave(cs_item);
			break;
		default:
			n = 0;
			break;
	}
	return(n);
}

int customize(VOID_A)
{
	int i, ch, old, changed, item[MAXCUSTOM], max[MAXCUSTOM];

	for (i = LFILETOP; i < LFILEBOTTOM; i++) {
		locate(0, i);
		putterm(l_clear);
	}
	for (i = 0; i < MAXCUSTOM; i++) item[i] = 0;

	tmpenvlist = copyenv(NULL);
	tmpmacrolist = copystrarray(NULL, macrolist, &tmpmaxmacro, maxmacro);
	tmphelpindex = copystrarray(NULL, helpindex, NULL, 10);
	tmpbindlist = copybind(NULL, bindlist);
	for (i = 0; bindlist[i].key >= 0; i++);
# if	!MSDOS && !defined (_NOKEYMAP)
	tmpkeymaplist = copykeymap(NULL);
# endif
# ifndef	_NOARCHIVE
	tmplaunchlist = copylaunch(NULL, launchlist,
		&tmpmaxlaunch, maxlaunch);
	tmparchivelist = copyarch(NULL, archivelist,
		&tmpmaxarchive, maxarchive);
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
	tmpfdtype = copydrive(NULL, fdtype);
# endif
	calcmax(max, 1);

	changed = 0;
	custno = 0;
	cs_item = item[custno];
	cs_max = max[custno];
	cs_row = (FILEPERROW - 2) / 2;
	cs_len = (int *)malloc2(cs_row * sizeof(int));
	custtitle();
	old = -1;
	do {
		if (old < 0) dispcust();
		if (cs_item != old) movecust(old, 1);
		locate(win_x, win_y);
		tflush();
		keyflush();
# ifdef	_NOEDITMODE
		ch = Xgetkey(1, 0);
# else
		Xgetkey(-1, 0);
		ch = Xgetkey(1, 0);
		Xgetkey(-1, 0);
# endif

		old = cs_item;
		switch (ch) {
			case K_UP:
				if (cs_item > 0 && !((--cs_item + 1) % cs_row))
					old = -1;
				break;
			case K_DOWN:
				if (cs_item < cs_max - 1
				&& !(++cs_item % cs_row)) old = -1;
				break;
			case K_RIGHT:
				if (custno < MAXCUSTOM - 1) {
					item[custno] = cs_item;
					max[custno] = cs_max;
					custno++;
					custtitle();
					cs_item = item[custno];
					cs_max = max[custno];
					old = -1;
				}
				break;
			case K_LEFT:
				if (custno > 0) {
					item[custno] = cs_item;
					max[custno] = cs_max;
					custno--;
					custtitle();
					cs_item = item[custno];
					cs_max = max[custno];
					old = -1;
				}
				break;
			case K_PPAGE:
				if (cs_item < cs_row) putterm(t_bell);
				else {
					cs_item = (cs_item / cs_row - 1)
						* cs_row;
					old = -1;
				}
				break;
			case K_NPAGE:
				if ((cs_item / cs_row + 1) * cs_row >= cs_max)
					putterm(t_bell);
				else {
					cs_item = (cs_item / cs_row + 1)
						* cs_row;
					old = -1;
				}
				break;
			case K_BEG:
			case K_HOME:
			case '<':
				if (cs_item / cs_row) old = -1;
				cs_item = 0;
				break;
			case K_EOL:
			case K_END:
			case '>':
				if (cs_item / cs_row != (cs_max - 1) / cs_row)
					old = -1;
				cs_item = cs_max - 1;
				break;
			case K_CTRL('L'):
				rewritefile(1);
				break;
			case K_CR:
				if (!editcust()) {
					helpbar();
					old = cs_max;
					break;
				}
				if (custno < MAXCUSTOM - 1) {
					max[custno] = cs_max;
					changed = 1;
				}
				else {
					calcmax(max, 1);
					changed = 0;
				}
				rewritefile(1);
				break;
			case K_ESC:
				if (changed && !yesno(NOSAV_K)) ch = '\0';
				break;
			default:
				break;
		}
	} while (ch != K_ESC);

	custno = -1;
	free(cs_len);
	for (i = 0; i < ENVLISTSIZ * 2; i++)
		if (tmpenvlist[i]) free(tmpenvlist[i]);
	free(tmpenvlist);
	if (tmpmacrolist) {
		freestrarray(tmpmacrolist, tmpmaxmacro);
		free(tmpmacrolist);
	}
	if (tmphelpindex) {
		freestrarray(tmphelpindex, 10);
		free(tmphelpindex);
	}
	if (tmpbindlist) free(tmpbindlist);
# if	!MSDOS && !defined (_NOKEYMAP)
	freekeymap(tmpkeymaplist);
# endif
# ifndef	_NOARCHIVE
	if (tmplaunchlist) {
		freelaunch(tmplaunchlist, tmpmaxlaunch);
		free(tmplaunchlist);
	}
	if (tmparchivelist) {
		freearch(tmparchivelist, tmpmaxarchive);
		free(tmparchivelist);
	}
# endif
# if	!MSDOS && !defined (_NODOSDRIVE)
	if (tmpfdtype) {
		freedrive(tmpfdtype);
		free(tmpfdtype);
	}
# endif
	return(0);
}
#endif	/* !_NOCUSTOMIZE */
