#include <strings.h>
#include <net/if.h>

#include "usnet_route.h"
#include "usnet_log.h"
#include "usnet_if.h"
#include "usnet_radix.h"
#include "usnet_buf.h"
#include "usnet_arp.h"

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define  SA(p) ((struct usn_sockaddr *)(p))

struct usn_radix_node_head *g_rt_tables[AF_MAX+1];
struct rtstat               g_rtstat;
int                         g_rttrash;    /* routes not in table but not freed */ 

struct rtentry *
rtalloc8(struct usn_sockaddr *dst, int report)
{  
   struct usn_radix_node_head *rnh = g_rt_tables[dst->sa_family];
   struct rtentry *rt;
   struct usn_radix_node *rn;
   struct rtentry *newrt = 0;
   struct rt_addrinfo info; 
   int err = 0; 
   int msgtype = RTM_MISS;
  
   (void)msgtype;

   //dump_payload_only((char*)dst, sizeof(*dst));

   if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
       ((rn->rn_flags & RNF_ROOT) == 0)) {
      newrt = rt = (struct rtentry *)rn;
      if (report && (rt->rt_flags & RTF_CLONING)) {
         err = rtrequest(RTM_RESOLVE, dst, SA(0),
                     SA(0), 0, &newrt);
         if (err) { 
            newrt = rt;
            rt->rt_refcnt++;
            DEBUG("missing info, err=%d", err);
            goto miss;
         }
         if ((rt = newrt) && (rt->rt_flags & RTF_XRESOLVE)) {
            msgtype = RTM_RESOLVE;
            DEBUG("missing info, rt_flags=%d", rt->rt_flags);
            goto miss;
         }
      } else
         rt->rt_refcnt++;
   } else {
      g_rtstat.rts_unreach++;
   miss: 
      if (report) {
         bzero((caddr_t)&info, sizeof(info));
         info.rti_info[RTAX_DST] = dst;
         //XXX implement this
         //rt_missmsg(msgtype, &info, 0, err);
      }
   }
   return (newrt);
}


struct ifaddr *
ifa_ifwithroute( int flags,
   struct usn_sockaddr   *dst,
   struct usn_sockaddr   *gateway)
{
   struct ifaddr *ifa;
   if ((flags & RTF_GATEWAY) == 0) {
      /*  
       * If we are adding a route to an interface,
       * and the interface is a pt to pt link
       * we should search for the destination
       * as our clue to the interface.  Otherwise
       * we can use the local address.
       */
      ifa = 0;
      if (flags & RTF_HOST) 
         ifa = ifa_ifwithdstaddr(dst);
      if (ifa == 0)
         ifa = ifa_ifwithaddr(gateway);
   } else {
      /*  
       * If we are adding a route to a remote net
       * or host, the gateway may still be on the
       * other end of a pt to pt link.
       */
      ifa = ifa_ifwithdstaddr(gateway);
   }   
   if (ifa == 0)
      ifa = ifa_ifwithnet(gateway);
   if (ifa == 0) {
      struct rtentry *rt = rtalloc8(dst, 0); 
      if (rt == 0)
         return (0);
      rt->rt_refcnt--;
      if ((ifa = rt->rt_ifa) == 0)
         return (0);
   }   
   if (ifa->ifa_addr->sa_family != dst->sa_family) {
      struct ifaddr *oifa = ifa;
      ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
      if (ifa == 0)
         ifa = oifa;
   }     
   return (ifa);
}   

void
rt_maskedcopy(
   struct usn_sockaddr *src,
   struct usn_sockaddr *dst,
   struct usn_sockaddr *netmask)
{
   register u_char *cp1 = (u_char *)src;
   register u_char *cp2 = (u_char *)dst;
   register u_char *cp3 = (u_char *)netmask;
   u_char *cplim = cp2 + *cp3;
   u_char *cplim2 = cp2 + *cp1;

   *cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
   cp3 += 2;
   if (cplim > cplim2)
      cplim = cplim2;
   while (cp2 < cplim)
      *cp2++ = *cp1++ & *cp3++;
   if (cp2 < cplim2)
      bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}

int
rtrequest(
   int req, 
   struct usn_sockaddr *dst,
   struct usn_sockaddr *gateway,
   struct usn_sockaddr *netmask,
   int    flags,
   struct rtentry **ret_nrt)
{
   int error = 0;
   struct rtentry *rt;
   struct usn_radix_node *rn;
   struct usn_radix_node_head *rnh;
   struct ifaddr *ifa;
   struct usn_sockaddr *ndst;
#define senderr(x) { g_errno = x ; goto bad; }

