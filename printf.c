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
#include "kctype.h"

#define	BUFUNIT		16
#define	THDIGIT		3

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
extern char *newkanjiconv __P_((char *, int, int, int));
# endif
extern char *restorearg __P_((char *));
#endif

static int NEAR getnum __P_((CONST char *, int *));
static int NEAR setint __P_((long_t, int, printbuf_t *, int, int));
static int NEAR setstr __P_((char *, printbuf_t *, int, int));
static int NEAR commonprintf __P_((printbuf_t *, CONST char *, va_list));


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

int setchar(n, pbufp)
int n;
printbuf_t *pbufp;
{
	char *tmp;

	if (pbufp -> buf && (pbufp -> flags & VF_FILE)) {
		if (fputc(n, (FILE *)(pbufp -> buf)) == EOF) return(-1);
		(pbufp -> ptr)++;
		return(1);
	}

	if (pbufp -> ptr + 1 >= pbufp -> size) {
		if (!(pbufp -> flags & VF_NEW)) {
			if (pbufp -> ptr < pbufp -> size)
				pbufp -> buf[pbufp -> ptr] = '\0';
			return(1);
		}
		pbufp -> size += BUFUNIT;
		if ((tmp = (char *)realloc(pbufp -> buf, pbufp -> size)))
			pbufp -> buf = tmp;
		else {
			free(pbufp -> buf);
			pbufp -> buf = NULL;
			return(-1);
		}
	}
	pbufp -> buf[(pbufp -> ptr)++] = n;
	return(1);
}

static int NEAR setint(n, base, pbufp, width, prec)
long_t n;
int base;
printbuf_t *pbufp;
int width, prec;
{
#ifndef	MINIMUMSHELL
	int cap;
#endif
	char num[MAXCOLSCOMMA(THDIGIT) + 1];
	u_long_t u;
	int i, c, len, ptr, sign, bit;

	u = (u_long_t)n;
	bit = 0;

#ifdef	MINIMUMSHELL
	if (base != 10) {
		sign = 0;
		for (i = 1; i < base; i <<= 1) bit++;
		base--;
	}
	else if (n >= 0) sign = 0;
#else	/* !MINIMUMSHELL */
	cap = 0;
	if (base >= 256) {
		cap++;
		base -= 256;
	}

	if (base != 10 || (pbufp -> flags & VF_UNSIGNED)) {
		sign = 0;
		pbufp -> flags &= ~(VF_PLUS | VF_SPACE);
		if (base != 10) {
			for (i = 1; i < base; i <<= 1) bit++;
			base--;
		}
	}
	else if (n >= 0) sign = (pbufp -> flags & VF_PLUS) ? 1 : 0;
#endif	/* !MINIMUMSHELL */
	else {
		sign = -1;
		n = -n;
	}

#ifdef	MINIMUMSHELL
	if (sign) width--;
#else
	if (sign || (pbufp -> flags & VF_SPACE)) width--;
#endif
	if ((pbufp -> flags & VF_ZERO) && prec < 0) prec = width;

	len = 0;
	if (!n) {
		if (prec) num[len++] = '0';
	}
	else while (len < sizeof(num) / sizeof(char)) {
#ifdef	MINIMUMSHELL
		if (!bit) i = (n % base) + '0';
		else if ((i = (u & base)) < 10) i += '0';
#else
		if (!bit) i = '0'
			+ (((pbufp -> flags & VF_UNSIGNED) ? u : n) % base);
		else if ((i = (u & base)) < 10) i += '0';
		else if (cap) i += 'A' - 10;
		else i += 'a' - 10;
#endif

		if ((pbufp -> flags & VF_THOUSAND)
		&& (len % (THDIGIT + 1)) == THDIGIT)
			num[len++] = ',';
		num[len++] = i;
		if (bit) {
			u >>= bit;
			if (!u) break;
		}
#ifndef	MINIMUMSHELL
		else if (pbufp -> flags & VF_UNSIGNED) {
			u /= base;
			if (!u) break;
		}
#endif
		else {
			n /= base;
			if (!n) break;
		}
	}

	ptr = pbufp -> ptr;
	if (!(pbufp -> flags & VF_MINUS) && width >= 0 && len < width) {
		while (len < width) {
			if (prec >= 0 && prec > width - 1) break;
			width--;
			if (setchar(' ', pbufp) < 0) return(-1);
		}
	}

	c = '\0';
	if (sign) c = (sign > 0) ? '+' : '-';
#ifndef	MINIMUMSHELL
	else if (pbufp -> flags & VF_SPACE) c = ' ';
#endif
	if (c && setchar(c, pbufp) < 0) return(-1);

	if (prec >= 0 && len < prec) {
		for (i = 0; i < prec - len; i++) {
			if ((pbufp -> flags & VF_STRICTWIDTH) && width == 1)
				break;
			width--;
			c = '0';
			if ((pbufp -> flags & VF_THOUSAND)
			&& !((prec - i) % (THDIGIT + 1)))
				c = (i) ? ',' : ' ';
			if (setchar(c, pbufp) < 0) return(-1);
		}
	}
	if ((pbufp -> flags & VF_STRICTWIDTH) && width >= 0 && width < len)
	for (i = 0; i < width; i++) {
		c = '9';
		if ((pbufp -> flags & VF_THOUSAND)
		&& !((width - i) % (THDIGIT + 1)))
			c = (i) ? ',' : ' ';
		if (setchar(c, pbufp) < 0) return(-1);
	}
	else for (i = 0; i < len; i++) {
		if (setchar(num[len - i - 1], pbufp) < 0) return(-1);
	}

	if (width >= 0) for (; i < width; i++) {
		if (setchar(' ', pbufp) < 0) return(-1);
	}

	return(pbufp -> ptr - ptr);
}

