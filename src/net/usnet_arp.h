#ifndef ARP_HEADER_H_
#define ARP_HEADER_H_

#include <ctype.h>      /* isprint() */
#include <sys/mman.h>
#include <sys/poll.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>	/* PRI* macros */
#include <string.h>	/* strcmp */
#include <fcntl.h>	/* open */
#include <unistd.h>	/* close */
#include <sys/ioctl.h>	/* ioctl */
#include <sys/param.h>
#include <sys/socket.h>	/* apple needs sockaddr */
#include <net/if.h>	/* ifreq */
//#include <libgen.h>	/* basename */
#include <stdlib.h>	/* atoi, free */


#define NETMAP_WITH_LIBS
#include <net/netmap.h>
#include <net/netmap_user.h>

#include "usnet_buf.h"
#include "usnet_buf_utils.h"
#include "usnet_types.h"
#include "usnet_socket.h"
#include "usnet_in.h"
#include "usnet_core.h"
#include "usnet_route.h"
#include "usnet_if_dl.h"


extern int	g_arpt_prune;	/* walk list every 5 minutes */
extern int	g_arpt_keep;	/* once resolved, good for 20 more minutes */
extern int	g_arpt_down;		/* once declared down, don't send for 20 secs */

/*
 * Structure of a 10Mb/s Ethernet header.
 */
typedef struct usn_ether_header ether_header_t;
struct	usn_ether_header {
	u_char	ether_dhost[6];
	u_char	ether_shost[6];
	u_short	ether_type;
};

#define  ETHERTYPE_PUP     0x0200   /* PUP protocol */
#define  ETHERTYPE_IP      0x0800   /* IP protocol */
#define ETHERTYPE_ARP      0x0806   /* Addr. resolution protocol */
#define ETHERTYPE_REVARP   0x8035   /* reverse Addr. resolution protocol */

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define  ETHERTYPE_TRAIL      0x1000      /* Trailer packet */
#define  ETHERTYPE_NTRAILER   16

#define  ETHERMTU 1500
#define  ETHERMIN (60-14)

/* 
 * ARP Routing Table
 * and buffer for holding outgoing packets
 */

/*
 * ARP Caching Mechanism
 */
struct llinfo_arp {               
   struct      llinfo_arp *la_next;
   struct      llinfo_arp *la_prev;
   struct      rtentry *la_rt;
   usn_mbuf_t *la_hold;    /* last packet until resolved/timeout */
   long        la_asked;      /* last time we QUERIED for this addr */
#define la_timer la_rt->rt_rmx.rmx_expire /* deletion time in seconds */
};
extern struct llinfo_arp g_llinfo_arg;

struct usn_sockaddr_inarp {
   u_char   sin_len;
   u_char   sin_family;
   u_short sin_port;
   struct   usn_in_addr sin_addr;
   struct   usn_in_addr sin_srcaddr;
   u_short  sin_tos;
   u_short  sin_other;
#define SIN_PROXY 0
};
/*
 * IP and ethernet specific routing flags
 */
#define	RTF_USETRAILERS	RTF_PROTO1	/* use trailers */
#define RTF_ANNOUNCE	RTF_PROTO2	/* announce new arp entry */


#define SIN(s) ((struct usn_sockaddr_in *)s)
#define SDL(s) ((struct usn_sockaddr_dl *)s)
#define SRP(s) ((struct usn_sockaddr_inarp *)s)


struct llinfo_arp *
arplookup( u_long addr, int create, int proxy);


typedef struct arp_entry arp_entry_t;
struct arp_entry {
   u_int32         ip_addr;
   u_char          eth_addr[6];
   long            last_time;
   u_int           asked;
   usn_mbuf_t     *hold;
   u_int           flags;
#define ARP_ENTRY_COMPLETE 0x0001
#define ARP_ENTRY_REJECT   0x0002

};

typedef struct arp_cache arp_cache_t;
struct arp_cache {
   arp_cache_t    *prev;
   arp_cache_t    *next;
	arp_entry_t     arp;
	usn_mbuf_t     *la_hold;		/* last packet until resolved/timeout */
	int	          la_asked;		/* last time we QUERIED for this addr */
};

