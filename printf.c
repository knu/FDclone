/*
 *	printf.c
 *
 *	Formatted printing Module
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "machine.h"

#ifndef	NOSTDLIBH
#include <stdlib.h>
#endif

#include "printf.h"
#include "kctype.h"

#define	BUFUNIT		16
#define	THDIGIT		3

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

static int NEAR checkchar __P_((int, printbuf_t *));
static int NEAR setint __P_((u_long_t, int, printbuf_t *, int, int));
static int NEAR setstr __P_((char *, printbuf_t *, int, int));
static int NEAR commonprintf __P_((printbuf_t *, CONST char *, va_list));

char printfflagchar[] = {
	'-', '0', '\'',
#ifndef	MINIMUMSHELL
	'+', ' ', '<',
#endif
	'\0',
};
int printfflag[] = {
	VF_MINUS, VF_ZERO, VF_THOUSAND,
#ifndef	MINIMUMSHELL
	VF_PLUS, VF_SPACE, VF_STRICTWIDTH,
#endif
};
char printfsizechar[] = {
	'l', 'q', 'i',
#ifndef	MINIMUMSHELL
	'L', 'C', 'h',
#endif
	'\0',
};
int printfsize[] = {
	sizeof(long), sizeof(off_t), sizeof(p_id_t),
#ifndef	MINIMUMSHELL
	sizeof(l_off_t), sizeof(char), sizeof(short),
#endif
};


int getnum(s, ptrp)
CONST char *s;
int *ptrp;
{
	int i, n;

	n = 0;
	for (i = *ptrp; isdigit2(s[i]); i++) {
		if (n > MAXTYPE(int) / 10
		|| (n == MAXTYPE(int) / 10
		&& s[i] > (char)(MAXTYPE(int) % 10) + '0'))
			return(-1);
		n = n * 10 + (s[i] - '0');
	}
	if (i <= *ptrp) n = -1;
	*ptrp = i;
	return(n);
}

static int NEAR checkchar(n, pbufp)
int n;
printbuf_t *pbufp;
{
	if (pbufp -> flags & (VF_NEW | VF_FILE)) return(0);
	if (pbufp -> ptr + n < pbufp -> size) return(0);
	return(-1);
}

int setchar(n, pbufp)
int n;
printbuf_t *pbufp;
{
	char *tmp;

	if (pbufp -> flags & VF_FILE) {
		if (pbufp -> buf && fputc(n, (FILE *)(pbufp -> buf)) == EOF)
			return(-1);
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

static int NEAR setint(u, base, pbufp, width, prec)
u_long_t u;
int base;
printbuf_t *pbufp;
int width, prec;
{
#ifndef	MINIMUMSHELL
	int cap;
#endif
	char num[MAXCOLSCOMMA(THDIGIT) + 1];
	long_t n;
	int i, c, len, ptr, sign, bit;

	memcpy(&n, &u, sizeof(n));
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

	if (pbufp -> flags & VF_UNSIGNED) {
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
		for (i = len = 0; s[i]; i++, len++) if (isekana(s, i)) i++;
#else
		len = strlen(s);
#endif
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

#if	defined (FD) && !defined (_NOKANJICONV)
	if (pbufp -> flags & VF_KANJI) {
		char *cp, *tmp;
		int n;

		if (!(tmp = (char *)malloc(len * KANAWID + 1))) return(-1);

		n = len;
		for (;;) {
			for (i = j = 0; i < n; i++, j++) {
				c = s[j];
				if (iskanji1(s, j)) {
					if (++i >= n) c = ' ';
					else {
						tmp[j] = c;
						c = s[++j];
					}
				}
# ifdef	CODEEUC
				else if (isekana(s, j)) {
					tmp[j] = c;
					c = s[++j];
				}
# endif
				tmp[j] = c;
			}
			tmp[j] = '\0';

			cp = newkanjiconv(tmp, DEFCODE, outputkcode, L_OUTPUT);
			if ((pbufp -> flags & (VF_NEW | VF_FILE))
			|| pbufp -> ptr + strlen(cp) < pbufp -> size)
				break;
			if (cp != tmp) free(cp);
			if (--n <= 0) {
				cp = NULL;
				break;
			}
		}
		if (cp != tmp) free(tmp);

		if (cp) {
			for (i = 0; cp[i]; i++)
				if (setchar(cp[i], pbufp) < 0) break;
			if (cp[i]) i = -1;
			free(cp);
			if (i < 0) return(-1);
		}
	}
	else
#endif	/* !FD || _NOKANJICONV */
	for (i = j = 0; i < len; i++, j++) {
		c = s[j];
		if (iskanji1(s, j)) {
			if (++i >= len || checkchar(2, pbufp) < 0) c = ' ';
			else {
				if (setchar(c, pbufp) < 0) return(-1);
				c = s[++j];
			}
		}
#ifdef	CODEEUC
		else if (isekana(s, j)) {
			if (checkchar(2, pbufp) < 0) c = ' ';
			else {
				if (setchar(c, pbufp) < 0) return(-1);
				c = s[++j];
			}
		}
#endif

		if (setchar(c, pbufp) < 0) return(-1);
	}

	if (width >= 0 && len < width) {
		total += width - len;
		for (i = len; i < width; i++)
			if (setchar(' ', pbufp) < 0) return(-1);
	}

	return(total);
}

static int NEAR commonprintf(pbufp, fmt, args)
printbuf_t *pbufp;
CONST char *fmt;
va_list args;
{
	u_long_t u, mask;
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
		pbufp -> flags &= (VF_NEW | VF_FILE);
		width = prec = -1;
		for (; fmt[i]; i++) {
			if (!(cp = strchr(printfflagchar, fmt[i]))) break;
			pbufp -> flags |= printfflag[cp - printfflagchar];
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

		len = sizeof(int);
		for (; fmt[i]; i++) {
			if (!(cp = strchr(printfsizechar, fmt[i]))) break;
			len = printfsize[cp - printfsizechar];
		}

		base = 0;
		switch (fmt[i]) {
			case 'd':
				base = 10;
				break;
			case 'a':
			case 'k':
#if	defined (FD) && !defined (_NOKANJICONV)
				pbufp -> flags |= VF_KANJI;
#endif
/*FALLTHRU*/
			case 's':
				cp = s = va_arg(args, char *);
#ifdef	FD
				if (fmt[i] == 'a') cp = restorearg(s);
#endif
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
				if ((len = setstr("0x", pbufp, -1, -1)) < 0)
					return(-1);
				total += len;
				len = sizeof(VOID_P);
/*FALLTHRU*/
			case 'x':
				pbufp -> flags |= VF_UNSIGNED;
				base = 16;
				break;
			case 'X':
				pbufp -> flags |= VF_UNSIGNED;
				base = 16 + 256;
				break;
#endif	/* !MINIMUMSHELL */
			case 'o':
				pbufp -> flags |= VF_UNSIGNED;
				base = 8;
				break;
			default:
				len = setchar(fmt[i], pbufp);
				break;
		}

		if (base) {
			if (len == sizeof(u_long_t))
				u = va_arg(args, u_long_t);
#ifdef	HAVELONGLONG
			else if (len == sizeof(u_long))
				u = va_arg(args, u_long);
#endif
			else u = va_arg(args, u_int);

			if (!(pbufp -> flags & VF_UNSIGNED)) {
				mask = (MAXUTYPE(u_long_t)
					>> ((sizeof(long_t) - len)
					* BITSPERBYTE + 1));
				if (u & ~mask) u |= ~mask;
			}
			len = setint(u, base, pbufp, width, prec);
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
