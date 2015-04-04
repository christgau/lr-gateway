#ifndef __ORIS_TABLE_H
#define __ORIS_TABLE_H

#include <stdbool.h>
#include <stddef.h>

#define ORIS_TABLE_ITEM_SEPERATOR '|'
#define ORIS_FOR_EACH_TBL_ROW(tbl) \
	for ((tbl)->current_row = 0; (tbl)->current_row < (tbl)->row_count; \
		(tbl)->current_row++)

/* a row within a table */
typedef struct oris_table_row {
	char** fields;
	int field_count;
} oris_table_row_t;


typedef enum { RECEIVING, COMPLETE } oris_table_recv_state;

/* a single named table with an index to the current (active) row */
typedef struct oris_table {
	char* name;
	int current_row;
	int row_count;
	oris_table_recv_state state;
	oris_table_row_t* rows;
	oris_table_row_t fields;
} oris_table_t;


/* list of tables */
typedef struct oris_table_list {
	size_t count;
	oris_table_t* tables;
} oris_table_list_t;


/* table row functions */
bool oris_table_add_row(oris_table_t* table, const char* s, char delim);


/* table functions */
void oris_table_init(oris_table_t* tbl);
void oris_table_clear(oris_table_t* tbl);
void oris_table_finalize(oris_table_t* tbl);
const char* oris_table_get_field(oris_table_t* tbl, const char* field);
const char* oris_table_get_field_by_index(oris_table_t* tbl, const int index);
int* oris_table_get_field_widths(oris_table_t* tbl);


/* table list functions */
void oris_tables_init(oris_table_list_t* list);
void oris_tables_finalize(oris_table_list_t* list);

oris_table_t* oris_get_or_create_table(oris_table_list_t* tbl_list,
	const char * name, bool create);

bool oris_tables_dump_to_file(oris_table_list_t* tables, const char* fname);
void oris_tables_load_from_file(oris_table_list_t* tables, const char* fname);

#define oris_get_table(tbls, name) oris_get_or_create_table(tbls, name, false)

/* misc/shortcut functions */
const char* oris_tables_get_field_by_index(oris_table_list_t* list,
    const char* name, const int index);

const char* oris_tables_get_field(oris_table_list_t* list, const char* name,
	const char* field);

const char* oris_tables_get_field_by_number(oris_table_list_t* list,
    const char* name, const int index);

#endif
