#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>


#include "usnet_if.h"
#include "usnet_eth.h"
#include "usnet_in.h"
#include "usnet_log.h"
#include "usnet_if_dl.h"
#include "usnet_radix.h"
#include "usnet_arp.h"
#include "usnet_route.h"

/*
 * Locate an interface based on a complete address.
 */
struct usn_sockaddr nm_ip_addr = { sizeof(nm_ip_addr), AF_INET};
struct in_ifaddr *g_in_ifaddr;
struct ifaddr    *g_ifaddr;
struct ifaddr   **g_ifnet_addrs;
struct ifnet     *g_ifnet;

#define  IFT_ETHER   0x6
#define  ETHERMTU 1500

/*
 * Trim a mask in a sockaddr
 */
void
in_socktrim( struct usn_sockaddr_in *ap)
{
    char *cplim = (char *) &ap->sin_addr;
    char *cp = (char *) (&ap->sin_addr + 1);

    ap->sin_len = 0;
    while (--cp >= cplim)
        if (*cp) {
           (ap)->sin_len = cp - (char *) (ap) + 1;
           break;
        }
}

int get_mac_addr(char* mac_str)
{
   int i;
   int lg_ether_addr[6];
   if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x", 
               &lg_ether_addr[0],
               &lg_ether_addr[1],
               &lg_ether_addr[2],
               &lg_ether_addr[3],
               &lg_ether_addr[4],
               &lg_ether_addr[5]) != 6 ) {
        DEBUG("could not get mac addr: %s", mac_str);
   }
   for (i=0; i < 6; i++) {
      g_ether_addr[i] = (char) lg_ether_addr[i];
   }
   return 0;
}

