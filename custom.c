/*
 *	custom.c
 *
 *	Customizer of configurations
 */

#include <fcntl.h>
#include "fd.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#ifndef	_NOORIGSHELL
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
#if	!defined (_USEDOSCOPY) && !defined (_NOEXTRACOPY)
extern int inheritcopy;
#endif
#if	!MSDOS
extern int adjtty;
#endif
extern int hideclock;
extern int defcolumns;
extern int minfilename;
extern char *histfile;
extern short histsize[];
extern int savehist;
#ifndef	_NOTREE
extern int dircountlimit;
#endif
#ifndef	_NODOSDRIVE
extern int dosdrive;
#endif
extern int showsecond;
#ifndef	_NOTRADLAYOUT
extern int tradlayout;
#endif
extern int sizeinfo;
#ifndef	_NOCOLOR
extern int ansicolor;
extern char *ansipalette;
#endif
#ifndef	_NOEDITMODE
extern char *editmode;
#endif
#if	FD >= 2
extern int loopcursor;
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
#if	!defined (_NOKANJICONV) || (defined (FD) && !defined (_NODOSDRIVE))
extern int unicodebuffer;
#endif
#ifndef	_NOKANJIFCONV
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
extern char *utf8macpath;
extern char *noconvpath;
#endif	/* !_NOKANJIFCONV */
extern int tmpumask;
#ifndef	_NOORIGSHELL
extern int dumbshell;
#endif
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
# ifndef	_NOKEYMAP
extern keyseq_t *origkeymaplist;
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
# ifdef	_USEDOSEMU
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
	union {
		char *str;
		int num;
	} def;
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
#if	MSDOS
#define	T_DDRV		11
#else
#define	T_DDRV		T_BOOL
#endif
#define	T_COLOR		12
#define	T_EDIT		13
#define	T_KIN		14
#define	T_KOUT		15
#define	T_KNAM		16
#define	T_OCTAL		17

#if	FD >= 2
static int NEAR atooctal __P_((char *));
#endif
static VOID NEAR _evalenv __P_((int));
#ifndef	_NOCUSTOMIZE
static int NEAR custputs __P_((char *));
static char *NEAR strcatalloc __P_((char *, char *));
static VOID NEAR putsep __P_((VOID_A));
static VOID NEAR fillline __P_((int, int));
static VOID NEAR setnamelist __P_((int, namelist *, char *));
static int NEAR browsenamelist __P_((namelist *, int,
		int, char *, char *, char **));
