#ifndef _USNET_CORE_HEADER_H_
#define _USNET_CORE_HEADER_H_

#include <net/if.h>	/* ifreq */
#include <time.h>	/* ifreq */
#include <sys/param.h>
#include <sys/cpuset.h>

#define NETMAP_WITH_LIBS
#include <net/netmap.h>
#include <net/netmap_user.h>

#include "usnet_buf.h"
#include "usnet_slab.h"

extern struct nm_desc  *g_nmd;
extern usn_slab_pool_t *g_slab_pool;

#ifdef TIME_NANO
extern struct timespec g_time;
#else
extern struct timeval g_time;
#endif

extern char* g_interface;
extern char* g_address;
extern char* g_netmask;
extern char* g_gateway;
extern char* g_macaddress;

struct glob_arg {
   int pkt_size;
   int burst;
   int forever;
   int npkts;  /* total packets to send */
   int tpkts;  /* total packets to send */
   int frags;  /* fragments per packet */
   int nthreads;
   int cpus;
   int options;   /* testing */
   int dev_type;
   int tx_rate;
   struct timespec tx_period;

   int affinity;
   int main_fd;
   struct nm_desc *nmd;
   int report_interval;    /* milliseconds between prints */
   void *(*td_body)(void *);
   void *mmap_addr;
#define MAX_IFNAMELEN 64
   char ifname[MAX_IFNAMELEN];
   char *nmr_config;
   int dummy_send;
   int virt_header;  /* send also the virt_header */
   int extra_bufs;      /* goes in nr_arg3 */
};

extern struct glob_arg g_arg;

typedef struct base_deivce base_device_t;
struct base_device {
   struct nm_desc mn_desc;
   u_int          info;
};

typedef struct usn_config usn_config_t;
struct usn_config {
   u_int  flags;
   // and more
};

// XXX: better to read config from file?
int
usnet_setup(int argc, char *argv[]);

struct nm_desc*
usnet_init( struct nm_desc *nmd, const char *dev_name, u_int flags);

int 
usnet_init_internal();

// Start handling loop
void
usnet_dispatch();

// ip stack handling
typedef void (*accept_handler_cb)(int fd);
typedef void (*tcp_handler_cb)(int fd);
typedef void (*udp_handler_cb)(int fd);

void
usnet_tcp_listen(u_short port, accept_handler_cb cb);

void
usnet_udp_listen(u_short port, udp_handler_cb cb);

void
usnet_register_tcp_handler(int fd, tcp_handler_cb cb);

int 
usnet_read(int fd, u_char* buff, u_int len);

int 
usnet_write(int fd, u_char* buff, u_int len);

size_t
usnet_get_length(int fd);

int 
usnet_drain(int fd, size_t len);


// Auxiliary functions

void 
print_msg(const char *msg, int len, const char *text);

void
dump_payload(u_char *p, int len, struct netmap_ring *ring, int cur);

void
dump_payload_only(char *p, int len);

void
dump_buffer(char *p, int len, const char *prefix);

int
usn_get_options(int argc, char* const *argv);

void 
show_help();

int 
send_packet(usn_mbuf_t *m);

void
netmap_flush();


void 
test_netmap(usn_mbuf_t *m);
#endif // !_USNET_CORE_HEADER_H_

