#ifndef BUF_UTILS_H_
#define BUF_UTILS_H_

#include "usnet_types.h"

#define swap_short(src)\
{\
	u_char tmp[2];\
	tmp[0] = ((u_char*)src)[1];\
	tmp[1] = ((u_char*)src)[0];\
	*src = *((u_short*)tmp);\
}

/*
u_char *
cpystrn(u_char *dst, u_char *src, size_t n)
{ 
    if (n == 0) {
        return dst;
    }
    
    while (--n) {
        *dst = *src;
  
        if (*dst == '\0') {
            return dst;
        }

        dst++;
        src++;
    }

    *dst = '\0';

    return dst;
}
*/

static inline void*
reverse_copy(void *dst, void* src, size_t len)
{
    size_t  k;

    if (dst == NULL || src == NULL) {
        return NULL;
    }

    for(k = 0; k < len; ++k) {
        ((u_char*)dst)[k] = ((u_char*)src)[len - 1 - k];
    }

    return dst;
}

#endif // BUF_UTILS_H_
