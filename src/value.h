#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef enum ValueType {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
} ValueType;

typedef struct Value {
    ValueType type;
    union as {
        bool boolean;
        double number;
    } as;
} Value;

#define IS_BOOL(value_struct) ((value_struct).type == VAL_BOOL)
#define IS_NIL(value_struct) ((value_struct).type == VAL_NIL)
#define IS_NUMBER(value_struct) ((value_struct).type == VAL_NUMBER)

#define AS_BOOL(value_struct) ((value_struct).as.boolean)
#define AS_NUMBER(value_struct) ((value_struct).as.number)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = (value)}})
#define NIL_VAL() ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = (value)}})

typedef struct ValueArray {
    int count;
    int capacity;
    Value* values;
} ValueArray;

void value_array_init(ValueArray* value_array);
void value_array_free(ValueArray* value_array);
void value_array_write(ValueArray* value_array, Value value);
Value value_get(ValueArray* array, uint8_t i);
void value_print(Value value);

#endif