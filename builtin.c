/*
 *	builtin.c
 *
 *	Builtin Command
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kanji.h"
#include "kctype.h"
#include "funcno.h"

#if	MSDOS
#include "unixemu.h"
#else
# ifdef	_NODOSDRIVE
# include <sys/param.h>
# else
# include "dosdisk.h"
# endif
#endif

#ifndef	_NOARCHIVE
extern launchtable launchlist[];
extern int maxlaunch;
extern archivetable archivelist[];
extern int maxarchive;
#endif
extern char *macrolist[];
extern int maxmacro;
extern aliastable aliaslist[];
extern int maxalias;
extern userfunctable userfunclist[];
extern int maxuserfunc;
extern bindtable bindlist[];
extern functable funclist[];
#if	!MSDOS && !defined (_NODOSDRIVE)
extern devinfo fdtype[];
#endif
extern char **sh_history;
extern char *helpindex[];

#ifndef	_NOARCHIVE
static char *getext();
static int setlaunch();
static int setarch();
static VOID printext();
static int extcmp();
static int printlaunch();
static int printarch();
#endif
static int getcommand();
static int getkeycode();
static int setkeybind();
static VOID _printmacro();
static int printmacro();
static int setalias();
static int unalias();
#if	!MSDOS && !defined (_NODOSDRIVE)
static int _setdrive();
static int setdrive();
static int unsetdrive();
static int printdrive();
#endif
static int setuserfunc();
#if	!MSDOS && !defined (_NOKEYMAP)
static int setkeymap();
static int getkey();
#endif
static int exportenv();
static int dochdir();
static int loadsource();
static int printhist();
static int isinternal();

builtintable builtinlist[] = {
	{printenv, "printenv"},
#ifndef	_NOARCHIVE
	{setlaunch, "launch"},
	{setarch, "arch"},
	{printlaunch, "printlaunch"},
	{printarch, "printarch"},
#endif
	{setkeybind, "bind"},
	{printmacro, "printbind"},
	{setalias, "alias"},
	{unalias, "unalias"},
#if	!MSDOS && !defined (_NODOSDRIVE)
	{setdrive, "setdrv"},
	{unsetdrive, "unsetdrv"},
	{printdrive, "printdrv"},
#endif
	{setuserfunc, "function"},
#if	!MSDOS && !defined (_NOKEYMAP)
	{setkeymap, "keymap"},
	{getkey, "getkey"},
#endif
	{exportenv, "export"},
	{dochdir, "chdir"},
	{dochdir, "cd"},
	{loadsource, "source"},
	{printhist, "history"},
	{NULL, NULL}
};
static strtable keyidentlist[] = {
	{K_DOWN,	"DOWN"},
	{K_UP,		"UP"},
	{K_LEFT,	"LEFT"},
	{K_RIGHT,	"RIGHT"},
	{K_HOME,	"HOME"},
	{K_BS,		"BS"},

	{K_F('*'),	"ASTER"},
	{K_F('+'),	"PLUS"},
	{K_F(','),	"COMMA"},
	{K_F('-'),	"MINUS"},
	{K_F('.'),	"DOT"},
	{K_F('/'),	"SLASH"},
	{K_F('0'),	"TK0"},
	{K_F('1'),	"TK1"},
	{K_F('2'),	"TK2"},
	{K_F('3'),	"TK3"},
	{K_F('4'),	"TK4"},
	{K_F('5'),	"TK5"},
	{K_F('6'),	"TK6"},
	{K_F('7'),	"TK7"},
	{K_F('8'),	"TK8"},
	{K_F('9'),	"TK9"},
	{K_F('='),	"EQUAL"},
	{K_F('?'),	"RET"},

	{K_DL,		"DELLIN"},
	{K_IL,		"INSLIN"},
	{K_DC,		"DEL"},
	{K_IC,		"INS"},
	{K_EIC,		"EIC"},
	{K_CLR,		"CLR"},
	{K_EOS,		"EOS"},
	{K_EOL,		"EOL"},
	{K_ESF,		"ESF"},
	{K_ESR,		"ESR"},
	{K_NPAGE,	"NPAGE"},
	{K_PPAGE,	"PPAGE"},
	{K_STAB,	"STAB"},
	{K_CTAB,	"CTAB"},
	{K_CATAB,	"CATAB"},
	{K_ENTER,	"ENTER"},
	{K_SRST,	"SRST"},
	{K_RST,		"RST"},
	{K_PRINT,	"PRINT"},
	{K_LL,		"LL"},
	{K_A1,		"A1"},
	{K_A3,		"A3"},
	{K_B2,		"B2"},
	{K_C1,		"C1"},
	{K_C3,		"C3"},
	{K_BTAB,	"BTAB"},
	{K_BEG,		"BEG"},
	{K_CANC,	"CANC"},
	{K_CLOSE,	"CLOSE"},
	{K_COMM,	"COMM"},
	{K_COPY,	"COPY"},
	{K_CREAT,	"CREAT"},
	{K_END,		"END"},
	{K_EXIT,	"EXIT"},
	{K_FIND,	"FIND"},
	{K_HELP,	"HELP"},
	{0,		NULL}
};
static char escapechar[] = "abefnrtv";
static char escapevalue[] = {0x07, 0x08, 0x1b, 0x0c, 0x0a, 0x0d, 0x09, 0x0b};


#ifndef	_NOARCHIVE
static char *getext(ext)
char *ext;
{
	char *cp, *tmp;

	cp = cnvregexp(ext, 0);
	if (*ext != '*') {
		tmp = cp;
		cp = (char *)malloc2(strlen(tmp) + 3);
		*cp = *tmp;
		strcpy(cp + 2, tmp);
		memcpy(cp + 1, "^.*", 3);
		free(tmp);
	}

	return(cp);
}

static int setlaunch(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp, *tmp, *ext;
	int i, j, ch;

	if (argc <= 1) return(-1);
	ext = getext(argv[1]);
	for (i = 0; i < maxlaunch; i++)
		if (!strpathcmp(launchlist[i].ext, ext)) break;
	if (i < maxlaunch) {
		free(launchlist[i].ext);
		free(launchlist[i].comm);
	}
	else if (maxlaunch < MAXLAUNCHTABLE) maxlaunch++;
	else {
		free(ext);
		return(-1);
	}

	if (argc == 2) {
		maxlaunch--;
		for (; i < maxlaunch; i++)
			memcpy(&launchlist[i], &launchlist[i + 1],
				sizeof(launchtable));
		free(ext);
		return(0);
	}

	launchlist[i].ext = ext;
	launchlist[i].comm = strdup2(argv[2]);

	if (argc <= 3) launchlist[i].topskip = 255;
	else {
		cp = tmp = catargs(argc - 3, &argv[3], 0);
		launchlist[i].topskip = atoi(cp);
		if (!(cp = strchr(cp, ','))) {
			launchlist[i].topskip = 255;
			free(tmp);
			return(0);
		}
		launchlist[i].bottomskip = atoi(++cp);
		ch = ':';
		for (j = 0; j < MAXLAUNCHFIELD; j++) {
			if (!(cp = strchr(cp, ch))) {
				launchlist[i].topskip = 255;
				free(tmp);
				return(0);
			}
			cp = getrange(++cp,
				&(launchlist[i].field[j]),
				&(launchlist[i].delim[j]),
				&(launchlist[i].width[j]));
			ch = ',';
		}
		ch = ':';
		for (j = 0; j < MAXLAUNCHSEP; j++) {
			if (!(cp = strchr(cp, ch))) break;
			launchlist[i].sep[j] =
				((ch = atoi(++cp)) >= 0) ? ch - 1 : 255;
			ch = ',';
		}
		for (; j < MAXLAUNCHSEP; j++) launchlist[i].sep[j] = -1;
		if (cp && (cp = strchr(cp, ':')) && (ch = atoi(++cp)) > 1)
			launchlist[i].lines = ch;
		else launchlist[i].lines = 1;
		free(tmp);
	}
	return(0);
}

static int setarch(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *ext;
	int i;

	if (argc <= 1) return(-1);
	ext = getext(argv[1]);

	for (i = 0; i < maxarchive; i++)
		if (!strpathcmp(archivelist[i].ext, ext)) break;
	if (i < maxarchive) {
		free(archivelist[i].ext);
		free(archivelist[i].p_comm);
		if (archivelist[i].u_comm) free(archivelist[i].u_comm);
	}
	else if (maxarchive < MAXARCHIVETABLE) maxarchive++;
	else {
		free(ext);
		return(-1);
	}

	if (argc == 2) {
		maxarchive--;
		for (; i < maxarchive; i++)
			memcpy(&archivelist[i], &archivelist[i + 1],
				sizeof(archivetable));
		free(ext);
		return(0);
	}

	archivelist[i].ext = ext;
	archivelist[i].p_comm = strdup2(argv[2]);
	archivelist[i].u_comm = (argc > 3) ? strdup2(argv[3]) : NULL;
	return(0);
}

static VOID printext(ext)
char *ext;
{
	char *cp;

	putch2('"');
	for (cp = ext + 1; *cp; cp++) {
		if (*cp == '\\') {
			if (!*(++cp)) break;
			putch2((int)(*(u_char *)cp));
		}
		else if (*cp == '.' && *(cp + 1) != '*') putch2('?');
		else if (!strchr(".^$", *cp)) putch2((int)(*(u_char *)cp));
	}
	putch2('"');
}

static int extcmp(str, ext)
char *str, *ext;
{
	char *cp1, *cp2;

	for (cp1 = str, cp2 = ext + 1; *cp1 && *cp2; cp2++) {
		if (*cp2 == '\\') {
			if (*cp1 == *(++cp2)) cp1++;
			else if (*cp2 != '.' || cp1 != str) return(-1);
		}
		else if (*cp2 == '.') {
			if (*(++cp2) == '*') {
				if (*cp1 == '*') cp1++;
				else if (cp1 != str) return(-1);
			}
			else if (*cp1 == '?') cp1++;
			else if (cp1 != str) return(-1);
		}
		else if (!strchr(".^$", *cp2)) {
			if (*(cp1++) != *cp2) return(-1);
		}
	}
	if (*cp2 && strchr(".^$", *cp2)) return(*cp1);
	return(*cp1 - *cp2);
}

static int printlaunch(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, j, ch, n;

	if (!comline) return(0);
	n = 1;
	if (argc <= 1) for (i = n = 0; i < maxlaunch; i++) {
		cputs2("launch ");
		printext(launchlist[i].ext);
		kanjiprintf("\t\"%s\"", launchlist[i].comm);
		if (launchlist[i].topskip < 255) cputs2(" (Arch)");
		cputs2("\r\n");
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	else for (i = 0; i < maxlaunch; i++)
	if (!extcmp(argv[1], launchlist[i].ext)) {
		kanjiprintf("\"%s\"", launchlist[i].comm);
		if (launchlist[i].topskip < 255) {
			cprintf2("\t(%d,%d",
				launchlist[i].topskip,
				launchlist[i].bottomskip);
			ch = ':';
			for (j = 0; j < MAXLAUNCHFIELD; j++) {
				putch2(ch);
				if (launchlist[i].field[j] >= 255)
					cprintf2("%d", 0);
				else cprintf2("%d", launchlist[i].field[j] + 1);
				if (launchlist[i].delim[j] >= 128)
					cprintf2("[%d]",
						launchlist[i].delim[j] - 128);
				else if (launchlist[i].delim[j])
					cprintf2("'%c'",
						launchlist[i].delim[j]);
				if (launchlist[i].width[j] >= 128)
					cprintf2("-%d",
						launchlist[i].width[j] - 128);
				else if (launchlist[i].width[j])
					cprintf2("-'%c'",
						launchlist[i].width[j]);
				ch = ',';
			}
			ch = ':';
			for (j = 0; j < MAXLAUNCHSEP; j++) {
				if (launchlist[i].sep[j] >= 255) break;
				putch2(ch);
				cprintf2("%d", launchlist[i].sep[j] + 1);
				ch = ',';
			}
			if (launchlist[i].lines > 1) {
				if (!j) putch2(':');
				cprintf2("%d", launchlist[i].lines);
			}
			putch2(')');
		}
		cputs2("\r\n");
		break;
	}
	return(i ? n : 1);
}

static int printarch(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, n;

	if (!comline) return(0);
	n = 1;
	if (argc <= 1) for (i = n = 0; i < maxarchive; i++) {
		cputs2("arch ");
		printext(archivelist[i].ext);
		kanjiprintf("\t\"%s\"", archivelist[i].p_comm);
		if (archivelist[i].u_comm)
			kanjiprintf("\t\"%s\"", archivelist[i].u_comm);
		cputs2("\r\n");
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	else for (i = 0; i < maxarchive; i++)
	if (!extcmp(argv[1], archivelist[i].ext)) {
		kanjiprintf("\"%s\"", archivelist[i].p_comm);
		if (archivelist[i].u_comm)
			kanjiprintf("\t\"%s\"", archivelist[i].u_comm);
		cputs2("\r\n");
		break;
	}
	return(i ? n : 1);
}
#endif	/* !_NOARCHIVE */

