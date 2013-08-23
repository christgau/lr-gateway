#include "oris_kvpair.h"

#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#define strdup _strdup
#endif

oris_kv_pair_t* oris_create_kv_pair(const char* key, void* value, oris_free_kv_value_fn_t free)
{
	oris_kv_pair_t* retval = calloc(1, sizeof(*retval));
	if (retval) {
		retval->key = strdup(key);
		retval->value = value;
		retval->value_destructor = free;
	}

	return retval;
}

void oris_free_kv_pair(oris_kv_pair_t* p)
{
	free(p->key);
	if (p->value_destructor) {
		p->value_destructor(p->value);
	}

	p->key = NULL;
	p->value = NULL;
}

void oris_free_kv_pair_void(void* p)
{
	oris_free_kv_pair((oris_kv_pair_t*) p);
}

