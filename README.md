libusnet
========

libusnet is a light-weight netmap-based network stack [1]. The aim of the library is to provide an easy way for delelopers to build high performance servers with millions of connections. On FreeBSD and Debian box, processing time for sending 60-bytes data to echosvr, written in libusnet, gives smaller amount of time at least 10-30%, compared to echo server, written in FreeBSD networking stack. 

We believe that further improvements are still possible, since we could add more network and application processes and take advantages of multi-queue support by network cards. In future, we shall conduct more experiments.

Here are summaries of techniques we have used:

- wait-free algorithm and data structures which are shared among processes without using lock mechanism. 
- multi-processes: one network process for packet processing, one application process for application tasks. 
- zero-copy technique: reducing number of copying buffers as mush as we can. 
- use cache-friendly data structure to reduce algorithm costs.
- design of mTcp.

To use libusnet, one needs netmap to be installed. Netmap is delivered with FreeBSD release since version 10. 

FreeBSD instructions: since recent FreeBSD distributions already include netmap, you only need build the new kernel or modules as below:

  + add 'device netmap' to your kernel config file and rebuild a kernel.
    This will include the netmap module and netmap support in the device
    drivers.  Alternatively, you can build standalone modules
    (netmap, ixgbe, em, lem, re, igb)

  + sample applications are in the examples/ directory in this archive,
    or in src/tools/tools/netmap/ in FreeBSD distributions

On Linux, one needs to install it as kernel module.

Linux instructions: on Linux, netmap is an out-of-tree module, so you need to compile it from these sources. The Makefile in the LINUX/ directory will also let you patch device driver sources and build some netmap-enabled device drivers.
  
  + make sure you have kernel sources matching your installed kernel
    (headers only suffice, if you want NETMAP/VALE but no drivers)

  + build kernel modules and sample applications:
    If kernel sources are in /foo//linux-A.B.C/ , then you should do
```
	cd netmap/LINUX
	//build kernel modules
	make NODRIVERS=1 KSRC=/foo/linux-A.B.C/	# only netmap
	make KSRC=/a/b/c/linux-A.B.C/		# netmap+device drivers
	//build sample applications
	make KSRC=/a/b/c/linux-A.B.C/ apps	# builds sample applications
```

You can omit KSRC if your kernel sources are in a standard place.

  + if you use distribution packages, source may not contain headers (e.g., on
    debian systems). Use
```
        make SRC=/a/b/c/linux-sources-A.B/ KSRC=/a/b/c/linux-headers-A.B/
```
Sample application:
   ```C
   epollfd = usnet_epoll_create(ctx,10);

   while (1) {
   
      nfds = usnet_epoll_wait(ctx,epollfd,events,MAX_EVENTS,5000);
      
      if ( nfds <= 0 ) 
         continue;
      
      for (n = 0; n < nfds; ++n) {
         if (events[n].data.fd == fd) {
             conn_sock = usnet_accept(ctx, fd, (struct sockaddr *) &local, &addrlen);
             if (conn_sock < 0) 
                exit(-1);
             ev.events = USN_EPOLLIN | USN_EPOLLET;
             ev.data.fd = conn_sock;
             if (usnet_epoll_ctl(ctx,epollfd, USN_EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                 exit(-1);
             }
         } else {
             handle_fd(ctx,events[n].data.fd);
         }
      }
   }
   ```



For more details, see example at https://github.com/jackiedinh8/libusnet/blob/master/src/sample/echosvr.c

[1] https://code.google.com/p/netmap
