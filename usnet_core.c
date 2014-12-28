#include <ctype.h>      /* isprint() */
#include <errno.h>
#include <stdio.h>
#include <string.h>  /* strcmp */
#include <sys/types.h>
#include <sys/sysctl.h>

#include "usnet_core.h"
#include "usnet_log.h"
#include "usnet_arp.h"
#include "usnet_eth.h"
#include "usnet_if.h"
#include "usnet_ip_var.h"
#include "usnet_udp.h"
#include "usnet_slab.h"

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
struct glob_arg g_arg;

struct ipq g_ipq;

void 
print_msg(const char *msg, int len, const char *text)
{
  int i, j;
  char tmp[4], buf[4086]={0};
  int print_len = 0;  
  print_len = len < 18 ? len : 18; 

  for(i=0,j=1; i<print_len; i++,j++) {
    sprintf(tmp, "%02X", (unsigned char)msg[i]);
    strcat(buf, tmp);
    strcat(buf, " ");
  }    
  printf("print_msg(%s): str=%s\n", text, buf);
}

void
usnet_setup(usn_config_t * config)
{
   return;
}

struct nm_desc*
usnet_init( struct nm_desc *gg_nmd, const char *dev_name, u_int flags)
{
   struct nmreq        nmr;
   struct nm_desc     *nmd = NULL;
   struct netmap_if   *nifp = NULL;
   struct netmap_ring *txr, *rxr;

   DEBUG("open dev: %s", dev_name);
   bzero(&nmr, sizeof(nmr));
   strcpy(nmr.nr_name, dev_name);

   //nmr.nr_version = NETMAP_API;
   //nmr.nr_ringid = 0;
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

   memset(&g_arg, 0, sizeof(g_arg));
   g_arg.burst = 1000;
   g_arg.tx_rate = 0;

   memset(&g_ipq, 0, sizeof(g_ipq));

   usnet_init_internal();

   route_init();

   init_network();

   udp_init();

   init_ipv4();


   DEBUG("usnet_init: done");
   return nmd;
}

int
usnet_init_internal()
{

   DEBUG("usnet_init_internal: start");

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

   DEBUG("usnet_init_internal: done");
   return 0;
}

void
usnet_dispatch()
{
   // XXX while loop goes here.
   return;
}

// ip stack handling
void
usnet_tcp_listen(u_short port, accept_handler_cb cb)
{
   return;
}

void
usnet_udp_listen(u_short port, udp_handler_cb cb)
{
   return;
}

void
usnet_register_tcp_handler(int fd, tcp_handler_cb cb)
{
   return;
}

int 
usnet_read(int fd)
{
   return 0;
}

int 
usnet_write(int fd)
{
   return 0;
}

size_t
usnet_get_length(int fd)
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

   return;

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

void
dump_payload_only(char *p, int len)
{
   char buf[128];
   int i, j, i0;

   return;

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
      printf("%s\n", buf);
   }
}

void
dump_payload(u_char *p, int len, struct netmap_ring *ring, int cur)
{
   char buf[128];
   int i, j, i0;

   return;

   /* get the length in ASCII of the length of the packet. */

   printf("ring %p[%p] cur %5d [buf %6d flags 0x%04x len %5d]\n",
      ring, p, cur, ring->slot[cur].buf_idx,
      ring->slot[cur].flags, len);

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
      printf("%s\n", buf);
   }
}

void show_help()
{
    DEBUG("desc: stack -i interface -a address -n netmask -g gateway -m macaddress");
}
int
usn_get_options(int argc, char* const *argv)
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
   return 0;
}

