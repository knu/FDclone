/*
 *	builtin.c
 *
 *	Builtin Command
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"
#include "funcno.h"
#include "kanji.h"

#if	!MSDOS
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
extern char **history[];
extern short histsize[];
extern short histno[];
extern char *helpindex[];

#ifndef	_NOARCHIVE
static char *getext __P_((char *));
static int setlaunch __P_((int, char *[], int));
static int setarch __P_((int, char *[], int));
static int extcmp __P_((char *, char *));
static int printlaunch __P_((int, char *[], int));
static int printarch __P_((int, char *[], int));
#endif
static int getcommand __P_((char *));
static int getkeycode __P_((char *));
static int freemacro __P_((int));
static int setkeybind __P_((int, char *[], int));
static VOID printmacro __P_((int));
static int printbind __P_((int, char *[], int));
static int setalias __P_((int, char *[], int));
static int unalias __P_((int, char *[], int));
#if	!MSDOS && !defined (_NODOSDRIVE)
static int _setdrive __P_((int, char *[], int));
static int setdrive __P_((int, char *[], int));
static int unsetdrive __P_((int, char *[], int));
static int printdrive __P_((int, char *[], int));
#endif
static int setuserfunc __P_((int, char *[], int));
#if	!MSDOS && !defined (_NOKEYMAP)
static int setkeymap __P_((int, char *[], int));
static int keytest __P_((int, char *[], int));
#endif
static int exportenv __P_((int, char *[], int));
static int dochdir __P_((int, char *[], int));
static int loadsource __P_((int, char *[], int));
static int printhist __P_((int, char *[], int));

static builtintable builtinlist[] = {
	{printenv,	"printenv"},
#ifndef	_NOARCHIVE
	{setlaunch,	"launch"},
	{setarch,	"arch"},
	{printlaunch,	"printlaunch"},
	{printarch,	"printarch"},
#endif
	{setkeybind,	"bind"},
	{printbind,	"printbind"},
	{setalias,	"alias"},
	{unalias,	"unalias"},
#if	!MSDOS && !defined (_NODOSDRIVE)
	{setdrive,	"setdrv"},
	{unsetdrive,	"unsetdrv"},
	{printdrive,	"printdrv"},
#endif
	{setuserfunc,	"function"},
#if	!MSDOS && !defined (_NOKEYMAP)
	{setkeymap,	"keymap"},
	{keytest,	"getkey"},
#endif
	{exportenv,	"export"},
	{dochdir,	"chdir"},
	{dochdir,	"cd"},
	{loadsource,	"source"},
	{printhist,	"history"},
	{NULL,		NULL}
};
static strtable keyidentlist[] = {
	{K_DOWN,	"DOWN"},
	{K_UP,		"UP"},
	{K_LEFT,	"LEFT"},
	{K_RIGHT,	"RIGHT"},
	{K_HOME,	"HOME"},
	{K_BS,		"BS"},

	{'*',		"ASTER"},
	{'+',		"PLUS"},
	{',',		"COMMA"},
	{'-',		"MINUS"},
	{'.',		"DOT"},
	{'/',		"SLASH"},
	{'0',		"TK0"},
	{'1',		"TK1"},
	{'2',		"TK2"},
	{'3',		"TK3"},
	{'4',		"TK4"},
	{'5',		"TK5"},
	{'6',		"TK6"},
	{'7',		"TK7"},
	{'8',		"TK8"},
	{'9',		"TK9"},
	{'=',		"EQUAL"},
	{CR,		"RET"},

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
	char *tmp;

	if (*ext == '*') tmp = strdup2(ext);
	else {
		tmp = malloc2(strlen(ext) + 2);
		*tmp = '*';
		strcpy(tmp + 1, ext);
	}
	return(tmp);
}

/*ARGSUSED*/
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
	if (i >= MAXLAUNCHTABLE) {
		free(ext);
		return(-1);
	}

	if (argc == 2) {
		free(ext);
		if (i >= maxlaunch) return(-1);

		free(launchlist[i].ext);
		free(launchlist[i].comm);
		maxlaunch--;
		for (; i < maxlaunch; i++)
			memcpy((char *)&(launchlist[i]),
				(char *)&(launchlist[i + 1]),
				sizeof(launchtable));
		return(0);
	}

	if (argc <= 3) launchlist[i].topskip = 255;
	else {
		cp = tmp = catargs(&(argv[3]), '\0');
		launchlist[i].topskip = atoi(cp);
		if (*(cp = skipnumeric(cp, 0)) != ',') {
			free(ext);
			free(tmp);
			return(-1);
		}
		launchlist[i].bottomskip = atoi(++cp);
		cp = skipnumeric(cp, 0);

		ch = ':';
		for (j = 0; j < MAXLAUNCHFIELD; j++) {
			if (!cp || *cp != ch) {
				free(ext);
				free(tmp);
				return(-1);
			}
			cp = getrange(++cp,
				&(launchlist[i].field[j]),
				&(launchlist[i].delim[j]),
				&(launchlist[i].width[j]));
			ch = ',';
		}

		ch = ':';
		for (j = 0; j < MAXLAUNCHSEP; j++) {
			if (*cp != ch) break;
			launchlist[i].sep[j] =
				((ch = atoi(++cp)) > 0) ? ch - 1 : 255;
			cp = skipnumeric(cp, 0);
			ch = ',';
		}
		for (; j < MAXLAUNCHSEP; j++) launchlist[i].sep[j] = -1;

		if (!cp || *cp != ':') launchlist[i].lines = 1;
		else {
			launchlist[i].lines =
				((ch = atoi(++cp)) > 0) ? ch : 1;
			cp = skipnumeric(cp, 0);
		}

		if (*cp) {
			free(ext);
			free(tmp);
			return(-1);
		}
		free(tmp);
	}

	if (i >= maxlaunch) maxlaunch++;
	else {
		free(launchlist[i].ext);
		free(launchlist[i].comm);
	}
	launchlist[i].ext = ext;
	launchlist[i].comm = strdup2(argv[2]);
	return(0);
}

