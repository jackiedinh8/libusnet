/*
 * Copyright (C) 2015 EunYoung Jeong, Shinae Woo, Muhammad Jamshed, Haewon Jeong, 
 *                    Sunghwan Ihm, Dongsu Han, KyoungSoo Park
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the 
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the 
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
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
 * @(#)tcb.c
 */

#include <netinet/in.h>

#include "tcb.h"
#include "socket.h"
#include "core.h"
#include "log.h"
#include "utils.h"
#include "sndbuf.h"
#include "ringbuf.h"
#include "mempool.h"
#include "tcp_out.h"
#include "timer.h"

int 
usnet_tcb_key(const void *item)
{
   usn_tcb_t *tcb =  (usn_tcb_t *)item;
   unsigned int hash, i;

   char *key = (char *)&tcb->saddr;

   for (hash = i = 0; i < 12; ++i) {
      hash += key[i];
      hash += (hash << 10);
      hash ^= (hash >> 6); 
   }   
   hash += (hash << 3); 
   hash ^= (hash >> 11);
   hash += (hash << 15);

   return hash;
}

int 
usnet_tcb_eq(const void *arg1, const void *arg2)
{
   usn_tcb_t *item1 = (usn_tcb_t *)arg1;
   usn_tcb_t *item2 = (usn_tcb_t *)arg2;
   return (item1->saddr == item2->saddr) &&
          (item1->daddr == item2->daddr) &&
          (item1->sport == item2->sport) &&
          (item1->dport == item2->dport);
}

int
usnet_tcb_isempty(const void *arg)
{
   usn_tcb_t *item = (usn_tcb_t *)arg;
   return (item->saddr == 0) && (item->daddr == 0) &&
          (item->sport == 0) && (item->dport == 0);
}

int
usnet_tcb_setempty(const void *arg)
{
   usn_tcb_t *item = (usn_tcb_t *)arg;
   item->saddr = 0;
   item->daddr = 0;
   item->sport = 0;
   item->dport = 0;
   return 0;
}

void
usnet_tcb_init(usn_context_t *ctx)
{
   usnet_cache_init(ctx->tcb_cache, 0x33339, 10, 100, sizeof(usn_tcb_t),1,
                    usnet_tcb_eq, usnet_tcb_key, usnet_tcb_isempty, usnet_tcb_setempty);
   return;
}

static inline usn_tcb_t*
usnet_init_newtcb(usn_context_t *ctx, usn_socket_t *so, int type,
      uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport)
{
   usn_tcb_t *tcb = 0;
   usn_tcb_t stcb;
   
   memset(&stcb, 0, sizeof(usn_tcb_t));
   stcb.saddr = saddr;  
   stcb.sport = sport;
   stcb.daddr = daddr;  
   stcb.dport = dport;
  
   tcb = CACHE_INSERT(ctx->tcb_cache, &stcb, usn_tcb_t*);
   if ( tcb == NULL ) {
      ERROR(ctx->log,"failed to allocate tcb");
      return NULL;
   }
   tcb->fd = ctx->listen_tcb->fd;

   //tcb->rcvvar = (usn_rcvvars_t*)usnet_malloc(ctx,sizeof(usn_rcvvars_t));
   tcb->rcvvar = usnet_mempool_allocate(ctx->rcvvars_mp);
   if ( tcb->rcvvar == NULL ) {
      CACHE_REMOVE(ctx->tcb_cache,tcb);
      return NULL;
   }

   //tcb->sndvar = (usn_sndvars_t*)usnet_malloc(ctx,sizeof(usn_sndvars_t));
   tcb->sndvar = usnet_mempool_allocate(ctx->sndvars_mp);
   if ( tcb->sndvar == NULL ) {
      usnet_mempool_free(ctx->rcvvars_mp, tcb->rcvvar);
      CACHE_REMOVE(ctx->tcb_cache,tcb);
      return NULL;
   }

   memset(tcb->rcvvar, 0, sizeof(usn_rcvvars_t));
   memset(tcb->sndvar, 0, sizeof(usn_sndvars_t));

   tcb->on_hash_table = 1;
   ctx->flow_cnt++;


   if (so) {
      tcb->socket = so;
      tcb->fd = so->so_fd;
      so->so_tcb = tcb;
   }
   
   tcb->type = type;
   tcb->state = TCPS_LISTEN;
   tcb->on_rto_idx = -1;

   tcb->sndvar->ip_id = 0;
   tcb->sndvar->mss = TCP_DEFAULT_MSS;
   tcb->sndvar->wscale = TCP_DEFAULT_WSCALE;
   //tcb->sndvar->nif_out = GetOutputInterface(stream->daddr);
   tcb->sndvar->nif_out = 0;

   tcb->sndvar->iss = rand() % TCP_MAX_SEQ;
   //stream->sndvar->iss = 0;
   tcb->rcvvar->irs = 0;

   tcb->snd_nxt = tcb->sndvar->iss;
   tcb->sndvar->snd_una = tcb->sndvar->iss;
   tcb->sndvar->snd_wnd = TCP_SNDBUF_SIZE;
   tcb->rcv_nxt = 0;
   tcb->rcvvar->rcv_wnd = TCP_INITIAL_WINDOW;

   tcb->rcvvar->snd_wl1 = tcb->rcvvar->irs - 1;

   tcb->sndvar->rto = TCP_INITIAL_RTO;
/*  
   { 
      uint8_t *sa; 
      uint8_t *da;
      (void)sa; (void)da;
      sa = (uint8_t *)&tcb->saddr;
      da = (uint8_t *)&tcb->daddr;
      DEBUG(ctx->log,"new tcp connect %d: "
         "%u.%u.%u.%u(%d) -> %u.%u.%u.%u(%d) (ISS: %u)", tcb->fd,
         sa[0], sa[1], sa[2], sa[3], ntohs(tcb->sport),
         da[0], da[1], da[2], da[3], ntohs(tcb->dport),
         tcb->sndvar->iss);
   }
*/
   return tcb;
}

