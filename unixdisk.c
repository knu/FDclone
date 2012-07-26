/*
 *	unixdisk.c
 *
 *	UNIX-like disk accessing module
 */

#include "headers.h"
#include "depend.h"
#include "kctype.h"
#include "string.h"
#include "sysemu.h"
#include "pathname.h"
#include "dosdisk.h"
#include "unixdisk.h"

#if	MSDOS
#include <io.h>
#endif

#define	KC_SJIS1		0001
#define	KC_SJIS2		0002
#define	KC_EUCJP		0010
#define	int21call(reg, sreg)	intcall(0x21, reg, sreg)

static char *NEAR duplpath __P_((CONST char *));
#ifndef	_NOUSELFN
static int NEAR isreserved __P_((CONST char *));
#endif
#ifndef	_NODOSDRIVE
static char *NEAR regpath __P_((CONST char *, char *));
static VOID NEAR biosreset __P_((int));
static int NEAR _biosdiskio __P_((int, int, int, int,
		u_char *, int, int, int));
#ifndef	PC98
static int NEAR xbiosdiskio __P_((int, u_long, u_long,
		u_char *, int, int, int));
#endif
static int NEAR getdrvparam __P_((int, drvinfo *));
static int NEAR _checkdrive __P_((int, int, int, u_long, u_long));
static int NEAR biosdiskio __P_((int, u_long, u_char *, int, int, int));
static int NEAR lockdrive __P_((int));
static int NEAR unlockdrive __P_((int, int));
static int NEAR dosrawdiskio __P_((int, u_long, u_char *, int, int, int));
#endif
static int NEAR dos_findfirst __P_((CONST char *, u_int, struct dosfind_t *));
static int NEAR dos_findnext __P_((struct dosfind_t *));
#ifndef	_NOUSELFN
static int NEAR lfn_findfirst __P_((u_int *, CONST char *,
		u_int, struct lfnfind_t *));
static int NEAR lfn_findnext __P_((u_int, struct lfnfind_t *));
static int NEAR lfn_findclose __P_((u_int));
#endif
#ifndef	_NOUSELFN
static int NEAR gendosname __P_((char *));
static int NEAR unixrenamedir __P_((CONST char *, CONST char *));
#endif
static int NEAR alter_findfirst __P_((CONST char *, u_int *,
		struct dosfind_t *, struct lfnfind_t *));

#ifndef	_NODOSDRIVE
static drvinfo drvlist['Z' - 'A' + 1];
static int maxdrive = -1;
#endif
static int dos7access = 0;
#define	D7_DOSVER		017
#define	D7_CAPITAL		020
#define	D7_NTARCH		040
#if	!defined (DJGPP) && !defined (_NODOSDRIVE)
static u_char int25bin[] = {
	0x1e,				/* 0000: push ds */
	0x06,				/* 0001: push es */
	0x55,				/* 0002: push bp */
	0x53,				/* 0003: push bx */
	0x51,				/* 0004: push cx */
	0x52,				/* 0005: push dx */
	0x56,				/* 0006: push di */
	0x57,				/* 0007: push si */

	0xb8, 0x00, 0x00,		/* 0008: mov ax, 0 */
	0x8e, 0xd8,			/* 000b: mov ds,ax */
	0xb8, 0x00, 0x00,		/* 000d: mov ax, 0 */
	0xbb, 0x00, 0x00,		/* 0010: mov bx, 0 */
	0xb9, 0x00, 0x00,		/* 0013: mov cx, 0 */
	0xba, 0x00, 0x00,		/* 0016: mov dx, 0 */
	0xfb,				/* 0019: sti */
	0xcd, 0x25,			/* 001a: int 25h */
	0xfa,				/* 001c: cli */
	0x5a,				/* 001d: pop dx */

	0x5f,				/* 001e: pop si */
	0x5e,				/* 001f: pop di */
	0x5a,				/* 0020: pop dx */
	0x59,				/* 0021: pop cx */
	0x5b,				/* 0022: pop bx */
	0x5d,				/* 0023: pop bp */
	0x07,				/* 0024: pop es */
	0x1f,				/* 0025: pop ds */
	0x72, 0x02,			/* 0026: jb $+2 */
	0x31, 0xc0,			/* 0028: xor ax,ax */
	0xcb				/* 002a: retf */
};
static int (far *doint25)__P_((VOID_A)) = NULL;
#endif	/* !DJGPP && !_NODOSDRIVE */
#ifdef	DOUBLESLASH
static int dsdrive = 0;
#endif
#ifndef	_NOUSELFN
static CONST char *inhibitname[] = INHIBITNAME;
#define	INHIBITNAMESIZ		arraysize(inhibitname)
#endif


static char *NEAR duplpath(path)
CONST char *path;
{
	static char buf[MAXPATHLEN];
	int i, j, ps, len, type;

	if (path == buf) return((char *)path);
	i = j = ps = 0;
	if ((len = getpathtop(path, NULL, &type))) {
		memcpy(buf, path, len);
		i = j = len;
	}
	switch (type) {
		case PT_NONE:
		case PT_DOS:
			if (path[i] == _SC_) buf[j++] = path[i++];
			break;
		default:
			break;
	}

	for (; path[i]; i++) {
		if (path[i] == _SC_) ps = 1;
		else {
			if (ps) {
				ps = 0;
				buf[j++] = _SC_;
			}
			buf[j++] = path[i];
			if (iskanji1(path, i)) buf[j++] = path[++i];
		}
	}
	buf[j] = '\0';

	return(buf);
}

int getcurdrv(VOID_A)
{
	int drive;

#ifdef	DOUBLESLASH
	if (dsdrive) return(dsdrive);
#endif
	drive = (dos7access & D7_CAPITAL) ? 'A' : 'a';
#ifndef	_NODOSDRIVE
	if (lastdrive >= 0) drive += Xtoupper(lastdrive) - 'A';
	else
#endif
	drive += (bdos(0x19, 0, 0) & 0xff);

	return(drive);
}

int setcurdrv(drive, nodir)
int drive, nodir;
{
#ifndef	_NODOSDRIVE
	char tmp[3];
#endif
	struct SREGS sreg;
	__dpmi_regs reg;
	CONST char *path;
	int drv, olddrv;

#ifdef	DOUBLESLASH
	if (drive == '_') {
		dsdrive = drive;
		return(0);
	}
	dsdrive = '\0';
#endif

	errno = EINVAL;
	if (Xislower(drive)) {
		drv = drive - 'a';
		dos7access &= ~D7_CAPITAL;
	}
	else {
		drv = drive - 'A';
		dos7access |= D7_CAPITAL;
	}
	if (drv < 0) return(-1);

#ifndef	_NODOSDRIVE
	if (checkdrive(drv) > 0) {
		VOID_C gendospath(tmp, drive, '\0');
		return(doschdir(tmp));
	}
#endif

	olddrv = (bdos(0x19, 0, 0) & 0xff);
	if ((bdos(0x0e, drv, 0) & 0xff) < drv
	|| (drive = (bdos(0x19, 0, 0) & 0xff)) != drv) {
		bdos(0x0e, olddrv, 0);
		dosseterrno(0x0f);	/* Invarid drive */
		return(-1);
	}

	if (!nodir) {
		path = curpath;
		reg.x.ax = 0x3b00;
#ifdef	DJGPP
		dos_putpath(path, 0);
#endif
		sreg.ds = PTR_SEG(path);
		reg.x.dx = PTR_OFF(path, 0);
		if (int21call(&reg, &sreg) < 0) {
			drv = errno;
			bdos(0x0e, olddrv, 0);
			errno = drv;
			return(-1);
		}
	}
#ifndef	_NODOSDRIVE
	lastdrive = drive + 'a';
#endif

	return(0);
}

#ifndef	_NOUSELFN
static int NEAR isreserved(path)
CONST char *path;
{
	int i, len;

	if (strdelim(path, 0)) return(0);
	len = strlen(path);
	for (i = 0; i < INHIBITNAMESIZ; i++) {
		if (strnpathcmp(path, inhibitname[i], len)) continue;
		if (!inhibitname[i][len] || inhibitname[i][len] == ' ')
			return(1);
	}

	if (!strnpathcmp(path, INHIBITCOM, strsize(INHIBITCOM))) {
		if (path[strsize(INHIBITCOM)] > '0'
		&& path[strsize(INHIBITCOM)] <= '0' + INHIBITCOMMAX
		&& !path[strsize(INHIBITCOM) + 1])
			return(1);
	}
	else if (!strnpathcmp(path, INHIBITLPT, strsize(INHIBITLPT))) {
		if (path[strsize(INHIBITLPT)] > '0'
		&& path[strsize(INHIBITLPT)] <= '0' + INHIBITLPTMAX
		&& !path[strsize(INHIBITLPT) + 1])
			return(1);
	}

	return(0);
}

int getdosver(VOID_A)
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int ver;

	if (!(dos7access & D7_DOSVER)) {
		reg.x.ax = 0x3000;
		int21call(&reg, &sreg);
		ver = reg.h.al;
		if (reg.x.ax == 0x0005) {
			reg.x.ax = 0x3306;
			int21call(&reg, &sreg);
			if (reg.h.al != 0xff && reg.x.bx == 0x3205) {
				/* Windows NT/2000/XP command prompt */
				ver = 7;
				dos7access |= D7_NTARCH;
			}
		}
		dos7access |= (ver & D7_DOSVER);
	}

	return(dos7access & D7_DOSVER);
}

