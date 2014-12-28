#ifndef _USNET_SOCKET_H_
#define _USNET_SOCKET_H_

#include <stdlib.h>
#include <stdint.h>

#include "usnet_buf.h"
#include "usnet_protosw.h"

/*
 * Used to maintain information about processes that wish to be
 * notified when I/O becomes possible.
 */
struct selinfo {
   pid_t si_pid;     /* process to be notified */
   short si_flags;   /* see below */
};

/*
 * Kernel structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct socket {
   short so_type;    /* generic type, see socket.h */
   short so_options;    /* from socket call, see socket.h */
   short so_linger;     /* time to linger while closing */
   short so_state;      /* internal state flags SS_*, below */
   caddr_t  so_pcb;        /* protocol control block */
   struct   protosw *so_proto;   /* protocol handle */
/*
 * Variables for connection queueing.
 * Socket where accepts occur is so_head in all subsidiary sockets.
 * If so_head is 0, socket is not related to an accept.
 * For head socket so_q0 queues partially completed connections,
 * while so_q is a queue of connections ready to be accepted.
 * If a connection is aborted and it has so_head set, then
 * it has to be pulled out of either so_q0 or so_q.
 * We allow connections to queue up based on current queue lengths
 * and limit on number of queued connections for this socket.
 */
   struct   socket *so_head;  /* back pointer to accept socket */
   struct   socket *so_q0;    /* queue of partial connections */
   struct   socket *so_q;     /* queue of incoming connections */
   short            so_q0len;      /* partials on so_q0 */
   short            so_qlen;    /* number of connections on so_q */
   short            so_qlimit;     /* max number queued connections */
   short            so_timeo;      /* connection timeout */
   u_short          so_error;      /* error affecting connection */
   pid_t            so_pgid;    /* pgid for signals */
   u_int            so_oobmark;    /* chars to oob mark */
/*
 * Variables for socket buffering.
 */
   struct   sockbuf {
      u_int          sb_cc;      /* actual chars in buffer */
      u_int          sb_hiwat;   /* max actual char count */
      u_int          sb_mbcnt;   /* chars of mbufs used */
      u_int          sb_mbmax;   /* max chars of mbufs to use */
      int            sb_lowat;   /* low water mark */
      usn_mbuf_t     *sb_mb;   /* the mbuf chain */
      struct selinfo sb_sel;   /* process selecting read/write */
      short          sb_flags;   /* flags, see below */
      short          sb_timeo;   /* timeout for read/write */
   } so_rcv, so_snd;
#define  SB_MAX      (256*1024)  /* default for max chars in sockbuf */
#define  SB_LOCK     0x01     /* lock on data queue */
#define  SB_WANT     0x02     /* someone is waiting to lock */
#define  SB_WAIT     0x04     /* someone is waiting for data/space */
#define  SB_SEL      0x08     /* someone is selecting */
#define  SB_ASYNC 0x10     /* ASYNC I/O, need signals */
#define  SB_NOTIFY   (SB_WAIT|SB_SEL|SB_ASYNC)
#define  SB_NOINTR   0x40     /* operations not interruptible */

   caddr_t  so_tpcb;    /* Wisc. protocol control block XXX */
   void  (*so_upcall) __P((struct socket *so, caddr_t arg, int waitf));
   caddr_t  so_upcallarg;     /* Arg for above */
};


/*
 * Definitions related to sockets: types, address families, options.
 */

/*
 * Types
 */
#define	SOCK_STREAM	1		/* stream socket */
#define	SOCK_DGRAM	2		/* datagram socket */
#define	SOCK_RAW	3		/* raw-protocol interface */
#define	SOCK_RDM	4		/* reliably-delivered message */
#define	SOCK_SEQPACKET	5		/* sequenced packet stream */

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER	0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define	SO_REUSEPORT	0x0200		/* allow local address & port reuse */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001		/* send buffer size */
#define SO_RCVBUF	0x1002		/* receive buffer size */
#define SO_SNDLOWAT	0x1003		/* send low-water mark */
#define SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define SO_SNDTIMEO	0x1005		/* send timeout */
#define SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */

/*
 * Structure used for manipulating linger option.
 */
