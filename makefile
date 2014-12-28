UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),FreeBSD)
LIBINC := -L/usr/local/lib -lc -lexecinfo 
CC = g++47
MD5 = md5
endif

ifeq ($(UNAME_S),Linux)
LIBINC := -ldl
CC = g++
MD5 = md5sum
endif



CFLAGS= -g -Werror -Wall 
CFLAGS1= -g -std=gnu++0x -DUSE_ADAPTIVE_CONTROL


BASE_DIR=./

INCLUDE = -I./
#-D_USN_ZERO_COPY_

LIB =  -L/home/dungdv/local/lib

OBJS    := stack.o usnet_arp.o usnet_eth.o usnet_ip.o usnet_core.o usnet_slab.o \
           usnet_buf.o usnet_shm.o usnet_ip_icmp.o usnet_if.o usnet_route.o \
           usnet_error.o usnet_ip_out.o usnet_in.o usnet_in_pcb.o usnet_udp.o usnet_radix.o

BIN     := stack

all: $(BIN)

$(BIN):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(INCLUDE) $^ $(LIB)

%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

install:
	cp -f $(BIN) ./run/bin/
	$(MD5) $(BIN) ./run/bin/$(BIN)

clean:
	rm -rfv $(OBJS) $(BIN)

