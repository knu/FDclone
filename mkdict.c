/*
 *	mkdict.c
 *
 *	Kanji translation table converter
 */

#include "headers.h"
#include "kctype.h"
#include "typesize.h"
#include "roman.h"
#include "hinsi.h"

#define	MAXLINESTR		1023
#define	DICTBUFUNIT		32
#define	MAXHINSI		16
#define	MAXBIAS			4
#define	MAXHINSILEN		16
#define	MAXFUZOKULEN		4
#define	MAXRANGE		50
#define	VERBOSECHAR		'.'
#define	J_DAKUTEN		0x212b
#define	J_U			0x2426
#define	J_VU			0x2574
#define	DIC_WNN			1
#define	DIC_CANNA		2
#define	DIC_SJ3			4
#define	DIC_WNNSJ3		(DIC_WNN | DIC_SJ3)
#define	getword(s, n)		(((u_short)((s)[(n) + 1]) << 8) | (s)[n])

typedef struct _dicttable {
	ALLOC_T ptr;
	ALLOC_T klen;
	ALLOC_T size;
	int max;
} dicttable;

typedef struct _biastable {
	u_short id;
	u_short klen;
	u_short kbuf[MAXBIAS];
} biastable;

typedef struct _hinsitable {
	u_short id;
	u_char dict;
	u_char klen;
	u_short kbuf[MAXHINSILEN];
} hinsitable;

typedef struct _hinsisettable {
	u_short id;
	u_short klen;
	u_short set[MAXHINSI];
} hinsisettable;

typedef struct _contable {
	u_short id;
	u_short klen;
	u_short kbuf[MAXFUZOKULEN];
	u_short nlen;
	u_short next[SH_MAX];
} contable;

static u_char *NEAR realloc2 __P_((VOID_P, ALLOC_T));
static int NEAR addstrbuf __P_((ALLOC_T));
static int NEAR adddictlist __P_((ALLOC_T, ALLOC_T, ALLOC_T));
static int NEAR setkbuf __P_((int, CONST u_short *));
static int NEAR setjisbuf __P_((CONST char *, int, int));
static int NEAR setword __P_((u_int));
static int NEAR sethinsi __P_((int, CONST u_short *));
static int cmphinsi __P_((CONST VOID_P, CONST VOID_P));
static int NEAR addhinsi __P_((int, u_short *, int, CONST char *));
static int NEAR _gethinsi __P_((int, u_short *, CONST char *, int *));
static int NEAR gethinsi __P_((int, u_short *, char *, int *));
static int cmpidlist __P_((CONST VOID_P, CONST VOID_P));
static int NEAR adddict __P_((CONST char *, CONST char *, char *, u_int));
static u_int NEAR getfreq __P_((char *));
static int NEAR addcannadict __P_((CONST char *, char *));
static int NEAR convdict __P_((off_t, FILE *));
static int NEAR addhinsidict __P_((VOID_A));
static int NEAR addconlist __P_((int, int, CONST contable *));
static int NEAR genconlist __P_((VOID_A));
static int NEAR cmpjisbuf __P_((CONST u_char *, int, CONST u_char *, int));
static int cmpdict __P_((CONST VOID_P, CONST VOID_P));
static int NEAR fputbyte __P_((int, FILE *));
static int NEAR fputword __P_((u_int, FILE *));
static int NEAR fputdword __P_((long, FILE *));
static int NEAR writeindex __P_((FILE *));
static int NEAR writehinsiindex __P_((FILE *));
static int NEAR writedict __P_((FILE *));
int main __P_((int, char *CONST []));

