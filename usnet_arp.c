#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "usnet_core.h"
#include "usnet_arp.h"
#include "usnet_eth.h"
#include "usnet_log.h"
#include "usnet_if.h"
#include "usnet_ip.h"
#include "usnet_radix.h"

arp_cache_t *g_arp_cache;
/* timer values */
int	g_arpt_prune = (5*60*1);	/* walk list every 5 minutes */
int	g_arpt_keep = (20*60);	/* once resolved, good for 20 more minutes */
int	g_arpt_down = 20;		/* once declared down, don't send for 20 secs */
u_int g_arp_maxtries = 5;
struct llinfo_arp g_llinfo_arp;
int	arp_inuse, arp_allocated, arp_intimer;
int	useloopback = 0;	/* use loopback interface for local traffic */

void arpinsque(struct llinfo_arp *la, struct llinfo_arp *head) 
{
   // TODO: better data structure, instead of linked lists.
   struct llinfo_arp *p;
   if ( head == NULL || la == NULL)
      return;
   p = head->la_next;
   head->la_next = la;
   la->la_next = p;
   la->la_prev = head;
}

void arpremque(struct llinfo_arp *la, struct llinfo_arp *head) 
{
   //travel the arp list to remove an item.
   struct llinfo_arp *p, *next, *prev;
   if ( head == NULL || la == NULL)
      return;

   prev = head;
   p = head->la_next;
   while (p) {
      if ( p != la ) {
         prev = p;
         p = p->la_next;
      }
      next = p->la_next;
      prev->la_next = p->la_next;
      next->la_prev = prev;
      usn_free_buf((u_char*)p);
   }
}

void arpinit()
{
   g_llinfo_arp.la_next = &g_llinfo_arp;
   g_llinfo_arp.la_prev = &g_llinfo_arp;

   arp_inuse = 0;
   arp_allocated = 0; 
   arp_intimer = 0;

}
struct llinfo_arp *
arplookup( u_long addr, int create, int proxy)
{
   struct rtentry *rt;
   struct usn_sockaddr_inarp sin = {sizeof(sin), AF_INET };

   sin.sin_addr.s_addr = addr;
   sin.sin_other = proxy ? SIN_PROXY : 0;
   rt = rtalloc8((struct usn_sockaddr *)&sin, create);
   if (rt == 0)
      return (0);
   rt->rt_refcnt--;
   if ((rt->rt_flags & RTF_GATEWAY) || (rt->rt_flags & RTF_LLINFO) == 0 ||
       rt->rt_gateway->sa_family != AF_LINK) {
      if (create)
         DEBUG("arplookup couldn't create %x\n", ntohl(addr));
      return (0);
   }   
   return ((struct llinfo_arp *)rt->rt_llinfo);
}

arp_cache_t* 
arplookup_old(arp_entry_t *ae, int create)
{
   arp_cache_t   *p_ac = NULL; 

   if ( !ae )
      goto out;

   p_ac = g_arp_cache;
   while (p_ac) {
      if ( p_ac->arp.ip_addr == ae->ip_addr )  {
        if ( p_ac->arp.flags & ARP_ENTRY_COMPLETE ) 
          goto found;
        else if ( create ) {
          if ( ae->flags & ARP_ENTRY_COMPLETE ) {
             memcpy(&p_ac->arp, ae, sizeof(*ae));
             goto found;
          }
          goto out;
        }
      }
      p_ac = p_ac->next;
   } 

   if ( !p_ac && create ) {
      // create new entry
      p_ac = (arp_cache_t*) usn_get_buf(0, sizeof(arp_cache_t));
      memcpy(&p_ac->arp, ae, sizeof(*ae));
      //set expired time.
      p_ac->arp.last_time = g_time.tv_sec + g_arpt_keep;
      
      p_ac->prev = g_arp_cache->prev;
      p_ac->next = g_arp_cache;
      g_arp_cache->prev = p_ac;

      g_arp_cache = p_ac;
      goto out;
   }

found:
   if ( p_ac ) {
      if ( bcmp(ae->eth_addr, p_ac->arp.eth_addr, 6) ) {
         DEBUG("arplookup: overwrite hardware address");
         memcpy(p_ac->arp.eth_addr, ae->eth_addr, 6);
      }
      //update expired time.
      if ( p_ac->arp.last_time )
          p_ac->arp.last_time = g_time.tv_sec + g_arpt_keep;
   }
out:
   return p_ac;
}

