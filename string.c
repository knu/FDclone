/*
 *	string.c
 *
 *	alternative string functions
 */

#include "headers.h"
#include "kctype.h"
#include "string.h"

#define	VT_OTHER		1
#define	VT_DIGIT		2
#define	VT_ALPHA		3


char *Xstrchr(s, c)
CONST char *s;
int c;
{
	for (; *s != c; s++) {
		if (!*s) return(NULL);
		else if (iswchar(s, 0)) s++;
	}

	return((char *)s);
}

char *Xstrrchr(s, c)
CONST char *s;
int c;
{
	char *cp;

	cp = NULL;
	for (;; s++) {
		if (*s == c) cp = (char *)s;
		else if (iswchar(s, 0)) s++;
		if (!*s) break;
	}

	return(cp);
}

char *Xmemchr(s, c, n)
CONST char *s;
int c, n;
{
	for (; n-- > 0; s++) {
		if (*s == c) return((char *)s);
		else if (iswchar(s, 0)) {
			if (n-- <= 0) break;
			s++;
		}
	}

	return(NULL);
}

char *Xstrcpy(s1, s2)
char *s1;
CONST char *s2;
{
	int i;

	for (i = 0; s2[i]; i++) s1[i] = s2[i];
	s1[i] = '\0';

	return(&(s1[i]));
}

char *Xstrncpy(s1, s2, n)
char *s1;
CONST char *s2;
int n;
{
	int i;

	for (i = 0; i < n && s2[i]; i++) s1[i] = s2[i];
	s1[i] = '\0';

	return(&(s1[i]));
}

int Xstrcasecmp(s1, s2)
CONST char *s1, *s2;
{
	int c1, c2;

	for (;;) {
		c1 = Xtoupper(*s1);
		c2 = Xtoupper(*s2);
		if (c1 != c2) return(c1 - c2);
		if (iswchar(s1, 0)) {
			s1++;
			s2++;
			if (*s1 != *s2) return((u_char)*s1 - (u_char)*s2);
		}
		if (!*s1) break;
		s1++;
		s2++;
	}

	return(0);
}

int Xstrncasecmp(s1, s2, n)
CONST char *s1, *s2;
int n;
{
	int c1, c2;

	while (n-- > 0) {
		c1 = Xtoupper(*s1);
		c2 = Xtoupper(*s2);
		if (c1 != c2) return(c1 - c2);
		if (iswchar(s1, 0)) {
			if (n-- <= 0) break;
			s1++;
			s2++;
			if (*s1 != *s2) return((u_char)*s1 - (u_char)*s2);
		}
		if (!*s1) break;
		s1++;
		s2++;
	}

	return(0);
}

#ifndef	_NOVERSCMP
static int NEAR verstype(c)
int c;
{
	if (Xisdigit(c)) return(VT_DIGIT);
	if (Xisalpha(c)) return(VT_ALPHA);

	return(VT_OTHER);
}

int Xstrverscmp(s1, s2, nocase)
CONST char *s1, *s2;
int nocase;
{
	int n, c1, c2, t1, t2, blk, last;

	blk = last = 0;
	for (;;) {
		c1 = *s1;
		c2 = *s2;
		if (nocase) {
			c1 = Xtoupper(c1);
			c2 = Xtoupper(c2);
		}
		n = c1 - c2;
		if (!c1 || !c2) return(n);

		t1 = verstype(c1);
		switch (t1) {
			case VT_OTHER:
				blk++;
				break;
			case VT_ALPHA:
				blk = 0;
				break;
			default:
				break;
		}
		if (n) break;

		if (iswchar(s1, 0)) {
			s1++;
			s2++;
			if (*s1 != *s2) return((u_char)*s1 - (u_char)*s2);
		}

		last = t1;
		s1++;
		s2++;
	}

	t2 = verstype(c2);
	if (blk > 0) /*EMPTY*/;
	else if (last == VT_DIGIT || (t1 == VT_DIGIT && t2 == VT_DIGIT)) {
		for (;;) {
			if (t1 != t2) return((t1 == VT_DIGIT) ? 1 : -1);
			if (t1 != VT_DIGIT) return(n);

			t1 = verstype(*(++s1));
			t2 = verstype(*(++s2));
		}
	}

	return((t1 == t2) ? n : t1 - t2);
}
#endif	/* !_NOVERSCMP */

#ifdef	CODEEUC
int strlen2(s)
CONST char *s;
{
	int i, len;

	for (i = len = 0; s[i]; i++, len++) if (isekana(s, i)) i++;

	return(len);
}
#endif	/* CODEEUC */

VOID Xstrtolower(s)
char *s;
{
	if (s) for (; *s; s++) {
		if (iswchar(s, 0)) s++;
		else *s = Xtolower(*s);
	}
}

VOID Xstrtoupper(s)
char *s;
{
	if (s) for (; *s; s++) {
		if (iswchar(s, 0)) s++;
		else *s = Xtoupper(*s);
	}
}

VOID Xstrntolower(s, n)
char *s;
int n;
{
	if (s) for (; n-- > 0 && *s; s++) {
		if (iswchar(s, 0)) s++;
		else *s = Xtolower(*s);
	}
}

VOID Xstrntoupper(s, n)
char *s;
int n;
{
	if (s) for (; n-- > 0 && *s; s++) {
		if (iswchar(s, 0)) s++;
		else *s = Xtoupper(*s);
	}
}
