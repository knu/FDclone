/*
 *	custom.c
 *
 *	customizer of configurations
 */

#include <fcntl.h>
#include "fd.h"
#include "func.h"
#include "funcno.h"
#include "kanji.h"

#ifndef	_NOORIGSHELL
#include "system.h"
#endif

#define	MAXCUSTOM		7
#define	MAXCUSTNAM		14
#define	MAXCUSTVAL		(n_column - MAXCUSTNAM - 3)
#define	noselect(n, m, x, s, v)	(selectstr(n, m, x, s, v) != K_CR)
#define	getmax(m, n)		do { \
					cs_max = ((!(n) & basiccustom) \
						? nbasic : (m)[n]); \
				} while (0)
#define	setmax(m, n)		do { \
					if (!(n)) (m)[n] = cs_max; \
				} while (0)
#define	MAXSAVEMENU		5
#define	DEFPALETTE		"89624351888"
#define	MAXPALETTE		11
#define	MAXCOLOR		10
#ifndef	O_TEXT
#define	O_TEXT			0
#endif

typedef struct _envtable {
	CONST char *env;
	VOID_P var;
#ifdef	FORCEDSTDC
	union {
		CONST char *str;
		int num;
	} def;
#else
	CONST char *def;
#endif
#ifndef	_NOJPNMES
	CONST char *hmes;
#endif
#ifndef	_NOENGMES
	CONST char *hmes_eng;
#endif
	u_char type;
} envtable;

#define	env_str(n)		(&(envlist[n].env[FDESIZ]))
#define	fdenv_str(n)		(envlist[n].env)
#define	env_type(n)		(envlist[n].type & T_TYPE)
#define	_B_(t)			(T_BASIC | (t))
#ifdef	FORCEDSTDC
#define	def_str(n)		(envlist[n].def.str)
#define	def_num(n)		(envlist[n].def.num)
#define	DEFVAL(d)		{(char *)(d)}
#else
#define	def_str(n)		(envlist[n].def)
#define	def_num(n)		((int)(envlist[n].def))
#define	DEFVAL(d)		(char *)(d)
#endif

#define	T_TYPE			0037
#define	T_PRIMAL		0040
#define	T_BASIC			0100
#define	T_BOOL			0
#define	T_SHORT			1
#define	T_INT			2
#define	T_NATURAL		3
#define	T_CHARP			4
#define	T_PATH			5
#define	T_PATHS			6
#define	T_SORT			7
#define	T_DISP			8
#define	T_WRFS			9
#define	T_COLUMN		10
#if	MSDOS
#define	T_DDRV			11
#else
#define	T_DDRV			T_BOOL
#endif
#define	T_COLOR			12
#define	T_COLORPAL		13
#define	T_EDIT			14
#define	T_KIN			15
#define	T_KOUT			16
#define	T_KNAM			17
#define	T_KTERM			18
#define	T_MESLANG		19
#define	T_KPATHS		20
#define	T_OCTAL			21
#define	T_KEYCODE		22
#define	T_HELP			23
#define	T_NOVAR			24

#ifndef	_NOKANJIFCONV
typedef struct _pathtable {
	VOID_P path;
	char *last;
	u_char lang;
	u_char flags;
} pathtable;

#define	P_ISARRAY		0001
#define	P_STABLE		0002
#endif	/* !_NOKANJIFCONV */

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
#ifndef	_NOEXTRACOPY
extern int progressbar;
extern int precopymenu;
#endif
#if	!MSDOS
extern int adjtty;
#endif
extern int hideclock;
extern int defcolumns;
extern int minfilename;
extern char *histfile[];
extern short histsize[];
extern short savehist[];
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
#if	FD >= 2
extern int helplayout;
#endif
#ifndef	_NOIME
extern int imekey;
extern int imebuffer;
#endif
#ifndef	_NOCOLOR
extern int ansicolor;
# if	FD >= 2
extern char *ansipalette;
# endif
#endif	/* !_NOCOLOR */
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
extern CONST char *promptstr;
#ifndef	_NOORIGSHELL
extern CONST char *promptstr2;
#endif
#ifdef	_USEUNICODE
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
extern char *utf8iconvpath;
extern char *noconvpath;
#endif	/* !_NOKANJIFCONV */
#if	FD >= 2
extern int tmpumask;
#endif
#ifndef	_NOORIGSHELL
extern int dumbshell;
#endif
#ifndef	_NOPTY
extern int ptymode;
extern char *ptyterm;
extern int ptymenukey;
#endif
#ifndef	_NOLOGGING
extern char *logfile;
extern int logsize;
# ifndef	NOSYSLOG
extern int usesyslog;
# endif
extern int loglevel;
# ifndef	NOUID
extern int rootloglevel;
# endif
#endif	/* !_NOLOGGING */
#if	FD >= 2
extern int thruargs;
#endif
extern int wheader;
extern char fullpath[];
extern char *origpath;
extern char *progpath;
#ifdef	_USEUNICODE
extern char *unitblpath;
#endif
#ifndef	_NOIME
extern char *dicttblpath;
#endif
#ifndef	_NOCUSTOMIZE
extern int curcolumns;
extern int subwindow;
extern int win_x;
extern int win_y;
# if	FD >= 2
extern int lcmdline;
# endif
extern int calc_x;
extern int calc_y;
extern CONST functable funclist[];
extern char *macrolist[];
extern int maxmacro;
extern char *helpindex[];
extern char **orighelpindex;
extern bindtable bindlist[];
extern bindtable *origbindlist;
extern CONST strtable keyidentlist[];
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

#if	FD >= 2
static int NEAR atooctal __P_((CONST char *));
#endif
static int NEAR getenvid __P_((CONST char *, int, int *));
static VOID NEAR _evalenv __P_((int));
static VOID NEAR evalenvone __P_((int));
#ifndef	_NOKANJIFCONV
static char *NEAR pathlang __P_((pathtable *, int));
static VOID NEAR pathconv __P_((pathtable *));
static VOID NEAR savepathlang __P_((VOID_A));
static VOID NEAR evalpathlang __P_((VOID_A));
#endif
static VOID NEAR evalheader __P_((VOID_A));
#ifndef	_NOCUSTOMIZE
static int NEAR custputs __P_((CONST char *));
static char *NEAR strcatalloc __P_((char *, CONST char *));
static VOID NEAR putsep __P_((VOID_A));
static VOID NEAR fillline __P_((int, int));
static char *NEAR inputcuststr __P_((CONST char *, int, CONST char *, int));
static char *NEAR inputcustenvstr __P_((CONST char *, int, CONST char *, int));
static VOID NEAR setnamelist __P_((int, namelist *, CONST char *));
static int NEAR browsenamelist __P_((namelist *, int,
		int, CONST char *, CONST char *, CONST char **));
