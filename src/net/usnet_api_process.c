#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "usnet_api_process.h"
#include "usnet_shm.h"
#include "usnet_mq.h"
#include "usnet_msg.h"
#include "usnet_log.h"

#define BUF_SIZE 40
#define MAX_BUFFER_SIZE 1024*1024

int g_event_cnt = 0;

void usnet_tcpev_process_handler(char *buff, int len, int fd)
{
   usn_tcpin_pkg_t pkg;
   
   if ( len < sizeof(pkg.header) ) {
      printf("buffer is too small, len=%d, fd=%d\n", len, fd);
      return;
   }
   memcpy(&pkg.header, buff, sizeof(pkg.header));
  
   usnet_print_header(&pkg.header); 

   return;
}

void usnet_tcpev_process(usn_shmmq_t *mq) 
{
   static char data[MAX_BUFFER_SIZE];
   unsigned int len = 0;
   unsigned int conn_id = 0;
   uint32_t cnt = 0;
   while(1){
      len = 0;
      conn_id = 0;
      cnt++;
      if ( cnt % 10000 == 0 ) 
      {   
          DEBUG("breaking the loop, cnt=%d", cnt);
          break;
      }   
      usnet_shmmq_dequeue(mq, data, MAX_BUFFER_SIZE, &len, &conn_id);
      if ( len == 0 ) {
         //printf("empty queue: lastFactor=%d, newFactor=%d, g_event_cnt=%d, cnt=%d\n",
         //        mq->_adaptive_ctrl->m_uiLastFactor, 
         //        mq->_adaptive_ctrl->m_uiFactor, g_event_cnt, cnt);
         return;
      }
      g_event_cnt++;
      usnet_tcpev_process_handler(data, len, conn_id);
      //sleep(1);
  }
  return;
}

void usnet_tcpev_read(usn_shmmq_t *mq) 
{
   static char data[MAX_BUFFER_SIZE];
   unsigned int len = 0;
   unsigned int conn_id = 0;
   uint32_t cnt = 0;
   while(1){
      len = 0;
      conn_id = 0;
      cnt++;
      if ( cnt % 10000 == 0 ) 
      {   
          DEBUG("breaking the loop, cnt=%d", cnt);
          break;
      }   
      usnet_shmmq_dequeue(mq, data, MAX_BUFFER_SIZE, &len, &conn_id);
      if ( len == 0 ) {
         //printf("empty queue: lastFactor=%d, newFactor=%d, g_event_cnt=%d, cnt=%d\n",
         //        mq->_adaptive_ctrl->m_uiLastFactor, 
         //        mq->_adaptive_ctrl->m_uiFactor, g_event_cnt, cnt);
         return;
      }
      g_event_cnt++;
      usnet_tcpev_process_handler(data, len, conn_id);
  }
  return;
}

int32 
usnet_tcpev_write(usn_shmmq_t *mq) 
{
   return 0;
}

