#include <string.h>

#include "oris_util.h"
#include "oris_log.h"
#include "oris_http.h"
#include "oris_automation.h"
#include "oris_configuration.h"
#include "oris_interpret_tools.h"

#include "grammars/configTree.h"
#include <antlr3basetree.h>

static char* oris_parse_request_tree(const pANTLR3_BASE_TREE parse_tree);
static char* oris_get_parsed_request(const char *name);

static void oris_perform_automation_actions(pANTLR3_BASE_TREE tree,
	oris_application_info_t* info);
static void oris_perform_automation_action(pANTLR3_BASE_TREE tree,
	oris_application_info_t* info);
static void oris_perform_automation_iterate(pANTLR3_BASE_TREE tree,
	oris_application_info_t* info);

bool oris_automation_init(oris_application_info_t* app_info)
{
	app_info = app_info;

	return true;
}

void oris_automation_finalize()
{
	return;
}

bool oris_is_same_automation_event(oris_automation_event_t* a, oris_automation_event_t* b)
{
	return a && b && a->type == b->type &&
		a->name && b->name && strcasecmp(a->name, b->name) == 0;
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
	pANTLR3_BASE_TREE tree;
	pANTLR3_COMMON_TREE_NODE_STREAM stream;

	if (!event || !event->name || !oris_get_automation_parse_tree(*event, &tree, &stream)) {
		return;
	}

	oris_perform_automation_actions(tree, info);
}

static void oris_perform_automation_actions(pANTLR3_BASE_TREE tree,
	oris_application_info_t* info)
{
	ANTLR3_UINT32 i;
	pANTLR3_BASE_TREE child;

	for (i = 0; i < tree->getChildCount(tree); i++) {
		child = tree->getChild(tree, i);
		if (child->getType(child) == ITERATE) {
			oris_perform_automation_iterate(child, info);
		} else {
			oris_perform_automation_action(child, info);
		}
	}
}

static void oris_perform_automation_iterate(pANTLR3_BASE_TREE tree,
	oris_application_info_t* info)
{
	pANTLR3_BASE_TREE name_node = tree->getChild(tree, 0);
	pANTLR3_BASE_TREE action_tree = tree->getChild(tree, 1);
	pANTLR3_BASE_TREE cond_expr_tree = tree->getChild(tree, 2);
	oris_table_t* tbl;
	int row;

	if (!name_node || !action_tree || !(tbl = oris_get_table(&info->data_tables,
			(const char*) name_node->getText(name_node)->chars))) {

		return;
	}

	if (cond_expr_tree && !oris_expr_as_bool_and_free(
			oris_expr_parse_from_tree(cond_expr_tree))) {
		return;
	}

	row = tbl->current_row;
	ORIS_FOR_EACH_TBL_ROW(tbl) {
		oris_perform_automation_actions(action_tree, info);
	}
	tbl->current_row = row;

}

static void oris_perform_automation_action(pANTLR3_BASE_TREE tree,
	oris_application_info_t* info)
{
	pANTLR3_COMMON_TREE_NODE_STREAM stream;
	pconfigTree walker;

    stream = antlr3CommonTreeNodeStreamNewTree(tree, ANTLR3_SIZE_HINT);
    walker = configTreeNew(stream);

	walker->conditional_action(walker, info);

    stream->free(stream);
    walker->free(walker);

	return;
}

void oris_automation_foreach_action(oris_application_info_t* info,
	const char* request_name, const char* tbl_name)
{
	oris_table_t* tbl = oris_get_table(&info->data_tables, tbl_name);
	pANTLR3_BASE_TREE req_tree = oris_get_request_parse_tree(request_name);
	int l;
	char* r;

	if (!tbl) {
		oris_log_f(LOG_DEBUG, "table %s not found, not performing "
				"foreach using %s",	tbl_name, request_name);
		return;
	}

	l = tbl->current_row;
	for (tbl->current_row = 0; tbl->current_row < tbl->row_count; tbl->current_row++) {
		r = oris_parse_request_tree(req_tree);
		oris_log_f(LOG_DEBUG, "requesting %s for row %d in %s", r, tbl->current_row, tbl->name);
		oris_connections_send(&info->connections, "data", r, strlen(r));
		free(r);
	}
	tbl->current_row = l;
}

