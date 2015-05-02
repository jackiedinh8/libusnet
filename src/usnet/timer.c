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
 * @(#)timer.c
 */

#include <assert.h>

#include "timer.h"
#include "core.h"
#include "log.h"
#include "tcp_out.h"
#include "event.h"
#include "utils.h"

struct rto_hashstore*
usnet_rtohash_init(usn_context_t *ctx)
{     
   int i;
   struct rto_hashstore* hs = calloc(1, sizeof(struct rto_hashstore));
   if (!hs) {
      ERROR(ctx->log,"calloc: InitHashStore");
      return 0;
   }

   for (i = 0; i < RTO_HASH; i++) 
      TAILQ_INIT(&hs->rto_list[i]);
       
   TAILQ_INIT(&hs->rto_list[RTO_HASH]);
   
   ctx->rto_store = hs;

   return hs;
}

inline void
usnet_add_timewait_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{     
   cur_tcb->rcvvar->ts_tw_expire = ctx->cur_time + ctx->tcp_timewait;
         
   if (cur_tcb->on_timewait_list) {
      // Update list in sorted way by ts_tw_expire
      TAILQ_REMOVE(&ctx->timewait_list, cur_tcb, sndvar->timer_link);
      TAILQ_INSERT_TAIL(&ctx->timewait_list, cur_tcb, sndvar->timer_link);
   } else {
      if (cur_tcb->on_rto_idx >= 0) { 
         DEBUG(ctx->log,"cannot be in both timewait and rto list, fd=%u", cur_tcb->fd);
         assert(0);
#ifdef DUMP_STREAM
         usnet_dump_tcb(ctx, cur_tcb);
#endif
         usnet_remove_rto_list(ctx, cur_tcb);
      }

      cur_tcb->on_timewait_list = 1;
      TAILQ_INSERT_TAIL(&ctx->timewait_list, cur_tcb, sndvar->timer_link);
      ctx->timewait_list_cnt++;
   }
}

inline void 
usnet_update_retransmission_timer(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
   /* Update the retransmission timer */
   assert(cur_tcb->sndvar->rto > 0);
   cur_tcb->sndvar->nrtx = 0;

   /* if in rto list, remove it */
   if (cur_tcb->on_rto_idx >= 0) {
      usnet_remove_rto_list(ctx, cur_tcb);
   }

   /* Reset retransmission timeout */
   if (TCP_SEQ_GT(cur_tcb->snd_nxt, cur_tcb->sndvar->snd_una)) {
      /* there are packets sent but not acked */
      /* update rto timestamp */
      cur_tcb->sndvar->ts_rto = ctx->cur_time + cur_tcb->sndvar->rto;
      usnet_add_rto_list(ctx, cur_tcb);

   } else {
      /* all packets are acked */
      DEBUG(ctx->log,"All packets are acked. snd_una: %u, snd_nxt: %u",
            cur_tcb->sndvar->snd_una, cur_tcb->snd_nxt);
   }
}

inline void 
usnet_remove_timewait_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{           
   if (!cur_tcb->on_timewait_list) {
      assert(0);
      return;
   }  
   
   TAILQ_REMOVE(&ctx->timewait_list, cur_tcb, sndvar->timer_link);
   cur_tcb->on_timewait_list = 0;
   ctx->timewait_list_cnt--;
}     

inline void
usnet_add_timeout_list(usn_context_t* ctx, usn_tcb_t *cur_tcb)
{
   if (cur_tcb->on_timeout_list) {
      assert(0);
      return;
   }     
   cur_tcb->on_timeout_list = 1;
   TAILQ_INSERT_TAIL(&ctx->timeout_list, cur_tcb, sndvar->timeout_link);
   ctx->timeout_list_cnt++;
}  

inline void 
usnet_remove_timeout_list(usn_context_t *ctx, usn_tcb_t *tcb)
{  
   if (tcb->on_timeout_list) {
      tcb->on_timeout_list = 0;
      TAILQ_REMOVE(&ctx->timeout_list, tcb, sndvar->timeout_link);
      ctx->timeout_list_cnt--;
   }  
}  

inline void  
usnet_update_timeout_list(usn_context_t *ctx, usn_tcb_t *tcb)
{
   if (tcb->on_timeout_list) {
      TAILQ_REMOVE(&ctx->timeout_list, tcb, sndvar->timeout_link);
      TAILQ_INSERT_TAIL(&ctx->timeout_list, tcb, sndvar->timeout_link);
   }
}    

inline void 
usnet_add_rto_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{     

   DEBUG(ctx->log,"add stream to rto list: fd=%u, state=%s",
         cur_tcb->fd,usnet_tcp_statestr(cur_tcb));

   if (!ctx->rto_list_cnt) {
      ctx->rto_store->rto_now_idx = 0;
      ctx->rto_store->rto_now_ts = cur_tcb->sndvar->ts_rto;
   }

   if (cur_tcb->on_rto_idx < 0 ) {
      if (cur_tcb->on_timewait_list) {
         ERROR(ctx->log,"cannot be in both rto and timewait list, fd=%u", 
               cur_tcb->fd);
#ifdef DUMP_STREAM
         usnet_dump_tcb(ctx, cur_tcb);
#endif
         return;
      }
   
      int diff = (int32_t)(cur_tcb->sndvar->ts_rto - ctx->rto_store->rto_now_ts);
      if (diff < RTO_HASH) {
         int offset= (diff + ctx->rto_store->rto_now_idx) % RTO_HASH;
         cur_tcb->on_rto_idx = offset;
         TAILQ_INSERT_TAIL(&(ctx->rto_store->rto_list[offset]),
               cur_tcb, sndvar->timer_link);
      } else {
         cur_tcb->on_rto_idx = RTO_HASH;
         TAILQ_INSERT_TAIL(&(ctx->rto_store->rto_list[RTO_HASH]),
               cur_tcb, sndvar->timer_link);
      }
      ctx->rto_list_cnt++;
   }
}

inline void 
usnet_remove_rto_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{

   if (cur_tcb->on_rto_idx < 0) {
      //assert(0);
      return;
   }   
   
   TAILQ_REMOVE(&ctx->rto_store->rto_list[cur_tcb->on_rto_idx], 
         cur_tcb, sndvar->timer_link);
   cur_tcb->on_rto_idx = -1; 

   ctx->rto_list_cnt--;
}

int
usnet_handle_rto(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
   uint8_t backoff;

   DEBUG(ctx->log,"timeout, fd=%u, rto=%u (%ums), snd_una=%u, snd_nxt=%u",
         cur_tcb->fd, cur_tcb->sndvar->rto, TS_TO_MSEC(cur_tcb->sndvar->rto),
         cur_tcb->sndvar->snd_una, cur_tcb->snd_nxt);
   assert(cur_tcb->sndvar->rto > 0);

   /* count number of retransmissions */
   if (cur_tcb->sndvar->nrtx < TCP_MAX_RTX) {
      cur_tcb->sndvar->nrtx++;
   } else {
      /* if it exceeds the threshold, destroy and notify to application */
      DEBUG(ctx->log,"exceed MAX_RTX, fd=%u", cur_tcb->fd);
      if (cur_tcb->state < TCPS_ESTABLISHED) {
         cur_tcb->state = TCPS_CLOSED;
         cur_tcb->close_reason = TCP_CONN_FAIL;
         usnet_release_tcb(ctx, cur_tcb);
      } else {
         cur_tcb->state = TCPS_CLOSED;
         cur_tcb->close_reason = TCP_CONN_LOST;
         DEBUG(ctx->log,"tcp state: fd=%u, state=%s",cur_tcb->fd,usnet_tcp_statestr(cur_tcb));
         if (cur_tcb->socket) {
            usnet_raise_error_event(ctx, cur_tcb);
         } else {
            usnet_release_tcb(ctx, cur_tcb);
         }
      }

      return -1;
   }
   if (cur_tcb->sndvar->nrtx > cur_tcb->sndvar->max_nrtx) {
      cur_tcb->sndvar->max_nrtx = cur_tcb->sndvar->nrtx;
   }
   /* update rto timestamp */
   if (cur_tcb->state >= TCPS_ESTABLISHED) {
      uint32_t rto_prev;
      backoff = MIN(cur_tcb->sndvar->nrtx, TCP_MAX_BACKOFF);

      rto_prev = cur_tcb->sndvar->rto;
      cur_tcb->sndvar->rto =
            ((cur_tcb->rcvvar->srtt >> 3) + cur_tcb->rcvvar->rttvar) << backoff;
      if (cur_tcb->sndvar->rto <= 0) {
         DEBUG(ctx->log,"fd=%%d, rto=%u, prev=%u, state=%s",
               cur_tcb->fd, cur_tcb->sndvar->rto, rto_prev,
               usnet_tcp_statestr(cur_tcb));
         cur_tcb->sndvar->rto = rto_prev;
      }
   }
   //cur_tcb->sndvar->ts_rto = cur_ts + cur_tcb->sndvar->rto;

   /* reduce congestion window and ssthresh */
   cur_tcb->sndvar->ssthresh = MIN(cur_tcb->sndvar->cwnd, cur_tcb->sndvar->peer_wnd) / 2;
   if (cur_tcb->sndvar->ssthresh < (2 * cur_tcb->sndvar->mss)) {
      cur_tcb->sndvar->ssthresh = cur_tcb->sndvar->mss * 2;
   }
   cur_tcb->sndvar->cwnd = cur_tcb->sndvar->mss;
   DEBUG(ctx->log,"timeout, fd=%u, cwnd=%u, ssthresh=%u",
         cur_tcb->fd, cur_tcb->sndvar->cwnd, cur_tcb->sndvar->ssthresh);

   /* Retransmission */
   if (cur_tcb->state == TCPS_SYN_SENT) {
      /* SYN lost */
      if (cur_tcb->sndvar->nrtx > TCP_MAX_SYN_RETRY) {
         cur_tcb->state = TCPS_CLOSED;
         cur_tcb->close_reason = TCP_CONN_FAIL;
         DEBUG(ctx->log,"SYN retries exceed maximum retries, fd=%u",
               cur_tcb->fd);
         if (cur_tcb->socket) {
            usnet_raise_error_event(ctx, cur_tcb);
         } else {
            usnet_release_tcb(ctx, cur_tcb);
         }

         return -2;
      }
      DEBUG(ctx->log,"retransmit SYN, fd=%u, snd_nxt=%u, snd_una=%u",
            cur_tcb->fd, cur_tcb->snd_nxt, cur_tcb->sndvar->snd_una);

   } else if (cur_tcb->state == TCPS_SYN_RECEIVED) {
      /* SYN/ACK lost */
      DEBUG(ctx->log,"retransmit SYN/ACK, fd=%u, snd_nxt=%u, snd_una=%u",
            cur_tcb->fd, cur_tcb->snd_nxt, cur_tcb->sndvar->snd_una);

   } else if (cur_tcb->state == TCPS_ESTABLISHED) {
      /* Data lost */
      DEBUG(ctx->log,"retransmit data, fd=%u, snd_nxt=%u, snd_una=%u",
            cur_tcb->fd, cur_tcb->snd_nxt, cur_tcb->sndvar->snd_una);

   } else if (cur_tcb->state == TCPS_CLOSE_WAIT) {
      /* Data lost */
      DEBUG(ctx->log,"retransmit data, fd=%u, snd_nxt=%u, snd_una=%u",
            cur_tcb->fd, cur_tcb->snd_nxt, cur_tcb->sndvar->snd_una);

   } else if (cur_tcb->state == TCPS_LAST_ACK) {
      /* FIN/ACK lost */
      DEBUG(ctx->log,"retransmit FIN/ACK, fd=%u, snd_nxt=%u, snd_una=%u",
            cur_tcb->fd, cur_tcb->snd_nxt, cur_tcb->sndvar->snd_una);
   } else if (cur_tcb->state == TCPS_FIN_WAIT_1) {
      /* FIN lost */
      DEBUG(ctx->log,"retransmit FIN, fd=%u, snd_nxt=%u, snd_una=%u",
            cur_tcb->fd, cur_tcb->snd_nxt, cur_tcb->sndvar->snd_una);
   } else if (cur_tcb->state == TCPS_CLOSING) {
      DEBUG(ctx->log,"retransmit ACK, fd=%u, snd_nxt=%u, snd_una=%u",
            cur_tcb->fd, cur_tcb->snd_nxt, cur_tcb->sndvar->snd_una);

   } else {
      DEBUG(ctx->log,"not implemented state, fd=%u, state=%s, rto=%u",
            cur_tcb->fd,
            usnet_tcp_statestr(cur_tcb), cur_tcb->sndvar->rto);

      usnet_raise_error_event(ctx, cur_tcb);
      usnet_release_tcb(ctx, cur_tcb);

      return -3;
   }

   cur_tcb->snd_nxt = cur_tcb->sndvar->snd_una;
   if (cur_tcb->state == TCPS_ESTABLISHED ||
         cur_tcb->state == TCPS_CLOSE_WAIT) {
      /* retransmit data at ESTABLISHED state */
      usnet_add_send_list(ctx, cur_tcb);

   } else if (cur_tcb->state == TCPS_FIN_WAIT_1 ||
         cur_tcb->state == TCPS_CLOSING ||
         cur_tcb->state == TCPS_LAST_ACK) {

      if (cur_tcb->sndvar->fss == 0) {
         ERROR(ctx->log,"fss not set, fd=%u", cur_tcb->fd);
      }
      /* decide to retransmit data or control packet */
      if (TCP_SEQ_LT(cur_tcb->snd_nxt, cur_tcb->sndvar->fss)) {
         /* need to retransmit data */
         if (cur_tcb->sndvar->on_control_list) {
            usnet_remove_control_list(ctx, cur_tcb);
         }
         cur_tcb->control_list_waiting = 1;
         usnet_add_send_list(ctx, cur_tcb);
      } else {
         /* need to retransmit control packet */
         usnet_add_control_list(ctx, cur_tcb);
      }

   } else {
      usnet_add_control_list(ctx, cur_tcb);
   }

   return 0;
}

void
usnet_check_rtm_timeout(usn_context_t *ctx, int thresh)
{
   usn_tcb_t *walk, *next;
   struct rto_head* rto_list;
   int cnt;
   int ret;
   
   if (!ctx->rto_list_cnt) {
      return;
   }

   cnt = 0;
            
   while (1) {
      
      rto_list = &ctx->rto_store->rto_list[ctx->rto_store->rto_now_idx];
      if ((int32_t)(ctx->cur_time - ctx->rto_store->rto_now_ts) < 0) {
         break;
      }

      for (walk = TAILQ_FIRST(rto_list);
            walk != NULL; walk = next) {
         if (++cnt > thresh) {
            break;
         }
         next = TAILQ_NEXT(walk, sndvar->timer_link);

         DEBUG(ctx->log,"Inside rto list. cnt: %u, fd=%h",
               cnt, walk->fd);

         if (walk->on_rto_idx >= 0) {
            ret = usnet_handle_rto(ctx, walk);
            TAILQ_REMOVE(rto_list, walk, sndvar->timer_link);
            ctx->rto_list_cnt--;
            walk->on_rto_idx = -1;
         } else {
            DEBUG(ctx->log,"not on rto list, fd=%u", walk->fd);
#ifdef DUMP_STREAM
            usnet_dump_tcb(ctx, walk);
#endif
         }
      }

      if (cnt > thresh) {
         break;
      } else {
         ctx->rto_store->rto_now_idx = (ctx->rto_store->rto_now_idx + 1) % RTO_HASH;
         ctx->rto_store->rto_now_ts++;
         if (!(ctx->rto_store->rto_now_idx % 1000)) {
            usnet_rearrange_rto_store(ctx);
         }
      }

   }

   DEBUG(ctx->log,"Checking retransmission timeout. cnt: %d", cnt);

   (void)(ret);

}

void
usnet_check_timewait_expire(usn_context_t *ctx, int thresh)
{
   usn_tcb_t  *walk, *next;
   int cnt;

   cnt = 0;

   for (walk = TAILQ_FIRST(&ctx->timewait_list);
            walk != NULL; walk = next) {
      if (++cnt > thresh)
         break;
      next = TAILQ_NEXT(walk, sndvar->timer_link);

      DEBUG(ctx->log,"Inside timewait list. cnt=%u, fd=%u",
            cnt, walk->fd);

      if (walk->on_timewait_list) {
         if ((int32_t)(ctx->cur_time - walk->rcvvar->ts_tw_expire) >= 0) {
            if (!walk->sndvar->on_control_list) {

               TAILQ_REMOVE(&ctx->timewait_list, walk, sndvar->timer_link);
               walk->on_timewait_list = 0;
               ctx->timewait_list_cnt--;

               walk->state = TCPS_CLOSED;
               walk->close_reason = TCP_ACTIVE_CLOSE;
               DEBUG(ctx->log,"waitime expired, fd=%u", walk->fd);
               usnet_release_tcb(ctx, walk);
            }
         } else {
            break;
         }
      } else {
         ERROR(ctx->log,"not on timewait list, fd=%u", walk->fd);
#ifdef DUMP_STREAM
         usnet_dump_tcb(ctx, walk);
#endif
      }
   }

   DEBUG(ctx->log,"Checking timewait timeout. cnt: %d", cnt);
}

void
usnet_check_connection_timeout(usn_context_t *ctx, int thresh)
{
   usn_tcb_t *walk, *next;
   int cnt;

   cnt = 0;
   for (walk = TAILQ_FIRST(&ctx->timeout_list);
         walk != NULL; walk = next) {
      if (++cnt > thresh)
         break;
      next = TAILQ_NEXT(walk, sndvar->timeout_link);

      if ((int32_t)(ctx->cur_time - walk->last_active_ts) >=
            ctx->tcp_timeout) {

         walk->on_timeout_list = 0;
         TAILQ_REMOVE(&ctx->timeout_list, walk, sndvar->timeout_link);
         ctx->timeout_list_cnt--;
         walk->state = TCPS_CLOSED;
         walk->close_reason = TCP_TIMEDOUT;
         if (walk->socket) {
            usnet_raise_error_event(ctx, walk);
         } else {
            usnet_release_tcb(ctx, walk);
         }
      } else {
         break;
      }
   }

   DEBUG(ctx->log,"Checking connection timeout. cnt: %d", cnt);
}

inline void 
usnet_rearrange_rto_store(usn_context_t *ctx)
 {
   usn_tcb_t *walk, *next;
   struct rto_head* rto_list = &ctx->rto_store->rto_list[RTO_HASH];
   int cnt = 0;

   for (walk = TAILQ_FIRST(rto_list);
         walk != NULL; walk = next) {
      next = TAILQ_NEXT(walk, sndvar->timer_link);

      int diff = (int32_t)(ctx->rto_store->rto_now_ts - walk->sndvar->ts_rto);
      if (diff < RTO_HASH) {
         int offset = (diff + ctx->rto_store->rto_now_idx) % RTO_HASH;
         TAILQ_REMOVE(&ctx->rto_store->rto_list[RTO_HASH],
                           walk, sndvar->timer_link);
         walk->on_rto_idx = offset;
         TAILQ_INSERT_TAIL(&(ctx->rto_store->rto_list[offset]),
               walk, sndvar->timer_link);
      }   
      cnt++;
   }   
}




