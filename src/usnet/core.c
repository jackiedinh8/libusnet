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
 * @(#)core.c
 */

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>

#include "core.h"
#include "log.h"
#include "eth.h"
#include "utils.h"
#include "arp.h"
#include "tcp.h"
#include "socket.h"
#include "ringbuf.h"
#include "sndbuf.h"
#include "tcp_out.h"
#include "event.h"
#include "timer.h"
#include "epoll.h"

static void
sigint_h(int sig)
{
	signal(SIGINT, SIG_DFL);
}

struct nm_desc*
usnet_netmap_init(usn_context_t *ctx, uint32_t flags)
{
   struct nmreq  nmr;

   signal(SIGINT, sigint_h);

   DEBUG(ctx->log, "set up netmap on %s", ctx->ifnet->iface);
   bzero(&nmr, sizeof(nmr));
   sprintf(nmr.nr_name,"%s:%s","netmap",ctx->ifnet->iface);

   //nmr.nr_flags = NR_REG_ALL_NIC;
   ctx->nmd = nm_open(nmr.nr_name, &nmr, 0, NULL);
   if ( ctx->nmd == NULL ) {
      ERROR(ctx->log, "can not open netmap device");
      exit(1);
   }

#ifdef USE_NETMAP_BUF

#else 
   ctx->nm_buf = (usn_buf_t*)malloc(sizeof(usn_buf_t));
   if (ctx->nm_buf == 0)
      exit(2);
   ctx->nm_buf->head = ctx->nm_buf->start = (char*)malloc(2048);
   if ( ctx->nm_buf->head == 0 )
      exit(3);
   ctx->nm_buf->len = 2048;
#endif //USE_NETMAP_BUF 

   // TODO: config them.
   ctx->ifnet->npkts = 0;
   ctx->ifnet->burst = 1024;
   ctx->ifnet->tx_rate = 0;

   return ctx->nmd;
}

int32_t
usnet_setup_process(usn_context_t *ctx)
{
   /*
   usnet_sockbuf_init();
   usnet_socketcache_init();
   */
   usnet_ringbuf_init(ctx, 8096, 0); // 8k for tcp receive buffer
   usnet_sendbuf_init(ctx, 8096, 0); // 8k for tcp send buffer
   usnet_tcpev_init(ctx);
   //usnet_apimq_init(ctx);

   return 0;
}

/* sysctl wrapper to return the number of active CPUs */
int
system_ncpus(void)
{
   int ncpus;
#if defined (__FreeBSD__)
   int mib[2] = { CTL_HW, HW_NCPU };
   size_t len = sizeof(mib);
   sysctl(mib, 2, &ncpus, &len, NULL, 0);
#elif defined(linux)
   ncpus = sysconf(_SC_NPROCESSORS_ONLN);
#else /* others */
   ncpus = 1;
#endif /* others */
   return (ncpus);
}

/* set the thread affinity. */
int
setaffinity(usn_context_t *ctx, int i)
{
/*
   cpuset_t cpumask;

   if (i == -1)
      return 0;

   // Set thread affinity affinity
   CPU_ZERO(&cpumask);
   CPU_SET(i, &cpumask);

   if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_CPUSET, 
          -1, sizeof(cpuset_t), &cpumask) != 0) { 
      DEBUG(ctx->log,"Unable to set affinity: %s", strerror(errno));
      return 1;
   }
*/
   return 0;
}


int 
usnet_recv_frame(usn_context_t *ctx, struct netmap_ring *ring, int limit, int dump)
{
   struct netmap_slot *slot;
   unsigned char      *p; 
   int                 cur, rx, n;
   //struct timeval total,start,end;
   //static struct timeval time;
   //static int cnt = 0;
   //if ( cnt == 0 )
   //   memset(&time,0,sizeof(time));

   cur = ring->cur;
   n = nm_ring_space(ring);
   if (n < limit)
      limit = n;  
   for (rx = 0; rx < limit; rx++) {
      slot = &ring->slot[cur];
      p = (unsigned char*)NETMAP_BUF(ring, slot->buf_idx);

      DEBUG(ctx->log, "frame received, len=%d", slot->len);

      //gettimeofday(&start,0); cnt++;
      usnet_eth_input(ctx, p, slot->len);
      //gettimeofday(&end,0); 
      //timersub(&end,&start,&total);
      //timeradd(&total,&time,&time);
      //if ( cnt == 50 ) {
      //   ERROR(ctx->log,"process time: %llu(s) %llu(ms),cnt=%u", 
      //                time.tv_sec, time.tv_usec,cnt);
      //   cnt = 0;
      //}
      cur = nm_ring_next(ring, cur);
   }   
   ring->head = ring->cur = cur; 

   return (rx);
}

void usnet_worker_setup(usn_context_t *ctx)
{
   ctx->log = usnet_log_init("./worker.log", 0, 0, 0);
   if ( ctx->log == NULL )
      printf("[worker]: cannot open log\n");


   DEBUG(ctx->log, "log init");

   sleep(2); //wait for network setup in deamon process.
   usnet_epoll_init(ctx);
   usnet_socket_init(ctx,0);
   usnet_ringbuf_init(ctx, 8096, 0); // 8k for tcp receive buffer
   usnet_sendbuf_init(ctx, 4*8096, 0); // 8k for tcp send buffer
   usnet_tcpev_init(ctx);

}

inline int
usnet_send_tcp_control_list(usn_context_t *ctx, int thresh)
{
   usn_tcb_t *cur_stream;
   usn_tcb_t *next, *last;
   int cnt = 0;
   int ret;

   thresh = MIN(thresh, ctx->control_list_cnt);

   /* Send TCP control messages */
   cnt = 0;
   cur_stream = TAILQ_FIRST(&ctx->control_list);
   last = TAILQ_LAST(&ctx->control_list, control_head);
   while (cur_stream) {
      if (++cnt > thresh)
         break;

      DEBUG(ctx->log,"Inside control loop. cnt: %u, stream: %d",
            cnt, cur_stream->fd);
      next = TAILQ_NEXT(cur_stream, sndvar->control_link);

      TAILQ_REMOVE(&ctx->control_list, cur_stream, sndvar->control_link);
      ctx->control_list_cnt--;

      if (cur_stream->sndvar->on_control_list) {
         cur_stream->sndvar->on_control_list = 0;
         DEBUG(ctx->log,"Stream %u: Sending control packet", cur_stream->fd);
         ret = usnet_send_control_packet(ctx, cur_stream);
         if (ret < 0) {
            TAILQ_INSERT_HEAD(&ctx->control_list,
                  cur_stream, sndvar->control_link);
            cur_stream->sndvar->on_control_list = 1;
            ctx->control_list_cnt++;
            /* since there is no available write buffer, break */
            break;
         }
      } else {
         ERROR(ctx->log,"Stream %d: not on control list.\n", cur_stream->fd);
      }

      if (cur_stream == last)
         break;
      cur_stream = next;
   }

   return cnt;
}

int
usnet_send_tcp_ack_list(usn_context_t *ctx, int thresh)
{
   usn_tcb_t *cur_tcb;
   usn_tcb_t *next, *last;
   int to_ack;
   int cnt = 0;
   int ret;

   /* Send aggregated acks */
   cnt = 0;
   cur_tcb = TAILQ_FIRST(&ctx->ack_list);
   last = TAILQ_LAST(&ctx->ack_list, ack_head);
   while (cur_tcb) {
      if (++cnt > thresh)
         break;

      DEBUG(ctx->log,"Inside ack loop. cnt: %u", cnt);
      next = TAILQ_NEXT(cur_tcb, sndvar->ack_link);

#ifdef DUMP_STREAM
      usnet_dump_tcb(ctx, cur_tcb);
#endif
      if (cur_tcb->sndvar->on_ack_list) {
         /* this list is only to ack the data packets */
         /* if the ack is not data ack, then it will not process here */
         to_ack = 0;
         if (cur_tcb->state == TCPS_ESTABLISHED ||  
               cur_tcb->state == TCPS_CLOSE_WAIT ||  
               cur_tcb->state == TCPS_FIN_WAIT_1 ||  
               cur_tcb->state == TCPS_FIN_WAIT_2 ||  
               cur_tcb->state == TCPS_TIME_WAIT) {
            /* TIMEWAIT is possible since the ack is queued 
               at FIN_WAIT_2 */
            if (cur_tcb->rcvvar->rcvbuf) {
               //if (TCP_SEQ_LEQ(cur_tcb->rcv_nxt, 
               //         cur_tcb->rcvvar->rcvbuf->head_seq + 
               //         cur_tcb->rcvvar->rcvbuf->merged_len)) {
               if (TCP_SEQ_LEQ(cur_tcb->rcv_nxt, 
                        cur_tcb->rcvvar->rcvbuf->head_seq)) {
                  //XXX: why not TCP_SEQ_LT
                  DEBUG(ctx->log,"to_ack, fd=%u, ack=%u, head_seq=%u, merged_len=%u",
                     cur_tcb->fd, cur_tcb->rcv_nxt, cur_tcb->rcvvar->rcvbuf->head_seq,
                     cur_tcb->rcvvar->rcvbuf->merged_len);
                  to_ack = 1;
               }
            }
         } else {
            DEBUG(ctx->log, "not proper state for sending ack, "
                  "fd=%u, state=%s, seq=%u, ack_seq=%u, on_control_list=%u",
                  cur_tcb->fd, usnet_tcp_statestr(cur_tcb),
                  cur_tcb->snd_nxt, cur_tcb->rcv_nxt,
                  cur_tcb->sndvar->on_control_list);
         }

         if (to_ack) {
            /* send the queued ack packets */
            while (cur_tcb->sndvar->ack_cnt > 0) {
               ret = usnet_send_tcp_packet(ctx, cur_tcb,
                     TH_ACK, NULL, 0);
               if (ret < 0) {
                  /* since there is no available write buffer, break */
                  break;
               }
               cur_tcb->sndvar->ack_cnt--;
            }

            /* if is_wack is set, send packet to get window advertisement */
            if (cur_tcb->sndvar->is_wack) {
               cur_tcb->sndvar->is_wack = 0;
               ret = usnet_send_tcp_packet(ctx, cur_tcb,
                     TH_ACK | TH_WACK, NULL, 0);
               if (ret < 0) {
                  /* since there is no available write buffer, break */
                  cur_tcb->sndvar->is_wack = 1;
               }
            }
            if (!(cur_tcb->sndvar->ack_cnt || cur_tcb->sndvar->is_wack)) {
               cur_tcb->sndvar->on_ack_list = 0;
               TAILQ_REMOVE(&ctx->ack_list, cur_tcb, sndvar->ack_link);
               ctx->ack_list_cnt--;
            }
         } else {
            cur_tcb->sndvar->on_ack_list = 0;
            cur_tcb->sndvar->ack_cnt = 0;
            cur_tcb->sndvar->is_wack = 0;
            TAILQ_REMOVE(&ctx->ack_list, cur_tcb, sndvar->ack_link);
            ctx->ack_list_cnt--;
         }

         if (cur_tcb->control_list_waiting) {
            if (!cur_tcb->sndvar->on_send_list) {
               cur_tcb->control_list_waiting = 0;
               usnet_add_control_list(ctx, cur_tcb);
            }
         }
      } else {
         ERROR(ctx->log,"not on ack list, fd=%u", cur_tcb->fd);
         TAILQ_REMOVE(&ctx->ack_list, cur_tcb, sndvar->ack_link);
         ctx->ack_list_cnt--;
#ifdef DUMP_STREAM
         DEBUG(ctx->log, "not on ack list, fd=%u", cur_tcb->fd);
         usnet_dump_tcb(ctx, cur_tcb);
#endif
      }

      if (cur_tcb == last)
         break;
      cur_tcb = next;
   }

   return cnt;
}

int
usnet_send_tcp_data_list(usn_context_t *ctx, int thresh)
{
   usn_tcb_t *cur_tcb;
   usn_tcb_t *next, *last;
   int cnt = 0;
   int ret;

   /* Send data */
   cnt = 0;
   cur_tcb = TAILQ_FIRST(&ctx->send_list);
   last = TAILQ_LAST(&ctx->send_list, send_head);
   while (cur_tcb) {
      if (++cnt > thresh)
         break;

      DEBUG(ctx->log,"Inside send loop. cnt: %u, stream: %d\n",
            cnt, cur_tcb->fd);
      next = TAILQ_NEXT(cur_tcb, sndvar->send_link);

      TAILQ_REMOVE(&ctx->send_list, cur_tcb, sndvar->send_link);
      if (cur_tcb->sndvar->on_send_list) {
         ret = 0;

         /* Send data here */
         /* Only can send data when ESTABLISHED or CLOSE_WAIT */
         if (cur_tcb->state == TCPS_ESTABLISHED) {
            if (cur_tcb->sndvar->on_control_list) {
               /* delay sending data after until on_control_list becomes off */
               ERROR(ctx->log,"delay sending data, fd=%u", cur_tcb->fd);
               ret = -1;
            } else {
               ret = usnet_flush_tcp_sendbuf(ctx, cur_tcb);
            }
         } else if (cur_tcb->state == TCPS_CLOSE_WAIT ||
               cur_tcb->state == TCPS_FIN_WAIT_1 ||
               cur_tcb->state == TCPS_LAST_ACK) {
            ret = usnet_flush_tcp_sendbuf(ctx, cur_tcb);
         } else {
            DEBUG(ctx->log,"Stream %d: on_send_list at state %s\n",
                  cur_tcb->fd, usnet_tcp_statestr(cur_tcb));
#if DUMP_STREAM
            usnet_dump_tcb(ctx, cur_tcb);
#endif
         }

         if (ret < 0) {
            DEBUG(ctx->log,"Reinsert into send list, fd=%u, ret=%d",cur_tcb->fd,ret);
            TAILQ_INSERT_TAIL(&ctx->send_list, cur_tcb, sndvar->send_link);
            /* since there is no available write buffer, break */
            break;

         } else {
            DEBUG(ctx->log,"Remove from send list, fd=%u, ret=%d",cur_tcb->fd,ret);
            cur_tcb->sndvar->on_send_list = 0;
            ctx->send_list_cnt--;
            /* the ret value is the number of packets sent. */
            /* decrease ack_cnt for the piggybacked acks */
#if ACK_PIGGYBACK
            if (cur_tcb->sndvar->ack_cnt > 0) {
               if (cur_tcb->sndvar->ack_cnt > ret) {
                  cur_tcb->sndvar->ack_cnt -= ret;
               } else {
                  cur_tcb->sndvar->ack_cnt = 0;
               }
            }
#endif
#if 1
            if (cur_tcb->control_list_waiting) {
               if (!cur_tcb->sndvar->on_ack_list) {
                  cur_tcb->control_list_waiting = 0;
                  usnet_add_control_list(ctx, cur_tcb);
               }
            }
#endif
         }
      } else {
         ERROR(ctx->log,"not on send list, fd=%u", cur_tcb->fd);
#ifdef DUMP_STREAM
         usnet_dump_tcb(ctx, cur_tcb);
#endif
      }

      if (cur_tcb == last)
         break;
      cur_tcb = next;
   }

   return cnt;
}

void
usnet_dispatch_net(usn_context_t *ctx)
{
   struct netmap_if   *nifp;
   struct pollfd       fds[2];
   struct netmap_ring *rxring;
   int                 ret;

   nifp = ctx->nmd->nifp;
   fds[0].fd = ctx->nmd->fd;
   fds[1].fd = ctx->ev_app2net_mq->_fd;

   while(1) {
       ctx->cur_time = time(0);
       //fds.events = POLLIN | POLLOUT;
       fds[0].events = POLLIN;
       fds[0].revents = 0;
       fds[1].events = POLLIN;
       fds[1].revents = 0;

       ret = poll(fds, 2, 1);
       if (ret <= 0 ) {
          usnet_check_rtm_timeout(ctx,512);
          usnet_check_timewait_expire(ctx,512);
          usnet_check_connection_timeout(ctx,512);
          continue;
       }

       // incoming packets from netmap
       if (fds[0].revents & POLLERR) {
          struct netmap_ring *rx = NETMAP_RXRING(nifp, ctx->nmd->cur_rx_ring); 
			 ERROR(ctx->log, "error on em1, rx [%d,%d,%d]", rx->head, rx->cur, rx->tail);
       }

       if (fds[0].revents & POLLIN) {
          int j;
          for (j = ctx->nmd->first_rx_ring; 
                   j <= ctx->nmd->last_rx_ring; j++) {
             rxring = NETMAP_RXRING(nifp, j);

				 //DEBUG("Ring info rx(%d), head=%d, cur=%d, tail=%d", 
             //      j, rxring->head, rxring->cur, rxring->tail); 
             if (nm_ring_empty(rxring)) {
                continue;
             }
             usnet_recv_frame(ctx, rxring, 512, 1);
          }
       }

       usnet_check_rtm_timeout(ctx,512);
       usnet_check_timewait_expire(ctx,512);
       usnet_check_connection_timeout(ctx,512);

       usnet_send_tcp_control_list(ctx, 512);
       usnet_send_tcp_ack_list(ctx, 512);

       if (fds[1].revents & POLLIN) {
          usnet_tcpev_net(ctx);
       }

       usnet_send_tcp_data_list(ctx, 512);
   }

   nm_close(ctx->nmd);
   return;
}

void 
usnet_net_setup(usn_context_t *ctx)
{
   unsigned char *ptr;

   ctx->log = usnet_log_init("./libusnet.log",0,0,0);
   if ( ctx->log == 0 )
      exit(0);


   setaffinity(ctx, 0);

   ERROR(ctx->log,"interface info, ifname:%s,addr=%x,bcast=%x,nmask=%x",
         ctx->ifnet->iface,ctx->ifnet->addr,ctx->ifnet->bcast,ctx->ifnet->nmask);


   ptr = (unsigned char*)ctx->ifnet->hwa;
   ERROR(ctx->log,"hwaddr: %02x:%02x:%02x:%02x:%02x:%02x\n", 
                  *ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5));
   // initialize usnet
   usnet_netmap_init(ctx, 0);
#ifdef USE_SLAB_MEM
   usnet_slab_init(ctx);
#endif
   usnet_rtohash_init(ctx);
   usnet_arp_init(ctx);
   usnet_socket_init(ctx,1);
   usnet_tcp_init(ctx);
   usnet_ringbuf_init(ctx, 8096, 1); // 8k for tcp receive buffer
   usnet_sendbuf_init(ctx, 4*8096, 1); // 16k for tcp send buffer
   usnet_tcpev_init(ctx);

   usnet_dispatch_net(ctx);
}

