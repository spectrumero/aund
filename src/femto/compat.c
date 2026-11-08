/*-
 * Copyright (c) 2026 Dylan Smith
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * This is part of aund, an implementation of Acorn Universal
 * Networking for Unix.
 *
 * compat.c - the POSIX functions aund needs, on the FileStick.
 *
 * The FileStick runs FatFS, through a small set of system calls, and a
 * newlib C library with libgloss system call stubs. This file fills the
 * gaps: things FatFS can't do (directories as files, permissions,
 * removing a directory holding a metadata file) and things the system
 * doesn't provide (rmdir, rename, statvfs, err/warn).
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "femto_compat.h"
#include "meta.h"

/* The FileStick's own opendir() and friends, not the DIR * wrappers */
#undef opendir
#undef readdir
#undef closedir

/* Defined by aund (fs_util.c) */
extern mode_t fs_access_to_mode(unsigned char, int);
extern uint8_t fs_mode_to_access(mode_t);

/* System call numbers, from the FileStick's system/syscalls.inc */
#define SYS_fstat	80
#define SYS_rename	1034
#define SYS_stat	1038

/* FileStick system errors (system/cust_errno.h) */
#define EBADSYSCALL	2000
#define EFATFS_START	2001
/* The FatFS FRESULT codes that fatfs_to_errno() doesn't translate */
#define FR_INT_ERR	2
#define FR_NOT_READY	3
#define FR_INVALID_NAME	6
#define FR_DENIED	7
#define FR_INVALID_OBJECT 9
#define FR_INVALID_DRIVE 11
#define FR_NOT_ENABLED	12
#define FR_NO_FILESYSTEM 13
#define FR_LOCKED	16
#define FR_INVALID_PARAMETER 19

/*
 * Turn the errno values the FileStick makes up for FatFS errors into
 * ones aund knows how to report to a client.
 */
static void
fix_errno(void)
{
	switch (errno) {
	case EBADSYSCALL:
		errno = ENOSYS; break;
	case EFATFS_START + FR_INVALID_NAME:
		errno = ENAMETOOLONG; break;	/* "Bad file name" */
	case EFATFS_START + FR_DENIED:
		errno = EACCES; break;
	case EFATFS_START + FR_LOCKED:
		errno = EBUSY; break;
	case EFATFS_START + FR_INVALID_PARAMETER:
		errno = EINVAL; break;
	case EFATFS_START + FR_INT_ERR:
	case EFATFS_START + FR_NOT_READY:
	case EFATFS_START + FR_INVALID_OBJECT:
	case EFATFS_START + FR_INVALID_DRIVE:
	case EFATFS_START + FR_NOT_ENABLED:
	case EFATFS_START + FR_NO_FILESYSTEM:
		errno = EIO; break;
	}
}

static long
femto_syscall(long num, long arg0, long arg1)
{
	register long a7 asm("a7") = num;
	register long a0 asm("a0") = arg0;
	register long a1 asm("a1") = arg1;

	asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
	if (a0 < 0) {
		errno = -a0;
		fix_errno();
		return -1;
	}
	return a0;
}

/*
 * stat() and fstat() go straight to the system. The libgloss versions
 * expect the system to fill in a Linux struct kernel_stat, and convert
 * from that, but the FileStick system fills in a newlib struct stat, so
 * through libgloss st_mode and st_size come out wrong.
 *
 * FatFS has only a read-only attribute for permissions, so the Acorn
 * access is filled in from the metadata file.
 */
int
femto_stat_meta(const char *path, struct stat *sb, struct femto_meta *meta)
{

	if (femto_syscall(SYS_stat, (long)path, (long)sb) < 0)
		return -1;
	if (femto_meta_get(path, meta) != 0)
		memset(meta, 0, sizeof(*meta));
	femto_meta_mode(sb, meta);
	return 0;
}

int
femto_stat(const char *path, struct stat *sb)
{
	struct femto_meta meta;

	return femto_stat_meta(path, sb, &meta);
}

int
femto_fstat(int fd, struct stat *sb)
{

	if (fd == FEMTO_DIRFD) {
		memset(sb, 0, sizeof(*sb));
		sb->st_mode = S_IFDIR | S_IRUSR | S_IWUSR;
		return 0;
	}
	if (femto_syscall(SYS_fstat, fd, (long)sb) < 0)
		return -1;
	return 0;
}

/*
 * open(), with POSIX meanings for O_CREAT and O_EXCL, and the ability
 * to open a directory.
 *
 * The FileStick system maps O_CREAT to FatFS FA_CREATE_ALWAYS, so
 * O_CREAT on its own truncates an existing file. Here, O_CREAT without
 * O_TRUNC opens an existing file as it is.
 */
