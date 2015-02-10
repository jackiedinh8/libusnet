/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
 * The Regents of the University of California.  All rights reserved.
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
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
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
 * @(#)uipc_socket2.c   8.2 (Berkeley) 2/14/95
 */

#include "usnet_socket.h"
#include "usnet_socket.h"
#include "usnet_common.h"
#include "usnet_log.h"

u_long g_sb_max;
/*
 * Allot mbufs to a sockbuf.
 * Attempt to scale mbmax so that mbcnt doesn't become limiting
 * if buffering efficiency is near the normal case.
 */
int
sbreserve(struct sockbuf *sb, u_long cc)
{
   if (cc > g_sb_max * MCLBYTES / (MSIZE + MCLBYTES))
      return (0);
   sb->sb_hiwat = cc;
   sb->sb_mbmax = min(cc * 2, g_sb_max);
   if (sb->sb_lowat > sb->sb_hiwat)
      sb->sb_lowat = sb->sb_hiwat;
   return (1);
}

/*
 * Free mbufs held by a socket, and reserved mbuf space.
 */
void
sbrelease( struct sockbuf *sb)
{
   sbflush(sb);
   sb->sb_hiwat = sb->sb_mbmax = 0;
}
/*
 * Socket buffer (struct sockbuf) utility routines.
 *
 * Each socket contains two socket buffers: one for sending data and
 * one for receiving data.  Each buffer contains a queue of mbufs,
 * information about the number of mbufs and amount of data in the
 * queue, and other fields allowing select() statements and notification
 * on data availability to be implemented.
 *
 * Data stored in a socket buffer is maintained as a list of records.
 * Each record is a list of mbufs chained together with the m_next
 * field.  Records are chained together with the m_nextpkt field. The upper
 * level routine soreceive() expects the following conventions to be
 * observed when placing information in the receive buffer:
 *
 * 1. If the protocol requires each message be preceded by the sender's
 *    name, then a record containing that name must be present before
 *    any associated data (mbuf's must be of type MT_SONAME).
 * 2. If the protocol supports the exchange of ``access rights'' (really
 *    just additional data associated with the message), and there are
 *    ``rights'' to be received, then a record containing this data
 *    should be present (mbuf's must be of type MT_RIGHTS).
 * 3. If a name or rights record exists, then it must be followed by
 *    a data record, perhaps of zero length.
 *
 * Before using a new socket structure it is first necessary to reserve
 * buffer space to the socket, by calling sbreserve().  This should commit
 * some of the available buffer space in the system buffer pool for the
 * socket (currently, it does nothing but enforce limits).  The space
 * should be released by calling sbrelease() when the socket is destroyed.
 */
int
soreserve(struct usn_socket *so, u_long sndcc, u_long rcvcc)
{

   if (sbreserve(&so->so_snd, sndcc) == 0)
      goto bad;
   if (sbreserve(&so->so_rcv, rcvcc) == 0)
      goto bad2;
   if (so->so_rcv.sb_lowat == 0)
      so->so_rcv.sb_lowat = 1;
   if (so->so_snd.sb_lowat == 0)
      so->so_snd.sb_lowat = MCLBYTES;
   if (so->so_snd.sb_lowat > so->so_snd.sb_hiwat)
      so->so_snd.sb_lowat = so->so_snd.sb_hiwat;
   return (0);
bad2:
   sbrelease(&so->so_snd);
bad:
   return -1;// no buffer available: (ENOBUFS);
}
 
void
soisdisconnecting(struct usn_socket *so)
{  

   so->so_state &= ~USN_ISCONNECTING;
   so->so_state |= (USN_ISDISCONNECTING|USN_CANTRCVMORE|USN_CANTSENDMORE);
   // FIXME: callbacks
   //wakeup((caddr_t)&so->so_timeo);
   //sowwakeup(so);
   //sorwakeup(so);
}  

void
soisconnecting(struct usn_socket *so)
{

   so->so_state &= ~(USN_ISCONNECTED|USN_ISDISCONNECTING);
   so->so_state |= USN_ISCONNECTING; 
}      

/*
 * Socantsendmore indicates that no more data will be sent on the
 * socket; it would normally be applied to a socket when the user
 * informs the system that no more data is to be sent, by the protocol
 * code (in case PRU_SHUTDOWN).  Socantrcvmore indicates that no more data
 * will be received, and will normally be applied to the socket by a
 * protocol when it detects that the peer will send no more data.
 * Data queued for reading in the socket may yet be read.
 */

void
socantsendmore(struct usn_socket *so)
{

   so->so_state |= USN_CANTSENDMORE;
   // FIXME: implement it.
   //sowwakeup(so);
   return;
}

void
soisdisconnected(struct usn_socket *so)
{
   so->so_state &= ~(USN_ISCONNECTING|USN_ISCONNECTED|USN_ISDISCONNECTING);
   so->so_state |= (USN_CANTRCVMORE|USN_CANTSENDMORE);

   // FIXME: fix callbacks
   //wakeup((caddr_t)&so->so_timeo);
   //sowwakeup(so);
   //sorwakeup(so);
}

/*
 * When an attempt at a new connection is noted on a socket
 * which accepts connections, sonewconn is called.  If the
 * connection is possible (subject to space constraints, etc.)
 * then we allocate a new structure, propoerly linked into the
 * data structure of the original socket, and return this.
 * Connstatus may be 0, or SO_ISCONFIRMING, or SO_ISCONNECTED.
 *
 * Currently, sonewconn() is defined as sonewconn1() in socketvar.h
 * to catch calls that are missing the (new) second parameter.
 */
struct usn_socket*
sonewconn1(struct usn_socket *head, int connstatus)
{
   struct usn_socket *so;
   int    soqueue = connstatus ? 1 : 0;
   int    i;

   DEBUG("new socket, glen=%d, q0len=%d, qlimit=%d", 
         head->so_qlen, head->so_q0len, head->so_qlimit); 
   if (head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2) {
      DEBUG("full queue, arg1=%d, arg2=%d", 
            head->so_qlen + head->so_q0len, 3 * head->so_qlimit / 2);
      return ((struct usn_socket *)0);
   }

   //MALLOC(so, struct socket *, sizeof(*so), M_SOCKET, M_DONTWAIT);
   so = (struct usn_socket*)usn_get_buf(0, sizeof(*so));
   if (so == NULL) {
      DEBUG("failed to allocate mem");
      return ((struct usn_socket *)0);
   }

   bzero((caddr_t)so, sizeof(*so));
   so->so_domain = head->so_domain;
   so->so_type = head->so_type;
   so->so_options = head->so_options &~ SO_ACCEPTCONN;
   so->so_linger = head->so_linger;
   so->so_state = head->so_state | USN_NOFDREF; // no fd yet
   // FIXME: do we need?
   //so->so_proto = head->so_proto;
   so->so_timeo = head->so_timeo;
   so->so_pgid = head->so_pgid;
   so->so_usrreq = head->so_usrreq;
   so->so_appcb = head->so_appcb;

   soreserve(so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);

   soqinsque(head, so, soqueue);

   if ((*so->so_usrreq)(so, PRU_ATTACH,
       (usn_mbuf_t *)0, (usn_mbuf_t *)0, (usn_mbuf_t *)0)) {
      soqremque(so, soqueue);
      usn_free_buf((u_char*)so);
      DEBUG("failed to attach new socket");
      return ((struct usn_socket *)0);
   }
   if (connstatus) {
      // FIXME: callbacks
      //sorwakeup(head);
      //wakeup((caddr_t)&head->so_timeo);
      so->so_state |= connstatus;
   }

   DEBUG("new connect has been created");
   for ( i=1; i<MAX_SOCKETS; i++) {
      if ( g_fds[i] == 0 ) break;
   }
   if ( i >= MAX_SOCKETS ) {
      DEBUG("ERROR: fd not available");
      return 0;
   }
   g_fds[i] = so;
   so->so_fd = i;
   so->so_state = head->so_state & ~USN_NOFDREF;

   return (so);
}

/*     
 * Drop data from (the front of) a sockbuf.
 */
void
sbdrop(struct sockbuf *sb, int len)
{
   usn_mbuf_t *m, *mn;
   usn_mbuf_t *next;
   next = (m = sb->sb_mb) ? m->queue : 0;
   while (len > 0) {
      if (m == 0) {
         if (next == 0) {
            DEBUG("panic: sbdrop");
            return;
         }
         m = next;
         next = m->queue;
         continue;
      }
      if (m->mlen > len) {
         m->mlen -= len;
         m->head += len;
         sb->sb_cc -= len;
         break;
      }
      len -= m->mlen;
      sbfree(sb, m);
      MFREE_FIRST(m, mn);
      m = mn;
   }
   while (m && m->mlen == 0) {
      sbfree(sb, m);
      MFREE_FIRST(m, mn);
      m = mn;
   }
   if (m) {
      sb->sb_mb = m;
      m->next = next;
   } else
      sb->sb_mb = next;
}

/*
 * Wakeup processes waiting on a socket buffer.
 * Do asynchronous notification via SIGIO
 * if the socket has the SS_ASYNC flag set.
 */
void
sowakeup(struct usn_socket *so, struct sockbuf *sb)
{
   DEBUG("waking up a process");
   usnet_tcpwakeup_socket(so, sb);
   return;
/*
   struct proc *p;

   selwakeup(&sb->sb_sel);
   sb->sb_flags &= ~SB_SEL;
   if (sb->sb_flags & SB_WAIT) {
      sb->sb_flags &= ~SB_WAIT;
      wakeup((caddr_t)&sb->sb_cc);
   }
   if (so->so_state & SS_ASYNC) {
      if (so->so_pgid < 0)
         gsignal(-so->so_pgid, SIGIO);
      else if (so->so_pgid > 0 && (p = pfind(so->so_pgid)) != 0)
         psignal(p, SIGIO); 
   }   
*/
}

int32
soewakeup(struct usn_socket *so, struct sockbuf *sb)
{
   DEBUG("event occurs");
   return usnet_ewakeup_socket(so, sb);
}

void
soqinsque(struct usn_socket *head, struct usn_socket *so, int q)
{
   struct usn_socket **prev;
   so->so_head = head;
   if (q == 0) {
      head->so_q0len++;
      so->so_q0 = 0;
      for (prev = &(head->so_q0); *prev; )
         prev = &((*prev)->so_q0);
   } else {
      head->so_qlen++;
      so->so_q = 0;
      for (prev = &(head->so_q); *prev; )
         prev = &((*prev)->so_q);
   }   
   *prev = so; 
}

int
soqremque(struct usn_socket *so, int q)
{
   struct usn_socket *head, *prev, *next;

   head = so->so_head;
   prev = head;
   for (;;) {
      next = q ? prev->so_q : prev->so_q0;
      if (next == so) 
         break;
      if (next == 0)
         return (0);
      prev = next;
   }   
   if (q == 0) {
      prev->so_q0 = next->so_q0;
      head->so_q0len--;
   } else {
      prev->so_q = next->so_q;
      head->so_qlen--;
   }   
   next->so_q0 = next->so_q = 0;
   next->so_head = 0;
   return (1);
}

void
soisconnected(struct usn_socket *so)
{
   struct usn_socket *head = so->so_head;

   so->so_state &= ~(USN_ISCONNECTING|USN_ISDISCONNECTING|USN_ISCONFIRMING);
   so->so_state |= USN_ISCONNECTED;
   if (head && soqremque(so, 0)) {
      DEBUG("tcp connection is established");
      soqinsque(head, so, 1);
      sorwakeup(so);
      // FIXME: implement it.
      //sorwakeup(head);
      //wakeup((caddr_t)&head->so_timeo);
   } else {
      // FIXME: implement it.
      //wakeup((caddr_t)&so->so_timeo);
      sorwakeup(so);
      sowwakeup(so);
   }
}

void  
sohasoutofband(struct usn_socket *so)
{     
   // FIXME: implement it.
   return;
/*
   struct proc *p;
      
   if (so->so_pgid < 0)
      gsignal(-so->so_pgid, SIGURG);
   else if (so->so_pgid > 0 && (p = pfind(so->so_pgid)) != 0)
      psignal(p, SIGURG);
   selwakeup(&so->so_rcv.sb_sel);
*/
}  

void
socantrcvmore( struct usn_socket *so)
{        
   so->so_state |= USN_CANTRCVMORE;
   sorwakeup(so);
}  

/* 
 * Must be called at splnet...
 */       
int
soabort( struct usn_socket *so)
{        
   return ((*so->so_usrreq)(so, PRU_ABORT,
      (usn_mbuf_t *)0, (usn_mbuf_t *)0, (usn_mbuf_t *)0));
}

/*
 * Compress mbuf chain m into the socket
 * buffer sb following mbuf n.  If n
 * is null, the buffer is presumed empty.
 */
void
sbcompress(struct sockbuf *sb, usn_mbuf_t *m, usn_mbuf_t *n) 
{
   usn_mbuf_t *t; 

   while (m) {
      if (m->mlen == 0 ) {
         MFREE_FIRST(m,t);
         m = t;
         continue;
      }   
      if (n && (n->head + n->mlen + m->mlen) < n->end) {
         bcopy(mtod(m, caddr_t), mtod(n, caddr_t) + n->mlen,
             (unsigned)m->mlen);
         n->mlen += m->mlen;
         sb->sb_cc += m->mlen;
         MFREE_FIRST(m,t);
         m = t;
         continue;
      }   
      if (n) 
         n->next = m;
      else
         sb->sb_mb = m;
      sballoc(sb, m);
      n = m;
      m->flags &= ~BUF_EOR;
      m = m->next;
      n->next = 0;
   }  
   return; 
}  

/*
 * the mbuf chain begins a new record.
 */
void
sbappendrecord(struct sockbuf *sb, usn_mbuf_t *m0)
{
   usn_mbuf_t *m; 

   if (m0 == 0)
      return;
   m = sb->sb_mb;
   if (m)
      while (m->queue)
         m = m->queue;
   /*  
    * Put the first mbuf on the queue.
    * Note this permits zero length records.
    */
   sballoc(sb, m0);
   if (m) 
      m->queue = m0; 
   else
      sb->sb_mb = m0; 
   m = m0->next;
   m0->next = 0;
   if (m && (m0->flags & BUF_EOR)) {
      m0->flags &= ~BUF_EOR;
      m->flags |= BUF_EOR;
   }
   sbcompress(sb, m, m0);
}

/*
 * Append mbuf chain m to the last record in the
 * socket buffer sb.  The additional space associated
 * the mbuf chain is recorded in sb.  Empty mbufs are
 * discarded and mbufs are compacted where possible.
 */
void    
sbappend(struct sockbuf *sb, usn_mbuf_t *m)
{
/*
   sb->sb_mb = m;
   sb->sb_cc += m->mlen;
   return;
*/
   usn_mbuf_t *n;
   if (m == 0)
      return;
   n = sb->sb_mb;
   if (n) {
      while (n->queue)
         n = n->queue;
      do {
/*
         if (n->flags & BUF_EOR) {
            DEBUG("end of record");
            sbappendrecord(sb, m); // XXXXXX!!!!
            return;
         }
*/
      } while (n->next && (n = n->next));
   }
   sbcompress(sb, m, n);
   return;
}

/*
 * Free all mbufs in a sockbuf.
 * Check that all resources are reclaimed.
 */
void
sbflush(struct sockbuf *sb)
{

   if (sb->sb_flags & SB_LOCK)
      DEBUG("panic: sbflush");
   while (sb->sb_mbcnt)
      sbdrop(sb, (int)sb->sb_cc);
   if (sb->sb_cc || sb->sb_mb)
      DEBUG("panic: sbflush 2");
}

void
sorflush(struct usn_socket *so)
{
   struct sockbuf *sb = &so->so_rcv;
   //struct protosw *pr = so->so_proto;
   struct sockbuf asb;
   //int s;

   sb->sb_flags |= SB_NOINTR;
   //sblock(sb, M_WAITOK);
   //s = splimp();
   socantrcvmore(so);
   //sbunlock(sb);
   asb = *sb;
   bzero((caddr_t)sb, sizeof (*sb));
   //splx(s);
   // FIXME
   //if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
   //   (*pr->pr_domain->dom_dispose)(asb.sb_mb);
   sbrelease(&asb);
}

int
sofree(struct usn_socket *so) 
{

   if (so->so_pcb || (so->so_state & USN_NOFDREF) == 0)
      return -1;
   if (so->so_head) {
      if (!soqremque(so, 0) && !soqremque(so, 1))
         DEBUG("panic: sofree dq");
      so->so_head = 0; 
   }
   sbrelease(&so->so_snd);
   sorflush(so);
   usn_free_buf((u_char*)so);
   return 0;
}

int32
sodisconnect(struct usn_socket *so)
{
   int error;

   if ((so->so_state & USN_ISCONNECTED) == 0) {
      error = -1;//USN_ENOTCONN;
      goto bad;
   }
   if (so->so_state & USN_ISDISCONNECTING) {
      error = -2;//USN_EALREADY;
      goto bad;
   }
   error = (*so->so_usrreq)(so, PRU_DISCONNECT,
       (usn_mbuf_t *)0, (usn_mbuf_t *)0, (usn_mbuf_t *)0);
bad:
   return (error);
}
/*
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 */
int32
soclose(struct usn_socket *so)
{
   int error = 0;

   if (so->so_options & SO_ACCEPTCONN) {
      while (so->so_q0)
         soabort(so->so_q0);
      while (so->so_q)
         soabort(so->so_q);
   }
   if (so->so_pcb == 0)
      goto discard;
   if (so->so_state & USN_ISCONNECTED) {
      if ((so->so_state & USN_ISDISCONNECTING) == 0) {
         error = sodisconnect(so);
         if (error)
            goto drop;
      }
      if (so->so_options & SO_LINGER) {
         if ((so->so_state & USN_ISDISCONNECTING) &&
             (so->so_state & USN_NBIO))
            goto drop;
         // FIXME: why sleep here?
         //while (so->so_state & USN_ISCONNECTED)
         //   if (error = tsleep((caddr_t)&so->so_timeo,
         //       PSOCK | PCATCH, netcls, so->so_linger * hz))
         //      break;
      }
   }
drop:
   if (so->so_pcb) {
      int error2 =
          (*so->so_usrreq)(so, PRU_DETACH,
         (usn_mbuf_t *)0, (usn_mbuf_t *)0, (usn_mbuf_t *)0);
      if (error == 0)
         error = error2;
   }
discard:
   if (so->so_state & USN_NOFDREF)
      DEBUG("panic: NOFDREF");
   so->so_state |= USN_NOFDREF;
   sofree(so);
   return (error);
}
