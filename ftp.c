/*
 *	ftp.c
 *
 *	FTP accesing in RFC959
 */

#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "string.h"
#include "malloc.h"
#include "time.h"
#include "sysemu.h"
#include "pathname.h"
#include "termio.h"
#include "parse.h"
#include "lsparse.h"
#include "socket.h"
#include "auth.h"
#include "urldisk.h"

#ifndef	NOFTPH
#include <arpa/ftp.h>
#endif

#define	FTP_QUIT		0
#define	FTP_USER		1
#define	FTP_PASS		2
#define	FTP_CWD			3
#define	FTP_PWD			4
#define	FTP_TYPE		5
#define	FTP_LIST		6
#define	FTP_NLST		7
#define	FTP_PORT		8
#define	FTP_PASV		9
#define	FTP_MDTM		10
#define	FTP_CHMOD		11
#define	FTP_DELE		12
#define	FTP_RNFR		13
#define	FTP_RNTO		14
#define	FTP_RETR		15
#define	FTP_STOR		16
#define	FTP_MKD			17
#define	FTP_RMD			18
#define	FTP_FEAT		19
#define	FTP_ABOR		20
#define	FTP_SIZE		21
#define	FTP_ASCII		"A"
#define	FTP_IMAGE		"I"

#ifndef	PRELIM
#define	PRELIM			1
#endif
#ifndef	COMPLETE
#define	COMPLETE		2
#endif
#ifndef	CONTINUE
#define	CONTINUE		3
#endif
#ifndef	TRANSIENT
#define	TRANSIENT		4
#endif
#ifndef	ERROR
#define	ERROR			5
#endif

typedef struct _ftpcmd_t {
	int id;
	CONST char *cmd[2];
	int argc;
	int flags;
} ftpcmd_t;

#define	FFL_INT			0001
#define	FFL_COMMA		0002
#define	FFL_LIST		0004

#ifdef	DEP_FTPPATH

static VOID NEAR vftplog __P_((CONST char *, va_list));
static VOID NEAR ftplog __P_((CONST char *, ...));
static char *NEAR ftprecv __P_((XFILE *));
static int NEAR vftpsend __P_((XFILE *, int, CONST char *, va_list));
static int NEAR ftppasv __P_((int));
static int NEAR ftpdata __P_((int));
static int NEAR sendrequest __P_((int, int, CONST char *));
static int NEAR ftpopenport __P_((int, int, CONST char *, CONST char *));
static char *ftpfgets __P_((VOID_P));
static int NEAR _ftprecvlist __P_((int, CONST char *, namelist **));
static int NEAR recvpwd __P_((int, char *, ALLOC_T));
static int NEAR ftpfeat __P_((int));
static int NEAR ftpmdtm __P_((int, CONST char *, namelist *, int, int));
static int NEAR ftpsize __P_((int, CONST char *, namelist *, int, int));

char *ftpaddress = NULL;
char *ftpproxy = NULL;
char *ftplogfile = NULL;

