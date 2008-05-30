/*
 *	encode.c
 *
 *	MD5 message digest in RFC1321 & BASE64 encoding in RFC3548
 */

#include "headers.h"
#include "kctype.h"
#include "sysemu.h"
#include "pathname.h"
#include "encode.h"

static VOID NEAR voidmd5 __P_((u_long, u_long, u_long, u_long));
static VOID NEAR calcmd5 __P_((md5_t *));
static VOID NEAR initmd5 __P_((md5_t *));
static VOID NEAR addmd5 __P_((md5_t *, CONST u_char *, ALLOC_T));
static VOID NEAR endmd5 __P_((md5_t *, u_char *, ALLOC_T *));
static VOID NEAR base64encodechar __P_((char *, int, int));

static CONST char encode64table[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/',
};


/*ARGSUSED*/
static VOID NEAR voidmd5(a, b, c, d)
u_long a, b, c, d;
{
	/* For bugs on NEWS-OS optimizer */
}

static VOID NEAR calcmd5(md5p)
md5_t *md5p;
{
	u_long a, b, c, d, tmp, t[MD5_BLOCKS];
	int i, s[MD5_BUFSIZ];

	a = md5p -> sum[0];
	b = md5p -> sum[1];
	c = md5p -> sum[2];
	d = md5p -> sum[3];

	s[0] = 7;
	s[1] = 12;
	s[2] = 17;
	s[3] = 22;
	t[0] = 0xd76aa478;	/* floor(4294967296.0 * fabs(sin(1.0))) */
	t[1] = 0xe8c7b756;
	t[2] = 0x242070db;
	t[3] = 0xc1bdceee;
	t[4] = 0xf57c0faf;
	t[5] = 0x4787c62a;
	t[6] = 0xa8304613;
	t[7] = 0xfd469501;
	t[8] = 0x698098d8;
	t[9] = 0x8b44f7af;
	t[10] = 0xffff5bb1;
	t[11] = 0x895cd7be;
	t[12] = 0x6b901122;
	t[13] = 0xfd987193;
	t[14] = 0xa679438e;
	t[15] = 0x49b40821;
	for (i = 0; i < MD5_BLOCKS; i++) {
		a += ((b & c) | (~b & d)) + (md5p -> x[i]) + t[i];
		tmp = b + ((a << s[i % MD5_BUFSIZ])
			| a >> (32 - s[i % MD5_BUFSIZ]));
		tmp &= (u_long)0xffffffff;
		a = d;
		d = c;
		c = b;
		b = tmp;
		voidmd5(a, b, c, d);
	}

	s[0] = 5;
	s[1] = 9;
	s[2] = 14;
	s[3] = 20;
	t[0] = 0xf61e2562;	/* floor(4294967296.0 * fabs(sin(16.0))) */
	t[1] = 0xc040b340;
	t[2] = 0x265e5a51;
	t[3] = 0xe9b6c7aa;
	t[4] = 0xd62f105d;
	t[5] = 0x02441453;
	t[6] = 0xd8a1e681;
	t[7] = 0xe7d3fbc8;
	t[8] = 0x21e1cde6;
	t[9] = 0xc33707d6;
	t[10] = 0xf4d50d87;
	t[11] = 0x455a14ed;
	t[12] = 0xa9e3e905;
	t[13] = 0xfcefa3f8;
	t[14] = 0x676f02d9;
	t[15] = 0x8d2a4c8a;
	for (i = 0; i < MD5_BLOCKS; i++) {
		a += ((b & d) | (c & ~d))
			+ (md5p -> x[(i * 5 + 1) % MD5_BLOCKS]) + t[i];
		tmp = b + ((a << s[i % MD5_BUFSIZ])
			| a >> (32 - s[i % MD5_BUFSIZ]));
		tmp &= (u_long)0xffffffff;
		a = d;
		d = c;
		c = b;
		b = tmp;
		voidmd5(a, b, c, d);
	}

	s[0] = 4;
	s[1] = 11;
	s[2] = 16;
	s[3] = 23;
	t[0] = 0xfffa3942;	/* floor(4294967296.0 * fabs(sin(32.0))) */
	t[1] = 0x8771f681;
	t[2] = 0x6d9d6122;
	t[3] = 0xfde5380c;
	t[4] = 0xa4beea44;
	t[5] = 0x4bdecfa9;
	t[6] = 0xf6bb4b60;
	t[7] = 0xbebfbc70;
	t[8] = 0x289b7ec6;
	t[9] = 0xeaa127fa;
	t[10] = 0xd4ef3085;
	t[11] = 0x04881d05;
	t[12] = 0xd9d4d039;
	t[13] = 0xe6db99e5;
	t[14] = 0x1fa27cf8;
	t[15] = 0xc4ac5665;
	for (i = 0; i < MD5_BLOCKS; i++) {
		a += (b ^ c ^ d)
			+ (md5p -> x[(i * 3 + 5) % MD5_BLOCKS]) + t[i];
		tmp = b + ((a << s[i % MD5_BUFSIZ])
			| a >> (32 - s[i % MD5_BUFSIZ]));
		tmp &= (u_long)0xffffffff;
		a = d;
		d = c;
		c = b;
		b = tmp;
		voidmd5(a, b, c, d);
	}

	s[0] = 6;
	s[1] = 10;
	s[2] = 15;
	s[3] = 21;
	t[0] = 0xf4292244;	/* floor(4294967296.0 * fabs(sin(48.0))) */
	t[1] = 0x432aff97;
	t[2] = 0xab9423a7;
	t[3] = 0xfc93a039;
	t[4] = 0x655b59c3;
	t[5] = 0x8f0ccc92;
	t[6] = 0xffeff47d;
	t[7] = 0x85845dd1;
	t[8] = 0x6fa87e4f;
	t[9] = 0xfe2ce6e0;
	t[10] = 0xa3014314;
	t[11] = 0x4e0811a1;
	t[12] = 0xf7537e82;
	t[13] = 0xbd3af235;
	t[14] = 0x2ad7d2bb;
	t[15] = 0xeb86d391;
	for (i = 0; i < MD5_BLOCKS; i++) {
		a += (c ^ (b | ~d))
			+ (md5p -> x[(i * 7) % MD5_BLOCKS]) + t[i];
		tmp = b + ((a << s[i % MD5_BUFSIZ])
			| a >> (32 - s[i % MD5_BUFSIZ]));
		tmp &= (u_long)0xffffffff;
		a = d;
		d = c;
		c = b;
		b = tmp;
		voidmd5(a, b, c, d);
	}

	md5p -> sum[0] = (md5p -> sum[0] + a) & (u_long)0xffffffff;
	md5p -> sum[1] = (md5p -> sum[1] + b) & (u_long)0xffffffff;
	md5p -> sum[2] = (md5p -> sum[2] + c) & (u_long)0xffffffff;
	md5p -> sum[3] = (md5p -> sum[3] + d) & (u_long)0xffffffff;
}

