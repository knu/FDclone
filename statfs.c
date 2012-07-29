/*
 *	statfs.c
 *
 *	file system information module
 */

#include "headers.h"
#include "fsinfo.h"


#ifdef	USEFSTATFS
int Xstatfs(path, buf)
CONST char *path;
statfs_t *buf;
{
	int n, fd;

	if ((fd = open(path, O_RDONLY, 0666)) < 0) return(-1);
	n = fstatfs(fd, buf);
	close(fd);

	return(n);
}
#endif	/* USEFSTATFS */
