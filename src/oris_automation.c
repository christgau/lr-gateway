#include <string.h>
#include <errno.h>

#include "oris_util.h"
#include "oris_log.h"
#include "oris_automation.h"
#include "oris_configuration.h"
#include "oris_interpret_tools.h"

#include "grammars/configTree.h"

static char* oris_parse_request_tree(const pANTLR3_BASE_TREE parse_tree);
static char* oris_get_parsed_request(const char *name);

static void http_request_done_cb(struct evhttp_request *req, void *ctx);
static void oris_perform_http_on_targets(oris_application_info_t* info, 
	const enum evhttp_cmd_type method, const char* uri, struct evbuffer* body);

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
	const char* request_name, const char* tbl_name)
{
	oris_table_t* tbl = oris_get_table(&info->data_tables, tbl_name);
	pANTLR3_BASE_TREE req_tree = oris_get_request_parse_tree(request_name);
	int l;
	char* r;
	
	if (!tbl) {
		oris_log_f(LOG_DEBUG, "table %s not found, not performing iterate using %s", 
				tbl_name, request_name);
		return;
	}

	l = tbl->current_row;
	for (tbl->current_row = 0; tbl->current_row < tbl->row_count; tbl->current_row++) {
		r = oris_parse_request_tree(req_tree);
		oris_connections_send(&info->connections, "data", r, strlen(r));
		free(r);
	}
	tbl->current_row = l;
}


void oris_automation_request_action(oris_application_info_t* info, 
	const char* request_name)
{
	char* r = oris_get_parsed_request(request_name);

	if (r != NULL) {
		oris_log_f(LOG_DEBUG, "sending requesting \%s\n", r);
		oris_connections_send(&info->connections, "data", r, strlen(r));
		free(r);
	}
}

static void http_request_done_cb(struct evhttp_request *req, void *ctx)
{
	int status;

	if (req == NULL) {
/* If req is NULL, it means an error occurred, but
* sadly we are mostly left guessing what the error
* might have been. We'll do our best... */
int printed_err = 0;
int errcode = EVUTIL_SOCKET_ERROR();
fprintf(stderr, "some request failed - no idea which one though!\n");
/* If the OpenSSL error queue was empty, maybe it was a
* socket error; let's try printing that. */
if (! printed_err)
fprintf(stderr, "socket error = %s (%d)\n",
evutil_socket_error_to_string(errcode),
errcode);
return;
}

	status = evhttp_request_get_response_code(req);
	oris_log_f(status / 100 != 2 ? LOG_ERR : LOG_INFO, "http response %d %s", 
			evhttp_request_get_response_code(req),
			evhttp_request_get_response_code_line(req));
	
	ctx = ctx;
}

static const char* oris_get_http_method_string(const enum evhttp_cmd_type method)
{
	switch (method) {
		case EVHTTP_REQ_GET:
			return "GET";
		case EVHTTP_REQ_POST:
			return "POST";
		case EVHTTP_REQ_PUT:
			return "PUT";
		case EVHTTP_REQ_DELETE:
			return "DELETE";
		default:
			return "?";
	}

	return "?";
}

#define MAX_URL_SIZE 256

static void oris_perform_http_on_targets(oris_application_info_t* info, 
		const enum evhttp_cmd_type method, const char* uri, struct evbuffer* body)
{
	char url_buf[MAX_URL_SIZE] = { 0 };
	struct evhttp_request *request;
	struct evkeyvalq *output_headers;
	struct evbuffer* output_buffer;
	int i;

	oris_log_f(LOG_INFO, "http %s %s (%lu bytes body) ", oris_get_http_method_string(method),
			uri, evbuffer_get_length(body));

	body = body;
	for (i = 0; i < info->targets.count; i++) {
		request = evhttp_request_new(http_request_done_cb, &(info->targets.items[i]));
		if (request != NULL) {
			output_headers = evhttp_request_get_output_headers(request);
			evhttp_add_header(output_headers, "Host", evhttp_uri_get_host(info->targets.items[i].uri));
			evhttp_add_header(output_headers, "User-Agent", ORIS_USER_AGENT);
			evhttp_add_header(output_headers, "Accept", "application/json, text/plain");
			evhttp_add_header(output_headers, "Accept-Charset", "utf-8");
			evhttp_add_header(output_headers, "Content-Type", "application/json");
			evutil_snprintf(url_buf, MAX_URL_SIZE - 1, "%s%s", evhttp_uri_get_path(
					info->targets.items[i].uri), uri);

			output_buffer = evhttp_request_get_output_buffer(request);
			evbuffer_add_buffer_reference(output_buffer, body);

			oris_log_f(LOG_DEBUG, "sending request %s to '%s'", url_buf, info->targets.items[i].name);
			if (evhttp_make_request(info->targets.items[i].connection, request, method, url_buf) != 0) {
				oris_log_f(LOG_ERR, "error making http request");
			}
		} else {
			oris_log_f(LOG_ERR, "could not send request %s to target %s. target's state may be undefined!", uri, 
					info->targets.items[i].name);
		}
	}
}


static char* oris_parse_request_tree(const pANTLR3_BASE_TREE parse_tree)
{
	pANTLR3_COMMON_TREE_NODE_STREAM node_stream;
	pconfigTree walker;
    oris_parse_expr_t* expr;
    char* retval = NULL;

    if (!parse_tree) {
        return NULL;
    }

    node_stream = antlr3CommonTreeNodeStreamNewTree(parse_tree, ANTLR3_SIZE_HINT);
    walker = configTreeNew(node_stream);

    expr = walker->expr(walker);
    retval = oris_expr_as_string(expr);
    oris_free_expr_value(expr);

    node_stream->free(node_stream);
    walker->free(walker);

    return retval;
}

