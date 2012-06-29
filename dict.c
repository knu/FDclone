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

#define	DICTTBL			"fd-dict.tbl"
#define	MAXHINSI		16
#define	FREQMAGIC		0x4446
#define	FREQVERSION		0x0100
#define	USERFREQBIAS		16
#define	freqbias(n)		(((long)(n) + USERFREQBIAS) * USERFREQBIAS)
#define	getword(s, n)		(((u_short)((s)[(n) + 1]) << 8) | (s)[n])
#define	getdword(s, n)		(((u_long)getword(s, (n) + 2) << 16) \
				| getword(s, n))
#define	skread(f,o,s,n)		(Xlseek(f, o, L_SET) >= (off_t)0 \
				&& sureread(f, s, n) == n)

typedef struct _kanjitable {
	u_short *kbuf;
	u_char klen;
	u_char len;
	u_char match;
	u_char kmatch;
	u_short freq;
	u_short hinsi[MAXHINSI];
	long ofs;
} kanjitable;

#ifdef	DEP_IME

static off_t NEAR dictlseek __P_((int, off_t, int));
static int NEAR fgetbyte __P_((u_char *, int));
static int NEAR fgetword __P_((u_short *, int));
static int NEAR fgetdword __P_((long *, int));
static int NEAR fgetstring __P_((kanjitable *, int));
# ifndef	DEP_EMBEDDICTTBL
static off_t NEAR fgetoffset __P_((long, int));
# endif
static int NEAR fgetjisbuf __P_((kanjitable *, long, int));
static int NEAR fgethinsi __P_((u_short [], int));
static int NEAR fgetfreqbuf __P_((kanjitable *, long, int));
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
static int NEAR fputstring __P_((CONST kanjitable *, int));
static lockbuf_t *NEAR openfreqtbl __P_((CONST char *, int));
static int NEAR getfreq __P_((kanjitable *, CONST kanjitable *));
static int NEAR copyuserfreq __P_((kanjitable *, CONST kanjitable *,
		int, int));
static int cmpdict __P_((CONST VOID_P, CONST VOID_P));
static int cmpfreq __P_((CONST VOID_P, CONST VOID_P));
static long NEAR addkanji __P_((long, kanjitable **, CONST kanjitable *));
static VOID NEAR freekanji __P_((kanjitable *));
static long NEAR addkanjilist __P_((long, kanjitable **,
		long, kanjitable *, kanjitable *, int));
static off_t NEAR nextofs __P_((off_t, int));
static long NEAR _searchdict __P_((long, kanjitable **, kanjitable *, int));
static long NEAR uniqkanji __P_((long, kanjitable *));

char *dicttblpath = NULL;
int imebuffer = 0;

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

static int NEAR fgetstring(kp, fd)
kanjitable *kp;
int fd;
{
	u_short *kbuf;
	u_char c;
	int i;

	kp -> kbuf = NULL;
	if (fgetbyte(&c, fd) < 0) return(-1);
	kp -> klen = c;
	kbuf = (u_short *)Xmalloc((kp -> klen + 1) * sizeof(u_short));
	for (i = 0; i < kp -> klen; i++) if (fgetword(&(kbuf[i]), fd) < 0) {
		Xfree(kbuf);
		return(-1);
	}
	kbuf[i] = (u_short)0;
	kp -> kbuf = kbuf;

	return(0);
}

# ifndef	DEP_EMBEDDICTTBL
static off_t NEAR fgetoffset(n, fd)
long n;
int fd;
{
	u_char buf[4];
	off_t ofs;

	ofs = (off_t)n * 4 + 4;
	if (!skread(fd, ofs, buf, sizeof(buf))) return((off_t)-1);
	ofs = (off_t)(dicttblent + 1) * 4 + 4 + getdword(buf, 0);

	return(ofs);
}
# endif	/* !DEP_EMBEDDICTTBL */

static int NEAR fgetjisbuf(kp, n, fd)
kanjitable *kp;
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
	if (!dicttblbuf) ofs += (off_t)(dicttblent + 1) * 4 + 4;
# endif
	if (dictlseek(fd, ofs, L_SET) < (off_t)0) return(-1);

	return(fgetstring(kp, fd));
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

