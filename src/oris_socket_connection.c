#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define close _close
#endif

#include <event2/http.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>

#include "oris_log.h"
#include "oris_connection.h"
#include "oris_socket_connection.h"
#include "oris_protocol.h"

#define MAX_LINE_SIZE 4096

static const int SOCKET_RECONNECT_TIMEOUT = 10; /* two seconds */

static void oris_socket_connection_write(const void* connection, const void* buf, const size_t bufsize);
static void oris_server_connection_do_accept(evutil_socket_t listener, short event, void *ctx); 
static void oris_server_socket_event_cb(struct bufferevent *bev, short event, void *ctx);
static void oris_connection_event_cb(struct bufferevent *bev, short events, void *arg);
static void oris_create_client_socket(oris_socket_connection_t* connection);

oris_socket_connection_t* oris_socket_connection_create(const char* name,
		oris_protocol_t* protocol, struct evhttp_uri *uri,
		oris_libevent_base_info_t *info)
{
	oris_socket_connection_t* retval;

	if (!protocol) {
		oris_log_f(LOG_DEBUG, "no protocol specified, connection will not be created");
		return (oris_socket_connection_t*) NULL;
	}

	retval = calloc(1, sizeof(*retval));
	if (!retval) {
		perror("calloc");
		oris_log_f(LOG_DEBUG, "could not create connection %s", name);
		return NULL;
	}

	if (!oris_socket_connection_init(retval, name, protocol, uri, info)) {
		free(retval);
		return NULL;
	}

	retval->base.write = oris_socket_connection_write;

	return retval;
}

static void oris_socket_connection_write(const void* connection, const void* buf, 
	const size_t bufsize)
{
	bufferevent_write(((oris_socket_connection_t*) connection)->bufev, 
		buf, bufsize);
}

static void oris_connection_read_cb(struct bufferevent *bev, void *ptr)
{
	oris_connection_t* connection = ptr;

	if (connection->protocol->read_cb) {
		connection->protocol->read_cb(bev, ptr);

	}
}

static void oris_create_client_socket(oris_socket_connection_t* connection)
{
	if (connection->bufev) {
		bufferevent_free(connection->bufev);
	}

	connection->bufev = bufferevent_socket_new(
			connection->libevent_info->base, -1, 
			BEV_OPT_CLOSE_ON_FREE);

	bufferevent_setcb(connection->bufev, oris_connection_read_cb, NULL, 
			oris_connection_event_cb, connection);
	bufferevent_enable(connection->bufev, EV_READ | EV_WRITE);
	bufferevent_socket_connect_hostname(connection->bufev, 
		connection->libevent_info->dns_base, AF_UNSPEC, 
		evhttp_uri_get_host(connection->uri), 
		evhttp_uri_get_port(connection->uri));
}

static void oris_connection_reconnect_timer_cb(evutil_socket_t fd, short event, void* arg)
{
	oris_socket_connection_t* connection = arg;

	event_free(connection->reconnect_timeout_event);
	connection->reconnect_timeout_event = NULL;

	oris_log_f(LOG_INFO, "connection '%s': reconnecting to %s:%d", 
			connection->base.name, 
			evhttp_uri_get_host(connection->uri), 
			evhttp_uri_get_port(connection->uri));

	oris_create_client_socket(connection);

	fd = fd;
	event = event;
}


