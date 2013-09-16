#ifndef __ORIS_PROTOCOL_H
#define __ORIS_PROTOCOL_H

#include <event2/buffer.h>
#include <event2/bufferevent.h>

/* function pointer type for functions to write on a connection */
typedef void (*oris_connection_write_fn_t) (const void* connection,
	const void* buf, const size_t count);

/* function pointer type for functions to write data using a protocol */
typedef void (*oris_protocol_write_fn_t) (const void* buf,
	size_t bufsize, void* con, oris_connection_write_fn_t transfer_fn);

typedef struct oris_protocol {
	char* name;
	void* data;
	void (*destroy)(struct oris_protocol*);
	/* callbacks */
	bufferevent_data_cb read_cb;
	void (*connected_cb)(struct oris_protocol*);
	void (*disconnected_cb)(void);
	oris_protocol_write_fn_t write;
} oris_protocol_t;

/**
 * creates a new (unuseable protocol instance)
 * @return oris_protocol_t* the new protocol instance, can be NULL
 */
oris_protocol_t* oris_protocol_create(void);

/**
 * creates a new protocol with the given readcb handler
 */
oris_protocol_t* oris_simple_protocol_create(const char* name, bufferevent_data_cb readcb);

void oris_protocol_free(oris_protocol_t* protocol);

/**
 * get the protocol instance from an uri scheme
 * @return oris_protocol_t* the according protocol instance or NULL
 * 	if no matching protocol is found
 */
oris_protocol_t* oris_get_protocol_from_scheme(const char* scheme, void *data);

/**
 * receive data from an input buffer into another buffer, that might be
 * reallocated if the provided space is not sufficient
 * @param
 * @param
 * @return the number of bytes received
 */

size_t oris_protocol_recv(struct evbuffer* input, char** buffer, size_t* buf_size,
	size_t* buf_capacity);

#endif /* __ORIS_PROTOCOL_H */
