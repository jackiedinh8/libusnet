/*                                                                                                                   
 * Copyright (c) 1982, 1986, 1991, 1993, 1995                                                                        
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
 * @(#)in_pcb.c   8.4 (Berkeley) 5/24/95                                                                             
 */                                      

#include <arpa/inet.h>

#include "usnet_in_pcb.h"
#include "usnet_error.h"
#include "usnet_buf.h"
#include "usnet_if.h"
#include "usnet_route.h"
#include "usnet_socket.h"

struct usn_in_addr g_zeroin_addr;

int 
in_losing (struct inpcb *inp)
{
   struct rtentry *rt;
   struct rt_addrinfo info;

   if ((rt = inp->inp_route.ro_rt)) {
      inp->inp_route.ro_rt = 0;
      bzero((caddr_t)&info, sizeof(info));
      info.rti_info[RTAX_DST] =
         (struct usn_sockaddr *)&inp->inp_route.ro_dst;
      info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
      info.rti_info[RTAX_NETMASK] = rt_mask(rt);

      // FIXME: implement this
      //rt_missmsg(RTM_LOSING, &info, rt->rt_flags, 0);

      if (rt->rt_flags & RTF_DYNAMIC)
         (void) rtrequest(RTM_DELETE, rt_key(rt),
            rt->rt_gateway, rt_mask(rt), rt->rt_flags,
            (struct rtentry **)0);
      else
      /*
       * A new route can be allocated
       * the next time output is attempted.
       */
         rtfree(rt);
   }

   return 0;
}

int 
in_pcballoc (struct usn_socket *so, struct inpcb *head)
{
   struct inpcb *inp;
 
   inp = (struct inpcb*)usn_get_buf(0, sizeof(*inp)); 

   if (inp == NULL) {
      DEBUG("can not alloc inpcb"); 
      return (ENOBUFS);
   }

   bzero((caddr_t)inp, sizeof(*inp));
   inp->inp_head = head;
   inp->inp_socket = so; 

   //insque(inp, head);
   inp->inp_next = head->inp_next;
   head->inp_next = inp;
   inp->inp_prev = head;
   if ( inp->inp_next )
      inp->inp_next->inp_prev = inp;
    
   so->so_pcb = (caddr_t)inp;

   return 0;
}

int 
in_pcblisten(struct inpcb *inp, usn_mbuf_t *nam)
{
   struct usn_socket *so;
   struct usn_appcb  *cb;

   if ( nam == NULL )
      return -1;

   cb = mtod(nam, struct usn_appcb *);
   so = inp->inp_socket;
   so->so_appcb = *cb;
   //inp->inp_flags |= INP_CONTROLOPTS|INP_RECVDSTADDR;
   return 0;
}

int 
in_pcbbind (struct inpcb *inp, usn_mbuf_t *nam)
{
   struct usn_socket *so = inp->inp_socket;
   struct inpcb *head = inp->inp_head;
   struct usn_sockaddr_in *sin;
   u_short lport = 0;
   int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);

   if (g_in_ifaddr == 0)
      return (EADDRNOTAVAIL);

   if (inp->inp_lport || inp->inp_laddr.s_addr != USN_INADDR_ANY)
      return (EINVAL);

   if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 )
      wild = INPLOOKUP_WILDCARD;

   if (nam) {
      sin = mtod(nam, struct usn_sockaddr_in *);
      if (nam->mlen != sizeof (*sin))
         return (EINVAL);

      /*
       * We should check the family, but old programs
       * incorrectly fail to initialize it.
       */
      //if (sin->sin_family != AF_INET)
      //   return (EAFNOSUPPORT);

      lport = sin->sin_port;
      DEBUG("listen on port: %d", lport);
      if (USN_IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
         /*
          * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
          * allow complete duplication of binding if
          * SO_REUSEPORT is set, or if SO_REUSEADDR is set
          * and a multicast address is bound on both
          * new and duplicated sockets.
          */
         if (so->so_options & SO_REUSEADDR)
            reuseport = SO_REUSEADDR|SO_REUSEPORT;
      } else if (sin->sin_addr.s_addr != USN_INADDR_ANY) {
         sin->sin_port = 0;      /* yech... */
         if (ifa_ifwithaddr((struct usn_sockaddr *)sin) == 0)
            return (EADDRNOTAVAIL);
      }
      if (lport) {
         struct inpcb *t;

         if ( ntohs(lport) < IPPORT_RESERVED ) {
            // check admin privileges here, usually with ports < 1024.
            return (EACCES);
         }
         t = in_pcblookup(head, g_zeroin_addr, 0,
                          sin->sin_addr, lport, wild);
         if (t && (reuseport & t->inp_socket->so_options) == 0)
            return (EADDRINUSE);
      }
      inp->inp_laddr = sin->sin_addr;
   }
   if (lport == 0)
      do {
         if (head->inp_lport++ < IPPORT_RESERVED ||
             head->inp_lport > IPPORT_USERRESERVED)
            head->inp_lport = IPPORT_RESERVED;
         lport = htons(head->inp_lport);
      } while (in_pcblookup(head,
             g_zeroin_addr, 0, inp->inp_laddr, lport, wild));
   inp->inp_lport = lport;

   return 0;
}

