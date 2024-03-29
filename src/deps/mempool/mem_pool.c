#include <stdio.h>
#include <stdlib.h>
#include "mem_pool.h"

 /*
 *      slab                slab
 *      +---------+    +--->+---------+
 *      |  *free  |    |    |  *free  |
 *      |  *next  |----+    |  *next  |
 *      +---------+         +---------+ -----------------------
 *      |         |         |         |  /|\            /|\
 *      |         |         |         |   |              |
 *      |         |         |         | obj_size         |
 *      |         |         |         |   |        real_obj_size
 *      |         |         |         |  \|/             |
 *      |         |         +---------+ --------         |
 *      |         |         |  void*  |                 \|/
 *      |         |         +---------+ -----------------------
 *      |         |         |         |
 *      |         |         |         |
 *      |         |         |         |
 *      |         |         |         |
 *      |         |         |         |
 *      |         |         +---------+
 *      |         |         |  void*  |
 *      |         |         +---------+
 *      |         |         |         |
 *      |         |         |         |
 *      |         |         |         |
 *      |         |         |         |
 *      |         |         |         |
 *      |         |         +---------+
 *      |         |         |  void*  |
 *      +---------+         +---------+
 *
 */

#define SLAB_CACHE(slab) ((char*)slab + sizeof(slab_t))

const size_t OBJ_NUM = 128;

void set_freepointer(mem_pool_t *mpl, void *obj, void *fp)
{
    *(char **)((char*) obj + mpl->obj_size) = fp;
}

void *get_freepointer(mem_pool_t *mpl, char *obj)
{
    return *(char **)(obj + mpl->obj_size);
}

void init_slab(mem_pool_t *mpl, slab_t *slab)
{
    char *start = SLAB_CACHE(slab);
    char *end = SLAB_CACHE(slab) + mpl->cache_size;

    slab->free = start;
    for (; start < end; start += mpl->real_obj_size) {
        set_freepointer(mpl, start, start + mpl->real_obj_size);
    }

    set_freepointer(mpl, start - mpl->real_obj_size, NULL);
}

slab_t *add_new_slab(mem_pool_t *mpl)
{
    slab_t *new_slab;

    new_slab = malloc(sizeof(slab_t) + mpl->cache_size);

    init_slab(mpl, new_slab);

    new_slab->next = mpl->slab_head;
    mpl->slab_head = new_slab;

    mpl->slab_nr++;
    mpl->obj_nr += OBJ_NUM;
    mpl->free_obj_nr += OBJ_NUM;

    return new_slab;
}

void free_slab(slab_t *slab)
{
    free(slab);
}

mem_pool_t *create_mem_pool(size_t obj_size)
{
    mem_pool_t *mpl = malloc(sizeof(mem_pool_t));

    if (obj_size % sizeof(long)) {
        mpl->obj_size = ( obj_size / sizeof(long) + 1 ) * sizeof(long);
    }
    else {
        mpl->obj_size = obj_size;
    }
    mpl->real_obj_size = mpl->obj_size + sizeof(void*);
    mpl->cache_size = OBJ_NUM * mpl->real_obj_size;

    mpl->slab_nr = 0;
    mpl->obj_nr = 0;
    mpl->free_obj_nr = 0;
    mpl->slab_head = NULL;
    mpl->recent_slab = NULL;

    return mpl;
}

void destroy_mem_pool(mem_pool_t *mpl)
{
    slab_t *slab;

    if (mpl) {
        slab = mpl->slab_head;
        while (slab) {
            mpl->slab_head = slab->next;
            free_slab(slab);
            slab = mpl->slab_head;
        }
        free(mpl);
    }
}

void *mem_pool_alloc(mem_pool_t *mpl)
{
    slab_t *found_slab;
    void *object;

    if (mpl->recent_slab && mpl->recent_slab->free) {
        found_slab = mpl->recent_slab;
        goto found;
    }

    found_slab = mpl->slab_head;
    while (found_slab) {
        if (found_slab->free) {
            mpl->recent_slab = found_slab;
            goto found;
        }
        found_slab = found_slab->next;
    }

    found_slab = add_new_slab(mpl);

found:
    object = found_slab->free;
    found_slab->free = get_freepointer(mpl, object);
    set_freepointer(mpl, object, found_slab);

    mpl->free_obj_nr--;

    return object;
}    

void mem_pool_free(mem_pool_t *mpl, void *object)
{
    slab_t *slab;

    slab = (slab_t*)get_freepointer(mpl, object);
    set_freepointer(mpl, object, slab->free);
    slab->free = object;

    mpl->free_obj_nr++;
}
