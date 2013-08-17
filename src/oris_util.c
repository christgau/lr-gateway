#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>

#include "oris_util.h"

const oris_error_t ORIS_SUCCESS = 0;
const oris_error_t ORIS_EINVALID_ARG = 0x80010001;

bool oris_safe_realloc(void** ptr, size_t n, size_t blk_size)
{
	void* newptr;
	size_t alloc_size = n * blk_size;

	/* overflow check */
	if (*ptr == NULL) {
		newptr = calloc(n, blk_size);
	} else {
		if (n > SIZE_MAX/blk_size || ptr == NULL) {
			return false; 
		}
		newptr = realloc(*ptr, alloc_size);
	}

	if (newptr) {
		*ptr = newptr;
		return true;
	} else {
		return false;
	}
}

bool oris_strtoint(const char* s, int* v)
{
	long lv;
	char *endptr;

	errno = 0;
	lv = strtol(s, &endptr, 10);

	if (((lv == LONG_MIN || lv == LONG_MAX) && (errno != 0)) || 
		(lv == 0 && errno != 0) ||
		(lv > INT_MAX || lv < INT_MIN) ||
		(*endptr != '\0')) {
		return false;
	} else  {
		*v = (int) lv;
		return true; 
	}
}

char* oris_ltrim(char* s)
{
	while (*s && isblank(*s)) {
		s++;
	}

	return s;
}


char* oris_rtrim(char* s)
{
	char* p;
	
	for (p = s; *p; p++) ;
	while (isblank(*p) && p != s) {
		*p = 0;
	}

	return s;
}

char* oris_upper_str(char* s)
{
	char* r = s;

	while (s && *s) {
		*s = toupper(*s);
		s++;
	}

	return r;
}
