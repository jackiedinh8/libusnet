/*
 * Copyright (c) 2014 Jackie Dinh <jackiedinh8@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)usnet_buf.c
 */

#include "usnet_buf.h"
#include "usnet_slab.h"
#include "usnet_log.h"
#include "usnet_common.h"

void
usn_free_buf(u_char *m)
{
   if ( m == NULL )
      return;

   usn_slab_free(g_slab_pool, m);

   m = NULL;
   return;
}

void
usn_free_mbuf(usn_mbuf_t *m)
{
   usn_mbuf_t *n;

   if ( m == NULL )
      return;
   m->refs--;
   DEBUG("free buffer, ptr=%p, refs=%d", m, m->refs);
   if ( m->refs == 0 ) {
      n = m->next;
      if ( n )
         n->prev = 0;
      usn_slab_free(g_slab_pool, m);
   }

   m = NULL;
   return;
}

void
usn_free_mbuf_chain(usn_mbuf_t *m)
{
   usn_mbuf_t *n, *t;

   if ( m == NULL )
      return;

   n = m;
   while (n) {
      t = n; n = n->next;
      usn_free_mbuf(t);
   }

   m = NULL;
   return;
}

u_char* 
usn_get_buf(u_char* buf, int size)
{
   u_char *p;

   p = (u_char*) usn_slab_alloc(g_slab_pool, size);

   if (p) {
      if (buf) 
         memcpy(p, buf, size);
      //else
      //   memset(p, 0, size);
   }
   else {
      // bad
   }
   return p;
}

usn_mbuf_t* 
usn_get_mbuf(u_char* buf, int size, u_short flags)
{
   usn_mbuf_t *p;
   int        len;
  
   len =  sizeof(usn_mbuf_t) + size;

   p = (usn_mbuf_t*)
       usn_slab_alloc(g_slab_pool, len);

   if (p) {
      p->queue = NULL;
      p->next = NULL;
      p->prev = NULL;
      p->start = (u_char*)p;
      p->head = (u_char*)p + sizeof(usn_mbuf_t);
      p->tail = p->end = (u_char*)p + len;
      p->mlen = size;
      p->flags = flags;
      p->refs = 1;
      if (buf) 
         memcpy(p->head, buf, size);
   }
   else {
      //XXX do statistics.
      goto bad;
   }

   return p;
bad:
   return NULL;
}

// zero-copy version of usn_get_mbuf
usn_mbuf_t* 
usn_get_mbuf_zc(u_char* buf, int size, u_short flags)
{
   usn_mbuf_t *p;
   int        len;

   if ( buf == NULL )
     return NULL;
  
   len =  sizeof(usn_buf_t);

   p = (usn_mbuf_t*)
       usn_slab_alloc(g_slab_pool, len);

   if (p) {
      p->queue = NULL;
      p->next = p;
      p->prev = p;
      p->start = buf;
      p->head = buf;
      p->tail = p->end = buf + size;
      p->mlen = size;
      p->flags = flags | BUF_ZERO_COPY;
      p->refs = 1;
   }
   else {
      //XXX do statistics.
      goto bad;
   }

   return p;
bad:
   return NULL;

}

void* 
usn_create_pool(int size)
{

   return NULL;
}

void
m_adj( usn_mbuf_t *mp, int req_len)
{
   int len = req_len;
   usn_mbuf_t *m;
   int count;

   if ((m = mp) == NULL)
      return;
   if (len >= 0) {
   
      // Trim from head.
      while (m != NULL && len > 0) {
         if ( m->tail - m->head  <= len) {
            len -= m->tail - m->head;
            m->head = m->tail; //set len equal to zero.
            m->mlen = 0;
            m = m->next;
         } else {
            m->head += len;
            m->mlen -= len;
            len = 0;
         }
      }
      m = mp;
   } else {
      // Trim from tail.
      len = -len;
      count = 0;
      for (;;) {
         count += m->mlen;
         if (m->next == (usn_mbuf_t *)0)
            break;
         m = m->next;
      }
      if (m->mlen >= (u_int) len) {
         m->tail -= len;
         m->mlen -= len;
         return;
      }
      count -= len;
      if (count < 0)
         count = 0;
      for (; m; m = m->prev ) {
         if ( m->mlen >= (u_int) len ) {
            m->tail -= len;
            m->mlen -= len;
            return;
         }
         // TODO: remove unused trailing buf?
         len -= m->mlen;
         m->tail = m->head;
         m->mlen = 0;
      }
   }

   return;
}


