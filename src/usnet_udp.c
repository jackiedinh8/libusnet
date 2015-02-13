/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
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
 * @(#)udp_usrreq.c  8.6 (Berkeley) 5/23/95
 */

#include <netinet/in.h>

#include "usnet_udp.h"
#include "usnet_ip_out.h"
#include "usnet_ip_icmp.h"
#include "usnet_socket.h"
#include "usnet_eth.h"
#include "usnet_arp.h"

int             g_udpcksum;
struct inpcb    g_udb;
struct udpstat  g_udpstat;
struct inpcb   *g_udp_last_inpcb;

struct usn_sockaddr_in g_udp_in = { sizeof(g_udp_in), AF_INET };

void 
usnet_udp_init()
{
   DEBUG("udp_init: start");
   g_udb.inp_next = g_udb.inp_prev = &g_udb;
   g_udpcksum = 1;
   g_udp_last_inpcb = &g_udb;
   DEBUG("udp_init: done");
}


void
udp_notify(struct inpcb *inp, int error)
{
   inp->inp_socket->so_error = errno; 
   // FIXME: callbacks
   //sorwakeup(inp->inp_socket);
   //sowwakeup(inp->inp_socket);
   return;
}
void                
udp_ctlinput(int cmd, struct usn_sockaddr *sa, usn_ip_t *ip)
{
   usn_udphdr_t *uh;
   //extern struct in_addr zeroin_addr;
   //extern u_char inetctlerrmap[];

   (void)uh;

   if (!PRC_IS_REDIRECT(cmd) &&
       ((unsigned)cmd >= PRC_NCMDS || g_inetctlerrmap[cmd] == 0))
           return;
   if (ip) {
      uh = (usn_udphdr_t *)((caddr_t)ip + (ip->ip_hl << 2));
      in_pcbnotify(&g_udb, sa, uh->uh_dport, ip->ip_src, uh->uh_sport, cmd, udp_notify);
   } else
      in_pcbnotify(&g_udb, sa, 0, g_zeroin_addr, 0, cmd, udp_notify);
}
 
int
udp_output(struct inpcb *inp, usn_mbuf_t *m, usn_mbuf_t *addr, usn_mbuf_t  *control)
{
   struct udpiphdr *ui;
   int len = m->mlen;
   struct usn_in_addr laddr;
   int error = 0;

   if (control)
      usn_free_mbuf(control);    /* XXX */

   if (addr) {
      laddr = inp->inp_laddr;
      if (inp->inp_faddr.s_addr != USN_INADDR_ANY) {
         error = EISCONN;
         goto release;
      }  
      /*  
       * Must block input while temporarily connected.
       */
      //s = splnet();
      error = in_pcbconnect(inp, addr);
      if (error) {
         //splx(s);
         goto release;
      }
   } else {
      if (inp->inp_faddr.s_addr == USN_INADDR_ANY) {
         error = ENOTCONN;
         goto release;
      }
   }
   /*
    * Calculate data length and get a mbuf
    * for UDP and IP headers.
    * TODO: allocate new mem if necessary.
    */
   BUF_PREPEND(m, sizeof(struct udpiphdr));
   if (m == 0) {
      DEBUG("cound not prepend buffer");
      error = ENOBUFS;
      goto release;
   }

   /*
    * Fill in mbuf with extended UDP header
    * and addresses and length put into network format.
    */
   ui = mtod(m, struct udpiphdr *);
   ui->ui_next = 0;
   ui->ui_prev = 0;
   ui->ui_x1 = 0;
   ui->ui_pr = IPPROTO_UDP;
   ui->ui_len = htons((u_short)len + sizeof (usn_udphdr_t));
   ui->ui_src = inp->inp_laddr;
   ui->ui_dst = inp->inp_faddr;
   ui->ui_sport = inp->inp_lport;
   ui->ui_dport = inp->inp_fport;
   ui->ui_ulen = ui->ui_len;

   /*
    * Stuff checksum and output datagram.
    */
   ui->ui_sum = 0;
   if (g_udpcksum) {
       if ((ui->ui_sum = in_cksum(m, sizeof (struct udpiphdr) + len)) == 0)
      ui->ui_sum = 0xffff;
   }
   //((usn_ip_t *)ui)->ip_len = htons(sizeof (struct udpiphdr) + len);
   ((usn_ip_t *)ui)->ip_len = (sizeof (struct udpiphdr) + len);
   ((usn_ip_t *)ui)->ip_ttl = inp->inp_ip.ip_ttl; /* XXX */
   ((usn_ip_t *)ui)->ip_tos = inp->inp_ip.ip_tos; /* XXX */

   g_udpstat.udps_opackets++;
   error = ipv4_output(m, inp->inp_options, &inp->inp_route,
       inp->inp_socket->so_options & (SO_DONTROUTE | SO_BROADCAST));

   if (addr) {
      in_pcbdisconnect(inp);
      inp->inp_laddr = laddr;
      //splx(s);
   }
   return (error);

release:
   MFREE(m);
   return (error);
} 

/*
 * Create a "control" mbuf containing the specified data
 * with the specified type for presentation with a datagram.
 */
usn_mbuf_t *
udp_saveopt( caddr_t p, int size, int type)
{
   struct cmsghdr *cp;
   usn_mbuf_t *m;

   if ((m = usn_get_mbuf(0, BUF_MSIZE, 0)) == NULL)
      return ((usn_mbuf_t *) NULL);
   cp = (struct cmsghdr *) mtod(m, struct cmsghdr *);
   bcopy(p, CMSG_DATA(cp), size);
   size += sizeof(*cp);
   m->mlen = size;
   m->flags = BUF_CTLMSG;
   cp->cmsg_len = size;
   cp->cmsg_level = IPPROTO_IP;
   cp->cmsg_type = type;
   return (m);
}


void
udp_input(usn_mbuf_t *m, u_int iphlen)
{
   usn_ip_t *ip;
   usn_udphdr_t *uh;
   struct inpcb *inp;
   usn_mbuf_t *opts = 0;
   int len;
   usn_ip_t save_ip;

   g_udpstat.udps_ipackets++;
   /* 
    * Strip IP options, if any; should skip this,
    * make available to user, and use on returned packets,
    * but we don't yet have a way to check the checksum
    * with options still present.
    */
   if (iphlen > sizeof(usn_ip_t)) {
      DEBUG("strip ip options");
      ip_stripoptions(m, (usn_mbuf_t *)0);
      iphlen = sizeof(usn_ip_t);
   }

   /*
    * Get IP and UDP header together in first mbuf.
    */
   ip = mtod(m, usn_ip_t *);
   if (m->mlen < iphlen + sizeof(usn_udphdr_t)) {
      if ((m = m_pullup(m, iphlen + sizeof(usn_udphdr_t))) == 0) {
         g_udpstat.udps_hdrops++;
         return;
      }
      ip = mtod(m, usn_ip_t *);
   }
   uh = (usn_udphdr_t *)((caddr_t)ip + iphlen);

   /*
    * Make mbuf data length reflect UDP length.
    * If not enough data to reflect UDP length, drop.
    * Recall that ipv4_input substract hlen from ip_len.
    */
   len = ntohs((u_short)uh->uh_ulen);
   if (ntohs(ip->ip_len) != len) {
      if (len > ip->ip_len) {
         g_udpstat.udps_badlen++;
         goto bad;
      }
      m_adj(m, len - ip->ip_len);
      /* ip->ip_len = len; */
   }
   /*
    * Save a copy of the IP header in case we want restore it
    * for sending an ICMP error message in response.
    */
   save_ip = *ip;

   /*
    * Checksum extended UDP header and data.
    */
   if (uh->uh_sum) {
      ((struct ipovly *)ip)->ih_next = 0;
      ((struct ipovly *)ip)->ih_prev = 0;
      ((struct ipovly *)ip)->ih_x1 = 0;
      ((struct ipovly *)ip)->ih_len = uh->uh_ulen;
      if ( (uh->uh_sum = in_cksum(m, len + sizeof (usn_ip_t))) ) {
         DEBUG("checksum failed");
         g_udpstat.udps_badsum++;
         usn_free_mbuf(m);
         return;
      }
   }
   // TODO Handle multicast part.

   /*
    * Locate pcb for datagram.
    */
   inp = g_udp_last_inpcb;
   if (inp->inp_lport != uh->uh_dport ||
       inp->inp_fport != uh->uh_sport ||
       inp->inp_faddr.s_addr != ip->ip_src.s_addr ||
       inp->inp_laddr.s_addr != ip->ip_dst.s_addr) {
      // TODO: improve lookup
      inp = in_pcblookup(&g_udb, ip->ip_src, uh->uh_sport,
          ip->ip_dst, uh->uh_dport, INPLOOKUP_WILDCARD);
      if (inp)
         g_udp_last_inpcb = inp;
      g_udpstat.udpps_pcbcachemiss++;
   }

   if (inp == 0) {
      g_udpstat.udps_noport++;
      if (m->flags & (BUF_BCAST | BUF_MCAST)) {
         g_udpstat.udps_noportbcast++;
         goto bad;
      }
      *ip = save_ip;
      ip->ip_len += iphlen;
      icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0, 0);
      return;
   }

   /*
    * Construct sockaddr format source address.
    * Stuff source address and datagram in user buffer.
    */
   g_udp_in.sin_port = uh->uh_sport;
   g_udp_in.sin_addr = ip->ip_src;
   DEBUG("g_upd_in: ip=%x, port=%d", 
          g_udp_in.sin_addr.s_addr, g_udp_in.sin_port);
   if (inp->inp_flags & INP_CONTROLOPTS) {
      usn_mbuf_t **mp = &opts;

      if (inp->inp_flags & INP_RECVDSTADDR) {
         *mp = udp_saveopt((caddr_t) &ip->ip_dst,
             sizeof(struct usn_in_addr), IP_RECVDSTADDR);
         if (*mp)
            mp = &(*mp)->next;
      }
#ifdef notyet
      /* options were tossed above */
      if (inp->inp_flags & INP_RECVOPTS) {
         *mp = udp_saveopt((caddr_t) opts_deleted_above,
             sizeof(struct in_addr), IP_RECVOPTS);
         if (*mp)
            mp = &(*mp)->next;
      }
      /* ip_srcroute doesn't do what we want here, need to fix */
      if (inp->inp_flags & INP_RECVRETOPTS) {
         *mp = udp_saveopt((caddr_t) ip_srcroute(),
             sizeof(struct in_addr), IP_RECVRETOPTS);
         if (*mp)
            mp = &(*mp)->next;
      }
#endif
   }
   iphlen += sizeof(usn_udphdr_t);
   m->mlen -= iphlen;
   m->head += iphlen;
  
   // insert msg into queue. 
   m->flags |= BUF_DATA;
   if (sbappendaddr(&inp->inp_socket->so_rcv, 
          (struct usn_sockaddr *)&g_udp_in, m, opts) == 0) {
      g_udpstat.udps_fullsock++;
      goto bad;
   }
   // callbacks
   usnet_udpwakeup_socket(inp);

   return;

bad:
   MFREE(m);
   MFREE(opts);
   return;
}




