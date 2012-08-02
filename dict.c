/*
 *	dict.c
 *
 *	dictionary accessing module
 */

#include "fd.h"
#include "termio.h"
#include "roman.h"
#include "hinsi.h"
#include "func.h"

#if	FD >= 3
#include "parse.h"
#include "kconv.h"
#endif

#define	DICTTBL			"fd-dict.tbl"
#define	MAXHINSI		16
#define	FREQMAGIC		0x4446
#define	FREQVERSION		0x0100
#define	dictoffset(n)		((off_t)4 + (off_t)(n) * 4)
#define	freqoffset(n)		((off_t)2 + 2 + 4 + (off_t)(n) * 4)
#define	getword(s, n)		(((u_short)((s)[(n) + 1]) << 8) | (s)[n])
#define	getdword(s, n)		(((u_long)getword(s, (n) + 2) << 16) \
				| getword(s, n))
#define	skread(f, o, s, n)	(Xlseek(f, o, L_SET) >= (off_t)0 \
				&& sureread(f, s, n) == n)

typedef struct _jisbuf {
	u_short *buf;
	u_char max;
} jisbuf;

typedef struct _kanjitable {
	jisbuf k;
	u_char len;
	u_char match;
	u_char kmatch;
	u_short freq;
	u_short hinsi[MAXHINSI];
	long ofs;
} kanjitable;

typedef struct _freq_t {
	jisbuf kana;
	jisbuf kanji;
	u_short freq;
} freq_t;

#ifdef	DEP_IME

static off_t NEAR dictlseek __P_((int, off_t, int));
static int NEAR fgetbyte __P_((u_char *, int));
static int NEAR fgetword __P_((u_short *, int));
static int NEAR fgetdword __P_((long *, int));
static int NEAR fgetstring __P_((jisbuf *, int));
# ifndef	DEP_EMBEDDICTTBL
static off_t NEAR fgetoffset __P_((long, int));
# endif
static int NEAR fgetjisbuf __P_((jisbuf *, long, int));
static int NEAR fgethinsi __P_((u_short [], int));
static int NEAR fseekfreq __P_((long, int));
static int NEAR fgetfreqbuf __P_((freq_t *, int));
static int NEAR fputfreqbuf __P_((freq_t *, int));
static int NEAR _fchkhinsi __P_((int, CONST u_short [], int));
static int NEAR fchkhinsi __P_((u_short [], CONST u_short [],
		u_short [], CONST u_short [], int));
# ifndef	DEP_EMBEDDICTTBL
static int NEAR opendicttbl __P_((CONST char *));
static u_char *NEAR gendicttbl __P_((int, off_t, ALLOC_T, int));
static VOID NEAR readdicttable __P_((int));
# endif
static int NEAR fputbyte __P_((int, int));
static int NEAR fputword __P_((u_int, int));
static int NEAR fputdword __P_((long, int));
static int NEAR fputstring __P_((CONST jisbuf *, int));
static lockbuf_t *NEAR openfreqtbl __P_((CONST char *, int));
static int NEAR findfreq __P_((freq_t *, long *));
static int NEAR copyfreq __P_((int, int));
static int NEAR copyuserfreq __P_((freq_t *, int, int));
static int NEAR adduserfreq __P_((CONST char *, freq_t *));
static int NEAR cmpjis __P_((CONST jisbuf *, CONST jisbuf *));
static int cmpdict __P_((CONST VOID_P, CONST VOID_P));
static int cmpfreq __P_((CONST VOID_P, CONST VOID_P));
static long NEAR addkanji __P_((long, kanjitable **, CONST kanjitable *));
static VOID NEAR freekanji __P_((kanjitable *));
static int NEAR fgetdict __P_((kanjitable *, CONST kanjitable *, int));
static long NEAR addkanjilist __P_((long, kanjitable **,
		long, kanjitable *, kanjitable *, int));
static off_t NEAR nextofs __P_((off_t, int));
static int NEAR finddict __P_((jisbuf *, int, long *));
static long NEAR _searchdict __P_((long, kanjitable **, kanjitable *, int));
static long NEAR uniqkanji __P_((long, kanjitable *));
# if	FD >= 3
static VOID NEAR fgetjis __P_((jisbuf *, XFILE *));
static CONST char *parsejis __P_((jisbuf *, CONST char *));
# endif

char *dicttblpath = NULL;
int imebuffer = 0;
short frequmask = (short)0;
# if	FD >= 3
char *freqfile = NULL;
short imelarning = 0;
# else
# define	imelarning	IMELARNING
# endif

static kanjitable *kanjilist = NULL;
static long freqtblent = 0L;
static off_t dictofs = (off_t)0;
# ifdef	DEP_EMBEDDICTTBL
extern CONST u_char dictindexbuf[];
extern CONST u_char dicttblbuf[];
extern CONST u_char hinsiindexbuf[];
extern CONST u_char hinsitblbuf[];
extern long dicttblent;
extern int hinsitblent;
# else
static u_char *dictindexbuf = NULL;
static u_char *dicttblbuf = NULL;
static u_char *hinsiindexbuf = NULL;
static u_char *hinsitblbuf = NULL;
static long dicttblent = 0L;
static int hinsitblent = 0;
static off_t hinsitblofs = (off_t)0;
# endif


static off_t NEAR dictlseek(fd, ofs, whence)
int fd;
off_t ofs;
int whence;
{
	if (fd >= 0) return(Xlseek(fd, ofs, whence));

	switch (whence) {
		case L_SET:
			dictofs = ofs;
			break;
		case L_INCR:
			dictofs += ofs;
			break;
		default:
			break;
	}

	return(dictofs);
}

static int NEAR fgetbyte(cp, fd)
u_char *cp;
int fd;
{
	if (fd >= 0) {
		if ((sureread(fd, cp, 1) != 1)) return(-1);
	}
	else *cp = dicttblbuf[dictofs++];

	return(0);
}

