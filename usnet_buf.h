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
 * @(#)usnet_buf.h
 */


#ifndef _USNET_BUF_H_
#define _USNET_BUF_H_

#include "usnet_types.h"
//#include "usnet_core.h"

#define  BUF_MSIZE    128 

typedef struct usn_mbuf usn_mbuf_t;
struct usn_mbuf {
   u_char          *head;  // point to the first byte of unprocessed portion.
   u_int32          mlen;
   usn_mbuf_t      *next;
   usn_mbuf_t      *prev;
   usn_mbuf_t      *queue; // to magage a queue of mbufs.
   u_char          *start; // start address of the buffer.
   u_char          *end;   // end address of the buffer.
   u_char          *tail;  // point to the last byte of unprocessed portion.
   u_short          type;  // XXX: internal and external buffer, currently not used.
   u_short          flags;
   u_short          refs;
#define	BUF_ETHERHDR             0x0001
#define	BUF_IPHDR                0x0002
#define	BUF_IPDATA               0x0004
#define	BUF_MODIFIED_IPHDR       0x0008
#define	BUF_IP_MF                0x0010
#define	BUF_DATA                 0x0020
#define	BUF_CTLMSG               0x0040
#define	BUF_ADDR                 0x0080
#define	BUF_BCAST                0x0100	/* send/received as link-level broadcast */
#define	BUF_MCAST 	             0x0200	/* send/received as link-level multicast */
#define	BUF_RAW  	             0x0400	/* raw packet */
#define	BUF_ZERO_COPY            0x1000	
}__attribute__((packed)); // XXX: check performance or need to padding

// this buffer overlays struct usn_mbuf above for application usage.
typedef struct usn_buf usn_buf_t;
struct usn_buf {
   u_char     *data;
   u_int32     len;
   usn_buf_t  *next;
   usn_buf_t  *prev;
   usn_buf_t  *queue;
}__attribute__((packed)); // XXX: check performance or need to padding

struct   ifqueue {
   usn_mbuf_t *ifq_head;
   usn_mbuf_t *ifq_tail;
   int         ifq_len;
   int         ifq_maxlen;
   int         ifq_drops;
};


u_char* 
usn_get_buf(u_char* buf, int size);

void
usn_free_buf(u_char* buf);

usn_mbuf_t* 
usn_get_mbuf(u_char* buf, int size, u_short flags);

usn_mbuf_t* 
usn_get_mbuf_zc(u_char* buf, int size, u_short flags);

void
usn_free_mbuf(usn_mbuf_t *m);

void
usn_free_mbuf_chain(usn_mbuf_t *m);

void* 
usn_create_pool(int size);

usn_mbuf_t* 
usn_copy_data(usn_mbuf_t *m, int off0, int len);

void
usn_copy_mbuf(usn_mbuf_t *m, int32 off, int32 len, caddr_t cp);

#define MBUF_GET(size) usn_malloc((size))
#define  mtod(m,t)   ((t)((m)->head))

#define BUF_PREPEND(m,size) \
           if (((m)->head - (m)->start ) >= (u_int)size ){\
              (m)->head -= size;\
              (m)->mlen += size;\
           } else {\
              usn_free_mbuf(m);\
              m = NULL;\
           }

void
m_adj( usn_mbuf_t *mp, int req_len);

usn_mbuf_t *
m_pullup(usn_mbuf_t *mp, int len);

#endif //!_USNET_BUF_H_