static inline void
usnet_parse_tcpopts(usn_context_t *ctx, usn_tcb_t *cur_tcb, 
                    uint8_t *tcpopt, int len)
{
   int i;
   unsigned int opt, optlen;

   for (i = 0; i < len; ) {
      opt = *(tcpopt + i++);
      
      if (opt == TCPOPT_EOL) {  // end of option field
         break;
      } else if (opt == TCPOPT_NOP) { // no option
         continue;
      } else {
   
         optlen = *(tcpopt + i++);
         if (i + optlen - 2 > len) {
            break;
         }

         if (opt == TCPOPT_MAXSEG) {
            cur_tcb->sndvar->mss = *(tcpopt + i++) << 8;
            cur_tcb->sndvar->mss += *(tcpopt + i++);
            cur_tcb->sndvar->eff_mss = cur_tcb->sndvar->mss;
#if TCPOPT_TIMESTAMP_ENABLED
            cur_tcb->sndvar->eff_mss -= (TCPOLEN_TIMESTAMP + 2);
#endif
         } else if (opt == TCPOPT_WINDOW) {
            cur_tcb->sndvar->wscale = *(tcpopt + i++);
         } else if (opt == TCPOPT_SACK_PERMITTED) {
            cur_tcb->sack_permit = 1;
         } else if (opt == TCPOPT_TIMESTAMP) {
            cur_tcb->saw_timestamp = 1;
            cur_tcb->rcvvar->ts_recent = ntohl(*(uint32_t *)(tcpopt + i));
            cur_tcb->rcvvar->ts_last_ts_upd = ctx->cur_time;
            i += 8;
         } else {
            // not handle
            i += optlen - 2;
         }
      }
   }
}

usn_tcb_t*
usnet_create_tcb(usn_context_t *ctx, usn_iphdr_t *iph, 
                 usn_tcphdr_t *tcph, uint32_t seq, uint16_t window)
{
   usn_tcb_t *tcb = 0;

   tcb = usnet_init_newtcb(ctx, 0, 0, iph->ip_dst, 
         tcph->th_dport, iph->ip_src, tcph->th_sport);

   if ( tcb == NULL )
      return NULL;

   tcb->rcvvar->irs = seq; 
   tcb->sndvar->peer_wnd = window;
   tcb->rcv_nxt = tcb->rcvvar->irs;
   tcb->sndvar->cwnd = 1; 
   usnet_parse_tcpopts(ctx, tcb, (uint8_t *)tcph + TCP_HEADER_LEN, 
         (tcph->th_off << 2) - TCP_HEADER_LEN);

   return tcb;
}

