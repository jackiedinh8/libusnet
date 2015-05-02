PREFIX_DIR := <put_your_installed_dir_here>
LIBINC := -L/usr/local/lib -lc -lm -lpthread -L${PREFIX_DIR}/lib -lusnet
CC = gcc47

CFLAGS= -g -Werror -Wall 

INCLUDE = -I${PREFIX_DIR}/include 

SVR     := echosvr
OBJS     := echosvr.o

all: $(SVR)

$(SVR):$(OBJS)
	$(CC) $(CFLAGS)  -o $@ $(INCLUDE) $^ $(LIBINC)

%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

clean:
	rm -rfv $(SVR) $(OBJS)

   

