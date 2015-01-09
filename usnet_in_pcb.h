/*
 * Copyright (c) 1982, 1986, 1990, 1993
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
 * @(#)in_pcb.h   8.1 (Berkeley) 6/10/93
 */


#ifndef _USNET_IN_PCB_H_
#define _USNET_IN_PCB_H_

#include "usnet_in.h"
#include "usnet_ip.h"
#include "usnet_route.h"

typedef struct usn_appcb usn_appcb_t;
struct usn_appcb {
   int                fd;
   void              *arg;
   accept_handler_cb  accept_cb; 
   socket_handler_cb  socket_cb; 
   error_handler_cb   error_cb; 
};

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */
struct inpcb {
	struct inpcb *inp_next,*inp_prev; /* pointers to other pcb's */
	struct inpcb *inp_head;	          /* pointer back to chain of inpcb's
					                         for this protocol */
	struct usn_in_addr inp_faddr;	    /* foreign host table entry */
	u_short inp_fport;    	          /* foreign port */
	struct usn_in_addr inp_laddr;	    /* local host table entry */
	u_short inp_lport;		          /* local port */
	struct usn_socket *inp_socket;        /* back pointer to socket */
	caddr_t inp_ppcb;		             /* pointer to per-protocol pcb */
	struct route inp_route;	          /* placeholder for routing entry */
	int inp_flags;		                /* generic IP/datagram flags */
	usn_ip_t    inp_ip;		          /* header prototype; should have more */
	usn_mbuf_t *inp_options;	       /* IP options */
	//struct ip_moptions *inp_moptions; /* IP multicast options */
   usn_appcb_t inp_appcb;                /* application specific settings */
};

/* flags in inp_flags: */
#define	INP_RECVOPTS		0x01	/* receive incoming IP options */
#define	INP_RECVRETOPTS		0x02	/* receive IP options for reply */
#define	INP_RECVDSTADDR		0x04	/* receive IP dst address */
#define	INP_CONTROLOPTS		(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR)
#define	INP_HDRINCL		0x08	/* user supplies entire IP header */

#define	INPLOOKUP_WILDCARD	1
#define	INPLOOKUP_SETLOCAL	2

#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb)

typedef void (*notify_func_t)(struct inpcb *, int); 

int	 in_losing (struct inpcb *);
int	 in_pcballoc (struct usn_socket *, struct inpcb *);
int	 in_pcbbind (struct inpcb *, usn_mbuf_t *);
int	 in_pcbconnect (struct inpcb *, usn_mbuf_t *);
int	 in_pcbdetach (struct inpcb *);
int	 in_pcbdisconnect (struct inpcb *);
struct inpcb * in_pcblookup (struct inpcb *,
	    struct usn_in_addr, u_int, struct usn_in_addr, u_int, int);
int    in_pcbnotify ( struct inpcb *head, struct usn_sockaddr *dst,
	            u_int fport_arg, u_int lport_arg, struct usn_in_addr laddr, 
               int cmd, //void (*notify)(struct inpcb *, int));
               notify_func_t notify);
void	 in_rtchange (struct inpcb *inp, int errno);
int	 in_setpeeraddr (struct inpcb *, usn_mbuf_t *);
int	 in_setsockaddr (struct inpcb *, usn_mbuf_t *);


int    in_pcblisten(struct inpcb *inp, usn_mbuf_t *nam);

#endif //!_USNET_IN_PCB_H_
