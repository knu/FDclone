/*
 *	printf.c
 *
 *	formatted printing module
 */

#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "typesize.h"
#include "string.h"

#ifdef	FD
#include "kconv.h"
#endif

#define	PRINTBUFUNIT		16
#define	THDIGIT			3
#define	NULLSTR			"(null)"

#ifdef	FD
extern char *restorearg __P_((CONST char *));
#endif

#ifndef	MINIMUMSHELL
static int NEAR gencntrl __P_((char *, int, printbuf_t *));
static int NEAR setcntrl __P_((int, int, printbuf_t *));
#endif
static int NEAR checkchar __P_((int, printbuf_t *));
static int NEAR setint __P_((u_long_t, int, printbuf_t *, int, int));
static int NEAR setstr __P_((CONST char *, printbuf_t *, int, int));
static int NEAR commonprintf __P_((printbuf_t *, CONST char *, va_list));

CONST char printfflagchar[] = {
	'-', '0', '\'',
#ifndef	MINIMUMSHELL
	'+', ' ', '<', '^',
#endif
	'\0',
};
CONST int printfflag[] = {
	VF_MINUS, VF_ZERO, VF_THOUSAND,
#ifndef	MINIMUMSHELL
	VF_PLUS, VF_SPACE, VF_STRICTWIDTH, VF_PRINTABLE,
#endif
};
CONST char printfsizechar[] = {
	'l', 'q', 'i',
#ifndef	MINIMUMSHELL
	'L', 'C', 'h',
#endif
	'\0',
};
CONST int printfsize[] = {
	sizeof(long), sizeof(off_t), sizeof(p_id_t),
#ifndef	MINIMUMSHELL
	sizeof(l_off_t), sizeof(char), sizeof(short),
#endif
};
int printf_urgent = 0;


#ifndef	MINIMUMSHELL
VOID getcharwidth(s, ptr, rwp, vwp)
CONST char *s;
ALLOC_T ptr;
int *rwp, *vwp;
{
	int rw, vw;

	rw = vw = 1;
	if (iskanji1(s, ptr)) rw = vw = 2;
# ifdef	CODEEUC
	else if (isekana(s, ptr)) rw++;
# else
	else if (isskana(s, ptr)) /*EMPTY*/;
# endif
	else if (Xiscntrl(s[ptr])) vw++;
	else if (ismsb(s[ptr])) vw = 4;

	if (rwp) *rwp = rw;
	if (vwp) *vwp = vw;
}

int strlen3(s)
CONST char *s;
{
	int r, v, rw, vw;

	r = v = 0;
	while (s[r]) {
		getcharwidth(s, r, &rw, &vw);
		r += rw;
		v += vw;
	}

	return(v);
}

static int NEAR gencntrl(buf, c, pbufp)
char *buf;
int c;
printbuf_t *pbufp;
{
	char *cp;
	int n;

	cp = buf;
	if (!(pbufp -> flags & VF_PRINTABLE)) return(0);
# ifndef	CODEEUC
	else if (Xiskana(c)) return(0);
# endif
	else if (Xiscntrl(c)) {
		*(cp++) = '^';
		*(cp++) = (c + '@') & 0x7f;
	}
	else if (ismsb(c)) {
		*(cp++) = '\\';
		n = 3;
		c &= 0xff;
		while (n-- > 0) {
			cp[n] = '0' + (c & 7);
			c >>= 3;
		}
		cp += 3;
	}

	return(cp - buf);
}

static int NEAR setcntrl(c, len, pbufp)
int c, len;
printbuf_t *pbufp;
{
	char buf[MAXCHARWID + 1];
	int i, n;

	n = gencntrl(buf, c, pbufp);
	if (n <= 0) return(n);

	if (checkchar(n, pbufp) < 0) n = pbufp -> size - pbufp -> ptr - 1;
	for (i = 0; i < n; i++) {
		if (len >= 0 && --len < 0) break;
		if (setchar(buf[i], pbufp) < 0) return(-1);
	}

	return(i);
}
#endif	/* !MINIMUMSHELL */

