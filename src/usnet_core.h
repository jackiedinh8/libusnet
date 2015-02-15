/*
 * Copyright (c) 2014-2015 Jackie Dinh <jackiedinh8@gmail.com>
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

// Start handling loop
void
usnet_dispatch();

// ip stack handling
struct usn_sockaddr;
struct usn_sockaddr_in;
typedef void (*read_handler_cb)(u_int32 fd, 
                       u_short flags, void* arg);
typedef void (*write_handler_cb)(u_int32 fd, 
                       u_short flags, void* arg);
typedef void (*accept_handler_cb)(u_int32 fd, 
                      struct usn_sockaddr* addr, 
                      int32 len, void* arg);
typedef void (*event_handler_cb)(u_int32 fd, 
                      int32 event, void* arg);

// socket functionality
int32
usnet_socket(u_int32 dom, u_int32 type, u_int32 proto);

int32 
usnet_bind(u_int32 fd, u_int32 addr, u_short port);

int32
usnet_listen(u_int32 fd, int32 flags, 
      accept_handler_cb func_cb, 
      event_handler_cb error_cb, void* arg);

int32
usnet_set_callbacks(u_int32 fd, read_handler_cb read_cb, 
      write_handler_cb write_cb, event_handler_cb event_cb);

int32 
usnet_connect(u_int32 fd, u_int32 addr, u_short port);

int32
usnet_close_socket(u_int32 fd);

int32 
usnet_recv(u_int32 fd, u_char* buff, u_int len);

int32 
usnet_send(u_int32 fd, u_char* buff, u_int len);

int32 
usnet_sendto(u_int32 fd, u_char* buff, u_int len,
            struct usn_sockaddr_in* addr);

int32
usnet_read(u_int32 fd, u_char* buff, u_int len);

int32
usnet_write(u_int32 fd, u_char* buff, u_int len);

int32 
usnet_writeto(u_int32 fd, u_char* buff, u_int len,
            struct usn_sockaddr_in* addr);

// other buffer functionality
usn_buf_t*
usnet_get_buffer_input(u_int fd);

usn_buf_t*
usnet_get_buffer_output(u_int fd);

u_int32 
usnet_get_length(usn_buf_t*);

int32
usnet_buffer_copyout(usn_buf_t* m, void* data, int32 datlen);

int32 
usnet_buffer_remove(usn_buf_t* m, void* data, int32 datlen);

int32 
usnet_buffer_drain(usn_buf_t* m, int32 len);

int32
usnet_write_buffer(u_int fd, usn_buf_t *buf);

// other functionality
int32
usnet_writeto_buffer(u_int fd, usn_buf_t *buf, 
      struct usn_sockaddr_in* addr);

int32
usnet_clear_buffer(u_int32 fd);


int32
usnet_udp_broadcast(u_int32 fd, u_char* buff, u_int32 len, 
      struct usn_sockaddr_in* addrs, u_int32 addr_num);


// eth functionality
int32 
usnet_send_frame(usn_mbuf_t *m);


// Auxiliary functions
//#define DUMP_PAYLOAD
void
dump_buffer(char *p, int len, const char *prefix);

void
dump_chain(usn_mbuf_t *m, const char *prefix);

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

