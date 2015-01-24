/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)tcp_subr.c	8.2 (Berkeley) 5/24/95
 */

#include <arpa/inet.h>

#include "usnet_tcp_subr.h"
#include "usnet_tcp.h"
#include "usnet_tcpip.h"
#include "usnet_tcp_seq.h"
#include "usnet_tcp_var.h"
#include "usnet_tcp_timer.h"
#include "usnet_tcp_fsm.h"
#include "usnet_protosw.h"
#include "usnet_socket_util.h"
#include "usnet_in_pcb.h"
#include "usnet_common.h"
#include "usnet_ip_out.h"



/* patchable/settable parameters for tcp */
int 	g_tcp_mssdflt = TCP_MSS;
int 	g_tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
int	g_tcp_do_rfc1323 = 1;
int   g_max_linkhdr;         /* largest link-level header */
int   g_max_iphdr;         /* largest ip header */
int   g_max_tcphdr;         /* largest tcp header */
int   g_max_protohdr;        /* largest protocol header */
int   g_max_hdr;       /* largest link+protocol header */
int   g_max_datalen;         /* MHLEN - max_hdr */

extern struct inpcb *g_tcp_last_inpcb;

/*
 * Tcp initialization
 */
void
tcp_init()
{
	g_tcp_iss = random();	/* wrong, but better than a constant */

	g_tcb.inp_next = g_tcb.inp_prev = &g_tcb;

	if (g_max_protohdr < sizeof(struct tcpiphdr))
		g_max_protohdr = sizeof(struct tcpiphdr);
   // FIXME: it's always false for ethernet frame
	if (g_max_linkhdr + sizeof(struct tcpiphdr) > BUF_MSIZE)
		DEBUG("tcp_init"); // panic

   // from tcp_slowtimo
	g_tcp_maxidle = g_tcp_keepcnt * g_tcp_keepintvl;

   g_max_linkhdr = 16;
   g_max_iphdr = 60;
   g_max_tcphdr = 60;
   //g_max_protohdr = 0;
   //g_max_hdr = 0;
   //g_max_datalen = 0;         /* MHLEN - max_hdr */


}

