#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <event2/event.h>

#include "oris_protocol_data.h"
#include "oris_util.h"
#include "oris_log.h"
#include "oris_table.h"
#include "oris_automation.h"
#include "oris_connection.h"

#define LINE_DELIM_START 0x02
#define LINE_DELIM_END   0x03
#define RECORD_DELIM     '|'

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4706 )
#endif

static void process_line(char* line, oris_data_protocol_data_t* protocol);
static void table_complete_cb(oris_table_t* tbl, oris_application_info_t* info);
static void oris_protocol_data_write(const void* buf, size_t bufsize,
	void* connection, oris_connection_write_fn_t transfer);
static void oris_protocol_data_free(struct oris_protocol* protocol);
static void oris_protocol_data_idle_event_cb(evutil_socket_t fd, short type,
	void *arg);

void oris_protocol_data_init(struct oris_protocol* self)
{
	oris_data_protocol_data_t* data = (oris_data_protocol_data_t*) self->data;

	self->write = oris_protocol_data_write;
	self->destroy = oris_protocol_data_free;
	data->connection = NULL;
	data->state = IDLE;
	data->last_req_tbl_name = NULL;
	data->idle_event = event_new(data->info->libevent_info.base, -1, 0,
		oris_protocol_data_idle_event_cb, data);

	STAILQ_INIT(&data->outstanding_requests);
}

static void oris_protocol_data_free(struct oris_protocol* self)
{
	oris_data_protocol_data_t* data = (oris_data_protocol_data_t*) self->data;
	oris_data_request_t* i;

	event_del(data->idle_event);
	event_free(data->idle_event);

	while (!STAILQ_EMPTY(&data->outstanding_requests)) {
		i = STAILQ_FIRST(&data->outstanding_requests);
		STAILQ_REMOVE_HEAD(&data->outstanding_requests, queue);
		free(i->message);
		free(i);
    }

	oris_free_and_null(data->last_req_tbl_name);
	oris_free_and_null(data->buffer);
	oris_protocol_free(self);
}