void 
arpwhohas(struct usn_in_addr *addr)
{
   // TODO: don't hardcode here. 
   //       Using parameter instead (g_ifnet, etc)
   DEBUG("who has: addr=%x", addr->s_addr);
   arprequest(g_ifnet, &g_in_addr, addr, g_ether_addr);
}

void 
arprequest(struct ifnet *ifp, const struct usn_in_addr *sip,
          const struct usn_in_addr *tip, u_char *enaddr)
{
   // the list of arp entries to free expired ones
   usn_mbuf_t *m;
   ether_header_t *eh; 
   struct ether_arp *ea;
   //struct arphdr *ah;
   struct usn_sockaddr sa;
   //u_char *carpaddr = NULL;


   m = usn_get_mbuf(NULL, sizeof(*eh) + sizeof(*ea), 0);
   if ( m == NULL )
     return;

   m->head += sizeof(*eh);
   m->mlen = sizeof(*ea);

   ea = (struct ether_arp *)m->head;
   eh = (ether_header_t *)sa.sa_data;

   memset(ea, 0, sizeof(*ea));
   memcpy(eh->ether_dhost, g_etherbroadcastaddr, sizeof(eh->ether_dhost));

   eh->ether_type = ETHERTYPE_ARP; // who swaps this value in network order?
   ea->arp_hrd = htons(ARPHRD_ETHER);
   ea->arp_pro = htons(ETHERTYPE_IP);
   ea->arp_hln = sizeof(ea->arp_sha);
   ea->arp_pln = sizeof(ea->arp_spa);
   ea->arp_op  = htons(ARPOP_REQUEST);

   memcpy(ea->arp_sha, enaddr, sizeof(ea->arp_sha));
   memcpy(ea->arp_spa, sip, sizeof(ea->arp_spa));
   memcpy(ea->arp_tpa, tip, sizeof(ea->arp_tpa));

   sa.sa_family = AF_UNSPEC;
   sa.sa_len = sizeof(sa);

   // send request
   DEBUG("send arp request");
   eth_output(m, &sa, (struct rtentry*)0 );
   return;
}

/*
 * Free an arp entry.
 */
void
arptfree(struct llinfo_arp *la)
{
	struct rtentry *rt = la->la_rt;
	struct usn_sockaddr_dl *sdl;
	if (rt == 0)
		DEBUG("arptfree: null pointer");
	if (rt->rt_refcnt > 0 && (sdl = SDL(rt->rt_gateway)) &&
	    sdl->sdl_family == AF_LINK) {
		sdl->sdl_alen = 0;
		la->la_asked = 0;
		rt->rt_flags &= ~RTF_REJECT;
		return;
	}
	rtrequest(RTM_DELETE, rt_key(rt), (struct usn_sockaddr *)0, rt_mask(rt),
			0, (struct rtentry **)0);
}

void 
arptimer(void* ignored_arg)
{
   struct llinfo_arp *la = g_llinfo_arp.la_next;
   
   // TODO: implement timeout
   //timeout(arptimer, (caddr_t)0, g_arpt_prune * hz);
   while (la != &g_llinfo_arp) {
      struct rtentry *rt = la->la_rt;
      la = la->la_next;
      if (rt->rt_expire && rt->rt_expire <= (u_long)g_time.tv_sec)
         arptfree(la->la_prev); /* timer has expired, clear */
   }     
   return;
}

void 
arpfree(arp_cache_t *c)
{
   arp_cache_t *p;

   // reclaim an entry if it exists.
   if ( c == NULL )
      return;

   p = c->next; 
   p->prev = c->prev;
   c->prev->next = p;

   usn_free_buf((u_char*)c);
   return;
}

/*
 * Resolve an IP address into an ethernet address.  If success,
 * desten is filled in.  If there is no entry in arptab,
 * set one up and broadcast a request for the IP address.
 * Hold onto this mbuf and resend it once the address
 * is finally resolved.  A return value of 1 indicates
 * that desten has been filled in and the packet should be sent
 * normally; a 0 return indicates that the packet has been
 * taken over here, either now or for later transmission.
 */   
