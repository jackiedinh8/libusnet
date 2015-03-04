#include <string.h>

#include "usnet_api_net.h"
#include "usnet_msg.h"
#include "usnet_log.h"
#include "usnet_event.h"

void usnet_tcpapi_net_handler(char *buff, int len, int fd)
{
   usn_tcpin_pkg_t pkg;
   
   if ( len < sizeof(pkg.header) ) {
      DEBUG("buffer is too small, len=%d, fd=%d\n", len, fd);
      return;
   }
   memcpy(&pkg.header, buff, sizeof(pkg.header));
  
   usnet_print_header(&pkg.header); 

   return;
}

void usnet_tcpapi_net() 
{
   static char data[MAX_BUFFER_SIZE];
   unsigned int len = 0;
   unsigned int conn_id = 0;
   uint32_t cnt = 0;
   usn_shmmq_t *mq = g_tcpev_app2net_mq;
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
      usnet_tcpapi_net_handler(data, len, conn_id);
  }
  return;
}

