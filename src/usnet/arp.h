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
 * @(#)arp.h
 */

#ifndef _USNET_ARP_H_
#define _USNET_ARP_H_

#include <stdint.h>

#include "types.h"

#define ARPHRD_ETHER  1  /* ethernet hardware format */
#define MAX_ARP_ENTRIES 1024

enum usn_arp_op_enum {
   USN_ARPOP_REQUEST    = 1,  /* request to resolve address */
   USN_ARPOP_REPLY      = 2,  /* response to previous request */
   USN_ARPOP_REVREQUEST = 3,  /* request protocol address given hardware */
   USN_ARPOP_REVREPLY   = 4,  /* response giving protocol address */
   USN_ARPOP_INVREQUEST = 8,  /* request to identify peer */
   USN_ARPOP_INVREPLY   = 9   /* response identifying peer */
};

typedef struct usn_arphdr usn_arphdr_t;
struct usn_arphdr {
   uint16_t  ar_hrd;     /* format of hardware address */
   uint16_t  ar_pro;     /* format of protocol address */
   uint8_t   ar_hln;     /* length of hardware address */
   uint8_t   ar_pln;     /* length of protocol address */
   uint16_t  ar_op;      /* arp operation */
   uint8_t   ar_sha[ETH_LEN];   /* sender hardware address */
   uint32_t  ar_spa;   /* sender protocol address */
   uint8_t   ar_tha[ETH_LEN];   /* target hardware address */
   uint32_t  ar_tpa;   /* target protocol address */
}__attribute__ ((packed));


/* ARP Caching Mechanism */
void 
usnet_arp_init(usn_context_t *ctx);

int
usnet_arp_input(usn_context_t *ctx, unsigned char *buf, int len);

unsigned char*
usnet_get_hwaddr(usn_context_t *ctx, uint32_t ip);

int
usnet_arp_request(usn_context_t *ctx, uint32_t ip);

#endif // USNET_ARP_H_



