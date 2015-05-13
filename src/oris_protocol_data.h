#ifndef __ORIS_PROTOCOL_DATA_H
#define __ORIS_PROTOCOL_DATA_H

#include <sys/queue.h>
#ifdef _WIN32
#define STAILQ_ENTRY SIMPLEQ_ENTRY
#define STAILQ_HEAD SIMPLEQ_HEAD
#define STAILQ_FIRST SIMPLEQ_FIRST
#define STAILQ_INIT SIMPLEQ_INIT
#define STAILQ_EMPTY SIMPLEQ_EMPTY
#define STAILQ_INSERT_TAIL SIMPLEQ_INSERT_TAIL
#define STAILQ_REMOVE_HEAD(head, field) SIMPLEQ_REMOVE_HEAD(head, (head)->sqh_first, field)
#endif

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
	void* connection;
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
