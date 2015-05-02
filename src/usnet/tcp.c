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
 * @(#)tcp.c
 */

#include <netinet/in.h>
#include <sys/queue.h>
#include <assert.h>

#include "tcp.h"
#include "tcb.h"
#include "utils.h"
#include "tcp_out.h"
#include "log.h"
#include "core.h"
#include "timer.h"
#include "ringbuf.h"
#include "event.h"
#include "sndbuf.h"


void
usnet_tcp_init(usn_context_t *ctx)
{
	DEBUG(ctx->log, "tcp_init");
   usnet_tcb_init(ctx);
}

inline int  
usnet_parse_tcptimestamp(usn_tcb_t *cur_tcb, 
      struct tcp_timestamp *ts, uint8_t *tcpopt, int len)
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
               
         if (opt == TCPOPT_TIMESTAMP) {
            ts->ts_val = ntohl(*(uint32_t *)(tcpopt + i));
            ts->ts_ref = ntohl(*(uint32_t *)(tcpopt + i + 4));
            return 0;
         } else {
            // not handle
            i += optlen - 2;
         }     
      }        
   }        
   return -1;
}              

inline void
usnet_estimate_rtt(usn_context_t *ctx, usn_tcb_t *cur_tcb, uint32_t mrtt)
{
   /* This function should be called for not retransmitted packets */
   /* TODO: determine tcp_rto_min */
#define TCP_RTO_MIN 0
   long m = mrtt;
   uint32_t tcp_rto_min = TCP_RTO_MIN;
   usn_rcvvars_t *rcvvar = cur_tcb->rcvvar;

   if (m == 0) {
      m = 1;
   }
   if (rcvvar->srtt != 0) {
      /* rtt = 7/8 rtt + 1/8 new */
      m -= (rcvvar->srtt >> 3);
      rcvvar->srtt += m;
      if (m < 0) {
         m = -m;
         m -= (rcvvar->mdev >> 2);
         if (m > 0) {
            m >>= 3;
         }
      } else {
         m -= (rcvvar->mdev >> 2);
      }
      rcvvar->mdev += m;
      if (rcvvar->mdev > rcvvar->mdev_max) {
         rcvvar->mdev_max = rcvvar->mdev;
         if (rcvvar->mdev_max > rcvvar->rttvar) {
            rcvvar->rttvar = rcvvar->mdev_max;
         }
      }
      if (TCP_SEQ_GT(cur_tcb->sndvar->snd_una, rcvvar->rtt_seq)) {
         if (rcvvar->mdev_max < rcvvar->rttvar) {
            rcvvar->rttvar -= (rcvvar->rttvar - rcvvar->mdev_max) >> 2;
         }
         rcvvar->rtt_seq = cur_tcb->snd_nxt;
         rcvvar->mdev_max = tcp_rto_min;
      }
   } else {
      /* fresh measurement */
      rcvvar->srtt = m << 3;
      rcvvar->mdev = m << 1;
      rcvvar->mdev_max = rcvvar->rttvar = MAX(rcvvar->mdev, tcp_rto_min);
      rcvvar->rtt_seq = cur_tcb->snd_nxt;
   }

   DEBUG(ctx->log,"mrtt: %u (%uus), srtt: %u (%ums), mdev: %u, mdev_max: %u, "
         "rttvar: %u, rtt_seq: %u", mrtt, mrtt * TIME_TICK,
         rcvvar->srtt, TS_TO_MSEC((rcvvar->srtt) >> 3), rcvvar->mdev,
         rcvvar->mdev_max, rcvvar->rttvar, rcvvar->rtt_seq);
}