/*
 *	 3: enable LFN access, but shared folder
 *	 2: enable LFN access, but FAT32
 *	 1: enable LFN access
 *	 0: do SFN access
 *	-1: raw device access
 *	-2: BIOS access
 *	-3: cannot use LFN with any method
 *	-4: illegal drive error
 */

int supportLFN(path)
CONST char *path;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	char drive, drv[4], buf[128];

	if (!path) drive = 0;
# ifdef	DOUBLESLASH
	else if (isdslash(path)) return(3);
# endif
	else drive = _dospath(path);
	if (!drive) drive = getcurdrv();

# ifdef	DOUBLESLASH
	if (drive == '_') return(3);
# endif
# ifndef	_NODOSDRIVE
	if (checkdrive(Xtoupper(drive) - 'A') > 0) return(-2);
# endif
	if (getdosver() < 7) {
# ifndef	_NODOSDRIVE
		if ((dosdrive & 1) && Xislower(drive)) return(-1);
# endif
		return(-3);
	}
	if (Xisupper(drive)) return(0);

	VOID_C gendospath(drv, drive, _SC_);
	reg.x.ax = 0x71a0;
	reg.x.bx = 0;
	reg.x.cx = sizeof(buf);
# ifdef	DJGPP
	dos_putpath(drv, 0);
# endif
	sreg.ds = PTR_SEG(drv);
	reg.x.dx = PTR_OFF(drv, 0);
	sreg.es = PTR_SEG(buf);
	reg.x.di = PTR_OFF(buf, sizeof(drv));
	if (int21call(&reg, &sreg) < 0 || !(reg.x.bx & 0x4000)) {
		if (reg.x.ax == 15) return(-4);
# ifndef	_NODOSDRIVE
		if (dosdrive & 1) return(-1);
# endif
		return(-3);
	}
# ifdef	DJGPP
	dosmemget(__tb + sizeof(drv), sizeof(buf), buf);
# endif
	if (!strcmp(buf, VOL_FAT32)) return(2);

	return(1);
}
#endif	/* !_NOUSELFN */

char *unixgetcurdir(pathname, drive)
char *pathname;
int drive;
{
	struct SREGS sreg;
	__dpmi_regs reg;

#ifdef	_NOUSELFN
	reg.x.ax = 0x4700;
#else
	reg.x.ax = (supportLFN(NULL) > 0) ? 0x7147 : 0x4700;
#endif
	reg.h.dl = drive;
	sreg.ds = PTR_SEG(pathname);
	reg.x.si = PTR_OFF(pathname, 0);
	if (int21call(&reg, &sreg) < 0) return(NULL);
#ifdef	DJGPP
	dosmemget(__tb, MAXPATHLEN - 3, pathname);
#endif
#ifndef	BSPATHDELIM
	adjustpname(pathname);
#endif

	return(pathname);
}

#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
static char *NEAR regpath(path, buf)
CONST char *path;
char *buf;
{
	CONST char *cp;
	int drive;

	cp = path;
	if (!(drive = _dospath(path))) drive = getcurdrv();
	else if (*(cp += 2)) return((char *)path);
	Xstrcpy(gendospath(buf, drive, (*cp) ? '\0' : '.'), cp);

	return(buf);
}
# endif	/* !_NODOSDRIVE */

char *shortname(path, alias)
CONST char *path;
char *alias;
{
# ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN];
# endif
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	path = duplpath(path);
	if ((i = supportLFN(path)) <= 0) {
# ifndef	_NODOSDRIVE
		if (i == -1) {
			dependdosfunc = 0;
			if (dosshortname(regpath(path, buf), alias))
				return(alias);
			else if (!dependdosfunc) return(NULL);
		}
# endif	/* !_NODOSDRIVE */
		return((char *)path);
	}
	reg.x.ax = 0x7160;
	reg.x.cx = 0x8001;
# ifdef	DJGPP
	i = dos_putpath(path, 0);
# endif
	sreg.ds = PTR_SEG(path);
	reg.x.si = PTR_OFF(path, 0);
	sreg.es = PTR_SEG(alias);
	reg.x.di = PTR_OFF(alias, i);
	if (int21call(&reg, &sreg) < 0) {
		/* Windows NT/2000/XP cannot support reserved filenames */
		if ((dos7access & D7_NTARCH) && isreserved(path))
			return((char *)path);
		return(NULL);
	}
# ifdef	DJGPP
	dosmemget(__tb + i, MAXPATHLEN, alias);
# endif
# ifndef	BSPATHDELIM
	adjustpname(alias);
# endif
# ifdef	DOUBLESLASH
	if (isdslash(alias)) return(alias);
# endif

	if (!path || !(i = _dospath(path))) i = getcurdrv();
	if (Xislower(i)) *alias = Xtolower(*alias);

	return(alias);
}
#endif	/* !_NOUSELFN */

char *unixrealpath(path, resolved)
CONST char *path;
char *resolved;
{
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN];
#endif
	struct SREGS sreg;
	__dpmi_regs reg;
	int i, j;

	path = duplpath(path);
#ifdef	_NOUSELFN
	reg.x.ax = 0x6000;
	reg.x.cx = 0;
#else	/* !_NOUSELFN */
	switch (supportLFN(path)) {
		case 3:
		case 2:
		case 1:
			reg.x.ax = 0x7160;
			reg.x.cx = 0x8002;
			break;
		case 0:
			reg.x.ax = 0x7160;
			reg.x.cx = 0x8001;
			break;
# ifndef	_NODOSDRIVE
		case -1:
		case -2:
			dependdosfunc = 0;
			if (doslongname(regpath(path, buf), resolved))
				return(resolved);
			else if (!dependdosfunc) {
				Xstrcpy(resolved, path);
				return(resolved);
			}
/*FALLTHRU*/
# endif	/* !_NODOSDRIVE */
		default:
			reg.x.ax = 0x6000;
			reg.x.cx = 0;
			break;
	}
#endif	/* !_NOUSELFN */
#ifdef	DJGPP
	i = dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.si = PTR_OFF(path, 0);
	sreg.es = PTR_SEG(resolved);
	reg.x.di = PTR_OFF(resolved, i);
	if (int21call(&reg, &sreg) < 0) {
		/* Windows NT/2000/XP cannot support reserved filenames */
		if ((dos7access & D7_NTARCH) && isreserved(path)) {
			Xstrcpy(resolved, path);
			return(resolved);
		}
		return(NULL);
	}

#ifdef	DJGPP
	dosmemget(__tb + i, MAXPATHLEN, resolved);
#endif
#ifndef	BSPATHDELIM
	adjustpname(resolved);
#endif
#ifdef	DOUBLESLASH
	if (isdslash(resolved)) return(resolved);
#endif

	if (!path || !(i = _dospath(path))) i = getcurdrv();
	else path += 2;
	if (!_dospath(resolved) || resolved[2] != _SC_) {
		if (resolved[0] == _SC_ && resolved[1] == _SC_) {
			resolved[0] = i;
			resolved[1] = ':';
			i = 2;
			if (Xisalpha(resolved[2])
			&& resolved[3] == '.' && resolved[4] == _SC_)
				i = 5;
			while (resolved[i]) if (resolved[i++] == _SC_) break;
			resolved[2] = _SC_;
			for (j = 0; resolved[i + j]; j++)
				resolved[j + 3] = resolved[i + j];
			resolved[j + 3] = '\0';
		}
		else {
			VOID_C gendospath(resolved, i, _SC_);
			if (path && *path == _SC_)
				Xstrcpy(&(resolved[2]), path);
		}
	}
	else *resolved = (Xisupper(i))
		? Xtoupper(*resolved) : Xtolower(*resolved);

	return(resolved);
}

#ifndef	BSPATHDELIM
char *adjustpname(path)
char *path;
{
	int i;

	for (i = 0; path[i]; i++) {
		if (path[i] == '\\') path[i] = _SC_;
		else if (iskanji1(path, i)) i++;
	}

	return(path);
}
#endif	/* !BSPATHDELIM */

#if	defined (DJGPP) || !defined (BSPATHDELIM)
char *adjustfname(path)
char *path;
{
# ifndef	_NOUSELFN
	char tmp[MAXPATHLEN];
# endif
	int i;

# ifndef	_NOUSELFN
	if (supportLFN(path) > 0 && unixrealpath(path, tmp)) {
		Xstrcpy(path, tmp);
		return(path);
	}
# endif

	i = (_dospath(path)) ? 2 : 0;
	Xstrtoupper(&(path[i]));
	for (; path[i]; i++) {
# ifdef	BSPATHDELIM
		if (path[i] == '/') path[i] = _SC_;
# else
		if (path[i] == '\\') path[i] = _SC_;
# endif
		if (iswchar(path, i)) i++;
	}

	return(path);
}
#endif	/* DJGPP || !BSPATHDELIM */

