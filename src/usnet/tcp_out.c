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
 * @(#)tcp_out.h
 */

#include <netinet/in.h>
#include <assert.h>

#include "tcp_out.h"
#include "tcp.h"
#include "utils.h"
#include "eth.h"
#include "log.h"
#include "core.h"
#include "timer.h"
#include "sndbuf.h"

static inline uint16_t
usnet_get_tcpopt_length(uint8_t flags)
{
   uint16_t optlen = 0;

   if ( flags & TH_SYN ) {
      optlen += TCPOLEN_MAXSEG;
#ifdef TCPOPT_SACK_ENABLED
      optlen += TCPOLEN_SACK_PERMITTED;
#ifndef TCPOPT_TIMESTAMP_ENABLED
      optlen += 2;
#endif // TCPOPT_TIMESTAMP_ENBALED
#endif // TCPOPT_SACK_ENABLED

#ifdef TCPOPT_TIMESTAMP_ENABLED
      optlen += TCPOLEN_TIMESTAMP;
#ifndef TCPOPT_SACK_ENABLED
      optlen += 2;
#endif // TCPOPT_SACK_ENABLED
#endif // TCPOPT_TIMESTAMP_ENBALED

      optlen += TCPOLEN_WINDOW + 1;

   } else {

#ifdef TCPOPT_TIMESTAMP_ENABLED
      optlen += TCPOLEN_TIMESTAMP + 2;
#endif // TCPOPT_TIMESTAMP_ENBALED

#ifndef TCPOPT_SACK_ENABLED
      if ( flags & TH_SACK ) {
         optlen += TCPOPTLEN_SACK + 2;
      }
#endif // TCPOPT_SACK_ENABLED

   }
   return optlen;
}

static inline void
usnet_generate_tcpts(usn_tcb_t *tcb, uint8_t *tcpopt, uint32_t cur_ts)
{
   uint32_t *ts = (uint32_t *)(tcpopt + 2);

   tcpopt[0] = TCPOPT_TIMESTAMP;
   tcpopt[1] = TCPOLEN_TIMESTAMP;
   ts[0] = htonl(cur_ts);
   ts[1] = htonl(tcb->rcvvar->ts_recent);
}


static inline void
usnet_generate_tcpopts(usn_context_t *ctx, usn_tcb_t *tcb,
           uint8_t flags, uint8_t *tcpopt, uint16_t optlen)
{
   int i = 0;

   if (flags & TH_SYN) {
      uint16_t mss;

      /* MSS option */
      mss = tcb->sndvar->mss;
      tcpopt[i++] = TCPOPT_MAXSEG;
      tcpopt[i++] = TCPOLEN_MAXSEG;
      tcpopt[i++] = mss >> 8;
      tcpopt[i++] = mss % 256;

      /* SACK permit */
#if TCPOPT_SACK_ENABLED
#if !TCPOPT_TIMESTAMP_ENABLED
      tcpopt[i++] = TCPOPT_NOP;
      tcpopt[i++] = TCPOPT_NOP;
#endif /* TCP_OPT_TIMESTAMP_ENABLED */
      tcpopt[i++] = TCPOPT_SACK_PERMITTED;
      tcpopt[i++] = TCPOLEN_SACK_PERMITTED;
      TRACE_SACK("Local SACK permited.\n");
#endif /* TCP_OPT_SACK_ENABLED */

      /* Timestamp */
#if TCPOPT_TIMESTAMP_ENABLED
#if !TCPOPT_SACK_ENABLED
      tcpopt[i++] = TCPOPT_NOP;
      tcpopt[i++] = TCPOPT_NOP;
#endif /* TCP_OPT_SACK_ENABLED */
      usnet_generate_tcpts(tcb, tcpopt + i, ctx->cur_time);
      i += TCPOLEN_TIMESTAMP;
#endif /* TCP_OPT_TIMESTAMP_ENABLED */

      /* Window scale */
      tcpopt[i++] = TCPOPT_NOP;
      tcpopt[i++] = TCPOPT_WINDOW;
      tcpopt[i++] = TCPOLEN_WINDOW;
      tcpopt[i++] = tcb->sndvar->wscale;

   } else {

#if TCPOPT_TIMESTAMP_ENABLED
      tcpopt[i++] = TCPOPT_NOP;
      tcpopt[i++] = TCPOPT_NOP;
      usnet_generate_tcpts(tcb, tcpopt + i, ctx->cur_time);
      i += TCPOLEN_TIMESTAMP;
#endif

#if TCPOPT_SACK_ENABLED
      if (flags & TCPOPT_SACK) {
         // TODO: implement SACK support
      }
#endif
   }

   assert (i == optlen);
}

int   
usnet_send_tcp_packet_alone(usn_context_t *ctx, uint32_t saddr, uint16_t sport, 
               uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack_seq,
               uint16_t window, uint8_t flags, uint8_t *payload, int payloadlen,
               uint32_t cur_ts, uint32_t echo_ts)
{
   usn_tcphdr_t *tcph;
   uint8_t      *tcpopt;
   uint32_t     *ts;
   uint16_t      optlen;

   optlen = usnet_get_tcpopt_length(flags);

   if ( payloadlen > TCP_DEFAULT_MSS + optlen ) {
      WARN(ctx->log, "pkt size exceeds MSS, len=%d", payload);
      return -1;
   }
  
   tcph = (usn_tcphdr_t*)usnet_ipv4_output_alone(ctx, 0, IPPROTO_TCP, 
                   saddr, daddr, TCP_HEADER_LEN + optlen + payloadlen);

   if ( tcph == NULL ) {
      return -2;
   }

   DEBUG(ctx->log, "payloadlen=%d, seq=%x, ack_seq=%x, optlen=%d", 
         payloadlen, seq, ack_seq, optlen);

   memset(tcph, 0, TCP_HEADER_LEN + optlen);

   tcph->th_sport = sport;
   tcph->th_dport = dport;

   tcph->th_seq = htonl(seq);

   tcph->th_flags = flags;
   if (flags & TH_ACK) {
      tcph->th_ack = htonl(ack_seq);
   }   

   tcph->th_win = htons(MIN(window, TCP_MAXWIN));

   tcpopt = (uint8_t *)tcph + TCP_HEADER_LEN;
   ts = (uint32_t *)(tcpopt + 4); 

   tcpopt[0] = TCPOPT_NOP;
   tcpopt[1] = TCPOPT_NOP;
   tcpopt[2] = TCPOPT_TIMESTAMP;
   tcpopt[3] = TCPOLEN_TIMESTAMP;
   ts[0] = htonl(cur_ts);
   ts[1] = htonl(echo_ts);

   tcph->th_off = (TCP_HEADER_LEN + optlen) >> 2;
   // copy payload if exist
   if (payloadlen > 0) {
      memcpy((uint8_t *)tcph + TCP_HEADER_LEN + optlen, payload, payloadlen);
   }   

   tcph->th_sum = tcp_cksum((uint16_t *)tcph, 
         TCP_HEADER_LEN + optlen + payloadlen, saddr, daddr);

   if (tcph->th_flags & (TH_SYN|TH_FIN)) {
      payloadlen++;
   }

   usnet_send_data(ctx);

   return payloadlen;
}

int
usnet_tcp_output(usn_context_t *ctx, unsigned char* buf, int len)
{
   return 0;
}

int
usnet_send_tcp_packet(usn_context_t *ctx, usn_tcb_t *tcb, uint8_t flags, 
      uint8_t *payload, int payloadlen)
{

   usn_tcphdr_t *tcph;
   uint16_t optlen;
   uint8_t wscale = 0;
   uint32_t window32 = 0;

   DEBUG(ctx->log,"send tcp packet, fd=%u, seq=%u, ack=%u, payloadlen=%u",
            tcb->fd, tcb->snd_nxt-1,tcb->rcv_nxt,payloadlen);

   optlen = usnet_get_tcpopt_length(flags);
   if (payloadlen > tcb->sndvar->mss + optlen) {
      ERROR(ctx->log,"Payload size exceeds MSS, payloadlen=%u, mss=%u", 
            payloadlen, tcb->sndvar->mss + optlen);
      return -1;
   }

   //tcph = (usn_tcphdr_t*)usnet_ipv4_output(ctx, 0, IPPROTO_TCP, saddr, daddr, 
   //                       TCP_HEADER_LEN + optlen + payloadlen);

   tcph = (usn_tcphdr_t *)usnet_ipv4_output(ctx, tcb,
         TCP_HEADER_LEN + optlen + payloadlen);

   if (tcph == NULL) {
      return -2;
   }
   memset(tcph, 0, TCP_HEADER_LEN + optlen);

   tcph->th_sport = tcb->sport;
   tcph->th_dport = tcb->dport;

   tcph->th_flags = flags;
   if (flags & TH_SYN) {
      //tcph->th_flags |= TH_SYN;
      if (tcb->snd_nxt != tcb->sndvar->iss) {
         DEBUG(ctx->log,"enexpected syn, fd=%u, "
               "snd_nxt: %u, iss: %u", tcb->fd,
               tcb->snd_nxt, tcb->sndvar->iss);
      }
      DEBUG(ctx->log,"sending syn, fd=%u, seq: %u, ack_seq: %u", 
            tcb->fd, tcb->snd_nxt, tcb->rcv_nxt);
   }
   if (flags & TH_RST) {
      DEBUG(ctx->log,"sending rst, fd=%u", tcb->fd);
   }
   if (flags & TH_PUSH) {
      //tcph->th_flags = TH_PUSH;
   }

   if (flags & TH_WACK) {
      tcph->th_seq = htonl(tcb->snd_nxt - 1);
      DEBUG(ctx->log,"sending ack to get new window advertisement. "
            "fd=%u, seq=%u, peer_wnd=%u, (snd_nxt - snd_una)=%u",
            tcb->fd,
            tcb->snd_nxt - 1, tcb->sndvar->peer_wnd,
            tcb->snd_nxt - tcb->sndvar->snd_una);
   } else if (flags & TH_FIN) {

      if (tcb->sndvar->fss == 0) {
         ERROR(ctx->log,"not fss set, fd=%u, closed: %u",
               tcb->fd, tcb->closed);
      }
      tcph->th_seq = htonl(tcb->sndvar->fss);
      tcb->sndvar->is_fin_sent = 1;
      DEBUG(ctx->log,"sending fin, fd=%u, seq=%u, ack_seq=%u",
            tcb->fd, tcb->snd_nxt, tcb->rcv_nxt);
   } else {
      tcph->th_seq = htonl(tcb->snd_nxt);
   }
   if (flags & TH_ACK) {
      tcph->th_ack = htonl(tcb->rcv_nxt);
      tcb->sndvar->ts_lastack_sent = ctx->cur_time;
      tcb->last_active_ts = ctx->cur_time;
      usnet_update_timeout_list(ctx, tcb);
   }

   if (flags & TH_SYN) {
      wscale = 0;
   } else {
      wscale = tcb->sndvar->wscale;
   }

   window32 = tcb->rcvvar->rcv_wnd >> wscale;
   tcph->th_win = htons(MIN((uint16_t)window32, TCP_MAX_WINDOW));
   // if the advertised window is 0, we need to advertise again later.
   if (window32 == 0) {
      tcb->need_wnd_adv = 1;
   }

   usnet_generate_tcpopts(ctx, tcb, flags,
         (uint8_t *)tcph + TCP_HEADER_LEN, optlen);

   tcph->th_off = (TCP_HEADER_LEN + optlen) >> 2;
   // copy payload if exist
   if (payloadlen > 0) {
      memcpy((uint8_t *)tcph + TCP_HEADER_LEN + optlen, payload, payloadlen);
   } 

   tcph->th_sum = tcp_cksum((uint16_t *)tcph,
         TCP_HEADER_LEN + optlen + payloadlen,
         tcb->saddr, tcb->daddr);

   tcb->snd_nxt += payloadlen;

   if ( (tcph->th_flags & (TH_SYN|TH_FIN)) ) {
      tcb->snd_nxt++;
      payloadlen++;
   }

   if (payloadlen > 0) {
      if (tcb->state > TCPS_ESTABLISHED) {
         DEBUG(ctx->log,"payload info: length=%d, snd_nxt=%u",
               payloadlen, tcb->snd_nxt);
      }

      // update retransmission timer if have payload 
      tcb->sndvar->ts_rto = ctx->cur_time + tcb->sndvar->rto;
      DEBUG(ctx->log,"updating retransmission timer, fd=%u, "
            "cur_ts: %u, rto: %u, ts_rto: %u\n", tcb->fd,
            ctx->cur_time, tcb->sndvar->rto, tcb->sndvar->ts_rto);
      usnet_add_rto_list(ctx, tcb);
   }

   usnet_send_data(ctx);

   return payloadlen;
}

int
usnet_send_control_packet(usn_context_t *ctx, usn_tcb_t *tcb)
{
   usn_sndvars_t *sndvar = tcb->sndvar;
   int ret = 0;
   int sent_ack = 0;

   DEBUG(ctx->log,"send control packet, fd=%u", tcb->fd);

   //ERROR(ctx->log,"send control packet, fd=%u, state=%s, seq=%u, ack=%u",
   //      tcb->fd, tcb->snd_nxt-1,tcb->rcv_nxt,usnet_tcp_statestr(tcb));
   if (tcb->state == TCPS_SYN_SENT) {
      /* Send SYN here */
      ret = usnet_send_tcp_packet(ctx, tcb, TH_SYN, NULL, 0);

   } else if (tcb->state == TCPS_SYN_RECEIVED) {
      /* Send SYN/ACK here */
      tcb->snd_nxt = sndvar->iss;
      ret = usnet_send_tcp_packet(ctx, tcb,
            TH_SYN | TH_ACK, NULL, 0);
      sent_ack = 1;

   } else if (tcb->state == TCPS_ESTABLISHED) {
      /* Send ACK here */
      ret = usnet_send_tcp_packet(ctx, tcb, TH_ACK, NULL, 0);
      sent_ack = 1;

   } else if (tcb->state == TCPS_CLOSE_WAIT) {
      /* Send ACK for the FIN here */
      ret = usnet_send_tcp_packet(ctx, tcb, TH_ACK, NULL, 0);
      sent_ack = 1;

   } else if (tcb->state == TCPS_LAST_ACK) {
      /* if it is on ack_list, send it after sending ack */
      if (sndvar->on_send_list || sndvar->on_ack_list) {
         ret = -1;
      } else {
         /* Send FIN/ACK here */
         ret = usnet_send_tcp_packet(ctx, tcb,
               TH_FIN | TH_ACK, NULL, 0);
         sent_ack = 1;
      }
   } else if (tcb->state == TCPS_FIN_WAIT_1) {
      /* if it is on ack_list, send it after sending ack */
      if (sndvar->on_send_list || sndvar->on_ack_list) {
         ret = -1;
      } else {
         /* Send FIN/ACK here */
         ret = usnet_send_tcp_packet(ctx, tcb,
               TH_FIN | TH_ACK, NULL, 0);
         sent_ack = 1;
      }

   } else if (tcb->state == TCPS_FIN_WAIT_2) {
      /* Send ACK here */
      ret = usnet_send_tcp_packet(ctx, tcb, TH_ACK, NULL, 0);
      sent_ack = 1;

   } else if (tcb->state == TCPS_CLOSING) {
      if (sndvar->is_fin_sent) {
         /* if the sequence is for FIN, send FIN */
         if (tcb->snd_nxt == sndvar->fss) {
            ret = usnet_send_tcp_packet(ctx, tcb,
                  TH_FIN | TH_ACK, NULL, 0);
            sent_ack = 1;
         } else {
            ret = usnet_send_tcp_packet(ctx, tcb,
                  TH_ACK, NULL, 0);
            sent_ack = 1;
         }
      } else {
         /* if FIN is not sent, send fin with ack */
         ret = usnet_send_tcp_packet(ctx, tcb,
               TH_FIN | TH_ACK, NULL, 0);
         sent_ack = 1;
      }

   } else if (tcb->state == TCPS_TIME_WAIT) {
      /* Send ACK here */
      ret = usnet_send_tcp_packet(ctx, tcb, TH_ACK, NULL, 0);
      sent_ack = 1;

   } else if (tcb->state == TCPS_CLOSED) {
      /* Send RST here */
      DEBUG(ctx->log,"sending rst, fd=%u",
            tcb->fd);
      /* first flush the data and ack */
      if (sndvar->on_send_list || sndvar->on_ack_list) {
         ret = -1;
      } else {
         ret = usnet_send_tcp_packet(ctx, tcb, TH_RST, NULL, 0);
         if (ret >= 0) {
            usnet_release_tcb(ctx, tcb);
         }
      }
   }

   if ( sent_ack ) {
      // not send ack later in this dispatch cycle.
      usnet_remove_ack_list(ctx,tcb);
   }
   return ret;
}

inline void 
usnet_add_control_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
#if TRY_SEND_BEFORE_QUEUE
   int ret=0; 
   ret = usnet_send_control_packet(ctx, cur_tcb);
   if (ret < 0) {
#endif   
      if (!cur_tcb->sndvar->on_control_list) {
         cur_tcb->sndvar->on_control_list = 1;
         TAILQ_INSERT_TAIL(&ctx->control_list, cur_tcb, sndvar->control_link);
         ctx->control_list_cnt++;
         DEBUG(ctx->log,"added to control list, fd=%u, cnt=%d", 
             cur_tcb->fd, ctx->control_list_cnt);
      }
#if TRY_SEND_BEFORE_QUEUE
   } else {
      if (cur_tcb->sndvar->on_control_list) {
         cur_tcb->sndvar->on_control_list = 0;
         TAILQ_REMOVE(&ctx->control_list, cur_tcb, sndvar->control_link);
         ctx->control_list_cnt--;
      }
   }
#endif

}

