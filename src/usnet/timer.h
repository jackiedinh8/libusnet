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
 * @(#)timer.h
 */

#ifndef _USN_TIMER_H_
#define _USN_TIMER_H_

#include <stdint.h>

#include "types.h"
#include "tcb.h"

#define RTO_HASH 3000
   
struct rto_hashstore 
{
   uint32_t rto_now_idx;
   uint32_t rto_now_ts;
      
   TAILQ_HEAD(rto_head , usn_tcb) rto_list[RTO_HASH+1];
}; 
   
struct rto_hashstore*
usnet_rtohash_init(usn_context_t *ctx);

inline void 
usnet_add_rto_list(usn_context_t *ctx, usn_tcb_t *cur_tcb);

inline void 
usnet_remove_rto_list(usn_context_t *ctx, usn_tcb_t *cur_tcb);

inline void
usnet_add_timewait_list(usn_context_t *ctx, usn_tcb_t *cur_stream);

inline void 
usnet_remove_timewait_list(usn_context_t *ctx, usn_tcb_t *cur_tcb);

inline void
usnet_add_timeout_list(usn_context_t* ctx, usn_tcb_t *cur_tcb);

inline void
usnet_remove_timeout_list(usn_context_t* ctx, usn_tcb_t *cur_tcb);

inline void  
usnet_update_timeout_list(usn_context_t *ctx, usn_tcb_t *tcb);

inline void 
usnet_update_retransmission_timer(usn_context_t *ctx, usn_tcb_t *cur_stream);


inline void 
usnet_rearrange_rto_store(usn_context_t *ctx);

void
usnet_check_rtm_timeout(usn_context_t *ctx, int thresh);

void
usnet_check_timewait_expire(usn_context_t *ctx, int thresh);

void
usnet_check_connection_timeout(usn_context_t *ctx, int thresh);

#endif //_USN_TIMER_H_