#ifndef	_NOUSELFN
char *preparefile(path, alias)
CONST char *path;
char *alias;
{
	int i;

	path = duplpath(path);
	i = supportLFN(path);
#ifdef	_NODOSDRIVE
	if (i < 0) return((char *)path);
#else	/* !_NODOSDRIVE */
	if (i < -2) return((char *)path);
#endif	/* !_NODOSDRIVE */

	if ((alias = shortname(path, alias))) return(alias);
#ifndef	_NODOSDRIVE
	else if (i < 0 && errno == EACCES) return((char *)path);
#endif

	return(NULL);
}

#ifndef	_NODOSDRIVE
static VOID NEAR biosreset(drive)
int drive;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.h.ah = BIOS_RESET;
#ifdef	PC98
	reg.h.al = drvlist[drive].drv;
#else
	reg.h.dl = drvlist[drive].drv;
#endif
	intcall(DISKBIOS, &reg, &sreg);
}

/*ARGSUSED*/
static int NEAR _biosdiskio(drive, head, sect, cyl, buf, nsect, mode, retry)
int drive, head, sect, cyl;
u_char *buf;
int nsect, mode, retry;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	u_long size, min, max;
	u_char *cp;
	int i, n, ofs;

	n = nsect;
	size = (u_long)n * drvlist[drive].sectsize;
	cp = buf;
	ofs = 0;
	for (i = 0; i < retry; i++) {
		reg.h.ah = mode;
#ifdef	PC98
		reg.h.al = drvlist[drive].drv;
		reg.x.bx = size;
		reg.x.cx = cyl;
		reg.h.dl = sect;
		reg.h.dh = head;
		sreg.es = PTR_SEG(cp + ofs);
		reg.x.bp = PTR_OFF(cp + ofs, ofs);
#else
		reg.h.al = n;
		reg.h.cl = ((cyl >> 2) & 0xc0) | ((sect + 1) & 0x3f);
		reg.h.ch = (cyl & 0xff);
		reg.h.dl = drvlist[drive].drv;
		reg.h.dh = (head & 0xff);
		sreg.es = PTR_SEG(cp + ofs);
		reg.x.bx = PTR_OFF(cp + ofs, ofs);
#endif
#ifdef	DJGPP
		if (mode != BIOS_READ) dosmemput(buf, size, __tb + ofs);
#endif
		if (intcall(DISKBIOS, &reg, &sreg) >= 0) {
#ifdef	DJGPP
			if (mode == BIOS_READ)
				dosmemget(__tb + ofs, size, buf);
#else
			if (cp != buf) {
				if (mode == BIOS_READ)
					memcpy((char *)buf, (char *)&(cp[ofs]),
						size);
				free(cp);
			}
#endif

			if (n >= nsect) return(0);
			cp = (buf += size);
			ofs = 0;
			sect += n;
			while (sect >= drvlist[drive].sect) {
				sect -= drvlist[drive].sect;
				if ((++head) >= drvlist[drive].head) {
					head = 0;
					if ((++cyl) >= drvlist[drive].cyl)
						return(seterrno(EACCES));
				}
			}
			n = (nsect -= n);
			size = (u_long)n * drvlist[drive].sectsize;
			i = -1;
			continue;
		}
		if (sect + n > drvlist[drive].sect) {
			n = drvlist[drive].sect - sect;
			size = (u_long)n * drvlist[drive].sectsize;
			i = -1;
			continue;
		}
		min = PTR_FAR(cp + ofs);
		max = min + size - 1;
		if (reg.h.ah != BIOS_DMAERR || cp != buf
		|| (min & ~((u_long)0xffff)) == (max & ~((u_long)0xffff))) {
			biosreset(drive);
			continue;
		}
		n = ((u_long)0x10000 - (min & 0xffff))
			/ drvlist[drive].sectsize;
		if (n > 0) {
			size = (u_long)n * drvlist[drive].sectsize;
			i = -1;
			continue;
		}
		n = 1;
		size = drvlist[drive].sectsize;
#ifndef	DJGPP
		if (!(cp = (u_char *)malloc(size * 2))) {
			cp = buf;
			biosreset(drive);
			continue;
		}
#endif
		min = PTR_FAR(cp);
		max = min + size - 1;
		ofs = ((min & ~((u_long)0xffff)) == (max & ~((u_long)0xffff)))
			? 0 : size;
#ifndef	DJGPP
		if (mode != BIOS_READ)
			memcpy((char *)&(cp[ofs]), (char *)buf, size);
#endif
		i = -1;
	}
	if (cp != buf) free(cp);

	return(seterrno(EACCES));
}

#ifndef	PC98
/*ARGSUSED*/
static int NEAR xbiosdiskio(drive, sect, f_sect, buf, nsect, mode, retry)
int drive;
u_long sect, f_sect;
u_char *buf;
int nsect, mode, retry;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	xpacket_t pac;
	u_long fs, ls, size, min, max;
	u_char *cp;
	int i, j, cy, n, ofs;

	if (!(drvlist[drive].flags & DI_LBA)) return(seterrno(EACCES));

	n = nsect;
	size = (u_long)n * drvlist[drive].sectsize;
	cp = buf;
	ofs = 0;
	for (i = 0; i < retry; i++) {
		pac.size = XPACKET_SIZE;
		pac.nsect[0] = (n & 0xff);
		pac.nsect[1] = (n >> 8);
		pac.bufptr[0] =
			(PTR_OFF(cp + ofs, ofs + sizeof(xpacket_t)) & 0xff);
		pac.bufptr[1] =
			(PTR_OFF(cp + ofs, ofs + sizeof(xpacket_t)) >> 8);
		pac.bufptr[2] = (PTR_SEG(cp + ofs) & 0xff);
		pac.bufptr[3] = (PTR_SEG(cp + ofs) >> 8);
		fs = f_sect;
		ls = sect;
		for (j = cy = 0; j < 8; j++) {
			cy += (fs & 0xff) + (ls & 0xff);
			pac.sect[j] = (cy & 0xff);
			cy >>= 8;
			fs >>= 8;
			ls >>= 8;
		}

		reg.h.ah = mode;
		reg.h.al = 0;		/* without verify */
		reg.h.dl = drvlist[drive].drv;
		sreg.ds = PTR_SEG(&pac);
		reg.x.si = PTR_OFF(&pac, 0);
#ifdef	DJGPP
		dosmemput(&pac, sizeof(xpacket_t), __tb);
		if (mode != BIOS_XREAD)
			dosmemput(buf, size, __tb + ofs + sizeof(xpacket_t));
#endif
		if (intcall(DISKBIOS, &reg, &sreg) >= 0) {
#ifdef	DJGPP
			if (mode == BIOS_XREAD)
				dosmemget(__tb + ofs + sizeof(xpacket_t),
					size, buf);
#else
			if (cp != buf) {
				if (mode == BIOS_XREAD)
					memcpy((char *)buf, (char *)&(cp[ofs]),
						size);
				free(cp);
			}
#endif

			if (n >= nsect) return(0);
			cp = (buf += size);
			ofs = 0;
			sect += n;
			n = (nsect -= n);
			size = (u_long)n * drvlist[drive].sectsize;
			i = -1;
			continue;
		}
		min = PTR_FAR(cp + ofs);
		max = min + size - 1;
		if (reg.h.ah != BIOS_DMAERR || cp != buf
		|| (min & ~((u_long)0xffff)) == (max & ~((u_long)0xffff))) {
			biosreset(drive);
			continue;
		}
		n = ((u_long)0x10000 - (min & 0xffff))
			/ drvlist[drive].sectsize;
		if (n > 0) {
			size = (u_long)n * drvlist[drive].sectsize;
			i = -1;
			continue;
		}
		n = 1;
		size = drvlist[drive].sectsize;
#ifndef	DJGPP
		if (!(cp = (u_char *)malloc(size * 2))) {
			cp = buf;
			biosreset(drive);
			continue;
		}
#endif
		min = PTR_FAR(cp);
		max = min + size - 1;
		ofs = ((min & ~((u_long)0xffff)) == (max & ~((u_long)0xffff)))
			? 0 : size;
#ifndef	DJGPP
		if (mode != BIOS_XREAD)
			memcpy((char *)cp + ofs + sizeof(xpacket_t),
				(char *)buf, size);
#endif
		i = -1;
	}
	if (cp != buf) free(cp);

	return(seterrno(EACCES));
}
#endif	/* !PC98 */

static int NEAR getdrvparam(drv, buf)
int drv;
drvinfo *buf;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	buf -> flags = 0;
#ifndef	PC98
	reg.h.ah = BIOS_TYPE;
	reg.h.dl = drv;
	if (intcall(DISKBIOS, &reg, &sreg) < 0) return(-1);
	if (reg.h.ah != DT_HARDDISK) return(0);

	reg.h.ah = BIOS_XCHECK;
	reg.x.bx = 0x55aa;
	reg.h.dl = drv;
	if (intcall(DISKBIOS, &reg, &sreg) >= 0
	&& reg.x.bx == 0xaa55 && (reg.x.cx & 1)) {
		xparam_t pbuf;

		pbuf.size[0] = (XPARAM_SIZE & 0xff);
		pbuf.size[1] = (XPARAM_SIZE >> 8);
		reg.h.ah = BIOS_XPARAM;
		reg.h.dl = drv;
		sreg.ds = PTR_SEG(&pbuf);
		reg.x.si = PTR_OFF(&pbuf, 0);
		if (intcall(DISKBIOS, &reg, &sreg) >= 0 && !(reg.h.ah)) {
#ifdef	DJGPP
			dosmemget(__tb, sizeof(xparam_t), &pbuf);
#endif
			buf -> flags |= DI_LBA;
			if (!(pbuf.flags[0] & 2))
				buf -> flags |= DI_INVALIDCHS;
		}
	}