int getnum(s, ptrp)
CONST char *s;
int *ptrp;
{
	int i, n;

	n = 0;
	for (i = *ptrp; Xisdigit(s[i]); i++) {
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
		if (pbufp -> buf && Xfputc(n, (XFILE *)(pbufp -> buf)) == EOF)
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
		pbufp -> size += PRINTBUFUNIT;
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

	memcpy((char *)&n, (char *)&u, sizeof(n));
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
	else sign = -1;

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
	else while (len < arraysize(num)) {
		if (bit) {
			c = (u & base);
#ifdef	MINIMUMSHELL
			c += '0';
#else
			c = tohexa(c);
			if (cap) c = Xtoupper(c);
#endif
		}
		else {
#ifndef	MINIMUMSHELL
			if (pbufp -> flags & VF_UNSIGNED) c = (u % base);
			else
#endif
			if ((c = (n % base)) < 0) c = -c;
			c += '0';
		}

		if ((pbufp -> flags & VF_THOUSAND)
		&& (len % (THDIGIT + 1)) == THDIGIT)
			num[len++] = ',';
		num[len++] = c;
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
#ifndef	MINIMUMSHELL
			if ((pbufp -> flags & VF_STRICTWIDTH) && width == 1)
				break;
#endif
			width--;
			c = '0';
			if ((pbufp -> flags & VF_THOUSAND)
			&& !((prec - i) % (THDIGIT + 1)))
				c = (i) ? ',' : ' ';
			if (setchar(c, pbufp) < 0) return(-1);
		}
	}
#ifndef	MINIMUMSHELL
	if ((pbufp -> flags & VF_STRICTWIDTH) && width >= 0 && width < len)
	for (i = 0; i < width; i++) {
		c = '9';
		if ((pbufp -> flags & VF_THOUSAND)
		&& !((width - i) % (THDIGIT + 1)))
			c = (i) ? ',' : ' ';
		if (setchar(c, pbufp) < 0) return(-1);
	}
	else
#endif	/* !MINIMUMSHELL */
	for (i = 0; i < len; i++) {
		if (setchar(num[len - i - 1], pbufp) < 0) return(-1);
	}

	if (width >= 0) for (; i < width; i++) {
		if (setchar(' ', pbufp) < 0) return(-1);
	}

	return(pbufp -> ptr - ptr);
}