static inline int 
usnet_process_ack(usn_context_t *ctx, usn_tcb_t *cur_tcb,
               usn_tcphdr_t *tcph, uint32_t seq, uint32_t ack_seq, 
               int16_t window, int payloadlen)
{
   usn_sndvars_t *sndvar = cur_tcb->sndvar;
   uint32_t cwindow, cwindow_prev;
   uint32_t rmlen;
   uint32_t snd_wnd_prev;
   uint32_t right_wnd_edge;
   uint8_t dup; 
   int ret; 


   cwindow = window;
   if (!(tcph->th_flags & TH_SYN)) {
      cwindow = cwindow << sndvar->wscale;
   }
   right_wnd_edge = sndvar->peer_wnd + cur_tcb->rcvvar->snd_wl2;

   DEBUG(ctx->log,"Ack update, fd=%u, ack: %u, peer_wnd: %u, snd_nxt: %u, snd_una: %u", 
         cur_tcb->fd, ack_seq, cwindow, cur_tcb->snd_nxt, sndvar->snd_una);

   /* If ack overs the sending buffer, return */
   if (cur_tcb->state == TCPS_FIN_WAIT_1 || 
         cur_tcb->state == TCPS_FIN_WAIT_2 ||
         cur_tcb->state == TCPS_CLOSING || 
         cur_tcb->state == TCPS_CLOSE_WAIT || 
         cur_tcb->state == TCPS_LAST_ACK) {
      if (sndvar->is_fin_sent && ack_seq == sndvar->fss + 1) { 
         ack_seq--;
      }    
   }
  
   if (TCP_SEQ_GT(ack_seq, sndvar->sndbuf->head_seq + sndvar->sndbuf->len)) {
      ERROR(ctx->log,"invalid acknologement, fd=%u, state=%s, "
            "ack_seq:%u, possible max_ack_seq:%u, head_seq:%u, len:%u", cur_tcb->fd, 
            usnet_tcp_statestr(cur_tcb), ack_seq, 
            sndvar->sndbuf->head_seq + sndvar->sndbuf->len, 
            sndvar->sndbuf->head_seq, sndvar->sndbuf->len);
      return -1;
   }

   /* Update window */
   if (TCP_SEQ_LT(cur_tcb->rcvvar->snd_wl1, seq) ||
         (cur_tcb->rcvvar->snd_wl1 == seq && 
         TCP_SEQ_LT(cur_tcb->rcvvar->snd_wl2, ack_seq)) ||
         (cur_tcb->rcvvar->snd_wl2 == ack_seq &&
         cwindow > sndvar->peer_wnd)) {
      cwindow_prev = sndvar->peer_wnd;
      sndvar->peer_wnd = cwindow;
      cur_tcb->rcvvar->snd_wl1 = seq;
      cur_tcb->rcvvar->snd_wl2 = ack_seq;

      DEBUG(ctx->log,"Window update, fd=%u, "
            "ack: %u, peer_wnd: %u, snd_nxt: %u, snd_una: %u", 
            cur_tcb->fd, ack_seq, cwindow, cur_tcb->snd_nxt, sndvar->snd_una);

      if (cwindow_prev < cur_tcb->snd_nxt - sndvar->snd_una &&
            sndvar->peer_wnd >= cur_tcb->snd_nxt - sndvar->snd_una) {
         DEBUG(ctx->log,"%u Broadcasting client window update! "
               "ack_seq: %u, peer_wnd: %u (before: %u), "
               "snd_nxt: %u, snd_una: %u",
               cur_tcb->fd, ack_seq, sndvar->peer_wnd, cwindow_prev,
               cur_tcb->snd_nxt, sndvar->snd_una);
         usnet_raise_write_event(ctx, cur_tcb);
      }
   }

   /* Check duplicated ack count */
   /* Duplicated ack if 
      1) ack_seq is old
      2) payload length is 0.
      3) advertised window not changed.
      4) there is outstanding unacknowledged data
      5) ack_seq == snd_una
    */

   dup = 0;
   if (TCP_SEQ_LT(ack_seq, cur_tcb->snd_nxt)) {
      if (ack_seq == cur_tcb->rcvvar->last_ack_seq && payloadlen == 0) {
         if (cur_tcb->rcvvar->snd_wl2 + sndvar->peer_wnd == right_wnd_edge) {
            if (cur_tcb->rcvvar->dup_acks + 1 > cur_tcb->rcvvar->dup_acks) {
               cur_tcb->rcvvar->dup_acks++;
            }
            dup = 1;
         }
      }
   }
   if (!dup) {
      cur_tcb->rcvvar->dup_acks = 0;
      cur_tcb->rcvvar->last_ack_seq = ack_seq;
   }

   /* Fast retransmission */
   if (dup && cur_tcb->rcvvar->dup_acks == 3) {
      DEBUG(ctx->log,"Triple duplicated ACKs!! ack_seq: %u", ack_seq);
      if (TCP_SEQ_LT(ack_seq, cur_tcb->snd_nxt)) {
         DEBUG(ctx->log,"Reducing snd_nxt from %u to %u",
               cur_tcb->snd_nxt, ack_seq);

         if (ack_seq != sndvar->snd_una) {
            DEBUG(ctx->log,"ack_seq and snd_una mismatch on tdp ack. "
                  "ack_seq: %u, snd_una: %u",
                  ack_seq, sndvar->snd_una);
         }
         cur_tcb->snd_nxt = ack_seq;
      }
      /* update congestion control variables */
      /* ssthresh to half of min of cwnd and peer wnd */
      sndvar->ssthresh = MIN(sndvar->cwnd, sndvar->peer_wnd) / 2;
      if (sndvar->ssthresh < 2 * sndvar->mss) {
         sndvar->ssthresh = 2 * sndvar->mss;
      }
      sndvar->cwnd = sndvar->ssthresh + 3 * sndvar->mss;
      DEBUG(ctx->log,"Fast retransmission. cwnd=%u, ssthresh=%u, mss=%u",
            sndvar->cwnd, sndvar->ssthresh, sndvar->mss);

      /* count number of retransmissions */
      if (sndvar->nrtx < TCP_MAX_RTX) {
         sndvar->nrtx++;
      } else {
         DEBUG(ctx->log,"Exceed MAX_RTX.");
      }

      usnet_add_send_list(ctx, cur_tcb);

   } else if (cur_tcb->rcvvar->dup_acks > 3) {
      /* Inflate congestion window until before overflow */
      if ((uint32_t)(sndvar->cwnd + sndvar->mss) > sndvar->cwnd) {
         sndvar->cwnd += sndvar->mss;
         DEBUG(ctx->log,"Dupack cwnd inflate, cwnd=%u, ssthresh=%u, mss=%u",
               sndvar->cwnd, sndvar->ssthresh, sndvar->mss);
      }
   }

#if TCPOPT_SACK_ENABLED
   usnet_parse_sack_option(ctx,cur_tcb, ack_seq, (uint8_t *)tcph + TCP_HEADER_LEN,
         (tcph->doff << 2) - TCP_HEADER_LEN);
#endif /* TCP_OPT_SACK_ENABLED */

   /* updating snd_nxt (when recovered from loss) */
   if (TCP_SEQ_GT(ack_seq, cur_tcb->snd_nxt)) {
      DEBUG(ctx->log,"Updating snd_nxt from %u to %u",
            cur_tcb->snd_nxt, ack_seq);
      cur_tcb->snd_nxt = ack_seq;
      if (sndvar->sndbuf->len == 0) {
         usnet_remove_send_list(ctx, cur_tcb);
      }
   }

   /* If ack_seq is previously acked, return */
   if (TCP_SEQ_GEQ(sndvar->sndbuf->head_seq, ack_seq)) {
      return -2;
   }

   /* Remove acked sequence from send buffer */
   rmlen = ack_seq - sndvar->sndbuf->head_seq;
   DEBUG(ctx->log,"updating una_seq:%u, ack_seq:%u, head_seq:%u, rmlen:%u, on_send_list:%d", 
         sndvar->snd_una, ack_seq, sndvar->sndbuf->head_seq, rmlen, sndvar->on_send_list);

   if (rmlen > 0) {
      /* Routine goes here only if there is new payload (not retransmitted) */
      uint16_t packets;

      /* If acks new data */
      packets = rmlen / sndvar->eff_mss;
      if ((rmlen / sndvar->eff_mss) * sndvar->eff_mss > rmlen) {
         packets++;
      }
      /* Estimate RTT and calculate rto */
      if (cur_tcb->saw_timestamp) {
         usnet_estimate_rtt(ctx, cur_tcb,
               ctx->cur_time - cur_tcb->rcvvar->ts_lastack_rcvd);
         sndvar->rto = (cur_tcb->rcvvar->srtt >> 3) + cur_tcb->rcvvar->rttvar;
         assert(sndvar->rto > 0);
      } else {
         //TODO: Need to implement timestamp estimation without timestamp
      }

      /* Update congestion control variables */
      if (cur_tcb->state >= TCPS_ESTABLISHED) {
         if (sndvar->cwnd < sndvar->ssthresh) {
            if ((sndvar->cwnd + sndvar->mss) > sndvar->cwnd) {
               sndvar->cwnd += (sndvar->mss * packets);
            }
            DEBUG(ctx->log,"slow start cwnd=%u, ssthresh=%u, mss=%u, packets=%u",
                  sndvar->cwnd, sndvar->ssthresh, sndvar->mss, packets);
         } else {
            uint32_t new_cwnd = sndvar->cwnd +
                  packets * sndvar->mss * sndvar->mss /
                  sndvar->cwnd;
            if (new_cwnd > sndvar->cwnd) {
               sndvar->cwnd = new_cwnd;
            }
            DEBUG(ctx->log,"congestion avoidance cwnd=%u, ssthresh=%u,"
                  " new_cwnd=%u, packets=%u, rmlen=%u, mss_eff=%u", 
                sndvar->cwnd, sndvar->ssthresh, new_cwnd, packets, rmlen, sndvar->eff_mss);
         }
      }

      ret = usnet_remove_data(ctx, sndvar->sndbuf, rmlen);
      sndvar->snd_una = ack_seq;
      snd_wnd_prev = sndvar->snd_wnd;
      sndvar->snd_wnd = sndvar->sndbuf->size - sndvar->sndbuf->len;

      /* If there was no available sending window */
      /* notify the newly available window to application */
      if (snd_wnd_prev <= 0 )
         usnet_raise_write_event(ctx, cur_tcb);

      usnet_update_retransmission_timer(ctx, cur_tcb);
   }

   DEBUG(ctx->log,"Ack update. "
         "ack: %u, peer_wnd: %u, snd_nxt: %u, snd_una: %u", 
         ack_seq, cwindow, cur_tcb->snd_nxt, sndvar->snd_una);
   (void)ret;

   return 0;
}

