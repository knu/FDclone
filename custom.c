/*
 *	custom.c
 *
 *	Customizer of configurations
 */

#include <signal.h>
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

#if	!MSDOS
# ifdef	_NODOSDRIVE
# include <sys/param.h>
# else
# include "dosdisk.h"
# endif
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

#define	MAXCUSTOM	7
#define	MAXCUSTNAM	14
#define	MAXCUSTVAL	(n_column - MAXCUSTNAM - 3)
#define	inputcuststr(p, d, s, h) \
			(inputstr(p, d, ((s) ? strlen(s) : 0), s, h))
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

static VOID NEAR _evalenv __P_((int));

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
# ifndef	_NOEXTRACOPY
	{"FD_INHERITCOPY", &inheritcopy, (char *)INHERITCOPY, IHTM_E, T_BOOL},
# endif
	{"FD_ADJTTY", &adjtty, (char *)ADJTTY, AJTY_E, T_BOOL},
#endif
	{"FD_COLUMNS", &defcolumns, (char *)COLUMNS, CLMN_E, T_COLUMN},
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
	{"FD_PROMPT", &promptstr, PROMPT, PRMP_E, T_CHARP},
#ifndef	_NOORIGSHELL
	{"FD_PS2", &promptstr2, PROMPT2, PRM2_E, T_CHARP},
#endif
#if	(!MSDOS && !defined (_NOKANJICONV)) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
	{"FD_LANGUAGE", &outputkcode, (char *)NOCNV, LANG_E, T_KOUT},
#endif
#if	!MSDOS && !defined (_NOKANJIFCONV)
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
};
#define	ENVLISTSIZ	((int)(sizeof(envlist) / sizeof(envtable)))


static VOID NEAR _evalenv(no)
int no;
{
	char *cp;
	int n;

	cp = getenv2(envlist[no].env);
	switch(envlist[no].type) {
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
#if	!MSDOS
			if ((n = atoi2(cp)) < 0) n = (int)(envlist[no].def);
			*((short *)(envlist[no].var)) = n;
			break;
#endif
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
			*((int *)(envlist[no].var))
				= getlang(cp, envlist[no].type - T_KIN);
			break;
#endif	/* (!MSDOS && !_NOKANJICONV) || (!_NOENGMES && !NOJPNMES) */
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

	for (i = 0; i < ENVLISTSIZ; i++) switch(envlist[i].type) {
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
