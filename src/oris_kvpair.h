#ifndef __ORIS_KV_PAIR
#define __ORIS_KV_PAIR

typedef void (*oris_free_kv_value_fn_t) (void* p);

typedef struct {
	char* key;
	void* value;
	oris_free_kv_value_fn_t value_destructor;
} oris_kv_pair_t;

oris_kv_pair_t* oris_create_kv_pair(const char* key, void* value, oris_free_kv_value_fn_t free);

void oris_free_kv_pair(oris_kv_pair_t* p);
void oris_free_kv_pair_void(void* p);

#endif