void
init_network()
{
   size_t socksize, ifasize;
   int    namelen, unitlen, masklen;
   struct usn_sockaddr_dl *sdl;
   char   *unitname;
   char   *if_name;
   struct ifaddr    *ifa;
   struct in_ifaddr *ia;
   struct usn_sockaddr_in sin;
   int    i = 0xffffffff;
   int    flags = RTF_UP;
   int    error;


   DEBUG("init_network: start");

   *((int*)g_ip_addr) = inet_addr(g_address);
   g_in_addr.s_addr = *((u_int32*)g_ip_addr);
   DEBUG("ip address: %x", *((int*)g_ip_addr));

   get_mac_addr(g_macaddress); 
   printf("mac addr: ");
   for (i=0; i < 6; i++) {
      printf("%2x ", g_ether_addr[i]);
   }
   printf("\n");


   arpinit();

   if_name = (char*)malloc(6);
   if ( if_name == NULL ) {
      DEBUG("Error: failed to init if_name");
      return;
   }
   bzero((caddr_t)if_name, 6);
   if_name[0] = 'e';
   if_name[1] = 'm';
 
   unitname = if_name + 4;
   unitname[0] = '1';
   

   // ifnet contain information for each network device.
   g_ifnet = (struct ifnet*)malloc(sizeof(struct ifnet));
   bzero((caddr_t)g_ifnet, sizeof(*g_ifnet));
   g_ifnet->if_flags |= USN_IFF_BROADCAST | USN_IFF_UP | USN_IFF_RUNNING;
   g_ifnet->if_addrlist = NULL;
   g_ifnet->if_name = if_name;
   g_ifnet->if_type = IFT_ETHER;
   g_ifnet->if_mtu = ETHERMTU;
   g_ifnet->if_addrlen = 6;
   g_ifnet->if_hdrlen = 14;
   g_ifnet->if_metric = 0;

#define ROUNDUP(a) (1 + (((a) - 1) | (sizeof(long) - 1)))
   namelen = strlen(g_ifnet->if_name);
   unitlen = strlen(unitname);
   masklen = 8 + namelen + unitlen;
   socksize = masklen + g_ifnet->if_addrlen;
   socksize = ROUNDUP(socksize);

   if ( socksize < sizeof(*sdl) )
        socksize = sizeof(*sdl);

   ifasize = sizeof(*g_ifaddr) + 2*socksize;

   g_ifnet_addrs = (struct ifaddr **)malloc( sizeof(struct ifaddr *) * 10);

   // from if_attach()
   g_ifaddr = (struct ifaddr*) malloc(ifasize);

   if ( g_ifaddr == NULL ) {
      DEBUG("Error: failed to init g_ifaddr");
      return;
   }

   bzero((caddr_t)g_ifaddr, ifasize);
   // First: initialize sockaddr_dl address.
   sdl = (struct usn_sockaddr_dl *)(g_ifaddr + 1); 
   g_ifaddr->ifa_addr = (struct usn_sockaddr *)sdl;

   sdl->sdl_len = socksize;
   sdl->sdl_family = AF_LINK;
   bcopy(g_ifnet->if_name, sdl->sdl_data, namelen);
   bcopy(unitname, namelen + (caddr_t)sdl->sdl_data, unitlen);
   sdl->sdl_nlen = (namelen += unitlen);
   sdl->sdl_index = 1;
   sdl->sdl_type = g_ifnet->if_type;
   g_ifnet_addrs[sdl->sdl_index - 1] = g_ifaddr;
   g_ifaddr->ifa_ifp = g_ifnet;
   g_ifaddr->ifa_next = g_ifnet->if_addrlist;
   g_ifnet->if_addrlist = g_ifaddr;
   g_ifaddr->ifa_addr = (struct usn_sockaddr *)sdl;

   // from ether_attach()
   sdl->sdl_alen = g_ifnet->if_addrlen;
   //bcopy((caddr_t)arpcom->ac_enaddr, LLADDR(sdl), g_ifnet->if_addrlen);

   // Second: initialize sockaddr_dl mask 
   sdl = (struct usn_sockaddr_dl *)(socksize + (caddr_t)sdl);
   g_ifaddr->ifa_netmask = (struct usn_sockaddr *)sdl;
   sdl->sdl_len = masklen;
   while (namelen != 0)
      sdl->sdl_data[--namelen] = 0xff;



   // Initialize internet addresses. from in_control()
   g_in_ifaddr = (struct in_ifaddr*)malloc(sizeof(struct in_ifaddr));
   if ( g_in_ifaddr == NULL ) {
      DEBUG("Error: failed to init g_in_ifaddr");
      return;
   }
   memset(g_in_ifaddr, 0, sizeof(g_in_ifaddr));
   g_in_ifaddr->ia_ifa.ifa_next = g_ifaddr->ifa_next;
   g_ifaddr->ifa_next = (struct ifaddr*)g_in_ifaddr;

   ia  = g_in_ifaddr;
   ia->ia_ifp = g_ifnet;
   ifa = &g_in_ifaddr->ia_ifa;
   ifa->ifa_ifp = g_ifnet;

   ia->ia_ifa.ifa_addr = (struct usn_sockaddr *)&ia->ia_addr;
   ia->ia_ifa.ifa_dstaddr = (struct usn_sockaddr *)&ia->ia_dstaddr;
   ia->ia_ifa.ifa_netmask = (struct usn_sockaddr *)&ia->ia_sockmask;
   ia->ia_sockmask.sin_len = 8;
   if (g_ifnet->if_flags & USN_IFF_BROADCAST) {
      ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
      ia->ia_broadaddr.sin_family = AF_INET;
   }   

   sin.sin_len = 8;
   sin.sin_family = AF_INET;
   sin.sin_port = 0;
   //sin.sin_addr.s_addr = inet_addr("10.10.10.2");
   sin.sin_addr.s_addr = inet_addr(g_address);
   ia->ia_addr = sin;
   i = ntohl(sin.sin_addr.s_addr);
   
   //sin.sin_addr.s_addr = inet_addr("10.10.10.255");
   sin.sin_addr.s_addr = inet_addr(g_netmask);
   ia->ia_broadaddr = sin;


   ia->ia_netmask = IN_CLASSC_NET;
   /*
    * The subnet mask usually includes at least the standard network part,
    * but may may be smaller in the case of supernetting.
    * If it is set, we believe it.
    */
   if (ia->ia_subnetmask == 0) {
      ia->ia_subnetmask = ia->ia_netmask;
      ia->ia_sockmask.sin_addr.s_addr = htonl(ia->ia_subnetmask);
   } else
      ia->ia_netmask &= ia->ia_subnetmask;
   ia->ia_net = i & ia->ia_netmask;
   ia->ia_subnet = i & ia->ia_subnetmask;
   in_socktrim(&ia->ia_sockmask);

   /*
    * Add route for the network.
    */
   ia->ia_ifa.ifa_metric = g_ifnet->if_metric;
   if (g_ifnet->if_flags & USN_IFF_BROADCAST) {
      ia->ia_broadaddr.sin_addr.s_addr =
         htonl(ia->ia_subnet | ~ia->ia_subnetmask);
      ia->ia_netbroadcast.s_addr =
         htonl(ia->ia_net | ~ ia->ia_netmask);
   }
   ia->ia_ifa.ifa_flags |= RTF_CLONING;

   { 
      int    req = RTM_ADD;
      struct usn_sockaddr dst;
      struct usn_sockaddr gateway;
      struct usn_sockaddr netmask;
      int    aflags = RTF_GATEWAY;
      struct rtentry *nrt = 0;
 
      bzero(&dst, sizeof(dst)); 
      dst.sa_len = 8;
      dst.sa_family = AF_INET;
      SIN(&dst)->sin_addr.s_addr = inet_addr("0.0.0.0");

      bzero(&gateway, sizeof(gateway)); 
      gateway.sa_len = 8;
      gateway.sa_family = AF_INET;
      //SIN(&gateway)->sin_addr.s_addr = inet_addr("10.10.10.8");
      SIN(&gateway)->sin_addr.s_addr = inet_addr(g_gateway);

      bzero(&netmask, sizeof(netmask)); 
      netmask.sa_len = 8;
      netmask.sa_family = AF_INET;
      //TODO: calculate netmask
      SIN(&netmask)->sin_addr.s_addr = inet_addr("255.255.255.0");
      in_socktrim(SIN(&netmask));

      error = rtrequest(req, &dst, &gateway, &netmask, aflags, &nrt);

      DEBUG("init_network: done, error=%d", error);
   }

   DEBUG("final route tree");
   print_radix_head((struct usn_radix_node_head *)g_rt_tables[AF_INET]);

   if ((error = rtinit(&(ia->ia_ifa), (int)RTM_ADD, flags | RTF_CLONING | RTF_UP)) == 0) {
      DEBUG("succeed to rtinit");
      ia->ia_flags |= IFA_ROUTE;
   }

   print_radix_head((struct usn_radix_node_head *)g_rt_tables[AF_INET]);


   return;
}