static inline int
usnet_process_tcp_payload(usn_context_t *ctx, usn_tcb_t *cur_tcb,
            uint32_t cur_ts, uint8_t *payload, uint32_t seq, int payloadlen)
{

   usn_rcvvars_t *rcvvar = cur_tcb->rcvvar;
   uint32_t prev_rcv_nxt;
   int ret = 0;

   // if seq and segment length is lower than rcv_nxt, ignore and send ack
   if (TCP_SEQ_LT(seq + payloadlen, cur_tcb->rcv_nxt)) {
      return -1;
   }
   // if payload exceeds receiving buffer, drop and send ack
   if (TCP_SEQ_GT(seq + payloadlen, cur_tcb->rcv_nxt + rcvvar->rcv_wnd)) {
      return -2;
   }

   // allocate receive buffer if not exist
   if (!rcvvar->rcvbuf) {
      rcvvar->rcvbuf = usnet_get_ringbuf(ctx, cur_tcb, rcvvar->irs + 1);
      if (!rcvvar->rcvbuf) {
         ERROR(ctx->log,"failed to allocate receive buffer,fd=%u", cur_tcb->fd);
         cur_tcb->state = TCPS_CLOSED;
         cur_tcb->close_reason = TCP_NO_MEM;
         usnet_raise_error_event(ctx, cur_tcb);

         return -3;
      }
   }

   prev_rcv_nxt = cur_tcb->rcv_nxt;

   ret = usnet_insert_fragment(ctx,
         rcvvar->rcvbuf, payload, (uint32_t)payloadlen, seq);
   if (ret < 0) {
      ERROR(ctx->log,"Cannot merge payload. reason: %d", ret);
   }

/*
   // discard the buffer if the state is FIN_WAIT_1 or FIN_WAIT_2, 
   //   meaning that the connection is already closed by the application
   if (cur_tcb->state == TCP_ST_FIN_WAIT_1 ||
         cur_tcb->state == TCP_ST_FIN_WAIT_2) {
      RBRemove(mtcp->rbm_rcv,
            rcvvar->rcvbuf, rcvvar->rcvbuf->merged_len, AT_MTCP);
   }
*/
   cur_tcb->rcv_nxt = rcvvar->rcvbuf->head_seq;// + rcvvar->rcvbuf->merged_len;
   rcvvar->rcv_wnd = rcvvar->rcvbuf->win;
   //rcvvar->rcv_wnd = rcvvar->rcvbuf->size 
   //   - (rcvvar->rcvbuf->last_seq - rcvvar->rcvbuf->head_seq) - 1;

   DEBUG(ctx->log, "rcv_nxt=%u, size=%u, head_seq=%u, last_seq=%u, rcv_wnd=%u", 
         cur_tcb->rcv_nxt, rcvvar->rcvbuf->size, rcvvar->rcvbuf->head_seq, 
         rcvvar->rcvbuf->last_seq, rcvvar->rcv_wnd );

   if (TCP_SEQ_LEQ(cur_tcb->rcv_nxt, prev_rcv_nxt)) {
      // There are some lost packets
      DEBUG(ctx->log,"rcv_nxt=%u, prev_rcv_nxt=%u", cur_tcb->rcv_nxt, prev_rcv_nxt);
      return -1;
   }
   DEBUG(ctx->log,"data arrived, fd=%u, len=%d",
                          cur_tcb->fd, payloadlen);

   if (cur_tcb->state == TCPS_ESTABLISHED) {
      usnet_raise_read_event(ctx, cur_tcb);
   }

   return 0;
}

