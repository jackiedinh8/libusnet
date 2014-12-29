#include <netinet/in.h>

#include "usnet_ip.h"
#include "usnet_ip_icmp.h"
#include "usnet_if.h"
#include "usnet_route.h"
#include "usnet_ip_out.h"
#include "usnet_ip_var.h"
#include "usnet_udp.h"
#include "usnet_arp.h"

#undef	ADDCARRY
#define ADDCARRY(sum)  {				\
			if (sum & 0xffff0000) {		\
				sum &= 0xffff;		\
				sum++;			\
			}				\
		}

#define  IPFORWARDING   0 
#define  IPSENDREDIRECTS   1
int g_ipsendredirects;// = IPSENDREDIRECTS;
int g_ipforwarding;// = IPFORWARDING;
u_short  g_ip_id;            /* ip packet ctr, for ids */

void 
init_ipv4()
{
   DEBUG("init_ipv4: start");
   g_ip_id = 1;
   g_ipforwarding = IPFORWARDING;
   g_ipsendredirects = IPSENDREDIRECTS;
   DEBUG("init_ipv4: done");
}


unsigned short 
in_cksum(usn_mbuf_t *m, int len)
{
	union word {
		char	c[2];
		u_short	s;
	} u;
	u_short *w;
	int sum = 0;
	int mlen = 0;
	for (;m && len; m = m->next) {
		//if (m->mlen == 0)
		if (m->head == m->tail) // empty buffer.
			continue;
		w = (u_short *) m->head;
		if (mlen == -1) {
			/*
			 * The first byte of this mbuf is the continuation
			 * of a word spanning between this mbuf and the
			 * last mbuf.
			 */

			/* u.c[0] is already saved when scanning previous 
			 * buf.
			 */
			u.c[1] = *(u_char *)w;
			sum += u.s;
			ADDCARRY(sum);
			w = (u_short *)((char *)w + 1);
			mlen = (m->tail - m->head) - 1;
			len--;
		} else
			mlen = m->tail - m->head;

		if (len < mlen)
			mlen = len;
		len -= mlen;

		/*
		 * add by words.
		 */
		while ((mlen -= 2) >= 0) {
			if ((uintptr_t)w & 0x1) {
				/* word is not aligned */
				u.c[0] = *(char *)w;
				u.c[1] = *((char *)w+1);
				sum += u.s;
				w++;
			} else
				sum += *w++;
			ADDCARRY(sum);
         //DEBUG("*w=%d, sum=%d",*w,sum);
		}
		if (mlen == -1)
			/*
			 * This mbuf has odd number of bytes. 
			 * There could be a word split betwen
			 * this mbuf and the next mbuf.
			 * Save the last byte (to prepend to next mbuf).
			 */
			u.c[0] = *(u_char *)w;
	}
	if (len)
		DEBUG("cksum: out of data\n");
	if (mlen == -1) {
		/* The last mbuf has odd # of bytes. Follow the
		   standard (the odd byte is shifted left by 8 bits) */
		u.c[1] = 0;
		sum += u.s;
		ADDCARRY(sum);
	}
	DEBUG("cksum: sum=%d, result=%d\n", sum, ~sum & 0xffff);
	return (~sum & 0xffff);
}

unsigned short 
udp_cksum(usn_udphdr_t* ip, int len)
{
   int32_t         sum = 0;
   unsigned short *p;

   ip->uh_sum = 0;
   p = (unsigned short*) ip;
   while (len > 1 ) {
      sum += *(p++);
      if ( sum & 0x80000000 )
         sum = (sum & 0xFFFF) + (sum >> 16);
      len -= 2;
   } 
   if (len)
      sum += (unsigned short) *(unsigned char *)ip;

   while (sum >> 16)
      sum = (sum & 0xFFFF) + (sum >> 16);

   return ~sum;
}