static VOID NEAR initmd5(md5p)
md5_t *md5p;
{
	md5p -> cl = md5p -> ch = (u_long)0;
	md5p -> sum[0] = (u_long)0x67452301;
	md5p -> sum[1] = (u_long)0xefcdab89;
	md5p -> sum[2] = (u_long)0x98badcfe;
	md5p -> sum[3] = (u_long)0x10325476;
	memset((char *)(md5p -> x), 0, sizeof(md5p -> x));
	md5p -> n = md5p -> b = 0;
}

static VOID NEAR addmd5(md5p, s, len)
md5_t *md5p;
CONST u_char *s;
ALLOC_T len;
{
	while (len--) {
		if (md5p -> cl <= (u_long)0xffffffff - (u_long)BITSPERBYTE)
			md5p -> cl += (u_long)BITSPERBYTE;
		else {
			md5p -> cl -=
				(u_long)0xffffffff - (u_long)BITSPERBYTE + 1;
			(md5p -> ch)++;
			md5p -> ch &= (u_long)0xffffffff;
		}

		md5p -> x[md5p -> n] |= ((u_long)(*(s++)) << (md5p -> b));
		if ((md5p -> b += BITSPERBYTE) >= BITSPERBYTE * 4) {
			md5p -> b = 0;
			if (++(md5p -> n) >= MD5_BLOCKS) {
				md5p -> n = 0;
				calcmd5(md5p);
				memset((char *)(md5p -> x),
					0, sizeof(md5p -> x));
			}
		}
	}
}

