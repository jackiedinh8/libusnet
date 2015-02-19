#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "usnet_shm.h"
#include "usnet_mq.h"
#include "usnet_viewmq.h"

int main(int argc, char **argv) 
{
   usn_shmmq_t *mq;
   usn_viewmq_t view;

   mq = (usn_shmmq_t *)malloc(sizeof(*mq));

   if ( mq == NULL ) {
      printf("malloc failed\n");
      return -1;
   }

   //usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 4194304);
   //usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 67108864);
   usnet_init_shmmq(mq,"/tmp/test.fifo", 0, 0, 1157647512, 33554432);
   print_shmmq(mq);
   view.mq = mq;

   while (1) {
      usnet_viewmq_stats(&view);
      sleep(1);
   }
   return 0;
}