unsigned short 
cksum(usn_ip_t* ip, int len)
{
   int32_t         sum = 0;
   unsigned short *p;

   ip->ip_sum = 0;
   p = (unsigned short*) ip;
   while (len > 1 ) {
      sum += *(p++);
      if ( sum & 0x80000000 )
         sum = (sum & 0xFFFF) + (sum >> 16);
      len -= 2;
   } 
   if (len)
      sum += (unsigned short) *(unsigned char *)ip;

   while (sum >> 16)
      sum = (sum & 0xFFFF) + (sum >> 16);

   ip->ip_sum = ~sum;
   return ~sum;
}

/*
 * We need to save the IP options in case a protocol wants to respond
 * to an incoming packet over the same route if the packet got here
 * using IP source routing.  This allows connection establishment and
 * maintenance when the remote end is on a network that is not known
 * to us.
 */
int   ip_nhops = 0;
static struct ip_srcrt {
   struct usn_in_addr dst;         /* final destination */
   char               nop;           /* one NOP to align */
   char               srcopt[IPOPT_OFFSET + 1];  /* OPTVAL, OLEN and OFFSET */
   struct usn_in_addr route[MAX_IPOPTLEN/sizeof(struct in_addr)];
} ip_srcrt;

/*
 * Save incoming source route for use in replies,
 * to be picked up later by ip_srcroute if the receiver is interested.
 */
void
save_rte(u_char *option, struct usn_in_addr dst) 
{
   unsigned olen;

   olen = option[IPOPT_OLEN];

   DEBUG("save_rte: olen %d", olen);

   if (olen > sizeof(ip_srcrt) - (1 + sizeof(dst)))
      return;

   bcopy((caddr_t)option, (caddr_t)ip_srcrt.srcopt, olen);
   ip_nhops = (olen - IPOPT_OFFSET - 1) / sizeof(struct in_addr);
   ip_srcrt.dst = dst; 
}

/*                                                                                                                   
 * Given address of next destination (final or next hop),                                                            
 * return internet address info of interface to be used to get there.                                                
 */                                                                                                                  
struct route ipforward_rt;

struct in_ifaddr *                                                                                                   
ip_rtaddr( struct usn_in_addr dst)
{                                                                                                                    
   struct usn_sockaddr_in *sin;                                                                                 
   
   // XXX not forward   
   return ((struct in_ifaddr *)0);                                                                                

   sin = (struct usn_sockaddr_in *) &ipforward_rt.ro_dst;                                                                
                                                                                                                     
   if (ipforward_rt.ro_rt == 0 || dst.s_addr != sin->sin_addr.s_addr) {                                              
      if (ipforward_rt.ro_rt) {                                                                                      
         RTFREE(ipforward_rt.ro_rt);                                                                                 
         ipforward_rt.ro_rt = 0;                                                                                     
      }                                                                                                              
      sin->sin_family = AF_INET;                                                                                     
      sin->sin_len = sizeof(*sin);                                                                                   
      sin->sin_addr = dst;                                                                                           
                                                                                                                     
      rtalloc(&ipforward_rt);                                                                                        
   }                                                                                                                 
   if (ipforward_rt.ro_rt == 0)                                                                                      
      return ((struct in_ifaddr *)0);                                                                                
   return ((struct in_ifaddr *) ipforward_rt.ro_rt->rt_ifa);                                                         
}

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 * The srcrt parameter indicates whether the packet is being forwarded
 * via a source route.
 */
void
ip_forward(usn_mbuf_t *m, int srcrt)
{
   usn_ip_t *ip =(usn_ip_t*) m->head;
   struct usn_sockaddr_in *sin;
   struct rtentry *rt;
   int error, type = 0, code;
   usn_mbuf_t *mcopy;
   n_long dest;
   struct ifnet *destifp;

   dest = 0;
#ifdef DIAGNOSTIC
   if (ipprintfs)
      DEBUG("forward: src %x dst %x ttl %x\n", ip->ip_src,
         ip->ip_dst, ip->ip_ttl);
#endif
   ip->ip_id = ntohs(ip->ip_id);
   if (ip->ip_ttl <= IPTTLDEC) {
      icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, dest, 0);
      return;
   }
   ip->ip_ttl -= IPTTLDEC;

   sin = (struct usn_sockaddr_in *)&ipforward_rt.ro_dst;
   if ((rt = ipforward_rt.ro_rt) == 0 ||
       ip->ip_dst.s_addr != sin->sin_addr.s_addr) {
      if (ipforward_rt.ro_rt) {
         RTFREE(ipforward_rt.ro_rt);
         ipforward_rt.ro_rt = 0;
      }
      sin->sin_family = AF_INET;
      sin->sin_len = sizeof(*sin);
      sin->sin_addr = ip->ip_dst;

      rtalloc(&ipforward_rt);
      if (ipforward_rt.ro_rt == 0) {
         icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, dest, 0);
         return;
      }
      rt = ipforward_rt.ro_rt;
   }

   /*
    * Save at most 64 bytes of the packet in case
    * we need to generate an ICMP message to the src.
    */
