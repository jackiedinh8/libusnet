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
 * @(#)tcp.h
 */

#ifndef _USNET_TCP_H_
#define _USNET_TCP_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include "types.h"
#include "ip.h"

typedef struct tcphdr usn_tcphdr_t;
struct tcphdr {
	uint16_t th_sport;		/* source port */
	uint16_t th_dport;		/* destination port */
	uint32_t	th_seq;			/* sequence number */
	uint32_t th_ack;			/* acknowledgement number */
#if BYTE_ORDER == LITTLE_ENDIAN 
   unsigned char   th_x2:4,		   /* (unused) */
                   th_off:4;		   /* data offset */
#endif
#if BYTE_ORDER == BIG_ENDIAN 
   unsigned char	 th_off:4,		   /* data offset */
                   th_x2:4;		   /* (unused) */
#endif
	char	th_flags;
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
#define	TH_SACK	0x40
#define	TH_WACK	0x80
	short	th_win;			/* window */
	short	th_sum;			/* checksum */
	short	th_urp;			/* urgent pointer */
} __attribute__((packed)) ;



#define  TCPS_CLOSED       0  /* closed */
#define  TCPS_LISTEN       1  /* listening for connection */
#define  TCPS_SYN_SENT     2  /* active, have sent syn */
#define  TCPS_SYN_RECEIVED 3  /* have send and received syn */
#define  TCPS_ESTABLISHED  4  /* established */
#define  TCPS_FIN_WAIT_1   5  /* have closed, sent fin */
#define  TCPS_FIN_WAIT_2   6  /* have closed, fin is acked */
#define  TCPS_CLOSE_WAIT   7  /* rcvd fin, waiting for close */
#define  TCPS_CLOSING      8  /* closed xchd FIN; await FIN ACK */
#define  TCPS_LAST_ACK     9  /* had fin and close; await FIN ACK */
#define  TCPS_TIME_WAIT    10 /* in 2*msl quiet wait after close */
#define  TCP_NSTATES       11

#define  TCPS_HAVERCVDSYN(s)  ((s) >= TCPS_SYN_RECEIVED)
#define  TCPS_HAVERCVDFIN(s)  ((s) >= TCPS_TIME_WAIT)

/* reason to close */
#define   TCP_NOT_CLOSED     0 
#define   TCP_ACTIVE_CLOSE   1 
#define   TCP_PASSIVE_CLOSE  2
#define   TCP_CONN_FAIL      3 
#define   TCP_CONN_LOST      4
#define   TCP_RESET          5 
#define   TCP_NO_MEM         6
#define   TCP_NOT_ACCEPTED   7 
#define   TCP_TIMEDOUT       8

#define TCPOPT_EOL             0
#define TCPOPT_NOP             1
#define TCPOPT_MAXSEG          2
#define TCPOLEN_MAXSEG         4
#define TCPOPT_WINDOW          3
#define TCPOLEN_WINDOW         3
#define TCPOPT_SACK_PERMITTED  4 
#define TCPOLEN_SACK_PERMITTED 2
#define TCPOPT_SACK            5 
#define TCPOPTLEN_SACK            10
#define TCPOPT_TIMESTAMP       8
#define TCPOLEN_TIMESTAMP      10
#define TCPOLEN_TSTAMP_APPA (TCPOLEN_TIMESTAMP+2) // appendix A

#define TCPOPT_TSTAMP_HDR	\
    (TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)

#define TCP_MSS	512
#define TCP_DEFAULT_MSS 1460
#define TCP_DEFAULT_WSCALE    7
#define TCP_MAXWIN	65535	// largest value for (unscaled) window
#define TCP_MAX_WINSHIFT	14	// maximum window shift
#define TCP_MAX_SEQ 4294967295
#define TCP_INITIAL_WINDOW    14600 // initial window size
#define TCP_MAX_WINDOW 65535
#define TCP_SNDBUF_SIZE 8192
#define TCP_HEADER_LEN 20

#define TCP_SEQ_LT(a,b)       ((int32_t)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b)      ((int32_t)((a)-(b)) <= 0)
#define TCP_SEQ_GT(a,b)       ((int32_t)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b)      ((int32_t)((a)-(b)) >= 0)
#define TCP_SEQ_BETWEEN(a,b,c)   (TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))

/* convert timeval to timestamp (precision: 10us) */
#ifdef __FreeBSD__
#define HZ                    1000
#endif

#define TIME_TICK             (1000000/HZ) // in us
#define TIMEVAL_TO_TS(t)      (uint32_t)((t)->tv_sec * HZ + \
                              ((t)->tv_usec / TIME_TICK))

#define TS_TO_USEC(t)         ((t) * TIME_TICK)
#define TS_TO_MSEC(t)         (TS_TO_USEC(t) / 1000)

#define USEC_TO_TS(t)         ((t) / TIME_TICK)
#define MSEC_TO_TS(t)         (USEC_TO_TS((t) * 1000))

#define SEC_TO_USEC(t)        ((t) * 1000000)
#define SEC_TO_MSEC(t)        ((t) * 1000)
#define MSEC_TO_USEC(t)       ((t) * 1000)
#define USEC_TO_SEC(t)        ((t) / 1000000)
//#define TCP_TIMEWAIT        (MSEC_TO_USEC(5000) / TIME_TICK) // 5s
#define TCP_TIMEWAIT       0
#define TCP_INITIAL_RTO       (MSEC_TO_USEC(500) / TIME_TICK)     // 500ms
#define TCP_FIN_RTO           (MSEC_TO_USEC(500) / TIME_TICK)     // 500ms
#define TCP_TIMEOUT           (MSEC_TO_USEC(30000) / TIME_TICK)   // 30s

#define TCP_MAX_RTX           16
#define TCP_MAX_SYN_RETRY     7
#define TCP_MAX_BACKOFF       7


void 
usnet_tcp_init(usn_context_t *ctx);

int
usnet_tcp_input(usn_context_t *ctx, usn_iphdr_t *iph, int iplen);

#endif // _USNET_TCP_H_


