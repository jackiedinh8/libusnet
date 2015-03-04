#ifndef _USNET_MSG_H
#define _USNET_MSG_H

#include "usnet_mq.h"

extern usn_shmmq_t *g_tcp_in_mq;
extern usn_shmmq_t *g_tcp_out_mq;

#define MAX_BUFFER_SIZE 1024*1024

typedef struct usn_header usn_header_t;
struct usn_header {
   u_char  type;
   u_char  event;
   u_short len;
};

enum usn_basic_type {
   USN_TCP_IN = 0x01,   /* events occurred by incoming pkgs*/
   USN_TCP_OUT = 0x02,
   USN_UDP_IN = 0x03,
   USN_UDP_OUT = 0x04,
   USN_EXT = 0x05
};

enum usn_tcp_event {
   /* tcp state change */
   USN_TCPST_CLOSED = 0,       /* closed */
   USN_TCPST_LISTEN = 1,       /* listening for connection */
   USN_TCPST_SYN_SENT = 2,     /* active, have sent syn */
   USN_TCPST_SYN_RECEIVED = 3, /* have send and received syn */
   USN_TCPST_ESTABLISHED = 4,  /* established */
   USN_TCPST_CLOSE_WAIT = 5,   /* rcvd fin, waiting for close */
   USN_TCPST_FIN_WAIT1 = 6,    /* have closed, sent fin */
   USN_TCPST_CLOSING = 7,      /* closed xchd FIN; await FIN ACK */
   USN_TCPST_LAST_ACK = 8,     /* had fin and close; await FIN ACK */
   USN_TCPST_FIN_WAIT2 = 9,    /* have closed, fin is acked */
   USN_TCPST_TIME_WAIT = 10,   /* in 2*msl quiet wait after close */
   /* tcp event */ 
   USN_TCPEV_READ = 11,        /* new pkg is comming */
   USN_TCPEV_WRITE = 12,       /* send buffer is available */
   USN_TCPEV_OUTOFBOUND = 13,  /* out of bound data occurs */
   USN_TCPEV_DISCONNECTED = 14,/* disconnected */
   /* tcp api */
   USN_TCPAPI_CONNECT = 15,     /* connect */
   USN_TCPAPI_BIND = 17,       /* connect */
   USN_TCPAPI_LISTEN = 18,       /* connect */
   USN_TCPAPI_CLOSE = 19,       /* connect */
   USN_TCP_NR = 20           /* total number of tcp events */
};

struct usn_tcpin_closed_t {
   // TODO: to be defined.
};

struct usn_tcpin_listen_t {
   // TODO: to be defined.
};

struct usn_tcpin_syn_sent_t {
   // TODO: to be defined.
};

struct usn_tcpin_syn_received_t {
   // TODO: to be defined.
};

struct usn_tcpin_established_t {
   // TODO: to be defined.
};

struct usn_tcpin_close_wait_t {
   // TODO: to be defined.
};

struct usn_tcpin_fin_wait1_t {
   // TODO: to be defined.
};

struct usn_tcpin_closing_t {
   // TODO: to be defined.
};

struct usn_tcpin_last_ack_t {
   // TODO: to be defined.
};

struct usn_tcpin_fin_wait2_t {
   // TODO: to be defined.
};

struct usn_tcpin_time_wait_t {
   // TODO: to be defined.
};

struct usn_tcpin_sosnd_avail_t {
   // TODO: to be defined.
};

struct usn_tcpin_sbnotify_t {
   // TODO: to be defined.
};

struct usn_tcpin_newdata_t {
   // TODO: to be defined.
};

struct usn_tcpin_outofbound_t {
   // TODO: to be defined.
};

typedef struct usn_tcpin_pkg usn_tcpin_pkg_t;
struct usn_tcpin_pkg {
   usn_header_t  header;
   u_int32       fd;
   union {
      struct usn_tcpin_closed_t       ev_closed;
      struct usn_tcpin_listen_t       ev_listen;
      struct usn_tcpin_syn_sent_t     ev_syn_sent;
      struct usn_tcpin_syn_received_t ev_syn_received;
      struct usn_tcpin_established_t  ev_established;
      struct usn_tcpin_close_wait_t   ev_close_wait;
      struct usn_tcpin_fin_wait1_t    ev_fin_wait1;
      struct usn_tcpin_closing_t      ev_closing;
      struct usn_tcpin_last_ack_t     ev_last_ack;
      struct usn_tcpin_fin_wait1_t    ev_fin_wait2;
      struct usn_tcpin_time_wait_t    ev_time_wait;
      struct usn_tcpin_sosnd_avail_t  ev_sosnd_avail;
      struct usn_tcpin_sbnotify_t     ev_sbnotify;
      struct usn_tcpin_newdata_t      ev_newdata;
      struct usn_tcpin_outofbound_t   ev_outofbound;
   };
};

struct usn_tcpout_closed_t {
   // TODO: to be defined.
};

struct usn_tcpout_listen_t {
   // TODO: to be defined.
};

struct usn_tcpout_syn_sent_t {
   // TODO: to be defined.
};

struct usn_tcpout_syn_received_t {
   // TODO: to be defined.
};

struct usn_tcpout_established_t {
   // TODO: to be defined.
};

struct usn_tcpout_close_wait_t {
   // TODO: to be defined.
};

struct usn_tcpout_fin_wait1_t {
   // TODO: to be defined.
};

struct usn_tcpout_closing_t {
   // TODO: to be defined.
};

struct usn_tcpout_last_ack_t {
   // TODO: to be defined.
};

struct usn_tcpout_fin_wait2_t {
   // TODO: to be defined.
};

struct usn_tcpout_time_wait_t {
   // TODO: to be defined.
};

struct usn_tcpout_pkg {
   usn_header_t header;
   u_int32       fd;
   union {
      struct usn_tcpout_closed_t       ev_closed;
      struct usn_tcpout_listen_t       ev_listen;
      struct usn_tcpout_syn_sent_t     ev_syn_sent;
      struct usn_tcpout_syn_received_t ev_syn_received;
      struct usn_tcpout_established_t  ev_established;
      struct usn_tcpout_close_wait_t   ev_close_wait;
      struct usn_tcpout_fin_wait1_t    ev_fin_wait1;
      struct usn_tcpout_closing_t      ev_closing;
      struct usn_tcpout_last_ack_t     ev_last_ack;
      struct usn_tcpout_fin_wait1_t    ev_fin_wait2;
      struct usn_tcpout_time_wait_t    ev_time_wait;
   };
};

enum usn_udp_event {
   usn_udpev_nr = 1           /* total number of udp events */
};


int32
usnet_mq_enqueue(usn_shmmq_t *mq, u_int32 fd, u_char type, 
      u_char event, u_char *body, int32 len);

int32
usnet_mq_dequeue(usn_shmmq_t *mq, u_char *buf, u_int32 buf_size, u_int32 *data_len);

void 
usnet_print_header(usn_header_t *h);

#endif //_USNET_MSG_H

