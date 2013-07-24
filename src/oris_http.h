#ifndef __ORIS_HTTP_H
#define __ORIS_HTTP_H

#include <event2/http.h>

#include "oris_libevent.h"

/* key-value pair holding information about an remote http target */
typedef struct oris_http_target {
	char* name;
	struct evhttp_uri* uri;
    struct ev_http_connection* connection;
} oris_http_target_t;


void oris_http_send_request(oris_libevent_base_info_t* libevent_info,
    oris_http_target_t* target, enum evhttp_cmd_type method, 
    const void* buf, size_t buf_size);

#endif /* __ORIS_HTTP_H */
