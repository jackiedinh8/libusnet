#ifndef _USNET_BUF_H_
#define _USNET_BUF_H_

#include "usnet_types.h"
//#include "usnet_core.h"

#define  BUF_MSIZE    128 

typedef struct usn_buf usn_buf_t;
struct usn_buf {
   u_char      *start; // start address of the buffer.
   u_char      *end;   // end address of the buffer.
   u_char      *head;  // point to the first byte of unprocessed portion.
   u_char      *tail;  // point to the first byte of available space.
   u_short      type;
//MBUF TYPES
#define BUF_IPHEADER_TYPE 1
#define BUF_TCPDATA_TYPE  2
#define BUF_UDPDATA_TYPE  3

   u_short      flags;
#define USN_IP_HEADER_MAX_LEN   60 // IP header + options
#define USN_ETHER_HEADER_LEN    14
#define USN_IPETHER_HEADER_LEN  74

};

// Link together buffers into a chain, 
// each chain is a complete msg or packet.
// For example, a chain of tcp data buffer

typedef struct usn_chain usn_chain_t;
struct usn_chain {
   usn_buf_t      *buf;   // point to actual buffer.
   usn_chain_t    *next;  // next buffer
};

// Link together buffer chains into a queue.
// A list of complete tcp packets.
typedef struct usn_queue usn_queue_t;
struct usn_queue {
   usn_queue_t    *next;
   usn_chain_t    *chain;
};

typedef struct usn_pool usn_pool_t;
struct usn_pool {
   u_char     *start;
   u_char     *end;
   usn_pool_t *next;
   uint32_t    flags;
};

typedef struct usn_mbuf usn_mbuf_t;
struct usn_mbuf {
   void            *queue;
   usn_mbuf_t      *prev;
   usn_mbuf_t      *next;
   //usn_buf_t        buf;
   u_char          *start; // start address of the buffer.
   u_char          *end;   // end address of the buffer.
   u_char          *head;  // point to the first byte of unprocessed portion.
   u_char          *tail;  // point to the last byte of unprocessed portion.
   u_int            mlen;
   u_short          type;
   u_short          flags;
   u_short          refs;
/* mbuf pkthdr flags, also in m_flags */
#define	BUF_BCAST                0x0100	/* send/received as link-level broadcast */
#define	BUF_MCAST 	             0x0200	/* send/received as link-level multicast */
#define	BUF_ZERO_COPY            0x1000	
#define	BUF_ETHERHDR             0x0001
#define	BUF_IPHDR                0x0002
#define	BUF_IPDATA               0x0004
#define	BUF_MODIFIED_IPHDR       0x0008
#define	BUF_IP_MF                0x0010
#define	BUF_DATA                 0x0020
};

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



void* 
usn_create_pool(int size);

inline usn_mbuf_t* 
mbuf_copy(usn_mbuf_t *m, int off, int len)
{
   // XXX create a new mbuf and copy len of bytes
   // from m.
   return m;
}

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
