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
udp_init()
{
   DEBUG("udp_init: start");
   g_udb.inp_next = g_udb.inp_prev = &g_udb;
   g_udpcksum = 1;
   g_udp_last_inpcb = &g_udb;
   DEBUG("udp_init: done");
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
   ((usn_ip_t *)ui)->ip_len = htons(sizeof (struct udpiphdr) + len);
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
   usn_free_mbuf(m);
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
   struct ipovly ipov;

   (void)ipov;
   DEBUG("dump info, size_ip=%lu, size_ipovly=%lu, "
         "ih_next=%lu, ih_prev=%lu, ih_x1=%lu, ih_pr=%lu, ih_len=%lu, ih_src=%lu, ih_dst=%lu", 
          sizeof(usn_ip_t), sizeof(ipov), 
          sizeof(ipov.ih_next), sizeof(ipov.ih_prev),
          sizeof(ipov.ih_x1), sizeof(ipov.ih_pr), sizeof(ipov.ih_len),
          sizeof(ipov.ih_src), sizeof(ipov.ih_dst));
   
   g_udpstat.udps_ipackets++;
   dump_buffer((char*)m->head, m->mlen, "udp_"); 
   /* 
    * Strip IP options, if any; should skip this,
    * make available to user, and use on returned packets,
    * but we don't yet have a way to check the checksum
    * with options still present.
    * TODO implement it
    */
   if (iphlen > sizeof(usn_ip_t)) {
      DEBUG("strip ip options");
      ip_stripoptions(m, (usn_mbuf_t *)0);
      iphlen = sizeof(usn_ip_t);
   }

   dump_buffer((char*)m->head, m->mlen, "opts"); 

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
         NDEBUG("checksum failed");
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
      u_int32 ipaddr;
      u_short port;
      port = uh->uh_sport;
      uh->uh_sport = uh->uh_dport;
      uh->uh_dport = port;
      *ip = save_ip;
      ipaddr = ip->ip_src.s_addr;
      ip->ip_src.s_addr = ip->ip_dst.s_addr;
      ip->ip_dst.s_addr = ipaddr;
      ip->ip_len = ntohs(ntohs(ip->ip_len) + sizeof(*ip));
      dump_buffer((char*)m->head, m->mlen, "test");
      goto test;

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

   // XXX callbacks

   //if (sbappendaddr(&inp->inp_socket->so_rcv, (struct sockaddr *)&g_udp_in,
   //    m, opts) == 0) {
   //   g_udpstat.udps_fullsock++;
   //   goto bad;
   //}

   //sorwakeup(inp->inp_socket);

test:
   m->refs++;
   DEBUG("increase refs, ptr=%p, refs=%d, pkt_size=%d", m, m->refs, m->mlen);
   ipv4_output(m, 0, 0, IP_ROUTETOIF);

//#define TEST_NETMAP
#ifdef TEST_NETMAP
   test_netmap(m);
#endif

#define TEST_UDP
#ifdef TEST_UDP
//dotest: 
   int i;
   struct timeval stime, etime, dtime;
   static int cnt = 0;
   static int g_cnt = 0;
   
   if ( cnt == 0 ) {
      cnt++;
      return;
   }
   cnt=0;
   gettimeofday(&stime,0);
   for (i=0; i < 100; i++) {
      //DEBUG("send %i_packet", i);
      //dump_buffer((char*)m->head, m->mlen, "ipv4");
      m->refs++;
      m->head += sizeof(ether_header_t);
      m->mlen -= sizeof(ether_header_t);
      if ( ipv4_output(m, 0, 0, IP_ROUTETOIF) ) cnt++;
      //if ( send_mbuf(m) ) cnt++;
   }
   gettimeofday(&etime,0);
   g_cnt += cnt;
   timersub(&etime, &stime, &dtime);
   printf("num of sent udp packets by netmap-base stack: %d \n", i);
   printf("total time: %lu seconds %lu microseconds, cnt=%d \n", 
           dtime.tv_sec, dtime.tv_usec, cnt);

   netmap_flush();
   sleep(1);
   //goto dotest;
#endif

   return;
bad:
   DEBUG("bad packet");
   if (m) 
      usn_free_mbuf(m);
   if (opts)
      usn_free_mbuf(opts);
   return;
}




