#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#ifdef _WIN32
#include <io.h>
#endif

#include "oris_table.h"
#include "oris_util.h"
#include "oris_log.h"

#define DUMP_DELIM ';'

bool oris_table_add_row(oris_table_t* tbl, const char* s, char delim)
{
	char* buf;

	if (!oris_safe_realloc((void**) &(tbl->rows), tbl->row_count + 1, sizeof(*(tbl->rows)))) {
		return false;
	}

	buf = strdup(s);
	if (buf == NULL) {
		return false;
	}

	tbl->rows[tbl->row_count].field_count = 1;
	tbl->rows[tbl->row_count].buffer = buf;

	while (*buf) {
		if (*buf == delim) {
			*buf = '\0';
			tbl->rows[tbl->row_count].field_count++;
		}
		buf++;
	}

	if (tbl->field_count < tbl->rows[tbl->row_count].field_count) {
		tbl->field_count = tbl->rows[tbl->row_count].field_count;
	}

	tbl->row_count++;

	return true;
}


void oris_table_init(oris_table_t* tbl)
{
	if (tbl) {
		memset(tbl, 0, sizeof(*tbl));
		tbl->current_row = -1;
		tbl->name = NULL;
		tbl->rows = NULL;
		tbl->state = COMPLETE;
	}
}

void oris_table_clear(oris_table_t* tbl)
{
	int i;

	for (i = 0; i < tbl->row_count; i++) {
		oris_free_and_null(tbl->rows[i].buffer);
		tbl->rows[i].field_count = 0;
	}

	tbl->row_count = 0;
	tbl->current_row = -1;
}

void oris_table_finalize(oris_table_t* tbl)
{
	if (tbl) {
		oris_table_clear(tbl);

		free(tbl->name);
		free(tbl->rows);
		free(tbl->field_names);

		tbl->name = NULL;
		tbl->rows = NULL;
		tbl->field_names = NULL;
	}
}

static int oris_table_get_field_index(oris_table_t* tbl, const char* field)
{
	int i, retval = 0;
	char *name;

	/* check if field is an integer */
	if (oris_strtoint(field, &retval)) {
		return retval - 1;
	}

	/* look up the field in the field name array */
	name = tbl->field_names;
	for (i = 0; i < tbl->field_count; i++) {
		if (strcmp(field, name) == 0) {
			return i;
		}

		name = name + strlen(name) + 1;
	}

	return -1;
}

const char* oris_table_get_field_by_index(oris_table_t* tbl, const int index)
{
	oris_table_row_t* row;
	char* retval;
    int i;

	if (tbl == NULL || tbl->row_count == 0) {
		return NULL;
	}

	if (tbl->current_row == -1) {
		tbl->current_row = 0;
	}

	row = &(tbl->rows[tbl->current_row]);
	if (index < 0 || index > (int) row->field_count) {
		return NULL;
	}

	retval = row->buffer;
	if (retval) {
		for (i = 1; i < index; i++) {
			retval = retval + strlen(retval) + 1;
		}
	}

	return retval;
}

const char* oris_table_get_field(oris_table_t* tbl, const char* field)
{
	int index;

	if (tbl == NULL || tbl->row_count == 0) {
		return NULL;
	}

	index = oris_table_get_field_index(tbl, field);

    return oris_table_get_field_by_index(tbl, index);
}

int* oris_table_get_field_widths(oris_table_t* tbl)
{
	int* retval = calloc(tbl->field_count, sizeof(*retval));
	int i, j, l;
	char* buf;

	if (!retval) {
		return retval;
	}

	for (i = 0; i < tbl->row_count; i++) {
		buf = tbl->rows[i].buffer;
		for (j = 0; j < tbl->rows[i].field_count; j++) {
			l = (int) strlen(buf);
			if (l > retval[j]) {
				retval[j] = l;
			}
			buf += l + 1;
		}
	}

	return retval;
}

/* table list functions */
void oris_tables_init(oris_table_list_t* list)
{
	list->count = 0;
	list->tables = NULL;
}

void oris_tables_finalize(oris_table_list_t* list)
{
	size_t i;

	for (i = 0; i < list->count; i++) {
		oris_table_finalize(&list->tables[i]);
	}

	list->count = 0;
	oris_free_and_null(list->tables);
}

oris_table_t* oris_get_or_create_table(oris_table_list_t* tbl_list,
	const char * name, bool create)
{
	size_t i, inspos;
	static size_t pos_cache = 0;

	if (name == NULL) {
		return NULL;
	}

	if (pos_cache < tbl_list->count && strcmp(tbl_list->tables[pos_cache].name, name) == 0) {
		return &(tbl_list->tables[pos_cache]);
	}

	for (i = 0; i < tbl_list->count; i++) {
		if (strcmp(tbl_list->tables[i].name, name) == 0) {
			pos_cache = i;
			return &(tbl_list->tables[i]);
		}
	}

	/* nothing found, create new table */
	if (create && oris_safe_realloc((void**) &(tbl_list->tables), tbl_list->count + 1, sizeof(*(tbl_list->tables)))) {

		/* find correct position in table list and shift array if */
		if (tbl_list->count > 0) {
			inspos = tbl_list->count - 1;

			while (inspos > 0 && strcmp(tbl_list->tables[inspos].name, name) > 0) {
				tbl_list->tables[i] = tbl_list->tables[i - 1];
				inspos--;
			}
		} else {
			inspos = 0;
		}

		tbl_list->count++;
		oris_table_init(&(tbl_list->tables[i]));
		tbl_list->tables[i].name = strdup(name);

		pos_cache = i;
		return &(tbl_list->tables[i]);
	} else {
		return NULL;
	}
}

bool oris_tables_dump_to_file(oris_table_list_t* tables, const char* fname)
{
	int j, col;
	size_t i;
	FILE* f;
	oris_table_row_t row;
	char* c;

	f = fopen(fname, "w");
	if (!f) {
		oris_log_f(LOG_ERR, "could not open file %s (%d)", fname, errno);
		return false;
	}

	fputs("[Definition]\n", f);
	for (i = 0; i < tables->count; i++) {
		fprintf(f, "%s=\n", tables->tables[i].name);
	}

	fputs("\n", f);
	for (i = 0; i < tables->count; i++) {
		fprintf(f, "[%s]\n", tables->tables[i].name);
		for (j = 0; j < tables->tables[i].row_count; j++) {
			row = tables->tables[i].rows[j];
			col = 1;
			c = row.buffer;
			while (col < row.field_count) {
				if (*c == '\0') {
					col++;
					fputc(DUMP_DELIM, f);
				} else {
					fputc(*c, f);
				}
				c++;
			}
		}
		fputs("\n", f);
	}

	fclose(f);

	return true;
}

/* misc/shortcut functions */
const char* oris_tables_get_field_by_number(oris_table_list_t* list,
    const char* name, const int index)
{
	oris_table_t* tbl;

	tbl = oris_get_table(list, name);

	return (tbl ? oris_table_get_field_by_index(tbl, index) : NULL);
}

const char* oris_tables_get_field(oris_table_list_t* list, const char* name,
	const char* field)
{
	oris_table_t* tbl;

	tbl = oris_get_table(list, name);

	return (tbl ? oris_table_get_field(tbl, field) : NULL);
}
