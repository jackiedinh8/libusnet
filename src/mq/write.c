#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "usnet_shm.h"
#include "usnet_mq.h"

#define BUF_SIZE 40
int main(int argc, char **argv) 
{
   int i, cnt;
   char buf[BUF_SIZE];
   usn_shmmq_t *mq;

   mq = (usn_shmmq_t *)malloc(sizeof(*mq));

   if ( mq == NULL ) {
      printf("malloc failed\n");
      return -1;
   }

   //usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 4194304);
   //usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 67108864);
   usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 33554432);

   print_shmmq(mq);

   cnt = atoi(argv[1]);
   printf("enqueue %d msg\n", cnt);
   for ( i=0; i < cnt; i++) {
      usnet_shmmq_enqueue(mq, 0, buf, BUF_SIZE, i);
      //usleep(10);
   }

   print_shmmq(mq);

   return 0;
}
