#ifndef _USNET_IF_H_
#define _USNET_IF_H_

#include <sys/time.h>

#include "usnet_types.h"
#include "usnet_in.h"
#include "usnet_socket.h"

#define  USN_IFF_UP      0x1      /* interface is up */
#define  USN_IFF_BROADCAST  0x2      /* broadcast address valid */
#define  USN_IFF_DEBUG   0x4      /* turn on debugging */
#define  USN_IFF_LOOPBACK   0x8      /* is a loopback net */
#define  USN_IFF_POINTOPOINT   0x10     /* interface is point-to-point link */
#define  USN_IFF_NOTRAILERS 0x20     /* avoid use of trailers */
#define  USN_IFF_RUNNING 0x40     /* resources allocated */
#define  USN_IFF_NOARP   0x80     /* no address resolution protocol */
#define  USN_IFF_PROMISC 0x100    /* receive all packets */
#define  USN_IFF_ALLMULTI   0x200    /* receive all multicast packets */
#define  USN_IFF_OACTIVE 0x400    /* transmission in progress */
#define  USN_IFF_SIMPLEX 0x800    /* can't hear own transmissions */
#define  USN_IFF_LINK0   0x1000      /* per link layer defined bit */
#define  USN_IFF_LINK1   0x2000      /* per link layer defined bit */
#define  USN_IFF_LINK2   0x4000      /* per link layer defined bit */
#define  USN_IFF_MULTICAST  0x8000      /* supports multicast */

/*
 * Structure describing information about an interface
 * which may be of interest to management entities.
 */
/*
 * Structure defining a queue for a network interface.
 */

struct ifnet {
   char    *if_name;      /* name, e.g. ``en'' or ``lo'' */
   struct   ifnet *if_next;      /* all struct ifnets are chained */
   struct   ifaddr *if_addrlist; /* linked list of addresses per if */
   u_short  if_index;      /* numeric abbreviation for this if  */
   short    if_unit;    /* sub-unit for lower level driver */
   short    if_flags;      /* up/down, broadcast, etc. */
   struct   if_data_new { // conflict with std lib.
/* generic interface information */
      u_char   ifi_type;   /* ethernet, tokenring, etc */
      u_char   ifi_addrlen;   /* media address length */
      u_char   ifi_hdrlen; /* media header length */
      u_int    ifi_mtu; /* maximum transmission unit */
      u_int    ifi_metric; /* routing metric (external only) */
      u_int    ifi_baudrate;  /* linespeed */
      struct   timeval ifi_lastchange;/* last updated */
   }  if_data_new;
};
#define  if_mtu      if_data_new.ifi_mtu
#define  if_type     if_data_new.ifi_type
#define  if_addrlen  if_data_new.ifi_addrlen
#define  if_hdrlen   if_data_new.ifi_hdrlen
#define  if_metric   if_data_new.ifi_metric
#define  if_baudrate if_data_new.ifi_baudrate
#define  if_lastchange  if_data_new.ifi_lastchange

/*
 * The ifaddr structure contains information about one address
 * of an interface.  They are maintained by the different address families,
 * are allocated and attached when an address is set, and are linked
 * together so all addresses for an interface can be located.
 */
struct ifaddr {
   struct   usn_sockaddr *ifa_addr;      /* address of interface */
   struct   usn_sockaddr *ifa_broadaddr; /* broadcast address interface */
#define  ifa_dstaddr   ifa_broadaddr     /* other end of p-to-p link */
   struct   usn_sockaddr *ifa_netmask;   /* used to determine subnet */
   struct   ifnet *ifa_ifp;              /* back-pointer to interface */
   struct   ifaddr *ifa_next;            /* next address for interface */
   u_short  ifa_flags;                   /* mostly rt_flags for cloning */
   short ifa_refcnt;    /* extra to malloc for link info */
   int   ifa_metric;    /* cost of going out this interface */
};
#define  IFA_ROUTE   RTF_UP      /* route installed */

/*
 * Interface address, Internet version.  One of these structures
 * is allocated for each interface with an Internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */
struct in_ifaddr {
   struct   ifaddr ia_ifa;    /* protocol-independent info */
#define ia_ifp      ia_ifa.ifa_ifp
#define ia_flags    ia_ifa.ifa_flags
                           /* ia_{,sub}net{,mask} in host order */
   u_int    ia_net;        /* network number of interface */
   u_int    ia_netmask;    /* mask of net part */
   u_int    ia_subnet;     /* subnet number, including net */
   u_int    ia_subnetmask; /* mask of subnet part */
   struct   usn_in_addr ia_netbroadcast; /* to recognize net broadcasts */
   struct   in_ifaddr *ia_next;  /* next in list of internet addresses */
   struct   usn_sockaddr_in ia_addr; /* reserve space for interface name */
   struct   usn_sockaddr_in ia_dstaddr; /* reserve space for broadcast addr */
#define  ia_broadaddr   ia_dstaddr
   struct   usn_sockaddr_in ia_sockmask; /* reserve space for general netmask */
};


extern struct in_ifaddr *g_in_ifaddr;
extern struct ifaddr    *g_if_addrlist;
extern struct ifaddr    *g_ifaddr;
extern struct ifnet     *g_ifnet;

/*
 * Initialize a list of addresses for ether interface.
 */
void
usnet_network_init();

/*
 * Locate an interface based on a complete address.
 */
struct ifaddr*
ifa_ifwithaddr(struct usn_sockaddr *addr);


/* 
 * Locate the point to point interface with a given destination address.
 */
struct ifaddr *
ifa_ifwithdstaddr(struct usn_sockaddr *addr);

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(struct usn_sockaddr *addr);


/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr( struct usn_sockaddr *addr, struct ifnet *ifp);

#endif //!_USNET_IF_H_
