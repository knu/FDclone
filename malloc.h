/*
 *	malloc.h
 *
 *	definitions & function prototype declarations for "malloc.c"
 */

#define	BUFUNIT			32
#define	b_size(n, type)		((((n) / BUFUNIT) + 1) \
				* BUFUNIT * sizeof(type))
#define	b_realloc(ptr, n, type)	(((n) % BUFUNIT) ? ((type *)(ptr)) \
				: (type *)Xrealloc(ptr, b_size(n, type)))

extern char *Xmalloc __P_((ALLOC_T));
extern char *Xrealloc __P_((VOID_P, ALLOC_T));
extern VOID Xfree __P_((VOID_P));
extern char *Xstrdup __P_((CONST char *));
extern char *Xstrndup __P_((CONST char *, int));
extern char *c_realloc __P_((char *, ALLOC_T, ALLOC_T *));
#ifndef	MINIMUMSHELL
extern int vasprintf2 __P_((char **, CONST char *, va_list));
extern char *asprintf2 __P_((CONST char *, ...));
#endif
