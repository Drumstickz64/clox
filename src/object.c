#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619

#define ALLOCATE_OBJ(type, obj_type) (type*)allocate_obj(sizeof(type), obj_type)

static Obj* allocate_obj(size_t size, ObjType type) {
    Obj* obj = (Obj*)reallocate(NULL, 0, size);
    obj->type = type;
    obj->is_marked = false;

    obj->next = vm.objects;
    vm.objects = obj;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return obj;
}

static ObjString* allocate_string_obj(char* chars, int len, uint32_t hash) {
    ObjString* string_obj = ALLOCATE_OBJ(ObjString, OBJ_STRING);

    string_obj->len = len;
    string_obj->chars = chars;
    string_obj->hash = hash;

    table_set(&vm.strings, string_obj, NIL_VAL);

    return string_obj;
}

static uint32_t hash_string(const char* key, int len) {
    uint32_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)key[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

ObjClosure* closure_new(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalue_count);
    for (int i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalue_count = function->upvalue_count;
    closure->upvalues = upvalues;
    return closure;
}

ObjFunction* function_new(void) {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    chunk_init(&function->chunk);

    return function;
}

ObjNative* native_new(NativeFn function, int arity) {
    ObjNative* native_fn = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native_fn->function = function;
    native_fn->arity = arity;
    return native_fn;
}

ObjString* copy_string(const char* src, int len) {
    uint32_t hash = hash_string(src, len);
    ObjString* interned = table_find_string(&vm.strings, src, len, hash);
    if (interned) {
        return interned;
    }

    // allocate chars array the string object will point to
    char* string = ALLOCATE(char, len + 1);
    memcpy(string, src, len);
    string[len] = '\0';
    // then allocate the string object itself
    return allocate_string_obj(string, len, hash);
}

ObjUpvalue* upvalue_new(Value* location) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = location;
    upvalue->closed = NIL_VAL;
    upvalue->next = NULL;
    return upvalue;
}

ObjString* take_string(char* string, int len) {
    uint32_t hash = hash_string(string, len);
    ObjString* interned = table_find_string(&vm.strings, string, len, hash);
    if (interned) {
        FREE_ARRAY(char, string, len);
        return interned;
    }

    return allocate_string_obj(string, len, hash);
}

static void function_print(ObjFunction* function) {
    if (!function->name) {
        printf("<script>");
        return;
    }

    printf("<fn %s>", function->name->chars);
}

void obj_print(Value value) {
    ASSERT(value.type == VAL_OBJ, "the value to print is an object");

    switch (OBJ_TYPE(value)) {
        case OBJ_CLOSURE:
            function_print(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            function_print(AS_FUNCTION(value));
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}