/*ARGSUSED*/
static int setarch(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *ext;
	int i;

	if (argc <= 1 || argc >= 5) return(-1);
	ext = getext(argv[1]);
	for (i = 0; i < maxarchive; i++)
		if (!strpathcmp(archivelist[i].ext, ext)) break;
	if (i >= MAXARCHIVETABLE) {
		free(ext);
		return(-1);
	}

	if (argc == 2) {
		free(ext);
		if (i >= maxarchive) return(-1);

		free(archivelist[i].ext);
		free(archivelist[i].p_comm);
		if (archivelist[i].u_comm) free(archivelist[i].u_comm);
		maxarchive--;
		for (; i < maxarchive; i++)
			memcpy((char *)&(archivelist[i]),
				(char *)&(archivelist[i + 1]),
				sizeof(archivetable));
		return(0);
	}

	if (i >= maxarchive) maxarchive++;
	else {
		free(archivelist[i].ext);
		free(archivelist[i].p_comm);
		if (archivelist[i].u_comm) free(archivelist[i].u_comm);
	}
	archivelist[i].ext = ext;
	archivelist[i].p_comm = strdup2(argv[2]);
	archivelist[i].u_comm = (argc > 3) ? strdup2(argv[3]) : NULL;
	return(0);
}

static int extcmp(s, ext)
char *s, *ext;
{
	if (*s != '*' && *ext == '*') {
		ext++;
		if (*s != '.' && *ext == '.') ext++;
	}
	return(strpathcmp(s, ext));
}

