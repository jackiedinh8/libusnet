/*
 * Copyright (c) 2014 Jackie Dinh <jackiedinh8@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)usnet_types.h
 */


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

#include <arpa/inet.h>
#define NTOHL(x) (x)=ntohl(x)
#define NTOHS(x) (x)=ntohs(x)
#define HTONS(x) (x)=ntohs(x)
#define HTONL(x) (x)=ntohs(x)

#endif /* !_USNET_TYPES_H_ */