void oris_automation_request_action(oris_application_info_t* info,
	const char* request_name)
{
	char* r = oris_get_parsed_request(request_name);

	if (!r) {
		return;
	}

	oris_connections_send(&info->connections, "data", r, strlen(r));
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

static void oris_add_escaped_json_str_to_buffer(struct evbuffer* target,
	oris_parse_expr_t* expr)
{
	char *s, *p;

	p = s = strdup((char*) expr->value.as_string->chars);
	while (s && *p) {
		if (*p == '"') { *p = '\''; }
		p++;
	}

	evbuffer_add_printf(target, "\"%s\"", s);
	free(s);
}

static void oris_add_expr_to_buf(struct evbuffer* target, oris_parse_expr_t* expr)
{
	if (expr->type == ET_INT) {
		evbuffer_add_printf(target, "%d", expr->value.as_int);
	} else if (expr->type == ET_STRING) {
		oris_add_escaped_json_str_to_buffer(target, expr);
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

static void oris_parse_foreach_template(struct evbuffer* buf, oris_table_t* tbl,
	pANTLR3_BASE_TREE template)
{
	char c;
	int l;

	l = tbl->current_row;

	for (tbl->current_row = 0; tbl->current_row < tbl->row_count; tbl->current_row++) {
		if (tbl->current_row > 0) {
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
		evbuffer_add_printf(buf, "{\"v\":");
		oris_add_escaped_json_str_to_buffer(buf, expr);
		evbuffer_add_printf(buf, "}");
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

	oris_perform_http_on_targets(info->targets.items, info->targets.count,
			method, url_str, buf);

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
		oris_parse_foreach_template(buf, tbl, tmpl);
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

    if (tmpl_name) {
		tbl = tbl_name ? oris_get_table(&info->data_tables, tbl_name) : NULL;
		tmpl = oris_get_template_by_name(tmpl_name->getText(tmpl_name));

		if (tbl) {
			/* table given: now perform http record-wise or for the whole table */
			oris_perform_http_on_table(info, method, url, buf, tmpl, tbl, perform_per_record);
		} else {
			/* no table given (or found), parse template and send it */
			oris_parse_template(buf, tmpl, true);
			oris_perform_http_with_buffer(info, method, url, buf);
		}
	} else if (value_expr) {
		oris_dump_expr_value_to_buffer(buf, value_expr);
		oris_perform_http_with_buffer(info, method, url, buf);
		oris_free_expr_value(value_expr);
    } else {
		oris_perform_http_with_buffer(info, method, url, buf);
	}

	evbuffer_free(buf);
}

void oris_automation_set_tbl_record(oris_application_info_t* info,
	const char* tbl_name, pANTLR3_BASE_TREE field,
	pANTLR3_BASE_TREE value_expr)
{
	oris_table_t* tbl = tbl_name ? oris_get_table(&info->data_tables, tbl_name) : NULL;
	oris_parse_expr_t *field_name, *value;
	int field_index;

	if (tbl) {
		field_name = oris_expr_parse_from_tree(field);
		value = oris_expr_parse_from_tree(value_expr);

		if (!oris_expr_as_int(field_name, &field_index)) {
			field_index = oris_table_get_field_index(tbl, (char*) field_name->value.as_string->chars);
			/* create new named field if it does not exists */
			if (field_index == -1) {
				field_index = oris_table_add_field(tbl, (char*) field_name->value.as_string->chars);
			}
		}

		oris_table_set_field(tbl, field_index, (char*) value->value.as_string->chars);

		oris_free_expr_value(field_name);
		oris_free_expr_value(value);
	}
}

