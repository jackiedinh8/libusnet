#ifndef _USNET_SOCKET_EVHANDLER_H_
#define _USNET_SOCKET_EVHANDLER_H_

#include "usnet_types.h"
#include "usnet_buf.h"
#include "usnet_socket_util.h"

int32
usnet_tcpin_rwakeup(struct usn_socket *so, u_char type, 
      u_char event, usn_mbuf_t *m);

int32
usnet_tcpin_wwakeup(struct usn_socket *so, u_char type, 
      u_char event, usn_mbuf_t *m);

int32
usnet_tcpin_ewakeup(struct usn_socket *so, u_char type, 
      u_char event, usn_mbuf_t *m);

#endif //_USNET_SOCKET_EVHANDLER_H_
