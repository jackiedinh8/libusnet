#ifndef USNET_TYPES_H_
#define USNET_TYPES_H_

#include <stdint.h>
#include <stdlib.h>


//normalize standard type
typedef	unsigned char    	 u_char;
typedef	unsigned short     u_short;
typedef	unsigned int    	 u_int;
typedef	unsigned long    	 u_long;
typedef	int32_t            int32;
typedef	uint32_t           u_int32;
typedef	int64_t            int64;
typedef	uint64_t           u_int64;
typedef	uint64_t	          u_quad_t;
typedef	char*	             caddr_t;
typedef	u_char*            ucaddr_t;


// our type definitions
typedef  uintptr_t          usn_uint_t;
typedef  intptr_t           usn_int_t;

typedef struct {
    size_t      len;
    u_char     *data;
} usn_str_t;

#endif /* !_USNET_TYPES_H_ */
