#ifndef __ORIS_CONNECTION_H
#define __ORIS_CONNECTION_H

#include <stdbool.h>
#include <event2/http.h>
#include <yaml.h>

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
} oris_connection_t;

typedef struct oris_connection_list {
	oris_connection_t** items;
	size_t count;
} oris_connection_list_t;

typedef struct oris_http_target {
	char* name;
	struct evhttp_uri* uri;
} oris_http_target_t;

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
 * clear the connection list
 */
void oris_connections_clear(oris_connection_list_t* list);

/**
 * oris_connections_parse_config
 *
 * parses the configuration and creates connections accordingly
 */
int oris_connections_parse_config(yaml_event_t event, void* data, int level, 
	bool is_key);


void oris_targets_clear(oris_http_target_t* targets, int *count);

int oris_targets_parse_config(yaml_event_t event, void* data, int level, 
	bool is_key);

#endif /* __ORIS_CONNECTION_H */
