#ifndef __ORIS_UTIL_H
#define __ORIS_UTIL_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#define strdup _strdup
#define isblank(x) ((x) == ' ' || (x) == '\t')
#endif

#define ORIS_VERSION "0.7.0"
#ifdef _WIN32
#define ORIS_USER_AGENT ("orisgateway/"  ORIS_VERSION " (win32)")
#else
#define ORIS_USER_AGENT ("orisgateway/"  ORIS_VERSION " (linux)")
#endif

typedef int oris_error_t;
extern const oris_error_t ORIS_SUCCESS;
extern const oris_error_t ORIS_EINVALID_ARG;

bool oris_safe_realloc(void** ptr, size_t n, size_t blk_size);
size_t oris_safe_strlen(const char* s);

#define oris_free_and_null(ptr) while (ptr) { free(ptr); ptr = NULL;  }

bool oris_strtoint(const char* s, int* v);

char* oris_ltrim(char* s);
char* oris_rtrim(char* s);
char* oris_upper_str(char* s);

void oris_buf_to_hex(const unsigned char* raw, size_t size, char* const s);

#endif /* __ORIS_UTIL_H */
