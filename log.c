/*
 *	log.c
 *
 *	system logging
 */

#include "fd.h"

#ifndef	_NOLOGGING

#include <fcntl.h>
#include "func.h"

#ifndef	NOSYSLOG
#include <syslog.h>
#endif

#ifndef	O_TEXT
#define	O_TEXT		0
#endif
#ifndef	LOG_PID
#define	LOG_PID		0
#endif
#ifndef	LOG_ERR
#define	LOG_ERR		3
#endif
#ifndef	LOG_INFO
#define	LOG_INFO	6
#endif

extern char *progname;

static int NEAR openlogfile __P_((VOID_A));
static VOID NEAR writelog __P_((int, int, char *, int));

char *logfile = NULL;
int logsize = 0;
#ifndef	NOSYSLOG
int usesyslog = 0;
#endif
int loglevel = 0;
#ifndef	NOUID
int rootloglevel = 0;
#endif

static char *logfname = NULL;
#ifndef	NOSYSLOG
static int syslogged = 0;
#endif


static int NEAR openlogfile(VOID_A)
{
	struct stat st;
	char *cp, *top, path[MAXPATHLEN];
	ALLOC_T size;
	int fd;

	cp = logfname;
	if (logfname) {
		if (!*logfname) return(-1);
	}
	else {
		if (!logfile || !*logfile) return(-1);

		logfname = "";
		top = logfile;
#if	MSDOS
		if (_dospath(top)) top += 2;
#endif
		if (*top == _SC_) cp = logfile;
		else {
			if (!(cp = gethomedir())) return(-1);
			strcatdelim2(path, cp, top);
			if (!*path) return(-1);
			cp = strdup2(path);
		}
	}

	size = (ALLOC_T)logsize * (ALLOC_T)1024;
	if (size > 0 && stat(cp, &st) >= 0 && st.st_size > size) {
		snprintf2(path, sizeof(path), "%s.old", cp);
		if (rename(cp, path) < 0) unlink(cp);
	}

	fd = open(cp, O_TEXT | O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd < 0) {
		free(cp);
		return(-1);
	}

#ifndef	NOFLOCK
	if (lockfile(fd, LCK_WRITE) < 0) {
		close(fd);
		return(-1);
	}
#endif

	logfname = cp;
	return(fd);
}

VOID logclose(VOID_A)
{
	if (logfname && *logfname) free(logfname);
	logfname = "";
#ifndef	NOSYSLOG
	if (syslogged > 0) closelog();
	syslogged = -1;
#endif
}

/*ARGSUSED*/
static VOID NEAR writelog(lvl, p, buf, len)
int lvl, p;
char *buf;
int len;
{
	struct tm *tm;
	char hbuf[MAXLOGLEN + 1];
	time_t t;
	u_char uc;
	int n, fd;

#ifndef	NOUID
	if (!getuid()) {
		n = rootloglevel;
		lvl--;
	}
	else
#endif
	n = loglevel;
	if (!n || n < lvl) return;

	if ((fd = openlogfile()) < 0) {
#ifndef	NOSYSLOG
		if (usesyslog) /*EMPTY*/;
		else
#endif
		return;
	}

	if (fd >= 0) {
		t = time(NULL);
		tm = localtime(&t);
#ifdef	NOUID
		n = snprintf2(hbuf, sizeof(hbuf),
			"%04u/%02u/%02u %02u:%02u:%02u %s[%d]:\n ",
			tm -> tm_year + 1900, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min, tm -> tm_sec,
			progname, getpid());
#else
		n = snprintf2(hbuf, sizeof(hbuf),
			"%04u/%02u/%02u %02u:%02u:%02u uid=%d %s[%d]:\n ",
			tm -> tm_year + 1900, tm -> tm_mon + 1, tm -> tm_mday,
			tm -> tm_hour, tm -> tm_min, tm -> tm_sec,
			getuid(), progname, getpid());
#endif
		write(fd, hbuf, n);
		write(fd, buf, len);
		uc = '\n';
		write(fd, &uc, sizeof(uc));
		lockfile(fd, LCK_UNLOCK);
		close(fd);
	}
#ifndef	NOSYSLOG
	if (usesyslog && syslogged >= 0) {
		if (!syslogged) {
			syslogged++;
# ifdef	LOG_USER
			openlog(progname, LOG_PID, LOG_USER);
# else
			openlog(progname, LOG_PID);
# endif
		}
		syslog(p, "%s", buf);
	}
#endif	/* !NOSYSLOG */
}

#ifdef	USESTDARGH
/*VARARGS2*/
VOID logsyscall(int lvl, int val, CONST char *fmt, ...)
#else
/*VARARGS2*/
VOID logsyscall(lvl, val, fmt, va_alist)
int lvl, val;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char buf[MAXLOGLEN + 1];
	int n, len, duperrno;

	duperrno = errno;
	if (val >= 0) lvl++;
	VA_START(args, fmt);
	len = vsnprintf2(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len >= 0) {
		if (val >= 0) n = snprintf2(&(buf[len]), sizeof(buf) - len,
			" succeeded");
		else n = snprintf2(&(buf[len]), sizeof(buf) - len,
			" -- FAILED -- (%k)", strerror2(duperrno));
		if (n < 0) buf[len] = '\0';
		else len += n;
		writelog(lvl, (val < 0) ? LOG_ERR : LOG_INFO, buf, len);
	}
	errno = duperrno;
}

#ifdef	USESTDARGH
/*VARARGS2*/
VOID logmessage(int lvl, CONST char *fmt, ...)
#else
/*VARARGS2*/
VOID logmessage(lvl, fmt, va_alist)
int lvl, val;
CONST char *fmt;
va_dcl
#endif
{
	va_list args;
	char buf[MAXLOGLEN + 1];
	int len, duperrno;

	duperrno = errno;
	VA_START(args, fmt);
	len = vsnprintf2(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len >= 0) writelog(lvl, LOG_INFO, buf, len);
	errno = duperrno;
}
#endif	/* !_NOLOGGING */
