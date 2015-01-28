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
 * @(#)usnet_core.c
 */


#include <ctype.h>      /* isprint() */
#include <errno.h>
#include <stdio.h>
#include <string.h>  /* strcmp */
#include <sys/types.h>
#include <sys/sysctl.h>

#include "usnet_core.h"
#include "usnet_common.h"
#include "usnet_log.h"
#include "usnet_arp.h"
#include "usnet_eth.h"
#include "usnet_if.h"
#include "usnet_ip_var.h"
#include "usnet_udp.h"
#include "usnet_tcp.h"
#include "usnet_slab.h"
#include "usnet_tcp_var.h"
#include "usnet_tcp_timer.h"

struct nm_desc *g_nmd;
#ifdef TIME_NANO
struct timespec g_time;
#else
struct timeval g_time;
#endif

char* g_interface;
char* g_address;
char* g_netmask;
char* g_gateway;
char* g_macaddress;
struct usn_glob_conf g_config;

struct ipq g_ipq;

static int do_abort = 0;

static void
sigint_h(int sig)
{
	(void)sig;  /* UNUSED */
	do_abort = 1;
	signal(SIGINT, SIG_DFL);
}

struct nm_desc*
usnet_init( struct nm_desc *gg_nmd, const char *dev_name, u_int flags)
{
   struct nmreq        nmr;
   struct nm_desc     *nmd = NULL;
   struct netmap_if   *nifp = NULL;
   struct netmap_ring *txr, *rxr;

   signal(SIGINT, sigint_h);

   bzero(&nmr, sizeof(nmr));
   strcpy(nmr.nr_name, dev_name);

   // XXX: which netmap flags?
   //nmr.nr_flags = NR_REG_ALL_NIC; //| flags;

   printf("nm_open: %s\n", nmr.nr_name);
   nmd = nm_open(nmr.nr_name, &nmr, 0, NULL);
   if ( nmd == NULL ) {
      DEBUG("Cannot open interface %s", nmr.nr_name);
      exit(1);
   }
   nifp = nmd->nifp;
   txr = NETMAP_TXRING(nifp, 0);
   rxr = NETMAP_RXRING(nifp, 0);

   printf("nmreq info, name=%s, version=%d,"
     " flags=%d, memsize=%d,"
     " ni_tx_rings=%d, ni_rx_rings=%d, num_tx_slots=%d, num_rx_slot=%d \n",
      nifp->ni_name,
      nifp->ni_version,
      nifp->ni_flags,
      nmd->memsize,
      nifp->ni_tx_rings,
      nifp->ni_rx_rings,
      txr->num_slots,
      rxr->num_slots);

   memset(&g_config, 0, sizeof(g_config));
   g_config.burst = 1000;
   g_config.tx_rate = 0;

   memset(&g_ipq, 0, sizeof(g_ipq));

   usnet_init_internal();
   usnet_route_init();
   usnet_network_init();
   usnet_udp_init();
   usnet_tcp_init();
   usnet_ipv4_init();
   usnet_socket_init();

   return nmd;
}

int
usnet_init_internal()
{
   g_arp_cache = (arp_cache_t*)malloc(sizeof(arp_cache_t));
   memset(g_arp_cache, 0, sizeof(arp_cache_t));

   g_arpt_keep = 20*60;
#ifdef TIME_NANO
   clock_gettime(CLOCK_REALTIME, &g_time);
#else
   gettimeofday(&g_time,0);
#endif

   //initialize slab_pool
   g_shm.addr = NULL;
   g_shm.key  = 0x12345;
   g_shm.size = 4024*1000;
   usn_shm_alloc(&g_shm);

   g_slab_pool = (usn_slab_pool_t*)g_shm.addr;
   usn_slab_init(g_slab_pool,&g_shm);

   return 0;
}


// ip stack handling
int 
usnet_recv(int fd, u_char* buff, u_int len)
{
   return 0;
}

int 
usnet_send(int fd, u_char* buff, u_int len)
{
   return 0;
}

u_int32
usnet_get_length(u_int fd)
{
   return 0;
}

int 
usnet_drain(int fd, size_t len)
{
   return 0;
}

void
dump_buffer(char *p, int len, const char *prefix)
{
   char buf[128];
   int i, j, i0;

   if ( p == NULL ) {
      DEBUG("null pointer");
      return;
   }
   /* get the length in ASCII of the length of the packet. */

   /* hexdump routine */
   for (i = 0; i < len; ) {
      memset(buf, sizeof(buf), ' ');
      sprintf(buf, "%5d: ", i);
      i0 = i;
      for (j=0; j < 16 && i < len; i++, j++)
         sprintf(buf+7+j*3, "%02x ", (uint8_t)(p[i]));
      i = i0;
      for (j=0; j < 16 && i < len; i++, j++)
         sprintf(buf+7+j + 48, "%c",
            isprint(p[i]) ? p[i] : '.');
      printf("%s: %s\n", prefix, buf);
   }
}

void show_help()
{
    DEBUG("desc: stack -i interface -a address -n netmask -g gateway -m macaddress");
}
int
usnet_get_options(int argc, char* const *argv)
{
    char     *p;  
    int         i;   

    for (i = 1; i < argc; i++) {

        p = argv[i];
        if (*p++ != '-') {
            DEBUG("invalid option: \"%s\"", argv[i]);
            return -1;
        }    

        while (*p) {

            switch (*p++) {

            case '?': 
            case 'h': 
                show_help();
                break;

            case 'v': 
                show_help();
                break;

            case 'V': 
                show_help();
                break;

            case 'i':
                if (*p) {
                    g_interface = p;
                    goto next;
                }

                if (argv[++i]) {
                    g_interface = argv[i];
                    DEBUG("interface: \"%s\"", g_interface);
                    goto next;
                }

                DEBUG("option \"-i\" requires interface name, e.g. em0");
                return -1;

            case 'a':
                if (*p) {
                    g_address = p;
                    goto next;
                }

                if (argv[++i]) {
                    g_address = argv[i];
                    DEBUG("ip address: \"%s\"", g_address);
                    goto next;
                }

                DEBUG("option \"-a\" requires address ip, e.g. 10.10.10.8");
                return -1;

            case 'n':
                if (*p) {
                    g_netmask = p;
                    goto next;
                }

                if (argv[++i]) {
                    g_netmask = argv[i];
                    DEBUG("netmask: \"%s\"", g_netmask);
                    goto next;
                }

                DEBUG("option \"-n\" requires netmask, e.g. 255.255.255.0");
                return -1;

            case 'g':
                if (*p) {
                    g_gateway = p;
                    goto next;
                }

                if (argv[++i]) {
                    g_gateway = argv[i];
                    DEBUG("gateway: \"%s\"", g_gateway);
                    goto next;
                }

                DEBUG("option \"-g\" requires gateway ip, e.g. 10.10.10.1");
                return -1;

            case 'm':
                if (*p) {
                    g_macaddress = p;
                    goto next;
                }

                if (argv[++i]) {
                    g_macaddress = argv[i];
                    DEBUG("mac address: \"%s\"", g_macaddress);
                    goto next;
                }

                DEBUG("option \"-g\" requires mac address, e.g. 00:0c:29:07:09:86");
                return -1;

            }
      next:
            continue;
      }
   }
   DEBUG("interface:  %s", g_interface);
   DEBUG("address:    %s", g_address);
   DEBUG("netmask:    %s", g_netmask);
   DEBUG("gateway:    %s", g_gateway);
   DEBUG("macaddress: %s", g_macaddress);
   return 0;
}

int32
usnet_send_frame(usn_mbuf_t *m)
{
   struct pollfd       fds;
   struct netmap_if   *nifp;
   int32               ret, error;
   u_int               size;
   u_char             *buf;
   int                 attemps = 0;
   int i, j;

   // TODO: put a check here
   fds.fd = g_nmd->fd;
   nifp = g_nmd->nifp;

   if ( m == 0 )
      return -USN_ENULLPTR;
   buf = m->head;
   size = m->mlen;

resend:
   if ( attemps==3 )
      return -USN_EBUSY;

   if(g_config.npkts >= g_config.burst ){
      fds.events = POLLOUT;
      fds.revents = 0;
      g_config.npkts = 0;

      ret = poll(&fds, 1, 2000);
      if (ret <= 0 ) {
         // XXX: save pending packets? 
         //      as it is easy to reach line rate.
         goto fail;
      }
      if (fds.revents & POLLERR) {
         struct netmap_ring *tx = NETMAP_RXRING(nifp, g_nmd->cur_tx_ring);
         (void)tx;
	      DEBUG("error on em1, rx [%d,%d,%d]",
         tx->head, tx->cur, tx->tail);
         error = -USN_EFDPOLL;
         goto fail;
      }
       
      if (fds.revents & POLLOUT) {
         goto send;
      }
      goto flush;
   }
send:
   for (j = g_nmd->first_tx_ring; 
            j <= g_nmd->last_tx_ring; j++) {
      struct netmap_ring *ring;
      uint32_t i, idx;
      ring = NETMAP_TXRING(nifp, j);

      if (nm_ring_empty(ring)) {
         continue;
      }

      i = ring->cur;
      idx = ring->slot[i].buf_idx;
      ring->slot[i].flags = 0;
      ring->slot[i].len = size;
      nm_pkt_copy(buf, NETMAP_BUF(ring, idx), size);
      ring->slot[i].len = size;
      g_nmd->cur_tx_ring = j;
      ring->head = ring->cur = nm_ring_next(ring, i); 

      g_config.npkts++;

      return size;
   }

flush:   
   /* flush any remaining packets */
   //printf("flush \n");
   ioctl(fds.fd, NIOCTXSYNC, NULL);
  /* final part: wait all the TX queues to be empty. */
   for (i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
      struct netmap_ring *txring = NETMAP_TXRING(nifp, i);
      while (nm_tx_pending(txring)) {
         ioctl(fds.fd, NIOCTXSYNC, NULL);
         usleep(1); /* wait 1 tick */
      }
   }
   attemps++; 
   goto resend;

fail:
   printf("send_packet: failed to send\n");
   return error;
} 

void 
usnet_netmap_flush()
{
   int i;
   ioctl(g_nmd->fd, NIOCTXSYNC, NULL);
  /* final part: wait all the TX queues to be empty. */
   for (i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
      struct netmap_ring *txring = NETMAP_TXRING(g_nmd->nifp, i);
      while (nm_tx_pending(txring)) {
         ioctl(g_nmd->fd, NIOCTXSYNC, NULL);
         usleep(1); /* wait 1 tick */
      }
   }

}
void 
netmap_wait()
{
   int i;
   struct pollfd pfd = { .fd = g_nmd->fd, .events = POLLOUT };

   for (i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
      struct netmap_ring *txring = NETMAP_TXRING(g_nmd->nifp, i);
      while (nm_tx_pending(txring)) {
         ioctl(pfd.fd, NIOCTXSYNC, NULL);
         usleep(1); // wait 1 tick
      }    
   }
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


static int
test_send(struct netmap_ring *ring, usn_mbuf_t *m,  u_int count)
{
   u_int n, sent, cur = ring->cur;

   n = nm_ring_space(ring);
   if (n < count)
      count = n;
   for (sent = 0; sent < count; sent++) {
      struct netmap_slot *slot = &ring->slot[cur];
      char *p = NETMAP_BUF(ring, slot->buf_idx);
      nm_pkt_copy(m->head, p, m->mlen);
      slot->len = m->mlen;
      cur = nm_ring_next(ring, cur);
   }
   ring->head = ring->cur = cur;
   return (sent);
}



void test_netmap(usn_mbuf_t *m)
{
   int tosend = 0;
   int n, i;
   int rate_limit = 0;
   int sent = 0;
   struct pollfd pfd = { .fd = g_nmd->fd, .events = POLLOUT };
   struct netmap_if *nifp = g_nmd->nifp;
   struct timeval stime, etime;
   struct nm_desc nmd = *g_nmd;
   struct nm_desc *t_nmd;
   uint64_t nmd_flags = 0;

   // re-open netmap device.
   nmd.req.nr_flags = NR_REG_ONE_NIC;
   nmd.req.nr_ringid = 0;  
   printf("interface name:%s,len=%d\n",g_interface, m->mlen);
   t_nmd = nm_open(g_interface, NULL, nmd_flags 
                   | NM_OPEN_IFNAME | NM_OPEN_NO_MMAP, &nmd);

   if (t_nmd == NULL) {
      printf("Unable to open %s: %s", g_interface, strerror(errno));
      return;
   }
   nifp =t_nmd->nifp;
   pfd.fd =t_nmd->fd;
   pfd.events = POLLOUT;

   n = 10000;
   sent = 0;
   g_config.burst = 512;
   printf("g_config.burst=%d\n", g_config.burst);
   gettimeofday(&stime, 0);
   while ( sent < n ) {
      /*
       * wait for available room in the send queue(s)
       */
      if (poll(&pfd, 1, 1000) <= 0) {
         D("poll error/timeout on queue: %s", strerror(errno));
         // goto quit;
      }
      if (pfd.revents & POLLERR) {
         D("poll error");
         goto quit;
      }
      for (i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
         int limit = rate_limit ?  tosend : g_config.burst;
         int cnt = 0;
         if (n > 0 && n - sent < limit)
            limit = n - sent;
         struct netmap_ring *txring = NETMAP_TXRING(nifp, i);
         if (nm_ring_empty(txring))
            continue;

         cnt = test_send(txring, m, limit);
         DEBUG("limit %d tail %d  cnt %d",
            limit, txring->tail, cnt);
         sent += cnt;
      }
   }
   // print info stats
   gettimeofday(&etime, 0);
   timersub(&etime,&stime,&etime);
   printf("num of sent pkts: %d\n", n);
   printf("total time: %lu (seconds) %lu (microseconds) \n", 
              etime.tv_sec, etime.tv_usec);

   /* flush any remaining packets */
   ioctl(pfd.fd, NIOCTXSYNC, NULL);

   /* final part: wait all the TX queues to be empty. */
   for (i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
      struct netmap_ring *txring = NETMAP_TXRING(nifp, i);
      while (nm_tx_pending(txring)) {
         ioctl(pfd.fd, NIOCTXSYNC, NULL);
         usleep(1); /* wait 1 tick */
      }
   }

quit:
  return;
}

/* set the thread affinity. */
int
setaffinity( int i)
{
   cpuset_t cpumask;

   if (i == -1)
      return 0;

   /* Set thread affinity affinity.*/
   CPU_ZERO(&cpumask);
   CPU_SET(i, &cpumask);

   if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_CPUSET, 
          -1, sizeof(cpuset_t), &cpumask) != 0) { 
      DEBUG("Unable to set affinity: %s", strerror(errno));
      return 1;
   }
   return 0;
}


