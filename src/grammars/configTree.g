tree grammar configTree;

options {
	language=C;
	tokenVocab = config;
	backtrack=true;
}

@header {
#include <stdbool.h>
#include <event2/http.h>

#include "oris_app_info.h"
#include "oris_automation_types.h"
#include "oris_automation.h"
#include "oris_configuration.h"
#include "oris_interpret_tools.h"
#include "oris_kvpair.h"
#include "oris_connection.h"
#include "oris_socket_connection.h"
#include "oris_log.h"
}

@members {
	bool in_event = false;
	bool do_action = false;
	bool in_action = false;
	bool in_configuration = false;
}

@synpredgate { BACKTRACKING == 0 && (in_configuration || (in_event && !in_action) || (in_action && do_action)) }

configuration[oris_application_info_t* value]
	@init {
		in_configuration = true;
	}
	@after {
		in_configuration = false;
	}
	: ^(CONFIG connections[value] targets[value] )
	;

connections[oris_application_info_t* info]
	: ^(CONNECTIONS (key=IDENTIFIER value=expr { oris_create_connection(info, (const char*) $key.text->chars, value); } )* )
	;

targets[oris_application_info_t* info]
	: ^(TARGETS (key=IDENTIFIER value=expr {
			char* v_str = oris_expr_as_string($value.value);
			oris_config_add_target(info, (const char*) $key.text->chars, v_str);
			free(v_str);
		} )*  )
	;

automation[oris_application_info_t* info, oris_automation_event_t* automation_event]
	: ^(AUTOMATION event[info, automation_event]*)
	;

event[oris_application_info_t* info, oris_automation_event_t* event]
	@init {	in_event = true; }
	@after { in_event = false; }
	: ^(EVENT o=object{ do_action = oris_is_same_automation_event(event, &o); } ACTIONLIST conditional_action[info]*)
	;

object returns [oris_automation_event_t event]
	: ^(CONNECTION state=(ESTABLISHED|CLOSED)) { oris_init_automation_event(&event, EVT_CONNECTION, (const char*) $state.text->chars); }
	| ^(TABLE name=IDENTIFIER) { oris_init_automation_event(&event, EVT_TABLE, (const char*) $name.text->chars); }
	| ^(COMMAND cmd=STRING) { oris_init_automation_event(&event, EVT_COMMAND, (const char*) $cmd.text->chars); }
	;

conditional_action [oris_application_info_t* info]
	: action[info]
	| ^(COND_ACTION cond=expr{ do_action = do_action && oris_expr_as_bool_and_free(cond); } action[info])
	;

action[oris_application_info_t* info]
	@init {	in_action = true; value = NULL; it = false; tbl=NULL; }
	@after { in_action = false; }
	: ^(FOREACH req=IDENTIFIER tbl=IDENTIFIER) { oris_automation_foreach_action(info, (const char*) $req.text->chars, (const char*) $tbl.text->chars); }
	| ^(REQUEST name=IDENTIFIER) { oris_automation_request_action(info, (const char*) $name.text->chars); }
	| ^(HTTP method=http_method url=exprTree ( tmpl_name=IDENTIFIER (it=table_iterate tbl=IDENTIFIER)? | value=expr )? ) { oris_automation_http_action(info, method, $url.start, $tmpl_name, value, $tbl != NULL ? (const char*) $tbl.text->chars : NULL, $it.value); }
	;

http_method returns [enum evhttp_cmd_type http_method]
	@init { http_method = EVHTTP_REQ_GET; }
	: 'get' { $http_method = EVHTTP_REQ_GET;  }
	| 'put' { $http_method =  EVHTTP_REQ_PUT; }
	| 'post' { $http_method = EVHTTP_REQ_POST; }
	| 'delete' { $http_method = EVHTTP_REQ_DELETE; }
	;

table_iterate returns [bool value]
	@init { value = false; }
	: 'table' { $value = false; }
	| 'record' { $value = true; }
	;

kv_list returns [pANTLR3_LIST list]
	@init
		{
			list = antlr3ListNew(sizeof(*list));
		}
	: (key=IDENTIFIER value=expr  {
			$list->add($list, oris_create_kv_pair((const char*) $key.text->chars, $value.value, oris_free_expr_value_void), oris_free_kv_pair_void);
		} )*
	;

exprTree
	: ^((EQUAL | NOT_EQUAL | LTH | LE | GE | GT | PLUS | MINUS | OR | MUL | DIV | MOD | AND)  exprTree exprTree)
	| ^(RECORD IDENTIFIER INTEGER)
	| ^(RECORD IDENTIFIER IDENTIFIER)
	| ^(FUNCTION IDENTIFIER ^(PARAMS exprTree*))
	| INTEGER
	| STRING
	;

expr returns [oris_parse_expr_t* value]
	@init { value = NULL; }
	: ^(op=(EQUAL | NOT_EQUAL | LTH | LE | GE | GT | PLUS | MINUS | OR | MUL | DIV | MOD | AND)  a=expr b=expr)
		{
			$value = oris_expr_eval_binary_op(a, b, $op.type);
		}
/*
   	: ^(op=(PLUS | MINUS) expr)
		{
			$value = oris_expr_eval_unary_op($op.type, factor);
		}
*/
	| ^(RECORD table=IDENTIFIER column=INTEGER)
		{
			int col = $column.text->toInt32($column.text);
			$value = oris_alloc_value_from_rec_i($table.text, col);
		}
	| ^(RECORD table=IDENTIFIER column=IDENTIFIER)
		{
			$value = oris_alloc_value_from_rec_s($table.text, $column.text);
		}
	| ^(FUNCTION IDENTIFIER parameters)
		{
			$value = oris_expr_eval_function($IDENTIFIER.text, $parameters.argv);
		}
	| INTEGER
		{
			$value = oris_alloc_int_value_from_str($INTEGER.text);
		}
	| STRING
		{
			$value = oris_alloc_string_value($STRING.text);
		}
	;

parameters returns [pANTLR3_LIST argv]
	@init
		{
			argv = antlr3ListNew(sizeof(*argv));
		}
	: ^(PARAMS ( param=expr { $argv->add($argv, $param.value, oris_free_expr_value_void); } )* )
	;

template_definition
	: ^(TEMPLATE IDENTIFIER IDENTIFIER kv_list)
	;

requests_definition
	: ^(REQUESTS kv_list)
	;
