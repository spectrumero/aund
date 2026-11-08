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
 * femto_config.c - the configuration of aund on the FileStick.
 *
 * There's no configuration file in a femto build: this sets the
 * same things conf_lex.l would from one, equivalent to
 *
 *	root /aund
 *	urd .
 *	lib .
 *	opt4 0
 *	infoformat riscos
 *	safehandles on
 *	typemap default fff
 *
 * The station number isn't set here: the FileStick sets it at boot
 * (configure netstation in boot.rc), and aund uses whatever it is.
 */

#include <stdbool.h>
#include <string.h>

#include "extern.h"
#include "fileserver.h"

/* The directory on the FileStick whose contents are served */
#define FEMTO_ROOT	"/aund"

/* The disc name clients see (at most 16 characters) */
#define FEMTO_DISCNAME	"FileStick"

void
conf_init(const char *conffile)
{

	(void)conffile;
	root = FEMTO_ROOT;
	fixedurd = ".";
	lib = ".";
	default_opt4 = 0;		/* no !BOOT: the root starts out empty */
	default_infoformat = FS_INFO_RISCOS;
	default_safehandles = true;	/* for NFS 3.60 and earlier */
	strcpy(discname, FEMTO_DISCNAME);
	debug = 0;
	using_syslog = 0;
	fs_add_typemap_default(0xfff);	/* text */
}
