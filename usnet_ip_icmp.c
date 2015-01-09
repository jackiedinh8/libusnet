/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 * @(#)ip_icmp.c  8.2 (Berkeley) 1/4/94
 */


#include <netinet/in.h>
#include <sys/time.h>

#include "usnet_ip_icmp.h"
#include "usnet_if.h"
#include "usnet_route.h"
#include "usnet_in.h"
#include "usnet_ip_out.h"
#include "usnet_buf.h"
#include "usnet_common.h"


/*
 * ICMP routines: error generation, receive packet processing, and
 * routines to turnaround packets back to the originator, and
 * host table maintenance routines.
 */

int	icmpmaskrepl = 0;
#define ICMPPRINTFS
#ifdef ICMPPRINTFS
int	icmpprintfs = 1;
#endif

extern	struct protosw inetsw[];

/*
 * Generate an error packet of type error
 * in response to bad packet ip.
 */

void
icmp_error(usn_mbuf_t *n, int type, int code, u_long dest, struct ifnet *destifp)
{
	usn_ip_t         *oip = GETIP(n);
	usn_ip_t         *nip;
	struct icmp      *icp;
	usn_mbuf_t       *m;
	unsigned          oiplen = oip->ip_hl << 2;
	unsigned          icmplen;

   DEBUG("not implemented yet"); return;

#ifdef ICMPPRINTFS
	if (icmpprintfs)
		DEBUG("icmp_error(%p, %d, %d)\n", oip, type, code);
#endif
	if (type != ICMP_REDIRECT)
		;//icmpstat.icps_error++;

	// Don't send error if not the first fragment of message.
	if (oip->ip_off &~ (IP_MF|IP_DF))
		goto freeit;

	// Don't error if the old packet protocol was ICMP
	// error message, only known informational types.
	if (oip->ip_p == IPPROTO_ICMP && 
       type != ICMP_REDIRECT &&
	    n->mlen >= oiplen + ICMP_MINLEN &&
	    !ICMP_INFOTYPE(((struct icmp *)((caddr_t)oip + oiplen))->icmp_type)) {
		//icmpstat.icps_oldicmp++;
		goto freeit;
	}

	// Don't send error in response to a multicast or broadcast packet
	if (n->flags & (BUF_BCAST|BUF_MCAST))
		goto freeit;

	// First, formulate icmp message
	//m = m_gethdr(M_DONTWAIT, MT_HEADER);
   // XXX define max len of icmp messages.
   m = usn_get_mbuf(0, BUF_MSIZE, 0);
	if (m == NULL)
		goto freeit;
	icmplen = oiplen + min(8, oip->ip_len);
	m->mlen = icmplen + ICMP_MINLEN;

	//MH_ALIGN(m, m->mlen); // XXX move all to the end of the buf.

   icp = (struct icmp*)(m->head);
	if ((u_int)type > ICMP_MAXTYPE) {
		DEBUG("panic: icmp_error");
      usn_free_mbuf(m);
      goto freeit;
   }

	//icmpstat.icps_outhist[type]++;
	icp->icmp_type = type;
	if (type == ICMP_REDIRECT)
		icp->icmp_gwaddr.s_addr = dest;
	else {
		icp->icmp_void = 0;
		// The following assignments assume an overlay with the
		// zeroed icmp_void field.
		if (type == ICMP_PARAMPROB) {
			icp->icmp_pptr = code;
			code = 0;
		} else if (type == ICMP_UNREACH &&
			code == ICMP_UNREACH_NEEDFRAG && destifp) {
			icp->icmp_nextmtu = htons(destifp->if_mtu);
		}
	}

	icp->icmp_code = code;
	bcopy((caddr_t)oip, (caddr_t)&icp->icmp_ip, icmplen);
	nip = &icp->icmp_ip;
	nip->ip_len = htons((u_short)(nip->ip_len + oiplen));

	// Now, copy old ip header (without options)
	// in front of icmp message.
	if (m->head - sizeof(usn_ip_t) < m->start) {
		DEBUG("panic: icmp len");
      usn_free_mbuf(m);
      goto freeit;
   }
	m->head -= sizeof(usn_ip_t);
	m->mlen += sizeof(usn_ip_t);
	//m->m_pkthdr.len = m->m_len;
	//m->m_pkthdr.rcvif = n->m_pkthdr.rcvif;
	nip = GETIP(m);

	bcopy((caddr_t)oip, (caddr_t)nip, sizeof(usn_ip_t));
	nip->ip_len = m->mlen;
	nip->ip_hl = sizeof(usn_ip_t) >> 2;
	nip->ip_p = IPPROTO_ICMP;
	nip->ip_tos = 0;

	icmp_reflect(m);

freeit:
	usn_free_mbuf(n);
}

static struct usn_sockaddr_in icmpsrc = { sizeof (struct usn_sockaddr_in), AF_INET };
static struct usn_sockaddr_in icmpdst = { sizeof (struct usn_sockaddr_in), AF_INET };
static struct usn_sockaddr_in icmpgw = { sizeof (struct usn_sockaddr_in), AF_INET };
struct usn_sockaddr_in icmpmask = { 8, 0 };

/*
 * Process a received ICMP message.
 */
void
icmp_input(usn_mbuf_t *m,int hlen)
{
   struct icmp      *icp; 
	struct in_ifaddr *ia;
	//void (*ctlfunc)   (int, struct usn_sockaddr *, struct ip *);
	//extern u_char     ip_protox[];
	usn_ip_t         *ip = GETIP(m);
	u_int             icmplen = ntohs((u_short)ip->ip_len);
	u_int             i;
	int               code;

   //(void)ctlfunc; (void)ip_protox;

	DEBUG("icmp_input: dump info: ptr=%p, len=%d\n", m->head, m->mlen);
   //dump_payload_only((char*)m->head, m->mlen);

   icmpprintfs = 1;
   
#ifdef ICMPPRINTFS
	if (icmpprintfs)
		DEBUG("icmp packet from %x to %x, icmplen=%d\n",
			ip->ip_src.s_addr, ip->ip_dst.s_addr,
			icmplen);
#endif

	// Locate icmp structure in mbuf, and check
	// that not corrupted and of at least minimum length.
	if (icmplen < ICMP_MINLEN) {
		//icmpstat.icps_tooshort++;
		goto freeit;
	}

#define min(x,y) ((x) < (y) ? (x) : (y))
	i = hlen + min(icmplen, ICMP_ADVLENMIN);
	if ( m->mlen < i &&
        (m = m_pullup(m, i)) == 0)  {
		//icmpstat.icps_tooshort++;
      DEBUG("rearrange buffer");
		return;
	}

   // check sum icmp part.
   m->head += hlen;
	m->mlen -= hlen;
   
	icp = (struct icmp *)(m->head);
	if (in_cksum(m, icmplen)) {
		DEBUG("checksum failed");
		//icmpstat.icps_checksum++;
		goto freeit;
	}
	m->head -= hlen;
	m->mlen += hlen;


#ifdef ICMPPRINTFS
	// Message type specific processing.
	if (icmpprintfs)
		DEBUG("icmp_input, type %d code %d\n", icp->icmp_type,
		    icp->icmp_code);
	//DEBUG("icmp_input: (fine) dump info: ptr=%p, len=%d\n", m->head, m->mlen);
   //dump_payload_only((char*)m->head, m->mlen);
#endif

	if (icp->icmp_type > ICMP_MAXTYPE)
		goto raw;
	//icmpstat.icps_inhist[icp->icmp_type]++;
	code = icp->icmp_code;
	switch (icp->icmp_type) {

	case ICMP_UNREACH:
		switch (code) {
			case ICMP_UNREACH_NET:
			case ICMP_UNREACH_HOST:
			case ICMP_UNREACH_PROTOCOL:
			case ICMP_UNREACH_PORT:
			case ICMP_UNREACH_SRCFAIL:
				code += PRC_UNREACH_NET;
				break;

			case ICMP_UNREACH_NEEDFRAG:
				code = PRC_MSGSIZE;
				break;
				
			case ICMP_UNREACH_NET_UNKNOWN:
			case ICMP_UNREACH_NET_PROHIB:
			case ICMP_UNREACH_TOSNET:
				code = PRC_UNREACH_NET;
				break;

			case ICMP_UNREACH_HOST_UNKNOWN:
			case ICMP_UNREACH_ISOLATED:
			case ICMP_UNREACH_HOST_PROHIB:
			case ICMP_UNREACH_TOSHOST:
				code = PRC_UNREACH_HOST;
				break;

			default:
				goto badcode;
		}
		goto deliver;

	case ICMP_TIMXCEED:
		if (code > 1)
			goto badcode;
		code += PRC_TIMXCEED_INTRANS;
		goto deliver;

	case ICMP_PARAMPROB:
		if (code > 1)
			goto badcode;
		code = PRC_PARAMPROB;
		goto deliver;

	case ICMP_SOURCEQUENCH:
		if (code)
			goto badcode;
		code = PRC_QUENCH;
	deliver:
		// Problem with datagram; advise higher level routines.
		if (icmplen < ICMP_ADVLENMIN || 
          icmplen < (u_int)ICMP_ADVLEN(icp) ||
		    icp->icmp_ip.ip_hl < (sizeof(usn_ip_t) >> 2)) {
			//icmpstat.icps_badlen++;
			goto freeit;
		}
#ifdef ICMPPRINTFS
		if (icmpprintfs)
			DEBUG("deliver to protocol %d\n", ntohs(icp->icmp_ip.ip_p));
#endif
		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
		//if (ctlfunc = inetsw[ip_protox[icp->icmp_ip.ip_p]].pr_ctlinput)
		//	(*ctlfunc)(code, (struct sockaddr *)&icmpsrc,
		//	    &icp->icmp_ip);
		break;

	badcode:
		//icmpstat.icps_badcode++;
		break;

	case ICMP_ECHO:
		icp->icmp_type = ICMP_ECHOREPLY;
		goto reflect;

	case ICMP_TSTAMP:
		if (icmplen < ICMP_TSLEN) {
			//icmpstat.icps_badlen++;
			break;
		}
		icp->icmp_type = ICMP_TSTAMPREPLY;
		icp->icmp_rtime = iptime();
		icp->icmp_ttime = icp->icmp_rtime;	// bogus, do later!
		goto reflect;
		
	case ICMP_MASKREQ:
#define	satosin(sa)	((struct usn_sockaddr_in *)(sa))
		if (icmpmaskrepl == 0)
			break;
		// We are not able to respond with all ones broadcast
		// unless we receive it over a point-to-point interface.
		if (icmplen < ICMP_MASKLEN)
			break;
		switch (ip->ip_dst.s_addr) {

		case INADDR_BROADCAST:
		case INADDR_ANY:
			icmpdst.sin_addr = ip->ip_src;
			break;

		default:
			icmpdst.sin_addr = ip->ip_dst;
		}
		ia = (struct in_ifaddr *)ifaof_ifpforaddr(
			    (struct usn_sockaddr *)&icmpdst, NULL);
			    //(struct sockaddr *)&icmpdst, m->m_pkthdr.rcvif);
		if (ia == 0)
			break;
		icp->icmp_type = ICMP_MASKREPLY;
		icp->icmp_mask = ia->ia_sockmask.sin_addr.s_addr;
		if (ip->ip_src.s_addr == 0) {
			if (ia->ia_ifp->if_flags & IFF_BROADCAST)
			    ip->ip_src = satosin(&ia->ia_broadaddr)->sin_addr;
			else if (ia->ia_ifp->if_flags & IFF_POINTOPOINT)
			    ip->ip_src = satosin(&ia->ia_dstaddr)->sin_addr;
		}
reflect:
		ip->ip_len = htons(ntohs(ip->ip_len) + hlen);	// since ip_input deducts this 
		//icmpstat.icps_reflect++;
		//icmpstat.icps_outhist[icp->icmp_type]++;
		icmp_reflect(m);
		return;

	case ICMP_REDIRECT:
		if (code > 3)
			goto badcode;
		if (icmplen < ICMP_ADVLENMIN || 
          icmplen < (u_int)ICMP_ADVLEN(icp) ||
		    icp->icmp_ip.ip_hl < (sizeof(usn_ip_t) >> 2)) {
			//icmpstat.icps_badlen++;
			break;
		}
		// Short circuit routing redirects to force
		// immediate change in the kernel's routing
		// tables.  The message is also handed to anyone
		// listening on a raw socket (e.g. the routing
		// daemon for use in updating its tables).
		icmpgw.sin_addr = ip->ip_src;
		icmpdst.sin_addr = icp->icmp_gwaddr;
#ifdef	ICMPPRINTFS
		if (icmpprintfs)
			DEBUG("redirect dst %x to %x\n", icp->icmp_ip.ip_dst.s_addr,
				icp->icmp_gwaddr.s_addr);
#endif
		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
		rtredirect((struct usn_sockaddr *)&icmpsrc,
		           (struct usn_sockaddr *)&icmpdst,
         		  (struct usn_sockaddr *)NULL, 
                 RTF_GATEWAY | RTF_HOST,
         		  (struct usn_sockaddr *)&icmpgw, 
                 (struct rtentry **)NULL);

		//pfctlinput(PRC_REDIRECT_HOST, (struct usn_sockaddr *)&icmpsrc);
		break;

	// No kernel processing for the following;
	// just fall through to send to raw listener.
	case ICMP_ECHOREPLY:
	case ICMP_ROUTERADVERT:
	case ICMP_ROUTERSOLICIT:
	case ICMP_TSTAMPREPLY:
	case ICMP_IREQREPLY:
	case ICMP_MASKREPLY:
	default:
		break;
	}

raw:
	//rip_input(m);
	return;

freeit:
	usn_free_mbuf(m);
   
}

/*
 * Reflect the ip packet back to the source
 */
void
icmp_reflect(usn_mbuf_t *m)
{
	usn_ip_t             *ip = GETIP(m);
	struct in_ifaddr     *ia;
	struct usn_in_addr    t;
	usn_mbuf_t           *opts = 0, *ip_srcroute();
	int optlen = (ip->ip_hl << 2) - sizeof(usn_ip_t);

	DEBUG("icmp_reflect: dump info: ptr=%p, len=%d, optlen=%d", 
         m->head, m->mlen, optlen);
   //dump_payload_only((char*)m->head, m->mlen);

	if (!in_canforward(ip->ip_src) &&
	    ((ntohl(ip->ip_src.s_addr) & USN_IN_CLASSA_NET) !=
	     (USN_IN_LOOPBACKNET << USN_IN_CLASSA_NSHIFT))) {
      DEBUG("drop icmp packet, ip=%d", ntohl(ip->ip_src.s_addr) );
		usn_free_mbuf(m);	// Bad return address
		goto done;	// Ip_output() will check for broadcast 
	}
	t = ip->ip_dst;
	ip->ip_dst = ip->ip_src;

	// If the incoming packet was addressed directly to us,
	// use dst as the src for the reply.  Otherwise (broadcast
	// or anonymous), use the address which corresponds
	// to the incoming interface.
	for (ia = g_in_ifaddr; ia; ia = ia->ia_next) {
		if (t.s_addr == IA_SIN(ia)->sin_addr.s_addr)
			break;
		if ((ia->ia_ifp->if_flags & USN_IFF_BROADCAST) &&
		    t.s_addr == satosin(&ia->ia_broadaddr)->sin_addr.s_addr)
			break;
	}
	icmpdst.sin_addr = t;
	if (ia == (struct in_ifaddr *)0)
		ia = (struct in_ifaddr *)ifaof_ifpforaddr(
			(struct usn_sockaddr *)&icmpdst, 0);//m->m_pkthdr.rcvif);

	// The following happens if the packet was not addressed to us,
	// and was received on an interface with no IP address.
	if (ia == (struct in_ifaddr *)0)
		ia = g_in_ifaddr;

	t = IA_SIN(ia)->sin_addr;
	ip->ip_src = t;
	ip->ip_ttl = MAXTTL;

	if (optlen > 0) {
		u_char *cp;
		u_int opt, cnt;
		u_int len;

		// Retrieve any source routing from the incoming packet;
		// add on any record-route or timestamp options.
      // XXX: how to get ip_srcroute???
		cp = (u_char *) (ip + 1);
		if (//(opts = ip_srcroute()) == 0 &&
		    (opts = usn_get_mbuf(0, BUF_MSIZE, BUF_DATA))) {
		    //(opts = m_gethdr(M_DONTWAIT, MT_HEADER))) {
			opts->mlen = sizeof(struct usn_in_addr);
			((struct usn_in_addr *)(opts->head))->s_addr = 0;
		}
		if (opts) {
#ifdef ICMPPRINTFS
          if (icmpprintfs)
	          DEBUG("icmp_reflect optlen %d rt %d => ", optlen, opts->mlen);
#endif
		    for (cnt = optlen; cnt > 0; cnt -= len, cp += len) {
			    opt = cp[IPOPT_OPTVAL];
			    if (opt == IPOPT_EOL)
				    break;
			    if (opt == IPOPT_NOP)
				    len = 1;
			    else {
				    len = cp[IPOPT_OLEN];
				    if (len <= 0 || len > cnt)
					    break;
			    }
			    // Should check for overflow, but it "can't happen"
			    if (opt == IPOPT_RR || 
                 opt == IPOPT_TS || 
				     opt == IPOPT_SECURITY) {
				    bcopy((caddr_t)cp,
					       (caddr_t)(opts->head) + opts->mlen, len);
				    opts->mlen += len;
			    }
		    }
		    // Terminate & pad, if necessary 
		    cnt = opts->mlen % 4;
		    if (cnt) {
			    for (; cnt < 4; cnt++) {
				    *( (caddr_t)opts->head  + opts->mlen) = IPOPT_EOL;
				    opts->mlen++;
			    }
		    }
#ifdef ICMPPRINTFS
		    if (icmpprintfs)
			    printf("%d\n", opts->mlen);
#endif
		}
		// Now strip out original options by copying rest of first
		// mbuf's data back, and adjust the IP length.
		ip->ip_len =  htons(ntohs(ip->ip_len) - optlen);
		ip->ip_hl = sizeof(usn_ip_t) >> 2;
		m->mlen -= optlen;

		//if (m->flags & M_PKTHDR)
			//m->m_pkthdr.len -= optlen;

		optlen += sizeof(usn_ip_t);
      // m->mlen have right len???
		bcopy((caddr_t)ip + optlen, (caddr_t)(ip + 1),
			 (unsigned)(m->mlen - sizeof(usn_ip_t)));
	}
	m->flags &= ~(BUF_BCAST|BUF_MCAST);
	icmp_output(m, opts);
done:
	if (opts)
		usn_free_mbuf(opts);

}

/*
 * Send an icmp packet back to the ip level,
 * after supplying a checksum.
 */
void
icmp_output(usn_mbuf_t *m, usn_mbuf_t *opts)
{
	usn_ip_t    *ip = GETIP(m);
	int          hlen;
	struct icmp *icp;

	DEBUG("icmp_output: dump info: ptr=%p, len=%d\n", m->head, m->mlen);
   //dump_payload_only((char*)m->head, m->mlen);

	hlen = ip->ip_hl << 2;
	m->head += hlen;
	m->mlen -= hlen;


	icp = (struct icmp *)m->head;
	icp->icmp_cksum = 0;
	icp->icmp_cksum = in_cksum(m, ntohs(ip->ip_len) - hlen);

	//DEBUG("icmp_output: dump info: ptr=%p, len=%d, icmplen=%d, cksum=%x\n", 
   //        m->head, m->mlen, ntohs(ip->ip_len) - hlen, icp->icmp_cksum);

	m->head -= hlen;
	m->mlen += hlen;

#ifdef ICMPPRINTFS
	if (icmpprintfs)
		DEBUG("icmp_send dst %x src %x\n", ip->ip_dst.s_addr, ip->ip_src.s_addr);
#endif
	ipv4_output(m, opts, NULL, 0);
}

u_long
iptime()
{
	struct timeval atv;
	u_long t;

	//microtime(&atv);
	gettimeofday(&atv, NULL);
	t = (atv.tv_sec % (24*60*60)) * 1000 + atv.tv_usec / 1000;
	return (htonl(t));
}

int
icmp_sysctl(int *name, u_int namelen, void *oldp,
     size_t *oldlenp, void *newp, size_t newlen)
{
   return 0;
   /*
	// All sysctl names at this level are terminal.
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case ICMPCTL_MASKREPL:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &icmpmaskrepl));
	default:
		return (ENOPROTOOPT);
	}
	// NOTREACHED
   */
}