static VOID NEAR endmd5(md5p, buf, sizep)
md5_t *md5p;
u_char *buf;
ALLOC_T *sizep;
{
	ALLOC_T ptr;
	int i, n;

	md5p -> x[md5p -> n] |= (1 << ((md5p -> b) + BITSPERBYTE - 1));
	if (md5p -> n >= 14) {
		calcmd5(md5p);
		memset((char *)(md5p -> x), 0, sizeof(md5p -> x));
	}
	md5p -> x[14] = md5p -> cl;
	md5p -> x[15] = md5p -> ch;
	calcmd5(md5p);

	ptr = (ALLOC_T)0;
	for (i = 0; i < MD5_BUFSIZ; i++) for (n = 0; n < 4; n++) {
		if (ptr >= *sizep) break;
		buf[ptr++] = (md5p -> sum[i] & 0xff);
		md5p -> sum[i] >>= BITSPERBYTE;
	}

	*sizep = ptr;
}

VOID md5encode(buf, sizep, s, len)
u_char *buf;
ALLOC_T *sizep;
CONST u_char *s;
ALLOC_T len;
{
	md5_t md5;

	if (!s) s = (u_char *)nullstr;
	if (len == (ALLOC_T)-1) len = strlen((char *)s);

	initmd5(&md5);
	addmd5(&md5, s, len);
	endmd5(&md5, buf, sizep);
}

int md5fencode(buf, sizep, fp)
u_char *buf;
ALLOC_T *sizep;
XFILE *fp;
{
	md5_t md5;
	u_char tmp[MD5_FILBUFSIZ];
	int n;

	initmd5(&md5);
	if (fp) for (;;) {
		n = Xfread((char *)tmp, sizeof(tmp), fp);
		if (n < 0) return(-1);
		else if (!n) break;
		addmd5(&md5, tmp, n);
	}
	endmd5(&md5, buf, sizep);

	return(0);
}

static VOID NEAR base64encodechar(s, n, val)
char *s;
int n, val;
{
	val &= 0x3f;
	s[n] = encode64table[val];
}

int base64encode(buf, size, s, len)
char *buf;
ALLOC_T size;
CONST u_char *s;
ALLOC_T len;
{
	int i, j, n;

	if (len == (ALLOC_T)-1) len = strlen((char *)s);
	for (i = j = 0; i < len; i++) {
		if (j + BASE64_ENCSIZ >= size) {
			errno = ERANGE;
			return(-1);
		}
		base64encodechar(buf, j++, s[i] >> 2);
		n = ((s[i] << 4) & 0x30);
		if (i + 1 >= len) {
			base64encodechar(buf, j++, n);
			buf[j++] = '=';
			buf[j++] = '=';
		}
		else {
			base64encodechar(buf, j++,
				n | ((s[++i] >> 4) & 0x0f));
			n = ((s[i] << 2) & 0x3c);
			if (i + 1 >= len) {
				base64encodechar(buf, j++, n);
				buf[j++] = '=';
			}
			else {
				base64encodechar(buf, j++,
					n | ((s[++i] >> 6) & 0x03));
				base64encodechar(buf, j++, s[i]);
			}
		}
	}
	buf[j] = '\0';
	errno = 0;

	return(0);
}