static int NEAR setstr(s, pbufp, width, prec)
CONST char *s;
printbuf_t *pbufp;
int width, prec;
{
#ifdef	DEP_KCONV
	CONST char *s2;
	char *cp, *tmp;
	int len2;
#endif
#ifndef	MINIMUMSHELL
	int n;
#endif
	int i, c, len, total;

	if (s) {
# ifndef	MINIMUMSHELL
		if (pbufp -> flags & VF_PRINTABLE) len = strlen3(s);
		else
# endif
		len = strlen2(s);
	}
	else {
		s = NULLSTR;
		len = strsize(NULLSTR);
#ifdef	LINUX
		/* spec. of glibc */
		if (prec >= 0 && len > prec) len = 0;
#endif
	}
	if (prec >= 0 && len > prec) len = prec;

	total = len;
	if (!(pbufp -> flags & VF_MINUS) && width >= 0 && len < width) {
		total = width;
		while (len < width--) if (setchar(' ', pbufp) < 0) return(-1);
	}

#ifdef	DEP_KCONV
	if (pbufp -> flags & VF_KANJI) {
		if (!(tmp = (char *)malloc(len * KANAWID + 1))) return(-1);

		len2 = len;
		for (;;) {
			s2 = s;
			cp = tmp;
			for (i = 0; i < len2; i++, s2++) {
				c = *s2;
				if (iskanji1(s2, 0)) {
					if (++i >= len2) c = ' ';
					else {
						*(cp++) = c;
						c = *(++s2);
					}
				}
# ifdef	CODEEUC
				else if (isekana(s2, 0)) {
					*(cp++) = c;
					c = *(++s2);
				}
# endif
# ifndef	MINIMUMSHELL
				else if ((n = gencntrl(cp, c, pbufp)) > 0) {
					while (n-- > 0) {
						if (i++ >= len2) break;
						cp++;
					}
					i--;
					continue;
				}
# endif

				*(cp++) = c;
			}
			*cp = '\0';

			cp = newkanjiconv(tmp, DEFCODE,
				getoutputkcode(), L_OUTPUT);
			if ((pbufp -> flags & (VF_NEW | VF_FILE))
			|| pbufp -> ptr + strlen(cp) < pbufp -> size)
				break;
			if (cp != tmp) free(cp);
			if (--len2 <= 0) {
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
#endif	/* DEP_KCONV */
	for (i = 0; i < len; i++, s++) {
		c = *s;
		if (iskanji1(s, 0)) {
			if (++i >= len || checkchar(2, pbufp) < 0) c = ' ';
			else {
				if (setchar(c, pbufp) < 0) return(-1);
				c = *(++s);
			}
		}
#ifdef	CODEEUC
		else if (isekana(s, 0)) {
			if (checkchar(2, pbufp) < 0) c = ' ';
			else {
				if (setchar(c, pbufp) < 0) return(-1);
				c = *(++s);
			}
		}
#endif
#ifndef	MINIMUMSHELL
		else if ((n = setcntrl(c, len - i, pbufp))) {
			if (n < 0) return(-1);
			i += n - 1;
			continue;
		}
#endif

		if (setchar(c, pbufp) < 0) return(-1);
	}

	if (width >= 0 && len < width) {
		total += width - len;
		while (len < width--) if (setchar(' ', pbufp) < 0) return(-1);
	}

	return(total);
}

static int NEAR commonprintf(pbufp, fmt, args)
printbuf_t *pbufp;
CONST char *fmt;
va_list args;
{
#ifndef	HAVELONGLONG
	u_long_t tmp;
	int hi;
#endif
	u_long_t u, mask;
	CONST char *cp;
	char *s;
	int i, c, len, total, base, width, prec;

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
			if (!(cp = Xstrchr(printfflagchar, fmt[i]))) break;
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
			if (!(cp = Xstrchr(printfsizechar, fmt[i]))) break;
			len = printfsize[cp - printfsizechar];
		}

		base = 0;
		switch (fmt[i]) {
			case 'd':
				base = 10;
				break;
			case 'a':
			case 'k':
#ifdef	DEP_KCONV
				if (!printf_urgent) pbufp -> flags |= VF_KANJI;
#endif
/*FALLTHRU*/
			case 's':
				cp = s = va_arg(args, char *);
#ifdef	FD
				if (!printf_urgent && fmt[i] == 'a')
					s = restorearg(cp);
#endif
				len = setstr(s, pbufp, width, prec);
				if (cp != s) free(s);
				break;
			case 'c':
				c = va_arg(args, int);
#ifndef	MINIMUMSHELL
				if ((len = setcntrl(c, -1, pbufp))) break;
#endif
				len = setchar(c & 0xff, pbufp);
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
#ifndef	HAVELONGLONG
			if (len > (int)sizeof(u_long_t)) {
# ifndef	HPUX
	/*
	 * HP-UX always pushes arguments from lowest to uppermost,
	 * in spite of the CPU endian.
	 */
				tmp = 0x5a;
				cp = (CONST char *)(&tmp);
				if (*cp != 0x5a) {
					hi = va_arg(args, u_int);
					len -= (int)sizeof(u_int);
					while (len > (int)sizeof(u_long_t)) {
						tmp = va_arg(args, u_int);
						len -= (int)sizeof(u_int);
					}
					u = va_arg(args, u_long_t);
				}
				else
# endif	/* !HPUX */
				{
					hi = 0;
					u = va_arg(args, u_long_t);
					len -= (int)sizeof(u_long_t);
					while (len > 0) {
						hi = va_arg(args, u_int);
						len -= (int)sizeof(u_int);
					}
				}

				if (pbufp -> flags & VF_UNSIGNED) {
					if (hi) u = MAXUTYPE(u_long_t);
				}
				else {
					mask = (MAXUTYPE(u_long_t) >> 1);
					if (hi < 0) {
						if (++hi || !(u & ~mask))
							u = ~mask;
					}
					else {
						if (hi || (u & ~mask))
							u = mask;
					}
				}
			}
			else
#endif	/* !HAVELONGLONG */
			if (len == (int)sizeof(u_long_t))
				u = va_arg(args, u_long_t);
#ifdef	HAVELONGLONG
			else if (len == (int)sizeof(u_long))
				u = va_arg(args, u_long);
#endif
			else u = va_arg(args, u_int);

			if (!(pbufp -> flags & VF_UNSIGNED)) {
				mask = MAXUTYPE(u_long_t);
				if (len < (int)sizeof(u_long_t))
					mask >>= ((int)sizeof(u_long_t) - len)
						* BITSPERBYTE;
				mask >>= 1;
				if (u & ~mask) u |= ~mask;
			}
			len = setint(u, base, pbufp, width, prec);
		}

		if (len < 0) return(-1);
		total += len;
	}
	if (pbufp -> buf && !(pbufp -> flags & VF_FILE))
		pbufp -> buf[pbufp -> ptr] = '\0';

	return(total);
}

#ifndef	MINIMUMSHELL
int Xvasprintf(sp, fmt, args)
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

# ifdef	USESTDARGH
/*VARARGS2*/
int Xasprintf(char **sp, CONST char *fmt, ...)
# else
/*VARARGS2*/
int Xasprintf(sp, fmt, va_alist)
char **sp;
CONST char *fmt;
va_dcl
# endif
{
	va_list args;
	int n;

	VA_START(args, fmt);
	n = Xvasprintf(sp, fmt, args);
	va_end(args);

	return(n);
}
#endif	/* !MINIMUMSHELL */

int Xvsnprintf(s, size, fmt, args)
char *s;
int size;
CONST char *fmt;
va_list args;
{
	printbuf_t pbuf;

	if (size < 0) return(-1);
	pbuf.buf = s;
	pbuf.size = size;
	pbuf.flags = 0;

	return(commonprintf(&pbuf, fmt, args));
}

#ifdef	USESTDARGH
/*VARARGS3*/
int Xsnprintf(char *s, int size, CONST char *fmt, ...)
#else
/*VARARGS3*/
int Xsnprintf(s, size, fmt, va_alist)
char *s;
int size;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	int n;

	VA_START(args, fmt);
	n = Xvsnprintf(s, size, fmt, args);
	va_end(args);

	return(n);
}