static char *form_ftp[] = {
	"%a %l %u %g %s %m %d %{yt} %*f",
	"%a %l %u %g %B, %b %m %d %{yt} %*f",
	"%a %l %u %s %m %d %{yt} %*f",
	"%a %l %u %B, %b %m %d %{yt} %*f",
	"%m-%d-%y %t %{s/<DIR>/} %*f",
	"%s %/DIR/ %m-%d-%y %t %*f",
	"%s %m-%d-%y %t %*f",
	NULL
};
static char *form_nlst[] = {
	"%*f",
	NULL
};
static char *ign_ftp[] = {
	"total *",
	NULL
};
static lsparse_t ftpformat = {
	NULL, NULL, form_ftp, ign_ftp, NULL, 0, 0, LF_NOTRAVERSE
};
static CONST ftpcmd_t ftpcmdlist[] = {
	{FTP_QUIT, {"QUIT", NULL}, 0, 0},
	{FTP_USER, {"USER", NULL}, 1, 0},
	{FTP_PASS, {"PASS", NULL}, 1, 0},
	{FTP_CWD, {"CWD", "XCWD"}, 1, 0},
	{FTP_PWD, {"PWD", "XPWD"}, 0, 0},
	{FTP_TYPE, {"TYPE", NULL}, 1, 0},
	{FTP_LIST, {"LIST -la", "LIST"}, 1, FFL_LIST},
	{FTP_NLST, {"NLST -a", "NLST"}, 1, FFL_LIST},
	{FTP_PORT, {"PORT", NULL}, 6, FFL_INT | FFL_COMMA},
	{FTP_PASV, {"PASV", NULL}, 0, 0},
	{FTP_MDTM, {"MDTM", NULL}, 1, 0},
	{FTP_CHMOD, {"SITE CHMOD", NULL}, 2, 0},
	{FTP_DELE, {"DELE", NULL}, 1, 0},
	{FTP_RNFR, {"RNFR", NULL}, 1, 0},
	{FTP_RNTO, {"RNTO", NULL}, 1, 0},
	{FTP_RETR, {"RETR", NULL}, 1, 0},
	{FTP_STOR, {"STOR", NULL}, 1, 0},
	{FTP_MKD, {"MKD", "XMKD"}, 1, 0},
	{FTP_RMD, {"RMD", "XRMD"}, 1, 0},
	{FTP_FEAT, {"FEAT", NULL}, 0, 0},
	{FTP_ABOR, {"ABOR", NULL}, 0, 0},
	{FTP_SIZE, {"SIZE", NULL}, 1, 0},
};
#define	FTPCMDLISTSIZ		arraysize(ftpcmdlist)


static VOID NEAR vftplog(fmt, args)
CONST char *fmt;
va_list args;
{
	XFILE *fp;

	if (!ftplogfile || !isrootdir(ftplogfile)) return;
	if (!(fp = Xfopen(ftplogfile, "a"))) return;
	if (logheader) {
		Xfputs(logheader, fp);
		Xfree(logheader);
		logheader = NULL;
	}
	VOID_C Xvfprintf(fp, fmt, args);
	VOID_C Xfclose(fp);
}

#ifdef	USESTDARGH
/*VARARGS1*/
static VOID NEAR ftplog(CONST char *fmt, ...)
#else
/*VARARGS1*/
static VOID NEAR ftplog(fmt, va_alist)
CONST char *fmt;
va_dcl
#endif
{
	va_list args;

	VA_START(args, fmt);
	vftplog(fmt, args);
	va_end(args);
}

static char *NEAR ftprecv(fp)
XFILE *fp;
{
	char *buf;

	buf = Xfgets(fp);
	if (!buf) return(NULL);
	ftplog("<-- \"%s\"\n", buf);

	return(buf);
}

static int NEAR vftpsend(fp, cmd, fmt, args)
XFILE *fp;
int cmd;
CONST char *fmt;
va_list args;
{
	u_char oob[3];
	char buf[URLMAXCMDLINE + 1];
	int n;

	n = Xsnprintf(buf, sizeof(buf), "--> \"%s\"\n", fmt);
	if (n < 0) /*EMPTY*/;
	else if (cmd == FTP_PASS) ftplog(buf, "????");
	else vftplog(buf, args);

	if (cmd == FTP_ABOR) {
		oob[0] = TELNET_IAC;
		oob[1] = TELNET_IP;
		oob[2] = TELNET_IAC;
		n = socksendoob(Xfileno(fp), oob, sizeof(oob));
		if (n >= 0) VOID_C Xfputc(TELNET_DM, fp);
	}

	n = Xvfprintf(fp, fmt, args);
	if (n >= 0) n = fputnl(fp);

	return(n);
}