usn_context_t*
usnet_create_context()
{   
   usn_context_t *ctx;

   ctx = (usn_context_t*)malloc(sizeof(usn_context_t));
   if ( ctx == NULL )
      return 0;

   memset(ctx, 0, sizeof(ctx));


   ctx->g_fd = 1;
   ctx->timewait_list_cnt = 0;

   TAILQ_INIT(&ctx->timewait_list);
   TAILQ_INIT(&ctx->timeout_list);

   TAILQ_INIT(&ctx->control_list);
   TAILQ_INIT(&ctx->send_list);
   TAILQ_INIT(&ctx->ack_list);
   
   ctx->tcp_timewait = 60;
   ctx->tcp_timeout = 120;

   ctx->g_fd = 1;

   return ctx; 
}

usn_context_t*
usnet_setup(const char* ifname)
{
   int pid;
   usn_context_t* ctx;
  
   ctx = usnet_create_context();
   if ( ctx == 0 )
      return 0;

   usnet_get_network_info(ctx,ifname,strlen(ifname));

   pid = fork();
   if ( pid < 0 ) {
      exit(-1);
   }
   else if ( pid != 0 ) // parent
      exit(-2);

   pid = fork();
   if ( pid < 0 )
      printf("error");
   else if ( pid == 0 ) { // child
      usnet_worker_setup(ctx);
   } else {
      usnet_net_setup(ctx);
   }

   return ctx;
}


