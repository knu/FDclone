/*
 *	mktankan.c
 *
 *	Tan-Kanji translation table generator
 */

#include "machine.h"
#include <stdio.h>

#ifndef	NOUNISTDH
#include <unistd.h>
#endif
#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include "printf.h"
#include "kctype.h"
#include "roman.h"

#define	MAXKANASTR	255
#define	DEFFREQ		0
#define	DEFTK(s,c)	{s, strsize(s), c}

typedef struct _tankantable {
	CONST char *str;
	ALLOC_T len;
	u_short code;
} tankantable;

extern romantable *romanlist;

static VOID NEAR roman2str __P_((char *, CONST char *, ALLOC_T));
int main __P_((int, char *CONST []));

static CONST u_short tankanstr[] = { 0x4331, 0x3441, 0x3b7a };
static CONST tankantable tankanlist[] = {
	DEFTK("a",	0x3021),
	DEFTK("ai",	0x3025),
	DEFTK("hou",	0x3029),	/* a + u */
	DEFTK("aoi",	0x302a),
	DEFTK("akane",	0x302b),
	DEFTK("aki",	0x302c),
	DEFTK("aku",	0x302d),
	DEFTK("asahi",	0x3030),
	DEFTK("asi",	0x3031),
	DEFTK("aji",	0x3033),
	DEFTK("azusa",	0x3034),
	DEFTK("atu",	0x3035),
	DEFTK("kyuu",	0x3037),	/* atsuka + u */
	DEFTK("ate",	0x3038),
	DEFTK("ane",	0x3039),
	DEFTK("abu",	0x303a),
	DEFTK("ame",	0x303b),
	DEFTK("aya",	0x303c),
	DEFTK("ayu",	0x303e),
	DEFTK("aru",	0x303f),
	DEFTK("awa",	0x3040),
	DEFTK("awase",	0x3041),
	DEFTK("an",	0x3042),

	DEFTK("i",	0x304a),
	DEFTK("iki",	0x3068),
	DEFTK("iku",	0x3069),
	DEFTK("iso",	0x306b),
	DEFTK("iti",	0x306c),
	DEFTK("itu",	0x306e),
	DEFTK("ine",	0x3070),
	DEFTK("ibara",	0x3071),
	DEFTK("imo",	0x3072),
	DEFTK("iwasi",	0x3073),
	DEFTK("in",	0x3074),

	DEFTK("u",	0x3126),
	DEFTK("ki",	0x312e),	/* ukaga + u */
	DEFTK("usi",	0x312f),
	DEFTK("usu",	0x3130),
	DEFTK("uzu",	0x3132),
	DEFTK("uso",	0x3133),
	DEFTK("uta",	0x3134),
	DEFTK("utu",	0x3135),
	DEFTK("unagi",	0x3137),
	DEFTK("uba",	0x3138),
	DEFTK("umaya",	0x3139),
	DEFTK("ura",	0x313a),
	DEFTK("uri",	0x313b),
	DEFTK("uruu",	0x313c),
	DEFTK("uwasa",	0x313d),
	DEFTK("un",	0x313e),

	DEFTK("e",	0x3141),
	DEFTK("ei",	0x3143),
	DEFTK("eki",	0x3155),
	DEFTK("etu",	0x3159),
	DEFTK("enoki",	0x315d),
	DEFTK("en",	0x315e),

	DEFTK("o",	0x3177),
	DEFTK("oi",	0x3179),
	DEFTK("ou",	0x317a),
	DEFTK("oka",	0x322c),
	DEFTK("oki",	0x322d),
	DEFTK("ogi",	0x322e),
	DEFTK("oku",	0x322f),
	DEFTK("oke",	0x3233),
	DEFTK("osu",	0x3234),
	DEFTK("otu",	0x3235),
	DEFTK("ore",	0x3236),
	DEFTK("on",	0x3237),

	DEFTK("ka",	0x323c),
	DEFTK("ga",	0x3264),
	DEFTK("kai",	0x3270),
	DEFTK("gai",	0x332e),
	DEFTK("kairi",	0x333d),
	DEFTK("kaori",	0x333e),
	DEFTK("kaeru",	0x333f),
	DEFTK("kaki",	0x3340),
	DEFTK("kagi",	0x3343),
	DEFTK("kaku",	0x3344),
	DEFTK("gaku",	0x3358),
	DEFTK("kake",	0x335d),
	DEFTK("kasa",	0x335e),
	DEFTK("kasi",	0x335f),
	DEFTK("kaji",	0x3361),
	DEFTK("kajika",	0x3362),
	DEFTK("kata",	0x3363),
	DEFTK("katu",	0x3364),
	DEFTK("katuo",	0x336f),
	DEFTK("kanou",	0x3370),
	DEFTK("kaba",	0x3371),
	DEFTK("kaban",	0x3373),
	DEFTK("kabu",	0x3374),
	DEFTK("kabuto",	0x3375),
	DEFTK("kago",	0x3376),
	DEFTK("kama",	0x3377),
	DEFTK("kamu",	0x337a),
	DEFTK("kamo",	0x337b),
	DEFTK("kasiwa",	0x337c),
	DEFTK("kaya",	0x337d),
	DEFTK("kayu",	0x3421),
	DEFTK("kari",	0x3422),
	DEFTK("kawara",	0x3424),
	DEFTK("kan",	0x3425),
	DEFTK("gan",	0x345d),

	DEFTK("ki",	0x346b),
	DEFTK("gi",	0x3536),
	DEFTK("kiku",	0x3545),
	DEFTK("kiti",	0x3548),
	DEFTK("kitu",	0x3549),
	DEFTK("kinuta",	0x354e),
	DEFTK("kine",	0x354f),
	DEFTK("kibi",	0x3550),
	DEFTK("kyaku",	0x3551),
	DEFTK("gyaku",	0x3554),
	DEFTK("kyuu",	0x3556),
	DEFTK("gyuu",	0x356d),
	DEFTK("kyo",	0x356e),
	DEFTK("gyo",	0x3579),
	DEFTK("kyou",	0x357c),
	DEFTK("gyou",	0x3644),
	DEFTK("kyoku",	0x3649),
	DEFTK("gyoku",	0x364c),
	DEFTK("kiri",	0x364d),
	DEFTK("kiro",	0x364e),
	DEFTK("kin",	0x364f),
	DEFTK("gin",	0x3663),

	DEFTK("ku",	0x3665),
	DEFTK("gu",	0x3671),
	DEFTK("kuu",	0x3674),
	DEFTK("guu",	0x3676),
	DEFTK("kusi",	0x367a),
	DEFTK("kusiro",	0x367c),
	DEFTK("kuzu",	0x367d),
	DEFTK("kutu",	0x367e),
	DEFTK("kutuwa",	0x3725),
	DEFTK("kubo",	0x3726),
	DEFTK("kuma",	0x3727),
	DEFTK("kume",	0x3729),
	DEFTK("kuri",	0x372a),
	DEFTK("kuwa",	0x372c),
	DEFTK("kun",	0x372e),
	DEFTK("gun",	0x3732),

	DEFTK("ke",	0x3735),
	DEFTK("kei",	0x3738),
	DEFTK("gei",	0x375d),
	DEFTK("geki",	0x3760),
	DEFTK("keta",	0x3765),
	DEFTK("ketu",	0x3766),
	DEFTK("getu",	0x376e),
	DEFTK("ken",	0x376f),
	DEFTK("gen",	0x3835),

	DEFTK("ko",	0x3843),
	DEFTK("go",	0x385e),
	DEFTK("kotu",	0x3870),	/* ko + i */
	DEFTK("koi",	0x3871),
	DEFTK("kou",	0x3872),
	DEFTK("gou",	0x3964),
	DEFTK("kouji",	0x396d),
	DEFTK("koku",	0x396e),
	DEFTK("goku",	0x3976),
	DEFTK("roku",	0x3977),	/* ko + si */
	DEFTK("kosi",	0x3978),
	DEFTK("kosiki",	0x3979),
	DEFTK("kotu",	0x397a),
	DEFTK("koma",	0x397d),
	DEFTK("komi",	0x397e),
	DEFTK("kono",	0x3a21),
	DEFTK("koro",	0x3a22),
	DEFTK("kon",	0x3a23),

	DEFTK("sa",	0x3a33),
	DEFTK("za",	0x3a41),
	DEFTK("sai",	0x3a44),
	DEFTK("zai",	0x3a5e),
	DEFTK("go",	0x3a63),	/* sa + eru */
	DEFTK("saka",	0x3a64),
	DEFTK("sakai",	0x3a66),
	DEFTK("sakaki",	0x3a67),
	DEFTK("sakana",	0x3a68),
	DEFTK("saki",	0x3a69),
	DEFTK("sagi",	0x3a6d),
	DEFTK("saku",	0x3a6e),
	DEFTK("sakura",	0x3a79),
	DEFTK("sake",	0x3a7a),
	DEFTK("sasa",	0x3a7b),
	DEFTK("saji",	0x3a7c),
	DEFTK("satu",	0x3a7d),
	DEFTK("zatu",	0x3b28),
	DEFTK("satuki",	0x3b29),
	DEFTK("saba",	0x3b2a),
	DEFTK("betu",	0x3b2b),	/* saba + ku */
	DEFTK("sabi",	0x3b2c),
	DEFTK("same",	0x3b2d),
	DEFTK("sara",	0x3b2e),
	DEFTK("sarasi",	0x3b2f),
	DEFTK("san",	0x3b30),
	DEFTK("zan",	0x3b42),

	DEFTK("si",	0x3b45),
	DEFTK("ji",	0x3b76),
	DEFTK("sio",	0x3c2e),
	DEFTK("sika",	0x3c2f),
	DEFTK("siki",	0x3c30),
	DEFTK("sigi",	0x3c32),
	DEFTK("jiku",	0x3c33),
	DEFTK("sisi",	0x3c35),
	DEFTK("sizuku",	0x3c36),
	DEFTK("siti",	0x3c37),
	DEFTK("situ",	0x3c38),
	DEFTK("jitu",	0x3c42),
	DEFTK("sitomi",	0x3c43),
	DEFTK("sino",	0x3c44),
	DEFTK("si",	0x3c45),	/* sino + bu */
	DEFTK("siba",	0x3c46),
	DEFTK("sibe",	0x3c49),
	DEFTK("sima",	0x3c4a),
	DEFTK("sha",	0x3c4b),
	DEFTK("ja",	0x3c58),
	DEFTK("shaku",	0x3c5a),
	DEFTK("jaku",	0x3c63),
	DEFTK("shu",	0x3c67),
	DEFTK("ju",	0x3c74),
	DEFTK("shuu",	0x3c7c),
	DEFTK("juu",	0x3d3a),
	DEFTK("shuku",	0x3d47),
	DEFTK("juku",	0x3d4e),
	DEFTK("shutu",	0x3d50),
	DEFTK("jutu",	0x3d51),
	DEFTK("shun",	0x3d53),
	DEFTK("jun",	0x3d5a),
	DEFTK("sho",	0x3d68),
	DEFTK("jo",	0x3d75),
	DEFTK("shou",	0x3d7d),
	DEFTK("jou",	0x3e65),
	DEFTK("shoku",	0x3e7c),
	DEFTK("joku",	0x3f2b),
	DEFTK("siri",	0x3f2c),
	DEFTK("sin",	0x3f2d),
	DEFTK("jin",	0x3f4d),

	DEFTK("su",	0x3f5a),
	DEFTK("zu",	0x3f5e),
	DEFTK("sui",	0x3f61),
	DEFTK("zui",	0x3f6f),
	DEFTK("suu",	0x3f72),
	DEFTK("sue",	0x3f78),
	DEFTK("sugi",	0x3f79),
	DEFTK("suge",	0x3f7b),
	DEFTK("ha",	0x3f7c),	/* sukobu + ru */
	DEFTK("suzume",	0x3f7d),
	DEFTK("suso",	0x3f7e),
	DEFTK("chou",	0x4021),	/* su + mu */
	DEFTK("suri",	0x4022),
	DEFTK("sun",	0x4023),

	DEFTK("se",	0x4024),
	DEFTK("ze",	0x4027),
	DEFTK("sei",	0x4028),
	DEFTK("zei",	0x4047),
	DEFTK("seki",	0x4049),
	DEFTK("setu",	0x405a),
	DEFTK("zetu",	0x4064),
	DEFTK("semi",	0x4066),
	DEFTK("sen",	0x4067),
	DEFTK("zen",	0x4130),
	DEFTK("senti",	0x4138),

	DEFTK("so",	0x4139),
	DEFTK("sou",	0x414e),
	DEFTK("zou",	0x417c),
	DEFTK("soku",	0x4225),
	DEFTK("zoku",	0x422f),
	DEFTK("sotu",	0x4234),
	DEFTK("sode",	0x4235),
	DEFTK("sono",	0x4236),
	DEFTK("sen",	0x4237),	/* soro + u */
	DEFTK("son",	0x4238),

	DEFTK("ta",	0x423e),
	DEFTK("da",	0x4242),
	DEFTK("tai",	0x424e),
	DEFTK("dai",	0x4265),
	DEFTK("taka",	0x426b),
	DEFTK("taki",	0x426c),
	DEFTK("taku",	0x426e),
	DEFTK("daku",	0x4279),
	DEFTK("take",	0x427b),
	DEFTK("tako",	0x427c),
	DEFTK("tada",	0x427e),
	DEFTK("kou",	0x4321),	/* tata + ku */
	DEFTK("tan",	0x4322),	/* tada + si */
	DEFTK("tatu",	0x4323),
	DEFTK("datu",	0x4325),
	DEFTK("tatumi",	0x4327),
	DEFTK("tate",	0x4328),
	DEFTK("ten",	0x4329),	/* tado + ru */
	DEFTK("tana",	0x432a),
	DEFTK("tani",	0x432b),
	DEFTK("tanuki",	0x432c),
	DEFTK("tara",	0x432d),
	DEFTK("taru",	0x432e),
	DEFTK("dare",	0x432f),
	DEFTK("tan",	0x4330),
	DEFTK("dan",	0x4344),

	DEFTK("ti",	0x434d),
	DEFTK("tiku",	0x435b),
	DEFTK("titu",	0x4361),
	DEFTK("cha",	0x4363),
	DEFTK("chaku",	0x4364),
	DEFTK("chuu",	0x4366),
	DEFTK("cho",	0x4374),
	DEFTK("chou",	0x437a),
	DEFTK("choku",	0x443c),
	DEFTK("tin",	0x443f),

	DEFTK("tu",	0x4445),
	DEFTK("tui",	0x4446),
	DEFTK("tuu",	0x444b),
	DEFTK("tuka",	0x444d),
	DEFTK("tuga",	0x444e),
	DEFTK("kaku",	0x444f),	/* tuka + mu */
	DEFTK("tuki",	0x4450),
	DEFTK("tukuda",	0x4451),
	DEFTK("tuke",	0x4452),
	DEFTK("tuge",	0x4453),
	DEFTK("tuji",	0x4454),
	DEFTK("tuta",	0x4455),
	DEFTK("tuduri",	0x4456),
	DEFTK("tuba",	0x4457),
	DEFTK("tubaki",	0x4458),
	DEFTK("kai",	0x4459),	/* tubu + su */
	DEFTK("tubo",	0x445a),
	DEFTK("tuma",	0x445c),
	DEFTK("tumugi",	0x445d),
	DEFTK("tume",	0x445e),
	DEFTK("turi",	0x445f),
	DEFTK("turu",	0x4461),

	DEFTK("tei",	0x4462),
	DEFTK("dei",	0x4525),
	DEFTK("teki",	0x4526),
	DEFTK("deki",	0x452e),
	DEFTK("tetu",	0x452f),
	DEFTK("ten",	0x4535),
	DEFTK("den",	0x4541),

	DEFTK("to",	0x4546),
	DEFTK("do",	0x4558),
	DEFTK("tou",	0x455d),
	DEFTK("dou",	0x462f),
	DEFTK("touge",	0x463d),
	DEFTK("toki",	0x463e),
	DEFTK("toku",	0x463f),
	DEFTK("doku",	0x4647),
	DEFTK("toti",	0x464a),
	DEFTK("totu",	0x464c),
	DEFTK("todo",	0x464e),
	DEFTK("todoke",	0x464f),
	DEFTK("tobi",	0x4650),
	DEFTK("toma",	0x4651),
	DEFTK("tora",	0x4652),
	DEFTK("tori",	0x4653),
	DEFTK("toro",	0x4654),
	DEFTK("ton",	0x4655),
	DEFTK("don",	0x465d),

	DEFTK("na",	0x4660),
	DEFTK("nai",	0x4662),
	DEFTK("nagara",	0x4663),
	DEFTK("nagi",	0x4664),
	DEFTK("nazo",	0x4666),
	DEFTK("nada",	0x4667),
	DEFTK("natu",	0x4668),
	DEFTK("nabe",	0x4669),
	DEFTK("nara",	0x466a),
	DEFTK("nare",	0x466b),
	DEFTK("nawa",	0x466c),
	DEFTK("nawate",	0x466d),
	DEFTK("nan",	0x466e),
	DEFTK("nanji",	0x4672),

	DEFTK("ni",	0x4673),
	DEFTK("nioi",	0x4677),
	DEFTK("sin",	0x4678),	/* nigi + wau */
	DEFTK("niku",	0x4679),
	DEFTK("niji",	0x467a),
	DEFTK("nijuu",	0x467b),
	DEFTK("niti",	0x467c),
	DEFTK("nyuu",	0x467d),
	DEFTK("nyo",	0x4721),
	DEFTK("nyou",	0x4722),
	DEFTK("nira",	0x4723),
	DEFTK("nin",	0x4724),

	DEFTK("nure",	0x4728),

	DEFTK("ne",	0x4729),
	DEFTK("nei",	0x472b),
	DEFTK("negi",	0x472c),
	DEFTK("neko",	0x472d),
	DEFTK("netu",	0x472e),
	DEFTK("nen",	0x472f),

	DEFTK("no",	0x4735),
	DEFTK("nou",	0x4739),
	DEFTK("si",	0x4741),	/* nozo + ku */
	DEFTK("nomi",	0x4742),

	DEFTK("ha",	0x4743),
	DEFTK("ba",	0x474c),
	DEFTK("hai",	0x4750),
	DEFTK("bai",	0x475c),
	DEFTK("sha",	0x4767),	/* ha + u */
	DEFTK("hae",	0x4768),
	DEFTK("hakari",	0x4769),
	DEFTK("sin",	0x476a),	/* ha + gu */
	DEFTK("hagi",	0x476b),
	DEFTK("haku",	0x476c),
	DEFTK("baku",	0x4778),
	DEFTK("hako",	0x4821),
	DEFTK("hazama",	0x4823),
	DEFTK("hasi",	0x4824),
	DEFTK("hajime",	0x4825),
	DEFTK("hazu",	0x4826),
	DEFTK("haze",	0x4827),
	DEFTK("hata",	0x4828),
	DEFTK("hada",	0x4829),
	DEFTK("hatake",	0x482a),
	DEFTK("hati",	0x482c),
	DEFTK("hatu",	0x482e),
	DEFTK("batu",	0x4832),
	DEFTK("hato",	0x4837),
	DEFTK("hanasi",	0x4838),
	DEFTK("haniwa",	0x4839),
	DEFTK("hamaguri",	0x483a),
	DEFTK("hayabusa",	0x483b),
	DEFTK("han",	0x483c),
	DEFTK("ban",	0x4854),

	DEFTK("hi",	0x485b),
	DEFTK("bi",	0x4877),
	DEFTK("hiiragi",	0x4922),
	DEFTK("hie",	0x4923),
	DEFTK("hiki",	0x4924),
	DEFTK("hige",	0x4926),
	DEFTK("hiko",	0x4927),
	DEFTK("hiza",	0x4928),
	DEFTK("hisi",	0x4929),
	DEFTK("hiji",	0x492a),
	DEFTK("hitu",	0x492b),
	DEFTK("hinoki",	0x4930),
	DEFTK("hime",	0x4931),
	DEFTK("himo",	0x4933),
	DEFTK("hyaku",	0x4934),
	DEFTK("byuu",	0x4935),
	DEFTK("hyou",	0x4936),
	DEFTK("byou",	0x4940),
	DEFTK("hiru",	0x4947),
	DEFTK("hire",	0x4949),
	DEFTK("hin",	0x494a),
	DEFTK("bin",	0x4952),

	DEFTK("fu",	0x4954),
	DEFTK("bu",	0x496e),
	DEFTK("fuu",	0x4975),
	DEFTK("fuki",	0x4978),
	DEFTK("fuku",	0x497a),
	DEFTK("futi",	0x4a25),
	DEFTK("futu",	0x4a26),
	DEFTK("butu",	0x4a29),
	DEFTK("funa",	0x4a2b),
	DEFTK("fun",	0x4a2c),
	DEFTK("bun",	0x4a38),

	DEFTK("hei",	0x4a3a),
	DEFTK("bei",	0x4a46),
	DEFTK("pe-ji",	0x4a47),
	DEFTK("heki",	0x4a48),
	DEFTK("betu",	0x4a4c),
	DEFTK("hera",	0x4a4f),
	DEFTK("hen",	0x4a50),
	DEFTK("ben",	0x4a58),

	DEFTK("ho",	0x4a5d),
	DEFTK("bo",	0x4a67),
	DEFTK("hou",	0x4a6f),
	DEFTK("bou",	0x4b33),
	DEFTK("bai",	0x4b4a),	/* ho + eru */
	DEFTK("hoo",	0x4b4b),
	DEFTK("hoku",	0x4b4c),
	DEFTK("boku",	0x4b4d),
	DEFTK("botan",	0x4b55),
	DEFTK("botu",	0x4b56),
	DEFTK("hotondo",	0x4b58),
	DEFTK("hori",	0x4b59),
	DEFTK("horo",	0x4b5a),
	DEFTK("hon",	0x4b5b),
	DEFTK("bon",	0x4b5e),

	DEFTK("ma",	0x4b60),
	DEFTK("mai",	0x4b64),
	DEFTK("mairu",	0x4b69),
	DEFTK("maki",	0x4b6a),
	DEFTK("maku",	0x4b6b),
	DEFTK("makura",	0x4b6d),
	DEFTK("maguro",	0x4b6e),
	DEFTK("masa",	0x4b6f),
	DEFTK("masu",	0x4b70),
	DEFTK("mata",	0x4b72),
	DEFTK("matu",	0x4b75),
	DEFTK("made",	0x4b78),
	DEFTK("mama",	0x4b79),
	DEFTK("mayu",	0x4b7a),
	DEFTK("maro",	0x4b7b),
	DEFTK("man",	0x4b7c),

	DEFTK("mi",	0x4c23),
	DEFTK("misaki",	0x4c28),
	DEFTK("mitu",	0x4c29),
	DEFTK("minato",	0x4c2b),
	DEFTK("mino",	0x4c2c),
	DEFTK("nen",	0x4c2d),	/* mino + ru */
	DEFTK("myaku",	0x4c2e),
	DEFTK("myou",	0x4c2f),
	DEFTK("miri",	0x4c30),
	DEFTK("min",	0x4c31),

	DEFTK("mu",	0x4c33),
	DEFTK("muku",	0x4c3a),
	DEFTK("muko",	0x4c3b),
	DEFTK("musume",	0x4c3c),

	DEFTK("mei",	0x4c3d),
	DEFTK("mesu",	0x4c46),
	DEFTK("metu",	0x4c47),
	DEFTK("men",	0x4c48),

	DEFTK("mo",	0x4c4e),
	DEFTK("mou",	0x4c51),
	DEFTK("cho",	0x4c59),	/* mou + keru */
	DEFTK("moku",	0x4c5a),
	DEFTK("moti",	0x4c5e),
	DEFTK("mottomo",	0x4c60),
	DEFTK("rei",	0x4c61),	/* modo + ru */
	DEFTK("momi",	0x4c62),
	DEFTK("morai",	0x4c63),
	DEFTK("mon",	0x4c64),
	DEFTK("monme",	0x4c68),

	DEFTK("ya",	0x4c69),
	DEFTK("yaku",	0x4c71),
	DEFTK("yasu",	0x4c77),
	DEFTK("yanagi",	0x4c78),
	DEFTK("yabu",	0x4c79),
	DEFTK("yari",	0x4c7a),

	DEFTK("yu",	0x4c7b),
	DEFTK("yui",	0x4d23),
	DEFTK("yuu",	0x4d24),

	DEFTK("yo",	0x4d3d),
	DEFTK("you",	0x4d43),
	DEFTK("yoku",	0x4d5d),
	DEFTK("yodo",	0x4d64),

	DEFTK("ra",	0x4d65),
	DEFTK("rai",	0x4d68),
	DEFTK("raku",	0x4d6c),
	DEFTK("ran",	0x4d70),

	DEFTK("ri",	0x4d78),
	DEFTK("riku",	0x4e26),
	DEFTK("ritu",	0x4e27),
	DEFTK("ryaku",	0x4e2b),
	DEFTK("ryuu",	0x4e2d),
	DEFTK("ryo",	0x4e37),
	DEFTK("ryou",	0x4e3b),
	DEFTK("ryoku",	0x4e4f),
	DEFTK("rin",	0x4e51),

	DEFTK("ru",	0x4e5c),
	DEFTK("rui",	0x4e5d),

	DEFTK("rei",	0x4e61),
	DEFTK("reki",	0x4e71),
	DEFTK("retu",	0x4e73),
	DEFTK("ren",	0x4e77),

	DEFTK("ro",	0x4f24),
	DEFTK("rou",	0x4f2b),
	DEFTK("roku",	0x4f3b),
	DEFTK("ron",	0x4f40),

	DEFTK("wa",	0x4f41),
	DEFTK("wai",	0x4f44),
	DEFTK("waki",	0x4f46),
	DEFTK("waku",	0x4f47),
	DEFTK("wasi",	0x4f49),
	DEFTK("wataru",	0x4f4a),
	DEFTK("sen",	0x4f4b),	/* wata + ru */
	DEFTK("wani",	0x4f4c),
	DEFTK("wabi",	0x4f4d),
	DEFTK("wara",	0x4f4e),
	DEFTK("warabi",	0x4f4f),
	DEFTK("wan",	0x4f50),

	DEFTK(NULL,	0x4f54),
};
#define	TANKANLISTSIZ	arraysize(tankanlist)


