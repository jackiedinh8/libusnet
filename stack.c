#include "usnet_core.h"

int main(int argc, char *argv[])
{
   usnet_setup(argc, argv);

   usnet_dispatch();

   return 0;
}



