/*
 *	printf.c
 *
 *	Formatted printing Module
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include "machine.h"

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include "printf.h"

#define	MAXLONGWIDTH	20		/* log10(2^64) = 19.266 */
#define	MAXCOLSCOMMA(d)	(MAXLONGWIDTH + (MAXLONGWIDTH / (d)))
#define	BUFUNIT		16
#define	THDIGIT		3
#define	VF_PLUS		00001
#define	VF_MINUS	00002
#define	VF_SPACE	00004
#define	VF_ZERO		00010
#define	VF_LONG		00020
#define	VF_OFFT		00040
#define	VF_PIDT		00100
#define	VF_LOFFT	00200
#define	VF_UNSIGNED	00400
#define	VF_THOUSAND	01000
#define	VF_STRICTWIDTH	02000

#ifdef	__STDC__
typedef long long		long_t;
typedef unsigned long long	u_long_t;
#else
typedef long			long_t;
typedef unsigned long		u_long_t;
#endif

#ifdef	USEPID_T
typedef pid_t	p_id_t;
#else
# if	MSDOS
typedef int	p_id_t;
# else
typedef long	p_id_t;
# endif
#endif

#ifdef	USELLSEEK
typedef long long	l_off_t;
#else
typedef off_t		l_off_t;
#endif

#ifdef	FD
# ifndef	_NOKANJICONV
#include "kctype.h"
extern char *malloc2 __P_((ALLOC_T));
extern int kanjiconv __P_((char *, char *, int, int, int, int));
# endif
extern char *restorearg __P_((char *));
#endif

static int NEAR getnum __P_((CONST char *, int *));
static char *NEAR setint __P_((long_t, int,
		char *, int *, int *, int, int, int, int));
#ifdef	FD
static char *NEAR setstr __P_((char *,
		char *, int *, int *, int, int, int, int, int));
#else
static char *NEAR setstr __P_((char *,
		char *, int *, int *, int, int, int, int));
#endif
static char *NEAR commonprintf __P_((char *, int *, CONST char *, va_list));


static int NEAR getnum(s, ptrp)
CONST char *s;
int *ptrp;
{
	int i, n;

	i = *ptrp;
	for (n = 0; isdigit(s[i]); i++) n = n * 10 + (s[i] - '0');
	if (i <= *ptrp) n = -1;
	*ptrp = i;
	return(n);
}

char *setchar(n, buf, ptrp, sizep, new)
int n;
char *buf;
int *ptrp, *sizep, new;
{
	char *tmp;

	if (buf && new < 0) {
		if (fputc(n, (FILE *)buf) == EOF) return(NULL);
		(*ptrp)++;
		return(buf);
	}

	if (*ptrp + 1 >= *sizep) {
		if (!new) {
			if (*ptrp + 1 == *sizep) buf[(*ptrp)++] = n;
			buf[*ptrp] = '\0';
			return(NULL);
		}
		*sizep += BUFUNIT;
		if ((tmp = (char *)realloc(buf, *sizep))) buf = tmp;
		else {
			free(buf);
			return(NULL);
		}
	}
	buf[(*ptrp)++] = n;
	return(buf);
}