static int NEAR fgetfreqbuf(kp, n, fd)
kanjitable *kp;
long n;
int fd;
{
	u_char buf[4];
	off_t ofs;

	ofs = (off_t)n * 4 + 4 + 4;
	if (!skread(fd, ofs, buf, sizeof(buf))) return(-1);
	ofs = (off_t)(freqtblent + 1) * 4 + 4 + 4 + getdword(buf, 0);
	if (Xlseek(fd, ofs, L_SET) < (off_t)0) return(-1);

	return(fgetstring(kp, fd));
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
		ofs = (off_t)(dicttblent + 1) * 4 + 4;
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

static int NEAR fputstring(kp, fd)
CONST kanjitable *kp;
int fd;
{
	int i;

	if (fputbyte(kp -> klen, fd) < 0) return(-1);
	for (i = 0; i < kp -> klen; i++)
		if (fputword(kp -> kbuf[i], fd) < 0) return(-1);

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
	file = evalpath(Xstrdup(file), 0);

	freqtblent = 0L;
	lck = lockopen(file, flags, 0666);
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

static int NEAR getfreq(kp1, kp2)
kanjitable *kp1;
CONST kanjitable *kp2;
{
	kanjitable tmp;
	lockbuf_t *lck;
	long ofs, min, max;
	int n;

	kp1 -> ofs = -1L;
	lck = openfreqtbl(FREQFILE, O_BINARY | O_RDONLY);
	if (lck -> fd < 0) return(0);

	min = -1L;
	max = freqtblent;
	for (;;) {
		ofs = (min + max) / 2;
		if (ofs <= min) {
			kp1 -> ofs = min + 1;
			return(0);
		}
		else if (ofs >= max) {
			kp1 -> ofs = max;
			return(0);
		}
		else if (fgetfreqbuf(&tmp, ofs, lck -> fd) < 0) return(-1);

		n = cmpdict(kp1, &tmp);
		Xfree(tmp.kbuf);
		if (n > 0) min = ofs;
		else if (n < 0) max = ofs;
		else if (fgetstring(&tmp, lck -> fd) < 0) return(-1);
		else {
			n = cmpdict(kp2, &tmp);
			Xfree(tmp.kbuf);
			if (n > 0) min = ofs;
			else if (n < 0) max = ofs;
			else break;
		}
	}

	kp1 -> ofs = ofs;
	if (fgetword(&(kp1 -> freq), lck -> fd) < 0) return(-1);

	return(1);
}

static int NEAR copyuserfreq(kp1, kp2, fdin, fdout)
kanjitable *kp1;
CONST kanjitable *kp2;
int fdin, fdout;
{
	kanjitable tmp;
	ALLOC_T size;
	long ofs, index, ent;
	int n, skip;

	size = 1 + (kp1 -> klen * 2) + 1 + (kp2 -> klen * 2) + 2;
	skip = 0;
	if (fdin < 0) {
		ent = kp1 -> ofs = 0L;
		kp1 -> freq = (u_short)1;
	}
	else {
		ent = freqtblent;
		n = getfreq(kp1, kp2);
		if (n < 0) return(-1);
		else if (!n) kp1 -> freq = (u_short)1;
		else {
			skip++;
			if (kp1 -> freq < MAXUTYPE(u_short)) kp1 -> freq++;
		}
		if (Xlseek(fdin, (off_t)1 * 4 + 4 + 4, L_SET) < (off_t)0)
			return(-1);
	}

	if (fputword(FREQMAGIC, fdout) < 0) return(-1);
	if (fputword(FREQVERSION, fdout) < 0) return(-1);
	if (fputdword(ent + 1 - skip, fdout) < 0) return(-1);
	index = 0L;
	if (fputdword(index, fdout) < 0) return(-1);
	for (ofs = 1; ofs <= kp1 -> ofs; ofs++) {
		if (fgetdword(&index, fdin) < 0) return(-1);
		if (fputdword(index, fdout) < 0) return(-1);
	}
	if (fputdword(index + size, fdout) < 0) return(-1);
	if (skip) {
		if (fgetdword(&index, fdin) < 0) return(-1);
		size = (ALLOC_T)0;
		ofs++;
	}
	for (; ofs <= ent; ofs++) {
		if (fgetdword(&index, fdin) < 0) return(-1);
		index += size;
		if (fputdword(index, fdout) < 0) return(-1);
	}

	for (ofs = 0; ofs < kp1 -> ofs; ofs++) {
		if (fgetstring(&tmp, fdin) < 0) return(-1);
		if (fputstring(&tmp, fdout) < 0) return(-1);
		Xfree(tmp.kbuf);
		if (fgetstring(&tmp, fdin) < 0) return(-1);
		if (fputstring(&tmp, fdout) < 0) return(-1);
		Xfree(tmp.kbuf);
		if (fgetword(&(tmp.freq), fdin) < 0) return(-1);
		if (fputword(tmp.freq, fdout) < 0) return(-1);
	}
	if (skip) {
		if (fgetstring(&tmp, fdin) < 0) return(-1);
		Xfree(tmp.kbuf);
		if (fgetstring(&tmp, fdin) < 0) return(-1);
		Xfree(tmp.kbuf);
		if (fgetword(&(tmp.freq), fdin) < 0) return(-1);
		ofs++;
	}
	if (fputstring(kp1, fdout) < 0) return(-1);
	if (fputstring(kp2, fdout) < 0) return(-1);
	if (fputword(kp1 -> freq, fdout) < 0) return(-1);
	for (; ofs < ent; ofs++) {
		if (fgetstring(&tmp, fdin) < 0) return(-1);
		if (fputstring(&tmp, fdout) < 0) return(-1);
		Xfree(tmp.kbuf);
		if (fgetstring(&tmp, fdin) < 0) return(-1);
		if (fputstring(&tmp, fdout) < 0) return(-1);
		Xfree(tmp.kbuf);
		if (fgetword(&(tmp.freq), fdin) < 0) return(-1);
		if (fputword(tmp.freq, fdout) < 0) return(-1);
	}

	return(0);
}

VOID saveuserfreq(kana, kbuf)
CONST u_short *kana, *kbuf;
{
	kanjitable tmp1, tmp2;
	lockbuf_t *lck;
	char *cp, path[MAXPATHLEN];
	long argc;
	int n, fdin, fdout;

	if (!kanjilist) return;

	for (argc = 0L; kanjilist[argc].kbuf; argc++)
		if (kbuf == kanjilist[argc].kbuf) break;
	if (!kanjilist[argc].kbuf) return;

	VOID_C openfreqtbl(NULL, 0);
	if (!(lck = openfreqtbl(FREQFILE, O_BINARY | O_RDWR))) return;
	cp = evalpath(Xstrdup(FREQFILE), 0);
	Xstrcpy(path, cp);
	if ((fdin = lck -> fd) >= 0) fdout = opentmpfile(path, 0666);
	else {
		VOID_C openfreqtbl(NULL, 0);
		lck = lockopen(path,
			O_BINARY | O_WRONLY | O_CREAT | O_EXCL, 0666);
		fdout = (lck) ? lck -> fd : -1;
	}

	if (fdout < 0) {
		VOID_C openfreqtbl(NULL, 0);
		if (fdin < 0) lockclose(lck);
		Xfree(cp);
		return;
	}

	tmp1.kbuf = (u_short *)kana;
	tmp1.klen = kanjilist[argc].match;
	tmp2.kbuf = (u_short *)kbuf;
	tmp2.klen = kanjilist[argc].kmatch;

	n = copyuserfreq(&tmp1, &tmp2, fdin, fdout);
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
		if (n >= 0) n = Xrename(path, cp);
		VOID_C openfreqtbl(NULL, 0);
		VOID_C Xclose(fdout);
# else	/* MSDOS || CYGWIN */
		VOID_C openfreqtbl(NULL, 0);
		VOID_C Xclose(fdout);
		if (n >= 0) {
#  if	MSDOS
			n = Xrename(path, cp);
#  else
			while ((n = Xrename(path, cp)) < 0) {
				if (errno != EACCES) break;
				usleep(100000L);
			}
#  endif
		}
# endif	/* MSDOS || CYGWIN */
		if (n < 0) VOID_C Xunlink(path);
	}

	Xfree(cp);
}

static int cmpdict(vp1, vp2)
CONST VOID_P vp1;
CONST VOID_P vp2;
{
	kanjitable *kp1, *kp2;
	u_short *kbuf1, *kbuf2;
	int i, klen1, klen2;

	kp1 = (kanjitable *)vp1;
	kp2 = (kanjitable *)vp2;
	kbuf1 = kp1 -> kbuf;
	kbuf2 = kp2 -> kbuf;
	klen1 = kp1 -> klen;
	klen2 = kp2 -> klen;

	for (i = 0; i < klen1; i++) {
		if (i >= klen2) return(1);
		if (kbuf1[i] > kbuf2[i]) return(1);
		else if (kbuf1[i] < kbuf2[i]) return(-1);
	}

	return(klen1 - klen2);
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

static long NEAR addkanji(argc, argvp, tmp)
long argc;
kanjitable **argvp;
CONST kanjitable *tmp;
{
	*argvp = (kanjitable *)Xrealloc(*argvp,
		(argc + 2) * sizeof(kanjitable));
	memcpy((char *)&((*argvp)[argc++]), tmp, sizeof(kanjitable));
	(*argvp)[argc].kbuf = NULL;

	return(argc);
}

static VOID NEAR freekanji(argv)
kanjitable *argv;
{
	long n;

	if (argv) {
		for (n = 0L; argv[n].kbuf; n++) Xfree(argv[n].kbuf);
		Xfree(argv);
	}
}

static long NEAR addkanjilist(argc, argvp, argc2, argv2, kp, fd)
long argc;
kanjitable **argvp;
long argc2;
kanjitable *argv2, *kp;
int fd;
{
	kanjitable *next, tmp1, tmp2;
	u_short hinsi[MAXHINSI], shuutan[MAXHINSI];
	long n;
	int i;

	tmp1.kbuf = kp -> kbuf;
	tmp1.klen = kp -> len;
	if (fgetstring(&tmp2, fd) < 0 || fgetword(&(tmp2.freq), fd) < 0
	|| fgethinsi(hinsi, fd) < 0) {
		Xfree(tmp2.kbuf);
		return(argc);
	}

	if (getfreq(&tmp1, &tmp2) <= 0) /*EMPTY*/;
	else if (tmp2.freq < MAXUTYPE(u_short) - freqbias(tmp1.freq))
		tmp2.freq += freqbias(tmp1.freq);
	else tmp2.freq = MAXUTYPE(u_short);
	tmp1.kbuf = tmp2.kbuf;
	tmp1.klen = tmp2.klen;
	tmp1.freq = tmp2.freq;

	tmp2.match = kp -> len;
	tmp2.kmatch = tmp1.klen;
	tmp2.len = kp -> len;
	tmp2.ofs = kp -> ofs;
	if (hinsitblent <= 0) {
		if (kp -> len < kp -> klen) {
			tmp2.klen += kp -> klen - kp -> len;
			tmp2.kbuf = (u_short *)Xrealloc(tmp2.kbuf,
				(tmp2.klen + 1) * sizeof(u_short));
			memcpy((char *)&(tmp2.kbuf[tmp1.klen]),
				(char *)&(kp -> kbuf[kp -> len]),
				(kp -> klen - kp -> len) * sizeof(short));
			tmp2.kbuf[tmp2.klen] = (short)0;
		}
		return(addkanji(argc, argvp, &tmp2));
	}

	i = fchkhinsi(NULL, kp -> hinsi, hinsi, hinsi, fd);
	if (i >= 0 && kp -> len >= kp -> klen) {
		shuutan[0] = SH_svkantan;
		shuutan[1] = MAXUTYPE(u_short);
		i = fchkhinsi(tmp2.hinsi, hinsi, NULL, shuutan, fd);
		if (i >= 0) return(addkanji(argc, argvp, &tmp2));
	}
	if (i < 0) {
		if (kp -> hinsi[0] == (u_short)HN_SENTOU
		&& kp -> len >= kp -> klen) {
			for (i = 0; i < MAXHINSI; i++) {
				if (hinsi[i] == MAXUTYPE(u_short)) break;
				if (hinsi[i] == (u_short)HN_TANKAN)
					return(addkanji(argc, argvp, &tmp2));
			}
		}
		Xfree(tmp1.kbuf);
		return(argc);
	}

	if (!(next = argv2)) {
		tmp2.kbuf = &(kp -> kbuf[kp -> len]);
		tmp2.klen = kp -> klen - kp -> len;
		memcpy((char *)(tmp2.hinsi), (char *)hinsi, sizeof(hinsi));
		for (tmp2.len = kp -> klen; tmp2.len > (u_char)0; tmp2.len--)
			argc2 = _searchdict(argc2, &argv2, &tmp2, fd);
		if (!argv2) {
			Xfree(tmp1.kbuf);
			return(argc);
		}
	}

	for (n = 0L; n < argc2; n++) {
		if (next
		&& fchkhinsi(tmp2.hinsi, hinsi, NULL, argv2[n].hinsi, fd) < 0)
			continue;

		tmp2.klen = tmp1.klen + argv2[n].klen;
		tmp2.kbuf =
			(u_short *)Xmalloc((tmp2.klen + 1) * sizeof(u_short));
		memcpy((char *)(tmp2.kbuf), (char *)(tmp1.kbuf),
			tmp1.klen * sizeof(u_short));
		memcpy((char *)&(tmp2.kbuf[tmp1.klen]),
			(char *)(argv2[n].kbuf),
			argv2[n].klen * sizeof(u_short));
		tmp2.kbuf[tmp2.klen] = (u_short)0;

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
	Xfree(tmp1.kbuf);

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

static long NEAR _searchdict(argc, argvp, kp, fd)
long argc;
kanjitable **argvp, *kp;
int fd;
{
	kanjitable *argv2, tmp1, tmp2;
	off_t curofs;
	long ofs, min, max, argc2;
	u_char c;
	int n;

	dictofs = (off_t)0;
	min = -1L;
	max = dicttblent;
	tmp1.kbuf = kp -> kbuf;
	tmp1.klen = kp -> len;
	for (;;) {
		ofs = (min + max) / 2;
		if (ofs <= min || ofs >= max
		|| (fgetjisbuf(&tmp2, ofs, fd)) < 0)
			return(argc);
		n = cmpdict(&tmp1, &tmp2);
		Xfree(tmp2.kbuf);
		if (n > 0) min = ofs;
		else if (n < 0) max = ofs;
		else break;
	}

	kp -> ofs = ofs;
	if (fgetbyte(&c, fd) < 0) return(argc);
	if (c <= 1) return(addkanjilist(argc, argvp, 0, NULL, kp, fd));

	curofs = dictlseek(fd, (off_t)0, L_INCR);
	if (curofs < (off_t)0) return(argc);
	argv2 = NULL;
	argc2 = 0L;
	if (hinsitblent > 0 && kp -> len < kp -> klen) {
		tmp1.kbuf = &(kp -> kbuf[kp -> len]);
		tmp1.klen = kp -> klen - kp -> len;
		tmp1.hinsi[0] = MAXUTYPE(u_short);
		for (tmp1.len = kp -> klen; tmp1.len > (u_char)0; tmp1.len--)
			argc2 = _searchdict(argc2, &argv2, &tmp1, fd);
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
		for (j = 1L; i + j < argc; j++) {
			if (argv[i].klen != argv[i + j].klen) break;
			c = memcmp((char *)(argv[i].kbuf),
				(char *)(argv[i + j].kbuf),
				argv[i].klen * sizeof(u_short));
			if (c) break;
			Xfree(argv[i + j].kbuf);
			if (argv[i + j].len > argv[i].len) {
				argv[i].len = argv[j].len;
				argv[i].freq = argv[j].freq;
			}
			else if (argv[i + j].len == argv[i].len) {
				if (argv[i + j].freq > argv[i].freq)
					argv[i].freq = argv[j].freq;
			}
		}
		if (j <= 1L) continue;
		memmove((char *)&(argv[i + 1]), (char *)&(argv[i + j]),
			(argc + 1 - (i + j)) * sizeof(kanjitable));
		argc -= --j;
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
				for (n = 0L; kanjilist[n].kbuf; n++)
					if (argv[argc] == kanjilist[n].kbuf)
						break;
				if (kanjilist[n].kbuf) continue;
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
	tmp.kbuf = kana;
	tmp.klen = len;
	tmp.hinsi[0] = (u_short)HN_SENTOU;
	tmp.hinsi[1] = MAXUTYPE(u_short);
	for (tmp.len = len; tmp.len > (u_char)0; tmp.len--)
		argc = _searchdict(argc, &kanjilist, &tmp, fd);

	tmp.klen = len;
	tmp.len = (u_char)0;
	tmp.freq = (u_short)0;
	tmp.hinsi[0] = (u_short)HN_TANKAN;
	tmp.hinsi[1] = MAXUTYPE(u_short);
	tmp.ofs = dicttblent;

	tmp.kbuf = (u_short *)Xmalloc((len + 1) * sizeof(u_short));
	for (i = 0; i < len; i++) tmp.kbuf[i] = kana[i];
	tmp.kbuf[len] = (u_short)0;
	argc = addkanji(argc, &kanjilist, &tmp);

	tmp.kbuf = (u_short *)Xmalloc((len + 1) * sizeof(u_short));
	for (i = 0; i < len; i++) {
		if ((kana[i] & 0xff00) != 0x2400) tmp.kbuf[i] = kana[i];
		else tmp.kbuf[i] = (0x2500 | (kana[i] & 0xff));
	}
	tmp.kbuf[len] = (u_short)0;
	tmp.ofs++;
	argc = addkanji(argc, &kanjilist, &tmp);

	if (!argc || !kanjilist) list = NULL;
	else {
		qsort(kanjilist, argc, sizeof(kanjitable), cmpdict);
		argc = uniqkanji(argc, kanjilist);
		qsort(kanjilist, argc, sizeof(kanjitable), cmpfreq);
		list = (u_short **)Xmalloc((argc + 1) * sizeof(u_short *));
		for (n = 0L; n < argc; n++) list[n] = kanjilist[n].kbuf;
		list[n] = NULL;
	}
	VOID_C openfreqtbl(NULL, 0);

	return(list);
}
#endif	/* DEP_IME */
