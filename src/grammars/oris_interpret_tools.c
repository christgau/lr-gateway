#include <stdlib.h>
#include <string.h>
#include <antlr3encodings.h>
#include <antlr3defs.h>

#include "oris_interpret_tools.h"
#include "oris_util.h"
#include "oris_table.h"
#include "configParser.h"
#include "configTree.h"

#ifdef __GNUC__
#include <locale.h>
#include <strings.h>
#define stricmp strcasecmp
#endif

mem_pool_t* oris_expr_mem_pool = NULL;

typedef oris_parse_expr_t*(*builtin_func_impl_t) (pANTLR3_LIST args);

typedef struct {
	char* name;
	builtin_func_impl_t f;
	size_t num_args;
	size_t opt_args;
} oris_builtin_func_t;

static oris_parse_expr_t* oris_built_in_length(pANTLR3_LIST args);
static oris_parse_expr_t* oris_built_in_quote(pANTLR3_LIST args);
static oris_parse_expr_t* oris_built_in_token(pANTLR3_LIST args);
static oris_parse_expr_t* oris_built_in_lpad(pANTLR3_LIST args);
static oris_parse_expr_t* oris_built_in_rpad(pANTLR3_LIST args);

static oris_builtin_func_t oris_builtin_funcs[] = {
	{ "LENGTH", oris_built_in_length, 1, 0 },
	{ "TOKEN", oris_built_in_token, 3, 0 },
	{ "QUOTE", oris_built_in_quote, 1, 0 },
	{ "LPAD", oris_built_in_lpad, 3, 0 },
	{ "RPAD", oris_built_in_rpad, 3, 0 }
/*	{ "IFTHEN", NULL, 2, 1 },
	{ "UPPERCASE", oris_built_in_uppercase, 1, 0},
	{ "LOWERCASE", NULL, 1, 0},
	{ "LOOKUP", NULL, 0, 0 },
	{ "DATESTR", NULL, 0, 0 },
	{ "TOKEN", NULL, 2, 1 }, */
};

static pANTLR3_STRING_FACTORY strFactory = NULL;
static oris_table_list_t* data_tbls;

bool oris_interpreter_init(oris_table_list_t* tbls)
{
	setlocale(LC_CTYPE, "");
	strFactory = antlr3StringFactoryNew(ANTLR3_ENC_UTF8);
	if (strFactory == NULL) {
		return false;
	}

	oris_expr_mem_pool = create_mem_pool(sizeof(oris_parse_expr_t));
	data_tbls = tbls;

	return (oris_expr_mem_pool);
}

void oris_interpreter_finalize(void)
{
	if (strFactory) {
		strFactory->close(strFactory);
		strFactory = NULL;
	}

	if (oris_expr_mem_pool) {
		destroy_mem_pool(oris_expr_mem_pool);
		oris_expr_mem_pool = NULL;
	}
}

oris_parse_expr_t* oris_alloc_int_value(int value)
{
	oris_parse_expr_t* retval = mem_pool_alloc(oris_expr_mem_pool);
	if (retval) {
		retval->type = ET_INT;
		retval->value.as_int = value;
	}

	return retval;
}

oris_parse_expr_t* oris_alloc_int_value_from_str(const pANTLR3_STRING s)
{
	oris_parse_expr_t* retval = oris_alloc_int_value(0);

	if (!oris_strtoint((char*) s->chars, &retval->value.as_int)) {
		oris_free_expr_value(retval);
		retval = NULL;
	}

	return retval;
}

oris_parse_expr_t* oris_alloc_string_value(const pANTLR3_STRING s)
{
	oris_parse_expr_t* retval = mem_pool_alloc(oris_expr_mem_pool);
	if (retval) {
		retval->type = ET_STRING;
		if (s->chars) {
			retval->value.as_string = strFactory->newStr(strFactory, s->chars);
		} else {
			retval->value.as_string = strFactory->newStr(strFactory, (pANTLR3_UINT8) "");
		}
	}
	
	return retval;
}