#endif

	reg.h.ah = BIOS_PARAM;
#ifdef	PC98
	reg.h.al = drv;
#else
	reg.h.dl = drv;
#endif
	if (intcall(DISKBIOS, &reg, &sreg) < 0) return(-1);

#ifdef	PC98
	buf -> head = reg.h.dh;
	buf -> sect = reg.h.dl;
	buf -> cyl = reg.x.cx;
	buf -> sectsize = reg.x.bx;
#else
	buf -> head = reg.h.dh + 1;
	buf -> sect = (reg.h.cl & 0x3f);
	buf -> cyl = reg.h.ch + ((u_short)(reg.h.cl & 0xc0) << 2) + 1;
	buf -> sectsize = BOOTSECTSIZE;
#endif
	if (!(buf -> head) || !(buf -> sect) || !(buf -> cyl)) return(0);
	buf -> secthead = buf -> head * buf -> sect;
	buf -> drv = drv;

	return(1);
}

/*ARGSUSED*/
static int NEAR _checkdrive(head, sect, cyl, l_sect, e_sect)
int head, sect, cyl;
u_long l_sect, e_sect;
{
	partition_t *pt;
	u_char *buf;
	int i, j, sh, ss, sc, ofs, size;
#ifndef	PC98
	u_long ls, ps;

	ps = l_sect;
#endif
	ofs = size = drvlist[maxdrive].sectsize;
	if (!(buf = (u_char *)malloc(size))) return(0);

	for (i = 0; i < PART_NUM; i++, ofs += PART_SIZE) {
		if (PART_TABLE + ofs >= size) {
#ifndef	PC98
			if (ps) {
				if (xbiosdiskio(maxdrive, ps++, (u_long)0,
				buf, 1, BIOS_XREAD, BIOSRETRY) < 0
				&& ((drvlist[maxdrive].flags & DI_INVALIDCHS)
				|| _biosdiskio(maxdrive, head, sect, cyl,
				buf, 1, BIOS_READ, BIOSRETRY) < 0)) {
					free(buf);
					return(-1);
				}
			}
			else
#endif
			if (_biosdiskio(maxdrive, head, sect, cyl,
			buf, 1, BIOS_READ, BIOSRETRY) < 0) {
				free(buf);
				return(-1);
			}
#ifndef	PC98
			if (buf[size - 2] != 0x55 || buf[size - 1] != 0xaa) {
				free(buf);
				return(0);
			}
#endif
			sect++;
			ofs = 0;
		}

		for (j = 0; j < PART_SIZE; j++)
			if (buf[PART_TABLE + ofs + j]) break;
		if (j >= PART_SIZE) continue;

		pt = (partition_t *)&(buf[PART_TABLE + ofs]);
		sh = pt -> s_head;
#ifdef	PC98
		ss = pt -> s_sect;
		sc = byte2word(pt -> s_cyl);
		if (!(pt -> filesys & 0x80)) continue;
#else	/* !PC98 */
		ss = (pt -> s_sect & 0x3f) - 1;
		sc = pt -> s_cyl + ((u_short)(pt -> s_sect & 0xc0) << 2);
		ls = byte2dword(pt -> f_sect);
		if (pt -> filesys == PT_EXTENDLBA) {
			if (e_sect) ls += e_sect;
			else e_sect = ls;
			if (_checkdrive(sh, ss, sc, ls, e_sect) < 0) {
				free(buf);
				return(-1);
			}
			continue;
		}
		if (pt -> filesys == PT_EXTEND) {
			if (_checkdrive(sh, ss, sc, (u_long)0, (u_long)0) < 0)
			{
				free(buf);
				return(-1);
			}
			continue;
		}

		drvlist[maxdrive].f_sect = (u_long)0;
		if (pt -> filesys == PT_FAT16XLBA
		|| pt -> filesys == PT_FAT32LBA)
			drvlist[maxdrive].f_sect = ls + l_sect;
		else
#endif	/* !PC98 */
		if (pt -> filesys != PT_FAT12 && pt -> filesys != PT_FAT16
		&& pt -> filesys != PT_FAT16X && pt -> filesys != PT_FAT32)
			continue;

		drvlist[maxdrive].s_head = sh;
		drvlist[maxdrive].s_sect = ss;
		drvlist[maxdrive].s_cyl = sc;
		drvlist[maxdrive].flags |= (DI_PSEUDO | DI_CHECKED);
		drvlist[maxdrive].filesys = pt -> filesys;

		if (maxdrive++ >= 'Z' - 'A') return(0);
		drvlist[maxdrive].drv = drvlist[maxdrive - 1].drv;
		drvlist[maxdrive].head = drvlist[maxdrive - 1].head;
		drvlist[maxdrive].sect = drvlist[maxdrive - 1].sect;
		drvlist[maxdrive].cyl = drvlist[maxdrive - 1].cyl;
		drvlist[maxdrive].sectsize = drvlist[maxdrive - 1].sectsize;
		drvlist[maxdrive].secthead = drvlist[maxdrive - 1].secthead;
		drvlist[maxdrive].flags = drvlist[maxdrive - 1].flags;
	}
	free(buf);

	return(1);
}

int checkdrive(drive)
int drive;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;
#ifndef	PC98
	int j, n;
#endif

	if (!(dosdrive & 2)) return(0);
	if (maxdrive < 0) {
		maxdrive = 0;
		for (i = 0; i < 'Z' - 'A' + 1; i++) {
			reg.x.ax = 0x4408;
			reg.h.bl = i + 1;
			if (int21call(&reg, &sreg) >= 0)
				drvlist[i].flags =
					(reg.h.al) ? DI_FIXED : DI_REMOVABLE;
			else {
				reg.x.ax = 0x4409;
				reg.h.bl = i + 1;
				if (int21call(&reg, &sreg) >= 0)
					drvlist[i].flags = DI_MISC;
				else drvlist[i].flags = DI_NOPLOVED;
			}
			if (drvlist[i].flags & DI_TYPE) maxdrive = i + 1;
			drvlist[i].flags |= DI_CHECKED;
		}
		if (maxdrive >= 'Z' - 'A' + 1) return(-1);

#ifdef	PC98
		for (i = 0; i < MAX_HDD && maxdrive < 'Z' - 'A' + 1; i++) {
			if (getdrvparam(BIOS_HDD | i, &(drvlist[maxdrive]))
			<= 0)
				continue;
			if (_checkdrive(0, 1, 0, (u_long)0, (u_long)0) < 0)
				return(-1);
		}
		for (i = 0; i < MAX_SCSI && maxdrive < 'Z' - 'A' + 1; i++) {
			if (getdrvparam(BIOS_SCSI | i, &(drvlist[maxdrive]))
			<= 0)
				continue;
			if (_checkdrive(0, 1, 0, (u_long)0, (u_long)0) < 0)
				return(-1);
		}
#else
		reg.h.ah = BIOS_PARAM;
		reg.h.dl = BIOS_HDD;
		if (intcall(DISKBIOS, &reg, &sreg) < 0) return(-1);
		n = reg.h.dl;

		for (i = 0; i < n && maxdrive < 'Z' - 'A' + 1; i++) {
			j = getdrvparam(BIOS_HDD | i, &(drvlist[maxdrive]));
			if (!j) continue;
			if (j < 0
			|| _checkdrive(0, 0, 0, (u_long)0, (u_long)0) < 0)
				return(-1);
		}
#endif
	}
	if (drive >= 0 && drive < maxdrive
	&& (drvlist[drive].flags & DI_PSEUDO))
		return(drvlist[drive].filesys);

	return(0);
}

/*ARGSUSED*/
static int NEAR biosdiskio(drive, sect, buf, n, size, iswrite)
int drive;
u_long sect;
u_char *buf;
int n, size, iswrite;
{
	int head, cyl;
#ifdef	PC98
	int i;

	if (sect && (i = size / drvlist[drive].sectsize) > 1) {
		sect *= i;
		n *= i;
	}
#else
	if (drvlist[drive].f_sect) {
		if (!iswrite) {
			if (xbiosdiskio(drive, sect, drvlist[drive].f_sect,
			buf, n, BIOS_XREAD, BIOSRETRY) >= 0)
				return(0);
		}
		else {
			if (xbiosdiskio(drive, sect, drvlist[drive].f_sect,
			buf, n, BIOS_XWRITE, BIOSRETRY) >= 0
			&& xbiosdiskio(drive, sect, drvlist[drive].f_sect,
			buf, n, BIOS_XVERIFY, 1) >= 0)
				return(0);
		}
		if ((drvlist[drive].flags & DI_INVALIDCHS)) return(-1);
	}
#endif

	sect += ((u_long)(drvlist[drive].s_cyl) * drvlist[drive].secthead)
		+ ((u_long)(drvlist[drive].s_head) * drvlist[drive].sect)
		+ (u_long)(drvlist[drive].s_sect);

	cyl = sect / drvlist[drive].secthead;
	sect %= drvlist[drive].secthead;
	head = sect / drvlist[drive].sect;
	sect %= drvlist[drive].sect;

	if (!iswrite)
		return(_biosdiskio(drive, head, sect, cyl,
			buf, n, BIOS_READ, BIOSRETRY));
	else if (_biosdiskio(drive, head, sect, cyl,
	buf, n, BIOS_WRITE, BIOSRETRY) < 0)
		return(-1);
	else return(_biosdiskio(drive, head, sect, cyl,
		buf, n, BIOS_VERIFY, 1));
}

