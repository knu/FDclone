/*
 *	hinsi.h
 *
 *	definitions for Hinsi grammar
 */

#define	HN_MIN		0
#define	HN_SENTOU	0
#define	HN_SUUJI	1
#define	HN_KANA		2
#define	HN_EISUU	3
#define	HN_KIGOU	4
#define	HN_HEIKAKKO	5
#define	HN_FUZOKUGO	6
#define	HN_KAIKAKKO	7
#define	HN_GIJI		8
#define	HN_MEISHI	9
#define	HN_JINMEI	10
#define	HN_CHIMEI	11
#define	HN_JINCHI	12
#define	HN_KOYUU	13
#define	HN_SUUSHI	14
#define	HN_KA5DOU	15
#define	HN_GA5DOU	16
#define	HN_SA5DOU	17
#define	HN_TA5DOU	18
#define	HN_NA5DOU	19
#define	HN_BA5DOU	20
#define	HN_MA5DOU	21
#define	HN_RA5DOU	22
#define	HN_WA5DOU	23
#define	HN_1DOU		24
#define	HN_SAHDOU	25
#define	HN_ZAHDOU	26
#define	HN_KA5YOU	27
#define	HN_GA5YOU	28
#define	HN_SA5YOU	29
#define	HN_TA5YOU	30
#define	HN_NA5YOU	31
#define	HN_BA5YOU	32
#define	HN_MA5YOU	33
#define	HN_RA5YOU	34
#define	HN_WA5YOU	35
#define	HN_1YOU		36
#define	HN_SAHYOU	37
#define	HN_ZAHYOU	38
#define	HN_KA5DAN	39
#define	HN_GA5DAN	40
#define	HN_SA5DAN	41
#define	HN_TA5DAN	42
#define	HN_NA5DAN	43
#define	HN_BA5DAN	44
#define	HN_MA5DAN	45
#define	HN_RA5DAN	46
#define	HN_WA5DAN	47
#define	HN_1DAN		48
#define	HN_1_MEI	49
#define	HN_KA_IKU	50
#define	HN_KO_KO	51
#define	HN_KI_KI	52
#define	HN_KU_KU	53
#define	HN_SAHEN	54
#define	HN_ZAHEN	55
#define	HN_SA_MEI	56
#define	HN_SI_SI	57
#define	HN_SU_SU	58
#define	HN_SE_SE	59
#define	HN_RA_KUD	60
#define	HN_KEIYOU	61
#define	HN_KEIDOU	62
#define	HN_KD_MEI	63
#define	HN_KD_TAR	64
#define	HN_FUKUSI	65
#define	HN_RENTAI	66
#define	HN_SETKAN	67
#define	HN_TANKAN	68
#define	HN_SETTOU	69
#define	HN_SETUBI	70
#define	HN_SETOSU	71
#define	HN_JOSUU	72
#define	HN_SETOJO	73
#define	HN_SETUJO	74
#define	HN_SETUJN	75
#define	HN_SETOCH	76
#define	HN_SETUCH	77
#define	HN_SETO_O	78
#define	HN_SETO_K	79
#define	HN_KD_STB	80
#define	HN_SA_M_S	81
#define	HN_SETUDO	82
#define	HN_KE_SED	83
#define	HN_MAX		84