void
usnet_dispatch(usn_context_t *ctx)
{

   struct pollfd       fds[1];
   int                 ret;

   fds[0].fd = ctx->ev_net2app_mq->_fd;

   DEBUG(ctx->log,"dispatch worker ...");
   while(1) {

       fds[0].events = POLLIN;
       fds[0].revents = 0;
       ret = poll(fds, 1, 5000);
       if (ret <= 0 ) {
          DEBUG(ctx->log, "poll %s ev %x %x", ret <= 0 ? "timeout" : "ok",
            fds[0].events, fds[0].revents);
          continue;
       }
       // tcpev jobs from message queue
       if (fds[0].revents & POLLERR) {
			 ERROR(ctx->log,"error on tcpev message queue");
       }
       if (fds[0].revents & POLLIN) {
          usnet_tcpev_process(ctx);
       }
   }

   return;
}

int
usnet_socket(usn_context_t *ctx, int dom, int type, int proto)
{
   usn_socket_t *so;
   usn_socket_t s;

   s.so_fd = ctx->g_fd++;

   so = CACHE_GET(ctx->socket_cache, &s, usn_socket_t*);
   
   if ( so == 0 )
      return -1;

   memset(so, 0, sizeof(usn_socket_t));
   so->so_fd = s.so_fd;
   so->so_dom = dom;
   so->so_type = type;
   so->so_proto = proto;
  
   return so->so_fd;
}