static dicttable *dictlist = NULL;
static long maxdict = 0L;
static long dictlistsize = 0L;
static u_char *strbuf = NULL;
static ALLOC_T maxstr = (ALLOC_T)0;
static ALLOC_T strbufsize = (ALLOC_T)0;
static int verbose = 0;
static int needhinsi = 0;
static CONST biastable biaslist[] = {
	{HN_KA_IKU, 1, {0x2443}},		/* tsu */
	{HN_KO_KO, 1, {0x2424}},		/* i */
	{HN_KU_KU, 2, {0x246b, 0x246c}},	/* ru, re */
	{HN_SU_SU, 1, {0x246b}},		/* ru */
};
#define	BIASLISTSIZ		arraysize(biaslist)
static CONST hinsisettable hinsisetlist[] = {
	{HN_JINCHI, 2, {HN_JINMEI, HN_CHIMEI}},
	{HN_KA5DAN, 2, {HN_KA5DOU, HN_KA5YOU}},
	{HN_GA5DAN, 2, {HN_GA5DOU, HN_GA5YOU}},
	{HN_SA5DAN, 2, {HN_SA5DOU, HN_SA5YOU}},
	{HN_TA5DAN, 2, {HN_TA5DOU, HN_TA5YOU}},
	{HN_NA5DAN, 2, {HN_NA5DOU, HN_NA5YOU}},
	{HN_BA5DAN, 2, {HN_BA5DOU, HN_BA5YOU}},
	{HN_MA5DAN, 2, {HN_MA5DOU, HN_MA5YOU}},
	{HN_RA5DAN, 2, {HN_RA5DOU, HN_RA5YOU}},
	{HN_WA5DAN, 2, {HN_WA5DOU, HN_WA5YOU}},
	{HN_1DAN, 2, {HN_1DOU, HN_1YOU}},
	{HN_1_MEI, 3, {HN_1DOU, HN_1YOU, HN_MEISHI}},
	{HN_SAHEN, 2, {HN_SAHDOU, HN_SAHYOU}},
	{HN_ZAHEN, 2, {HN_ZAHDOU, HN_ZAHDOU}},
	{HN_KD_MEI, 2, {HN_KEIDOU, HN_MEISHI}},
};
#define	HINSISETLISTSIZ		arraysize(hinsisetlist)
static hinsitable hinsilist[] = {
	{HN_SENTOU, DIC_WNN,		/* Sentou */
	 2, {0x4068, 0x462c}},

	{HN_SUUJI, DIC_WNN,		/* Suuji */
	 2, {0x3f74, 0x3b7a}},

	{HN_KANA, DIC_WNN,		/* Kana */
	 2, {0x252b, 0x254a}},

	{HN_EISUU, DIC_WNN,		/* EiSuu */
	 2, {0x3151, 0x3f74}},

	{HN_KIGOU, DIC_WNN,		/* Kigou */
	 2, {0x352d, 0x3966}},

	{HN_HEIKAKKO, DIC_WNN,		/* HeiKakko */
	 3, {0x4a44, 0x3367, 0x384c}},

	{HN_FUZOKUGO, DIC_WNN,		/* Fuzokugo */
	 3, {0x4955, 0x4230, 0x386c}},

	{HN_KAIKAKKO, DIC_WNN,		/* KaiKakko */
	 3, {0x332b, 0x3367, 0x384c}},

	{HN_GIJI, DIC_WNN,		/* Giji */
	 2, {0x353f, 0x3b77}},

	{HN_MEISHI, DIC_WNN,		/* Meishi */
	 2, {0x4c3e, 0x3b6c}},
	{HN_MEISHI, DIC_CANNA,		/* #KN */
	 3, {'#', 'K', 'N'}},
	{HN_MEISHI, DIC_CANNA,		/* #T25 */
	 4, {'#', 'T', '2', '5'}},
	{HN_MEISHI, DIC_CANNA,		/* #T33 */
	 4, {'#', 'T', '3', '3'}},
	{HN_MEISHI, DIC_CANNA,		/* #T35 */
	 4, {'#', 'T', '3', '5'}},
	{HN_MEISHI, DIC_CANNA,		/* #OKX */
	 4, {'#', 'O', 'K', 'X'}},
	{HN_MEISHI, DIC_SJ3,		/* Ikkatsu */
	 2, {0x306c, 0x3367}},
	{HN_MEISHI, DIC_SJ3,		/* Dai1 */
	 2, {0x4265, 0x2331}},
	{HN_MEISHI, DIC_SJ3,		/* Dai2 */
	 2, {0x4265, 0x2332}},
	{HN_MEISHI, DIC_SJ3,		/* Dai3 */
	 2, {0x4265, 0x2333}},
	{HN_MEISHI, DIC_SJ3,		/* Dai4 */
	 2, {0x4265, 0x2334}},
	{HN_MEISHI, DIC_SJ3,		/* Dai5 */
	 2, {0x4265, 0x2335}},
	{HN_MEISHI, DIC_SJ3,		/* Dai6 */
	 2, {0x4265, 0x2336}},
	{HN_MEISHI, DIC_SJ3,		/* Mei1 */
	 2, {0x4c3e, 0x2331}},
	{HN_MEISHI, DIC_SJ3,		/* Mei2 */
	 2, {0x4c3e, 0x2332}},
	{HN_MEISHI, DIC_SJ3,		/* Mei3 */
	 2, {0x4c3e, 0x2333}},
	{HN_MEISHI, DIC_SJ3,		/* Mei4 */
	 2, {0x4c3e, 0x2334}},
	{HN_MEISHI, DIC_SJ3,		/* Mei5 */
	 2, {0x4c3e, 0x2335}},
	{HN_MEISHI, DIC_SJ3,		/* Mei6 */
	 2, {0x4c3e, 0x2336}},
	{HN_MEISHI, DIC_SJ3,		/* Mei7 */
	 2, {0x4c3e, 0x2337}},
	{HN_MEISHI, DIC_SJ3,		/* Mei8 */
	 2, {0x4c3e, 0x2339}},
	{HN_MEISHI, DIC_SJ3,		/* Mei10 */
	 3, {0x4c3e, 0x2331, 0x2330}},
	{HN_MEISHI, DIC_SJ3,		/* Mei11 */
	 3, {0x4c3e, 0x2331, 0x2331}},
	{HN_MEISHI, DIC_SJ3,		/* Mei20 */
	 3, {0x4c3e, 0x2332, 0x2330}},

	{HN_JINMEI, DIC_WNN,		/* Jinmei */
	 2, {0x3f4d, 0x4c3e}},
	{HN_JINMEI, DIC_CANNA,		/* #JN */
	 3, {'#', 'J', 'N'}},
	{HN_JINMEI, DIC_CANNA,		/* #JNM */
	 4, {'#', 'J', 'N', 'M'}},
	{HN_JINMEI, DIC_SJ3,		/* Myouji */
	 2, {0x4944, 0x3b7a}},
	{HN_JINMEI, DIC_SJ3,		/* Namae */
	 2, {0x4c3e, 0x4130}},

	{HN_CHIMEI, DIC_WNNSJ3,		/* Chimei */
	 2, {0x434f, 0x4c3e}},
	{HN_CHIMEI, DIC_CANNA,		/* #CN */
	 3, {'#', 'C', 'N'}},
	{HN_CHIMEI, DIC_CANNA,		/* #CNS */
	 4, {'#', 'C', 'N', 'S'}},
	{HN_CHIMEI, DIC_CANNA,		/* #JNS */
	 4, {'#', 'J', 'N', 'S'}},
	{HN_CHIMEI, DIC_SJ3,		/* Kenku */
	 2, {0x3829, 0x3668}},

	{HN_JINCHI, DIC_WNN,		/* Jimmei&Chimei */
	 5, {0x3f4d, 0x4c3e, '&', 0x434f, 0x4c3e}},
	{HN_JINCHI, DIC_CANNA,		/* #JCN */
	 4, {'#', 'J', 'C', 'N'}},

	{HN_KOYUU, DIC_WNN,		/* KoyuuMeishi */
	 4, {0x3847, 0x4d2d, 0x4c3e, 0x3b6c}},
	{HN_KOYUU, DIC_CANNA,		/* #KK */
	 3, {'#', 'K', 'K'}},
	{HN_KOYUU, DIC_SJ3,		/* Kigyou */
	 2, {0x346b, 0x3648}},

	{HN_SUUSHI, DIC_WNNSJ3,		/* Suushi */
	 2, {0x3f74, 0x3b6c}},
	{HN_SUUSHI, DIC_CANNA,		/* #NN */
	 3, {'#', 'N', 'N'}},
	{HN_SUUSHI, DIC_CANNA,		/* #N00 */
	 4, {'#', 'N', '0', '0'}},
	{HN_SUUSHI, DIC_CANNA,		/* #N01 */
	 4, {'#', 'N', '0', '1'}},
	{HN_SUUSHI, DIC_CANNA,		/* #N02 */
	 4, {'#', 'N', '0', '2'}},
	{HN_SUUSHI, DIC_CANNA,		/* #N03 */
	 4, {'#', 'N', '0', '3'}},
	{HN_SUUSHI, DIC_SJ3,		/* Suushi2 */
	 3, {0x3f74, 0x3b6c, 0x2332}},

	{HN_KA5DOU, DIC_WNN,		/* KaDouKan */
	 3, {0x252b, 0x4630, 0x3434}},

	{HN_GA5DOU, DIC_WNN,		/* GaDouKan */
	 3, {0x252c, 0x4630, 0x3434}},

	{HN_SA5DOU, DIC_WNN,		/* SaDouKan */
	 3, {0x2535, 0x4630, 0x3434}},

	{HN_TA5DOU, DIC_WNN,		/* TaDouKan */
	 3, {0x253f, 0x4630, 0x3434}},

	{HN_NA5DOU, DIC_WNN,		/* NaDouKan */
	 3, {0x254a, 0x4630, 0x3434}},

	{HN_BA5DOU, DIC_WNN,		/* BaDouKan */
	 3, {0x2550, 0x4630, 0x3434}},

	{HN_MA5DOU, DIC_WNN,		/* MaDouKan */
	 3, {0x255e, 0x4630, 0x3434}},

	{HN_RA5DOU, DIC_WNN,		/* RaDouKan */
	 3, {0x2569, 0x4630, 0x3434}},

	{HN_WA5DOU, DIC_WNN,		/* WaDouKan */
	 3, {0x256f, 0x4630, 0x3434}},

	{HN_1DOU, DIC_WNN,		/* 1DouKan */
	 3, {0x306c, 0x4630, 0x3434}},

	{HN_SAHDOU, DIC_WNN,		/* SaHenDouKan */
	 4, {0x2535, 0x4a51, 0x4630, 0x3434}},

	{HN_ZAHDOU, DIC_WNN,		/* ZaHenDouKan */
	 4, {0x2536, 0x4a51, 0x4630, 0x3434}},

	{HN_KA5YOU, DIC_WNN,		/* KaDouRenyouKan */
	 5, {0x252b, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_GA5YOU, DIC_WNN,		/* GaDouRenyouKan */
	 5, {0x252c, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_SA5YOU, DIC_WNN,		/* SaDouRenyouKan */
	 5, {0x2535, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_TA5YOU, DIC_WNN,		/* TaDouRenyouKan */
	 5, {0x253f, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_NA5YOU, DIC_WNN,		/* NaDouRenyouKan */
	 5, {0x254a, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_BA5YOU, DIC_WNN,		/* BaDouRenyouKan */
	 5, {0x2550, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_MA5YOU, DIC_WNN,		/* MaDouRenyouKan */
	 5, {0x255e, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_RA5YOU, DIC_WNN,		/* RaDouRenyouKan */
	 5, {0x2569, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_WA5YOU, DIC_WNN,		/* WaDouRenyouKan */
	 5, {0x256f, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_1YOU, DIC_WNN,		/* 1DouRenyouKan */
	 5, {0x306c, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_SAHYOU, DIC_WNN,		/* SaHenDouRenyouKan */
	 6, {0x2535, 0x4a51, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_ZAHYOU, DIC_WNN,		/* ZaHenDouRenyouKan */
	 6, {0x2536, 0x4a51, 0x4630, 0x4f22, 0x4d51, 0x3434}},

	{HN_KA5DAN, DIC_WNN,		/* KaGyou5Dan */
	 4, {0x252b, 0x3954, 0x385e, 0x434a}},
	{HN_KA5DAN, DIC_CANNA,		/* #K5 */
	 3, {'#', 'K', '5'}},
	{HN_KA5DAN, DIC_CANNA,		/* #K5r */
	 4, {'#', 'K', '5', 'r'}},
	{HN_KA5DAN, DIC_SJ3,		/* Ka5-1 */
	 3, {0x252b, 0x385e, 0x2331}},
	{HN_KA5DAN, DIC_SJ3,		/* Ka5-2 */
	 3, {0x252b, 0x385e, 0x2332}},
	{HN_KA5DAN, DIC_SJ3,		/* Ka5-3 */
	 3, {0x252b, 0x385e, 0x2333}},
	{HN_KA5DAN, DIC_SJ3,		/* Ka5-4 */
	 3, {0x252b, 0x385e, 0x2334}},
	{HN_KA5DAN, DIC_SJ3,		/* Ka5-5 */
	 3, {0x252b, 0x385e, 0x2335}},
	{HN_KA5DAN, DIC_SJ3,		/* Ka5-6 */
	 3, {0x252b, 0x385e, 0x2336}},
	{HN_KA5DAN, DIC_SJ3,		/* Ka5-7 */
	 3, {0x252b, 0x385e, 0x2337}},
	{HN_KA5DAN, DIC_SJ3,		/* Ka5-8 */
	 3, {0x252b, 0x385e, 0x2338}},

	{HN_GA5DAN, DIC_WNN,		/* GaGyou5Dan */
	 4, {0x252c, 0x3954, 0x385e, 0x434a}},
	{HN_GA5DAN, DIC_CANNA,		/* #G5 */
	 3, {'#', 'G', '5'}},
	{HN_GA5DAN, DIC_CANNA,		/* #G5r */
	 4, {'#', 'G', '5', 'r'}},
	{HN_GA5DAN, DIC_SJ3,		/* Ga5-1 */
	 3, {0x252c, 0x385e, 0x2331}},
	{HN_GA5DAN, DIC_SJ3,		/* Ga5-2 */
	 3, {0x252c, 0x385e, 0x2332}},
	{HN_GA5DAN, DIC_SJ3,		/* Ga5-3 */
	 3, {0x252c, 0x385e, 0x2333}},
	{HN_GA5DAN, DIC_SJ3,		/* Ga5-4 */
	 3, {0x252c, 0x385e, 0x2334}},
	{HN_GA5DAN, DIC_SJ3,		/* Ga5-5 */
	 3, {0x252c, 0x385e, 0x2335}},
	{HN_GA5DAN, DIC_SJ3,		/* Ga5-6 */
	 3, {0x252c, 0x385e, 0x2336}},
	{HN_GA5DAN, DIC_SJ3,		/* Ga5-7 */
	 3, {0x252c, 0x385e, 0x2337}},
	{HN_GA5DAN, DIC_SJ3,		/* Ga5-8 */
	 3, {0x252c, 0x385e, 0x2338}},

	{HN_SA5DAN, DIC_WNN,		/* SaGyou5Dan */
	 4, {0x2535, 0x3954, 0x385e, 0x434a}},
	{HN_SA5DAN, DIC_CANNA,		/* #S5 */
	 3, {'#', 'S', '5'}},
	{HN_SA5DAN, DIC_CANNA,		/* #S5r */
	 4, {'#', 'S', '5', 'r'}},
	{HN_SA5DAN, DIC_SJ3,		/* Sa5-1 */
	 3, {0x2535, 0x385e, 0x2331}},
	{HN_SA5DAN, DIC_SJ3,		/* Sa5-2 */
	 3, {0x2535, 0x385e, 0x2332}},
	{HN_SA5DAN, DIC_SJ3,		/* Sa5-3 */
	 3, {0x2535, 0x385e, 0x2333}},
	{HN_SA5DAN, DIC_SJ3,		/* Sa5-4 */
	 3, {0x2535, 0x385e, 0x2334}},
	{HN_SA5DAN, DIC_SJ3,		/* Sa5-5 */
	 3, {0x2535, 0x385e, 0x2335}},
	{HN_SA5DAN, DIC_SJ3,		/* Sa5-6 */
	 3, {0x2535, 0x385e, 0x2336}},
	{HN_SA5DAN, DIC_SJ3,		/* Sa5-7 */
	 3, {0x2535, 0x385e, 0x2337}},
	{HN_SA5DAN, DIC_SJ3,		/* Sa5-8 */
	 3, {0x2535, 0x385e, 0x2338}},

	{HN_TA5DAN, DIC_WNN,		/* TaGyou5Dan */
	 4, {0x253f, 0x3954, 0x385e, 0x434a}},
	{HN_TA5DAN, DIC_CANNA,		/* #T5 */
	 3, {'#', 'T', '5'}},
	{HN_TA5DAN, DIC_CANNA,		/* #T5r */
	 4, {'#', 'T', '5', 'r'}},
	{HN_TA5DAN, DIC_SJ3,		/* Ta5-1 */
	 3, {0x253f, 0x385e, 0x2331}},
	{HN_TA5DAN, DIC_SJ3,		/* Ta5-2 */
	 3, {0x253f, 0x385e, 0x2332}},
	{HN_TA5DAN, DIC_SJ3,		/* Ta5-3 */
	 3, {0x253f, 0x385e, 0x2333}},
	{HN_TA5DAN, DIC_SJ3,		/* Ta5-4 */
	 3, {0x253f, 0x385e, 0x2334}},
	{HN_TA5DAN, DIC_SJ3,		/* Ta5-5 */
	 3, {0x253f, 0x385e, 0x2335}},
	{HN_TA5DAN, DIC_SJ3,		/* Ta5-6 */
	 3, {0x253f, 0x385e, 0x2336}},
	{HN_TA5DAN, DIC_SJ3,		/* Ta5-7 */
	 3, {0x253f, 0x385e, 0x2337}},
	{HN_TA5DAN, DIC_SJ3,		/* Ta5-8 */
	 3, {0x253f, 0x385e, 0x2338}},

	{HN_NA5DAN, DIC_WNN,		/* NaGyou5Dan */
	 4, {0x254a, 0x3954, 0x385e, 0x434a}},
	{HN_NA5DAN, DIC_CANNA,		/* #N5 */
	 3, {'#', 'N', '5'}},
	{HN_NA5DAN, DIC_SJ3,		/* Na5 */
	 2, {0x254a, 0x385e}},

	{HN_BA5DAN, DIC_WNN,		/* BaGyou5Dan */
	 4, {0x2550, 0x3954, 0x385e, 0x434a}},
	{HN_BA5DAN, DIC_CANNA,		/* #B5 */
	 3, {'#', 'B', '5'}},
	{HN_BA5DAN, DIC_CANNA,		/* #B5r */
	 4, {'#', 'B', '5', 'r'}},
	{HN_BA5DAN, DIC_SJ3,		/* Ba5-1 */
	 3, {0x2550, 0x385e, 0x2331}},
	{HN_BA5DAN, DIC_SJ3,		/* Ba5-2 */
	 3, {0x2550, 0x385e, 0x2332}},
	{HN_BA5DAN, DIC_SJ3,		/* Ba5-3 */
	 3, {0x2550, 0x385e, 0x2333}},
	{HN_BA5DAN, DIC_SJ3,		/* Ba5-4 */
	 3, {0x2550, 0x385e, 0x2334}},
	{HN_BA5DAN, DIC_SJ3,		/* Ba5-5 */
	 3, {0x2550, 0x385e, 0x2335}},
	{HN_BA5DAN, DIC_SJ3,		/* Ba5-6 */
	 3, {0x2550, 0x385e, 0x2336}},
	{HN_BA5DAN, DIC_SJ3,		/* Ba5-7 */
	 3, {0x2550, 0x385e, 0x2337}},
	{HN_BA5DAN, DIC_SJ3,		/* Ba5-8 */
	 3, {0x2550, 0x385e, 0x2338}},

	{HN_MA5DAN, DIC_WNN,		/* MaGyou5Dan */
	 4, {0x255e, 0x3954, 0x385e, 0x434a}},
	{HN_MA5DAN, DIC_CANNA,		/* #M5 */
	 3, {'#', 'M', '5'}},
	{HN_MA5DAN, DIC_CANNA,		/* #M5r */
	 4, {'#', 'M', '5', 'r'}},
	{HN_MA5DAN, DIC_SJ3,		/* Ma5-1 */
	 3, {0x255e, 0x385e, 0x2331}},
	{HN_MA5DAN, DIC_SJ3,		/* Ma5-2 */
	 3, {0x255e, 0x385e, 0x2332}},
	{HN_MA5DAN, DIC_SJ3,		/* Ma5-3 */
	 3, {0x255e, 0x385e, 0x2333}},
	{HN_MA5DAN, DIC_SJ3,		/* Ma5-4 */
	 3, {0x255e, 0x385e, 0x2334}},
	{HN_MA5DAN, DIC_SJ3,		/* Ma5-5 */
	 3, {0x255e, 0x385e, 0x2335}},
	{HN_MA5DAN, DIC_SJ3,		/* Ma5-6 */
	 3, {0x255e, 0x385e, 0x2336}},
	{HN_MA5DAN, DIC_SJ3,		/* Ma5-7 */
	 3, {0x255e, 0x385e, 0x2337}},
	{HN_MA5DAN, DIC_SJ3,		/* Ma5-8 */
	 3, {0x255e, 0x385e, 0x2338}},

	{HN_RA5DAN, DIC_WNN,		/* RaGyou5Dan */
	 4, {0x2569, 0x3954, 0x385e, 0x434a}},
	{HN_RA5DAN, DIC_CANNA,		/* #R5 */
	 3, {'#', 'R', '5'}},
	{HN_RA5DAN, DIC_CANNA,		/* #R5r */
	 4, {'#', 'R', '5', 'r'}},
	{HN_RA5DAN, DIC_SJ3,		/* Ra5-1 */
	 3, {0x2569, 0x385e, 0x2331}},
	{HN_RA5DAN, DIC_SJ3,		/* Ra5-2 */
	 3, {0x2569, 0x385e, 0x2332}},
	{HN_RA5DAN, DIC_SJ3,		/* Ra5-3 */
	 3, {0x2569, 0x385e, 0x2333}},
	{HN_RA5DAN, DIC_SJ3,		/* Ra5-4 */
	 3, {0x2569, 0x385e, 0x2334}},
	{HN_RA5DAN, DIC_SJ3,		/* Ra5-5 */
	 3, {0x2569, 0x385e, 0x2335}},
	{HN_RA5DAN, DIC_SJ3,		/* Ra5-6 */
	 3, {0x2569, 0x385e, 0x2336}},
	{HN_RA5DAN, DIC_SJ3,		/* Ra5-7 */
	 3, {0x2569, 0x385e, 0x2337}},
	{HN_RA5DAN, DIC_SJ3,		/* Ra5-8 */
	 3, {0x2569, 0x385e, 0x2338}},

	{HN_WA5DAN, DIC_WNN,		/* WaGyou5Dan */
	 4, {0x256f, 0x3954, 0x385e, 0x434a}},
	{HN_WA5DAN, DIC_CANNA,		/* #W5 */
	 3, {'#', 'W', '5'}},
	{HN_WA5DAN, DIC_CANNA,		/* #W5r */
	 4, {'#', 'W', '5', 'r'}},
	{HN_WA5DAN, DIC_SJ3,		/* Wa5-1 */
	 3, {0x256f, 0x385e, 0x2331}},
	{HN_WA5DAN, DIC_SJ3,		/* Wa5-2 */
	 3, {0x256f, 0x385e, 0x2332}},
	{HN_WA5DAN, DIC_SJ3,		/* Wa5-3 */
	 3, {0x256f, 0x385e, 0x2333}},
	{HN_WA5DAN, DIC_SJ3,		/* Wa5-4 */
	 3, {0x256f, 0x385e, 0x2334}},
	{HN_WA5DAN, DIC_SJ3,		/* Wa5-5 */
	 3, {0x256f, 0x385e, 0x2335}},
	{HN_WA5DAN, DIC_SJ3,		/* Wa5-6 */
	 3, {0x256f, 0x385e, 0x2336}},
	{HN_WA5DAN, DIC_SJ3,		/* Wa5-7 */
	 3, {0x256f, 0x385e, 0x2337}},
	{HN_WA5DAN, DIC_SJ3,		/* Wa5-8 */
	 3, {0x256f, 0x385e, 0x2338}},

	{HN_1DAN, DIC_WNN,		/* 1Dan */
	 2, {0x306c, 0x434a}},
	{HN_1DAN, DIC_CANNA,		/* #KS */
	 3, {'#', 'K', 'S'}},
	{HN_1DAN, DIC_SJ3,		/* Ippan1 */
	 3, {0x306c, 0x434a, 0x2331}},
	{HN_1DAN, DIC_SJ3,		/* Ippan2 */
	 3, {0x306c, 0x434a, 0x2332}},
	{HN_1DAN, DIC_SJ3,		/* Ippan3 */
	 3, {0x306c, 0x434a, 0x2333}},
	{HN_1DAN, DIC_SJ3,		/* Ippan4 */
	 3, {0x306c, 0x434a, 0x2334}},

	{HN_1_MEI, DIC_WNN,		/* 1Dan&Meishi */
	 5, {0x306c, 0x434a, '&', 0x4c3e, 0x3b6c}},
	{HN_1_MEI, DIC_CANNA,		/* #KSr */
	 4, {'#', 'K', 'S', 'r'}},

	{HN_KA_IKU, DIC_WNN,		/* KaGyou(Iku) */
	 6, {0x252b, 0x3954, '(', 0x3954, 0x242f, ')'}},
	{HN_KA_IKU, DIC_CANNA,		/* #C5r */
	 4, {'#', 'C', '5', 'r'}},
	{HN_KA_IKU, DIC_SJ3,		/* Ka5Onbin */
	 4, {0x252b, 0x385e, 0x323b, 0x4a58}},

	{HN_KO_KO, DIC_WNN,		/* Ko(Ko) */
	 4, {0x4d68, '(', 0x2433, ')'}},
	{HN_KO_KO, DIC_CANNA,		/* #kxo */
	 4, {'#', 'k', 'x', 'o'}},
	{HN_KO_KO, DIC_CANNA,		/* #kxoi */
	 5, {'#', 'k', 'x', 'o', 'i'}},
	{HN_KO_KO, DIC_SJ3,		/* KaHenMi */
	 3, {0x252b, 0x4a51, 0x4c24}},
	{HN_KO_KO, DIC_SJ3,		/* KaHenMei */
	 3, {0x252b, 0x4a51, 0x4c3f}},

	{HN_KI_KI, DIC_WNN,		/* Ki(Ki) */
	 4, {0x4d68, '(', 0x242d, ')'}},
	{HN_KI_KI, DIC_CANNA,		/* #kxi */
	 4, {'#', 'k', 'x', 'i'}},
	{HN_KI_KI, DIC_SJ3,		/* KaHenYou */
	 3, {0x252b, 0x4a51, 0x4d51}},

	{HN_KU_KU, DIC_WNN,		/* Ku(Ku) */
	 4, {0x4d68, '(', 0x242f, ')'}},
	{HN_KU_KU, DIC_CANNA,		/* #kxuru */
	 6, {'#', 'k', 'x', 'u', 'r', 'u'}},
	{HN_KU_KU, DIC_CANNA,		/* #kxure */
	 6, {'#', 'k', 'x', 'u', 'r', 'e'}},
	{HN_KU_KU, DIC_SJ3,		/* KaHenKa */
	 3, {0x252b, 0x4a51, 0x323e}},
	{HN_KU_KU, DIC_SJ3,		/* KaHenShuuTai */
	 4, {0x252b, 0x4a51, 0x3d2a, 0x424e}},

	{HN_SAHEN, DIC_WNN,		/* SaGyou(Suru) */
	 6, {0x2535, 0x3954, '(', 0x2439, 0x246b, ')'}},
	{HN_SAHEN, DIC_CANNA,		/* #T10 */
	 4, {'#', 'T', '1', '0'}},
	{HN_SAHEN, DIC_CANNA,		/* #T11 */
	 4, {'#', 'T', '1', '1'}},
	{HN_SAHEN, DIC_CANNA,		/* #T12 */
	 4, {'#', 'T', '1', '2'}},
	{HN_SAHEN, DIC_CANNA,		/* #SX */
	 3, {'#', 'S', 'X'}},
	{HN_SAHEN, DIC_SJ3,		/* SaHen */
	 2, {0x2535, 0x4a51}},

	{HN_ZAHEN, DIC_WNN,		/* ZaGyou(Zuru) */
	 6, {0x2536, 0x3954, '(', 0x243a, 0x246b, ')'}},
	{HN_ZAHEN, DIC_CANNA,		/* #ZX */
	 3, {'#', 'Z', 'X'}},
	{HN_ZAHEN, DIC_SJ3,		/* ZaHen */
	 2, {0x2536, 0x4a51}},

	{HN_SA_MEI, DIC_WNN,		/* SaGyou(Suru)&Meishi */
	 9, {0x2535, 0x3954, '(', 0x2439, 0x246b, ')', '&', 0x4c3e, 0x3b6c}},
	{HN_SA_MEI, DIC_CANNA,		/* #T00 */
	 4, {'#', 'T', '0', '0'}},
	{HN_SA_MEI, DIC_CANNA,		/* #T30 */
	 4, {'#', 'T', '3', '0'}},
	{HN_SA_MEI, DIC_SJ3,		/* Mei8 */
	 2, {0x4c3e, 0x2338}},

	{HN_SI_SI, DIC_WNN,		/* Shi(Shi) */
	 4, {0x3059, '(', 0x2437, ')'}},
	{HN_SI_SI, DIC_CANNA,		/* #sxi */
	 4, {'#', 's', 'x', 'i'}},
	{HN_SI_SI, DIC_SJ3,		/* SaHenMiYou */
	 4, {0x2535, 0x4a51, 0x4c24, 0x4d51}},

	{HN_SU_SU, DIC_WNN,		/* Su(Su) */
	 4, {0x3059, '(', 0x2439, ')'}},
	{HN_SU_SU, DIC_CANNA,		/* #sxuru */
	 6, {'#', 's', 'x', 'u', 'r', 'u'}},
	{HN_SU_SU, DIC_CANNA,		/* #sxu1 */
	 5, {'#', 's', 'x', 'u', '1'}},
	{HN_SU_SU, DIC_SJ3,		/* SaHenKa */
	 3, {0x2535, 0x4a51, 0x323e}},
	{HN_SU_SU, DIC_SJ3,		/* SaHenShuuTai */
	 4, {0x2535, 0x4a51, 0x3d2a, 0x424e}},

	{HN_SE_SE, DIC_WNN,		/* Se(Se) */
	 4, {0x3059, '(', 0x243b, ')'}},
	{HN_SE_SE, DIC_CANNA,		/* #sxe */
	 4, {'#', 's', 'x', 'e'}},
	{HN_SE_SE, DIC_SJ3,		/* SaHenMi2 */
	 4, {0x2535, 0x4a51, 0x4c24, 0x2332}},
	{HN_SE_SE, DIC_SJ3,		/* SaHenMei2 */
	 4, {0x2535, 0x4a51, 0x4c3f, 0x2332}},

	{HN_RA_KUD, DIC_WNN,		/* RaGyou(Kudasai) */
	 7, {0x2569, 0x3954, '(', 0x323c, 0x2435, 0x2424, ')'}},
	{HN_RA_KUD, DIC_CANNA,		/* #L5 */
	 3, {'#', 'L', '5'}},

	{HN_KEIYOU, DIC_WNN,		/* Keiyoushi */
	 3, {0x3741, 0x4d46, 0x3b6c}},
	{HN_KEIYOU, DIC_CANNA,		/* #KY */
	 3, {'#', 'K', 'Y'}},
	{HN_KEIYOU, DIC_CANNA,		/* #KYmi */
	 5, {'#', 'K', 'Y', 'm', 'i'}},
	{HN_KEIYOU, DIC_CANNA,		/* #KYU */
	 4, {'#', 'K', 'Y', 'U'}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei1 */
	 2, {0x3741, 0x2331}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei2 */
	 2, {0x3741, 0x2332}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei3 */
	 2, {0x3741, 0x2333}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei4 */
	 2, {0x3741, 0x2334}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei5 */
	 2, {0x3741, 0x2335}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei6 */
	 2, {0x3741, 0x2336}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei7 */
	 2, {0x3741, 0x2337}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei8 */
	 2, {0x3741, 0x2338}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei9 */
	 2, {0x3741, 0x2339}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei10 */
	 3, {0x3741, 0x2331, 0x2330}},
	{HN_KEIYOU, DIC_SJ3,		/* Kei11 */
	 3, {0x3741, 0x2331, 0x2331}},

	{HN_KEIDOU, DIC_WNN,		/* KeiyouDoushi */
	 4, {0x3741, 0x4d46, 0x4630, 0x3b6c}},
	{HN_KEIDOU, DIC_CANNA,		/* #T02 */
	 4, {'#', 'T', '0', '2'}},
	{HN_KEIDOU, DIC_CANNA,		/* #T03 */
	 4, {'#', 'T', '0', '3'}},
	{HN_KEIDOU, DIC_CANNA,		/* #T07 */
	 4, {'#', 'T', '0', '7'}},
	{HN_KEIDOU, DIC_CANNA,		/* #T08 */
	 4, {'#', 'T', '0', '8'}},
	{HN_KEIDOU, DIC_CANNA,		/* #T16 */
	 4, {'#', 'T', '1', '6'}},
	{HN_KEIDOU, DIC_CANNA,		/* #T17 */
	 4, {'#', 'T', '1', '7'}},
	{HN_KEIDOU, DIC_CANNA,		/* #T18 */
	 4, {'#', 'T', '1', '8'}},
	{HN_KEIDOU, DIC_CANNA,		/* #T19 */
	 4, {'#', 'T', '1', '9'}},
	{HN_KEIDOU, DIC_SJ3,		/* KeiDou1 */
	 3, {0x3741, 0x4630, 0x2331}},
	{HN_KEIDOU, DIC_SJ3,		/* KeiDou3 */
	 3, {0x3741, 0x4630, 0x2333}},
	{HN_KEIDOU, DIC_SJ3,		/* KeiDou4 */
	 3, {0x3741, 0x4630, 0x2334}},
	{HN_KEIDOU, DIC_SJ3,		/* KeiDou5 */
	 3, {0x3741, 0x4630, 0x2335}},
	{HN_KEIDOU, DIC_SJ3,		/* KeiDou6 */
	 3, {0x3741, 0x4630, 0x2336}},
	{HN_KEIDOU, DIC_SJ3,		/* KeiDou7 */
	 3, {0x3741, 0x4630, 0x2337}},
	{HN_KEIDOU, DIC_SJ3,		/* KeiDou8 */
	 3, {0x3741, 0x4630, 0x2338}},
	{HN_KEIDOU, DIC_SJ3,		/* KeiDou9 */
	 3, {0x3741, 0x4630, 0x2339}},

	{HN_KD_MEI, DIC_WNN,		/* KeiyouDoushi&Meishi */
	 7, {0x3741, 0x4d46, 0x4630, 0x3b6c, '&', 0x4c3e, 0x3b6c}},
	{HN_KD_MEI, DIC_CANNA,		/* #T05 */
	 4, {'#', 'T', '0', '5'}},
	{HN_KD_MEI, DIC_CANNA,		/* #T15 */
	 4, {'#', 'T', '1', '5'}},

	{HN_KD_TAR, DIC_WNN,		/* KeiyouDoushi(Taru) */
	 8, {0x3741, 0x4d46, 0x4630, 0x3b6c, '(', 0x243f, 0x246b, ')'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F00 */
	 4, {'#', 'F', '0', '0'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F01 */
	 4, {'#', 'F', '0', '1'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F02 */
	 4, {'#', 'F', '0', '2'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F03 */
	 4, {'#', 'F', '0', '3'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F05 */
	 4, {'#', 'F', '0', '5'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F06 */
	 4, {'#', 'F', '0', '6'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F09 */
	 4, {'#', 'F', '0', '9'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F10 */
	 4, {'#', 'F', '1', '0'}},
	{HN_KD_TAR, DIC_CANNA,		/* #F11 */
	 4, {'#', 'F', '1', '1'}},
	{HN_KD_TAR, DIC_SJ3,		/* KeiDou2 */
	 3, {0x3741, 0x4630, 0x2332}},

	{HN_FUKUSI, DIC_WNN,		/* Fukushi */
	 2, {0x497b, 0x3b6c}},
	{HN_FUKUSI, DIC_CANNA,		/* #T06 */
	 4, {'#', 'T', '0', '6'}},
	{HN_FUKUSI, DIC_CANNA,		/* #T26 */
	 4, {'#', 'T', '2', '6'}},
	{HN_FUKUSI, DIC_CANNA,		/* #T31 */
	 4, {'#', 'T', '3', '1'}},
	{HN_FUKUSI, DIC_CANNA,		/* #T32 */
	 4, {'#', 'T', '3', '2'}},
	{HN_FUKUSI, DIC_CANNA,		/* #T36 */
	 4, {'#', 'T', '3', '6'}},
	{HN_FUKUSI, DIC_CANNA,		/* #F04 */
	 4, {'#', 'F', '0', '4'}},
	{HN_FUKUSI, DIC_CANNA,		/* #F12 */
	 4, {'#', 'F', '1', '2'}},
	{HN_FUKUSI, DIC_CANNA,		/* #F14 */
	 4, {'#', 'F', '1', '4'}},
	{HN_FUKUSI, DIC_CANNA,		/* #F15 */
	 4, {'#', 'F', '1', '5'}},
	{HN_FUKUSI, DIC_SJ3,		/* TokushuFuku */
	 3, {0x4643, 0x3c6c, 0x497b}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku1 */
	 2, {0x497b, 0x2331}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku2 */
	 2, {0x497b, 0x2332}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku3 */
	 2, {0x497b, 0x2333}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku4 */
	 2, {0x497b, 0x2334}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku5 */
	 2, {0x497b, 0x2335}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku6 */
	 2, {0x497b, 0x2336}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku7 */
	 2, {0x497b, 0x2337}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku8 */
	 2, {0x497b, 0x2338}},
	{HN_FUKUSI, DIC_SJ3,		/* Fuku9 */
	 2, {0x497b, 0x2339}},

	{HN_RENTAI, DIC_WNN,		/* Rentaishi */
	 3, {0x4f22, 0x424e, 0x3b6c}},
	{HN_RENTAI, DIC_CANNA,		/* #RT */
	 3, {'#', 'R', 'T'}},
	{HN_RENTAI, DIC_SJ3,		/* Rentai */
	 2, {0x4f22, 0x424e}},

	{HN_SETKAN, DIC_WNN,		/* Setsuzokushi,Kandoushi */
	 7, {0x405c, 0x4233, 0x3b6c, ',', 0x3436, 0x4630, 0x3b6c}},
	{HN_SETKAN, DIC_CANNA,		/* #CJ */
	 3, {'#', 'C', 'J'}},
	{HN_SETKAN, DIC_SJ3,		/* AiSatzu */
	 2, {0x3027, 0x3b22}},
	{HN_SETKAN, DIC_SJ3,		/* Kandou */
	 2, {0x3436, 0x4630}},
	{HN_SETKAN, DIC_SJ3,		/* Setsuzoku */
	 2, {0x405c, 0x4233}},

	{HN_TANKAN, DIC_WNN,		/* TanKanji */
	 3, {0x4331, 0x3441, 0x3b7a}},
	{HN_TANKAN, DIC_CANNA,		/* #KJ */
	 3, {'#', 'K', 'J'}},
	{HN_TANKAN, DIC_CANNA,		/* #TK */
	 3, {'#', 'T', 'K'}},
	{HN_TANKAN, DIC_SJ3,		/* TanKan */
	 2, {0x4331, 0x3441}},

	{HN_SETTOU, DIC_WNN,		/* Settougo */
	 3, {0x405c, 0x462c, 0x386c}},
	{HN_SETTOU, DIC_CANNA,		/* #PRE */
	 4, {'#', 'P', 'R', 'E'}},
	{HN_SETTOU, DIC_SJ3,		/* Settou1 */
	 3, {0x405c, 0x462c, 0x2331}},
	{HN_SETTOU, DIC_SJ3,		/* Settou3 */
	 3, {0x405c, 0x462c, 0x2333}},
	{HN_SETTOU, DIC_SJ3,		/* Settou5 */
	 3, {0x405c, 0x462c, 0x2335}},

	{HN_SETUBI, DIC_WNN,		/* Setsubigo */
	 3, {0x405c, 0x4878, 0x386c}},
	{HN_SETUBI, DIC_CANNA,		/* #N2T10 */
	 6, {'#', 'N', '2', 'T', '1', '0'}},
	{HN_SETUBI, DIC_CANNA,		/* #N2KS */
	 5, {'#', 'N', '2', 'K', 'S'}},
	{HN_SETUBI, DIC_CANNA,		/* #SUC */
	 4, {'#', 'S', 'U', 'C'}},
	{HN_SETUBI, DIC_SJ3,		/* Setsubi1 */
	 3, {0x405c, 0x4878, 0x2331}},
	{HN_SETUBI, DIC_SJ3,		/* Setsubi2 */
	 3, {0x405c, 0x4878, 0x2332}},
	{HN_SETUBI, DIC_SJ3,		/* Setsubi3 */
	 3, {0x405c, 0x4878, 0x2333}},
	{HN_SETUBI, DIC_SJ3,		/* Setsubi8 */
	 3, {0x405c, 0x4878, 0x2338}},
	{HN_SETUBI, DIC_SJ3,		/* Setsubi9 */
	 3, {0x405c, 0x4878, 0x2339}},

	{HN_SETOSU, DIC_WNN,		/* SettouSuushi */
	 4, {0x405c, 0x462c, 0x3f74, 0x3b6c}},
	{HN_SETOSU, DIC_CANNA,		/* #NNPRE */
	 6, {'#', 'N', 'N', 'P', 'R', 'E'}},
	{HN_SETOSU, DIC_SJ3,		/* Settou4 */
	 3, {0x405c, 0x462c, 0x2334}},

	{HN_JOSUU, DIC_WNN,		/* JoSuushi */
	 3, {0x3d75, 0x3f74, 0x3b6c}},
	{HN_JOSUU, DIC_CANNA,		/* #JS */
	 3, {'#', 'J', 'S'}},
	{HN_JOSUU, DIC_SJ3,		/* Josuu */
	 2, {0x3d75, 0x3f74}},
	{HN_JOSUU, DIC_SJ3,		/* Josuu2 */
	 3, {0x3d75, 0x3f74, 0x2332}},

	{HN_SETOJO, DIC_WNN,		/* SettouJoSuushi */
	 5, {0x405c, 0x462c, 0x3d75, 0x3f74, 0x3b6c}},

	{HN_SETUJO, DIC_WNN,		/* SetsubiJoSuushi */
	 5, {0x405c, 0x4878, 0x3d75, 0x3f74, 0x3b6c}},
	{HN_SETUJO, DIC_CANNA,		/* #JSSUC */
	 6, {'#', 'J', 'S', 'S', 'U', 'C'}},

	{HN_SETUJN, DIC_WNN,		/* SetsubJinmei */
	 4, {0x405c, 0x4878, 0x3f4d, 0x4c3e}},
	{HN_SETUJN, DIC_CANNA,		/* #JNSUC */
	 6, {'#', 'J', 'N', 'S', 'U', 'C'}},

	{HN_SETOCH, DIC_WNN,		/* SettouChimei */
	 4, {0x405c, 0x462c, 0x434f, 0x4c3e}},
	{HN_SETOCH, DIC_CANNA,		/* #CNPRE */
	 6, {'#', 'C', 'N', 'P', 'R', 'E'}},
	{HN_SETOCH, DIC_SJ3,		/* Settou2 */
	 3, {0x405c, 0x462c, 0x2332}},

	{HN_SETUCH, DIC_WNN,		/* SetsubiChimei */
	 4, {0x405c, 0x4878, 0x434f, 0x4c3e}},
	{HN_SETUCH, DIC_CANNA,		/* #CNSUC1 */
	 7, {'#', 'C', 'N', 'S', 'U', 'C', '1'}},
	{HN_SETUCH, DIC_SJ3,		/* Setsubi4 */
	 3, {0x405c, 0x4878, 0x2334}},

	{HN_SETO_O, DIC_WNN,		/* Settougo(O) */
	 6, {0x405c, 0x462c, 0x386c, '(', 0x242a, ')'}},

	{HN_SETO_K, DIC_WNN,		/* Settougo(Kaku) */
	 6, {0x405c, 0x462c, 0x386c, '(', 0x3346, ')'}},

	{HN_KD_STB, DIC_WNN,		/* KeiyouDoushiKaSetsubigo */
	 8, {0x3741, 0x4d46, 0x4630, 0x3b6c, 0x323d, 0x405c, 0x4878, 0x386c}},
	{HN_KD_STB, DIC_CANNA,		/* #N2T15 */
	 6, {'#', 'N', '2', 'T', '1', '5'}},
	{HN_KD_STB, DIC_CANNA,		/* #N2T16 */
	 6, {'#', 'N', '2', 'T', '1', '6'}},
	{HN_KD_STB, DIC_CANNA,		/* #N2T17 */
	 6, {'#', 'N', '2', 'T', '1', '7'}},
	{HN_KD_STB, DIC_CANNA,		/* #N2KYT */
	 6, {'#', 'N', '2', 'K', 'Y', 'T'}},
	{HN_KD_STB, DIC_SJ3,		/* Setsubi6 */
	 3, {0x405c, 0x4878, 0x2336}},

	{HN_SA_M_S, DIC_WNN,		/* Sagyou(Suru)&MeishiKaSetsubigo */
	 13, {0x2535, 0x3954, '(', 0x2439, 0x246b, ')',
	 '&', 0x4c3e, 0x3b6c, 0x323d, 0x405c, 0x4878, 0x386c}},
	{HN_SA_M_S, DIC_CANNA,		/* #N2T30 */
	 6, {'#', 'N', '2', 'T', '3', '0'}},
	{HN_SA_M_S, DIC_SJ3,		/* Setsubi7 */
	 3, {0x405c, 0x4878, 0x2337}},

	{HN_SETUDO, DIC_WNN,		/* SetsubiDoushi */
	 4, {0x405c, 0x4878, 0x4630, 0x3b6c}},
	{HN_SETUDO, DIC_CANNA,		/* #D2T35 */
	 6, {'#', 'D', '2', 'T', '3', '5'}},

	{HN_KE_SED, DIC_WNN,		/* KeiyoushiKaSetsubiDoushi */
	 8, {0x3741, 0x4d46, 0x3b6c, 0x323d, 0x405c, 0x4878, 0x4630, 0x3b6c}},
	{HN_KE_SED, DIC_CANNA,		/* #D2KY */
	 5, {'#', 'D', '2', 'K', 'Y'}},
	{HN_KE_SED, DIC_SJ3,		/* Setsubi5 */
	 3, {0x405c, 0x4878, 0x2335}},

	{HN_MAX, DIC_CANNA,		/* #sxa */
	 4, {'#', 's', 'x', 'a'}},
	{HN_MAX, DIC_CANNA,		/* #sxiro */
	 6, {'#', 's', 'x', 'i', 'r', 'o'}},
	{HN_MAX, DIC_SJ3,		/* SaHenMi1 */
	 4, {0x2535, 0x4a51, 0x4c24, 0x2331}},
	{HN_MAX, DIC_SJ3,		/* SaHenMei1 */
	 4, {0x2535, 0x4a51, 0x4c3f, 0x2331}},
};
#define	HINSILISTSIZ		arraysize(hinsilist)
static CONST contable jirconlist[] = {
	/* Sentou */
	{HN_SENTOU, 0, {0}, 1, {HN_SENTOU}},

	/* Suuji */
	{HN_SUUJI, 0, {0}, 4, {HN_SENTOU, HN_SUUJI, HN_SUUSHI, HN_SETOSU}},

	/* Kana */
	{HN_KANA, 0, {0}, 1, {HN_SENTOU}},

	/* EiSuu */
	{HN_EISUU, 0, {0}, 2, {HN_SENTOU, HN_KIGOU}},

	/* Kigou */
	{HN_KIGOU, 0, {0}, 3, {HN_SENTOU, HN_EISUU, HN_KIGOU}},

	/* HeiKakko */
	{HN_HEIKAKKO, 0, {0}, 1, {HN_SENTOU}},

	/* Fuzokugo */
	{HN_FUZOKUGO, 0, {0}, 1, {HN_SENTOU}},

	/* KaiKakko */
	{HN_KAIKAKKO, 0, {0}, 1, {HN_SENTOU}},

	/* Giji */
	{HN_GIJI, 0, {0}, 1, {HN_SENTOU}},

	/* Meishi */
	{HN_MEISHI, 0, {0}, 3, {HN_SENTOU, HN_SETTOU, HN_SETO_K}},

	/* Jinmei */
	{HN_JINMEI, 0, {0}, 1, {HN_SENTOU}},

	/* Chimei */
	{HN_CHIMEI, 0, {0}, 2, {HN_SENTOU, HN_SETOCH}},

	/* KoyuuMeishi */
	{HN_KOYUU, 0, {0}, 2, {HN_SENTOU, HN_SETTOU}},

	/* Suushi */
	{HN_SUUSHI, 0, {0}, 4, {HN_SENTOU, HN_SUUJI, HN_SUUSHI, HN_SETOSU}},

	/* KaDouKan */
	{HN_KA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* GaDouKan */
	{HN_GA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* SaDouKan */
	{HN_SA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* TaDouKan */
	{HN_TA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* NaDouKan */
	{HN_NA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* BaDouKan */
	{HN_BA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* MaDouKan */
	{HN_MA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* RaDouKan */
	{HN_RA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* WaDouKan */
	{HN_WA5DOU, 0, {0}, 1, {HN_SENTOU}},

	/* 1DouKan */
	{HN_1DOU, 0, {0}, 1, {HN_SENTOU}},

	/* SaHenDouKan */
	{HN_SAHDOU, 0, {0}, 1, {HN_SENTOU}},

	/* ZaHenDouKan */
	{HN_ZAHDOU, 0, {0}, 1, {HN_SENTOU}},

	/* KaDouRenyouKan */
	{HN_KA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* GaDouRenyouKan */
	{HN_GA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* SaDouRenyouKan */
	{HN_SA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* TaDouRenyouKan */
	{HN_TA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* NaDouRenyouKan */
	{HN_NA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* BaDouRenyouKan */
	{HN_BA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* MaDouRenyouKan */
	{HN_MA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* RaDouRenyouKan */
	{HN_RA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* WaDouRenyouKan */
	{HN_WA5YOU, 0, {0}, 1, {HN_SETO_O}},

	/* 1DouRenyouKan */
	{HN_1YOU, 0, {0}, 1, {HN_SETO_O}},

	/* SaHenDouRenyouKan */
	{HN_SAHYOU, 0, {0}, 1, {HN_SETO_O}},

	/* ZaHenDouRenyouKan */
	{HN_ZAHYOU, 0, {0}, 1, {HN_SETO_O}},

	/* KaGyou(Iku) */
	{HN_KA_IKU, 0, {0}, 1, {HN_SENTOU}},

	/* Ko(Ko) */
	{HN_KO_KO, 0, {0}, 1, {HN_SENTOU}},

	/* Ki(Ki) */
	{HN_KI_KI, 0, {0}, 1, {HN_SENTOU}},

	/* Ku(Ku) */
	{HN_KU_KU, 0, {0}, 1, {HN_SENTOU}},

	/* SaGyou(Suru)&Meishi */
	{HN_SA_MEI, 0, {0}, 3, {HN_SENTOU, HN_SETTOU, HN_SETO_K}},

	/* Shi(Shi) */
	{HN_SI_SI, 0, {0}, 1, {HN_SENTOU}},

	/* Su(Su) */
	{HN_SU_SU, 0, {0}, 1, {HN_SENTOU}},

	/* Se(Se) */
	{HN_SE_SE, 0, {0}, 1, {HN_SENTOU}},

	/* RaGyou(Kudasai) */
	{HN_RA_KUD, 0, {0}, 1, {HN_SENTOU}},

	/* Keiyoushi */
	{HN_KEIYOU, 0, {0}, 2, {HN_SENTOU, HN_SETO_O}},

	/* KeiyouDoushi */
	{HN_KEIDOU, 0, {0}, 2, {HN_SENTOU, HN_SETO_O}},

	/* KeiyouDoushi(Taru) */
	{HN_KD_TAR, 0, {0}, 1, {HN_SENTOU}},

	/* Fukushi */
	{HN_FUKUSI, 0, {0}, 1, {HN_SENTOU}},

	/* Rentaishi */
	{HN_RENTAI, 0, {0}, 1, {HN_SENTOU}},

	/* Setsuzokushi,Kandoushi */
	{HN_SETKAN, 0, {0}, 1, {HN_SENTOU}},

	/* Settougo */
	{HN_SETTOU, 0, {0}, 2, {HN_SENTOU, HN_SETO_K}},

	/* Setsubigo */
	{HN_SETUBI, 0, {0}, 5,
	 {HN_KANA, HN_EISUU, HN_MEISHI, HN_KOYUU, HN_SA_MEI}},

	/* SettouSuushi */
	{HN_SETOSU, 0, {0}, 1, {HN_SENTOU}},

	/* JoSuushi */
	{HN_JOSUU, 0, {0}, 3, {HN_SUUJI, HN_SUUSHI, HN_SETOJO}},

	/* SettouJoSuushi */
	{HN_SETOJO, 0, {0}, 2, {HN_SUUJI, HN_SUUSHI}},

	/* SetsubiJoSuushi */
	{HN_SETUJO, 0, {0}, 3, {HN_SUUJI, HN_SUUSHI, HN_JOSUU}},

	/* SetsubJinmei */
	{HN_SETUJN, 0, {0}, 1, {HN_JINMEI}},

	/* SettouChimei */
	{HN_SETOCH, 0, {0}, 1, {HN_SENTOU}},

	/* SetsubiChimei */
	{HN_SETUCH, 0, {0}, 2, {HN_CHIMEI, HN_SETUCH}},

	/* Settougo(O) */
	{HN_SETO_O, 0, {0}, 1, {HN_SENTOU}},

	/* Settougo(Kaku) */
	{HN_SETO_K, 0, {0}, 1, {HN_SENTOU}},

	/* KeiyouDoushiKaSetsubigo */
	{HN_KD_STB, 0, {0}, 5,
	 {HN_KANA, HN_EISUU, HN_MEISHI, HN_KOYUU, HN_SA_MEI}},

	/* Sagyou(Suru)&MeishiKaSetsubigo */
	{HN_SA_M_S, 0, {0}, 5,
	 {HN_KANA, HN_EISUU, HN_MEISHI, HN_KOYUU, HN_SA_MEI}},

	/* SetsubiDoushi */
	{HN_SETUDO, 0, {0}, 27,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I4, FZ_I8, FZ_KANE, FZ_GARI, FZ_KI4,
	  FZ_KI6, FZ_GI, FZ_KURE3, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI7,
	  FZ_SHIME, FZ_JI2, FZ_SE4, FZ_CHI, FZ_DEKI, FZ_NI3, FZ_BI, FZ_MI,
	  FZ_MI3, FZ_RARE, FZ_RI2, FZ_RE5}},

	/* KeiyoushiKaSetsubiDoushi */
	{HN_KE_SED, 0, {0}, 27,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I4, FZ_I8, FZ_KANE, FZ_GARI, FZ_KI4,
	  FZ_KI6, FZ_GI, FZ_KURE3, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI7,
	  FZ_SHIME, FZ_JI2, FZ_SE4, FZ_CHI, FZ_DEKI, FZ_NI3, FZ_BI, FZ_MI,
	  FZ_MI3, FZ_RARE, FZ_RI2, FZ_RE5}},
};
#define	JIRCONLISTSIZ		arraysize(jirconlist)
static CONST contable conlist[] = {
	/* KeiDouTai-Na */
	{FZ_NA, 1, {0x244a}, 7,
	 {HN_KANA, HN_EISUU, HN_KEIDOU, HN_KD_STB, FZ_SOU2, FZ_MITAI,
	  FZ_YOU2}},

	/* JoDou-Da-Tai-Na */
	{FZ_NA2, 1, {0x244a}, 51,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_KA3, FZ_KARA2, FZ_GARI,
	  FZ_KI, FZ_KI4, FZ_KIRI, FZ_GI, FZ_KURAI, FZ_GURAI, FZ_GE2, FZ_KOSO,
	  FZ_SA3, FZ_SHI4, FZ_ZUTSU, FZ_DAKE, FZ_CHI, FZ_NAZO, FZ_NADO, FZ_NI3,
	  FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_ME2,
	  FZ_YUE, FZ_RI2}},

	/* KeiDou,JoDou-Da-Shi */
	{FZ_DA, 1, {0x2440}, 71,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1YOU,
	  HN_SA_MEI, HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_I4, FZ_I5, FZ_KA3, FZ_KARA2, FZ_GARI, FZ_GARI2, FZ_KI, FZ_KI4,
	  FZ_KI5, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KURAI, FZ_GURAI, FZ_GE2, FZ_KOSO,
	  FZ_SA3, FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_JI, FZ_ZUTSU, FZ_SOU2, FZ_DAKE,
	  FZ_CHI, FZ_CHI2, FZ_NAZO, FZ_NADO, FZ_NI3, FZ_NI4, FZ_NI6, FZ_NO2,
	  FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2,
	  FZ_MI4, FZ_MITAI, FZ_ME2, FZ_YUE, FZ_YOU2, FZ_RI2, FZ_RI3}},

	/* KeiDou,JoDou-Da-Ka */
	{FZ_NARA, 2, {0x244a, 0x2469}, 102,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1YOU,
	  HN_SA_MEI, HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_I, FZ_I4, FZ_I5, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_KA3,
	  FZ_KARA2, FZ_GARI, FZ_GARI2, FZ_GARU, FZ_KI, FZ_KI4, FZ_KI5, FZ_KIRI,
	  FZ_GI, FZ_GI2, FZ_KU3, FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2,
	  FZ_KOSO, FZ_SA3, FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_SHIMAU, FZ_JI,
	  FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_SOU2, FZ_TA,
	  FZ_DA2, FZ_DAKE, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TO, FZ_TO4, FZ_NAZO,
	  FZ_NADO, FZ_NARU2, FZ_NI, FZ_NI3, FZ_NI4, FZ_NI6, FZ_NU2, FZ_NO2,
	  FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HODO, FZ_MADE, FZ_MI,
	  FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME2, FZ_YARU, FZ_YUE, FZ_YOU2,
	  FZ_YORU, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3}},
	{FZ_RA, 1, {0x2469}, 2, {FZ_TA, FZ_DA2}},

	/* KeiDouYou1-2-Ni */
	{FZ_NI, 1, {0x244b}, 6,
	 {HN_KEIDOU, HN_KD_STB, FZ_GE2, FZ_MITAI, FZ_ME2, FZ_YOU2}},
	{FZ_TO, 1, {0x2448}, 1, {HN_KD_TAR}},

	/* YoutaiJoDou-Souda-You-Ni */
	{FZ_NI2, 1, {0x244b}, 1, {FZ_SOU2}},

	/* KeiDou,JoDou-Da-You1-1-De */
	{FZ_DE, 1, {0x2447}, 71,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1YOU,
	  HN_SA_MEI, HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_I4, FZ_I5, FZ_KA3, FZ_KARA2, FZ_GARI, FZ_GARI2, FZ_KI, FZ_KI4,
	  FZ_KI5, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KURAI, FZ_GURAI, FZ_GE2, FZ_KOSO,
	  FZ_SA3, FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_JI, FZ_ZUTSU, FZ_SOU2, FZ_DAKE,
	  FZ_CHI, FZ_CHI2, FZ_NAZO, FZ_NADO, FZ_NI3, FZ_NI4, FZ_NI6, FZ_NO2,
	  FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2,
	  FZ_MI4, FZ_MITAI, FZ_ME2, FZ_YUE, FZ_YOU2, FZ_RI2, FZ_RI3}},

	/* DoDou-Desu-ShiTai */
	{FZ_SU, 1, {0x2439}, 1, {FZ_DE3}},

	/* Tari-KeiDou,JoDou-Taru,Aru-Mi */
	{FZ_RA2, 1, {0x2469}, 1, {FZ_A}},
	{FZ_TARA, 2, {0x243f, 0x2469}, 35,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KD_TAR, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_GI,
	  FZ_GE2, FZ_SA3, FZ_SHI4, FZ_CHI, FZ_NI3, FZ_BI, FZ_MI, FZ_MI4,
	  FZ_ME2, FZ_RI2}},

	/* Tari-KeiDouYouShi */
	{FZ_TARI, 2, {0x243f, 0x246a}, 1, {HN_KD_TAR}},

	/* Tari-KeiDou,JoDou-NaruTaru-MiMei */
	{FZ_NARE, 2, {0x244a, 0x246c}, 35,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KEIDOU, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_GI,
	  FZ_GE2, FZ_SA3, FZ_SHI4, FZ_CHI, FZ_NI3, FZ_BI, FZ_MI, FZ_MI4,
	  FZ_ME2, FZ_RI2}},
	{FZ_TARE, 2, {0x243f, 0x246c}, 35,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KD_TAR, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_GI,
	  FZ_GE2, FZ_SA3, FZ_SHI4, FZ_CHI, FZ_NI3, FZ_BI, FZ_MI, FZ_MI4,
	  FZ_ME2, FZ_RI2}},

	/* JoDou-Naru-Tai */
	{FZ_NARU, 2, {0x244a, 0x246b}, 35,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KEIDOU, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_GI,
	  FZ_GE2, FZ_SA3, FZ_SHI4, FZ_CHI, FZ_NI3, FZ_BI, FZ_MI, FZ_MI4,
	  FZ_ME2, FZ_RI2}},

	/* Tari-KeiDou,JoDou-Taru-Tai */
	{FZ_TARU, 2, {0x243f, 0x246b}, 35,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KD_TAR, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_GI,
	  FZ_GE2, FZ_SA3, FZ_SHI4, FZ_CHI, FZ_NI3, FZ_BI, FZ_MI, FZ_MI4,
	  FZ_ME2, FZ_RI2}},

	/* JoDou-Masu-Ka-Sure */
	{FZ_SURE, 2, {0x2439, 0x246c}, 1, {FZ_MA3}},

	/* JoDou-Masu-ShiTai */
	{FZ_SU2, 1, {0x2439}, 1, {FZ_MA3}},

	/* JoDou-Masu-Mi1Mei-Se */
	{FZ_SE, 1, {0x243b}, 1, {FZ_MA3}},

	/* JoDou-Mai-ShiTai */
	{FZ_MAI, 2, {0x245e, 0x2424}, 54,
	 {HN_1DOU, HN_SI_SI, FZ_I8, FZ_IKU, FZ_IKE, FZ_U3, FZ_E, FZ_OKU,
	  FZ_OKE, FZ_ORU, FZ_ORE, FZ_KANE, FZ_GARU, FZ_GARE, FZ_KU3, FZ_KURU,
	  FZ_KURE3, FZ_GU, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE, FZ_SHI7, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_JI2, FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU,
	  FZ_ZURU, FZ_SE2, FZ_SE4, FZ_TSU, FZ_TE, FZ_DEKI, FZ_NARU2, FZ_NARE2,
	  FZ_NU2, FZ_NE, FZ_BU, FZ_BE, FZ_MI3, FZ_MU, FZ_ME, FZ_YARU, FZ_YARE,
	  FZ_YORU, FZ_RARE, FZ_RU, FZ_RU2, FZ_RE2, FZ_RE5}},

	/* JoDou-Beshi-Tai-Ki */
	{FZ_KI, 1, {0x242d}, 3, {FZ_SUBE, FZ_ZUBE, FZ_BE2}},

	/* JoDou-Beshi-Shi */
	{FZ_SHI, 1, {0x2437}, 3, {FZ_SUBE, FZ_ZUBE, FZ_BE2}},

	/* JoDou-Beshi-You1-Ku */
	{FZ_KU, 1, {0x242f}, 3, {FZ_SUBE, FZ_ZUBE, FZ_BE2}},

	/* JoDou-N-Shi2Tai2-Nu */
	{FZ_NU, 1, {0x244c}, 53,
	 {HN_1DOU, HN_KO_KO, HN_SE_SE, FZ_I8, FZ_IKA, FZ_IKE, FZ_E, FZ_OKA,
	  FZ_OKE, FZ_ORA, FZ_ORE, FZ_KA, FZ_KANE, FZ_GA, FZ_GARA, FZ_GARE,
	  FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SA, FZ_SASE, FZ_SARE, FZ_SHIMAE,
	  FZ_SHIMAWA, FZ_SHIME, FZ_SE, FZ_SE2, FZ_SE3, FZ_SE4, FZ_ZE, FZ_TA2,
	  FZ_TARA, FZ_TE, FZ_DEKI, FZ_NA3, FZ_NARA2, FZ_NARE2, FZ_NE, FZ_BA,
	  FZ_BE, FZ_MA, FZ_MI3, FZ_ME, FZ_YARA, FZ_YARE, FZ_YORA, FZ_RA2,
	  FZ_RA3, FZ_RARE, FZ_RE2, FZ_RE5, FZ_WA}},

	/* JoDou-N-You2-Tai1 */
	{FZ_N, 1, {0x2473}, 53,
	 {HN_1DOU, HN_KO_KO, HN_SE_SE, FZ_I8, FZ_IKA, FZ_IKE, FZ_E, FZ_OKA,
	  FZ_OKE, FZ_ORA, FZ_ORE, FZ_KA, FZ_KANE, FZ_GA, FZ_GARA, FZ_GARE,
	  FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SA, FZ_SASE, FZ_SARE, FZ_SHIMAE,
	  FZ_SHIMAWA, FZ_SHIME, FZ_SE, FZ_SE2, FZ_SE3, FZ_SE4, FZ_ZE, FZ_TA2,
	  FZ_TARA, FZ_TE, FZ_DEKI, FZ_NA3, FZ_NARA2, FZ_NARE2, FZ_NE, FZ_BA,
	  FZ_BE, FZ_MA, FZ_MI3, FZ_ME, FZ_YARA, FZ_YARE, FZ_YORA, FZ_RA2,
	  FZ_RA3, FZ_RARE, FZ_RE2, FZ_RE5, FZ_WA}},

	/* JoDou-Nai-ShiTai */
	{FZ_I, 1, {0x2424}, 2, {FZ_NA5, FZ_YASHINA}},

	/* JoDou-Ta,Da-ShiTai */
	{FZ_TA, 1, {0x243f}, 51,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I3, FZ_I8, FZ_IKE, FZ_IXTSU, FZ_E,
	  FZ_OI, FZ_OKE, FZ_OXTSU, FZ_ORE, FZ_KAXTSU, FZ_KAXTSU2, FZ_KANE,
	  FZ_GAXTSU, FZ_GARE, FZ_KI6, FZ_KURE3, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE,
	  FZ_SHI3, FZ_SHI7, FZ_SHIMAE, FZ_SHIMAXTSU, FZ_SHIME, FZ_JI2, FZ_SE2,
	  FZ_SE4, FZ_DAXTSU, FZ_XTSU, FZ_XTSU2, FZ_TE, FZ_TE2, FZ_DE4, FZ_DEKI,
	  FZ_DESHI, FZ_NAXTSU, FZ_NARE2, FZ_NE, FZ_BE, FZ_MI3, FZ_ME,
	  FZ_YAXTSU, FZ_YARE, FZ_YOXTSU, FZ_RARE, FZ_RE2, FZ_RE5}},
	{FZ_DA2, 1, {0x2440}, 2, {FZ_I2, FZ_N2}},

	/* JoDouDenBun-Soudesu,Denbun-Souda-Shi */
	{FZ_DESU, 2, {0x2447, 0x2439}, 1, {FZ_SOU}},
	{FZ_DA3, 1, {0x2440}, 1, {FZ_SOU}},

	/* Denbun-Souda-You1-Soude */
	{FZ_DE2, 1, {0x2447}, 1, {FZ_SOU}},

	/* JoDou-Zu-YouShiKa-Zu */
	{FZ_ZU, 1, {0x243a}, 53,
	 {HN_1DOU, HN_KO_KO, HN_SE_SE, FZ_I8, FZ_IKA, FZ_IKE, FZ_E, FZ_OKA,
	  FZ_OKE, FZ_ORA, FZ_ORE, FZ_KA, FZ_KANE, FZ_KARA, FZ_GA, FZ_GARA,
	  FZ_GARE, FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SA, FZ_SASE, FZ_SARE,
	  FZ_SHIMAE, FZ_SHIMAWA, FZ_SHIME, FZ_SE2, FZ_SE3, FZ_SE4, FZ_ZE,
	  FZ_TA2, FZ_TARA, FZ_TE, FZ_DEKI, FZ_NA3, FZ_NARA2, FZ_NARE2, FZ_NE,
	  FZ_BA, FZ_BE, FZ_MA, FZ_MI3, FZ_ME, FZ_YARA, FZ_YARE, FZ_YORA,
	  FZ_RA2, FZ_RA3, FZ_RARE, FZ_RE2, FZ_RE5, FZ_WA}},

	/* JoDou-Zu-Ka-Zun */
	{FZ_ZUN, 2, {0x243a, 0x2473}, 53,
	 {HN_1DOU, HN_KO_KO, HN_SE_SE, FZ_I8, FZ_IKA, FZ_IKE, FZ_E, FZ_OKA,
	  FZ_OKE, FZ_ORA, FZ_ORE, FZ_KA, FZ_KANE, FZ_KARA, FZ_GA, FZ_GARA,
	  FZ_GARE, FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SA, FZ_SASE, FZ_SARE,
	  FZ_SHIMAE, FZ_SHIMAWA, FZ_SHIME, FZ_SE2, FZ_SE3, FZ_SE4, FZ_ZE,
	  FZ_TA2, FZ_TARA, FZ_TE, FZ_DEKI, FZ_NA3, FZ_NARA2, FZ_NARE2, FZ_NE,
	  FZ_BA, FZ_BE, FZ_MA, FZ_MI3, FZ_ME, FZ_YARA, FZ_YARE, FZ_YORA,
	  FZ_RA2, FZ_RA3, FZ_RARE, FZ_RE2, FZ_RE5, FZ_WA}},

	/* JoDou-Zu-Tai-Zaru */
	{FZ_ZARU, 2, {0x2436, 0x246b}, 53,
	 {HN_1DOU, HN_KO_KO, HN_SE_SE, FZ_I8, FZ_IKA, FZ_IKE, FZ_E, FZ_OKA,
	  FZ_OKE, FZ_ORA, FZ_ORE, FZ_KA, FZ_KANE, FZ_KARA, FZ_GA, FZ_GARA,
	  FZ_GARE, FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SA, FZ_SASE, FZ_SARE,
	  FZ_SHIMAE, FZ_SHIMAWA, FZ_SHIME, FZ_SE2, FZ_SE3, FZ_SE4, FZ_ZE,
	  FZ_TA2, FZ_TARA, FZ_TE, FZ_DEKI, FZ_NA3, FZ_NARA2, FZ_NARE2, FZ_NE,
	  FZ_BA, FZ_BE, FZ_MA, FZ_MI3, FZ_ME, FZ_YARA, FZ_YARE, FZ_YORA,
	  FZ_RA2, FZ_RA3, FZ_RARE, FZ_RE2, FZ_RE5, FZ_WA}},

	/* JoDou-Gotoshi-Tai-Gotoku */
	{FZ_KU2, 1, {0x242f}, 1, {FZ_GOTO}},

	/* JoDou-Gotoshi-You-Gotoku */
	{FZ_KI2, 1, {0x242d}, 1, {FZ_GOTO}},

	/* JoDou-You,U-ShiTai */
	{FZ_YOU, 2, {0x2468, 0x2426}, 35,
	 {HN_1DOU, HN_KO_KO, HN_SI_SI, FZ_I8, FZ_IKE, FZ_E, FZ_OKE, FZ_ORE,
	  FZ_KANE, FZ_GARE, FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SASE, FZ_SARE,
	  FZ_SHI7, FZ_SHIMAE, FZ_SHIME, FZ_JI2, FZ_SE2, FZ_SE4, FZ_TE, FZ_TE2,
	  FZ_DE4, FZ_DEKI, FZ_NARE2, FZ_NE, FZ_BE, FZ_MI3, FZ_ME, FZ_YARE,
	  FZ_RARE, FZ_RE2, FZ_RE5}},
	{FZ_U, 1, {0x2426}, 17,
	 {FZ_IKO, FZ_O, FZ_OKO, FZ_ORO, FZ_GARO, FZ_KO, FZ_GO, FZ_SHIMAO,
	  FZ_SO, FZ_TO2, FZ_NARO, FZ_NO, FZ_BO, FZ_MO, FZ_YARO, FZ_YORO,
	  FZ_RO}},

	/* SuiryouJoDou-U-ShiTai */
	{FZ_U2, 1, {0x2426}, 6,
	 {FZ_KARO, FZ_SHO, FZ_DARO, FZ_DESHO, FZ_RO2, FZ_RO3}},

	/* JoDou-Dearu,Aru-KaMei */
	{FZ_RE, 1, {0x246c}, 1, {FZ_A}},

	/* JoDou-Dearu,Aru-ShiTai */
	{FZ_RU, 1, {0x246b}, 1, {FZ_A}},

	/* JoDou-Dearu,Aru-You */
	{FZ_RI, 1, {0x246a}, 1, {FZ_A}},

	/* JoDou-Ki-Shi */
	{FZ_KI3, 1, {0x242d}, 12,
	 {FZ_I4, FZ_KARI, FZ_GARI, FZ_KI4, FZ_GI, FZ_SHI4, FZ_TARI, FZ_CHI,
	  FZ_NI3, FZ_BI, FZ_MI, FZ_RI2}},

	/* JoDou-Ki-Tai-Shi */
	{FZ_SHI2, 1, {0x2437}, 19,
	 {HN_KO_KO, HN_KI_KI, HN_SE_SE, FZ_I4, FZ_KARI, FZ_GARI, FZ_KI4,
	  FZ_KI6, FZ_GI, FZ_KO2, FZ_SHI4, FZ_SE3, FZ_ZE, FZ_TARI, FZ_CHI,
	  FZ_NI3, FZ_BI, FZ_MI, FZ_RI2}},

	/* 5DanSaHenTouShiTai */
	{FZ_U3, 1, {0x2426}, 1, {HN_WA5DOU}},
	{FZ_RU2, 1, {0x246b}, 4, {HN_RA5DOU, HN_KU_KU, HN_SU_SU, HN_RA_KUD}},
	{FZ_MU, 1, {0x2460}, 1, {HN_MA5DOU}},
	{FZ_BU, 1, {0x2456}, 1, {HN_BA5DOU}},
	{FZ_NU2, 1, {0x244c}, 1, {HN_NA5DOU}},
	{FZ_TSU, 1, {0x2444}, 1, {HN_TA5DOU}},
	{FZ_SU3, 1, {0x2439}, 1, {HN_SA5DOU}},
	{FZ_GU, 1, {0x2430}, 1, {HN_GA5DOU}},
	{FZ_KU3, 1, {0x242f}, 2, {HN_KA5DOU, HN_KA_IKU}},
	{FZ_ZURU, 2, {0x243a, 0x246b}, 1, {HN_ZAHDOU}},
	{FZ_JIRU, 2, {0x2438, 0x246b}, 1, {HN_ZAHDOU}},
	{FZ_IKU, 2, {0x2424, 0x242f}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_OKU, 2, {0x242a, 0x242f}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_ORU, 2, {0x242a, 0x246b}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_YORU, 2, {0x2468, 0x246b}, 1, {FZ_NI6}},
	{FZ_YARU, 2, {0x2464, 0x246b}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_NARU2, 2, {0x244a, 0x246b}, 10,
	 {FZ_KU4, FZ_KU5, FZ_DEMO, FZ_TO, FZ_TO4, FZ_NI, FZ_NI6, FZ_HA, FZ_BA3,
	  FZ_MO2}},
	{FZ_KURU, 2, {0x242f, 0x246b}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_SURU, 2, {0x2439, 0x246b}, 24,
	 {HN_KANA, HN_SAHDOU, HN_1YOU, HN_SA_MEI, HN_SA_M_S, FZ_I5, FZ_GARI2,
	  FZ_KI5, FZ_GI2, FZ_KU4, FZ_KU5, FZ_SHI5, FZ_SHI6, FZ_JI, FZ_CHI2,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_NI, FZ_NI4, FZ_NI6, FZ_BI2, FZ_MI2,
	  FZ_RI3}},
	{FZ_SHIMAU, 3, {0x2437, 0x245e, 0x2426}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_GARU, 2, {0x242c, 0x246b}, 2, {HN_KEIYOU, FZ_TA4}},

	/* SaHenMi3Se */
	{FZ_SE3, 1, {0x243b}, 11,
	 {HN_KANA, HN_SAHDOU, HN_SA_MEI, HN_SA_M_S, FZ_KU4, FZ_KU5, FZ_TO,
	  FZ_TO3, FZ_TO4, FZ_NI, FZ_NI6}},
	{FZ_ZE, 1, {0x243c}, 1, {HN_ZAHDOU}},

	/* 5KaMei */
	{FZ_E, 1, {0x2428}, 1, {HN_WA5DOU}},
	{FZ_RE2, 1, {0x246c}, 2, {HN_RA5DOU, HN_RA_KUD}},
	{FZ_ME, 1, {0x2461}, 1, {HN_MA5DOU}},
	{FZ_BE, 1, {0x2459}, 1, {HN_BA5DOU}},
	{FZ_NE, 1, {0x244d}, 1, {HN_NA5DOU}},
	{FZ_TE, 1, {0x2446}, 1, {HN_TA5DOU}},
	{FZ_SE2, 1, {0x243b}, 1, {HN_SA5DOU}},
	{FZ_GE, 1, {0x2432}, 1, {HN_GA5DOU}},
	{FZ_KE, 1, {0x2431}, 2, {HN_KA5DOU, HN_KA_IKU}},
	{FZ_IKE, 2, {0x2424, 0x2431}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_OKE, 2, {0x242a, 0x2431}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_ORE, 2, {0x242a, 0x246c}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_NARE2, 2, {0x244a, 0x246c}, 10,
	 {FZ_KU4, FZ_KU5, FZ_DEMO, FZ_TO, FZ_TO4, FZ_NI, FZ_NI6, FZ_HA, FZ_BA3,
	  FZ_MO2}},
	{FZ_YARE, 2, {0x2464, 0x246c}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_SHIMAE, 3, {0x2437, 0x245e, 0x2428}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_GARE, 2, {0x242c, 0x246c}, 2, {HN_KEIYOU, FZ_TA4}},

	/* 5IBin2 */
	{FZ_N2, 1, {0x2473}, 3, {HN_NA5DOU, HN_BA5DOU, HN_MA5DOU}},
	{FZ_I2, 1, {0x2424}, 1, {HN_GA5DOU}},

	/* 5DanTouMi1 */
	{FZ_WA, 1, {0x246f}, 1, {HN_WA5DOU}},
	{FZ_RA3, 1, {0x2469}, 2, {HN_RA5DOU, HN_RA_KUD}},
	{FZ_MA, 1, {0x245e}, 1, {HN_MA5DOU}},
	{FZ_BA, 1, {0x2450}, 1, {HN_BA5DOU}},
	{FZ_NA3, 1, {0x244a}, 1, {HN_NA5DOU}},
	{FZ_TA2, 1, {0x243f}, 1, {HN_TA5DOU}},
	{FZ_SA, 1, {0x2435}, 1, {HN_SA5DOU}},
	{FZ_GA, 1, {0x242c}, 1, {HN_GA5DOU}},
	{FZ_KA, 1, {0x242b}, 2, {HN_KA5DOU, HN_KA_IKU}},
	{FZ_IKA, 2, {0x2424, 0x242b}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_OKA, 2, {0x242a, 0x242b}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_ORA, 2, {0x242a, 0x2469}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_YORA, 2, {0x2468, 0x2469}, 1, {FZ_NI6}},
	{FZ_NARA2, 2, {0x244a, 0x2469}, 10,
	 {FZ_KU4, FZ_KU5, FZ_DEMO, FZ_TO, FZ_TO4, FZ_NI, FZ_NI6, FZ_HA, FZ_BA3,
	  FZ_MO2}},
	{FZ_YARA, 2, {0x2464, 0x2469}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_SHIMAWA, 3, {0x2437, 0x245e, 0x246f}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_GARA, 2, {0x242c, 0x2469}, 2, {HN_KEIYOU, FZ_TA4}},

	/* 5DanTouMi */
	{FZ_O, 1, {0x242a}, 1, {HN_WA5DOU}},
	{FZ_RO, 1, {0x246d}, 1, {HN_RA5DOU}},
	{FZ_MO, 1, {0x2462}, 1, {HN_MA5DOU}},
	{FZ_BO, 1, {0x245c}, 1, {HN_BA5DOU}},
	{FZ_NO, 1, {0x244e}, 1, {HN_NA5DOU}},
	{FZ_TO2, 1, {0x2448}, 1, {HN_TA5DOU}},
	{FZ_SO, 1, {0x243d}, 1, {HN_SA5DOU}},
	{FZ_GO, 1, {0x2434}, 1, {HN_GA5DOU}},
	{FZ_KO, 1, {0x2433}, 2, {HN_KA5DOU, HN_KA_IKU}},
	{FZ_IKO, 2, {0x2424, 0x2433}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_OKO, 2, {0x242a, 0x2433}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_ORO, 2, {0x242a, 0x246d}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_YORO, 2, {0x2468, 0x246d}, 1, {FZ_NI6}},
	{FZ_NARO, 2, {0x244a, 0x246d}, 10,
	 {FZ_KU4, FZ_KU5, FZ_DEMO, FZ_TO, FZ_TO4, FZ_NI, FZ_NI6, FZ_HA, FZ_BA3,
	  FZ_MO2}},
	{FZ_YARO, 2, {0x2464, 0x246d}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_SHIMAO, 3, {0x2437, 0x245e, 0x242a}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_GARO, 2, {0x242c, 0x246d}, 2, {HN_KEIYOU, FZ_TA4}},

	/* KeiDou,JoDou-Da-TouMi */
	{FZ_DARO, 2, {0x2440, 0x246d}, 88,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO,
	  HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4,
	  FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_KA3, FZ_KARA2, FZ_GARI,
	  FZ_GARU, FZ_KI, FZ_KI4, FZ_KIRI, FZ_GI, FZ_KU3, FZ_KURAI, FZ_KURU,
	  FZ_GU, FZ_GURAI, FZ_GE2, FZ_KOSO, FZ_SA3, FZ_SHI4, FZ_SHIMAU,
	  FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_SOU2, FZ_TA, FZ_DA2,
	  FZ_DAKE, FZ_CHI, FZ_TSU, FZ_TO4, FZ_NAZO, FZ_NADO, FZ_NARU2, FZ_NI3,
	  FZ_NI6, FZ_NU, FZ_NU2, FZ_NO2, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BU,
	  FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME2, FZ_YARU,
	  FZ_YUE, FZ_YOU2, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},
	{FZ_DESHO, 3, {0x2447, 0x2437, 0x2467}, 100,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1YOU,
	  HN_SA_MEI, HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_I, FZ_I4, FZ_I5, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_KA3,
	  FZ_KARA2, FZ_GARI, FZ_GARI2, FZ_GARU, FZ_KI, FZ_KI4, FZ_KI5, FZ_KIRI,
	  FZ_GI, FZ_GI2, FZ_KU3, FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2,
	  FZ_KOSO, FZ_SA3, FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_SHIMAU, FZ_JI,
	  FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_SOU2, FZ_TA,
	  FZ_DA2, FZ_DAKE, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TO4, FZ_NAZO, FZ_NADO,
	  FZ_NARU2, FZ_NI3, FZ_NI4, FZ_NU2, FZ_NO2, FZ_NOMI, FZ_BAKARI, FZ_BI,
	  FZ_BI2, FZ_BU, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI,
	  FZ_MU, FZ_ME2, FZ_YARU, FZ_YUE, FZ_YOU2, FZ_YORU, FZ_RI2, FZ_RI3,
	  FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},
	{FZ_SHO, 2, {0x2437, 0x2467}, 1, {FZ_MA3}},
	{FZ_KARO, 2, {0x242b, 0x246d}, 6,
	 {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_NA5, FZ_YASHINA, FZ_RASHI}},
	{FZ_RO2, 1, {0x246d}, 1, {FZ_A}},
	{FZ_RO3, 1, {0x246d}, 2, {FZ_TA, FZ_DA2}},

	/* JoDou-Desu-Kan */
	{FZ_DE3, 1, {0x2447}, 76,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1YOU,
	  HN_SA_MEI, HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_I, FZ_I4, FZ_I5, FZ_I7, FZ_KA3, FZ_KARA2, FZ_GARI, FZ_GARI2,
	  FZ_KI, FZ_KI4, FZ_KI5, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KURAI, FZ_GURAI,
	  FZ_GE2, FZ_KOSO, FZ_SA3, FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_JI, FZ_ZUTSU,
	  FZ_SOU2, FZ_TA, FZ_DA2, FZ_DAKE, FZ_CHI, FZ_CHI2, FZ_TO4, FZ_NAZO,
	  FZ_NADO, FZ_NI3, FZ_NI4, FZ_NO2, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2,
	  FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_ME2, FZ_YUE,
	  FZ_YOU2, FZ_RI2, FZ_RI3, FZ_N}},

	/* 5DanTouSoku,Hatsu */
	{FZ_XTSU, 1, {0x2443}, 5,
	 {HN_TA5DOU, HN_RA5DOU, HN_WA5DOU, HN_KA_IKU, HN_RA_KUD}},
	{FZ_I3, 1, {0x2424}, 1, {HN_KA5DOU}},
	{FZ_SHI3, 1, {0x2437}, 3, {HN_SA5DOU, FZ_DE3, FZ_MA3}},
	{FZ_IXTSU, 2, {0x2424, 0x2443}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_OI, 2, {0x242a, 0x2424}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_OXTSU, 2, {0x242a, 0x2443}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_YOXTSU, 2, {0x2468, 0x2443}, 1, {FZ_NI6}},
	{FZ_NAXTSU, 2, {0x244a, 0x2443}, 10,
	 {FZ_KU4, FZ_KU5, FZ_DEMO, FZ_TO, FZ_TO4, FZ_NI, FZ_NI6, FZ_HA, FZ_BA3,
	  FZ_MO2}},
	{FZ_YAXTSU, 2, {0x2464, 0x2443}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_SHIMAXTSU, 3, {0x2437, 0x245e, 0x2443}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_GAXTSU, 2, {0x242c, 0x2443}, 2, {HN_KEIYOU, FZ_TA4}},
	{FZ_KAXTSU, 2, {0x242b, 0x2443}, 2, {FZ_NA5, FZ_YASHINA}},
	{FZ_XTSU2, 1, {0x2443}, 1, {FZ_A}},
	{FZ_DAXTSU, 2, {0x2440, 0x2443}, 88,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO,
	  HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4,
	  FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_KA3, FZ_KARA2, FZ_GARI,
	  FZ_GARU, FZ_KI, FZ_KI4, FZ_KIRI, FZ_GI, FZ_KU3, FZ_KURAI, FZ_KURU,
	  FZ_GU, FZ_GURAI, FZ_GE2, FZ_KOSO, FZ_SA3, FZ_SHI4, FZ_SHIMAU,
	  FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_SOU2, FZ_TA, FZ_DA2,
	  FZ_DAKE, FZ_CHI, FZ_TSU, FZ_TO4, FZ_NAZO, FZ_NADO, FZ_NARU2, FZ_NI3,
	  FZ_NI6, FZ_NU, FZ_NU2, FZ_NO2, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BU,
	  FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME2, FZ_YARU,
	  FZ_YUE, FZ_YOU2, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},
	{FZ_DESHI, 2, {0x2447, 0x2437}, 1, {FZ_SOU}},
	{FZ_KAXTSU2, 2, {0x242b, 0x2443}, 4,
	 {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},

	/* 5DanTouYou */
	{FZ_I4, 1, {0x2424}, 1, {HN_WA5DOU}},
	{FZ_RI2, 1, {0x246a}, 2, {HN_RA5DOU, HN_RA_KUD}},
	{FZ_MI, 1, {0x245f}, 1, {HN_MA5DOU}},
	{FZ_BI, 1, {0x2453}, 1, {HN_BA5DOU}},
	{FZ_SHI4, 1, {0x2437}, 1, {HN_SA5DOU}},
	{FZ_NI3, 1, {0x244b}, 1, {HN_NA5DOU}},
	{FZ_CHI, 1, {0x2441}, 1, {HN_TA5DOU}},
	{FZ_GI, 1, {0x242e}, 1, {HN_GA5DOU}},
	{FZ_KI4, 1, {0x242d}, 2, {HN_KA5DOU, HN_KA_IKU}},
	{FZ_GARI, 2, {0x242c, 0x246a}, 2, {HN_KEIYOU, FZ_TA4}},

	/* 5DanTouYouHiTai */
	{FZ_IKI, 2, {0x2424, 0x242d}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_OKI, 2, {0x242a, 0x242d}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_ORI, 2, {0x242a, 0x246a}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_YORI, 2, {0x2468, 0x246a}, 1, {FZ_NI6}},
	{FZ_NARI, 2, {0x244a, 0x246a}, 10,
	 {FZ_KU4, FZ_KU5, FZ_DEMO, FZ_TO, FZ_TO4, FZ_NI, FZ_NI6, FZ_HA, FZ_BA3,
	  FZ_MO2}},
	{FZ_YARI, 2, {0x2464, 0x246a}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_SHIMAI, 3, {0x2437, 0x245e, 0x2424}, 2, {FZ_TE2, FZ_DE4}},

	/* O-5DanTouYou */
	{FZ_I5, 1, {0x2424}, 1, {HN_WA5YOU}},
	{FZ_RI3, 1, {0x246a}, 1, {HN_RA5YOU}},
	{FZ_MI2, 1, {0x245f}, 1, {HN_MA5YOU}},
	{FZ_BI2, 1, {0x2453}, 1, {HN_BA5YOU}},
	{FZ_SHI5, 1, {0x2437}, 1, {HN_SA5YOU}},
	{FZ_NI4, 1, {0x244b}, 1, {HN_NA5YOU}},
	{FZ_CHI2, 1, {0x2441}, 1, {HN_TA5YOU}},
	{FZ_GI2, 1, {0x242e}, 1, {HN_GA5YOU}},
	{FZ_KI5, 1, {0x242d}, 1, {HN_KA5YOU}},
	{FZ_GARI2, 2, {0x242c, 0x246a}, 2, {HN_KEIYOU, FZ_TA4}},
	{FZ_SHI6, 1, {0x2437}, 1, {HN_SAHYOU}},
	{FZ_JI, 1, {0x2438}, 1, {HN_ZAHYOU}},

	/* O-5DanTouMi1 */
	{FZ_WA2, 1, {0x246f}, 1, {HN_WA5YOU}},
	{FZ_RA4, 1, {0x2469}, 1, {HN_RA5YOU}},
	{FZ_MA2, 1, {0x245e}, 1, {HN_MA5YOU}},
	{FZ_BA2, 1, {0x2450}, 1, {HN_BA5YOU}},
	{FZ_NA4, 1, {0x244a}, 1, {HN_NA5YOU}},
	{FZ_TA3, 1, {0x243f}, 1, {HN_TA5YOU}},
	{FZ_SA2, 1, {0x2435}, 1, {HN_SA5YOU}},
	{FZ_GA2, 1, {0x242c}, 1, {HN_GA5YOU}},
	{FZ_KA2, 1, {0x242b}, 1, {HN_KA5YOU}},
	{FZ_GARA2, 2, {0x242c, 0x2469}, 2, {HN_KEIYOU, FZ_TA4}},

	/* Yoru-KaMei */
	{FZ_YORE, 2, {0x2468, 0x246c}, 1, {FZ_NI6}},

	/* Kuru-Mi */
	{FZ_KO2, 1, {0x2433}, 2, {FZ_TE2, FZ_DE4}},

	/* Kuru-You */
	{FZ_KI6, 1, {0x242d}, 2, {FZ_TE2, FZ_DE4}},

	/* SaHenMi1You-Shi */
	{FZ_SHI7, 1, {0x2437}, 24,
	 {HN_KANA, HN_SAHDOU, HN_1YOU, HN_SA_MEI, HN_SA_M_S, FZ_I5, FZ_GARI2,
	  FZ_KI5, FZ_GI2, FZ_KU4, FZ_KU5, FZ_SHI5, FZ_SHI6, FZ_JI, FZ_CHI2,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_NI, FZ_NI4, FZ_NI6, FZ_BI2, FZ_MI2,
	  FZ_RI3}},
	{FZ_JI2, 1, {0x2438}, 1, {HN_ZAHDOU}},

	/* 1DanTouShiTai */
	{FZ_RU3, 1, {0x246b}, 30,
	 {HN_1DOU, FZ_I8, FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_KANE, FZ_GARE,
	  FZ_KURE3, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE, FZ_SHIMAE, FZ_SHIME,
	  FZ_SE2, FZ_SE4, FZ_TE, FZ_TE2, FZ_DE4, FZ_DEKI, FZ_NARE2, FZ_NE,
	  FZ_BE, FZ_MI3, FZ_ME, FZ_YARE, FZ_RARE, FZ_RE2, FZ_RE5}},

	/* 1DanTouMei-Yo */
	{FZ_YO, 1, {0x2468}, 30,
	 {HN_1DOU, FZ_I8, FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_KANE, FZ_GARE,
	  FZ_KURE3, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE, FZ_SHIMAE, FZ_SHIME,
	  FZ_SE2, FZ_SE4, FZ_TE, FZ_TE2, FZ_DE4, FZ_DEKI, FZ_NARE2, FZ_NE,
	  FZ_BE, FZ_MI3, FZ_ME, FZ_YARE, FZ_RARE, FZ_RE2, FZ_RE5}},
	{FZ_SEYO, 2, {0x243b, 0x2468}, 11,
	 {HN_KANA, HN_SAHDOU, HN_SA_MEI, HN_SA_M_S, FZ_KU4, FZ_KU5, FZ_TO,
	  FZ_TO3, FZ_TO4, FZ_NI, FZ_NI6}},
	{FZ_ZEYO, 2, {0x243c, 0x2468}, 1, {HN_ZAHDOU}},
	{FZ_JIYO, 2, {0x2438, 0x2468}, 1, {HN_ZAHDOU}},

	/* 1DanTouMei-Ro */
	{FZ_RO4, 1, {0x246d}, 30,
	 {HN_1DOU, FZ_I8, FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_KANE, FZ_GARE,
	  FZ_KURE3, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE, FZ_SHIMAE, FZ_SHIME,
	  FZ_SE2, FZ_SE4, FZ_TE, FZ_TE2, FZ_DE4, FZ_DEKI, FZ_NARE2, FZ_NE,
	  FZ_BE, FZ_MI3, FZ_ME, FZ_YARE, FZ_RARE, FZ_RE2, FZ_RE5}},
	{FZ_KOI, 2, {0x2433, 0x2424}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_SHIRO, 2, {0x2437, 0x246d}, 11,
	 {HN_KANA, HN_SAHDOU, HN_SA_MEI, HN_SA_M_S, FZ_KU4, FZ_KU5, FZ_TO,
	  FZ_TO3, FZ_TO4, FZ_NI, FZ_NI6}},
	{FZ_JIRO, 2, {0x2438, 0x246d}, 1, {HN_ZAHDOU}},
	{FZ_I6, 1, {0x2424}, 1, {HN_KO_KO}},
	{FZ_KURE, 2, {0x242f, 0x246c}, 2, {FZ_TE2, FZ_DE4}},

	/* Kureru,Kaneru-KanMiYou */
	{FZ_KURE3, 2, {0x242f, 0x246c}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_KANE, 2, {0x242b, 0x244d}, 27,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I4, FZ_I8, FZ_KANE, FZ_GARI, FZ_KI4,
	  FZ_KI6, FZ_GI, FZ_KURE3, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI7,
	  FZ_SHIME, FZ_JI2, FZ_SE4, FZ_CHI, FZ_DEKI, FZ_NI3, FZ_BI, FZ_MI,
	  FZ_MI3, FZ_RARE, FZ_RI2, FZ_RE5}},
	{FZ_DEKI, 2, {0x2447, 0x242d}, 24,
	 {HN_KANA, HN_SAHDOU, HN_1YOU, HN_SA_MEI, HN_SA_M_S, FZ_I5, FZ_GARI2,
	  FZ_KI5, FZ_GI2, FZ_KU4, FZ_KU5, FZ_SHI5, FZ_SHI6, FZ_JI, FZ_CHI2,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_NI, FZ_NI4, FZ_NI6, FZ_BI2, FZ_MI2,
	  FZ_RI3}},

	/* 1DanTouKa */
	{FZ_RE3, 1, {0x246c}, 30,
	 {HN_1DOU, FZ_I8, FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_KANE, FZ_GARE,
	  FZ_KURE3, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE, FZ_SHIMAE, FZ_SHIME,
	  FZ_SE2, FZ_SE4, FZ_TE, FZ_TE2, FZ_DE4, FZ_DEKI, FZ_NARE2, FZ_NE,
	  FZ_BE, FZ_MI3, FZ_ME, FZ_YARE, FZ_RARE, FZ_RE2, FZ_RE5}},
	{FZ_KURE2, 2, {0x242f, 0x246c}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_SURE2, 2, {0x2439, 0x246c}, 24,
	 {HN_KANA, HN_SAHDOU, HN_1YOU, HN_SA_MEI, HN_SA_M_S, FZ_I5, FZ_GARI2,
	  FZ_KI5, FZ_GI2, FZ_KU4, FZ_KU5, FZ_SHI5, FZ_SHI6, FZ_JI, FZ_CHI2,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_NI, FZ_NI4, FZ_NI6, FZ_BI2, FZ_MI2,
	  FZ_RI3}},
	{FZ_ZURE, 2, {0x243a, 0x246c}, 1, {HN_ZAHDOU}},
	{FZ_JIRE, 2, {0x2438, 0x246c}, 1, {HN_ZAHDOU}},
	{FZ_RE4, 1, {0x246c}, 2, {HN_KU_KU, HN_SU_SU}},
	{FZ_NE2, 1, {0x244d}, 53,
	 {HN_1DOU, HN_KO_KO, HN_SE_SE, FZ_I8, FZ_IKA, FZ_IKE, FZ_E, FZ_OKA,
	  FZ_OKE, FZ_ORA, FZ_ORE, FZ_KA, FZ_KANE, FZ_GA, FZ_GARA, FZ_GARE,
	  FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SA, FZ_SASE, FZ_SARE, FZ_SHIMAE,
	  FZ_SHIMAWA, FZ_SHIME, FZ_SE, FZ_SE2, FZ_SE3, FZ_SE4, FZ_ZE, FZ_TA2,
	  FZ_TARA, FZ_TE, FZ_DEKI, FZ_NA3, FZ_NARA2, FZ_NARE2, FZ_NE, FZ_BA,
	  FZ_BE, FZ_MA, FZ_MI3, FZ_ME, FZ_YARA, FZ_YARE, FZ_YORA, FZ_RA2,
	  FZ_RA3, FZ_RARE, FZ_RE2, FZ_RE5, FZ_WA}},
	{FZ_KERE, 2, {0x2431, 0x246c}, 6,
	 {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_NA5, FZ_YASHINA, FZ_RASHI}},
	{FZ_ZARE, 2, {0x2436, 0x246c}, 53,
	 {HN_1DOU, HN_KO_KO, HN_SE_SE, FZ_I8, FZ_IKA, FZ_IKE, FZ_E, FZ_OKA,
	  FZ_OKE, FZ_ORA, FZ_ORE, FZ_KA, FZ_KANE, FZ_KARA, FZ_GA, FZ_GARA,
	  FZ_GARE, FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SA, FZ_SASE, FZ_SARE,
	  FZ_SHIMAE, FZ_SHIMAWA, FZ_SHIME, FZ_SE2, FZ_SE3, FZ_SE4, FZ_ZE,
	  FZ_TA2, FZ_TARA, FZ_TE, FZ_DEKI, FZ_NA3, FZ_NARA2, FZ_NARE2, FZ_NE,
	  FZ_BA, FZ_BE, FZ_MA, FZ_MI3, FZ_ME, FZ_YARA, FZ_YARE, FZ_YORA,
	  FZ_RA2, FZ_RA3, FZ_RARE, FZ_RE2, FZ_RE5, FZ_WA}},

	/* KeiYou1-Ku */
	{FZ_KU4, 1, {0x242f}, 4, {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},
	{FZ_KU5, 1, {0x242f}, 2, {FZ_NA5, FZ_YASHINA}},

	/* KeiYou3-Yuu */
	{FZ_XYUU, 2, {0x2465, 0x2426}, 1, {FZ_RASHI}},

	/* KeiTaiShi */
	{FZ_I7, 1, {0x2424}, 4, {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},

	/* KeiBunShi */
	{FZ_SHI8, 1, {0x2437}, 2, {HN_KEIYOU, HN_KE_SED}},

	/* KeiBun,JoDou-Beshi-Mi-Kara */
	{FZ_KARA, 2, {0x242b, 0x2469}, 7,
	 {HN_KEIYOU, HN_KE_SED, FZ_SUBE, FZ_ZUBE, FZ_TA4, FZ_BE2, FZ_RASHI}},

	/* KeiBunYou */
	{FZ_KARI, 2, {0x242b, 0x246a}, 4,
	 {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},

	/* KeiBunTai1-Ki */
	{FZ_KI7, 1, {0x242d}, 4, {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},

	/* KeiBunTai2-Karu */
	{FZ_KARU, 2, {0x242b, 0x246b}, 4,
	 {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},

	/* KeiBunMei */
	{FZ_KARE, 2, {0x242b, 0x246c}, 4,
	 {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},

	/* Iru,Miru-KanMiYou */
	{FZ_I8, 1, {0x2424}, 2, {FZ_TE2, FZ_DE4}},
	{FZ_MI3, 1, {0x245f}, 2, {FZ_TE2, FZ_DE4}},

	/* JoDou-Aru-Kan */
	{FZ_A, 1, {0x2422}, 10,
	 {FZ_SAE, FZ_SURA, FZ_ZUTSU, FZ_DAKE, FZ_TSUTSU, FZ_TE2, FZ_DE, FZ_DE2,
	  FZ_DE4, FZ_NOMI}},

	/* JoDou-Gotoshi-Kan */
	{FZ_GOTO, 2, {0x2434, 0x2448}, 25,
	 {FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU, FZ_GU,
	  FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TSU, FZ_NARU2,
	  FZ_NU, FZ_NU2, FZ_NO3, FZ_BU, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2,
	  FZ_RU3}},

	/* JoDou-Rareru,Reru-KanMi1You */
	{FZ_RARE, 2, {0x2469, 0x246c}, 12,
	 {HN_1DOU, HN_KO_KO, HN_SE_SE, FZ_I8, FZ_KO2, FZ_SASE, FZ_SE3, FZ_SE4,
	  FZ_ZE, FZ_TE2, FZ_DE4, FZ_MI3}},
	{FZ_RE5, 1, {0x246c}, 29,
	 {FZ_IKA, FZ_OKA, FZ_ORA, FZ_KA, FZ_KA2, FZ_GA, FZ_GA2, FZ_GARA,
	  FZ_GARA2, FZ_SA, FZ_SA2, FZ_SHIMAWA, FZ_TA2, FZ_TA3, FZ_TARA, FZ_NA3,
	  FZ_NA4, FZ_NARA2, FZ_BA, FZ_BA2, FZ_MA, FZ_MA2, FZ_YARA, FZ_YORA,
	  FZ_RA2, FZ_RA3, FZ_RA4, FZ_WA, FZ_WA2}},
	{FZ_SARE, 2, {0x2435, 0x246c}, 28,
	 {HN_KANA, HN_SAHDOU, HN_SA_MEI, HN_SA_M_S, FZ_IKA, FZ_OKA, FZ_ORA,
	  FZ_KA, FZ_GA, FZ_GARA, FZ_KU4, FZ_KU5, FZ_SA, FZ_SHIMAWA, FZ_TA2,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_NA3, FZ_NARA2, FZ_NI, FZ_NI6, FZ_BA, FZ_MA,
	  FZ_YARA, FZ_YORA, FZ_RA3, FZ_WA}},

	/* JoDou-Seru,Saseru-KanMiYou */
	{FZ_SASE, 2, {0x2435, 0x243b}, 32,
	 {HN_KANA, HN_1DOU, HN_SAHDOU, HN_KO_KO, HN_SA_MEI, HN_SA_M_S, FZ_I8,
	  FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_GARE, FZ_KU4, FZ_KU5, FZ_KE, FZ_GE,
	  FZ_KO2, FZ_SHIMAE, FZ_SE2, FZ_TE, FZ_TO, FZ_TO3, FZ_TO4, FZ_NARE2,
	  FZ_NI, FZ_NI6, FZ_NE, FZ_BE, FZ_MI3, FZ_ME, FZ_YARE, FZ_RE2}},
	{FZ_SE4, 1, {0x243b}, 19,
	 {FZ_IKA, FZ_OKA, FZ_ORA, FZ_KA, FZ_GA, FZ_GARA, FZ_SA, FZ_SHIMAWA,
	  FZ_TA2, FZ_TARA, FZ_NA3, FZ_NARA2, FZ_BA, FZ_MA, FZ_YARA, FZ_YORA,
	  FZ_RA2, FZ_RA3, FZ_WA}},

	/* JoDou-Shimeru-KanMiYou */
	{FZ_SHIME, 2, {0x2437, 0x2461}, 19,
	 {FZ_IKA, FZ_OKA, FZ_ORA, FZ_KA, FZ_GA, FZ_GARA, FZ_SA, FZ_SHIMAWA,
	  FZ_TA2, FZ_TARA, FZ_NA3, FZ_NARA2, FZ_BA, FZ_MA, FZ_YARA, FZ_YORA,
	  FZ_RA2, FZ_RA3, FZ_WA}},

	/* DenBunJoDou-Souda,Soudesu-Kan */
	{FZ_SOU, 2, {0x243d, 0x2426}, 29,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TA,
	  FZ_DA, FZ_DA2, FZ_TSU, FZ_NARU2, FZ_NU2, FZ_BU, FZ_MU, FZ_YARU,
	  FZ_YORU, FZ_RU, FZ_RU2, FZ_RU3}},

	/* YouTaiJoDou-Souda,Soudesu-Kan */
	{FZ_SOU2, 2, {0x243d, 0x2426}, 38,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, HN_KEIYOU, HN_KEIDOU, HN_KE_SED, FZ_I4,
	  FZ_I8, FZ_IKI, FZ_OKI, FZ_ORI, FZ_KANE, FZ_GARI, FZ_KI4, FZ_KI6,
	  FZ_GI, FZ_KURE3, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI7, FZ_SHIMAI,
	  FZ_SHIME, FZ_JI2, FZ_SE4, FZ_CHI, FZ_DEKI, FZ_NARI, FZ_NI3, FZ_BI,
	  FZ_MI, FZ_MI3, FZ_YARI, FZ_YORI, FZ_RARE, FZ_RI, FZ_RI2, FZ_RE5}},

	/* JoDou-Tai-Kan */
	{FZ_TA4, 1, {0x243f}, 35,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I4, FZ_I8, FZ_IKI, FZ_OKI, FZ_ORI,
	  FZ_KANE, FZ_GARI, FZ_KI4, FZ_KI6, FZ_GI, FZ_KURE3, FZ_SASE, FZ_SARE,
	  FZ_SHI4, FZ_SHI7, FZ_SHIMAI, FZ_SHIME, FZ_JI2, FZ_SE4, FZ_CHI,
	  FZ_DEKI, FZ_NARI, FZ_NI3, FZ_BI, FZ_MI, FZ_MI3, FZ_YARI, FZ_YORI,
	  FZ_RARE, FZ_RI, FZ_RI2, FZ_RE5}},

	/* JoDou-Nai-Kan */
	{FZ_NA5, 1, {0x244a}, 63,
	 {HN_1DOU, HN_KO_KO, HN_SI_SI, FZ_I8, FZ_IKA, FZ_IKE, FZ_E, FZ_OKA,
	  FZ_OKE, FZ_ORA, FZ_ORE, FZ_KA, FZ_KANE, FZ_GA, FZ_GARA, FZ_GARE,
	  FZ_KU4, FZ_KU5, FZ_KURE3, FZ_KE, FZ_GE, FZ_KO2, FZ_SA, FZ_SAE,
	  FZ_SASE, FZ_SARE, FZ_SHI7, FZ_SHIKA, FZ_SHIMAE, FZ_SHIMAWA, FZ_SHIME,
	  FZ_JI2, FZ_SURA, FZ_SE2, FZ_SE4, FZ_TA2, FZ_TE, FZ_TE2, FZ_DE,
	  FZ_DE4, FZ_DEKI, FZ_DEMO, FZ_NA3, FZ_NARA2, FZ_NARE2, FZ_NI2, FZ_NE,
	  FZ_HA, FZ_BA, FZ_BE, FZ_MA, FZ_MI3, FZ_ME, FZ_MO2, FZ_YARA, FZ_YARE,
	  FZ_XYUU, FZ_YORA, FZ_RA3, FZ_RARE, FZ_RE2, FZ_RE5, FZ_WA}},
	{FZ_YASHINA, 3, {0x2464, 0x2437, 0x244a}, 22,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_GARE,
	  FZ_KI6, FZ_KE, FZ_GE, FZ_SHI7, FZ_SHIMAE, FZ_JI2, FZ_SE2, FZ_TE,
	  FZ_NARE2, FZ_NE, FZ_BE, FZ_ME, FZ_YARE, FZ_RE2}},

	/* JoDou-Beshi-Kan */
	{FZ_BE2, 1, {0x2459}, 25,
	 {FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_KARU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TARU, FZ_TSU,
	  FZ_NARU2, FZ_NU2, FZ_BU, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2,
	  FZ_RU3}},
	{FZ_SUBE, 2, {0x2439, 0x2459}, 24,
	 {HN_KANA, HN_SAHDOU, HN_1YOU, HN_SA_MEI, HN_SA_M_S, FZ_I5, FZ_GARI2,
	  FZ_KI5, FZ_GI2, FZ_KU4, FZ_KU5, FZ_SHI5, FZ_SHI6, FZ_JI, FZ_CHI2,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_NI, FZ_NI4, FZ_NI6, FZ_BI2, FZ_MI2,
	  FZ_RI3}},
	{FZ_ZUBE, 2, {0x243a, 0x2459}, 1, {HN_ZAHDOU}},

	/* JoDou-Masu-Kan */
	{FZ_MA3, 1, {0x245e}, 54,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I4, FZ_I8, FZ_I9, FZ_IKI, FZ_IKE,
	  FZ_E, FZ_OKI, FZ_OKE, FZ_ORI, FZ_ORE, FZ_KANE, FZ_GARI, FZ_GARE,
	  FZ_KI4, FZ_KI6, FZ_GI, FZ_KURE3, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE,
	  FZ_SHI4, FZ_SHI7, FZ_SHIMAI, FZ_SHIMAE, FZ_SHIME, FZ_JI2, FZ_SE2,
	  FZ_SE4, FZ_CHI, FZ_TE, FZ_TE2, FZ_DE4, FZ_DEKI, FZ_NARI, FZ_NARE2,
	  FZ_NI3, FZ_NE, FZ_BI, FZ_BE, FZ_MI, FZ_MI3, FZ_ME, FZ_YARI, FZ_YARE,
	  FZ_YORI, FZ_RARE, FZ_RI, FZ_RI2, FZ_RE2, FZ_RE5}},

	/* JoDou-Mitaida,Mitaidesu,Youda,Youdesu-Kan */
	{FZ_MITAI, 3, {0x245f, 0x243f, 0x2424}, 67,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU,
	  FZ_ORU, FZ_KARA2, FZ_GARI, FZ_GARU, FZ_KI4, FZ_KIRI, FZ_GI, FZ_KU3,
	  FZ_KURU, FZ_GU, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHIMAU, FZ_JIRU, FZ_SU3,
	  FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_DA2, FZ_CHI, FZ_TSU, FZ_NARU2,
	  FZ_NI3, FZ_NU, FZ_NU2, FZ_BAKARI, FZ_BI, FZ_BU, FZ_MADE, FZ_MI,
	  FZ_MI4, FZ_MU, FZ_ME2, FZ_YARU, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2,
	  FZ_RU3}},
	{FZ_YOU2, 2, {0x2468, 0x2426}, 30,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TA, FZ_DA2,
	  FZ_TSU, FZ_NA, FZ_NARU2, FZ_NU, FZ_NU2, FZ_NO3, FZ_BU, FZ_MU,
	  FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2, FZ_RU3}},

	/* JoDou-Rashii-Kan */
	{FZ_RASHI, 2, {0x2469, 0x2437}, 79,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KEIDOU, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3,
	  FZ_OKU, FZ_ORU, FZ_KA3, FZ_KARA2, FZ_GARI, FZ_GARU, FZ_KI, FZ_KI4,
	  FZ_KIRI, FZ_GI, FZ_KU3, FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2,
	  FZ_SA3, FZ_SHI4, FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZUTSU,
	  FZ_ZURU, FZ_TA, FZ_DA2, FZ_DAKE, FZ_CHI, FZ_TSU, FZ_NAZO, FZ_NADO,
	  FZ_NARU2, FZ_NI3, FZ_NU, FZ_NU2, FZ_NO2, FZ_NOMI, FZ_BAKARI, FZ_BI,
	  FZ_BU, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2, FZ_YARU,
	  FZ_YUE, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3}},

	/* RaHenShi */
	{FZ_I9, 1, {0x2424}, 1, {HN_RA_KUD}},

	/* JunJo-Sa */
	{FZ_SA3, 1, {0x2435}, 5,
	 {HN_KEIYOU, HN_KEIDOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},
	{FZ_MI4, 1, {0x245f}, 2, {HN_KEIYOU, FZ_TA4}},

	/* JunJo-Me */
	{FZ_ME2, 1, {0x2461}, 1, {HN_KEIYOU}},
	{FZ_GE2, 1, {0x2432}, 4, {HN_KEIYOU, HN_KE_SED, FZ_TA4, FZ_RASHI}},

	/* JunJo-No */
	{FZ_NO2, 1, {0x244e}, 35,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZU,
	  FZ_ZURU, FZ_TA, FZ_TARU, FZ_DA2, FZ_TSU, FZ_NA, FZ_NA2, FZ_NARU2,
	  FZ_NU, FZ_NU2, FZ_BU, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2, FZ_RU3,
	  FZ_N}},

	/* SetsuJo-Nagara */
	{FZ_NAGARA, 3, {0x244a, 0x242c, 0x2469}, 41,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, HN_KEIDOU, FZ_I, FZ_I4, FZ_I7, FZ_I8,
	  FZ_IKI, FZ_OKI, FZ_ORI, FZ_KANE, FZ_GARI, FZ_KI4, FZ_KI6, FZ_GI,
	  FZ_KURE3, FZ_GE2, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI7, FZ_SHIMAI,
	  FZ_SHIME, FZ_JI2, FZ_SE4, FZ_CHI, FZ_DEKI, FZ_NARI, FZ_NI3, FZ_NU,
	  FZ_BI, FZ_MI, FZ_MI3, FZ_ME2, FZ_YARI, FZ_YORI, FZ_RARE, FZ_RI,
	  FZ_RI2, FZ_RE5}},

	/* SetsuJo-Nari */
	{FZ_NARI2, 2, {0x244a, 0x246a}, 28,
	 {FZ_I, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU, FZ_GU,
	  FZ_SHIMAU, FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TA, FZ_DA2,
	  FZ_TSU, FZ_NARU2, FZ_NU, FZ_NU2, FZ_BU, FZ_MU, FZ_YARU, FZ_YORU,
	  FZ_RU, FZ_RU2, FZ_RU3}},

	/* HeiJo-Dari,Tari */
	{FZ_DARI, 2, {0x2440, 0x246a}, 2, {FZ_I2, FZ_N2}},
	{FZ_TARI2, 2, {0x243f, 0x246a}, 51,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I3, FZ_I8, FZ_IKE, FZ_IXTSU, FZ_E,
	  FZ_OI, FZ_OKE, FZ_OXTSU, FZ_ORE, FZ_KAXTSU, FZ_KAXTSU2, FZ_KANE,
	  FZ_GAXTSU, FZ_GARE, FZ_KI6, FZ_KURE3, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE,
	  FZ_SHI3, FZ_SHI7, FZ_SHIMAE, FZ_SHIMAXTSU, FZ_SHIME, FZ_JI2, FZ_SE2,
	  FZ_SE4, FZ_DAXTSU, FZ_XTSU, FZ_XTSU2, FZ_TE, FZ_TE2, FZ_DE4, FZ_DEKI,
	  FZ_DESHI, FZ_NAXTSU, FZ_NARE2, FZ_NE, FZ_BE, FZ_MI3, FZ_ME,
	  FZ_YAXTSU, FZ_YARE, FZ_YOXTSU, FZ_RARE, FZ_RE2, FZ_RE5}},

	/* SetsuJo-Tsutsu */
	{FZ_TSUTSU, 2, {0x2444, 0x2444}, 28,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I4, FZ_I8, FZ_KANE, FZ_GARI, FZ_KI4,
	  FZ_KI6, FZ_GI, FZ_KURE3, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI7,
	  FZ_SHIME, FZ_JI2, FZ_SE4, FZ_CHI, FZ_DEKI, FZ_NI3, FZ_BI, FZ_MI,
	  FZ_MI3, FZ_RARE, FZ_RI, FZ_RI2, FZ_RE5}},

	/* SetsuJo-Kara */
	{FZ_KARA2, 2, {0x242b, 0x2469}, 36,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U2, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3,
	  FZ_KURU, FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU,
	  FZ_ZURU, FZ_TA, FZ_DA, FZ_DA2, FZ_DA3, FZ_TSU, FZ_DESU, FZ_NARU2,
	  FZ_NU, FZ_NU2, FZ_BU, FZ_MAI, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2,
	  FZ_RU3, FZ_N}},

	/* SetsuJo-Ni */
	{FZ_NI5, 1, {0x244b}, 28,
	 {FZ_I, FZ_IKU, FZ_U2, FZ_U3, FZ_OKU, FZ_ORU, FZ_KARA2, FZ_GARU,
	  FZ_KU3, FZ_KURU, FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU,
	  FZ_ZURU, FZ_TSU, FZ_NARU2, FZ_NU, FZ_NU2, FZ_BU, FZ_MU, FZ_YARU,
	  FZ_YORU, FZ_RU, FZ_RU2, FZ_RU3}},

	/* SetsuJo-De,Te */
	{FZ_DE4, 1, {0x2447}, 6, {FZ_I, FZ_I2, FZ_TARI2, FZ_DARI, FZ_N, FZ_N2}},
	{FZ_TE2, 1, {0x2446}, 64,
	 {HN_1DOU, HN_KI_KI, HN_SI_SI, FZ_I3, FZ_I4, FZ_I8, FZ_IKE, FZ_IXTSU,
	  FZ_E, FZ_OI, FZ_OKE, FZ_OXTSU, FZ_ORE, FZ_KAXTSU, FZ_KAXTSU2,
	  FZ_KANE, FZ_GAXTSU, FZ_GARI, FZ_GARE, FZ_KI4, FZ_KI6, FZ_GI, FZ_KU4,
	  FZ_KU5, FZ_KURE3, FZ_KE, FZ_GE, FZ_SASE, FZ_SARE, FZ_SHI3, FZ_SHI4,
	  FZ_SHI7, FZ_SHIMAE, FZ_SHIMAXTSU, FZ_SHIME, FZ_JI2, FZ_SE2, FZ_SE4,
	  FZ_DAXTSU, FZ_CHI, FZ_XTSU, FZ_XTSU2, FZ_TE, FZ_DEKI, FZ_DESHI,
	  FZ_TO3, FZ_TO4, FZ_NAXTSU, FZ_NARE2, FZ_NI3, FZ_NI6, FZ_NE, FZ_BI,
	  FZ_BE, FZ_MI, FZ_MI3, FZ_ME, FZ_YAXTSU, FZ_YARE, FZ_YOXTSU, FZ_RARE,
	  FZ_RI2, FZ_RE2, FZ_RE5}},

	/* SetsuJo-Keredo,Kedo,Do */
	{FZ_KEREDO, 3, {0x2431, 0x246c, 0x2449}, 36,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZURU,
	  FZ_TA, FZ_DA, FZ_DA2, FZ_DA3, FZ_TSU, FZ_TE2, FZ_DE4, FZ_DESU,
	  FZ_NARU2, FZ_NU, FZ_NU2, FZ_BU, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU,
	  FZ_RU2, FZ_RU3, FZ_N}},
	{FZ_KEDO, 2, {0x2431, 0x2449}, 34,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZURU,
	  FZ_TA, FZ_DA, FZ_DA2, FZ_DA3, FZ_TSU, FZ_DESU, FZ_NARU2, FZ_NU,
	  FZ_NU2, FZ_BU, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2, FZ_RU3,
	  FZ_N}},
	{FZ_DO, 1, {0x2449}, 28,
	 {FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_GARE, FZ_KURE2, FZ_KE, FZ_KERE,
	  FZ_GE, FZ_ZARE, FZ_SHIMAE, FZ_JIRE, FZ_SURE, FZ_SURE2, FZ_ZURE,
	  FZ_SE2, FZ_TE, FZ_NARE2, FZ_NE, FZ_NE2, FZ_BE, FZ_ME, FZ_YARE,
	  FZ_YORE, FZ_RE, FZ_RE2, FZ_RE3, FZ_RE4}},

	/* SetsuJo-To */
	{FZ_TO3, 1, {0x2448}, 49,
	 {FZ_I, FZ_I7, FZ_I9, FZ_IKU, FZ_U, FZ_U2, FZ_U3, FZ_OKU, FZ_ORU,
	  FZ_KARA2, FZ_GARU, FZ_KU3, FZ_KU4, FZ_KU5, FZ_KURU, FZ_GU, FZ_SHI,
	  FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZU, FZ_ZURU,
	  FZ_TA, FZ_TARI2, FZ_DA, FZ_DA2, FZ_DA3, FZ_DARI, FZ_TSU, FZ_DESU,
	  FZ_NARA, FZ_NARU2, FZ_NI6, FZ_NU, FZ_NU2, FZ_BU, FZ_MAI, FZ_MU,
	  FZ_YARU, FZ_YOU, FZ_YORU, FZ_RA, FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},

	/* SetsuJo-Ga */
	{FZ_GA3, 1, {0x242c}, 39,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U, FZ_U2, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU,
	  FZ_KI, FZ_KU3, FZ_KURU, FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2,
	  FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TA, FZ_DA, FZ_DA2, FZ_DA3, FZ_TSU,
	  FZ_DESU, FZ_NARU2, FZ_NU, FZ_NU2, FZ_BU, FZ_MAI, FZ_MU, FZ_YARU,
	  FZ_YOU, FZ_YORU, FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},

	/* SetsuJo-Ba */
	{FZ_BA3, 1, {0x2450}, 34,
	 {FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_GARE, FZ_KURE2, FZ_KE, FZ_KERE,
	  FZ_GE, FZ_ZARE, FZ_SHIMAE, FZ_JIRE, FZ_SURE, FZ_SURE2, FZ_ZU,
	  FZ_ZURE, FZ_ZUN, FZ_SE2, FZ_TARA, FZ_TE, FZ_NARA, FZ_NARE2, FZ_NE,
	  FZ_NE2, FZ_BE, FZ_ME, FZ_YARE, FZ_YORE, FZ_RA, FZ_RA2, FZ_RE, FZ_RE2,
	  FZ_RE3, FZ_RE4}},

	/* SetsuJo-Shi,Kakujoshi-Tte */
	{FZ_SHI9, 1, {0x2437}, 36,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U2, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3,
	  FZ_KURU, FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU,
	  FZ_ZURU, FZ_TA, FZ_DA, FZ_DA2, FZ_DA3, FZ_TSU, FZ_DESU, FZ_NARU2,
	  FZ_NU, FZ_NU2, FZ_BU, FZ_MAI, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2,
	  FZ_RU3, FZ_N}},

	/* SetsuJo-Kuseni */
	{FZ_KUSENI, 3, {0x242f, 0x243b, 0x244b}, 30,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TA, FZ_TARU,
	  FZ_DA2, FZ_TSU, FZ_NA, FZ_NARU2, FZ_NU, FZ_NU2, FZ_BU, FZ_MU,
	  FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2, FZ_RU3}},

	/* KakariJo-Koso */
	{FZ_KOSO, 2, {0x2433, 0x243d}, 100,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_KI_KI, HN_SA_MEI, HN_SI_SI, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_I8,
	  FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_KANE, FZ_KARA2, FZ_GARI, FZ_GARE,
	  FZ_KI2, FZ_KI4, FZ_KI6, FZ_KIRI, FZ_GI, FZ_KU4, FZ_KU5, FZ_KURAI,
	  FZ_KURE3, FZ_GURAI, FZ_KE, FZ_GE, FZ_GE2, FZ_SA3, FZ_SASE, FZ_SARE,
	  FZ_SHI4, FZ_SHI7, FZ_SHIMAE, FZ_SHIME, FZ_JI2, FZ_ZUTSU, FZ_SE2,
	  FZ_SE4, FZ_TARI2, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_TE, FZ_TE2,
	  FZ_DE, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_TO, FZ_TO4, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARE2, FZ_NI, FZ_NI3, FZ_NI5, FZ_NI6, FZ_NE, FZ_NO2,
	  FZ_NO3, FZ_NOMI, FZ_BA3, FZ_BAKARI, FZ_BI, FZ_HE, FZ_BE, FZ_MADE,
	  FZ_MI, FZ_MI3, FZ_MI4, FZ_ME, FZ_ME2, FZ_YARE, FZ_XYUU, FZ_RARE,
	  FZ_RI2, FZ_RE2, FZ_RE5, FZ_WO}},

	/* KakariJo-Demo,Mo */
	{FZ_DEMO, 2, {0x2447, 0x2462}, 101,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_SA_MEI, HN_SI_SI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO,
	  HN_SETUJN, HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_I8, FZ_IKE,
	  FZ_E, FZ_OKE, FZ_ORE, FZ_KANE, FZ_KARA3, FZ_GARI, FZ_GARE, FZ_KI2,
	  FZ_KI4, FZ_KIRI, FZ_GI, FZ_KU4, FZ_KU5, FZ_KURAI, FZ_KURE3, FZ_GURAI,
	  FZ_KE, FZ_GE, FZ_GE2, FZ_SA3, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI7,
	  FZ_SHIMAE, FZ_SHIME, FZ_JI2, FZ_ZUTSU, FZ_SE2, FZ_SE4, FZ_TARI2,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_TSUTSU, FZ_TE, FZ_TE2, FZ_DE,
	  FZ_DE4, FZ_DE5, FZ_DEKI, FZ_TO, FZ_TO3, FZ_TO4, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARI3, FZ_NARE2, FZ_NI, FZ_NI3, FZ_NI6, FZ_NE, FZ_NO2,
	  FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_HE, FZ_BE, FZ_HODO, FZ_MADE,
	  FZ_MI, FZ_MI3, FZ_MI4, FZ_ME, FZ_ME2, FZ_YARA2, FZ_YARE, FZ_YORI2,
	  FZ_RARE, FZ_RI2, FZ_RE2, FZ_RE5, FZ_WO}},
	{FZ_MO2, 1, {0x2462}, 137,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_1YOU, HN_KI_KI, HN_SA_MEI, HN_SI_SI, HN_FUKUSI, HN_SETUBI,
	  HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_SA_M_S,
	  HN_SETUDO, FZ_I, FZ_I4, FZ_I5, FZ_I8, FZ_IKI, FZ_IKE, FZ_E, FZ_OKI,
	  FZ_OKE, FZ_ORI, FZ_ORE, FZ_KA3, FZ_KANE, FZ_KARA3, FZ_GARI, FZ_GARI2,
	  FZ_GARE, FZ_KI2, FZ_KI4, FZ_KI5, FZ_KI6, FZ_KIRI, FZ_GI, FZ_GI2,
	  FZ_KU, FZ_KU4, FZ_KU5, FZ_KURAI, FZ_KURE3, FZ_GURAI, FZ_KE, FZ_KEDO,
	  FZ_KEREDO, FZ_GE, FZ_GE2, FZ_SA3, FZ_SAE, FZ_SASE, FZ_SARE, FZ_SHI4,
	  FZ_SHI5, FZ_SHI6, FZ_SHI7, FZ_SHIMAI, FZ_SHIMAE, FZ_SHIME, FZ_JI,
	  FZ_JI2, FZ_SURA, FZ_ZU, FZ_ZUTSU, FZ_SE2, FZ_SE4, FZ_TARI2, FZ_DAKE,
	  FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSUTSU, FZ_TE, FZ_TE2, FZ_DE,
	  FZ_DE4, FZ_DE5, FZ_DEKI, FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NAGARA,
	  FZ_NAZO, FZ_NADO, FZ_NARI, FZ_NARI3, FZ_NARE2, FZ_NI, FZ_NI3, FZ_NI4,
	  FZ_NI5, FZ_NI6, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI,
	  FZ_BI2, FZ_HE, FZ_BE, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI3,
	  FZ_MI4, FZ_ME, FZ_ME2, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARE, FZ_XYUU,
	  FZ_YORI, FZ_YORI2, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RE2, FZ_RE5,
	  FZ_WO}},

	/* KakariJo-Ha */
	{FZ_HA, 1, {0x244f}, 133,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_1YOU, HN_KI_KI, HN_SA_MEI, HN_SI_SI, HN_FUKUSI, HN_SETUBI,
	  HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_SA_M_S,
	  HN_SETUDO, FZ_I, FZ_I4, FZ_I5, FZ_I8, FZ_IKI, FZ_IKE, FZ_E, FZ_OKI,
	  FZ_OKE, FZ_ORI, FZ_ORE, FZ_KA3, FZ_KANE, FZ_KARA2, FZ_KARA3, FZ_GARI,
	  FZ_GARI2, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI4, FZ_KI5, FZ_KI6, FZ_KIRI,
	  FZ_GI, FZ_GI2, FZ_KU, FZ_KU4, FZ_KU5, FZ_KURAI, FZ_KURE3, FZ_GURAI,
	  FZ_KE, FZ_GE, FZ_GE2, FZ_KOSO, FZ_SA3, FZ_SAE, FZ_SASE, FZ_SARE,
	  FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_SHI7, FZ_SHIMAI, FZ_SHIMAE, FZ_SHIME,
	  FZ_JI, FZ_JI2, FZ_SURA, FZ_ZUTSU, FZ_SE2, FZ_SE4, FZ_TARI2, FZ_DAKE,
	  FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TE, FZ_TE2, FZ_DE, FZ_DE4,
	  FZ_DE5, FZ_DEKI, FZ_TO, FZ_TO3, FZ_TO4, FZ_NAGARA, FZ_NAZO, FZ_NADO,
	  FZ_NARI, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_HE,
	  FZ_BE, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI3, FZ_MI4, FZ_ME,
	  FZ_ME2, FZ_YARA2, FZ_YARI, FZ_YARE, FZ_XYUU, FZ_YORI, FZ_YORI2,
	  FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RE2, FZ_RE5}},

	/* KakariJo-Ya */
	{FZ_YA, 1, {0x2464}, 25,
	 {FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU, FZ_GU,
	  FZ_SHIMAU, FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TSU,
	  FZ_NARU2, FZ_NU, FZ_NU2, FZ_BU, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU,
	  FZ_RU2, FZ_RU3}},

	/* FukuJo-Yue */
	{FZ_YUE, 2, {0x2466, 0x2428}, 85,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU,
	  FZ_KARA2, FZ_GA3, FZ_GARI, FZ_GARU, FZ_KI, FZ_KI4, FZ_KIRI, FZ_GI,
	  FZ_KU3, FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_ZARU,
	  FZ_SHI4, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU,
	  FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_TARU, FZ_DA2, FZ_DAKE, FZ_CHI, FZ_TSU,
	  FZ_TO4, FZ_NA, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARU, FZ_NARU2,
	  FZ_NI3, FZ_NU, FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BA3, FZ_BAKARI,
	  FZ_BI, FZ_BU, FZ_MAI, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2, FZ_YARU, FZ_YORU,
	  FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3}},

	/* FukuJo-Gurai,Kurai */
	{FZ_GURAI, 3, {0x2430, 0x2469, 0x2424}, 88,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_RENTAI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3,
	  FZ_OKU, FZ_ORU, FZ_KARA3, FZ_GARI, FZ_GARU, FZ_KI, FZ_KI4, FZ_KIRI,
	  FZ_GI, FZ_KU3, FZ_KURU, FZ_GU, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHIMAU,
	  FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_TARI2, FZ_DA2,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_TSU, FZ_DE5, FZ_TO3, FZ_TO4,
	  FZ_NA, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARU2, FZ_NI3, FZ_NI6, FZ_NU,
	  FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BU, FZ_HE,
	  FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2, FZ_YA2, FZ_YARA2,
	  FZ_YARU, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3}},
	{FZ_KURAI, 3, {0x242f, 0x2469, 0x2424}, 88,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_RENTAI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3,
	  FZ_OKU, FZ_ORU, FZ_KARA3, FZ_GARI, FZ_GARU, FZ_KI, FZ_KI4, FZ_KIRI,
	  FZ_GI, FZ_KU3, FZ_KURU, FZ_GU, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHIMAU,
	  FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_TARI2, FZ_DA2,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_TSU, FZ_DE5, FZ_TO3, FZ_TO4,
	  FZ_NA, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARU2, FZ_NI3, FZ_NI6, FZ_NU,
	  FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BU, FZ_HE,
	  FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2, FZ_YA2, FZ_YARA2,
	  FZ_YARU, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3}},

	/* FukuJo-Ka */
	{FZ_KA3, 1, {0x242b}, 127,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1YOU,
	  HN_SA_MEI, HN_KEIDOU, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO,
	  HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4,
	  FZ_I5, FZ_I7, FZ_IKU, FZ_U, FZ_U2, FZ_U3, FZ_OKU, FZ_ORU, FZ_KARA2,
	  FZ_KARA3, FZ_GA4, FZ_GARI, FZ_GARI2, FZ_GARU, FZ_KI, FZ_KI4, FZ_KI5,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5, FZ_KURAI,
	  FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2, FZ_KOSO, FZ_SA3, FZ_SAE, FZ_ZARU,
	  FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_SHIKA, FZ_SHIMAU, FZ_JI, FZ_JIRU,
	  FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_SOU2,
	  FZ_TA, FZ_TARU, FZ_DA, FZ_DA2, FZ_DAKE, FZ_DANO, FZ_CHI, FZ_CHI2,
	  FZ_TSU, FZ_TE2, FZ_DE4, FZ_DE5, FZ_DEMO, FZ_TO, FZ_TO3, FZ_TO4,
	  FZ_NAGARA, FZ_NARU, FZ_NARU2, FZ_NI, FZ_NI3, FZ_NI4, FZ_NI6, FZ_NU,
	  FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU,
	  FZ_HE, FZ_HODO, FZ_MAI, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI,
	  FZ_MU, FZ_ME2, FZ_MO2, FZ_YARU, FZ_YOU, FZ_YOU2, FZ_YORI2, FZ_YORU,
	  FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},

	/* FukuJo-Hodo */
	{FZ_HODO, 2, {0x245b, 0x2449}, 74,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_I9, FZ_IKU, FZ_U3,
	  FZ_OKU, FZ_ORU, FZ_GARI, FZ_GARU, FZ_KI, FZ_KI4, FZ_GI, FZ_KU2,
	  FZ_KU3, FZ_KURU, FZ_GU, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHIMAU, FZ_JIRU,
	  FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_TARU, FZ_DA2, FZ_CHI,
	  FZ_TSU, FZ_NA, FZ_NAZO, FZ_NADO, FZ_NARU2, FZ_NI3, FZ_NU, FZ_NU2,
	  FZ_NO2, FZ_NO3, FZ_BI, FZ_BU, FZ_MAI, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2,
	  FZ_YARU, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},

	/* FukuJo-Nado,Nazo */
	{FZ_NADO, 2, {0x244a, 0x2449}, 156,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_1YOU, HN_KI_KI, HN_SA_MEI, HN_SI_SI, HN_SETUBI, HN_JOSUU,
	  HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_SA_M_S, HN_SETUDO,
	  FZ_I, FZ_I4, FZ_I5, FZ_I7, FZ_I8, FZ_I9, FZ_IKU, FZ_IKE, FZ_U, FZ_U2,
	  FZ_U3, FZ_E, FZ_OKU, FZ_OKE, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA3, FZ_GA4, FZ_GARI, FZ_GARI2, FZ_GARU, FZ_GARE, FZ_KI4,
	  FZ_KI5, FZ_KI6, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KURAI, FZ_KURU, FZ_KURE3, FZ_GU, FZ_GURAI, FZ_KE, FZ_GE, FZ_GE2,
	  FZ_SA3, FZ_SASE, FZ_SARE, FZ_SHI, FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_SHI7,
	  FZ_SHIMAU, FZ_SHIMAE, FZ_SHIME, FZ_JI, FZ_JI2, FZ_JIRU, FZ_SU,
	  FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_SE2, FZ_SE4, FZ_TA,
	  FZ_TARI2, FZ_DA2, FZ_DA3, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2,
	  FZ_TSU, FZ_TSUTSU, FZ_TE, FZ_TE2, FZ_DE, FZ_DE4, FZ_DE5, FZ_DEKI,
	  FZ_DESU, FZ_TO, FZ_TO3, FZ_TO4, FZ_NA, FZ_NAGARA, FZ_NARU2, FZ_NARE2,
	  FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI6, FZ_NU, FZ_NU2, FZ_NE, FZ_NO2,
	  FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE,
	  FZ_HODO, FZ_MAI, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI3, FZ_MI4, FZ_MU,
	  FZ_ME, FZ_ME2, FZ_YA2, FZ_YARA2, FZ_YARU, FZ_YARE, FZ_XYUU, FZ_YOU,
	  FZ_YORI2, FZ_YORU, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2,
	  FZ_RU3, FZ_RE2, FZ_RE5}},
	{FZ_NAZO, 2, {0x244a, 0x243e}, 156,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_1YOU, HN_KI_KI, HN_SA_MEI, HN_SI_SI, HN_SETUBI, HN_JOSUU,
	  HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_SA_M_S, HN_SETUDO,
	  FZ_I, FZ_I4, FZ_I5, FZ_I7, FZ_I8, FZ_I9, FZ_IKU, FZ_IKE, FZ_U, FZ_U2,
	  FZ_U3, FZ_E, FZ_OKU, FZ_OKE, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA3, FZ_GA4, FZ_GARI, FZ_GARI2, FZ_GARU, FZ_GARE, FZ_KI4,
	  FZ_KI5, FZ_KI6, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KURAI, FZ_KURU, FZ_KURE3, FZ_GU, FZ_GURAI, FZ_KE, FZ_GE, FZ_GE2,
	  FZ_SA3, FZ_SASE, FZ_SARE, FZ_SHI, FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_SHI7,
	  FZ_SHIMAU, FZ_SHIMAE, FZ_SHIME, FZ_JI, FZ_JI2, FZ_JIRU, FZ_SU,
	  FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_SE2, FZ_SE4, FZ_TA,
	  FZ_TARI2, FZ_DA2, FZ_DA3, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2,
	  FZ_TSU, FZ_TSUTSU, FZ_TE, FZ_TE2, FZ_DE, FZ_DE4, FZ_DE5, FZ_DEKI,
	  FZ_DESU, FZ_TO, FZ_TO3, FZ_TO4, FZ_NA, FZ_NAGARA, FZ_NARU2, FZ_NARE2,
	  FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI6, FZ_NU, FZ_NU2, FZ_NE, FZ_NO2,
	  FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE,
	  FZ_HODO, FZ_MAI, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI3, FZ_MI4, FZ_MU,
	  FZ_ME, FZ_ME2, FZ_YA2, FZ_YARA2, FZ_YARU, FZ_YARE, FZ_XYUU, FZ_YOU,
	  FZ_YORI2, FZ_YORU, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2,
	  FZ_RU3, FZ_RE2, FZ_RE5}},

	/* FukuJo-Yara */
	{FZ_YARA2, 2, {0x2464, 0x2469}, 96,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KEIDOU, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_I9, FZ_IKU,
	  FZ_U, FZ_U3, FZ_OKU, FZ_ORU, FZ_KARA3, FZ_GARI, FZ_GARU, FZ_KI,
	  FZ_KI4, FZ_KIRI, FZ_GI, FZ_KU3, FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI,
	  FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3,
	  FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_TARI2, FZ_TARU, FZ_DA2,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_TSU, FZ_DE5, FZ_TO3, FZ_TO4,
	  FZ_NA, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI3, FZ_NARU2, FZ_NI3,
	  FZ_NI6, FZ_NU, FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI,
	  FZ_BU, FZ_HE, FZ_MAI, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2, FZ_YARU,
	  FZ_YOU, FZ_YORI2, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3}},

	/* FukuJo-Zutsu */
	{FZ_ZUTSU, 2, {0x243a, 0x2444}, 38,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_GI, FZ_KURAI,
	  FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_CHI, FZ_NI3, FZ_BAKARI, FZ_BI,
	  FZ_HODO, FZ_MI, FZ_MI4, FZ_ME2, FZ_RI2}},

	/* FukuJo-Giri,Kiri */
	{FZ_KIRI, 2, {0x242d, 0x246a}, 36,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_GI, FZ_GE2, FZ_SA3,
	  FZ_SHI4, FZ_TA, FZ_DA2, FZ_CHI, FZ_NI3, FZ_BI, FZ_MI, FZ_MI4, FZ_ME2,
	  FZ_RI2}},

	/* FukuJo-Bakashi,Bakari */
	{FZ_BAKARI, 3, {0x2450, 0x242b, 0x246a}, 95,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU,
	  FZ_ORU, FZ_KARA3, FZ_GARI, FZ_GARU, FZ_KI, FZ_KI4, FZ_KIRI, FZ_GI,
	  FZ_KU3, FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4,
	  FZ_SHIMAU, FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU,
	  FZ_TA, FZ_TARI2, FZ_TARU, FZ_DA2, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI,
	  FZ_TSU, FZ_TSUTSU, FZ_TE2, FZ_DE4, FZ_DE5, FZ_TO3, FZ_TO4, FZ_NA,
	  FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARU, FZ_NARU2, FZ_NI3, FZ_NI5,
	  FZ_NI6, FZ_NU, FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BI, FZ_BU, FZ_HE,
	  FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2, FZ_YARU, FZ_YORI2,
	  FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},

	/* FukuJo-Nomi,Dake */
	{FZ_NOMI, 2, {0x244e, 0x245f}, 90,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_I9, FZ_IKU, FZ_U3,
	  FZ_OKU, FZ_ORU, FZ_KARA3, FZ_GARI, FZ_GARU, FZ_KI, FZ_KI4, FZ_KIRI,
	  FZ_GI, FZ_KU3, FZ_KURU, FZ_GU, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHIMAU,
	  FZ_JIRU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_TARI2,
	  FZ_TARU, FZ_DA2, FZ_DARI, FZ_CHI, FZ_TSU, FZ_TSUTSU, FZ_TE2, FZ_DE4,
	  FZ_DE5, FZ_TO3, FZ_TO4, FZ_NA, FZ_NAGARA, FZ_NARI3, FZ_NARU,
	  FZ_NARU2, FZ_NI3, FZ_NI5, FZ_NI6, FZ_NU, FZ_NU2, FZ_NO2, FZ_NO3,
	  FZ_BAKARI, FZ_BI, FZ_BU, FZ_HE, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU,
	  FZ_ME2, FZ_YARU, FZ_YORI2, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3,
	  FZ_WO}},
	{FZ_DAKE, 2, {0x2440, 0x2431}, 95,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU,
	  FZ_ORU, FZ_KA3, FZ_KARA3, FZ_GA4, FZ_GARI, FZ_GARU, FZ_KI, FZ_KI4,
	  FZ_KIRI, FZ_GI, FZ_KU3, FZ_KU4, FZ_KU5, FZ_KURAI, FZ_KURU, FZ_GU,
	  FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHIMAU, FZ_JIRU, FZ_SU3,
	  FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_TARI2, FZ_TARU, FZ_DA2,
	  FZ_DANO, FZ_DARI, FZ_CHI, FZ_TSU, FZ_TSUTSU, FZ_TE2, FZ_DE4, FZ_DE5,
	  FZ_TO3, FZ_TO4, FZ_NA, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI3,
	  FZ_NARU2, FZ_NI3, FZ_NI5, FZ_NI6, FZ_NU, FZ_NU2, FZ_NO2, FZ_NO3,
	  FZ_BI, FZ_BU, FZ_HE, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2, FZ_YARA2,
	  FZ_YARU, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3, FZ_WO}},

	/* FukuJo-Sura,Sae */
	{FZ_SURA, 2, {0x2439, 0x2469}, 94,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_1YOU, HN_SA_MEI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO,
	  HN_SETUJN, HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_I5, FZ_I8,
	  FZ_KA3, FZ_KANE, FZ_KARA3, FZ_GARI, FZ_GARI2, FZ_KI4, FZ_KI5,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU4, FZ_KU5, FZ_KURAI, FZ_KURE3, FZ_GURAI,
	  FZ_GE2, FZ_SA3, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI5, FZ_SHI6,
	  FZ_SHIME, FZ_JI, FZ_ZUTSU, FZ_SE4, FZ_TARI2, FZ_DAKE, FZ_DANO,
	  FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSUTSU, FZ_TE2, FZ_DE, FZ_DE4, FZ_DE5,
	  FZ_DEKI, FZ_TO, FZ_TO3, FZ_TO4, FZ_NAGARA, FZ_NAZO, FZ_NADO,
	  FZ_NARI3, FZ_NI, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6, FZ_NO2, FZ_NO3,
	  FZ_NOMI, FZ_BI, FZ_BI2, FZ_HE, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI3,
	  FZ_MI4, FZ_ME2, FZ_YA2, FZ_YORI2, FZ_RARE, FZ_RI2, FZ_RI3, FZ_RE5,
	  FZ_WO}},
	{FZ_SAE, 2, {0x2435, 0x2428}, 126,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_1YOU, HN_KI_KI, HN_SA_MEI, HN_SI_SI, HN_SETUBI, HN_JOSUU,
	  HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_SA_M_S, HN_SETUDO,
	  FZ_I4, FZ_I5, FZ_I8, FZ_IKI, FZ_IKE, FZ_E, FZ_OKI, FZ_OKE, FZ_ORI,
	  FZ_ORE, FZ_KA3, FZ_KANE, FZ_KARA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARE, FZ_KI4, FZ_KI5, FZ_KI6, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU4,
	  FZ_KU5, FZ_KURAI, FZ_KURE3, FZ_GURAI, FZ_KE, FZ_GE, FZ_GE2, FZ_SA3,
	  FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_SHI7, FZ_SHIMAI,
	  FZ_SHIMAE, FZ_SHIME, FZ_JI, FZ_JI2, FZ_ZUTSU, FZ_SE2, FZ_SE4,
	  FZ_TARI2, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSUTSU,
	  FZ_TE, FZ_TE2, FZ_DE, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_TO, FZ_TO3, FZ_TO4,
	  FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI, FZ_NARI3, FZ_NARE2, FZ_NI,
	  FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_HE, FZ_BE, FZ_MADE, FZ_MI, FZ_MI2,
	  FZ_MI3, FZ_MI4, FZ_ME, FZ_ME2, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARE,
	  FZ_XYUU, FZ_YORI, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RE2, FZ_RE5,
	  FZ_WO}},

	/* FukuJo-Shika */
	{FZ_SHIKA, 2, {0x2437, 0x242b}, 106,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_I9, FZ_IKU, FZ_U3,
	  FZ_OKU, FZ_ORU, FZ_KA3, FZ_KARA3, FZ_GA4, FZ_GARI, FZ_GARU, FZ_KI,
	  FZ_KI2, FZ_KI4, FZ_KIRI, FZ_GI, FZ_KU3, FZ_KU4, FZ_KU5, FZ_KURAI,
	  FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHIMAU,
	  FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA,
	  FZ_TARI2, FZ_DA2, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_TSU,
	  FZ_TSUTSU, FZ_TE2, FZ_DE, FZ_DE4, FZ_DE5, FZ_TO, FZ_TO3, FZ_TO4,
	  FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI3, FZ_NARU2, FZ_NI, FZ_NI3,
	  FZ_NI5, FZ_NI6, FZ_NU, FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI,
	  FZ_BI, FZ_BU, FZ_HE, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2,
	  FZ_YARA2, FZ_YARU, FZ_XYUU, FZ_YORI2, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2,
	  FZ_RU3, FZ_WO}},

	/* HeiJo-Nari */
	{FZ_NARI3, 2, {0x244a, 0x246a}, 82,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU,
	  FZ_ORU, FZ_KARA3, FZ_GARI, FZ_GARU, FZ_KI2, FZ_KI4, FZ_KIRI, FZ_GI,
	  FZ_KU3, FZ_KU4, FZ_KU5, FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2,
	  FZ_SA3, FZ_SHI4, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU3, FZ_SURU,
	  FZ_ZUTSU, FZ_ZURU, FZ_TA, FZ_DA, FZ_DA2, FZ_DAKE, FZ_CHI, FZ_TSU,
	  FZ_DE5, FZ_NAGARA, FZ_NARU2, FZ_NI3, FZ_NI5, FZ_NI6, FZ_NU, FZ_NU2,
	  FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BI, FZ_BU, FZ_HE, FZ_MADE, FZ_MI, FZ_MI4,
	  FZ_MU, FZ_ME2, FZ_YARU, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3}},

	/* HeiJo-Dano */
	{FZ_DANO, 2, {0x2440, 0x244e}, 130,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KEIDOU, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I6, FZ_I7, FZ_IKU,
	  FZ_IKE, FZ_U, FZ_U3, FZ_E, FZ_OKU, FZ_OKE, FZ_ORU, FZ_ORE, FZ_KA3,
	  FZ_KARA3, FZ_GARI, FZ_GARU, FZ_GARE, FZ_KI, FZ_KI4, FZ_KIRI, FZ_GI,
	  FZ_KU3, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_GU, FZ_GURAI, FZ_KE, FZ_GE,
	  FZ_GE2, FZ_KOI, FZ_SA3, FZ_SHI4, FZ_SHIMAU, FZ_SHIMAE, FZ_SHIRO,
	  FZ_JIYO, FZ_JIRU, FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SEYO, FZ_ZEYO, FZ_TA, FZ_TARI2, FZ_TARU,
	  FZ_TARE, FZ_DA2, FZ_DAKE, FZ_DARI, FZ_CHI, FZ_TSU, FZ_TE, FZ_DE5,
	  FZ_TO4, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI3,
	  FZ_NARU2, FZ_NARE, FZ_NARE2, FZ_NI3, FZ_NI6, FZ_NU, FZ_NU2, FZ_NE,
	  FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BU, FZ_HE, FZ_BE,
	  FZ_MAI, FZ_MADE, FZ_MI, FZ_MI4, FZ_MU, FZ_ME, FZ_ME2, FZ_YA2,
	  FZ_YARA2, FZ_YARU, FZ_YARE, FZ_YO, FZ_YOU, FZ_YORI2, FZ_YORU,
	  FZ_YORE, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE, FZ_RE2, FZ_RO4, FZ_WO,
	  FZ_N}},

	/* HeiJo-Ya */
	{FZ_YA2, 1, {0x2464}, 57,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_KARA3, FZ_GARI, FZ_KI4, FZ_KIRI,
	  FZ_GI, FZ_KURAI, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_ZUTSU,
	  FZ_TARI2, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_DE5, FZ_TO4,
	  FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI3, FZ_NI3, FZ_NI6, FZ_NO2,
	  FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_HE, FZ_MADE, FZ_MI, FZ_MI4,
	  FZ_ME2, FZ_YORI2, FZ_RI2}},

	/* FukuJo-Made */
	{FZ_MADE, 2, {0x245e, 0x2447}, 93,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU,
	  FZ_ORU, FZ_GARI, FZ_GARU, FZ_KI4, FZ_KIRI, FZ_GI, FZ_KU3, FZ_KURAI,
	  FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_ZARU, FZ_SHI4,
	  FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_TA,
	  FZ_TARI2, FZ_TARU, FZ_DA2, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_TSU,
	  FZ_TSUTSU, FZ_TE2, FZ_DE4, FZ_DE5, FZ_TO3, FZ_TO4, FZ_NA, FZ_NAGARA,
	  FZ_NAZO, FZ_NADO, FZ_NARI3, FZ_NARU2, FZ_NI3, FZ_NI5, FZ_NI6, FZ_NU,
	  FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BU, FZ_HE,
	  FZ_HODO, FZ_MI, FZ_MI4, FZ_MU, FZ_ME2, FZ_YA2, FZ_YARA2, FZ_YARU,
	  FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2, FZ_RU3}},

	/* KakuJo-To */
	{FZ_TO4, 1, {0x2448}, 95,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO,
	  HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_I6,
	  FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_KA3, FZ_KARA3, FZ_GARI, FZ_GARE,
	  FZ_KI4, FZ_KIRI, FZ_GI, FZ_KURAI, FZ_KURE, FZ_GURAI, FZ_KE, FZ_GE,
	  FZ_GE2, FZ_KOI, FZ_SA3, FZ_SAE, FZ_SHI4, FZ_SHIKA, FZ_SHIMAE,
	  FZ_SHIRO, FZ_JIYO, FZ_JIRO, FZ_SURA, FZ_ZUTSU, FZ_SE, FZ_SE2,
	  FZ_SEYO, FZ_ZEYO, FZ_TARE, FZ_DAKE, FZ_DANO, FZ_CHI, FZ_TE, FZ_NA6,
	  FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI2, FZ_NARI3, FZ_NARE, FZ_NARE2,
	  FZ_NI3, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_HE,
	  FZ_BE, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_ME, FZ_ME2, FZ_YA2,
	  FZ_YARA2, FZ_YARE, FZ_YO, FZ_YORE, FZ_RI2, FZ_RE, FZ_RE2, FZ_RO4,
	  FZ_WA3}},

	/* KakuJo-Yori */
	{FZ_YORI2, 2, {0x2468, 0x246a}, 82,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU,
	  FZ_ORU, FZ_KA3, FZ_GARI, FZ_GARU, FZ_KI4, FZ_KIRI, FZ_GI, FZ_KU3,
	  FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_ZARU, FZ_SHI4,
	  FZ_SHIKA, FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU,
	  FZ_TA, FZ_TARU, FZ_DA2, FZ_DAKE, FZ_DANO, FZ_CHI, FZ_TSU, FZ_TO4,
	  FZ_NA, FZ_NAZO, FZ_NADO, FZ_NARU2, FZ_NI3, FZ_NU, FZ_NU2, FZ_NO2,
	  FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BU, FZ_MADE, FZ_MI, FZ_MI4,
	  FZ_MU, FZ_ME2, FZ_YA2, FZ_YARU, FZ_YORU, FZ_RI2, FZ_RU, FZ_RU2,
	  FZ_RU3}},

	/* KakuJo-Kara */
	{FZ_KARA3, 2, {0x242b, 0x2469}, 52,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_KIRI, FZ_GI,
	  FZ_KURAI, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_ZUTSU, FZ_DAKE,
	  FZ_DANO, FZ_CHI, FZ_TE2, FZ_DE4, FZ_TO4, FZ_NAZO, FZ_NADO, FZ_NARI3,
	  FZ_NI3, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_HODO, FZ_MI,
	  FZ_MI4, FZ_ME2, FZ_YA2, FZ_RI2}},

	/* KakuJo-De */
	{FZ_DE5, 1, {0x2447}, 83,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I, FZ_I4, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU,
	  FZ_ORU, FZ_KA3, FZ_KARA3, FZ_GARI, FZ_GARU, FZ_KI4, FZ_KIRI, FZ_GI,
	  FZ_KU3, FZ_KURAI, FZ_KURU, FZ_GU, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4,
	  FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZUTSU, FZ_ZURU, FZ_DAKE,
	  FZ_DANO, FZ_CHI, FZ_TSU, FZ_TO4, FZ_NAGARA, FZ_NAZO, FZ_NADO,
	  FZ_NARI2, FZ_NARI3, FZ_NARU2, FZ_NI3, FZ_NU, FZ_NU2, FZ_NO2, FZ_NO3,
	  FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BU, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4,
	  FZ_MU, FZ_ME2, FZ_YA2, FZ_YARA2, FZ_YARU, FZ_YORU, FZ_RI2, FZ_RU,
	  FZ_RU2, FZ_RU3, FZ_WA3}},

	/* KakuJo-Wo */
	{FZ_WO, 1, {0x2472}, 59,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_KA3, FZ_KARA3, FZ_GARI, FZ_KI4,
	  FZ_KIRI, FZ_GI, FZ_KURAI, FZ_GURAI, FZ_GE2, FZ_KOSO, FZ_SA3, FZ_SAE,
	  FZ_ZARU, FZ_SHI4, FZ_SURA, FZ_ZUTSU, FZ_DAKE, FZ_DANO, FZ_CHI,
	  FZ_TO4, FZ_NAZO, FZ_NADO, FZ_NARI2, FZ_NARI3, FZ_NI3, FZ_NO2, FZ_NO3,
	  FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI4, FZ_ME2,
	  FZ_YA2, FZ_YARA2, FZ_RI2}},

	/* KakuJo-He */
	{FZ_HE, 1, {0x2458}, 51,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_SA_MEI,
	  HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH,
	  HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_GARI, FZ_KI4, FZ_KIRI, FZ_GI,
	  FZ_KURAI, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_ZUTSU, FZ_DAKE,
	  FZ_DANO, FZ_CHI, FZ_TO4, FZ_NAZO, FZ_NADO, FZ_NARI3, FZ_NI3, FZ_NO2,
	  FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_HODO, FZ_MI, FZ_MI4, FZ_ME2,
	  FZ_YA2, FZ_YARA2, FZ_RI2}},

	/* KakuJo-Ni */
	{FZ_NI6, 1, {0x244b}, 105,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1DOU,
	  HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_FUKUSI, HN_SETUBI, HN_JOSUU,
	  HN_SETOJO, HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_SA_M_S, HN_SETUDO,
	  FZ_I4, FZ_I5, FZ_I8, FZ_IKE, FZ_E, FZ_OKE, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA3, FZ_GARI, FZ_GARI2, FZ_GARE, FZ_KI2, FZ_KI4, FZ_KI5,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KURAI, FZ_KURE3, FZ_GURAI, FZ_KE, FZ_GE,
	  FZ_GE2, FZ_SA3, FZ_SAE, FZ_SASE, FZ_SARE, FZ_SHI4, FZ_SHI5, FZ_SHI6,
	  FZ_SHI7, FZ_SHIMAE, FZ_SHIME, FZ_JI, FZ_JI2, FZ_SURA, FZ_ZU,
	  FZ_ZUTSU, FZ_SE2, FZ_SE4, FZ_DAKE, FZ_DANO, FZ_CHI, FZ_CHI2, FZ_TE,
	  FZ_DEKI, FZ_TO4, FZ_NAZO, FZ_NADO, FZ_NARI2, FZ_NARI3, FZ_NARE2,
	  FZ_NI3, FZ_NI4, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI,
	  FZ_BI2, FZ_BE, FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI3, FZ_MI4,
	  FZ_ME, FZ_ME2, FZ_YA2, FZ_YARA2, FZ_YARE, FZ_YUE, FZ_RARE, FZ_RI2,
	  FZ_RI3, FZ_RE2, FZ_RE5}},

	/* KakuJo-No */
	{FZ_NO3, 1, {0x244e}, 74,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1YOU,
	  HN_SA_MEI, HN_KEIDOU, HN_FUKUSI, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_I5,
	  FZ_KA3, FZ_KARA3, FZ_GARI, FZ_GARI2, FZ_KI4, FZ_KI5, FZ_KIRI, FZ_GI,
	  FZ_GI2, FZ_KURAI, FZ_GURAI, FZ_GE2, FZ_SA3, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_JI, FZ_ZUTSU, FZ_DAKE, FZ_CHI, FZ_CHI2, FZ_TE2, FZ_DE4,
	  FZ_DE5, FZ_TO3, FZ_TO4, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI2,
	  FZ_NARI3, FZ_NI3, FZ_NI4, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_HE,
	  FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_ME2, FZ_YA2, FZ_YARA2,
	  FZ_YORI2, FZ_RI2, FZ_RI3}},

	/* KakuJo-Ga */
	{FZ_GA4, 1, {0x242c}, 74,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_GIJI,
	  HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI, HN_1YOU,
	  HN_SA_MEI, HN_SETUBI, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETUCH, HN_SA_M_S, HN_SETUDO, FZ_I4, FZ_I5, FZ_KA3, FZ_KARA3,
	  FZ_GARI, FZ_GARI2, FZ_KI4, FZ_KI5, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KURAI,
	  FZ_GURAI, FZ_GE2, FZ_KOSO, FZ_SA3, FZ_SAE, FZ_SHI4, FZ_SHI5, FZ_SHI6,
	  FZ_SHIKA, FZ_JI, FZ_SURA, FZ_ZUTSU, FZ_DAKE, FZ_DANO, FZ_CHI,
	  FZ_CHI2, FZ_DEMO, FZ_TO4, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARI3,
	  FZ_NI3, FZ_NI4, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_BAKARI, FZ_BI, FZ_BI2,
	  FZ_HODO, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_ME2, FZ_MO2, FZ_YA2,
	  FZ_YARA2, FZ_RI2, FZ_RI3}},

	/* ShuuJo-Na */
	{FZ_NA6, 1, {0x244a}, 23,
	 {FZ_I9, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU3, FZ_SURU, FZ_ZURU, FZ_TSU,
	  FZ_NARU2, FZ_NU2, FZ_BU, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU2, FZ_RU3}},

	/* ShuuJo-Wa */
	{FZ_WA3, 1, {0x246f}, 33,
	 {FZ_I, FZ_I7, FZ_IKU, FZ_U3, FZ_OKU, FZ_ORU, FZ_GARU, FZ_KU3, FZ_KURU,
	  FZ_GU, FZ_SHIMAU, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURU, FZ_ZURU,
	  FZ_TA, FZ_DA, FZ_DA2, FZ_DA3, FZ_TSU, FZ_DESU, FZ_NARU2, FZ_NU2,
	  FZ_BU, FZ_MU, FZ_YARU, FZ_YORU, FZ_RU, FZ_RU2, FZ_RU3, FZ_N}},

	/* KuTouten */
	{FZ_KUTEN, 1, {0x2123}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
	{FZ_ZENPERIOD, 1, {0x2125}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
	{FZ_PERIOD, 1, {'.'}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
	{FZ_QUEST, 1, {'?'}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
	{FZ_ZENQUEST, 1, {0x2129}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
	{FZ_BAN, 1, {'!'}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
	{FZ_ZENBAN, 1, {0x212a}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
	{FZ_TOUTEN, 1, {0x2122}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
	{FZ_COMMA, 1, {','}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},
};
#define	CONLISTSIZ		arraysize(conlist)
static CONST contable shuutanlist[] = {
	/* svkanren */
	{SH_svkanren, 0, {0}, 238,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I6,
	  FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U, FZ_U2, FZ_U3, FZ_E,
	  FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE, FZ_KA3, FZ_KANE,
	  FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4, FZ_GARI, FZ_GARI2,
	  FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5, FZ_KI7,
	  FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4, FZ_KU5,
	  FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI, FZ_KOSO, FZ_SA3,
	  FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5,
	  FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU,
	  FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2, FZ_JIYO, FZ_JIRU,
	  FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU, FZ_ZU, FZ_ZUTSU,
	  FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO, FZ_SOU, FZ_SOU2,
	  FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA, FZ_DA2, FZ_DA3,
	  FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE,
	  FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO,
	  FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6, FZ_NAGARA, FZ_NAZO,
	  FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3, FZ_NARU, FZ_NARU2,
	  FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5, FZ_NI6,
	  FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO, FZ_MAI,
	  FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME, FZ_ME2,
	  FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE, FZ_XYUU,
	  FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU, FZ_YORE,
	  FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2, FZ_RU3, FZ_RE,
	  FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},

	/* svkantan */
	{SH_svkantan, 0, {0}, 264,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_KA5DOU, HN_GA5DOU, HN_SA5DOU, HN_TA5DOU, HN_NA5DOU, HN_BA5DOU,
	  HN_MA5DOU, HN_RA5DOU, HN_WA5DOU, HN_1DOU, HN_SAHDOU, HN_ZAHDOU,
	  HN_1YOU, HN_KA_IKU, HN_KO_KO, HN_KI_KI, HN_KU_KU, HN_SA_MEI,
	  HN_SI_SI, HN_SU_SU, HN_SE_SE, HN_RA_KUD, HN_KEIYOU, HN_KEIDOU,
	  HN_KD_TAR, HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_TANKAN, HN_SETTOU,
	  HN_SETUBI, HN_SETOSU, HN_JOSUU, HN_SETOJO, HN_SETUJO, HN_SETUJN,
	  HN_SETOCH, HN_SETUCH, HN_SETO_O, HN_SETO_K, HN_KD_STB, HN_SA_M_S,
	  HN_SETUDO, HN_KE_SED, FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST,
	  FZ_TOUTEN, FZ_KUTEN, FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I,
	  FZ_I4, FZ_I5, FZ_I6, FZ_I7, FZ_I9, FZ_IKI, FZ_IKU, FZ_IKE, FZ_U,
	  FZ_U2, FZ_U3, FZ_E, FZ_OKI, FZ_OKU, FZ_OKE, FZ_ORI, FZ_ORU, FZ_ORE,
	  FZ_KA3, FZ_KANE, FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4,
	  FZ_GARI, FZ_GARI2, FZ_GARU, FZ_GARE, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4,
	  FZ_KI5, FZ_KI7, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3,
	  FZ_KU4, FZ_KU5, FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE, FZ_KURE3,
	  FZ_GU, FZ_GURAI, FZ_KE, FZ_KEDO, FZ_KEREDO, FZ_GE, FZ_GE2, FZ_KOI,
	  FZ_KOSO, FZ_SA3, FZ_SAE, FZ_SASE, FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2,
	  FZ_SHI4, FZ_SHI5, FZ_SHI6, FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA,
	  FZ_SHIMAI, FZ_SHIMAU, FZ_SHIMAE, FZ_SHIME, FZ_SHIRO, FZ_JI, FZ_JI2,
	  FZ_JIYO, FZ_JIRU, FZ_JIRO, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU,
	  FZ_ZU, FZ_ZUTSU, FZ_ZURU, FZ_SE, FZ_SE2, FZ_SE4, FZ_SEYO, FZ_ZEYO,
	  FZ_SOU, FZ_SOU2, FZ_TA, FZ_TARI, FZ_TARI2, FZ_TARU, FZ_TARE, FZ_DA,
	  FZ_DA2, FZ_DA3, FZ_DAKE, FZ_DANO, FZ_DARI, FZ_CHI, FZ_CHI2, FZ_TSU,
	  FZ_TSUTSU, FZ_TE, FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4, FZ_DE5, FZ_DEKI,
	  FZ_DESU, FZ_DEMO, FZ_TO, FZ_TO3, FZ_TO4, FZ_DO, FZ_NA, FZ_NA6,
	  FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2, FZ_NARI3,
	  FZ_NARU, FZ_NARU2, FZ_NARE, FZ_NARE2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4,
	  FZ_NI5, FZ_NI6, FZ_NU, FZ_NU2, FZ_NE, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA,
	  FZ_BA3, FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_BE, FZ_HODO,
	  FZ_MAI, FZ_MADE, FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME,
	  FZ_ME2, FZ_MO2, FZ_YA, FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_YARE,
	  FZ_XYUU, FZ_YUE, FZ_YO, FZ_YOU, FZ_YOU2, FZ_YORI, FZ_YORI2, FZ_YORU,
	  FZ_YORE, FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3, FZ_RU, FZ_RU2,
	  FZ_RU3, FZ_RE, FZ_RE2, FZ_RE5, FZ_RO4, FZ_WA3, FZ_WO, FZ_N}},

	/* svbunsetsu */
	{SH_svbunsetsu, 0, {0}, 206,
	 {HN_SUUJI, HN_KANA, HN_EISUU, HN_KIGOU, HN_HEIKAKKO, HN_KAIKAKKO,
	  HN_GIJI, HN_MEISHI, HN_JINMEI, HN_CHIMEI, HN_KOYUU, HN_SUUSHI,
	  HN_1DOU, HN_1YOU, HN_SA_MEI, HN_SI_SI, HN_KEIDOU, HN_KD_TAR,
	  HN_FUKUSI, HN_RENTAI, HN_SETKAN, HN_SETUBI, HN_JOSUU, HN_SETOJO,
	  HN_SETUJO, HN_SETUJN, HN_SETUCH, HN_KD_STB, HN_SA_M_S, HN_SETUDO,
	  FZ_BAN, FZ_COMMA, FZ_PERIOD, FZ_QUEST, FZ_TOUTEN, FZ_KUTEN,
	  FZ_ZENPERIOD, FZ_ZENQUEST, FZ_ZENBAN, FZ_I, FZ_I4, FZ_I5, FZ_I7,
	  FZ_I9, FZ_IKI, FZ_IKU, FZ_U, FZ_U2, FZ_U3, FZ_OKI, FZ_OKU, FZ_ORI,
	  FZ_ORU, FZ_KA3, FZ_KANE, FZ_KARA2, FZ_KARA3, FZ_KARE, FZ_GA3, FZ_GA4,
	  FZ_GARI, FZ_GARI2, FZ_GARU, FZ_KI, FZ_KI2, FZ_KI3, FZ_KI4, FZ_KI5,
	  FZ_KI7, FZ_KIRI, FZ_GI, FZ_GI2, FZ_KU, FZ_KU2, FZ_KU3, FZ_KU4,
	  FZ_KU5, FZ_KUSENI, FZ_KURAI, FZ_KURU, FZ_KURE3, FZ_GU, FZ_GURAI,
	  FZ_KEDO, FZ_KEREDO, FZ_GE2, FZ_KOSO, FZ_SA3, FZ_SAE, FZ_SASE,
	  FZ_SARE, FZ_ZARU, FZ_SHI, FZ_SHI2, FZ_SHI4, FZ_SHI5, FZ_SHI6,
	  FZ_SHI7, FZ_SHI8, FZ_SHI9, FZ_SHIKA, FZ_SHIMAI, FZ_SHIMAU, FZ_SHIME,
	  FZ_JI, FZ_JI2, FZ_JIRU, FZ_SU, FZ_SU2, FZ_SU3, FZ_SURA, FZ_SURU,
	  FZ_ZU, FZ_ZUTSU, FZ_ZURU, FZ_SE4, FZ_SOU, FZ_SOU2, FZ_TA, FZ_TARI,
	  FZ_TARI2, FZ_TARU, FZ_DA, FZ_DA2, FZ_DA3, FZ_DAKE, FZ_DANO, FZ_DARI,
	  FZ_CHI, FZ_CHI2, FZ_TSU, FZ_TSUTSU, FZ_TE2, FZ_DE, FZ_DE2, FZ_DE4,
	  FZ_DE5, FZ_DEKI, FZ_DESU, FZ_DEMO, FZ_TO, FZ_TO3, FZ_TO4, FZ_DO,
	  FZ_NA, FZ_NAGARA, FZ_NAZO, FZ_NADO, FZ_NARA, FZ_NARI, FZ_NARI2,
	  FZ_NARI3, FZ_NARU, FZ_NARU2, FZ_NI, FZ_NI2, FZ_NI3, FZ_NI4, FZ_NI5,
	  FZ_NI6, FZ_NU, FZ_NU2, FZ_NO2, FZ_NO3, FZ_NOMI, FZ_HA, FZ_BA3,
	  FZ_BAKARI, FZ_BI, FZ_BI2, FZ_BU, FZ_HE, FZ_HODO, FZ_MAI, FZ_MADE,
	  FZ_MI, FZ_MI2, FZ_MI4, FZ_MITAI, FZ_MU, FZ_ME2, FZ_MO2, FZ_YA,
	  FZ_YA2, FZ_YARA2, FZ_YARI, FZ_YARU, FZ_XYUU, FZ_YUE, FZ_YOU, FZ_YOU2,
	  FZ_YORI, FZ_YORI2, FZ_YORU, FZ_RA, FZ_RARE, FZ_RI, FZ_RI2, FZ_RI3,
	  FZ_RU, FZ_RU2, FZ_RU3, FZ_RE5, FZ_WA3, FZ_WO, FZ_N}},
};
#define	SHUUTANLISTSIZ		arraysize(shuutanlist)


static u_char *NEAR realloc2(buf, size)
VOID_P buf;
ALLOC_T size;
{
	return((buf) ? (u_char *)realloc(buf, size) : (u_char *)malloc(size));
}

static int NEAR addstrbuf(len)
ALLOC_T len;
{
	u_char *cp;
	ALLOC_T size;

	len += maxstr;
	size = (strbufsize) ? strbufsize : DICTBUFUNIT;
	while (size < len) size *= 2;
	if (size <= strbufsize) return(0);
	if (!(cp = realloc2(strbuf, size))) return(-1);
	strbuf = cp;
	strbufsize = size;

	return(0);
}

static int NEAR adddictlist(ptr, klen, size)
ALLOC_T ptr, klen, size;
{
	dicttable *new;

	if (maxdict >= dictlistsize) {
		new = (dicttable *)realloc2(dictlist,
			(dictlistsize + DICTBUFUNIT) * sizeof(dicttable));
		if (!new) return(-1);
		dictlist = new;
		dictlistsize += DICTBUFUNIT;
	}

	dictlist[maxdict].ptr = ptr;
	dictlist[maxdict].klen = klen;
	dictlist[maxdict].size = size;
	dictlist[maxdict].max = 0;
	maxdict++;

	return(0);
}

static int NEAR setkbuf(klen, kbuf)
int klen;
CONST u_short *kbuf;
{
	int i;

	if (addstrbuf(1 + klen * 2) < 0) return(-1);
	strbuf[maxstr++] = klen;
	for (i = 0; i < klen; i++) {
		strbuf[maxstr++] = (kbuf[i] & 0xff);
		strbuf[maxstr++] = ((kbuf[i] >> 8) & 0xff);
	}

	return(0);
}

static int NEAR setjisbuf(s, roman, bias)
CONST char *s;
int roman, bias;
{
	u_short kbuf[MAXUTYPE(u_char) - 1];
	int i, n;

	n = str2jis(kbuf, arraysize(kbuf), s);
	if (roman) {
		for (i = 0; i < n - 1; i++) {
			if (kbuf[i] != J_U || kbuf[i + 1] != J_DAKUTEN)
				continue;
			kbuf[i] = J_VU;
			memmove((char *)&(kbuf[i + 1]), (char *)&(kbuf[i + 2]),
				(--n - i - 1) * sizeof(u_short));
		}
	}
	if (bias < BIASLISTSIZ) {
		for (i = 0; i < biaslist[bias].klen; i++)
			if (kbuf[n - 1] == biaslist[bias].kbuf[i]) break;
		if (i < biaslist[bias].klen) n--;
	}

	return(setkbuf(n, kbuf));
}

static int NEAR setword(w)
u_int w;
{
	if (addstrbuf(2) < 0) return(-1);
	strbuf[maxstr++] = (w & 0xff);
	strbuf[maxstr++] = ((w >> 8) & 0xff);

	return(0);
}

static int NEAR sethinsi(len, idlist)
int len;
CONST u_short *idlist;
{
	int i;

	if (addstrbuf(2 + len * 2) < 0) return(-1);
	strbuf[maxstr++] = (len & 0xff);
	strbuf[maxstr++] = ((len >> 8) & 0xff);
	for (i = 0; i < len; i++) {
		strbuf[maxstr++] = (idlist[i] & 0xff);
		strbuf[maxstr++] = ((idlist[i] >> 8) & 0xff);
	}

	return(0);
}

static int cmphinsi(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	hinsitable *hp1, *hp2;
	u_short *buf1, *buf2;
	int i, klen1, klen2;

	hp1 = (hinsitable *)vp1;
	hp2 = (hinsitable *)vp2;
	buf1 = hp1 -> kbuf;
	buf2 = hp2 -> kbuf;
	klen1 = hp1 -> klen;
	klen2 = hp2 -> klen;

	for (i = 0; i < klen1; i++) {
		if (i >= klen2) return(1);
		if (buf1[i] > buf2[i]) return(1);
		else if (buf1[i] < buf2[i]) return(-1);
	}

	return(klen1 - klen2);
}

static int NEAR addhinsi(hmax, idlist, id, hstr)
int hmax;
u_short *idlist;
int id;
CONST char *hstr;
{
	int n;

	for (n = 0; n < hmax; n++) if (idlist[n] == id) return(hmax);
	if (hmax >= MAXHINSI) {
		if (verbose) fprintf(stderr,
			"%s: Too many Hinsi fields.\n", hstr);
		return(hmax);
	}
	idlist[hmax++] = id;

	return(hmax);
}

static int NEAR _gethinsi(hmax, idlist, hstr, dictp)
int hmax;
u_short *idlist;
CONST char *hstr;
int *dictp;
{
	hinsitable tmp;
	int i, n, min, max;

	if (!hstr || !*hstr) return(hmax);
	tmp.klen = str2jis(tmp.kbuf, MAXHINSILEN, hstr);
	min = -1;
	max = HINSILISTSIZ;
	for (;;) {
		i = (min + max) / 2;
		if (i <= min || i >= max) {
			if (verbose) fprintf(stderr,
				"%s: Unknown Hinsi srting (ignored).\n", hstr);
			return(hmax);
		}
		n = cmphinsi(&(hinsilist[i]), &tmp);
		if (n < 0) min = i;
		else if (n > 0) max = i;
		else break;
	}

	*dictp = hinsilist[i].dict;
	hmax = addhinsi(hmax, idlist, hinsilist[i].id, hstr);
	for (n = 0; n < HINSISETLISTSIZ; n++)
		if (hinsisetlist[n].id == hinsilist[i].id) break;
	if (n < HINSISETLISTSIZ) for (i = 0; i < hinsisetlist[n].klen; i++)
		hmax = addhinsi(hmax, idlist, hinsisetlist[n].set[i], hstr);

	return(hmax);
}

static int NEAR gethinsi(hmax, idlist, hstr, dictp)
int hmax;
u_short *idlist;
char *hstr;
int *dictp;
{
	CONST char *cp;
	char *next;
	int dict, mask;

	next = hstr;
	while ((cp = next)) {
		mask = ~DIC_SJ3;
		if ((next = strchr(cp, ':'))) {
			mask = !mask;
			*(next++) = '\0';
		}
		hmax = _gethinsi(hmax, idlist, cp, &dict);
		*dictp |= (dict & mask);
	}

	return(hmax);
}

static int cmpidlist(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	u_short *sp1, *sp2;

	sp1 = (u_short *)vp1;
	sp2 = (u_short *)vp2;

	return(*sp1 - *sp2);
}

static int NEAR adddict(str, kstr, hstr, freq)
CONST char *str, *kstr;
char *hstr;
u_int freq;
{
	ALLOC_T ptr, klen;
	u_short idlist[MAXHINSI];
	char *cp, *next;
	int i, n, dict, hmax;

	dict = hmax = 0;
	next = hstr;
	while ((cp = next)) {
		if ((next = strchr(cp, '/'))) *(next++) = '\0';
		hmax = gethinsi(hmax, idlist, cp, &dict);
	}
	qsort(idlist, hmax, sizeof(u_short), cmpidlist);
	n = BIASLISTSIZ;
	if (!(dict & DIC_WNN)) for (i = 0; i < hmax; i++) {
		for (n = 0; n < BIASLISTSIZ; n++)
			if (idlist[i] == biaslist[n].id) break;
		if (n < BIASLISTSIZ) break;
	}

	ptr = maxstr;
	if (setjisbuf(str, 1, n) < 0) return(-1);
	klen = maxstr - ptr;
	if (setjisbuf(kstr, 0, n) < 0) return(-1);
	if (setword(freq) < 0) return(-1);
	if (needhinsi && setkbuf(hmax, idlist) < 0) return(-1);

	return(adddictlist(ptr, klen, maxstr - ptr - klen));
}

static u_int NEAR getfreq(cp)
char *cp;
{
	long freq;

	if (!cp || !*cp) freq = 0L;
	else {
		*(cp++) = '\0';
		if ((freq = strtol(cp, NULL, 10)) < 0L) freq = 0L;
		else if (freq > MAXUTYPE(u_short)) freq = MAXUTYPE(u_short);
	}

	return((u_int)freq);
}

static int NEAR addcannadict(str, buf)
CONST char *str;
char *buf;
{
	CONST char *kstr;
	char *cp;
	u_int freq;
	int n, ptr;

	cp = NULL;
	for (ptr = 1; buf[ptr]; ptr++) {
		if (buf[ptr] == '*') cp = &(buf[ptr]);
		else if (isspace2(buf[ptr])) break;
		else if (!isalnum2(buf[ptr])) return(0);
	}
	if (ptr <= 1) return(0);
	buf[ptr++] = '\0';

	freq = getfreq(cp);
	while (buf[ptr] && isspace2(buf[ptr])) ptr++;

	for (n = 0; buf[ptr]; n++) {
		if (buf[ptr] == '#') {
			if (addcannadict(str, &(buf[ptr])) < 0) return(-1);
			return(1);
		}

		kstr = &(buf[ptr]);
		while (buf[ptr] && !isspace2(buf[ptr])) ptr++;
		buf[ptr++] = '\0';
		if (*kstr == '@') kstr = str;
		if (adddict(str, kstr, buf, freq) < 0) return(-1);
	}

	return(1);
}

static int NEAR convdict(size, fp)
off_t size;
FILE *fp;
{
	char *str, *kstr, *hstr, buf[MAXLINESTR + 1];
	long ofs;
	int n, ptr, tmp;

	n = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		if (verbose && size) {
			if ((ofs = ftell(fp)) < 0L) ofs = 0L;
			tmp = (int)((ofs * MAXRANGE) / size);
			for (; n < tmp; n++) fputc(VERBOSECHAR, stdout);
			fflush(stdout);
		}

		ptr = 0;
		while (buf[ptr] && isspace2(buf[ptr])) ptr++;
		if (!buf[ptr]) continue;

		str = &(buf[ptr]);
		for (; buf[ptr]; ptr++) {
			if (iskanji1(buf, ptr)) ptr++;
			else if (!iskana1(buf, &ptr)) break;
		}
		if (!buf[ptr] || !isspace2(buf[ptr])) continue;

		buf[ptr++] = '\0';
		while (buf[ptr] && isspace2(buf[ptr])) ptr++;
		if (!buf[ptr]) continue;

		if (buf[ptr] == '#'
		&& (tmp = addcannadict(str, &(buf[ptr])))) {
			if (tmp < 0) return(-1);
			continue;
		}

		kstr = &(buf[ptr++]);
		while (buf[ptr] && !isspace2(buf[ptr])) ptr++;

		if (!(buf[ptr])) hstr = NULL;
		else {
			buf[ptr++] = '\0';
			while (buf[ptr] && isspace2(buf[ptr])) ptr++;
			if (!buf[ptr]) hstr = NULL;
			else {
				hstr = &(buf[ptr++]);
				while (buf[ptr] && !isspace2(buf[ptr])) ptr++;
			}
		}

		if (adddict(str, kstr, hstr, getfreq(&(buf[ptr]))) < 0)
			return(-1);
	}
	if (verbose) {
		for (; n < MAXRANGE; n++) fputc(VERBOSECHAR, stdout);
		fputs("  done.\n", stdout);
	}

	return(0);
}

static int NEAR addhinsidict(VOID_A)
{
	ALLOC_T ptr, klen;
	int i;

	for (i = 0; i < CONLISTSIZ; i++) {
		ptr = maxstr;
		if (setkbuf(conlist[i].klen, conlist[i].kbuf) < 0) return(-1);
		klen = maxstr - ptr;
		if (setkbuf(conlist[i].klen, conlist[i].kbuf) < 0) return(-1);
		if (setword(0) < 0) return(-1);
		if (setkbuf(1, &(conlist[i].id)) < 0) return(-1);

		if (adddictlist(ptr, klen, maxstr - ptr - klen)) return(-1);
	}

	return(0);
}

static int NEAR addconlist(n, max, list)
int n, max;
CONST contable *list;
{
	ALLOC_T ptr;
	CONST u_short *idlist;
	int i, len;

	idlist = NULL;
	len = 0;
	for (i = 0; i < max; i++) if (list[i].id == n) {
		idlist = list[i].next;
		len = list[i].nlen;
		break;
	}

	ptr = maxstr;
	if (sethinsi(len, idlist) < 0) return(-1);
	if (adddictlist(ptr, (ALLOC_T)0, maxstr - ptr)) return(-1);

	return(0);
}

static int NEAR genconlist(VOID_A)
{
	int i;

	free(dictlist);
	dictlist = NULL;
	maxdict = dictlistsize = 0L;
	free(strbuf);
	strbuf = NULL;
	maxstr = strbufsize = (ALLOC_T)0;

	for (i = HN_MIN; i < HN_MAX; i++)
		if (addconlist(i, JIRCONLISTSIZ, jirconlist) < 0) return(-1);
	for (i = FZ_MIN; i < FZ_MAX; i++)
		if (addconlist(i, CONLISTSIZ, conlist) < 0) return(-1);
	for (i = SH_MIN; i < SH_MAX; i++)
		if (addconlist(i, SHUUTANLISTSIZ, shuutanlist) < 0) return(-1);

	return(0);
}

static int NEAR cmpjisbuf(buf1, len1, buf2, len2)
CONST u_char *buf1;
int len1;
CONST u_char *buf2;
int len2;
{
	int i, c1, c2;

	for (i = 0; i < len1; i += 2) {
		if (i >= len2) return(1);
		c1 = getword(buf1, i);
		c2 = getword(buf2, i);
		if (c1 > c2) return(1);
		else if (c1 < c2) return(-1);
	}
	if (i < len2) return(-1);

	return(0);
}

static int cmpdict(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	dicttable *dp1, *dp2;
	u_char *buf1, *buf2;
	int n, lvl, len1, len2;

	dp1 = (dicttable *)vp1;
	dp2 = (dicttable *)vp2;

	lvl = 3;
	buf1 = &(strbuf[dp1 -> ptr]);
	buf2 = &(strbuf[dp2 -> ptr]);
	len1 = *(buf1++) * 2;
	len2 = *(buf2++) * 2;
	if ((n = cmpjisbuf(buf1, len1, buf2, len2))) return(n * lvl);

	lvl--;
	buf1 += len1;
	buf2 += len2;
	len1 = *(buf1++) * 2;
	len2 = *(buf2++) * 2;
	if ((n = cmpjisbuf(buf1, len1, buf2, len2))) return(n * lvl);
	if (!needhinsi) return(0);

	lvl--;
	buf1 += len1 + 2;
	buf2 += len2 + 2;
	len1 = *(buf1++) * 2;
	len2 = *(buf2++) * 2;
	if ((n = cmpjisbuf(buf1, len1, buf2, len2))) return(n * lvl);

	return(0);
}

static int NEAR fputbyte(c, fp)
int c;
FILE *fp;
{
	return((fputc(c, fp) == EOF && ferror(fp)) ? -1 : 0);
}

static int NEAR fputword(w, fp)
u_int w;
FILE *fp;
{
	if (fputbyte((int)(w & 0xff), fp) < 0
	|| fputbyte((int)((w >> 8) & 0xff), fp) < 0)
		return(-1);

	return(0);
}

static int NEAR fputdword(dw, fp)
long dw;
FILE *fp;
{
	if (fputword((u_int)(dw & 0xffff), fp) < 0
	|| fputword((u_int)((dw >> 16) & 0xffff), fp) < 0)
		return(-1);

	return(0);
}

static int NEAR writeindex(fp)
FILE *fp;
{
	u_char *buf;
	char tmp[4 + 1];
	ALLOC_T ptr;
	long i, j, n, max;
	int len;

	for (i = max = 0L; i < maxdict; i++) {
		if (!(dictlist[i].size)) continue;
		max++;
		dictlist[i].max = 1;
		for (j = 1L; i + j < maxdict; j++) {
			if (!(dictlist[i + j].size)) continue;
			n = cmpdict(&(dictlist[i + j - 1]),
				&(dictlist[i + j]));
			if (n < -2 || n > 2) break;
			dictlist[i].max++;
			dictlist[i + j].max = 0;
			if (!n) dictlist[i + j].size = (ALLOC_T)0;
		}
		i += j - 1;
	}

	if (fputdword(max, fp) < 0) return(-1);

	ptr = (ALLOC_T)0;
	for (i = 0L; i < maxdict; i++) {
		if (!(dictlist[i].size)) continue;
		if (fputdword(ptr, fp) < 0) return(-1);
		ptr += dictlist[i].klen;
		ptr++;
		for (j = max = 0L; j < dictlist[i].max; j++) {
			if (!(dictlist[i + j].size)) continue;
			if (++max > MAXUTYPE(u_char)) {
				if (verbose) {
					buf = &(strbuf[dictlist[i + j].ptr]);
					len = *(buf++) * 2;
					buf += len;
					len = *(buf++) * 2;
					while (len > 0) {
						VOID_C jis2str(tmp,
							getword(buf, 0));
						fprintf(stderr, "%s", tmp);
						buf += 2;
						len -= 2;
					}
					fprintf(stderr,
		": Too many entries for the same reading (ignored).\n");
				}
				continue;
			}
			ptr += dictlist[i + j].size;
		}
		i += dictlist[i].max - 1;
	}
	if (fputdword(ptr, fp) < 0) return(-1);

	return(0);
}

static int NEAR writedict(fp)
FILE *fp;
{
	u_char *buf;
	long i, j, max;

	for (i = 0L; i < maxdict; i++) {
		if (!(dictlist[i].size)) continue;
		buf = &(strbuf[dictlist[i].ptr]);
		if (fwrite(buf, dictlist[i].klen, 1, fp) != 1) return(-1);
		for (j = max = 0L; j < dictlist[i].max; j++)
			if (dictlist[i + j].size) max++;
		if (max > MAXUTYPE(u_char)) max = MAXUTYPE(u_char);
		if (fputbyte(max, fp) < 0) return(-1);
		for (j = max = 0L; j < dictlist[i].max; j++) {
			if (!(dictlist[i + j].size)) continue;
			if (++max > MAXUTYPE(u_char)) break;

			buf = &(strbuf[dictlist[i + j].ptr]);
			buf += dictlist[i + j].klen;
			if (fwrite(buf, dictlist[i + j].size, 1, fp) != 1)
				return(-1);
		}
		i += dictlist[i].max - 1;
	}

	return(0);
}

static int NEAR writehinsiindex(fp)
FILE *fp;
{
	ALLOC_T ptr;
	long i;

	if (fputword(maxdict, fp) < 0) return(-1);

	ptr = (ALLOC_T)0;
	for (i = 0L; i < maxdict; i++) {
		if (fputword(ptr, fp) < 0) return(-1);
		ptr += dictlist[i].size;
	}
	if (fputword(ptr, fp) < 0) return(-1);

	return(0);
}

static int NEAR writehinsidict(fp)
FILE *fp;
{
	u_char *buf;
	long i;

	for (i = 0L; i < maxdict; i++) {
		buf = &(strbuf[dictlist[i].ptr]);
		if (fwrite(buf, dictlist[i].size, 1, fp) != 1) return(-1);
	}

	return(0);
}

int main(argc, argv)
int argc;
char *CONST argv[];
{
	FILE *fpin, *fpout;
	struct stat st;
	off_t size;
	int i, c, n, max;

	n = 1;
	if (n < argc && !strcmp(argv[n], "-h")) {
		needhinsi++;
		n++;
	}
	if (n < argc && !strcmp(argv[n], "-v")) {
		verbose++;
		n++;
	}

	if (n >= argc) {
		fprintf(stderr,
			"Usage: %s [-h] [-v] outfile [infile...]\n", argv[0]);
		return(1);
	}

	if (!(fpout = fopen(argv[n], "wb"))) {
		fprintf(stderr, "Cannot open file.\n");
		return(1);
	}

	qsort(hinsilist, HINSILISTSIZ, sizeof(hinsitable), cmphinsi);
	for (i = n + 1; i < argc; i++) {
		size = (off_t)0;
		if (verbose) {
			fprintf(stdout, "%s:\n", argv[i]);
			if (stat(argv[i], &st) >= 0) size = st.st_size;
		}
		if (!(fpin = fopen(argv[i], "r"))) {
			fprintf(stderr, "%s: Cannot open file.\n", argv[i]);
			continue;
		}
		c = convdict(size, fpin);
		fclose(fpin);
		if (c < 0) break;
	}

	max = maxdict;
	if (needhinsi && addhinsidict() < 0) {
		needhinsi = 0;
		maxdict = max;
	}

	if (verbose) {
		fprintf(stdout, "sorting...");
		fflush(stdout);
	}
	qsort(dictlist, maxdict, sizeof(dicttable), cmpdict);
	if (verbose) {
		fputs("  done.\n", stdout);
		fprintf(stdout, "writing...");
		fflush(stdout);
	}

	if (writeindex(fpout) < 0 || writedict(fpout) < 0) {
		fprintf(stderr, "%s: Cannot write file.\n", argv[n]);
		return(1);
	}

	if (!needhinsi) {
		if (fputword(0, fpout) < 0) return(-1);
	}
	else {
		if (genconlist() < 0) return(1);
		if (writehinsiindex(fpout) < 0 || writehinsidict(fpout) < 0) {
			fprintf(stderr, "%s: Cannot write file.\n", argv[n]);
			return(1);
		}
	}

	fclose(fpout);
	if (verbose) fputs("  done.\n", stdout);

	return(0);
}
