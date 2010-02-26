/*
 *	socket.h
 *
 *	definitions & function prototype declarations for "socket.c"
 */

#ifndef	__SOCKET_H_
#define	__SOCKET_H_

#include "depend.h"

#if	!MSDOS
#include <sys/socket.h>
#endif

#ifdef	USESOCKLEN
#define	sock_len_t		socklen_t
#else
typedef int			sock_len_t;
#endif

#ifndef	SHUT_RD
#define	SHUT_RD			0
#endif
#ifndef	SHUT_WR
#define	SHUT_WR			1
#endif
#ifndef	SHUT_RDWR
#define	SHUT_RDWR		2
#endif

#define	SCK_BACKLOG		5
#define	SCK_ADDRSIZE		(3 * 4 + 3)

#define	TELNET_IAC		255
#define	TELNET_DONT		254
#define	TELNET_DO		253
#define	TELNET_WONT		252
#define	TELNET_WILL		251
#define	TELNET_IP		244
#define	TELNET_DM		242

typedef struct _sockdirdesc {
	int dd_id;
	int dd_fd;
	long dd_loc;
	long dd_size;
} sockDIR;

#define	SID_IFURLDRIVE		(-2)

#define	SCK_TOSTYPE		0007
#define	SCK_LOWDELAY		0001
#define	SCK_THROUGHPUT		0002
#define	SCK_RELIABILITY		0003
#define	SCK_MINCOST		0004
#define	SCK_NORMAL		0005
#define	SCK_KEEPALIVE		0010
#define	SCK_REUSEADDR		0020

extern int cmpsockport __P_((CONST char *, CONST char *));
extern int cmpsockaddr __P_((CONST char *, CONST char *));
extern int issocket __P_((int));
extern int getsockinfo __P_((int, char *, ALLOC_T, int *, int));
extern int chgsockopt __P_((int, int));
extern int sockconnect __P_((CONST char *, int, int, int));
extern int sockbind __P_((CONST char *, int, int));
extern int sockreply __P_((int, int, u_char *, ALLOC_T, int));
extern int sockaccept __P_((int, int));
extern int socksendoob __P_((int, CONST VOID_P, ALLOC_T));

#endif	/* !__SOCKET_H_ */