static int NEAR setstr(s, pbufp, width, prec)
char *s;
printbuf_t *pbufp;
int width, prec;
{
	int i, j, c, len, total;

	if (s) {
#ifdef	CODEEUC
		if (pbufp -> flags & VF_KANJI) {
			for (i = len = 0; s[i]; i++, len++)
				if (isekana(s, i)) i++;
		}
		else
#endif
		len = strlen(s);
	}
	else {
		s = "(null)";
		len = sizeof("(null)") - 1;
#ifdef	LINUX
		/* spec. of glibc */
		if (prec >= 0 && len > prec) len = 0;
#endif
	}
	if (prec >= 0 && len > prec) len = prec;

	total = 0;
	if (!(pbufp -> flags & VF_MINUS) && width >= 0 && len < width) {
		total += width - len;
		while (len < width--) {
			if (setchar(' ', pbufp) < 0) return(-1);
		}
	}
	total += len;
	if (!(pbufp -> flags & VF_KANJI)) {
		for (i = 0; i < len; i++)
			if (setchar(s[i], pbufp) < 0) return(-1);
	}
	else for (i = j = 0; i < len; i++, j++) {
		c = s[j];
#ifdef	CODEEUC
		if (isekana(s, j)) {
			if (setchar(c, pbufp) < 0) return(-1);
			c = s[++j];
		}
		else
#endif
		if (iskanji1(s, j)) {
			if (++i >= len) c = ' ';
			else {
				if (setchar(c, pbufp) < 0) return(-1);
				c = s[++j];
			}
		}

		if (setchar(c, pbufp) < 0) return(-1);
	}

	if (width >= 0 && i < width) {
		total += width - i;
		for (; i < width; i++) if (setchar(' ', pbufp) < 0) return(-1);
	}

	return(total);
}

