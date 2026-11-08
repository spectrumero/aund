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
 * statvfs() for aund on the FileStick. The system has no call to find
 * the free space on a volume, so statvfs() always fails with ENOSYS.
 */
#ifndef FEMTO_SYS_STATVFS_H
#define FEMTO_SYS_STATVFS_H

#include <sys/types.h>	/* fsblkcnt_t, fsfilcnt_t */

struct statvfs {
	unsigned long	f_bsize;
	unsigned long	f_frsize;
	fsblkcnt_t	f_blocks;
	fsblkcnt_t	f_bfree;
	fsblkcnt_t	f_bavail;
	fsfilcnt_t	f_files;
	fsfilcnt_t	f_ffree;
	fsfilcnt_t	f_favail;
	unsigned long	f_fsid;
	unsigned long	f_flag;
	unsigned long	f_namemax;
};

int	statvfs(const char *, struct statvfs *);

#endif
