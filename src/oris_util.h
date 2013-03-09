#ifndef __ORIS_UTIL_H
#define __ORIS_UTIL_H

#include <stdbool.h>
#include <event2/event.h>
#include <event2/dns.h>

#include "oris_connection.h"

#define ORIS_VERSION "0.0.1"

typedef uint32_t oris_error_t;
extern const oris_error_t ORIS_SUCCESS;
extern const oris_error_t ORIS_EINVALID_ARG;

typedef struct oris_libevent_base_info {
	struct event_base *base;
	struct evdns_base *dns_base;
} oris_libevent_base_info_t;


typedef struct oris_application_info {
	oris_libevent_base_info_t libevent_info;
	oris_connection_list_t* connections;
	struct event *sigint_event;

	struct {
		oris_http_target_t* items;
		int count;
	} targets;

	void* data_tbl;
	int (*main)(struct oris_application_info*);

	bool paused;
	int log_level;
	int argc;
	char** argv;
} oris_application_info_t;


bool oris_safe_realloc(void** ptr, size_t n, size_t blk_size);


/* void oris_free_and_null(void** ptr);*/ 

#define oris_free_and_null(ptr) do { free(ptr); ptr = NULL;  } while (0);

bool oris_strtoint(const char* s, int* v);


#endif /* __ORIS_UTIL_H */