static int 
receive_packets(struct netmap_ring *ring, u_int limit, int dump)
{
   struct netmap_slot *slot;
   u_int               cur, rx, n;
   u_char             *p; 

#ifdef TIME_NANO
   clock_gettime(CLOCK_REALTIME, &g_time);
#else
   gettimeofday(&g_time, 0); 
#endif
   cur = ring->cur;
   n = nm_ring_space(ring);
   if (n < limit)
      limit = n;  
   for (rx = 0; rx < limit; rx++) {
      slot = &ring->slot[cur];
      p = (u_char*)NETMAP_BUF(ring, slot->buf_idx);

      eth_input(p, slot->len);

      cur = nm_ring_next(ring, cur);
   }   
   ring->head = ring->cur = cur; 

   return (rx);
}

int
usnet_setup(int argc, char *argv[])
{
   //struct nm_desc     *nmd;
   char               *p;
   int                 ret;

   (void)argc;
   (void)argv;
   (void)p;
   //(void)nmd;

   setaffinity(0);

   ret = usnet_get_options(argc, argv);
   if ( ret < 0 ) {
      show_help();
      exit(0);
   }

   g_nmd = usnet_init(g_nmd, (char*)g_interface, 0);

  if (1) {
      struct netmap_if *nifp = g_nmd->nifp;
      struct nmreq *req = &g_nmd->req;
      int i;
      D("fisrt_tx_ring=%d, last_tx_ring=%d", g_nmd->first_tx_ring, g_nmd->last_tx_ring);
      D("nifp at offset %d, %d tx %d rx region %d",
          req->nr_offset, req->nr_tx_rings, req->nr_rx_rings,
          req->nr_arg2);
      for (i = 0; i <= req->nr_tx_rings; i++) {
         struct netmap_ring *ring = NETMAP_TXRING(nifp, i);
         D("   TX%d at 0x%p slots %d", i,
             (void *)((char *)ring - (char *)nifp), ring->num_slots);
      }    
      for (i = 0; i <= req->nr_rx_rings; i++) {
         struct netmap_ring *ring = NETMAP_RXRING(nifp, i);
         D("   RX%d at 0x%p slots %d", i,
             (void *)((char *)ring - (char *)nifp), ring->num_slots);
      }    
   }
   return 0;
}
void
usnet_dispatch()
{
   struct netmap_if   *nifp;
   struct pollfd       fds;
   struct netmap_ring *rxring;
   int                 ret;

   nifp = g_nmd->nifp;
   fds.fd = g_nmd->fd;

   while(!do_abort) {

#ifdef TIME_NANO
       clock_gettime(CLOCK_REALTIME, &g_time);
#else
       gettimeofday(&g_time,0);
#endif
       //fds.events = POLLIN | POLLOUT;
       fds.events = POLLIN;
       fds.revents = 0;
       ret = poll(&fds, 1, 3000);
       if (ret <= 0 ) {
          DEBUG("poll %s ev %x %x rx @%d:%d:%d ", 
            ret <= 0 ? "timeout" : "ok",
            fds.events,
            fds.revents,
            NETMAP_RXRING(g_nmd->nifp, g_nmd->cur_rx_ring)->head,
            NETMAP_RXRING(g_nmd->nifp, g_nmd->cur_rx_ring)->cur,
            NETMAP_RXRING(g_nmd->nifp, g_nmd->cur_rx_ring)->tail);
          DEBUG("poll %s ev %x %x tx @%d:%d:%d ", 
            ret <= 0 ? "timeout" : "ok",
            fds.events,
            fds.revents,
            NETMAP_RXRING(g_nmd->nifp, g_nmd->cur_tx_ring)->head,
            NETMAP_RXRING(g_nmd->nifp, g_nmd->cur_tx_ring)->cur,
            NETMAP_RXRING(g_nmd->nifp, g_nmd->cur_tx_ring)->tail);
          continue;
       }
       if (fds.revents & POLLERR) {
          struct netmap_ring *rx = NETMAP_RXRING(nifp, g_nmd->cur_rx_ring);
          (void)rx;
			 DEBUG("error on em1, rx [%d,%d,%d]",
					 rx->head, rx->cur, rx->tail);
       }
/*       
       if (fds.revents & POLLOUT) {
          for (int j = g_nmd->first_tx_ring; 
                   j <= g_nmd->last_tx_ring; j++) {
             txring = NETMAP_RXRING(nifp, j);

				 DEBUG("Ring info tx(%d), head=%d, cur=%d, tail=%d", 
                   j, txring->head, txring->cur, txring->tail); 

             if (nm_ring_empty(txring)) {
                continue;
             }

             //send_packets(rxring, 512, 1);
          }
       }
*/
       if (fds.revents & POLLIN) {
          int j;
          for (j = g_nmd->first_rx_ring; 
                   j <= g_nmd->last_rx_ring; j++) {
             rxring = NETMAP_RXRING(nifp, j);

				 DEBUG("Ring info rx(%d), head=%d, cur=%d, tail=%d", 
                   j, rxring->head, rxring->cur, rxring->tail); 

             if (nm_ring_empty(rxring)) {
                continue;
             }

             receive_packets(rxring, 512, 1);
          }
       }
       //tcp_fasttimo();
       tcp_slowtimo();
   }
   nm_close(g_nmd);
   return;
}