int
femto_open(const char *path, int flags, ...)
{
	struct stat sb;
	int fd;

	if ((flags & O_CREAT) && !(flags & O_TRUNC)) {
		fd = open(path, flags & ~(O_CREAT | O_EXCL));
		if (fd >= 0) {
			if (flags & O_EXCL) {
				close(fd);
				errno = EEXIST;
				return -1;
			}
			return fd;
		}
		fix_errno();
		if (errno != ENOENT)
			return -1;
		/* Doesn't exist yet: fall through to create it */
	} else if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL) &&
	    femto_syscall(SYS_stat, (long)path, (long)&sb) == 0) {
		errno = EEXIST;
		return -1;
	}

	fd = open(path, flags & ~O_EXCL);
	if (fd >= 0)
		return fd;
	fix_errno();
	if ((flags & O_ACCMODE) == O_RDONLY && !(flags & O_CREAT)) {
		int saved = errno;
		if (femto_syscall(SYS_stat, (long)path, (long)&sb) == 0 &&
		    S_ISDIR(sb.st_mode))
			return FEMTO_DIRFD;
		errno = saved;
	}
	return -1;
}

int
femto_close(int fd)
{

	if (fd == FEMTO_DIRFD)
		return 0;
	if (close(fd) < 0) {
		fix_errno();
		return -1;
	}
	return 0;
}

/* A short write to a file means the disc is full */
ssize_t
femto_write(int fd, const void *buf, size_t len)
{
	ssize_t done;

	done = write(fd, buf, len);
	if (done < 0) {
		fix_errno();
		return -1;
	}
	if ((size_t)done < len) {
		errno = ENOSPC;
		return -1;
	}
	return done;
}

int
femto_unlink(const char *path)
{
	struct stat sb;

	/* f_unlink() removes empty directories too, unlink() mustn't */
	if (femto_syscall(SYS_stat, (long)path, (long)&sb) == 0 &&
	    S_ISDIR(sb.st_mode)) {
		errno = EISDIR;
		return -1;
	}
	if (unlink(path) < 0) {
		fix_errno();
		return -1;
	}
	return 0;
}

int
femto_mkdir(const char *path, mode_t mode)
{

	if (mkdir(path, mode) < 0) {
		fix_errno();
		if (errno == EACCES)
			errno = ENOSPC;	/* FR_DENIED: directory full */
		return -1;
	}
	return 0;
}

/*
 * rmdir(): a directory that aund has stored metadata in holds the
 * metadata file, which has to go first. Only remove it if it's all
 * that's there.
 */
int
femto_rmdir(const char *path)
{
	struct stat sb;
	struct dirent *d;
	DIR *dir;
	char *metapath;
	int has_meta = 0;

	if (femto_syscall(SYS_stat, (long)path, (long)&sb) < 0)
		return -1;
	if (!S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		return -1;
	}
	if ((dir = femto_opendir(path)) == NULL)
		return -1;
	while ((d = femto_readdir(dir)) != NULL) {
		if (strcmp(d->d_name, FEMTO_META_FILE) == 0) {
			has_meta = 1;
			continue;
		}
		femto_closedir(dir);
		errno = ENOTEMPTY;
		return -1;
	}
	femto_closedir(dir);

	if (has_meta) {
		metapath = malloc(strlen(path) + sizeof(FEMTO_META_FILE) + 1);
		if (metapath == NULL) {
			errno = ENOMEM;
			return -1;
		}
		sprintf(metapath, "%s/%s", path, FEMTO_META_FILE);
		unlink(metapath);
		free(metapath);
	}
	if (unlink(path) < 0) {
		fix_errno();
		return -1;
	}
	return 0;
}

/*
 * rename(), using the system call added for aund. The metadata record
 * goes with the object.
 */
int
femto_rename(const char *from, const char *to)
{
	struct femto_meta meta;
	int has_meta;

	has_meta = femto_meta_get(from, &meta) == 0;
	if (femto_syscall(SYS_rename, (long)from, (long)to) < 0)
		return -1;
	if (has_meta) {
		femto_meta_del(from);
		femto_meta_set(to, &meta);
	}
	return 0;
}

/* chmod(): FatFS has no permissions, so the access goes in the metadata */
int
femto_chmod(const char *path, mode_t mode)
{
	struct femto_meta meta;
	struct stat sb;

	if (femto_syscall(SYS_stat, (long)path, (long)&sb) < 0)
		return -1;
	meta.flags = FEMTO_META_ACCESS;
	meta.access = fs_mode_to_access(mode & ~S_IFMT);
	return femto_meta_set(path, &meta);
}

