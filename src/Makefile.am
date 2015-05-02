ACLOCAL_AMFLAGS = -I m4 --install
lib_LTLIBRARIES = libusnet.la

libusnet_la_SOURCES = usnet/epoll.c usnet/ip_icmp.c usnet/utils.c usnet/buf.c usnet/ringbuf.c usnet/socket.c  usnet/eth.c usnet/socket.h usnet/cache.c usnet/log.c usnet/shm.c usnet/tcb.c usnet/timer.c usnet/event.c usnet/mempool.c usnet/tcp.c usnet/core.c usnet/ip.c usnet/udp.c usnet/arp.c usnet/mq.c usnet/sndbuf.c usnet/tcp_out.c usnet/epoll.h usnet/ip_icmp.h usnet/utils.h usnet/buf.h usnet/ringbuf.h usnet/socket.h  usnet/eth.h usnet/socket.h usnet/cache.h usnet/log.h usnet/shm.h usnet/tcb.h usnet/timer.h usnet/event.h usnet/mempool.h usnet/tcp.h usnet/core.h usnet/ip.h usnet/udp.h usnet/arp.h usnet/mq.h usnet/sndbuf.h usnet/tcp_out.h

nobase_include_HEADERS = usnet/buf.h usnet/common.h usnet/epoll.h usnet/log.h usnet/shm.h usnet/tcb.h usnet/types.h usnet/cache.h usnet/core.h usnet/ip.h usnet/mq.h usnet/tcp.h

libusnet_la_CFLAGS = -g -Werror -Wall -DTCPOPT_TIMESTAMP_ENABLED -DTRY_SEND_BEFORE_QUEUE -DDUMP_STREAM
libusnet_la_LIBADD = -lpthread -lm
#AM_CFLAGS = -Werror -Wall -DTCPOPT_TIMESTAMP_ENABLED -DTRY_SEND_BEFORE_QUEUE -DDUMP_STREAM

EXTRA_DIST = m4/NOTES sample/echosvr.c sample/makefile


