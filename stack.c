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

   DEBUG("process data: len=%d \n", buf->len);
//#define TEST_BCAST
#ifdef TEST_BCAST
   int32 i, cnt, num, ret;
   usn_mbuf_t *m = ((usn_mbuf_t*)buf);
   struct timeval stime, etime, dtime;
   printf("head=%p, mlen=%d\n",m->head,m->mlen);
   cnt = 150000;
   num = 0;
   gettimeofday(&stime, 0);
   for (i=0; i<cnt; i++){
      ((usn_mbuf_t*)buf)->refs++;
      ret = usnet_writeto_buffer(fd, buf, inaddr);

      if ( ret < 0 ) {
         printf("ret=%d, i=%d\n",ret,i);
         continue;
      }
      num++;
      ((usn_mbuf_t*)buf)->head += sizeof(usn_ip_t) + sizeof(usn_udphdr_t)+14;
      ((usn_mbuf_t*)buf)->mlen -= sizeof(usn_ip_t) + sizeof(usn_udphdr_t)+14;
      
   }
   gettimeofday(&etime, 0); 
   timersub(&etime, &stime, &dtime);
   printf("num of sent udp packets by netmap-based application: %d(/%d)\n", num, cnt);
   printf("total time: %lu (seconds) %lu (microseconds)\n", dtime.tv_sec, dtime.tv_usec);
#else
   usnet_writeto_buffer(fd, buf, inaddr);
#endif

   //usnet_write_buffer(fd, buf);

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



