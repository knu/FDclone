/*
 *	unixdisk.c
 *
 *	UNIXlike Disk Access Module
 */

#include <stdio.h>
#include "machine.h"
#include "unixdisk.h"

#ifdef	NOVOID
#define	VOID
#define	VOID_T	int
#else
#define	VOID	void
#define	VOID_T	void
#endif

#include "dosdisk.h"

#ifndef	issjis1
#define	issjis1(c)	((0x81 <= (c) && (c) <= 0x9f)\
			|| (0xe0 <= (c) && (c) <= 0xfc))
#endif

#ifdef	FD
extern int toupper2 __P_((int));
extern int isdelim __P_((char *, int));
#else
static int toupper2 __P_((int));
static int isdelim __P_((char *, int));
#endif

static int seterrno __P_((u_short));
#ifdef	DJGPP
static int dos_putpath __P_((char *, int));
#endif
static char *duplpath __P_((char *));
static long intcall __P_((int, __dpmi_regs *, struct SREGS *));
#define	int21call(reg, sreg)	intcall(0x21, reg, sreg)
#ifndef	_NODOSDRIVE
static char *regpath __P_((char *, char *));
static VOID biosreset __P_((int));
static int _biosdiskio __P_((int, int, int, int, u_char *, int, int, int));
#ifndef	PC98
static int xbiosdiskio __P_((int, u_long, u_long, u_char *, int, int, int));
#endif
static int getdrvparam __P_((int, drvinfo *));
static int _checkdrive __P_((int, int, int, u_long));
static int biosdiskio __P_((int, u_long, u_char *, int, int, int));
static int dosrawdiskio __P_((int, u_long, u_char *, int, int, int));
#endif
static int dos_findfirst __P_((char *, struct dosfind_t *, u_short));
static int dos_findnext __P_((struct dosfind_t *));
#ifndef	_NOUSELFN
static long lfn_findfirst __P_((char *, struct lfnfind_t *, u_short));
static int lfn_findnext __P_((u_short, struct lfnfind_t *));
static int lfn_findclose __P_((u_short));
#endif
static u_short getdosmode __P_((u_char));
static u_char putdosmode __P_((u_short));
static time_t getdostime __P_((u_short, u_short));
#ifndef	_NOUSELFN
static int putdostime __P_((u_short *, u_short *, time_t));
#endif

#ifndef	_NODOSDRIVE
int dosdrive = 0;
extern int lastdrive;
extern int dependdosfunc;
static drvinfo drvlist['Z' - 'A' + 1];
static int maxdrive = -1;
#endif

static u_short doserrlist[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 18, 65, 80
};
static int unixerrlist[] = {
	0, EINVAL, ENOENT, ENOENT, EMFILE, EACCES,
	EBADF, ENOMEM, ENOMEM, ENOMEM, ENODEV, 0, EACCES, EEXIST
};
#if !defined (DJGPP) && !defined (_NODOSDRIVE)
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
#endif
static int dos7access = 0;
#define	D7_DOSVER	017
#define	D7_CAPITAL	020


