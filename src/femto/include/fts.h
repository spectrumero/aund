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
 * A minimal fts(3) for aund on the FileStick.
 *
 * aund only uses fts to stat its root arguments (fts_read() returns
 * each root in turn, it never descends) and to list a directory
 * (fts_children()), so that is all this implements.
 *
 * Each entry also carries the Acorn metadata (load and exec addresses,
 * access) for the object, read from the directory's metadata file at
 * the same time as the entry is statted. See meta.h.
 */
#ifndef FEMTO_FTS_H
#define FEMTO_FTS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#include "meta.h"

typedef struct _ftsent {
	struct _ftsent	*fts_link;	/* next entry in a fts_children list */
	char		*fts_accpath;	/* path to access the object */
	char		*fts_path;	/* same as fts_accpath */
	struct stat	*fts_statp;	/* always valid, zeroed if FTS_NS */
	int		 fts_errno;	/* errno for FTS_NS/FTS_ERR/FTS_DNR */
	unsigned short	 fts_info;	/* FTS_D, FTS_F, ... */
	unsigned short	 fts_namelen;	/* strlen(fts_name) */
	struct femto_meta fts_meta;	/* Acorn metadata (femto only) */
	char		 fts_name[1];	/* leaf name, allocated to fit */
} FTSENT;

typedef struct {
	FTSENT		**fts_roots;	/* NULL terminated */
	int		  fts_next;	/* next root for fts_read() */
	FTSENT		 *fts_cur;	/* last root returned */
	FTSENT		 *fts_child;	/* list from fts_children() */
	int		  fts_options;
	int		(*fts_compar)(const FTSENT **, const FTSENT **);
} FTS;

/* fts_open() options */
#define FTS_COMFOLLOW	0x001
#define FTS_LOGICAL	0x002
#define FTS_NOCHDIR	0x004
#define FTS_NOSTAT	0x008
#define FTS_PHYSICAL	0x010

/* fts_children() option */
#define FTS_NAMEONLY	0x100

/* fts_info values */
#define FTS_D		 1	/* directory */
#define FTS_DC		 2	/* directory that causes cycles */
#define FTS_DEFAULT	 3	/* none of the others */
#define FTS_DNR		 4	/* unreadable directory */
#define FTS_DOT		 5	/* dot or dot-dot */
#define FTS_DP		 6	/* postorder directory */
#define FTS_ERR		 7	/* error; errno is set */
#define FTS_F		 8	/* regular file */
#define FTS_INIT	 9	/* initialized only */
#define FTS_NS		10	/* stat(2) failed */
#define FTS_NSOK	11	/* no stat(2) requested */
#define FTS_SL		12	/* symbolic link */
#define FTS_SLNONE	13	/* symbolic link without target */

FTS	*fts_open(char * const *, int,
	    int (*)(const FTSENT **, const FTSENT **));
FTSENT	*fts_read(FTS *);
FTSENT	*fts_children(FTS *, int);
int	 fts_close(FTS *);

#endif
