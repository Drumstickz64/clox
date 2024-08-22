#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"

#define TABLE_MAX_LOAD 0.75

static Entry* find_entry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;
    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == key) {
            return entry;
        }

        if (!entry->key) {
            if (IS_NIL(entry->value)) {
                // empty entry
                return tombstone != NULL ? tombstone : entry;
            } else {
                // tombstone
                tombstone = entry;
            }
        }

        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(Table* table, int new_capacity) {
    // TODO(OPT): merge the two loops
    Entry* entries = ALLOCATE(Entry, new_capacity);

    for (int i = 0; i < new_capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* src_entry = &table->entries[i];
        if (!src_entry->key) {
            continue;
        }

        Entry* dest_entry = find_entry(entries, new_capacity, src_entry->key);
        dest_entry->key = src_entry->key;
        dest_entry->value = src_entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = new_capacity;
}

void table_init(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void table_free(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->count);
    table_init(table);
}

bool table_get(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (!entry->key) {
        return false;
    }

    *value = entry->value;
    return true;
}

bool table_set(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int new_capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, new_capacity);
    }
    Entry* entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    bool is_non_tombstone = is_new_key && IS_NIL(entry->value);
    if (is_non_tombstone) {
        table->count++;
    }

    entry->key = key;
    entry->value = value;

    return is_new_key;
}

bool table_delete(Table* table, ObjString* key) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key) {
        return false;
    }

    // place tombstone
    entry->key = NULL;
    entry->value = BOOL_VAL(false);
    return true;
}

void table_add_all(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* src_entry = &from->entries[i];
        if (!src_entry->key) {
            continue;
        }

        table_set(to, src_entry->key, src_entry->value);
    }
}

ObjString* table_find_string(Table* table,
                             const char* chars,
                             int len,
                             uint32_t hash) {
    if (table->count == 0) {
        return NULL;
    }

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];

        if (!entry->key) {
            if (IS_NIL(entry->value)) {
                // Stop if we find an empty non-tombstone entry.
                return NULL;
            }
        } else if (len == entry->key->len && hash == entry->key->hash &&
                   memcmp(chars, entry->key->chars, len) == 0) {
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

void table_remove_white(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            table_delete(table, entry->key);
        }
    }
}

void mark_table(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        mark_object((Obj*)entry->key);
        mark_value(entry->value);
    }
}
