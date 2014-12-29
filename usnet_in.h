#ifndef USNET_IN_H_
#define USNET_IN_H_

#include "usnet_types.h"

/*
  * Definitions of bits in internet address integers.
  * On subnets, the decomposition of addresses to host and net parts
  * is done according to subnet mask, not the masks here.
  */

#define USN_IN_CLASSA(i)      (((u_int)(i) & 0x80000000) == 0)
#define USN_IN_CLASSA_NET     0xff000000
#define USN_IN_CLASSA_NSHIFT  24
#define USN_IN_CLASSA_HOST    0x00ffffff
#define USN_IN_CLASSA_MAX     128

#define USN_IN_CLASSB(i)      (((u_int)(i) & 0xc0000000) == 0x80000000)
#define USN_IN_CLASSB_NET     0xffff0000
#define USN_IN_CLASSB_NSHIFT  16
#define USN_IN_CLASSB_HOST    0x0000ffff
#define USN_IN_CLASSB_MAX     65536

#define USN_IN_CLASSC(i)      (((u_int)(i) & 0xe0000000) == 0xc0000000)
#define USN_IN_CLASSC_NET     0xffffff00
#define USN_IN_CLASSC_NSHIFT  8
#define USN_IN_CLASSC_HOST    0x000000ff

#define USN_IN_CLASSD(i)      (((u_int)(i) & 0xf0000000) == 0xe0000000)
#define USN_IN_CLASSD_NET     0xf0000000  /* These ones aren't really */
#define USN_IN_CLASSD_NSHIFT  28    /* net and host fields, but */
#define USN_IN_CLASSD_HOST    0x0fffffff  /* routing needn't know.    */
#define USN_IN_MULTICAST(i)     USN_IN_CLASSD(i)

#define USN_IN_EXPERIMENTAL(i)   (((u_int)(i) & 0xf0000000) == 0xf0000000)
#define USN_IN_BADCLASS(i)    (((u_int)(i) & 0xf0000000) == 0xf0000000)

#define USN_INADDR_ANY     (u_int)0x00000000
#define USN_INADDR_BROADCAST  (u_int)0xffffffff   /* must be masked */
#define USN_INADDR_NONE    0xffffffff     /* -1 return */

#define USN_INADDR_UNSPEC_GROUP  (u_int)0xe0000000   /* 224.0.0.0 */
#define USN_INADDR_ALLHOSTS_GROUP   (u_int)0xe0000001   /* 224.0.0.1 */
#define USN_INADDR_MAX_LOCAL_GROUP  (u_int)0xe00000ff   /* 224.0.0.255 */

#define USN_IN_LOOPBACKNET    127         /* official! */

/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981, and numerous additions.
 */

/*
 * Protocols
 */
#define	IPPROTO_IP		0		/* dummy for IP */
#define	IPPROTO_ICMP		1		/* control message protocol */
#define	IPPROTO_IGMP		2		/* group mgmt protocol */
#define	IPPROTO_GGP		3		/* gateway^2 (deprecated) */
#define	IPPROTO_TCP		6		/* tcp */
#define	IPPROTO_EGP		8		/* exterior gateway protocol */
#define	IPPROTO_PUP		12		/* pup */
#define	IPPROTO_UDP		17		/* user datagram protocol */
#define	IPPROTO_IDP		22		/* xns idp */
#define	IPPROTO_TP		29 		/* tp-4 w/ class negotiation */
#define	IPPROTO_EON		80		/* ISO cnlp */
#define	IPPROTO_ENCAP		98		/* encapsulation header */

#define	IPPROTO_RAW		255		/* raw IP packet */
#define	IPPROTO_MAX		256


/*
 * Local port number conventions:
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 * Ports > IPPORT_USERRESERVED are reserved
 * for servers, not necessarily privileged.
 */
#define	IPPORT_RESERVED		1024
#define	IPPORT_USERRESERVED	5000

/*
 * Internet address (a structure for historical reasons)
 */
typedef u_int usn_in_addr_t;

struct usn_in_addr {
	u_int32 s_addr;
};

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct usn_sockaddr {
   u_char   sa_len;        /* total length */
   u_char   sa_family;     /* address family */
   char  sa_data[14];      /* actually longer; address value */
};

/*
 * Socket address, internet style.
 */
struct usn_sockaddr_in {
   u_char   sin_len;
   u_char   sin_family;
   u_short  sin_port;
   struct   usn_in_addr sin_addr;
   char  sin_zero[8];
};


/*
 * Structure used to describe IP options.
 * Used to store options internally, to pass them to a process,
 * or to restore options retrieved earlier.
 * The ip_dst is used for the first-hop gateway when using a source route
 * (this gets put into the header proper).
 */
struct usn_ip_opts {
	struct	usn_in_addr ip_dst;		/* first hop, 0 w/o src rt */
	char	ip_opts[40];		/* actually variable in size */
};

/*
 * Options for use with [gs]etsockopt at the IP level.
 * First word of comment is data type; bool is stored in int.
 */
#define	IP_OPTIONS		1    /* buf/ip_opts; set/get IP options */
#define	IP_HDRINCL		2    /* int; header is included with data */
#define	IP_TOS			3    /* int; IP type of service and preced. */
#define	IP_TTL			4    /* int; IP time to live */
#define	IP_RECVOPTS		5    /* bool; receive all IP opts w/dgram */
#define	IP_RECVRETOPTS		6    /* bool; receive IP opts for response */
#define	IP_RECVDSTADDR		7    /* bool; receive IP dst addr w/dgram */
#define	IP_RETOPTS		8    /* ip_opts; set/get IP options */
#define	IP_MULTICAST_IF		9    /* u_char; set/get IP multicast i/f  */
#define	IP_MULTICAST_TTL	10   /* u_char; set/get IP multicast ttl */
#define	IP_MULTICAST_LOOP	11   /* u_char; set/get IP multicast loopback */
#define	IP_ADD_MEMBERSHIP	12   /* ip_mreq; add an IP group membership */
#define	IP_DROP_MEMBERSHIP	13   /* ip_mreq; drop an IP group membership */


int 
in_canforward(struct usn_in_addr in);

int
in_localaddr( struct usn_in_addr in);

struct ifnet; // forward declaration.
int 
in_broadcast(struct usn_in_addr in, struct ifnet *ifp);

#endif // USNET_IN_H_
