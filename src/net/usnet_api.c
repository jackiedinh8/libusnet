#include "usnet_msg.h"
#include "usnet_log.h"

usn_shmmq_t *g_api_app2net_mq;
usn_shmmq_t *g_api_net2app_mq;

int32
usnet_apimq_init()
{
   g_api_app2net_mq = (usn_shmmq_t *)malloc(sizeof(*g_api_app2net_mq));

   if ( g_api_app2net_mq == NULL ) { 
      DEBUG("malloc failed\n");
      return -1; 
   }   
   usnet_init_shmmq(g_api_app2net_mq,
         "/tmp/api_app2net.fifo", 0, 0, 1157647514, 4194304);

   g_api_net2app_mq = (usn_shmmq_t *)malloc(sizeof(*g_api_net2app_mq));

   if ( g_api_net2app_mq == NULL ) { 
      DEBUG("malloc failed\n");
      return -1; 
   }   
   usnet_init_shmmq(g_api_net2app_mq,
         "/tmp/api_net2app.fifo", 0, 0, 1157647515, 4194304);

   return 0;
}

int32
usnet_api_a2n_enqueue(u_int32 fd, u_char type, u_char event, u_char *body, int32 len)
{
   usn_tcpin_pkg_t pkg;

   pkg.header.type = type;
   pkg.header.event = event;
   //pkg.header.len = (u_short)sizeof(pkg);
   pkg.header.len = sizeof(pkg);
   pkg.fd = fd;
  
   DEBUG("enqueue an event, type=%d, event=%d, fd=%d", type, event, fd);  
   usnet_shmmq_enqueue(g_api_app2net_mq, time(0), &pkg, sizeof(pkg), fd); 

   return 0;
}

int32
usnet_api_a2n_dequeue(u_char *buf, u_int32 buf_size, u_int32 *data_len)
{
   u_int32 flow = 0;
  
   DEBUG("dequeue an event");
   usnet_shmmq_dequeue(g_api_app2net_mq, buf, buf_size, data_len, &flow); 

   return 0;
}


int32
usnet_api_n2a_enqueue(u_int32 fd, u_char type, u_char event, u_char *body, int32 len)
{
   usn_tcpin_pkg_t pkg;

   pkg.header.type = type;
   pkg.header.event = event;
   //pkg.header.len = (u_short)sizeof(pkg);
   pkg.header.len = sizeof(pkg);
   pkg.fd = fd;
  
   DEBUG("enqueue an event, type=%d, event=%d, fd=%d", type, event, fd);  
   usnet_shmmq_enqueue(g_api_net2app_mq, time(0), &pkg, sizeof(pkg), fd); 

   return 0;
}

int32
usnet_api_n2a_dequeue(u_char *buf, u_int32 buf_size, u_int32 *data_len)
{
   u_int32 flow = 0;
  
   DEBUG("dequeue an event");
   usnet_shmmq_dequeue(g_api_net2app_mq, buf, buf_size, data_len, &flow); 

   return 0;
}