#define imin(x,y)  ((x) > (y) ? (y) : (x))
   mcopy = usn_mbuf_copy(m, 0, imin((int)ip->ip_len, 64));

   /*
    * If forwarding packet using same interface that it came in on,
    * perhaps should send a redirect to sender to shortcut a hop.
    * Only send redirect if source is sending directly to us,
    * and if packet was not source routed (or has any options).
    * Also, don't send redirect if forwarding using a default route
    * or a route modified by a redirect.
    */
#define  satosin(sa) ((struct usn_sockaddr_in *)(sa))
   if ( //rt->rt_ifp == m->m_pkthdr.rcvif &&
       (rt->rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) == 0 &&
       satosin(rt_key(rt))->sin_addr.s_addr != 0 &&
       g_ipsendredirects && !srcrt) {
#define  RTA(rt)  ((struct in_ifaddr *)(rt->rt_ifa))
      u_long src = ntohl(ip->ip_src.s_addr);

      if (RTA(rt) &&
          (src & RTA(rt)->ia_subnetmask) == RTA(rt)->ia_subnet) {
          if (rt->rt_flags & RTF_GATEWAY)
         dest = satosin(rt->rt_gateway)->sin_addr.s_addr;
          else
         dest = ip->ip_dst.s_addr;
          /* Router requirements says to only send host redirects */
          type = ICMP_REDIRECT;
          code = ICMP_REDIRECT_HOST;
#ifdef DIAGNOSTIC
          if (ipprintfs)
              printf("redirect (%d) to %lx\n", code, (u_long)dest);
#endif
      }
   }

   error = ipv4_output(m, NULL, &ipforward_rt, IP_FORWARDING);

   if (error)
      ;//ipstat.ips_cantforward++;
   else {
      ;//ipstat.ips_forward++;
      if (type)
         ;//ipstat.ips_redirectsent++;
      else {
         if (mcopy)
            usn_free_mbuf(mcopy);
         return;
      }
   }
   if (mcopy == NULL)
      return;
   destifp = NULL;

   switch (error) {

   case 0:           /* forwarded, but need redirect */
      /* type, code set above */
      break;

   case ENETUNREACH:    /* shouldn't happen, checked above */
   case EHOSTUNREACH:
   case ENETDOWN:
   case EHOSTDOWN:
   default:
      type = ICMP_UNREACH;
      code = ICMP_UNREACH_HOST;
      break;

   case EMSGSIZE:
      type = ICMP_UNREACH;
      code = ICMP_UNREACH_NEEDFRAG;
      // XXX implement this
      if (ipforward_rt.ro_rt)
         destifp = NULL;
         //destifp = ipforward_rt.ro_rt->rt_ifp;
      ;//ipstat.ips_cantfrag++;
      break;

   case ENOBUFS:
      type = ICMP_SOURCEQUENCH;
      code = 0;
      break;
   }
   icmp_error(mcopy, type, code, dest, destifp);
}

/*
 * Do option processing on a datagram,
 * possibly discarding it if bad options are encountered,
 * or forwarding it if source-routed.
 * Returns 1 if packet has been forwarded/freed,
 * 0 if the packet should be processed further.
 */