int 
arpresolve(struct rtentry *rt, usn_mbuf_t *m, struct usn_sockaddr *dst, u_char *desten)
{           
   struct llinfo_arp *la;
   struct usn_sockaddr_dl *sdl;

   if ( m == NULL ) {
      DEBUG("m is NULL");
      return 0;
   }

   if (m->flags & BUF_BCAST) {   /* broadcast */
      bcopy((caddr_t)g_etherbroadcastaddr, (caddr_t)desten,
          sizeof(g_etherbroadcastaddr));
      return (1);
   }  

   if (rt)
      la = (struct llinfo_arp *)rt->rt_llinfo;
   else {
      la = arplookup(SIN(dst)->sin_addr.s_addr, 1, 0);
      if (la)
         rt = la->la_rt;
   }
   if (la == 0 || rt == 0) {
      DEBUG("arpresolve: can't allocate llinfo");
      usn_free_mbuf(m);
      return (0);
   }
   sdl = SDL(rt->rt_gateway);
   /*
    * Check the address family and length is valid, the address
    * is resolved; otherwise, try to resolve.
    */
   if ((rt->rt_expire == 0 || rt->rt_expire > (u_long)g_time.tv_sec) &&
       sdl->sdl_family == AF_LINK && sdl->sdl_alen != 0) {
      DEBUG("found ether address");
      bcopy(LLADDR(sdl), desten, sdl->sdl_alen);
      return 1;
   }
   /*
    * There is an arptab entry, but no ethernet address
    * response yet. Replace the held mbuf with this
    * latest one.
    */
   if (la->la_hold)
      usn_free_mbuf(la->la_hold);
   m->flags |= BUF_RAW;
   m->refs++;
   la->la_hold = m;
   if (rt->rt_expire) {
      rt->rt_flags &= ~RTF_REJECT;
      if (la->la_asked == 0 || rt->rt_expire != (u_long)g_time.tv_sec) {
         rt->rt_expire = g_time.tv_sec;
         if (la->la_asked++ < g_arp_maxtries)
            arpwhohas(&(SIN(dst)->sin_addr));
         else {
            rt->rt_flags |= RTF_REJECT;
            rt->rt_expire += g_arpt_down;
            la->la_asked = 0;
         }
      }
   }
   return (0);
}

