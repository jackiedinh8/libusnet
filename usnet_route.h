#ifndef _USNET_ROUTE_H_
#define _USNET_ROUTE_H_

#include <sys/socket.h>

#include "usnet_in.h"
#include "usnet_error.h"
#include "usnet_radix.h"

#define RTM_VERSION  3  /* Up the ante and ignore older versions */

#define RTM_ADD      0x1   /* Add Route */
#define RTM_DELETE   0x2   /* Delete Route */
#define RTM_CHANGE   0x3   /* Change Metrics or flags */
#define RTM_GET      0x4   /* Report Metrics */
#define RTM_LOSING   0x5   /* Kernel Suspects Partitioning */
#define RTM_REDIRECT 0x6   /* Told to use different route */
#define RTM_MISS     0x7   /* Lookup failed on this address */
#define RTM_LOCK     0x8   /* fix specified metrics */
#define RTM_OLDADD   0x9   /* caused by SIOCADDRT */
#define RTM_OLDDEL   0xa   /* caused by SIOCDELRT */
#define RTM_RESOLVE  0xb   /* req to resolve dst to LL addr */
#define RTM_NEWADDR  0xc   /* address being added to iface */
#define RTM_DELADDR  0xd   /* address being removed from iface */
#define RTM_IFINFO   0xe   /* iface going up/down etc. */

#define RTV_MTU      0x1   /* init or lock _mtu */
#define RTV_HOPCOUNT 0x2   /* init or lock _hopcount */
#define RTV_EXPIRE   0x4   /* init or lock _hopcount */
#define RTV_RPIPE    0x8   /* init or lock _recvpipe */
#define RTV_SPIPE    0x10  /* init or lock _sendpipe */
#define RTV_SSTHRESH 0x20  /* init or lock _ssthresh */
#define RTV_RTT      0x40  /* init or lock _rtt */
#define RTV_RTTVAR   0x80  /* init or lock _rttvar */


#define RTF_UP      0x1      /* route usable */
#define RTF_GATEWAY 0x2      /* destination is a gateway */
#define RTF_HOST    0x4      /* host entry (net otherwise) */
#define RTF_REJECT  0x8      /* host or net unreachable */
#define RTF_DYNAMIC 0x10     /* created dynamically (by redirect) */
#define RTF_MODIFIED   0x20     /* modified dynamically (by redirect) */
#define RTF_DONE  0x40     /* message confirmed */
#define RTF_MASK  0x80     /* subnet mask present */
#define RTF_CLONING  0x100    /* generate new routes on use */
#define RTF_XRESOLVE 0x200    /* external daemon resolves name */
#define RTF_LLINFO   0x400    /* generated by ARP or ESIS */
#define RTF_STATIC   0x800    /* manually added */
#define RTF_BLACKHOLE   0x1000      /* just discard pkts (during updates) */
#define RTF_PROTO2   0x4000      /* protocol specific routing flag */
#define RTF_PROTO1   0x8000      /* protocol specific routing flag */


/*
 * Index offsets for sockaddr array for alternate internal encoding.
 */
#define RTAX_DST  0  /* destination sockaddr present */
#define RTAX_GATEWAY 1  /* gateway sockaddr present */
#define RTAX_NETMASK 2  /* netmask sockaddr present */
#define RTAX_GENMASK 3  /* cloning mask sockaddr present */
#define RTAX_IFP  4  /* interface name sockaddr present */
#define RTAX_IFA  5  /* interface addr sockaddr present */
#define RTAX_AUTHOR  6  /* sockaddr for author of redirect */
#define RTAX_BRD  7  /* for NEWADDR, broadcast or p-p dest addr */
#define RTAX_MAX  8  /* size of array to allocate */


struct rt_addrinfo {
   int    rti_addrs;
   struct usn_sockaddr *rti_info[RTAX_MAX];
};

/*
 * Routing statistics.
 */
