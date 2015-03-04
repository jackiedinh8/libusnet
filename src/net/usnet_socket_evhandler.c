#include "usnet_socket_evhandler.h"
#include "usnet_msg.h"
#include "usnet_log.h"
#include "usnet_event.h"

int32
usnet_tcpin_rwakeup(struct usn_socket *so, u_char type, 
      u_char event, usn_mbuf_t *m)
{
   DEBUG("enqueue msg, type=%d, event=%d", type, event);
   usnet_tcpev_n2a_enqueue(so->so_fd, 
         type, event, 0, 0);
   return 0;
}

int32
usnet_tcpin_wwakeup(struct usn_socket *so, u_char type, 
      u_char event, usn_mbuf_t *m)
{
   DEBUG("enqueue msg, type=%d, event=%d", type, event);
   usnet_tcpev_n2a_enqueue(so->so_fd, 
         type, event, 0, 0);
   return 0;
}

int32
usnet_tcpin_ewakeup(struct usn_socket *so, u_char type, 
      u_char event, usn_mbuf_t *m)
{
   DEBUG("enqueue msg, type=%d, event=%d", type, event);
   usnet_tcpev_n2a_enqueue(so->so_fd, 
         type, event, 0, 0);
   return 0;
}


