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

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t old_size, size_t new_size) {
    vm.bytes_allocated += new_size - old_size;

    if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
        collect_garbage();
#endif

        if (vm.bytes_allocated > vm.next_gc) {
            collect_garbage();
        }
    }

    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    ASSERT(result != NULL, "we are able to allocate memory");
    return result;
}

static void free_object(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif
    switch (object->type) {
        case OBJ_CLASS: {
            FREE(ObjClass, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            table_free(&instance->fields);
            FREE(ObjInstance, object);
            break;
        }
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

    free(vm.gray_stack);
}

void mark_object(Obj* obj) {
    if (obj == NULL) {
        return;
    }

    if (obj->is_marked) {
        return;
    }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)obj);
    value_print(OBJ_VAL(obj));
    printf("\n");
#endif

    obj->is_marked = true;

    if (vm.gray_capacity < vm.gray_count + 1) {
        vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
        vm.gray_stack =
            (Obj**)realloc(vm.gray_stack, sizeof(Obj*) * vm.gray_capacity);
    }

    if (!vm.gray_stack) {
        exit(1);
    }

    vm.gray_stack[vm.gray_count++] = obj;
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

static void mark_array(ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        mark_value(array->values[i]);
    }
}

static void blacken_object(Obj* obj) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)obj);
    value_print(OBJ_VAL(obj));
    printf("\n");
#endif

    switch (obj->type) {
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)obj;
            mark_object((Obj*)klass->name);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)obj;
            mark_object((Obj*)instance->klass);
            mark_table(&instance->fields);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)obj;

            mark_object((Obj*)closure->function);

            for (int i = 0; i < closure->upvalue_count; i++) {
                mark_object((Obj*)closure->upvalues[i]);
            }

            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)obj;
            mark_object((Obj*)function->name);
            mark_array(&function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE:
            mark_value(((ObjUpvalue*)obj)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void trace_references(void) {
    while (vm.gray_count > 0) {
        Obj* obj = vm.gray_stack[--vm.gray_count];
        blacken_object(obj);
    }
}

static void sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            free_object(unreached);
        }
    }
}

void collect_garbage(void) {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytes_allocated;
#endif

    mark_roots();
    trace_references();
    table_remove_white(&vm.strings);
    sweep();

    vm.next_gc = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytes_allocated, before, vm.bytes_allocated, vm.next_gc);
#endif
}
