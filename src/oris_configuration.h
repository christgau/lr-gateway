#ifndef __ORIS_CONFIGURATION_H
#define __ORIS_CONFIGURATION_H

#include <stdbool.h>
#include <antlr3treeparser.h>

#include "oris_app_info.h"
#include "oris_automation_types.h"

/* load configuration from the config file */
bool oris_load_configuration(oris_application_info_t* info);

/* init finalization stuff */
void oris_configuration_init(void);
void oris_configuration_finalize(void);

void oris_configuration_perform_automation(oris_automation_event_t* event, oris_application_info_t* info);

pANTLR3_BASE_TREE oris_get_request_tree(const char* name);
char* oris_parse_request_tree(const pANTLR3_BASE_TREE parse_tree);
char* oris_get_parsed_request(const char *name);

#endif /* __ORIS_CONFIGURATION_H */
