#include <stdio.h>

#include "assert.h"
#include "memory.h"
#include "value.h"

void value_array_init(ValueArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void value_array_free(ValueArray *array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    value_array_init(array);
}

void value_array_write(ValueArray *array, Value value) {
    // why not array->count == array->capacity?
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values =
            GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

Value value_get(ValueArray *array, uint8_t i) {
    ASSERT(i < array->count, "index is in bounds");
    return array->values[i];
}

void value_print(Value value) { printf("%g", value); }
