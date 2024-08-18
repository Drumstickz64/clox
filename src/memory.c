#include "memory.h"
#include "assert.h"
#include "object.h"
#include "vm.h"

void* reallocate(void* pointer, size_t old_size, size_t new_size) {
    UNUSED(old_size);

    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    ASSERT(result != NULL, "we are able to allocate memory");
    return result;
}

static void free_object(Obj* object) {
    switch (object->type) {
        case OBJ_CLOSURE: {
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            chunk_free(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(ObjNative, object);
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->len + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_UPVALUE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE(ObjUpvalue, object);
            break;
        }
    }
}

void free_objects(void) {
    Obj* curr_obj = vm.objects;
    while (curr_obj) {
        Obj* next_obj = curr_obj->next;
        free_object(curr_obj);
        curr_obj = next_obj;
    }
}
