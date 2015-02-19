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

	shm->mem = usnet_shmat(id);

   if ( shm->mem == NULL ) {
      free(shm);
      return 0;
   }

	return shm;
}

usn_shm_t* 
usnet_create_shm_only(key_t key, size_t size)
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

	shm->mem = usnet_shmat(id);

   if ( shm->mem == NULL ) {
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

