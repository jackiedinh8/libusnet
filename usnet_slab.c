#include "usnet_slab.h"
#include "usnet_log.h"


#define NGX_SLAB_PAGE_MASK   3
#define NGX_SLAB_PAGE        0
#define NGX_SLAB_BIG         1
#define NGX_SLAB_EXACT       2
#define NGX_SLAB_SMALL       3

#if (NGX_PTR_SIZE == 4)

#define NGX_SLAB_PAGE_FREE   0
#define NGX_SLAB_PAGE_BUSY   0xffffffff
#define NGX_SLAB_PAGE_START  0x80000000

#define NGX_SLAB_SHIFT_MASK  0x0000000f
#define NGX_SLAB_MAP_MASK    0xffff0000
#define NGX_SLAB_MAP_SHIFT   16

#define NGX_SLAB_BUSY        0xffffffff

#else /* (NGX_PTR_SIZE == 8) */

#define NGX_SLAB_PAGE_FREE   0
#define NGX_SLAB_PAGE_BUSY   0xffffffffffffffff
#define NGX_SLAB_PAGE_START  0x8000000000000000

#define NGX_SLAB_SHIFT_MASK  0x000000000000000f
#define NGX_SLAB_MAP_MASK    0xffffffff00000000
#define NGX_SLAB_MAP_SHIFT   32

#define NGX_SLAB_BUSY        0xffffffffffffffff

#endif


#define usn_slab_junk(p, size)     usn_memset(p, 0xA5, size)

usn_slab_pool_t *g_slab_pool;
usn_shm_t g_shm;

static usn_slab_page_t *usn_slab_alloc_pages(usn_slab_pool_t *pool,
    usn_uint_t pages);
static void usn_slab_free_pages(usn_slab_pool_t *pool, usn_slab_page_t *page,
    usn_uint_t pages);
//static void usn_slab_error(usn_slab_pool_t *pool, usn_uint_t level,
//    char *text);


static usn_uint_t  usn_slab_max_size;
static usn_uint_t  usn_slab_exact_size;
static usn_uint_t  usn_slab_exact_shift;

u_int  usn_pagesize;
usn_uint_t  usn_pagesize_shift;
usn_uint_t  usn_cacheline_size;

