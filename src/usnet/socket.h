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
 * @(#)socket.h
 */

#ifndef USNET_SOCKET_H_
#define USNET_SOCKET_H_

#include <stdint.h>

#include "types.h"

#define SO_DEFAULT_BACKLOG 500;
struct backlog_queue {
   int bq_head;
   int bq_tail;
   int bq_size;
   uint32_t *bq_connq;
};

struct usn_socket {
   uint32_t  so_fd;
   uint16_t  so_dom;
   uint16_t  so_type;
   uint16_t  so_proto;
   uint16_t  so_state;
   uint16_t  so_sport;
   uint16_t  so_dport;
   uint32_t  so_saddr;
   uint32_t  so_daddr;
   uint16_t  so_options;
   void     *so_tcb;
   struct backlog_queue *so_backlogq;
};

/* so_options */
#define  SO_BIND        0x0001       /* bound to local address */
#define  SO_ACCEPT      0x0002      /* socket has had listen() */
//#define  SO_REUSEADDR   0x0004      /* allow local address reuse */
//#define  SO_KEEPALIVE   0x0008      /* keep connections alive */
//#define  SO_DONTROUTE   0x0010      /* just use interface addresses */
//#define  SO_BROADCAST   0x0020      /* permit sending of broadcast msgs */
//#define  SO_USELOOPBACK 0x0040      /* bypass hardware when possible */
//#define  SO_LINGER      0x0080      /* linger on close if data present */
//#define  SO_OOBINLINE   0x0100      /* leave received OOB data in line */
//#define  SO_REUSEPORT   0x0200      /* allow local address & port reuse */

/* so_state */
#define  SO_NOFDREF           0x001 /* no file table ref any more */
#define  SO_ISCONNECTED       0x002 /* socket connected to a peer */
#define  SO_ISCONNECTING      0x004 /* in process of connecting to peer */
#define  SO_ISDISCONNECTING   0x008 /* in process of disconnecting */
#define  SO_CANTSENDMORE      0x010 /* can't send more data to peer */
#define  SO_CANTRCVMORE       0x020 /* can't receive more data from peer */
#define  SO_RCVATMARK         0x040 /* at mark on input */
#define  SO_PRIV              0x080 /* privileged for broadcast, raw... */
#define  SO_NBIO              0x100 /* non-blocking ops, user-space */
#define  SO_ASYNC             0x200 /* async i/o notify, user-space */
#define  SO_ISCONFIRMING      0x400 /* deciding to accept connection req */

void
usnet_socket_init(usn_context_t *ctx, int create);

inline usn_socket_t*
usnet_get_socket(usn_context_t *ctx, usn_socket_t *key);

inline usn_socket_t*
usnet_search_socket(usn_context_t *ctx, usn_socket_t *sitem);

inline usn_socket_t*
usnet_insert_socket(usn_context_t *ctx, usn_socket_t *sitem);

inline int
usnet_remove_socket(usn_context_t *ctx, usn_socket_t *sitem);

/* socket events */
int
usnet_send_listen_event(usn_context_t *ctx, uint32_t fd, uint32_t addr, uint16_t port);

#endif //USNET_SOCKET_H_