static int NEAR fgetword(wp, fd)
u_short *wp;
int fd;
{
	CONST u_char *cp;
	u_char buf[2];

	if (fd >= 0) {
		if (sureread(fd, buf, sizeof(buf)) != sizeof(buf)) return(-1);
		cp = buf;
	}
	else {
		cp = &(dicttblbuf[dictofs]);
		dictofs += 2;
	}
	*wp = getword(cp, 0);

	return(0);
}

static int NEAR fgetdword(lp, fd)
long *lp;
int fd;
{
	u_char buf[4];

	if (sureread(fd, buf, sizeof(buf)) != sizeof(buf)) return(-1);
	*lp = getdword(buf, 0);

	return(0);
}

static int NEAR fgetstring(jp, fd)
jisbuf *jp;
int fd;
{
	u_short *kbuf;
	u_char c;
	int i;

	jp -> buf = NULL;
	if (fgetbyte(&c, fd) < 0) return(-1);
	jp -> max = c;
	kbuf = (u_short *)Xmalloc(((int)c + 1) * sizeof(u_short));
	for (i = 0; i < (int)c; i++) if (fgetword(&(kbuf[i]), fd) < 0) {
		Xfree(kbuf);
		return(-1);
	}
	kbuf[i] = (u_short)0;
	jp -> buf = kbuf;

	return(0);
}

# ifndef	DEP_EMBEDDICTTBL
static off_t NEAR fgetoffset(n, fd)
long n;
int fd;
{
	u_char buf[4];
	off_t ofs;

	ofs = dictoffset(n);
	if (!skread(fd, ofs, buf, sizeof(buf))) return((off_t)-1);
	ofs = dictoffset(dicttblent + 1) + getdword(buf, 0);

	return(ofs);
}
# endif	/* !DEP_EMBEDDICTTBL */

static int NEAR fgetjisbuf(jp, n, fd)
jisbuf *jp;
long n;
int fd;
{
# ifndef	DEP_EMBEDDICTTBL
	u_char buf[4];
# endif
	CONST u_char *cp;
	off_t ofs;

	ofs = (off_t)n * 4;
# ifndef	DEP_EMBEDDICTTBL
	if (!dictindexbuf) {
		ofs += (off_t)4;
		if (!skread(fd, ofs, buf, sizeof(buf))) return((off_t)-1);
		cp = buf;
	}
	else
# endif
	cp = &(dictindexbuf[ofs]);

	ofs = getdword(cp, 0);

# ifndef	DEP_EMBEDDICTTBL
	if (!dicttblbuf) ofs += dictoffset(dicttblent + 1);
# endif
	if (dictlseek(fd, ofs, L_SET) < (off_t)0) return(-1);

	return(fgetstring(jp, fd));
}

static int NEAR fgethinsi(hinsi, fd)
u_short hinsi[MAXHINSI];
int fd;
{
	u_char c;
	int i, n;

	if (hinsitblent <= 0) n = 0;
	else {
		if (fgetbyte(&c, fd) < 0) return(-1);
		if ((n = c) > MAXHINSI) n = MAXHINSI;
		for (i = 0; i < n; i++)
			if (fgetword(&(hinsi[i]), fd) < 0) return(-1);
	}
	if (n < MAXHINSI) hinsi[n] = MAXUTYPE(u_short);

	return(n);
}

static int NEAR fseekfreq(n, fd)
long n;
int fd;
{
	off_t ofs;
	long l;

	ofs = freqoffset(n);
	if (Xlseek(fd, ofs, L_SET) < (off_t)0 || fgetdword(&l, fd) < 0)
		return(-1);
	ofs = freqoffset(freqtblent + 1) + l;
	if (Xlseek(fd, ofs, L_SET) < (off_t)0) return(-1);

	return(0);
}

static int NEAR fgetfreqbuf(frp, fd)
freq_t *frp;
int fd;
{
	if (fgetstring(&(frp -> kana), fd) < 0) return(-1);
	if (fgetstring(&(frp -> kanji), fd) < 0) {
		Xfree(frp -> kana.buf);
		return(-1);
	}
	if (fgetword(&(frp -> freq), fd) < 0) {
		Xfree(frp -> kana.buf);
		Xfree(frp -> kanji.buf);
		return(-1);
	}

	return(0);
}

static int NEAR fputfreqbuf(frp, fd)
freq_t *frp;
int fd;
{
	if (fputstring(&(frp -> kana), fd) < 0) return(-1);
	if (fputstring(&(frp -> kanji), fd) < 0) return(-1);
	if (fputword(frp -> freq, fd) < 0) return(-1);

	return(0);
}

static int NEAR _fchkhinsi(id, hinsi, fd)
int id;
CONST u_short hinsi[MAXHINSI];
int fd;
{
# ifndef	DEP_EMBEDDICTTBL
	u_char buf[2];
# endif
	CONST u_char *cp;
	u_char *hbuf;
	off_t ofs;
	u_short w;
	int i, j, len;

	if (hinsitblent <= 0) return(0);
	if (id >= hinsitblent) return(MAXUTYPE(u_short));

	ofs = (off_t)id * 2;
# ifndef	DEP_EMBEDDICTTBL
	if (!hinsiindexbuf) {
		ofs += hinsitblofs + 2;
		if (!skread(fd, ofs, buf, sizeof(buf))) return(-1);
		ofs = getword(buf, 0);
	}
	else
# endif
	ofs = getword(hinsiindexbuf, ofs);

# ifndef	DEP_EMBEDDICTTBL
	if (!hinsitblbuf) {
		ofs += hinsitblofs + (off_t)(hinsitblent + 1) * 2 + 2;
		if (!skread(fd, ofs, buf, 2)) return(-1);
		if ((len = getword(buf, 0)) <= 0) return(MAXUTYPE(u_short));
		cp = hbuf = (u_char *)Xmalloc(len * 2);
		if (sureread(fd, hbuf, len * 2) != len * 2) {
			Xfree(hbuf);
			return(-1);
		}
	}
	else
# endif
	{
		cp = &(hinsitblbuf[ofs]);
		len = getword(cp, 0);
		cp += 2;
		hbuf = NULL;
	}

	for (i = 0; i < len; i++, cp += 2) {
		w = getword(cp, 0);
		for (j = 0; j < MAXHINSI; j++) {
			if (hinsi[j] == MAXUTYPE(u_short)) break;
			if (hinsi[j] == w) {
				Xfree(hbuf);
				return(j);
			}
		}
	}
	Xfree(hbuf);

	return(MAXUTYPE(u_short));
}