void
usn_slab_init(usn_slab_pool_t *g_pool, usn_shm_t *shm)
{
    u_char           *p;
    size_t            size;
    usn_int_t         m;
    usn_uint_t        i, n, pages;
    usn_slab_page_t  *slots;
    usn_slab_pool_t  *pool = NULL;

    if ( shm->addr == NULL )
       return;

    // init slab pool at the begining of the shared mem.
    pool = (usn_slab_pool_t *) shm->addr;
    pool->start = shm->addr;
    pool->end = shm->addr + shm->size;
    pool->min_shift = 3;

    //g_pool = pool;

    usn_pagesize = getpagesize(); //usn_pagesize = 4096;
    usn_pagesize_shift = 0;
    usn_slab_max_size  = 0;
    usn_slab_exact_size  = 0;
    usn_slab_exact_shift  = 0;
    usn_cacheline_size = 64;

    for (n = usn_pagesize; n >>= 1; usn_pagesize_shift++) {;/*void*/}

    /* STUB */
    if (usn_slab_max_size == 0) {
        usn_slab_max_size = usn_pagesize / 2;
        usn_slab_exact_size = usn_pagesize / (8 * sizeof(uintptr_t));
        for (n = usn_slab_exact_size; n >>= 1; usn_slab_exact_shift++) {
            /* void */
        }
    }
    /**/

    pool->min_size = 1 << pool->min_shift;

    p = (u_char *) pool + sizeof(usn_slab_pool_t);
    size = pool->end - p;

    usn_slab_junk(p, size);

    slots = (usn_slab_page_t *) p;
    n = usn_pagesize_shift - pool->min_shift;

    for (i = 0; i < n; i++) {
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(usn_slab_page_t);

    pages = (usn_uint_t) (size / (usn_pagesize + sizeof(usn_slab_page_t)));

    usn_memzero(p, pages * sizeof(usn_slab_page_t));

    pool->pages = (usn_slab_page_t *) p;

    pool->free.prev = 0;
    pool->free.next = (usn_slab_page_t *) p;

    pool->pages->slab = pages;
    pool->pages->next = &pool->free;
    pool->pages->prev = (uintptr_t) &pool->free;

    pool->start = (u_char *)
                  usn_align_ptr((uintptr_t) p + pages * sizeof(usn_slab_page_t),
                                 usn_pagesize);

    m = pages - (pool->end - pool->start) / usn_pagesize;
    if (m > 0) {
        pages -= m;
        pool->pages->slab = pages;
    }

    pool->last = pool->pages + pages;

    pool->log_nomem = 1;
    pool->log_ctx = &pool->zero;
    pool->zero = '\0';
}


void *
usn_slab_alloc(usn_slab_pool_t *pool, size_t size)
{
    void  *p;

    //usn_shmtx_lock(&pool->mutex);

    p = usn_slab_alloc_locked(pool, size);

    //usn_shmtx_unlock(&pool->mutex);

    return p;
}


void *
usn_slab_alloc_locked(usn_slab_pool_t *pool, size_t size)
{
    size_t            s;
    uintptr_t         p, n, m, mask, *bitmap;
    usn_uint_t        i, slot, shift, map;
    usn_slab_page_t  *page, *prev, *slots;

    if (size > usn_slab_max_size) {

        //usn_log_debug1(NGX_LOG_DEBUG_ALLOC, usn_cycle->log, 0,
        //               "slab alloc: %uz", size);

        page = usn_slab_alloc_pages(pool, (size >> usn_pagesize_shift)
                                          + ((size % usn_pagesize) ? 1 : 0));
        if (page) {
            p = (page - pool->pages) << usn_pagesize_shift;
            p += (uintptr_t) pool->start;

        } else {
            p = 0;
        }

        goto done;
    }

    if (size > pool->min_size) {
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }
        slot = shift - pool->min_shift;

    } else {
        size = pool->min_size;
        shift = pool->min_shift;
        slot = 0;
    }

    //usn_log_debug2(NGX_LOG_DEBUG_ALLOC, usn_cycle->log, 0,
    //               "slab alloc: %uz slot: %ui", size, slot);

    slots = (usn_slab_page_t *) ((u_char *) pool + sizeof(usn_slab_pool_t));
    page = slots[slot].next;
    
    if (page->next != page) {

        if (shift < usn_slab_exact_shift) {

            do {
                p = (page - pool->pages) << usn_pagesize_shift;
                bitmap = (uintptr_t *) (pool->start + p);

                map = (1 << (usn_pagesize_shift - shift))
                          / (sizeof(uintptr_t) * 8);

                for (n = 0; n < map; n++) {

                    if (bitmap[n] != NGX_SLAB_BUSY) {

                        for (m = 1, i = 0; m; m <<= 1, i++) {
                            if ((bitmap[n] & m)) {
                                continue;
                            }

                            bitmap[n] |= m;

                            i = ((n * sizeof(uintptr_t) * 8) << shift)
                                + (i << shift);

                            if (bitmap[n] == NGX_SLAB_BUSY) {
                                for (n = n + 1; n < map; n++) {
                                     if (bitmap[n] != NGX_SLAB_BUSY) {
                                         p = (uintptr_t) bitmap + i;

                                         goto done;
                                     }
                                }

                                prev = (usn_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK);
                                prev->next = page->next;
                                page->next->prev = page->prev;

                                page->next = NULL;
                                page->prev = NGX_SLAB_SMALL;
                            }

                            p = (uintptr_t) bitmap + i;

                            goto done;
                        }
                    }
                }

                page = page->next;

            } while (page);

        } else if (shift == usn_slab_exact_shift) {

            do {
                if (page->slab != NGX_SLAB_BUSY) {

                    for (m = 1, i = 0; m; m <<= 1, i++) {
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m;

                        if (page->slab == NGX_SLAB_BUSY) {
                            prev = (usn_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = NGX_SLAB_EXACT;
                        }

                        p = (page - pool->pages) << usn_pagesize_shift;
                        p += i << shift;
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);

        } else { /* shift > usn_slab_exact_shift */

            n = usn_pagesize_shift - (page->slab & NGX_SLAB_SHIFT_MASK);
            n = 1 << n;
            n = ((uintptr_t) 1 << n) - 1;
            mask = n << NGX_SLAB_MAP_SHIFT;

            do {
                if ((page->slab & NGX_SLAB_MAP_MASK) != mask) {

                    for (m = (uintptr_t) 1 << NGX_SLAB_MAP_SHIFT, i = 0;
                         m & mask;
                         m <<= 1, i++)
                    {
                        if ((page->slab & m)) {
                            continue;
                        }

                        page->slab |= m;

                        if ((page->slab & NGX_SLAB_MAP_MASK) == mask) {
                            prev = (usn_slab_page_t *)
                                            (page->prev & ~NGX_SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = NGX_SLAB_BIG;
                        }

                        p = (page - pool->pages) << usn_pagesize_shift;
                        p += i << shift;
                        p += (uintptr_t) pool->start;

                        goto done;
                    }
                }

                page = page->next;

            } while (page);
        }
    }

    page = usn_slab_alloc_pages(pool, 1);

    if (page) {
        if (shift < usn_slab_exact_shift) {
            p = (page - pool->pages) << usn_pagesize_shift;
            bitmap = (uintptr_t *) (pool->start + p);

            s = 1 << shift;
            n = (1 << (usn_pagesize_shift - shift)) / 8 / s;

            if (n == 0) {
                n = 1;
            }

            bitmap[0] = (2 << n) - 1;

            map = (1 << (usn_pagesize_shift - shift)) / (sizeof(uintptr_t) * 8);

            for (i = 1; i < map; i++) {
                bitmap[i] = 0;
            }

            page->slab = shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL;

            slots[slot].next = page;

            p = ((page - pool->pages) << usn_pagesize_shift) + s * n;
            p += (uintptr_t) pool->start;

            goto done;

        } else if (shift == usn_slab_exact_shift) {

            page->slab = 1;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;

            slots[slot].next = page;

            p = (page - pool->pages) << usn_pagesize_shift;
            p += (uintptr_t) pool->start;

            goto done;

        } else { /* shift > usn_slab_exact_shift */

            page->slab = ((uintptr_t) 1 << NGX_SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;

            slots[slot].next = page;

            p = (page - pool->pages) << usn_pagesize_shift;
            p += (uintptr_t) pool->start;

            goto done;
        }
    }

    p = 0;

done:

    //usn_log_debug1(NGX_LOG_DEBUG_ALLOC, usn_cycle->log, 0, "slab alloc: %p", p);

    return (void *) p;
}


void *
usn_slab_calloc(usn_slab_pool_t *pool, size_t size)
{
    void  *p;

    //usn_shmtx_lock(&pool->mutex);

    p = usn_slab_calloc_locked(pool, size);

    //usn_shmtx_unlock(&pool->mutex);

    return p;
}


void *
usn_slab_calloc_locked(usn_slab_pool_t *pool, size_t size)
{
    void  *p;

    p = usn_slab_alloc_locked(pool, size);
    if (p) {
        usn_memzero(p, size);
    }

    return p;
}


void
usn_slab_free(usn_slab_pool_t *pool, void *p)
{
    //usn_shmtx_lock(&pool->mutex);
    usn_slab_free_locked(pool, p);

    //usn_shmtx_unlock(&pool->mutex);
}


void
usn_slab_free_locked(usn_slab_pool_t *pool, void *p)
{
    size_t            size;
    uintptr_t         slab, m, *bitmap;
    usn_uint_t        n, type, slot, shift, map;
    usn_slab_page_t  *slots, *page;

    //usn_log_debug1(NGX_LOG_DEBUG_ALLOC, usn_cycle->log, 0, "slab free: %p", p);

    if ((u_char *) p < pool->start || (u_char *) p > pool->end) {
        //usn_slab_error(pool, NGX_LOG_ALERT, "usn_slab_free(): outside of pool");
        goto fail;
    }

    n = ((u_char *) p - pool->start) >> usn_pagesize_shift;
    page = &pool->pages[n];
    slab = page->slab;
    type = page->prev & NGX_SLAB_PAGE_MASK;

    switch (type) {

    case NGX_SLAB_SMALL:

        shift = slab & NGX_SLAB_SHIFT_MASK;
        size = 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        n = ((uintptr_t) p & (usn_pagesize - 1)) >> shift;
        m = (uintptr_t) 1 << (n & (sizeof(uintptr_t) * 8 - 1));
        n /= (sizeof(uintptr_t) * 8);
        bitmap = (uintptr_t *)
                             ((uintptr_t) p & ~((uintptr_t) usn_pagesize - 1));

        if (bitmap[n] & m) {

            if (page->next == NULL) {
                slots = (usn_slab_page_t *)
                                   ((u_char *) pool + sizeof(usn_slab_pool_t));
                slot = shift - pool->min_shift;

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL;
                page->next->prev = (uintptr_t) page | NGX_SLAB_SMALL;
            }

            bitmap[n] &= ~m;

            n = (1 << (usn_pagesize_shift - shift)) / 8 / (1 << shift);

            if (n == 0) {
                n = 1;
            }

            if (bitmap[0] & ~(((uintptr_t) 1 << n) - 1)) {
                goto done;
            }

            map = (1 << (usn_pagesize_shift - shift)) / (sizeof(uintptr_t) * 8);

            for (n = 1; n < map; n++) {
                if (bitmap[n]) {
                    goto done;
                }
            }

            usn_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_EXACT:

        m = (uintptr_t) 1 <<
                (((uintptr_t) p & (usn_pagesize - 1)) >> usn_slab_exact_shift);
        size = usn_slab_exact_size;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        if (slab & m) {
            if (slab == NGX_SLAB_BUSY) {
                slots = (usn_slab_page_t *)
                                   ((u_char *) pool + sizeof(usn_slab_pool_t));
                slot = usn_slab_exact_shift - pool->min_shift;

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;
                page->next->prev = (uintptr_t) page | NGX_SLAB_EXACT;
            }

            page->slab &= ~m;

            if (page->slab) {
                goto done;
            }

            usn_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_BIG:

        shift = slab & NGX_SLAB_SHIFT_MASK;
        size = 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        m = (uintptr_t) 1 << ((((uintptr_t) p & (usn_pagesize - 1)) >> shift)
                              + NGX_SLAB_MAP_SHIFT);

        if (slab & m) {

            if (page->next == NULL) {
                slots = (usn_slab_page_t *)
                                   ((u_char *) pool + sizeof(usn_slab_pool_t));
                slot = shift - pool->min_shift;

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;
                page->next->prev = (uintptr_t) page | NGX_SLAB_BIG;
            }

            page->slab &= ~m;

            if (page->slab & NGX_SLAB_MAP_MASK) {
                goto done;
            }

            usn_slab_free_pages(pool, page, 1);

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_PAGE:

        if ((uintptr_t) p & (usn_pagesize - 1)) {
            goto wrong_chunk;
        }

        if (slab == NGX_SLAB_PAGE_FREE) {
            //usn_slab_error(pool, NGX_LOG_ALERT,
            //               "usn_slab_free(): page is already free");
            goto fail;
        }

        if (slab == NGX_SLAB_PAGE_BUSY) {
            //usn_slab_error(pool, NGX_LOG_ALERT,
            //               "usn_slab_free(): pointer to wrong page");
            goto fail;
        }

        n = ((u_char *) p - pool->start) >> usn_pagesize_shift;
        size = slab & ~NGX_SLAB_PAGE_START;

        usn_slab_free_pages(pool, &pool->pages[n], size);

        usn_slab_junk(p, size << usn_pagesize_shift);

        return;
    }

    /* not reached */

    return;

done:

    usn_slab_junk(p, size);

    return;

wrong_chunk:

    //usn_slab_error(pool, NGX_LOG_ALERT,
    //               "usn_slab_free(): pointer to wrong chunk");

    goto fail;

chunk_already_free:

    //usn_slab_error(pool, NGX_LOG_ALERT,
    //               "usn_slab_free(): chunk is already free");

fail:

    return;
}


static usn_slab_page_t *
usn_slab_alloc_pages(usn_slab_pool_t *pool, usn_uint_t pages)
{
    usn_slab_page_t  *page, *p;

    for (page = pool->free.next; page != &pool->free; page = page->next) {

        if (page->slab >= pages) {

            if (page->slab > pages) {
                page[page->slab - 1].prev = (uintptr_t) &page[pages];

                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;
                page[pages].prev = page->prev;

                p = (usn_slab_page_t *) page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t) &page[pages];

            } else {
                p = (usn_slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | NGX_SLAB_PAGE_START;
            page->next = NULL;
            page->prev = NGX_SLAB_PAGE;

            if (--pages == 0) {
                return page;
            }

            for (p = page + 1; pages; pages--) {
                p->slab = NGX_SLAB_PAGE_BUSY;
                p->next = NULL;
                p->prev = NGX_SLAB_PAGE;
                p++;
            }

            return page;
        }
    }

    if (pool->log_nomem) {
        //usn_slab_error(pool, NGX_LOG_CRIT,
        //               "usn_slab_alloc() failed: no memory");
    }

    return NULL;
}


static void
usn_slab_free_pages(usn_slab_pool_t *pool, usn_slab_page_t *page,
    usn_uint_t pages)
{
    usn_uint_t        type;
    usn_slab_page_t  *prev, *join;

    page->slab = pages--;

    if (pages) {
        usn_memzero(&page[1], pages * sizeof(usn_slab_page_t));
    }

    if (page->next) {
        prev = (usn_slab_page_t *) (page->prev & ~NGX_SLAB_PAGE_MASK);
        prev->next = page->next;
        page->next->prev = page->prev;
    }

    join = page + page->slab;

    if (join < pool->last) {
        type = join->prev & NGX_SLAB_PAGE_MASK;

        if (type == NGX_SLAB_PAGE) {

            if (join->next != NULL) {
                pages += join->slab;
                page->slab += join->slab;

                prev = (usn_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
                prev->next = join->next;
                join->next->prev = join->prev;

                join->slab = NGX_SLAB_PAGE_FREE;
                join->next = NULL;
                join->prev = NGX_SLAB_PAGE;
            }
        }
    }

    if (page > pool->pages) {
        join = page - 1;
        type = join->prev & NGX_SLAB_PAGE_MASK;

        if (type == NGX_SLAB_PAGE) {

            if (join->slab == NGX_SLAB_PAGE_FREE) {
                join = (usn_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
            }

            if (join->next != NULL) {
                pages += join->slab;
                join->slab += page->slab;

                prev = (usn_slab_page_t *) (join->prev & ~NGX_SLAB_PAGE_MASK);
                prev->next = join->next;
                join->next->prev = join->prev;

                page->slab = NGX_SLAB_PAGE_FREE;
                page->next = NULL;
                page->prev = NGX_SLAB_PAGE;

                page = join;
            }
        }
    }

    if (pages) {
        page[pages].prev = (uintptr_t) page;
    }

    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}


//static void
//usn_slab_error(usn_slab_pool_t *pool, usn_uint_t level, char *text)
//{
    //usn_log_error(level, usn_cycle->log, 0, "%s%s", text, pool->log_ctx);
//}
