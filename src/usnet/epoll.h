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
 * @(#)epoll.h
 */

#ifndef _USNET_EPOLL_H_
#define _USNET_EPOLL_H_

#include <stdint.h>

#include "types.h"

enum usn_epoll_op
{  
   USN_EPOLL_CTL_ADD = 1,
   USN_EPOLL_CTL_DEL = 2,
   USN_EPOLL_CTL_MOD = 3,
};

enum usn_epoll_event_type
{
   USN_EPOLLNONE = 0x000,
   USN_EPOLLIN   = 0x001,
   USN_EPOLLPRI  = 0x002,
   USN_EPOLLOUT  = 0x004,
   USN_EPOLLRDNORM  = 0x040,
   USN_EPOLLRDBAND  = 0x080,
   USN_EPOLLWRNORM  = 0x100,
   USN_EPOLLWRBAND  = 0x200,
   USN_EPOLLMSG     = 0x400,
   USN_EPOLLERR     = 0x008,
   USN_EPOLLHUP     = 0x010,
   USN_EPOLLRDHUP   = 0x2000,
   USN_EPOLLONESHOT = (1 << 30),
   USN_EPOLLET      = (1 << 31)
};


union usn_epoll_data {
   void *ptr;
   int   fd;
   uint32_t u32;
   uint64_t u64;
};

struct usn_epoll_event {
   uint32_t events;
   usn_epoll_data_t data;
};

struct usn_epoll_fd {
   int               fd;
   int               epollid;
   usn_epoll_event_t ev;
};

void
usnet_epoll_init(usn_context_t *ctx);

int
usnet_epoll_create(usn_context_t *ctx, int size);

int
usnet_epoll_wait(usn_context_t *ctx, int epid, 
            struct usn_epoll_event *events, int maxevents, int timeout);

int
usnet_epoll_ctl(usn_context_t *ctx, int epid, 
            int op, int fd, struct usn_epoll_event *event);

int
usnet_epoll_read(usn_context_t *ctx, int fd);

int
usnet_epoll_write(usn_context_t *ctx, int fd, void* buf, int size);

#endif  //_USNET_EPOLL_H_

