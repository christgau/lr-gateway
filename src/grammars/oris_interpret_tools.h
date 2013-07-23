#ifndef __ORIS_INTERPRET_TOOLS_H
#define __ORIS_INTERPRET_TOOLS_H

#include <stdlib.h>
#include <stdbool.h>
#include <antlr3string.h>

#include "oris_table.h"
#include "mem_pool.h"

/* type defs */

/* type of an expression (an integer or string) */
typedef enum {ET_NONE, ET_STRING, ET_INT} oris_grammar_expr_type_t;

/* representation of an AST nodes value */
typedef struct {
	oris_grammar_expr_type_t type;
	union {
		ANTLR3_INT32 as_int;
		pANTLR3_STRING as_string;
	} value;
} oris_parse_expr_t;

/* globals */

extern mem_pool_t* oris_expr_mem_pool;

/* functions */
bool oris_interpreter_init(oris_table_list_t* tbls);
void oris_interpreter_finalize(void);

oris_parse_expr_t* oris_alloc_int_value(int value);
oris_parse_expr_t* oris_alloc_int_value_from_str(const pANTLR3_STRING s);
oris_parse_expr_t* oris_alloc_string_value(const pANTLR3_STRING s);

oris_parse_expr_t* oris_alloc_value_from_rec_i(const pANTLR3_STRING tbl, int col);
oris_parse_expr_t* oris_alloc_value_from_rec_s(const pANTLR3_STRING tbl, const pANTLR3_STRING col);

void oris_free_expr_value(oris_parse_expr_t* v);
void oris_free_expr_value_void(void* v);

oris_parse_expr_t* oris_expr_eval_unary_op(oris_parse_expr_t* a, int op);
oris_parse_expr_t* oris_expr_eval_binary_op(oris_parse_expr_t* a, 
	oris_parse_expr_t* b, int op);

oris_parse_expr_t* oris_expr_eval_function(const pANTLR3_STRING fname,
	pANTLR3_LIST args);

char* oris_expr_as_string(const oris_parse_expr_t* expr);
bool oris_expr_as_int(const oris_parse_expr_t* expr, int* v);
bool oris_expr_as_bool(const oris_parse_expr_t* expr, bool* v);

void oris_expr_dump(const oris_parse_expr_t* v);

#endif /* __ORIS_INTERPRET_TOOLS_H  */
