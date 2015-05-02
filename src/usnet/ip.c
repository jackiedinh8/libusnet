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
 * @(#)ip.c
 */

#include <netinet/in.h>

#include "ip.h"
#include "core.h"
#include "eth.h"
#include "utils.h"
#include "tcp.h"
#include "udp.h"
#include "ip_icmp.h"
#include "arp.h"
#include "log.h"

void 
usnet_ipv4_init(usn_context_t *ctx)
{
   return;
}

int
usnet_ipv4_input(usn_context_t *ctx, unsigned char *buf, int len)
{
   usn_iphdr_t *iph = (usn_iphdr_t*)(buf + sizeof(usn_ethhdr_t));
   int iplen = ntohs(iph->ip_len);

   if ( iplen < sizeof(usn_iphdr_t) )
      return -1;

   if ( ip_fast_csum(iph, iph->ip_hl) )
      return -2;

   if ( iph->ip_v != 0x04 )
      return -3;

   DEBUG(ctx->log, "ip packet, ver=%x, hl=%x, proto=%d, iplen=%d", 
         iph->ip_v, iph->ip_hl, iph->ip_p, iplen);

   switch(iph->ip_p) {
      case IPPROTO_TCP:
         return usnet_tcp_input(ctx,iph,iplen);
      case IPPROTO_UDP:
         return usnet_udp_input(ctx,iph,iplen);
      case IPPROTO_ICMP:
         return usnet_icmp_input(ctx,iph,iplen);
      default:
         WARN(ctx->log, "unsupported ip protocol: %d", iph->ip_p);
   }

   return 0;
}


/* @param ip_id is in network byte order */
uint8_t*
usnet_ipv4_output_alone(usn_context_t *ctx, uint16_t ip_id, 
        uint8_t proto, uint32_t saddr, uint32_t daddr, int len)
{
   usn_iphdr_t *iph;
   unsigned char *haddr = NULL;

   DEBUG(ctx->log,"ipv4 output");

   haddr = usnet_get_hwaddr(ctx,daddr);
   
   if ( haddr == 0 ) {
      usnet_arp_request(ctx, daddr);
      return 0;
   }

   DEBUG(ctx->log, "found hardware address");

   iph = (usn_iphdr_t*)usnet_eth_output_nm(ctx, 
         htons(USN_ETHERTYPE_IP), haddr, &ctx->trigger_ring, len + IP_HEADER_LEN);

   if ( iph == NULL ) {
      ERROR(ctx->log,"failed to get netmap buffer");
      return 0;
   }

   iph->ip_hl = IP_HEADER_LEN >> 2;
   iph->ip_v = 4;
   iph->ip_tos = 0;
   iph->ip_len = htons(len + IP_HEADER_LEN);
   iph->ip_id = ip_id;
   iph->ip_off = 0;//htons(IP_DF);
   iph->ip_ttl = 255;
   iph->ip_p = proto;//IPPROTO_TCP;
   iph->ip_src = saddr;
   iph->ip_dst = daddr;
   iph->ip_sum = 0;
   iph->ip_sum = ip_fast_csum(iph, iph->ip_hl);

   return (uint8_t*)(iph + 1);
}

uint8_t*
usnet_ipv4_output(usn_context_t *ctx, usn_tcb_t *tcb, uint16_t iplen)
{
   usn_iphdr_t *iph;
   unsigned char *haddr;

   haddr = usnet_get_hwaddr(ctx,tcb->daddr);
   if (!haddr) {
      usnet_arp_request(ctx, tcb->daddr);
      return 0;
   }
   
   iph = (usn_iphdr_t*)usnet_eth_output_nm(ctx, 
         htons(USN_ETHERTYPE_IP), haddr, &ctx->trigger_ring, iplen + IP_HEADER_LEN);

   if (!iph) {
      return 0;
   }
   iph->ip_hl = IP_HEADER_LEN >> 2;
   iph->ip_v = 4;
   iph->ip_tos = 0;
   iph->ip_len = htons(IP_HEADER_LEN + iplen);
   iph->ip_id = htons(tcb->sndvar->ip_id++);
   iph->ip_off = htons(0x4000);   // no fragmentation
   iph->ip_ttl = 64;
   iph->ip_p = IPPROTO_TCP;
   iph->ip_src = tcb->saddr;
   iph->ip_dst = tcb->daddr;
   iph->ip_sum = 0;
   iph->ip_sum = ip_fast_csum(iph, iph->ip_hl);

   return (uint8_t *)(iph + 1);
}





