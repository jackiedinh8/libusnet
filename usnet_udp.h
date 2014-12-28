#ifndef _USNET_UDP_H_
#define _USNET_UDP_H_

#include "usnet_types.h"
#include "usnet_buf.h"
#include "usnet_in_pcb.h"

/*
 * Udp protocol header.
 * Per RFC 768, September, 1981.
 */
typedef struct usn_udphdr usn_udphdr_t;
struct usn_udphdr {
	u_short	uh_sport;		/* source port */
	u_short	uh_dport;		/* destination port */
	short	   uh_ulen;		/* udp length */
	u_short	uh_sum;			/* udp checksum */
};

/*
 * UDP kernel structures and variables.
 */

/*
 * Overlay for ip header used by other protocols (tcp, udp).
 */
struct ipovly {
   union {
      caddr_t  ih_next; /* for protocol sequence q's */
      caddr_t  ih_prev; 
   };
//#define	ih_prev		ih_next
   u_char   ih_x1;         /* (unused) */
   u_char   ih_pr;         /* protocol */
   short    ih_len;        /* protocol length */
   struct   usn_in_addr ih_src;      /* source internet address */
   struct   usn_in_addr ih_dst;      /* destination internet address */
} __attribute__((packed));

struct	udpiphdr {
	struct 	ipovly   ui_i;		/* overlaid ip structure */
	usn_udphdr_t          ui_u;		/* udp header */
};
#define	ui_next		ui_i.ih_next
#define	ui_prev		ui_i.ih_prev
#define	ui_x1		ui_i.ih_x1
#define	ui_pr		ui_i.ih_pr
#define	ui_len		ui_i.ih_len
#define	ui_src		ui_i.ih_src
#define	ui_dst		ui_i.ih_dst
#define	ui_sport	ui_u.uh_sport
#define	ui_dport	ui_u.uh_dport
#define	ui_ulen		ui_u.uh_ulen
#define	ui_sum		ui_u.uh_sum

struct	udpstat {
				/* input statistics: */
	u_long	udps_ipackets;		/* total input packets */
	u_long	udps_hdrops;		/* packet shorter than header */
	u_long	udps_badsum;		/* checksum error */
	u_long	udps_badlen;		/* data length larger than packet */
	u_long	udps_noport;		/* no socket on port */
	u_long	udps_noportbcast;	/* of above, arrived as broadcast */
	u_long	udps_fullsock;		/* not delivered, input socket full */
	u_long	udpps_pcbcachemiss;	/* input packets missing pcb cache */
				/* output statistics: */
	u_long	udps_opackets;		/* total output packets */
};

/*
 * Names for UDP sysctl objects
 */
#define	UDPCTL_CHECKSUM		1	/* checksum UDP packets */
#define UDPCTL_MAXID		2

#define UDPCTL_NAMES { \
	{ 0, 0 }, \
	{ "checksum", CTLTYPE_INT }, \
}


extern struct inpcb g_udb;

void
udp_init();

int
udp_output(struct inpcb *inp, usn_mbuf_t *m, usn_mbuf_t *addr, usn_mbuf_t  *control);

void
udp_input( usn_mbuf_t *m, u_int iphlen);

#endif //!_USNET_UDP_H_