struct   rtstat {
   short rts_badredirect;  /* bogus redirect calls */
   short rts_dynamic;      /* routes created by redirects */
   short rts_newgateway;      /* routes modified by redirects */
   short rts_unreach;      /* lookups which failed */
   short rts_wildcard;     /* lookups satisfied by a wildcard */
};

#define  RTFREE(rt) \
   if ((rt)->rt_refcnt <= 1) \
      rtfree(rt); \
   else \
      (rt)->rt_refcnt--;

#define  IFAFREE(ifa) \
   if ((ifa)->ifa_refcnt <= 0) \
      ifafree(ifa); \
   else \
      (ifa)->ifa_refcnt--;


//#define AF_MAX 26
extern struct usn_radix_node_head *g_rt_tables[AF_MAX+1];
extern struct rtstat g_rtstat;

/*
 * These numbers are used by reliable protocols for determining
 * retransmission behavior and are included in the routing structure.
 */
struct rt_metrics {
   u_long   rmx_locks;  /* Kernel must leave these values alone */
   u_long   rmx_mtu; /* MTU for this path */
   u_long   rmx_hopcount;  /* max hops expected */
   u_long   rmx_expire; /* lifetime for route, e.g. redirect */
   u_long   rmx_recvpipe;  /* inbound delay-bandwith product */
   u_long   rmx_sendpipe;  /* outbound delay-bandwith product */
   u_long   rmx_ssthresh;  /* outbound gateway buffer limit */
   u_long   rmx_rtt; /* estimated round trip time */
   u_long   rmx_rttvar; /* estimated rtt variance */
   u_long   rmx_pksent; /* packets sent using this route */
};


/*
 * We distinguish between routes to hosts and routes to networks,
 * preferring the former if available.  For each route we infer
 * the interface to use from the gateway address supplied when
 * the route was entered.  Routes that forward packets through
 * gateways are marked so that the output routines know to address the
 * gateway rather than the ultimate destination.
 */
struct rtentry {
   struct usn_radix_node rt_nodes[2]; /* tree glue, and other values */
#define  rt_key(r)   ((struct usn_sockaddr *)((r)->rt_nodes->rn_key))
#define  rt_mask(r)  ((struct usn_sockaddr *)((r)->rt_nodes->rn_mask))
   struct usn_sockaddr *rt_gateway;   /* value */
   short rt_flags;      /* up/down?, host/net */
   short rt_refcnt;     /* # held references */
   u_long   rt_use;        /* raw # packets forwarded */
   struct ifnet *rt_ifp;    /* the answer: interface to use */
   struct ifaddr *rt_ifa;      /* the answer: interface to use */
   struct usn_sockaddr *rt_genmask;   /* for generation of cloned routes */
   caddr_t rt_llinfo;     /* pointer to link level info cache */
   struct rt_metrics rt_rmx;   /* metrics used by rx'ing protocols */
   struct rtentry *rt_gwroute; /* implied entry for gatewayed routes */
#define  rt_expire rt_rmx.rmx_expire
};



/*
 * A route consists of a destination address and a reference
 * to a routing entry.  These are often held by protocols
 * in their control blocks, e.g. inpcb.
 */
struct route {
   struct rtentry      *ro_rt;
   struct usn_sockaddr  ro_dst;
};

int
rtrequest(
   int req, 
   struct usn_sockaddr *dst,
   struct usn_sockaddr *gateway,
   struct usn_sockaddr *netmask,
   int flags,
   struct rtentry **ret_nrt);

int      
rt_setgate(
   struct rtentry *rt0,
   struct usn_sockaddr *dst,
   struct usn_sockaddr *gate);

void
rtalloc(struct route *ro);

struct rtentry *
rtalloc8(struct usn_sockaddr *dst, int report);

void
rtfree( struct rtentry *rt);

int
rtredirect(
   struct usn_sockaddr *dst,
   struct usn_sockaddr *gateway,
   struct usn_sockaddr *netmask,
   int    flags,
   struct usn_sockaddr *src,
   struct rtentry **rtp);

int
rtinit( struct ifaddr *ifa, int cmd, int flags);

void
usnet_route_init();



#endif //!_USNET_ROUTE_H_
