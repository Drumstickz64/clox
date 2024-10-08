#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "memory.h"
#include "object.h"
#include "value.h"

void value_array_init(ValueArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void value_array_free(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    value_array_init(array);
}

void value_array_write(ValueArray* array, Value value) {
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

Value value_get(ValueArray* array, uint8_t i) {
    ASSERT(i < array->count, "index is in bounds");
    return array->values[i];
}

void value_print(Value value) {
    switch (value.type) {
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_OBJ:
            obj_print(value);
            break;
    }
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type)
        return false;

    switch (a.type) {
        case VAL_NIL:
            return true;
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:
            return AS_OBJ(a) == AS_OBJ(b);

        default:
            UNREACHABLE("could not handle value equality");
    }
}
