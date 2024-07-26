#include <stdio.h>

#include "assert.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

VM vm;

static void reset_stack(void) { vm.stack_top = vm.stack; }

static InterpretResult run(void) {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (value_get(&vm.chunk->constants, READ_BYTE()))

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stack_top; slot++) {
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
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}

void init_vm(void) { reset_stack(); }

void free_vm(void) {}

InterpretResult interpret(Chunk *chunk) {
    vm.chunk = chunk;
    vm.ip = chunk->code;
    return run();
}

void push(Value value) {
    *vm.stack_top = value;
    vm.stack_top++;
}
Value pop() {
    ASSERT(vm.stack_top != vm.stack, "the stack is not empty");
    vm.stack_top--;
    Value value = *vm.stack_top;
    return value;
}