int Xvfprintf(fp, fmt, args)
XFILE *fp;
CONST char *fmt;
va_list args;
{
	printbuf_t pbuf;

	pbuf.buf = (char *)fp;
	pbuf.flags = VF_FILE;

	return(commonprintf(&pbuf, fmt, args));
}

#ifdef	USESTDARGH
/*VARARGS2*/
int Xfprintf(XFILE *fp, CONST char *fmt, ...)
#else
/*VARARGS2*/
int Xfprintf(fp, fmt, va_alist)
XFILE *fp;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	int n;

	VA_START(args, fmt);
	n = Xvfprintf(fp, fmt, args);
	va_end(args);

	return(n);
}

#ifdef	USESTDARGH
/*VARARGS2*/
int Xprintf(CONST char *fmt, ...)
#else
/*VARARGS2*/
int Xprintf(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	int n;

	VA_START(args, fmt);
	n = Xvfprintf(Xstdout, fmt, args);
	va_end(args);

	return(n);
}

int fputnl(fp)
XFILE *fp;
{
	if (Xfputc('\n', fp) == EOF || Xfflush(fp) == EOF) return(-1);

	return(0);
}

#ifdef	FD
VOID kanjifputs(s, fp)
CONST char *s;
XFILE *fp;
{
	Xfprintf(fp, "%k", s);
}
#endif	/* FD */