oris_parse_expr_t* oris_alloc_value_from_rec_i(const pANTLR3_STRING tbl, const int col)
{
	const char* str;

	oris_parse_expr_t* retval = mem_pool_alloc(oris_expr_mem_pool);
	if (retval) {
		str = oris_tables_get_field_by_number(data_tbls, (const char*) (tbl->chars), col);
		if (str == NULL) {
			str = "";
		}
		retval->type = ET_STRING;
		retval->value.as_string = strFactory->newStr(strFactory, (pANTLR3_UINT8) str);
	}
	
	return retval;
}

oris_parse_expr_t* oris_alloc_value_from_rec_s(const pANTLR3_STRING tbl, const pANTLR3_STRING col)
{
	oris_parse_expr_t* retval = mem_pool_alloc(oris_expr_mem_pool);
	if (retval) {
		retval->type = ET_STRING;
		retval->value.as_string = strFactory->newStr(strFactory,(pANTLR3_UINT8) 
				oris_tables_get_field(data_tbls, (const char*) (tbl->chars), 
				(const char*) col->chars));
	}
	
	return retval;
}


void oris_free_expr_value(oris_parse_expr_t* v)
{
	if (v) {
		if (v->type == ET_STRING && v->value.as_string) {
			v->value.as_string->factory->destroy(
					v->value.as_string->factory,
					v->value.as_string);
		}

		mem_pool_free(oris_expr_mem_pool, v);
	}
}

void oris_free_expr_value_void(void* v)
{
	oris_free_expr_value(v);
}

static void oris_expr_cast_to_str(oris_parse_expr_t *r)
{
	int iv;

	if (r->type == ET_INT) {
		iv = r->value.as_int;
		r->value.as_string = strFactory->newRaw(strFactory);
		r->value.as_string->addi(r->value.as_string, iv);
		r->type = ET_STRING;
	}
}

static void oris_grammar_eval_plus(oris_parse_expr_t* a,
	oris_parse_expr_t* b, oris_parse_expr_t* r)
{
	int ia, ib;

	if (oris_expr_as_int(a, &ia) && oris_expr_as_int(b, &ib)) {
		/* both opr's can be interpreted as int, add them */
		r->type = ET_INT;
		r->value.as_int = ia + ib;
	} else {
		/*  convert to string */
		oris_expr_cast_to_str(a);
		oris_expr_cast_to_str(b);

		r->type = ET_STRING;
		r->value.as_string = strFactory->newSize(strFactory, 
				a->value.as_string->len + 
				b->value.as_string->len);
		r->value.as_string->appendS(r->value.as_string, a->value.as_string);
		r->value.as_string->appendS(r->value.as_string, b->value.as_string);
	}

}

static void oris_grammar_eval_arith_op2(int a, int b, int op, int* r)
{
	switch (op) {
		case MINUS:
			*r = a - b;
			break;
		case MUL:
			*r = a * b;
			break;
		case DIV:
			*r = a / b;
			break;
		case MOD:
			*r = a % b;
			break;
		case OR:
			*r = a || b;
			break;
		case AND:
			*r = a && b;
			break;
		default:
			break;
	}
}

static void oris_expr_eval_log_op2(oris_parse_expr_t* a, 
	oris_parse_expr_t* b, int op, oris_parse_expr_t* r)
{
	int ia, ib;
	int* ir;

	ir = &r->value.as_int;
	if (oris_expr_as_int(a, &ia) && oris_expr_as_int(b, &ib)) {
		/* both are integers */
		switch (op) {
			case EQUAL:
				*ir = ia == ib;
				break;
			case NOT_EQUAL:
				*ir = ia != ib;
				break;
			case LTH:
				*ir = ia < ib;
				break;
			case LE:
				*ir = ia <= ib;
				break;
			case GE:
				*ir = ia >= ib;
				break;
			case GT:
				*ir = ia > ib;
				break;
			default:
				break;
		}
	} else {
		/* at least one operand is a string, convert them in sito */
		oris_expr_cast_to_str(a);
		oris_expr_cast_to_str(b);
		*ir = a->value.as_string->compareS(a->value.as_string, b->value.as_string);
	}
}

