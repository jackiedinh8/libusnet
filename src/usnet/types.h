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
 * @(#)types.h
 */

#ifndef _USNET_TYPES_H_
#define _USNET_TYPES_H_

typedef struct usn_context usn_context_t;
typedef struct usn_ifnet usn_ifnet_t;
typedef struct usn_tcb usn_tcb_t;
typedef struct usn_iphdr usn_iphdr_t;
typedef struct usn_nmring usn_nmring_t;
typedef struct usn_socket usn_socket_t;
typedef struct usn_udphdr usn_udphdr_t;
typedef struct usn_log usn_log_t;
typedef struct usn_arp_table usn_arp_table_t;
typedef struct usn_arp_entry usn_arp_entry_t;
typedef struct usn_config usn_config_t;
typedef struct usn_sendbuf usn_sendbuf_t;
typedef struct usn_ringbuf usn_ringbuf_t;
typedef struct usn_rcvvars usn_rcvvars_t;
typedef struct usn_mempool usn_mempool_t;
typedef union usn_epoll_data usn_epoll_data_t;
typedef struct usn_epoll_event usn_epoll_event_t;
typedef struct usn_epoll_fd usn_epoll_fd_t;


#define MAX_BUFFER_SIZE 4*1024
#define ETH_LEN 6

#endif //_USNET_TYPES_H_