int in_pcbconnect (struct inpcb *inp, usn_mbuf_t *nam)
{
   struct in_ifaddr *ia = NULL;
   struct usn_sockaddr_in *ifaddr = NULL;
   struct usn_sockaddr_in *sin = mtod(nam, struct usn_sockaddr_in *); 

   if (nam->mlen != sizeof (*sin))
      return (EINVAL);

   if (sin->sin_family != AF_INET)
      return (EAFNOSUPPORT);

   if (sin->sin_port == 0)
      return (EADDRNOTAVAIL);

   if (g_in_ifaddr) {
      /*  
       * If the destination address is INADDR_ANY,
       * use the primary local address.
       * If the supplied address is INADDR_BROADCAST,
       * and the primary interface supports broadcast,
       * choose the broadcast address for that interface.
       */
#define  satosin(sa) ((struct usn_sockaddr_in *)(sa))
#define sintosa(sin) ((struct usn_sockaddr *)(sin))
#define ifatoia(ifa) ((struct in_ifaddr *)(ifa))
      if (sin->sin_addr.s_addr == USN_INADDR_ANY)
          sin->sin_addr = IA_SIN(g_in_ifaddr)->sin_addr;
      else if (sin->sin_addr.s_addr == (u_long)USN_INADDR_BROADCAST &&
        (g_in_ifaddr->ia_ifp->if_flags & IFF_BROADCAST))
          sin->sin_addr = satosin(&g_in_ifaddr->ia_broadaddr)->sin_addr;
   }   
   if (inp->inp_laddr.s_addr == USN_INADDR_ANY) {
      struct route *ro;
      ia = (struct in_ifaddr *)0;
      /* 
       * If route is known or can be allocated now,
       * our src addr is taken from the i/f, else punt.
       */
      ro = &inp->inp_route;
      if (ro->ro_rt &&
          (satosin(&ro->ro_dst)->sin_addr.s_addr !=
         sin->sin_addr.s_addr ||
          inp->inp_socket->so_options & SO_DONTROUTE)) {
         RTFREE(ro->ro_rt);
         ro->ro_rt = (struct rtentry *)0;
      }
      // XXX: we dont do "dont route" things
      if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0
          && (ro->ro_rt == (struct rtentry *)0
          //|| ro->ro_rt->rt_ifp == (struct ifnet *)0
          )) {
         // No route yet, so try to acquire one
         ro->ro_dst.sa_family = AF_INET;
         ro->ro_dst.sa_len = sizeof(struct usn_sockaddr_in);
         ((struct usn_sockaddr_in *) &ro->ro_dst)->sin_addr =
            sin->sin_addr;
         rtalloc(ro);
      }
      

      /*
       * If we found a route, use the address
       * corresponding to the outgoing interface
       * unless it is the loopback (in case a route
       * to our address on another net goes to loopback).
       */

      if (ro->ro_rt 
          && !(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))
         ia = ifatoia(ro->ro_rt->rt_ifa);

      if (ia == 0) {
         u_short fport = sin->sin_port;
         sin->sin_port = 0;
         ia = ifatoia(ifa_ifwithdstaddr(sintosa(sin)));
         if (ia == 0)
            ia = ifatoia(ifa_ifwithnet(sintosa(sin)));
         sin->sin_port = fport;
         if (ia == 0)
            ia = g_in_ifaddr;
         if (ia == 0)
            return (EADDRNOTAVAIL);
      }
      /*
       * If the destination address is multicast and an outgoing
       * interface has been set as a multicast option, use the
       * address of that interface as our source address.
       */
      /*
      if (USN_IN_MULTICAST(ntohl(sin->sin_addr.s_addr))
          //&& inp->inp_moptions != NULL) 
         ) {
         struct ip_moptions *imo;
         struct ifnet *ifp;
         imo = inp->inp_moptions;
         if (imo->imo_multicast_ifp != NULL) {
            ifp = imo->imo_multicast_ifp;
            for (ia = in_ifaddr; ia; ia = ia->ia_next)
               if (ia->ia_ifp == ifp)
                  break;
            if (ia == 0)
               return (EADDRNOTAVAIL);
         }
      }
      */
      ifaddr = (struct usn_sockaddr_in *)&ia->ia_addr;
   }
   if (in_pcblookup(inp->inp_head,
       sin->sin_addr,
       sin->sin_port,
       inp->inp_laddr.s_addr ? inp->inp_laddr : ifaddr->sin_addr,
       inp->inp_lport,
       0))
      return (EADDRINUSE);
   if (inp->inp_laddr.s_addr == USN_INADDR_ANY) {
      if (inp->inp_lport == 0)
         (void)in_pcbbind(inp, (usn_mbuf_t *)0);
      inp->inp_laddr = ifaddr->sin_addr;
   }
   inp->inp_faddr = sin->sin_addr;
   inp->inp_fport = sin->sin_port;   

   return 0;
}