static int NEAR lockdrive(drv)
int drv;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int i, start, end;

	if (getdosver() < 7) return(0);

	reg.x.ax = 0x1600;
	intcall(0x2f, &reg, &sreg);
	if (reg.h.al == 0xff || reg.h.al == 0x80 || reg.h.al < 3)
		start = end = 4;
	else {
		start = 1;
		end = 2;
	}

	for (i = start; i <= end; i++) {
		reg.x.ax = 0x440d;
		reg.h.bl = drv + 1;
		reg.h.bh = i;
		reg.x.cx = 0x084a;
		reg.x.dx = 0x0001;
		if (int21call(&reg, &sreg) < 0) {
			while (--i >= start) {
				reg.x.ax = 0x440d;
				reg.h.bl = drv + 1;
				reg.x.cx = 0x086a;
				if (int21call(&reg, &sreg) < 0) break;
			}
			return(-1);
		}
	}

	return(end);
}

static int NEAR unlockdrive(drv, level)
int drv, level;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int i, start, end;

	if (level < 1) return(0);

	start = level;
	end = (level <= 3) ? 1 : level;

	for (i = start; i >= end; i--) {
		reg.x.ax = 0x440d;
		reg.h.bl = drv + 1;
		reg.x.cx = 0x086a;
		if (int21call(&reg, &sreg) < 0) return(-1);
	}

	return(0);
}

/*ARGSUSED*/
static int NEAR dosrawdiskio(drv, sect, buf, n, size, iswrite)
int drv;
u_long sect;
u_char *buf;
int n, size, iswrite;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	struct iopacket_t pac;
	int err;

	size *= n;
#ifdef	DJGPP
	reg.x.ax = (iswrite) ? 0x3526 : 0x3525;
	__dpmi_int(0x21, &reg);
	reg.x.cs = reg.x.es;
	reg.x.ip = reg.x.bx;
#else
	int25bin[0x1b] = (iswrite) ? 0x26 : 0x25;
	doint25 = (int (far *)__P_((VOID_A)))int25bin;
#endif

	err = 0x0207;
	if (sect <= 0xffff) {
#ifdef	DJGPP
		reg.x.ax = drv;
		reg.x.cx = n;
		reg.x.dx = (u_short)sect;
		reg.x.ds = PTR_SEG(buf);
		reg.x.bx = PTR_OFF(buf, sizeof(struct iopacket_t));
		reg.x.ss = reg.x.sp = 0;
		if (iswrite)
			dosmemput(buf, size, __tb + sizeof(struct iopacket_t));
		_go32_dpmi_simulate_fcall(&reg);
		err = (reg.x.flags & FR_CARRY) ? reg.x.ax : 0;
#else
		int25bin[0x09] = (PTR_SEG(buf) & 0xff);
		int25bin[0x0a] = ((PTR_SEG(buf) >> 8) & 0xff);
		int25bin[0x0e] = (drv & 0xff);
		int25bin[0x0f] = ((drv >> 8) & 0xff);
		int25bin[0x11] = (PTR_OFF(buf, sizeof(struct iopacket_t))
			& 0xff);
		int25bin[0x12] = ((PTR_OFF(buf, sizeof(struct iopacket_t))
			>> 8) & 0xff);
		int25bin[0x14] = (n & 0xff);
		int25bin[0x15] = ((n >> 8) & 0xff);
		int25bin[0x17] = ((u_short)sect & 0xff);
		int25bin[0x18] = (((u_short)sect >> 8) & 0xff);
		err = doint25();
#endif
	}
	if (err == 0x0207) {
		pac.sect = sect;
		pac.size = n;
		pac.buf_seg = PTR_SEG(buf);
		pac.buf_off = PTR_OFF(buf, sizeof(struct iopacket_t));
#ifdef	DJGPP
		dosmemput(&pac, sizeof(struct iopacket_t), __tb);
		if (iswrite)
			dosmemput(buf, size, __tb + sizeof(struct iopacket_t));
		reg.x.ax = drv;
		reg.x.cx = 0xffff;
		reg.x.ds = PTR_SEG(&pac);
		reg.x.bx = PTR_OFF(&pac, 0);
		reg.x.ss = reg.x.sp = 0;
		_go32_dpmi_simulate_fcall(&reg);
		err = (reg.x.flags & FR_CARRY) ? reg.x.ax : 0;
#else
		int25bin[0x09] = (PTR_SEG(&pac) & 0xff);
		int25bin[0x0a] = ((PTR_SEG(&pac) >> 8) & 0xff);
		int25bin[0x0e] = (drv & 0xff);
		int25bin[0x0f] = ((drv >> 8) & 0xff);
		int25bin[0x11] = (PTR_OFF(&pac, 0) & 0xff);
		int25bin[0x12] = ((PTR_OFF(&pac, 0) >> 8) & 0xff);
		int25bin[0x14] = (0xffff & 0xff);
		int25bin[0x15] = ((0xffff >> 8) & 0xff);
		err = doint25();
#endif
		if (err == 0x0207) err = 0x0001;
	}
	if (err == 0x0001) {
		pac.sect = sect;
		pac.size = n;
		pac.buf_seg = PTR_SEG(buf);
		pac.buf_off = PTR_OFF(buf, sizeof(struct iopacket_t));
#ifdef	DJGPP
		dosmemput(&pac, sizeof(struct iopacket_t), __tb);
		if (iswrite)
			dosmemput(buf, size, __tb + sizeof(struct iopacket_t));
#endif
		reg.x.ax = 0x7305;
		reg.x.cx = 0xffff;
		reg.h.dl = drv + 1;
		reg.x.si = (iswrite) ? 0x0001 : 0x0000;
		sreg.ds = PTR_SEG(&pac);
		reg.x.bx = PTR_OFF(&pac, 0);
		if (int21call(&reg, &sreg) < 0) return(-1);
	}
	else if (err) {
		switch (err & 0xff) {
			case 0:
			case 10:
			case 11:
				errno = EACCES;
				break;
			case 1:
			case 2:
				errno = ENODEV;
				break;
			default:
				errno = EINVAL;
				break;
		}
		return(-1);
	}
#ifdef	DJGPP
	if (!iswrite) dosmemget(__tb + sizeof(struct iopacket_t), size, buf);
#endif

	return(0);
}

int rawdiskio(drive, sect, buf, n, size, iswrite)
int drive;
u_long sect;
u_char *buf;
int n, size, iswrite;
{
	int drv, ret, level;

	drv = Xtoupper(drive) - 'A';
	if (drv < 0 || drv > 'Z' - 'A') return(seterrno(EINVAL));

	ret = 0;
#ifdef	FAKEUNINIT
	level = 0;	/* fake for -Wuninitialized */
#endif
#ifdef	DJGPP
	if ((long)size * n > tbsize / 2) {
		if (checkdrive(drv) > 0) while (n-- > 0) {
			ret = biosdiskio(drv, sect, buf, 1, size, iswrite);
			if (ret < 0) break;
			buf += size;
			sect++;
		}
		else {
			if (iswrite && (level = lockdrive(drv)) < 0)
				return(-1);
			while (n-- > 0) {
				ret = dosrawdiskio(drv, sect, buf,
					1, size, iswrite);
				if (ret < 0) break;
				buf += size;
				sect++;
			}
			if (iswrite && unlockdrive(drv, level) < 0) return(-1);
		}
	}
	else
#endif
	if (checkdrive(drv) > 0)
		ret = biosdiskio(drv, sect, buf, n, size, iswrite);
	else {
		if (iswrite && (level = lockdrive(drv)) < 0) return(-1);
		ret = dosrawdiskio(drv, sect, buf, n, size, iswrite);
		if (iswrite && unlockdrive(drv, level) < 0) return(-1);
	}

	return(ret);
}
#endif	/* !_NODOSDRIVE */
#endif	/* !_NOUSELFN */

static int NEAR dos_findfirst(path, attr, result)
CONST char *path;
u_int attr;
struct dosfind_t *result;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	path = duplpath(path);
	reg.x.ax = 0x1a00;
	sreg.ds = PTR_SEG(result);
	reg.x.dx = PTR_OFF(result, 0);
	int21call(&reg, &sreg);

	reg.x.ax = 0x4e00;
	reg.x.cx = attr;
#ifdef	DJGPP
	dos_putpath(path, sizeof(struct dosfind_t));
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, sizeof(struct dosfind_t));
	if (int21call(&reg, &sreg) < 0) {
		if (!errno) errno = ENOENT;
		return(-1);
	}
#ifdef	DJGPP
	dosmemget(__tb, sizeof(struct dosfind_t), result);
#endif

	return(0);
}

static int NEAR dos_findnext(result)
struct dosfind_t *result;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x1a00;
	sreg.ds = PTR_SEG(result);
	reg.x.dx = PTR_OFF(result, 0);