int
ip_dooptions(usn_mbuf_t *m)
{
	struct ip_timestamp *ipt;
	struct in_ifaddr    *ia;
	struct usn_in_addr  *sin, dst;
	usn_ip_t            *ip = (usn_ip_t*) m->head;
   struct usn_sockaddr_in   ipaddr = { sizeof(ipaddr), AF_INET };
	u_char              *cp;
	n_time               ntime;
	u_int                opt, optlen, cnt, off;
   int                  code, type = ICMP_PARAMPROB;
   int                  forward = 0;
   
   (void)sin;
   (void)ntime;
   ia = NULL;
   ipt = NULL;

	dst = ip->ip_dst;
	cp = (u_char *)(ip + 1);
	cnt = (ip->ip_hl << 2) - sizeof(usn_ip_t); // len of ip options.
  
	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= 0 || optlen > cnt) {
				code = &cp[IPOPT_OLEN] - (u_char *)ip;
				goto bad;
			}
		}

		switch (opt) {

		default:
			break;

		 //* Source routing with record.
		 //* Find interface with current destination address.
		 //* If none on this machine then drop if strictly routed,
		 //* or do nothing if loosely routed.
		 //* Record interface address and bring up next address
		 //* component.  If strictly routed make sure next
		 //* address is on directly accessible net.

		case IPOPT_LSRR:
		case IPOPT_SSRR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			ipaddr.sin_addr = ip->ip_dst; // this is global variable
			ia = (struct in_ifaddr *)
		   		ifa_ifwithaddr((struct usn_sockaddr *)&ipaddr);
			if (ia == 0) {
				if (opt == IPOPT_SSRR) {
					type = ICMP_UNREACH;
					code = ICMP_UNREACH_SRCFAIL;
					goto bad;
				}
				 //* Loose routing, and not at next destination
				 //* yet; nothing to do except forward.
				break;
			}
			off--; // 0 origin
			if (off > optlen - sizeof(struct usn_in_addr)) {
				// XXX End of source route. Should be for us.
				save_rte(cp, ip->ip_src);
				break;
			}
			//* locate outgoing interface
			bcopy((caddr_t)(cp + off), (caddr_t)&ipaddr.sin_addr,
			    sizeof(ipaddr.sin_addr));
			if (opt == IPOPT_SSRR) {
#define	INA	struct in_ifaddr *
#define	SA	struct usn_sockaddr *

            // look at point-to-point interface,
            //   always FAILURE since we do not forward
			   if ((ia = (INA)ifa_ifwithdstaddr((SA)&ipaddr)) == 0)
				  ia = (INA)ifa_ifwithnet((SA)&ipaddr);
			} else
				ia = ip_rtaddr(ipaddr.sin_addr);

			if (ia == 0) {
				type = ICMP_UNREACH;
				code = ICMP_UNREACH_SRCFAIL;
				goto bad;
			}
         // XXX never reach this code.
         /*
			ip->ip_dst = ipaddr.sin_addr;
			bcopy((caddr_t)&(IA_SIN(ia)->sin_addr),
			    (caddr_t)(cp + off), sizeof(struct in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct in_addr);
			// Let ip_intr's mcast routing check handle mcast pkts
			forward = !IN_MULTICAST(ntohl(ip->ip_dst.s_addr));
         */
			break;

		case IPOPT_RR:
			if ((off = cp[IPOPT_OFFSET]) < IPOPT_MINOFF) {
				code = &cp[IPOPT_OFFSET] - (u_char *)ip;
				goto bad;
			}
			// If no space remains, ignore.
			off--;			// 0 origin
			if (off > optlen - sizeof(struct usn_in_addr))
				break;
			bcopy((caddr_t)(&ip->ip_dst), (caddr_t)&ipaddr.sin_addr,
			    sizeof(ipaddr.sin_addr));
			// locate outgoing interface; if we're the destination,
			// use the incoming interface (should be same).
			if ((ia = (INA)ifa_ifwithaddr((SA)&ipaddr)) == 0 &&
			    (ia = ip_rtaddr(ipaddr.sin_addr)) == 0) {
		   		type = ICMP_UNREACH;
		   		code = ICMP_UNREACH_HOST;
		   		goto bad;
			}
			bcopy((caddr_t)&(IA_SIN(ia)->sin_addr),
			    (caddr_t)(cp + off), sizeof(struct usn_in_addr));
			cp[IPOPT_OFFSET] += sizeof(struct usn_in_addr);
			break;

		case IPOPT_TS:
			code = cp - (u_char *)ip;
			ipt = (struct ip_timestamp *)cp;
			if (ipt->ipt_len < 5)
				goto bad;
         // XXX wierd "long" here, just replaced by "u_int"
			if (ipt->ipt_ptr > ipt->ipt_len - sizeof (u_int)) {
				if (++ipt->ipt_oflw == 0)
					goto bad;
				break;
			}
			sin = (struct usn_in_addr *)(cp + ipt->ipt_ptr - 1);
			switch (ipt->ipt_flg) {

			case IPOPT_TS_TSONLY:
				break;

			case IPOPT_TS_TSANDADDR:
				if (ipt->ipt_ptr + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				ipaddr.sin_addr = dst;
				//ia = (INA)ifaof_ifpforaddr((SA)&ipaddr, 
			   // 			    m->m_pkthdr.rcvif);
				ia = (INA)ifaof_ifpforaddr((SA)&ipaddr, NULL);

				if (ia == 0)
					continue;

				bcopy((caddr_t)&IA_SIN(ia)->sin_addr,
				    (caddr_t)sin, sizeof(struct in_addr));
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			case IPOPT_TS_PRESPEC:
				if (ipt->ipt_ptr + sizeof(n_time) +
				    sizeof(struct in_addr) > ipt->ipt_len)
					goto bad;
				bcopy((caddr_t)sin, (caddr_t)&ipaddr.sin_addr,
				    sizeof(struct in_addr));
				if (ifa_ifwithaddr((SA)&ipaddr) == 0)
			   		continue;
				ipt->ipt_ptr += sizeof(struct in_addr);
				break;

			default:
				goto bad;
			}
         // XXX implement this
			ntime = iptime();
			bcopy((caddr_t)&ntime, (caddr_t)cp + ipt->ipt_ptr - 1,
			    sizeof(n_time));
			ipt->ipt_ptr += sizeof(n_time);
		}
	}
	if (forward) {
      // XXX never reach this point.
		ip_forward(m, 1);
		return (1);
	}
	return (0);
