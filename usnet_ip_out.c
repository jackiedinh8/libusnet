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
 * @(#)ip_output.c   8.3 (Berkeley) 1/21/94
 */


#include <netinet/in.h> 

#include "usnet_ip_out.h"
#include "usnet_ip.h"
#include "usnet_route.h"
#include "usnet_if.h"
#include "usnet_eth.h"
#include "usnet_in_pcb.h"
#include "usnet_tcp.h"


/*
 * Insert IP options into preformed packet.
 * Adjust IP destination as required for IP source routing,
 * as indicated by a non-zero in_addr at the start of the options.
 */
static usn_mbuf_t *
ip_insertoptions( usn_mbuf_t *m, usn_mbuf_t *opt, int *phlen)
{
   struct ipoption *p = mtod(opt, struct ipoption *);
   usn_mbuf_t *n;
   usn_ip_t   *ip = mtod(m, usn_ip_t *);
   unsigned optlen;

   optlen = opt->mlen - sizeof(p->ipopt_dst);
   if (optlen + (u_short)ip->ip_len > IP_MAXPACKET)
      return (m);    /* XXX should fail */

   if (p->ipopt_dst.s_addr)
      ip->ip_dst = p->ipopt_dst;

   if (m->head - optlen < m->start) {
      n = usn_get_mbuf(0, MSIZE, 0);
      if (n == 0)
         return (m);
      m->mlen -= sizeof(usn_ip_t);
      m->head += sizeof(usn_ip_t);
      n->next = m;
      m = n;
      m->mlen = optlen + sizeof(usn_ip_t);
      m->head += g_max_linkhdr;
      bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(usn_ip_t));
   } else {
      m->head -= optlen;
      m->mlen += optlen;
      bcopy((caddr_t)ip, mtod(m, caddr_t), sizeof(usn_ip_t));
   }

   ip = mtod(m, usn_ip_t *);
   bcopy((caddr_t)p->ipopt_list, (caddr_t)(ip + 1), (unsigned)optlen);
   *phlen = sizeof(usn_ip_t) + optlen;
   // XXX: convert to host order.
   ip->ip_len += optlen;
   return (m);
}

/*
 * Copy options from ip to jp,
 * omitting those not copied during fragmentation.
 */   
int
ip_optcopy(usn_ip_t *ip, usn_ip_t *jp)
{     
   u_char *cp, *dp;
   int opt, optlen, cnt;
      
   cp = (u_char *)(ip + 1);
   dp = (u_char *)(jp + 1);
   cnt = (ip->ip_hl << 2) - sizeof (usn_ip_t);
   for (; cnt > 0; cnt -= optlen, cp += optlen) {
      opt = cp[0];
      if (opt == IPOPT_EOL)
         break;
      if (opt == IPOPT_NOP) {
         /* Preserve for IP mcast tunnel's LSRR alignment. */
         *dp++ = IPOPT_NOP;
         optlen = 1;
         continue;
      } else
         optlen = cp[IPOPT_OLEN];
      /* bogus lengths should have been caught by ip_dooptions */
      if (optlen > cnt)
         optlen = cnt;
      if (IPOPT_COPIED(opt)) {
         bcopy((caddr_t)cp, (caddr_t)dp, (unsigned)optlen);
         dp += optlen;
      }
   }
   for (optlen = dp - (u_char *)(jp+1); optlen & 0x3; optlen++)
      *dp++ = IPOPT_EOL;
   return (optlen);
}