oris_parse_expr_t* oris_expr_eval_unary_op(oris_parse_expr_t* a, int op)
{
	if (a->type == ET_INT && op == MINUS) {
		a->value.as_int = -a->value.as_int;
	}

	return a;
}

oris_parse_expr_t* oris_expr_eval_binary_op(oris_parse_expr_t* a, 
	oris_parse_expr_t* b, int op)
{
	oris_parse_expr_t* retval = mem_pool_alloc(oris_expr_mem_pool);
	int ia, ib;

	if (op == PLUS) {
		oris_grammar_eval_plus(a, b, retval);
	} else if (op == MINUS || op == MUL || op == DIV || op == MOD || op == OR || op == AND) {
		if (oris_expr_as_int(a, &ia) && oris_expr_as_int(b, &ib)) {
			retval->type = ET_INT;
			oris_grammar_eval_arith_op2(ia, ib, op, &retval->value.as_int);
		}
	} else if (op == EQUAL || op == NOT_EQUAL || op == LTH || op == LE || op == GE || op == GT) {
		retval->type = ET_INT;
		oris_expr_eval_log_op2(a, b, op, retval);
	}

	/* free operands */
	oris_free_expr_value(a);
	oris_free_expr_value(b);

	return retval;
}

static oris_builtin_func_t* get_builtin_fn_by_name(const char* name)
{
	size_t i;

	for (i = 0; i < sizeof(oris_builtin_funcs) / sizeof(*oris_builtin_funcs); i++) {
		if (stricmp(oris_builtin_funcs[i].name, name) == 0) {
			return &oris_builtin_funcs[i];
		}
	}

	return NULL;
}

oris_parse_expr_t* oris_expr_eval_function(const pANTLR3_STRING fname,
	pANTLR3_LIST args)
{
	oris_builtin_func_t* fn;
	oris_parse_expr_t* retval = NULL;

	fn = get_builtin_fn_by_name((char*) fname->chars);
	if (fn) {
		if (args->size(args) < fn->num_args) {
			fprintf(stderr, "too few argument  (%d) to function %s", args->size(args), fname->chars);
		}

		retval = fn->f(args);
	} else {
		fprintf(stderr, "unknown function %s", fname->chars);
	}

	args->free(args);
	return retval;
}

char* oris_expr_as_string(const oris_parse_expr_t* expr)
{
	char* retval = NULL;
	char itoabuf[16] = { 0 };

	if (expr) {
		if (expr->type == ET_STRING) {
			retval = strdup((char*) expr->value.as_string->chars);
		} else {
			snprintf(itoabuf, sizeof(itoabuf) - 1, "%d", expr->value.as_int);
			retval = strdup(itoabuf);
		}
	}

	return retval;
}

bool oris_expr_as_int(const oris_parse_expr_t* expr, int* v)
{
	int retval = expr != NULL;
	int tmp;

	if (retval) {
		if (expr->type == ET_INT) {
			*v = expr->value.as_int;
			retval = true;
		} else {
			retval = oris_strtoint((char*) expr->value.as_string->chars, &tmp);
			if (retval) {
				*v = tmp;
			}
		}
	}

	return retval;
}

bool oris_expr_as_bool(const oris_parse_expr_t* expr, bool* v)
{
	int iv;

	if (oris_expr_as_int(expr, &iv)) {
		*v = iv != 0;
		return true;
	} 

	return false;
}

void oris_expr_dump(const oris_parse_expr_t* v)
{
	if (v->type == ET_NONE) {
		printf("undefined\n");
	} else if (v->type == ET_INT) {
		printf("int: %d\n", v->value.as_int);
	} else if (v->type == ET_STRING) {
		printf("string: %s\n", (char*) v->value.as_string->chars);
	}
}

/* builtin function implementation */

static oris_parse_expr_t* oris_built_in_length(pANTLR3_LIST args)
{
	oris_parse_expr_t* arg = args->get(args, 1);
	oris_expr_cast_to_str(arg);

	return oris_alloc_int_value(mbstowcs(NULL, (char*) arg->value.as_string->chars, 0));
}