#define	FZ_MIN		(HN_MAX + 0)
#define	FZ_BAN		(HN_MAX + 0)
#define	FZ_COMMA	(HN_MAX + 1)
#define	FZ_PERIOD	(HN_MAX + 2)
#define	FZ_QUEST	(HN_MAX + 3)
#define	FZ_TOUTEN	(HN_MAX + 4)
#define	FZ_KUTEN	(HN_MAX + 5)
#define	FZ_ZENPERIOD	(HN_MAX + 6)
#define	FZ_ZENQUEST	(HN_MAX + 7)
#define	FZ_ZENBAN	(HN_MAX + 8)
#define	FZ_A		(HN_MAX + 9)
#define	FZ_I		(HN_MAX + 10)
#define	FZ_I2		(HN_MAX + 11)
#define	FZ_I3		(HN_MAX + 12)
#define	FZ_I4		(HN_MAX + 13)
#define	FZ_I6		(HN_MAX + 14)
#define	FZ_I5		(HN_MAX + 15)
#define	FZ_I7		(HN_MAX + 16)
#define	FZ_I8		(HN_MAX + 17)
#define	FZ_I9		(HN_MAX + 18)
#define	FZ_IKA		(HN_MAX + 19)
#define	FZ_IKI		(HN_MAX + 20)
#define	FZ_IKU		(HN_MAX + 21)
#define	FZ_IKE		(HN_MAX + 22)
#define	FZ_IKO		(HN_MAX + 23)
#define	FZ_IXTSU	(HN_MAX + 24)
#define	FZ_U		(HN_MAX + 25)
#define	FZ_U2		(HN_MAX + 26)
#define	FZ_U3		(HN_MAX + 27)
#define	FZ_E		(HN_MAX + 28)
#define	FZ_O		(HN_MAX + 29)
#define	FZ_OI		(HN_MAX + 30)
#define	FZ_OKA		(HN_MAX + 31)
#define	FZ_OKI		(HN_MAX + 32)
#define	FZ_OKU		(HN_MAX + 33)
#define	FZ_OKE		(HN_MAX + 34)
#define	FZ_OKO		(HN_MAX + 35)
#define	FZ_OXTSU	(HN_MAX + 36)
#define	FZ_ORA		(HN_MAX + 37)
#define	FZ_ORI		(HN_MAX + 38)
#define	FZ_ORU		(HN_MAX + 39)
#define	FZ_ORE		(HN_MAX + 40)
#define	FZ_ORO		(HN_MAX + 41)
#define	FZ_KA		(HN_MAX + 42)
#define	FZ_KA2		(HN_MAX + 43)
#define	FZ_KA3		(HN_MAX + 44)
#define	FZ_KAXTSU	(HN_MAX + 45)
#define	FZ_KAXTSU2	(HN_MAX + 46)
#define	FZ_KANE		(HN_MAX + 47)
#define	FZ_KARA		(HN_MAX + 48)
#define	FZ_KARA2	(HN_MAX + 49)
#define	FZ_KARA3	(HN_MAX + 50)
#define	FZ_KARI		(HN_MAX + 51)
#define	FZ_KARU		(HN_MAX + 52)
#define	FZ_KARE		(HN_MAX + 53)
#define	FZ_KARO		(HN_MAX + 54)
#define	FZ_GA		(HN_MAX + 55)
#define	FZ_GA2		(HN_MAX + 56)
#define	FZ_GA3		(HN_MAX + 57)
#define	FZ_GA4		(HN_MAX + 58)
#define	FZ_GAXTSU	(HN_MAX + 59)
#define	FZ_GARA		(HN_MAX + 60)
#define	FZ_GARA2	(HN_MAX + 61)
#define	FZ_GARI		(HN_MAX + 62)
#define	FZ_GARI2	(HN_MAX + 63)
#define	FZ_GARE		(HN_MAX + 64)
#define	FZ_GARO		(HN_MAX + 65)
#define	FZ_GARU		(HN_MAX + 66)
#define	FZ_DE		(HN_MAX + 67)
#define	FZ_KI		(HN_MAX + 68)
#define	FZ_KI2		(HN_MAX + 69)
#define	FZ_KI3		(HN_MAX + 70)
#define	FZ_KI4		(HN_MAX + 71)
#define	FZ_KI5		(HN_MAX + 72)
#define	FZ_KI6		(HN_MAX + 73)
#define	FZ_KI7		(HN_MAX + 74)
#define	FZ_KIRI		(HN_MAX + 75)
#define	FZ_GI		(HN_MAX + 76)
#define	FZ_GI2		(HN_MAX + 77)
#define	FZ_KU		(HN_MAX + 78)
#define	FZ_KU2		(HN_MAX + 79)
#define	FZ_KU3		(HN_MAX + 80)
#define	FZ_KU4		(HN_MAX + 81)
#define	FZ_KU5		(HN_MAX + 82)
#define	FZ_KUSENI	(HN_MAX + 83)
#define	FZ_KURAI	(HN_MAX + 84)
#define	FZ_KURU		(HN_MAX + 85)
#define	FZ_KURE		(HN_MAX + 86)
#define	FZ_KURE2	(HN_MAX + 87)
#define	FZ_KURE3	(HN_MAX + 88)
#define	FZ_GU		(HN_MAX + 89)
#define	FZ_GURAI	(HN_MAX + 90)
#define	FZ_KE		(HN_MAX + 91)
#define	FZ_KEDO		(HN_MAX + 92)
#define	FZ_KERE		(HN_MAX + 93)
#define	FZ_KEREDO	(HN_MAX + 94)
#define	FZ_GE		(HN_MAX + 95)
#define	FZ_GE2		(HN_MAX + 96)
#define	FZ_KO		(HN_MAX + 97)
#define	FZ_KO2		(HN_MAX + 98)
#define	FZ_KOI		(HN_MAX + 99)
#define	FZ_KOSO		(HN_MAX + 100)
#define	FZ_GO		(HN_MAX + 101)
#define	FZ_GOTO		(HN_MAX + 102)
#define	FZ_SA		(HN_MAX + 103)
#define	FZ_SA2		(HN_MAX + 104)
#define	FZ_SA3		(HN_MAX + 105)
#define	FZ_SAE		(HN_MAX + 106)
#define	FZ_SASE		(HN_MAX + 107)
#define	FZ_SARE		(HN_MAX + 108)
#define	FZ_ZARU		(HN_MAX + 109)
#define	FZ_ZARE		(HN_MAX + 110)
#define	FZ_SHI		(HN_MAX + 111)
#define	FZ_SHI2		(HN_MAX + 112)
#define	FZ_SHI3		(HN_MAX + 113)
#define	FZ_SHI4		(HN_MAX + 114)
#define	FZ_SHI5		(HN_MAX + 115)
#define	FZ_SHI6		(HN_MAX + 116)
#define	FZ_SHI7		(HN_MAX + 117)
#define	FZ_SHI8		(HN_MAX + 118)
#define	FZ_SHI9		(HN_MAX + 119)
#define	FZ_SHIKA	(HN_MAX + 120)
#define	FZ_SHIMAI	(HN_MAX + 121)
#define	FZ_SHIMAE	(HN_MAX + 122)
#define	FZ_SHIMAO	(HN_MAX + 123)
#define	FZ_SHIMAXTSU	(HN_MAX + 124)
#define	FZ_SHIMAWA	(HN_MAX + 125)
#define	FZ_SHIMAU	(HN_MAX + 126)
#define	FZ_SHIME	(HN_MAX + 127)
#define	FZ_SHO		(HN_MAX + 128)
#define	FZ_SHIRO	(HN_MAX + 129)
#define	FZ_JI		(HN_MAX + 130)
#define	FZ_JI2		(HN_MAX + 131)
#define	FZ_JIYO		(HN_MAX + 132)
#define	FZ_JIRU		(HN_MAX + 133)
#define	FZ_JIRE		(HN_MAX + 134)
#define	FZ_JIRO		(HN_MAX + 135)
#define	FZ_SU		(HN_MAX + 136)
#define	FZ_SU2		(HN_MAX + 137)
#define	FZ_SU3		(HN_MAX + 138)
#define	FZ_SUBE		(HN_MAX + 139)
#define	FZ_SURA		(HN_MAX + 140)
#define	FZ_SURU		(HN_MAX + 141)
#define	FZ_SURE		(HN_MAX + 142)
#define	FZ_SURE2	(HN_MAX + 143)
#define	FZ_ZU		(HN_MAX + 144)
#define	FZ_ZUTSU	(HN_MAX + 145)
#define	FZ_ZUBE		(HN_MAX + 146)
#define	FZ_ZURU		(HN_MAX + 147)
#define	FZ_ZURE		(HN_MAX + 148)
#define	FZ_ZUN		(HN_MAX + 149)
#define	FZ_SE		(HN_MAX + 150)
#define	FZ_SE2		(HN_MAX + 151)
#define	FZ_SE3		(HN_MAX + 152)
#define	FZ_SE4		(HN_MAX + 153)
#define	FZ_SEYO		(HN_MAX + 154)
#define	FZ_ZE		(HN_MAX + 155)
#define	FZ_ZEYO		(HN_MAX + 156)
#define	FZ_SO		(HN_MAX + 157)
#define	FZ_SOU		(HN_MAX + 158)
#define	FZ_SOU2		(HN_MAX + 159)
#define	FZ_TA		(HN_MAX + 160)
#define	FZ_TA2		(HN_MAX + 161)
#define	FZ_TA3		(HN_MAX + 162)
#define	FZ_TA4		(HN_MAX + 163)
#define	FZ_TARA		(HN_MAX + 164)
#define	FZ_TARI		(HN_MAX + 165)
#define	FZ_TARI2	(HN_MAX + 166)
#define	FZ_TARU		(HN_MAX + 167)
#define	FZ_TARE		(HN_MAX + 168)
#define	FZ_DA		(HN_MAX + 169)
#define	FZ_DA2		(HN_MAX + 170)
#define	FZ_DA3		(HN_MAX + 171)
#define	FZ_DAKE		(HN_MAX + 172)
#define	FZ_DAXTSU	(HN_MAX + 173)
#define	FZ_DANO		(HN_MAX + 174)
#define	FZ_DARI		(HN_MAX + 175)
#define	FZ_DARO		(HN_MAX + 176)
#define	FZ_CHI		(HN_MAX + 177)
#define	FZ_CHI2		(HN_MAX + 178)
#define	FZ_XTSU		(HN_MAX + 179)
#define	FZ_XTSU2	(HN_MAX + 180)
#define	FZ_TSU		(HN_MAX + 181)
#define	FZ_TSUTSU	(HN_MAX + 182)
#define	FZ_TE		(HN_MAX + 183)
#define	FZ_TE2		(HN_MAX + 184)
#define	FZ_DE2		(HN_MAX + 185)
#define	FZ_DE3		(HN_MAX + 186)
#define	FZ_DE4		(HN_MAX + 187)
#define	FZ_DE5		(HN_MAX + 188)
#define	FZ_DEKI		(HN_MAX + 189)
#define	FZ_DESHI	(HN_MAX + 190)
#define	FZ_DESHO	(HN_MAX + 191)
#define	FZ_DESU		(HN_MAX + 192)
#define	FZ_DEMO		(HN_MAX + 193)
#define	FZ_TO		(HN_MAX + 194)
#define	FZ_TO2		(HN_MAX + 195)
#define	FZ_TO3		(HN_MAX + 196)
#define	FZ_TO4		(HN_MAX + 197)
#define	FZ_DO		(HN_MAX + 198)
#define	FZ_NI2		(HN_MAX + 199)
#define	FZ_NI3		(HN_MAX + 200)
#define	FZ_NA		(HN_MAX + 201)
#define	FZ_NA2		(HN_MAX + 202)
#define	FZ_NA3		(HN_MAX + 203)
#define	FZ_NA4		(HN_MAX + 204)
#define	FZ_NA5		(HN_MAX + 205)
#define	FZ_NA6		(HN_MAX + 206)
#define	FZ_NAGARA	(HN_MAX + 207)
#define	FZ_NAZO		(HN_MAX + 208)
#define	FZ_NAXTSU	(HN_MAX + 209)
#define	FZ_NADO		(HN_MAX + 210)
#define	FZ_NARA		(HN_MAX + 211)
#define	FZ_NARA2	(HN_MAX + 212)
#define	FZ_NARI		(HN_MAX + 213)
#define	FZ_NARI2	(HN_MAX + 214)
#define	FZ_NARI3	(HN_MAX + 215)
#define	FZ_NARU		(HN_MAX + 216)
#define	FZ_NARU2	(HN_MAX + 217)
#define	FZ_NARE		(HN_MAX + 218)
#define	FZ_NARE2	(HN_MAX + 219)
#define	FZ_NARO		(HN_MAX + 220)
#define	FZ_NI		(HN_MAX + 221)
#define	FZ_NI4		(HN_MAX + 222)
#define	FZ_NI5		(HN_MAX + 223)
#define	FZ_NI6		(HN_MAX + 224)
#define	FZ_NU		(HN_MAX + 225)
#define	FZ_NU2		(HN_MAX + 226)
#define	FZ_NE		(HN_MAX + 227)
#define	FZ_NE2		(HN_MAX + 228)
#define	FZ_NO		(HN_MAX + 229)
#define	FZ_NO2		(HN_MAX + 230)
#define	FZ_NO3		(HN_MAX + 231)
#define	FZ_NOMI		(HN_MAX + 232)
#define	FZ_HA		(HN_MAX + 233)
#define	FZ_BA		(HN_MAX + 234)
#define	FZ_BA2		(HN_MAX + 235)
#define	FZ_BA3		(HN_MAX + 236)
#define	FZ_BAKARI	(HN_MAX + 237)
#define	FZ_BI		(HN_MAX + 238)
#define	FZ_BI2		(HN_MAX + 239)
#define	FZ_BU		(HN_MAX + 240)
#define	FZ_HE		(HN_MAX + 241)
#define	FZ_BE		(HN_MAX + 242)
#define	FZ_BE2		(HN_MAX + 243)
#define	FZ_HODO		(HN_MAX + 244)
#define	FZ_BO		(HN_MAX + 245)
#define	FZ_MA		(HN_MAX + 246)
#define	FZ_MA2		(HN_MAX + 247)
#define	FZ_MA3		(HN_MAX + 248)
#define	FZ_MAI		(HN_MAX + 249)
#define	FZ_MADE		(HN_MAX + 250)
#define	FZ_MI		(HN_MAX + 251)
#define	FZ_MI2		(HN_MAX + 252)
#define	FZ_MI3		(HN_MAX + 253)
#define	FZ_MI4		(HN_MAX + 254)
#define	FZ_MITAI	(HN_MAX + 255)
#define	FZ_MU		(HN_MAX + 256)
#define	FZ_ME		(HN_MAX + 257)
#define	FZ_ME2		(HN_MAX + 258)
#define	FZ_MO		(HN_MAX + 259)
#define	FZ_MO2		(HN_MAX + 260)
#define	FZ_YA		(HN_MAX + 261)
#define	FZ_YA2		(HN_MAX + 262)
#define	FZ_YASHINA	(HN_MAX + 263)
#define	FZ_YAXTSU	(HN_MAX + 264)
#define	FZ_YARA		(HN_MAX + 265)
#define	FZ_YARA2	(HN_MAX + 266)
#define	FZ_YARI		(HN_MAX + 267)
#define	FZ_YARU		(HN_MAX + 268)
#define	FZ_YARE		(HN_MAX + 269)
#define	FZ_YARO		(HN_MAX + 270)
#define	FZ_XYUU		(HN_MAX + 271)
#define	FZ_YUE		(HN_MAX + 272)
#define	FZ_YO		(HN_MAX + 273)
#define	FZ_YOU		(HN_MAX + 274)
#define	FZ_YOU2		(HN_MAX + 275)
#define	FZ_YOXTSU	(HN_MAX + 276)
#define	FZ_YORA		(HN_MAX + 277)
#define	FZ_YORI		(HN_MAX + 278)
#define	FZ_YORI2	(HN_MAX + 279)
#define	FZ_YORU		(HN_MAX + 280)
#define	FZ_YORE		(HN_MAX + 281)
#define	FZ_YORO		(HN_MAX + 282)
#define	FZ_RA		(HN_MAX + 283)
#define	FZ_RA2		(HN_MAX + 284)
#define	FZ_RA3		(HN_MAX + 285)
#define	FZ_RA4		(HN_MAX + 286)
#define	FZ_RASHI	(HN_MAX + 287)
#define	FZ_RARE		(HN_MAX + 288)
#define	FZ_RI		(HN_MAX + 289)
#define	FZ_RI2		(HN_MAX + 290)
#define	FZ_RI3		(HN_MAX + 291)
#define	FZ_RU		(HN_MAX + 292)
#define	FZ_RU2		(HN_MAX + 293)
#define	FZ_RU3		(HN_MAX + 294)
#define	FZ_RE		(HN_MAX + 295)
#define	FZ_RE2		(HN_MAX + 296)
#define	FZ_RE3		(HN_MAX + 297)
#define	FZ_RE4		(HN_MAX + 298)
#define	FZ_RE5		(HN_MAX + 299)
#define	FZ_RO		(HN_MAX + 300)
#define	FZ_RO2		(HN_MAX + 301)
#define	FZ_RO3		(HN_MAX + 302)
#define	FZ_RO4		(HN_MAX + 303)
#define	FZ_WA		(HN_MAX + 304)
#define	FZ_WA2		(HN_MAX + 305)
#define	FZ_WA3		(HN_MAX + 306)
#define	FZ_WO		(HN_MAX + 307)
#define	FZ_N		(HN_MAX + 308)
#define	FZ_N2		(HN_MAX + 309)
#define	FZ_MAX		(HN_MAX + 310)

#define	SH_MIN		(FZ_MAX + 0)
#define	SH_svkanren	(FZ_MAX + 0)
#define	SH_svkantan	(FZ_MAX + 1)
#define	SH_svbunsetsu	(FZ_MAX + 2)
#define	SH_MAX		(FZ_MAX + 3)
