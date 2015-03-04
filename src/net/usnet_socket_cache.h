#ifndef _USNET_SOCKET_CACHE_H_
#define _USNET_SOCKET_CACHE_H_

#include "usnet_types.h"
#include "usnet_buf.h"
#include "usnet_cache.h"
#include "usnet_socket_util.h"

extern usn_hashbase_t g_socket_cache;

void
usnet_socketcache_init();

usn_socket_t *
usnet_get_socket(usn_hashbase_t *base);

usn_socket_t *
usnet_find_socket(usn_hashbase_t *base, int32 fd);

int32
usnet_free_socket(usn_hashbase_t *base, int32 fd);

#endif //_USNET_SOCKET_CACHE_H_