int
usnet_register_socket(usn_context_t *ctx, usn_socket_t *so, 
      uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport)
{
   usn_tcb_t *tcb = 0;
   usn_tcb_t stcb;

   memset(&stcb, 0, sizeof(usn_tcb_t));
   stcb.saddr = saddr;  
   stcb.sport = sport;
   stcb.daddr = daddr;  
   stcb.dport = dport;
  
   tcb = CACHE_INSERT(ctx->tcb_cache, &stcb, usn_tcb_t*);
   if ( tcb == 0 ) {
      ERROR(ctx->log,"failed to allocate tcb");
      return -1;
   }

   tcb->on_hash_table = 1;

   if (so) {
      tcb->socket = so;
      tcb->fd = so->so_fd;
      so->so_tcb = tcb;
   } else {
      CACHE_REMOVE(ctx->tcb_cache, tcb);  
      return -2;
   }

   tcb->type = 0;
   tcb->state = TCPS_LISTEN;
   tcb->on_rto_idx = -1;

   ctx->listen_tcb = tcb;
    
   return 0;
}

inline usn_tcb_t*
usnet_search_tcb(usn_context_t *ctx, usn_tcb_t *sitem)
{
   return (usn_tcb_t*)(usnet_cache_search(ctx->tcb_cache, sitem));
}

int 
usnet_tcb_eq_new(const void *arg1, const void *arg2)
{
   usn_tcb_t *item1 = (usn_tcb_t *)arg1;
   usn_tcb_t *item2 = (usn_tcb_t *)arg2;
   return (item1->fd == item2->fd);
}

inline usn_tcb_t*
usnet_search_tcb_new(usn_context_t *ctx, usn_tcb_t *sitem)
{
   return (usn_tcb_t*)(usnet_cache_search_new(ctx->tcb_cache, sitem, usnet_tcb_eq_new));
}

inline usn_tcb_t*
usnet_insert_tcb(usn_context_t *ctx, usn_tcb_t *sitem) 
{
   return (usn_tcb_t*)usnet_cache_insert(ctx->tcb_cache, sitem);
}

inline int
usnet_remove_tcb(usn_context_t *ctx, usn_tcb_t *sitem) 
{
   return usnet_cache_remove(ctx->tcb_cache, sitem);
}

void
usnet_release_tcb(usn_context_t *ctx, usn_tcb_t *tcb)
{
   usnet_remove_control_list(ctx,tcb);
   usnet_remove_send_list(ctx,tcb);
   usnet_remove_ack_list(ctx,tcb);

   if (tcb->on_rto_idx >= 0)
      usnet_remove_rto_list(ctx,tcb);
   
   if (tcb->on_timewait_list)
      usnet_remove_timewait_list(ctx,tcb);

   if (ctx->tcp_timeout > 0)
      usnet_remove_timeout_list(ctx,tcb);

   CACHE_REMOVE(ctx->sndbuf_cache, tcb->sndvar->sndbuf);
   CACHE_REMOVE(ctx->ringbuf_cache, tcb->rcvvar->rcvbuf);
   usnet_mempool_free(ctx->rcvvars_mp, tcb->rcvvar);
   usnet_mempool_free(ctx->sndvars_mp, tcb->sndvar);
   CACHE_REMOVE(ctx->tcb_cache,tcb);
}

char *close_reason_str[] = {
   "NOT_CLOSED",
   "CLOSE", 
   "CLOSED",  
   "CONN_FAIL",
   "CONN_LOST",
   "RESET",
   "NO_MEM",
   "DENIED", 
   "TIMEDOUT"
}; 

