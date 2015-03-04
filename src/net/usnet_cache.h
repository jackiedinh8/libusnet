#ifndef _USNET_CACHE_H_
#define _USNET_CACHE_H_

#include "usnet_types.h"
                          
typedef struct usn_hashbase usn_hashbase_t;
struct usn_hashbase {
   int32   hb_pid;
   int32   hb_time;
   int32   hb_len;
   int32   hb_objsize;
   int32  *hb_base;
   int32   hb_size;
   u_char *hb_cache;  
};

void 
usnet_hashbase_init(usn_hashbase_t *base);

int32 
usnet_cache_init(usn_hashbase_t *base, int32 key, 
      int32 hashtime, int32 hashlen, int32 objsize, int32 create);


/*
int 
usnet_get_cachesize();

void 
usnet_show_cachestatus(float& fHashPercent,int& iHashTimeFree);

void 
usnet_show_itemcount();
*/

#endif //_USNET_CACHE_H_
