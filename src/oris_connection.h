#ifndef __ORIS_CONNECTION_H
#define __ORIS_CONNECTION_H

#include <stdbool.h>
#include <event2/http.h>

#include "oris_protocol.h"

typedef struct oris_connection { 
	/* name of the connection */
	char* name;
	/* statistical data */
	size_t bytesIn;
	size_t bytesOut;
	/* protocol which is used for this connection */
	struct oris_protocol* protocol;
	/* pointer to destructor */
	void (*destroy)(struct oris_connection*);
	/* this is a virtual method that actually writes to the connection */
	oris_connection_write_fn_t write;
} oris_connection_t;

typedef struct oris_connection_list {
	oris_connection_t** items;
	size_t count;
} oris_connection_list_t;

/* life-cycle functions */

/**
 * oris_connection_new
 *
 * create a new connection with the given name using the specified protocol
 * 
 * @param name char* 
 * @param protocol oris_protocol_t
 * @return oris_connection_t* 
 */
oris_connection_t* oris_connection_create(const char* name, struct oris_protocol* protocol);

/**
 * initialize connection
 * @return bool true if initialization was successful
 */
bool oris_connection_init(oris_connection_t* connection, const char* name, 
		struct oris_protocol* protocol);


/**
 * send data via protocol (encapsulate stuff) and connection (actual media transfer)
 */
void oris_connection_send(oris_connection_t* connection, const void* buf, size_t buf_size);

/**
 * finalize connection
 */
void oris_connection_finalize(oris_connection_t* connection);


/**
 * oris_connection_free
 *
 * destroys an existing connection
 * @param connection oris_connection_t* the connection to be destroyed
 */
void oris_connection_free(oris_connection_t* connection);


/**
 * add a connection to the list
 * @param connections oris_connection_list_t* list of connections
 * @param connection oris_connection_t* to be added
 */
void oris_connections_add(oris_connection_list_t* list, oris_connection_t* connection);

/**
 * sends the buffer/buf_size to all connections having a protocol with the given name 
 */
void oris_connections_send(oris_connection_list_t* list, const char* proto_name, 
    const void* buf, size_t buf_size);

/**
 * clear the connection list
 */
void oris_connections_clear(oris_connection_list_t* list);

/* clear and free list  */
void oris_free_connections(oris_connection_list_t *list);

#endif /* __ORIS_CONNECTION_H */