static inline int
usnet_validate_seq(usn_context_t *ctx, usn_tcb_t *tcb, 
            usn_tcphdr_t *tcph, uint32_t seq, uint32_t 
            seq_ack, int payloadlen)
{
   /* Protect Against Wrapped Sequence number (PAWS) */
   if (!(tcph->th_flags & TH_RST) && tcb->saw_timestamp) {
      struct tcp_timestamp ts;
  
      if (usnet_parse_tcptimestamp(tcb, &ts,
            (uint8_t *)tcph + TCP_HEADER_LEN,
            (tcph->th_off << 2) - TCP_HEADER_LEN) < 0) {
         /* if there is no timestamp */
         /* TODO: implement here */
         DEBUG(ctx->log, "No timestamp found.");
         return -1;
      }

      /* RFC1323: if SEG.TSval < TS.Recent, drop and send ack */
      if (TCP_SEQ_LT(ts.ts_val, tcb->rcvvar->ts_recent)) {
         /* TODO: ts_recent should be invalidated 
                before timestamp wraparound for long idle flow */
         DEBUG(ctx->log,"PAWS Detect wrong timestamp. "
               "seq: %u, ts_val: %u, prev: %u",
               seq, ts.ts_val, tcb->rcvvar->ts_recent);
         usnet_enqueue_ack(ctx, tcb, ACK_OPT_NOW);
         return -2;
      } else {
         /* valid timestamp */
         if (TCP_SEQ_GT(ts.ts_val, tcb->rcvvar->ts_recent)) {
            DEBUG(ctx->log,"Timestamp update. cur: %u, prior: %u "
               "(time diff: %uus)", ts.ts_val, tcb->rcvvar->ts_recent,
               TS_TO_USEC(ctx->cur_time - tcb->rcvvar->ts_last_ts_upd));
            tcb->rcvvar->ts_last_ts_upd = ctx->cur_time;
         }

         tcb->rcvvar->ts_recent = ts.ts_val;
         tcb->rcvvar->ts_lastack_rcvd = ts.ts_ref;
      }
   }

   /* TCP sequence validation */
   if (!TCP_SEQ_BETWEEN(seq + payloadlen, tcb->rcv_nxt,
            tcb->rcv_nxt + tcb->rcvvar->rcv_wnd - 1)) {
      DEBUG(ctx->log,"payloadlen=%u, new_seq=%u, rcv_nxt=%u, rcv_wnd=%u", 
            payloadlen, seq+payloadlen, tcb->rcv_nxt, tcb->rcvvar->rcv_wnd);
      /* if RST bit is set, ignore the segment */
      if (tcph->th_flags & TH_RST)
         return -3;

      if (tcb->state == TCPS_ESTABLISHED) {
         /* check if it is to get window advertisement */
         if (seq + 1 == tcb->rcv_nxt) {
            DEBUG(ctx->log,"Window update request. (seq: %u, rcv_wnd: %u)", 
                  seq, tcb->rcvvar->rcv_wnd);
            usnet_enqueue_ack(ctx, tcb, ACK_OPT_AGGREGATE);
            return -4;

         }
         if (TCP_SEQ_LEQ(seq, tcb->rcv_nxt)) {
            usnet_enqueue_ack(ctx, tcb, ACK_OPT_AGGREGATE);
         } else {
            usnet_enqueue_ack(ctx, tcb, ACK_OPT_NOW);
         }
      } else {
         if (tcb->state == TCPS_TIME_WAIT) {
            DEBUG(ctx->log,"tw expire update to %u, fd=%u",
                  tcb->fd, tcb->rcvvar->ts_tw_expire);
            usnet_add_timewait_list(ctx, tcb);
         }
         usnet_add_control_list(ctx, tcb);
      }
      return -5;
   }

   return 0;
}

static inline void
usnet_update_timerlist(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
   if (cur_tcb->on_timeout_list) {
      TAILQ_REMOVE(&ctx->timeout_list, cur_tcb, sndvar->timeout_link);
      TAILQ_INSERT_TAIL(&ctx->timeout_list, cur_tcb, sndvar->timeout_link);
   } 
   return;
}

static inline int
usnet_handle_rst(usn_context_t *ctx, usn_tcb_t *cur_tcb, uint32_t ack_seq)
{
   /* TODO: we need reset validation logic */
   /* the sequence number of a RST should be inside window */
   /* (in SYN_SENT state, it should ack the previous SYN */

   DEBUG(ctx->log,"receicing rst, fd=%u, state=%s)", 
         cur_tcb->fd, usnet_tcp_statestr(cur_tcb));
   
   if (cur_tcb->state <= TCPS_SYN_SENT) {
      /* not handled here */
      return -1;
   }

   if (cur_tcb->state == TCPS_SYN_RECEIVED) {
      if (ack_seq == cur_tcb->rcv_nxt) {
         cur_tcb->state = TCPS_CLOSED;
         cur_tcb->close_reason = TCP_RESET;
         usnet_release_tcb(ctx, cur_tcb);
      }
      return 0;
   }

   /* if the application is already closed the connection, 
      just destroy the it */
   if (cur_tcb->state == TCPS_FIN_WAIT_1 ||
         cur_tcb->state == TCPS_FIN_WAIT_2 ||
         cur_tcb->state == TCPS_LAST_ACK ||
         cur_tcb->state == TCPS_CLOSING ||
         cur_tcb->state == TCPS_TIME_WAIT) {
      cur_tcb->state = TCPS_CLOSED;
      cur_tcb->close_reason = TCP_ACTIVE_CLOSE;
      usnet_release_tcb(ctx, cur_tcb);
      return 0;
   }

   if (cur_tcb->state >= TCPS_ESTABLISHED &&
         cur_tcb->state <= TCPS_CLOSE_WAIT) {
      /* ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
      /* TODO: flush all the segment queues */
      //NotifyConnectionReset(mtcp, cur_tcb);
   }

   if (!(cur_tcb->sndvar->on_closeq || cur_tcb->sndvar->on_closeq_int ||
            cur_tcb->sndvar->on_resetq || cur_tcb->sndvar->on_resetq_int)) {
      cur_tcb->state = TCPS_CLOSE_WAIT;
      cur_tcb->close_reason = TCP_RESET;
      usnet_raise_tcp_event(ctx, cur_tcb);
   }

   return 0;
}

static inline void
usnet_handle_listen(usn_context_t *ctx, usn_tcb_t *cur_tcb, 
                    usn_tcphdr_t *tcph)
{
  
   if (tcph->th_flags & TH_SYN) {
      cur_tcb->state = TCPS_SYN_RECEIVED;
      cur_tcb->rcv_nxt++;

      usnet_raise_connect_event(ctx,cur_tcb);
   } else {
      ERROR(ctx->log,"TCPS_LISTEN: packet without syn, fd=%u", 
            cur_tcb->fd);
   }
   return;
}

static inline int
usnet_handle_active_open(usn_context_t *ctx, usn_tcb_t *tcb,
               usn_tcphdr_t *tcph, uint32_t seq, uint32_t ack_seq, 
               uint16_t window)
{
   return 0;
}