#ifndef	FD
static int toupper2(c)
int c;
{
	return((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
}

static int isdelim(s, ptr)
char *s;
int ptr;
{
	int i;

	if (ptr < 0 || s[ptr] != _SC_) return(0);
	if (--ptr < 0) return(1);
	if (!ptr) return(!issjis1((u_char)s[0]));

	for (i = 0; s[i] && i < ptr; i++) if (issjis1((u_char)s[i])) i++;
	if (!s[i] || i > ptr) return(1);
	return(!issjis1((u_char)s[i]));
}
#endif

static int seterrno(doserr)
u_short doserr;
{
	int i;

	for (i = 0; i < sizeof(doserrlist) / sizeof(u_short); i++)
		if (doserr == doserrlist[i]) return(errno = unixerrlist[i]);
	return(errno = EINVAL);
}

#ifdef	DJGPP
static int dos_putpath(path, offset)
char *path;
int offset;
{
	int i;

	i = (int)strlen(path) + 1;
	dosmemput(path, i, __tb + offset);
	return(i);
}
#endif

static char *duplpath(path)
char *path;
{
	static char buf[MAXPATHLEN + 1];
	int i, j, ps;

	if (path == buf) return(path);
	i = j = ps = 0;
	if (isalpha(path[0]) && path[1] == ':') {
		buf[j++] = path[i++];
		buf[j++] = path[i++];
	}
	if (path[i] == '\\') buf[j++] = path[i++];
	for (; path[i]; i++) {
		if (path[i] == '\\') ps = 1;
		else {
			if (ps) {
				ps = 0;
				buf[j++] = '\\';
			}
			buf[j++] = path[i];
			if (issjis1((u_char)path[i])) buf[j++] = path[++i];
		}
	}
	buf[j] = '\0';
	return(buf);
}

static long intcall(vect, regp, sregp)
int vect;
__dpmi_regs *regp;
struct SREGS *sregp;
{
#ifdef	__TURBOC__
	struct REGPACK preg;
#endif

	(*regp).x.flags |= FR_CARRY;
#ifdef	__TURBOC__
	preg.r_ax = (*regp).x.ax;
	preg.r_bx = (*regp).x.bx;
	preg.r_cx = (*regp).x.cx;
	preg.r_dx = (*regp).x.dx;
	preg.r_bp = (*regp).x.bp;
	preg.r_si = (*regp).x.si;
	preg.r_di = (*regp).x.di;
	preg.r_ds = (*sregp).ds;
	preg.r_es = (*sregp).es;
	preg.r_flags = (*regp).x.flags;
	intr(vect, &preg);
	(*regp).x.ax = preg.r_ax;
	(*regp).x.bx = preg.r_bx;
	(*regp).x.cx = preg.r_cx;
	(*regp).x.dx = preg.r_dx;
	(*regp).x.bp = preg.r_bp;
	(*regp).x.si = preg.r_si;
	(*regp).x.di = preg.r_di;
	(*sregp).ds = preg.r_ds;
	(*sregp).es = preg.r_es;
	(*regp).x.flags = preg.r_flags;
#else	/* !__TURBOC__ */
# ifdef	DJGPP
	(*regp).x.ds = (*sregp).ds;
	(*regp).x.es = (*sregp).es;
	__dpmi_int(vect, regp);
	(*sregp).ds = (*regp).x.ds;
	(*sregp).es = (*regp).x.es;
# else
	int86x(vect, regp, regp, sregp);
# endif
#endif	/* !__TURBOC__ */
	if (!((*regp).x.flags & FR_CARRY)) return((long)((*regp).x.ax));
	seterrno((*regp).x.ax);
	return(-1L);
}

int getcurdrv(VOID_A)
{
	int drive;

	drive = (dos7access & D7_CAPITAL) ? 'A' : 'a';
#ifndef	_NODOSDRIVE
	if (lastdrive >= 0) drive += toupper2(lastdrive) - 'A';
	else
#endif
	drive += (bdos(0x19, 0, 0) & 0xff);
	return(drive);
}

int setcurdrv(drive)
int drive;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	char *path;
	int drv, olddrv;

	errno = EINVAL;
	if (drive >= 'a' && drive <= 'z') {
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
		char tmp[3];

		tmp[0] = drive;
		tmp[1] = ':';
		tmp[2] = '\0';
		return(doschdir(tmp));
	}
#endif

	olddrv = (bdos(0x19, 0, 0) & 0xff);
	if ((bdos(0x0e, drv, 0) & 0xff) < drv
	|| (drive = (bdos(0x19, 0, 0) & 0xff)) != drv) {
		seterrno(0x0f);		/* Invarid drive */
		return(-1);
	}

	path = ".";
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

#ifndef	_NODOSDRIVE
	lastdrive = drive + 'a';
#endif
	return(0);
}

#ifndef	_NOUSELFN
int getdosver(VOID_A)
{
	if (!(dos7access & D7_DOSVER))
		dos7access |= ((u_char)bdos(0x30, 0, 0) & D7_DOSVER);
	return(dos7access & D7_DOSVER);
}

int supportLFN(path)
char *path;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	char drv[4], buf[128];

	if (!path || !isalpha(drv[0] = path[0]) || path[1] != ':')
		drv[0] = getcurdrv();
#ifndef	_NODOSDRIVE
	if (checkdrive(toupper2(drv[0]) - 'A') > 0) return(-2);
#endif
	if (getdosver() < 7) {
#ifndef	_NODOSDRIVE
		if ((dosdrive & 1) && drv[0] >= 'a' && drv[0] <= 'z')
			return(-1);
#endif
		return(-3);
	}
	if (drv[0] >= 'A' && drv[0] <= 'Z') return(0);

	drv[1] = ':';
	drv[2] = _SC_;
	drv[3] = '\0';

	reg.x.ax = 0x71a0;
	reg.x.bx = 0;
	reg.x.cx = sizeof(buf);
#ifdef	DJGPP
	dos_putpath(drv, 0);
#endif
	sreg.ds = PTR_SEG(drv);
	reg.x.dx = PTR_OFF(drv, 0);
	sreg.es = PTR_SEG(buf);
	reg.x.di = PTR_OFF(buf, sizeof(drv));
	if (int21call(&reg, &sreg) < 0 || !(reg.x.bx & 0x4000)) {
		if (reg.x.ax == 15) return(-4);
#ifndef	_NODOSDRIVE
		if (dosdrive & 1) return(-1);
#endif
		return(-3);
	}
#ifdef	DJGPP
	dosmemget(__tb + sizeof(drv), sizeof(buf), buf);
#endif
	if (!strcmp(buf, VOL_FAT32)) return(2);
	return(1);
}
#endif	/* !_NOUSELFN */

char *getcurdir(pathname, drive)
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
	dosmemget(__tb, MAXPATHLEN, pathname);
#endif
	return(pathname);
}

#ifndef	_NOUSELFN
# ifndef	_NODOSDRIVE
static char *regpath(path, buf)
char *path, *buf;
{
	char *cp;
	int drive;

	cp = path;
	if (isalpha(drive = path[0]) && path[1] == ':') cp += 2;
	else drive = getcurdrv();
	buf[0] = drive;
	buf[1] = ':';
	if (!*cp) {
		buf[2] = '.';
		buf[3] = '\0';
	}
	else if (cp == path + 2) return(path);
	else strcpy(&(buf[2]), cp);
	return(buf);
}
# endif	/* !_NODOSDRIVE */