/*ARGSUSED*/
static int printlaunch(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, j, ch, n;

	if (argc >= 3) return(-1);
	if (!comline) return(0);
	n = 1;
	if (argc <= 1) for (i = n = 0; i < maxlaunch; i++) {
		cputs2("launch ");
		cputs2(launchlist[i].ext);
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
				else cprintf2("%d",
					launchlist[i].field[j] + 1);
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

/*ARGSUSED*/
static int printarch(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, n;

	if (argc >= 3) return(-1);
	if (!comline) return(0);
	n = 1;
	if (argc <= 1) for (i = n = 0; i < maxarchive; i++) {
		cputs2("arch ");
		cputs2(archivelist[i].ext);
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
	int n;

	for (n = 0; n < NO_OPERATION; n++)
		if (!strpathcmp(cp, funclist[n].ident)) break;
	if (n < NO_OPERATION);
	else if (maxmacro >= MAXMACROTABLE) n = -1;
	else {
		macrolist[maxmacro] = strdup2(cp);
		return(NO_OPERATION + (++maxmacro));
	}
	return(n);
}

static int getkeycode(cp)
char *cp;
{
	int i, j, ch;

	ch = *(cp++);
	if (*cp) switch (ch) {
		case '\\':
			if (*cp >= '0' && *cp <= '7') {
				ch = *(cp++) - '0';
				for (j = 1; j < 3; j++) {
					if (*cp < '0' || *cp > '7') break;
					ch = ch * 8 + *(cp++) - '0';
				}
			}
			else {
				for (j = 0; escapechar[j]; j++)
					if (*cp == escapechar[j]) break;
				if (escapechar[j]) ch = escapevalue[j];
				else ch = *cp;
				cp++;
			}
			break;
		case '^':
			ch = toupper2(*(cp++));
			if (ch < '?' || ch > '_') ch = -1;
			else ch = ((ch - '@') & 0x7f);
			break;
		case '@':
			if (*cp >= 'a' && *cp <= 'z') ch = (*(cp++) | 0x80);
			else if (*cp >= 'A' && *cp <= 'Z')
#if	MSDOS
				ch = ((*(cp++) + 'a' - 'A') | 0x80);
#else
				ch = (*(cp++) | 0x80);
#endif
			else ch = -1;
			break;
		case 'F':
			if ((i = atoi2(cp)) >= 1 && i <= 20) {
				cp = skipnumeric(cp, 1);
				ch = K_F(i);
				break;
			}
		default:
			cp--;
			for (i = 0; keyidentlist[i].no; i++)
				if (!strcmp(keyidentlist[i].str, cp)) break;
			if (!(ch = keyidentlist[i].no)) ch = -1;
			else cp += strlen(cp);
			break;
	}
	if (*cp) ch = -1;
	return(ch);
}

static int freemacro(n)
int n;
{
	int i;

	if (n <= NO_OPERATION || n >= 255) return(-1);

	free(macrolist[n - NO_OPERATION - 1]);
	maxmacro--;
	for (i = n - NO_OPERATION - 1; i < maxmacro; i++)
		macrolist[i] = macrolist[i + 1];

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (bindlist[i].f_func > n) bindlist[i].f_func--;
		if (bindlist[i].d_func > n && bindlist[i].d_func < 255)
			bindlist[i].d_func--;
	}

	return(0);
}

/*ARGSUSED*/
static int setkeybind(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, j, ch, n1, n2;

	if (argc <= 1 || argc >= 6) return(-1);
	if ((ch = getkeycode(argv[1])) < 0 || ch == '\033') return(-1);

	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
		if (ch == (int)(bindlist[i].key)) break;
	if (i >= MAXBINDTABLE - 1) return(-1);

	if (argc == 2) {
		if (bindlist[i].key < 0) return(-1);

		freemacro(bindlist[i].f_func);
		freemacro(bindlist[i].d_func);
		for (; i < MAXBINDTABLE && bindlist[i].key >= 0; i++)
			memcpy((char *)&(bindlist[i]),
				(char *)&(bindlist[i + 1]),
				sizeof(bindtable));
		return(0);
	}

	if ((n1 = getcommand(argv[2])) < 0) return(-1);

	n2 = 255;
	j = 4;
	if (argc > 3) {
		if (argv[3][0] == ';') j = 3;
		else if ((n2 = getcommand(argv[3])) < 0) {
			freemacro(n1);
			return(-1);
		}
	}
	if (argc > j) {
		if (argv[j][0] != ';' || ch < K_F(1) || ch > K_F(10)) {
			freemacro(n1);
			freemacro(n2);
			return(-1);
		}
		free(helpindex[ch - K_F(1)]);
		helpindex[ch - K_F(1)] = strdup2(&(argv[j][1]));
	}

	if (bindlist[i].key < 0) {
		memcpy((char *)&(bindlist[i + 1]), (char *)&(bindlist[i]),
			sizeof(bindtable));
		bindlist[i].f_func = (u_char)n1;
		bindlist[i].d_func = (u_char)n2;
	}
	else {
		j = bindlist[i].f_func;
		bindlist[i].f_func = (u_char)n1;
		freemacro(j);
		j = bindlist[i].d_func;
		bindlist[i].d_func = (u_char)n2;
		freemacro(j);
	}
	bindlist[i].key = (short)ch;
	return(0);
}

static VOID printmacro(n)
int n;
{
	if (bindlist[n].f_func <= NO_OPERATION)
		cputs2(funclist[bindlist[n].f_func].ident);
	else kanjiprintf("\"%s\"",
		macrolist[bindlist[n].f_func - NO_OPERATION - 1]);
	if (bindlist[n].d_func <= NO_OPERATION) {
		cputs2("\t");
		cputs2(funclist[bindlist[n].d_func].ident);
	}
	else if (bindlist[n].d_func < 255) kanjiprintf("\t\"%s\"",
		macrolist[bindlist[n].d_func - NO_OPERATION - 1]);

	cputs2("\r\n");
}

char *getkeysym(c)
int c;
{
	static char buf[5];
	int i, j;

	i = 0;
	if (c < ' ' || c == C_DEL) {
		buf[i++] = '^';
		buf[i++] = (c + '@') & 0x7f;
	}
	else if (c >= K_F(1) && c <= K_F(20)) {
		c -= K_F0;
		buf[i++] = 'F';
		if (c > 10) buf[i++] = (c / 10) + '0';
		buf[i++] = (c % 10) + '0';
	}
	else if ((c & ~0x7f) == 0x80
	&& (((c & 0x7f) >= 'a' && (c & 0x7f) <= 'z')
	|| ((c & 0x7f) >= 'A' && (c & 0x7f) <= 'Z'))) {
		buf[i++] = '@';
		buf[i++] = c & 0x7f;
	}
	else {
		for (j = 0; keyidentlist[j].no; j++)
			if ((u_short)(c) == keyidentlist[j].no) break;
		if (keyidentlist[j].no >= K_MIN) return(keyidentlist[j].str);
		else if (isprint(c)) buf[i++] = c;
		else {
			buf[i++] = '\\';
			buf[i++] = (c / (8 * 8)) + '0';
			buf[i++] = ((c % (8 * 8)) / 8) + '0';
			buf[i++] = (c % 8) + '0';
		}
	}
	buf[i] = '\0';
	return(buf);
}

/*ARGSUSED*/
static int printbind(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, c, n;

	if (argc >= 3) return(-1);
	if (!comline) return(0);
	n = 1;
	if (argc <= 1)
	for (i = n = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (bindlist[i].f_func <= NO_OPERATION
		&& (bindlist[i].d_func <= NO_OPERATION
		|| bindlist[i].d_func == 255)) continue;

		cputs2("bind '");
		cputs2(getkeysym(bindlist[i].key));
		cputs2("'\t");
		printmacro(i);
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	else if ((c = getkeycode(argv[1])) >= 0)
	for (i = 0; i < MAXBINDTABLE && bindlist[i].key >= 0; i++) {
		if (c == bindlist[i].key) {
			printmacro(i);
			break;
		}
	}
	else i = 0;
	return(i ? n : 1);
}

/*ARGSUSED*/
static int setalias(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp, *tmp;
	int i, n;

	if (argc >= 4) return(-1);
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
	if (*cp) return(-1);
	for (i = 0; i < maxalias; i++)
		if (!strpathcmp(argv[1], aliaslist[i].alias)) break;

	if (argc == 2) {
		if (!comline) return(0);
		if (i < maxalias) kanjiprintf("\"%s\"\r\n", aliaslist[i].comm);
		return(1);
	}

	if (i >= MAXALIASTABLE) return(-1);
	if (i >= maxalias) maxalias++;
	else {
		free(aliaslist[i].comm);
		free(aliaslist[i].alias);
	}
	aliaslist[i].alias = strdup2(argv[1]);
	aliaslist[i].comm = strdup2(argv[2]);
#if	MSDOS
	for (cp = aliaslist[i].alias; cp && *cp; cp++)
		if (*cp >= 'A' && *cp <= 'Z') *cp += 'a' - 'A';
#endif
	return(0);
}

/*ARGSUSED*/
static int unalias(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	reg_t *re;
	int i, j, n;

	if (argc <= 1 || argc >= 4) return(-1);
	n = 0;
	re = regexp_init(argv[1], -1);
	for (i = 0; i < maxalias; i++)
		if (regexp_exec(re, aliaslist[i].alias, 0)) {
			free(aliaslist[i].alias);
			free(aliaslist[i].comm);
			maxalias--;
			for (j = i; j < maxalias; j++)
				memcpy((char *)&(aliaslist[j]),
					(char *)&(aliaslist[j + 1]),
					sizeof(aliastable));
			i--;
			n++;
		}
	regexp_free(re);
	return(n ? 0 : -1);
}

#if	!MSDOS && !defined (_NODOSDRIVE)
/*ARGSUSED*/
static int _setdrive(argc, argv, set)
int argc;
char *argv[];
int set;
{
	char *cp, *tmp;
	int i, j, n, drive, head, sect, cyl;

	if (argc <= 3) return(-1);
	if (!isalpha(drive = toupper2(argv[1][0]))) return(-1);

#ifdef	HDDMOUNT
	if (!strncmp(argv[3], "HDD", 3)) {
		if (argc > 5) return(-1);
		cyl = 0;
		if (!argv[3][3]) sect = 0;
		else if (!strcmp(&(argv[3][3]), "98")) sect = 98;
		else return(-1);
		if (argc < 5) head = 'n';
		else if (!strcmp(argv[4], "ro")) head = 'r';
		else if (!strcmp(argv[4], "rw")) head = 'w';
		else return(-1);
	}
	else
#endif
	{
		cp = tmp = catargs(&(argv[3]), '\0');

		head = atoi(cp);
		if (head <= 0 || *(cp = skipnumeric(cp, 1)) != ',') {
			free(tmp);
			return(-1);
		}
		sect = atoi(++cp);
		if (sect <= 0 || *(cp = skipnumeric(cp, 1)) != ',') {
			free(tmp);
			return(-1);
		}
		cyl = atoi(++cp);
		if (cyl <= 0 || *(cp = skipnumeric(cp, 0))) {
			free(tmp);
			return(-1);
		}
		free(tmp);
	}

#ifdef	HDDMOUNT
	if (!cyl) for (i = 0; fdtype[i].name; i++) {
		if (drive == fdtype[i].drive) break;
	}
	else
#endif
	for (i = 0; fdtype[i].name; i++)
		if (drive == fdtype[i].drive
		&& head == fdtype[i].head
		&& sect == fdtype[i].sect
		&& cyl == fdtype[i].cyl
		&& !strpathcmp(argv[2], fdtype[i].name)) break;
	if (!set) {
		if (!fdtype[i].name) return(-1);
#ifdef	HDDMOUNT
		if (!cyl
		&& (fdtype[i].cyl || strpathcmp(argv[2], fdtype[i].name)))
			return(-1);
#endif

		free(fdtype[i].name);
		for (; fdtype[i + 1].name; i++)
			memcpy((char *)&(fdtype[i]),
				(char *)&(fdtype[i + 1]), sizeof(devinfo));
		fdtype[i].name = NULL;
	}
	else {
#ifdef	HDDMOUNT
		off64_t *sp = NULL;
		int c;

		if (!cyl) {
			if (!(sp = readpt(argv[2], sect))) return(0);
			for (n = 0; sp[n + 1]; n++);
			if (!n) {
				free(sp);
				return(0);
			}
			sect = sp[0];
		}
		else
#endif
		n = 1;
		if (i >= MAXDRIVEENTRY - n || fdtype[i].name) {
#ifdef	HDDMOUNT
			if (sp) free(sp);
#endif
			return(-1);
		}
		fdtype[i + n].name = NULL;
		for (; i > 0; i--) {
			if (!strcmp(argv[2], fdtype[i - 1].name)) break;
			memcpy((char *)&(fdtype[i + n - 1]),
				(char *)&(fdtype[i - 1]), sizeof(devinfo));
		}

		for (j = 0; j < n; j++) {
			fdtype[i + j].drive = drive;
			fdtype[i + j].name = strdup2(argv[2]);
			fdtype[i + j].head = head;
			fdtype[i + j].sect = sect;
			fdtype[i + j].cyl = cyl;
#ifdef	HDDMOUNT
			fdtype[i + j].offset = (cyl) ? (off64_t)0 : sp[j + 1];
			if (j + 1 < n) do {
				if (++drive > 'Z') {
					while (j >= 0)
						free(fdtype[i + (j--)].name);
					for (; fdtype[i + n].name; i++)
						memcpy((char *)&(fdtype[i]),
						(char *)&(fdtype[i + n]),
							sizeof(devinfo));
					fdtype[i].name = NULL;
					if (sp) free(sp);
					return(-1);
				}
				for (c = 0; fdtype[c].name; c++)
					if (drive == fdtype[c].drive) break;
			} while (fdtype[c].name);
#endif
		}
#ifdef	HDDMOUNT
		if (sp) free(sp);
#endif
	}

	return(0);
}

/*ARGSUSED*/
static int setdrive(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	return(_setdrive(argc, argv, 1));
}

/*ARGSUSED*/
static int unsetdrive(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	return(_setdrive(argc, argv, 0));
}

/*ARGSUSED*/
static int printdrive(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
#ifdef	HDDMOUNT
	char buf[14 + 1];
#endif
	int i, n;

	if (argc >= 3) return(-1);
	if (!comline) return(0);
	n = 1;
	if (argc <= 1) for (i = n = 0; fdtype[i].name; i++) {
		cputs2("setdrv ");
		kanjiprintf("'%c'\t\"%s\"\t",
			fdtype[i].drive, fdtype[i].name);
#ifdef	HDDMOUNT
		if (!fdtype[i].cyl)
			kanjiprintf("HDD #offset=%-14.14s\r\n",
				inscomma(buf,
					fdtype[i].offset / fdtype[i].sect,
					3, 14));
		else
#endif
		kanjiprintf("%3d,%3d,%3d\r\n",
			fdtype[i].head, fdtype[i].sect, fdtype[i].cyl);
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	else for (i = 0; fdtype[i].name; i++)
	if (toupper2(argv[1][0]) == fdtype[i].drive) {
		kanjiprintf("\"%s\"\t", fdtype[i].name);
#ifdef	HDDMOUNT
		if (!fdtype[i].cyl)
			kanjiprintf("HDD #offset=%-14.14s\r\n",
				inscomma(buf,
					fdtype[i].offset / fdtype[i].sect,
					3, 14));
		else
#endif
		kanjiprintf("%3d,%3d,%3d\r\n",
			fdtype[i].head, fdtype[i].sect, fdtype[i].cyl);
	}
	return(i ? n : 1);
}
#endif	/* !MSDOS && !_NODOSDRIVE */

/*ARGSUSED*/
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

	cp = line = catargs(&(argv[1]), ' ');
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
		if (!strpathcmp(tmp, userfunclist[i].func)) break;
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

	if (i >= MAXFUNCTABLE) {
		free(line);
		free(tmp);
		return(-1);
	}

	userfunclist[i].func = tmp;
	if (*cp != '{') {
		free(line);
		free(tmp);
		return(-1);
	}
	cp = skipspace(cp + 1);
	if ((tmp = strtkchr(cp, '}', 0))) {
		*tmp = '\0';
		if (*(++tmp)) {
			free(line);
			free(userfunclist[i].func);
			return(-1);
		}
	}
	if (!*cp) {
		free(userfunclist[i].func);
		if (i >= maxuserfunc) return(-1);

		for (j = 0; userfunclist[i].comm[j]; j++)
			free(userfunclist[i].comm[j]);
		free(userfunclist[i].comm);
		maxuserfunc--;
		for (; i < maxuserfunc; i++)
			memcpy((char *)&(userfunclist[i]),
				(char *)&(userfunclist[i + 1]),
				sizeof(userfunctable));
		return(0);
	}

	for (j = 0; j < MAXFUNCLINES && *cp; j++) {
		if (!(tmp = strtkchr(cp, ';', 0))) {
			list[j++] = strdup2(cp);
			break;
		}
		len = tmp - cp;
		list[j] = malloc2(len + 1);
		strncpy2(list[j], cp, len);
		cp = skipspace(tmp + 1);
	}

	if (i >= maxuserfunc) maxuserfunc++;
	else {
		free(userfunclist[i].func);
		for (j = 0; userfunclist[i].comm[j]; j++)
			free(userfunclist[i].comm[j]);
		free(userfunclist[i].comm);
	}
	userfunclist[i].comm = (char **)malloc2(sizeof(char *) * (j + 1));
	list[j] = NULL;
	for (; j >= 0; j--) userfunclist[i].comm[j] = list[j];
#if	MSDOS
	for (cp = userfunclist[i].func; cp && *cp; cp++)
		if (*cp >= 'A' && *cp <= 'Z') *cp += 'a' - 'A';
#endif
	free(line);
	return(0);
}

#if	!MSDOS && !defined (_NOKEYMAP)
/*ARGSUSED*/
static int setkeymap(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp, *line;
	int i, j, k, ch, l;

	if (argc >= 4) return(-1);
	if (argc <= 1) {
		if (!comline) return(0);
		k = 0;
		for (i = 0; i < 20 || keyidentlist[i - 20].no; i++) {
			ch = (i < 20) ? K_F(i + 1) : keyidentlist[i - 20].no;
			if (!(cp = getkeyseq(ch, &l)) || !l) continue;

			cputs2("keymap ");
			if (i < 20) cprintf2("F%d \"", i + 1);
			else cprintf2("%s \"", keyidentlist[i - 20].str);
			for (j = 0; j < l; j++) {
				if (isprint(cp[j])) putch2(cp[j]);
				else cprintf2("\\%03o", cp[j]);
			}
			cputs2("\"\r\n");
			if (++k >= n_line - 1) {
				k = 0;
				warning(0, HITKY_K);
			}
		}
		return(1);
	}
	if (argv[1][0] == 'F' && argv[1][1] >= '1' && argv[1][1] <= '9') {
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
		if (!comline || !ch || !(cp = getkeyseq(ch, &l)) || !l)
			return(0);
		cputs2("keymap ");
		if (ch >= K_F(i) && ch <= K_F(20)) cprintf2("F%d \"", i);
		else cprintf2("%s \"", keyidentlist[i].str);
		for (i = 0; i < l; i++) {
			if (isprint(cp[i])) putch2(cp[i]);
			else cprintf2("\\%03o", cp[i]);
		}
		cputs2("\"\r\n");
		return(1);
	}
	if (!ch) return(-1);
	if (!argv[2][0]) {
		setkeyseq(ch, NULL, 0);
		return(0);
	}

	line = malloc2(strlen(argv[2]) + 1);
	for (i = j = 0; argv[2][i]; i++, j++) {
		if (argv[2][i] == '^'
		&& (k = toupper2(argv[2][i + 1])) >= '?' && k <= '_') {
			i++;
			line[j] = ((toupper2(k) - '@') & 0x7f);
		}
		else if (argv[2][i] != '\\') line[j] = argv[2][i];
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
	if (!j) return(-1);

	cp = malloc2(j + 1);
	for (i = 0; i < j; i++) cp[i] = line[i];
	cp[i] = '\0';
	free(line);
	setkeyseq(ch, cp, j);

	return(0);
}

/*ARGSUSED*/
static int keytest(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, n, ch, next;

	if (argc >= 3) return(-1);
	if (!comline) return(0);

	n = (argc >= 2) ? atoi2(argv[1]) : 1;
	if (n < 0) return(-1);

	i = 0;
	for (;;) {
		kanjiputs(GETKY_K);
		if (n != 1) kanjiputs(SPCED_K);
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
		if (n) {
			if (++i >= n) break;
		}
		if (n != 1 && ch == ' ') break;
	}
	return(1);
}
#endif	/* !MSDOS && !_NOKEYMAP */

/*ARGSUSED*/
static int exportenv(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	char *cp, *env;
	int i, len;

	i = argc - 1;
	if ((cp = getenvval(&i, &(argv[1]))) == (char *)-1 || i + 1 < argc)
		return(-1);
	if (!cp) env = argv[1];
	else {
		len = strlen(argv[1]);
		env = malloc2(len + strlen(cp) + 2);
		memcpy(env, argv[1], len);
		env[len++] = '=';
		strcpy(&(env[len]), cp);
		free(cp);
	}
	if (putenv2(env) < 0) error(argv[1]);
#if	!MSDOS
	adjustpath();
#endif
	evalenv();
	return(0);
}

/*ARGSUSED*/
static int dochdir(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	if (argc != 2) return(-1);
	chdir3(argv[1]);
	return(0);
}

/*ARGSUSED*/
static int loadsource(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	if (argc != 2) return(-1);
	return(loadruncom(argv[1], 1));
}

/*ARGSUSED*/
static int printhist(argc, argv, comline)
int argc;
char *argv[];
int comline;
{
	int i, n, no, max, size;

	if (argc >= 3) return(-1);
	if (!comline) return(0);
	size = histsize[0];
	no = histno[0];
	if (argc < 2 || (max = atoi2(argv[1])) > size) max = size;
	if (max <= 0) return(-1);

	n = 0;
	for (i = 1; i <= max; i++) {
		if (!history[0][max - i]) continue;
		kanjiprintf("%5d  %s\r\n",
			no + i - max, history[0][max - i]);
		if (++n >= n_line - 1) {
			n = 0;
			warning(0, HITKY_K);
		}
	}
	return(i ? n : 1);
}

int isinternal(comname, comline)
char *comname;
int comline;
{
	int i;

	if (!comline) return(NO_OPERATION);
	for (i = 0; i <= NO_OPERATION; i++)
		if (!strpathcmp(comname, funclist[i].ident)) break;
	if (i > NO_OPERATION) return(-1);
	return(i);
}

int execbuiltin(command, list, maxp, comline)
char *command;
namelist *list;
int *maxp, comline;
{
	char *cp, *comname, **argv;
	int i, n, argc;

	n = -2;
	command = skipspace(command);
	if (!(argc = getargs(command, &argv))) {
		freeargs(argv);
		return(-2);
	}
	comname = argv[0];
	for (i = 0; builtinlist[i].ident; i++)
		if (!strpathcmp(comname, builtinlist[i].ident)) break;
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
			n = dohistory(argv, list, maxp);
			if (n < 0) n = 4;
			else if (n < 2) n = 2;
		}
	}
	else if (list && maxp && (i = isinternal(comname, comline)) >= 0) {
		if (argc <= 2)
			n = (int)(*funclist[i].func)(list, maxp, argv[1]);
		else {
			if (comline) warning(0, ILFNC_K);
			n = 2;
		}
	}
	else if (isalpha(*command) || *command == '_') {
		i = argc;
		if ((cp = getenvval(&i, argv)) != (char *)-1 && i == argc) {
			if (setenv2(argv[0], cp) < 0) error(argv[0]);
			evalenv();
			if (cp) free(cp);
			n = 4;
		}
	}

	freeargs(argv);
	return(n);
}

#ifndef	_NOCOMPLETE
int completebuiltin(com, argc, argvp)
char *com;
int argc;
char ***argvp;
{
	int i, len;

	if (strdelim(com, 1)) return(argc);
	len = strlen(com);
	for (i = 0; builtinlist[i].ident; i++) {
		if (strnpathcmp(com, builtinlist[i].ident, len)
		|| finddupl(builtinlist[i].ident, argc, *argvp)) continue;
		*argvp = (char **)realloc2(*argvp,
			(argc + 1) * sizeof(char *));
		(*argvp)[argc++] = strdup2(builtinlist[i].ident);
	}
	return(argc);
}
#endif

#ifdef	DEBUG
VOID freedefine(VOID_A)
{
	int i, j;

#ifndef	_NOARCHIVE
	for (i = 0; i < maxlaunch; i++) {
		free(launchlist[i].ext);
		free(launchlist[i].comm);
	}
	for (i = 0; i < maxarchive; i++) {
		free(archivelist[i].ext);
		free(archivelist[i].p_comm);
		free(archivelist[i].u_comm);
	}
#endif	/* !_NOARCHIVE */
	for (i = 0; i < maxmacro; i++) free(macrolist[i]);
	for (i = 0; i < maxalias; i++) {
		free(aliaslist[i].alias);
		free(aliaslist[i].comm);
	}
	for (i = 0; i < maxuserfunc; i++) {
		free(userfunclist[i].func);
		for (j = 0; userfunclist[i].comm[j]; j++)
			free(userfunclist[i].comm[j]);
		free(userfunclist[i].comm);
	}
#if	!MSDOS && !defined (_NODOSDRIVE)
	for (i = 0; fdtype[i].name; i++) free(fdtype[i].name);
#endif
	for (i = 0; i < 10; i++) free(helpindex[i]);
}
#endif
