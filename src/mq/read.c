#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <event2/event.h>

#include "usnet_shm.h"
#include "usnet_mq.h"

#define BUF_SIZE 40
#define MAX_BUFFER_SIZE 1024*1024

int g_cnt = 0;

void read_mq(int fd, short int event,void* ctx) 
{
   static char data[MAX_BUFFER_SIZE];
   unsigned int len = 0;
   unsigned int conn_id = 0;
   uint32_t cnt = 0;
   usn_shmmq_t *mq = (usn_shmmq_t *) ctx;;

  while(1){
    len = 0;
    conn_id = 0;
    cnt++;
    if ( cnt % 10000 == 0 ) 
    {   
        printf("breaking the loop, cnt=%d\n", cnt);
        break;
    }   
    usnet_shmmq_dequeue(mq, data, MAX_BUFFER_SIZE, &len, &conn_id);
    if ( len == 0 ) {
        //printf("empty queue: lastFactor=%d, newFactor=%d, g_cnt=%d, cnt=%d\n",
        //        mq->_adaptive_ctrl->m_uiLastFactor, 
        //        mq->_adaptive_ctrl->m_uiFactor, g_cnt, cnt);
        return;
    }
    g_cnt++;
    if ( mq->_adaptive_ctrl->m_uiLastFactor != mq->_adaptive_ctrl->m_uiFactor )
        printf("change load factor: lastFactor=%d, newFactor=%d, g_cnt=%d\n",
                mq->_adaptive_ctrl->m_uiLastFactor, 
                mq->_adaptive_ctrl->m_uiFactor, g_cnt);

    //if( g_cnt % 10000 == 0 )
    //    printf("load_algorithm: lastFactor=%d, newFactor=%d, g_cnt=%d, cnt=%d\n",
    //            mq->_adaptive_ctrl->m_uiLastFactor, 
    //            mq->_adaptive_ctrl->m_uiFactor, g_cnt, cnt);
  }
  return;
}
int main(int argc, char **argv) 
{
   usn_shmmq_t *mq;
   struct event *q_event;
   struct event_base *evbase;

   mq = (usn_shmmq_t *)malloc(sizeof(*mq));

   if ( mq == NULL ) {
      printf("malloc failed\n");
      return -1;
   }

   //usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 4194304);
   //usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 67108864);
   usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 33554432);

   print_shmmq(mq);

   evbase = event_base_new();
   q_event = event_new(evbase, mq->_fd, EV_TIMEOUT|EV_READ|EV_PERSIST, read_mq, mq);
   event_add(q_event, NULL);
   event_base_dispatch(evbase);

   return 0;
}