char *shortname(path, alias)
char *path, *alias;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	path = duplpath(path);
	if ((i = supportLFN(path)) <= 0) {
# ifndef	_NODOSDRIVE
		if (i == -1) {
			char buf[MAXPATHLEN + 1];

			dependdosfunc = 0;
			if (dosshortname(regpath(path, buf), alias))
				return(alias);
			else if (!dependdosfunc) return(NULL);
		}
# endif	/* !_NODOSDRIVE */
		strcpy(alias, path);
		return(alias);
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
	if (int21call(&reg, &sreg) < 0) return(NULL);
# ifdef	DJGPP
	dosmemget(__tb + i, MAXPATHLEN, alias);
# endif

	if (!path || !isalpha(i = path[0]) || path[1] != ':') i = getcurdrv();
	if (i >= 'a' && i <= 'z') *alias += 'a' - 'A';

	return(alias);
}
#endif	/* !_NOUSELFN */

char *unixrealpath(path, resolved)
char *path, *resolved;
{
#ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN + 1];
#endif
	int i, j;

	struct SREGS sreg;
	__dpmi_regs reg;

	path = duplpath(path);
#ifdef	_NOUSELFN
	reg.x.ax = 0x6000;
	reg.x.cx = 0;
#else	/* !_NOUSELFN */
	switch (supportLFN(path)) {
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
				strcpy(resolved, path);
				return(resolved);
			}
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
	if (int21call(&reg, &sreg) < 0) return(NULL);
#ifdef	DJGPP
	dosmemget(__tb + i, MAXPATHLEN, resolved);
#endif
	if (!path || !isalpha(i = path[0]) || path[1] != ':') i = getcurdrv();
	else path += 2;
	if (!isalpha(resolved[0])
	|| resolved[1] != ':' || resolved[2] != _SC_) {
		if (resolved[0] == _SC_ && resolved[1] == _SC_) {
			resolved[0] = i;
			resolved[1] = ':';
			i = 2;
			if (isalpha(resolved[2])
			&& resolved[3] == '.' && resolved[4] == _SC_) i = 5;
			while (resolved[i]) if (resolved[i++] == _SC_) break;
			resolved[2] = _SC_;
			for (j = 0; resolved[i + j]; j++)
				resolved[j + 3] = resolved[i + j];
			resolved[j + 3] = '\0';
		}
		else {
			resolved[0] = i;
			resolved[1] = ':';
			resolved[2] = _SC_;
			resolved[3] = '\0';
			if (path && *path == _SC_) strcpy(resolved + 2, path);
		}
	}
	else if (i >= 'a' && i <= 'z' && *resolved >= 'A' && *resolved <= 'Z')
		*resolved += 'a' - 'A';
	else if (i >= 'A' && i <= 'Z' && *resolved >= 'a' && *resolved <= 'z')
		*resolved += 'A' - 'a';

	return(resolved);
}

#ifndef	_NOUSELFN
char *preparefile(path, alias, iscreat)
char *path, *alias;
int iscreat;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	char *cp;

#ifdef	_NODOSDRIVE
	path = duplpath(path);
	if (supportLFN(path) < 0) return(path);
	if ((cp = shortname(path, alias))) return(cp);
	if (!iscreat || errno != ENOENT) return(NULL);
#else	/* !_NODOSDRIVE */
	int i;

	path = duplpath(path);
	if ((i = supportLFN(path)) < -2) return(path);
	if ((cp = shortname(path, alias))) return(cp);
	else if (i < 0 && errno == EACCES) return(path);
	if (!iscreat || errno != ENOENT) return(NULL);
	if (i < 0) {
		char buf[MAXPATHLEN + 1];

		i = dosopen(regpath(path, buf),
			O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (i < 0) return(NULL);
		dosclose(i);
		return(shortname(path, alias));
	}
#endif	/* !_NODOSDRIVE */

	reg.x.ax = 0x716c;
	reg.x.bx = 0x0111;	/* O_WRONLY | SH_DENYRW | NO_BUFFER */
	reg.x.cx = DS_IARCHIVE;
	reg.x.dx = 0x0012;	/* O_CREAT | O_TRUNC */
#ifdef	DJGPP
	dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.si = PTR_OFF(path, 0);
	if (int21call(&reg, &sreg) < 0) return(NULL);

	reg.x.bx = reg.x.ax;
	reg.x.ax = 0x3e00;
	int21call(&reg, &sreg);

	return(shortname(path, alias));
}

#ifdef	DJGPP
char *adjustfname(path)
char *path;
{
	char tmp[MAXPATHLEN + 1];
	int i;

	if (supportLFN(path) > 0) {
		unixrealpath(path, tmp);
		strcpy(path, tmp);
	}
	else
	for (i = (isalpha(path[0]) && path[1] == ':') ? 2 : 0; path[i]; i++) {
		if (path[i] == '/') path[i] = _SC_;
		else path[i] = toupper2(path[i]);
	}
	return(path);
}
#endif	/* DJGPP */

