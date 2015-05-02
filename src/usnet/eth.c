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
 * @(#)eth.c
 */

#include <arpa/inet.h>
#include <poll.h>

#include "core.h"
#include "eth.h"
#include "arp.h"
#include "ip.h"
#include "utils.h"
#include "log.h"

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
char*
usnet_ether_sprintf(usn_context_t *ctx, char *ap)
{
	static char etherbuf[18];
	char        *cp = etherbuf;
	int         i;

	for (i = 0; i < 6; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (etherbuf);
}

void
usnet_eth_input(usn_context_t *ctx, unsigned char *buf, int len)
{
	usn_ethhdr_t      *ethh = (usn_ethhdr_t*)buf;
   unsigned short     type = ntohs(ethh->type);

	switch(type){
      case USN_ETHERTYPE_IP: // IP protocol
         usnet_ipv4_input(ctx, buf, len);
			break;
      case USN_ETHERTYPE_ARP:// Addr. resolution protocol
			usnet_arp_input(ctx, buf, len);
			break;
		default:
			WARN(ctx->log,"protocol is not supported, ether_type=%d", type);
			break;
	}

   return;
}

int32_t
usnet_eth_output()
{
   return 0;
}


char*
usnet_get_nmbuf(usn_context_t *ctx, usn_nmring_t *trigger, int len)
{
   int    i,j;  
   int    attemps = 0;
   struct pollfd     fds;
   struct netmap_if   *nifp;
   struct nm_desc   *g_nmd = ctx->nmd;
   int    ret = 0;

   fds.fd = g_nmd->fd;
   nifp = g_nmd->nifp;

resend:
   if ( attemps == 3 ) { 
      return 0;
   }   
   if(ctx->ifnet->npkts >= ctx->ifnet->burst ){
      fds.events = POLLOUT;
      fds.revents = 0; 
      ret = poll(&fds, 1, 2000);
      if (ret <= 0 ) {
         // TODO: save pending packets? 
         // as it is easy to reach line rate.
         ERROR(ctx->log,"poll error");
         goto failure;
      }  
      if (fds.revents & POLLERR) {
         struct netmap_ring *tx = NETMAP_RXRING(nifp, g_nmd->cur_tx_ring);
         ERROR(ctx->log, "error on em1, rx [%d,%d,%d]",
         tx->head, tx->cur, tx->tail);
         goto failure;
      }
      if (fds.revents & POLLOUT) {
         goto send;
      }
      goto flush;
   }
send:

   for (j = g_nmd->first_tx_ring; 
        j <= g_nmd->last_tx_ring; j++) {
      /* compute current ring to use */
      struct netmap_ring *ring;
      uint32_t i, idx;

      ring = NETMAP_TXRING(g_nmd->nifp, j); 

      if (nm_ring_empty(ring)) {
         continue;
      }  

      i = ring->cur;
      idx = ring->slot[i].buf_idx;
      ring->slot[i].flags = 0;
      ring->slot[i].len = len;
      //g_nmd->cur_tx_ring = j;

      // saving info to trigger hw to send msg later.
      trigger->ring = ring;
      trigger->ring_idx = j;
      DEBUG(ctx->log,"get ring info: j=%d ptr=%p, idx=%d, cur=%d, len=%d",j,ring,idx,i,len);

      return NETMAP_BUF(ring,idx);
   }  
flush:
   // flush any remaining packets
   ioctl(fds.fd, NIOCTXSYNC, NULL);
   // final part: wait all the TX queues to be empty.
   for (i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
      struct netmap_ring *txring = NETMAP_TXRING(nifp, i);
      while (nm_tx_pending(txring)) {
         ioctl(fds.fd, NIOCTXSYNC, NULL);
         usleep(1); // wait 1 tick
      }
   }
   attemps++;
   goto resend;

failure:
   return 0; /* fail */
}

int
usnet_send_frame(usn_context_t *ctx, usn_buf_t *m)
{
   struct pollfd       fds; 
   struct netmap_if   *nifp;
   int                 ret;
   int                 size;
   char               *buf;
   int                 attemps = 0; 
   struct nm_desc     *g_nmd = ctx->nmd;
   int i, j;

   fds.fd = g_nmd->fd;
   nifp = g_nmd->nifp;

   if ( m == 0 )
      return -1;

   buf = m->start;
   size = m->len;
resend:
   if ( attemps==3 )
      return -2;

   if(ctx->ifnet->npkts >= ctx->ifnet->burst ){
      fds.events = POLLOUT;
      fds.revents = 0; 
      ctx->ifnet->npkts = 0; 

      ret = poll(&fds, 1, 2000);
      if (ret <= 0 ) {
         // TODO: save pending packets? 
         // as it is easy to reach line rate.
         goto fail;
      }    
      if (fds.revents & POLLERR) {
         struct netmap_ring *tx = NETMAP_RXRING(nifp, g_nmd->cur_tx_ring);
         ERROR(ctx->log,"error on em1, rx [%d,%d,%d]", tx->head, tx->cur, tx->tail);
         goto fail;
      }

      if (fds.revents & POLLOUT) {
         goto send;
      }
      goto flush;
   }
send:
   for (j = g_nmd->first_tx_ring;
        j <= g_nmd->last_tx_ring; j++) {
      struct netmap_ring *ring;
      uint32_t i, idx;
      ring = NETMAP_TXRING(nifp, j);

      if (nm_ring_empty(ring)) {
         continue;
      }

      i = ring->cur;
      idx = ring->slot[i].buf_idx;
      ring->slot[i].flags = 0;
      ring->slot[i].len = size;
      DEBUG(ctx->log,"len=%d",size);
      //dump_buffer(ctx, buf, size, "nm");
      nm_pkt_copy(buf, NETMAP_BUF(ring, idx), size);
      g_nmd->cur_tx_ring = j;
      ring->head = ring->cur = nm_ring_next(ring, i);
      ctx->ifnet->npkts++;

      return size;
   }

flush:
   // flush any remaining packets
   ioctl(fds.fd, NIOCTXSYNC, NULL);
   // final part: wait all the TX queues to be empty.
   for (i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
      struct netmap_ring *txring = NETMAP_TXRING(nifp, i);
      while (nm_tx_pending(txring)) {
         ioctl(fds.fd, NIOCTXSYNC, NULL);
         usleep(1); // wait 1 tick
      }
   }
   attemps++;
   goto resend;

fail:
   ERROR(ctx->log,"failed to send");
   return -3;
}

void
usnet_trigger_nm(usn_context_t *ctx, usn_nmring_t *trigger)
{
   struct netmap_ring *ring = trigger->ring;

   if ( ring == 0 ) {
      ERROR(ctx->log, "ring ptr is null");
      return;
   }
   trigger->ring = 0;

   DEBUG(ctx->log,"ring info: head=%d, cur=%d, tail=%d", 
         ring->head, ring->cur, ring->tail);

   dump_buffer(ctx,NETMAP_BUF(ring,ring->slot[ring->cur].buf_idx), 
         ring->slot[ring->cur].len,"trigger");
   ctx->nmd->cur_tx_ring = trigger->ring_idx;
   ring->head = ring->cur = nm_ring_next(ring, ring->cur);
   //ioctl(ctx->nmd->fd, NIOCTXSYNC, NULL);
   DEBUG(ctx->log,"ring info: head=%d, cur=%d, tail=%d", ring->head, ring->cur, ring->tail);
   return;
}

void
usnet_send_data(usn_context_t *ctx)
{
#ifdef USE_NETMAP_BUF
   usnet_trigger_nm(ctx, &ctx->trigger_ring);
#else
   usnet_send_frame(ctx, ctx->nm_buf);
#endif
}

char*
usnet_eth_output_nm(usn_context_t *ctx, int type, uint8_t *haddr, usn_nmring_t *trigger, int len)
{
   int i;
   char *buf;
   usn_ethhdr_t *ethh;
#ifdef USE_NETMAP_BUF
   buf = usnet_get_nmbuf(ctx,trigger,len + sizeof(*ethh));
#else
   buf = ctx->nm_buf->start;
   ctx->nm_buf->len = len + sizeof(*ethh);
#endif 
   ethh = (usn_ethhdr_t*)buf;
   for ( i=0; i<ETH_LEN; ++i) {
      ethh->dest[i] = haddr[i];
      ethh->source[i] = ctx->ifnet->hwa[i];
   }
   ethh->type = type;

   return (char*)(ethh + 1);
}