static VOID NEAR custtitle __P_((VOID_A));
static VOID NEAR calcmax __P_((int [], int));
static VOID NEAR envcaption __P_((CONST char *));
static char **NEAR copyenv __P_((char **));
static VOID NEAR cleanupenv __P_((VOID_A));
# if	FD >= 2
static char *NEAR ascoctal __P_((int, char *));
# endif
# ifndef	_NOORIGSHELL
static VOID NEAR putargs __P_((CONST char *, int, CONST char *[], FILE *));
# endif
static char *NEAR int2str __P_((char *, int));
static int NEAR inputkeycode __P_((CONST char *));
static int NEAR dispenv __P_((int));
static int NEAR editenv __P_((int));
static int NEAR dumpenv __P_((CONST char *, FILE *));
# ifndef	_NOORIGSHELL
static int NEAR checkenv __P_((char *, char *CONST *, int *, FILE *));
static int NEAR checkunset __P_((char *, int, char *CONST *, FILE *));
# endif
static VOID NEAR cleanupbind __P_((VOID_A));
static int NEAR dispbind __P_((int));
static int NEAR selectbind __P_((int, int, CONST char *));
static int NEAR editbind __P_((int));
static int NEAR issamebind __P_((CONST bindtable *, CONST bindtable *));
static int NEAR dumpbind __P_((CONST char *, char *, FILE *));
# ifndef	_NOORIGSHELL
static int NEAR checkbind __P_((char *, char *, int, char *CONST *, FILE *));
# endif
# ifndef	_NOKEYMAP
static VOID NEAR cleanupkeymap __P_((VOID_A));
static int NEAR dispkeymap __P_((int));
static int NEAR editkeymap __P_((int));
static int NEAR searchkeymap __P_((CONST keyseq_t *, CONST keyseq_t *));
static int NEAR issamekeymap __P_((CONST keyseq_t *, CONST keyseq_t *));
static int NEAR dumpkeymap __P_((CONST char *, char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkkeymap __P_((char *, char *, int, char *CONST *, FILE *));
#  endif
# endif
# ifndef	_NOARCHIVE
static VOID NEAR cleanuplaunch __P_((VOID_A));
static int NEAR displaunch __P_((int));
static VOID NEAR verboselaunch __P_((launchtable *));
static char **NEAR editvar __P_((CONST char *, char **));
static int NEAR editarchbrowser __P_((launchtable *));
static int NEAR editlaunch __P_((int));
static int NEAR issamelaunch __P_((CONST launchtable *, CONST launchtable *));
static int NEAR dumplaunch __P_((CONST char *, char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checklaunch __P_((char *, char *, int, char *CONST *, FILE *));
#  endif
static VOID NEAR cleanuparch __P_((VOID_A));
static int NEAR disparch __P_((int));
static int NEAR editarch __P_((int));
static int NEAR issamearch __P_((CONST archivetable *, CONST archivetable *));
static int NEAR dumparch __P_((CONST char *, char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkarch __P_((char *, char *, int, char *CONST *, FILE *));
#  endif
# endif
# ifdef	_USEDOSEMU
static VOID NEAR cleanupdosdrive __P_((VOID_A));
static int NEAR dispdosdrive __P_((int));
static int NEAR editdosdrive __P_((int));
static int NEAR dumpdosdrive __P_((CONST char *, char *, FILE *));
#  ifndef	_NOORIGSHELL
static int NEAR checkdosdrive __P_((char *, char *,
		int, char *CONST *, FILE *));
#  endif
# endif
static int NEAR dispsave __P_((int));
# ifndef	_NOORIGSHELL
static int NEAR overwriteconfig __P_((int *, CONST char *));
# endif
static int NEAR selectmulti __P_((int, CONST char *CONST [], int[]));
static int NEAR editsave __P_((int));
static VOID NEAR dispname __P_((int, int, int));
static VOID NEAR dispcust __P_((VOID_A));
static VOID NEAR movecust __P_((int, int));
static int NEAR editcust __P_((VOID_A));
#endif	/* !_NOCUSTOMIZE */

#ifndef	_NOCUSTOMIZE
int custno = -1;
int basiccustom = 0;
#endif

static CONST envtable envlist[] = {
#ifndef	_NOCUSTOMIZE
	{"FD_BASICCUSTOM", &basiccustom,
		DEFVAL(BASICCUSTOM), BSCS_E, _B_(T_BOOL)},
#endif
	{"FD_SORTTYPE", &sorttype, DEFVAL(SORTTYPE), STTP_E, _B_(T_SORT)},
	{"FD_DISPLAYMODE", &displaymode,
		DEFVAL(DISPLAYMODE), DPMD_E, _B_(T_DISP)},
#ifndef	_NOTREE
	{"FD_SORTTREE", &sorttree, DEFVAL(SORTTREE), STTR_E, T_BOOL},
#endif
#ifndef	_NOWRITEFS
	{"FD_WRITEFS", &writefs, DEFVAL(WRITEFS), WRFS_E, T_WRFS},
#endif
#if	!defined (PATHNOCASE) && (FD >= 2)
	{"FD_IGNORECASE", &pathignorecase, DEFVAL(IGNORECASE), IGNC_E, T_BOOL},
#endif
#if	!defined (_USEDOSCOPY) && !defined (_NOEXTRACOPY)
	{"FD_INHERITCOPY", &inheritcopy, DEFVAL(INHERITCOPY), IHTM_E, T_BOOL},
#endif
#ifndef	_NOEXTRACOPY
	{"FD_PROGRESSBAR", &progressbar, DEFVAL(PROGRESSBAR), PRGB_E, T_BOOL},
	{"FD_PRECOPYMENU", &precopymenu, DEFVAL(PRECOPYMENU), PCMN_E, T_BOOL},
#endif
#if	!MSDOS
	{"FD_ADJTTY", &adjtty, DEFVAL(ADJTTY), AJTY_E, T_BOOL},
# if	FD >= 2
	{"FD_USEGETCURSOR", &usegetcursor,
		DEFVAL(USEGETCURSOR), UGCS_E, T_BOOL},
# endif
#endif	/* !MSDOS */
	{"FD_DEFCOLUMNS", &defcolumns,
		DEFVAL(DEFCOLUMNS), CLMN_E, _B_(T_COLUMN)},
	{"FD_MINFILENAME", &minfilename,
		DEFVAL(MINFILENAME), MINF_E, T_NATURAL},
	{"FD_HISTFILE", &(histfile[0]), DEFVAL(HISTFILE), HSFL_E, T_PATH},
#if	FD >= 2
	{"FD_DIRHISTFILE", &(histfile[1]),
		DEFVAL(DIRHISTFILE), DHFL_E, T_PATH},
#endif
	{"FD_HISTSIZE", &(histsize[0]),
		DEFVAL(HISTSIZE), HSSZ_E, _B_(T_SHORT)},
	{"FD_DIRHIST", &(histsize[1]), DEFVAL(DIRHIST), DRHS_E, _B_(T_SHORT)},
	{"FD_SAVEHIST", &(savehist[0]),
		DEFVAL(SAVEHIST), SVHS_E, _B_(T_SHORT)},
#if	FD >= 2
	{"FD_SAVEDIRHIST", &(savehist[1]),
		DEFVAL(SAVEDIRHIST), SVDH_E, T_SHORT},
#endif
#ifndef	_NOTREE
	{"FD_DIRCOUNTLIMIT", &dircountlimit,
		DEFVAL(DIRCOUNTLIMIT), DCLM_E, T_INT},
#endif
#ifndef	_NODOSDRIVE
	{"FD_DOSDRIVE", &dosdrive, DEFVAL(DOSDRIVE), DOSD_E, T_DDRV},
#endif
	{"FD_SECOND", &showsecond, DEFVAL(SECOND), SCND_E, _B_(T_BOOL)},
#ifndef	_NOTRADLAYOUT
	{"FD_TRADLAYOUT", &tradlayout, DEFVAL(TRADLAYOUT), TRLO_E, T_BOOL},
#endif
	{"FD_SIZEINFO", &sizeinfo, DEFVAL(SIZEINFO), SZIF_E, _B_(T_BOOL)},
#if	FD >= 2
	{"FD_FUNCLAYOUT", &helplayout, DEFVAL(FUNCLAYOUT), FNLO_E, T_HELP},
#endif
#ifndef	_NOIME
	{"FD_IMEKEY", &imekey, DEFVAL(IMEKEY), IMKY_E, T_KEYCODE},
	{"FD_IMEBUFFER", &imebuffer, DEFVAL(IMEBUFFER), IMBF_E, T_BOOL},
#endif
#ifndef	_NOCOLOR
	{"FD_ANSICOLOR", &ansicolor, DEFVAL(ANSICOLOR), ACOL_E, _B_(T_COLOR)},
# if	FD >= 2
	{"FD_ANSIPALETTE", &ansipalette,
		DEFVAL(ANSIPALETTE), APAL_E, T_COLORPAL},
# endif
#endif	/* !_NOCOLOR */
#ifndef	_NOEDITMODE
	{"FD_EDITMODE", &editmode, DEFVAL(EDITMODE), EDMD_E, T_EDIT},
#endif
#if	FD >= 2
	{"FD_LOOPCURSOR", &loopcursor, DEFVAL(LOOPCURSOR), LPCS_E, T_BOOL},
#endif
	{"FD_TMPDIR", &deftmpdir, DEFVAL(TMPDIR), TMPD_E, T_PATH},
#if	FD >= 2
	{"FD_TMPUMASK", &tmpumask, DEFVAL(TMPUMASK), TUMSK_E, T_OCTAL},
#endif
#ifndef	_NOROCKRIDGE
	{"FD_RRPATH", &rockridgepath, DEFVAL(RRPATH), RRPT_E, T_PATHS},
#endif
#ifndef	_NOPRECEDE
	{"FD_PRECEDEPATH", &precedepath, DEFVAL(PRECEDEPATH), PCPT_E, T_PATHS},
#endif
#if	FD >= 2
	{"FD_PS1", &promptstr, DEFVAL(PROMPT), PRMP_E, T_CHARP | T_PRIMAL},
#else
	{"FD_PROMPT", &promptstr, DEFVAL(PROMPT), PRMP_E, T_CHARP},
#endif
#ifndef	_NOORIGSHELL
	{"FD_PS2", &promptstr2, DEFVAL(PROMPT2), PRM2_E, T_CHARP | T_PRIMAL},
	{"FD_DUMBSHELL", &dumbshell, DEFVAL(DUMBSHELL), DMSHL_E, T_BOOL},
#endif
#ifndef	_NOPTY
	{"FD_PTYMODE", &ptymode, DEFVAL(PTYMODE), PTYMD_E, T_BOOL},
	{"FD_PTYTERM", &ptyterm, DEFVAL(PTYTERM), PTYTM_E, T_CHARP},
	{"FD_PTYMENUKEY", &ptymenukey, DEFVAL(PTYMENUKEY), PTYKY_E, T_KEYCODE},
#endif
#ifndef	_NOLOGGING
	{"FD_LOGFILE", &logfile, DEFVAL(LOGFILE), LGFIL_E, T_PATH},
	{"FD_LOGSIZE", &logsize, DEFVAL(LOGSIZE), LGSIZ_E, T_INT},
# ifndef	NOSYSLOG
	{"FD_USESYSLOG", &usesyslog, DEFVAL(USESYSLOG), USYLG_E, T_BOOL},
# endif
	{"FD_LOGLEVEL", &loglevel, DEFVAL(LOGLEVEL), LGLVL_E, T_INT},
# ifndef	NOUID
	{"FD_ROOTLOGLEVEL", &rootloglevel,
		DEFVAL(ROOTLOGLEVEL), RLGLV_E, T_INT},
# endif
#endif	/* !_NOLOGGING */
#if	FD >= 2
	{"FD_THRUARGS", &thruargs, DEFVAL(THRUARGS), THARG_E, T_BOOL},
#endif
#ifdef	_USEUNICODE
	{"FD_UNICODEBUFFER", &unicodebuffer,
		DEFVAL(UNICODEBUFFER), UNBF_E, T_BOOL},
#endif
#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
	{"FD_LANGUAGE", &outputkcode, DEFVAL(NOCNV), LANG_E, _B_(T_KOUT)},
#endif
#ifndef	_NOKANJIFCONV
	{"FD_DEFKCODE", &defaultkcode, DEFVAL(NOCNV), DFKC_E, T_KNAM},
#endif
#ifndef	_NOKANJICONV
	{"FD_INPUTKCODE", &inputkcode, DEFVAL(NOCNV), IPKC_E, _B_(T_KIN)},
#endif
#if	!defined (_NOKANJICONV) && !defined (_NOPTY)
	{"FD_PTYINKCODE", &ptyinkcode, DEFVAL(NOCNV), PIKC_E, T_KTERM},
	{"FD_PTYOUTKCODE", &ptyoutkcode, DEFVAL(NOCNV), POKC_E, T_KTERM},
#endif
#ifndef	_NOKANJIFCONV
	{"FD_FNAMEKCODE", &fnamekcode, DEFVAL(NOCNV), FNKC_E, T_KNAM},
#endif
#if	!defined (_NOENGMES) && !defined (_NOJPNMES)
	{"FD_MESSAGELANG", &messagelang, DEFVAL(NOCNV), MESL_E, T_MESLANG},
#endif
#ifndef	_NOKANJIFCONV
	{"FD_SJISPATH", &sjispath, DEFVAL(SJISPATH), SJSP_E, T_KPATHS},
	{"FD_EUCPATH", &eucpath, DEFVAL(EUCPATH), EUCP_E, T_KPATHS},
	{"FD_JISPATH", &jis7path, DEFVAL(JISPATH), JISP_E, T_KPATHS},
	{"FD_JIS8PATH", &jis8path, DEFVAL(JIS8PATH), JS8P_E, T_KPATHS},
	{"FD_JUNETPATH", &junetpath, DEFVAL(JUNETPATH), JNTP_E, T_KPATHS},
	{"FD_OJISPATH", &ojis7path, DEFVAL(OJISPATH), OJSP_E, T_KPATHS},
	{"FD_OJIS8PATH", &ojis8path, DEFVAL(OJIS8PATH), OJ8P_E, T_KPATHS},
	{"FD_OJUNETPATH", &ojunetpath, DEFVAL(OJUNETPATH), OJNP_E, T_KPATHS},
	{"FD_HEXPATH", &hexpath, DEFVAL(HEXPATH), HEXP_E, T_KPATHS},
	{"FD_CAPPATH", &cappath, DEFVAL(CAPPATH), CAPP_E, T_KPATHS},
	{"FD_UTF8PATH", &utf8path, DEFVAL(UTF8PATH), UTF8P_E, T_KPATHS},
	{"FD_UTF8MACPATH", &utf8macpath,
		DEFVAL(UTF8MACPATH), UTF8MACP_E, T_KPATHS},
	{"FD_UTF8ICONVPATH", &utf8iconvpath,
		DEFVAL(UTF8ICONVPATH), UTF8ICONVP_E, T_KPATHS},
	{"FD_NOCONVPATH", &noconvpath, DEFVAL(NOCONVPATH), NCVP_E, T_KPATHS},
#endif	/* !_NOKANJIFCONV */
#ifndef	_NOCUSTOMIZE
	{"FD_PAGER", NULL, DEFVAL(NULL), PAGR_E, T_NOVAR},
	{"FD_EDITOR", NULL, DEFVAL(NULL), EDIT_E, T_NOVAR},
	{"FD_SHELL", NULL, DEFVAL(NULL), SHEL_E, T_NOVAR},
# ifndef	NOPOSIXUTIL
	{"FD_FCEDIT", NULL, DEFVAL(NULL), FCED_E, T_NOVAR},
# endif
# if	MSDOS
	{"FD_COMSPEC", NULL, DEFVAL(NULL), CMSP_E, T_NOVAR},
# endif
#endif	/* !_NOCUSTOMIZE */
};
#define	ENVLISTSIZ	arraysize(envlist)
#ifndef	_NOKANJIFCONV
static pathtable pathlist[] = {
	{fullpath, NULL, NOCNV, P_ISARRAY},
	{&origpath, NULL, NOCNV, P_STABLE},
	{&progpath, NULL, NOCNV, P_STABLE},
# ifdef	_USEUNICODE
	{&unitblpath, NULL, NOCNV, P_STABLE},
# endif
# ifndef	_NOIME
	{&dicttblpath, NULL, NOCNV, P_STABLE},
# endif
};
#define	PATHLISTSIZ	arraysize(pathlist)
# ifndef	_NOSPLITWIN
#  ifndef	_NOARCHIVE
static pathtable archlist[MAXWINDOWS];
#  endif
static pathtable fulllist[MAXWINDOWS];
# endif	/* !_NOSPLITWIN */
#endif	/* !_NOKANJIFCONV */
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
#define	MEDIADESCRSIZ	arraysize(mediadescr)
# endif	/* _USEDOSEMU */
static char basicenv[ENVLISTSIZ];
static int nbasic = 0;
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


VOID initenv(VOID_A)
{
#if	!MSDOS && defined (FORCEDSTDC)
	char *cp;
	int w;
#endif
	int i;

#if	!MSDOS && defined (FORCEDSTDC)
	if ((w = sizeof(char *) - sizeof(int)) > 0) {
		i = 0x5a;
		cp = (char *)(&i);
		if (*cp == 0x5a) w = 0;
	}
#endif

	for (i = 0; i < ENVLISTSIZ; i++) {
		switch (env_type(i)) {
			case T_CHARP:
			case T_PATH:
			case T_PATHS:
			case T_COLORPAL:
			case T_EDIT:
			case T_KPATHS:
			case T_NOVAR:
				if (envlist[i].var
				&& (*(char **)(envlist[i].var)))
					*((char **)(envlist[i].var)) = NULL;
				break;
			default:
#if	!MSDOS && defined (FORCEDSTDC)
				if (w > 0) {
					cp = (char *)&def_num(i);
					memmove(cp, &(cp[w]), sizeof(int));
				}
#endif
				break;
		}
		_evalenv(i);
	}
}

#if	FD >= 2
static int NEAR atooctal(s)
CONST char *s;
{
	int n;

	if (!sscanf2(s, "%<o%$", &n)) return(-1);
	n &= 0777;

	return(n);
}
#endif	/* FD >= 2 */

static int NEAR getenvid(s, len, envp)
CONST char *s;
int len, *envp;
{
	int i;

	if (len < 0) len = strlen(s);
	for (i = 0; i < ENVLISTSIZ; i++) {
		if (!strnenvcmp(s, fdenv_str(i), len) && !fdenv_str(i)[len]) {
			if (envp) *envp = 0;
			return(i);
		}
		if (!strnenvcmp(s, env_str(i), len) && !env_str(i)[len]) {
			if (envp) *envp = 1;
			return(i);
		}
	}

	return(-1);
}

static VOID NEAR _evalenv(no)
int no;
{
#if	MSDOS && !defined (_NODOSDRIVE)
	int i;
#endif
	CONST char *cp;
	char *new;
	int n;

	cp = getenv2(fdenv_str(no));
	switch (env_type(no)) {
		case T_BOOL:
			if (!cp) n = def_num(no);
			else n = (*cp && atoi2(cp)) ? 1 : 0;
			*((int *)(envlist[no].var)) = n;
			break;
		case T_SHORT:
			if ((n = atoi2(cp)) < 0) n = def_num(no);
			*((short *)(envlist[no].var)) = n;
			break;
		case T_INT:
			if ((n = atoi2(cp)) < 0) n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
		case T_NATURAL:
			if ((n = atoi2(cp)) < 0) n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
		case T_PATH:
			if (!cp) cp = def_str(no);
			new = evalpath(strdup2(cp), 0);
			if (*((char **)(envlist[no].var)))
				free(*((char **)(envlist[no].var)));
			*((char **)(envlist[no].var)) = new;
			break;
		case T_PATHS:
		case T_KPATHS:
			if (!cp) cp = def_str(no);
			new = evalpaths(cp, ':');
			if (*((char **)(envlist[no].var)))
				free(*((char **)(envlist[no].var)));
			*((char **)(envlist[no].var)) = new;
			break;
		case T_SORT:
			if ((n = atoi2(cp)) < 0 || (n / 100) > MAXSORTINHERIT
			|| ((n % 100) & ~15) || ((n % 100) & 7) > MAXSORTTYPE)
				n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			sorton = n % 100;
			break;
		case T_DISP:
#ifdef	HAVEFLAGS
			if ((n = atoi2(cp)) < 0 || n > 15)
#else
			if ((n = atoi2(cp)) < 0 || n > 7)
#endif
				n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
#ifndef	_NOWRITEFS
		case T_WRFS:
			if ((n = atoi2(cp)) < 0 || n > 2) n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
#endif
		case T_COLUMN:
			if ((n = atoi2(cp)) <= 0 || n > 5 || n == 4)
				n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
#if	MSDOS && !defined (_NODOSDRIVE)
		case T_DDRV:
			if (!cp) n = def_num(no);
			else {
				n = 0;
				if (!(new = strchr(cp, ','))) {
					if (*cp && atoi2(cp)) n |= 1;
				}
				else {
					if (!strcmp(&(new[1]), "BIOS")) n |= 2;
					if (new <= cp) /*EMPTY*/;
					else if (sscanf2(cp, "%<d", &i) != new)
						n |= 1;
					else if (i) n |= 1;
				}
			}
			*((int *)(envlist[no].var)) = n;
			break;
#endif	/* MSDOS && !_NODOSDRIVE */
#ifndef	_NOCOLOR
		case T_COLOR:
			if ((n = atoi2(cp)) < 0 || n > 3) n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
#endif
#ifndef	_NOEDITMODE
		case T_EDIT:
			if (!cp) cp = def_str(no);
			else if (strcmp(cp, "emacs")
			&& strcmp(cp, "vi")
			&& strcmp(cp, "wordstar"))
				cp = NULL;
			*((char **)(envlist[no].var)) = (char *)cp;
			break;
#endif	/* !_NOEDITMODE */
#if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
		case T_KIN:
		case T_KOUT:
		case T_KNAM:
		case T_KTERM:
		case T_MESLANG:
			n = (1 << (env_type(no) - T_KIN));
			*((int *)(envlist[no].var)) = getlang(cp, n);
			break;
#endif	/* !_NOKANJICONV || (!_NOENGMES && !NOJPNMES) */
#if	FD >= 2
		case T_OCTAL:
			if ((n = atooctal(cp)) < 0) n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
#endif
#if	!defined (_NOPTY) || !defined (_NOIME)
		case T_KEYCODE:
			if ((n = getkeycode(cp, 0)) < 0) n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
#endif
#if	FD >= 2
		case T_HELP:
			if ((n = atoi2(cp)) < 0 || (n / 100) > MAXHELPINDEX
			|| (n % 100) > (n / 100))
				n = def_num(no);
			*((int *)(envlist[no].var)) = n;
			break;
#endif
#ifndef	_NOCUSTOMIZE
		case T_NOVAR:
			break;
#endif
		default:
			if (!cp) cp = def_str(no);
			*((char **)(envlist[no].var)) = (char *)cp;
			break;
	}
}

#ifndef	_NOKANJIFCONV
static char *NEAR pathlang(pp, stable)
pathtable *pp;
int stable;
{
	char *path;

	if (pp -> flags & P_ISARRAY) path = (char *)(pp -> path);
	else path = *((char **)(pp -> path));
	if (!path) return(NULL);

	if (!stable) /*EMPTY*/;
	else if (pp -> lang == NOCNV) {
		if (!(pp -> flags & P_STABLE)) {
			if (pp -> last) free(pp -> last);
			pp -> last = NULL;
		}
		stable = 0;
	}
	else if (pp -> flags & P_STABLE) /*EMPTY*/;
	else if (!(pp -> last) || strcmp(path, pp -> last)) stable = 0;

	if (!stable && (pp -> lang = getkcode(path)) == NOCNV)
		pp -> lang = DEFCODE;

	return(path);
}

static VOID NEAR pathconv(pp)
pathtable *pp;
{
	char *path, buf[MAXPATHLEN];
	int lang;

	lang = pp -> lang;
	if (!(path = pathlang(pp, 0))) return;
	if (lang != pp -> lang) {
		kanjiconv(buf, path, MAXPATHLEN - 1, DEFCODE, lang, L_FNAME);
		if (pp -> flags & P_ISARRAY)
			kanjiconv(path, buf,
				MAXPATHLEN - 1, pp -> lang, DEFCODE, L_FNAME);
		else {
			path = newkanjiconv(buf, pp -> lang, DEFCODE, L_FNAME);
			if (path == buf) path = strdup2(buf);
			free(*((char **)(pp -> path)));
			*((char **)(pp -> path)) = path;
		}
		if (kanjierrno) pp -> lang = DEFCODE;
	}
	if (!(pp -> flags & P_STABLE)) {
		if (pp -> last) free(pp -> last);
		pp -> last = strdup2(path);
	}
}

static VOID NEAR savepathlang(VOID_A)
{
	int i;

	for (i = 0; i < PATHLISTSIZ; i++) VOID_C pathlang(&(pathlist[i]), 1);
# ifndef	_NOSPLITWIN
	for (i = 0; i < MAXWINDOWS; i++) {
#  ifndef	_NOARCHIVE
		archlist[i].path = &(winvar[i].v_archivedir);
		archlist[i].flags = P_STABLE;
		VOID_C pathlang(&(archlist[i]), 1);
#  endif
		fulllist[i].path = &(winvar[i].v_fullpath);
		fulllist[i].flags = P_STABLE;
		VOID_C pathlang(&(fulllist[i]), 1);
	}
# endif	/* !_NOSPLITWIN */
}

static VOID NEAR evalpathlang(VOID_A)
{
	int i;

	for (i = 0; i < PATHLISTSIZ; i++) pathconv(&(pathlist[i]));
# ifndef	_NOSPLITWIN
	for (i = 0; i < MAXWINDOWS; i++) {
#  ifndef	_NOARCHIVE
		pathconv(&(archlist[i]));
#  endif
		pathconv(&(fulllist[i]));
	}
# endif	/* !_NOSPLITWIN */
# ifndef	_NOPTY
	changekcode();
	changeinkcode();
	changeoutkcode();
# endif
}
#endif	/* !_NOKANJIFCONV */

static VOID NEAR evalheader(VOID_A)
{
#ifndef	_NOEXTRAWIN
	int i;
#endif
	int n;

	n = wheader;
	wheader = WHEADER;
	if (n == wheader) return;

#ifdef	_NOEXTRAWIN
	calcwin();
#else
	n -= wheader;
	for (i = 0; i < windows; i++)
		if (winvar[i].v_fileperrow + n >= WFILEMIN) break;
	if (i < windows) winvar[i].v_fileperrow += n;
#endif

#ifndef	_NOPTY
	changewsize(wheader, windows);
#endif
}

static VOID NEAR evalenvone(n)
int n;
{
#ifndef	_NOKANJIFCONV
	int type;
#endif

#ifndef	_NOKANJIFCONV
	type = env_type(n);
	if (type != T_KPATHS && (type < T_KIN || type > T_KTERM)) type = -1;
	if (type >= 0) savepathlang();
#endif
	_evalenv(n);
#ifndef	_NOKANJIFCONV
	if (type >= 0) evalpathlang();
#endif
}

VOID evalenv(s, len)
CONST char *s;
int len;
{
	int i, duperrno;

	duperrno = errno;

	if (s) {
		if ((i = getenvid(s, len, NULL)) < 0) return;
		evalenvone(i);
	}
	else {
#ifndef	_NOKANJIFCONV
		savepathlang();
#endif
		for (i = 0; i < ENVLISTSIZ; i++) _evalenv(i);
#ifndef	_NOKANJIFCONV
		evalpathlang();
#endif
	}

	evalheader();
	errno = duperrno;
}

#ifdef	DEBUG
VOID freeenvpath(VOID_A)
{
	int i;

# ifndef	_NOKANJIFCONV
	for (i = 0; i < PATHLISTSIZ; i++) {
		if (pathlist[i].last) free(pathlist[i].last);
		pathlist[i].last = NULL;
	}
# endif
	for (i = 0; i < ENVLISTSIZ; i++) switch (env_type(i)) {
		case T_PATH:
		case T_PATHS:
		case T_KPATHS:
			if (*((char **)(envlist[i].var)))
				free(*((char **)(envlist[i].var)));
			*((char **)(envlist[i].var)) = NULL;
			break;
		default:
			break;
	}
}
#endif	/* DEBUG */

#ifndef	_NOCUSTOMIZE
static int NEAR custputs(s)
CONST char *s;
{
	return(Xcprintf2("%.*k", n_lastcolumn, s));
}

static char *NEAR strcatalloc(s1, s2)
char *s1;
CONST char *s2;
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
	Xputterm(T_STANDOUT);
	Xputch2(' ');
	Xputterm(END_STANDOUT);
}

static VOID NEAR fillline(y, w)
int y, w;
{
	Xlocate(0, y);
	Xputterm(T_STANDOUT);
	cputspace(w);
	Xputterm(END_STANDOUT);
}

static char *NEAR inputcuststr(prompt, delsp, s, h)
CONST char *prompt;
int delsp;
CONST char *s;
int h;
{
	return(inputstr(prompt, delsp, (s) ? strlen(s) : 0, s, h));
}

static char *NEAR inputcustenvstr(prompt, delsp, s, h)
CONST char *prompt;
int delsp;
CONST char *s;
int h;
{
	char *cp;

	if (!(cp = inputcuststr(prompt, delsp, s, h))) return((char *)-1);
	if (!*cp && yesno(USENV_K, prompt)) {
		free(cp);
		return(NULL);
	}

	return(cp);
}

static VOID NEAR setnamelist(n, list, s)
int n;
namelist *list;
CONST char *s;
{
	memset((char *)&(list[n]), 0, sizeof(*list));
	list[n].name = (char *)s;
	list[n].ent = n;
	list[n].flags = (F_ISRED | F_ISWRI);
	list[n].tmpflags = F_STAT;
}

static int NEAR browsenamelist(list, max, col, def, prompt, mes)
namelist *list;
int max, col;
CONST char *def, *prompt, **mes;
{
	int ch, pos, old;
	int dupwin_x, dupwin_y, dupminfilename, dupcolumns, dupdispmode;

	dupminfilename = minfilename;
	dupcolumns = curcolumns;
	dupdispmode = dispmode;
	minfilename = n_column;
	curcolumns = col;
	dispmode = 0;

	dupwin_x = win_x;
	dupwin_y = win_y;
	subwindow = 1;
	pos = listupfile(list, max, def, 1);

	envcaption(prompt);
	do {
		Xlocate(0, L_INFO);
		Xputterm(L_CLEAR);
		custputs(mes[pos] ? mes[pos] : list[pos].name);
		Xputch2('.');

		Xlocate(win_x, win_y);
		Xtflush();
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
				if (pos < FILEPERPAGE) Xputterm(T_BELL);
				else pos = ((pos / FILEPERPAGE) - 1)
					* FILEPERPAGE;
				break;
			case K_NPAGE:
				if (((pos / FILEPERPAGE) + 1) * FILEPERPAGE
				>= max - 1)
					Xputterm(T_BELL);
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
			pos = listupfile(list, max, list[pos].name, 1);
		else if (old != pos) {
			putname(list, old, -1);
			calc_x += putname(list, pos, 1) + 1;
			win_x = calc_x;
			win_y = calc_y;
		}
	} while (ch != K_ESC && ch != K_CR);

	win_x = dupwin_x;
	win_y = dupwin_y;
	subwindow = 0;
	minfilename = dupminfilename;
	curcolumns = dupcolumns;
	dispmode = dupdispmode;

	return((ch == K_CR) ? pos : -1);
}

static VOID NEAR custtitle(VOID_A)
{
	CONST char *str[MAXCUSTOM];
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

	Xlocate(0, filetop(win));
	Xputterm(L_CLEAR);
	for (i = 0; i < MAXCUSTOM; i++) {
		if (i != custno) Xcprintf2("/%.*k", width, str[i]);
		else {
			Xputterm(T_STANDOUT);
			Xcprintf2("/%.*k", width, str[i]);
			Xputterm(END_STANDOUT);
		}
		if (len) Xputch2(' ');
	}

	fillline(filetop(win) + 1, n_column);
}

static VOID NEAR calcmax(max, new)
int max[], new;
{
	int i;

	max[0] = ENVLISTSIZ;
	for (i = 0; bindlist[i].key >= 0; i++) /*EMPTY*/;
	max[1] = i + new;
# ifdef	_NOKEYMAP
	max[2] = new;
# else
	for (i = 0; keyseqlist[i] >= 0; i++) /*EMPTY*/;
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
	for (i = 0; fdtype[i].name; i++) /*EMPTY*/;
	max[5] = i + new;
# else
	max[5] = new;
# endif
	max[6] = (new) ? MAXSAVEMENU : 0;
}

static VOID NEAR envcaption(s)
CONST char *s;
{
	Xlocate(0, L_HELP);
	Xputterm(L_CLEAR);
	Xputterm(T_STANDOUT);
	Xcprintf2("%.*k", n_column, s);
	Xputterm(END_STANDOUT);
}

VOID freestrarray(list, max)
char **list;
int max;
{
	int i;

	if (list) for (i = 0; i < max; i++) free(list[i]);
}

char **copystrarray(dest, src, ndestp, nsrc)
char **dest, *CONST *src;
int *ndestp, nsrc;
{
	int i;

	if (dest) freestrarray(dest, (ndestp) ? *ndestp : nsrc);
	else if (nsrc > 0) dest = (char **)malloc2(nsrc * sizeof(*dest));
	for (i = 0; i < nsrc; i++) dest[i] = strdup2(src[i]);
	if (ndestp) *ndestp = nsrc;

	return(dest);
}

static char **NEAR copyenv(list)
char **list;
{
	int i;

	if (list) {
# ifndef	_NOKANJIFCONV
		savepathlang();
# endif
		for (i = 0; i < ENVLISTSIZ; i++) {
			setenv2(fdenv_str(i), list[i * 2], 0);
			setenv2(env_str(i), list[i * 2 + 1], 0);
			_evalenv(i);
		}
# ifndef	_NOKANJIFCONV
		evalpathlang();
# endif
		evalheader();
	}
	else {
		list = (char **)malloc2(ENVLISTSIZ * 2 * sizeof(*list));
		for (i = 0; i < ENVLISTSIZ; i++) {
			list[i * 2] = strdup2(getshellvar(fdenv_str(i), -1));
			list[i * 2 + 1] = strdup2(getshellvar(env_str(i), -1));
		}
	}

	return(list);
}

static VOID NEAR cleanupenv(VOID_A)
{
	CONST char *cp;
	int i;

# ifndef	_NOKANJIFCONV
	savepathlang();
# endif
	for (i = 0; i < ENVLISTSIZ; i++) {
		setenv2(fdenv_str(i), NULL, 0);
		cp = (env_type(i) == T_CHARP) ? def_str(i) : NULL;
		setenv2(env_str(i), cp, 0);
		_evalenv(i);
	}
# ifndef	_NOKANJIFCONV
	evalpathlang();
# endif
	evalheader();
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
CONST char *s;
int argc;
CONST char *argv[];
FILE *fp;
{
	int i;

	if (!argc) return;

	fputs(s, fp);
	for (i = 0; i < argc; i++) fprintf2(fp, " %s", argv[i]);
	fputnl(fp);
}
# endif	/* !_NOORIGSHELL */

static char *NEAR int2str(buf, n)
char *buf;
int n;
{
	snprintf2(buf, MAXLONGWIDTH + 1, "%d", n);

	return(buf);
}

static int NEAR inputkeycode(s)
CONST char *s;
{
	int c, dupwin_x, dupwin_y;

	dupwin_x = win_x;
	dupwin_y = win_y;
	Xlocate(0, L_INFO);
	Xputterm(L_CLEAR);
	win_x = custputs(s);
	win_y = L_INFO;
	Xlocate(win_x, win_y);
	Xtflush();
	keyflush();
	Xgetkey(-1, 0);
	c = Xgetkey(1, 0);
	Xgetkey(-1, 0);
	win_x = dupwin_x;
	win_y = dupwin_y;

	return(c);
}

static int NEAR dispenv(no)
int no;
{
	CONST char *cp, *str[MAXSELECTSTRS];
	char *new, buf[MAXLONGWIDTH + 1];
	int n, p;

	new = NULL;
	switch (env_type(no)) {
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
			p = n / 100;
			if (p > MAXSORTINHERIT) p = MAXSORTINHERIT;
			n %= 100;
			if ((n & 7) > MAXSORTTYPE)
				n = ((n & ~7) | MAXSORTTYPE);
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
#if	FD >= 2
				if (p > 1) cp = VSARC_K;
				else
#endif
				cp = VSORT_K;
				new = strcatalloc(new, cp);
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
				new = strdup2(cp);
				cp = new = strcatalloc(new, VBIOS_K);
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
#  if	FD >= 2
		case T_COLORPAL:
			str[0] = VBLK1_K;
			str[1] = VRED1_K;
			str[2] = VGRN1_K;
			str[3] = VYEL1_K;
			str[4] = VBLU1_K;
			str[5] = VMAG1_K;
			str[6] = VCYN1_K;
			str[7] = VWHI1_K;
			str[8] = VFOR1_K;
			str[9] = VBAK1_K;
			if (!(cp = getenv2(fdenv_str(no)))) cp = def_str(no);
			new = NULL;
			for (n = 0; n < MAXPALETTE; n++) {
				if (n) new = strcatalloc(new, "/");
				p = (isdigit2(cp[n])) ? cp[n] : DEFPALETTE[n];
				p -= '0';
				new = strcatalloc(new, str[p]);
				if (!cp[n]) cp--;
			}
			cp = new;
			break;
#  endif	/* FD >= 2*/
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
		case T_KTERM:
		case T_MESLANG:
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
			str[I_UTF8] = "UTF-8 for iconv";
#  endif	/* !_NOKANJICONV */
			cp = str[*((int *)(envlist[no].var))];
			break;
# endif	/* !_NOKANJICONV || (!_NOENGMES && !NOJPNMES) */
# if	FD >= 2
		case T_OCTAL:
			cp = ascoctal(*((int *)(envlist[no].var)), buf);
			break;
# endif
# if	!defined (_NOPTY) || !defined (_NOIME)
		case T_KEYCODE:
			cp = getenv2(fdenv_str(no));
			break;
# endif
# if	FD >= 2
		case T_HELP:
			n = *((int *)(envlist[no].var));
			p = n / 100;
			n %= 100;
			new = strdup2(VFNMX_K);
			new = strcatalloc(new, int2str(buf, p));
			new = strcatalloc(new, ", ");
			new = strcatalloc(new, VFNBR_K);
			new = strcatalloc(new, int2str(buf, n));
			cp = new;
			break;
# endif	/* FD >= 2 */
		default:
			if (!(cp = getenv2(fdenv_str(no)))) cp = def_str(no);
			break;
	}

	if (!getenv2(fdenv_str(no)))
		cp = new = strcatalloc(strdup2(cp),
			(cp && *cp) ? VDEF_K : VUDEF_K);
	cputstr(MAXCUSTVAL, cp);
	n = strlen2(cp);
	if (n > MAXCUSTVAL - 1) n = MAXCUSTVAL - 1;
	if (new) free(new);

	return(n);
}

static int NEAR editenv(no)
int no;
{
	CONST char *cp, *env, *str[MAXSELECTSTRS];
	char *s, *new, buf[MAXLONGWIDTH + 1];
	int n, p, tmp, val[MAXSELECTSTRS];

	for (n = 0; n < MAXSELECTSTRS; n++) val[n] = n;

	new = NULL;
	env = env_str(no);
	switch (env_type(no)) {
		case T_BOOL:
			n = (*((int *)(envlist[no].var))) ? 1 : 0;
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			str[2] = VUSET_K;
			val[2] = -1;
			envcaption(env);
			if (noselect(&n, 3, 0, str, val)) return(0);
			cp = (n >= 0) ? int2str(buf, n) : NULL;
			break;
		case T_SHORT:
			int2str(buf, *((short *)(envlist[no].var)));
			cp = s = inputcustenvstr(env, 1, buf, -1);
			if (cp == (char *)-1) return(0);
			if (!cp) break;
			n = atoi2(cp);
			free(s);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			cp = int2str(buf, n);
			break;
		case T_INT:
		case T_NATURAL:
			int2str(buf, *((int *)(envlist[no].var)));
			cp = s = inputcustenvstr(env, 1, buf, -1);
			if (cp == (char *)-1) return(0);
			if (!cp) break;
			n = atoi2(cp);
			free(s);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			cp = int2str(buf, n);
			break;
		case T_PATH:
		case T_PATHS:
		case T_KPATHS:
			if (!(cp = getenv2(fdenv_str(no)))) cp = def_str(no);
			cp = new = inputcustenvstr(env, 1, cp, HST_PATH);
			if (cp == (char *)-1) return(0);
			break;
		case T_SORT:
			n = *((int *)(envlist[no].var));
			str[0] = ONAME_K;
			str[1] = OEXT_K;
			str[2] = OSIZE_K;
			str[3] = ODATE_K;
			str[4] = OLEN_K;
			str[5] = ORAW_K;
			str[6] = OUST_K;
			val[0] = 1;
			val[1] = 2;
			val[2] = 3;
			val[3] = 4;
			val[4] = 5;
			val[5] = 0;
			val[6] = -1;
			p = n / 100;
			if (p > MAXSORTINHERIT) p = MAXSORTINHERIT;
			n %= 100;
			if ((n & 7) > MAXSORTTYPE)
				n = ((n & ~7) | MAXSORTTYPE);
			tmp = n & ~7;
			n &= 7;
			envcaption(env);
			if (noselect(&n, 7, 0, str, val)) return(0);
			if (n < 0) {
				cp = NULL;
				break;
			}
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
			str[2] = VSRT2_K;
			val[0] = 0;
			val[1] = 1;
			val[2] = 2;
			if (noselect(&p, MAXSORTINHERIT + 1, 0, str, val))
				return(0);
			n += p * 100;
			cp = int2str(buf, n);
			break;
		case T_DISP:
			n = *((int *)(envlist[no].var));
			envcaption(VDS1B_K);
			str[0] = VDS10_K;
			str[1] = VDS11_K;
			str[2] = VUSET_K;
			val[2] = -1;
			tmp = n & 1;
			if (noselect(&tmp, 3, 0, str, val)) return(0);
			if (tmp < 0) {
				cp = NULL;
				break;
			}
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
			str[3] = VUSET_K;
			val[3] = -1;
			envcaption(env);
			if (noselect(&n, 4, 0, str, val)) return(0);
			cp = (n >= 0) ? int2str(buf, n) : NULL;
			break;
# endif	/* !_NOWRITEFS */
# if	MSDOS && !defined (_NODOSDRIVE)
		case T_DDRV:
			n = (*((int *)(envlist[no].var)) & 1);
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			str[2] = VUSET_K;
			val[2] = -1;
			envcaption(env);
			if (noselect(&n, 3, 0, str, val)) return(0);
			cp = (n >= 0) ? int2str(buf, n) : NULL;
			if (cp && *((int *)(envlist[no].var)) & 2) {
				new = strdup2(cp);
				cp = new = strcatalloc(new, ",BIOS");
			}
			break;
# endif	/* MSDOS && !_NODOSDRIVE */
		case T_COLUMN:
			n = *((int *)(envlist[no].var));
			str[0] = "1";
			str[1] = "2";
			str[2] = "3";
			str[3] = "5";
			str[4] = VUSET_K;
			val[0] = 1;
			val[1] = 2;
			val[2] = 3;
			val[3] = 5;
			val[4] = -1;
			envcaption(env);
			if (noselect(&n, 5, 0, str, val)) return(0);
			cp = (n >= 0) ? int2str(buf, n) : NULL;
			break;
# ifndef	_NOCOLOR
		case T_COLOR:
			n = *((int *)(envlist[no].var));
			str[0] = VBOL0_K;
			str[1] = VBOL1_K;
			str[2] = VCOL2_K;
			str[3] = VCOL3_K;
			str[4] = VUSET_K;
			val[4] = -1;
			envcaption(env);
			if (noselect(&n, 5, 0, str, val)) return(0);
			cp = (n >= 0) ? int2str(buf, n) : NULL;
			break;
#  if	FD >= 2
		case T_COLORPAL:
			n = 0;
			str[0] = VCREG_K;
			str[1] = VCBAK_K;
			str[2] = VCDIR_K;
			str[3] = VCUWR_K;
			str[4] = VCURD_K;
			str[5] = VCLNK_K;
			str[6] = VCSCK_K;
			str[7] = VCFIF_K;
			str[8] = VCBLD_K;
			str[9] = VCCHD_K;
			str[10] = VCEXE_K;
			str[MAXPALETTE] = VUSET_K;
			val[MAXPALETTE] = -1;
			envcaption(env);
			if (noselect(&n, MAXPALETTE + 1, 0, str, val))
				return(0);
			if (n < 0) {
				cp = NULL;
				break;
			}
			cp = *((char **)(envlist[no].var));
			p = (cp) ? strlen(cp) : 0;
			if (p < MAXPALETTE) p = MAXPALETTE;
			new = malloc2(p + 1);
			p = 0;
			if (cp) for (; cp[p]; p++) new[p] = cp[p];
			for (; p < MAXPALETTE; p++) new[p] = DEFPALETTE[p];
			new[p] = '\0';
			p = (isdigit2(new[n])) ? new[n] : DEFPALETTE[n];
			p -= '0';
			envcaption(str[n]);
			str[0] = VBLK2_K;
			str[1] = VRED2_K;
			str[2] = VGRN2_K;
			str[3] = VYEL2_K;
			str[4] = VBLU2_K;
			str[5] = VMAG2_K;
			str[6] = VCYN2_K;
			str[7] = VWHI2_K;
			str[8] = VFOR2_K;
			str[9] = VBAK2_K;
			if (noselect(&p, MAXCOLOR, 0, str, val)) {
				free(new);
				return(0);
			}
			new[n] = p + '0';
			cp = new;
			break;
#  endif	/* FD >= 2*/
# endif	/* !_NOCOLOR */
# ifndef	_NOEDITMODE
		case T_EDIT:
			cp = *((char **)(envlist[no].var));
			str[0] = "emacs";
			str[1] = "wordstar";
			str[2] = "vi";
			str[3] = VUSET_K;
			val[3] = -1;
			if (!cp) n = 0;
			else for (n = 2; n > 0; n--)
				if (!strcmp(cp, str[n])) break;
			envcaption(env);
			if (noselect(&n, 4, 0, str, val)) return(0);
			cp = (n >= 0) ? str[n] : NULL;
			break;
# endif	/* !_NOEDITMODE */
# if	!defined (_NOKANJICONV) \
|| (!defined (_NOENGMES) && !defined (_NOJPNMES))
		case T_KIN:
		case T_KNAM:
		case T_KOUT:
		case T_KTERM:
		case T_MESLANG:
			tmp = 0;
			str[tmp] = VNCNV_K;
			val[tmp++] = NOCNV;
#  if	!defined (_NOENGMES) && !defined (_NOJPNMES)
			str[tmp] = VENG_K;
			val[tmp++] = ENG;
#  endif
#  ifndef	_NOKANJICONV
			str[tmp] = "SJIS";
			val[tmp++] = SJIS;
			str[tmp] = "EUC";
			val[tmp++] = EUC;
			str[tmp] = "7bit-JIS";
			val[tmp++] = JIS7;
			str[tmp] = "8bit-JIS";
			val[tmp++] = JIS8;
			str[tmp] = "ISO-2022-JP";
			val[tmp++] = JUNET;
			str[tmp] = "Hex";
			val[tmp++] = HEX;
			str[tmp] = "UTF-8";
			val[tmp++] = UTF8;
#  endif	/* !_NOKANJICONV */
			str[tmp] = VUSET_K;
			val[tmp++] = -1;
			p = (1 << (env_type(no) - T_KIN));
			for (n = 0; n < tmp; n++) {
				if (val[n] < 0 || (kanjiiomode[val[n]] & p))
					continue;
				tmp--;
				memmove((char *)&(str[n]),
					(char *)&(str[n + 1]),
					(tmp - n) * sizeof(*str));
				memmove((char *)&(val[n]),
					(char *)&(val[n + 1]),
					(tmp - n) * sizeof(*val));
				n--;
			}
			if (!tmp) return(0);
			n = *((int *)(envlist[no].var));
#  ifndef	_NOKANJICONV
			switch (n) {
				case O_JIS7:
				case O_JIS8:
				case O_JUNET:
				case CAP:
					p = 1;
					n--;
					break;
				case M_UTF8:
				case I_UTF8:
					p = n - UTF8;
					n = UTF8;
					break;
				default:
					p = 0;
					break;
			}
#  endif	/* !_NOKANJICONV */
			envcaption(env);
			if (noselect(&n, tmp, 0, str, val)) return(0);
			if (n < 0) {
				cp = NULL;
				break;
			}
#  ifndef	_NOKANJICONV
			switch (n) {
				case JIS7:
				case JIS8:
				case JUNET:
					str[0] = VNJIS_K;
					str[1] = VOJIS_K;
					tmp = 2;
					break;
				case HEX:
					str[0] = "HEX";
					str[1] = "CAP";
					tmp = 2;
					break;
				case UTF8:
					str[0] = VUTF8_K;
					str[1] = VUTFM_K;
					str[2] = VUTFI_K;
					tmp = 3;
					break;
				default:
					tmp = -1;
					break;
			}
			if (tmp >= 0) {
				val[0] = 0;
				val[1] = 1;
				val[2] = 2;
				if (noselect(&p, tmp, 64, str, val)) return(0);
				n += p;
			}
#  endif	/* !_NOKANJICONV */
			str[NOCNV] = nullstr;
#  if	!defined (_NOENGMES) && !defined (_NOJPNMES)
			str[ENG] = "C";
#  endif
#  ifndef	_NOKANJICONV
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
			str[I_UTF8] = "utf8-iconv";
#  endif	/* !_NOKANJICONV */
			cp = str[n];
			break;
# endif	/* !_NOKANJICONV || (!_NOENGMES && !NOJPNMES) */
# if	FD >= 2
		case T_OCTAL:
			ascoctal(*((int *)(envlist[no].var)), buf);
			cp = s = inputcustenvstr(env, 1, buf, -1);
			if (cp == (char *)-1) return(0);
			if (!cp) break;
			n = atooctal(cp);
			free(s);
			if (n < 0) {
				warning(0, VALNG_K);
				return(0);
			}
			cp = ascoctal(n, buf);
			break;
# endif	/* FD >= 2 */
# if	!defined (_NOPTY) || !defined (_NOIME)
		case T_KEYCODE:
			cp = s = asprintf3(VKYCD_K, env);
			n = inputkeycode(cp);
			free(s);
			if (n == K_ESC) {
				if (!yesno(USENV_K, env)) return(0);
				cp = NULL;
				break;
			}
			cp = getkeysym(n, 0);
			if (!yesno(VKYOK_K, cp, env_str(no))) return(0);
			break;
# endif	/* !_NOPTY || !_NOIME */
# if	FD >= 2
		case T_HELP:
			n = *((int *)(envlist[no].var));
			p = n / 100;
			n %= 100;
			envcaption(env);
			int2str(buf, p);
			lcmdline = -1;
			cp = s = inputcustenvstr(VFNMX_K, 1, buf, -1);
			if (cp == (char *)-1) return(0);
			if (!cp) break;
			p = atoi2(cp);
			free(s);
			if (p < 0 || p > MAXHELPINDEX) {
				warning(0, VALNG_K);
				return(0);
			}

			envcaption(env);
			int2str(buf, n);
			lcmdline = -1;
			cp = s = inputcustenvstr(VFNBR_K, 1, buf, -1);
			if (cp == (char *)-1) return(0);
			if (!cp) break;
			n = atoi2(cp);
			free(s);
			if (n < 0 || n > p) {
				warning(0, VALNG_K);
				return(0);
			}
			cp = int2str(buf, p * 100 + n);
			break;
# endif	/* FD >= 2 */
		default:
			if (!(cp = getenv2(fdenv_str(no)))) cp = def_str(no);
			cp = new = inputcustenvstr(env, 0, cp, -1);
			if (cp == (char *)-1) return(0);
			break;
	}

	if (getshellvar(fdenv_str(no), -1)) n = setenv2(fdenv_str(no), cp, 0);
	else n = setenv2(env, cp, 0);
	if (new) free(new);
	if (n < 0) warning(-1, env);

	evalenvone(no);
	evalheader();

	return(1);
}

static int NEAR dumpenv(flaglist, fp)
CONST char *flaglist;
FILE *fp;
{
	char *cp;
	int i, n;

	for (i = n = 0; i < ENVLISTSIZ; i++) {
		if ((!flaglist || !(flaglist[i] & 2))
		&& (cp = getshellvar(env_str(i), -1))
		&& (env_type(i) != T_CHARP || strenvcmp(cp, def_str(i))))
			n++;
		if ((!flaglist || !(flaglist[i] & 1))
		&& getshellvar(fdenv_str(i), -1))
			n++;
	}
	if (!n || !fp) return(n);

	fputnl(fp);
	fputs("# shell variables definition\n", fp);
	for (i = 0; i < ENVLISTSIZ; i++) {
		if ((!flaglist || !(flaglist[i] & 2))
		&& (cp = getshellvar(env_str(i), -1))
		&& (env_type(i) != T_CHARP || strenvcmp(cp, def_str(i)))) {
			cp = killmeta(cp);
			fprintf2(fp, "%s=%s\n", env_str(i), cp);
			free(cp);
		}
		if ((!flaglist || !(flaglist[i] & 1))
		&& (cp = getshellvar(fdenv_str(i), -1))) {
			cp = killmeta(cp);
			fprintf2(fp, "%s=%s\n", fdenv_str(i), cp);
			free(cp);
		}
	}

	return(n);
}

# ifndef	_NOORIGSHELL
static int NEAR checkenv(flaglist, argv, len, fp)
char *flaglist, *CONST *argv;
int *len;
FILE *fp;
{
	CONST char *ident, **unset, **trash;
	char *cp;
	int i, n, ns, nu, nt, f;

	for (n = 0; argv[n]; n++)
		if (getenvid(argv[n], len[n], NULL) >= 0) break;
	if (!argv[n]) return(0);

	unset = trash = NULL;
	ns = nu = nt = 0;
	for (n = 0; argv[n]; n++) {
		if ((i = getenvid(argv[n], len[n], &f)) < 0) {
			if (ns++) fputc(' ', fp);
			fputs(argv[n], fp);
			continue;
		}

		ident = fdenv_str(i);
		if (f) ident += FDESIZ;
		f = (1 << f);

		if (!flaglist || !(flaglist[i] & f)) {
			if (flaglist) flaglist[i] |= f;
			if ((cp = getshellvar(ident, -1))
			&& (env_type(i) != T_CHARP
			|| strenvcmp(cp, def_str(i)))) {
				if (ns++) fputc(' ', fp);
				cp = killmeta(cp);
				fprintf2(fp, "%s=%s", ident, cp);
				free(cp);
				continue;
			}
			else if (!(envlist[i].type & T_PRIMAL)) {
				unset = (CONST char **)realloc2(unset,
					(nu + 1) * sizeof(*unset));
				unset[nu++] = ident;
				continue;
			}
		}

		trash = (CONST char **)realloc2(trash,
			(nt + 1) * sizeof(*trash));
		trash[nt++] = argv[n];
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
char *CONST *argv;
FILE *fp;
{
	CONST char *ident, **unset, **trash;
	char *cp;
	int i, n, ns, nu, nt, f;

	if (strcommcmp(argv[0], BL_UNSET)) return(0);
	if (argc < 2) return(-1);

	for (n = 1; n < argc; n++) if (getenvid(argv[n], -1, NULL) >= 0) break;
	if (n >= argc) return(0);

	unset = trash = NULL;
	ns = nu = nt = 0;
	for (n = 1; n < argc; n++) {
		if ((i = getenvid(argv[n], -1, &f)) < 0) {
			unset = (CONST char **)realloc2(unset,
				(nu + 1) * sizeof(*unset));
			unset[nu++] = argv[n];
			continue;
		}

		ident = fdenv_str(i);
		if (f) ident += FDESIZ;
		f = (1 << f);

		if (!flaglist || !(flaglist[i] & f)) {
			if (flaglist) flaglist[i] |= f;
			if ((cp = getshellvar(ident, -1))
			&& (env_type(i) != T_CHARP
			|| strenvcmp(cp, def_str(i)))) {
				if (ns++) fputc(' ', fp);
				cp = killmeta(cp);
				fprintf2(fp, "%s=%s", ident, cp);
				free(cp);
				continue;
			}
			else if (!(envlist[i].type & T_PRIMAL)) {
				unset = (CONST char **)realloc2(unset,
					(nu + 1) * sizeof(*unset));
				unset[nu++] = ident;
				continue;
			}
		}

		trash = (CONST char **)realloc2(trash,
			(nt + 1) * sizeof(*trash));
		trash[nt++] = ident;
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
bindtable *dest;
CONST bindtable *src;
{
	int i;

	if (!src) {
		if (dest) dest[0].key = -1;
	}
	else {
		for (i = 0; src[i].key >= 0; i++);
		if (dest);
		else dest = (bindtable *)malloc2((i + 1) * sizeof(*dest));
		memcpy((char *)dest, (char *)src, (i + 1) * sizeof(*dest));
	}

	return(dest);
}

static VOID NEAR cleanupbind(VOID_A)
{
	freestrarray(macrolist, maxmacro);
	maxmacro = 0;
	copybind(bindlist, origbindlist);
	copystrarray(helpindex, orighelpindex, NULL, MAXHELPINDEX);
}

static int NEAR dispbind(no)
int no;
{
	CONST char *cp1, *cp2;
	int len, width;

	if (bindlist[no].key < 0) {
		cputspace(MAXCUSTVAL);
		return(0);
	}
	if (ffunc(no) < FUNCLISTSIZ) cp1 = funclist[ffunc(no)].ident;
	else cp1 = getmacro(ffunc(no));
	if (dfunc(no) < FUNCLISTSIZ) cp2 = funclist[dfunc(no)].ident;
	else cp2 = (hasdfunc(no)) ? getmacro(dfunc(no)) : NULL;
	if (!cp2) {
		width = MAXCUSTVAL;
		cputstr(width, cp1);
	}
	else {
		width = (MAXCUSTVAL - 1) / 2;
		cputstr(width, cp1);
		putsep();
		cputstr(MAXCUSTVAL - 1 - width, cp2);
	}
	len = strlen2(cp1);
	if (len > --width) len = width;

	return(len);
}

static int NEAR selectbind(n, max, prompt)
int n, max;
CONST char *prompt;
{
	namelist *list;
	CONST char *cp, **mes;
	int i, dupsorton;

	max += FUNCLISTSIZ;

	list = (namelist *)malloc2((FUNCLISTSIZ + 2) * sizeof(namelist));
	mes = (CONST char **)malloc2((FUNCLISTSIZ + 2) * sizeof(char *));
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
	bindtable bind;
	CONST char *str;
	char *cp, *buf, *func1, *func2;
	int i, n1, n2, key;

	if ((key = bindlist[no].key) < 0) {
		key = inputkeycode(BINDK_K);
		for (no = 0; no < MAXBINDTABLE && bindlist[no].key >= 0; no++)
			if (key == (int)(bindlist[no].key)) break;
		if (no >= MAXBINDTABLE - 1) {
			warning(0, OVERF_K);
			return(0);
		}
		if (bindlist[no].key < 0)
			memcpy((char *)&(bindlist[no + 1]),
				(char *)&(bindlist[no]), sizeof(*bindlist));
	}
	if (key == K_ESC) {
		warning(0, ESCNG_K);
		return(0);
	}
	str = getkeysym(key, 0);

	func1 = func2 = NULL;
	for (;;) {
		if (bindlist[no].key < 0) {
			n1 = FUNCLISTSIZ;
			i = 1;
		}
		else {
			n1 = ffunc(no);
			i = 2;
		}
		buf = asprintf3(BINDF_K, str);
		n1 = selectbind(n1, i, buf);
		free(buf);
		if (n1 < 0) return(-1);
		else if (n1 < FUNCLISTSIZ) break;
		else if (n1 == FUNCLISTSIZ + 1) {
			if (!yesno(DLBOK_K, str)) return(-1);
			deletekeybind(no);

			return(1);
		}
		else if (maxmacro >= MAXMACROTABLE) warning(0, OVERF_K);
		else {
			i = (bindlist[no].key >= 0) ? ffunc(no) : -1;
			buf = asprintf3(BNDFC_K, str);
			cp = inputcuststr(buf, 0, getmacro(i), HST_COM);
			free(buf);
			if (!cp);
			else if (!*cp) {
				free(cp);
				n1 = NO_OPERATION;
				break;
			}
			else {
				func1 = cp;
				n1 = FNO_SETMACRO;
				break;
			}
		}
	}

	if (yesno(KBDIR_K)) n2 = FNO_NONE;
	else for (;;) {
		if (bindlist[no].key < 0) n2 = FUNCLISTSIZ;
		else if ((n2 = dfunc(no)) == FNO_NONE) n2 = ffunc(no);
		buf = asprintf3(BINDD_K, str);
		n2 = selectbind(i = n2, 1, buf);
		free(buf);
		if (n2 < 0) {
			freemacro(n1);
			return(-1);
		}
		else if (n2 < FUNCLISTSIZ) {
			if (n2 == n1) n2 = FNO_NONE;
			break;
		}
		else if (maxmacro >= MAXMACROTABLE) warning(0, OVERF_K);
		else {
			if (bindlist[no].key < 0) i = -1;
			buf = asprintf3(BNDDC_K, str);
			cp = inputcuststr(buf, 0, getmacro(i), HST_COM);
			free(buf);
			if (!cp);
			else if (!*cp) {
				free(cp);
				n2 = FNO_NONE;
				break;
			}
			else {
				func2 = cp;
				n2 = FNO_SETMACRO;
				break;
			}
		}
	}

	if (key < K_F(1) || key > K_F(MAXHELPINDEX)) cp = NULL;
	else cp = inputcuststr(FCOMM_K, 0, helpindex[key - K_F(1)], -1);
	bind.key = (short)key;
	bind.f_func = (u_char)n1;
	bind.d_func = (u_char)n2;
	VOID_C addkeybind(no, &bind, func1, func2, cp);

	return(1);
}

static int NEAR issamebind(bindp1, bindp2)
CONST bindtable *bindp1, *bindp2;
{
	if (ismacro(bindp1 -> f_func) || bindp1 -> f_func != bindp2 -> f_func)
		return(0);
	if (ismacro(bindp1 -> d_func) || bindp1 -> d_func != bindp2 -> d_func)
		return(0);
	if (gethelp(bindp1)) return(0);

	return(1);
}

static int NEAR dumpbind(flaglist, origflaglist, fp)
CONST char *flaglist;
char *origflaglist;
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
		j = searchkeybind(&(bindlist[i]), origbindlist);
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
		j = searchkeybind(&(bindlist[i]), origbindlist);
		if (j < MAXBINDTABLE && origbindlist[j].key >= 0) {
			if (issamebind(&(bindlist[i]), &(origbindlist[j])))
				continue;
		}

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
char *CONST *argv;
FILE *fp;
{
	bindtable bind;
	int i, j, n;

	if (strcommcmp(argv[0], BL_BIND)) return(0);
	if (parsekeybind(argc, argv, &bind) < 0) return(-1);
	n = (bind.d_func == FNO_NONE) ? 3 : 4;

	i = searchkeybind(&bind, bindlist);
	j = searchkeybind(&bind, origbindlist);
	if (i < MAXBINDTABLE && bindlist[i].key >= 0) {
		if (!flaglist || flaglist[i]) return(-1);
		if (j < MAXBINDTABLE && origbindlist[j].key >= 0) {
			if (!origflaglist || origflaglist[j]) return(-1);
			if (argc <= n
			&& bind.f_func == origbindlist[j].f_func
			&& bind.d_func == origbindlist[j].d_func)
				return(-1);
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
		cputspace(MAXCUSTVAL);
		return(0);
	}
	cp = encodestr(key.str, key.len);
	cputstr(MAXCUSTVAL, cp);
	len = strlen2(cp);
	if (len > MAXCUSTVAL - 1) len = MAXCUSTVAL - 1;
	free(cp);

	return(len);
}

static int NEAR editkeymap(no)
int no;
{
	CONST char *cp;
	char *str, *buf;
	ALLOC_T size;
	int i, len, key, dupwin_x, dupwin_y;

	key = keyseqlist[no];
	cp = getkeysym(key, 1);

	dupwin_x = win_x;
	dupwin_y = win_y;
	Xlocate(0, L_INFO);
	Xputterm(L_CLEAR);
	buf = asprintf3(KYMPK_K, cp);
	win_x = Xkanjiputs(buf);
	win_y = L_INFO;
	Xlocate(win_x, win_y);
	Xtflush();
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
CONST keyseq_t *list, *kp;
{
	int i;

	for (i = 0; i < K_MAX - K_MIN + 1; i++)
		if (kp -> code == list[i].code) break;

	return(i);
}

static int NEAR issamekeymap(kp1, kp2)
CONST keyseq_t *kp1, *kp2;
{
	if (!(kp1 -> len)) return((kp2 -> len) ? 0 : 1);
	else if (!(kp2 -> len) || kp1 -> len != kp2 -> len) return(0);
	else if (!(kp1 -> str) || !(kp2 -> str)
	|| memcmp(kp1 -> str, kp2 -> str, kp1 -> len))
		return(0);

	return(1);
}

static int NEAR dumpkeymap(flaglist, origflaglist, fp)
CONST char *flaglist;
char *origflaglist;
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
		if (getkeyseq(&key) >= 0) {
			if (issamekeymap(&key, &(origkeymaplist[i]))) {
				origflaglist[i] = 1;
				continue;
			}
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
		if (j < K_MAX - K_MIN + 1) {
			if (issamekeymap(&key, &(origkeymaplist[j]))) continue;
		}

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
char *CONST *argv;
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

	if (list) for (i = 0; i < max; i++) freelaunch(&(list[i]));
}

launchtable *copylaunch(dest, src, ndestp, nsrc)
launchtable *dest;
CONST launchtable *src;
int *ndestp, nsrc;
{
	int i;

	if (dest) freelaunchlist(dest, *ndestp);
	else if (nsrc > 0)
		dest = (launchtable *)malloc2(nsrc * sizeof(*dest));
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
		cputspace(MAXCUSTVAL);
		return(0);
	}
	if (!launchlist[no].format) {
		width = MAXCUSTVAL;
		cputstr(width, launchlist[no].comm);
	}
	else {
		width = (MAXCUSTVAL - 1 - 6) / 2;
		cputstr(width, launchlist[no].comm);
		putsep();
		width = MAXCUSTVAL - 1 - 6 - width;
		cputstr(width, launchlist[no].format[0]);
		putsep();
		Xcprintf2("%-2d", (int)(launchlist[no].topskip));
		putsep();
		Xcprintf2("%-2d", (int)(launchlist[no].bottomskip));
	}
	len = strlen2(launchlist[no].comm);
	if (len > --width) len = width;

	return(len);
}

static VOID NEAR verboselaunch(list)
launchtable *list;
{
	int y, yy, len, nf, ni, ne;

	custtitle();
	yy = filetop(win);
	y = 1;
	fillline(yy + y++, n_column);
	len = strlen2(list -> ext + 1);
	if (list -> flags & LF_IGNORECASE) len++;
	if (len >= MAXCUSTNAM) len = MAXCUSTNAM;
	fillline(yy + y++, MAXCUSTNAM + 1 - len);
	Xputterm(L_CLEAR);
	if (list -> flags & LF_IGNORECASE) {
		Xputch2('/');
		len--;
	}
	Xcprintf2("%.*k", len, list -> ext + 1);
	putsep();
	cputstr(MAXCUSTVAL, list -> comm);
	putsep();
	fillline(yy + y++, n_column);
	nf = ni = ne = 0;
	if (list -> format) for (; y < FILEPERROW - 1; y++) {
		if (nf >= 0) {
			if (!(list -> format[nf])) {
				nf = -1;
				fillline(yy + y, n_column);
				continue;
			}
			else if (nf) fillline(yy + y, MAXCUSTNAM + 2);
			else {
				Xlocate(0, yy + y);
				putsep();
				Xputterm(T_STANDOUT);
				Xcputs2("-t");
				Xputterm(END_STANDOUT);
				Xcprintf2("%3d", (int)(list -> topskip));
				putsep();
				Xputterm(T_STANDOUT);
				Xcputs2("-b");
				Xputterm(END_STANDOUT);
				Xcprintf2("%3d", (int)(list -> bottomskip));
				putsep();
				Xputterm(T_STANDOUT);
				Xcputs2("-f");
				Xputterm(END_STANDOUT);
				putsep();
			}
			cputstr(MAXCUSTVAL, list -> format[nf]);
			putsep();
			nf++;
		}
		else if (ni >= 0 && list -> lignore) {
			if (!(list -> lignore[ni])) {
				ni = -1;
				fillline(yy + y, n_column);
				continue;
			}
			else if (ni) fillline(yy + y, MAXCUSTNAM + 2);
			else {
				fillline(yy + y, MAXCUSTNAM - 2 + 1);
				Xputterm(T_STANDOUT);
				Xcputs2("-i");
				Xputterm(END_STANDOUT);
				putsep();
			}
			cputstr(MAXCUSTVAL, list -> lignore[ni]);
			putsep();
			ni++;
		}
		else if (ne >= 0 && list -> lerror) {
			if (!(list -> lerror[ne])) {
				ne = -1;
				fillline(yy + y, n_column);
				continue;
			}
			else if (ne) fillline(yy + y, MAXCUSTNAM + 2);
			else {
				fillline(yy + y, MAXCUSTNAM - 2 + 1);
				Xputterm(T_STANDOUT);
				Xcputs2("-e");
				Xputterm(END_STANDOUT);
				putsep();
			}
			cputstr(MAXCUSTVAL, list -> lerror[ne]);
			putsep();
			ne++;
		}
		else break;
	}
	for (; y < FILEPERROW; y++) fillline(yy + y, n_column);
}

static char **NEAR editvar(prompt, var)
CONST char *prompt;
char **var;
{
	namelist *list;
	CONST char *usg, **mes, *str[4];
	char *cp, *tmp;
	int i, n, max, val[4];

	max = countvar(var);
	for (i = 0; i < arraysize(val); i++) val[i] = i;

	list = (namelist *)malloc2((max + 1) * sizeof(namelist));
	mes = (CONST char **)malloc2((max + 1) * sizeof(char *));
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
			if (!(tmp = inputcuststr(prompt, 1, NULL, -1)))
				continue;
			if (!*tmp) {
				free(tmp);
				continue;
			}
			list = (namelist *)realloc2(list,
				(max + 2) * sizeof(*list));
			mes = (CONST char **)realloc2(mes,
				(max + 2) * sizeof(*mes));
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
		if (noselect(&i, arraysize(val), 0, str, val)) continue;
		if (i == 0) {
			if (!(tmp = inputcuststr(prompt, 1, cp, -1)))
				continue;
			if (!*tmp) {
				free(tmp);
				continue;
			}
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
		var = (char **)malloc2((max + 1) * sizeof(*var));
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
	CONST char *cp, *str[6];
	char *tmp, buf[MAXLONGWIDTH + 1];
	int i, n, val[6];
	u_char *skipp;

	str[0] = "-f";
	str[1] = "-t";
	str[2] = "-b";
	str[3] = "-i";
	str[4] = "-e";
	str[5] = ARDON_K;
	for (i = 0; i < arraysize(val); i++) val[i] = i;

	n = 0;
	for (;;) {
		verboselaunch(list);
		envcaption(ARSEL_K);
		if (noselect(&n, arraysize(val), 0, str, val)) {
			if (yesno(ARCAN_K)) return(0);
			continue;
		}
		if (n == arraysize(val) - 1) {
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
			int2str(buf, *skipp);
			if (!(tmp = inputcuststr(cp, 1, buf, -1))) continue;
			i = atoi2(tmp);
			free(tmp);
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
	char *cp;
	int n;

	if (no < maxlaunch) {
		list.ext = strdup2(launchlist[no].ext);
		list.flags = launchlist[no].flags;
		verboselaunch(&(launchlist[no]));
	}
	else {
		if (!(cp = inputcuststr(EXTLN_K, 1, NULL, -1))) return(0);
		list.ext = getext(cp, &(list.flags));
		free(cp);
		if (!(list.ext[0]) || !(list.ext[1])) {
			free(list.ext);
			return(0);
		}

		for (no = 0; no < maxlaunch; no++) {
			n = extcmp(list.ext, list.flags,
				launchlist[no].ext, launchlist[no].flags, 1);
			if (!n) break;
		}
		if (no >= MAXLAUNCHTABLE) {
			warning(0, OVERF_K);
			return(0);
		}
		if (no >= maxlaunch) {
			launchlist[no].comm = NULL;
			launchlist[no].format =
			launchlist[no].lignore = launchlist[no].lerror = NULL;
			launchlist[no].topskip =
			launchlist[no].bottomskip = (u_char)0;
			launchlist[no].flags = list.flags;
		}
	}

	for (;;) {
		list.format = duplvar(launchlist[no].format, -1);
		list.lignore = duplvar(launchlist[no].lignore, -1);
		list.lerror = duplvar(launchlist[no].lerror, -1);
		list.topskip = launchlist[no].topskip;
		list.bottomskip = launchlist[no].bottomskip;

		list.comm = inputcuststr(LNCHC_K, 0,
			launchlist[no].comm, HST_COM);
		if (!(list.comm)) {
			freelaunch(&list);
			return(-1);
		}
		else if (!*(list.comm)) {
			freelaunch(&list);
			if (no >= maxlaunch) return(-1);
			if (!yesno(DLLOK_K, launchlist[no].ext)) return(-1);
			deletelaunch(no);

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

	addlaunch(no, &list);

	return(1);
}

static int NEAR issamelaunch(lp1, lp2)
CONST launchtable *lp1, *lp2;
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
CONST char *flaglist;
char *origflaglist;
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
		j = searchlaunch(&(launchlist[i]),
			origlaunchlist, origmaxlaunch);
		if (j < origmaxlaunch) {
			origflaglist[j] = 1;
			j = issamelaunch(&(launchlist[i]),
				&(origlaunchlist[j]));
			if (j) continue;
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
		j = searchlaunch(&(launchlist[i]),
			origlaunchlist, origmaxlaunch);
		if (j < origmaxlaunch) {
			j = issamelaunch(&(launchlist[i]),
				&(origlaunchlist[j]));
			if (j) continue;
		}

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
char *CONST *argv;
FILE *fp;
{
	launchtable launch;
	int i, j;

	if (strcommcmp(argv[0], BL_LAUNCH)) return(0);
	if (parselaunch(argc, argv, &launch) < 0) return(-1);

	i = searchlaunch(&launch, launchlist, maxlaunch);
	j = searchlaunch(&launch, origlaunchlist, origmaxlaunch);
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

	if (list) for (i = 0; i < max; i++) freearch(&(list[i]));
}

archivetable *copyarch(dest, src, ndestp, nsrc)
archivetable *dest;
CONST archivetable *src;
int *ndestp, nsrc;
{
	int i;

	if (dest) freearchlist(dest, *ndestp);
	else if (nsrc > 0)
		dest = (archivetable *)malloc2(nsrc * sizeof(*dest));
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
		cputspace(MAXCUSTVAL);
		return(0);
	}
	width = (MAXCUSTVAL - 1) / 2;
	cputstr(width, archivelist[no].p_comm);
	putsep();
	cputstr(MAXCUSTVAL - 1 - width, archivelist[no].u_comm);
	len = (archivelist[no].p_comm) ? strlen2(archivelist[no].p_comm) : 0;
	if (len > --width) len = width;

	return(len);
}

static int NEAR editarch(no)
int no;
{
	archivetable list;
	char *cp;
	int n;

	if (no < maxarchive) {
		list.ext = strdup2(archivelist[no].ext);
		list.flags = archivelist[no].flags;
	}
	else {
		if (!(cp = inputcuststr(EXTAR_K, 1, NULL, -1))) return(0);
		list.ext = getext(cp, &(list.flags));
		free(cp);
		if (!(list.ext[0]) || !(list.ext[1])) {
			free(list.ext);
			return(0);
		}

		for (no = 0; no < maxarchive; no++) {
			n = extcmp(list.ext, list.flags,
				archivelist[no].ext, archivelist[no].flags, 1);
			if (!n) break;
		}
		if (no >= MAXARCHIVETABLE) {
			warning(0, OVERF_K);
			return(0);
		}
		if (no >= maxarchive) {
			archivelist[no].p_comm = archivelist[no].u_comm = NULL;
			archivelist[no].flags = list.flags;
		}
	}

	for (;;) {
		list.p_comm = inputcuststr(PACKC_K, 0,
			archivelist[no].p_comm, HST_COM);
		if (!(list.p_comm)) {
			free(list.ext);
			return(0);
		}
		else if (!*(list.p_comm)) {
			free(list.p_comm);
			list.p_comm = NULL;
		}

		list.u_comm = inputcuststr(UPCKC_K, 0,
			archivelist[no].u_comm, HST_COM);
		if (!(list.u_comm)) {
			if (list.p_comm) free(list.p_comm);
			continue;
		}
		else if (!*(list.u_comm)) {
			free(list.u_comm);
			list.u_comm = NULL;
		}
		break;
	}

	if (!(list.p_comm) && !(list.u_comm)) {
		free(list.ext);
		if (no >= maxarchive) return(0);
		if (!yesno(DLAOK_K, archivelist[no].ext)) return(0);
		deletearch(no);

		return(1);
	}

	addarch(no, &list);

	return(1);
}

static int NEAR issamearch(ap1, ap2)
CONST archivetable *ap1, *ap2;
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
CONST char *flaglist;
char *origflaglist;
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
		j = searcharch(&(archivelist[i]),
			origarchivelist, origmaxarchive);
		if (j < origmaxarchive) {
			origflaglist[j] = 1;
			j = issamearch(&(archivelist[i]),
				&(origarchivelist[j]));
			if (j) continue;
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
		j = searcharch(&(archivelist[i]),
			origarchivelist, origmaxarchive);
		if (j < origmaxarchive) {
			j = issamearch(&(archivelist[i]),
				&(origarchivelist[j]));
			if (j) continue;
		}

		printarchcomm(archivelist, i, 1, fp);
	}
	for (i = 0; i < origmaxarchive; i++) {
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
char *CONST *argv;
FILE *fp;
{
	archivetable arch;
	int i, j;

	if (strcommcmp(argv[0], BL_ARCH)) return(0);
	if (parsearch(argc, argv, &arch) < 0) return(-1);

	i = searcharch(&arch, archivelist, maxarchive);
	j = searcharch(&arch, origarchivelist, origmaxarchive);
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
#  endif	/* !_NOORIGSHELL */
# endif	/* !_NOARCHIVE */

# ifdef	_USEDOSEMU
VOID freedosdrive(list)
devinfo *list;
{
	int i;

	if (list) for (i = 0; list[i].name; i++) free(list[i].name);
}

devinfo *copydosdrive(dest, src)
devinfo *dest;
CONST devinfo *src;
{
	int i;

	if (dest) freedosdrive(dest);
	if (!src) {
		if (dest) dest[0].name = NULL;
	}
	else {
		for (i = 0; src[i].name; i++);
		if (dest);
		else dest = (devinfo *)malloc2((i + 1) * sizeof(*dest));
		for (i = 0; src[i].name; i++) {
			memcpy((char *)&(dest[i]), (char *)&(src[i]),
				sizeof(*dest));
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
#  ifdef	HDDMOUNT
	char buf[strsize("HDD98 #offset=") + MAXCOLSCOMMA(3) + 1];
#  endif
	int i, len, w1, w2, width;

	if (!fdtype[no].name) {
		cputspace(MAXCUSTVAL);
		return(0);
	}
	width = (MAXCUSTVAL - 1) / 2;
	cputstr(width, fdtype[no].name);
	putsep();

	w1 = MAXCUSTVAL - 1 - width;
#  ifdef	HDDMOUNT
	if (!fdtype[no].cyl) {
		strcpy(buf, "HDD");
		if (isupper2(fdtype[no].head)) strcat(buf, "98");
		i = strlen(buf);
		snprintf2(&(buf[i]), (int)sizeof(buf) - i, " #offset=%'Ld",
			fdtype[no].offset / fdtype[no].sect);
		cputstr(w1, buf);
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
		Xcprintf2("%-*d", w2, (int)(fdtype[no].head));
		putsep();
		Xcprintf2("%-*d", w2, (int)(fdtype[no].sect));
		putsep();
		Xcprintf2("%-*d", w2, (int)(fdtype[no].cyl));
		putsep();

		for (i = 0; i < MEDIADESCRSIZ; i++) {
			if (fdtype[no].head == mediadescr[i].head
			&& fdtype[no].sect == mediadescr[i].sect
			&& fdtype[no].cyl == mediadescr[i].cyl)
				break;
		}
		w1 -= (w2 + 1) * 3;
		cputstr(w1, (i < MEDIADESCRSIZ) ? mediadescr[i].name : NULL);
	}
	len = strlen2(fdtype[no].name);
	if (len > --width) len = width;

	return(len);
}

static int NEAR editdosdrive(no)
int no;
{
#  ifdef	HDDMOUNT
	int j;
#  endif
	devinfo dev;
	CONST char *str['Z' - 'A' + 1];
	char *cp, buf[MAXLONGWIDTH + 1], sbuf['Z' - 'A' + 1][3];
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
			VOID_C gendospath(sbuf[n], dev.drive + 'A', '\0');
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
			dev.name = inputcuststr(cp, 1,
				fdtype[no].name, HST_PATH);
			free(cp);
			if (!(dev.name)) return(0);
			else if (!*(dev.name)) {
				free(dev.name);
				if (!(fdtype[no].name)) return(0);
				if (!yesno(DLDOK_K, fdtype[no].drive))
					return(0);
				VOID_C deletedrv(no);
				return(1);
			}
		}

		envcaption(DRMED_K);
		for (i = n = 0; i < MEDIADESCRSIZ; i++) {
#  ifdef	HDDMOUNT
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

	n = searchdrv(&dev, fdtype, 1);
	if (fdtype[n].name) {
		if (n != no) warning(0, DUPLD_K);
		return(0);
	}

	if (fdtype[no].name) no = deletedrv(no);

	if ((n = insertdrv(no, &dev)) < 0) {
		if (n < -1) warning(0, OVERF_K);
		else warning(ENODEV, dev.name);
		free(dev.name);
		return(0);
	}

	return(1);
}

static int NEAR dumpdosdrive(flaglist, origflaglist, fp)
CONST char *flaglist;
char *origflaglist;
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
		j = searchdrv(&(fdtype[i]), origfdtype, 1);
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
		j = searchdrv(&(fdtype[i]), origfdtype, 1);
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
char *CONST *argv;
FILE *fp;
{
	devinfo dev;
	int i, j;

	if (strcommcmp(argv[0], BL_SDRIVE) && strcommcmp(argv[0], BL_UDRIVE))
		return(0);
	if (parsesetdrv(argc, argv, &dev) < 0) return(-1);

	i = searchdrv(&dev, fdtype, 1);
	j = searchdrv(&dev, origfdtype, 1);
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
			|| strpathcmp(fdtype[j].name, dev.name))
				break;
			flaglist[j] = 1;
		}
		for (; i > 0; i--) {
			if (!flaglist || flaglist[i - 1]) break;
			if (fdtype[i - 1].cyl
			|| fdtype[i - 1].drive + 1 != fdtype[i].drive
			|| strpathcmp(fdtype[i - 1].name, dev.name))
				break;
			flaglist[i] = 1;
		}
	}
#   endif

	flaglist[i] = 1;
	printsetdrv(fdtype, i, 1, 0, fp);

	return(1);
}
#  endif	/* !_NOORIGSHELL */
# endif	/* _USEDOSEMU */

static int NEAR dispsave(no)
int no;
{
	CONST char *mes[MAXSAVEMENU];
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
	cputstr(MAXCUSTVAL, mes[no]);
	len = strlen2(mes[no]);
	if (len > MAXCUSTVAL - 1) len = MAXCUSTVAL - 1;

	return(len);
}

# ifndef	_NOORIGSHELL
static int NEAR overwriteconfig(val, file)
int *val;
CONST char *file;
{
	syntaxtree *trp, *stree;
	lockbuf_t *lck;
	FILE *fpin, *fpout;
	char *cp, *buf, **argv, **subst, path[MAXPATHLEN];
	char *flaglist[MAXCUSTOM - 1], *origflaglist[MAXCUSTOM - 1];
	int i, n, len, *slen, fd, argc, max[MAXCUSTOM], origmax[MAXCUSTOM - 1];

	if (!(lck = lockfopen(file, "r", O_TEXT | O_RDWR))) {
		warning(-1, file);
		return(-1);
	}

	strcpy(path, file);
	if ((fpin = lck -> fp)) fd = opentmpfile(path, 0666);
	else {
		lockclose(lck);
		lck = lockopen(path,
			O_TEXT | O_WRONLY | O_CREAT | O_EXCL, 0666);
		fd = (lck) ? lck -> fd : -1;
	}
	fpout = (fd >= 0) ? Xfdopen(fd, "w") : NULL;

	if (fpout) {
		if (!fpin) {
			lck -> fp = fpout;
			lck -> flags |= LCK_STREAM;
		}
	}
	else {
		warning(-1, path);
#  if	!MSDOS && !defined (CYGWIN)
		if (fd >= 0) {
			Xunlink(path);
			if (fpin) VOID_C Xclose(fd);
		}
		lockclose(lck);
#  else
		lockclose(lck);
		if (fd >= 0) {
			if (fpin) VOID_C Xclose(fd);
			Xunlink(path);
		}
#  endif
		return(-1);
	}

	calcmax(max, 0);
	origmax[0] = 0;
	for (i = 0; origbindlist[i].key >= 0; i++) /*EMPTY*/;
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
	for (i = 0; origfdtype[i].name; i++) /*EMPTY*/;
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
	for (i = 0; i < MAXCUSTOM - 1; i++) {
		if (flaglist[i]) free(flaglist[i]);
		if (origflaglist[i]) free(origflaglist[i]);
	}

	if (!fpin) lockclose(lck);
	else {
#  if	!MSDOS && !defined (CYGWIN)
		n = Xrename(path, file);
		lockclose(lck);
		Xfclose(fpout);
#  else	/* MSDOS || CYGWIN */
		lockclose(lck);
		Xfclose(fpout);
#   if	MSDOS
		n = Xrename(path, file);
#   else
		while ((n = Xrename(path, file)) < 0) {
			if (errno != EACCES) break;
			usleep(100000L);
		}
#   endif
#  endif	/* MSDOS || CYGWIN */
		if (n < 0) {
			warning(-1, file);
			Xunlink(path);
			return(-1);
		}
	}

	return(0);
}
# endif	/* !_NOORIGSHELL */

static int NEAR selectmulti(max, str, val)
int max;
CONST char *CONST str[];
int val[];
{
	int i, ch;

	ch = selectstr(NULL, max, 0, str, val);
	if (ch == K_CR) {
		for (i = 0; i < max; i++) if (val[i]) break;
		if (i >= max) ch = K_ESC;
	}

	return(ch != K_CR);
}

static int NEAR editsave(no)
int no;
{
	lockbuf_t *lck;
	CONST char *str[MAXCUSTOM - 1];
	char *file;
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
			if (selectmulti(MAXCUSTOM - 1, str, val)) break;
			done = 1;
			if (val[0]) copyenv(tmpenvlist);
			if (val[1]) {
				copystrarray(macrolist, tmpmacrolist,
					&maxmacro, tmpmaxmacro);
				copystrarray(helpindex, tmphelpindex,
					NULL, MAXHELPINDEX);
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
			if (selectmulti(MAXCUSTOM - 1, str, val)) break;
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
			file = inputcuststr(FLOAD_K, 1, FD_RCFILE, HST_PATH);
			if (!file) break;
			if (!*file) {
				free(file);
				break;
			}
			done = 1;
			n = sigvecset(0);
			Xlocate(0, n_line - 1);
			inruncom = 1;
			i = loadruncom(file, 0);
			free(file);
			inruncom = 0;
			sigvecset(n);
			if (i < 0) {
				hideclock = 1;
				warning(0, HITKY_K);
			}
			break;
		case 3:
			file = inputcuststr(FSAVE_K, 1, FD_RCFILE, HST_PATH);
			if (!file) break;
			done = 1;
			file = evalpath(file, 0);
# ifdef	FAKEUNINIT
			lck = NULL;	/* fake for -Wuninitialized */
# endif
			if (!*file
			|| (Xaccess(file, F_OK) >= 0 && yesno(FSVOK_K)))
				done = 0;
			else {
				envcaption(SSAVE_K);
				if (selectmulti(MAXCUSTOM - 1, str, val))
					done = 0;
				else {
					lck = lockfopen(file, "w",
						O_TEXT | O_WRONLY
						| O_CREAT | O_TRUNC);
					if (!lck || !(lck -> fp)) {
						warning(-1, file);
						lockclose(lck);
						done = 0;
					}
				}
			}
			free(file);

			if (!done) break;
			fputs("# configurations by customizer\n", lck -> fp);
			if (val[0]) dumpenv(NULL, lck -> fp);
			if (val[1]) dumpbind(NULL, NULL, lck -> fp);
# ifndef	_NOKEYMAP
			if (val[2]) dumpkeymap(NULL, NULL, lck -> fp);
# endif
# ifndef	_NOARCHIVE
			if (val[3]) dumplaunch(NULL, NULL, lck -> fp);
			if (val[4]) dumparch(NULL, NULL, lck -> fp);
# endif
# ifdef	_USEDOSEMU
			if (val[5]) dumpdosdrive(NULL, NULL, lck -> fp);
# endif
			lockclose(lck);
			break;
		case 4:
# ifndef	_NOORIGSHELL
			file = inputcuststr(FOVWR_K, 1, FD_RCFILE, HST_PATH);
			if (!file) break;
			done = 1;
			file = evalpath(file, 0);
			if (!*file
			|| (Xaccess(file, F_OK) < 0 && errno == ENOENT
			&& !yesno(FOVOK_K)))
				done = 0;
			else {
				envcaption(SOVWR_K);
				if (selectmulti(MAXCUSTOM - 1, str, val))
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
	CONST char *cp;
	char *name[MAXSAVEMENU];
	int len;

	cp = NEWET_K;
	switch (custno) {
		case 0:
			if (basiccustom) no = basicenv[no];
			cp = env_str(no);
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
				VOID_C gendospath(buf, fdtype[no].drive, '\0');
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
			cp = nullstr;
			break;
	}

	len = strlen2(cp);
	if (len >= MAXCUSTNAM) len = MAXCUSTNAM;
	fillline(y, MAXCUSTNAM + 1 - len);
	if (isstandout) {
		Xputterm(T_STANDOUT);
		Xcprintf2("%.*k", MAXCUSTNAM, cp);
		Xputterm(END_STANDOUT);
	}
	else if (stable_standout) Xlocate(MAXCUSTNAM + 1, y);
	else Xcprintf2("%.*k", MAXCUSTNAM, cp);
	putsep();
}

static VOID NEAR dispcust(VOID_A)
{
	int i, n, y, yy, start, end;

	yy = filetop(win);
	y = 2;
	start = (cs_item / cs_row) * cs_row;
	end = start + cs_row;
	if (end > cs_max) end = cs_max;

	for (i = start; i < end; i++) {
		dispname(i, yy + y++, 1);
		Xputterm(L_CLEAR);
		switch (custno) {
			case 0:
				n = (basiccustom) ? basicenv[i] : i;
				cs_len[i - start] = dispenv(n);
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
					Xcprintf2("%.*k", MAXCUSTVAL, NIMPL_K);
				if (cs_len[i - start] > MAXCUSTVAL - 1)
					cs_len[i - start] = MAXCUSTVAL - 1;
				else cputspace(MAXCUSTVAL - cs_len[i - start]);
				break;
		}
		putsep();
		fillline(yy + y++, n_column);
	}
	for (; y < FILEPERROW; y++) fillline(yy + y, n_column);
}

VOID rewritecust(VOID_A)
{
	cs_row = (FILEPERROW - 2) / 2;
	cs_len = (int *)realloc2(cs_len, cs_row * sizeof(*cs_len));
	dispcust();
	movecust(-1, 1);
	custtitle();
	Xtflush();
}

static VOID NEAR movecust(old, all)
int old, all;
{
	CONST char *hmes[MAXCUSTOM];

	if (all > 0) {
		hmes[0] = NULL;
		hmes[1] = HBIND_K;
		hmes[2] = HKYMP_K;
		hmes[3] = HLNCH_K;
		hmes[4] = HARCH_K;
		hmes[5] = HSDRV_K;
		hmes[6] = HSAVE_K;
		Xlocate(0, L_INFO);
		Xputterm(L_CLEAR);
		if (hmes[custno]) custputs(hmes[custno]);
		else custputs(mesconv(envlist[cs_item].hmes,
			envlist[cs_item].hmes_eng));
	}

	win_y = filetop(win) + 2 + (cs_item % cs_row) * 2;
	dispname(cs_item, win_y, 0);
	win_x = cs_len[cs_item % cs_row] + MAXCUSTNAM + 2;
	if (old >= 0 && old < cs_max)
		dispname(old, filetop(win) + 2 + (old % cs_row) * 2, 1);
	Xlocate(win_x, win_y);
}

static int NEAR editcust(VOID_A)
{
	int i, n;

	switch (custno) {
		case 0:
			n = (basiccustom) ? basicenv[cs_item] : cs_item;
			n = editenv(n);
			cs_max = (basiccustom) ? nbasic : ENVLISTSIZ;
			break;
		case 1:
			if ((n = editbind(cs_item))) {
				for (i = 0; bindlist[i].key >= 0; i++)
					/*EMPTY*/;
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
				for (i = 0; fdtype[i].name; i++) /*EMPTY*/;
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
	int i, yy, ch, old, changed, item[MAXCUSTOM], max[MAXCUSTOM];

	yy = filetop(win);
	for (i = 0; i < FILEPERROW; i++) {
		Xlocate(0, yy + i);
		Xputterm(L_CLEAR);
	}
	for (i = 0; i < MAXCUSTOM; i++) item[i] = 0;

	nbasic = 0;
	for (i = 0; i < ENVLISTSIZ; i++)
		if (envlist[i].type & T_BASIC) basicenv[nbasic++] = i;
	while (i < ENVLISTSIZ) basicenv[i++] = -1;

	tmpenvlist = copyenv(NULL);
	tmpmacrolist = copystrarray(NULL, macrolist, &tmpmaxmacro, maxmacro);
	tmphelpindex = copystrarray(NULL, helpindex, NULL, MAXHELPINDEX);
	tmpbindlist = copybind(NULL, bindlist);
	for (i = 0; bindlist[i].key >= 0; i++) /*EMPTY*/;
# ifndef	_NOKEYMAP
	tmpkeymaplist = copykeyseq(NULL);
	for (n = 0; keyidentlist[n].no > 0; n++) /*EMPTY*/;
	keyseqlist = (short *)malloc2((n + 20 + 1) * sizeof(*keyseqlist));
	n = 0;
	for (i = 1; i <= 20; i++) {
		for (ch = 0; ch <= K_MAX - K_MIN; ch++)
			if (tmpkeymaplist[ch].code == K_F(i)) break;
		if (ch <= K_MAX - K_MIN) keyseqlist[n++] = K_F(i);
	}
	for (i = 0; keyidentlist[i].no > 0; i++) {
		for (ch = 0; ch <= K_MAX - K_MIN; ch++)
			if (tmpkeymaplist[ch].code == keyidentlist[i].no)
				break;
		if (ch <= K_MAX - K_MIN) keyseqlist[n++] = keyidentlist[i].no;
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
	getmax(max, custno);
	cs_row = (FILEPERROW - 2) / 2;
	cs_len = (int *)malloc2(cs_row * sizeof(*cs_len));
	custtitle();
	old = -1;
	do {
		if (old < 0) dispcust();
		if (cs_item != old) movecust(old, 1);
		Xlocate(win_x, win_y);
		Xtflush();
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
				&& !(++cs_item % cs_row))
					old = -1;
				break;
			case K_RIGHT:
				if (custno < MAXCUSTOM - 1) {
					item[custno] = cs_item;
					setmax(max, custno);
					custno++;
					custtitle();
					cs_item = item[custno];
					getmax(max, custno);
					old = -1;
				}
				break;
			case K_LEFT:
				if (custno > 0) {
					item[custno] = cs_item;
					setmax(max, custno);
					custno--;
					custtitle();
					cs_item = item[custno];
					getmax(max, custno);
					old = -1;
				}
				break;
			case K_PPAGE:
				if (cs_item < cs_row) Xputterm(T_BELL);
				else {
					cs_item = (cs_item / cs_row - 1)
						* cs_row;
					old = -1;
				}
				break;
			case K_NPAGE:
				if ((cs_item / cs_row + 1) * cs_row >= cs_max)
					Xputterm(T_BELL);
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
					setmax(max, custno);
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
	freestrarray(tmpmacrolist, tmpmaxmacro);
	if (tmpmacrolist) free(tmpmacrolist);
	freestrarray(tmphelpindex, MAXHELPINDEX);
	if (tmphelpindex) free(tmphelpindex);
	if (tmpbindlist) free(tmpbindlist);
# ifndef	_NOKEYMAP
	freekeyseq(tmpkeymaplist);
	free(keyseqlist);
# endif
# ifndef	_NOARCHIVE
	freelaunchlist(tmplaunchlist, tmpmaxlaunch);
	if (tmplaunchlist) free(tmplaunchlist);
	freearchlist(tmparchivelist, tmpmaxarchive);
	if (tmparchivelist) free(tmparchivelist);
# endif
# ifdef	_USEDOSEMU
	freedosdrive(tmpfdtype);
	if (tmpfdtype) free(tmpfdtype);
# endif

	return(1);
}
#endif	/* !_NOCUSTOMIZE */
