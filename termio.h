/*
 *	termio.h
 *
 *	type definitions for "termio.c"
 */

#ifndef	__TERMIO_H_
#define	__TERMIO_H_

#if	MSDOS
#undef	USETERMIOS
#undef	USETERMIO
#undef	USESGTTY
#else
#include <sys/ioctl.h>
#endif

#ifndef	STDIN_FILENO
#define	STDIN_FILENO	0
#endif
#ifndef	STDOUT_FILENO
#define	STDOUT_FILENO	1
#endif
#ifndef	STDERR_FILENO
#define	STDERR_FILENO	2
#endif

#ifdef	USETERMIOS
#include <termios.h>
typedef struct termios	termioctl_t;
typedef struct termios	ldiscioctl_t;
#define	tioctl(d, r, a)	((r) ? Xtcsetattr(d, (r) - 1, a) : Xtcgetattr(d, a))
#define	getspeed(t)	cfgetospeed(&t)
# ifdef	HAVECLINE
# define	ldisc(a)	((a).c_line)
# endif
#define	REQGETP		0
#define	REQGETD		0
#define	REQSETP		(TCSAFLUSH + 1)
#define	REQSETD		(TCSADRAIN + 1)
#define	REQSETN		(TCSANOW + 1)
#endif	/* !USETERMIOS */

#ifdef	USETERMIO
#include <termio.h>
typedef struct termio	termioctl_t;
typedef struct termio	ldiscioctl_t;
#define	tioctl		Xioctl
#define	getspeed(t)	((t).c_cflag & CBAUD)
#define	ldisc(a)	((a).c_line)
#define	REQGETP		TCGETA
#define	REQGETD		TCGETA
#define	REQSETP		TCSETAF
#define	REQSETD		TCSETAW
#define	REQSETN		TCSETA
#endif	/* !USETERMIO */

#ifdef	USESGTTY
#include <sgtty.h>
typedef struct sgttyb	termioctl_t;
typedef int		ldiscioctl_t;
#define	tioctl		Xioctl
#define	getspeed(t)	((t).sg_ospeed)
#define	ldisc(a)	(a)
#define	REQGETP		TIOCGETP
#define	REQGETD		TIOCGETD
#define	REQSETP		TIOCSETP
#define	REQSETD		TIOCSETD
#define	REQSETN		TIOCSETN
#endif	/* !USESGTTY */

#ifdef	TIOCGWINSZ
typedef struct winsize	termwsize_t;
#define	REQGETWS	TIOCGWINSZ
#define	REQSETWS	TIOCSWINSZ
#else	/* !TIOCGWINSZ */
# ifdef	WIOCGETD
typedef struct uwdata	termwsize_t;
#define	REQGETWS	WIOCGETD
#define	REQSETWS	WIOCSETD
# else	/* !WIOCGETD */
#  ifdef	TIOCGSIZE
typedef struct ttysize	termwsize_t;
#define	REQGETWS	TIOCGSIZE
#define	REQSETWS	TIOCSSIZE
#  else	/* !TIOCGSIZE */
#define	NOTERMWSIZE
#  endif	/* !TIOCGSIZE */
# endif	/* !WIOCGETD */
#endif	/* !TIOCGWINSZ */

#ifndef	PENDIN
#define	PENDIN		0
#endif
#ifndef	IEXTEN
#define	IEXTEN		0
#endif
#ifndef	ECHOCTL
#define	ECHOCTL		0
#endif
#ifndef	ECHOKE
#define	ECHOKE		0
#endif
#ifndef	OCRNL
#define	OCRNL		0
#endif
#ifndef	ONOCR
#define	ONOCR		0
#endif
#ifndef	ONLRET
#define	ONLRET		0
#endif
#ifndef	TAB3
#define	TAB3		OXTABS
#endif

#define	TIO_LCBREAK	(ISIG | IEXTEN)
#define	TIO_LCOOKED	(TIO_LCBREAK | ICANON)
#define	TIO_LECHO	(ECHO | ECHOE | ECHOCTL | ECHOKE)
#define	TIO_LNOECHO	~(ECHO | ECHOE | ECHOK | ECHONL)
#define	TIO_ICOOKED	(BRKINT | IXON)
#define	TIO_INOCOOKED	~(IGNBRK | ISTRIP)
#define	TIO_ONL		(OPOST | ONLCR)
#define	TIO_ONONL	~(OCRNL | ONOCR | ONLRET)

#if	MSDOS
# ifdef	DJGPP
# define	TIO_BUFSIZ	(ALLOC_T)0
# else
# define	TIO_BUFSIZ	sizeof(u_char)
# endif
# define	TIO_WINSIZ	(ALLOC_T)0
#else	/* !MSDOS */
# ifdef	USESGTTY
# define	TIO_BUFSIZ	(sizeof(termioctl_t) \
				+ sizeof(int) + sizeof(struct tchars))
