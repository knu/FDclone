/*
 *	time.h
 *
 *	definitions & function prototype declarations for "time.c"
 */

#if	(GETTODARGS == 1)
#define	Xgettimeofday(tv, tz)	gettimeofday(tv)
#else
#define	Xgettimeofday(tv, tz)	gettimeofday(tv, tz)
#endif

extern time_t Xtime __P_((time_t *));
extern time_t Xtimegm __P_((CONST struct tm *));
extern time_t Xtimelocal __P_((struct tm *));