   DEBUG("dst->sa_family=%d", dst->sa_family);
   if ((rnh = g_rt_tables[dst->sa_family]) == 0)
      senderr(ESRCH);
   if (flags & RTF_HOST)
      netmask = 0;
   switch (req) {
   case RTM_DELETE:
      if ((rn = rnh->rnh_deladdr(dst, netmask, rnh)) == 0)
         senderr(ESRCH);
      if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
         DEBUG("rtrequest delete");
      rt = (struct rtentry *)rn;
      rt->rt_flags &= ~RTF_UP;
      if (rt->rt_gwroute) {
         rt = rt->rt_gwroute; RTFREE(rt);
         (rt = (struct rtentry *)rn)->rt_gwroute = 0;
      }

      // XXX implement this: 
      // used by ARP to delete corresponding ARP entry.
      //if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
      //  ifa->ifa_rtrequest(RTM_DELETE, rt, SA(0));
      arp_rtrequest(RTM_DELETE, rt, SA(0));

      g_rttrash++;
      if (ret_nrt)
         *ret_nrt = rt;
      else if (rt->rt_refcnt <= 0) {
         rt->rt_refcnt++;
         rtfree(rt);
      }
      break;

   case RTM_RESOLVE:
      if (ret_nrt == 0 || (rt = *ret_nrt) == 0)
         senderr(EINVAL);
      ifa = rt->rt_ifa;
      flags = rt->rt_flags & ~RTF_CLONING;
      gateway = rt->rt_gateway;
      if ((netmask = rt->rt_genmask) == 0)
         flags |= RTF_HOST;
      goto makeroute;

   case RTM_ADD:
      DEBUG("add a route, flags=%d, dst=%x, gateway=%x", 
             flags, (((struct usn_sockaddr_in *)dst)->sin_addr.s_addr),
             (((struct usn_sockaddr_in *)gateway)->sin_addr.s_addr));
             
      if ((ifa = ifa_ifwithroute(flags, dst, gateway)) == 0)
         senderr(ENETUNREACH);
   makeroute:
      R_Malloc(rt, struct rtentry *, sizeof(*rt));
      if (rt == 0)
         senderr(ENOBUFS);
      Bzero(rt, sizeof(*rt));
      rt->rt_flags = RTF_UP | flags;
      if (rt_setgate(rt, dst, gateway)) {
         Free(rt);
         senderr(ENOBUFS);
      }

      ndst = rt_key(rt);
      if (netmask) {
         rt_maskedcopy(dst, ndst, netmask);
      } else
         Bcopy(dst, ndst, dst->sa_len);
      rn = rnh->rnh_addaddr((caddr_t)ndst, (caddr_t)netmask,
               rnh, rt->rt_nodes);
      if (rn == 0) {
         if (rt->rt_gwroute)
            rtfree(rt->rt_gwroute);
         Free(rt_key(rt));
         Free(rt);
         senderr(EEXIST);
      }
      ifa->ifa_refcnt++;
      rt->rt_ifa = ifa;
      rt->rt_ifp = ifa->ifa_ifp;
      if (req == RTM_RESOLVE)
         rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */

      // TODO: fix me, used by ARP.
      //if (ifa->ifa_rtrequest)
      //ifa->ifa_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt : 0));
      arp_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt : 0));

      if (ret_nrt) {
         *ret_nrt = rt;
         rt->rt_refcnt++;
      }
      break;
   }
bad:
   //splx(s);
   return (error);
}

int      
rt_setgate(
   struct rtentry *rt0,
   struct usn_sockaddr *dst,
   struct usn_sockaddr *gate)
{            
   caddr_t anew, old;
   int dlen = ROUNDUP(dst->sa_len);
   int glen = ROUNDUP(gate->sa_len);
   struct rtentry *rt = rt0;
      
