#include "memory.h"
#include "assert.h"
#include "compiling/compiler.h"
#include "config.h"
#include "object.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

void* reallocate(void* pointer, size_t old_size, size_t new_size) {
    if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
        collect_garbage();
#endif
    }

    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    ASSERT(result != NULL, "we are able to allocate memory");
    return result;
}

void mark_object(Obj* obj) {
    if (!obj) {
        return;
    }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)obj);
    value_print(OBJ_VAL(obj));
    printf("\n");
#endif

    obj->is_marked = true;
}

static void mark_roots(void) {
    for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
        mark_value(*slot);
    }

    for (int i = 0; i < vm.frame_count; i++) {
        mark_object((Obj*)vm.frames[i].closure);
    }

    for (ObjUpvalue* upvalue = vm.open_upvalues; upvalue != NULL;
         upvalue = upvalue->next) {
        mark_object((Obj*)upvalue);
    }

    mark_table(&vm.globals);

    mark_compiler_roots();
}

void mark_value(Value value) {
    if (IS_OBJ(value)) {
        mark_object(AS_OBJ(value));
    }
}

void collect_garbage(void) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
#endif

    mark_roots();

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
#endif
}

static void free_object(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif
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
