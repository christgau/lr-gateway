#include <string.h>

#include "oris_log.h"
#include "oris_automation.h"
#include "oris_configuration.h"

bool oris_is_same_automation_event(oris_automation_event_t* a, oris_automation_event_t* b)
{
	return a != NULL && b != NULL && a->type == b->type && 
		a->name != NULL && b->name != NULL && strcasecmp(a->name, b->name) == 0;
}


void oris_init_automation_event(oris_automation_event_t* event, 
		oris_event_type_t type, const char* name)
{
	if (event) {
		event->type = type;
		event->name = strdup(name);
	}
}


void oris_free_automation_event(oris_automation_event_t* event)
{
	if (event && event->name) {
		free(event->name);
		event->name = NULL;
	}
}

void oris_automation_trigger(oris_automation_event_t* event, oris_application_info_t *info)
{
	if (!event || !event->name) {
		return;
	}

	oris_configuration_perform_automation(event, info);
}

void oris_automation_iterate_action(oris_application_info_t* info, 
	const char* tbl_name, const char* request_name)
{
	oris_table_t* tbl = oris_get_table(&info->data_tables, tbl_name);
//	pANTLR3_BASE_TREE req_tree = oris_get_request_tree(request_name);

	printf("iterate table %s with request %s\n", tbl->name, request_name); 
}


void oris_automation_request_action(oris_application_info_t* info, 
	const char* request_name)
{
	char* r = oris_get_parsed_request(request_name);

	oris_log_f(LOG_DEBUG, "sending requesting \%s\n", r);
	oris_connections_send(&info->connections, "data", r, strlen(r));
		
	free(r);
}