static void oris_connection_event_cb(struct bufferevent *bev, short events, void *arg)
{
	struct timeval timeout;
	oris_socket_connection_t* connection = arg;

	oris_log_f(LOG_DEBUG, "event connection status %d", events);
	if (events & BEV_EVENT_CONNECTED) {
		oris_log_f(LOG_INFO, "connection '%s' connected", connection->base.name);	
		if (connection->base.protocol->connected_cb) {
			connection->base.protocol->connected_cb(connection->base.protocol);
		}
	} else if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
		if (events & BEV_EVENT_ERROR) {
			int err = bufferevent_socket_get_dns_error(bev);
			if (err)
				oris_log_f(LOG_WARNING, "connection '%s': DNS error: %s", 
					connection->base.name, evutil_gai_strerror(err));
			else
				oris_log_f(LOG_DEBUG, "connection '%s': other error: proberbly not connected", 
					connection->base.name);
		}
		if (events & BEV_EVENT_EOF && connection->base.protocol->disconnected_cb) {
			connection->base.protocol->disconnected_cb();
		}
		oris_log_f(LOG_DEBUG, "connection '%s' trying to reconnect to %s %d in %d secs", 
			connection->base.name,
			evhttp_uri_get_host(connection->uri), 
			evhttp_uri_get_port(connection->uri), 
			SOCKET_RECONNECT_TIMEOUT);

		/* start timer for reconnect attempts */
		connection->reconnect_timeout_event = evtimer_new(connection->libevent_info->base,
				oris_connection_reconnect_timer_cb, arg);

		evutil_timerclear(&timeout);
		timeout.tv_sec = SOCKET_RECONNECT_TIMEOUT;

		if (event_add(connection->reconnect_timeout_event, &timeout)) {
			oris_log_f(LOG_ERR, "connection '%s' could not add reconnect event. connection lost!",
				connection->base.name);
		}
	} else {
		oris_log_f(LOG_DEBUG, "connection '%s' event occurred: %x", connection->base.name, events);
	}
}

bool oris_socket_connection_init(oris_socket_connection_t* connection, const char *name, 
		oris_protocol_t* protocol, struct evhttp_uri *uri, 
		oris_libevent_base_info_t *info)
{
	if (!oris_connection_init((oris_connection_t*) connection, name, protocol)) {
		return false;
	}

	/* override destructor */
	connection->base.destroy = oris_socket_connection_free;

	connection->uri = uri;
	connection->bufev = NULL;
	connection->libevent_info = info;
	connection->reconnect_timeout_event = NULL;

	/* setup libevent stuff and start attempt to connect to server */
	oris_create_client_socket(connection);

	return true;
}

void oris_socket_connection_finalize(oris_socket_connection_t* connection)
{
	oris_socket_connection_t* sc = (oris_socket_connection_t*) connection;

	if (sc->uri) {
		evhttp_uri_free(sc->uri);
		sc->uri = NULL;
	}

	if (sc->bufev) {
		bufferevent_free(sc->bufev); /* causes leaks/problems? */
		sc->bufev = NULL;
	}

	if (sc->reconnect_timeout_event) {
		event_free(sc->reconnect_timeout_event);
		sc->reconnect_timeout_event = NULL;
	}
}

void oris_socket_connection_free(oris_connection_t* connection)
{
	oris_socket_connection_finalize((oris_socket_connection_t*) connection);
	oris_connection_free(connection);
}

oris_server_connection_t* oris_server_connection_create(const char* name,
		struct evhttp_uri *uri,	oris_libevent_base_info_t *info)
{
	struct sockaddr_in svr_addr;
	oris_server_connection_t* retval;
	int status;
	
	retval = calloc(1, sizeof(*retval));
	if (!retval) {
		perror("calloc");
		oris_log_f(LOG_ERR, "could not create connection '%s'", name);
		return NULL;
	}

	if (!oris_connection_init((oris_connection_t*) retval, name, NULL)) {
		oris_log_f(LOG_ERR, "could not init the connection '%s'", name);
	}

	((oris_connection_t*) retval)->destroy = oris_server_connection_free;

	retval->base.uri = uri;
	retval->base.libevent_info = info;

	memset(&svr_addr, 0, sizeof(svr_addr));
	svr_addr.sin_family = AF_INET;
	svr_addr.sin_addr.s_addr = 0;
	svr_addr.sin_port = htons((unsigned short) evhttp_uri_get_port(uri));

	retval->socket = socket(AF_INET, SOCK_STREAM, 0);
	if (retval->socket == -1) {
		perror("socket");
		oris_log_f(LOG_ERR, "could not create sever socket for connection '%s'", name);
		free(retval);
		return NULL;
	}