inline void 
usnet_remove_control_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
   if (cur_tcb->sndvar->on_control_list) {
      cur_tcb->sndvar->on_control_list = 0;
      TAILQ_REMOVE(&ctx->control_list, cur_tcb, sndvar->control_link);
      ctx->control_list_cnt--;
      //TRACE_DBG("Stream %u: Removed from control list (cnt: %d)\n", 
      //    cur_tcb->id, sender->control_list_cnt);
   }   

}

inline void 
usnet_remove_send_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{     
   DEBUG(ctx->log,"Remove from send list, fd=%u", cur_tcb->fd);
   if (cur_tcb->sndvar->on_send_list) {
      cur_tcb->sndvar->on_send_list = 0;
      TAILQ_REMOVE(&ctx->send_list, cur_tcb, sndvar->send_link);
      ctx->send_list_cnt--;
   }
}

inline void 
usnet_add_send_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
   if(!cur_tcb->sndvar->sndbuf) {
      ERROR(ctx->log,"Stream %u: No send buffer available.", 
                        cur_tcb->fd);
      //assert(0);
      return;
   }   

   DEBUG(ctx->log,"Add to send list, fd=%u", cur_tcb->fd);
   if (!cur_tcb->sndvar->on_send_list) {
      cur_tcb->sndvar->on_send_list = 1;
      TAILQ_INSERT_TAIL(&ctx->send_list, cur_tcb, sndvar->send_link);
      ctx->send_list_cnt++;
   }   
}


