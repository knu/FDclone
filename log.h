/*
 *	log.h
 *
 *	definitions & function prototype declarations for "log.c"
 */

#include "depend.h"

#define	_LOG_WARNING_		0
#define	_LOG_NOTICE_		1
#define	_LOG_INFO_		2
#define	_LOG_DEBUG_		3

#ifdef	DEP_LOGGING
#define	LOG0(l, n, f)		logsyscall(l, n, f)
#define	LOG1(l, n, f, a1)	logsyscall(l, n, f, a1)
#define	LOG2(l, n, f, a1, a2)	logsyscall(l, n, f, a1, a2)
#define	LOG3(l, n, f, a1, a2, a3) \
				logsyscall(l, n, f, a1, a2, a3)
extern VOID logclose __P_((VOID_A));
extern VOID logsyscall __P_((int, int, CONST char *, ...));
extern VOID logmessage __P_((int, CONST char *, ...));
#else	/* !DEP_LOGGING */
#define	LOG0(l, n, f)
#define	LOG1(l, n, f, a1)
#define	LOG2(l, n, f, a1, a2)
#define	LOG3(l, n, f, a1, a2, a3)
#endif	/* !DEP_LOGGING */
