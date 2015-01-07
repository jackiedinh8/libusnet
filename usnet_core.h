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
//#include "usnet_socket.h"

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


struct usn_glob_conf {
   int burst;
   int forever;
   int npkts;  // total packets to send
   int nthreads;
   int cpus;
   int options;   // testing
   int dev_type;
   int tx_rate;
   struct timespec tx_period;

   int affinity;
   int main_fd;
   struct nm_desc *nmd;
   int report_interval;    // milliseconds between prints
   void *mmap_addr;
#define MAX_IFNAMELEN 64
   char ifname[MAX_IFNAMELEN];
   char *nmr_config;
   int dummy_send;
   int extra_bufs;      // goes in nr_arg3
};

extern struct usn_glob_conf g_config;

// XXX: better to read config from file?
int 
usnet_parse_conf(int argc, char *argv[]);

int
usnet_setup(int argc, char *argv[]);

struct nm_desc*
usnet_init( struct nm_desc *nmd, const char *dev_name, u_int flags);

int32
usnet_finit( struct nm_desc *nmd);

int32 
usnet_init_internal();

int32
usnet_init_internal();

// Start handling loop
void
usnet_dispatch();

// ip stack handling
struct usn_sockaddr;
struct usn_sockaddr_in;
typedef void (*socket_handler_cb)(u_int32 fd, u_short flags, void* arg);
typedef void (*accept_handler_cb)(u_int32 fd, struct usn_sockaddr* addr, int32 len, void* arg);
typedef void (*error_handler_cb)(int32 error, void* arg);

// common functionality
int32
usnet_socket(u_int32 dom, u_int32 type, u_int32 proto);

int32 
usnet_bind(u_int32 s, u_int32 addr, u_short port);

int32
usnet_listen(u_int32 fd, int32 flags, accept_handler_cb func_cb, error_handler_cb error_cb, void* arg);

int32
usnet_set_socket_cb(u_int32 fd, int32 flags, socket_handler_cb func_cb, void* arg);

int 
usnet_connect();

int 
usnet_recv(int fd, u_char* buff, u_int len);

int 
usnet_send(int fd, u_char* buff, u_int len);

int32
usnet_read(u_int32 fd, u_char* buff, u_int len);

int32
usnet_write(u_int32 fd, u_char* buff, u_int len);

u_int32
usnet_get_length(u_int fd);

usn_buf_t*
usnet_get_buffer(u_int fd);

int32
usnet_write_buffer(u_int fd, usn_buf_t *buf);

int32
usnet_writeto_buffer(u_int fd, usn_buf_t *buf, struct usn_sockaddr_in* addr);

int 
usnet_drain(int fd, size_t len);

int
usnet_udp_broadcast(int *fd, u_int fd_size, u_char* buff, u_int len);

// eth functionality
int 
usnet_send_frame(usn_mbuf_t *m);


// Auxiliary functions
//#define DUMP_PAYLOAD
void
dump_buffer(char *p, int len, const char *prefix);

int
usnet_get_options(int argc, char* const *argv);

void 
show_help();

// netmap-related functionality
void
usnet_netmap_flush();


void 
test_netmap(usn_mbuf_t *m);
#endif // !_USNET_CORE_HEADER_H_