void
arp_rtrequest(int req, struct rtentry *rt, struct usn_sockaddr *sa)
{
	struct usn_sockaddr *gate = rt->rt_gateway;
	struct llinfo_arp *la = (struct llinfo_arp *)rt->rt_llinfo;
	static struct usn_sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK};

	//if (!arpinit_done) {
	//	arpinit_done = 1;
	//	timeout(arptimer, (caddr_t)0, hz);
	//}

	if (rt->rt_flags & RTF_GATEWAY)
		return;
	switch (req) {

	case RTM_ADD:
		/*
		 * XXX: If this is a manually added route to interface
		 * such as older version of routed or gated might provide,
		 * restore cloning bit.
		 */
		if ((rt->rt_flags & RTF_HOST) == 0 &&
		    SIN(rt_mask(rt))->sin_addr.s_addr != 0xffffffff)
			rt->rt_flags |= RTF_CLONING;
		if (rt->rt_flags & RTF_CLONING) {
			/*
			 * Case 1: This route should come from a route to iface.
			 */
			rt_setgate(rt, rt_key(rt), (struct usn_sockaddr *)&null_sdl);
			gate = rt->rt_gateway;
			SDL(gate)->sdl_type = rt->rt_ifp->if_type;
			SDL(gate)->sdl_index = rt->rt_ifp->if_index;
			rt->rt_expire = g_time.tv_sec;
			break;
		}
		/* Announce a new entry if requested. */
		if (rt->rt_flags & RTF_ANNOUNCE)
			arprequest(rt->rt_ifp,
			    (struct usn_in_addr*)&SIN(rt_key(rt))->sin_addr.s_addr,
			    (struct usn_in_addr*)&SIN(rt_key(rt))->sin_addr.s_addr,
			    (u_char *)LLADDR(SDL(gate)));
		/*FALLTHROUGH*/
	case RTM_RESOLVE:
		if (gate->sa_family != AF_LINK ||
		    gate->sa_len < sizeof(null_sdl)) {
			DEBUG("arp_rtrequest: bad gateway value");
			break;
		}
		SDL(gate)->sdl_type = rt->rt_ifp->if_type;
		SDL(gate)->sdl_index = rt->rt_ifp->if_index;
		if (la != 0)
			break; /* This happens on a route change */
		/*
		 * Case 2:  This route may come from cloning, or a manual route
		 * add with a LL address.
		 */
		R_Malloc(la, struct llinfo_arp *, sizeof(*la));
		rt->rt_llinfo = (caddr_t)la;
		if (la == 0) {
			DEBUG("arp_rtrequest: malloc failed\n");
			break;
		}
		arp_inuse++, arp_allocated++;
		Bzero(la, sizeof(*la));
		la->la_rt = rt;
		rt->rt_flags |= RTF_LLINFO;
		arpinsque(la, &g_llinfo_arp);
		if (SIN(rt_key(rt))->sin_addr.s_addr ==
		    (IA_SIN(rt->rt_ifa))->sin_addr.s_addr) {
		    /*
		     * This test used to be
		     *	if (loif.if_flags & IFF_UP)
		     * It allowed local traffic to be forced
		     * through the hardware by configuring the loopback down.
		     * However, it causes problems during network configuration
		     * for boards that can't receive packets they send.
		     * It is now necessary to clear "useloopback" and remove
		     * the route to force traffic out to the hardware.
		     */
			rt->rt_expire = 0;
			Bcopy(g_ether_addr, LLADDR(SDL(gate)), SDL(gate)->sdl_alen = 6);
			//if (useloopback)
			//	rt->rt_ifp = &loif;

		}
		break;

	case RTM_DELETE:
		if (la == 0)
			break;
		arp_inuse--;
		arpremque(la, &g_llinfo_arp);
		rt->rt_llinfo = 0;
		rt->rt_flags &= ~RTF_LLINFO;
		if (la->la_hold)
			usn_free_mbuf(la->la_hold);
		usn_free_buf((u_char*)la);
	}
}

void
in_arpintr(usn_mbuf_t *m)
{
   struct arphdr *ar;

   if (m == 0) {
      DEBUG("panic: arpintr");
      return;
   }

   if (m->mlen >= sizeof(struct arphdr) &&
       (ar = mtod(m, struct arphdr *)) &&
       ntohs(ar->ar_hrd) == ARPHRD_ETHER &&
       m->mlen >= sizeof(struct arphdr) + 2 * ar->ar_hln + 2 * ar->ar_pln) {
          switch (ntohs(ar->ar_pro)) {
             case ETHERTYPE_IP:
                in_arpinput(m);
             //case ETHERTYPE_IPTRAILERS:
             //   DEBUG("handle_arp: not support iptrailers");
             default:
                DEBUG("handle_arp: not support");
          }   
   }
   if (m) 
      usn_free_mbuf(m);
   
}

