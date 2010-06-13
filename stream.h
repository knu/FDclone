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

#if	MSDOS
#define	CH_EOF			'\032'
#else
#define	CH_EOF			'\004'
#endif

#ifdef	DEP_ORIGSTREAM
typedef struct _XFILE {
	int fd;
	int status;
	int flags;
	ALLOC_T ptr;
	ALLOC_T count;
	char buf[XF_BUFSIZ];
# ifdef	DEP_STREAMTIMEOUT
	int timeout;
# endif
# ifdef	DEP_STREAMLOG
	VOID_T (*dumpfunc)__P_((CONST u_char *, ALLOC_T, CONST char *));
	int debuglvl;
	CONST char *debugmes;
	char path[1];
# endif
} XFILE;
#else	/* !DEP_ORIGSTREAM */
#define	XFILE			FILE
#endif	/* !DEP_ORIGSTREAM */

#define	XS_EOF			000001
#define	XS_ERROR		000002
#define	XS_CLOSED		000004
#define	XS_READ			000010
#define	XS_WRITTEN		000020
#define	XS_BINARY		000040
#define	XS_RDONLY		000100
#define	XS_WRONLY		000200
#define	XS_LOCKED		000400
#define	XS_NOAHEAD		001000
#define	XS_CLEARBUF		002000
#define	XF_NOBUF		000001
#define	XF_LINEBUF		000002
#define	XF_CRNL			000004
#define	XF_NOCLOSE		000010
#define	XF_NONBLOCK		000020
#define	XF_CONNECTED		000040
#define	XF_TELNET		000100
#define	XF_NULLCONV		000200

#ifdef	DEP_ORIGSTREAM
extern XFILE *Xfopen __P_((CONST char *, CONST char *));
extern XFILE *Xfdopen __P_((int, CONST char *));
extern int Xfclose __P_((XFILE *));
extern VOID Xclearerr __P_((XFILE *));
extern int Xfeof __P_((XFILE *));
extern int Xferror __P_((XFILE *));
extern int Xfileno __P_((XFILE *));
extern VOID Xsetflags __P_((XFILE *, int));
# ifdef	DEP_STREAMTIMEOUT
extern VOID Xsettimeout __P_((XFILE *, int));
# endif
extern int Xfflush __P_((XFILE *));
extern int Xfpurge __P_((XFILE *));
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
#define	Xfpurge(f)
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
extern int (*stream_isnfsfunc)__P_((CONST char *));
extern XFILE *Xstdin;
extern XFILE *Xstdout;
extern XFILE *Xstderr;
#else
#define	Xstdin			stdin
#define	Xstdout			stdout
#define	Xstderr			stderr
#endif

#endif	/* !__STREAM_H_ */
