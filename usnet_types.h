#ifndef USNET_TYPES_H_
#define USNET_TYPES_H_

#include <stdint.h>
#include <stdlib.h>

typedef	unsigned char    	 u_char;
typedef	unsigned short     u_short;
typedef	unsigned int    	 u_int;
typedef	unsigned long    	 u_long;
typedef	unsigned short	    ushort;		/* Sys V compatibility */
typedef	unsigned int	    uint;		/* Sys V compatibility */
typedef	uint32_t   u_int32;

typedef	uint64_t	u_quad_t;	/* quads */
typedef	int64_t	quad_t;
typedef	quad_t *	qaddr_t;

typedef	char *	caddr_t;	/* core address */
typedef	uint32_t	dev_t;		/* device number */
typedef  uint32_t	fixpt_t;	/* fixed point number */
typedef	uint32_t	gid_t;		/* group id */
typedef	uint32_t	ino_t;		/* inode number */
typedef	long		key_t;		/* IPC key (for Sys V IPC) */
typedef	uint16_t	mode_t;		/* permissions */
typedef	uint16_t	nlink_t;	/* link count */
typedef	int32_t	pid_t;		/* process id */
typedef	int32_t	swblk_t;	/* swap offset */
typedef	uint32_t	uid_t;		/* user id */

typedef u_short n_short;		/* short as received from the net */
typedef u_long	n_long;			/* long as received from the net */
typedef	u_long	n_time;			/* ms since 00:00 GMT, byte rev */


// Our definition of types
typedef uintptr_t       usn_uint_t;
typedef intptr_t        usn_int_t;


typedef struct {
    size_t      len;
    u_char     *data;
} usn_str_t;

#endif /* !_USNET_TYPES_H_ */