#ifndef	_NODOSDRIVE
static VOID biosreset(drive)
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
static int _biosdiskio(drive, head, sect, cyl, buf, nsect, mode, retry)
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
					memcpy((char *)buf, (char *)cp + ofs,
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
					if ((++cyl) >= drvlist[drive].cyl) {
						errno = EACCES;
						return(-1);
					}
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
		|| (min & ~0xffffUL) == (max & ~0xffffUL)) {
			biosreset(drive);
			continue;
		}
		n = (0x10000UL - (min & 0xffff)) / drvlist[drive].sectsize;
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
		ofs = ((min & ~0xffffUL) == (max & ~0xffffUL)) ? 0 : size;
#ifndef	DJGPP
		if (mode != BIOS_READ)
			memcpy((char *)cp + ofs, (char *)buf, size);
#endif
		i = -1;
	}
	if (cp != buf) free(cp);
	errno = EACCES;
	return(-1);
}

#ifndef	PC98
/*ARGSUSED*/
static int xbiosdiskio(drive, sect, f_sect, buf, nsect, mode, retry)
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

	if (!(drvlist[drive].flags & DI_LBA)) {
		errno = EACCES;
		return(-1);
	}

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
					memcpy((char *)buf, (char *)cp + ofs,
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
		|| (min & ~0xffffUL) == (max & ~0xffffUL)) {
			biosreset(drive);
			continue;
		}
		n = (0x10000UL - (min & 0xffff)) / drvlist[drive].sectsize;
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
		ofs = ((min & ~0xffffUL) == (max & ~0xffffUL)) ? 0 : size;
#ifndef	DJGPP
		if (mode != BIOS_XREAD)
			memcpy((char *)cp + ofs + sizeof(xpacket_t),
				(char *)buf, size);
#endif
		i = -1;
	}
	if (cp != buf) free(cp);
	errno = EACCES;
	return(-1);
}
#endif	/* !PC98 */

static int getdrvparam(drv, buf)
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
			buf -> flags = DI_LBA;
			if (!(buf -> flags & 2)) buf -> flags |= DI_INVALIDCHS;
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

static int _checkdrive(head, sect, cyl, l_sect)
int head, sect, cyl;
u_long l_sect;
{
	partition_t *pt;
	u_char *buf;
	int i, j, sh, ss, sc, ofs, size;
#ifndef	PC98
	u_long ls;
#endif

	ofs = size = drvlist[maxdrive].sectsize;
	if (!(buf = (u_char *)malloc(size))) return(0);

	for (i = 0; i < PART_NUM; i++, ofs += PART_SIZE) {
		if (PART_TABLE + ofs >= size) {
#ifndef	PC98
			if (l_sect) {
				if (xbiosdiskio(maxdrive, l_sect, 0L,
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
			l_sect++;
			ofs = 0;
		}

		for (j = 0; j < PART_SIZE; j++)
			if (buf[PART_TABLE + ofs + j]) break;
		if (j >= PART_SIZE) continue;

		pt = (partition_t *)(&(buf[PART_TABLE + ofs]));
		sh = pt -> s_head;
#ifdef	PC98
		ss = pt -> s_sect;
		sc = byte2word(pt -> s_cyl);
		if (!(pt -> filesys & 0x80)) continue;
#else
		ss = (pt -> s_sect & 0x3f) - 1;
		sc = pt -> s_cyl + ((u_short)(pt -> s_sect & 0xc0) << 2);
		ls = byte2dword(pt -> f_sect);
		if (pt -> filesys == PT_EXTENDLBA) {
			if (_checkdrive(sh, ss, sc, ls) < 0) {
				free(buf);
				return(-1);
			}
			continue;
		}
		if (pt -> filesys == PT_EXTEND) {
			if (_checkdrive(sh, ss, sc, 0L) < 0) {
				free(buf);
				return(-1);
			}
			continue;
		}
#endif

#ifndef	PC98
		drvlist[maxdrive].f_sect = 0L;
		if (pt -> filesys == PT_FAT16XLBA
		|| pt -> filesys == PT_FAT32LBA)
			drvlist[maxdrive].f_sect = ls;
		else
#endif
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
			if (getdrvparam(BIOS_HDD | i,
				&(drvlist[maxdrive])) <= 0) continue;
			if (_checkdrive(0, 1, 0, 0L) < 0) return(-1);
		}
		for (i = 0; i < MAX_SCSI && maxdrive < 'Z' - 'A' + 1; i++) {
			if (getdrvparam(BIOS_SCSI | i,
				&(drvlist[maxdrive])) <= 0) continue;
			if (_checkdrive(0, 1, 0, 0L) < 0) return(-1);
		}
#else
		reg.h.ah = BIOS_PARAM;
		reg.h.dl = BIOS_HDD;
		if (intcall(DISKBIOS, &reg, &sreg) < 0) return(-1);
		n = reg.h.dl;

		for (i = 0; i < n && maxdrive < 'Z' - 'A' + 1; i++) {
			if (!(j = getdrvparam(BIOS_HDD | i,
				&(drvlist[maxdrive])))) continue;
			if (j < 0 || _checkdrive(0, 0, 0, 0L) < 0) return(-1);
		}
#endif
	}
	if (drive >= 0 && drive < maxdrive
	&& (drvlist[drive].flags & DI_PSEUDO))
		return(drvlist[drive].filesys);
	return(0);
}

/*ARGSUSED*/
static int biosdiskio(drive, sect, buf, n, size, iswrite)
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

	if (!iswrite) return(_biosdiskio(drive, head, sect, cyl,
		buf, n, BIOS_READ, BIOSRETRY));
	else if (_biosdiskio(drive, head, sect, cyl,
	buf, n, BIOS_WRITE, BIOSRETRY) < 0) return(-1);
	else return(_biosdiskio(drive, head, sect, cyl,
		buf, n, BIOS_VERIFY, 1));
}

/*ARGSUSED*/
static int dosrawdiskio(drv, sect, buf, n, size, iswrite)
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
		if (err == 0x0207) err = 0x0001;
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
		seterrno(err & 0xff);
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
	int drv;

	drv = toupper2(drive) - 'A';
	if (drv < 0 || drv > 'Z' - 'A') {
		errno = EINVAL;
		return(-1);
	}

#ifdef	DJGPP
	if ((long)size * n > tbsize / 2) {
		if (checkdrive(drv) > 0) while (n-- > 0) {
			if (biosdiskio(drv, sect, buf, 1, size, iswrite) < 0)
				return(-1);
			buf += size;
			sect++;
		}
		else while (n-- > 0) {
			if (dosrawdiskio(drv, sect, buf, 1, size, iswrite) < 0)
				return(-1);
			buf += size;
			sect++;
		}
		return(0);
	}
	else
#endif
	if (checkdrive(drv) > 0)
		return(biosdiskio(drv, sect, buf, n, size, iswrite));
	else return(dosrawdiskio(drv, sect, buf, n, size, iswrite));
}
#endif	/* !_NODOSDRIVE */
#endif	/* !_NOUSELFN */

