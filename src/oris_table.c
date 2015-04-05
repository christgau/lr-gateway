#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#endif

#include "oris_table.h"
#include "oris_util.h"
#include "oris_log.h"

#define DUMP_DELIM ';'

bool oris_table_add_row(oris_table_t* tbl, const char* s, char delim)
{
	char *buf, *ptr;
	int i;

	if (!oris_safe_realloc((void**) &(tbl->rows), tbl->row_count + 1, sizeof(*(tbl->rows)))) {
		return false;
	}

	buf = strdup(s);
	if (buf == NULL) {
		return false;
	}

	tbl->rows[tbl->row_count].field_count = 1;
	for (ptr = buf; *ptr; ptr++) {
		if (*ptr == delim) {
			tbl->rows[tbl->row_count].field_count++;
			*ptr = '\0';
		}
	}

	ptr = buf;
	tbl->rows[tbl->row_count].fields = calloc(tbl->rows[tbl->row_count].field_count,
			sizeof(*tbl->rows[tbl->row_count].fields));
	for (i = 0; i < tbl->rows[tbl->row_count].field_count; i++) {
		tbl->rows[tbl->row_count].fields[i] = strdup(ptr);
		ptr += strlen(ptr) + 1;
	}

	free(buf);

	if (tbl->fields.field_count < tbl->rows[tbl->row_count].field_count) {
		if (!oris_safe_realloc((void**) &tbl->fields.fields,
					tbl->rows[tbl->row_count].field_count,
					sizeof(*tbl->fields.fields))) {
			return false;
		}

		for (i = tbl->fields.field_count; i < tbl->rows[tbl->row_count].field_count; i++) {
			tbl->fields.fields[i] = NULL;
		}

		tbl->fields.field_count = tbl->rows[tbl->row_count].field_count;
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
		tbl->fields.field_count = 0;
		tbl->fields.fields = NULL;
	}
}

static void oris_table_clear_row(oris_table_row_t* row)
{
	int i;

	for (i = 0; i< row->field_count; i++) {
		oris_free_and_null(row->fields[i]);
	}

	row->field_count = 0;
	oris_free_and_null(row->fields);
}

void oris_table_clear(oris_table_t* tbl)
{
	int i;

	for (i = 0; i < tbl->row_count; i++) {
		oris_free_and_null(tbl->rows[i].fields);
		tbl->rows[i].field_count = 0;
	}

	oris_table_clear_row(&tbl->fields);
	tbl->row_count = 0;
	tbl->current_row = -1;
}

void oris_table_finalize(oris_table_t* tbl)
{
	if (tbl) {
		oris_table_clear(tbl);

		free(tbl->name);
		free(tbl->rows);

		tbl->name = NULL;
		tbl->rows = NULL;
	}
}

int oris_table_add_field(oris_table_t* tbl, const char* field_name)
{
	if (!oris_safe_realloc((void**) &tbl->fields.fields, ++tbl->fields.field_count, sizeof(*tbl->fields.fields))) {
		return -1;
	}

	tbl->fields.fields[tbl->fields.field_count - 1] = strdup(field_name);
	return tbl->fields.field_count;
}

int oris_table_get_field_index(oris_table_t* tbl, const char* field)
{
	int i, retval = 0;
	char *name;

	/* check if field is an integer */
	if (oris_strtoint(field, &retval)) {
		return retval - 1;
	}

	/* look up the field in the field name array */
	for (i = 0; i < tbl->fields.field_count; i++) {
		name = tbl->fields.fields[i];
		if (name && strcmp(field, name) == 0) {
			return i + 1;
		}
	}

	return -1;
}

inline static bool oris_table_check_and_reset_cursor(oris_table_t* tbl)
{
	if (tbl == NULL || tbl->row_count == 0) {
		return false;
	}

	if (tbl->current_row == -1) {
		tbl->current_row = 0;
	}

	return true;
}

const char* oris_table_get_field_by_index(oris_table_t* tbl, const int index)
{
	oris_table_row_t* row;

	if (!oris_table_check_and_reset_cursor(tbl)) {
		return NULL;
	}

	row = &(tbl->rows[tbl->current_row]);
	if (index <= 0 || index > (int) row->field_count) {
		return NULL;
	}

	return tbl->rows[tbl->current_row].fields[index - 1];
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
	int* retval = calloc(tbl->fields.field_count, sizeof(*retval));
	int i, j, l;

	if (!retval) {
		return retval;
	}

	for (i = 0; i < tbl->row_count; i++) {
		for (j = 0; j < tbl->rows[i].field_count; j++) {
			l = (int) mbstowcs(NULL, tbl->rows[i].fields[j], 0);
			if (l > retval[j]) {
				retval[j] = l;
			}
		}
	}

	return retval;
}

