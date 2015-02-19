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

OBJS    := write.o usnet_shm.o usnet_mq.o

BIN     := write

all: $(BIN) view read

$(BIN):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(INCLUDE) $^ $(LIB)

view:view.o usnet_shm.o usnet_mq.o usnet_viewmq.o
	$(CC) $(CFLAGS) -o $@ $(INCLUDE) $^ $(LIB)

read:read.o usnet_shm.o usnet_mq.o 
	$(CC) $(CFLAGS) -o $@ $(INCLUDE) $^ $(LIB) -levent

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<


clean:
	rm -rfv $(OBJS) $(BIN) read.o view.o usnet_viewmq.o read view

love: clean all
   

