#include "memory.h"
#include "assert.h"
#include "object.h"
#include "vm.h"

void* reallocate(void* pointer, size_t old_size, size_t new_size) {
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
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->len + 1);
            FREE(ObjString, object);
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