extern arp_cache_t *g_arp_cache;

arp_cache_t* 
arplookup_old(arp_entry_t *e, int create);


void 
arpwhohas(struct usn_in_addr *addr);

void 
arprequest(struct ifnet *ifp, const struct usn_in_addr *sip,
          const struct usn_in_addr *tip, u_char *enaddr);

void 
arptimer();

void 
arpfree(arp_cache_t *e);

int 
arpresolve(struct rtentry *rt, usn_mbuf_t *m, struct usn_sockaddr *dst, u_char *desten);

void
in_arpintr(usn_mbuf_t *m);

void
in_arpinput(usn_mbuf_t *m);

void arpinit();

void
arp_rtrequest(int req, struct rtentry *rt, struct usn_sockaddr *sa);

void
handle_arp_old(u_char *m, int len, struct netmap_ring *ring, int cur);

/*
 * Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  ARP packets are variable
 * in size; the arphdr structure defines the fixed-length portion.
 * Protocol type values are the same as those for 10 Mb/s Ethernet.
 * It is followed by the variable-sized fields ar_sha, arp_spa,
 * arp_tha and arp_tpa in that order, according to the lengths
 * specified.  Field names used correspond to RFC 826.
 */
struct   arphdr {
   u_short  ar_hrd;     /* format of hardware address */
#define ARPHRD_ETHER    1  /* ethernet hardware format */
#define ARPHRD_FRELAY   15 /* frame relay hardware format */
   u_short  ar_pro;     /* format of protocol address */
   u_char   ar_hln;     /* length of hardware address */
   u_char   ar_pln;     /* length of protocol address */
   u_short  ar_op;      /* one of: */
#define  ARPOP_REQUEST  1  /* request to resolve address */
#define  ARPOP_REPLY 2  /* response to previous request */
#define  ARPOP_REVREQUEST 3   /* request protocol address given hardware */
#define  ARPOP_REVREPLY 4  /* response giving protocol address */
#define ARPOP_INVREQUEST 8    /* request to identify peer */
#define ARPOP_INVREPLY  9  /* response identifying peer */
/*
 * The remaining fields are variable in size,
 * according to the sizes above.
 */
#ifdef COMMENT_ONLY
   u_char   ar_sha[];   /* sender hardware address */
   u_char   ar_spa[];   /* sender protocol address */
   u_char   ar_tha[];   /* target hardware address */
   u_char   ar_tpa[];   /* target protocol address */
#endif
};

/*
 * Ethernet Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  Structure below is adapted
 * to resolving internet addresses.  Field names used correspond to 
 * RFC 826.
 */
struct	ether_arp {
	struct	arphdr ea_hdr;	/* fixed-size header */
	u_char	arp_sha[6];	/* sender hardware address */
	u_char	arp_spa[4];	/* sender protocol address */
	u_char	arp_tha[6];	/* target hardware address */
	u_char	arp_tpa[4];	/* target protocol address */
};
#define	arp_hrd	ea_hdr.ar_hrd
#define	arp_pro	ea_hdr.ar_pro
#define	arp_hln	ea_hdr.ar_hln
#define	arp_pln	ea_hdr.ar_pln
#define	arp_op	ea_hdr.ar_op


/*
 * ARP ioctl request
 */
struct arpreq {
   struct   usn_sockaddr arp_pa;     /* protocol address */
   struct   usn_sockaddr arp_ha;     /* hardware address */
   int   arp_flags;        /* flags */
};
/*  arp_flags and at_flags field values */
#define  ATF_INUSE   0x01  /* entry in use */
#define ATF_COM      0x02  /* completed entry (enaddr valid) */
#define  ATF_PERM 0x04  /* permanent entry */
#define  ATF_PUBL 0x08  /* publish entry (respond for other host) */
#define  ATF_USETRAILERS   0x10  /* has requested trailers */


#endif // ARP_HEADER_H_

