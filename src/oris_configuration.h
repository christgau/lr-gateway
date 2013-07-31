#ifndef __ORIS_CONFIGURATION_H
#define __ORIS_CONFIGURATION_H

#include <stdbool.h>
#include <antlr3treeparser.h>
#include <antlr3string.h>
#include <event2/buffer.h>

#include "oris_app_info.h"
#include "oris_automation_types.h"
#include "oris_table.h"

/* load configuration from the config file */
bool oris_load_configuration(oris_application_info_t* info);

/* init finalization stuff */
void oris_configuration_init(void);
void oris_configuration_finalize(void);

void oris_configuration_perform_automation(oris_automation_event_t* event, oris_application_info_t* info);

pANTLR3_BASE_TREE oris_get_request_tree(const char* name);
pANTLR3_BASE_TREE oris_get_request_parse_tree(const char *name);
pANTLR3_BASE_TREE oris_get_template_by_name(pANTLR3_STRING name);

#endif /* __ORIS_CONFIGURATION_H */
