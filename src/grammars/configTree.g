tree grammar expr_eval;

options {
	language=C;
	tokenVocab = configuration;
/*	ASTLabelType = pANTLR3_BASE_TREE; */
}

@header {
#include "oris_util.h"
#include "oris_interpret_tools.h"
}

expr returns [oris_grammar_expr_value_t* value]
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
	: ^(PARAMS ( param = expr { $argv->add($argv, $param.value, oris_free_expr_value_void); } )* )
	;

