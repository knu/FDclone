/*
 *	kanji.c
 *
 *	Kanji Convert Function
 */

#include "fd.h"
#include "term.h"
#include "func.h"
#include "kctype.h"

#include <varargs.h>

extern char *language;

#define	ASCII	0
#define	KANA	1
#define	KANJI	2

#define	NOCNV	0
#define	ENG	1
#define	JIS7	2
#define	SJIS	3
#define	EUC	4

#ifdef	CODEEUC
#define	iskana3(str, i)	(str[i] == 0x8e && str[++i])
#else
#define	iskana3(str, i)	(str[i] >= 0xa1 && str[i] <= 0xdf)
#endif

static int getlang();
static char *jis7();
static char *another();


static int getlang()
{
	char *cp;

	if (!(cp = language)) return(NOCNV);
	else if (strstr(cp, "SJIS") || strstr(cp, "sjis")) return(SJIS);
	else if (strstr(cp, "EUC") || strstr(cp, "euc")) return(EUC);
	else if (strstr(cp, "JIS") || strstr(cp, "jis")) return(JIS7);
	else if (strstr(cp, "ENG") || strstr(cp, "eng") || !strcmp(cp, "C"))
		return(ENG);
	else return(NOCNV);
}

char *mesconv(jpn, eng)
char *jpn, *eng;
{
	return((getlang() == ENG) ? eng : jpn);
}

int onkanji1(s, ptr)
u_char *s;
int ptr;
{
	int i;

	if (ptr < 0) return(0);
	if (!ptr) return(iskanji1((int)s[0]));

	for (i = 0; i < ptr; i++) if (iskanji1((int)s[i])) i++;
	if (i > ptr) return(0);
	return(iskanji1((int)s[ptr]));
}

static char *jis7(buf, str)
char *buf;
u_char *str;
{
	int i, j, mode;

	mode = ASCII;
	for (i = 0, j = 0; str[i] && j < MAXLINESTR - 8; i++, j++) {
		if (iskana3(str, i)) {
			if (!(mode & KANA)) buf[j++] = '\016';
			mode |= KANA;
			buf[j] = str[i] & ~0x80;
			continue;
		}
		if (mode & KANA) buf[j++] = '\017';
		mode &= ~KANA;
		if (iskanji1(str[i])) {
			if (!(mode & KANJI)) {
				strcpy(&buf[j], "\033$B");
				j += 3;
			}
			mode |= KANJI;
#ifdef	CODEEUC
			buf[j++] = str[i++] & ~0x80;
			buf[j] = str[i] & ~0x80;
#else
			buf[j++] = str[i] * 2
				- ((str[i] <= 0x9f) ? 0xe1 : 0x161);
			buf[j] = str[++i];
			if (str[i] < 0x9f)
				buf[j] -= (str[i] > 0x7f) ? 0x20 : 0x1f;
			else {
				buf[j] -= 0x7e;
				buf[j - 1]++;
			}
#endif
		}
		else {
			if (mode & KANJI) {
				strcpy(&buf[j], "\033(B");
				j += 3;
			}
			mode &= ~KANJI;
			buf[j] = str[i];
		}
	}
	if (mode & KANA) buf[j++] = '\017';
	if (mode & KANJI) strcpy(&buf[j], "\033(B");
	else buf[j] = '\0';
	return(buf);
}

static char *another(buf, str)
char *buf;
u_char *str;
{
	int i, j;

	for (i = 0, j = 0; str[i] && j < MAXLINESTR; i++, j++) {
		if (iskana3(str, i)) {
#ifndef	CODEEUC
			buf[j++] = 0x8e;
#endif
			buf[j] = str[i];
		}
		else if (iskanji1(str[i])) {
#ifdef	CODEEUC
			buf[j++] = ((str[i] - 0x81) >> 1)
				+ ((str[i] < 0xde) ? 0x71 : 0xb1);
			buf[j] = str[++i] - 0x80;
			if (str[i - 1] & 1)
				buf[j] += (str[i] < 0xe0) ? 0x1f : 0x20;
			else buf[j] += 0x7e;
#else
			buf[j++] = str[i] * 2
				- ((str[i] <= 0x9f) ? 0xe1 : 0x161) + 0x80;
			buf[j] = str[++i];
			if (str[i] < 0x9f)
				buf[j] += (str[i] > 0x7f) ? 0x60 : 0x61;
			else {
				buf[j] += 2;
				buf[j - 1]++;
			}
#endif
		}
		else buf[j] = str[i];
	}
	buf[j] = '\0';
	return(buf);
}

int kanjiputs(str)
char *str;
{
	char *cp, buf[MAXLINESTR + 1];

	cp = str;
	switch (getlang()) {
		case JIS7:
			cp = jis7(buf, str);
			break;
#ifdef	CODEEUC
		case SJIS:
#else
		case EUC:
#endif
			cp = another(buf, str);
			break;
		default:
			break;
	}
	cputs(cp);
	return(strlen(cp));
}

int kanjiprintf(fmt, va_alist)
char *fmt;
va_dcl
{
	va_list args;
	char buf[MAXLINESTR + 1];

	va_start(args);
	vsprintf(buf, fmt, args);
	va_end(args);

	return(kanjiputs(buf));
}

int kanjiputs2(s, len, ptr)
char *s;
int len, ptr;
{
	char *dupl;
	int i;

	i = ptr;
	if (ptr < 0) ptr = 0;
	if (len >= strlen(s + ptr)) {
		kanjiputs(s + ptr);
		if (i >= 0) for (i = strlen(s + ptr); i < len; i++) putch(' ');
	}
	else {
		dupl = strdup2(s + ptr);
		dupl[len] = '\0';
		if (onkanji1((u_char *)s, len - 1)) dupl[len - 1] = ' ';
		kanjiputs(dupl);
		free(dupl);
	}
	return(len);
}