void
in_arpinput(usn_mbuf_t *m)
{
   struct usn_sockaddr    sa;
   struct usn_in_addr     isaddr, itaddr, myaddr;
   struct in_ifaddr      *ia, *maybe_ia = 0;
   struct ether_arp      *ea;
   ether_header_t        *eh;
   struct llinfo_arp     *la = 0;
   struct rtentry        *rt;
   struct usn_sockaddr_dl    *sdl;
   //arp_entry_t            ae;
   int op;
   
   DEBUG("dump info, ptr=%p, len=%d", m->head, m->mlen);

   ea = mtod(m, struct ether_arp *);
   op = ntohs(ea->arp_op);
   bcopy((caddr_t)ea->arp_spa, (caddr_t)&isaddr, sizeof (isaddr));
   bcopy((caddr_t)ea->arp_tpa, (caddr_t)&itaddr, sizeof (itaddr));

   // TODO: do it simpler.
   for (ia = g_in_ifaddr; ia; ia = ia->ia_next)
      //if (ia->ia_ifp == &ac->ac_if) 
      {
         maybe_ia = ia;
         if ((itaddr.s_addr == ia->ia_addr.sin_addr.s_addr) ||
              (isaddr.s_addr == ia->ia_addr.sin_addr.s_addr))
            break;
      }  

   if (maybe_ia == 0)
      goto out;

   // XXX why choose maybe_ia.
   myaddr = ia ? ia->ia_addr.sin_addr : maybe_ia->ia_addr.sin_addr;
   
   if (!bcmp((caddr_t)ea->arp_sha, g_ether_addr, sizeof (ea->arp_sha)))
      goto out;   /* it's from me, ignore it. */

   if (!bcmp((caddr_t)ea->arp_sha, g_etherbroadcastaddr,
       sizeof (ea->arp_sha))) {
      DEBUG( "arp: ether address is broadcast for IP address %x!\n",
          ntohl(isaddr.s_addr));
      goto out;
   }

   if (isaddr.s_addr == myaddr.s_addr) {
      DEBUG( "duplicate IP address %x!! sent from ethernet address: %s\n",
         ntohl(isaddr.s_addr), ether_sprintf(ea->arp_sha));
      itaddr = myaddr;
      goto reply;
   }

   la = arplookup(isaddr.s_addr, itaddr.s_addr == myaddr.s_addr, 0);
   if (la && (rt = la->la_rt) && (sdl = SDL(rt->rt_gateway))) {
      if (sdl->sdl_alen &&
          bcmp((caddr_t)ea->arp_sha, LLADDR(sdl), sdl->sdl_alen))
         DEBUG("arp info overwritten for %x by %s\n",
             ntohl(isaddr.s_addr), ether_sprintf(ea->arp_sha));
      bcopy((caddr_t)ea->arp_sha, LLADDR(sdl),
             sdl->sdl_alen = sizeof(ea->arp_sha));
      if (rt->rt_expire)
         rt->rt_expire = g_time.tv_sec + g_arpt_keep;
      rt->rt_flags &= ~RTF_REJECT;
      la->la_asked = 0;
      if (la->la_hold) {
         DEBUG("send a pending packet, ptr=%p, len=%d", 
                 la->la_hold, la->la_hold->mlen);
         eth_output(la->la_hold, rt_key(rt), rt);
         la->la_hold = 0;
      }
   }

reply:
   if (op != ARPOP_REQUEST) {
   out:
      usn_free_mbuf(m);
      return;
   }  
   if (itaddr.s_addr == myaddr.s_addr) {
      /* I am the target */
      bcopy((caddr_t)ea->arp_sha, (caddr_t)ea->arp_tha,
          sizeof(ea->arp_sha));
      bcopy((caddr_t)g_ether_addr, (caddr_t)ea->arp_sha,
          sizeof(ea->arp_sha));
   }
   else {
     /* XXX look for a proxy ARP entry.
      la = arplookup(itaddr.s_addr, 0, SIN_PROXY);
      if (la == NULL)
         goto out;
      rt = la->la_rt;
      bcopy((caddr_t)ea->arp_sha, (caddr_t)ea->arp_tha,
          sizeof(ea->arp_sha));
      sdl = SDL(rt->rt_gateway);
      bcopy(LLADDR(sdl), (caddr_t)ea->arp_sha, sizeof(ea->arp_sha));
     */
   }
   bcopy((caddr_t)ea->arp_spa, (caddr_t)ea->arp_tpa, sizeof(ea->arp_spa));
   bcopy((caddr_t)&itaddr, (caddr_t)ea->arp_spa, sizeof(ea->arp_spa));
   ea->arp_op = htons(ARPOP_REPLY);
   ea->arp_pro = htons(ETHERTYPE_IP); /* let's be sure! */
   eh = (ether_header_t *)sa.sa_data;
   bcopy((caddr_t)ea->arp_tha, (caddr_t)eh->ether_dhost,
       sizeof(eh->ether_dhost));
   eh->ether_type = ETHERTYPE_ARP;
   sa.sa_family = AF_UNSPEC;
   sa.sa_len = sizeof(sa);

   m->mlen = sizeof(*ea);
   // send response out
   eth_output(m, &sa, 0);

   return;
}