static inline void
usnet_handle_syn_sent(usn_context_t *ctx, usn_tcb_t *tcb, 
      usn_iphdr_t *iph, usn_tcphdr_t *tcph, uint32_t seq, 
      uint32_t ack_seq, int payloadlen, uint16_t window)
{
   /* when active open */
   if (tcph->th_flags & TH_ACK) {
      /* filter the unacceptable acks */
      if (TCP_SEQ_LEQ(ack_seq, tcb->sndvar->iss) ||
            TCP_SEQ_GT(ack_seq, tcb->snd_nxt)) {
         if ( !(tcph->th_flags & TH_RST) ) {
            usnet_send_tcp_packet_alone(ctx,
                  iph->ip_dst, tcph->th_dport, iph->ip_src, tcph->th_sport,
                  ack_seq, 0, 0, TH_RST, NULL, 0, ctx->cur_time, 0);
         }
         //XXX: release tcb?
         return;
      }
      /* accept the ack */
      tcb->sndvar->snd_una++;
   }
  
   if (tcph->th_flags & TH_RST) {
      if (tcph->th_flags & TH_ACK) {
         tcb->state = TCPS_CLOSE_WAIT;
         tcb->close_reason = TCP_RESET;
         if (tcb->socket) {
            usnet_raise_error_event(ctx, tcb);
         } else {
            usnet_release_tcb(ctx, tcb);
         }
      }
      return;
   }

   if (tcph->th_flags & TH_SYN) {
      if (tcph->th_flags & TH_ACK) {
         int ret = usnet_handle_active_open(ctx,
               tcb, tcph, seq, ack_seq, window);
         if (!ret) {
            return;
         }
         tcb->sndvar->nrtx = 0;
         usnet_remove_rto_list(ctx, tcb);
         tcb->state = TCPS_ESTABLISHED;
         DEBUG(ctx->log, "TCPS_ESTABLISHED, fd=%u", tcb->fd);

         if (tcb->socket) {
            usnet_raise_accept_event(ctx, tcb);
         } else {
            DEBUG(ctx->log,"established connection without socket, fd=%u", tcb->fd);
            usnet_send_tcp_packet_alone(ctx,
                  iph->ip_dst, tcph->th_dport, iph->ip_src, tcph->th_sport,
                  0, seq + payloadlen + 1, 0, TH_RST | TH_ACK,
                  NULL, 0, ctx->cur_time, 0);
            tcb->close_reason = TCP_ACTIVE_CLOSE;
            usnet_release_tcb(ctx, tcb);
         }
         usnet_add_control_list(ctx, tcb);
         if (ctx->tcp_timeout > 0)
            usnet_add_timeout_list(ctx, tcb);

      } else {
         tcb->state = TCPS_SYN_RECEIVED;
         DEBUG(ctx->log,"TCPS_SYN_RECEIVED, fd=%u", tcb->fd);
         tcb->snd_nxt = tcb->sndvar->iss;
         usnet_add_control_list(ctx, tcb);
      }
   }
}

static inline void
usnet_handle_syn_received(usn_context_t *ctx, usn_tcb_t *cur_tcb,
     usn_tcphdr_t *tcph, uint32_t ack_seq)
{
   usn_sndvars_t *sndvar = cur_tcb->sndvar;

   if (tcph->th_flags & TH_ACK) {
      uint32_t prior_cwnd;
      /* check if ACK of SYN */
      if (ack_seq != sndvar->iss + 1) {
         ERROR(ctx->log, "unexpected ack, fd=%u, ack_seq: %u, iss: %un",
               cur_tcb->fd, ack_seq, sndvar->iss);
         return;
      }

      sndvar->snd_una++;
      cur_tcb->snd_nxt = ack_seq;
      prior_cwnd = sndvar->cwnd;
      sndvar->cwnd = ((prior_cwnd == 1)?
            (sndvar->mss * 2): sndvar->mss);

      sndvar->nrtx = 0;
      usnet_remove_rto_list(ctx, cur_tcb);

      cur_tcb->state = TCPS_ESTABLISHED;
      DEBUG(ctx->log,"TCPS_ESTABLISHED, fd=%u, cwnd=%u, mss=%u", 
            cur_tcb->fd, sndvar->cwnd, sndvar->mss);
       
      if (ctx->tcp_timeout > 0)
         usnet_add_timeout_list(ctx, cur_tcb);

   } else {
      DEBUG(ctx->log,"no ack, fd=%u, state=%s", 
            cur_tcb->fd, usnet_tcp_statestr(cur_tcb));
      /* retransmit SYN/ACK */
      cur_tcb->snd_nxt = sndvar->iss;
      usnet_add_control_list(ctx, cur_tcb);
   }
}

static inline void
usnet_handle_established(usn_context_t *ctx, usn_tcb_t *cur_tcb, 
      usn_tcphdr_t *tcph, uint32_t seq, uint32_t ack_seq, 
      uint8_t *payload, int payloadlen, uint16_t window)
{
   int ret = 0; 

   if (tcph->th_flags & TH_SYN) {
      DEBUG(ctx->log,"unexpected SYN, fd=%u,"
            "seq: %u, expected: %u, ack_seq: %u, expected: %u",
            cur_tcb->fd, seq, cur_tcb->rcv_nxt,
            ack_seq, cur_tcb->snd_nxt);
      cur_tcb->snd_nxt = ack_seq;
      usnet_add_control_list(ctx, cur_tcb);
      return;
   }

   if (payloadlen > 0) {
      ret = usnet_process_tcp_payload(ctx, cur_tcb,
            ctx->cur_time, payload, seq, payloadlen);
      if ( !ret ) {
         DEBUG(ctx->log, "send ack, fd=%u, ret=%d",cur_tcb->fd, ret);
         usnet_enqueue_ack(ctx, cur_tcb, ACK_OPT_AGGREGATE);
      } else {
         DEBUG(ctx->log, "send ack, fd=%u, ret=%d",cur_tcb->fd, ret);
         usnet_enqueue_ack(ctx, cur_tcb, ACK_OPT_NOW);
      }
   }

   if (tcph->th_flags & TH_ACK) {
      if (cur_tcb->sndvar->sndbuf) {
         usnet_process_ack(ctx, cur_tcb,
               tcph, seq, ack_seq, window, payloadlen);
      }
   }

   if (tcph->th_flags & TH_FIN) {
      /* process the FIN only if the sequence is valid */
      if (seq == cur_tcb->rcv_nxt) {
         cur_tcb->state = TCPS_CLOSE_WAIT;
         DEBUG(ctx->log,"fd=%u, state=%s, rcv_nxt=%u",
               cur_tcb->fd, usnet_tcp_statestr(cur_tcb), cur_tcb->rcv_nxt);
         cur_tcb->rcv_nxt++;
         usnet_add_control_list(ctx, cur_tcb);

         /* notify FIN to application */
         usnet_raise_tcp_event(ctx, cur_tcb);
      } else {
         usnet_enqueue_ack(ctx, cur_tcb, ACK_OPT_NOW);
      }
   }

}

