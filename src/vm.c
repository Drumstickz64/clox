#include <stdarg.h>
#include <stdio.h>

#include "assert.h"
#include "common.h"
#include "compiling/compiler.h"
#include "config.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

VM vm;

static void reset_stack(void) {
    vm.stack_top = vm.stack;
}

static Value peek(size_t distance) {
    return vm.stack_top[-1 - distance];
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
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            value_print(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_RETURN: {
                value_print(pop());
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL:
                push(NIL_VAL());
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
            case OP_ADD:
                BINARY_OP(NUMBER_VAL, +);
                break;
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
}

void free_vm(void) {}

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