static char *NEAR setint(n, base, buf, ptrp, sizep, flags, width, prec, new)
long_t n;
int base;
char *buf;
int *ptrp, *sizep, flags, width, prec, new;
{
#ifndef	MINIMUMSHELL
	int cap;
#endif
	char num[MAXCOLSCOMMA(THDIGIT) + 1];
	u_long_t u;
	int i, c, len, sign, bit;

#ifndef	MINIMUMSHELL
	cap = 0;
	if (base >= 256) {
		cap++;
		base -= 256;
	}
#endif

	u = (u_long_t)n;
	bit = 0;
	if (base != 10) {
		sign = 0;
#ifndef	MINIMUMSHELL
		flags &= ~(VF_PLUS | VF_SPACE);
#endif
		for (i = 1; i < base; i <<= 1) bit++;
		base--;
	}
#ifdef	MINIMUMSHELL
	else if (n >= 0) sign = 0;
#else
	else if (n >= 0 || (flags & VF_UNSIGNED))
		sign = (flags & VF_PLUS) ? 1 : 0;
#endif
	else {
		sign = -1;
		n = -n;
	}
#ifdef	MINIMUMSHELL
	if (sign) width--;
#else
	if (sign || (flags & VF_SPACE)) width--;
#endif
	if ((flags & VF_ZERO) && prec < 0) prec = width;
	len = 0;
	while (len < sizeof(num) / sizeof(char)) {
#ifdef	MINIMUMSHELL
		if (!bit) i = (n % base) + '0';
		else if ((i = (u & base)) < 10) i += '0';
#else
		if (!bit) i = (((flags & VF_UNSIGNED) ? u : n) % base) + '0';
		else if ((i = (u & base)) < 10) i += '0';
		else if (cap) i += 'A' - 10;
		else i += 'a' - 10;
#endif

		if ((flags & VF_THOUSAND) && (len % (THDIGIT + 1)) == THDIGIT)
			num[len++] = ',';
		num[len++] = i;
		if (bit) {
			u >>= bit;
			if (!u) break;
		}
#ifdef	MINIMUMSHELL
		else if (flags & VF_UNSIGNED) {
			u /= base;
			if (!u) break;
		}
#endif
		else {
			n /= base;
			if (!n) break;
		}
	}

	if (!(flags & VF_MINUS) && width >= 0 && len < width) {
		while (len < width) {
			if (prec >= 0 && prec > width - 1) break;
			width--;
			if (!(buf = setchar(' ', buf, ptrp, sizep, new)))
				return(NULL);
		}
	}

	c = '\0';
	if (sign) c = (sign > 0) ? '+' : '-';
#ifndef	MINIMUMSHELL
	else if (flags & VF_SPACE) c = ' ';
#endif
	if (c && !(buf = setchar(c, buf, ptrp, sizep, new))) return(NULL);

	if (prec >= 0 && len < prec) {
		for (i = 0; i < prec - len; i++) {
			if ((flags & VF_STRICTWIDTH) && width == 1) break;
			width--;
			c = '0';
			if ((flags & VF_THOUSAND)
			&& !((prec - i) % (THDIGIT + 1)))
				c = (i) ? ',' : ' ';
			if (!(buf = setchar(c, buf, ptrp, sizep, new)))
				return(NULL);
		}
	}
	if ((flags & VF_STRICTWIDTH) && width >= 0 && width < len)
	for (i = 0; i < width; i++) {
		c = '9';
		if ((flags & VF_THOUSAND) && !((width - i) % (THDIGIT + 1)))
			c = (i) ? ',' : ' ';
		if (!(buf = setchar(c, buf, ptrp, sizep, new))) return(NULL);
	}
	else for (i = 0; i < len; i++) {
		if (!(buf = setchar(num[len - i - 1], buf, ptrp, sizep, new)))
			return(NULL);
	}

	if (width >= 0) for (; i < width; i++) {
		if (!(buf = setchar(' ', buf, ptrp, sizep, new))) return(NULL);
	}

	return(buf);
}

