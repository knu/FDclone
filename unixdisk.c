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

static int seterrno __P_((u_short));
#ifdef	DJGPP
static int dos_putpath __P_((char *, int));
#endif
static long int21call __P_((__dpmi_regs *, struct SREGS *));
#ifndef	_NODOSDRIVE
static char *regpath __P_((char *, char *));
#endif
#ifndef	NOLFNEMU
static int dos_findfirst __P_((char *, struct dosfind_t *, u_short));
static int dos_findnext __P_((struct dosfind_t *));
static long lfn_findfirst __P_((char *, struct lfnfind_t *, u_short));
static int lfn_findnext __P_((u_short, struct lfnfind_t *));
static int lfn_findclose __P_((u_short));
#endif
static u_short getdosmode __P_((u_char));
static u_char putdosmode __P_((u_short));
static time_t getdostime __P_((u_short, u_short));

#ifndef	_NODOSDRIVE
int dosdrive = 0;
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
	0xb8, 0x00, 0x00, 		/* 000d: mov ax, 0 */
	0xbb, 0x00, 0x00, 		/* 0010: mov bx, 0 */
	0xb9, 0x00, 0x00, 		/* 0013: mov cx, 0 */
	0xba, 0x00, 0x00, 		/* 0016: mov dx, 0 */
	0xcd, 0x25,			/* 0019: int 25h */
	0x5a,				/* 001b: pop dx */

	0x5f,				/* 001c: pop si */
	0x5e,				/* 001d: pop di */
	0x5a,				/* 001e: pop dx */
	0x59,				/* 001f: pop cx */
	0x5b,				/* 0020: pop bx */
	0x5d,				/* 0021: pop bp */
	0x07,				/* 0022: pop es */
	0x1f,				/* 0023: pop ds */
	0x72, 0x02,			/* 0024: jb $+2 */
	0x31, 0xc0,			/* 0027: xor ax,ax */
	0xcb				/* 0029: retf */
};
static int (far *doint25)__P_((VOID_A)) = NULL;
#endif
static int dos7access = 0;
#define	D7_DOSVER	017
#define	D7_CAPITAL	020


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
	dosmemput(path, strlen(path) + 1, __tb + offset);
	return(offset);
}
#endif

static long int21call(regp, sregp)
__dpmi_regs *regp;
struct SREGS *sregp;
{
	int n;

# ifdef	DJGPP
	(*regp).x.ds = (*sregp).ds;
	(*regp).x.es = (*sregp).es;
	__dpmi_int(0x21, regp);
	(*sregp).ds = (*regp).x.ds;
	(*sregp).es = (*regp).x.es;
# else
	int86x(0x21, regp, regp, sregp);
# endif
	n = ((*regp).x.flags & 1);
	if (!n) return((*regp).x.ax);
	seterrno((*regp).x.ax);
	return(-1);
}

int getcurdrv(VOID_A)
{
	return((u_char)bdos(0x19, 0, 0)
		+ ((dos7access & D7_CAPITAL) ? 'A' : 'a'));
}

int setcurdrv(drive)
int drive;
{
	union REGS reg;

	errno = EINVAL;
	if (drive >= 'a' && drive <= 'z') {
		drive -= 'a';
		dos7access &= ~D7_CAPITAL;
	}
	else {
		drive -= 'A';
		dos7access |= D7_CAPITAL;
	}
	if (drive < 0) return(-1);

	reg.x.ax = 0x0e00;
	reg.h.dl = drive;
	intdos(&reg, &reg);
	if (reg.h.al < drive) {
		seterrno(0x0f);		/* Invarid drive */
		return(-1);
	}

	return(0);
}

int getdosver(VOID_A)
{
	if (!(dos7access & D7_DOSVER))
		dos7access |= ((u_char)bdos(0x30, 0, 0) & D7_DOSVER);
	return(dos7access & D7_DOSVER);
}

