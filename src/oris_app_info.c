#include <stdlib.h>
#include <string.h>

#include "oris_log.h"
#include "oris_app_info.h"
#include "oris_socket_connection.h"

/* forwards */
static void oris_targets_clear(oris_http_target_t* targets, int *count);

/* definitons */
void oris_app_info_init(oris_application_info_t* info)
{
	info->targets.items = NULL;
	info->targets.count = 0;
}

void oris_app_info_finalize(oris_application_info_t* info)
{
	oris_targets_clear(info->targets.items, &info->targets.count);

	free(info->targets.items);
	info->targets.items = NULL;

	oris_free_connections(&info->connections);
}

void oris_config_add_target(oris_application_info_t* config, const char* name, const char* uri)
{
	oris_http_target_t* items;
	struct evhttp_uri *evuri;
	
	evuri = evhttp_uri_parse(uri);
	if (evuri) {
		items = realloc(config->targets.items, (config->targets.count + 1) * sizeof(*items));
		if (items) {
			config->targets.items = items;
			config->targets.items[config->targets.count].name = strdup(name);
			config->targets.items[config->targets.count].uri = evuri;
			config->targets.items[config->targets.count].bev = 
				bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

            config->targets.items[config->targets.count].connection = 
                evhttp_connection_base_new(config->libevent_info.base, config->libevent_info.dns_base,
					config->targets.items[config->targets.count].bev, evhttp_uri_get_host(evuri),
					evhttp_uri_get_porr(evuri));

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
		if (targets[i].uri) {
			evhttp_uri_free(targets[i].uri);
			targets[i].uri = NULL;
		}

		if (targets[i].name) {
			free(targets[i].name);
			targets[i].name = NULL;
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