void
usnet_dump_tcb(usn_context_t *ctx, usn_tcb_t *stream)
{
   uint8_t *sa, *da;
   usn_sndvars_t *sndvar = stream->sndvar;
   usn_rcvvars_t *rcvvar = stream->rcvvar;
   
   (void)sa;(void)da;   
   sa = (uint8_t *)&stream->saddr;
   da = (uint8_t *)&stream->daddr;
   DEBUG(ctx->log, "socket fd %u: " "%u.%u.%u.%u(%u) -> %u.%u.%u.%u(%u)", stream->fd,
         sa[0], sa[1], sa[2], sa[3], ntohs(stream->sport),
         da[0], da[1], da[2], da[3], ntohs(stream->dport));
   DEBUG(ctx->log, "state: %s, close_reason: %s", usnet_tcp_statestr(stream), 
         close_reason_str[stream->close_reason]);
   if (stream->socket) {
      usn_socket_t *socket = stream->socket;
      (void)socket;
      DEBUG(ctx->log, "Socket fd: %u", socket->so_fd);
   } else {
      DEBUG(ctx->log, "Socket: (null)");
   }

   DEBUG(ctx->log,
         "on_hash_table: %u, on_control_list: %u (wait: %u), on_send_list: %u, "
         "on_ack_list: %u, is_wack: %u, ack_cnt: %u"
         "on_rto_idx: %d, on_timewait_list: %u, on_timeout_list: %u, "
         "on_rcv_br_list: %u, on_snd_br_list: %u"
         "on_sendq: %u, on_ackq: %u, closed: %u, on_closeq: %u, "
         "on_closeq_int: %u, on_resetq: %u, on_resetq_int: %u"
         "have_reset: %u, is_fin_sent: %u, is_fin_ackd: %u, "
         "saw_timestamp: %u, sack_permit: %u, "
         "is_bound_addr: %u, need_wnd_adv: %u", stream->on_hash_table,
         sndvar->on_control_list, stream->control_list_waiting, sndvar->on_send_list,
         sndvar->on_ack_list, sndvar->is_wack, sndvar->ack_cnt,
         stream->on_rto_idx, stream->on_timewait_list, stream->on_timeout_list,
         stream->on_rcv_br_list, stream->on_snd_br_list,
         sndvar->on_sendq, sndvar->on_ackq,
         stream->closed, sndvar->on_closeq, sndvar->on_closeq_int,
         sndvar->on_resetq, sndvar->on_resetq_int,
         stream->have_reset, sndvar->is_fin_sent,
         sndvar->is_fin_ackd, stream->saw_timestamp, stream->sack_permit,
         stream->is_bound_addr, stream->need_wnd_adv);

   DEBUG(ctx->log, "Send variables:");
   DEBUG(ctx->log, "ip_id: %u, mss: %u, eff_mss: %u, wscale: %u, nif_out: %d",
         sndvar->ip_id, sndvar->mss, sndvar->eff_mss,
         sndvar->wscale, sndvar->nif_out);
   DEBUG(ctx->log, "snd_nxt: %u, snd_una: %u, iss: %u, fss: %usnd_wnd: %u, "
         "peer_wnd: %u, cwnd: %u, ssthresh: %u",
         stream->snd_nxt, sndvar->snd_una, sndvar->iss, sndvar->fss,
         sndvar->snd_wnd, sndvar->peer_wnd, sndvar->cwnd, sndvar->ssthresh);

   if (sndvar->sndbuf) {
      DEBUG(ctx->log, "Send buffer: init_seq: %u, head_seq: %u, "
            "len: %d, cum_len: %lu, size: %d, head: %u, tail: %u",
            sndvar->sndbuf->init_seq, sndvar->sndbuf->head_seq,
            sndvar->sndbuf->len, sndvar->sndbuf->cum_len, sndvar->sndbuf->size);
   } else {
      DEBUG(ctx->log, "Send buffer: (null)");
   }
   DEBUG(ctx->log,
         "nrtx: %u, max_nrtx: %u, rto: %u, ts_rto: %u, "
         "ts_lastack_sent: %u", sndvar->nrtx, sndvar->max_nrtx,
         sndvar->rto, sndvar->ts_rto, sndvar->ts_lastack_sent);

   DEBUG(ctx->log,
         "Receive variables:");
   DEBUG(ctx->log,
         "rcv_nxt: %u, irs: %u, rcv_wnd: %u, "
         "snd_wl1: %u, snd_wl2: %u",
         stream->rcv_nxt, rcvvar->irs,
         rcvvar->rcv_wnd, rcvvar->snd_wl1, rcvvar->snd_wl2);
   if (rcvvar->rcvbuf) {
      DEBUG(ctx->log, "Receive buffer: init_seq: %u, head_seq: %u, "
            "last_seq: %u, merged_len: %lu, win: %d, size: %d, data: %d, head: %d",
            /*rcvvar->rcvbuf->init_seq*/0, rcvvar->rcvbuf->head_seq,
            rcvvar->rcvbuf->last_seq, rcvvar->rcvbuf->merged_len, 
            rcvvar->rcvbuf->win, rcvvar->rcvbuf->size, rcvvar->rcvbuf->data, rcvvar->rcvbuf->head);
   } else {
      DEBUG(ctx->log, "Receive buffer: (null)");
   }
   DEBUG(ctx->log, "last_ack_seq: %u, dup_acks: %u",
         rcvvar->last_ack_seq, rcvvar->dup_acks);
   DEBUG(ctx->log, "ts_recent: %u, ts_lastack_rcvd: %u, ts_last_ts_upd: %u, "
         "ts_tw_expire: %u", rcvvar->ts_recent, rcvvar->ts_lastack_rcvd,
         rcvvar->ts_last_ts_upd, rcvvar->ts_tw_expire);
   DEBUG(ctx->log, "srtt: %u, mdev: %u, mdev_max: %u, rttvar: %u, rtt_seq: %u",
         rcvvar->srtt, rcvvar->mdev, rcvvar->mdev_max,
         rcvvar->rttvar, rcvvar->rtt_seq);

}
