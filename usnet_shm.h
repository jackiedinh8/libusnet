#ifndef USNET_SHM_H_
#define USNET_SHM_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "usnet_types.h"

typedef struct {
    u_char      *addr;
    u_int32      key;
    size_t       size;
} usn_shm_t;

usn_int_t
usn_shm_alloc(usn_shm_t *shm);

void 
usn_shm_free(usn_shm_t *shm);

#endif /* !_USNET_SHM_H_ */
