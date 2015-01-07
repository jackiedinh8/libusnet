#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "usnet_core.h"
#include "usnet_log.h"
#include "usnet_socket.h"
#include "usnet_in.h"

void accept_handler(u_int32 fd, struct usn_sockaddr* addr, int32 len, void* arg)
{
   struct usn_sockaddr_in *inaddr = (struct usn_sockaddr_in *)addr;
   usn_buf_t *buf = NULL;

   (void)inaddr;
   DEBUG("revc data, fd=%d, addr_len=%d, ip=%x, port=%d, ip_=%x", 
            fd, len, inaddr->sin_addr.s_addr, inaddr->sin_port, inet_addr("10.10.10.1"));
  
   buf = usnet_get_buffer(fd); 

   if ( buf == NULL ) {
      DEBUG("reading error");
      return;
   }

   DEBUG("process data: len=%d \n, %s", buf->len, buf->data);
   usnet_writeto_buffer(fd, buf, inaddr);
   //usnet_write_buffer(fd, buf);

   return;
}

int main(int argc, char *argv[])
{
   int   fd; 
   int32 ret;

   usnet_setup(argc, argv);

   fd = usnet_socket(0,0,0);

   ret = usnet_bind(fd, inet_addr("10.10.10.2"), 35355);
   if ( ret < 0 )
      printf("binding error: ret=%d", ret);
  
   ret = usnet_listen(fd, 0, accept_handler, 0, 0);

   usnet_dispatch();

   return 0;
}