int supportLFN(path)
char *path;
{
#ifdef	NOLFNEMU
	return(0);
#else	/* !NOLFNEMU */
	struct SREGS sreg;
	__dpmi_regs reg;
	char drv[4], buf[128];

	if (!path || !isalpha(drv[0] = path[0]) || path[1] != ':')
		drv[0] = getcurdrv();
	if (drv[0] >= 'A' && drv[0] <= 'Z') return(0);
	if (getdosver() < 7) {
# ifndef	_NODOSDRIVE
		if (dosdrive) return(-1);
# endif
		return(-2);
	}

	drv[1] = ':';
	drv[2] = _SC_;
	drv[3] = '\0';

	reg.x.ax = 0x71a0;
	reg.x.cx = sizeof(buf);
# ifdef	DJGPP
	dos_putpath(drv, 0);
	sreg.ds = __tb_segment;
	reg.x.dx = __tb_offset;
	sreg.es = sreg.ds;
	reg.x.di = __tb_offset + 4;
# else
	sreg.ds = FP_SEG(drv);
	reg.x.dx = FP_OFF(drv);
	sreg.es = FP_SEG(buf);
	reg.x.di = FP_OFF(buf);
# endif
	if (int21call(&reg, &sreg) < 0) return(-2);
	return((reg.x.bx & 0x4000) ? 1 : -2);
#endif	/* !NOLFNEMU */
}

char *getcurdir(pathname, drive)
char *pathname;
int drive;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = (supportLFN(NULL) > 0) ? 0x7147 : 0x4700;
	reg.h.dl = drive;
#ifdef	DJGPP
	sreg.ds = __tb_segment;
	reg.x.si = __tb_offset;
	if (int21call(&reg, &sreg) < 0) return(NULL);
	dosmemget(__tb, MAXPATHLEN, pathname);
#else
	sreg.ds = FP_SEG(pathname);
	reg.x.si = FP_OFF(pathname);
	if (int21call(&reg, &sreg) < 0) return(NULL);
#endif
	return(pathname);
}

#ifndef	_NODOSDRIVE
static char *regpath(path, buf)
char *path, *buf;
{
	if (isalpha(path[0]) && path[1] == ':') return(path);
	buf[0] = getcurdrv();
	buf[1] = ':';
	strcpy(&(buf[2]), path);
	return(buf);
}
#endif	/* !_NODOSDRIVE */

char *shortname(path, alias)
char *path, *alias;
{
#ifdef	NOLFNEMU
	return(path);
#else
	struct SREGS sreg;
	__dpmi_regs reg;
	int i;

	if ((i = supportLFN(path)) <= 0) {
# ifndef	_NODOSDRIVE
		if (i == -1) {
			char buf[MAXPATHLEN + 1];

			if (dosshortname(regpath(path, buf), alias))
				return(alias);
			else if (errno != EACCES) return(NULL);
		}
# endif	/* !_NODOSDRIVE */
		strcpy(alias, path);
		return(alias);
	}
	reg.x.ax = 0x7160;
	reg.x.cx = 0x8001;
# ifdef	DJGPP
	i = strlen(path) + 1;
	dos_putpath(path, 0);
	sreg.ds = __tb_segment;
	reg.x.si = __tb_offset;
	sreg.es = sreg.ds;
	reg.x.di = __tb_offset + i;
	if (int21call(&reg, &sreg) < 0) return(NULL);
	dosmemget(__tb + i, MAXPATHLEN, alias);
# else
	sreg.ds = FP_SEG(path);
	reg.x.si = FP_OFF(path);
	sreg.es = FP_SEG(alias);
	reg.x.di = FP_OFF(alias);
	if (int21call(&reg, &sreg) < 0) return(NULL);
# endif
	if (!path || !isalpha(i = path[0]) || path[1] != ':') i = getcurdrv();
	if (i >= 'a' && i <= 'z') *alias += 'a' - 'A';

	return(alias);
#endif	/* !NOLFNEMU */
}

