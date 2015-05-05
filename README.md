libusnet
========

libusnet is a light-weight netmap-based network stack. The aim of the library is to provide an easy way for delelopers to build high performance servers with millions of connections. 

As far as we can benchmark it, libusnet can improve network performance by 10-30%, compared to FreeBSD stack on a simple echo server, despite of using only one cpu core and netmap simulator (not native-netmap cards). Here are summaries of techniques we have used:

- wait-free algorithm and data structures which are shared among processes without using lock mechanism. 
- multi-processes: one network process for packet processing, one application process for application tasks. 
- zero-copy technique: reducing number of copying buffers as mush as we can. 
- use cache-friendly data structures to reduce algorithm costs.
- use design of mTCP project.

We believe that further improvements are quite possible, for example, multi-processes to support multi-queue cards, improving wait-free buffers. Currently, libusnet supports FreeBSD 10 and later.

libusnet provide epoll-like interface, you can look at sample code in directory 'sample'.

To use libusnet, you need to install netmap [1] and following instructions:

  ./configure
  
  make
  
  make install


[1] https://code.google.com/p/netmap
