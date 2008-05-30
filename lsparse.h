/*
 *	lsparse.h
 *
 *	definitions & function prototype declarations for "lsparse.c"
 */

#ifndef	__LSPARSE_H_
#define	__LSPARSE_H_

#include "typesize.h"
#include "namelist.h"

#if	defined (FD) && (FD < 2) && !defined (OLDPARSE)
#define	OLDPARSE
#endif

#define	MAXLSPARSEFIELD		9
#define	MAXLSPARSESEP		3
typedef struct _lsparse_t {
	char *ext;
	char *comm;
#ifndef	OLDPARSE
	char **format;
	char **lignore;
	char **lerror;
#endif
	u_char topskip;
	u_char bottomskip;
#ifdef	OLDPARSE
	u_char field[MAXLSPARSEFIELD];
	u_char delim[MAXLSPARSEFIELD];
	u_char width[MAXLSPARSEFIELD];
	u_char sep[MAXLSPARSESEP];
	u_char lines;
#endif
	u_char flags;
} lsparse_t;

#define	F_MODE			0
#define	F_UID			1
#define	F_GID			2
#define	F_SIZE			3
#define	F_YEAR			4
#define	F_MON			5
#define	F_DAY			6
#define	F_TIME			7
#define	F_NAME			8
#define	LF_IGNORECASE		0001
#define	LF_DIRLOOP		0002
#define	LF_DIRNOPREP		0004
#define	LF_FILELOOP		0010
#define	LF_FILENOPREP		0020
#define	SKP_NONE		MAXUTYPE(u_char)
#define	FLD_NONE		MAXUTYPE(u_char)
#define	SEP_NONE		MAXUTYPE(u_char)

extern u_int getfmode __P_((int));
extern int getfsymbol __P_((u_int));
#ifdef	NOUID
extern int logical_access __P_((u_int));
#define	logical_access2(s)	logical_access((u_int)((s) -> st_mode))
#else
extern int logical_access __P_((u_int, uid_t, gid_t));
#define	logical_access2(s)	logical_access((u_int)((s) -> st_mode), \
					(s) -> st_uid, (s) -> st_gid)
#endif
#ifdef	DEP_LSPARSE
extern VOID initlist __P_((namelist *, CONST char *));
extern VOID todirlist __P_((namelist *, u_int));
extern int dirmatchlen __P_((CONST char *, CONST char *));
extern int parsefilelist __P_((VOID_P, CONST lsparse_t *,
		namelist *, int *, char *(*)__P_((VOID_P))));
extern namelist *addlist __P_((namelist *, int, int *));
extern VOID freelist __P_((namelist *, int));
extern int lsparse __P_((VOID_P, CONST lsparse_t *,
		namelist **, char *(*)__P_((VOID_P))));
extern int strptime2 __P_((CONST char *, CONST char *, struct tm *, int *));
#endif

extern int (*lsintrfunc)__P_((VOID_A));

#endif	/* !__LSPARSE_H_ */
