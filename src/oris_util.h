#ifndef __ORIS_UTIL_H
#define __ORIS_UTIL_H

#include <stdlib.h>
#include <stdbool.h>
#include <event2/event.h>
#include <event2/dns.h>

#include "oris_configuration.h"
#include "oris_table.h"

#define ORIS_VERSION "0.0.2"
#ifdef _WINDOWS
#define ORIS_USER_AGENT ("orisgateway/"  ORIS_VERSION, " (win32)")
#else
#define ORIS_USER_AGENT ("orisgateway/"  ORIS_VERSION " (linux)")
#endif

typedef uint32_t oris_error_t;
extern const oris_error_t ORIS_SUCCESS;
extern const oris_error_t ORIS_EINVALID_ARG;

bool oris_safe_realloc(void** ptr, size_t n, size_t blk_size);

#define oris_free_and_null(ptr) do { free(ptr); ptr = NULL;  } while (0);

bool oris_strtoint(const char* s, int* v);

char* oris_ltrim(char* s);
char* oris_rtrim(char* s);
char* oris_upper_str(char* s);

#endif /* __ORIS_UTIL_H */
