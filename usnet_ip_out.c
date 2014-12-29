#include <netinet/in.h> 

#include "usnet_ip_out.h"
#include "usnet_ip.h"
#include "usnet_route.h"
#include "usnet_if.h"
#include "usnet_eth.h"


/*
 * Insert IP options into preformed packet.
 * Adjust IP destination as required for IP source routing,
 * as indicated by a non-zero in_addr at the start of the options.
 */
static usn_mbuf_t *
ip_insertoptions( usn_mbuf_t *m, usn_mbuf_t *opt, int *phlen)
{
   struct ipoption *p = mtod(opt, struct ipoption *);
   //usn_mbuf_t *n;
   usn_ip_t   *ip = mtod(m, usn_ip_t *);
   unsigned optlen;

   optlen = opt->mlen - sizeof(p->ipopt_dst);
   if (optlen + (u_short)ip->ip_len > IP_MAXPACKET)
      return (m);    /* XXX should fail */

   if (p->ipopt_dst.s_addr)
      ip->ip_dst = p->ipopt_dst;
/*
   if (m->flags & M_EXT || m->m_data - optlen < m->m_pktdat) {
      MGETHDR(n, M_DONTWAIT, MT_HEADER);
      if (n == 0)
         return (m);
      n->m_pkthdr.len = m->m_pkthdr.len + optlen;
      m->m_len -= sizeof(struct ip);
      m->m_data += sizeof(struct ip);
      n->m_next = m;
      m = n;
      m->m_len = optlen + sizeof(struct ip);
      m->m_data += max_linkhdr;
      bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
   } else {
      m->m_data -= optlen;
      m->m_len += optlen;
      m->m_pkthdr.len += optlen;
      ovbcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(struct ip));
   }
*/
   ip = mtod(m, usn_ip_t *);
   bcopy((caddr_t)p->ipopt_list, (caddr_t)(ip + 1), (unsigned)optlen);
   *phlen = sizeof(usn_ip_t) + optlen;
   ip->ip_len += optlen;
   return (m);
}

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int
ipv4_output(usn_mbuf_t *m0, usn_mbuf_t *opt, struct route *ro, int flags)
{
   usn_ip_t *ip, *mhip;
   struct ifnet *ifp;
   usn_mbuf_t *m = m0;
   int hlen = sizeof (usn_ip_t); 
   int len, off, error = 0; 
   struct route iproute;
   struct usn_sockaddr_in *dst;
   struct in_ifaddr *ia; 
  
#ifdef DUMP_PAYLOAD 
	DEBUG("ipv4_output: dump info: ptr=%p, len=%d\n", m->head, m->mlen);
   dump_buffer((char*)m->head, m->mlen, "ip4");
#endif

   if ( m == NULL ) {
      ERROR("m is NULL");
      return -1;
   }

   if (opt) {
      DEBUG("insert ip options");
      m = ip_insertoptions(m, opt, &len);
      hlen = len; 
   }

   ip = mtod(m, usn_ip_t *);
   /*   
    * Fill in IP header.
    */
   if ((flags & (IP_FORWARDING|IP_RAWOUTPUT)) == 0) { 
      DEBUG("fill ip fields");
      ip->ip_v = IPVERSION;
      ip->ip_off &= IP_DF;
      ip->ip_id = htons(g_ip_id++);
      ip->ip_hl = hlen >> 2;
      //g_ipstat.ips_localout++;
   } else {
      hlen = ip->ip_hl << 2;
   }
   /*
    * Route packet.
    */
   if (ro == 0) {
      DEBUG("use empty route");
      ro = &iproute;
      bzero((caddr_t)ro, sizeof (*ro));
   }
   dst = (struct usn_sockaddr_in *)&ro->ro_dst;
   /*
    * If there is a cached route,
    * check that it is to the same destination
    * and is still up.  If not, free it and try again.
    */
   if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
      dst->sin_addr.s_addr != ip->ip_dst.s_addr)) {
      RTFREE(ro->ro_rt);
      ro->ro_rt = (struct rtentry *)0;
   }
   DEBUG("ptr=%p", m);
   dump_buffer((char*)m->head, m->mlen, "dst1");
   if (ro->ro_rt == 0) {
      DEBUG("set default dst");
      dst->sin_family = AF_INET;
      dst->sin_len = sizeof(*dst);
      dst->sin_addr = ip->ip_dst;
      dump_buffer((char*)dst, sizeof(*dst), "dst2");
   }
   /*
    * If routing to interface only,
    * short circuit routing lookup.
    */