char *unixrealpath(path, resolved)
char *path, *resolved;
{
# ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN + 1];
# endif
	int i;

#ifdef	NOLFNEMU
	_fixpath(path, resolved);
	for (i = 2; resolved[i]; i++) {
		if (resolved[i] == '/') resolved[i] = _SC_;
		else if (resolved[i] >= 'a' && resolved[i] <= 'z')
			resolved[i] -= 'a' - 'A';
	}
#else	/* !NOLFNEMU */
	struct SREGS sreg;
	__dpmi_regs reg;

	switch (supportLFN(path)) {
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
			if (doslongname(regpath(path, buf), resolved))
				return(resolved);
			else if (errno != EACCES) {
				strcpy(resolved, path);
				return(resolved);
			}
# endif	/* !_NODOSDRIVE */
		default:
			reg.x.ax = 0x6000;
			reg.x.cx = 0;
	}
# ifdef	DJGPP
	i = strlen(path) + 1;
	dos_putpath(path, 0);
	sreg.ds = __tb_segment;
	reg.x.si = __tb_offset;
	sreg.es = sreg.ds;
	reg.x.di = __tb_offset + i;
	if (int21call(&reg, &sreg) < 0) return(NULL);
	dosmemget(__tb + i, MAXPATHLEN, resolved);
# else
	sreg.ds = FP_SEG(path);
	reg.x.si = FP_OFF(path);
	sreg.es = FP_SEG(resolved);
	reg.x.di = FP_OFF(resolved);
	if (int21call(&reg, &sreg) < 0) return(NULL);
# endif
#endif	/* !NOLFNEMU */
	if (!path || !isalpha(i = path[0]) || path[1] != ':') i = getcurdrv();
	if (i >= 'a' && i <= 'z' && *resolved >= 'A' && *resolved <= 'Z')
		*resolved += 'a' - 'A';
	if (i >= 'A' && i <= 'Z' && *resolved >= 'a' && *resolved <= 'z')
		*resolved += 'A' - 'a';

	return(resolved);
}

char *preparefile(path, alias, iscreat)
char *path, *alias;
int iscreat;
{
#ifdef	NOLFNEMU
	return(path);
#else	/* !NOLFNEMU */
	struct SREGS sreg;
	__dpmi_regs reg;
	char *cp;

# ifdef	_NODOSDRIVE
	if (supportLFN(path) < 0) return(path);
	if ((cp = shortname(path, alias))) return(cp);
	if (!iscreat || errno != ENOENT) return(NULL);
# else	/* !_NODOSDRIVE */
	int i;

	if ((i = supportLFN(path)) < -1) return(path);
	if ((cp = shortname(path, alias))) return(cp);
	else if (i < 0 && errno == EACCES) return(path);
	if (!iscreat || errno != ENOENT) return(NULL);
	if (i == -1) {
		char buf[MAXPATHLEN + 1];

		i = dosopen(regpath(path, buf),
			O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (i < 0) return(NULL);
		dosclose(i);
		return(shortname(path, alias));
	}
# endif	/* !_NODOSDRIVE */

	reg.x.ax = 0x716c;
	reg.x.bx = 0x0111;	/* O_WRONLY | SH_DENYRW | NO_BUFFER */
	reg.x.cx = DS_IARCHIVE;
	reg.x.dx = 0x0012;	/* O_CREAT | O_TRUNC */
# ifdef	DJGPP
	dos_putpath(path, 0);
	sreg.ds = __tb_segment;
	reg.x.si = __tb_offset;
# else
	sreg.ds = FP_SEG(path);
	reg.x.si = FP_OFF(path);
# endif
	if (int21call(&reg, &sreg) < 0) return(NULL);

	reg.x.bx = reg.x.ax;
	reg.x.ax = 0x3e00;
	int21call(&reg, &sreg);

	return(shortname(path, alias));
#endif	/* !NOLFNEMU */
}

#ifdef	__GNUC__
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
		else if (path[i] >= 'a' && path[i] <= 'z')
			path[i] -= 'a' - 'A';
	}
	return(path);
}
#endif	/* __GNUC__ */

