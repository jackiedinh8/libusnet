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
 * @(#)event.h
 */

#ifndef _USNET_EVENT_H_
#define _USNET_EVENT_H_

#include <stdint.h>

#include "types.h"

extern char *type_str[];
extern char *event_str[];
 
enum usn_proto_type {
   TCP_TYPE = 1,
   UDP_TYPE = 2
};

enum usn_events {
   USN_READ_EV = 1,
   USN_WRITE_EV = 2,
   USN_ACCEPT_EV = 3,
   USN_TCPST_EV = 4,
   USN_ERROR_EV = 5,
   USN_LISTEN_EV = 6,
   USN_CONNECT_EV = 7,
};

struct header {
   uint8_t type;
   uint8_t event;
   uint16_t length;
   
};

struct usn_read_ev {
   struct header h;
   uint32_t fd;
};

struct usn_write_ev {
   struct header h;
   uint32_t fd;
};

struct usn_accept_ev {
   struct header h;
   uint32_t fd;
   uint32_t ip;
   uint16_t port;
   int32_t  result;
};

struct usn_connect_ev {
   struct header h;
   uint32_t fd;
   uint32_t saddr;
   uint16_t sport;
   uint32_t daddr;
   uint16_t dport;
   int32_t  result;
};

struct usn_error_ev {
   struct header h;
   uint32_t fd;
   uint32_t error;
};

struct usn_tcpst_ev {
   struct header h;
   uint32_t fd;
   uint32_t state;
};

/* socket api */
struct usn_listen_ev {
   struct header h;
   uint32_t fd;
   uint32_t addr;
   uint16_t port;
   int32_t  result;
};


int
usnet_tcpev_init(usn_context_t *ctx);

/* used by network stack */
void
usnet_raise_read_event(usn_context_t *ctx, usn_tcb_t *tcb);

void 
usnet_raise_write_event(usn_context_t *ctx, usn_tcb_t *tcb);

void 
usnet_raise_error_event(usn_context_t *ctx, usn_tcb_t *tcb);

void    
usnet_raise_accept_event(usn_context_t *ctx, usn_tcb_t* tcb);

void    
usnet_raise_connect_event(usn_context_t *ctx, usn_tcb_t* tcb);

void    
usnet_raise_tcp_event(usn_context_t *ctx, usn_tcb_t* tcb);

void
usnet_tcpev_net(usn_context_t *ctx);

/* used by applications */

void
usnet_tcpev_process(usn_context_t *ctx);

int
usnet_write_new(usn_context_t *ctx, uint32_t fd, char* data, int len);

void
usnet_write_notify(usn_context_t *ctx, uint32_t fd);



#endif  //!_USNET_API_H_