/*
 * IP output. The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int32
ipv4_output(usn_mbuf_t *m0, usn_mbuf_t *opt, struct route *ro, int flags)
{
   usn_ip_t *ip, *mhip;
   struct ifnet *ifp;
   usn_mbuf_t *m = m0;
   int hlen = sizeof (usn_ip_t); 
   int len, off;
   struct route iproute;
   struct usn_sockaddr_in *dst;
   struct in_ifaddr *ia; 
   int32 error = -1;
  
#ifdef DUMP_PAYLOAD 
	DEBUG("ipv4_output: dump info: ptr=%p, len=%d\n", m, m->mlen);
   dump_buffer((char*)m->head, m->mlen, "ip4");
#endif

   if ( m == NULL ) {
      DEBUG("m is NULL");
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

#ifdef DUMP_PAYLOAD
   DEBUG("ptr=%p", m);
   dump_buffer((char*)m->head, m->mlen, "dst1");
#endif

   if (ro->ro_rt == 0) {
      DEBUG("set default dst");
      dst->sin_family = AF_INET;
      dst->sin_len = sizeof(*dst);
      dst->sin_addr = ip->ip_dst;

#ifdef DUMP_PAYLOAD
   DEBUG("ptr=%p", m);
   dump_buffer((char*)dst, sizeof(*dst), "dst2");
#endif

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
         error = -ENETUNREACH;
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
         error = -EHOSTUNREACH;
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
   if (ip->ip_src.s_addr == USN_INADDR_ANY) {
      ip->ip_src = IA_SIN(ia)->sin_addr;
      DEBUG("set source ip address, addr=%x", ip->ip_src.s_addr);
   }

   /*
    * Look for broadcast address and
    * and verify user is allowed to send
    * such a packet.
    */
   if (in_broadcast(dst->sin_addr, ifp)) {
      if ((ifp->if_flags & IFF_BROADCAST) == 0) {
         DEBUG("broadcast is not available, addr=%x", dst->sin_addr.s_addr);
         error = -EADDRNOTAVAIL;
         goto bad;
      }
      if ((flags & IP_ALLOWBROADCAST) == 0) {
         DEBUG("broadcast is not allowed, addr=%x", dst->sin_addr.s_addr);
         error = -EACCES;
         goto bad;
      }
      /* don't allow broadcast messages to be fragmented */
      if ((u_short)ip->ip_len > ifp->if_mtu) {
         DEBUG("broadcast can not be fragmented, addr=%x", dst->sin_addr.s_addr);
         error = -EMSGSIZE;
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
   DEBUG("check mtu, ip_len=%d(%d), mtu=%d", ip->ip_len, htons((u_short)ip->ip_len), ifp->if_mtu);
   if (ip->ip_len <= ifp->if_mtu) {
      ip->ip_len = htons((u_short)ip->ip_len);
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
      error = -EMSGSIZE;
      //ipstat.ips_cantfrag++;
      goto bad;
   }
   len = (ifp->if_mtu - hlen) &~ 7;
   if (len < 8) {
      DEBUG("too small, len=%d", len);
      error = -EMSGSIZE;
      goto bad;
   }

 {
   int mhlen, firstlen = len;
   usn_mbuf_t **mnext = &m->queue;

   /*
    * Loop through length of segment after first fragment,
    * make new header and copy data of each part and link onto chain.
    */
   DEBUG("send fragments, ip_len=%d, mtu=%d", ip->ip_len, ifp->if_mtu );
   m0 = m;
   mhlen = sizeof (usn_ip_t);
   for (off = hlen + len; off < (u_short)ip->ip_len; off += len) {
      m = usn_get_mbuf(0, MSIZE, 0);
      if (m == 0) {
         error = -ENOBUFS;
         //ipstat.ips_odropped++;
         DEBUG("m is null");
         goto sendorfree;
      }
      m->head += g_max_linkhdr;
      m->mlen -= g_max_linkhdr;
      mhip = mtod(m, usn_ip_t *);
      *mhip = *ip;
      if (hlen > (int)sizeof (usn_ip_t)) {
         mhlen = ip_optcopy(ip, mhip) + sizeof (usn_ip_t);
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
      m->next = usn_copy_data(m0, off, len);
      if (m->next == 0) {
         MFREE(m);
         error = -ENOBUFS;  /* ??? */
         //ipstat.ips_odropped++;
         goto sendorfree;
      }
      //m->m_pkthdr.len = mhlen + len;
      //m->m_pkthdr.rcvif = (struct ifnet *)0;
      mhip->ip_off = htons((u_short)mhip->ip_off);
      mhip->ip_sum = 0;
      mhip->ip_sum = in_cksum(m, mhlen);
      *mnext = m;
      mnext = &m->queue;
      //ipstat.ips_ofragments++;
   }
   /*
    * Update first fragment by trimming what's been copied out
    * and updating header, then send each fragment (in order).
    */
   m = m0;
   m_adj(m, hlen + firstlen - (u_short)ip->ip_len);
   ip->ip_len = htons((u_short)usn_get_mbuflen(m));

   ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
   ip->ip_sum = 0;
   ip->ip_sum = in_cksum(m, hlen);

sendorfree:
   DEBUG("error=%d, m=%p",error, m);
   //return eth_output(m,(struct usn_sockaddr *)dst, ro->ro_rt); 

   for (m = m0; m; m = m0) {
      m0 = m->queue;
      m->queue = 0;
      if (error == 0)
         error = eth_output(m,(struct usn_sockaddr *)dst, ro->ro_rt); 
      else
         MFREEQ(m);
   }
   if (error == 0)
      ;//g_ipstat.ips_fragmented++;
 }
done:
   if (ro == &iproute && (flags & IP_ROUTETOIF) == 0 && ro->ro_rt) {
      RTFREE(ro->ro_rt);
   }
   return (error);
bad:
   MFREE(m0);
   goto done;
   return 0;
}
/*
 * Return the IP multicast options in response to user getsockopt().
 */
int
//ip_getmoptions( int optname, struct ip_moptions *imo, usn_mbuf_t **mp)
ip_getmoptions( int optname, void *imo, usn_mbuf_t **mp)
{
   return 0;
/*
   u_char *ttl;
   u_char *loop;
   struct in_addr *addr;
   struct in_ifaddr *ia;

   *mp = m_get(M_WAIT, MT_SOOPTS);

   switch (optname) {

   case IP_MULTICAST_IF:
      addr = mtod(*mp, struct in_addr *);
      (*mp)->m_len = sizeof(struct in_addr);
      if (imo == NULL || imo->imo_multicast_ifp == NULL)
         addr->s_addr = INADDR_ANY;
      else {
         IFP_TO_IA(imo->imo_multicast_ifp, ia);
         addr->s_addr = (ia == NULL) ? INADDR_ANY
               : IA_SIN(ia)->sin_addr.s_addr;
      }
      return (0);

   case IP_MULTICAST_TTL:
      ttl = mtod(*mp, u_char *);
      (*mp)->m_len = 1;
      *ttl = (imo == NULL) ? IP_DEFAULT_MULTICAST_TTL
                 : imo->imo_multicast_ttl;
      return (0);
   case IP_MULTICAST_LOOP:
      loop = mtod(*mp, u_char *);
      (*mp)->m_len = 1;
      *loop = (imo == NULL) ? IP_DEFAULT_MULTICAST_LOOP
                  : imo->imo_multicast_loop;
      return (0); 
      
   default:
      return (EOPNOTSUPP);
   }  
*/
}  
/*
 * Set the IP multicast options in response to user setsockopt().
 */
int
//ip_setmoptions( int optname, struct ip_moptions **imop, usn_mbuf_t *m)
ip_setmoptions( int optname, void **imop, usn_mbuf_t *m)
{
   return 0;
/*
   register int error = 0;
   u_char loop;
   register int i;
   struct in_addr addr;
   register struct ip_mreq *mreq;
   register struct ifnet *ifp;
   register struct ip_moptions *imo = *imop;
   struct route ro;
   register struct sockaddr_in *dst;
         
   if (imo == NULL) {
      // No multicast option buffer attached to the pcb;
      // allocate one and initialize to default values.
      imo = (struct ip_moptions*)malloc(sizeof(*imo), M_IPMOPTS,
          M_WAITOK);

      if (imo == NULL)
         return (ENOBUFS);
      *imop = imo;
      imo->imo_multicast_ifp = NULL;
      imo->imo_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
      imo->imo_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
      imo->imo_num_memberships = 0;
   }
   switch (optname) {

   case IP_MULTICAST_IF:

      // Select the interface for outgoing multicast packets.
      if (m == NULL || m->m_len != sizeof(struct in_addr)) {
         error = EINVAL;
         break;
      }
      addr = *(mtod(m, struct in_addr *));

      // INADDR_ANY is used to remove a previous selection.
      // When no interface is selected, a default one is
      // chosen every time a multicast packet is sent.
      if (addr.s_addr == INADDR_ANY) {
         imo->imo_multicast_ifp = NULL;
         break;
      }

      // The selected interface is identified by its local
      // IP address.  Find the interface and confirm that
      // it supports multicasting.
      INADDR_TO_IFP(addr, ifp);
      if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
         error = EADDRNOTAVAIL;
         break;
      }
      imo->imo_multicast_ifp = ifp;
      break;

   case IP_MULTICAST_TTL:

      // Set the IP time-to-live for outgoing multicast packets.
      if (m == NULL || m->m_len != 1) {
         error = EINVAL;
         break;
      }
      imo->imo_multicast_ttl = *(mtod(m, u_char *));
      break;

   case IP_MULTICAST_LOOP:

      // Set the loopback flag for outgoing multicast packets.
      // Must be zero or one.
      if (m == NULL || m->m_len != 1 ||
         (loop = *(mtod(m, u_char *))) > 1) {
         error = EINVAL;
         break;
      }
      imo->imo_multicast_loop = loop;
      break;

   case IP_ADD_MEMBERSHIP:

      // Add a multicast group membership.
      // Group must be a valid IP multicast address.
      if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
         error = EINVAL;
         break;
      }
      mreq = mtod(m, struct ip_mreq *);
      if (!IN_MULTICAST(ntohl(mreq->imr_multiaddr.s_addr))) {
         error = EINVAL;
         break;
      }

      // If no interface address was provided, use the interface of
      // the route to the given multicast address.
      if (mreq->imr_interface.s_addr == INADDR_ANY) {
         ro.ro_rt = NULL;
         dst = (struct sockaddr_in *)&ro.ro_dst;
         dst->sin_len = sizeof(*dst);
         dst->sin_family = AF_INET;
         dst->sin_addr = mreq->imr_multiaddr;
         rtalloc(&ro);
         if (ro.ro_rt == NULL) {
            error = EADDRNOTAVAIL;
            break;
         }
         ifp = ro.ro_rt->rt_ifp;
         rtfree(ro.ro_rt);
      }
      else {
         INADDR_TO_IFP(mreq->imr_interface, ifp);
      }

      // See if we found an interface, and confirm that it
      // supports multicast.
      if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
         error = EADDRNOTAVAIL;
         break;
      }

      // See if the membership already exists or if all the
      // membership slots are full.
      for (i = 0; i < imo->imo_num_memberships; ++i) {
         if (imo->imo_membership[i]->inm_ifp == ifp &&
             imo->imo_membership[i]->inm_addr.s_addr
                  == mreq->imr_multiaddr.s_addr)
            break;
      }
      if (i < imo->imo_num_memberships) {
         error = EADDRINUSE;
         break;
      }
      if (i == IP_MAX_MEMBERSHIPS) {
         error = ETOOMANYREFS;
         break;
      }

      // Everything looks good; add a new record to the multicast
      // address list for the given interface.
      if ((imo->imo_membership[i] =
          in_addmulti(&mreq->imr_multiaddr, ifp)) == NULL) {
         error = ENOBUFS;
         break;
      }
      ++imo->imo_num_memberships;
      break;

   case IP_DROP_MEMBERSHIP:

      // Drop a multicast group membership.
      // Group must be a valid IP multicast address.
      if (m == NULL || m->m_len != sizeof(struct ip_mreq)) {
         error = EINVAL;
         break;
      }
      mreq = mtod(m, struct ip_mreq *);
      if (!IN_MULTICAST(ntohl(mreq->imr_multiaddr.s_addr))) {
         error = EINVAL;
         break;
      }

      // If an interface address was specified, get a pointer
      // to its ifnet structure.
      if (mreq->imr_interface.s_addr == INADDR_ANY)
         ifp = NULL;
      else {
         INADDR_TO_IFP(mreq->imr_interface, ifp);
         if (ifp == NULL) {
            error = EADDRNOTAVAIL;
            break;
         }
      }

      // Find the membership in the membership array.
      for (i = 0; i < imo->imo_num_memberships; ++i) {
         if ((ifp == NULL ||
              imo->imo_membership[i]->inm_ifp == ifp) &&
              imo->imo_membership[i]->inm_addr.s_addr ==
              mreq->imr_multiaddr.s_addr)
            break;
      }
      if (i == imo->imo_num_memberships) {
         error = EADDRNOTAVAIL;
         break;
      }
      // Give up the multicast address record to which the
      // membership points.
      in_delmulti(imo->imo_membership[i]);
      // Remove the gap in the membership array.
      for (++i; i < imo->imo_num_memberships; ++i)
         imo->imo_membership[i-1] = imo->imo_membership[i];
      --imo->imo_num_memberships;
      break;

   default:
      error = EOPNOTSUPP;
      break;
   }


   // If all options have default values, no need to keep the mbuf.
   if (imo->imo_multicast_ifp == NULL &&
       imo->imo_multicast_ttl == IP_DEFAULT_MULTICAST_TTL &&
       imo->imo_multicast_loop == IP_DEFAULT_MULTICAST_LOOP &&
       imo->imo_num_memberships == 0) {
      free(*imop, M_IPMOPTS);
      *imop = NULL;
   }

   return (error);
*/
}

/*
 * Set up IP options in pcb for insertion in output packets.
 * Store in mbuf with pointer in pcbopt, adding pseudo-option
 * with destination address if source routed.
 */
int
ip_pcbopts(int optname, usn_mbuf_t **pcbopt, usn_mbuf_t *m)
{
   return 0;
/*
   u_int cnt, optlen;
   u_char *cp;
   u_char opt;

   // turn off any old options
   if (*pcbopt)
      usn_free_mbuf(*pcbopt);
   *pcbopt = 0;
   if (m == (usn_mbuf_t *)0 || m->mlen == 0) {
      
      // Only turning off any previous options.
      if (m)
         usn_free_mbuf(m);
      return (0);
   }

   if (m->mlen % sizeof(u_long))
      goto bad;

   // IP first-hop destination address will be stored before
   // actual options; move other options back
   // and clear it when none present.
   // FIXME: no field: m_dat 
   //if (m->head + m->mlen + sizeof(struct in_addr) >= &m->m_dat[MLEN])
   //   goto bad;
   cnt = m->mlen;
   m->mlen += sizeof(struct in_addr);
   cp = mtod(m, u_char *) + sizeof(struct in_addr);
   // FIXME: implement it.
   //ovbcopy(mtod(m, caddr_t), (caddr_t)cp, (unsigned)cnt);
   bzero(mtod(m, caddr_t), sizeof(struct in_addr));

   for (; cnt > 0; cnt -= optlen, cp += optlen) {
      opt = cp[IPOPT_OPTVAL];
      if (opt == IPOPT_EOL)
         break;
      if (opt == IPOPT_NOP)
         optlen = 1;
      else {
         optlen = cp[IPOPT_OLEN];
         if (optlen <= IPOPT_OLEN || optlen > cnt)
            goto bad;
      }
      switch (opt) {

      default:
         break;
      case IPOPT_LSRR:
      case IPOPT_SSRR:
         // user process specifies route as:
         // ->A->B->C->D
         // D must be our final destination (but we can't
         // check that since we may not have connected yet).
         // A is first hop destination, which doesn't appear in
         // actual IP option, but is stored before the options.
         if (optlen < IPOPT_MINOFF - 1 + sizeof(struct in_addr))
            goto bad;
         m->m_len -= sizeof(struct in_addr);
         cnt -= sizeof(struct in_addr);
         optlen -= sizeof(struct in_addr);
         cp[IPOPT_OLEN] = optlen;

         // Move first hop before start of options.
         bcopy((caddr_t)&cp[IPOPT_OFFSET+1], mtod(m, caddr_t),
             sizeof(struct in_addr));

         // Then copy rest of options back
         // to close up the deleted entry.
         ovbcopy((caddr_t)(&cp[IPOPT_OFFSET+1] +
             sizeof(struct in_addr)),
             (caddr_t)&cp[IPOPT_OFFSET+1],
             (unsigned)cnt + sizeof(struct in_addr));
         break;
      }
   }
   if (m->m_len > MAX_IPOPTLEN + sizeof(struct in_addr))
      goto bad;
   *pcbopt = m;
   return (0);

bad:
   usn_free_mbuf(m);
   return (EINVAL);
*/
}
/*
 * IP socket option processing.
 */
int
ip_ctloutput(int op, struct usn_socket *so, int level, int optname, usn_mbuf_t **mp)
{
   struct inpcb *inp = sotoinpcb(so);
   usn_mbuf_t *m = *mp;
   int optval;
   int error = 0;

   if (level != IPPROTO_IP) {
      error = EINVAL;
      if (op == PRCO_SETOPT && *mp)
         usn_free_mbuf(*mp);
   } else switch (op) {

   case PRCO_SETOPT:
      switch (optname) {
      case IP_OPTIONS:
      case IP_RETOPTS:
         return (ip_pcbopts(optname, &inp->inp_options, m));

      case IP_TOS:
      case IP_TTL:
      case IP_RECVOPTS:
      case IP_RECVRETOPTS:
      case IP_RECVDSTADDR:
         if (m->mlen != sizeof(int))
            error = EINVAL;
         else {
            optval = *mtod(m, int *);
            switch (optname) {

            case IP_TOS:
               inp->inp_ip.ip_tos = optval;
               break;

            case IP_TTL:
               inp->inp_ip.ip_ttl = optval;
               break;
#define  OPTSET(bit) \
   if (optval) \
      inp->inp_flags |= bit; \
   else \
      inp->inp_flags &= ~bit;

            case IP_RECVOPTS:
               OPTSET(INP_RECVOPTS);
               break;

            case IP_RECVRETOPTS:
               OPTSET(INP_RECVRETOPTS);
               break;

            case IP_RECVDSTADDR:
               OPTSET(INP_RECVDSTADDR);
               break;
            }
         }
         break;
#undef OPTSET
      case IP_MULTICAST_IF:
      case IP_MULTICAST_TTL:
      case IP_MULTICAST_LOOP:
      case IP_ADD_MEMBERSHIP:
      case IP_DROP_MEMBERSHIP:
         // FIXME: define ip_moptions
         //error = ip_setmoptions(optname, &inp->inp_moptions, m);
         error = ip_setmoptions(optname, 0, m);
         break;

      default:
         error = ENOPROTOOPT;
         break;
      }
      if (m)
         usn_free_mbuf(m);
      break;

   case PRCO_GETOPT:
      switch (optname) {
      case IP_OPTIONS:
      case IP_RETOPTS:
         *mp = m = usn_get_mbuf(0, BUF_MSIZE, 0);
         if (inp->inp_options) {
            m->mlen = inp->inp_options->mlen;
            bcopy(mtod(inp->inp_options, caddr_t),
                mtod(m, caddr_t), (unsigned)m->mlen);
         } else
            m->mlen = 0;
         break;

      case IP_TOS:
      case IP_TTL:
      case IP_RECVOPTS:
      case IP_RECVRETOPTS:
      case IP_RECVDSTADDR:
         *mp = m = usn_get_mbuf(0, BUF_MSIZE, 0);
         m->mlen = sizeof(int);
         switch (optname) {
         case IP_TOS:
            optval = inp->inp_ip.ip_tos;
            break;

         case IP_TTL:
            optval = inp->inp_ip.ip_ttl;
            break;

#define  OPTBIT(bit) (inp->inp_flags & bit ? 1 : 0)

         case IP_RECVOPTS:
            optval = OPTBIT(INP_RECVOPTS);
            break;

         case IP_RECVRETOPTS:
            optval = OPTBIT(INP_RECVRETOPTS);
            break;

         case IP_RECVDSTADDR:
            optval = OPTBIT(INP_RECVDSTADDR);
            break;
         }
         *mtod(m, int *) = optval;
         break;

      case IP_MULTICAST_IF:
      case IP_MULTICAST_TTL:
      case IP_MULTICAST_LOOP:
      case IP_ADD_MEMBERSHIP:
      case IP_DROP_MEMBERSHIP:
         // FIXME: define ip_moptions
         //error = ip_getmoptions(optname, inp->inp_moptions, mp);
         error = ip_getmoptions(optname, 0, mp);
         break;

      default:
         error = ENOPROTOOPT;
         break;
      }
      break;
   }
   return (error);
}