#ifdef	FD
static char *NEAR setstr(s, buf, ptrp, sizep, flags, width, prec, new, kanji)
char *s, *buf;
int *ptrp, *sizep, flags, width, prec, new, kanji;
#else
static char *NEAR setstr(s, buf, ptrp, sizep, flags, width, prec, new)
char *s, *buf;
int *ptrp, *sizep, flags, width, prec, new;
#endif
{
#ifdef	FD
	int j, c;
#endif
	int i, len;

	if (s) {
#if	defined (FD) && defined (CODEEUC)
		if (kanji) {
			for (i = len = 0; s[i]; i++, len++)
				if (isekana(s, i)) i++;
		}
		else
#endif
		len = strlen(s);
		if (prec >= 0 && len > prec) len = prec;
	}
	else {
		s = "(null)";
		len = sizeof("(null)") - 1;
		if (prec >= 0 && len > prec) len = 0;
	}

	if (!(flags & VF_MINUS) && width >= 0 && len < width) {
		while (len < width--) {
			if (!(buf = setchar(' ', buf, ptrp, sizep, new)))
				return(NULL);
		}
	}
#ifdef	FD
	if (kanji) for (i = j = 0; i < len; i++, j++) {
		c = s[j];
# ifdef	CODEEUC
		if (isekana(s, j)) {
			if (!(buf = setchar(c, buf, ptrp, sizep, new)))
				return(NULL);
			c = s[++j];
		}
		else
# endif
		if (iskanji1(s, j)) {
			if (++i >= len) c = ' ';
			else {
				if (!(buf = setchar(c, buf, ptrp, sizep, new)))
					return(NULL);
				c = s[++j];
			}
		}

		if (!(buf = setchar(c, buf, ptrp, sizep, new))) return(NULL);
	}
	else
#endif	/* FD */
	for (i = 0; i < len; i++) {
		if (!(buf = setchar(s[i], buf, ptrp, sizep, new)))
			return(NULL);
	}

	if (width >= 0) for (; i < width; i++) {
		if (!(buf = setchar(' ', buf, ptrp, sizep, new))) return(NULL);
	}

	return(buf);
}

static char *NEAR commonprintf(buf, sizep, fmt, args)
char *buf;
int *sizep;
CONST char *fmt;
va_list args;
{
#ifdef	FD
	char *s, *cp;
# ifndef	_NOKANJICONV
	char *tmp;
	int len;
# endif
#endif
	long_t n;
	int i, j, base, flags, width, prec, new;

	if (buf) new = (*sizep >= 0) ? 0 : -1;
	else if (!(buf = (char *)malloc(*sizep = BUFUNIT))) return(NULL);
	else new = 1;
	for (i = j = 0; fmt[i]; i++) {
		if (fmt[i] != '%') {
			if (!(buf = setchar(fmt[i], buf, &j, sizep, new)))
				return(NULL);
			continue;
		}

		i++;
		flags = 0;
		width = prec = -1;
		for (;;) {
			if (fmt[i] == '-') flags |= VF_MINUS;
			else if (fmt[i] == '0') flags |= VF_ZERO;
			else if (fmt[i] == '\'') flags |= VF_THOUSAND;
#ifndef	MINIMUMSHELL
			else if (fmt[i] == '+') flags |= VF_PLUS;
			else if (fmt[i] == ' ') flags |= VF_SPACE;
			else if (fmt[i] == '<') flags |= VF_STRICTWIDTH;
#endif
			else break;

			i++;
		}

		if (fmt[i] != '*') width = getnum(fmt, &i);
		else {
			i++;
			width = (int)va_arg(args, int);
		}
		if (fmt[i] == '.') {
			i++;
			if (fmt[i] != '*') prec = getnum(fmt, &i);
			else {
				i++;
				prec = va_arg(args, int);
			}
		}
		if (fmt[i] == 'l') {
			i++;
			flags |= VF_LONG;
		}
		else if (fmt[i] == 'q') {
			i++;
			flags |= VF_OFFT;
		}
		else if (fmt[i] == 'i') {
			i++;
			flags |= VF_PIDT;
		}
#ifndef	MINIMUMSHELL
		else if (fmt[i] == 'L') {
			i++;
			flags |= VF_LOFFT;
		}
#endif

		base = 0;
		switch (fmt[i]) {
			case 'd':
				base = 10;
				break;
#ifdef	FD
			case 'a':
			case 'k':
				cp = s = va_arg(args, char *);
				if (fmt[i] == 'a') cp = restorearg(s);

# ifndef	_NOKANJICONV
				len = strlen(cp) * 3 + 3;
				tmp = malloc2(len + 1);
				len = kanjiconv(tmp, cp, len,
					DEFCODE, outputkcode, L_OUTPUT);
				tmp[len] = '\0';
				if (cp != s) free(cp);
				cp = tmp;
# endif
				buf = setstr(cp, buf, &j, sizep,
					flags, width, prec, new, 1);
				if (cp != s) free(cp);
				break;
			case 's':
				buf = setstr(va_arg(args, char *),
					buf, &j, sizep,
					flags, width, prec, new, 0);
				break;
#else	/* FD */
			case 'a':
			case 'k':
			case 's':
				buf = setstr(va_arg(args, char *),
					buf, &j, sizep,
					flags, width, prec, new);
				break;
#endif	/* FD */
			case 'c':
				buf = setchar(va_arg(args, int),
					buf, &j, sizep, new);
				break;
#ifndef	MINIMUMSHELL
			case 'u':
				flags |= VF_UNSIGNED;
				base = 10;
				break;
			case 'p':
				flags |= VF_LONG;
/*FALLTHRU*/
			case 'x':
				base = 16;
				break;
			case 'X':
				base = 16 + 256;
				break;
#endif	/* !MINIMUMSHELL */
			case 'o':
				base = 8;
				break;
			default:
				buf = setchar(fmt[i], buf, &j, sizep, new);
				break;
		}

		if (base) {
			if (flags & VF_LONG) n = (long_t)va_arg(args, long);
			else if (flags & VF_OFFT)
				n = (long_t)va_arg(args, off_t);
			else if (flags & VF_PIDT)
				n = (long_t)va_arg(args, p_id_t);
#ifndef	MINIMUMSHELL
			else if (flags & VF_LOFFT)
				n = (long_t)va_arg(args, l_off_t);
#endif
			else n = (long_t)va_arg(args, int);
			buf = setint(n, base,
				buf, &j, sizep, flags, width, prec, new);
		}

		if (!buf) return(NULL);
	}
	va_end(args);
	if (new >= 0) buf[j] = '\0';
	*sizep = j;
	return(buf);
}

