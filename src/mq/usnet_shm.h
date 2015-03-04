#ifndef _USNET_SHM_H_
#define _USNET_SHM_H_

#include <sys/shm.h>

#include "usnet_types.h"

typedef struct usn_shm usn_shm_t;
struct usn_shm
{
	key_t key;
	size_t size;
	int id;
	u_char* addr;
};

usn_shm_t* 
usnet_shm_open(key_t key, size_t size);

usn_shm_t* 
usnet_shm_create(key_t key, size_t size);

char* 
usnet_shmat(int _id);

void 
usn_shmdt(char* _mem);

int32
usn_shm_alloc(usn_shm_t *shm);

void 
usn_shm_free(usn_shm_t *shm);


#endif//_USNET_SHM_H_

