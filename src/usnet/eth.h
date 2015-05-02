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
 * @(#)eth.h
 */

#ifndef _USNET_ETH_H_
#define _USNET_ETH_H_

#include "types.h"
#include "buf.h"

/* supported ether type */
#define USN_ETHERTYPE_IP      0x0800   /* IP protocol */
#define USN_ETHERTYPE_ARP     0x0806   /* Addr. resolution protocol */


#define USN_ETH_ALEN 6
typedef struct usn_ethhdr usn_ethhdr_t;
struct   usn_ethhdr {
   unsigned char   dest[USN_ETH_ALEN];
   unsigned char   source[USN_ETH_ALEN];
   unsigned short  type;
};


char *
usnet_ether_sprintf(usn_context_t *ctx, char *ap);

void
usnet_eth_input(usn_context_t *ctx, unsigned char *buf, int len);

int32_t
usnet_eth_output();

char*
usnet_get_nmbuf(usn_context_t *ctx, usn_nmring_t *trigger, int len);

void
usnet_send_data(usn_context_t *ctx);

char*
usnet_eth_output_nm(usn_context_t *ctx, int type, uint8_t *haddr, usn_nmring_t *trigger, int len);


#endif // !_USNET_ETH_H_

