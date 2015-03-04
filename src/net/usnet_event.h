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
 * @(#)usnet_event.h
 */

#ifndef _USNET_EVENT_H_
#define _USNET_EVENT_H_

#include "usnet_core.h"
#include "usnet_mq.h"

extern usn_shmmq_t *g_tcpev_net2app_mq;
extern usn_shmmq_t *g_tcpev_app2net_mq;

int32
usnet_tcpev_init();

int32
usnet_tcpev_a2n_enqueue(u_int32 fd, u_char type, u_char event, u_char *body, int32 len);

int32
usnet_tcpev_a2n_dequeue(u_char *buf, u_int32 buf_size, u_int32 *data_len);

int32
usnet_tcpev_n2a_enqueue(u_int32 fd, u_char type, u_char event, u_char *body, int32 len);

int32
usnet_tcpev_n2a_dequeue(u_char *buf, u_int32 buf_size, u_int32 *data_len);


#endif  //!_USNET_API_H_
