UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),FreeBSD)
LIBINC := -L/usr/local/lib -lc -lexecinfo 
CC = gcc47
MD5 = md5
endif

ifeq ($(UNAME_S),Linux)
LIBINC := -ldl
CC = g++
MD5 = md5sum
endif

CFLAGS= -g -Werror -Wall 

BASE_DIR=./

INCLUDE = -I./
#-D_USN_ZERO_COPY_
#LIB =  -L/home/dungdv/local/lib

OBJS    := stack.o usnet_arp.o usnet_eth.o usnet_ip.o usnet_core.o usnet_slab.o \
           usnet_buf.o usnet_shm.o usnet_ip_icmp.o usnet_if.o usnet_route.o \
           usnet_error.o usnet_ip_out.o usnet_in.o usnet_in_pcb.o usnet_udp.o usnet_radix.o\
           usnet_socket.o usnet_socket_util.o usnet_order32.o\
           usnet_tcp_subr.o usnet_tcp_var.o usnet_tcp_timer.o usnet_tcp_seq.o\
           usnet_tcp_output.o usnet_tcp_input.o

BIN     := libusnet.a

all: $(BIN) stack

$(BIN):$(OBJS)
	ar -rs $@ $^

stack:stack.o libusnet.a
	$(CC) $(CFLAGS) -o $@ $(INCLUDE) $^ $(LIB)

%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

install:
	cp -f $(BIN) ./run/bin/
	$(MD5) $(BIN) ./run/bin/$(BIN)

clean:
	rm -rfv $(OBJS) $(BIN) stack

love: clean all
   