int
usnet_bind(usn_context_t *ctx, int fd, const struct sockaddr *addr, int len)
{
   struct sockaddr_in *addrin = 0;
   usn_socket_t *so;
   usn_socket_t s;

   s.so_fd = fd;
   so = CACHE_SEARCH(ctx->socket_cache,&s,usn_socket_t*);

   if ( so == 0 )
      return -1;

   if ( len != sizeof(struct sockaddr_in) ) {
      return -2;
   }

   addrin = (struct sockaddr_in*)addr;

   if ( so->so_dom != addrin->sin_family ) {
      ERROR(ctx->log,"wrong domain, fd=%u, so_dom=%d, sin_family=%d", 
            fd, so->so_dom, addrin->sin_family);
      return -3;
   }

   so->so_sport = addrin->sin_port;
   so->so_saddr = addrin->sin_addr.s_addr;
   so->so_options |= SO_BIND;

   return 0;
}

int
usnet_connect(usn_context_t *ctx, int fd, const struct sockaddr *addr, int len)
{
   return 0;
}

int
usnet_listen(usn_context_t *ctx, int32_t fd, int backlog) 
{
   usn_socket_t *so;
   usn_socket_t s;
   int ret = 0;

   s.so_fd = fd;
   so = CACHE_SEARCH(ctx->socket_cache,&s,usn_socket_t*);

   if ( so == 0 )
      return -1;

   DEBUG(ctx->log,"found socket, fd=%u",fd);

   // setup backlog queue;
   so->so_backlogq = (struct backlog_queue*)malloc(sizeof(struct backlog_queue));
   if ( so->so_backlogq == 0 )
      return -2;

   memset(so->so_backlogq, 0, sizeof(so->so_backlogq));
   so->so_backlogq->bq_size = SO_DEFAULT_BACKLOG;
   so->so_backlogq->bq_connq = 
      (uint32_t*)calloc(so->so_backlogq->bq_size, sizeof(uint32_t));

   if (so->so_backlogq->bq_connq == 0 ) {
      free(so->so_backlogq);
      return -3;
   }

   so->so_options |= SO_ACCEPT;

   ret = usnet_send_listen_event(ctx,fd, so->so_saddr, so->so_sport);
   if ( ret < 0 ) {
      ERROR(ctx->log,"failed to register listening socket, fd=%u, ret=%d", fd, ret);
      free(so->so_backlogq->bq_connq);
      free(so->so_backlogq);
      return -4;
   }

   return ret;
}

