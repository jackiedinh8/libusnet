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
 * @(#)ringbuf.c
 */

#include <assert.h>

#include "ringbuf.h"
#include "core.h"
#include "cache.h"
#include "mempool.h"
#include "log.h"
#include "utils.h"

int 
usnet_rb_key(const void *arg)
{
   usn_ringbuf_t *item =  (usn_ringbuf_t *)arg;
   return item->fd;
}

int 
usnet_rb_eq(const void *arg1, const void *arg2)
{
   usn_ringbuf_t *item1 = (usn_ringbuf_t *)arg1;
   usn_ringbuf_t *item2 = (usn_ringbuf_t *)arg2;
   return (item1->fd == item2->fd);
}

int
usnet_rb_isempty(const void *arg)
{
   usn_ringbuf_t *item = (usn_ringbuf_t *)arg;
   return (item->fd == 0);
}

int
usnet_rb_setempty(const void *arg)
{
   usn_ringbuf_t *item = (usn_ringbuf_t *)arg;
   item->fd = 0;
   return 0;
}

void 
usnet_ringbuf_init(usn_context_t *ctx, int default_size, int create_mp)
{

   ctx->ringbuf_cache = (usn_hashbase_t*)malloc(sizeof(usn_hashbase_t));
   if ( ctx->ringbuf_cache == NULL )
      return;

   usnet_cache_init(ctx->ringbuf_cache, 0x33340, 10, 100, sizeof(usn_ringbuf_t) + default_size,1,
                    usnet_rb_eq, usnet_rb_key, usnet_rb_isempty, usnet_rb_setempty);

   if ( create_mp ) {
      ctx->frag_mp = usn_mempool_create(sizeof(struct fragment), 
                             1024 * sizeof(struct fragment), 1);

      ctx->rcvvars_mp = usn_mempool_create(sizeof(usn_rcvvars_t), 
                             1024 * sizeof(usn_rcvvars_t), 1);
   }
   return;
}

usn_ringbuf_t*
usnet_get_ringbuf(usn_context_t *ctx, usn_tcb_t *tcb, uint32_t init_seq)
{
   usn_ringbuf_t p;
   usn_ringbuf_t *buf;

   p.fd = tcb->fd;

   buf = CACHE_GET(ctx->ringbuf_cache, &p, usn_ringbuf_t*);
   
   if ( buf == 0 )
      return 0;

   // initialize ring buffer.
   memset(buf, 0, sizeof(usn_ringbuf_t));
   buf->fd = tcb->fd;
   buf->data = buf->head = 0;
   buf->size = ctx->ringbuf_cache->hb_objsize - sizeof(usn_ringbuf_t);
   buf->head_seq = buf->last_seq = init_seq;
   buf->data_ptr = (char*)(buf + 1);

   DEBUG(ctx->log, "init_seq=%u, head_seq=%u, last_seq=%u",
         init_seq, buf->head_seq, buf->last_seq);

   return buf;
}

struct fragment*
usnet_alloc_fragment(usn_context_t *ctx)
{
   struct fragment *frag = 0;
   int    is_calloc = 0;

   frag = usnet_mempool_allocate(ctx->frag_mp);
   if ( frag == 0 ) {
      ERROR(ctx->log,"fragments depleted, fall back to calloc");
      frag = calloc(1, sizeof(struct fragment));
      if (frag == NULL) {
         ERROR(ctx->log,"calloc failed\n");
         return 0;
      }
      is_calloc = 1;
   }
   memset(frag, 0, sizeof(*frag));
   if ( is_calloc )
      frag->flags = FRAG_CALLOC;

   return frag;
}

void
usnet_release_ringbuf(usn_context_t *ctx, usn_ringbuf_t *buf)
{
   return;
}

#define MAXSEQ               ((uint32_t)(0xFFFFFFFF))
static inline uint32_t
minseq(uint32_t a, uint32_t b)
{
   if (a == b) return a;
   if (a < b)
      return ((b - a) <= MAXSEQ/2) ? a : b;
   /* b < a */
   return ((a - b) <= MAXSEQ/2) ? b : a;
}

static inline uint32_t
maxseq(uint32_t a, uint32_t b)
{
   if (a == b) return a;
   if (a < b)
      return ((b - a) <= MAXSEQ/2) ? b : a;
   /* b < a */
   return ((a - b) <= MAXSEQ/2) ? a : b;
}

static inline int
canmerge(const struct fragment *a, const struct fragment *b)
{
   uint32_t a_end = a->seq + a->len + 1;
   uint32_t b_end = b->seq + b->len + 1;

   if (minseq(a_end, b->seq) == a_end ||
      minseq(b_end, a->seq) == b_end)
      return 0;
   return (1);
}

static inline void
merge_fragments(struct fragment *a, struct fragment *b) 
{
   /* merge a into b */
   uint32_t min_seq, max_seq;

   min_seq = minseq(a->seq, b->seq);
   max_seq = maxseq(a->seq + a->len, b->seq + b->len);
   b->seq  = min_seq;
   b->len  = max_seq - min_seq;
}

static inline void
usnet_free_onefragment(usn_mempool_t *mp, struct fragment *frag)
{  
   if (frag->flags & FRAG_CALLOC)
      free(frag);
   else  
      usnet_mempool_free(mp, frag);
}

static inline void
usnet_free_fragment_chain(usn_mempool_t *mp, struct fragment *frag)
{  
/*
   if (frag->flags & FRAG_CALLOC)
      free(frag);
   else  
      usnet_mempool_free(ctx->frag_mp, frag);
*/
}


int
usnet_insert_fragment(usn_context_t *ctx, usn_ringbuf_t *buf, 
                           void* data, int len, uint32_t seq)
{
   struct fragment *new_frag;   
   struct fragment *iter;
   struct fragment *prev, *pprev;
   int data_off, start_off, end_off;
   int tail_len, avail_len;
   //char *data_ptr = (char*)(buf + 1);
   int merged = 0;

   if ( minseq(buf->head_seq, seq) != buf->head_seq ) {
      ERROR(ctx->log,"wrong seq, seq=%u, head_seq=%u", seq, buf->head_seq);
      return -1;
   }
    
   if ( len > buf->size )
      return -2;//not have enough space.

   avail_len = (buf->data>buf->head? buf->data-buf->head 
                       : buf->data + buf->size - buf->head);

   data_off = seq - buf->head_seq;
   end_off = data_off + len;
   if ( end_off  > avail_len )
      return -3; //out of range.

   DEBUG(ctx->log, "head_seq=%u, last_seq=%u, new_last_seq=%u, avail_len=%u", 
         buf->head_seq, buf->last_seq, seq+len, avail_len);

   if ( seq + len > buf->last_seq )
      buf->last_seq = seq + len;

   start_off = buf->head + data_off;
   if ( start_off > buf->size )
      start_off -= buf->size;

   tail_len = buf->size - start_off;
   if ( tail_len >= len ) {
      memcpy(buf->data_ptr + start_off, data, len);
      buf->head += len;

      DEBUG(ctx->log, "tcp payload, len=%d",len);
      dump_buffer(ctx,buf->data_ptr + start_off,len,"tcp");
   } else {
      // copy first part.
      memcpy(buf->data_ptr + start_off, data, tail_len);
      // copy second part.
      memcpy(buf->data_ptr, data+tail_len, len-tail_len);
      buf->head += len -  buf->size;

      DEBUG(ctx->log, "tcp payload, len=%d",tail_len);
      dump_buffer(ctx,buf->data_ptr + start_off,tail_len,"tcp");
      DEBUG(ctx->log, "tcp payload, len=%d",len-tail_len);
      dump_buffer(ctx,buf->data_ptr,len-tail_len,"tcp");
   }
   
   new_frag = usnet_mempool_allocate(ctx->frag_mp);
   if ( new_frag == NULL )
      return -4;

   new_frag->seq  = seq;
   new_frag->len  = len;
   new_frag->next = NULL;

   // traverse list of fragments, merging if possible.
   for (iter = buf->frags, prev = NULL, pprev = NULL;
      iter != NULL;
      pprev = prev, prev = iter, iter = iter->next) {

      if (canmerge(new_frag, iter)) {
         /* merge the first fragment into the second fragment */
         merge_fragments(new_frag, iter);

         /* remove the first fragment */
         if (prev == new_frag) {
            if (pprev)
               pprev->next = iter;
            else
               buf->frags = iter;
            prev = pprev;
         } 
         usnet_free_onefragment(ctx->frag_mp, new_frag);
         new_frag = iter;
         merged = 1;
      }
      else if (merged ||
             maxseq(seq + len, iter->seq) == iter->seq) {
         /* merged at some point, but no more mergeable
            then stop it now */
         break;
      }
   }

   if (!merged) {
      if (buf->frags == NULL) {
         buf->frags = new_frag;
      } else if (minseq(seq, buf->frags->seq) == seq) {
         /* if the new packet's seqnum is before the existing fragments */
         new_frag->next = buf->frags;
         buf->frags = new_frag;
      } else {
         /* if the seqnum is in-between the fragments or
            at the last */
         assert(minseq(seq, prev->seq + prev->len) ==
               prev->seq + prev->len);
         prev->next = new_frag;
         new_frag->next = iter;
      }
   }

   if (buf->head_seq == buf->frags->seq) {
      avail_len = buf->frags->len;
      buf->merged_len = avail_len;
      iter = buf->frags;
      buf->frags = iter->next;
      buf->head_seq += avail_len;

      usnet_free_onefragment(ctx->frag_mp, iter);
   }
   buf->win = (buf->data>buf->head ? buf->data-buf->head 
                     : buf->size + buf->data - buf->head ) -1;  
   DEBUG(ctx->log, "data=%u, head=%u, seq=%u, len=%u, head_seq=%u, last_seq=%u, win=%u",
         buf->data, buf->head, seq, len, buf->head_seq, buf->last_seq, buf->win);

   return len;
}

int
usnet_remove_fragment(usn_context_t *ctx, usn_ringbuf_t *buf, uint32_t next_seq)
{
   return 0;
}


