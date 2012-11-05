#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <event2/http.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>

#include "oris_log.h"
#include "oris_connection.h"
#include "oris_socket_connection.h"

static const int SOCKET_RECONNECT_TIMEOUT = 10; /* two seconds */

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

	return retval;
}


static void oris_connection_read_cb(struct bufferevent *bev, void *ptr)
{
	oris_connection_t* connection = ptr;

	oris_logs(LOG_DEBUG, "got data\n");
	if (connection->protocol->read_cb) {
		connection->protocol->read_cb(bev, ptr);

	}
}

static void oris_connection_reconnect_timer_cb(evutil_socket_t fd, short event, void* arg)
{
	oris_socket_connection_t* connection = arg;

	event_free(connection->reconnect_timeout_event);

	oris_log_f(LOG_INFO, "connection '%s': reconnect to %s:%d", connection->base.name, 
			evhttp_uri_get_host(connection->uri), 
			evhttp_uri_get_port(connection->uri));
		
	bufferevent_socket_connect_hostname(connection->bufev, 
		connection->libevent_info->dns_base, AF_UNSPEC, 
		evhttp_uri_get_host(connection->uri), 
		evhttp_uri_get_port(connection->uri));
}


static void oris_connection_event_cb(struct bufferevent *bev, short events, void *arg)
{
	struct timeval timeout;
	oris_socket_connection_t* connection = arg;

	if (events & BEV_EVENT_CONNECTED) {
		oris_log_f(LOG_INFO, "connection '%s' connected", connection->base.name);	
		if (connection->base.protocol->connected_cb) {
			connection->base.protocol->connected_cb(connection);
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
			oris_log_f(LOG_ERR, "connection '%s' could not add reconnect event",
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
	connection->libevent_info = info;
	connection->reconnect_timeout_event = NULL;

	/* setup libevent stuff and start attempt to connect to server */
	connection->bufev = bufferevent_socket_new(info->base, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(connection->bufev, oris_connection_read_cb, NULL, oris_connection_event_cb, connection);
	bufferevent_enable(connection->bufev, EV_READ | EV_WRITE);
	bufferevent_socket_connect_hostname(connection->bufev, info->dns_base, AF_UNSPEC, 
		evhttp_uri_get_host(connection->uri), evhttp_uri_get_port(connection->uri));

	return true;
}

void oris_socket_connection_finalize(oris_socket_connection_t* connection)
{
	oris_connection_finalize((oris_connection_t*) connection);
}

void oris_socket_connection_free(oris_connection_t* connection)
{
	if (((oris_socket_connection_t*) connection)->uri != NULL) {
		evhttp_uri_free(((oris_socket_connection_t*) connection)->uri);
	}

	if (((oris_socket_connection_t*) connection)->bufev != NULL) {
		bufferevent_free(((oris_socket_connection_t*) connection)->bufev);
	}

	if (((oris_socket_connection_t*) connection)->reconnect_timeout_event != NULL) {
		event_free(((oris_socket_connection_t*) connection)->reconnect_timeout_event);
	}

	oris_socket_connection_finalize((oris_socket_connection_t*) connection);
	oris_connection_free(connection);
}

