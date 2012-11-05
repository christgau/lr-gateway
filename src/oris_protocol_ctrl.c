#include <string.h>
#include <event2/bufferevent.h>

#include "oris_protocol_ctrl.h"
#include "oris_connection.h"
#include "oris_log.h"
#include "oris_util.h"

#define LINE_DELIM_CR 0x0D
#define LINE_DELIM_LF 0x0A

static void process_command(const char* cmd, oris_application_info_t* info);

void oris_protocol_ctrl_read_cb(struct bufferevent *bev, void *ctx)
{
	oris_connection_t* con = ctx;
	oris_ctrl_protocol_data_t* pdata = (oris_ctrl_protocol_data_t*) con->protocol->data;

	struct evbuffer* input = bufferevent_get_input(bev);
	char *p, *bufstart;

	/* receive and return if nothing was recevied */
	if (oris_protocol_recv(input, &pdata->buffer, &pdata->buf_size, &pdata->buf_capacity) == 0) {
		return;
	}

	/* process da data */
	bufstart = pdata->buffer;
	while (pdata->buf_size > 0 && (p = memchr(bufstart, LINE_DELIM_CR, pdata->buf_size))) {
		*p = '\0';
		process_command(bufstart, pdata->info);
		
		pdata->buf_size -= (p - bufstart) + 1;
		bufstart = ++p;

		if (pdata->buf_size > 0 && *p == LINE_DELIM_LF) {
			pdata->buf_size--;
			bufstart++;
		}
	}

	/* move unprocessed data to the beginning of the buffer */
	if (pdata->buf_size > 0) {
		memmove(bufstart, pdata->buffer, pdata->buf_size);
	}
}

static void process_command(const char* cmd, oris_application_info_t* info)
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
