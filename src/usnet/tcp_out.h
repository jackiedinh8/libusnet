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

#ifndef USNET_TCP_OUT_H_
#define USNET_TCP_OUT_H_

#include "types.h"

enum ack_opt
{
   ACK_OPT_NOW,
   ACK_OPT_AGGREGATE, 
   ACK_OPT_WACK
};

int
usnet_send_control_packet(usn_context_t *ctx, usn_tcb_t *tcb);

int   
usnet_send_tcp_packet_alone(usn_context_t *ctx, uint32_t saddr, uint16_t sport, 
               uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack_seq,
               uint16_t window, uint8_t flags, uint8_t *payload, int payloadlen,
               uint32_t cur_ts, uint32_t echo_ts);

int
usnet_send_tcp_packet(usn_context_t *ctx, usn_tcb_t *tcb, uint8_t flags, 
      uint8_t *payload, int payloadlen);

int
usnet_flush_tcp_sendbuf(usn_context_t *ctx, usn_tcb_t *cur_tcb);

int
usnet_tcp_output(usn_context_t *ctx, unsigned char* buf, int len);


inline void 
usnet_add_control_list(usn_context_t *ctx, usn_tcb_t *cur_tcb);

inline void 
usnet_remove_control_list(usn_context_t *ctx, usn_tcb_t *cur_tcb);

inline void
usnet_add_ack_list(usn_context_t *ctx, usn_tcb_t *tcb);

inline void 
usnet_remove_ack_list(usn_context_t *ctx, usn_tcb_t *cur_tcb);

inline void 
usnet_add_send_list(usn_context_t *ctx, usn_tcb_t *cur_tcb);

inline void 
usnet_remove_send_list(usn_context_t *ctx, usn_tcb_t *cur_tcb);

inline void 
usnet_enqueue_ack(usn_context_t *ctx, usn_tcb_t *cur_tcb, uint8_t opt);

#endif //USNET_TCP_OUT_H_