/*
 * ftruncate(): there's no system call for this, so it can only make a
 * file longer, which it does by writing zeros.
 */
int
femto_ftruncate(int fd, off_t len)
{
	static const char zeros[64];
	off_t pos, size;
	size_t n;

	if ((pos = lseek(fd, 0, SEEK_CUR)) < 0 ||
	    (size = lseek(fd, 0, SEEK_END)) < 0) {
		fix_errno();
		return -1;
	}
	if (len < size) {
		lseek(fd, pos, SEEK_SET);
		errno = ENOSYS;
		return -1;
	}
	while (size < len) {
		n = len - size > sizeof(zeros) ? sizeof(zeros) : len - size;
		if (femto_write(fd, zeros, n) < 0) {
			lseek(fd, pos, SEEK_SET);
			return -1;
		}
		size += n;
	}
	if (lseek(fd, pos, SEEK_SET) < 0) {
		fix_errno();
		return -1;
	}
	return 0;
}

int
statvfs(const char *path, struct statvfs *buf)
{

	errno = ENOSYS;
	return -1;
}

/*
 * dirname(3), which newlib-nano lacks. Like the POSIX one, it may
 * modify its argument and return a pointer into it.
 */
char *
dirname(char *path)
{
	char *p;

	if (path == NULL || *path == '\0')
		return ".";
	p = path + strlen(path) - 1;
	while (p > path && *p == '/')		/* trailing slashes */
		p--;
	while (p > path && *p != '/')		/* the leaf */
		p--;
	if (*p != '/')
		return ".";
	while (p > path && *p == '/')		/* slashes before it */
		p--;
	p[1] = '\0';
	return path;
}

/*
 * Directories
 */
struct femto_dir {
	int	handle;
};

DIR *
femto_opendir(const char *path)
{
	DIR *d;
	int h;

	if ((h = opendir(path)) < 0) {
		fix_errno();
		return NULL;
	}
	if ((d = malloc(sizeof(*d))) == NULL) {
		closedir(h);
		errno = ENOMEM;
		return NULL;
	}
	d->handle = h;
	return d;
}

struct dirent *
femto_readdir(DIR *d)
{

	return readdir(d->handle);
}

int
femto_closedir(DIR *d)
{
	int rc;

	rc = closedir(d->handle);
	free(d);
	return rc;
}

/*
 * strerror() for the errors that can come from the FileStick system or
 * from this file. newlib's has the whole table, which costs 3K. aund
 * maps the common ones (ENOENT, EACCES...) to Acorn errors itself
 * (fs_error.c), so these are the rest.
 */
char *
femto_strerror(int code)
{
	static char buf[sizeof("Error 2147483647")];
	static const struct {
		int code;
		const char *msg;
	} msgs[] = {
		{ EEXIST,	"Already exists" },
		{ EBUSY,	"Object in use" },
		{ EINVAL,	"Bad parameter" },
		{ EISDIR,	"Is a directory" },
		{ EBADF,	"Channel" },
		{ EMSGSIZE,	"Too big" },
		{ ETIMEDOUT,	"No reply" },
		{ EHOSTUNREACH,	"Not listening" },
		{ ENOSYS,	"Not supported by FileStick" },
	};
	size_t i;

	for (i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++)
		if (msgs[i].code == code)
			return (char *)msgs[i].msg;
	snprintf(buf, sizeof(buf), "Error %d", code);
	return buf;
}

/*
 * err(3). There's no program name to print, aund is the only thing
 * running.
 */
static void
vwarnc(int code, const char *fmt, va_list ap)
{

	fputs("aund: ", stderr);
	if (fmt != NULL) {
		vfprintf(stderr, fmt, ap);
		if (code)
			fputs(": ", stderr);
	}
	if (code)
		fputs(femto_strerror(code), stderr);
	fputc('\n', stderr);
}

void
warn(const char *fmt, ...)
{
	va_list ap;
	int code = errno;

	va_start(ap, fmt);
	vwarnc(code, fmt, ap);
	va_end(ap);
}

void
warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnc(0, fmt, ap);
	va_end(ap);
}

void
err(int status, const char *fmt, ...)
{
	va_list ap;
	int code = errno;

	va_start(ap, fmt);
	vwarnc(code, fmt, ap);
	va_end(ap);
	exit(status);
}

void
errx(int status, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnc(0, fmt, ap);
	va_end(ap);
	exit(status);
}