usn_mbuf_t *
m_pullup(usn_mbuf_t *n, int len)
{
   usn_mbuf_t *m;
   u_int32 exlen; 
   u_int32 cnt;
   if ( n->head + len <= n->end ) { // have enough space
      if ( n->mlen >= len )
         return n;
      m = n;
      n = n->next;
      len -= m->mlen;
   } else { // allocate new mbuf
      exlen = n->head - n->start + len;
      m = usn_get_mbuf(0, exlen, 0); 
      if ( m == NULL ) {
        DEBUG("panic: not enough space");
        goto bad;
      }
      bcopy(n->start, m->start, n->head - n->start);
      m->head += n->head - n->start;
      n->mlen = 0;
   }
   
   do {
      cnt = min(n->mlen, len);
      bcopy(n->head, m->head + m->mlen, cnt);
      len -= cnt;
      m->mlen += cnt;
      n->mlen -= cnt;
      if ( n->mlen )
         n->head += cnt;
      else {
         usn_mbuf_t *t;
         MFREE(n, t);
         n = t;
      }
   } while ( len > 0 && n );
   
   if ( len > 0 ) {
      usn_free_mbuf(m);
      goto bad;
   }   
   m->next = n;
   return m;
bad:
   usn_free_mbuf_chain(n);
   return NULL;
}

// create a new mbuf and copy len of bytes from m.
usn_mbuf_t* 
usn_copy_data(usn_mbuf_t *m, int off0, int len)
{
   usn_mbuf_t *n, **np;
   int off = off0;
   usn_mbuf_t *top;
   //int copyhdr = 0;

   if (off < 0 || len < 0) {
      //DEBUG("invalid len or offset");
      return 0;
   }

   while (off > 0) {
      if (m == 0) {
         return 0;
      }
      if (off < m->mlen)
         break;
      off -= m->mlen;
      m = m->next;
   }

   np = &top;
   top = 0;
   while (len > 0) {

      if (m == 0) 
         break;

      n = (usn_mbuf_t*)usn_get_mbuf(0, BUF_MSIZE, 0);
      *np = n;
      if (n == 0)
         goto nospace;

      // TODO: large buffer
      n->mlen = min(len, m->mlen - off);
      bcopy(mtod(m, caddr_t)+off, mtod(n, caddr_t), n->mlen);
      len -= n->mlen;
      off = 0;
      m = m->next;
      np = &n->next;
   }
   return (top);
nospace:
   usn_free_mbuf(top);
   return (0);
}

void
usn_copy_mbuf(usn_mbuf_t *m, int32 off, int32 len, caddr_t cp)
{
   u_int32 count;

   if (off < 0 || len < 0) {
      DEBUG("invalid len or offset");
      return;
   }

   while (off > 0) {
      if (m == 0) {
         return;
      }
      if (off < m->mlen)
         break;
      off -= m->mlen;
      m = m->next;
   }   
   while (len > 0) {
      if (m == 0) {
         break;
      }
      count = min(m->mlen - off, len);
      bcopy(mtod(m, caddr_t) + off, cp, count);
      len -= count;
      cp += count;
      off = 0;
      m = m->next;
   }   
}

u_int32 
usn_get_mbuflen(usn_mbuf_t *m)
{
   u_int32 len = 0;
   while (m) {
      len += m->mlen;
      m = m->next;
      if ( m && m == m->next ) {
         DEBUG("panic: a loop in mbuf");
         break;
      }
   }
   return len;
}

u_int32 
usn_get_mbuf_actlen(usn_mbuf_t *m)
{
   u_int32 len = 0;
   while (m) {
      len += m->end - m->start;
      m = m->next;
      if ( m && m == m->next ) {
         DEBUG("panic: a loop in mbuf");
         break;
      }
   }
   return len;
}

