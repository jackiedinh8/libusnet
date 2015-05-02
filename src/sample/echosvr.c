#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "usnet/core.h"
//#include "usnet/common.h"
#include "usnet/buf.h"
#include "usnet/epoll.h"

void
handle_fd(usn_context_t *ctx, int fd) 
{
   static char  buf[2*1024];
   int n, wbytes,sent;

   n = usnet_read(ctx,fd,buf,2*1024);
   if ( n <= 0 ) {
      return;
   }
   sent = 0;
   while (n>0) {
      wbytes = usnet_write(ctx,fd,buf+sent,n);
      if ( wbytes <= 0 ) {
         usleep(100);
         continue;
      } 
      sent += wbytes;
      n -= wbytes; 
   }
}

#define MAX_EVENTS 10
int main(int argc, char** argv)
{
   usn_context_t *ctx;
   struct usn_epoll_event ev, events[MAX_EVENTS];
   struct sockaddr_in local;
   int addrlen = 0;
   int ret;
   int fd, conn_sock, epollfd;
   int n;
   int nfds;

   ctx = usnet_setup("em1");
   if ( ctx == 0 )
      return 1;

   fd = usnet_socket(ctx,AF_INET,SOCK_STREAM,0);
   if ( fd < 0 ) {
      exit(0);
   }

   local.sin_family = AF_INET;
   local.sin_port = ntohs(35355);
   local.sin_addr.s_addr = inet_addr("10.10.10.2"); 

   ret = usnet_bind(ctx, fd, (struct sockaddr*)&local, sizeof(local));
   if ( ret < 0 ) {
      exit(0);
   }

   ret = usnet_listen(ctx,fd, 5); 
   if ( ret < 0 ) {
      exit(0);
   }

   epollfd = usnet_epoll_create(ctx,10);
   while (1) {

      nfds = usnet_epoll_wait(ctx,epollfd,events,MAX_EVENTS,5000);

      if ( nfds <= 0 ) {
         continue;
      }

      for (n = 0; n < nfds; ++n) {
         if (events[n].data.fd == fd) {
             conn_sock = usnet_accept(ctx, fd, (struct sockaddr *) &local, &addrlen);

             if (conn_sock < 0) {
                //exit(-1);
             }

             //setnonblocking(conn_sock);
             ev.events = USN_EPOLLIN | USN_EPOLLET;
             ev.data.fd = conn_sock;
             if (usnet_epoll_ctl(ctx,epollfd, USN_EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                 //exit(-1);
             }
         } else {
             handle_fd(ctx,events[n].data.fd);
         }
      }
   }

   return 0;
}