static int NEAR fchkhinsi(fdest, fsrc, bdest, bsrc, fd)
u_short fdest[MAXHINSI];
CONST u_short fsrc[MAXHINSI];
u_short bdest[MAXHINSI];
CONST u_short bsrc[MAXHINSI];
int fd;
{
	u_short fhit[MAXHINSI], bhit[MAXHINSI];
	int i, j, n;

	if (hinsitblent <= 0 || fsrc[0] == MAXUTYPE(u_short)) {
		if (fdest) memcpy((char *)fdest, (char *)fsrc, sizeof(fhit));
		if (bdest) memcpy((char *)bdest, (char *)bsrc, sizeof(bhit));
		return(0);
	}

	for (i = 0; i < MAXHINSI; i++) fhit[i] = bhit[i] = MAXUTYPE(u_short);
	for (i = j = 0; i < MAXHINSI; i++) {
		if (bsrc[i] == MAXUTYPE(u_short)) break;
		n = _fchkhinsi(bsrc[i], fsrc, fd);
		if (n < 0) return(-1);
		if (n < MAXUTYPE(u_short)) {
			fhit[n] = fsrc[n];
			bhit[i] = bsrc[i];
			j++;
		}
	}
	if (!j) return(-1);

	if (fdest) {
		for (i = j = 0; i < MAXHINSI; i++)
			if (fhit[i] < MAXUTYPE(u_short)) fdest[j++] = fhit[i];
		if (j < MAXHINSI) fdest[j] = MAXUTYPE(u_short);
	}
	if (bdest) {
		for (i = j = 0; i < MAXHINSI; i++)
			if (bhit[i] < MAXUTYPE(u_short)) bdest[j++] = bhit[i];
		if (j < MAXHINSI) bdest[j] = MAXUTYPE(u_short);
	}

	return(0);
}

# ifndef	DEP_EMBEDDICTTBL
static int NEAR opendicttbl(file)
CONST char *file;
{
	static int fd = -2;
	u_char buf[2];
	char path[MAXPATHLEN];

	if (!file) {
		if (fd >= 0) VOID_C Xclose(fd);
		fd = -2;
		return(0);
	}

	if (fd >= -1) return(fd);

	if (!dicttblpath || !*dicttblpath) Xstrcpy(path, file);
	else strcatdelim2(path, dicttblpath, file);

	if ((fd = Xopen(path, O_BINARY | O_RDONLY, 0666)) < 0) fd = -1;
	else if (!dicttblent && fgetdword(&dicttblent, fd) < 0) {
		VOID_C Xclose(fd);
		fd = -1;
	}
	else if (!hinsitblofs
	&& (hinsitblofs = fgetoffset(dicttblent, fd)) < (off_t)0)
		hinsitblent = -1;
	else if (!hinsitblent) {
		if (!skread(fd, hinsitblofs, buf, sizeof(buf)))
			hinsitblent = -1;
		else hinsitblent = getword(buf, 0);
	}

	return(fd);
}

static u_char *NEAR gendicttbl(fd, ofs, size, lvl)
int fd;
off_t ofs;
ALLOC_T size;
int lvl;
{
	u_char *tbl;

	if (ofs != (off_t)-1 && Xlseek(fd, ofs, L_SET) < (off_t)0)
		return(NULL);
	if (!(tbl = (u_char *)malloc(size))) {
		imebuffer = lvl;
		return(NULL);
	}
	if (sureread(fd, tbl, size) != size) {
		Xfree(tbl);
		return(NULL);
	}

	return(tbl);
}

static VOID NEAR readdicttable(fd)
int fd;
{
	u_char *tbl, buf[2];
	ALLOC_T size;
	off_t ofs;

	if (!hinsiindexbuf && hinsitblent > 0) {
		ofs = (off_t)hinsitblofs + 2;
		size = (ALLOC_T)hinsitblent * 2;
		if (!(tbl = gendicttbl(fd, ofs, size, 0))) return;
		hinsiindexbuf = tbl;
	}

	if (imebuffer < 1) return;
	if (!hinsitblbuf && hinsitblent > 0) {
		ofs = (off_t)hinsitblent * 2 + 2;
		if (!skread(fd, hinsitblofs + ofs, buf, sizeof(buf))) return;
		size = getword(buf, 0);
		if (!(tbl = gendicttbl(fd, (off_t)-1, size, 0))) return;
		hinsitblbuf = tbl;
	}

	if (imebuffer < 2) return;
	if (!dictindexbuf && dicttblent > 0) {
		ofs = (off_t)4;
		size = (ALLOC_T)dicttblent * 4;
		if (!(tbl = gendicttbl(fd, ofs, size, 1))) return;
		dictindexbuf = tbl;
	}

	if (imebuffer < 3) return;
	if (!dicttblbuf && dicttblent > 0) {
		ofs = dictoffset(dicttblent + 1);
		size = (ALLOC_T)hinsitblofs - ofs;
		if (!(tbl = gendicttbl(fd, ofs, size, 2))) return;
		dicttblbuf = tbl;
	}
}
# endif	/* !DEP_EMBEDDICTTBL */

