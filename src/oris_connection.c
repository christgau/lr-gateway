#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <event2/http.h>

#include "oris_connection.h"
#include "oris_util.h"
#include "oris_protocol.h"
#include "oris_log.h"
#include "oris_socket_connection.h"

oris_connection_t* oris_connection_create(const char* name, oris_protocol_t* protocol)
{
	oris_connection_t* retval;

	if (!protocol) {
		oris_logs(LOG_DEBUG, "no protocol specified, connection will not be created");
		return NULL;
	}

	retval = calloc(1, sizeof(*retval));
	if (!retval) {
		perror("calloc");
		oris_log_f(LOG_DEBUG, "could not create connection %s", name);
		return NULL;
	}

	if (!oris_connection_init(retval, name, protocol)) {
		free(retval);
		return NULL;
	}

	return retval;
}


bool oris_connection_init(oris_connection_t* connection, const char* name, oris_protocol_t* protocol) 
{ 
	connection->write = NULL;
	connection->protocol = protocol;
	connection->name = strdup(name);

	if (connection->name == NULL) {
		perror("strdup");
		oris_log_f(LOG_DEBUG, "could not duplicate connection name %s", name);
		return false;
	}

	connection->destroy = oris_connection_free;

	return true;
}

void oris_connection_send(oris_connection_t* connection, const void* buf, size_t bufsize)
{
	if (connection->protocol) {
		if (connection->protocol->write) {
			connection->protocol->write(buf, bufsize, connection, connection->write);
		}
	}
}

void oris_connection_finalize(oris_connection_t* connection)
{
	if (connection->name) {
		free(connection->name);
	}

	if (connection->protocol && connection->protocol->destroy) {
		connection->protocol->destroy(connection->protocol);
	} else {
		oris_free_and_null(connection->protocol);
	}

	connection->name = NULL;
	connection->write = NULL;
}

void oris_connection_free(oris_connection_t* connection)
{
	oris_connection_finalize(connection);
	free(connection);
}

/*
int oris_connections_parse_config(yaml_event_t event, void* data, int level, bool is_key)
{
	oris_connection_list_t *connections = ((oris_application_info_t*) data)->connections;
	oris_connection_t* connection;

	static bool in_connections = false;
	static char *key = NULL;

	const int CONNECTION_LEVEL = 1;
	const char CONNECTIONS_STR[] = "connections";

	struct evhttp_uri *uri;

	if (level == CONNECTION_LEVEL) {		
		in_connections = (event.type == YAML_SCALAR_EVENT && 
			strcmp((char*) event.data.scalar.value, CONNECTIONS_STR) == 0);
	} if (level == CONNECTION_LEVEL + 1 && in_connections && event.type == YAML_SCALAR_EVENT) {
		if (is_key) {
			key = strdup((char*) event.data.scalar.value);
		} else {
			uri = evhttp_uri_parse((const char*) event.data.scalar.value);
			if (uri) {
				connection = oris_create_connection_from_uri(uri, key, data);
				if (connection) {
					oris_log_f(LOG_INFO, "new connection: %s -> %s", 
						key, event.data.scalar.value);
					oris_connections_add(connections, connection);
				} else {
					oris_log_f(LOG_ERR, "could not create connection %s", key);
				}
			} else {
				oris_log_f(LOG_ERR, "invalid URI for connection %s: %s", 
					key, event.data.scalar.value);
			}
			free(key);
		}
	}

	return 0;
}
*/

void oris_connections_add(oris_connection_list_t* list, oris_connection_t* connection)
{
	oris_connection_t **new_items, **items = list->items;
	
	if (connection) {
		new_items = realloc(items, sizeof(*connection) * (list->count + 1));
		if (new_items) {
			list->items = new_items;
			list->items[list->count] = connection;
			list->count++;
		}
	}
}

void oris_connections_send(oris_connection_list_t* list, const char* proto_name, 
    const void* buf, size_t buf_size)
{
	size_t i;

	for (i = 0; i < list->count; i++) {
		if (list->items[i] && strcmp(list->items[i]->protocol->name, proto_name) == 0) {
			oris_connection_send(list->items[i], buf, buf_size);
		}
	}
}

void oris_connections_clear(oris_connection_list_t* list)
{
	size_t i; 

	for (i = 0; i < list->count; i++) {
		if (list->items[i]) {
			list->items[i]->destroy(list->items[i]);
		}
	}

	list->count = 0;
	oris_free_and_null(list->items);
}


void oris_free_connections(oris_connection_list_t *list)
{
	oris_connections_clear(list);

	free(list->items);
	list->items = NULL;
	list->count = 0;
}




/*
int oris_targets_parse_config(yaml_event_t event, void* data, int level, 
	bool is_key)
{
	oris_application_info_t *info = (oris_application_info_t*) data;
	oris_http_target_t* items;

	static bool in_targets = false;
	static char *key = NULL;

	const int CONNECTION_LEVEL = 1;
	const char CONNECTIONS_STR[] = "targets";

	struct evhttp_uri *uri;

	if (level == CONNECTION_LEVEL) {		
		in_targets = (event.type == YAML_SCALAR_EVENT && 
			strcmp((char*) event.data.scalar.value, CONNECTIONS_STR) == 0);
	} if (level == CONNECTION_LEVEL + 1 && in_targets && event.type == YAML_SCALAR_EVENT) {
		if (is_key) {
			key = strdup((char*) event.data.scalar.value);
		} else {
			items = realloc(info->targets.items, (info->targets.count + 1) * sizeof(*items));
			if (items) {
				info->targets.items = items;
				uri = evhttp_uri_parse((const char*) event.data.scalar.value);
				if (uri) {
					info->targets.items[info->targets.count].name = strdup(key);
					info->targets.items[info->targets.count].uri = uri;
					info->targets.count++;

					oris_log_f(LOG_DEBUG, "new target %s: %s", key, event.data.scalar.value);
				} else {
					oris_log_f(LOG_ERR, "invalid URI for target %s: %s", key, event.data.scalar.value);
				}
			}
			free(key);
		}
	}

	return 0;
}
*/
