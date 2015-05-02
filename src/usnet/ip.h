/*
 * Copyright (c) 2015 Jackie Dinh <jackiedinh8@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1 Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  2 Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution.
 *  3 Neither the name of the <organization> nor the 
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @(#)ip.h
 */

#ifndef _USNET_IP_HEADER_H_
#define _USNET_IP_HEADER_H_

#include "types.h"

#define IP_HEADER_LEN 20 

struct usn_iphdr {
#if BYTE_ORDER == LITTLE_ENDIAN 
	char	ip_hl:4,		/* header length */
		   ip_v:4;			/* version */
#endif
#if BYTE_ORDER == BIG_ENDIAN 
	char	ip_v:4,			/* version */
      	ip_hl:4;		/* header length */
#endif
	char	ip_tos;			/* type of service */
	short	ip_len;			/* total length */
	short	ip_id;			/* identification */
	short	ip_off;			/* fragment offset field */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
	char	ip_ttl;			/* time to live */
	char	ip_p;			/* protocol */
	short	ip_sum;			/* checksum */
	uint32_t    ip_src,ip_dst;	/* source and dest address */
}__attribute__((packed));

void
usnet_ipv4_init(usn_context_t *ctx);

int
usnet_ipv4_input(usn_context_t *ctx, unsigned char *buf, int len);

uint8_t*
usnet_ipv4_output_alone(usn_context_t *ctx, uint16_t ip_id, uint8_t proto, uint32_t saddr, uint32_t daddr, int len);

uint8_t*
usnet_ipv4_output(usn_context_t *ctx, usn_tcb_t *tcb, uint16_t tcplen);

#endif // !_USNET_IP_HEADER_H_

