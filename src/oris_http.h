#ifndef __ORIS_HTTP_H
#define __ORIS_HTTP_H

#include <stdbool.h>

#include "oris_libevent.h"

#include <event2/bufferevent.h>
#include <event2/http.h>
#include <openssl/ssl.h>

/* key-value pair holding information about an remote http target */
typedef struct oris_http_target {
	char* name;
	struct evhttp_uri* uri;
    struct evhttp_connection* connection;
	struct bufferevent* bev;
	SSL* ssl;
	bool enabled;
} oris_http_target_t;

void oris_perform_http_on_targets(oris_http_target_t* targets, int target_count,
	const enum evhttp_cmd_type method, const char* uri, struct evbuffer* body);

bool oris_str_to_http_method(const char* str, enum evhttp_cmd_type* method);

#endif /* __ORIS_HTTP_H */