bad:
	ip->ip_len -= ip->ip_hl << 2;   // XXX icmp_error adds in hdr length
	icmp_error(m, type, code, 0, 0);
	//ipstat.ips_badoptions++;

	return (1);
}

/*
 * Take incoming datagram fragment and try to reassemble it into
 * whole datagram.  If the argument is the first fragment or one
 * in between the function will return NULL and store the mbuf
 * in the fragment chain.  If the argument is the last fragment
 * the packet will be reassembled and the pointer to the new
 * mbuf returned for further processing.  Only m_tags attached
 * to the first packet/fragment are preserved.
 * The IP header is *NOT* adjusted out of iplen.
 */
usn_mbuf_t *
ip_reass(usn_mbuf_t *m)
{
   usn_mbuf_t      *p,*q,*nq;
   usn_mbuf_t      *t;
   struct ipq      *fp;
   usn_ip_t        *ip = GETIP(m);//(usn_ip_t*)(m->head);
   int              hlen = ip->ip_hl << 2;
   int              i, next;

   // Look for queue of fragments
   // of this datagram. 
   // XXX It is better to use hash functions
   for (fp = g_ipq.next; fp != NULL; fp = fp->next) {
      if (ip->ip_id == fp->ipq_id &&
          ip->ip_src.s_addr == fp->ipq_src.s_addr &&
          ip->ip_dst.s_addr == fp->ipq_dst.s_addr &&
          ip->ip_p == fp->ipq_p) {
         goto found;
      }
   }
   fp = 0;

found:
   /*   
    * Adjust ip_len to not reflect header,
    * convert offset of this to bytes.
    */
   ip->ip_len -= hlen;
   if (ip->ip_off & IP_MF) {
      /*   
       * Make sure that fragments have a data length
       * that's a non-zero multiple of 8 bytes.
       */
      if (ip->ip_len == 0 || (ip->ip_len & 0x7) != 0) { 
         //IPSTAT_INC(ips_toosmall); // XXX
         goto dropfrag;
      }    
      m->flags |= BUF_IP_MF;
   }
   ip->ip_off <<= 3;

   /*
    * Presence of header sizes in mbufs
    * would confuse code below.
    */
   m->head += hlen; 
   m->mlen -= hlen;

   /*
    * If first fragment to arrive, create a reassembly queue.
    */
   if (fp == 0) {
      t = usn_get_mbuf(NULL,sizeof(struct ipq),0);
      if ( t == NULL )
         goto dropfrag;

      fp = (struct ipq *)(t->head);

      usn_insert_ipq(fp);

      fp->ipq_ttl = IPFRAGTTL;
      fp->ipq_p = ip->ip_p;
      fp->ipq_id = ip->ip_id;
      fp->frags_list = m;
      fp->ipq_src = ((usn_ip_t *)ip)->ip_src;
      fp->ipq_dst = ((usn_ip_t *)ip)->ip_dst;

      goto check;
   }

   /*
    * Find a segment which begins after this one does.
    */
   for (p = NULL, q = fp->frags_list; q ; p =q, q = q->next)
      if (GETIP(q)->ip_off > ip->ip_off)
         break;
   /*
    * If there is a preceding segment, it may provide some of
    * our data already. If so, drop the data from the incoming
    * segment. If it provides all of our data, drop us.
    */
   if (p) {
      i = GETIP(p)->ip_off + GETIP(p)->ip_len - ip->ip_off;
      if (i > 0) {
         if (i >= ip->ip_len)
            goto dropfrag;
         m->head += hlen + i;
         m->flags |= BUF_MODIFIED_IPHDR;
         ip->ip_off += i;
         ip->ip_len -= i;
      }
      m->next = p->next;
      p->next = m;
   }

   /*
    * While we overlap succeeding segments trim them or,
    * if they are completely covered, dequeue them.
    */
   while (q != NULL && ip->ip_off + ip->ip_len > GETIP(q)->ip_off) {
      i = (ip->ip_off + ip->ip_len) - GETIP(q)->ip_off;
      if (i < GETIP(q)->ip_len) {
         GETIP(q)->ip_len -= i;
         GETIP(q)->ip_off += i;
         if ( q->flags & BUF_MODIFIED_IPHDR ) {
            q->head += i;
         } else {
            q->head += (GETIP(q)->ip_hl << 2) + i;
            q->flags |= BUF_MODIFIED_IPHDR;
         }
         break;
      }
      nq = q->next;
      m->next = nq;
      usn_free_mbuf(q);
   }

check:
   next = 0;
   for (p = NULL, q = fp->frags_list; q; p = q, q = q->next) {
      if (GETIP(q)->ip_off != next) {
         // XXX do statistics
         goto done;
      }
      next += GETIP(q)->ip_len;
   }
   /* Make sure the last packet didn't have the IP_MF flag */
   if ( p->flags & BUF_IP_MF ) {
      // XXX implement this
      //if (fp->ipq_nfrags > V_maxfragsperpacket) {
      //   IPSTAT_ADD(ips_fragdropped, fp->ipq_nfrags);
      //   ip_freef(head, fp);
      //}
      //ip_freef(head, fp);
      goto done;
   }

   /*
    * Reassembly is complete; concatenate fragments.
    */
   q = fp->frags_list;
   ip = mtoiphdr(q);
   if (next + (ip->ip_hl << 2) > IP_MAXPACKET) {
      // XXX implement this
      //IPSTAT_INC(ips_toolong);
      //IPSTAT_ADD(ips_fragdropped, fp->ipq_nfrags);
      //ip_freef(head, fp);
      goto done;
   }

   /*
    * Create header for new ip packet by
    * modifying header of first packet;
    * dequeue and discard fragment reassembly header.
    * Make header visible.
    */
   ip->ip_len = (ip->ip_hl << 2) + next;
   //ip->ip_src = fp->ipq_src;
   //ip->ip_dst = fp->ipq_dst;
   q->mlen += ip->ip_hl << 2;
   q->head -= ip->ip_hl << 2;
   q->flags |= ~BUF_IP_MF;

   // XXX free fp pointer
   usn_remove_ipq(fp);

   return q;

dropfrag:
   //ipstat.ips_fragdropped++;
   usn_free_mbuf(m);
done:
   return (0);
}

 

