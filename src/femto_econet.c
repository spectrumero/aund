/*-
 * Copyright (c) 2025 Dylan Smith
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
 * Native econet implementation for Femto board.
 */ 

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include "aun.h"
#include "extern.h"

static void
f_econet_setup(void)
{
}

static struct aun_packet *
f_econet_recv(ssize_t *outsize, struct aun_srcaddr *from, int want_port)
{
   return NULL;
}

static ssize_t
f_econet_xmit(struct aun_packet *pkt, size_t len, struct aun_srcaddr *to)
{
   return -1;
}

static char *
f_econet_ntoa(struct aun_srcaddr *addr)
{
}

static void
f_econet_get_stn(struct aun_srcaddr *addr, uint8_t *out)
{
}

const struct aun_funcs femto_econet = {
   .max_block  = 1024,
   .setup      = f_econet_setup,
   .recv       = f_econet_recv,
   .xmit       = f_econet_xmit,
   .ntoa       = f_econet_ntoa,
   .get_stn    = f_econet_get_stn
};