int ftpgetreply(fp, sp)
XFILE *fp;
char **sp;
{
	char *cp, *buf;
	ALLOC_T len, size;
	int i, n, code;

	if (!(buf = ftprecv(fp))) return(-1);
	for (i = n = 0; i < 3; i++) {
		if (!Xisdigit(buf[i])) break;
		n *= 10;
		n += buf[i] - '0';
	}
	code = n;

	if (i < 3) {
		Xfree(buf);
		return(seterrno(EINVAL));
	}
	if (buf[3] == '-') {
		size = strlen(buf);
		for (;;) {
			if (!(cp = ftprecv(fp))) {
				Xfree(buf);
				return(-1);
			}
			for (i = n = 0; i < 3; i++) {
				if (!Xisdigit(cp[i])) break;
				n *= 10;
				n += cp[i] - '0';
			}
			if (i < 3 || cp[3] != ' ') n = -1;

			len = strlen(cp);
			buf = Xrealloc(buf, size + 1 + len + 1);
			buf[size++] = '\n';
			memcpy(&(buf[size]), cp, len + 1);
			size += len;
			Xfree(cp);
			if (n == code) break;
		}
	}
	if (sp) *sp = buf;
	else Xfree(buf);

	return(code);
}

int ftpseterrno(n)
int n;
{
	if (n < 0) return(-1);

	switch (n / 100) {
		case COMPLETE:
			n = 0;
			break;
		case PRELIM:
			n = seterrno(EACCES);
			break;
		case CONTINUE:
			n = seterrno(EACCES);
			break;
		case TRANSIENT:
			n = seterrno(EIO);
			break;
		default:
			switch (n) {
				case 550:
					n = seterrno(ENOENT);
					break;
				default:
					n = seterrno(EINVAL);
					break;
			}
			break;
	}

	return(n);
}

int vftpcommand(uh, sp, cmd, args)
int uh;
char **sp;
int cmd;
va_list args;
{
	char *cp, buf[URLMAXCMDLINE + 1];
	ALLOC_T size;
	int i, j, n, c, delim;

	if (sp) *sp = NULL;
	if (cmd < 0 || cmd >= FTPCMDLISTSIZ) return(seterrno(EINVAL));

	n = 0;
	for (i = 0; i < 2; i++) {
		if (!(ftpcmdlist[cmd].cmd[i])) break;
		if (i || !(ftpcmdlist[cmd].flags & FFL_LIST)) /*EMPTY*/;
		else if (urlhostlist[uh].options & UOP_NOLISTOPT) continue;

		/* Some FTP proxy cannot allow option arguments */
		if (urlhostlist[uh].flags & UFL_PROXIED) {
			cp = Xstrchr(ftpcmdlist[cmd].cmd[i], ' ');
			if (cp && *skipspace(++cp) == '-') continue;
		}

		cp = buf;
		size = sizeof(buf);
		n = Xsnprintf(cp, size, "%s", ftpcmdlist[cmd].cmd[i]);
		if (n < 0) break;

		delim = ' ';
		c = (ftpcmdlist[cmd].flags & FFL_INT) ? 'd' : 's';
		for (j = 0; j < ftpcmdlist[cmd].argc; j++) {
			cp += n;
			size -= n;
			n = Xsnprintf(cp, size, "%c%%%c", delim, c);
			if (n < 0) break;
			if (ftpcmdlist[cmd].flags & FFL_COMMA) delim = ',';
		}
		if (n < 0) break;
		n = vftpsend(urlhostlist[uh].fp, cmd, buf, args);
		if (n < 0) break;

		if (cmd == FTP_QUIT)
			Xsettimeout(urlhostlist[uh].fp, URLENDTIMEOUT);
		n = urlgetreply(uh, sp);
		Xsettimeout(urlhostlist[uh].fp, urltimeout);
		if (n == 500) continue;
		if (n != 421 || cmd == FTP_USER || cmd == FTP_PASS) break;

		if (urlreconnect(uh) < 0) break;
		if (ftplogin(uh) < 0) {
			safefclose(urlhostlist[uh].fp);
			urlhostlist[uh].fp = NULL;
			break;
		}
		i--;
	}

	return(n);
}

int ftpquit(uh)
int uh;
{
	if (urlhostlist[uh].flags & UFL_LOCKED) return(0);

	return(urlcommand(uh, NULL, FTP_QUIT));
}

int ftpabort(uh)
int uh;
{
	return(urlcommand(uh, NULL, FTP_ABOR));
}

