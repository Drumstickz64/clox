#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;

typedef struct ValueArray {
    int count;
    int capacity;
    Value *values;
} ValueArray;

void value_array_init(ValueArray *value_array);
void value_array_free(ValueArray *value_array);
void value_array_write(ValueArray *value_array, Value value);
Value value_get(ValueArray *array, int i);
void value_print(Value value);

#endif