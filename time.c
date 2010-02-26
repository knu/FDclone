/*
 *	time.c
 *
 *	alternative time functions
 */

#include "headers.h"
#include "typesize.h"
#include "string.h"
#include "time.h"

#if	!MSDOS && !defined (USEMKTIME) && !defined (USETIMELOCAL)
#include "sysemu.h"
#include "pathname.h"
#include "stream.h"
#endif

#ifndef	NOTZFILEH
#include <tzfile.h>
#endif
#if	MSDOS
#include <sys/timeb.h>
#endif

#if	!MSDOS && !defined (NOTZFILEH) \
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static long NEAR char2long __P_((CONST u_char *));
static int NEAR tmcmp __P_((CONST struct tm *, CONST struct tm *));
# ifdef	DEP_ORIGSTREAM
static int NEAR Xfseek __P_((XFILE *, off_t, int));
# else
# define	Xfseek		fseek
# endif
#endif	/* !MSDOS && !NOTZFILEH && !USEMKTIME && !USETIMELOCAL */
static int NEAR getmaxday __P_((int, int));
#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long NEAR gettimezone __P_((CONST struct tm *, time_t));
#endif


time_t Xtime(tp)
time_t *tp;
{
	time_t t;
#if	MSDOS
	struct timeb buffer;

	VOID_C ftime(&buffer);
	t = (time_t)(buffer.time);
#else
	struct timeval t_val;
	struct timezone tz;

	VOID_C Xgettimeofday(&t_val, &tz);
	t = (time_t)(t_val.tv_sec);
#endif
	if (tp) *tp = t;

	return(t);
}

#if	!MSDOS && !defined (NOTZFILEH) \
&& !defined (USEMKTIME) && !defined (USETIMELOCAL)
static long NEAR char2long(s)
CONST u_char *s;
{
	return((long)((u_long)(s[3])
		| ((u_long)(s[2]) << (BITSPERBYTE * 1))
		| ((u_long)(s[1]) << (BITSPERBYTE * 2))
		| ((u_long)(s[0]) << (BITSPERBYTE * 3))));
}

static int NEAR tmcmp(tm1, tm2)
CONST struct tm *tm1, *tm2;
{
	if (tm1 -> tm_year != tm2 -> tm_year)
		return (tm1 -> tm_year - tm2 -> tm_year);
	if (tm1 -> tm_mon != tm2 -> tm_mon)
		return (tm1 -> tm_mon - tm2 -> tm_mon);
	if (tm1 -> tm_mday != tm2 -> tm_mday)
		return (tm1 -> tm_mday - tm2 -> tm_mday);
	if (tm1 -> tm_hour != tm2 -> tm_hour)
		return (tm1 -> tm_hour - tm2 -> tm_hour);
	if (tm1 -> tm_min != tm2 -> tm_min)
		return (tm1 -> tm_min - tm2 -> tm_min);

	return (tm1 -> tm_sec - tm2 -> tm_sec);
}

# ifdef	DEP_ORIGSTREAM
static int NEAR Xfseek(fp, offset, whence)
XFILE *fp;
off_t offset;
int whence;
{
	if (!fp || (fp -> flags & XF_ERROR)) return(-1);
	if ((fp -> flags & XF_WRITTEN) && Xfflush(fp) == EOF) return(-1);
	offset = Xlseek(fp -> fd, offset, whence);
	fp -> ptr = fp -> count = (ALLOC_T)0;
	fp -> flags &= ~XF_EOF;

	return((offset < (off_t)0) ? -1 : 0);
}
# endif	/* DEP_ORIGSTREAM */
#endif	/* !MSDOS && !NOTZFILEH && !USEMKTIME && !USETIMELOCAL */

static int NEAR getmaxday(mon, year)
int mon, year;
{
	int mday;

	switch (mon) {
		case 2:
			mday = 28;
			if (!(year % 4) && ((year % 100) || !(year % 400)))
				mday++;
			break;
		case 4:
		case 6:
		case 9:
		case 11:
			mday = 30;
			break;
		default:
			mday = 31;
			break;
	}

	return(mday);
}

time_t Xtimegm(tm)
CONST struct tm *tm;
{
	time_t t;
	int i, y;

	if (tm -> tm_year < 0) return((time_t)-1);
	y = (tm -> tm_year < 1900) ? tm -> tm_year + 1900 : tm -> tm_year;
	if (tm -> tm_mon < 0 || tm -> tm_mon > 11) return((time_t)-1);
	if (tm -> tm_mday < 1
	|| tm -> tm_mday > getmaxday(tm -> tm_mon + 1, y))
		return((time_t)-1);
	if (tm -> tm_hour < 0 || tm -> tm_hour > 23) return((time_t)-1);
	if (tm -> tm_min < 0 || tm -> tm_min > 59) return((time_t)-1);
	if (tm -> tm_sec < 0 || tm -> tm_sec > 60) return((time_t)-1);

	t = ((long)y - 1970) * 365;
	t += ((y - 1 - 1968) / 4)
		- ((y - 1 - 1900) / 100)
		+ ((y - 1 - 1600) / 400);
	for (i = 1; i < tm -> tm_mon + 1; i++) t += getmaxday(i, y);
	t += tm -> tm_mday - 1;
	t *= 60L * 60L * 24L;
	t += ((long)(tm -> tm_hour) * 60L + tm -> tm_min) * 60L + tm -> tm_sec;

	return(t);
}

