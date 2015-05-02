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
 * @(#)tcb.h
 */

#ifndef _USNET_TCB_H_
#define _USNET_TCB_H_

#include <sys/queue.h>

#include "types.h"
#include "tcp.h"

struct usn_rcvvars
{
   /* receiver variables */
   uint32_t rcv_wnd;    /* receive window (unscaled) */
   //uint32_t rcv_up;      /* receive urgent pointer */
   uint32_t irs;        /* initial receiving sequence */
   uint32_t snd_wl1;    /* segment seq number for last window update */
   uint32_t snd_wl2;    /* segment ack number for last window update */

   /* timestamps */
   uint32_t ts_recent;        /* recent peer timestamp */
   uint32_t ts_lastack_rcvd;  /* last ack rcvd time */
   uint32_t ts_last_ts_upd;   /* last peer ts update time */
   uint32_t ts_tw_expire;  // timestamp for timewait expire

   /* RTT estimation variables */
   uint32_t srtt;       /* smoothed round trip time << 3 (scaled) */
   uint32_t mdev;       /* medium deviation */
   uint32_t mdev_max;   /* maximal mdev ffor the last rtt period */
   uint32_t rttvar;     /* smoothed mdev_max */
   uint32_t rtt_seq;    /* sequence number to update rttvar */

   /* variables for fast retransmission */
   uint32_t last_ack_seq;  /* highest ackd seq */
   uint16_t dup_acks;       /* number of duplicated acks */

   usn_ringbuf_t *rcvbuf;
#if TCPOPT_SACK_ENABLED      /* currently not used */
#define MAX_SACK_ENTRY 8
   struct sack_entry sack_table[MAX_SACK_ENTRY];
   uint8_t sacks:3;
#endif /* TCPOPT_SACK_ENABLED */
};

typedef struct usn_sndvars usn_sndvars_t;
struct usn_sndvars
{
   /* IP-level information */
   uint16_t ip_id;

   uint16_t mss;        /* maximum segment size */
   uint16_t eff_mss;    /* effective segment size (excluding tcp option) */

   uint8_t wscale;         /* window scale */
   int8_t nif_out;         /* cached output network interface */
   unsigned char *d_haddr; /* cached destination MAC address */

   /* send sequence variables */
   uint32_t snd_una;    /* send unacknoledged */
   uint32_t snd_wnd;    /* send window (unscaled) */
   uint32_t peer_wnd;   /* client window size */
   //uint32_t snd_up;   /* send urgent pointer (not used) */
   uint32_t iss;        /* initial sending sequence */
   uint32_t fss;        /* final sending sequence */

   /* retransmission timeout variables */
   uint8_t nrtx;        /* number of retransmission */
   uint8_t max_nrtx;    /* max number of retransmission */
   uint32_t rto;        /* retransmission timeout */
   uint32_t ts_rto;     /* timestamp for retransmission timeout */

   /* congestion control variables */
   uint32_t cwnd;          /* congestion window */
   uint32_t ssthresh;      /* slow start threshold */

   /* timestamp */
   uint32_t ts_lastack_sent;  /* last ack sent time */

   uint8_t is_wack:1,        /* is ack for window adertisement? */
           ack_cnt:6;        /* number of acks to send. max 64 */

   uint8_t on_control_list;
   uint8_t on_send_list;
   uint8_t on_ack_list;
   uint8_t on_sendq;
   uint8_t on_ackq;
   uint8_t on_closeq;
   uint8_t on_resetq;

   uint8_t on_closeq_int:1,
           on_resetq_int:1,
           is_fin_sent:1,
           is_fin_ackd:1;

   TAILQ_ENTRY(usn_tcb) control_link;
   TAILQ_ENTRY(usn_tcb) send_link;
   TAILQ_ENTRY(usn_tcb) ack_link;

   TAILQ_ENTRY(usn_tcb) timer_link;    // timer link (rto list, tw list)
   TAILQ_ENTRY(usn_tcb) timeout_link;  // connection timeout link

   usn_sendbuf_t *sndbuf;
};

typedef struct usn_tcb usn_tcb_t;
struct usn_tcb {
   uint32_t fd;
   usn_socket_t   *socket;
   uint32_t        saddr;         /* in network order */
   uint32_t        daddr;         /* in network order */
   uint16_t        sport;         /* in network order */
   uint16_t        dport;         /* in network order */

   uint8_t  type;
   uint8_t state;          /* tcp state */
   uint8_t close_reason;   /* close reason */
   uint8_t on_hash_table;
   uint8_t on_timewait_list; 
   uint8_t ht_idx;
   uint8_t closed;
   uint8_t is_bound_addr;
   uint8_t need_wnd_adv;
   int16_t on_rto_idx;

   uint16_t on_timeout_list:1,
            on_rcv_br_list:1,
            on_snd_br_list:1,
            saw_timestamp:1,  /* whether peer sends timestamp */
            sack_permit:1,    /* whether peer permits SACK */
            control_list_waiting:1,
            have_reset:1;

   uint32_t snd_nxt;    /* send next */
   uint32_t rcv_nxt;    /* receive next */

   usn_rcvvars_t  *rcvvar;
   usn_sndvars_t  *sndvar;

   uint32_t last_active_ts;      /* ts_last_ack_sent or ts_last_ts_upd */

};


void
usnet_tcb_init(usn_context_t *ctx);

usn_tcb_t*
usnet_create_tcb(usn_context_t *ctx, usn_iphdr_t *iph, 
                 usn_tcphdr_t *tcph, uint32_t seq, uint16_t window);

int
usnet_register_socket(usn_context_t *ctx, usn_socket_t *so, 
      uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport);

inline usn_tcb_t*
usnet_search_tcb(usn_context_t *ctx, usn_tcb_t *sitem);

inline usn_tcb_t*
usnet_search_tcb_new(usn_context_t *ctx, usn_tcb_t *sitem);

inline usn_tcb_t*
usnet_insert_tcb(usn_context_t *ctx, usn_tcb_t *sitem);

inline int
usnet_remove_tcb(usn_context_t *ctx, usn_tcb_t *sitem);

void
usnet_release_tcb(usn_context_t *ctx, usn_tcb_t *tcb);

void
usnet_dump_tcb(usn_context_t *ctx, usn_tcb_t *stream);


#endif //_USNET_TCB_H_