static int NEAR commonprintf(pbufp, fmt, args)
printbuf_t *pbufp;
CONST char *fmt;
va_list args;
{
#if	defined (FD) && !defined (_NOKANJICONV)
	char *tmp;
#endif
	long_t n;
	char *s, *cp;
	int i, len, total, base, width, prec;

	if (pbufp -> flags & VF_NEW) {
		if (!(pbufp -> buf = (char *)malloc(1))) return(-1);
		pbufp -> size = 0;
	}
	pbufp -> ptr = total = 0;

	for (i = 0; fmt[i]; i++) {
		if (fmt[i] != '%') {
			total++;
			if (setchar(fmt[i], pbufp) < 0) return(-1);
			continue;
		}

		i++;
		width = prec = -1;
		for (;;) {
			if (fmt[i] == '-') pbufp -> flags |= VF_MINUS;
			else if (fmt[i] == '0') pbufp -> flags |= VF_ZERO;
			else if (fmt[i] == '\'') pbufp -> flags |= VF_THOUSAND;
#ifndef	MINIMUMSHELL
			else if (fmt[i] == '+') pbufp -> flags |= VF_PLUS;
			else if (fmt[i] == ' ') pbufp -> flags |= VF_SPACE;
			else if (fmt[i] == '<')
				pbufp -> flags |= VF_STRICTWIDTH;
#endif
			else break;

			i++;
		}

		if (fmt[i] != '*') width = getnum(fmt, &i);
		else {
			i++;
			width = va_arg(args, int);
		}
		if (fmt[i] == '.') {
			i++;
			if (fmt[i] != '*') prec = getnum(fmt, &i);
			else {
				i++;
				prec = va_arg(args, int);
			}
			if (prec < 0) prec = 0;
		}
		if (fmt[i] == 'l') {
			i++;
			pbufp -> flags |= VF_LONG;
		}
		else if (fmt[i] == 'q') {
			i++;
			pbufp -> flags |= VF_OFFT;
		}
		else if (fmt[i] == 'i') {
			i++;
			pbufp -> flags |= VF_PIDT;
		}
#ifndef	MINIMUMSHELL
		else if (fmt[i] == 'L') {
			i++;
			pbufp -> flags |= VF_LOFFT;
		}
#endif

		base = 0;
		switch (fmt[i]) {
			case 'd':
				base = 10;
				break;
			case 'a':
			case 'k':
				pbufp -> flags |= VF_KANJI;
/*FALLTHRU*/
			case 's':
				cp = s = va_arg(args, char *);
#ifdef	FD
				if (fmt[i] == 'a') cp = restorearg(s);

# ifndef	_NOKANJICONV
				if (cp && (pbufp -> flags & VF_KANJI)) {
					tmp = newkanjiconv(cp, DEFCODE,
						outputkcode, L_OUTPUT);
					if (cp != s && cp != tmp) free(cp);
					cp = tmp;
				}
# endif
#endif	/* FD */
				len = setstr(cp, pbufp, width, prec);
				if (cp != s) free(cp);
				break;
			case 'c':
				len = setchar(va_arg(args, int) & 0xff, pbufp);
				break;
#ifndef	MINIMUMSHELL
			case 'u':
				pbufp -> flags |= VF_UNSIGNED;
				base = 10;
				break;
			case 'p':
				pbufp -> flags |= VF_LONG;
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
				len = setchar(fmt[i], pbufp);
				break;
		}

		if (base) {
			if (pbufp -> flags & VF_LONG) {
				n = va_arg(args, long);
				len = (sizeof(long_t) - sizeof(long))
					* BITSPERBYTE;
			}
			else if (pbufp -> flags & VF_OFFT) {
				n = va_arg(args, off_t);
				len = (sizeof(long_t) - sizeof(off_t))
					* BITSPERBYTE;
			}
			else if (pbufp -> flags & VF_PIDT) {
				n = va_arg(args, p_id_t);
				len = (sizeof(long_t) - sizeof(p_id_t))
					* BITSPERBYTE;
			}
#ifndef	MINIMUMSHELL
			else if (pbufp -> flags & VF_LOFFT) {
				n = va_arg(args, l_off_t);
				len = (sizeof(long_t) - sizeof(l_off_t))
					* BITSPERBYTE;
			}
#endif
			else {
				n = va_arg(args, int);
				len = (sizeof(long_t) - sizeof(int))
					* BITSPERBYTE;
			}
#ifdef	MINIMUMSHELL
			if (len > 0 && base != 10)
#else
			if (len > 0
			&& (base != 10 || (pbufp -> flags & VF_UNSIGNED)))
#endif
				n &= (MAXUTYPE(u_long_t) >> len);
			len = setint(n, base, pbufp, width, prec);
		}

		if (len < 0) return(-1);
		total += len;
	}
	va_end(args);
	if (pbufp -> buf && !(pbufp -> flags & VF_FILE))
		pbufp -> buf[pbufp -> ptr] = '\0';
	return(total);
}

int vasprintf2(sp, fmt, args)
char **sp;
CONST char *fmt;
va_list args;
{
	printbuf_t pbuf;
	int n;

	if (!sp) return(-1);
	pbuf.flags = VF_NEW;
	n = commonprintf(&pbuf, fmt, args);
	*sp = pbuf.buf;
	return(n);
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
	printbuf_t pbuf;
	int n;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	if (!sp) return(-1);
	pbuf.flags = VF_NEW;
	n = commonprintf(&pbuf, fmt, args);
	*sp = pbuf.buf;
	va_end(args);
	return(n);
}

#ifdef	USESTDARGH
/*VARARGS3*/
int snprintf2(char *s, int size, CONST char *fmt, ...)
#else
/*VARARGS3*/
int snprintf2(s, size, fmt, va_alist)
char *s;
int size;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	printbuf_t pbuf;
	int n;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	if (size < 0) return(-1);
	pbuf.buf = s;
	pbuf.size = size;
	pbuf.flags = 0;
	n = commonprintf(&pbuf, fmt, args);
	va_end(args);
	return(n);
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
	printbuf_t pbuf;
	int n;

#ifdef	USESTDARGH
	va_start(args, fmt);
#else
	va_start(args);
#endif

	pbuf.buf = (char *)fp;
	pbuf.flags = VF_FILE;
	n = commonprintf(&pbuf, fmt, args);
	va_end(args);
	return(n);
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