#ifdef	DJGPP
	dosmemput(result, sizeof(struct dosfind_t), __tb);
#endif
	int21call(&reg, &sreg);

	reg.x.ax = 0x4f00;
	if (int21call(&reg, &sreg) < 0) return(-1);
#ifdef	DJGPP
	dosmemget(__tb, sizeof(struct dosfind_t), result);
#endif

	return(0);
}

#ifndef	_NOUSELFN
static int NEAR lfn_findfirst(fdp, path, attr, result)
u_int *fdp;
CONST char *path;
u_int attr;
struct lfnfind_t *result;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	path = duplpath(path);
	reg.x.ax = 0x714e;
	reg.x.cx = attr;
	reg.x.si = DATETIMEFORMAT;
#ifdef	DJGPP
	dos_putpath(path, sizeof(struct lfnfind_t));
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, sizeof(struct lfnfind_t));
	sreg.es = PTR_SEG(result);
	reg.x.di = PTR_OFF(result, 0);
	if (int21call(&reg, &sreg) < 0) {
		if (!errno) errno = ENOENT;
		return(-1);
	}
	*fdp = reg.x.ax;
#ifdef	DJGPP
	dosmemget(__tb, sizeof(struct lfnfind_t), result);
#endif

	return(0);
}

static int NEAR lfn_findnext(fd, result)
u_int fd;
struct lfnfind_t *result;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x714f;
	reg.x.bx = fd;
	reg.x.si = DATETIMEFORMAT;
	sreg.es = PTR_SEG(result);
	reg.x.di = PTR_OFF(result, 0);
	if (int21call(&reg, &sreg) < 0) return(-1);
#ifdef	DJGPP
	dosmemget(__tb, sizeof(struct lfnfind_t), result);
#endif

	return(0);
}

static int NEAR lfn_findclose(fd)
u_int fd;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x71a1;
	reg.x.bx = fd;

	return(int21call(&reg, &sreg));
}
#endif	/* !_NOUSELFN */

DIR *unixopendir(dir)
CONST char *dir;
{
	DIR *dirp;
	char *cp, path[MAXPATHLEN];
	int i;

#ifndef	_NODOSDRIVE
	if ((i = supportLFN(dir)) < 0 && i > -3) {
		dependdosfunc = 0;
		if ((dirp = dosopendir(regpath(dir, path)))) return(dirp);
		else if (!dependdosfunc) return(NULL);
	}
#endif

	if (!unixrealpath(dir, path)) return(NULL);
	cp = strcatdelim(path);

	if (!(dirp = (DIR *)malloc(sizeof(DIR)))) return(NULL);
	dirp -> dd_off = 0;

#ifndef	_NOUSELFN
	if (supportLFN(path) > 0) {
		dirp -> dd_id = DID_IFLFN;
		Xstrcpy(cp, "*");
		dirp -> dd_buf = (char *)malloc(sizeof(struct lfnfind_t));
		if (!dirp -> dd_buf) {
			free(dirp);
			return(NULL);
		}
		if (cp - 1 > &(path[3])) i = -1;
		else i = dos_findfirst(path, DS_IFLABEL,
			(struct dosfind_t *)(dirp -> dd_buf));
		if (i >= 0) dirp -> dd_id |= DID_IFLABEL;
		else {
			u_int fd;

			i = lfn_findfirst(&fd, path, SEARCHATTRS,
				(struct lfnfind_t *)(dirp -> dd_buf));
			if (i >= 0) dirp -> dd_fd = fd;
			else dirp -> dd_id &= ~DID_IFLFN;
		}
	}
	else
#endif	/* !_NOUSELFN */
	{
		dirp -> dd_id = DID_IFNORMAL;
		Xstrcpy(cp, "*.*");
		dirp -> dd_buf = (char *)malloc(sizeof(struct dosfind_t));
		if (!dirp -> dd_buf) {
			free(dirp);
			return(NULL);
		}
		if (cp - 1 > &(path[3])) i = -1;
		else i = dos_findfirst(path, DS_IFLABEL,
			(struct dosfind_t *)(dirp -> dd_buf));
		if (i >= 0) dirp -> dd_id |= DID_IFLABEL;
		else i = dos_findfirst(path, (SEARCHATTRS | DS_IFLABEL),
			(struct dosfind_t *)(dirp -> dd_buf));
	}

	if (i < 0) {
		if (!errno || errno == ENOENT) dirp -> dd_off = -1;
		else {
#ifndef	_NOUSELFN
			if (dirp -> dd_id & DID_IFLFN)
				VOID_C lfn_findclose(dirp -> dd_fd);
#endif
			free(dirp -> dd_buf);
			free(dirp);
			return(NULL);
		}
	}
	if (!(dirp -> dd_path = (char *)malloc(strlen(dir) + 1))) {
#ifndef	_NOUSELFN
		if (dirp -> dd_id & DID_IFLFN)
			VOID_C lfn_findclose(dirp -> dd_fd);
#endif
		free(dirp -> dd_buf);
		free(dirp);
		return(NULL);
	}
	Xstrcpy(dirp -> dd_path, dir);

	return(dirp);
}

int unixclosedir(dirp)
DIR *dirp;
{
#ifndef	_NODOSDRIVE
	if (dirp -> dd_id == DID_IFDOSDRIVE) return(dosclosedir(dirp));
#endif
#ifndef	_NOUSELFN
	if (dirp -> dd_id & DID_IFLFN) VOID_C lfn_findclose(dirp -> dd_fd);
#endif
	free(dirp -> dd_buf);
	free(dirp -> dd_path);
	free(dirp);

	return(0);
}

struct dirent *unixreaddir(dirp)
DIR *dirp;
{
#ifndef	_NOUSELFN
	struct lfnfind_t *lbufp;
	u_int fd;
#endif
	struct dosfind_t *dbufp;
	static struct dirent d;
	char *cp, path[MAXPATHLEN];
	int i;

#ifndef	_NODOSDRIVE
	if (dirp -> dd_id == DID_IFDOSDRIVE) return(dosreaddir(dirp));
#endif
	if (dirp -> dd_off < 0) return(NULL);
	d.d_off = dirp -> dd_off;

#ifndef	_NOUSELFN
	if ((dirp -> dd_id & DID_IFLFN) && !(dirp -> dd_id & DID_IFLABEL)) {
		lbufp = (struct lfnfind_t *)(dirp -> dd_buf);
		Xstrcpy(d.d_name, lbufp -> name);
		Xstrcpy(d.d_alias, lbufp -> alias);
	}
	else
#endif	/* !_NOUSELFN */
	{
		dbufp = (struct dosfind_t *)(dirp -> dd_buf);
		Xstrcpy(d.d_name, dbufp -> name);
		d.d_alias[0] = '\0';
	}

	if (!(dirp -> dd_id & DID_IFLABEL)) {
#ifndef	_NOUSELFN
		if (dirp -> dd_id & DID_IFLFN)
			i = lfn_findnext(dirp -> dd_fd,
				(struct lfnfind_t *)(dirp -> dd_buf));
		else
#endif	/* !_NOUSELFN */
		i = dos_findnext((struct dosfind_t *)(dirp -> dd_buf));
	}
	else {
		cp = strcatdelim2(path, dirp -> dd_path, NULL);

#ifndef	_NOUSELFN
		if (dirp -> dd_id & DID_IFLFN) {
			Xstrcpy(cp, "*");
			i = lfn_findfirst(&fd, path, SEARCHATTRS,
				(struct lfnfind_t *)(dirp -> dd_buf));
			if (i >= 0) dirp -> dd_fd = fd;
			else dirp -> dd_id &= ~DID_IFLFN;
		}
		else
#endif	/* !_NOUSELFN */
		{
			Xstrcpy(cp, "*.*");
			i = dos_findfirst(path, SEARCHATTRS,
				(struct dosfind_t *)(dirp -> dd_buf));
		}
	}
	if (i >= 0) dirp -> dd_off++;
	else {
		dirp -> dd_off = -1;
#ifndef	_NOUSELFN
		if (dirp -> dd_id & DID_IFLABEL) dirp -> dd_id &= ~DID_IFLFN;
#endif
	}
	dirp -> dd_id &= ~DID_IFLABEL;

	return(&d);
}

int unixrewinddir(dirp)
DIR *dirp;
{
	DIR *dupdirp;
	char *cp;

#ifndef	_NODOSDRIVE
	if (dirp -> dd_id == DID_IFDOSDRIVE) return(dosrewinddir(dirp));
#endif
#ifndef	_NOUSELFN
	if (dirp -> dd_id & DID_IFLFN) VOID_C lfn_findclose(dirp -> dd_fd);
#endif
	if (!(dupdirp = unixopendir(dirp -> dd_path))) return(-1);

	free(dirp -> dd_buf);
	free(dupdirp -> dd_path);
	cp = dirp -> dd_path;
	memcpy((char *)dirp, (char *)dupdirp, sizeof(DIR));
	dirp -> dd_path = cp;
	free(dupdirp);

	return(0);
}