int in_pcbdetach (struct inpcb *inp)
{
   struct usn_socket *so = inp->inp_socket;

   so->so_pcb = 0;
   //sofree(so);

   if (inp->inp_options)
      (void)usn_free_mbuf(inp->inp_options);

   if (inp->inp_route.ro_rt)
      rtfree(inp->inp_route.ro_rt);

   // multicast options
   //ip_freemoptions(inp->inp_moptions);

   //remque(inp);
   inp->inp_next->inp_prev = inp->inp_prev;
   inp->inp_prev->inp_next = inp->inp_next;

   usn_free_buf((u_char*)inp);
   return 0;
}

int in_pcbdisconnect (struct inpcb *inp)
{
   inp->inp_faddr.s_addr = USN_INADDR_ANY;
   inp->inp_fport = 0;
   if (inp->inp_socket->so_state & USN_NOFDREF)
      in_pcbdetach(inp);
   return 0;
}

struct inpcb* 
in_pcblookup (
   struct inpcb *head, 
   struct usn_in_addr faddr, 
   u_int fport_arg, 
   struct usn_in_addr laddr, 
   u_int lport_arg, 
   int flags)
{
   struct inpcb *inp, *match = 0;
   int matchwild = 3, wildcard;
   u_short fport = fport_arg, lport = lport_arg;
  
   for (inp = head->inp_next; inp != head; inp = inp->inp_next) {
      if (inp->inp_lport != lport)
         continue;
      wildcard = 0;
      if (inp->inp_laddr.s_addr != USN_INADDR_ANY) {
         if (laddr.s_addr == USN_INADDR_ANY)
            wildcard++;
         else if (inp->inp_laddr.s_addr != laddr.s_addr)
            continue;
      } else {
         if (laddr.s_addr != USN_INADDR_ANY)
            wildcard++;
      }   
      if (inp->inp_faddr.s_addr != USN_INADDR_ANY) {
         if (faddr.s_addr == USN_INADDR_ANY)
            wildcard++;
         else if (inp->inp_faddr.s_addr != faddr.s_addr ||
             inp->inp_fport != fport)
            continue;
      } else {
         if (faddr.s_addr != USN_INADDR_ANY)
            wildcard++;
      }   
      if (wildcard && (flags & INPLOOKUP_WILDCARD) == 0)
         continue;
      if (wildcard < matchwild) {
         match = inp;
         matchwild = wildcard;
         if (matchwild == 0)
            break;
      }   
   }   
   DEBUG("laddr=%x, lport=%d, faddr=%x, fport=%d, is_matched=%d", 
         laddr.s_addr, lport, faddr.s_addr, fport, match? 1:0);
   return (match); 
}

