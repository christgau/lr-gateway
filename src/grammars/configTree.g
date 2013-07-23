tree grammar configTree;

options {
	language=C;
	tokenVocab = config;
	backtrack=true;
}

@header {
#include <stdbool.h>

#include "oris_app_info.h"
#include "oris_automation_types.h"
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
	: ^(CONNECTIONS (key=IDENTIFIER value=expr { oris_create_connection(info, $key.text->chars, value); } )* )
	;


targets[oris_application_info_t* info]
	: ^(TARGETS (key=IDENTIFIER value=expr { 
			char* v_str = oris_expr_as_string($value.value);
			oris_config_add_target(info, $key.text->chars, v_str);
			free(v_str);
		} )*  )
	;

automation[oris_application_info_t* info, oris_automation_event_t* automation_event]
	: ^(AUTOMATION event[info, automation_event]*) 
	;

event[oris_application_info_t* info, oris_automation_event_t* event]
	@init {	in_event = true; }
	@after { in_event = false; }
	: ^(EVENT o=object{ do_action = oris_is_same_automation_event(event, &o); } ACTIONLIST action[info]*) 
	;

object returns [oris_automation_event_t event]
	: ^(CONNECTION state=(ESTABLISHED|CLOSED)) { event.type = EVT_CONNECTION; event.name = $state.text->chars; }
	| ^(TABLE name=IDENTIFIER) { event.type = EVT_TABLE; event.name = $name.text->chars; }
	| ^(COMMAND cmd=STRING) { event.type = EVT_COMMAND; event.name = $cmd.text->chars; }
	;

action[oris_application_info_t* info]
	@init {	in_action = true; }
	@after { in_action = false;	}
	: ^(ITERATE tbl_name=IDENTIFIER) { oris_automation_iterate_action(info, $tbl_name.text->chars); }
	| ^(REQUEST name=IDENTIFIER) { oris_automation_request_action(info, $name.text->chars); }
	| ^(HTTP method=http_method url=expr ( tmpl_name=IDENTIFIER | value=expr )) { /* oris_automation_http_action(); */ }
	;

http_method
	: 'get'
	| 'put'
	| 'post'
	| 'delete'
	;

kv_list returns [pANTLR3_LIST list]
	@init 
		{
			list = antlr3ListNew(sizeof(*list));
		}
	: (key=IDENTIFIER value=expr  { 
			$list->add($list, oris_create_kv_pair($key.text->chars, $value.value, oris_free_expr_value_void), oris_free_kv_pair_void); 
		} )*
	;

expr returns [oris_parse_expr_t* value]
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
	: ^(TEMPLATE IDENTIFIER kv_list)
	;

requests_definition
	: ^(REQUESTS kv_list)
	;