VOID discarddicttable(VOID_A)
{
# ifndef	DEP_EMBEDDICTTBL
	Xfree(hinsiindexbuf);
	hinsiindexbuf = NULL;
	Xfree(hinsitblbuf);
	hinsitblbuf = NULL;
	Xfree(dictindexbuf);
	dictindexbuf = NULL;
	Xfree(dicttblbuf);
	dicttblbuf = NULL;
# endif
}

static int NEAR fputbyte(c, fd)
int c, fd;
{
	u_char uc;

	uc = c;
	if (surewrite(fd, &uc, sizeof(uc)) < 0) return(-1);

	return(0);
}

static int NEAR fputword(w, fd)
u_int w;
int fd;
{
	u_char buf[2];

	buf[0] = (w & 0xff);
	buf[1] = ((w >> 8) & 0xff);
	if (surewrite(fd, buf, sizeof(buf)) < 0) return(-1);

	return(0);
}

static int NEAR fputdword(dw, fd)
long dw;
int fd;
{
	u_char buf[4];

	buf[0] = (dw & 0xff);
	buf[1] = ((dw >> 8) & 0xff);
	buf[2] = ((dw >> 16) & 0xff);
	buf[3] = ((dw >> 24) & 0xff);
	if (surewrite(fd, buf, sizeof(buf)) < 0) return(-1);

	return(0);
}

static int NEAR fputstring(jp, fd)
CONST jisbuf *jp;
int fd;
{
	int i;

	if (fputbyte(jp -> max, fd) < 0) return(-1);
	for (i = 0; i < jp -> max; i++)
		if (fputword(jp -> buf[i], fd) < 0) return(-1);

	return(0);
}

static lockbuf_t *NEAR openfreqtbl(file, flags)
CONST char *file;
int flags;
{
	static lockbuf_t *lck = NULL;
	u_short w;

	if (!file) {
		lockclose(lck);
		lck = NULL;
		return(NULL);
	}

	if (lck) return(lck);

	freqtblent = 0L;
	lck = lockopen(file, flags, 0666 & ~frequmask);
	if (!lck || lck -> fd < 0) /*EMPTY*/;
	else if (fgetword(&w, lck -> fd) < 0 || w != FREQMAGIC
	|| fgetword(&w, lck -> fd) < 0 || w != FREQVERSION
	|| fgetdword(&freqtblent, lck -> fd) < 0) {
		lockclose(lck);
		lck = NULL;
	}

	if (!lck) {
		lck = (lockbuf_t *)Xmalloc(sizeof(lockbuf_t));
		lck -> fd = -1;
		lck -> fp = NULL;
		lck -> name = NULL;
		lck -> flags = LCK_INVALID;
	}

	return(lck);
}

static int NEAR findfreq(frp, ofsp)
freq_t *frp;
long *ofsp;
{
# if	FD < 3
	char *freqfile;
# endif
	freq_t tmp;
	lockbuf_t *lck;
	long ofs, min, max;
	int n;

	if (ofsp) *ofsp = -1L;
# if	FD < 3
	freqfile = evalpath(Xstrdup(FREQFILE), 0);
# endif
	if (!freqfile || !*freqfile) return(0);
	lck = openfreqtbl(freqfile, O_BINARY | O_RDONLY);
# if	FD < 3
	Xfree(freqfile);
# endif
	if (lck -> fd < 0) return(0);

	min = -1L;
	max = freqtblent;
	for (;;) {
		ofs = (min + max) / 2;
		if (ofs <= min) {
			if (ofsp) *ofsp = min + 1;
			return(0);
		}
		else if (ofs >= max) {
			if (ofsp) *ofsp = max;
			return(0);
		}
		if (fseekfreq(ofs, lck -> fd) < 0) return(-1);
		if (fgetfreqbuf(&tmp, lck -> fd) < 0) return(-1);

		if (!(n = cmpjis(&(frp -> kana), &(tmp.kana))))
			n = cmpjis(&(frp -> kanji), &(tmp.kanji));
		Xfree(tmp.kana.buf);
		Xfree(tmp.kanji.buf);

		if (n > 0) min = ofs;
		else if (n < 0) max = ofs;
		else break;
	}

	if (ofsp) *ofsp = ofs;
	frp -> freq = tmp.freq;

	return(1);
}

static int NEAR copyfreq(fdin, fdout)
int fdin, fdout;
{
	freq_t tmp;
	int n;

	if (fgetfreqbuf(&tmp, fdin) < 0) return(-1);
	n = (fdout < 0) ? 0 : fputfreqbuf(&tmp, fdout);
	Xfree(tmp.kana.buf);
	Xfree(tmp.kanji.buf);

	return(n);
}