#ifdef	_NOUSELFN
# ifndef	DJGPP
/*ARGSUSED*/
int unixmkdir(path, mode)
CONST char * path;
int mode;
{
	struct stat st;

	if (unixstat(path, &st) >= 0) return(seterrno(EEXIST));

	return(mkdir(path) ? -1 : 0);
}
# endif
#else	/* !_NOUSELFN */
int unixunlink(path)
CONST char *path;
{
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN];
#endif
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	path = duplpath(path);
	i = supportLFN(path);
#ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) {
		if (dosunlink(regpath(path, buf)) >= 0) return(0);
	}
#endif	/* !_NODOSDRIVE */
	if (i <= 0) reg.x.ax = 0x4100;
	else {
		reg.x.ax = 0x7141;
		reg.x.cx = SEARCHATTRS & ~DS_IFDIR;
		reg.x.si = 0;		/* forbid wild card */
	}
#ifdef	DJGPP
	dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);

	return(int21call(&reg, &sreg));
}

static int NEAR gendosname(s)
char *s;
{
	char *cp;
	int i;

	if ((cp = strrdelim(s, 1))) s = &(cp[1]);

	for (i = 0; i < 8 && s[i]; i++) {
		if (s[i] == ' ') return(0);
		if (s[i] == '.') break;
		if (iswchar(s, i)) {
			if (i + 1 >= 8) break;
			i++;
		}
	}
	if (!i) return(-1);

	s += i;
	if (!*s) return(1);
	if (*s == '.') cp = &(s[1]);
	else if ((cp = Xstrchr(&(s[1]), '.'))) cp++;

	for (i = 0; i < 3 && cp && cp[i]; i++) {
		if (cp[i] == ' ') return(0);
		if (cp[i] == '.') return(-1);
		if (iswchar(cp, i)) {
			if (i + 1 >= 3) break;
			s[i + 1] = cp[i];
			i++;
		}
		s[i + 1] = cp[i];
	}
	if (i) *(s++) = '.';
	s[i] = '\0';
	Xstrtoupper(s);

	return(1);
}

static int NEAR unixrenamedir(from, to)
CONST char *from, *to;
{
	DIR *dirp;
	struct dirent *dp;
	CONST char *cp;
	char *fp, *tp, fbuf[MAXPATHLEN], tbuf[MAXPATHLEN];
	int i;

	Xstrcpy(fbuf, to);
	if (!(tp = strrdelim(fbuf, 1))) {
		cp = to;
		Xstrcpy(fbuf, curpath);
	}
	else {
		if (*tp == _SC_) tp++;
		*tp = '\0';
		cp = &(to[tp - fbuf]);
	}
	if (!(to = unixrealpath(fbuf, tbuf))
	|| !(from = unixrealpath(from, fbuf)))
		return(-1);
	Xstrcpy(strcatdelim(tbuf), cp);

	for (i = 0; from[i]; i++) if (from[i] != to[i]) break;
	if (!strdelim(&(from[i]), 0) && !strdelim(&(to[i]), 0)) return(-1);

	if (unixmkdir(to, 0777) < 0) return(-1);
	if (!(dirp = unixopendir(from))) {
		VOID_C unixrmdir(to);
		return(-1);
	}
	i = strlen(from);
	fp = strcatdelim(fbuf);
	tp = strcatdelim(tbuf);
	while ((dp = unixreaddir(dirp))) {
		if (isdotdir(dp -> d_name)) continue;
		Xstrcpy(fp, dp -> d_name);
		Xstrcpy(tp, dp -> d_name);
		if (unixrename(from, to) < 0) {
			VOID_C unixclosedir(dirp);
			return(-1);
		}
	}
	VOID_C unixclosedir(dirp);
	fbuf[i] = '\0';
	if (unixrmdir(from) < 0) return(-1);

	return(0);
}

int unixrename(from, to)
CONST char *from, *to;
{
#ifndef	_NODOSDRIVE
	char buf1[MAXPATHLEN], buf2[MAXPATHLEN];
#endif
	struct dosfind_t dbuf;
	struct SREGS sreg;
	__dpmi_regs reg;
	char fbuf[MAXPATHLEN], tbuf[MAXPATHLEN];
	int i, ax, f, t;

	Xstrcpy(fbuf, duplpath(from));
	from = fbuf;
	Xstrcpy(tbuf, duplpath(to));
	to = tbuf;
	f = supportLFN(from);
	t = supportLFN(to);
#ifndef	_NODOSDRIVE
	if (((f < 0 && f > -3) || (t < 0 && t > -3)) && (f != -3 && t != -3)) {
		dependdosfunc = 0;
		if (dosrename(regpath(from, buf1), regpath(to, buf2)) >= 0)
			return(0);
		else if (!dependdosfunc) return(-1);
	}
#endif	/* !_NODOSDRIVE */
	ax = ((f > 0 || t > 0) && (f >= 0 && t >= 0)) ? 0x7156 : 0x5600;
	if (f < t) t = f;

	for (i = 0; i < 3; i++) {
		reg.x.ax = ax;
#ifdef	DJGPP
		f = dos_putpath(from, 0);
		dos_putpath(to, f);
#endif
		sreg.ds = PTR_SEG(from);
		reg.x.dx = PTR_OFF(from, 0);
		sreg.es = PTR_SEG(to);
		reg.x.di = PTR_OFF(to, f);
		if (int21call(&reg, &sreg) >= 0) return(0);
		if (reg.x.ax != 0x05) return(-1);

		if (i == 0 && unixunlink(to) < 0) i++;
		if (i == 1) {
			if (ax != 0x5600
			|| dos_findfirst(from, SEARCHATTRS, &dbuf) < 0
			|| !(dbuf.attr & DS_IFDIR))
				break;

			if (t > -3) {
				if ((f = gendosname(tbuf)) < 0) break;
				if (f > 0) {
					ax = 0x7156;
					continue;
				}
			}
			if (unixrenamedir(from, to) < 0) break;
			return(0);
		}
	}

	return(seterrno(EACCES));
}

/*ARGSUSED*/
int unixmkdir(path, mode)
CONST char *path;
int mode;
{
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN];
#endif
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	path = duplpath(path);
	i = supportLFN(path);
#ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) {
		dependdosfunc = 0;
		if (dosmkdir(regpath(path, buf), mode) >= 0) return(0);
		else if (!dependdosfunc) return(-1);
	}
#endif	/* !_NODOSDRIVE */
	reg.x.ax = (i > 0) ? 0x7139 : 0x3900;
#ifdef	DJGPP
	dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);
	if (int21call(&reg, &sreg) >= 0) return(0);
	if (reg.x.ax == 5) errno = EEXIST;

	return(-1);
}

int unixrmdir(path)
CONST char *path;
{
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN];
#endif
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	path = duplpath(path);
	i = supportLFN(path);
#ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) {
		if (dosrmdir(regpath(path, buf)) >= 0) return(0);
	}
#endif	/* !_NODOSDRIVE */
	reg.x.ax = (i > 0) ? 0x713a : 0x3a00;
#ifdef	DJGPP
	dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);

	return(int21call(&reg, &sreg));
}

int unixchdir(path)
CONST char *path;
{
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN];
#endif
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	path = duplpath(path);
	if (!path[(_dospath(path)) ? 2 : 0]) return(0);
	i = supportLFN(path);
#ifndef	_NODOSDRIVE
	if (i == -1) {
		if (!(path = preparefile(path, buf))) return(-1);
	}
#endif	/* !_NODOSDRIVE */
	reg.x.ax = (i > 0) ? 0x713b : 0x3b00;
#ifdef	DJGPP
	dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);

	return(int21call(&reg, &sreg));
}
#endif	/* !_NOUSELFN */

char *unixgetcwd(pathname, size)
char *pathname;
int size;
{
#ifdef	DJGPP
	char tmp[MAXPATHLEN];
#endif
	char *cp;
	int drive;

	drive = getcurdrv();
#ifndef	_NODOSDRIVE
	if (checkdrive(Xtoupper(drive) - 'A') > 0) {
		if (!dosgetcwd(pathname, size)) return(NULL);
	}
	else
#endif
	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);
	else {
#ifdef	DOUBLESLASH
		if (drive == '_') cp = pathname;
		else
#endif
		cp = gendospath(pathname, drive, _SC_);
		if (!unixgetcurdir(cp, 0)) return(NULL);
	}

	*pathname = (dos7access & D7_CAPITAL)
		? Xtoupper(*pathname) : Xtolower(*pathname);
#ifdef	DJGPP
	if (unixrealpath(pathname, tmp)) Xstrcpy(pathname, tmp);
#endif

	return(pathname);
}

