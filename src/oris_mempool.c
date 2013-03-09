#include <oris_mempool.h>

typedef struct oris_

typedef struct oris_mempool_block_t {
	oris_block_t* next;
	void* next_free;
} oris_mempool_block_t;

typedef struct {
	size_t item_size;
	oris_mempool_block_free_cb_t destructor;
	oris_mempool_block_t* block_head;
} oris_mempool_t;

/* create new memory pool */
oris_mempool_t* oris_new_mempool(size_t item_size, oris_mempool_block_free_cb_t free_cb);

/*  get new item from memory pool */ 
void* oris_mempool_get(oris_mempool_t* pool);

/* free item from pool */
void oris_mempool_free(void* ptr);

/* clear the memory pool */
void oris_mempool_clear(oris_mempool_t* pool); 

/* free the memory pool */
void oris_mempool_free(oris_mempool_t* pool);
