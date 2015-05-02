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
 * @(#)utils.h
 */

#ifndef _USNET_UTILS_H_
#define _USNET_UTILS_H_

#include "types.h"

struct tcp_timestamp
{        
   uint32_t ts_val;
   uint32_t ts_ref;
};


inline char*
usnet_tcp_statestr(usn_tcb_t *tcb);

inline char*
usnet_tcp_statestr_new(int state);

void
dump_buffer(usn_context_t *ctx, char *p, int len, const char *prefix);

int
convert_haddr_to_array(char* str, uint8_t *haddr);

static inline uint16_t ip_fast_csum(const void *iph, unsigned int ihl)
{
   unsigned int sum;

   asm("  movl (%1), %0\n"
       "  subl $4, %2\n"
       "  jbe 2f\n"
       "  addl 4(%1), %0\n"
       "  adcl 8(%1), %0\n"
       "  adcl 12(%1), %0\n"
       "1: adcl 16(%1), %0\n"
       "  lea 4(%1), %1\n"
       "  decl %2\n"
       "  jne      1b\n"
       "  adcl $0, %0\n"
       "  movl %0, %2\n"
       "  shrl $16, %0\n"
       "  addw %w2, %w0\n"
       "  adcl $0, %0\n"
       "  notl %0\n"
       "2:"
       /* Since the input registers which are loaded with iph and ih
          are modified, we must also specify them as outputs, or gcc
          will assume they contain their original values. */
       : "=r" (sum), "=r" (iph), "=r" (ihl)
       : "1" (iph), "2" (ihl)
          : "memory");
   return (uint16_t)sum;
}

unsigned short
cksum(usn_context_t *ctx, char* buf, int len) ;

unsigned short 
in_cksum(usn_context_t *ctx, char* buf, int len);

uint16_t
udp_cksum(uint16_t *buf, uint16_t len, uint32_t saddr, uint32_t daddr);

uint16_t
tcp_cksum(uint16_t *buf, uint16_t len, uint32_t saddr, uint32_t daddr);

void
usnet_get_network_info(usn_context_t *ctx, const char *ifname, int len);


#endif //_USNET_UTILS_H_
