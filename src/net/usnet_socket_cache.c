#include <math.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>

#include "usnet_socket_cache.h"
#include "usnet_log.h"

usn_hashbase_t g_socket_cache;
int32 g_fd;

void
usnet_socketcache_init()
{
   DEBUG("init socket cache");
   usnet_cache_init(&g_socket_cache, 0x33338, 
         5, 100, sizeof(usn_socket_t), 1);

   g_fd = 1;

   return;
}

usn_socket_t *
usnet_find_socket(usn_hashbase_t *base, int32 fd)
{
   usn_socket_t *p = 0;
   usn_socket_t *item = 0;
   usn_socket_t *table = 0;
   int32      unused = 0;
   int32      value = 0;
   int32      i;

   table = (usn_socket_t* )base->hb_cache;

   for ( i=0; i < base->hb_time; i++ ) {
      value = fd % base->hb_base[i];
      item = table + i*base->hb_len + value;
      if ( !unused ) {
         if ( item->so_fd == 0 ) {
            p = item;
            unused = 1;
            continue;
         }
      }
      if ( item->so_fd == fd )
         return item;
   }
   if ( unused ) {
      memset(p, 0, sizeof(usn_socket_t));
      p->so_fd = fd;
      return p;
   }
   return NULL;
}

usn_socket_t *
usnet_get_socket(usn_hashbase_t *base)
{
   usn_socket_t *p = 0;
   usn_socket_t *item = 0;
   usn_socket_t *table = 0;
   int32      value = 0;
   int32      i;
   int32      fd;

   table = (usn_socket_t* )base->hb_cache;

next:
   fd = ++g_fd;
   for ( i=0; i < base->hb_time; i++ ) {
      value = fd % base->hb_base[i];
      item = table + i*base->hb_len + value;
      if ( item->so_fd == 0 ) {
         p = item;
         goto done;
      }
      if ( item->so_fd == fd ) {
         goto next;
      }
   }
   return NULL;
done:
   memset(p, 0, sizeof(usn_socket_t));
   p->so_fd = fd;
   return p;
}

int32
usnet_free_socket(usn_hashbase_t *base, int32 fd)
{
   usn_socket_t *item = 0;
   usn_socket_t *table = 0;
   int32      value = 0;
   int32      i;

   table = (usn_socket_t* )base->hb_cache;
   if ( base == NULL ) {
      ERROR("panic: null hash base");
      return -1;
   }

   for ( i=0; i < base->hb_time; i++ ) {
      value = fd % base->hb_base[i];
      item = table + i*base->hb_len + value;
      if ( item->so_fd == fd ) {
         item->so_fd = 0;
         goto done;
      }
   }

done:
   return 0;
}