   if (rt->rt_gateway == 0 || glen > (int)ROUNDUP(rt->rt_gateway->sa_len)) {
      old = (caddr_t)rt_key(rt);
      R_Malloc(anew, caddr_t, dlen + glen);
      if (anew == 0)
         return 1;
      rt->rt_nodes->rn_key = anew;
   } else {
      anew = rt->rt_nodes->rn_key;
      old = 0;
   }
   Bcopy(gate, (rt->rt_gateway = (struct usn_sockaddr *)(anew + dlen)), glen);
   if (old) {
      Bcopy(dst, anew, dlen);
      Free(old);
   }
   if (rt->rt_gwroute) {
      rt = rt->rt_gwroute; RTFREE(rt);
      rt = rt0; rt->rt_gwroute = 0;
   }
   if (rt->rt_flags & RTF_GATEWAY) {
      rt->rt_gwroute = rtalloc8(gate, 1);
   }
   return 0;
}


/*
 * Packet routing routines.
 */



void
rtalloc(struct route *ro)
{  
   if (ro->ro_rt && 
       ro->ro_rt->rt_ifp && 
       (ro->ro_rt->rt_flags & RTF_UP))
      return;            /* XXX */
   DEBUG("search for a route");
   ro->ro_rt = rtalloc8(&ro->ro_dst, 1);
}

void
ifafree( struct ifaddr *ifa)
{
   if (ifa == NULL)
      DEBUG("ifafree is NULL");
   if (ifa->ifa_refcnt == 0) {
      // XXX Implement this
      ;//free(ifa, M_IFADDR);
   } else {
      ifa->ifa_refcnt--;
   }
}



void
rtfree( struct rtentry *rt)
{
   struct ifaddr *ifa;

   if (rt == 0)
      DEBUG("rtfree");

   rt->rt_refcnt--;
   if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
      if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
         DEBUG("rtfree 2");
      g_rttrash--;
      if (rt->rt_refcnt < 0) {
         DEBUG("rtfree: %p not freed (neg refs)\n", rt);
         return;
      }
      ifa = rt->rt_ifa;
      IFAFREE(ifa);
      Free(rt_key(rt));
      Free(rt);
   }   
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 *
 * N.B.: must be called at splnet
 *
 */
int
rtredirect(
   struct usn_sockaddr *dst,
   struct usn_sockaddr *gateway,
   struct usn_sockaddr *netmask,
   int    flags,
   struct usn_sockaddr *src,
   struct rtentry **rtp)
{
   register struct rtentry *rt;
   int error = 0;
   short *stat = 0;
   struct rt_addrinfo info;
   struct ifaddr *ifa;

   /* verify the gateway is directly reachable */
   if ((ifa = ifa_ifwithnet(gateway)) == 0) {
      error = ENETUNREACH;
      goto out;
   }   
   rt = rtalloc8(dst, 0); 
   /*  
    * If the redirect isn't from our current router for this dst,
    * it's either old or wrong.  If it redirects us to ourselves,
    * we have a routing loop, perhaps as a result of an interface
    * going down recently.
    */
#define  equal(a1, a2) (bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
   if (!(flags & RTF_DONE) && rt &&
        (!equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
      error = EINVAL;
   else if (ifa_ifwithaddr(gateway))
      error = EHOSTUNREACH;
   if (error)
      goto done;
   /*
    * Create a new entry if we just got back a wildcard entry
    * or the the lookup failed.  This is necessary for hosts
    * which use routing redirects generated by smart gateways
    * to dynamically build the routing tables.
    */
   if ((rt == 0) || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
      goto create;
   /*
    * Don't listen to the redirect if it's
    * for a route to an interface. 
    */
   if (rt->rt_flags & RTF_GATEWAY) {
      if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)) {
         /*
          * Changing from route to net => route to host.
          * Create new route, rather than smashing route to net.
          */
      create:
         flags |=  RTF_GATEWAY | RTF_DYNAMIC;
         error = rtrequest((int)RTM_ADD, dst, gateway,
                netmask, flags,
                (struct rtentry **)0);
         //stat = &rtstat.rts_dynamic;
      } else {
         /*
          * Smash the current notion of the gateway to
          * this destination.  Should check about netmask!!!
          */
         rt->rt_flags |= RTF_MODIFIED;
         flags |= RTF_MODIFIED;
         //stat = &rtstat.rts_newgateway;
         rt_setgate(rt, rt_key(rt), gateway);
      }
   } else
      error = EHOSTUNREACH;
done:
   if (rt) {
      if (rtp && !error)
         *rtp = rt;
      else
         rtfree(rt);
   }
out:
   if (error)
      ;//rtstat.rts_badredirect++;
   else if (stat != NULL)
      (*stat)++;
   bzero((caddr_t)&info, sizeof(info));
   info.rti_info[RTAX_DST] = dst;
   info.rti_info[RTAX_GATEWAY] = gateway;
   info.rti_info[RTAX_NETMASK] = netmask;
   info.rti_info[RTAX_AUTHOR] = src;
   //rt_missmsg(RTM_REDIRECT, &info, flags, error);
   return 0;
}


void
rtable_init(void **table)
{  
   int rtoffset = 32; // for AF_INET domain.
   DEBUG("rtable_init: start");
   rn1_inithead(&table[AF_INET], rtoffset);
   print_radix_head((struct usn_radix_node_head *)table[AF_INET]);
   DEBUG("rtable_init: done");
/*
   struct domain *dom;
   for (dom = domains; dom; dom = dom->dom_next)
      if (dom->dom_rtattach)
         dom->dom_rtattach(&table[dom->dom_family],
             dom->dom_rtoffset);
*/
}

/*
 * Set up a routing table entry, normally
 * for an interface.
 */
int
rtinit( struct ifaddr *ifa, int cmd, int flags)
{
   struct rtentry *rt = 0;
   struct usn_sockaddr *dst = 0;
   struct usn_sockaddr *deldst = 0;
   struct rtentry *nrt = 0;
   int error;

#ifdef DUMP_PAYLOAD
   DEBUG("rtinit: start, flags=%d", flags);
   dump_buffer((char*)dst, sizeof(*dst), "dst");
   dump_buffer((char*)ifa->ifa_addr, sizeof(*dst), "adr");
#endif

   dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;
   if (cmd == RTM_DELETE) {
      if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
         deldst = (struct usn_sockaddr *)usn_get_buf(0, sizeof(*dst));
         rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
         dst = deldst;
      }   
      rt = rtalloc8(dst, 0);
      if (rt) {
         rt->rt_refcnt--;
         if (rt->rt_ifa != ifa) {
            if (deldst) 
               (void) usn_free_buf((u_char*)deldst);
            return (flags & RTF_HOST ? EHOSTUNREACH
                     : ENETUNREACH);
         }   
      }   
   }   

#ifdef DUMP_PAYLOAD
   dump_buffer((char*)ifa->ifa_netmask, sizeof(*ifa->ifa_netmask), "msk");
#endif

