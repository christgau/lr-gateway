#include <string.h>

#include "oris_protocol_data.h"
#include "oris_util.h"
#include "oris_log.h"

#define LINE_DELIM_START 0x02
#define LINE_DELIM_END   0x03
#define RECORD_DELIM     '|'

static void process_line(char* line, oris_application_info_t* info);

void oris_protocol_data_read_cb(struct bufferevent *bev, void *ctx)
{
	oris_connection_t* con = ctx;
	oris_data_protocol_data_t* pdata = (oris_data_protocol_data_t*) con->protocol->data;

	struct evbuffer* input = bufferevent_get_input(bev);
	char *p_start, *p_end, *bufstart;

	/* receive and return if nothing was recevied */
	if (oris_protocol_recv(input, &pdata->buffer, &pdata->buf_size, &pdata->buf_capacity) == 0) {
		return;
	}

	/* process da data */
	bufstart = pdata->buffer;
	while (pdata->buf_size > 0 && (p_start = memchr(bufstart, LINE_DELIM_START, pdata->buf_size))) {
		
		pdata->buf_size -= (p_start - bufstart) + 1;

		/* find end delim */
		if (pdata->buf_size > 0) {
			p_end = memchr(++p_start, LINE_DELIM_END, pdata->buf_size);
			if (p_end) {
				*p_end = '\0';
				process_line(p_start, pdata->info);
				pdata->buf_size -= (p_end - p_start) + 1;
				bufstart = p_end + 1;
			} else {
				/* no delim found, discard recieved stuff and wait for next recv */
				bufstart = p_start;
				break;
			}
		}
	}

	/* move unprocessed data to the beginning of the buffer */
	if (pdata->buf_size > 0) {
		memmove(bufstart, pdata->buffer, pdata->buf_size);
	}
}

static void process_line(char* line, oris_application_info_t* info)
{
/*	oris_log_f(LOG_INFO, "got line: %s", line);*/
	char *s, *c;
	int rec_count = 1;

	if (strlen(line) == 0) {
		return;
	}

	s = strdup(line);
	c = s;
	for (; *c; c++) {
		if (*c == RECORD_DELIM) {
			*c = '\0';
			rec_count++;
		}
	}

	oris_log_f(LOG_INFO, "line with %d records", rec_count);
}

