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
 * @(#)udp.c
 */

#include <netinet/in.h>

#include "udp.h"
#include "eth.h"
#include "utils.h"
#include "ip.h"
#include "core.h"
#include "log.h"

void
usnet_udp_init(usn_context_t *ctx)
{
   return;
}

int
usnet_udp_output(usn_context_t *ctx, unsigned char *buf, int len)
{
   return 0;
}

int
usnet_udp_input(usn_context_t *ctx, usn_iphdr_t *iph, int iplen)
{
   usn_udphdr_t *udph;
   uint16_t port;
   unsigned char*    payload;
   int      udplen;

	DEBUG(ctx->log, "udp packet");

   udph = (usn_udphdr_t *)((char*)iph + (iph->ip_hl << 2));
   udplen = iplen - (iph->ip_hl << 2);

   payload = (unsigned char*)usnet_ipv4_output_alone(ctx, iph->ip_id, 
                        IPPROTO_UDP, iph->ip_dst, iph->ip_src, udplen);
   if ( payload == 0 ) {
      ERROR(ctx->log,"could not get buffer");
      return -1;
   }

   //swap ports
   port = udph->uh_dport;
   udph->uh_dport = udph->uh_sport;
   udph->uh_sport = port;
 
   // check sum
   udph->uh_sum = 0;
   udph->uh_sum = udp_cksum((uint16_t*)udph, udplen, iph->ip_dst, iph->ip_src);

   memcpy(payload, udph, udplen);

   usnet_send_data(ctx);

   return 0;
}