static inline void
usnet_handle_close_wait(usn_context_t *ctx, usn_tcb_t *cur_tcb,
      usn_tcphdr_t *tcph, uint32_t seq, uint32_t ack_seq,
      int payloadlen, uint16_t window)
{
   if (TCP_SEQ_LT(seq, cur_tcb->rcv_nxt)) {
      DEBUG(ctx->log, "unexpected seq, fd=%u, seq: %u, expected: %u, state=%s",
            cur_tcb->fd, seq, cur_tcb->rcv_nxt, usnet_tcp_statestr(cur_tcb));
      usnet_add_control_list(ctx, cur_tcb);
      return;
   }

   if (cur_tcb->sndvar->sndbuf) {
      usnet_process_ack(ctx, cur_tcb,
            tcph, seq, ack_seq, window, payloadlen);
   }

   return;
}

static inline void
usnet_handle_fin_wait1(usn_context_t *ctx, usn_tcb_t *cur_tcb,
      usn_tcphdr_t *tcph, uint32_t seq, uint32_t ack_seq, 
      uint8_t *payload, int payloadlen, uint16_t window)
{
   if (TCP_SEQ_LT(seq, cur_tcb->rcv_nxt)) {
      DEBUG(ctx->log, "unexpected seq, fd=%u, state=%s, seq: %u, expected: %u",
            cur_tcb->fd, usnet_tcp_statestr(cur_tcb), seq, cur_tcb->rcv_nxt);
      usnet_add_control_list(ctx, cur_tcb);
      return;
   }

   if (tcph->th_flags & TH_ACK) {
      if (cur_tcb->sndvar->sndbuf) {
         usnet_process_ack(ctx, cur_tcb,
               tcph, seq, ack_seq, window, payloadlen);
      }

      if (cur_tcb->sndvar->is_fin_sent &&
            ack_seq == cur_tcb->sndvar->fss + 1) {
         cur_tcb->sndvar->snd_una = ack_seq;
         if (TCP_SEQ_GT(ack_seq, cur_tcb->snd_nxt)) {
            DEBUG(ctx->log,"update snd_nxt, fd=%u, snd_nxt=%u",
                  cur_tcb->fd, ack_seq);
            cur_tcb->snd_nxt = ack_seq;
         }
         cur_tcb->sndvar->nrtx = 0;
         usnet_remove_rto_list(ctx, cur_tcb);
         cur_tcb->state = TCPS_FIN_WAIT_2;
         DEBUG(ctx->log,"TCPS_FIN_WAIT_2, fd=%u", cur_tcb->fd);
      }
   } else {
      DEBUG(ctx->log,"no ack, fd=%u", cur_tcb->fd);
      return;
   }

   if (payloadlen > 0) {
      if (usnet_process_tcp_payload(ctx, cur_tcb,
            ctx->cur_time, payload, seq, payloadlen)) {
         /* if return is TRUE, send ACK */
         usnet_enqueue_ack(ctx, cur_tcb, ACK_OPT_AGGREGATE);
      } else {
         usnet_enqueue_ack(ctx, cur_tcb, ACK_OPT_NOW);
      }
   }

   if (tcph->th_flags & TH_FIN) {
      /* process the FIN only if the sequence is valid */
      if (seq == cur_tcb->rcv_nxt) {
         cur_tcb->rcv_nxt++;

         if (cur_tcb->state == TCPS_FIN_WAIT_1) {
            cur_tcb->state = TCPS_CLOSING;
            DEBUG(ctx->log,"TCPS_CLOSING, fd=%u", cur_tcb->fd);

         } else if (cur_tcb->state == TCPS_FIN_WAIT_2) {
            cur_tcb->state = TCPS_TIME_WAIT;
            DEBUG(ctx->log,"TCPS_TIME_WAIT, fd=%u", cur_tcb->fd);
            usnet_add_timewait_list(ctx, cur_tcb);
         }
         usnet_add_control_list(ctx, cur_tcb);
      }
   }
}

static inline void
usnet_handle_closing(usn_context_t *ctx, usn_tcb_t *cur_tcb,
      usn_tcphdr_t *tcph, uint32_t seq, uint32_t ack_seq,
      int payloadlen, uint16_t window)
{
   if (tcph->th_flags & TH_ACK) {
      if (cur_tcb->sndvar->sndbuf) {
         usnet_process_ack(ctx, cur_tcb,
               tcph, seq, ack_seq, window, payloadlen);
      }

      if (!cur_tcb->sndvar->is_fin_sent) {
         DEBUG(ctx->log, "no fin sent yet, fd=%u, state=%s", 
               cur_tcb->fd, usnet_tcp_statestr(cur_tcb));
         return;
      }

      // check if ACK of FIN
      if (ack_seq != cur_tcb->sndvar->fss + 1) {
         /* if the packet is not the ACK of FIN, ignore */
         ERROR(ctx->log,"not ack of fin, fd=%u, "
               "ack_seq=%u, snd_nxt=%u, snd_una=%u, fss=%u", 
               cur_tcb->fd, ack_seq, cur_tcb->snd_nxt, 
               cur_tcb->sndvar->snd_una, cur_tcb->sndvar->fss);
         usnet_dump_tcb(ctx, cur_tcb);
         return;
      }

      cur_tcb->sndvar->snd_una = ack_seq;
      cur_tcb->snd_nxt = ack_seq;
      usnet_update_retransmission_timer(ctx, cur_tcb);

      cur_tcb->state = TCPS_TIME_WAIT;
      DEBUG(ctx->log,"TCPS_TIME_WAIT, fd=%u", cur_tcb->fd);

      usnet_add_timewait_list(ctx, cur_tcb);

   } else {
      ERROR(ctx->log,"not ack, fd=%u, state=%s",
            cur_tcb->fd,usnet_tcp_statestr(cur_tcb));
      return;
   }
}

