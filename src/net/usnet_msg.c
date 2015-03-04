#include "usnet_msg.h"
#include "usnet_log.h"



int32
usnet_mq_enqueue(usn_shmmq_t *mq, u_int32 fd, u_char type, 
      u_char event, u_char *body, int32 len)
{
   usn_tcpin_pkg_t pkg;

   pkg.header.type = type;
   pkg.header.event = event;
   //pkg.header.len = (u_short)sizeof(pkg);
   pkg.header.len = sizeof(pkg);
   pkg.fd = fd;
  
   DEBUG("enqueue an event, type=%d, event=%d, fd=%d", type, event, fd);  
   usnet_shmmq_enqueue(mq, time(0), &pkg, sizeof(pkg), fd); 

   return 0;
}

int32
usnet_mq_dequeue(usn_shmmq_t *mq, u_char *buf, u_int32 buf_size, u_int32 *data_len)
{
   u_int32 flow = 0;
  
   DEBUG("dequeue an event");
   usnet_shmmq_dequeue(mq, buf, buf_size, data_len, &flow); 

   return 0;
}


void usnet_print_header(usn_header_t *h)
{
   switch(h->type) {
      case USN_TCP_IN:
         printf("tcp_in ");
         break;
      case USN_TCP_OUT:
         printf("tcp_out ");
         break;
      case 0x03:
         printf("udp_in ");
         break;
      case 0x04:
         printf("udp_out ");
         break;
      default:
         printf("unknown type, type=%d\n", h->type);
         break;
   }

   switch(h->event) {
      case USN_TCPST_CLOSED:
         printf("CLOSED \n");
         break;
      case USN_TCPST_LISTEN:
         printf("LISTEN \n");
         break;
      case USN_TCPST_SYN_SENT:
         printf("SYN_SENT \n");
         break;
      case USN_TCPST_SYN_RECEIVED:
         printf("SYN_RECEIVED \n");
         break;
      case USN_TCPST_ESTABLISHED:
         printf("ESTABLISHED \n");
         break;
      case USN_TCPST_CLOSE_WAIT:
         printf("CLOSE_WAIT \n");
         break;
      case USN_TCPST_FIN_WAIT1:
         printf("FIN_WAIT1 \n");
         break;
      case USN_TCPST_CLOSING:
         printf("CLOSING \n");
         break;
      case USN_TCPST_LAST_ACK:
         printf("LAST_ACK \n");
         break;
      case USN_TCPST_FIN_WAIT2:
         printf("FIN_WAIT2 \n");
         break;
      case USN_TCPST_TIME_WAIT:
         printf("TIME_WAIT \n");
         break;
      case USN_TCPEV_READ:
         printf("READ \n");
         break;
      case USN_TCPEV_WRITE:
         printf("WRITE \n");
         break;
      case USN_TCPEV_OUTOFBOUND:
         printf("OUTOFBOUND \n");
         break;
      case USN_TCPEV_DISCONNECTED:
         printf("DISCONNECTED \n");
         break;
      case USN_TCPAPI_CONNECT:
         printf("API CONNECT \n");
         break;

 
      default:
         printf("unknown event, event=%d\n", h->event);
         break;
   }

   return;
}




