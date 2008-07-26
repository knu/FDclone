/*
 *	malloc.c
 *
 *	alternative memoly allocation functions
 */

#include "headers.h"
#include "printf.h"
#include "malloc.h"
#include "sysemu.h"

#ifdef	FD
extern VOID error __P_((CONST char *));
#else
static VOID NEAR error __P_((CONST char *));
#endif


#ifndef	FD
static VOID NEAR error(s)
CONST char *s;
{
	fprintf2(Xstderr, "%s: memory allocation error", s);
	VOID_C fputnl(Xstderr);

	exit(2);
}
#endif	/* !FD */

char *malloc2(size)
ALLOC_T size;
{
	char *tmp;

	if (!size || !(tmp = (char *)malloc(size))) {
		error("malloc()");
#ifdef	FAKEUNINIT
		tmp = NULL;	/* fake for -Wuninitialized */
#endif
	}

	return(tmp);
}

char *realloc2(ptr, size)
VOID_P ptr;
ALLOC_T size;
{
	char *tmp;

	if (!size
	|| !(tmp = (ptr) ? (char *)realloc(ptr, size) : (char *)malloc(size)))
	{
		error("realloc()");
#ifdef	FAKEUNINIT
		tmp = NULL;	/* fake for -Wuninitialized */
#endif
	}

	return(tmp);
}

VOID free2(ptr)
VOID_P ptr;
{
	int duperrno;

	if (!ptr) return;
	duperrno = errno;
	free(ptr);
	errno = duperrno;
}

char *c_realloc(ptr, n, sizep)
char *ptr;
ALLOC_T n, *sizep;
{
	if (!ptr) {
		*sizep = BUFUNIT;
		return(malloc2(*sizep));
	}
	while (n + 1 >= *sizep) *sizep *= 2;

	return(realloc2(ptr, *sizep));
}

char *strdup2(s)
CONST char *s;
{
	char *tmp;
	int n;

	if (!s) return(NULL);
	n = strlen(s);
	if (!(tmp = (char *)malloc((ALLOC_T)n + 1))) error("malloc()");
	memcpy(tmp, s, n + 1);

	return(tmp);
}

char *strndup2(s, n)
CONST char *s;
int n;
{
	char *tmp;
	int i;

	if (!s) return(NULL);
	for (i = 0; i < n; i++) if (!s[i]) break;
	if (!(tmp = (char *)malloc((ALLOC_T)i + 1))) error("malloc()");
	memcpy(tmp, s, i);
	tmp[i] = '\0';

	return(tmp);
}

#ifndef	MINIMUMSHELL
int vasprintf3(sp, fmt, args)
char **sp;
CONST char *fmt;
va_list args;
{
	int n;

	n = vasprintf2(sp, fmt, args);
	if (n < 0) error("malloc()");

	return(n);
}

#ifdef	USESTDARGH
/*VARARGS1*/
char *asprintf3(CONST char *fmt, ...)
#else
/*VARARGS1*/
char *asprintf3(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char *cp;

	VA_START(args, fmt);
	VOID_C vasprintf3(&cp, fmt, args);
	va_end(args);

	return(cp);
}
#endif	/* !MINIMUMSHELL */
