#ifndef _USN_SLAB_H_
#define _USN_SLAB_H_

#include <unistd.h>
#include <string.h>

#include <usnet_types.h>
#include <usnet_shm.h>

// Nginx slab allocator

#define usn_memzero(buf, n)       (void) memset(buf, 0, n)
#define usn_memset(buf, c, n)     (void) memset(buf, c, n)

#define usn_align_ptr(p, a)                                                   \
    (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))


/*  'slab' field is used for two purposes: 
 *    + a size of object in slab, which represented as a "shift".
 *    + if shift is "exact", then it represents bitmap for the slab.
 *    + if shift is bigger than "exact", the shift is 16 lower bits.
 *  'next' field:
 *    + point to the next slab page.
 *    + NULL means occupied.
 *  'prev' field:
 *    + the address of the prev slab.
 *    + 2 lower bits determines slab type.
 */

typedef struct usn_slab_page_s  usn_slab_page_t;
struct usn_slab_page_s {
    uintptr_t         slab;
    usn_slab_page_t  *next;
    uintptr_t         prev;
};


typedef struct {
    //usn_shmtx_sh_t    lock;

    size_t            min_size;
    size_t            min_shift;

    usn_slab_page_t  *pages;
    usn_slab_page_t  *last;
    usn_slab_page_t   free;

    u_char           *start;
    u_char           *end;

    //usn_shmtx_t       mutex;

    u_char           *log_ctx;
    u_char            zero;

    unsigned          log_nomem:1;

    void             *data;
    void             *addr;
} usn_slab_pool_t;


extern usn_slab_pool_t *g_slab_pool;
extern usn_shm_t g_shm;

void usn_slab_init(usn_slab_pool_t *pool, usn_shm_t *shm);
void *usn_slab_alloc(usn_slab_pool_t *pool, size_t size);
void *usn_slab_alloc_locked(usn_slab_pool_t *pool, size_t size);
void *usn_slab_calloc(usn_slab_pool_t *pool, size_t size);
void *usn_slab_calloc_locked(usn_slab_pool_t *pool, size_t size);
void usn_slab_free(usn_slab_pool_t *pool, void *p);
void usn_slab_free_locked(usn_slab_pool_t *pool, void *p);


#endif /*!_USNET_SLAB_H_ */
