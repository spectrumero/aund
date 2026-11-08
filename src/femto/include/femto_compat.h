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
 * femto_compat.h - POSIX compatibility for aund on the FileStick.
 *
 * This is force-included (gcc -include) into every aund source file in
 * a femto build. It redirects the POSIX calls that the FileStick
 * doesn't have, or that behave differently on FatFS, to the versions
 * in src/femto/compat.c.
 */
#ifndef FEMTO_COMPAT_H
#define FEMTO_COMPAT_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * Pseudo file descriptor returned by open() on a directory. FatFS can't
 * open a directory as a file, but aund opens directories to make
 * directory handles (and never reads them), so fstat() and close() on
 * this value behave as they would on a Unix directory descriptor.
 */
#define FEMTO_DIRFD		0x7ffffff0

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN		64
#endif

/* flock(): there is only one aund process, and no locking on FatFS. */
#ifndef LOCK_SH
#define LOCK_SH			1
#define LOCK_EX			2
#define LOCK_NB			4
#define LOCK_UN			8
#endif

#ifndef S_BLKSIZE
#define S_BLKSIZE		512
#endif

int	femto_stat(const char *, struct stat *);
int	femto_fstat(int, struct stat *);
int	femto_open(const char *, int, ...);
int	femto_close(int);
ssize_t	femto_write(int, const void *, size_t);
int	femto_unlink(const char *);
int	femto_mkdir(const char *, mode_t);
int	femto_rmdir(const char *);
int	femto_rename(const char *, const char *);
int	femto_chmod(const char *, mode_t);
int	femto_ftruncate(int, off_t);
char	*femto_strerror(int);

#ifndef FEMTO_COMPAT_IMPL
/*
 * Function-like macros so that "struct stat" and the like are left
 * alone. There are no symbolic links on FatFS, so lstat() is stat().
 */
#define stat(p, sb)		femto_stat(p, sb)
#define lstat(p, sb)		femto_stat(p, sb)
#define fstat(fd, sb)		femto_fstat(fd, sb)
#define open(...)		femto_open(__VA_ARGS__)
#define close(fd)		femto_close(fd)
#define write(fd, buf, len)	femto_write(fd, buf, len)
#define unlink(p)		femto_unlink(p)
#define mkdir(p, m)		femto_mkdir(p, m)
#define rmdir(p)		femto_rmdir(p)
#define rename(o, n)		femto_rename(o, n)
#define chmod(p, m)		femto_chmod(p, m)
#define ftruncate(fd, len)	femto_ftruncate(fd, len)
/*
 * There's no clock or time zone on the FileStick. gmtime() gives the
 * same answer as localtime() would, without pulling in the time zone
 * code (and sscanf) from newlib.
 */
#define localtime(t)		gmtime(t)
/* Only the errors that can happen on the FileStick have messages */
#define strerror(e)		femto_strerror(e)
#define fsync(fd)		femto_fsync(fd)
#define flock(fd, op)		femto_flock(fd, op)
#define umask(m)		femto_umask(m)
#endif

/* Writes reach the disc when the file is closed; nothing to do here */
static inline int femto_fsync(int fd) { (void)fd; return 0; }
/* Only one program runs at a time, so locking isn't needed */
static inline int femto_flock(int fd, int op) { (void)fd; (void)op; return 0; }
static inline mode_t femto_umask(mode_t m) { (void)m; return 022; }

#endif
