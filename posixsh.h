/*
 *	posixsh.h
 *
 *	function prototype declarations for "posixsh.c"
 */

#include "stream.h"

#ifndef	NOJOB
extern int gettermio __P_((p_id_t, int));
extern VOID dispjob __P_((int, XFILE *));
extern int statjob __P_((int));
extern VOID freejob __P_((int));
extern int searchjob __P_((p_id_t, int *));
extern int getjob __P_((CONST char *));
extern int stackjob __P_((p_id_t, int, int, syntaxtree *));
extern int stoppedjob __P_((p_id_t));
extern VOID killjob __P_((VOID_A));
extern VOID checkjob __P_((XFILE *));
#endif	/* !NOJOB */
extern char *evalposixsubst __P_((CONST char *, int *));
#if	!MSDOS
extern VOID replacemailpath __P_((CONST char *, int));
extern VOID checkmail __P_((int));
#endif
#ifndef	NOALIAS
extern int addalias __P_((char *, char *));
extern int deletealias __P_((CONST char *));
extern VOID freealias __P_((shaliastable *));
extern int checkalias __P_((syntaxtree *, char *, int, int));
#endif
#ifndef	NOJOB
extern int posixjobs __P_((syntaxtree *));
extern int posixfg __P_((syntaxtree *));
extern int posixbg __P_((syntaxtree *));
#endif
#ifndef	NOALIAS
extern int posixalias __P_((syntaxtree *));
extern int posixunalias __P_((syntaxtree *));
#endif
extern int posixkill __P_((syntaxtree *));
extern int posixtest __P_((syntaxtree *));
#ifndef	NOPOSIXUTIL
extern int posixcommand __P_((syntaxtree *));
extern int posixgetopts __P_((syntaxtree *));
#endif

#ifndef	NOJOB
extern jobtable *joblist;
extern int maxjobs;
#endif
#if	!MSDOS
extern int mailcheck;
#endif
#ifndef	NOALIAS
extern shaliastable *shellalias;
#endif
#ifndef	NOPOSIXUTIL
extern int posixoptind;
#endif