int32
usnet_socket(u_int32 dom, u_int32 type, u_int32 proto)
{
   struct usn_socket* so;
   int    fd;
   //fd = usnet_create_socket(dom, &so, type, proto);
   fd = usnet_create_socket(USN_AF_INET, &so, type, proto);
   if ( fd <= 0 ) {
      DEBUG("failed to create socket, fd=%d", fd);
      return -1;
   }
   return fd;
}


int 
usnet_bind(u_int32 s, u_int32 addr, u_short port)
{
   DEBUG("binding addr: addr=%x, port=%d", addr, port);
   usnet_bind_socket(s, addr, port);
   return 0;
}


int32
usnet_listen(u_int32 fd, int32 flags, 
             accept_handler_cb accept_cb, 
             error_handler_cb error_cb, void* arg)
{
   DEBUG("listen on socket, fd=%d", fd);

   usnet_listen_socket(fd, flags, accept_cb, error_cb, arg);

   return 0;
}


int32
usnet_read(u_int fd, u_char *buf, u_int len)
{
   int32 ret = 0;
   ret = usnet_read_socket(fd, buf, len);
   return ret;
}

usn_buf_t*
usnet_get_buffer(u_int32 fd)
{
   return usnet_get_sobuffer(fd);
}

int32
usnet_write_buffer(u_int32 fd, usn_buf_t *buf)
{
   return usnet_write_sobuffer(fd, buf);
}

int32
usnet_writeto_buffer(u_int32 fd, usn_buf_t *buf, struct usn_sockaddr_in* addr)
{
   return usnet_writeto_sobuffer(fd, buf, addr);
}


int32
usnet_udp_broadcast(u_int32 fd, u_char* buff, u_int32 len, 
       struct usn_sockaddr_in* addrs, u_int32 addr_num)
{
   usnet_udp_sobroadcast(fd, buff, len, addrs, addr_num);
   return 0;
}

int32
usnet_set_callbacks(u_int32 fd, 
                    read_handler_cb read_cb, 
                    write_handler_cb write_cb, 
                    error_handler_cb error_cb)
{
   return 0;
}


