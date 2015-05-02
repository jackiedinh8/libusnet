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
 * @(#)ip_icmp.c
 */

#include <netinet/in.h>

#include "ip_icmp.h"
#include "utils.h"
#include "eth.h"
#include "log.h"
#include "core.h"

int
usnet_icmp_reflect(usn_context_t *ctx, usn_iphdr_t* iph, int icmplen)
{
   unsigned char *payload;
   usn_icmphdr_t *icmph;

   DEBUG(ctx->log,"reflect, len=%d",icmplen);
	icmph = (usn_icmphdr_t *)((char*)iph + (iph->ip_hl << 2));
	icmph->icmp_type = ICMP_ECHOREPLY;
   icmph->icmp_cksum = 0;
   icmph->icmp_cksum = in_cksum(ctx, (char*)icmph, icmplen);

   payload = (unsigned char*)usnet_ipv4_output_alone(ctx, iph->ip_id, 
         IPPROTO_ICMP, iph->ip_dst, iph->ip_src, icmplen);

   if ( payload == 0 ) {
      DEBUG(ctx->log, "buffer is null");
      return 0;
   }
   memcpy(payload, icmph, icmplen);

   usnet_send_data(ctx);

   return 0;
}

int
usnet_icmp_input(usn_context_t *ctx, usn_iphdr_t *iph, int iplen)
{
	int icmplen;
   usn_icmphdr_t *icmph;

   DEBUG(ctx->log, "icmp packet, len=%d", iplen);

   icmplen = iplen - (iph->ip_hl << 2);
	if (icmplen < ICMP_MINLEN) {
      ERROR(ctx->log, "len is too small, len=%d", icmplen);
		return -1;
	}
	icmph = (usn_icmphdr_t *)((char*)iph + (iph->ip_hl << 2));

	//in_cksum(ctx, (char*)icmph, icmplen);
	if (cksum(ctx, (char*)icmph, icmplen)) {
      ERROR(ctx->log, "check sum failed");
		return -2;
	}

   if ( icmph->icmp_type != ICMP_ECHO ) {
      ERROR(ctx->log, "not supported icmp type, type=%d", icmph->icmp_type);
      return -3;
   }

   usnet_icmp_reflect(ctx, iph, icmplen);

   return 0;   
}

int
usnet_icmp_output(usn_context_t *ctx, unsigned char *buf, int len)
{
   return 0;
}


