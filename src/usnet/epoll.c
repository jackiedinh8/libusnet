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
 * @(#)epoll.c
 */


#include <poll.h>

#include "epoll.h"
#include "core.h"
#include "log.h"
#include "event.h"
#include "ringbuf.h"
#include "utils.h"


int
usnet_epoll_create(usn_context_t *ctx, int size)
{
   return 0;
}

void
usnet_epoll_read_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
{
   struct usn_read_ev *ev;
   usn_ringbuf_t s;
   usn_ringbuf_t *buf;
   char *data_ptr = 0;
   int data_len;

   if ( len < sizeof(*ev) )
      return;

   ev = (struct usn_read_ev*)data;
   s.fd = ev->fd;
   buf = CACHE_SEARCH(ctx->ringbuf_cache, &s,usn_ringbuf_t*);

   if ( buf == 0 )
      return;

   DEBUG(ctx->log,"found recevied buffer, fd=%u, data=%d, head=%d", 
         ev->fd, buf->data, buf->head);

   data_ptr = (char*)(buf+1); 
   if ( buf->head == buf->data ) {
      DEBUG(ctx->log,"empty buffer");
      return;
   } else if ( buf->head > buf->data ) {
      data_len = buf->head-buf->data;
      DEBUG(ctx->log,"data_len=%d",data_len);
      dump_buffer(ctx,data_ptr+buf->data, data_len,"tcp");
      usnet_write_new(ctx, ev->fd, data_ptr+buf->data, data_len);

   } else {
      int tail_len = buf->size - buf->data;
      //dump_buffer(ctx,data_ptr+buf->data, tail_len,"tcp");
      //dump_buffer(ctx,data_ptr, buf->head,"tcp");
      usnet_write_new(ctx, ev->fd, data_ptr+buf->data, tail_len);
      usnet_write_new(ctx, ev->fd, data_ptr, buf->head);

   }
   buf->data = buf->head;

}

void
usnet_epoll_tcpst_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
{
   struct usn_tcpst_ev *ev;
   //usn_socket_t t;
   //usn_socket_t *so;

   if ( len < sizeof(*ev) ) {
      DEBUG(ctx->log, "buf is too small, len=%d, need_len=%d",len,sizeof(*ev));
      return;
   }

   ev = (struct usn_tcpst_ev*)data;

   DEBUG(ctx->log,"new state: fd=%d, state=%s", ev->fd, usnet_tcp_statestr_new(ev->state));
   // TODO: invoke user callbacks
}

void
usnet_epoll_proc_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
{
   struct header *h;

   if ( len < sizeof(*h) )
      return;
   h = (struct header*)data;

   DEBUG(ctx->log, "ev: type=%s, event=%s", type_str[h->type], event_str[h->event]);
   if ( h->type != TCP_TYPE )
      return;

   switch(h->event) {
      case USN_READ_EV:
         usnet_epoll_read_handler(ctx, data, len, flow);
         break;
      case USN_TCPST_EV:
         usnet_epoll_tcpst_handler(ctx, data, len, flow);
         break;
      default:
         break;
   }
}


int
usnet_epoll_process(usn_context_t *ctx)
{
   static char data[MAX_BUFFER_SIZE];
   int32_t len = 0;
   uint32_t flow = 0;
   uint32_t cnt = 0;
   while(1){
      len = 0;
      flow = 0;
      cnt++;
      if ( cnt % 10000 == 0 ) 
      {   
          DEBUG(ctx->log,"breaking the loop, cnt=%d", cnt);
          break;
      }   
      usnet_shmmq_dequeue(ctx->ev_net2app_mq, data, MAX_BUFFER_SIZE, &len, &flow);
      if ( len == 0 ) { 
         DEBUG(ctx->log,"empty queue: lastFactor=%d, newFactor=%d, cnt=%d",
                 ctx->ev_net2app_mq->_adaptive_ctrl->m_uiLastFactor, 
                 ctx->ev_net2app_mq->_adaptive_ctrl->m_uiFactor, cnt);
         return cnt;
      }   
      usnet_epoll_proc_handler(ctx, data, len, flow);
   }
   return cnt;
}

int
usnet_ep_eq(const void* arg1, const  void* arg2)
{
   usn_epoll_fd_t *item1 = (usn_epoll_fd_t *)arg1;
   usn_epoll_fd_t *item2 = (usn_epoll_fd_t *)arg2;
   return (item1->fd == item2->fd);
   return 0;
}

int
usnet_ep_key(const void* arg)
{
   usn_epoll_fd_t *item =  (usn_epoll_fd_t *)arg;
   return item->fd;
}

int
usnet_ep_isempty(const void* arg)
{
   usn_epoll_fd_t *item = (usn_epoll_fd_t *)arg;
   return (item->fd == 0);
}

int
usnet_ep_setempty(const void* arg)
{
   usn_epoll_fd_t *item = (usn_epoll_fd_t *)arg;
   item->fd = 0;
   return 0;
}

void
usnet_epoll_init(usn_context_t *ctx)
{
   usnet_cache_init(ctx->epoll_cache, 0x33344, 10, 100, 
                    sizeof(usn_epoll_fd_t),1,
                    usnet_ep_eq, usnet_ep_key, usnet_ep_isempty, usnet_ep_setempty);
   return;
}

inline int
usnet_epoll_watch(usn_context_t *ctx, int epid, int fd) 
{
   usn_epoll_fd_t *efd;
   usn_epoll_fd_t t;

   t.fd = fd;
   efd = CACHE_SEARCH(ctx->epoll_cache,&t,usn_epoll_fd_t*);

   return (efd != 0);
}

int
usnet_epoll_wait(usn_context_t *ctx, int epid, 
            struct usn_epoll_event *events, int maxevents, int timeout)
{
   static char    data[MAX_BUFFER_SIZE];
   struct pollfd  fds[1];
   struct header *h;
   int            ret = 0;;
   int32_t len = 0;
   uint32_t flow = 0;
   uint32_t cnt = 0;
   int evnum = 0;

   fds[0].fd = ctx->ev_net2app_mq->_fd;

   DEBUG(ctx->log,"epoll wait for %d(ms) ...", timeout);

   fds[0].events = POLLIN;
   fds[0].revents = 0;
   ret = poll(fds, 1, timeout);
   if (ret <= 0 ) { 
      DEBUG(ctx->log, "poll %s ev %x %x, ret=%d", ret <= 0 ? "timeout" : "ok",
        fds[0].events, fds[0].revents, ret);
      return 0;
   }   
   // tcpev jobs from message queue
   if (fds[0].revents & POLLERR) {
      ERROR(ctx->log,"error on message queue");
   }   
   if (fds[0].revents & POLLIN) {
      //ret = usnet_epoll_process(ctx);
      while(1){
         len = 0;
         flow = 0;
         cnt++;
         if ( cnt % 10000 == 0 ) 
         {   
             DEBUG(ctx->log,"breaking the loop, cnt=%d", cnt);
             break;
         }   

         if ( evnum >= maxevents )
            return evnum;

         usnet_shmmq_dequeue(ctx->ev_net2app_mq, data, MAX_BUFFER_SIZE, &len, &flow);
         if ( len == 0 ) { 
            DEBUG(ctx->log,"empty queue: lastFactor=%d, newFactor=%d, cnt=%d",
                    ctx->ev_net2app_mq->_adaptive_ctrl->m_uiLastFactor, 
                    ctx->ev_net2app_mq->_adaptive_ctrl->m_uiFactor, cnt);
            return evnum;
         }   

         if ( len < sizeof(*h) )
            continue;
         h = (struct header*)data;

         DEBUG(ctx->log, "ev: type=%s, event=%s", type_str[h->type], event_str[h->event]);
         if ( h->type != TCP_TYPE )
            continue;

         switch(h->event) {
            case USN_READ_EV:
               {
                  //usnet_epoll_read_handler(ctx, data, len, flow);
                  struct usn_read_ev *ev;

                  if ( len < sizeof(*ev) )
                     break;
                  ev = (struct usn_read_ev*)data;
                  if ( !usnet_epoll_watch(ctx,epid,ev->fd) )
                     break;
                  events[evnum].events = USN_EPOLLIN;
                  events[evnum].data.fd = ev->fd;
                  ++evnum;

                  DEBUG(ctx->log,"read ev, fd=%u, evnum=%d",ev->fd,evnum);
                  break;
               }

            case USN_WRITE_EV:
               {
                  struct usn_write_ev *ev;

                  if ( len < sizeof(*ev) )
                     break;

                  ev = (struct usn_write_ev*)data;
                  events[evnum].events = USN_EPOLLOUT;
                  events[evnum].data.fd = ev->fd;
                  ++evnum;

                  DEBUG(ctx->log,"write ev, fd=%u, evnum=%d",ev->fd,evnum);
                  break;
               }

            case USN_CONNECT_EV:
               {
                  uint32_t new_fd;
                  struct usn_connect_ev *ev;

                  (void)new_fd;

                  if ( len < sizeof(*ev) )
                     break;

                  ev = (struct usn_connect_ev*)data;
                  events[evnum].events = USN_EPOLLIN;
                  events[evnum].data.fd = ev->fd;
                  ++evnum;

                  new_fd = usnet_accept_newconn(ctx,ev->fd,ev->saddr,
                                                  ev->sport,ev->daddr,ev->dport); 
                  DEBUG(ctx->log,"accept ev, fd=%u, new_fd=%u, evnum=%d",
                        ev->fd,new_fd,evnum);
                  break;
               }

            case USN_TCPST_EV:
               {
                  //usnet_epoll_tcpst_handler(ctx, data, len, flow);
                  break;
               }
            default:
               break;
         }

      }

   }   

   return evnum;
}

int
usnet_epoll_ctl(usn_context_t *ctx, int epid, 
            int op, int fd, struct usn_epoll_event *ev)
{
   usn_epoll_fd_t efd;
   usn_epoll_fd_t *t;
   
   switch (op) {
      case USN_EPOLL_CTL_ADD:
         {
            efd.fd = fd;
            efd.epollid = epid;
            memcpy(&efd.ev,ev, sizeof(*ev));

            t = CACHE_INSERT(ctx->epoll_cache,&efd,usn_epoll_fd_t*);
            if ( t == 0 )
               return -1;
            DEBUG(ctx->log,"insert fd into epoll_cache, fd=%u",fd);
            break;
         }
      case USN_EPOLL_CTL_MOD:
         {
            break;
         }
      case USN_EPOLL_CTL_DEL:
         {
            break;
         }
   }

   return 0;
}

int
usnet_epoll_read(usn_context_t *ctx, int fd)
{
   return 0;
}

int
usnet_epoll_write(usn_context_t *ctx, int fd, void* buf, int size)
{
   return 0;
}


