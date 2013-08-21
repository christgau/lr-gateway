#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef WIN32
#include <unistd.h>
#else
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4706 )
#endif
#endif
#include <event2/bufferevent.h>

#include "oris_automation.h"
#include "oris_protocol_ctrl.h"
#include "oris_connection.h"
#include "oris_log.h"
#include "oris_util.h"

#define LINE_DELIM_CR 0x0D
#define LINE_DELIM_LF 0x0A

#define ORIS_CTRL_PROMPT "\x1b[1moris>\x1b[0m "

typedef void (*oris_cmd_fn_t) (char* s, oris_application_info_t* info, 
		struct evbuffer* out);

typedef struct {
	char* fn;
	char* help;
	oris_cmd_fn_t f;
} oris_ctrl_cmd_t;

static void oris_builtin_cmd_terminate(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_pause_resume(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_help(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_dump(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_list(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_show(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_clear(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_request(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_add(char* s, oris_application_info_t* info,
	struct evbuffer* out);
static void oris_builtin_cmd_trigger(char* s, oris_application_info_t* info,
	struct evbuffer* out);

static oris_ctrl_cmd_t ctrl_commands[] = {
	{ "add", "add a row of records to a table (usage: add table row)", oris_builtin_cmd_add },
	{ "clear", "clears the given data table (argument)", oris_builtin_cmd_clear },
	{ "dump", "dump all tables to file (optional argument)", oris_builtin_cmd_dump },
	{ "exit", "terminate connection", NULL },
	{ "help", "show this help", oris_builtin_cmd_help },
	{ "list", "list objects: tables, targets", oris_builtin_cmd_list },
	{ "pause", "disable automation actions", oris_builtin_cmd_pause_resume },
	{ "request", "issue request to data feed provider(s)", oris_builtin_cmd_request },
	{ "resume", "re-enable automation actions", oris_builtin_cmd_pause_resume },
	{ "show", "show content of table (name is argument)", oris_builtin_cmd_show },
	{ "terminate", "terminate the gateway", oris_builtin_cmd_terminate },
	{ "trigger", "trigger actions (table, command)", oris_builtin_cmd_trigger }
};


static void process_command(const char* cmd, oris_application_info_t* info,
	struct evbuffer* output);

void oris_protocol_ctrl_connected_cb(struct oris_protocol* self)
{
	/* sorry, self is not a protocol here */
	struct bufferevent* bev = (struct bufferevent*) self;
	struct evbuffer* output = bufferevent_get_output(bev);

	evbuffer_add_printf(output, "%s\r\n%s", ORIS_USER_AGENT, ORIS_CTRL_PROMPT);
}

void oris_protocol_ctrl_read_cb(struct bufferevent *bev, void *ctx)
{
	oris_connection_t* con = ctx;
	oris_ctrl_protocol_data_t* pdata = (oris_ctrl_protocol_data_t*) con->protocol->data;

	struct evbuffer* input, *output; 
	char* line, *cmd;
	size_t n;
	bool close = false;

	input = bufferevent_get_input(bev);
	output = bufferevent_get_output(bev);

	while ((line = evbuffer_readln(input, &n, EVBUFFER_EOL_CRLF))) {
		cmd = oris_rtrim(oris_ltrim(line));
		close = strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0;

		if (strlen(cmd) > 0 && !close) {
			process_command(cmd, pdata->info, output);
			if (evbuffer_get_length(output) > 0) {
				evbuffer_add_printf(output, "\r\n");
			}
		} 

		if (close) {
			evbuffer_add_printf(output, "Bye!\n");
		} else {
			evbuffer_add_printf(output, "%s", ORIS_CTRL_PROMPT);
		}

		free(line);
	}

	if (evbuffer_get_length(input) > 1024) {
		evbuffer_add_printf(output, "Sorry, line too long\r\n");
		evbuffer_drain(input, evbuffer_get_length(input));
	}

	if (close) {
		bufferevent_free(bev);
	}
}

static void process_command(const char* cmd, oris_application_info_t* info, 
	struct evbuffer* output)
{
	size_t i;

	oris_log_f(LOG_DEBUG, "got command '%s'", cmd);
	for (i = 0; i < sizeof(ctrl_commands) / sizeof(*ctrl_commands); i++) {
		if (strstr(cmd, ctrl_commands[i].fn) == cmd && ctrl_commands[i].f) {
			ctrl_commands[i].f((char*) cmd, info, output);
			return;
		}
	}
	
	oris_log_f(LOG_ERR, "unknown remote command '%s'", cmd);
	evbuffer_add_printf(output, "unknown command %s", cmd);
}

/* implementation of builtin commands */

static void word_end(char** s)
{
	char* str;

	for (str = *s; *str && !isblank(*str); str++);
	
	*s = *str ? str : NULL;
}

static char* next_word(char** s)
{
	char* str, *retval = NULL;

	if (*s == NULL) {
		return NULL;
	}

	for (str = *s; *str && isblank(*str);  str++) ; /* skip white space */

	if (*str) {
		retval = str;
		for ( ; *str && !isblank(*str); str++) ; /* skip word letters */
		if (*str) {
			*str++ = 0;
			if (*str == 0) {
				*s = NULL;
			} else {
				*s = str;
			}
		}
	}

	return retval;
}

static void oris_builtin_cmd_terminate(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	oris_log_f(LOG_INFO, "terminating external command (%s)", s);
	evbuffer_add_printf(out, "terminating");
	event_base_loopbreak(info->libevent_info.base);	
}

static void oris_builtin_cmd_pause_resume(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	if (strcmp(s, "pause") == 0) {
		if (!info->paused) {
			oris_log_f(LOG_INFO, "pausing activity on targets");
			evbuffer_add_printf(out, "paused");
			info->paused = true;
		} else {
			evbuffer_add_printf(out, "already paused");
		}
	} else if (strcmp(s, "resume") == 0) {
		if (info->paused) {
			oris_log_f(LOG_INFO, "resuming activity on targets");
			evbuffer_add_printf(out, "resumed");
			info->paused = false;
		} else {
			evbuffer_add_printf(out, "not paused");
		}
	}
}

static void oris_builtin_cmd_help(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	size_t i;
	for (i = 0; i < sizeof(ctrl_commands) / sizeof(*ctrl_commands); i++) {
		evbuffer_add_printf(out, "%s: %s\r\n", ctrl_commands[i].fn, 
				ctrl_commands[i].help);
	}

	s = s;
	info = info;
}

static void oris_builtin_cmd_dump(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	char *fn;
	word_end(&s);
	
	fn = next_word(&s);
	if (oris_tables_dump_to_file(&info->data_tables, fn ? fn : info->dump_fn)) {
		evbuffer_add_printf(out, "tables dumped to %s", fn ? fn : info->dump_fn);
	} else {
		evbuffer_add_printf(out, "could not dump to %s", fn ? fn : info->dump_fn);
	}
}

static void oris_builtin_cmd_list(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	char* object;
	size_t i; 

	word_end(&s);
	object = next_word(&s);

	if (!object) {
		evbuffer_add_printf(out, "don't know what to dump");
	} else if (strcmp(object, "tables") == 0) {
		evbuffer_add_printf(out, "%d tables present", (int) info->data_tables.count);
		for (i = 0; i < info->data_tables.count; i++) {
			evbuffer_add_printf(out, "\r\n\t%s (%d)", info->data_tables.tables[i].name,
					info->data_tables.tables[i].row_count);
		}
	} else if (strcmp(object, "targets") == 0) {
		evbuffer_add_printf(out, "%d http targets defined", (int) info->targets.count);
		for (i = 0; i < (size_t) info->targets.count; i++) {
			evbuffer_add_printf(out, "\r\n\t%s -> %s://%s/%s", info->targets.items[i].name, 
					evhttp_uri_get_scheme(info->targets.items[i].uri),
					evhttp_uri_get_host(info->targets.items[i].uri),
					evhttp_uri_get_path(info->targets.items[i].uri));
		}
	} else {
		evbuffer_add_printf(out, "unknown objects to list: '%s'", object);
	}
}

static void oris_builtin_cmd_show(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	char* str;
	int row, i, j, *field_widths; 
	oris_table_t* tbl;

	word_end(&s);
	str = next_word(&s);

	if (!str) {
		evbuffer_add_printf(out, "missing table name");
		return;
	}

	tbl = oris_get_table(&info->data_tables, oris_upper_str(str)); 
	if (!tbl) {
		evbuffer_add_printf(out, "unknown table '%s'", str);
		return;
	}

	if (tbl->row_count == 0) {
		evbuffer_add_printf(out, "table '%s' is empty", tbl->name);
		return;
	}

	evbuffer_add_printf(out, "table '%s' has %d records and %d fields", 
			str, tbl->row_count, tbl->field_count);

	field_widths = oris_table_get_field_widths(tbl);
	row = tbl->current_row;
	for (tbl->current_row = 0; tbl->current_row < tbl->row_count; tbl->current_row++) {
		evbuffer_add_printf(out, "\r\n");
		for (i = 1; i <= tbl->rows[tbl->current_row].field_count; i++) {
			if (i > 1) {
				for (j = (int) strlen(str); j < field_widths[i - 2]; j++) {
					evbuffer_add(out, " ", 1);
				}
				evbuffer_add(out, " | ", 3);
			}
			str = (char*) oris_table_get_field_by_index(tbl, i);
			evbuffer_add_printf(out, "%s", str);

		}
	}
	tbl->current_row = row;
}

static void oris_builtin_cmd_clear(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	char* tbl_name;
	oris_table_t* tbl;

	word_end(&s);
	tbl_name = next_word(&s);

	if (!tbl_name) {
		evbuffer_add_printf(out, "missing table name");
		return;
	}

	tbl = oris_get_table(&info->data_tables, oris_upper_str(tbl_name));
	if (!tbl) {
		evbuffer_add_printf(out, "unknown table '%s'", tbl_name);
		return;
	}

	oris_table_clear(tbl);
}

static void oris_builtin_cmd_request(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	char* request;

	word_end(&s);
	request = next_word(&s);

	if (request) {
		oris_log_f(LOG_DEBUG, "sending requesting %s", request);
		oris_connections_send(&info->connections, "data", request, strlen(request));
	} else {
		evbuffer_add_printf(out, "now request given");
	}
}

static void oris_builtin_cmd_add(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	char *tbl_name, *row = NULL;
	oris_table_t* tbl;

	word_end(&s);
	tbl_name = next_word(&s);

	if (!tbl_name) {
		evbuffer_add_printf(out, "missing table name");
		return;
	} 

	if (s) {
		for (; *s && isblank(*s); s++) ; /* skip white space */
		row = s;
	}

	if (!row) {
		evbuffer_add_printf(out, "missing/empty row for table %s (not added)", tbl_name);
		return;
	}

	tbl = oris_get_or_create_table(&info->data_tables, oris_upper_str(tbl_name), true); 
	if (!tbl) {
		evbuffer_add_printf(out, "could not allocate new table %s", tbl_name);
		return;
	}
	
	oris_table_add_row(tbl, row, '|');
}

static void oris_builtin_cmd_trigger(char* s, oris_application_info_t* info,
	struct evbuffer* out)
{
	char* object;
	oris_automation_event_t ev;

	word_end(&s);
	object = next_word(&s);
	ev.name = next_word(&s);

	if (!object) {
		evbuffer_add_printf(out, "missing object (table or command");
		return;
	}

	if (!ev.name) {
		evbuffer_add_printf(out, "missing object name");
		return;
	}

	if (strcasecmp(object, "table") == 0) {
		ev.type = EVT_TABLE;
	} else if (strcasecmp(object, "command") == 0) {
		ev.type = EVT_COMMAND;
	} else {
		evbuffer_add_printf(out, "invalid trigger object '%s'", object);
		return;
	}

	oris_automation_trigger(&ev, info);
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
