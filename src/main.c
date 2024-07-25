#include <stdio.h>

#include "assert.h"
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(void) {
    init_vm();
    Chunk chunk;
    chunk_init(&chunk);

    int constant = chunk_add_constant(&chunk, 1.2);
    chunk_write(&chunk, OP_CONSTANT, 123);
    chunk_write(&chunk, constant, 123);

    chunk_write(&chunk, OP_RETURN, 123);

    printf("\nDebug:\n");
    disassemble_chunk(&chunk, "test chunk");

    printf("\nRunning:\n");
    InterpretResult result = interpret(&chunk);
    switch (result) {
    case INTERPRET_OK: {
        printf("OK\n");
        break;
    }
    default: {
        printf("UNHANDLED INTEPRET RESULT: %d\n", result);
        exit(1);
    }
    }

    chunk_free(&chunk);
    free_vm();
    return 0;
}