int ftplogin(uh)
int uh;
{
	CONST char *pass;
	char buf[URLMAXCMDLINE + 1];
	int n, anon, flags;

	flags = (UGP_USER | UGP_ANON);
	if (urlhostlist[uh].flags & UFL_PROXIED) {
		flags |= UGP_HOST;
		urlhostlist[uh].options |= UOP_NOFEAT;
	}
	if (urlgenpath(uh, buf, sizeof(buf), flags) < 0) return(-1);

	n = urlcommand(uh, NULL, FTP_USER, buf);
	if (n / 100 == CONTINUE) {
		anon = 0;
		if ((pass = urlhostlist[uh].host.pass)) /*EMPTY*/;
		else if (urlhostlist[uh].host.user) pass = nullstr;
		else {
			anon++;
			pass = (ftpaddress && *ftpaddress)
				? ftpaddress : FTPANONPASS;
		}

		n = urlcommand(uh, NULL, FTP_PASS, pass);
		if (n == 530) {
			if (anon) VOID_C Xfprintf(Xstderr,
				"%s: Invalid address.\r\n", pass);
			else VOID_C Xfprintf(Xstderr,
				"%s: Login incorrect.\r\n", buf);
			Xfree(urlhostlist[uh].host.pass);
			urlhostlist[uh].host.pass = NULL;
		}
	}
	if (ftpseterrno(n) < 0) return(-1);
	authentry(&(urlhostlist[uh].host), urlhostlist[uh].type);
	VOID_C ftpfeat(uh);

	return(0);
}

