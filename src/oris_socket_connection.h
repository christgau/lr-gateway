#ifndef __ORIS_SOCKET_CONNECTION
#define __ORIS_SOCKET_CONNECTION

#include <stdbool.h>
#include <event2/http.h>
#include <event2/util.h>
#include <event2/bufferevent.h>

#include "oris_connection.h"
#include "oris_protocol.h"
#include "oris_util.h"

#define ORIS_INVALID_SOCKET -1

typedef struct oris_socket_connection {
	oris_connection_t base;
 	struct bufferevent* bufev;
	struct evhttp_uri* uri;
	oris_libevent_base_info_t* libevent_info;
	
	struct event* reconnect_timeout_event;
    
} oris_socket_connection_t;

typedef struct oris_server_connection {
	oris_socket_connection_t base;
	struct event* listen_event;
	evutil_socket_t socket;
} oris_server_connection_t;


/* create and init a new connection */
oris_socket_connection_t* oris_socket_connection_create(const char* name,
		oris_protocol_t* protocol, struct evhttp_uri *uri,
		oris_libevent_base_info_t *info);

/* init a new connection */
bool oris_socket_connection_init(oris_socket_connection_t* connection, const
		char *name, oris_protocol_t* protocol, struct evhttp_uri *uri, 
		oris_libevent_base_info_t *info);

/* finalize (close socket) */
void oris_socket_connection_finalize(oris_socket_connection_t* connection);

/* (free) */
void oris_socket_connection_free(oris_connection_t* connection);

/* create a server socket */
oris_server_connection_t* oris_serover_connection_create(const char* name,
		struct evhttp_uri *uri,	oris_libevent_base_info_t *info);

/* destructor for server socket */
void oris_server_connection_free(oris_connection_t* connection);

/* get a connection from the given uri (protocol is established as well) */
oris_connection_t* oris_create_connection_from_uri(struct evhttp_uri* uri,
		const char* connection_name, void* data);

#endif /* __ORIS_SOCKET_CONNECTION */
