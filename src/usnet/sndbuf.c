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
 * @(#)sndbuf.c
 */

#include <string.h>

#include "sndbuf.h"
#include "core.h"
#include "cache.h"
#include "tcb.h"
#include "log.h"
#include "mempool.h"

int 
usnet_sb_key(const void *arg)
{
   usn_sendbuf_t *item =  (usn_sendbuf_t *)arg;
   return item->fd;
}

int 
usnet_sb_eq(const void *arg1, const void *arg2)
{
   usn_sendbuf_t *item1 = (usn_sendbuf_t *)arg1;
   usn_sendbuf_t *item2 = (usn_sendbuf_t *)arg2;
   return (item1->fd == item2->fd);
}

int
usnet_sb_isempty(const void *arg)
{
   usn_sendbuf_t *item = (usn_sendbuf_t *)arg;
   return (item->fd == 0);
}

int
usnet_sb_setempty(const void *arg)
{
   usn_sendbuf_t *item = (usn_sendbuf_t *)arg;
   item->fd = 0;
   return 0;
}

void
usnet_sendbuf_init(usn_context_t *ctx, int default_size, int create)
{

   ctx->sndbuf_cache = (usn_hashbase_t*)malloc(sizeof(usn_hashbase_t));
   if ( ctx->sndbuf_cache == NULL )
      return;

   usnet_cache_init(ctx->sndbuf_cache, 0x33341, 10, 100, 
                    sizeof(usn_sendbuf_t) + default_size,create,
                    usnet_sb_eq, usnet_sb_key, usnet_sb_isempty, usnet_sb_setempty);

   if ( create ) {
      ctx->sndvars_mp = usn_mempool_create(sizeof(usn_sndvars_t), 
                             1024 * sizeof(usn_sndvars_t), 1);
   }
}

usn_sendbuf_t*
usnet_get_sendbuf(usn_context_t *ctx, usn_tcb_t *tcb, uint32_t init_seq)
{
   usn_sendbuf_t p;
   usn_sendbuf_t *buf;

   p.fd = tcb->fd;

   buf = CACHE_GET(ctx->sndbuf_cache, &p, usn_sendbuf_t*);
   
   if ( buf == 0 )
      return 0;

   // initialize ring buffer.
   memset(buf, 0, sizeof(usn_sendbuf_t));
   buf->fd = tcb->fd;
   buf->size = ctx->sndbuf_cache->hb_objsize - sizeof(usn_sendbuf_t);
   buf->head_seq = buf->init_seq = init_seq;
   buf->data_ptr = (char*)(buf+1);

   DEBUG(ctx->log, "init_seq=%u, head_seq=%u, last_seq=%u, fd=%u",
         init_seq, buf->head_seq, buf->init_seq, buf->fd);

   return buf;
}


int
usnet_write_data(usn_context_t *ctx, usn_sendbuf_t *buf, char *data, int len)
{
   int avail_len;
   int tail_len;
   char* data_ptr;

   avail_len =  buf->head>buf->tail ? buf->head-buf->tail : buf->size + buf->head-buf->tail;
   tail_len = buf->size - buf->tail;

   DEBUG(ctx->log,"snd buf, len=%u, head=%u, tail=%u, head_seq=%u, avail_len=%u, tail_len=%u",
         len, buf->head,buf->tail,buf->head_seq,avail_len,tail_len);

   if ( avail_len <= len ) {
      ERROR(ctx->log,"not enouch space, len=%u, head=%u, tail=%u, avail_len=%u, tail_len=%u",
         len, buf->head,buf->tail,avail_len,tail_len);
      return -1; // not have enough space.
   }

   data_ptr = (char*)(buf+1);

   if ( tail_len >= len ) {
      memcpy(data_ptr + buf->tail, data, len);
      buf->tail += len;
   } else {
      memcpy(data_ptr + buf->tail, data, tail_len);
      memcpy(data_ptr, data, len-tail_len);
      buf->tail = len - tail_len;//buf->size - len;
   }
   //buf->len += len;
   buf->len = usnet_get_sendbuf_len(ctx,buf);

   DEBUG(ctx->log,"snd buf, head=%u, tail=%u, head_seq=%u",
         buf->head,buf->tail,buf->head_seq);

   return len;
}

int
usnet_remove_data(usn_context_t *ctx, usn_sendbuf_t *buf, int len)
{
   int data_len;

   data_len = buf->tail>=buf->head ? buf->tail-buf->head : 
                buf->size+buf->tail-buf->head;
   if ( len > data_len ) {
      WARN(ctx->log,"overflow, data_len=%u, len=%u", data_len, len);
      //XXX; reset buffer?
      buf->head = buf->tail = 0;
      buf->head_seq += data_len;
      //buf->len = 0;
      return data_len;
   }

   buf->head += len;
   if ( buf->head >= buf->size )
      buf->head -= buf->size;

   DEBUG(ctx->log,"update head_seq of send buffer, head_seq=%u, len=%u, new_head_seq=%u",
         buf->head_seq, len, buf->head_seq + len);
   buf->head_seq += len;

   return len;
}


inline int
usnet_get_sendbuf_len(usn_context_t *ctx, usn_sendbuf_t *buf)
{
   return buf->tail>=buf->head ? buf->tail-buf->head : 
                buf->size+buf->tail-buf->head;
}







