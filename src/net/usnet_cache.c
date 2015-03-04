#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <errno.h>

#include "usnet_cache.h"
#include "usnet_log.h"


void 
usnet_hashbase_init(usn_hashbase_t *base)
{
    int iSqrt;
    int i, j, k;
    int iFlag;

    if ( base == NULL )
       return;

    base->hb_base = malloc(sizeof(int32)*base->hb_time);
    if ( base->hb_base == NULL ) {
       ERROR("malloc failed"); 
       return;
    }

    iSqrt = (int) sqrt(base->hb_len);
    for(i = base->hb_len, j = 0; j <base->hb_time; i--)
    {
        iFlag = 1;
        for(k = 2; k <= iSqrt; k++)
        {
            if(i % k == 0)
            {
                iFlag = 0;
                break;
            }
        }
        if(iFlag == 1)
        {
            base->hb_base[j] = i;
            if(i<=0)
            {
                ERROR("HashBase[%d] = %d,HashLen[%d] is too small!\n",
                      j,i,base->hb_len);
                exit(-1);
            }
            j++;
        }
    }
    return;
}

u_char*
usnet_get_shm(int32 key, int32 size, int32 flag)
{
   int id; 
   u_char *addr;
   
   id = shmget(key, size, flag);

   if (id < 0) {
      ERROR("error: %s", strerror(errno));
      return 0;
   }   

   addr = (u_char*)shmat(id, NULL, 0);

   if ( addr == NULL ) { 
      ERROR("error: %s", strerror(errno));
      return 0;
   }   

   return addr;
}

int32 
usnet_get_cache(usn_hashbase_t *base, int32 key, int32 size, int32 create)
{
    u_char* pData;// = usnet_get_shm(key, size, 0666);

    base->hb_size = size;

    printf("shm size:%d\n", size);
    pData = usnet_get_shm(key, size, 0666);
    if (pData == NULL)
    {
        if(create)
        {
            pData = usnet_get_shm(key, size, (0666|IPC_CREAT));
            if(pData == NULL)
            {
                printf("Create shm %d failed\n", key);
                return -1;
            }
            memset(pData, 0, size);
            base->hb_cache = pData;
        }
        else
        {
            printf("InvertedCache init fail\n");
            return -2;
        }
    }
    else
    {
        memset(pData, 0, size);
        base->hb_cache = pData;
    }

    return 0;
}

int32 
usnet_cache_init(usn_hashbase_t *base, int32 key, 
      int32 hashtime, int32 hashlen, int32 objsize, int32 create)
{
   int32 iSize;
   int32 iKey;
   int32 ret;

   if ( base == NULL ) {
      ERROR("hash base is null\n");
      return -1;
   }

   base->hb_pid = 0;
   base->hb_time = hashtime;
   base->hb_len = hashlen;
   base->hb_objsize = objsize;

   iSize = objsize * hashtime * hashlen;
   iKey = key + base->hb_pid;
   ret = usnet_get_cache(base, iKey, iSize, create);

   if ((0 != ret) || (NULL == base->hb_cache))
   {
      ERROR("InvertedCache init fail\n");
      return -2;
   }

   usnet_hashbase_init(base);

   return 0;
}