#define ifatoia(ifa) ((struct in_ifaddr *)(ifa))
#define sintosa(sin) ((struct usn_sockaddr *)(sin))
   if (flags & IP_ROUTETOIF) {
      if ((ia = ifatoia(ifa_ifwithdstaddr(sintosa(dst)))) == 0 &&
          (ia = ifatoia(ifa_ifwithnet(sintosa(dst)))) == 0) {
         //ipstat.ips_noroute++;
         DEBUG("not found outgoing interface");
         error = ENETUNREACH;
         goto bad;
      }
      ifp = ia->ia_ifp;
      ip->ip_ttl = 1;
   } else {
      if (ro->ro_rt == 0) {
         DEBUG("looking for a route");
         rtalloc(ro);
      }
      if (ro->ro_rt == 0) {
         //ipstat.ips_noroute++;
         DEBUG("not found route");
         error = EHOSTUNREACH;
         goto bad;
      }
      DEBUG("route found");
      ia = ifatoia(ro->ro_rt->rt_ifa);
      ifp = ro->ro_rt->rt_ifp;
      ro->ro_rt->rt_use++;
      if (ro->ro_rt->rt_flags & RTF_GATEWAY) {
         DEBUG("gateway-route found");
         dst = (struct usn_sockaddr_in *)ro->ro_rt->rt_gateway;
      }
   }
   // TODO Handle multicast part.

   /*
    * If source address not specified yet, use address
    * of outgoing interface.
    */
   DEBUG("set source ip address, addr=%x", ip->ip_src.s_addr);
   if (ip->ip_src.s_addr == USN_INADDR_ANY) {
      ip->ip_src = IA_SIN(ia)->sin_addr;
   }

   /*
    * Look for broadcast address and
    * and verify user is allowed to send
    * such a packet.
    */
   if (in_broadcast(dst->sin_addr, ifp)) {
      if ((ifp->if_flags & IFF_BROADCAST) == 0) {
         DEBUG("broadcast is not available, addr=%x", dst->sin_addr.s_addr);
         error = EADDRNOTAVAIL;
         goto bad;
      }
      if ((flags & IP_ALLOWBROADCAST) == 0) {
         DEBUG("broadcast is not allowed, addr=%x", dst->sin_addr.s_addr);
         error = EACCES;
         goto bad;
      }
      /* don't allow broadcast messages to be fragmented */
      if ((u_short)ip->ip_len > ifp->if_mtu) {
         DEBUG("broadcast can not be fragmented, addr=%x", dst->sin_addr.s_addr);
         error = EMSGSIZE;
         goto bad;
      }
      m->flags |= BUF_BCAST;
   } else
      m->flags &= ~BUF_BCAST;

   /*
    * If small enough for interface, can just send directly.
    */
#ifdef DUMP_PAYLOAD
   dump_buffer((char*)m->head, m->mlen, "ip4");
