#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

VM vm;

void init_vm(void) {}

void free_vm(void) {}

static InterpretResult run(void) {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (value_get(&vm.chunk->constants, READ_BYTE()))

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
        case OP_RETURN:
            return INTERPRET_OK;
        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
            value_print(constant);
            printf("\n");
            break;
        }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(Chunk *chunk) {
    vm.chunk = chunk;
    vm.ip = chunk->code;
    return run();
}
