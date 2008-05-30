/*
 *	time.h
 *
 *	definitions & function prototype declarations for "time.c"
 */

#if	(GETTODARGS == 1)
#define	gettimeofday2(tv, tz)	gettimeofday(tv)
#else
#define	gettimeofday2(tv, tz)	gettimeofday(tv, tz)
#endif

extern time_t time2 __P_((VOID_A));
extern time_t timegm2 __P_((CONST struct tm *));
extern time_t timelocal2 __P_((struct tm *));