void 
ipv4_input(usn_mbuf_t* m)
{
   usn_ip_t           *pip;
   u_short             hlen;
   struct in_ifaddr   *ia;

	DEBUG("ipv4_input: dump info: ptr=%p, len=%d\n", m->head, m->mlen);
   dump_payload_only((char*)m->head, m->mlen);

   // 1. Verification of incoming packets
	if ( m->mlen < sizeof(usn_ip_t) ) {
		//ipstat.ips_toosmall++;
	   DEBUG("incomplete ip header, len=%d", m->mlen);
		goto bad;
	}

   pip = GETIP(m);
	if (pip->ip_v != IPVERSION) {
		//ipstat.ips_badvers++;
	   DEBUG("not support ip version, ip_ver=%d", pip->ip_v);
		goto bad;
	}
	hlen = pip->ip_hl << 2;
	if (hlen < sizeof(usn_ip_t)) {	/* minimum header length */
		//ipstat.ips_badhlen++;
	   DEBUG("header len is too small, hlen=%d", hlen);
		goto bad;
	}
   
   // update to reflect buffer state.
   m->flags |= BUF_IPHDR;

   if ( in_cksum(m, hlen) ) {
      // discard damaged packet
      DEBUG("ip checksum error, v=%d, proto=%d, len=%d, ip_sum=%d, computed_sum=%d", 
            pip->ip_v, pip->ip_p, hlen, pip->ip_sum, in_cksum(m, hlen));
      goto bad;
   }

   // Convert fields to host representation.
   if (ntohs(pip->ip_len) < hlen) {
      //ipstat.ips_badlen++;
      DEBUG("not enough data, ip_len=%d, hlen=%d", 
              ntohs(pip->ip_len), hlen);
      goto bad; 
   }

   // 2. Option processing and forward
   if (hlen > sizeof (usn_ip_t) && ip_dooptions(m)) {
      DEBUG("wrong options, ip_len=%d, hlen=%d", pip->ip_len, hlen);
      goto bad;
   }

   m->flags |= BUF_IPDATA;

   // Check our list of addresses, to see if the packet is for us.
   DEBUG("desination addr, ip_addr=%x", pip->ip_dst.s_addr);
   for (ia = g_in_ifaddr; ia; ia = ia->ia_next) {
      DEBUG("interface addr, ip_addr=%x", IA_SIN(ia)->sin_addr.s_addr);
      if (IA_SIN(ia)->sin_addr.s_addr == pip->ip_dst.s_addr)
         goto ours;

      // TODO: set broadcast addr
      if ( 
//#ifdef   DIRECTED_BROADCAST
//          ia->ia_ifp == m->m_pkthdr.rcvif &&
//#endif
          (ia->ia_ifp->if_flags & USN_IFF_BROADCAST)) {
         u_long t;

         if (satosin(&ia->ia_broadaddr)->sin_addr.s_addr ==
             pip->ip_dst.s_addr)
            goto ours;
         if (pip->ip_dst.s_addr == ia->ia_netbroadcast.s_addr)
            goto ours;
         /*   
          * Look for all-0's host part (old broadcast addr),
          * either for subnet or net.
          */
         t = ntohl(pip->ip_dst.s_addr);
         if (t == ia->ia_subnet)
            goto ours;
         if (t == ia->ia_net)
            goto ours;
      }    
   }

/*
   if (USN_IN_MULTICAST(ntohl(pip->ip_dst.s_addr))) {
      // XXX remove multicast datagram & multicast router.
      goto bad;
   }
*/

   if (pip->ip_dst.s_addr == (u_int)USN_INADDR_BROADCAST)
      goto ours;
   if (pip->ip_dst.s_addr == USN_INADDR_ANY)
      goto ours;


   // Not for us; forward if possible and desirable.
   if (g_ipforwarding == 0) {
      //ipstat.ips_cantforward++;
      usn_free_mbuf(m);
   } else
      ip_forward(m, 0); //never called.

   goto next;

   // 3. Packet reassembly
ours:
   /*
    * Attempt reassembly; if it succeeds, proceed.
    * ip_reass() will return a different mbuf.
    */
   DEBUG("protocol=%d, ip_len=%d", pip->ip_p, ntohs(pip->ip_len));
   if (pip->ip_off & ( IP_MF | IP_OFFMASK )) {
      DEBUG("do reassembly");
      //ipstat.ips_fragments++;
      m = ip_reass(m); 
      if (m == 0)
         goto next;
      //ipstat.ips_reassembled++;
      pip  = mtoiphdr(m);
      /* Get the header length of the reassembled packet */
      hlen = pip->ip_hl << 2;
   }
   else 
      pip->ip_len =  htons(ntohs(pip->ip_len) - hlen); // to be consistent for all ip packets.

   // 4. Demultiplexing

   // Switch out to protocol's input routine.
   //ipstat.ips_delivered++;
   
   // handle TCP, UDP, and ICMP
   DEBUG("protocol=%d, header_len=%d, ip_len=%d", pip->ip_p, hlen, ntohs(pip->ip_len));

   switch(pip->ip_p) {
      case IPPROTO_ICMP:
         icmp_input(m, hlen);
         break;
      case IPPROTO_UDP:
         udp_input(m, hlen);
         break;
      case IPPROTO_TCP:
         //tcp_input(m, hlen);
         break;
      default:
         break;
   }

   goto next;

bad:
   usn_free_mbuf(m);

next:
   return;

}


