/*
 * Copyright (c) 1982, 1986, 1993
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
 * @(#)if_ether.h 8.3 (Berkeley) 5/2/95
 */

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

#define ETHERTYPE_PUP		0x0200	/* PUP protocol */
#define ETHERTYPE_IP		0x0800	/* IP protocol */
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