struct	usnet_linger {
	int	l_onoff;		/* option on/off */
	int	l_linger;		/* linger time in seconds */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Address families.
 */

//#define	AF_UNSPEC	0		/* unspecified */
//#define	AF_LOCAL	1		/* local to host (pipes, portals) */
//#define	AF_UNIX		AF_LOCAL	/* backward compatibility */
//#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
//#define	AF_IMPLINK	3		/* arpanet imp addresses */
//#define	AF_PUP		4		/* pup protocols: e.g. BSP */
//#define	AF_CHAOS	5		/* mit CHAOS protocols */
//#define	AF_NS		6		/* XEROX NS protocols */
//#define	AF_ISO		7		/* ISO protocols */
//#define	AF_OSI		AF_ISO
//#define	AF_ECMA		8		/* european computer manufacturers */
//#define	AF_DATAKIT	9		/* datakit protocols */
//#define	AF_CCITT	10		/* CCITT protocols, X.25 etc */
//#define	AF_SNA		11		/* IBM SNA */
//#define AF_DECnet	12		/* DECnet */
//#define AF_DLI		13		/* DEC Direct data link interface */
//#define AF_LAT		14		/* LAT */
//#define	AF_HYLINK	15		/* NSC Hyperchannel */
//#define	AF_APPLETALK	16		/* Apple Talk */
//#define	AF_ROUTE	17		/* Internal Routing Protocol */
//#define	AF_LINK		18		/* Link layer interface */
//#define	pseudo_AF_XTP	19		/* eXpress Transfer Protocol (no AF) */
//#define	AF_COIP		20		/* connection-oriented IP, aka ST II */
//#define	AF_CNT		21		/* Computer Network Technology */
//#define pseudo_AF_RTIP	22		/* Help Identify RTIP packets */
//#define	AF_IPX		23		/* Novell Internet Protocol */
//#define	AF_SIP		24		/* Simple Internet Protocol */
//#define pseudo_AF_PIP	25		/* Help Identify PIP packets */

//#define	AF_MAX		26

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct usnet_sockaddr {
	u_char	sa_len;			/* total length */
	u_char	sa_family;		/* address family */
	char	sa_data[14];		/* actually longer; address value */
};

/*
 * Structure used by kernel to pass protocol
 * information in raw sockets.
 */
struct usnet_sockproto {
	u_short	sp_family;		/* address family */
	u_short	sp_protocol;		/* protocol */
};

/*
 * Protocol families, same as address families for now.
 */
//#define	PF_UNSPEC	AF_UNSPEC
//#define	PF_LOCAL	AF_LOCAL
//#define	PF_UNIX		PF_LOCAL	/* backward compatibility */
//#define	PF_INET		AF_INET
//#define	PF_IMPLINK	AF_IMPLINK
//#define	PF_PUP		AF_PUP
//#define	PF_CHAOS	AF_CHAOS
//#define	PF_NS		AF_NS
//#define	PF_ISO		AF_ISO
//#define	PF_OSI		AF_ISO
//#define	PF_ECMA		AF_ECMA
//#define	PF_DATAKIT	AF_DATAKIT
//#define	PF_CCITT	AF_CCITT
//#define	PF_SNA		AF_SNA
//#define PF_DECnet	AF_DECnet
//#define PF_DLI		AF_DLI
//#define PF_LAT		AF_LAT
//#define	PF_HYLINK	AF_HYLINK
//#define	PF_APPLETALK	AF_APPLETALK
//#define	PF_ROUTE	AF_ROUTE
//#define	PF_LINK		AF_LINK
//#define	PF_XTP		pseudo_AF_XTP	/* really just proto family, no AF */
//#define	PF_COIP		AF_COIP
//#define	PF_CNT		AF_CNT
//#define	PF_SIP		AF_SIP
//#define	PF_IPX		AF_IPX		/* same format as AF_NS */
//#define PF_RTIP		pseudo_AF_FTIP	/* same format as AF_INET */
//#define PF_PIP		pseudo_AF_PIP

//#define	PF_MAX		AF_MAX

/*
 * Definitions for network related sysctl, CTL_NET.
 *
 * Second level is protocol family.
 * Third level is protocol number.
 *
 * Further levels are defined by the individual families below.
 */
#define NET_MAXID	AF_MAX

#define CTL_NET_NAMES { \
	{ 0, 0 }, \
	{ "local", CTLTYPE_NODE }, \
	{ "inet", CTLTYPE_NODE }, \
	{ "implink", CTLTYPE_NODE }, \
	{ "pup", CTLTYPE_NODE }, \
	{ "chaos", CTLTYPE_NODE }, \
	{ "xerox_ns", CTLTYPE_NODE }, \
	{ "iso", CTLTYPE_NODE }, \
	{ "emca", CTLTYPE_NODE }, \
	{ "datakit", CTLTYPE_NODE }, \
	{ "ccitt", CTLTYPE_NODE }, \
	{ "ibm_sna", CTLTYPE_NODE }, \
	{ "decnet", CTLTYPE_NODE }, \
	{ "dec_dli", CTLTYPE_NODE }, \
	{ "lat", CTLTYPE_NODE }, \
	{ "hylink", CTLTYPE_NODE }, \
	{ "appletalk", CTLTYPE_NODE }, \
	{ "route", CTLTYPE_NODE }, \
	{ "link_layer", CTLTYPE_NODE }, \
	{ "xtp", CTLTYPE_NODE }, \
	{ "coip", CTLTYPE_NODE }, \
	{ "cnt", CTLTYPE_NODE }, \
	{ "rtip", CTLTYPE_NODE }, \
	{ "ipx", CTLTYPE_NODE }, \
	{ "sip", CTLTYPE_NODE }, \
	{ "pip", CTLTYPE_NODE }, \
}

/*
 * Socket state bits.
 */
#define  SS_NOFDREF     0x001 /* no file table ref any more */
#define  SS_ISCONNECTED    0x002 /* socket connected to a peer */
#define  SS_ISCONNECTING      0x004 /* in process of connecting to peer */
#define  SS_ISDISCONNECTING   0x008 /* in process of disconnecting */
#define  SS_CANTSENDMORE      0x010 /* can't send more data to peer */
#define  SS_CANTRCVMORE    0x020 /* can't receive more data from peer */
#define  SS_RCVATMARK      0x040 /* at mark on input */
      
#define  SS_PRIV        0x080 /* privileged for broadcast, raw... */
#define  SS_NBIO        0x100 /* non-blocking ops */
#define  SS_ASYNC    0x200 /* async i/o notify */
#define  SS_ISCONFIRMING      0x400 /* deciding to accept connection req */

#endif /* USNET_SOCKET_H_ */