static int NEAR copyuserfreq(frp, fdin, fdout)
freq_t *frp;
int fdin, fdout;
{
	ALLOC_T size;
	long l, tmp, ofs, index, ent, freq;
	int n, skip;

	size = (ALLOC_T)1 + (frp -> kana.max * 2)
		+ 1 + (frp -> kanji.max * 2) + 2;
	skip = 0;
	if (fdin < 0) ent = ofs = 0L;
	else {
		freq = (long)(frp -> freq);
		ent = freqtblent;
		n = findfreq(frp, &ofs);
		if (n < 0) return(-1);
		else if (n) {
			skip++;
			freq += frp -> freq;
			if (freq > MAXUTYPE(u_short)) freq = MAXUTYPE(u_short);
			frp -> freq = freq;
		}
		if (Xlseek(fdin, freqoffset(1), L_SET) < (off_t)0) return(-1);
	}

	if (fputword(FREQMAGIC, fdout) < 0) return(-1);
	if (fputword(FREQVERSION, fdout) < 0) return(-1);
	if (fputdword(ent + 1 - skip, fdout) < 0) return(-1);
	index = 0L;
	if (fputdword(index, fdout) < 0) return(-1);
	for (l = 1L; l <= ofs; l++) {
		if (fgetdword(&index, fdin) < 0) return(-1);
		if (fputdword(index, fdout) < 0) return(-1);
	}
	if (fputdword(index + size, fdout) < 0) return(-1);
	if (skip) {
		if (fgetdword(&tmp, fdin) < 0) return(-1);
		size += (ALLOC_T)(index - tmp);
		l++;
	}
	for (; l <= ent; l++) {
		if (fgetdword(&index, fdin) < 0) return(-1);
		index += size;
		if (fputdword(index, fdout) < 0) return(-1);
	}

	for (l = 0; l < ofs; l++)
		if (copyfreq(fdin, fdout) < 0) return(-1);
	if (skip) {
		if (copyfreq(fdin, -1) < 0) return(-1);
		l++;
	}
	if (fputstring(&(frp -> kana), fdout) < 0) return(-1);
	if (fputstring(&(frp -> kanji), fdout) < 0) return(-1);
	if (fputword(frp -> freq, fdout) < 0) return(-1);
	for (; l < ent; l++)
		if (copyfreq(fdin, fdout) < 0) return(-1);

	return(0);
}

static int NEAR adduserfreq(file, frp)
CONST char *file;
freq_t *frp;
{
	lockbuf_t *lck;
	char path[MAXPATHLEN];
	int n, fdin, fdout;

	VOID_C openfreqtbl(NULL, 0);
	if (!(lck = openfreqtbl(file, O_BINARY | O_RDWR))) return(-1);
	Xstrcpy(path, file);
	if ((fdin = lck -> fd) >= 0)
		fdout = opentmpfile(path, 0666 & ~frequmask);
	else {
		VOID_C openfreqtbl(NULL, 0);
		lck = lockopen(path,
			O_BINARY | O_WRONLY | O_CREAT | O_EXCL,
			0666 & ~frequmask);
		fdout = (lck) ? lck -> fd : -1;
	}

	if (fdout < 0) {
		VOID_C openfreqtbl(NULL, 0);
		if (fdin < 0) lockclose(lck);
		return(-1);
	}

	n = copyuserfreq(frp, fdin, fdout);
	if (fdin < 0) {
# if	!MSDOS && !defined (CYGWIN)
		if (n < 0) VOID_C Xunlink(path);
		lockclose(lck);
# else
		lockclose(lck);
		if (n < 0) VOID_C Xunlink(path);
# endif
	}
	else {
# if	!MSDOS && !defined (CYGWIN)
		if (n >= 0) n = Xrename(path, file);
		VOID_C openfreqtbl(NULL, 0);
		VOID_C Xclose(fdout);
# else	/* MSDOS || CYGWIN */
		VOID_C openfreqtbl(NULL, 0);
		VOID_C Xclose(fdout);
		if (n >= 0) {
#  if	MSDOS
			n = Xrename(path, file);
#  else
			while ((n = Xrename(path, file)) < 0) {
				if (errno != EACCES) break;
				usleep(100000L);
			}
#  endif
		}
# endif	/* MSDOS || CYGWIN */
		if (n < 0) VOID_C Xunlink(path);
	}

	return(n);
}

VOID saveuserfreq(kana, kbuf)
CONST u_short *kana, *kbuf;
{
# if	FD < 3
	char *freqfile;
# endif
	freq_t tmp;
	long argc;

	if (!kanjilist) return;

	for (argc = 0L; kanjilist[argc].k.buf; argc++)
		if (kbuf == kanjilist[argc].k.buf) break;
	if (!kanjilist[argc].k.buf) return;

	tmp.kana.buf = (u_short *)kana;
	tmp.kana.max = kanjilist[argc].match;
	tmp.kanji.buf = (u_short *)kbuf;
	tmp.kanji.max = kanjilist[argc].kmatch;
	tmp.freq = (u_short)1;

# if	FD < 3
	freqfile = evalpath(Xstrdup(FREQFILE), 0);
# endif
	if (!freqfile || !*freqfile) return;
	VOID_C adduserfreq(freqfile, &tmp);
# if	FD < 3
	Xfree(freqfile);
# endif
}

static int cmpjis(jp1, jp2)
CONST jisbuf *jp1;
CONST jisbuf *jp2;
{
	int i;

	for (i = 0; i < jp1 -> max; i++) {
		if (i >= jp2 -> max) return(1);
		if (jp1 -> buf[i] > jp2 -> buf[i]) return(1);
		else if (jp1 -> buf[i] < jp2 -> buf[i]) return(-1);
	}

	return((int)(jp1 -> max) - (int)(jp2 -> max));
}

static int cmpdict(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	kanjitable *kp1, *kp2;

	kp1 = (kanjitable *)vp1;
	kp2 = (kanjitable *)vp2;

	return(cmpjis(&(kp1 -> k), &(kp2 -> k)));
}

static int cmpfreq(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	kanjitable *kp1, *kp2;

	kp1 = (kanjitable *)vp1;
	kp2 = (kanjitable *)vp2;

	if (kp1 -> len < kp2 -> len) return(1);
	else if (kp1 -> len > kp2 -> len) return(-1);
	if (kp1 -> freq < kp2 -> freq) return(1);
	else if (kp1 -> freq > kp2 -> freq) return(-1);
	if (kp1 -> ofs > kp2 -> ofs) return(1);
	else if (kp1 -> ofs < kp2 -> ofs) return(-1);

	return(0);
}

