/* 
 * File:   queue_stats_view.cpp
 * Author: dungdv
 *
 * Created on June 07, 2013, 9:46 AM
 */

#ifndef _USNET_VIEWMQ_H_
#define _USNET_VIEWMQ_H_

#include "usnet_mq.h"

typedef struct usn_viewmq usn_viewmq_t;
struct usn_viewmq {
   usn_shmmq_t *mq;
   unsigned _used_len; 
   unsigned _free_len; 
   unsigned _total_len; 
   unsigned _enqueued_msg_cnt;
   unsigned _dequeued_msg_cnt;
};

void 
usnet_viewmq_stats(usn_viewmq_t *view);

#endif // _USNET_VIEWMQ_H_

