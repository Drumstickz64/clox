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

    obj->next = vm.objects;
    vm.objects = obj;

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

ObjFunction* function_new(void) {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    chunk_init(&function->chunk);

    return function;
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
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_FUNCTION:
            function_print(AS_FUNCTION(value));
            break;
    }
}