static long NEAR addkanji(argc, argvp, kp)
long argc;
kanjitable **argvp;
CONST kanjitable *kp;
{
	*argvp = (kanjitable *)Xrealloc(*argvp,
		(argc + 2) * sizeof(kanjitable));
	memcpy((char *)&((*argvp)[argc++]), kp, sizeof(kanjitable));
	(*argvp)[argc].k.buf = NULL;

	return(argc);
}

static VOID NEAR freekanji(argv)
kanjitable *argv;
{
	long n;

	if (argv) {
		for (n = 0L; argv[n].k.buf; n++) Xfree(argv[n].k.buf);
		Xfree(argv);
	}
}

static int NEAR fgetdict(kp1, kp2, fd)
kanjitable *kp1;
CONST kanjitable *kp2;
int fd;
{
	freq_t tmp;
	long freq;

	if (fgetstring(&(kp1 -> k), fd) < 0) return(-1);
	if (fgetword(&(kp1 -> freq), fd) < 0
	|| fgethinsi(kp1 -> hinsi, fd) < 0) {
		Xfree(kp1 -> k.buf);
		return(-1);
	}

# if	FD >= 3
	if (imelarning <= 0) return(0);
# endif

	tmp.kana.buf = kp1 -> k.buf;
	tmp.kana.max = kp1 -> k.max;
	tmp.kanji.buf = kp2 -> k.buf;
	tmp.kanji.max = kp2 -> len;
	if (findfreq(&tmp, NULL) <= 0 || tmp.freq <= 0) return(0);
	freq = ((long)(tmp.freq) + imelarning) * imelarning;
	freq += kp1 -> freq;
	if (freq > MAXUTYPE(u_short)) freq = MAXUTYPE(u_short);
	kp1 -> freq = (u_short)freq;

	return(0);
}

static long NEAR addkanjilist(argc, argvp, argc2, argv2, kp, fd)
long argc;
kanjitable **argvp;
long argc2;
kanjitable *argv2, *kp;
int fd;
{
	kanjitable *next, tmp1, tmp2;
	u_short shuutan[MAXHINSI];
	long n;
	int i;

	if (fgetdict(&tmp1, kp, fd) < 0) return(argc);

	tmp2.k.buf = tmp1.k.buf;
	tmp2.k.max = tmp1.k.max;
	tmp2.len = kp -> len;
	tmp2.match = kp -> len;
	tmp2.kmatch = tmp1.k.max;
	tmp2.freq = tmp1.freq;
	tmp2.ofs = kp -> ofs;
	if (hinsitblent <= 0) {
		if (kp -> len < kp -> k.max) {
			tmp2.k.max += kp -> k.max - kp -> len;
			tmp2.k.buf = (u_short *)Xrealloc(tmp2.k.buf,
				(tmp2.k.max + 1) * sizeof(u_short));
			memcpy((char *)&(tmp2.k.buf[tmp1.k.max]),
				(char *)&(kp -> k.buf[kp -> len]),
				(kp -> k.max - kp -> len) * sizeof(u_short));
			tmp2.k.buf[tmp2.k.max] = (u_short)0;
		}
		return(addkanji(argc, argvp, &tmp2));
	}

	i = fchkhinsi(NULL, kp -> hinsi, tmp1.hinsi, tmp1.hinsi, fd);
	if (i >= 0 && kp -> len >= kp -> k.max) {
		shuutan[0] = SH_svkantan;
		shuutan[1] = MAXUTYPE(u_short);
		i = fchkhinsi(tmp2.hinsi, tmp1.hinsi, NULL, shuutan, fd);
		if (i >= 0) return(addkanji(argc, argvp, &tmp2));
	}
	if (i < 0) {
		if (kp -> hinsi[0] == (u_short)HN_SENTOU
		&& kp -> len >= kp -> k.max) {
			for (i = 0; i < MAXHINSI; i++) {
				if (tmp1.hinsi[i] == MAXUTYPE(u_short)) break;
				if (tmp1.hinsi[i] == (u_short)HN_TANKAN)
					return(addkanji(argc, argvp, &tmp2));
			}
		}
		Xfree(tmp1.k.buf);
		return(argc);
	}

	if (!(next = argv2)) {
		tmp2.k.buf = &(kp -> k.buf[kp -> len]);
		tmp2.k.max = kp -> k.max - kp -> len;
		memcpy((char *)(tmp2.hinsi),
			(char *)(tmp1.hinsi), sizeof(tmp2.hinsi));
		for (tmp2.len = kp -> k.max; tmp2.len > (u_char)0; tmp2.len--)
			argc2 = _searchdict(argc2, &argv2, &tmp2, fd);
		if (!argv2) {
			Xfree(tmp1.k.buf);
			return(argc);
		}
	}

	for (n = 0L; n < argc2; n++) {
		if (next) {
			i = fchkhinsi(tmp2.hinsi, tmp1.hinsi,
				NULL, argv2[n].hinsi, fd);
			if (i < 0) continue;
		}

		tmp2.k.max = tmp1.k.max + argv2[n].k.max;
		tmp2.k.buf =
			(u_short *)Xmalloc((tmp2.k.max + 1) * sizeof(u_short));
		memcpy((char *)(tmp2.k.buf), (char *)(tmp1.k.buf),
			tmp1.k.max * sizeof(u_short));
		memcpy((char *)&(tmp2.k.buf[tmp1.k.max]),
			(char *)(argv2[n].k.buf),
			argv2[n].k.max * sizeof(u_short));
		tmp2.k.buf[tmp2.k.max] = (u_short)0;

		if (argv2[n].hinsi[0] >= (u_short)HN_MAX) {
			tmp2.len = kp -> len + argv2[n].len;
			tmp2.freq = tmp1.freq + argv2[n].freq;
		}
		else if (tmp2.hinsi[0] >= (u_short)HN_MAX) {
			tmp2.len = argv2[n].len;
			tmp2.freq = argv2[n].freq;
		}
		else if (kp -> len > argv2[n].len) {
			tmp2.len = kp -> len;
			tmp2.freq = tmp1.freq;
		}
		else if (tmp2.len < argv2[n].len) {
			tmp2.len = argv2[n].len;
			tmp2.freq = argv2[n].freq;
		}
		else {
			tmp2.len = kp -> len;
			tmp2.freq = (tmp1.freq > argv2[n].freq)
				? tmp1.freq : argv2[n].freq;
		}

		argc = addkanji(argc, argvp, &tmp2);
	}

	if (!next) freekanji(argv2);
	Xfree(tmp1.k.buf);

	return(argc);
}

