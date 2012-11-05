#ifndef __ORIS_CONFIGURATION
#define __ORIS_CONFIGURATION

#include <stdbool.h>
#include <yaml.h>
#include "oris_util.h"

extern const oris_error_t ORIS_YAML_ERROR;
extern const oris_error_t ORIS_CONFIG_ERROR;

/** callback invoked by the parser */
typedef int(*oris_config_parse_cb_t)(yaml_event_t event, void* data, int level, bool is_key);

/** parses a config file */
int oris_config_parse(oris_config_parse_cb_t callback, void* data);

#endif /* __ORIS_CONFIGURATION */
