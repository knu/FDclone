/*
 *	typesize.h
 *
 *	definitions for size of types
 */

#ifndef	__TYPESIZE_H_
#define	__TYPESIZE_H_

#define	strsize(s)		((int)sizeof(s) - 1)
#define	arraysize(a)		((int)((u_int)sizeof(a) / (u_int)sizeof(*(a))))

#define	MAXLONGWIDTH		20	/* log10(2^64) = 19.266 */
#define	MAXCOLSCOMMA(d)		(MAXLONGWIDTH + (MAXLONGWIDTH / (d)))
#ifndef	BITSPERBYTE
# ifdef	CHAR_BIT
# define	BITSPERBYTE	CHAR_BIT
# else	/* !CHAR_BIT */
#  ifdef	NBBY
#  define	BITSPERBYTE	NBBY
#  else
#  define	BITSPERBYTE	8
#  endif
# endif	/* !CHAR_BIT */
#endif	/* !BITSPERBYTE */

#ifdef	HAVELONGLONG
typedef long long		long_t;
typedef unsigned long long	u_long_t;
#else
typedef long			long_t;
typedef unsigned long		u_long_t;
#endif

#define	_MAXUTYPE(t)		((t)(~(t)0))
#define	MINTYPE(t)		((t)(_MAXUTYPE(t) << \
				(BITSPERBYTE * sizeof(t) - 1)))
#define	MAXTYPE(t)		((t)~MINTYPE(t))
#ifdef	LSI_C
#define	MAXUTYPE(t)		((u_long_t)~0 >> \
				(BITSPERBYTE * (sizeof(u_long_t) - sizeof(t))))
#else
#define	MAXUTYPE(t)		_MAXUTYPE(t)
#endif

#ifdef	USELLSEEK
typedef long_t			l_off_t;
#else
typedef off_t			l_off_t;
#endif

#ifdef	USEPID_T
typedef pid_t			p_id_t;
#else	/* !USEPID_T */
# if	MSDOS
typedef int			p_id_t;
# else
typedef long			p_id_t;
# endif
#endif	/* !USEPID_T */

#endif	/* !__TYPESIZE_H_ */