/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 */
struct tcpiphdr *
tcp_template(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	usn_mbuf_t *m;
	struct tcpiphdr *n;

	if ((n = tp->t_template) == 0) {
      // TODO: alloc exact size!
		m = usn_get_mbuf(0, BUF_MSIZE, 0);
		if (m == NULL)
			return (0);
		m->mlen = sizeof (struct tcpiphdr);
		n = mtod(m, struct tcpiphdr *);
	}
	n->ti_next = n->ti_prev = 0;
	n->ti_x1 = 0;
	n->ti_pr = IPPROTO_TCP;
	n->ti_len = htons(sizeof (struct tcpiphdr) - sizeof (usn_ip_t));
	n->ti_src = inp->inp_laddr;
	n->ti_dst = inp->inp_faddr;
	n->ti_sport = inp->inp_lport;
	n->ti_dport = inp->inp_fport;
	n->ti_seq = 0;
	n->ti_ack = 0;
	n->ti_x2 = 0;
	n->ti_off = 5;
	n->ti_flags = 0;
	n->ti_win = 0;
	n->ti_sum = 0;
	n->ti_urp = 0;
	return (n);
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == 0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 */
void
tcp_respond(struct tcpcb *tp, struct tcpiphdr *ti,
	         usn_mbuf_t *m, tcp_seq ack, tcp_seq seq, int flags)
{
	int tlen;
	int win = 0;
	struct route *ro = 0;

	if (tp) {
		win = sbspace(&tp->t_inpcb->inp_socket->so_rcv);
		ro = &tp->t_inpcb->inp_route;
	}
	if (m == 0) {
		//m = m_gethdr(M_DONTWAIT, MT_HEADER);
      m = (usn_mbuf_t*)usn_get_mbuf(0, BUF_MSIZE, 0);
		if (m == NULL)
			return;
		tlen = 0;
		m->head += g_max_linkhdr;
		m->mlen = sizeof (struct tcpiphdr);
		*mtod(m, struct tcpiphdr *) = *ti;
		ti = mtod(m, struct tcpiphdr *);
		flags = TH_ACK;
	} else {
      // FIXME: reuse the fisrt mbuf, i.e. packet header.
		usn_free_mbuf(m->next);
		m->next = 0;
		bcopy((caddr_t)ti, m->head, sizeof (struct tcpiphdr));
		m->mlen = sizeof (struct tcpiphdr);
		tlen = 0;
#define xchg(a,b,type) { type t; t=a; a=b; b=t; }
		xchg(ti->ti_dst.s_addr, ti->ti_src.s_addr, u_int32);
		xchg(ti->ti_dport, ti->ti_sport, u_short);
#undef xchg
	}
	ti->ti_len = htons((u_short)(sizeof (struct tcphdr) + tlen));
	tlen += sizeof (struct tcpiphdr);
	m->mlen = tlen;
	ti->ti_next = ti->ti_prev = 0;
	ti->ti_x1 = 0;
	ti->ti_seq = htonl(seq);
	ti->ti_ack = htonl(ack);
	ti->ti_x2 = 0;
	ti->ti_off = sizeof (struct tcphdr) >> 2;
	ti->ti_flags = flags;
	if (tp)
		ti->ti_win = htons((u_short) (win >> tp->rcv_scale));
	else
		ti->ti_win = htons((u_short)win);
	ti->ti_urp = 0;
	ti->ti_sum = 0;
	ti->ti_sum = in_cksum(m, tlen);
	((usn_ip_t *)ti)->ip_len = tlen;
	((usn_ip_t *)ti)->ip_ttl = g_ip_defttl;
	(void) ipv4_output(m, NULL, ro, 0);
   return;
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.
 */
struct tcpcb*
tcp_newtcpcb( struct inpcb *inp)
{
	struct tcpcb *tp;

	//tp = malloc(sizeof(*tp), M_PCB, M_NOWAIT);
	tp = (struct tcpcb*)usn_get_buf(0,sizeof(*tp));

	if (tp == NULL)
		return ((struct tcpcb *)0);

	bzero((char *) tp, sizeof(struct tcpcb));

	tp->seg_next = tp->seg_prev = (struct tcpiphdr *)tp;
	tp->t_maxseg = g_tcp_mssdflt;

	tp->t_flags = g_tcp_do_rfc1323 ? (TF_REQ_SCALE|TF_REQ_TSTMP) : 0;
	tp->t_inpcb = inp;

	 // Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 // rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
	 // reasonable initial retransmit time.

	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar = g_tcp_rttdflt * PR_SLOWHZ << 2;
	tp->t_rttmin = TCPTV_MIN;
	TCPT_RANGESET(tp->t_rxtcur, 
	    ((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1,
	    TCPTV_MIN, TCPTV_REXMTMAX);
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	inp->inp_ip.ip_ttl = g_ip_defttl;
	inp->inp_ppcb = (caddr_t)tp;
	return (tp);
}

/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */
struct tcpcb* tcp_drop(struct tcpcb* tp, u_int error)
{
	struct usn_socket *so = tp->t_inpcb->inp_socket;

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		tcp_output(tp);
		g_tcpstat.tcps_drops++;
	} else
		g_tcpstat.tcps_conndrops++;
	if (error == ETIMEDOUT && tp->t_softerror)
		error = tp->t_softerror;
	so->so_error = error;
	return (tcp_close(tp));
}

/*
 * Close a TCP control block:
 *	discard all space held by the tcp
 *	discard internet protocol block
 *	wake up any sleepers
 */
struct tcpcb*
tcp_close(struct tcpcb *tp)
{
	struct tcpiphdr *t;
	struct inpcb *inp = tp->t_inpcb;
	struct usn_socket *so = inp->inp_socket;
	usn_mbuf_t *m;
#ifdef RTV_RTT
	struct rtentry *rt;

	// If we sent enough data to get some meaningful characteristics,
	// save them in the routing entry.  'Enough' is arbitrarily 
	// defined as the sendpipesize (default 4K) * 16.  This would
	// give us 16 rtt samples assuming we only get one sample per
	// window (the usual case on a long haul net).  16 samples is
	// enough for the srtt filter to converge to within 5% of the correct
	// value; fewer samples and we could save a very bogus rtt.
	//
	// Don't update the default route's characteristics and don't
	// update anything that the user "locked".
	if (SEQ_LT(tp->iss + so->so_snd.sb_hiwat * 16, tp->snd_max) &&
	    (rt = inp->inp_route.ro_rt) &&
	    ((struct usn_sockaddr_in *)rt_key(rt))->sin_addr.s_addr != USN_INADDR_ANY) {
		u_long i;

		if ((rt->rt_rmx.rmx_locks & RTV_RTT) == 0) {
			i = tp->t_srtt *
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTT_SCALE));
			if (rt->rt_rmx.rmx_rtt && i)
		
				// filter this update to half the old & half
				// the new values, converting scale.
				// See route.h and tcp_var.h for a
				// description of the scaling constants.
				rt->rt_rmx.rmx_rtt =
				    (rt->rt_rmx.rmx_rtt + i) / 2;
			else
				rt->rt_rmx.rmx_rtt = i;
		}
		if ((rt->rt_rmx.rmx_locks & RTV_RTTVAR) == 0) {
			i = tp->t_rttvar *
			    (RTM_RTTUNIT / (PR_SLOWHZ * TCP_RTTVAR_SCALE));
			if (rt->rt_rmx.rmx_rttvar && i)
				rt->rt_rmx.rmx_rttvar =
				    (rt->rt_rmx.rmx_rttvar + i) / 2;
			else
				rt->rt_rmx.rmx_rttvar = i;
		}

		// update the pipelimit (ssthresh) if it has been updated
		// already or if a pipesize was specified & the threshhold
		// got below half the pipesize.  I.e., wait for bad news
		// before we start updating, then update on both good
		// and bad news.
		if (((rt->rt_rmx.rmx_locks & RTV_SSTHRESH) == 0 &&
		    (i = tp->snd_ssthresh) && rt->rt_rmx.rmx_ssthresh) ||
		    i < (rt->rt_rmx.rmx_sendpipe / 2)) {

			// convert the limit from user data bytes to
			// packets then to packet data bytes.

			i = (i + tp->t_maxseg / 2) / tp->t_maxseg;
			if (i < 2)
				i = 2;
			i *= (u_long)(tp->t_maxseg + sizeof (struct tcpiphdr));
			if (rt->rt_rmx.rmx_ssthresh)
				rt->rt_rmx.rmx_ssthresh =
				    (rt->rt_rmx.rmx_ssthresh + i) / 2;
			else
				rt->rt_rmx.rmx_ssthresh = i;
		}
	}
#endif // RTV_RTT 
	// free the reassembly queue, if any
	t = tp->seg_next;
	while (t != (struct tcpiphdr *)tp) {
		t = (struct tcpiphdr *)t->ti_next;
		m = REASS_MBUF((struct tcpiphdr *)t->ti_prev);
      // FIXME: define it
		//remque(t->ti_prev);
		usn_free_mbuf(m);
	}

	if (tp->t_template)
		usn_free_buf((u_char*)tp->t_template);

	usn_free_buf((u_char*)tp);

	inp->inp_ppcb = 0;
	soisdisconnected(so);

	// clobber input pcb cache if we're closing the cached connection
	if (inp == g_tcp_last_inpcb)
		g_tcp_last_inpcb = &g_tcb;

	in_pcbdetach(inp);

	g_tcpstat.tcps_closed++;
	return ((struct tcpcb *)0);
}

void
tcp_drain()
{

}

/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 */
void
tcp_notify(struct inpcb *inp, int error)
{
   struct tcpcb *tp = (struct tcpcb *)inp->inp_ppcb;
   struct usn_socket *so = inp->inp_socket;

	// Ignore some errors if we are hooked up.
	// If connection hasn't completed, has retransmitted several times,
	// and receives a second error, give up now.  This is better
	// than waiting a long time to establish a connection that
	// can never complete.
	if (tp->t_state == TCPS_ESTABLISHED &&
	     (error == EHOSTUNREACH || error == ENETUNREACH ||
	      error == EHOSTDOWN)) {
		return;
	} else if (tp->t_state < TCPS_ESTABLISHED && tp->t_rxtshift > 3 &&
	    tp->t_softerror)
		so->so_error = error;
	else 
		tp->t_softerror = error;

   // FIXME: callbacks
	//wakeup((caddr_t) &so->so_timeo);
	//sorwakeup(so);
	//sowwakeup(so);
   return; 
}

void
tcp_ctlinput( int cmd, struct usn_sockaddr *sa,usn_ip_t *ip)
{
	struct tcphdr *th;
	//extern struct in_addr g_zeroin_addr;
	//extern u_char inetctlerrmap[];
	void (*notify) __P((struct inpcb *, int)) = tcp_notify;

	if (cmd == PRC_QUENCH)
		notify = tcp_quench;
	else if (!PRC_IS_REDIRECT(cmd) &&
		 ((unsigned)cmd > PRC_NCMDS || g_inetctlerrmap[cmd] == 0))
		return;
	if (ip) {
		th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		in_pcbnotify(&g_tcb, sa, th->th_dport, ip->ip_src, th->th_sport,
			cmd, notify);
	} else
		in_pcbnotify(&g_tcb, sa, 0, g_zeroin_addr, 0, cmd, notify);
}

/*
 * When a source quench is received, close congestion window
 * to one segment.  We will gradually open it again as we proceed.
 */
void   
tcp_quench(struct inpcb* inp, int error)
{
	struct tcpcb *tp = intotcpcb(inp);
	if (tp)
		tp->snd_cwnd = tp->t_maxseg;
   return;
}
