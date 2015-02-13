#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "usnet_common.h"
#include "usnet_core.h"
#include "usnet_log.h"
#include "usnet_ip.h"
#include "usnet_udp.h"
#include "usnet_socket.h"
#include "usnet_in.h"

void read_handler(u_int32 fd, u_short flags, void* arg)
{
   usn_buf_t *buf = NULL;
   buf = usnet_get_buffer(fd); 
   if ( buf == NULL ) {
      DEBUG("reading error");
      return;
   }
   DEBUG("process data: fd=%d, len=%d \n", fd, buf->len);
   usnet_writeto_buffer(fd, buf, 0);
   usnet_drain_buffer(fd);

   return;
}
void error_handler(u_int32 fd, int32 event, void* arg)
{
   if ( event == 1 )
      DEBUG("CLOSE_WAIT");
   usnet_close_socket(fd);
}
void accept_handler(u_int32 fd, struct usn_sockaddr* addr, int32 len, void* arg)
{
   /* 
   struct usn_sockaddr_in *inaddr = (struct usn_sockaddr_in *)addr;
   usn_buf_t *buf = NULL;
   buf = usnet_get_buffer(fd); 
   if ( buf == NULL ) {
      DEBUG("reading error");
      return;
   }
   DEBUG("process data: len=%d \n", buf->len);
   usnet_writeto_buffer(fd, buf, inaddr);
   */

   DEBUG("new connection, fd=%d", fd);
   usnet_set_socketcb(fd, 0, read_handler, 0, error_handler, 0);

   //DEBUG("new connection, fd=%d, addr_len=%d, ip=%x, port=%d, ip_=%x", 
   //         fd, len, inaddr->sin_addr.s_addr, inaddr->sin_port, inet_addr("10.10.10.1"));
   return;
}

int main(int argc, char *argv[])
{
   int   fd; 
   int32 ret;

   usnet_setup(argc, argv);

   fd = usnet_socket(USN_AF_INET,SOCK_STREAM,0);

   ret = usnet_bind(fd, inet_addr("10.10.10.2"), 35355);
   if ( ret < 0 )
      printf("binding error: ret=%d", ret);
  
   ret = usnet_listen(fd, 0, accept_handler, 0, 0);

   usnet_dispatch();

   return 0;
}



