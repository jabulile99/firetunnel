/*
 * Copyright (C) 2018 Firetunnel Authors
 *
 * This file is part of firetunnel project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "firetunnel.h"

//**********************************************************************************
// Skytale scrambler
//**********************************************************************************
// Skytale was a tool used  to perform a transposition cipher, consisting of a cylinder with a
// strip of parchment wound around it on which is written a message. The ancient Greeks,
// and the Spartans in particular, are said to have used this cipher to communicate during
// military campaigns.
//
// More:  https://en.wikipedia.org/wiki/Scytale
//
// Please don't confuse this for serious encryption. Network traffic is highly recognizable.
// Somebody who knows what he's doing will figure this out in exactly 10 minutes just by
// looking at traffic traces.
//**********************************************************************************

#define BLOCKLEN 8

// transposition routine; same function is used for encoding and decoding
static void skytale(uint8_t *in) {
	uint8_t out[BLOCKLEN] = {0};
	uint8_t *ptr = in;

	int j;
	uint8_t mask_out = 1;
	for (j = 0; j < BLOCKLEN; j++, ptr++, mask_out <<= 1) {
		int i;
		uint8_t mask_in = 1;
		for (i = 0; i < BLOCKLEN; i++, mask_in <<= 1)
			out[i] |= (*ptr & mask_in) ? mask_out : 0;
	}

	memcpy(in, out, BLOCKLEN);
}

#ifdef TESTING
int arg_noscrambling = 0;
#endif

// scrambling function
__attribute__((weak)) void scramble(uint8_t *ptr, int len, PacketHeader *hdr) {
	assert(ptr);
	(void) hdr;

	// no scrambling if the program was started with --noscrambling command line option
	if (arg_noscrambling || len < BLOCKLEN)
		return;

	// padding: multiple of BLOCKLEN
	int i;
	for ( i = 0; i < (len / BLOCKLEN); i++)
		skytale(ptr + i * BLOCKLEN);

	if (len % BLOCKLEN)
		skytale(ptr + len - BLOCKLEN);

}

// descrambling function
__attribute__((weak)) void descramble(uint8_t *ptr, int len, PacketHeader *hdr) {
	assert(ptr);
	(void) hdr;

	// no scrambling if the program was started with --noscrambling command line option
	if (arg_noscrambling || len < BLOCKLEN)
		return;

	if (len % BLOCKLEN)
		skytale(ptr + len - BLOCKLEN);

	int i;
	for ( i = 0; i < (len / BLOCKLEN); i++)
		skytale(ptr + i * BLOCKLEN);
}

#ifdef TESTING
#include <time.h>

// rtdsc timestamp on x86-64/amd64  processors
static inline unsigned long long getticks(void) {
#if defined(__x86_64__)
	unsigned a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((unsigned long long)a) | (((unsigned long long)d) << 32);
#elif defined(__i386__)
	unsigned long long ret;
	__asm__ __volatile__("rdtsc" : "=A" (ret));
	return ret;
#else
	return 0; // not implemented
#endif
}


int main(int argc, char **argv) {
	PacketHeader h;
	memset(&h, 0, sizeof(h));

	if (argc != 2) {
		printf("usage: ./a.out bufsize\n");
		return 1;
	}
	int buflen = atoi(argv[1]);

	uint8_t *buf = malloc(buflen);
	uint8_t *buf_in = malloc(buflen);
	uint8_t *buf_out = malloc(buflen);
//	srand(time(NULL));

	int i;
	for (i = 0; i < buflen; i++) {
		buf_in[i] = (uint8_t) ( rand() % 256);
		printf("%02x ", buf_in[i]);
	}
	printf("\n");

	memcpy(buf, buf_in, buflen);
	scramble(buf, buflen, &h);
	for (i = 0; i < buflen; i++) {
		printf("%02x ", buf[i]);
	}
	printf("\n");

	memcpy(buf_out, buf, buflen);
	descramble(buf_out, buflen, &h);

	for (i = 0; i < buflen; i++) {
		printf("%02x ", buf_out[i]);
	}
	printf("\n");
	for (i = 0; i < buflen; i++) {
		if (buf_out[i] != buf_in[i])
			printf("error position %d\n", i);
	}

	// evaluate time
	for (i = 0; i < buflen; i++) {
		buf_in[i] = (uint8_t) ( rand() % 256);
	}

	unsigned cnt = 10000;
	unsigned long long tstart = getticks();
	for (i = 0; i < cnt; i++)
		scramble(buf, buflen, &h);
	unsigned long long tend = getticks();
	usleep(1000);
	unsigned long long t1ms = getticks();

	double delta = (double) (tend - tstart) / (double) (t1ms - tend);
	double delta_packet = delta / cnt;
	double delta_byte = (delta_packet * 1000) / buflen;
	double rate = 8 / delta_byte;
	printf("Skytale %d bytes: %f ms / packet, rate %f Mbps\n", buflen, delta_packet, rate);

	free(buf);
	free(buf_in);
	free(buf_out);

	return 0;
}
#endif
