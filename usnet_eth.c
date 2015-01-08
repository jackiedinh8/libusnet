#include <arpa/inet.h>

#include "usnet_eth.h"
#include "usnet_arp.h"
#include "usnet_ip.h"
#include "usnet_if.h"
#include "usnet_error.h"
#include "usnet_route.h"

u_char g_ether_addr[6];
u_char g_ip_addr[4];
struct usn_in_addr g_in_addr;
u_char g_eth_bcast_addr[6] = 
       { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
u_char g_etherbroadcastaddr[6] = 
       { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * Convert Ethernet address to printable (loggable) representation.
 */
static char digits[] = "0123456789abcdef";
char *
ether_sprintf(u_char *ap)
{
	static char etherbuf[18];
	char        *cp = etherbuf;
	int         i;

	for (i = 0; i < 6; i++) {
		*cp++ = digits[*ap >> 4];
		*cp++ = digits[*ap++ & 0xf];
		*cp++ = ':';
	}
	*--cp = 0;
	return (etherbuf);
}

/*
 * Process a received Ethernet packet;
 * the packet is in the mbuf chain m without
 * the ether header, which is provided separately.
 */

/*
void
ether_input(
	struct ifnet *ifp,
	struct ether_header *eh,
	struct mbuf *m)
{
   return;
	struct ifqueue *inq;
	struct llc *l;
	struct arpcom *ac = (struct arpcom *)ifp;
	int s;

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	ifp->if_lastchange = time;
	ifp->if_ibytes += m->m_pkthdr.len + sizeof (*eh);
	if (bcmp((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
	    sizeof(etherbroadcastaddr)) == 0)
		m->m_flags |= M_BCAST;
	else if (eh->ether_dhost[0] & 1)
		m->m_flags |= M_MCAST;
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

	switch (eh->ether_type) {

	case ETHERTYPE_IP:
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ETHERTYPE_ARP:
		schednetisr(NETISR_ARP);
		inq = &arpintrq;
		break;

		dropanyway:
		default:
			m_freem(m);
			return;
		}
	}

	s = splimp();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
}
*/

void
eth_input(u_char *buf, int len)
{
	ether_header_t      *et;
   usn_mbuf_t          *m;
	int                  flags;
   u_short              ether_type;

   // TODO do statistics & verify status of the interface
	//if_ipackets++;

#ifdef TIME_NANO
   clock_gettime(CLOCK_REALTIME, &g_time);
#else
   gettimeofday(&g_time, 0);
#endif

	et = (ether_header_t *)buf;

   // XXX more checks here
	if (len <= 0) {
		// if_ierrors++;
		return;
	}

#ifdef DUMP_PAYLOAD
   DEBUG("eth_input: dump info, ptr=%p, len=%d", buf, len);
   dump_buffer((char*)buf,len,"frame");
#endif

	flags = 0;
	if (bcmp((caddr_t)g_etherbroadcastaddr,
	    (caddr_t)et->ether_dhost, sizeof(g_etherbroadcastaddr)) == 0)
		flags |= BUF_BCAST;
	if (et->ether_dhost[0] & 1)
		flags |= BUF_MCAST;

	/*
	 *  construct mbuf for the frame.
    *  XXX delay to copy as much as possible.
	 */
#ifdef _USN_ZERO_COPY_
   m = usn_get_mbuf_zc(buf, len, flags);
#else
   m = usn_get_mbuf(buf, len, flags);
#endif

	if (m == 0) {
      // XXX do statistics
      // if_nomem++ 
		return;
   }
   m->flags |= BUF_ETHERHDR;

   // adjust buffer head.
   m->head += sizeof(ether_header_t);
   m->mlen -= sizeof(ether_header_t);

#ifdef DUMP_PAYLOAD
	DEBUG("eth_input: dump info: ptr=%p, len=%d\n", m->head, m->mlen);
   dump_buffer((char*)m->head, m->mlen, "eth_head");
#endif

	ether_type = ntohs((u_short)et->ether_type);
	switch(ether_type){
      case ETHERTYPE_IP: /* IP protocol */
         ipv4_input(m);
			break;
      case ETHERTYPE_ARP:/* Addr. resolution protocol */
			in_arpinput(m);
			break;
		case ETHERTYPE_REVARP:/* reverse Addr. resolution protocol */
			DEBUG("RARP protocol is not supported");
			break;
		default:
			DEBUG("protocol is not supported, proto=%d", ether_type);
			break;
	}

   return;
}

#define senderr(x) { g_errno = x; error=-(x) ; goto bad; }
int32
eth_output(usn_mbuf_t *m0, struct usn_sockaddr *dst, struct rtentry *rt0)
{
   short type;
   int32  error = -1;
   u_char edst[6];
   usn_mbuf_t *m = m0;
   struct rtentry *rt;
   ether_header_t *eh;

#ifdef DUMP_PAYLOAD
   DEBUG("eth_output: dump info, ptr=%p, len=%d", m->head, m->mlen);
   dump_buffer((char*)m->head, m->mlen,"frm");
#endif

   // XXX g_ifnet could be passed as argument if we implement multi interfaces.
   if ((g_ifnet->if_flags & (USN_IFF_UP|USN_IFF_RUNNING)) != (USN_IFF_UP|USN_IFF_RUNNING))
      senderr(USN_ENETDOWN);

   // TODO: update it
   //g_ifnet->if_lastchange = time(0);
   if ( rt0 == NULL ) DEBUG("rt0 is null");
   rt = rt0;
   if (rt) {
      if ((rt->rt_flags & RTF_UP) == 0) {
         rt0 = rt = rtalloc8(dst, 1);
         if (rt0)
            rt->rt_refcnt--;
         else
            senderr(USN_ENOROUTE);
      }
      if (rt->rt_flags & RTF_GATEWAY) {
         if (rt->rt_gwroute == 0)
            goto lookup;
         if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
            rtfree(rt); 
            rt = rt0;
         lookup: 
            rt->rt_gwroute = rtalloc8(rt->rt_gateway, 1);
            if ((rt = rt->rt_gwroute) == 0)
               senderr(USN_ENOROUTE);
         }
      }
      if (rt->rt_flags & RTF_REJECT)
         if (rt->rt_rmx.rmx_expire == 0 ||
             g_time.tv_sec < (long)rt->rt_rmx.rmx_expire)
            senderr(rt == rt0 ? USN_EHOSTDOWN : USN_ENOROUTE);
   }

   switch (dst->sa_family) {
   case AF_INET:
      DEBUG("call arpresolve, family=%d", dst->sa_family);
      if (!arpresolve(rt, m, dst, edst))
         return -1;//(0); /* if not yet resolved */

      // TODO: If broadcasting on a simplex interface, loopback a copy
      //if ((m->m_flags & BUF_BCAST) && (ifp->if_flags & USN_IFF_SIMPLEX))
      //   mcopy = m_copy(m, 0, (int)M_COPYALL);
      //off = m->m_pkthdr.len - m->m_len;
      type = ETHERTYPE_IP;
      break;

   case AF_UNSPEC:
      DEBUG("set ether address, family=%d", dst->sa_family);
      eh = (ether_header_t *)dst->sa_data;
      bcopy((caddr_t)eh->ether_dhost, (caddr_t)edst, sizeof (edst));
      type = eh->ether_type;
      break;

   default:
      DEBUG("%s%d: can't handle af%d\n", g_ifnet->if_name, g_ifnet->if_unit,
         dst->sa_family);
      senderr(USN_EAFNOSUPPORT);
   }

   /* 
    * Add local net header.  If no space in first mbuf,
    * allocate another.
    */

   // FIXME: ensuring that there is room for 14 bytes 
   //         at the front of the packet.
   if ( (m->flags & BUF_RAW) == 0 ) {
      BUF_PREPEND(m, sizeof (ether_header_t));
   }

   if (m == 0) {
      DEBUG("eth_output: empty data");
      senderr(USN_ENOBUFSPACE);
   }
#ifdef DUMP_PAYLOAD
   DEBUG("eth_output: prepend eth header, ptr=%p, len=%d, type=%d", 
         m->head, m->mlen, type);
   dump_buffer((char*)m->head, m->mlen, "frm");
#endif

   eh = mtod(m, ether_header_t *);
   type = htons((u_short)type);
   bcopy((caddr_t)&type,(caddr_t)&eh->ether_type, sizeof(eh->ether_type));
   bcopy((caddr_t)edst, (caddr_t)eh->ether_dhost, sizeof (edst));
   bcopy((caddr_t)&g_ether_addr, (caddr_t)eh->ether_shost, sizeof(eh->ether_shost));
   
   /*
    * Queue message on interface, and start output if interface
    * not yet active.
    */
   //return send_mbuf(m);
   return usnet_send_frame(m);
bad:
   if (m) 
      usn_free_mbuf(m);
   return (error);
}



int send_mbuf(usn_mbuf_t *m) 
{
   u_int      size;
   u_char    *buf;
   int        j;
   int        attemps = 0;
   //struct pollfd       fds;
   //fds.fd = g_nmd->fd;

   if ( m == 0 )
      return 0;


   //DEBUG("send_mbuf: dump info, ptr=%p, len=%d", m->head, m->mlen);
   //dump_payload_only((char*)m->head, m->mlen);

   // TODO: send a buffer chain, not one buffer.
   buf = m->head;
   size = m->mlen;
resend:
   if ( attemps == 3 ) {
      return 0;
   }
   for (j = g_nmd->first_tx_ring; 
            j <= g_nmd->last_tx_ring + 1; j++) {
      /* compute current ring to use */
      struct netmap_ring *ring;
      uint32_t i, idx;

      ring = NETMAP_TXRING(g_nmd->nifp, j);

      if (nm_ring_empty(ring)) {
         //printf("send_mbuf: ring is empty, ring_num=%d\n", j);
         continue;
      }   
      i = ring->cur;
      idx = ring->slot[i].buf_idx;
      ring->slot[i].len = size;
      nm_pkt_copy(buf, NETMAP_BUF(ring, idx), size);
      g_nmd->cur_tx_ring = j;

      // updating triggers hw to send msg?
      ring->head = ring->cur = nm_ring_next(ring, i); 
      return size;
   }   
   ioctl(g_nmd->fd, NIOCTXSYNC, NULL);
   usleep(1);
   attemps++;
   /*
   if ( attemps == 2 ) {
      int ret = 0;
      fds.events = POLLOUT;
      fds.revents = 0;
      ret = poll(&fds, 1, 20);
      //printf("send_mbuf: rings are full, ring=%d, attemps=%d, ret=%d\n", j, attemps, ret);
      if ( ret < 0 )
         return 0;
      if ( fds.revents & POLLOUT )
         goto resend;
   }
   */
   goto resend;
   return 0; /* fail */

}

