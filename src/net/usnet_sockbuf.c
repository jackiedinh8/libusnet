#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

#include "usnet_sockbuf.h"
#include "usnet_log.h"

usn_hashbase_t g_sorcv_cache;
usn_hashbase_t g_sosnd_cache;

void
usnet_sockbuf_init()
{
   DEBUG("init sockbuf cache: sorcv");
   usnet_cache_init(&g_sorcv_cache, 0x22228, 
         5, 100, sizeof(sockbuf_t), 1);

   DEBUG("init sockbuf cache: sosnd");
   usnet_cache_init(&g_sosnd_cache, 0x22229, 
         5, 100, sizeof(sockbuf_t), 1);

   return;
}

sockbuf_t *
usnet_get_sockbuf(usn_hashbase_t *base, int32 fd)
{
   sockbuf_t *p = 0;
   sockbuf_t *item = 0;
   sockbuf_t *table = 0;
   int32      unused = 0;
   int32      value = 0;
   int32      i;

   table = (sockbuf_t* )base->hb_cache;

   for ( i=0; i < base->hb_time; i++ ) {
      value = fd % base->hb_base[i];
      item = table + i*base->hb_len + value;
      if ( !unused ) {
         if ( item->sb_fd == 0 ) {
            p = item;
            unused = 1;
            continue;
         }
      }
      if ( item->sb_fd == fd ) 
         return item;
   }
   if ( unused ) {
      memset(p, 0, sizeof(sockbuf_t));
      p->sb_fd = fd;
      return p;
   }
   return NULL;
}


sockbuf_t *
usnet_find_sockbuf(usn_hashbase_t *base, int32 fd)
{
   sockbuf_t *p = 0;
   sockbuf_t *item = 0;
   sockbuf_t *table = 0;
   int32      unused = 0;
   int32      value = 0;
   int32      i;

   table = (sockbuf_t* )base->hb_cache;

   for ( i=0; i < base->hb_time; i++ ) {
      value = fd % base->hb_base[i];
      item = table + i*base->hb_len + value;
      if ( !unused ) {
         if ( item->sb_fd == 0 ) {
            p = item;
            unused = 1;
            continue;
         }
      }
      if ( item->sb_fd == fd )
         return item;
   }
   if ( unused ) {
      memset(p, 0, sizeof(sockbuf_t));
      p->sb_fd = fd;
      return p;
   }
   return NULL;
}

int32
usnet_free_sockbuf(usn_hashbase_t *base, int32 fd)
{
   sockbuf_t *item = 0;
   sockbuf_t *table = 0;
   int32      value = 0;
   int32      i;

   table = (sockbuf_t* )base->hb_cache;
   if ( base == NULL ) {
      ERROR("panic: null hash base");
      return -1;
   }

   for ( i=0; i < base->hb_time; i++ ) {
      value = fd % base->hb_base[i];
      item = table + i*base->hb_len + value;
      if ( item->sb_fd == fd ) {
         item->sb_fd = 0;
         goto done;
      }
   }

done:
   return 0;
}



