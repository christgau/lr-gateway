#include <stdlib.h>
#include <string.h>
#include <event2/bufferevent.h>
#include <event2/http.h>

#ifndef _WINDOWS
#include <signal.h>
#else
#include <unistd.h>
#endif

#include <event2/event.h>
#include <event2/dns.h>

#include "oris_log.h"
#include "oris_app_info.h"
#include "oris_socket_connection.h"

/* forwards */
static void oris_targets_clear(oris_http_target_t* targets, int *count);

/* definitons */
static void oris_libevent_log_cb(int severity, const char* msg)
{
	int sev;

	puts(msg);
	switch (severity) {
		case _EVENT_LOG_DEBUG:
			sev = LOG_DEBUG;
			break;
		case _EVENT_LOG_MSG:
			sev = LOG_INFO;
			break;
		case _EVENT_LOG_WARN:
			sev = LOG_WARNING;
			break;
		case _EVENT_LOG_ERR:
			sev = LOG_ERR;
			break;
		default: 
			sev = LOG_WARNING;
	}

	oris_logs(sev, msg);
}

static void oris_sigint_cb(evutil_socket_t fd, short type, void *arg)
{
	struct event_base *base = arg;

	oris_logs(LOG_INFO, "Received SIGINT. Cleaning up and exiting.\n");
	fflush(stdout);

	event_base_loopbreak(base);

	/* keep compiler happy */
	fd = fd;
	type = type;
}

static bool oris_init_libevent(struct oris_application_info *info)
{
	info->libevent_info.base = event_base_new();
	info->libevent_info.dns_base = evdns_base_new(info->libevent_info.base, true);

	if (info->libevent_info.base == NULL || info->libevent_info.dns_base == NULL) {
		oris_log_f(LOG_CRIT, "could not init libevent");
		return false;
	} else {
		info->sigint_event = evsignal_new(info->libevent_info.base, SIGINT, oris_sigint_cb, 
			info->libevent_info.base);
		event_set_log_callback(oris_libevent_log_cb);
		event_add(info->sigint_event, NULL);

		return true;
	}
}

bool oris_app_info_init(oris_application_info_t* info)
{
	info->targets.items = NULL;
	info->targets.count = 0;

	oris_tables_init(&info->data_tables);

	return oris_init_libevent(info);
}

void oris_app_info_finalize(oris_application_info_t* info)
{
	oris_tables_finalize(&info->data_tables);
	oris_targets_clear(info->targets.items, &info->targets.count);

	free(info->targets.items);
	info->targets.items = NULL;

	oris_free_connections(&info->connections);

	event_free(info->sigint_event);
	evdns_base_free(info->libevent_info.dns_base, 0);
	event_base_free(info->libevent_info.base);
}

void oris_config_add_target(oris_application_info_t* config, const char* name, const char* uri)
{
	oris_http_target_t* items;
	struct evhttp_uri *evuri;
	int port;

	evuri = evhttp_uri_parse(uri);
	if (evuri) {
		items = realloc(config->targets.items, (config->targets.count + 1) * sizeof(*items));
		port = evhttp_uri_get_port(evuri);
		if (port == -1) {
			if (strcasecmp(evhttp_uri_get_scheme(evuri), "https") == 0) {
				evhttp_uri_set_port(evuri, 443);
			} else {
				evhttp_uri_set_port(evuri, 80);
			}
		}
		oris_log_f(LOG_INFO, "http port is %d", evhttp_uri_get_port(evuri));

		if (items) {
			config->targets.items = items;
			config->targets.items[config->targets.count].name = strdup(name);
			config->targets.items[config->targets.count].uri = evuri;
			config->targets.items[config->targets.count].bev = 
				bufferevent_socket_new(config->libevent_info.base, -1, BEV_OPT_CLOSE_ON_FREE);
            config->targets.items[config->targets.count].connection = 
                evhttp_connection_base_bufferevent_new(config->libevent_info.base, 
					config->libevent_info.dns_base, 
					config->targets.items[config->targets.count].bev, 
					evhttp_uri_get_host(evuri),
					evhttp_uri_get_port(evuri));

			config->targets.count++;
		}
		oris_log_f(LOG_DEBUG, "new target %s: %s", name, uri);
	} else {
		oris_log_f(LOG_WARNING, "invalid URI for target %s: %s. Skipping.", name, uri);
	}
}

void oris_targets_clear(oris_http_target_t* targets, int *count)
{
	int i;

	for (i = 0; i < *count; i++) {
		if (targets[i].uri != NULL) {
			evhttp_uri_free(targets[i].uri);
			targets[i].uri = NULL;
		}

		if (targets[i].name != NULL) {
			free(targets[i].name);
			targets[i].name = NULL;
		}

		if (targets[i].connection != NULL) {
			evhttp_connection_free(targets[i].connection);
			targets[i].connection = NULL;
		}
	}

	*count = 0;
}

void oris_create_connection(oris_application_info_t* info, const char* name, oris_parse_expr_t* e)
{
	char* v_str = oris_expr_as_string(e);
	struct evhttp_uri *uri = evhttp_uri_parse((const char*) v_str);

	if (uri) {
		oris_connection_t* c = oris_create_connection_from_uri(uri, name, (void*) info);
		if (c != NULL) {
			oris_log_f(LOG_DEBUG, "new connection \%s -> \%s", name, v_str);
			oris_connections_add(&info->connections, c);
			/* uri is freed by connection */
		} else {
			evhttp_uri_free(uri);
		}
	} else {
		oris_log_f(LOG_ERR, "invalid uri for connection \%s: \%s", name, v_str);
	}

	free(v_str);
}
