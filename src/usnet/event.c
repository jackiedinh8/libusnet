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
 * @(#)event.c
 */

#include "event.h"
#include "log.h"
#include "core.h"
#include "socket.h"
#include "ringbuf.h"
#include "sndbuf.h"
#include "utils.h"
#include "cache.h"
#include "tcp_out.h"

char *type_str[] = { 
   "EMPTY",
   "TCP",
   "UDP"
};  
char *event_str[] = { 
   "EMPTY",
   "READ",
   "WRITE",
   "ACCEPT",
   "TCPSTATE",
   "ERROR",
   "LISTEN",
   "CONNECT"
}; 


int
usnet_tcpev_init(usn_context_t *ctx)
{
   int ret = 0;
   ctx->ev_net2app_mq = (usn_shmmq_t *)malloc(sizeof(*ctx->ev_net2app_mq));

   if ( ctx->ev_net2app_mq == NULL ) { 
      DEBUG(ctx->log,"malloc failed");
      return -1; 
   }   
   ret = usnet_init_shmmq(ctx->ev_net2app_mq,
         "/tmp/tcpev_net2app_mq.fifo", 0, 0, 1157647512, 33554432, 0);
   
   if ( ret < 0 ) {
      DEBUG(ctx->log,"failed to init shmmq, ret=%d",ret);
      return -1;
   }

   ctx->ev_app2net_mq = (usn_shmmq_t *)malloc(sizeof(*ctx->ev_app2net_mq));

   if ( ctx->ev_app2net_mq == NULL ) { 
      DEBUG(ctx->log,"malloc failed\n");
      return -1; 
   }   
   ret = usnet_init_shmmq(ctx->ev_app2net_mq,
         "/tmp/tcpev_app2net_mq.fifo", 0, 0, 1157647513, 33554432, 0);

   if ( ret < 0 ) {
      DEBUG(ctx->log,"failed to init shmmq, ret=%d",ret);
      return -2;
   }

   return 0;
}
/*
int
usnet_tcpev_a2n_enqueue(uint32_t fd, char *body, int len)
{
#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(g_tcpev_app2net_mq, time(0), body, len, fd); 
#else
   usnet_shmmq_enqueue(g_tcpev_app2net_mq, 0, body, len, fd); 
#endif
   return 0;
}

int
usnet_tcpev_a2n_dequeue(char *buf, uint32_t buf_size, uint32_t *data_len)
{
   uint32_t flow = 0;
   usnet_shmmq_dequeue(g_tcpev_app2net_mq, buf, buf_size, data_len, &flow); 
   return 0;
}


int
usnet_tcpev_n2a_enqueue(uint32_t fd, char *body, int len)
{
#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(g_tcpev_net2app_mq, time(0), body, len, fd); 
#else
   usnet_shmmq_enqueue(g_tcpev_net2app_mq, 0, body, len, fd); 
#endif
   return 0;
}

int
usnet_tcpev_n2a_dequeue(char *buf, uint32_t buf_size, uint32_t *data_len)
{
   uint32_t flow = 0;
  
   usnet_shmmq_dequeue(g_tcpev_net2app_mq, buf, buf_size, data_len, &flow); 

   return 0;
}
*/

void
usnet_raise_read_event(usn_context_t *ctx, usn_tcb_t *tcb)
{
   struct usn_read_ev ev;

   memset(&ev, 0, sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_READ_EV;
   ev.h.length = sizeof(ev);
   ev.fd = tcb->fd;

   //usnet_tcpev_n2a_enqueue(ev.fd, (char*)&ev, sizeof(ev));
#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif
/*
   if (stream->socket) {
      if (stream->socket->epoll & MTCP_EPOLLIN) {
         AddEpollEvent(mtcp->ep, 
               MTCP_EVENT_QUEUE, stream->socket, MTCP_EPOLLIN);
#if BLOCKING_SUPPORT
      } else if (!(stream->socket->opts & MTCP_NONBLOCK)) {
         if (!stream->on_rcv_br_list) {
            stream->on_rcv_br_list = TRUE;
            TAILQ_INSERT_TAIL(&mtcp->rcv_br_list, stream, rcvvar->rcv_br_link);
            mtcp->rcv_br_list_cnt++;
         }
#endif
      }  
   } else {
      TRACE_EPOLL("Stream %d: Raising read without a socket!\n", stream->id);
   }
*/
}

void 
usnet_raise_write_event(usn_context_t *ctx, usn_tcb_t *tcb)
{
   struct usn_write_ev ev;

   memset(&ev, 0, sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_WRITE_EV;
   ev.h.length = sizeof(ev);
   ev.fd = tcb->fd;

   //usnet_tcpev_n2a_enqueue(ev.fd, (char*)&ev, sizeof(ev));
#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif

/*
   if (tcb->socket) {
      if (tcb->socket->epoll & MTCP_EPOLLOUT) {
         AddEpollEvent(mtcp->ep, 
               MTCP_EVENT_QUEUE, stream->socket, MTCP_EPOLLOUT);
#if BLOCKING_SUPPORT
      } else if (!(stream->socket->opts & MTCP_NONBLOCK)) {
         if (!stream->on_snd_br_list) {
            stream->on_snd_br_list = TRUE;
            TAILQ_INSERT_TAIL(&mtcp->snd_br_list, stream, sndvar->snd_br_link);
            mtcp->snd_br_list_cnt++;
         }  
#endif      
      }  
   } else {
      TRACE_EPOLL("Stream %d: Raising write without a socket!\n", stream->id);
   }        
*/
}


void 
usnet_raise_error_event(usn_context_t *ctx, usn_tcb_t *tcb)
{        
   struct usn_error_ev ev;

   memset(&ev, 0, sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_ERROR_EV;
   ev.h.length = sizeof(ev);
   ev.fd = tcb->fd;

   //usnet_tcpev_n2a_enqueue(ev.fd, (char*)&ev, sizeof(ev));
#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif

/*
   if (stream->socket) {
      if (stream->socket->epoll & MTCP_EPOLLERR) {
         AddEpollEvent(mtcp->ep, 
               MTCP_EVENT_QUEUE, stream->socket, MTCP_EPOLLERR);
#if BLOCKING_SUPPORT
      } else if (!(stream->socket->opts & MTCP_NONBLOCK)) {
         if (!stream->on_rcv_br_list) {
            stream->on_rcv_br_list = TRUE;
            TAILQ_INSERT_TAIL(&mtcp->rcv_br_list, stream, rcvvar->rcv_br_link);
            mtcp->rcv_br_list_cnt++;
         }  
         if (!stream->on_snd_br_list) {
            stream->on_snd_br_list = TRUE;
            TAILQ_INSERT_TAIL(&mtcp->snd_br_list, stream, sndvar->snd_br_link);
            mtcp->snd_br_list_cnt++;
         }  
#endif
      } 
   } else {
      TRACE_EPOLL("Stream %d: Raising error without a socket!\n", stream->id);
   }
*/
}

void    
usnet_raise_accept_event(usn_context_t *ctx, usn_tcb_t* tcb)
{  
   struct usn_accept_ev ev;
   
   memset(&ev, 0, sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_ACCEPT_EV;
   ev.h.length = sizeof(ev);
   ev.fd = tcb->fd;

#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif

}

void    
usnet_raise_connect_event(usn_context_t *ctx, usn_tcb_t* tcb)
{  
   struct usn_connect_ev ev;
   
   memset(&ev, 0, sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_CONNECT_EV;
   ev.h.length = sizeof(ev);
   ev.fd = tcb->fd;
   ev.saddr = tcb->saddr;
   ev.sport = tcb->sport;
   ev.daddr = tcb->daddr;
   ev.dport = tcb->dport;

#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif

}

void    
usnet_raise_tcp_event(usn_context_t *ctx, usn_tcb_t* tcb)
{  
   struct usn_tcpst_ev ev;
   
   memset(&ev, 0, sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_TCPST_EV;
   ev.h.length = sizeof(ev);
   ev.fd = tcb->fd;
   ev.state = tcb->state;

#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif

}

void
usnet_write_net_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
{
   struct usn_read_ev *ev;
   usn_socket_t t;
   usn_socket_t *so;

   usn_sendbuf_t s;
   usn_sendbuf_t *buf;

   if ( len < sizeof(*ev) )
      return;

   ev = (struct usn_read_ev*)data;
   s.fd = ev->fd;
   buf = CACHE_SEARCH(ctx->sndbuf_cache, &s,usn_sendbuf_t*);

   if ( buf == 0 )
      return;

   DEBUG(ctx->log,"found send buffer, fd=%u, head=%d, tail=%d", 
         ev->fd, buf->head, buf->tail);

   if ( buf->head == buf->tail ) {
      DEBUG(ctx->log,"empty buffer");
      return;
   } 

   t.so_fd = ev->fd;
   so = CACHE_SEARCH(ctx->socket_cache,&t,usn_socket_t*);
   if ( so && so->so_tcb ) {
      usnet_add_send_list(ctx,so->so_tcb);
      return;
   }
   DEBUG(ctx->log,"no found tcb, fd=%u",ev->fd);
}

void
usnet_listen_net_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
{
   struct usn_listen_ev *ev;
   usn_socket_t t;
   usn_socket_t *so;
   int ret = 0;


   if ( len < sizeof(*ev) )
      return;

   ev = (struct usn_listen_ev*)data;

   t.so_fd = ev->fd;
   so = CACHE_SEARCH(ctx->socket_cache,&t,usn_socket_t*);
   if ( so == 0 ) {
      ERROR(ctx->log,"not found socket, fd=%u",ev->fd);
      ev->result = -1;
      goto finish;
   }

   // register listen socket.
   ret = usnet_register_socket(ctx,so,ev->addr,ev->port,0,0);

   if ( ret < 0 ) {
      ERROR(ctx->log,"registering socket failed, fd=%u",ev->fd);
      ev->result = -2;
      goto finish;
   }
   ev->result = 0;

   ERROR(ctx->log,"registering socket, fd=%u, ret=%d",ev->fd, ret);

finish:
   usnet_shmmq_enqueue(ctx->ev_net2app_mq, time(0), ev, sizeof(*ev), flow);
   return;
}

void
usnet_connect_net_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
{
   struct usn_connect_ev *ev;
   usn_socket_t  s;
   usn_socket_t *so;
   usn_tcb_t     t;
   usn_tcb_t    *tcb;

   if ( len < sizeof(*ev) )
      return;

   ev = (struct usn_connect_ev*)data;

   s.so_fd = ev->fd;
   so = CACHE_SEARCH(ctx->socket_cache,&s,usn_socket_t*);
   if ( so == 0 ) {
      ERROR(ctx->log,"not found socket, fd=%u",ev->fd);
      goto reject;
   }

   t.saddr = ev->saddr;
   t.sport = ev->sport;
   t.daddr = ev->daddr;
   t.dport = ev->dport;
   tcb = CACHE_SEARCH(ctx->tcb_cache,&t,usn_tcb_t*);

   if ( tcb == 0 ) {
      ERROR(ctx->log,"not found tcb, fd=%u,saddr=%x,sport=%u,daddr=%x,dport=%u",
         ev->fd, ev->saddr,ev->sport,ev->daddr,ev->dport);
      goto reject;
   }

   if ( ev->result < 0 )
      goto destroy;

   tcb->socket = so;
   so->so_tcb = tcb;
   tcb->fd = so->so_fd;

   // allocate send buffer.
   if (!tcb->sndvar->sndbuf) {
      DEBUG(ctx->log,"allocate send buff, fd=%u",tcb->fd);
      tcb->sndvar->sndbuf = usnet_get_sendbuf(ctx, tcb, tcb->sndvar->iss + 1);
   }

   if (!tcb->sndvar->sndbuf) {
      ERROR(ctx->log,"Stream %d: Failed to allocate send buffer", tcb->fd);
      goto destroy;
   }

   usnet_add_control_list(ctx,tcb);
   return;
destroy:
   // TODO: clean stuff.
reject:
   // send reset packet.
   ERROR(ctx->log,"reject connection, fd=%u, %x:%u -> %x:%u", ev->fd,
         ev->daddr, ev->dport, ev->saddr, ev->sport);
   //usnet_send_tcp_packet_alone();
   return;
}

void
usnet_tcpev_net_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
{
   struct header *h;

   if ( len < sizeof(*h) )
      return;
   h = (struct header*)data;

   DEBUG(ctx->log, "ev: type=%s, event=%s", type_str[h->type], event_str[h->event]);
   if ( h->type != TCP_TYPE )
      return;

   switch(h->event) {
      case USN_WRITE_EV:
         usnet_write_net_handler(ctx, data, len, flow);
         break;
      case USN_LISTEN_EV:
         usnet_listen_net_handler(ctx, data, len, flow);
         break;
      case USN_CONNECT_EV:
         usnet_connect_net_handler(ctx, data, len, flow);
         break;
      default:
         break;
   }
}

void
usnet_tcpev_net(usn_context_t *ctx)
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
      usnet_shmmq_dequeue(ctx->ev_app2net_mq, data, MAX_BUFFER_SIZE, &len, &flow);
      if ( len == 0 ) { 
         DEBUG(ctx->log,"empty queue: lastFactor=%d, newFactor=%d, cnt=%d",
                 ctx->ev_net2app_mq->_adaptive_ctrl->m_uiLastFactor, 
                 ctx->ev_net2app_mq->_adaptive_ctrl->m_uiFactor, cnt);
         return;
      }   
      usnet_tcpev_net_handler(ctx, data, len, flow);
  }
  return;
}


/* The following code is used by worker processes. */

void
usnet_read_proc_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
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
usnet_tcpst_proc_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
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
usnet_tcpev_proc_handler(usn_context_t *ctx, char *data, int32_t len, uint32_t flow)
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
         usnet_read_proc_handler(ctx, data, len, flow);
         break;
      case USN_TCPST_EV:
         usnet_tcpst_proc_handler(ctx, data, len, flow);
         break;
      default:
         break;
   }
}

void
usnet_tcpev_process(usn_context_t *ctx)
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
         return;
      }   
      usnet_tcpev_proc_handler(ctx, data, len, flow);
  }
  return;
}

