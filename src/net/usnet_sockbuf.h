#ifndef _USNET_SOCKBUF_H_
#define _USNET_SOCKBUF_H_

#include "usnet_types.h"
#include "usnet_buf.h"
#include "usnet_cache.h"

/*
 * Used to maintain information about processes that wish to be
 * notified when I/O becomes possible.
 */
struct usn_selinfo {
   int   si_pid;     /* process to be notified */
   short si_flags;   /* see below */
};


/*
 * socket buffering.
 */
typedef struct sockbuf sockbuf_t;
struct sockbuf {
   u_int32        sb_fd;        /* file descriptor */
   u_int32        sb_cc;        /* actual chars in buffer */
   u_int32        sb_hiwat;     /* max actual char count */
   u_int32        sb_mbcnt;     /* chars of mbufs used */
   u_int32        sb_mbmax;     /* max chars of mbufs to use */
   u_int32        sb_lowat;     /* low water mark */
   usn_mbuf_t     *sb_mb;       /* the mbuf chain */
   //struct usn_selinfo sb_sel; /* process selecting read/write */
   u_short          sb_flags;   /* flags, see below */
   u_short          sb_timeo;   /* timeout for read/write */
}__attribute__((packed));
#define  SB_MAX      (256*1024)  /* default for max chars in sockbuf */
#define  SB_LOCK     0x01        /* lock on data queue */
#define  SB_WANT     0x02        /* someone is waiting to lock */
#define  SB_WAIT     0x04        /* someone is waiting for data/space */
#define  SB_SEL      0x08        /* someone is selecting */
#define  SB_ASYNC 0x10           /* ASYNC I/O, need signals */
#define  SB_NOTIFY   (SB_WAIT|SB_SEL|SB_ASYNC)
#define  SB_NOINTR   0x40        /* operations not interruptible */


extern usn_hashbase_t g_sorcv_cache;
extern usn_hashbase_t g_sosnd_cache;

void
usnet_sockbuf_init();

sockbuf_t *
usnet_get_sockbuf(usn_hashbase_t *base, int32 fd);

int32
usnet_free_sockbuf(usn_hashbase_t *base, int32 fd);

#endif //_USNET_SOCKBUF_H_

