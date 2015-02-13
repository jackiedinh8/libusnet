#include "usnet_shm.h"

usn_int_t
usn_shm_alloc(usn_shm_t *shm)
{
    int  id;
   
    if ( shm->key == 0 )
       id = shmget(IPC_PRIVATE, shm->size, (SHM_R|SHM_W|IPC_CREAT));
    else
       id = shmget(shm->key, shm->size, (SHM_R|SHM_W|IPC_CREAT));

    if (id == -1) {
        // XXX print log
        return -1;
    }

    shm->addr = (u_char*)shmat(id, NULL, 0);

    if (shm->addr == (void *) -1) {
        // XXX print log
        return -1;
    }

    // XXX: remove the segment from the system, no futher attachment is possible.
    if (shmctl(id, IPC_RMID, NULL) == -1) {
        // XXX print log
        return -2;
    }

    return (shm->addr == (void *) -1) ? -3 : 0;
}


void usn_shm_free(usn_shm_t *shm)
{
    if (shmdt(shm->addr) == -1) {
        // XXX print log
    }
}

