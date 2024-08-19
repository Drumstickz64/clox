#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"

typedef enum ObjType {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;

struct Obj {
    ObjType type;
    bool is_marked;
    struct Obj* next;
};

typedef struct ObjFunction {
    Obj obj;
    int arity;
    int upvalue_count;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int arg_count, Value* args);

typedef struct ObjNative {
    Obj obj;
    int arity;
    NativeFn function;
} ObjNative;

typedef struct ObjString {
    Obj obj;
    int len;
    char* chars;
    uint32_t hash;
} ObjString;

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct ObjClosure {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalue_count;
} ObjClosure;

#define OBJ_TYPE(value_struct) (AS_OBJ(value_struct)->type)

#define IS_CLOSURE(obj) (obj_is_type(obj, OBJ_CLOSURE))
#define IS_FUNCTION(obj) (obj_is_type(obj, OBJ_FUNCTION))
#define IS_NATIVE(obj) (obj_is_type(obj, OBJ_NATIVE))
#define IS_STRING(obj) (obj_is_type(obj, OBJ_STRING))

#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value)))

static inline bool obj_is_type(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

ObjClosure* closure_new(ObjFunction* function);
ObjFunction* function_new(void);
ObjNative* native_new(NativeFn function, int arity);
ObjString* copy_string(const char* src, int len);
ObjUpvalue* upvalue_new(Value* location);
ObjString* take_string(char* chars, int len);
void obj_print(Value value);

#endif