void
usnet_write_notify(usn_context_t *ctx, uint32_t fd)
{
   struct usn_write_ev ev;

   memset(&ev, 0, sizeof(ev));
   ev.h.type = TCP_TYPE;
   ev.h.event = USN_WRITE_EV;
   ev.h.length = sizeof(ev);
   ev.fd = fd;

#ifdef USE_ADAPTIVE_CONTROL
   usnet_shmmq_enqueue(ctx->ev_app2net_mq, time(0), &ev, sizeof(ev), ev.fd); 
#else
   usnet_shmmq_enqueue(ctx->ev_app2net_mq, 0, &ev, sizeof(ev), ev.fd); 
#endif

}

int
usnet_write_new(usn_context_t *ctx, uint32_t fd, char* data, int len) 
{
   usn_sendbuf_t s;
   usn_sendbuf_t *sndbuf;
   
   DEBUG(ctx->log,"write data, len=%d, fd=%u",len,fd);
   dump_buffer(ctx,data,len,"app");

   s.fd = fd;
   sndbuf = CACHE_SEARCH(ctx->sndbuf_cache, &s, usn_sendbuf_t*);

   if (sndbuf==0)
      return -1;

   DEBUG(ctx->log, "found send buffer, fd=%d",fd);
   usnet_write_data(ctx, sndbuf, data, len);

   usnet_write_notify(ctx,fd);
   
   return 0;
}