static off_t NEAR nextofs(ofs, fd)
off_t ofs;
int fd;
{
	u_char c;

	if (dictlseek(fd, ofs, L_SET) < (off_t)0) return((off_t)-1);
	if (fgetbyte(&c, fd) < 0) return((off_t)-1);
	ofs = (off_t)c * 2 + 2;
	if ((ofs = dictlseek(fd, ofs, L_INCR)) < (off_t)0) return((off_t)-1);
	if (hinsitblent <= 0) return(ofs);
	if (fgetbyte(&c, fd) < 0) return((off_t)-1);
	ofs = (off_t)c * 2;

	return(dictlseek(fd, ofs, L_INCR));
}

static int NEAR finddict(jp, fd, ofsp)
jisbuf *jp;
int fd;
long *ofsp;
{
	jisbuf tmp;
	long ofs, min, max;
	int n;

	if (ofsp) *ofsp = -1L;

	min = -1L;
	max = dicttblent;
	for (;;) {
		ofs = (min + max) / 2;
		if (ofs <= min) {
			if (ofsp) *ofsp = min + 1;
			return(0);
		}
		else if (ofs >= max) {
			if (ofsp) *ofsp = max;
			return(0);
		}
		if (fgetjisbuf(&tmp, ofs, fd) < 0) return(-1);

		n = cmpjis(jp, &tmp);
		Xfree(tmp.buf);
		if (n > 0) min = ofs;
		else if (n < 0) max = ofs;
		else break;
	}

	if (ofsp) *ofsp = ofs;

	return(1);
}

static long NEAR _searchdict(argc, argvp, kp, fd)
long argc;
kanjitable **argvp, *kp;
int fd;
{
	kanjitable *argv2, tmp;
	off_t curofs;
	long argc2;
	u_char c;
	int n;

	dictofs = (off_t)0;
	tmp.k.buf = kp -> k.buf;
	tmp.k.max = kp -> len;
	if (finddict(&(tmp.k), fd, &(kp -> ofs)) <= 0) return(argc);

	if (fgetbyte(&c, fd) < 0) return(argc);
	if (c <= 1) return(addkanjilist(argc, argvp, 0, NULL, kp, fd));

	curofs = dictlseek(fd, (off_t)0, L_INCR);
	if (curofs < (off_t)0) return(argc);
	argv2 = NULL;
	argc2 = 0L;
	if (hinsitblent > 0 && kp -> len < kp -> k.max) {
		tmp.k.buf = &(kp -> k.buf[kp -> len]);
		tmp.k.max = kp -> k.max - kp -> len;
		tmp.hinsi[0] = MAXUTYPE(u_short);
		for (tmp.len = kp -> k.max; tmp.len > (u_char)0; tmp.len--)
			argc2 = _searchdict(argc2, &argv2, &tmp, fd);
		if (!argv2) return(argc);
	}

	if (dictlseek(fd, curofs, L_SET) >= (off_t)0) {
		for (n = 0; n < c; n++, curofs = nextofs(curofs, fd)) {
			if (curofs < (off_t)0) break;
			argc = addkanjilist(argc, argvp, argc2, argv2, kp, fd);
		}
	}

	freekanji(argv2);

	return(argc);
}

static long NEAR uniqkanji(argc, argv)
long argc;
kanjitable *argv;
{
	long i, j;
	int c;

	for (i = 0L; i < argc - 1; i++) {
		for (j = i + 1L; j < argc; j++) {
			if (argv[i].k.max != argv[j].k.max) break;
			c = memcmp((char *)(argv[i].k.buf),
				(char *)(argv[j].k.buf),
				argv[i].k.max * sizeof(u_short));
			if (c) break;

			Xfree(argv[j].k.buf);
			if (argv[j].len > argv[i].len) {
				argv[i].len = argv[j].len;
				argv[i].freq = argv[j].freq;
			}
			else if (argv[j].len < argv[i].len) continue;
			else if (argv[j].freq > argv[i].freq)
				argv[i].freq = argv[j].freq;
		}
		if (j <= i + 1L) continue;
		memmove((char *)&(argv[i + 1]), (char *)&(argv[j]),
			(argc + 1 - j) * sizeof(kanjitable));
		argc -= --j - i;
	}

	return(argc);
}

VOID freekanjilist(argv)
u_short **argv;
{
	long n, argc;

	if (argv) {
		for (argc = 0L; argv[argc]; argc++) {
			if (kanjilist) {
				for (n = 0L; kanjilist[n].k.buf; n++)
					if (argv[argc] == kanjilist[n].k.buf)
						break;
				if (kanjilist[n].k.buf) continue;
			}
			Xfree(argv[argc]);
		}
		Xfree(argv);
	}
	freekanji(kanjilist);
	kanjilist = NULL;
}