int 
usnet_accept_newconn(usn_context_t *ctx, int fd, 
      uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport)
{
   usn_socket_t *accept_so;
   usn_socket_t *so;
   usn_socket_t s;
   struct backlog_queue *queue;
   //int capacity = 0;

   s.so_fd = fd;
   accept_so = CACHE_SEARCH(ctx->socket_cache,&s,usn_socket_t*);

   if ( accept_so == 0 )
      return -1;

   if ( !(accept_so->so_options & SO_ACCEPT) ) {
      ERROR(ctx->log,"non-listening socket, fd=%u, so_options=%x", 
            fd, accept_so->so_options);
      return -2;
   }

   //so = usnet_socket(ctx,accept_so->so_dom,accept_so->so_type,accept_so->so_proto);
   s.so_fd = ctx->g_fd++;
   so = CACHE_GET(ctx->socket_cache, &s, usn_socket_t*);
   if ( so == 0 )
      return -3;

   memset(so, 0, sizeof(usn_socket_t));
   so->so_fd = s.so_fd;
   so->so_saddr = saddr;
   so->so_sport = sport;
   so->so_daddr = daddr;
   so->so_dport = dport;
   so->so_dom = accept_so->so_dom;
   so->so_type = accept_so->so_type;
   so->so_proto = accept_so->so_proto;
   so->so_state |= SO_ISCONNECTING; 

   queue = accept_so->so_backlogq;
   queue->bq_connq[queue->bq_tail++] = so->so_fd;
   if ( queue->bq_tail > queue->bq_size )
      queue->bq_tail -= queue->bq_size;

   DEBUG(ctx->log,"accept connection: fd=%u,saddr=%x,sport=%u,daddr=%x,dport=%u",
         so->so_fd,saddr,sport,daddr,dport);
   return so->so_fd;
}

