#include <errno.h>
#include <string.h>
#include <unistd.h>

#if __FreeBSD__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>

#endif

#include "usnet_shm.h"

static const int C_PROJECT_ID_TFC = 9527;
static const int C_DEFAULT_OPEN_FLAG = 0600;

usn_shm_t* 
usnet_shm_open(key_t key, size_t size)
{
   usn_shm_t* shm = NULL;
   int id; 

   id = shmget(key, size, C_DEFAULT_OPEN_FLAG);
	if (id < 0) {
		printf("error: %s", strerror(errno));
      return 0;
   }
	
   shm = (usn_shm_t *) malloc(sizeof(usn_shm_t));
   if ( shm == NULL ) {
      printf("malloc failed\n");
      return 0;
   }
   shm->key = key;
   shm->size = size;
   shm->id = id;

	shm->addr = (u_char*)usnet_shmat(id);

   if ( shm->addr == NULL ) {
      free(shm);
      return 0;
   }

	return shm;
}

usn_shm_t* 
usnet_shm_create(key_t key, size_t size)
{
   usn_shm_t* shm = NULL;
   int id;
   
   id = shmget(key, size, C_DEFAULT_OPEN_FLAG | IPC_CREAT | IPC_EXCL);
	if (id < 0) {
		printf("error: %s\n", strerror(errno));
      return 0;
   }

	shm = (usn_shm_t *) malloc(sizeof(usn_shm_t));
   if ( shm == NULL ) {
      printf("malloc failed\n");
   }
   shm->key = key;
   shm->size = size;
   shm->id = id;

	shm->addr = (u_char*)usnet_shmat(id);

   if ( shm->addr == NULL ) {
      free(shm);
      return 0;
   }

	return shm;
}

char* 
usnet_shmat(int _id)
{
	char* p = (char*) shmat(_id, NULL, 0);
	if (p == (char*)-1) {
		printf("error: %s", strerror(errno));
      return 0;
   }

	return p;
}

void 
usn_shmdt(char* _mem)
{
	if (_mem == NULL)
		return;
	
	int ret = shmdt(_mem);
	if (ret < 0) {
		printf("error: %s", strerror(errno));
      return;
   }
}

int32
usn_shm_alloc(usn_shm_t *shm)
{
    int  id;
   
    if ( shm->key == 0 )
       id = shmget(IPC_PRIVATE, shm->size, (SHM_R|SHM_W|IPC_CREAT));
    else
       id = shmget(shm->key, shm->size, (SHM_R|SHM_W|IPC_CREAT));

    if (id == -1) {
        // TODO print log
        return -1;
    }

    shm->addr = (u_char*)shmat(id, NULL, 0);

    if (shm->addr == (void *) -1) {
        // TODO print log
        return -1;
    }

    // remove the segment from the system, no futher attachment is possible.
    if (shmctl(id, IPC_RMID, NULL) == -1) {
        // TODO print log
        return -2;
    }

    return (shm->addr == (void *) -1) ? -3 : 0;
}

void usn_shm_free(usn_shm_t *shm)
{
    if (shmdt(shm->addr) == -1) {
        // TODO print log
    }
}