static oris_parse_expr_t* oris_built_in_quote(pANTLR3_LIST args)
{
	oris_parse_expr_t* arg = oris_alloc_string_value(args->get(args, 1));

	arg->value.as_string->insert(arg->value.as_string, 0, "\"");
	arg->value.as_string->append(arg->value.as_string, "\"");
	
	return arg;
}

static oris_parse_expr_t* oris_built_in_token(pANTLR3_LIST args)
{
	oris_parse_expr_t* str_arg = args->get(args, 1);
	oris_parse_expr_t* nr_arg = args->get(args, 2);
	oris_parse_expr_t* delim_str = args->get(args, 3);
	oris_parse_expr_t* retval = NULL;
	int nr; 
	ANTLR3_UINT32 start, end;
	ANTLR3_UCHAR delim;
	pANTLR3_STRING token, str;

	oris_expr_cast_to_str(str_arg);
	oris_expr_cast_to_str(delim_str);

	str = str_arg->value.as_string;
	delim = delim_str->value.as_string->charAt(delim_str->value.as_string, 0);

	if (oris_expr_as_int(nr_arg, &nr)) {
		if (nr <= 0) {
			return oris_alloc_string_value(NULL);
		}

		/* search starting delim */
		for (start = 0; start < str->len && nr > 1; start++) {
			if (str->charAt(str, start) == delim) {
				nr--;
			}
		}

		/* search end of record */
		for (end = start + 1; end < str->len && str->charAt(str, end) != delim; end++);

		token = str->subString(str, start, end);
		retval = oris_alloc_string_value(token);
		/*token->factory->destroy(token->factory, token);*/
	} else {
		oris_log_f(LOG_CRIT, "invalid 2nd argument for TOKEN");
	}

	return retval;
}

static oris_parse_expr_t* oris_built_in_lpad(pANTLR3_LIST args)
{
	oris_parse_expr_t* str_arg = oris_alloc_string_value(args->get(args, 1));
	oris_parse_expr_t* minlen_arg = args->get(args, 2);
	oris_parse_expr_t* fill_arg = args->get(args, 3);
	pANTLR3_STRING fill;
	int minlen;

	oris_expr_cast_to_str(fill_arg);
	fill = fill_arg->value.as_string;

	if (oris_expr_as_int(minlen_arg, &minlen)) {
		while ((int) str_arg->value.as_string->len < minlen) {
			str_arg->value.as_string->insertS(str_arg->value.as_string, 0, fill);
		}
	}
	
	return str_arg;
}


static oris_parse_expr_t* oris_built_in_rpad(pANTLR3_LIST args)
{
	oris_parse_expr_t* str_arg = oris_alloc_string_value(args->get(args, 1));
	oris_parse_expr_t* minlen_arg = args->get(args, 2);
	oris_parse_expr_t* fill_arg = args->get(args, 3);
	pANTLR3_STRING fill;
	int minlen;

	oris_expr_cast_to_str(fill_arg);
	fill = fill_arg->value.as_string;

	if (oris_expr_as_int(minlen_arg, &minlen)) {
		while ((int) str_arg->value.as_string->len < minlen) {
			str_arg->value.as_string->appendS(str_arg->value.as_string, fill);
		}
	}	
	
	return str_arg;
}


oris_parse_expr_t* oris_expr_parse_from_tree(const pANTLR3_BASE_TREE tree)
{
	pANTLR3_COMMON_TREE_NODE_STREAM node_stream;
	pconfigTree walker;
    oris_parse_expr_t* expr;

    node_stream = antlr3CommonTreeNodeStreamNewTree(tree, ANTLR3_SIZE_HINT);
	if (node_stream == NULL) {
		return NULL;
	}

    walker = configTreeNew(node_stream);
	if (walker == NULL) {
		return NULL;
	}

    expr = walker->expr(walker);

    node_stream->free(node_stream);
    walker->free(walker);

	return expr;
}