static inline void
usnet_handle_last_ack(usn_context_t *ctx, usn_iphdr_t *iph,
      int iplen, usn_tcb_t *cur_tcb, usn_tcphdr_t *tcph, 
      uint32_t seq, uint32_t ack_seq, int payloadlen, 
      uint16_t window)
{
   if (TCP_SEQ_LT(seq, cur_tcb->rcv_nxt)) {
      DEBUG(ctx->log, "unexpected seq, fd=%u, state=%s, seq: %u, expected: %u",
            cur_tcb->fd, usnet_tcp_statestr(cur_tcb), seq, cur_tcb->rcv_nxt);
      return;
   }

   if (tcph->th_flags & TH_ACK) {
      if (cur_tcb->sndvar->sndbuf) {
         usnet_process_ack(ctx, cur_tcb,
               tcph, seq, ack_seq, window, payloadlen);
      }

      if (!cur_tcb->sndvar->is_fin_sent) {
         /* the case that FIN is not sent yet */
         /* this is not ack for FIN, ignore */
         DEBUG(ctx->log, "no fin sent yet, fd=%u, state=%s", 
               cur_tcb->fd, usnet_tcp_statestr(cur_tcb));
#if DUMP_STREAM
         usnet_dump_tcb(ctx, cur_tcb);
#endif
         return;
      }
      /* check if ACK of FIN */
      if (ack_seq == cur_tcb->sndvar->fss + 1) {
         cur_tcb->sndvar->snd_una++;
         usnet_update_retransmission_timer(ctx, cur_tcb);
         cur_tcb->state = TCPS_CLOSED;
         cur_tcb->close_reason = TCP_PASSIVE_CLOSE;
         DEBUG(ctx->log,"TCPS_CLOSED, fd=%u", cur_tcb->fd);
         usnet_release_tcb(ctx, cur_tcb);
      } else {
         DEBUG(ctx->log,"not ack of fin, fd=%u, state=%s, " "ack_seq: %u, expected: %u",
               cur_tcb->fd, usnet_tcp_statestr(cur_tcb), ack_seq, cur_tcb->sndvar->fss + 1);
         usnet_add_control_list(ctx, cur_tcb);
      }
   } else {
      ERROR(ctx->log,"no ack, fd=%u, state=%s",
            cur_tcb->fd, usnet_tcp_statestr(cur_tcb));
      usnet_add_control_list(ctx, cur_tcb);
   }
}

static inline void
usnet_handle_fin_wait2(usn_context_t *ctx, usn_tcb_t *cur_tcb,
      usn_tcphdr_t *tcph, uint32_t seq, uint32_t ack_seq, 
      uint8_t *payload, int payloadlen, uint16_t window)
{
   if (tcph->th_flags & TH_ACK) {
      if (cur_tcb->sndvar->sndbuf) {
         usnet_process_ack(ctx, cur_tcb,
               tcph, seq, ack_seq, window, payloadlen);
      }
   } else {
      DEBUG(ctx->log,"no ack, fd=%u, state=%s",
            cur_tcb->fd, usnet_tcp_statestr(cur_tcb));
      return;
   }

   if (payloadlen > 0) {
      if (usnet_process_tcp_payload(ctx, cur_tcb,
            ctx->cur_time, payload, seq, payloadlen)) {
         /* if return is TRUE, send ACK */
         usnet_enqueue_ack(ctx, cur_tcb, ACK_OPT_AGGREGATE);
      } else {
         usnet_enqueue_ack(ctx, cur_tcb, ACK_OPT_NOW);
      }
   }

   if (tcph->th_flags & TH_FIN) {
      /* process the FIN only if the sequence is valid */
      if (seq == cur_tcb->rcv_nxt) {
         cur_tcb->state = TCPS_TIME_WAIT;
         cur_tcb->rcv_nxt++;
         DEBUG(ctx->log,"TCPS_TIME_WAIT, fd=%u", cur_tcb->fd);

         usnet_add_timewait_list(ctx, cur_tcb);
         usnet_add_control_list(ctx, cur_tcb);
      }
   }
}

static inline void
usnet_handle_time_wait(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
   /* the only thing that can arrive in this state is a retransmission 
      of the remote FIN. Acknowledge it, and restart the 2 MSL timeout */
   if (cur_tcb->on_timewait_list) {
      usnet_remove_timewait_list(ctx, cur_tcb);
      usnet_add_timewait_list(ctx, cur_tcb);
   }
   usnet_add_control_list(ctx, cur_tcb);
}

static inline int
usnet_handle_syn_pkt(usn_context_t *ctx, uint32_t ip, uint16_t port)
{
   // TODO: support more than one listening socket.
   if ( ctx->listen_tcb == 0 )
      return 0;

   DEBUG(ctx->log, "listen socket: fd=%u, %x:%u", 
       ctx->listen_tcb->fd, ctx->listen_tcb->saddr, 
       ctx->listen_tcb->sport);

   return  ( (ip == ctx->listen_tcb->saddr) && (port == ctx->listen_tcb->sport) );
}

usn_tcb_t*
usnet_handle_passive_open(usn_context_t *ctx, usn_iphdr_t *iph, uint32_t iplen, 
                 usn_tcphdr_t *tcph, uint32_t seq, uint32_t ack_seq, 
                 int payloadlen, uint16_t window)
{
   usn_tcb_t *cur_tcb = 0;
   int ret;

   if ( (tcph->th_flags & (TH_SYN|TH_ACK)) == TH_SYN ) {
      ret = usnet_handle_syn_pkt(ctx, iph->ip_dst, tcph->th_dport);
      if ( !ret ) {
         // send RST|ACK
         DEBUG(ctx->log, "send rst|ack, payloadlen=%d, ret=%d", payloadlen, ret); 
         usnet_send_tcp_packet_alone(ctx, iph->ip_dst, tcph->th_dport, 
               iph->ip_src, tcph->th_sport, 0, seq + payloadlen +1, 0,
               TH_RST|TH_ACK, NULL, 0, ctx->cur_time, 0);
         return 0;
      }
      // accept new connection.
      cur_tcb = usnet_create_tcb(ctx,iph,tcph,seq,window);

      if ( cur_tcb == 0 ) {
         // send RST|ACK
         DEBUG(ctx->log, "send rst|ack, payloadlen=%d", payloadlen); 
         usnet_send_tcp_packet_alone(ctx, iph->ip_dst, tcph->th_dport, 
               iph->ip_src, tcph->th_sport, 0, seq + payloadlen +1, 0,
               TH_RST|TH_ACK, NULL, 0, ctx->cur_time, 0);
         return 0;
      }

      return cur_tcb;

   } else if ( tcph->th_flags & TH_RST ) {
      // just discard.
   } else {
      // weird pkt.
      if ( tcph->th_flags & TH_ACK ) {
         // send RST.
         DEBUG(ctx->log, "send rst|ack, payloadlen=%d", payloadlen); 
         usnet_send_tcp_packet_alone(ctx, iph->ip_dst, tcph->th_dport, 
               iph->ip_src, tcph->th_sport, ack_seq, 0, 0,
               TH_RST, NULL, 0, ctx->cur_time, 0);
      } else {
         // send RST|ACK.
         DEBUG(ctx->log, "send rst|ack, payloadlen=%d", payloadlen); 
         usnet_send_tcp_packet_alone(ctx, iph->ip_dst, tcph->th_dport, 
               iph->ip_src, tcph->th_sport, 0, seq + payloadlen, 0,
               TH_RST|TH_ACK, NULL, 0, ctx->cur_time, 0);
      }
   }

   return 0;
}