#ifndef	_NODOSDRIVE
/*ARGSUSED*/
int rawdiskio(drive, sect, buf, size, iswrite)
int drive;
long sect;
u_char *buf;
int size, iswrite;
{
#ifdef	DJGPP
	__dpmi_regs reg;
#endif
	struct iopacket_t pac;
	int n;

	if (drive >= 'a' && drive <= 'z') drive -= 'a';
	else drive -= 'A';
	if (drive < 0 || drive > 'z' - 'a') {
		errno = EINVAL;
		return(-1);
	}

#ifdef	DJGPP
	reg.x.ax = (iswrite) ? 0x3526 : 0x3525;
	__dpmi_int(0x21, &reg);
	reg.x.cs = reg.x.es;
	reg.x.ip = reg.x.bx;
#else
	int25bin[0x1a] = (iswrite) ? 0x26 : 0x25;
	doint25 = (int (far *)__P_((VOID_A)))int25bin;
#endif

	n = 0x0207;
	if (sect <= 0xffff) {
#ifdef	DJGPP
		reg.x.ax = drive;
		reg.x.cx = 1;
		reg.x.dx = (u_short)sect;
		reg.x.bx = __tb_offset + sizeof(struct iopacket_t);
		reg.x.ds = __tb_segment;
		reg.x.ss = reg.x.sp = 0;
		if (iswrite)
			dosmemput(buf, size, __tb + sizeof(struct iopacket_t));
		_go32_dpmi_simulate_fcall(&reg);
		n = (reg.x.flags & 1) ? reg.x.ax : 0;
#else
		int25bin[0x09] = (FP_SEG(buf) & 0xff);
		int25bin[0x0a] = ((FP_SEG(buf) >> 8) & 0xff);
		int25bin[0x0e] = (drive & 0xff);
		int25bin[0x0f] = ((drive >> 8) & 0xff);
		int25bin[0x11] = (FP_OFF(buf) & 0xff);
		int25bin[0x12] = ((FP_OFF(buf) >> 8) & 0xff);
		int25bin[0x14] = (1 & 0xff);
		int25bin[0x15] = ((1 >> 8) & 0xff);
		int25bin[0x17] = ((u_short)sect & 0xff);
		int25bin[0x18] = (((u_short)sect >> 8) & 0xff);
		n = doint25();
#endif
	}
	if (n) {
		if (n != 0x0207) {
			seterrno(n & 0xff);
			return(-1);
		}
		pac.sect = sect;
		pac.size = 1;
#ifdef	DJGPP
		pac.buf_off = __tb_offset + sizeof(struct iopacket_t);
		pac.buf_seg = __tb_segment;
		reg.x.ax = drive;
		reg.x.cx = 0xffff;
		reg.x.bx = __tb_offset;
		reg.x.ds = __tb_segment;
		reg.x.ss = reg.x.sp = 0;
		dosmemput(&pac, sizeof(struct iopacket_t), __tb);
		if (iswrite)
			dosmemput(buf, size, __tb + sizeof(struct iopacket_t));
		_go32_dpmi_simulate_fcall(&reg);
		n = (reg.x.flags & 1) ? reg.x.ax : 0;
#else
		pac.buf_off = FP_OFF(buf);
		pac.buf_seg = FP_SEG(buf);
		int25bin[0x09] = (FP_SEG(&pac) & 0xff);
		int25bin[0x0a] = ((FP_SEG(&pac) >> 8) & 0xff);
		int25bin[0x0e] = (drive & 0xff);
		int25bin[0x0f] = ((drive >> 8) & 0xff);
		int25bin[0x11] = (FP_OFF(&pac) & 0xff);
		int25bin[0x12] = ((FP_OFF(&pac) >> 8) & 0xff);
		int25bin[0x14] = (0xffff & 0xff);
		int25bin[0x15] = ((0xffff >> 8) & 0xff);
		n = doint25();
#endif
		if (n) {
			seterrno(n & 0xff);
			return(-1);
		}
	}
#ifdef	DJGPP
	if (!iswrite) dosmemget(__tb + sizeof(struct iopacket_t), size, buf);
#endif
	return(0);
}
#endif	/* !_NODOSDRIVE */