static int dos_findfirst(path, result, attr)
char *path;
struct dosfind_t *result;
u_short attr;
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
	if (int21call(&reg, &sreg) < 0) return(-1);
#ifdef	DJGPP
	dosmemget(__tb, sizeof(struct dosfind_t), result);
#endif
	return(0);
}

static int dos_findnext(result)
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
static long lfn_findfirst(path, result, attr)
char *path;
struct lfnfind_t *result;
u_short attr;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	long fd;

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
	if ((fd = int21call(&reg, &sreg)) < 0) return(-1L);
#ifdef	DJGPP
	dosmemget(__tb, sizeof(struct lfnfind_t), result);
#endif
	return(fd);
}

static int lfn_findnext(fd, result)
u_short fd;
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

static int lfn_findclose(fd)
u_short fd;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x71a1;
	reg.x.bx = fd;
	return(int21call(&reg, &sreg));
}
#endif	/* !_NOUSELFN */

DIR *unixopendir(dir)
char *dir;
{
	DIR *dirp;
	char path[MAXPATHLEN + 1];
	int i;

#ifndef	_NODOSDRIVE
	if ((i = supportLFN(dir)) < 0 && i > -3) {
		dependdosfunc = 0;
		if ((dirp = dosopendir(regpath(dir, path)))) return(dirp);
		else if (!dependdosfunc) return(NULL);
	}
#endif

	if (!unixrealpath(dir, path)) return(NULL);
	i = strlen(path);
	if (!isdelim(path, i - 1)) strcat(path, _SS_);

	if (!(dirp = (DIR *)malloc(sizeof(DIR)))) return(NULL);
	dirp -> dd_off = 0;

#ifndef	_NOUSELFN
	if (supportLFN(path) > 0) {
		dirp -> dd_id = DID_IFLFN;
		strcat(path, "*");
		dirp -> dd_buf = (char *)malloc(sizeof(struct lfnfind_t));
		if (!dirp -> dd_buf) {
			free(dirp);
			return(NULL);
		}
		if (i > 3) i = -1;
		else i = dos_findfirst(path,
			(struct dosfind_t *)dirp -> dd_buf, DS_IFLABEL);
		if (i >= 0) dirp -> dd_id |= DID_IFLABEL;
		else {
			long fd;

			fd = lfn_findfirst(path,
				(struct lfnfind_t *)dirp -> dd_buf,
				SEARCHATTRS);
			if (fd >= 0) {
				dirp -> dd_fd = (u_short)fd;
				i = 0;
			}
			else {
				dirp -> dd_id &= ~DID_IFLFN;
				i = -1;
			}
		}
	}
	else
#endif	/* !_NOUSELFN */
	{
		dirp -> dd_id = DID_IFNORMAL;
		strcat(path, "*.*");
		dirp -> dd_buf = (char *)malloc(sizeof(struct dosfind_t));
		if (!dirp -> dd_buf) {
			free(dirp);
			return(NULL);
		}
		if (i > 3) i = -1;
		else i = dos_findfirst(path,
			(struct dosfind_t *)dirp -> dd_buf, DS_IFLABEL);
		if (i >= 0) dirp -> dd_id |= DID_IFLABEL;
		else i = dos_findfirst(path,
			(struct dosfind_t *)dirp -> dd_buf,
			(SEARCHATTRS | DS_IFLABEL));
	}

	if (i < 0) {
		if (!errno || errno == ENOENT) dirp -> dd_off = -1;
		else {
#ifndef	_NOUSELFN
			if (dirp -> dd_id & DID_IFLFN)
				lfn_findclose(dirp -> dd_fd);
#endif
			dirp -> dd_id &= ~DID_IFLFN;
			free(dirp -> dd_buf);
			free(dirp);
			return(NULL);
		}
	}
	if (!(dirp -> dd_path = (char *)malloc(strlen(dir) + 1))) {
#ifndef	_NOUSELFN
		if (dirp -> dd_id & DID_IFLFN) lfn_findclose(dirp -> dd_fd);
#endif
		dirp -> dd_id &= ~DID_IFLFN;
		free(dirp -> dd_buf);
		free(dirp);
		return(NULL);
	}
	strcpy(dirp -> dd_path, dir);
	return(dirp);
}