/*
 * IP timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
ip_slowtimo()
{
   struct ipq *fp, *p; 

   fp = g_ipq.next;
   if (fp == NULL) { 
      return;
   }
   while (fp != NULL) {
      --fp->ipq_ttl;
      p = fp;
      fp = fp->next;
      if (p->ipq_ttl == 0) { 
         //ipstat.ips_fragtimeout++;
         usn_remove_ipq(p);
      }    
   }
}

/*
 * Strip out IP options, at higher
 * level protocol in the kernel.
 * Second argument is buffer to which options
 * will be moved, and return value is their length.
 * XXX should be deleted; last arg currently ignored.
 */
void
ip_stripoptions( usn_mbuf_t *m, usn_mbuf_t *mopt)
{
   int i; 
   usn_ip_t *ip = mtod(m, usn_ip_t *);
   caddr_t opts;
   int olen;
   
   olen = (ip->ip_hl<<2) - sizeof (usn_ip_t);
   opts = (caddr_t)(ip + 1);
   i = m->mlen - (sizeof (usn_ip_t) + olen);
   bcopy(opts  + olen, opts, (unsigned)i);
   m->mlen -= olen;
   ip->ip_hl = sizeof(usn_ip_t) >> 2;
}


inline void
usn_insert_ipq(struct ipq *fp)
{
   fp->next = g_ipq.next;
   g_ipq.next = fp; 
}

inline void
usn_remove_ipq(struct ipq *fp)
{
   struct ipq *p,*q;
   for ( p = NULL, q = &g_ipq; q; p = q, q = q->next)
      if ( fp == q ) {
         p->next = q->next; 
         usn_free_mbuf(IPQ_TO_MBUF(fp));
         break;
      }
}

inline void
insert_ipfrag(struct ipq *fp, struct ipasfrag *ip)
{
   return;
}

/*
 * Put an ip fragment on a reassembly chain.
 * Like insque, but pointers in middle of structure.
 */
inline void
ip_enq(struct ipasfrag *p, struct ipasfrag *prev)
{

   p->ipf_prev = prev;
   p->ipf_next = prev->ipf_next;
   prev->ipf_next->ipf_prev = p;
   prev->ipf_next = p;
}


