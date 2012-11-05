#include <stdlib.h>
#include <string.h>

#include <event2/buffer.h>

#include "oris_log.h"
#include "oris_protocol.h"
#include "oris_protocol_ctrl.h"
#include "oris_protocol_data.h"


oris_protocol_t* oris_protocol_create(void)
{
	oris_protocol_t* retval = calloc(1, sizeof(*retval));

	return retval;
}

oris_protocol_t* oris_simple_protocol_create(const char* name, bufferevent_data_cb readcb)
{
	oris_protocol_t* retval = oris_protocol_create();
	if (retval) {
		retval->name = strdup(name);
		retval->read_cb = readcb;
		retval->destroy = oris_protocol_free;
		retval->connected_cb = NULL;
		retval->disconnected_cb = NULL;
		retval->data = NULL;
	}

	return retval;
}

void oris_protocol_free(oris_protocol_t* protocol)
{
	if (protocol->name) {
		free(protocol->name);
	}

	if (protocol->data) {
		free(protocol->data);
	}

	protocol->read_cb = NULL;
	protocol->connected_cb = NULL;
	protocol->disconnected_cb = NULL;
	free(protocol);
}

oris_protocol_t* oris_get_protocol_from_scheme(const char* scheme, void *data)
{
	oris_protocol_t* retval = NULL;

	if (scheme) {
		if (strcmp(scheme, "ctrl") == 0) {
			retval = oris_simple_protocol_create(scheme, 
				oris_protocol_ctrl_read_cb);
			if (retval) {
				retval->data = calloc(1, sizeof(oris_ctrl_protocol_data_t));
				if (retval->data) {
					((oris_ctrl_protocol_data_t*) retval->data)->info = data;
				}
			}
		} else if (strcmp(scheme, "data") == 0) {
			retval = oris_simple_protocol_create(scheme, 
				oris_protocol_data_read_cb);
			if (retval) {
				retval->data = calloc(1, sizeof(oris_data_protocol_data_t));
				if (retval->data) {
					((oris_data_protocol_data_t*) retval->data)->info = data;
				}
			}
		}
	}

	return retval;;
}


size_t oris_protocol_recv(struct evbuffer* input, char** buffer, size_t* buf_size,
	size_t* buf_capacity)
{
	size_t rlen = evbuffer_get_length(input);
	char* p;

	if (input == NULL || rlen == 0) {
		return 0;
	}

	/* receive data */
	while (*buf_capacity < *buf_size + rlen) {
		if (*buf_capacity == 0) {
			*buf_capacity = 64;
		} else {
			*buf_capacity *= 2;
		}
	}

	p = realloc(*buffer, *buf_capacity);
	if (p) {
		*buffer = p;
	} else {
		oris_log_f(LOG_ERR, "could not allocate additional recv buffer (out of memory?). Dropping data");
		return 0;
	}

	rlen = evbuffer_remove(input, *buffer + *buf_size, rlen);
	if (rlen > 0) {
		*buf_size += rlen;
	}

	return rlen;
}