int unixclosedir(dirp)
DIR *dirp;
{
#ifndef	_NODOSDRIVE
	if (dirp -> dd_id < 0) return(dosclosedir(dirp));
#endif
#ifndef	_NOUSELFN
	if (dirp -> dd_id & DID_IFLFN) lfn_findclose(dirp -> dd_fd);
#endif
	free(dirp -> dd_buf);
	free(dirp -> dd_path);
	free(dirp);
	return(0);
}

struct dirent *unixreaddir(dirp)
DIR *dirp;
{
	static struct dirent d;
	char path[MAXPATHLEN + 1];
	int i;

#ifndef	_NODOSDRIVE
	if (dirp -> dd_id < 0) return(dosreaddir(dirp));
#endif
	if (dirp -> dd_off < 0) return(NULL);
	d.d_off = dirp -> dd_off;

#ifndef	_NOUSELFN
	if ((dirp -> dd_id & DID_IFLFN) && !(dirp -> dd_id & DID_IFLABEL)) {
		struct lfnfind_t *bufp;

		bufp = (struct lfnfind_t *)(dirp -> dd_buf);
		strcpy(d.d_name, bufp -> name);
		strcpy(d.d_alias, bufp -> alias);
	}
	else
#endif	/* !_NOUSELFN */
	{
		struct dosfind_t *bufp;

		bufp = (struct dosfind_t *)(dirp -> dd_buf);
		strcpy(d.d_name, bufp -> name);
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
		i = strlen(dirp -> dd_path);
		strcpy(path, dirp -> dd_path);
		if (!isdelim(path, i - 1)) path[i++] = _SC_;

#ifndef	_NOUSELFN
		if (dirp -> dd_id & DID_IFLFN) {
			long fd;

			strcpy(path + i, "*");
			fd = lfn_findfirst(path,
				(struct lfnfind_t *)(dirp -> dd_buf),
				SEARCHATTRS);
			if (fd >= 0) {
				dirp -> dd_fd = (u_short)fd;
				i = 0;
			}
			else {
				dirp -> dd_id &= ~DID_IFLFN;
				i = -1;
			}
		}
		else
#endif	/* !_NOUSELFN */
		{
			strcpy(path + i, "*.*");
			i = dos_findfirst(path,
				(struct dosfind_t *)dirp -> dd_buf,
				SEARCHATTRS);
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
	if (dirp -> dd_id < 0) return(dosrewinddir(dirp));
#endif
#ifndef	_NOUSELFN
	if (dirp -> dd_id & DID_IFLFN) lfn_findclose(dirp -> dd_fd);
#endif
	if (!(dupdirp = unixopendir(dirp -> dd_path))) return(-1);

	free(dirp -> dd_buf);
	free(dupdirp -> dd_path);
	cp = dirp -> dd_path;
	memcpy(dirp, dupdirp, sizeof(DIR));
	dirp -> dd_path = cp;
	free(dupdirp);

	return(0);
}

#ifndef	_NOUSELFN
int unixrename(from, to)
char *from, *to;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	char buf[MAXPATHLEN + 1];
	int f, t;

	from = strcpy(buf, duplpath(from));
	to = duplpath(to);
	f = supportLFN(from);
	t = supportLFN(to);
#ifndef	_NODOSDRIVE
	if (((f < 0 && f > -3) || (t < 0 && t > -3)) && (f != -3 && t != -3)) {
		char buf1[MAXPATHLEN + 1], buf2[MAXPATHLEN + 1];

		dependdosfunc = 0;
		if (dosrename(regpath(from, buf1), regpath(to, buf2)) >= 0)
			return(0);
		else if (!dependdosfunc) return(-1);
	}
#endif	/* !_NODOSDRIVE */
	reg.x.ax = ((f > 0 || t > 0) && (f >= 0 && t >= 0)) ? 0x7156 : 0x5600;
#ifdef	DJGPP
	f = dos_putpath(from, 0);
	t = dos_putpath(to, f);
#endif
	sreg.ds = PTR_SEG(from);
	reg.x.dx = PTR_OFF(from, 0);
	sreg.es = PTR_SEG(to);
	reg.x.di = PTR_OFF(to, f);
	return(int21call(&reg, &sreg));
}

/*ARGSUSED*/
int unixmkdir(path, mode)
char *path;
int mode;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	path = duplpath(path);
	i = supportLFN(path);
#ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) {
		char buf[MAXPATHLEN + 1];

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

int unixchdir(path)
char *path;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	path = duplpath(path);
	if (!path[(isalpha(path[0]) && path[1] == ':') ? 2 : 0]) return(0);
	i = supportLFN(path);
#ifndef	_NODOSDRIVE
	if (i == -1) {
		char buf[MAXPATHLEN + 1];

		if (!(path = preparefile(path, buf, 0))) return(-1);
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
	char tmp[MAXPATHLEN + 1];
#endif

	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);

	pathname[0] = getcurdrv();
	pathname[1] = ':';
	pathname[2] = _SC_;

#ifndef	_NODOSDRIVE
	if (checkdrive(toupper2(pathname[0]) - 'A') > 0) {
		if (!dosgetcwd(pathname, size)) return(NULL);
	}
	else
#endif

	if (!getcurdir(pathname + 3, 0)) return(NULL);
	if (!(dos7access & D7_CAPITAL) && *pathname >= 'A' && *pathname <= 'Z')
		*pathname += 'a' - 'A';
	if ((dos7access & D7_CAPITAL) && *pathname >= 'a' && *pathname <= 'z')
		*pathname += 'A' - 'a';
#ifdef	DJGPP
	strcpy(tmp, pathname);
	unixrealpath(tmp, pathname);
#endif
	return(pathname);
}

