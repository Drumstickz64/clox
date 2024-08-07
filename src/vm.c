#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

static void reset_stack(void) {
    vm.stack_top = vm.stack;
}

static Value peek(size_t distance) {
    return vm.stack_top[-1 - distance];
}

static bool is_falsy(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && AS_BOOL(value));
}

static void concatenate(void) {
    ObjString* b = AS_STRING(pop());
    ObjString* a = AS_STRING(pop());
    int len = a->len + b->len;
    char* chars = ALLOCATE(char, len + 1);
    memcpy(chars, a->chars, a->len);
    memcpy(chars + a->len, b->chars, b->len);
    chars[len] = '\0';

    ObjString* result = take_string(chars, len);
    push(OBJ_VAL(result));
}

static void runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction_idx = vm.ip - 1 - vm.chunk->code;
    int line = vm.chunk->lines[instruction_idx];
    fprintf(stderr, "[line %d] of script\n", line);
    reset_stack();
}

static InterpretResult run(void) {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (value_get(&vm.chunk->constants, READ_BYTE()))
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
        if (vm.stack_top != vm.stack) {
            printf("          ");
            for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
                printf("[ ");
                value_print(*slot);
                printf(" ]");
            }
            printf("\n");
        }

        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                // exit the interpreter
                return INTERPRET_OK;
            }
            case OP_PRINT: {
                value_print(pop());
                printf("\n");
                break;
            }
            case OP_POP:
                pop();
                break;
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
                push(BOOL_VAL(!is_falsy(pop())));
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
#undef BINARY_OP
}

void init_vm(void) {
    reset_stack();
    vm.objects = NULL;
    table_init(&vm.strings);
}

void free_vm(void) {
    table_free(&vm.strings);
    free_objects();
}

InterpretResult interpret(const char* source) {
    Chunk chunk;
    chunk_init(&chunk);

    if (!compile(source, &chunk)) {
        chunk_free(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();

    chunk_free(&chunk);
    return result;
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