static char* oris_get_parsed_request(const char *name)
{
    return oris_parse_request_tree(oris_get_request_parse_tree(name));
}

static void oris_add_expr_to_buf(struct evbuffer* target, oris_parse_expr_t* expr)
{
	if (expr->type == ET_INT) {
		evbuffer_add_printf(target, "%d", expr->value.as_int);
	} else if (expr->type == ET_STRING) {
		evbuffer_add_printf(target, "\"%s\"", expr->value.as_string->chars);
	} else {
		evbuffer_add_printf(target, "null");
	}
}

static void oris_parse_template(struct evbuffer* target, pANTLR3_BASE_TREE tmpl, bool with_v_prefix)
{
	ANTLR3_UINT32 i;
	pANTLR3_BASE_TREE c;
	pANTLR3_STRING s;
	oris_parse_expr_t* v;

	if (tmpl->getType(tmpl) != TEMPLATE) {
		return;
	}

	if (with_v_prefix) {
		evbuffer_add_printf(target, "{\"v\":{");
	}

	for (i = 1; i < tmpl->getChildCount(tmpl); i += 2) {
		c = tmpl->getChild(tmpl, i);
		if (c->getType(c) == IDENTIFIER) {
			if (i > 1) {
				evbuffer_add_printf(target, ",");
			}
			s = c->getText(c);
			evbuffer_add_printf(target, "\"%s\":", s->chars);

			c = tmpl->getChild(tmpl, i + 1);
			v = oris_expr_parse_from_tree(c);
			oris_add_expr_to_buf(target, v);

			oris_free_expr_value(v);
		}
	}

	if (with_v_prefix) {
		evbuffer_add_printf(target, "}}");
	}
}


static void oris_parse_iterated_template(struct evbuffer* buf, oris_table_t* tbl, 
	pANTLR3_BASE_TREE template)
{
	char c;
	int l;

	l = tbl->current_row;

	for (tbl->current_row = 0; tbl->current_row < tbl->row_count; tbl->current_row++) {
		if (tbl->current_row > 1) {
			c = ','; evbuffer_add(buf, &c, sizeof(c));
		}
		c = '{'; evbuffer_add(buf, &c, sizeof(c));
		oris_parse_template(buf, template, false);
		c = '}'; evbuffer_add(buf, &c, sizeof(c));
	}

	tbl->current_row = l;
}

static void oris_dump_expr_value_to_buffer(struct evbuffer* buf, oris_parse_expr_t* expr)
{
	if (expr->type == ET_INT) {
		evbuffer_add_printf(buf, "{\"v\":\"%d\"}", expr->value.as_int);
	} else if (expr->type == ET_STRING) {
		evbuffer_add_printf(buf, "{\"v\":\"%s\"}", expr->value.as_string->chars);
	} else {
		evbuffer_add_printf(buf, "{\"v\": null}");
	}
}


static void oris_perform_http_with_buffer(oris_application_info_t* info,
	enum evhttp_cmd_type method, const pANTLR3_BASE_TREE url, struct evbuffer* buf)
{
	char* url_str;
    oris_parse_expr_t* url_expr;

	url_expr = oris_expr_parse_from_tree(url);
	url_str = oris_expr_as_string(url_expr);

	oris_perform_http_on_targets(info, method, url_str, buf);

	oris_free_and_null(url_str);
	oris_free_expr_value(url_expr);
}

static void oris_perform_http_on_table(oris_application_info_t* info,
	enum evhttp_cmd_type method, pANTLR3_BASE_TREE url, struct evbuffer* buf,
	pANTLR3_BASE_TREE tmpl, oris_table_t* tbl, bool perform_per_record)
{
	int l;

	if (perform_per_record) {
		l = tbl->current_row;

		for (tbl->current_row = 0; tbl->current_row < tbl->row_count; tbl->current_row++) {
			evbuffer_drain(buf, evbuffer_get_length(buf));
			oris_parse_template(buf, tmpl, true);
			oris_perform_http_with_buffer(info, method, url, buf);
		}

		tbl->current_row = l;
	} else {
		evbuffer_add_printf(buf, "{\"v\":[");
		oris_parse_iterated_template(buf, tbl, tmpl);
		evbuffer_add_printf(buf, "]}");

		oris_perform_http_with_buffer(info, method, url, buf);
	}
}

void oris_automation_http_action(oris_application_info_t* info, 
    enum evhttp_cmd_type method, pANTLR3_BASE_TREE url, 
    pANTLR3_BASE_TREE tmpl_name, oris_parse_expr_t* value_expr, 
	const char* tbl_name, bool perform_per_record)
{
	struct evbuffer* buf;
	oris_table_t* tbl;
	pANTLR3_BASE_TREE tmpl;

	buf = evbuffer_new();

    if (tmpl_name != NULL) {
		tbl = tbl_name != NULL ? oris_get_table(&info->data_tables, tbl_name) : NULL;
		tmpl = oris_get_template_by_name(tmpl_name->getText(tmpl_name));

		if (tbl != NULL) {
			/* table given: now perform http record-wise or for the whole table */
			oris_perform_http_on_table(info, method, url, buf, tmpl, tbl, perform_per_record);
		} else {
			/* no table given (or found), parse template and send it */
			oris_parse_template(buf, tmpl, true);
			oris_perform_http_with_buffer(info, method, url, buf);
		}

	} else if (value_expr != NULL) {
		oris_dump_expr_value_to_buffer(buf, value_expr);
		oris_perform_http_with_buffer(info, method, url, buf);
		oris_free_expr_value(value_expr);
    }

	evbuffer_free(buf);
}