static u_short getdosmode(attr)
u_char attr;
{
	u_short mode;

	mode = 0;
	if (attr & DS_IARCHIVE) mode |= S_ISVTX;
	if (!(attr & DS_IHIDDEN)) mode |= S_IREAD;
	if (!(attr & DS_IRDONLY)) mode |= S_IWRITE;
	if (attr & DS_IFDIR) mode |= (S_IFDIR | S_IEXEC);
	else if (attr & DS_IFLABEL) mode |= S_IFIFO;
	else if (attr & DS_IFSYSTEM) mode |= S_IFSOCK;
	else mode |= S_IFREG;

	return(mode);
}

static u_char putdosmode(mode)
u_short mode;
{
	u_char attr;

	attr = 0;
	if (mode & S_ISVTX) attr |= DS_IARCHIVE;
	if (!(mode & S_IREAD)) attr |= DS_IHIDDEN;
	if (!(mode & S_IWRITE)) attr |= DS_IRDONLY;
	if ((mode & S_IFMT) == S_IFSOCK) attr |= DS_IFSYSTEM;

	return(attr);
}

static time_t getdostime(date, time)
u_short date, time;
{
	struct tm tm;

	tm.tm_year = 1980 + ((date >> 9) & 0x7f);
	tm.tm_year -= 1900;
	tm.tm_mon = ((date >> 5) & 0x0f) - 1;
	tm.tm_mday = (date & 0x1f);
	tm.tm_hour = ((time >> 11) & 0x1f);
	tm.tm_min = ((time >> 5) & 0x3f);
	tm.tm_sec = ((time << 1) & 0x3e);
	tm.tm_isdst = -1;

	return(mktime(&tm));
}

#ifndef	_NOUSELFN
static int putdostime(dp, tp, clock)
u_short *dp, *tp;
time_t clock;
{
	struct tm *tm;

	tm = localtime(&clock);
	*dp = (((tm -> tm_year - 80) & 0x7f) << 9)
		+ (((tm -> tm_mon + 1) & 0x0f) << 5)
		+ (tm -> tm_mday & 0x1f);
	*tp = ((tm -> tm_hour & 0x1f) << 11)
		+ ((tm -> tm_min & 0x3f) << 5)
		+ ((tm -> tm_sec & 0x3e) >> 1);

	return(*tp);
}
#endif	/* !_NOUSELFN */

int unixstatfs(path, buf)
char *path;
statfs_t *buf;
{
#ifndef	_NOUSELFN
	struct fat32statfs_t fsbuf;
	int i;
#endif
	struct SREGS sreg;
	__dpmi_regs reg;
	int drv, drive;

	if (!path || !isalpha(drive = path[0]) || path[1] != ':')
		drive = getcurdrv();
	drv = toupper2(drive) - 'A';

	if (drv < 0 || drv > 'Z' - 'A') {
		errno = ENOENT;
		return(-1);
	}

#ifndef	_NOUSELFN
	i = supportLFN(path);
# ifndef _NODOSDRIVE
	if (i == -2) {
		char tmp[sizeof(long) * 3 + 1];

		if (dosstatfs(drive, tmp) < 0) return(-1);
		buf -> f_bsize = *((long *)(&tmp[sizeof(long) * 0]));
		buf -> f_blocks = *((long *)(&tmp[sizeof(long) * 1]));
		buf -> f_bfree =
		buf -> f_bavail = *((long *)(&tmp[sizeof(long) * 2]));
		buf -> f_files = -1;
	}
	else
# endif
	if (i == 2) {
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
		buf -> f_bsize = (long)fsbuf.f_clustsize
			* (long)fsbuf.f_sectsize;
		buf -> f_blocks = (long)fsbuf.f_blocks;
		buf -> f_bfree =
		buf -> f_bavail = (long)fsbuf.f_bavail;
		buf -> f_files = -1;
	}
	else
#endif	/* !_NOUSELFN */
	{
		reg.x.ax = 0x3600;
		reg.h.dl = (u_char)drv + 1;
		int21call(&reg, &sreg);
		if (reg.x.ax == 0xffff) {
			errno = ENOENT;
			return(-1);
		}

		if (!reg.x.ax || !reg.x.cx || !reg.x.dx || reg.x.dx < reg.x.bx)
			buf -> f_bsize = buf -> f_blocks = 
			buf -> f_bfree = buf -> f_bavail = -1L;
		else {
			buf -> f_bsize = (long)(reg.x.ax) * (long)(reg.x.cx);
			buf -> f_blocks = (long)(reg.x.dx);
			buf -> f_bfree =
			buf -> f_bavail = (long)(reg.x.bx);
		}
		buf -> f_files = -1;
	}

	return(0);
}

