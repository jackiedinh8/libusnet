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
 * @(#)core.h
 */

#ifndef _USNET_CORE_HEADER_H_
#define _USNET_CORE_HEADER_H_

#include <net/if.h>
#include <time.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/queue.h>
#include <netinet/in.h>

#define NETMAP_WITH_LIBS
#include <net/netmap.h>
#include <net/netmap_user.h>

#include "types.h"
#include "buf.h"
#include "mq.h"
#include "cache.h"
#include "tcb.h"

#ifdef USE_SLAB_MEM
#include "slab.h"
#endif

struct usn_nmring {
   struct netmap_ring *ring;
   int                 ring_idx;
   //int                 idx;
};

/* ARP Routing Table */
struct route_table
{
   uint32_t daddr;
   uint32_t mask;
   uint32_t masked;
   int prefix;
   int nif;
};

/* ARP Table */
struct usn_arp_entry
{
   uint32_t ip; 
   int8_t   prefix;
   uint32_t ip_mask;
   uint32_t ip_masked;
   unsigned char haddr[ETH_LEN];
};

struct usn_arp_table {
   usn_arp_entry_t *entries;
   int cnt;
};

struct usn_ifnet {
   char   iface[ETH_LEN];
   char   hwa[ETH_LEN];
   uint32_t addr;
   uint32_t bcast;
   uint32_t nmask;
   uint16_t mtu;
   // netmap-related params
   int      npkts;   
   int      burst;   
   int      tx_rate;

};

        
struct usn_context {
   struct nm_desc   *nmd;
   usn_log_t        *log;

   time_t            cur_time;
   uint32_t          g_fd;
   uint32_t          flow_cnt;

   int               tcp_timewait;
   int               tcp_timeout;

   usn_ifnet_t      *ifnet;

   // arp table
   usn_arp_table_t   arp;
   
   // message queues
   usn_shmmq_t  *api_net2app_mq;
   usn_shmmq_t  *api_app2net_mq;
   usn_shmmq_t  *ev_net2app_mq;
   usn_shmmq_t  *ev_app2net_mq;

#ifdef USE_NETMAP_BUF
   // trigger netmap device to send data;
   usn_nmring_t trigger_ring;
#else
   // buff
   usn_nmring_t trigger_ring;
   usn_buf_t    *nm_buf;
#endif //USE_NETMAP_BUF

   usn_hashbase_t *tcb_cache;
   usn_hashbase_t *socket_cache;
   usn_hashbase_t *ringbuf_cache;
   usn_hashbase_t *sndbuf_cache;
   usn_hashbase_t *epoll_cache;

   usn_mempool_t *rcvvars_mp;
   usn_mempool_t *sndvars_mp;
   usn_mempool_t *frag_mp;

#ifdef USE_SLAB_MEM   
   usn_slab_pool_t *slab_pool;
#endif

   TAILQ_HEAD (control_head, usn_tcb) control_list;
   TAILQ_HEAD (send_head, usn_tcb) send_list;
   TAILQ_HEAD (ack_head, usn_tcb) ack_list;
   int control_list_cnt;
   int send_list_cnt;
   int ack_list_cnt;

   struct rto_hashstore* rto_store;
   int rto_list_cnt;
               
   TAILQ_HEAD (timewait_head, usn_tcb) timewait_list; 
   TAILQ_HEAD (timeout_head, usn_tcb) timeout_list;
   int timewait_list_cnt;
   int timeout_list_cnt;

   usn_tcb_t        *listen_tcb;
};

usn_context_t*
usnet_setup(const char* ifname);

void 
usnet_net_setup(usn_context_t *ctx);

// Start handling loop
void
usnet_dispatch(usn_context_t *ctx);

// ip stack handling
typedef void (*read_handler_cb)(uint32_t fd, uint16_t flags, void* arg);
typedef void (*write_handler_cb)(uint32_t fd, uint16_t flags, void* arg);
typedef void (*accept_handler_cb)(uint32_t fd, uint32_t *addr, uint32_t len, void* arg);
typedef void (*event_handler_cb)(uint32_t fd, uint32_t event, void* arg);

// socket functionality
int
usnet_socket(usn_context_t *ctx, int dom, int type, int proto);

int
usnet_bind(usn_context_t *ctx, int fd, const struct sockaddr *addr, int len);

int
usnet_connect(usn_context_t *ctx, int fd, const struct sockaddr *addr, int len);

int
usnet_listen(usn_context_t *ctx, int32_t fd, int backlog);

int 
usnet_accept_newconn(usn_context_t *ctx, int fd, 
      uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport);

int
usnet_accept(usn_context_t *ctx, int fd, struct sockaddr *addr, int *addrlen);

int
usnet_close(usn_context_t *ctx, int fd);

ssize_t
usnet_read(usn_context_t *ctx, int fd, void *buf, size_t count);

ssize_t
usnet_write(usn_context_t *ctx, int fd, void *buf, size_t count);

#endif // !_USNET_CORE_HEADER_H_

