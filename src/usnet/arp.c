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
 * @(#)arp.c
 */

#include <arpa/inet.h>

#include "arp.h"
#include "core.h"
#include "eth.h"
#include "utils.h"
#include "log.h"

usn_arp_entry_t*
usnet_get_arp(usn_context_t *ctx, uint32_t ip)
{
   int i;

   for ( i=0; i < ctx->arp.cnt; i++ ) {
      if ( ctx->arp.entries[i].ip == ip ) 
         return &ctx->arp.entries[i]; 
   }

   return 0;
}

unsigned char*
usnet_get_hwaddr(usn_context_t *ctx, uint32_t ip)
{
   int i;

   for ( i=0; i < ctx->arp.cnt; i++ ) {
      if ( ctx->arp.entries[i].ip == ip ) 
         return ctx->arp.entries[i].haddr; 
   }

   return 0;
}

void
usnet_register_arp(usn_context_t *ctx, uint32_t ip, const uint8_t *haddr)
{
   int idx = ctx->arp.cnt; 

   ctx->arp.entries[idx].ip =  ip;
   memcpy(ctx->arp.entries[idx].haddr, haddr, ETH_LEN);

   ctx->arp.cnt++;

   return;
}

int
usnet_arp_output(usn_context_t *ctx, int op, uint32_t dst_ip, uint8_t *dst_haddr)
{
   usn_nmring_t  ring;
   usn_arphdr_t *arph;

   DEBUG(ctx->log, "arp output");

   arph = (usn_arphdr_t*)usnet_eth_output_nm(ctx, htons(USN_ETHERTYPE_ARP), dst_haddr, 
                                                 &ring, sizeof(usn_arphdr_t));
   
   arph->ar_hrd = htons(ARPHRD_ETHER);
   arph->ar_pro = htons(USN_ETHERTYPE_IP);
   arph->ar_hln = ETH_LEN;
   arph->ar_pln = 4;     
   arph->ar_op  = ntohs(op);
   arph->ar_spa = ctx->ifnet->addr;
   arph->ar_tpa = dst_ip;
   memcpy(arph->ar_sha, ctx->ifnet->hwa, ETH_LEN);
   memcpy(arph->ar_tha, dst_haddr, ETH_LEN);

   usnet_send_data(ctx);

   return 0;
}

int
usnet_arp_request(usn_context_t *ctx, uint32_t ip)
{
   unsigned char haddr[ETH_LEN];

   DEBUG(ctx->log, "arp request");
   // TODO: save pending packet to send later.

   memset(haddr, 0xFF, ETH_LEN);
   usnet_arp_output(ctx, USN_ARPOP_REQUEST,ip, haddr);

   return 0;
}

int
usnet_arp_req_handler(usn_context_t *ctx, usn_arphdr_t *arph)
{
   usn_arp_entry_t *entry;

   entry = usnet_get_arp(ctx, arph->ar_spa);
   if ( entry == 0 )
      usnet_register_arp(ctx, arph->ar_spa, arph->ar_sha);

   usnet_arp_output(ctx, USN_ARPOP_REPLY, arph->ar_spa, arph->ar_sha);  

   return 0;
}

int
usnet_arp_reply_handler(usn_context_t *ctx, usn_arphdr_t *arph)
{
   usn_arp_entry_t *entry;

   entry = usnet_get_arp(ctx, arph->ar_spa);
   if ( entry == 0 )
      usnet_register_arp(ctx, arph->ar_spa, arph->ar_sha);

   DEBUG(ctx->log, "remove arp request from queue and send pending buffer");


   return 0;
}

int
usnet_arp_input(usn_context_t *ctx, unsigned char *buf, int len)
{
   usn_arphdr_t  *arph = (usn_arphdr_t*)(buf +sizeof(usn_ethhdr_t));

   DEBUG(ctx->log, "dst_ip=%x, local_ip=%x", 
         arph->ar_tpa, ctx->ifnet->addr);
   if ( arph->ar_tpa != ctx->ifnet->addr ) {
      // not for us, drop packet.
      return -1;
   }

   dump_buffer(ctx, (char*)buf, len, "arp");

   switch (ntohs(arph->ar_op)) {
      case USN_ARPOP_REQUEST:
         usnet_arp_req_handler(ctx, arph);
         break;
      case USN_ARPOP_REPLY:
         usnet_arp_reply_handler(ctx, arph);
         break;
      default:
         WARN(ctx->log, "non supported arp op: %d", ntohs(arph->ar_op));
         break;
   }

   return 0;
}


void 
usnet_arp_init(usn_context_t *ctx)
{
   ctx->arp.cnt = 0;
   ctx->arp.entries = (usn_arp_entry_t*)malloc(sizeof(usn_arp_entry_t) * MAX_ARP_ENTRIES);
   memset(ctx->arp.entries, 0, sizeof(usn_arp_entry_t) * MAX_ARP_ENTRIES);

   memcpy(ctx->arp.entries[0].haddr, ctx->ifnet->hwa, ETH_LEN);
   ctx->arp.cnt++;
/*
   { 
      char haddr[ETH_LEN];
      ctx->arp.entries[0].ip=0x010a0a0a;
      //3c:07:54:6d:f3:39
      haddr[0] = 0x3c;
      haddr[1] = 0x07;
      haddr[2] = 0x54;
      haddr[3] = 0x6d;
      haddr[4] = 0xf3;
      haddr[5] = 0x39;
      memcpy(ctx->arp.entries[0].haddr, haddr, ETH_LEN);
      ctx->arp.cnt++;

      ctx->arp.entries[1].ip=0x080a0a0a;
      //00:21:cc:c0:04:45
      haddr[0] = 0x00;
      haddr[1] = 0x21;
      haddr[2] = 0xcc;
      haddr[3] = 0xc0;
      haddr[4] = 0x04;
      haddr[5] = 0x45;
      memcpy(ctx->arp.entries[1].haddr, haddr, ETH_LEN);
      ctx->arp.cnt++;
   }
*/
   return;
}













