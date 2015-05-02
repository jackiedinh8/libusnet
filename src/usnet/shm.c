/*
 * Copyright (c) 2015 Jackie Dinh <jackiedinh8@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1 Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  2 Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution.
 *  3 Neither the name of the <organization> nor the 
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @(#)shm.c
 */

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

#include "shm.h"
#include "log.h"

static const int C_PROJECT_ID_TFC = 9527;
static const int C_DEFAULT_OPEN_FLAG = 0666;

usn_shm_t* 
usnet_shm_open(key_t key, size_t size)
{
   usn_shm_t* shm = NULL;
   int id; 

   id = shmget(key, size, C_DEFAULT_OPEN_FLAG);
	if (id < 0) {
		//ERROR("error: %s", strerror(errno));
      return 0;
   }
	
   shm = (usn_shm_t *) malloc(sizeof(usn_shm_t));
   if ( shm == NULL ) {
      //ERROR("malloc failed\n");
      return 0;
   }
   shm->key = key;
   shm->size = size;
   shm->id = id;

	shm->addr = (char*)usnet_shmat(id);

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
      //WARN("failed to create shm, key=%d, size=%d", key, size);   
      return 0;
   }

	shm = (usn_shm_t *) malloc(sizeof(usn_shm_t));
   if ( shm == NULL ) {
      //ERROR("malloc failed\n");
      return 0;
   }
   shm->key = key;
   shm->size = size;
   shm->id = id;

	shm->addr = (char*)usnet_shmat(id);

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
		//ERROR("error: %s", strerror(errno));
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

int
usn_shm_alloc(usn_shm_t *shm)
{
    int  id;
   
    if ( shm->key == 0 )
       id = shmget(IPC_PRIVATE, shm->size, (SHM_R|SHM_W|IPC_CREAT));
    else
       id = shmget(shm->key, shm->size, (SHM_R|SHM_W|IPC_CREAT));

    if (id == -1) {
        //ERROR("Could not get shm, key=%lu", shm->key);
        return -1;
    }

    shm->addr = (char*)shmat(id, NULL, 0);

    if (shm->addr == (void *) -1) {
        //ERROR("Could not attach shm, key=%lu", shm->key);
        return -1;
    }

    // remove the segment from the system, no futher attachment is possible.
    //if (shmctl(id, IPC_RMID, NULL) == -1) {
    //    ERROR("Could not isolate shm, key=%lu", shm->key);
    //    return -2;
    //}

    return (shm->addr == (void *) -1) ? -3 : 0;
}

void usn_shm_free(usn_shm_t *shm)
{
    if (shmdt(shm->addr) == -1) {
        // TODO print log
    }
}

