/*
 * Copyright (c) 2014 Jackie Dinh <jackiedinh8@gmail.com>
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
 * @(#)usnet_common.h
 */


#ifndef _USNET_COMMON_H_
#define _USNET_COMMON_H_

/*
 * Address families.
 */

#define	USN_AF_UNSPEC	0		/* unspecified */
#define	USN_AF_LOCAL	1		/* local to host (pipes, portals) */
#define	USN_AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	USN_AF_IMPLINK	3		/* arpanet imp addresses */
#define	USN_AF_PUP		4		/* pup protocols: e.g. BSP */
#define	USN_AF_CHAOS	5		/* mit CHAOS protocols */
#define	USN_AF_NS		6		/* XEROX NS protocols */
#define	USN_AF_ISO		7		/* ISO protocols */
#define	USN_AF_OSI		USN_AF_ISO
#define	USN_AF_ECMA		8		/* european computer manufacturers */
#define	USN_AF_DATAKIT	9		/* datakit protocols */
#define	USN_AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define	USN_AF_SNA		11		/* IBM SNA */
#define  USN_AF_DECnet	12		/* DECnet */
#define  USN_AF_DLI		13		/* DEC Direct data link interface */
#define  USN_AF_LAT		14		/* LAT */
#define	USN_AF_HYLINK	15		/* NSC Hyperchannel */
#define	USN_AF_APPLETALK	16		/* Apple Talk */
#define	USN_AF_ROUTE	17		/* Internal Routing Protocol */
#define	USN_AF_LINK		18		/* Link layer interface */
#define	USN_pseudo_AF_XTP	19		/* eXpress Transfer Protocol (no AF) */
#define	USN_AF_COIP		20		/* connection-oriented IP, aka ST II */
#define	USN_AF_CNT		21		/* Computer Network Technology */
#define  USN_pseudo_AF_RTIP	22		/* Help Identify RTIP packets */
#define	USN_AF_IPX		23		/* Novell Internet Protocol */
#define	USN_AF_SIP		24		/* Simple Internet Protocol */
#define  USN_pseudo_AF_PIP	25		/* Help Identify PIP packets */
#define	USN_AF_MAX		26

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

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) < (y) ? (y) : (x))


#endif //_USNET_COMMON_H_
