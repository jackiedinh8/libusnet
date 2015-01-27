#include "usnet_tcp.h"
#include "usnet_tcpip.h"
#include "usnet_tcp_timer.h"
#include "usnet_tcp_var.h"
#include "usnet_tcp_seq.h"

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

/*
 * Tcp initialization
 */
void
usnet_tcp_init()
{
	DEBUG("tcp_init");

	g_tcp_iss = random();	/* wrong, but better than a constant */
	g_tcb.inp_next = g_tcb.inp_prev = &g_tcb;

	if (g_max_protohdr < sizeof(struct tcpiphdr))
		g_max_protohdr = sizeof(struct tcpiphdr);
   // FIXME: it's always false for ethernet frame
	if (g_max_linkhdr + sizeof(struct tcpiphdr) > BUF_MSIZE)
		DEBUG("panic: tcp_init"); // panic

   // from tcp_slowtimo
	g_tcp_maxidle = g_tcp_keepcnt * g_tcp_keepintvl;

   g_max_linkhdr = 16;
   g_max_iphdr = 60;
   g_max_tcphdr = 60;
   //g_max_protohdr = 0;
   //g_max_hdr = 0;
   //g_max_datalen = 0;         /* MHLEN - max_hdr */

   g_tcp_now = 0;

}


