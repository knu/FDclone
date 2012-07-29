/*
 *	gentbl.c
 *
 *	table generator module
 */

#include "headers.h"
#include "gentbl.h"

#define	MAXBYTES		8

int textmode = 0;

static int textlen = 0;
static int textnext = 0;


FILE *opentbl(path)
CONST char *path;
{
	FILE *fp;

	if (!(fp = fopen(path, (textmode) ? "w" : "wb"))) {
		fprintf(stderr, "%s: Cannot open.\n", path);
		return(NULL);
	}

	return(fp);
}

int fputheader(name, fp)
CONST char *name;
FILE *fp;
{
	if (!textmode) return(0);
	if (fprintf(fp, "#include \"%s.h\"\n", name) < 0 && ferror(fp))
		return(-1);

	return(0);
}

int fputbegin(name, fp)
CONST char *name;
FILE *fp;
{
	if (!textmode) return(0);
	textnext = textlen = 0;
	if (fprintf(fp, "CONST u_char %s[] = {", name) < 0 && ferror(fp))
		return(-1);

	return(0);
}

int fputend(fp)
FILE *fp;
{
	if (!textmode) return(0);

	if (!textnext) {
		fputbyte('\0', fp);
		fputs("\t/* dummy entry */", fp);
	}

	return((fputs("\n};\n", fp) == EOF && ferror(fp)) ? -1 : 0);
}

int fputbyte(c, fp)
int c;
FILE *fp;
{
	CONST char *sep1, *sep2;

	if (!textmode) return((fputc(c, fp) == EOF && ferror(fp)) ? -1 : 0);

	sep1 = (textnext) ? "," : "";
	sep2 = (textlen) ? " " : "\n\t";

	textnext = 1;
	if (++textlen >= MAXBYTES) textlen = 0;

	if (fprintf(fp, "%s%s0x%02x", sep1, sep2, c & 0xff) < 0 && ferror(fp))
		return(-1);

	return(0);
}

int fputword(w, fp)
u_int w;
FILE *fp;
{
	if (fputbyte((int)(w & 0xff), fp) < 0
	|| fputbyte((int)((w >> 8) & 0xff), fp) < 0)
		return(-1);

	return(0);
}

int fputdword(dw, fp)
long dw;
FILE *fp;
{
	if (fputword((u_int)(dw & 0xffff), fp) < 0
	|| fputword((u_int)((dw >> 16) & 0xffff), fp) < 0)
		return(-1);

	return(0);
}

int fputbuf(buf, size, fp)
CONST u_char *buf;
ALLOC_T size;
FILE *fp;
{
	ALLOC_T n;

	if (!textmode) {
		if (fwrite(buf, size, (ALLOC_T)1, fp) != (ALLOC_T)1)
			return(-1);
	}
	else for (n = (ALLOC_T)0; n < size; n++)
		if (fputbyte((int)(buf[n]), fp) < 0) return(-1);

	return(0);
}

int fputlength(name, len, fp, width)
CONST char *name;
long len;
FILE *fp;
int width;
{
	CONST char *cp;

	if (!textmode) {
		switch (width) {
			case 2:
				if (fputword(len, fp) < 0) return(-1);
				break;
			case 4:
				if (fputdword(len, fp) < 0) return(-1);
				break;
			default:
				if (fputbyte(len, fp) < 0) return(-1);
				break;
		}
		return(0);
	}

	switch (width) {
		case 2:
			cp = "u_int";
			break;
		case 4:
			cp = "long";
			break;
		default:
			cp = "int";
			break;
	}

	if (fprintf(fp, "%s %s = %ld;\n", cp, name, len) < 0 && ferror(fp))
		return(-1);

	return(0);
}
