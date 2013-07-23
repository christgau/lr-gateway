#include <stdlib.h>
#include <string.h>

#include "oris_protocol_data.h"
#include "oris_util.h"
#include "oris_log.h"
#include "oris_table.h"
#include "oris_automation.h"

#define LINE_DELIM_START 0x02
#define LINE_DELIM_END   0x03
#define RECORD_DELIM     '|'

static void process_line(char* line, oris_application_info_t* info);
static void table_complete_cb(oris_table_t* tbl, oris_application_info_t* info);
static void oris_protocol_data_write(const void* buf, size_t bufsize, 
	void* connection, oris_data_write_fn_t transfer);

void oris_protocol_data_init(struct oris_protocol* self)
{
	self->write = (void*) oris_protocol_data_write;
}

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
	oris_table_t* tbl;
	char *s, *c, *tbl_name;
	size_t last_char_index;
	bool is_last_line, is_response_line;

	if (strlen(line) == 0) {
		return;
	}

	s = strdup(line);
	c = s;

	/* extract table name */
	while (*c && *c != RECORD_DELIM) {
		c++;
	}

	if (*c == '\0') {
		return;
	}

	*c = '\0';
	tbl_name = s;

	tbl = oris_get_or_create_table(&info->data_tables, tbl_name, true);
	if (!tbl) {
		free(line);
		return;
	}

	last_char_index = strlen(tbl_name) - 1;
	is_last_line = tbl_name[last_char_index] == '1';
	is_response_line = tbl_name[last_char_index] == '!';

	if (is_response_line || (tbl->state == COMPLETE && !is_last_line)) {
		oris_table_clear(tbl);
	}

	oris_table_add_row(tbl, s + last_char_index + 1, ORIS_TABLE_ITEM_SEPERATOR);
	tbl->state = (is_response_line || is_last_line) ? COMPLETE : RECEIVING;
	if (tbl->state == COMPLETE) {
		table_complete_cb(tbl, info);
	}

	free(s);
	return;
}

static void table_complete_cb(oris_table_t* tbl, oris_application_info_t* info)
{
	oris_automation_event_t e = { EVT_TABLE, tbl->name };
	oris_log_f(LOG_INFO, "table %s received", tbl->name);

	oris_automation_trigger(&e, info);
}

void oris_protocol_data_connected_cb(struct oris_protocol* self)
{
	oris_automation_event_t e = { EVT_CONNECTION, "established" };
	oris_automation_trigger(&e, ((oris_data_protocol_data_t*) self->data)->info);

	oris_log_f(LOG_DEBUG, "triggering connection actions...");
}

static void oris_protocol_data_write(const void* buf, size_t bufsize, 
	void* connection, oris_data_write_fn_t transfer)
{
	uint8_t c;
	
	c = LINE_DELIM_START;
	transfer(connection, &c, sizeof(c));

	transfer(connection, buf, bufsize);

	c = LINE_DELIM_END;
	transfer(connection, &c, sizeof(c));
}