   error = rtrequest(cmd, dst, ifa->ifa_addr, ifa->ifa_netmask,
         flags | ifa->ifa_flags, &nrt);
   if (deldst) 
      (void) usn_free_buf((u_char*)deldst);

   if (cmd == RTM_DELETE && error == 0 && (rt = nrt)) {
      // XXX: fix me
      //rt_newaddrmsg(cmd, ifa, error, nrt);
      if (rt->rt_refcnt <= 0) {
         rt->rt_refcnt++;
         rtfree(rt);
      }
   }

   DEBUG("rtrequest result, cmd=%d, ret=%d, rt=%p, nrt=%p", cmd, error, rt, nrt);

   if (cmd == RTM_ADD && error == 0 && (rt = nrt)) {
      rt->rt_refcnt--;
      if (rt->rt_ifa != ifa) {
         DEBUG("rtinit: wrong ifa (%p) was (%p)\n", ifa, rt->rt_ifa);

         // XXX: fix me
         //if (rt->rt_ifa->ifa_rtrequest)
         //    rt->rt_ifa->ifa_rtrequest(RTM_DELETE, rt, SA(0));
         arp_rtrequest(RTM_DELETE, rt, SA(0));

         IFAFREE(rt->rt_ifa);
         rt->rt_ifa = ifa;
         rt->rt_ifp = ifa->ifa_ifp;
         ifa->ifa_refcnt++;

         // XXX: fix me
         //if (ifa->ifa_rtrequest)
         //    ifa->ifa_rtrequest(RTM_ADD, rt, SA(0));
         arp_rtrequest(RTM_ADD, rt, SA(0));
      }
      // XXX: fix me
      //rt_newaddrmsg(cmd, ifa, error, nrt);
   }
   DEBUG("rtinit: done");
   return (error);
}

void
usnet_route_init()
{
   //int error; (void)error;

   DEBUG("route_init: start");
   rn1_init();  /* initialize all zeroes, all ones, mask table */
   rtable_init((void **)g_rt_tables);
   DEBUG("route_init: rtinit");

   //error = rtinit(g_ifaddr,RTM_ADD,0);
   //print_radix_head((struct usn_radix_node_head *)g_rt_tables[AF_INET]);
   //DEBUG("route_init: rtinit, ret=%d", error);
}



