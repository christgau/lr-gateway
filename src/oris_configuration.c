#include <errno.h>
#include <stdio.h>

#include "oris_configuration.h"
#include "oris_util.h"

const oris_error_t ORIS_YAML_ERROR = 0x80002001;
const oris_error_t ORIS_CONFIG_ERROR = 0x80002002;

#ifdef _WINDOWS
#include <windows.h>
#define PATH_DELIMITER '\\'
#define CONFIG_SUBPATH "" 
#else
#define PATH_DELIMITER '/'
#define CONFIG_SUBPATH "/etc"
#endif /* _WINDOWS */

const char* ORIS_GW_CFG_DEFAULT_FILENAME = "gateway.config.yaml";

#ifndef MAX_PATH
#define MAX_PATH 256
#endif /* MAX_PATH */

static FILE* oris_open_configfile(void)
{
	char fname[MAX_PATH], *basepath;
	FILE* retval;

#ifdef _WINDOWS
	basepath = calloc(MAX_PATH, sizeof(*defaultpath));
	/* get directory of the application */
#else
	basepath = getenv("HOME");
#endif
	if (basepath == NULL) {
		return false;
	}

	snprintf(fname, MAX_PATH - 1, "%s%s%c%s", basepath, CONFIG_SUBPATH, PATH_DELIMITER, ORIS_GW_CFG_DEFAULT_FILENAME);
	retval = fopen(fname, "rb");
	if (retval) {
		goto exit_fn;
	}

#ifndef _WINDOWS
	/* next try: /etc file on non-Windows systems */
	snprintf(fname, MAX_PATH, "/etc/%s", ORIS_GW_CFG_DEFAULT_FILENAME);
	retval = fopen(fname, "rb");
#endif
exit_fn:
#ifdef _WINDOWS
	free(fname);
#endif
	return retval;
}

int oris_config_parse(oris_config_parse_cb_t callback, void* data)
{
	/* taken from http://pyyaml.org/wiki/LibYAML */ 
	yaml_parser_t parser;
	yaml_event_t event;
	bool done = false, is_key = false;
	int level = 0;
	FILE *input = oris_open_configfile();

	if (callback == NULL) {
		return ORIS_EINVALID_ARG;
	}

	if (input == NULL) {
		return errno;
	}

	if (yaml_parser_initialize(&parser) == 0) {
		return ORIS_YAML_ERROR;
	}

	yaml_parser_set_input_file(&parser, input);

	while (!done) {
		if (!yaml_parser_parse(&parser, &event)) {
			fclose(input);
			yaml_parser_delete(&parser);
			return ORIS_YAML_ERROR;
		}

		if (event.type == YAML_MAPPING_END_EVENT || event.type == YAML_MAPPING_START_EVENT) {
			is_key = true;
			level += event.type == YAML_MAPPING_END_EVENT ? -1 : 1;
		}

		if (callback(event, data, level, is_key) != 0) {
			yaml_event_delete(&event);
			break;
		}

		if (event.type == YAML_SCALAR_EVENT) {
			is_key = !is_key;
		}

		done = (event.type == YAML_STREAM_END_EVENT);
		yaml_event_delete(&event);
	}

	fclose(input);
	yaml_parser_delete(&parser);

	return ORIS_SUCCESS;
}