int vasprintf2(sp, fmt, args)
char **sp;
CONST char *fmt;
va_list args;
{
	char *cp;
	int size;

	if (!(cp = commonprintf(NULL, &size, fmt, args))) return(-1);
	if (sp) *sp = cp;
	else free(cp);
	return(size);
}

#ifdef	USESTDARGH
/*VARARGS2*/
int asprintf2(char **sp, CONST char *fmt, ...)
#else
/*VARARGS2*/
int asprintf2(sp, fmt, va_alist)
char **sp;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	int size;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	*sp = commonprintf(NULL, &size, fmt, args);
	va_end(args);
	if (!*sp) return(-1);
	return(size);
}

#ifdef	USESTDARGH
/*VARARGS3*/
char *snprintf2(char *s, int size, CONST char *fmt, ...)
#else
/*VARARGS3*/
char *snprintf2(s, size, fmt, va_alist)
char *s;
int size;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	if (size < 0) return(NULL);
	commonprintf(s, &size, fmt, args);
	va_end(args);
	return(s);
}

#ifdef	USESTDARGH
/*VARARGS2*/
int fprintf2(FILE *fp, CONST char *fmt, ...)
#else
/*VARARGS2*/
int fprintf2(fp, fmt, va_alist)
FILE *fp;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *cp;
	int size;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	size = -1;
	cp = commonprintf((char *)fp, &size, fmt, args);
	va_end(args);
	if (!cp) return(-1);
	return(size);
}

VOID fputnl(fp)
FILE *fp;
{
	fputc('\n', fp);
	fflush(fp);
}

#ifdef	FD
VOID kanjifputs(s, fp)
char *s;
FILE *fp;
{
	fprintf2(fp, "%k", s);
}
#endif	/* FD */
