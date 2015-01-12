#include "usnet_buf.h"
#include "usnet_slab.h"
#include "usnet_log.h"


void
usn_free_buf(u_char *m)
{
   // XXX free a buffer
   if ( m == NULL )
      return;

   usn_slab_free(g_slab_pool, m);

   m = NULL;
   return;
}

void
usn_free_mbuf(usn_mbuf_t *m)
{
   // XXX free a buffer
   if ( m == NULL )
      return;
   m->refs--;
   DEBUG("free buffer, ptr=%p, refs=%d", m, m->refs);
   if ( m->refs == 0 )
      usn_slab_free(g_slab_pool, m);

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
      /*
       * Trim from head.
       */
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
      //if (mp->flags & M_PKTHDR)
      //   m->m_pkthdr.len -= (req_len - len);
   } else {
      /*
       * Trim from tail.  Scan the mbuf chain,
       * calculating its length and finding the last mbuf.
       * If the adjustment only affects this mbuf, then just
       * adjust and return.  Otherwise, rescan and truncate
       * after the remaining size.
       */
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
m_pullup(usn_mbuf_t *m, int len)
{
   // TODO implement this 
   DEBUG("not implemeted yet");
   return m;
}

inline usn_mbuf_t* 
usn_mbuf_copy(usn_mbuf_t *m, int off, int len)
{
   // XXX create a new mbuf and copy len of bytes
   // from m.
   return m;
}

