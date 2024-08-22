#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "assert.h"
#include "common.h"
#include "compiling/compiler.h"
#include "config.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

VM vm;

static Value clock_native(int arg_count, Value* args) {
    UNUSED(arg_count);
    UNUSED(args);
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void reset_stack(void) {
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = NULL;
}

static void runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frame_count - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction_index = frame->ip - 1 - function->chunk.code;
        int line = function->chunk.lines[instruction_index];

        fprintf(stderr, "[line %d] in ", line);

        if (function->name) {
            fprintf(stderr, "%s()\n", function->name->chars);
        } else {
            fprintf(stderr, "script\n");
        }
    }

    reset_stack();
}

static void define_native(const char* name, NativeFn function, int arity) {
    push(OBJ_VAL(copy_string(name, (int)strlen(name))));
    push(OBJ_VAL(native_new(function, arity)));
    table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

static Value peek(int distance) {
    return vm.stack_top[-1 - distance];
}

static bool call(ObjClosure* closure, int arg_count) {
    if (arg_count != closure->function->arity) {
        runtime_error("expected %d arguments, got %d", closure->function->arity,
                      arg_count);
        return false;
    }

    if (vm.frame_count == FRAMES_MAX) {
        runtime_error("stack overflow");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - 1 - arg_count;
    return true;
}

static bool call_value(Value callee, int arg_count) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE: {
                ObjNative* native_fn = AS_NATIVE(callee);
                if (arg_count != native_fn->arity) {
                    runtime_error("expected %d arguments, got %d",
                                  native_fn->arity, arg_count);
                }
                Value result =
                    native_fn->function(arg_count, vm.stack_top - arg_count);
                // +1 to also free the native function object
                vm.stack_top -= arg_count + 1;
                push(result);
                return true;
            }
            default:
                break;
        }
    }

    runtime_error("can only call functions and classes");
    return false;
}

static ObjUpvalue* capture_upvalue(Value* local) {
    ObjUpvalue* prev_upvalue = NULL;
    ObjUpvalue* upvalue = vm.open_upvalues;
    while (upvalue && upvalue->location > local) {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* new_upvalue = upvalue_new(local);
    new_upvalue->next = upvalue;

    if (!prev_upvalue) {
        vm.open_upvalues = new_upvalue;
    } else {
        prev_upvalue->next = new_upvalue;
    }

    return new_upvalue;
}

static void close_upvalues(Value* last) {
    while (vm.open_upvalues && vm.open_upvalues->location >= last) {
        ObjUpvalue* upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
}

static bool is_falsy(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void) {
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int len = a->len + b->len;
    char* chars = ALLOCATE(char, len + 1);
    memcpy(chars, a->chars, a->len);
    memcpy(chars + a->len, b->chars, b->len);
    chars[len] = '\0';

    ObjString* result = take_string(chars, len);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static InterpretResult run(void) {
    CallFrame* frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() \
    (value_get(&frame->closure->function->chunk.constants, READ_BYTE()))
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() (AS_STRING(READ_CONSTANT()))
#define BINARY_OP(value_type, op)                                            \
    do {                                                                     \
        if (!IS_NUMBER(peek(1))) {                                           \
            runtime_error("left operand of '%s' operator must be a number",  \
                          (#op));                                            \
            return INTERPRET_RUNTIME_ERROR;                                  \
        }                                                                    \
        if (!IS_NUMBER(peek(0))) {                                           \
            runtime_error("right operand of '%s' operator must be a number", \
                          (#op));                                            \
            return INTERPRET_RUNTIME_ERROR;                                  \
        }                                                                    \
        double b = AS_NUMBER(pop());                                         \
        double a = AS_NUMBER(pop());                                         \
        push(value_type(a op b));                                            \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            value_print(*slot);
            printf(" ]");
        }
        printf("\n");

        disassemble_instruction(
            &frame->closure->function->chunk,
            (int)(frame->ip - frame->closure->function->chunk.code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                Value result = pop();
                close_upvalues(frame->slots);
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    // finished executing top-level code
                    pop();  // pop the <script> function
                    return INTERPRET_OK;
                }

                vm.stack_top = frame->slots;
                push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_CLASS:
                push(OBJ_VAL(class_new(READ_STRING())));
                break;
            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = closure_new(function);
                push(OBJ_VAL(closure));

                for (int i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();

                    if (is_local) {
                        closure->upvalues[i] =
                            capture_upvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }

                break;
            }
            case OP_JUMP: {
                uint16_t jump = READ_SHORT();
                frame->ip += jump;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t jump = READ_SHORT();
                if (is_falsy(peek(0))) {
                    frame->ip += jump;
                }
                break;
            }
            case OP_LOOP: {
                uint16_t jump = READ_SHORT();
                frame->ip -= jump;
                break;
            }
            case OP_CALL: {
                int arg_count = READ_BYTE();
                if (!call_value(peek(arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_PRINT: {
                value_print(pop());
                printf("\n");
                break;
            }
            case OP_POP:
                pop();
                break;
            case OP_CLOSE_UPVALUE:
                close_upvalues(vm.stack_top - 1);
                pop();
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                ASSERT(slot < (uint8_t)(vm.stack_top - vm.stack),
                       "variable slot is inside of the stack");
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                ASSERT(slot < (uint8_t)(vm.stack_top - vm.stack),
                       "variable slot is inside of the stack");
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!table_get(&vm.globals, name, &value)) {
                    runtime_error("undefined variable: '%s'", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (table_set(&vm.globals, name, peek(0))) {
                    table_delete(&vm.globals, name);
                    runtime_error("undefined variable: '%s'", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                table_set(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);
                break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <);
                break;
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL:
                push(NIL_VAL);
                break;
            case OP_TRUE:
                push(BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(BOOL_VAL(false));
                break;
            case OP_NEGATE: {
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("negation operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }
            case OP_NOT:
                push(BOOL_VAL(is_falsy(pop())));
                break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtime_error(
                        "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL, -);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL, *);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL, /);
                break;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

void init_vm(void) {
    reset_stack();

    vm.objects = NULL;
    vm.bytes_allocated = 0;
    vm.next_gc = 1024 * 1024;
    vm.gray_count = 0;
    vm.gray_capacity = 0;
    vm.gray_stack = NULL;

    table_init(&vm.globals);
    table_init(&vm.strings);

    define_native("clock", clock_native, 0);
}

void free_vm(void) {
    table_free(&vm.globals);
    table_free(&vm.strings);
    free_objects();
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (!function) {
        return INTERPRET_COMPILE_ERROR;
    }

    push(OBJ_VAL(function));
    ObjClosure* closure = closure_new(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}

void push(Value value) {
    ASSERT((int)(vm.stack_top - vm.stack) < STACK_MAX, "the stack is not full");
    *vm.stack_top = value;
    vm.stack_top++;
}
Value pop(void) {
    ASSERT(vm.stack_top != vm.stack, "the stack is not empty");
    vm.stack_top--;
    Value value = *vm.stack_top;
    return value;
}