int unixstat(path, status)
char *path;
struct stat *status;
{
#ifndef	_NOUSELFN
	struct lfnfind_t lbuf;
	long fd;
#endif
	struct dosfind_t dbuf;
	int i;

#ifdef	_NOUSELFN
	if ((i = dos_findfirst(path, &dbuf, SEARCHATTRS)) < 0) {
		if (errno) return(-1);
		if (!strcmp(path, ".."))
			i = dos_findfirst(".", &dbuf, SEARCHATTRS);
		else i = dos_findfirst(path, &dbuf, DS_IFLABEL);
	}
#else	/* !_NOUSELFN */
	i = supportLFN(path);
# ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) {
		char buf[MAXPATHLEN + 1];

		dependdosfunc = 0;
		if (dosstat(regpath(path, buf), status) >= 0) return(0);
		else if (!dependdosfunc) return(-1);
	}
# endif	/* !_NODOSDRIVE */
	if (i <= 0) {
		fd = -1;
		if ((i = dos_findfirst(path, &dbuf, SEARCHATTRS)) < 0) {
			if (errno) return(-1);
			if (!strcmp(path, ".."))
				i = dos_findfirst(".", &dbuf, SEARCHATTRS);
			else i = dos_findfirst(path, &dbuf, DS_IFLABEL);
		}
	}
	else {
		if ((fd = lfn_findfirst(path, &lbuf, SEARCHATTRS)) < 0) {
			if (errno && errno != ENOENT) return(-1);
			if (!strcmp(path, "..")) {
				fd = lfn_findfirst(".", &lbuf, SEARCHATTRS);
				i = (fd >= 0) ? 0 : -1;
			}
			else {
				fd = -1;
				i = dos_findfirst(path, &dbuf, DS_IFLABEL);
			}
		}
	}
#endif	/* !_NOUSELFN */
	if (i < 0) {
		if (!errno) errno = ENOENT;
		return(-1);
	}

#ifndef	_NOUSELFN
	if (fd >= 0) {
		status -> st_mode = getdosmode(lbuf.attr);
		status -> st_mtime = getdostime(lbuf.wrdate, lbuf.wrtime);
		status -> st_size = lbuf.size_l;
		lfn_findclose(fd);
	}
	else
#endif	/* !_NOUSELFN */
	{
		status -> st_mode = getdosmode(dbuf.attr);
		status -> st_mtime = getdostime(dbuf.wrdate, dbuf.wrtime);
		status -> st_size = dbuf.size_l;
	}
	status -> st_ctime = status -> st_atime = status -> st_mtime;
	status -> st_dev =
	status -> st_ino = 0;
	status -> st_nlink = 1;
	status -> st_uid =
	status -> st_gid = -1;
	return(0);
}

int unixchmod(path, mode)
char *path;
int mode;
{
	struct SREGS sreg;
	__dpmi_regs reg;
#ifndef	_NOUSELFN
	char buf[MAXPATHLEN + 1];
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
		path = preparefile(path, buf, 0);
		reg.x.ax = 0x4301;
	}
	else {
		reg.x.ax = 0x7143;
		reg.h.bl = 0x01;
	}
#endif	/* !_NOUSELFN */
	reg.x.cx = putdosmode(mode);
#ifdef	DJGPP
	dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);
	return(int21call(&reg, &sreg));
}

#ifndef	_NOUSELFN
#ifdef	USEUTIME
int unixutime(path, times)
char *path;
struct utimbuf *times;
{
	time_t clock = times -> modtime;
	__dpmi_regs reg;
	struct SREGS sreg;
	char buf[MAXPATHLEN + 1];
	int i;

	path = duplpath(path);
	i = supportLFN(path);
# ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) return(dosutime(regpath(path, buf), times));
# endif
	if (i <= 0) {
		path = preparefile(path, buf, 0);
		return(utime(path, times));
	}
#else	/* !USEUTIME */
int unixutimes(path, tvp)
char *path;
struct timeval tvp[2];
{
	time_t clock = tvp[1].tv_sec;
	__dpmi_regs reg;
	struct SREGS sreg;
	char buf[MAXPATHLEN + 1];
	int i;

	path = duplpath(path);
	i = supportLFN(path);
# ifndef	_NODOSDRIVE
	if (i < 0 && i > -3) return(dosutimes(regpath(path, buf), tvp));
# endif
	if (i <= 0) {
		path = preparefile(path, buf, 0);
		return(utimes(path, tvp));
	}
#endif	/* !USEUTIME */
	reg.x.ax = 0x7143;
	reg.h.bl = 0x03;
	putdostime(&(reg.x.di), &(reg.x.cx), clock);
#ifdef	DJGPP
	dos_putpath(path, 0);
#endif
	sreg.ds = PTR_SEG(path);
	reg.x.dx = PTR_OFF(path, 0);
	return(int21call(&reg, &sreg));
}
#endif	/* !_NOUSELFN */
