#ifndef MEM_POOL_H
#define MEM_POOL_H

struct slab {
    void *free;
    struct slab *next;
};

typedef struct slab slab_t;

struct mem_pool {
    size_t obj_size;
    size_t real_obj_size;
    size_t cache_size;
    size_t slab_nr;
    size_t obj_nr;
    size_t free_obj_nr;
    slab_t *slab_head;
    slab_t *recent_slab;
};

typedef struct mem_pool mem_pool_t;

mem_pool_t *create_mem_pool(size_t obj_size);
void destroy_mem_pool(mem_pool_t *mpl);

void *mem_pool_alloc(mem_pool_t *mpl);

void mem_pool_free(mem_pool_t *mpl, void *mem);

#endif
