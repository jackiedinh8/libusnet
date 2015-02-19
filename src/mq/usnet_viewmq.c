/* 
 * File:   queue_stats_view.cpp
 * Author: dungdv
 *
 * Created on June 07, 2013, 9:46 AM
 */

#include <stdio.h>

#include "usnet_viewmq.h"

void usnet_viewmq_stats(usn_viewmq_t *view)
{
   usn_shmmq_t *mq = view->mq;
   unsigned head;// = *mq->_head;
   unsigned tail;// = *mq->_tail;
   unsigned used_len;// = (tail>=head) ? tail-head : tail+mq->_block_size-head;
   unsigned free_len;// = head>tail? head-tail: head+mq->_block_size-tail;
   unsigned total_len;// = mq->_block_size;

   head = *mq->_head;
   tail = *mq->_tail;
   used_len = (tail>=head) ? tail-head : tail+mq->_block_size-head;
   free_len = head>tail? head-tail: head+mq->_block_size-tail;
   total_len = mq->_block_size;

   printf("Usage=%1.2f, used_len=%10d, free_len=%10d, in_cnt=%10d, in_rate=%10dmsgs/sec out_cnt=%10d, out_rate=%10dmsgs/sec \n",
              (float)used_len/total_len ,used_len, free_len, 
              *mq->_enqueued_msg_cnt, *mq->_enqueued_msg_cnt - view->_enqueued_msg_cnt, 
              *mq->_dequeued_msg_cnt, *mq->_dequeued_msg_cnt - view->_dequeued_msg_cnt);

   view->_used_len= used_len;
   view->_free_len= free_len;
   view->_total_len= total_len;
   view->_enqueued_msg_cnt= *mq->_enqueued_msg_cnt;
   view->_dequeued_msg_cnt= *mq->_dequeued_msg_cnt;
  
};