# else
# define	TIO_BUFSIZ	sizeof(termioctl_t)
# endif
# ifdef	NOTERMWSIZE
# define	TIO_WINSIZ	(ALLOC_T)0
# else
# define	TIO_WINSIZ	sizeof(termwsize_t)
# endif
#endif	/* !MSDOS */

#if	defined (USETERMIOS) || defined (USETERMIO)
#if	(VEOF == VMIN) || (VEOL == VTIME)
#define	VAL_VMIN	'\004'
#define	VAL_VTIME	255
#else
#define	VAL_VMIN	0
#define	VAL_VTIME	0
#endif
#endif	/* USETERMIOS || USETERMIO */

#if	MSDOS
#include <dos.h>
#include <io.h>
#define	FR_CARRY	00001
#define	FR_PARITY	00004
#define	FR_ACARRY	00020
#define	FR_ZERO		00100
#define	FR_SIGN		00200
#define	FR_TRAP		00400
#define	FR_INTERRUPT	01000
#define	FR_DIRECTION	02000
#define	FR_OVERFLOW	04000
# ifdef	DJGPP
# include <dpmi.h>
# include <go32.h>
# include <sys/farptr.h>
#  if	(DJGPP >= 2)
#  include <libc/dosio.h>
#  else
#  define	__dpmi_regs	_go32_dpmi_registers
#  define	__dpmi_int(v,r)	((r) -> x.ss = (r) -> x.sp = 0, \
				_go32_dpmi_simulate_int(v, r))
#  define	_dos_ds		_go32_info_block.selector_for_linear_memory
#  define	__tb	_go32_info_block.linear_address_of_transfer_buffer
#  define	__tb_offset	(__tb & 15)
#  define	__tb_segment	(__tb / 16)
#  endif
# define	tbsize		_go32_info_block.size_of_transfer_buffer
# define	PTR_FAR(ptr)		((u_long)(__tb))
# define	PTR_SEG(ptr)		(__tb_segment)
# define	PTR_OFF(ptr, ofs)	(__tb_offset + (ofs))
# else	/* !DJGPP */
#  ifdef	__TURBOC__	/* Oops!! Borland C++ has not x.bp !! */
typedef union DPMI_REGS {
	struct XREGS {
		u_short ax, bx, cx, dx, si, di, bp, flags;
	} x;
	struct HREGS {
		u_char al, ah, bl, bh, cl, ch, dl, dh;
	} h;
} __dpmi_regs;
#  else
typedef union REGS	__dpmi_regs;
#  endif
# define	__attribute__(x)
# define	PTR_FAR(ptr)		(((u_long)FP_SEG(ptr) << 4) \
					+ FP_OFF(ptr))
# define	PTR_SEG(ptr)		FP_SEG(ptr)
# define	PTR_OFF(ptr, ofs)	FP_OFF(ptr)
# endif	/* !DJGPP */
#endif	/* MSDOS */

#define	ENVTERM		"TERM"
#define	ENVLINES	"LINES"
#define	ENVCOLUMNS	"COLUMNS"
#define	ENVEMACS	"EMACS"

#ifndef	_PATH_TTY
# if	MSDOS
# define	_PATH_TTY	"CON"
# else
# define	_PATH_TTY	"/dev/tty"
# endif
#endif	/* !_PATH_TTY */

#if	defined (CYGWIN) && !defined (__PATHNAME_H_)
# ifdef	USEPID_T
typedef pid_t		p_id_t;
# else
typedef long		p_id_t;
# endif
#endif	/* CYGWIN && !__PATHNAME_H_ */

#ifdef	LSI_C
extern int safe_dup __P_((int));
extern int safe_dup2 __P_((int, int));
#else
#define	safe_dup	dup
#define	safe_dup2	dup2
#endif
#if	MSDOS
extern int seterrno __P_((u_int));
extern int intcall __P_((int, __dpmi_regs *, struct SREGS *));
#endif
extern int Xgetdtablesize __P_((VOID_A));
extern int isvalidfd __P_((int));
extern int newdup __P_((int));
extern int sureread __P_((int, VOID_P, int));
extern int surewrite __P_((int, VOID_P, int));
extern VOID safeclose __P_((int));
extern int opentty __P_((int *, FILE **));
extern VOID closetty __P_((int *, FILE **));
#if	!MSDOS
extern VOID closeonexec __P_((int));
extern int Xioctl __P_((int, int, VOID_P));
# ifdef	USETERMIOS
extern int Xtcgetattr __P_((int, termioctl_t *));
extern int Xtcsetattr __P_((int, int, termioctl_t *));
extern int Xtcflush __P_((int, int));
# endif
#endif	/* !MSDOS */
extern VOID loadtermio __P_((int, char *, char *));
extern VOID savetermio __P_((int, char **, char **));
#ifdef	CYGWIN
extern p_id_t Xfork __P_((VOID_A));
#else
#define	Xfork		fork
#endif
#if	!MSDOS \
|| (!defined(NOTUSEBIOS) && defined (DJGPP) && (DJGPP >= 2) && !defined(PC98))
extern int readselect __P_((int, int [], char [], VOID_P));
#endif

#endif	/* !__TERMIO_H_ */