#endif

   DEBUG("check mtu, ip_len=%d, mtu=%d", htons((u_short)ip->ip_len), ifp->if_mtu);
   if (htons((u_short)ip->ip_len) <= ifp->if_mtu) {
      //ip->ip_len = htons((u_short)ip->ip_len);
      //ip->ip_off = htons((u_short)ip->ip_off);
      ip->ip_sum = 0;
      ip->ip_sum = in_cksum(m, hlen);

#ifdef DUMP_PAYLOAD
      DEBUG("send packet out, ip_len=%d, ip_off=%d, ip_sum=%d", 
             ip->ip_len, ip->ip_off, ip->ip_sum);
      dump_buffer((char*)dst, *((char*)dst),"ip4");
#endif

      error = eth_output( m, (struct usn_sockaddr *)dst, ro->ro_rt);
      goto done;
   }
   /*
    * Too large for interface; fragment if possible.
    * Must be able to put at least 8 bytes per fragment.
    */
   if (ip->ip_off & IP_DF) {
      DEBUG("bad fragment, ip_off=%d", ip->ip_off);
      error = EMSGSIZE;
      //ipstat.ips_cantfrag++;
      goto bad;
   }
   len = (ifp->if_mtu - hlen) &~ 7;
   if (len < 8) {
      DEBUG("too small, len=%d", len);
      error = EMSGSIZE;
      goto bad;
   }

 {
   int mhlen, firstlen = len;
   usn_mbuf_t **mnext = &m->next;

   /*
    * Loop through length of segment after first fragment,
    * make new header and copy data of each part and link onto chain.
    */
   DEBUG("send fragments, ip_len=%d, mtu=%d", ip->ip_len, ifp->if_mtu );
   m0 = m;
   mhlen = sizeof (usn_ip_t);
   for (off = hlen + len; off < (u_short)ip->ip_len; off += len) {
      // FIXME implement this
      //MGETHDR(m, M_DONTWAIT, MT_HEADER);
      m = 0;
      if (m == 0) {
         error = ENOBUFS;
         //ipstat.ips_odropped++;
         DEBUG("m is null");
         goto sendorfree;
      }
      // FIXME implement this
      //m->m_data += max_linkhdr;
      mhip = mtod(m, usn_ip_t *);
      *mhip = *ip;
      if (hlen > (int)sizeof (usn_ip_t)) {
         // FIXME implement this
         //mhlen = ip_optcopy(ip, mhip) + sizeof (usn_ip_t);
         mhip->ip_hl = mhlen >> 2;
      }
      m->mlen = mhlen;
      mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
      if (ip->ip_off & IP_MF)
         mhip->ip_off |= IP_MF;
      if (off + len >= (u_short)ip->ip_len)
         len = (u_short)ip->ip_len - off;
      else
         mhip->ip_off |= IP_MF;
      mhip->ip_len = htons((u_short)(len + mhlen));
      // FIXME implement this
      //m->next = m_copy(m0, off, len);
      if (m->next == 0) {
         DEBUG("m is released");
         usn_free_mbuf(m);
         error = ENOBUFS;  /* ??? */
         //ipstat.ips_odropped++;
         goto sendorfree;
      }
      //m->m_pkthdr.len = mhlen + len;
      //m->m_pkthdr.rcvif = (struct ifnet *)0;
      mhip->ip_off = htons((u_short)mhip->ip_off);
      mhip->ip_sum = 0;
      mhip->ip_sum = in_cksum(m, mhlen);
      *mnext = m;
      // FIXME implement this
      //mnext = &m->m_nextpkt;
      //ipstat.ips_ofragments++;
   }
   /*
    * Update first fragment by trimming what's been copied out
    * and updating header, then send each fragment (in order).
    */
   m = m0;
   m_adj(m, hlen + firstlen - (u_short)ip->ip_len);

   // FIXME: do we need?
   //ip->ip_len = htons((u_short)m->m_pkthdr.len);

   ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
   ip->ip_sum = 0;
   ip->ip_sum = in_cksum(m, hlen);

sendorfree:
   DEBUG("error=%d, m=%p",error, m);
   return eth_output(m,(struct usn_sockaddr *)dst, ro->ro_rt); 

   //for (m = m0; m; m = m0) {
      //m0 = m->m_nextpkt;
      //m->m_nextpkt = 0;
      //if (error == 0)
      //   error = (*ifp->if_output)(ifp, m,
      //       (struct usn_sockaddr *)dst, ro->ro_rt);
      //else
      //   usn_free_mbuf(m);
   //}
     //if (error == 0)
     // ipstat.ips_fragmented++;
   ;
 }
done:
   if (ro == &iproute && (flags & IP_ROUTETOIF) == 0 && ro->ro_rt) {
      RTFREE(ro->ro_rt);
   }
   return (error);
bad:
   usn_free_mbuf(m0);
   goto done;

   return 0;
}