u_char g_inetctlerrmap[PRC_NCMDS] = { 
   0,    0,    0,    0,
   0,    EMSGSIZE,   EHOSTDOWN,  EHOSTUNREACH,
   EHOSTUNREACH,  EHOSTUNREACH,  ECONNREFUSED,  ECONNREFUSED,
   EMSGSIZE,   EHOSTUNREACH,  0,    0,
   0,    0,    0,    0,
   ENOPROTOOPT
};   


int
in_pcbnotify (
   struct inpcb *head, 
   struct usn_sockaddr *dst,
	u_int fport_arg, 
   struct usn_in_addr laddr, 
	u_int lport_arg, 
   int cmd, 
   //void (*notify)(struct inpcb *, int))
   notify_func_t notify)
{
   struct inpcb *inp, *oinp;
   struct usn_in_addr faddr;
   u_short fport = fport_arg, lport = lport_arg;
   int errno;

   if ((unsigned)cmd > PRC_NCMDS || dst->sa_family != AF_INET)
      return 0;
   faddr = ((struct usn_sockaddr_in *)dst)->sin_addr;
   if (faddr.s_addr == USN_INADDR_ANY)
      return 0;

   /*  
    * Redirects go to all references to the destination,
    * and use in_rtchange to invalidate the route cache.
    * Dead host indications: notify all references to the destination.
    * Otherwise, if we have knowledge of the local port and address,
    * deliver only to that socket.
    */
   if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
      fport = 0;
      lport = 0;
      laddr.s_addr = 0;
      if (cmd != PRC_HOSTDEAD) // XXX: never see this error.
         notify = (notify_func_t)in_rtchange;
   }   
   errno = g_inetctlerrmap[cmd];
   for (inp = head->inp_next; inp != head;) {
      if (inp->inp_faddr.s_addr != faddr.s_addr ||
          inp->inp_socket == 0 ||
          (lport && inp->inp_lport != lport) ||
          (laddr.s_addr && inp->inp_laddr.s_addr != laddr.s_addr) ||
          (fport && inp->inp_fport != fport)) {
         inp = inp->inp_next;
         continue;
      }
      oinp = inp;
      inp = inp->inp_next;
      if (notify)
         (*notify)(oinp, errno);
   }

   return 0;
}

void	 
in_rtchange (struct inpcb *inp, int errno)
{
   if (inp->inp_route.ro_rt) {
      rtfree(inp->inp_route.ro_rt);
      inp->inp_route.ro_rt = 0;
      /*
       * A new route can be allocated the next time
       * output is attempted.
       */
   }
   return;
}

int
in_setpeeraddr (struct inpcb *inp, usn_mbuf_t *nam)
{
   struct usn_sockaddr_in *sin;
   
   nam->mlen = sizeof (*sin);
   sin = mtod(nam, struct usn_sockaddr_in *); 
   bzero((caddr_t)sin, sizeof (*sin));
   sin->sin_family = AF_INET;
   sin->sin_len = sizeof(*sin);
   sin->sin_port = inp->inp_fport;
   sin->sin_addr = inp->inp_faddr;
   return 0;
}

int
in_setsockaddr (struct inpcb *inp, usn_mbuf_t *nam)
{
   // XXX: no need mbuf, just pure buffer
   struct usn_sockaddr_in *sin;
   
   nam->mlen = sizeof (*sin);
   sin = mtod(nam, struct usn_sockaddr_in *); 
   bzero((caddr_t)sin, sizeof (*sin));
   sin->sin_family = AF_INET;
   sin->sin_len = sizeof(*sin);
   sin->sin_port = inp->inp_lport;
   sin->sin_addr = inp->inp_laddr;
   return 0;
}


