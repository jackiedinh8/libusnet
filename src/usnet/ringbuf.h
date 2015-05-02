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
 * @(#)ringbuf.h
 */

#ifndef _USNET_RINGBUF_H_
#define _USNET_RINGBUF_H_

#include <stdint.h>

#include "types.h"

struct fragment {
   uint32_t        seq;
   uint16_t        len;
   uint16_t        flags;
   struct fragment *next;
};
#define FRAG_CALLOC 0x01

typedef struct usn_ringbuf usn_ringbuf_t;
struct usn_ringbuf {
   uint32_t fd;
   uint16_t data;
   uint16_t head;
   uint32_t size;
   uint32_t head_seq;
   uint32_t last_seq;
   uint32_t merged_len;
   uint32_t win;
   struct fragment *frags;
   char*    data_ptr;
};

void 
usnet_ringbuf_init(usn_context_t *ctx, int default_size, int create_mp);

usn_ringbuf_t*
usnet_get_ringbuf(usn_context_t *ctx, usn_tcb_t *tcb, uint32_t init_seq);

void
usnet_release_ringbuf(usn_context_t *ctx, usn_ringbuf_t *buf);

int
usnet_insert_fragment(usn_context_t *ctx, usn_ringbuf_t *buf, 
                           void* data, int len, uint32_t seq);

int
usnet_remove_fragment(usn_context_t *ctx, usn_ringbuf_t *buf, uint32_t next_seq);

#endif //_USNET_RINGBUF_H_
