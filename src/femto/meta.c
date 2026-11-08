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
 * meta.c - Acorn metadata on FatFS. See include/meta.h for the format.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "femto_compat.h"
#include "meta.h"

/* Defined by aund (fs_util.c) */
extern mode_t fs_access_to_mode(unsigned char, int);

struct meta_rec {
	char		name[FEMTO_META_NAMELEN];
	struct femto_meta meta;
};

/*
 * Split path into the metadata file for its directory (returned, to be
 * freed by the caller) and its leaf name.
 */
static char *
meta_path(const char *path, const char **leafp)
{
	const char *slash, *dir;
	size_t dirlen;
	char *mp;

	if ((slash = strrchr(path, '/')) != NULL) {
		dir = path;
		dirlen = slash - path;
		*leafp = slash + 1;
		if (dirlen == 0) {	/* "/name" */
			dir = "/";
			dirlen = 1;
		}
	} else {
		dir = ".";
		dirlen = 1;
		*leafp = path;
	}
	if ((mp = malloc(dirlen + sizeof(FEMTO_META_FILE) + 1)) == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	memcpy(mp, dir, dirlen);
	mp[dirlen] = '/';
	strcpy(mp + dirlen + 1, FEMTO_META_FILE);
	return mp;
}

static int
read_rec(int fd, struct meta_rec *rec)
{

	if (read(fd, rec, sizeof(*rec)) != sizeof(*rec))
		return 0;
	rec->name[FEMTO_META_NAMELEN - 1] = '\0';
	return 1;
}

/*
 * Find leaf in an open metadata file. Returns the offset of its record
 * (which is left in *rec), or -1. *freep is set to the offset of the
 * first free record, which may be the end of the file.
 */
static off_t
meta_find(int fd, const char *leaf, struct meta_rec *rec, off_t *freep)
{
	off_t off;

	*freep = -1;
	for (off = 0; read_rec(fd, rec); off += sizeof(*rec)) {
		if (rec->name[0] == '\0') {
			if (*freep < 0)
				*freep = off;
		} else if (strcasecmp(rec->name, leaf) == 0) {
			return off;
		}
	}
	if (*freep < 0)
		*freep = off;
	return -1;
}

int
femto_meta_get(const char *path, struct femto_meta *meta)
{
	struct meta_rec rec;
	const char *leaf;
	char *mp;
	off_t freeoff, off = -1;
	int fd;

	if ((mp = meta_path(path, &leaf)) == NULL)
		return -1;
	if (*leaf && strlen(leaf) < FEMTO_META_NAMELEN &&
	    (fd = open(mp, O_RDONLY)) >= 0) {
		off = meta_find(fd, leaf, &rec, &freeoff);
		close(fd);
	}
	free(mp);
	if (off < 0)
		return -1;
	*meta = rec.meta;
	return 0;
}

int
femto_meta_set(const char *path, const struct femto_meta *meta)
{
	struct meta_rec rec;
	const char *leaf;
	char *mp;
	off_t freeoff, off;
	int fd, ret = -1;

	if ((mp = meta_path(path, &leaf)) == NULL)
		return -1;
	if (!*leaf || strlen(leaf) >= FEMTO_META_NAMELEN) {
		errno = ENAMETOOLONG;
		goto out;
	}
	/*
	 * The system's O_CREAT truncates, so only use it if there isn't
	 * a metadata file yet.
	 */
	if ((fd = open(mp, O_RDWR)) < 0 &&
	    (fd = open(mp, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0)
		goto out;
	off = meta_find(fd, leaf, &rec, &freeoff);
	if (off < 0) {
		off = freeoff;
		memset(&rec, 0, sizeof(rec));
		strcpy(rec.name, leaf);
	}
	if (meta->flags & FEMTO_META_ADDRS) {
		memcpy(rec.meta.load_addr, meta->load_addr,
		    sizeof(rec.meta.load_addr));
		memcpy(rec.meta.exec_addr, meta->exec_addr,
		    sizeof(rec.meta.exec_addr));
	}
	if (meta->flags & FEMTO_META_ACCESS)
		rec.meta.access = meta->access;
	rec.meta.flags |= meta->flags;
	if (lseek(fd, off, SEEK_SET) == off) {
		ssize_t n = write(fd, &rec, sizeof(rec));
		if (n == sizeof(rec))
			ret = 0;
		else if (n >= 0)
			errno = ENOSPC;	/* short write: disc full */
	}
	close(fd);
out:
	free(mp);
	return ret;
}

void
femto_meta_del(const char *path)
{
	struct meta_rec rec;
	const char *leaf;
	char *mp;
	off_t freeoff, off;
	int fd;

	if ((mp = meta_path(path, &leaf)) == NULL)
		return;
	if (*leaf && strlen(leaf) < FEMTO_META_NAMELEN &&
	    (fd = open(mp, O_RDWR)) >= 0) {
		off = meta_find(fd, leaf, &rec, &freeoff);
		if (off >= 0 && lseek(fd, off, SEEK_SET) == off) {
			memset(&rec, 0, sizeof(rec));
			write(fd, &rec, sizeof(rec));
		}
		close(fd);
	}
	free(mp);
}

void
femto_meta_scan(const char *dir,
    void (*fn)(void *, const char *, const struct femto_meta *), void *ctx)
{
	struct meta_rec rec;
	char *mp;
	int fd;

	if ((mp = malloc(strlen(dir) + sizeof(FEMTO_META_FILE) + 1)) == NULL)
		return;
	strcpy(mp, dir);
	strcat(mp, "/" FEMTO_META_FILE);
	if ((fd = open(mp, O_RDONLY)) >= 0) {
		while (read_rec(fd, &rec))
			if (rec.name[0] != '\0')
				fn(ctx, rec.name, &rec.meta);
		close(fd);
	}
	free(mp);
}

void
femto_meta_mode(struct stat *sb, const struct femto_meta *meta)
{

	if (meta != NULL && (meta->flags & FEMTO_META_ACCESS)) {
		sb->st_mode = (sb->st_mode & S_IFMT) |
		    fs_access_to_mode(meta->access, 0);
	} else if (!S_ISDIR(sb->st_mode)) {
		/* The usual default for a new file: WR/r */
		sb->st_mode |= S_IROTH;
	}
}