static VOID NEAR roman2str(buf, s, size)
char *buf;
CONST char *s;
ALLOC_T size;
{
	ALLOC_T ptr, len;
	int i, n;

	ptr = (ALLOC_T)0;
	while (size) {
#ifdef	FAKEUNINIT
		n = 0;		/* fake for -Wuninitialized */
#endif
		for (len = size; len > (ALLOC_T)0; len--)
			if ((n = searchroman(s, len)) >= 0) break;
		if (len <= (ALLOC_T)0) {
			if (*s == 'n') ptr += jis2str(&(buf[ptr]), J_NN);
			else if (*s == '-') ptr += jis2str(&(buf[ptr]), J_CHO);
			else if (*s == *(s + 1))
				ptr += jis2str(&(buf[ptr]), J_TSU);

			s++;
			size--;
			continue;
		}

		for (i = 0; i < R_MAXKANA; i++) {
			if (!romanlist[n].code[i]) break;
			ptr += jis2str(&(buf[ptr]), romanlist[n].code[i]);
		}
		s += len;
		size -= len;
	}
	buf[ptr] = '\0';
}

int main(argc, argv)
int argc;
char *CONST argv[];
{
	FILE *fp;
	char buf[MAXKANASTR + 1];
	char kbuf[MAXKLEN * R_MAXKANA + 1], tbuf[sizeof(tankanstr) + 1];
	u_short code;
	int n, len;

	if (argc < 2 || !(fp = fopen(argv[1], "wb"))) {
		fprintf(stderr, "Cannot open file.\n");
		return(1);
	}

	initroman();
	for (n = len = 0; n < arraysize(tankanstr); n++)
		len += jis2str(&(tbuf[len]), tankanstr[n]);
	for (n = 0; n < TANKANLISTSIZ - 1; n++) {
		if (!tankanlist[n].str) continue;
		roman2str(buf, tankanlist[n].str, tankanlist[n].len);
		code = tankanlist[n].code;
		while (code < tankanlist[n + 1].code) {
			VOID_C jis2str(kbuf, code++);
			fprintf(fp, "%s %s %s %d\n", buf, kbuf, tbuf, DEFFREQ);
			while (!VALIDJIS(code)) if (++code >= J_MAX) break;
		}
	}
	fclose(fp);

	return(0);
}
