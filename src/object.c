#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, obj_type) (type*)allocate_obj(sizeof(type), obj_type)

static Obj* allocate_obj(size_t size, ObjType type) {
    Obj* obj = (Obj*)reallocate(NULL, 0, size);
    obj->type = type;

    obj->next = vm.objects;
    vm.objects = obj;

    return obj;
}

static ObjString* allocate_string_obj(char* chars, int len) {
    ObjString* string_obj = ALLOCATE_OBJ(ObjString, OBJ_STRING);

    string_obj->len = len;
    string_obj->chars = chars;

    return string_obj;
}

ObjString* copy_string(const char* src, int len) {
    // allocate chars array the string object will point to
    char* string = ALLOCATE(char, len + 1);
    memcpy(string, src, len);
    string[len] = '\0';
    // then allocate the string object itself
    return allocate_string_obj(string, len);
}

ObjString* take_string(char* chars, int len) {
    return allocate_string_obj(chars, len);
}

void obj_print(Value value) {
    ASSERT(value.type == VAL_OBJ, "the value to print is an object");

    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}
