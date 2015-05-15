#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

#include "oris_log.h"
#include "oris_util.h"
#include "oris_app_info.h"
#include "oris_configuration.h"
#include "oris_automation.h"

int oris_main_default(oris_application_info_t *info)
{
	if (!oris_app_info_init(info)) {
		oris_log_f(LOG_CRIT, "could not init application (see above). Exiting");
		return EXIT_FAILURE;
	}

	oris_automation_init(info);
	oris_interpreter_init(&info->data_tables);
	oris_configuration_init();

	if (!oris_load_configuration(info)) {
		oris_log_f(LOG_ERR, "failed to load configuration");
		return EXIT_FAILURE;
	}

	if (info->connections.count == 0) {
		oris_log_f(LOG_ERR, "no connections available. Exiting.\n");
		return EXIT_FAILURE;
	}

	/* run */
	event_base_dispatch(info->libevent_info.base);

	/* cleanup */
	oris_configuration_finalize();
	oris_interpreter_finalize();
	oris_automation_finalize();
	oris_app_info_finalize(info);

#if LIBEVENT_VERSION_NUMBER >= 0x02010100
	oris_log_f(LOG_DEBUG, "LibEvent global shutdown.");
	libevent_global_shutdown();
#endif

	oris_log_f(LOG_INFO, "Done.");

	return EXIT_SUCCESS;
}


int oris_main_print_version(oris_application_info_t *info)
{
	printf("oris gateway version %s with libevent %s\n", ORIS_VERSION, event_get_version());

	(void) info;
	return EXIT_SUCCESS;
}

int oris_print_usage(oris_application_info_t* info)
{
	printf("usage %s [options]\n\n", info->argv[0]);
	printf("options: \n\t--verbose\t - verbose output (same as info loglevel)\n");
	printf("\t--loglevel level - sets the loglevel to error, warn, info or debug\n");
	printf("\t--config file\t - use given file to read configuration\n");
	printf("\t--datafile file\t - loads data from a CP file\n");
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
		{ "loglevel", required_argument, NULL, 'l' },
		{ "datafile", required_argument, NULL, 'd' },
		{ "config", required_argument, NULL, 'c' },
		{ "cert", required_argument, NULL, 'C' },
/*		{ "logfile", no_argument, NULL, 'L' },*/
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	const char* short_opt_str = "vVl:d:c:C:h?";

	opt_code = getopt_long(info->argc, info->argv, short_opt_str, long_opts, &opt_idx);
	while (opt_code != -1) {
		switch (opt_code) {
			case 'v':
				info->log_level = LOG_INFO;
				oris_init_log(NULL, info->log_level);
				break;
			case 'V':
				retval = true;
				info->main = &oris_main_print_version;
				break;
			case 'l':
				if (strcmp(optarg, "debug") == 0) {
					info->log_level = LOG_DEBUG;
				} else if (strcmp(optarg, "info") == 0) {
					info->log_level = LOG_INFO;
				} else if (strcmp(optarg, "warn") == 0) {
					info->log_level = LOG_WARNING;
				} else if (strcmp(optarg, "error") == 0) {
					info->log_level = LOG_ERR;
				} else {
					fprintf(stderr, "invalid log level %s\n", optarg);
					info->main = &oris_print_usage;
					retval = true;
				}
				oris_init_log(NULL, info->log_level);
				break;
			case 'd':
				oris_tables_load_from_file(&info->data_tables, optarg);
				break;
			case 'c':
				info->config_fn = strdup(optarg);
				break;
			case 'C':
				info->cert_fn = strdup(optarg);
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
		opt_code = getopt_long(info->argc, info->argv, short_opt_str, long_opts, &opt_idx);
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
	oris_application_info_t info;

	memset(&info, 0, sizeof(info));
	info.argc = argc;
	info.argv = argv;
	info.config_fn = NULL;
	info.cert_fn = NULL;

	info.log_level = LOG_ERR;
	oris_init_log(NULL, info.log_level);
	oris_tables_init(&info.data_tables);

	retval = EXIT_SUCCESS;

	if (oris_handle_args(&info) == true) {
		if (info.main) {
			/* debug move causes memory leak in bufferevents */
			/* event_enable_debug_mode();*/
			retval = info.main(&info);
		}
	} else {
		retval = oris_print_usage(&info);
	}

	oris_finalize_log();

	return retval;
}
