#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include "oris_util.h"

const oris_error_t ORIS_SUCCESS = 0;
const oris_error_t ORIS_EINVALID_ARG = 0x80010001;

bool oris_safe_realloc(void** ptr, size_t n, size_t blk_size)
{
	void* newptr;
	size_t alloc_size = n * blk_size;

	/* overflow check */
	if (n > SIZE_MAX/blk_size || ptr == NULL) {
		return false; 
	}

	if (*ptr == NULL) {
		newptr = malloc(blk_size);
	} else {
		newptr = realloc(*ptr, alloc_size);
	}

	if (newptr != NULL) {
		*ptr = newptr;
		return true;
	} else {
		return false;
	}
}

/*
void oris_free_and_null(void** ptr)
{
	if (ptr) {
		free(*ptr);
		*ptr = NULL;
	}
}
*/

bool oris_strtoint(const char* s, int* v)
{
	long lv;

	lv = strtol(s, (char**) NULL, 10);

	if (((lv == LONG_MIN || lv == LONG_MAX) && (errno != 0)) || 
		(lv > INT_MAX || lv < INT_MIN)) {
		return false;
	} else {
		*v = (int) lv;
		return true; 
	}
}
