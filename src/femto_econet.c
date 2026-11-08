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
 * Native econet implementation for Femto board (the FileStick).
 *
 * The FileStick system does the four way handshake and immediate
 * Machine Type Peeks itself. Each open of /dev/econet can listen on one
 * port; read() gives the source station and network followed by the
 * payload of a frame, and write() sends a frame to the address set with
 * ECONET_SET_SEND_ADDR. The station number is set when the FileStick
 * boots (configure netstation), so it isn't set here.
 *
 * aund listens on two ports: the file server port, and the port clients
 * send SAVE and PUTBYTES data to. The system has one receive buffer for
 * all ports, so frames are read as soon as they arrive, whichever port
 * aund is currently waiting on: a request that arrives while aund is
 * receiving data from another station is kept until aund is ready for
 * it, and stray data frames are thrown away.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/econet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <filestick.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aun.h"
#include "extern.h"

/* Largest data block, in each direction */
#define FEMTO_MAX_BLOCK		1024

/* Largest request kept while waiting for data (a *command is < 256) */
#define FEMTO_MAX_HELD		300

/* Waiting for a particular station: 50 polls of 100ms, as beebem.c */
#define RECV_POLLS		50
#define RECV_POLL_MS		100

/* Transmission retries when the client isn't (yet) listening */
#define XMIT_TRIES		10
#define XMIT_RETRY_MS		20

/* The driver's read() gives station, network, payload */
#define SRCLEN			2
/*
 * Read frames this far into the buffer, so the payload lands at
 * pkt->data. The source address overlaps the end of the sequence
 * number, which is filled in afterwards.
 */
#define PKTOFF			(offsetof(struct aun_packet, data) - SRCLEN)

struct femto_addr {			/* overlays struct aun_srcaddr */
	uint8_t	station;
	uint8_t	network;
};

static int fs_fd = -1;			/* listening on EC_PORT_FS */
static int data_fd = -1;		/* listening on EC_PORT_FS_DATA */
static bool have_rxflag;		/* system reports control bytes */

/* Received frame; +1 because file_server() NUL terminates requests */
static union {
	struct aun_packet pkt;
	uint8_t b[offsetof(struct aun_packet, data) + FEMTO_MAX_BLOCK + 1];
} rbuf;

/* A file server request that arrived while waiting for data */
static struct {
	ssize_t	len;			/* 0 if none */
	uint8_t	flag;
	uint8_t	frame[SRCLEN + FEMTO_MAX_HELD];
} held;

static int
f_econet_listen(uint8_t port)
{
	int fd;

	if ((fd = open("/dev/econet", O_RDWR)) < 0)
		err(1, "/dev/econet");
	if (ioctl(fd, ECONET_SET_RECV_PORT | port) < 0)
		errx(1, "can't listen on Econet port &%02X", port);
	return fd;
}

static void
f_econet_setup(void)
{
	uint8_t station = 0, flag;

	fs_fd = f_econet_listen(EC_PORT_FS);
	data_fd = f_econet_listen(EC_PORT_FS_DATA);

	if (ioctl(fs_fd, ECONET_GET_ADDR, &station) < 0 || station == 0)
		errx(1, "no Econet station number: use configure netstation");

	/*
	 * BGET and BPUT tell a repeated request from a new one by the
	 * sequence bit in the control byte. Older systems don't report
	 * it, and then every BGET after the first looks like a repeat.
	 */
	have_rxflag = ioctl(fs_fd, ECONET_GET_RXFLAG, &flag) == 0;
	if (!have_rxflag)
		warnx("system doesn't report Econet control bytes: "
		    "BGET and BPUT will not work");

	printf("aund: file server on station %d\n", station);
}

/*
 * Read the frame waiting on fd into buf. Returns its length (source
 * address and payload), or -1 if it's no use.
 */
static ssize_t
f_econet_read(int fd, uint8_t *buf, size_t size, uint8_t *flag)
{
	uint8_t junk[32];
	ssize_t len;

	len = read(fd, buf, size);
	if (len == (ssize_t)size && fd_peek(fd) > 0) {
		/* Too big for us: throw the rest away */
		while (fd_peek(fd) > 0 && read(fd, junk, sizeof(junk)) > 0)
			continue;
		return -1;
	}
	if (len < SRCLEN)
		return -1;
	*flag = 0x80;
	if (have_rxflag)
		ioctl(fd, ECONET_GET_RXFLAG, flag);
	return len;
}

static struct aun_packet *
f_econet_packet(ssize_t len, uint8_t flag, int port, ssize_t *outsize,
    struct aun_srcaddr *from)
{
	struct femto_addr *afrom = (struct femto_addr *)from;

