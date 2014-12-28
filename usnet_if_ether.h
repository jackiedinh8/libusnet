#ifndef _USNET_IF_ETHER_H_
#define _USNET_IF_ETHER_H_

#include "usnet_arp.h"
#include "usnet_in.h"
#include "usnet_if.h"

/*
 * Structure of a 10Mb/s Ethernet header.
 */
typedef struct usn_ether_header ether_header_t;
struct	usn_ether_header {
	u_char	ether_dhost[6];
	u_char	ether_shost[6];
	u_short	ether_type;
};

#define	ETHERTYPE_PUP		0x0200	/* PUP protocol */
#define	ETHERTYPE_IP		0x0800	/* IP protocol */
#define ETHERTYPE_ARP		0x0806	/* Addr. resolution protocol */
#define ETHERTYPE_REVARP	0x8035	/* reverse Addr. resolution protocol */

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define	ETHERTYPE_TRAIL		0x1000		/* Trailer packet */
#define	ETHERTYPE_NTRAILER	16

#define	ETHERMTU	1500
#define	ETHERMIN	(60-14)

/*
 * Structure shared between the ethernet driver modules and
 * the address resolution code.  For example, each ec_softc or il_softc
 * begins with this structure.
 */
struct	usn_arpcom {
	struct 	ifnet ac_if;		/* network-visible interface */
	u_char	ac_enaddr[6];		/* ethernet hardware address */
	struct	usn_in_addr ac_ipaddr;	/* copy of ip address- XXX */
	struct	ether_multi *ac_multiaddrs; /* list of ether multicast addrs */
	int	ac_multicnt;		/* length of ac_multiaddrs list */	
};

struct llinfo_arp {				
	struct   	llinfo_arp *la_next;
	struct   	llinfo_arp *la_prev;
	struct	   rtentry *la_rt;
	usn_mbuf_t *la_hold;		/* last packet until resolved/timeout */
	long	      la_asked;		/* last time we QUERIED for this addr */
#define la_timer la_rt->rt_rmx.rmx_expire /* deletion time in seconds */
};

struct usn_sockaddr_inarp {
	u_char	sin_len;
	u_char	sin_family;
	u_short sin_port;
	struct	usn_in_addr sin_addr;
	struct	usn_in_addr sin_srcaddr;
	u_short	sin_tos;
	u_short	sin_other;
#define SIN_PROXY 1
};
/*
 * IP and ethernet specific routing flags
 */
#define	RTF_USETRAILERS	RTF_PROTO1	/* use trailers */
#define RTF_ANNOUNCE	RTF_PROTO2	/* announce new arp entry */


#endif // USNET_IF_ETHTER_H_

