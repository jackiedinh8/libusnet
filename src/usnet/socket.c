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
 * @(#)socket.c
 */

#include "socket.h"
#include "cache.h"
#include "core.h"
#include "event.h"
#include "log.h"

inline int 
usnet_so_key(const void *item)
{
   usn_socket_t *so =  (usn_socket_t *)item;
   return so->so_fd;
}

inline int 
usnet_so_eq(const void *arg1, const void *arg2)
{
   usn_socket_t *item1 = (usn_socket_t *)arg1;
   usn_socket_t *item2 = (usn_socket_t *)arg2;
   return (item1->so_fd == item2->so_fd);
}

inline int
usnet_so_isempty(const void *arg)
{
   usn_socket_t *item = (usn_socket_t *)arg;
   return (item->so_fd == 0);
}

inline int
usnet_so_setempty(const void *arg)
{
   usn_socket_t *item = (usn_socket_t *)arg;
   item->so_fd = 0;
   return 0;
}

void
usnet_socket_init(usn_context_t *ctx, int create)
{

   ctx->socket_cache = (usn_hashbase_t*)malloc(sizeof(usn_hashbase_t));
   if ( ctx->socket_cache == NULL )
      return;

   usnet_cache_init(ctx->socket_cache, 0x33338, 10, 100, sizeof(usn_socket_t),create,
                    usnet_so_eq, usnet_so_key, usnet_so_isempty, usnet_so_setempty);
   return;
}

inline usn_socket_t*
usnet_get_socket(usn_context_t *ctx, usn_socket_t *key)
{
   usn_socket_t *so;

   so = CACHE_GET(ctx->socket_cache, &key, usn_socket_t*);
   
   if ( so == 0 )
      return 0;

   memset(so, 0, sizeof(usn_socket_t));
   so->so_fd = so->so_fd;

   return so;
}

inline usn_socket_t*
usnet_search_socket(usn_context_t *ctx, usn_socket_t *sitem)
{
   return (usn_socket_t*)usnet_cache_search(ctx->socket_cache, sitem);
}

inline usn_socket_t*
usnet_insert_socket(usn_context_t *ctx, usn_socket_t *sitem)
{
   return (usn_socket_t*)usnet_cache_insert(ctx->socket_cache, sitem);
}

inline int
usnet_remove_socket(usn_context_t *ctx, usn_socket_t *sitem)
{
   return usnet_cache_remove(ctx->socket_cache, sitem);
}

int
usnet_send_listen_event(usn_context_t *ctx, uint32_t fd, uint32_t addr, uint16_t port)
{
   static u_char data[MAX_BUFFER_SIZE];
   struct usn_listen_ev ev;
   struct usn_listen_ev *resp;
   int len = 0;
   uint32_t flow = 0;

   memset(&ev, 0, sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_LISTEN_EV;
   ev.h.length = sizeof(ev);
   ev.fd = fd;
   ev.addr = addr;
   ev.port = port;

#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_app2net_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_app2net_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif

   usnet_shmmq_dequeue_wait(ctx->ev_net2app_mq, data, MAX_BUFFER_SIZE, &len, &flow);
   if ( len < sizeof(*resp) )
      return -8;

   resp = (struct usn_listen_ev*)data;

   return resp->result;
}