void oris_table_set_field(oris_table_t* tbl, int index, const char* value)
{
	oris_table_row_t* row;
	int i;

	oris_table_check_and_reset_cursor(tbl);
	row = &(tbl->rows[tbl->current_row]);

	if (index > tbl->fields.field_count || index < 1) {
		return;
	}

	if (index > row->field_count) {
		if (!oris_safe_realloc((void**) row->fields, index, sizeof(*row->fields))) {
			return;
		}

		for (i = row->field_count; i < index - 1; i++) {
			row->fields[i] = strdup("");
		}

		row->field_count = index;
	} else {
		oris_free_and_null(row->fields[index - 1]);
	}

	row->fields[index - 1] = strdup(value);
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

	if (pos_cache < tbl_list->count && strcasecmp(tbl_list->tables[pos_cache].name, name) == 0) {
		return &(tbl_list->tables[pos_cache]);
	}

	for (i = 0; i < tbl_list->count; i++) {
		if (strcasecmp(tbl_list->tables[i].name, name) == 0) {
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

	f = fopen(fname, "w");
	if (!f) {
		oris_log_f(LOG_ERR, "could not open file %s (%d)", fname, errno);
		return false;
	}

	fputs("[Definition]\n", f);
	for (i = 0; i < tables->count; i++) {
		fprintf(f, "%s=", tables->tables[i].name);
		for (j = 0; j < tables->tables[i].fields.field_count; j++) {
			if (j > 0) {
				fputc(';', f);
			}
			if (tables->tables[i].fields.fields[j]) {
				fputs(tables->tables[i].fields.fields[j], f);
			} else {
				fprintf(f, "%d", j + 1);
			}
		}
		fputc('\n', f);
	}

	fputs("\n", f);
	for (i = 0; i < tables->count; i++) {
		fprintf(f, "[%s]\n", tables->tables[i].name);
		for (j = 0; j < tables->tables[i].row_count; j++) {
			row = tables->tables[i].rows[j];
			for (col = 0; col < row.field_count; col++) {
				if (col) {
					fputc(';', f);
				}
				fputs(row.fields[col], f);
			}
			fputs("\n", f);
		}
		fputs("\n", f);
	}

	fclose(f);

	return true;
}

static bool oris_find_table_in_file(FILE* f, const char* tbl_name)
{
	char* line = NULL, *s;
	size_t size = 0;
	ssize_t length;

	rewind(f);

	while ((length = getline(&line, &size, f)) != -1) {
		s = line;
		if ((size_t) length >= strlen(tbl_name) + 2 && *line == '[') {
			s++;
			if (strncasecmp(tbl_name, s, strlen(tbl_name)) == 0 &&
					line[strlen(tbl_name) + 1] == ']') {
				free(line);
				return true;
			}
		}
	}

	free(line);
	return false;
}

static size_t oris_read_table_from_file(FILE* f, oris_table_t* tbl, bool definition)
{
	char* line = NULL, *s;
	size_t size = 0;
	size_t retval = 0;
	ssize_t length;

	if (!oris_find_table_in_file(f, tbl->name)) {
		oris_log_f(LOG_ERR, "table %s not found in data file", tbl->name);
		return 0;
	}

	while ((length = getline(&line, &size, f)) != -1) {
		if (*line && isspace(*line)) {
			break;
		}

		if (iscntrl(line[--length])) {
			line[length--] = 0;
		}

		if (definition && (s = strchr(line, '='))) {
			*s = DUMP_DELIM;
		}

		/* think: allow comments */
		oris_table_add_row(tbl, line, DUMP_DELIM);
		retval++;
	}

	return retval;
}

static void oris_table_add_fields_from_definition(oris_table_t* tbl,
	oris_table_t* def_tbl)
{
	int i;
	oris_table_row_t* def = &def_tbl->rows[def_tbl->current_row];

	if (def->field_count <= 0) {
		return;
	}

	tbl->fields.field_count = def->field_count - 1;
	tbl->fields.fields = calloc(tbl->fields.field_count, sizeof(*tbl->fields.fields));

	for (i = 1; i < def->field_count; i++) {
		tbl->fields.fields[i - 1] = strdup(def->fields[i]);
	}
}

void oris_tables_load_from_file(oris_table_list_t* tables, const char* fname)
{
	FILE* f;
	size_t n = 0;
	const char* name;
	oris_table_t def_tbl = { .name = "Definition" };
	oris_table_t* tbl;

	f = fopen(fname, "r");
	if (!f) {
		oris_log_f(LOG_WARNING, "error while reading tables from %s: %s", fname, strerror(errno));
		return;
	}

	oris_read_table_from_file(f, &def_tbl, true);
	ORIS_FOR_EACH_TBL_ROW(&def_tbl) {
		name = oris_table_get_field_by_index(&def_tbl, 1);
		if (!name || strlen(name) == 0) {
			continue;
		}

		tbl = oris_get_or_create_table(tables, name, true);
		if (!tbl) {
			oris_log_f(LOG_ERR, "unable to create table %s", name);
			continue;
		}

		oris_table_add_fields_from_definition(tbl, &def_tbl);
		n = oris_read_table_from_file(f, tbl, false);
		oris_log_f(LOG_INFO, "loaded %d records for table %s", n, name);
	}

	fclose(f);
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
