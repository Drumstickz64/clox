#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum ValueType {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct Value {
    ValueType type;
    union as {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

#define IS_BOOL(value_struct) ((value_struct).type == VAL_BOOL)
#define IS_NIL(value_struct) ((value_struct).type == VAL_NIL)
#define IS_NUMBER(value_struct) ((value_struct).type == VAL_NUMBER)
#define IS_OBJ(value_struct) ((value_struct).type == VAL_OBJ)

#define AS_BOOL(value_struct) ((value_struct).as.boolean)
#define AS_NUMBER(value_struct) ((value_struct).as.number)
#define AS_OBJ(value_struct) ((value_struct).as.obj)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = (value)}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = (value)}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)(object)}})

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
bool values_equal(Value a, Value b);

#endif
