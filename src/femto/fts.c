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
 * fts.c - just enough of fts(3) for aund, on the FileStick.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fts.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "femto_compat.h"
#include "meta.h"

/* The FileStick's own directory calls (lib/readdir.c, lib/syscall.S) */
#include <sys/dirent.h>

/* Defined by aund (fs_nametrans.c) */
extern bool fs_hidden_name(char *);

/* Defined in compat.c */
extern int femto_stat_meta(const char *, struct stat *, struct femto_meta *);

#define ALIGN8(x)	(((x) + 7) & ~(size_t)7)

/*
 * Allocate an entry, with its name, its path (dir/name, or just path
 * if dir is NULL) and, if wanted, a stat buffer, all in one block.
 */
static FTSENT *
fts_alloc(const char *dir, const char *name, const char *path,
    bool want_stat)
{
	FTSENT *p;
	size_t namelen, pathlen, dirlen, pathoff, statoff;

	namelen = strlen(name);
	dirlen = dir ? strlen(dir) + 1 : 0;	/* "dir/" */
	pathlen = dir ? dirlen + namelen : strlen(path);
	pathoff = sizeof(FTSENT) + namelen;
	statoff = ALIGN8(pathoff + pathlen + 1);
	p = malloc(statoff + (want_stat ? sizeof(struct stat) : 0));
	if (p == NULL)
		return NULL;
	memset(p, 0, sizeof(*p));
	memcpy(p->fts_name, name, namelen + 1);
	p->fts_namelen = namelen;

	p->fts_accpath = p->fts_path = (char *)p + pathoff;
	if (dir) {
		memcpy(p->fts_accpath, dir, dirlen - 1);
		p->fts_accpath[dirlen - 1] = '/';
		memcpy(p->fts_accpath + dirlen, name, namelen + 1);
	} else {
		memcpy(p->fts_accpath, path, pathlen + 1);
	}
	if (want_stat) {
		p->fts_statp = (struct stat *)((char *)p + statoff);
		memset(p->fts_statp, 0, sizeof(struct stat));
	}
	return p;
}

static unsigned short
fts_type(const struct stat *sb)
{

	if (S_ISDIR(sb->st_mode)) return FTS_D;
	if (S_ISREG(sb->st_mode)) return FTS_F;
	return FTS_DEFAULT;
}

static void
fts_free_list(FTSENT *p)
{
	FTSENT *next;

	for (; p != NULL; p = next) {
		next = p->fts_link;
		free(p);
	}
}

FTS *
fts_open(char * const *argv, int options,
    int (*compar)(const FTSENT **, const FTSENT **))
{
	FTS *sp;
	FTSENT *p;
	const char *leaf;
	int i, n;

	for (n = 0; argv[n] != NULL; n++)
		continue;
	if ((sp = calloc(1, sizeof(*sp) + (n + 1) * sizeof(FTSENT *))) == NULL)
		return NULL;
	sp->fts_roots = (FTSENT **)(sp + 1);
	sp->fts_options = options;
	sp->fts_compar = compar;

	for (i = 0; i < n; i++) {
		/*
		 * The root's name is its leaf (aund builds metadata and
		 * directory names from it), its path the whole argument.
		 */
		leaf = strrchr(argv[i], '/');
		leaf = leaf ? leaf + 1 : argv[i];
		if ((p = fts_alloc(NULL, leaf, argv[i], true)) == NULL)
			goto fail;
		sp->fts_roots[i] = p;

		if (femto_stat_meta(p->fts_accpath, p->fts_statp,
		    &p->fts_meta) < 0) {
			p->fts_errno = errno;
			p->fts_info = FTS_NS;
			memset(p->fts_statp, 0, sizeof(struct stat));
		} else {
			p->fts_info = fts_type(p->fts_statp);
		}
	}
	return sp;
fail:
	fts_close(sp);
	errno = ENOMEM;
	return NULL;
}

/* Return each root in turn. aund never needs fts to descend. */
FTSENT *
fts_read(FTS *sp)
{

	if (sp == NULL || sp->fts_roots[sp->fts_next] == NULL)
		return NULL;
	return sp->fts_cur = sp->fts_roots[sp->fts_next++];
}

struct meta_match {
	FTSENT *list;
};

static void
fts_meta_match(void *ctx, const char *name, const struct femto_meta *meta)
{
	FTSENT *p;

	for (p = ((struct meta_match *)ctx)->list; p != NULL; p = p->fts_link)
		if (strcasecmp(p->fts_name, name) == 0) {
			p->fts_meta = *meta;
			break;
		}
}

/*
 * Sort a list with the user's comparison function (an insertion sort,
 * which needs no extra memory; directories are small).
 */
static FTSENT *
fts_sort(FTSENT *list, int (*compar)(const FTSENT **, const FTSENT **))
{
	FTSENT *sorted = NULL, **pp, *p, *next;

	for (p = list; p != NULL; p = next) {
		next = p->fts_link;
		for (pp = &sorted; *pp != NULL; pp = &(*pp)->fts_link) {
			const FTSENT *a = p, *b = *pp;
			if (compar(&a, &b) < 0)
				break;
		}
		p->fts_link = *pp;
		*pp = p;
	}
	return sorted;
}

/*
 * List the directory last returned by fts_read(). Names aund hides
 * from clients (fs_hidden_name()) are left out, to save memory.
 */
FTSENT *
fts_children(FTS *sp, int options)
{
	struct meta_match match;
	struct dirent *d;
	FTSENT *parent, *list = NULL, *p;
	bool nameonly = (options & FTS_NAMEONLY) != 0;
	int h;

	fts_free_list(sp->fts_child);
	sp->fts_child = NULL;
	parent = sp->fts_cur;
	if (parent == NULL || parent->fts_info != FTS_D) {
		errno = parent ? ENOTDIR : 0;
		return NULL;
	}
	if ((h = opendir(parent->fts_accpath)) < 0)
		return NULL;
	errno = 0;
	while ((d = readdir(h)) != NULL) {
		if (fs_hidden_name(d->d_name))
			continue;
		if ((p = fts_alloc(parent->fts_accpath, d->d_name, NULL,
		    !nameonly)) == NULL) {
			errno = ENOMEM;
			break;
		}
		if (nameonly) {
			p->fts_info = FTS_NSOK;
		} else {
			p->fts_statp->st_mode = (d->d_isdir ? S_IFDIR : S_IFREG)
			    | S_IRUSR | S_IWUSR;
			p->fts_statp->st_size = d->d_isdir ? 0 : d->d_size;
			p->fts_info = d->d_isdir ? FTS_D : FTS_F;
		}
		p->fts_link = list;
		list = p;
	}
	closedir(h);
	if (errno == ENOMEM) {
		fts_free_list(list);
		return NULL;
	}

	if (!nameonly) {
		match.list = list;
		femto_meta_scan(parent->fts_accpath, fts_meta_match, &match);
		for (p = list; p != NULL; p = p->fts_link)
			femto_meta_mode(p->fts_statp, &p->fts_meta);
	}
	if (sp->fts_compar)
		list = fts_sort(list, sp->fts_compar);
	return sp->fts_child = list;
}

int
fts_close(FTS *sp)
{
	int i;

	if (sp == NULL)
		return 0;
	fts_free_list(sp->fts_child);
	for (i = 0; sp->fts_roots[i] != NULL; i++)
		free(sp->fts_roots[i]);
	free(sp);
	return 0;
}