void oris_protocol_data_read_cb(struct bufferevent *bev, void *ctx)
{
	oris_connection_t* con = ctx;
	oris_data_protocol_data_t* pdata = (oris_data_protocol_data_t*) con->protocol->data;

	struct evbuffer* input = bufferevent_get_input(bev);
	char *p_start, *p_end, *bufstart;

	pdata->connection = con;
	/* receive and return if nothing was recevied */
	if (oris_protocol_recv(input, &pdata->buffer, &pdata->buf_size, &pdata->buf_capacity) == 0) {
		return;
	}

	/* process da data */
	bufstart = pdata->buffer;
	while (pdata->buf_size > 0 && (p_start = memchr((void*) bufstart, LINE_DELIM_START, pdata->buf_size))) {

		pdata->buf_size -= (p_start - bufstart) + 1;

		/* find end delim */
		if (pdata->buf_size > 0) {
			p_end = memchr((void*) ++p_start, LINE_DELIM_END, pdata->buf_size);
			if (p_end) {
				*p_end = '\0';
				process_line(p_start, pdata);
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
		memmove(pdata->buffer, bufstart - 1, pdata->buf_size);
	}
}

/* conversion of ISO-8859-1 to UTF-8 (from stackoverflow) */
static char* strdup_iso8859_to_utf8(char* line)
{
	unsigned char *c, *retval, *out;
	size_t size = 0;

	for (c = (unsigned char*) line; *c; c++, size++) {
		if (*c >= 128) {
			size++;
		}
	}

	retval = out = malloc(size + 1);
	if (retval == NULL) {
		return NULL;
	}

	c = (unsigned char*) line;
	while (*c) {
		if (*c < 128) {
			*out++ = *c++;
		} else {
			*out++ = 0xc2 + (*c > 0xbf);
			*out++ = 0x80 + (*c++ & 0x3f);
		}
	}
	*out = 0;

	return (char*) retval;
}

static void process_line(char* line, oris_data_protocol_data_t* protocol)
{
	oris_table_t* tbl;
	oris_application_info_t* info = protocol->info;
	char *s, *c, *tbl_name;
	size_t last_char_index;
	bool is_last_line, is_response_line;

	if (strlen(line) == 0) {
		return;
	}

	s = strdup_iso8859_to_utf8(line);
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

	if (strlen(tbl_name) < 2) {
		return;
	}

	last_char_index = strlen(tbl_name) - 1;
	is_last_line = tbl_name[last_char_index] == '0';
	is_response_line = !is_last_line && tbl_name[last_char_index] != '1';
	if (!is_response_line) {
		tbl_name[last_char_index] = '\0';
	}

	tbl = oris_get_or_create_table(&info->data_tables, tbl_name, true);
	if (!tbl) {
		free(line);
		return;
	}

/*	if (is_response_line || (tbl->state == COMPLETE && !is_last_line)) {*/
	if (is_response_line || (tbl->state == COMPLETE)) {
		oris_table_clear(tbl);
	}

	oris_table_add_row(tbl, s + last_char_index + 2, ORIS_TABLE_ITEM_SEPERATOR);
	tbl->state = (is_response_line || is_last_line) ? COMPLETE : RECEIVING;
	if (tbl->state == COMPLETE) {
		table_complete_cb(tbl, info);
		if (protocol->state == WAIT_FOR_RESPONSE && strcmp(tbl_name,
				protocol->last_req_tbl_name) == 0) {
			/* reset state so we can send outstanding requests */
			oris_log_f(LOG_DEBUG, "received %s, state is idle now, trigger event", tbl_name);
			protocol->state = IDLE;
			event_active(protocol->idle_event, EV_READ, 0);
		}
	}

	free(s);
	return;
}

static void table_complete_cb(oris_table_t* tbl, oris_application_info_t* info)
{
	oris_automation_event_t e;

	e.type = EVT_TABLE;
	e.name = tbl->name;

	oris_log_f(LOG_INFO, "table %s received (%d lines)", tbl->name, tbl->row_count);
	oris_automation_trigger(&e, info);
}

void oris_protocol_data_connected_cb(struct oris_protocol* self)
{
	oris_automation_event_t e = { EVT_CONNECTION, "established" };
	oris_automation_trigger(&e, ((oris_data_protocol_data_t*) self->data)->info);

	oris_log_f(LOG_DEBUG, "triggering connection actions...");
}

static void oris_protocol_data_write(const void* buf, size_t bufsize,
	void* connection, oris_connection_write_fn_t transfer)
{
	uint8_t c;
	char *s;
	oris_protocol_t* protocol = ((oris_connection_t*) connection)->protocol;
	oris_data_protocol_data_t* self = (oris_data_protocol_data_t*) protocol->data;
	oris_data_request_t* request;
	struct timeval response_timeout = { 2, 500000 }; /* 2.5 seconds */

	self->connection = connection;
	if (self->state == IDLE) {
		oris_log_f(LOG_DEBUG, "sending data request %s", buf);
		c = LINE_DELIM_START;
		transfer(connection, &c, sizeof(c));
		transfer(connection, buf, bufsize);
		c = LINE_DELIM_END;
		transfer(connection, &c, sizeof(c));

		/* extract requested table name if any */
		if (bufsize > 0) {
			s = (char*) buf;
			if (*s++ == '?') {
				s = strndup((const char*) s, bufsize - 1);
				if (s) {
					event_add(self->idle_event, &response_timeout);
					self->state = WAIT_FOR_RESPONSE;
					self->last_req_tbl_name = s;
					while (*s++) {
						if (*s == ORIS_TABLE_ITEM_SEPERATOR) {
							*s = 0;
							break;
						}
					}
				}
			}
		}
	} else {
		request = calloc(1, sizeof(*request));
		request->message = malloc(bufsize);
		request->size = bufsize;
		memcpy(request->message, buf, bufsize);

		STAILQ_INSERT_TAIL(&self->outstanding_requests, request, queue);
	}
}

static void oris_protocol_data_idle_event_cb(evutil_socket_t fd, short type,
	void *arg)
{
	oris_data_request_t* request;
	oris_data_protocol_data_t* self = (oris_data_protocol_data_t*) arg;

	if (STAILQ_EMPTY(&self->outstanding_requests)) {
		return;
	}

	if (type == EV_TIMEOUT) {
		self->state = IDLE;
		oris_log_f(LOG_DEBUG, "change connection to idle state due to "
				"missing or timedout reponse");
	}

	request = STAILQ_FIRST(&self->outstanding_requests);
	if (self->state != IDLE) {
		oris_log_f(LOG_DEBUG, "connection not idle, waiting for next chance");
		return;
	}

	STAILQ_REMOVE_HEAD(&self->outstanding_requests, queue);
	oris_protocol_data_write(request->message, request->size, self->connection,
		((oris_connection_t*) self->connection)->write);

	/* keep compiler happy */
	fd = fd;
	type = type;
	return;
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif
