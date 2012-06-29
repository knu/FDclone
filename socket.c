/*
 *	socket.c
 *
 *	socket module
 */

#include "headers.h"
#include "depend.h"
#include "printf.h"
#include "kctype.h"
#include "typesize.h"
#include "malloc.h"
#include "sysemu.h"
#include "termio.h"
#include "socket.h"

#ifdef	USEINSYSTMH
#include <netinet/in_systm.h>
#endif
#if	!MSDOS
#include <netinet/in.h>
# ifndef	MINIX
# include <netinet/ip.h>
# endif
#include <arpa/inet.h>
#include <netdb.h>
#endif	/* !MSDOS */

#define	PROTONAME_TCP		"tcp"

#ifdef	DEP_SOCKET

static int NEAR getport __P_((int, struct sockaddr_in *));
static int NEAR getaddr __P_((CONST char *, struct in_addr **));
static int NEAR setsockopt2 __P_((int, int, int, int));
static int NEAR preconnect __P_((int, int));
static int NEAR preaccept __P_((int, struct sockaddr_in *, sock_len_t, int));
# ifdef	SO_ERROR
static int NEAR checkprogress __P_((int, long));
# endif
static int NEAR sureconnect __P_((int, struct sockaddr_in *, int));


static int NEAR getport(port, sinp)
int port;
struct sockaddr_in *sinp;
{
	if (port < 0 || port > MAXUTYPE(u_short)) return(seterrno(EINVAL));
	sinp -> sin_port = htons(port);

	return(0);
}

static int NEAR getaddr(s, listp)
CONST char *s;
struct in_addr **listp;
{
	struct sockaddr_in sin;
	struct hostent *hp;
	struct in_addr *new;
	char *cp, **addr;
	int n, naddr;

	if (!s || !*s) return(seterrno(EINVAL));

# ifdef	USEINETATON
	n = (inet_aton(s, &(sin.sin_addr))) ? 0 : -1;
# else	/* !USEINETATON */
#  ifdef	USEINETPTON
	n = (inet_pton(AF_INET, s, &(sin.sin_addr)) > 0) ? 0: -1;
#  else
	sin.sin_addr.s_addr = inet_addr(s);
	n = (sin.sin_addr.s_addr != INADDR_NONE) ? 0 : -1;
#  endif
# endif	/* !USEINETATON */

	if (n >= 0) {
		naddr = 1;
		cp = (char *)&(sin.sin_addr);
		addr = &cp;
	}
	else if (!(hp = gethostbyname(s)) || hp -> h_addrtype != AF_INET)
		return(seterrno(ENOENT));
	else {
# ifdef	NOHADDRLIST
		naddr = 1;
		addr = &(hp -> h_addr);
# else
		for (n = 0; hp -> h_addr_list[n]; n++) /*EMPTY*/;
		naddr = n;
		addr = hp -> h_addr_list;
# endif
	}

	new = (struct in_addr *)Xmalloc(naddr * sizeof(struct in_addr));
	for (n = 0; n < naddr; n++)
		memcpy((char *)&(new[n]), addr[n], sizeof(struct in_addr));
	*listp = new;

	return(n);
}

int cmpsockaddr(s1, s2)
CONST char *s1, *s2;
{
	struct in_addr *addr1, *addr2;
	int i, j, n, n1, n2;

	if (!strcmp(s1, s2)) return(0);
	if ((n1 = getaddr(s1, &addr1)) < 0) return(-1);
	if ((n2 = getaddr(s2, &addr2)) < 0) return(-1);

	n = -1;
	for (i = 0; i < n1; i++) {
		for (j = 0; j < n2; j++) {
			n = memcmp((char *)&(addr1[i]), (char *)&(addr2[j]),
				sizeof(struct in_addr));
			if (!n) break;
		}
		if (!n) break;
	}
	Xfree(addr1);
	Xfree(addr2);

	return(n);
}

int issocket(s)
int s;
{
# if	defined (SOL_SOCKET) && defined (SO_TYPE)
	sock_len_t len;
	int n;

	len = sizeof(n);
	return(getsockopt(s, SOL_SOCKET, SO_TYPE, &n, &len));
# else
	return(-1);
# endif
}

int getsockinfo(s, buf, size, portp, peer)
int s;
char *buf;
ALLOC_T size;
int *portp, peer;
{
	struct sockaddr *sp;
	struct sockaddr_in sin;
	sock_len_t len;
	u_char *cp;
	int n;

	len = sizeof(sin);
	sp = (struct sockaddr *)&sin;
	n = (peer) ? getpeername(s, sp, &len) : getsockname(s, sp, &len);
	if (n < 0) return(-1);
	cp = (u_char *)&(sin.sin_addr.s_addr);
	n = Xsnprintf(buf, size, "%u.%u.%u.%u", cp[0], cp[1], cp[2], cp[3]);
	if (n < 0) return(-1);
	if (portp) *portp = ntohs(sin.sin_port);

	return(0);
}

static int NEAR setsockopt2(s, level, name, val)
int s, level, name, val;
{
	return(setsockopt(s, level, name, &val, sizeof(val)));
}

int chgsockopt(s, opt)
int s, opt;
{
# if	defined (IPPROTO_IP) && defined (IP_TOS)
	int tos;
# endif
	int n;

	n = 0;
# if	defined (IPPROTO_IP) && defined (IP_TOS)
	switch (opt & SCK_TOSTYPE) {
#  ifdef	IPTOS_LOWDELAY
		case SCK_LOWDELAY:
			tos = IPTOS_LOWDELAY;
			break;
#  endif
#  ifdef	IPTOS_THROUGHPUT
		case SCK_THROUGHPUT:
			tos = IPTOS_THROUGHPUT;
			break;
#  endif
#  ifdef	IPTOS_RELIABILITY
		case SCK_RELIABILITY:
			tos = IPTOS_RELIABILITY;
			break;
#  endif
#  ifdef	IPTOS_MINCOST
		case SCK_MINCOST:
			tos = IPTOS_MINCOST;
			break;
#  endif
		case SCK_NORMAL:
			tos = 0;
			break;
		default:
			tos = -1;
			break;
	}
	if (tos >= 0) {
		if (setsockopt2(s, IPPROTO_IP, IP_TOS, tos) < 0) n = -1;
	}
# endif	/* IPPROTO_IP && IP_TOS */
# ifdef	SOL_SOCKET
#  ifdef	SO_KEEPALIVE
	if (opt & SCK_KEEPALIVE) {
		if (setsockopt2(s, SOL_SOCKET, SO_KEEPALIVE, 1) < 0) n = -1;
	}
#  endif
	if (opt & SCK_REUSEADDR) {
#  ifdef	SO_REUSEADDR
		if (setsockopt2(s, SOL_SOCKET, SO_REUSEADDR, 1) < 0) n = -1;
#  endif
#  ifdef	SO_REUSEPORT
		if (setsockopt2(s, SOL_SOCKET, SO_REUSEPORT, 1) < 0) n = -1;
#  endif
	}
# endif	/* SOL_SOCKET */

	return(n);
}

static int NEAR preconnect(s, opt)
int s, opt;
{
	int n;

	closeonexec(s);
	if (chgsockopt(s, opt) < 0) return(-1);
	if ((n = fcntl(s, F_GETFL, 0)) < 0) return(-1);
	if (fcntl(s, F_SETFL, n | O_NONBLOCK) < 0) return(-1);

	return(n);
}

static int NEAR preaccept(s, sinp, len, opt)
int s;
struct sockaddr_in *sinp;
sock_len_t len;
int opt;
{
	if (chgsockopt(s, opt) < 0) return(-1);
	if (bind(s, (struct sockaddr *)sinp, len) < 0) return(-1);
	if (listen(s, SCK_BACKLOG) < 0) return(-1);

	return(0);
}

# ifdef	SO_ERROR
static int NEAR checkprogress(s, usec)
int s;
long usec;
{
	struct timeval tv;
	sock_len_t len;
	int n;

	tv.tv_sec = 0L;
	tv.tv_usec = usec;
	while (tv.tv_usec >= 1000000L) {
		tv.tv_sec++;
		tv.tv_usec -= 1000000L;
	}

	n = sureselect(1, &s, NULL, &tv, SEL_WRITE | SEL_NOINTR);
	if (n <= 0) return(n);

	len = sizeof(n);
	n = 0;
	if (getsockopt(s, SOL_SOCKET, SO_ERROR, &n, &len) < 0) return(-1);
	if (n) {
		errno = n;
		return(-1);
	}

	return(1);
}
# endif	/* SO_ERROR */

static int NEAR sureconnect(s, sinp, timeout)
int s;
struct sockaddr_in *sinp;
int timeout;
{
	int i, n;

	n = 0;
	for (i = 0; timeout <= 0 || i < timeout * 10; i++) {
		n = connect(s,
			(struct sockaddr *)sinp, sizeof(struct sockaddr_in));
		if (n >= 0) break;
# ifdef	EAGAIN
		else if (errno == EAGAIN) continue;
# endif
# ifdef	EALREADY
		else if (errno == EALREADY) /*EMPTY*/;
# endif
# ifdef	EINPROGRESS
		else if (errno == EINPROGRESS) /*EMPTY*/;
# endif
		else return(-1);

# ifdef	SO_ERROR
		n = checkprogress(s, 1000000L / 10);
		if (n < 0) return(-1);
		if (n) break;
# endif
	}
	if (timeout > 0 && i >= timeout * 10) return(seterrno(ETIMEDOUT));

	return(0);
}

int sockconnect(host, port, timeout, opt)
CONST char *host;
int port, timeout, opt;
{
	struct sockaddr_in sin;
	struct in_addr *addr;
	int i, n, s, naddr;

	memset((char *)&sin, 0, sizeof(sin));
	if (getport(port, &sin) < 0) return(-1);
	if (!(sin.sin_port)) return(seterrno(EINVAL));
	if ((naddr = getaddr(host, &addr)) < 0) return(-1);
	sin.sin_family = AF_INET;

	s = -1;
	opt |= SO_KEEPALIVE;
	for (i = 0; i < naddr; i++) {
		if ((s = socket(sin.sin_family, SOCK_STREAM, 0)) < 0) break;
		if ((n = preconnect(s, opt)) < 0) {
			safeclose(s);
			s = -1;
			break;
		}
		memcpy((char *)&(sin.sin_addr.s_addr), (char *)&(addr[i]),
			sizeof(struct in_addr));
		if (sureconnect(s, &sin, timeout) >= 0) {
			VOID_C fcntl(s, F_SETFL, n & ~O_NONBLOCK);
			break;
		}
		safeclose(s);
		s = -1;
	}
	Xfree(addr);

	return(s);
}

int sockbind(host, port, opt)
CONST char *host;
int port, opt;
{
	struct sockaddr_in sin;
	struct in_addr *addr;
	int s;

	memset((char *)&sin, 0, sizeof(sin));
	if (getport(port, &sin) < 0) return(-1);
	if (host) {
		if (getaddr(host, &addr) < 0) return(-1);
		memcpy((char *)&(sin.sin_addr.s_addr), (char *)&(addr[0]),
			sizeof(struct in_addr));
		Xfree(addr);
	}
	else sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	if ((s = socket(sin.sin_family, SOCK_STREAM, 0)) < 0) return(-1);

	opt |= SO_KEEPALIVE;
	if (preaccept(s, &sin, sizeof(sin), opt) < 0) {
		safeclose(s);
		return(-1);
	}

	return(s);
}

int sockaccept(s, opt)
int s, opt;
{
	struct sockaddr_in sin;
	sock_len_t len;
	int fd;

	len = sizeof(sin);
	fd = accept(s, (struct sockaddr *)&sin, &len);
	safeclose(s);
	if (fd < 0) return(-1);
	if (chgsockopt(fd, opt) < 0) {
		safeclose(fd);
		return(-1);
	}
	closeonexec(fd);

	return(fd);
}

int socksendoob(s, buf, size)
int s;
CONST VOID_P buf;
ALLOC_T size;
{
	int n;

# ifdef	NOSENDFLAGS
	n = seterrno(EIO);
# else
	n = send(s, buf, size, MSG_OOB);
	if (n >= 0 && (ALLOC_T)n < size) n = seterrno(EIO);
# endif

	return(n);
}
#endif	/* DEP_SOCKET */
