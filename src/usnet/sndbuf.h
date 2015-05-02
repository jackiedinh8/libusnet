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
 * @(#)sndbuf.h
 */

#ifndef _USNET_SNDBUF_H_
#define _USNET_SNDBUF_H_

#include <stdint.h>

#include "types.h"

typedef struct usn_sendbuf usn_sendbuf_t;
struct usn_sendbuf
{        
   uint32_t fd;
   uint32_t head;
   uint32_t sent_head;
   uint32_t tail;
   uint32_t len;
   uint64_t cum_len;
   uint32_t size;
   uint32_t head_seq;
   uint32_t init_seq;
   char     *data_ptr;
}; 

void
usnet_sendbuf_init(usn_context_t *ctx, int default_size, int create);

usn_sendbuf_t*
usnet_get_sendbuf(usn_context_t *ctx, usn_tcb_t *tcb, uint32_t init_seq);

int
usnet_write_data(usn_context_t *ctx, usn_sendbuf_t *buf, char *data, int len);

int
usnet_remove_data(usn_context_t *ctx, usn_sendbuf_t *buf, int len);

inline int
usnet_get_sendbuf_len(usn_context_t *ctx, usn_sendbuf_t *buf);

#endif //_USNET_SNDBUF_H_