u_short **searchdict(kana, len)
u_short *kana;
int len;
{
	kanjitable tmp;
	u_short **list;
	long n, argc;
	int i, fd;

	freekanjilist(NULL);

	if (!kana) {
# ifndef	DEP_EMBEDDICTTBL
		VOID_C opendicttbl(NULL);
# endif
		VOID_C openfreqtbl(NULL, 0);
		if (imebuffer <= 0) discarddicttable();
		return(NULL);
	}

	if (len > MAXUTYPE(u_char)) len = MAXUTYPE(u_char);
	argc = 0L;
# ifdef	DEP_EMBEDDICTTBL
	fd = -1;
# else
	if (dicttblbuf) fd = -1;
	else if ((fd = opendicttbl(DICTTBL)) < 0) return(NULL);
	else {
		readdicttable(fd);
		if (dicttblbuf) {
			VOID_C opendicttbl(NULL);
			fd = -1;
		}
	}
# endif
	tmp.k.buf = kana;
	tmp.k.max = len;
	tmp.hinsi[0] = (u_short)HN_SENTOU;
	tmp.hinsi[1] = MAXUTYPE(u_short);
	for (tmp.len = len; tmp.len > (u_char)0; tmp.len--)
		argc = _searchdict(argc, &kanjilist, &tmp, fd);

	tmp.k.max = len;
	tmp.len = (u_char)0;
	tmp.freq = (u_short)0;
	tmp.hinsi[0] = (u_short)HN_TANKAN;
	tmp.hinsi[1] = MAXUTYPE(u_short);
	tmp.ofs = dicttblent;

	tmp.k.buf = (u_short *)Xmalloc((len + 1) * sizeof(u_short));
	for (i = 0; i < len; i++) tmp.k.buf[i] = kana[i];
	tmp.k.buf[len] = (u_short)0;
	argc = addkanji(argc, &kanjilist, &tmp);

	tmp.k.buf = (u_short *)Xmalloc((len + 1) * sizeof(u_short));
	for (i = 0; i < len; i++) {
		if ((kana[i] & 0xff00) != 0x2400) tmp.k.buf[i] = kana[i];
		else tmp.k.buf[i] = (0x2500 | (kana[i] & 0xff));
	}
	tmp.k.buf[len] = (u_short)0;
	tmp.ofs++;
	argc = addkanji(argc, &kanjilist, &tmp);

	if (!argc || !kanjilist) list = NULL;
	else {
		qsort(kanjilist, argc, sizeof(kanjitable), cmpdict);
		argc = uniqkanji(argc, kanjilist);
		qsort(kanjilist, argc, sizeof(kanjitable), cmpfreq);
		list = (u_short **)Xmalloc((argc + 1) * sizeof(u_short *));
		for (n = 0L; n < argc; n++) list[n] = kanjilist[n].k.buf;
		list[n] = NULL;
	}
	VOID_C openfreqtbl(NULL, 0);

	return(list);
}

# if	FD >= 3
static NEAR VOID fgetjis(jp, fp)
jisbuf *jp;
XFILE *fp;
{
	char buf[MAXKLEN * R_MAXKANA + 1];
	int n;

	for (n = 0; n < jp -> max; n++) {
		VOID_C jis2str(buf, jp -> buf[n]);
		kanjifputs(buf, fp);
	}
}

int fgetuserfreq(path, fp)
CONST char *path;
XFILE *fp;
{
	freq_t tmp;
	lockbuf_t *lck;
	long n;

	VOID_C openfreqtbl(NULL, 0);
	if (!(lck = openfreqtbl(path, O_BINARY | O_RDONLY))) return(-1);
	if (lck -> fd < 0) {
		VOID_C openfreqtbl(NULL, 0);
		errno = ENOENT;
		return(-1);
	}

	for (n = 0; n < freqtblent; n++) {
		if (fseekfreq(n, lck -> fd) < 0) break;
		if (fgetfreqbuf(&tmp, lck -> fd) < 0) break;

		fgetjis(&(tmp.kana), fp);
		Xfree(tmp.kana.buf);
		Xfputc('\t', fp);
		fgetjis(&(tmp.kanji), fp);
		Xfree(tmp.kanji.buf);
		Xfprintf(fp, "\t%d\n", tmp.freq);
	}
	VOID_C openfreqtbl(NULL, 0);

	return((n < freqtblent) ? -1 : 0);
}

static CONST char *parsejis(jp, s)
jisbuf *jp;
CONST char *s;
{
	u_short *kbuf;
	char *cp;
	int n;

	for (n = 0; s[n] && s[n] != '\t'; n++) /*EMPTY*/;
	if (n <= 0) return(NULL);

	cp = Xstrndup(s, n);
	for (s += n; *s == '\t'; s++) /*EMPTY*/;

	renewkanjiconv(&cp, defaultkcode, DEFCODE, L_INPUT);
	n = strlen(cp);
	kbuf = (u_short *)Xmalloc(n * sizeof(u_short));
	n = str2jis(kbuf, n, cp);
	Xfree(cp);
	jp -> buf = kbuf;
	jp -> max = n;

	return(s);
}

int fputuserfreq(path, fp)
CONST char *path;
XFILE *fp;
{
	freq_t tmp;
	CONST char *cp;
	char *line;
	int n, freq;

	VOID_C openfreqtbl(NULL, 0);
	n = 0;
	while ((line = Xfgets(fp))) {
		tmp.kana.buf = tmp.kanji.buf = NULL;
		for (cp = line; *cp == '\t'; cp++) /*EMPTY*/;
		if ((cp = parsejis(&(tmp.kana), cp))
		&& (cp = parsejis(&(tmp.kanji), cp))
		&& (freq = Xatoi(cp)) >= 0) {
			if (freq > MAXUTYPE(u_short)) freq = MAXUTYPE(u_short);
			tmp.freq = (u_short)freq;
			n = adduserfreq(path, &tmp);
		}

		Xfree(tmp.kana.buf);
		Xfree(tmp.kanji.buf);
		Xfree(line);

		if (n < 0) break;
	}
	VOID_C openfreqtbl(NULL, 0);

	return(n);
}
# endif	/* FD >= 3 */
#endif	/* DEP_IME */
