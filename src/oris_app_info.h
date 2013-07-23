#ifndef __ORIS_APP_INFO_H
#define __ORIS_APP_INFO_H

#include "oris_table.h"
#include "oris_connection.h"
#include "oris_interpret_tools.h"

/* libevent related global application stuff */
typedef struct oris_libevent_base_info {
	struct event_base *base;
	struct evdns_base *dns_base;
} oris_libevent_base_info_t;


/* key-value pair holding information about an remote http target */
typedef struct oris_http_target {
	char* name;
	struct evhttp_uri* uri;
} oris_http_target_t;


/* main application state and configuration is held herein */
typedef struct oris_application_info {
	oris_libevent_base_info_t libevent_info;
	oris_table_list_t data_tables;
	oris_connection_list_t connections;

	struct {
		oris_http_target_t* items;
		int count;
	} targets;

	struct event *sigint_event;

	int (*main)(struct oris_application_info*);

	bool paused;
	int log_level;
	int argc;
	char** argv;
} oris_application_info_t;

/* init clean up stuff */
void oris_app_info_init(oris_application_info_t* info);
void oris_app_info_finalize(oris_application_info_t* info);

/* adding and clearing targets from above */
void oris_config_add_target(oris_application_info_t* config, const char* name, const char* uri);
void oris_create_connection(oris_application_info_t* info, const char* name, oris_parse_expr_t* e);

#endif /* __ORIS_APP_INFO_H */