#if	!defined (USEMKTIME) && !defined (USETIMELOCAL)
static long NEAR gettimezone(tm, t)
CONST struct tm *tm;
time_t t;
{
# if	MSDOS
	struct timeb buffer;

	ftime(&buffer);
	return((long)(buffer.timezone) * 60L);
# else	/* !MSDOS */
#  ifndef	NOTZFILEH
	struct tzhead head;
	XFILE *fp;
	time_t tmp;
	long i, leap, nleap, ntime, ntype, nchar;
	char *cp, buf[MAXPATHLEN];
	u_char c;
#  endif
	struct tm tmbuf;
	long tz;

	memcpy((char *)&tmbuf, (char *)tm, sizeof(struct tm));

#  ifdef	NOTMGMTOFF
	tz = (long)t - (long)Xtimegm(localtime(&t));
#  else
	tz = -(localtime(&t) -> tm_gmtoff);
#  endif

#  ifndef	NOTZFILEH
	cp = getenv("TZ");
	if (!cp || !*cp) cp = TZDEFAULT;
	if (cp[0] == _SC_) Xstrcpy(buf, cp);
	else strcatdelim2(buf, TZDIR, cp);
	if (!(fp = Xfopen(buf, "rb"))) return(tz);
	if (Xfread((char *)&head, sizeof(head), fp) != sizeof(head)) {
		VOID_C Xfclose(fp);
		return(tz);
	}
#   ifdef	USELEAPCNT
	nleap = char2long(head.tzh_leapcnt);
#   else
	nleap = char2long(head.tzh_timecnt - 4);
#   endif
	ntime = char2long(head.tzh_timecnt);
	ntype = char2long(head.tzh_typecnt);
	nchar = char2long(head.tzh_charcnt);

	for (i = 0; i < ntime; i++) {
		if (Xfread(buf, 4, fp) != 4) {
			VOID_C Xfclose(fp);
			return(tz);
		}
		tmp = char2long(buf);
		if (tmcmp(&tmbuf, localtime(&tmp)) < 0) break;
	}
	if (i > 0) {
		i--;
		i *= (int)sizeof(char);
		i += (int)sizeof(struct tzhead) + ntime * 4 * sizeof(char);
		if (Xfseek(fp, i, 0) < 0 || Xfread((char *)&c, 1, fp) != 1) {
			VOID_C Xfclose(fp);
			return(tz);
		}
		i = c;
	}
	i *= (4 + 1 + 1) * sizeof(char);
	i += (int)sizeof(struct tzhead) + ntime * (4 + 1) * sizeof(char);
	if (Xfseek(fp, i, 0) < 0 || Xfread(buf, 4, fp) != 4) {
		VOID_C Xfclose(fp);
		return(tz);
	}
	tmp = char2long(buf);
	tz = -tmp;

	i = (int)sizeof(struct tzhead) + ntime * (4 + 1) * sizeof(char)
		+ ntype * (4 + 1 + 1) * sizeof(char)
		+ nchar * sizeof(char);
	if (Xfseek(fp, i, 0) < 0) {
		VOID_C Xfclose(fp);
		return(tz);
	}
	leap = 0;
	for (i = 0; i < nleap; i++) {
		if (Xfread(buf, 4, fp) != 4) {
			VOID_C Xfclose(fp);
			return(tz);
		}
		tmp = char2long(buf);
		if (tmcmp(&tmbuf, localtime(&tmp)) <= 0) break;
		if (Xfread(buf, 4, fp) != 4) {
			VOID_C Xfclose(fp);
			return(tz);
		}
		leap = char2long(buf);
	}

	tz += leap;
	VOID_C Xfclose(fp);
#  endif	/* !NOTZFILEH */

	return(tz);
# endif	/* !MSDOS */
}
#endif	/* !USEMKTIME && !USETIMELOCAL */

time_t Xtimelocal(tm)
struct tm *tm;
{
#ifdef	USEMKTIME
	tm -> tm_isdst = -1;
	return(mktime(tm));
#else	/* !USEMKTIME */
# ifdef	USETIMELOCAL
	return(timelocal(tm));
# else	/* !USETIMELOCAL */
	time_t t;

	t = Xtimegm(tm);
	if (t == (time_t)-1) return(t);
	t += gettimezone(tm, t);

	return(t);
# endif	/* !USETIMELOCAL */
#endif	/* !USEMKTIME */
}