#ifndef	NOLFNEMU
static int dos_findfirst(path, result, attr)
char *path;
struct dosfind_t *result;
u_short attr;
{
	struct SREGS sreg;
	__dpmi_regs reg;
# ifdef	DJGPP
	int i;

	i = strlen(path) + 1;
	sreg.ds = __tb_segment;
	reg.x.dx = __tb_offset + i;
# else
	sreg.ds = FP_SEG(result);
	reg.x.dx = FP_OFF(result);
# endif
	reg.x.ax = 0x1a00;
	int21call(&reg, &sreg);

	reg.x.ax = 0x4e00;
	reg.x.cx = attr;
# ifdef	DJGPP
	dos_putpath(path, 0);
	sreg.ds = __tb_segment;
	reg.x.dx = __tb_offset;
	if (int21call(&reg, &sreg) < 0) return(-1);
	dosmemget(__tb + i, sizeof(struct dosfind_t), result);
# else
	sreg.ds = FP_SEG(path);
	reg.x.dx = FP_OFF(path);
	if (int21call(&reg, &sreg) < 0) return(-1);
# endif
	return(0);
}

static int dos_findnext(result)
struct dosfind_t *result;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x1a00;
# ifdef	DJGPP
	sreg.ds = __tb_segment;
	reg.x.dx = __tb_offset;
	int21call(&reg, &sreg);
	dosmemput(result, sizeof(struct dosfind_t), __tb);
# else
	sreg.ds = FP_SEG(result);
	reg.x.dx = FP_OFF(result);
	int21call(&reg, &sreg);
# endif

	reg.x.ax = 0x4f00;
	if (int21call(&reg, &sreg) < 0) return(-1);
# ifdef	DJGPP
	dosmemget(__tb, sizeof(struct dosfind_t), result);
# endif
	return(0);
}

static long lfn_findfirst(path, result, attr)
char *path;
struct lfnfind_t *result;
u_short attr;
{
# ifdef	DJGPP
	int i;
# endif
	struct SREGS sreg;
	__dpmi_regs reg;
	long fd;

	reg.x.ax = 0x714e;
	reg.x.cx = attr;
	reg.x.si = DATETIMEFORMAT;
# ifdef	DJGPP
	i = strlen(path) + 1;
	dos_putpath(path, 0);
	sreg.ds = __tb_segment;
	reg.x.dx = __tb_offset;
	sreg.es = sreg.ds;
	reg.x.di = __tb_offset + i;
	if ((fd = int21call(&reg, &sreg)) < 0) return(-1L);
	dosmemget(__tb + i, sizeof(struct lfnfind_t), result);
# else
	sreg.ds = FP_SEG(path);
	reg.x.dx = FP_OFF(path);
	sreg.es = FP_SEG(result);
	reg.x.di = FP_OFF(result);
	if ((fd = int21call(&reg, &sreg)) < 0) return(-1L);
# endif
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
# ifdef	DJGPP
	sreg.es = __tb_segment;
	reg.x.di = __tb_offset;
	if (int21call(&reg, &sreg) < 0) return(-1);
	dosmemget(__tb, sizeof(struct lfnfind_t), result);
# else
	sreg.es = FP_SEG(result);
	reg.x.di = FP_OFF(result);
	if (int21call(&reg, &sreg) < 0) return(-1);
# endif
	return(0);
}

static int lfn_findclose(fd)
u_short fd;
{
	struct SREGS sreg;
	__dpmi_regs reg;

	reg.x.ax = 0x71a1;
	reg.x.bx = fd;
	return((int21call(&reg, &sreg) < 0) ? -1 : 0);
}

