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
   // FIXME: 
   //sbflush(sb);
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

