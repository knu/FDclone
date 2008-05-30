/*
 *	malloc.h
 *
 *	definitions & function prototype declarations for "malloc.c"
 */

#define	BUFUNIT			32
#define	b_size(n, type)		((((n) / BUFUNIT) + 1) \
				* BUFUNIT * sizeof(type))
#define	b_realloc(ptr, n, type)	(((n) % BUFUNIT) ? ((type *)(ptr)) \
				: (type *)realloc2(ptr, b_size(n, type)))

extern char *malloc2 __P_((ALLOC_T));
extern char *realloc2 __P_((VOID_P, ALLOC_T));
extern VOID free2 __P_((VOID_P));
extern char *strdup2 __P_((CONST char *));
extern char *strndup2 __P_((CONST char *, int));
extern char *c_realloc __P_((char *, ALLOC_T, ALLOC_T *));
#ifndef	MINIMUMSHELL
extern int vasprintf3 __P_((char **, CONST char *, va_list));
extern char *asprintf3 __P_((CONST char *, ...));
#endif