DIR *unixopendir(dir)
char *dir;
{
	DIR *dirp;
	char path[MAXPATHLEN + 1];
	long fd;
	int i;
# ifndef	_NODOSDRIVE
	if (supportLFN(dir) == -1) {
		if ((dirp = dosopendir(regpath(dir, path)))) return(dirp);
		else if (errno != EACCES) return(NULL);
	}
# endif

	i = strlen(dir);
	strcpy(path, dir);
	if (i && path[i - 1] != _SC_) strcat(path, _SS_);

	if (!(dirp = (DIR *)malloc(sizeof(DIR)))) return(NULL);
	dirp -> dd_off = 0;

	if (supportLFN(path) <= 0) {
		dirp -> dd_id = DID_IFNORMAL;
		strcat(path, "*.*");
		dirp -> dd_buf = (char *)malloc(sizeof(struct dosfind_t));
		if (!dirp -> dd_buf) {
			free(dirp);
			return(NULL);
		}
		i = dos_findfirst(path,
			(struct dosfind_t *)dirp -> dd_buf, DS_IFLABEL);
		if (i >= 0) dirp -> dd_id |= DID_IFLABEL;
		else i = dos_findfirst(path,
			(struct dosfind_t *)dirp -> dd_buf,
			(SEARCHATTRS | DS_IFLABEL));
	}
	else {
		dirp -> dd_id = DID_IFLFN;
		strcat(path, "*");
		dirp -> dd_buf = (char *)malloc(sizeof(struct lfnfind_t));
		if (!dirp -> dd_buf) {
			free(dirp);
			return(NULL);
		}
		i = dos_findfirst(path,
			(struct dosfind_t *)dirp -> dd_buf, DS_IFLABEL);
		if (i >= 0) dirp -> dd_id |= DID_IFLABEL;
		else {
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

	if (i < 0
	|| !(dirp -> dd_path = (char *)malloc(strlen(dir) + 1))) {
		if (!errno || errno == ENOENT) dirp -> dd_off = -1;
		else {
			if (dirp -> dd_id & DID_IFLFN)
				lfn_findclose(dirp -> dd_fd);
			dirp -> dd_id &= ~DID_IFLFN;
			free(dirp -> dd_buf);
			free(dirp);
			return(NULL);
		}
	}
	strcpy(dirp -> dd_path, dir);
	return(dirp);
}

int unixclosedir(dirp)
DIR *dirp;
{
# ifndef	_NODOSDRIVE
	if (dirp -> dd_id < 0) return(dosclosedir(dirp));
# endif
	if (dirp -> dd_id & DID_IFLFN) lfn_findclose(dirp -> dd_fd);
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
	long fd;
	int i;

# ifndef	_NODOSDRIVE
	if (dirp -> dd_id < 0) return(dosreaddir(dirp));
# endif
	if (dirp -> dd_off < 0) return(NULL);
	d.d_off = dirp -> dd_off;

	if (!(dirp -> dd_id & DID_IFLFN) || (dirp -> dd_id & DID_IFLABEL)) {
		struct dosfind_t *bufp;

		bufp = (struct dosfind_t *)(dirp -> dd_buf);
		strcpy(d.d_name, bufp -> name);
		d.d_alias[0] = '\0';
	}
	else {
		struct lfnfind_t *bufp;

		bufp = (struct lfnfind_t *)(dirp -> dd_buf);
		strcpy(d.d_name, bufp -> name);
		strcpy(d.d_alias, bufp -> alias);
	}

	if (!(dirp -> dd_id & DID_IFLABEL)) {
		if (!(dirp -> dd_id & DID_IFLFN))
			i = dos_findnext((struct dosfind_t *)(dirp -> dd_buf));
		else i = lfn_findnext(dirp -> dd_fd,
			(struct lfnfind_t *)(dirp -> dd_buf));
	}
	else {
		i = strlen(dirp -> dd_path);
		strcpy(path, dirp -> dd_path);
		if (i && path[i - 1] != _SC_) path[i++] =  _SC_;

		if (!(dirp -> dd_id & DID_IFLFN)) {
			strcpy(path + i, "*.*");
			i = dos_findfirst(path,
				(struct dosfind_t *)dirp -> dd_buf,
				(SEARCHATTRS | DS_IFLABEL));
		}
		else {
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
	}
	if (i >= 0) dirp -> dd_off++;
	else {
		dirp -> dd_off = -1;
		if (dirp -> dd_id & DID_IFLABEL) dirp -> dd_id &= ~DID_IFLFN;
	}
	dirp -> dd_id &= ~DID_IFLABEL;

	return(&d);
}

int unixrewinddir(dirp)
DIR *dirp;
{
	DIR *dupdirp;
	char *cp;

# ifndef	_NODOSDRIVE
	if (dirp -> dd_id < 0) return(dosrewinddir(dirp));
# endif
	if (dirp -> dd_id & DID_IFLFN) lfn_findclose(dirp -> dd_fd);
	if (!(dupdirp = unixopendir(dirp -> dd_path))) return(-1);

	free(dirp -> dd_buf);
	free(dupdirp -> dd_path);
	cp = dirp -> dd_path;
	memcpy(dirp, dupdirp, sizeof(DIR));
	dirp -> dd_path = cp;
	free(dupdirp);

	return(0);
}

int unixrename(from, to)
char *from, *to;
{
	struct SREGS sreg;
	__dpmi_regs reg;
	int f, t;

	f = supportLFN(from);
	t = supportLFN(to);
# ifndef	_NODOSDRIVE
	if ((f == -1 || t == -1) && (f != -2 && t != -2)) {
		char buf1[MAXPATHLEN + 1], buf2[MAXPATHLEN + 1];

		if (dosrename(regpath(from, buf1), regpath(to, buf2)) >= 0)
			return(0);
		else if (errno != EACCES) return(-1);
	}
# endif	/* !_NODOSDRIVE */
	reg.x.ax = ((f > 0 || t > 0) && (f >= 0 && t >= 0)) ? 0x7156 : 0x5600;
# ifdef	DJGPP
	dos_putpath(from, 0);
	t = strlen(from) + 1;
	dos_putpath(to, t);
	sreg.ds = __tb_segment;
	reg.x.dx = __tb_offset;
	sreg.es = sreg.ds;
	reg.x.di = __tb_offset + t;
# else
	sreg.ds = FP_SEG(from);
	reg.x.dx = FP_OFF(from);
	sreg.es = FP_SEG(to);
	reg.x.di = FP_OFF(to);
# endif
	return((int21call(&reg, &sreg) < 0) ? -1 : 0);
}

/*ARGSUSED*/
int unixmkdir(path, mode)
char *path;
int mode;
{
	struct SREGS sreg;
	__dpmi_regs reg;
# ifndef	_NODOSDRIVE
	int i;

	if ((i = supportLFN(path)) == -1) {
		char buf[MAXPATHLEN + 1];

		if (dosmkdir(regpath(path, buf), mode) >= 0) return(0);
		else if (errno != EACCES) return(-1);
	}
	reg.x.ax = (i > 0) ? 0x7139 : 0x3900;
# else	/* !_NODOSDRIVE */
	reg.x.ax = (supportLFN(path) > 0) ? 0x7139 : 0x3900;
# endif	/* !_NODOSDRIVE */
# ifdef	DJGPP
	dos_putpath(path, 0);
	sreg.ds = __tb_segment;
	reg.x.dx = __tb_offset;
# else
	sreg.ds = FP_SEG(path);
	reg.x.dx = FP_OFF(path);
# endif
	return((int21call(&reg, &sreg) < 0) ? -1 : 0);
}
#endif	/* !NOLFNEMU */

char *unixgetcwd(pathname, size)
char *pathname;
int size;
{
#ifdef	NOLFNEMU
	if (getcwd(pathname, size)) adjustfname(pathname + 2);
	else pathname = NULL;
#else	/* !NOLFNEMU */
# ifdef	DJGPP
	char tmp[MAXPATHLEN + 1];
# endif

	if (!pathname && !(pathname = (char *)malloc(size))) return(NULL);

	pathname[0] = getcurdrv();
	pathname[1] = ':';
	pathname[2] = _SC_;

	if (!getcurdir(pathname + 3, 0)) return(NULL);
#endif	/* !NOLFNEMU */
	if (!(dos7access & D7_CAPITAL) && *pathname >= 'A' && *pathname <= 'Z')
		*pathname += 'a' - 'A';
	if ((dos7access & D7_CAPITAL) && *pathname >= 'a' && *pathname <= 'z')
		*pathname += 'A' - 'a';
#if	!defined (NOLFNEMU) && defined (DJGPP)
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
	if ((mode & S_IFMT) == S_IFDIR) attr |= DS_IFDIR;
	else if ((mode & S_IFMT) == S_IFIFO) attr |= DS_IFLABEL;
	else if ((mode & S_IFMT) == S_IFSOCK) attr |= DS_IFSYSTEM;

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

int unixstat(path, status)
char *path;
struct stat *status;
{
	long fd;
	int i;
#ifdef	NOLFNEMU
	struct ffblk dbuf;

	fd = -1;
	if ((i = findfirst(path, &dbuf, SEARCHATTRS)) != 0) {
		if (errno != ENMFILE) return(-1);
		if (!strcmp(path, ".."))
			i = findfirst(".", &dbuf, SEARCHATTRS);
		else i = dos_findfirst(path, &dbuf, DS_IFLABEL);
	}
	if (i < 0) {
		if (!errno || errno == ENMFILE) errno = ENOENT;
		return(-1);
	}

	status -> st_mode = getdosmode(dbuf.ff_attrib);
	status -> st_mtime = getdostime(dbuf.ff_fdate, dbuf.ff_ftime);
	status -> st_size = dbuf.ff_fsize;
#else	/* !NOLFNEMU */
	struct dosfind_t dbuf;
	struct lfnfind_t lbuf;
# ifdef	_NODOSDRIVE

	if (supportLFN(path) <= 0) {
# else	/* !_NODOSDRIVE */
	if ((i = supportLFN(path)) == -1) {
		char buf[MAXPATHLEN + 1];

		if (dosstat(regpath(path, buf), status) >= 0) return(0);
		else if (errno != EACCES) return(-1);
	}
	if (i <= 0) {
# endif	/* !_NODOSDRIVE */
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
	if (i < 0) {
		if (!errno) errno = ENOENT;
		return(-1);
	}

	if (fd < 0) {
		status -> st_mode = getdosmode(dbuf.attr);
		status -> st_mtime = getdostime(dbuf.wrdate, dbuf.wrtime);
		status -> st_size = dbuf.size_l;
	}
	else {
		status -> st_mode = getdosmode(lbuf.attr);
		status -> st_mtime = getdostime(lbuf.wrdate, lbuf.wrtime);
		status -> st_size = lbuf.size_l;
		lfn_findclose(fd);
	}
#endif	/* !NOLFNEMU */
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
	__dpmi_regs reg;
#ifdef	NOLFNEMU

	reg.x.ax = 0x4301;
	reg.x.cx = putdosmode(mode);
	reg.x.dx = (unsigned)path;
	intdos(&reg, &reg);
	if (reg.x.cflag) {
		seterrno((u_short)(reg.x.ax));
		return(-1);
	}
	return(0);
#else	/* !NOLFNEMU */
	struct SREGS sreg;
# ifndef	_NODOSDRIVE
	char buf[MAXPATHLEN + 1];
	int i;

	if ((i = supportLFN(path)) == -1) path = preparefile(path, buf, 0);
	if (i <= 0) reg.x.ax = 0x4301;
# else	/* !_NODOSDRIVE */
	if (supportLFN(path) <= 0) reg.x.ax = 0x4301;
# endif	/* !_NODOSDRIVE */
	else {
		reg.x.ax = 0x7143;
		reg.h.bl = 0x01;
	}
	reg.x.cx = putdosmode(mode);
# ifdef	DJGPP
	dos_putpath(path, 0);
	sreg.ds = __tb_segment;
	reg.x.dx = __tb_offset;
# else
	sreg.ds = FP_SEG(path);
	reg.x.dx = FP_OFF(path);
# endif
	return((int21call(&reg, &sreg) < 0) ? -1 : 0);
#endif	/* !NOLFNEMU */
}