static int NEAR ftppasv(uh)
int uh;
{
	u_char addr[4], port[2];
	char *cp, *buf, host[4 * 4];
	int n;

	n = urlcommand(uh, &buf, FTP_PASV);
	if (ftpseterrno(n) < 0) {
		Xfree(buf);
		return(-1);
	}
	if ((cp = Xstrchr(buf, '(')))
		cp = Xsscanf(cp, "(%Cu,%Cu,%Cu,%Cu,%Cu,%Cu)",
			&(addr[0]), &(addr[1]), &(addr[2]), &(addr[3]),
			&(port[0]), &(port[1]));
	Xfree(buf);
	if (!cp) return(seterrno(EINVAL));
	n = Xsnprintf(host, sizeof(host),
		"%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
	if (n < 0) return(seterrno(EINVAL));
	n = (port[0] << 8 | port[1]);

	return(sockconnect(host, n, urltimeout, SCK_THROUGHPUT));
}

static int NEAR ftpdata(uh)
int uh;
{
	u_char addr[4];
	char *cp, host[SCK_ADDRSIZE + 1];
	int n, s, port, opt;

	if (!(urlhostlist[uh].options & UOP_NOPASV)) return(ftppasv(uh));

	s = Xfileno(urlhostlist[uh].fp);
	if (getsockinfo(s, host, sizeof(host), &port, 0) < 0) return(-1);
	if (urlhostlist[uh].options & UOP_NOPORT)
		opt = (SCK_LOWDELAY | SCK_REUSEADDR);
	else {
		opt = SCK_LOWDELAY;
		port = 0;
	}
	if ((s = sockbind(host, port, opt)) < 0) return(-1);
	if (urlhostlist[uh].options & UOP_NOPORT) return(s);

	if (getsockinfo(s, host, sizeof(host), &port, 0) < 0) {
		safeclose(s);
		return(-1);
	}
	cp = Xsscanf(host, "%Cu.%Cu.%Cu.%Cu%$",
		&(addr[0]), &(addr[1]), &(addr[2]), &(addr[3]));
	if (!cp) return(seterrno(EINVAL));

	n = urlcommand(uh, NULL, FTP_PORT,
		addr[0], addr[1], addr[2], addr[3],
		(port >> 8) & 0xff, port & 0xff);
	if (ftpseterrno(n) < 0) {
		safeclose(s);
		return(-1);
	}

	return(s);
}

static int NEAR sendrequest(uh, cmd, path)
int uh, cmd;
CONST char *path;
{
	int n;

	if ((n = urlcommand(uh, NULL, cmd, path)) < 0) return(-1);
	if (n / 100 == PRELIM) return(1);
	if (n != 425) return(seterrno(EINVAL));
	if ((urlhostlist[uh].flags & UFL_RETRYPORT)
	&& (urlhostlist[uh].flags & UFL_RETRYPASV))
		return(seterrno(EINVAL));

	if (!(urlhostlist[uh].options & UOP_NOPASV)
	|| (urlhostlist[uh].flags & UFL_RETRYPORT)) {
		urlhostlist[uh].flags |= UFL_RETRYPASV;
		urlhostlist[uh].options ^= UOP_NOPASV;
	}
	else {
		urlhostlist[uh].flags |= UFL_RETRYPORT;
		urlhostlist[uh].options ^= UOP_NOPORT;
	}

	return(0);
}

static int NEAR ftpopenport(uh, cmd, path, type)
int uh, cmd;
CONST char *path, *type;
{
	int n, fd;

	n = urlcommand(uh, NULL, FTP_TYPE, type);
	if (ftpseterrno(n) < 0) return(-1);

	for (;;) {
		if ((fd = ftpdata(uh)) < 0) return(-1);
		if ((n = sendrequest(uh, cmd, path)) > 0) break;
		if (n < 0) {
			safeclose(fd);
			return(-1);
		}
	}

	if (urlhostlist[uh].options & UOP_NOPASV)
		fd = sockaccept(fd, SCK_THROUGHPUT);

	return(fd);
}

static char *ftpfgets(vp)
VOID_P vp;
{
	char *buf;

	buf = Xfgets((XFILE *)vp);
	if (!buf) return(NULL);
	ftplog("    \"%s\"\n", buf);

	return(buf);
}

static int NEAR _ftprecvlist(uh, path, listp)
int uh;
CONST char *path;
namelist **listp;
{
	XFILE *fp;
	int n, fd, cmd, max;

	if (urlhostlist[uh].options & UOP_USENLST) {
		cmd = FTP_NLST;
		ftpformat.format = form_nlst;
		ftpformat.flags |= LF_BASENAME;
	}
	else {
		cmd = FTP_LIST;
		ftpformat.format = form_ftp;
		ftpformat.flags &= ~LF_BASENAME;
	}

	if ((fd = ftpopenport(uh, cmd, path, FTP_ASCII)) < 0) return(-1);
	if (!(fp = Xfdopen(fd, "r"))) {
		safeclose(fd);
		return(-1);
	}
	Xsettimeout(fp, urltimeout);
	Xsetflags(fp, XF_CRNL | XF_NONBLOCK);

	*listp = NULL;
	ftplog("<-- (data)\n");
	max = lsparse(fp, &ftpformat, listp, ftpfgets);
	safefclose(fp);
	if (max < 0) return(-1);

	n = urlgetreply(uh, NULL);
	if (ftpseterrno(n) < 0) {
		freelist(*listp, max);
		return(-1);
	}

	return(max);
}

int ftprecvlist(uh, path, listp)
int uh;
CONST char *path;
namelist **listp;
{
	int n;

	n = _ftprecvlist(uh, path, listp);
	if (n < 0 || (urlhostlist[uh].flags & UFL_FIXLISTOPT)) return(n);
	if (n > 1) {
		urlhostlist[uh].flags |= UFL_FIXLISTOPT;
		return(n);
	}
	urlhostlist[uh].options ^= UOP_NOLISTOPT;
	n = _ftprecvlist(uh, path, listp);
	urlhostlist[uh].options ^= UOP_NOLISTOPT;
	if (n > 1) {
		urlhostlist[uh].options ^= UOP_NOLISTOPT;
		urlhostlist[uh].flags |= UFL_FIXLISTOPT;
	}

	return(n);
}

static int NEAR recvpwd(uh, path, size)
int uh;
char *path;
ALLOC_T size;
{
	char *buf;
	int i, j, n, quote;

	if ((n = urlcommand(uh, &buf, FTP_PWD)) < 0) return(-1);
	if (n != 257) {
		Xfree(buf);
		return(seterrno(EACCES));
	}

	for (i = 4; Xisblank(buf[i]); i++) /*EMPTY*/;
	quote = '\0';
	for (j = 0; buf[i]; i++) {
		if (buf[i] == quote) {
			if (buf[i + 1] != quote) {
				quote = '\0';
				continue;
			}
			i++;
		}
		else if (quote) /*EMPTY*/;
		else if (Xisblank(buf[i])) break;
		else if (buf[i] == '"') {
			quote = buf[i];
			continue;
		}

		if (j >= size - 1) break;
		path[j++] = buf[i];
	}
	path[j] = '\0';
	Xfree(buf);

	return(0);
}

static int NEAR ftpfeat(uh)
int uh;
{
	char *cp, *next, *buf;
	int n, mdtm, size;

	if (urlhostlist[uh].options & UOP_NOFEAT) return(0);

	n = urlcommand(uh, &buf, FTP_FEAT);
	if (ftpseterrno(n) < 0) {
		if (n == 500 || n == 502)
			urlhostlist[uh].options |= UOP_NOFEAT;
		Xfree(buf);
		return(-1);
	}

	next = NULL;
	mdtm = size = 0;
	for (cp = buf; cp && *cp; cp = next) {
		while (Xisblank(*cp)) cp++;
		if (!(next = Xstrchr(cp, '\n'))) n = strlen(cp);
		else {
			n = next - cp;
			*(next++) = '\0';
		}
		if (!Xstrcasecmp(cp, "MDTM")) mdtm++;
		if (!Xstrcasecmp(cp, "SIZE")) size++;
	}
	if (!mdtm) urlhostlist[uh].options |= UOP_NOMDTM;
	if (!size) urlhostlist[uh].options |= UOP_NOSIZE;
	Xfree(buf);

	return(0);
}

static int NEAR ftpmdtm(uh, path, namep, st, ent)
int uh;
CONST char *path;
namelist *namep;
int st, ent;
{
	namelist *tmp;
	struct tm tm;
	char *cp, *buf;
	time_t t;
	int n, year, mon, day, hour, min, sec;

	if (urlhostlist[uh].options & UOP_NOMDTM) return(0);
	if (ismark(namep)) return(0);
	tmp = NULL;
	if (st >= 0 && st < maxurlstat
	&& ent >= 0 && ent < urlstatlist[st].max)
		tmp = &(urlstatlist[st].list[ent]);

	n = urlcommand(uh, &buf, FTP_MDTM, path);
	if (n >= 0 && tmp) tmp -> tmpflags |= F_ISMRK;
	if (ftpseterrno(n) < 0) {
		Xfree(buf);
		if (n == 500 || n == 502)
			urlhostlist[uh].options |= UOP_NOMDTM;
		if (n == 550) {
			if (!isdir(namep)) todirlist(namep, (u_int)-1);
			if (tmp && !isdir(tmp)) todirlist(tmp, (u_int)-1);
			return(0);
		}
		return(-1);
	}
	if ((cp = Xstrchr(buf, ' '))) {
		cp = skipspace(&(cp[1]));
		cp = Xsscanf(cp, "%04u%02u%02u%02u%02u%02u",
			&year, &mon, &day, &hour, &min, &sec);
	}
	Xfree(buf);
	if (!cp) return(seterrno(EINVAL));
	tm.tm_year = year - 1900;
	tm.tm_mon = mon - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;

	t = Xtimegm(&tm);
	if (t == (time_t)-1) return(seterrno(EINVAL));
	namep -> st_mtim = t;
	if (tmp) tmp -> st_mtim = t;

	return(0);
}

static int NEAR ftpsize(uh, path, namep, st, ent)
int uh;
CONST char *path;
namelist *namep;
int st, ent;
{
	namelist *tmp;
	char *cp, *buf;
	off_t size;
	int n;

	if (urlhostlist[uh].options & UOP_NOSIZE) return(0);
	if (wasmark(namep)) return(0);
	tmp = NULL;
	if (st >= 0 && st < maxurlstat
	&& ent >= 0 && ent < urlstatlist[st].max)
		tmp = &(urlstatlist[st].list[ent]);

	n = urlcommand(uh, &buf, FTP_SIZE, path);
	if (n >= 0 && tmp) tmp -> tmpflags |= F_WSMRK;
	if (ftpseterrno(n) < 0) {
		Xfree(buf);
		if (n == 500 || n == 502)
			urlhostlist[uh].options |= UOP_NOSIZE;
		if (n == 550) {
			if (!isdir(namep)) todirlist(namep, (u_int)-1);
			if (tmp && !isdir(tmp)) todirlist(tmp, (u_int)-1);
			return(0);
		}
		return(-1);
	}
	if ((cp = Xstrchr(buf, ' '))) {
		cp = skipspace(&(cp[1]));
		cp = Xsscanf(cp, "%qu", &size);
	}
	Xfree(buf);
	if (!cp) return(seterrno(EINVAL));

	namep -> st_size = size;
	if (tmp) tmp -> st_size = size;

	return(0);
}

int ftprecvstatus(uh, path, namep, n, ent)
int uh;
CONST char *path;
namelist *namep;
int n, ent;
{
	if (ftpmdtm(uh, path, namep, n, ent) < 0) return(-1);
	if (!(urlhostlist[uh].options & UOP_USENLST)) /*EMPTY*/;
	else if (ftpsize(uh, path, namep, n, ent) < 0) return(-1);

	return(0);
}

int ftpchdir(uh, path)
int uh;
CONST char *path;
{
	return(ftpseterrno(urlcommand(uh, NULL, FTP_CWD, path)));
}

char *ftpgetcwd(uh, path, size)
int uh;
char *path;
ALLOC_T size;
{
	char *cp;
	int n;

	cp = path;
	n = urlgenpath(uh, cp, size, UGP_SCHEME | UGP_USER | UGP_HOST);
	if (n < 0) return(NULL);
	cp += n;
	size -= n;
	if (recvpwd(uh, cp, size) < 0) return(NULL);

	return(path);
}

int ftpchmod(uh, path, mode)
int uh;
CONST char *path;
int mode;
{
	char buf[MAXLONGWIDTH + 1];
	int n;

	mode &= 0777;
	if (Xsnprintf(buf, sizeof(buf), "%03o", mode) < 0) return(-1);
	n = urlcommand(uh, NULL, FTP_CHMOD, buf, path);

	return(ftpseterrno(n));
}

int ftpunlink(uh, path)
int uh;
CONST char *path;
{
	return(ftpseterrno(urlcommand(uh, NULL, FTP_DELE, path)));
}

int ftprename(uh, from, to)
int uh;
CONST char *from, *to;
{
	int n;

	n = urlcommand(uh, NULL, FTP_RNFR, from);
	if (n / 100 != CONTINUE) {
		if (n >= 0) errno = ENOENT;
		return(-1);
	}

	return(ftpseterrno(urlcommand(uh, NULL, FTP_RNTO, to)));
}

int ftpopen(uh, path, flags)
int uh;
CONST char *path;
int flags;
{
	int cmd;

	switch (flags & O_ACCMODE) {
		case O_RDONLY:
			cmd = FTP_RETR;
			break;
		case O_WRONLY:
			cmd = FTP_STOR;
			break;
		default:
			return(seterrno(EACCES));
/*NOTREACHED*/
			break;
	}

	return(ftpopenport(uh, cmd, path, FTP_IMAGE));
}

/*ARGSUSED*/
int ftpclose(uh, fd)
int uh, fd;
{
	return(ftpseterrno(urlgetreply(uh, NULL)));
}

/*ARGSUSED*/
int ftpfstat(uh, stp)
int uh;
struct stat *stp;
{
	return(seterrno(ENOENT));
}

/*ARGSUSED*/
int ftpread(uh, fd, buf, nbytes)
int uh, fd;
char *buf;
int nbytes;
{
	return(read(fd, buf, nbytes));
}

/*ARGSUSED*/
int ftpwrite(uh, fd, buf, nbytes)
int uh, fd;
CONST char *buf;
int nbytes;
{
	return(write(fd, buf, nbytes));
}

int ftpmkdir(uh, path)
int uh;
CONST char *path;
{
	return(ftpseterrno(urlcommand(uh, NULL, FTP_MKD, path)));
}

int ftprmdir(uh, path)
int uh;
CONST char *path;
{
	return(ftpseterrno(urlcommand(uh, NULL, FTP_RMD, path)));
}
#endif	/* DEP_FTPPATH */
