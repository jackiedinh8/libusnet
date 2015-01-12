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
 * @(#)ip.h 8.2 (Berkeley) 6/1/94
 */             

#ifndef _USNET_IP_HEADER_H_
#define _USNET_IP_HEADER_H_

#include "usnet_types.h"
#include "usnet_core.h"
#include "usnet_in.h"
#include "usnet_buf_utils.h"
#include "usnet_log.h"
#include "usnet_buf.h"
#include "usnet_ip_var.h"

/*
 * Definitions for internet protocol version 4.
 * Per RFC 791, September 1981.
 */
#define	IPVERSION	4

/*
 * Structure of an internet header, naked of options.
 *
 * We declare ip_len and ip_off to be short, rather than u_short
 * pragmatically since otherwise unsigned comparisons can result
 * against negative integers quite easily, and fail in subtle ways.
 */

#define mtoiphdr(m) (usn_ip_t*)((m)->start + sizeof(ether_header_t))
#define GETIP(m) ((usn_ip_t*)((m)->head))

typedef struct usn_ip usn_ip_t;
struct usn_ip {
#if BYTE_ORDER == LITTLE_ENDIAN 
	u_char	ip_hl:4,		/* header length */
		      ip_v:4;			/* version */
#endif
#if BYTE_ORDER == BIG_ENDIAN 
	u_char	ip_v:4,			/* version */
      		ip_hl:4;		/* header length */
#endif
	u_char	ip_tos;			/* type of service */
	short	   ip_len;			/* total length */
	u_short	ip_id;			/* identification */
	short	   ip_off;			/* fragment offset field */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
	u_char	ip_ttl;			/* time to live */
	u_char	ip_p;			/* protocol */
	u_short	ip_sum;			/* checksum */
	struct usn_in_addr    ip_src,ip_dst;	/* source and dest address */
}__attribute__((packed));

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


#define	IP_MAXPACKET	65535		/* maximum packet size */

/*
 * Definitions for IP type of service (ip_tos)
 */
#define	IPTOS_LOWDELAY		0x10
#define	IPTOS_THROUGHPUT	0x08
#define	IPTOS_RELIABILITY	0x04

/*
 * Definitions for IP precedence (also in ip_tos) (hopefully unused)
 */
#define	IPTOS_PREC_NETCONTROL		0xe0
#define	IPTOS_PREC_INTERNETCONTROL	0xc0
#define	IPTOS_PREC_CRITIC_ECP		0xa0
#define	IPTOS_PREC_FLASHOVERRIDE	0x80
#define	IPTOS_PREC_FLASH		0x60
#define	IPTOS_PREC_IMMEDIATE		0x40
#define	IPTOS_PREC_PRIORITY		0x20
#define	IPTOS_PREC_ROUTINE		0x00

/*
 * Definitions for options.
 */
#define	IPOPT_COPIED(o)		((o)&0x80)
#define	IPOPT_CLASS(o)		((o)&0x60)
#define	IPOPT_NUMBER(o)		((o)&0x1f)

#define	IPOPT_CONTROL		0x00
#define	IPOPT_RESERVED1		0x20
#define	IPOPT_DEBMEAS		0x40
#define	IPOPT_RESERVED2		0x60

#define	IPOPT_EOL		0		/* end of option list */
#define	IPOPT_NOP		1		/* no operation */

#define	IPOPT_RR		7		/* record packet route */
#define	IPOPT_TS		68		/* timestamp */
#define	IPOPT_SECURITY		130		/* provide s,c,h,tcc */
#define	IPOPT_LSRR		131		/* loose source route */
#define	IPOPT_SATID		136		/* satnet id */
#define	IPOPT_SSRR		137		/* strict source route */

/*
 * Offsets to fields in options other than EOL and NOP.
 */
#define	IPOPT_OPTVAL		0		/* option ID */
#define	IPOPT_OLEN  	   1		/* option length */
#define  IPOPT_OFFSET		2		/* offset within option */
#define	IPOPT_MINOFF		4		/* min value of above */

/*
 * Time stamp option structure.
 */
struct	ip_timestamp {
	u_char	ipt_code;		/* IPOPT_TS */
	u_char	ipt_len;		/* size of structure (variable) */
	u_char	ipt_ptr;		/* index of current entry */
#if BYTE_ORDER == LITTLE_ENDIAN 
	u_char	ipt_flg:4,		/* flags, see below */
		      ipt_oflw:4;		/* overflow counter */
#endif
#if BYTE_ORDER == BIG_ENDIAN 
	u_char	ipt_oflw:4,		/* overflow counter */
		      ipt_flg:4;		/* flags, see below */
#endif
	union ipt_timestamp {
		u_long	ipt_time[1];
		struct	ipt_ta {
			struct usn_in_addr ipt_addr;
			u_long ipt_time;
		} ipt_ta[1];
	} ipt_timestamp;
};

/* flag bits for ipt_flg */
#define	IPOPT_TS_TSONLY		0		/* timestamps only */
#define	IPOPT_TS_TSANDADDR	1		/* timestamps and addresses */
#define	IPOPT_TS_PRESPEC	3		/* specified modules only */

/* bits for security (not byte swapped) */
#define	IPOPT_SECUR_UNCLASS	0x0000
#define	IPOPT_SECUR_CONFID	0xf135
#define	IPOPT_SECUR_EFTO	0x789a
#define	IPOPT_SECUR_MMMM	0xbc4d
#define	IPOPT_SECUR_RESTR	0xaf13
#define	IPOPT_SECUR_SECRET	0xd788
#define	IPOPT_SECUR_TOPSECRET	0x6bc5

/*
 * Internet implementation parameters.
 */
#define	MAXTTL		255		/* maximum time to live (seconds) */
#define	IPDEFTTL	64		/* default ttl, from RFC 1340 */
#define	IPFRAGTTL	60		/* time to live for frags, slowhz */
#define	IPTTLDEC	1		/* subtracted when forwarding */

#define	IP_MSS		576		/* default maximum segment size */

/*
 * Structure stored in mbuf in inpcb.ip_options
 * and passed to ip_output when ip options are in use.
 * The actual length of the options (including ipopt_dst)
 * is in m_len.
 */   
#define MAX_IPOPTLEN 40
   
struct ipoption {       
   struct usn_in_addr ipopt_dst;   /* first-hop dst if source routed */
   char               ipopt_list[MAX_IPOPTLEN];  /* options proper */
};


/* Forwarding Config */
#define  IP_FORWARDING     0x1      /* most of ip header exists */
#define  IP_RAWOUTPUT      0x2      /* raw ip header exists */
#define  IP_ROUTETOIF      SO_DONTROUTE   /* bypass routing tables */
#define  IP_ALLOWBROADCAST SO_BROADCAST   /* can send broadcast packets */


extern u_short  g_ip_id;            /* ip packet ctr, for ids */
extern int g_ip_defttl;

void 
usnet_ipv4_init();

unsigned short 
in_cksum(usn_mbuf_t *m, int len);

void 
ipv4_input(usn_mbuf_t* m);

void 
handle_ipv4_old(u_char *m, int len, struct netmap_ring *ring, int cur);

void
ip_stripoptions( usn_mbuf_t *m, usn_mbuf_t *mopt);

inline void
usn_insert_ipq(struct ipq *fp);

inline void
usn_remove_ipq(struct ipq *fp);

inline void
insert_ipfrag(struct ipq *fp, struct ipasfrag *ip);


inline void
ip_enq(struct ipasfrag *p, struct ipasfrag *prev);

#define  IA_SIN(ia) (&(((struct in_ifaddr *)(ia))->ia_addr))

#endif // !_USNET_IP_HEADER_H_

