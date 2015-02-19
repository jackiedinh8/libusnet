#ifndef _USNET_SHM_H_
#define _USNET_SHM_H_

#include <sys/shm.h>

typedef struct usn_shm usn_shm_t;
struct usn_shm
{
	key_t key;
	size_t size;
	int id;
	char* mem;
};

usn_shm_t* 
usnet_shm_open(key_t key, size_t size);

usn_shm_t* 
usnet_create_shm_only(key_t key, size_t size);

char* 
usnet_shmat(int _id);

void 
usn_shmdt(char* _mem);

#endif//_USNET_SHM_H_

