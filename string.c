/*
 *	string.c
 *
 *	alternative string functions
 */

#include "headers.h"
#include "kctype.h"
#include "string.h"


char *strchr2(s, c)
CONST char *s;
int c;
{
	for (; *s != c; s++) {
		if (!*s) return(NULL);
		if (iskanji1(s, 0)) s++;
#ifdef	CODEEUC
		else if (isekana(s, 0)) s++;
#endif
	}

	return((char *)s);
}

char *strrchr2(s, c)
CONST char *s;
int c;
{
	char *cp;

	cp = NULL;
	for (; *s; s++) {
		if (*s == c) cp = (char *)s;
		if (iskanji1(s, 0)) s++;
#ifdef	CODEEUC
		else if (isekana(s, 0)) s++;
#endif
	}
	if (!c) cp = (char *)s;

	return(cp);
}

#ifndef	MINIMUMSHELL
char *memchr2(s, c, n)
CONST char *s;
int c, n;
{
	int i;

	for (i = 0; i < n && s[i]; i++) {
		if (s[i] == c) return((char *)&(s[i]));
		if (iskanji1(s, i)) i++;
# ifdef	CODEEUC
		else if (isekana(s, i)) i++;
# endif
	}

	return(NULL);
}
#endif	/* !MINIMUMSHELL */

char *strcpy2(s1, s2)
char *s1;
CONST char *s2;
{
	int i;

	for (i = 0; s2[i]; i++) s1[i] = s2[i];
	s1[i] = '\0';

	return(&(s1[i]));
}

char *strncpy2(s1, s2, n)
char *s1;
CONST char *s2;
int n;
{
	int i;

	for (i = 0; i < n && s2[i]; i++) s1[i] = s2[i];
	s1[i] = '\0';

	return(&(s1[i]));
}

int strcasecmp2(s1, s2)
CONST char *s1, *s2;
{
	int c1, c2;

	for (;;) {
		c1 = toupper2(*s1);
		c2 = toupper2(*s2);
		if (c1 != c2) return(c1 - c2);
#ifndef	CODEEUC
		if (issjis1(*s1)) {
			s1++;
			s2++;
			if (*s1 != *s2) return((u_char)*s1 - (u_char)*s2);
		}
#endif
		if (!*s1) break;
		s1++;
		s2++;
	}

	return(0);
}

int strncasecmp2(s1, s2, n)
CONST char *s1, *s2;
int n;
{
	int c1, c2;

	while (n-- > 0) {
		c1 = toupper2(*s1);
		c2 = toupper2(*s2);
		if (c1 != c2) return(c1 - c2);
#ifndef	CODEEUC
		if (issjis1(*s1)) {
			if (n-- <= 0) break;
			s1++;
			s2++;
			if (*s1 != *s2) return((u_char)*s1 - (u_char)*s2);
		}
#endif
		if (!*s1) break;
		s1++;
		s2++;
	}

	return(0);
}

#ifdef	CODEEUC
int strlen2(s)
CONST char *s;
{
	int i, len;

	for (i = len = 0; s[i]; i++, len++) if (isekana(s, i)) i++;

	return(len);
}
#endif	/* CODEEUC */