struct ifaddr*
ifa_ifwithaddr(struct usn_sockaddr *addr)
{
   struct ifnet *ifp;
   struct ifaddr *ifa;

#define  equal(a1, a2) \
  (bcmp((caddr_t)(a1), (caddr_t)(a2), ((struct usn_sockaddr *)(a1))->sa_len) == 0)
   for (ifp = g_ifnet; ifp; ifp = ifp->if_next)
       for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr->sa_family != addr->sa_family)
         continue;
      if (equal(addr, ifa->ifa_addr))
         return (ifa);
      if ((ifp->if_flags & IFF_BROADCAST) && ifa->ifa_broadaddr &&
          equal(ifa->ifa_broadaddr, addr))
         return (ifa);
   }   
   return ((struct ifaddr *)0);

}

/* 
 * Locate the point to point interface with a given destination address.
 */
struct ifaddr *
ifa_ifwithdstaddr(struct usn_sockaddr *addr)
{
   struct ifnet *ifp;
   struct ifaddr *ifa;

   for (ifp = g_ifnet; ifp; ifp = ifp->if_next)
     if (ifp->if_flags & USN_IFF_POINTOPOINT)
      for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
         if (ifa->ifa_addr->sa_family != addr->sa_family ||
             ifa->ifa_dstaddr == NULL)
            continue;
         if (equal(addr, ifa->ifa_dstaddr))
            return (ifa);
   }
   return ((struct ifaddr *)0);
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(struct usn_sockaddr *addr)
{
   struct ifnet *ifp;
   struct ifaddr *ifa;
   struct ifaddr *ifa_maybe = (struct ifaddr *) 0;
   u_int af = addr->sa_family;
   char *addr_data = addr->sa_data, *cplim;

   if (af == AF_LINK) {
       struct usn_sockaddr_dl *sdl = (struct usn_sockaddr_dl *)addr;
       if (sdl->sdl_index)// && sdl->sdl_index <= if_index)
          return (g_ifnet_addrs[sdl->sdl_index - 1]);
   }
   for (ifp = g_ifnet; ifp; ifp = ifp->if_next)
       for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
      char *cp, *cp2, *cp3;

      if (ifa->ifa_addr->sa_family != af || ifa->ifa_netmask == 0)
         next: continue;
      cp = addr_data;
      cp2 = ifa->ifa_addr->sa_data;
      cp3 = ifa->ifa_netmask->sa_data;
      cplim = ifa->ifa_netmask->sa_len + (char *)ifa->ifa_netmask;
      while (cp3 < cplim)
         if ((*cp++ ^ *cp2++) & *cp3++)
            goto next;
      if (ifa_maybe == 0 ||
          rn1_refines((caddr_t)ifa->ifa_netmask,
          (caddr_t)ifa_maybe->ifa_netmask))
         ifa_maybe = ifa;
       }
   return (ifa_maybe);
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr( struct usn_sockaddr *addr, struct ifnet *ifp)
{
   struct ifaddr *ifa;
   char *cp, *cp2, *cp3;
   char *cplim;
   struct ifaddr *ifa_maybe = 0;
   u_int af = addr->sa_family; 
      
   if (af >= AF_MAX)
      return (0);
   for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
      if (ifa->ifa_addr->sa_family != af)
         continue;
      ifa_maybe = ifa;
      if (ifa->ifa_netmask == 0) {
         if (equal(addr, ifa->ifa_addr) ||
             (ifa->ifa_dstaddr && equal(addr, ifa->ifa_dstaddr)))
            return (ifa);
         continue;
      }
      cp = addr->sa_data;
      cp2 = ifa->ifa_addr->sa_data;
      cp3 = ifa->ifa_netmask->sa_data;
      cplim = ifa->ifa_netmask->sa_len + (char *)ifa->ifa_netmask;
      for (; cp3 < cplim; cp3++)
         if ((*cp++ ^ *cp2++) & *cp3)
            break;
      if (cp3 == cplim)
         return (ifa);
   }
   return (ifa_maybe);
}
/*
 * Map interface name to
 * interface structure pointer.
 */   
struct ifnet * 
ifunit(char *name)
{
   char *cp;
   struct ifnet *ifp;
   int unit;
   unsigned len;
   char *ep, c;
   
   for (cp = name; cp < name + IFNAMSIZ && *cp; cp++)
      if (*cp >= '0' && *cp <= '9')
         break;
   if (*cp == '\0' || cp == name + IFNAMSIZ)
      return ((struct ifnet *)0);
   /* 
    * Save first char of unit, and pointer to it,
    * so we can put a null there to avoid matching
    * initial substrings of interface names.
    */
   len = cp - name + 1;
   c = *cp; 
   ep = cp;
   for (unit = 0; *cp >= '0' && *cp <= '9'; )
      unit = unit * 10 + *cp++ - '0';
   *ep = 0;
   for (ifp = g_ifnet; ifp; ifp = ifp->if_next) {
      if (bcmp(ifp->if_name, name, len))
         continue;
      if (unit == ifp->if_unit)
         break;
   }
   *ep = c;
   return (ifp);
}

