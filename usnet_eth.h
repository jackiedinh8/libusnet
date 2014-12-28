#ifndef _USNET_FRAME_HEADER_H_
#define _USNET_FRAME_HEADER_H_

//#include <iostream>     // std::cout
//#include <utility>      // std::pair
//#include <functional>   // std::equal_to
//#include <algorithm>    // std::mismatch

#include "usnet_buf.h"
#include "usnet_buf_utils.h"
#include "usnet_types.h"
#include "usnet_socket.h"
#include "usnet_log.h"

extern u_char g_ether_addr[6];
extern u_char g_ip_addr[4];
extern struct usn_in_addr g_in_addr;
extern u_char g_eth_bcast_addr[6];
extern u_char g_etherbroadcastaddr[6];

char *
ether_sprintf(u_char *ap);

void
eth_input(u_char *m, int len);

int
eth_output(usn_mbuf_t *m, struct usn_sockaddr *dst, struct rtentry *rt0);

void 
handle_frame(u_char *m, int len, struct netmap_ring *ring, int cur);

int send_mbuf(usn_mbuf_t *m);

#endif // !_USNET_FRAME_HEADER_H_