int send_packet(usn_mbuf_t *m)
{
   struct pollfd       fds;
   struct netmap_if    *nifp;
   int                 ret;
   u_int               size;
   u_char             *buf;
   int                 attemps = 0;
   // TODO: put a check here
   fds.fd = g_nmd->fd;
   nifp = g_nmd->nifp;

   if ( m == 0 )
      return 0;
   buf = m->head;
   size = m->mlen;

resend:
   if ( attemps==3 )
      return 0;

   if(g_arg.npkts >= g_arg.burst ){
      fds.events = POLLOUT;
      fds.revents = 0;
      g_arg.npkts = 0;

      ret = poll(&fds, 1, 2000);
      if (ret <= 0 ) {
         // XXX: save pending packets? 
         //      it is easy to reach line rate.
         goto fail;
      }
      if (fds.revents & POLLERR) {
         struct netmap_ring *tx = NETMAP_RXRING(nifp, g_nmd->cur_tx_ring);
         (void)tx;
	      DEBUG("error on em1, rx [%d,%d,%d]",
         tx->head, tx->cur, tx->tail);
         goto fail;
      }
       
      if (fds.revents & POLLOUT) {
         goto send;
      }
      goto flush;
   }
send:
   for (int j = g_nmd->first_tx_ring; 
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

      g_arg.npkts++;
      g_arg.tpkts++;

      return size;
   }

flush:   
   /* flush any remaining packets */
   //printf("flush \n");
   ioctl(fds.fd, NIOCTXSYNC, NULL);
  /* final part: wait all the TX queues to be empty. */
   for (int i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
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
   return 0;
} 

void netmap_flush()
{
   ioctl(g_nmd->fd, NIOCTXSYNC, NULL);
  /* final part: wait all the TX queues to be empty. */
   for (int i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
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
   struct pollfd pfd = { .fd = g_nmd->fd, .events = POLLOUT };

   for (int i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
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
   int n;
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
   printf("interface name:%s",g_interface);
   t_nmd = nm_open(g_interface, NULL, nmd_flags 
                   | NM_OPEN_IFNAME | NM_OPEN_NO_MMAP, &nmd);

   if (t_nmd == NULL) {
      printf("Unable to open %s: %s", g_interface, strerror(errno));
      return;
   }
   nifp =t_nmd->nifp;
   pfd.fd =t_nmd->fd;
   pfd.events = POLLOUT;

   n = 1000000;
   gettimeofday(&stime, 0);
   while ( sent < n ) {
      /*
       * wait for available room in the send queue(s)
       */
      if (poll(&pfd, 1, 2000) <= 0) {
         D("poll error/timeout on queue: %s", strerror(errno));
         // goto quit;
      }
      if (pfd.revents & POLLERR) {
         D("poll error");
         goto quit;
      }
      for (int i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
         int limit = rate_limit ?  tosend : g_arg.burst;
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
   for (int i = g_nmd->first_tx_ring; i <= g_nmd->last_tx_ring; i++) {
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

static int do_abort = 0;

static void
sigint_h(int sig)
{
	(void)sig;  /* UNUSED */
	do_abort = 1;
	signal(SIGINT, SIG_DFL);
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

      if (dump)
         dump_payload(p, slot->len, ring, cur);

      eth_input(p, slot->len);

      cur = nm_ring_next(ring, cur);
   }   
   ring->head = ring->cur = cur; 

   return (rx);
}

int
usnet_setup(int argc, char *argv[])
{
   struct netmap_if   *nifp;
   struct netmap_ring *txring, *rxring;
   struct nm_desc     *nmd;
   struct pollfd       fds;
   char               *p;
   int                 ret;

   (void)argc;
   (void)argv;
   (void)nifp;
   (void)p;
   (void)txring;
   (void)nmd;

   setaffinity(0);

   ret = usn_get_options(argc, argv);
   if ( ret < 0 ) {
      show_help();
      exit(0);
   }

   DEBUG("interface:  %s", g_interface);
   DEBUG("address:    %s", g_address);
   DEBUG("netmask:    %s", g_netmask);
   DEBUG("gateway:    %s", g_gateway);
   DEBUG("macaddress: %s", g_macaddress);
   //g_nmd = usnet_init(g_nmd, "netmap:em1", 0);
   g_nmd = usnet_init(g_nmd, (char*)g_interface, 0);

  if (true) {
      struct netmap_if *nifp = g_nmd->nifp;
      struct nmreq *req = &g_nmd->req;
      D("fisrt_tx_ring=%d, last_tx_ring=%d", g_nmd->first_tx_ring, g_nmd->last_tx_ring);
      D("nifp at offset %d, %d tx %d rx region %d",
          req->nr_offset, req->nr_tx_rings, req->nr_rx_rings,
          req->nr_arg2);
      for (int i = 0; i <= req->nr_tx_rings; i++) {
         struct netmap_ring *ring = NETMAP_TXRING(nifp, i);
         D("   TX%d at 0x%p slots %d", i,
             (void *)((char *)ring - (char *)nifp), ring->num_slots);
      }    
      for (int i = 0; i <= req->nr_rx_rings; i++) {
         struct netmap_ring *ring = NETMAP_RXRING(nifp, i);
         D("   RX%d at 0x%p slots %d", i,
             (void *)((char *)ring - (char *)nifp), ring->num_slots);
      }    
   }

   nifp = g_nmd->nifp;
   fds.fd = g_nmd->fd;


   signal(SIGINT, sigint_h);
   // XXX: dispatch funtion of libusnet
   while(!do_abort) {
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
          for (int j = g_nmd->first_rx_ring; 
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
   }
   nm_close(g_nmd);
   return 0;
}

