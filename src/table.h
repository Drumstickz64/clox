#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct Entry {
    ObjString* key;
    Value value;
} Entry;

typedef struct Table {
    int count;
    int capacity;
    Entry* entries;
} Table;

void table_init(Table* table);
void table_free(Table* table);
bool table_get(Table* table, ObjString* key, Value* value);
bool table_set(Table* table, ObjString* key, Value value);
bool table_contains(Table* table, ObjString* key);
bool table_delete(Table* table, ObjString* key);
void table_add_all(Table* from, Table* to);
ObjString* table_find_string(Table* table,
                             const char* chars,
                             int len,
                             uint32_t hash);
void table_remove_white(Table* table);
void mark_table(Table* table);

#endif
