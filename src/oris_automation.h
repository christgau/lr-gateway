#ifndef __ORIS_AUTOMATION_H
#define __ORIS_AUTOMATION_H

#include <stdbool.h>
#include <event2/http.h>
#include <antlr3commontree.h>

#include "oris_app_info.h"
#include "oris_automation_types.h"

/* functions */
bool oris_is_same_automation_event(oris_automation_event_t* a, oris_automation_event_t* b);

void oris_init_automation_event(oris_automation_event_t* event, 
		oris_event_type_t type, const char* name);
void oris_free_automation_event(oris_automation_event_t* event);

void oris_automation_trigger(oris_automation_event_t* event, oris_application_info_t* info);

/* implementation of the automation actions */
void oris_automation_iterate_action(oris_application_info_t* info, 
	const char* request_name, const char* tbl_name);

void oris_automation_request_action(oris_application_info_t* info, 
	const char* request_name);

void oris_automation_http_action(oris_application_info_t* info, 
    enum evhttp_cmd_type method, pANTLR3_BASE_TREE url, 
    pANTLR3_BASE_TREE tmpl_name, oris_parse_expr_t* value_expr, 
	const char* tbl_name, bool request_per_record);

#endif /* __ORIS_AUTOMATION_H */