	evutil_make_socket_nonblocking(retval->socket);
#ifndef _WIN32
	{
		int one = 1;
		setsockopt(retval->socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	}
#endif

	status = bind(retval->socket, (struct sockaddr*) &svr_addr, sizeof(svr_addr));
	if (status < 0) {
		perror("bind");
		oris_log_f(LOG_ERR, "could not bind %s to port %d (%d)", name, ntohs(svr_addr.sin_port), status);
		free(retval);
		return NULL;
	}

	if (listen(retval->socket, 8) < 0) {
		perror("listen");
		oris_log_f(LOG_ERR, "listen for %s failed", name);
	}

	retval->listen_event = event_new(info->base, retval->socket, EV_READ | EV_PERSIST,
		oris_server_connection_do_accept, (void*) retval);
	if (retval->listen_event == NULL) {
		oris_log_f(LOG_ERR, "could not create event struct for %s", name);
		evutil_closesocket(retval->socket);
		free(retval);
		return NULL;
	}

	event_add(retval->listen_event, NULL);
	oris_log_f(LOG_INFO, "server connection '%s' ready", name);
	return retval;
}

static void oris_server_connection_do_accept(evutil_socket_t listener, short event, void *arg)
{
	oris_server_connection_t* connection = (oris_server_connection_t*) arg;
	struct event_base* base = ((oris_socket_connection_t*) (connection))->libevent_info->base;
	struct sockaddr_storage addr;
	socklen_t slen = sizeof(addr);
	evutil_socket_t socket;

	oris_log_f(LOG_INFO, "new connection on %s", connection->base.base.name);
	socket = accept(listener, (struct sockaddr*)&addr, &slen); 
	if (socket < 0) {
		perror("accept");
		oris_log_f(LOG_ERR, "could not accept new client from %s",
				((oris_connection_t*) connection)->name);
	} else {
		struct bufferevent* bev;

		oris_log_f(LOG_DEBUG, "accepted connection");
		evutil_make_socket_nonblocking(socket);
		bev = bufferevent_socket_new(base, socket, BEV_OPT_CLOSE_ON_FREE);
		bufferevent_setcb(bev, ((oris_connection_t*) connection)->protocol->read_cb, 
				NULL, oris_server_socket_event_cb, connection);
		bufferevent_setwatermark(bev, EV_READ, 0, MAX_LINE_SIZE);
		bufferevent_enable(bev, EV_READ | EV_WRITE);
		if (((oris_connection_t*) connection)->protocol->connected_cb) {
		/*	((oris_connection_t*) connection)->protocol->connected_cb(((oris_connection_t*) connection)->protocol);*/
			((oris_connection_t*) connection)->protocol->connected_cb((void*) bev);
		}
	}

	event = event;
}

void oris_server_connection_free(oris_connection_t* connection)
{
	evutil_closesocket(((oris_server_connection_t*) connection)->socket);
	if (((oris_server_connection_t*) connection)->listen_event) {
		event_free(((oris_server_connection_t*) connection)->listen_event);
	}

	oris_socket_connection_free(connection);
}

oris_connection_t* oris_create_connection_from_uri(struct evhttp_uri* uri,
		const char* connection_name, void* data)
{
	oris_connection_t* retval = NULL;
	oris_protocol_t* protocol = oris_get_protocol_from_scheme(evhttp_uri_get_scheme(uri), data);
	const char* scheme = evhttp_uri_get_scheme(uri);

	if (protocol == NULL) { 
		return NULL;
	}

	if (strcmp(scheme, "ctrl") == 0) {
		retval = (oris_connection_t*) oris_server_connection_create(connection_name, uri, data);
		if (retval) {
			retval->protocol = protocol;
		}
	} else if (strcmp(scheme, "data") == 0) {
		retval = (oris_connection_t*) oris_socket_connection_create(connection_name, protocol, uri, data);
	}

	return retval;
}

static void oris_server_socket_event_cb(struct bufferevent *bev, short event, void *ctx)
{
	oris_log_f(LOG_DEBUG, "event on server connection %d\n", event);

	if (event & BEV_EVENT_EOF) {
		oris_log_f(LOG_INFO, "connection to client closed");
	} else if (event & BEV_EVENT_ERROR) {

	} else if (event & BEV_EVENT_TIMEOUT) {

	}

	ctx = ctx;
	bufferevent_free(bev);
}