static int getcommand(cp)
char *cp;
{
	char *tmp;
	int n;

	for (n = 0; n < NO_OPERATION; n++)
		if (!strcmp(cp, funclist[n].ident)) break;
	if (n < NO_OPERATION) return(n);

	if (maxmacro >= MAXMACROTABLE) return(-1);
	macrolist[maxmacro] = strdup2(cp);
	n = NO_OPERATION + (++maxmacro);
	return(n);
}

static int getkeycode(cp)
char *cp;
{
	int i, ch;

	ch = *(cp++);
	if (*cp) switch (ch) {
		case '^':
			if (*cp < '@' || *cp > '_') return(-1);
			ch = toupper2(*(cp++)) - '@';
			break;
		case 'F':
			if ((i = atoi2(cp)) < 1 || i > 20) return(-1);
			for (; *cp >= '0' && *cp <= '9'; cp++);
			ch = K_F(i);
			break;
		default:
			cp--;
			for (i = 0; keyidentlist[i].no; i++)
				if (!strcmp(keyidentlist[i].str, cp)) break;
			if (!(ch = keyidentlist[i].no)) return(-1);
			cp += strlen(cp);
			break;
	}
	return(!(*cp) ? ch : -1);
}

static int setkeybind(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp;
	int i, j, ch, n1, n2;

	if (argc <= 1) return(-1);
	if ((ch = getkeycode(argv[1])) < 0 || ch == '\033') return(-1);

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
		if (ch == (int)(bindlist[i].key)) break;
	if (i >= MAXBINDTABLE - 1) return(-1);
	if (bindlist[i].key < 0)
		memcpy(&bindlist[i + 1], &bindlist[i], sizeof(bindtable));
	else {
		n1 = bindlist[i].f_func;
		n2 = bindlist[i].d_func;
		bindlist[i].f_func = bindlist[i].d_func = NO_OPERATION;
		if (n1 <= NO_OPERATION) n1 = 255;
		else {
			free(macrolist[n1 - NO_OPERATION - 1]);
			maxmacro--;
			for (j = n1 - NO_OPERATION - 1; j < maxmacro; j++)
				memcpy(&macrolist[j], &macrolist[j + 1],
					sizeof(char *));
		}
		if (n2 <= NO_OPERATION || n2 >= 255) n2 = 255;
		else {
			free(macrolist[n2 - NO_OPERATION - 1]);
			maxmacro--;
			for (j = n2 - NO_OPERATION - 1; j < maxmacro; j++)
				memcpy(&macrolist[j], &macrolist[j + 1],
					sizeof(char *));
		}
		for (j = 0; j < MAXBINDTABLE && bindlist[j].key >= 0; j++) {
			if (bindlist[j].f_func > n2) bindlist[j].f_func--;
			if (bindlist[j].f_func >= n1) bindlist[j].f_func--;
			if (bindlist[j].d_func < 255) {
				if (bindlist[j].d_func > n2)
					bindlist[j].d_func--;
				if (bindlist[j].d_func >= n1)
					bindlist[j].d_func--;
			}
		}
	}

	if (argc == 2) {
		for (; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			memcpy(&bindlist[i], &bindlist[i + 1],
				sizeof(bindtable));
		return(0);
	}
	if ((n1 = getcommand(argv[2])) < 0) return(-1);
	n2 = -1;
	j = 4;
	if (argc > 3) {
		if (argv[3][0] == ';') j = 3;
		else n2 = getcommand(argv[3]);
	}
	if (argc > j && argv[j][0] == ';' && ch >= K_F(1) && ch <= K_F(10)) {
		free(helpindex[ch - K_F(1)]);
		helpindex[ch - K_F(1)] = strdup2(argv[j] + 1);
	}

	bindlist[i].key = (short)ch;
	bindlist[i].f_func = (u_char)n1;
	bindlist[i].d_func = (n2 >= 0) ? (u_char)n2 : 255;
	return(0);
}

static VOID _printmacro(n)
int n;
{
	int i;

	if (bindlist[n].f_func <= NO_OPERATION)
		cputs2(funclist[bindlist[n].f_func].ident);
	else kanjiprintf("\"%s\"",
		macrolist[bindlist[n].f_func - NO_OPERATION - 1]);
	putch2('\t');
	if (bindlist[n].d_func <= NO_OPERATION)
		cputs2(funclist[bindlist[n].d_func].ident);
	else if (bindlist[n].d_func < 255) kanjiprintf("\"%s\"",
		macrolist[bindlist[n].d_func - NO_OPERATION - 1]);

	cputs2("\r\n");
}

static int printmacro(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, j, c, n;

	if (!comline) return(0);
	n = 1;
	if (argc <= 1)
	for (i = n = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (bindlist[i].f_func <= NO_OPERATION
		&& (bindlist[i].d_func <= NO_OPERATION
		|| bindlist[i].d_func == 255)) continue;

		cputs2("bind ");
		if (bindlist[i].key < ' ')
			cprintf2("'^%c'\t", bindlist[i].key + '@');
		else if (bindlist[i].key >= K_F(1)
		&& bindlist[i].key <= K_F(20))
			cprintf2("'F%d'\t", bindlist[i].key - K_F0);
		else {
			for (j = 0; keyidentlist[j].no; j++)
				if (bindlist[i].key == keyidentlist[j].no)
					break;
			if (keyidentlist[j].no)
				cprintf2("'%s'\t", keyidentlist[j].str);
			else cprintf2("'%c'\t", bindlist[i].key);
		}

		_printmacro(i);
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	else if ((c = getkeycode(argv[1])) >= 0)
	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (c == bindlist[i].key) {
			_printmacro(i);
			break;
		}
	}
	return(i ? n : 1);
}

static int setalias(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp, *tmp;
	int i, n;

	if (argc <= 1) {
		if (!comline) return(0);
		n = 0;
		for (i = 0; i < maxalias; i++) {
			cputs2("alias ");
			kanjiprintf("%s \"%s\"\r\n",
				aliaslist[i].alias, aliaslist[i].comm);
			if (++n >= n_line - 1) {
				n = 0;
				warning(0, HITKY_K);
			}
		}
		return(i ? n : 1);
	}

	cp = argv[1];
	if (!(tmp = gettoken(&cp, ""))) return(-1);
	free(tmp);
	for (i = 0; i < maxalias; i++)
		if (!strcmp(argv[1], aliaslist[i].alias)) break;
	if (argc == 2) {
		if (!comline) return(0);
		if (i < maxalias) kanjiprintf("\"%s\"\r\n", aliaslist[i].comm);
		return(1);
	}

	if (i < maxalias) {
		free(aliaslist[i].comm);
		free(aliaslist[i].alias);
	}
	else if (maxalias < MAXALIASTABLE) maxalias++;
	else return(-1);

	aliaslist[i].alias = strdup2(argv[1]);
	aliaslist[i].comm = strdup2(argv[2]);
	return(0);
}

static int unalias(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	reg_t *re;
	char *cp;
	int i, j, n;

	if (argc <= 1) return(-1);
	n = 0;
	cp = cnvregexp(argv[1], 0);
	re = regexp_init(cp);
	free(cp);
	for (i = 0; i < maxalias; i++)
		if (regexp_exec(re, aliaslist[i].alias)) {
			free(aliaslist[i].alias);
			free(aliaslist[i].comm);
			maxalias--;
			for (j = i; j < maxalias; j++)
				memcpy(&aliaslist[j], &aliaslist[j + 1],
					sizeof(aliastable));
			i--;
			n++;
		}
	regexp_free(re);
	return(n ? 0 : -1);
}

#if	!MSDOS && !defined (_NODOSDRIVE)
static int _setdrive(argc, argv, set)
int argc;
char *argv[];
int set;
{
	char *cp, *tmp, *name;
	int i, drive, head, sect, cyl;

	if (argc <= 1) return(-1);
	for (i = 0; fdtype[i].name; i++);
	if (i >= MAXDRIVEENTRY - 1 || argc <= 3) return(-1);

	drive = toupper2(argv[1][0]);
	name = strdup2(argv[2]);
	cp = tmp = catargs(argc - 3, &argv[3], 0);
	head = atoi(cp);
	if (head <= 0 || !(cp = strchr(cp, ','))) {
		free(tmp);
		return(-1);
	}
	sect = atoi(++cp);
	if (sect <= 0 || !(cp = strchr(cp, ','))) {
		free(tmp);
		return(-1);
	}
	cyl = atoi(++cp);
	free(tmp);
	if (cyl <= 0) return(-1);

	if (!set) {
		for (i = 0; fdtype[i].name; i++)
		if (drive == fdtype[i].drive
		&& head == fdtype[i].head
		&& sect == fdtype[i].sect
		&& cyl == fdtype[i].cyl
		&& !strcmp(name, fdtype[i].name)) break;
		if (!fdtype[i].name) return(-1);
		free(fdtype[i].name);
		for (; fdtype[i + 1].name; i++) {
			fdtype[i].drive = fdtype[i + 1].drive;
			fdtype[i].name = fdtype[i + 1].name;
			fdtype[i].head = fdtype[i + 1].head;
			fdtype[i].sect = fdtype[i + 1].sect;
			fdtype[i].cyl = fdtype[i + 1].cyl;
		}
		fdtype[i].name = NULL;
		return(0);
	}

	fdtype[i].drive = drive;
	fdtype[i].name = name;
	fdtype[i].head = head;
	fdtype[i].sect = sect;
	fdtype[i].cyl = cyl;
	return(0);
}

static int setdrive(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	return(_setdrive(argc, argv, 1));
}

static int unsetdrive(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	return(_setdrive(argc, argv, 0));
}

static int printdrive(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, n;

	if (!comline) return(0);
	n = 1;
	if (argc <= 1) for (i = n = 0; fdtype[i].name; i++) {
		cputs2("drive ");
		kanjiprintf("'%c'\t\"%s\"\t(%1d,%3d,%3d)\r\n",
			fdtype[i].drive, fdtype[i].name,
			fdtype[i].head, fdtype[i].sect, fdtype[i].cyl);
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	else for (i = 0; fdtype[i].name; i++)
	if (toupper2(argv[1][0]) == fdtype[i].drive) {
		kanjiprintf("\"%s\"\t(%1d,%3d,%3d)\r\n", fdtype[i].name,
			fdtype[i].head, fdtype[i].sect, fdtype[i].cyl);
	}
	return(i ? n : 1);
}
#endif	/* !MSDOS && !_NODOSDRIVE */

static int setuserfunc(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp, *tmp, *line, *list[MAXFUNCLINES + 1];
	int i, j, n, len;

	if (argc <= 1) {
		if (!comline) return(0);
		n = 0;
		for (i = 0; i < maxuserfunc; i++) {
			cputs2("function ");
			kanjiprintf("%s() {", userfunclist[i].func);
			for (j = 0; userfunclist[i].comm[j]; j++)
				kanjiprintf(" %s;", userfunclist[i].comm[j]);
			cputs2(" }\r\n");
			if (++n >= n_line - 1) {
				n = 0;
				warning(0, HITKY_K);
			}
		}
		return(i ? n : 1);
	}
	cp = line = catargs(argc - 1, &argv[1], ' ');

	if (!(tmp = gettoken(&cp, " ({"))) {
		free(line);
		return(-1);
	}
	if (*cp == '(') {
		if (*(++cp) == ')') cp = skipspace(cp + 1);
		else {
			free(line);
			free(tmp);
			return(-1);
		}
	}

	for (i = 0; i < maxuserfunc; i++)
		if (!strcmp(tmp, userfunclist[i].func)) break;
	if (!*cp) {
		free(line);
		free(tmp);
		if (!comline || i >= maxuserfunc) return(0);
		cputs2("function ");
		kanjiprintf("%s() {\r\n", userfunclist[i].func);
		for (j = 0; userfunclist[i].comm[j]; j++)
			kanjiprintf("\t%s;\r\n", userfunclist[i].comm[j]);
		cputs2("}\r\n");
		return(1);
	}

	if (i < maxuserfunc) {
		for (j = 0; userfunclist[i].comm[j]; j++)
			free(userfunclist[i].comm[j]);
		free(userfunclist[i].comm);
		free(userfunclist[i].func);
	}
	else if (maxuserfunc < MAXFUNCTABLE) maxuserfunc++;
	else {
		free(line);
		free(tmp);
		return(-1);
	}

	userfunclist[i].func = tmp;
	if (*cp != '{') {
		userfunclist[i].comm = (char **)malloc2(sizeof(char *) * 2);
		userfunclist[i].comm[0] = geteostr(&cp, 1);
		userfunclist[i].comm[1] = NULL;
		free(line);
		return(0);
	}

	cp = skipspace(cp + 1);
	if (tmp = strtkchr(cp, '}', 0)) *tmp = '\0';
	if (!*cp) {
		free(userfunclist[i].func);
		maxuserfunc--;
		for (; i < maxuserfunc; i++) {
			userfunclist[i].comm = userfunclist[i + 1].comm;
			userfunclist[i].func = userfunclist[i + 1].func;
		}
		free(line);
		return(0);
	}

	for (j = 0; j < MAXFUNCLINES && *cp; j++) {
		if (!(line = strtkchr(cp, ';', 0))) {
			list[j++] = strdup2(cp);
			break;
		}
		len = line - cp;
		list[j] = (char *)malloc2(len + 1);
		strncpy2(list[j], cp, len);
		cp = skipspace(line + 1);
	}
	userfunclist[i].comm = (char **)malloc2(sizeof(char *) * (j + 1));
	list[j] = NULL;
	for (; j >= 0; j--) userfunclist[i].comm[j] = list[j];

	return(0);
}

#if	!MSDOS && !defined (_NOKEYMAP)
static int setkeymap(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp, *tmp, *line;
	int i, j, k, ch;

	if (argc <= 1) return(-1);
	if (argv[1][0] == 'F' && argv[1][1] >= '1' && argv[1][1] <= '2') {
		for (i = 2; argv[1][i]; i++)
			if (argv[1][i] < '0' || argv[1][i] > '9') break;
		if (argv[1][i] || (i = atoi2(argv[1] + 1)) < 1 || i > 20)
			ch = 0;
		else ch = K_F(i);
	}
	else {
		for (i = 0; keyidentlist[i].no; i++)
			if (!strcmp(argv[1], keyidentlist[i].str)) break;
		ch = keyidentlist[i].no;
	}

	if (argc == 2) {
		if (!comline || !ch || !(cp = getkeyseq(ch))) return(0);
		cputs2("keymap ");
		if (ch >= K_F(i) && ch <= K_F(20)) cprintf2(" F%d \"", i);
		cprintf2(" %s \"", keyidentlist[i].str);
		for (i = 0; cp[i]; i++) {
			if (isprint(cp[i])) putch2(cp[i]);
			else cprintf2("\\%03o", cp[i]);
		}
		cputs2("\"\r\n");
		return(1);
	}
	if (!ch) return(-1);

	line = (char *)malloc2(strlen(argv[2]) + 1);
	for (i = j = 0; argv[2][i]; i++, j++) {
		if (argv[2][i] != '\\') line[j] = argv[2][i];
		else if (argv[2][++i] >= '0' && argv[2][i] <= '7') {
			line[j] = argv[2][i] - '0';
			for (k = 1; k < 3; k++) {
				if (argv[2][i + 1] < '0'
				|| argv[2][i + 1] > '7') break;
				line[j] = line[j] * 8 + argv[2][++i] - '0';
			}
		}
		else {
			for (k = 0; escapechar[k]; k++)
				if (argv[2][i] == escapechar[k]) break;
			if (escapechar[k]) line[j] = escapevalue[k];
			else line[j] = argv[2][i];
		}
	}
	line[j] = '\0';
	if (!j) return(-1);

	cp = strdup2(line);
	free(line);
	setkeyseq(ch, cp);

	return(0);
}

static int getkey(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int ch, next;

	if (!comline) return(0);
	kanjiputs(GETKY_K);
	tflush();

	next = 0;
	while (!kbhit2(1000000L / SENSEPERSEC));
	cputs2("\r\"");
	putterm(l_clear);
	do {
		ch = getch2();
		next = kbhit2(WAITKEYPAD * 1000L);
		if (isprint(ch)) putch2(ch);
		else cprintf2("\\%03o", ch);
	} while (next);
	cputs2("\"\r\n");
	return(1);
}
#endif	/* !MSDOS && !_NOKEYMAP */

static int exportenv(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp, *tmp, *line;
#ifndef	USESETENV
	char *env;
#endif

	if (argc <= 1) return(-1);
	tmp = line = catargs(argc - 1, &argv[1]);
	if ((cp = getenvval(&tmp)) == (char *)-1) {
		free(line);
		return(-1);
	}
#ifdef	USESETENV
	if (!cp) unsetenv(tmp);
	else {
		if (setenv(tmp, cp, 1) < 0) error(cp);
		free(cp);
	}
#else
	env = (char *)malloc2(strlen(tmp) + 1 + strlen(cp) + 1);
	strcpy(env, tmp);
	strcat(env, "=");
	if (cp) {
		strcat(env, cp);
		free(cp);
	}
	if (putenv2(env)) error(env);
#endif
	free(line);
	free(tmp);
#if	!MSDOS
	adjustpath();
#endif
	evalenv();
	return(0);
}

static int dochdir(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	if (argc <= 1) return(-1);
	chdir3(argv[1]);
	return(0);
}

static int loadsource(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	if (argc <= 1) return(-1);
	loadruncom(argv[1]);
	return(0);
}

static int printhist(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, n, no, max, size;

	if (!comline) return(0);
	size = (int)(sh_history[0]);
	no = counthistory(sh_history);
	if (argc < 2 || (max = atoi(argv[1])) > size) max = size;
	n = 0;
	for (i = 1; i <= max; i++) {
		if (!sh_history[max - i + 2]) continue;
		kanjiprintf("%5d  %s\r\n",
			no + i - max, sh_history[max - i + 2]);
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	return(i ? n : 1);
}

static int isinternal(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i;

	if (!comline) return(NO_OPERATION);
	for (i = 0; i <= NO_OPERATION; i++)
		if (!strcmp(argv[0], funclist[i].ident)) break;
	if (i > NO_OPERATION) return(-1);
	return(i);
}

int execbuiltin(command, list, maxp, comline)
char *command;
namelist *list;
int *maxp, comline;
{
	char *cp, *tmp, *argv[MAXARGS + 2];
	int i, n, argc;

	n = -2;
	command = skipspace(command);
	argc = getargs(command, argv, MAXARGS + 1);
	for (i = 0; builtinlist[i].ident; i++)
		if (!strcmp(argv[0], builtinlist[i].ident)) break;
	if (builtinlist[i].ident) {
		if (comline) {
			locate(0, n_line - 1);
			tflush();
		}
		n = (int)(*builtinlist[i].func)(argc, argv, comline);
		if (n < 0) {
			if (comline) warning(0, ILFNC_K);
			n = 2;
		}
		else {
			if (n && comline) warning(0, HITKY_K);
			n = 4;
		}
	}
	else if (argv[0][0] == '!') {
		if (comline) {
			n = dohistory(argc, argv, list, maxp);
			if (n < 0) n = 4;
			else if (n < 2) n = 2;
		}
	}
	else if (list && maxp && (i = isinternal(argc, argv, comline)) >= 0)
		n = (int)(*funclist[i].func)(list, maxp, argv[1]);
	else if (isalpha(*command) || *command == '_') {
		tmp = command;
		if ((cp = getenvval(&tmp)) == (char *)-1) free(tmp);
		else {
			if (setenv2(tmp, cp, 1) < 0) error(tmp);
			evalenv();
			if (cp) free(cp);
			n = 4;
		}
	}

	for (i = 0; i < argc; i++) free(argv[i]);
	return(n);
}
