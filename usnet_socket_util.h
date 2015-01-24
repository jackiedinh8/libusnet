/*-
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
 * @(#)socketvar.h   8.3 (Berkeley) 2/19/95
 */


#ifndef _USNET_SOCKET_UTIL_H_
#define _USNET_SOCKET_UTIL_H_

#include <stdlib.h>
#include <stdint.h>

#include "usnet_buf.h"
#include "usnet_core.h"
#include "usnet_protosw.h"

/*
 * Used to maintain information about processes that wish to be
 * notified when I/O becomes possible.
 */
struct usn_selinfo {
   int   si_pid;     /* process to be notified */
   short si_flags;   /* see below */
};

/*
 * Data structure per socket.
 * Contains send and receive buffer queues,
 * handle on protocol and pointer to protocol
 * private data and error information.
 */
struct usn_socket;
typedef int (*pr_usrreq_func_t)(struct usn_socket*, int, usn_mbuf_t*, usn_mbuf_t*, usn_mbuf_t*);
struct usn_socket {
   short    so_type;    /* generic type, see socket.h */
   short    so_family;  /* family address */ 
#define     so_domain so_family
   short    so_options;    /* from socket call, see socket.h */
   short    so_linger;     /* time to linger while closing */
   short    so_state;      /* internal state flags USN_*, below */
   u_int32  so_fd;         /* file descriptor */
   caddr_t  so_pcb;        /* protocol control block */
   /* user-protocol hook */
   pr_usrreq_func_t so_usrreq;      /* user request: see list below */
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
   struct   usn_socket *so_head;  /* back pointer to accept socket */
   struct   usn_socket *so_q0;    /* queue of partial connections */
   struct   usn_socket *so_q;     /* queue of incoming connections */
   short            so_q0len;      /* partials on so_q0 */
   short            so_qlen;    /* number of connections on so_q */
   short            so_qlimit;     /* max number queued connections */
   short            so_timeo;      /* connection timeout */
   u_short          so_error;      /* error affecting connection */
   int              so_pgid;    /* pgid for signals */
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
      struct usn_selinfo sb_sel;   /* process selecting read/write */
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

   //caddr_t  so_tpcb;    // Wisc. protocol control block XXX
   void  (*so_upcall) (struct usn_socket *so, caddr_t arg, int waitf);
   caddr_t  so_upcallarg;     /* Arg for above */
};

struct usn_ctlmsg {
   u_int   len;      /* data byte count, including hdr */
   u_int   level;    /* originating protocol */
   u_int   type;     /* protocol-specific type */
/* followed by u_char  cmsg_data[]; */
};    

/* given pointer to struct cmsghdr, return pointer to data */
#define  CTLMSG_DATA(ctlmsg)      ((u_char *)((ctlmsg) + 1))

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
#define	SO_DEBUG	      0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER	   0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define	SO_REUSEPORT	0x0200		/* allow local address & port reuse */

/*
 * Additional options, not kept in so_options.
 */
#define  SO_SNDBUF	0x1001		/* send buffer size */
#define  SO_RCVBUF	0x1002		/* receive buffer size */
#define  SO_SNDLOWAT	0x1003		/* send low-water mark */
#define  SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define  SO_SNDTIMEO	0x1005		/* send timeout */
#define  SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	   0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */

/*
 * Structure used for manipulating linger option.
 */
struct	usn_linger {
	int	l_onoff;		/* option on/off */
	int	l_linger;		/* linger time in seconds */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct usn_sockaddr {
	u_char	sa_len;			/* total length */
	u_char	sa_family;		/* address family */
	char	sa_data[14];		/* actually longer; address value */
};

/*
 * Structure used by kernel to pass protocol
 * information in raw sockets.
 */
struct usn_sockproto {
	u_short	sp_family;		/* address family */
	u_short	sp_protocol;	/* protocol */
};

/*
 * Socket state bits.
 */
#define  USN_NOFDREF           0x001 /* no file table ref any more */
#define  USN_ISCONNECTED       0x002 /* socket connected to a peer */
#define  USN_ISCONNECTING      0x004 /* in process of connecting to peer */
#define  USN_ISDISCONNECTING   0x008 /* in process of disconnecting */
#define  USN_CANTSENDMORE      0x010 /* can't send more data to peer */
#define  USN_CANTRCVMORE       0x020 /* can't receive more data from peer */
#define  USN_RCVATMARK         0x040 /* at mark on input */
      
#define  USN_PRIV              0x080 /* privileged for broadcast, raw... */
#define  USN_NBIO              0x100 /* non-blocking ops, user-space */
#define  USN_ASYNC             0x200 /* async i/o notify, user-space */
#define  USN_ISCONFIRMING      0x400 /* deciding to accept connection req */

#define  USN_DEF_BACKLOG  5
#define  USN_SOMAXCONN    512

/*
 * How much space is there in a socket buffer (so->so_snd or so->so_rcv)?
 * This is problematical if the fields are unsigned, as the space might
 * still be negative (cc > hiwat or mbcnt > mbmax).  Should detect
 * overflow and return 0.  Should use "lmin" but it doesn't exist now.
 */
#define  sbspace(sb) \
    ((u_long) min((u_int)((sb)->sb_hiwat - (sb)->sb_cc), \
    (u_int)((sb)->sb_mbmax - (sb)->sb_mbcnt)))

/* adjust counters in sb reflecting allocation of m */
#define  sballoc(sb, m) { \
   (sb)->sb_cc += (m)->mlen; \
   (sb)->sb_mbcnt += m->end - m->head; \
}

/* adjust counters in sb reflecting freeing of m */
#define  sbfree(sb, m) { \
      (sb)->sb_cc -= (m)->mlen; \
      (sb)->sb_mbcnt -= BUF_MSIZE; \
      /*if ((m)->m_flags & M_EXT)*/ \
         /*(sb)->sb_mbcnt -= (m)->m_ext.ext_size;*/ \
}

int32 
sbappendaddr( struct sockbuf *sb, struct usn_sockaddr *asa, 
                    usn_mbuf_t *m0, usn_mbuf_t *control);

void    
sbappend(struct sockbuf *sb, usn_mbuf_t *m);

int
soreserve(struct usn_socket *so, u_long sndcc, u_long rcvcc);

int
sbreserve(struct sockbuf *sb, u_long cc);

void
soisdisconnected(struct usn_socket *so);

extern u_long   g_sb_max;
/* to catch callers missing new second argument to sonewconn: */
#define sonewconn(head, connstatus)   sonewconn1((head), (connstatus))
struct usn_socket *
sonewconn1 (struct usn_socket *head, int connstatus);

void
sbdrop(struct sockbuf *sb, int len);

#define  sorwakeup(so)  { sowakeup((so), &(so)->so_rcv); \
           if ((so)->so_upcall) \
             (*((so)->so_upcall))((so), (so)->so_upcallarg, 0 /*M_DONTWAIT*/); \
         }

#define  sowwakeup(so)  sowakeup((so), &(so)->so_snd)
void
sowakeup(struct usn_socket *so, struct sockbuf *sb);

void
soqinsque(struct usn_socket *head, struct usn_socket *so, int q);

int
soqremque(struct usn_socket *so, int q);

void
soisconnected(struct usn_socket *so);

void  
sohasoutofband(struct usn_socket *so);

void
socantrcvmore( struct usn_socket *so);

int
soabort( struct usn_socket *so);

#endif /* USNET_SOCKET_UTIL_H_ */