int unixstatfs(path, buf)
CONST char *path;
statfs_t *buf;
{
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	char tmp[3 * sizeof(long) + 1];
# endif
	struct fat32statfs_t fsbuf;
	int i;
#endif	/* !_NOUSELFN */
	struct SREGS sreg;
	__dpmi_regs reg;
	int drv, drive;

	if (!path || !(drive = _dospath(path))) drive = getcurdrv();
	drv = Xtoupper(drive) - 'A';

	if (drv < 0 || drv > 'Z' - 'A') return(seterrno(ENOENT));

#ifndef	_NOUSELFN
	i = supportLFN(path);
# ifndef	_NODOSDRIVE
	if (i == -2) {
		if (dosstatfs(drive, tmp) < 0) return(-1);
		buf -> Xf_bsize = *((u_long *)&(tmp[0 * sizeof(long)]));
		buf -> Xf_blocks = *((u_long *)&(tmp[1 * sizeof(long)]));
		buf -> Xf_bfree =
		buf -> Xf_bavail = *((u_long *)&(tmp[2 * sizeof(long)]));
	}
	else
# endif
	if (i >= 2) {
		reg.x.ax = 0x7303;
		reg.x.cx = sizeof(fsbuf);
# ifdef	DJGPP
		dos_putpath(path, sizeof(fsbuf));
# endif
		sreg.es = PTR_SEG(&fsbuf);
		reg.x.di = PTR_OFF(&fsbuf, 0);
		sreg.ds = PTR_SEG(path);
		reg.x.dx = PTR_OFF(path, sizeof(fsbuf));
		if (int21call(&reg, &sreg) < 0) return(-1);

# ifdef	DJGPP
		dosmemget(__tb, sizeof(fsbuf), &fsbuf);
# endif
		buf -> Xf_bsize = (u_long)fsbuf.f_clustsize
			* (long)fsbuf.f_sectsize;
		buf -> Xf_blocks = (u_long)fsbuf.f_blocks;
		buf -> Xf_bfree =
		buf -> Xf_bavail = (u_long)fsbuf.f_bavail;
	}
	else
#endif	/* !_NOUSELFN */
	{
		reg.x.ax = 0x3600;
		reg.h.dl = (u_char)drv + 1;
		int21call(&reg, &sreg);
		if (reg.x.ax == 0xffff) return(seterrno(ENOENT));

		buf -> Xf_bsize = (u_long)(reg.x.ax) * (u_long)(reg.x.cx);
		buf -> Xf_blocks = (u_long)(reg.x.dx);
		buf -> Xf_bfree =
		buf -> Xf_bavail = (u_long)(reg.x.bx);
	}

	if (!(buf -> Xf_blocks) || !(buf -> Xf_bsize)
	|| buf -> Xf_blocks < buf -> Xf_bavail)
		return(seterrno(EIO));
	buf -> Xf_files = -1L;

	return(0);
}

/*ARGSUSED*/
static int NEAR alter_findfirst(path, fdp, dbuf, lbuf)
CONST char *path;
u_int *fdp;
struct dosfind_t *dbuf;
struct lfnfind_t *lbuf;
{
	CONST char *cp;
	char tmp[MAXPATHLEN];
	int n, len;

#ifndef	_NOUSELFN
	if (fdp) n = lfn_findfirst(fdp, path, SEARCHATTRS, lbuf);
	else
#endif
	n = dos_findfirst(path, SEARCHATTRS, dbuf);
	if (n >= 0) return(n);
	if (errno != ENOENT) return(-1);

	if ((cp = strrdelim(path, 1))) cp++;
	else cp = path;

	if (isdotdir(cp) == 1) {
		len = cp - path;
		memcpy(tmp, path, len);
		Xstrcpy(&(tmp[len]), curpath);
#ifndef	_NOUSELFN
		if (fdp) n = lfn_findfirst(fdp, tmp, SEARCHATTRS, lbuf);
		else
#endif
		n = dos_findfirst(tmp, SEARCHATTRS, dbuf);
	}
	else {
#ifdef	DOUBLESLASH
		if (isdslash(path)) return(-1);
		else if (_dospath(path)) /*EMPTY*/;
		else if (getcurdrv() == '_') return(-1);
#endif
		n = dos_findfirst(path, DS_IFLABEL, dbuf);
	}

	return(n);
}

int unixstat(path, stp)
CONST char *path;
struct stat *stp;
{
#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN];
# endif
	struct lfnfind_t lbuf;
	u_int fd;
#endif	/* !_NOUSELFN */
	struct dosfind_t dbuf;
	int i, n;

	if (strpbrk(path, "*?")) return(seterrno(ENOENT));
#ifdef	_NOUSELFN
	n = alter_findfirst(path, NULL, &dbuf, NULL);
#else	/* !_NOUSELFN */
	fd = (u_int)-1;
	i = supportLFN(path);
# ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) {
		dependdosfunc = 0;
		if (dosstat(regpath(path, buf), stp) >= 0) return(0);
		else if (!dependdosfunc) return(-1);
	}
# endif
	if (i <= 0) n = alter_findfirst(path, NULL, &dbuf, &lbuf);
	else n = alter_findfirst(path, &fd, &dbuf, &lbuf);
#endif	/* !_NOUSELFN */
	if (n < 0) return(-1);

#ifndef	_NOUSELFN
	if (fd != (u_int)-1) {
		stp -> st_mode = getunixmode(lbuf.attr);
		stp -> st_mtime = getunixtime(lbuf.wrdate, lbuf.wrtime);
		stp -> st_size = lbuf.size_l;
		VOID_C lfn_findclose(fd);
	}
	else
#endif	/* !_NOUSELFN */
	{
		stp -> st_mode = getunixmode(dbuf.attr);
		stp -> st_mtime = getunixtime(dbuf.wrdate, dbuf.wrtime);
		stp -> st_size = dbuf.size_l;
	}
	stp -> st_ctime = stp -> st_atime = stp -> st_mtime;
	stp -> st_dev = stp -> st_ino = 0;
	stp -> st_nlink = 1;
	stp -> st_uid = stp -> st_gid = -1;

	return(0);
}

int unixchmod(path, mode)
CONST char *path;
int mode;
{
	struct SREGS sreg;
	__dpmi_regs reg;
#ifndef	_NOUSELFN
	char buf[MAXPATHLEN];
	int i;
#endif

	path = duplpath(path);
#ifdef	_NOUSELFN
	reg.x.ax = 0x4301;
#else	/* !_NOUSELFN */
	i = supportLFN(path);
	if (i <= 0) {
# ifndef	_NODOSDRIVE
		if (i == -2) return(doschmod(path, mode));
# endif
		if (!(path = preparefile(path, buf))) return(-1);
		reg.x.ax = 0x4301;
	}
	else {
		reg.x.ax = 0x7143;
		reg.h.bl = 0x01;
	}
#endif	/* !_NOUSELFN */
	reg.x.cx = getdosmode(mode);
#ifdef	DJGPP
	dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);

	return(int21call(&reg, &sreg));
}

#ifndef	_NOUSELFN
int unixutimes(path, utp)
CONST char *path;
CONST struct utimes_t *utp;
{
	time_t t;
	__dpmi_regs reg;
	struct SREGS sreg;
	char buf[MAXPATHLEN];
	int i, fd;

	t = utp -> modtime;
	path = duplpath(path);
	i = supportLFN(path);
	if (i < 0 && i > -3) {
# ifndef	_NODOSDRIVE
		return(dosutimes(regpath(path, buf), utp));
# else
		if (!(path = preparefile(path, buf))) return(-1);
# endif
	}
	if (i <= 0) {
		if ((fd = open(path, O_RDONLY, 0666)) >= 0) {
			reg.x.ax = 0x5701;
			reg.x.bx = (u_short)fd;
			VOID_C getdostime(&(reg.x.dx), &(reg.x.cx), t);
			i = int21call(&reg, &sreg);
			VOID_C close(fd);
			return(i);
		}
		if (i || errno != EACCES) return(-1);
	}

	reg.x.ax = 0x7143;
	reg.h.bl = 0x03;
	VOID_C getdostime(&(reg.x.di), &(reg.x.cx), t);
# ifdef	DJGPP
	dos_putpath(path, 0);
# endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);

	return(int21call(&reg, &sreg));
}

int unixopen(path, flags, mode)
CONST char *path;
int flags, mode;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	char buf[MAXPATHLEN];
	int i;

	path = duplpath(path);
	i = supportLFN(path);
# ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) {
		dependdosfunc = 0;
		if ((i = dosopen(regpath(path, buf), flags, mode)) >= 0)
			return(i);
		else if (!dependdosfunc) return(-1);
	}
# endif	/* !_NODOSDRIVE */
	if (i <= 0) return(open(path, flags, mode));

	reg.x.ax = 0x716c;
	reg.x.bx = 0x0110;	/* SH_DENYRW | NO_BUFFER */
	if ((flags & O_ACCMODE) == O_WRONLY) reg.x.bx |= 0x0001;
	else if ((flags & O_ACCMODE) == O_RDWR) reg.x.bx |= 0x0002;
	reg.x.cx = getdosmode(mode);
	if (flags & O_CREAT) {
		if (flags & O_EXCL) {
			reg.x.dx = 0x0010;
			flags &= ~(O_CREAT | O_EXCL);
		}
		else if (flags & O_TRUNC) reg.x.dx = 0x0012;
		else reg.x.dx = 0x0011;
	}
	else if (flags & O_TRUNC) reg.x.dx = 0x0002;
	else reg.x.dx = 0x0001;
# ifdef	DJGPP
	dos_putpath(path, 0);
# endif
	sreg.ds = PTR_SEG(path);
	reg.x.si = PTR_OFF(path, 0);
	if (int21call(&reg, &sreg) < 0) return(-1);

	reg.x.bx = reg.x.ax;
	reg.x.ax = 0x3e00;
	int21call(&reg, &sreg);

	if (!(path = shortname(path, buf))) return(-1);

	return(open(path, flags, mode));
}
#endif	/* !_NOUSELFN */