	memset(from, 0, sizeof(*from));
	afrom->station = rbuf.b[PKTOFF];
	afrom->network = rbuf.b[PKTOFF + 1];
	rbuf.pkt.type = AUN_TYPE_UNICAST;
	rbuf.pkt.dest_port = port;
	rbuf.pkt.flag = flag;
	rbuf.pkt.retrans = 0;
	memset(rbuf.pkt.seq, 0, sizeof(rbuf.pkt.seq));
	*outsize = len - SRCLEN + offsetof(struct aun_packet, data);
	return &rbuf.pkt;
}

/*
 * Receive a frame for want_port. If from is a station, only accept a
 * frame from there, and give up after a few seconds.
 */
static struct aun_packet *
f_econet_recv(ssize_t *outsize, struct aun_srcaddr *from, int want_port)
{
	struct femto_addr *afrom = (struct femto_addr *)from;
	struct pollfd pfd[2];
	bool any = !(afrom->station || afrom->network);
	int polls = RECV_POLLS, i, port;
	uint8_t flag;
	ssize_t len;

	if (want_port != EC_PORT_FS && want_port != EC_PORT_FS_DATA) {
		errno = EINVAL;
		return NULL;
	}

	if (want_port == EC_PORT_FS && held.len) {
		memcpy(rbuf.b + PKTOFF, held.frame, held.len);
		len = held.len;
		held.len = 0;
		return f_econet_packet(len, held.flag, EC_PORT_FS, outsize,
		    from);
	}

	for (;;) {
		pfd[0].fd = fs_fd;
		pfd[1].fd = data_fd;
		pfd[0].events = pfd[1].events = POLLIN;
		/* A FileStick poll() timeout of 0 waits forever */
		if (poll(pfd, 2, any ? 0 : RECV_POLL_MS) == 0) {
			if (--polls == 0) {
				errno = ETIMEDOUT;
				return NULL;
			}
			continue;
		}
		for (i = 0; i < 2; i++) {
			if (!(pfd[i].revents & POLLIN))
				continue;
			len = f_econet_read(pfd[i].fd, rbuf.b + PKTOFF,
			    sizeof(rbuf.b) - PKTOFF - 1, &flag);
			if (len < 0)
				continue;
			port = pfd[i].fd == fs_fd ? EC_PORT_FS :
			    EC_PORT_FS_DATA;
			if (port != want_port) {
				/*
				 * A request while we wait for data: keep
				 * it (if there's room) for later. Data
				 * we aren't waiting for is a stray.
				 */
				if (port == EC_PORT_FS && !held.len &&
				    len <= (ssize_t)sizeof(held.frame)) {
					memcpy(held.frame, rbuf.b + PKTOFF, len);
					held.len = len;
					held.flag = flag;
				}
				continue;
			}
			if (!any && (rbuf.b[PKTOFF] != afrom->station ||
			    rbuf.b[PKTOFF + 1] != afrom->network))
				continue;	/* someone else's data */
			return f_econet_packet(len, flag, port, outsize, from);
		}
	}
}

static ssize_t
f_econet_xmit(struct aun_packet *pkt, size_t len, struct aun_srcaddr *to)
{
	struct femto_addr *ato = (struct femto_addr *)to;
	struct econet_addr dest;
	size_t payload = len - offsetof(struct aun_packet, data);
	int tries;

	dest.port = pkt->dest_port;
	dest.net = ato->network;
	dest.station = ato->station;
	if (ioctl(fs_fd, ECONET_SET_SEND_ADDR, &dest) < 0) {
		errno = EINVAL;
		return -1;
	}
	/*
	 * The client may not be listening yet (it has to set up a
	 * receive for the reply), so try a few times, as a real file
	 * server would.
	 */
	for (tries = 0; tries < XMIT_TRIES; tries++) {
		if (write(fs_fd, pkt->data, payload) >= 0)
			return len;
		if (errno != EHOSTUNREACH && errno != ETIMEDOUT)
			break;
		poll(NULL, 0, XMIT_RETRY_MS);	/* just a delay */
	}
	return -1;
}

static char *
f_econet_ntoa(struct aun_srcaddr *addr)
{
	struct femto_addr *a = (struct femto_addr *)addr;
	static char buf[sizeof("station 255.255")];

	snprintf(buf, sizeof(buf), "station %d.%d", a->network, a->station);
	return buf;
}

static void
f_econet_get_stn(struct aun_srcaddr *addr, uint8_t *out)
{
	struct femto_addr *a = (struct femto_addr *)addr;

	out[0] = a->station;
	out[1] = a->network;
}

const struct aun_funcs femto_econet = {
	.max_block	= FEMTO_MAX_BLOCK,
	.setup		= f_econet_setup,
	.recv		= f_econet_recv,
	.xmit		= f_econet_xmit,
	.ntoa		= f_econet_ntoa,
	.get_stn	= f_econet_get_stn
};