inline void
usnet_add_ack_list(usn_context_t *ctx, usn_tcb_t *tcb)
{
   if (!tcb->sndvar->on_ack_list) {
      tcb->sndvar->on_ack_list = 1;
      TAILQ_INSERT_TAIL(&ctx->ack_list, tcb, sndvar->ack_link);
      ctx->ack_list_cnt++;
   }
}

inline void 
usnet_remove_ack_list(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
   if (cur_tcb->sndvar->on_ack_list) {
      cur_tcb->sndvar->on_ack_list = 0;
      TAILQ_REMOVE(&ctx->ack_list, cur_tcb, sndvar->ack_link);
      ctx->ack_list_cnt--;
   }   
}

inline void 
usnet_enqueue_ack(usn_context_t *ctx, usn_tcb_t *cur_tcb, uint8_t opt)
{        
   if (!(cur_tcb->state == TCPS_ESTABLISHED || 
         cur_tcb->state == TCPS_CLOSE_WAIT ||
         cur_tcb->state == TCPS_FIN_WAIT_1 || 
         cur_tcb->state == TCPS_FIN_WAIT_2)) {
      DEBUG(ctx->log, "enqueueing ack, fd=%u, state=%s",
            cur_tcb->fd, usnet_tcp_statestr(cur_tcb));
   }
      
   if (opt == ACK_OPT_NOW) {
      if (cur_tcb->sndvar->ack_cnt < cur_tcb->sndvar->ack_cnt + 1) {
         cur_tcb->sndvar->ack_cnt++;
      }
   } else if (opt == ACK_OPT_AGGREGATE) {
      if (cur_tcb->sndvar->ack_cnt == 0) {
         cur_tcb->sndvar->ack_cnt = 1;
      }
   } else if (opt == ACK_OPT_WACK) {
      cur_tcb->sndvar->is_wack = 1;
   }
   DEBUG(ctx->log, "enqueueing ack, fd=%u, state=%s, ack_cnt=%u",
         cur_tcb->fd, usnet_tcp_statestr(cur_tcb),cur_tcb->sndvar->ack_cnt);
   usnet_add_ack_list(ctx, cur_tcb);
}