int
usnet_accept(usn_context_t *ctx, int fd, struct sockaddr *addr, int *addrlen)
{
   struct usn_connect_ev ev;
   usn_socket_t *so;
   usn_socket_t s;
   struct backlog_queue *queue;
   int con_fd;

   s.so_fd = fd;
   so = CACHE_SEARCH(ctx->socket_cache,&s,usn_socket_t*);

   if ( so == 0 )
      return -1;

   if ( !(so->so_options & SO_ACCEPT) ) {
      ERROR(ctx->log,"non-listening socket, fd=%u", fd);
      return -2;
   }

   queue = so->so_backlogq; 

   if ( queue->bq_head == queue->bq_tail )
      return 0;

   con_fd = queue->bq_connq[queue->bq_head++];
   if ( queue->bq_head >= queue->bq_size )
      queue->bq_head -= queue->bq_size;


   s.so_fd = con_fd;
   so = CACHE_SEARCH(ctx->socket_cache,&s,usn_socket_t*);
   if ( so == 0 )
      return -3;
   DEBUG(ctx->log,"send conncetion event to net, fd=%u,saddr=%x,sport=%u,daddr=%x,dport=%u",
         con_fd,so->so_saddr,so->so_sport,so->so_daddr,so->so_dport);

   memset(&ev,0,sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_CONNECT_EV;
   ev.fd = con_fd;
   ev.saddr = so->so_saddr;
   ev.sport = so->so_sport;
   ev.daddr = so->so_daddr;
   ev.dport = so->so_dport;
   ev.result = 0;
#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_app2net_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_app2net_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif

   return con_fd;
}

ssize_t
usnet_read(usn_context_t *ctx, int fd, void *buffer, size_t count)
{
   //ret = usnet_read_socket(fd, buf, len);
   usn_ringbuf_t s;
   usn_ringbuf_t *buf;
   char *data_ptr = 0;
   int data_len;


   s.fd = fd;
   buf = CACHE_SEARCH(ctx->ringbuf_cache, &s,usn_ringbuf_t*);

   if ( buf == 0 )
      return -1;

   DEBUG(ctx->log,"found recevied buffer, fd=%u, data=%d, head=%d", 
         fd, buf->data, buf->head);

   data_ptr = (char*)(buf+1); 
   if ( buf->head == buf->data ) {
      return -2;

   } else if ( buf->head > buf->data ) {
      data_len = buf->head-buf->data;
      DEBUG(ctx->log,"data_len=%d, count=%d",data_len,count);

      if ( data_len > count )
         data_len = count;

      //dump_buffer(ctx,data_ptr+buf->data, data_len,"tcp");
      memcpy(buffer,data_ptr+buf->data, data_len);
      buf->data += data_len;

   } else {
      int tail_len = buf->size - buf->data;
      data_len = buf->size + buf->head - buf->data;

      if ( data_len > count )
         data_len = count;

      if ( data_len <= tail_len ) {
         memcpy(buffer, data_ptr+buf->data, tail_len);
         buf->data += data_len;
      } else {
         memcpy(buffer, data_ptr+buf->data, tail_len);
         memcpy(buffer+tail_len, data_ptr, data_len - tail_len);
         buf->data = data_len - tail_len;
      }
   }

   return data_len;
}

ssize_t
usnet_write(usn_context_t *ctx, int fd, void *buf, size_t len)
{
   usn_sendbuf_t s;
   usn_sendbuf_t *sndbuf;
   ssize_t ret = 0;
   
   DEBUG(ctx->log,"write data, len=%d, fd=%u",len,fd);

   s.fd = fd;
   sndbuf = CACHE_SEARCH(ctx->sndbuf_cache, &s, usn_sendbuf_t*);

   if (sndbuf==0) {
      ERROR(ctx->log,"not found send buffer, fd=%u",fd);
      return -1;
   }

   DEBUG(ctx->log, "found send buffer, fd=%d",fd);
   ret = usnet_write_data(ctx, sndbuf, buf, len);

   if ( ret > 0 )
      usnet_write_notify(ctx,fd);
   
   return ret;
}

// buffer functionality
int
usnet_close(usn_context_t *ctx, int fd) 
{
   //return usnet_soclose(fd);
   return 0;
}