static VOID NEAR custtitle __P_((VOID_A));
static VOID NEAR calcmax __P_((int [], int));
static VOID NEAR envcaption __P_((char *));
static char **NEAR copyenv __P_((char **));
static VOID NEAR cleanupenv __P_((VOID_A));
# if	FD >= 2
static char *NEAR ascoctal __P_((int, char *));
# endif
# ifndef	_NOORIGSHELL
static VOID NEAR putargs __P_((char *, int, char *[], FILE *));
# endif
static char *NEAR int2str __P_((char *, int));
static int NEAR dispenv __P_((int));
static int NEAR editenv __P_((int));
static int NEAR dumpenv __P_((char *, FILE *));
# ifndef	_NOORIGSHELL
static int NEAR checkenv __P_((char *, char **, int *, FILE *));
static int NEAR checkunset __P_((char *, int, char **, FILE *));
# endif
static VOID NEAR cleanupbind __P_((VOID_A));
static int NEAR dispbind __P_((int));
static int NEAR selectbind __P_((int, int, char *));
static int NEAR editbind __P_((int));
static int NEAR issamebind __P_((bindtable *, bindtable *));
static int NEAR dumpbind __P_((char *, char *, FILE *));
# ifndef	_NOORIGSHELL
static int NEAR checkbind __P_((char *, char *, int, char **, FILE *));
# endif
# ifndef	_NOKEYMAP
static VOID NEAR cleanupkeymap __P_((VOID_A));
static int NEAR dispkeymap __P_((int));
static int NEAR editkeymap __P_((int));
static int NEAR searchkeymap __P_((keyseq_t *, keyseq_t *));
static int NEAR issamekeymap __P_((keyseq_t *, keyseq_t *));
static int NEAR dumpkeymap __P_((char *, char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkkeymap __P_((char *, char *, int, char **, FILE *));
#  endif
# endif
# ifndef	_NOARCHIVE
static VOID NEAR cleanuplaunch __P_((VOID_A));
static int NEAR displaunch __P_((int));
static VOID NEAR verboselaunch __P_((launchtable *));
static char **NEAR editvar __P_((char *, char **));
static int NEAR editarchbrowser __P_((launchtable *));
static int NEAR editlaunch __P_((int));
static int NEAR issamelaunch __P_((launchtable *, launchtable *));
static int NEAR dumplaunch __P_((char *, char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checklaunch __P_((char *, char *, int, char **, FILE *));
#  endif
static VOID NEAR cleanuparch __P_((VOID_A));
static int NEAR disparch __P_((int));
static int NEAR editarch __P_((int));
static int NEAR issamearch __P_((archivetable *, archivetable *));
static int NEAR dumparch __P_((char *, char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkarch __P_((char *, char *, int, char **, FILE *));
#  endif
# endif
# ifdef	_USEDOSEMU
static VOID NEAR cleanupdosdrive __P_((VOID_A));
static int NEAR dispdosdrive __P_((int));
static int NEAR editdosdrive __P_((int));
static int NEAR dumpdosdrive __P_((char *, char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkdosdrive __P_((char *, char *, int, char **, FILE *));
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

static CONST envtable envlist[] = {
	{"FD_SORTTYPE", &sorttype, {(char *)SORTTYPE}, STTP_E, T_SORT},
	{"FD_DISPLAYMODE", &displaymode, {(char *)DISPLAYMODE}, DPMD_E, T_DISP},
#ifndef	_NOTREE
	{"FD_SORTTREE", &sorttree, {(char *)SORTTREE}, STTR_E, T_BOOL},
#endif
#ifndef	_NOWRITEFS
	{"FD_WRITEFS", &writefs, {(char *)WRITEFS}, WRFS_E, T_WRFS},
#endif
#if	!defined (PATHNOCASE) && (FD >= 2)
	{"FD_IGNORECASE", &pathignorecase,
		{(char *)IGNORECASE}, IGNC_E, T_BOOL},
#endif
#if	!defined (_USEDOSCOPY) && !defined (_NOEXTRACOPY)
	{"FD_INHERITCOPY", &inheritcopy, {(char *)INHERITCOPY}, IHTM_E, T_BOOL},
#endif
#if	!MSDOS
	{"FD_ADJTTY", &adjtty, {(char *)ADJTTY}, AJTY_E, T_BOOL},
# if	FD >= 2
	{"FD_USEGETCURSOR", &usegetcursor,
		{(char *)USEGETCURSOR}, UGCS_E, T_BOOL},
# endif
#endif	/* !MSDOS */
	{"FD_DEFCOLUMNS", &defcolumns, {(char *)DEFCOLUMNS}, CLMN_E, T_COLUMN},
	{"FD_MINFILENAME", &minfilename,
		{(char *)MINFILENAME}, MINF_E, T_NATURAL},
	{"FD_HISTFILE", &histfile, {HISTFILE}, HSFL_E, T_PATH},
	{"FD_HISTSIZE", &(histsize[0]), {(char *)HISTSIZE}, HSSZ_E, T_SHORT},
	{"FD_DIRHIST", &(histsize[1]), {(char *)DIRHIST}, DRHS_E, T_SHORT},
	{"FD_SAVEHIST", &savehist, {(char *)SAVEHIST}, SVHS_E, T_INT},
#ifndef	_NOTREE
	{"FD_DIRCOUNTLIMIT", &dircountlimit,
		{(char *)DIRCOUNTLIMIT}, DCLM_E, T_INT},
#endif
#ifndef	_NODOSDRIVE
	{"FD_DOSDRIVE", &dosdrive, {(char *)DOSDRIVE}, DOSD_E, T_DDRV},
#endif
	{"FD_SECOND", &showsecond, {(char *)SECOND}, SCND_E, T_BOOL},
#ifndef	_NOTRADLAYOUT
	{"FD_TRADLAYOUT", &tradlayout, {(char *)TRADLAYOUT}, TRLO_E, T_BOOL},
#endif
	{"FD_SIZEINFO", &sizeinfo, {(char *)SIZEINFO}, SZIF_E, T_BOOL},
#ifndef	_NOCOLOR
	{"FD_ANSICOLOR", &ansicolor, {(char *)ANSICOLOR}, ACOL_E, T_COLOR},
# if	FD >= 2
	{"FD_ANSIPALETTE", &ansipalette,
		{(char *)ANSIPALETTE}, APAL_E, T_CHARP},
# endif
#endif
#ifndef	_NOEDITMODE
	{"FD_EDITMODE", &editmode, {EDITMODE}, EDMD_E, T_EDIT},
#endif
#if	FD >= 2
	{"FD_LOOPCURSOR", &loopcursor, {(char *)LOOPCURSOR}, LPCS_E, T_BOOL},
#endif
	{"FD_TMPDIR", &deftmpdir, {TMPDIR}, TMPD_E, T_PATH},
#ifndef	_NOROCKRIDGE
	{"FD_RRPATH", &rockridgepath, {RRPATH}, RRPT_E, T_PATHS},
#endif
#ifndef	_NOPRECEDE
	{"FD_PRECEDEPATH", &precedepath, {PRECEDEPATH}, PCPT_E, T_PATHS},
#endif
#if	FD >= 2
	{"FD_PS1", &promptstr, {PROMPT}, PRMP_E, T_CHARP},
#else
	{"FD_PROMPT", &promptstr, {PROMPT}, PRMP_E, T_CHARP},
#endif
#ifndef	_NOORIGSHELL
	{"FD_PS2", &promptstr2, {PROMPT2}, PRM2_E, T_CHARP},
#endif
#if	!defined (_NOKANJICONV) || (defined (FD) && !defined (_NODOSDRIVE))
	{"FD_UNICODEBUFFER", &unicodebuffer, {UNICODEBUFFER}, UNBF_E, T_BOOL},
#endif
#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
	{"FD_LANGUAGE", &outputkcode, {(char *)NOCNV}, LANG_E, T_KOUT},
#endif
#ifndef	_NOKANJICONV
	{"FD_INPUTKCODE", &inputkcode, {(char *)NOCNV}, IPKC_E, T_KIN},
#endif
#ifndef	_NOKANJIFCONV
	{"FD_FNAMEKCODE", &fnamekcode, {(char *)NOCNV}, FNKC_E, T_KNAM},
	{"FD_SJISPATH", &sjispath, {SJISPATH}, SJSP_E, T_PATHS},
	{"FD_EUCPATH", &eucpath, {EUCPATH}, EUCP_E, T_PATHS},
	{"FD_JISPATH", &jis7path, {JISPATH}, JISP_E, T_PATHS},
	{"FD_JIS8PATH", &jis8path, {JIS8PATH}, JS8P_E, T_PATHS},
	{"FD_JUNETPATH", &junetpath, {JUNETPATH}, JNTP_E, T_PATHS},
	{"FD_OJISPATH", &ojis7path, {OJISPATH}, OJSP_E, T_PATHS},
	{"FD_OJIS8PATH", &ojis8path, {OJIS8PATH}, OJ8P_E, T_PATHS},
	{"FD_OJUNETPATH", &ojunetpath, {OJUNETPATH}, OJNP_E, T_PATHS},
	{"FD_HEXPATH", &hexpath, {HEXPATH}, HEXP_E, T_PATHS},
	{"FD_CAPPATH", &cappath, {CAPPATH}, CAPP_E, T_PATHS},
	{"FD_UTF8PATH", &utf8path, {UTF8PATH}, UTF8P_E, T_PATHS},
	{"FD_UTF8MACPATH", &utf8macpath, {UTF8MACPATH}, UTF8MACP_E, T_PATHS},
	{"FD_NOCONVPATH", &noconvpath, {NOCONVPATH}, NCVP_E, T_PATHS},
#endif	/* !_NOKANJIFCONV */
#if	FD >= 2
	{"FD_TMPUMASK", &tmpumask, {(char *)TMPUMASK}, TUMSK_E, T_OCTAL},
#endif
#ifndef	_NOORIGSHELL
	{"FD_DUMBSHELL", &dumbshell, {(char *)DUMBSHELL}, DMSHL_E, T_BOOL},
#endif
};
#define	ENVLISTSIZ	((int)(sizeof(envlist) / sizeof(envtable)))

#ifndef	_NOCUSTOMIZE
# ifdef	_USEDOSEMU
static CONST devinfo mediadescr[] = {
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
# endif	/* _USEDOSEMU */
static int cs_item = 0;
static int cs_max = 0;
static int cs_row = 0;
static int *cs_len = NULL;
static char **tmpenvlist = NULL;
static char **tmpmacrolist = NULL;
static int tmpmaxmacro = 0;
static char **tmphelpindex = NULL;
static bindtable *tmpbindlist = NULL;
# ifndef	_NOKEYMAP
static keyseq_t *tmpkeymaplist = NULL;
static short *keyseqlist = NULL;
# endif
# ifndef	_NOARCHIVE
static launchtable *tmplaunchlist = NULL;
static int tmpmaxlaunch = 0;
static archivetable *tmparchivelist = NULL;
static int tmpmaxarchive = 0;
# endif
# ifdef	_USEDOSEMU
static devinfo *tmpfdtype = NULL;
# endif
#endif	/* !_NOCUSTOMIZE */


#if	FD >= 2
static int NEAR atooctal(s)
char *s;
{
	int n;

	if (!sscanf2(s, "%o%$", &n)) return(-1);
	n &= 0777;
	return(n);
}
#endif	/* FD >= 2 */

static VOID NEAR _evalenv(no)
int no;
{
	char *cp;
	int n;

	cp = getenv2(envlist[no].env);
	switch (envlist[no].type) {
		case T_BOOL:
			if (!cp) n = envlist[no].def.num;
			else n = (*cp && atoi2(cp)) ? 1 : 0;
			*((int *)(envlist[no].var)) = n;
			break;
		case T_SHORT:
			if ((n = atoi2(cp)) < 0) n = envlist[no].def.num;
			*((short *)(envlist[no].var)) = n;
			break;
		case T_INT:
			if ((n = atoi2(cp)) < 0) n = envlist[no].def.num;
			*((int *)(envlist[no].var)) = n;
			break;
		case T_NATURAL:
			if ((n = atoi2(cp)) <= 0) n = envlist[no].def.num;
			*((int *)(envlist[no].var)) = n;
			break;
		case T_PATH:
			if (!cp) cp = envlist[no].def.str;
			cp = evalpath(strdup2(cp), 1);
			if (*((char **)(envlist[no].var)))
				free(*((char **)(envlist[no].var)));
			*((char **)(envlist[no].var)) = cp;
			break;
		case T_PATHS:
			if (!cp) cp = envlist[no].def.str;
			cp = evalpaths(cp, ':');
			if (*((char **)(envlist[no].var)))
				free(*((char **)(envlist[no].var)));
			*((char **)(envlist[no].var)) = cp;
			break;
		case T_SORT:
			if (((n = atoi2(cp)) < 0 || (n & 7) > 5)
			&& (n < 100 || ((n - 100) & 7) > 5))
				n = envlist[no].def.num;
			*((int *)(envlist[no].var)) = n;
			break;
		case T_DISP:
#ifdef	HAVEFLAGS
			if ((n = atoi2(cp)) < 0 || n > 15)
#else
			if ((n = atoi2(cp)) < 0 || n > 7)
#endif
				n = envlist[no].def.num;
			*((int *)(envlist[no].var)) = n;
			break;
#ifndef	_NOWRITEFS
		case T_WRFS:
			if ((n = atoi2(cp)) < 0 || n > 2)
				n = envlist[no].def.num;
			*((int *)(envlist[no].var)) = n;
			break;
#endif
		case T_COLUMN:
			if ((n = atoi2(cp)) < 0 || n > 5 || n == 4)
				n = envlist[no].def.num;
			*((int *)(envlist[no].var)) = n;
			break;
#if	MSDOS && !defined (_NODOSDRIVE)
		case T_DDRV:
			if (!cp) n = envlist[no].def.num;
			else {
				char *dupl;

				n = 0;
				dupl = strdup2(cp);
				if ((cp = strchr(dupl, ','))) {
					*(cp++) = '\0';
					if (!strcmp(cp, "BIOS")) n |= 2;
				}
				if (*dupl && atoi2(dupl)) n |= 1;
				free(dupl);
			}
			*((int *)(envlist[no].var)) = n;
			break;
#endif	/* MSDOS && !_NODOSDRIVE */
#ifndef	_NOCOLOR
		case T_COLOR:
			if ((n = atoi2(cp)) < 0 || n > 3)
				n = envlist[no].def.num;
			*((int *)(envlist[no].var)) = n;
			break;
#endif
#ifndef	_NOEDITMODE
		case T_EDIT:
			if (!cp) cp = envlist[no].def.str;
			else if (strcmp(cp, "emacs")
			&& strcmp(cp, "vi")
			&& strcmp(cp, "wordstar")) cp = NULL;
			*((char **)(envlist[no].var)) = cp;
			break;
#endif	/* !_NOEDITMODE */
#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
		case T_KIN:
		case T_KOUT:
		case T_KNAM:
			*((int *)(envlist[no].var)) =
				getlang(cp, envlist[no].type - T_KIN);
			break;
#endif	/* !_NOKANJICONV || (!_NOENGMES && !NOJPNMES) */
#if	FD >= 2
		case T_OCTAL:
			if ((n = atooctal(cp)) < 0) n = envlist[no].def.num;
			*((int *)(envlist[no].var)) = n;
			break;
#endif
		default:
			if (!cp) cp = envlist[no].def.str;
			*((char **)(envlist[no].var)) = cp;
			break;
	}
}

VOID evalenv(VOID_A)
{
	int i, duperrno;

	duperrno = errno;
	for (i = 0; i < ENVLISTSIZ; i++) _evalenv(i);
	errno = duperrno;
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
static int NEAR custputs(s)
char *s;
{
	return(cprintf2("%.*k", n_lastcolumn, s));
}

static char *NEAR strcatalloc(s1, s2)
char *s1, *s2;
{
	int l1, l2;

	l1 = (s1) ? strlen(s1) : 0;
	l2 = strlen(s2);
	s1 = realloc2(s1, l1 + l2 + 1);
	strncpy2(&(s1[l1]), s2, l2);
	return(s1);
}

static VOID NEAR putsep(VOID_A)
{
	putterm(t_standout);
	putch2(' ');
	putterm(end_standout);
}

static VOID NEAR fillline(y, w)
int y, w;
{
	locate(0, y);
	putterm(t_standout);
	cprintf2("%*s", w, "");
	putterm(end_standout);
}

static VOID NEAR setnamelist(n, list, s)
int n;
namelist *list;
char *s;
{
	memset((char *)&(list[n]), 0, sizeof(namelist));
	list[n].name = s;
	list[n].flags = (F_ISRED | F_ISWRI);
	list[n].ent = n;
	list[n].tmpflags = F_STAT;
}

static int NEAR browsenamelist(list, max, col, def, prompt, mes)
namelist *list;
int max, col;
char *def, *prompt, **mes;
{
	int ch, pos, old;
	int dupwin_x, dupwin_y, dupminfilename, dupcolumns;

	dupminfilename = minfilename;
	dupcolumns = curcolumns;
	minfilename = n_column;
	curcolumns = col;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	pos = listupfile(list, max, def);

	envcaption(prompt);
	do {
		locate(0, L_INFO);
		putterm(l_clear);
		custputs(mes[pos] ? mes[pos] : list[pos].name);
		putch2('.');

		locate(win_x, win_y);
		tflush();
		keyflush();
		Xgetkey(-1, 0);
		ch = Xgetkey(1, 0);
		Xgetkey(-1, 0);

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

	return((ch == K_CR) ? pos : -1);
}

static VOID NEAR custtitle(VOID_A)
{
	char *str[MAXCUSTOM];
	int i, len, max, width;

	str[0] = TENV_K;
	str[1] = TBIND_K;
	str[2] = TKYMP_K;
	str[3] = TLNCH_K;
	str[4] = TARCH_K;
	str[5] = TSDRV_K;
	str[6] = TSAVE_K;

	for (i = width = max = 0; i < MAXCUSTOM; i++) {
		len = strlen2(str[i]);
		width += len + 2;
		if (max < len) max = len;
	}
	width--;
	len = 1;
	if (width <= n_column) width = max;
	else {
		len = 0;
		if (width - (MAXCUSTOM - 1) <= n_column) width = max;
		else width = n_column / MAXCUSTOM - 1;
	}

	locate(0, LFILETOP);
	putterm(l_clear);
	for (i = 0; i < MAXCUSTOM; i++) {
		if (i != custno) cprintf2("/%.*k", width, str[i]);
		else {
			putterm(t_standout);
			cprintf2("/%.*k", width, str[i]);
			putterm(end_standout);
		}
		if (len) putch2(' ');
	}

	fillline(LFILETOP + 1, n_column);
}

static VOID NEAR calcmax(max, new)
int max[], new;
{
	int i;

	max[0] = ENVLISTSIZ;
	for (i = 0; bindlist[i].key >= 0; i++);
	max[1] = i + new;
# ifdef	_NOKEYMAP
	max[2] = new;
# else
	for (i = 0; keyseqlist[i] >= 0; i++);
	max[2] = i;
# endif
# ifdef	_NOARCHIVE
	max[3] = new;
	max[4] = new;
# else
	max[3] = maxlaunch + new;
	max[4] = maxarchive + new;
# endif
# ifdef	_USEDOSEMU
	for (i = 0; fdtype[i].name; i++);
	max[5] = i + new;
# else
	max[5] = new;
# endif
	max[6] = (new) ? MAXSAVEMENU : 0;
}

static VOID NEAR envcaption(s)
char *s;
{
	locate(0, L_HELP);
	putterm(l_clear);
	putterm(t_standout);
	cprintf2("%.*k", n_column, s);
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
			setenv2(envlist[i].env, list[i * 2], 0);
			setenv2(&(envlist[i].env[FDESIZ]),
				list[i * 2 + 1], 0);
			_evalenv(i);
		}
	}
	else {
		list = (char **)malloc2(ENVLISTSIZ * 2 * sizeof(char *));
		for (i = 0; i < ENVLISTSIZ; i++) {
			list[i * 2] = strdup2(getshellvar(envlist[i].env, -1));
			list[i * 2 + 1] =
				strdup2(getshellvar(&(envlist[i].env[FDESIZ]),
					-1));
		}
	}
	return(list);
}

static VOID NEAR cleanupenv(VOID_A)
{
	char *cp;
	int i;

	for (i = 0; i < ENVLISTSIZ; i++) {
		setenv2(envlist[i].env, NULL, 0);
		cp = (envlist[i].type == T_CHARP) ? envlist[i].def.str : NULL;
		setenv2(&(envlist[i].env[FDESIZ]), cp, 0);
		_evalenv(i);
	}
}

# if	FD >= 2
static char *NEAR ascoctal(n, buf)
int n;
char *buf;
{
# ifdef	BASHSTYLE
	snprintf2(buf, MAXLONGWIDTH + 1, "%03o", n & 0777);
# else
	snprintf2(buf, MAXLONGWIDTH + 1, "%04o", n & 0777);
# endif
	return(buf);
}
# endif	/* FD >= 2 */

# ifndef	_NOORIGSHELL
static VOID NEAR putargs(s, argc, argv, fp)
char *s;
int argc;
char *argv[];
FILE *fp;
{
	int i;

	if (!argc) return;

	fputs(s, fp);
	for (i = 0; i < argc; i++) fprintf2(fp, " %s", argv[i]);
	fputnl(fp);
}
# endif	/* _NOORIGSHELL */

static char *NEAR int2str(buf, n)
char *buf;
int n;
{
	snprintf2(buf, MAXLONGWIDTH + 1, "%d", n);
	return(buf);
}

static int NEAR dispenv(no)
int no;
{
	char *cp, *new, buf[MAXLONGWIDTH + 1], *str[MAXSELECTSTRS];
	int n, p;

	new = NULL;
	switch (envlist[no].type) {
		case T_BOOL:
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			cp = str[(*((int *)(envlist[no].var))) ? 1 : 0];
			break;
		case T_SHORT:
			cp = int2str(buf, *((short *)(envlist[no].var)));
			break;
		case T_INT:
		case T_NATURAL:
		case T_COLUMN:
			cp = int2str(buf, *((int *)(envlist[no].var)));
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
			new = strdup2(&(str[n & 7][3]));

			if (n & 7) {
				str[0] = OINC_K;
				str[1] = ODEC_K;
				new = strcatalloc(new, "(");
				new = strcatalloc(new, &(str[n / 8][3]));
				new = strcatalloc(new, ")");
			}
			if (p) {
				new = strcatalloc(new, ", ");
				new = strcatalloc(new, VSORT_K);
			}
			cp = new;
			break;
		case T_DISP:
			n = *((int *)(envlist[no].var));
			new = strdup2(VDS1A_K);
			str[0] = VDS10_K;
			str[1] = VDS11_K;
			new = strcatalloc(new, str[n & 1]);
			new = strcatalloc(new, VDS2A_K);
			str[0] = VDS20_K;
			str[1] = VDS21_K;
			new = strcatalloc(new, str[(n >> 1) & 1]);
			new = strcatalloc(new, VDS3A_K);
			str[0] = VDS30_K;
			str[1] = VDS31_K;
			new = strcatalloc(new, str[(n >> 2) & 1]);
# ifdef	HAVEFLAGS
			new = strcatalloc(new, VDS4A_K);
			str[0] = VDS40_K;
			str[1] = VDS41_K;
			new = strcatalloc(new, str[(n >> 3) & 1]);
# endif
			cp = new;
			break;
# ifndef	_NOWRITEFS
		case T_WRFS:
			str[0] = VWFS0_K;
			str[1] = VWFS1_K;
			str[2] = VWFS2_K;
			cp = str[*((int *)(envlist[no].var))];
			break;
# endif
# if	MSDOS && !defined (_NODOSDRIVE)
		case T_DDRV:
			n = *((int *)(envlist[no].var));
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			cp = str[n & 1];
			if (n & 2) {
				cp = strdup2(cp);
				cp = new = strcatalloc(cp, VBIOS_K);
			}
			break;
# endif	/* MSDOS && !_NODOSDRIVE */
# ifndef	_NOCOLOR
		case T_COLOR:
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			str[2] = VCOL2_K;
			str[3] = VCOL3_K;
			cp = str[*((int *)(envlist[no].var))];
			break;
# endif	/* !_NOCOLOR */
# ifndef	_NOEDITMODE
		case T_EDIT:
			if (!(cp = *((char **)(envlist[no].var))))
				cp = VNONE_K;
			break;
# endif	/* !_NOEDITMODE */
# if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
		case T_KIN:
		case T_KOUT:
		case T_KNAM:
			str[NOCNV] = VNCNV_K;
			str[ENG] = VENG_K;
#  ifndef	_NOKANJICONV
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
			str[M_UTF8] = "UTF-8 for Mac OS X";
#  endif	/* !_NOKANJICONV */
			cp = str[*((int *)(envlist[no].var))];
			break;
# endif	/* !_NOKANJICONV || (!_NOENGMES && !NOJPNMES) */
# if	FD >= 2
		case T_OCTAL:
			cp = ascoctal(*((int *)(envlist[no].var)), buf);
			break;
# endif
		default:
			if (!(cp = getenv2(envlist[no].env))) {
				cp = strdup2(envlist[no].def.str);
				if (cp && *cp) cp = strcatalloc(cp, " ");
				cp = new = strcatalloc(cp, VDEF_K);
			}
			break;
	}
	cprintf2("%-*.*k", MAXCUSTVAL, MAXCUSTVAL, cp);
	n = strlen2(cp);
	if (n > MAXCUSTVAL - 1) n = MAXCUSTVAL - 1;
	if (new) free(new);
	return(n);
}

static int NEAR editenv(no)
int no;
{
	char *cp, *new, *env, buf[MAXLONGWIDTH + 1], *str[MAXSELECTSTRS];
	int n, p, tmp, val[MAXSELECTSTRS];

	for (n = 0; n < MAXSELECTSTRS; n++) val[n] = n;

	new = NULL;
	env = &(envlist[no].env[FDESIZ]);
	switch (envlist[no].type) {
		case T_BOOL:
			n = (*((int *)(envlist[no].var))) ? 1 : 0;
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			envcaption(env);
			if (noselect(&n, 2, 0, str, val)) return(0);
			cp = int2str(buf, n);
			break;
		case T_SHORT:
			int2str(buf, *((short *)(envlist[no].var)));
			cp = inputcuststr(env, 1, buf, -1);
			if (!cp) return(0);
			n = atoi2(cp);
			free(cp);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			cp = int2str(buf, n);
			break;
		case T_INT:
		case T_NATURAL:
			int2str(buf, *((int *)(envlist[no].var)));
			cp = inputcuststr(env, 1, buf, -1);
			if (!cp) return(0);
			n = atoi2(cp);
			free(cp);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			cp = int2str(buf, n);
			break;
		case T_PATH:
			if (!(cp = getenv2(envlist[no].env)))
				cp = envlist[no].def.str;
			new = inputcuststr(env, 1, cp, 1);
			if (!new) return(0);
			cp = new;
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
			envcaption(env);
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
			cp = int2str(buf, n);
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
			cp = int2str(buf, n);
			break;
# ifndef	_NOWRITEFS
		case T_WRFS:
			n = *((int *)(envlist[no].var));
			str[0] = VWFS0_K;
			str[1] = VWFS1_K;
			str[2] = VWFS2_K;
			envcaption(env);
			if (noselect(&n, 3, 0, str, val)) return(0);
			cp = int2str(buf, n);
			break;
# endif	/* !_NOWRITEFS */
# if	MSDOS && !defined (_NODOSDRIVE)
		case T_DDRV:
			n = (*((int *)(envlist[no].var)) & 1);
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			envcaption(env);
			if (noselect(&n, 2, 0, str, val)) return(0);
			cp = int2str(buf, n);
			if (*((int *)(envlist[no].var)) & 2) {
				cp = strdup2(cp);
				cp = new = strcatalloc(cp, ",BIOS");
			}
			break;
# endif	/* MSDOS && !_NODOSDRIVE */
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
			envcaption(env);
			if (noselect(&n, 4, 0, str, val)) return(0);
			cp = int2str(buf, n);
			break;
# ifndef	_NOCOLOR
		case T_COLOR:
			n = *((int *)(envlist[no].var));
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			str[2] = VCOL2_K;
			str[3] = VCOL3_K;
			envcaption(env);
			if (noselect(&n, 4, 0, str, val)) return(0);
			cp = int2str(buf, n);
			break;
# endif	/* !_NOCOLOR */
# ifndef	_NOEDITMODE
		case T_EDIT:
			cp = *((char **)(envlist[no].var));
			str[0] = "emacs";
			str[1] = "wordstar";
			str[2] = "vi";
			if (!cp) n = 0;
			else for (n = 2; n > 0; n--)
				if (!strcmp(cp, str[n])) break;
			envcaption(env);
			if (noselect(&n, 3, 0, str, val)) return(0);
			cp = str[n];
			break;
# endif	/* !_NOEDITMODE */
# ifndef	_NOKANJICONV
		case T_KIN:
			n = *((int *)(envlist[no].var));
			str[0] = "ShiftJIS";
			str[1] = "EUC-JP";
			val[0] = SJIS;
			val[1] = EUC;
			envcaption(env);
			if (noselect(&n, 2, 0, str, val)) return(0);
			str[SJIS] = "sjis";
			str[EUC] = "euc";
			cp = str[n];
			break;
# endif	/* !_NOKANJICONV */
# if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
		case T_KOUT:
			n = *((int *)(envlist[no].var));
			str[0] = VNCNV_K;
			str[1] = VENG_K;
			val[0] = NOCNV;
			val[1] = ENG;
#  ifdef	_NOKANJICONV
			envcaption(env);
			if (noselect(&n, 2, 0, str, val)) return(0);
#  else	/* !_NOKANJICONV */

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
			if (n != O_JIS7 && n != O_JIS8 && n != O_JUNET
			&& n != M_UTF8) p = 0;
			else {
				p = 1;
				n--;
			}
			envcaption(env);
			if (noselect(&n, 8, 0, str, val)) return(0);
			if (n >= JIS7 && n <= JUNET) {
				str[0] = VNJIS_K;
				str[1] = VOJIS_K;
				val[0] = 0;
				val[1] = 1;
				if (noselect(&p, 2, 64, str, val)) return(0);
				n += p;
			}
			else if (n == UTF8) {
				str[0] = VUTF8_K;
				str[1] = VUTFM_K;
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
			str[M_UTF8] = "utf8-mac";
#  endif	/* !_NOKANJICONV */
			str[NOCNV] = "";
			str[ENG] = "C";
			cp = str[n];
			break;
# endif	/* !_NOKANJICONV || (!_NOENGMES && !NOJPNMES) */
# ifndef	_NOKANJIFCONV
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
			if (n != O_JIS7 && n != O_JIS8 && n != O_JUNET
			&& n != M_UTF8) p = 0;
			else {
				p = 1;
				n--;
			}
			envcaption(env);
			if (noselect(&n, 9, 0, str, val)) return(0);
			if (n >= JIS7 && n <= JUNET) {
				str[0] = VNJIS_K;
				str[1] = VOJIS_K;
				val[0] = 0;
				val[1] = 1;
				if (noselect(&p, 2, 64, str, val)) return(0);
				n += p;
			}
			else if (n == UTF8) {
				str[0] = VUTF8_K;
				str[1] = VUTFM_K;
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
			str[M_UTF8] = "utf8-mac";
			cp = str[n];
			break;
# endif	/* !_NOKANJIFCONV */
# if	FD >= 2
		case T_OCTAL:
			ascoctal(*((int *)(envlist[no].var)), buf);
			cp = inputcuststr(env, 1, buf, -1);
			if (!cp) return(0);
			n = atooctal(cp);
			free(cp);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			cp = ascoctal(n, buf);
			break;
# endif	/* FD >= 2 */
		default:
			if (!(cp = getenv2(envlist[no].env)))
				cp = envlist[no].def.str;
			new = inputcuststr(env, 0, cp, -1);
			if (!new) return(0);
			cp = new;
			break;
	}

	if (getshellvar(envlist[no].env, -1)) setenv2(envlist[no].env, cp, 0);
	else setenv2(env, cp, 0);
	if (new) free(new);
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
		&& (cp = getshellvar(&(envlist[i].env[FDESIZ]), -1))
		&& (envlist[i].type != T_CHARP
		|| strenvcmp(cp, envlist[i].def.str))) n++;
		if ((!flaglist || !(flaglist[i] & 1))
		&& getshellvar(envlist[i].env, -1)) n++;
	}
	if (!n || !fp) return(n);

	fputnl(fp);
	fputs("# shell variables definition\n", fp);
	for (i = 0; i < ENVLISTSIZ; i++) {
		if ((!flaglist || !(flaglist[i] & 2))
		&& (cp = getshellvar(&(envlist[i].env[FDESIZ]), -1))
		&& (envlist[i].type != T_CHARP
		|| strenvcmp(cp, envlist[i].def.str))) {
			cp = killmeta(cp);
			fprintf2(fp, "%s=%s\n", &(envlist[i].env[FDESIZ]), cp);
			free(cp);
		}
		if ((!flaglist || !(flaglist[i] & 1))
		&& (cp = getshellvar(envlist[i].env, -1))) {
			cp = killmeta(cp);
			fprintf2(fp, "%s=%s\n", envlist[i].env, cp);
			free(cp);
		}
	}
	return(n);
}

# ifndef	_NOORIGSHELL
static int NEAR checkenv(flaglist, argv, len, fp)
char *flaglist, *argv[];
int *len;
FILE *fp;
{
	char *cp, *ident, **unset, **trash;
	int i, n, ns, nu, nt, f;

	for (n = 0; argv[n]; n++) {
		for (i = 0; i < ENVLISTSIZ; i++) {
			ident = envlist[i].env;
			if (!strnenvcmp(argv[n], ident, len[n])) break;
			ident += FDESIZ;
			if (!strnenvcmp(argv[n], ident, len[n])) break;
		}
		if (i < ENVLISTSIZ) break;
	}
	if (!argv[n]) return(0);

	unset = trash = NULL;
	ns = nu = nt = 0;
	for (n = 0; argv[n]; n++) {
#  ifdef	FAKEUNINIT
		f = 0;	/* fake for -Wuninitialized */
#  endif
		for (i = 0; i < ENVLISTSIZ; i++) {
			ident = envlist[i].env;
			if (!strnenvcmp(argv[n], ident, len[n])) {
				f = 1;
				break;
			}
			ident += FDESIZ;
			if (!strnenvcmp(argv[n], ident, len[n])) {
				f = 2;
				break;
			}
		}
		if (i >= ENVLISTSIZ) {
			if (ns++) fputc(' ', fp);
			fputs(argv[n], fp);
		}
		else if (flaglist && (flaglist[i] & f)) {
			trash = (char **)realloc2(trash,
				(nt + 1) * sizeof(char *));
			trash[nt++] = argv[n];
		}
		else {
			if (flaglist) flaglist[i] |= f;
			if ((cp = getshellvar(ident, -1))
			&& (envlist[i].type != T_CHARP
			|| strenvcmp(cp, envlist[i].def.str))) {
				if (ns++) fputc(' ', fp);
				cp = killmeta(cp);
				fprintf2(fp, "%s=%s", ident, cp);
				free(cp);
			}
			else {
				unset = (char **)realloc2(unset,
					(nu + 1) * sizeof(char *));
				unset[nu++] = ident;
			}
		}
	}
	if (ns) fputnl(fp);

	if (unset) {
		putargs(BL_UNSET, nu, unset, fp);
		free(unset);
	}
	if (trash) {
		putargs("#", nt, trash, fp);
		free(trash);
	}

	return(1);
}

static int NEAR checkunset(flaglist, argc, argv, fp)
char *flaglist;
int argc;
char *argv[];
FILE *fp;
{
	char *cp, *ident, **unset, **trash;
	int i, n, ns, nu, nt, f;

	if (strcommcmp(argv[0], BL_UNSET)) return(0);
	if (argc < 2) return(-1);

	for (n = 1; argv[n]; n++) {
		for (i = 0; i < ENVLISTSIZ; i++) {
			ident = envlist[i].env;
			if (!strenvcmp(argv[n], ident)) break;
			ident += FDESIZ;
			if (!strenvcmp(argv[n], ident)) break;
		}
		if (i < ENVLISTSIZ) break;
	}
	if (!argv[n]) return(0);

	unset = trash = NULL;
	ns = nu = nt = 0;
	for (n = 1; n < argc; n++) {
#  ifdef	FAKEUNINIT
		f = 0;	/* fake for -Wuninitialized */
#  endif
		for (i = 0; i < ENVLISTSIZ; i++) {
			ident = envlist[i].env;
			if (!strenvcmp(argv[n], ident)) {
				f = 1;
				break;
			}
			ident += FDESIZ;
			if (!strenvcmp(argv[n], ident)) {
				f = 2;
				break;
			}
		}
		if (i >= ENVLISTSIZ) {
			unset = (char **)realloc2(unset,
				(nu + 1) * sizeof(char *));
			unset[nu++] = argv[n];
		}
		else if (flaglist && (flaglist[i] & f)) {
			trash = (char **)realloc2(trash,
				(nt + 1) * sizeof(char *));
			trash[nt++] = ident;
		}
		else {
			if (flaglist) flaglist[i] |= f;
			if ((cp = getshellvar(ident, -1))
			&& (envlist[i].type != T_CHARP
			|| strenvcmp(cp, envlist[i].def.str))) {
				if (ns++) fputc(' ', fp);
				cp = killmeta(cp);
				fprintf2(fp, "%s=%s", ident, cp);
				free(cp);
			}
			else {
				unset = (char **)realloc2(unset,
					(nu + 1) * sizeof(char *));
				unset[nu++] = ident;
			}
		}
	}
	if (ns) fputnl(fp);

	if (unset) {
		putargs(BL_UNSET, nu, unset, fp);
		free(unset);
	}
	if (trash) {
		fputs("# ", fp);
		putargs(BL_UNSET, nt, trash, fp);
		free(trash);
	}

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
	int len, width;

	if (bindlist[no].key < 0) {
		cprintf2("%*s", MAXCUSTVAL, "");
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
	if (!cp2) {
		width = MAXCUSTVAL;
		cprintf2("%-*.*k", width, width, cp1);
	}
	else {
		width = (MAXCUSTVAL - 1) / 2;
		cprintf2("%-*.*k", width, width, cp1);
		putsep();
		cprintf2("%-*.*k",
			MAXCUSTVAL - 1 - width, MAXCUSTVAL - 1 - width, cp2);
	}
	len = strlen2(cp1);
	if (len > --width) len = width;
	return(len);
}

static int NEAR selectbind(n, max, prompt)
int n, max;
char *prompt;
{
	namelist *list;
	char *cp, **mes;
	int i, dupsorton;

	max += FUNCLISTSIZ;

	list = (namelist *)malloc2((FUNCLISTSIZ + 2) * sizeof(namelist));
	mes = (char **)malloc2((FUNCLISTSIZ + 2) * sizeof(char *));
	for (i = 0; i < FUNCLISTSIZ; i++)
		setnamelist(i, list, funclist[i].ident);
	setnamelist(i, list, USRDF_K);
	mes[i++] = NULL;
	setnamelist(i, list, DELKB_K);
	mes[i] = NULL;

	dupsorton = sorton;
	sorton = 1;
	qsort(list, FUNCLISTSIZ, sizeof(namelist), cmplist);
	sorton = dupsorton;
	for (i = 0; i < FUNCLISTSIZ; i++)
		mes[i] = mesconv(funclist[list[i].ent].hmes,
			funclist[list[i].ent].hmes_eng);

	cp = (n < FUNCLISTSIZ) ? funclist[n].ident : list[FUNCLISTSIZ].name;
	n = browsenamelist(list, max, 5, cp, prompt, mes);

	if (n >= 0) n = list[n].ent;
	free(list);
	free(mes);

	return (n);
}

static int NEAR editbind(no)
int no;
{
	char *cp, *str, *buf;
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
		win_x = custputs(BINDK_K);
		win_y = L_INFO;
		locate(win_x, win_y);
		tflush();
		keyflush();
		Xgetkey(-1, 0);
		key = Xgetkey(1, 0);
		Xgetkey(-1, 0);
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
		buf = asprintf3(BINDF_K, str);
		n1 = selectbind(n1, i, buf);
		free(buf);
		if (n1 < 0) return(-1);
		else if (n1 < FUNCLISTSIZ) break;
		else if (n1 == FUNCLISTSIZ + 1) {
			if (!yesno(DLBOK_K, str)) return(-1);
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
			buf = asprintf3(BNDFC_K, str);
			cp = inputcuststr(buf, 0, cp, 0);
			free(buf);
			if (!cp);
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
		buf = asprintf3(BINDD_K, str);
		n2 = selectbind(i = n2, 1, buf);
		free(buf);
		if (n2 < 0) {
			freemacro(n1);
			return(-1);
		}
		else if (n2 < FUNCLISTSIZ) break;
		else if (maxmacro >= MAXMACROTABLE) warning(0, OVERF_K);
		else {
			if (bindlist[no].key < 0 || i < FUNCLISTSIZ) cp = NULL;
			else cp = macrolist[i - FUNCLISTSIZ];
			buf = asprintf3(BNDDC_K, str);
			cp = inputcuststr(buf, 0, cp, 0);
			free(buf);
			if (!cp);
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

static int NEAR issamebind(bindp1, bindp2)
bindtable *bindp1, *bindp2;
{
	if (ismacro(bindp1 -> f_func) || bindp1 -> f_func != bindp2 -> f_func)
		return(0);
	if (ismacro(bindp1 -> d_func) || bindp1 -> d_func != bindp2 -> d_func)
		return(0);
	if (gethelp(bindp1)) return(0);
	return(1);
}

static int NEAR dumpbind(flaglist, origflaglist, fp)
char *flaglist, *origflaglist;
FILE *fp;
{
	char *new;
	int i, j, n;

	if (origflaglist) new = NULL;
	else {
		for (n = 0; origbindlist[n].key >= 0; n++);
		origflaglist = new = malloc2(n * sizeof(char));
		memset(origflaglist, 0, n * sizeof(char));
	}

	for (i = n = 0; bindlist[i].key >= 0; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searchkeybind(origbindlist, &(bindlist[i]));
		if (j < MAXBINDTABLE && origbindlist[j].key >= 0) {
			origflaglist[j] = 1;
			if (issamebind(&(bindlist[i]), &(origbindlist[j])))
				continue;
		}
		n++;
	}
	for (i = 0; origbindlist[i].key >= 0; i++) {
		if (origflaglist[i]) continue;
		n++;
	}

	if (!n || !fp) {
		if (new) free(new);
		return(n);
	}

	fputnl(fp);
	fputs("# key bind definition\n", fp);
	for (i = 0; bindlist[i].key >= 0; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searchkeybind(origbindlist, &(bindlist[i]));
		if (j < MAXBINDTABLE && origbindlist[j].key >= 0
		&& issamebind(&(bindlist[i]), &(origbindlist[j])))
			continue;

		printmacro(bindlist, i, 1, fp);
	}
	for (i = 0; origbindlist[i].key >= 0; i++) {
		if (origflaglist[i]) continue;
		printmacro(origbindlist, i, 0, fp);
	}
	if (new) free(new);
	return(n);
}

# ifndef	_NOORIGSHELL
static int NEAR checkbind(flaglist, origflaglist, argc, argv, fp)
char *flaglist, *origflaglist;
int argc;
char *argv[];
FILE *fp;
{
	bindtable bind;
	int i, j, n;

	if (strcommcmp(argv[0], BL_BIND)) return(0);
	if (parsekeybind(argc, argv, &bind) < 0) return(-1);
	n = (bind.d_func == 255) ? 3 : 4;

	i = searchkeybind(bindlist, &bind);
	j = searchkeybind(origbindlist, &bind);
	if (i < MAXBINDTABLE && bindlist[i].key >= 0) {
		if (!flaglist || flaglist[i]) return(-1);
		if (j < MAXBINDTABLE && origbindlist[j].key >= 0) {
			if (!origflaglist || origflaglist[j]) return(-1);
			if (argc <= n
			&& bind.f_func == origbindlist[j].f_func
			&& bind.d_func == origbindlist[j].d_func) return(-1);
			origflaglist[j] = 1;
		}
	}
	else {
		if (j >= MAXBINDTABLE || origbindlist[j].key < 0) return(-1);
		if (!origflaglist || origflaglist[j]) return(-1);
		origflaglist[j] = 1;
		printmacro(origbindlist, j, 0, fp);
		return(1);
	}

	flaglist[i] = 1;
	printmacro(bindlist, i, 1, fp);
	return(1);
}
# endif	/* !_NOORIGSHELL */

# ifndef	_NOKEYMAP
static VOID NEAR cleanupkeymap(VOID_A)
{
	copykeyseq(origkeymaplist);
}

static int NEAR dispkeymap(no)
int no;
{
	keyseq_t key;
	char *cp;
	int len;

	key.code = keyseqlist[no];
	if (getkeyseq(&key) < 0 || !(key.len)) {
		cprintf2("%*s", MAXCUSTVAL, "");
		return(0);
	}
	cp = encodestr(key.str, key.len);
	cprintf2("%-*.*k", MAXCUSTVAL, MAXCUSTVAL, cp);
	len = strlen2(cp);
	if (len > MAXCUSTVAL - 1) len = MAXCUSTVAL - 1;
	free(cp);
	return(len);
}

static int NEAR editkeymap(no)
int no;
{
	char *cp, *str, *buf;
	ALLOC_T size;
	int i, len, key, dupwin_x, dupwin_y;

	key = keyseqlist[no];
	cp = getkeysym(key, 1);

	dupwin_x = win_x;
	dupwin_y = win_y;
	locate(0, L_INFO);
	putterm(l_clear);
	buf = asprintf3(KYMPK_K, cp);
	win_x = kanjiputs(buf);
	win_y = L_INFO;
	locate(win_x, win_y);
	tflush();
	keyflush();
	free(buf);

	while (!kbhit2(1000000L / SENSEPERSEC));
	buf = c_realloc(NULL, 0, &size);
	len = 0;
	for (;;) {
		buf = c_realloc(buf, len, &size);
		if ((i = getch2()) == EOF) break;
		buf[len++] = i;
		if (!kbhit2(WAITKEYPAD * 1000L)) break;
	}
	buf = realloc2(buf, len + 1);
	buf[len] = '\0';

	win_x = dupwin_x;
	win_y = dupwin_y;

	if (len == 1 && buf[0] == K_ESC) {
		free(buf);
		if (!yesno(DELKM_K, cp)) return(0);
		setkeyseq(key, NULL, 0);
		return(1);
	}

	str = encodestr(buf, len);
	i = yesno(SETKM_K, cp, str);
	free(str);
	if (!i) {
		free(buf);
		return(0);
	}

	setkeyseq(key, buf, len);
	return(1);
}

static int NEAR searchkeymap(list, kp)
keyseq_t *list, *kp;
{
	int i;

	for (i = 0; i < K_MAX - K_MIN + 1; i++)
		if (kp -> code == list[i].code) break;
	return(i);
}

static int NEAR issamekeymap(kp1, kp2)
keyseq_t *kp1, *kp2;
{
	if (!(kp1 -> len)) return((kp2 -> len) ? 0 : 1);
	else if (!(kp2 -> len) || kp1 -> len != kp2 -> len) return(0);
	else if (!(kp1 -> str) || !(kp2 -> str)
	|| memcmp(kp1 -> str, kp2 -> str, kp1 -> len))
		return(0);

	return(1);
}

static int NEAR dumpkeymap(flaglist, origflaglist, fp)
char *flaglist, *origflaglist;
FILE *fp;
{
	keyseq_t key;
	char *new;
	int i, j, n;

	if (origflaglist) new = NULL;
	else {
		origflaglist =
		new = malloc2((K_MAX - K_MIN + 1) * sizeof(char));
		memset(origflaglist, 0, (K_MAX - K_MIN + 1) * sizeof(char));
	}

	for (i = n = 0; keyseqlist[i] >= 0; i++) {
		if (flaglist && flaglist[i]) continue;
		key.code = keyseqlist[i];
		if (getkeyseq(&key) < 0) continue;
		j = searchkeymap(origkeymaplist, &key);
		if (j < K_MAX - K_MIN + 1) {
			origflaglist[j] = 1;
			if (issamekeymap(&key, &(origkeymaplist[j]))) continue;
		}
		n++;
	}
	for (i = 0; i < K_MAX - K_MIN + 1; i++) {
		if (origflaglist[i]) continue;
		if (!(origkeymaplist[i].len)) {
			origflaglist[i] = 1;
			continue;
		}
		key.code = origkeymaplist[i].code;
		if (getkeyseq(&key) >= 0
		&& issamekeymap(&key, &(origkeymaplist[i]))) {
			origflaglist[i] = 1;
			continue;
		}
		n++;
	}
	if (!n || !fp) {
		if (new) free(new);
		return(n);
	}

	fputnl(fp);
	fputs("# keymap definition\n", fp);
	for (i = 0; keyseqlist[i] >= 0; i++) {
		if (flaglist && flaglist[i]) continue;
		key.code = keyseqlist[i];
		if (getkeyseq(&key) < 0) continue;
		j = searchkeymap(origkeymaplist, &key);
		if (j < K_MAX - K_MIN + 1
		&& issamekeymap(&key, &(origkeymaplist[j])))
			continue;

		printkeymap(&key, 1, fp);
	}
	for (i = 0; i < K_MAX - K_MIN + 1; i++) {
		if (origflaglist[i]) continue;

		printkeymap(&(origkeymaplist[i]), 1, fp);
	}
	if (new) free(new);
	return(n);
}

#  ifndef	_NOORIGSHELL
static int NEAR checkkeymap(flaglist, origflaglist, argc, argv, fp)
char *flaglist, *origflaglist;
int argc;
char *argv[];
FILE *fp;
{
	keyseq_t key;
	int i, j;

	if (strcommcmp(argv[0], BL_KEYMAP)) return(0);
	if (parsekeymap(argc, argv, &key) < 0) return(-1);
	if (key.code < 0 || !(key.len)) return(0);

	for (i = 0; keyseqlist[i] >= 0; i++)
		if (key.code == keyseqlist[i]) break;
	j = searchkeymap(origkeymaplist, &key);
	if (keyseqlist[i] >= 0) {
		if (!flaglist || flaglist[i]) {
			if (key.str) free(key.str);
			return(-1);
		}
		if (j < K_MAX - K_MIN + 1) {
			if (!origflaglist || origflaglist[j]
			|| issamekeymap(&key, &(origkeymaplist[j]))) {
				if (key.str) free(key.str);
				return(-1);
			}
			origflaglist[j] = 1;
		}
	}
	else {
		if (key.str) free(key.str);
		if (j >= K_MAX - K_MIN + 1) return(-1);
		if (!origflaglist || origflaglist[j]) return(-1);
		origflaglist[j] = 1;
		printkeymap(&(origkeymaplist[j]), 0, fp);
		return(1);
	}

	if (key.str) free(key.str);
	if (getkeyseq(&key) < 0) return(-1);

	flaglist[i] = 1;
	printkeymap(&key, 1, fp);
	return(1);
}
#  endif	/* !_NOORIGSHELL */
# endif	/* !_NOKEYMAP */

# ifndef	_NOARCHIVE
VOID freelaunchlist(list, max)
launchtable *list;
int max;
{
	int i;

	for (i = 0; i < max; i++) freelaunch(&(list[i]));
}

launchtable *copylaunch(dest, src, ndestp, nsrc)
launchtable *dest, *src;
int *ndestp, nsrc;
{
	int i;

	if (dest) freelaunchlist(dest, *ndestp);
	else if (nsrc > 0)
		dest = (launchtable *)malloc2(nsrc * sizeof(launchtable));
	for (i = 0; i < nsrc; i++) {
		dest[i].ext = strdup2(src[i].ext);
		dest[i].comm = strdup2(src[i].comm);
		dest[i].format = duplvar(src[i].format, -1);
		dest[i].lignore = duplvar(src[i].lignore, -1);
		dest[i].lerror = duplvar(src[i].lerror, -1);
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
	int len, width;

	if (no >= maxlaunch) {
		cprintf2("%*s", MAXCUSTVAL, "");
		return(0);
	}
	if (!launchlist[no].format) {
		width = MAXCUSTVAL;
		cprintf2("%-*.*k", width, width, launchlist[no].comm);
	}
	else {
		width = (MAXCUSTVAL - 1 - 6) / 2;
		cprintf2("%-*.*k", width, width, launchlist[no].comm);
		putsep();
		width = MAXCUSTVAL - 1 - 6 - width;
		cprintf2("%-*.*k", width, width, launchlist[no].format[0]);
		putsep();
		cprintf2("%-2d", (int)(launchlist[no].topskip));
		putsep();
		cprintf2("%-2d", (int)(launchlist[no].bottomskip));
	}
	len = strlen2(launchlist[no].comm);
	if (len > --width) len = width;
	return(len);
}

static VOID NEAR verboselaunch(list)
launchtable *list;
{
	int y, len, nf, ni, ne;

	custtitle();
	y = LFILETOP + 1;
	fillline(y++, n_column);
	len = strlen2(list -> ext + 1);
	if (list -> flags & LF_IGNORECASE) len++;
	if (len >= MAXCUSTNAM) len = MAXCUSTNAM;
	fillline(y++, MAXCUSTNAM + 1 - len);
	putterm(l_clear);
	if (list -> flags & LF_IGNORECASE) {
		putch2('/');
		len--;
	}
	cprintf2("%.*k", len, list -> ext + 1);
	putsep();
	cprintf2("%-*.*k", MAXCUSTVAL, MAXCUSTVAL, list -> comm);
	putsep();
	fillline(y++, n_column);
	nf = ni = ne = 0;
	if (list -> format) for(; y < LFILEBOTTOM - 1; y++) {
		if (nf >= 0) {
			if (!(list -> format[nf])) {
				nf = -1;
				fillline(y, n_column);
				continue;
			}
			else if (nf) fillline(y, MAXCUSTNAM + 2);
			else {
				locate(0, y);
				putsep();
				putterm(t_standout);
				cputs2("-t");
				putterm(end_standout);
				cprintf2("%3d", (int)(list -> topskip));
				putsep();
				putterm(t_standout);
				cputs2("-b");
				putterm(end_standout);
				cprintf2("%3d", (int)(list -> bottomskip));
				putsep();
				putterm(t_standout);
				cputs2("-f");
				putterm(end_standout);
				putsep();
			}
			cprintf2("%-*.*k",
				MAXCUSTVAL, MAXCUSTVAL, list -> format[nf]);
			putsep();
			nf++;
		}
		else if (ni >= 0 && list -> lignore) {
			if (!(list -> lignore[ni])) {
				ni = -1;
				fillline(y, n_column);
				continue;
			}
			else if (ni) fillline(y, MAXCUSTNAM + 2);
			else {
				fillline(y, MAXCUSTNAM - 2 + 1);
				putterm(t_standout);
				cputs2("-i");
				putterm(end_standout);
				putsep();
			}
			cprintf2("%-*.*k",
				MAXCUSTVAL, MAXCUSTVAL, list -> lignore[ni]);
			putsep();
			ni++;
		}
		else if (ne >= 0 && list -> lerror) {
			if (!(list -> lerror[ne])) {
				ne = -1;
				fillline(y, n_column);
				continue;
			}
			else if (ne) fillline(y, MAXCUSTNAM + 2);
			else {
				fillline(y, MAXCUSTNAM - 2 + 1);
				putterm(t_standout);
				cputs2("-e");
				putterm(end_standout);
				putsep();
			}
			cprintf2("%-*.*k",
				MAXCUSTVAL, MAXCUSTVAL, list -> lerror[ne]);
			putsep();
			ne++;
		}
		else break;
	}
	for (; y < LFILEBOTTOM; y++) fillline(y, n_column);
}

static char **NEAR editvar(prompt, var)
char *prompt, **var;
{
	namelist *list;
	char *cp, *tmp, *usg, *str[4], **mes;
	int i, n, max, val[4];

	max = countvar(var);
	for (i = 0; i < sizeof(val) / sizeof(int); i++) val[i] = i;

	list = (namelist *)malloc2((max + 1) * sizeof(namelist));
	mes = (char **)malloc2((max + 1) * sizeof(char *));
	usg = ARUSG_K;
	for (i = 0; i < max; i++) {
		setnamelist(i, list, var[i]);
		mes[i] = usg;
	}
	setnamelist(i, list, ARADD_K);
	mes[i] = NULL;

	cp = NULL;
	for (;;) {
		n = browsenamelist(list, max + 1, 1, cp, prompt, mes);
		if (n < 0) break;
		cp = list[n].name;
		if (n >= max) {
			if (!(tmp = inputcuststr(prompt, 0, NULL, -1)))
				continue;
			list = (namelist *)realloc2(list,
				(max + 2) * sizeof(namelist));
			mes = (char **)realloc2(mes,
				(max + 2) * sizeof(char *));
			setnamelist(max, list, tmp);
			mes[max++] = usg;
			setnamelist(max, list, cp);
			mes[max] = NULL;
			cp = tmp;
			continue;
		}

		str[0] = AREDT_K;
		str[1] = (n > 0) ? ARUPW_K : NULL;
		str[2] = (n < max - 1) ? ARDWN_K : NULL;
		str[3] = ARDEL_K;
		i = 0;
		if (noselect(&i, sizeof(val) / sizeof(int), 0, str, val))
			continue;
		if (i == 0) {
			if (!(tmp = inputcuststr(prompt, 0, cp, -1)))
				continue;
			free(cp);
			cp = list[n].name = tmp;
		}
		else if (i == 1) {
			list[n].name = list[n - 1].name;
			list[n - 1].name = cp;
		}
		else if (i == 2) {
			list[n].name = list[n + 1].name;
			list[n + 1].name = cp;
		}
		else if (i == 3) {
			if (!yesno(ADLOK_K)) continue;
			free(cp);
			for (i = n; i < max; i++)
				list[i].name = list[i + 1].name;
			mes[--max] = NULL;
			cp = list[n].name;
		}
	}

	if (var) free(var);
	if (!max) var = NULL;
	else {
		var = (char **)malloc2((max + 1) * sizeof(char *));
		for (i = 0; i < max; i++) var[i] = list[i].name;
		var[i] = NULL;
	}
	free(list);
	free(mes);

	return(var);
}

static int NEAR editarchbrowser(list)
launchtable *list;
{
	char *cp, *str[6], buf[MAXLONGWIDTH + 1];
	int i, n, val[6];
	u_char *skipp;

	str[0] = "-f";
	str[1] = "-t";
	str[2] = "-b";
	str[3] = "-i";
	str[4] = "-e";
	str[5] = ARDON_K;
	for (i = 0; i < sizeof(val) / sizeof(int); i++) val[i] = i;

	n = 0;
	for (;;) {
		verboselaunch(list);
		envcaption(ARSEL_K);
		if (noselect(&n, sizeof(val) / sizeof(int), 0, str, val)) {
			if (yesno(ARCAN_K)) return(0);
			continue;
		}
		if (n == sizeof(val) / sizeof(int) - 1) {
			if (!(list -> format)) {
				if (!yesno(ARNOT_K)) continue;
				freevar(list -> lignore);
				freevar(list -> lerror);
				list -> lignore = list -> lerror = NULL;
				list -> topskip =
				list -> bottomskip = (u_char)0;
			}
			break;
		}
		if (n == 0) list -> format = editvar(ARCHF_K, list -> format);
		else if (n == 3)
			list -> lignore = editvar(ARIGN_K, list -> lignore);
		else if (n == 4)
			list -> lerror = editvar(ARERR_K, list -> lerror);
		else {
			if (n == 1) {
				skipp = &(list -> topskip);
				cp = TOPSK_K;
			}
			else {
				skipp = &(list -> bottomskip);
				cp = BTMSK_K;
			}
			cp = int2str(buf, *skipp);
			if (!(cp = inputcuststr(cp, 1, buf, -1))) continue;
			i = atoi2(cp);
			free(cp);
			if (i < 0) {
				warning(0, VALNG_K);
				continue;
			}
			*skipp = i;
		}
	}
	return(1);
}

static int NEAR editlaunch(no)
int no;
{
	launchtable list;
	char *cp, *ext;
	int i;

	if (no < maxlaunch) {
		ext = NULL;
		list.ext = launchlist[no].ext;
		list.flags = launchlist[no].flags;
		verboselaunch(&(launchlist[no]));
	}
	else {
		if (no >= MAXLAUNCHTABLE) {
			warning(0, OVERF_K);
			return(0);
		}

		if (!(cp = inputcuststr(EXTLN_K, 1, NULL, -1))) return(0);
		list.ext = ext = getext(cp, &(list.flags));
		free(cp);
		if (!ext[0] || !ext[1]) {
			free(ext);
			return(0);
		}

		for (i = 0; i < maxlaunch; i++)
			if (!extcmp(list.ext, list.flags,
			launchlist[i].ext, launchlist[i].flags, 1)) break;
		if (i >= maxlaunch) {
			launchlist[i].comm = NULL;
			launchlist[i].format =
			launchlist[i].lignore = launchlist[i].lerror = NULL;
			launchlist[i].topskip =
			launchlist[i].bottomskip = (u_char)0;
			launchlist[i].flags = list.flags;
		}
		no = i;
	}

	for (;;) {
		list.format = duplvar(launchlist[no].format, -1);
		list.lignore = duplvar(launchlist[no].lignore, -1);
		list.lerror = duplvar(launchlist[no].lerror, -1);
		list.topskip = launchlist[no].topskip;
		list.bottomskip = launchlist[no].bottomskip;

		if (!(list.comm =
		inputcuststr(LNCHC_K, 0, launchlist[no].comm, 0))) {
			if (ext) free(ext);
			freevar(list.format);
			freevar(list.lignore);
			freevar(list.lerror);
			return(-1);
		}
		else if (!*(list.comm)) {
			free(list.comm);
			freevar(list.format);
			freevar(list.lignore);
			freevar(list.lerror);
			if (ext) {
				free(ext);
				return(-1);
			}

			if (!yesno(DLLOK_K, launchlist[no].ext)) return(-1);
			freelaunch(&(launchlist[no]));
			maxlaunch--;
			for (i = no; i < maxlaunch; i++)
				memcpy((char *)&(launchlist[i]),
				(char *)&(launchlist[i + 1]),
					sizeof(launchtable));
			return(1);
		}
		else if (!yesno(ISARC_K)) {
			freevar(list.format);
			freevar(list.lignore);
			freevar(list.lerror);
			list.format = list.lignore = list.lerror = NULL;
			list.topskip = list.bottomskip = (u_char)0;
			break;
		}

		if (editarchbrowser(&list)) break;
		free(list.comm);
		freevar(list.format);
		freevar(list.lignore);
		freevar(list.lerror);
	}

	if (no >= maxlaunch) maxlaunch++;
	else {
		if (ext) free(launchlist[no].ext);
		free(launchlist[no].comm);
		freevar(launchlist[no].format);
		freevar(launchlist[no].lignore);
		freevar(launchlist[no].lerror);
	}
	memcpy((char *)&(launchlist[no]), (char *)&list, sizeof(list));
	return(1);
}

static int NEAR issamelaunch(lp1, lp2)
launchtable *lp1, *lp2;
{
	int i;

	if (!(lp1 -> format)) {
		if (lp2 -> format) return(0);
	}
	else if (!(lp2 -> format)) return(0);
	else if (lp1 -> topskip != lp2 -> topskip
	|| lp1 -> bottomskip != lp2 -> bottomskip)
		return(0);

	if (strcommcmp(lp1 -> comm, lp2 -> comm)) return(0);
	if (!(lp1 -> format)) return(1);

	for (i = 0; lp1 -> format[i]; i++)
		if (strcmp(lp1 -> format[i], lp2 -> format[i])) break;
	if (lp1 -> format[i]) return(0);

	if (!(lp1 -> lignore)) {
		if (lp2 -> lignore) return(0);
	}
	else if (!(lp2 -> lignore)) return(0);
	else {
		for (i = 0; lp1 -> lignore[i]; i++)
			if (strcmp(lp1 -> lignore[i], lp2 -> lignore[i]))
				break;
		if (lp1 -> lignore[i]) return(0);
	}

	if (!(lp1 -> lerror)) {
		if (lp2 -> lerror) return(0);
	}
	else if (!(lp2 -> lerror)) return(0);
	else {
		for (i = 0; lp1 -> lerror[i]; i++)
			if (strcmp(lp1 -> lerror[i], lp2 -> lerror[i]))
				break;
		if (lp1 -> lerror[i]) return(0);
	}

	return(1);
}

static int NEAR dumplaunch(flaglist, origflaglist, fp)
char *flaglist, *origflaglist;
FILE *fp;
{
	char *new;
	int i, j, n;

	if (origflaglist) new = NULL;
	else {
		origflaglist = new = malloc2(origmaxlaunch * sizeof(char));
		memset(origflaglist, 0, origmaxlaunch * sizeof(char));
	}

	for (i = n = 0; i < maxlaunch; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searchlaunch(origlaunchlist, origmaxlaunch,
			&(launchlist[i]));
		if (j < origmaxlaunch) {
			origflaglist[j] = 1;
			if (issamelaunch(&(launchlist[i]),
			&(origlaunchlist[j])))
				continue;
		}
		n++;
	}
	for (i = 0; i < origmaxlaunch; i++) {
		if (origflaglist[i]) continue;
		n++;
	}
	if (!n || !fp) {
		if (new) free(new);
		return(n);
	}

	fputnl(fp);
	fputs("# launcher definition\n", fp);
	for (i = 0; i < maxlaunch; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searchlaunch(origlaunchlist, origmaxlaunch,
			&(launchlist[i]));
		if (j < origmaxlaunch
		&& issamelaunch(&(launchlist[i]), &(origlaunchlist[j])))
			continue;

		printlaunchcomm(launchlist, i, 1, 0, fp);
	}
	for (i = 0; i < origmaxlaunch; i++) {
		if (origflaglist[i]) continue;

		printlaunchcomm(origlaunchlist, i, 0, 0, fp);
	}
	if (new) free(new);
	return(n);
}

#  ifndef	_NOORIGSHELL
static int NEAR checklaunch(flaglist, origflaglist, argc, argv, fp)
char *flaglist, *origflaglist;
int argc;
char *argv[];
FILE *fp;
{
	launchtable launch;
	int i, j;

	if (strcommcmp(argv[0], BL_LAUNCH)) return(0);
	if (parselaunch(argc, argv, &launch) < 0) return(-1);

	i = searchlaunch(launchlist, maxlaunch, &launch);
	j = searchlaunch(origlaunchlist, origmaxlaunch, &launch);
	if (i < maxlaunch) {
		if (!flaglist || flaglist[i]) {
			freelaunch(&launch);
			return(-1);
		}
		if (j < origmaxlaunch) {
			if (!origflaglist || origflaglist[j]
			|| !issamelaunch(&launch, &(origlaunchlist[j]))) {
				freelaunch(&launch);
				return(-1);
			}
			origflaglist[j] = 1;
		}
	}
	else {
		freelaunch(&launch);
		if (j >= origmaxlaunch) return(-1);
		if (!origflaglist || origflaglist[j]) return(-1);
		origflaglist[j] = 1;
		printlaunchcomm(origlaunchlist, j, 0, 0, fp);
		return(1);
	}

	freelaunch(&launch);
	flaglist[i] = 1;
	printlaunchcomm(launchlist, i, 1, 0, fp);
	return(1);
}
#  endif	/* !_NOORIGSHELL */

VOID freearchlist(list, max)
archivetable *list;
int max;
{
	int i;

	for (i = 0; i < max; i++) freearch(&(list[i]));
}

archivetable *copyarch(dest, src, ndestp, nsrc)
archivetable *dest, *src;
int *ndestp, nsrc;
{
	int i;

	if (dest) freearchlist(dest, *ndestp);
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
	int len, width;

	if (no >= maxarchive) {
		cprintf2("%*s", MAXCUSTVAL, "");
		return(0);
	}
	width = (MAXCUSTVAL - 1) / 2;
	if (!archivelist[no].p_comm) cprintf2("%*s", width, "");
	else cprintf2("%-*.*k", width, width, archivelist[no].p_comm);
	putsep();
	if (!archivelist[no].u_comm)
		cprintf2("%*s", MAXCUSTVAL - 1 - width, "");
	else cprintf2("%-*.*k",
		MAXCUSTVAL - 1 - width, MAXCUSTVAL - 1 - width,
		archivelist[no].u_comm);
	len = (archivelist[no].p_comm) ? strlen2(archivelist[no].p_comm) : 0;
	if (len > --width) len = width;
	return(len);
}

static int NEAR editarch(no)
int no;
{
	archivetable list;
	char *cp, *ext;
	int i;

	if (no < maxarchive) {
		ext = NULL;
		list.ext = archivelist[no].ext;
		list.flags = archivelist[no].flags;
	}
	else {
		if (no >= MAXARCHIVETABLE) {
			warning(0, OVERF_K);
			return(0);
		}

		if (!(cp = inputcuststr(EXTAR_K, 1, NULL, -1))) return(0);
		list.ext = ext = getext(cp, &(list.flags));
		free(cp);
		if (!ext[0] || !ext[1]) {
			free(ext);
			return(0);
		}

		for (i = 0; i < maxarchive; i++)
			if (!extcmp(list.ext, list.flags,
			archivelist[i].ext, archivelist[i].flags, 1)) break;
		if (i >= maxarchive) {
			archivelist[i].p_comm = archivelist[i].u_comm = NULL;
			archivelist[i].flags = list.flags;
		}
		no = i;
	}

	for (;;) {
		if (!(list.p_comm =
		inputcuststr(PACKC_K, 0, archivelist[no].p_comm, 0))) {
			if (ext) free(ext);
			return(0);
		}
		else if (!*(list.p_comm)) {
			free(list.p_comm);
			list.p_comm = NULL;
		}

		if (!(list.u_comm =
		inputcuststr(UPCKC_K, 0, archivelist[no].u_comm, 0))) {
			if (list.p_comm) free(list.p_comm);
			continue;
		}
		if (!*(list.u_comm)) {
			free(list.u_comm);
			list.u_comm = NULL;
		}
		break;
	}

	if (!(list.p_comm) && !(list.u_comm)) {
		if (ext) {
			free(ext);
			return(0);
		}

		if (!yesno(DLAOK_K, archivelist[no].ext)) return(0);
		freearch(&(archivelist[no]));
		maxarchive--;
		for (i = no; i < maxarchive; i++)
			memcpy((char *)&(archivelist[i]),
			(char *)&(archivelist[i + 1]),
				sizeof(archivetable));
		return(1);
	}

	if (no >= maxarchive) maxarchive++;
	else {
		if (ext) free(archivelist[no].ext);
		if (archivelist[no].p_comm) free(archivelist[no].p_comm);
		if (archivelist[no].u_comm) free(archivelist[no].u_comm);
	}
	memcpy((char *)&(archivelist[no]), (char *)&list, sizeof(list));
	return(1);
}

static int NEAR issamearch(ap1, ap2)
archivetable *ap1, *ap2;
{
	if (!(ap1 -> p_comm)) {
		if (ap2 -> p_comm) return(0);
	}
	else if (!(ap2 -> p_comm)) return(0);
	else if (strcommcmp(ap1 -> p_comm, ap2 -> p_comm)) return(0);
	if (strcommcmp(ap1 -> u_comm, ap2 -> u_comm)) return(0);

	return(1);
}

static int NEAR dumparch(flaglist, origflaglist, fp)
char *flaglist, *origflaglist;
FILE *fp;
{
	char *new;
	int i, j, n;

	if (origflaglist) new = NULL;
	else {
		origflaglist = new = malloc2(origmaxarchive * sizeof(char));
		memset(origflaglist, 0, origmaxarchive * sizeof(char));
	}

	for (i = n = 0; i < maxarchive; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searcharch(origarchivelist, origmaxarchive,
			&(archivelist[i]));
		if (j < origmaxarchive) {
			origflaglist[j] = 1;
			if (issamearch(&(archivelist[i]),
			&(origarchivelist[j])))
				continue;
		}
		n++;
	}
	for (i = 0; i < origmaxarchive; i++) {
		if (origflaglist[i]) continue;
		n++;
	}
	if (!n || !fp) {
		if (new) free(new);
		return(n);
	}

	fputnl(fp);
	fputs("# archiver definition\n", fp);
	for (i = 0; i < maxarchive; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searcharch(origarchivelist, origmaxarchive,
			&(archivelist[i]));
		if (j < origmaxarchive
		&& issamearch(&(archivelist[i]), &(origarchivelist[j])))
			continue;

		printarchcomm(archivelist, i, 1, fp);
	}
	for (i = 0; i < origmaxlaunch; i++) {
		if (origflaglist[i]) continue;

		printarchcomm(origarchivelist, i, 0, fp);
	}
	if (new) free(new);
	return(n);
}

#  ifndef	_NOORIGSHELL
static int NEAR checkarch(flaglist, origflaglist, argc, argv, fp)
char *flaglist, *origflaglist;
int argc;
char *argv[];
FILE *fp;
{
	archivetable arch;
	int i, j;

	if (strcommcmp(argv[0], BL_ARCH)) return(0);
	if (parsearch(argc, argv, &arch) < 0) return(-1);

	i = searcharch(archivelist, maxarchive, &arch);
	j = searcharch(origarchivelist, origmaxarchive, &arch);
	if (i < maxarchive) {
		if (!flaglist || flaglist[i]) {
			freearch(&arch);
			return(-1);
		}
		if (j < origmaxarchive) {
			if (!origflaglist || origflaglist[j]
			|| !issamearch(&arch, &(origarchivelist[j]))) {
				freearch(&arch);
				return(-1);
			}
			origflaglist[j] = 1;
		}
	}
	else {
		freearch(&arch);
		if (j >= origmaxarchive) return(-1);
		if (!origflaglist || origflaglist[j]) return(-1);
		origflaglist[j] = 1;
		printarchcomm(origarchivelist, j, 0, fp);
		return(1);
	}

	freearch(&arch);
	flaglist[i] = 1;
	printarchcomm(archivelist, i, 1, fp);
	return(1);
}
#  endif	/* _NOORIGSHELL */
# endif	/* !_NOARCHIVE */

# ifdef	_USEDOSEMU
VOID freedosdrive(list)
devinfo *list;
{
	int i;

	for (i = 0; list[i].name; i++) free(list[i].name);
}

devinfo *copydosdrive(dest, src)
devinfo *dest, *src;
{
	int i;

	if (dest) freedosdrive(dest);
	if (!src) {
		if (dest) dest[0].name = NULL;
	}
	else {
		for (i = 0; src[i].name; i++);
		if (dest);
		else dest = (devinfo *)malloc2((i + 1) * sizeof(devinfo));
		for (i = 0; src[i].name; i++) {
			memcpy((char *)&(dest[i]), (char *)&(src[i]),
				sizeof(devinfo));
			dest[i].name = strdup2(src[i].name);
		}
		dest[i].name = NULL;
	}
	return(dest);
}

static VOID NEAR cleanupdosdrive(VOID_A)
{
	copydosdrive(fdtype, origfdtype);
}

static int NEAR dispdosdrive(no)
int no;
{
	int i, len, w1, w2, width;

	if (!fdtype[no].name) {
		cprintf2("%*s", MAXCUSTVAL, "");
		return(0);
	}
	width = (MAXCUSTVAL - 1) / 2;
	cprintf2("%-*.*k", width, width, fdtype[no].name);
	putsep();

	w1 = MAXCUSTVAL - 1 - width;
#  ifdef	HDDMOUNT
	if (!fdtype[no].cyl) {
		char buf[sizeof("HDD98 #offset=") + MAXCOLSCOMMA(3)];

		strcpy(buf, "HDD");
		if (isupper2(fdtype[no].head)) strcat(buf, "98");
		i = strlen(buf);
		snprintf2(&(buf[i]), sizeof(buf) - i, " #offset=%'Ld",
			fdtype[no].offset / fdtype[no].sect);
		cprintf2("%-*.*k", w1, w1, buf);
	}
	else
#  endif	/* HDDMOUNT */
	{
		len = 0;
		for (i = 0; i < MEDIADESCRSIZ; i++) {
			w2 = strlen2(mediadescr[i].name);
			if (len < w2) len = w2;
		}

		w2 = (w1 - len) / 3 - 1;
		if (w2 <= 0) w2 = 1;
		cprintf2("%-*d", w2, (int)(fdtype[no].head));
		putsep();
		cprintf2("%-*d", w2, (int)(fdtype[no].sect));
		putsep();
		cprintf2("%-*d", w2, (int)(fdtype[no].cyl));
		putsep();

		for (i = 0; i < MEDIADESCRSIZ; i++) {
			if (fdtype[no].head == mediadescr[i].head
			&& fdtype[no].sect == mediadescr[i].sect
			&& fdtype[no].cyl == mediadescr[i].cyl) break;
		}
		w1 -= (w2 + 1) * 3;
		if (i >= MEDIADESCRSIZ) cprintf2("%*s", w1, "");
		else cprintf2("%-*.*k", w1, w1, mediadescr[i].name);
	}
	len = strlen2(fdtype[no].name);
	if (len > --width) len = width;
	return(len);
}

static int NEAR editdosdrive(no)
int no;
{
	devinfo dev;
	char *cp, buf[MAXLONGWIDTH + 1];
	char *str['Z' - 'A' + 1], sbuf['Z' - 'A' + 1][3];
	int i, n, val['Z' - 'A' + 1];

	if (fdtype[no].name) dev.drive = fdtype[no].drive;
	else {
		if (no >= MAXDRIVEENTRY - 1) {
			warning(0, OVERF_K);
			return(0);
		}

		for (dev.drive = n = 0; dev.drive <= 'Z' - 'A'; dev.drive++) {
#  ifdef	HDDMOUNT
			for (i = 0; fdtype[i].name; i++)
				if (dev.drive == fdtype[i].drive
				&& !(fdtype[i].cyl))
					break;
			if (fdtype[i].name) continue;
#  endif
			sbuf[n][0] = dev.drive + 'A';
			sbuf[n][1] = ':';
			sbuf[n][2] = '\0';
			str[n] = sbuf[n];
			val[n++] = dev.drive + 'A';
		}

		envcaption(DRNAM_K);
		i = val[0];
		if (noselect(&i, n, 0, str, val)) return(0);
		dev.drive = i;

		fdtype[no].name = NULL;
	}

	dev.name = NULL;
#  ifdef	FAKEUNINIT
	dev.head = dev.sect = dev.cyl = 0;	/* fake for -Wuninitialized */
#  endif
	for (;;) {
		if (!(dev.name)) {
			cp = asprintf3(DRDEV_K, dev.drive);
			dev.name = inputcuststr(cp, 1, fdtype[no].name, 1);
			free(cp);
			if (!(dev.name)) return(0);
			else if (!*(dev.name)) {
				if (!(fdtype[no].name)) return(0);

				if (!yesno(DLDOK_K, fdtype[no].drive))
					return(0);
				deletedrv(no);
				return(1);
			}
		}

		envcaption(DRMED_K);
		for (i = n = 0; i < MEDIADESCRSIZ; i++) {
#  ifdef	HDDMOUNT
			int j;

			if (!mediadescr[i].cyl) {
				if (fdtype[no].name) continue;
				for (j = 0; fdtype[j].name; j++)
					if (dev.drive == fdtype[j].drive)
						break;
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
			free(dev.name);
			dev.name = NULL;
			continue;
		}

		if (i < MEDIADESCRSIZ) {
			dev.head = mediadescr[i].head;
			dev.sect = mediadescr[i].sect;
			dev.cyl = mediadescr[i].cyl;
			break;
		}

		for (;;) {
			if (!(fdtype[no].name)) cp = NULL;
			else cp = int2str(buf, fdtype[no].head);

			if (!(cp = inputcuststr(DRHED_K, 1, cp, -1))) break;
			dev.head = n = atoi2(cp);
			free(cp);
			if (n > 0 && n <= MAXUTYPE(u_char)) break;
			warning(0, VALNG_K);
		}
		if (!cp) continue;

		for (;;) {
			if (!(fdtype[no].name)) cp = NULL;
			else cp = int2str(buf, fdtype[no].sect);

			if (!(cp = inputcuststr(DRSCT_K, 1, cp, -1))) break;
			dev.sect = n = atoi2(cp);
			free(cp);
			if (dev.sect > 0) break;
			if (n > 0 && n <= MAXUTYPE(u_short)) break;
			warning(0, VALNG_K);
		}
		if (!cp) continue;

		for (;;) {
			if (!(fdtype[no].name)) cp = NULL;
			else cp = int2str(buf, fdtype[no].cyl);

			if (!(cp = inputcuststr(DRCYL_K, 1, cp, -1))) break;
			dev.cyl = n = atoi2(cp);
			free(cp);
			if (n > 0 && n <= MAXUTYPE(u_short)) break;
			warning(0, VALNG_K);
		}
		if (cp) break;
	}

	n = searchdrv(fdtype, &dev, 1);
	if (fdtype[n].name) {
		if (n != no) warning(0, DUPLD_K);
		return(0);
	}

	if (fdtype[no].name) no = deletedrv(no);

	n = insertdrv(no, &dev);
	free(dev.name);
	if (n < 0) {
		if (n < -1) warning(0, OVERF_K);
		else warning(ENODEV, dev.name);
		return(0);
	}
	return(1);
}

static int NEAR dumpdosdrive(flaglist, origflaglist, fp)
char *flaglist, *origflaglist;
FILE *fp;
{
	char *new;
	int i, j, n;

	if (origflaglist) new = NULL;
	else {
		for (n = 0; origfdtype[n].name; n++);
		origflaglist = new = malloc2(n * sizeof(char));
		memset(origflaglist, 0, n * sizeof(char));
	}

	for (i = n = 0; fdtype[i].name; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searchdrv(origfdtype, &(fdtype[i]), 1);
		if (origfdtype[j].name) {
			origflaglist[j] = 1;
			continue;
		}
		n++;
#  ifdef	HDDMOUNT
		if (fdtype[i].cyl) continue;
		for (; fdtype[i + 1].name; i++) {
			if (fdtype[i + 1].cyl
			|| fdtype[i + 1].drive != fdtype[i].drive + 1
			|| strpathcmp(fdtype[i + 1].name, fdtype[i].name))
				break;
		}
#  endif	/* HDDMOUNT */
	}
	for (i = 0; origfdtype[i].name; i++) {
		if (origflaglist[i]) continue;
		n++;
	}

	if (!n || !fp) {
		if (new) free(new);
		return(n);
	}

	fputnl(fp);
	fputs("# MS-DOS drive definition\n", fp);
	for (i = 0; fdtype[i].name; i++) {
		if (flaglist && flaglist[i]) continue;
		j = searchdrv(origfdtype, &(fdtype[i]), 1);
		if (origfdtype[j].name) continue;
		printsetdrv(fdtype, i, 1, 0, fp);
#  ifdef	HDDMOUNT
		if (fdtype[i].cyl) continue;
		for (; fdtype[i + 1].name; i++) {
			if (fdtype[i + 1].cyl
			|| fdtype[i + 1].drive != fdtype[i].drive + 1
			|| strpathcmp(fdtype[i + 1].name, fdtype[i].name))
				break;
		}
#  endif	/* HDDMOUNT */
	}
	for (i = 0; origfdtype[i].name; i++) {
		if (origflaglist[i]) continue;
		printsetdrv(origfdtype, i, 0, 0, fp);
	}
	if (new) free(new);
	return(n);
}

#  ifndef	_NOORIGSHELL
static int NEAR checkdosdrive(flaglist, origflaglist, argc, argv, fp)
char *flaglist, *origflaglist;
int argc;
char *argv[];
FILE *fp;
{
	devinfo dev;
	int i, j;

	if (strcommcmp(argv[0], BL_SDRIVE) && strcommcmp(argv[0], BL_UDRIVE))
		return(0);
	if (parsesetdrv(argc, argv, &dev) < 0) return(-1);

	i = searchdrv(fdtype, &dev, 1);
	j = searchdrv(origfdtype, &dev, 1);
	if (fdtype[i].name) {
		if (!flaglist || flaglist[i]) return(-1);
		if (origfdtype[j].name) return(-1);
	}
	else {
		if (!(origfdtype[j].name)) return(-1);
		if (!origflaglist || origflaglist[j]) return(-1);
		origflaglist[j] = 1;
		printsetdrv(origfdtype, j, 0, 0, fp);
		return(1);
	}

#   ifdef	HDDMOUNT
	if (!(dev.cyl)) {
		for (j = i + 1; fdtype[j].name; j++) {
			if (!flaglist || flaglist[j]) break;
			if (fdtype[j].cyl
			|| fdtype[j].drive != fdtype[j - 1].drive + 1
			|| strpathcmp(fdtype[j].name, dev.name)) break;
			flaglist[j] = 1;
		}
		for (; i > 0; i--) {
			if (!flaglist || flaglist[i - 1]) break;
			if (fdtype[i - 1].cyl
			|| fdtype[i - 1].drive + 1 != fdtype[i].drive
			|| strpathcmp(fdtype[i - 1].name, dev.name)) break;
			flaglist[i] = 1;
		}
	}
#   endif

	flaglist[i] = 1;
	printsetdrv(fdtype, i, 1, 0, fp);

	return(1);
}
#  endif	/* _NOORIGSHELL */
# endif	/* _USEDOSEMU */

static int NEAR dispsave(no)
int no;
{
	char *mes[MAXSAVEMENU];
	int len;

	mes[0] = CCNCL_K;
	mes[1] = CCLEA_K;
	mes[2] = CLOAD_K;
	mes[3] = CSAVE_K;
# ifdef	_NOORIGSHELL
	mes[4] = NIMPL_K;
# else
	mes[4] = COVWR_K;
# endif
	cprintf2("%-*.*k", MAXCUSTVAL, MAXCUSTVAL, mes[no]);
	len = strlen2(mes[no]);
	if (len > MAXCUSTVAL - 1) len = MAXCUSTVAL - 1;
	return(len);
}

# ifndef	_NOORIGSHELL
static int NEAR overwriteconfig(val, file)
int *val;
char *file;
{
	syntaxtree *trp, *stree;
	FILE *fpin, *fpout;
	char *cp, *buf, **argv, **subst, path[MAXPATHLEN];
	char *flaglist[MAXCUSTOM - 1], *origflaglist[MAXCUSTOM - 1];
	int i, n, len, *slen, fd, argc, max[MAXCUSTOM], origmax[MAXCUSTOM - 1];

	if (!(fpin = Xfopen(file, "r")) && errno != ENOENT) {
		warning(-1, file);
		return(-1);
	}

	strcpy(path, file);
	cp = getbasename(path);
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
	origmax[0] = 0;
	for (i = 0; origbindlist[i].key >= 0; i++);
	origmax[1] = i;
#  ifdef	_NOKEYMAP
	origmax[2] = 0;
#  else
	origmax[2] = K_MAX - K_MIN + 1;
#  endif
#  ifdef	_NOARCHIVE
	origmax[3] = 0;
	origmax[4] = 0;
#  else
	origmax[3] = origmaxlaunch;
	origmax[4] = origmaxarchive;
#  endif
#  ifdef	_USEDOSEMU
	for (i = 0; origfdtype[i].name; i++);
	origmax[5] = i;
#  else
	origmax[5] = 0;
#  endif
	for (i = 0; i < MAXCUSTOM - 1; i++) {
		if (!max[i]) flaglist[i] = NULL;
		else {
			flaglist[i] = malloc2(max[i] * sizeof(char));
			memset(flaglist[i], 0, max[i] * sizeof(char));
		}
		if (!origmax[i]) origflaglist[i] = NULL;
		else {
			origflaglist[i] = malloc2(origmax[i] * sizeof(char));
			memset(origflaglist[i], 0, origmax[i] * sizeof(char));
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
			for (i = 0; i < argc; i++)
				stripquote(argv[i], EA_STRIPQ);

			if (subst[0]) {
				if (!argc && val[0])
					n = checkenv(flaglist[0],
						subst, slen, fpout);
			}
			else {
				if (!n && val[0])
					n = checkunset(flaglist[0],
						argc, argv, fpout);
				if (!n && val[1])
					n = checkbind(flaglist[1],
						origflaglist[1],
						argc, argv, fpout);
#  ifndef	_NOKEYMAP
				if (!n && val[2])
					n = checkkeymap(flaglist[2],
						origflaglist[2],
						argc, argv, fpout);
#  endif
#  ifndef	_NOARCHIVE
				if (!n && val[3])
					n = checklaunch(flaglist[3],
						origflaglist[3],
						argc, argv, fpout);
				if (!n && val[4])
					n = checkarch(flaglist[4],
						origflaglist[4],
						argc, argv, fpout);
#  endif
#  ifdef	_USEDOSEMU
				if (!n && val[5])
					n = checkdosdrive(flaglist[5],
						origflaglist[5],
						argc, argv, fpout);
#  endif
			}

			freevar(subst);
			free(slen);
		}

		freestree(stree);
		trp = stree;
		if (n <= 0) {
			if (n < 0) fputs("# ", fpout);
			fputs(buf, fpout);
			fputnl(fpout);
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
			n += dumpbind(flaglist[1], origflaglist[1], NULL);
#  ifndef	_NOKEYMAP
		if (!n && val[2] && flaglist[2])
			n += dumpkeymap(flaglist[2], origflaglist[2], NULL);
#  endif
#  ifndef	_NOARCHIVE
		if (!n && val[3] && flaglist[3])
			n += dumplaunch(flaglist[3], origflaglist[3], NULL);
		if (!n && val[4] && flaglist[4])
			n += dumparch(flaglist[4], origflaglist[4], NULL);
#  endif
#  ifdef	_USEDOSEMU
		if (!n && val[5] && flaglist[5])
			n += dumpdosdrive(flaglist[5], origflaglist[5], NULL);
#  endif
		if (n) {
			fputnl(fpout);
			fputs("# additional configurations by customizer\n",
				fpout);
		}
	}

	if (val[0] && flaglist[0]) dumpenv(flaglist[0], fpout);
	if (val[1] && flaglist[1])
		dumpbind(flaglist[1], origflaglist[1], fpout);
#  ifndef	_NOKEYMAP
	if (val[2] && flaglist[2])
		dumpkeymap(flaglist[2], origflaglist[2], fpout);
#  endif
#  ifndef	_NOARCHIVE
	if (val[3] && flaglist[3])
		dumplaunch(flaglist[3], origflaglist[3], fpout);
	if (val[4] && flaglist[4])
		dumparch(flaglist[4], origflaglist[4], fpout);
#  endif
#  ifdef	_USEDOSEMU
	if (val[5] && flaglist[5])
		dumpdosdrive(flaglist[5], origflaglist[5], fpout);
#  endif
	Xfclose(fpout);
	for (i = 0; i < MAXCUSTOM - 1; i++) {
		if (flaglist[i]) free(flaglist[i]);
		if (origflaglist[i]) free(origflaglist[i]);
	}

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
# ifndef	_NOKEYMAP
	str[2] = TKYMP_K;
# endif
# ifndef	_NOARCHIVE
	str[3] = TLNCH_K;
	str[4] = TARCH_K;
# endif
# ifdef	_USEDOSEMU
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
# ifndef	_NOKEYMAP
			if (val[2]) copykeyseq(tmpkeymaplist);
# endif
# ifndef	_NOARCHIVE
			if (val[3]) copylaunch(launchlist, tmplaunchlist,
					&maxlaunch, tmpmaxlaunch);
			if (val[4]) copyarch(archivelist, tmparchivelist,
					&maxarchive, tmpmaxarchive);
# endif
# ifdef	_USEDOSEMU
			if (val[5]) copydosdrive(fdtype, tmpfdtype);
# endif
			break;
		case 1:
			envcaption(SCLEA_K);
			if (noselect(NULL, MAXCUSTOM - 1, 0, str, val)) break;
			done = 1;
			if (val[0]) cleanupenv();
			if (val[1]) cleanupbind();
# ifndef	_NOKEYMAP
			if (val[2]) cleanupkeymap();
# endif
# ifndef	_NOARCHIVE
			if (val[3]) cleanuplaunch();
			if (val[4]) cleanuparch();
# endif
# ifdef	_USEDOSEMU
			if (val[5]) cleanupdosdrive();
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
			sigvecset(n);
			if (i < 0) {
				hideclock = 1;
				warning(0, HITKY_K);
			}
			break;
		case 3:
			if (!(file = inputcuststr(FSAVE_K, 1, RUNCOMFILE, 1)))
				break;
			done = 1;
			file = evalpath(file, 1);
# ifdef	FAKEUNINIT
			fp = NULL;	/* fake for -Wuninitialized */
# endif
			if (!*file
			|| (Xaccess(file, F_OK) >= 0 && yesno(FSVOK_K)))
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
			if (val[1]) dumpbind(NULL, NULL, fp);
# ifndef	_NOKEYMAP
			if (val[2]) dumpkeymap(NULL, NULL, fp);
# endif
# ifndef	_NOARCHIVE
			if (val[3]) dumplaunch(NULL, NULL, fp);
			if (val[4]) dumparch(NULL, NULL, fp);
# endif
# ifdef	_USEDOSEMU
			if (val[5]) dumpdosdrive(NULL, NULL, fp);
# endif
			Xfclose(fp);
			break;
		case 4:
# ifndef	_NOORIGSHELL
			if (!(file = inputcuststr(FOVWR_K, 1, RUNCOMFILE, 1)))
				break;
			done = 1;
			file = evalpath(file, 1);
			if (!*file
			|| (Xaccess(file, F_OK) < 0 && errno == ENOENT
			&& !yesno(FOVOK_K)))
				done = 0;
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
# if	!defined (_NOARCHIVE) || defined (_USEDOSEMU)
	char buf[MAXCUSTNAM * 2 + 1];
# endif
	char *cp, *name[MAXSAVEMENU];
	int len;

	cp = NEWET_K;
	switch (custno) {
		case 0:
			cp = &(envlist[no].env[FDESIZ]);
			break;
		case 1:
			if (bindlist[no].key >= 0)
				cp = getkeysym(bindlist[no].key, 0);
			break;
# ifndef	_NOKEYMAP
		case 2:
			cp = getkeysym(keyseqlist[no], 1);
			break;
# endif
# ifndef	_NOARCHIVE
		case 3:
			if (no < maxlaunch) {
				if (!(launchlist[no].flags & LF_IGNORECASE))
					cp = launchlist[no].ext + 1;
				else {
					strncpy2(buf, launchlist[no].ext,
						MAXCUSTNAM * 2);
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
						MAXCUSTNAM * 2);
					buf[0] = '/';
					cp = buf;
				}
			}
			break;
# endif
# ifdef	_USEDOSEMU
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

	len = strlen2(cp);
	if (len >= MAXCUSTNAM) len = MAXCUSTNAM;
	fillline(y, MAXCUSTNAM + 1 - len);
	if (isstandout) {
		putterm(t_standout);
		cprintf2("%.*k", MAXCUSTNAM, cp);
		putterm(end_standout);
	}
	else if (stable_standout) locate(MAXCUSTNAM + 1, y);
	else cprintf2("%.*k", MAXCUSTNAM, cp);
	putsep();
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
		putterm(l_clear);
		switch (custno) {
			case 0:
				cs_len[i - start] = dispenv(i);
				break;
			case 1:
				cs_len[i - start] = dispbind(i);
				break;
# ifndef	_NOKEYMAP
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
# ifdef	_USEDOSEMU
			case 5:
				cs_len[i - start] = dispdosdrive(i);
				break;
# endif
			case 6:
				cs_len[i - start] = dispsave(i);
				break;
			default:
				cs_len[i - start] =
					cprintf2("%.*k", MAXCUSTVAL, NIMPL_K);
				if (cs_len[i - start] > MAXCUSTVAL - 1)
					cs_len[i - start] = MAXCUSTVAL - 1;
				else cprintf2("%*s",
					MAXCUSTVAL - cs_len[i - start], "");
				break;
		}
		putsep();
		fillline(y++, n_column);
	}
	for (; y < LFILEBOTTOM; y++) fillline(y, n_column);
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
		if (hmes[custno]) custputs(hmes[custno]);
		else custputs(mesconv(envlist[cs_item].hmes,
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
# ifndef	_NOKEYMAP
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
# ifdef	_USEDOSEMU
		case 5:
			if ((n = editdosdrive(cs_item))) {
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
	if (n < 0) {
		rewritefile(1);
		n = 0;
	}
	return(n);
}

int customize(VOID_A)
{
# ifndef	_NOKEYMAP
	int n;
# endif
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
# ifndef	_NOKEYMAP
	tmpkeymaplist = copykeyseq(NULL);
	for (n = 0; keyidentlist[n].no > 0; n++);
	keyseqlist = (short *)malloc2((n + 20 + 1) * sizeof(short));
	n = 0;
	for (i = 1; i <= 20; i++)
		for (ch = 0; ch <= K_MAX - K_MIN; ch++)
			if (tmpkeymaplist[ch].code == K_F(i)) {
				keyseqlist[n++] = K_F(i);
				break;
			}
	for (i = 0; keyidentlist[i].no > 0; i++)
		for (ch = 0; ch <= K_MAX - K_MIN; ch++)
			if (tmpkeymaplist[ch].code == keyidentlist[i].no) {
				keyseqlist[n++] = keyidentlist[i].no;
				break;
			}
	keyseqlist[n] = -1;
# endif
# ifndef	_NOARCHIVE
	tmplaunchlist = copylaunch(NULL, launchlist,
		&tmpmaxlaunch, maxlaunch);
	tmparchivelist = copyarch(NULL, archivelist,
		&tmpmaxarchive, maxarchive);
# endif
# ifdef	_USEDOSEMU
	tmpfdtype = copydosdrive(NULL, fdtype);
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
		Xgetkey(-1, 0);
		ch = Xgetkey(1, 0);
		Xgetkey(-1, 0);

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
# ifndef	_NOKEYMAP
	freekeyseq(tmpkeymaplist);
	free(keyseqlist);
# endif
# ifndef	_NOARCHIVE
	if (tmplaunchlist) {
		freelaunchlist(tmplaunchlist, tmpmaxlaunch);
		free(tmplaunchlist);
	}
	if (tmparchivelist) {
		freearchlist(tmparchivelist, tmpmaxarchive);
		free(tmparchivelist);
	}
# endif
# ifdef	_USEDOSEMU
	if (tmpfdtype) {
		freedosdrive(tmpfdtype);
		free(tmpfdtype);
	}
# endif
	return(0);
}
#endif	/* !_NOCUSTOMIZE */
