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
 * @(#)in.c 8.3 (Berkeley) 1/9/95
 */


#include <netinet/in.h>
#include <sys/socket.h>

#include "usnet_in.h"
#include "usnet_if.h"

/*
 * Return the network number from an internet address.
 */
u_long
in_netof( struct usn_in_addr in)
{
	u_int32 i = ntohl(in.s_addr);
	u_int32 net;
	struct in_ifaddr *ia;

	if (USN_IN_CLASSA(i))
		net = i & USN_IN_CLASSA_NET;
	else if (USN_IN_CLASSB(i))
		net = i & USN_IN_CLASSB_NET;
	else if (USN_IN_CLASSC(i))
		net = i & USN_IN_CLASSC_NET;
	else if (USN_IN_CLASSD(i))
		net = i & USN_IN_CLASSD_NET;
	else
		return (0);

	/*
	 * Check whether network is a subnet;
	 * if so, return subnet number.
	 */
	for (ia = g_in_ifaddr; ia; ia = ia->ia_next)
		if (net == ia->ia_net)
			return (i & ia->ia_subnetmask);
	return (net);
}

#ifndef SUBNETSARELOCAL
#define	SUBNETSARELOCAL	1
#endif
int subnetsarelocal = SUBNETSARELOCAL;
/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net.
 * Otherwise, it includes only the directly-connected (sub)nets.
 */
int
in_localaddr( struct usn_in_addr in)
{
	u_long i = ntohl(in.s_addr);
	struct in_ifaddr *ia;

	if (subnetsarelocal) {
		for (ia = g_in_ifaddr; ia; ia = ia->ia_next)
			if ((i & ia->ia_netmask) == ia->ia_net)
				return (1);
	} else {
		for (ia = g_in_ifaddr; ia; ia = ia->ia_next)
			if ((i & ia->ia_subnetmask) == ia->ia_subnet)
				return (1);
	}
	return (0);
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int 
in_canforward(struct usn_in_addr in)
{
	u_int32 i = ntohl(in.s_addr);
	u_int32 net;

	if (USN_IN_EXPERIMENTAL(i) || USN_IN_MULTICAST(i))
		return (0);
	if (USN_IN_CLASSA(i)) {
		net = i & USN_IN_CLASSA_NET;
		if (net == 0 || net == (USN_IN_LOOPBACKNET << USN_IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * Return 1 if the address might be a local broadcast address.
 */
int 
in_broadcast(struct usn_in_addr in, struct ifnet *ifp)
{
   struct ifaddr *ifa;
   u_long t;

   if (in.s_addr == USN_INADDR_BROADCAST ||
       in.s_addr == USN_INADDR_ANY)
      return 1;
   if ((ifp->if_flags & USN_IFF_BROADCAST) == 0)
      return 0;
   t = ntohl(in.s_addr);
   /*  
    * Look through the list of addresses for a match
    * with a broadcast address.
    */
#define ia ((struct in_ifaddr *)ifa)
   for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
      if (ifa->ifa_addr->sa_family == AF_INET &&
          (in.s_addr == ia->ia_broadaddr.sin_addr.s_addr ||
           in.s_addr == ia->ia_netbroadcast.s_addr ||
           /*  
            * Check for old-style (host 0) broadcast.
            */
           t == ia->ia_subnet || t == ia->ia_net))
             return 1;
   return (0);
#undef ia
}

