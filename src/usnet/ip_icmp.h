/*
 * Copyright (c) 2015 Jackie Dinh <jackiedinh8@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1 Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  2 Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution.
 *  3 Neither the name of the <organization> nor the 
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @(#)ip_icmp.h
 */

#ifndef _USNET_IP_ICMP_H_
#define _USNET_IP_ICMP_H_

#include "ip.h"

typedef struct usn_icmphdr usn_icmphdr_t;
struct usn_icmphdr {
	char	icmp_type;
	char	icmp_code;
	short	icmp_cksum;
	union {
		char ih_pptr;
		uint32_t ih_gwaddr;
		struct ih_idseq {
			short	icd_id;
			short	icd_seq;
		} ih_idseq;
		int ih_void;

		struct ih_pmtu {
			short ipm_void;    
			short ipm_nextmtu;
		} ih_pmtu;
	} icmp_hun;
	union {
		struct id_ts {
			uint32_t its_otime;
			uint32_t its_rtime;
			uint32_t its_ttime;
		} id_ts;
		struct id_ip  {
			usn_iphdr_t idi_ip;
			/* options and then 64 bits of data */
		} id_ip;
		uint32_t id_mask;
		char id_data[1];
	} icmp_dun;
};

#define  ICMP_MINLEN 8 

/* icmp types */
#define ICMP_ECHOREPLY 0 /* echo reply */
#define ICMP_ECHO      8 /* echo service */   
int
usnet_icmp_input(usn_context_t *ctx, usn_iphdr_t *iph, int len);

int 
usnet_icmp_output(usn_context_t *ctx, unsigned char* buf, int len);



#endif //!_USNET_IP_ICMP_H_
