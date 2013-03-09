#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <event2/bufferevent.h>

#include "oris_protocol_ctrl.h"
#include "oris_connection.h"
#include "oris_log.h"
#include "oris_util.h"

#define LINE_DELIM_CR 0x0D
#define LINE_DELIM_LF 0x0A

static void process_command(const char* cmd, oris_application_info_t* info,
	struct evbuffer* output);

void oris_protocol_ctrl_read_cb(struct bufferevent *bev, void *ctx)
{
	oris_connection_t* con = ctx;
	oris_ctrl_protocol_data_t* pdata = (oris_ctrl_protocol_data_t*) con->protocol->data;

	struct evbuffer* input, *output; 
	char* line;
	size_t n;

	input = bufferevent_get_input(bev);
	output = bufferevent_get_output(bev);

	while ((line = evbuffer_readln(input, &n, EVBUFFER_EOL_LF))) {
		process_command(line, pdata->info, output);
		evbuffer_add(output, "\r\n", 2);
		free(line);
	}

	if (evbuffer_get_length(input) > sysconf(_SC_PAGESIZE)) {
		evbuffer_add_printf(output, "Sorry, line too long\r\n");
		evbuffer_drain(input, evbuffer_get_length(input));
	}
}

static void process_command(const char* cmd, oris_application_info_t* info, 
	struct evbuffer* output)
{
	oris_log_f(LOG_INFO, "got command >%s<", cmd);
	if (strcmp(cmd, "terminate") == 0 || strcmp(cmd, "exit") == 0) {
		oris_log_f(LOG_INFO, "exiting by external command");
		event_base_loopbreak(info->libevent_info.base);	
	} else if (strcmp(cmd, "pause") == 0 && !info->paused) {
		oris_log_f(LOG_INFO, "pausing activity on targets");
		info->paused = true;
	} else if (strcmp(cmd, "resume") == 0 && info->paused) {
		oris_log_f(LOG_INFO, "resuming activity on targets");
		info->paused = false;
	} else if (strcmp(cmd, "produce today") == 0) {
		oris_log_f(LOG_INFO, "generating comps for today");
	} else if (strcmp(cmd, "produce tomorrow") == 0) {
		oris_log_f(LOG_INFO, "generating comps for tomorrow");
	} else {
		oris_log_f(LOG_INFO, "unknown remote command '%s'", cmd);
	}
}
