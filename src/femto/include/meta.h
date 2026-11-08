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
 * Acorn metadata for aund on the FileStick.
 *
 * Unix aund keeps each object's load and exec addresses in a symbolic
 * link in a .Acorn directory beside it, and its access in the Unix
 * permission bits. FatFS has neither symbolic links nor permissions, so
 * the femto build keeps both in one small file per directory,
 * FEMTO_META_FILE, holding an array of fixed size records:
 *
 *	offset	size
 *	0	22	leaf name, NUL padded (an empty name is a free slot)
 *	22	4	load address, little endian
 *	26	4	exec address, little endian
 *	30	1	Acorn access byte (EC_FS_ACCESS_*)
 *	31	1	flags: FEMTO_META_ADDRS, FEMTO_META_ACCESS
 *
 * One file per directory, rather than one per object, matters on FAT:
 * every file takes at least a whole cluster.
 */
#ifndef FEMTO_META_H
#define FEMTO_META_H

#include <sys/stat.h>
#include <stdint.h>

#define FEMTO_META_FILE		".aundmeta"
#define FEMTO_META_NAMELEN	22	/* including the terminating NUL */

#define FEMTO_META_ADDRS	0x01	/* load_addr and exec_addr are set */
#define FEMTO_META_ACCESS	0x02	/* access is set */

struct femto_meta {
	uint8_t	load_addr[4];
	uint8_t	exec_addr[4];
	uint8_t	access;
	uint8_t	flags;
};

/* Read the metadata for path. Returns 0, or -1 if there is none. */
int	femto_meta_get(const char *path, struct femto_meta *);

/*
 * Update the metadata for path. Only the parts named in the flags of
 * the struct passed in are changed; the rest of any existing record is
 * kept. Returns 0, or -1 with errno set.
 */
int	femto_meta_set(const char *path, const struct femto_meta *);

/* Remove the metadata for path. */
void	femto_meta_del(const char *path);

/*
 * Call fn for every record in the metadata file of directory dir, in
 * file order.
 */
void	femto_meta_scan(const char *dir,
	    void (*fn)(void *ctx, const char *name, const struct femto_meta *),
	    void *ctx);

/*
 * Fill in the permission bits of a stat buffer: from the stored access
 * if there is one, or the Acorn default (WR/r for files) if not.
 */
void	femto_meta_mode(struct stat *, const struct femto_meta *);

#endif
