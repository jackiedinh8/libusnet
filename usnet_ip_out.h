#ifndef _USNET_IP_OUT_H_
#define _USNET_IP_OUT_H_

#include "usnet_buf.h"
#include "usnet_route.h"

/*
 * IP output.  The packet in mbuf chain m contains a skeletal IP
 * header (with len, off, ttl, proto, tos, src, dst).
 * The mbuf chain containing the packet will be freed.
 * The mbuf opt, if present, will not be freed.
 */
int32
ipv4_output(
   usn_mbuf_t *m0, 
   usn_mbuf_t *opt,
   struct route *ro, 
   int flags);


#endif //!_USNET_IP_OUT_H_
