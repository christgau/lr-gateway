#include <stdlib.h>
#include <stdbool.h>

#ifndef _WINDOWS
#include <getopt.h>
#include <signal.h>
#else 
#include <unistd.h>
#endif

#include <event2/event.h>
#include <event2/dns.h>

#include "oris_log.h"
#include "oris_util.h"
#include "oris_connection.h"
#include "oris_configuration.h"
#include "oris_protocol.h"
#include "oris_socket_connection.h"

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
}

static int oris_init_libevent(struct oris_application_info *info)
{
	info->libevent_info.base = event_base_new();
	info->libevent_info.dns_base = evdns_base_new(info->libevent_info.base, true);

	if (info->libevent_info.base == NULL || info->libevent_info.dns_base == NULL) {
		return EXIT_FAILURE;
	} else {
		info->sigint_event = evsignal_new(info->libevent_info.base, SIGINT, oris_sigint_cb, 
			info->libevent_info.base);
		event_set_log_callback(oris_libevent_log_cb);
		event_add(info->sigint_event, NULL);

		return EXIT_SUCCESS;
	}
}

void oris_free_connections(oris_connection_list_t *list)
{
	oris_connections_clear(list);

	free(list->items);
	list->items = NULL;
	list->count = 0;
}


int oris_main_default(oris_application_info_t *info)
{
	/* load http targets */
	oris_config_parse(&oris_targets_parse_config, info);
	if (info->targets.count == 0) {
		oris_log_f(LOG_ERR, "no http targets defined. Exiting");
		return EXIT_FAILURE;
	}

	/* prepare libevent stuff */
	if (oris_init_libevent(info)) {
		oris_log_f(LOG_ERR, "could not init libevent. Exiting\n");
		return EXIT_FAILURE;
	}

	/* load, init and open connections */
	info->connections = (oris_connection_list_t*) calloc(1, sizeof(oris_connection_list_t));
	oris_config_parse(&oris_connections_parse_config, info);
	if (info->connections->count == 0) {
		oris_log_f(LOG_ERR, "no connections available. Exiting.\n");
		return EXIT_FAILURE;
	}

	/* run */
	event_base_dispatch(info->libevent_info.base);

	/* cleanup */
	oris_free_connections(info->connections);
	free(info->connections);

	oris_targets_clear(info->targets.items, &(info->targets.count));
	free(info->targets.items);

	event_free(info->sigint_event);
	evdns_base_free(info->libevent_info.dns_base, 0);
	event_base_free(info->libevent_info.base);

	oris_logs(LOG_INFO, "finished.");

#if LIBEVENT_VERSION_NUMBER >= 0x02010100 
	libevent_global_shutdown();
#endif

	return EXIT_SUCCESS;
}


int oris_main_print_version(oris_application_info_t *info)
{
	printf("oris gateway version %s with libevent %s\n", ORIS_VERSION, event_get_version());

	return EXIT_SUCCESS;
}

int oris_print_usage(oris_application_info_t* info)
{
	printf("usage %s [options]\n\n", info->argv[0]);
	printf("options: \n\t--verbose\t - verbose output\n");
	printf("\t--version\t - print version and exit\n");
	printf("\t--help   \t - print this help\n");

	return EXIT_FAILURE;
}


bool oris_handle_args(oris_application_info_t *info)
{
	int opt_idx, opt_code;
	bool retval = false;

	static struct option long_opts[] = {
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
/*		{ "logfile", no_argument, NULL, 'L' },*/
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	opt_code = getopt_long(info->argc, info->argv, "vVlh?", long_opts, &opt_idx);
	while (opt_code != -1) {
		switch (opt_code) {
			case 'v':
				info->log_level = LOG_INFO;
				break;
			case 'V':
				retval = true;
				info->main = &oris_main_print_version;
				break;
			case '?':
			case 'h':
				retval = true;
				info->main = &oris_print_usage; 
				break;
			default:
				fprintf(stderr, "unregonized option (%d)\n", opt_code);
				break;
		}
		opt_code = getopt_long(info->argc, info->argv, "vVlh", long_opts, &opt_idx);
	}

	if (retval == false) {
		info->main = oris_main_default;
		retval = true;
	}

	return retval;
}


int main(int argc, char **argv)
{
	int retval;
	oris_application_info_t info = { { NULL, NULL } };

	info.argc = argc;
	info.argv = argv;

	retval = EXIT_SUCCESS;

	if (oris_handle_args(&info) == true) {
		if (info.main != NULL) {
			event_enable_debug_mode();
			oris_init_log(NULL, info.log_level);
			retval = info.main(&info);
		}
	} else {
		retval = oris_print_usage(&info);
	}

	oris_finalize_log();

	return retval;
}
