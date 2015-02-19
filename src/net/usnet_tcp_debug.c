/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)tcp_debug.c	8.1 (Berkeley) 6/10/93
 */

#include "usnet_tcp_debug.h"
#include "usnet_tcp_fsm.h"
#include "usnet_tcpip.h"
#include "usnet_ip_icmp.h"


int g_tcpconsdebug = 1;
char *prurequests[] = {
	"ATTACH",	"DETACH",	"BIND",		"LISTEN",
	"CONNECT",	"ACCEPT",	"DISCONNECT",	"SHUTDOWN",
	"RCVD",		"SEND",		"ABORT",	"CONTROL",
	"SENSE",	"RCVOOB",	"SENDOOB",	"SOCKADDR",
	"PEERADDR",	"CONNECT2",	"FASTTIMO",	"SLOWTIMO",
	"PROTORCV",	"PROTOSEND",
};
char *tcptimers[] =
    { "REXMT", "PERSIST", "KEEP", "2MSL" };
char *tcpstates[] = {
	"CLOSED",	"LISTEN",	"SYN_SENT",	"SYN_RCVD",
	"ESTABLISHED",	"CLOSE_WAIT",	"FIN_WAIT_1",	"CLOSING",
	"LAST_ACK",	"FIN_WAIT_2",	"TIME_WAIT",
};
char	*tanames[] =
    { "input", "output", "user", "respond", "drop" };


/*
 * Tcp debug routines
 */
void
tcp_trace( short act, short ostate, struct tcpcb *tp, 
           struct tcpiphdr *ti, int req)
{
   struct usn_socket *so;
	tcp_seq seq, ack;
	int len, flags;
	struct tcp_debug *td = &g_tcp_debug[g_tcp_debx++];

   (void)so;

	if (g_tcp_debx == TCP_NDEBUG)
		g_tcp_debx = 0;
	td->td_time = iptime();
	td->td_act = act;
	td->td_ostate = ostate;
	td->td_tcb = (caddr_t)tp;
	if (tp)
		td->td_cb = *tp;
	else
		bzero((caddr_t)&td->td_cb, sizeof (*tp));
	if (ti)
		td->td_ti = *ti;
	else
		bzero((caddr_t)&td->td_ti, sizeof (*ti));
	td->td_req = req;

	if (g_tcpconsdebug == 0)
		return;
	if (tp)
		ERROR("%p %s:", tp, tcpstates[ostate]);
	else
		ERROR("???????? ");
	ERROR("%s ", tanames[act]);
	switch (act) {

	case TA_INPUT:
	case TA_OUTPUT:
	case TA_DROP:
		if (ti == 0)
			break;
		seq = ti->ti_seq;
		ack = ti->ti_ack;
		len = ti->ti_len;
		if (act == TA_OUTPUT) {
			seq = ntohl(seq);
			ack = ntohl(ack);
			len = ntohs((u_short)len);
		}
		if (act == TA_OUTPUT)
			len -= sizeof (struct tcphdr);
		if (len)
			ERROR("[%u..%u)", seq, seq+len);
		else
			ERROR("%u", seq);
		ERROR("@%x, urp=%u", ack, ti->ti_urp);
		flags = ti->ti_flags;
		if (flags) {
         ERROR("flags: %s%s%s%s%s%s",
             ti->ti_flags&TH_SYN ? "S": " ",
             ti->ti_flags&TH_ACK ? "A": " ",
             ti->ti_flags&TH_FIN ? "F": " ",
             ti->ti_flags&TH_RST ? "R": " ",
             ti->ti_flags&TH_PUSH ? "P": " ",
             ti->ti_flags&TH_URG ? "U": " "
             );
			//char *cp = "<";
         //#define pf(f) { if (ti->ti_flags&TH_ ## f) { printf("%s%s", cp, "#f"); cp = ","; } }
         //pf(SYN); pf(ACK); pf(FIN); pf(RST); pf(PUSH); pf(URG);
			//ERROR(">");
		}
		break;

	case TA_USER:
		ERROR("%s", prurequests[req&0xff]);
		if ((req & 0xff) == PRU_SLOWTIMO)
			ERROR("<%s>", tcptimers[req>>8]);
		break;
	}
	if (tp)
		ERROR(" -> %s", tcpstates[tp->t_state]);
	/* print out internal state of tp !?! */
	if (tp == 0)
		return;
	ERROR("\trcv_(nxt,wnd,up) (%u,%lu,%u) snd_(una,nxt,max) (%u,%u,%u)",
	    tp->rcv_nxt, tp->rcv_wnd, tp->rcv_up, tp->snd_una, tp->snd_nxt,
	    tp->snd_max);
	ERROR("\tsnd_(wl1,wl2,wnd) (%u,%u,%lu)",
	    tp->snd_wl1, tp->snd_wl2, tp->snd_wnd);

   so = tp->t_inpcb->inp_socket;
   ERROR("rcv_buff: %u",so->so_rcv.sb_mb ? usn_get_mbuflen(so->so_rcv.sb_mb) : 0);
   ERROR("snd_buff: %u",so->so_snd.sb_mb ? usn_get_mbuflen(so->so_snd.sb_mb) : 0);
}

void tcp_print(struct tcpiphdr *ti)
{
   ERROR("tcp info, seq=%u, ack=%u, win=%u, urg=%u", 
            ti->ti_seq,
            ti->ti_ack,
            ti->ti_win,
            ti->ti_urp);
}

