#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

typedef enum ObjType {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

typedef struct ObjString {
    Obj obj;
    int len;
    char* chars;
    uint32_t hash;
} ObjString;

#define OBJ_TYPE(value_struct) (AS_OBJ(value_struct)->type)

#define IS_STRING(obj) (obj_is_type(obj, OBJ_STRING))

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

static inline bool obj_is_type(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

ObjString* copy_string(const char* src, int len);
ObjString* take_string(char* chars, int len);
void obj_print(Value value);

#endif