int
usnet_flush_tcp_sendbuf(usn_context_t *ctx, usn_tcb_t *cur_tcb)
{
   usn_sndvars_t *sndvar = cur_tcb->sndvar;
   usn_sendbuf_t *buf;
   const uint32_t maxlen = sndvar->mss - usnet_get_tcpopt_length(TH_ACK);
   uint8_t *data;
   uint32_t buffered_len;
   uint32_t seq;
   uint16_t len;
   int16_t sndlen;
   uint32_t window;
   int packets = 0;
   int tail_len;
   uint32_t head,tail;

   if (!sndvar->sndbuf) {
      ERROR(ctx->log,"Stream %d: No send buffer available.\n", cur_tcb->fd);
      assert(0);
      return 0;
   }

   buf = sndvar->sndbuf;

   DEBUG(ctx->log,"send buf: buf->head=%u buf->sent_head=%u, buf->tail=%u, maxlen=%d", 
         buf->head, buf->sent_head, buf->tail, maxlen);
   if (buf->head == buf->tail) {
      return 0;
   }
   head = buf->sent_head;
   tail = buf->tail;
   window = MIN(sndvar->cwnd, sndvar->peer_wnd);
   DEBUG(ctx->log,"send buf: window=%u, cwnd=%u, peer_wnd=%u, maxlen=%u",
              window, sndvar->cwnd, sndvar->peer_wnd,maxlen);

   while (1) {
      seq = cur_tcb->snd_nxt;

      DEBUG(ctx->log,"send buf: seq=%u, head_seq=%u head=%u, tail=%u",
              seq, buf->head_seq, head, tail);

      if (TCP_SEQ_LT(seq, buf->head_seq)) {
         ERROR(ctx->log,"Stream %d: Invalid sequence to send. "
               "state: %s, seq: %u, head_seq: %u.\n",
               cur_tcb->fd, usnet_tcp_statestr(cur_tcb),
               seq, buf->head_seq);
         assert(0);
         break;
      }
      //buffered_len = sndvar->sndbuf->head_seq + sndvar->sndbuf->len - seq;
      buffered_len = tail>=head ?  tail-head 
                          : buf->size + tail-head;

      if (buffered_len == 0){
         DEBUG(ctx->log,"head_seq=%u, len=%u, seq=%u, "
               "buffered_len=%u, head=%u, tail=%u",
               sndvar->sndbuf->head_seq, sndvar->sndbuf->len, 
               seq, buffered_len, head, tail);
         break;
      }

      if (buffered_len > maxlen) {
         len = maxlen;
      } else {
         len = buffered_len;
      }

      //if ( len > 1024 )
      //   len = 1024;

      //data = sndvar->sndbuf->head +
      //      (seq - sndvar->sndbuf->head_seq);
      tail_len = buf->size - buf->tail;
      if ( len >= tail_len ) {
         len = tail_len;
         data = (uint8_t*)buf->data_ptr + head;
      } else {
         data = (uint8_t*)buf->data_ptr + head;
      }

      DEBUG(ctx->log,"send data, len=%u, head_seq=%u",len,buf->head_seq);
      dump_buffer(ctx,(char*)data,len,"tcp");


      if (len <= 0) {
         ERROR(ctx->log,"len is zero, len=%u, head_seq=%u",len,buf->head_seq);
         break;
      }

      if (cur_tcb->state > TCPS_ESTABLISHED) {
         ERROR(ctx->log,"Flushing after ESTABLISHED: seq: %u, len: %u, "
               "buffered_len: %u", seq, len, buffered_len);
      }

      if (seq - sndvar->snd_una + len > window) {
         /* Ask for new window advertisement to peer */
         if (seq - sndvar->snd_una + len > sndvar->peer_wnd) {
#if 0
            TRACE_CLWND("Full peer window. "
                  "peer_wnd: %u, (snd_nxt-snd_una): %u\n", 
                  sndvar->peer_wnd, seq - sndvar->snd_una);
#endif
            if (TS_TO_MSEC(ctx->cur_time - sndvar->ts_lastack_sent) > 500) {
               usnet_enqueue_ack(ctx, cur_tcb, ACK_OPT_WACK);
            }
         }

         DEBUG(ctx->log,"Full peer window, len:%u, peer_wnd:%u, window:%u, seq:%u,"
                        " snd_nxt:%u, snd_una:%u", 
                  len, sndvar->peer_wnd, window, seq, seq, sndvar->snd_una);
         return -3;
      }

      sndlen = usnet_send_tcp_packet(ctx, cur_tcb, TH_ACK, data, len);
      if (sndlen < 0) {
         ERROR(ctx->log,"error sending: ret=%d, size=%u, sndlen=%u, head=%u, head_sent=%u, tail=%u,"
                     " head_seq=%u, packets=%u, buf->head=%u, buf->tail=%u", 
                     sndlen, buf->size, sndlen, head, buf->sent_head, 
                     tail, buf->head_seq, packets, buf->head, buf->tail);
         return sndlen;
      }
      //buf->head_seq += sndlen;

      // update buffer head.
      head += sndlen;
      if ( head >= buf->size)
         head -= buf->size;
      buf->sent_head = head;

      packets++;
      DEBUG(ctx->log,"send buf: packets=%u, size=%u, sndlen=%u, head=%u, head_sent=%u, tail=%u,"
                     " head_seq=%u, buf->head=%u, buf->tail=%u", 
                     packets, buf->size, sndlen, head, buf->sent_head, 
                     tail, buf->head_seq, packets, buf->head, buf->tail);
   }

   return packets;
}
