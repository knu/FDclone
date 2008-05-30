/*
 *	stream.h
 *
 *	definitions & function prototype declarations for "stream.c"
 */

#ifndef	__STREAM_H_
#define	__STREAM_H_

#include "depend.h"

#ifndef	XF_BUFSIZ
#define	XF_BUFSIZ		((ALLOC_T)BUFSIZ)
#endif

#ifdef	DEP_ORIGSTREAM
typedef struct _XFILE {
	int fd;
	char buf[XF_BUFSIZ];
	ALLOC_T ptr;
	ALLOC_T count;
	int flags;
#if	defined (DEP_URLPATH) && !defined (NOSELECT)
	int timeout;
#endif
} XFILE;
#else
#define	XFILE			FILE
#endif

#define	XF_EOF			000001
#define	XF_ERROR		000002
#define	XF_CLOSED		000004
#define	XF_READ			000010
#define	XF_WRITTEN		000020
#define	XF_RDONLY		000040
#define	XF_WRONLY		000100
#define	XF_NOBUF		000200
#define	XF_LINEBUF		000400
#define	XF_CRNL			001000
#define	XF_NONBLOCK		002000
#define	XF_TELNET		004000
#define	XF_NULLCONV		010000

#ifdef	DEP_ORIGSTREAM
extern XFILE *Xfopen __P_((CONST char *, CONST char *));
extern XFILE *Xfdopen __P_((int, CONST char *));
extern int Xfclose __P_((XFILE *));
extern VOID Xclearerr __P_((XFILE *));
extern int Xfeof __P_((XFILE *));
extern int Xferror __P_((XFILE *));
extern int Xfileno __P_((XFILE *));
extern VOID Xsetflags __P_((XFILE *, int));
# if	defined (DEP_URLPATH) && !defined (NOSELECT)
extern VOID Xsettimeout __P_((XFILE *, int));
# endif
extern int Xfflush __P_((XFILE *));
extern int Xfread __P_((char *, ALLOC_T, XFILE *));
extern int Xfwrite __P_((CONST char *, ALLOC_T, XFILE *));
extern int Xfgetc __P_((XFILE *));
extern int Xfputc __P_((int, XFILE *));
extern char *Xfgets __P_((XFILE *));
extern int Xfputs __P_((CONST char *, XFILE *));
extern VOID Xsetbuf __P_((XFILE *));
extern VOID Xsetlinebuf __P_((XFILE *));
#else	/* !DEP_ORIGSTREAM */
#define	Xfopen			fopen
#define	Xfdopen			fdopen
#define	Xfclose			fclose
#define	Xclearerr		clearerr
#define	Xfeof			feof
#define	Xferror			ferror
#define	Xfileno			fileno
#define	Xfflush			fflush
#define	Xfread(p, s, f)		fread(p, 1, s, f)
#define	Xfwrite(p, s, f)	fwrite(p, 1, s, f)
#define	Xfgetc			fgetc
#define	Xfputc			fputc
#define	Xfputs			fputs
# if	MSDOS
# define	Xsetbuf(f)	setbuf(f, NULL)
# define	Xsetlinebuf(f)
# else	/* !MSDOS */
#  ifdef	USESETVBUF
#  define	Xsetbuf(f)	setvbuf(f, NULL, _IONBF, 0)
#  define	Xsetlinebuf(f)	setvbuf(f, NULL, _IOLBF, 0)
#  else
#  define	Xsetbuf(f)	setbuf(f, NULL)
#  define	Xsetlinebuf(f)	setlinebuf(f)
#  endif
# endif	/* !MSDOS */
#endif	/* !DEP_ORIGSTREAM */
#if	defined (FD) && !defined (DEP_ORIGSHELL)
extern XFILE *Xpopen __P_((CONST char *, CONST char *));
extern int Xpclose __P_((XFILE *));
#endif
#ifndef	FD
extern char *gets2 __P_((CONST char *));
#endif

#ifdef	DEP_ORIGSTREAM
extern XFILE *Xstdin;
extern XFILE *Xstdout;
extern XFILE *Xstderr;
#else
#define	Xstdin			stdin
#define	Xstdout			stdout
#define	Xstderr			stderr
#endif

#endif	/* !__STREAM_H_ */
