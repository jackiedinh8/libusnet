/*
 * Copyright (c) 2014 Jackie Dinh <jackiedinh8@gmail.com>
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
 * @(#)usnet_socket.h
 */

#ifndef _USNET_SOCKET_H_
#define _USNET_SOCKET_H_

#include <stdlib.h>
#include <stdint.h>

#include "usnet_socket_util.h"
#include "usnet_buf.h"
#include "usnet_core.h"
#include "usnet_protosw.h"

#define MAX_SOCKETS 1024
extern struct usn_socket* g_fds[MAX_SOCKETS];

extern u_long g_udp_sendspace;
extern u_long g_udp_recvspace;
extern u_long g_sb_max;
extern int    g_fds_idx;
extern struct usn_socket* g_fds[MAX_SOCKETS];

void 
usnet_socket_init();

int32
usnet_create_socket(u_int32 dom, struct usn_socket **aso, 
      u_int32 type, u_int32 proto);

int32
usnet_bind_socket(u_int32 fd, u_int32 addr, u_short port);

int32
usnet_listen_socket(u_int32 fd, int32 flags, 
      accept_handler_cb accept_cb, 
      error_handler_cb error_cb, void* arg);

int32
usnet_soclose(u_int32 fd);

struct inpcb;

int32
usnet_tcpaccept_socket(struct usn_socket *so, struct sockbuf *sb);

int32
usnet_tcpwakeup_socket(struct usn_socket *so, struct sockbuf *sb);

int32
usnet_udpwakeup_socket(struct inpcb* pcb);

int32
usnet_ewakeup_socket(struct usn_socket *so, struct sockbuf *sb);

// @return: 
//   >= 0: length of available buffer.
//    < 0: error code.
int32
usnet_read_socket(u_int fd, u_char *buf, u_int len);


usn_buf_t*
usnet_get_sobuffer(u_int32 fd);

int32
usnet_write_sobuffer(u_int32 fd, usn_buf_t *buf);

int32
usnet_writeto_sobuffer(u_int32 fd, usn_buf_t *buf, 
      struct usn_sockaddr_in *addr);

int32
usnet_drain_sobuffer(u_int32 fd);

int32
usnet_udp_sobroadcast(u_int32 fd, u_char* buff, u_int32 len, 
       struct usn_sockaddr_in* addrs, u_int32 addr_num);

int32
usnet_set_socketcb(u_int32 fd, int32 flags, 
      read_handler_cb read_cb, 
      write_handler_cb write_cb, 
      error_handler_cb error_cb, void* arg);

#endif /* USNET_SOCKET_H_ */
