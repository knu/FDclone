/*
 *	html.h
 *
 *	definitions & function prototype declarations for "html.c"
 */

#include "stream.h"

typedef struct _htmlstat_t {
	XFILE *fp;
	char *buf;
	char *path;
	ALLOC_T ptr;
	ALLOC_T len;
	ALLOC_T max;
	int charset;
	int flags;
} htmlstat_t;

#define	HTML_LVL		00007
#define	HTML_NONE		00000
#define	HTML_HTML		00001
#define	HTML_HEAD		00002
#define	HTML_BODY		00003
#define	HTML_PRE		00004
#define	HTML_TAG		00010
#define	HTML_CLOSE		00020
#define	HTML_COMMENT		00040
#define	HTML_BREAK		00100
#define	HTML_NEWLINE		00200
#define	HTML_ANCHOR		00400
#define	htmllvl(h)		(((hp) -> flags) & HTML_LVL)

extern VOID htmllog __P_((CONST char *, ...));
extern int getcharset __P_((char *CONST *));
extern VOID htmlinit __P_((htmlstat_t *, XFILE *, CONST char *));
extern VOID htmlfree __P_((htmlstat_t *));
extern char *htmlfgets __P_((htmlstat_t *));
