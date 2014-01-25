#ifndef __ORIS_PROTOCOL_DATA_H
#define __ORIS_PROTOCOL_DATA_H

#include <sys/queue.h>

#include <event2/bufferevent.h>

#include "oris_app_info.h"
#include "oris_protocol.h"

typedef struct data_request {
	char* message;
	size_t size;
	STAILQ_ENTRY(data_request) queue;
} oris_data_request_t;

typedef struct oris_data_protocol_data {
	oris_application_info_t* info;
	char* buffer;
	size_t buf_size;
	size_t buf_capacity;
	enum { IDLE, WAIT_FOR_RESPONSE } state;
	char* last_req_tbl_name;
	struct event* idle_event;
	STAILQ_HEAD(request_list, data_request) outstanding_requests;
} oris_data_protocol_data_t;

void oris_protocol_data_init(struct oris_protocol* self);
void oris_protocol_data_read_cb(struct bufferevent *bev, void *ctx);
void oris_protocol_data_connected_cb(struct oris_protocol* self);

#endif /* __ORIS_PROTOCOL_DATA_H */