int
usnet_tcp_input(usn_context_t *ctx, usn_iphdr_t *iph, int iplen)
{
   usn_tcphdr_t *tcph = (usn_tcphdr_t *)((char*)iph + (iph->ip_hl << 2));
   uint8_t *payload   = (uint8_t *)tcph + (tcph->th_off << 2);
   //int tcplen         = iplen - (iph->ip_hl << 2);
   int payloadlen     = iplen - (payload - (uint8_t*)iph);
   uint32_t seq = ntohl(tcph->th_seq);
   uint32_t ack_seq = ntohl(tcph->th_ack);
   uint16_t window = ntohs(tcph->th_win);
   usn_tcb_t *cur_tcb;
   usn_tcb_t tcb;
   int ret = 0;

	DEBUG(ctx->log, "tcp packet, iplen=%d, payloadlen=%d, seq=%u, tcp_off=%x(%x), flags=0x%x", 
         iplen, payloadlen, seq, tcph->th_off << 2, tcph->th_off, tcph->th_flags);

   /* verify tcp packet */
   if ( iplen < ((iph->ip_hl + tcph->th_off) << 2) )
      return -1;

   /* checksum */
   if ( tcp_cksum((uint16_t*)tcph, (tcph->th_off << 2) + payloadlen, 
            iph->ip_src, iph->ip_dst) ) {
      ERROR(ctx->log, "tcp checksum failed");
      return -2;
   }

   /* find tcb */
   tcb.saddr = iph->ip_dst;
   tcb.sport = tcph->th_dport;
   tcb.daddr = iph->ip_src;
   tcb.dport = tcph->th_sport;

   cur_tcb = CACHE_SEARCH(ctx->tcb_cache, &tcb, usn_tcb_t*);

   if ( cur_tcb == 0 ) {
      cur_tcb = usnet_handle_passive_open(ctx, iph, iplen, 
            tcph, seq, ack_seq, payloadlen, window);

      if ( cur_tcb == 0 )
         return 0;
   }

   DEBUG(ctx->log, "found tcb, fd=%u, state=%s, listen_tcb=%p, tcb=%p", 
         cur_tcb->fd, usnet_tcp_statestr(cur_tcb),ctx->listen_tcb,cur_tcb);

   if ( cur_tcb->state >= TCPS_SYN_RECEIVED ) {
      if ( (ret = usnet_validate_seq(ctx, cur_tcb, 
               tcph, seq, ack_seq, payloadlen)) < 0 ) {
         ERROR(ctx->log,"wrong sequence, fd=%u, ret=%d, seq=%u, payloadlen=%u,"
                        " new_seq=%u, rcv_nxt=%u, rcv_wnd=%u", cur_tcb->fd, ret, 
                        seq,payloadlen,seq+payloadlen,cur_tcb->rcv_nxt,cur_tcb->rcvvar->rcv_wnd);
         return -3;
      }
   }

   /* update window size */
   if ( tcph->th_flags & TH_SYN ) {
      cur_tcb->sndvar->peer_wnd = window;
   } else {
      cur_tcb->sndvar->peer_wnd = 
         (uint32_t)window << cur_tcb->sndvar->wscale;
   }

   cur_tcb->last_active_ts = ctx->cur_time;
   usnet_update_timerlist(ctx, cur_tcb);

   /* process RST flag */
   if ( tcph->th_flags & TH_RST ) {
      cur_tcb->have_reset = 1;
      if (cur_tcb->state > TCPS_SYN_SENT) {
         if (usnet_handle_rst(ctx, cur_tcb, ack_seq)) {
            return 0;
         }    
      }   
   }
   
   DEBUG(ctx->log, "handle tcp packet, state=%s", usnet_tcp_statestr(cur_tcb));
   switch(cur_tcb->state) {
      case TCPS_LISTEN:
         usnet_handle_listen(ctx, cur_tcb, tcph);
         break;
      case TCPS_SYN_SENT:
         usnet_handle_syn_sent(ctx, cur_tcb, iph, tcph,
               seq, ack_seq, payloadlen, window);
         break;
      case TCPS_SYN_RECEIVED:
         usnet_handle_syn_received(ctx, cur_tcb, tcph, ack_seq);
         break;
      case TCPS_ESTABLISHED:
         usnet_handle_established(ctx, cur_tcb, tcph, seq, 
               ack_seq, payload, payloadlen, window);
         break;
      case TCPS_CLOSE_WAIT:
         usnet_handle_close_wait(ctx, cur_tcb, tcph, seq, ack_seq,
                                 payloadlen, window);
         break;
      case TCPS_FIN_WAIT_1:
         usnet_handle_fin_wait1(ctx, cur_tcb, tcph, seq, ack_seq, 
               payload, payloadlen, window);
         break;
      case TCPS_CLOSING:
         usnet_handle_closing(ctx, cur_tcb, tcph, seq, ack_seq,
                                 payloadlen, window);
         break;
      case TCPS_LAST_ACK:
         usnet_handle_last_ack(ctx, iph, iplen, cur_tcb, tcph, 
               seq, ack_seq, payloadlen, window);
         break;
      case TCPS_FIN_WAIT_2:
         usnet_handle_fin_wait2(ctx, cur_tcb, tcph, seq, ack_seq, 
               payload, payloadlen, window);
         break;
      case TCPS_TIME_WAIT:
         usnet_handle_time_wait(ctx, cur_tcb);
         break;
      case TCPS_CLOSED:
         break;
      default:
         break;
   }

   return 0;
}


